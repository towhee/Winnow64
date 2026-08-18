#ifndef WORKINGIMAGE_H
#define WORKINGIMAGE_H

#include <vector>

/*
    The shared, high-precision representation that both decode paths converge on and the
    Develop stage operates on:

        RAW:      ... -> Demosaic -> camera-RGB -> working space   (RawColor stops LINEAR)
        NON-RAW:  QImage decode -> un-gamma to linear -> working space
        consumer: WorkingImage -> Develop(EditParams) -> OutputTransform -> QImage

    Pixels are interleaved float RGB, scene-linear (no display gamma). Adjustments such as
    exposure, tone recovery and dehaze need this linear headroom; the display gamma / ICC
    transform is applied last, by OutputTransform.

    Working colour space: scaffold uses linear sRGB primaries / D65. A colour-space tag is
    left for later so wider gamuts (e.g. linear ProPhoto) can be carried without changing
    this struct's shape.
*/
/*
    The colour characterisation of the source, carried with the pixels so Develop can
    compute an ABSOLUTE white balance (a Kelvin temperature + green/magenta tint)
    rather than a blind relative nudge. See Develop/whitebalance.h.

    Both decode paths fill it:

      RAW      xyzToCam is the model's XYZ->camera matrix, asShotMul the as-shot
               multipliers RawColor applied, camToSrgb the camera->linear-sRGB
               matrix it applied after.
      NON-RAW  a synthetic "camera" whose response IS linear sRGB: asShotMul = 1,
               camToSrgb = identity, xyzToCam = XYZ->linear sRGB. A display-referred
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
    float camToSrgb[3][3] = {{1,0,0}, {0,1,0}, {0,0,1}};
    float asShotK    = 0.0f;    // 0 = not solved
    float asShotTint = 0.0f;
};

struct WorkingImage {
    std::vector<float> rgb;     // interleaved R,G,B, scene-linear
    int width = 0;
    int height = 0;

    CameraColor cam;            // colour characterisation for absolute white balance

    float white = 1.0f;         // linear value that maps to display white (1.0)

    /* True for sensor data (RAW): scene-referred linear with highlight headroom (values may
       exceed white), so OutputTransform applies a baseline tone curve (exposure + filmic
       shoulder) to render a pleasing default. False for display-referred input (a JPEG
       un-gamma'd by InputTransform): it already carries the camera's tone curve, so
       OutputTransform only re-applies the display gamma. */
    bool sceneReferred = false;

    bool isValid() const {
        return width > 0 && height > 0 &&
               rgb.size() == static_cast<size_t>(width) * static_cast<size_t>(height) * 3;
    }
};

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
    dst.cam           = src.cam;
    dst.white         = src.white;
    dst.sceneReferred = src.sceneReferred;
    dst.rgb.assign(src.rgb.begin(), src.rgb.end());
}

#endif // WORKINGIMAGE_H
