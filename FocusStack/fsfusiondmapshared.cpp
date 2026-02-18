#include "fsfusiondmapshared.h"
#include "fsfusion.h"   // for FSFusion::seEllipse (or forward-declare it if you prefer)
#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
#include <limits>

namespace FSFusionDMapShared
{
    cv::Size computePadSizeForPyr(const cv::Size& s, int levels)
    {
        const int lv = std::max(1, levels);
        const int m  = 1 << std::max(0, lv - 1);
        const int w  = ((s.width  + m - 1) / m) * m;
        const int h  = ((s.height + m - 1) / m) * m;
        return cv::Size(w, h);
    }

    cv::Mat padCenterReflect(const cv::Mat& src, const cv::Size& dstSize)
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

    cv::Mat fillHoles8(const cv::Mat& bin8)
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


    // If you don't have conf01, this gives a decent proxy from top2 scores:
    // conf ~= margin between best and runner-up.
    // Output: CV_32F 0..1 (roughly)
    cv::Mat confidence01_fromTop2Scores(const cv::Mat& s0_32,
                                        const cv::Mat& s1_32,
                                        float eps )
    {
        CV_Assert(s0_32.type() == CV_32F && s1_32.type() == CV_32F);
        CV_Assert(s0_32.size() == s1_32.size());

        cv::Mat den = s0_32 + s1_32 + eps;
        cv::Mat conf = (s0_32 - s1_32) / den;          // ~0..1 for s0>=s1
        cv::max(conf, 0.0f, conf);
        cv::min(conf, 1.0f, conf);
        return conf;
    }

    // Depth stability idea (robust + simple):
    // stable if depth is close to a median filtered depth AND close to runner-up.
    // Output: CV_8U 0/255
    cv::Mat depthStabilityMask8(const cv::Mat& idx0_16,
                                const cv::Mat& idx1_16,
                                int medianK,
                                int tolSlices)
    {
        CV_Assert(idx0_16.type() == CV_16U);
        CV_Assert(idx1_16.type() == CV_16U);
        CV_Assert(idx0_16.size() == idx1_16.size());

        medianK = (medianK == 3) ? 3 : 5;

        cv::Mat med16;
        cv::medianBlur(idx0_16, med16, medianK);

        // |idx0 - med| <= tol
        cv::Mat diffMed16;
        cv::absdiff(idx0_16, med16, diffMed16);
        cv::Mat mMed = (diffMed16 <= (uint16_t)tolSlices); // CV_8U 0/255

        // |idx0 - idx1| <= tol  (winner not wildly ambiguous)
        cv::Mat diffTop16;
        cv::absdiff(idx0_16, idx1_16, diffTop16);
        cv::Mat mTop = (diffTop16 <= (uint16_t)tolSlices); // CV_8U 0/255

        cv::Mat stable;
        cv::bitwise_and(mMed, mTop, stable);
        stable.convertTo(stable, CV_8U, 255.0);
        return stable;
    }

    // Halo-risk ring: pixels outside FG but within ringPx of FG boundary.
    // fg8 must be 0/255.
    cv::Mat haloRiskRing8(const cv::Mat& fg8, int ringPx)
    {
        CV_Assert(fg8.type() == CV_8U);
        ringPx = std::max(1, ringPx);

        cv::Mat fgBin;
        cv::compare(fg8, 0, fgBin, cv::CMP_GT); // 0/255

        cv::Mat dil;
        cv::dilate(fgBin, dil, cv::getStructuringElement(cv::MORPH_ELLIPSE,
                                                         cv::Size(2*ringPx+1, 2*ringPx+1)));

        // ring = dilated - fg
        cv::Mat ring;
        cv::subtract(dil, fgBin, ring); // 0/255
        return ring;
    }

