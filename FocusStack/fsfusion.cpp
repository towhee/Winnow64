#include "FSFusion.h"

#include <QDir>
#include <QFileInfoList>
#include <QDebug>

#include <opencv2/core.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>

namespace FSFusion {

bool run(const QString    &alignedFolder,
         const QString    &depthFolder,
         const QString    &fusionFolder,
         const Options    &opt,
         std::atomic_bool *abortFlag,
         ProgressCallback  progressCb,
         StatusCallback    statusCb)
{
    QDir alignDir(alignedFolder);
    if (!alignDir.exists()) {
        if (statusCb) statusCb("FSFusion: Align folder does not exist: " + alignedFolder, true);
        return false;
    }

    QDir depthDir(depthFolder);
    if (!depthDir.exists()) {
        if (statusCb) statusCb("FSFusion: Depth folder does not exist: " + depthFolder, true);
        return false;
    }

    QDir outDir(fusionFolder);
    if (!outDir.exists() && !outDir.mkpath(".")) {
        if (statusCb) statusCb("FSFusion: Unable to create fusion folder: " + fusionFolder, true);
        return false;
    }

    const QString depthIdxPath = depthDir.absoluteFilePath("depth_index.png");
    cv::Mat depthIndex = cv::imread(depthIdxPath.toStdString(), cv::IMREAD_GRAYSCALE);
    if (depthIndex.empty()) {
        if (statusCb) statusCb("FSFusion: Failed to load depth_index.png", true);
        return false;
    }

    QFileInfoList files = alignDir.entryInfoList(QStringList() << "*.tif" << "*.tiff" << "*.jpg" << "*.jpeg" << "*.png",
                                                 QDir::Files, QDir::Name);
    const int slices = files.size();
    if (slices == 0) {
        if (statusCb) statusCb("FSFusion: No aligned images found.", true);
        return false;
    }

    if (statusCb) statusCb(
            QString("FSFusion: Fusing %1 aligned images using depth index.").arg(slices),
            false
            );

    const int rows = depthIndex.rows;
    const int cols = depthIndex.cols;

    int cvType = opt.write16Bit ? CV_16UC3 : CV_8UC3;
    cv::Mat fused(rows, cols, cvType, cv::Scalar::all(0));

    // Very simple "nearest slice" fusion for now (placeholder, not PMax).
    // This is where you will drop in Petteri's wavelet PMax.
    for (int y = 0; y < rows; ++y) {
        if (abortFlag && abortFlag->load(std::memory_order_relaxed)) {
            if (statusCb) statusCb("FSFusion: Aborted by user.", true);
            return false;
        }

        for (int x = 0; x < cols; ++x) {
            uchar idx = depthIndex.at<uchar>(y, x);
            int sliceIdx = std::min<int>(idx, slices - 1);

            cv::Mat img = cv::imread(files.at(sliceIdx).absoluteFilePath().toStdString(),
                                     cv::IMREAD_COLOR);
            if (img.empty())
                continue;

            if (img.rows != rows || img.cols != cols) {
                cv::resize(img, img, cv::Size(cols, rows), 0, 0, cv::INTER_LINEAR);
            }

            if (cvType == CV_8UC3) {
                fused.at<cv::Vec3b>(y, x) = img.at<cv::Vec3b>(y, x);
            }
            else {
                cv::Vec3b pix8 = img.at<cv::Vec3b>(y, x);
                fused.at<cv::Vec3w>(y, x) = cv::Vec3w(
                    static_cast<uint16_t>(pix8[0]) << 8,
                    static_cast<uint16_t>(pix8[1]) << 8,
                    static_cast<uint16_t>(pix8[2]) << 8
                    );
            }
        }

        if (progressCb) {
            int percent = static_cast<int>((y + 1) * 100.0 / rows);
            progressCb(percent);
        }
    }

    const QString fusedPath = outDir.absoluteFilePath("fused_pmax.tif");
    if (!cv::imwrite(fusedPath.toStdString(), fused)) {
        if (statusCb) statusCb("FSFusion: Failed to write fused_pmax.tif", true);
        return false;
    }

    if (statusCb) statusCb("FSFusion: Fusion complete.", false);
    return true;
}

} // namespace FSFusion
