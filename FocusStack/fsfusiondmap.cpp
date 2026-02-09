#include "fsfusiondmap.h"


#include "Main/global.h"
#include "fsloader.h"
#include "fsutilities.h"

#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/imgcodecs.hpp>

#include <cmath>
#include <algorithm>
#include <limits>
#include <deque>

namespace
{
// ============================================================
// debug helpers
// ============================================================

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

// ============================================================
// Small generic image helpers (pad/geometry/label filters)
// ============================================================

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

// ============================================================
// Top-K / metric / confidence / slice selection
// ============================================================

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

// ============================================================
// Weight building (softmax + boundary/band + per-slice weights)
// ============================================================

// Keep all connected components in bin8 that intersect touch8 (both CV_8U 0/255)
static cv::Mat keepCCsTouchingMask(const cv::Mat& bin8, const cv::Mat& touch8, int connectivity = 8)
{
    CV_Assert(bin8.type() == CV_8U);
    CV_Assert(touch8.type() == CV_8U);
    CV_Assert(bin8.size() == touch8.size());

    cv::Mat src;
    cv::compare(bin8, 0, src, cv::CMP_GT); // 0/255
    if (cv::countNonZero(src) == 0)
        return cv::Mat::zeros(src.size(), CV_8U);

    cv::Mat labels, stats, centroids;
    int n = cv::connectedComponentsWithStats(src, labels, stats, centroids, connectivity, CV_32S);
    if (n <= 1)
        return cv::Mat::zeros(src.size(), CV_8U);

    // Which labels intersect touch8?
    std::vector<uint8_t> keep(n, 0);
    for (int y = 0; y < labels.rows; ++y)
    {
        const int* L = labels.ptr<int>(y);
        const uint8_t* T = touch8.ptr<uint8_t>(y);
        for (int x = 0; x < labels.cols; ++x)
        {
            if (!T[x]) continue;
            int id = L[x];
            if (id > 0 && id < n) keep[id] = 1;
        }
    }

    cv::Mat out = cv::Mat::zeros(src.size(), CV_8U);
    for (int y = 0; y < labels.rows; ++y)
    {
        const int* L = labels.ptr<int>(y);
        uint8_t* O = out.ptr<uint8_t>(y);
        for (int x = 0; x < labels.cols; ++x)
        {
            int id = L[x];
            if (id > 0 && id < n && keep[id]) O[x] = 255;
        }
    }
    return out;
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

// softmax weights from topk scores
static void buildTopKWeightsSoftmax(const FSFusionDMap::TopKMaps& topk,
                                    const FSFusionDMap::DMapParams& p,
                                    FSFusionDMap::DMapWeights& weightsOut)
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
                                     FSFusionDMap::DMapWeights& W)
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
}

static cv::Mat buildSliceWeightMap32(const FSFusionDMap::TopKMaps& topk,
                                     const FSFusionDMap::DMapWeights& W,
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

// ============================================================
// Depth Map Experiments
// ============================================================

static cv::Mat depthExperiment_majoritySmooth(const cv::Mat& base16,
                                              const cv::Mat& conf01,
                                              float confGate = 0.25f,
                                              int iters = 1)
{
    CV_Assert(base16.type() == CV_16U);
    CV_Assert(conf01.type() == CV_32F);
    CV_Assert(base16.size() == conf01.size());

    cv::Mat d = base16.clone();

    cv::Mat unreliable8 = (conf01 < confGate);
    unreliable8.convertTo(unreliable8, CV_8U, 255.0);

    for (int i = 0; i < iters; ++i)
    {
        if (cv::countNonZero(unreliable8) == 0) break;
        cv::Mat filt = majorityFilterLabels16_3x3(d);
        filt.copyTo(d, unreliable8);
    }
    return d;
}


// ============================================================
// Halo utilities (donor/seed/alpha helpers)
// ============================================================



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

// Older/alternate halo-ring path helpers (looks unused in current donor method)
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
// top only helpers (member impls)
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

    params.depthMode = FSFusionDMap::DepthMode::Experimental;

    params.depthHook =
        [](const cv::Mat& base16, const cv::Mat& conf01,
           const FSFusionDMap::TopKMaps& topk, int N, cv::Mat& out16) -> bool
    {
        (void)topk; (void)N;
        out16 = depthExperiment_majoritySmooth(base16, conf01, /*confGate=*/0.25f, /*iters=*/2);
        return true;
    };
}

void FSFusionDMap::reset()
{
    active_ = false;
    sliceCount_ = 0;

    // base geometry
    resetGeometry();

    topk_pad_.reset();
}

bool FSFusionDMap::validateStreamFinishState(const QString& srcFun,
                                             const QStringList& inputPaths,
                                             const std::vector<Result>& globals,
                                             std::atomic_bool* abortFlag,
                                             int& N) const
{
    if (!active_ || !topk_pad_.valid()) {
        qWarning().noquote() << "WARNING:" << srcFun << "state not initialized.";
        return false;
    }

    N = inputPaths.size();
    if (N <= 0 || (int)globals.size() != N) {
        qWarning().noquote() << "WARNING:" << srcFun << "inputPaths/globals size mismatch.";
        return false;
    }

    if (FSFusion::isAbort(abortFlag)) return false;

    if (!hasValidGeometry()) {
        qWarning().noquote() << "WARNING:" << srcFun
                             << "missing/invalid geometry:" << geometryText();
        return false;
    }

    return true;
}

bool FSFusionDMap::computeCropGeometry(const QString& srcFun,
                                       cv::Rect& roiPadToAlign,
                                       cv::Size& origSz)
{
    roiPadToAlign = cv::Rect(0, 0, alignSize.width, alignSize.height);

    if (padSize != alignSize) {
        const int padW = padSize.width  - alignSize.width;
        const int padH = padSize.height - alignSize.height;
        if (padW < 0 || padH < 0) {
            qWarning().noquote() << "WARNING:" << srcFun << "padSize smaller than alignSize.";
            return false;
        }
        roiPadToAlign.x = padW / 2;
        roiPadToAlign.y = padH / 2;
    }

    if (!FSFusion::rectInside(roiPadToAlign, topk_pad_.sz)) {
        qWarning().noquote() << "WARNING:" << srcFun << "roiPadToAlign out of bounds for PAD maps.";
        return false;
    }

    const cv::Size alignSz(roiPadToAlign.width, roiPadToAlign.height);
    if (!FSFusion::rectInside(validAreaAlign, alignSz)) {
        qWarning().noquote() << "WARNING:" << srcFun << "validAreaAlign out of bounds for ALIGN crop.";
        return false;
    }

    origSz = cv::Size(validAreaAlign.width, validAreaAlign.height);
    origSize = origSz;
    return true;
}

// Crop TopK maps PAD→ALIGN→ORIG
bool FSFusionDMap::cropTopKToOrig(const QString& srcFun,
                                  const cv::Rect& roiPadToAlign,
                                  const cv::Size& origSz,
                                  TopKMaps& topkOrig) const
{
    topkOrig.create(origSz, topk_pad_.K);

    for (int k = 0; k < topk_pad_.K; ++k) {
        cv::Mat idxAlign = topk_pad_.idx16[k](roiPadToAlign);
        cv::Mat scAlign  = topk_pad_.score32[k](roiPadToAlign);

        topkOrig.idx16[k]   = idxAlign(validAreaAlign).clone();
        topkOrig.score32[k] = scAlign(validAreaAlign).clone();
    }
    return true;
}

