#include "FSDepth.h"
#include "Main/global.h"

#include <QDir>
#include <QFileInfoList>
#include <QDebug>

#include <opencv2/core.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>

#include "FocusStack/fsfocus.h"
#include "FocusStack/fsfusionwavelet.h"
#include "FocusStack/fsmerge.h"
#include "FocusStack/FSUtilities.h"

/*
FSDepth implements the “depth map” stage of the focus stacking pipeline.  It
computes a per-pixel depth index (which slice is most in-focus) and writes a
stable on-disk representation plus optional human-friendly previews.

The module supports two depth-building methods:
    1.	Simple:     derive depth from existing focus maps on disk (focus_*.tif).
    2.	MultiScale: derive depth directly from aligned grayscale slices using the
        wavelet pipeline (FSFusionWavelet + FSMerge), producing a depth index that
        tends to be cleaner at multiple spatial scales.

Key outputs (written via FSUtilities::writePngWithTitle):
    •	depth_index.png   (CV_16U, pixel values are slice indices 0..N-1)
    •	depth_preview.png (optional, enhanced visualization)

CORE RESPONSIBILITIES
    1.	Build a depth index (slice selector)
        •	For every pixel, choose the slice with the strongest focus response.
        •	Store the winning slice index as a 16-bit value (CV_16U).
    2.	Provide two computation paths
        •	runSimple():
            •	Reads focus_*.tif files from focusFolder.
            •	For each pixel, tracks the maximum focus value seen so far and the
                corresponding slice index.
            •	Applies a small median blur to reduce speckle in the index map.
        •	runMultiScale():
            •	Accepts in-memory grayscale slices (or loads them from disk via
                runMultiScaleFromDisk()).
            •	Pads slices to wavelet-friendly dimensions (divisible by 2^levels).
            •	Runs FSFusionWavelet::forward on each padded slice.
            •	Uses FSMerge::merge to produce:
            •	mergedWavelet (not persisted here)
            •	depthIndexPadded16 (CV_16U)
            •	Crops the padded depth index back to original image size.
    3.	Generate a readable preview (optional)
        •	makeDepthPreviewEnhanced():
        •	Produces a stacked visualization:
            (a) grayscale depth map (0..255 view)
            (b) JET heatmap version
            (c) legend bar with slice tick labels
        •	Intended for quick diagnosis of depth continuity and slice ordering.
    4.	Handle I/O and validation
        •	Verifies folder existence and creates output folders as needed.
        •	Validates slice counts, image loads, and size consistency.
        •	Supports abort via atomic_bool and reports progress/status via callbacks.
        •	Writes PNGs through FSUtilities::writePngWithTitle so debug artifacts can
            carry metadata (Title/Author) for later attribution.

PUBLIC ENTRY POINTS (FSDepth namespace)
    •	run(focusFolder, depthFolder, opt, …)
        Dispatches to:
            * MultiScale if opt.method == “MultiScale”
            * Simple otherwise
                •	runFromGraySlices(graySlices, depthFolder, opt, …)
        Skips disk enumeration and runs the MultiScale path directly from memory.

HOW TO EVALUATE RESULTS
    •	depth_index.png:
            The canonical “truth” output. View it as 16-bit grayscale to confirm the
            index range and check for speckle/banding; it should be smooth within
            continuous surfaces and change mostly at real depth boundaries.
    •	depth_preview.png:
            Best quick diagnostic.  Look for:
            * smooth gradients where depth changes gradually
            * sharp boundaries where subject edges exist
            * legend labels matching expected slice order
*/

