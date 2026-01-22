#include "fsfusion.h"
#include "Main/global.h"

#include "fsfusionwavelet.h"
#include "fsutilities.h"

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

static cv::Mat cropPadToOrig(const cv::Mat &pad,
                             const cv::Rect &roiPadToAlign,
                             const cv::Rect &validAreaAlign)
{
    CV_Assert(!pad.empty());
    CV_Assert(roiPadToAlign.width  > 0);
    CV_Assert(validAreaAlign.width > 0);

    cv::Mat align = pad(roiPadToAlign);
    cv::Mat orig  = align(validAreaAlign);
    return orig.clone();
}

static void dumpDepthErosionDebugCropped(
    const QString &basePath,
    const cv::Mat &fusedGray8_padded,
    const cv::Mat &edgeMask8U_padded,
    const cv::Mat &winnerBefore16_padded,
    const cv::Mat &winnerAfter16_padded,
    const cv::Rect &roiPadToAlign,
    const cv::Rect &validAreaAlign)
{
    QString srcFun = "FSFusion::dumpDepthErosionDebugCropped";
    if (G::FSLog) G::log(srcFun, basePath);

    CV_Assert(fusedGray8_padded.type() == CV_8U);
    CV_Assert(edgeMask8U_padded.type() == CV_8U);
    CV_Assert(winnerBefore16_padded.type() == CV_16U);
    CV_Assert(winnerAfter16_padded.type() == CV_16U);

    // ------------------------------------------------------------
    // Crop EVERYTHING to origSize
    // ------------------------------------------------------------
    cv::Mat fusedGray8   = cropPadToOrig(fusedGray8_padded,   roiPadToAlign, validAreaAlign);
    cv::Mat edgeMask8U   = cropPadToOrig(edgeMask8U_padded,   roiPadToAlign, validAreaAlign);
    cv::Mat before16     = cropPadToOrig(winnerBefore16_padded, roiPadToAlign, validAreaAlign);
    cv::Mat after16      = cropPadToOrig(winnerAfter16_padded,  roiPadToAlign, validAreaAlign);

    // ------------------------------------------------------------
    // Edge mask
    // ------------------------------------------------------------
    cv::imwrite((basePath + "/edgeMask.png").toStdString(), edgeMask8U);

    // ------------------------------------------------------------
    // Depth maps (normalized)
    // ------------------------------------------------------------
    double minV = 0.0, maxV = 0.0;
    cv::minMaxLoc(before16, &minV, &maxV);
    if (maxV <= minV) maxV = minV + 1.0;

    cv::Mat before8, after8;
    before16.convertTo(before8, CV_8U,
                       255.0 / (maxV - minV),
                       -minV * 255.0 / (maxV - minV));
    after16.convertTo(after8, CV_8U,
                      255.0 / (maxV - minV),
                      -minV * 255.0 / (maxV - minV));

    cv::imwrite((basePath + "/depthBefore.png").toStdString(), before8);
    cv::imwrite((basePath + "/depthAfter.png").toStdString(),  after8);

    // ------------------------------------------------------------
    // Delta map
    // ------------------------------------------------------------
    cv::Mat delta32;
    after16.convertTo(delta32, CV_32F);
    cv::Mat before32;
    before16.convertTo(before32, CV_32F);
    delta32 -= before32;

    cv::min(delta32,  10.0f, delta32);
    cv::max(delta32, -10.0f, delta32);

    cv::Mat delta8;
    delta32.convertTo(delta8, CV_8U, 255.0 / 20.0, 128.0);
    cv::imwrite((basePath + "/depthDelta.png").toStdString(), delta8);

    // ------------------------------------------------------------
    // Changed mask
    // ------------------------------------------------------------
    cv::Mat changedMask = (before16 != after16);
    changedMask.convertTo(changedMask, CV_8U, 255.0);
    cv::imwrite((basePath + "/changedMask.png").toStdString(), changedMask);

    // ------------------------------------------------------------
    // Overlays on fused grayscale
    // ------------------------------------------------------------
    cv::Mat fusedBGR;
    cv::cvtColor(fusedGray8, fusedBGR, cv::COLOR_GRAY2BGR);

    // Red = changed
    {
        cv::Mat overlay = fusedBGR.clone();
        overlay.setTo(cv::Scalar(0,0,255), changedMask);
        cv::Mat out;
        cv::addWeighted(fusedBGR, 0.65, overlay, 0.35, 0.0, out);
        cv::imwrite((basePath + "/fusedGray_overlayChanged.png").toStdString(), out);
    }

    // Yellow = edge band
    {
        cv::Mat overlay = fusedBGR.clone();
        overlay.setTo(cv::Scalar(0,255,255), edgeMask8U);
        cv::Mat out;
        cv::addWeighted(fusedBGR, 0.70, overlay, 0.30, 0.0, out);
        cv::imwrite((basePath + "/fusedGray_overlayEdge.png").toStdString(), out);
    }

    // Red = changed inside edge
    {
        cv::Mat changedInEdge;
        cv::bitwise_and(changedMask, edgeMask8U, changedInEdge);

        cv::Mat overlay = fusedBGR.clone();
        overlay.setTo(cv::Scalar(0,0,255), changedInEdge);
        cv::Mat out;
        cv::addWeighted(fusedBGR, 0.65, overlay, 0.35, 0.0, out);
        cv::imwrite((basePath + "/fusedGray_overlayChangedInsideEdge.png").toStdString(), out);
    }
}

