#ifndef COLORGRADE_H
#define COLORGRADE_H

#include <cmath>

/*
    Pure colour-grading math, shared by the Develop point-op pipeline (Develop::build /
    applyPointOps) and its unit test. No Qt, no OpenCV, so it stays header-only and
    testable in isolation.

    THE MODEL IS LIGHTROOM'S, MEASURED (2026-10-12, 2018-01-14_0010.NEF; LR exports in
    _LRref, analysis in notes/Documentation.txt "Color Grade calibration"). Graded vs
    ungraded Lightroom exports showed that a Color Grading range is:

        an ADDITIVE push, in display-referred ProPhoto RGB encoded with gamma 2.2,
        along the vector  HSV(hue, 1, 1) - mean,  minus a small uniform offset,
        scaled LINEARLY by saturation and by a smooth window over the pixel's L*.

    Three findings pinned it down. In that space the red, green and blue midtone pushes
    have the SAME amplitude (0.197 / 0.201 / 0.200 at the peak), so strength is
    hue-independent there and nowhere else tried (sRGB, Rec.2020, linear). HSV hue in
    ProPhoto primaries lands on Lightroom's Lab hue angles (H0 -> 13 deg, H40 -> 61,
    H120 -> 153; the blue hues read a little off only because the sRGB export clips them).
    And the
    lightness side effects -- blue darkens, green brightens -- are exactly what a
    MEAN-subtracted (not luma-subtracted) vector predicts.

    What it replaced: a zero-luma push added in SCENE-LINEAR light. Linear-additive floods
    the shadows (a 0.39 push on a pixel at 0.02), and keying the windows on scene values
    ahead of the view transform put the highlight window above almost everything, so the
    old grade was 2-30x Lightroom's strength in the shadows and nearly absent in the
    highlights.

      gradePushDir      hue -> the unit-less push direction (HSV - mean)
      gradeWindowAmp    a range's push amplitude at full saturation for a pixel's L*,
                        with the panel's Blending / Balance applied
      gradeLightOffset  a range's lightness offset ratio at the given Blending / Balance
      lstarFromY        CIE L* of a display-linear luminance
*/
namespace ColorGrade {

/* Linear Rec.2020 -> linear ProPhoto RGB (ROMM primaries, Bradford-adapted D50 -> D65 so
   neutral stays neutral; rows sum to 1), and its inverse. Derived from the published
   primaries in the 2026-10-12 calibration; the push lives in this space. */
constexpr float kRec2020ToProPhoto[9] = {
     0.8351932f, 0.0488653f, 0.1159414f,
     0.0540346f, 0.9289099f, 0.0170555f,
    -0.0023420f, 0.0363322f, 0.9660099f };
constexpr float kProPhotoToRec2020[9] = {
     1.2006506f, -0.0575637f, -0.1430869f,
    -0.0699434f,  1.0806280f, -0.0106845f,
     0.0055415f, -0.0407826f,  1.0352410f };

/* The encoding the push is additive in (ProPhoto channels, display-linear -> e). */
constexpr float kPushGamma = 2.2f;

/* Measured windows: push amplitude at saturation 100 (in gamma-2.2 ProPhoto units, along
   the HSV - mean vector) at L* = 0, 5, ..., 100. Read from the medians of 5-L* bins and
   lightly smoothed. The shadow window below L* 20 was then raised ~1.5x to match the
   rendered result (the in-space median there was depressed by clipping at black).
   Above L* 90 the reference had too few unclipped pixels, so those entries are
   extrapolated (the highlight window is held up towards white, the midtone one tapers
   to 0). GLOBAL is not a flat amplitude, as it first looked: it is a broad window,
   ~0.20 across L* 45-80 and falling to ~0.04 at black (measured at S50, doubled). */
constexpr int   kWindowN = 21;
constexpr float kWindow[4][kWindowN] = {
    /* shadows    */ {0.085f, 0.102f, 0.110f, 0.116f, 0.120f, 0.126f, 0.132f, 0.133f,
                      0.122f, 0.100f, 0.076f, 0.060f, 0.051f, 0.044f, 0.035f, 0.026f,
                      0.019f, 0.012f, 0.007f, 0.003f, 0.000f},
    /* midtones   */ {0.000f, 0.006f, 0.014f, 0.024f, 0.032f, 0.044f, 0.063f, 0.089f,
                      0.121f, 0.155f, 0.184f, 0.198f, 0.193f, 0.173f, 0.142f, 0.107f,
                      0.074f, 0.046f, 0.025f, 0.010f, 0.000f},
    /* highlights */ {0.000f, 0.002f, 0.006f, 0.009f, 0.011f, 0.013f, 0.015f, 0.017f,
                      0.023f, 0.032f, 0.043f, 0.056f, 0.073f, 0.099f, 0.132f, 0.163f,
                      0.182f, 0.187f, 0.180f, 0.165f, 0.150f},
    /* global     */ {0.040f, 0.055f, 0.070f, 0.084f, 0.096f, 0.112f, 0.132f, 0.150f,
                      0.166f, 0.180f, 0.192f, 0.200f, 0.205f, 0.205f, 0.198f, 0.188f,
                      0.172f, 0.150f, 0.128f, 0.105f, 0.085f} };

/* Where each windowed range peaks (L*); Blending's warp is about these. */
constexpr float kWindowCentre[3] = {33.0f, 56.0f, 85.0f};

/* The uniform offset that rides with each range's push, as a fraction of its amplitude
   (push = amp * sat * (dir - offset)). Positive darkens: midtones +0.156, highlights
   +0.145, Global +0.16, shadows -0.18 (they lift slightly). At non-default Blending /
   Balance Lightroom darkens much more: the measured midtone ratio is ~0.60 at both
   Blending 100 and Balance +100, i.e. +kOffsetGrowth at full deflection (see
   gradeLightOffset). */
constexpr float kLightOffset[4] = {-0.18f, 0.156f, 0.145f, 0.16f};
constexpr float kOffsetGrowth   = 0.44f;

/* Luminance (-100..100): a UNIFORM additive offset on all three encoded channels, shaped
   by the range's own window -- +/-100 moves the midtone peak by 0.43 x its saturation-100
   push (measured +0.086 / -0.084 at L* 55, ~+/-8.6 L*), and its profile over L* tracks the
   midtone window to within a few percent. */
constexpr float kLumScale = 0.43f;

/* Blending, measured at 100 on the midtone window (asymmetric): its centre moves up
   kBlendShift L*, distance below the centre is scaled by 1 - kBlendLo * spread and above
   it by 1 - kBlendHi * spread (scale < 1 = wider), and its amplitude by
   1 - kBlendAmp * spread. spread = (blending - 50) / 50, so 50 is the identity; below 50
   the same parameters run the other way (narrower), which is extrapolated. */
constexpr float kBlendShift = 5.0f;
constexpr float kBlendLo    = 0.20f;
constexpr float kBlendHi    = 0.06f;
constexpr float kBlendAmp   = 0.04f;

/* Balance, measured at +100 on the midtone window: the L* axis is read through a POWER,
   L' = 100 (L/100)^g with g = kBalanceGamma^(balance/100), so +100 has a pixel at L* 12
   read the window at L* 56 -- the midtones are squeezed down into the shadows -- and the
   squeezed window's amplitude drops to kBalanceAmp^|balance/100| (0.285 at full). It is a
   much bigger move than a shift: a fitted shift was 3x too small. Negative balance is the
   mirror (g > 1). Which range is "squeezed" (and so loses amplitude) is inferred, not
   measured: midtones always, shadows at positive balance, highlights at negative. */
constexpr float kBalanceGamma = 0.265f;
constexpr float kBalanceAmp   = 0.285f;

/* HSV(hueDeg, 1, 1) minus its mean: the direction of a range's push. Zero-sum, so it
   moves chroma; the lightness side effects come from the per-range offset. */
inline void gradePushDir(float hueDeg, float out[3])
{
    const float h  = hueDeg / 60.0f;
    const float ff = h - std::floor(h);
    const int   seg = (static_cast<int>(std::floor(h)) % 6 + 6) % 6;
    float r = 0, g = 0, b = 0;
    switch (seg) {
    case 0:  r = 1;      g = ff;     b = 0;      break;
    case 1:  r = 1 - ff; g = 1;      b = 0;      break;
    case 2:  r = 0;      g = 1;      b = ff;     break;
    case 3:  r = 0;      g = 1 - ff; b = 1;      break;
    case 4:  r = ff;     g = 0;      b = 1;      break;
    default: r = 1;      g = 0;      b = 1 - ff; break;
    }
    const float m = (r + g + b) / 3.0f;
    out[0] = r - m;
    out[1] = g - m;
    out[2] = b - m;
}

/* CIE L* (0..100) of a display-linear luminance Y (0..1, white = 1). */
inline float lstarFromY(float Y)
{
    if (Y <= 0.0f) return 0.0f;
    if (Y >= 1.0f) return 100.0f;
    return Y > 0.008856f ? 116.0f * std::cbrt(Y) - 16.0f : 903.3f * Y;
}

/* Linear interpolation into one measured window at L* (clamped to 0..100). */
inline float windowAt(int range, float L)
{
    if (L <= 0.0f) return kWindow[range][0];
    if (L >= 100.0f) return kWindow[range][kWindowN - 1];
    const float f = L / 5.0f;
    const int   i = static_cast<int>(f);
    const float t = f - i;
    return kWindow[range][i] + (kWindow[range][i + 1] - kWindow[range][i]) * t;
}

/* A range's push amplitude at saturation 1 for a pixel at L*, with the panel-wide
   Blending (0..100) and Balance (-100..100) applied. range 0..2 = shadows / midtones /
   highlights, 3 = Global (not tone-selective, so neither slider moves it). */
inline float gradeWindowAmp(int range, float L, float blending, float balance)
{
    if (range == 3) return windowAt(3, L);
    const float bal = balance / 100.0f;                       // -1..1
    float amp = 1.0f;
    if (bal != 0.0f) {
        const float x = L <= 0.0f ? 0.0f : (L >= 100.0f ? 1.0f : L / 100.0f);
        L = 100.0f * std::pow(x, std::pow(kBalanceGamma, bal));
        const bool squeezed = range == 1 || (range == 0 && bal > 0.0f) ||
                              (range == 2 && bal < 0.0f);
        if (squeezed) amp = std::pow(kBalanceAmp, std::fabs(bal));
    }
    const float spread = (blending - 50.0f) / 50.0f;          // -1..1, 0 at default
    if (spread != 0.0f) {
        const float c = kWindowCentre[range] + kBlendShift * spread;
        const float k = (L < c) ? 1.0f - kBlendLo * spread : 1.0f - kBlendHi * spread;
        L = kWindowCentre[range] + (L - c) * k;
        amp *= 1.0f - kBlendAmp * spread;
    }
    return amp * windowAt(range, L);
}

/* A range's lightness offset ratio (see kLightOffset) at the given Blending / Balance.
   Grows by kOffsetGrowth with Blending above 50 and with |Balance|; Blending below 50 is
   left at the measured default (unmeasured, and lightening there would be a guess). The
   growth is measured on midtones only and applied to every windowed range. */
inline float gradeLightOffset(int range, float blending, float balance)
{
    if (range == 3) return kLightOffset[3];
    const float spread = (blending - 50.0f) / 50.0f;
    return kLightOffset[range] + kOffsetGrowth * ((spread > 0.0f ? spread : 0.0f)
                                                  + std::fabs(balance / 100.0f));
}

} // namespace ColorGrade

#endif // COLORGRADE_H
