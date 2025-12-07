// FSFusion.h
#ifndef FSFUSION_H
#define FSFUSION_H

#include <opencv2/core.hpp>
#include <vector>

class FSFusion
{
public:
    struct Options
    {
        bool useOpenCL  = true; // GPU wavelet acceleration via cv::UMat
        int  consistency = 1;    // 0 = off, 1 = subband denoise, 2 = +neighbour denoise
    };

    /*
     * Petteri-style PMax fusion:
     *  - grayImgs  : aligned grayscale 8U (one per slice, all same size)
     *  - colorImgs : aligned color 8UC3 or 16UC3 (same count/size as gray)
     *  - opt       : see above
     *  - outputColor8 : final fused RGB 8-bit
     *  - depthIndex16 : (optional) 16U index map (0..N-1) indicating which slice won
     *
     * Returns false on any error (size/type mismatch, empty input, etc).
     */
    static bool fuseStack(const std::vector<cv::Mat> &grayImgs,
                          const std::vector<cv::Mat> &colorImgs,
                          const Options &opt,
                          cv::Mat &outputColor8,
                          cv::Mat &depthIndex16);
};

#endif // FSFUSION_H
