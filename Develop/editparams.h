#ifndef EDITPARAMS_H
#define EDITPARAMS_H

#include "Develop/tonecurve.h"

/*
    Parametric, non-destructive develop adjustments (Lightroom-style). One versioned
    struct, identity defaults, so an unset / absent EditParams is a no-op and changes
    nothing. This is the SINGLE source of truth for all adjustment values: every stage
    that needs an adjustment (the generic Develop processor, and for RAW the white-
    balance / denoise steps inside RawFormat) reads from this one struct.

    Persisted per-image (datamodel column + sidecar) and bound to the slider UI -- both
    deferred. See notes/Documentation.txt "DEVELOP / IMAGE EDIT".
*/
struct EditParams {
    /* White balance, ABSOLUTE (Lightroom-style), resolved against the source's own colour
       characterisation -- see Develop/whitebalance.h.

         temp  correlated colour temperature in KELVIN (2000..50000). The sentinel 0 means
               "as shot": the image has no white-balance edit, so it stays at its decoded
               balance and reads as identity. It does NOT mean 0 K.
         tint  green/magenta offset (-150..+150, positive = magenta), absolute like temp.

       The panel writes both together -- moving Tint alone still commits the resolved
       Kelvin -- so temp == 0 only ever means a pristine image and the pair can never
       disagree about their reference.

       wbPreset is the WB dropdown's selection (a WhiteBalance::Preset cast to int; 0 =
       As Shot). Kept only so the dropdown can show what the user picked: temp/tint above
       are what actually render, and any manual slider move switches this to Custom. */
    float temp = 0.0f;
    float tint = 0.0f;
    int   wbPreset = 0;

    /* Tone. All 0 = identity. */
    float exposure   = 0.0f;    // EV
    float contrast   = 0.0f;
    float highlights = 0.0f;
    float shadows    = 0.0f;
    float whites     = 0.0f;
    float blacks     = 0.0f;

    /* Tone-region split positions (perceptual 0..1), set by the histogram region slider. They
       move WHERE the shadows/highlights controls act and how far each reaches; blacks (0) and
       whites (1) stay pinned at the ends. Defaults 0.25/0.50/0.75 reproduce the fixed centres
       the tone curve used before, so they are a no-op until moved (and only matter when a tone
       slider is non-zero -- hence not part of isIdentity()). */
    float toneShadowCenter    = 0.25f;
    float toneCrossover       = 0.50f;
    float toneHighlightCenter = 0.75f;

    /* Tone curve (Curves panel) -- Lightroom's Tone Curve. FOUR independent point curves:
       channel 0 is the RGB composite (applied to every channel) and 1/2/3 are Red / Green
       / Blue (applied on top of the composite, to that channel alone). Control points are
       (x, y) in the PERCEPTUAL 0..1 domain, ordered by x with both endpoints pinned at
       x = 0 and x = 1 (y free, which is what gives the black-/white-point moves).

       Fixed-size arrays rather than a container so operator== stays defaulted, the struct
       stays trivially copyable for the per-render / per-history-snapshot copies, and
       EditStack::sanitizeParams stays purely numeric. curveN[c] == 2 with the diagonal
       is identity, and the defaults below are exactly that -- so an untouched image
       writes no sidecar. UNLIKE the tone splits above, the curve IS a primary control
       and so DOES count towards isIdentity(). Maths, repair and the shared string
       encoding are all in Develop/tonecurve.h. */
    int   curveN[ToneCurve::kChannels] = {2, 2, 2, 2};
    float curveX[ToneCurve::kChannels][ToneCurve::kMaxPts] =
        {{0.0f, 1.0f}, {0.0f, 1.0f}, {0.0f, 1.0f}, {0.0f, 1.0f}};
    float curveY[ToneCurve::kChannels][ToneCurve::kMaxPts] =
        {{0.0f, 1.0f}, {0.0f, 1.0f}, {0.0f, 1.0f}, {0.0f, 1.0f}};

    /* True when all four curves are the 2-point diagonal. */
    bool curvesAreIdentity() const {
        for (int c = 0; c < ToneCurve::kChannels; ++c)
            if (!ToneCurve::isIdentity(curveX[c], curveY[c], curveN[c])) return false;
        return true;
    }

    /* Presence. */
    float texture = 0.0f;
    float dehaze  = 0.0f;

    /* Colour -- RGB. Per-channel scene-linear gain (-100..100). Folded into the fused point
       pass's per-channel gain alongside white balance + exposure, so they cost nothing extra.
       0 = no change; positive lifts that channel, negative attenuates it. */
    float red   = 0.0f;
    float green = 0.0f;
    float blue  = 0.0f;

