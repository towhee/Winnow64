#include "fsfusiondmap.h"

#include "fusionpyr.h"

#include "Main/global.h"
#include "fsloader.h"

#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/imgcodecs.hpp>

#include <cmath>
#include <algorithm>
#include <limits>

namespace
{
// ------------------------------------------------------------
// debug helpers
// ------------------------------------------------------------

static inline cv::Scalar turboColorForSliceBGR(int s, int N)
{
    // Build a 1x1 "index" image, scale to 0..255 like the donor colorization
    CV_Assert(N > 0);
    int v = 0;
    if (N > 1) v = (int)std::lround(255.0 * (double)s / (double)(N - 1));
    v = std::max(0, std::min(255, v));

    cv::Mat idx(1, 1, CV_8U, cv::Scalar(v));
    cv::Mat col;
    cv::applyColorMap(idx, col, cv::COLORMAP_TURBO); // 1x1 BGR8
    cv::Vec3b c = col.at<cv::Vec3b>(0,0);
    return cv::Scalar(c[0], c[1], c[2]); // B,G,R
}

static inline void addSliceLegendStripBottomBGR8(cv::Mat& overlayBGR8,
                                                 int N,
                                                 int stripH = 140,
                                                 int cols = 8)
{
    CV_Assert(overlayBGR8.type() == CV_8UC3);
    if (overlayBGR8.empty() || N <= 0) return;

    stripH = std::max(60, std::min(stripH, overlayBGR8.rows)); // readable but safe
    cols   = std::max(1, cols);

    const int H = overlayBGR8.rows;
    const int W = overlayBGR8.cols;
    const int y0 = H - stripH;

    // Overwrite bottom area with dark background (no transparency => deterministic pixels)
    cv::rectangle(overlayBGR8, cv::Rect(0, y0, W, stripH), cv::Scalar(0,0,0), cv::FILLED);

    // Grid for entries
    const int rows = (N + cols - 1) / cols;
    const int cellW = std::max(1, W / cols);
    const int cellH = std::max(1, stripH / std::max(1, rows));

    // Text size: big and readable
    const int fontFace = cv::FONT_HERSHEY_SIMPLEX;
    double fontScale = std::max(0.6, cellH / 28.0);   // 140px strip => nice big text
    int thickness = std::max(2, (int)std::lround(fontScale * 2.0));

    for (int s = 0; s < N; ++s)
    {
        const int r = s / cols;
        const int c = s % cols;

        int x = c * cellW;
        int y = y0 + r * cellH;

        // Slight padding inside cell
        const int pad = std::max(4, cellH / 12);
        cv::Rect cell(x, y, (c == cols-1) ? (W - x) : cellW, (r == rows-1) ? (H - y) : cellH);
        cv::Rect swatch(cell.x + pad, cell.y + pad, std::max(18, cellH - 2*pad), std::max(18, cellH - 2*pad));
        swatch.width  = std::min(swatch.width,  cell.width  - 2*pad);
        swatch.height = std::min(swatch.height, cell.height - 2*pad);

        // Color swatch
        cv::Scalar bgr = turboColorForSliceBGR(s, N);
        cv::rectangle(overlayBGR8, swatch, bgr, cv::FILLED);
        cv::rectangle(overlayBGR8, swatch, cv::Scalar(255,255,255), 1);

        // Index text (white with black outline for readability)
        const std::string label = std::to_string(s);
        int baseline = 0;
        cv::Size ts = cv::getTextSize(label, fontFace, fontScale, thickness, &baseline);

        // Place text to the right of swatch, vertically centered
        int tx = swatch.x + swatch.width + pad;
        int ty = cell.y + (cell.height + ts.height) / 2;

        // Clamp within cell
        tx = std::min(tx, cell.x + cell.width - ts.width - pad);
        ty = std::min(ty, cell.y + cell.height - pad);

        // outline then fill
        cv::putText(overlayBGR8, label, cv::Point(tx, ty),
                    fontFace, fontScale, cv::Scalar(0,0,0), thickness + 2, cv::LINE_AA);
        cv::putText(overlayBGR8, label, cv::Point(tx, ty),
                    fontFace, fontScale, cv::Scalar(255,255,255), thickness, cv::LINE_AA);
    }

    // Optional title in top-left of strip
    {
        const std::string title = "Donor slice -> color (TURBO)";
        double titleScale = std::max(0.6, fontScale * 0.9);
        int titleThick = std::max(2, thickness);

        cv::putText(overlayBGR8, title, cv::Point(10, y0 + std::max(30, cellH/2)),
                    fontFace, titleScale, cv::Scalar(0,0,0), titleThick + 2, cv::LINE_AA);
        cv::putText(overlayBGR8, title, cv::Point(10, y0 + std::max(30, cellH/2)),
                    fontFace, titleScale, cv::Scalar(255,255,255), titleThick, cv::LINE_AA);
    }
}

static inline cv::Mat toGray8_fromOut32(const cv::Mat& out32)
{
    CV_Assert(out32.type() == CV_32FC3);

    cv::Mat gray32, gray8;
    cv::cvtColor(out32, gray32, cv::COLOR_BGR2GRAY);

    // clamp then convert
    cv::max(gray32, 0.0f, gray32);
    cv::min(gray32, 1.0f, gray32);
    gray32.convertTo(gray8, CV_8U, 255.0);

    return gray8;
}

// Colorize donor labels into BGR8 for overlay.
// Labels outside donor range / invalid (0xFFFF) become black.
static inline cv::Mat colorizeDonorLabelsBGR8(const cv::Mat& donor16, int N)
{
    CV_Assert(donor16.type() == CV_16U);
    CV_Assert(N > 0);

    // Map label -> 8U index (0..N-1), invalid -> 0
    cv::Mat idx8(donor16.size(), CV_8U, cv::Scalar(0));
    for (int y = 0; y < donor16.rows; ++y)
    {
        const uint16_t* d = donor16.ptr<uint16_t>(y);
        uint8_t* o        = idx8.ptr<uint8_t>(y);
        for (int x = 0; x < donor16.cols; ++x)
        {
            const uint16_t v = d[x];
            if (v == 0xFFFF) { o[x] = 0; continue; }
            o[x] = (uint8_t)(std::min<int>((int)v, N - 1));
        }
    }

    // Spread 0..N-1 across 0..255 so colormap uses distinct colors
    cv::Mat scaled8;
    if (N > 1)
        idx8.convertTo(scaled8, CV_8U, 255.0 / (double)(N - 1));
    else
        scaled8 = idx8.clone();

    cv::Mat colorBGR;
    cv::applyColorMap(scaled8, colorBGR, cv::COLORMAP_TURBO); // BGR8 output

    // Mask out invalid donors so they don't tint the image
    cv::Mat valid8 = (donor16 != (uint16_t)0xFFFF);
    valid8.convertTo(valid8, CV_8U, 255.0);
    cv::Mat invValid;
    cv::bitwise_not(valid8, invValid);
    colorBGR.setTo(cv::Scalar(0,0,0), invValid);

    return colorBGR;
}

// Overlay: baseGray8 (1ch) + band8 (1ch) + donorColorBGR8 (3ch)
static inline cv::Mat makeHaloDebugOverlayBGR8(const cv::Mat& baseGray8,
                                               const cv::Mat& band8,
                                               const cv::Mat& donorColorBGR8,
                                               float bandAlpha = 0.45f,
                                               float donorAlpha = 0.55f)
{
    CV_Assert(baseGray8.type() == CV_8U);
    CV_Assert(band8.empty() || band8.type() == CV_8U);
    CV_Assert(donorColorBGR8.empty() || donorColorBGR8.type() == CV_8UC3);

    cv::Mat baseBGR;
    cv::cvtColor(baseGray8, baseBGR, cv::COLOR_GRAY2BGR);

    cv::Mat out = baseBGR.clone();

    // Band overlay in yellow (BGR = 0,255,255)
    if (!band8.empty())
    {
        cv::Mat bandMask;
        cv::compare(band8, 0, bandMask, cv::CMP_GT); // 255 where band
        if (cv::countNonZero(bandMask) > 0)
        {
            cv::Mat yellow(out.size(), CV_8UC3, cv::Scalar(0,255,255));
            cv::Mat tmp;
            cv::addWeighted(out, 1.0f, yellow, bandAlpha, 0.0, tmp);
            tmp.copyTo(out, bandMask);
        }
    }

    // Donor overlay: blend only where donorColor is non-zero
    if (!donorColorBGR8.empty())
    {
        cv::Mat donorMask;
        cv::Mat gray;
        cv::cvtColor(donorColorBGR8, gray, cv::COLOR_BGR2GRAY);
        cv::compare(gray, 0, donorMask, cv::CMP_GT);

        if (cv::countNonZero(donorMask) > 0)
        {
            cv::Mat tmp;
            cv::addWeighted(out, 1.0f, donorColorBGR8, donorAlpha, 0.0, tmp);
            tmp.copyTo(out, donorMask);
        }
    }

    return out;
}

static inline bool hasNaNInf(const cv::Mat& m)
{
    if (m.empty()) return false;
    cv::Mat f;
    if (m.depth() == CV_32F || m.depth() == CV_64F) f = m;
    else m.convertTo(f, CV_32F);

    cv::Mat bad = (f != f); // NaN
    if (cv::countNonZero(bad.reshape(1)) > 0) return true;

    cv::Mat inf = (f == std::numeric_limits<float>::infinity()) |
                  (f == -std::numeric_limits<float>::infinity());
    if (cv::countNonZero(inf.reshape(1)) > 0) return true;

    return false;
}

static inline void dbgMat3Stats(const char* tag, const cv::Mat& m)
{
    qDebug().noquote() << tag
                       << "type=" << m.type()
                       << "depth=" << m.depth()
                       << "ch=" << m.channels()
                       << "size=" << m.cols << "x" << m.rows
                       << "empty=" << m.empty()
                       << "step=" << (qulonglong)m.step;

    if (m.empty()) return;

    // Mean for any channels
    cv::Scalar mean = cv::mean(m);
    qDebug().noquote() << tag << "mean =" << mean[0] << mean[1] << mean[2] << mean[3];

    // Per-channel min/max (if multichannel, split)
    if (m.channels() == 1)
    {
        double mn=0, mx=0;
        cv::minMaxLoc(m, &mn, &mx);
        qDebug().noquote() << tag << "min/max =" << mn << mx
                           << "NaN/Inf=" << hasNaNInf(m);
    }
    else
    {
        std::vector<cv::Mat> ch;
        cv::split(m, ch);
        for (int i = 0; i < (int)ch.size(); ++i)
        {
            double mn=0, mx=0;
            cv::minMaxLoc(ch[i], &mn, &mx);
            qDebug().noquote() << tag << "ch" << i
                               << "min/max =" << mn << mx
                               << "NaN/Inf=" << hasNaNInf(ch[i]);
        }
    }
}

static inline void dbgMaskStats(const char* tag, const cv::Mat& m)
{
    qDebug().noquote() << tag
                       << "type=" << m.type()
                       << "ch=" << m.channels()
                       << "size=" << m.cols << "x" << m.rows
                       << "empty=" << m.empty();

    if (m.empty()) return;

    cv::Mat m1 = (m.channels() == 1) ? m : m.reshape(1);
    int nz = cv::countNonZero(m1);
    double frac = (m1.total() > 0) ? (double)nz / (double)m1.total() : 0.0;
    qDebug().noquote() << tag << "nonzero =" << nz << "/" << (qulonglong)m1.total()
                       << "frac=" << frac;

    double mn=0, mx=0;
    cv::minMaxLoc(m1, &mn, &mx);
    qDebug().noquote() << tag << "min/max =" << mn << mx
                       << "NaN/Inf=" << hasNaNInf(m1);
}

static inline void dbgType(const char* tag, const cv::Mat& m)
{
    qDebug().noquote() << tag
                       << "type=" << m.type()
                       << "depth=" << m.depth()
                       << "ch=" << m.channels()
                       << "size=" << m.cols << "x" << m.rows
                       << "empty=" << m.empty();
}

static cv::Mat majorityFilterLabels16_3x3(const cv::Mat& in16)
{
    CV_Assert(in16.type() == CV_16U);

    cv::Mat out16 = in16.clone();

    const int H = in16.rows, W = in16.cols;

    for (int y = 1; y < H - 1; ++y)
    {
        const uint16_t* r0 = in16.ptr<uint16_t>(y - 1);
        const uint16_t* r1 = in16.ptr<uint16_t>(y);
        const uint16_t* r2 = in16.ptr<uint16_t>(y + 1);
        uint16_t* o = out16.ptr<uint16_t>(y);

        for (int x = 1; x < W - 1; ++x)
        {
            // 3x3 neighborhood labels
            uint16_t a[9] = {
                r0[x-1], r0[x], r0[x+1],
                r1[x-1], r1[x], r1[x+1],
                r2[x-1], r2[x], r2[x+1]
            };

            // Find the most frequent label in a[0..8]
            uint16_t best = a[0];
            int bestCount = 1;

            for (int i = 0; i < 9; ++i)
            {
                int cnt = 0;
                for (int j = 0; j < 9; ++j)
                    cnt += (a[j] == a[i]) ? 1 : 0;

                if (cnt > bestCount)
                {
                    bestCount = cnt;
                    best = a[i];
                }
            }
            o[x] = best;
        }
    }

    return out16;
}

static inline bool toColor32_01_FromLoaded(cv::Mat colorTmp,   // pass-by-value on purpose
                                           cv::Mat& color32,
                                           const QString& where,
                                           int sliceIdx = -1)
{
    color32.release();

    if (colorTmp.empty())
    {
        qWarning().noquote() << "WARNING:" << where << "empty colorTmp"
                             << (sliceIdx >= 0 ? QString("slice %1").arg(sliceIdx) : QString());
        return false;
    }

    // Match your main fusion logic exactly: only handle 1ch and 4ch explicitly.
    if (colorTmp.channels() == 1)
    {
        cv::Mat bgr;
        cv::cvtColor(colorTmp, bgr, cv::COLOR_GRAY2BGR);
        colorTmp = bgr;
    }
    else if (colorTmp.channels() == 4)
    {
        cv::Mat bgr;
        cv::cvtColor(colorTmp, bgr, cv::COLOR_BGRA2BGR);
        colorTmp = bgr;
    }
    else if (colorTmp.channels() != 3)
    {
        qWarning().noquote() << "WARNING:" << where << "unsupported channels"
                             << colorTmp.channels()
                             << (sliceIdx >= 0 ? QString("slice %1").arg(sliceIdx) : QString());
        return false;
    }

    // No BGR<->RGB swap here. Keep same channel order as returned by loader,
    // because your main fusion path assumes that is correct.

    if (colorTmp.depth() == CV_8U)
        colorTmp.convertTo(color32, CV_32FC3, 1.0 / 255.0);
    else if (colorTmp.depth() == CV_16U)
        colorTmp.convertTo(color32, CV_32FC3, 1.0 / 65535.0);
    else
    {
        qWarning().noquote() << "WARNING:" << where << "unsupported depth"
                             << colorTmp.depth()
                             << (sliceIdx >= 0 ? QString("slice %1").arg(sliceIdx) : QString());
        return false;
    }

    CV_Assert(color32.type() == CV_32FC3);
    return true;
}

// ------------------------------------------------------------
// pad helpers (engine-owned)
// ------------------------------------------------------------
static inline cv::Size computePadSizeForPyr(const cv::Size& s, int levels)
{
    const int lv = std::max(1, levels);
    const int m  = 1 << std::max(0, lv - 1);
    const int w  = ((s.width  + m - 1) / m) * m;
    const int h  = ((s.height + m - 1) / m) * m;
    return cv::Size(w, h);
}

static inline cv::Mat padCenterReflect(const cv::Mat& src, const cv::Size& dstSize)
{
    if (src.empty()) return cv::Mat();
    if (src.size() == dstSize) return src;

    const int padW = dstSize.width  - src.cols;
    const int padH = dstSize.height - src.rows;
    CV_Assert(padW >= 0 && padH >= 0);

    const int left   = padW / 2;
    const int right  = padW - left;
    const int top    = padH / 2;
    const int bottom = padH - top;

    cv::Mat dst;
    cv::copyMakeBorder(src, dst, top, bottom, left, right, cv::BORDER_REFLECT);
    return dst;
}

// counts horizontal label changes for a CV_16U map
static inline long long countHorizontalDiffs16(const cv::Mat& idx16)
{
    CV_Assert(idx16.type() == CV_16U);
    if (idx16.cols < 2) return 0;

    long long diff = 0;
    for (int y = 0; y < idx16.rows; ++y)
    {
        const uint16_t* r = idx16.ptr<uint16_t>(y);
        for (int x = 1; x < idx16.cols; ++x)
            diff += (r[x] != r[x - 1]) ? 1LL : 0LL;
    }
    return diff;
}

// ------------------------------------------------------------
// focus metric (dmap)
// ------------------------------------------------------------
static cv::Mat focusMetric32_dmap(const cv::Mat& gray8, float preBlurSigma, int lapKSize)
{
    CV_Assert(gray8.type() == CV_8U);
    cv::Mat g32 = FSFusion::toFloat01(gray8); // 0..1 CV_32F

    if (preBlurSigma > 0.0f)
        cv::GaussianBlur(g32, g32, cv::Size(0, 0),
                         preBlurSigma, preBlurSigma,
                         cv::BORDER_REFLECT);

    if (lapKSize != 3 && lapKSize != 5)
        lapKSize = 3;

    cv::Mat lap32;
    cv::Laplacian(g32, lap32, CV_32F, lapKSize);

    cv::Mat absLap32;
    cv::absdiff(lap32, cv::Scalar::all(0), absLap32);

    cv::GaussianBlur(absLap32, absLap32, cv::Size(0, 0),
                     0.8, 0.8,
                     cv::BORDER_REFLECT);

    return absLap32;
}

// ------------------------------------------------------------
// top-k maintenance
// ------------------------------------------------------------
static void updateTopKPerSlice(FSFusionDMap::TopKMaps& topk,
                               const cv::Mat& score32This,
                               uint16_t sliceIndex)
{
    CV_Assert(topk.K > 0);
    CV_Assert(!score32This.empty());
    CV_Assert(score32This.type() == CV_32F);
    CV_Assert(score32This.size() == topk.sz);

    const int K = topk.K;
    const int rows = topk.sz.height;
    const int cols = topk.sz.width;

    std::vector<float*>    sPtr(K);
    std::vector<uint16_t*> iPtr(K);

    for (int y = 0; y < rows; ++y)
    {
        const float* sNew = score32This.ptr<float>(y);

        for (int k = 0; k < K; ++k)
        {
            sPtr[k] = topk.score32[k].ptr<float>(y);
            iPtr[k] = topk.idx16[k].ptr<uint16_t>(y);
        }

        for (int x = 0; x < cols; ++x)
        {
            const float s = sNew[x];

            int ins = -1;
            for (int k = 0; k < K; ++k)
            {
                if (s > sPtr[k][x]) { ins = k; break; }
            }
            if (ins < 0) continue;

            for (int k = K - 1; k > ins; --k)
            {
                sPtr[k][x] = sPtr[k - 1][x];
                iPtr[k][x] = iPtr[k - 1][x];
            }

            sPtr[ins][x] = s;
            iPtr[ins][x] = sliceIndex;
        }
    }
}

// Keep only the largest connected component from a binary 8-bit mask (0/255).
// Returns a CV_8U 0/255 mask. If empty or no foreground, returns zeros.
static cv::Mat keepLargestCC(const cv::Mat& bin8, int connectivity = 8)
{
    CV_Assert(bin8.empty() || bin8.type() == CV_8U);
    if (bin8.empty()) return cv::Mat();

    cv::Mat src;
    cv::compare(bin8, 0, src, cv::CMP_GT); // 0/255
    if (cv::countNonZero(src) == 0)
        return cv::Mat::zeros(src.size(), CV_8U);

    cv::Mat labels, stats, centroids;
    int n = cv::connectedComponentsWithStats(src, labels, stats, centroids, connectivity, CV_32S);
    if (n <= 1) return cv::Mat::zeros(src.size(), CV_8U);

    int best = 1, bestArea = stats.at<int>(1, cv::CC_STAT_AREA);
    for (int i = 2; i < n; ++i)
    {
        int area = stats.at<int>(i, cv::CC_STAT_AREA);
        if (area > bestArea) { bestArea = area; best = i; }
    }

    cv::Mat out;
    cv::compare(labels, best, out, cv::CMP_EQ); // 0/255
    return out;
}

// ------------------------------------------------------------
// confidence from top-2
// ------------------------------------------------------------
static cv::Mat computeConfidence01_Top2(const FSFusionDMap::TopKMaps& topk, float eps)
{
    CV_Assert(topk.valid());
    if (topk.K < 2)
        return cv::Mat(topk.sz, CV_32F, cv::Scalar(1.0f));

    const cv::Mat& s0 = topk.score32[0];
    const cv::Mat& s1 = topk.score32[1];

    const float e = std::max(1e-12f, eps);
    cv::Mat conf = (s0 - s1) / (s0 + e);
    cv::max(conf, 0.0f, conf);
    cv::min(conf, 1.0f, conf);
    return conf;
}

static int pickForegroundSlice(const cv::Mat& win16,
                               const cv::Mat& conf01,
                               const cv::Mat& top1Score32,
                               const FSFusionDMap::DMapParams& p,
                               int N)
{
    CV_Assert(win16.type() == CV_16U);
    CV_Assert(conf01.type() == CV_32F);
    CV_Assert(top1Score32.type() == CV_32F);
    CV_Assert(win16.size() == conf01.size());
    CV_Assert(win16.size() == top1Score32.size());

    // If your stack ordering is near->far, "closest" is max index.
    if (p.haloUseMaxIndexAsForeground)
    {
        double mn = 0, mx = 0;
        cv::minMaxLoc(win16, &mn, &mx);
        int s = (int)mx;
        return (s >= 0 && s < N) ? s : 0;
    }

    // Otherwise: pick the slice that dominates high-confidence + high-texture pixels.
    // (More robust if ordering varies.)
    std::vector<int> hist(N, 0);

    // Build a crude "texture" from top1Score32 (already a Laplacian magnitude).
    double smn=0, smx=0;
    cv::minMaxLoc(top1Score32, &smn, &smx);
    float thr = (float)smx * 0.15f; // take top ~15% energy as "textured"
    if (thr <= 0) thr = 1e-6f;

    for (int y = 0; y < win16.rows; ++y)
    {
        const uint16_t* w = win16.ptr<uint16_t>(y);
        const float*    c = conf01.ptr<float>(y);
        const float*    s = top1Score32.ptr<float>(y);
        for (int x = 0; x < win16.cols; ++x)
        {
            const int idx = (int)w[x];
            if ((unsigned)idx >= (unsigned)N) continue;

            if (c[x] > 0.35f && s[x] > thr) // confident + textured
                hist[idx]++;
        }
    }

    int best = 0;
    for (int i = 1; i < N; ++i)
        if (hist[i] > hist[best]) best = i;

    return best;
}

// Choose sFg as the most common winner label inside confident pixels.
// This is MUCH more stable than "max label".
int pickForegroundSliceByConf(const cv::Mat& win16, const cv::Mat& conf01,
                              int N, float confThr, const QString& tag,
                              std::vector<int>* outHist /*=nullptr*/)
{
    CV_Assert(win16.type() == CV_16U);
    CV_Assert(conf01.type() == CV_32F);
    CV_Assert(win16.size() == conf01.size());

    std::vector<int> hist(N, 0);
    int used = 0;

    for (int y = 0; y < win16.rows; ++y)
    {
        const uint16_t* w = win16.ptr<uint16_t>(y);
        const float*    c = conf01.ptr<float>(y);
        for (int x = 0; x < win16.cols; ++x)
        {
            if (c[x] > confThr)
            {
                const int s = (int)w[x];
                if (0 <= s && s < N) { hist[s]++; used++; }
            }
        }
    }

    int bestS = 0, bestCnt = -1;
    for (int s = 0; s < N; ++s)
        if (hist[s] > bestCnt) { bestCnt = hist[s]; bestS = s; }

    qDebug() << tag << "pick sFg by confThr=" << confThr
             << " usedPx=" << used << " bestS=" << bestS << " bestCnt=" << bestCnt;

    std::vector<std::pair<int,int>> pairs;
    pairs.reserve(N);
    for (int s = 0; s < N; ++s) pairs.push_back({hist[s], s});
    std::sort(pairs.rbegin(), pairs.rend());
    qDebug() << tag << "top labels:";
    for (int i = 0; i < std::min(5, (int)pairs.size()); ++i)
        qDebug() << "  s" << pairs[i].second << "cnt" << pairs[i].first;

    if (outHist) *outHist = hist;
    return bestS;
}

// ------------------------------------------------------------
// weights containers
// ------------------------------------------------------------
struct DMapWeights
{
    std::vector<cv::Mat> w32; // K x CV_32F (orig size)

