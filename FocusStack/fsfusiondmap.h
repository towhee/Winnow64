#pragma once

#include "fsalign_types.h"
#include "fsfusion.h"

#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>

#include <QString>
#include <atomic>
#include <vector>

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

    struct DMapParams
    {
        bool enableHardWeightsOnLowpass = true;  // used in FusionPyr::accumulateSlicePyr

        int   topK = 2;

        float scoreSigma = 1.5f;
        int   scoreKSize  = 3;

        float softTemp = 0.25f;
        float wMin     = 0.01f;

        // Veto pyramid
        bool  enableDepthGradLowpassVeto = false;
        int   hardFromLevel = 4;  // -1
        int   vetoFromLevel = -1;
        float vetoStrength = 1.0f;
        float depthGradThresh = 1.25f;
        int   vetoDilatePx = 2;

        float confEps  = 1e-6f;             // confidence map energy
        float confLow  = 0.08f;
        float confHigh = 0.25f;

        int   bandDilatePx = 12;
        int   uncertaintyDilatePx = 6;

        float weightBlurSigma = 1.2f;

        // --- Halo fix near foreground edges ---
        bool  enableHaloRingFix = true;
        int   haloRingPx        = 50;     // 40–60 width of ring outside foreground
        float haloFeatherSigma  = 10.0f;  // 8–14 feather smoothness (in px-ish)
        float haloConfMax       = 0.22f;  // 0.20–0.25 only touch low-confidence pixels
        int   haloTexMax        = 12;     // 10–14 only touch low-texture pixels (8U scale)
        bool  haloUseMaxIndexAsForeground = true; // assumes near->far ordering

        bool  useTiling = false;
        int   tilePx = 512;
        int   tileOverlapPx = 32;

        int   pyrLevels = 5;
    };

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

public:
    DMapParams params;

public:
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

    TopKMaps topk_pad_;
};
