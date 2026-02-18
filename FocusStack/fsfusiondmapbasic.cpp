#include "fsfusiondmapbasic.h"

#include "Main/global.h"
#include "fsloader.h"
#include "fsutilities.h"

// your shared helpers:
#include "fsfusiondmapshared.h"

#include <opencv2/imgcodecs.hpp>
#include <cmath>
#include <algorithm>

namespace
{

    // empty for now

} // end anon namespace

FSFusionDMapBasic::FSFusionDMapBasic()
{
    reset();
}

void FSFusionDMapBasic::reset()
{
    active_ = false;
    sliceCount_ = 0;

    padSize = {};
    idx0_pad16.release();
    idx1_pad16.release();
    s0_pad32.release();
    s1_pad32.release();
    winIdx16_.release();
    top1Score32_.release();
}

int FSFusionDMapBasic::computePyrLevels(const cv::Size& origSz) const
{
/*
Ensure dimensions are friendly for pyramid/downsampling
(powers of 2 / divisible sizes).
*/
    int levels = o.pyrLevels;
    if (levels <= 0) {
        int dim = std::max(origSz.width, origSz.height);
        levels = 5;
        while ((dim >> levels) > 8 && levels < 8) levels++;
    }
    return std::max(3, std::min(10, levels));
}

bool FSFusionDMapBasic::toColor32_01_FromLoaded(cv::Mat colorTmp,
                                                cv::Mat& color32,
                                                const QString& where,
                                                int sliceIdx)
{
    color32.release();

    if (colorTmp.empty()) {
        qWarning().noquote() << "WARNING:" << where << "empty colorTmp"
                             << (sliceIdx >= 0 ? QString("slice %1").arg(sliceIdx) : QString());
        return false;
    }

    if (colorTmp.channels() == 1) {
        cv::Mat bgr; cv::cvtColor(colorTmp, bgr, cv::COLOR_GRAY2BGR);
        colorTmp = bgr;
    } else if (colorTmp.channels() == 4) {
        cv::Mat bgr; cv::cvtColor(colorTmp, bgr, cv::COLOR_BGRA2BGR);
        colorTmp = bgr;
    } else if (colorTmp.channels() != 3) {
        qWarning().noquote() << "WARNING:" << where << "unsupported channels"
                             << colorTmp.channels()
                             << (sliceIdx >= 0 ? QString("slice %1").arg(sliceIdx) : QString());
        return false;
    }

    if (colorTmp.depth() == CV_8U)
        colorTmp.convertTo(color32, CV_32FC3, 1.0 / 255.0);
    else if (colorTmp.depth() == CV_16U)
        colorTmp.convertTo(color32, CV_32FC3, 1.0 / 65535.0);
    else {
        qWarning().noquote() << "WARNING:" << where << "unsupported depth"
                             << colorTmp.depth()
                             << (sliceIdx >= 0 ? QString("slice %1").arg(sliceIdx) : QString());
        return false;
    }

    return true;
}

void FSFusionDMapBasic::updateTop2(const cv::Mat& score32This, uint16_t sliceIndex)
{
/*
Update accumulated best and 2nd best score and slice index
*/
    CV_Assert(score32This.type() == CV_32F);
    CV_Assert(!s0_pad32.empty());
    CV_Assert(score32This.size() == s0_pad32.size());
    CV_Assert(score32This.size() == s1_pad32.size());
    CV_Assert(score32This.size() == idx0_pad16.size());
    CV_Assert(score32This.size() == idx1_pad16.size());
    CV_Assert(score32This.size() == top1Score32_.size());
    CV_Assert(score32This.size() == winIdx16_.size());

    const int H = score32This.rows;
    const int W = score32This.cols;

    for (int y = 0; y < H; ++y)
    {
        const float* sNew = score32This.ptr<float>(y);

        float* s0 = s0_pad32.ptr<float>(y);
        float* s1 = s1_pad32.ptr<float>(y);

        uint16_t* i0 = idx0_pad16.ptr<uint16_t>(y);
        uint16_t* i1 = idx1_pad16.ptr<uint16_t>(y);

        float*    top1 = top1Score32_.ptr<float>(y);
        uint16_t* win  = winIdx16_.ptr<uint16_t>(y);

        for (int x = 0; x < W; ++x)
        {
            const float s = sNew[x];

            if (s > s0[x])
            {
                // shift best -> second
                s1[x] = s0[x]; i1[x] = i0[x];
                s0[x] = s;     i0[x] = sliceIndex;
            }
            else if (s > s1[x])
            {
                s1[x] = s; i1[x] = sliceIndex;
            }

            // Keep “current best” mirrors for diagnostics / low-contrast gating
            top1[x] = s0[x];
            win[x]  = i0[x];
        }
    }
}

