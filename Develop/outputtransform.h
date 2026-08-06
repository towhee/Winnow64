#ifndef OUTPUTTRANSFORM_H
#define OUTPUTTRANSFORM_H

#include <QImage>
#include <QColorSpace>
#include "Develop/workingimage.h"

/*
    Final stage: converts a scene-linear WorkingImage to a display QImage by applying the
    output primaries and transfer function. This is the gamma/ICC step that RawColor bakes
    in today; once the Develop pipeline is wired, RawColor stops at linear and this stage
    owns the conversion for BOTH raw and non-raw paths.

    Full ICC output via the existing Lcms2 module (arbitrary destination profiles) is a
    later phase; the three spaces below are done with a fixed 3x3 and a known curve, which
    covers what export offers.

    COLOUR SPACE. The pipeline works in scene-linear with sRGB (Rec.709) primaries, D65.
    Space picks what comes out:

      sRGB        primaries unchanged, sRGB piecewise transfer -- the display/loupe path
                  and the default everywhere.
      DisplayP3   3x3 to P3 primaries in LINEAR, then the SAME sRGB piecewise transfer
                  (which is what the Display P3 spec uses).
      AdobeRGB    3x3 to Adobe RGB (1998) primaries in linear, then its gamma 563/256
                  (~2.2) curve.

    Both matrices are non-negative with rows summing to 1, so white stays white and an
    in-gamut sRGB input cannot leave 0..1: the wider spaces simply do not use their extra
    gamut when fed sRGB-primaries data. That is the expected result of a WIDER
    container -- the point is that the file is correctly tagged and loses nothing when
    the working space itself widens later.

    Whatever writes the QImage must tag it to MATCH: use ColorSpaceOf(space), or the file
    says one thing and the pixels another.

    BIT DEPTH. ToImage packs to 8-bit RGB888 -- the loupe hot path, and what every
    interactive render uses. ToImage16 is its 16-bit twin (Format_RGBX64) for EXPORT: the
    develop pipeline is float throughout, so 8-bit packing is the only place precision is
    lost, and a 16-bit TIFF/PNG export is worth having for images that will be edited
    onward. The maths is identical in both; only the final quantisation differs.
*/
class OutputTransform
{
public:
    /* Output colour space. Deliberately NOT Export/exportsettings.h's OutputSpace: that
       one is a published preset key owned by the export layer, and Develop must not
       depend on Export. ImageExporter maps between them. */
    enum class Space { sRGB, DisplayP3, AdobeRGB };

    /* The QColorSpace to TAG output produced for space with. */
    static QColorSpace ColorSpaceOf(Space space);

    /* Scene-linear float -> 8-bit QImage (Format_RGB888) in space. */
    bool ToImage(const WorkingImage &img, QImage &out, Space space = Space::sRGB);

    /* Scene-linear float -> 16-bit QImage (Format_RGBX64), for export. Same tone curve,
       primaries and transfer function as ToImage, quantised to 16 bits instead of 8. */
    bool ToImage16(const WorkingImage &img, QImage &out, Space space = Space::sRGB);
};

#endif // OUTPUTTRANSFORM_H
