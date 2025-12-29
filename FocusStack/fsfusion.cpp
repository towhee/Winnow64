#include "FSFusion.h"
#include "Main/global.h"

#include "FSFusionWavelet.h"
#include "FSMerge.h"
#include "FSFusionReassign.h"

#include <opencv2/imgproc.hpp>
#include <cassert>

#include <QString>

/*
Purpose
FSFusion implements the fusion stage of the focus stacking pipeline. It takes
a set of aligned grayscale slices and aligned color slices and produces a
single fused color image (CV_8UC3) using either a simple per-pixel slice pick
(“Simple”) or the full Petteri-style wavelet PMax path (default).

Key Inputs / Outputs
Inputs
- grayImgs      : vector of aligned grayscale images (1 channel)
- colorImgs     : vector of aligned color images (3 channel)
- depthIndex16  : CV_16U map with per-pixel slice indices (0..N-1)
- opt           : FSFusion options (method, OpenCL, consistency, etc.)
- abortFlag     : optional cancellation flag
- callbacks     : optional status/progress callbacks

Output
- outputColor8  : fused result as CV_8UC3 at original (non-padded) size

Two Fusion Modes
    1.	Simple fusion (method == “Simple”)
    •	Uses depthIndex16 directly: for each pixel, select the color from the
“winning” slice index stored in depthIndex16.
    •	Does NOT use wavelets.
    •	Normalizes input color slices to CV_8UC3 (converts from 16UC3 if needed).
    •	Produces outputColor8 by direct lookup per pixel.
    2.	PMax fusion (default path)
    •	Reproduces the current successful wavelet-based fusion workflow.
    •	IMPORTANT: depthIndex16 is validated (type/size) but is not used to drive
the wavelet merge decisions. The merge computes its own internal decisions.
    •	Uses the following high-level stages:
(a) Pad inputs to wavelet-friendly size (reflect border padding)
(b) Forward wavelet transform per grayscale slice
(c) Merge wavelet stacks with FSMerge::merge (consistency parameter)
(d) Inverse wavelet to get fused grayscale (still padded)
(e) Build a color reassignment map from padded gray+color slices
(f) Apply color map to fused grayscale to produce padded color result
(g) Crop padded result back to original size to produce outputColor8

Padding Behavior
Wavelet code requires dimensions compatible with its level structure. The
helper padForWavelet() expands images via cv::BORDER_REFLECT using the same
expanded-size logic as FSFusionWavelet::computeLevelsAndExpandedSize(). The
result is cropped back to original size at the end of PMax fusion.

Abort / Progress
    •	abortFlag is checked at multiple points in the PMax path (between stages and
per-slice wavelet forward).
    •	progressCallback is invoked as a simple “tick” at key milestones and per
slice during wavelet forward.
    •	statusCallback is used to provide coarse stage messages in the PMax path.

Role in the Pipeline:
FSFusion is the stage that converts aligned slice stacks into the final fused
image. In “Simple” mode it relies directly on FSDepth’s depthIndex16. In PMax
mode it uses wavelet merge + color reassignment to produce the fused color
image, while treating the FSDepth depthIndex16 primarily as a validated
artifact rather than a control signal.
*/

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
        if (G::FSLog) G::log(srcFun, "Depth index size mismatch vs input images");
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
            if (G::FSLog) G::log(srcFun, "Unsupported color type in slice " + QString::number(i));
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
              cv::Mat                    &depthIndex16,
              cv::Mat                    &outputColor8,
              std::atomic_bool           *abortFlag,
              FSFusion::StatusCallback    statusCallback,
              ProgressCallback            progressCallback)
{
    QString srcFun = "FSFusion::fusePMax";
    if (G::FSLog) G::log(srcFun, "Start PMax fusion");

    const int N = static_cast<int>(grayImgs.size());
    if (N == 0 || N != static_cast<int>(colorImgs.size()))
        return false;

    // if (depthIndex16.empty() || depthIndex16.type() != CV_16U)
    // {
    //     if (G::FSLog) G::log(srcFun, "Depth index missing or wrong type");
    //     return false;
    // }

    // Validate sizes and types
    const cv::Size orig = grayImgs[0].size();
    for (int i = 0; i < N; ++i)
    {
        if (grayImgs[i].empty() || colorImgs[i].empty())
            return false;
        if (grayImgs[i].size() != orig || colorImgs[i].size() != orig)
            return false;
    }

    // if (depthIndex16.size() != orig)
    // {
    //     if (G::FSLog) G::log(srcFun, "Depth index size mismatch vs input images");
    //     return false;
    // }

    // --------------------------------------------------------------------
    // 0. Pad grayscale + color images BEFORE processing (wavelet-friendly)
    // --------------------------------------------------------------------
    std::vector<cv::Mat> grayP(N);
    std::vector<cv::Mat> colorP(N);
    cv::Size paddedSize;

    for (int i = 0; i < N; ++i)
    {
        if (abortFlag && abortFlag->load(std::memory_order_relaxed)) return false;
        grayP[i]  = padForWavelet(grayImgs[i], paddedSize);
        colorP[i] = padForWavelet(colorImgs[i], paddedSize);
    }

    const cv::Size ps = paddedSize;
    // --------------------------------------------------------------------
    // 1. Forward wavelet per slice (grayscale only)
    // --------------------------------------------------------------------
    std::vector<cv::Mat> wavelets(N);

    if (G::FSLog) G::log(srcFun, "Forward wavelet per slice");
    for (int i = 0; i < N; ++i)
    {
        if (abortFlag && abortFlag->load(std::memory_order_relaxed)) return false;
        QString msg = "Forward wavelet slice" + QString::number(i);
        if (G::FSLog) G::log(srcFun, msg);
        if (statusCallback) statusCallback(msg);
        if (progressCallback) progressCallback();

        if (!FSFusionWavelet::forward(grayP[i], opt.useOpenCL, wavelets[i]))
        {
            if (G::FSLog) G::log(srcFun, "Wavelet forward failed");
            return false;
        }
    }

    // --------------------------------------------------------------------
    // 2. Merge wavelet stacks → mergedWavelet (we ignore depthIndex here)
    // --------------------------------------------------------------------
    QString msg = "Merge wavelet stacks";
    if (G::FSLog) G::log(srcFun, msg);
    if (statusCallback) statusCallback(msg);
    if (progressCallback) progressCallback();

    cv::Mat mergedWavelet;
    cv::Mat dummyDepthIndex16;

    if (!FSMerge::merge(wavelets,
                              opt.consistency,
                              abortFlag,
                              mergedWavelet,
                              depthIndex16))
                              // dummyDepthIndex16))
    {
        if (G::FSLog) G::log(srcFun, "FSMerge::merge failed");
        return false;
    }

    // --------------------------------------------------------------------
    // 3. Inverse wavelet → fusedGray8 (still padded size)
    // --------------------------------------------------------------------
    msg = "Inverse wavelet";
    if (G::FSLog) G::log(srcFun, msg);
    if (statusCallback) statusCallback(msg);
    if (progressCallback) progressCallback();

    cv::Mat fusedGray8;
    if (!FSFusionWavelet::inverse(mergedWavelet, opt.useOpenCL, fusedGray8))
    {
        if (G::FSLog) G::log(srcFun, "FSFusionWavelet::inverse failed");
        return false;
    }

    if (abortFlag && abortFlag->load(std::memory_order_relaxed)) return false;

    // --------------------------------------------------------------------
    // 4. Build color map using padded grayscale + padded RGB images
    // --------------------------------------------------------------------
    msg = "Build color map";
    if (G::FSLog) G::log(srcFun, msg);
    if (statusCallback) statusCallback(msg);
    if (progressCallback) progressCallback();

    std::vector<FSFusionReassign::ColorEntry> colorEntries;
    std::vector<uint8_t> counts;

    if (!FSFusionReassign::buildColorMap(grayP,
                                         colorP,
                                         colorEntries,
                                         counts))
    {
        QString msg ="FSFusionReassign::buildColorMap failed";
        if (G::FSLog) G::log(srcFun, msg);
        qWarning() << "WARNING:" << srcFun << msg;
        return false;
    }

    if (abortFlag && abortFlag->load(std::memory_order_relaxed)) return false;

    // --------------------------------------------------------------------
    // 5. Apply color reassignment to padded fused grayscale
    // --------------------------------------------------------------------
    msg = "Apply color reassignment";
    if (G::FSLog) G::log(srcFun, msg);
    if (statusCallback) statusCallback(msg);
    if (progressCallback) progressCallback();

    cv::Mat paddedColorOut;
    if (!FSFusionReassign::applyColorMap(fusedGray8,
                                         colorEntries,
                                         counts,
                                         paddedColorOut))
    {
        QString msg = "FSFusionReassign::applyColorMap failed";
        if (G::FSLog) G::log(srcFun, msg);
        qWarning() << "WARNING:" << srcFun << msg;
        return false;
    }

    if (abortFlag && abortFlag->load(std::memory_order_relaxed)) return false;

    // --------------------------------------------------------------------
    // 6. Crop back to original (non-padded) size
    // --------------------------------------------------------------------
    if (G::FSLog) G::log(srcFun, "Crop back to original size");

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
                         cv::Mat                    &depthIndex16,
                         cv::Mat                    &outputColor8,
                         std::atomic_bool           *abortFlag,
                         StatusCallback              statusCallback,
                         ProgressCallback            progressCallback)
{
    QString srcFun = "FSFusion::fuseStack";


    const QString method = opt.method.trimmed();
    if (G::FSLog) G::log(srcFun, "Method = " + method);

    if (method == "Simple")     // uses depthIndex16
    {
        return fuseSimple(grayImgs,
                          colorImgs,
                          depthIndex16,
                          outputColor8,
                          abortFlag,
                          progressCallback);
    }

    // Default: full PMax fusion
    if (method == "FullWaveletMerge")       // full wavelet merge
    return fusePMax(grayImgs,
                    colorImgs,
                    opt,
                    depthIndex16,
                    outputColor8,
                    abortFlag,
                    statusCallback,
                    progressCallback);

    qWarning() << "WARNING:" << srcFun << "Invalid method =" << method;
    return false;
}
