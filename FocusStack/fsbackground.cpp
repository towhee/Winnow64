#include "FocusStack/fsbackground.h"
#include "FocusStack/FSUtilities.h"

#include <QDir>
#include <QFileInfo>
#include <QDebug>

// Workaround for occasional Qt trait instantiation issues if some TU pulls QModelIndex
#include <QModelIndex>

#include <opencv2/imgproc.hpp>
#include <opencv2/imgcodecs.hpp>

#include <deque>
#include <algorithm>
#include <cmath>

namespace FSBackground {

// ============================================================
// Small helpers
// ============================================================

static inline bool aborted(std::atomic_bool* f)
{
    return f && f->load(std::memory_order_relaxed);
}

static void ensureDebugFolder(const QString& folder)
{
    if (folder.isEmpty()) return;
    QDir d(folder);
    if (!d.exists())
        QDir().mkpath(folder);
}

static void clamp01InPlace(cv::Mat& m32)
{
    CV_Assert(m32.type() == CV_32F);
    cv::max(m32, 0.0f, m32);
    cv::min(m32, 1.0f, m32);
}

// Sample-based quantile (fast, robust; avoids full sort)
static float quantile32f_sampled(const cv::Mat& m32, float q)
{
    CV_Assert(m32.type() == CV_32F);
    CV_Assert(q >= 0.0f && q <= 1.0f);

    const int rows = m32.rows, cols = m32.cols;
    const int total = rows * cols;
    if (total <= 0) return 0.0f;

    // Aim ~200k samples max
    const int maxSamples = 200000;
    int step = 1;
    if (total > maxSamples) step = (total / maxSamples);

    std::vector<float> v;
    v.reserve(std::min(total, maxSamples) + 16);

    // stride scan
    int idx = 0;
    for (int y = 0; y < rows; ++y)
    {
        const float* row = m32.ptr<float>(y);
        for (int x = 0; x < cols; ++x, ++idx)
        {
            if ((idx % step) != 0) continue;
            float f = row[x];
            if (std::isfinite(f))
                v.push_back(f);
        }
    }

    if (v.empty()) return 0.0f;

    const size_t k = static_cast<size_t>(std::clamp(q, 0.0f, 1.0f) * float(v.size() - 1));
    std::nth_element(v.begin(), v.begin() + k, v.end());
    return v[k];
}

static cv::Mat normalizeRobust01(const cv::Mat& m32, float qLo, float qHi)
{
    CV_Assert(m32.type() == CV_32F);
    const float lo = quantile32f_sampled(m32, qLo);
    const float hi = quantile32f_sampled(m32, qHi);

    if (!(hi > lo + 1e-12f))
    {
        // Degenerate: return zeros
        return cv::Mat(m32.size(), CV_32F, cv::Scalar(0.0f));
    }

    cv::Mat out;
    out = (m32 - lo) * (1.0f / (hi - lo));
    clamp01InPlace(out);
    return out;
}

// Depth index (0..N-1) -> float depth 0..1
static cv::Mat depthNorm01FromIndex16(const cv::Mat& depth16, int sliceCount)
{
    CV_Assert(depth16.type() == CV_16U);
    CV_Assert(sliceCount >= 2);

    const float denom = float(std::max(1, sliceCount - 1));
    cv::Mat d32;
    depth16.convertTo(d32, CV_32F, 1.0f / denom);
    clamp01InPlace(d32);
    return d32;
}

// Local stddev on depth indices in "slice units"
static cv::Mat localStdDevDepthIndex(const cv::Mat& depth16, int radius)
{
    CV_Assert(depth16.type() == CV_16U);
    CV_Assert(radius >= 1);

    cv::Mat f;
    depth16.convertTo(f, CV_32F);

    const int k = 2 * radius + 1;

    cv::Mat mean, meanSq;
    cv::boxFilter(f, mean, CV_32F, cv::Size(k, k), cv::Point(-1,-1), true, cv::BORDER_REFLECT);
    cv::boxFilter(f.mul(f), meanSq, CV_32F, cv::Size(k, k), cv::Point(-1,-1), true, cv::BORDER_REFLECT);

    cv::Mat var = meanSq - mean.mul(mean);
    cv::max(var, 0.0f, var);

    cv::Mat stdv;
    cv::sqrt(var, stdv);
    return stdv; // in slice-index units
}

// Focus normalization per-slice by p99 (scale-invariant)
// Returns normalized slices (approx 0..1+), and also writes a "scale map" if requested
static std::vector<cv::Mat> normalizeFocusSlicesP99(const std::vector<cv::Mat>& focusSlices32,
                                                    std::atomic_bool* abortFlag,
                                                    std::vector<float>* perSliceP99Out = nullptr)
{
    std::vector<cv::Mat> out;
    out.reserve(focusSlices32.size());

    if (perSliceP99Out) {
        perSliceP99Out->clear();
        perSliceP99Out->reserve(focusSlices32.size());
    }

    for (size_t i = 0; i < focusSlices32.size(); ++i)
    {
        if (aborted(abortFlag)) return {};

        const cv::Mat& m = focusSlices32[i];
        CV_Assert(!m.empty());
        CV_Assert(m.type() == CV_32F);

        // Robust scale = p99 (avoid crazy max spikes)
        float p99 = quantile32f_sampled(m, 0.99f);
        if (!std::isfinite(p99) || p99 <= 1e-12f) p99 = 1.0f;

        if (perSliceP99Out) perSliceP99Out->push_back(p99);

        cv::Mat n = m * (1.0f / p99);
        // do not hard clamp yet; keep >1 for strong peaks, we clamp later when needed
        out.push_back(n);
    }
    return out;
}

// Compute per-pixel max focus across slices (expects CV_32F slices, same size)
static cv::Mat computeFocusMax(const std::vector<cv::Mat>& focusSlices32,
                               std::atomic_bool* abortFlag,
                               ProgressCallback progressCb)
{
    if (focusSlices32.empty()) return cv::Mat();
    const cv::Size sz = focusSlices32.front().size();

    cv::Mat maxF(sz, CV_32F, cv::Scalar(0.0f));

    for (size_t i = 0; i < focusSlices32.size(); ++i)
    {
        if (aborted(abortFlag)) return cv::Mat();
        const cv::Mat& f = focusSlices32[i];
        CV_Assert(f.type() == CV_32F && f.size() == sz);

        cv::max(maxF, f, maxF);

        if (progressCb) progressCb(int(i + 1), int(focusSlices32.size()));
    }
    return maxF;
}

// Scale-invariant support ratio.
// support if sliceFocus >= max( supportAbs , supportRel * maxF )
static cv::Mat computeFocusSupportRatio_ScaleInvariant(const std::vector<cv::Mat>& focusSlicesN,
                                                       const cv::Mat& maxF,
                                                       float supportAbs,
                                                       float supportRel,
                                                       std::atomic_bool* abortFlag)
{
    CV_Assert(maxF.type() == CV_32F);
    const cv::Size sz = maxF.size();
    const int N = int(focusSlicesN.size());
    CV_Assert(N > 0);

    cv::Mat count(sz, CV_32F, cv::Scalar(0.0f));

    // precompute threshold map thr = max(supportAbs, supportRel*maxF)
    cv::Mat thr = supportRel * maxF;
    cv::max(thr, supportAbs, thr);

    for (int i = 0; i < N; ++i)
    {
        if (aborted(abortFlag)) return cv::Mat();
        const cv::Mat& f = focusSlicesN[i];
        CV_Assert(f.type() == CV_32F && f.size() == sz);

        cv::Mat m;
        cv::compare(f, thr, m, cv::CMP_GT);   // 0/255
        m.convertTo(m, CV_32F, 1.0 / 255.0);  // 0/1
        count += m;
    }

    cv::Mat ratio = count * (1.0f / float(N));
    clamp01InPlace(ratio);
    return ratio;
}

// Optional: color variance across slices (in 0..1-ish)
static cv::Mat computeColorVariance01(const std::vector<cv::Mat>& colorSlices,
                                      std::atomic_bool* abortFlag)
{
    if (colorSlices.empty()) return cv::Mat();
    const cv::Size sz = colorSlices.front().size();

    // Convert to float 0..1 BGR
    auto toFloat01 = [&](const cv::Mat& in) -> cv::Mat {
        CV_Assert(!in.empty() && in.size() == sz);
        cv::Mat f;
        if (in.type() == CV_8UC3) in.convertTo(f, CV_32F, 1.0/255.0);
        else if (in.type() == CV_32FC3) f = in;
        else {
            cv::Mat tmp;
            in.convertTo(tmp, CV_8UC3);
            tmp.convertTo(f, CV_32F, 1.0/255.0);
        }
        return f;
    };

    cv::Mat mean(sz, CV_32FC3, cv::Scalar(0,0,0));
    const int N = int(colorSlices.size());

    std::vector<cv::Mat> fSlices;
    fSlices.reserve(N);

    for (int i = 0; i < N; ++i)
    {
        if (aborted(abortFlag)) return cv::Mat();
        cv::Mat f = toFloat01(colorSlices[i]);
        fSlices.push_back(f);
        mean += f;
    }
    mean *= (1.0f / float(N));

    cv::Mat var(sz, CV_32FC3, cv::Scalar(0,0,0));
    for (int i = 0; i < N; ++i)
    {
        if (aborted(abortFlag)) return cv::Mat();
        cv::Mat d = fSlices[i] - mean;
        var += d.mul(d);
    }
    var *= (1.0f / float(N));

    // Reduce channels -> scalar variance
    std::vector<cv::Mat> ch(3);
    cv::split(var, ch);
    cv::Mat v = (ch[0] + ch[1] + ch[2]) * (1.0f / 3.0f);

    // v is already 0..something; robust normalize later in caller
    return v;
}

// Keep only pixels connected to border (for seed stabilization)
static cv::Mat keepBorderConnected(const cv::Mat& mask8)
{
    CV_Assert(mask8.type() == CV_8U);
    const int rows = mask8.rows, cols = mask8.cols;

    cv::Mat visited(rows, cols, CV_8U, cv::Scalar(0));
    cv::Mat out(rows, cols, CV_8U, cv::Scalar(0));
    std::deque<cv::Point> q;

    auto pushIf = [&](int x, int y){
        if ((unsigned)x >= (unsigned)cols || (unsigned)y >= (unsigned)rows) return;
        if (visited.at<uchar>(y,x)) return;
        if (mask8.at<uchar>(y,x) == 0) return;
        visited.at<uchar>(y,x) = 255;
        q.emplace_back(x,y);
    };

    // seed from all border pixels
    for (int x = 0; x < cols; ++x) {
        pushIf(x, 0);
        pushIf(x, rows-1);
    }
    for (int y = 0; y < rows; ++y) {
        pushIf(0, y);
        pushIf(cols-1, y);
    }

    // BFS flood
    const int dx[4] = {1,-1,0,0};
    const int dy[4] = {0,0,1,-1};

    while (!q.empty())
    {
        cv::Point p = q.front(); q.pop_front();
        out.at<uchar>(p.y, p.x) = 255;

        for (int k = 0; k < 4; ++k)
            pushIf(p.x + dx[k], p.y + dy[k]);
    }

    return out;
}

// Build background seeds from bgScore01 using quantile threshold
static cv::Mat buildBackgroundSeeds(const cv::Mat& bgScore01, const Options& opt)
{
    CV_Assert(bgScore01.type() == CV_32F);

    const float qSeed = (opt.seedHighQuantile > 0.0f ? opt.seedHighQuantile : 0.92f);
    const float thr   = quantile32f_sampled(bgScore01, qSeed);

    cv::Mat seed;
    cv::compare(bgScore01, thr, seed, cv::CMP_GE); // 0/255

    if (opt.preferBorderSeeds)
        seed = keepBorderConnected(seed);

    return seed;
}

// Grow from seeds into pixels above a (lower) threshold
static cv::Mat growBackgroundRegion(const cv::Mat& bgScore01,
                                    const cv::Mat& seedBg8,
                                    const Options& opt)
{
    CV_Assert(bgScore01.type() == CV_32F);
    CV_Assert(seedBg8.type() == CV_8U);
    CV_Assert(bgScore01.size() == seedBg8.size());

    const float qGrow = (opt.growLowQuantile > 0.0f ? opt.growLowQuantile : 0.70f);
    const float thrGrow = quantile32f_sampled(bgScore01, qGrow);

    cv::Mat allow;
    cv::compare(bgScore01, thrGrow, allow, cv::CMP_GE); // 0/255

    // BFS from seeds into allow
    const int rows = allow.rows, cols = allow.cols;
    cv::Mat visited(rows, cols, CV_8U, cv::Scalar(0));
    cv::Mat out(rows, cols, CV_8U, cv::Scalar(0));
    std::deque<cv::Point> q;

    auto pushIf = [&](int x, int y){
        if ((unsigned)x >= (unsigned)cols || (unsigned)y >= (unsigned)rows) return;
        if (visited.at<uchar>(y,x)) return;
        if (allow.at<uchar>(y,x) == 0) return;
        visited.at<uchar>(y,x) = 255;
        q.emplace_back(x,y);
    };

    for (int y = 0; y < rows; ++y)
    {
        const uchar* sRow = seedBg8.ptr<uchar>(y);
        for (int x = 0; x < cols; ++x)
            if (sRow[x]) pushIf(x,y);
    }

    const int dx[4] = {1,-1,0,0};
    const int dy[4] = {0,0,1,-1};

    while (!q.empty())
    {
        cv::Point p = q.front(); q.pop_front();
        out.at<uchar>(p.y,p.x) = 255;

        for (int k = 0; k < 4; ++k)
            pushIf(p.x + dx[k], p.y + dy[k]);
    }

    return out;
}

static void cleanupMask(cv::Mat& subject8, const Options& opt)
{
    CV_Assert(subject8.type() == CV_8U);

    // Median blur helps remove salt-and-pepper
    if (opt.medianK >= 3 && (opt.medianK % 2) == 1)
        cv::medianBlur(subject8, subject8, opt.medianK);

    // Morph cleanup
    if (opt.morphRadius > 0)
    {
        const int r = opt.morphRadius;
        cv::Mat se = cv::getStructuringElement(cv::MORPH_ELLIPSE, cv::Size(2*r+1, 2*r+1));
        // close then open is often good for masks (fill pinholes then remove specks)
        cv::morphologyEx(subject8, subject8, cv::MORPH_CLOSE, se);
        cv::morphologyEx(subject8, subject8, cv::MORPH_OPEN,  se);
    }

    // Remove tiny subject islands
    if (opt.removeIslandsMinArea > 0)
    {
        cv::Mat labels, stats, centroids;
        int n = cv::connectedComponentsWithStats(subject8 > 0, labels, stats, centroids, 8, CV_32S);
        if (n > 1)
        {
            cv::Mat out = cv::Mat::zeros(subject8.size(), CV_8U);
            for (int i = 1; i < n; ++i)
            {
                int area = stats.at<int>(i, cv::CC_STAT_AREA);
                if (area >= opt.removeIslandsMinArea)
                    out.setTo(255, labels == i);
            }
            subject8 = out;
        }
    }
}

// ============================================================
// Public: makeOverlayBGR
// ============================================================

cv::Mat makeOverlayBGR(const cv::Mat &baseGrayOrBGR,
                       const cv::Mat &bgConfidence01,   // CV_32F
                       const Options &opt)
{
    QString srcFun = "FSBackground::makeOverlayBGR";
    CV_Assert(!baseGrayOrBGR.empty());
    CV_Assert(bgConfidence01.type() == CV_32F);
    // FIX: be forgiving — resize base to match bgConfidence size
    cv::Mat base = baseGrayOrBGR;
    if (base.size() != bgConfidence01.size())
    {
        const bool downscale = (base.cols > bgConfidence01.cols) || (base.rows > bgConfidence01.rows);
        cv::resize(base, base, bgConfidence01.size(), 0, 0, downscale ? cv::INTER_AREA : cv::INTER_LINEAR);
    }
    // CV_Assert(bgConfidence01.size() == baseGrayOrBGR.size());  // Fails

    cv::Mat baseBGR;

    if (base.channels() == 1)
    {
        cv::Mat base8;
        if (base.type() == CV_8U) base8 = base;
        else base.convertTo(base8, CV_8U, 255.0);
        cv::cvtColor(base8, baseBGR, cv::COLOR_GRAY2BGR);
    }
    else
    {
        if (base.type() == CV_8UC3) baseBGR = base.clone();
        else {
            cv::Mat tmp8;
            base.convertTo(tmp8, CV_8U, 255.0);
            baseBGR = tmp8;
        }
    }

    // confidence -> heatmap
    cv::Mat conf8;
    bgConfidence01.convertTo(conf8, CV_8U, 255.0);

    cv::Mat heat;
    cv::applyColorMap(conf8, heat, cv::COLORMAP_JET);

    // only overlay where confidence is above threshold
    cv::Mat mask;
    cv::compare(bgConfidence01, opt.overlayThresh, mask, cv::CMP_GT); // 0/255

    cv::Mat blended;
    cv::addWeighted(baseBGR, 1.0f - opt.overlayAlpha, heat, opt.overlayAlpha, 0.0, blended);

    // composite blended into base using mask
    blended.copyTo(baseBGR, mask);
    return baseBGR;
}

// ============================================================
// Public: replaceBackground
// ============================================================

bool replaceBackground(cv::Mat              &fusedInOut,      // CV_8UC3 or CV_32FC3
                       const cv::Mat        &subjectMask8,    // CV_8U 0/255
                       const cv::Mat        &bgSource,        // same type/size as fusedInOut
                       const Options        &opt,
                       std::atomic_bool     *abortFlag)
{
    if (fusedInOut.empty() || subjectMask8.empty() || bgSource.empty())
        return false;

    CV_Assert(subjectMask8.type() == CV_8U);
    CV_Assert(fusedInOut.size() == subjectMask8.size());
    CV_Assert(bgSource.size()   == subjectMask8.size());
    CV_Assert(bgSource.type()   == fusedInOut.type());
    CV_Assert(fusedInOut.channels() == 3);

    if (aborted(abortFlag)) return false;

    // alpha = subject (1 inside subject, 0 in background), with feather
    cv::Mat alpha32;
    subjectMask8.convertTo(alpha32, CV_32F, 1.0 / 255.0);

    // Feather edge: blur alpha; larger featherRadius => smoother transition
    if (opt.featherRadius > 0)
    {
        // sigma heuristic; stable on high-res
        const double sigma = std::max(0.8, opt.featherRadius / 2.0);
        const int k = std::max(3, (opt.featherRadius * 2 + 1) | 1);
        cv::GaussianBlur(alpha32, alpha32, cv::Size(k, k), sigma, sigma, cv::BORDER_REFLECT);
    }

    // Gamma shaping
    if (std::abs(opt.featherGamma - 1.0f) > 1e-3f)
    {
        cv::pow(alpha32, opt.featherGamma, alpha32);
    }

    // Convert images to float for blend
    cv::Mat F, B;
    if (fusedInOut.type() == CV_8UC3) fusedInOut.convertTo(F, CV_32FC3, 1.0/255.0);
    else F = fusedInOut;

    if (bgSource.type() == CV_8UC3) bgSource.convertTo(B, CV_32FC3, 1.0/255.0);
    else B = bgSource;

    if (aborted(abortFlag)) return false;

    // Blend: out = alpha*F + (1-alpha)*B
    std::vector<cv::Mat> a3(3, alpha32);
    cv::Mat alpha3;
    cv::merge(a3, alpha3);

    cv::Mat out = alpha3.mul(F) + (cv::Scalar(1,1,1) - alpha3).mul(B);

    // Back to original type
    if (fusedInOut.type() == CV_8UC3)
        out.convertTo(fusedInOut, CV_8UC3, 255.0);
    else
        fusedInOut = out;

    return true;
}

// ============================================================
// Public: run
// ============================================================

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

