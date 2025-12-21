#include "FSBackground.h"
#include "FocusStack/fsutilities.h"
#include "Main/global.h"

#include <opencv2/imgproc.hpp>
#include <opencv2/imgcodecs.hpp>
#include <QDir>
#include <QDebug>
#include <queue>
#include <algorithm>
#include <numeric>
#include <cmath>

namespace FSBackground {

// ---- Debug writers --------------------------------------------------
static void ensureDebugFolder(const QString& folder)
{
    QDir d(folder);
    if (!d.exists())
        QDir().mkpath(folder);
}

static bool writeU8(const QString& path, const cv::Mat& u8)
{
    if (u8.empty()) return false;
    CV_Assert(u8.type() == CV_8U || u8.type() == CV_8UC3);
    return cv::imwrite(path.toStdString(), u8);
}

// Writes CV_32F assumed 0..1 (clamps) -> PNG 8U
static bool writeFloat01AsU8(const QString& path, cv::Mat f32)
{
    if (f32.empty()) return false;
    CV_Assert(f32.type() == CV_32F);

    cv::min(f32, 1.0f, f32);
    cv::max(f32, 0.0f, f32);

    cv::Mat u8;
    f32.convertTo(u8, CV_8U, 255.0);
    return cv::imwrite(path.toStdString(), u8);
}

// Writes CV_32F with auto min/max normalization (optionally log-compress)
static bool writeFloatAutoU8(const QString& path, const cv::Mat& f32in, bool logCompress)
{
    if (f32in.empty()) return false;
    CV_Assert(f32in.type() == CV_32F);

    cv::Mat m = f32in.clone();

    if (logCompress) {
        // log(1 + x) for visualization
        cv::max(m, 0.0f, m);
        m += 1.0f;
        cv::log(m, m);
    }

    double vmin = 0.0, vmax = 0.0;
    cv::minMaxLoc(m, &vmin, &vmax);

    cv::Mat u8;
    if (vmax <= vmin + 1e-12) {
        u8 = cv::Mat(m.size(), CV_8U, cv::Scalar(0));
    } else {
        m.convertTo(u8, CV_8U, 255.0 / (vmax - vmin), -vmin * 255.0 / (vmax - vmin));
    }
    return cv::imwrite(path.toStdString(), u8);
}

// Writes a binary mask (0/255) as-is
static bool writeMask(const QString& path, const cv::Mat& mask8)
{
    if (mask8.empty()) return false;
    CV_Assert(mask8.type() == CV_8U);
    return cv::imwrite(path.toStdString(), mask8);
}

// -------------------- small local helpers --------------------

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

// robust percentile for CV_32F single-channel
static float percentile32F(const cv::Mat& m32f, float q01)
{
    CV_Assert(m32f.type() == CV_32F && m32f.channels() == 1);
    CV_Assert(q01 >= 0.0f && q01 <= 1.0f);

    std::vector<float> v;
    v.reserve((size_t)m32f.total());

    for (int y = 0; y < m32f.rows; ++y)
    {
        const float* p = m32f.ptr<float>(y);
        for (int x = 0; x < m32f.cols; ++x)
        {
            float a = p[x];
            if (std::isfinite(a)) v.push_back(a);
        }
    }
    if (v.empty()) return 0.0f;

    const size_t k = (size_t)std::clamp<double>(q01 * (v.size() - 1), 0.0, double(v.size() - 1));
    std::nth_element(v.begin(), v.begin() + (ptrdiff_t)k, v.end());
    return v[k];
}

// normalize m to 0..1 using robust low/high percentiles
static cv::Mat normalizeRobust01(const cv::Mat& m32f, float qLo, float qHi)
{
    CV_Assert(m32f.type() == CV_32F && m32f.channels() == 1);
    float lo = percentile32F(m32f, qLo);
    float hi = percentile32F(m32f, qHi);
    if (hi <= lo + 1e-9f)
        hi = lo + 1e-3f;

    cv::Mat out(m32f.size(), CV_32F);
    for (int y = 0; y < m32f.rows; ++y)
    {
        const float* ip = m32f.ptr<float>(y);
        float* op = out.ptr<float>(y);
        for (int x = 0; x < m32f.cols; ++x)
        {
            float t = (ip[x] - lo) / (hi - lo);
            t = std::min(1.0f, std::max(0.0f, t));
            op[x] = t;
        }
    }
    return out;
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

        cv::Mat m = (focusSlices32[i] > supportThresh); // CV_8U
        count += m / 255;

        if (progressCb) progressCb(i + 1, N);
    }

