#ifndef FSGRAY_H
#define FSGRAY_H

#include <QString>
#include <QStringList>
#include <atomic>
#include <functional>

namespace FSGray {

struct Options
{
    bool saveDebug   = false;
    int  numThreads  = 0;  // 0 = auto
    bool use16Bit    = false;
};

using ProgressCallback = std::function<void(int)>;
using StatusCallback   = std::function<void(const QString &message, bool isError)>;

// Convert aligned color images to grayscale
bool run(const QStringList &alignedPaths,
         const QStringList &grayPaths,
         const Options     &opt,
         std::atomic_bool  *abortFlag,
         ProgressCallback   progressCb,
         StatusCallback     statusCb);

} // namespace FSGray

#endif // FSGRAY_H
