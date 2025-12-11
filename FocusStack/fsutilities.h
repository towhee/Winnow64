#ifndef FSUTILITIES_H
#define FSUTILITIES_H

#include <opencv2/core.hpp>
#include <QString>

namespace FSUtilities {

/*
 * Create a diagnostic composite:
 *   ┌──────────────────────┐
 *   │      Depth Preview    │
 *   ├──────────────────────┤
 *   │      Fused Image      │
 *   ├──────────────────────┤
 *   │ Slice A │  Slice B    │
 *   └──────────────────────┘
 */
bool makeDebugOverview(const cv::Mat &depthPreview,
                       const cv::Mat &fusedColor,
                       const cv::Mat &sliceA,
                       const cv::Mat &sliceB,
                       const QString &outputPath,
                       int maxWidth = 2400);

/*
 * Utility: ensure image is 3-channel BGR.
 */
cv::Mat ensureColor(const cv::Mat &img);

/*
 * Utility: draws a label text onto an image (top-left corner).
 */
cv::Mat addLabel(const cv::Mat &img, const std::string &text);

/*
 * Horizontal stack with automatic height matching + resizing.
 */
cv::Mat hstack(const cv::Mat &a, const cv::Mat &b);

/*
 * Vertical stack with automatic width matching + resizing.
 */
cv::Mat vstack(const cv::Mat &a, const cv::Mat &b);

} // namespace FSUtilities

#endif // FSUTILITIES_H
