#include "FocusStack/fsbackground.h"
#include "FocusStack/fsutilities.h"
#include "Main/global.h"

#include <opencv2/imgproc.hpp>
#include <opencv2/imgcodecs.hpp>

#include <QDir>
#include <QDebug>
#include <deque>
#include <algorithm>
#include <numeric>
#include <cmath>

namespace FSBackground {

// ------------------------------------------------------------
// helpers (kept inside namespace; they do NOT need to be outside)
// ------------------------------------------------------------
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

    cv::Mat d32;
    depthIndex16.convertTo(d32, CV_32F);

    cv::Mat mean, mean2;
    cv::boxFilter(d32, mean,  CV_32F, cv::Size(k,k), cv::Point(-1,-1), true, cv::BORDER_REFLECT);
    cv::boxFilter(d32.mul(d32), mean2, CV_32F, cv::Size(k,k), cv::Point(-1,-1), true, cv::BORDER_REFLECT);

    cv::Mat var = mean2 - mean.mul(mean);
    cv::max(var, 0.0f, var);
    cv::Mat stdv;
    cv::sqrt(var, stdv);
    return stdv;
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

        cv::Mat m = (focusSlices32[i] > supportThresh); // CV_8U 0/255
        count += m / 255;

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
    CV_Assert(!alignedColor.empty());
    const int N = (int)alignedColor.size();
    const cv::Size sz = alignedColor.front().size();

    cv::Mat mean  = cv::Mat::zeros(sz, CV_32F);
    cv::Mat mean2 = cv::Mat::zeros(sz, CV_32F);

    for (int i = 0; i < N; ++i)
    {
        if (abortRequested(abortFlag)) return {};
        CV_Assert(alignedColor[i].size() == sz);

        cv::Mat g = toFloat01_Gray(alignedColor[i]);
        mean  += g;
        mean2 += g.mul(g);

        if (progressCb) progressCb(i + 1, N);
    }

    mean  *= (1.0f / float(N));
    mean2 *= (1.0f / float(N));

    cv::Mat var = mean2 - mean.mul(mean);
    cv::max(var, 0.0f, var);
    return var;
}