static void dumpDepthErosionDebugOrigSize(const QString &basePath,
                                          const cv::Mat &fusedGrayPadded8,     // CV_8U padSize
                                          const cv::Mat &edgeMaskPadded8,      // CV_8U padSize
                                          const cv::Mat &winnerBeforePadded16, // CV_16U padSize
                                          const cv::Mat &winnerAfterPadded16,  // CV_16U padSize
                                          const cv::Rect &roiPadToAlign,       // pad -> align ROI
                                          const cv::Rect &validAreaAlign)      // align -> orig ROI
{
    QString srcFun = "FSFusion::dumpDepthErosionDebugOrigSize";
    if (G::FSLog) G::log(srcFun, basePath);

    CV_Assert(fusedGrayPadded8.type() == CV_8U);
    CV_Assert(edgeMaskPadded8.type() == CV_8U);
    CV_Assert(winnerBeforePadded16.type() == CV_16U);
    CV_Assert(winnerAfterPadded16.type() == CV_16U);

    CV_Assert(fusedGrayPadded8.size() == edgeMaskPadded8.size());
    CV_Assert(fusedGrayPadded8.size() == winnerBeforePadded16.size());
    CV_Assert(fusedGrayPadded8.size() == winnerAfterPadded16.size());

    // -----------------------------
    // Build masks at padded size
    // -----------------------------
    cv::Mat changedMaskPadded = (winnerAfterPadded16 != winnerBeforePadded16);
    changedMaskPadded.convertTo(changedMaskPadded, CV_8U, 255.0);

    cv::Mat edgeMaskBinPadded = (edgeMaskPadded8 != 0);

    cv::Mat changedInsideEdgePadded;
    cv::bitwise_and(changedMaskPadded, edgeMaskPadded8, changedInsideEdgePadded);

    cv::Mat changedOutsideEdgePadded;
    {
        cv::Mat notEdge;
        cv::bitwise_not(edgeMaskPadded8, notEdge);
        cv::bitwise_and(changedMaskPadded, notEdge, changedOutsideEdgePadded);
    }

    int changedTotal   = cv::countNonZero(changedMaskPadded);
    int changedInside  = cv::countNonZero(changedInsideEdgePadded);
    int changedOutside = cv::countNonZero(changedOutsideEdgePadded);

    if (G::FSLog) {
        G::log(srcFun, "changedTotal=" + QString::number(changedTotal) +
                       " changedInsideEdge=" + QString::number(changedInside) +
                       " changedOutsideEdge=" + QString::number(changedOutside));
    }

    // -----------------------------
    // Crop: pad -> align -> orig
    // -----------------------------
    auto cropToOrig = [&](const cv::Mat &padded) -> cv::Mat
    {
        CV_Assert(padded.size() == fusedGrayPadded8.size());
        cv::Mat align = padded(roiPadToAlign);
        cv::Mat orig  = align(validAreaAlign);
        return orig.clone();
    };

    cv::Mat fusedOrig8   = cropToOrig(fusedGrayPadded8);
    cv::Mat edgeOrig8    = cropToOrig(edgeMaskPadded8);
    cv::Mat changedOrig8 = cropToOrig(changedMaskPadded);
    cv::Mat outsideOrig8 = cropToOrig(changedOutsideEdgePadded);

    // -----------------------------
    // Build overlay: gray -> BGR with red changed pixels
    // -----------------------------
    cv::Mat overlayBGR;
    cv::cvtColor(fusedOrig8, overlayBGR, cv::COLOR_GRAY2BGR);

    // Make red overlay on changed pixels (BGR: (0,0,255))
    overlayBGR.setTo(cv::Scalar(0, 0, 255), changedOrig8);

    // -----------------------------
    // Write PNGs (origSize!)
    // -----------------------------
    cv::imwrite((basePath + "/dbe_fusedGray_orig.png").toStdString(), fusedOrig8);
    cv::imwrite((basePath + "/dbe_edgeMask_orig.png").toStdString(), edgeOrig8);
    cv::imwrite((basePath + "/dbe_changedMask_orig.png").toStdString(), changedOrig8);
    cv::imwrite((basePath + "/dbe_changedOutsideEdge_orig.png").toStdString(), outsideOrig8);
    cv::imwrite((basePath + "/dbe_overlay_orig.png").toStdString(), overlayBGR);
}

static cv::Mat makeEdgeMask8U(const cv::Mat &fusedGray8,
                              float sigma,
                              float threshFrac,
                              int dilatePx)
{
/*
    Used by DepthBiasedErosion
*/
    QString srcFun = "FSFusion::makeEdgeMask8U";
    if (G::FSLog) G::log(srcFun);

    CV_Assert(fusedGray8.type() == CV_8U);

    cv::Mat gx, gy;
    cv::Sobel(fusedGray8, gx, CV_32F, 1, 0, 3);
    cv::Sobel(fusedGray8, gy, CV_32F, 0, 1, 3);

    cv::Mat mag;
    cv::magnitude(gx, gy, mag);

    if (sigma > 0.0f)
    {
        int k = int(sigma * 4.0f) + 1;
        if ((k & 1) == 0) ++k;
        if (k < 3) k = 3;
        cv::GaussianBlur(mag, mag, cv::Size(k, k), sigma, sigma,
                         cv::BORDER_REFLECT);
    }

    double minV = 0.0, maxV = 0.0;
    cv::minMaxLoc(mag, &minV, &maxV);
    float thr = float(maxV) * threshFrac;

    cv::Mat mask;
    cv::threshold(mag, mask, thr, 255.0, cv::THRESH_BINARY);
    mask.convertTo(mask, CV_8U);

    if (dilatePx > 0)
    {
        int k = 2 * dilatePx + 1;
        cv::Mat se = cv::getStructuringElement(cv::MORPH_ELLIPSE,
                                               cv::Size(k, k));
        cv::dilate(mask, mask, se);
    }

    return mask;
}