    if (statusCb) statusCb("FSBackground: Building background mask (seed + grow)...");

    const bool needDepth  = m.contains("Depth", Qt::CaseInsensitive);
    const bool needFocus  = m.contains("Focus", Qt::CaseInsensitive);
    const bool needColorV = m.contains("ColorVar", Qt::CaseInsensitive) || opt.enableColorVar;

    // Validate inputs
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

    // Canonical size (prefer focus if used, else depth)
    cv::Size canonicalSize;
    if (needFocus)      canonicalSize = focusSlices32.front().size();
    else if (needDepth) canonicalSize = depthIndex16.size();
    else                return false;

    // Canonicalize inputs ONCE
    cv::Mat depth16C;
    if (needDepth)
        depth16C = FSUtilities::canonicalizeDepthIndex16(depthIndex16, canonicalSize,
                                                         "FSBackground depthIndex16");

    std::vector<cv::Mat> focusC;
    if (needFocus)
        focusC = FSUtilities::canonicalizeFocusSlices(focusSlices32, canonicalSize, abortFlag);

    std::vector<cv::Mat> colorC;
    if (needColorV)
        colorC = FSUtilities::canonicalizeAlignedColor(alignedColor, canonicalSize, abortFlag);

    if (needDepth && depth16C.empty()) return false;
    if (needFocus && focusC.empty())  return false;
    if (needColorV && colorC.empty()) return false;
    
