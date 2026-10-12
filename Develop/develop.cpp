#include "Develop/develop.h"
#include "Develop/cameraprofile.h"
#include "Develop/huesatmap.h"
#include "Develop/outputlook.h"
#include "Develop/baselineexposure.h"
#include "Develop/colorspace.h"
#include "Develop/whitebalance.h"
#include "Develop/calibrate.h"
#include "Develop/colorgrade.h"
#include "Develop/tonecurve.h"
/* DisplayPosition: the tone regions are placed by handles drawn on the histogram, so
   their weights are evaluated on the display axis rather than on the working value. */
#include "Develop/outputtransform.h"
#include "Develop/sharpen.h"
#include "Develop/localcontrast.h"
#include <QtConcurrent>
#include <QThreadPool>
#include <QElapsedTimer>
#include <QFuture>
#include <QVector>
#include <QtGlobal>
#include <QDebug>
#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <vector>

/* Luma weights for the WORKING space, folded at compile time from
   ColorSpaceMath::kWorking. These used to be the literals 0.2126 / 0.7152 / 0.0722
   written out at every site; they now follow the working space automatically, so
   widening it does not silently leave the ops weighting the wrong primaries. */
using ColorSpaceMath::kLumR;
using ColorSpaceMath::kLumG;
using ColorSpaceMath::kLumB;

