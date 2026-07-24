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

#endif // WORKINGIMAGE_H