namespace {

//----------------------------------------------------------
// Shared: build enhanced preview (grayscale + heatmap + legend)
//----------------------------------------------------------

cv::Mat makeDepthPreviewEnhanced(const cv::Mat &depthIndex16,
                                 int sliceCount,
                                 std::string &title)
{
    CV_Assert(depthIndex16.type() == CV_16U);
    CV_Assert(sliceCount > 0);

    const int rows = depthIndex16.rows;
    const int cols = depthIndex16.cols;

    // --- Base grayscale preview (0..255) ---
    cv::Mat gray8;
    cv::normalize(depthIndex16, gray8, 0, 255, cv::NORM_MINMAX);
    gray8.convertTo(gray8, CV_8U);

    // --- Color heatmap preview (same depth, colored) ---
    cv::Mat heatColor;
    cv::applyColorMap(gray8, heatColor, cv::COLORMAP_JET);

    // ----------------------------------------------------------
    // Legend builders
    // ----------------------------------------------------------
    const int legendHeight = 40;

    auto colorForSlice = [&](int s, bool colorLegend) -> cv::Scalar {
        int g = 0;
        if (sliceCount > 1)
            g = static_cast<int>(255.0 * s / (sliceCount - 1));
        g = std::clamp(g, 0, 255);

        if (!colorLegend) {
            return cv::Scalar(g, g, g); // grayscale BGR
        }

        // Jet legend: map this gray through the same colormap
        cv::Mat one(1, 1, CV_8U, cv::Scalar(g));
        cv::Mat oneColor(1, 1, CV_8UC3);
        cv::applyColorMap(one, oneColor, cv::COLORMAP_JET);
        const cv::Vec3b bgr = oneColor.at<cv::Vec3b>(0, 0);
        return cv::Scalar(bgr[0], bgr[1], bgr[2]);
    };

    auto makeLegendBar = [&](bool colorLegend) -> cv::Mat {
        cv::Mat legend(legendHeight, cols, CV_8UC3, cv::Scalar(255, 255, 255));

        for (int s = 0; s < sliceCount; ++s)
        {
            float t0 = static_cast<float>(s)     / static_cast<float>(sliceCount);
            float t1 = static_cast<float>(s + 1) / static_cast<float>(sliceCount);

            int x0 = static_cast<int>(t0 * cols);
            int x1 = static_cast<int>(t1 * cols);
            if (x1 <= x0) x1 = x0 + 1;

            const cv::Scalar col = colorForSlice(s, colorLegend);

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
                const int g = (sliceCount > 1)
                ? static_cast<int>(255.0 * s / (sliceCount - 1))
                : 0;

                // Choose contrasting label color
                const int textGray = (g < 128 ? 255 : 0);
                const cv::Scalar textCol(textGray, textGray, textGray);

                cv::putText(legend,
                            std::to_string(s),
                            cv::Point(x0 + 2, legendHeight - 10),
                            cv::FONT_HERSHEY_SIMPLEX,
                            0.6,
                            textCol,
                            1,
                            cv::LINE_AA);
            }
        }

        return legend;
    };

    // Grayscale panel (converted to 3-channel for stacking)
    cv::Mat grayColor;
    cv::cvtColor(gray8, grayColor, cv::COLOR_GRAY2BGR);

    // Two legends: one under grayscale, one under color heatmap
    cv::Mat legendGray  = makeLegendBar(/*colorLegend=*/false);
    cv::Mat legendColor = makeLegendBar(/*colorLegend=*/true);

    // ----------------------------------------------------------
    // Stack layout:
    //   [title band]
    //   [gray image]
    //   [gray legend]
    //   [color heatmap]
    //   [color legend]
    // ----------------------------------------------------------
    const int titleBandH = 100; // small band to keep title off the image
    cv::Mat out(titleBandH + rows + legendHeight + rows + legendHeight,
                cols,
                CV_8UC3,
                cv::Scalar(255, 255, 255));

    int y = 0;

    // Title band (top-left). "font size 5" interpreted as a small label.
    // If you literally want huge text, increase titleScale (e.g. 5.0).
    const double titleScale = 3.0;
    const int    titleThick = 8;

    cv::putText(out,
                title,
                cv::Point(6, titleBandH - 25),
                cv::FONT_HERSHEY_SIMPLEX,
                titleScale,
                cv::Scalar(0, 0, 0),
                titleThick,
                cv::LINE_AA);

    y += titleBandH;

    grayColor.copyTo(out(cv::Rect(0, y, cols, rows)));
    y += rows;

    legendGray.copyTo(out(cv::Rect(0, y, cols, legendHeight)));
    y += legendHeight;

    heatColor.copyTo(out(cv::Rect(0, y, cols, rows)));
    y += rows;

    legendColor.copyTo(out(cv::Rect(0, y, cols, legendHeight)));

    return out;
}

