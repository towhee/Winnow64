#include "fsfusiondmapshared.h"
#include "fsfusion.h"   // for FSFusion::seEllipse (or forward-declare it if you prefer)
#include <opencv2/imgproc.hpp>

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
        cv::Mat out = FSFusionDMapShared::keepCCsTouchingMask(weak, strong, 8);
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

    cv::Mat buildFgFromTop1AndDepth(const cv::Mat& top1Score32,
                                    const cv::Mat& depthIndex16,
                                    int depthStableRadiusPx,
                                    int depthMaxRangeSlices,
                                    float strongFrac,
                                    float weakFrac)
    {
    /*
    Return a foreground mask based on a silhouette from top1Score32 combined with
    contiguous slices in depthIndex16.
    */
        CV_Assert(top1Score32.type() == CV_32F);
        CV_Assert(depthIndex16.type() == CV_16U);
        CV_Assert(top1Score32.size() == depthIndex16.size());

        // 1) seed from top1Score hysteresis
        cv::Mat seed8 = hysteresisMask8_fromTop1Score(top1Score32, strongFrac, weakFrac);

        // Thicken seeds slightly so “touching” works better
        cv::dilate(seed8, seed8, FSFusion::seEllipse(2));

        // 2) stable depth interior
        cv::Mat stable8 = stableDepthMask8_rangeLe(depthIndex16, depthStableRadiusPx, depthMaxRangeSlices);

        // 3) keep stable regions connected to seed
        cv::Mat fg8 = FSFusionDMapShared::keepCCsTouchingMask(stable8, seed8, 8);

        // 4) cleanup
        cv::morphologyEx(fg8, fg8, cv::MORPH_CLOSE, FSFusion::seEllipse(3));
        cv::Mat filled = FSFusionDMapShared::fillHoles8(fg8);

        // If you want ONE subject blob:
        // filled = FSFusionDMapShared::keepLargestCC(filled);

        return filled; // 0/255
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