    if (opt.writeDebug && !opt.backgroundFolder.isEmpty())
        ensureDebugFolder(opt.backgroundFolder);

    // --- Component maps (canonical CV_32F) ---
    cv::Mat bgScore01 = cv::Mat::zeros(canonicalSize, CV_32F);

    // Depth components
    cv::Mat d01, stdv, dN, stdvN, depthCombined;
    if (needDepth)
    {
        if (statusCb) statusCb("FSBackground: Depth features...");

        d01  = depthNorm01FromIndex16(depth16C, sliceCount);     // 0..1
        stdv = localStdDevDepthIndex(depth16C, 2);               // slice units

        // Normalize with robust quantiles (these are empirical; adjust if needed)
        dN    = normalizeRobust01(d01,  0.20f, 0.98f);
        stdvN = normalizeRobust01(stdv, 0.60f, 0.98f);

        depthCombined = 0.70f * dN + 0.30f * stdvN;
        clamp01InPlace(depthCombined);

        bgScore01 = cv::max(bgScore01, depthCombined);
    }

    // Focus components (FIXED normalization + scale-invariant support)
    cv::Mat maxF, strongF, focusWeak, supportRatio, ratioN, ratioWeak, focusCombined;
    std::vector<float> perSliceP99;

    if (needFocus)
    {
        if (statusCb) statusCb("FSBackground: Focus features (scale-invariant)...");

        // 1) Normalize each slice by its own p99 so values are comparable across stacks
        std::vector<cv::Mat> focusN = normalizeFocusSlicesP99(focusC, abortFlag, &perSliceP99);
        if (focusN.empty()) return false;

        // 2) maxF in normalized space
        maxF = computeFocusMax(focusN, abortFlag, progressCb);
        if (maxF.empty()) return false;

        // 3) strong focus (robust normalize), then weak focus = 1 - strong
        //    (quantiles chosen to emphasize separation)
        strongF  = normalizeRobust01(maxF, 0.50f, 0.995f);
        focusWeak = 1.0f - strongF;
        clamp01InPlace(focusWeak);

        // 4) Support ratio (scale-invariant): per-pixel threshold uses maxF
        //    supportAbs is your UI "supportFocusThresh" (interpreted in normalized space).
        //    supportRel makes it adapt to local maxF to avoid false support in weak areas.
        const float supportAbs = std::max(0.0f, opt.supportFocusThresh);
        const float supportRel = 0.25f; // << key scale-invariant term; tune 0.15..0.35
        supportRatio = computeFocusSupportRatio_ScaleInvariant(focusN, maxF,
                                                               supportAbs, supportRel, abortFlag);
        if (supportRatio.empty()) return false;

        ratioN   = normalizeRobust01(supportRatio, 0.05f, 0.95f);
        ratioWeak = 1.0f - ratioN;
        clamp01InPlace(ratioWeak);

        focusCombined = 0.65f * focusWeak + 0.35f * ratioWeak;
        clamp01InPlace(focusCombined);

        bgScore01 = cv::max(bgScore01, focusCombined);
    }