static cv::Mat depthBiasedErode16(const cv::Mat &winner16,
                                  const cv::Mat &edgeMask8U,
                                  int radius,
                                  int iters,
                                  int maxDelta,
                                  float minEdgeDelta)
{
/*
    Used by DepthBiasedErosion
*/
    QString srcFun = "FSFusion::depthBiasedErode16";
    if (G::FSLog) G::log(srcFun);

    CV_Assert(winner16.type() == CV_16U);
    CV_Assert(edgeMask8U.type() == CV_8U);
    CV_Assert(winner16.size() == edgeMask8U.size());

    cv::Mat cur = winner16.clone();

    int k = 2 * radius + 1;
    cv::Mat se = cv::getStructuringElement(cv::MORPH_ELLIPSE,
                                           cv::Size(k, k));

    for (int i = 0; i < iters; ++i)
    {
        cv::Mat localMax, localMin;
        cv::dilate(cur, localMax, se);
        cv::erode(cur,  localMin, se);

        // Only act where:
        //  - in edge band
        //  - there is meaningful local depth contrast
        cv::Mat contrast32;
        {
            cv::Mat max32, min32;
            localMax.convertTo(max32, CV_32F);
            localMin.convertTo(min32, CV_32F);
            contrast32 = max32 - min32;
        }

        cv::Mat actMask = (edgeMask8U != 0) & (contrast32 >= minEdgeDelta);
        actMask.convertTo(actMask, CV_8U); // ensure 0/255
        cv::bitwise_and(actMask, edgeMask8U, actMask);

        // target = localMax (deeper)
        // cur = min(cur + maxDelta, target) in actMask
        cv::Mat cur32, tgt32;
        cur.convertTo(cur32, CV_32F);
        localMax.convertTo(tgt32, CV_32F);

        cv::Mat raised = cur32 + float(maxDelta);
        cv::min(raised, tgt32, raised);

        // write back only in mask
        cv::Mat raised16;
        raised.convertTo(raised16, CV_16U);
        raised16.copyTo(cur, actMask);
    }

    return cur;
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

    if (cb) cb();
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

    int outDepth = colorImgs.at(0).depth();

    const int N = static_cast<int>(grayImgs.size());
    if (N == 0 || N != static_cast<int>(colorImgs.size()))
        return false;

    // Validate sizes and types
    const cv::Size orig = grayImgs[0].size();
    for (int i = 0; i < N; ++i)
    {
        if (grayImgs[i].empty() || colorImgs[i].empty())
            return false;
        if (grayImgs[i].size() != orig || colorImgs[i].size() != orig)
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

    if (!FSMerge::merge(wavelets,
                        opt.consistency,
                        abortFlag,
                        mergedWavelet,
                        depthIndex16))
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
    FSFusionReassign::ColorDepth cd = (outDepth == CV_16U)
           ? FSFusionReassign::ColorDepth::U16
           : FSFusionReassign::ColorDepth::U8;
    if (!FSFusionReassign::applyColorMap(fusedGray8,
                                         colorEntries,
                                         counts,
                                         paddedColorOut,
                                         cd))
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

} // end anonymous namespace

static inline FSFusionReassign::ColorDepth reassignDepth(int depth)
{
    return (depth == CV_16U)
        ? FSFusionReassign::ColorDepth::U16
        : FSFusionReassign::ColorDepth::U8;
}

// Depth-Biased Erosion
// Purpose:
//   Second-pass color override after Depth-Biased Erosion changed the winner map.
//   Only override *edge-band* pixels whose winner index changed, and only when the
//   replacement slice actually contains non-trivial content (to avoid “black branches”).
//
// Key safety guards added:
//   1) Only operate on pixels where (edgeMask != 0) AND (winnerAfter != winnerBefore)
//   2) Per-slice mask is restricted to those changed pixels.
//   3) “Content check”: don’t override with a slice that is essentially background
//      at those pixels (counts luminance > threshold within mask). If too few “valid”
//      pixels, skip that slice’s override.
//   4) Apply contrast/WB in 8-bit space (FSAlign implementation is 8U).
//   5) Optional clamp of winner indices to [0, N-1] before comparing.
//   6) Optional debug: log per-slice mask size and accepted pixels.
//
// Call signature stays compatible with your current call site, BUT you should pass
// the winnerBefore and winnerAfter maps (recommended). If you can’t, see the overload
// note at the bottom.

bool FSFusion::applyDepthBiasedColorOverrideSecondPass(
    cv::Mat &paddedColorOut,
    const cv::Mat &winnerBeforePadded16,
    const cv::Mat &winnerAfterPadded16,
    const cv::Mat &edgeMask8U,
    const QStringList &inputPaths,
    const std::vector<FSAlign::Result> &globals,
    const Options &opt,
    std::atomic_bool *abortFlag
    )
{
    QString srcFun = "FSFusion::applyDepthBiasedColorOverrideSecondPass";
    if (G::FSLog) G::log(srcFun);

    // -----------------------------
    // Validate
    // -----------------------------
    CV_Assert(paddedColorOut.type() == CV_8UC3 || paddedColorOut.type() == CV_16UC3);
    CV_Assert(winnerAfterPadded16.type() == CV_16U);
    CV_Assert(edgeMask8U.type() == CV_8U);
    CV_Assert(winnerBeforePadded16.empty() || winnerBeforePadded16.type() == CV_16U);

    CV_Assert(paddedColorOut.size() == winnerAfterPadded16.size());
    CV_Assert(paddedColorOut.size() == edgeMask8U.size());
    if (!winnerBeforePadded16.empty())
        CV_Assert(winnerBeforePadded16.size() == winnerAfterPadded16.size());

    const int N = int(inputPaths.size());
    if (N <= 0 || int(globals.size()) != N)
        return false;

    // -----------------------------
    // Build act mask:
    //   - Must be in edge band
    //   - Must be a changed pixel (winnerAfter != winnerBefore)
    // -----------------------------
    cv::Mat edgeMask = (edgeMask8U != 0);
    edgeMask.convertTo(edgeMask, CV_8U, 255.0);

    if (cv::countNonZero(edgeMask) == 0) {
        if (G::FSLog) G::log(srcFun, "No edge pixels; skipping override");
        return true;
    }

    cv::Mat changedMask;
    if (!winnerBeforePadded16.empty())
    {
        changedMask = (winnerAfterPadded16 != winnerBeforePadded16);
        changedMask.convertTo(changedMask, CV_8U, 255.0);
    }
    else
    {
        // If you cannot supply winnerBefore, we can’t know what changed.
        // Fallback: operate on all edge pixels (less safe; can cause black branches).
        // Strongly recommended to pass winnerBeforePadded16.
        changedMask = edgeMask.clone();
        if (G::FSLog) G::log(srcFun, "WARNING: winnerBefore not provided; using edgeMask as changedMask");
    }

    cv::Mat actMask;
    cv::bitwise_and(edgeMask, changedMask, actMask);

    const int actCount = cv::countNonZero(actMask);
    if (actCount == 0) {
        if (G::FSLog) G::log(srcFun, "No changed edge pixels; skipping override");
        return true;
    }

    if (G::FSLog) {
        G::log(srcFun, "Act pixels (changed in edge band) = " + QString::number(actCount));
    }

    // -----------------------------
    // Optional: clamp winners to [0..N-1] for safety
    // (Rare, but protects against any out-of-range erosion artifacts.)
    // -----------------------------
    cv::Mat winnerClamped = winnerAfterPadded16;
    {
        // Only if we detect any out-of-range; keep fast path otherwise.
        double mn=0, mx=0;
        cv::minMaxLoc(winnerAfterPadded16, &mn, &mx);
        if (mn < 0.0 || mx > double(N - 1))
        {
            winnerClamped = winnerAfterPadded16.clone();
            for (int y = 0; y < winnerClamped.rows; ++y)
            {
                uint16_t *p = winnerClamped.ptr<uint16_t>(y);
                for (int x = 0; x < winnerClamped.cols; ++x)
                {
                    int v = int(p[x]);
                    if (v < 0) v = 0;
                    if (v > N - 1) v = N - 1;
                    p[x] = uint16_t(v);
                }
            }
            if (G::FSLog) G::log(srcFun, "Clamped winner indices to [0..N-1]");
        }
    }

    // -----------------------------
    // Content check thresholds
    // -----------------------------
    // We will reject “background/black” overrides by checking luminance on the candidate slice.
    //
    // For 16U: threshold ~ 1% (655) is conservative.
    // For 8U: threshold ~ 3 is conservative.
    //
    // You can expose these in Options later; for now keep fixed.
    // const int    minValidPixels = 256; // minimum pixels that pass luminance check for a slice override to be applied
    // const double lumThresh8  = 3.0;
    // const double lumThresh16 = 655.0;
    const int    minValidPixels = 1;   // instead of 256
    const double lumThresh8  = 0.0;    // instead of 3.0
    const double lumThresh16 = 0.0;    // instead of 655.0
    // -----------------------------
    // Helper: compute luminance (8U) for a BGR image
    // -----------------------------
    auto computeLuma8U = [&](const cv::Mat &bgr8, cv::Mat &luma8)
    {
        CV_Assert(bgr8.type() == CV_8UC3);
        std::vector<cv::Mat> ch(3);
        cv::split(bgr8, ch); // B,G,R

        // Use integer-ish weights in float then convert back:
        // Y ≈ 0.114B + 0.587G + 0.299R
        cv::Mat y32 = 0.114f * ch[0] + 0.587f * ch[1] + 0.299f * ch[2];
        y32.convertTo(luma8, CV_8U);
    };

    // -----------------------------
    // For each slice, override only pixels that want that slice AND are “safe”
    // -----------------------------
    for (int i = 0; i < N; ++i)
    {
        if (abortFlag && abortFlag->load(std::memory_order_relaxed)) return false;

        // maskI = (winner == i) & actMask
        cv::Mat maskI = (winnerClamped == uint16_t(i));
        maskI.convertTo(maskI, CV_8U, 255.0);
        cv::bitwise_and(maskI, actMask, maskI);

        const int wanted = cv::countNonZero(maskI);
        if (wanted == 0)
            continue;

        if (G::FSLog) {
            G::log(srcFun, "Slice " + QString::number(i) +
                               " wanted pixels (pre-check) = " + QString::number(wanted));
        }

        // Load original slice
        FSLoader::Image img = FSLoader::load(inputPaths[i].toStdString());

        // Warp to aligned space using global transform
        cv::Mat alignedColor;
        FSAlign::applyTransform(img.color, globals[i].transform, alignedColor);

        // Apply contrast/WB in 8-bit space
        cv::Mat aligned8;
        if (alignedColor.depth() == CV_16U)
            alignedColor.convertTo(aligned8, CV_8UC3, 255.0 / 65535.0);
        else
            aligned8 = alignedColor; // view

        FSAlign::applyContrastWhiteBalance(aligned8, globals[i]); // 8U safe

        // Convert back to output depth (to match paddedColorOut)
        cv::Mat alignedCorrected;
        if (paddedColorOut.depth() == CV_16U)
            aligned8.convertTo(alignedCorrected, CV_16UC3, 65535.0 / 255.0);
        else
            alignedCorrected = aligned8; // 8U

        // Pad to wavelet padSize (must match pad used in fusion)
        cv::Size paddedSizeDummy;
        cv::Mat colorP = padForWavelet(alignedCorrected, paddedSizeDummy);

        if (colorP.size() != paddedColorOut.size()) {
            qWarning() << "WARNING:" << srcFun << "colorP.size != padSize for slice" << i
                       << " got=" ;//<< colorP.size() << " expected=" << paddedColorOut.size();
            return false;
        }

        // Ensure type matches output
        if (colorP.type() != paddedColorOut.type())
        {
            if (paddedColorOut.type() == CV_8UC3 && colorP.type() == CV_16UC3)
                colorP.convertTo(colorP, CV_8UC3, 255.0 / 65535.0);
            else if (paddedColorOut.type() == CV_16UC3 && colorP.type() == CV_8UC3)
                colorP.convertTo(colorP, CV_16UC3, 65535.0 / 255.0);
            else {
                qWarning() << "WARNING:" << srcFun << "unsupported color type conversion";
                return false;
            }
        }

        // -----------------------------
        // Content check:
        //   Keep only pixels where candidate slice is “not black”
        // -----------------------------
        cv::Mat validMask = maskI.clone(); // will be reduced

        if (paddedColorOut.depth() == CV_16U)
        {
            // Compute simple luminance proxy: max(B,G,R) is cheap & robust in 16U
            // valid if maxChan > lumThresh16
            std::vector<cv::Mat> ch(3);
            cv::split(colorP, ch);

            cv::Mat max01, maxAll;
            cv::max(ch[0], ch[1], max01);
            cv::max(max01, ch[2], maxAll);           // CV_16U

            cv::Mat bright = (maxAll > uint16_t(lumThresh16));
            bright.convertTo(bright, CV_8U, 255.0);

            cv::bitwise_and(validMask, bright, validMask);
        }
        else
        {
            // 8U: compute luma and require luma > lumThresh8
            cv::Mat luma8;
            computeLuma8U(colorP, luma8);
            cv::Mat bright = (luma8 > uint8_t(lumThresh8));
            bright.convertTo(bright, CV_8U, 255.0);

            cv::bitwise_and(validMask, bright, validMask);
        }

        const int accepted = cv::countNonZero(validMask);
        if (accepted < minValidPixels)
        {
            // Too few pixels look non-black in this slice; skip override.
            if (G::FSLog) {
                G::log(srcFun, "Slice " + QString::number(i) +
                                   " skipped (accepted=" + QString::number(accepted) +
                                   " < " + QString::number(minValidPixels) + ")");
            }
            continue;
        }

        // -----------------------------
        // Apply override (only validMask pixels)
        // -----------------------------
        colorP.copyTo(paddedColorOut, validMask);

        if (G::FSLog) {
            G::log(srcFun, "Slice " + QString::number(i) +
                               " override applied accepted=" + QString::number(accepted));
        }
    }

    return true;
}

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

// StreamPMax pipeline
bool FSFusion::streamPMaxSlice(int slice,
                               const cv::Mat      &grayImg,
                               const cv::Mat      &colorImg,
                               const Options      &opt,
                               std::atomic_bool   *abortFlag,
                               StatusCallback     statusCallback,
                               ProgressCallback   progressCallback
                               )
{
    QString srcFun = "FSFusion::fusePMaxSlice";
    QString s = QString::number(slice);
    QString msg = "Fusing slice " + s;
    msg += "  method: " + opt.method;
    msg += "  mergeMode: " + opt.mergeMode;
    msg += "  winnerMap: " + opt.winnerMap;
    if (G::FSLog) G::log(srcFun, msg);

    msg = "DepthBiasedErosion: enableDepthBiasedErosion = " +
          QVariant(opt.enableDepthBiasedErosion).toString();
    if (G::FSLog) G::log(srcFun, msg);
    // if (statusCallback) statusCallback(msg);

    // Validate sizes and types
    if (grayImg.empty() || colorImg.empty()) {
        QString msg = "Slice " + s + " grayImg.empty() || colorImg.empty()";
        qWarning() << "WARNING:" << srcFun << msg;
        return false;
    }

    if (slice == 0) {
        alignSize = grayImg.size();
        outDepth = colorImg.depth();
    }
    else if (grayImg.size() != alignSize) {
        QString msg = "Slice " + s + " grayImg.size() != orig";
        qWarning() << "WARNING:" << srcFun << msg;
        return false;
    }
    else if (colorImg.size() != alignSize) {
        QString msg = "Slice " + s + " colorImg.size() != orig";
        qWarning() << "WARNING:" << srcFun << msg;
        return false;
    }
    else if (colorImg.depth() != outDepth) {
        qWarning() << "WARNING:" << srcFun << "Color depth changed across slices";
        return false;
    }

    // --------------------------------------------------------------------
    // Pad grayscale + color images BEFORE processing (wavelet-friendly)
    // --------------------------------------------------------------------
    if (G::FSLog) G::log(srcFun, "Pad for wavelet");

    cv::Mat grayP;
    cv::Mat colorP;
    cv::Size paddedSize;

    if (abortFlag && abortFlag->load(std::memory_order_relaxed)) return false;

    grayP  = padForWavelet(grayImg, paddedSize);
    colorP = padForWavelet(colorImg, paddedSize);

    // Lock padded size on slice 0; enforce identical thereafter
    if (slice == 0)
        padSize = paddedSize;
    else if (paddedSize != padSize)
    {
        QString msg = "Slice " + s + " paddedSize != ps";
        qWarning() << "WARNING:" << srcFun << msg;
        return false;
    }

    // Sanity Check
    // /*
    if (slice > 0 && paddedSize != padSize)
    {
        QString m = QString("Sanity: slice %1 paddedSize %2x%3 != ps %4x%5")
            .arg(slice)
            .arg(paddedSize.width).arg(paddedSize.height)
            .arg(padSize.width).arg(padSize.height);
        if (G::FSLog) G::log(srcFun, m);
        qWarning().noquote() << "WARNING:" << srcFun << m;
        return false;
    }
    //*/

    // Init builders/state on slice 0 (once)
    if (slice == 0)
    {
        colorBuilder.begin(grayP.size(), /*fixedCapPerPixel=*/4);
        mergeState.reset();
        mergedWavelet.release();
        wavelet.release();
    }

    // --------------------------------------------------------------------
    // Forward wavelet per slice (grayscale only)
    // --------------------------------------------------------------------

    if (abortFlag && abortFlag->load(std::memory_order_relaxed)) return false;

    msg = "Forward wavelet slice " + s;
    if (G::FSLog) G::log(srcFun, msg);
    // if (statusCallback) statusCallback(msg);
    // if (progressCallback) progressCallback();

    if (!FSFusionWavelet::forward(grayP, opt.useOpenCL, wavelet))
    {
        QString msg = "Slice " + s + " Wavelet forward failed";
        if (G::FSLog) G::log(srcFun, msg);
        qWarning() << "WARNING:" << srcFun << msg;
        return false;
    }

    // Lock wavelet size on slice 0; enforce identical thereafter
    if (slice == 0)
        waveletSize = wavelet.size();
    // Sanity Check
    // /*
    if (slice > 0 && wavelet.size() != waveletSize)
    {
        QString m = QString("Sanity: slice %1 wavelet.size %2x%3 != waveletSize %4x%5")
        .arg(slice)
            .arg(wavelet.cols).arg(wavelet.rows)
            .arg(waveletSize.width).arg(waveletSize.height);
        if (G::FSLog) G::log(srcFun, m);
        qWarning().noquote() << "WARNING:" << srcFun << m;
        return false;
    }

    auto matInfo = [](const cv::Mat& m) -> QString {
        return QString("size=%1x%2 type=%3 channels=%4 step=%5")
        .arg(m.cols).arg(m.rows)
            .arg(m.type())
            .arg(m.channels())
            .arg(static_cast<qulonglong>(m.step));
    };

    if (slice == 0)
    {
        QString msg =
            "Sanity(slice0): "
            "orig=" + QString("%1x%2").arg(alignSize.width).arg(alignSize.height) + " "
            "ps="   + QString("%1x%2").arg(padSize.width).arg(padSize.height) + " "
            "waveletSize=" +
            QString("%1x%2").arg(waveletSize.width).arg(waveletSize.height) +
            " | "
            "colorImg(" + matInfo(colorImg) + ") "
            "grayP("    + matInfo(grayP)    + ") "
            "colorP("   + matInfo(colorP)   + ") "
            "wavelet("  + matInfo(wavelet) + ")";

        if (G::FSLog) G::log(srcFun, msg);
        // qDebug().noquote() << srcFun << log;
    }
    //*/

    if (wavelet.size() != waveletSize)
    {
        QString msg = "Slice " + s + " wavelet.size() != waveletSize";
        qWarning() << "WARNING:" << srcFun << msg;
        return false;
    }

    // --------------------------------------------------------------------
    // Merge wavelet stacks → mergedWavelet (we ignore depthIndex here)
    // --------------------------------------------------------------------
    msg = "Merge wavelet stacks.  mergeMode: " + opt.mergeMode;
    if (G::FSLog) G::log(srcFun, msg);
    // if (statusCallback) statusCallback(msg);
    // if (progressCallback) progressCallback();

    if (opt.mergeMode == "PMax") {
        if (!FSMerge::mergeSlice(mergeState,
                                 wavelet,
                                 waveletSize,
                                 opt.consistency,
                                 abortFlag,
                                 mergedWavelet))
        {
            QString msg = "Slice " + s + " FSMerge::mergeSlice PMax failed.";
            qWarning() << "WARNING:" << srcFun << msg;
            if (G::FSLog) G::log(srcFun, msg);
            return false;
        }
    }
    else if (opt.mergeMode == "Weighted") {
        FSMerge::WeightedParams wp;
        wp.power = opt.weightedPower;
        wp.sigma0 = opt.weightedSigma0;
        wp.includeLowpass = opt.weightedIncludeLowpass;
        wp.epsEnergy = opt.weightedEpsEnergy;
        wp.epsWeight = opt.weightedEpsWeight;
        if (!FSMerge::mergeSliceWeighted(mergeState,
                                         wavelet,
                                         waveletSize,
                                         wp,
                                         opt.consistency,
                                         abortFlag,
                                         mergedWavelet))
        {
            QString msg = "Slice " + s + " FSMerge::mergeSliceWeighted Weighted failed.";
            qWarning() << "WARNING:" << srcFun << msg;
            if (G::FSLog) G::log(srcFun, msg);
            return false;
        }
    }

    // --------------------------------------------------------------------
    // Build color map using padded grayscale + padded RGB images
    // --------------------------------------------------------------------
    msg = "Build color map";
    if (G::FSLog) G::log(srcFun, msg);
    // if (statusCallback) statusCallback(msg);

    FSFusionReassign::ColorEntry colorEntry;
    if (!colorBuilder.addSlice(grayP, colorP))
    {
        QString msg = "Slice " + s + "FSFusionReassign::addSlice failed.";
        if (G::FSLog) G::log(srcFun, msg);
        qWarning() << "WARNING:" << srcFun << msg;
        return false;
    }

    if (abortFlag && abortFlag->load(std::memory_order_relaxed)) return false;

    // if (progressCallback) progressCallback();
    return true;
}

// StreamPMax pipeline
bool FSFusion::streamPMaxFinish(
    cv::Mat &outputColor,
    const Options &opt,
    cv::Mat &depthIndex16,
    const QStringList &inputPaths,
    const std::vector<FSAlign::Result> &globals,
    std::atomic_bool *abortFlag,
    StatusCallback statusCallback,
    ProgressCallback progressCallback)
{
    QString srcFun = "FSFusion::streamPMaxFinish";
    QString msg = "Finishing Fusion";
    msg = "DepthBiasedErosion: enableDepthBiasedErosion = " +
          QVariant(opt.enableDepthBiasedErosion).toString();
    if (statusCallback) statusCallback(msg);

    // --------------------------------------------------------------------
    // Finish merge after all slices processed and before invert
    // --------------------------------------------------------------------
    msg = "Finish merge after last slice";
    if (G::FSLog) G::log(srcFun, msg);
    // if (statusCallback) statusCallback(msg);
    // if (progressCallback) progressCallback();

    if (opt.mergeMode == "PMax") {
        if (!FSMerge::mergeSliceFinish(mergeState,
                                       opt.consistency,
                                       abortFlag,
                                       mergedWavelet,
                                       depthIndexPadded16))
        {
            return false;
        }
    }
    else if (opt.mergeMode == "Weighted") {
        FSMerge::WeightedParams wp;
        wp.power = opt.weightedPower;
        wp.sigma0 = opt.weightedSigma0;
        wp.includeLowpass = opt.weightedIncludeLowpass;
        wp.epsEnergy = opt.weightedEpsEnergy;
        wp.epsWeight = opt.weightedEpsWeight;

        cv::Mat weightedWinnerPadded16;
        cv::Mat energyWinnerPadded16;

        if (!FSMerge::mergeSliceFinishWeighted(mergeState,
                                               wp,
                                               opt.consistency,
                                               abortFlag,
                                               mergedWavelet,
                                               weightedWinnerPadded16,
                                               energyWinnerPadded16))
        {
            return false;
        }

        // Choose which map becomes "the" depthIndexPadded16 used downstream
        if (G::FSLog) G::log(srcFun, "winnerMap = " + opt.winnerMap);
        if (opt.winnerMap == "Energy")
            depthIndexPadded16 = energyWinnerPadded16;
        else
            depthIndexPadded16 = weightedWinnerPadded16;
    }

    if (abortFlag && abortFlag->load(std::memory_order_relaxed)) return false;

    // --------------------------------------------------------------------
    // Inverse wavelet → fusedGray (still padded size)
    // --------------------------------------------------------------------
    msg = "Inverse wavelet";
    if (G::FSLog) G::log(srcFun, msg);
    // if (statusCallback) statusCallback(msg);
    // if (progressCallback) progressCallback();

    // cv::Mat fusedGray; // local, then ensure CV_8U
    if (!FSFusionWavelet::inverse(mergedWavelet, opt.useOpenCL, fusedGray8))
    {
        if (G::FSLog) G::log(srcFun, "FSFusionWavelet::inverse failed");
        return false;
    }

    if (fusedGray8.type() != CV_8U) {
        msg = "Inverse wavelet: fusedGray8.type() != CV_8U";
        qWarning() << "WARNING:" << srcFun << msg;
        return false;
    }
    if (fusedGray8.size() != padSize) {
        msg = "Inverse wavelet: fusedGray8.size() != ps";
        qWarning() << "WARNING:" << srcFun << msg;
        return false;
    }

    if (abortFlag && abortFlag->load(std::memory_order_relaxed)) return false;

    // --------------------------------------------------------------------
    // Depth-Biased Erosion: refines the winner map
    // --------------------------------------------------------------------
    msg = "DepthBiasedErosion: enableDepthBiasedErosion = " +
          QVariant(opt.enableDepthBiasedErosion).toString();

    if (G::FSLog) G::log(srcFun, msg);
    cv::Mat erodedWinnerPadded16;
    cv::Mat winnerBefore;
    cv::Mat winnerAfter;
    cv::Mat edgeMask8U;

    if (opt.enableDepthBiasedErosion)
    {
        msg = "DepthBiasedErosion:"
            " erosionEdgeSigma = " + QVariant(opt.erosionEdgeSigma).toString() +
            " erosionEdgeThresh = " + QVariant(opt.erosionEdgeThresh).toString() +
            " erosionEdgeThresh = " + QVariant(opt.erosionEdgeDilate).toString()
            ;
        if (G::FSLog) G::log(srcFun, msg);
        winnerBefore = depthIndexPadded16.clone();

        edgeMask8U = makeEdgeMask8U(fusedGray8,
                                            opt.erosionEdgeSigma,
                                            opt.erosionEdgeThresh,
                                            opt.erosionEdgeDilate);

        winnerAfter = depthBiasedErode16(winnerBefore,
                                                 edgeMask8U,
                                                 opt.erosionRadius,
                                                 opt.erosionIters,
                                                 opt.erosionMaxDelta,
                                                 opt.erosionMinEdgeDelta);

        int changed = cv::countNonZero(winnerAfter != winnerBefore);
        msg = "DepthBiasedErosion changed pixels = " + QString::number(changed);
        if (G::FSLog) G::log(srcFun, msg);

        /* Write debug pngs
        dumpDepthErosionDebugCropped(
            opt.depthFolderPath,
            fusedGray8,                 // padded
            edgeMask8U,                 // padded
            winnerBefore,               // padded
            winnerAfter,                // padded
            roiPadToAlign,
            validAreaAlign
            );
        //*/

        depthIndexPadded16 = winnerAfter;
    }

    if (abortFlag && abortFlag->load(std::memory_order_relaxed)) return false;

    // --------------------------------------------------------------------
    // Finish color builder after all slices processed
    // --------------------------------------------------------------------
    msg = "Finish color builder";
    if (G::FSLog) G::log(srcFun, msg);
    // if (statusCallback) statusCallback(msg);
    // if (progressCallback) progressCallback();

    colorBuilder.finish(colorEntries, counts);

    if (abortFlag && abortFlag->load(std::memory_order_relaxed)) return false;

    // --------------------------------------------------------------------
    // Apply color reassignment to padded fused grayscale → paddedColorOut
    // --------------------------------------------------------------------
    msg = "Apply color reassignment";
    if (G::FSLog) G::log(srcFun, msg);
    // if (statusCallback) statusCallback(msg);
    // if (progressCallback) progressCallback();

    cv::Mat paddedColorOut;
    if (!FSFusionReassign::applyColorMap(fusedGray8, colorEntries,
                                         counts, paddedColorOut,
                                         reassignDepth(outDepth)))
    {
        msg = "FSFusionReassign::applyColorMap failed";
        if (G::FSLog) G::log(srcFun, msg);
        qWarning() << "WARNING:" << srcFun << msg;
        return false;
    }

    if (paddedColorOut.size() != padSize) {
        msg = "Apply color map: paddedColorOut.size() != ps";
        qWarning() << "WARNING:" << srcFun << msg;
        return false;
    }

    const int expectedType = (outDepth == CV_16U) ? CV_16UC3 : CV_8UC3;
    if (paddedColorOut.type() != expectedType) {
        msg = "Apply color map: paddedColorOut.type() mismatch";
        qWarning() << "WARNING:" << srcFun << msg
                   << " got =" << paddedColorOut.type()
                   << " expected=" << expectedType;
        return false;
    }

    if (abortFlag && abortFlag->load(std::memory_order_relaxed)) return false;

    // ------------------------------------------------------------
    // Apply Depth-Biased Erosion
    // ------------------------------------------------------------
    if (opt.enableDepthBiasedErosion)
    {
        // Recompute edge mask (or reuse the one you already computed earlier)
        cv::Mat edgeMask8U = makeEdgeMask8U(fusedGray8,
                                            opt.erosionEdgeSigma,
                                            opt.erosionEdgeThresh,
                                            opt.erosionEdgeDilate);

        // depthIndexPadded16 already replaced by winnerAfter earlier in your code
        if (!applyDepthBiasedColorOverrideSecondPass(
                paddedColorOut,
                winnerBefore,          // <-- before DBE
                depthIndexPadded16,    // <-- after DBE (you set it earlier)
                edgeMask8U,
                inputPaths,
                globals,
                opt,
                abortFlag))
        {
            qWarning() << "WARNING:" << srcFun << "applyDepthBiasedColorOverrideSecondPass failed";
            return false;
        }
    }

    // --------------------------------------------------------------------
    // Crop back to original (non-padded) size
    //   Step 1: padSize   -> alignSize
    //   Step 2: alignSize -> origSize (validAreaAlign)
    // --------------------------------------------------------------------
    msg = "Crop back to original size";
    msg += " origSize=" + FSUtilities::cvSizeToText(origSize) +
           " alignSize=" + FSUtilities::cvSizeToText(alignSize) +
           " padSize=" + FSUtilities::cvSizeToText(padSize) +
           " validAreaAlign=(" +
           QString::number(validAreaAlign.x) + "," +
           QString::number(validAreaAlign.y) + "," +
           QString::number(validAreaAlign.width) + "," +
           QString::number(validAreaAlign.height) + ")";
    if (G::FSLog) G::log(srcFun, msg);

    // Sanity: validAreaAlign must describe origSize inside alignSize
    if (validAreaAlign.width  != origSize.width ||
        validAreaAlign.height != origSize.height)
    {
        qWarning() << "WARNING:" << srcFun
                   << "validAreaAlign does not match origSize";
        return false;
    }

    // ------------------------------------------------------------
    // 1) Undo FSFusion padding: padSize -> alignSize
    // ------------------------------------------------------------
    cv::Rect roiPadToAlign(0, 0, alignSize.width, alignSize.height);

    if (padSize != alignSize)
    {
        const int padW = padSize.width  - alignSize.width;
        const int padH = padSize.height - alignSize.height;

        if (padW < 0 || padH < 0)
        {
            qWarning() << "WARNING:" << srcFun
                       << "padSize smaller than alignSize";
            return false;
        }

        const int left = padW / 2;
        const int top  = padH / 2;

        roiPadToAlign = cv::Rect(left, top,
                                 alignSize.width,
                                 alignSize.height);
    }

    // Bounds check
    if (roiPadToAlign.x < 0 || roiPadToAlign.y < 0 ||
        roiPadToAlign.x + roiPadToAlign.width  > paddedColorOut.cols ||
        roiPadToAlign.y + roiPadToAlign.height > paddedColorOut.rows ||
        roiPadToAlign.x + roiPadToAlign.width  > depthIndexPadded16.cols ||
        roiPadToAlign.y + roiPadToAlign.height > depthIndexPadded16.rows)
    {
        qWarning() << "WARNING:" << srcFun
                   << "roiPadToAlign out of bounds";
        return false;
    }

    // Views at alignSize
    cv::Mat colorAlign = paddedColorOut(roiPadToAlign);
    cv::Mat depthAlign = depthIndexPadded16(roiPadToAlign);

    // ------------------------------------------------------------
    // 2) Undo FSLoader padding: alignSize -> origSize
    // ------------------------------------------------------------
    if (validAreaAlign.x < 0 || validAreaAlign.y < 0 ||
        validAreaAlign.x + validAreaAlign.width  > colorAlign.cols ||
        validAreaAlign.y + validAreaAlign.height > colorAlign.rows ||
        validAreaAlign.x + validAreaAlign.width  > depthAlign.cols ||
        validAreaAlign.y + validAreaAlign.height > depthAlign.rows)
    {
        qWarning() << "WARNING:" << srcFun
                   << "validAreaAlign out of bounds";
        return false;
    }

    // Final outputs (origSize)
    outputColor  = colorAlign(validAreaAlign).clone();
    depthIndex16 = depthAlign(validAreaAlign).clone();

    // /* Write debug pngs
    if (opt.enableDepthBiasedErosion)
    {
        // NEW: origSize debug overlay (very useful)
        dumpDepthErosionDebugOrigSize(
            opt.depthFolderPath,
            fusedGray8,        // padded gray
            edgeMask8U,        // padded
            winnerBefore,      // padded
            winnerAfter,       // padded
            roiPadToAlign,     // pad -> align ROI
            validAreaAlign     // align -> orig ROI
            );

        dumpDepthErosionDebugCropped(
            opt.depthFolderPath,
            fusedGray8,                 // padded
            edgeMask8U,                 // padded
            winnerBefore,               // padded
            winnerAfter,                // padded
            roiPadToAlign,
            validAreaAlign
            );
    }
    //*/


    // ------------------------------------------------------------
    // Housekeeping
    // ------------------------------------------------------------
    colorEntries.clear();
    counts.clear();
    mergedWavelet.release();
    wavelet.release();
    mergeState.reset();         // also clears cached wavelets
    colorBuilder.reset();

    // if (progressCallback) progressCallback();

    return true;
}

// end StreamPMax pipeline

