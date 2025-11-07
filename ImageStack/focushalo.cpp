#include "focushalo.h"

FocusHalo::FocusHalo(QObject *parent)
    : QObject(parent)
{
}

void FocusHalo::setOutputDir(const QDir &dir)  { haloDir = dir; }
void FocusHalo::setStrength(float s)           { strength = std::clamp(s, 0.0f, 1.0f); }
void FocusHalo::setRadius(int r)               { radius = std::max(1, r); }

bool FocusHalo::removeHalos(const QString &fusedPath, const QString &outputName)
{
    QString src = "FocusHalo::removeHalos";
    qDebug() << src;
    emit updateStatus(false, QString("Removing halos from %1").arg(fusedPath), src);

    cv::Mat input = cv::imread(fusedPath.toStdString(), cv::IMREAD_COLOR);
    if (input.empty()) {
        emit updateStatus(false, "Failed to load fused image.", src);
        qDebug() << src << "Failed to load fused image.";
        return false;
    }

    // --- Core halo suppression using broad-halo model ----------------------
    int haloWidthPx = 12;   // Adjust to match your observation (10–12 px)
    float k = 1.2f;         // Clamp factor
    float edgeThresh = 0.08f;

    cv::Mat result = removeBroadHalo(input, haloWidthPx, k, edgeThresh);

    // cv::Mat gray;
    // cv::cvtColor(input, gray, cv::COLOR_BGR2GRAY);

    // cv::Mat haloMask = detectHalos(gray);
    // saveDebug(haloMask * 255, "halo_mask.png");

    // cv::Mat result = suppressHalos(input, haloMask);

    // Annotate parameters
    const int bannerHeight = 40;
    cv::Mat annotated(result.rows + bannerHeight, result.cols, result.type(), cv::Scalar(0, 0, 0));
    result.copyTo(annotated(cv::Rect(0, 0, result.cols, result.rows)));

    // Dynamic font scaling to fill banner height
    QString text = QString("radius=%1   strength=%2")
                       .arg(radius)
                       .arg(strength, 0, 'f', 2);

    int baseline = 0;
    int thickness = 2;
    double fontScale = bannerHeight / 25.0;  // empirically fills ~80–90% of banner
    cv::Size textSize = cv::getTextSize(text.toStdString(),
                                        cv::FONT_HERSHEY_SIMPLEX,
                                        fontScale,
                                        thickness,
                                        &baseline);

    // Center vertically in banner, with left margin
    int x = 20;
    int y = result.rows + (bannerHeight + textSize.height) / 2 - baseline;

    cv::putText(annotated,
                text.toStdString(),
                cv::Point(x, y),
                cv::FONT_HERSHEY_SIMPLEX,
                fontScale,
                cv::Scalar(255, 255, 255),
                thickness,
                cv::LINE_AA);

    // Generate unique output filename
    QString baseName = QFileInfo(outputName).completeBaseName(); // usually "halo_reduced"
    QString ext = QFileInfo(outputName).suffix();
    if (ext.isEmpty()) ext = "png";

    int idx = 1;
    QString outPath;
    do {
        outPath = haloDir.filePath(QString("%1_%2.%3").arg(baseName).arg(idx).arg(ext));
        idx++;
    } while (QFile::exists(outPath));

    // Save result
    qDebug() << src << "output to" << outPath;
    if (!cv::imwrite(outPath.toStdString(), annotated)) {
        emit updateStatus(false, QString("Failed to save halo-reduced image: %1").arg(outPath), src);
        qDebug() << src << "Failed to save " << outPath;
        return false;
    }

    emit updateStatus(false, QString("Saved halo-reduced image to %1").arg(outPath), src);
    return true;
}

