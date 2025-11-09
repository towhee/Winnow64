#ifndef FOCUSSTACKUTILITIES_H
#define FOCUSSTACKUTILITIES_H

#include <QImage>
#include <QString>
#include <QDir>
#include <QFileInfo>
#include <opencv2/opencv.hpp>

//
// ============================================================================
// Focus Stack Utilities
// ============================================================================
// These functions provide low-level image and math utilities for the
// FocusStack pipeline (FocusMeasure, DepthMap, StackFusion).
// All are thread-safe and standalone — they do not depend on Winnow’s globals.
// ============================================================================
//

namespace FSUtils {

// ----------------------------------------------------------------------------
// QImage ↔︎ cv::Mat conversions
// ----------------------------------------------------------------------------
cv::Mat qimageToMat(const QImage &img);
QImage matToQImage(const cv::Mat &mat);

// ----------------------------------------------------------------------------
// Normalization / visualization
// ----------------------------------------------------------------------------
cv::Mat normalizeTo8U(const cv::Mat &src);
QImage colorizeDepth(const cv::Mat &depthIdx);
void debugSaveMat(const cv::Mat &mat, const QString &path);

// ----------------------------------------------------------------------------
// Common mathematical helpers
// ----------------------------------------------------------------------------
cv::Mat safeDivide(const cv::Mat &num, const cv::Mat &den, double eps = 1e-6);
cv::Mat applyGamma(const cv::Mat &src, double gamma);
cv::Mat blendByMask(const cv::Mat &a, const cv::Mat &b, const cv::Mat &mask);
cv::Mat autoLevels(const cv::Mat &src, double clipLow = 1.0, double clipHigh = 99.0);
bool hasNaNs(const cv::Mat &m, const QString &label = QString());

// ----------------------------------------------------------------------------
// EXR helpers (optional)
// ----------------------------------------------------------------------------
bool saveFloatEXR(const QString &path, const cv::Mat &img);
cv::Mat loadFloatEXR(const QString &path);

} // namespace FSUtils

#endif // FOCUSSTACKUTILITIES_H
