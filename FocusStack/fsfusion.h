#ifndef FSFUSION_H
#define FSFUSION_H

#include <opencv2/core.hpp>
#include <vector>
#include <functional>

#include <QString>

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
                     This reproduces the current successful pipeline.

          "Simple" = use depthIndex16 only to pick per-pixel color from
                     the corresponding slice (no wavelets). Fast, pairs
                     naturally with FSDepth::method == "Simple".
        */
        QString method      = "PMax";

        // PMax-specific options
        bool useOpenCL      = true;  // GPU wavelet via cv::UMat
        int  consistency    = 2;     // 0 = off, 1 = subband denoise, 2 = +neighbour
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
     - outputColor8 : fused RGB 8-bit

    Returns false on any error (size/type mismatch, empty input, etc).

    NOTE:
      - FSFusion no longer computes or exposes any "canonical" depth map.
        The only depth map in the system is the one produced by FSDepth.
    */
    static bool fuseStack(const std::vector<cv::Mat> &grayImgs,
                          const std::vector<cv::Mat> &colorImgs,
                          const Options              &opt,
                          cv::Mat                    &depthIndex16,
                          cv::Mat                    &outputColor8,
                          std::atomic_bool           *abortFlag,
                          StatusCallback              statusCb,
                          ProgressCallback            progressCallback);
};

#endif // FSFUSION_H