namespace {
/* Contrast is applied in a perceptual (~sRGB gamma) domain so the slope steepens the tone
   curve evenly instead of exploding scene-linear highlights. kGamma/kInvGamma encode/decode;
   kContrastSlopeRange keeps the control gentle (full slider = 1 +/- 0.4 slope). */
constexpr float kGamma             = 2.2f;
constexpr float kInvGamma          = 1.0f / kGamma;
constexpr float kContrastSlopeRange = 0.4f;
/* The contrast slider is an integer -100..100 control (see DevelopProperties::addBasic),
   so normalise to the -1..1 "amount" the slope math is designed around. */
constexpr float kContrastFullScale  = 100.0f;

/* White balance is a per-channel gain in SCENE-LINEAR (the physically correct domain: it
   rescales the light before any tone curve). The gains are ABSOLUTE: p.temp is a Kelvin
   temperature and p.tint a green/magenta offset, resolved against the image's own colour
   characterisation (WorkingImage::cam) by WhiteBalance::relativeGains -- so "5500 K"
   means the same light on every camera body. See Develop/whitebalance.h. */

/* Colour -- RGB sliders (red/green/blue). Per-channel scene-linear gain, normalised -1..1 by
   kRgbFullScale and folded into the same per-channel factor as WB + exposure (free; no extra
   pass). Gentle creative range: full slider = +/-50% on that channel. */
constexpr float kRgbFullScale = 100.0f;
constexpr float kRgbGain      = 0.5f;

/* Colour -- HSL sliders (hue/saturation/luminance), normalised -1..1 by kHslFullScale. Hue is a
   rotation about the neutral (1,1,1) axis: full slider = +/-kHueMaxRad. Saturation scales chroma
   about Rec.709 luma: -1 -> grayscale, +1 -> 2x. Luminance is a uniform gain: -1 -> black,
   +1 -> 2x. Applied as a cross-channel block after the tone curve (see applyPointOps). */
constexpr float kHslFullScale = 100.0f;
constexpr float kHueMaxRad    = 1.04719755f;   // 60 degrees at full slider

/* Colour grading (Color Grade panel): the whole model -- push space, windows, strength,
   Luminance, Blending / Balance -- is the measured Lightroom one in Develop/colorgrade.h.
   The Luminance slider's -100..100 is normalised by this. */
constexpr float kGradeLumFullScale = 100.0f;

/* Tone controls (highlights/shadows/whites/blacks), applied after contrast (pipeline order
   #5), all in the PERCEPTUAL (gamma) domain. Sliders are integer -100..100 (normalised to
   +/-1 by kToneFullScale); positive always brightens, negative darkens, like Lightroom. The
   table is built over up to ~3 stops above white (kToneLutMaxN) so highlight headroom is
   shaped rather than clipped.

   THEY ARE TWO DIFFERENT KINDS OF CONTROL, and conflating them is what made Winnow's tone
   sliders behave unlike Lightroom's. Whites and Blacks are ENDPOINT MOVES -- they rescale
   where the curve reaches white and black, so everything between moves with them. Shadows
   and Highlights are REGION LIFTS -- weighted shifts of a band, which leave the endpoints
   alone. All four used to be additive Gaussian lifts, two of them pinned at s=0 and s=1,
   which is a bump rather than an endpoint move: a bump brightens a BAND near the end and
   can never move the end itself. That is why Lightroom's Whites pushed the histogram's
   right edge and Winnow's only brightened, and why Whites moved mid-grey by 0 levels of
   255 where an endpoint move moves it by tens. See tst_toneregions, which pins the KIND of
   operator each control is, and the tone-region note in notes/Documentation.txt. */
constexpr float kToneFullScale    = 100.0f;
constexpr float kToneLutMaxN      = 8.0f;   // build the curve over linear n in [0, 8] (~3 stops)

/*
    THE FOUR TONE OPERATORS, FITTED TO LIGHTROOM (2026-09-17).

    MEASURED, NOT CHOSEN. A single raw was exported from Lightroom at 17 settings -- each
    of blacks / shadows / highlights / whites at -100, -50, +50, +100, plus an all-zero
    control -- and from Winnow at the same 17. Histogram-matching the two zero renders
    recovers the baseline difference (a different camera profile and view transform) as a
    per-pixel curve and divides it out; the two renders agree to 0.70 levels of 255 after
    it. Histogram-matching each slider render against its own zero render then recovers
    THAT SLIDER'S OPERATOR as an explicit curve, with no model assumed. The constants below
    are fitted to those curves and the fit is 2.56 levels of 255 RMS overall, worst
    operator 3.85. Method and harness: notes/Documentation.txt, the tone-region note.

    THEY ARE APPLIED ON THE DISPLAY AXIS, which is why the amplitudes read as fractions of
    the visible range rather than of anything internal. That is the axis Lightroom's curves
    were measured on and the axis the ToneRegionSlider handles are drawn on; see
    applyToneShape.

    THE MODEL FAMILIES ARE NOT UNIFORM, AND THAT IS THE FINDING. Lightroom's endpoint
    controls are endpoint moves in the direction that CLIPS and gentle band lifts in the
    direction that cannot:

        Blacks -    black point raise   crushes; clips at 0
        Blacks +    band lift           opens the shadows; nothing to clip against
        Whites +    white point lower   brightens; clips at white
        Whites -    band pull           recovers; nothing to clip against
        Shadows +/- gain        multiplies display-linear light (refit 2026-10-05)
        Highlights +/- band

    Modelling all four as one kind of operator is what the first attempt did -- all four as
    Gaussian bumps, then all four as affine endpoint moves -- and both were measurably
    wrong in the same way: an operator that is symmetric in sign cannot reproduce a control
    whose two directions do different things. Blacks +100 lifts by at most 26 levels while
    Blacks -100 crushes everything below level 48 to black.

    THE EXPONENTS ARE NOT COSMETIC. Lightroom's response is not linear in the slider: the
    ratio between the +100 and +50 curves runs from 1.8 (highlights) to 3.2 (whites) where
    linear would be 2.0. Fitting amplitude alone left several operators right at one end of
    their travel and wrong at the other.
*/
struct ToneOp {
    float amp;          // full-slider strength, as a fraction of the display range
    float expo;         // response exponent: shift scales with amount^expo
    float lo, peak, hi; // band support and peak, on the display axis (endpoint ops: unused)
};

/* Bands. */
constexpr ToneOp kBlacksUp    = {0.12f, 1.3f, 0.00f, 0.10f, 1.00f};
constexpr ToneOp kHighsUp     = {0.20f, 0.8f, 0.10f, 0.78f, 1.00f};
constexpr ToneOp kHighsDown   = {0.117f, 1.0f, 0.05f, 0.92f, 1.00f};  // amp: see below
constexpr ToneOp kWhitesDown  = {0.10f, 0.9f, 0.15f, 0.94f, 1.00f};

/*
    HIGHLIGHTS - IS IMAGE-ADAPTIVE (refit 2026-10-10). Lightroom Classic exports of two
    raws -- the ILCE-1 above, and an E-M1 snow scene at Exposure 0 and +1.5 -- show ONE
    band shape (kHighsDown: up from 0.05, peak 0.92, down to white) times a strength that
    differs by image: 0.28 on the dark ILCE-1 frame, 0.08 on the snow at Exposure 0, 0.05
    on the snow at +1.5. The 0.22 fixed amp fitted on the ILCE-1 alone was 15.9 levels RMS
    off on the snow and 35.3 at +1.5, where it pulled the median down 52 levels to
    Lightroom's 11. Linear in the slider (-50 is half of -100), and close to 1-D WITHIN a
    frame (the surround at any radius adds at most 0.1 of variance explained) -- so the
    adaptation is one number per image, not a local operator.

    THE NUMBER IS A BUDGET. strength x the band's mean weight over the frame's (per
    channel) histogram came out at 8.0 / 11.5 / 9.3 levels: Lightroom spreads a roughly
    fixed average pull over whatever is bright, so a dark frame's few highlights take a
    hard pull and a snow field a gentle one. One shared budget, kHighsDownBudget, predicts
    all five Lightroom curves at 0.75-3.3 levels RMS, and each image held out of the fit
    is still predicted at 1.5-4.8. kHighsDownAmpMax keeps a near-black frame (mean weight
    ~0) from asking for an unbounded pull. kHighsDown.amp is what is used with no frame to
    measure -- the Curves panel's Parametric plot -- and is the strength for a frame whose
    mean weight is 0.30.

    The mean weight is measured on the frame's StatSample (Develop::Apply), never on the
    buffer being rendered: a proxy, the settle render and a Detail patch (a crop) must
    all draw the same curve. Highlights + is not refitted (no data) and stays fixed.
*/
constexpr float kHighsDownBudget = 0.017f;   // mean display shift at -100 (0.035 before
                                             // the frame-relative term, 0.020 before its
                                             // range scaling; both below)
constexpr float kHighsDownAmpMax = 0.60f;   // was 0.35; see THE STRENGTH below

/*
    HIGHLIGHTS - IS ALSO LOCAL, AND A PUSH RAISES ITS BUDGET (refit 2026-10-11, against
    Lightroom Classic exports of an OM-1 forest frame -- a sunlit fern pool in dark woods
    -- at Exposure +2.5, Highlights 0 / -50 / -100, plus the three frames above).

    The "close to 1-D" finding above holds for the frames it was measured on, but not for
    a small bright REGION in a dark frame. There Lightroom pulls the whole pool down as
    one piece, 1-1.5 stops at -100: the dark gaps between fronds go down with the bright
    fronds, trunks standing in front of the pool are left alone, nothing haloes, and fine
    contrast inside the pool RISES. The pixel's own level explained 41% of its gain.

    THE OPERATOR IS A LOCAL LAPLACIAN FILTER (Paris, Hasinoff & Kautz 2011, in the fast
    form of Aubry et al. 2014), carrying the fitted band in its remapping function: at
    each pyramid level a tone within kHighsLocalSigma of the local reference g takes the
    band's shift AT g (so a region moves as a region and its detail rides along), and a
    tone further from g takes its OWN shift (so an edge is per pixel and cannot halo). It
    runs on the band's own axis (d0) for luminance, on the frame's ~1024 px LocalSample;
    what it adds beyond the per-pixel band is guided-upsampled to the render and added to
    each channel's own per-pixel shift (Develop::buildHiField, PointCoeffs::hiShiftByPos).
    One field per frame, so a proxy, the settle render, an export and a Detail patch (a
    crop, WorkingImage::frameX..) agree.

    REPLACED A GUIDED-FILTER REGION (same day, shipped as v4 and pulled): read on a 128 px
    grid, its region estimate leaked across the trunks, and bright fronds beside them were
    judged to be in a dark region and left unpulled -- a milky halo the RMS fit could not
    see. Scored with a halo term (pull on bright pixels 2-10 px from dark objects, vs
    Lightroom): guided 128 px +12.8 levels short, guided at full resolution +9.1, this
    filter -0.6 at the 1024 px sample, upsampled. RMS against Lightroom (forest -100 /
    -50, snow +1.5, snow 0, ILCE -100), per-pixel band -> this: 15.3 / 5.5 / 3.5 / 5.4 /
    4.3 -> 11.2 / 4.7 / 3.2 / 5.2 / 4.0.

    THE STRENGTH IS THE OPEN QUESTION. Fitted through the real pipeline (developrender
    on calibration dumps of three raws, scored on each app's pull against its own
    Highlights 0 render): Lightroom's mean pull over the frame at -100 is a steady 8-12
    levels on four renders and 18.9 on the forest frame. A second pushed frame (ILCE at
    +1.5) sits at 9.9, so the push does NOT explain the forest -- it is the only very
    high-contrast scene in the set (38% near black, 14% bright), which one frame cannot
    establish.
    kHighsDownBudgetPerEV = 0.3 with kHighsDownAmpMax raised 0.35 -> 0.60 is the
    compromise: mean RMS over eight renders 6.26 -> 5.85, forest -100 16.1 -> 11.2, at
    +0.8 levels on ILCE +1.5 and +1.5 on snow +1.5; Exposure-0 frames unchanged. A plateau
    above the band's peak and a steeper slider exponent were tried and bought 0.04.
*/
constexpr float kHighsLocalSigma      = 0.07f;   // detail/edge split, display units of d0
constexpr int   kHighsLocalLevels     = 12;      // reference levels (fast local LLF)
constexpr int   kHighsLocalLong       = 1024;    // LocalSample long edge
constexpr int   kHighsLocalUpRadius   = 2;       // guided upsample radius, sample px
constexpr float kHighsLocalUpEps      = 1e-4f;   // guided upsample regulariser
constexpr float kHighsDownBudgetPerEV = 0.3f;    // budget x (1 + this x max(0, Exposure))

/*
    HIGHLIGHTS - ALSO PULLS BY HOW FAR A REGION STANDS ABOVE THE FRAME (2026-10-11).
    Measured in log luminance, Lightroom's -100 removes a FRACTION of each tone's distance
    above the frame's log-mean: 0.57 of it on the forest frame at Exposure 0 (81% near
    black, a lit fern pool), 0.32 on ILCE, 0.23 on snow at +1.5 -- the further the
    highlights stand above the frame, the harder the pull. The display-axis band cannot
    see that: it pulls a level the same in every frame, so on the dark forest it pulled
    level 32-96 tones by 6-27 levels where Lightroom pulls 24-50.

    So a second term rides on the band: a gain in display-linear light of 2^-(amount^q *
    k * (L - anchor)^p), L the log2 display-linear luminance of the band-free DISPLAY
    value (one value on a clipped span must get one gain, or Whites + inverts) and anchor
    the frame's log-mean of it (from the StatSample, so every render shares it), tapered
    to nothing at white -- so white is left to the recovery stage, and -50 leaves part of
    a push blown, as Lightroom's does (0.51% -> 0.14% -> 0% blown at 0 / -50 / -100). It
    is carried BY REGION, through a second local Laplacian on L (buildHiField): applied
    per pixel it compressed texture by a third, since a brighter pixel took more drop
    than its neighbours. The band's budget drops to make room. q > 1 because Lightroom's
    -50 is well under half its -100 on bright tones.

    Fitted through the real pipeline (tests/calibration/lrcompare.py, nine Lightroom
    pairs, coordinate search): mean RMS 6.56 -> 6.26; forest 0 12.2 -> 10.1, ILCE +1.5
    7.2 -> 6.5, snow +1.5 5.2 -> 3.8; ILCE 0 5.5 -> 6.1 and snow 0 7.0 -> 8.1 pay for it.
    A taper starting below 0.92 or leaving pull at white scored up to 0.2 better but
    broke the white / partial-recovery / no-compounding properties above. Applying the
    curve only to what a region adds beyond the pixel (tried first) scored 6.82.
*/
/*
    ...AND HARDER THE FURTHER THE FRAME'S HIGHLIGHTS STAND ABOVE IT (2026-10-11). With ten
    more Lightroom pairs at Exposure 0 (17 frames: forest, beach, snow, portraits, birds,
    a sunset, the sun in frame) one shared shape times a per-frame strength still explained
    Lightroom's gain, but the strength ran from 0.4 (snow) to 3.2 (a sunset band over a
    dark field) -- and the best single predictor (R2 0.72 of log strength) was the frame's
    RANGE above its anchor, the 99th percentile minus the log-mean, in stops. Lightroom
    also darkens a high-range frame as a whole: at the anchor itself -1.2 stops on the
    forest at 0 and -2.6 on the sunset, against -0.2 on snow and portraits.

    So the term is amount^q * e^(b * (range - r0)) * (k * (L - anchor)^p + o), still
    tapered to nothing at white, the o part the whole-frame darkening (it rides on every
    tone, so it lowers a dark frame's shadows a little in display terms and its mids
    more). Fitted through the real pipeline on all 19 lrcompare pairs (coordinate search,
    after the registration fix -- Lightroom corrects lens distortion, which had
    misaligned seven of them): mean RMS 6.02 -> 4.76; forest 0 8.4 -> 3.4, forest +2.5
    10.6 -> 8.3, snow 0 7.4 -> 5.2, sunset 14.7 -> 7.2; worst cost ILCE +1.5 5.7 -> 7.1
    and the flat seascape 1.7 -> 3.5. sigma 2.5 scored 0.03 better but haloed a pool edge
    by 2 levels (tst_toneregions highlightsPullsTheRegion); 1.9 is the largest that does
    not. Still short: the forest at +2.5. Lightroom compresses its tones almost evenly
    (baseline 75 -> 63, 200 -> 114, 225 -> 133); this pulls 75 -> 58 but 200 -> 127 and
    225 -> 150, so the sunlit ferns stay bright. Not the taper (starting it at 0.99 moved
    that frame 8.33 -> 8.10), and a larger per-EV budget fixed the top but over-pulled the
    mids and the other two pushes. Note Lightroom's push + -100 renders that frame's
    bright tones 30-50 levels DARKER than its unpushed, unedited render -- which
    tst_toneregions highlightsRecoveryDoesNotCompound forbids; that held on snow only.
*/
constexpr float kHighsAnchorK         = 0.16f;   // stops removed per stop^p above anchor
constexpr float kHighsAnchorP         = 1.125f;  // power on the distance above anchor
constexpr float kHighsAnchorO         = 0.30f;   // stops removed from every tone (whole frame)
constexpr float kHighsAnchorRangeB    = 0.35f;   // strength x e^(b (range - r0))
constexpr float kHighsAnchorRange0    = 4.5f;    // range (stops, p99 - anchor) where x1
constexpr float kHighsAnchorQ         = 1.5f;    // power on the slider amount
constexpr float kHighsAnchorTaper     = 1.0f;    // share of the term gone at white
constexpr float kHighsAnchorTaperFrom = 0.92f;   // display d where the taper starts
constexpr float kHighsAnchorSigma     = 1.9f;    // its region split, stops of L

/*
    SHADOWS IS A GAIN, NOT A BAND (refit 2026-10-05 against Lightroom Classic exports of
    one ILCE-1 raw at Shadows -100/-50/0/+50/+100, 16-bit sRGB).

    The band above it replaced was an additive display-space lift peaking at level 5, and
    adding a constant to the deepest tones is a black-point raise: at +100 the darkest 1%
    rose from level 5 to 35 while the contrast between dark tones FELL. Lightroom does the
    opposite. Per pixel, its change is a gain on display-linear light (sRGB-decoded), the
    same in stops for -100 as for +100 mirrored, and linear in the slider (+-50 is half of
    +-100 in stops). Black stays black because 0 times any gain is 0, and detail inside
    shadow regions GROWS, by 43% at +100, because the tones in them are scaled apart rather
    than shifted together.

    The gain is 2^(amount * kShadowGainStops * w(d0)), where w is 1 at black, decays
    quickly (kShadowGainDecay, an e-folding length on the display axis) and keeps a slow
    tail (kShadowGainTail of the full weight) that smoothsteps out at kShadowGainEnd.
    Lightroom's measured gain at +100: +2.6 stops at level 2, +1.2 at 18, +0.5 at 42,
    +0.15 at 74, +0.04 at 146, 0 by ~190. The fit is 0.31 levels of 255 RMS across all four
    settings, worst point 1.35; the band it replaced was 3.9-14.3 RMS, worst 23.

    WHAT IT IS NOT: a local operator. Lightroom's gain is only slightly better predicted by
    an 8 px neighbourhood than by the pixel itself (82% vs 78% of variance, falling away
    at larger radii), so a 1-D curve captures nearly all of it. Monotone at both ends of
    the slider for the default handles: the composed curve's minimum slope is 0.79 of the
    input's at +100 and 0.20 at -100.
*/
constexpr float kShadowGainStops = 2.34f;   // full-slider gain at black, in stops
constexpr float kShadowGainDecay = 0.089f;  // e-folding length of the fast part (display)
constexpr float kShadowGainTail  = 0.058f;  // share of the weight in the slow tail
constexpr float kShadowGainEnd   = 0.874f;  // where the tail reaches zero (display)

/*
    HIGHLIGHTS - ALSO RECOVERS WHAT EXPOSURE PUSHED PAST WHITE (2026-10-10). The band
    above cannot reach it: it lives on the display axis, which clamps at white, so every
    value pushed past the view's clip sits at d0 = 1, where the band's weight is zero,
    although the data under it is intact. Push Exposure +3 and the top of the frame is
    255 at any Highlights setting.

    So Highlights - with a positive Exposure runs a second, scene-linear stage BEFORE
    everything else in applyToneShape: in x = log2(n), identity below a knee, then
        x' = knee + L (1 - exp(-(x - knee) / L))
    which leaves the knee with slope 1 (no crease) and is monotone everywhere. L is solved
    so the BRIGHTEST VALUE THE PUSH CAN HAVE CREATED -- the view's clip point times 2^e,
    i.e. an original whose top sat right at the clip -- lands on kRecoverDisplay (level
    250) at -100, and halfway in stops at -50. The knee sits at most kRecoverKneeMax
    below the landing point, so the stage only ever reshapes the shoulder; the band does
    the rest, as it does at Exposure 0.

    IT NEEDS NO IMAGE, and that is the second version of that decision. The first
    MEASURED the frame's top, and the measurement differed by render (proxy, full res, a
    Detail patch is a crop, a mask scope develops from the accumulator) -- so each would
    have drawn a different curve. A third version undid the push in the highlights
    outright (back to their Exposure-0 tones) and was rejected in an in-app trial: on a
    snow scene the un-push and the band both pulled the same tones, the interquartile
    range fell from 44 levels to 9 and the frame went flat grey. Recovery has to stop at
    "no longer blown", not "unexposed".
*/
constexpr float kRecoverDisplay     = 0.98f;   // where the pushed top lands at -100
constexpr float kRecoverClipDisplay = 0.998f;  // at or past this the view has clipped
constexpr float kRecoverKneeMax     = 1.0f;    // knee: stops below the landing, at most

/* Endpoint moves. R is how far full slider drags the point, on the display axis. */
constexpr float kBlackPointRange = 0.220f, kBlackPointExpo = 1.6f;
constexpr float kWhitePointRange = 0.310f, kWhitePointExpo = 1.8f;

/* The endpoint move divides by (1 - point), so the point cannot reach 1. Only a
   hand-edited sidecar can get near it; full slider stops at 0.31. */
constexpr float kPointMax = 0.90f;

/* Smoothstep -- C1 at both ends, so a band's edge cannot show as a crease in a smooth
   ramp the way a clamped linear ramp does. */
inline float toneSmooth(float x)
{
    if (x <= 0.0f) return 0.0f;
    if (x >= 1.0f) return 1.0f;
    return x * x * (3.0f - 2.0f * x);
}

/* A band's weight at display position d: rises from 0 at lo to 1 at peak, back to 0 at hi,
   and is zero outside [lo, hi] entirely. shift slides the whole band along the axis, which
   is how the ToneRegionSlider handles move a region without changing its shape. */
inline float toneBand(float d, const ToneOp &op, float shift, float reach)
{
    const float lo = op.lo + shift, pk = op.peak + shift, hi = op.hi + shift + reach;
    if (d <= lo || d >= hi || pk <= lo || hi <= pk) return 0.0f;
    return (d <= pk) ? toneSmooth((d - lo) / (pk - lo))
                     : toneSmooth((hi - d) / (hi - pk));
}

/* The shadows gain's weight at display position d. The shadow handle slides the start of
   the decay (below it the weight is flat at 1) and the crossover handle stretches the
   tail, the same two things they do to a band; the defaults are a no-op. */
inline float shadowGainWeight(float d, float shift, float reach)
{
    const float x   = std::max(0.0f, d - shift);
    const float end = std::max(0.05f, kShadowGainEnd + reach);
    return ((1.0f - kShadowGainTail) * std::exp(-x / kShadowGainDecay) + kShadowGainTail)
           * (1.0f - toneSmooth(x / end));
}

/* sRGB's transfer, the one OutputTransform::DisplayPosition ends with: the display axis
   is sRGB-encoded, and the shadows gain is applied to the light it encodes. */
inline float displayToLinear(float d)
{
    return d <= 0.04045f ? d / 12.92f : std::pow((d + 0.055f) / 1.055f, 2.4f);
}
inline float linearToDisplay(float v)
{
    if (v <= 0.0f) return 0.0f;
    return v <= 0.0031308f ? v * 12.92f : 1.055f * std::pow(v, 1.0f / 2.4f) - 0.055f;
}

/* One signed control -> its display-space shift at d. downAmp replaces down.amp: the
   negative direction's strength is resolved per frame (Highlights -, see the budget). */
inline float toneOpShift(float amount, const ToneOp &up, const ToneOp &down,
                         float downAmp, float d, float shift, float reach)
{
    if (amount == 0.0f) return 0.0f;
    const float mag = std::fabs(amount);
    const ToneOp &op = (amount > 0.0f) ? up : down;
    const float w = toneBand(d, op, shift, reach);
    if (w == 0.0f) return 0.0f;
    const float amp = (amount > 0.0f) ? op.amp : downAmp;
    return (amount > 0.0f ? 1.0f : -1.0f) * amp * std::pow(mag, op.expo) * w;
}

/* The PARAMETRIC tone shape -- the contrast slope, the two endpoint moves and the two
   region lifts -- resolved from the params once. Shared by Develop::buildPointCoeffs (which
   bakes it into the tone LUT) and Develop::ParametricCurve (which the Curves panel's
   Parametric view draws), so the curve the user sees and the curve that actually renders
   cannot diverge. Everything here is in the PERCEPTUAL (gamma) domain. */
struct ToneShape {
    float pivot = 0.0f, slope = 1.0f;
    float bk = 0.0f, sh = 0.0f, hi = 0.0f, wh = 0.0f;   // slider amounts, -1..1
    float blackPoint = 0.0f, whitePoint = 1.0f;         // display-axis endpoint moves
    float shShift = 0.0f, hiShift = 0.0f, reach = 0.0f; // from the region handles
    float hiDownAmp = kHighsDown.amp;                   // Highlights -: frame-adaptive
    OutputTransform::ViewTransform view = OutputTransform::ViewTransform::None;
    bool  active = false;
    /* Highlight recovery (Highlights - with a positive Exposure); see the note by
       kRecoverDisplay. Resolved in buildToneShape from the params alone. */
    bool  recover = false;
    float recKnee = 0.0f, recL = 1.0f;                  // log2(n) knee, compression scale
    float lutMaxN = kToneLutMaxN;                       // linear n the LUT spans
};

/* Resolve the Highlights - recovery from the params: Highlights negative and Exposure
   positive, or nothing can have been pushed past white. Runs last in buildToneShape,
   because it needs the view and the contrast slope. See the note by kRecoverDisplay. */
inline void setHighlightRecovery(ToneShape &t, float exposure)
{
    t.recover = false;
    if (!(t.hi < 0.0f) || !(exposure > 0.0f)) return;

    /* Positions on the input axis (log2 n) of display levels AFTER the contrast slope --
       carried back through it, because recovery runs first and contrast would otherwise
       push the landed top straight back over. */
    auto inputX = [&t](float display) {
        const float s = OutputTransform::PerceptualFromDisplay(display, t.view,
                                                               kToneLutMaxN);
        const float sPre = t.pivot + (s - t.pivot) / t.slope;
        return sPre > 0.0f ? kGamma * std::log2(sPre) : -100.0f;
    };
    const float xClip = inputX(kRecoverClipDisplay);
    const float xTop  = xClip + exposure;            // the most the push can have made
    /* Barely pushed lands barely under the clip, so the landing point is continuous in
       Exposure rather than jumping to kRecoverDisplay the moment it leaves 0. */
    const float xTarget = qMax(inputX(kRecoverDisplay), xClip - exposure);
    const float over = xTop - xTarget;

    const float a = qMin(1.0f, -t.hi);
    const float knee = qMin(over, kRecoverKneeMax);  // stops below the landing point
    const float U  = over + knee;                    // top's distance above the knee
    const float Ut = knee + (1.0f - a) * over;       // ... and where it has to land
    if (!(U > 0.0f) || Ut >= U * 0.9999f) return;

    /* Solve L (1 - exp(-U/L)) = Ut. The left side rises from 0 to U as L does, so the
       root is bracketed by [Ut, the first doubling that overshoots]. */
    auto g = [U](float L) { return L * (1.0f - std::exp(-U / L)); };
    float lo = Ut, hi = Ut * 2.0f;
    while (g(hi) < Ut && hi < 1e6f) hi *= 2.0f;
    for (int i = 0; i < 50; ++i) {
        const float mid = 0.5f * (lo + hi);
        (g(mid) < Ut ? lo : hi) = mid;
    }
    t.recKnee = xTarget - knee;
    t.recL    = 0.5f * (lo + hi);
    t.recover = true;
    t.active  = true;
    /* The table has to reach the pushed data; past it, pixels take the last entry. */
    t.lutMaxN = kToneLutMaxN * std::exp2(exposure);
}

inline ToneShape buildToneShape(const EditParams &p)
{
    ToneShape t;
    t.pivot = std::pow(0.18f, kInvGamma);
    t.slope = (p.contrast != 0.0f)
                  ? 1.0f + kContrastSlopeRange * (p.contrast / kContrastFullScale)
                  : 1.0f;
    t.bk = p.blacks     / kToneFullScale;     // -1..1, + brightens
    t.sh = p.shadows    / kToneFullScale;
    t.hi = p.highlights / kToneFullScale;
    t.wh = p.whites     / kToneFullScale;

    /* The two endpoint moves, each in the ONE direction that clips (see the operator note
       above). The other direction of each is a band and is handled with the rest. */
    if (t.bk < 0.0f)
        t.blackPoint = qMin(kPointMax,
                            kBlackPointRange * std::pow(-t.bk, kBlackPointExpo));
    if (t.wh > 0.0f)
        t.whitePoint = qMax(1.0f - kPointMax,
                            1.0f - kWhitePointRange * std::pow(t.wh, kWhitePointExpo));

    /* THE REGION HANDLES SLIDE THE BANDS, they no longer set their peaks. The band shapes
       are fitted to Lightroom, so a handle that overwrote a peak would throw the fit away
       the moment it was touched; a shift moves where a region acts while keeping the shape
       that was measured. The DEFAULTS (0.25 / 0.50 / 0.75) are an exact no-op, which is
       the property the handles have always had and which the sidecar format depends on --
       an image saved before this change still renders from the same three numbers.
       Clamped so a hand-edited sidecar cannot invert a band. */
    const float shC = qBound(0.02f, p.toneShadowCenter,    0.94f);
    const float hiC = qBound(shC + 0.04f, p.toneHighlightCenter, 0.98f);
    const float crX = qBound(shC + 0.01f, p.toneCrossover,  hiC - 0.01f);
    t.shShift = shC - 0.25f;
    t.hiShift = hiC - 0.75f;
    t.reach   = crX - 0.50f;

    t.view = OutputTransform::ViewFromInt(p.viewTransform);
    t.active = (t.slope != 1.0f) || (t.bk != 0.0f) || (t.sh != 0.0f) ||
               (t.hi != 0.0f) || (t.wh != 0.0f);
    setHighlightRecovery(t, p.exposure);
    return t;
}

/*
    Contrast, then the tone controls.

    THE CONTROLS RUN ON THE DISPLAY AXIS and contrast does not, which is a real distinction
    rather than an inconsistency. Contrast is a slope about mid-grey in the working domain,
    where "about mid-grey" means something physical. The four tone controls are placed by
    handles drawn under the histogram of the FINISHED render and were fitted to Lightroom's
    operators measured in levels of that render, so both their placement and their strength
    are quantities on that axis. Applying them anywhere else was the bug: evaluated on the
    working value, the three handles at 0.25 / 0.50 / 0.75 acted at display 67 / 170 / 236,
    which put the whole Highlights region above level 170 and left it reaching a quarter of
    Lightroom's range.

    So the value goes out to the display axis, is moved there, and comes back. Converting
    the shift the other way instead -- into a working-value shift via the local slope of
    the view transform -- fails near the shoulder, where that slope is small enough that
    the conversion explodes.

    THE BANDS ARE EVALUATED ON THE PRE-ENDPOINT POSITION, for the same reason the weights
    are evaluated pre-contrast: a handle names a place on the histogram the user is looking
    at, and the endpoint moves would otherwise slide that place out from under it.
*/
/* applyToneShape in two parts, for the LOCAL Highlights - (see the note by
   kHighsLocalSigma): d is the finished display value WITHOUT the Highlights shift,
   d0 the position the bands read their weights at. Every move after the shadows gain is
   an additive shift weighted by d0 alone, so they commute, and adding the Highlights
   shift last is the same curve as adding it between them. */
struct ToneParts { float d, d0; };
inline ToneParts toneShapeParts(const ToneShape &t, float s)
{
    /* Highlight recovery first: it reshapes the INPUT, so the handles' d0 and everything
       below see the recovered value. Below the knee it is the identity. */
    if (t.recover && s > 0.0f) {
        const float x = kGamma * std::log2(s);
        if (x > t.recKnee) {
            const float u = x - t.recKnee;
            const float xr = t.recKnee + t.recL * (1.0f - std::exp(-u / t.recL));
            s = std::exp2(xr * kInvGamma);
        }
    }

    /* d0 -- where this tone sits on the histogram the handles are drawn under -- is taken
       BEFORE the contrast slope, so raising Contrast cannot slide the region a handle
       selects out from under it. d is the value actually being moved, so it is taken
       after. Two positions, deliberately, and collapsing them into one reintroduces the
       contrast bug. */
    const float d0 = OutputTransform::DisplayPosition(s, t.view);
    s = t.pivot + (s - t.pivot) * t.slope;
    float d = OutputTransform::DisplayPosition(s, t.view);

    if (t.blackPoint > 0.0f) d = qMax(0.0f, (d - t.blackPoint) / (1.0f - t.blackPoint));
    if (t.whitePoint < 1.0f) d = qMin(1.0f, d / t.whitePoint);

    /* Shadows scales the light, so it goes first: the additive moves below then shift
       the scaled value, and black is still black when they start. */
    if (t.sh != 0.0f)
        d = linearToDisplay(displayToLinear(d) *
                            std::exp2(t.sh * kShadowGainStops *
                                      shadowGainWeight(d0, t.shShift, t.reach)));
    /* Blacks + and Whites - are the non-clipping directions, so they are bands. Their
       opposite directions were consumed by the endpoint moves above. */
    if (t.bk > 0.0f) d += kBlacksUp.amp  * std::pow(t.bk,  kBlacksUp.expo)
                          * toneBand(d0, kBlacksUp,  t.shShift, t.reach);
    if (t.wh < 0.0f) d -= kWhitesDown.amp * std::pow(-t.wh, kWhitesDown.expo)
                          * toneBand(d0, kWhitesDown, t.hiShift, t.reach);
    return {d, d0};
}

/* The Highlights band's display shift for a tone whose (recovered) position is d0. */
inline float toneHighlightShift(const ToneShape &t, float d0)
{
    return toneOpShift(t.hi, kHighsUp, kHighsDown, t.hiDownAmp, d0, t.hiShift, t.reach);
}

inline float applyToneShape(const ToneShape &t, float s)
{
    const ToneParts tp = toneShapeParts(t, s);
    const float d = tp.d + toneHighlightShift(t, tp.d0);
    if (d <= 0.0f) return 0.0f;
    return OutputTransform::PerceptualFromDisplay(d, t.view, t.lutMaxN);
}

/* Texture (spatial op): mid-frequency luminance local contrast. The base-blur radius is a
   fraction of the image's longer edge so the proxy and full-res renders shape the same band;
   kTextureGain sets how hard a positive slider amplifies the detail (negative smooths it). */
constexpr float kTextureFullScale = 100.0f;
constexpr float kTextureSigmaFrac = 0.0015f;   // ~13px on a 8640px edge
constexpr float kTextureGain      = 1.5f;      // amount=+1 -> detail *2.5

/* Clarity (spatial op #6.5): mid-radius luminance local contrast, between Texture's fine
   detail band and Dehaze's atmospheric one. Two things keep it from being "Texture with a
   bigger number": the effect is weighted toward the MIDTONES (so it cannot crush blacks
   or flatten highlights the way a flat wide band does) and rolls off at strong edges,
   where a band this wide rings. Gentler gain than Texture because a wider band reads
   much stronger per unit. See Develop/localcontrast.h. */
constexpr float kClarityFullScale = 100.0f;
constexpr float kClaritySigmaFrac = 0.007f;    // ~60px on a 8640px edge
constexpr float kClarityGain      = 0.8f;      // vs kTextureGain 1.5, at 5x the radius
constexpr float kClarityHaloKnee  = 0.15f;     // excursion counting as a hard edge

/* Dehaze (spatial op, APPROXIMATION): a large-radius luminance local-contrast boost, a contrast
   pull about a low pivot (deepens shadows, the hallmark of clearing haze), and a saturation
   lift. Radius is a larger fraction of the image; the box blur (cv::blur) is used because its
   running-sum cost is ~independent of radius, keeping the big base blur cheap at full res. */
constexpr float kDehazeFullScale  = 100.0f;
constexpr float kDehazeSigmaFrac  = 0.02f;     // ~170px on a 8640px edge
constexpr float kDehazeLocalGain  = 1.0f;      // local-contrast strength
constexpr float kDehazeContrast   = 0.3f;      // contrast about the low pivot
constexpr float kDehazePivot      = 0.3f;      // perceptual pivot (below mid-grey)
constexpr float kDehazeSat        = 0.3f;      // saturation lift at full slider

/* Sharpen (#8.5) has NO tuning constants here. Its amount is stored pre-scaled (0..1.5,
   like grainAmount / localDenoise*, the panel applies the divisor) and, uniquely, its
   radius is ABSOLUTE pixels rather than a fraction of max(w,h) -- so the sigma and the
   detail/mask knees all come from Develop/sharpen.h, scaled by WorkingImage::render-
   Scale instead of by image size. See the Sharpen() comment. */

/* Vignette (spatial op #8): a radial exposure falloff about the image centre. The mask is
   pow(rn, k) where rn is the elliptical radius normalised to 1 at the corners, so it is 0
   at the centre and 1 at the corners; feather maps to the exponent k (high feather -> low
   k -> gradual/reaches inward; low feather -> high k -> effect hugs the corners). The
   corner gain is 2^vignetteExposure, scaled by the mask so the centre is untouched. */
constexpr float kVignetteFeatherKMin = 0.5f;   // exponent at feather = 1 (gradual)
constexpr float kVignetteFeatherKMax = 5.0f;   // exponent at feather = 0 (corner-hugging)

/* Grain (spatial op #9): monochromatic film grain added to perceptual luminance, most
   visible in the midtones. The noise is generated at a REDUCED resolution (a grain-cell
   grid) and bilinearly upsampled, so the particle size is set by the cell size and the
   cost stays low; the cell scales with the long edge (kGrainCellUnit) so the proxy and
   full-res render show proportionally the same grain. grainSize maps the cell from fine
   to coarse (kGrainCellMin..Max units); grainRoughness modulates the grain amplitude with
   a second, lower-frequency field (kGrainRoughFreqDiv) so it is patchy, not uniform.
   kGrainStrength is the max perceptual std at full amount. A fixed RNG seed keeps the
   pattern stable across re-renders (no shimmer). */
constexpr float  kGrainStrength     = 0.12f;   // perceptual std of grain at amount = 1
constexpr double kGrainCellUnit     = 1500.0;  // long-edge px per grain "unit"
constexpr double kGrainCellMin      = 0.75;    // grain cell (units) at size = 0 (fine)
constexpr double kGrainCellMax      = 3.75;    // grain cell (units) at size = 1 (coarse)
constexpr int    kGrainRoughFreqDiv = 4;       // roughness field: lower frequency factor
constexpr uint64_t kGrainSeed = 0x9E3779B97F4A7C15ULL;   // fixed: grain must not shimmer

/* Run fn over the pixel-index range [0, n) split into chunks across the global pool (the same
   data-parallel idiom as Develop::applyPointOps / OutputTransform::ToImage). fn(i0, i1) must
   process a disjoint half-open slice; chunks are pixel-aligned so callers that index rgb[i*3+c],
   yp[i], etc. stay race-free. Falls back to a single in-line call when the pool is one thread or
   the work is small. */
template <class F>
inline void parallelFor(size_t n, F fn)
{
    const int maxThreads = qMax(1, QThreadPool::globalInstance()->maxThreadCount());
    const size_t kMinPerChunk = 1u << 16;   // ~65k pixels: keep chunks coarse vs. dispatch cost
    if (maxThreads == 1 || n < kMinPerChunk * 2) { fn(static_cast<size_t>(0), n); return; }

    const int chunks = static_cast<int>(qMin<size_t>(maxThreads, (n + kMinPerChunk - 1) / kMinPerChunk));
    const size_t per = (n + chunks - 1) / chunks;
    QVector<QFuture<void>> futures;
    futures.reserve(chunks);
    for (int k = 0; k < chunks; ++k) {
        const size_t i0 = static_cast<size_t>(k) * per;
        const size_t i1 = qMin(n, i0 + per);
        if (i0 >= i1) break;
        futures.append(QtConcurrent::run(QThreadPool::globalInstance(),
                                         [fn, i0, i1]() { fn(i0, i1); }));
    }
    for (QFuture<void> &f : futures) f.waitForFinished();
}

/* ------------------------------------------------------------------------------------
   Shared perceptual-luminance band pass

   Texture (#6), Clarity (#6.5), Dehaze (#7) and Sharpen (#8.5) all do the same three
   things: lift a perceptual luminance off the RGB, reshape it against a blurred base,
   and fold the result back as a RATIO on RGB so hue and chroma are untouched. Only the
   middle step differs. These two helpers own the first and third, so each op is just its
   own blur plus its own shape function -- see Develop/localcontrast.h for the shared
   scalar math and tests/unit/tst_localcontrast.cpp for the characterization gate that
   pins what Texture and Dehaze rendered before this was extracted.
   ------------------------------------------------------------------------------------ */

/* Perceptual, white-normalised luminance into a blurrable cv::Mat, plus the scene-linear
   luma kept alongside for the ratio fold-back. */
inline void buildPerceptualLuma(const WorkingImage &img, cv::Mat &Yp,
                                std::vector<float> &Ylin)
{
    const int w = img.width, h = img.height;
    const size_t n = static_cast<size_t>(w) * static_cast<size_t>(h);
    const float white = (img.white > 0.0f) ? img.white : 1.0f;
    const float invWhite = 1.0f / white;
    const float *rgb = img.rgb.data();

    Yp.create(h, w, CV_32FC1);
    Ylin.resize(n);
    float *yp = Yp.ptr<float>();
    float *ylin = Ylin.data();
    parallelFor(n, [=](size_t i0, size_t i1) {
        for (size_t i = i0; i < i1; ++i) {
            const float r = rgb[i * 3 + 0], g = rgb[i * 3 + 1], b = rgb[i * 3 + 2];
            const float Y = kLumR * r + kLumG * g + kLumB * b;
            ylin[i] = Y;
            float nrm = Y * invWhite;
            if (nrm < 0.0f) nrm = 0.0f;
            yp[i] = std::pow(nrm, kInvGamma);
        }
    });
}

/* Gaussian base for a band op, with the large-sigma downscale trick.

   sigmaFrac is a fraction of the image's LONGER EDGE, so the proxy and the full-res
   settle render shape the same relative band (the invariant every spatial op but Sharpen
   holds). The base is a smooth low-frequency signal, so for a large sigma the costly
   Gaussian runs on a DOWNSCALED luminance image and is upsampled -- visually identical to
   a full-res blur, but the cost drops ~kDown^2, and GaussianBlur is the dominant cost of
   these ops at full resolution. The downscale targets a well-sampled small-image working
   sigma (~3px) and is capped so the base stays smooth; a sigma <= ~3 (e.g. on the proxy)
   blurs at full size. Shared by Texture (#6) and Clarity (#6.5); Dehaze uses a box blur
   instead because at its much larger radius a running-sum blur is cheaper still. */
inline cv::Mat gaussianBase(const cv::Mat &Yp, float sigmaFrac, int w, int h)
{
    const double sigma = qMax(1.0, static_cast<double>(sigmaFrac) * qMax(w, h));
    const int kDown = qBound(1, static_cast<int>(sigma / 3.0), 4);
    cv::Mat base;
    if (kDown > 1 && w >= kDown * 2 && h >= kDown * 2) {
        cv::Mat small;
        cv::resize(Yp, small, cv::Size(w / kDown, h / kDown), 0, 0, cv::INTER_AREA);
        cv::GaussianBlur(small, small, cv::Size(0, 0), sigma / kDown);
        cv::resize(small, base, cv::Size(w, h), 0, 0, cv::INTER_LINEAR);
    } else {
        cv::GaussianBlur(Yp, base, cv::Size(0, 0), sigma);
    }
    return base;
}

/* Reshape the perceptual luma and fold it back onto RGB as a ratio.

   shape(i, yp[i]) returns the new perceptual luma for pixel i; the op's own lambda closes
   over its blurred base. TEMPLATED ON PURPOSE: the shape inlines into this loop, so the
   op still costs ONE parallel pass and no extra buffer, exactly as the hand-written
   versions did. Pixels at or below black are skipped so true black stays black (a ratio
   is meaningless there). */
template <class Shape>
inline void shapeAndFoldLuma(WorkingImage &img, const float *yp, const float *ylin,
                             Shape shape)
{
    const size_t n = static_cast<size_t>(img.width) * static_cast<size_t>(img.height);
    const float white = (img.white > 0.0f) ? img.white : 1.0f;
    float *rgb = img.rgb.data();
    constexpr float kEps = 1e-6f;
    parallelFor(n, [=](size_t i0, size_t i1) {
        for (size_t i = i0; i < i1; ++i) {
            const float Y = ylin[i];
            if (Y <= kEps) continue;
            float s = shape(i, yp[i]);
            if (s < 0.0f) s = 0.0f;
            const float Yd = std::pow(s, kGamma) * white;
            const float r = Yd / Y;
            rgb[i * 3 + 0] *= r;
            rgb[i * 3 + 1] *= r;
            rgb[i * 3 + 2] *= r;
        }
    });
}
}

