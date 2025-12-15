#ifndef FSDEPTH_H
#define FSDEPTH_H

#include <QString>
#include <atomic>
#include <functional>

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
bool run(const QString    &focusFolder,
         const QString    &depthFolder,
         const Options    &opt,
         std::atomic_bool *abortFlag,
         ProgressCallback  progressCb,
         StatusCallback    statusCb);

} // namespace FSDepth

#endif // FSDEPTH_H