    cv::Mat boundary8;
    cv::Mat band8;

    cv::Mat overrideMask8;
    cv::Mat overrideWinner16;

    void create(cv::Size sz, int K)
    {
        w32.assign(K, cv::Mat(sz, CV_32F, cv::Scalar(0)));
        boundary8.release();
        band8.release();
        overrideMask8.release();
        overrideWinner16.release();
    }
};

static cv::Mat boundaryFromLabels8(const cv::Mat& lab16)
{
    CV_Assert(lab16.type() == CV_16U);
    cv::Mat b = cv::Mat::zeros(lab16.size(), CV_8U);

    for (int y = 0; y < lab16.rows; ++y)
    {
        const uint16_t* r  = lab16.ptr<uint16_t>(y);
        const uint16_t* ru = (y > 0) ? lab16.ptr<uint16_t>(y-1) : nullptr;
        uint8_t* out = b.ptr<uint8_t>(y);
        const uint16_t* rd = (y+1 < lab16.rows) ? lab16.ptr<uint16_t>(y+1) : nullptr;

        for (int x = 0; x < lab16.cols; ++x)
        {
            const uint16_t v = r[x];
            bool diff = false;
            if (x > 0) diff |= (v != r[x-1]);
            if (x+1 < lab16.cols) diff |= (v != r[x+1]);
            if (ru) diff |= (v != ru[x]);
            if (rd) diff |= (v != rd[x]);
            if (diff) out[x] = 255;
        }
    }
    return b;
}

static cv::Mat propagateDonorLabels16(const cv::Mat& seedMask8,
                                      const cv::Mat& seedLabel16,
                                      const cv::Mat& haloCand8,
                                      int maxSteps)
{
    CV_Assert(seedMask8.type() == CV_8U);
    CV_Assert(seedLabel16.type() == CV_16U);
    CV_Assert(haloCand8.type() == CV_8U);
    CV_Assert(seedMask8.size() == seedLabel16.size());
    CV_Assert(seedMask8.size() == haloCand8.size());

    maxSteps = std::max(1, maxSteps);

    const int H = seedMask8.rows;
    const int W = seedMask8.cols;

    // Donor slice id for each pixel (0xFFFF = unset)
    cv::Mat donor16(H, W, CV_16U, cv::Scalar(0xFFFF));
    // BFS distance (0xFFFF = unvisited)
    cv::Mat dist16(H, W, CV_16U, cv::Scalar(0xFFFF));

    std::deque<cv::Point> q;
    q.clear();

    // ------------------------------------------------------------
    // Seed initialization:
    // - only accept seeds that have a real donor label
    // - donor label is taken from seedLabel16 (NOT from win16)
    // ------------------------------------------------------------
    for (int y = 0; y < H; ++y)
    {
        const uint8_t* sm = seedMask8.ptr<uint8_t>(y);
        const uint16_t* sl = seedLabel16.ptr<uint16_t>(y);

        uint16_t* drow = donor16.ptr<uint16_t>(y);
        uint16_t* trow = dist16.ptr<uint16_t>(y);

        for (int x = 0; x < W; ++x)
        {
            if (!sm[x]) continue;

            const uint16_t lbl = sl[x];
            if (lbl == (uint16_t)0xFFFF) continue; // ignore "unknown" seeds

            drow[x] = lbl;
            trow[x] = 0;
            q.emplace_back(x, y);
        }
    }

    const int dx4[4] = { 1, -1, 0, 0 };
    const int dy4[4] = { 0, 0, 1, -1 };

    // ------------------------------------------------------------
    // BFS flood: only fill pixels that are in haloCand8
    // ------------------------------------------------------------
    while (!q.empty())
    {
        const cv::Point p = q.front();
        q.pop_front();

        const int x = p.x;
        const int y = p.y;

        const uint16_t d = donor16.at<uint16_t>(y, x);
        const uint16_t t = dist16.at<uint16_t>(y, x);

        if (t >= (uint16_t)maxSteps) continue;

        for (int k = 0; k < 4; ++k)
        {
            const int xx = x + dx4[k];
            const int yy = y + dy4[k];
            if ((unsigned)xx >= (unsigned)W || (unsigned)yy >= (unsigned)H) continue;

            // Only fill halo candidate pixels (this is your "domain")
            if (!haloCand8.at<uint8_t>(yy, xx)) continue;

            uint16_t& dstT = dist16.at<uint16_t>(yy, xx);
            if (dstT != (uint16_t)0xFFFF) continue; // visited already

            donor16.at<uint16_t>(yy, xx) = d;
            dstT = (uint16_t)(t + 1);
            q.emplace_back(xx, yy);
        }
    }

    return donor16;
}

// softmax weights from topk scores
static void buildTopKWeightsSoftmax(const FSFusionDMap::TopKMaps& topk,
                                    const FSFusionDMap::DMapParams& p,
                                    DMapWeights& weightsOut)
{
    CV_Assert(topk.valid());
    const int K = topk.K;
    CV_Assert(K >= 1 && K <= 16);

    weightsOut.create(topk.sz, K);

    const float temp = std::max(1e-6f, p.softTemp);
    const float wMin = std::max(0.0f, p.wMin);

    std::vector<const float*> sPtr(K);
    std::vector<float*>       wPtr(K);

    for (int y = 0; y < topk.sz.height; ++y)
    {
        for (int k = 0; k < K; ++k)
        {
            sPtr[k] = topk.score32[k].ptr<float>(y);
            wPtr[k] = weightsOut.w32[k].ptr<float>(y);
        }

        for (int x = 0; x < topk.sz.width; ++x)
        {
            float sMax = sPtr[0][x];
            for (int k = 1; k < K; ++k) sMax = std::max(sMax, sPtr[k][x]);

            float e[16];
            float sum = 0.0f;
            for (int k = 0; k < K; ++k)
            {
                float z = (sPtr[k][x] - sMax) / temp;
                z = std::max(-80.0f, std::min(80.0f, z));
                e[k] = std::exp(z);
                sum += e[k];
            }

            if (sum <= 0.0f)
            {
                for (int k = 0; k < K; ++k) wPtr[k][x] = 0.0f;
                wPtr[0][x] = 1.0f;
                continue;
            }

            float sum2 = 0.0f;
            for (int k = 0; k < K; ++k)
            {
                float w = e[k] / sum;
                if (wMin > 0.0f) w = std::max(w, wMin);
                wPtr[k][x] = w;
                sum2 += w;
            }

            const float inv = (sum2 > 0.0f) ? (1.0f / sum2) : 1.0f;
            for (int k = 0; k < K; ++k) wPtr[k][x] *= inv;
        }
    }
}

// boundary/band gated by low confidence pixels
static void buildBoundaryAndBandFromUncertainty(const cv::Mat& top1Idx16,
                                                const cv::Mat& lowConf8,   // 0/255
                                                int bandDilatePx,
                                                cv::Mat& boundary8,
                                                cv::Mat& band8)
{
    CV_Assert(top1Idx16.type() == CV_16U);
    CV_Assert(lowConf8.type() == CV_8U);
    CV_Assert(lowConf8.size() == top1Idx16.size());

    boundary8 = cv::Mat::zeros(top1Idx16.size(), CV_8U);

    const int rows = top1Idx16.rows;
    const int cols = top1Idx16.cols;

    for (int y = 0; y < rows; ++y)
    {
        const uint16_t* row   = top1Idx16.ptr<uint16_t>(y);
        const uint16_t* rowUp = (y > 0) ? top1Idx16.ptr<uint16_t>(y - 1) : nullptr;
        const uint16_t* rowDn = (y + 1 < rows) ? top1Idx16.ptr<uint16_t>(y + 1) : nullptr;

        const uint8_t*  lc    = lowConf8.ptr<uint8_t>(y);
        uint8_t* out          = boundary8.ptr<uint8_t>(y);

        for (int x = 0; x < cols; ++x)
        {
            if (!lc[x]) continue; // only build boundary in uncertain pixels

            const uint16_t v = row[x];
            bool diff = false;
            if (x > 0)        diff |= (v != row[x - 1]);
            if (x + 1 < cols) diff |= (v != row[x + 1]);
            if (rowUp)        diff |= (v != rowUp[x]);
            if (rowDn)        diff |= (v != rowDn[x]);

            if (diff) out[x] = 255;
        }
    }

    band8 = boundary8.clone();
    if (bandDilatePx > 0)
        cv::dilate(band8, band8, FSFusion::seEllipse(bandDilatePx));
}

// boundary/band gated by reliable pixels
static void buildBoundaryAndBandGated(const cv::Mat& top1Idx16,
                                      const cv::Mat& reliable8,
                                      int bandDilatePx,
                                      cv::Mat& boundary8,
                                      cv::Mat& band8)
{
    CV_Assert(top1Idx16.type() == CV_16U);
    CV_Assert(reliable8.empty() || (reliable8.type() == CV_8U && reliable8.size() == top1Idx16.size()));

    boundary8 = cv::Mat::zeros(top1Idx16.size(), CV_8U);

    const int rows = top1Idx16.rows;
    const int cols = top1Idx16.cols;

    for (int y = 0; y < rows; ++y)
    {
        const uint16_t* row   = top1Idx16.ptr<uint16_t>(y);
        const uint16_t* rowUp = (y > 0) ? top1Idx16.ptr<uint16_t>(y - 1) : nullptr;
        const uint16_t* rowDn = (y + 1 < rows) ? top1Idx16.ptr<uint16_t>(y + 1) : nullptr;

        const uint8_t* rel   = reliable8.empty() ? nullptr : reliable8.ptr<uint8_t>(y);
        const uint8_t* relUp = (reliable8.empty() || y == 0) ? nullptr : reliable8.ptr<uint8_t>(y - 1);
        const uint8_t* relDn = (reliable8.empty() || y + 1 >= rows) ? nullptr : reliable8.ptr<uint8_t>(y + 1);

        uint8_t* out = boundary8.ptr<uint8_t>(y);

        for (int x = 0; x < cols; ++x)
        {
            if (rel)
            {
                const bool anyReliable =
                    (rel[x] != 0) ||
                    (x > 0        && rel[x - 1] != 0) ||
                    (x + 1 < cols && rel[x + 1] != 0) ||
                    (relUp && relUp[x] != 0) ||
                    (relDn && relDn[x] != 0);

                if (anyReliable) continue;
            }

            const uint16_t v = row[x];
            bool diff = false;
            if (x > 0)        diff |= (v != row[x - 1]);
            if (x + 1 < cols) diff |= (v != row[x + 1]);
            if (rowUp)        diff |= (v != rowUp[x]);
            if (rowDn)        diff |= (v != rowDn[x]);

            if (diff) out[x] = 255;
        }
    }

    band8 = boundary8.clone();
    if (bandDilatePx > 0)
        cv::dilate(band8, band8, FSFusion::seEllipse(bandDilatePx));
}

static void buildDMapWeightsFromTopK(const FSFusionDMap::TopKMaps& topk,
                                     const FSFusionDMap::DMapParams& p,
                                     DMapWeights& W)
{
    CV_Assert(topk.valid());
    buildTopKWeightsSoftmax(topk, p, W);

    cv::Mat conf01 = computeConfidence01_Top2(topk, p.confEps);


    cv::Mat lowConf = (conf01 < p.confLow);
    cv::Mat lowConf8;
    lowConf.convertTo(lowConf8, CV_8U, 255.0);

    buildBoundaryAndBandFromUncertainty(topk.idx16[0], lowConf8,
                                        std::max(0, p.bandDilatePx),
                                        W.boundary8,
                                        W.band8);

    double smn = 0.0, smx = 0.0;
    cv::minMaxLoc(topk.score32[0], &smn, &smx);

    const float scoreThr = float(smx) * 0.005f;

    cv::Mat strong = (topk.score32[0] > scoreThr);
    strong.convertTo(strong, CV_8U, 255.0);

    cv::Mat confident = (conf01 > p.confHigh);
    confident.convertTo(confident, CV_8U, 255.0);

    cv::Mat reliable8;
    cv::bitwise_or(strong, confident, reliable8);

    // buildBoundaryAndBandGated(topk.idx16[0], reliable8,
    //                           std::max(0, p.bandDilatePx),
    //                           W.boundary8,
    //                           W.band8);
}

static cv::Mat buildSliceWeightMap32(const FSFusionDMap::TopKMaps& topk,
                                     const DMapWeights& W,
                                     int sliceIdx)
{
    CV_Assert(topk.valid());
    CV_Assert((int)W.w32.size() == topk.K);

    cv::Mat w32(topk.sz, CV_32F, cv::Scalar(0));

    for (int k = 0; k < topk.K; ++k)
    {
        cv::Mat m = (topk.idx16[k] == (uint16_t)sliceIdx);
        cv::Mat mk;
        m.convertTo(mk, CV_32F, 1.0 / 255.0);

        cv::Mat tmp;
        cv::multiply(W.w32[k], mk, tmp);
        w32 += tmp;
    }

    if (!W.overrideMask8.empty() && !W.overrideWinner16.empty())
    {
        cv::Mat overrideBin;
        cv::compare(W.overrideMask8, 0, overrideBin, cv::CMP_GT);

        cv::Mat isMe = (W.overrideWinner16 == (uint16_t)sliceIdx);

        cv::Mat m;
        cv::bitwise_and(isMe, overrideBin, m);
        w32.setTo(1.0f, m);

        cv::Mat notIsMe, notMe;
        cv::bitwise_not(isMe, notIsMe);
        cv::bitwise_and(overrideBin, notIsMe, notMe);
        w32.setTo(0.0f, notMe);
    }

    return w32;
}

static cv::Mat buildDepthJumpEdges8(const cv::Mat& win16, int jumpThr, int dilatePx = 1)
{
    CV_Assert(win16.type() == CV_16U);
    jumpThr = std::max(1, jumpThr);

    cv::Mat edges = cv::Mat::zeros(win16.size(), CV_8U);

    for (int y = 0; y < win16.rows; ++y)
    {
        const uint16_t* r = win16.ptr<uint16_t>(y);
        uint8_t* e = edges.ptr<uint8_t>(y);

        for (int x = 0; x < win16.cols; ++x)
        {
            const uint16_t v = r[x];
            bool jump = false;

            if (x > 0)             jump |= (std::abs((int)v - (int)r[x-1]) >= jumpThr);
            if (x+1 < win16.cols)  jump |= (std::abs((int)v - (int)r[x+1]) >= jumpThr);

            if (y > 0)
            {
                const uint16_t* ru = win16.ptr<uint16_t>(y-1);
                jump |= (std::abs((int)v - (int)ru[x]) >= jumpThr);
            }
            if (y+1 < win16.rows)
            {
                const uint16_t* rd = win16.ptr<uint16_t>(y+1);
                jump |= (std::abs((int)v - (int)rd[x]) >= jumpThr);
            }

            if (jump) e[x] = 255;
        }
    }

    if (dilatePx > 0)
        cv::dilate(edges, edges, FSFusion::seEllipse(dilatePx));

    return edges;
}

static void buildForegroundBoundarySeeds(const cv::Mat& win16,       // CV_16U
                                         const cv::Mat& boundary8,   // CV_8U 0/255
                                         cv::Mat& seeds8,            // CV_8U 0/255 out
                                         cv::Mat& seedLabel16,       // CV_16U out (0xFFFF unset)
                                         int edgeZoneDilatePx = 2)   // 1..3 typically
{
    CV_Assert(win16.type() == CV_16U);
    CV_Assert(boundary8.type() == CV_8U);
    CV_Assert(win16.size() == boundary8.size());

    const int H = win16.rows;
    const int W = win16.cols;

    seeds8 = cv::Mat::zeros(win16.size(), CV_8U);
    seedLabel16 = cv::Mat(win16.size(), CV_16U, cv::Scalar(0xFFFF));

    // Make a slightly thicker zone around the boundary to allow seeds to sit on the FG side.
    cv::Mat edgeZone8 = boundary8.clone();
    if (edgeZoneDilatePx > 0)
        cv::dilate(edgeZone8, edgeZone8, FSFusion::seEllipse(edgeZoneDilatePx));

    // For each boundary pixel, pick the max label across its 4-neighborhood.
    // Then place seeds ON pixels that actually have that max label (FG side).
    for (int y = 1; y < H - 1; ++y)
    {
        const uint8_t* bRow = boundary8.ptr<uint8_t>(y);
        for (int x = 1; x < W - 1; ++x)
        {
            if (!bRow[x]) continue;

            // collect candidate labels around the boundary pixel
            const uint16_t c  = win16.at<uint16_t>(y, x);
            const uint16_t l  = win16.at<uint16_t>(y, x - 1);
            const uint16_t r  = win16.at<uint16_t>(y, x + 1);
            const uint16_t u  = win16.at<uint16_t>(y - 1, x);
            const uint16_t d  = win16.at<uint16_t>(y + 1, x);

            const uint16_t fg = std::max(std::max(c, l), std::max(r, std::max(u, d)));

            // mark seed pixels on the FG side (pixels whose label == fg)
            auto trySeed = [&](int yy, int xx)
            {
                if (!edgeZone8.at<uint8_t>(yy, xx)) return;
                if (win16.at<uint16_t>(yy, xx) != fg) return;

                seeds8.at<uint8_t>(yy, xx) = 255;
                seedLabel16.at<uint16_t>(yy, xx) = fg;
            };

            trySeed(y, x);
            trySeed(y, x - 1);
            trySeed(y, x + 1);
            trySeed(y - 1, x);
            trySeed(y + 1, x);
        }
    }
}

static cv::Mat buildHaloRingAlpha01(const cv::Mat& win16,
                                    int sFg,
                                    int ringPx,
                                    float featherSigma)
{
    CV_Assert(win16.type() == CV_16U);
    ringPx = std::max(1, ringPx);

    cv::Mat fg = (win16 == (uint16_t)sFg);
    fg.convertTo(fg, CV_8U, 255.0);

    // Dilate foreground outward
    cv::Mat fgDil = fg.clone();
    cv::dilate(fgDil, fgDil, FSFusion::seEllipse(ringPx));

    // Ring is outside the fg but within dilation
    cv::Mat ring;
    cv::bitwise_and(fgDil, ~fg, ring); // 255 where ring

    if (cv::countNonZero(ring) == 0)
        return cv::Mat();

    // Distance from foreground (inside ring only)
    // distanceTransform expects 0 as features; so invert fg.
    cv::Mat invFg;
    cv::compare(fg, 0, invFg, cv::CMP_EQ); // 255 where NOT fg

    cv::Mat dist32;
    cv::distanceTransform(invFg, dist32, cv::DIST_L2, 3); // float pixels

    // Alpha: 1 at fg edge -> 0 at ringPx
    cv::Mat alpha = 1.0f - (dist32 / (float)ringPx);
    cv::max(alpha, 0.0f, alpha);
    cv::min(alpha, 1.0f, alpha);

    // Zero alpha outside ring
    cv::Mat ring01;
    ring.convertTo(ring01, CV_32F, 1.0 / 255.0);
    alpha = alpha.mul(ring01);

    // Feather smoothing
    if (featherSigma > 0.0f)
        cv::GaussianBlur(alpha, alpha, cv::Size(0,0), featherSigma, featherSigma, cv::BORDER_REFLECT);

    cv::max(alpha, 0.0f, alpha);
    cv::min(alpha, 1.0f, alpha);
    return alpha; // CV_32F 0..1
}

static cv::Mat buildHaloEligibleMask01(const cv::Mat& conf01,
                                       const cv::Mat& top1Score32,
                                       float confMax,
                                       int texMax8u)
{
    CV_Assert(conf01.type() == CV_32F);
    CV_Assert(top1Score32.type() == CV_32F);
    CV_Assert(conf01.size() == top1Score32.size());

    // low confidence
    cv::Mat lowConf = (conf01 < confMax);
    lowConf.convertTo(lowConf, CV_8U, 255.0);

    // low texture: map score to an 8U-ish scale for thresholding
    // Use robust scaling (p85-ish) if you have it; for now do max-based.
    double smn=0, smx=0;
    cv::minMaxLoc(top1Score32, &smn, &smx);
    float scale = (smx > 1e-12) ? (255.0f / (float)smx) : 1.0f;

    cv::Mat tex8;
    top1Score32.convertTo(tex8, CV_8U, scale);
    cv::Mat lowTex = (tex8 <= texMax8u);
    lowTex.convertTo(lowTex, CV_8U, 255.0);

    cv::Mat eligible8;
    cv::bitwise_and(lowConf, lowTex, eligible8);

    cv::Mat eligible01;
    eligible8.convertTo(eligible01, CV_32F, 1.0 / 255.0);
    return eligible01; // 0..1
}

static void applyHaloRingFix(cv::Mat& out32,                 // CV_32FC3 0..1
                             int sFg,
                             const cv::Mat& alphaRing01,     // CV_32F 0..1
                             const cv::Mat& eligible01,      // CV_32F 0..1
                             const cv::Mat& fgColor32)       // CV_32FC3 0..1
{
    CV_Assert(out32.type() == CV_32FC3);
    CV_Assert(fgColor32.type() == CV_32FC3);
    CV_Assert(out32.size() == fgColor32.size());
    CV_Assert(alphaRing01.type() == CV_32F);
    CV_Assert(eligible01.type() == CV_32F);
    CV_Assert(alphaRing01.size() == out32.size());
    CV_Assert(eligible01.size() == out32.size());

    // alpha = ringAlpha * eligible
    cv::Mat alpha = alphaRing01.mul(eligible01);

    if (cv::countNonZero(alpha > 1e-6f) == 0)
        return;

    // Expand alpha to 3 channels for blending
    cv::Mat alpha3;
    cv::Mat ch[] = { alpha, alpha, alpha };
    cv::merge(ch, 3, alpha3);

    // Avoid blue tint
    cv::Mat invA3;
    cv::subtract(cv::Scalar::all(1.0f), alpha3, invA3);
    out32 = out32.mul(invA3) + fgColor32.mul(alpha3);
}

} // namespace