bool Develop::InputMatrix(const WorkingImage &img, const EditParams *p,
                         float m[3][3], bool &wbIncluded)
{
    wbIncluded = false;
    if (!img.cam.valid) return false;
    const bool ok = InputMatrixUnexposed(img, p, m, wbIncluded);
    /* THE CAMERA'S BASELINE EXPOSURE, folded in here because this matrix is applied
       exactly once per image (ToWorkingSpace or the preMat fold) -- a mask layer works on
       a copy of the converted base and never sees it again. A uniform scale, so it
       commutes with the white balance in the matrix. See Develop/baselineexposure.h. */
    const float k = std::exp2(BaselineExposure::ForModel(img.cam.cameraModel));
    if (ok && k != 1.0f)
        for (int i = 0; i < 3; ++i)
            for (int j = 0; j < 3; ++j) m[i][j] *= k;
    return ok;
}

bool Develop::InputMatrixUnexposed(const WorkingImage &img, const EditParams *p,
                                   float m[3][3], bool &wbIncluded)
{

    /*
        A CAMERA PROFILE replaces this whole stage. Under DNG the chosen white picks the
        interpolation weight between the profile's two calibrations AND sets the white
        balance inside the matrix, so the matrix is re-derived per temperature -- which is
        why the caller is told the white balance is already in it. See
        Develop/cameraprofile.h.

        A profile that fails to build (a corrupt file, a singular matrix) falls through to
        the built-in path rather than failing the render: the picture is then rendered the
        way it was before a profile was chosen, which is wrong-but-recognisable rather
        than absent. The panel is what tells the user, from the same resolve.
    */
    if (img.cam.profile) {
        float kelvin = 0.0f, tint = 0.0f;
        WhiteBalance::resolve(img.cam, p ? p->temp : 0.0f, p ? p->tint : 0.0f, kelvin, tint);
        if (CameraProfile::camToWorking(*img.cam.profile, kelvin, tint, m)) {
            wbIncluded = true;
            return true;
        }
    }

    /* The built-in path: camToWorking . diag(asShotMul). Scaling a matrix's COLUMN j by
       asShotMul[j] is what right-multiplying by a diagonal does. */
    for (int i = 0; i < 3; ++i)
        for (int j = 0; j < 3; ++j)
            m[i][j] = img.cam.camToWorking[i][j] * img.cam.asShotMul[j];
    return true;
}

bool Develop::ProfileTablesActive(const WorkingImage &img, const EditParams &p)
{
    if (!img.cam.valid || !img.cam.profile) return false;
    const Dcp::Profile &d = *img.cam.profile;
    /* A cheap STRUCTURAL test -- does the profile carry a HueSatMap -- rather than
       building the tables, because this is asked on every render to choose the route.
       Only the HueSatMap is per-pixel work HERE; the look half (LookTable, offset, tone
       curve) is carried to the output stage and costs stage 0 nothing. */
    Q_UNUSED(p)
    return !d.cal[0].hueSatMap.isEmpty() || !d.cal[1].hueSatMap.isEmpty();
}

void Develop::ApplyProfileTables(WorkingImage &img, const EditParams &p)
{
    if (!img.isValid() || !img.cam.valid || !img.cam.profile) return;
    /* A fresh base pass: whatever look a previous render left on this buffer belongs to
       that render's profile, not necessarily this one. */
    img.look.reset();

    float kelvin = 0.0f, tint = 0.0f;
    WhiteBalance::resolve(img.cam, p.temp, p.tint, kelvin, tint);

    CameraProfile::Tables t;
    if (!CameraProfile::tables(*img.cam.profile, kelvin, t) || !t.active) return;

    /*
        THE LOOK GOES TO THE OUTPUT STAGE. The DNG SDK applies it after exposure --
        HueSatMap, exposure (with BaselineExposureOffset), LookTable, ToneCurve -- so it
        must follow every edit, not precede them. It used to run here, before exposure and
        with the curve ahead of the LookTable: a +3 EV push then scaled pixels the curve
        had already carried to display white. See Develop/outputlook.h.
    */
    if (!t.lookTable.isEmpty() || t.exposureScale != 1.0f || !t.toneCurve.isEmpty()) {
        auto look = std::make_shared<OutputLook>();
        look->lookTable = t.lookTable;
        look->exposureScale = t.exposureScale;
        look->profileCurve = t.toneCurve;
        for (int i = 0; i < 3; ++i)
            for (int j = 0; j < 3; ++j) {
                look->toTable[i][j]   = t.toTable[i][j];
                look->fromTable[i][j] = t.fromTable[i][j];
            }
        img.look = look;
    }

    /* THE CHARACTERISATION STAYS HERE: the HueSatMap is a correction to the camera's
       colour, so it runs before anything is adjusted on top of it. */
    if (t.hueSatMap.isEmpty()) return;
    /* The table is defined in a space reached FROM the working space; camera-native
       pixels have not got there yet. Apply() guarantees this by forcing the early
       conversion when a HueSatMap is present, so reaching here untagged is a caller
       error. */
    if (img.space != ColorSpaceMath::kWorking) return;

    const HueSatMap::Table &hsm = t.hueSatMap;
    float to[3][3], fr[3][3];
    for (int i = 0; i < 3; ++i)
        for (int j = 0; j < 3; ++j) { to[i][j] = t.toTable[i][j]; fr[i][j] = t.fromTable[i][j]; }

    float *rgb = img.rgb.data();
    const size_t n = static_cast<size_t>(img.width) * static_cast<size_t>(img.height);
    parallelFor(n, [=, &hsm](size_t i0, size_t i1) {
        for (size_t i = i0; i < i1; ++i) {
            float *px = rgb + i * 3;
            const float r = px[0], g = px[1], b = px[2];
            float pr = to[0][0] * r + to[0][1] * g + to[0][2] * b;
            float pg = to[1][0] * r + to[1][1] * g + to[1][2] * b;
            float pb = to[2][0] * r + to[2][1] * g + to[2][2] * b;
            HueSatMap::Apply(hsm, pr, pg, pb);
            px[0] = fr[0][0] * pr + fr[0][1] * pg + fr[0][2] * pb;
            px[1] = fr[1][0] * pr + fr[1][1] * pg + fr[1][2] * pb;
            px[2] = fr[2][0] * pr + fr[2][1] * pg + fr[2][2] * pb;
        }
    });
}

void Develop::ToWorkingSpace(WorkingImage &img, const EditParams *p)
{
    if (img.space != ColorSpaceMath::ColorSpace::CameraNative) return;
    if (!img.isValid()) return;

    /* An invalid characterisation leaves the pixels alone and just re-tags: an unknown
       camera renders approximately rather than not at all. */
    float m[3][3] = {{1,0,0}, {0,1,0}, {0,0,1}};
    bool wbIncluded = false;
    if (InputMatrix(img, p, m, wbIncluded)) {

        float *rgb = img.rgb.data();
        const size_t n = static_cast<size_t>(img.width) * static_cast<size_t>(img.height);
        parallelFor(n, [=](size_t i0, size_t i1) {
            for (size_t i = i0; i < i1; ++i) {
                float *px = rgb + i * 3;
                const float r = px[0], g = px[1], b = px[2];
                px[0] = m[0][0] * r + m[0][1] * g + m[0][2] * b;
                px[1] = m[1][0] * r + m[1][1] * g + m[1][2] * b;
                px[2] = m[2][0] * r + m[2][1] * g + m[2][2] * b;
            }
        });
    }
    img.space = ColorSpaceMath::kWorking;
}

