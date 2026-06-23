#ifndef RAWCOLOR_H
#define RAWCOLOR_H

#include <vector>
#include "ImageFormats/Raw/rawimage.h"
#include "Develop/workingimage.h"

/*
    Converts demosaiced camera-RGB (interleaved floats, 0..1) into a scene-linear
    WorkingImage: white balance + camera-RGB -> linear sRGB colour matrix. It deliberately
    stops at LINEAR (no tone/gamma) -- the Develop stage operates on this buffer and
    OutputTransform applies the display gamma/ICC. This is the convergence point where the
    RAW path joins the same WorkingImage that non-raw decodes produce via InputTransform.

    Colour follows dcraw/libraw: from the model's XYZ->camera matrix (raw.xyzToCam) it builds
    the camera->linear-sRGB transform and a neutral white balance, applies the white balance
    (as-shot raw.camMul when present, else the matrix-derived neutral) and the matrix, and
    clamps to [0,1]. An identity xyzToCam (unknown camera) degrades to an sRGB-assumed
    pass-through so the image still renders.
*/
class RawColor
{
public:
    bool ToWorking(const RawImage &raw,
                   const std::vector<float> &rgb,
                   WorkingImage &out);
};

#endif // RAWCOLOR_H
