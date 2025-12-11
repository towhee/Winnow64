// FSFusionMerge.h
#ifndef FSFUSIONMERGE_H
#define FSFUSIONMERGE_H

#include <opencv2/core.hpp>
#include <vector>

namespace FSFusionMerge
{
/*
     * Merge a stack of complex wavelet images into one (PMax):
     *  - wavelets : N images, CV_32FC2, all same size
     *  - consistency :
     *      0 = no denoise, pure max-abs selection
     *      1 = subband consistency voting
     *      2 = + neighbourhood outlier clean-up
     *  - mergedOut  : CV_32FC2 (same size)
     *  - depthIndex : CV_16U (slice index 0..N-1)
     */
bool merge(const std::vector<cv::Mat> &wavelets,
           int consistency,
           std::atomic_bool *abortFlag,
           cv::Mat &mergedOut,
           cv::Mat &depthIndex16);
}

#endif // FSFUSIONMERGE_H