    cv::Mat dmapDiagnosticBGR(const cv::Mat& idx0_16,
                              const cv::Mat& idx1_16,
                              const cv::Mat& fg8,        // 0/255 foreground silhouette (your best FG)
                              const cv::Mat& conf01_opt, // CV_32F 0..1, may be empty
                              const cv::Mat& s0_32_opt,   // CV_32F, may be empty if conf01 provided
                              const cv::Mat& s1_32_opt,   // CV_32F
                              int ringPx,
                              float ringConfMax,  // only show ring risk where confidence is low
                              int stabilityMedianK,
                              int stabilityTolSlices)
    {
    // /*
    // Blue = FG confidence (0..255)
    // Green = Depth stability (0..255)
    // Red = Halo-risk ring (0/255, optionally gated)
    // */
        CV_Assert(idx0_16.type() == CV_16U);
        CV_Assert(idx1_16.type() == CV_16U);
        CV_Assert(fg8.type() == CV_8U);
        CV_Assert(idx0_16.size() == idx1_16.size());
        CV_Assert(idx0_16.size() == fg8.size());

        // --- confidence map ---
        cv::Mat conf01;
        if (!conf01_opt.empty()) {
            CV_Assert(conf01_opt.type() == CV_32F);
            CV_Assert(conf01_opt.size() == idx0_16.size());
            conf01 = conf01_opt;
        } else {
            CV_Assert(!s0_32_opt.empty() && !s1_32_opt.empty());
            CV_Assert(s0_32_opt.type() == CV_32F && s1_32_opt.type() == CV_32F);
            CV_Assert(s0_32_opt.size() == idx0_16.size());
            conf01 = confidence01_fromTop2Scores(s0_32_opt, s1_32_opt);
        }

        // --- stability mask (0/255) ---
        cv::Mat stable8 = depthStabilityMask8(idx0_16, idx1_16, stabilityMedianK, stabilityTolSlices);

        // --- ring mask (0/255) ---
        cv::Mat ring8 = haloRiskRing8(fg8, ringPx);

        // gate ring to low confidence (optional but super informative)
        if (ringConfMax > 0.0f) {
            cv::Mat lowConf8 = (conf01 < ringConfMax); // 0/255
            lowConf8.convertTo(lowConf8, CV_8U, 255.0);
            cv::bitwise_and(ring8, lowConf8, ring8);
        }

        // --- pack into BGR ---
        cv::Mat B, G, R;

        conf01.convertTo(B, CV_8U, 255.0); // Blue = confidence
        G = stable8;                       // Green = stability
        R = ring8;                         // Red = halo-risk ring

        std::vector<cv::Mat> ch{B, G, R};
        cv::Mat out;
        cv::merge(ch, out);                // CV_8UC3
        return out;
    }

    cv::Mat morphClose8(const cv::Mat& bin8, int px)
    {
        CV_Assert(bin8.type() == CV_8U);
        if (bin8.empty() || px <= 0) return bin8.clone();
        cv::Mat out = bin8.clone();
        cv::morphologyEx(out, out, cv::MORPH_CLOSE, FSFusion::seEllipse(px));
        return out;
    }

    int FSFusionDMapShared::defaultRingPx(const cv::Size& sz)
    {
        // ~0.6% of max dimension, clamped
        int m = std::max(sz.width, sz.height);
        int r = (int)std::round(m * 0.01);
        return std::max(12, std::min(80, r));
    }

    cv::Mat ringOutsideFg8(const cv::Mat& fg8, int ringPx)
    {
        CV_Assert(fg8.type() == CV_8U);
        if (fg8.empty() || ringPx <= 0) return cv::Mat::zeros(fg8.size(), CV_8U);

        cv::Mat dil;
        cv::dilate(fg8, dil, FSFusion::seEllipse(ringPx));

        cv::Mat ring = dil.clone();
        ring.setTo(0, fg8); // ring = dilate - fg
        return ring;
    }

