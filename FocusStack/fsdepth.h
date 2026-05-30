#ifndef FSDEPTH_H
#define FSDEPTH_H

#include <QString>
#include <atomic>
#include <functional>

#include <opencv2/imgcodecs.hpp>

// namespace {

// cv::Mat makeDepthPreviewEnhanced(const cv::Mat &depthIndex16, int sliceCount);

// } // anonymous namespace

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
    bool saveWaveletDebug = true;

    int  numThreads  = 0;       // reserved (currently unused)
};

struct TenengradParams
{
    float radius    = 2.0f;     // Gaussian sigma
    float threshold = 6000.0f;  // energy threshold (before sqrt), 0 disables
};

struct StreamState
{
    bool initialized = false;
    cv::Size size {0,0};
    int sliceIndex = 0;

    cv::Mat bestVal32;      // CV_32F, per-pixel best focus energy so far
    cv::Mat depthIndex16;   // CV_16U, per-pixel winning slice index

    TenengradParams ten;    // parameters for Tenengrad

    void reset()
    {
        initialized = false;
        size = cv::Size(0,0);
        sliceIndex = 0;
        bestVal32.release();
        depthIndex16.release();
        ten = TenengradParams{};
    }
};

using ProgressCallback = std::function<void()>;
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

// Streaming entry point: call once per slice.
// On the last slice, call finishStreamingTenengrad() to write outputs.
bool streamGraySlice(const cv::Mat        &graySlice,
                     int                  sliceIndex,
                     const QString        &depthFolder,
                     const Options        &opt,
                     StreamState          &state,
                     std::atomic_bool     *abortFlag,
                     ProgressCallback      progressCb,
                     StatusCallback        statusCb);

// Finalize + write outputs (call after the last slice).
bool finishStreaming(const QString    &depthFolder,
                     const Options    &opt,
                     int              sliceCount,
                     StreamState      &state,
                     StatusCallback    statusCb,
                     cv::Mat          *depthIndex16Out = nullptr);

} // namespace FSDepth

#endif // FSDEPTH_H
