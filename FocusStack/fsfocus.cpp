#include "FSFocus.h"

#include <QDir>
#include <QFileInfoList>
#include <QDebug>

#include <opencv2/core.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>

namespace FSFocus {

bool run(const QString    &alignFolder,
         const QString    &focusFolder,
         const Options    &opt,
         std::atomic_bool *abortFlag,
         ProgressCallback  progressCb,
         StatusCallback    statusCb)
{
    QDir inDir(alignFolder);
    if (!inDir.exists()) {
        if (statusCb) statusCb("FSFocus: Align folder does not exist: " + alignFolder, true);
        return false;
    }

    QDir outDir(focusFolder);
    if (!outDir.exists() && !outDir.mkpath(".")) {
        if (statusCb) statusCb("FSFocus: Unable to create focus folder: " + focusFolder, true);
        return false;
    }

    QFileInfoList files = inDir.entryInfoList(QStringList() << "*.tif" << "*.tiff" << "*.jpg" << "*.jpeg" << "*.png",
                                              QDir::Files, QDir::Name);
    const int total = files.size();
    if (total == 0) {
        if (statusCb) statusCb("FSFocus: No images found in align folder.", true);
        return false;
    }

    const int numThreads = (opt.numThreads > 0 ? opt.numThreads : std::max(1, QThread::idealThreadCount()));

    if (statusCb) statusCb(
            QString("FSFocus: Computing focus maps for %1 images using %2 threads.")
                .arg(total).arg(numThreads),
            false
            );

    // Placeholder: simple serial focus metric (Laplacian variance).
    for (int i = 0; i < total; ++i) {
        if (abortFlag && abortFlag->load(std::memory_order_relaxed)) {
            if (statusCb) statusCb("FSFocus: Aborted by user.", true);
            return false;
        }

        const QFileInfo fi = files.at(i);
        const QString srcPath = fi.absoluteFilePath();
        const QString dstPath = outDir.absoluteFilePath(fi.completeBaseName() + "_focus.png");

        cv::Mat img = cv::imread(srcPath.toStdString(), cv::IMREAD_GRAYSCALE);
        if (img.empty()) {
            if (statusCb) statusCb("FSFocus: Failed to load " + srcPath, true);
            return false;
        }

        // TODO: replace with your FocusMeasure (including EmulateZerene).
        cv::Mat lap;
        cv::Laplacian(img, lap, CV_32F, 3);
        cv::Mat focus;
        cv::convertScaleAbs(lap, focus);

        if (!cv::imwrite(dstPath.toStdString(), focus)) {
            if (statusCb) statusCb("FSFocus: Failed to write " + dstPath, true);
            return false;
        }

        if (progressCb) {
            int percent = static_cast<int>((i + 1) * 100.0 / total);
            progressCb(percent);
        }
    }

    if (statusCb) statusCb("FSFocus: Focus map computation complete.", false);
    return true;
}

} // namespace FSFocus
