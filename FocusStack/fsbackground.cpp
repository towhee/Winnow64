#include "FSBackground.h"
#include "FocusStack/fsutilities.h"
#include "Main/global.h"

#include <opencv2/imgproc.hpp>
#include <opencv2/imgcodecs.hpp>

#include <QDir>
#include <QDebug>
#include <numeric>
#include <cmath>

namespace FSBackground {

static inline bool abortRequested(std::atomic_bool *a)
{
    return a && a->load(std::memory_order_relaxed);
}

static inline int oddK(int k) { return (k % 2 == 1) ? k : (k + 1); }

static void clamp01InPlace(cv::Mat &m)
{
    if (m.empty()) return;
    cv::min(m, 1.0f, m);
    cv::max(m, 0.0f, m);
}

static cv::Mat toFloat01_Gray(const cv::Mat &img)
{
    if (img.empty()) return {};
    cv::Mat g;

    if (img.channels() == 1) g = img;
    else cv::cvtColor(img, g, cv::COLOR_BGR2GRAY);

    if (g.type() == CV_32F) return g;

    cv::Mat f;
    if (g.depth() == CV_8U) g.convertTo(f, CV_32F, 1.0 / 255.0);
    else g.convertTo(f, CV_32F);
    return f;
}

static cv::Mat depthNorm01FromIndex16(const cv::Mat &depthIndex16, int sliceCount)
{
    CV_Assert(depthIndex16.type() == CV_16U);
    CV_Assert(sliceCount >= 2);

    cv::Mat out(depthIndex16.size(), CV_32F);

    const float denom = float(sliceCount - 1);
    for (int y = 0; y < depthIndex16.rows; ++y)
    {
        const uint16_t *d = depthIndex16.ptr<uint16_t>(y);
        float *o = out.ptr<float>(y);
        for (int x = 0; x < depthIndex16.cols; ++x)
            o[x] = float(d[x]) / denom;
    }
    return out;
}

static cv::Mat localStdDevDepthIndex(const cv::Mat &depthIndex16, int radius)
{
    CV_Assert(depthIndex16.type() == CV_16U);
    radius = std::max(1, radius);
    const int k = 2 * radius + 1;

    // compute mean and mean2 using box filter on float
    cv::Mat d32;
    depthIndex16.convertTo(d32, CV_32F);

    cv::Mat mean, mean2;
    cv::boxFilter(d32, mean,  CV_32F, cv::Size(k,k), cv::Point(-1,-1), true, cv::BORDER_REFLECT);
    cv::boxFilter(d32.mul(d32), mean2, CV_32F, cv::Size(k,k), cv::Point(-1,-1), true, cv::BORDER_REFLECT);

    cv::Mat var = mean2 - mean.mul(mean);
    cv::max(var, 0.0f, var);
    cv::Mat stdv;
    cv::sqrt(var, stdv);
    return stdv; // "slice index units"
}

static cv::Mat computeFocusMax(const std::vector<cv::Mat> &focusSlices32,
                               std::atomic_bool *abortFlag,
                               ProgressCallback progressCb)
{
    CV_Assert(!focusSlices32.empty());
    cv::Mat maxF = cv::Mat::zeros(focusSlices32.front().size(), CV_32F);

    const int N = (int)focusSlices32.size();
    for (int i = 0; i < N; ++i)
    {
        if (abortRequested(abortFlag)) return {};
        CV_Assert(focusSlices32[i].type() == CV_32F);
        CV_Assert(focusSlices32[i].size() == maxF.size());

        cv::max(maxF, focusSlices32[i], maxF);
        if (progressCb) progressCb(i + 1, N);
    }
    return maxF;
}

static cv::Mat computeFocusSupportRatio(const std::vector<cv::Mat> &focusSlices32,
                                        float supportThresh,
                                        std::atomic_bool *abortFlag,
                                        ProgressCallback progressCb)
{
    CV_Assert(!focusSlices32.empty());
    const cv::Size sz = focusSlices32.front().size();
    cv::Mat count = cv::Mat::zeros(sz, CV_32S);

    const int N = (int)focusSlices32.size();
    for (int i = 0; i < N; ++i)
    {
        if (abortRequested(abortFlag)) return {};
        CV_Assert(focusSlices32[i].type() == CV_32F);

        cv::Mat m = (focusSlices32[i] > supportThresh); // CV_8U
        count += m / 255; // adds 1 where true

        if (progressCb) progressCb(i + 1, N);
    }

    cv::Mat ratio(sz, CV_32F);
    count.convertTo(ratio, CV_32F, 1.0 / float(N));
    return ratio;
}

static cv::Mat computeColorVariance01(const std::vector<cv::Mat> &alignedColor,
                                      std::atomic_bool *abortFlag,
                                      ProgressCallback progressCb)
{
    // Very simple: per-pixel variance of grayscale across slices.
    // alignedColor: CV_8UC3 or CV_32FC3
    CV_Assert(!alignedColor.empty());
    const int N = (int)alignedColor.size();
    const cv::Size sz = alignedColor.front().size();

    cv::Mat mean = cv::Mat::zeros(sz, CV_32F);
    cv::Mat mean2 = cv::Mat::zeros(sz, CV_32F);

    for (int i = 0; i < N; ++i)
    {
        if (abortRequested(abortFlag)) return {};
        CV_Assert(alignedColor[i].size() == sz);

        cv::Mat g = toFloat01_Gray(alignedColor[i]); // 0..1 float
        mean  += g;
        mean2 += g.mul(g);

        if (progressCb) progressCb(i + 1, N);
    }

    mean  *= (1.0f / float(N));
    mean2 *= (1.0f / float(N));

    cv::Mat var = mean2 - mean.mul(mean);
    cv::max(var, 0.0f, var);
    return var; // 0.. ~
}

static void cleanupMask(cv::Mat &mask8, const Options &opt)
{
    // mask8: 0/255 (subject)
    if (mask8.empty()) return;
    CV_Assert(mask8.type() == CV_8U);

    if (opt.medianK >= 3)
        cv::medianBlur(mask8, mask8, oddK(opt.medianK));

    if (opt.morphRadius > 0)
    {
        int r = opt.morphRadius;
        cv::Mat se = cv::getStructuringElement(cv::MORPH_ELLIPSE,
                                               cv::Size(2*r+1, 2*r+1));
        cv::morphologyEx(mask8, mask8, cv::MORPH_CLOSE, se);
        cv::morphologyEx(mask8, mask8, cv::MORPH_OPEN,  se);
    }

    // Remove small islands (keep big connected components)
    if (opt.removeIslandsMinArea > 0)
    {
        cv::Mat labels, stats, centroids;
        int n = cv::connectedComponentsWithStats(mask8 > 0, labels, stats, centroids, 8, CV_32S);

        // 0 is background
        cv::Mat cleaned = cv::Mat::zeros(mask8.size(), CV_8U);
        for (int i = 1; i < n; ++i)
        {
            int area = stats.at<int>(i, cv::CC_STAT_AREA);
            if (area >= opt.removeIslandsMinArea)
            {
                cleaned.setTo(255, labels == i);
            }
        }
        mask8 = cleaned;
    }
}

static cv::Mat makeFeatherWeight01(const cv::Mat &subjectMask8,
                                   const Options &opt)
{
    // weight01 = 1.0 inside subject, 0.0 in background, feathered at boundary
    CV_Assert(subjectMask8.type() == CV_8U);

    cv::Mat subj = (subjectMask8 > 0);
    cv::Mat distIn, distOut;

    // distance to nearest background from inside
    cv::distanceTransform(subj, distIn, cv::DIST_L2, 3);
    // distance to nearest subject from outside
    cv::distanceTransform(255 - subj, distOut, cv::DIST_L2, 3);

    // boundary band weight: within featherRadius of boundary blend
    const float R = float(std::max(1, opt.featherRadius));

    cv::Mat w = cv::Mat::zeros(subjectMask8.size(), CV_32F);
    for (int y = 0; y < w.rows; ++y)
    {
        const float *din  = distIn.ptr<float>(y);
        const float *dout = distOut.ptr<float>(y);
        float *wp = w.ptr<float>(y);

        for (int x = 0; x < w.cols; ++x)
        {
            if (subj.at<uchar>(y,x))
            {
                float t = std::min(1.0f, din[x] / R);
                wp[x] = std::pow(t, opt.featherGamma);
            }
            else
            {
                wp[x] = 0.0f;
            }
        }
    }
    clamp01InPlace(w);
    return w;
}

cv::Mat makeOverlayBGR(const cv::Mat &baseGrayOrBGR,
                       const cv::Mat &bgConfidence01,
                       const Options &opt)
{
    QString srcFun = "FSBackground::makeOverlayBGR";
    CV_Assert(bgConfidence01.type() == CV_32F);
    cv::Mat baseBGR;

    if (baseGrayOrBGR.channels() == 1)
    {
        cv::Mat g8;
        if (baseGrayOrBGR.type() == CV_8U) g8 = baseGrayOrBGR;
        else {
            cv::Mat f = baseGrayOrBGR;
            cv::Mat f01 = f.clone();
            clamp01InPlace(f01);
            f01.convertTo(g8, CV_8U, 255.0);
        }
        cv::cvtColor(g8, baseBGR, cv::COLOR_GRAY2BGR);
    }
    else
    {
        if (baseGrayOrBGR.type() == CV_8UC3) baseBGR = baseGrayOrBGR.clone();
        else {
            // assume float 0..1
            cv::Mat b8;
            cv::Mat f = baseGrayOrBGR;
            cv::Mat f01 = f.clone();
            cv::min(f01, 1.0f, f01);
            cv::max(f01, 0.0f, f01);
            f01.convertTo(b8, CV_8U, 255.0);
            baseBGR = b8;
        }
    }

    cv::Mat conf = bgConfidence01.clone();
    if (conf.size() != baseBGR.size())
    {
        qWarning() << srcFun
                   << ": resizing bgConfidence from"
                   << conf.cols << "x" << conf.rows
                   << "to"
                   << baseBGR.cols << "x" << baseBGR.rows;

        cv::resize(conf, conf, baseBGR.size(), 0, 0, cv::INTER_LINEAR);
    }
    clamp01InPlace(conf);

    // Heat color for confidence (yellow->orange->red)
    cv::Mat heat(baseBGR.size(), CV_8UC3, cv::Scalar(0,0,0));
    for (int y = 0; y < conf.rows; ++y)
    {
        const float *c = conf.ptr<float>(y);
        cv::Vec3b *h = heat.ptr<cv::Vec3b>(y);
        for (int x = 0; x < conf.cols; ++x)
        {
            float v = c[x];
            if (v < 0.02f)      h[x] = {0,0,0};
            else if (v < 0.08f) h[x] = {0,255,255};   // yellow
            else if (v < 0.18f) h[x] = {0,165,255};   // orange
            else                h[x] = {0,0,255};     // red
        }
    }

    cv::Mat mask = (conf >= opt.overlayThresh);

    cv::Mat out = baseBGR.clone();
    // Blend only where mask is on
    cv::Mat blend;
    cv::addWeighted(baseBGR, 1.0 - opt.overlayAlpha, heat, opt.overlayAlpha, 0.0, blend);
    blend.copyTo(out, mask);  // crash

    return out;
}

bool run(const cv::Mat                 &depthIndex16,
         const std::vector<cv::Mat>    &focusSlices32,
         const std::vector<cv::Mat>    &alignedColor,
         int                            sliceCount,
         const Options                 &opt,
         std::atomic_bool              *abortFlag,
         ProgressCallback               progressCb,
         StatusCallback                 statusCb,
         cv::Mat                       &bgConfidence01Out,
         cv::Mat                       &subjectMask8Out)
{
    const QString srcFun = "FSBackground::run";
    const QString m = opt.method.trimmed();

    if (statusCb) statusCb("FSBackground: Building background mask...");

    const bool needDepth  = m.contains("Depth", Qt::CaseInsensitive);
    const bool needFocus  = m.contains("Focus", Qt::CaseInsensitive);
    const bool needColorV = m.contains("ColorVar", Qt::CaseInsensitive) || opt.enableColorVar;

    // --- Validate required inputs ---
    if (needDepth)
    {
        if (depthIndex16.empty() || depthIndex16.type() != CV_16U)
        {
            if (statusCb) statusCb(srcFun + ": Depth requested but depthIndex16 invalid.");
            return false;
        }
        if (sliceCount < 2)
        {
            if (statusCb) statusCb(srcFun + ": sliceCount must be >= 2.");
            return false;
        }
    }

    if (needFocus && focusSlices32.empty())
    {
        if (statusCb) statusCb(srcFun + ": Focus requested but focusSlices32 is empty.");
        return false;
    }

    if (needColorV && alignedColor.empty())
    {
        if (statusCb) statusCb(srcFun + ": ColorVar requested but alignedColor is empty.");
        return false;
    }

    // --- Establish canonical size (prefer focus if used, else depth) ---
    cv::Size canonicalSize;
    if (needFocus)      canonicalSize = focusSlices32.front().size();
    else if (needDepth) canonicalSize = depthIndex16.size();
    else
    {
        if (statusCb) statusCb(srcFun + ": Method requires neither Depth nor Focus.");
        return false;
    }

    // --- Canonicalize inputs to canonicalSize ---
    cv::Mat depth16C;
    if (needDepth) {
        depth16C = FSUtilities::canonicalizeDepthIndex16(depthIndex16, canonicalSize, "depthIndex16");
    }

    std::vector<cv::Mat> focusC;
    if (needFocus)
    {
        if (statusCb) statusCb("FSBackground: Canonicalizing focus slices...");
        focusC = FSUtilities::canonicalizeFocusSlices(focusSlices32, canonicalSize, abortFlag);
        if (focusC.empty()) return false;
    }

    std::vector<cv::Mat> colorC;
    if (needColorV)
    {
        if (statusCb) statusCb("FSBackground: Canonicalizing color slices...");
        colorC = FSUtilities::canonicalizeAlignedColor(alignedColor, canonicalSize, abortFlag);
        if (colorC.empty()) return false;
    }

    // --- From this point on: EVERYTHING uses canonical mats only ---
    cv::Mat bg01 = cv::Mat::zeros(canonicalSize, CV_32F);

    // 1) Depth prior (farther depth => more background)
    if (needDepth)
    {
        if (statusCb) statusCb("FSBackground: Depth prior...");

        cv::Mat d01 = depthNorm01FromIndex16(depth16C, sliceCount); // CV_32F canonical

        cv::Mat depthPrior = cv::Mat::zeros(canonicalSize, CV_32F);
        const float a = opt.farDepthStart01;
        const float b = std::max(a + 1e-6f, opt.farDepthStrong01);

        for (int y = 0; y < d01.rows; ++y)
        {
            const float *dp = d01.ptr<float>(y);
            float *op = depthPrior.ptr<float>(y);
            for (int x = 0; x < d01.cols; ++x)
            {
                float t = (dp[x] - a) / (b - a);
                t = std::min(1.0f, std::max(0.0f, t));
                op[x] = t;
            }
        }

        if (statusCb) statusCb("FSBackground: Depth instability...");
        cv::Mat stdv = localStdDevDepthIndex(depth16C, /*radius=*/2); // canonical
        cv::Mat unstable = (stdv > opt.depthUnstableStd);            // CV_8U
        cv::Mat unstableF;
        unstable.convertTo(unstableF, CV_32F, 1.0 / 255.0);          // CV_32F canonical

        cv::Mat depthCombined = 0.70f * depthPrior + 0.30f * unstableF; // CV_32F canonical
        bg01 = cv::max(bg01, depthCombined);
    }

    // 2) Focus weakness / support
    if (needFocus)
    {
        if (statusCb) statusCb("FSBackground: Focus support...");

        cv::Mat maxF = computeFocusMax(focusC, abortFlag, progressCb); // canonical
        if (maxF.empty()) return false;

        cv::Mat focusBg(canonicalSize, CV_32F);

        const float lo = opt.focusLowThresh;
        const float hi = std::max(lo + 1e-6f, opt.focusHighThresh);

        for (int y = 0; y < maxF.rows; ++y)
        {
            const float *fp = maxF.ptr<float>(y);
            float *bp = focusBg.ptr<float>(y);
            for (int x = 0; x < maxF.cols; ++x)
            {
                float t = (fp[x] - lo) / (hi - lo);
                t = std::min(1.0f, std::max(0.0f, t));
                bp[x] = 1.0f - t;
            }
        }

        cv::Mat ratio = computeFocusSupportRatio(focusC, opt.supportFocusThresh, abortFlag, nullptr); // canonical
        if (ratio.empty()) return false;

        cv::Mat ratioBg(canonicalSize, CV_32F);
        const float r0 = opt.minSupportRatio;

        for (int y = 0; y < ratio.rows; ++y)
        {
            const float *rp = ratio.ptr<float>(y);
            float *bp = ratioBg.ptr<float>(y);
            for (int x = 0; x < ratio.cols; ++x)
            {
                float t = (rp[x] / std::max(1e-6f, r0));
                t = std::min(1.0f, std::max(0.0f, t));
                bp[x] = 1.0f - t;
            }
        }

        cv::Mat focusCombined = 0.65f * focusBg + 0.35f * ratioBg; // canonical
        bg01 = cv::max(bg01, focusCombined);
    }

    // 3) Optional color variance
    if (needColorV)
    {
        if (statusCb) statusCb("FSBackground: Color variance...");
        cv::Mat var = computeColorVariance01(colorC, abortFlag, nullptr); // canonical
        if (var.empty()) return false;

        cv::Mat varBg(canonicalSize, CV_32F);
        const float t0 = opt.colorVarThresh;
        const float t1 = std::max(t0 + 1e-6f, t0 * 3.0f);

        for (int y = 0; y < var.rows; ++y)
        {
            const float *vp = var.ptr<float>(y);
            float *bp = varBg.ptr<float>(y);
            for (int x = 0; x < var.cols; ++x)
            {
                float t = (vp[x] - t0) / (t1 - t0);
                t = std::min(1.0f, std::max(0.0f, t));
                bp[x] = t;
            }
        }

        bg01 = cv::max(bg01, varBg);
    }

    clamp01InPlace(bg01);

    // Subject mask
    cv::Mat subject8 = (bg01 < 0.50f);
    subject8 *= 255;
    cleanupMask(subject8, opt);

    bgConfidence01Out = bg01;
    subjectMask8Out   = subject8;
    qDebug() << srcFun << "7";

    if (opt.writeDebug && !opt.debugFolder.isEmpty())
    {
        QDir().mkpath(opt.debugFolder);

        cv::Mat conf8;
        bg01.convertTo(conf8, CV_8U, 255.0);
        cv::imwrite((opt.debugFolder + "/bg_confidence.png").toStdString(), conf8);
        cv::imwrite((opt.debugFolder + "/subject_mask.png").toStdString(), subject8);
    }

    if (statusCb) statusCb("FSBackground: Background mask complete.");
    return true;
}

bool replaceBackground(cv::Mat          &fusedInOut,
                       const cv::Mat    &subjectMask8,
                       const cv::Mat    &bgSource,
                       const Options    &opt,
                       std::atomic_bool *abortFlag)
{
    if (fusedInOut.empty() || bgSource.empty() || subjectMask8.empty())
        return false;

    CV_Assert(fusedInOut.size() == bgSource.size());
    CV_Assert(fusedInOut.type() == bgSource.type());
    CV_Assert(subjectMask8.type() == CV_8U);
    CV_Assert(subjectMask8.size() == fusedInOut.size());

    // Feather weights
    cv::Mat w = makeFeatherWeight01(subjectMask8, opt); // 1 subject, 0 bg
    if (w.empty()) return false;

    const int rows = fusedInOut.rows;
    const int cols = fusedInOut.cols;
    const int ch   = fusedInOut.channels();

    // Blend: out = w*fused + (1-w)*bg
    if (fusedInOut.depth() == CV_8U)
    {
        for (int y = 0; y < rows; ++y)
        {
            if (abortRequested(abortFlag)) return false;

            const float *wp = w.ptr<float>(y);
            const uchar *bp = bgSource.ptr<uchar>(y);
            uchar *op       = fusedInOut.ptr<uchar>(y);

            for (int x = 0; x < cols; ++x)
            {
                float a = wp[x];
                float b = 1.0f - a;
                int idx = x * ch;
                for (int c = 0; c < ch; ++c)
                {
                    float vf = float(op[idx + c]);
                    float vb = float(bp[idx + c]);
                    float vo = a * vf + b * vb;
                    op[idx + c] = (uchar)std::min(255.0f, std::max(0.0f, vo));
                }
            }
        }
    }
    else if (fusedInOut.depth() == CV_32F)
    {
        for (int y = 0; y < rows; ++y)
        {
            if (abortRequested(abortFlag)) return false;

            const float *wp = w.ptr<float>(y);
            const float *bp = bgSource.ptr<float>(y);
            float *op       = fusedInOut.ptr<float>(y);

            for (int x = 0; x < cols; ++x)
            {
                float a = wp[x];
                float b = 1.0f - a;
                int idx = x * ch;
                for (int c = 0; c < ch; ++c)
                {
                    float vf = op[idx + c];
                    float vb = bp[idx + c];
                    op[idx + c] = a * vf + b * vb;
                }
            }
        }
    }
    else
    {
        // For other depths, convert before calling
        return false;
    }

    return true;
}

} // namespace FSBackground