    /* Colour -- HSL (global, -100..100). Cross-channel point op applied after the tone
       curve in the same fused pass. hue = rotation about the neutral (gray) axis;
       saturation = chroma scale about luma (-100 -> grayscale, +100 -> 2x); vibrance =
       saturation weighted by how muted each pixel already is (boosts low-chroma pixels,
       protects saturated ones); luminance = uniform brightness gain. */
    float hue        = 0.0f;
    float saturation = 0.0f;
    float vibrance   = 0.0f;
    float luminance  = 0.0f;

    /* Camera calibration (Calibrate panel) -- rotates the three PRIMARIES of the working
       space: each of R/G/B gets a hue shift (-100..100 -> +/-30 deg about the neutral
       axis) and a saturation scale (-100..100 -> chroma x0..x2 about the primary's own
       luma). Built into one 3x3 matrix whose columns are the re-pointed primaries, so
       neutrals stay neutral. Applied in LINEAR light before the tone curve -- distinct
       from the Color panel's red/green/blue, which are per-channel gains folded into
       white balance, and from its HSL hue, which runs after the curve. Math is
       header-only in Develop/calibrate.h. Identity = all six zero. */
    float calRedHue   = 0.0f, calRedSat   = 0.0f;
    float calGreenHue = 0.0f, calGreenSat = 0.0f;
    float calBlueHue  = 0.0f, calBlueSat  = 0.0f;

    /* Colour grading (Color Grade panel) -- tonal-range tinting, the Lightroom "teal
       shadows / orange highlights" look. Three ranges (shadows / midtones / highlights);
       each ADDS a chroma tint of the given hue at the given saturation and NUDGES that
       range's luminance. Applied as a point op after HSL in the fused pass, weighted by
       smooth per-pixel tonal windows. hue is 0..360 (degrees), sat 0..1 (0 = no tint, so
       hue is irrelevant there), lum -100..100. Identity = every sat and lum 0. */
    float gradeShadowHue = 0.0f, gradeShadowSat = 0.0f, gradeShadowLum = 0.0f;
    float gradeMidHue    = 0.0f, gradeMidSat    = 0.0f, gradeMidLum    = 0.0f;
    float gradeHighHue   = 0.0f, gradeHighSat   = 0.0f, gradeHighLum   = 0.0f;

    /* A fourth grading range that is NOT tone-selective: Global applies its tint and
       luminance to every pixel at full weight, on top of the three windowed ranges. */
    float gradeGlobalHue = 0.0f, gradeGlobalSat = 0.0f, gradeGlobalLum = 0.0f;

    /* Shape of the three tonal windows, panel-wide rather than per-range.
       gradeBlending (0..100, default 50) widens or narrows the OVERLAP between shadows
       and highlights: 50 reproduces the fixed split the panel had before these existed,
       0 pulls them apart (a broader pure-midtone band), 100 pushes them together.
       gradeBalance (-100..100, default 0) slides the whole split -- positive favours the
       highlight range, negative the shadow range. Both are inert while every range is at
       sat/lum 0, so neither is part of isIdentity. */
    float gradeBlending = 50.0f;
    float gradeBalance  = 0.0f;

    /* Noise reduction -- GLOBAL, applied in the raw decode pipeline (RawFormat / the Apple
       engine) during demosaic, alongside start WB / black / white. Not maskable: these are
       baked into WorkingImage before Develop runs. 0 = engine default. */
    /* Default color (chroma) NR is full: chroma noise is objectionable and chroma
       detail is low-frequency, so baking 100 at decode is the right baseline.
       Default luma NR is 0.75: strong enough to clean sensor grain while leaving
       fine detail intact. A fresh raw carrying these defaults still counts as
       identity (see isIdentity); a saved recipe overrides them per image. */
    static constexpr float kDefaultDenoiseLuma   = 0.75f;
    static constexpr float kDefaultDenoiseChroma = 1.0f;
    float denoiseLuma   = kDefaultDenoiseLuma;
    float denoiseChroma = kDefaultDenoiseChroma;

    /* Local (maskable) NR -- Develop SPATIAL ops layered on TOP of the global baseline, operating
       on the already-decoded WorkingImage (see Develop::Denoise and notes/Documentation.txt
       "Scope & masking model"). localDenoiseLuma = luminance NR (ratio-preserving); localDenoise-
       Chroma = colour/chroma NR (opponent-chroma blur, luma kept exact). 0 = off. Range 0..1. */
    float localDenoiseLuma = 0.0f;
    float localDenoiseChroma = 0.0f;

