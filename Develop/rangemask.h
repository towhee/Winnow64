#ifndef RANGEMASK_H
#define RANGEMASK_H

/*
    Shared reference + coverage for the CONTENT-BASED develop masks (Luminance Range and
    Color Range), used by BOTH the ImageView live overlay (preview) and the develop render
    (mainwindow buildMaskBuffer) so the two are pixel-identical. This mirrors the Brush
    auto-mask guide (Develop/brushstamp.h): a small map of the displayed image registered by
    image path and sampled by both sides through the same output-normalized coords. Header-
    only (all inline, no Q_OBJECT), so no build-system entry is needed.

    Unlike the geometric mask tools (linear/radial), a range mask's coverage depends on the
    pixel VALUE, not its position, so evalMaskComp's coords-only signature cannot express it.
    RangeRef supplies those values.

    RangeRef is a display-referred RGB map (0..1, gamma-encoded sRGB as shown) of the
    DEVELOPED BASE layer -- Base params + OutputTransform + EXIF orientation -- output-
    oriented and capped in size. It is BASE-ONLY (not the full composite) on purpose: a range
    mask that shifts its own tones must not feed back into its own selection (which would
    oscillate). Coverage is measured on what the user sees: luminance uses the same Rec.601
    weights as the brush guide; colour distance is a cheap luma/chroma opponent-space metric.

    onx/ony are OUTPUT-normalized (0..1 of the oriented image) -- the same space the geometric
    tools and buildMaskBuffer's degrees mapping already use, so a range component drops into
    the same per-pixel loop.
*/

#include <vector>
#include <cmath>
#include <algorithm>
#include <memory>
#include <QHash>
#include <QString>
#include <QMutex>

namespace RangeMask {

struct RangeRef {
    std::vector<float> rgb;     // interleaved R,G,B 0..1, row-major, output-oriented, display-referred
    int w = 0, h = 0;
    bool valid() const { return w > 0 && h > 0 && rgb.size() == size_t(w) * size_t(h) * 3; }
};

/* Path-registered store (mirrors BrushStamp::guideStore): the GUI thread builds/registers the
   ref; the render worker reads it. Crude-capped -- each ref is a few MB. */
inline QMutex &refMutex() { static QMutex m; return m; }
inline QHash<QString, std::shared_ptr<const RangeRef>> &refStore()
{ static QHash<QString, std::shared_ptr<const RangeRef>> s; return s; }

inline void putRef(const QString &path, std::shared_ptr<const RangeRef> r)
{
    QMutexLocker lk(&refMutex());
    if (refStore().size() > 8) refStore().clear();
    refStore().insert(path, std::move(r));
}

inline std::shared_ptr<const RangeRef> getRef(const QString &path)
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

inline float luma(float r, float g, float b) { return 0.299f * r + 0.587f * g + 0.114f * b; }

/* Sample the reference RGB at output-normalized (onx,ony). */
inline void sampleRGB(const RangeRef &ref, double onx, double ony, float &r, float &g, float &b)
{
    const int x = std::clamp(int(onx * ref.w), 0, ref.w - 1);
    const int y = std::clamp(int(ony * ref.h), 0, ref.h - 1);
    const float *p = &ref.rgb[(size_t(y) * ref.w + size_t(x)) * 3];
    r = p[0]; g = p[1]; b = p[2];
}

/* ---- Luminance Range ----
   1 where the pixel's luminance lies in [lo,hi], easing to 0 over a smootherstep band whose
   half-width is set by featherPct (0..100 -> up to 0.5 of the 0..1 luma range). inverted flips. */
inline float lumCoverage(const RangeRef &ref, double onx, double ony,
                         double lo, double hi, double featherPct, bool inverted)
{
    float r, g, b; sampleRGB(ref, onx, ony, r, g, b);
    const double L = luma(r, g, b);
    const double band = std::clamp(featherPct / 100.0, 0.0, 1.0) * 0.5;
    double v;
    if (L >= lo && L <= hi)      v = 1.0;
    else if (band <= 1e-6)       v = 0.0;
    else if (L < lo)             v = 1.0 - smoother((lo - L) / band);
    else                         v = 1.0 - smoother((L - hi) / band);
    const float c = float(v);
    return inverted ? 1.0f - c : c;
}

/* ---- Color Range ----
   A sampled colour in a cheap luma/chroma opponent space (luma down-weighted so the match
   keys on hue/saturation more than brightness, as Lightroom's colour range does). Callers
   pre-convert each stored sample with toOpp() once, then pass the list. */
struct ColorSample { float y, cb, cr; };

inline ColorSample toOpp(float r, float g, float b)
{
    const float Y = luma(r, g, b);
    return { Y, b - Y, r - Y };
}

/* Distance tolerance from refine (0..100): higher refine = tighter selection. */
inline double colorTol(double refinePct)
{
    return 0.05 + (1.0 - std::clamp(refinePct / 100.0, 0.0, 1.0)) * 0.45;   // 0.05 .. 0.50
}

/* Coverage 1 where the pixel is within tol of the NEAREST sample, easing to 0 over a
   smootherstep band around tol set by featherPct. No samples -> nothing selected. */
inline float colorCoverage(const RangeRef &ref, double onx, double ony,
                           const std::vector<ColorSample> &samples,
                           double refinePct, double featherPct, bool inverted)
{
    if (samples.empty()) return inverted ? 1.0f : 0.0f;
    float r, g, b; sampleRGB(ref, onx, ony, r, g, b);
    const ColorSample px = toOpp(r, g, b);
    double best = 1e30;
    for (const ColorSample &s : samples) {
        const double dy  = (px.y  - s.y) * 0.5;    // luma down-weighted
        const double dcb =  px.cb - s.cb;
        const double dcr =  px.cr - s.cr;
        const double d = std::sqrt(dy * dy + dcb * dcb + dcr * dcr);
        if (d < best) best = d;
    }
    const double tol  = colorTol(refinePct);
    const double band = std::max(1e-6, std::clamp(featherPct / 100.0, 0.0, 1.0) * tol);
    const double loEdge = tol - band * 0.5, hiEdge = tol + band * 0.5;
    double v;
    if (best <= loEdge)      v = 1.0;
    else if (best >= hiEdge) v = 0.0;
    else                     v = 1.0 - smoother((best - loEdge) / band);
    const float c = float(v);
    return inverted ? 1.0f - c : c;
}

} // namespace RangeMask

#endif // RANGEMASK_H