// --- New core: broad-halo removal by edge-aware detail clamping ------------
cv::Mat FocusHalo::removeBroadHalo(const cv::Mat& bgr,
                        int haloWidthPx,   // ~10–12 for your case
                        float k,        // clamp > k*local_sigma
                        float edgeThresh) // edge gate in [0..1]
{
    CV_Assert(bgr.type() == CV_8UC3);

    // 1) Convert to float and luminance (linear-ish)
    cv::Mat f32; bgr.convertTo(f32, CV_32F, 1.0/255.0);
    cv::Mat bgrLin; cv::pow(cv::max(f32, 0), 2.2, bgrLin); // simple gamma to linear
    std::vector<cv::Mat> ch; cv::split(bgrLin, ch);
    cv::Mat L = 0.27f*ch[2] + 0.67f*ch[1] + 0.06f*ch[0];

    // 2) Base/detail at the halo scale
    double sigma = std::max(1.0, haloWidthPx/2.0);         // e.g. ~6 for 12 px halo
    int ksize = int(std::ceil(sigma*6.0)) | 1;             // odd kernel
    cv::Mat base; cv::GaussianBlur(L, base, cv::Size(ksize, ksize), sigma, sigma);
    cv::Mat detail = L - base;

    // 3) Edge magnitude (gate). Normalize to [0..1] and blur to halo scale
    cv::Mat gx, gy, mag;
    cv::Sobel(base, gx, CV_32F, 1, 0, 3);
    cv::Sobel(base, gy, CV_32F, 0, 1, 3);
    cv::magnitude(gx, gy, mag);
    double mmin, mmax; cv::minMaxLoc(mag, &mmin, &mmax);
    if (mmax > 0) mag /= (float)mmax;
    cv::GaussianBlur(mag, mag, cv::Size(ksize, ksize), sigma, sigma); // widen to ~halo

    // 4) Local detail sigma (robust): E[detail^2]^(1/2) in halo window
    cv::Mat d2; cv::multiply(detail, detail, d2);
    cv::Mat d2mean; cv::boxFilter(d2, d2mean, -1, cv::Size(ksize, ksize), cv::Point(-1,-1), true, cv::BORDER_REPLICATE);
    cv::Mat dSigma; cv::sqrt(cv::max(d2mean, 1e-8f), dSigma);

    // 5) Edge-aware asymmetric clamp for positive overshoot
    //    gate = smooth step of mag vs threshold
    cv::Mat gate;  // [0..1]
    {
        // smoothstep: t = saturate((mag - T0)/(T1-T0))
        float T0 = edgeThresh * 0.6f;
        float T1 = edgeThresh * 1.6f;
        gate = (mag - T0) / std::max(1e-6f, (T1 - T0));
        cv::min(cv::max(gate, 0), 1, gate);
        // ease curve (optional)
        cv::multiply(gate, gate, gate); // t^2
    }

    cv::Mat posMask = (detail > k * dSigma); // where bright lobe is too big
    cv::Mat clampTarget = k * dSigma;

    // detail_clamped = where(posMask)  mix(detail, clampTarget, gate)
    cv::Mat detailClamped = detail.clone();
    // Apply only on positives and near edges
    for (int y = 0; y < detail.rows; ++y) {
        float* det = detail.ptr<float>(y);
        float* dcl = detailClamped.ptr<float>(y);
        const float* tgt = clampTarget.ptr<float>(y);
        const float* gt  = gate.ptr<float>(y);
        const uchar* pm  = posMask.ptr<uchar>(y);
        for (int x = 0; x < detail.cols; ++x) {
            if (pm[x]) {
                float t = gt[x]; // 0..1 blend
                dcl[x] = (1.0f - t) * det[x] + t * tgt[x];
            }
        }
    }

    // Optional: do the symmetric for negative halos by replacing (detail < -k*sigma)

    // 6) Reconstruct luminance and re-apply chroma
    cv::Mat Lout = base + detailClamped;
    Lout = cv::max(Lout, 0);
    // scale RGB channels by Lout/L (preserve hue), safe divide
    cv::Mat ratio;
    cv::divide(Lout, cv::max(L, 1e-6f), ratio);
    std::vector<cv::Mat> chOut(3);
    for (int c = 0; c < 3; ++c)
        chOut[c] = ch[c].mul(ratio);
    cv::Mat bgrLinOut;
    cv::merge(chOut, bgrLinOut);

    // back to sRGB-ish
    cv::Mat bgrOut; cv::pow(cv::max(bgrLinOut, 0), 1.0/2.2, bgrOut);
    cv::Mat u8; bgrOut.convertTo(u8, CV_8U, 255.0);
    return u8;
}

