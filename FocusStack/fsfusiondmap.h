#pragma once

#include "fsalign_types.h"
#include "fsfusion.h"
#include "fusionpyr.h"

#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>

#include <QString>
#include <atomic>
#include <vector>

// ============================================================
// FSFusionDMap
// "continuous-depth" DMap using Top-2 winners only (streamed 2-pass)
//
// pass-1: compute focus metric, maintain top2 (idx0/idx1 + scores) in PAD
// pass-2: crop to ORIG, build per-slice weights from top2 mix, pyramid blend
// ============================================================

class FSFusionDMap : public FSFusion
{
public:
    FSFusionDMap();
    ~FSFusionDMap() override = default;

    struct Params
    {
        // focus metric
        float scoreSigma = 0.75;    // 1.5
        int   scoreKSize = 3;       // 3

        // ADO: Adaptive Donor Override
        // Foreground
        int depthStableRadiusPx = 3;  // ↑ more strict
        int depthMaxRangeSlicesCore = 1;   // ↓ more strict (>1 = halos)
        int depthMaxRangeSlicesLoose = 5;  // ↓ more strict (>1 = halos)
        float expandTexFrac = 0.05f;  //   FG Must be connected to interior and
        // strongFrac must be > weakFrac
        float strongFrac = 0.030f;    // ↓ interior / holes
        float weakFrac = 0.01f;       // ↓ interior / holes (subtle)
        int seedDilatePx = 1;         // ↓ finer items (ie twigs)
        int closePx = 5;
        int openPx  = 1;
        int interiorPx = 3;  // default 3

        // --- Boundary Ownership / Halo elimination (Two-pass ownership propagation) ---
        int  ownershipClosePx = 3;      // close FG gaps a bit before building ring (0..3)
        int  seedBandPx       = 1;
        // int  ownershipErodePx = 1;      // boundary = FG - erode(FG)

        // // Low-contrast suppression (Zerene-style)
        bool  enableContrastThreshold = true;
        float contrastMinFrac = 0.20f;   // 0.3%..3% typical; start 1% (fraction of max top1Score)
        int   lowContrastMedianK = 5;     // 3 or 5
        int   lowContrastDilatePx = 1;    // optional (0..4)

        // pyramid / blend
        bool  enablePyramidBlend = true;
        bool  enableHardWeightsOnLowpass = false;    // false
        bool  enableDepthGradLowpassVeto = false;
        int   hardFromLevel = 0;    // 4
        int   vetoFromLevel = -1;
        float vetoStrength  = 1.0f;
        float weightBlurSigma = 0.0f; // 1.2f;
        float mixEps = 1e-6f;        // for score0+score1 denominator
        float wMin   = 0.0f;         // optional floor (0..0.02), usually 0

        int   pyrLevels = 5;

        bool enableDiagnostics = true;
    };

    Params o;

    // tmp debugging
    std::vector<QString> alignedColorPaths;     // intermediate
    std::vector<QString> alignedGrayPaths;      // intermediate


    void reset();

    bool streamSlice(int slice,
                     const cv::Mat& grayAlign8,      // CV_8U ALIGN
                     const cv::Mat& colorAlign,      // optional
                     const FSFusion::Options& opt,
                     std::atomic_bool* abortFlag,
                     FSFusion::StatusCallback statusCb,
                     FSFusion::ProgressCallback progressCb);

    bool streamFinish(cv::Mat& outputColor,
                      const FSFusion::Options& opt,
                      cv::Mat& depthIndex16,          // OUT: idx0 (best) ORIG
                      const QStringList& inputPaths,
                      const std::vector<Result>& globals,
                      std::atomic_bool* abortFlag,
                      FSFusion::StatusCallback statusCb,
                      FSFusion::ProgressCallback progressCb);



    bool active_ = false;
    int  sliceCount_ = 0;
    int N = 0;

    // PAD-space top2
    cv::Size padSize;
    cv::Mat  idx0_pad16;   // CV_16U
    cv::Mat  idx1_pad16;   // CV_16U
    cv::Mat  s0_pad32;     // CV_32F
    cv::Mat  s1_pad32;     // CV_32F

    cv::Mat idx0_16, idx1_16, s0_32, s1_32, top1_32;

    cv::Mat winIdx16_;     // CV_16U
    cv::Mat top1Score32_;  // CV_32F

    // geometry set by caller
    cv::Size alignSize;
    cv::Rect validAreaAlign;
    cv::Size origSize;

    int outDepth = CV_8U;

    private:
    cv::Mat fg8;
    cv::Mat fgOwn8;
    cv::Mat overrideMask8;
    cv::Mat overrideWinner16;
    cv::Mat lowC8;

    // helpers
    int computePyrLevels(const cv::Size& origSz) const;

    bool toColor32_01_FromLoaded(cv::Mat colorTmp,
                                 cv::Mat& color32,
                                 const QString& where,
                                 int sliceIdx);

    void updateTop2(const cv::Mat& score32This, uint16_t sliceIndex);

    bool computeCropGeometry(const QString& srcFun,
                             cv::Rect& roiPadToAlign,
                             cv::Size& origSz) const;

    void accumulatedMeasures(const cv::Rect& roiPadToAlign,
                       const cv::Size& origSz,
                       cv::Mat& idx0_16,
                       cv::Mat& idx1_16,
                       cv::Mat& s0_32,
                       cv::Mat& s1_32,
                       cv::Mat& top1Score32) const;

    void diagnostics(QString path, cv::Mat &depthIndex16);
};
