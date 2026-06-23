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

    SCAFFOLD: placeholder white balance + matrix maths so the pipeline runs end to end.
    Accurate per-camera matrices land in a later phase.
*/
class RawColor
{
public:
    bool ToWorking(const RawImage &raw,
                   const std::vector<float> &rgb,
                   WorkingImage &out);
};

#endif // RAWCOLOR_H
