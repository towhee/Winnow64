#pragma once

#include <QString>
#include <functional>
#include <vector>
#include <atomic>

#include <opencv2/core.hpp>

namespace FSArtifact
{

struct Options
{
    // Method selector (mirrors FSFusion / FSDepth)
    // "Simple"
    // "MultiScale"
    // "MultiScale+DepthEdges"
    // "MultiScale+PatchNCC"
    QString method = "MultiScale";

    // --- Shared thresholds ---
    float detailThreshold   = 0.02f;
    float supportRatio      = 0.60f;

    // --- Laplacian ---
    int   laplacianKSize    = 3;

    // --- Depth edge boost ---
    bool  enableDepthEdges = true;
    float depthEdgeThresh  = 5.0f;
    float depthBoost       = 0.35f;

    // --- Patch NCC ---
    bool  enablePatchNCC   = false;
    int   patchRadius      = 3;
    float nccRejectThresh  = 0.35f;
    float nccBoost         = 0.50f;

    // --- Post ---
    int   blurRadius       = 1;

    // --- Debug ---
    bool    writeDebug     = false;
    QString debugFolder;
    float   overlayAlpha  = 0.55f;
    float   overlayThresh = 0.70f;
};

using ProgressCallback = std::function<void()>;
using StatusCallback   = std::function<void(const QString&)>;

// Main entry point (mirrors FSDepth::run)
bool detect(const cv::Mat& fusedGray32,
         const std::vector<cv::Mat>& slicesGray32,
         const cv::Mat* depthMap32,
         const cv::Mat* includeMask8,
         cv::Mat& artifactConfidence01,   // OUT
         const Options& opt,
         std::atomic_bool* abortFlag,
         ProgressCallback progressCb,
         StatusCallback statusCb);

// Method variants
bool runSimple(const cv::Mat &fusedGray32_in,
               const std::vector<cv::Mat> &slicesGray32_in,
               const cv::Mat *depthMap32,
               const cv::Mat *includeMask8,
               cv::Mat &artifactConfidence01,
               const Options &opt,
               std::atomic_bool *abortFlag,
               ProgressCallback progressCb,
               StatusCallback statusCb);

bool runMultiScale(const cv::Mat &fusedGray32_in,
                   const std::vector<cv::Mat> &slicesGray32_in,
                   const cv::Mat *depthMap32,
                   const cv::Mat *includeMask8,
                   cv::Mat &artifactConfidence01,
                   const Options &opt,
                   std::atomic_bool *abortFlag,
                   ProgressCallback progressCb,
                   StatusCallback statusCb);

// Repair artifact pixels in-place
// - fusedInOut: CV_8U / CV_16U / CV_32F, 1 or 3 channels
// - artifact01: CV_32F [0..1]
// - alignedSlices: aligned source images (same size/type as fusedInOut)
// - backgroundSourceIndex: which slice to copy from
// - threshold: confidence >= threshold => repair
//
bool repair(cv::Mat &fusedInOut,
            const cv::Mat &artifact01,
            const std::vector<cv::Mat> &alignedSlices,
            int backgroundSourceIndex,
            float threshold = 0.02f,
            std::atomic_bool *abortFlag = nullptr);

} // namespace FSArtifact
