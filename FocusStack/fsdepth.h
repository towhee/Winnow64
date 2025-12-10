#ifndef FSDEPTH_H
#define FSDEPTH_H

#include <QString>
#include <atomic>
#include <functional>

#include <opencv2/core.hpp>

namespace FSDepth {

struct Options
{
    bool preview = true;
    int  numThreads  = 0;
};

using ProgressCallback = std::function<void(int)>;
using StatusCallback   = std::function<void(const QString &message, bool isError)>;

// Input:  focus maps in focusFolder
// Output: depth_index.png (and optionally depth_preview.png) in depthFolder
bool run(const QString    &focusFolder,
         const QString    &depthFolder,
         const Options    &opt,
         std::atomic_bool *abortFlag,
         ProgressCallback  progressCb,
         StatusCallback    statusCb,
         cv::Mat          *depthIndexOut,
         const std::vector<cv::Mat> *focusMaps = nullptr);

} // namespace FSDepth

#endif // FSDEPTH_H
