#ifndef FSFUSION_H
#define FSFUSION_H

#include <opencv2/core.hpp>
#include <vector>
#include <functional>

#include <QString>

#include "FSMerge.h"
#include "FSFusionReassign.h"
#include "FSAlign.h"

class FSFusion
{
public:
    using ProgressCallback = std::function<void()>;
    using StatusCallback   = std::function<void(const QString &message)>;

    struct Options
    {
        /*
        method:
          "PMax"   = full multiscale PMax fusion (wavelet-based)

          "Simple" = use depthIndex16 only to pick per-pixel color from
                     the corresponding slice (no wavelets). Fast, pairs
                     naturally with FSDepth::method == "Simple".
        */
        QString method      = "PMax";       // "Simple" or "PMax"
        QString mergeMode   = "PMax";       // “PMax” or “Weighted”
        QString winnerMap = "Weighted";     // "Energy" or "Weighted"

        bool enableDepthBiasedErosion = false;
        bool enableEdgeAdaptiveSigma  = false;

        // PMax-specific options
        bool useOpenCL      = true;  // GPU wavelet via cv::UMat
        int  consistency    = 2;     // 0 = off, 1 = subband denoise, 2 = +neighbour

        // mergeMode = "Weighted" (Stage 2)
        float weightedPower = 4.0f;
        float weightedSigma0 = 1.0f;
        bool  weightedIncludeLowpass = true;
        float weightedEpsEnergy = 1e-8f;
        float weightedEpsWeight = 1e-8f;

        // Depth-biased erosion (Stage 3)
        // Deeper = higher slice index

        // // Edge mask from fusedGray8:
        // float erosionEdgeSigma      = 1.0f;   // blur on gradient magnitude
        // float erosionEdgeThresh     = 0.06f;  // 0..1 fraction of max grad
        // int   erosionEdgeDilate     = 2;      // expands edge band

        // // Erosion itself:
        // int   erosionRadius         = 2;      // neighborhood radius (1..4)
        // int   erosionIters          = 1;      // 1..3 typical
        // int   erosionMaxDelta       = 6;      // clamp upward change per iter
        // float erosionMinEdgeDelta   = 2.0f;   // require local contrast to act

        // depthmap path for debugging pngs
        QString depthFolderPath;
    };

    inline bool isAbort(const std::atomic_bool *f)
    {
        return f && f->load(std::memory_order_relaxed);
    }


    /*
    PMax / Simple fusion:
     *
     - grayImgs     : aligned grayscale 8U (one per slice, all same size)
     - colorImgs    : aligned color 8UC3 or 16UC3 (same size/count as gray)
     - opt          : see Options above
     - depthIndex16 : REQUIRED depth index (0..N-1) from FSDepth
     - outputColor  : fused RGB 8-bit / 16-bit

    Returns false on any error (size/type mismatch, empty input, etc).
    */

    static bool fuseStack(const std::vector<cv::Mat> &grayImgs,
                          const std::vector<cv::Mat> &colorImgs,
                          const Options              &opt,
                          cv::Mat                    &depthIndex16,
                          cv::Mat                    &outputColor,
                          std::atomic_bool           *abortFlag,
                          StatusCallback              statusCb,
                          ProgressCallback            progressCallback
                          );


    // StreamPMax pipeline
    // Merge
    FSMerge::StreamState mergeState;
    cv::Rect validAreaAlign;             // original image region inside padded frame
    cv::Size origSize;                   // focus stack input image dimensions
    cv::Size alignSize;                  // aligned image dimensions
    cv::Size firstAlignedSize;
    cv::Mat wavelet;
    cv::Mat mergedWavelet;
    cv::Size waveletSize;
    cv::Mat depthIndexPadded16;   // CV_16U, size == padSize
    // Color reassign
    FSFusionReassign::ColorMapBuilder colorBuilder;
    std::vector<FSFusionReassign::ColorEntry> colorEntries;
    std::vector<uint8_t> counts;
    cv::Size padSize;
    cv::Mat fusedGray8;
    int outDepth;

    bool streamPMaxSlice(
        int slice,
        const cv::Mat &grayImg,
        const cv::Mat &colorImg,
        const Options &opt,
        std::atomic_bool *abortFlag,
        StatusCallback statusCb,
        ProgressCallback progressCallback
    );

    bool streamPMaxFinish(
        cv::Mat &outputColor,
        const Options &opt,
        cv::Mat &depthIndex16,
        const QStringList  &inputPaths,
        const std::vector<FSAlign::Result> &globals,
        std::atomic_bool *abortFlag,
        StatusCallback statusCallback,
        ProgressCallback progressCallback
    );

    // end StreamPMax pipeline

}; // end class FSFusion

#endif // FSFUSION_H
