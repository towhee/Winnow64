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
// FSFusionDMapBasic
// "continuous-depth" DMap using Top-2 winners only (streamed 2-pass)
//
// pass-1: compute focus metric, maintain top2 (idx0/idx1 + scores) in PAD
// pass-2: crop to ORIG, build per-slice weights from top2 mix, pyramid blend
// ============================================================

class FSFusionDMapBasic : public FSFusion
{
public:
    FSFusionDMapBasic();
    ~FSFusionDMapBasic() override = default;

    struct Params
    {
        QString method = "Pyramid";  // Simple or Pyramid

        // focus metric
        float scoreSigma = 1.5f;
        int   scoreKSize = 3;

        // Low-contrast suppression (Zerene-style)
        bool  enableContrastThreshold = true;
        float contrastMinFrac = 0.010f;   // 0.3%..3% typical; start 1% (fraction of max top1Score)
        int   lowContrastMedianK = 5;     // 3 or 5
        int   lowContrastDilatePx = 0;    // optional (0..4)

        // continuous mixing
        float mixEps = 1e-6f;        // for score0+score1 denominator
        float wMin   = 0.0f;         // optional floor (0..0.02), usually 0

        // pyramid / blend
        bool  enableHardWeightsOnLowpass = false; // usually off for Basic
        bool  enableDepthGradLowpassVeto = false; // Basic can ignore initially
        int   hardFromLevel = 4;
        int   vetoFromLevel = -1;
        float vetoStrength  = 1.0f;
        float weightBlurSigma = 0.0f; // 1.2f;

        int   pyrLevels = 5;
    };

    Params params;

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

// private:
    bool active_ = false;
    int  sliceCount_ = 0;

    // PAD-space top2
    cv::Size padSize;
    cv::Mat  idx0_pad16;   // CV_16U
    cv::Mat  idx1_pad16;   // CV_16U
    cv::Mat  s0_pad32;     // CV_32F
    cv::Mat  s1_pad32;     // CV_32F

    cv::Mat winIdx16_;     // CV_16U
    cv::Mat top1Score32_;  // CV_32F

    // geometry set by caller (same pattern as FSFusionDMap)
    cv::Size alignSize;
    cv::Rect validAreaAlign;
    cv::Size origSize;

    int outDepth = CV_8U;

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

    void cropPadToOrig(const cv::Rect& roiPadToAlign,
                       const cv::Size& origSz,
                       cv::Mat& idx0_16,
                       cv::Mat& idx1_16,
                       cv::Mat& s0_32,
                       cv::Mat& s1_32,
                       cv::Mat& top1Score32) const;
};
