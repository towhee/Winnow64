#include "FSDepth.h"
#include "Main/global.h"

#include <QDir>
#include <QFileInfoList>
#include <QDebug>

#include <opencv2/core.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>

#include "FocusStack/fsfusionwavelet.h"
#include "FocusStack/fsfusionmerge.h"
#include "FocusStack/FSUtilities.h"

namespace {

//----------------------------------------------------------
// Shared: build enhanced preview (grayscale + heatmap + legend)
//----------------------------------------------------------
cv::Mat makeDepthPreviewEnhanced(const cv::Mat &depthIndex16,
                                 int sliceCount)
{
    CV_Assert(depthIndex16.type() == CV_16U);

    int rows = depthIndex16.rows;
    int cols = depthIndex16.cols;

    // --- Base grayscale preview (0..255) ---
    cv::Mat gray8;
    cv::normalize(depthIndex16, gray8, 0, 255, cv::NORM_MINMAX);
    gray8.convertTo(gray8, CV_8U);

    // --- Heatmap preview (same depth, colored) ---
    cv::Mat heatColor;
    cv::applyColorMap(gray8, heatColor, cv::COLORMAP_JET);

    // --- Legend bar ---
    int legendHeight = 40;
    cv::Mat legend(legendHeight, cols, CV_8UC3, cv::Scalar(255, 255, 255));

    for (int s = 0; s < sliceCount; ++s)
    {
        float t0 = static_cast<float>(s) / static_cast<float>(sliceCount);
        float t1 = static_cast<float>(s + 1) / static_cast<float>(sliceCount);

        int x0 = static_cast<int>(t0 * cols);
        int x1 = static_cast<int>(t1 * cols);
        if (x1 <= x0) x1 = x0 + 1;

        int gray = 0;
        if (sliceCount > 1)
            gray = static_cast<int>(255.0 * s / (sliceCount - 1));

        uchar g = static_cast<uchar>(gray);
        cv::Scalar col(g, g, g);

        cv::rectangle(legend,
                      cv::Point(x0, 0),
                      cv::Point(x1 - 1, legendHeight - 1),
                      col,
                      cv::FILLED);

        // Decide which ticks to label
        bool drawLabel = false;
        if (sliceCount <= 16) {
            drawLabel = true;
        }
        else if (sliceCount <= 32 && (s % 2 == 0)) {
            drawLabel = true;
        }
        else if (sliceCount > 32 && (s % 4 == 0)) {
            drawLabel = true;
        }

        if (drawLabel)
        {
            QString label = QString::number(s);
            int textGray  = (gray < 128 ? 255 : 0);

            cv::putText(legend,
                        label.toStdString(),
                        cv::Point(x0 + 2, legendHeight - 10),
                        cv::FONT_HERSHEY_SIMPLEX,
                        0.6,
                        cv::Scalar(textGray, textGray, textGray),
                        1,
                        cv::LINE_AA);
        }
    }

    // Stack: [gray] [heat] [legend] vertically
    cv::Mat grayColor;
    cv::cvtColor(gray8, grayColor, cv::COLOR_GRAY2BGR);

    cv::Mat out(gray8.rows + heatColor.rows + legendHeight, cols, CV_8UC3);
    grayColor.copyTo(out(cv::Rect(0, 0, cols, rows)));
    heatColor.copyTo(out(cv::Rect(0, rows, cols, heatColor.rows)));
    legend.copyTo(out(cv::Rect(0, rows + heatColor.rows, cols, legendHeight)));

    return out;
}

//----------------------------------------------------------
// Simple method: scalar max over focus_*.tif (current behavior)
//----------------------------------------------------------
bool runSimple(const QString    &focusFolder,
               const QString    &depthFolder,
               const FSDepth::Options &opt,
               std::atomic_bool *abortFlag,
               FSDepth::ProgressCallback  progressCb,
               FSDepth::StatusCallback    statusCb)
{
    QString srcFun = "FSDepth::runSimple";

    QDir inDir(focusFolder);
    if (!inDir.exists()) {
        if (statusCb) statusCb("FSDepth(Simple): Focus folder does not exist: " + focusFolder, true);
        return false;
    }

    QDir outDir(depthFolder);
    if (!outDir.exists() && !outDir.mkpath(".")) {
        if (statusCb) statusCb("FSDepth(Simple): Unable to create depth folder: " + depthFolder, true);
        return false;
    }

    QFileInfoList files = inDir.entryInfoList(QStringList()
                                                  << "focus_*.tif"
                                                  << "focus_*.tiff",
                                              QDir::Files,
                                              QDir::Name);
    const int total = files.size();
    if (total == 0) {
        if (statusCb) statusCb("FSDepth(Simple): No focus maps found.", true);
        return false;
    }

    if (statusCb) statusCb(
            QString("FSDepth(Simple): Building depth map from %1 focus maps.").arg(total),
            false);

    cv::Mat first = cv::imread(files.first().absoluteFilePath().toStdString(),
                               cv::IMREAD_UNCHANGED);
    if (first.empty()) {
        if (statusCb) statusCb("FSDepth(Simple): Failed to load first focus map.", true);
        return false;
    }

    if (first.type() != CV_16U && first.type() != CV_8U) {
        if (statusCb) statusCb("FSDepth(Simple): Unsupported focus map type (expected 16U or 8U).", true);
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
            if (statusCb) statusCb("FSDepth(Simple): Aborted by user.", true);
            return false;
        }

        G::log(srcFun, "Slice " + QString::number(s));

        const QFileInfo fi = files.at(s);
        const QString srcPath = fi.absoluteFilePath();

        cv::Mat fm = cv::imread(srcPath.toStdString(), cv::IMREAD_UNCHANGED);
        if (fm.empty()) {
            if (statusCb) statusCb("FSDepth(Simple): Failed to load " + srcPath, true);
            return false;
        }

        if (fm.rows != rows || fm.cols != cols) {
            if (statusCb) statusCb("FSDepth(Simple): Focus map size mismatch in " + srcPath, true);
            return false;
        }

        cv::Mat fm32;
        fm.convertTo(fm32, CV_32F);

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

        if (progressCb)
            progressCb(s + 1);
    }