    /* Sharpen (Detail SPATIAL op, runs after Vignette and before Grain -- so film grain
       is never itself sharpened and the vignette's darkened corners do not get
       amplitude-boosted detail). Capture sharpening: an unsharp mask on perceptual
       luminance, ratio-preserving like Texture and Denoise.

       This is the ONLY Develop op with an ABSOLUTE pixel radius. Every other spatial op
       scales its radius to max(w,h) so the proxy preview matches the full-res render;
       acutance is a property of the pixel grid, so sharpening cannot. It reads
       WorkingImage::renderScale instead and is judged at 1:1 -- see Develop/sharpen.h.

       Distinct from Texture, which is a resolution-PROPORTIONAL mid-frequency band
       (~13 px on an 8640 px edge, ungated): the two occupy disjoint bands and stack.

         sharpenAmount   0..1.5  strength. 0 = off. The primary control (isIdentity).
         sharpenRadius   0.5..3  unsharp radius in PIXELS at full resolution.
         sharpenDetail   0..1    how much of the finest band survives: low soft-thresholds
                                 the smallest amplitudes away (treats them as noise), high
                                 passes everything through (more bite, more halo).
         sharpenMasking  0..1    edge gate. 0 sharpens everything, 1 edges only -- this
                                 is what stops sharpening amplifying sky/skin noise.

       Radius / detail / masking are inert while sharpenAmount is 0, so they are NOT part
       of isIdentity() -- same rule as vignetteFeather and grainSize. There is no
       negative amount ("soften"): negative Texture and Denoise already cover it. */
    float sharpenAmount  = 0.0f;
    float sharpenRadius  = 1.0f;
    float sharpenDetail  = 0.25f;
    float sharpenMasking = 0.0f;

    /* Vignette (Effects SPATIAL op, runs last). A radial exposure falloff about the image
       centre: vignetteExposure is the EV applied at the corners (negative darkens = the
       classic vignette, positive brightens), ramping smoothly to 0 at the centre.
       vignetteFeather (0..1) shapes the falloff -- high spreads the effect inward
       (gradual), low concentrates it in the corners. For anything off-centre or shaped,
       the user paints radial masks instead, so these two sliders are all the global
       vignette needs. Feather is not part of isIdentity (it only matters when
       vignetteExposure is non-zero). */
    float vignetteExposure = 0.0f;
    float vignetteFeather  = 0.5f;

    /* Grain (Effects SPATIAL op, runs last -- after Vignette). Monochromatic film grain
       added to luminance (ratio-preserving), most visible in the midtones and fading
       toward black / white. grainAmount (0..1) is the strength; grainSize (0..1) the
       particle size, scaled to the image so the proxy preview matches full res;
       grainRoughness (0..1) the irregularity of the grain amplitude (0 = uniform). Size
       and roughness only matter when grainAmount is non-zero, so they are not part of
       isIdentity(). */
    float grainAmount    = 0.0f;
    float grainSize      = 0.25f;
    float grainRoughness = 0.5f;

    int version = 1;

    /* Adjustment groups, matching the Develop panel's section headers (Basic / Color / Effects).
       These drive the per-section Preview (show/ignore) and Reset (restore defaults) controls: a
       group is a fixed subset of the fields above. Note the grouping follows the UI, not the field
       comments -- texture/dehaze sit under Basic, and the tone splits belong to Basic for
       Reset (they are irrelevant to Preview since the tone sliders they modulate are zeroed anyway).
       localDenoiseLuma ("Denoise", local post-demosaic NR) is under Detail. denoiseLuma/denoiseChroma
       are decode-time global NR (the Global scope's "Denoise raw", baked before Develop runs) so they
       are in NO group and cannot be previewed/reset via params. */
    /* ColorGrade = the nine colour-grading fields, Calibrate = the six primary fields;
       each is its own group so its panel's Preview/Reset are independent of the Color
       panel's RGB/HSL group. */
    /* Detail = sharpening + the local (post-demosaic) NR pair. The two fight each other
       -- NR removes the detail sharpening amplifies -- so they share a panel, as they do
       in Lightroom. localDenoise* moved here from Effects; the saved preset format is
       unaffected (presets store a flat map of field names, section titles only group the
       checklist UI), but a scope's showDetail rather than showEffects now gates them. */
    enum class Group { Basic, Curves, Color, Calibrate, ColorGrade, Detail, Effects };

