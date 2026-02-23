#include "fsfusiondmap.h"

#include "Main/global.h"
#include "fsloader.h"
#include "fsutilities.h"

// your shared helpers:
// #include "fsfusiondmapshared.h"

#include <opencv2/imgcodecs.hpp>
#include <cmath>
#include <algorithm>

namespace
{

static cv::Size computePadSizeForPyr(const cv::Size& s, int levels)
{
    const int lv = std::max(1, levels);
    const int m  = 1 << std::max(0, lv - 1);
    const int w  = ((s.width  + m - 1) / m) * m;
    const int h  = ((s.height + m - 1) / m) * m;
    return cv::Size(w, h);
}

static     cv::Mat padCenterReflect(const cv::Mat& src, const cv::Size& dstSize)
{
    /*
    This pads the aligned image to padSize, using reflection at the borders,
    centered. This prevents edge artifacts in the focus metric and keeps all slices
    the same padded size.
    */
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

// robust max: 99th percentile
static float robustMax99(const cv::Mat& m32f)
{
    CV_Assert(m32f.type() == CV_32F);
    cv::Mat v = m32f.reshape(1, 1).clone();
    cv::sort(v, v, cv::SORT_ASCENDING);
    int idx = (int)std::floor(0.99 * (v.cols - 1));
    return v.at<float>(0, std::max(0, std::min(idx, v.cols-1)));
}

static cv::Mat morphClose8(const cv::Mat& bin8, int px)
{
    CV_Assert(bin8.type() == CV_8U);
    if (bin8.empty() || px <= 0) return bin8.clone();
    cv::Mat out = bin8.clone();
    cv::morphologyEx(out, out, cv::MORPH_CLOSE, FSFusion::seEllipse(px));
    return out;
}

static cv::Mat fillHoles8(const cv::Mat& bin8)
{
    CV_Assert(bin8.type() == CV_8U);

    cv::Mat inv;
    cv::bitwise_not(bin8, inv);

    cv::Mat invPad;
    cv::copyMakeBorder(inv, invPad, 1, 1, 1, 1,
                       cv::BORDER_CONSTANT, cv::Scalar(255));

    cv::floodFill(invPad, cv::Point(0, 0), cv::Scalar(0));

    cv::Mat ff = invPad(cv::Rect(1, 1, bin8.cols, bin8.rows)).clone();

    cv::Mat holesMask;
    cv::compare(ff, 0, holesMask, cv::CMP_GT); // 255 where holes

    cv::Mat filled = bin8.clone();
    filled.setTo(255, holesMask);
    return filled;
}

// stable8: 0/255, seed8: 0/255
// returns only stable components that touch seed
static cv::Mat keepCCsTouchingMask(const cv::Mat& bin8,
                            const cv::Mat& touch8,
                            int connectivity)
{
    CV_Assert(bin8.type() == CV_8U && touch8.type() == CV_8U);
    CV_Assert(bin8.size() == touch8.size());

    cv::Mat src, touch;
    cv::compare(bin8, 0, src,   cv::CMP_GT); // 0/255
    cv::compare(touch8, 0, touch, cv::CMP_GT);

    if (cv::countNonZero(src) == 0 || cv::countNonZero(touch) == 0)
        return cv::Mat::zeros(bin8.size(), CV_8U);

    cv::Mat labels, stats, centroids;
    int n = cv::connectedComponentsWithStats(src, labels, stats, centroids, connectivity, CV_32S);
    if (n <= 1) return cv::Mat::zeros(bin8.size(), CV_8U);

    // Mark labels that touch touch-mask
    std::vector<uint8_t> keep(n, 0);

    for (int y = 0; y < labels.rows; ++y)
    {
        const int* L = labels.ptr<int>(y);
        const uint8_t* T = touch.ptr<uint8_t>(y);
        for (int x = 0; x < labels.cols; ++x)
        {
            if (!T[x]) continue;
            int id = L[x];
            if (id > 0) keep[id] = 1;
        }
    }

    cv::Mat out = cv::Mat::zeros(bin8.size(), CV_8U);
    for (int y = 0; y < labels.rows; ++y)
    {
        const int* L = labels.ptr<int>(y);
        uint8_t* O = out.ptr<uint8_t>(y);
        for (int x = 0; x < labels.cols; ++x)
        {
            int id = L[x];
            if (id > 0 && keep[id]) O[x] = 255;
        }
    }
    return out;
}

static int defaultRingPx(const cv::Size& sz)
{
    // ~0.6% of max dimension, clamped
    int m = std::max(sz.width, sz.height);
    int r = (int)std::round(m * 0.01);
    return std::max(12, std::min(80, r));
}

static cv::Mat ringOutsideFg8(const cv::Mat& fg8, int ringPx)
{
    CV_Assert(fg8.type() == CV_8U);
    if (fg8.empty() || ringPx <= 0) return cv::Mat::zeros(fg8.size(), CV_8U);

    cv::Mat dil;
    cv::dilate(fg8, dil, FSFusion::seEllipse(ringPx));

    cv::Mat ring = dil.clone();
    ring.setTo(0, fg8); // ring = dilate - fg
    return ring;
}

static cv::Mat lowContrastMask8_fromTop1Score(const cv::Mat& top1Score32,
                                       float contrastMinFrac,
                                       float minAbs = 1e-12f)
{
    qDebug() << "10";

    CV_Assert(top1Score32.type() == CV_32F);

    const float p99 = robustMax99(top1Score32);
    const float thr = std::max(p99 * std::max(0.0f, contrastMinFrac), minAbs);

    cv::Mat low = (top1Score32 < thr);
    low.convertTo(low, CV_8U, 255.0);
    return low;
}

static bool ownershipPropagateTwoPass_Outward(
    const cv::Mat& fg8,
    const cv::Mat& depthIndex16,
    int ringPx,
    int seedBandPx,          // NEW: thickness of seed band outside FG (use 1 or 2)
    cv::Mat& overrideMask8,
    cv::Mat& overrideWinner16)
{
    CV_Assert(!fg8.empty() && fg8.type() == CV_8U);
    CV_Assert(!depthIndex16.empty() && depthIndex16.type() == CV_16U);
    CV_Assert(fg8.size() == depthIndex16.size());

    const cv::Size sz = fg8.size();

    // Ring where we will override (outside FG only)
    overrideMask8 = ringOutsideFg8(fg8, ringPx);
    if (cv::countNonZero(overrideMask8) == 0) {
        overrideWinner16.release();
        return true;
    }

    // --- Background-side seeds: a thin band just OUTSIDE FG ---
    seedBandPx = std::max(1, seedBandPx);

    cv::Mat fgDil;
    cv::dilate(fg8, fgDil, FSFusion::seEllipse(seedBandPx));

    cv::Mat seed8 = fgDil.clone();
    seed8.setTo(0, fg8);                 // seed = dilate(FG) - FG  (outside FG)

    // Optional: only seeds inside the ring (keeps it local)
    seed8.setTo(0, overrideMask8 == 0);

    if (cv::countNonZero(seed8) == 0) {
        // No seeds -> nothing we can propagate; don't override.
        overrideWinner16.release();
        overrideMask8.setTo(0);
        return true;
    }

    // Chamfer 2-pass
    const uint16_t INF = (uint16_t)60000;
    cv::Mat dist16(sz, CV_16U, cv::Scalar(INF));
    overrideWinner16 = cv::Mat(sz, CV_16U, cv::Scalar(0));

    // Init seeds: distance=0, donor = depthIndex16 at seed (BACKGROUND owners!)
    for (int y = 0; y < sz.height; ++y) {
        const uint8_t* s = seed8.ptr<uint8_t>(y);
        const uint16_t* d = depthIndex16.ptr<uint16_t>(y);
        uint16_t* dist = dist16.ptr<uint16_t>(y);
        uint16_t* donor = overrideWinner16.ptr<uint16_t>(y);
        for (int x = 0; x < sz.width; ++x) {
            if (s[x]) { dist[x] = 0; donor[x] = d[x]; }
        }
    }

    const uint16_t C1 = 3, C2 = 4;

    // Pass 1
    for (int y = 0; y < sz.height; ++y) {
        uint16_t* dist = dist16.ptr<uint16_t>(y);
        uint16_t* donor = overrideWinner16.ptr<uint16_t>(y);
        for (int x = 0; x < sz.width; ++x) {
            uint16_t bestD = dist[x], bestS = donor[x];

            if (x > 0) {
                uint16_t cand = (uint16_t)std::min<int>(INF, dist[x-1] + C1);
                if (cand < bestD) { bestD = cand; bestS = donor[x-1]; }
            }
            if (y > 0) {
                const uint16_t* distUp = dist16.ptr<uint16_t>(y-1);
                const uint16_t* donorUp = overrideWinner16.ptr<uint16_t>(y-1);

                uint16_t cand = (uint16_t)std::min<int>(INF, distUp[x] + C1);
                if (cand < bestD) { bestD = cand; bestS = donorUp[x]; }

                if (x > 0) {
                    cand = (uint16_t)std::min<int>(INF, distUp[x-1] + C2);
                    if (cand < bestD) { bestD = cand; bestS = donorUp[x-1]; }
                }
                if (x + 1 < sz.width) {
                    cand = (uint16_t)std::min<int>(INF, distUp[x+1] + C2);
                    if (cand < bestD) { bestD = cand; bestS = donorUp[x+1]; }
                }
            }
            dist[x] = bestD;
            donor[x] = bestS;
        }
    }

    // Pass 2
    for (int y = sz.height - 1; y >= 0; --y) {
        uint16_t* dist = dist16.ptr<uint16_t>(y);
        uint16_t* donor = overrideWinner16.ptr<uint16_t>(y);
        for (int x = sz.width - 1; x >= 0; --x) {
            uint16_t bestD = dist[x], bestS = donor[x];

            if (x + 1 < sz.width) {
                uint16_t cand = (uint16_t)std::min<int>(INF, dist[x+1] + C1);
                if (cand < bestD) { bestD = cand; bestS = donor[x+1]; }
            }
            if (y + 1 < sz.height) {
                const uint16_t* distDn = dist16.ptr<uint16_t>(y+1);
                const uint16_t* donorDn = overrideWinner16.ptr<uint16_t>(y+1);

                uint16_t cand = (uint16_t)std::min<int>(INF, distDn[x] + C1);
                if (cand < bestD) { bestD = cand; bestS = donorDn[x]; }

                if (x > 0) {
                    cand = (uint16_t)std::min<int>(INF, distDn[x-1] + C2);
                    if (cand < bestD) { bestD = cand; bestS = donorDn[x-1]; }
                }
                if (x + 1 < sz.width) {
                    cand = (uint16_t)std::min<int>(INF, distDn[x+1] + C2);
                    if (cand < bestD) { bestD = cand; bestS = donorDn[x+1]; }
                }
            }
            dist[x] = bestD;
            donor[x] = bestS;
        }
    }

    // IMPORTANT: clear overrideMask where no seed was reachable
    for (int y = 0; y < sz.height; ++y) {
        uint8_t* ring = overrideMask8.ptr<uint8_t>(y);
        uint16_t* dist = dist16.ptr<uint16_t>(y);
        for (int x = 0; x < sz.width; ++x) {
            if (!ring[x]) continue;
            if (dist[x] >= INF) ring[x] = 0;
        }
    }

    return true;
}



static cv::Mat stableDepthMask8_rangeLe(const cv::Mat& depth16, int r, int maxRange)
{
    CV_Assert(depth16.type() == CV_16U);
    r = std::max(1, r);
    maxRange = std::max(0, maxRange);

    cv::Mat k = cv::getStructuringElement(cv::MORPH_ELLIPSE, cv::Size(2*r+1, 2*r+1));

    cv::Mat localMin16, localMax16;
    cv::erode(depth16,  localMin16, k);
    cv::dilate(depth16, localMax16, k);

    cv::Mat diff16;
    cv::subtract(localMax16, localMin16, diff16); // CV_16U

    cv::Mat stable = (diff16 <= (uint16_t)maxRange);
    stable.convertTo(stable, CV_8U, 255);
    return stable;
}

static cv::Mat focusMetric32Laplacian(const cv::Mat& gray8,
                                      float preBlurSigma,
                                      int lapKSize)
{
    /*
    This computes a per-pixel focus/sharpness score from a grayscale 8-bit image
    using (optionally pre-blurred) Laplacian magnitude, then lightly smooths the
    result.

    Because it’s based on Laplacian magnitude, it likes edges and contrasty
    micro-detail. That can make it sensitive to:
    •	noise (mitigated by preBlurSigma + the 0.8 blur),
    •	halos/edge artifacts (they can still produce Laplacian energy even if lower
        contrast than the subject, depending on blur scale and edge shape).

    Tried using a tennengrad version, but it created false detail.
    */
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

// buildFgFromTop1AndDepth
// - Conservative FG core (halo-safe) + controlled interior expansion (fixes “blue arrow”)
// - Key idea: NEVER let maxRange>1 define the boundary. Use it only as an *interior grow* candidate,
//   gated by (a) weak silhouette from top1Score, (b) texture threshold, and (c) geodesic growth
//   from the eroded FG core.
static cv::Mat buildFgFromTop1AndDepth(
                            const cv::Mat& top1Score32,
                            const cv::Mat& depthIndex16,
                            int depthStableRadiusPx,
                            int depthMaxRangeSlicesCore,
                            int depthMaxRangeSlicesLoose,
                            float strongFrac,
                            float weakFrac,
                            int seedDilatePx,
                            int closePx,
                            int openPx,
                            int interiorPx,
                            float expandTexFrac /* NEW: e.g. 0.02f */)
{
    CV_Assert(top1Score32.type() == CV_32F);
    CV_Assert(depthIndex16.type() == CV_16U);
    CV_Assert(top1Score32.size() == depthIndex16.size());

    depthStableRadiusPx = std::max(1, depthStableRadiusPx);
    depthMaxRangeSlicesCore  = std::max(0, depthMaxRangeSlicesCore);
    depthMaxRangeSlicesLoose = std::max(0, depthMaxRangeSlicesLoose);
    seedDilatePx = std::max(0, seedDilatePx);
    closePx = std::max(0, closePx);
    openPx  = std::max(0, openPx);
    interiorPx = std::max(0, interiorPx);
    expandTexFrac = std::max(0.0f, expandTexFrac);

    // Ensure strong is stricter than weak
    float sFrac = strongFrac, wFrac = weakFrac;
    if (sFrac < wFrac) std::swap(sFrac, wFrac);

    const float mx = robustMax99(top1Score32);
    const float strongThr = std::max(1e-12f, mx * sFrac);
    const float weakThr   = std::max(1e-12f, mx * wFrac);

    // -----------------------------
    // 1) Seeds (strong texture)
    // -----------------------------
    cv::Mat seed8 = (top1Score32 >= strongThr);
    seed8.convertTo(seed8, CV_8U, 255.0);
    if (seedDilatePx > 0)
        cv::dilate(seed8, seed8, FSFusion::seEllipse(seedDilatePx));

    // -----------------------------
    // 2) Weak silhouette from top1Score (connects outline)
    // -----------------------------
    cv::Mat weak8 = (top1Score32 >= weakThr);
    weak8.convertTo(weak8, CV_8U, 255.0);

    if (closePx > 0)
        cv::morphologyEx(weak8, weak8, cv::MORPH_CLOSE, FSFusion::seEllipse(closePx));

    weak8 = fillHoles8(weak8);

    // Keep only weak regions that are connected to seeds (hysteresis)
    weak8 = keepCCsTouchingMask(weak8, seed8, 8);

    // -----------------------------
    // 3) Halo-safe stable depth core
    // -----------------------------
    cv::Mat stableCore8 = stableDepthMask8_rangeLe(depthIndex16,
                                                   depthStableRadiusPx,
                                                   depthMaxRangeSlicesCore);

    cv::Mat insideCore8;
    cv::bitwise_and(stableCore8, weak8, insideCore8);

    cv::Mat fgCore8 = keepCCsTouchingMask(insideCore8, seed8, 8);

    // If nothing, return early
    if (cv::countNonZero(fgCore8) == 0) {
        return cv::Mat::zeros(top1Score32.size(), CV_8U);
    }

    // Optional gentle close on the core only (connect tiny twigs)
    if (closePx > 0)
        cv::morphologyEx(fgCore8, fgCore8, cv::MORPH_CLOSE, FSFusion::seEllipse(std::min(2, closePx)));

    // -----------------------------
    // 4) Controlled interior expansion (fix “blue arrow”)
    //     - allow maxRangeLoose > core, but only:
    //       (a) inside weak silhouette
    //       (b) enough texture (expandTexFrac)
    //       (c) reachable by geodesic growth from eroded fgCore
    // -----------------------------
    cv::Mat fg8 = fgCore8.clone();

    if (depthMaxRangeSlicesLoose > depthMaxRangeSlicesCore && expandTexFrac > 0.0f)
    {
        cv::Mat stableLoose8 = stableDepthMask8_rangeLe(depthIndex16,
                                                        depthStableRadiusPx,
                                                        depthMaxRangeSlicesLoose);

        // texture gate (prevents halos entering)
        const float texThr = std::max(1e-12f, mx * expandTexFrac);
        cv::Mat tex8 = (top1Score32 >= texThr);
        tex8.convertTo(tex8, CV_8U, 255.0);

        // allowed region to grow into
        cv::Mat allow8;
        {
            cv::Mat t;
            cv::bitwise_and(stableLoose8, weak8, t);
            cv::bitwise_and(t, tex8, allow8);
        }

        // growth seed = interior of fgCore8
        cv::Mat seedGrow8;
        if (interiorPx > 0)
            cv::erode(fgCore8, seedGrow8, FSFusion::seEllipse(interiorPx));
        else
            seedGrow8 = fgCore8.clone();

        // If erosion killed it, fall back to fgCore
        if (cv::countNonZero(seedGrow8) == 0)
            seedGrow8 = fgCore8.clone();

        // geodesic dilation: grow inside allow8
        cv::Mat grow = seedGrow8.clone();
        const int itMax = std::max(6, interiorPx * 4); // good default growth budget
        for (int it = 0; it < itMax; ++it)
        {
            cv::Mat d;
            cv::dilate(grow, d, FSFusion::seEllipse(1));
            cv::bitwise_and(d, allow8, d);

            // stop if converged
            cv::Mat diff;
            cv::bitwise_xor(d, grow, diff);
            if (cv::countNonZero(diff) == 0) break;

            grow = d;
        }

        fg8 = fgCore8 | grow;
    }

    // -----------------------------
    // 5) Cleanup (IMPORTANT: keep conservative edge)
    // -----------------------------
    if (openPx > 0)
        cv::morphologyEx(fg8, fg8, cv::MORPH_OPEN, FSFusion::seEllipse(openPx));

    if (closePx > 0)
        cv::morphologyEx(fg8, fg8, cv::MORPH_CLOSE, FSFusion::seEllipse(closePx));

    fg8 = fillHoles8(fg8);
    cv::threshold(fg8, fg8, 127, 255, cv::THRESH_BINARY);

    return fg8;
}


} // end anon namespace

FSFusionDMap::FSFusionDMap()
{
    reset();
}

void FSFusionDMap::reset()
{
    active_ = false;
    sliceCount_ = 0;

    padSize = {};
    idx0_pad16.release();
    idx1_pad16.release();
    s0_pad32.release();
    s1_pad32.release();
    winIdx16_.release();
    top1Score32_.release();
}

int FSFusionDMap::computePyrLevels(const cv::Size& origSz) const
{
/*
Ensure dimensions are friendly for pyramid/downsampling
(powers of 2 / divisible sizes).
*/
    int levels = o.pyrLevels;
    if (levels <= 0) {
        int dim = std::max(origSz.width, origSz.height);
        levels = 5;
        while ((dim >> levels) > 8 && levels < 8) levels++;
    }
    return std::max(3, std::min(10, levels));
}

bool FSFusionDMap::toColor32_01_FromLoaded(cv::Mat colorTmp,
                                                cv::Mat& color32,
                                                const QString& where,
                                                int sliceIdx)
{
    color32.release();

    if (colorTmp.empty()) {
        qWarning().noquote() << "WARNING:" << where << "empty colorTmp"
                             << (sliceIdx >= 0 ? QString("slice %1").arg(sliceIdx) : QString());
        return false;
    }

    if (colorTmp.channels() == 1) {
        cv::Mat bgr; cv::cvtColor(colorTmp, bgr, cv::COLOR_GRAY2BGR);
        colorTmp = bgr;
    } else if (colorTmp.channels() == 4) {
        cv::Mat bgr; cv::cvtColor(colorTmp, bgr, cv::COLOR_BGRA2BGR);
        colorTmp = bgr;
    } else if (colorTmp.channels() != 3) {
        qWarning().noquote() << "WARNING:" << where << "unsupported channels"
                             << colorTmp.channels()
                             << (sliceIdx >= 0 ? QString("slice %1").arg(sliceIdx) : QString());
        return false;
    }

    if (colorTmp.depth() == CV_8U)
        colorTmp.convertTo(color32, CV_32FC3, 1.0 / 255.0);
    else if (colorTmp.depth() == CV_16U)
        colorTmp.convertTo(color32, CV_32FC3, 1.0 / 65535.0);
    else {
        qWarning().noquote() << "WARNING:" << where << "unsupported depth"
                             << colorTmp.depth()
                             << (sliceIdx >= 0 ? QString("slice %1").arg(sliceIdx) : QString());
        return false;
    }

    return true;
}

void FSFusionDMap::updateTop2(const cv::Mat& score32This, uint16_t sliceIndex)
{
    /*
Update accumulated best and 2nd best score and slice index
*/
    CV_Assert(score32This.type() == CV_32F);
    CV_Assert(!s0_pad32.empty());
    CV_Assert(score32This.size() == s0_pad32.size());
    CV_Assert(score32This.size() == s1_pad32.size());
    CV_Assert(score32This.size() == idx0_pad16.size());
    CV_Assert(score32This.size() == idx1_pad16.size());
    CV_Assert(score32This.size() == top1Score32_.size());
    CV_Assert(score32This.size() == winIdx16_.size());

    const int H = score32This.rows;
    const int W = score32This.cols;

    for (int y = 0; y < H; ++y)
    {
        const float* sNew = score32This.ptr<float>(y);

        float* s0 = s0_pad32.ptr<float>(y);
        float* s1 = s1_pad32.ptr<float>(y);

        uint16_t* i0 = idx0_pad16.ptr<uint16_t>(y);
        uint16_t* i1 = idx1_pad16.ptr<uint16_t>(y);

        float*    top1 = top1Score32_.ptr<float>(y);
        uint16_t* win  = winIdx16_.ptr<uint16_t>(y);

        for (int x = 0; x < W; ++x)
        {
            const float s = sNew[x];

            if (s > s0[x])
            {
                // shift best -> second
                s1[x] = s0[x]; i1[x] = i0[x];
                s0[x] = s;     i0[x] = sliceIndex;
            }
            else if (s > s1[x])
            {
                s1[x] = s; i1[x] = sliceIndex;
            }

            // Keep “current best” mirrors for diagnostics / low-contrast gating
            top1[x] = s0[x];
            win[x]  = i0[x];
        }
    }
}

bool FSFusionDMap::computeCropGeometry(const QString& srcFun,
                                            cv::Rect& roiPadToAlign,
                                            cv::Size& origSz) const
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

    const cv::Size alignSz(roiPadToAlign.width, roiPadToAlign.height);
    if (!FSFusion::rectInside(validAreaAlign, alignSz)) {
        qWarning().noquote() << "WARNING:" << srcFun << "validAreaAlign out of bounds.";
        return false;
    }

    origSz = cv::Size(validAreaAlign.width, validAreaAlign.height);
    return true;
}

void FSFusionDMap::accumulatedMeasures(const cv::Rect& roiPadToAlign,
                                      const cv::Size& origSz,
                                      cv::Mat& idx0_16,
                                      cv::Mat& idx1_16,
                                      cv::Mat& s0_32,
                                      cv::Mat& s1_32,
                                      cv::Mat& top1Score32) const
{
    CV_Assert(!idx0_pad16.empty());
    CV_Assert(!top1Score32_.empty());
    CV_Assert(!winIdx16_.empty());

    const cv::Rect roiAlign = roiPadToAlign;

    idx0_16     = idx0_pad16(roiAlign)(validAreaAlign).clone();
    idx1_16     = idx1_pad16(roiAlign)(validAreaAlign).clone();
    s0_32       = s0_pad32  (roiAlign)(validAreaAlign).clone();
    s1_32       = s1_pad32  (roiAlign)(validAreaAlign).clone();
    top1Score32 = top1Score32_(roiAlign)(validAreaAlign).clone();

    CV_Assert(idx0_16.size()     == origSz && idx0_16.type()     == CV_16U);
    CV_Assert(idx1_16.size()     == origSz && idx1_16.type()     == CV_16U);
    CV_Assert(s0_32.size()       == origSz && s0_32.type()       == CV_32F);
    CV_Assert(s1_32.size()       == origSz && s1_32.type()       == CV_32F);
    CV_Assert(top1Score32.size() == origSz && top1Score32.type() == CV_32F);
}

void FSFusionDMap::diagnostics(QString path, cv::Mat& depthIndex16)
{
    qDebug().noquote() << QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss");
    qDebug().noquote() << "Fusion                        =" << "DMap";
    qDebug().noquote() << "FOCUS METRIC:";
    qDebug().noquote() << " scoreSigma                   =" << o.scoreSigma;
    qDebug().noquote() << " scoreKSize                   =" << o.scoreKSize;
    qDebug().noquote() << "FOREGROUND:";
    qDebug().noquote() << " depthStableRadiusPx          =" << o.depthStableRadiusPx;
    qDebug().noquote() << " depthMaxRangeSlicesCore      =" << o.depthMaxRangeSlicesCore;
    qDebug().noquote() << " depthMaxRangeSlicesLoose     =" << o.depthMaxRangeSlicesLoose;
    qDebug().noquote() << " expandTexFrac                =" << o.expandTexFrac;
    qDebug().noquote() << " strongFrac                   =" << o.strongFrac;
    qDebug().noquote() << " weakFrac                     =" << o.weakFrac;
    qDebug().noquote() << " seedDilatePx                 =" << o.seedDilatePx;
    qDebug().noquote() << " closePx                      =" << o.closePx;
    qDebug().noquote() << " openPx                       =" << o.openPx;
    qDebug().noquote() << " interiorPx                   =" << o.interiorPx;
    qDebug().noquote() << "OWNERSHIP PROPOGATION:";
    qDebug().noquote() << " ownershipClosePx             =" << o.ownershipClosePx;
    qDebug().noquote() << " seedBandPx                   =" << o.seedBandPx;
    qDebug().noquote() << "CONTRAST THRESHOLD:";
    qDebug().noquote() << " enableContrastThreshold      =" << o.enableContrastThreshold;
    qDebug().noquote() << " contrastMinFrac              =" << o.contrastMinFrac;
    qDebug().noquote() << " lowContrastMedianK           =" << o.lowContrastMedianK;
    qDebug().noquote() << " lowContrastDilatePx          =" << o.lowContrastDilatePx;
    qDebug().noquote() << "PYRAMID FUSION:";
    qDebug().noquote() << " enablePyramidBlend           =" << o.enablePyramidBlend;
    qDebug().noquote() << " pyrLevels                    =" << o.pyrLevels;
    qDebug().noquote() << " enableHardWeightsOnLowpass   =" << o.enableHardWeightsOnLowpass;
    qDebug().noquote() << " enableDepthGradLowpassVeto   =" << o.enableDepthGradLowpassVeto;
    qDebug().noquote() << " hardFromLevel                =" << o.hardFromLevel;
    qDebug().noquote() << " vetoFromLevel                =" << o.vetoFromLevel;
    qDebug().noquote() << " vetoStrength                 =" << o.vetoStrength;
    qDebug().noquote() << " weightBlurSigma              =" << o.weightBlurSigma;
    qDebug().noquote() << " mixEps                       =" << o.mixEps;
    qDebug().noquote() << " wMin                         =" << o.wMin;

    if (path.isEmpty()) return;

    std::string s;
    s = (path + "/dmap_fg.png").toStdString();
    cv::imwrite(s, fg8);

    s = (path + "/dmap_fg_own.png").toStdString();
    cv::imwrite(s, fgOwn8);

    s = (path + "/dmap_ownership_ring.png").toStdString();
    cv::imwrite(s, overrideMask8);

    s = (path + "/dmap_ownership_winner.png").toStdString();
    cv::imwrite(s, FSUtilities::depthHeatmap(overrideWinner16, N, "Ownership winner"));

    cv::Mat topRatio32 = s0_32 / (s1_32 + 1e-6f);
    s = (path + "/dmap_topRatio32.png").toStdString();
    cv::imwrite(s, topRatio32);

    s = (path + "/dmap_depthIndex16.png").toStdString();
    cv::imwrite(s, FSUtilities::depthHeatmap(depthIndex16, N, "DMap depthIndex16"));

    // this looks like a good fg ??
    double mn=0,mx=0; cv::minMaxLoc(top1_32, &mn, &mx);
    cv::Mat top18;
    top1_32.convertTo(top18, CV_8U, (mx > 1e-12) ? (255.0 / mx) : 1.0);
    s = (path + "/dmap_top1Score8.png").toStdString();
    cv::imwrite(s, top18);

    if (o.enableContrastThreshold) {
        s = (path + "/dmap_lowContrastMask.png").toStdString();
        cv::imwrite(s, lowC8);
    }
}

bool FSFusionDMap::streamSlice(int slice,
                                    const cv::Mat& grayAlign8,
                                    const cv::Mat& colorAlign,
                                    const FSFusion::Options& opt,
                                    std::atomic_bool* abortFlag,
                                    FSFusion::StatusCallback /*statusCb*/,
                                    FSFusion::ProgressCallback /*progressCb*/)
{
/*
This function streams one aligned grayscale slice into a running “depth-map by
best-focus” accumulator. In other words: for every pixel, it’s tracking which slice
index is most in-focus (and its score), and also the 2nd-best.
    idx0_pad16:     best slice index
    idx1_pad16:     2nd best slice index
    s0_pad32:       best focus score
    s1_pad32:       2nd best focus score
    winIdx16_:      looks like a “current winner index” output buffer (often same as
                    idx0, depending on later code)
    top1Score32_:   top score buffer (often same as s0, depending on later code).
*/
    const QString srcFun = "FSFusionDMap::streamSlice";
    if (FSFusion::isAbort(abortFlag)) return false;

    if (grayAlign8.empty() || grayAlign8.type() != CV_8U) {
        qWarning().noquote() << "WARNING:" << srcFun << "grayAlign8 empty or not CV_8U.";
        return false;
    }
    if (slice == 0)
    {
        /* Caller must have set these before calling streamSlice (same as your
           DMapAdvanced pattern).  alignSize / validAreaAlign should already be
           valid. */
        if (alignSize.width <= 0 || alignSize.height <= 0) {
            qWarning().noquote() << "WARNING:" << srcFun << "alignSize not set.";
            return false;
        }

        const int levelsForPad = computePyrLevels(alignSize);
        o.pyrLevels = levelsForPad;

        padSize = computePadSizeForPyr(alignSize, levelsForPad);

        if (!colorAlign.empty())
            outDepth = (colorAlign.depth() == CV_16U) ? CV_16U : CV_8U;

        idx0_pad16   = cv::Mat(padSize, CV_16U, cv::Scalar(0)); // best slice index
        idx1_pad16   = cv::Mat(padSize, CV_16U, cv::Scalar(0)); // 2nd best slice index
        s0_pad32     = cv::Mat(padSize, CV_32F, cv::Scalar(-1.0f));
        s1_pad32     = cv::Mat(padSize, CV_32F, cv::Scalar(-1.0f));
        winIdx16_    = cv::Mat(padSize, CV_16U, cv::Scalar(0));
        top1Score32_ = cv::Mat(padSize, CV_32F, cv::Scalar(-1.0f));  // CRASH

        active_ = true;
        sliceCount_ = 0;
    }
    else
    {
        if (!active_ || s0_pad32.empty()) {
            qWarning().noquote() << "WARNING:" << srcFun << "called without active state.";
            return false;
        }
    }

    if (grayAlign8.size() != alignSize) {
        qWarning().noquote() << "WARNING:" << srcFun << "alignSize mismatch.";
        return false;
    }

    // pad the aligned image to padSize, using reflection at the borders, centered
    cv::Mat grayPad8 = padCenterReflect(grayAlign8, padSize);

    // focus metric
    cv::Mat score32;
    score32 = focusMetric32Laplacian(grayPad8, o.scoreSigma, o.scoreKSize);

    // Soft highlight weighting (keep some score even in highlights)
    cv::Mat g = grayPad8;                // CV_8U
    cv::Mat g32; g.convertTo(g32, CV_32F, 1.0f/255.0f);

    // start fading at ~0.92, fully faded by ~0.99 (tweak)
    const float t0 = 0.92f;
    const float t1 = 0.99f;

    // w = 1 in midtones, down to wMin near clip
    const float wMin = 0.25f; // never fully kill
    cv::Mat w = (g32 - t0) / (t1 - t0);
    cv::max(w, 0.0f, w);
    cv::min(w, 1.0f, w);
    // smoothstep
    w = w.mul(w).mul(3.0f - 2.0f*w);
    w = 1.0f - w;                 // 1 -> 0 near highlights
    w = wMin + (1.0f - wMin)*w;    // clamp floor

    score32 = score32.mul(w);
    // end highlight weighting (keep some score even in highlights)

    // update accumulated best and 2nd best score and slice index
    updateTop2(score32, (uint16_t)std::max(0, slice));

    // Diagnostics for slice
    if (!opt.depthFolderPath.isEmpty()) {
        double mn=0,mx=0; cv::minMaxLoc(score32, &mn, &mx);
        cv::Mat s8u;
        score32.convertTo(s8u, CV_8U, (mx > 1e-12) ? (255.0/mx) : 1.0);
        std::string f = opt.depthFolderPath.toStdString();
        std::string s = QString::number(slice).toStdString();
        std::string name = f + "/score_" + s + ".png";
        cv::imwrite(name, s8u);
    }


    sliceCount_++;
    return true;
}

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
    QString msg;