    cv::medianBlur(depthIndex, depthIndex, 3);

    const QString depthIdxPath = outDir.absoluteFilePath("depth_index.png");
    if (!cv::imwrite(depthIdxPath.toStdString(), depthIndex)) {
        if (statusCb) statusCb("FSDepth(Simple): Failed to write depth_index.png", true);
        return false;
    }

    if (opt.preview) {
        cv::Mat preview = makeDepthPreviewEnhanced(depthIndex, slices);
        const QString depthPreviewPath = outDir.absoluteFilePath("depth_preview.png");
        if (!cv::imwrite(depthPreviewPath.toStdString(), preview)) {
            if (statusCb) statusCb("FSDepth(Simple): Failed to write depth_preview.png", true);
            return false;
        }
    }

    if (statusCb) statusCb("FSDepth(Simple): Depth map computation complete.", false);
    return true;
}

//----------------------------------------------------------
// MultiScale A: wavelet-based depth using FSFusionWavelet + FSFusionMerge
//----------------------------------------------------------
bool runMultiScale(const QString    &focusFolder,
                   const QString    &depthFolder,
                   const FSDepth::Options &opt,
                   std::atomic_bool *abortFlag,
                   FSDepth::ProgressCallback  progressCb,
                   FSDepth::StatusCallback    statusCb)
{
    QString srcFun = "FSDepth::runMultiScale";

    // Source of grayscale slices: alignFolder if provided, else focusFolder
    QString grayRoot = opt.alignFolder.isEmpty() ? focusFolder : opt.alignFolder;

    QDir inDir(grayRoot);
    if (!inDir.exists()) {
        if (statusCb) statusCb("FSDepth(MultiScale): Gray root does not exist: " + grayRoot, true);
        return false;
    }

    QDir outDir(depthFolder);
    if (!outDir.exists() && !outDir.mkpath(".")) {
        if (statusCb) statusCb("FSDepth(MultiScale): Unable to create depth folder: " + depthFolder, true);
        return false;
    }

    // We follow FSFocus naming: gray_*.tif etc.
    QFileInfoList files = inDir.entryInfoList(
        {"gray_*.tif", "gray_*.tiff", "gray_*.png", "gray_*.jpg", "gray_*.jpeg"},
        QDir::Files,
        QDir::Name);

    int total = files.size();
    if (total == 0) {
        if (statusCb) statusCb("FSDepth(MultiScale): No grayscale aligned slices found in " + grayRoot, true);
        return false;
    }

    if (statusCb)
        statusCb(QString("FSDepth(MultiScale): %1 gray slices detected.").arg(total), false);

    // --- Load first image to determine size and padding ---
    cv::Mat firstGray = cv::imread(files.first().absoluteFilePath().toStdString(),
                                   cv::IMREAD_GRAYSCALE);
    if (firstGray.empty()) {
        if (statusCb) statusCb("FSDepth(MultiScale): Failed to load first gray slice.", true);
        return false;
    }

    cv::Size origSize = firstGray.size();
    cv::Size paddedSize;
    FSFusionWavelet::computeLevelsAndExpandedSize(origSize, paddedSize);

    int padW = paddedSize.width  - origSize.width;
    int padH = paddedSize.height - origSize.height;
    int left   = padW / 2;
    int right  = padW - left;
    int top    = padH / 2;
    int bottom = padH - top;

    // --- Build padded gray stack and wavelets ---
    std::vector<cv::Mat> wavelets(total);

    for (int s = 0; s < total; ++s)
    {
        qApp->processEvents();
        if (abortFlag && abortFlag->load(std::memory_order_relaxed)) {
            if (statusCb) statusCb("FSDepth(MultiScale): Aborted by user.", true);
            return false;
        }

        QFileInfo fi = files[s];
        QString srcPath = fi.absoluteFilePath();

        cv::Mat gray = cv::imread(srcPath.toStdString(), cv::IMREAD_GRAYSCALE);
        if (gray.empty()) {
            if (statusCb) statusCb("FSDepth(MultiScale): Cannot load " + srcPath, true);
            return false;
        }

        if (gray.size() != origSize) {
            if (statusCb) statusCb("FSDepth(MultiScale): Gray size mismatch in " + srcPath, true);
            return false;
        }

        // Pad to wavelet-friendly size
        cv::Mat grayPadded;
        if (paddedSize == origSize) {
            grayPadded = gray;
        } else {
            cv::copyMakeBorder(gray,
                               grayPadded,
                               top, bottom,
                               left, right,
                               cv::BORDER_REFLECT);
        }

        // Wavelet transform using same logic as FSFusionWavelet::forward
        if (!FSFusionWavelet::forward(grayPadded, opt.numThreads > 0 ? true : true, wavelets[s])) {
            if (statusCb) statusCb("FSDepth(MultiScale): Wavelet forward failed for " + srcPath, true);
            return false;
        }

        if (opt.saveWaveletDebug) {
            // magnitude, log-compressed, colorized
            cv::Mat complexMag;
            {
                cv::Mat mag32;
                std::vector<cv::Mat> ch(2);
                cv::split(wavelets[s], ch);
                cv::magnitude(ch[0], ch[1], mag32);

                mag32 += 1.0f;
                cv::log(mag32, mag32);

                double vmin, vmax;
                cv::minMaxLoc(mag32, &vmin, &vmax);
                cv::Mat mag8;
                mag32.convertTo(mag8, CV_8U,
                                255.0 / (vmax - vmin),
                                -vmin * 255.0 / (vmax - vmin));

                cv::applyColorMap(mag8, complexMag, cv::COLORMAP_VIRIDIS);
            }

            QString dbgPath = outDir.absoluteFilePath(
                QString("wavelet_mag_%1.png").arg(s, 3, 10, QChar('0')));
            cv::imwrite(dbgPath.toStdString(), complexMag);
        }

        if (progressCb)
            progressCb(s + 1);
    }

    // --- Merge wavelet stack into mergedWavelet + depthIndex (padded size) ---
    cv::Mat mergedWavelet;
    cv::Mat depthIndexPadded16;

    if (!FSFusionMerge::merge(wavelets,
                              /*consistency=*/2,
                              abortFlag,
                              mergedWavelet,
                              depthIndexPadded16))
    {
        if (statusCb) statusCb("FSDepth(MultiScale): FSFusionMerge::merge failed.", true);
        return false;
    }

    qApp->processEvents(); if (abortFlag) return false;

    // Optional merged wavelet debug
    if (opt.saveWaveletDebug) {
        cv::Mat magMergedColor;
        {
            std::vector<cv::Mat> ch(2);
            cv::split(mergedWavelet, ch);
            cv::Mat mag32;
            cv::magnitude(ch[0], ch[1], mag32);

            mag32 += 1.0f;
            cv::log(mag32, mag32);

            double vmin, vmax;
            cv::minMaxLoc(mag32, &vmin, &vmax);
            cv::Mat mag8;
            mag32.convertTo(mag8, CV_8U,
                            255.0 / (vmax - vmin),
                            -vmin * 255.0 / (vmax - vmin));

            cv::applyColorMap(mag8, magMergedColor, cv::COLORMAP_VIRIDIS);
        }

        QString dbgMerged = outDir.absoluteFilePath("wavelet_mag_merged.png");
        cv::imwrite(dbgMerged.toStdString(), magMergedColor);
    }

    qApp->processEvents(); if (abortFlag) return false;

    // --- Crop depthIndex to original size (per Q1 = 1) ---
    cv::Mat depthIndex16;
    if (paddedSize == origSize) {
        depthIndex16 = depthIndexPadded16;
    } else {
        cv::Rect roi(left, top, origSize.width, origSize.height);
        depthIndex16 = depthIndexPadded16(roi).clone();
    }

    const QString depthIdxPath = outDir.absoluteFilePath("depth_index.png");
    if (!cv::imwrite(depthIdxPath.toStdString(), depthIndex16)) {
        if (statusCb) statusCb("FSDepth(MultiScale): Failed to write depth_index.png", true);
        return false;
    }

    qApp->processEvents(); if (abortFlag) return false;

    if (opt.preview)
    {
        cv::Mat preview = makeDepthPreviewEnhanced(depthIndex16, total);
        const QString depthPreviewPath = outDir.absoluteFilePath("depth_preview.png");
        if (!cv::imwrite(depthPreviewPath.toStdString(), preview)) {
            if (statusCb) statusCb("FSDepth(MultiScale): Failed to write depth_preview.png", true);
            return false;
        }
    }

    if (statusCb) statusCb("FSDepth(MultiScale): Depth map computation complete.", false);
    return true;
}

} // anonymous namespace

//----------------------------------------------------------------------
// Public entry point
//----------------------------------------------------------------------
namespace FSDepth {

bool run(const QString    &focusFolder,
         const QString    &depthFolder,
         const Options    &opt,
         std::atomic_bool *abortFlag,
         ProgressCallback  progressCb,
         StatusCallback    statusCb)
{
    const QString method = opt.method.trimmed();

    if (method.compare("MultiScale", Qt::CaseInsensitive) == 0)
        return runMultiScale(focusFolder, depthFolder, opt,
                             abortFlag, progressCb, statusCb);
    else
        return runSimple(focusFolder, depthFolder, opt,
                         abortFlag, progressCb, statusCb);
}

} // namespace FSDepth
