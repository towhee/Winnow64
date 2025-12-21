#include "FocusStack/fsbackground.h"
#include "FocusStack/FSUtilities.h"

#include <opencv2/imgproc.hpp>
#include <opencv2/imgcodecs.hpp>

#include <QDir>
#include <QFileInfo>
#include <QDebug>

#include <atomic>
#include <vector>
#include <deque>
#include <algorithm>
#include <cmath>

namespace FSBackground {

// ------------------------------------------------------------
// Small helpers
// ------------------------------------------------------------
static inline bool abortRequested(std::atomic_bool *a)
{
    return a && a->load(std::memory_order_relaxed);
}

static inline int oddK(int k) { return (k % 2 == 1) ? k : (k + 1); }

static void clamp01InPlace(cv::Mat &m)
{
    if (m.empty()) return;
    CV_Assert(m.type() == CV_32F || m.type() == CV_32FC1);
    cv::min(m, 1.0f, m);
    cv::max(m, 0.0f, m);
}

static void ensureDebugFolder(const QString &folder)
{
    if (folder.isEmpty()) return;
    QDir().mkpath(folder);
}

static bool writeDbgPng(const QString &folder,
                        const QString &fileName,
                        const cv::Mat &img)
{
    if (folder.isEmpty()) return false;
    ensureDebugFolder(folder);
    const QString path = folder + "/" + fileName;
    return FSUtilities::writePngWithTitle(path, img);
}

// ------------------------------------------------------------
// Robust quantiles + normalization (scale-safe diagnostics)
// ------------------------------------------------------------

// Sample up to ~500k pixels for quantiles (fast enough, stable enough)
static float quantile32f(const cv::Mat &m32f, float q)
{
    CV_Assert(m32f.type() == CV_32F);
    CV_Assert(q >= 0.0f && q <= 1.0f);
    if (m32f.empty()) return 0.0f;

    const int64_t total = (int64_t)m32f.total();
    const int64_t targetSamples = 500000;
    const int step = (total > targetSamples) ? (int)std::max<int64_t>(1, total / targetSamples) : 1;

    std::vector<float> v;
    v.reserve((size_t)(total / step + 1));

    const float *p = (const float*)m32f.ptr<float>(0);
    for (int64_t i = 0; i < total; i += step)
    {
        float x = p[i];
        if (std::isfinite(x)) v.push_back(x);
    }

    if (v.empty()) return 0.0f;

    size_t k = (size_t)std::floor(q * (float)(v.size() - 1));
    std::nth_element(v.begin(), v.begin() + k, v.end());
    return v[k];
}

static cv::Mat normalizeRobust01(const cv::Mat &in32f,
                                 float qLow,
                                 float qHigh,
                                 float eps = 1e-6f)
{
    CV_Assert(in32f.type() == CV_32F);
    CV_Assert(qLow >= 0.0f && qLow < qHigh && qHigh <= 1.0f);

    float lo = quantile32f(in32f, qLow);
    float hi = quantile32f(in32f, qHigh);

    if (!std::isfinite(lo)) lo = 0.0f;
    if (!std::isfinite(hi)) hi = lo + 1.0f;

    if (hi - lo < eps)
        hi = lo + 1.0f;

    cv::Mat out;
    in32f.convertTo(out, CV_32F, 1.0, -lo);
    out *= (1.0f / (hi - lo));
    clamp01InPlace(out);
    return out;
}

// ------------------------------------------------------------
// Feature builders
// ------------------------------------------------------------

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
    return out; // 0..1
}