bool FSFusionDMapBasic::computeCropGeometry(const QString& srcFun,
                                            cv::Rect& roiPadToAlign,
                                            cv::Size& origSz) const
{
    roiPadToAlign = cv::Rect(0, 0, alignSize.width, alignSize.height);

    if (padSize != alignSize) {
        const int padW = padSize.width  - alignSize.width;
        const int padH = padSize.height - alignSize.height;
        if (padW < 0 || padH < 0) {
            qWarning().noquote() << "WARNING:" << srcFun << "padSize smaller than alignSize.";
            return false;
        }
        roiPadToAlign.x = padW / 2;
        roiPadToAlign.y = padH / 2;
    }

    const cv::Size alignSz(roiPadToAlign.width, roiPadToAlign.height);
    if (!FSFusion::rectInside(validAreaAlign, alignSz)) {
        qWarning().noquote() << "WARNING:" << srcFun << "validAreaAlign out of bounds.";
        return false;
    }

    origSz = cv::Size(validAreaAlign.width, validAreaAlign.height);
    return true;
}

void FSFusionDMapBasic::cropPadToOrig(const cv::Rect& roiPadToAlign,
                                      const cv::Size& origSz,
                                      cv::Mat& idx0_16,
                                      cv::Mat& idx1_16,
                                      cv::Mat& s0_32,
                                      cv::Mat& s1_32,
                                      cv::Mat& top1Score32) const
{
    CV_Assert(!idx0_pad16.empty());
    CV_Assert(!top1Score32_.empty());
    CV_Assert(!winIdx16_.empty());

    const cv::Rect roiAlign = roiPadToAlign;

    idx0_16     = idx0_pad16(roiAlign)(validAreaAlign).clone();
    idx1_16     = idx1_pad16(roiAlign)(validAreaAlign).clone();
    s0_32       = s0_pad32  (roiAlign)(validAreaAlign).clone();
    s1_32       = s1_pad32  (roiAlign)(validAreaAlign).clone();
    top1Score32 = top1Score32_(roiAlign)(validAreaAlign).clone();

    CV_Assert(idx0_16.size()     == origSz && idx0_16.type()     == CV_16U);
    CV_Assert(idx1_16.size()     == origSz && idx1_16.type()     == CV_16U);
    CV_Assert(s0_32.size()       == origSz && s0_32.type()       == CV_32F);
    CV_Assert(s1_32.size()       == origSz && s1_32.type()       == CV_32F);
    CV_Assert(top1Score32.size() == origSz && top1Score32.type() == CV_32F);
}

