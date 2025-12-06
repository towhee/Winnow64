#include "FSGray.h"

#include <QFileInfo>
#include <QDir>
#include <QThread>

#include <opencv2/core.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>

namespace FSGray {

bool run(const QStringList &alignedPaths,
         const QStringList &grayPaths,
         const Options     &opt,
         std::atomic_bool  *abortFlag,
         ProgressCallback   progressCb,
         StatusCallback     statusCb)
{
    if (alignedPaths.size() != grayPaths.size()) {
        if (statusCb) statusCb("FSGray: alignedPaths and grayPaths size mismatch.", true);
        return false;
    }

    int total = alignedPaths.size();
    if (total == 0) {
        if (statusCb) statusCb("FSGray: No aligned images to convert.", true);
        return false;
    }

    int numThreads =
        (opt.numThreads > 0 ? opt.numThreads : std::max(1, QThread::idealThreadCount()));

    if (statusCb) {
        statusCb(
            QString("FSGray: Converting %1 images to grayscale using %2 threads.")
                .arg(total).arg(numThreads),
            false
            );
    }

    for (int i = 0; i < total; ++i) {

        if (abortFlag && abortFlag->load(std::memory_order_relaxed)) {
            if (statusCb) statusCb("FSGray: Aborted by user.", true);
            return false;
        }

        const QString &srcPath = alignedPaths.at(i);
        const QString &dstPath = grayPaths.at(i);

        cv::Mat img = cv::imread(srcPath.toStdString(), cv::IMREAD_UNCHANGED);
        if (img.empty()) {
            if (statusCb) statusCb("FSGray: Failed to load " + srcPath, true);
            return false;
        }

        cv::Mat gray;

        //
        // If the pipeline is 16-bit, preserve bit depth.
        //
        if (opt.use16Bit && img.depth() == CV_16U) {
            cv::cvtColor(img, gray, cv::COLOR_BGR2GRAY);
            // gray is CV_16UC1
        }
        else {
            cv::cvtColor(img, gray, cv::COLOR_BGR2GRAY);
            // gray is CV_8UC1
        }

        if (!cv::imwrite(dstPath.toStdString(), gray)) {
            if (statusCb) statusCb("FSGray: Failed to write " + dstPath, true);
            return false;
        }

        if (progressCb) {
            int percent = static_cast<int>((i + 1) * 100.0 / total);
            progressCb(percent);
        }
    }

    if (statusCb) statusCb("FSGray: Grayscale conversion complete.", false);
    return true;
}

} // namespace FSGray
