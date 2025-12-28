#include "FSFocus.h"
#include "fsutilities.h"
#include "Main/global.h"

#include <QDir>
#include <QFileInfoList>
#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/imgcodecs.hpp>

#include "FSFusionWavelet.h"

/*
    FSFocus module overview
    -----------------------

    Purpose:
    - Compute per-slice focus metrics ("focus maps") from aligned grayscale images.
    - Optionally write per-slice numeric focus maps plus human-viewable previews
      (log-gray, heatmap, and slice-index pseudo-color) for diagnostics.

    Inputs:
    - alignFolder: folder containing aligned grayscale images named gray_*.tif/png/jpg.
    - focusFolder: output folder for focus_*.tif and preview PNGs.
    - Options:
      * useOpenCL: passed into FSFusionWavelet::forward().
      * keepIntermediates: writes numeric focus_*.tif (16-bit).
      * preview: writes PNG previews (loggray/heatmap/colorslice) per slice.
      * downsampleFactor / numThreads: currently reserved (not used in code shown).
    - abortFlag: allows early exit between slices.
    - progressCb/statusCb: UI reporting.
    - focusMapsOut (optional): returns CV_32F focus metric for each slice in memory.

    Core algorithm:
    1) Enumerate input slices in alignFolder using the "gray_*" naming convention.
    2) For each slice:
       - Load grayscale (IMREAD_GRAYSCALE).
       - Run a wavelet forward transform:
            FSFusionWavelet::forward(gray, useOpenCL, wavelet)
            Output is expected to be complex CV_32FC2.
       - Convert complex wavelet coefficients to a scalar focus metric:
            mag32 = magnitude(real, imag)  // CV_32F
            This mag32 is the per-pixel "focus energy" for the slice.
       - Optionally store mag32 into focusMapsOut for later stages (e.g., FSDepth).

    Disk outputs (when keepIntermediates is true):
    - focus_<base>.tif:
      * 16-bit scalar image derived from mag32 by per-slice min/max normalization
        into 0..65535. (This is a per-slice normalization for storage/viewing,
        not a cross-slice normalization.)

    Diagnostic previews (when preview is true):
    - focus_loggray_<base>.png:
      * log(1 + mag32) then min/max normalize to 8-bit for visibility.
    - focus_heatmap_<base>.png:
      * high-frequency emphasis (mag32 - GaussianBlur), clamp negatives, log,
        normalize to 8-bit, then apply COLORMAP_JET.
    - focus_colorslice_<base>.png:
      * log-gray preview modulated by a per-slice color tint (slice index cue).

    Debug metadata:
    - Preview PNGs are written through FSUtilities::writePngWithTitle(path, img),
      which embeds PNG text metadata (Title/Author) so uploaded debug PNGs can
      be traced back to their file name (Title uses the output file name).

    Failure modes / assumptions:
    - Expects wavelet output to be CV_32FC2; returns false if not.
    - Expects at least one gray_* input file; returns false if none.
    - Per-slice 16-bit focus_*.tif uses slice-local scaling, so direct numerical
      comparison across slices should use mag32 (focusMapsOut) rather than the
      stored 16-bit maps unless you re-normalize consistently.
*/

