#include "FSDepth.h"
#include "Main/global.h"

#include <QDir>
#include <QFileInfoList>
#include <QDebug>

#include <opencv2/core.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>

#include "FocusStack/fsfusionwavelet.h"
#include "FocusStack/fsmerge.h"
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
bool runSimple(const QString &focusFolder,
               const QString &depthFolder,
               const FSDepth::Options &opt,
               std::atomic_bool *abortFlag,
               FSDepth::ProgressCallback progressCb,
               FSDepth::StatusCallback statusCb,
               cv::Mat *depthIndex16Out)
{
    QString srcFun = "FSDepth::runSimple";

    QDir inDir(focusFolder);
    if (!inDir.exists()) {
        if (statusCb) statusCb("FSDepth(Simple): Focus folder does not exist: " + focusFolder);
        return false;
    }

    QDir outDir(depthFolder);
    if (!outDir.exists() && !outDir.mkpath(".")) {
        if (statusCb) statusCb("FSDepth(Simple): Unable to create depth folder: " + depthFolder);
        return false;
    }

    QFileInfoList files = inDir.entryInfoList(QStringList()
                                                  << "focus_*.tif"
                                                  << "focus_*.tiff",
                                              QDir::Files,
                                              QDir::Name);
    const int total = files.size();
    if (total == 0) {
        if (statusCb) statusCb("FSDepth(Simple): No focus maps found.");
        return false;
    }

    if (statusCb) statusCb(
            QString("FSDepth(Simple): Building depth map from %1 focus maps.").arg(total));

    cv::Mat first = cv::imread(files.first().absoluteFilePath().toStdString(),
                               cv::IMREAD_UNCHANGED);
    if (first.empty()) {
        if (statusCb) statusCb("FSDepth(Simple): Failed to load first focus map.");
        return false;
    }

    if (first.type() != CV_16U && first.type() != CV_8U) {
        if (statusCb) statusCb("FSDepth(Simple): Unsupported focus map type (expected 16U or 8U).");
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
            if (statusCb) statusCb("FSDepth(Simple): Aborted by user.");
            return false;
        }

        if (G::FSLog) G::log(srcFun, "Slice " + QString::number(s));

        const QFileInfo fi = files.at(s);
        const QString srcPath = fi.absoluteFilePath();

        cv::Mat fm = cv::imread(srcPath.toStdString(), cv::IMREAD_UNCHANGED);
        if (fm.empty()) {
            if (statusCb) statusCb("FSDepth(Simple): Failed to load " + srcPath);
            return false;
        }

        if (fm.rows != rows || fm.cols != cols) {
            if (statusCb) statusCb("FSDepth(Simple): Focus map size mismatch in " + srcPath);
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
    if (!FSUtilities::writePngWithTitle(depthIdxPath, depthIndex)) {
        if (statusCb) statusCb("FSDepth(Simple): Failed to write depth_index.png");
        return false;
    }

    if (opt.preview) {
        cv::Mat preview = makeDepthPreviewEnhanced(depthIndex, slices);
        const QString depthPreviewPath = outDir.absoluteFilePath("depth_preview.png");
        if (!FSUtilities::writePngWithTitle(depthPreviewPath, preview)) {
            if (statusCb) statusCb("FSDepth(Simple): Failed to write depth_preview.png");
            return false;
        }
    }
    if (statusCb) statusCb("FSDepth(Simple): Depth map computation complete.");
    return true;
}

//----------------------------------------------------------
// MultiScale A: wavelet-based depth using FSFusionWavelet + FSMerge
//----------------------------------------------------------

bool runMultiScale(
    const std::vector<cv::Mat> &graySlices,      // CV_8U or CV_32F
    const QString              &depthFolder,
    const FSDepth::Options     &opt,
    std::atomic_bool           *abortFlag,
    FSDepth::ProgressCallback   progressCb,
    FSDepth::StatusCallback     statusCb,
    cv::Mat                    *depthIndex16Out)
{
    const QString srcFun = "FSDepth::runMultiScaleFromGrayMats";

    if (G::FSLog) G::log(srcFun);

    if (graySlices.empty())
    {
        if (statusCb) statusCb("FSDepth(MultiScale): No gray slices provided.");
        return false;
    }

    const int total = static_cast<int>(graySlices.size());
    const cv::Size origSize = graySlices.front().size();

    // --- Validate input ---
    for (int i = 0; i < total; ++i)
    {
        if (graySlices[i].empty() || graySlices[i].size() != origSize)
        {
            if (statusCb)
                statusCb(QString("FSDepth(MultiScale): Invalid gray slice %1").arg(i));
            return false;
        }
    }

    if (statusCb)
        statusCb(QString("FSDepth(MultiScale): %1 gray slices (memory)").arg(total));

    // --- Compute wavelet-friendly padded size ---
    cv::Size paddedSize;
    FSFusionWavelet::computeLevelsAndExpandedSize(origSize, paddedSize);

    const int padW = paddedSize.width  - origSize.width;
    const int padH = paddedSize.height - origSize.height;
    const int left   = padW / 2;
    const int right  = padW - left;
    const int top    = padH / 2;
    const int bottom = padH - top;

    // --- Build wavelet stack ---
    std::vector<cv::Mat> wavelets(total);

    for (int s = 0; s < total; ++s)
    {
        if (abortFlag && abortFlag->load(std::memory_order_relaxed))
            return false;

        if (statusCb)
            statusCb(QString("FSDepth(MultiScale): Wavelet slice %1").arg(s));

        cv::Mat gray = graySlices[s];
        cv::Mat gray8;

        // Normalize to CV_8U for wavelet input
        if (gray.type() == CV_32F)
            gray.convertTo(gray8, CV_8U, 255.0);
        else
            gray8 = gray;

        cv::Mat grayPadded;
        if (paddedSize == origSize)
        {
            grayPadded = gray8;
        }
        else
        {
            cv::copyMakeBorder(gray8, grayPadded,
                               top, bottom, left, right,
                               cv::BORDER_REFLECT);
        }

        if (!FSFusionWavelet::forward(grayPadded,
                                      /*useOpenCL=*/true,
                                      wavelets[s]))
        {
            if (statusCb)
                statusCb(QString("FSDepth(MultiScale): Wavelet failed at slice %1").arg(s));
            return false;
        }

        if (progressCb)
            progressCb(s + 1);

        if (G::FSLog) G::log(srcFun, "Slice " + QString::number(s));
    }

    // --- Merge wavelets → depth index (padded) ---
    cv::Mat mergedWavelet;
    cv::Mat depthIndexPadded16;

    if (!FSMerge::merge(wavelets,
                        /*consistency=*/2,
                        abortFlag,
                        mergedWavelet,
                        depthIndexPadded16))
    {
        if (statusCb)
            statusCb("FSDepth(MultiScale): FSMerge failed.");
        return false;
    }

    // --- Crop to original size ---
    cv::Mat depthIndex16;
    if (paddedSize == origSize)
    {
        depthIndex16 = depthIndexPadded16;
    }
    else
    {
        depthIndex16 = depthIndexPadded16(
                           cv::Rect(left, top, origSize.width, origSize.height)
                           ).clone();
    }

    // --- Output to caller ---
    if (depthIndex16Out)
        depthIndex16.copyTo(*depthIndex16Out);

    // --- Optional disk outputs ---
    QDir outDir(depthFolder);
    if (!outDir.exists())
        outDir.mkpath(".");

    if (opt.preview || opt.keep)
    {
        const QString p = outDir.absoluteFilePath("depth_index.png");
        if (!FSUtilities::writePngWithTitle(p, depthIndex16))
            return false;
    }

    if (opt.preview)
    {
        cv::Mat preview = makeDepthPreviewEnhanced(depthIndex16, total);
        const QString p = outDir.absoluteFilePath("depth_preview.png");
        if (!FSUtilities::writePngWithTitle(p, preview))
            return false;
    }

    if (statusCb)
        statusCb("FSDepth(MultiScale): Depth map complete.");

    return true;
}

bool runMultiScaleFromDisk(const QString    &focusFolder,
                   const QString            &depthFolder,
                   const FSDepth::Options   &opt,
                   std::atomic_bool         *abortFlag,
                   FSDepth::ProgressCallback progressCb,
                   FSDepth::StatusCallback   statusCb,
                   cv::Mat                  *depthIndex16Out)
{
    const QString srcFun = "FSDepth::runMultiScale";

    // Source of grayscale slices
    const QString grayRoot =
        opt.alignFolder.isEmpty() ? focusFolder : opt.alignFolder;

    QDir inDir(grayRoot);
    if (!inDir.exists())
    {
        if (statusCb)
            statusCb("FSDepth(MultiScale): Gray source folder not found.");
        return false;
    }

    QFileInfoList files = inDir.entryInfoList(
        {"gray_*.tif", "gray_*.tiff", "gray_*.png", "gray_*.jpg", "gray_*.jpeg"},
        QDir::Files,
        QDir::Name
        );

    if (files.isEmpty())
    {
        if (statusCb)
            statusCb("FSDepth(MultiScale): No grayscale slices found.");
        return false;
    }

    std::vector<cv::Mat> graySlices;
    graySlices.reserve(files.size());

    for (const QFileInfo &fi : files)
    {
        if (abortFlag && abortFlag->load(std::memory_order_relaxed))
            return false;

        cv::Mat g = cv::imread(fi.absoluteFilePath().toStdString(),
                               cv::IMREAD_GRAYSCALE);
        if (g.empty())
        {
            if (statusCb)
                statusCb("FSDepth(MultiScale): Failed to load " + fi.fileName());
            return false;
        }

        graySlices.push_back(g);
    }

    // return true;

    // Delegate to the core implementation
    return runMultiScale(
        graySlices,
        depthFolder,
        opt,
        abortFlag,
        progressCb,
        statusCb,
        depthIndex16Out
        );
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
         StatusCallback    statusCb,
         cv::Mat *depthIndex16Out)
{
    const QString method = opt.method.trimmed();

    qDebug() << "FSDepth::run method =" << opt.method;
    qDebug() << "FSDepth::run statusCb valid?" << static_cast<bool>(statusCb);

    if (method.compare("MultiScale", Qt::CaseInsensitive) == 0)
        return runMultiScaleFromDisk(focusFolder, depthFolder, opt, abortFlag,
                             progressCb, statusCb, depthIndex16Out);
    else
        return runSimple(focusFolder, depthFolder, opt, abortFlag,
                         progressCb, statusCb, depthIndex16Out);
}

bool runFromGraySlices(
    const std::vector<cv::Mat> &graySlices,
    const QString              &depthFolder,
    const Options              &opt,
    std::atomic_bool           *abortFlag,
    ProgressCallback            progressCb,
    StatusCallback              statusCb,
    cv::Mat                    *depthIndex16Out)
{
    if (graySlices.empty())
    {
        if (statusCb) {
            QString msg = "FSDepth::runFromGraySlices No in-memory gray slices provided.";
            statusCb(msg);
            qWarning() << "WARNING:" << msg;
        }
        return false;
    }

    // Validate consistency
    const cv::Size sz = graySlices.front().size();
    for (const auto &m : graySlices)
    {
        if (m.empty() || m.size() != sz)
        {
            if (statusCb) {
                QString msg = "FSDepth::runFromGraySlices Gray slice size mismatch.";
                statusCb(msg);
                qWarning() << "WARNING:" << msg;
            }
            return false;
        }
    }

    return runMultiScale(
        graySlices,
        depthFolder,
        opt,
        abortFlag,
        progressCb,
        statusCb,
        depthIndex16Out
        );
}

} // namespace FSDepth
