#include "FSFusionPMax.h"

#include "FSFusionWavelet.h"
#include "FSUtilities.h"
#include "FSLoader.h"
#include "Main/global.h"

#include <opencv2/imgproc.hpp>

namespace
{
static inline FSFusionReassign::ColorDepth reassignDepth(int depth)
{
    return (depth == CV_16U)
               ? FSFusionReassign::ColorDepth::U16
               : FSFusionReassign::ColorDepth::U8;
}

cv::Mat padForWavelet(const cv::Mat& img, cv::Size& paddedSizeOut)
{
    if (img.empty()) return img;

    cv::Size expanded;
    FSFusionWavelet::computeLevelsAndExpandedSize(img.size(), expanded);
    paddedSizeOut = expanded;

    if (expanded == img.size())
        return img;

    const int padW = expanded.width  - img.cols;
    const int padH = expanded.height - img.rows;

    const int left   = padW / 2;
    const int right  = padW - left;
    const int top    = padH / 2;
    const int bottom = padH - top;

    cv::Mat padded;
    cv::copyMakeBorder(img, padded, top, bottom, left, right, cv::BORDER_REFLECT);
    return padded;
}
} // namespace

FSFusionPMax::FSFusionPMax() = default;
FSFusionPMax::~FSFusionPMax() = default;

void FSFusionPMax::reset()
{
    mergeState_.reset();
    wavelet_.release();
    mergedWavelet_.release();
    waveletSize_ = {};

    depthIndexPadded16_.release();

    colorBuilder_.reset();
    colorEntries_.clear();
    counts_.clear();

    fusedGray8_.release();

    alignSize_ = {};
    padSize_ = {};
    outDepth_ = CV_8U;
}

// ============================================================
// Streaming PMax
// ============================================================

bool FSFusionPMax::streamSlice(int slice,
                               const cv::Mat& grayAlign,
                               const cv::Mat& colorAlign,
                               const FSFusion::Options& opt,
                               std::atomic_bool* abortFlag,
                               FSFusion::StatusCallback /*statusCb*/,
                               FSFusion::ProgressCallback /*progressCb*/)
{
    const QString srcFun = "FSFusionPMax::streamSlice";
    const QString s = QString::number(slice);

    if (grayAlign.empty() || colorAlign.empty())
        return false;

    if (slice == 0)
    {
        alignSize_ = grayAlign.size();
        outDepth_  = colorAlign.depth();
    }
    else
    {
        if (grayAlign.size() != alignSize_) return false;
        if (colorAlign.size() != alignSize_) return false;
        if (colorAlign.depth() != outDepth_) return false;
    }

    // Pad slice
    cv::Size paddedSize;
    cv::Mat grayP  = padForWavelet(grayAlign, paddedSize);
    cv::Mat colorP = padForWavelet(colorAlign, paddedSize);

    if (slice == 0)
    {
        padSize_ = paddedSize;

        // init once
        colorBuilder_.begin(grayP.size(), /*fixedCapPerPixel=*/4);
        mergeState_.reset();
        mergedWavelet_.release();
        wavelet_.release();
        waveletSize_ = {};
    }
    else
    {
        if (paddedSize != padSize_) return false;
    }

    if (FSFusion::isAbort(abortFlag)) return false;

    // Forward wavelet
    if (!FSFusionWavelet::forward(grayP, opt.useOpenCL, wavelet_))
        return false;

    if (slice == 0)
        waveletSize_ = wavelet_.size();
    else if (wavelet_.size() != waveletSize_)
        return false;

    // Merge slice
    if (opt.mergeMode == "PMax")
    {
        if (!FSMerge::mergeSlice(mergeState_,
                                 wavelet_,
                                 waveletSize_,
                                 opt.consistency,
                                 abortFlag,
                                 mergedWavelet_))
            return false;
    }
    else if (opt.mergeMode == "Weighted")
    {
        FSMerge::WeightedParams wp;
        wp.power          = opt.weightedPower;
        wp.sigma0         = opt.weightedSigma0;
        wp.includeLowpass = opt.weightedIncludeLowpass;
        wp.epsEnergy      = opt.weightedEpsEnergy;
        wp.epsWeight      = opt.weightedEpsWeight;

        if (!FSMerge::mergeSliceWeighted(mergeState_,
                                         wavelet_,
                                         waveletSize_,
                                         wp,
                                         opt.consistency,
                                         abortFlag,
                                         mergedWavelet_))
            return false;
    }
    else
    {
        qWarning().noquote() << "WARNING:" << srcFun << "Unknown mergeMode:" << opt.mergeMode;
        return false;
    }

    // Add to color builder
    if (!colorBuilder_.addSlice(grayP, colorP))
        return false;

    return true;
}