bool Develop::Apply(WorkingImage &img, const EditParams &p, StageTimings *t)
{
    if (!img.isValid()) return false;
    lastHiWeight = -1.0f;
    lastCoeffs.reset();

    /*
        THE FRAME'S STATISTICS SAMPLE GOES FIRST, through this same Apply with the same
        params, so it is in exactly the state img will be in at every stage. Its
        measurement of the Highlights band's mean weight sets img's band strength (see
        kHighsDownBudget), so a proxy, the settle render and a Detail patch -- a crop,
        whose own pixels are not the frame -- all draw one curve; its point coefficients
        are reused for img, so the tone table is built once. And the DEVELOPED sample is
        handed on with img below, so a mask scope that develops from this result
        measures what the Global scope produced, not the raw frame. A ~128 px pass; the
        sample's own Apply does not recurse (statSample).
    */
    /* The LOCAL sample likewise, and for the same reason taken before img's pixels
       change: Highlights -'s local Laplacian runs on it (see buildHiField). */
    const std::shared_ptr<const WorkingImage> local0 =
        (p.highlights < 0.0f && !img.statSample) ? LocalSample(img) : nullptr;

    float hiWeight = -1.0f;
    std::shared_ptr<const PointCoeffs> sampleCoeffs;
    std::shared_ptr<const WorkingImage> developedSample;
    if (!img.statSample) {
        if (const std::shared_ptr<const WorkingImage> s0 = StatSample(img)) {
            WorkingImage s;
            s.width = s0->width;
            s.height = s0->height;
            s.rgb = s0->rgb;
            copyMetadata(s, img);                   // img's metadata NOW (profile, space)
            s.renderScale = img.renderScale * float(std::max(s.width, s.height)) /
                            float(std::max(img.width, img.height));
            s.statSlot.reset();
            s.statSample = true;
            s.frameX = s.frameY = s.frameW = s.frameH = 0;   // the sample IS the frame
            Develop sd;
            sd.Apply(s, p);
            hiWeight = sd.lastHiWeight;
            sampleCoeffs = sd.lastCoeffs;
            developedSample = std::make_shared<const WorkingImage>(std::move(s));
        }
    }
    /* img's pixels are about to change, so it gets a NEW slot holding the developed
       sample -- never a write into the slot it shares with the cached original. */
    auto handOnSample = [&img, &developedSample]() {
        if (!developedSample) return;
        img.statSlot = std::make_shared<StatSampleSlot>();
        img.statSlot->sample = developedSample;
    };

    /* The input profile is NOT an edit -- a raw arrives in camera-native primaries and
       has to be converted whatever the params say, so this precedes the identity
       early-out. Denoise (#1) runs before the fused point pass, so when it is active the
       conversion has to happen here as its own pass; otherwise it is left to
       buildPointCoeffs, which folds it into preMat for free. */
    const bool denoiseActive = (p.localDenoiseLuma > 0.0f || p.localDenoiseChroma > 0.0f);
    /* A HueSatMap is a TABLE, so it cannot be folded into preMat the way the matrix half
       of a profile is: the conversion has to happen as its own pass first, exactly as an
       active Denoise forces it to. */
    const bool tablesActive = ProfileTablesActive(img, p);
    if (p.isIdentity() || denoiseActive || tablesActive) ToWorkingSpace(img, &p);

    if (p.isIdentity()) {                                // see the note below
        img.cam.profile.reset();
        img.look.reset();
        handOnSample();
        return true;
    }

    QElapsedTimer probe;
    if (t) probe.start();

    /* Stage 0.5: finish the input profile before anything is adjusted on top of it. */
    ApplyProfileTables(img, p);
    if (t) t->profileMs = probe.restart();

    /* Fixed pipeline order: spatial ops own a pass, point ops share the fused pass.
       See notes/Documentation.txt "DEVELOP / IMAGE EDIT". Denoise (#1) -> fused point
       pass (#2-5: WB, exposure, contrast, tone regions) -> Texture (#6) ->
       Clarity (#6.5, mid-radius local contrast) -> Dehaze (#7) ->
       Vignette (#8, a radial exposure falloff) -> Sharpen (#8.5, capture USM) ->
       Grain (#9, film grain, last). */
    Denoise(img, p);
    if (t) t->denoiseMs = probe.restart();

    /* Held by pointer, never by value: PointCoeffs carries its tone tables inline (~150
       KB), and Apply nests -- the frame's sample is developed through this same function
       first -- so two copies on a 512 KB pool-thread stack overflowed it. */
    const std::shared_ptr<const PointCoeffs> coeffs =
        sampleCoeffs ? sampleCoeffs : buildPointCoeffs(p, img, &hiWeight);
    const PointCoeffs &c = *coeffs;
    lastHiWeight = hiWeight;
    if (img.statSample) lastCoeffs = coeffs;
    const std::shared_ptr<const HiField> hiField =
        (c.active && c.hiLocal && local0) ? buildHiField(*local0, p, c) : nullptr;
    if (c.active) applyPointOps(img, c, hiField.get());
    /* preMat carried the camera-native -> working conversion, so the pixels are in the
       working space now and the tag has to say so: every op after this point, and
       OutputTransform, read img.space to pick their luma weights and matrices. */
    if (c.preMatActive) img.space = ColorSpaceMath::kWorking;
    if (t) t->pointMs = probe.restart();

    Texture(img, p);
    if (t) t->textureMs = probe.restart();

    Clarity(img, p);
    if (t) t->clarityMs = probe.restart();

    Dehaze(img, p);
    if (t) t->dehazeMs = probe.restart();

    Vignette(img, p);
    if (t) t->vignetteMs = probe.restart();

    Sharpen(img, p);
    if (t) t->sharpenMs = probe.restart();

    Grain(img, p);
    if (t) t->grainMs = probe.restart();

    /*
        STAGE 0 IS SPENT -- drop the profile.

        This image has now been through the profile's matrix AND its HueSatMap, and the
        scope compositor develops each MASK LAYER by copying this result and calling
        Apply again with that scope's params (WorkingImageCache::renderStack). A layer
        inherits `cam` wholesale, so a profile left here would be applied a second time
        per scope: the HueSatMap re-run on top of itself, and -- worse, because it is
        invisible rather than merely wrong -- the scope's OWN white balance dropped, since
        buildPointCoeffs reads cam.profile to mean "the white balance is already in the
        matrix". It is, for the base. It is not for an adjustment layered on top of it.

        Cleared HERE rather than by the caller because this is the layer that knows stage 0
        just ran; leaving it to every render site is how one of them forgets.
    */
    img.cam.profile.reset();
    handOnSample();
    return true;
}

std::shared_ptr<const WorkingImage> Develop::StatSample(const WorkingImage &img)
{
    if (img.statSample || !img.statSlot || !img.isValid()) return nullptr;
    constexpr int kLongEdge = 128;
    StatSampleSlot &slot = *img.statSlot;
    std::lock_guard<std::mutex> lock(slot.mutex);
    if (slot.sample) return slot.sample;

    auto s = std::make_shared<WorkingImage>();
    const int longEdge = std::max(img.width, img.height);
    const double f = std::min(1.0, double(kLongEdge) / longEdge);
    s->width  = std::max(1, int(img.width  * f + 0.5));
    s->height = std::max(1, int(img.height * f + 0.5));
    s->rgb.resize(size_t(s->width) * size_t(s->height) * 3);
    const cv::Mat src(img.height, img.width, CV_32FC3, const_cast<float *>(img.rgb.data()));
    cv::Mat dst(s->height, s->width, CV_32FC3, s->rgb.data());
    /* INTER_AREA: each sample is the MEAN of the pixels it covers, which is what keeps a
       statistic over it close to the same statistic over the full frame. */
    cv::resize(src, dst, dst.size(), 0, 0, cv::INTER_AREA);
    copyMetadata(*s, img);
    s->statSlot.reset();
    s->statSample = true;
    s->frameX = s->frameY = s->frameW = s->frameH = 0;  // the sample IS the frame
    slot.sample = s;
    return slot.sample;
}

std::shared_ptr<const WorkingImage> Develop::LocalSample(const WorkingImage &img)
{
    if (img.statSample || !img.statSlot || !img.isValid()) return nullptr;
    StatSampleSlot &slot = *img.statSlot;
    std::lock_guard<std::mutex> lock(slot.mutex);
    if (slot.local) return slot.local;
    if (img.frameW > 0) return nullptr;                 // a crop is not the frame

    auto s = std::make_shared<WorkingImage>();
    const int longEdge = std::max(img.width, img.height);
    const double f = std::min(1.0, double(kHighsLocalLong) / longEdge);
    s->width  = std::max(1, int(img.width  * f + 0.5));
    s->height = std::max(1, int(img.height * f + 0.5));
    s->rgb.resize(size_t(s->width) * size_t(s->height) * 3);
    const cv::Mat src(img.height, img.width, CV_32FC3, const_cast<float *>(img.rgb.data()));
    cv::Mat dst(s->height, s->width, CV_32FC3, s->rgb.data());
    cv::resize(src, dst, dst.size(), 0, 0, cv::INTER_AREA);
    copyMetadata(*s, img);
    s->statSlot.reset();
    s->statSample = true;                               // never developed into a sample
    s->frameX = s->frameY = s->frameW = s->frameH = 0;
    slot.local = s;
    return slot.local;
}

/*
    THE LOCAL HIGHLIGHTS - FIELD (see the note by kHighsLocalSigma).

    1. The frame's LocalSample through the point pass's front stages (input profile, white
       balance, exposure, calibrate) to luminance, and onto the band's own axis: I = d0.
    2. A fast local Laplacian on I (Aubry et al. 2014): for each of kHighsLocalLevels
       reference tones g, remap I by r_g(i) = i + T(g) - g where |i - g| is small and
       T(i) - i where it is large (a Gaussian split of width kHighsLocalSigma), take r_g's
       Laplacian pyramid, and give each output coefficient the two reference pyramids
       bracketing the input's own Gaussian-pyramid value there. T(d) = d + the band's
       shift at d (PointCoeffs::hiShiftByPos). The coarsest level takes T directly.
    3. correction = (result - I) - shift(I): what the region adds beyond the per-pixel band.
    4. A guided upsample (guide I, radius kHighsLocalUpRadius): correction ~ a * I + b,
       locally. The fused pass reads a, b bilinearly and evaluates them on each pixel's own
       d0, which puts the field's edges back on the full-resolution pixels.
*/
/*
    A fast local Laplacian filter (Aubry et al. 2014) of I over the value range [lo, hi]:
    each of kHighsLocalLevels reference values g remaps I by r_g(i) = i + T(g) - g for
    |i - g| well inside sigma and T(i) - i well outside it (a Gaussian split), and each
    output coefficient takes the two reference pyramids bracketing the input's own
    Gaussian-pyramid value there. shift(v) = T(v) - v. Returns the filtered image.
*/
template <class Shift>
static cv::Mat localLaplacian(const cv::Mat &I, float lo, float hi, float sigma, Shift shift)
{
    const int w = I.cols, h = I.rows;
    const int levels = qMax(1, int(std::floor(std::log2(double(qMin(w, h))))) - 3);
    std::vector<cv::Mat> G(levels + 1);
    G[0] = I;
    for (int l = 0; l < levels; ++l) cv::pyrDown(G[l], G[l + 1]);

    const int K = kHighsLocalLevels;
    const float range = qMax(1e-6f, hi - lo);
    const float step = range / (K - 1);
    const float inv2s2 = 1.0f / (2.0f * sigma * sigma);
    std::vector<std::vector<cv::Mat>> lap(static_cast<size_t>(K));
    QVector<int> ks(K);
    for (int k = 0; k < K; ++k) ks[k] = k;
    constexpr int kRemapN = 2048;                       // remap tabulated over [lo, hi]
    QtConcurrent::blockingMap(ks, [&](int k) {
        const float gref = lo + k * step;
        const float tg = shift(gref);
        /* r_g depends only on the value and the reference: tabulate it, so the per-pixel
           cost is one interpolated read. */
        std::vector<float> table(kRemapN + 1);
        for (int j = 0; j <= kRemapN; ++j) {
            const float iv = lo + range * float(j) / kRemapN;
            const float d = iv - gref;
            const float f = std::exp(-d * d * inv2s2);
            table[size_t(j)] = iv + tg * f + shift(iv) * (1.0f - f);
        }
        cv::Mat R(h, w, CV_32F);
        const float *in = I.ptr<float>();
        float *r = R.ptr<float>();
        for (int i = 0; i < w * h; ++i) {
            const float fj = qBound(0.0f, (in[i] - lo) / range, 1.0f) * kRemapN;
            const int j = qMin(int(fj), kRemapN - 1);
            r[i] = table[size_t(j)] + (table[size_t(j) + 1] - table[size_t(j)]) * (fj - j);
        }
        std::vector<cv::Mat> &L = lap[size_t(k)];
        L.resize(size_t(levels));
        cv::Mat cur = R;
        for (int l = 0; l < levels; ++l) {
            cv::Mat down, up;
            cv::pyrDown(cur, down);
            cv::pyrUp(down, up, cur.size());
            L[size_t(l)] = cur - up;
            cur = down;
        }
    });

    std::vector<cv::Mat> out(size_t(levels) + 1);
    for (int l = 0; l < levels; ++l) {
        out[size_t(l)] = cv::Mat::zeros(G[l].size(), CV_32F);
        const int n = G[l].rows * G[l].cols;
        const float *gp = G[l].ptr<float>();
        float *op = out[size_t(l)].ptr<float>();
        std::vector<const float *> lp(static_cast<size_t>(K));
        for (int k = 0; k < K; ++k) lp[size_t(k)] = lap[size_t(k)][size_t(l)].ptr<float>();
        const float *const *lk = lp.data();
        parallelFor(size_t(n), [=](size_t i0, size_t i1) {
            for (size_t i = i0; i < i1; ++i) {
                const float t = qBound(0.0f, (gp[i] - lo) / step, float(K - 1));
                const int k0 = qMin(int(t), K - 2);
                const float fr = t - k0;
                op[i] = lk[k0][i] * (1.0f - fr) + lk[k0 + 1][i] * fr;
            }
        });
    }
    {
        cv::Mat top = G[levels].clone();
        float *tp = top.ptr<float>();
        for (int i = 0; i < top.rows * top.cols; ++i) tp[i] += shift(tp[i]);
        out[size_t(levels)] = top;
    }
    cv::Mat x = out[size_t(levels)];
    for (int l = levels - 1; l >= 0; --l) {
        cv::Mat up;
        cv::pyrUp(x, up, out[size_t(l)].size());
        x = up + out[size_t(l)];
    }
    return x;
}

/* The guided upsample of v with guide I: v ~ a * I + b locally, averaged once more. */
static void guidedCoefficients(const cv::Mat &I, const cv::Mat &v, std::vector<float> &aOut,
                               std::vector<float> &bOut)
{
    const cv::Size ks(2 * kHighsLocalUpRadius + 1, 2 * kHighsLocalUpRadius + 1);
    cv::Mat mI, mV, mIV, mII;
    cv::boxFilter(I, mI, CV_32F, ks, cv::Point(-1, -1), true, cv::BORDER_REFLECT);
    cv::boxFilter(v, mV, CV_32F, ks, cv::Point(-1, -1), true, cv::BORDER_REFLECT);
    cv::boxFilter(I.mul(v), mIV, CV_32F, ks, cv::Point(-1, -1), true, cv::BORDER_REFLECT);
    cv::boxFilter(I.mul(I), mII, CV_32F, ks, cv::Point(-1, -1), true, cv::BORDER_REFLECT);
    cv::Mat a = (mIV - mI.mul(mV)) / (mII - mI.mul(mI) + kHighsLocalUpEps);
    cv::Mat b = mV - a.mul(mI);
    cv::boxFilter(a, a, CV_32F, ks, cv::Point(-1, -1), true, cv::BORDER_REFLECT);
    cv::boxFilter(b, b, CV_32F, ks, cv::Point(-1, -1), true, cv::BORDER_REFLECT);
    aOut.assign(a.ptr<float>(), a.ptr<float>() + a.total());
    bOut.assign(b.ptr<float>(), b.ptr<float>() + b.total());
}

std::shared_ptr<const Develop::HiField> Develop::buildHiField(const WorkingImage &localIn,
                                                              const EditParams &p,
                                                              const PointCoeffs &c)
{
    if (!localIn.isValid() || !c.hiLocal) return nullptr;
    WorkingImage local = localIn;
    /* preMat carries camera-native -> working when it is active; when it is not (an
       active Denoise or profile table converted img up front), convert the copy here. */
    if (local.space == ColorSpaceMath::ColorSpace::CameraNative && !c.preMatActive)
        ToWorkingSpace(local, &p);

    const int w = local.width, h = local.height;
    const float invWhite = 1.0f / c.white;
    const float sMax = c.toneLutSMax;
    const int lutLast = PointCoeffs::kLutSize - 1;
    const float lutScale = lutLast / sMax;
    const int backLast = PointCoeffs::kBackSize - 1;
    auto atS = [&](const float *t, float Y) {
        const float fi = std::pow(qMax(0.0f, Y), kInvGamma) * lutScale;
        if (fi >= lutLast) return t[lutLast];
        const int i = int(fi);
        return t[i] + (t[i + 1] - t[i]) * (fi - i);
    };
    auto bandShiftAt = [&](float d) {
        const float fk = qBound(0.0f, d, 1.0f) * backLast;
        const int k = qMin(int(fk), backLast - 1);
        return c.hiShiftByPos[k] + (c.hiShiftByPos[k + 1] - c.hiShiftByPos[k]) * (fk - k);
    };

    /* Luminance after the front stages, onto the band's axis (I = d0) and as log2 of the
       band-free display value in display-linear light (J), the frame-relative term's. */
    cv::Mat I(h, w, CV_32F), J(h, w, CV_32F);
    const float *pm = c.preMat, *cm = c.calMat, *g = c.channelGain;
    parallelFor(size_t(w) * h, [&](size_t i0, size_t i1) {
        float *oi = I.ptr<float>(), *oj = J.ptr<float>();
        for (size_t i = i0; i < i1; ++i) {
            const float *px = local.rgb.data() + i * 3;
            float v[3] = {px[0], px[1], px[2]};
            if (c.preMatActive) {
                const float r = v[0], gg = v[1], b = v[2];
                v[0] = pm[0] * r + pm[1] * gg + pm[2] * b;
                v[1] = pm[3] * r + pm[4] * gg + pm[5] * b;
                v[2] = pm[6] * r + pm[7] * gg + pm[8] * b;
            }
            for (int k = 0; k < 3; ++k) v[k] *= g[k];
            if (c.calActive) {
                const float r = v[0], gg = v[1], b = v[2];
                v[0] = cm[0] * r + cm[1] * gg + cm[2] * b;
                v[1] = cm[3] * r + cm[4] * gg + cm[5] * b;
                v[2] = cm[6] * r + cm[7] * gg + cm[8] * b;
            }
            const float Y = (kLumR * v[0] + kLumG * v[1] + kLumB * v[2]) * invWhite;
            oi[i] = atS(c.hiPosLut, Y);
            oj[i] = atS(c.hiLogLumLut, Y);
        }
    });

    auto field = std::make_shared<HiField>();
    field->w = w;
    field->h = h;

    /* 1. The band, carried by region (see the note by kHighsLocalSigma): what the region
          adds beyond the per-pixel band, guided-upsampled on d0. */
    {
        const cv::Mat x = localLaplacian(I, 0.0f, 1.0f, kHighsLocalSigma, bandShiftAt);
        cv::Mat corr(h, w, CV_32F);
        const float *in = I.ptr<float>(), *xp = x.ptr<float>();
        float *cp = corr.ptr<float>();
        for (int i = 0; i < w * h; ++i) cp[i] = (xp[i] - in[i]) - bandShiftAt(in[i]);
        guidedCoefficients(I, corr, field->a, field->b);
    }

    /* 2. The frame-relative term, carried by region the same way (see the note by
          kHighsAnchorK): the whole log gain, guided-upsampled on J. */
    {
        const float *dl = c.hiDropByL;
        auto dropShift = [&](float L) {
            const float fl = qBound(0.0f, (L - PointCoeffs::kHiLMin) /
                                              -PointCoeffs::kHiLMin, 1.0f) * backLast;
            const int k = qMin(int(fl), backLast - 1);
            return -(dl[k] + (dl[k + 1] - dl[k]) * (fl - k));
        };
        const cv::Mat x = localLaplacian(J, PointCoeffs::kHiLMin, 0.0f,
                                         kHighsAnchorSigma, dropShift);
        cv::Mat lg = x - J;
        cv::min(lg, 0.0, lg);
        guidedCoefficients(J, lg, field->ga, field->gb);
    }
    return field;
}

/* Local luminance noise reduction (spatial op). Ratio-preserving: edge-preserving smoothing of
   luminance only, applied as a per-pixel scale of RGB so hue/chroma ratios are untouched. The
   smoothing runs in the perceptual (gamma) domain -- noise is more uniform there, and it matches
   the contrast op's domain -- while the final ratio is formed in scene-linear. Portable (OpenCV,
   mac + Windows), independent of which engine decoded the image.

   GLOBAL (no-mask) case: smooths the whole frame. When the scope compositor lands, it calls this
   per scope bounded to the mask bbox and blends the result by the scope's alpha (see
   notes/Documentation.txt "Scope & masking model"); the cached output then only re-blends, not
   re-computes, on a point-slider drag. */
