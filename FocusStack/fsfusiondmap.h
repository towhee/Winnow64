#pragma once

#include "fsalign_types.h"
#include "fsfusion.h"
#include "fsfusiondmapshared.h"
#include "fusionpyr.h"

#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>

#include <QString>
#include <atomic>
#include <vector>
#include <functional>

// ============================================================
// fsfusiondmap.h
// dmap (decision map) implementation: streamed 2-pass.
// pass-1: build top-k focus score maps (stored in PAD space)
// pass-2: compute weights + pyramid blend (in ORIG space)
// ============================================================

class FSFusionDMap : public FSFusion
{
public:
    FSFusionDMap();
    ~FSFusionDMap() override = default;

    struct TopKMaps
    {
        int K = 0;
        cv::Size sz;
        std::vector<cv::Mat> idx16;   // K x CV_16U
        std::vector<cv::Mat> score32; // K x CV_32F

        void reset();
        void create(cv::Size s, int k);
        bool valid() const;
    };

    enum class DepthMode {
        Baseline,       // current win16
        Experimental     // use hook result
    };

    using DepthHookFn = std::function<bool(
        const cv::Mat& baselineDepth16,   // CV_16U, orig size
        const cv::Mat& conf01,            // CV_32F, orig size
        const TopKMaps& topkOrig,         // scores/idx, orig size
        int N,
        cv::Mat& outDepth16               // CV_16U, orig size (must be filled)
        )>;


    struct DMapParams
    {
        // Experiment start
        DepthMode depthMode = DepthMode::Baseline;
        DepthHookFn depthHook;  // optional experiment hook (if nullptr => do nothing)

        // Optional “tighten” knobs:
        float depthConfSmoothMax = 0.08f;
        float depthConfStableMax = 0.18f;
        int   depthBoundaryDilateSmoothPx = 0;
        int   depthBoundaryDilateStablePx = 0;
        int   depthTexMax = 12;
        bool  depthUseTextureGate = true;
        bool  writeDepthDiagnostics = true;

        // --- boundary override knobs (used by BWO earlier; ADO will reuse gates) ---
        int   bwoRingPx = 40;        // 30–60
        float bwoConfMax = 0.22f;    // low-confidence only
        int   bwoTexMax  = 12;       // low-texture only (8U scale)
        int   bwoBandDilatePx = 0;   // thicken band slightly (0..3)
        // Experiment end

        bool enableHardWeightsOnLowpass = true;

        int   topK = 2;

        float scoreSigma = 1.5f;
        int   scoreKSize  = 3;

        float softTemp = 0.25f;
        float wMin     = 0.01f;

        // Veto pyramid
        bool  enableDepthGradLowpassVeto = true;
        int   hardFromLevel = 4;
        int   vetoFromLevel = -1;
        float vetoStrength = 1.0f;
        float depthGradThresh = 1.25f;
        int   vetoDilatePx = 2;

        float confEps  = 1e-6f;
        float confLow  = 0.08f;
        float confHigh = 0.25f;

        int   bandDilatePx = 12;
        int   uncertaintyDilatePx = 6;

        float weightBlurSigma = 1.2f;

        // --- Halo fix near foreground edges ---
        bool  enableHaloRingFix = true;
        int   haloRingPx        = 50;
        float haloFeatherSigma  = 10.0f;
        float haloConfMax       = 0.22f;
        int   haloTexMax        = 12;
        bool  haloUseMaxIndexAsForeground = true;

        bool  useTiling = false;
        int   tilePx = 512;
        int   tileOverlapPx = 32;

        int   pyrLevels = 5;
    };

    // ------------------------------------------------------------
    // weights containers
    // ------------------------------------------------------------
    struct DMapWeights
    {
        std::vector<cv::Mat> w32; // K x CV_32F (orig size)

        cv::Mat boundary8;
        cv::Mat band8;

        // Donor override channel (ADO will populate these):
        // overrideMask8:   CV_8U 0/255 domain where override applies
        // overrideWinner16 CV_16U donor slice id per pixel (same size)
        cv::Mat overrideMask8;
        cv::Mat overrideWinner16;

        void create(cv::Size sz, int K)
        {
            w32.assign(K, cv::Mat(sz, CV_32F, cv::Scalar(0)));
            boundary8.release();
            band8.release();
            overrideMask8.release();
            overrideWinner16.release();
        }
    };

    DMapParams params;



    void reset();

