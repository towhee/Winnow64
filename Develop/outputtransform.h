#ifndef OUTPUTTRANSFORM_H
#define OUTPUTTRANSFORM_H

#include <QImage>
#include "Develop/workingimage.h"

/*
    Final stage: converts a scene-linear WorkingImage to a display QImage by applying the
    output transfer function (and, later, the ICC display profile). This is the gamma/ICC
    step that RawColor bakes in today; once the Develop pipeline is wired, RawColor stops at
    linear and this stage owns the conversion for BOTH raw and non-raw paths.

    SCAFFOLD: applies a plain linear -> sRGB transfer and packs to 8-bit RGB. Full ICC output
    via the existing Lcms2 module is a later phase.
*/
class OutputTransform
{
public:
    bool ToImage(const WorkingImage &img, QImage &out);
};

#endif // OUTPUTTRANSFORM_H