    cv::Mat ratio(sz, CV_32F);
    count.convertTo(ratio, CV_32F, 1.0 / float(N));
    return ratio;
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

        cv::Mat g = toFloat01_Gray(alignedColor[i]); // 0..1 float
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

// -------------------- Seed + Grow core --------------------

// Build seed mask from bgScore01 using adaptive quantile thresholds.
// Returns CV_8U 0/255.
static cv::Mat buildBackgroundSeeds(const cv::Mat& bgScore01,
                                    const Options& opt)
{
    CV_Assert(bgScore01.type() == CV_32F);

    // Auto seed quantile: default to strong seeds = top ~8% of score
    float qHi = (opt.seedHighQuantile > 0.0f) ? opt.seedHighQuantile : 0.92f;
    qHi = std::min(0.995f, std::max(0.50f, qHi));

    float tHi = percentile32F(bgScore01, qHi);

    cv::Mat seeds = (bgScore01 >= tHi); // CV_8U 0/255

    if (opt.preferBorderSeeds)
    {
        // Keep only seed pixels connected to borders (macro assumption: background touches edges).
        // This eliminates “floating” interior seeds that often punch holes in subject.
        cv::Mat keep = cv::Mat::zeros(seeds.size(), CV_8U);

        std::queue<cv::Point> q;
        auto pushIfSeed = [&](int x, int y){
            if ((unsigned)x >= (unsigned)seeds.cols || (unsigned)y >= (unsigned)seeds.rows) return;
            if (keep.at<uchar>(y,x)) return;
            if (!seeds.at<uchar>(y,x)) return;
            keep.at<uchar>(y,x) = 255;
            q.push({x,y});
        };

        // initialize with border seed pixels
        for (int x = 0; x < seeds.cols; ++x) { pushIfSeed(x, 0); pushIfSeed(x, seeds.rows-1); }
        for (int y = 0; y < seeds.rows; ++y) { pushIfSeed(0, y); pushIfSeed(seeds.cols-1, y); }

        const int dx[8] = {1,1,0,-1,-1,-1,0,1};
        const int dy[8] = {0,1,1,1,0,-1,-1,-1};

        while (!q.empty())
        {
            cv::Point p = q.front(); q.pop();
            for (int k = 0; k < 8; ++k)
                pushIfSeed(p.x + dx[k], p.y + dy[k]);
        }

        seeds = keep; // border-connected seeds only
    }

    return seeds;
}

// Grow background from seeds using hysteresis thresholds on bgScore01.
// high seeds are already selected; grow into pixels above low threshold.
// Returns CV_8U 0/255 background region.
static cv::Mat growBackgroundRegion(const cv::Mat& bgScore01,
                                    const cv::Mat& seedBg8,
                                    const Options& opt)
{
    CV_Assert(bgScore01.type() == CV_32F);
    CV_Assert(seedBg8.type() == CV_8U);
    CV_Assert(bgScore01.size() == seedBg8.size());

    // Auto grow quantile: default to allow growth into top ~30% bg-score
    float qLo = (opt.growLowQuantile > 0.0f) ? opt.growLowQuantile : 0.70f;
    qLo = std::min(0.98f, std::max(0.05f, qLo));

    float tLo = percentile32F(bgScore01, qLo);

    cv::Mat grown = cv::Mat::zeros(seedBg8.size(), CV_8U);
    std::queue<cv::Point> q;

    for (int y = 0; y < seedBg8.rows; ++y)
    {
        const uchar* sp = seedBg8.ptr<uchar>(y);
        for (int x = 0; x < seedBg8.cols; ++x)
        {
            if (sp[x])
            {
                grown.at<uchar>(y,x) = 255;
                q.push({x,y});
            }
        }
    }

    const int dx[8] = {1,1,0,-1,-1,-1,0,1};
    const int dy[8] = {0,1,1,1,0,-1,-1,-1};

    while (!q.empty())
    {
        cv::Point p = q.front(); q.pop();

        for (int k = 0; k < 8; ++k)
        {
            int nx = p.x + dx[k];
            int ny = p.y + dy[k];
            if ((unsigned)nx >= (unsigned)bgScore01.cols || (unsigned)ny >= (unsigned)bgScore01.rows)
                continue;

            if (grown.at<uchar>(ny,nx))
                continue;

            float s = bgScore01.at<float>(ny,nx);
            if (s >= tLo)
            {
                grown.at<uchar>(ny,nx) = 255;
                q.push({nx,ny});
            }
        }
    }

    return grown;
}

// -------------------- Your overlay (fixed to never crash on size mismatch) --------------------

cv::Mat makeOverlayBGR(const cv::Mat &baseGrayOrBGR,
                       const cv::Mat &bgConfidence01,
                       const Options &opt)
{
    const QString srcFun = "FSBackground::makeOverlayBGR";
    if (baseGrayOrBGR.empty() || bgConfidence01.empty())
        return {};

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

    // Canonicalize conf to base size
    cv::Mat conf = bgConfidence01;
    if (conf.size() != baseBGR.size())
    {
        qWarning() << srcFun
                   << ": resizing bgConfidence from"
                   << conf.cols << "x" << conf.rows
                   << "to"
                   << baseBGR.cols << "x" << baseBGR.rows;
        cv::resize(conf, conf, baseBGR.size(), 0, 0, cv::INTER_LINEAR);
    }
    conf = conf.clone();
    clamp01InPlace(conf);

    // Heat
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

    cv::Mat mask = (conf >= opt.overlayThresh); // CV_8U same size as base

    cv::Mat blend;
    cv::addWeighted(baseBGR, 1.0 - opt.overlayAlpha, heat, opt.overlayAlpha, 0.0, blend);

    cv::Mat out = baseBGR.clone();
    CV_Assert(out.size() == mask.size());
    blend.copyTo(out, mask);
    return out;
}

// replaceBackground() unchanged from your version (kept as-is)
static cv::Mat makeFeatherWeight01(const cv::Mat &subjectMask8, const Options &opt)
{
    CV_Assert(subjectMask8.type() == CV_8U);

    cv::Mat subj = (subjectMask8 > 0);
    cv::Mat distIn, distOut;

    cv::distanceTransform(subj, distIn, cv::DIST_L2, 3);
    cv::distanceTransform(255 - subj, distOut, cv::DIST_L2, 3);

    const float R = float(std::max(1, opt.featherRadius));

    cv::Mat w = cv::Mat::zeros(subjectMask8.size(), CV_32F);
    for (int y = 0; y < w.rows; ++y)
    {
        const float *din = distIn.ptr<float>(y);
        float *wp = w.ptr<float>(y);
        for (int x = 0; x < w.cols; ++x)
        {
            if (subj.at<uchar>(y,x))
            {
                float t = std::min(1.0f, din[x] / R);
                wp[x] = std::pow(t, opt.featherGamma);
            }
            else wp[x] = 0.0f;
        }
    }
    clamp01InPlace(w);
    return w;
}

// ---------- helpers for debug writing (PNG with Title) -----------------

// Convert CV_8UC1/CV_8UC3 to QImage (copies data -> safe)
static QImage mat8ToQImageCopy(const cv::Mat& m)
{
    CV_Assert(!m.empty());
    if (m.type() == CV_8UC1)
    {
        QImage img(m.cols, m.rows, QImage::Format_Grayscale8);
        for (int y = 0; y < m.rows; ++y)
            memcpy(img.scanLine(y), m.ptr(y), size_t(m.cols));
        return img;
    }
    if (m.type() == CV_8UC3)
    {
        // OpenCV is BGR; QImage wants RGB
        cv::Mat rgb;
        cv::cvtColor(m, rgb, cv::COLOR_BGR2RGB);
        QImage img(rgb.data, rgb.cols, rgb.rows, int(rgb.step), QImage::Format_RGB888);
        return img.copy(); // deep copy
    }
    CV_Assert(false && "mat8ToQImageCopy expects CV_8UC1 or CV_8UC3");
    return {};
}

static bool writePngWithTitle(const QString& pathPng,
                              const cv::Mat& img8u1_or_8u3,
                              const QString& title)
{
    CV_Assert(!img8u1_or_8u3.empty());
    CV_Assert(img8u1_or_8u3.type() == CV_8UC1 || img8u1_or_8u3.type() == CV_8UC3);

    QImage qimg = mat8ToQImageCopy(img8u1_or_8u3);
    if (qimg.isNull()) return false;

    QImageWriter w(pathPng, "png");
    w.setText("Title", title);
    // Optional extra breadcrumbs (handy later):
    w.setText("Software", "Winnow FocusStack");
    w.setText("Description", title);

    if (!w.write(qimg))
    {
        qWarning() << "writePngWithTitle failed:" << pathPng << w.errorString();
        return false;
    }
    return true;
}

static inline bool writeDebugPNG8(const QString& folder,
                                  const QString& fileName,
                                  const cv::Mat& img8)
{
    if (folder.isEmpty() || fileName.isEmpty() || img8.empty()) return false;
    ensureDebugFolder(folder);
    const QString path = folder + "/" + fileName;
    return FSUtilities::writePngWithTitle(path, img8);
}

static inline bool writeDebugPNG_Float01_As8(const QString& folder,
                                             const QString& fileName,
                                             const cv::Mat& f01)
{
    if (f01.empty()) return false;
    CV_Assert(f01.type() == CV_32F);

    cv::Mat c = f01.clone();
    clamp01InPlace(c);

    cv::Mat out8;
    c.convertTo(out8, CV_8U, 255.0);
    return writeDebugPNG8(folder, fileName, out8);
}

static inline bool writeDebugPNG_FloatRobust_As8(const QString& folder,
                                                 const QString& fileName,
                                                 const cv::Mat& fAny,
                                                 float qLo,
                                                 float qHi)
{
    if (fAny.empty()) return false;
    CV_Assert(fAny.type() == CV_32F);

    cv::Mat n = normalizeRobust01(fAny, qLo, qHi); // should output 0..1
    return writeDebugPNG_Float01_As8(folder, fileName, n);
}

// ------------------------------------------

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