void Develop::Denoise(WorkingImage &img, const EditParams &p)
{
    const float lumAmt = p.localDenoiseLuma;
    const float chrAmt = p.localDenoiseChroma;
    if (lumAmt <= 0.0f && chrAmt <= 0.0f) return;

    const int w = img.width;
    const int h = img.height;
    const size_t n = static_cast<size_t>(w) * static_cast<size_t>(h);
    float *rgb = img.rgb.data();
    const float white = (img.white > 0.0f) ? img.white : 1.0f;
    const float invWhite = 1.0f / white;

    /* LUMINANCE NR (localDenoiseLuma): edge-preserving smoothing on luminance only, ratio-
       preserving so chroma is untouched. */
    if (lumAmt > 0.0f) {
        /* Perceptual, white-normalised luminance (Rec.709 linear-luma weights). Ylin keeps the
           scene-linear luma so the post-smoothing ratio can be formed in linear. */
        cv::Mat Yp(h, w, CV_32FC1);
        std::vector<float> Ylin(n);
        float *yp = Yp.ptr<float>();
        float *ylin = Ylin.data();
        parallelFor(n, [=](size_t i0, size_t i1) {
            for (size_t i = i0; i < i1; ++i) {
                const float r = rgb[i * 3 + 0], g = rgb[i * 3 + 1], b = rgb[i * 3 + 2];
                const float Y = kLumR * r + kLumG * g + kLumB * b;
                ylin[i] = Y;
                float nrm = Y * invWhite;
                if (nrm < 0.0f) nrm = 0.0f;
                yp[i] = std::pow(nrm, kInvGamma);
            }
        });

        /* Edge-preserving smoothing on luminance only; strength scales the range/space sigmas.
           d = 0 derives the kernel diameter from sigmaSpace. The bilateral is the expensive part
           and (unlike the O(n) passes) grows with pixel count, so run it at a BOUNDED resolution:
           downscale to ~2 MP, filter with a proportionally smaller spatial sigma (same effective
           radius), then upscale. This keeps the interactive proxy render off the GUI thread's
           critical path (see the Develop perf note in Documentation.txt); small images (<=2 MP,
           e.g. the proxy on a modest display) skip the downscale and filter at full resolution. */
        const double sigmaColor = 0.03 + 0.09 * static_cast<double>(lumAmt);
        const double sigmaSpace = 2.0 + 4.0 * static_cast<double>(lumAmt);
        const double mp = static_cast<double>(n) / 1e6;
        const int scale = (mp > 2.0) ? std::max(1, int(std::lround(std::sqrt(mp / 2.0)))) : 1;
        cv::Mat Ypd;
        if (scale > 1) {
            const int sw = std::max(1, w / scale), sh = std::max(1, h / scale);
            cv::Mat lo, loD;
            cv::resize(Yp, lo, cv::Size(sw, sh), 0, 0, cv::INTER_AREA);
            cv::bilateralFilter(lo, loD, 0, sigmaColor, sigmaSpace / scale);
            cv::resize(loD, Ypd, cv::Size(w, h), 0, 0, cv::INTER_LINEAR);
        }
        else {
            cv::bilateralFilter(Yp, Ypd, 0, sigmaColor, sigmaSpace);
        }

        /* Scale RGB by the linear luminance ratio, preserving chroma. */
        const float *ypd = Ypd.ptr<float>();
        constexpr float kEps = 1e-6f;
        parallelFor(n, [=](size_t i0, size_t i1) {
            for (size_t i = i0; i < i1; ++i) {
                const float Y = ylin[i];
                if (Y <= kEps) continue;                          // black pixel: nothing to scale
                const float Yd = std::pow(ypd[i], kGamma) * white;   // perceptual -> linear
                const float factor = Yd / Y;
                rgb[i * 3 + 0] *= factor;
                rgb[i * 3 + 1] *= factor;
                rgb[i * 3 + 2] *= factor;
            }
        });
    }

    /* COLOUR/CHROMA NR (localDenoiseChroma): smooth the opponent chroma (R-Y, B-Y) while keeping
       luminance EXACT, so colour blotches wash out without touching luma detail. Chroma noise is
       low-frequency, so we blur a downscaled copy (fast, O(1)-ish -- see the Develop perf note in
       Documentation.txt) and upsample; mild colour bleed at edges is acceptable for chroma. Runs
       after the luma pass, on the luma-denoised RGB. */
    if (chrAmt > 0.0f) {
        cv::Mat chroma(h, w, CV_32FC3);          // (Cr, Cb, 0), white-normalised linear
        std::vector<float> Yn(n);
        float *cp = chroma.ptr<float>();
        float *yn = Yn.data();
        parallelFor(n, [=](size_t i0, size_t i1) {
            for (size_t i = i0; i < i1; ++i) {
                const float r = rgb[i * 3 + 0] * invWhite;
                const float g = rgb[i * 3 + 1] * invWhite;
                const float b = rgb[i * 3 + 2] * invWhite;
                const float Y = kLumR * r + kLumG * g + kLumB * b;
                yn[i] = Y;
                cp[i * 3 + 0] = r - Y;            // Cr
                cp[i * 3 + 1] = b - Y;            // Cb
                cp[i * 3 + 2] = 0.0f;
            }
        });

        /* Downscaled edge-preserving blur of the chroma; strength scales the sigmas. */
        const int scale = 4;
        const int sw = std::max(1, w / scale), sh = std::max(1, h / scale);
        cv::Mat lo, loD;
        cv::resize(chroma, lo, cv::Size(sw, sh), 0, 0, cv::INTER_AREA);
        const double sigmaColorC = 0.02 + 0.10 * static_cast<double>(chrAmt);
        const double sigmaSpaceC = 2.0 + 6.0 * static_cast<double>(chrAmt);
        cv::bilateralFilter(lo, loD, 0, sigmaColorC, sigmaSpaceC);
        cv::Mat chromaD;
        cv::resize(loD, chromaD, cv::Size(w, h), 0, 0, cv::INTER_LINEAR);

        /* Recombine: keep Y exact, take the smoothed chroma; derive G so Y is preserved. */
        const float *cpd = chromaD.ptr<float>();
        parallelFor(n, [=](size_t i0, size_t i1) {
            for (size_t i = i0; i < i1; ++i) {
                const float Y = yn[i];
                const float R = Y + cpd[i * 3 + 0];
                const float B = Y + cpd[i * 3 + 1];
                const float G = (Y - kLumR * R - kLumB * B) / kLumG;
                rgb[i * 3 + 0] = std::max(0.0f, R * white);
                rgb[i * 3 + 1] = std::max(0.0f, G * white);
                rgb[i * 3 + 2] = std::max(0.0f, B * white);
            }
        });
    }
}

void Develop::BlendRawDenoise(const WorkingImage &clean, const WorkingImage &den,
                              float lum, float chr, WorkingImage &out)
{
/*
    Interactive "Denoise raw" blend: out = clean + amount-scaled (den - clean). PMRID runs at full
    strength once (den); the two Global amounts scale a luma/chroma split of the correction here, so
    dragging the sliders only re-blends (cheap) instead of re-running the model. Mirrors the
    highlight-preserving split used by the old post-demosaic path: highlights (correction ~0) pass
    through untouched.
*/
    out = clean;                            // dims/white/space/sceneReferred + fallback
    if (clean.width != den.width || clean.height != den.height) return;

    /* Both sides must be in the SAME colour state. den always comes from the raw decode
       (camera-native, scene-linear); clean can come from the WorkingImageCache, which --
       despite what its header promises -- also holds display-referred working-space
       entries built from the 8-bit decode (InputTransform::FromImage, what Preview mode
       leaves under a raw's path). Adding a camera-native difference to white-balanced
       pixels tints the whole image: the sensor's green sits far above its red/blue
       before the white balance, so the difference is dominated by green and the render
       goes magenta/green. A caller that hands over mismatched buffers gets the clean
       image back, not a cast. */
    if (clean.space != den.space || clean.sceneReferred != den.sceneReferred) {
        qWarning("Develop::BlendRawDenoise: colour-state mismatch (space %d vs %d, "
                 "sceneReferred %d vs %d) -- serving the clean base",
                 int(clean.space), int(den.space), int(clean.sceneReferred),
                 int(den.sceneReferred));
        return;
    }


    /* Luma weights for the space the buffers are ACTUALLY in. Since RawColor stops at
       camera-native this blend now runs on SENSOR primaries, where the working space's
       weights are the wrong numbers -- a camera's green channel does not carry 0.72 of
       its luminance. lumaWeightsFor (workingimage.h) returns the right ones. */
    const ColorSpaceMath::Luma LW = lumaWeightsFor(clean);
    const float wR = LW.r, wG = LW.g, wB = LW.b;
    lum = std::clamp(lum, 0.0f, 1.0f);
    chr = std::clamp(chr, 0.0f, 1.0f);
    const float cAmt = std::max(lum, chr);
    if (cAmt <= 0.0f) return;                        // nothing to add -> clean

    const size_t n = static_cast<size_t>(clean.width) * static_cast<size_t>(clean.height);
    const float *c = clean.rgb.data();
    const float *d = den.rgb.data();
    float *o = out.rgb.data();
    parallelFor(n, [=](size_t i0, size_t i1) {
        for (size_t i = i0; i < i1; ++i) {
            const float dr = d[i * 3 + 0] - c[i * 3 + 0];
            const float dg = d[i * 3 + 1] - c[i * 3 + 1];
            const float db = d[i * 3 + 2] - c[i * 3 + 2];
            const float dY = wR * dr + wG * dg + wB * db;   // luma of the correction
            o[i * 3 + 0] = c[i * 3 + 0] + lum * dY + cAmt * (dr - dY);
            o[i * 3 + 1] = c[i * 3 + 1] + lum * dY + cAmt * (dg - dY);
            o[i * 3 + 2] = c[i * 3 + 2] + lum * dY + cAmt * (db - dY);
        }
    });
}

/* Texture (spatial op #6). Ratio-preserving luminance local contrast in the perceptual domain:
   a Gaussian base isolates a mid-frequency detail band, which the slider amplifies (crisper) or
   attenuates (smoother). Runs after the point pass. See the class doc and the constants above. */
void Develop::Texture(WorkingImage &img, const EditParams &p)
{
    const float amt = p.texture / kTextureFullScale;     // -1..1
    if (amt == 0.0f) return;

    cv::Mat Yp;
    std::vector<float> Ylin;
    buildPerceptualLuma(img, Yp, Ylin);

    /* Mid-frequency base; sigma scales with the long edge so proxy and full-res match. */
    const cv::Mat base = gaussianBase(Yp, kTextureSigmaFrac, img.width, img.height);

    /* Positive uses the stronger kTextureGain; negative scales the detail down to zero
       at -1 (bandFactor clamps, so it lands on the base rather than inverting). */
    const float factor =
        LocalContrast::bandFactor(amt, (amt >= 0.0f) ? kTextureGain : 1.0f);
    const float *bp = base.ptr<float>();
    shapeAndFoldLuma(img, Yp.ptr<float>(), Ylin.data(), [=](size_t i, float yp) {
        return LocalContrast::applyBand(yp, bp[i], factor);
    });
}

/* Clarity (spatial op #6.5). Ratio-preserving luminance local contrast at a MID radius --
   between Texture (#6, ~13px) and Dehaze (#7, ~170px) on an 8640px edge -- so the three
   stack rather than compete. Positive adds midtone punch, negative gives a soft glow.

   Unlike Texture the band is WEIGHTED per pixel before it is scaled: midtoneWeight fades
   the effect toward black and white, and haloGuard rolls it off where the high-pass
   excursion is large, i.e. at the strong edges where a band this wide would otherwise
   ring. Weighting the AMOUNT (rather than the result) means the two guards simply pull
   the band factor back toward 1, so they can never invert or overshoot the band.

   Shares gaussianBase with Texture (the downscale trick keeps the wider blur cheap) and
   the perceptual-luma prep / ratio fold-back with every other band op. Math in
   Develop/localcontrast.h; no-op at 0. */
void Develop::Clarity(WorkingImage &img, const EditParams &p)
{
    const float amt = p.clarity / kClarityFullScale;     // -1..1
    if (amt == 0.0f) return;

    cv::Mat Yp;
    std::vector<float> Ylin;
    buildPerceptualLuma(img, Yp, Ylin);

    const cv::Mat base = gaussianBase(Yp, kClaritySigmaFrac, img.width, img.height);
    const float *bp = base.ptr<float>();

    shapeAndFoldLuma(img, Yp.ptr<float>(), Ylin.data(), [=](size_t i, float yp) {
        const float b = bp[i];
        const float w = LocalContrast::midtoneWeight(yp) *
                        LocalContrast::haloGuard(yp - b, kClarityHaloKnee);
        const float factor = LocalContrast::bandFactor(amt * w, kClarityGain);
        return LocalContrast::applyBand(yp, b, factor);
    });
}

/* Dehaze (spatial op #7), APPROXIMATION. Stage 1: ratio-preserving luminance large-radius local
   contrast plus a contrast pull about a low pivot (deepens shadows / extends range, the look of
   clearing haze). Stage 2: a saturation lift (haze desaturates). Positive removes haze, negative
   adds it. Not a physical dark-channel-prior dehaze -- see notes/Documentation.txt. */
void Develop::Dehaze(WorkingImage &img, const EditParams &p)
{
    const float amt = p.dehaze / kDehazeFullScale;       // -1..1
    if (amt == 0.0f) return;

    const int w = img.width;
    const int h = img.height;
    const size_t n = static_cast<size_t>(w) * static_cast<size_t>(h);
    float *rgb = img.rgb.data();

    cv::Mat Yp;
    std::vector<float> Ylin;
    buildPerceptualLuma(img, Yp, Ylin);

    /* Large-radius base via a box blur: its running-sum cost is ~independent of kernel
       size, so the wide blur stays cheap even at full resolution -- which is why this op
       does NOT use gaussianBase like Texture and Clarity. */
    const int rad = qMax(1, static_cast<int>(std::lround(static_cast<double>(kDehazeSigmaFrac) * qMax(w, h))));
    const int ksz = rad * 2 + 1;
    cv::Mat base;
    cv::blur(Yp, base, cv::Size(ksz, ksz));

    /* Stage 1: luminance local contrast + low-pivot contrast (ratio-preserving). The
       local
       contrast is the shared band expression: this op historically wrote it as
       yp + k*(yp-base), which is the same thing as base + (1+k)*(yp-base) in exact
       arithmetic and differs only in float rounding (~1e-7, far below one 8-bit code
       value). tst_localcontrast pins the pre-extraction render to prove it stayed put. */
    const float factor = LocalContrast::bandFactor(amt, kDehazeLocalGain);
    const float cont  = amt * kDehazeContrast;
    const float *bp = base.ptr<float>();
    shapeAndFoldLuma(img, Yp.ptr<float>(), Ylin.data(), [=](size_t i, float yp) {
        const float s = LocalContrast::applyBand(yp, bp[i], factor);
        return kDehazePivot + (s - kDehazePivot) * (1.0f + cont);   // deepen shadows
    });

    /* Stage 2: saturation about per-pixel luminance (haze desaturates; dehaze restores). */
    const float sat = 1.0f + amt * kDehazeSat;
    parallelFor(n, [=](size_t i0, size_t i1) {
        for (size_t i = i0; i < i1; ++i) {
            const float r = rgb[i * 3 + 0], g = rgb[i * 3 + 1], b = rgb[i * 3 + 2];
            const float Y2 = kLumR * r + kLumG * g + kLumB * b;
            float nr = Y2 + sat * (r - Y2);
            float ng = Y2 + sat * (g - Y2);
            float nb = Y2 + sat * (b - Y2);
            if (nr < 0.0f) nr = 0.0f;
            if (ng < 0.0f) ng = 0.0f;
            if (nb < 0.0f) nb = 0.0f;
            rgb[i * 3 + 0] = nr;
            rgb[i * 3 + 1] = ng;
            rgb[i * 3 + 2] = nb;
        }
    });
}

/* Vignette (spatial op #8, runs last). Per-pixel radial exposure gain about the centre;
   see the constants block above for the mask/feather model. A pure per-pixel op (no
   neighbourhood), but kept out of the fused point pass since it needs pixel coordinates,
   not just the channel value. Custom / off-centre vignettes are handled by radial masks,
   so this global op is centre-only. */
void Develop::Vignette(WorkingImage &img, const EditParams &p)
{
    const float ev = p.vignetteExposure;                 // EV applied at the corners
    if (ev == 0.0f) return;

    const int w = img.width;
    const int h = img.height;
    if (w < 1 || h < 1) return;
    const size_t n = static_cast<size_t>(w) * static_cast<size_t>(h);
    float *rgb = img.rgb.data();

    /* Elliptical normalisation: dx,dy in [-1,1] across the frame, so the radius reaches 1
       at the mid-edges and sqrt(2) at the corners; dividing by sqrt(2) puts full effect
       at the corners. */
    const float cx = (w - 1) * 0.5f;
    const float cy = (h - 1) * 0.5f;
    const float invHalfW = (w > 1) ? 2.0f / (w - 1) : 0.0f;
    const float invHalfH = (h > 1) ? 2.0f / (h - 1) : 0.0f;
    const float invCorner = 0.70710678f;                 // 1 / sqrt(2)

    /* Feather -> falloff exponent. High feather spreads the effect inward (low k); low
       feather concentrates it in the corners (high k). */
    const float feather = qBound(0.0f, p.vignetteFeather, 1.0f);
    const float k = kVignetteFeatherKMin +
                    (1.0f - feather) * (kVignetteFeatherKMax - kVignetteFeatherKMin);

    parallelFor(n, [=](size_t i0, size_t i1) {
        for (size_t i = i0; i < i1; ++i) {
            const int x = static_cast<int>(i % static_cast<size_t>(w));
            const int y = static_cast<int>(i / static_cast<size_t>(w));
            const float dx = (x - cx) * invHalfW;         // -1..1
            const float dy = (y - cy) * invHalfH;         // -1..1
            float rn = std::sqrt(dx * dx + dy * dy) * invCorner;   // 0 centre -> 1 corner
            if (rn > 1.0f) rn = 1.0f;
            const float mask = std::pow(rn, k);           // 0 centre -> 1 corner
            const float gain = std::exp2(ev * mask);      // ev stops at corner
            rgb[i * 3 + 0] *= gain;
            rgb[i * 3 + 1] *= gain;
            rgb[i * 3 + 2] *= gain;
        }
    });
}

/* Sharpen (spatial op #8.5, between Vignette and Grain). Capture sharpening: an unsharp
   mask on perceptual luminance, folded back as a ratio-preserving RGB scale so hue and
   chroma are untouched -- the same house idiom as Texture / Denoise / Grain.

   THE ONE OP THAT IS NOT SCALE-INVARIANT. Texture and Dehaze derive their sigma from
   max(w,h) so the proxy and the full-res settle render shape the same RELATIVE band.
   Acutance is a property of the pixel grid, so sharpenRadius is an ABSOLUTE pixel radius
   and the effective sigma is scaled by img.renderScale instead: the proxy shows a
   scale-honest preview, but the true result is the full-res one, judged at 1:1.

   Detail soft-thresholds the smallest amplitudes away; Masking gates the whole thing by
   local edge strength so flat sky / skin is left alone. Both live in Develop/sharpen.h.
   The Gaussian here is small (sigma <= 3), so unlike Texture there is no downscale
   trick -- a separable blur at this radius is already cheap. No-op at amount 0. */
