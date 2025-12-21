#ifndef FSUTILITIES_H
#define FSUTILITIES_H

#include <opencv2/core.hpp>
#include <QString>
#include <QDebug>
#include <atomic>
#include <vector>

namespace FSUtilities {

// Small helpers can be inline in the header
inline QString sizeToStr(const cv::Size& s) {
    return QString("%1x%2").arg(s.width).arg(s.height);
}

inline QString matSizeToStr(const cv::Mat& m) {
    return sizeToStr(m.size());
}

// Declarations (NO bodies here)
bool makeDebugOverview(const cv::Mat &depthPreview,
                       const cv::Mat &fusedColor,
                       const cv::Mat &sliceA,
                       const cv::Mat &sliceB,
                       const QString &outputPath,
                       int   maxWidth);

cv::Mat ensureColor(const cv::Mat &img);
cv::Mat addLabel(const cv::Mat &img, const std::string &text);
cv::Mat hstack(const cv::Mat &a, const cv::Mat &b);
cv::Mat vstack(const cv::Mat &a, const cv::Mat &b);

cv::Mat showWithMask(const cv::Mat &baseGray,
                     const cv::Mat &maskBGR,
                     float alpha);

bool showWithMask(const cv::Mat &baseGray,
                  const cv::Mat &maskBGR,
                  const QString &outputPath,
                  float alpha);

void assertSameSize(const cv::Mat& a,
                    const cv::Mat& b,
                    const QString& context);

void assertSameSize(const cv::Mat& a,
                    const cv::Mat& b,
                    const cv::Mat& c,
                    const QString& context);

cv::Mat canonicalizeGrayFloat01(const cv::Mat& in,
                                const cv::Size& canonicalSize,
                                int interp,
                                const char* tag);

cv::Mat canonicalizeDepthIndex16(const cv::Mat& depthIndex16,
                                 const cv::Size& canonicalSize,
                                 const char* tag);

std::vector<cv::Mat> canonicalizeFocusSlices(const std::vector<cv::Mat>& focusSlices32,
                                             const cv::Size& canonicalSize,
                                             std::atomic_bool* abortFlag);

std::vector<cv::Mat> canonicalizeAlignedColor(const std::vector<cv::Mat>& alignedColor,
                                              const cv::Size& canonicalSize,
                                              std::atomic_bool* abortFlag);

// If you want canonicalizeToSize available to other modules, declare it here.
// (Make it non-inline and implement in .cpp, OR inline it here.)
cv::Mat canonicalizeToSize(const cv::Mat& src,
                           const cv::Size& canonicalSize,
                           int interp,
                           const QString& context);

bool writePngWithTitle(const QString& pngPath,
                       const cv::Mat& img);

bool writePngFromFloatMapRobust(const QString& pngPath,
                                const cv::Mat& map32f,
                                float loQuantile,
                                float hiQuantile);

} // namespace FSUtilities

#endif // FSUTILITIES_H
