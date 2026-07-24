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
   A HUE + SATURATION band selection in HSV (value is ignored -- brightness stays with the
   Luminance Range mask). Each sampled colour fixes a centre (hue angle, sat radius); the
   mask selects pixels whose HUE lies in [centre - hueLo, centre + hueHi] AND whose SAT
   lies in [centre - satLo, centre + satHi] of ANY sample (union), each edge eased by a
   smootherstep feather band. hueLo/hueHi are degrees; satLo/satHi are sat fractions
   (0..1) measured from the sample's own saturation, so the band tracks the sample.
   A tiny absolute saturation floor drops near-neutral pixels, whose hue is noise.

   This matches the dock's colour WHEEL exactly (same HSV hue angle + sat radius), so the
   pick dots and band sector the user drags are a faithful picture of the selection. */
struct ColorSample { float hue, sat; };    // hue degrees 0..360, sat 0..1 (HSV)

/* Fast HSV hue (degrees, 0 for neutral) + saturation (0..1) from display RGB 0..1. */
inline void rgb2hs(float r, float g, float b, float &hueDeg, float &sat)
{
    const float mx = std::max(r, std::max(g, b));
    const float mn = std::min(r, std::min(g, b));
    const float d = mx - mn;
    sat = (mx <= 0.0f) ? 0.0f : d / mx;
    if (d <= 1e-6f) { hueDeg = 0.0f; return; }
    float h;
    if      (mx == r) h = 60.0f * (std::fmod((g - b) / d, 6.0f));
    else if (mx == g) h = 60.0f * ((b - r) / d + 2.0f);
    else              h = 60.0f * ((r - g) / d + 4.0f);
    if (h < 0.0f) h += 360.0f;
    hueDeg = h;
}

inline ColorSample toHueSat(float r, float g, float b)
{
    ColorSample s; rgb2hs(r, g, b, s.hue, s.sat); return s;
}

/* Smallest signed angular difference a-b in degrees, wrapped to (-180, 180]. */
inline double angDiffDeg(double a, double b)
{
    double d = a - b;
    while (d >  180.0) d -= 360.0;
    while (d < -180.0) d += 360.0;
    return d;
}

/* One-sided smootherstep gate: 1 while |x| <= edge, easing to 0 across band beyond. */
inline double bandGate(double x, double edge, double band)
{
    const double over = std::fabs(x) - std::max(0.0, edge);
    if (over <= 0.0) return 1.0;
    return 1.0 - smoother(over / std::max(1e-6, band));
}

/* Coverage 1 where the pixel's hue AND saturation fall inside the BEST-matching sample's
   window, easing to 0 over smootherstep feather bands beyond each edge. hueLo/hueHi are
   degrees; satLo/satHi are saturation fractions (0..1). No sample -> nothing selected. */
inline float colorCoverage(const RangeRef &ref, double onx, double ony,
                           const std::vector<ColorSample> &samples,
                           double hueLoDeg, double hueHiDeg,
                           double satLo, double satHi,
                           double featherPct, bool inverted)
{
    if (samples.empty()) return inverted ? 1.0f : 0.0f;
    float r, g, b; sampleRGB(ref, onx, ony, r, g, b);
    const ColorSample px = toHueSat(r, g, b);

    const double f = std::clamp(featherPct / 100.0, 0.0, 1.0);
    const double hueBand = f * 30.0;    // up to a 30 deg soft hue edge
    const double satBand = f * 0.25;    // up to a 0.25 soft saturation edge
    const double satFloor = 0.015;      // below this the hue is noise -> not selectable

    double best = 0.0;
    for (const ColorSample &s : samples) {
        const double dh = angDiffDeg(px.hue, s.hue);   // +ve = hi side, -ve = lo side
        const double hueCov = bandGate(dh, (dh >= 0.0) ? hueHiDeg : hueLoDeg, hueBand);
        const double ds = px.sat - s.sat;              // +ve = hi side, -ve = lo side
        const double satCov = bandGate(ds, (ds >= 0.0) ? satHi : satLo, satBand);
        const double cov = hueCov * satCov;
        if (cov > best) best = cov;
    }
    if (px.sat < satFloor) best *= smoother(px.sat / satFloor);   // fade near-neutral out
    const float c = float(best);
    return inverted ? 1.0f - c : c;
}

} // namespace RangeMask

#endif // RANGEMASK_H
