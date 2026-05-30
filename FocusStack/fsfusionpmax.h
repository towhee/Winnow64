#pragma once

#include "fsalign_types.h"
#include "fsfusion.h"
#include "FSMerge.h"
#include "FSFusionReassign.h"

#include <opencv2/core.hpp>
#include <atomic>

class FSFusion;

// ============================================================
// FSFusionPMax.h
// PMax implementation (one-shot + streaming).
// ============================================================

class FSFusionPMax : public FSFusion
{
public:
    FSFusionPMax();
    ~FSFusionPMax() override;

    struct Options
    {
        QString energyMode           = "Max";     // "Max" or "Weighted"
        float weightedPower          = 4.0f;
        float weightedSigma0         = 1.0f;
        bool  weightedIncludeLowpass = true;
        float weightedEpsEnergy      = 1e-6f;
        float weightedEpsWeight      = 1e-6f;
    } popt;

    // Streaming PMax
    bool streamSlice(int slice,
                     const cv::Mat& grayAlign,
                     const cv::Mat& colorAlign,
                     const FSFusion::Options& opt,
                     std::atomic_bool* abortFlag,
                     FSFusion::StatusCallback statusCb,
                     FSFusion::ProgressCallback progressCb);

    bool streamFinish(cv::Mat& outputColor,
                      const FSFusion::Options& opt,
                      cv::Mat& depthIndex16,
                      const QStringList& inputPaths,
                      const std::vector<Result>& globals,
                      std::atomic_bool* abortFlag,
                      FSFusion::StatusCallback statusCb,
                      FSFusion::ProgressCallback progressCb);

    void reset();

private:
    // ------------------------------------------------------------
    // Streaming state
    // ------------------------------------------------------------
    FSMerge::StreamState mergeState_;
    cv::Mat wavelet_;
    cv::Mat mergedWavelet_;
    cv::Size waveletSize_;

    cv::Mat depthIndexPadded16_; // CV_16U padded winner map from merge

    // Color reassign streaming builder
    FSFusionReassign::ColorMapBuilder colorBuilder_;
    std::vector<FSFusionReassign::ColorEntry> colorEntries_;
    std::vector<uint8_t> counts_;

    cv::Mat fusedGray8_;   // padded fused grayscale

    // Streaming invariants
    cv::Size alignSize_;
    cv::Size padSize_;
    int outDepth_ = CV_8U;
};