    cv::Mat w = makeFeatherWeight01(subjectMask8, opt);
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
    else return false;

    return true;
}

// -------------------- Main run() (seed + grow) --------------------

// ---------- FSBackground::run() ----------------------------------------

bool FSBackground::run(const cv::Mat                 &depthIndex16,
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
        depth16C = FSUtilities::canonicalizeDepthIndex16(depthIndex16, canonicalSize, "FSBackground depthIndex16");

    std::vector<cv::Mat> focusC;
    if (needFocus)
        focusC = FSUtilities::canonicalizeFocusSlices(focusSlices32, canonicalSize, abortFlag);

    std::vector<cv::Mat> colorC;
    if (needColorV)
        colorC = FSUtilities::canonicalizeAlignedColor(alignedColor, canonicalSize, abortFlag);

    if (needDepth && depth16C.empty()) return false;
    if (needFocus && focusC.empty())   return false;
    if (needColorV && colorC.empty())  return false;

    // Debug folder + writer lambda
    const bool doDbg = opt.writeDebug && !opt.debugFolder.isEmpty();
    if (doDbg) QDir().mkpath(opt.debugFolder);

    auto dbgPath = [&](const QString& file)->QString {
        return opt.debugFolder + "/" + file;
    };
    auto dbgWriteU8 = [&](const QString& file, const cv::Mat& imgAnyType)->void {
        if (!doDbg) return;
        const QString p = dbgPath(file);
        FSUtilities::writePngWithTitle(p, imgAnyType);
    };
    auto dbgWriteF32Robust = [&](const QString& file, const cv::Mat& f32,
                                 float loQ=0.02f, float hiQ=0.98f)->void {
        if (!doDbg) return;
        const QString p = dbgPath(file);
        FSUtilities::writePngFromFloatMapRobust(p, f32, loQ, hiQ);
    };
    auto dbgWriteF01 = [&](const QString& file, const cv::Mat& f01)->void {
        if (!doDbg) return;
        cv::Mat cl = f01.clone();
        cv::min(cl, 1.0f, cl);
        cv::max(cl, 0.0f, cl);
        cv::Mat u8;
        cl.convertTo(u8, CV_8U, 255.0);
        dbgWriteU8(file, u8);
    };

    // --- Build component maps (all canonical CV_32F; some are 0..1) ---
    cv::Mat bgScore01 = cv::Mat::zeros(canonicalSize, CV_32F);

    // Keep component diagnostics
    cv::Mat d01, stdv, stdvN, dN, depthCombined;
    cv::Mat maxF, strongF, focusWeak;
    cv::Mat supportRatio, ratioN, ratioWeak, focusCombined;
    cv::Mat colorVar, colorVarN;

    // Depth components (data-adaptive ramps)
    if (needDepth)
    {
        if (statusCb) statusCb("FSBackground: Depth features...");

        d01   = depthNorm01FromIndex16(depth16C, sliceCount); // 0..1
        stdv  = localStdDevDepthIndex(depth16C, 2);           // slice-units

        // normalize instability into 0..1 robustly
        stdvN = normalizeRobust01(stdv, 0.60f, 0.98f);

        // far depth prior (robust, in case d01 is skewed)
        dN    = normalizeRobust01(d01, 0.20f, 0.98f);

        depthCombined = 0.70f * dN + 0.30f * stdvN;
        bgScore01     = cv::max(bgScore01, depthCombined);

        // Diagnostics
        dbgWriteF01("depth_norm01.png", d01);
        dbgWriteF32Robust("depth_stddev.png", stdv, 0.02f, 0.98f);
        dbgWriteF01("depth_stddevN.png", stdvN);
        dbgWriteF01("depth_priorN.png", dN);
        dbgWriteF01("depth_combined.png", depthCombined);
    }

    // Focus components (normalize robustly per run)
    if (needFocus)
    {
        if (statusCb) statusCb("FSBackground: Focus features...");

        maxF = computeFocusMax(focusC, abortFlag, progressCb);
        if (maxF.empty()) return false;

        // strongF in 0..1 (robust)
        strongF   = normalizeRobust01(maxF, 0.50f, 0.995f);
        focusWeak = 1.0f - strongF;

        supportRatio = computeFocusSupportRatio(focusC, opt.supportFocusThresh, abortFlag, nullptr);
        if (supportRatio.empty()) return false;

        ratioN    = normalizeRobust01(supportRatio, 0.05f, 0.95f);
        ratioWeak = 1.0f - ratioN;

        focusCombined = 0.65f * focusWeak + 0.35f * ratioWeak;
        bgScore01     = cv::max(bgScore01, focusCombined);

        // Diagnostics (these match the list you said you *don’t* have yet)
        dbgWriteF32Robust("maxF.png", maxF, 0.02f, 0.995f);
        dbgWriteF01("strongF.png", strongF);
        dbgWriteF01("focusWeak.png", focusWeak);

        dbgWriteF01("supportRatio.png", supportRatio);
        dbgWriteF01("ratioN.png", ratioN);
        dbgWriteF01("ratioWeak.png", ratioWeak);

        dbgWriteF01("focusCombined.png", focusCombined);
    }

    // Optional color variance
    if (needColorV)
    {
        if (statusCb) statusCb("FSBackground: Color variance feature...");

        colorVar = computeColorVariance01(colorC, abortFlag, nullptr);
        if (colorVar.empty()) return false;

        colorVarN = normalizeRobust01(colorVar, 0.60f, 0.995f);
        bgScore01 = cv::max(bgScore01, colorVarN);

        dbgWriteF32Robust("colorVar.png", colorVar, 0.02f, 0.995f);
        dbgWriteF01("colorVarN.png", colorVarN);
    }

    clamp01InPlace(bgScore01);
    dbgWriteF01("bgScore01.png", bgScore01);

    // ---------------- Stage 1: SEEDS ----------------
    if (statusCb) statusCb("FSBackground: Seeding background...");
    cv::Mat seedBg8 = buildBackgroundSeeds(bgScore01, opt); // 255 where seed
    if (seedBg8.empty()) return false;
    dbgWriteU8("bg_seed.png", seedBg8);

    // ---------------- Stage 2: REGION GROW ----------------
    if (statusCb) statusCb("FSBackground: Growing background region...");
    cv::Mat grownBg8 = growBackgroundRegion(bgScore01, seedBg8, opt); // 255 where grown bg
    if (grownBg8.empty()) return false;
    dbgWriteU8("bg_grown.png", grownBg8);

    // Convert grown background into subject mask (subject=255)
    cv::Mat subject8 = (grownBg8 == 0);
    subject8.convertTo(subject8, CV_8U, 255.0);

    cleanupMask(subject8, opt);
    dbgWriteU8("subject_mask.png", subject8);

    // Confidence output: use bgScore, but enforce grown region as strong background
    cv::Mat conf = bgScore01.clone();
    conf.setTo(1.0f, grownBg8);
    clamp01InPlace(conf);

    bgConfidence01Out = conf;
    subjectMask8Out   = subject8;

    dbgWriteF01("bg_confidence01.png", conf);

    if (statusCb) statusCb("FSBackground: Background mask complete.");
    return true;
}


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

} // namespace FSBackground