namespace {

//--------------------------------------------------------
// Utility: Log-compressed grayscale preview (good default)
//--------------------------------------------------------
cv::Mat makeLogGrayPreview(const cv::Mat &metric32)
{
    cv::Mat m;
    metric32.convertTo(m, CV_32F);
    m += 1.0f;
    cv::log(m, m);

    double vmin, vmax;
    cv::minMaxLoc(m, &vmin, &vmax);

    cv::Mat out8;
    m.convertTo(out8, CV_8U, 255.0 / (vmax - vmin), -vmin * 255.0 / (vmax - vmin));
    return out8;
}

//--------------------------------------------------------
// Utility: Heatmap preview (warmer = sharper)
//--------------------------------------------------------
cv::Mat makeHeatPreview(const cv::Mat &metric32)
{
    // Always work in 32F
    cv::Mat m32;
    metric32.convertTo(m32, CV_32F);

    // 1. Remove low-frequency gradients (background, vignetting, etc.)
    cv::Mat blur32;
    cv::GaussianBlur(m32, blur32, cv::Size(0, 0), 3.0);

    cv::Mat hf32 = m32 - blur32;

    // 2. Clamp negatives -> 0 (we only want "extra" local energy)
    cv::threshold(hf32, hf32, 0.0, 0.0, cv::THRESH_TOZERO);

    // 3. Log compression to make small differences visible
    hf32 += 1.0f;          // avoid log(0)
    cv::log(hf32, hf32);

    // 4. Robust normalization to 0..255
    double vmin = 0.0, vmax = 0.0;
    cv::minMaxLoc(hf32, &vmin, &vmax);

    cv::Mat norm8;
    if (vmax <= vmin) {
        // degenerate case – return a mid-blue image so we *see* that something is off
        norm8 = cv::Mat(hf32.rows, hf32.cols, CV_8U, cv::Scalar(64));
    }
    else {
        hf32.convertTo(norm8, CV_8U,
                       255.0 / (vmax - vmin),
                       -vmin * 255.0 / (vmax - vmin));
    }

    // 5. Colorize: blue = low HF energy, red/yellow = strong HF (sharp)
    cv::Mat colorMap;
    cv::applyColorMap(norm8, colorMap, cv::COLORMAP_JET);
    return colorMap;
}

//--------------------------------------------------------
// Utility: Pseudo-color using slice index (good for depth)
//--------------------------------------------------------
cv::Mat makeSliceColorPreview(const cv::Mat &metric32, int sliceIdx, int sliceTotal)
{
    float t = float(sliceIdx) / float(std::max(1, sliceTotal - 1));
    cv::Vec3b color(
        uchar(255 * t),
        uchar(255 * (1.0f - t)),
        uchar(128 + 127 * std::sin(t * 3.14159f))
        );

    cv::Mat gray = makeLogGrayPreview(metric32);
    cv::Mat out(gray.size(), CV_8UC3);

    for (int y = 0; y < gray.rows; y++)
    {
        const uchar *gRow = gray.ptr<uchar>(y);
        cv::Vec3b *oRow = out.ptr<cv::Vec3b>(y);

        for (int x = 0; x < gray.cols; x++)
            oRow[x] = cv::Vec3b(
                uchar(color[0] * (gRow[x] / 255.0f)),
                uchar(color[1] * (gRow[x] / 255.0f)),
                uchar(color[2] * (gRow[x] / 255.0f))
                );
    }
    return out;
}

} // anonymous namespace

