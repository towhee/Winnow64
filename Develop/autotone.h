#ifndef AUTOTONE_H
#define AUTOTONE_H

#include <QImage>
#include "Develop/editparams.h"
#include "Develop/workingimage.h"

/*
    AUTO TONE -- Lightroom's Basic-panel "Auto": one click sets Exposure, Contrast,
    Highlights, Shadows, Whites, Blacks, Vibrance and Saturation from the image itself.

    Lightroom's own Auto is a trained network (Adobe Sensei, since Classic 7.1). This is
    the HEURISTIC equivalent: measure the picture, set each slider from a rule, in the
    order a person would. Every measurement is taken from a small proxy rendered through
    the REAL pipeline (WorkingImageCache::render, the same develop + view transform +
    transfer the loupe uses), so the sliders' nonlinearity, the view transform's shoulder
    and whatever else the image already carries (profile, WB, Texture, Dehaze...) are all
    accounted for without modelling any of them. A proxy render is a few milliseconds, so
    the solver can afford to search rather than predict.

    THE ORDER, each step measured on the render the previous steps produced:

      1. Exposure    bisection: the frame's log-average display-linear luminance (its
                     "key", centre-weighted) moves kKeyStrength of the way, in stops,
                     toward middle grey. Partial on purpose -- a full correction greys
                     out every low-key and high-key scene. The pull is halved again when
                     the frame already has a real bright end (brightening) or a real
                     dark end (darkening), the signature of a deliberately dark / bright
                     scene rather than a mis-exposed one.
      2. Contrast    rule: the spread (std. dev.) of display luma against kSdTarget.
      3. Highlights  rule: proportional to the fraction of the frame above kHiThresh.
         Shadows     rule: proportional to the fraction below kLoThresh.
      4. Whites      bisection: the kWhitePct luma percentile lands at kWhiteTarget.
         Blacks      bisection: the kBlackPct luma percentile lands at kBlackTarget.
                     Last, because they set the end points and everything above moves
                     them. This is Lightroom's Shift+double-click auto-clip.
      5. Vibrance /  rule: mean CIELAB chroma of the mid-tones against kChromaTarget,
         Saturation  mostly through Vibrance; capped when the frame is already near the
                     gamut edge, zero for a monochrome frame.

    The tone sliders and Vibrance / Saturation are reset before measuring, so Auto gives
    the same answer however the sliders were left. Everything else in `base` (profile,
    tone mapping, white balance, presence, effects) is kept and rendered with.

    All the tuning lives in the constants at the top of autotone.cpp. They are first
    guesses meant to be calibrated against Lightroom's Auto with the LR calibration loop
    (headless dump + developrender + lrcompare.py): Lightroom writes its Auto results into
    the XMP (crs:Exposure2012 ...), so its answers are free training labels.

    Global only: Auto measures the whole frame through scope 0. Masks are not rendered.
    Geometry is not applied either -- the measurement is of the uncropped frame.
*/
namespace AutoTone {

struct Result {
    float exposure   = 0.0f;    // EV, 2 dp
    float contrast   = 0.0f;    // the rest are integer slider values, -100..100
    float highlights = 0.0f;
    float shadows    = 0.0f;
    float whites     = 0.0f;
    float blacks     = 0.0f;
    float vibrance   = 0.0f;
    float saturation = 0.0f;
    int   renders    = 0;       // proxy renders the solve took (latency probe)
    /* Stage timings, ms (latency probe): the full-frame passes (proxy downscale, the
       slot's StatSample + LocalSample) and the proxy renders + measurements. */
    int   proxyMs    = 0;
    int   sampleMs   = 0;
    int   localMs    = 0;
    int   rendersMs  = 0;
};

/* What the solver reads off one rendered proxy. Exposed for the unit test. */
struct Stats {
    double hist[256] = {};      // BT.709 display luma (the histogram scope's)
    double total     = 0.0;
    float  key       = 0.0f;    // centre-weighted log-average display-LINEAR luminance
    float  mean      = 0.0f;    // display luma, 0..1
    float  sd        = 0.0f;    // display luma std. dev., 0..1
    float  meanChroma = 0.0f;   // CIELAB C* over the mid-tones (only when asked for)
    float  highChroma = 0.0f;   // its 98th percentile

    /* Display luma (0..1) below which a fraction q of the frame lies, interpolated
       within a bin so bisection sees a continuous target. */
    float percentile(double q) const;
    /* Fraction of the frame with display luma above / below v (0..1). */
    float fractionAbove(float v) const;
    float fractionBelow(float v) const;
};

/* Measure a rendered 8-bit image (Format_RGB888, as WorkingImageCache::render makes it).
   withChroma adds the CIELAB pass, which only the last step needs. */
Stats Measure(const QImage &img, bool withChroma);

/* Solve the Auto values for the pre-develop image `work` (the WorkingImageCache entry)
   rendered with `base`. False when the image is unusable or a render fails. */
bool Solve(const WorkingImage &work, const EditParams &base, Result &out);

/* What ApplyPresetAutos managed. A step that was not asked for reads as ok. */
struct AutosResult {
    bool wbOk   = true;
    bool toneOk = true;
    int  renders = 0;           // Auto tone's proxy renders (latency probe)
    Result tone;                // Auto tone's own result, its stage timings included
    bool ok() const { return wbOk && toneOk; }
};

/*
    A DEVELOP PRESET'S AUTO SETTINGS ("autoWhiteBalance" / "autoTone" in its Global
    group): unlike every other preset key they are COMMANDS, not values, so each image
    the preset lands on is solved from its own pixels instead of receiving the numbers
    the source image happened to get.

    `p` is the image's scope-0 params with the preset's literal values already merged,
    and is updated in place. Auto white balance runs FIRST: Auto tone measures through
    the render, and the balance moves every channel it measures. Its result is written
    as the WB row's own Auto pick does (temp / tint + wbPreset Auto). Auto tone then
    writes its eight sliders, overriding any the preset also carried. A step that cannot
    run (no camera characterisation, an unusable frame) leaves its fields as the merge
    left them and reports false. Pure: safe off the GUI thread.
*/
AutosResult ApplyPresetAutos(const WorkingImage &work, bool wb, bool tone, EditParams &p);

} // namespace AutoTone

#endif // AUTOTONE_H
