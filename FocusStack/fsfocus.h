#ifndef FSFOCUS_H
#define FSFOCUS_H

#include <QString>
#include <QThread>
#include <atomic>
#include <functional>

namespace FSFocus {

struct Options
{
    int  downsampleFactor = 1;
    bool saveDebug        = false;
    int  numThreads       = 0; // 0 = auto
    // future: enum Method { Laplacian, EmulateZerene, ... }
};

using ProgressCallback = std::function<void(int)>;
using StatusCallback   = std::function<void(const QString &message, bool isError)>;

// Input:  aligned images in alignFolder
// Output: per-slice focus maps in focusFolder
bool run(const QString    &alignFolder,
         const QString    &focusFolder,
         const Options    &opt,
         std::atomic_bool *abortFlag,
         ProgressCallback  progressCb,
         StatusCallback    statusCb);

} // namespace FSFocus

#endif // FSFOCUS_H
