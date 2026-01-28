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
        QString method      = "PMax";       // "Simple", "PMax", "DMap"
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
        const std::vector<Result> &globals,
        std::atomic_bool *abortFlag,
        StatusCallback statusCallback,
        ProgressCallback progressCallback
    );

    // end StreamPMax pipeline

    // --------------- DMAP -----------------

    struct DMapParams
    {
        // Pass-1: keep top-K focus candidates per pixel
        int   topK = 2;                 // 1 = hard winner; 2 is typical

        // Focus score (Pass-1)
        float scoreSigma = 1.0f;        // pre-blur on gray before Laplacian (stability)
        int   scoreKSize  = 3;          // Laplacian kernel (3 or 5)
        float scoreEps    = 1e-6f;      // small epsilon (future-proofing)

        // Soft weights from top-K scores (Pass-2)
        float softTemp = 0.15f;         // smaller = harder assignment
        float wMin     = 0.02f;         // per-slot weight floor (avoid zeros)

        // Weight smoothing (halo killer)
        int   weightBlurPx    = 3;      // boundary band dilation in pixels (orig)
        float weightBlurSigma = 1.2f;   // Gaussian sigma for weight smoothing

        // Optional tiling (future)
        bool  useTiling = false;
        int   tilePx = 512;
        int   tileOverlapPx = 32;

        // Pyramid blend (Pass-2)
        int   pyrLevels = 5;            // 4..7 typical; 0 = auto
    };
    DMapParams dmapParams;

    struct TopKMaps
    {
        int K = 0;
        cv::Size sz;

        // K mats, each size=sz:
        //  idx16[k]   : CV_16U slice index for kth best score at each pixel
        //  score32[k] : CV_32F kth best score at each pixel (descending)
        std::vector<cv::Mat> idx16;
        std::vector<cv::Mat> score32;

        void create(cv::Size s, int k)
        {
            reset();
            sz = s;
            K  = k;

            idx16.resize(K);
            score32.resize(K);

            for (int i = 0; i < K; ++i)
            {
                idx16[i]   = cv::Mat(sz, CV_16U, cv::Scalar(0));
                score32[i] = cv::Mat(sz, CV_32F, cv::Scalar(-1e30f)); // very low
            }
        }

        void reset()
        {
            K = 0;
            sz = {};
            idx16.clear();
            score32.clear();
        }

        bool valid() const
        {
            if (K <= 0) return false;
            if (sz.width <= 0 || sz.height <= 0) return false;
            if ((int)idx16.size() != K) return false;
            if ((int)score32.size() != K) return false;

            for (int k = 0; k < K; ++k)
            {
                if (idx16[k].empty() || score32[k].empty()) return false;
                if (idx16[k].size() != sz || score32[k].size() != sz) return false;
                if (idx16[k].type() != CV_16U) return false;
                if (score32[k].type() != CV_32F) return false;
            }
            return true;
        }
    };
    TopKMaps   dmapTopKPad;  // pass-1 maps in PAD size

    struct DMapPass1
    {
        bool initialized = false;
        int  sliceCount = 0;
        cv::Size origSize;

        TopKMaps topk;          // (idx, score)
        cv::Mat  edgeMask8;     // optional: computed from fusedGray later, or from last slice
        // You can also store a running “bestScore” convenience ref to topk.score32[0].

        void begin(cv::Size orig, int K)
        {
            initialized = true;
            sliceCount = 0;
            origSize = orig;
            topk.create(orig, K);
            edgeMask8.release();
        }

        void reset()
        {
            initialized = false;
            sliceCount = 0;
            origSize = {};
            topk.reset();
            edgeMask8.release();
        }
    };

    // struct DMapStreamState;
    // std::unique_ptr<DMapStreamState> dmap;   // allocated on first slice, reset after finish

    bool      dmapActive = false;
    int       dmapSliceCount = 0;

    bool streamDMapSlice(int slice,
                         const cv::Mat& grayImg,
                         const cv::Mat& colorImg,
                         const Options& opt,
                         std::atomic_bool* abortFlag,
                         StatusCallback statusCallback,
                         ProgressCallback progressCallback);

    bool streamDMapFinish(cv::Mat& outputColor,
                          const Options& opt,
                          cv::Mat& depthIndex16,
                          const QStringList& inputPaths,
                          const std::vector<Result>& globals,
                          std::atomic_bool* abortFlag,
                          StatusCallback statusCallback,
                          ProgressCallback progressCallback);

}; // end class FSFusion

#endif // FSFUSION_H
