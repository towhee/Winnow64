#pragma once

#include "fsalign_types.h"
#include "fsfusion.h"
#include "fusionpyr.h"

#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>

#include <QString>
#include <atomic>
#include <vector>

// ============================================================
// FSFusionDMap
// "continuous-depth" DMap using Top-2 winners only (streamed 2-pass)
//
// pass-1: compute focus metric, maintain top2 (idx0/idx1 + scores) in PAD
// pass-2: crop to ORIG, build per-slice weights from top2 mix, pyramid blend
// ============================================================

class FSFusionDMap : public FSFusion
{
public:
    FSFusionDMap();
    ~FSFusionDMap() override = default;

    struct Params
    {
        // focus metric
        QString focusMetricMethod = "Laplacian";  // Laplacian or Tennengrad
        // QString focusMetricMethod = "Tennengrad";  // Laplacian or Tennengrad
        float scoreSigma = 0.75;    // 1.5
        int   scoreKSize = 3;       // 3

        // ADO: Adaptive Donor Override
        // Foreground
        int depthStableRadiusPx = 3;  // ↑ more strict
        int depthMaxRangeSlicesCore = 1;   // ↓ more strict (>1 = halos)
        int depthMaxRangeSlicesLoose = 5;  // ↓ more strict (>1 = halos)
        float expandTexFrac = 0.05f;  //   FG Must be connected to interior and
        // strongFrac must be > weakFrac
        float strongFrac = 0.030f;    // ↓ interior / holes
        float weakFrac = 0.01f;       // ↓ interior / holes (subtle)
        int seedDilatePx = 1;         // ↓ finer items (ie twigs)
        int closePx = 5;
        int openPx  = 1;
        int interiorPx = 3;  // default 3

        // --- Boundary Ownership / Halo elimination (Two-pass ownership propagation) ---
        int  ownershipClosePx = 3;      // close FG gaps a bit before building ring (0..3)
        int  seedBandPx       = 1;
        // int  ownershipErodePx = 1;      // boundary = FG - erode(FG)

        // // Low-contrast suppression (Zerene-style)
        bool  enableContrastThreshold = true;
        float contrastMinFrac = 0.20f;   // 0.3%..3% typical; start 1% (fraction of max top1Score)
        int   lowContrastMedianK = 5;     // 3 or 5
        int   lowContrastDilatePx = 1;    // optional (0..4)

        // pyramid / blend
        bool  enablePyramidBlend = true;
        bool  enableHardWeightsOnLowpass = false;    // false
        bool  enableDepthGradLowpassVeto = false;
        int   hardFromLevel = 0;    // 4
        int   vetoFromLevel = -1;
        float vetoStrength  = 1.0f;
        float weightBlurSigma = 0.0f; // 1.2f;
        float mixEps = 1e-6f;        // for score0+score1 denominator
        float wMin   = 0.0f;         // optional floor (0..0.02), usually 0

        int   pyrLevels = 5;

        bool enableDiagnostics = true;
    };

    Params o;

    // tmp debugging
    std::vector<QString> alignedColorPaths;     // intermediate
    std::vector<QString> alignedGrayPaths;      // intermediate


    void reset();

    bool streamSlice(int slice,
                     const cv::Mat& grayAlign8,      // CV_8U ALIGN
                     const cv::Mat& colorAlign,      // optional
                     const FSFusion::Options& opt,
                     std::atomic_bool* abortFlag,
                     FSFusion::StatusCallback statusCb,
                     FSFusion::ProgressCallback progressCb);

    bool streamFinish(cv::Mat& outputColor,
                      const FSFusion::Options& opt,
                      cv::Mat& depthIndex16,          // OUT: idx0 (best) ORIG
                      const QStringList& inputPaths,
                      const std::vector<Result>& globals,
                      std::atomic_bool* abortFlag,
                      FSFusion::StatusCallback statusCb,
                      FSFusion::ProgressCallback progressCb);

    // private:
    bool active_ = false;
    int  sliceCount_ = 0;

