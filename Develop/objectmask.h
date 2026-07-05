#ifndef OBJECTMASK_H
#define OBJECTMASK_H

/*
    Shared reference + coverage for the AI "Object Mask" develop tool, used by BOTH the ImageView
    live overlay (preview) and the develop render (mainwindow buildMaskBuffer) so the two are pixel-
    identical. Mirrors SubjectMask::SubjectRef (Develop/subjectmask.h): a small map registered by
    image path and sampled by both sides through the same output-normalized coords. Header-only (all
    inline, no Q_OBJECT), so no build-system entry is needed.

    The coverage is a FIXED alpha produced by the SAM 2 decoder (Utilities/objectmaskpredictor) from
    the user's brush stroke, edge-refined against the photo (shared SegRefine) and stored at the guide
    (~1024) resolution. Unlike the range masks it is NOT re-thresholded on slider ticks; unlike the
    Subject mask it is re-produced whenever the user edits the brush (a new decoder pass, ~40ms), and
    the ObjectRef is re-registered. coverage() bilinearly samples it and applies the component's
    feather/invert at eval time -- identical semantics to SubjectMask::coverage.

    onx/ony are OUTPUT-normalized (0..1 of the oriented image) -- the same space the geometric and
    range tools and buildMaskBuffer's degrees mapping already use, so an object component drops into
    the same per-pixel loop.
*/

#include <vector>
#include <cmath>
#include <algorithm>
#include <memory>
#include <QHash>
#include <QString>
#include <QMutex>

namespace ObjectMask {

struct ObjectRef {
    std::vector<float> cov;     // single channel 0..1, row-major, output-oriented
    int w = 0, h = 0;
    bool valid() const { return w > 0 && h > 0 && cov.size() == size_t(w) * size_t(h); }
};

/* Path-registered store (mirrors SubjectMask::refStore): the GUI thread builds/registers the ref;
   the render worker reads it. */
inline QMutex &refMutex() { static QMutex m; return m; }
inline QHash<QString, std::shared_ptr<const ObjectRef>> &refStore()
{ static QHash<QString, std::shared_ptr<const ObjectRef>> s; return s; }

inline void putRef(const QString &path, std::shared_ptr<const ObjectRef> r)
{
    QMutexLocker lk(&refMutex());
    if (refStore().size() > 8) refStore().clear();
    refStore().insert(path, std::move(r));
}

inline std::shared_ptr<const ObjectRef> getRef(const QString &path)
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
inline float sampleCov(const ObjectRef &ref, double onx, double ony)
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

/*
    Coverage for the object at (onx,ony). The alpha is thresholded around 0.5 with a feather band
    (featherPct 0..100 -> up to 0.5 half-width) so Feather softens the cutout edge; feather 0 gives a
    hard 0.5 threshold. inverted flips the selection (everything but the object).
*/
inline float coverage(const ObjectRef &ref, double onx, double ony,
                      float featherPct, bool inverted)
{
    const float s = sampleCov(ref, onx, ony);
    const double band = std::clamp(double(featherPct) / 100.0, 0.0, 1.0) * 0.5;
    double v;
    if (band <= 1e-6)  v = (s >= 0.5f) ? 1.0 : 0.0;      // hard threshold
    else               v = smoother((s - (0.5 - band)) / (2.0 * band));
    const float c = float(v);
    return inverted ? 1.0f - c : c;
}

} // namespace ObjectMask

#endif // OBJECTMASK_H
