#ifndef RAWCOLOR_H
#define RAWCOLOR_H

#include <vector>
#include "ImageFormats/Raw/rawimage.h"
#include "Develop/workingimage.h"

/*
    Wraps demosaiced camera-RGB (interleaved floats, 0..1) into a WorkingImage tagged
    CameraNative, and CHARACTERISES it -- it does not convert it.

    WHAT IT COMPUTES. Colour follows dcraw/libraw: from the model's XYZ->camera matrix
    (raw.xyzToCam) it builds the camera->working transform and a neutral white balance.
    Both go into WorkingImage::cam, along with the as-shot multipliers (raw.camMul when
    the file provides them, else the matrix-derived neutral). An identity xyzToCam
    (unknown camera) degrades to a pass-through so the image still renders, approximately.

    WHAT IT DELIBERATELY DOES NOT DO. It does not apply the white balance, does not apply
    the matrix, and does not clamp. All three used to happen here; they are now Develop's
    stage 0 (Develop::ToWorkingSpace, usually folded into PointCoeffs::preMat).

    WHY. This buffer is what WorkingImageCache holds. Anything baked in here can only be
    undone by decoding the raw again, so a camera profile or white-balance change would
    cost a full re-decode instead of a re-render. Keeping the pixels sensor-native puts
    the cache boundary UPSTREAM of every colour decision. The dropped clamp matters for
    the same reason in the other direction: out-of-gamut sensor colour now survives to
    the output stage instead of being destroyed before the user sees the image.

    The convergence point with the non-raw path (InputTransform, which arrives already in
    the working space) is therefore Develop, not this function.
*/
class RawColor
{
public:
    bool ToCameraNative(const RawImage &raw,
                   const std::vector<float> &rgb,
                   WorkingImage &out);
};

#endif // RAWCOLOR_H
