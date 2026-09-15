#ifndef OUTPUTTRANSFORM_H
#define OUTPUTTRANSFORM_H

#include <QImage>
#include <QColorSpace>
#include "Develop/lut3d.h"
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

    /*
        THE VIEW TRANSFORM -- how scene-linear data is mapped to a displayable range.

        This used to be a hardcoded constant (a fixed exposure lift plus an ACES shoulder)
        welded into the transfer stage, which made the app's whole look one number nobody
        could choose. It is now a value, and the output stage is three composable steps:

            view transform  ->  output primaries  ->  transfer function

        Only the first is a creative decision; the other two are colour management.

        SCENE-REFERRED ONLY. A view transform tone-maps; a display-referred file (a JPEG)
        already carries its camera's tone curve, so applying one would tone-map twice.
        The transform is therefore forced to None when img.sceneReferred is false, and
        callers do not have to special-case it.
    */
    /*
        THE NUMBERING IS A PUBLISHED FORMAT. These values are what EditParams stores and
        what lands in the sidecar, so they may be ADDED TO but never renumbered. None is
        0 deliberately: it is the IDENTITY, and the identity has to be zero, because a
        default-constructed EditParams is what "no edits" means -- what an untouched raw
        renders as, and what Reset Basic restores. A non-zero identity makes every reset
        and every isIdentity() test carry a special case for this one field.
    */
    enum class ViewTransform {
        None   = 0, // the default: no tone mapping -- scene-linear to the transfer fn
        Filmic = 1, // +0.68 EV then an ACES/Narkowicz shoulder
        AgX    = 2  // log2 window + inset/outset gamut compression (see the .cpp)
    };

    /* EditParams::viewTransform (an int, so it rides the existing int machinery) -> the
       enum. An unrecognised value resolves to the DEFAULT rather than asserting: a
       sidecar written by a later build that added a transform must still open here and
       render, showing the default look. */
    static ViewTransform ViewFromInt(int v)
    {
        switch (v) {
        case int(ViewTransform::AgX):    return ViewTransform::AgX;
        case int(ViewTransform::Filmic): return ViewTransform::Filmic;
        default: break;
        }
        return ViewTransform::None;
    }

    /* The QColorSpace to TAG output produced for space with. */
    static QColorSpace ColorSpaceOf(Space space);

    /*
        THE CREATIVE LOOK -- a film-simulation LUT, and the one stage here that is not
        colour management.

        WHY IT LIVES IN THE OUTPUT STAGE AT ALL. A .cube or HaldCLUT is authored against
        DISPLAY-REFERRED sRGB: that is the encoding its author was looking at. This is
        the only place in the pipeline where pixels are in that state, so it is the only
        place the look means what it says. Everything in Develop happens before it, which
        is the right reading anyway -- the adjustments are the exposure, the look is the
        film stock they are printed on.

        IT IS APPLIED IN sRGB WHATEVER THE OUTPUT SPACE IS. The look is sandwiched
        between a conversion to linear sRGB and back, so a P3 or Adobe RGB export gets
        the SAME look as the loupe rather than the table misread through the wrong
        primaries. See ApplyLook in the .cpp.

        nullptr -- the default -- is no look, and that path is byte-for-byte what this
        transform produced before looks existed (pinned by
        tst_outputtransform::noLookOutputIsUnchanged). The pointee must outlive the call
        and is read from every worker thread, so it must be immutable: LutStore hands out
        shared_ptr<const Lut3d::Table> and never mutates one after construction.

        A caller that forgets this argument silently renders without the look, so every
        call site is listed in notes/Documentation.txt, "Film-Look LUTs".
    */

    /* Scene-linear float -> 8-bit QImage (Format_RGB888) in space.
       view defaults to None: the identity, so a caller that does not carry a recipe
       renders the pixels as they are rather than imposing a look of its own. */
    bool ToImage(const WorkingImage &img, QImage &out, Space space = Space::sRGB,
                 ViewTransform view = ViewTransform::None,
                 const Lut3d::Table *look = nullptr);

    /* Scene-linear float -> 16-bit QImage (Format_RGBX64), for export. Same view
       transform, primaries and transfer function as ToImage, quantised to 16 bits
       instead of 8 -- one shared code path, so export cannot drift from the loupe. */
    bool ToImage16(const WorkingImage &img, QImage &out, Space space = Space::sRGB,
                   ViewTransform view = ViewTransform::None,
                   const Lut3d::Table *look = nullptr);
};

#endif // OUTPUTTRANSFORM_H