    N = inputPaths.size();

    if (!active_ || s0_pad32.empty() || N <= 0 || (int)globals.size() != N)
        return false;
    if (FSFusion::isAbort(abortFlag)) return false;

    // PAD→ALIGN→ORIG crop
    cv::Rect roiPadToAlign;
    cv::Size origSz;
    if (!computeCropGeometry(srcFun, roiPadToAlign, origSz))
        return false;

    // ------------------------------------------------------------
    // Measures and depth index
    // ------------------------------------------------------------

    // cv::Mat idx0_16, idx1_16, s0_32, s1_32, top1_32;
    // assign accumulated values and crop to original size
    accumulatedMeasures(roiPadToAlign, origSz, idx0_16, idx1_16, s0_32, s1_32, top1_32);

    // Depth output for inspection (best slice index)
    depthIndex16 = idx0_16.clone();


    // ------------------------------------------------------------
    // Foreground mask
    // fg8
    // fgOwn8
    // ------------------------------------------------------------

    // Debugging
    msg = "Building foreground";
    if (G::FSLog) G::log(srcFun, msg);

    // --- after you compute fg8 (your diagnostic FG) ---
    fg8 = buildFgFromTop1AndDepth(
        top1_32, depthIndex16,
        o.depthStableRadiusPx,
        o.depthMaxRangeSlicesCore,
        o.depthMaxRangeSlicesLoose,
        o.strongFrac,
        o.weakFrac,
        o.seedDilatePx,
        o.closePx,
        o.openPx,
        o.interiorPx,
        o.expandTexFrac);

