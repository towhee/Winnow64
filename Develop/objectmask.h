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
#include <cstdint>
#include <cmath>
#include <algorithm>
#include <memory>
#include <QHash>
#include <QString>
#include <QMutex>

namespace ObjectMask {

/*
    ---- Perimeter-paint closure + fill ----
    The Object Mask is painted as a PERIMETER: the user traces the object boundary with a
    solid brush (multiple strokes; Alt erases). The strokes accumulate into a perimeter
    coverage raster; this pair of helpers decides whether that boundary encloses a region
    and, if so, returns the filled silhouette (perimeter + interior) handed to SAM 2 as a
    dense prompt. Header-only and shared so the ImageView live overlay (preview) and the
    develop render (parseObjectBrush) compute the SAME closure/fill.

    bridgePx: the gap-bridging dilation, as a fraction of the long edge so the closure
    decision is resolution-independent (preview and render rasterize at different sizes).
    Both sides MUST call this so a loop shown closed (green) in the preview also fills.
*/
inline int bridgePx(int w, int h)
{
    return std::max(1, int(std::round(0.008 * std::max(w, h))));
}

/*
    fillEnclosed: `perim` is the accumulated perimeter coverage (row-major w*h, 0..1,
    output-oriented). Morphological close + border flood-fill (robust to a thick wall):
      1. Threshold at 0.5 -> a binary wall.
      2. Thicken the wall by `bridge` px (separable square dilation) so small gaps between
         separate strokes still count as closed.
      3. Flood-fill the BACKGROUND inward from every border pixel (4-neighbour BFS over
         non-wall pixels). Any non-wall pixel the flood never reaches is ENCLOSED.
      4. fill = wall OR enclosed. closed = enclosed-area fraction exceeds minAreaFrac
         (ignores the speck a not-quite-closed loop leaves). Not closed -> fill = wall.
    Returns closed; fill is always written.
*/
inline bool fillEnclosed(const std::vector<float>& perim, int w, int h, int bridge,
                         std::vector<float>& fill, double minAreaFrac = 0.0008)
{
    fill.assign(size_t(w) * size_t(h), 0.0f);
    if (w <= 0 || h <= 0 || perim.size() != size_t(w) * size_t(h)) return false;

    /* 1) binary wall */
    std::vector<uint8_t> wall(size_t(w) * h, 0);
    for (size_t i = 0; i < wall.size(); ++i) wall[i] = perim[i] >= 0.5f ? 1 : 0;

    /* 2) thicken the wall by `bridge` px (separable dilation) to bridge small gaps */
    if (bridge > 0) {
        std::vector<uint8_t> tmp(wall.size(), 0);
        for (int y = 0; y < h; ++y) {                 // horizontal pass: wall -> tmp
            const uint8_t *s = wall.data() + size_t(y) * w;
            uint8_t *d = tmp.data() + size_t(y) * w;
            for (int x = 0; x < w; ++x) {
                uint8_t v = 0;
                const int k0 = std::max(0, x - bridge), k1 = std::min(w - 1, x + bridge);
                for (int k = k0; k <= k1; ++k) if (s[k]) { v = 1; break; }
                d[x] = v;
            }
        }
        for (int x = 0; x < w; ++x) {                 // vertical pass: tmp -> wall
            for (int y = 0; y < h; ++y) {
                uint8_t v = 0;
                const int k0 = std::max(0, y - bridge), k1 = std::min(h - 1, y + bridge);
                for (int k = k0; k <= k1; ++k) if (tmp[size_t(k) * w + x]) { v = 1; break; }
                wall[size_t(y) * w + x] = v;
            }
        }
    }

    /* 3) BFS the background inward from the border (non-wall pixels only) */
    std::vector<uint8_t> reached(size_t(w) * h, 0);
    std::vector<int> stack;
    stack.reserve(size_t(w) * h / 4 + 16);
    auto push = [&](int x, int y) {
        const size_t i = size_t(y) * w + x;
        if (!wall[i] && !reached[i]) { reached[i] = 1; stack.push_back(int(i)); }
    };
    for (int x = 0; x < w; ++x) { push(x, 0); push(x, h - 1); }
    for (int y = 0; y < h; ++y) { push(0, y); push(w - 1, y); }
    while (!stack.empty()) {
        const int i = stack.back(); stack.pop_back();
        const int x = i % w, y = i / w;
        if (x > 0)     push(x - 1, y);
        if (x < w - 1) push(x + 1, y);
        if (y > 0)     push(x, y - 1);
        if (y < h - 1) push(x, y + 1);
    }

    /* 4) enclosed = neither wall nor reached; fill = wall + enclosed */
    size_t enclosed = 0;
    for (size_t i = 0; i < fill.size(); ++i) {
        const bool inside = !wall[i] && !reached[i];
        if (inside) ++enclosed;
        fill[i] = (wall[i] || inside) ? 1.0f : 0.0f;
    }
    const bool closed = double(enclosed) > minAreaFrac * double(w) * double(h);
    if (!closed)
        for (size_t i = 0; i < fill.size(); ++i) fill[i] = wall[i] ? 1.0f : 0.0f;
    return closed;
}

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
