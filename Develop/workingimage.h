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
struct WorkingImage {
    std::vector<float> rgb;     // interleaved R,G,B, scene-linear
    int width = 0;
    int height = 0;

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