    // Optional color variance
    cv::Mat var, varN;
    if (needColorV)
    {
        if (statusCb) statusCb("FSBackground: Color variance feature...");
        var = computeColorVariance01(colorC, abortFlag);
        if (var.empty()) return false;

        varN = normalizeRobust01(var, 0.60f, 0.995f);
        bgScore01 = cv::max(bgScore01, varN);
    }

    clamp01InPlace(bgScore01);

    // ---------------- Stage 1: SEEDS ----------------
    if (statusCb) statusCb("FSBackground: Seeding background...");
    cv::Mat seedBg8 = buildBackgroundSeeds(bgScore01, opt);
    if (seedBg8.empty()) return false;

    // ---------------- Stage 2: REGION GROW ----------------
    if (statusCb) statusCb("FSBackground: Growing background region...");
    cv::Mat grownBg8 = growBackgroundRegion(bgScore01, seedBg8, opt);
    if (grownBg8.empty()) return false;

    // Subject mask = NOT background (subject=255)
    cv::Mat subject8 = (grownBg8 == 0);
    subject8.convertTo(subject8, CV_8U, 255.0);

    cleanupMask(subject8, opt);

    // Confidence output:
    // Use bgScore, but enforce grown region as strong background
    cv::Mat conf = bgScore01.clone();
    conf.setTo(1.0f, grownBg8);
    clamp01InPlace(conf);

