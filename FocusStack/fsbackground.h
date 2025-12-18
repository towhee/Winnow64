#ifndef FSBACKGROUND_H
#define FSBACKGROUND_H

#pragma once

#include <QString>
#include <functional>
#include <vector>
#include <atomic>

#include <opencv2/core.hpp>

namespace FSBackground
{

struct Options
{
    // Methods (macro-friendly defaults):
    //  "Depth+Focus"
    //  "FocusOnly"
    //  "DepthOnly"
    //  "Depth+Focus+ColorVar"

    /*  Notes:
            •	focusLowThresh = 0.018f
            •	focusHighThresh = 0.060f
            •	supportFocusThresh = 0.020f
            •	minSupportRatio = 0.08f
            •	farDepthStart01 = 0.78f
            •	farDepthStrong01 = 0.90f
            •	depthUnstableStd = 2.0f (in “slice units”)

        Why these values work for macro:
            •	background almost never exceeds ~0.02 focus energy
            •	subject “true sharp” often exceeds ~0.06 at least somewhere
            •	depth indices are less stable in background/transition zones
            •	far end of stack usually corresponds to background plane

        You will still want to expose one slider in UI:
            •	subject/background threshold (currently hardcoded at 0.50 in run())
    */
    QString method = "Depth+Focus";

    // --- Macro tuned thresholds (good starting point) ---
    // Focus maps / wavelet energy or any "focus score" normalized to ~0..1
    float focusLowThresh        = 0.018f;   // background if max focus below this
    float focusHighThresh       = 0.060f;   // definitely subject if max focus above this
    float supportFocusThresh    = 0.020f;   // slice "supports" pixel if focus > this
    float minSupportRatio       = 0.08f;    // background if supported by <8% of slices

    // Depth index rules (depthIndex16 is slice index 0..N-1):
    // Background often clusters near far end for macro stacks.
    float farDepthStart01       = 0.78f;    // background prior if depthNormalized > this
    float farDepthStrong01      = 0.90f;    // strong background if > this
    float depthUnstableStd      = 2.0f;     // local stddev in indices (in "slice units")

    // Color variance across slices (optional, expensive)
    bool  enableColorVar        = false;
    float colorVarThresh        = 0.020f;   // in float 0..1 space

    // Post cleanup
    int   medianK               = 5;        // 3 or 5 good
    int   morphRadius           = 3;        // 2..5 (struct element radius)
    int   removeIslandsMinArea  = 800;      // px; macro stacks often large subject

    // Feathering / blend edge
    int   featherRadius         = 6;        // 3..12 (bigger = smoother transition)
    float featherGamma          = 1.2f;      // >1 sharpens edge; <1 softens

    // Debug
    bool    writeDebug          = true;
    QString debugFolder;
    float   overlayAlpha        = 0.55f;    // for debug overlay
    float   overlayThresh       = 0.50f;    // show overlay where bgConfidence > thresh

    // --- NEW (auto by default) ---
    // If < 0: computed from percentiles of score map each run.
    float seedHighQuantile      = -1.0f;  // e.g. 0.92 means top 8% bg-score become seeds
    float growLowQuantile       = -1.0f;  // e.g. 0.70 means grow into top 30%
    bool  preferBorderSeeds     = true;   // macro stacks: background usually touches borders

};

using ProgressCallback = std::function<void(int current, int total)>;
using StatusCallback   = std::function<void(const QString &message)>;

// Main entry: compute background confidence and subject mask
bool run(const cv::Mat                 &depthIndex16,      // CV_16U, optional if method doesn’t need it
         const std::vector<cv::Mat>    &focusSlices32,     // CV_32F per slice (recommended); can be empty if not used
         const std::vector<cv::Mat>    &alignedColor,      // optional for colorVar (CV_8UC3 or CV_32FC3)
         int                            sliceCount,
         const Options                 &opt,
         std::atomic_bool              *abortFlag,
         ProgressCallback               progressCb,
         StatusCallback                 statusCb,
         cv::Mat                       &bgConfidence01Out, // CV_32F 0..1
         cv::Mat                       &subjectMask8Out);  // CV_8U 0 or 255

// Replace background cleanly using a single background source slice.
// Uses subjectMask8 + feathering to avoid seams.
bool replaceBackground(cv::Mat                    &fusedInOut,      // CV_8UC3 or CV_32FC3
                       const cv::Mat              &subjectMask8,    // CV_8U 0/255
                       const cv::Mat              &bgSource,        // same type/size as fusedInOut
                       const Options              &opt,
                       std::atomic_bool           *abortFlag);

// Debug overlay (like artifact overlay): show bg-confidence on top of fused gray
cv::Mat makeOverlayBGR(const cv::Mat &baseGrayOrBGR,
                       const cv::Mat &bgConfidence01,   // CV_32F
                       const Options &opt);

} // namespace FSBackground

#endif // FSBACKGROUND_H
