#include "FSUtilities.h"
#include <QtCore/qdebug.h>
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



} // namespace FSUtilities
