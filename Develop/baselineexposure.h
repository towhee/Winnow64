#ifndef BASELINEEXPOSURE_H
#define BASELINEEXPOSURE_H

#include <QString>
#include "Develop/baselineexposuredata.h"

/*
    ADOBE'S PER-CAMERA BASELINE EXPOSURE, the brightness Lightroom adds to every raw from
    a given body before anything else (dng_render.cpp: the exposure ramp is the user's
    Exposure + TotalBaselineExposure). It is not small and not uniform: across the bodies
    sampled it runs from -0.78 EV (Leica M10, early Nikons) to +1.15 EV (Fujifilm X-T2).
    Winnow used to ignore it and absorb ONE camera's value into Standard roll-off's lift,
    so every other body rendered off by the difference -- which is why the lift was first
    "validated" at +0.68 EV on an A9 II and later re-measured at +0.25 EV on an ILCE-1,
    and why a D7200 came out darker than Lightroom.

    Applied once, at stage 0, folded into the camera-native -> working matrix
    (Develop::InputMatrix): that is the one step every raw passes through exactly once,
    so a mask layer -- developed from a copy of the converted base -- cannot add it a
    second time, and it lands before the profile's LookTable as in the SDK.

    The values come from DNGs Adobe DNG Converter made of one sample per body
    (tools/gen_baseline_exposure.py -> Develop/baselineexposuredata.h).
*/
namespace BaselineExposure {

/* For a body missing from the table: the most common value in it (Sony, Nikon and Canon
   full-frame bodies alike). Better than 0, which would render an unlisted camera darker
   than every listed one of the same family. */
constexpr float kUnlistedEv = 0.35f;

/*
    EV for a canonical camera model ("Sony ILCE-1"; ImageFormats/Raw/cameramodel.h).
    Empty -- a non-raw file or a synthetic test image -- is 0: nothing to correct. Adobe
    spells newer Olympus bodies "OM Digital Solutions"; the canonical model follows
    libraw's "Olympus", so the two prefixes are tried both ways.
*/
inline float ForModel(const QString &model)
{
    if (model.isEmpty()) return 0.0f;
    QStringList keys{model};
    const QString om = QStringLiteral("OM Digital Solutions "), oly = QStringLiteral("Olympus ");
    if (model.startsWith(oly, Qt::CaseInsensitive)) keys << om + model.mid(oly.size());
    if (model.startsWith(om, Qt::CaseInsensitive))  keys << oly + model.mid(om.size());
    for (const QString &k : keys)
        for (const auto &e : BaselineExposureData::kTable)
            if (k.compare(QLatin1String(e.model), Qt::CaseInsensitive) == 0) return e.ev;
    return kUnlistedEv;
}

} // namespace BaselineExposure

#endif // BASELINEEXPOSURE_H
