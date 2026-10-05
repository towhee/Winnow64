#ifndef OUTPUTLOOK_H
#define OUTPUTLOOK_H

#include "Develop/cameracurve.h"
#include "Develop/huesatmap.h"

/*
    THE LOOK HALF OF A CAMERA PROFILE, carried from Develop to the output stage.

    A DNG profile has two halves. The CHARACTERISATION (matrices + HueSatMap) describes
    the sensor and belongs at the front of the pipeline, before any edit -- stage 0.
    The LOOK (LookTable, BaselineExposureOffset, ProfileToneCurve) is a rendering, and the
    DNG SDK applies it AFTER exposure, in this order (dng_render.cpp, ProcessArea):

        HueSatMap -> exposure (incl. BaselineExposureOffset) -> LookTable -> ToneCurve

    So it belongs at the END of Winnow's pipeline, after every edit, which is where the
    view transform already lives. It used to run at stage 0, before exposure, with the
    curve BEFORE the LookTable: a +3 EV push then multiplied pixels the curve had already
    taken to display white -- the hard clip -- and the LookTable read its value axis off
    post-curve data it was not fitted to.

    Built once per render by Develop::ApplyProfileTables (the base pass, where the profile
    is resolved) and attached to the WorkingImage; OutputTransform applies it. Immutable
    once built, shared by the copies the scope compositor makes.

    WHICH PARTS APPLY depends on the tone mapping chosen (OutputTransform):
      Profile curve   offset -> LookTable -> the profile's curve (with Winnow's roll-off)
      anything else   offset -> LookTable -> offset undone -> Winnow's tone mapping
    The offset is the exposure the look was BUILT at, so the LookTable always reads at
    that exposure; it is undone afterwards for Winnow's own curves, which were fitted
    against renders without it (Standard roll-off against Adobe Standard, offset 0).
*/
struct OutputLook {
    HueSatMap::Table lookTable;     // empty when the profile carries none
    float exposureScale = 1.0f;     // 2^BaselineExposureOffset
    float toTable[3][3]   = {{1,0,0}, {0,1,0}, {0,0,1}};   // working -> linear ProPhoto
    float fromTable[3][3] = {{1,0,0}, {0,1,0}, {0,0,1}};   // and back
    CameraCurve::Curve profileCurve;    // empty when the profile carries no tone curve

    bool hasLookTable() const { return !lookTable.isEmpty(); }
    bool hasProfileCurve() const { return !profileCurve.isEmpty(); }
};

#endif // OUTPUTLOOK_H
