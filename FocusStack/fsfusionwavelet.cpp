// FSFusionWavelet.cpp
#include "fsfusionwavelet.h"

// Petteri wavelet templates (no Task/worker usage here)
#include "FocusStack/FSFusionWaveletTemplates.h"

#include <opencv2/core/ocl.hpp>
#include <cassert>

#include <QDebug>

namespace FSFusionWavelet
{

// Compute expanded wavelet-friendly size (Petteri compatible)
int computeLevelsAndExpandedSize(const cv::Size &orig,
                                        cv::Size &expanded)
{
    int w = orig.width;
    int h = orig.height;

    int levels = 5;     // Petteri::min_levels = 5
    while ((w >> levels) > 8 && (h >> levels) > 8 && levels < 10)
        levels++;

    int divider = (1 << levels);
    expanded = orig;

    if (expanded.width % divider != 0)
        expanded.width += divider - (expanded.width % divider);

    if (expanded.height % divider != 0)
        expanded.height += divider - (expanded.height % divider);

    return levels;
}

// Thin wrapper so everything uses the same logic
int levelsForSize(cv::Size size)
{
    cv::Size dummy;
    return computeLevelsAndExpandedSize(size, dummy);
}

bool forward(const cv::Mat &inputGray8,
             bool useOpenCL,
             cv::Mat &waveletOut)
{
    if (inputGray8.empty())
        return false;

    CV_Assert(inputGray8.channels() == 1);

    cv::Mat gray8;
    if (inputGray8.type() != CV_8U)
        inputGray8.convertTo(gray8, CV_8U);
    else
        gray8 = inputGray8;

    // --- NEW: ensure padding to wavelet-friendly size ---
    cv::Size expanded;
    int levels = computeLevelsAndExpandedSize(gray8.size(), expanded);

    if (expanded != gray8.size())
    {
        int padW = expanded.width  - gray8.cols;
        int padH = expanded.height - gray8.rows;

        int left   = padW / 2;
        int right  = padW - left;
        int top    = padH / 2;
        int bottom = padH - top;

        cv::copyMakeBorder(gray8,
                           gray8,
                           top, bottom,
                           left, right,
                           cv::BORDER_REFLECT);
    }

    // Now safe to assert wavelet constraints
    int factor = (1 << levels);
    assert(gray8.rows % factor == 0 && gray8.cols % factor == 0);

    if (!useOpenCL)
    {
        // qDebug() << "FSFusionWavelet Processing forward using CPU";
        // CPU path: Wavelet<cv::Mat>
        cv::Mat tmp(gray8.rows, gray8.cols, CV_32FC2);
        waveletOut.create(gray8.rows, gray8.cols, CV_32FC2);

        // Real -> complex (imaginary = 0)
        cv::Mat fimg(gray8.rows, gray8.cols, CV_32F);
        cv::Mat zeros(gray8.rows, gray8.cols, CV_32F);
        gray8.convertTo(fimg, CV_32F);
        zeros = 0.0f;

        cv::Mat channels[] = {fimg, zeros};
        cv::merge(channels, 2, tmp);

        FStack::Wavelet<cv::Mat>::decompose_multilevel(tmp, waveletOut, levels);
    }
    else
    {
        // qDebug() << "FSFusionWavelet Processing forward using OpenCL";
        // OpenCL path: Wavelet<cv::UMat>
        cv::Mat tmp(gray8.rows, gray8.cols, CV_32FC2);

        cv::Mat fimg(gray8.rows, gray8.cols, CV_32F);
        cv::Mat zeros(gray8.rows, gray8.cols, CV_32F);
        gray8.convertTo(fimg, CV_32F);
        zeros = 0.0f;

        cv::Mat channels[] = {fimg, zeros};
        cv::merge(channels, 2, tmp);

        cv::UMat utmp(gray8.rows, gray8.cols, CV_32FC2);
        tmp.copyTo(utmp);

        cv::UMat uout(gray8.rows, gray8.cols, CV_32FC2);
        FStack::Wavelet<cv::UMat>::decompose_multilevel(utmp, uout, levels);

        uout.copyTo(waveletOut);
    }

    return true;
}

bool inverse(const cv::Mat &wavelet,
             bool useOpenCL,
             cv::Mat &grayOut8)
{
    if (wavelet.empty())
        return false;

    CV_Assert(wavelet.type() == CV_32FC2);

    int levels = levelsForSize(wavelet.size());

    if (!useOpenCL)
    {
        // CPU path
        cv::Mat tmp(wavelet.rows, wavelet.cols, CV_32FC2);
        FStack::Wavelet<cv::Mat>::compose_multilevel(wavelet, tmp, levels);

        cv::Mat channels[2];
        cv::split(tmp, channels);       // channels[0] = real

        channels[0].convertTo(grayOut8, CV_8U);
    }
    else
    {
        // OpenCL path
        cv::UMat usrc;
        wavelet.copyTo(usrc);

        cv::UMat utmp(usrc.rows, usrc.cols, CV_32FC2);
        FStack::Wavelet<cv::UMat>::compose_multilevel(usrc, utmp, levels);

        cv::Mat channels[2];
        cv::split(utmp.getMat(cv::ACCESS_READ), channels);

        channels[0].convertTo(grayOut8, CV_8U);
    }

    return true;
}

} // namespace FSFusionWavelet