// =============================================================================
// toponly helpers (member impls)
// =============================================================================

void FSFusionDMap::TopKMaps::reset()
{
    K = 0;
    sz = {};
    idx16.clear();
    score32.clear();
}

void FSFusionDMap::TopKMaps::create(cv::Size s, int k)
{
    reset();
    sz = s;
    K = std::max(0, k);
    idx16.resize(K);
    score32.resize(K);

    for (int i = 0; i < K; ++i)
    {
        idx16[i]   = cv::Mat(sz, CV_16U, cv::Scalar(0));
        score32[i] = cv::Mat(sz, CV_32F, cv::Scalar(-1.0f)); // so first slice always inserts
    }
}

bool FSFusionDMap::TopKMaps::valid() const
{
    if (K <= 0) return false;
    if (sz.width <= 0 || sz.height <= 0) return false;
    if ((int)idx16.size() != K || (int)score32.size() != K) return false;

    for (int i = 0; i < K; ++i)
    {
        if (idx16[i].empty() || score32[i].empty()) return false;
        if (idx16[i].type() != CV_16U) return false;
        if (score32[i].type() != CV_32F) return false;
        if (idx16[i].size() != sz) return false;
        if (score32[i].size() != sz) return false;
    }
    return true;
}

// =============================================================================
// FSFusionDMap
// =============================================================================

