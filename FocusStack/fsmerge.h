// FSMerge.h
#ifndef FSMERGE_H
#define FSMERGE_H

#include <opencv2/core.hpp>
#include <vector>
#include <atomic>

namespace FSMerge
{
/*
    Merge a stack of complex wavelet images into one (PMax):
    - wavelets : N images, CV_32FC2, all same size
    - consistency :
      0 = no denoise, pure max-abs selection
      1 = subband consistency voting
      2 = + neighbourhood outlier clean-up
    - mergedOut  : CV_32FC2 (same size)
    - depthIndex : CV_16U (slice index 0..N-1)
*/

enum class MergeMode
{
    PMax,
    Weighted
};

struct WeightedParams
{
    // Weight function: w = pow(normEnergy + eps, power)
    // power ~ 2..8. Higher = closer to PMax.
    float power = 4.0f;

    // Base sigma for the finest level (level 0). Coarser levels scale up.
    // Example: sigma0=1.0 -> level 1 sigma=2, level 2 sigma=4, ...
    float sigma0 = 1.0f;

    // Small constants for numerical stability.
    float epsEnergy = 1e-8f;
    float epsWeight = 1e-8f;

    // If true, also blend the lowpass (top-left) region per level.
    // If false, lowpass is blended globally once (still weighted).
    bool includeLowpass = true;

    // NEW: protect level0 lowpass from weighted blending (LL)
    // Toggle this to run StmPMaxWt vs StmPMaxWtLL
    bool protectLowpassLevel0 = true;
};


bool merge(const std::vector<cv::Mat> &wavelets,
           int consistency,
           std::atomic_bool *abortFlag,
           cv::Mat &mergedOut,
           cv::Mat &depthIndex16);

// StreamPMax pipeline
struct StreamState
{
    // Debug "winner" tracking (still useful even in Weighted mode)
    cv::Mat maxAbs;                 // CV_32F
    cv::Mat depthIndex16;           // CV_16U

    // Weighted energy
    cv::Mat maxWeightedScore;       // CV_32F
    cv::Mat weightedDepthIndex16;   // CV_16U

    // Cached wavelets for PMax neighbor denoise (consistency==2)
    std::vector<cv::Mat> wavelets;

    // Weighted accumulators (used only in Weighted mode)
    cv::Mat sumW;                   // CV_32F
    cv::Mat sumWV;                  // CV_32FC2

    // NEW: protected level0 lowpass (LL)
    cv::Mat lpMaxAbs;     // CV_32F, size == wavelet
    cv::Mat lpMerged;     // CV_32FC2, size == wavelet

    uint16_t sliceIndex = 0;
    cv::Size size {0,0};

    void reset()
    {
        maxAbs.release();
        depthIndex16.release();
        maxWeightedScore.release();
        weightedDepthIndex16.release();
        wavelets.clear();
        sumW.release();
        sumWV.release();
        sliceIndex = 0;
        size = cv::Size(0,0);
    }

    bool initializedPMax() const
    {
        return size.width > 0 && size.height > 0 &&
               !maxAbs.empty() && !depthIndex16.empty();
    }

    bool initializedWeighted() const
    {
        return initializedPMax() &&
               !sumW.empty() && !sumWV.empty() &&
               !maxWeightedScore.empty() && !weightedDepthIndex16.empty();
    }

    // NEW helper (LL)
    bool hasProtectedLP() const
    {
        return !lpMaxAbs.empty() && !lpMerged.empty();
    }
};

// Call once before slice 0 (or anytime you want to restart)
void mergeSliceReset();

// ---------------------------------------------------------
// PMax streaming entry points
bool mergeSlice(StreamState &state,
                const cv::Mat &wavelet,
                const cv::Size &size,
                int consistency,
                std::atomic_bool *abortFlag,
                cv::Mat &mergedOut);

// Call once after last slice to apply consistency logic
// and (optionally) obtain depthIndex16
bool mergeSliceFinish(StreamState &state,
                      int consistency,
                      std::atomic_bool *abortFlag,
                      cv::Mat &mergedOut,
                      cv::Mat &depthIndex16Out);

// ---------------------------------------------------------
// Weighted streaming entry points (reduce haloing)

bool mergeSliceWeighted(StreamState &state,
                        const cv::Mat &wavelet,
                        const cv::Size &size,
                        const WeightedParams &wp,
                        int consistency,
                        std::atomic_bool *abortFlag,
                        cv::Mat &mergedOut);

bool mergeSliceFinishWeighted(StreamState &state,
                              const WeightedParams &wp,
                              int consistency,
                              std::atomic_bool *abortFlag,
                              cv::Mat &mergedOut,
                              cv::Mat &weightedWinnerOut,
                              cv::Mat &energyWinnerOut);

// end StreamPMax pipeline

} // end namespace FSMerge

#endif // FSMERGE_H