bool FSFusionPMax::streamFinish(cv::Mat& outputColor,
                                const FSFusion::Options& opt,
                                cv::Mat& depthIndex16,
                                const QStringList& /*inputPaths*/,
                                const std::vector<Result>& /*globals*/,
                                std::atomic_bool* abortFlag,
                                FSFusion::StatusCallback /*statusCb*/,
                                FSFusion::ProgressCallback /*progressCb*/)
{
    const QString srcFun = "FSFusionPMax::streamFinish";

    if (FSFusion::isAbort(abortFlag)) return false;

    // Finish merge
    if (opt.mergeMode == "PMax")
    {
        if (!FSMerge::mergeSliceFinish(mergeState_,
                                       opt.consistency,
                                       abortFlag,
                                       mergedWavelet_,
                                       depthIndexPadded16_))
            return false;
    }
    else if (opt.mergeMode == "Weighted")
    {
        FSMerge::WeightedParams wp;
        wp.power          = opt.weightedPower;
        wp.sigma0         = opt.weightedSigma0;
        wp.includeLowpass = opt.weightedIncludeLowpass;
        wp.epsEnergy      = opt.weightedEpsEnergy;
        wp.epsWeight      = opt.weightedEpsWeight;

        cv::Mat weightedWinnerPadded16;
        cv::Mat energyWinnerPadded16;

        if (!FSMerge::mergeSliceFinishWeighted(mergeState_,
                                               wp,
                                               opt.consistency,
                                               abortFlag,
                                               mergedWavelet_,
                                               weightedWinnerPadded16,
                                               energyWinnerPadded16))
            return false;

        depthIndexPadded16_ = (opt.winnerMap == "Energy") ? energyWinnerPadded16
                                                          : weightedWinnerPadded16;
    }

    if (FSFusion::isAbort(abortFlag)) return false;

    // Inverse wavelet -> fusedGray8 (padded)
    if (!FSFusionWavelet::inverse(mergedWavelet_, opt.useOpenCL, fusedGray8_))
        return false;

    if (fusedGray8_.type() != CV_8U || fusedGray8_.size() != padSize_)
        return false;

    // Finish color builder
    colorBuilder_.finish(colorEntries_, counts_);

    // Apply color map -> padded color out
    cv::Mat paddedColorOut;
    if (!FSFusionReassign::applyColorMap(fusedGray8_,
                                         colorEntries_,
                                         counts_,
                                         paddedColorOut,
                                         reassignDepth(outDepth_)))
        return false;

    // Crop back:
    // padSize -> alignSize -> origSize (validAreaAlign)
    // const cv::Size alignSize = FSFusion::alignSize;
    // const cv::Rect validAreaAlign = owner.validAreaAlign;
    // const cv::Size origSize = owner.origSize;

    // 1) pad -> align ROI
    cv::Rect roiPadToAlign(0, 0, alignSize.width, alignSize.height);
    if (padSize_ != alignSize)
    {
        const int padW = padSize_.width  - alignSize.width;
        const int padH = padSize_.height - alignSize.height;
        if (padW < 0 || padH < 0) return false;

        roiPadToAlign = cv::Rect(padW / 2, padH / 2, alignSize.width, alignSize.height);
    }

    if (roiPadToAlign.x < 0 || roiPadToAlign.y < 0 ||
        roiPadToAlign.x + roiPadToAlign.width  > paddedColorOut.cols ||
        roiPadToAlign.y + roiPadToAlign.height > paddedColorOut.rows ||
        roiPadToAlign.x + roiPadToAlign.width  > depthIndexPadded16_.cols ||
        roiPadToAlign.y + roiPadToAlign.height > depthIndexPadded16_.rows)
        return false;

    // 2) align -> orig ROI
    if (validAreaAlign.width != origSize.width || validAreaAlign.height != origSize.height)
        return false;

    cv::Mat colorAlign = paddedColorOut(roiPadToAlign);
    cv::Mat depthAlign = depthIndexPadded16_(roiPadToAlign);

    if (validAreaAlign.x < 0 || validAreaAlign.y < 0 ||
        validAreaAlign.x + validAreaAlign.width  > colorAlign.cols ||
        validAreaAlign.y + validAreaAlign.height > colorAlign.rows ||
        validAreaAlign.x + validAreaAlign.width  > depthAlign.cols ||
        validAreaAlign.y + validAreaAlign.height > depthAlign.rows)
        return false;

    outputColor  = colorAlign(validAreaAlign).clone();
    depthIndex16 = depthAlign(validAreaAlign).clone();

    // ------------------------------------------------------------
    // Optional debug hooks (halo detection, etc.)
    // Put your existing “LL testing / halo detector” block HERE if desired,
    // using fusedGray8_ (padded) and the cropping ROIs above.
    // ------------------------------------------------------------

    // Cleanup streaming state
    reset();

    return true;
}
