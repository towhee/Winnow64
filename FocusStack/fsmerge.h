// FSMerge.h
#ifndef FSMERGE_H
#define FSMERGE_H

#include <opencv2/core.hpp>
#include <vector>

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
bool merge(const std::vector<cv::Mat> &wavelets,
           int consistency,
           std::atomic_bool *abortFlag,
           cv::Mat &mergedOut,
           cv::Mat &depthIndex16);

// StreamPMax pipeline
struct StreamState
{
    cv::Mat maxAbs;                 // CV_32F
    cv::Mat depthIndex16;           // CV_16U
    std::vector<cv::Mat> wavelets;  // cached wavelets for consistency==2
    uint16_t sliceIndex = 0;
    cv::Size size {0,0};

    void reset()
    {
        maxAbs.release();
        depthIndex16.release();
        wavelets.clear();
        sliceIndex = 0;
        size = cv::Size(0,0);
    }

    bool initialized() const
    {
        return !maxAbs.empty() && !depthIndex16.empty() &&
               size.width > 0 && size.height > 0;
    }
};

bool mergeSlice(StreamState &state,
                const cv::Mat &wavelet,
                const cv::Size &size,
                int consistency,
                std::atomic_bool *abortFlag,
                cv::Mat &mergedOut);

// Call once before slice 0 (or anytime you want to restart)
void mergeSliceReset();

// Call once after last slice to apply consistency logic
// and (optionally) obtain depthIndex16
bool mergeSliceFinish(StreamState &state,
                      int consistency,
                      std::atomic_bool *abortFlag,
                      cv::Mat &mergedOut,
                      cv::Mat *depthIndex16Out = nullptr);

// end StreamPMax pipeline

} // end namespace FSMerge

#endif // FSMERGE_H