    // ------------------------------------------------------------
    // Ownership FG: use a *filled/closed* version ONLY for ownership,
    // so the ring can’t leak into tiny pinholes / gaps.
    // Keep fg8 unchanged for diagnostics.
    // ------------------------------------------------------------
    msg = "Defining ownership (foreground, ring, background";
    if (G::FSLog) G::log(srcFun, msg);

    fgOwn8 = fg8.clone();

    // 1) close small gaps (twigs, pinholes along boundary)
    if (o.ownershipClosePx > 0)
        fgOwn8 = morphClose8(fgOwn8, o.ownershipClosePx);

    // 2) fill holes (critical to prevent ring sneaking into interior)
    fgOwn8 = fillHoles8(fgOwn8);

    // Optional: if you still see “ring leaks” through hairline slits,
    // do a *very small* extra close+fill (keep it tiny!)
    // Note: this makes no difference
    //
    fgOwn8 = morphClose8(fgOwn8, 1);
    fgOwn8 = fillHoles8(fgOwn8);

    // ------------------------------------------------------------
    // Ownership propagation (halo elimination): override depth in ring
    // ------------------------------------------------------------
    const int ringPx = defaultRingPx(origSz);

    if (!ownershipPropagateTwoPass_Outward(
            fgOwn8,
            depthIndex16,
            ringPx,
            o.seedBandPx,
            overrideMask8,
            overrideWinner16))
    {
        return false;
    }

