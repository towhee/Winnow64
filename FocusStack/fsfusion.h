#pragma once

#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>

#include <QString>
#include <functional>
#include <atomic>
#include <algorithm>

#include <QDebug>

class FSFusion
{
public:
    FSFusion() = default;
    virtual ~FSFusion() = default;

    // ------------------------------------------------------------
    // callbacks
    // ------------------------------------------------------------
    using ProgressCallback = std::function<void()>;
    using StatusCallback   = std::function<void(const QString&)>;

    // ------------------------------------------------------------
    // shared options
    // ------------------------------------------------------------
    struct Options
    {
        QString method        = "PMax";     // "PMax" or "DMap"
        bool    useOpenCL     = false;
        int     consistency   = 2;
        QString depthFolderPath;
    };

    // ------------------------------------------------------------
    // shared geometry (set by caller before engine runs)
    // ------------------------------------------------------------
    cv::Size alignSize;
    cv::Size padSize;
    cv::Size origSize;
    cv::Rect validAreaAlign;

    int outDepth = CV_8U; // CV_8U or CV_16U

    // ------------------------------------------------------------
    // geometry helpers
    // ------------------------------------------------------------
    void resetGeometry();
    bool hasValidGeometry() const;
    QString geometryText() const;

    // ------------------------------------------------------------
    // utilities
    // ------------------------------------------------------------
    static inline bool isAbort(const std::atomic_bool* f)
    {
        return f && f->load(std::memory_order_relaxed);
    }

    static inline QString cvSizeToText(const cv::Size& s)
    {
        return QString("%1x%2").arg(s.width).arg(s.height);
    }

    static inline QString cvRectToText(const cv::Rect& r)
    {
        return QString("(%1,%2,%3,%4)")
            .arg(r.x).arg(r.y).arg(r.width).arg(r.height);
    }

    // clamps
    static inline int clampi(int v, int lo, int hi)
    {
        return std::max(lo, std::min(hi, v));
    }

    static inline float clampf(float v, float lo, float hi)
    {
        return std::max(lo, std::min(hi, v));
    }

    // param == -1 means "use defaultIfNegOne"
    static inline int resolveLevelParam(int param, int levels, int defaultIfNegOne)
    {
        if (levels <= 0) return 0;

        if (param == -1) param = defaultIfNegOne;
        if (param < 0) param = 0;

        return clampi(param, 0, std::max(0, levels - 1));
    }

    // geometry
    static inline bool rectInside(const cv::Rect& r, const cv::Size& s)
    {
        return r.x >= 0 && r.y >= 0 &&
               r.width > 0 && r.height > 0 &&
               r.x + r.width  <= s.width &&
               r.y + r.height <= s.height;
    }

    // morphology
    static inline cv::Mat seEllipse(int px)
    {
        const int k = 2 * px + 1;
        return cv::getStructuringElement(cv::MORPH_ELLIPSE, cv::Size(k, k));
    }

    // conversions
    static inline cv::Mat toFloat01(const cv::Mat& gray8)
    {
        CV_Assert(gray8.type() == CV_8U);
        cv::Mat f;
        gray8.convertTo(f, CV_32F, 1.0 / 255.0);
        return f;
    }

    static inline void gaussianBlurInPlace(cv::Mat& m, float sigma)
    {
        if (sigma <= 0.0f) return;
        cv::GaussianBlur(m, m, cv::Size(0, 0), sigma, sigma, cv::BORDER_REFLECT);
    }

    // debug
    static inline QString matInfo(const cv::Mat& m)
    {
        return QString("size=%1x%2 type=%3 ch=%4 empty=%5 step=%6")
            .arg(m.cols).arg(m.rows)
            .arg(m.type())
            .arg(m.channels())
            .arg(m.empty())
            .arg(static_cast<qulonglong>(m.step));
    }

    static inline void assertSameMat(const cv::Mat& a, const cv::Mat& b, const char* where)
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
};