    bgConfidence01Out = conf;
    subjectMask8Out   = subject8;

    // Debug dumps (WITH embedded PNG metadata via FSUtilities::writePngWithTitle)
    if (opt.writeDebug && !opt.backgroundFolder.isEmpty())
    {
        ensureDebugFolder(opt.backgroundFolder);

        auto P = [&](const QString& name) {
            return opt.backgroundFolder + "/" + name;
        };

        bool writeTitle = false;

        // Primary outputs
        FSUtilities::writePngWithTitle(P("bg_score.png"), bgScore01, writeTitle);
        FSUtilities::writePngWithTitle(P("bg_seed.png"), seedBg8, writeTitle);
        FSUtilities::writePngWithTitle(P("bg_grown.png"), grownBg8, writeTitle);
        FSUtilities::writePngWithTitle(P("subject_mask.png"), subject8, writeTitle);
        FSUtilities::writePngWithTitle(P("bg_confidence.png"), conf, writeTitle);

        // Depth diagnostics
        if (needDepth)
        {
            FSUtilities::writePngWithTitle(P("depthPrior.png"), d01, writeTitle);
            FSUtilities::writePngWithTitle(P("depthPriorN.png"), dN, writeTitle);
            FSUtilities::writePngWithTitle(P("depthStdDev.png"), stdv, writeTitle);
            FSUtilities::writePngWithTitle(P("depthStdDevN.png"), stdvN, writeTitle);
            FSUtilities::writePngWithTitle(P("depthCombined.png"), depthCombined, writeTitle);
        }

        // Focus diagnostics
        if (needFocus)
        {
            FSUtilities::writePngWithTitle(P("maxF.png"), maxF, writeTitle);
            FSUtilities::writePngWithTitle(P("strongF.png"), strongF, writeTitle);
            FSUtilities::writePngWithTitle(P("focusBG.png"), focusWeak, writeTitle);       // "BG-ness" from focus
            FSUtilities::writePngWithTitle(P("supportRatio.png"), supportRatio, writeTitle);
            FSUtilities::writePngWithTitle(P("ratioN.png"), ratioN, writeTitle);
            FSUtilities::writePngWithTitle(P("ratioBG.png"), ratioWeak, writeTitle);
            FSUtilities::writePngWithTitle(P("focusCombined.png"), focusCombined, writeTitle);
        }

        // ColorVar diagnostics
        if (needColorV)
        {
            FSUtilities::writePngWithTitle(P("colorVar.png"), var, writeTitle);
            FSUtilities::writePngWithTitle(P("colorVarN.png"), varN, writeTitle);
        }
    }