    // PAD-space top2
    cv::Size padSize;
    cv::Mat  idx0_pad16;   // CV_16U
    cv::Mat  idx1_pad16;   // CV_16U
    cv::Mat  s0_pad32;     // CV_32F
    cv::Mat  s1_pad32;     // CV_32F

    cv::Mat winIdx16_;     // CV_16U
    cv::Mat top1Score32_;  // CV_32F

    // geometry set by caller
    cv::Size alignSize;
    cv::Rect validAreaAlign;
    cv::Size origSize;

    int outDepth = CV_8U;

    // helpers
    int computePyrLevels(const cv::Size& origSz) const;

    bool toColor32_01_FromLoaded(cv::Mat colorTmp,
                                 cv::Mat& color32,
                                 const QString& where,
                                 int sliceIdx);

    void updateTop2(const cv::Mat& score32This, uint16_t sliceIndex);

    bool computeCropGeometry(const QString& srcFun,
                             cv::Rect& roiPadToAlign,
                             cv::Size& origSz) const;

    void accumulatedMeasures(const cv::Rect& roiPadToAlign,
                       const cv::Size& origSz,
                       cv::Mat& idx0_16,
                       cv::Mat& idx1_16,
                       cv::Mat& s0_32,
                       cv::Mat& s1_32,
                       cv::Mat& top1Score32) const;
};

