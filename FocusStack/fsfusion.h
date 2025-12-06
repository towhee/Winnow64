#ifndef FSFUSION_H
#define FSFUSION_H

#include <QString>
#include <atomic>
#include <functional>

namespace FSFusion {

struct Options
{
    bool write16Bit      = true;
    bool saveDebug       = false;
    int  numThreads      = 0;
    // future: enum Method { PMax, DMap, PMax2, EmulateZerene, ... }
};

using ProgressCallback = std::function<void(int)>;
using StatusCallback   = std::function<void(const QString &message, bool isError)>;

// Input:  alignedFolder (images), depthFolder (depth_index/preview)
// Output: fused result(s) in fusionFolder
bool run(const QString    &alignedFolder,
         const QString    &depthFolder,
         const QString    &fusionFolder,
         const Options    &opt,
         std::atomic_bool *abortFlag,
         ProgressCallback  progressCb,
         StatusCallback    statusCb);

} // namespace FSFusion

#endif // FSFUSION_H