static void cleanupMask(cv::Mat &mask8, const Options &opt)
{
    if (mask8.empty()) return;
    CV_Assert(mask8.type() == CV_8U);

    if (opt.medianK >= 3)
        cv::medianBlur(mask8, mask8, oddK(opt.medianK));

    if (opt.morphRadius > 0)
    {
        int r = opt.morphRadius;
        cv::Mat se = cv::getStructuringElement(cv::MORPH_ELLIPSE, cv::Size(2*r+1, 2*r+1));
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

// ---------- quantile helpers (data-adaptive thresholds) ----------
static float quantile01_sampled(const cv::Mat& m32f01, float q, int step = 3)
{
    CV_Assert(m32f01.type() == CV_32F);
    q = std::min(1.0f, std::max(0.0f, q));

    std::vector<float> v;
    v.reserve((m32f01.rows/step + 1) * (m32f01.cols/step + 1));

    for (int y = 0; y < m32f01.rows; y += step)
    {
        const float* p = m32f01.ptr<float>(y);
        for (int x = 0; x < m32f01.cols; x += step)
            v.push_back(p[x]);
    }
    if (v.empty()) return 0.5f;

    size_t k = (size_t)std::llround(q * double(v.size() - 1));
    std::nth_element(v.begin(), v.begin() + (long)k, v.end());
    return v[k];
}

static float quantile01_border(const cv::Mat& m32f01, float q, int border = 12, int step = 2)
{
    CV_Assert(m32f01.type() == CV_32F);
    border = std::max(1, border);

    std::vector<float> v;
    v.reserve((m32f01.rows + m32f01.cols) * 2);

    const int rows = m32f01.rows;
    const int cols = m32f01.cols;

    // top/bottom bands
    for (int y = 0; y < border; y += step) {
        const float* p = m32f01.ptr<float>(y);
        for (int x = 0; x < cols; x += step) v.push_back(p[x]);
    }
    for (int y = rows - border; y < rows; y += step) {
        if (y < 0) continue;
        const float* p = m32f01.ptr<float>(y);
        for (int x = 0; x < cols; x += step) v.push_back(p[x]);
    }

    // left/right bands (avoid double-count corners too much; not critical)
    for (int y = 0; y < rows; y += step) {
        const float* p = m32f01.ptr<float>(y);
        for (int x = 0; x < border; x += step) v.push_back(p[x]);
        for (int x = cols - border; x < cols; x += step) if (x >= 0) v.push_back(p[x]);
    }

    if (v.empty()) return quantile01_sampled(m32f01, q, step);

    q = std::min(1.0f, std::max(0.0f, q));
    size_t k = (size_t)std::llround(q * double(v.size() - 1));
    std::nth_element(v.begin(), v.begin() + (long)k, v.end());
    return v[k];
}

static cv::Mat makeFeatherAlpha01(const cv::Mat& subjectMask8, int featherRadius, float gamma)
{
    CV_Assert(subjectMask8.type() == CV_8U);

    cv::Mat subj = (subjectMask8 > 0);
    subj.convertTo(subj, CV_8U, 255);

    cv::Mat inv; cv::bitwise_not(subj, inv);

    cv::Mat dIn, dOut;
    cv::distanceTransform(subj, dIn,  cv::DIST_L2, 3);
    cv::distanceTransform(inv,  dOut, cv::DIST_L2, 3);

    cv::Mat alpha = dIn / (dIn + dOut + 1e-6f); // 0..1, CV_32F

    if (featherRadius > 0) {
        // Soft-limit the transition width (optional)
        // (Keeps alpha from bleeding too far)
        cv::Mat a = alpha.clone();
        // No hard rule here—your edgeErode is doing most of the work.
        alpha = a;
    }

    if (gamma != 1.0f) {
        cv::pow(alpha, gamma, alpha);
    }
    return alpha;
}


// ---------- seed + region grow ----------
static cv::Mat borderSeedMask(const cv::Size& sz, int border = 12)
{
    border = std::max(1, border);
    cv::Mat b = cv::Mat::zeros(sz, CV_8U);
    cv::rectangle(b, cv::Rect(0, 0, sz.width, border), 255, cv::FILLED);
    cv::rectangle(b, cv::Rect(0, sz.height - border, sz.width, border), 255, cv::FILLED);
    cv::rectangle(b, cv::Rect(0, 0, border, sz.height), 255, cv::FILLED);
    cv::rectangle(b, cv::Rect(sz.width - border, 0, border, sz.height), 255, cv::FILLED);
    return b;
}

static cv::Mat regionGrowFromSeeds(const cv::Mat& allowMask8,  // CV_8U 0/255
                                   const cv::Mat& seeds8,      // CV_8U 0/255
                                   std::atomic_bool* abortFlag)
{
    CV_Assert(allowMask8.type() == CV_8U);
    CV_Assert(seeds8.type() == CV_8U);
    CV_Assert(allowMask8.size() == seeds8.size());

    const int rows = allowMask8.rows;
    const int cols = allowMask8.cols;

    cv::Mat out = cv::Mat::zeros(allowMask8.size(), CV_8U);
    cv::Mat vis = cv::Mat::zeros(allowMask8.size(), CV_8U);

    std::deque<cv::Point> q;
    for (int y = 0; y < rows; ++y) {
        const uchar* sp = seeds8.ptr<uchar>(y);
        const uchar* ap = allowMask8.ptr<uchar>(y);
        for (int x = 0; x < cols; ++x) {
            if (sp[x] && ap[x]) {
                vis.at<uchar>(y,x) = 255;
                out.at<uchar>(y,x) = 255;
                q.emplace_back(x,y);
            }
        }
    }

    const int dx[4] = { 1, -1, 0, 0 };
    const int dy[4] = { 0, 0, 1, -1 };

    while (!q.empty()) {
        if (abortRequested(abortFlag)) return {};
        cv::Point p = q.front(); q.pop_front();

        for (int k = 0; k < 4; ++k) {
            int nx = p.x + dx[k];
            int ny = p.y + dy[k];
            if ((unsigned)nx >= (unsigned)cols || (unsigned)ny >= (unsigned)rows) continue;
            if (vis.at<uchar>(ny,nx)) continue;
            if (!allowMask8.at<uchar>(ny,nx)) continue;

            vis.at<uchar>(ny,nx) = 255;
            out.at<uchar>(ny,nx) = 255;
            q.emplace_back(nx,ny);
        }
    }
    return out;
}

static cv::Mat seamMaskFromAlpha(const cv::Mat& alpha32,
                                 float lo = 0.08f, float hi = 0.92f)
{
    CV_Assert(alpha32.type() == CV_32F);
    cv::Mat m = (alpha32 > lo) & (alpha32 < hi);   // CV_8U 0/255
    m.convertTo(m, CV_8U, 255);
    return m;
}

// Make seam mask from the matte edge
static cv::Mat seamMaskFromMask(const cv::Mat& subjectMask8, int bandRadiusPx = 2)
{
    CV_Assert(subjectMask8.type() == CV_8U);
    bandRadiusPx = std::max(1, bandRadiusPx);

    cv::Mat se = cv::getStructuringElement(cv::MORPH_ELLIPSE,
                                           {2*bandRadiusPx+1, 2*bandRadiusPx+1});

    cv::Mat dil, ero, edge;
    cv::dilate(subjectMask8, dil, se);
    cv::erode (subjectMask8, ero, se);
    cv::absdiff(dil, ero, edge);          // edge band
    edge.setTo(255, edge > 0);
    return edge;
}

// Simple “red/magenta defringe” in BGR8:
// if R is much higher than max(G,B) in seam, pull it down.
static void defringeRedClampEdge(cv::Mat& bgr8,
                                 const cv::Mat& seam8,
                                 int excessThresh = 10,     // 6..16
                                 float strength = 0.70f)    // 0.3..0.9
{
    CV_Assert(bgr8.type() == CV_8UC3);
    CV_Assert(seam8.type() == CV_8U);
    CV_Assert(bgr8.size() == seam8.size());

    const float s = std::min(1.0f, std::max(0.0f, strength));

    for (int y = 0; y < bgr8.rows; ++y)
    {
        const uchar* m = seam8.ptr<uchar>(y);
        cv::Vec3b* p   = bgr8.ptr<cv::Vec3b>(y);

        for (int x = 0; x < bgr8.cols; ++x)
        {
            if (!m[x]) continue;

            int B = p[x][0];
            int G = p[x][1];
            int R = p[x][2];

            int base = std::max(G, B);
            int ex = R - base;

            if (ex > excessThresh)
            {
                int newEx = int(ex * (1.0f - s));          // reduce excess
                int newR  = std::min(255, base + newEx);
                p[x][2] = (uchar)newR;
            }
        }
    }
}

// replace defringeRedClampEdge
static void defringeLabChroma(cv::Mat& bgr8,
                              const cv::Mat& seam8,
                              float strength = 0.60f,   // 0..1
                              int chromaThresh = 6)     // 3..12
{
    CV_Assert(bgr8.type() == CV_8UC3);
    CV_Assert(seam8.type() == CV_8U);
    CV_Assert(bgr8.size() == seam8.size());

    strength = std::min(1.0f, std::max(0.0f, strength));

    cv::Mat lab;
    cv::cvtColor(bgr8, lab, cv::COLOR_BGR2Lab);

    for (int y = 0; y < lab.rows; ++y)
    {
        const uchar* m = seam8.ptr<uchar>(y);
        cv::Vec3b* p   = lab.ptr<cv::Vec3b>(y);

        for (int x = 0; x < lab.cols; ++x)
        {
            if (!m[x]) continue;

            int a = int(p[x][1]) - 128;
            int b = int(p[x][2]) - 128;
            int chroma = std::abs(a) + std::abs(b);

            if (chroma > chromaThresh)
            {
                a = int(a * (1.0f - strength));
                b = int(b * (1.0f - strength));
                p[x][1] = (uchar)std::clamp(128 + a, 0, 255);
                p[x][2] = (uchar)std::clamp(128 + b, 0, 255);
            }
        }
    }

    cv::cvtColor(lab, bgr8, cv::COLOR_Lab2BGR);
}

// ------------------------------------------------------------
// Public overlay (you already had this; keep with size safety)
// ------------------------------------------------------------
cv::Mat makeOverlayBGR(const cv::Mat &baseGrayOrBGR,
                       const cv::Mat &bgConfidence01,
                       const Options &opt)
{
    const QString srcFun = "FSBackground::makeOverlayBGR";
    if (G::FSLog) G::log(srcFun);

    CV_Assert(bgConfidence01.type() == CV_32F);

    cv::Mat baseBGR;
    if (baseGrayOrBGR.channels() == 1)
    {
        cv::Mat g8;
        if (baseGrayOrBGR.type() == CV_8U) g8 = baseGrayOrBGR;
        else {
            cv::Mat f01 = baseGrayOrBGR.clone();
            clamp01InPlace(f01);
            f01.convertTo(g8, CV_8U, 255.0);
        }
        cv::cvtColor(g8, baseBGR, cv::COLOR_GRAY2BGR);
    }
    else
    {
        if (baseGrayOrBGR.type() == CV_8UC3) baseBGR = baseGrayOrBGR.clone();
        else {
            cv::Mat f01 = baseGrayOrBGR.clone();
            cv::min(f01, 1.0f, f01);
            cv::max(f01, 0.0f, f01);
            f01.convertTo(baseBGR, CV_8UC3, 255.0);
        }
    }

    cv::Mat conf = bgConfidence01;
    if (conf.size() != baseBGR.size()) {
        // qWarning() << srcFun << ": resizing bgConfidence from"
        //            << FSUtilities::sizeToStr(conf.size())
        //            << "to" << FSUtilities::sizeToStr(baseBGR.size());
        cv::resize(conf, conf, baseBGR.size(), 0, 0, cv::INTER_LINEAR);
    }
    conf = conf.clone();
    clamp01InPlace(conf);

    cv::Mat heat(baseBGR.size(), CV_8UC3, cv::Scalar(0,0,0));
    for (int y = 0; y < conf.rows; ++y)
    {
        const float *c = conf.ptr<float>(y);
        cv::Vec3b *h = heat.ptr<cv::Vec3b>(y);
        for (int x = 0; x < conf.cols; ++x)
        {
            float v = c[x];
            if (v < 0.02f)      h[x] = {0,0,0};
            else if (v < 0.08f) h[x] = {0,255,255};
            else if (v < 0.18f) h[x] = {0,165,255};
            else                h[x] = {0,0,255};
        }
    }

    cv::Mat mask = (conf >= opt.overlayThresh); // CV_8U, same size
    CV_Assert(mask.size() == baseBGR.size());

    cv::Mat out = baseBGR.clone();
    cv::Mat blend;
    cv::addWeighted(baseBGR, 1.0 - opt.overlayAlpha, heat, opt.overlayAlpha, 0.0, blend);
    blend.copyTo(out, mask);
    return out;
}

bool replaceBackground(cv::Mat &fusedInOut,
                       const cv::Mat &subjectMask8,
                       const cv::Mat &bgSource,
                       const Options &opt,
                       std::atomic_bool *abortFlag)
{
    QString srcFun = "FSBackground::replaceBackground";
    if (abortRequested(abortFlag)) return false;

    if (G::FSLog) G::log(srcFun);

    /*
    qDebug() << "replaceBackground: "
             << "fused"   << fusedInOut.cols << fusedInOut.rows
             << "mask"    << subjectMask8.cols << subjectMask8.rows
             << "bg"      << bgSource.cols << bgSource.rows;
    //*/

    CV_Assert(!fusedInOut.empty());
    CV_Assert(fusedInOut.size() == bgSource.size());
    CV_Assert(fusedInOut.type() == bgSource.type());
    CV_Assert(subjectMask8.size() == fusedInOut.size());
    CV_Assert(subjectMask8.type() == CV_8U);

    cv::Mat alpha = makeFeatherAlpha01(subjectMask8, opt.featherRadius, opt.featherGamma);

    cv::Mat F,B;
    if (fusedInOut.depth() == CV_8U) {
        fusedInOut.convertTo(F, CV_32F, 1.0/255.0);
        bgSource.convertTo(B, CV_32F, 1.0/255.0);
    } else {
        fusedInOut.convertTo(F, CV_32F);
        bgSource.convertTo(B, CV_32F);
    }

    std::vector<cv::Mat> a3(3, alpha);
    cv::Mat A3; cv::merge(a3, A3);

    cv::Mat out32 = A3.mul(F) + (cv::Scalar(1,1,1) - A3).mul(B);

    if (fusedInOut.depth() == CV_8U)
        out32.convertTo(fusedInOut, fusedInOut.type(), 255.0);
    else
        out32.convertTo(fusedInOut, fusedInOut.type());

    // --- Defringe only in the seam band (where alpha transitions) ---
    qDebug() << "fusedInOut.type() =" << fusedInOut.type();
    if (fusedInOut.type() == CV_8UC3)
    {
        if (G::FSLog) G::log(srcFun, "Defringing");
        // cv::Mat seam8 = seamMaskFromAlpha(alpha, 0.08f, 0.92f);
        cv::Mat seam8 = seamMaskFromMask(subjectMask8, 2);

        // optional: widen seam a touch (helps at high magnification)
        int r = 1;
        cv::Mat se = cv::getStructuringElement(cv::MORPH_ELLIPSE, {2*r+1, 2*r+1});
        cv::dilate(seam8, seam8, se);

        defringeLabChroma(fusedInOut, seam8, 0.60f, 6);
        defringeRedClampEdge(fusedInOut, seam8,/*excessThresh=*/10,/*strength=*/0.70f);
    }

    return true;
}

// ------------------------------------------------------------
// Main entry (refactored into score → seeds → region grow)
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

    QString msg = "Building background mask usiing method " + m;
    if (G::FSLog) G::log(srcFun, msg);
    if (statusCb) statusCb(msg);

    const bool needDepth  = m.contains("Depth", Qt::CaseInsensitive);
    const bool needFocus  = m.contains("Focus", Qt::CaseInsensitive);
    const bool needColorV = m.contains("ColorVar", Qt::CaseInsensitive) || opt.enableColorVar;

    // Validate
    if (needDepth)
    {
        if (depthIndex16.empty() || depthIndex16.type() != CV_16U) {
            if (statusCb) statusCb(srcFun + ": Depth requested but depthIndex16 invalid.");
            return false;
        }
        if (sliceCount < 2) {
            if (statusCb) statusCb(srcFun + ": sliceCount must be >= 2.");
            return false;
        }
    }
    if (needFocus && focusSlices32.empty()) {
        if (statusCb) statusCb(srcFun + ": Focus requested but focusSlices32 is empty.");
        return false;
    }
    if (needColorV && alignedColor.empty()) {
        if (statusCb) statusCb(srcFun + ": ColorVar requested but alignedColor is empty.");
        return false;
    }

    // Canonical size
    cv::Size canonicalSize;
    if (needDepth)      canonicalSize = depthIndex16.size();
    else if (needFocus) canonicalSize = focusSlices32.front().size();
    else {
        if (statusCb) statusCb(srcFun + ": Method requires neither Depth nor Focus.");
        return false;
    }

    // Canonicalize inputs
    cv::Mat depth16C;
    if (needDepth)
        depth16C = FSUtilities::canonicalizeDepthIndex16(depthIndex16, canonicalSize, "depthIndex16");

    std::vector<cv::Mat> focusC;
    if (needFocus) {
        focusC = FSUtilities::canonicalizeFocusSlices(focusSlices32, canonicalSize, abortFlag);
        if (focusC.empty()) return false;
    }

    std::vector<cv::Mat> colorC;
    if (needColorV) {
        colorC = FSUtilities::canonicalizeAlignedColor(alignedColor, canonicalSize, abortFlag);
        if (colorC.empty()) return false;
    }

    // --------------- Stage A: build bgScore01 (0..1) ---------------
    cv::Mat bgScore01 = cv::Mat::zeros(canonicalSize, CV_32F);

    if (needDepth)
    {
        if (statusCb) statusCb("FSBackground: Depth score...");

        cv::Mat d01 = depthNorm01FromIndex16(depth16C, sliceCount);

        cv::Mat depthPrior = cv::Mat::zeros(canonicalSize, CV_32F);
        const float a = opt.farDepthStart01;
        const float b = std::max(a + 1e-6f, opt.farDepthStrong01);

        for (int y = 0; y < d01.rows; ++y) {
            const float* dp = d01.ptr<float>(y);
            float* op = depthPrior.ptr<float>(y);
            for (int x = 0; x < d01.cols; ++x) {
                float t = (dp[x] - a) / (b - a);
                t = std::min(1.0f, std::max(0.0f, t));
                op[x] = t;
            }
        }

        cv::Mat stdv = localStdDevDepthIndex(depth16C, 2);
        cv::Mat unstable = (stdv > opt.depthUnstableStd);
        cv::Mat unstableF;
        unstable.convertTo(unstableF, CV_32F, 1.0/255.0);

        cv::Mat depthCombined = 0.70f * depthPrior + 0.30f * unstableF;
        bgScore01 = cv::max(bgScore01, depthCombined);
    }

    if (needFocus)
    {
        if (statusCb) statusCb("FSBackground: Focus score...");

        cv::Mat maxF = computeFocusMax(focusC, abortFlag, progressCb);
        if (maxF.empty()) return false;

        cv::Mat focusBg(canonicalSize, CV_32F);
        const float lo = opt.focusLowThresh;
        const float hi = std::max(lo + 1e-6f, opt.focusHighThresh);

        for (int y = 0; y < maxF.rows; ++y) {
            const float* fp = maxF.ptr<float>(y);
            float* bp = focusBg.ptr<float>(y);
            for (int x = 0; x < maxF.cols; ++x) {
                float t = (fp[x] - lo) / (hi - lo);
                t = std::min(1.0f, std::max(0.0f, t));
                bp[x] = 1.0f - t;
            }
        }

        cv::Mat ratio = computeFocusSupportRatio(focusC, opt.supportFocusThresh, abortFlag, nullptr);
        if (ratio.empty()) return false;

        cv::Mat ratioBg(canonicalSize, CV_32F);
        const float r0 = opt.minSupportRatio;

        for (int y = 0; y < ratio.rows; ++y) {
            const float* rp = ratio.ptr<float>(y);
            float* bp = ratioBg.ptr<float>(y);
            for (int x = 0; x < ratio.cols; ++x) {
                float t = (rp[x] / std::max(1e-6f, r0));
                t = std::min(1.0f, std::max(0.0f, t));
                bp[x] = 1.0f - t;
            }
        }

        cv::Mat focusCombined = 0.65f * focusBg + 0.35f * ratioBg;
        bgScore01 = cv::max(bgScore01, focusCombined);
    }

    if (needColorV)
    {
        if (statusCb) statusCb("FSBackground: Color variance score...");
        cv::Mat var = computeColorVariance01(colorC, abortFlag, nullptr);
        if (var.empty()) return false;

        cv::Mat varBg(canonicalSize, CV_32F);
        const float t0 = opt.colorVarThresh;
        const float t1 = std::max(t0 + 1e-6f, t0 * 3.0f);

        for (int y = 0; y < var.rows; ++y) {
            const float* vp = var.ptr<float>(y);
            float* bp = varBg.ptr<float>(y);
            for (int x = 0; x < var.cols; ++x) {
                float t = (vp[x] - t0) / (t1 - t0);
                t = std::min(1.0f, std::max(0.0f, t));
                bp[x] = t;
            }
        }

        bgScore01 = cv::max(bgScore01, varBg);
    }

    clamp01InPlace(bgScore01);

    // --------------- Stage B: choose thresholds (auto if negative) ---------------
    float seedQ = opt.seedHighQuantile; // interpreted as quantile (0..1)
    float growQ = opt.growLowQuantile;

    // Auto defaults tuned to “background touches border”
    if (seedQ < 0.0f) seedQ = 0.92f;  // conservative seeds
    if (growQ < 0.0f) growQ = 0.35f;  // aggressive grow (top ~65% allowed)

    // Convert quantiles into thresholds.
    // Also compute border-based thresholds to adapt to each image.
    const float Ts_global = quantile01_sampled(bgScore01, seedQ, 3);
    const float Tg_global = quantile01_sampled(bgScore01, growQ, 3);

    const float Ts_border = quantile01_border(bgScore01, 0.80f, 12, 2);
    const float Tg_border = quantile01_border(bgScore01, 0.25f, 12, 2);

    float Ts = std::max(Ts_global, Ts_border);
    float Tg = std::min(Tg_global, Tg_border);   // allow more if border suggests it
    Tg = std::min(Ts - 1e-4f, Tg);               // ensure Tg < Ts
    Tg = std::max(0.0f, std::min(1.0f, Tg));

    // --------------- Stage C: seed + region grow ---------------
    if (statusCb) statusCb("FSBackground: Seeding + region grow...");

    cv::Mat allowMask8 = (bgScore01 >= Tg);
    allowMask8.convertTo(allowMask8, CV_8U, 255.0);

    cv::Mat seeds8 = (bgScore01 >= Ts);
    seeds8.convertTo(seeds8, CV_8U, 255.0);

    if (opt.preferBorderSeeds)
    {
        cv::Mat border = borderSeedMask(canonicalSize, 12);
        cv::bitwise_and(seeds8, border, seeds8);
    }

    // If we got zero seeds, relax Ts until we get something (data-adaptive safety)
    for (int relax = 0; relax < 4; ++relax)
    {
        if (cv::countNonZero(seeds8) > 0) break;
        Ts *= 0.92f;
        seeds8 = (bgScore01 >= Ts);
        seeds8.convertTo(seeds8, CV_8U, 255.0);
        if (opt.preferBorderSeeds) {
            cv::Mat border = borderSeedMask(canonicalSize, 12);
            cv::bitwise_and(seeds8, border, seeds8);
        }
    }

    cv::Mat bgRegion8 = regionGrowFromSeeds(allowMask8, seeds8, abortFlag);
    if (bgRegion8.empty()) return false;

    // Optional light cleanup on background region (close small gaps)
    if (opt.morphRadius > 0)
    {
        int r = std::max(1, opt.morphRadius);
        cv::Mat se = cv::getStructuringElement(cv::MORPH_ELLIPSE, cv::Size(2*r+1, 2*r+1));
        cv::morphologyEx(bgRegion8, bgRegion8, cv::MORPH_CLOSE, se);
    }

    // --------------- Output ---------------
    // Keep confidence as the raw bgScore01 (useful for debug + future tuning)
    bgConfidence01Out = bgScore01;

    // Subject mask = NOT background region
    cv::Mat subject8;
    cv::bitwise_not(bgRegion8, subject8);
    cleanupMask(subject8, opt);
    subjectMask8Out = subject8;

    // Debug writes
    if (opt.writeDebug && !opt.debugFolder.isEmpty())
    {
        QDir().mkpath(opt.debugFolder);

        cv::Mat score8;
        bgScore01.convertTo(score8, CV_8U, 255.0);

        cv::imwrite((opt.debugFolder + "/bg_score.png").toStdString(), score8);
        cv::imwrite((opt.debugFolder + "/bg_seeds.png").toStdString(), seeds8);
        cv::imwrite((opt.debugFolder + "/bg_allow.png").toStdString(), allowMask8);
        cv::imwrite((opt.debugFolder + "/bg_region.png").toStdString(), bgRegion8);
        cv::imwrite((opt.debugFolder + "/subject_mask.png").toStdString(), subject8);
    }

    if (statusCb) statusCb("FSBackground: Background mask complete.");
    return true;
}

} // namespace FSBackground