/*
// makeDepthPreviewEnhanced duplicated in FSUtilities
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
//*/

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

        if (progressCb) progressCb();
    }

    cv::medianBlur(depthIndex, depthIndex, 3);

    const QString depthIdxPath = outDir.absoluteFilePath("depth_index.png");
    if (!FSUtilities::writePngWithTitle(depthIdxPath, depthIndex)) {
        if (statusCb) statusCb("FSDepth(Simple): Failed to write depth_index.png");
        return false;
    }

    if (opt.preview) {
        std::string title = "Simple";
        cv::Mat preview = makeDepthPreviewEnhanced(depthIndex, slices, title);
        const QString depthPreviewPath = outDir.absoluteFilePath("depth_preview.png");
        if (!FSUtilities::writePngWithTitle(depthPreviewPath, preview)) {
            if (statusCb) statusCb("FSDepth(Simple): Failed to write depth_preview.png");
            return false;
        }
    }
    if (statusCb) statusCb("FSDepth(Simple): Depth map computation complete.");
    return true;
}

bool runTenengrad(
    const std::vector<cv::Mat> &graySlices,      // CV_8U or CV_32F
    const QString              &depthFolder,
    const FSDepth::Options     &opt,
    std::atomic_bool           *abortFlag,
    FSDepth::ProgressCallback   progressCb,
    FSDepth::StatusCallback     statusCb,
    cv::Mat                    *depthIndex16Out)
{
    const QString srcFun = "FSDepth::runTenengrad";
    if (G::FSLog) G::log(srcFun);

    if (graySlices.empty())
    {
        if (statusCb) statusCb("FSDepth(Tenengrad): No gray slices provided.");
        return false;
    }

    const int slices = int(graySlices.size());
    const cv::Size sz = graySlices.front().size();

    // check validity
    for (int i = 0; i < slices; ++i)
    {
        if (graySlices[i].empty() || graySlices[i].size() != sz)
        {
            if (statusCb) statusCb("FSDepth(Tenengrad): Slice size mismatch.");
            return false;
        }
    }

    // Parameters (start simple; tune later)
    const float r = 2.0f;       // blur radius
    const float t = 6000.0f;    // threshold ENERGY (before sqrt). 0 disables.

    std::vector<cv::Mat> focus(slices);

    if (statusCb) statusCb(QString("FSDepth(Tenengrad): computing %1 focus maps...")
                     .arg(slices));

    for (int i = 0; i < slices; ++i)
    {
        if (abortFlag && abortFlag->load(std::memory_order_relaxed)) return false;

        QString msg = "Tenengrad focus for slice " + QString::number(i+1);
        if (G::FSLog) G::log(srcFun, msg);

        // Ensure grayscale is single-channel (caller says graySlices, but be defensive)
        if (!FSFocus::runTenengrad(graySlices[i], focus[i], r, t))
            return false;

        if (progressCb) progressCb();
    }

    // Build depthIndex16 = argmax focus across slices.
    cv::Mat bestVal32(sz, CV_32F, cv::Scalar(-1.0f));
    cv::Mat depthIndex16(sz, CV_16U, cv::Scalar(0));

    for (int i = 0; i < slices; ++i)
    {
        if (abortFlag && abortFlag->load(std::memory_order_relaxed)) return false;

        const cv::Mat &fm = focus[i];
        CV_Assert(fm.type() == CV_32F && fm.size() == sz);

        QString msg = "Tenengrad max for slice " + QString::number(i+1);
        if (G::FSLog) G::log(srcFun, msg);

        for (int y = 0; y < sz.height; ++y)
        {
            const float *fRow = fm.ptr<float>(y);
            float       *bRow = bestVal32.ptr<float>(y);
            uint16_t    *dRow = depthIndex16.ptr<uint16_t>(y);

            for (int x = 0; x < sz.width; ++x)
            {
                float v = fRow[x];
                if (v > bRow[x])
                {
                    bRow[x] = v;
                    dRow[x] = static_cast<uint16_t>(i);
                }
            }
        }
    }

    // Optional mild cleanup (keeps “source slice per pixel” intent)
    if (opt.preview) {
        cv::medianBlur(depthIndex16, depthIndex16, 3);
    }

    if (depthIndex16Out) {
        depthIndex16.copyTo(*depthIndex16Out);
    }

    // Write only what you asked for: full-size heatmap preview (no pyramid stuff)
    QDir outDir(depthFolder);
    if (!outDir.exists()) outDir.mkpath(".");

    if (opt.preview)
    {
        QString fName = "depth_heatmap_tenengrad.png";
        const QString heatPath = outDir.absoluteFilePath(fName);
        if (!FSUtilities::heatMapPerSlice(heatPath,
                                          depthIndex16,
                                          slices,
                                          cv::COLORMAP_JET))
            return false;
    }

    if (statusCb) statusCb("FSDepth(Tenengrad): complete.");
    return true;
}

