#ifndef INPUTTRANSFORM_H
#define INPUTTRANSFORM_H

#include <QImage>
#include "Develop/workingimage.h"

/*
    Non-raw decode -> WorkingImage: the "un-gamma to linear" input step. Takes a decoded
    display-referred QImage (sRGB, 8-bit) and produces a scene-linear float WorkingImage that
    the Develop stage can operate on. The symmetric counterpart of OutputTransform; together
    they bracket Develop so every image type is edited in the same linear working space.

    THE PIXELS MUST BE THE FILE'S OWN: decoded, oriented, never display-converted. An
    image-cache image has been through ImageDecoder::colorManage (file profile -> DISPLAY
    profile) when colour management is on, and is NOT a valid input -- read as sRGB, its
    numbers were then converted for the display a second time on the way out. Only
    ImageDecoder builds a display-referred WorkingImage, before colorManage(); see
    ImageDecoder::applyDevelop and "15.2 Display Colour Management of Rendered Frames" in
    notes/Documentation.md.

    icc is the file's embedded profile (the one colorManage would use). Empty = sRGB, the
    fast path. Otherwise lcms converts it to the linear working space directly, so a
    wide-gamut file (Adobe RGB, Display P3) keeps its gamut instead of being read as
    sRGB. A profile that is not RGB, or that lcms cannot open, falls back to sRGB. Alpha
    is dropped.
*/
class InputTransform
{
public:
    bool FromImage(const QImage &in, WorkingImage &out, const QByteArray &icc = QByteArray());
};

#endif // INPUTTRANSFORM_H