bool FSFusionDMap::sanityCheckTop1(const QString& srcFun,
                                   const TopKMaps& topkOrig,
                                   int N) const
{
    // diffs
    {
        const cv::Mat& d = topkOrig.idx16[0];
        const long long diffCount = countHorizontalDiffs16(d);
        const long long total = (long long)d.rows * (d.cols - 1);
        qDebug() << "top1 diffs (ORIG, pre-weight) =" << diffCount << "/" << total
                 << " frac=" << (double)diffCount / (double)total;
    }

    // range
    {
        double idxMin = 0, idxMax = 0;
        cv::minMaxLoc(topkOrig.idx16[0], &idxMin, &idxMax);
        qDebug() << "Top1 idx min/max =" << idxMin << idxMax << " N=" << N;

        if (idxMin < 0 || idxMax >= N) {
            qWarning().noquote() << "WARNING:" << srcFun
                                 << "Top1 idx out of range: min/max="
                                 << idxMin << idxMax << " N=" << N;
            return false;
        }
    }
    return true;
}

//  Weights + confidence + winner stabilization (+ “stable” labels for halo)
bool FSFusionDMap::buildMapsAndStabilize(const QString& srcFun,
                                         const TopKMaps& topkOrig,
                                         int N,
                                         std::atomic_bool* abortFlag,
                                         DMapWeights& W,
                                         cv::Mat& conf01,
                                         cv::Mat& depthIndex16,
                                         cv::Mat& winStable16) const
{
    buildDMapWeightsFromTopK(topkOrig, params, W);
    conf01 = computeConfidence01_Top2(topkOrig, params.confEps);

    // winner + light stabilization (your median-on-lowConf)
    cv::Mat win16 = topkOrig.idx16[0].clone();
    const long long before = countHorizontalDiffs16(win16);

    if (topkOrig.K >= 2) {
        cv::Mat lowConf8 = (conf01 < params.confLow);
        lowConf8.convertTo(lowConf8, CV_8U, 255.0);

        if (params.uncertaintyDilatePx > 0)
            cv::dilate(lowConf8, lowConf8, FSFusion::seEllipse(params.uncertaintyDilatePx));

        if (win16.cols >= 5 && win16.rows >= 5 && cv::countNonZero(lowConf8) > 0) {
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

    // Stable labels for halo boundary detection
    winStable16 = depthIndex16.clone();
    cv::Mat unreliable8 = (conf01 < params.confHigh);
    unreliable8.convertTo(unreliable8, CV_8U, 255.0);

    if (cv::countNonZero(unreliable8) > 0) {
        cv::Mat filt16 = majorityFilterLabels16_3x3(winStable16);
        filt16.copyTo(winStable16, unreliable8);
    }

    return true;
}

// Experimental depth map
bool FSFusionDMap::depthExperiment(const QString& srcFun,
                                   const FSFusion::Options& opt,
                                   const TopKMaps& topkOrig,
                                   int N,
                                   const cv::Mat& conf01,
                                   std::atomic_bool* abortFlag,
                                   cv::Mat& depthIndex16,
                                   cv::Mat& winStable16)
{

    if (FSFusion::isAbort(abortFlag)) return false;

    CV_Assert(topkOrig.valid());
    CV_Assert(N > 0);
    CV_Assert(conf01.type() == CV_32F);
    CV_Assert(conf01.size() == topkOrig.sz);

    // ---------------------------------------------------------------------
    // Baseline depth = top-1 winner labels (orig size)
    // ---------------------------------------------------------------------
    cv::Mat depthBaseline16 = topkOrig.idx16[0].clone();
    CV_Assert(depthBaseline16.type() == CV_16U);

    // Clamp indices defensively (should already be ok)
    {
        double mn=0, mx=0;
        cv::minMaxLoc(depthBaseline16, &mn, &mx);
        if (mn < 0 || mx >= N)
        {
            qWarning().noquote() << "WARNING:" << srcFun
                                 << "depthBaseline16 out of range mn/mx="
                                 << mn << mx << " N=" << N;
            // Clamp in-place
            for (int y=0; y<depthBaseline16.rows; ++y)
            {
                uint16_t* d = depthBaseline16.ptr<uint16_t>(y);
                for (int x=0; x<depthBaseline16.cols; ++x)
                {
                    int v = (int)d[x];
                    v = std::max(0, std::min(v, N-1));
                    d[x] = (uint16_t)v;
                }
            }
        }
    }

    // ---------------------------------------------------------------------
    // Build helper masks: boundary, low-conf, optional low-tex
    // Tighten expansion by ONLY smoothing where:
    //    low conf  AND near a label boundary AND (optional) low texture.
    // ---------------------------------------------------------------------

    auto boundaryFromLabels8_local = [](const cv::Mat& lab16)->cv::Mat
    {
        CV_Assert(lab16.type() == CV_16U);
        cv::Mat b = cv::Mat::zeros(lab16.size(), CV_8U);

        for (int y = 0; y < lab16.rows; ++y)
        {
            const uint16_t* r  = lab16.ptr<uint16_t>(y);
            const uint16_t* ru = (y > 0) ? lab16.ptr<uint16_t>(y-1) : nullptr;
            const uint16_t* rd = (y+1 < lab16.rows) ? lab16.ptr<uint16_t>(y+1) : nullptr;
            uint8_t* out = b.ptr<uint8_t>(y);

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
    };

    // Boundary (raw) then slightly dilated for masks
    cv::Mat boundary8 = boundaryFromLabels8_local(depthBaseline16);

    cv::Mat boundarySmooth8 = boundary8.clone();
    if (params.depthBoundaryDilateSmoothPx > 0)
        cv::dilate(boundarySmooth8, boundarySmooth8,
                   FSFusion::seEllipse(params.depthBoundaryDilateSmoothPx));

    cv::Mat boundaryStable8 = boundary8.clone();
    if (params.depthBoundaryDilateStablePx > 0)
        cv::dilate(boundaryStable8, boundaryStable8,
                   FSFusion::seEllipse(params.depthBoundaryDilateStablePx));

    // Low confidence masks (0/255)
    cv::Mat lowConfSmooth8 = (conf01 < params.depthConfSmoothMax);
    lowConfSmooth8.convertTo(lowConfSmooth8, CV_8U, 255.0);

    cv::Mat lowConfStable8 = (conf01 < params.depthConfStableMax);
    lowConfStable8.convertTo(lowConfStable8, CV_8U, 255.0);

    // Optional texture gate (from top1 score, scaled to 8U like halo uses)
    cv::Mat lowTex8; // 0/255 (255 means "LOW texture")
    if (params.depthUseTextureGate)
    {
        double smn=0.0, smx=0.0;
        cv::minMaxLoc(topkOrig.score32[0], &smn, &smx);
        const float scale = (smx > 1e-12) ? (255.0f / (float)smx) : 1.0f;

        cv::Mat tex8;
        topkOrig.score32[0].convertTo(tex8, CV_8U, scale);

        lowTex8 = (tex8 <= params.depthTexMax);
        lowTex8.convertTo(lowTex8, CV_8U, 255.0);
    }
    else
    {
        lowTex8 = cv::Mat(depthBaseline16.size(), CV_8U, cv::Scalar(255));
    }

    // // ---------------------------------------------------------------------
    // // 1) “Tighten” pass: ONLY allow idx0 <-> idx1 flips when ambiguity is high
    // // This is safer than medianBlur on label images (which tends to expand regions).
    // // ---------------------------------------------------------------------
    // cv::Mat depthTight16 = depthBaseline16.clone();
    // {
    //     CV_Assert(topkOrig.K >= 1);
    //     const cv::Mat& idx0 = topkOrig.idx16[0];
    //     const cv::Mat& idx1 = (topkOrig.K >= 2) ? topkOrig.idx16[1] : topkOrig.idx16[0];
    //     const cv::Mat& s0   = topkOrig.score32[0];
    //     const cv::Mat& s1   = (topkOrig.K >= 2) ? topkOrig.score32[1] : topkOrig.score32[0];

    //     // margin01 ~ “how much better is top1 than top2”
    //     cv::Mat margin01 = (s0 - s1) / (s0 + std::max(1e-12f, params.confEps));
    //     cv::max(margin01, 0.0f, margin01);
    //     cv::min(margin01, 1.0f, margin01);

    //     // Build mask: boundary & lowConf & lowTex
    //     cv::Mat m8;
    //     cv::bitwise_and(boundarySmooth8, lowConfSmooth8, m8);
    //     cv::bitwise_and(m8, lowTex8, m8);

    //     // Only touch pixels that are truly ambiguous
    //     const float ambMax = 0.15f; // start 0.10–0.18
    //     cv::Mat amb8 = (margin01 < ambMax);
    //     amb8.convertTo(amb8, CV_8U, 255.0);
    //     cv::bitwise_and(m8, amb8, m8);


    //     // =====================================================================
    //     // SUBJECT-FOCUSED ROI DIAGNOSTICS (slice 5 by default)
    //     // Writes CROPPED images + ROI-only stats, so you can see twig behavior.
    //     // =====================================================================
    //     const int dbgSlice = 5;                     // <- twig slice id
    //     const int roiPadPx = 40;                    // <- expand ROI a bit
    //     const int ccMinArea = 2000;                 // <- ignore tiny specks

    //     auto clampRectToImage = [](cv::Rect r, const cv::Size& sz)->cv::Rect
    //     {
    //         int x0 = std::max(0, r.x);
    //         int y0 = std::max(0, r.y);
    //         int x1 = std::min(sz.width,  r.x + r.width);
    //         int y1 = std::min(sz.height, r.y + r.height);
    //         return cv::Rect(x0, y0, std::max(0, x1 - x0), std::max(0, y1 - y0));
    //     };

    //     auto boundingRectOfLargestCC = [&](const cv::Mat& bin8, int minArea)->cv::Rect
    //     {
    //         CV_Assert(bin8.type() == CV_8U);

    //         cv::Mat src;
    //         cv::compare(bin8, 0, src, cv::CMP_GT); // 0/255
    //         if (cv::countNonZero(src) == 0) return cv::Rect();

    //         cv::Mat labels, stats, centroids;
    //         int n = cv::connectedComponentsWithStats(src, labels, stats, centroids, 8, CV_32S);
    //         if (n <= 1) return cv::Rect();

    //         int best = -1;
    //         int bestArea = 0;
    //         for (int i = 1; i < n; ++i)
    //         {
    //             int area = stats.at<int>(i, cv::CC_STAT_AREA);
    //             if (area >= minArea && area > bestArea) { bestArea = area; best = i; }
    //         }
    //         if (best < 0) return cv::Rect();

    //         int x = stats.at<int>(best, cv::CC_STAT_LEFT);
    //         int y = stats.at<int>(best, cv::CC_STAT_TOP);
    //         int w = stats.at<int>(best, cv::CC_STAT_WIDTH);
    //         int h = stats.at<int>(best, cv::CC_STAT_HEIGHT);
    //         return cv::Rect(x, y, w, h);
    //     };

    //     // Build mask for dbgSlice in baseline depth
    //     cv::Mat mSlice8 = (depthBaseline16 == (uint16_t)dbgSlice);
    //     mSlice8.convertTo(mSlice8, CV_8U, 255.0);

    //     cv::Rect roi = boundingRectOfLargestCC(mSlice8, ccMinArea);
    //     if (roi.area() > 0)
    //     {
    //         // Expand ROI
    //         roi.x -= roiPadPx; roi.y -= roiPadPx;
    //         roi.width  += 2 * roiPadPx;
    //         roi.height += 2 * roiPadPx;
    //         roi = clampRectToImage(roi, depthBaseline16.size());

    //         // ROI-only stats helper
    //         auto roiMean = [&](const cv::Mat& m32)->double {
    //             CV_Assert(m32.type() == CV_32F);
    //             cv::Scalar mu = cv::mean(m32(roi));
    //             return mu[0];
    //         };

    //         // conf + margin ROI means
    //         const double confMeanROI   = roiMean(conf01);
    //         const double marginMeanROI = roiMean(margin01);

    //         // How much of ROI is labeled dbgSlice in baseline?
    //         int roiPx = roi.width * roi.height;
    //         int slicePxBase = cv::countNonZero(mSlice8(roi));
    //         qDebug().noquote()
    //             << "DepthROI(slice" << dbgSlice << "): roi=" << roi.x << roi.y << roi.width << roi.height
    //             << " roiPx=" << roiPx
    //             << " baseSlicePx=" << slicePxBase
    //             << " frac=" << (roiPx > 0 ? (double)slicePxBase / (double)roiPx : 0.0)
    //             << " confMean=" << confMeanROI
    //             << " marginMean=" << marginMeanROI;

    //         if (params.writeDepthDiagnostics)
    //         {
    //             const QString folder = opt.depthFolderPath;

    //             // Write CROPPED masks/maps (orig size crop, no resize)
    //             cv::imwrite((folder + "/roi_mask_slice6_base.png").toStdString(), mSlice8(roi));

    //             // canFlip in ROI (idx0!=idx1)
    //             cv::Mat canFlip = (idx0 != idx1);
    //             canFlip.convertTo(canFlip, CV_8U, 255.0);
    //             cv::imwrite((folder + "/roi_canFlip_idx0neqidx1.png").toStdString(), canFlip(roi));

    //             // idx1==6 in ROI (does top2 even offer 6?)
    //             if (topkOrig.K >= 2)
    //             {
    //                 cv::Mat idx1is6 = (idx1 == (uint16_t)dbgSlice);
    //                 idx1is6.convertTo(idx1is6, CV_8U, 255.0);
    //                 cv::imwrite((folder + "/roi_idx1_is6.png").toStdString(), idx1is6(roi));
    //             }

    //             // conf ROI (0..255)
    //             cv::Mat conf8;
    //             conf01(roi).convertTo(conf8, CV_8U, 255.0);
    //             cv::imwrite((folder + "/roi_conf.png").toStdString(), conf8);

    //             // margin ROI (0..255)
    //             cv::Mat mar8;
    //             margin01(roi).convertTo(mar8, CV_8U, 255.0);
    //             cv::imwrite((folder + "/roi_margin.png").toStdString(), mar8);

    //             // texture ROI from top1Score32 (scaled like you do)
    //             {
    //                 double smn=0.0, smx=0.0;
    //                 cv::minMaxLoc(topkOrig.score32[0], &smn, &smx);
    //                 float scale = (smx > 1e-12) ? (255.0f / (float)smx) : 1.0f;

    //                 cv::Mat tex8;
    //                 topkOrig.score32[0](roi).convertTo(tex8, CV_8U, scale);
    //                 cv::imwrite((folder + "/roi_tex8.png").toStdString(), tex8);
    //             }

    //             // Depth heatmap crop (baseline only)
    //             {
    //                 cv::Mat heat = FSUtilities::depthHeatmap(depthBaseline16, N, "Depth (baseline)");
    //                 cv::imwrite((folder + "/roi_depth_baseline.png").toStdString(), heat(roi));
    //             }
    //         }
    //     }
    //     else
    //     {
    //         qDebug() << "DepthROI: no sufficiently large CC found for slice" << dbgSlice;
    //     }

    //     // END  SUBJECT-FOCUSED ROI DIAGNOSTICS (slice 5 by default)
    //     // -----------------------------------------------------------------------

    //     if (params.writeDepthDiagnostics)
    //     {
    //         const QString folder = opt.depthFolderPath;

    //         const int twigSlice = 5;

    //         cv::Mat m0 = (topkOrig.idx16[0] == (uint16_t)twigSlice);
    //         cv::Mat m1 = (topkOrig.K >= 2) ? (topkOrig.idx16[1] == (uint16_t)twigSlice) : cv::Mat();

    //         m0.convertTo(m0, CV_8U, 255);
    //         cv::imwrite((folder + "/dbg_idx0_is6.png").toStdString(), m0);

    //         if (!m1.empty()) {
    //             m1.convertTo(m1, CV_8U, 255);
    //             cv::imwrite((folder + "/dbg_idx1_is6.png").toStdString(), m1);
    //         }

    //         // where idx0 != idx1 (places where your flip logic could ever act)
    //         cv::Mat canFlip = (topkOrig.idx16[0] != topkOrig.idx16[1]);
    //         canFlip.convertTo(canFlip, CV_8U, 255);
    //         cv::imwrite((folder + "/dbg_canFlip_idx0neqidx1.png").toStdString(), canFlip);

    //         // optional: ambiguous pixels mask you actually used (m8)
    //         // write m8 too, so you can see if twig boundary is even covered
    //         cv::imwrite((folder + "/dbg_tight_mask.png").toStdString(), m8);
    //     }

    //     qDebug() << "DepthTight: maskPx=" << cv::countNonZero(m8);

    //     auto majority3x3 = [&](const cv::Mat& lab16, int y, int x, int& outCount)->uint16_t
    //     {
    //         uint16_t a[9];
    //         int k=0;
    //         for (int yy=y-1; yy<=y+1; ++yy)
    //             for (int xx=x-1; xx<=x+1; ++xx)
    //                 a[k++] = lab16.at<uint16_t>(yy,xx);

    //         uint16_t best = a[0];
    //         int bestCnt = 1;
    //         for (int i=0;i<9;++i){
    //             int c=0;
    //             for (int j=0;j<9;++j) c += (a[j]==a[i]);
    //             if (c>bestCnt){ bestCnt=c; best=a[i]; }
    //         }
    //         outCount = bestCnt;
    //         return best;
    //     };

    //     int flips = 0;

    //     // Only allow idx0 -> idx1 flip when the local neighborhood “votes” for idx1
    //     for (int y=1; y<depthTight16.rows-1; ++y)
    //     {
    //         const uint8_t* mm = m8.ptr<uint8_t>(y);
    //         for (int x=1; x<depthTight16.cols-1; ++x)
    //         {
    //             if (!mm[x]) continue;

    //             const uint16_t a0 = idx0.at<uint16_t>(y,x);
    //             const uint16_t a1 = idx1.at<uint16_t>(y,x);
    //             if (a0 == a1) continue;

    //             int cnt=0;
    //             const uint16_t maj = majority3x3(depthTight16, y, x, cnt);

    //             // Flip ONLY if majority == this pixel’s own top2 alternative
    //             if (maj == a1 && cnt >= 5)
    //             {
    //                 depthTight16.at<uint16_t>(y,x) = a1;
    //                 flips++;
    //             }
    //         }
    //     }

    //     qDebug() << "DepthTight: flips=" << flips;
    //     {
    //         cv::Mat d = (depthTight16 != depthBaseline16);
    //         d.convertTo(d, CV_8U, 255);
    //         const int changed = cv::countNonZero(d);
    //         qDebug() << "DepthTight: changedPx=" << changed;

    //         if (params.writeDepthDiagnostics) {
    //             const QString folder = opt.depthFolderPath;
    //             cv::imwrite((folder + "/depth_diff_tight.png").toStdString(), d); // orig size
    //         }
    //     }
    // }


    // ---------------------------------------------------------------------
    // 1) Tighten by "confident core" + nearest-label fill on ambiguous pixels
    // This actually moves boundaries inward (shrinks fat regions).
    // ---------------------------------------------------------------------
    cv::Mat depthTight16 = depthBaseline16.clone();

    auto makeTex8 = [&]()->cv::Mat {
        // Convert top1Score32 to 8U (0..255) like you do elsewhere
        double smn=0.0, smx=0.0;
        cv::minMaxLoc(topkOrig.score32[0], &smn, &smx);
        const float scale = (smx > 1e-12) ? (255.0f / (float)smx) : 1.0f;

        cv::Mat tex8;
        topkOrig.score32[0].convertTo(tex8, CV_8U, scale);
        return tex8;
    };

    cv::Mat tex8 = makeTex8();


    // ---------------------------------------------------------------------
    // SUBJECT SCOPE MASK (optional): load fg.png and constrain depth tightening
    // - If fg.png exists and matches size => scope8 is 0/255 (255 = in-scope)
    // - If missing or invalid => scope8 = all 255 (full frame, no constraint)
    // ---------------------------------------------------------------------
    cv::Mat scope8(depthBaseline16.size(), CV_8U, cv::Scalar(255)); // default: whole frame

    {
        const QString fgPath = opt.depthFolderPath + "/fg.png"; // <-- adjust if needed

        cv::Mat fg = cv::imread(fgPath.toStdString(), cv::IMREAD_UNCHANGED);

        if (fg.empty())
        {
            qDebug().noquote() << "DepthScope: fg.png not found; using full-frame scope:" << fgPath;
        }
        else
        {
            cv::Mat fg8;

            // Convert to 8U single-channel mask-ish image
            if (fg.channels() == 4)
            {
                // If RGBA, use alpha as mask
                std::vector<cv::Mat> ch;
                cv::split(fg, ch);
                fg8 = ch[3];
            }
            else if (fg.channels() == 3)
            {
                cv::cvtColor(fg, fg8, cv::COLOR_BGR2GRAY);
            }
            else
            {
                fg8 = fg;
            }

            if (fg8.type() != CV_8U)
                fg8.convertTo(fg8, CV_8U);

            if (fg8.size() != depthBaseline16.size())
            {
                qWarning().noquote()
                << "WARNING:" << srcFun
                << "fg.png size mismatch. fg=" << fg8.cols << "x" << fg8.rows
                << " expected=" << depthBaseline16.cols << "x" << depthBaseline16.rows
                << " -> ignoring fg.png (full-frame scope).";
            }
            else
            {
                // Binary scope: any non-zero pixel is "foreground/in-scope"
                scope8 = (fg8 > 0);
                scope8.convertTo(scope8, CV_8U, 255.0);

                // Optional: slightly dilate the scope so tightening can act right on edges.
                // Keep small so you don't overreach.
                const int scopeDilatePx = 4; // try 2..8
                if (scopeDilatePx > 0)
                    cv::dilate(scope8, scope8, FSFusion::seEllipse(scopeDilatePx));

                qDebug() << "DepthScope: scopePx=" << cv::countNonZero(scope8);

                if (params.writeDepthDiagnostics)
                {
                    const QString folder = opt.depthFolderPath;
                    cv::imwrite((folder + "/dbg_scope8.png").toStdString(), scope8); // orig size
                }
            }
        }
    }



    // --- core criteria (tune these) ---
    const float confCoreMin   = std::max(params.confHigh, 0.25f);   // core must be confident
    const int   texCoreMin8   = std::max(20, params.depthTexMax + 6); // core must be textured
    const float confAmbMax    = params.depthConfStableMax;          // ambiguous zone max conf
    const int   texAmbMax8    = params.depthTexMax;                 // ambiguous zone max texture

    // Core mask: pixels we "trust" (seeds)
    cv::Mat core8 = (conf01 > confCoreMin);
    core8.convertTo(core8, CV_8U, 255.0);

    cv::Mat texCore8 = (tex8 >= texCoreMin8);
    texCore8.convertTo(texCore8, CV_8U, 255.0);

    cv::bitwise_and(core8, texCore8, core8);

    qDebug() << "DepthCore: corePx=" << cv::countNonZero(core8);

    // Ambiguous mask: pixels we are allowed to reassign (where twig fatness lives)
    cv::Mat amb8 = (conf01 < confAmbMax);
    amb8.convertTo(amb8, CV_8U, 255.0);

    cv::Mat texAmb8 = (tex8 <= texAmbMax8);
    texAmb8.convertTo(texAmb8, CV_8U, 255.0);

    cv::bitwise_and(amb8, texAmb8, amb8);

    // Constrain ambiguous zone to subject scope
    cv::bitwise_and(amb8, scope8, amb8);

    // Constrain core to subject scope
    cv::bitwise_and(core8, scope8, core8);

    qDebug() << "DepthCore: ambPx=" << cv::countNonZero(amb8);

    if (params.writeDepthDiagnostics)
    {
        const QString folder = opt.depthFolderPath;
        cv::imwrite((folder + "/dbg_core8.png").toStdString(), core8);
        cv::imwrite((folder + "/dbg_amb8.png").toStdString(), amb8);
        cv::imwrite((folder + "/dbg_tex8.png").toStdString(), tex8);
    }

    // (Optional) limit ambiguous only near boundaries so we don't disturb big flat regions
    if (params.depthBoundaryDilateStablePx >= 0)
    {
        cv::Mat boundaryGate8 = boundary8.clone();   // <-- use outer boundary8
        if (params.depthBoundaryDilateStablePx > 0)
            cv::dilate(boundaryGate8, boundaryGate8, FSFusion::seEllipse(params.depthBoundaryDilateStablePx));

        cv::bitwise_and(amb8, boundaryGate8, amb8);
        qDebug() << "DepthCore: ambPx(boundary-gated)=" << cv::countNonZero(amb8);
    }

    // BFS/Voronoi fill:
    // - keep labels on core pixels
    // - for amb pixels, assign nearest core label
    const uint16_t UNK = 0xFFFF;
    cv::Mat out16(depthBaseline16.size(), CV_16U, cv::Scalar(UNK));
    cv::Mat dist16(depthBaseline16.size(), CV_16U, cv::Scalar(0xFFFF));
    std::deque<cv::Point> q;

    // seed queue from core pixels
    for (int y = 0; y < out16.rows; ++y)
    {
        const uint8_t* c = core8.ptr<uint8_t>(y);
        const uint16_t* b = depthBaseline16.ptr<uint16_t>(y);
        uint16_t* o = out16.ptr<uint16_t>(y);
        uint16_t* d = dist16.ptr<uint16_t>(y);

        for (int x = 0; x < out16.cols; ++x)
        {
            if (!c[x]) continue;
            o[x] = b[x];
            d[x] = 0;
            q.emplace_back(x, y);
        }
    }

    if (q.empty())
    {
        qDebug() << "DepthCore: no core seeds; leaving depthTight as baseline.";
    }
    else
    {
        const int dx4[4] = { 1,-1,0,0 };
        const int dy4[4] = { 0,0,1,-1 };

        int filled = 0;
        const int maxSteps = 4096; // safety; usually not hit

        while (!q.empty())
        {
            cv::Point p = q.front(); q.pop_front();
            const int x = p.x, y = p.y;

            const uint16_t lab = out16.at<uint16_t>(y,x);
            const uint16_t dd  = dist16.at<uint16_t>(y,x);
            if (dd >= (uint16_t)maxSteps) continue;

            for (int k=0;k<4;++k)
            {
                int xx = x + dx4[k];
                int yy = y + dy4[k];
                if ((unsigned)xx >= (unsigned)out16.cols || (unsigned)yy >= (unsigned)out16.rows) continue;

                // only fill ambiguous pixels; leave non-ambiguous as baseline
                if (!amb8.at<uint8_t>(yy,xx)) continue;

                uint16_t& dstD = dist16.at<uint16_t>(yy,xx);
                if (dstD != (uint16_t)0xFFFF) continue; // visited

                out16.at<uint16_t>(yy,xx) = lab;
                dstD = (uint16_t)(dd + 1);
                q.emplace_back(xx, yy);
                filled++;
            }
        }

        qDebug() << "DepthCore: filledAmbPx=" << filled;

        // Build tightened depth:
        // - start with baseline
        // - overwrite only ambiguous pixels we successfully filled
        depthTight16 = depthBaseline16.clone();
        cv::Mat hasFill8 = (out16 != UNK);
        hasFill8.convertTo(hasFill8, CV_8U, 255.0);

        cv::Mat useFill8;
        cv::bitwise_and(hasFill8, amb8, useFill8);

        out16.copyTo(depthTight16, useFill8);

        if (params.writeDepthDiagnostics)
        {
            const QString folder = opt.depthFolderPath;

            // 0/255 map of where depth changed (tight vs baseline)
            cv::Mat delta8 = (depthTight16 != depthBaseline16);
            delta8.convertTo(delta8, CV_8U, 255.0);

            // also scope overlay so you can see if changes are inside the FG
            cv::imwrite((folder + "/dbg_depth_tight_delta.png").toStdString(), delta8);
        }

        qDebug() << "DepthCore: appliedFillPx=" << cv::countNonZero(useFill8);

        if (params.writeDepthDiagnostics)
        {
            const QString folder = opt.depthFolderPath;
            cv::imwrite((folder + "/dbg_useFill8.png").toStdString(), useFill8);
        }

        // ------------------------------------------------------------
        // FULL-SIZE subject-specific diagnostics: per-slice area + diff
        // (No ROI; these are easy to compare with baseline/fused)
        // ------------------------------------------------------------
        if (params.writeDepthDiagnostics)
        {
            const QString folder = opt.depthFolderPath;

            auto writeSliceMasks = [&](int slice)
            {
                cv::Mat mBase = (depthBaseline16 == (uint16_t)slice);
                cv::Mat mTight = (depthTight16 == (uint16_t)slice);

                mBase.convertTo(mBase, CV_8U, 255.0);
                mTight.convertTo(mTight, CV_8U, 255.0);

                cv::Mat mDiff;
                cv::bitwise_xor(mBase, mTight, mDiff);      // pixels that changed membership

                const int basePx  = cv::countNonZero(mBase);
                const int tightPx = cv::countNonZero(mTight);
                const int diffPx  = cv::countNonZero(mDiff);

                qDebug().noquote()
                    << "DepthSlice" << slice
                    << " basePx=" << basePx
                    << " tightPx=" << tightPx
                    << " delta=" << (tightPx - basePx)
                    << " diffPx=" << diffPx;

                cv::imwrite((folder + QString("/dbg_slice%1_base.png").arg(slice)).toStdString(),  mBase);
                cv::imwrite((folder + QString("/dbg_slice%1_tight.png").arg(slice)).toStdString(), mTight);
                cv::imwrite((folder + QString("/dbg_slice%1_diff.png").arg(slice)).toStdString(),  mDiff);
            };

            // You have some confusion in the thread between slice 5 vs 6.
            // Write BOTH so we stop guessing.
            writeSliceMasks(5);
            writeSliceMasks(6);

            // ---------------------------------------------------------------------
            // OPTIONAL: Use manual foreground mask (fg.png) to judge "fatness"
            // Writes full-size overlays + prints inside/outside-band stats.
            // Put this INSIDE: if (params.writeDepthDiagnostics) { ... }  (near the end)
            // ---------------------------------------------------------------------
            {
                const QString folder = opt.depthFolderPath;
                const QString fgPath = folder + "/fg.png";

                cv::Mat fg;
                fg = cv::imread(fgPath.toStdString(), cv::IMREAD_UNCHANGED);

                if (!fg.empty())
                {
                    // Make fg8: 0/255, same size
                    cv::Mat fg8;
                    if (fg.channels() == 4)
                    {
                        std::vector<cv::Mat> ch;
                        cv::split(fg, ch);
                        fg8 = ch[3]; // alpha
                    }
                    else if (fg.channels() == 3)
                    {
                        cv::cvtColor(fg, fg8, cv::COLOR_BGR2GRAY);
                    }
                    else
                    {
                        fg8 = fg;
                    }

                    if (fg8.size() != depthBaseline16.size())
                    {
                        qWarning().noquote() << "WARNING:" << srcFun
                                             << "fg.png size mismatch. fg="
                                             << fg8.cols << "x" << fg8.rows
                                             << " depth=" << depthBaseline16.cols << "x" << depthBaseline16.rows;
                    }
                    else
                    {
                        // Ensure binary 0/255
                        cv::threshold(fg8, fg8, 0, 255, cv::THRESH_BINARY);

                        // Build full-size heatmaps (already doing elsewhere; reuse here if you want)
                        cv::Mat heatBase = FSUtilities::depthHeatmap(depthBaseline16, N, "Depth (baseline)");
                        cv::Mat heatTight = FSUtilities::depthHeatmap(depthTight16, N, "Depth (tightened)");

                        // Draw FG outline (contours) on heatmaps
                        std::vector<std::vector<cv::Point>> contours;
                        cv::findContours(fg8.clone(), contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);

                        // Thick black then thin white = visible on any color
                        cv::drawContours(heatBase,  contours, -1, cv::Scalar(0,0,0), 3, cv::LINE_AA);
                        cv::drawContours(heatBase,  contours, -1, cv::Scalar(255,255,255), 1, cv::LINE_AA);
                        cv::drawContours(heatTight, contours, -1, cv::Scalar(0,0,0), 3, cv::LINE_AA);
                        cv::drawContours(heatTight, contours, -1, cv::Scalar(255,255,255), 1, cv::LINE_AA);

                        cv::imwrite((folder + "/depth_baseline_fgOutline.png").toStdString(), heatBase);
                        cv::imwrite((folder + "/depth_tight_fgOutline.png").toStdString(), heatTight);

                        // Build a thin outside band around FG for "spill" measurement
                        const int bandPx = 8; // start 6..12
                        cv::Mat fgDil;
                        cv::dilate(fg8, fgDil, FSFusion::seEllipse(bandPx));
                        cv::Mat band8;
                        cv::subtract(fgDil, fg8, band8); // outside ring only (0/255)

                        // Helper: count pixels for a slice inside fg and in outside band
                        auto countInMask = [&](const cv::Mat& depth16, int slice, const cv::Mat& mask8)->int {
                            cv::Mat m = (depth16 == (uint16_t)slice);
                            m.convertTo(m, CV_8U, 255.0);
                            cv::bitwise_and(m, mask8, m);
                            return cv::countNonZero(m);
                        };

                        // Report for slices you care about (5/6 here; adjust as needed)
                        for (int s : {5, 6})
                        {
                            const int baseIn  = countInMask(depthBaseline16, s, fg8);
                            const int tightIn = countInMask(depthTight16,    s, fg8);

                            const int baseOut  = countInMask(depthBaseline16, s, band8);
                            const int tightOut = countInMask(depthTight16,    s, band8);

                            qDebug().noquote()
                                << "FGStats slice" << s
                                << " IN(base/tight)=" << baseIn  << "/" << tightIn
                                << " OUTband(base/tight)=" << baseOut << "/" << tightOut;
                        }

                        // Optional: write spill masks (full size) for a slice
                        const int s = 6; // pick twig slice
                        cv::Mat spillBase = (depthBaseline16 == (uint16_t)s);
                        spillBase.convertTo(spillBase, CV_8U, 255.0);
                        cv::bitwise_and(spillBase, band8, spillBase);
                        cv::imwrite((folder + "/spill_slice6_baseline.png").toStdString(), spillBase);

                        cv::Mat spillTight = (depthTight16 == (uint16_t)s);
                        spillTight.convertTo(spillTight, CV_8U, 255.0);
                        cv::bitwise_and(spillTight, band8, spillTight);
                        cv::imwrite((folder + "/spill_slice6_tight.png").toStdString(), spillTight);
                    }
                }
                else
                {
                    qDebug() << "FGStats: fg.png not found at" << fgPath;
                }
            }
        }
    }

    // ---------------------------------------------------------------------
    // 2) “Stable labels” pass: majority filter only in boundary zones
    // Again boundary-limited to prevent outward growth.
    // ---------------------------------------------------------------------
    cv::Mat depthStable16 = depthTight16.clone();
    {
        cv::Mat stableMask8;
        cv::bitwise_and(lowConfStable8, boundaryStable8, stableMask8);
        cv::bitwise_and(stableMask8, lowTex8, stableMask8);

        const int px = cv::countNonZero(stableMask8);
        qDebug() << "DepthTight: stableMaskPx=" << px;

        if (px > 0)
        {
            cv::Mat filt16 = majorityFilterLabels16_3x3(depthStable16);
            filt16.copyTo(depthStable16, stableMask8);
        }
    }

    if (FSFusion::isAbort(abortFlag)) return false;

    // ---------------------------------------------------------------------
    // Optional experimental hook
    // (Uses baseline OR tightened baseline? I recommend baseline input + conf + topk.)
    // ---------------------------------------------------------------------
    // cv::Mat depthUsed16 = depthStable16; // default: tightened depth for fusion
    cv::Mat depthUsed16 = depthTight16; // default: tightened depth for fusion
    if (params.depthMode == DepthMode::Experimental && params.depthHook)
    {
        cv::Mat depthExp16;
        bool ok = params.depthHook(depthBaseline16, conf01, topkOrig, N, depthExp16);

        if (ok && !depthExp16.empty())
        {
            CV_Assert(depthExp16.type() == CV_16U);
            CV_Assert(depthExp16.size() == depthBaseline16.size());
            depthUsed16 = depthExp16;
        }
        else
        {
            qWarning().noquote() << "WARNING:" << srcFun
                                 << "depthHook failed; falling back to tightened baseline.";
            depthUsed16 = depthTight16;
        }
    }
    {
        cv::Mat d = (depthUsed16 != depthBaseline16);
        d.convertTo(d, CV_8U, 255);
        const int changed = cv::countNonZero(d);
        qDebug() << "DepthUsed: changedPx=" << changed;

        if (params.writeDepthDiagnostics) {
            const QString folder = opt.depthFolderPath;
            cv::imwrite((folder + "/depth_diff_used.png").toStdString(), d); // orig size
        }
    }

    // ---------------------------------------------------------------------
    // Diagnostics (orig size only)
    // IMPORTANT: your FSUtilities::depthHeatmap should draw a title (top/left)
    // ---------------------------------------------------------------------
    if (params.writeDepthDiagnostics)
    {
        const QString folder = opt.depthFolderPath;

        cv::imwrite((folder + "/depth_baseline.png").toStdString(),
                    FSUtilities::depthHeatmap(depthBaseline16, N, "Depth (baseline)"));

        cv::imwrite((folder + "/depth_tight.png").toStdString(),
                    FSUtilities::depthHeatmap(depthTight16, N, "Depth (tightened)"));

        cv::imwrite((folder + "/depth_stable.png").toStdString(),
                    FSUtilities::depthHeatmap(depthStable16, N, "Depth (stable labels)"));

        if (params.depthMode == DepthMode::Experimental)
            cv::imwrite((folder + "/depth_experimental.png").toStdString(),
                        FSUtilities::depthHeatmap(depthUsed16, N, "Depth (experimental)"));
    }

    // ---------------------------------------------------------------------
    // Outputs
    // depthIndex16: depth used for pyramids/fusion
    // winStable16 : stable labels for halo boundary detection
    // ---------------------------------------------------------------------
    depthIndex16 = depthUsed16.clone();
    winStable16  = depthStable16.clone();

    // quick sanity logging
    {
        const long long diffBase  = countHorizontalDiffs16(depthBaseline16);
        const long long diffTight = countHorizontalDiffs16(depthTight16);
        const long long diffUsed  = countHorizontalDiffs16(depthIndex16);
        qDebug() << "Depth diffs: base=" << diffBase
                 << " tight=" << diffTight
                 << " used="  << diffUsed;
    }

    return true;
}

// Pyramid prep
int FSFusionDMap::computePyrLevels(const cv::Size& origSz) const
{
    int levels = params.pyrLevels;
    if (levels <= 0) {
        int dim = std::max(origSz.width, origSz.height);
        levels = 5;
        while ((dim >> levels) > 8 && levels < 8) levels++;
    }
    return std::max(3, std::min(10, levels));
}

void FSFusionDMap::buildPyramids(const cv::Mat& depthIndex16,
                                 int levels,
                                 std::vector<cv::Mat>& idxPyr16,
                                 std::vector<cv::Mat>& vetoPyr8) const
{
    FusionPyr::buildIndexPyrNearest(depthIndex16, levels, idxPyr16);

    vetoPyr8.clear();
    if (params.enableDepthGradLowpassVeto) {
        FusionPyr::DepthEdgeVetoParams vp;
        vp.vetoFromLevel   = params.vetoFromLevel;
        vp.depthGradThresh = params.depthGradThresh;
        vp.vetoDilatePx    = params.vetoDilatePx;
        FusionPyr::buildDepthEdgeVetoPyr(idxPyr16, vetoPyr8, vp);
    }
}

void FSFusionDMap::initAccumulator(const cv::Size& origSz,
                                   int levels,
                                   FusionPyr::PyrAccum& A,
                                   cv::Mat& sumW) const
{
    A.reset(origSz, levels);
    sumW = cv::Mat(origSz, CV_32F, cv::Scalar(0));
}

// Streaming accumulation loop
bool FSFusionDMap::accumulateAllSlices(const QString& srcFun,
                                       const QStringList& inputPaths,
                                       const std::vector<Result>& globals,
                                       const cv::Rect& roiPadToAlign,
                                       const cv::Rect& validAreaAlign,
                                       const TopKMaps& topkOrig,
                                       const DMapWeights& W,
                                       const std::vector<cv::Mat>& idxPyr16,
                                       const std::vector<cv::Mat>& vetoPyr8,
                                       int levels,
                                       std::atomic_bool* abortFlag,
                                       FSFusion::StatusCallback statusCb,
                                       FSFusion::ProgressCallback progressCb,
                                       FusionPyr::PyrAccum& A,
                                       cv::Mat& sumW) const
{
    const int N = inputPaths.size();
    const cv::Size origSz = topkOrig.idx16[0].size();

    for (int s = 0; s < N; ++s)
    {
        progressCb();
        statusCb("Fusing Slice: " + QString::number(s) + " of " + QString::number(N) + " ");

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
    return true;
}

// Finalize + clamp
cv::Mat FSFusionDMap::finalizeOut32(const FusionPyr::PyrAccum& A) const
{
    cv::Mat out32 = FusionPyr::finalizeBlend(A, /*eps=*/1e-8f);
    cv::max(out32, 0.0f, out32);
    cv::min(out32, 1.0f, out32);
    return out32;
}

// Halo debug helpers
void FSFusionDMap::debugHaloInputs(const cv::Mat& out32,
                                   const TopKMaps& topkOrig,
                                   const cv::Mat& conf01) const
{
    dbgMat3Stats("DBG out32 preHalo clamp", out32);
    dbgMat3Stats("DBG top1Score32", topkOrig.score32[0]);
    dbgMat3Stats("DBG conf01", conf01);
}

void FSFusionDMap::debugHaloOutput(const cv::Mat& out32) const
{
    dbgMat3Stats("DBG out32 postHalo clamp", out32);
}

// HaloFix
void FSFusionDMap::applyHaloFixIfEnabled(const FSFusion::Options& opt,
                                         const QString& srcFun,
                                         int N,
                                         const TopKMaps& topkOrig,
                                         const cv::Mat& conf01,
                                         const cv::Mat& winStable16,
                                         const QStringList& inputPaths,
                                         const std::vector<Result>& globals,
                                         const cv::Rect& roiPadToAlign,
                                         const cv::Rect& validAreaAlign,
                                         std::atomic_bool* abortFlag,
                                         cv::Mat& out32)
{
    if (!params.enableHaloRingFix) return;
    if (FSFusion::isAbort(abortFlag)) return;

    const int ringPx = std::max(2, params.haloRingPx);



    // EXPERIMENTAL FG: based purely on depth discontinuities

    cv::Mat jumpEdges8 = buildDepthJumpEdges8(winStable16, 3, 1);

    // Invert edges to get smooth regions
    cv::Mat regionMask;
    cv::bitwise_not(jumpEdges8, regionMask);

    // Label connected regions
    cv::Mat ccLabels, stats, centroids;
    int ncc = cv::connectedComponentsWithStats(regionMask, ccLabels, stats, centroids, 8, CV_32S);
    if (ncc <= 1) {
        qDebug() << "HaloFix: no CC regions found; skipping.";
        return;
    }

    // Score each region by total high-confidence mass
    std::vector<double> regionScore(ncc, 0.0);

    for (int y = 0; y < conf01.rows; ++y) {
        const float* c = conf01.ptr<float>(y);
        const int*   L = ccLabels.ptr<int>(y);
        for (int x = 0; x < conf01.cols; ++x) {
            int lab = L[x];
            if (lab > 0) regionScore[lab] += c[x];
        }
    }

    // Pick region with highest score
    int bestRegion = std::max_element(regionScore.begin()+1, regionScore.end()) - regionScore.begin();
    if (bestRegion <= 0 || bestRegion >= ncc) return;

    cv::Mat fg8 = (ccLabels == bestRegion);
    fg8.convertTo(fg8, CV_8U, 255.0);

    // -------------------------------
    // HOLE-SAFE mask for boundary/band
    // -------------------------------

    // Fill holes in a binary mask (CV_8U 0/255) robustly, even if FG touches image border.
    auto fillHoles8 = [](const cv::Mat& bin8)->cv::Mat
    {
        CV_Assert(bin8.type() == CV_8U);

        // Invert: background -> 255, foreground -> 0
        cv::Mat inv;
        cv::bitwise_not(bin8, inv);

        // Pad with a 1-pixel 255 border so (0,0) is guaranteed to be "outside background" in inv-space.
        cv::Mat invPad;
        cv::copyMakeBorder(inv, invPad, 1, 1, 1, 1, cv::BORDER_CONSTANT, cv::Scalar(255));

        // Flood fill from the guaranteed-outside corner, turning outside background to 0
        cv::floodFill(invPad, cv::Point(0, 0), cv::Scalar(0));

        // Crop back to original size
        cv::Mat ff = invPad(cv::Rect(1, 1, bin8.cols, bin8.rows)).clone();

        // Holes are the remaining 255 regions in ff (not connected to outside)
        cv::Mat holesMask;
        cv::compare(ff, 0, holesMask, cv::CMP_GT); // 255 where holes

        // Fill holes into the foreground
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
        return;
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
        return;
    }

    double mn=0, mx=0;
    cv::minMaxLoc(fg8, &mn, &mx);
    qDebug() << "HaloFix: fg8 min/max =" << mn << mx;

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
        return;
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
        return;
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
}

void FSFusionDMap::convertOut32ToOutput(const cv::Mat& out32, cv::Mat& outputColor) const
{
    cv::Mat clamped = out32.clone();
    cv::max(clamped, 0.0f, clamped);
    cv::min(clamped, 1.0f, clamped);

    if (outDepth == CV_16U)
        clamped.convertTo(outputColor, CV_16UC3, 65535.0);
    else
        clamped.convertTo(outputColor, CV_8UC3, 255.0);
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

        // decide pad size from align using SAME logic as streamFinish
        params.topK = std::max(1, std::min(8, params.topK));

        // Use identical pyramid-level computation as streamFinish
        const int levelsForPad = computePyrLevels(alignSize);

        // Optionally lock it so both passes agree explicitly
        params.pyrLevels = levelsForPad;

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
    const QString srcFun = "FSFusionDMap::streamFinish";
    if (G::FSLog) G::log(srcFun);

    int N = 0;
    if (!validateStreamFinishState(srcFun, inputPaths, globals, abortFlag, N))
        return false;

    cv::Rect roiPadToAlign;
    cv::Size origSz;
    if (!computeCropGeometry(srcFun, roiPadToAlign, origSz))
        return false;

    TopKMaps topkOrig;
    if (!cropTopKToOrig(srcFun, roiPadToAlign, origSz, topkOrig))
        return false;

    if (!sanityCheckTop1(srcFun, topkOrig, N))
        return false;

    DMapWeights W;
    cv::Mat conf01, depthIndexStable16;
    if (!buildMapsAndStabilize(srcFun, topkOrig, N, abortFlag,
                               W, conf01, depthIndex16, depthIndexStable16))
        return false;

    // Optional experimental depth
    if (!depthExperiment(srcFun, opt, topkOrig, N, conf01, abortFlag,
                         depthIndex16, depthIndexStable16))
        return false;

    int levels = computePyrLevels(origSz);

    std::vector<cv::Mat> idxPyr16;
    std::vector<cv::Mat> vetoPyr8;
    buildPyramids(depthIndex16, levels, idxPyr16, vetoPyr8);

    FusionPyr::PyrAccum A;
    cv::Mat sumW;
    initAccumulator(origSz, levels, A, sumW);

    if (!accumulateAllSlices(srcFun, inputPaths, globals,
                             roiPadToAlign, validAreaAlign,
                             topkOrig, W, idxPyr16, vetoPyr8,
                             levels, abortFlag, statusCb, progressCb,
                             A, sumW))
        return false;

    cv::Mat out32 = finalizeOut32(A);

    // Optional debug
    debugHaloInputs(out32, topkOrig, conf01);

    // Halo fix
    applyHaloFixIfEnabled(opt, srcFun, N, topkOrig, conf01, depthIndexStable16,
                          inputPaths, globals, roiPadToAlign, validAreaAlign,
                          abortFlag, out32);

    debugHaloOutput(out32);

    convertOut32ToOutput(out32, outputColor);

    reset();
    return true;
}