void Develop::Sharpen(WorkingImage &img, const EditParams &p)
{
    const float amount = qMax(0.0f, p.sharpenAmount);
    if (amount == 0.0f) return;

    const int w = img.width;
    const int h = img.height;
    if (w < 3 || h < 3) return;                      // no neighbourhood to work with

    cv::Mat Yp;
    std::vector<float> Ylin;
    buildPerceptualLuma(img, Yp, Ylin);

    const float scale = (img.renderScale > 0.0f) ? img.renderScale : 1.0f;
    const float sigma = Sharpen::effectiveSigma(p.sharpenRadius, scale);

    cv::Mat base;
    cv::GaussianBlur(Yp, base, cv::Size(0, 0), sigma);

    /* Edge strength for the mask gate. Only computed when Masking is actually engaged --
       at 0 the gate is 1 everywhere and the Sobel pair would be pure waste. */
    const float masking = qBound(0.0f, p.sharpenMasking, 1.0f);
    cv::Mat grad;
    if (masking > 0.0f) {
        /* Gradient of the BLURRED luma, so noise does not read as an edge. The 3x3 Sobel
           kernel has a gain of 8 (a ramp of slope s answers 8s), so scale by 1/8 to get
           the TRUE per-pixel slope -- Sharpen::maskKnee is expressed in perceptual-luma
           units per pixel, and unscaled Sobel would open the gate ~8x too easily. */
        constexpr double kSobelNorm = 1.0 / 8.0;
        cv::Mat gx, gy;
        cv::Sobel(base, gx, CV_32F, 1, 0, 3, kSobelNorm);
        cv::Sobel(base, gy, CV_32F, 0, 1, 3, kSobelNorm);
        cv::magnitude(gx, gy, grad);
    }

    const float detail = qBound(0.0f, p.sharpenDetail, 1.0f);
    const float *bp = base.ptr<float>();
    const float *gp = grad.empty() ? nullptr : grad.ptr<float>();
    shapeAndFoldLuma(img, Yp.ptr<float>(), Ylin.data(), [=](size_t i, float yp) {
        const float gm = gp ? gp[i] : 0.0f;
        return Sharpen::applyPixel(yp, bp[i], gm, amount, detail, masking, scale);
    });
}

/* Grain (spatial op #9, runs LAST). Monochromatic film grain: a deterministic noise field
   is generated on a coarse grain-cell grid (size sets the cell), bilinearly upsampled,
   its amplitude modulated by a lower-frequency roughness field. It is added to PERCEPTUAL
   luminance and folded back as a ratio-preserving RGB scale (like Texture) so only the
   luminance is perturbed. A midtone weight fades the grain toward pure black / white. See
   the grain constants block for the size / roughness / proxy-match model. No-op at 0. */
void Develop::Grain(WorkingImage &img, const EditParams &p)
{
    const float amount = qBound(0.0f, p.grainAmount, 1.0f);
    if (amount == 0.0f) return;

    const int w = img.width;
    const int h = img.height;
    if (w < 1 || h < 1) return;
    const size_t n = static_cast<size_t>(w) * static_cast<size_t>(h);
    float *rgb = img.rgb.data();
    const float white = (img.white > 0.0f) ? img.white : 1.0f;
    const float invWhite = 1.0f / white;

    /* Grain cell size in pixels: scales with the long edge (so the proxy and full-res
       render show the same grain proportionally) and grows with grainSize (fine to
       coarse). The noise grid divides the image into cells: bigger cell = bigger grain. */
    const float size01 = qBound(0.0f, p.grainSize, 1.0f);
    const double unit = qMax(1.0, static_cast<double>(qMax(w, h)) / kGrainCellUnit);
    const double cellPx = unit * (kGrainCellMin + size01 * (kGrainCellMax - kGrainCellMin));
    const int nw = qMax(1, static_cast<int>(std::lround(w / cellPx)));
    const int nh = qMax(1, static_cast<int>(std::lround(h / cellPx)));

    /* Fixed-seed noise -> stable across re-renders (no shimmer). Grain N(0,1) on the cell
       grid, upsampled to full res; the roughness field is a coarser [0,1] grid that patchily
       modulates the grain amplitude (0 roughness = uniform, 1 = strongly varying). */
    cv::RNG rng(kGrainSeed);
    cv::Mat grainSmall(nh, nw, CV_32FC1);
    rng.fill(grainSmall, cv::RNG::NORMAL, 0.0, 1.0);
    cv::Mat grain;
    cv::resize(grainSmall, grain, cv::Size(w, h), 0, 0, cv::INTER_LINEAR);

    const float rough01 = qBound(0.0f, p.grainRoughness, 1.0f);
    cv::Mat roughUp;
    if (rough01 > 0.0f) {
        const int rw = qMax(1, nw / kGrainRoughFreqDiv);
        const int rh = qMax(1, nh / kGrainRoughFreqDiv);
        cv::Mat roughSmall(rh, rw, CV_32FC1);
        rng.fill(roughSmall, cv::RNG::UNIFORM, 0.0, 1.0);
        cv::resize(roughSmall, roughUp, cv::Size(w, h), 0, 0, cv::INTER_LINEAR);
    }

    const float strength = amount * kGrainStrength;
    const float *gp = grain.ptr<float>();
    const float *rp = roughUp.empty() ? nullptr : roughUp.ptr<float>();
    constexpr float kEps = 1e-6f;
    parallelFor(n, [=](size_t i0, size_t i1) {
        for (size_t i = i0; i < i1; ++i) {
            const float r = rgb[i * 3 + 0], g = rgb[i * 3 + 1], b = rgb[i * 3 + 2];
            const float Y = kLumR * r + kLumG * g + kLumB * b;
            if (Y <= kEps) continue;
            float nrm = Y * invWhite;
            if (nrm < 0.0f) nrm = 0.0f;
            const float s = std::pow(nrm, kInvGamma);
            /* Midtone weight: peaks at mid-grey, fades to 0 at black / white; sqrt broadens
               it so shadows and highlights still carry some grain. Shared with Clarity --
               the expression is unchanged, so grain renders exactly as before. */
            const float lw = LocalContrast::midtoneWeight(s);
            /* Roughness: mean-1 amplitude that varies more as roughness rises (u in [0,1]). */
            const float amp = rp ? (1.0f - rough01 + rough01 * 2.0f * rp[i]) : 1.0f;
            float s2 = s + strength * lw * amp * gp[i];
            if (s2 < 0.0f) s2 = 0.0f;
            const float Yd = std::pow(s2, kGamma) * white;
            const float ratio = Yd / Y;
            rgb[i * 3 + 0] = r * ratio;
            rgb[i * 3 + 1] = g * ratio;
            rgb[i * 3 + 2] = b * ratio;
        }
    });
}

