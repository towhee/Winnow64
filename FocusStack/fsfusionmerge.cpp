// FSFusionMerge.cpp
#include "FSFusionMerge.h"
#include "Main/global.h"

#include <opencv2/core.hpp>
#include <cassert>

// We reuse the levels logic from FSFusionWavelet
namespace
{
int levelsForSize(cv::Size size)
{
    const int minLevels = 5;
    const int maxLevels = 10;

    int dimension = std::max(size.width, size.height);

    int levels = minLevels;
    while ((dimension >> levels) > 8 && levels < maxLevels)
    {
        levels++;
    }

    int divider = (1 << levels);
    cv::Size expanded = size;

    if (expanded.width % divider != 0)
    {
        expanded.width += divider - expanded.width % divider;
    }
    if (expanded.height % divider != 0)
    {
        expanded.height += divider - expanded.height % divider;
    }

    if (levels < maxLevels && expanded != size)
    {
        levels = levelsForSize(expanded);
    }

    return levels;
}

inline void getSqAbsval(const cv::Mat &complexMat, cv::Mat &absval)
{
    CV_Assert(complexMat.type() == CV_32FC2);
    absval.create(complexMat.rows, complexMat.cols, CV_32F);

    for (int y = 0; y < complexMat.rows; ++y)
    {
        const cv::Vec2f *row = complexMat.ptr<cv::Vec2f>(y);
        float *outRow = absval.ptr<float>(y);
        for (int x = 0; x < complexMat.cols; ++x)
        {
            const cv::Vec2f &v = row[x];
            outRow[x] = v[0] * v[0] + v[1] * v[1];
        }
    }
}

// Two-out-of-three subband voting (adapted from Task_Merge::denoise_subbands)
void denoiseSubbands(cv::Mat &mergedWavelet, cv::Mat &depthIndex)
{
    CV_Assert(mergedWavelet.type() == CV_32FC2);
    CV_Assert(depthIndex.type() == CV_16U);
    CV_Assert(mergedWavelet.size() == depthIndex.size());

    int levels = levelsForSize(mergedWavelet.size());
    for (int level = 0; level < levels; ++level)
    {
        int w  = mergedWavelet.cols >> level;
        int h  = mergedWavelet.rows >> level;
        int w2 = w / 2;
        int h2 = h / 2;

        cv::Mat sub1 = depthIndex(cv::Rect(w2,     0,   w2, h2));
        cv::Mat sub2 = depthIndex(cv::Rect(w2,    h2,   w2, h2));
        cv::Mat sub3 = depthIndex(cv::Rect(0,     h2,   w2, h2));

        for (int y = 0; y < h2; ++y)
        {
            for (int x = 0; x < w2; ++x)
            {
                uint16_t v1 = sub1.at<uint16_t>(y, x);
                uint16_t v2 = sub2.at<uint16_t>(y, x);
                uint16_t v3 = sub3.at<uint16_t>(y, x);

                if (v1 == v2 && v2 == v3)
                {
                    // already consistent
                    continue;
                }

                if (v2 == v3)
                {
                    // Fix sub1
                    sub1.at<uint16_t>(y, x) = v2;
                }
                else if (v1 == v3)
                {
                    // Fix sub2
                    sub2.at<uint16_t>(y, x) = v1;
                }
                else if (v1 == v2)
                {
                    // Fix sub3
                    sub3.at<uint16_t>(y, x) = v1;
                }
            }
        }
    }
}

// Neighbourhood denoise (adapted from Task_Merge::denoise_neighbours)
void denoiseNeighbours(cv::Mat &mergedWavelet,
                       cv::Mat &depthIndex,
                       const std::vector<cv::Mat> &wavelets)
{
    CV_Assert(mergedWavelet.type() == CV_32FC2);
    CV_Assert(depthIndex.type() == CV_16U);

    for (int y = 1; y < depthIndex.rows - 1; ++y)
    {
        for (int x = 1; x < depthIndex.cols - 1; ++x)
        {
            uint16_t left   = depthIndex.at<uint16_t>(y, x - 1);
            uint16_t right  = depthIndex.at<uint16_t>(y, x + 1);
            uint16_t top    = depthIndex.at<uint16_t>(y - 1, x);
            uint16_t bottom = depthIndex.at<uint16_t>(y + 1, x);
            uint16_t center = depthIndex.at<uint16_t>(y, x);

            bool gtAll = (center > top && center > bottom && center > left && center > right);
            bool ltAll = (center < top && center < bottom && center < left && center < right);

            if (!(gtAll || ltAll))
                continue;

            int avg = (top + bottom + left + right + 2) / 4;
            depthIndex.at<uint16_t>(y, x) = static_cast<uint16_t>(avg);

            // Re-sample wavelet from the winning slice
            CV_Assert(avg >= 0 && avg < static_cast<int>(wavelets.size()));
            const cv::Mat &srcWavelet = wavelets[avg];
            mergedWavelet.at<cv::Vec2f>(y, x) =
                srcWavelet.at<cv::Vec2f>(y, x);
        }
    }
}

} // anonymous namespace

namespace FSFusionMerge
{

bool merge(const std::vector<cv::Mat> &wavelets,
           int consistency,
           std::atomic_bool *abortFlag,
           cv::Mat &mergedOut,
           cv::Mat &depthIndex16)
{
    if (wavelets.empty()) {
        QString msg = "FSFusionMerge: wavelets are empty.";
        qWarning() << msg;
        return false;
    }

    const int N = static_cast<int>(wavelets.size());
    const cv::Size size = wavelets[0].size();

    // Validate wavelets
    for (int i = 0; i < N; ++i)
    {
        // qApp->processEvents(); if (abortFlag) return false;

        if (wavelets[i].empty() ||
            wavelets[i].type() != CV_32FC2 ||
            wavelets[i].size() != size)
        {
            QString idx = QString::number(i);
            QString msg = "FSFusionMerge: wavelets " + idx;
            if (wavelets[i].empty()) msg += " is empty.";
            if (wavelets[i].type() != CV_32FC2) msg += " type != CV_32FC2.";
            if (wavelets[i].size() != size) msg += " != wavelets[0].size.";
            qWarning() << msg;
            return false;
        }
    }

    int rows = size.height;
    int cols = size.width;

    cv::Mat maxAbs(rows, cols, CV_32F);
    mergedOut.create(rows, cols, CV_32FC2);
    depthIndex16.create(rows, cols, CV_16U);

    maxAbs = -1.0f;
    depthIndex16 = 0;

    // PMax: for each pixel, choose wavelet with max |v|^2
    for (int i = 0; i < N; ++i)
    {
        // qApp->processEvents(); if (abortFlag) return false;
        cv::Mat absval(rows, cols, CV_32F);
        getSqAbsval(wavelets[i], absval);

        cv::Mat mask = (absval > maxAbs);
        absval.copyTo(maxAbs, mask);
        wavelets[i].copyTo(mergedOut, mask);
        depthIndex16.setTo(static_cast<uint16_t>(i), mask);
    }

    // qApp->processEvents(); if (abortFlag) return false;

    if (consistency >= 1)
    {
        denoiseSubbands(mergedOut, depthIndex16);
    }

    if (consistency >= 2)
    {
        denoiseNeighbours(mergedOut, depthIndex16, wavelets);
    }

    return true;
}

} // namespace FSFusionMerge