    // pass-1: caller provides ALIGN-space mats; engine pads internally to PAD-space
    bool streamSlice(int slice,
                     const cv::Mat& grayAlign8,     // CV_8U ALIGN
                     const cv::Mat& colorAlign,     // ALIGN (optional for depth sanity)
                     const FSFusion::Options& opt,
                     std::atomic_bool* abortFlag,
                     FSFusion::StatusCallback statusCb,
                     FSFusion::ProgressCallback progressCb);

    // pass-2: reloads slices internally via FSLoader
    bool streamFinish(cv::Mat& outputColor,
                      const FSFusion::Options& opt,
                      cv::Mat& depthIndex16,
                      const QStringList& inputPaths,
                      const std::vector<Result>& globals,
                      std::atomic_bool* abortFlag,
                      FSFusion::StatusCallback statusCb,
                      FSFusion::ProgressCallback progressCb);

private:
    bool active_ = false;
    int  sliceCount_ = 0;
    QString depthFolderPath;
    TopKMaps topk_pad_;

    bool validateStreamFinishState(const QString& srcFun,
                                   const QStringList& inputPaths,
                                   const std::vector<Result>& globals,
                                   std::atomic_bool* abortFlag,
                                   int& N) const;
    bool computeCropGeometry(const QString& srcFun,
                             cv::Rect& roiPadToAlign,
                             cv::Size& origSz);
    bool cropTopKToOrig(const QString& srcFun,
                        const cv::Rect& roiPadToAlign,
                        const cv::Size& origSz,
                        TopKMaps& topkOrig) const;
    bool sanityCheckTop1(const QString& srcFun,
                         const TopKMaps& topkOrig,
                         int N) const;
    bool buildMapsAndStabilize(const QString& srcFun,
                               const TopKMaps& topkOrig,
                               int N,
                               std::atomic_bool* abortFlag,
                               DMapWeights& W,
                               cv::Mat& conf01,
                               cv::Mat& depthIndex16,
                               cv::Mat& winStable16) const;
    bool depthExperiment(const QString& srcFun,
                         const FSFusion::Options& opt,
                         const TopKMaps& topkOrig,
                         int N,
                         const cv::Mat& conf01,
                         std::atomic_bool* abortFlag,
                         cv::Mat& depthIndex16,   // OUT: depth used for pyramids/fusion
                         cv::Mat& winStable16);   // OUT: stable labels for halo boundary work
    int computePyrLevels(const cv::Size& origSz) const;
    void buildPyramids(const cv::Mat& depthIndex16,
                       int levels,
                       std::vector<cv::Mat>& idxPyr16,
                       std::vector<cv::Mat>& vetoPyr8) const;
    void initAccumulator(const cv::Size& origSz,
                         int levels,
                         FusionPyr::PyrAccum& A,
                         cv::Mat& sumW) const;
    bool accumulateAllSlices(const QString& srcFun,
                             const QStringList& inputPaths,
                             const std::vector<Result>& globals,
                             const cv::Rect& roiPadToAlign,
                             const cv::Rect& validAreaAlign,
                             const TopKMaps& topkOrig,
                             const DMapWeights& W,
                             const std::vector<cv::Mat>& idxPyr16,
                             const std::vector<cv::Mat>& vetoPyr8,
                             int levels,
                             const cv::Mat& conf01,
                             std::atomic_bool* abortFlag,
                             FSFusion::StatusCallback statusCb,
                             FSFusion::ProgressCallback progressCb,
                             FusionPyr::PyrAccum& A,
                             cv::Mat& sumW) const;
    cv::Mat finalizeOut32(const FusionPyr::PyrAccum& A) const;
    void debugHaloInputs(const cv::Mat& out32,
                         const TopKMaps& topkOrig,
                         const cv::Mat& conf01) const;
    void debugHaloOutput(const cv::Mat& out32) const;
    void applyHaloFixIfEnabled(const FSFusion::Options& opt,
                               const QString& srcFun,
                               int N,
                               const TopKMaps& topkOrig,
                               const cv::Mat& conf01,
                               const cv::Mat& winStable16,
                               const QStringList& inputPaths,
                               const std::vector<Result>& globals,
                               const cv::Rect& roiPadToAlign,
                               const cv::Rect& validAreaAlign,
                               std::atomic_bool* abortFlag,
                               cv::Mat& out32);
    void convertOut32ToOutput(const cv::Mat& out32, cv::Mat& outputColor) const;
};
