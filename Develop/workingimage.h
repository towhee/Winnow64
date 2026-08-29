#ifndef WORKINGIMAGE_H
#define WORKINGIMAGE_H

#include <vector>
#include "Develop/colorspace.h"

/*
    The shared, high-precision representation that both decode paths converge on and the
    Develop stage operates on:

        RAW:      ... -> Demosaic -> camera-RGB           (RawColor stops CAMERA-NATIVE)
        NON-RAW:  QImage decode -> un-gamma to linear -> working space
        consumer: WorkingImage -> Develop(EditParams) -> OutputTransform -> QImage

    Pixels are interleaved float RGB, linear (no display gamma). Adjustments such as
    exposure, tone recovery and dehaze need this linear headroom; the display gamma / ICC
    transform is applied last, by OutputTransform.

    COLOUR SPACE. `space` says which primaries the pixels are in RIGHT NOW; it is not an
    assumption any more. A raw decode hands over CameraNative (sensor primaries, described
    by `cam`) and Develop's first stage converts to ColorSpaceMath::kWorking. A non-raw
    decode converts on the way in and arrives already in the working space.

    The working space is linear Rec.2020 / D65 -- wide enough that a saturated sensor
    colour survives to the view transform instead of being clipped at decode, and D65 so
    no chromatic adaptation hides between the camera matrix and the white-balance solver.
    See Develop/colorspace.h for why not ACEScg.
*/
/*
    The colour characterisation of the source, carried with the pixels so Develop can
    compute an ABSOLUTE white balance (a Kelvin temperature + green/magenta tint)
    rather than a blind relative nudge. See Develop/whitebalance.h.

    Both decode paths fill it:

      RAW      xyzToCam is the model's XYZ->camera matrix, asShotMul the as-shot
               multipliers RawColor applied, camToWorking the camera->linear-sRGB
               matrix it applied after.
      NON-RAW  a synthetic "camera" whose response IS linear sRGB: asShotMul = 1,
               camToWorking = identity, xyzToCam = XYZ->linear sRGB. A display-referred
               file is already balanced to its own white point, which for sRGB is
               D65 -- so the same maths resolves its as-shot temperature to ~6500 K
               and the sliders behave uniformly.

    asShotK / asShotTint are the temperature and tint the as-shot rendering
    corresponds to, solved once at decode (WhiteBalance::solve). They are what the
    Temp / Tint sliders show before the user touches them, and the reference the
    stored EditParams values are relative to -- EditParams::temp == 0 means "as
    shot", NOT 0 K.
*/
struct CameraColor {
    bool  valid = false;
    float asShotMul[3]  = {1.0f, 1.0f, 1.0f};
    float xyzToCam[3][3] = {{1,0,0}, {0,1,0}, {0,0,1}};
    float camToWorking[3][3] = {{1,0,0}, {0,1,0}, {0,0,1}};
    float asShotK    = 0.0f;    // 0 = not solved
    float asShotTint = 0.0f;
};

struct WorkingImage {
    std::vector<float> rgb;     // interleaved R,G,B, scene-linear
    int width = 0;
    int height = 0;

    CameraColor cam;            // colour characterisation for absolute white balance

    float white = 1.0f;         // linear value that maps to display white (1.0)

    /* Which primaries `rgb` is in right now. CameraNative means the pixels are still in
       the sensor's own space and Develop's input-profile stage has not run yet -- the
       matrix that converts them is not here but in `cam`, because it is per-model. Every
       other value means the pixels are in that space's primaries, D65.

       Ops that ask for luma weights must pass this through
       ColorSpaceMath::lumaWeights(space): luma belongs to a colour space, and an op
       assuming Rec.709 while running on Rec.2020 data quietly stops preserving luma. */
    ColorSpaceMath::ColorSpace space = ColorSpaceMath::ColorSpace::LinearSRGB;

    /* True for sensor data (RAW): scene-referred linear with highlight headroom (values
       may exceed white), so the output stage applies a view transform (tone mapping) to
       render a pleasing default. False for display-referred input (a JPEG un-gamma'd by
       InputTransform): it already carries the camera's tone curve, so the output stage
       only re-applies the display gamma.

       Orthogonal to `space`: together they are the full colour state -- WHICH primaries
       (space) and WHICH referring (this). Both needed; neither implies the other. */
    bool sceneReferred = false;

    /* This render's long edge as a fraction of the FULL-RESOLUTION long edge: 1.0 for a
       full-res (settle / export) render, ~0.3 for a typical screen proxy. Every Develop
       spatial op but one scales its radius to max(w,h) and so is scale-invariant by
       construction; Sharpen has an ABSOLUTE pixel radius (acutance belongs to the pixel
       grid) and is the only op that must know how much the image has been downscaled.
       Deliberately NOT an EditParams field: EditParams is the sidecar format AND the
       per-scope render-cache key, so a render detail there would be persisted to every
       sidecar and make the proxy and settle renders miss each other's cache entries. */
    float renderScale = 1.0f;

    bool isValid() const {
        return width > 0 && height > 0 &&
               rgb.size() == static_cast<size_t>(width) * static_cast<size_t>(height) * 3;
    }
};

