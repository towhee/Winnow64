#ifndef FOCUSWAVELET_H
#define FOCUSWAVELET_H

#include <opencv2/opencv.hpp>
#include <vector>
#include <QDebug>
#include "ImageStack/focusstackutilities.h"


namespace FSWavelet {

// 3-detail + 1-lowpass per level (Haar)
struct Level {
    cv::Mat low;     // L at next level (downsampled)
    cv::Mat lh, hl, hh; // details at this level (same size as 'low')
};

using Pyramid = std::vector<Level>;

// ---------- Core API ----------

// levels: 1..5 typical. Input must be CV_32FC1 (gray) or CV_32FC3 (BGR).
Pyramid decompose(const cv::Mat& img32f, int levels = 4);

// Reconstruct image from a pyramid (inverse Haar), returns CV_32F (same chans as input).
cv::Mat reconstruct(const Pyramid& pyr);

// Focus energy map (CV_32F, full-res): sum of |detail|^2 over all levels, upsampled.
cv::Mat focusEnergy(const Pyramid& pyr);

// Wavelet-domain fusion of a stack using Petteri μ/σ/A depth guidance.
// stack32f: CV_32FC3 images [0..1], same size.
// mu32f: CV_32F in slice units (0..N-1), sigma32f: CV_32F (>0), amp32f: CV_32F [0..1] (optional).
// levels: number of wavelet levels. Returns CV_32FC3 [0..1].
cv::Mat fuseWavelet(const std::vector<cv::Mat>& stack32f,
                    const cv::Mat& mu32f, const cv::Mat& sigma32f, const cv::Mat& amp32f,
                    int levels = 4);

} // namespace FSWavelet
#endif
// FOCUSWAVELET_H