    if (statusCb) statusCb("FSBackground: Background mask complete.");
    return true;
}

} // namespace FSBackground


// bool run(const cv::Mat                 &depthIndex16,
//          const std::vector<cv::Mat>    &focusSlices32,
//          const std::vector<cv::Mat>    &alignedColor,
//          int                            sliceCount,
//          const Options                 &opt,
//          std::atomic_bool              *abortFlag,
//          ProgressCallback               progressCb,
//          StatusCallback                 statusCb,
//          cv::Mat                       &bgConfidence01Out,
//          cv::Mat                       &subjectMask8Out)
// {
//     const QString srcFun = "FSBackground::run";
//     const QString m = opt.method.trimmed();

//     if (statusCb) statusCb("FSBackground: Building background mask (seed + grow)...");

//     const bool needDepth  = m.contains("Depth", Qt::CaseInsensitive);
//     const bool needFocus  = m.contains("Focus", Qt::CaseInsensitive);
//     const bool needColorV = m.contains("ColorVar", Qt::CaseInsensitive) || opt.enableColorVar;

//     // Validate inputs
//     if (needDepth)
//     {
//         if (depthIndex16.empty() || depthIndex16.type() != CV_16U)
//         {
//             if (statusCb) statusCb(srcFun + ": Depth requested but depthIndex16 invalid.");
//             return false;
//         }
//         if (sliceCount < 2)
//         {
//             if (statusCb) statusCb(srcFun + ": sliceCount must be >= 2.");
//             return false;
//         }
//     }
//     if (needFocus && focusSlices32.empty())
//     {
//         if (statusCb) statusCb(srcFun + ": Focus requested but focusSlices32 is empty.");
//         return false;
//     }
//     if (needColorV && alignedColor.empty())
//     {
//         if (statusCb) statusCb(srcFun + ": ColorVar requested but alignedColor is empty.");
//         return false;
//     }