bool FSFusionDMapBasic::streamSlice(int slice,
                                    const cv::Mat& grayAlign8,
                                    const cv::Mat& colorAlign,
                                    const FSFusion::Options& /*opt*/,
                                    std::atomic_bool* abortFlag,
                                    FSFusion::StatusCallback /*statusCb*/,
                                    FSFusion::ProgressCallback /*progressCb*/)
{
/*
This function streams one aligned grayscale slice into a running “depth-map by
best-focus” accumulator. In other words: for every pixel, it’s tracking which slice
index is most in-focus (and its score), and also the 2nd-best.
    idx0_pad16:     best slice index
    idx1_pad16:     2nd best slice index
    s0_pad32:       best focus score
    s1_pad32:       2nd best focus score
    winIdx16_:      looks like a “current winner index” output buffer (often same as
                    idx0, depending on later code)
    top1Score32_:   top score buffer (often same as s0, depending on later code).
*/
    const QString srcFun = "FSFusionDMapBasic::streamSlice";
    if (FSFusion::isAbort(abortFlag)) return false;

    qDebug().noquote() << srcFun
                       << "this=" << (void*)this
                       << "slice=" << slice
                       << "top1Score32_.data=" << (void*)top1Score32_.data
                       << "top1Score32_.rows=" << top1Score32_.rows
                       << "cols=" << top1Score32_.cols;

    if (grayAlign8.empty() || grayAlign8.type() != CV_8U) {
        qWarning().noquote() << "WARNING:" << srcFun << "grayAlign8 empty or not CV_8U.";
        return false;
    }
    if (slice == 0)
    {
        /* Caller must have set these before calling streamSlice (same as your
           DMapAdvanced pattern).  alignSize / validAreaAlign should already be
           valid. */
        if (alignSize.width <= 0 || alignSize.height <= 0) {
            qWarning().noquote() << "WARNING:" << srcFun << "alignSize not set.";
            return false;
        }

        const int levelsForPad = computePyrLevels(alignSize);
        o.pyrLevels = levelsForPad;

        padSize = FSFusionDMapShared::computePadSizeForPyr(alignSize, levelsForPad);

        qDebug().noquote() << srcFun
                           << "padSize=" << padSize.width << "x" << padSize.height;

        if (!colorAlign.empty())
            outDepth = (colorAlign.depth() == CV_16U) ? CV_16U : CV_8U;

        idx0_pad16   = cv::Mat(padSize, CV_16U, cv::Scalar(0)); // best slice index
        idx1_pad16   = cv::Mat(padSize, CV_16U, cv::Scalar(0)); // 2nd best slice index
        s0_pad32     = cv::Mat(padSize, CV_32F, cv::Scalar(-1.0f));
        s1_pad32     = cv::Mat(padSize, CV_32F, cv::Scalar(-1.0f));
        winIdx16_    = cv::Mat(padSize, CV_16U, cv::Scalar(0));
        qDebug() << "1";
        top1Score32_ = cv::Mat(padSize, CV_32F, cv::Scalar(-1.0f));  // CRASH
        qDebug() << "2";

        active_ = true;
        sliceCount_ = 0;
    }
    else
    {
        if (!active_ || s0_pad32.empty()) {
            qWarning().noquote() << "WARNING:" << srcFun << "called without active state.";
            return false;
        }
    }

    if (grayAlign8.size() != alignSize) {
        qWarning().noquote() << "WARNING:" << srcFun << "alignSize mismatch.";
        return false;
    }

    // pad the aligned image to padSize, using reflection at the borders, centered
    cv::Mat grayPad8 = FSFusionDMapShared::padCenterReflect(grayAlign8, padSize);

    // focus metric
    cv::Mat score32 = FSFusionDMapShared::focusMetric32_dmap(grayPad8, o.scoreSigma, o.scoreKSize);

    // update accumulated best and 2nd best score and slice index
    updateTop2(score32, (uint16_t)std::max(0, slice));

    sliceCount_++;
    return true;
}