std::shared_ptr<const Develop::PointCoeffs> Develop::buildPointCoeffs(const EditParams &p,
                                                                      const WorkingImage &img,
                                                                      float *hiWeight)
{
    /* On the heap: the tables make PointCoeffs too big for a pool thread's stack. */
    const std::shared_ptr<PointCoeffs> coeffs = std::make_shared<PointCoeffs>();
    PointCoeffs &c = *coeffs;

    /* Exposure: a scene-linear gain of 2^EV (a +1 EV slider doubles the linear value). */
    const float exposureGain = (p.exposure != 0.0f) ? std::exp2(p.exposure) : 1.0f;

    /* White balance: per-channel scene-linear gains for the absolute (Kelvin, tint),
       taken RELATIVE to the as-shot rendering so an untouched image (p.temp == 0) is
       an exact no-op. Folded with the uniform exposure gain AND the Colour RGB
       sliders (per-channel gain, kRgbGain) into a single per-channel factor -- all
       are linear multiplies, so they commute and the fused pass applies one multiply
       per channel. */
    float wbGain[3] = {1.0f, 1.0f, 1.0f};
    /*
        A CAMERA PROFILE CARRIES THE WHITE BALANCE, so there are no gains to apply here:
        under DNG the chosen white sets diag(1/referenceNeutral) INSIDE the input matrix
        (see Develop/cameraprofile.h). Applying the gains as well would white-balance
        twice -- which looks like a plausible, slightly-too-warm picture rather than like
        a bug.

        Tested on cam.profile rather than on the image's space tag, because by the time an
        active local Denoise has run ToWorkingSpace the pixels are already converted and
        the tag no longer says where they came from -- but the profile is still on `cam`,
        which is exactly why ToWorkingSpace leaves it there.
    */
    const bool profileHasWb = img.cam.valid && img.cam.profile != nullptr;
    if (!profileHasWb && (p.temp > 0.0f || p.tint != 0.0f)) {
        float kelvin, tint;
        WhiteBalance::resolve(img.cam, p.temp, p.tint, kelvin, tint);
        WhiteBalance::relativeGains(img.cam, kelvin, tint, wbGain);
    }
    const float rGain = 1.0f + kRgbGain * (p.red   / kRgbFullScale);
    const float gGain = 1.0f + kRgbGain * (p.green / kRgbFullScale);
    const float bGain = 1.0f + kRgbGain * (p.blue  / kRgbFullScale);
    c.channelGain[0] = wbGain[0] * exposureGain * rGain;
    c.channelGain[1] = wbGain[1] * exposureGain * gGain;
    c.channelGain[2] = wbGain[2] * exposureGain * bGain;

    c.white = (img.white > 0.0f ? img.white : 1.0f);

    /* HSL (hue/saturation/luminance): a cross-channel block applied after the tone curve. Hue
       rotates about the neutral (1,1,1) axis (Rodrigues rotation of RGB space, which fixes the
       gray axis so neutrals stay neutral); saturation scales chroma about luma; luminance is a
       uniform gain. */
    const float hue = p.hue        / kHslFullScale;     // -1..1
    const float sat = p.saturation / kHslFullScale;
    const float vib = p.vibrance   / kHslFullScale;
    const float lum = p.luminance  / kHslFullScale;
    c.satFactor = 1.0f + sat;                          // -1 -> grayscale, +1 -> 2x chroma
    c.vibAmount = vib;                                 // per-pixel weighted chroma boost
    c.lumGain   = 1.0f + lum;                          // -1 -> black, +1 -> 2x
    if (hue != 0.0f) {
        const float a = hue * kHueMaxRad;
        const float cs = std::cos(a);
        const float sn = std::sin(a);
        const float k  = (1.0f - cs) / 3.0f;            // axis (1,1,1)/sqrt3 -> k = (1-cos)*nx*ny
        const float w  = sn * 0.57735027f;              // sin * 1/sqrt(3)
        c.hueMat[0] = cs + k;       c.hueMat[1] = k - w;       c.hueMat[2] = k + w;
        c.hueMat[3] = k + w;        c.hueMat[4] = cs + k;      c.hueMat[5] = k - w;
        c.hueMat[6] = k - w;        c.hueMat[7] = k + w;       c.hueMat[8] = cs + k;
    }
    c.hslActive = (c.satFactor != 1.0f) || (c.vibAmount != 0.0f) ||
                  (c.lumGain != 1.0f) || (hue != 0.0f);

    /* Calibration: re-point the R/G/B primaries into one 3x3 matrix. Runs in linear light
       before the tone curve, so it is built here but applied earlier in the fused loop
       than HSL. All six sliders at 0 leaves the identity matrix, which we skip. */
    c.calActive = !Calibrate::isIdentity(p.calRedHue, p.calRedSat, p.calGreenHue,
                                         p.calGreenSat, p.calBlueHue, p.calBlueSat);
    if (c.calActive)
        Calibrate::buildMatrix(p.calRedHue, p.calRedSat, p.calGreenHue, p.calGreenSat,
                               p.calBlueHue, p.calBlueSat, c.calMat);

    /* Colour grading (see PointCoeffs and Develop/colorgrade.h). Each range's hue + sat
       becomes a push vector in gamma-2.2 ProPhoto, amp * sat * (dir - offset); the amp
       comes from the range's measured window at the pixel's L*, so the four ranges fold
       into ONE table of summed push by luminance. The transfer tables follow the active
       view transform, because the windows and the push were measured on what Lightroom
       DISPLAYS -- keyed on scene values the windows land in the wrong place. */
    {
        const float gHue[4] = {p.gradeShadowHue, p.gradeMidHue, p.gradeHighHue,
                               p.gradeGlobalHue};
        const float gSat[4] = {p.gradeShadowSat, p.gradeMidSat, p.gradeHighSat,
                               p.gradeGlobalSat};
        const float gLum[4] = {p.gradeShadowLum, p.gradeMidLum, p.gradeHighLum,
                               p.gradeGlobalLum};
        /* Per range, the push at window amplitude 1: sat * (dir - offset) for the hue,
           plus kLumScale * lum on every channel for Luminance (a uniform offset in the
           same space, shaped by the same window -- see colorgrade.h). */
        float vec[4][3] = {};
        c.gradeActive = false;
        for (int r = 0; r < 4; ++r) {
            const float sat = qBound(0.0f, gSat[r], 1.0f);
            const float off = ColorGrade::gradeLightOffset(r, p.gradeBlending,
                                                           p.gradeBalance);
            const float lum = ColorGrade::kLumScale * (gLum[r] / kGradeLumFullScale);
            float dir[3] = {0.0f, 0.0f, 0.0f};
            if (sat > 0.0f) ColorGrade::gradePushDir(gHue[r], dir);
            for (int k = 0; k < 3; ++k) vec[r][k] = sat * (dir[k] - off) + lum;
            if (sat > 0.0f || lum != 0.0f) c.gradeActive = true;
        }
        /* Point Color: the points that move something, as precomputed coefficients. A
           visualised point is kept even when inert -- showing its selection is the
           whole point of the toggle before anything has been shifted. */
        c.pcN = 0;
        c.pcVisualize = -1;
        for (int i = 0; i < p.pointColorN && i < PointColor::kMaxPts; ++i) {
            const bool vis = (i == p.pointColorVisualize);
            if (p.pointColor[i].isInert() && !vis) continue;
            if (vis) c.pcVisualize = c.pcN;
            c.pc[c.pcN++] = PointColor::makeCoef(p.pointColor[i]);
        }
        c.pcActive = c.pcN > 0;
        if (c.pcActive) {
            PointColor::workingToLms(c.pcToLms);
            PointColor::lmsToWorking(c.pcFromLms);
        }

        if (c.gradeActive) {
            /* Summed push by display luminance, sqrt-spaced so the shadows (where L*
               moves fastest) get the resolution. */
            for (int i = 0; i < PointCoeffs::kGradeWN; ++i) {
                const float u = float(i) / (PointCoeffs::kGradeWN - 1);
                const float L = ColorGrade::lstarFromY(u * u);
                float push[3] = {0.0f, 0.0f, 0.0f};
                for (int r = 0; r < 4; ++r) {
                    const float amp = ColorGrade::gradeWindowAmp(r, L, p.gradeBlending,
                                                                 p.gradeBalance);
                    for (int k = 0; k < 3; ++k) push[k] += amp * vec[r][k];
                }
                for (int k = 0; k < 3; ++k) c.gradePush[i][k] = push[k];
            }
        }
        if (c.gradeActive || c.pcActive) {
            /* Scene n -> display-linear v through the view transform (the chain ToImage
               runs: DisplayPosition, then undo its sRGB transfer), and the inverse by a
               monotone walk of the same samples. Where the view clips (v flat at 1) the
               walk takes the FIRST n to reach it. */
            const OutputTransform::ViewTransform view =
                OutputTransform::ViewFromInt(p.viewTransform);
            const int N = PointCoeffs::kGradeTfN;
            c.gradeNMax = kToneLutMaxN;
            for (int i = 0; i < N; ++i) {
                const float u = float(i) / (N - 1);
                const float n = c.gradeNMax * u * u;
                c.gradeFwd[i] = displayToLinear(
                    OutputTransform::DisplayPosition(std::pow(n, kInvGamma), view));
            }
            int j = 0;
            for (int i = 0; i < N; ++i) {
                const float u = float(i) / (N - 1);
                const float v = u * u;
                while (j < N - 2 && c.gradeFwd[j + 1] < v) ++j;
                const float f0 = c.gradeFwd[j], f1 = c.gradeFwd[j + 1];
                float t = (f1 > f0) ? (v - f0) / (f1 - f0) : 0.0f;
                t = qBound(0.0f, t, 1.0f);
                const float u0 = float(j) / (N - 1), u1 = float(j + 1) / (N - 1);
                const float uu = u0 + (u1 - u0) * t;
                c.gradeInv[i] = c.gradeNMax * uu * uu;
            }
        }
        if (c.gradeActive) {
            /* The push's own encoding, ProPhoto P <-> P^(1/2.2). */
            const int N = PointCoeffs::kGradeTfN;
            c.gradeEncMax = 1.5f;
            c.gradeDecMax = std::pow(c.gradeEncMax, 1.0f / ColorGrade::kPushGamma) + 0.5f;
            for (int i = 0; i < N; ++i) {
                const float u = float(i) / (N - 1);
                c.gradeEnc[i] = std::pow(c.gradeEncMax * u * u,
                                         1.0f / ColorGrade::kPushGamma);
                c.gradeDec[i] = std::pow(c.gradeDecMax * u, ColorGrade::kPushGamma);
            }
        }
    }

    /*
        FOLD THE FRONT OF THE PIPELINE INTO ONE MATRIX.

        For camera-native input (a raw that RawColor deliberately left unconverted) the
        first three stages are all linear:

            InputMatrix                      the input profile -- what the decoder used
                                             to bake in, moved here so a profile change
                                             is a re-render, not a re-decode. With a
                                             camera profile selected this is the profile's
                                             matrix and the white balance is INSIDE it
            diag(channelGain)                white balance x exposure x RGB sliders
            calMat                           the Calibrate primaries rotation

        so they multiply out to a single 3x3. channelGain and calMat are then cleared:
        they are IN preMat, and applying them twice is the obvious bug here.
    */
    float inMat[3][3];
    bool inWbIncluded = false;
    if (img.space == ColorSpaceMath::ColorSpace::CameraNative &&
        InputMatrix(img, &p, inMat, inWbIncluded)) {
        /* The input profile -- a camera profile when one is selected, otherwise
           camToWorking . diag(asShotMul). ONE definition, shared with ToWorkingSpace. */
        float m[3][3];
        for (int i = 0; i < 3; ++i)
            for (int j = 0; j < 3; ++j) m[i][j] = inMat[i][j];

        /* diag(channelGain) . m -- a diagonal on the LEFT scales ROW i. */
        for (int i = 0; i < 3; ++i)
            for (int j = 0; j < 3; ++j) m[i][j] *= c.channelGain[i];

        /* calMat . m, when the Calibrate panel is doing anything. */
        if (c.calActive) {
            float o[3][3];
            for (int i = 0; i < 3; ++i)
                for (int j = 0; j < 3; ++j) {
                    float s = 0.0f;
                    for (int k = 0; k < 3; ++k) s += c.calMat[i * 3 + k] * m[k][j];
                    o[i][j] = s;
                }
            for (int i = 0; i < 3; ++i)
                for (int j = 0; j < 3; ++j) m[i][j] = o[i][j];
        }

        for (int i = 0; i < 3; ++i)
            for (int j = 0; j < 3; ++j) c.preMat[i * 3 + j] = m[i][j];
        c.preMatActive = true;
        c.preMatClamps = c.calActive;    // the calibrate block clamps; the fold must too
        c.calActive    = false;                                  // folded into preMat
        c.channelGain[0] = c.channelGain[1] = c.channelGain[2] = 1.0f;   // ditto
    }

    /* Perceptual tone curve = contrast + the four tone-region controls + the Curves
       panel's point curves, baked into a per-channel 1-D LUT (see PointCoeffs). Everything here is in the PERCEPTUAL (gamma) domain, NOT scene-linear:
       a contrast/tone curve in linear pivots around 0.18 and sends highlights (several stops
       above the pivot) far too high. Encoding to ~gamma first puts mid-grey near 0.46 so the
       curve shapes tones evenly, like a normal tone control.

       Contrast is a gentle slope about mid-grey: the -100..100 slider normalises to amount
       -1..1, mapping to 1 + 0.4*amount -> [0.6, 1.4] (kContrastSlopeRange); >1 steepens.
       The tone regions add a Gaussian-weighted lift on top (see kToneShift / toneWeight).

       Built AFTER the front matrix above: Highlights -'s strength depends on where the
       frame's pixels sit on the display axis, which is those pixels through exactly
       that matrix (exposure, white balance, profile). */
    ToneShape shape = buildToneShape(p);
    constexpr int kLumBins = 2048;
    constexpr float kLumBinSMax = 4.0f;
    std::vector<long long> lumBins;    // Highlights -: frame luminance, for the anchor
    if (shape.hi < 0.0f) {
        float wMean = hiWeight ? *hiWeight : -1.0f;
        if (wMean < 0.0f) {
            /* The band's mean weight over this image, per channel -- the statistic the
               budget was fitted on. The front stages mirror applyPointOps; d0 is taken
               where applyToneShape takes it, before the contrast slope. The values are
               binned on the perceptual axis and the band evaluated once per bin: a few
               thousand DisplayPosition calls instead of three per pixel, which was the
               whole cost of a StatSample's develop. Strided to at most ~64k pixels; on a
               StatSample (the normal caller) that is all of them. */
            constexpr int kBins = 2048;
            constexpr float kBinSMax = 4.0f;             // n up to ~21; above: exact
            std::vector<long long> bins(kBins, 0);
            const int w = img.width, h = img.height;
            const int step = qMax(1, int(std::ceil(std::sqrt(double(w) * h / 65536.0))));
            const float invWhite = 1.0f / c.white;
            const float *pm = c.preMat, *cm = c.calMat, *g = c.channelGain;
            auto weightAt = [&shape](float sv) {
                const float d0 = OutputTransform::DisplayPosition(sv, shape.view);
                return toneBand(d0, kHighsDown, shape.hiShift, shape.reach);
            };
            double sum = 0.0;
            long long n = 0;
            for (int y = 0; y < h; y += step) {
                const float *row = img.rgb.data() + size_t(y) * size_t(w) * 3;
                for (int x = 0; x < w; x += step) {
                    const float *px = row + size_t(x) * 3;
                    float v[3] = {px[0], px[1], px[2]};
                    if (c.preMatActive) {
                        const float r = v[0], gg = v[1], b = v[2];
                        v[0] = pm[0] * r + pm[1] * gg + pm[2] * b;
                        v[1] = pm[3] * r + pm[4] * gg + pm[5] * b;
                        v[2] = pm[6] * r + pm[7] * gg + pm[8] * b;
                    }
                    for (int k = 0; k < 3; ++k) v[k] *= g[k];
                    if (c.calActive) {
                        const float r = v[0], gg = v[1], b = v[2];
                        v[0] = cm[0] * r + cm[1] * gg + cm[2] * b;
                        v[1] = cm[3] * r + cm[4] * gg + cm[5] * b;
                        v[2] = cm[6] * r + cm[7] * gg + cm[8] * b;
                    }
                    for (int k = 0; k < 3; ++k) {
                        const float nv = v[k] * invWhite;
                        const float sv = nv > 0.0f ? std::pow(nv, kInvGamma) : 0.0f;
                        if (sv < kBinSMax) ++bins[size_t(sv * (kBins / kBinSMax))];
                        else sum += weightAt(sv);
                    }
                    n += 3;
                }
            }
            for (int i = 0; i < kBins; ++i)
                if (bins[size_t(i)])
                    sum += bins[size_t(i)] * weightAt((i + 0.5f) * (kBinSMax / kBins));
            wMean = n ? float(sum / double(n)) : 0.0f;
            if (hiWeight) *hiWeight = wMean;
        }
        const float budget = kHighsDownBudget *
                             (1.0f + kHighsDownBudgetPerEV * qMax(0.0f, p.exposure));
        shape.hiDownAmp = qMin(kHighsDownAmpMax, budget / qMax(wMean, 1e-4f));

        /* The frame's luminance on the perceptual axis, binned -- the anchor (its
           log-mean in display-linear light) is resolved once the tone tables exist. */
        {
            const int w = img.width, h = img.height;
            const int step = qMax(1, int(std::ceil(std::sqrt(double(w) * h / 65536.0))));
            const float invWhite = 1.0f / c.white;
            const float *pm = c.preMat, *cm = c.calMat, *g = c.channelGain;
            lumBins.assign(kLumBins, 0);
            for (int y = 0; y < h; y += step) {
                const float *row = img.rgb.data() + size_t(y) * size_t(w) * 3;
                for (int x = 0; x < w; x += step) {
                    const float *px = row + size_t(x) * 3;
                    float v[3] = {px[0], px[1], px[2]};
                    if (c.preMatActive) {
                        const float r = v[0], gg = v[1], b = v[2];
                        v[0] = pm[0] * r + pm[1] * gg + pm[2] * b;
                        v[1] = pm[3] * r + pm[4] * gg + pm[5] * b;
                        v[2] = pm[6] * r + pm[7] * gg + pm[8] * b;
                    }
                    for (int k = 0; k < 3; ++k) v[k] *= g[k];
                    if (c.calActive) {
                        const float r = v[0], gg = v[1], b = v[2];
                        v[0] = cm[0] * r + cm[1] * gg + cm[2] * b;
                        v[1] = cm[3] * r + cm[4] * gg + cm[5] * b;
                        v[2] = cm[6] * r + cm[7] * gg + cm[8] * b;
                    }
                    const float Y = (kLumR * v[0] + kLumG * v[1] + kLumB * v[2]) * invWhite;
                    const float sv = Y > 0.0f ? std::pow(Y, kInvGamma) : 0.0f;
                    ++lumBins[size_t(qBound(0, int(sv * (kLumBins / kLumBinSMax)), kLumBins - 1))];
                }
            }
        }

    }

    /* The Curves panel's tone curve (Develop/tonecurve.h). Channel 0 is the RGB
       composite; 1/2/3 are the per-channel Red / Green / Blue curves layered on top of
       it. Built once here (n <= 16 control points each), then evaluated per LUT entry --
       never per pixel. */
    ToneCurve::Spline curve[ToneCurve::kChannels];
    for (int ch = 0; ch < ToneCurve::kChannels; ++ch)
        curve[ch].build(p.curveX[ch], p.curveY[ch], p.curveN[ch]);
    const bool curveActive = !p.curvesAreIdentity();

    c.toneActive = shape.active || curveActive;
    if (c.toneActive) {
        const float sMax = std::pow(shape.lutMaxN, kInvGamma);
        c.toneLutSMax = sMax;
        for (int j = 0; j < PointCoeffs::kLutSize; ++j) {
            /* s = the perceptual input value this table entry represents. */
            float s = (static_cast<float>(j) / (PointCoeffs::kLutSize - 1)) * sMax;
            s = applyToneShape(shape, s);                    // contrast + tone regions
            /* The tone curve runs AFTER the parametric controls above, matching
               Lightroom's order: the RGB composite first, then that channel's own curve
               on the composite's output. Both extrapolate past x = 1 with their end
               slope, so the ~3 stops of highlight headroom in this table stay smooth. */
            if (curveActive) s = curve[0].eval(s);
            for (int ch = 0; ch < 3; ++ch) {
                float sc = curveActive ? curve[ch + 1].eval(s) : s;
                if (sc < 0.0f) sc = 0.0f;                    // crush blacks gracefully
                /* decode -> white-normalised linear */
                c.toneLut[ch][j] = std::pow(sc, kGamma);
            }
        }
        /* MONOTONICITY, enforced once over the finished table. A brighter input must never
           render darker than a dimmer one: a locally inverted tone shows as a bright halo
           inside a smooth ramp, and it is the same failure Develop/tonecurve.h picks
           Fritsch-Carlson over a plain cubic to avoid on the point curve. The parametric
           side had no equivalent guard, and opposing sliders could invert it -- Blacks +100
           with Shadows -100 did, 20 samples into a 256-step ramp.

           HERE RATHER THAN INSIDE applyToneShape, because this is the one place the WHOLE
           composed curve exists: the parametric shape, the RGB composite curve and the
           per-channel curve, in the order they render. Clamping any one of them alone would
           still let the composition invert. It costs one pass over 1024 entries per render
           and nothing per pixel.

           Develop::ParametricCurve does NOT see this clamp -- it draws the parametric shape
           only, which is what the Curves panel's Parametric view is a view of. Where the
           clamp fires, the plot therefore shows a dip the render does not have. Left that
           way deliberately: the alternative is a plot that stops responding to a slider the
           user is still dragging, which reads as a broken control rather than a limit. */
        for (int ch = 0; ch < 3; ++ch) {
            float *lut = c.toneLut[ch];
            for (int j = 1; j < PointCoeffs::kLutSize; ++j)
                if (lut[j] < lut[j - 1]) lut[j] = lut[j - 1];
        }

        /* The local Highlights - tables (see PointCoeffs::hiLocal). The same monotonicity
           guard, moved to the display axis: the band-free curve is clamped, the full
           curve is clamped, and the shift is their difference -- so with no region
           correction a pixel renders exactly toneLut, and the shift stays <= 0. */
        if (shape.hi < 0.0f) {
            float dispMax = 0.0f, fullMax = 0.0f, posMax = 0.0f;
            for (int j = 0; j < PointCoeffs::kLutSize; ++j) {
                const float s = (static_cast<float>(j) / (PointCoeffs::kLutSize - 1)) * sMax;
                const ToneParts tp = toneShapeParts(shape, s);
                const float full = tp.d + toneHighlightShift(shape, tp.d0);
                dispMax = qMax(dispMax, tp.d);
                fullMax = qMax(fullMax, full);
                posMax = qMax(posMax, tp.d0);
                c.hiDispLut[j]  = dispMax;
                c.hiShiftLut[j] = qMin(0.0f, fullMax - dispMax);
                c.hiPosLut[j]   = posMax;
            }
            for (int k = 0; k < PointCoeffs::kBackSize; ++k) {
                const float d = float(k) / (PointCoeffs::kBackSize - 1);
                float s = d > 0.0f ? OutputTransform::PerceptualFromDisplay(d, shape.view,
                                                                             shape.lutMaxN)
                                   : 0.0f;
                if (curveActive) s = curve[0].eval(s);
                for (int ch = 0; ch < 3; ++ch) {
                    float sc = curveActive ? curve[ch + 1].eval(s) : s;
                    if (sc < 0.0f) sc = 0.0f;
                    float v = std::pow(sc, kGamma);
                    if (k > 0) v = qMax(v, c.hiBackLut[ch][k - 1]);
                    c.hiBackLut[ch][k] = v;
                }
            }
            /* The band's shift by d0 -- the axis the field is built and read on --
               inverting hiPosLut. */
            int j = 0;
            for (int k = 0; k < PointCoeffs::kBackSize; ++k) {
                const float d = float(k) / (PointCoeffs::kBackSize - 1);
                while (j < PointCoeffs::kLutSize - 1 && c.hiPosLut[j] < d) ++j;
                float shift = c.hiShiftLut[j];
                if (j > 0 && c.hiPosLut[j] > c.hiPosLut[j - 1]) {
                    const float t = (d - c.hiPosLut[j - 1]) /
                                    (c.hiPosLut[j] - c.hiPosLut[j - 1]);
                    shift = c.hiShiftLut[j - 1] +
                            (c.hiShiftLut[j] - c.hiShiftLut[j - 1]) * qBound(0.0f, t, 1.0f);
                }
                c.hiShiftByPos[k] = shift;
            }
            c.hiLocal = true;

            /* The frame-relative term. Anchor = the frame's log-mean luminance, measured
               on the band-free DISPLAY value in display-linear light (not d0: Whites +
               clips the display flat, and one flat value must get one gain, or the
               clipped span inverts); a tone L loses amount * (k * (L - anchor)^p + o)
               stops, scaled by the frame's range and tapered toward white. */
            double acc = 0.0;
            long long cnt = 0;
            const float lutScaleS = (PointCoeffs::kLutSize - 1) / sMax;
            auto dispAtS = [&](float sv) {
                const float fi = qMin(sv * lutScaleS, float(PointCoeffs::kLutSize - 1));
                const int i = qMin(int(fi), PointCoeffs::kLutSize - 2);
                return c.hiDispLut[i] + (c.hiDispLut[i + 1] - c.hiDispLut[i]) * (fi - i);
            };
            auto logLin = [](float d) {
                return std::log2(qMax(displayToLinear(qBound(0.0f, d, 1.0f)), 1e-6f));
            };
            for (int i = 0; i < kLumBins; ++i) {
                if (!lumBins.empty() && lumBins[size_t(i)]) {
                    acc += lumBins[size_t(i)] *
                           logLin(dispAtS((i + 0.5f) * (kLumBinSMax / kLumBins)));
                    cnt += lumBins[size_t(i)];
                }
            }
            const float anchor = cnt ? float(acc / double(cnt)) : -6.0f;
            /* The frame's range above the anchor: its 99th percentile, same axis. */
            float top = anchor;
            if (cnt) {
                long long run = 0;
                for (int i = 0; i < kLumBins; ++i) {
                    run += lumBins[size_t(i)];
                    if (run >= cnt * 99 / 100) {
                        top = logLin(dispAtS((i + 0.5f) * (kLumBinSMax / kLumBins)));
                        break;
                    }
                }
            }
            const float range = qMax(0.0f, top - anchor);
            /* Stronger the further the frame's highlights stand above its anchor (see
               the note by kHighsAnchorRangeB). */
            const float rangeGain = std::exp(kHighsAnchorRangeB * (range - kHighsAnchorRange0));
            const float amount = std::pow(qMin(1.0f, -shape.hi), kHighsAnchorQ);
            const float aK = kHighsAnchorK * rangeGain, aP = kHighsAnchorP;
            const float aO = kHighsAnchorO * rangeGain, aT = kHighsAnchorTaper;
            auto dropAt = [&](float L) {
                const float d = linearToDisplay(std::exp2(qMin(0.0f, L)));
                const float u = qMax(0.0f, L - anchor);
                const float taper = 1.0f - aT * toneSmooth((d - kHighsAnchorTaperFrom) /
                                                           (1.0f - kHighsAnchorTaperFrom));
                return amount * (aK * std::pow(u, aP) + aO) * taper;
            };
            for (int j = 0; j < PointCoeffs::kLutSize; ++j) {
                const float sv = (static_cast<float>(j) / (PointCoeffs::kLutSize - 1)) * sMax;
                const float L = logLin(dispAtS(sv));
                c.hiLogLumLut[j] = L;
                c.hiGainByS[j] = std::exp2(-dropAt(L));
            }
            for (int k = 0; k < PointCoeffs::kBackSize; ++k) {
                const float L = PointCoeffs::kHiLMin *
                                (1.0f - float(k) / (PointCoeffs::kBackSize - 1));
                c.hiDropByL[k] = dropAt(L);
            }
            for (int k = 0; k < PointCoeffs::kBackSize; ++k) {
                const float t = float(k) / (PointCoeffs::kBackSize - 1);
                c.hiLinOfD[k] = displayToLinear(t);
                c.hiDOfRootLin[k] = linearToDisplay(t * t);
            }
        }
    }

    c.active = (c.channelGain[0] != 1.0f) || (c.channelGain[1] != 1.0f) ||
               (c.channelGain[2] != 1.0f) || c.toneActive || c.calActive ||
               c.hslActive || c.gradeActive || c.pcActive || c.preMatActive;
    return coeffs;
}

/* Sample the parametric tone shape over the DISPLAY range [0,1] for the Curves panel.
   Clamped to [0,1] because it is drawn inside a unit plot; the LUT itself is not clamped
   (it carries ~3 stops of headroom above white), so this is a view of the visible part of
   the same curve, not a different curve. */
void Develop::ParametricCurve(const EditParams &p, float *out, int n)
{
    if (!out || n < 2) return;
    const ToneShape shape = buildToneShape(p);
    for (int i = 0; i < n; ++i) {
        const float s = static_cast<float>(i) / (n - 1);
        out[i] = qBound(0.0f, applyToneShape(shape, s), 1.0f);
    }
}

