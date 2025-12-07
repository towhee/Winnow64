#include "Main/global.h"
#include "FSFusion.h"
#include "FSFusionWavelet.h"
#include "FSFusionMerge.h"
#include "FSFusionReassign.h"

#include <opencv2/imgproc.hpp>

#include <QString>
// #include <opencv2/imgcodecs.hpp>

/*
 * Padding helper
 * Ensures image is padded to a wavelet-friendly size using the same logic
 * as FSFusionWavelet::forward().
 * This avoids size mismatch between:
 *   - gray input to wavelet
 *   - merged/inverse wavelet output
 *   - reassignment map
 */
static cv::Mat padForWavelet(const cv::Mat &img)
{
    if (img.empty()) return img;

    CV_Assert(img.channels() == 1 || img.channels() == 3);

    cv::Size expanded;
    int levels = FSFusionWavelet::computeLevelsAndExpandedSize(img.size(), expanded);

    if (expanded == img.size())
        return img;   // No padding required

    int padW = expanded.width  - img.cols;
    int padH = expanded.height - img.rows;

    int left   = padW / 2;
    int right  = padW - left;
    int top    = padH / 2;
    int bottom = padH - top;

    cv::Mat padded;
    cv::copyMakeBorder(img,
                       padded,
                       top, bottom,
                       left, right,
                       cv::BORDER_REFLECT);

    return padded;
}

bool FSFusion::fuseStack(const std::vector<cv::Mat> &grayImgs,
                         const std::vector<cv::Mat> &colorImgs,
                         const Options &opt,
                         cv::Mat &outputColor8,
                         cv::Mat &depthIndex16,
                         ProgressCallback progressCallback)
{
    QString srcFun = "FSFusion::fuseStack";
    G::log(srcFun, "Start, validate gray and color images");

    auto tick = [&]() {
        if (progressCallback) progressCallback();
    };

    const int N = static_cast<int>(grayImgs.size());
    if (N == 0 || N != static_cast<int>(colorImgs.size()))
        return false;

    // Validate all images are same original size
    const cv::Size orig = grayImgs[0].size();
    for (int i = 0; i < N; ++i)
    {
        if (grayImgs[i].empty() || colorImgs[i].empty())
            return false;
        if (grayImgs[i].size() != orig || colorImgs[i].size() != orig)
            return false;
    }

    // --------------------------------------------------------------------
    // 0. Pad all grayscale + color images BEFORE processing
    // --------------------------------------------------------------------
    G::log(srcFun, "std::vector<cv::Mat> grayP(N)");
    std::vector<cv::Mat> grayP(N);
    G::log(srcFun, "std::vector<cv::Mat> colorP(N)");
    std::vector<cv::Mat> colorP(N);

    for (int i = 0; i < N; ++i)
    {
        G::log(srcFun, "Padding " + QString::number(i));
        grayP[i]  = padForWavelet(grayImgs[i]);
        colorP[i] = padForWavelet(colorImgs[i]);  // Must match grayscale geometry
    }

    const cv::Size paddedSize = grayP[0].size();

    // --------------------------------------------------------------------
    // 1. Forward wavelet per slice
    // --------------------------------------------------------------------
    G::log(srcFun, "Forward wavelet per slice");
    std::vector<cv::Mat> wavelets(N);

    for (int i = 0; i < N; ++i)
    {
        G::log(srcFun, "Forward wavelet per slice " + QString::number(i));
        tick();
        if (!FSFusionWavelet::forward(grayP[i], opt.useOpenCL, wavelets[i]))
            return false;
    }

    // --------------------------------------------------------------------
    // 2. Merge wavelet stacks → mergedWavelet + depthIndex16
    // --------------------------------------------------------------------
    G::log(srcFun, "Merge wavelet stacks");
    tick();
    cv::Mat mergedWavelet;

    if (!FSFusionMerge::merge(wavelets,
                              opt.consistency,
                              mergedWavelet,
                              depthIndex16))
    {
        return false;
    }

    // --------------------------------------------------------------------
    // 3. Inverse wavelet → fusedGray8 (still padded size)
    // --------------------------------------------------------------------
    G::log(srcFun, "Inverse wavelet");
    tick();
    cv::Mat fusedGray8;

    if (!FSFusionWavelet::inverse(mergedWavelet,
                                  opt.useOpenCL,
                                  fusedGray8))
    {
        return false;
    }

    // --------------------------------------------------------------------
    // 4. Build color map using *padded* grayscale + padded RGB images
    // --------------------------------------------------------------------
    G::log(srcFun, "Build color map");
    tick();
    std::vector<FSFusionReassign::ColorEntry> colorEntries;
    std::vector<uint8_t> counts;

    if (!FSFusionReassign::buildColorMap(grayP,
                                         colorP,
                                         colorEntries,
                                         counts))
    {
        return false;
    }

    // --------------------------------------------------------------------
    // 5. Apply color reassignment to padded fused grayscale
    // --------------------------------------------------------------------
    G::log(srcFun, "Apply color reasignment");
    tick();
    cv::Mat paddedColorOut;

    if (!FSFusionReassign::applyColorMap(fusedGray8,
                                         colorEntries,
                                         counts,
                                         paddedColorOut))
    {
        return false;
    }

    // --------------------------------------------------------------------
    // 6. Crop back to original (non-padded) size
    // --------------------------------------------------------------------
    G::log(srcFun, "Crop back to original");
    if (paddedSize == orig)
    {
        outputColor8 = paddedColorOut;    // no padding was used
    }
    else
    {
        int padW = paddedSize.width  - orig.width;
        int padH = paddedSize.height - orig.height;

        int left   = padW / 2;
        int top    = padH / 2;

        cv::Rect roi(left, top, orig.width, orig.height);
        outputColor8 = paddedColorOut(roi).clone();

        // Depth index also must be cropped
        if (!depthIndex16.empty() &&
            depthIndex16.size() == paddedSize)
        {
            depthIndex16 = depthIndex16(roi).clone();
        }
    }

    return true;
}