FSFusionDMap::FSFusionDMap()
{
    reset();
}

void FSFusionDMap::reset()
{
    active_ = false;
    sliceCount_ = 0;

    // base geometry
    resetGeometry();

    topk_pad_.reset();
}

// -----------------------------------------------------------------------------
// streamSlice (pass-1)
// -----------------------------------------------------------------------------
bool FSFusionDMap::streamSlice(int slice,
                               const cv::Mat& grayAlign8,   // CV_8U ALIGN
                               const cv::Mat& colorAlign,   // ALIGN (optional)
                               const Options & /*opt*/,
                               std::atomic_bool* abortFlag,
                               StatusCallback /*statusCb*/,
                               ProgressCallback /*progressCb*/)
{
    const QString srcFun = "fsfusiondmap::streamSlice";

    if (FSFusion::isAbort(abortFlag)) return false;

    if (grayAlign8.empty() || grayAlign8.type() != CV_8U)
    {
        qWarning().noquote() << "WARNING:" << srcFun << "grayAlign8 empty or not CV_8U.";
        return false;
    }

    // slice-0 init expects caller has set alignSize/validAreaAlign/origSize
    if (slice == 0)
    {
        if (alignSize.width <= 0 || alignSize.height <= 0)
        {
            qWarning().noquote() << "WARNING:" << srcFun << "alignSize not set by caller.";
            return false;
        }

        // decide pad size from align + pyrLevels
        params.topK = std::max(1, std::min(8, params.topK));
        const int levelsForPad = std::max(1, params.pyrLevels);

        padSize = computePadSizeForPyr(alignSize, levelsForPad);

        // outDepth from input color if provided; else keep existing base outDepth
        if (!colorAlign.empty())
            outDepth = (colorAlign.depth() == CV_16U) ? CV_16U : CV_8U;

        topk_pad_.create(padSize, params.topK);

        active_ = true;
        sliceCount_ = 0;
    }
    else
    {
        if (!active_ || !topk_pad_.valid())
        {
            qWarning().noquote() << "WARNING:" << srcFun << "called without active state.";
            return false;
        }
    }

    if (grayAlign8.size() != alignSize)
    {
        qWarning().noquote() << "WARNING:" << srcFun
                             << "alignSize mismatch. expected"
                             << cvSizeToText(alignSize)
                             << "got" << cvSizeToText(grayAlign8.size());
        return false;
    }

    // pad ALIGN -> PAD
    cv::Mat grayPad8 = padCenterReflect(grayAlign8, padSize);
    CV_Assert(grayPad8.type() == CV_8U && grayPad8.size() == padSize);

    // focus metric in PAD space
    cv::Mat score32 = focusMetric32_dmap(grayPad8, params.scoreSigma, params.scoreKSize);

    // update top-k in PAD space
    updateTopKPerSlice(topk_pad_, score32, (uint16_t)std::max(0, slice));

    sliceCount_++;
    return true;
}

