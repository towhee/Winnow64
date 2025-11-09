#include "focusstackutilities.h"
#include <QDebug>

namespace FSUtils {

constexpr float EPS_NORM = 1e-12f;

// ============================================================================
// QImage → cv::Mat
// ============================================================================

cv::Mat qimageToMat(const QImage &img)
{
    if (img.isNull()) return cv::Mat();

    // Always convert once to a CPU-only, contiguous format we fully own.
    QImage converted = img.convertToFormat(QImage::Format_RGBA8888);

    // Wrap then immediately convert to BGR 8U 3ch (commonly expected in OpenCV),
    // and CLONE so we're detached from QImage storage.
    cv::Mat rgba(converted.height(), converted.width(), CV_8UC4,
                 const_cast<uchar*>(converted.bits()), converted.bytesPerLine());
    cv::Mat bgr;
    cv::cvtColor(rgba, bgr, cv::COLOR_RGBA2BGR);
    return bgr.clone();  // fully detached
}

// ============================================================================
// cv::Mat → QImage
// ============================================================================
QImage matToQImage(const cv::Mat &mat)
{
    if (mat.empty()) return QImage();

    switch (mat.type()) {
        case CV_8UC1: {
            QImage out(mat.cols, mat.rows, QImage::Format_Grayscale8);
            for (int y = 0; y < mat.rows; ++y)
                memcpy(out.scanLine(y), mat.ptr(y), size_t(mat.cols));
            return out;
        }
        case CV_8UC3: {
            // BGR -> RGB
            cv::Mat rgb;
            cv::cvtColor(mat, rgb, cv::COLOR_BGR2RGB);
            QImage out(rgb.cols, rgb.rows, QImage::Format_RGB888);
            for (int y = 0; y < rgb.rows; ++y)
                memcpy(out.scanLine(y), rgb.ptr(y), size_t(rgb.cols * 3));
            return out;
        }
        case CV_8UC4: {
            // Assume BGRA; convert to RGBA8888 for Qt
            cv::Mat rgba;
            cv::cvtColor(mat, rgba, cv::COLOR_BGRA2RGBA);
            QImage out(rgba.cols, rgba.rows, QImage::Format_RGBA8888);
            for (int y = 0; y < rgba.rows; ++y)
                memcpy(out.scanLine(y), rgba.ptr(y), size_t(rgba.cols * 4));
            return out;
        }
        default: {
            // Normalize anything else (e.g., CV_32F/64F) to 8-bit grayscale
            cv::Mat src32;
            mat.convertTo(src32, CV_32F);
            cv::patchNaNs(src32, 0.0);

            // Clamp into [0,1] range (most fusion intermediates are already normalized)
            cv::threshold(src32, src32, 0, 1, cv::THRESH_TOZERO);
            cv::min(src32, 1.0, src32);

            cv::Mat norm8;
            src32.convertTo(norm8, CV_8U, 255.0);

            QImage out(norm8.cols, norm8.rows, QImage::Format_Grayscale8);
            for (int y = 0; y < norm8.rows; ++y)
                memcpy(out.scanLine(y), norm8.ptr(y), size_t(norm8.cols));
            return out;
        }
    }
}

// ============================================================================
// Normalization helpers
// ============================================================================

cv::Mat normalizeTo8U(const cv::Mat &mat)
{
    if (mat.empty())
        return cv::Mat();

    // Convert to float for uniform normalization
    cv::Mat src;
    if (mat.depth() == CV_8U)
        mat.convertTo(src, CV_32F, 1.0 / 255.0);
    else
        mat.convertTo(src, CV_32F);

    std::vector<cv::Mat> channels;
    cv::split(src, channels);

    std::vector<cv::Mat> normCh;
    normCh.reserve(channels.size());

    for (auto &ch : channels) {
        double mn, mx;
        cv::minMaxLoc(ch, &mn, &mx);
        cv::Mat norm;

        if (std::fabs(mx - mn) < EPS_NORM) {
            // Flat channel: pick appropriate gray level
            float val = static_cast<float>(mn);
            if (val >= 0.5f)
                norm = cv::Mat(ch.size(), CV_8U, cv::Scalar(255));
            else if (val > 1e-6f)
                norm = cv::Mat(ch.size(), CV_8U, cv::Scalar(val * 255.0f));
            else
                norm = cv::Mat(ch.size(), CV_8U, cv::Scalar(0));
        } else {
            cv::Mat tmp = (ch - static_cast<float>(mn)) / static_cast<float>(mx - mn);
            tmp.convertTo(norm, CV_8U, 255.0);
        }

        normCh.push_back(norm);
    }

    cv::Mat out;
    if (normCh.size() == 1)
        out = normCh[0];
    else
        cv::merge(normCh, out);

    return out;
}

QImage colorizeDepth(const cv::Mat &depthIdx)
{
    if (depthIdx.empty()) return QImage();

    cv::Mat idx8;
    if (depthIdx.type() != CV_8UC1) {
        // scale to [0..255]
        double mn, mx;
        cv::minMaxLoc(depthIdx, &mn, &mx);
        if (mx <= mn) return QImage(depthIdx.cols, depthIdx.rows, QImage::Format_Grayscale8);
        depthIdx.convertTo(idx8, CV_8U, 255.0 / (mx - mn), -mn * 255.0 / (mx - mn));
    } else {
        idx8 = depthIdx;
    }

    cv::Mat colorBGR;
    cv::applyColorMap(idx8, colorBGR, cv::COLORMAP_JET);
    cv::Mat colorRGB;
    cv::cvtColor(colorBGR, colorRGB, cv::COLOR_BGR2RGB);
    return matToQImage(colorRGB);
}

void debugSaveMat(const cv::Mat &mat, const QString &path)
{
    if (mat.empty()) return;

    cv::Mat src;
    if (mat.depth() == CV_8U) {
        src = mat;
    } else {
        mat.convertTo(src, CV_32F);
    }

    // Handle multi-channel input by normalizing per-channel
    std::vector<cv::Mat> channels;
    cv::split(src, channels);

    std::vector<cv::Mat> normCh;
    normCh.reserve(channels.size());

    for (auto &ch : channels) {
        double mn, mx;
        cv::minMaxLoc(ch, &mn, &mx);
        cv::Mat norm;
        if (std::fabs(mx - mn) < EPS_NORM) {
            // Flat image — visualize as black or white depending on value
            float val = (float)mn;
            if (val > 0.5f)
                norm = cv::Mat(ch.size(), CV_8U, cv::Scalar(255));
            else if (val > 1e-6f)
                norm = cv::Mat(ch.size(), CV_8U, cv::Scalar(val * 255.0f));
            else
                norm = cv::Mat(ch.size(), CV_8U, cv::Scalar(0));
        } else {
            ch.convertTo(norm, CV_32F);
            norm = (norm - (float)mn) / (float)(mx - mn);
            norm.convertTo(norm, CV_8U, 255.0);
        }
        normCh.push_back(norm);
    }

    cv::Mat toWrite;
    if (normCh.size() == 1)
        toWrite = normCh[0];
    else
        cv::merge(normCh, toWrite);

    // Ensure directories exist
    QFileInfo fi(path);
    QDir().mkpath(fi.path());

    cv::imwrite(path.toStdString(), toWrite);
}

// ============================================================================
// Mathematical helpers
// ============================================================================
cv::Mat safeDivide(const cv::Mat &num, const cv::Mat &den, double eps)
{
    CV_Assert(num.size() == den.size() && num.type() == den.type());
    cv::Mat out(num.size(), num.type(), cv::Scalar(0));

    // Per-pixel mask for valid denominators
    cv::Mat mask = (cv::abs(den) > eps);

    cv::Mat tmp;
    cv::divide(num, den + eps, tmp, 1.0, num.type());
    tmp.copyTo(out, mask); // copy only where valid

    return out;
}

cv::Mat applyGamma(const cv::Mat &src, double gamma)
{
    if (gamma == 1.0) return src.clone();

    cv::Mat src32;
    if (src.depth() == CV_8U)
        src.convertTo(src32, CV_32F, 1.0 / 255.0);
    else
        src.convertTo(src32, CV_32F);

    cv::pow(src32, gamma, src32);

    cv::Mat dst;
    src32.convertTo(dst, CV_8U, 255.0);
    return dst;
}

cv::Mat blendByMask(const cv::Mat &a, const cv::Mat &b, const cv::Mat &mask)
{
    CV_Assert(a.size() == b.size() && a.type() == b.type());
    cv::Mat mF;

    // Detect pre-normalized mask range/type
    if (mask.depth() == CV_8U)
        mask.convertTo(mF, CV_32F, 1.0 / 255.0);
    else
        mF = mask.clone();

    cv::threshold(mF, mF, 0.0, 1.0, cv::THRESH_TOZERO);
    cv::min(mF, 1.0, mF);

    cv::Mat inv = 1.0 - mF;

    cv::Mat a32, b32; a.convertTo(a32, CV_32F); b.convertTo(b32, CV_32F);
    cv::Mat out = a32.mul(inv) + b32.mul(mF);
    out.convertTo(out, a.type());
    return out;
}

cv::Mat autoLevels(const cv::Mat &src, double clipLow, double clipHigh)
{
    // Skip for non-8U images — avoids amplifying float noise
    if (src.depth() != CV_8U)
        return src.clone();

    cv::Mat gray;
    if (src.channels() == 3) cv::cvtColor(src, gray, cv::COLOR_BGR2GRAY);
    else gray = src;

    cv::Mat flat = gray.reshape(1, 1).clone();
    flat.convertTo(flat, CV_32F);

    // percentiles
    int total = flat.cols;
    int iLow  = std::clamp(int(total * (clipLow  / 100.0)), 0, total-1);
    int iHigh = std::clamp(int(total * (clipHigh / 100.0)), 0, total-1);

    cv::Mat sorted;
    cv::sort(flat, sorted, cv::SORT_ASCENDING);

    float vmin = sorted.at<float>(0, iLow);
    float vmax = sorted.at<float>(0, iHigh);
    if (vmax <= vmin) return src.clone();

    cv::Mat src32, norm;
    src.convertTo(src32, CV_32F);
    norm = (src32 - vmin) / (vmax - vmin);
    cv::threshold(norm, norm, 0, 0, cv::THRESH_TOZERO);
    cv::threshold(norm, norm, 1, 1, cv::THRESH_TRUNC);
    norm.convertTo(norm, src.depth(), (src.depth()==CV_8U) ? 255.0 : 1.0);
    norm.convertTo(norm, src.type());
    return norm;
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

bool hasNaNs(const cv::Mat &m, const QString &label)
{
    if (m.empty())
        return false;

    cv::Mat isnanMask, isinfMask;

    // Detect NaN and Inf separately
    // cv::patchNaNs(const_cast<cv::Mat&>(m), 0.0); // ensures NaNs don't propagate in comparison
    cv::compare(m, m, isnanMask, cv::CMP_NE);   // NaN != NaN → true
    cv::Mat absM; cv::absdiff(m, 0, absM);
    cv::compare(absM, std::numeric_limits<float>::infinity(), isinfMask, cv::CMP_EQ);

    int countNaN = 0, countInf = 0;

    if (isnanMask.channels() > 1) {
        std::vector<cv::Mat> ch;
        cv::split(isnanMask, ch);
        for (const auto &c : ch) countNaN += cv::countNonZero(c);
    } else {
        countNaN = cv::countNonZero(isnanMask);
    }

    if (isinfMask.channels() > 1) {
        std::vector<cv::Mat> ch;
        cv::split(isinfMask, ch);
        for (const auto &c : ch) countInf += cv::countNonZero(c);
    } else {
        countInf = cv::countNonZero(isinfMask);
    }

    bool found = (countNaN > 0 || countInf > 0);

    #ifndef NDEBUG
    if (found) {
        qWarning().noquote() << "*******[FSUtils::hasNaNs]*******"
                             << (label.isEmpty() ? QStringLiteral("<unnamed>") : label)
                             << QString("NaN:%1  Inf:%2  (%3×%4, %5ch)")
                                    .arg(countNaN)
                                    .arg(countInf)
                                    .arg(m.cols)
                                    .arg(m.rows)
                                    .arg(m.channels());
    }
    #endif

    // cv::patchNaNs(const_cast<cv::Mat&>(m), 0.0); // ensures NaNs don't propagate in comparison

    return found;
}

} // namespace FSUtils