//--------------------------------------------------------------------
// MAIN FSFocus IMPLEMENTATION
//--------------------------------------------------------------------
namespace FSFocus {

bool run(const QString &alignFolder,
         const QString &focusFolder,
         const Options &opt,
         std::atomic_bool *abortFlag,
         ProgressCallback progressCb,
         StatusCallback statusCb,
         std::vector<cv::Mat> *focusMapsOut)
{
    QString srcFun = "FSFocus::run";

    // ------------------------------------------------------------
    // Validate input / output folders
    // ------------------------------------------------------------
    QDir inDir(alignFolder);
    if (!inDir.exists()) {
        if (statusCb) statusCb("FSFocus: Align folder missing: " + alignFolder);
        return false;
    }

    QDir outDir(focusFolder);
    if (!outDir.exists() && !outDir.mkpath(".")) {
        if (statusCb) statusCb("FSFocus: Cannot create focus folder: " + focusFolder);
        return false;
    }

    // Enumerate aligned grayscale images
    QFileInfoList files = inDir.entryInfoList(
        {"gray_*.tif", "gray_*.tiff",
         "gray_*.png", "gray_*.jpg", "gray_*.jpeg"},
        QDir::Files,
        QDir::Name);

    const int total = files.size();
    if (total == 0) {
        if (statusCb) statusCb("FSFocus: No grayscale aligned images found.");
        return false;
    }

    if (statusCb) {
        statusCb(QString("FSFocus: %1 slices detected.").arg(total));
    }

    // For future use (downsampling / threading), but not used yet:
    const int numThreads = (opt.numThreads > 0
                                ? opt.numThreads
                                : std::max(1, QThread::idealThreadCount()));
    Q_UNUSED(numThreads);
    Q_UNUSED(opt.downsampleFactor);

    // If caller wants in-memory focus maps, size the vector up front
    if (focusMapsOut) {
        focusMapsOut->clear();
        focusMapsOut->resize(total);
    }

    // ------------------------------------------------------------
    // Process each slice
    // ------------------------------------------------------------
    for (int i = 0; i < total; ++i)
    {
        if (abortFlag && abortFlag->load(std::memory_order_relaxed)) {
            if (statusCb) statusCb("FSFocus: Aborted.");
            return false;
        }

        const QFileInfo fi = files[i];
        const QString base    = fi.completeBaseName();
        const QString srcPath = fi.absoluteFilePath();

        // --------------------------------------------------------
        // 1. Load grayscale aligned image
        // --------------------------------------------------------
        QString msg = "Loading grayscale slice " + QString::number(i);
        if (G::FSLog) G::log(srcFun, msg);
        if (statusCb) statusCb(srcFun + ": " + msg);

        cv::Mat gray = cv::imread(srcPath.toStdString(), cv::IMREAD_GRAYSCALE);
        if (gray.empty()) {
            if (statusCb) statusCb("FSFocus: Cannot load " + srcPath);
            return false;
        }

        // (Optional future: apply downsampleFactor here)

        // --------------------------------------------------------
        // 2. Wavelet transform → complex CV_32FC2
        // --------------------------------------------------------
        cv::Mat wavelet;
        if (!FSFusionWavelet::forward(gray, opt.useOpenCL, wavelet)) {
            if (statusCb) statusCb("FSFocus: Wavelet failed for " + base);
            return false;
        }

        if (wavelet.type() != CV_32FC2) {
            if (statusCb) statusCb("FSFocus: Wavelet output type mismatch (expected CV_32FC2).");
            return false;
        }

        // --------------------------------------------------------
        // 3. Focus metric: magnitude of complex wavelet coefficients
        //    mag32 = sqrt(real^2 + imag^2)  (CV_32F)
        // --------------------------------------------------------
        msg = "Magnitude of complex wavelet coefficients slice " + QString::number(i);
        if (G::FSLog) G::log(srcFun, msg);
        if (statusCb) statusCb(srcFun + ": " + msg);

        std::vector<cv::Mat> ch(2);
        cv::split(wavelet, ch);

        cv::Mat mag32;
        cv::magnitude(ch[0], ch[1], mag32);   // CV_32F

        double mn = 0.0, mx = 0.0;
        cv::minMaxLoc(mag32, &mn, &mx);
        cv::Scalar meanVal = cv::mean(mag32);

        if (G::FSLog) G::log(srcFun,
               QString("Slice %1 mag32 min=%2 max=%3 mean=%4")
                   .arg(i)
                   .arg(mn)
                   .arg(mx)
                   .arg(meanVal[0]));

        // Keep this metric in memory if requested (for FSDepth or other uses)
        if (focusMapsOut) {
            // Ensure correct size (defensive if caller modified it mid-run)
            if (static_cast<int>(focusMapsOut->size()) != total)
                focusMapsOut->resize(total);
            (*focusMapsOut)[i] = mag32.clone();   // CV_32F per slice
        }

        // --------------------------------------------------------
        // 4. Optional: write focus_*.tif + previews if keepIntermediates
        // --------------------------------------------------------
        if (opt.keepIntermediates)
        {
            msg = "Write previews for slice " + QString::number(i);
            if (G::FSLog) G::log(srcFun, msg);
            if (statusCb) statusCb(srcFun + ": " + msg);

            // Numeric 16U metric (normalized per slice to 0..65535)
            cv::Mat focus16;
            if (mx > mn) {
                double scale = 65535.0 / (mx - mn);
                double shift = -mn * scale;
                mag32.convertTo(focus16, CV_16U, scale, shift);
            }
            else {
                focus16 = cv::Mat(mag32.rows, mag32.cols, CV_16U, cv::Scalar(0));
            }

            {
                double fMin = 0.0, fMax = 0.0;
                cv::minMaxLoc(focus16, &fMin, &fMax);
                if (G::FSLog) G::log(srcFun,
                       QString("Slice %1 focus16 min=%2 max=%3")
                           .arg(i)
                           .arg(fMin)
                           .arg(fMax));
            }

            const QString dstMetricPath =
                outDir.absoluteFilePath("focus_" + base + ".tif");

            if (!cv::imwrite(dstMetricPath.toStdString(), focus16)) {
                QString msg = srcFun + " Failed to write " + dstMetricPath;
                if (statusCb) statusCb(msg);
                qWarning() << "WARNING:" << srcFun << msg;
                return false;
            }

            // ----------------------------------------------------
            // Previews (only if preview flag is enabled)
            // ----------------------------------------------------
            if (opt.preview)
            {
                QString p;

                // Log-compressed grayscale preview
                p = outDir.absoluteFilePath("focus_loggray_" + base + ".png");
                if (!FSUtilities::writePngWithTitle(p, makeLogGrayPreview(mag32))) return false;

                // Heatmap preview
                p = outDir.absoluteFilePath("focus_heatmap_" + base + ".png");
                if (!FSUtilities::writePngWithTitle(p, makeHeatPreview(mag32))) return false;

                // Pseudo-color by slice index
                p = outDir.absoluteFilePath("focus_colorslice_" + base + ".png");
                if (!FSUtilities::writePngWithTitle(p, makeSliceColorPreview(mag32, i, total))) return false;
            }
        }

        // --------------------------------------------------------
        // 5. Progress callback
        // --------------------------------------------------------
        if (progressCb) progressCb(i + 1);
    }

    if (statusCb)
        statusCb("FSFocus: Completed.");

    return true;
}

} // namespace FSFocus