// -----------------------------------------------------------------------------
// streamFinish (pass-2)
// -----------------------------------------------------------------------------
bool FSFusionDMap::streamFinish(cv::Mat& outputColor,
                                const FSFusion::Options& opt,
                                cv::Mat& depthIndex16,
                                const QStringList& inputPaths,
                                const std::vector<Result>& globals,
                                std::atomic_bool* abortFlag,
                                FSFusion::StatusCallback statusCb,
                                FSFusion::ProgressCallback progressCb)
{
    const QString srcFun = "fsfusiondmap::streamFinish";
    if (G::FSLog) G::log(srcFun);

    if (!active_ || !topk_pad_.valid())
    {
        qWarning().noquote() << "WARNING:" << srcFun << "state not initialized.";
        return false;
    }

    const int N = inputPaths.size();
    if (N <= 0 || (int)globals.size() != N)
    {
        qWarning().noquote() << "WARNING:" << srcFun << "inputPaths/globals size mismatch.";
        return false;
    }

    if (FSFusion::isAbort(abortFlag)) return false;

    if (!hasValidGeometry())
    {
        qWarning().noquote() << "WARNING:" << srcFun
                             << "missing/invalid geometry:" << geometryText();
        return false;
    }

    // ---- PAD -> ALIGN crop rect ----
    cv::Rect roiPadToAlign(0, 0, alignSize.width, alignSize.height);
    if (padSize != alignSize)
    {
        const int padW = padSize.width  - alignSize.width;
        const int padH = padSize.height - alignSize.height;
        if (padW < 0 || padH < 0)
        {
            qWarning().noquote() << "WARNING:" << srcFun << "padSize smaller than alignSize.";
            return false;
        }

        roiPadToAlign.x = padW / 2;
        roiPadToAlign.y = padH / 2;
    }

    if (!FSFusion::rectInside(roiPadToAlign, topk_pad_.sz))
    {
        qWarning().noquote() << "WARNING:" << srcFun << "roiPadToAlign out of bounds for PAD maps.";
        return false;
    }

    const cv::Size alignSz(roiPadToAlign.width, roiPadToAlign.height);
    if (!FSFusion::rectInside(validAreaAlign, alignSz))
    {
        qWarning().noquote() << "WARNING:" << srcFun << "validAreaAlign out of bounds for ALIGN crop.";
        return false;
    }

    const cv::Size origSz(validAreaAlign.width, validAreaAlign.height);
    origSize = origSz;

    // ---- Crop TopK maps: PAD -> ALIGN -> ORIG ----
    TopKMaps topkOrig;
    topkOrig.create(origSz, topk_pad_.K);

    for (int k = 0; k < topk_pad_.K; ++k)
    {
        cv::Mat idxAlign = topk_pad_.idx16[k](roiPadToAlign);
        cv::Mat scAlign  = topk_pad_.score32[k](roiPadToAlign);

        topkOrig.idx16[k]   = idxAlign(validAreaAlign).clone();
        topkOrig.score32[k] = scAlign(validAreaAlign).clone();
    }

    // sanity: top-1 diffs
    {
        const cv::Mat& d = topkOrig.idx16[0];
        const long long diffCount = countHorizontalDiffs16(d);
        const long long total = (long long)d.rows * (d.cols - 1);
        qDebug() << "top1 diffs (ORIG, pre-weight) =" << diffCount << "/" << total
                 << " frac=" << (double)diffCount / (double)total;
    }

    // sanity: top-1 index range
    {
        double idxMin = 0, idxMax = 0;
        cv::minMaxLoc(topkOrig.idx16[0], &idxMin, &idxMax);
        qDebug() << "Top1 idx min/max =" << idxMin << idxMax << " N=" << N;

        if (idxMin < 0 || idxMax >= N)
        {
            qWarning().noquote() << "WARNING:" << srcFun
                                 << "Top1 idx out of range: min/max="
                                 << idxMin << idxMax << " N=" << N;
            return false;
        }
    }

    // ---- build weights ----
    DMapWeights W;
    buildDMapWeightsFromTopK(topkOrig, params, W);

    // ---- confidence map ----
    cv::Mat conf01 = computeConfidence01_Top2(topkOrig, params.confEps);

    // ---- winner map + light stabilization ----
    cv::Mat win16 = topkOrig.idx16[0].clone();
    const long long before = countHorizontalDiffs16(win16);

    if (topkOrig.K >= 2)
    {
        cv::Mat lowConf8 = (conf01 < params.confLow);
        lowConf8.convertTo(lowConf8, CV_8U, 255.0);

        if (params.uncertaintyDilatePx > 0)
            cv::dilate(lowConf8, lowConf8, FSFusion::seEllipse(params.uncertaintyDilatePx));

        if (win16.cols >= 5 && win16.rows >= 5 && cv::countNonZero(lowConf8) > 0)
        {
            cv::Mat med;
            cv::medianBlur(win16, med, 5);
            med.copyTo(win16, lowConf8);
        }
    }

    {
        const long long after = countHorizontalDiffs16(win16);
        const long long total = (long long)win16.rows * (win16.cols - 1);
        qDebug() << "win16 diffs AFTER mode (masked) =" << after << "/" << total
                 << " (before=" << before << ")";
    }

    depthIndex16 = win16.clone();
    if (FSFusion::isAbort(abortFlag)) return false;

    // ---- pyramid levels ----
    int levels = params.pyrLevels;
    if (levels <= 0)
    {
        int dim = std::max(origSz.width, origSz.height);
        levels = 5;
        while ((dim >> levels) > 8 && levels < 8) levels++;
    }
    levels = std::max(3, std::min(10, levels));

    // ---- build index pyramid ----
    std::vector<cv::Mat> idxPyr16;
    FusionPyr::buildIndexPyrNearest(depthIndex16, levels, idxPyr16);

    // ---- optional veto pyramid ----
    std::vector<cv::Mat> vetoPyr8;
    if (params.enableDepthGradLowpassVeto)
    {
        FusionPyr::DepthEdgeVetoParams vp;
        vp.vetoFromLevel   = params.vetoFromLevel;
        vp.depthGradThresh = params.depthGradThresh;
        vp.vetoDilatePx    = params.vetoDilatePx;

        FusionPyr::buildDepthEdgeVetoPyr(idxPyr16, vetoPyr8, vp);
    }

    // ---- accumulator ----
    FusionPyr::PyrAccum A;
    A.reset(origSz, levels);

    cv::Mat sumW(origSz, CV_32F, cv::Scalar(0));

    // ---- pass-2: stream slices ----
    for (int s = 0; s < N; ++s)
    {
        progressCb();
        QString ss = " Slice: " + QString::number(s) + " of " + QString::number(N) + " ";
        statusCb("Fusing" + ss);

        if (FSFusion::isAbort(abortFlag)) return false;

        cv::Mat grayTmp, colorTmp;
        if (!FSLoader::loadAlignedSliceOrig(s, inputPaths, globals,
                                            roiPadToAlign, validAreaAlign,
                                            grayTmp, colorTmp))
        {
            qWarning().noquote() << "WARNING:" << srcFun
                                 << "loadAlignedSliceOrig failed for slice" << s;
            return false;
        }

        if (colorTmp.empty())
        {
            qWarning().noquote() << "WARNING:" << srcFun << "colorTmp empty for slice" << s;
            return false;
        }

        cv::Mat color32;
        if (!toColor32_01_FromLoaded(colorTmp, color32, srcFun, s))
            return false;

        CV_Assert(color32.size() == origSz);

        cv::Mat w32 = buildSliceWeightMap32(topkOrig, W, s);
        CV_Assert(w32.type() == CV_32F && w32.size() == origSz);

        sumW += w32;

        FusionPyr::AccumDMapParams ap;
        ap.enableHardWeightsOnLowpass = params.enableHardWeightsOnLowpass;
        ap.enableDepthGradLowpassVeto = params.enableDepthGradLowpassVeto;
        ap.hardFromLevel = params.hardFromLevel;
        ap.vetoFromLevel = params.vetoFromLevel;
        ap.vetoStrength  = params.vetoStrength;
        ap.wMin          = params.wMin;

        FusionPyr::accumulateSlicePyr(A,
                                      color32,
                                      w32,
                                      idxPyr16,
                                      params.enableDepthGradLowpassVeto ? &vetoPyr8 : nullptr,
                                      s,
                                      ap,
                                      levels,
                                      params.weightBlurSigma);
    }

    cv::Mat out32 = FusionPyr::finalizeBlend(A, /*eps=*/1e-8f);

    cv::max(out32, 0.0f, out32);
    cv::min(out32, 1.0f, out32);

    // ---- Keep: halo-relevant inputs (NOT blue debugging) ----
    // If you want to reduce further, keep only conf01 + top1Score32.
    dbgMat3Stats("DBG out32 preHalo clamp", out32);
    dbgMat3Stats("DBG top1Score32", topkOrig.score32[0]);
    dbgMat3Stats("DBG conf01", conf01);

    // --- Build a stable label map for boundary detection ---
    cv::Mat winStable16 = depthIndex16.clone();

    cv::Mat unreliable8 = (conf01 < params.confHigh);
    unreliable8.convertTo(unreliable8, CV_8U, 255.0);

    if (cv::countNonZero(unreliable8) > 0)
    {
        cv::Mat filt16 = majorityFilterLabels16_3x3(winStable16);
        filt16.copyTo(winStable16, unreliable8);
    }

    // ---------------------------------------------------------------------
    // Halo fix (LOCAL donor slice follows depth map)
    // ---------------------------------------------------------------------
    do {
        if (!params.enableHaloRingFix) break;

        const int ringPx = std::max(2, params.haloRingPx);

        // Foreground slice pick (and get histogram of confident winner labels)
        std::vector<int> hist;
        int seedS = pickForegroundSliceByConf(winStable16, conf01, N,
                                              params.confHigh, "HaloFix", &hist);

        // Sort labels by histogram count (desc)
        std::vector<std::pair<int,int>> pairs;
        pairs.reserve(N);
        for (int s = 0; s < N; ++s) pairs.push_back({hist[s], s});
        std::sort(pairs.rbegin(), pairs.rend());

        auto getCnt = [&](int s)->int { return (s >= 0 && s < N) ? hist[s] : 0; };

        // Evaluate top M candidates by resulting FG area after the SAME early gates
        // (prevents “reversed order” and “wrong slice chosen” from collapsing fg8)
        const int M = 6;

        int bestS = pairs.empty() ? 0 : pairs[0].second;
        int bestLo = bestS, bestHi = bestS;
        int bestArea = -1;

        for (int i = 0; i < std::min(M, (int)pairs.size()); ++i)
        {
            const int sCand = pairs[i].second;

            // Build a label range around sCand (same neighbor-expansion idea)
            int loCand = sCand, hiCand = sCand;
            const int base = std::max(1, getCnt(sCand));
            const float keepFrac = 0.55f;
            const int maxExpand  = 3;

            for (int step = 1; step <= maxExpand; ++step)
            {
                const int L = sCand - step;
                const int R = sCand + step;
                if (L >= 0 && getCnt(L) >= (int)(keepFrac * base)) loCand = L;
                if (R <  N && getCnt(R) >= (int)(keepFrac * base)) hiCand = R;
            }

            // Quick FG candidate scored the same way your real FG will start:
            // range -> confLow gate -> largest CC
            cv::Mat fgCand8 = (winStable16 >= (uint16_t)loCand) & (winStable16 <= (uint16_t)hiCand);
            fgCand8.convertTo(fgCand8, CV_8U, 255.0);

            cv::Mat confOK = (conf01 > params.confLow);
            confOK.convertTo(confOK, CV_8U, 255.0);
            cv::bitwise_and(fgCand8, confOK, fgCand8);

            fgCand8 = keepLargestCC(fgCand8, 8);
            const int area = cv::countNonZero(fgCand8);

            qDebug() << "HaloFix cand:"
                     << " s=" << sCand
                     << " lo/hi=" << loCand << hiCand
                     << " area=" << area;

            if (area > bestArea)
            {
                bestArea = area;
                bestS = sCand;
                bestLo = loCand;
                bestHi = hiCand;
            }
        }

        int lo = bestLo;
        int hi = bestHi;

        qDebug() << "HaloFix chosen by area:"
                 << " seedS=" << seedS
                 << " bestS=" << bestS
                 << " lo=" << lo << " hi=" << hi
                 << " bestArea=" << bestArea;

        // FG Build (tight + reliable)
        // 1) Start from UNION of labels in [lo..hi]
        cv::Mat fg8 = (winStable16 >= (uint16_t)lo) & (winStable16 <= (uint16_t)hi);
        fg8.convertTo(fg8, CV_8U, 255.0);

        // 2) Light confidence gate (use confLow, NOT confHigh)
        cv::Mat confOK = (conf01 > params.confLow);
        confOK.convertTo(confOK, CV_8U, 255.0);
        cv::bitwise_and(fg8, confOK, fg8);

        // 3) Keep largest CC EARLY (before any big closing)
        fg8 = keepLargestCC(fg8, 8);

        // 4) DO NOT open/close/erode the FG mask itself.
        // The open(2) is what is nuking fine branches/twigs and collapsing fg8 to tiny islands.

        // Keep a "display/seed FG" that preserves thin structures
        cv::Mat fgFull8 = fg8.clone();            // <-- this is what halo_fg.png should show

        // Make a *separate* small "core" for the ring/band so the band is truly outside
        cv::Mat fgCore8 = fgFull8.clone();
        const int fgCoreErodePx = 1;              // 0 or 1 only
        if (fgCoreErodePx > 0)
            cv::erode(fgCore8, fgCore8, FSFusion::seEllipse(fgCoreErodePx));

        // Use fgCore8 for subsequent boundary/band, but keep fg8 as the full mask for writing/debug
        fg8 = fgFull8;

        // Optional sanity prints (remove later)
        qDebug() << "HaloFix: fgFullPx=" << cv::countNonZero(fgFull8)
                 << " fgCorePx=" << cv::countNonZero(fgCore8);

        qDebug() << "HaloFix: fgPx=" << cv::countNonZero(fg8)
                 << " frac=" << (double)cv::countNonZero(fg8) / (double)fg8.total()
                 << " bestS=" << bestS << " lo=" << lo << " hi=" << hi
                 << " confLow=" << params.confLow
                 << " confHigh=" << params.confHigh
                 << " confMax=" << params.haloConfMax
                 << " texMax=" << params.haloTexMax
                 << " featherSigma=" << params.haloFeatherSigma;

        // -------------------------------
        // HOLE-SAFE mask for boundary/band
        // -------------------------------

        // Fill holes in fg8 ONLY to keep the halo ring outside the subject silhouette.
        // fg8 is CV_8U 0/255.
        auto fillHoles8 = [](const cv::Mat& bin8)->cv::Mat
        {
            CV_Assert(bin8.type() == CV_8U);

            // 1) Invert: holes become 255 regions inside foreground
            cv::Mat inv;
            cv::bitwise_not(bin8, inv);

            // 2) Flood fill from the border on inv => marks true "outside background"
            cv::Mat ff = inv.clone();
            cv::floodFill(ff, cv::Point(0, 0), cv::Scalar(0)); // outside background set to 0

            // 3) Remaining 255 in ff are holes (NOT connected to border). Turn them into FG.
            // holesMask: 255 where holes
            cv::Mat holesMask;
            cv::compare(ff, 0, holesMask, cv::CMP_GT); // 255 where ff > 0 (holes)

            // 4) Add holes into original FG
            cv::Mat filled = bin8.clone();
            filled.setTo(255, holesMask);
            return filled;
        };

        cv::Mat fgFilled8 = fillHoles8(fg8);

        qDebug() << "HaloFix: fgPx=" << cv::countNonZero(fg8)
                 << " fgFilledPx=" << cv::countNonZero(fgFilled8)
                 << " delta=" << (cv::countNonZero(fgFilled8) - cv::countNonZero(fg8));

        // boundary for seeding: use filled silhouette (outer boundary only)
        cv::Mat boundary8;
        cv::morphologyEx(fgFilled8, boundary8, cv::MORPH_GRADIENT, FSFusion::seEllipse(1));

        // band (ring outside FG): use filled silhouette so ring does NOT enter holes
        cv::Mat fgDil, band8;
        cv::dilate(fgFilled8, fgDil, FSFusion::seEllipse(ringPx));
        cv::bitwise_and(fgDil, ~fgFilled8, band8);

        const int bandPx = cv::countNonZero(band8);
        if (bandPx == 0)
        {
            qDebug() << "HaloFix: band empty; skipping.";
            break;
        }

        // candidate = band & low-conf & low-tex
        cv::Mat lowConf8 = (conf01 < params.haloConfMax);
        lowConf8.convertTo(lowConf8, CV_8U, 255.0);

        double smn=0.0, smx=0.0;
        cv::minMaxLoc(topkOrig.score32[0], &smn, &smx);
        const float scale = (smx > 1e-12) ? (255.0f / (float)smx) : 1.0f;

        cv::Mat tex8;
        topkOrig.score32[0].convertTo(tex8, CV_8U, scale);

        cv::Mat lowTex8 = (tex8 <= params.haloTexMax);
        lowTex8.convertTo(lowTex8, CV_8U, 255.0);

        cv::Mat haloCand8;
        cv::bitwise_and(band8, lowConf8, haloCand8);
        cv::bitwise_and(haloCand8, lowTex8, haloCand8);

        // never touch very-confident pixels
        cv::Mat veryConf8 = (conf01 > params.confHigh);
        veryConf8.convertTo(veryConf8, CV_8U, 255.0);
        cv::bitwise_and(haloCand8, ~veryConf8, haloCand8);

        const int candPx = cv::countNonZero(haloCand8);
        qDebug() << "HaloFix: bandPx=" << bandPx
                 << " candPx=" << candPx
                 << " cand/band=" << (double)candPx / (double)std::max(1, bandPx);

        if (candPx == 0)
        {
            qDebug() << "HaloFix: no candidates; skipping.";
            break;
        }

        // Visual debug (masks)
        {
            const QString folder = opt.depthFolderPath;
            cv::imwrite((folder + "/halo_fg.png").toStdString(), fg8);
            cv::imwrite((folder + "/halo_fg_filled.png").toStdString(), fgFilled8);
            cv::imwrite((folder + "/halo_boundary.png").toStdString(), boundary8);
            cv::imwrite((folder + "/halo_band.png").toStdString(), band8);
            cv::imwrite((folder + "/halo_cand.png").toStdString(), haloCand8);

            cv::Mat baseGray8 = toGray8_fromOut32(out32);
            cv::Mat overlay0  = makeHaloDebugOverlayBGR8(baseGray8, band8, cv::Mat(), 0.45f, 0.0f);
            addSliceLegendStripBottomBGR8(overlay0, N, 140, 8);
            cv::imwrite((folder + "/halo_overlay_band.png").toStdString(), overlay0);
        }

        // Seeds + donors
        cv::Mat seeds8, seedLabel16;
        buildForegroundBoundarySeeds(winStable16, boundary8, seeds8, seedLabel16);

        const int seedsPx = cv::countNonZero(seeds8);
        qDebug() << "HaloFix: seedsPx=" << seedsPx;
        if (seedsPx == 0)
        {
            qDebug() << "HaloFix: no seeds; skipping.";
            break;
        }

        cv::Mat donor16 = propagateDonorLabels16(seeds8, seedLabel16, haloCand8, ringPx);

        cv::Mat hasDonor8 = (donor16 != (uint16_t)0xFFFF);
        hasDonor8.convertTo(hasDonor8, CV_8U, 255.0);
        const int donorPx = cv::countNonZero(hasDonor8);

        qDebug() << "HaloFix: donorPx=" << donorPx
                 << " donor/cand=" << (double)donorPx / (double)std::max(1, candPx);

        if (donorPx == 0)
        {
            qDebug() << "HaloFix: no donor labels; skipping.";
            break;
        }

        // Visual debug (donor overlay)
        {
            const QString folder = opt.depthFolderPath;
            cv::Mat baseGray8 = toGray8_fromOut32(out32);

            cv::Mat donorColor = colorizeDonorLabelsBGR8(donor16, N);

            cv::Mat donorColorForFile = donorColor.clone();
            addSliceLegendStripBottomBGR8(donorColorForFile, N, 140, 8);
            cv::imwrite((folder + "/halo_donor_labels.png").toStdString(), donorColorForFile);

            cv::Mat overlay = makeHaloDebugOverlayBGR8(baseGray8, band8, donorColor, 0.35f, 0.60f);
            addSliceLegendStripBottomBGR8(overlay, N, 140, 8);
            cv::imwrite((folder + "/halo_overlay.png").toStdString(), overlay);
        }

        // Alpha feather
        cv::Mat alphaBand01;
        haloCand8.convertTo(alphaBand01, CV_32F, 1.0 / 255.0);

        const float sig = std::max(0.0f, params.haloFeatherSigma);
        if (sig > 0.0f)
            cv::GaussianBlur(alphaBand01, alphaBand01, cv::Size(0, 0), sig, sig, cv::BORDER_REFLECT);

        cv::max(alphaBand01, 0.0f, alphaBand01);
        cv::min(alphaBand01, 1.0f, alphaBand01);

        // halo-focused alpha stats
        {
            double amn=0, amx=0;
            cv::minMaxLoc(alphaBand01, &amn, &amx);
            const cv::Scalar am = cv::mean(alphaBand01);
            qDebug() << "HaloFix: alphaBand01 mean=" << am[0] << " min/max=" << amn << amx;
        }

        // Optional: write alpha image (very useful to see feather width)
        {
            const QString folder = opt.depthFolderPath;
            cv::Mat a8;
            alphaBand01.convertTo(a8, CV_8U, 255.0);
            cv::imwrite((folder + "/halo_alphaBand.png").toStdString(), a8);
        }

        // Blend donor slices (no per-donor debug)
        for (int s = 0; s < N; ++s)
        {
            cv::Mat ms = (donor16 == (uint16_t)s);
            ms.convertTo(ms, CV_32F, 1.0 / 255.0);

            cv::Mat alpha = alphaBand01.mul(ms);
            if (cv::countNonZero(alpha > 1e-6f) == 0)
                continue;

            cv::Mat gTmp, cTmp;
            if (!FSLoader::loadAlignedSliceOrig(s, inputPaths, globals,
                                                roiPadToAlign, validAreaAlign,
                                                gTmp, cTmp) || cTmp.empty())
            {
                qWarning().noquote() << "WARNING: HaloFix loadAlignedSliceOrig failed for slice" << s;
                continue;
            }

            cv::Mat col32;
            if (!toColor32_01_FromLoaded(cTmp, col32, "HaloFix donor", s))
                continue;

            cv::Mat a3;
            { cv::Mat ch[] = { alpha, alpha, alpha }; cv::merge(ch, 3, a3); }

            cv::Mat invA3;
            cv::subtract(cv::Scalar::all(1.0f), a3, invA3);
            out32 = out32.mul(invA3) + col32.mul(a3);
        }

        cv::max(out32, 0.0f, out32);
        cv::min(out32, 1.0f, out32);

    } while(false);

    // ---- Keep: halo-result stats (NOT blue-related) ----
    dbgMat3Stats("DBG out32 postHalo clamp", out32);

    // Clamp AFTER halo fix
    cv::max(out32, 0.0f, out32);
    cv::min(out32, 1.0f, out32);

    // Convert to output depth
    if (outDepth == CV_16U)
        out32.convertTo(outputColor, CV_16UC3, 65535.0);
    else
        out32.convertTo(outputColor, CV_8UC3, 255.0);

    reset();
    return true;
}
