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
    /* The same, taking rgb's buffer instead of copying it (a 120 MP Linear Raw DNG is
       1.5 GB of floats). */
    bool ToCameraNative(const RawImage &raw,
                   std::vector<float> &&rgb,
                   WorkingImage &out);

    /*
        Just the CHARACTERISATION half of ToCameraNative: given a model's XYZ->camera
        matrix and the file's as-shot multipliers (R, G, B, G2 -- G2 unused), fill a
        CameraColor and solve its as-shot Kelvin/tint. No pixels involved.

        Split out because the APPLE CORE IMAGE engine needs it too. That engine hands
        back linear-sRGB pixels with the as-shot balance already applied and no camera
        characterisation at all, which used to leave cam invalid and the Temp/Tint row
        reading WhiteBalance::resolve's 6500/0 default on every raw file. The matrix and
        the multipliers come off ImageMetadata::rawInfo there, not off a decoded
        RawImage, so this takes the two arrays rather than a RawImage.

        camMul all-equal (or all 1) means "the file gave us none": the matrix-derived
        neutral is used instead, exactly as in the full path -- UNLESS asShotStated says
        the file wrote them (RawSensorInfo::asShotStated; iPhone ProRAW's 1, 1, 1).
    */
    static void Characterise(const float xyzToCam[3][3], const float camMul[4],
                             CameraColor &out, bool asShotStated = false);

    /*
        CLIPPED HIGHLIGHTS -> NEUTRAL. A blown photosite reads 1.0 in every channel, but
        the as-shot white balance then scales R and B (Sony ~2x / ~1.5x) above G, so a
        blown white is really MAGENTA-white. Display clipping hides it at the default
        tone; any control that pulls the highlights back below white (Highlights -,
        Exposure -, Whites -) turns blown skies, snow and lichen pink.

        Fix, dcraw-style but soft: each channel's ceiling in white-balanced terms is
        clip_c x asShotMul_c, and a pixel whose channel c is clipped cannot be brighter
        than that ceiling in ANY channel without its colour being invented. So every
        channel is capped at the lowest ceiling among the clipped channels. A blown
        neutral therefore balances to (1,1,1) -- white -- while a saturated red with only
        R clipped keeps its colour (its ceiling is R's own). "Clipped" is a smoothstep
        over the top kKnee of each channel's clip level, and the ceiling is divided by
        it, so the cap rises to infinity as the channel leaves the knee: continuous, no
        banding at the clip edge.

        clip_c is MEASURED (a pile-up at the channel maximum), not assumed to be the
        declared white level: some cameras saturate below it, and then nothing would
        ever reach the knee. A channel with no pile-up is not clipped anywhere.

        Mosaic raws only: a Linear Raw (HDR/pano merge DNG) has no sensor clip, and its
        values above 1 are real. In camera-native space, before the cache, using the
        AS-SHOT balance (the profile's neutral); a later Temp change shifts the neutral
        a little, which is invisible next to the pink this removes.
    */
    static void NeutraliseClipped(WorkingImage &img);
};

#endif // RAWCOLOR_H
