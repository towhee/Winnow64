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

    // DIAGNOSTIC CONFIDENCE / STABILITY / HALO-RISK
    cv::Mat confidence01_fromTop2Scores(const cv::Mat& s0_32,
                                        const cv::Mat& s1_32,
                                        float eps = 1e-6f);
    cv::Mat depthStabilityMask8(const cv::Mat& idx0_16,
                                const cv::Mat& idx1_16,
                                int medianK = 5,
                                int tolSlices = 1);
    cv::Mat haloRiskRing8(const cv::Mat& fg8, int ringPx);
    cv::Mat dmapDiagnosticBGR(const cv::Mat& idx0_16,
                              const cv::Mat& idx1_16,
                              const cv::Mat& fg8,        // 0/255 foreground silhouette (your best FG)
                              const cv::Mat& conf01_opt, // CV_32F 0..1, may be empty
                              const cv::Mat& s0_32_opt,   // CV_32F, may be empty if conf01 provided
                              const cv::Mat& s1_32_opt,   // CV_32F
                              int ringPx,
                              float ringConfMax = 0.22f,  // only show ring risk where confidence is low
                              int stabilityMedianK = 5,
                              int stabilityTolSlices = 1);
    // END DIAGNOSTIC CONFIDENCE / STABILITY / HALO-RISK

    // BOUNDARY OWNERSHIP
    // FG cleanup (small utility)
    cv::Mat morphClose8(const cv::Mat& bin8, int px); // 0/255 -> 0/255

    // Deterministic ring px default width (input src image size)
    int defaultRingPx(const cv::Size& sz);

    // Build ring just outside FG: ring = dilate(FG, ringPx) - FG
    cv::Mat ringOutsideFg8(const cv::Mat& fg8, int ringPx);

    // Two-pass ownership propagation (deterministic):
    // Given FG mask + depth labels, produce donor index map in the ring.
    // - Only pixels in ringOut8 get assigned (others untouched).
    // - Seeds are boundary pixels of FG; donor slice index is depthIndex16(seed).
    // Output:
    // - overrideMask8 = ringOut8 (0/255)
    // - overrideWinner16 = CV_16U donor slice index for ring pixels
    bool ownershipPropagateTwoPass_Outward(
        const cv::Mat& fg8,                 // CV_8U 0/255
        const cv::Mat& depthIndex16,        // CV_16U
        int ringPx,
        int erodePx,
        cv::Mat& overrideMask8,             // OUT CV_8U 0/255
        cv::Mat& overrideWinner16           // OUT CV_16U (valid only where mask=255)
        );
    // END BOUNDARY OWNERSHIP


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
    cv::Mat focusMetric32Laplacian(const cv::Mat& gray8, float preBlurSigma, int lapKSize);
    cv::Mat focusMetric32Tenengrad(const cv::Mat& gray8, float preBlurSigma, int lapKSize);

    // --- Small inline helpers ---

    // Keep only the largest connected component from a binary 8-bit mask (0/255).
    // Returns a CV_8U 0/255 mask. If empty or no foreground, returns zeros.
    cv::Mat keepLargestCC(const cv::Mat& bin8, int connectivity = 8);

    cv::Mat keepCCsTouchingMask(const cv::Mat& bin8,
                               const cv::Mat& touch8,
                               int connectivity);

    float robustMax99(const cv::Mat& m32f);

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
                                    int interiorPx,
                                    float expandTexFrac);

    cv::Mat buildFgFromGroundTruth(cv::Size origSz);

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

    inline uint16_t clampU16(int v) {
        if (v < 0) return 0;
        if (v > 65535) return 65535;
        return (uint16_t)v;
    }
};

#endif // FSFUSIONDMAPSHARED_H
