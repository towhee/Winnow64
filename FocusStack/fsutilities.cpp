#include "FSUtilities.h"
#include "Main/global.h"
#include <QtCore/qdebug.h>
#include <QFileInfo>
#include <QImage>
#include <QImageWriter>
#include <opencv2/imgproc.hpp>
#include <opencv2/imgcodecs.hpp>

namespace FSUtilities {

cv::Mat canonicalizeGrayFloat01(const cv::Mat& in,
                                const cv::Size& canonicalSize,
                                int interp,
                                const char* tag)
{
    cv::Mat g32 = in;
    CV_Assert(!g32.empty());

    // ensure CV_32F
    if (g32.type() != CV_32F) {
        // assume input already 0..1 if float, else 0..255
        if (g32.depth() == CV_8U) g32.convertTo(g32, CV_32F, 1.0 / 255.0);
        else g32.convertTo(g32, CV_32F);
    }

    // resize if needed
    if (g32.size() != canonicalSize) {
        cv::Mat r;
        cv::resize(g32, r, canonicalSize, 0, 0, interp);
        g32 = r;
    }

    return g32;
}

    cv::Mat canonicalizeDepthIndex16(const cv::Mat& depthIndex16,
                                     const cv::Size& canonicalSize,
                                     const char* tag)
{
    CV_Assert(depthIndex16.type() == CV_16U);

    if (depthIndex16.size() == canonicalSize)
        return depthIndex16;

    cv::Mat r;
    cv::resize(depthIndex16, r, canonicalSize, 0, 0, cv::INTER_NEAREST); // categorical!
    return r;
}

std::vector<cv::Mat> canonicalizeFocusSlices(const std::vector<cv::Mat>& focusSlices32,
                                                    const cv::Size& canonicalSize,
                                                    std::atomic_bool* abortFlag)
{
    std::vector<cv::Mat> out;
    out.reserve(focusSlices32.size());

    const int N = (int)focusSlices32.size();
    for (int i = 0; i < N; ++i)
    {
        if (abortFlag && abortFlag->load(std::memory_order_relaxed)) return {};
        CV_Assert(!focusSlices32[i].empty());

        // focus maps are continuous → INTER_LINEAR
        out.push_back(canonicalizeGrayFloat01(focusSlices32[i], canonicalSize,
                                              cv::INTER_LINEAR, "focusSlice"));
    }
    return out;
}

std::vector<cv::Mat> canonicalizeAlignedColor(const std::vector<cv::Mat>& alignedColor,
                                              const cv::Size& canonicalSize,
                                              std::atomic_bool* abortFlag)
{
    std::vector<cv::Mat> out;
    out.reserve(alignedColor.size());

    const int N = (int)alignedColor.size();
    for (int i = 0; i < N; ++i)
    {
        if (abortFlag && abortFlag->load(std::memory_order_relaxed)) return {};
        CV_Assert(!alignedColor[i].empty());

        cv::Mat m = alignedColor[i];

        if (m.size() != canonicalSize) {
            cv::Mat r;
            cv::resize(m, r, canonicalSize, 0, 0, cv::INTER_LINEAR);
            m = r;
        }
        out.push_back(m);
    }
    return out;
}

//-------------------------------------------------------------
// Ensure image has 3 channels (convert GRAY → BGR)
//-------------------------------------------------------------
cv::Mat ensureColor(const cv::Mat &img)
{
    if (img.empty()) return cv::Mat();
    if (img.channels() == 1) {
        cv::Mat c;
        cv::cvtColor(img, c, cv::COLOR_GRAY2BGR);
        return c;
    }
    return img;
}

//-------------------------------------------------------------
// Label helper
//-------------------------------------------------------------
cv::Mat addLabel(const cv::Mat &img, const std::string &text)
{
    if (img.empty()) return img;
    cv::Mat out = img.clone();

    // ---- Text positioning (top-left margin) ----
    int x = 20;
    int y = 50;

    double fontScale = 1.9;
    int thickness = 2;
    int baseline = 0;

    // ---- Compute text size (optional future use) ----
    cv::Size textSize = cv::getTextSize(
        text,
        cv::FONT_HERSHEY_SIMPLEX,
        fontScale,
        thickness,
        &baseline
        );

    // ---- Draw black outline (stroke) around text ----
    // Offsets create an 8-direction stroke
    std::vector<cv::Point> offsets = {
        { -2, -2 }, {  0, -2 }, {  2, -2 },
        { -2,  0 },             {  2,  0 },
        { -2,  2 }, {  0,  2 }, {  2,  2 }
    };

    for (const auto &ofs : offsets)
    {
        cv::putText(out, text,
                    cv::Point(x + ofs.x, y + ofs.y),
                    cv::FONT_HERSHEY_SIMPLEX,
                    fontScale,
                    cv::Scalar(0, 0, 0),   // black border
                    thickness + 2,
                    cv::LINE_AA);
    }

    // ---- Draw main white text ----
    cv::putText(out, text,
                cv::Point(x, y),
                cv::FONT_HERSHEY_SIMPLEX,
                fontScale,
                cv::Scalar(255, 255, 255), // white interior
                thickness,
                cv::LINE_AA);

    return out;
}

//-------------------------------------------------------------
// Horizontal stack
//-------------------------------------------------------------
cv::Mat hstack(const cv::Mat &a, const cv::Mat &b)
{
    if (a.empty()) return ensureColor(b);
    if (b.empty()) return ensureColor(a);

    cv::Mat A = ensureColor(a);
    cv::Mat B = ensureColor(b);

    int height = std::max(A.rows, B.rows);

    cv::resize(A, A, cv::Size(A.cols * height / A.rows, height));
    cv::resize(B, B, cv::Size(B.cols * height / B.rows, height));

    cv::Mat out(height, A.cols + B.cols, CV_8UC3);
    A.copyTo(out(cv::Rect(0,     0, A.cols, height)));
    B.copyTo(out(cv::Rect(A.cols, 0, B.cols, height)));

    return out;
}

//-------------------------------------------------------------
// Vertical stack
//-------------------------------------------------------------
cv::Mat vstack(const cv::Mat &a, const cv::Mat &b)
{
    if (a.empty()) return ensureColor(b);
    if (b.empty()) return ensureColor(a);

    cv::Mat A = ensureColor(a);
    cv::Mat B = ensureColor(b);

    int width = std::max(A.cols, B.cols);

    cv::resize(A, A, cv::Size(width, A.rows * width / A.cols));
    cv::resize(B, B, cv::Size(width, B.rows * width / B.cols));

    cv::Mat out(A.rows + B.rows, width, CV_8UC3);
    A.copyTo(out(cv::Rect(0, 0, width, A.rows)));
    B.copyTo(out(cv::Rect(0, A.rows, width, B.rows)));

    return out;
}

//----------------------------------------------------------
// Shared: add a title to a Mat
//----------------------------------------------------------

static inline void drawTitleOverlay(cv::Mat& imgBGR8,
                                    const std::string& title,
                                    int x = 10, int y = 10)
{
    CV_Assert(imgBGR8.type() == CV_8UC3);
    if (title.empty()) return;

    const int fontFace = cv::FONT_HERSHEY_SIMPLEX;
    const double fontScale = 0.9;                 // tweak if you want
    const int thickness = 2;

    int baseline = 0;
    cv::Size ts = cv::getTextSize(title, fontFace, fontScale, thickness, &baseline);

    const int pad = 8;
    const int boxW = ts.width + 2 * pad;
    const int boxH = ts.height + baseline + 2 * pad;

    // clamp within image
    int x0 = std::max(0, std::min(x, imgBGR8.cols - boxW - 1));
    int y0 = std::max(0, std::min(y, imgBGR8.rows - boxH - 1));

    cv::Rect r(x0, y0, std::min(boxW, imgBGR8.cols - x0), std::min(boxH, imgBGR8.rows - y0));
    if (r.width <= 0 || r.height <= 0) return;

    // darken patch for readability
    cv::Mat roi = imgBGR8(r);
    cv::Mat tmp;
    cv::addWeighted(roi, 0.30, cv::Mat(roi.size(), roi.type(), cv::Scalar(0,0,0)), 0.70, 0.0, tmp);
    tmp.copyTo(roi);

    // outlined text
    const int tx = r.x + pad;
    const int ty = r.y + pad + ts.height;
    cv::putText(imgBGR8, title, cv::Point(tx, ty),
                fontFace, fontScale, cv::Scalar(0,0,0), thickness + 2, cv::LINE_AA);
    cv::putText(imgBGR8, title, cv::Point(tx, ty),
                fontFace, fontScale, cv::Scalar(255,255,255), thickness, cv::LINE_AA);
}

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
// Shared: build enhanced preview (heatmap + legend)
//----------------------------------------------------------

cv::Mat depthHeatmap(const cv::Mat& depthIndex16,
                     int sliceCount,
                     const std::string& title)
{
    CV_Assert(depthIndex16.type() == CV_16U);
    CV_Assert(sliceCount > 0);

    const int rows = depthIndex16.rows;
    const int cols = depthIndex16.cols;

    // --- Map slice index -> 0..255 for colormap ---
    cv::Mat idx8(rows, cols, CV_8U, cv::Scalar(0));
    for (int y = 0; y < rows; ++y)
    {
        const uint16_t* d = depthIndex16.ptr<uint16_t>(y);
        uint8_t* o        = idx8.ptr<uint8_t>(y);
        for (int x = 0; x < cols; ++x)
        {
            int s = (int)d[x];
            s = std::max(0, std::min(s, sliceCount - 1));
            if (sliceCount > 1)
                o[x] = (uint8_t)std::lround(255.0 * (double)s / (double)(sliceCount - 1));
            else
                o[x] = 0;
        }
    }

    // --- Heatmap (orig size) ---
    cv::Mat heat;
    cv::applyColorMap(idx8, heat, cv::COLORMAP_TURBO); // CV_8UC3

    // --- Top-left title overlay for A/B comparisons (no resize) ---
    drawTitleOverlay(heat, title);

    // --- Draw legend OVERLAY at bottom (does NOT change image size) ---
    const int legendH = std::max(24, std::min(140, rows / 6));
    const int y0 = rows - legendH;
    if (y0 < 0) return heat;

    // Slight darkening of bottom strip for readability
    {
        cv::Mat roi = heat(cv::Rect(0, y0, cols, legendH));
        cv::Mat tmp;
        cv::addWeighted(roi, 0.35, cv::Mat(roi.size(), roi.type(), cv::Scalar(0,0,0)), 0.65, 0.0, tmp);
        tmp.copyTo(roi);
    }

    // Title inside legend strip (kept)
    {
        const std::string legendTitle = "Depth index -> color (TURBO)";
        double fontScale = std::max(0.6, legendH / 40.0);
        int thick = std::max(1, (int)std::lround(fontScale * 2.0));

        cv::putText(heat, legendTitle, cv::Point(10, y0 + std::max(22, legendH / 4)),
                    cv::FONT_HERSHEY_SIMPLEX, fontScale,
                    cv::Scalar(0,0,0), thick + 2, cv::LINE_AA);
        cv::putText(heat, legendTitle, cv::Point(10, y0 + std::max(22, legendH / 4)),
                    cv::FONT_HERSHEY_SIMPLEX, fontScale,
                    cv::Scalar(255,255,255), thick, cv::LINE_AA);
    }

    const int barY0 = y0 + std::max(18, legendH / 3);
    const int barY1 = rows - 6;

    for (int s = 0; s < sliceCount; ++s)
    {
        float t0 = (float)s / (float)sliceCount;
        float t1 = (float)(s + 1) / (float)sliceCount;

        int x0 = (int)std::lround(t0 * cols);
        int x1 = (int)std::lround(t1 * cols);
        if (x1 <= x0) x1 = x0 + 1;
        x0 = std::max(0, std::min(cols - 1, x0));
        x1 = std::max(1, std::min(cols, x1));

        int v = 0;
        if (sliceCount > 1)
            v = (int)std::lround(255.0 * (double)s / (double)(sliceCount - 1));
        v = std::max(0, std::min(255, v));

        cv::Mat one(1, 1, CV_8U, cv::Scalar(v));
        cv::Mat oneCol;
        cv::applyColorMap(one, oneCol, cv::COLORMAP_TURBO);
        cv::Vec3b c = oneCol.at<cv::Vec3b>(0,0);

        cv::rectangle(heat,
                      cv::Rect(x0, barY0, x1 - x0, barY1 - barY0),
                      cv::Scalar(c[0], c[1], c[2]),
                      cv::FILLED);

        bool drawLabel = false;
        if (sliceCount <= 16) drawLabel = true;
        else if (sliceCount <= 32 && (s % 2 == 0)) drawLabel = true;
        else if (sliceCount > 32 && (s % 4 == 0)) drawLabel = true;

        if (drawLabel)
        {
            const std::string label = std::to_string(s);

            int lum = (int)(0.114 * c[0] + 0.587 * c[1] + 0.299 * c[2]);
            cv::Scalar textCol = (lum < 128) ? cv::Scalar(255,255,255) : cv::Scalar(0,0,0);

            double fontScale = std::max(0.5, legendH / 55.0);
            int thick = std::max(1, (int)std::lround(fontScale * 2.0));

            int tx = std::min(x0 + 2, cols - 20);
            int ty = std::min(rows - 10, barY1 - 4);

            cv::putText(heat, label, cv::Point(tx, ty),
                        cv::FONT_HERSHEY_SIMPLEX, fontScale,
                        cv::Scalar(0,0,0), thick + 2, cv::LINE_AA);
            cv::putText(heat, label, cv::Point(tx, ty),
                        cv::FONT_HERSHEY_SIMPLEX, fontScale,
                        textCol, thick, cv::LINE_AA);
        }
    }

    cv::rectangle(heat, cv::Rect(0, y0, cols, legendH), cv::Scalar(255,255,255), 1);
    return heat;
}

//-------------------------------------------------------------
// Main Debug Composite Builder
//-------------------------------------------------------------

bool makeDebugOverview(const cv::Mat &depthPreview,
                       const cv::Mat &fusedColor,
                       const cv::Mat &sliceA,
                       const cv::Mat &sliceB,
                       const QString &outputPath,
                       int maxWidth)
{
    if (depthPreview.empty() || fusedColor.empty())
        return false;

    // Label versions
    cv::Mat depthL = addLabel(ensureColor(depthPreview), "Depth Preview");
    cv::Mat fusedL = addLabel(ensureColor(fusedColor),   "Fused Result");
    cv::Mat sAL    = addLabel(ensureColor(sliceA),       "Slice A");
    cv::Mat sBL    = addLabel(ensureColor(sliceB),       "Slice B");

    // Top: depth + fused vertically
    cv::Mat top = vstack(depthL, fusedL);

    // Bottom: sliceA + sliceB horizontally
    cv::Mat bottom = hstack(sAL, sBL);

    // Final composite
    cv::Mat final = vstack(top, bottom);

    // Limit width if too large
    if (final.cols > maxWidth)
    {
        double scale = double(maxWidth) / final.cols;
        cv::resize(final, final, cv::Size(), scale, scale);
    }

    return cv::imwrite(outputPath.toStdString(), final);
}

cv::Mat showWithMask(const cv::Mat &baseGray,
                     const cv::Mat &maskBGR,
                     float alpha)
{
    CV_Assert(maskBGR.type() == CV_8UC3);
    CV_Assert(alpha >= 0.0f && alpha <= 1.0f);

    cv::Mat base8;
    if (baseGray.type() == CV_8U)
        base8 = baseGray;
    else
        baseGray.convertTo(base8, CV_8U, 255.0);

    cv::Mat baseBGR;
    cv::cvtColor(base8, baseBGR, cv::COLOR_GRAY2BGR);

    cv::Mat out;
    cv::addWeighted(baseBGR, 1.0f - alpha,
                    maskBGR, alpha,
                    0.0, out);

    return out;
}

bool showWithMask(const cv::Mat &baseGray,
                  const cv::Mat &maskBGR,
                  const QString &outputPath,
                  float alpha)
{
    cv::Mat out = showWithMask(baseGray, maskBGR, alpha);
    return cv::imwrite(outputPath.toStdString(), out);
}

/*



*/

QString cvSizeToText(cv::Size cvSize)
{
    return "(" +
           QString::number(cvSize.width) +
           ", " +
           QString::number(cvSize.height) +
           ")";
}

void assertSameSize(const cv::Mat& a,
                    const cv::Mat& b,
                    const QString& context)
{
    if (a.empty() || b.empty())
        qFatal("FSUtilities::assertSameSize (%s): empty Mat", context.toUtf8().constData());

    if (a.size() != b.size())
        qFatal("FSUtilities::assertSameSize (%s): size mismatch (%dx%d vs %dx%d)",
               context.toUtf8().constData(),
               a.cols, a.rows, b.cols, b.rows);
}

void assertSameSize(const cv::Mat& a,
                    const cv::Mat& b,
                    const cv::Mat& c,
                    const QString& context)
{
    assertSameSize(a, b, context);
    assertSameSize(a, c, context);
}

cv::Mat canonicalizeToSize(const cv::Mat& src,
                           const cv::Size& canonicalSize,
                           int interp,
                           const QString& context)
{
    CV_Assert(!src.empty());
    if (src.size() == canonicalSize) return src;

    qWarning() << context
               << ": resizing from"
               << src.cols << "x" << src.rows
               << "to canonical"
               << canonicalSize.width << "x" << canonicalSize.height;

    cv::Mat out;
    cv::resize(src, out, canonicalSize, 0, 0, interp);
    return out;
}

cv::Mat FSUtilities::alignToOrigSize(const cv::Mat& img, cv::Size origSize)
{
    CV_Assert(!img.empty());
    CV_Assert(origSize.width  > 0);
    CV_Assert(origSize.height > 0);
    CV_Assert(img.cols >= origSize.width);
    CV_Assert(img.rows >= origSize.height);

    // If already correct size, return a clone
    if (img.size() == origSize)
        return img.clone();

    const int padW = img.cols - origSize.width;
    const int padH = img.rows - origSize.height;

    // Padding must be symmetric (as produced by padForWavelet / FSLoader)
    CV_Assert(padW >= 0 && padH >= 0);

    const int left = padW / 2;
    const int top  = padH / 2;

    CV_Assert(left + origSize.width  <= img.cols);
    CV_Assert(top  + origSize.height <= img.rows);

    cv::Rect roi(left, top, origSize.width, origSize.height);
    return img(roi).clone();
}

static cv::Mat to8uViewable(const cv::Mat& img)
{
    CV_Assert(!img.empty());

    if (img.depth() == CV_8U)
        return img;

    cv::Mat out8;

    if (img.depth() == CV_16U)
    {
        // simple downscale (viewable)
        img.convertTo(out8, CV_8U, 1.0 / 257.0);
        return out8;
    }

    if (img.depth() == CV_32F || img.depth() == CV_64F)
    {
        cv::Mat f;
        img.convertTo(f, CV_32F);

        double mn=0, mx=0;
        cv::minMaxLoc(f, &mn, &mx);
        if (mx <= mn) {
            out8 = cv::Mat(img.size(), CV_8U, cv::Scalar(0));
        } else {
            f.convertTo(out8, CV_8U, 255.0 / (mx - mn), -mn * 255.0 / (mx - mn));
        }
        return out8;
    }

    // fallback
    img.convertTo(out8, CV_8U);
    return out8;
}

static QImage matToQImage8(const cv::Mat& in)
{
    CV_Assert(!in.empty());
    CV_Assert(in.depth() == CV_8U);

    if (in.type() == CV_8UC1)
    {
        QImage q(in.data, in.cols, in.rows, (int)in.step, QImage::Format_Grayscale8);
        return q.copy();
    }
    else if (in.type() == CV_8UC3)
    {
        // OpenCV is BGR; Qt expects RGB for Format_RGB888
        cv::Mat rgb;
        cv::cvtColor(in, rgb, cv::COLOR_BGR2RGB);
        QImage q(rgb.data, rgb.cols, rgb.rows, (int)rgb.step, QImage::Format_RGB888);
        return q.copy();
    }
    else if (in.type() == CV_8UC4)
    {
        // OpenCV is BGRA; Qt expects RGBA8888 or ARGB32
        cv::Mat rgba;
        cv::cvtColor(in, rgba, cv::COLOR_BGRA2RGBA);
        QImage q(rgba.data, rgba.cols, rgba.rows, (int)rgba.step, QImage::Format_RGBA8888);
        return q.copy();
    }

    // Unexpected
    CV_Assert(false);
    return {};
}

static QImage matToQImageForPng(const cv::Mat& m)
{
    if (m.empty()) return QImage();

    // ---- 8-bit grayscale ----
    if (m.type() == CV_8UC1)
    {
        QImage img(m.cols, m.rows, QImage::Format_Grayscale8);
        for (int y = 0; y < m.rows; ++y)
            memcpy(img.scanLine(y), m.ptr(y), size_t(m.cols));
        return img;
    }

    // ---- 16-bit grayscale (preserve your depth_index.png!) ----
    if (m.type() == CV_16UC1)
    {
        QImage img(m.cols, m.rows, QImage::Format_Grayscale16);
        for (int y = 0; y < m.rows; ++y)
            memcpy(img.scanLine(y), m.ptr(y), size_t(m.cols) * 2);
        return img;
    }

    // ---- 8-bit BGR -> RGB ----
    if (m.type() == CV_8UC3)
    {
        cv::Mat rgb;
        cv::cvtColor(m, rgb, cv::COLOR_BGR2RGB);

        QImage img(rgb.data, rgb.cols, rgb.rows, int(rgb.step), QImage::Format_RGB888);
        return img.copy(); // deep copy so memory stays valid
    }

    // ---- 32F gray (any range) -> 8-bit gray preview ----
    if (m.type() == CV_32FC1)
    {
        double mn=0, mx=0;
        cv::minMaxLoc(m, &mn, &mx);

        cv::Mat out8;
        if (mx > mn)
            m.convertTo(out8, CV_8U, 255.0 / (mx - mn), -mn * 255.0 / (mx - mn));
        else
            out8 = cv::Mat(m.size(), CV_8U, cv::Scalar(0));

        return matToQImageForPng(out8);
    }

    // ---- 32F color -> 8-bit color preview ----
    if (m.type() == CV_32FC3)
    {
        // assume 0..1-ish, clamp
        cv::Mat clamped;
        cv::min(m, 1.0, clamped);
        cv::max(clamped, 0.0, clamped);

        cv::Mat bgr8;
        clamped.convertTo(bgr8, CV_8UC3, 255.0);
        return matToQImageForPng(bgr8);
    }

    // ---- Anything else: convert to 8U and try again ----
    cv::Mat tmp;
    m.convertTo(tmp, CV_8U);
    return matToQImageForPng(tmp);
}

bool writePngWithTitle(const QString& pngPath,
                       const cv::Mat& img,
                       bool writeMeta)
{
/*
    Setting the title and author and writing using QImageWriter and then bundling
    resulting file into a zip enables ChatGPT to extract the source file name,
    which makes it much easier to analyse many files.

    However, this is very slow, hence the writeMeta switch.
*/
    // return false;
    QString srcFun = "FSUtilities::writePngWithTitle";
    if (G::FSLog) G::log(srcFun, pngPath);

    if (img.empty()) return false;

    const QString t = QFileInfo(pngPath).fileName();

    if (!writeMeta) {
        cv::imwrite(pngPath.toStdString(), img);
        return true;
    }

    cv::Mat img8 = to8uViewable(img);
    QImage q = matToQImage8(img8);

    q.setText("Title", t);
    q.setText("Author", "Winnow FocusStack");

    if (G::FSLog) G::log(srcFun, t);

    QImageWriter writer(pngPath, "png");

    const bool ok = writer.write(q);
    if (!ok)
        qWarning() << "writePngWithTitle failed:" << pngPath << writer.errorString();
    return ok;
}

// small helper: quantile via histogram on 32F (fast enough for debug)
static void robustMinMax32F(const cv::Mat& m32f, float loQ, float hiQ, float& outLo, float& outHi)
{
    CV_Assert(m32f.type() == CV_32F);
    CV_Assert(loQ >= 0.f && loQ < hiQ && hiQ <= 1.f);

    double mn=0, mx=0;
    cv::minMaxLoc(m32f, &mn, &mx);
    if (mx <= mn) { outLo = (float)mn; outHi = (float)mx; return; }

    // 4096-bin histogram
    const int bins = 4096;
    std::vector<int> hist(bins, 0);

    const float fmn = (float)mn;
    const float fmx = (float)mx;
    const float inv = (bins - 1) / (fmx - fmn);

    const int total = m32f.rows * m32f.cols;
    for (int y = 0; y < m32f.rows; ++y)
    {
        const float* p = m32f.ptr<float>(y);
        for (int x = 0; x < m32f.cols; ++x)
        {
            float v = p[x];
            int b = (int)((v - fmn) * inv);
            b = std::max(0, std::min(bins - 1, b));
            hist[b]++;
        }
    }

    auto findQ = [&](float q)->float {
        const int target = (int)std::round(q * (total - 1));
        int acc = 0;
        for (int i = 0; i < bins; ++i) {
            acc += hist[i];
            if (acc >= target) {
                float t = (float)i / (bins - 1);
                return fmn + t * (fmx - fmn);
            }
        }
        return fmx;
    };

    outLo = findQ(loQ);
    outHi = findQ(hiQ);
    if (outHi <= outLo) { outLo = (float)mn; outHi = (float)mx; }
}

bool writePngFromFloatMapRobust(const QString& pngPath,
                                const cv::Mat& map32f,
                                float loQuantile,
                                float hiQuantile)
{
    if (map32f.empty()) return false;
    CV_Assert(map32f.type() == CV_32F);

    float lo=0, hi=0;
    robustMinMax32F(map32f, loQuantile, hiQuantile, lo, hi);

    cv::Mat clipped = map32f.clone();
    cv::min(clipped, hi, clipped);
    cv::max(clipped, lo, clipped);

    cv::Mat out8;
    if (hi <= lo) out8 = cv::Mat(map32f.size(), CV_8U, cv::Scalar(0));
    else clipped.convertTo(out8, CV_8U, 255.0 / (hi - lo), -lo * 255.0 / (hi - lo));

    return writePngWithTitle(pngPath, out8);
}

bool heatMapPerSlice(const QString &pngPath,
                              const cv::Mat& depthIndex16,
                              int sliceCount,
                              int colormap)
{
    QString srcFun = "FSUtilities::heatMapPerSlice";
    CV_Assert(!depthIndex16.empty());
    CV_Assert(depthIndex16.type() == CV_16U);
    CV_Assert(sliceCount >= 2);

    /*
    // 1) Convert slice index -> 0..255 using sliceCount (NOT min/max normalize)
    //    This makes coloring consistent across images and runs.
    cv::Mat idx32;
    depthIndex16.convertTo(idx32, CV_32F);

    // Clamp to valid index range (defensive)
    cv::min(idx32, float(sliceCount - 1), idx32);
    cv::max(idx32, 0.0f, idx32);

    const float inv = 255.0f / float(sliceCount - 1);

    cv::Mat lut8;
    idx32.convertTo(lut8, CV_8U, inv);  // 0..255, slice-count-accurate

    // 2) Apply colormap
    cv::Mat heatBGR;    // CV_8UC3 (BGR)
    cv::applyColorMap(lut8, heatBGR, colormap);

    return writePngWithTitle(pngPath, heatBGR);
    */

    const int rows = depthIndex16.rows;
    const int cols = depthIndex16.cols;

    // Build HSV image:
    //   H: 0..179 (OpenCV hue range)
    //   S: 255
    //   V: 255
    cv::Mat hsv(rows, cols, CV_8UC3);

    for (int y = 0; y < rows; ++y)
    {
        const uint16_t* dRow = depthIndex16.ptr<uint16_t>(y);
        cv::Vec3b*      hRow = hsv.ptr<cv::Vec3b>(y);

        for (int x = 0; x < cols; ++x)
        {
            int idx = int(dRow[x]);
            if (idx < 0) idx = 0;
            if (idx >= sliceCount) idx = sliceCount - 1;

            // Map idx -> hue in [0..179], evenly spaced by sliceCount.
            // Use rounding so endpoints hit nicely.
            int hue = int(std::lround(179.0 * double(idx) / double(sliceCount - 1)));

            hRow[x][0] = (uchar)hue;   // H
            hRow[x][1] = 255;          // S
            hRow[x][2] = 255;          // V
        }
    }

    cv::Mat heatBGR;
    cv::cvtColor(hsv, heatBGR, cv::COLOR_HSV2BGR);

    qDebug() << srcFun << "1";

    return writePngWithTitle(pngPath, heatBGR);
}

QString cvTypeToStr(int type)
{
    const int depth = type & CV_MAT_DEPTH_MASK;
    const int chans = 1 + (type >> CV_CN_SHIFT);

    QString d;
    switch (depth) {
    case CV_8U:  d = "8U"; break;
    case CV_8S:  d = "8S"; break;
    case CV_16U: d = "16U"; break;
    case CV_16S: d = "16S"; break;
    case CV_32S: d = "32S"; break;
    case CV_32F: d = "32F"; break;
    case CV_64F: d = "64F"; break;
    default:     d = "?"; break;
    }
    return QString("CV_%1C%2").arg(d).arg(chans);
}

void dumpMatStats(const char* name, const cv::Mat& m)
{
    if (m.empty()) {
        qDebug().noquote() << name << ": EMPTY";
        return;
    }
    double mn=0, mx=0;
    cv::minMaxLoc(m.reshape(1), &mn, &mx); // flatten channels
    qDebug().noquote() << QString("%1: %2x%3 %4 min=%5 max=%6")
                              .arg(name)
                              .arg(m.rows).arg(m.cols)
                              .arg(cvTypeToStr(m.type()))
                              .arg(mn, 0, 'g', 8)
                              .arg(mx, 0, 'g', 8);
}

void dumpMatFirstN(const char* name, const cv::Mat& m, int N)
{
    if (m.empty()) {
        qDebug().noquote() << name << ": EMPTY";
        return;
    }
    CV_Assert(m.cols == 1 || m.rows == 1); // assume vector-like for your 5x1 / 6x1

    cv::Mat v = m.reshape(1, (int)m.total()); // Nx1, single channel view
    QStringList parts;
    const int n = std::min(N, v.rows);

    for (int i = 0; i < n; ++i)
    {
        double val = 0.0;
        int t = v.type();
        switch (t) {
        case CV_32F: val = v.at<float>(i,0); break;
        case CV_64F: val = v.at<double>(i,0); break;
        case CV_16S: val = v.at<int16_t>(i,0); break;
        case CV_16U: val = v.at<uint16_t>(i,0); break;
        case CV_32S: val = v.at<int>(i,0); break;
        case CV_8U:  val = v.at<uint8_t>(i,0); break;
        default:
            parts << "<?>"; continue;
        }
        parts << QString::number(val, 'g', 10);
    }

    qDebug().noquote() << QString("%1 first=%2").arg(name).arg(parts.join(", "));
}

} // namespace FSUtilities
