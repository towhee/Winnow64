#include "FSUtilities.h"
#include <opencv2/imgproc.hpp>
#include <opencv2/imgcodecs.hpp>

namespace FSUtilities {

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

} // namespace FSUtilities
