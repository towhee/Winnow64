#ifndef DEPTHMASK_H
#define DEPTHMASK_H

/*
    Shared reference + coverage for the AI "Depth Range" develop mask. A single-channel DEPTH FIELD
    (0 = near, 1 = far), produced once by the MiDaS depth model (Utilities/depthpredictor) and guided-
    upsampled (Utilities/segrefine.h), registered by image path and sampled identically by the
    ImageView overlay (preview) and the develop render (buildMaskBuffer) so the two are pixel-identical.
    Header-only (all inline, no Q_OBJECT).

    Unlike Subject/Sky (binary saliency thresholded at 0.5), Depth Range is a BAND selection over a
    continuous field -- the exact analogue of RangeMask::lumCoverage over luminance, but the value is
    per-pixel DEPTH instead of luminance. coverage() selects [lo,hi] with a smootherstep feather band.

    onx/ony are OUTPUT-normalized (0..1 of the oriented image), the same space the other tools use.
*/

#include <vector>
#include <cmath>
#include <algorithm>
#include <memory>
#include <QHash>
#include <QString>
#include <QMutex>

namespace DepthMask {

struct DepthRef {
    std::vector<float> depth;   // single channel 0=near..1=far, row-major, output-oriented
    int w = 0, h = 0;
    bool valid() const { return w > 0 && h > 0 && depth.size() == size_t(w) * size_t(h); }
};

inline QMutex &refMutex() { static QMutex m; return m; }
inline QHash<QString, std::shared_ptr<const DepthRef>> &refStore()
{ static QHash<QString, std::shared_ptr<const DepthRef>> s; return s; }

inline void putRef(const QString &path, std::shared_ptr<const DepthRef> r)
{
    QMutexLocker lk(&refMutex());
    if (refStore().size() > 8) refStore().clear();
    refStore().insert(path, std::move(r));
}

inline std::shared_ptr<const DepthRef> getRef(const QString &path)
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

/* Bilinear sample of the depth at output-normalized (onx,ony). */
inline float sampleDepth(const DepthRef &ref, double onx, double ony)
{
    const double fx = std::clamp(onx, 0.0, 1.0) * (ref.w - 1);
    const double fy = std::clamp(ony, 0.0, 1.0) * (ref.h - 1);
    const int x0 = int(fx), y0 = int(fy);
    const int x1 = std::min(x0 + 1, ref.w - 1), y1 = std::min(y0 + 1, ref.h - 1);
    const double tx = fx - x0, ty = fy - y0;
    const float *c = ref.depth.data();
    const float c00 = c[size_t(y0) * ref.w + x0], c10 = c[size_t(y0) * ref.w + x1];
    const float c01 = c[size_t(y1) * ref.w + x0], c11 = c[size_t(y1) * ref.w + x1];
    const float top = float(c00 + (c10 - c00) * tx);
    const float bot = float(c01 + (c11 - c01) * tx);
    return float(top + (bot - top) * ty);
}

/* 1 where depth is in [lo,hi], easing to 0 over a smootherstep band whose half-width is featherPct
   (0..100 -> up to 0.5 of the 0..1 depth range). inverted flips. Mirrors RangeMask::lumCoverage. */
inline float coverage(const DepthRef &ref, double onx, double ony,
                      double lo, double hi, double featherPct, bool inverted)
{
    const double D = sampleDepth(ref, onx, ony);
    const double band = std::clamp(featherPct / 100.0, 0.0, 1.0) * 0.5;
    double v;
    if (D >= lo && D <= hi)      v = 1.0;
    else if (band <= 1e-6)       v = 0.0;
    else if (D < lo)             v = 1.0 - smoother((lo - D) / band);
    else                         v = 1.0 - smoother((D - hi) / band);
    const float c = float(v);
    return inverted ? 1.0f - c : c;
}

} // namespace DepthMask

#endif // DEPTHMASK_H
