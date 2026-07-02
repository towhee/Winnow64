#ifndef SKYMASK_H
#define SKYMASK_H

/*
    Shared reference + coverage for the AI "Select Sky" develop mask -- the sky twin of
    Develop/subjectmask.h. A single-channel sky coverage (0..1), produced once by the sky
    segmentation model (Utilities/skypredictor) and edge-refined (Utilities/segrefine.h), registered
    by image path and sampled identically by the ImageView overlay (preview) and the develop render
    (buildMaskBuffer) so the two are pixel-identical. Header-only (all inline, no Q_OBJECT).

    A SEPARATE store from SubjectMask on purpose: a layer stack may carry both a Subject and a Sky
    mask on the same image, so their coverage maps must not collide on the shared path key.

    onx/ony are OUTPUT-normalized (0..1 of the oriented image) -- the same space the geometric, range
    and subject tools use, so a sky component drops into the same per-pixel loop.
*/

#include <vector>
#include <cmath>
#include <algorithm>
#include <memory>
#include <QHash>
#include <QString>
#include <QMutex>

namespace SkyMask {

struct SkyRef {
    std::vector<float> cov;     // single channel 0..1, row-major, output-oriented
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

inline float smoother(double v)
{
    v = v < 0.0 ? 0.0 : (v > 1.0 ? 1.0 : v);
    return float(v * v * v * (v * (v * 6.0 - 15.0) + 10.0));   // smootherstep (quintic)
}

/* Bilinear sample of the coverage at output-normalized (onx,ony). */
inline float sampleCov(const SkyRef &ref, double onx, double ony)
{
    const double fx = std::clamp(onx, 0.0, 1.0) * (ref.w - 1);
    const double fy = std::clamp(ony, 0.0, 1.0) * (ref.h - 1);
    const int x0 = int(fx), y0 = int(fy);
    const int x1 = std::min(x0 + 1, ref.w - 1), y1 = std::min(y0 + 1, ref.h - 1);
    const double tx = fx - x0, ty = fy - y0;
    const float *c = ref.cov.data();
    const float c00 = c[size_t(y0) * ref.w + x0], c10 = c[size_t(y0) * ref.w + x1];
    const float c01 = c[size_t(y1) * ref.w + x0], c11 = c[size_t(y1) * ref.w + x1];
    const float top = float(c00 + (c10 - c00) * tx);
    const float bot = float(c01 + (c11 - c01) * tx);
    return float(top + (bot - top) * ty);
}

/* Coverage for the sky at (onx,ony): the saliency thresholded around 0.5 with a feather band
   (featherPct 0..100 -> up to 0.5 half-width). inverted flips (selects non-sky). */
inline float coverage(const SkyRef &ref, double onx, double ony, float featherPct, bool inverted)
{
    const float s = sampleCov(ref, onx, ony);
    const double band = std::clamp(double(featherPct) / 100.0, 0.0, 1.0) * 0.5;
    double v;
    if (band <= 1e-6)  v = (s >= 0.5f) ? 1.0 : 0.0;      // hard threshold
    else               v = smoother((s - (0.5 - band)) / (2.0 * band));
    const float c = float(v);
    return inverted ? 1.0f - c : c;
}

} // namespace SkyMask

#endif // SKYMASK_H