void Develop::applyPointOps(WorkingImage &img, const PointCoeffs &c,
                            const HiField *hiField)
{
    const int w = img.width;
    const int h = img.height;
    float *rgb = img.rgb.data();

    const float g0 = c.channelGain[0];
    const float g1 = c.channelGain[1];
    const float g2 = c.channelGain[2];
    const float white = c.white;
    const float invWhite = 1.0f / white;
    const bool doGain = (g0 != 1.0f) || (g1 != 1.0f) || (g2 != 1.0f);
    const bool doTone = c.toneActive;
    const bool doCal  = c.calActive;
    const bool doPre  = c.preMatActive;
    const bool doPreClamp = c.preMatClamps;
    const bool doHsl  = c.hslActive;
    const bool doGrade = c.gradeActive;
    const bool doPoint = c.pcActive;

    /* Fused front matrix (input profile + gains + calibrate), local like the rest. */
    const float pm0 = c.preMat[0], pm1 = c.preMat[1], pm2 = c.preMat[2];
    const float pm3 = c.preMat[3], pm4 = c.preMat[4], pm5 = c.preMat[5];
    const float pm6 = c.preMat[6], pm7 = c.preMat[7], pm8 = c.preMat[8];

    /* Calibration matrix, local so the block folds into the inner loop like the rest. */
    const float cm0 = c.calMat[0], cm1 = c.calMat[1], cm2 = c.calMat[2];
    const float cm3 = c.calMat[3], cm4 = c.calMat[4], cm5 = c.calMat[5];
    const float cm6 = c.calMat[6], cm7 = c.calMat[7], cm8 = c.calMat[8];

    /* HSL coeffs (cross-channel, applied after the tone curve). Locals so they fold into the
       inner loop the same way the gain/tone constants do. */
    const float hm0 = c.hueMat[0], hm1 = c.hueMat[1], hm2 = c.hueMat[2];
    const float hm3 = c.hueMat[3], hm4 = c.hueMat[4], hm5 = c.hueMat[5];
    const float hm6 = c.hueMat[6], hm7 = c.hueMat[7], hm8 = c.hueMat[8];
    const float satF = c.satFactor;
    const float vibA = c.vibAmount;
    const bool  doVib = (vibA != 0.0f);
    const float lumG = c.lumGain;

    /* Colour grading: the tables live in c (see PointCoeffs); the two matrices and the
       table scales are hoisted so they fold into the inner loop. lerpU reads a table
       indexed by u in [0,1] (sqrt-spaced tables pass sqrt of their argument). */
    const float *gFwd = c.gradeFwd, *gInv = c.gradeInv;
    const float *gEnc = c.gradeEnc, *gDec = c.gradeDec;
    /* A pointer, never c itself: processRows captures [=], and naming c there copies
       the whole PointCoeffs onto a 512 KB pool-thread stack. */
    const float (*gPush)[3] = c.gradePush;
    const float gInvNMax = 1.0f / c.gradeNMax;
    const float gInvEncMax = 1.0f / c.gradeEncMax;
    const float gInvDecMax = 1.0f / c.gradeDecMax;
    const float *gToPP = ColorGrade::kRec2020ToProPhoto;
    const float *gFromPP = ColorGrade::kProPhotoToRec2020;
    auto lerpU = [](const float *t, int n, float u) -> float {
        if (u <= 0.0f) return t[0];
        const float f = u * (n - 1);
        if (f >= n - 1) return t[n - 1];
        const int i = static_cast<int>(f);
        return t[i] + (t[i + 1] - t[i]) * (f - i);
    };
    constexpr int gTfN = PointCoeffs::kGradeTfN;
    constexpr int gWN  = PointCoeffs::kGradeWN;

    /* Point Color: pointers again, never c (see gPush). */
    const PointColor::Coef *pcK = c.pc;
    const int pcN = c.pcN;
    const int pcVis = c.pcVisualize;
    const float *pcTo = c.pcToLms, *pcFrom = c.pcFromLms;

    /* Perceptual tone curve via the precomputed LUT (contrast + tone regions, see
       buildPointCoeffs). The table is indexed by the perceptual value s = (v/white)^(1/gamma)
       and already returns the white-normalised linear output, so the only transcendental in the
       hot loop is the single encode pow. Constants are local so they fold into the inner loop. */
    const float *lutR = c.toneLut[0];
    const float *lutG = c.toneLut[1];
    const float *lutB = c.toneLut[2];
    const int lutLast = PointCoeffs::kLutSize - 1;
    const float lutScale = lutLast / c.toneLutSMax;
    /* `lut` is the channel's own table -- the three differ only when a per-channel
       Curves R/G/B curve is in play, and are byte-identical otherwise. */
    auto toneCh = [=](const float *lut, float v) -> float {
        float n = v * invWhite;
        if (n < 0.0f) n = 0.0f;
        const float s = std::pow(n, kInvGamma);
        float fi = s * lutScale;
        if (fi >= lutLast) return lut[lutLast] * white;      // clamp above the table's domain
        const int i = static_cast<int>(fi);
        const float frac = fi - i;
        return (lut[i] + (lut[i + 1] - lut[i]) * frac) * white;
    };

    /* Local Highlights - (see PointCoeffs::hiLocal): the channel's band-free display
       value at its own s, plus its own band shift and the field's region correction
       (never past 0: Highlights - does not brighten), back through that channel's
       display -> linear table. */
    const HiField *hb = hiField;
    const bool doHiLocal = doTone && c.hiLocal;
    const bool doHiField = doHiLocal && hb && hb->w > 0 && hb->h > 0;
    const float *hiGain = c.hiGainByS;
    const float *hiLogLum = c.hiLogLumLut;
    const float *hiLinOfD = c.hiLinOfD;
    const float *hiDOfRootLin = c.hiDOfRootLin;
    const float *hiDisp = c.hiDispLut;
    const float *hiShift = c.hiShiftLut;
    const float *hiPos = c.hiPosLut;
    /* Pointers, never c itself, inside the row lambda: it captures by value, and naming
       c there copies the whole PointCoeffs -- its tables run to ~300 KB -- onto a pool
       thread's 512 KB stack (a SIGBUS stack overflow in ___chkstk_darwin, 2026-10-11). */
    const float *hiBackR = c.hiBackLut[0];
    const float *hiBackG = c.hiBackLut[1];
    const float *hiBackB = c.hiBackLut[2];
    const int backLast = PointCoeffs::kBackSize - 1;
    auto atS = [=](const float *t, float sv) {
        const float fi = sv * lutScale;
        if (fi >= lutLast) return t[lutLast];
        const int i = static_cast<int>(fi);
        return t[i] + (t[i + 1] - t[i]) * (fi - i);
    };
    auto atBack = [=](const float *t, float u) {
        const float fu = qBound(0.0f, u, 1.0f) * backLast;
        const int k = qMin(static_cast<int>(fu), backLast - 1);
        return t[k] + (t[k + 1] - t[k]) * (fu - k);
    };
    auto toneChLocal = [=](const float *back, float v, float corr, float gain) -> float {
        float n = v * invWhite;
        if (n < 0.0f) n = 0.0f;
        const float s = std::pow(n, kInvGamma);
        float shift = atS(hiShift, s) + corr;
        if (shift > 0.0f) shift = 0.0f;
        float d = atS(hiDisp, s) + shift;
        if (d < 0.0f) d = 0.0f;
        /* The frame-relative term: a gain on the channel's display-linear light. */
        if (gain < 1.0f) d = atBack(hiDOfRootLin, std::sqrt(atBack(hiLinOfD, d) * gain));
        const float fk = d * backLast;
        if (fk >= backLast) return back[backLast] * white;
        const int k = static_cast<int>(fk);
        return (back[k] + (back[k + 1] - back[k]) * (fk - k)) * white;
    };
    /* This buffer's pixels on the field's grid: a crop (the Detail patch) maps through the
       frame it was cut from, so it reads the field where the loupe does. */
    const int frameW = img.frameW > 0 ? img.frameW : w;
    const int frameH = img.frameH > 0 ? img.frameH : h;
    const int frameX = img.frameW > 0 ? img.frameX : 0;
    const int frameY = img.frameH > 0 ? img.frameY : 0;
    const float gsx = doHiField ? float(hb->w) / frameW : 0.0f;
    const float gsy = doHiField ? float(hb->h) / frameH : 0.0f;

    /* One fused per-pixel kernel over a band of rows. Point ops apply PER CHANNEL (R,G,B
       interleaved), so the inner loop walks a pixel at a time: per-channel linear gain (white
       balance + exposure) then the perceptual tone curve, matching the fixed pipeline order
       (WB/exposure in scene-linear, contrast + tone regions in the gamma domain). */
    auto processRows = [=](int y0, int y1) {
        const int span = w * 3;
        for (int y = y0; y < y1; ++y) {
            float *row = rgb + static_cast<size_t>(y) * span;
            /* The field grid's two rows bracketing this one, for the bilinear read. */
            const float *aRow0 = nullptr, *aRow1 = nullptr;
            const float *bRow0 = nullptr, *bRow1 = nullptr;
            const float *gaRow0 = nullptr, *gaRow1 = nullptr;
            const float *gbRow0 = nullptr, *gbRow1 = nullptr;
            float fy = 0.0f;
            if (doHiField) {
                const float gy = qBound(0.0f, (y + frameY + 0.5f) * gsy - 0.5f,
                                        float(hb->h - 1));
                const int gy0 = static_cast<int>(gy);
                const int gy1 = qMin(gy0 + 1, hb->h - 1);
                fy = gy - gy0;
                aRow0 = hb->a.data() + size_t(gy0) * hb->w;
                aRow1 = hb->a.data() + size_t(gy1) * hb->w;
                bRow0 = hb->b.data() + size_t(gy0) * hb->w;
                bRow1 = hb->b.data() + size_t(gy1) * hb->w;
                gaRow0 = hb->ga.data() + size_t(gy0) * hb->w;
                gaRow1 = hb->ga.data() + size_t(gy1) * hb->w;
                gbRow0 = hb->gb.data() + size_t(gy0) * hb->w;
                gbRow1 = hb->gb.data() + size_t(gy1) * hb->w;
            }
            for (int x = 0; x < w; ++x) {
                float *px = row + static_cast<size_t>(x) * 3;
                if (doPre) {
                    /* Camera-native -> working space, with the white balance, exposure,
                       RGB sliders and Calibrate all folded into this one matrix (see
                       PointCoeffs::preMat). Replaces the gain and calibrate blocks
                       below -- buildPointCoeffs cleared them so they cannot double up.

                       The clamp fires only when CALIBRATE was folded in, because that is
                       the only stage in the sequence that clamps. Without it, negatives
                       are KEPT: an out-of-gamut sensor colour is real data, and the
                       output stage is what decides how to bring it back, not a clamp
                       buried mid-pipeline. */
                    const float r = px[0], g = px[1], b = px[2];
                    float pr = pm0 * r + pm1 * g + pm2 * b;
                    float pg = pm3 * r + pm4 * g + pm5 * b;
                    float pb = pm6 * r + pm7 * g + pm8 * b;
                    if (doPreClamp) {
                        if (pr < 0.0f) pr = 0.0f;
                        if (pg < 0.0f) pg = 0.0f;
                        if (pb < 0.0f) pb = 0.0f;
                    }
                    px[0] = pr; px[1] = pg; px[2] = pb;
                }
                if (doGain) { px[0] *= g0; px[1] *= g1; px[2] *= g2; }
                if (doCal) {
                    /* Re-point the primaries in LINEAR light, before any tone shaping.
                       The matrix fixes the neutral axis, so greys pass through. */
                    const float r = px[0], g = px[1], b = px[2];
                    float cr = cm0 * r + cm1 * g + cm2 * b;
                    float cg = cm3 * r + cm4 * g + cm5 * b;
                    float cb = cm6 * r + cm7 * g + cm8 * b;
                    px[0] = (cr < 0.0f) ? 0.0f : cr;
                    px[1] = (cg < 0.0f) ? 0.0f : cg;
                    px[2] = (cb < 0.0f) ? 0.0f : cb;
                }
                if (doHiLocal) {
                    float Y = (kLumR * px[0] + kLumG * px[1] + kLumB * px[2]) * invWhite;
                    if (Y < 0.0f) Y = 0.0f;
                    const float sY = std::pow(Y, kInvGamma);
                    /* The field's correction where this pixel sits: a * d0 + b, the
                       guided upsample of the frame's local Laplacian, read bilinearly. */
                    float corr = 0.0f;
                    float gain = atS(hiGain, sY);
                    if (doHiField) {
                        const float gx = qBound(0.0f, (x + frameX + 0.5f) * gsx - 0.5f,
                                                float(hb->w - 1));
                        const int gx0 = static_cast<int>(gx);
                        const int gx1 = qMin(gx0 + 1, hb->w - 1);
                        const float fx = gx - gx0;
                        auto bilerp = [=](const float *r0, const float *r1) {
                            const float t = r0[gx0] + (r0[gx1] - r0[gx0]) * fx;
                            const float u = r1[gx0] + (r1[gx1] - r1[gx0]) * fx;
                            return t + (u - t) * fy;
                        };
                        const float pos = atS(hiPos, sY);
                        corr = bilerp(aRow0, aRow1) * pos + bilerp(bRow0, bRow1);
                        /* The frame-relative term by region: log2 gain = ga * L + gb. */
                        const float lg = bilerp(gaRow0, gaRow1) * atS(hiLogLum, sY) +
                                         bilerp(gbRow0, gbRow1);
                        gain = std::exp2(qMin(0.0f, lg));
                    }
                    px[0] = toneChLocal(hiBackR, px[0], corr, gain);
                    px[1] = toneChLocal(hiBackG, px[1], corr, gain);
                    px[2] = toneChLocal(hiBackB, px[2], corr, gain);
                }
                else if (doTone) {
                    px[0] = toneCh(lutR, px[0]);
                    px[1] = toneCh(lutG, px[1]);
                    px[2] = toneCh(lutB, px[2]);
                }
                if (doHsl) {
                    float r = px[0], g = px[1], b = px[2];
                    /* Hue: rotate about the neutral axis (identity matrix when hue == 0). */
                    const float hr = hm0 * r + hm1 * g + hm2 * b;
                    const float hg = hm3 * r + hm4 * g + hm5 * b;
                    const float hb = hm6 * r + hm7 * g + hm8 * b;
                    /* Saturation: scale chroma about Rec.709 luma. Vibrance adds a
                       per-pixel boost weighted by (1 - HSV saturation), so muted pixels
                       move more than already-saturated ones; the two factors combine
                       multiplicatively. */
                    const float Y = kLumR * hr + kLumG * hg + kLumB * hb;
                    float sF = satF;
                    if (doVib) {
                        const float mx = (hr > hg ? (hr > hb ? hr : hb) : (hg > hb ? hg : hb));
                        const float mn = (hr < hg ? (hr < hb ? hr : hb) : (hg < hb ? hg : hb));
                        const float pxSat = (mx > 1e-6f) ? (mx - mn) / mx : 0.0f;  // 0..1
                        sF *= 1.0f + vibA * (1.0f - pxSat);
                        if (sF < 0.0f) sF = 0.0f;        // never invert chroma
                    }
                    r = (Y + sF * (hr - Y)) * lumG;       // luminance: uniform gain
                    g = (Y + sF * (hg - Y)) * lumG;
                    b = (Y + sF * (hb - Y)) * lumG;
                    px[0] = (r < 0.0f) ? 0.0f : r;
                    px[1] = (g < 0.0f) ? 0.0f : g;
                    px[2] = (b < 0.0f) ? 0.0f : b;
                }
                if (doPoint) {
                    /* Display-linear (through the view), OKLab, the summed weighted
                       change of every point, and the CHANGE back to scene-linear -- the
                       Color Grade route, see PointCoeffs::pcActive. */
                    float n[3], v[3];
                    for (int k = 0; k < 3; ++k) {
                        n[k] = px[k] * invWhite;
                        if (n[k] < 0.0f) n[k] = 0.0f;
                        v[k] = lerpU(gFwd, gTfN, std::sqrt(n[k] * gInvNMax));
                    }
                    float lab[3];
                    PointColor::toLab(pcTo, v, lab);
                    const float C = std::sqrt(lab[1] * lab[1] + lab[2] * lab[2]);
                    const float hh = std::atan2(lab[2], lab[1]);
                    float d[3] = {0.0f, 0.0f, 0.0f};
                    float wVis = 0.0f;
                    bool any = false;
                    for (int i = 0; i < pcN; ++i) {
                        float di[3] = {0.0f, 0.0f, 0.0f};
                        const float wi = PointColor::accumulate(pcK[i], lab, C, hh, di);
                        if (wi <= 0.0f) continue;
                        any = true;
                        if (i == pcVis) wVis = wi;
                        for (int k = 0; k < 3; ++k) d[k] += di[k];
                    }
                    if (any || pcVis >= 0) {
                        float o[3] = {lab[0] + d[0], lab[1] + d[1], lab[2] + d[2]};
                        if (o[0] < 0.0f) o[0] = 0.0f;
                        if (pcVis >= 0) { o[1] *= wVis; o[2] *= wVis; }
                        float v2[3];
                        PointColor::fromLab(pcFrom, o, v2);
                        for (int k = 0; k < 3; ++k) {
                            const float vk = v2[k] < 0.0f ? 0.0f
                                                          : (v2[k] > 1.0f ? 1.0f : v2[k]);
                            const float dn = lerpU(gInv, gTfN, std::sqrt(vk))
                                           - lerpU(gInv, gTfN, std::sqrt(v[k]));
                            const float out = (n[k] + dn) * white;
                            px[k] = out < 0.0f ? 0.0f : out;
                        }
                    }
                }
                if (doGrade) {
                    /* Out to the display axis (per channel, through the view), find the
                       pixel's L* -> the summed push (hue + Luminance), add it in
                       gamma-2.2 ProPhoto, then carry the CHANGE back to scene-linear.
                       See PointCoeffs. */
                    float n[3], v[3];
                    for (int k = 0; k < 3; ++k) {
                        n[k] = px[k] * invWhite;
                        if (n[k] < 0.0f) n[k] = 0.0f;
                        v[k] = lerpU(gFwd, gTfN, std::sqrt(n[k] * gInvNMax));
                    }
                    float Y = kLumR * v[0] + kLumG * v[1] + kLumB * v[2];
                    Y = Y < 0.0f ? 0.0f : (Y > 1.0f ? 1.0f : Y);
                    const float fY = std::sqrt(Y) * (gWN - 1);
                    const int   iY = fY >= gWN - 1 ? gWN - 2 : static_cast<int>(fY);
                    const float tY = fY - iY;
                    const float *p0 = gPush[iY], *p1 = gPush[iY + 1];
                    float e[3];
                    for (int k = 0; k < 3; ++k) {
                        float P = gToPP[3 * k] * v[0] + gToPP[3 * k + 1] * v[1]
                                + gToPP[3 * k + 2] * v[2];
                        if (P < 0.0f) P = 0.0f;
                        e[k] = lerpU(gEnc, gTfN, std::sqrt(P * gInvEncMax))
                             + p0[k] + (p1[k] - p0[k]) * tY;
                    }
                    float P2[3];
                    for (int k = 0; k < 3; ++k)
                        P2[k] = lerpU(gDec, gTfN, (e[k] < 0.0f ? 0.0f : e[k]) * gInvDecMax);
                    for (int k = 0; k < 3; ++k) {
                        float v2 = (gFromPP[3 * k] * P2[0] + gFromPP[3 * k + 1] * P2[1]
                                  + gFromPP[3 * k + 2] * P2[2]);
                        v2 = v2 < 0.0f ? 0.0f : (v2 > 1.0f ? 1.0f : v2);
                        const float dn = lerpU(gInv, gTfN, std::sqrt(v2))
                                       - lerpU(gInv, gTfN, std::sqrt(v[k]));
                        const float o = (n[k] + dn) * white;
                        px[k] = o < 0.0f ? 0.0f : o;
                    }
                }
            }
        }
    };

    /* Data-parallel over row ranges within this single render (QtConcurrent + the global
       pool, the same idiom as FocusStack). The interactive edit is one image, so splitting
       its rows is what uses all cores; a background decoder that also calls render() shares
       the fixed-size pool, which bounds oversubscription. */
    const int maxThreads = qMax(1, QThreadPool::globalInstance()->maxThreadCount());
    const int kMinRowsPerChunk = 64;
    if (maxThreads == 1 || h < kMinRowsPerChunk * 2) {
        processRows(0, h);
        return;
    }

    const int chunks = qMin(maxThreads, (h + kMinRowsPerChunk - 1) / kMinRowsPerChunk);
    const int rowsPerChunk = (h + chunks - 1) / chunks;
    QVector<QFuture<void>> futures;
    futures.reserve(chunks);
    for (int k = 0; k < chunks; ++k) {
        const int y0 = k * rowsPerChunk;
        const int y1 = qMin(h, y0 + rowsPerChunk);
        if (y0 >= y1) break;
        futures.append(QtConcurrent::run(QThreadPool::globalInstance(),
                                         [processRows, y0, y1]() { processRows(y0, y1); }));
    }
    for (QFuture<void> &f : futures) f.waitForFinished();
}