bool runTenengradSlice(const cv::Mat &graySlice,
                                cv::Mat &focus32,
                                float radius,
                                float threshold)
{
    QString srcFun = "runTenengradSlice";
    QString msg = " radius = " + QString::number(radius) +
                  " threshold = " + QString::number(threshold);
    if (G::FSLog) G::log(srcFun, msg);

    if (graySlice.empty()) {
        QString msg = "grayslice is empty.";
        qWarning() << "WARNING:" << srcFun << msg;
        return false;
    }

    cv::Mat gray;
    if (graySlice.channels() == 1)
    {
        gray = graySlice;
    }
    else
    {
        cv::cvtColor(graySlice, gray, cv::COLOR_BGR2GRAY);
    }

    if (gray.type() == CV_32F)
    {
        // ok
    }
    else if (gray.type() == CV_8U)
    {
        // ok
    }
    else
    {
        // normalize odd types to 8U
        cv::Mat tmp;
        gray.convertTo(tmp, CV_8U);
        gray = tmp;
    }

    const int rows = gray.rows;
    const int cols = gray.cols;

    cv::Mat sobel(rows, cols, CV_32F);
    cv::Mat magnitude(rows, cols, CV_32F, cv::Scalar(0));

    // Sobel expects single-channel; gray is guaranteed 1ch now.
    cv::Sobel(gray, sobel, CV_32F, 1, 0);
    cv::accumulateSquare(sobel, magnitude);

    cv::Sobel(gray, sobel, CV_32F, 0, 1);
    cv::accumulateSquare(sobel, magnitude);

    if (threshold > 0.0f)
        magnitude.setTo(0, magnitude < threshold);

    if (radius > 0.0f)
    {
        int blurwindow = int(radius * 4.0f) + 1;
        if ((blurwindow & 1) == 0) blurwindow += 1; // force odd
        cv::GaussianBlur(magnitude, magnitude,
                         cv::Size(blurwindow, blurwindow),
                         radius, radius, cv::BORDER_REFLECT);
    }

    cv::sqrt(magnitude, focus32); // CV_32F

    if (focus32.type() != CV_32F) {
        QString msg = "focus32.type() != CV_32F.";
        qWarning() << "WARNING:" << srcFun << msg;
    }

    return (focus32.type() == CV_32F);
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

        if (progressCb) progressCb();
            // progressCb(s + 1);

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
        std::string title = "MultiScale Wavelet Depth Map";
        cv::Mat preview = makeDepthPreviewEnhanced(depthIndex16, total, title);
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
    qDebug() << "FSDepth::run statusCb valid =" << static_cast<bool>(statusCb);

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

    if (opt.method == "MultiScale")
    return runMultiScale(
        graySlices,
        depthFolder,
        opt,
        abortFlag,
        progressCb,
        statusCb,
        depthIndex16Out
        );

    if (opt.method == "Tenengrad")
        return runTenengrad(
            graySlices,
            depthFolder,
            opt,
            abortFlag,
            progressCb,
            statusCb,
            depthIndex16Out
            );

    return false;
}

