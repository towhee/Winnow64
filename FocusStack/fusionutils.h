#pragma once

#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>

#include <QtGlobal>
#include <QDebug>
#include <QString>

// ============================================================
// FusionUtils.h
// Small inline helpers used across fusion modules.
// Keep this header light and dependency-safe.
// ============================================================

namespace FusionUtils
{
// ----------------------------
// morphology (dup in fsfusion.h)
// ----------------------------
static inline cv::Mat seEllipse(int px)
{
    const int k = 2 * px + 1;
    return cv::getStructuringElement(cv::MORPH_ELLIPSE, cv::Size(k, k));
}

// ----------------------------
// Debug helper
// ----------------------------
inline QString matInfo(const cv::Mat& m)
{
    return QString("size=%1x%2 type=%3 ch=%4 empty=%5 step=%6")
        .arg(m.cols).arg(m.rows)
        .arg(m.type())
        .arg(m.channels())
        .arg(m.empty())
        .arg(static_cast<qulonglong>(m.step));
}

inline void assertSameMat(const cv::Mat& a, const cv::Mat& b, const char* where)
{
    if (a.size() != b.size() || a.type() != b.type())
    {
        qWarning().noquote()
            << "MAT MISMATCH at" << where
            << "A:" << matInfo(a)
            << "B:" << matInfo(b);
        CV_Assert(a.size() == b.size());
        CV_Assert(a.type() == b.type());
    }
}
} // namespace FusionUtils
