#ifndef SHARPEN_H
#define SHARPEN_H

#include <algorithm>
#include <cmath>

/*
    Pure capture-sharpening math, shared by the Develop spatial op (Develop::Sharpen) and
    its unit test. No Qt, no OpenCV, so it stays header-only and testable in isolation --
    the same split as colorgrade.h / tonecurve.h / calibrate.h. Develop::Sharpen owns the
    OpenCV Gaussian and the parallelFor; everything scalar lives here.

    THE MODEL. An unsharp mask on perceptual luminance: a Gaussian base isolates a
    high-frequency band (hp = yp - base) which the slider adds back. Two controls shape
    what gets added:

      Detail   soft-thresholds the smallest amplitudes. Low treats them as noise and
               suppresses them; high passes everything (more bite, more halo).
      Masking  gates by local edge strength, so flat areas (sky, skin) are left alone.
               This is what makes sharpening usable rather than a noise amplifier.

    WHY THIS OP IS SCALE-AWARE AND THE OTHERS ARE NOT. Every other Develop spatial op
    scales its radius to max(w,h) so the proxy preview and the full-res settle render
    shape the same relative band. Acutance is a property of the PIXEL GRID, so sharpening
    cannot: a 1 px radius is 1 px whatever the image is. effectiveSigma() therefore takes
    the render scale (WorkingImage::renderScale) and shrinks the radius for a proxy, so
    the proxy shows a scale-honest preview of a result that is only true at 1:1.
*/
namespace Sharpen {

/* The absolute (full-resolution) radius in pixels, as the slider stores it. */
constexpr float kMinRadius = 0.5f;
constexpr float kMaxRadius = 3.0f;

/* Amplitude, in perceptual-luma units, below which detail is treated as noise when
   Detail is 0. The knee scales with the render scale alongside the radius: a proxy's
   high-frequency band carries less amplitude, so a fixed threshold over-suppresses. */
constexpr float kDetailKnee = 0.06f;

/* Edge-strength (gradient magnitude, perceptual-luma units per pixel) that counts as a
   full edge -- the knee the gate rolls off below. It is a function of the Masking slider
   rather than a single constant, because a LINEAR ramp ran out of travel: at the top of
   the slider a knee of 0.10/px still passed ordinary texture (~0.05/px) at half strength,
   so "maximum masking" could not actually quiet a noisy sky or skin.

   knee(m) = m * (kMaskKnee + (kMaskKneeMax - kMaskKnee) * m^2)

   -- linear at the bottom, cubic at the top. The low end keeps the feel it had (m = 0.25
   moves the knee 0.031 against 0.025 before), while the top end reaches 5x as far, so the
   last third of the slider is where the strong protection now lives. Values are per
   FULL-RESOLUTION pixel; edgeGate scales the result by the render scale.

   NOTE this re-calibrates what a STORED sharpenMasking means (2026-08-21): the key and its
   0..1 range are unchanged -- so nothing needs migrating -- but a sidecar written before
   this protects more than it used to above the middle of the slider. Sharpening shipped
   in the previous commit, so there is nothing older than that to be wrong about. */
constexpr float kMaskKnee    = 0.10f;    // linear term: the low end's feel
constexpr float kMaskKneeMax = 0.50f;    // the knee at Masking 1

inline float maskKnee(float masking)
{
    const float m = std::clamp(masking, 0.0f, 1.0f);
    return m * (kMaskKnee + (kMaskKneeMax - kMaskKnee) * m * m);
}

/* A proxy Gaussian narrower than this is pointless -- it degenerates to a no-op kernel
   and the slider would look dead during a drag. */
constexpr float kMinSigma = 0.4f;

/* Blur sigma for THIS render. radius is the full-resolution pixel radius; renderScale is
   this render's long edge over the full-res long edge (1.0 full-res, ~0.3 a typical
   proxy). Clamped so a small proxy still shows a visible, if approximate, effect. */
inline float effectiveSigma(float radius, float renderScale)
{
    const float r = std::clamp(radius, kMinRadius, kMaxRadius);
    const float s = (renderScale > 0.0f) ? renderScale : 1.0f;
    return std::max(kMinSigma, r * s);
}

/* Smooth 0..1 ramp (Hermite), the same shape the mask falloff code uses. */
inline float smoothstep(float edge0, float edge1, float x)
{
    if (edge1 <= edge0) return (x < edge0) ? 0.0f : 1.0f;
    const float t = std::clamp((x - edge0) / (edge1 - edge0), 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

/* Shape one high-pass sample by the Detail slider.

   detail == 1 returns hp untouched (a plain unsharp mask). Below that, amplitudes under
   the knee are progressively suppressed by a soft (squared-ramp) threshold rather than a
   hard cut -- a hard threshold quantises smooth gradients into visible plateaux. Sign is
   always preserved. */
inline float shapeDetail(float hp, float detail, float renderScale)
{
    const float d = std::clamp(detail, 0.0f, 1.0f);
    if (d >= 1.0f) return hp;
    const float s = (renderScale > 0.0f) ? renderScale : 1.0f;
    const float knee = kDetailKnee * (1.0f - d) * s;
    if (knee <= 0.0f) return hp;
    const float mag = std::fabs(hp);
    const float t = smoothstep(0.0f, knee, mag);
    return hp * t;
}

/* Edge gate from a local gradient magnitude.

   masking == 0 returns 1 everywhere (sharpen everything). As masking rises the gate
   demands more edge strength -- along maskKnee's ramp, so the top of the slider has real
   travel -- and flat regions fall to zero while only real edges keep a full gate. Like
   the detail knee, the threshold scales with the render so a proxy gates comparably. */
inline float edgeGate(float gradMag, float masking, float renderScale)
{
    const float m = std::clamp(masking, 0.0f, 1.0f);
    if (m <= 0.0f) return 1.0f;
    const float s = (renderScale > 0.0f) ? renderScale : 1.0f;
    const float knee = maskKnee(m) * s;
    if (knee <= 0.0f) return 1.0f;
    /* Roll in from zero to the knee, then blend the gate toward "open" by (1 - masking)
       so intermediate settings soften rather than switch. */
    const float gate = smoothstep(0.0f, knee, gradMag);
    return gate + (1.0f - m) * (1.0f - gate);
}

/* The full per-pixel step, in the perceptual domain.

   yp       this pixel's perceptual luma (pow(Ylin / white, 1/gamma))
   base     the Gaussian-blurred perceptual luma at this pixel
   gradMag  local gradient magnitude of yp, for the edge gate
   returns  the sharpened perceptual luma, clamped at 0

   Callers convert back to scene-linear and apply the result as a RATIO on RGB, which is
   what keeps hue and chroma untouched (the house idiom, see Develop::Texture). */
inline float applyPixel(float yp, float base, float gradMag,
                        float amount, float detail, float masking, float renderScale)
{
    if (amount <= 0.0f) return yp;
    const float hp = shapeDetail(yp - base, detail, renderScale);
    const float gate = edgeGate(gradMag, masking, renderScale);
    const float s = yp + amount * gate * hp;
    return (s > 0.0f) ? s : 0.0f;
}

} // namespace Sharpen

#endif // SHARPEN_H