//     // Canonical size (prefer focus if used, else depth)
//     cv::Size canonicalSize;
//     if (needFocus)      canonicalSize = focusSlices32.front().size();
//     else if (needDepth) canonicalSize = depthIndex16.size();
//     else                return false;

//     // Canonicalize inputs ONCE
//     cv::Mat depth16C;
//     if (needDepth)
//         depth16C = FSUtilities::canonicalizeDepthIndex16(depthIndex16, canonicalSize, "FSBackground depthIndex16");

//     std::vector<cv::Mat> focusC;
//     if (needFocus)
//         focusC = FSUtilities::canonicalizeFocusSlices(focusSlices32, canonicalSize, abortFlag);

//     std::vector<cv::Mat> colorC;
//     if (needColorV)
//         colorC = FSUtilities::canonicalizeAlignedColor(alignedColor, canonicalSize, abortFlag);

//     if (needDepth && depth16C.empty()) return false;
//     if (needFocus && focusC.empty())  return false;
//     if (needColorV && colorC.empty()) return false;

//     if (opt.writeDebug && !opt.debugFolder.isEmpty())
//         ensureDebugFolder(opt.debugFolder);

//     // --- Build component maps (all canonical CV_32F 0..1 where possible) ---
//     cv::Mat bgScore01 = cv::Mat::zeros(canonicalSize, CV_32F);

