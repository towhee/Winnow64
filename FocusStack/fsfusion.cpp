#include "FSFusion.h"
#include "Main/global.h"

#include "FSFusionWavelet.h"
#include "FSFusionMerge.h"
#include "FSFusionReassign.h"

#include <opencv2/imgproc.hpp>
#include <cassert>

#include <QString>

//--------------------------------------------------------------
// Helpers
//--------------------------------------------------------------
namespace
{

using ProgressCallback = FSFusion::ProgressCallback;

inline void tick(ProgressCallback cb)
{
    if (cb) cb();
}

/*
 * Padding helper
 * Ensures image is padded to a wavelet-friendly size using the same logic
 * as FSFusionWavelet::computeLevelsAndExpandedSize().
 */
cv::Mat padForWavelet(const cv::Mat &img, cv::Size &paddedSizeOut)
{
    if (img.empty()) return img;

    CV_Assert(img.channels() == 1 || img.channels() == 3);

    cv::Size expanded;
    FSFusionWavelet::computeLevelsAndExpandedSize(img.size(), expanded);
    paddedSizeOut = expanded;

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

/*
 * Simple fusion: use depthIndex16 to pick color from the winning slice.
 * This does not use wavelets; it pairs naturally with FSDepth::method="Simple".
 */
bool fuseSimple(const std::vector<cv::Mat> &grayImgs,
                const std::vector<cv::Mat> &colorImgs,
                const cv::Mat              &depthIndex16,
                cv::Mat                    &outputColor8,
                std::atomic_bool           *abortFlag,
                ProgressCallback            cb)
{
    QString srcFun = "FSFusion::fuseSimple";

    const int N = static_cast<int>(grayImgs.size());
    if (N == 0 || N != static_cast<int>(colorImgs.size()))
        return false;

    if (depthIndex16.empty() || depthIndex16.type() != CV_16U)
        return false;

    const cv::Size size = grayImgs[0].size();

    for (int i = 0; i < N; ++i)
    {
        if (grayImgs[i].empty() || colorImgs[i].empty())
            return false;
        if (grayImgs[i].size() != size || colorImgs[i].size() != size)
            return false;
    }

    if (depthIndex16.size() != size)
    {
        G::log(srcFun, "Depth index size mismatch vs input images");
        return false;
    }

    // Normalize all colors to 8UC3
    std::vector<cv::Mat> color8(N);
    for (int i = 0; i < N; ++i)
    {
        if (colorImgs[i].type() == CV_8UC3)
        {
            color8[i] = colorImgs[i];
        }
        else if (colorImgs[i].type() == CV_16UC3)
        {
            colorImgs[i].convertTo(color8[i], CV_8UC3, 255.0 / 65535.0);
        }
        else
        {
            G::log(srcFun, "Unsupported color type in slice " + QString::number(i));
            return false;
        }
    }

    outputColor8.create(size.height, size.width, CV_8UC3);

    for (int y = 0; y < size.height; ++y)
    {
        const uint16_t *dRow = depthIndex16.ptr<uint16_t>(y);
        cv::Vec3b *outRow    = outputColor8.ptr<cv::Vec3b>(y);

        for (int x = 0; x < size.width; ++x)
        {
            uint16_t s = dRow[x];
            if (s >= static_cast<uint16_t>(N))
                s = static_cast<uint16_t>(N - 1);

            outRow[x] = color8[s].at<cv::Vec3b>(y, x);
        }
    }

    tick(cb);
    return true;
}

/*
 * Full PMax fusion: this reproduces the current successful pipeline.
 *
 * IMPORTANT:
 *  - DepthIndex16 from FSDepth is validated (size only) but not used to drive
 *    the PMax decisions; the wavelet merge is self-contained.
 *  - The canonical depth map in the system is the one from FSDepth. Any
 *    implicit "depth" inside the PMax merge is internal and not exposed.
 */
bool fusePMax(const std::vector<cv::Mat> &grayImgs,
              const std::vector<cv::Mat> &colorImgs,
              const FSFusion::Options    &opt,
              const cv::Mat              &depthIndex16,
              cv::Mat                    &outputColor8,
              std::atomic_bool           *abortFlag,
              ProgressCallback            cb)
{
    QString srcFun = "FSFusion::fusePMax";
    G::log(srcFun, "Start PMax fusion");

    const int N = static_cast<int>(grayImgs.size());
    if (N == 0 || N != static_cast<int>(colorImgs.size()))
        return false;

    if (depthIndex16.empty() || depthIndex16.type() != CV_16U)
    {
        G::log(srcFun, "Depth index missing or wrong type");
        return false;
    }

    // Validate sizes and types
    const cv::Size orig = grayImgs[0].size();
    for (int i = 0; i < N; ++i)
    {
        if (grayImgs[i].empty() || colorImgs[i].empty())
            return false;
        if (grayImgs[i].size() != orig || colorImgs[i].size() != orig)
            return false;
    }

    if (depthIndex16.size() != orig)
    {
        G::log(srcFun, "Depth index size mismatch vs input images");
        return false;
    }

    // --------------------------------------------------------------------
    // 0. Pad grayscale + color images BEFORE processing (wavelet-friendly)
    // --------------------------------------------------------------------
    std::vector<cv::Mat> grayP(N);
    std::vector<cv::Mat> colorP(N);
    cv::Size paddedSize;

    for (int i = 0; i < N; ++i)
    {
        // qApp->processEvents(); if (abortFlag) return false;
        grayP[i]  = padForWavelet(grayImgs[i], paddedSize);
        colorP[i] = padForWavelet(colorImgs[i], paddedSize);
    }

    const cv::Size ps = paddedSize;
    // --------------------------------------------------------------------
    // 1. Forward wavelet per slice (grayscale only)
    // --------------------------------------------------------------------
    std::vector<cv::Mat> wavelets(N);

    G::log(srcFun, "Forward wavelet per slice");
    for (int i = 0; i < N; ++i)
    {
        // qApp->processEvents(); if (abortFlag) return false;
        G::log(srcFun, "Forward wavelet slice " + QString::number(i));
        tick(cb);

        if (!FSFusionWavelet::forward(grayP[i], opt.useOpenCL, wavelets[i]))
        {
            G::log(srcFun, "Wavelet forward failed");
            return false;
        }
    }

    // --------------------------------------------------------------------
    // 2. Merge wavelet stacks → mergedWavelet (we ignore depthIndex here)
    // --------------------------------------------------------------------
    G::log(srcFun, "Merge wavelet stacks");
    tick(cb);

    cv::Mat mergedWavelet;
    cv::Mat dummyDepthIndex16;

    if (!FSFusionMerge::merge(wavelets,
                              opt.consistency,
                              abortFlag,
                              mergedWavelet,
                              dummyDepthIndex16))
    {
        G::log(srcFun, "FSFusionMerge::merge failed");
        return false;
    }

    // --------------------------------------------------------------------
    // 3. Inverse wavelet → fusedGray8 (still padded size)
    // --------------------------------------------------------------------
    G::log(srcFun, "Inverse wavelet");
    tick(cb);

    cv::Mat fusedGray8;
    if (!FSFusionWavelet::inverse(mergedWavelet, opt.useOpenCL, fusedGray8))
    {
        G::log(srcFun, "FSFusionWavelet::inverse failed");
        return false;
    }

    // qApp->processEvents(); if (abortFlag) return false;

    // --------------------------------------------------------------------
    // 4. Build color map using padded grayscale + padded RGB images
    // --------------------------------------------------------------------
    G::log(srcFun, "Build color map");
    tick(cb);

    std::vector<FSFusionReassign::ColorEntry> colorEntries;
    std::vector<uint8_t> counts;

    if (!FSFusionReassign::buildColorMap(grayP,
                                         colorP,
                                         colorEntries,
                                         counts))
    {
        G::log(srcFun, "FSFusionReassign::buildColorMap failed");
        return false;
    }

    // qApp->processEvents(); if (abortFlag) return false;

    // --------------------------------------------------------------------
    // 5. Apply color reassignment to padded fused grayscale
    // --------------------------------------------------------------------
    G::log(srcFun, "Apply color reassignment");
    tick(cb);

    cv::Mat paddedColorOut;
    if (!FSFusionReassign::applyColorMap(fusedGray8,
                                         colorEntries,
                                         counts,
                                         paddedColorOut))
    {
        G::log(srcFun, "FSFusionReassign::applyColorMap failed");
        return false;
    }

    // qApp->processEvents(); if (abortFlag) return false;

    // --------------------------------------------------------------------
    // 6. Crop back to original (non-padded) size
    // --------------------------------------------------------------------
    G::log(srcFun, "Crop back to original size");

    if (ps == orig)
    {
        outputColor8 = paddedColorOut;
    }
    else
    {
        int padW = ps.width  - orig.width;
        int padH = ps.height - orig.height;

        int left = padW / 2;
        int top  = padH / 2;

        cv::Rect roi(left, top, orig.width, orig.height);
        outputColor8 = paddedColorOut(roi).clone();
    }

    return true;
}

} // anonymous namespace

//--------------------------------------------------------------
// Public entry point
//--------------------------------------------------------------
bool FSFusion::fuseStack(const std::vector<cv::Mat> &grayImgs,
                         const std::vector<cv::Mat> &colorImgs,
                         const Options              &opt,
                         const cv::Mat              &depthIndex16,
                         cv::Mat                    &outputColor8,
                         std::atomic_bool           *abortFlag,
                         ProgressCallback            progressCallback)
{
    const QString method = opt.method.trimmed();

    if (method.compare("Simple", Qt::CaseInsensitive) == 0)
    {
        return fuseSimple(grayImgs,
                          colorImgs,
                          depthIndex16,
                          outputColor8,
                          abortFlag,
                          progressCallback);
    }

    // Default: full PMax fusion
    return fusePMax(grayImgs,
                    colorImgs,
                    opt,
                    depthIndex16,
                    outputColor8,
                    abortFlag,
                    progressCallback);
}
