#ifndef FSFOCUS_H
#define FSFOCUS_H

#include <QString>
#include <QThread>
#include <atomic>
#include <functional>

#include <opencv2/core.hpp>

namespace FSFocus {

struct Options
{
    int  downsampleFactor   = 1;
    int  numThreads         = 0;   // 0 = auto
    bool useOpenCL          = true;

    bool preview            = false;      // write numeric logs
    bool keepIntermediates  = true;       // write focus_*.tif + previews
};

using ProgressCallback = std::function<void(int)>;
using StatusCallback   = std::function<void(const QString &msg, bool isError)>;

bool run(const QString    &alignFolder,
         const QString    &focusFolder,
         const Options    &opt,
         std::atomic_bool *abortFlag,
         ProgressCallback  progressCb,
         StatusCallback    statusCb,
         std::vector<cv::Mat> *focusMapsOut = nullptr);

} // namespace FSFocus

#endif // FSFOCUS_H
