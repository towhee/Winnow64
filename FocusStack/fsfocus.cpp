#include "FSFocus.h"
#include "Main/global.h"

#include <QDir>
#include <QFileInfoList>
#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/imgcodecs.hpp>

#include "FSFusionWavelet.h"

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
         StatusCallback statusCb)
{
    QString srcFun = "FSFocus::run";

    QDir inDir(alignFolder);
    QDir outDir(focusFolder);

    if (!inDir.exists()) {
        if (statusCb) statusCb("FSFocus: Align folder missing: " + alignFolder, true);
        return false;
    }
    if (!outDir.exists() && !outDir.mkpath(".")) {
        if (statusCb) statusCb("FSFocus: Cannot create focus folder: " + focusFolder, true);
        return false;
    }

    QFileInfoList files = inDir.entryInfoList(
        {"gray_*.tif","gray_*.tiff","gray_*.png","gray_*.jpg","gray_*.jpeg"},
        QDir::Files, QDir::Name);

    int total = files.size();
    if (total == 0) {
        if (statusCb) statusCb("FSFocus: No grayscale aligned images found.", true);
        return false;
    }

    if (statusCb)
        statusCb(QString("FSFocus: %1 slices detected.").arg(total), false);

    //--------------------------------------------------------
    // Process each slice
    //--------------------------------------------------------
    for (int i = 0; i < total; i++)
    {
        if (abortFlag && abortFlag->load()) {
            if (statusCb) statusCb("FSFocus: Aborted.", true);
            return false;
        }

        G::log(srcFun, "Slice " + QString::number(i));

        QFileInfo fi = files[i];
        QString base = fi.completeBaseName();
        QString srcPath = fi.absoluteFilePath();

        cv::Mat gray = cv::imread(srcPath.toStdString(), cv::IMREAD_GRAYSCALE);
        if (gray.empty()) {
            if (statusCb) statusCb("FSFocus: Cannot load " + srcPath, true);
            return false;
        }

        //--------------------------------------------------------
        // Wavelet transform
        //--------------------------------------------------------
        cv::Mat wavelet;
        if (!FSFusionWavelet::forward(gray, opt.useOpenCL, wavelet)) {
            if (statusCb) statusCb("FSFocus: Wavelet failed for " + base, true);
            return false;
        }

        std::vector<cv::Mat> ch(2);
        cv::split(wavelet, ch);

        cv::Mat mag32;
        cv::magnitude(ch[0], ch[1], mag32);

        //--------------------------------------------------------
        // Build numeric 16U focus metric
        //--------------------------------------------------------
        double mn, mx;
        cv::minMaxLoc(mag32, &mn, &mx);

        cv::Mat focus16;
        double scale = (mx > mn) ? (65535.0 / (mx - mn)) : 0.0;
        double shift = -mn * scale;
        mag32.convertTo(focus16, CV_16U, scale, shift);

        cv::imwrite(outDir.absoluteFilePath("focus_" + base + ".tif").toStdString(),
                    focus16);

        // Preview
        if (opt.preview) {
            QString p;
            p = outDir.absoluteFilePath("focus_loggray_" + base + ".png");
            cv::imwrite(p.toStdString(), makeLogGrayPreview(mag32));
            // p = outDir.absoluteFilePath("focus_heatmap_" + base + ".png");
            // cv::imwrite(p.toStdString(), makeHeatPreview(mag32));
            // p = outDir.absoluteFilePath("focus_colorslice_" + base + ".png");
            // cv::imwrite(p.toStdString(), makeSliceColorPreview(mag32, i, total));
        }

        if (progressCb)
            progressCb(i + 1);
    }

    if (statusCb) statusCb("FSFocus: Completed.", false);
    return true;
}

} // namespace FSFocus

