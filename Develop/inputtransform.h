#ifndef INPUTTRANSFORM_H
#define INPUTTRANSFORM_H

#include <QImage>
#include "Develop/workingimage.h"

/*
    Non-raw decode -> WorkingImage: the "un-gamma to linear" input step. Takes a decoded
    display-referred QImage (sRGB, 8-bit) and produces a scene-linear float WorkingImage that
    the Develop stage can operate on. The symmetric counterpart of OutputTransform; together
    they bracket Develop so every image type is edited in the same linear working space.

    SCAFFOLD: assumes sRGB input and drops alpha. ICC-aware input (using the image's embedded
    profile via the Lcms2 module) is a later phase.
*/
class InputTransform
{
public:
    bool FromImage(const QImage &in, WorkingImage &out);
};

#endif // INPUTTRANSFORM_H