static cv::Mat localStdDevDepthIndex(const cv::Mat &depthIndex16, int radius)
{
    CV_Assert(depthIndex16.type() == CV_16U);
    radius = std::max(1, radius);
    const int k = 2 * radius + 1;

    cv::Mat d32;
    depthIndex16.convertTo(d32, CV_32F);

    cv::Mat mean, mean2;
    cv::boxFilter(d32, mean,  CV_32F, cv::Size(k,k), cv::Point(-1,-1), true, cv::BORDER_REFLECT);
    cv::boxFilter(d32.mul(d32), mean2, CV_32F, cv::Size(k,k), cv::Point(-1,-1), true, cv::BORDER_REFLECT);

    cv::Mat var = mean2 - mean.mul(mean);
    cv::max(var, 0.0f, var);
    cv::Mat stdv;
    cv::sqrt(var, stdv);
    return stdv; // slice-index units
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

// Scale-invariant support:
// count slices where focus_i >= supportFrac * maxF(pixel)
// AND maxF(pixel) must be “meaningful” (avoid tiny-max false support).
static cv::Mat computeSupportRatioScaleInvariant(const std::vector<cv::Mat> &focusSlices32,
                                                 const cv::Mat &maxF32,
                                                 float supportFrac,          // e.g. 0.25..0.45
                                                 float minMaxFNorm,          // e.g. 0.05 in normalized maxF space
                                                 float maxFHiForNorm,        // global robust high for maxF normalization
                                                 std::atomic_bool *abortFlag)
{
    CV_Assert(!focusSlices32.empty());
    CV_Assert(maxF32.type() == CV_32F);
    CV_Assert(maxF32.size() == focusSlices32.front().size());

    supportFrac = std::max(0.01f, std::min(0.95f, supportFrac));
    minMaxFNorm = std::max(0.0f, std::min(1.0f, minMaxFNorm));
    maxFHiForNorm = std::max(1e-6f, maxFHiForNorm);

    const cv::Size sz = maxF32.size();
    cv::Mat count = cv::Mat::zeros(sz, CV_32S);

    const int N = (int)focusSlices32.size();
    for (int i = 0; i < N; ++i)
    {
        if (abortRequested(abortFlag)) return {};
        const cv::Mat &F = focusSlices32[i];
        CV_Assert(F.type() == CV_32F);
        CV_Assert(F.size() == sz);

        for (int y = 0; y < sz.height; ++y)
        {
            const float *fp = F.ptr<float>(y);
            const float *mp = maxF32.ptr<float>(y);
            int *cp = count.ptr<int>(y);

            for (int x = 0; x < sz.width; ++x)
            {
                const float m = mp[x];
                const float mNorm = m / maxFHiForNorm;

                if (mNorm < minMaxFNorm)
                    continue; // treat as “no focus”; support stays low

                const float thr = supportFrac * m;
                if (fp[x] >= thr)
                    cp[x] += 1;
            }
        }
    }

    cv::Mat ratio(sz, CV_32F);
    count.convertTo(ratio, CV_32F, 1.0 / float(N));
    return ratio; // 0..1
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

static cv::Mat computeColorVariance01(const std::vector<cv::Mat> &alignedColor,
                                      std::atomic_bool *abortFlag)
{
    CV_Assert(!alignedColor.empty());
    const int N = (int)alignedColor.size();
    const cv::Size sz = alignedColor.front().size();

    cv::Mat mean = cv::Mat::zeros(sz, CV_32F);
    cv::Mat mean2 = cv::Mat::zeros(sz, CV_32F);

    for (int i = 0; i < N; ++i)
    {
        if (abortRequested(abortFlag)) return {};
        CV_Assert(alignedColor[i].size() == sz);

        cv::Mat g = toFloat01_Gray(alignedColor[i]); // 0..1 float-ish
        mean  += g;
        mean2 += g.mul(g);
    }

    mean  *= (1.0f / float(N));
    mean2 *= (1.0f / float(N));

    cv::Mat var = mean2 - mean.mul(mean);
    cv::max(var, 0.0f, var);
    return var; // 0..~
}

// ------------------------------------------------------------
// Mask cleanup + feathering
// ------------------------------------------------------------
static void cleanupMask(cv::Mat &mask8, const Options &opt)
{
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

    if (opt.removeIslandsMinArea > 0)
    {
        cv::Mat labels, stats, centroids;
        int n = cv::connectedComponentsWithStats(mask8 > 0, labels, stats, centroids, 8, CV_32S);

        cv::Mat cleaned = cv::Mat::zeros(mask8.size(), CV_8U);
        for (int i = 1; i < n; ++i)
        {
            int area = stats.at<int>(i, cv::CC_STAT_AREA);
            if (area >= opt.removeIslandsMinArea)
                cleaned.setTo(255, labels == i);
        }
        mask8 = cleaned;
    }
}

static cv::Mat makeFeatherWeight01(const cv::Mat &subjectMask8,
                                   const Options &opt)
{
    CV_Assert(subjectMask8.type() == CV_8U);

    cv::Mat subj = (subjectMask8 > 0); // 0/255
    cv::Mat distIn;

    cv::distanceTransform(subj, distIn, cv::DIST_L2, 3);

    const float R = float(std::max(1, opt.featherRadius));
    cv::Mat w = cv::Mat::zeros(subjectMask8.size(), CV_32F);

    for (int y = 0; y < w.rows; ++y)
    {
        const float *din  = distIn.ptr<float>(y);
        const uchar *sm   = subj.ptr<uchar>(y);
        float *wp = w.ptr<float>(y);

        for (int x = 0; x < w.cols; ++x)
        {
            if (sm[x])
            {
                float t = std::min(1.0f, din[x] / R);
                wp[x] = std::pow(t, opt.featherGamma);
            }
            else
                wp[x] = 0.0f;
        }
    }

    clamp01InPlace(w);
    return w;
}

// ------------------------------------------------------------
// Seed + grow (region from borders or strong BG scores)
// ------------------------------------------------------------
static cv::Mat buildBorderBandMask8(const cv::Size &sz, int band)
{
    band = std::max(1, band);
    cv::Mat m(sz, CV_8U, cv::Scalar(0));
    m.rowRange(0, std::min(band, sz.height)).setTo(255);
    m.rowRange(std::max(0, sz.height - band), sz.height).setTo(255);
    m.colRange(0, std::min(band, sz.width)).setTo(255);
    m.colRange(std::max(0, sz.width - band), sz.width).setTo(255);
    return m;
}

static cv::Mat buildBackgroundSeeds(const cv::Mat &bgScore01,
                                    const Options &opt,
                                    float seedQuantileUsedOut = 0.0f)
{
    Q_UNUSED(seedQuantileUsedOut);
    CV_Assert(bgScore01.type() == CV_32F);

    float q = opt.seedHighQuantile;
    if (q < 0.0f) q = 0.93f; // default top 7%

    const float thr = quantile32f(bgScore01, q);
    cv::Mat seed = (bgScore01 >= thr); // CV_8U 0/255

    if (opt.preferBorderSeeds)
    {
        cv::Mat border = buildBorderBandMask8(bgScore01.size(), /*band=*/6);
        seed &= border;

        // If border seeding is too sparse, fall back to global seeds
        const double cnt = cv::countNonZero(seed);
        const double area = (double)bgScore01.total();
        if (area > 0.0 && cnt / area < 0.0005) // <0.05% pixels
            seed = (bgScore01 >= thr);
    }

    // Clean seeds slightly
    cv::Mat se = cv::getStructuringElement(cv::MORPH_ELLIPSE, cv::Size(5,5));
    cv::morphologyEx(seed, seed, cv::MORPH_OPEN, se);

    return seed;
}

static cv::Mat growBackgroundRegion(const cv::Mat &bgScore01,
                                    const cv::Mat &seedBg8,
                                    const Options &opt,
                                    cv::Mat *allowedMask8Out = nullptr)
{
    CV_Assert(bgScore01.type() == CV_32F);
    CV_Assert(seedBg8.type() == CV_8U);
    CV_Assert(seedBg8.size() == bgScore01.size());

    float q = opt.growLowQuantile;
    if (q < 0.0f) q = 0.70f; // default top 30% are allowed

    const float thr = quantile32f(bgScore01, q);
    cv::Mat allowed = (bgScore01 >= thr); // CV_8U 0/255

    if (allowedMask8Out)
        *allowedMask8Out = allowed.clone();

    // Multi-source BFS over allowed region
    const int W = bgScore01.cols;
    const int H = bgScore01.rows;

    cv::Mat grown(H, W, CV_8U, cv::Scalar(0));
    std::deque<cv::Point> qpts;

    // Initialize queue with seed pixels that are also allowed
    for (int y = 0; y < H; ++y)
    {
        const uchar *sp = seedBg8.ptr<uchar>(y);
        const uchar *ap = allowed.ptr<uchar>(y);
        uchar *gp = grown.ptr<uchar>(y);

        for (int x = 0; x < W; ++x)
        {
            if (sp[x] && ap[x])
            {
                gp[x] = 255;
                qpts.emplace_back(x, y);
            }
        }
    }

    auto inside = [&](int x, int y){ return (unsigned)x < (unsigned)W && (unsigned)y < (unsigned)H; };

    const int dx[8] = {1,-1,0,0, 1,1,-1,-1};
    const int dy[8] = {0,0,1,-1, 1,-1,1,-1};

    while (!qpts.empty())
    {
        const cv::Point p = qpts.front();
        qpts.pop_front();

        for (int k = 0; k < 8; ++k)
        {
            int nx = p.x + dx[k];
            int ny = p.y + dy[k];
            if (!inside(nx, ny)) continue;

            if (grown.at<uchar>(ny, nx)) continue;          // already in region
            if (!allowed.at<uchar>(ny, nx)) continue;       // not allowed

            grown.at<uchar>(ny, nx) = 255;
            qpts.emplace_back(nx, ny);
        }
    }

    // Smooth grown mask a bit
    cv::Mat se = cv::getStructuringElement(cv::MORPH_ELLIPSE, cv::Size(7,7));
    cv::morphologyEx(grown, grown, cv::MORPH_CLOSE, se);

    return grown;
}

// ------------------------------------------------------------
// Public: overlay
// ------------------------------------------------------------
cv::Mat makeOverlayBGR(const cv::Mat &baseGrayOrBGR,
                       const cv::Mat &bgConfidence01,
                       const Options &opt)
{
    CV_Assert(bgConfidence01.type() == CV_32F);

    cv::Mat baseBGR;
    if (baseGrayOrBGR.channels() == 1)
    {
        cv::Mat g8;
        if (baseGrayOrBGR.type() == CV_8U) g8 = baseGrayOrBGR;
        else {
            cv::Mat f = baseGrayOrBGR.clone();
            if (f.type() != CV_32F) f.convertTo(f, CV_32F);
            cv::Mat f01 = normalizeRobust01(f, 0.02f, 0.995f);
            f01.convertTo(g8, CV_8U, 255.0);
        }
        cv::cvtColor(g8, baseBGR, cv::COLOR_GRAY2BGR);
    }
    else
    {
        if (baseGrayOrBGR.type() == CV_8UC3) baseBGR = baseGrayOrBGR.clone();
        else {
            cv::Mat f = baseGrayOrBGR.clone();
            if (f.depth() != CV_32F) f.convertTo(f, CV_32F);
            cv::Mat f01 = f.clone();
            cv::min(f01, 1.0f, f01);
            cv::max(f01, 0.0f, f01);
            cv::Mat b8;
            f01.convertTo(b8, CV_8U, 255.0);
            baseBGR = b8;
        }
    }

    cv::Mat conf = bgConfidence01.clone();
    if (conf.size() != baseBGR.size())
        cv::resize(conf, conf, baseBGR.size(), 0, 0, cv::INTER_LINEAR);
    clamp01InPlace(conf);

    // Simple heat: yellow->orange->red
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

    cv::Mat mask = (conf >= opt.overlayThresh); // CV_8U 0/255
    CV_Assert(mask.type() == CV_8U);

    cv::Mat blend;
    cv::addWeighted(baseBGR, 1.0 - opt.overlayAlpha, heat, opt.overlayAlpha, 0.0, blend);

    cv::Mat out = baseBGR.clone();
    blend.copyTo(out, mask);
    return out;
}

// ------------------------------------------------------------
// Public: replaceBackground
// ------------------------------------------------------------
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

    cv::Mat w = makeFeatherWeight01(subjectMask8, opt); // 1 subject, 0 bg
    if (w.empty()) return false;

    const int rows = fusedInOut.rows;
    const int cols = fusedInOut.cols;
    const int ch   = fusedInOut.channels();

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
                    op[idx + c] = a * op[idx + c] + b * bp[idx + c];
            }
        }
    }
    else
    {
        return false;
    }

    return true;
}

