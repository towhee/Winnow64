#pragma once

#include <opencv2/core.hpp>
#include <vector>

namespace FusionPyr
{
// ----------------------------
// Gaussian / Laplacian pyramids
// ----------------------------
void buildGaussianPyr(const cv::Mat& img32, int levels, std::vector<cv::Mat>& gp);
void buildLaplacianPyr(const cv::Mat& img32, int levels, std::vector<cv::Mat>& lp);
cv::Mat collapseLaplacianPyr(const std::vector<cv::Mat>& lp);

// ----------------------------
// Label pyramid helpers (idx16)
// ----------------------------
cv::Mat downsampleIdx16_Mode2x2(const cv::Mat& src16);
void buildIndexPyrNearest(const cv::Mat& idx16Level0, int levels, std::vector<cv::Mat>& idxPyr);

// ----------------------------
// Depth-edge veto pyramid (optional) — ORIGINAL behavior
// ----------------------------
struct DepthEdgeVetoParams
{
    int   vetoFromLevel   = -1;    // <0 => residual only (levels-1)
    float depthGradThresh = 1.25f; // in "index units" after Sobel normalization
    int   vetoDilatePx    = 2;     // 0..N
};

void buildDepthEdgeVetoPyr(const std::vector<cv::Mat>& idxPyr16, // CV_16U, size=levels
                           std::vector<cv::Mat>& vetoPyr8,      // out CV_8U, size=levels
                           const DepthEdgeVetoParams& p);

// ----------------------------
// Accumulator pyramids
// ----------------------------
struct PyrAccum
{
    int levels = 0;
    std::vector<cv::Mat> num3; // CV_32FC3
    std::vector<cv::Mat> den1; // CV_32F

    void reset(const cv::Size& sz, int levels_);
};

void accumulateSlicePyr(PyrAccum& A,
                        const cv::Mat& color32FC3,
                        const cv::Mat& weight32F,
                        int levels,
                        float weightBlurSigma);

// Rich accumulate used by FSFusionDMap.
struct AccumDMapParams
{
    bool  enableHardWeightsOnLowpass = true;
    bool  enableDepthGradLowpassVeto = false;

    int   hardFromLevel = -1;
    int   vetoFromLevel = -1;
    float vetoStrength  = 1.0f;

    float wMin = 0.0f;
};

void accumulateSlicePyr(PyrAccum& A,
                        const cv::Mat& color32FC3,
                        const cv::Mat& weight32F,
                        const std::vector<cv::Mat>& idxPyr16,       // CV_16U, size=levels
                        const std::vector<cv::Mat>* vetoPyr8_orNull, // optional CV_8U, size=levels
                        int sliceIndex,
                        const AccumDMapParams& p,
                        int levels,
                        float weightBlurSigma);

// Finalize
cv::Mat finalizeBlend(const PyrAccum& A, float eps);

} // namespace FusionPyr
