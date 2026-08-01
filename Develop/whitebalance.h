#ifndef WHITEBALANCE_H
#define WHITEBALANCE_H

#include <QString>
#include "Develop/workingimage.h"

/*
    Absolute white balance: a correlated colour temperature in Kelvin plus a
    green/magenta tint, resolved against the source's own colour characterisation
    (WorkingImage::cam) rather than applied as a blind per-channel nudge.

    THE MODEL

    A white balance answers one question: "what illuminant was the scene lit by?"
    Given that illuminant we can say what a NEUTRAL surface under it looks like once
    it has been through the decode pipeline, and the correction is whatever gains
    make that colour grey again.

    For a target (kelvin, tint) the forward chain is:

        (K, tint) -> xy chromaticity on the Planckian / daylight locus, tint
                     displacing it perpendicular to that locus  (locusXY)
                  -> XYZ
                  -> camera RGB              (cam.xyzToCam)
                  -> the as-shot WB the decode already baked in  (cam.asShotMul)
                  -> linear sRGB             (cam.camToSrgb)
                  = s, the rendered colour of that illuminant's neutral

    gains() then returns (s.g/s.r, 1, s.g/s.b): the per-channel scene-linear gains
    that make s neutral. Green is pinned to 1 so exposure does not drift with
    temperature. Develop folds these into its fused per-channel multiply, so an
    absolute white balance costs exactly what the old relative one did.

    THE INVERSE

    solve() goes the other way: given a colour in linear sRGB, find the (K, tint)
    whose illuminant renders to that chromaticity. Two nested bisections -- K on the
    red/blue ratio (monotonic: hotter is bluer), then tint on the green residual --
    repeated a few times since the two axes are weakly coupled. This ONE function
    serves three callers:

        as-shot estimate  solve(cam, {1,1,1})   -- the as-shot render is neutral by
                                                   construction, so the illuminant
                                                   that renders neutral IS the
                                                   shooting illuminant
        the dropper       solve(cam, sampled)   -- "make THIS pixel neutral"
        Auto              solve(cam, estimate)  -- on a robust scene average

    Presets need no solve at all: they name a Kelvin directly.

    RELATIVE TO AS-SHOT

    EditParams::temp == 0 means "as shot", not 0 K -- so a freshly opened image is
    identity and never marks itself edited. relativeGains() divides by the gains at
    (cam.asShotK, cam.asShotTint), which guarantees EXACT identity at as-shot even
    though asShotK came from a bisection fit and carries a little residual error.
*/
namespace WhiteBalance {

/* Slider / storage limits, matching Lightroom's. */
constexpr float kMinKelvin = 2000.0f;
constexpr float kMaxKelvin = 50000.0f;
constexpr float kMinTint   = -150.0f;
constexpr float kMaxTint   = 150.0f;

/* The named white balances offered in the Basic panel's WB dropdown. AsShot leaves
   the image at its decoded balance; Auto is solved from image content; Custom is
   whatever the user dialled or dropped. The rest are fixed illuminants. */
enum class Preset {
    AsShot, Auto, Daylight, Cloudy, Shade, Tungsten, Fluorescent, Flash, Custom
};

/* Preset <-> the strings used in the dropdown and the XMP sidecar. */
QString    presetName(Preset p);
Preset     presetFromName(const QString &name);

/* The fixed (kelvin, tint) a named preset stands for. False for AsShot / Auto /
   Custom, which are not fixed illuminants and must be resolved by the caller. */
bool       presetValues(Preset p, float &kelvin, float &tint);

/* Per-channel scene-linear gains for an absolute (kelvin, tint), normalised so
   green == 1. Falls back to no-op gains when cam is not valid. */
void       gains(const CameraColor &cam, float kelvin, float tint, float out[3]);

/* Gains RELATIVE to the as-shot rendering: what Develop actually applies, so that
   the as-shot temperature is an exact no-op. This is the entry point for rendering. */
void       relativeGains(const CameraColor &cam, float kelvin, float tint, float out[3]);

/* Resolve a stored EditParams temp/tint pair (0 = as shot) into the absolute
   Kelvin/tint the UI should display and the render should use. */
void       resolve(const CameraColor &cam, float storedTemp, float storedTint,
                   float &kelvin, float &tint);

/* Inverse: the (kelvin, tint) whose illuminant renders to the chromaticity of
   linear-sRGB colour (r,g,b). Returns false if the colour is degenerate (a
   non-positive channel -- clipped or black). */
bool       solve(const CameraColor &cam, float r, float g, float b,
                 float &kelvin, float &tint);

/*
    SKIN TONE sampling: correct the white balance from a sampled patch of SKIN, which
    is not neutral. Instead of driving the sample to grey, it is driven back onto the
    skin HUE LINE while its saturation and lightness are left alone.

    Why the hue line and not a fixed reference skin colour: measured skin of very
    different depths agrees almost exactly on HUE and differs mainly in SATURATION.
    The ColorChecker light- and dark-skin patches sit 0.3 degrees apart in the
    (warmth, green) plane below while their chroma magnitudes differ by 23%. Hue is
    therefore the part a white-balance error corrupts, and the part safe to correct;
    forcing a fixed chromaticity would desaturate deep skin by roughly a quarter and
    push olive skin off its own colour.

    Geometry: the same (warmth, green) log axes solve() already bisects on, in which
    NEUTRAL IS THE ORIGIN -- so a hue is a direction from the origin and saturation is
    the distance along it. The sampled chroma is projected onto the skin direction
    (keeping the along-line component, zeroing the perpendicular error); that
    projection is the target, and the correction is the one that carries the sample to
    it. Exact in one shot: the corrected sample lands ON the line by construction, so
    no iteration is needed.
*/
constexpr float kSkinHueDeg = -8.5f;      // skin direction in the (warmth, green) plane

/*
    How far off the line a sample may be and still be treated as skin.

    Sized from measurements, and deliberately GENEROUS, because the whole point is to
    fix casts: skin under a strong tungsten cast is already 14 degrees off, and a
    tighter limit would refuse exactly the images the tool exists for. It rejects
    foliage (67 degrees off) and blue sky (167). It CANNOT reject a red or orange
    object -- a red shirt is only 18 degrees off, because skin hue genuinely is
    orange. This guard catches gross mis-clicks, not subtle ones.
*/
constexpr float kSkinHueToleranceDeg = 60.0f;
constexpr float kSkinMinChroma       = 0.05f;   // below this there is no hue to read

enum class SkinPick {
    Ok,
    TooNeutral,     // no colour to read (grey/blown) -- use the neutral dropper
    NotSkin,        // hue too far off the line (sky, foliage, a cool cast beyond rescue)
    Degenerate      // black / clipped channel, or no camera characterisation
};

/* Solve for the (kelvin, tint) that puts a sampled SKIN colour back on the skin hue
   line. hueErrorDeg, when non-null, receives how far off the line the sample was
   BEFORE correction (filled even when the result is NotSkin, so callers can report
   it). kelvin/tint are only written on Ok. */
SkinPick   solveSkin(const CameraColor &cam, float r, float g, float b,
                     float &kelvin, float &tint, float *hueErrorDeg = nullptr);

/* Fill cam.asShotK / cam.asShotTint by solving for neutral. Called once per decode. */
void       resolveAsShot(CameraColor &cam);

/* Auto white balance: a robust neutral estimate over the image (a bright-but-unclipped
   average, so a large saturated subject cannot drag the balance), then solved to
   (kelvin, tint). False when the image gives nothing usable. */
bool       autoWhiteBalance(const WorkingImage &img, float &kelvin, float &tint);

} // namespace WhiteBalance

#endif // WHITEBALANCE_H