bool FSDepth::streamGraySlice(const cv::Mat        &graySlice,
                              int                  sliceIndex,
                              const QString        &depthFolder,
                              const Options        &opt,
                              StreamState          &state,
                              std::atomic_bool     *abortFlag,
                              ProgressCallback      progressCb,
                              StatusCallback        statusCb)
{
    const QString srcFun = "FSDepth::streamGraySlice";
    QString s = QString::number(sliceIndex);
    QString msg = " " + s;
    if (G::FSLog) G::log(srcFun, msg);

    if (abortFlag && abortFlag->load(std::memory_order_relaxed))
        return false;

    if (graySlice.empty())
    {
        QString msg = "FSDepth(TenengradStream): graySlice empty.";
        if (statusCb) statusCb(msg);
        qWarning() << "WARNING:" << srcFun << msg;
        return false;
    }

    const cv::Size sz = graySlice.size();

    if (!state.initialized)
    {
        state.initialized = true;
        state.size = sz;
        state.sliceIndex = 0;

        state.bestVal32.create(sz, CV_32F);
        state.bestVal32.setTo(-1.0f);

        state.depthIndex16.create(sz, CV_16U);
        state.depthIndex16.setTo(uint16_t(0));
    }
    else
    {
        if (sz != state.size)
        {
            if (statusCb) statusCb("FSDepth(TenengradStream): size mismatch.");
            QString msg = "Size mismatch.";
            qWarning() << "WARNING:" << srcFun << msg;
            return false;
        }
    }

    if (sliceIndex != state.sliceIndex)
    {
        // Keeps you honest in streaming callers
        if (statusCb) statusCb("FSDepth(TenengradStream): sliceIndex out of order.");
        QString msg = "sliceindex out of order.";
        qWarning() << "WARNING:" << srcFun << msg;
        return false;
    }

    cv::Mat focus32;
    if (!runTenengradSlice(graySlice, focus32,
                           state.ten.radius, state.ten.threshold))
    {
        if (statusCb) statusCb("FSDepth(TenengradStream): runTenengradSlice failed.");
        return false;
    }

    CV_Assert(focus32.type() == CV_32F);
    CV_Assert(focus32.size() == state.size);

    // Argmax update: if focus32 > bestVal32, set bestVal32 and depthIndex16.
    for (int y = 0; y < state.size.height; ++y)
    {
        const float *fRow = focus32.ptr<float>(y);
        float       *bRow = state.bestVal32.ptr<float>(y);
        uint16_t    *dRow = state.depthIndex16.ptr<uint16_t>(y);

        for (int x = 0; x < state.size.width; ++x)
        {
            const float v = fRow[x];
            if (v > bRow[x])
            {
                bRow[x] = v;
                dRow[x] = static_cast<uint16_t>(sliceIndex);
            }
        }
    }

    ++state.sliceIndex;

    if (progressCb) progressCb();

    return true;
}

bool FSDepth::finishStreaming(const QString    &depthFolder,
                              const Options    &opt,
                              int              sliceCount,
                              StreamState      &state,
                              StatusCallback    statusCb,
                              cv::Mat          *depthIndex16Out)
{
    QString srcFun = "FSDepth::finishStreaming";
    if (G::FSLog) G::log(srcFun);

    if (!state.initialized || state.depthIndex16.empty())
        return false;

    if (sliceCount <= 0 || state.sliceIndex != sliceCount)
    {
        if (statusCb) statusCb("FSDepth(TenengradStream): sliceCount mismatch.");
        return false;
    }

    QDir outDir(depthFolder);
    if (!outDir.exists())
        outDir.mkpath(".");

    // Optional cleanup (keep light; you said you want source slice per pixel).
    // If you want NONE, remove this block.
    cv::Mat depthOut = state.depthIndex16;
    // cv::medianBlur(depthOut, depthOut, 3);

    const QString idxPath = outDir.absoluteFilePath("depth_index.png");
    if (!FSUtilities::writePngWithTitle(idxPath, depthOut)) {
        QString msg = "Failed to write depth_index.png";
        qWarning() << "WARNING:" << srcFun << msg;
    }

    if (opt.preview)
    {
        // If you want ONLY a full-size heatmap (no legend), swap to your
        // heatmap-only helper instead of makeDepthPreviewEnhanced.
        QString r = QString::number(state.ten.radius);
        QString t = QString::number(state.ten.threshold);
        QString f = "depth_preview_" + r + "_" + t + ".png";
        const QString previewPath = outDir.absoluteFilePath(f);
        QString msg = "Write preview " + previewPath;
        if (G::FSLog) G::log(srcFun, msg);
        QString s = "Tenengrad Depth Map  radius: " + r +"  threshold: " + t;
        std::string title = s.toStdString();
        cv::Mat preview = makeDepthPreviewEnhanced(depthOut, sliceCount, title);
        if (!FSUtilities::writePngWithTitle(previewPath, preview))
            return false;
    }

    if (depthIndex16Out)
        depthOut.copyTo(*depthIndex16Out);

    if (statusCb) statusCb("FSDepth(TenengradStream): complete.");
    return true;
}

} // namespace FSDepth