    if (!overrideMask8.empty() && cv::countNonZero(overrideMask8) > 0)
    {
        // Safety: never override inside your *diagnostic* FG (conservative)
        // (and also never override inside fgOwn8 by construction, but keep this anyway)
        overrideMask8.setTo(0, fg8);

        // Clamp donor values (paranoia)
        if (!overrideWinner16.empty())
            cv::min(overrideWinner16, (uint16_t)(N - 1), overrideWinner16);

        // Apply deterministic “ownership” in the ring
        overrideWinner16.copyTo(depthIndex16, overrideMask8);
        overrideWinner16.copyTo(idx0_16,      overrideMask8);
        overrideWinner16.copyTo(idx1_16,      overrideMask8);

        // Force hard selection in ring (no blending)
        s0_32.setTo(1.0f, overrideMask8);
        s1_32.setTo(0.0f, overrideMask8);
    }

    // ------------------------------------------------------------
    // Contrast Threshold
    // ------------------------------------------------------------
    if (o.enableContrastThreshold)
    {
        msg = "Contrast threshold";
        if (G::FSLog) G::log(srcFun, msg);

        lowC8 = lowContrastMask8_fromTop1Score(top1_32, o.contrastMinFrac);

        if (!fgOwn8.empty()) lowC8.setTo(0, fgOwn8);  // don’t touch FG

        if (o.lowContrastDilatePx > 0)
            cv::dilate(lowC8, lowC8, FSFusion::seEllipse(o.lowContrastDilatePx));

        if (cv::countNonZero(lowC8) > 0)
        {
            // 1) Stabilize depth labels in low-contrast regions (kills confetti/noise winners)
            int k = o.lowContrastMedianK;
            if (k < 3) k = 3;
            if ((k & 1) == 0) k += 1;          // make odd
            k = std::min(k, 51);               // sanity cap
            cv::Mat med16;
            cv::medianBlur(depthIndex16, med16, k);
            med16.copyTo(depthIndex16, lowC8);

            // Keep idx0 consistent with the stabilized depth map
            depthIndex16.copyTo(idx0_16, lowC8);

            // 2) FORCE HARD SELECTION (NO BLEND) in low-contrast pixels:
            // make idx1 == idx0 and make (s0,s1) = (1,0) so w32 becomes 1.0 for idx0 slice
            idx0_16.copyTo(idx1_16, lowC8);

            s0_32.setTo(1.0f, lowC8);
            s1_32.setTo(0.0f, lowC8);
        }
    }