bool FSFusionDMapBasic::streamFinish(cv::Mat& outputColor,
                                     const FSFusion::Options& opt,
                                     cv::Mat& depthIndex16,
                                     const QStringList& inputPaths,
                                     const std::vector<Result>& globals,
                                     std::atomic_bool* abortFlag,
                                     FSFusion::StatusCallback statusCb,
                                     FSFusion::ProgressCallback progressCb)
{
    const QString srcFun = "FSFusionDMapBasic::streamFinish";
    if (G::FSLog) G::log(srcFun);

    const int N = inputPaths.size();

    if (!active_ || s0_pad32.empty() || N <= 0 || (int)globals.size() != N)
        return false;
    if (FSFusion::isAbort(abortFlag)) return false;

    // PAD→ALIGN→ORIG crop
    cv::Rect roiPadToAlign;
    cv::Size origSz;
    if (!computeCropGeometry(srcFun, roiPadToAlign, origSz))
        return false;

    // ------------------------------------------------------------
    // Measures and depth index
    // ------------------------------------------------------------

    cv::Mat idx0_16, idx1_16, s0_32, s1_32, top1_32;
    // assign accumulated values and crop to original size
    cropPadToOrig(roiPadToAlign, origSz, idx0_16, idx1_16, s0_32, s1_32, top1_32);

    // Depth output for inspection (best slice index)
    depthIndex16 = idx0_16.clone();

    cv::Mat topRatio32 = s0_32 / (s1_32 + 1e-6f);

    /*
    // Foreground mask
    // std::string fgPath =  "/Users/roryhill/Temp/Photos_to_be_curated/2026/202601/2026-01-08_FocusStack/FocusStack/2026-01-08_0048_StmDMapBasic/depth/fg.png";
    // cv::Mat fg8 = cv::imread(fgPath, cv::IMREAD_GRAYSCALE);
    // if (fg8.size() != origSz) {
    //     cv::Mat r;
    //     // For masks: INTER_NEAREST only
    //     cv::resize(fg8, r, origSz, 0, 0, cv::INTER_NEAREST);
    //     fg8 = r;
    // }
    // // Binarize to strict 0/255
    // // If your fg.png is already binary, this is harmless.
    // cv::threshold(fg8, fg8, 127, 255, cv::THRESH_BINARY);
    // // Ensure type exactly CV_8U
    // if (fg8.type() != CV_8U) {
    //     fg8.convertTo(fg8, CV_8U);
    // }
    // qDebug() << "fg.png size:" << fg8.cols << fg8.rows
    //          << "origSz:" << origSz.width << origSz.height; */

    cv::Mat fg8 = FSFusionDMapShared::buildFgFromTop1AndDepth(
        top1_32,
        depthIndex16,
        o.depthStableRadiusPx,
        o.depthMaxRangeSlices,
        o.strongFrac,
        o.weakFrac,
        o.seedDilatePx,
        o.closePx,
        o.openPx
        );

    /*
    cv::Mat diagBGR = FSFusionDMapShared::dmapDiagnosticBGR(
        idx0_16, idx1_16, fg8,
        cv::Mat(),   // conf01_opt
        s0_32,       // s0_32_opt
        s1_32,       // s1_32_opt
        50,          // ringPx
        0.22f);      // ringConfMax
    cv::imwrite((opt.depthFolderPath + "/dmap_diag_BGR.png").toStdString(), diagBGR);
        */

    // ------------------------------------------------------------
    // Ownership propagation (halo elimination): override depth in ring
    // ------------------------------------------------------------

    cv::Mat overrideMask8, overrideWinner16;

    // optional: close small fg gaps before ring (helps twigs)
    cv::Mat fgClean = FSFusionDMapShared::morphClose8(fg8, o.ownershipClosePx);

    if (!FSFusionDMapShared::ownershipPropagateTwoPass_Outward(
            fgClean,
            depthIndex16,
            FSFusionDMapShared::defaultRingPx(origSz),
            o.seedBandPx,
            overrideMask8,
            overrideWinner16))
    {
        return false;
    }

    if (!overrideMask8.empty() && cv::countNonZero(overrideMask8) > 0)
    {
        // Clamp overrideWinner16 to [0, N-1] before copying
        // Just in case donor values can ever be junk from uninitialized areas
        if (!overrideWinner16.empty()) {
            cv::min(overrideWinner16, (uint16_t)(N - 1), overrideWinner16);
        }

        // prevent any chance accidentally touch FG due to ring math
        overrideMask8.setTo(0, fg8); // never override inside FG

        // Deterministic “ownership”: replace depthIndex16 in ring with donor slice id
        overrideWinner16.copyTo(depthIndex16, overrideMask8);

        // Keep your working copies consistent (since you use idx0_16 for weights)
        overrideWinner16.copyTo(idx0_16, overrideMask8);
        overrideWinner16.copyTo(idx1_16, overrideMask8);

        // Force hard selection in ring (no blending)
        s0_32.setTo(1.0f, overrideMask8);
        s1_32.setTo(0.0f, overrideMask8);
    }

    /* // Contrast Threshold (not used)
    // cv::Mat lowC8;
    // if (params.enableContrastThreshold)
    // {
    //     lowC8 = FSFusionDMapShared::lowContrastMask8_fromTop1Score(
    //         top1_32, params.contrastMinFrac);

    //     if (params.lowContrastDilatePx > 0)
    //         cv::dilate(lowC8, lowC8, FSFusion::seEllipse(params.lowContrastDilatePx));

    //     if (cv::countNonZero(lowC8) > 0)
    //     {
    //         // 1) Stabilize depth labels in low-contrast regions (kills confetti/noise winners)
    //         const int k = (params.lowContrastMedianK == 3) ? 3 : 5;
    //         cv::Mat med16;
    //         cv::medianBlur(depthIndex16, med16, k);
    //         med16.copyTo(depthIndex16, lowC8);

    //         // Keep idx0 consistent with the stabilized depth map
    //         depthIndex16.copyTo(idx0_16, lowC8);

    //         // 2) FORCE HARD SELECTION (NO BLEND) in low-contrast pixels:
    //         // make idx1 == idx0 and make (s0,s1) = (1,0) so w32 becomes 1.0 for idx0 slice
    //         idx0_16.copyTo(idx1_16, lowC8);

    //         s0_32.setTo(1.0f, lowC8);
    //         s1_32.setTo(0.0f, lowC8);
    //     }
    // } */

    // ------------------------------------------------------------
    // Final fusion
    // ------------------------------------------------------------
    {

    cv::Mat out32(origSz, CV_32FC3, cv::Scalar(0,0,0));

    if (o.method == "Simple") {
        for (int s = 0; s < N; ++s)
        {
            cv::Mat grayTmp, colorTmp;
            if (!FSLoader::loadAlignedSliceOrig(s, inputPaths, globals,
                                                roiPadToAlign, validAreaAlign,
                                                grayTmp, colorTmp) || colorTmp.empty())
                return false;

            cv::Mat color32;
            if (!toColor32_01_FromLoaded(colorTmp, color32, srcFun, s))
                return false;

            cv::Mat mask8 = (depthIndex16 == (uint16_t)s);  // CV_8U 0/255
            color32.copyTo(out32, mask8);
        }
    }

    if (o.method == "Pyramid") {

        // Build pyramids from depthIndex16 (same as advanced path)
        const int levels = computePyrLevels(origSz);

        std::vector<cv::Mat> idxPyr16;
        FusionPyr::buildIndexPyrNearest(depthIndex16, levels, idxPyr16);

        // accumulator
        FusionPyr::PyrAccum A;
        A.reset(origSz, levels);

        cv::Mat sumW(origSz, CV_32F, cv::Scalar(0));

        // Per-slice accumulation
        for (int s = 0; s < N; ++s)
        {
            progressCb();
            statusCb("Fusing Slice: " + QString::number(s) + " of " + QString::number(N) + " ");

            if (FSFusion::isAbort(abortFlag)) return false;

            cv::Mat grayTmp, colorTmp;
            if (!FSLoader::loadAlignedSliceOrig(s, inputPaths, globals,
                                                roiPadToAlign, validAreaAlign,
                                                grayTmp, colorTmp) || colorTmp.empty())
                return false;

            cv::Mat color32;
            if (!toColor32_01_FromLoaded(colorTmp, color32, srcFun, s))
                return false;

            // w32 from continuous top2 mix
            cv::Mat w32(origSz, CV_32F, cv::Scalar(0));

            const float eps = std::max(1e-12f, o.mixEps);
            for (int y = 0; y < origSz.height; ++y)
            {
                const uint16_t* i0 = idx0_16.ptr<uint16_t>(y);
                const uint16_t* i1 = idx1_16.ptr<uint16_t>(y);
                const float*    a0 = s0_32.ptr<float>(y);
                const float*    a1 = s1_32.ptr<float>(y);

                float* w = w32.ptr<float>(y);

                for (int x = 0; x < origSz.width; ++x)
                {
                    const int d0 = (int)i0[x];
                    const int d1 = (int)i1[x];

                    if (d0 != s && d1 != s) continue;

                    const float s0 = std::max(0.0f, a0[x]);
                    const float s1 = std::max(0.0f, a1[x]);
                    const float den = s0 + s1 + eps;

                    float w0 = s0 / den;
                    float w1 = 1.0f - w0;

                    if (o.wMin > 0.0f) {
                        w0 = std::max(w0, o.wMin);
                        w1 = std::max(w1, o.wMin);
                        const float inv = 1.0f / (w0 + w1);
                        w0 *= inv; w1 *= inv;
                    }

                    if (d0 == s) w[x] = 1.0f;  // ignore top2 blend entirely
                    else         w[x] = 0.0f;
                    // if (d0 == s) w[x] = w0;
                    // else         w[x] = w1;
                }
            }

            sumW += w32;

            FusionPyr::AccumDMapParams ap;
            ap.enableHardWeightsOnLowpass = o.enableHardWeightsOnLowpass;
            ap.enableDepthGradLowpassVeto = o.enableDepthGradLowpassVeto;
            ap.hardFromLevel = o.hardFromLevel;
            ap.vetoFromLevel = o.vetoFromLevel;
            ap.vetoStrength  = o.vetoStrength;
            ap.wMin          = 0.0f; // wMin already handled above

            FusionPyr::accumulateSlicePyr(A,
                                          color32,
                                          w32,
                                          idxPyr16,
                                          nullptr,
                                          s,
                                          ap,
                                          levels,
                                          o.weightBlurSigma);
        }

        // cv::Mat32 out32 = FusionPyr::finalizeBlend(A, 1e-8f);
        out32 = FusionPyr::finalizeBlend(A, 1e-8f);
        cv::max(out32, 0.0f, out32);
        cv::min(out32, 1.0f, out32);
    }

    if (outDepth == CV_16U)
        out32.convertTo(outputColor, CV_16UC3, 65535.0);
    else
        out32.convertTo(outputColor, CV_8UC3, 255.0);

    } // end final fusion

    // ------------------------------------------------------------
    // Diagnostics
    // ------------------------------------------------------------
    qDebug().noquote() << QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss");
    qDebug().noquote() << "depthStableRadiusPx =" << o.depthStableRadiusPx;
    qDebug().noquote() << "depthMaxRangeSlices =" << o.depthMaxRangeSlices;
    qDebug().noquote() << "strongFrac          =" << o.strongFrac;
    qDebug().noquote() << "weakFrac            =" << o.weakFrac;
    qDebug().noquote() << "seedDilatePx        =" << o.seedDilatePx;
    qDebug().noquote() << "closePx             =" << o.closePx;
    qDebug().noquote() << "openPx              =" << o.openPx;
    qDebug().noquote() << "scoreSigma          =" << o.scoreSigma;
    qDebug().noquote() << "scoreKSize          =" << o.scoreKSize;
    qDebug().noquote() << "ownershipClosePx    =" << o.ownershipClosePx;
    qDebug().noquote() << "seedBandPx          =" << o.seedBandPx;
    qDebug().noquote() << "pyrLevels           =" << o.seedBandPx;

    if (!opt.depthFolderPath.isEmpty() && o.enableDiagnostics)
    {
        std::string s;
        s = (opt.depthFolderPath + "/dmapbasic_fg.png").toStdString();
        cv::imwrite(s, fg8);

        s = (opt.depthFolderPath + "/dmapbasic_ownership_ring.png").toStdString();
        cv::imwrite(s, overrideMask8);

        s = (opt.depthFolderPath + "/dmapbasic_ownership_winner.png").toStdString();
        cv::imwrite(s, FSUtilities::depthHeatmap(overrideWinner16, N, "Ownership winner"));

        s = (opt.depthFolderPath + "/dmapbasic_topRatio32.png").toStdString();
        cv::imwrite(s, topRatio32);

        s = (opt.depthFolderPath + "/dmapbasic_depth_idx0.png").toStdString();
        cv::imwrite(s, FSUtilities::depthHeatmap(depthIndex16, N, "DMapBasic idx0"));

        // this looks like a good fg ??
        double mn=0,mx=0; cv::minMaxLoc(top1_32, &mn, &mx);
        cv::Mat top18;
        top1_32.convertTo(top18, CV_8U, (mx > 1e-12) ? (255.0 / mx) : 1.0);
        s = (opt.depthFolderPath + "/dmapbasic_top1Score8.png").toStdString();
        cv::imwrite(s, top18);

        // s = (opt.depthFolderPath + "/dmapbasic_lowContrastMask.png").toStdString();
        // cv::imwrite(s, lowC8);
    }

    reset();
    return true;
}
