#ifndef FSDEPTH_H
#define FSDEPTH_H

#include <QString>
#include <atomic>
#include <functional>

#include <opencv2/imgcodecs.hpp>

namespace FSDepth {

struct Options
{
    // "Simple"    = original scalar max over focus_*.tif
    // "MultiScale"= wavelet-based depth using FSFusionWavelet + FSMerge
    QString method = "Simple";

    // For MultiScale: where to find gray_*.tif (aligned grayscale)
    // If empty, we fall back to focusFolder.
    QString alignFolder;

    // Whether to write depth_preview.png (grayscale + heatmap + legend)
    bool preview = true;

    // Write intermediates
    bool keep = true;

    // When method == "MultiScale", save per-slice wavelet debug images:
    //   wavelet_mag_XXX.png
    //   wavelet_mag_merged.png
    bool saveWaveletDebug = false;

    int  numThreads  = 0;       // reserved (currently unused)
};

using ProgressCallback = std::function<void(int)>;
using StatusCallback   = std::function<void(const QString &message)>;

// Input:  focus maps in focusFolder (Simple)
//         OR aligned gray_*.tif in opt.alignFolder (MultiScale)
// Output: depth_index.png (+ depth_preview.png) in depthFolder

// When reading from disk
bool run(const QString &focusFolder,
         const QString &depthFolder,
         const Options &opt,
         std::atomic_bool *abortFlag,
         ProgressCallback progressCb,
         StatusCallback statusCb,
         cv::Mat *depthIndex16Out = nullptr);

// When using cache
bool runFromGraySlices(
    const std::vector<cv::Mat> &graySlices,   // CV_8U or CV_32F
    const QString              &depthFolder,
    const Options              &opt,
    std::atomic_bool           *abortFlag,
    ProgressCallback            progressCb,
    StatusCallback              statusCb,
    cv::Mat                    *depthIndex16Out = nullptr
    );

// // internal helper
// bool runMultiScaleFromGrayMats(
//     const std::vector<cv::Mat> &graySlices,
//     const QString              &depthFolder,
//     const FSDepth::Options     &opt,
//     std::atomic_bool           *abortFlag,
//     FSDepth::ProgressCallback   progressCb,
//     FSDepth::StatusCallback     statusCb,
//     cv::Mat                    *depthIndex16Out
//     );

} // namespace FSDepth

#endif // FSDEPTH_H