    // ------------------------------------------------------------
    // Final fusion using Pyramids
    // ------------------------------------------------------------
    cv::Mat out32(origSz, CV_32FC3, cv::Scalar(0,0,0));

    if (o.enablePyramidBlend) {
        // Build pyramids from depthIndex16 (same as advanced path)
        const int levels = computePyrLevels(origSz);

        std::vector<cv::Mat> idxPyr16;
        FusionPyr::buildIndexPyrNearest(depthIndex16, levels, idxPyr16);

        // accumulator
        FusionPyr::PyrAccum A;
        A.reset(origSz, levels);

        // Check for varied illumination
        // cv::Mat sumW(origSz, CV_32F, cv::Scalar(0));

        // Per-slice accumulation
        for (int s = 0; s < N; ++s)
        {
            QString ss = QString::number(s+1);
            msg = "Pyramid blending: slice " + ss;
            if (G::FSLog) G::log(srcFun, msg);

            progressCb();
            statusCb("Fusing Slice: " + ss + " of " + QString::number(N) + " ");

            if (FSFusion::isAbort(abortFlag)) return false;

            cv::Mat colorSliceAligned = cv::imread(
                alignedColorPaths[s].toStdString(), cv::IMREAD_UNCHANGED);
            if (colorSliceAligned.empty()) {
                qWarning().noquote() << "WARNING:" << srcFun <<
                    "colorSliceAligned " + ss + " is empty";
                return false;
            }

            cv::Mat color32;
            if (!toColor32_01_FromLoaded(colorSliceAligned, color32, srcFun, s)) {
                qWarning().noquote() << "WARNING:" << srcFun <<
                    "toColor32_01_FromLoaded " + ss + " failed";
                return false;
            }

            // w32 from continuous top2 mix
            cv::Mat w32(origSz, CV_32F, cv::Scalar(0));

            const float eps = std::max(1e-12f, o.mixEps);
            for (int y = 0; y < origSz.height; ++y)
            {
                const uint16_t* i0 = idx0_16.ptr<uint16_t>(y);
                const uint16_t* i1 = idx1_16.ptr<uint16_t>(y);
                const float*    a0 = s0_32.ptr<float>(y);
                const float*    a1 = s1_32.ptr<float>(y);

                float* w = w32.ptr<float>(y);

                for (int x = 0; x < origSz.width; ++x)
                {
                    const int d0 = (int)i0[x];
                    const int d1 = (int)i1[x];

                    if (d0 != s && d1 != s) continue;

                    const float s0 = std::max(0.0f, a0[x]);
                    const float s1 = std::max(0.0f, a1[x]);
                    const float den = s0 + s1 + eps;

                    float w0 = s0 / den;
                    float w1 = 1.0f - w0;

                    if (o.wMin > 0.0f) {
                        w0 = std::max(w0, o.wMin);
                        w1 = std::max(w1, o.wMin);
                        const float inv = 1.0f / (w0 + w1);
                        w0 *= inv; w1 *= inv;
                    }

                    if (d0 == s) w[x] = 1.0f;  // ignore top2 blend entirely
                    else         w[x] = 0.0f;
                }
            }

            // sumW += w32;
            // double mn, mx;
            // cv::minMaxLoc(sumW, &mn, &mx);
            // qDebug() << "slice" << s << "sumW min/max =" << mn << mx;

            FusionPyr::AccumDMapParams ap;
            ap.enableHardWeightsOnLowpass = o.enableHardWeightsOnLowpass;
            ap.enableDepthGradLowpassVeto = o.enableDepthGradLowpassVeto;
            ap.hardFromLevel = o.hardFromLevel;
            ap.vetoFromLevel = o.vetoFromLevel;
            ap.vetoStrength  = o.vetoStrength;
            ap.wMin          = 0.0f; // wMin already handled above

            FusionPyr::accumulateSlicePyr(A,
                                          color32,
                                          w32,
                                          idxPyr16,
                                          nullptr,
                                          s,
                                          ap,
                                          levels,
                                          o.weightBlurSigma);
        }
        out32 = FusionPyr::finalizeBlend(A, 1e-8f);
    }
    // ------------------------------------------------------------
    // Final fusion directly from depthIndex16 (Simple)
    // ------------------------------------------------------------
    else {
        msg = "Simple blending";
        if (G::FSLog) G::log(srcFun, msg);
        for (int s = 0; s < N; ++s)
        {
            cv::Mat colorSliceAligned = cv::imread(
                alignedColorPaths[s].toStdString(), cv::IMREAD_UNCHANGED);
            if (colorSliceAligned.empty()) return false;

            cv::Mat color32;
            if (!toColor32_01_FromLoaded(colorSliceAligned, color32, srcFun, s))
                return false;

            cv::Mat mask8 = (depthIndex16 == (uint16_t)s);  // CV_8U 0/255
            color32.copyTo(out32, mask8);
        }
    }

    // cv::Mat out32 = FusionPyr::finalizeBlend(A, 1e-8f);
    cv::max(out32, 0.0f, out32);
    cv::min(out32, 1.0f, out32);

    if (outDepth == CV_16U) out32.convertTo(outputColor, CV_16UC3, 65535.0);
    else out32.convertTo(outputColor, CV_8UC3, 255.0);

    // // ------------------------------------------------------------
    // // Diagnostics
    // // ------------------------------------------------------------
    if (o.enableDiagnostics) diagnostics(opt.depthFolderPath, depthIndex16);
    std::string s;
    s = (opt.depthFolderPath + "/dmap_depthIndex16.png").toStdString();
    cv::imwrite(s, FSUtilities::depthHeatmap(depthIndex16, N, "DMap depthIndex16"));

    reset();
    return true;
}
