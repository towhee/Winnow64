// FSFusionWavelet.h
#ifndef FSFUSIONWAVELET_H
#define FSFUSIONWAVELET_H

#include <opencv2/core.hpp>

namespace FSFusionWavelet
{
int computeLevelsAndExpandedSize(const cv::Size &orig, cv::Size &expanded);

int levelsForSize(cv::Size size);

bool forward(const cv::Mat &inputGray8,
             bool useOpenCL,
             cv::Mat &waveletOut);   // NEW: return the padded size

bool inverse(const cv::Mat &wavelet,
             bool useOpenCL,
             cv::Mat &grayOut8);
}

#endif // FSFUSIONWAVELET_H
