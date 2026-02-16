#ifndef FSFUSIONDMAPSHARED_H
#define FSFUSIONDMAPSHARED_H

#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>

#include <QString>
#include <atomic>
#include <vector>
#include <functional>

namespace FSFusionDMapShared
{
    // --- Declarations ---

    cv::Size computePadSizeForPyr(const cv::Size& s, int levels);

    // ALIGN -> PAD, centered, reflect border
    cv::Mat padCenterReflect(const cv::Mat& src, const cv::Size& dstSize);

    cv::Mat fillHoles8(const cv::Mat& bin8);  // CV_8U 0/255 -> CV_8U 0/255

    cv::Mat hysteresisMask8_fromTop1Score(const cv::Mat& top1Score32,
                                          float strongFrac,
                                          float weakFrac,
                                          float minAbs = 1e-12f);

    // depth16: CV_16U (orig size)
    // r: radius in px for the neighborhood (e.g. 2..6)
    // maxRange: 1 means winners vary by at most 1 slice locally
    cv::Mat stableDepthMask8_rangeLe(const cv::Mat& depth16, int r, int maxRange);

    // Build a binary “low-contrast” mask for contrast threshold control
    cv::Mat lowContrastMask8_fromTop1Score(const cv::Mat& top1Score32,
                                           float contrastMinFrac,
                                           float minAbs = 1e-12f);

    // Build a "best available" FG mask in CV_8U 0/255.
    // This is the single source of truth for HaloFix and ADO.
    // - winStable16: stable labels (CV_16U)
    // - conf01: confidence (CV_32F 0..1)
    // - N: slice count
    // Returns fg8 (0/255). Optionally returns fgFilled8.
    cv::Mat foregroundMask8(const cv::Mat& winStable16,
                            const cv::Mat& conf01,
                            int N,
                            bool nearIsMinIndex,
                            cv::Mat* outFgFilled8 = nullptr);

    // stable8: 0/255, seed8: 0/255
    // returns only stable components that touch seed
    cv::Mat focusMetric32_dmap(const cv::Mat& gray8, float preBlurSigma, int lapKSize);

    // --- Small inline helpers ---

    // Keep only the largest connected component from a binary 8-bit mask (0/255).
    // Returns a CV_8U 0/255 mask. If empty or no foreground, returns zeros.
    cv::Mat keepLargestCC(const cv::Mat& bin8, int connectivity = 8);

    cv::Mat keepCCsTouchingMask(const cv::Mat& bin8,
                               const cv::Mat& touch8,
                               int connectivity);

    cv::Mat buildFgFromTop1AndDepth(const cv::Mat& top1Score32,
                                    const cv::Mat& depthIndex16,
                                    int depthStableRadiusPx,
                                    int depthMaxRangeSlices,
                                    float strongFrac,
                                    float weakFrac);

    // Stack ordering: near -> far, so "foreground" is the MIN slice index.
    // win16 is CV_16U winner labels.
    inline int foregroundSliceNearIsMinIndex(const cv::Mat& win16, int N)
    {
        CV_Assert(win16.type() == CV_16U);
        CV_Assert(N > 0);

        double mn = 0.0, mx = 0.0;
        cv::minMaxLoc(win16, &mn, &mx);

        int s = (int)mn;
        if (s < 0) s = 0;
        if (s >= N) s = N - 1;
        return s;
    }
};

#endif // FSFUSIONDMAPSHARED_H