    /* Force one group's fields back to their identity defaults, in place. The defaults come from a
       fresh EditParams{} so the non-zero tone-split defaults (0.25/0.50/0.75) restore correctly.
       Reset calls this on the STORED params (destructive); the Preview fold calls it on a COPY. */
    static void resetGroup(EditParams &p, Group g) {
        const EditParams def;
        switch (g) {
        case Group::Basic:
            p.temp = def.temp; p.tint = def.tint; p.wbPreset = def.wbPreset;
            p.exposure = def.exposure; p.contrast = def.contrast;
            p.highlights = def.highlights; p.shadows = def.shadows;
            p.whites = def.whites; p.blacks = def.blacks;
            p.texture = def.texture; p.dehaze = def.dehaze;
            p.toneShadowCenter = def.toneShadowCenter;
            p.toneCrossover = def.toneCrossover;
            p.toneHighlightCenter = def.toneHighlightCenter;
            break;
        case Group::Curves:
            /* Every channel back to the diagonal. The tone splits are NOT reset here:
               they are shared with Basic's shadows/highlights (the Curves panel only
               mirrors them), and Basic owns them for Reset. */
            for (int c = 0; c < ToneCurve::kChannels; ++c)
                ToneCurve::setIdentity(p.curveN[c], p.curveX[c], p.curveY[c]);
            break;
        case Group::Color:
            p.red = def.red; p.green = def.green; p.blue = def.blue;
            p.hue = def.hue; p.saturation = def.saturation;
            p.vibrance = def.vibrance; p.luminance = def.luminance;
            break;
        case Group::Calibrate:
            p.calRedHue = def.calRedHue;     p.calRedSat = def.calRedSat;
            p.calGreenHue = def.calGreenHue; p.calGreenSat = def.calGreenSat;
            p.calBlueHue = def.calBlueHue;   p.calBlueSat = def.calBlueSat;
            break;
        case Group::ColorGrade:
            p.gradeShadowHue = def.gradeShadowHue; p.gradeShadowSat = def.gradeShadowSat;
            p.gradeShadowLum = def.gradeShadowLum;
            p.gradeMidHue = def.gradeMidHue; p.gradeMidSat = def.gradeMidSat;
            p.gradeMidLum = def.gradeMidLum;
            p.gradeHighHue = def.gradeHighHue; p.gradeHighSat = def.gradeHighSat;
            p.gradeHighLum = def.gradeHighLum;
            p.gradeGlobalHue = def.gradeGlobalHue;
            p.gradeGlobalSat = def.gradeGlobalSat;
            p.gradeGlobalLum = def.gradeGlobalLum;
            p.gradeBlending = def.gradeBlending; p.gradeBalance = def.gradeBalance;
            break;
        case Group::Detail:
            p.sharpenAmount = def.sharpenAmount;             // "Sharpening" (capture USM)
            p.sharpenRadius = def.sharpenRadius;
            p.sharpenDetail = def.sharpenDetail;
            p.sharpenMasking = def.sharpenMasking;
            p.localDenoiseLuma = def.localDenoiseLuma;       // "Denoise" (local NR)
            p.localDenoiseChroma = def.localDenoiseChroma;   // "Denoise Color" (chroma)
            break;
        case Group::Effects:
            p.vignetteExposure = def.vignetteExposure;       // "Vignette" (radial)
            p.vignetteFeather = def.vignetteFeather;
            p.grainAmount = def.grainAmount;                 // "Grain" (film grain)
            p.grainSize = def.grainSize;
            p.grainRoughness = def.grainRoughness;
            break;
        }
    }

    /* Field-wise equality (all members are scalars), used by groupIsDefault below. */
    bool operator==(const EditParams &) const = default;

    /* True when every field in one group still holds its identity default, i.e. the
       panel has no edits. Derived from resetGroup on a copy so it can never drift out
       of step with the group membership above: resetGroup touches only that group's
       fields, so an unchanged copy means the group is pristine. The Develop panel uses
       this to flag edited sections in their header ("Basic *"). */
    static bool groupIsDefault(const EditParams &p, Group g) {
        EditParams q = p;
        resetGroup(q, g);
        return q == p;
    }

    /* True when nothing would change, letting callers skip the Develop stage and
       serve the cached WorkingImage directly. */
    bool isIdentity() const {
        return temp == 0.0f && tint == 0.0f &&
               exposure == 0.0f && contrast == 0.0f &&
               highlights == 0.0f && shadows == 0.0f &&
               whites == 0.0f && blacks == 0.0f &&
               texture == 0.0f && dehaze == 0.0f &&
               red == 0.0f && green == 0.0f && blue == 0.0f &&
               calRedHue == 0.0f && calRedSat == 0.0f &&
               calGreenHue == 0.0f && calGreenSat == 0.0f &&
               calBlueHue == 0.0f && calBlueSat == 0.0f &&
               hue == 0.0f && saturation == 0.0f && vibrance == 0.0f && luminance == 0.0f &&
               gradeShadowSat == 0.0f && gradeShadowLum == 0.0f &&
               gradeMidSat == 0.0f && gradeMidLum == 0.0f &&
               gradeHighSat == 0.0f && gradeHighLum == 0.0f &&
               gradeGlobalSat == 0.0f && gradeGlobalLum == 0.0f &&
               denoiseLuma == kDefaultDenoiseLuma && denoiseChroma == kDefaultDenoiseChroma &&
               localDenoiseLuma == 0.0f && localDenoiseChroma == 0.0f &&
               sharpenAmount == 0.0f &&
               vignetteExposure == 0.0f && grainAmount == 0.0f &&
               curvesAreIdentity();
    }
};

#endif // EDITPARAMS_H