    bool ownershipPropagateTwoPass_Outward(
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

    cv::Mat hysteresisMask8_fromTop1Score(const cv::Mat& top1Score32,
                                          float strongFrac,
                                          float weakFrac,
                                          float minAbs)
    {
        CV_Assert(top1Score32.type() == CV_32F);

        double mn=0, mx=0;
        cv::minMaxLoc(top1Score32, &mn, &mx);

        const float strongThr = std::max((float)mx * std::max(0.0f, strongFrac), minAbs);
        const float weakThr   = std::max((float)mx * std::max(0.0f, weakFrac),   minAbs);

        cv::Mat strong = (top1Score32 >= strongThr); strong.convertTo(strong, CV_8U, 255);
        cv::Mat weak   = (top1Score32 >= weakThr);   weak.convertTo(weak,   CV_8U, 255);

        // Keep only weak components that touch strong components (hysteresis)
        // (You’ll add keepCCsTouchingMask to shared; code below.)
        cv::Mat out = keepCCsTouchingMask(weak, strong, 8);
        return out; // 0/255
    }

    cv::Mat stableDepthMask8_rangeLe(const cv::Mat& depth16, int r, int maxRange)
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

    cv::Mat lowContrastMask8_fromTop1Score(const cv::Mat& top1Score32,
                                                  float contrastMinFrac,
                                                  float minAbs)
    {
        CV_Assert(top1Score32.type() == CV_32F);

        double mn=0.0, mx=0.0;
        cv::minMaxLoc(top1Score32, &mn, &mx);

        const float thr = std::max((float)mx * std::max(0.0f, contrastMinFrac), minAbs);

        cv::Mat low = (top1Score32 < thr);  // CV_8U 0/255
        low.convertTo(low, CV_8U, 255.0);
        return low;
    }

    cv::Mat foregroundMask8(const cv::Mat& winStable16,
                            const cv::Mat& /*conf01*/,
                            int N,
                            bool nearIsMinIndex,
                            cv::Mat* outFgFilled8)
    {
    /*
    NOTE: this uses ONLY stable winner labels to define FG silhouette.
    If that FG is imperfect, halo cleanup can still work because:
    (a) we only operate in a *ring outside fg*, and
    (b) we further gate by low-conf + low-tex, and
    (c) donor propagation uses boundary seeds, so it tends to follow real edges.
    */
        CV_Assert(winStable16.type() == CV_16U);
        CV_Assert(N > 0);

        // Pick FG slice id from global min/max of stable depth labels
        double mn = 0.0, mx = 0.0;
        cv::minMaxLoc(winStable16, &mn, &mx);

        int sFg = nearIsMinIndex ? (int)mn : (int)mx;
        sFg = std::max(0, std::min(sFg, N - 1));

        cv::Mat fg8 = (winStable16 == (uint16_t)sFg);
        fg8.convertTo(fg8, CV_8U, 255.0);

        // Light close to connect micro-gaps (helps twigs)
        cv::morphologyEx(fg8, fg8, cv::MORPH_CLOSE, FSFusion::seEllipse(2));

        // Keep largest CC to prevent “salt & pepper FG”
        // (If you want to keep multiple twig islands, swap to keepCCsTouchingMask later.)
        {
            cv::Mat labels, stats, centroids;
            cv::Mat src;
            cv::compare(fg8, 0, src, cv::CMP_GT);

            int n = cv::connectedComponentsWithStats(src, labels, stats, centroids, 8, CV_32S);
            if (n > 1)
            {
                int best = 1, bestArea = stats.at<int>(1, cv::CC_STAT_AREA);
                for (int i = 2; i < n; ++i)
                {
                    int area = stats.at<int>(i, cv::CC_STAT_AREA);
                    if (area > bestArea) { bestArea = area; best = i; }
                }
                cv::compare(labels, best, fg8, cv::CMP_EQ); // 0/255
            }
            else
            {
                fg8 = cv::Mat::zeros(fg8.size(), CV_8U);
            }
        }

        cv::Mat fgFilled8 = fillHoles8(fg8);
        if (outFgFilled8) *outFgFilled8 = fgFilled8;

        return fg8;
    }

    cv::Mat keepLargestCC(const cv::Mat& bin8, int connectivity)
    {
    /*
    Takes a binary-ish 8-bit image and returns a mask containing only the single
    largest connected component (everything else becomes 0).
    */
        CV_Assert(bin8.empty() || bin8.type() == CV_8U);
        if (bin8.empty()) return cv::Mat();

        cv::Mat src;
        cv::compare(bin8, 0, src, cv::CMP_GT); // 0/255
        if (cv::countNonZero(src) == 0)
            return cv::Mat::zeros(src.size(), CV_8U);

        cv::Mat labels, stats, centroids;
        int n = cv::connectedComponentsWithStats(
            src, labels, stats, centroids,
            connectivity, CV_32S
            );
        if (n <= 1)
            return cv::Mat::zeros(src.size(), CV_8U);

        int best = 1;
        int bestArea = stats.at<int>(1, cv::CC_STAT_AREA);

        for (int i = 2; i < n; ++i)
        {
            int area = stats.at<int>(i, cv::CC_STAT_AREA);
            if (area > bestArea) { bestArea = area; best = i; }
        }

        cv::Mat out;
        cv::compare(labels, best, out, cv::CMP_EQ); // 0/255
        return out;
    }

    // stable8: 0/255, seed8: 0/255
    // returns only stable components that touch seed
    cv::Mat keepCCsTouchingMask(const cv::Mat& bin8,
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

    // robust max: 99th percentile
    float robustMax99(const cv::Mat& m32f)
    {
        CV_Assert(m32f.type() == CV_32F);
        cv::Mat v = m32f.reshape(1, 1).clone();
        cv::sort(v, v, cv::SORT_ASCENDING);
        int idx = (int)std::floor(0.99 * (v.cols - 1));
        return v.at<float>(0, std::max(0, std::min(idx, v.cols-1)));
    }

    cv::Mat buildFgFromTop1AndDepth(const cv::Mat& top1Score32,
                                    const cv::Mat& depthIndex16,
                                    int depthStableRadiusPx,
                                    int depthMaxRangeSlicesCore,
                                    int depthMaxRangeSlicesLoose,
                                    float strongFrac,
                                    float weakFrac,
                                    int seedDilatePx,
                                    int closePx,
                                    int openPx,
                                    int interiorPx)
    {
    /*
    Return a foreground mask based on a silhouette from top1Score32 combined with
    contiguous slices in depthIndex16.
    */
        CV_Assert(top1Score32.type() == CV_32F);
        CV_Assert(depthIndex16.type() == CV_16U);
        CV_Assert(top1Score32.size() == depthIndex16.size());

        // Ensure strong >= weak (strong threshold must be stricter)
        float sFrac = strongFrac, wFrac = weakFrac;
        if (sFrac < wFrac) std::swap(sFrac, wFrac);

        const float mx = robustMax99(top1Score32);
        const float strongThr = std::max(1e-12f, mx * sFrac);
        const float weakThr   = std::max(1e-12f, mx * wFrac);

        // 1) Strong seeds
        cv::Mat seed8 = (top1Score32 >= strongThr);
        seed8.convertTo(seed8, CV_8U, 255.0);
        if (seedDilatePx > 0)
            cv::dilate(seed8, seed8, FSFusion::seEllipse(seedDilatePx));

        // 2) Weak silhouette (don’t keepLargestCC here)
        cv::Mat weak8 = (top1Score32 >= weakThr);
        weak8.convertTo(weak8, CV_8U, 255.0);

        if (closePx > 0)
            cv::morphologyEx(weak8, weak8, cv::MORPH_CLOSE, FSFusion::seEllipse(closePx));
        weak8 = FSFusionDMapShared::fillHoles8(weak8);

        // Keep weak silhouette parts that are connected to strong seeds
        weak8 = keepCCsTouchingMask(weak8, seed8, 8);

        // 3) Core stable depth (halo-safe)
        cv::Mat stableCore8  = stableDepthMask8_rangeLe(depthIndex16,
                                                       depthStableRadiusPx,
                                                       depthMaxRangeSlicesCore);

        cv::Mat insideCore8;
        cv::bitwise_and(stableCore8, weak8, insideCore8);

        cv::Mat fgCore8 = keepCCsTouchingMask(insideCore8, seed8, 8);

        // 4) Loose stable depth candidate (to fix “blue arrow” transitions)
        cv::Mat stableLoose8 = stableDepthMask8_rangeLe(depthIndex16,
                                                        depthStableRadiusPx,
                                                        depthMaxRangeSlicesLoose);

        cv::Mat insideLoose8;
        cv::bitwise_and(stableLoose8, weak8, insideLoose8);

        cv::Mat fgLoose8 = keepCCsTouchingMask(insideLoose8, seed8, 8);

        // 5) Interior-only expansion: only add loose pixels well inside core
        cv::Mat fgCoreE8;
        if (interiorPx > 0)
            cv::erode(fgCore8, fgCoreE8, FSFusion::seEllipse(interiorPx));
        else
            fgCoreE8 = fgCore8;

        cv::Mat fgAdd8 = keepCCsTouchingMask(fgLoose8, fgCoreE8, 8);

        cv::Mat fg8 = fgCore8 | fgAdd8;

        // 6) cleanup
        if (openPx > 0)
            cv::morphologyEx(fg8, fg8, cv::MORPH_OPEN,  FSFusion::seEllipse(openPx));
        if (closePx > 0)
            cv::morphologyEx(fg8, fg8, cv::MORPH_CLOSE, FSFusion::seEllipse(closePx));

        fg8 = FSFusionDMapShared::fillHoles8(fg8);
        cv::threshold(fg8, fg8, 127, 255, cv::THRESH_BINARY);
        return fg8;
    }

    cv::Mat focusMetric32_dmap(const cv::Mat& gray8, float preBlurSigma, int lapKSize)
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


} // end namespace FSFusionDMapShared