/*
    READING PIXELS OUT OF A WorkingImage THAT HAS NOT BEEN THROUGH DEVELOP.

    Since RawColor stops at CameraNative, anything that samples img.rgb BEFORE
    Develop::Apply has run is looking at sensor primaries, not the working space. Two
    kinds of consumer care, and both must go through the helpers below rather than
    assuming Rec.709:

      - luminance    the mask halo guide, the raw-denoise blend: they weight channels by
                     how much each contributes to brightness.
      - colour       the white-balance dropper and Auto WB: they hand a sampled colour to
                     WhiteBalance::solve, whose forward chain ENDS in the working space,
                     so a camera-native sample would be solved against the wrong space
                     and return the wrong temperature.

    Both transforms are LINEAR, which is what makes them cheap to use correctly: an
    average of samples can be converted once, after averaging, instead of per sample.
*/

/* The luma weights appropriate to img's actual space. For camera-native pixels these are
   row 1 of (camToWorking . diag(asShotMul)) -- how much each CAMERA channel contributes
   to working-space luminance -- normalised to sum to 1. */
inline ColorSpaceMath::Luma lumaWeightsFor(const WorkingImage &img)
{
    const ColorSpaceMath::Luma w = ColorSpaceMath::lumaWeights(ColorSpaceMath::kWorking);
    if (img.space != ColorSpaceMath::ColorSpace::CameraNative || !img.cam.valid)
        return ColorSpaceMath::lumaWeights(img.space);

    const float lw[3] = {w.r, w.g, w.b};
    float o[3] = {0.0f, 0.0f, 0.0f};
    for (int j = 0; j < 3; ++j) {
        for (int i = 0; i < 3; ++i) o[j] += lw[i] * img.cam.camToWorking[i][j];
        o[j] *= img.cam.asShotMul[j];
    }
    const float sum = o[0] + o[1] + o[2];
    if (!(sum > 1e-6f)) return w;
    return { o[0] / sum, o[1] / sum, o[2] / sum };
}

/* Luminance of one pixel of img, in img's own space. */
inline float lumaOf(const WorkingImage &img, float r, float g, float b)
{
    const ColorSpaceMath::Luma w = lumaWeightsFor(img);
    return w.r * r + w.g * g + w.b * b;
}

/* Convert ONE colour sampled from img.rgb into the WORKING space -- the same transform
   Develop's stage 0 applies to every pixel. A no-op when img is already converted. */
inline void toWorkingColor(const WorkingImage &img, float &r, float &g, float &b)
{
    if (img.space != ColorSpaceMath::ColorSpace::CameraNative || !img.cam.valid) return;
    const float cr = r * img.cam.asShotMul[0];
    const float cg = g * img.cam.asShotMul[1];
    const float cb = b * img.cam.asShotMul[2];
    r = img.cam.camToWorking[0][0]*cr + img.cam.camToWorking[0][1]*cg
      + img.cam.camToWorking[0][2]*cb;
    g = img.cam.camToWorking[1][0]*cr + img.cam.camToWorking[1][1]*cg
      + img.cam.camToWorking[1][2]*cb;
    b = img.cam.camToWorking[2][0]*cr + img.cam.camToWorking[2][1]*cg
      + img.cam.camToWorking[2][2]*cb;
}

/*
    Copy EVERY non-pixel field of src onto dst -- dimensions excepted, since a caller that
    resamples owns those.

    ONE PLACE, because there are two callers that must not diverge: assignReusing (which
    keeps dst's buffer) and WorkingImageCache::downscaled (which builds a proxy). Both
    used to list the fields by hand, and a field added to WorkingImage but missed in one
    of them does not fail to compile -- it silently ships the DEFAULT.

    That is not hypothetical: `space` was added for the camera-native decode and missed
    here, so a downscaled proxy claimed to be in the working space while still holding
    sensor pixels. Develop then skipped its input-profile stage and the interactive render
    came out strongly green (camera-native green is ~2x the other channels), snapping to
    the right colour on the full-res settle pass that still had the tag. Add new fields
    HERE and both callers get them.
*/
inline void copyMetadata(WorkingImage &dst, const WorkingImage &src)
{
    dst.cam           = src.cam;
    dst.white         = src.white;
    dst.space         = src.space;
    dst.sceneReferred = src.sceneReferred;
    dst.renderScale   = src.renderScale;
}

/*
    Copy src into dst REUSING dst's existing pixel buffer instead of replacing it.

    Plain assignment (dst = src) releases dst's buffer and allocates a new one. At proxy
    resolution that buffer is ~w*h*12 bytes -- 38 MB on a 3.1 MP proxy -- and on macOS a
    free that size goes back to the OS, so the next tick faults every page in again. It
    is not a rounding error: [DevTime] measured 29 ms per free against 2 ms to memcpy the
    same bytes, and an interactive Develop tick was spending 72% of its time there.

    vector::assign keeps the allocation whenever the capacity already fits, which for a
    scratch buffer reused tick after tick at one proxy size is always. Use this anywhere
    a WorkingImage is refilled on a hot path; use plain assignment when the destination
    is genuinely new.
*/
inline void assignReusing(WorkingImage &dst, const WorkingImage &src)
{
    /* Nothing to reuse -- dst has no allocation big enough. Take plain assignment, which
       is exactly what the one-shot callers (the full-res settle render, which allocates
       locally and should give its ~290 MB back) did before this helper existed. Forcing
       those down vector::assign's grow path instead measured 631 ms against 49 ms for
       the same copy at 24 MP, so the fast path here is load-bearing, not a nicety. */
    if (dst.rgb.capacity() < src.rgb.size()) {
        dst = src;
        return;
    }
    dst.width         = src.width;
    dst.height        = src.height;
    copyMetadata(dst, src);
    dst.rgb.assign(src.rgb.begin(), src.rgb.end());
}

#endif // WORKINGIMAGE_H
