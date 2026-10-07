#ifndef SKYMASK_H
#define SKYMASK_H

/*
    Shared reference + coverage for the AI "Select Sky" develop mask -- the sky twin of
    Develop/subjectmask.h. A single-channel sky ALPHA (0..1), produced once per image by
    the two-model sky matte (Utilities/skypredictor + Utilities/skyrefine.h), registered
    by image path and sampled identically by the ImageView overlay (preview) and the
    develop render (buildMaskBuffer) so the two are pixel-identical. Header-only (all
    inline, no Q_OBJECT).

    RESOLUTION. The alpha is a real matte -- leaf gaps and branch tips carry fractional
    values -- so it is stored at up to 4096 px on the long edge (MW::ensureSkyMask), not
    the 1024 the old path used: bilinear sampling of a 1024 map onto a 6000 px render
    was a ~6 px ramp, and that ramp was the halo. Stored as 8 bits (11 MB at 4096x2730
    instead of 45 MB as float); a 1/255 alpha step is invisible under any adjustment.

    A SEPARATE store from SubjectMask on purpose: a scope stack may carry both a Subject
    and a Sky mask on the same image, so their coverage maps must not collide on the
    shared path key.

    onx/ony are OUTPUT-normalized (0..1 of the oriented image) -- the same space the
    geometric, range and subject tools use, so a sky component drops into the same
    per-pixel loop.
*/

#include <vector>
#include <cstdint>
#include <cmath>
#include <algorithm>
#include <memory>
#include "Develop/maskfalloff.h"
#include <QHash>
#include <QString>
#include <QMutex>

namespace SkyMask {

struct SkyRef {
    std::vector<uint8_t> cov;   // alpha 0..255, row-major, output-oriented
    int w = 0, h = 0;
    bool valid() const { return w > 0 && h > 0 && cov.size() == size_t(w) * size_t(h); }
};

/* Path-registered store (mirrors SubjectMask::refStore): the GUI thread builds/registers the ref;
   the render worker reads it. Crude-capped. */
inline QMutex &refMutex() { static QMutex m; return m; }
inline QHash<QString, std::shared_ptr<const SkyRef>> &refStore()
{ static QHash<QString, std::shared_ptr<const SkyRef>> s; return s; }

inline void putRef(const QString &path, std::shared_ptr<const SkyRef> r)
{
    QMutexLocker lk(&refMutex());
    if (refStore().size() > 8) refStore().clear();
    refStore().insert(path, std::move(r));
}

inline std::shared_ptr<const SkyRef> getRef(const QString &path)
{
    QMutexLocker lk(&refMutex());
    auto it = refStore().find(path);
    return it != refStore().end() ? it.value() : nullptr;
}


/* Bilinear sample of the coverage at output-normalized (onx,ony). */
inline float sampleCov(const SkyRef &ref, double onx, double ony)
{
    const double fx = std::clamp(onx, 0.0, 1.0) * (ref.w - 1);
    const double fy = std::clamp(ony, 0.0, 1.0) * (ref.h - 1);
    const int x0 = int(fx), y0 = int(fy);
    const int x1 = std::min(x0 + 1, ref.w - 1), y1 = std::min(y0 + 1, ref.h - 1);
    const double tx = fx - x0, ty = fy - y0;
    const uint8_t *c = ref.cov.data();
    const float c00 = c[size_t(y0) * ref.w + x0], c10 = c[size_t(y0) * ref.w + x1];
    const float c01 = c[size_t(y1) * ref.w + x0], c11 = c[size_t(y1) * ref.w + x1];
    const float top = float(c00 + (c10 - c00) * tx);
    const float bot = float(c01 + (c11 - c01) * tx);
    return float(top + (bot - top) * ty) * (1.0f / 255.0f);
}

/* Coverage for the sky at (onx,ony). Feather 0 uses the matte AS IS -- its fractional
   edge is the result, as for the Object Mask. Feather > 0 re-shapes it with a Gaussian
   band around 0.5 (featherPct 0..100 -> up to 0.5 half-width), the same control every
   AI mask has. inverted flips (selects non-sky). */
inline float coverage(const SkyRef &ref, double onx, double ony, float featherPct, bool inverted)
{
    const float s = sampleCov(ref, onx, ony);
    const double band = std::clamp(double(featherPct) / 100.0, 0.0, 1.0) * 0.5;
    const double v = (band <= 1e-6) ? std::clamp(double(s), 0.0, 1.0)     // matte as-is
                                    : MaskFalloff::cdf((s - 0.5) / (0.48 * band));
    const float c = float(v);
    return inverted ? 1.0f - c : c;
}

} // namespace SkyMask

#endif // SKYMASK_H
