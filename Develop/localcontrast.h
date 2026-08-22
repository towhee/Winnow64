#ifndef LOCALCONTRAST_H
#define LOCALCONTRAST_H

#include <algorithm>
#include <cmath>

/*
    Shared local-contrast math for the Develop band ops, plus its unit test. No Qt, no
    OpenCV, so it stays header-only and testable in isolation -- the same split as
    colorgrade.h / tonecurve.h / sharpen.h. develop.cpp owns the OpenCV blurs and the
    parallelFor; every scalar decision lives here.

    ONE OPERATION AT FOUR RADII. Texture, Clarity and Dehaze's first stage are the same
    thing -- split a frequency band off a blurred base and scale it -- differing only in
    radius and weighting. (Sharpen is the same shape again at ~1px, but it carries its own
    scale-awareness and edge gating, so it keeps its own header.)

        Sharpen  ~1 px absolute      edge-gated          acutance
        Texture  0.0015 x long edge  ungated             fine detail contrast
        Clarity  0.007  x long edge  midtone + halo      midtone punch
        Dehaze   0.02   x long edge  ungated (+ pivot)   atmospheric veiling

    They stack rather than compete because the radii are an order of magnitude apart.

    THE BAND EXPRESSION. Every one of them reduces to

        s = base + factor * (yp - base)

    on PERCEPTUAL luminance, folded back as a ratio on RGB so chroma is untouched. Texture
    wrote it in that form already; Dehaze wrote the algebraically identical
    `yp + k*(yp - base)`. Both now route through applyBand, so there is one definition of
    what a band op does. See tests/unit/tst_localcontrast.cpp -- it pins the pre-
    extraction render of Texture and Dehaze, because these ops shape saved recipes.
*/
namespace LocalContrast {

/* Scale a band about its base. factor == 1 is a no-op, > 1 amplifies (crisper), 0..1
   attenuates toward the base (smoother), 0 collapses onto the base exactly. */
inline float applyBand(float yp, float base, float factor)
{
    return base + factor * (yp - base);
}

/* Slider amount (-1..1) -> band factor. Clamped at 0 so a full negative lands ON the base
   (fully smoothed) and never inverts the band into a negative-detail artefact. */
inline float bandFactor(float amount, float gain)
{
    const float f = 1.0f + amount * gain;
    return (f > 0.0f) ? f : 0.0f;
}

/* Midtone weight for a perceptual luma s in 0..1: peaks at mid-grey and falls to 0 at
   black and white. The sqrt broadens the lobe so shadows and highlights still carry some
   effect rather than being cut off abruptly.

   This is Develop::Grain's weight, lifted verbatim -- the same expression, so Grain's
   output is unchanged by adopting it. Clarity uses it for the reason Grain does: a wide
   band applied flat crushes blacks and flattens highlights, and weighting toward the
   midtones is what separates Clarity from "Texture with a bigger radius". */
inline float midtoneWeight(float s)
{
    const float t = 4.0f * s * (1.0f - s);
    return std::sqrt((t > 0.0f) ? t : 0.0f);
}

/* Halo guard. A mid-radius band produces halos exactly where the high-pass excursion is
   large, i.e. at strong edges -- the bright rim around a tree against sky. Roll the
   effect off as |detail| approaches the knee, so flat and gently-modulated areas get the
   full effect and hard edges get progressively less.

   Returns 1 well below the knee and tends to 0 well above it. knee <= 0 disables it. */
inline float haloGuard(float detail, float knee)
{
    if (knee <= 0.0f) return 1.0f;
    const float m = std::fabs(detail) / knee;
    return 1.0f / (1.0f + m * m);          // smooth, always > 0, no hard cut
}

} // namespace LocalContrast

#endif // LOCALCONTRAST_H