//     // Depth components (data-adaptive ramps)
//     if (needDepth)
//     {
//         if (statusCb) statusCb("FSBackground: Depth features...");
//         cv::Mat d01 = depthNorm01FromIndex16(depth16C, sliceCount);  // 0..1
//         cv::Mat stdv = localStdDevDepthIndex(depth16C, 2);          // slice-units

//         // normalize instability into 0..1 robustly
//         cv::Mat stdvN = normalizeRobust01(stdv, 0.60f, 0.98f);

//         // far depth prior (also robust, in case d01 is skewed)
//         cv::Mat dN = normalizeRobust01(d01, 0.20f, 0.98f);

//         cv::Mat depthCombined = 0.70f * dN + 0.30f * stdvN;
//         bgScore01 = cv::max(bgScore01, depthCombined);
//     }

//     // Focus components (do NOT assume focus is 0..1; normalize robustly per run)
//     if (needFocus)
//     {
//         if (statusCb) statusCb("FSBackground: Focus features...");
//         cv::Mat maxF = computeFocusMax(focusC, abortFlag, progressCb);
//         if (maxF.empty()) return false;

//         // Robust normalize maxF; then “weak focus” = 1 - strongFocus
//         // If background dominates, this usually separates well.
//         cv::Mat strongF = normalizeRobust01(maxF, 0.50f, 0.995f);
//         cv::Mat focusWeak = 1.0f - strongF;

//         // Support ratio is already 0..1; still normalize so it adapts to stack behavior
//         cv::Mat ratio = computeFocusSupportRatio(focusC, opt.supportFocusThresh, abortFlag, nullptr);
//         if (ratio.empty()) return false;

//         cv::Mat ratioN = normalizeRobust01(ratio, 0.05f, 0.95f);
//         cv::Mat ratioWeak = 1.0f - ratioN;

//         cv::Mat focusCombined = 0.65f * focusWeak + 0.35f * ratioWeak;
//         bgScore01 = cv::max(bgScore01, focusCombined);
//     }

//     // Optional color variance
//     if (needColorV)
//     {
//         if (statusCb) statusCb("FSBackground: Color variance feature...");
//         cv::Mat var = computeColorVariance01(colorC, abortFlag, nullptr);
//         if (var.empty()) return false;

//         cv::Mat varN = normalizeRobust01(var, 0.60f, 0.995f);
//         bgScore01 = cv::max(bgScore01, varN);
//     }

//     clamp01InPlace(bgScore01);

//     // ---------------- Stage 1: SEEDS ----------------
//     if (statusCb) statusCb("FSBackground: Seeding background...");
//     cv::Mat seedBg8 = buildBackgroundSeeds(bgScore01, opt);      // 255 where seed
//     if (seedBg8.empty()) return false;

//     // ---------------- Stage 2: REGION GROW ----------------
//     if (statusCb) statusCb("FSBackground: Growing background region...");
//     cv::Mat grownBg8 = growBackgroundRegion(bgScore01, seedBg8, opt); // 255 where grown bg
//     if (grownBg8.empty()) return false;

//     // Convert grown background into subject mask (subject=255)
//     cv::Mat subject8 = (grownBg8 == 0);
//     subject8.convertTo(subject8, CV_8U, 255.0);

//     cleanupMask(subject8, opt);

//     // Confidence output:
//     // Use bgScore, but enforce grown region as strong background.
//     cv::Mat conf = bgScore01.clone();
//     conf.setTo(1.0f, grownBg8); // grown bg => max confidence background
//     clamp01InPlace(conf);

//     bgConfidence01Out = conf;
//     subjectMask8Out   = subject8;

//     // Debug
//     if (opt.writeDebug && !opt.debugFolder.isEmpty())
//     {
//         QDir().mkpath(opt.debugFolder);

//         cv::Mat conf8;
//         conf.convertTo(conf8, CV_8U, 255.0);

//         cv::imwrite((opt.debugFolder + "/bg_score.png").toStdString(), conf8);
//         cv::imwrite((opt.debugFolder + "/bg_seed.png").toStdString(), seedBg8);
//         cv::imwrite((opt.debugFolder + "/bg_grown.png").toStdString(), grownBg8);
//         cv::imwrite((opt.debugFolder + "/subject_mask.png").toStdString(), subject8);
//     }

//     if (statusCb) statusCb("FSBackground: Background mask complete.");
//     return true;
// }

// } // namespace FSBackground
