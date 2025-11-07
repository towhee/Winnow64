#include "focusstackutilities.h"
#include <QDebug>

namespace FSUtils {

// ============================================================================
// QImage → cv::Mat
// ============================================================================
cv::Mat qimageToMat(const QImage &img)
{
    if (img.isNull()) return cv::Mat();

    switch (img.format()) {
    case QImage::Format_RGB888: {
        cv::Mat mat(img.height(), img.width(), CV_8UC3,
                    const_cast<uchar*>(img.bits()), img.bytesPerLine());
        cv::Mat matBGR;
        cv::cvtColor(mat, matBGR, cv::COLOR_RGB2BGR);
        return matBGR.clone();
    }
    case QImage::Format_Grayscale8:
        return cv::Mat(img.height(), img.width(), CV_8UC1,
                       const_cast<uchar*>(img.bits()), img.bytesPerLine()).clone();
    case QImage::Format_ARGB32:
    case QImage::Format_RGBA8888:
        return cv::Mat(img.height(), img.width(), CV_8UC4,
                       const_cast<uchar*>(img.bits()), img.bytesPerLine()).clone();
    default: {
        QImage conv = img.convertToFormat(QImage::Format_RGB888);
        return qimageToMat(conv);
    }
    }
}

// ============================================================================
// cv::Mat → QImage
// ============================================================================
QImage matToQImage(const cv::Mat &mat)
{
    if (mat.empty()) return QImage();

    switch (mat.type()) {
    case CV_8UC1: {
        QImage img(mat.cols, mat.rows, QImage::Format_Grayscale8);
        memcpy(img.bits(), mat.data, size_t(mat.total()));
        return img;
    }
    case CV_8UC3: {
        cv::Mat rgb;
        cv::cvtColor(mat, rgb, cv::COLOR_BGR2RGB);
        QImage img(rgb.data, rgb.cols, rgb.rows, rgb.step, QImage::Format_RGB888);
        return img.copy();  // Deep copy to detach from cv::Mat memory
    }
    case CV_8UC4: {
        QImage img(mat.data, mat.cols, mat.rows, mat.step, QImage::Format_ARGB32);
        return img.copy();
    }
    default: {
        // Normalize float or other types to 8-bit grayscale
        cv::Mat norm8;
        cv::normalize(mat, norm8, 0, 255, cv::NORM_MINMAX);
        norm8.convertTo(norm8, CV_8U);
        QImage img(norm8.data, norm8.cols, norm8.rows, norm8.step, QImage::Format_Grayscale8);
        return img.copy();
    }
    }
}

// ============================================================================
// Normalization helpers
// ============================================================================
cv::Mat normalizeTo8U(const cv::Mat &src)
{
    cv::Mat norm8;
    cv::normalize(src, norm8, 0, 255, cv::NORM_MINMAX);
    norm8.convertTo(norm8, CV_8U);
    return norm8;
}

QImage colorizeDepth(const cv::Mat &depthIdx)
{
    cv::Mat color;
    cv::applyColorMap(FSUtils::normalizeTo8U(depthIdx), color, cv::COLORMAP_JET);
    return FSUtils::matToQImage(color);
}

void debugSaveMat(const cv::Mat &mat, const QString &path)
{
    cv::Mat out;
    if (mat.depth() != CV_8U)
        cv::normalize(mat, out, 0, 255, cv::NORM_MINMAX);
    else
        out = mat;
    cv::imwrite(path.toStdString(), out);
}

// ============================================================================
// Mathematical helpers
// ============================================================================
cv::Mat safeDivide(const cv::Mat &num, const cv::Mat &den, double eps)
{
    CV_Assert(num.size() == den.size() && num.type() == den.type());
    cv::Mat denomSafe = den + eps;
    cv::Mat out;
    cv::divide(num, denomSafe, out);
    return out;
}

cv::Mat applyGamma(const cv::Mat &src, double gamma)
{
    cv::Mat dst;
    if (gamma == 1.0)
        return src.clone();

    cv::Mat src32;
    src.convertTo(src32, CV_32F, 1.0 / 255.0);
    cv::pow(src32, gamma, src32);
    src32.convertTo(dst, CV_8U, 255.0);
    return dst;
}

cv::Mat blendByMask(const cv::Mat &a, const cv::Mat &b, const cv::Mat &mask)
{
    CV_Assert(a.size() == b.size());
    cv::Mat maskF;
    mask.convertTo(maskF, CV_32F, 1.0 / 255.0);
    cv::Mat a32, b32, out;
    a.convertTo(a32, CV_32F);
    b.convertTo(b32, CV_32F);
    cv::addWeighted(a32, 1.0, b32, 1.0, 0.0, out, CV_32F);
    cv::Mat blended = a32.mul(1.0 - maskF) + b32.mul(maskF);
    blended.convertTo(out, a.type());
    return out;
}

cv::Mat autoLevels(const cv::Mat &src, double clipLow, double clipHigh)
{
    cv::Mat gray;
    if (src.channels() == 3)
        cv::cvtColor(src, gray, cv::COLOR_BGR2GRAY);
    else
        gray = src.clone();

    // Compute histogram percentiles
    cv::Mat flat = gray.reshape(1, 1);
    std::vector<float> vals; flat.convertTo(vals, CV_32F);
    std::nth_element(vals.begin(), vals.begin() + vals.size() * clipLow / 100.0, vals.end());
    float vmin = vals[vals.size() * clipLow / 100.0];
    std::nth_element(vals.begin(), vals.begin() + vals.size() * clipHigh / 100.0, vals.end());
    float vmax = vals[vals.size() * clipHigh / 100.0];

    cv::Mat dst;
    cv::Mat src32; src.convertTo(src32, CV_32F);
    dst = (src32 - vmin) / std::max(1e-5f, vmax - vmin);
    cv::threshold(dst, dst, 0, 0, cv::THRESH_TOZERO);
    cv::threshold(dst, dst, 1, 1, cv::THRESH_TRUNC);
    dst.convertTo(dst, CV_8U, 255.0);
    return dst;
}

// ============================================================================
// EXR helpers (for float maps)
// ============================================================================
bool saveFloatEXR(const QString &path, const cv::Mat &img)
{
#ifdef HAVE_OPENCV_IMGCODECS
    if (img.type() != CV_32FC1 && img.type() != CV_32FC3)
        qWarning() << "saveFloatEXR: expected CV_32F, converting.";
    cv::Mat tmp;
    img.convertTo(tmp, CV_32F);
    return cv::imwrite(path.toStdString(), tmp);
#else
    Q_UNUSED(path);
    Q_UNUSED(img);
    qWarning() << "saveFloatEXR: OpenCV built without EXR support.";
    return false;
#endif
}

cv::Mat loadFloatEXR(const QString &path)
{
#ifdef HAVE_OPENCV_IMGCODECS
    cv::Mat img = cv::imread(path.toStdString(), cv::IMREAD_UNCHANGED);
    if (img.empty()) {
        qWarning() << "loadFloatEXR: failed" << path;
        return cv::Mat();
    }
    if (img.type() != CV_32F)
        img.convertTo(img, CV_32F);
    return img;
#else
    Q_UNUSED(path);
    qWarning() << "loadFloatEXR: OpenCV built without EXR support.";
    return cv::Mat();
#endif
}

} // namespace FSUtils
