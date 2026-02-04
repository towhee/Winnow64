#pragma once

#include "fsalign_types.h"
#include "fsfusion.h"

#include <opencv2/core.hpp>
#include <atomic>

#include "FSMerge.h"
#include "FSFusionReassign.h"

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

    // One-shot in-memory PMax
    bool fusePMax(const std::vector<cv::Mat>& grayImgs,
                  const std::vector<cv::Mat>& colorImgs,
                  const FSFusion::Options&     opt,
                  cv::Mat&                     depthIndex16,
                  cv::Mat&                     outputColor,
                  std::atomic_bool*            abortFlag,
                  FSFusion::StatusCallback     statusCb,
                  FSFusion::ProgressCallback   progressCb);

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