cv::Mat FocusHalo::detectHalos(const cv::Mat &gray)
{
    cv::Mat fgray;
    gray.convertTo(fgray, CV_32F, 1.0 / 255.0);

    // --- 1. Wide-band Difference of Gaussians (for ~10–12 px halos) -------
    cv::Mat blurSmall, blurLarge;
    cv::GaussianBlur(fgray, blurSmall, cv::Size(7,7), 1.5);
    cv::GaussianBlur(fgray, blurLarge, cv::Size(25,25), 6.0);
    cv::Mat dog = blurSmall - blurLarge;   // bright side = positive values

    // --- 2. Normalize and threshold (lower = more sensitive) --------------
    cv::normalize(dog, dog, 0.0, 1.0, cv::NORM_MINMAX);
    cv::Mat haloMask;
    cv::threshold(dog, haloMask, 0.05, 1.0, cv::THRESH_BINARY);

    // --- 3. Smooth and dilate to match observed 10–12 px width ------------
    cv::GaussianBlur(haloMask, haloMask, cv::Size(9,9), 2.0);
    cv::dilate(haloMask, haloMask,
               cv::getStructuringElement(cv::MORPH_ELLIPSE, cv::Size(7,7)),
               cv::Point(-1,-1), 1);

    // Optional debug save
    saveDebug(haloMask * 255, "halo_mask.png");

    return haloMask;
}

/*
cv::Mat FocusHalo::detectHalos(const cv::Mat &gray)
{
    cv::Mat blur, diff, haloMask;

    // Edge-preserving smoothing and difference
    cv::bilateralFilter(gray, blur, radius, radius*4, radius);
    cv::absdiff(gray, blur, diff);

    diff.convertTo(diff, CV_32F, 1.0 / 255.0);
    cv::normalize(diff, diff, 0.0, 1.0, cv::NORM_MINMAX);

    // Threshold: isolate bright overshoots
    cv::threshold(diff, haloMask, 0.08, 1.0, cv::THRESH_BINARY);
    cv::dilate(haloMask, haloMask, cv::Mat(), cv::Point(-1, -1), 1);

    return haloMask;
}
*/

cv::Mat FocusHalo::suppressHalos(const cv::Mat &srcBgr, const cv::Mat &haloMask)
{
    cv::Mat smooth, maskF, result;
    cv::bilateralFilter(srcBgr, smooth, radius, radius*4, radius);
    haloMask.convertTo(maskF, CV_32F);

    result = srcBgr.clone();
    result.convertTo(result, CV_32F);
    smooth.convertTo(smooth, CV_32F);

    for (int y = 0; y < srcBgr.rows; ++y) {
        for (int x = 0; x < srcBgr.cols; ++x) {
            float m = maskF.at<float>(y, x) * strength;
            cv::Vec3f srcPx = srcBgr.at<cv::Vec3b>(y, x);
            cv::Vec3f smPx = smooth.at<cv::Vec3f>(y, x);
            result.at<cv::Vec3f>(y, x) = (1.0f - m) * srcPx + m * smPx;
        }
    }

    result.convertTo(result, CV_8U);
    return result;
}

void FocusHalo::saveDebug(const cv::Mat &mat, const QString &name)
{
    if (!haloDir.exists())
        haloDir.mkpath(".");

    QString path = haloDir.filePath(name);
    cv::imwrite(path.toStdString(), mat);
}