// ------------------------------------------------------------
// Public: run()
// ------------------------------------------------------------
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

    if (statusCb) statusCb("FSBackground: Building background mask (robust + scale-invariant support)...");

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

    // Canonical size
    cv::Size canonicalSize;
    if (needFocus)      canonicalSize = focusSlices32.front().size();
    else if (needDepth) canonicalSize = depthIndex16.size();
    else
    {
        if (statusCb) statusCb(srcFun + ": Method requires neither Depth nor Focus.");
        return false;
    }

    // Canonicalize ONCE
    cv::Mat depth16C;
    if (needDepth)
        depth16C = FSUtilities::canonicalizeDepthIndex16(depthIndex16, canonicalSize, "FSBackground depthIndex16");

    std::vector<cv::Mat> focusC;
    if (needFocus)
        focusC = FSUtilities::canonicalizeFocusSlices(focusSlices32, canonicalSize, abortFlag);

    std::vector<cv::Mat> colorC;
    if (needColorV)
        colorC = FSUtilities::canonicalizeAlignedColor(alignedColor, canonicalSize, abortFlag);

    if (needDepth && depth16C.empty()) return false;
    if (needFocus && focusC.empty())  return false;
    if (needColorV && colorC.empty()) return false;

    if (opt.writeDebug && !opt.debugFolder.isEmpty())
        ensureDebugFolder(opt.debugFolder);

    // --- Build bgScore01 as max over features (all in 0..1) ---
    cv::Mat bgScore01 = cv::Mat::zeros(canonicalSize, CV_32F);

    // ---------------- Depth features ----------------
    cv::Mat d01, stdv, dN, stdvN, depthCombined;
    if (needDepth)
    {
        if (statusCb) statusCb("FSBackground: Depth features...");

        d01  = depthNorm01FromIndex16(depth16C, sliceCount);     // 0..1
        stdv = localStdDevDepthIndex(depth16C, /*radius=*/2);    // slice-units

        // Robust normalize (handles different stacks and slice counts)
        dN    = normalizeRobust01(d01,  0.20f, 0.995f);
        stdvN = normalizeRobust01(stdv, 0.60f, 0.995f);

        depthCombined = 0.70f * dN + 0.30f * stdvN;
        clamp01InPlace(depthCombined);

        bgScore01 = cv::max(bgScore01, depthCombined);

        if (opt.writeDebug && !opt.debugFolder.isEmpty())
        {
            writeDbgPng(opt.debugFolder, "depthNorm01.png", d01);
            writeDbgPng(opt.debugFolder, "depthStdDev.png", normalizeRobust01(stdv, 0.50f, 0.995f));
            writeDbgPng(opt.debugFolder, "depthStdDevN.png", stdvN);
            writeDbgPng(opt.debugFolder, "depthN.png", dN);
            writeDbgPng(opt.debugFolder, "depthCombined.png", depthCombined);
        }
    }

    // ---------------- Focus features ----------------
    cv::Mat maxF, maxFNorm01, strongF, focusWeak, supportRatio, supportRatioN, ratioWeak, focusCombined;
    float maxF_hi = 1.0f;

    if (needFocus)
    {
        if (statusCb) statusCb("FSBackground: Focus features (robust + scale-invariant support)...");

        maxF = computeFocusMax(focusC, abortFlag, progressCb);
        if (maxF.empty()) return false;

        // Robust global "high" for scale: treat this as ~1.0 for the stack
        maxF_hi = std::max(1e-6f, quantile32f(maxF, 0.995f));
        maxFNorm01 = maxF * (1.0f / maxF_hi);
        clamp01InPlace(maxFNorm01);

        // "Strong focus" map: robust normalize the normalized maxF
        // (this makes it resilient when most of the image is background)
        strongF = normalizeRobust01(maxFNorm01, 0.30f, 0.995f);
        focusWeak = 1.0f - strongF;

        // Scale-invariant support fraction derived from your old absolute-ish knobs:
        // default: 0.02 / 0.06 = 0.333 (a slice “supports” pixel if it has >= 33% of best focus at that pixel)
        float supportFrac = opt.supportFocusThresh / std::max(1e-6f, opt.focusHighThresh);
        supportFrac = std::max(0.10f, std::min(0.60f, supportFrac));

        // Also require some minimal maxF to avoid “everything supports” when maxF is tiny.
        // This is in normalized maxF space (0..1-ish).
        const float minMaxFNorm = 0.05f;

        supportRatio = computeSupportRatioScaleInvariant(
            focusC, maxF, supportFrac, minMaxFNorm, maxF_hi, abortFlag);
        if (supportRatio.empty()) return false;

        // Normalize support a bit so it adapts to different stack sizes / subject sizes
        supportRatioN = normalizeRobust01(supportRatio, 0.05f, 0.98f);
        ratioWeak = 1.0f - supportRatioN;

        focusCombined = 0.65f * focusWeak + 0.35f * ratioWeak;
        clamp01InPlace(focusCombined);

        bgScore01 = cv::max(bgScore01, focusCombined);

        if (opt.writeDebug && !opt.debugFolder.isEmpty())
        {
            // For “raw maxF”, write a robust-normalized view for readability
            writeDbgPng(opt.debugFolder, "maxF.png", normalizeRobust01(maxF, 0.50f, 0.995f));
            writeDbgPng(opt.debugFolder, "maxFNorm01.png", maxFNorm01);
            writeDbgPng(opt.debugFolder, "strongF.png", strongF);
            writeDbgPng(opt.debugFolder, "focusWeak.png", focusWeak);
            writeDbgPng(opt.debugFolder, "supportRatio.png", supportRatio);
            writeDbgPng(opt.debugFolder, "supportRatioN.png", supportRatioN);
            writeDbgPng(opt.debugFolder, "ratioWeak.png", ratioWeak);
            writeDbgPng(opt.debugFolder, "focusCombined.png", focusCombined);
        }
    }

    // ---------------- Color variance feature ----------------
    cv::Mat var, varN;
    if (needColorV)
    {
        if (statusCb) statusCb("FSBackground: Color variance feature...");

        var = computeColorVariance01(colorC, abortFlag);
        if (var.empty()) return false;

        // Robust normalize; variance scale differs across stacks
        varN = normalizeRobust01(var, 0.60f, 0.995f);
        bgScore01 = cv::max(bgScore01, varN);

        if (opt.writeDebug && !opt.debugFolder.isEmpty())
        {
            writeDbgPng(opt.debugFolder, "colorVar.png", normalizeRobust01(var, 0.50f, 0.995f));
            writeDbgPng(opt.debugFolder, "colorVarN.png", varN);
        }
    }

    clamp01InPlace(bgScore01);

    // ---------------- Stage 1: Seeds ----------------
    if (statusCb) statusCb("FSBackground: Seeding background...");
    cv::Mat seedBg8 = buildBackgroundSeeds(bgScore01, opt);
    if (seedBg8.empty()) return false;

    // ---------------- Stage 2: Grow ----------------
    if (statusCb) statusCb("FSBackground: Growing background region...");
    cv::Mat allowedMask8;
    cv::Mat grownBg8 = growBackgroundRegion(bgScore01, seedBg8, opt, &allowedMask8);
    if (grownBg8.empty()) return false;

    // Subject mask: subject=255, background=0
    cv::Mat subject8 = (grownBg8 == 0);
    subject8.convertTo(subject8, CV_8U, 255.0);
    cleanupMask(subject8, opt);

    // Confidence: bgScore, but grown region is enforced as confident background
    cv::Mat conf = bgScore01.clone();
    conf.setTo(1.0f, grownBg8);
    clamp01InPlace(conf);

    bgConfidence01Out = conf;
    subjectMask8Out   = subject8;

    // Debug
    if (opt.writeDebug && !opt.debugFolder.isEmpty())
    {
        writeDbgPng(opt.debugFolder, "bg_score.png", bgScore01);
        writeDbgPng(opt.debugFolder, "bg_seed.png", seedBg8);
        writeDbgPng(opt.debugFolder, "bg_growAllowed.png", allowedMask8);
        writeDbgPng(opt.debugFolder, "bg_grown.png", grownBg8);
        writeDbgPng(opt.debugFolder, "bg_confidence.png", conf);
        writeDbgPng(opt.debugFolder, "subject_mask.png", subject8);
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
