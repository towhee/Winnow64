#include "FSDepth.h"
#include "Main/global.h"

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
    QString srcFun = "FSDepth::run";

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

    QFileInfoList files = inDir.entryInfoList(QStringList()
                                                  << "focus_*.tif"
                                                  << "focus_*.tiff",
                                              QDir::Files,
                                              QDir::Name);
    const int total = files.size();
    if (total == 0) {
        if (statusCb) statusCb("FSDepth: No focus maps found.", true);
        return false;
    }

    if (statusCb) statusCb(
            QString("FSDepth: Building depth map from %1 focus maps.").arg(total),
            false
            );

    cv::Mat first = cv::imread(files.first().absoluteFilePath().toStdString(),
                               cv::IMREAD_UNCHANGED);
    if (first.empty()) {
        if (statusCb) statusCb("FSDepth: Failed to load first focus map.", true);
        return false;
    }

    if (first.type() != CV_16U && first.type() != CV_8U) {
        if (statusCb) statusCb("FSDepth: Unsupported focus map type (expected 16U or 8U).", true);
        return false;
    }

    const int rows   = first.rows;
    const int cols   = first.cols;
    const int slices = total;

    cv::Mat bestVal(rows, cols, CV_32F);
    bestVal.setTo(-1.0f);

    cv::Mat depthIndex(rows, cols, CV_16U);
    depthIndex.setTo(0);

    for (int s = 0; s < slices; ++s)
    {
        if (abortFlag && abortFlag->load(std::memory_order_relaxed)) {
            if (statusCb) statusCb("FSDepth: Aborted by user.", true);
            return false;
        }

        G::log(srcFun, "Slice " + QString::number(s));

        const QFileInfo fi = files.at(s);
        const QString srcPath = fi.absoluteFilePath();

        cv::Mat fm = cv::imread(srcPath.toStdString(), cv::IMREAD_UNCHANGED);
        if (fm.empty()) {
            if (statusCb) statusCb("FSDepth: Failed to load " + srcPath, true);
            return false;
        }

        if (fm.rows != rows || fm.cols != cols) {
            if (statusCb) statusCb("FSDepth: Focus map size mismatch in " + srcPath, true);
            return false;
        }

        cv::Mat fm32;
        if (fm.type() == CV_16U) {
            fm.convertTo(fm32, CV_32F);
        }
        else {
            fm.convertTo(fm32, CV_32F);
        }

        for (int y = 0; y < rows; ++y)
        {
            const float *fRow  = fm32.ptr<float>(y);
            float       *bRow  = bestVal.ptr<float>(y);
            uint16_t    *dRow  = depthIndex.ptr<uint16_t>(y);

            for (int x = 0; x < cols; ++x)
            {
                float v = fRow[x];
                if (v > bRow[x]) {
                    bRow[x] = v;
                    dRow[x] = static_cast<uint16_t>(s);
                }
            }
        }

        if (progressCb) {
            progressCb(s + 1);
        }
    }

    cv::medianBlur(depthIndex, depthIndex, 3);

    const QString depthIdxPath = outDir.absoluteFilePath("depth_index.png");
    if (!cv::imwrite(depthIdxPath.toStdString(), depthIndex)) {
        if (statusCb) statusCb("FSDepth: Failed to write depth_index.png", true);
        return false;
    }

    /*
    - Normalize depthIndex to 0..255 as a grayscale preview
    - Create a legend bar that shows the mapping from shade to slice index
    - Stack preview + legend vertically and save as depth_preview.png
    */
    if (opt.preview) {
        cv::Mat preview;
        cv::normalize(depthIndex, preview, 0, 255, cv::NORM_MINMAX);
        preview.convertTo(preview, CV_8U);

        int legendHeight = 40;
        int width        = preview.cols;
        int height       = preview.rows;
        int slices       = total;   // number of focus maps

        cv::Mat legend(legendHeight, width, CV_8U, cv::Scalar(255));

        for (int s = 0; s < slices; ++s) {
            // Horizontal range for this slice bin
            float t0 = static_cast<float>(s) / static_cast<float>(slices);
            float t1 = static_cast<float>(s + 1) / static_cast<float>(slices);

            int x0 = static_cast<int>(t0 * width);
            int x1 = static_cast<int>(t1 * width);
            if (x1 <= x0) x1 = x0 + 1;

            // Match the grayscale used in preview: 0..255 from slice index
            int gray = 0;
            if (slices > 1) {
                gray = static_cast<int>(255.0 * s / (slices - 1));
            }

            cv::rectangle(legend,
                          cv::Point(x0, 0),
                          cv::Point(x1 - 1, legendHeight - 1),
                          cv::Scalar(gray),
                          cv::FILLED);

            // Draw slice index labels if there is enough room
            bool drawLabel = false;
            if (slices <= 16) {
                drawLabel = true;
            }
            else if (slices <= 32 && (s % 2 == 0)) {
                drawLabel = true;
            }
            else if (slices > 32 && (s % 4 == 0)) {
                drawLabel = true;
            }

            if (drawLabel) {
                QString label = QString::number(s);
                int textGray  = (gray < 128 ? 255 : 0);

                cv::putText(legend,
                            label.toStdString(),
                            cv::Point(x0 + 2, legendHeight - 8),
                            cv::FONT_HERSHEY_SIMPLEX,
                            1.2,
                            cv::Scalar(textGray),
                            1,
                            cv::LINE_AA);
            }
        }

        cv::Mat previewWithLegend(height + legendHeight, width, CV_8U);
        preview.copyTo(previewWithLegend(cv::Rect(0, 0, width, height)));
        legend.copyTo(previewWithLegend(cv::Rect(0, height, width, legendHeight)));

        const QString depthPreviewPath = outDir.absoluteFilePath("depth_preview.png");
        if (!cv::imwrite(depthPreviewPath.toStdString(), previewWithLegend)) {
            if (statusCb) statusCb("FSDepth: Failed to write depth_preview.png", true);
            return false;
        }
    }

    if (statusCb) statusCb("FSDepth: Depth map computation complete.", false);
    return true;
}

} // namespace FSDepth
