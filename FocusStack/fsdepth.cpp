#include "FSDepth.h"

#include <QDir>
#include <QFileInfoList>
#include <QDebug>

#include <opencv2/core.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>

namespace FSDepth {

bool run(const QString    &focusFolder,
         const QString    &depthFolder,
         const Options    &opt,
         std::atomic_bool *abortFlag,
         ProgressCallback  progressCb,
         StatusCallback    statusCb)
{
    QDir inDir(focusFolder);
    if (!inDir.exists()) {
        if (statusCb) statusCb("FSDepth: Focus folder does not exist: " + focusFolder, true);
        return false;
    }

    QDir outDir(depthFolder);
    if (!outDir.exists() && !outDir.mkpath(".")) {
        if (statusCb) statusCb("FSDepth: Unable to create depth folder: " + depthFolder, true);
        return false;
    }

    QFileInfoList files = inDir.entryInfoList(QStringList() << "*_focus.png",
                                              QDir::Files, QDir::Name);
    const int total = files.size();
    if (total == 0) {
        if (statusCb) statusCb("FSDepth: No focus maps found.", true);
        return false;
    }

    if (statusCb) statusCb(
            QString("FSDepth: Building depth map from %1 focus maps.").arg(total),
            false
            );

    // Load all focus maps into a 3D stack [slice, y, x]
    cv::Mat first = cv::imread(files.first().absoluteFilePath().toStdString(),
                               cv::IMREAD_GRAYSCALE);
    if (first.empty()) {
        if (statusCb) statusCb("FSDepth: Failed to load first focus map.", true);
        return false;
    }

    const int rows = first.rows;
    const int cols = first.cols;
    const int slices = total;

    cv::Mat depthIndex(rows, cols, CV_8U, cv::Scalar(0));

    // Very simple argmax depth for now (single-threaded)
    for (int y = 0; y < rows; ++y) {
        if (abortFlag && abortFlag->load(std::memory_order_relaxed)) {
            if (statusCb) statusCb("FSDepth: Aborted by user.", true);
            return false;
        }

        for (int x = 0; x < cols; ++x) {
            int bestSlice = 0;
            uchar bestVal = 0;

            for (int s = 0; s < slices; ++s) {
                cv::Mat fm = cv::imread(files.at(s).absoluteFilePath().toStdString(),
                                        cv::IMREAD_GRAYSCALE);
                if (fm.empty()) continue;

                uchar val = fm.at<uchar>(y, x);
                if (val > bestVal) {
                    bestVal = val;
                    bestSlice = s;
                }
            }

            depthIndex.at<uchar>(y, x) = static_cast<uchar>(bestSlice);
        }

        if (progressCb) {
            int percent = static_cast<int>((y + 1) * 100.0 / rows);
            progressCb(percent);
        }
    }

    const QString depthIdxPath = outDir.absoluteFilePath("depth_index.png");
    if (!cv::imwrite(depthIdxPath.toStdString(), depthIndex)) {
        if (statusCb) statusCb("FSDepth: Failed to write depth_index.png", true);
        return false;
    }

    if (opt.savePreview) {
        cv::Mat preview;
        cv::normalize(depthIndex, preview, 0, 255, cv::NORM_MINMAX);
        const QString depthPreviewPath = outDir.absoluteFilePath("depth_preview.png");
        if (!cv::imwrite(depthPreviewPath.toStdString(), preview)) {
            if (statusCb) statusCb("FSDepth: Failed to write depth_preview.png", true);
            return false;
        }
    }

    if (statusCb) statusCb("FSDepth: Depth map computation complete.", false);
    return true;
}

} // namespace FSDepth
