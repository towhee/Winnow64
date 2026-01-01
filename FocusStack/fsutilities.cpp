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

    if (img.empty()) return false;

    const QString t = QFileInfo(pngPath).fileName();

    if (!writeMeta) {
        qDebug() << srcFun << "1";
        cv::imwrite(pngPath.toStdString(), img);
        qDebug() << srcFun << "2";
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

} // namespace FSUtilities
