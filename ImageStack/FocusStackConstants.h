#ifndef FOCUSSTACKCONSTANTS_H
#define FOCUSSTACKCONSTANTS_H

#include <QtGlobal>
#include <QString>
#include <QSize>

//
// ============================================================================
// Focus Stack Constants & Presets
// ============================================================================
// Centralizes default values and tuning presets for all focus-stack stages.
// ============================================================================
//

namespace FSConst {

// ----------------------------------------------------------------------------
// General
// ----------------------------------------------------------------------------
constexpr int    kDefaultDownsample        = 1;
constexpr double kDefaultGamma             = 1.0;
constexpr double kMinEpsilon               = 1e-6;
constexpr int    kDefaultThreadCount       = 4;

// ----------------------------------------------------------------------------
// FocusMeasure defaults
// ----------------------------------------------------------------------------
namespace Focus {
constexpr float  kPetteriGaussRadius       = 1.0f;
constexpr float  kPetteriEnergyThreshold   = 0.0f;
constexpr int    kDefaultLevels            = 1;     // single-scale
constexpr bool   kSaveResultsDefault       = true;
} // namespace Focus

// ----------------------------------------------------------------------------
// DepthMap defaults
// ----------------------------------------------------------------------------
namespace Depth {
constexpr int  kThresholdDefault   = 10;
constexpr int  kSmoothXYDefault    = 20;
constexpr int  kSmoothZDefault     = 40;
constexpr int  kHaloRadiusDefault  = 20;
constexpr bool kSaveStepsDefault   = false;
constexpr bool kUsePetteriDefault  = true;
constexpr bool kSmooth             = true;
} // namespace Depth

// ----------------------------------------------------------------------------
// Fusion defaults
// ----------------------------------------------------------------------------
namespace Fusion {
constexpr int    kConsistencyDefault = 1;      // 0..2
constexpr double kDenoiseDefault     = 1.0;
constexpr int    kLevelsAuto         = -1;
constexpr bool   kKeepFullDefault    = false;
constexpr bool   kSaveStepsDefault   = false;
} // namespace Fusion

// ----------------------------------------------------------------------------
// Preset identifiers
// ----------------------------------------------------------------------------
namespace Preset {
const QString kPetteri = "Petteri";
const QString kZerene  = "Zerene";
const QString kWinnow  = "Winnow";
} // namespace Preset

// ----------------------------------------------------------------------------
// Helper: Wavelet levels heuristic (Petteri-style)
// ----------------------------------------------------------------------------
inline int levelsForSize(const QSize &size)
{
    int minDim = qMin(size.width(), size.height());
    int levels = 1;
    while ((minDim >> levels) > 64 && levels < 10)
        ++levels;
    return levels;
}

// ----------------------------------------------------------------------------
// Unified Preset Structures
// ----------------------------------------------------------------------------
struct FocusPreset {
    float gaussRadius;
    float energyThreshold;
    int   levels;
    bool  saveResults;
};

struct DepthPreset {
    bool smooth;
    int threshold;
    int smoothXY;
    int smoothZ;
    int haloRadius;
    bool saveSteps;
};

struct FusionPreset {
    int consistency;
    double denoise;
    int levels;
    bool keepFull;
    bool saveSteps;
};

// ----------------------------------------------------------------------------
// Predefined tuning sets for A/B comparison
// ----------------------------------------------------------------------------
inline FocusPreset presetFocusPetteri() {
    return { Focus::kPetteriGaussRadius,
            Focus::kPetteriEnergyThreshold,
            Focus::kDefaultLevels,
            Focus::kSaveResultsDefault };
}

inline DepthPreset presetDepthPetteri() {
    return {Depth::kSmooth,
            Depth::kThresholdDefault,
            Depth::kSmoothXYDefault,
            Depth::kSmoothZDefault,
            Depth::kHaloRadiusDefault,
            Depth::kSaveStepsDefault };
}

inline FusionPreset presetFusionPetteri() {
    return { Fusion::kConsistencyDefault,
            Fusion::kDenoiseDefault,
            Fusion::kLevelsAuto,
            Fusion::kKeepFullDefault,
            Fusion::kSaveStepsDefault };
}

// Example alternate tuning (Zerene-like)
inline FocusPreset presetFocusZerene() {
    return { 0.8f, 0.0f, 1, true };
}
inline DepthPreset presetDepthZerene() {
    return { 15, 15, 35, 10, false };
}
inline FusionPreset presetFusionZerene() {
    return { 1, 0.8, -1, false, false };
}

// Winnow baseline
inline FocusPreset presetFocusWinnow() {
    return { 0.0f, 0.0f, 1, true };
}
inline DepthPreset presetDepthWinnow() {
    return { 10, 10, 20, 10, false };
}
inline FusionPreset presetFusionWinnow() {
    return { 0, 0.0, -1, false, false };
}

} // namespace FSConst

#endif // FOCUSSTACKCONSTANTS_H