/*

🔎 Focus Metric

scoreSigma = 1.5

Purpose
Gaussian pre-blur before Laplacian focus metric.

Impact
Controls noise sensitivity vs fine-detail sensitivity.

Increase
    •	More robust to noise
    •	Less sensitive to micro detail
    •	Slightly softer depth transitions
    •	Halos slightly less likely

Decrease
    •	More sensitive to fine edges
    •	More noise-triggered winners
    •	More halo risk
    •	Can increase “confetti” depth noise

⸻

scoreKSize = 3

Purpose
Laplacian kernel size.

Impact
Controls edge scale sensitivity.

Increase (5)
    •	Favors broader edges
    •	Less sensitive to fine twigs
    •	More stable depth map

Decrease (3)
    •	Captures fine twig detail
    •	More sensitive to micro halos
    •	Slightly noisier depth

⸻

🌿 Foreground Parameters

depthStableRadiusPx = 3

Purpose
Neighborhood radius for depth stability test.

Impact
Main halo suppressor.

Increase
    •	Stricter stability
    •	Fewer halos
    •	May lose thin foreground
    •	Can create transition blur (blue-arrow case)

Decrease
    •	More FG coverage
    •	More halo risk
    •	More slice bleeding

⸻

depthMaxRangeSlicesCore = 1

Purpose
Max slice variation allowed for stable FG core.

Impact
Boundary safety.

Increase
    •	More FG accepted
    •	Halos reappear quickly

Decrease
    •	Very halo safe
    •	More conservative FG
    •	More reliance on expansion logic

⸻

depthMaxRangeSlicesLoose = 5

Purpose
Max slice variation allowed for interior expansion.

Impact
Bridges large slice transitions inside subject.

Increase
    •	Fixes multi-slice FG transitions
    •	Risk of halo swallowing if texture gating weak

Decrease
    •	Less transition blur
    •	May fail to bridge real FG depth jumps

⸻

expandTexFrac = 0.05

Purpose
Minimum texture strength required for loose expansion.

Impact
Prevents halos entering via low-contrast regions.

Increase
    •	Safer from halos
    •	May fail to expand real low-texture subject regions

Decrease
    •	Better FG coverage
    •	More halo risk

⸻

strongFrac = 0.03

Purpose
High threshold for strong edge seeds.

Impact
Determines how much confident edge area exists.

Increase
    •	Stronger, tighter seeds
    •	More conservative FG
    •	May fragment thin structures

Decrease
    •	More seeds
    •	More aggressive FG growth
    •	Higher halo risk

⸻

weakFrac = 0.01

Purpose
Lower threshold for weak silhouette mask.

Impact
Defines potential subject region.

Increase
    •	Smaller silhouette
    •	More holes in FG

Decrease
    •	Larger silhouette
    •	Risk of including background texture

⸻

seedDilatePx = 1

Purpose
Expands strong seeds slightly.

Impact
Helps thin twig continuity.

Increase
    •	Stronger connectivity
    •	Slightly more aggressive FG

Decrease
    •	Finer structure preservation
    •	May fragment twigs

⸻

closePx = 5

Purpose
Morph close on weak silhouette and final FG.

Impact
Fills small gaps.

Increase
    •	Fewer pinholes
    •	Smoother FG boundary
    •	May over-smooth thin details

Decrease
    •	More natural shape
    •	Possible ring leaks

⸻

openPx = 1

Purpose
Removes tiny noise specks from FG.

Impact
Small cleanup.

Increase
    •	Cleaner FG
    •	Can erode fine detail

Decrease
    •	Keeps tiny features
    •	Slightly noisier mask

⸻

interiorPx = 3

Purpose
Erode core before geodesic growth.

Impact
Controls how deep interior expansion must start.

Increase
    •	Safer (less boundary creep)
    •	Harder to bridge narrow FG transitions

Decrease
    •	More aggressive expansion
    •	Slight halo risk

⸻

🧭 Ownership Propagation

ownershipClosePx = 3

Purpose
Close FG before building ownership ring.

Impact
Prevents ring leaking into pinholes.

Increase
    •	Safer halo suppression
    •	Slightly expands FG for ownership

Decrease
    •	More ring leakage risk

⸻

seedBandPx = 1

Purpose
Thickness of seed band outside FG for ownership.

Impact
Determines how ownership spreads.

Increase
    •	Smoother background correction
    •	Slight risk of overreach

Decrease
    •	Tighter halo removal
    •	May leave thin halo remnants

⸻

🎚 Contrast Threshold

enableContrastThreshold = true

Purpose
Stabilize depth in low-contrast regions.

Impact
    •	Reduces confetti depth noise
    •	Can slightly flatten soft background transitions

⸻

contrastMinFrac = 0.01

Purpose
Defines low-contrast threshold.

Increase
    •	More area stabilized
    •	More aggressive flattening

Decrease
    •	Less stabilization
    •	More depth noise

⸻

lowContrastMedianK = 5

Purpose
Median filter size for depth stabilization.

Increase
    •	Stronger smoothing
    •	May smear subtle transitions

Decrease
    •	Finer preservation
    •	Less noise removal

⸻

lowContrastDilatePx = 0

Purpose
Expand low-contrast mask.

Increase
    •	Larger stabilized areas
    •	Risk of flattening real detail

Decrease
    •	Minimal impact area

⸻

🏗 Pyramid Fusion

pyrLevels = 5

Purpose
Number of Laplacian pyramid levels.

Impact
    •	More levels = smoother blending
    •	Fewer levels = sharper but harsher transitions

⸻

enableHardWeightsOnLowpass = false

Purpose
Force hard selection at coarse pyramid levels.

Impact
    •	True: sharper boundaries
    •	False: smoother large-scale transitions

⸻

enableDepthGradLowpassVeto = false

Purpose
Prevent coarse blending across depth gradients.

Impact
    •	True: protects hard depth edges
    •	False: smoother blend across depth slopes

⸻

hardFromLevel = 4

Used only if hardWeights enabled.

⸻

vetoFromLevel = -1

Disabled (since veto disabled).

⸻

vetoStrength = 1

Full veto strength (only matters if enabled).

⸻

weightBlurSigma = 0

Purpose
Blur weight maps before pyramid.

Increase
    •	Smoother blends
    •	Softer edges

Decrease
    •	Harder transitions

⸻

mixEps = 1e-6

Numerical stability for division.
No visual effect.

⸻

wMin = 0

Minimum blending weight.

Increase
    •	More blending
    •	Smoother transitions
    •	Potential softness

Decrease
    •	Harder slice selection
    •	Sharper transitions

*/
