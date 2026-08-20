#ifndef TONECURVE_H
#define TONECURVE_H

#include <QString>
#include <QStringList>
#include <cmath>

/*
    Pure tone-curve math, shared by the Develop point-op pipeline
    (Develop::buildPointCoeffs), the Curves panel's editor widget, the sidecar and the
    preset round trip -- the same header-only shape as Develop/calibrate.h and
    Develop/colorgrade.h. The only Qt used is QString, for the ONE shared encoding
    (see encode / decode below); the maths itself is raw arrays.

    WHAT THIS IS. Lightroom's Tone Curve: an ordered list of control points, in the
    PERCEPTUAL (gamma) domain, interpolated into a smooth input -> output mapping. Four
    independent curves are carried: channel 0 is the RGB composite (applied to every
    channel) and channels 1/2/3 are Red / Green / Blue (applied on top of the composite,
    to that channel alone). Both endpoints are pinned in x (0 and 1) but free in y, which
    is what gives the black-point / white-point moves.

    WHY MONOTONE CUBIC (Fritsch-Carlson) AND NOT CATMULL-ROM. A plain cubic through
    control points overshoots between closely spaced points, and an overshoot in a tone
    curve is a locally INVERTED tone -- a bright halo inside a shadow ramp.
    Fritsch-Carlson
    limits the tangents so a monotone set of control points can only produce a
    monotone curve, which is how Lightroom's curve behaves and what users expect.

    WHERE IT RUNS. Composed into the existing 1-D tone LUT in Develop::buildPointCoeffs,
    AFTER the contrast slope and the tone-region lifts (i.e. after the parametric tone
    controls, matching Lightroom's order) and BEFORE the LUT's decode back to linear. A
    point op folded into a table that is built anyway, so it costs nothing per pixel.

    DOMAIN BEYOND WHITE. The LUT runs to ~3 stops above white, so the curve is asked for
    inputs well past x = 1. Spline::eval extrapolates with the END SLOPE rather than
    clamping, so pulling the white endpoint down scales the headroom smoothly instead of
    flattening everything above 1.0 into one value.
*/
namespace ToneCurve {

/* Lightroom's ceiling. Kept small because EditParams is copied per render and snapshot
   per history entry. */
constexpr int kMaxPts   = 16;
constexpr int kChannels = 4;        // 0 = RGB composite, 1 = R, 2 = G, 3 = B

/* Minimum x separation between adjacent control points, so a drag can never stack two
   points on one x (which would give an infinite segment slope). */
constexpr float kMinGap = 0.002f;

/* The 2-point diagonal: no change. */
inline bool isIdentity(const float *x, const float *y, int n)
{
    if (n != 2) return false;
    return x[0] == 0.0f && y[0] == 0.0f && x[1] == 1.0f && y[1] == 1.0f;
}

inline void setIdentity(int &n, float *x, float *y)
{
    n = 2;
    x[0] = 0.0f; y[0] = 0.0f;
    x[1] = 1.0f; y[1] = 1.0f;
}

/* Repair one channel loaded from a sidecar / preset that a hand edit or an older build
   could have left inconsistent. Returns true if anything had to change. Anything
   that cannot be repaired in place (too few points, non-increasing x) falls back to
   the diagonal rather than guessing -- a wrong curve is more visible than a missing
   one. */
inline bool sanitize(int &n, float *x, float *y)
{
    bool fixed = false;
    if (n < 2 || n > kMaxPts) { setIdentity(n, x, y); return true; }
    for (int i = 0; i < n; ++i) {
        const float cx = x[i], cy = y[i];
        if (!std::isfinite(cx) || !std::isfinite(cy)) {
            setIdentity(n, x, y);
            return true;
        }
        if (cx < 0.0f) { x[i] = 0.0f; fixed = true; }
        if (cx > 1.0f) { x[i] = 1.0f; fixed = true; }
        if (cy < 0.0f) { y[i] = 0.0f; fixed = true; }
        if (cy > 1.0f) { y[i] = 1.0f; fixed = true; }
    }
    /* Order is checked BEFORE the endpoints are pinned, and deliberately so: pinning
       x[0] to 0 and x[n-1] to 1 can only widen the span, so it would quietly turn a
       disordered set like {0, 0.8, 0.4} into the legal-but-different {0, 0.8, 1} --
       silently reshaping the user's curve instead of admitting it could not be read. */
    for (int i = 1; i < n; ++i) {
        if (!(x[i] > x[i - 1])) { setIdentity(n, x, y); return true; }
    }
    /* Endpoints are pinned in x (y stays free -- that is the black/white point). */
    if (x[0] != 0.0f)     { x[0] = 0.0f;     fixed = true; }
    if (x[n - 1] != 1.0f) { x[n - 1] = 1.0f; fixed = true; }
    return fixed;
}

/* A built curve: control points plus their Fritsch-Carlson tangents. Build once per LUT
   build, then eval per table entry. */
struct Spline {
    int   n = 2;
    bool  identity = true;
    float x[kMaxPts] = {0.0f, 1.0f};
    float y[kMaxPts] = {0.0f, 1.0f};
    float m[kMaxPts] = {1.0f, 1.0f};        // tangent (dy/dx) at each control point

    void build(const float *xs, const float *ys, int count)
    {
        identity = isIdentity(xs, ys, count);
        n = (count < 2 || count > kMaxPts) ? 2 : count;
        if (count < 2 || count > kMaxPts) {
            x[0] = 0.0f; y[0] = 0.0f; x[1] = 1.0f; y[1] = 1.0f;
            m[0] = m[1] = 1.0f;
            identity = true;
            return;
        }
        for (int i = 0; i < n; ++i) { x[i] = xs[i]; y[i] = ys[i]; }

        /* Secant slopes, then the standard three-point tangent estimate. */
        float d[kMaxPts];
        for (int i = 0; i < n - 1; ++i) {
            const float dx = x[i + 1] - x[i];
            d[i] = (dx > 0.0f) ? (y[i + 1] - y[i]) / dx : 0.0f;
        }
        m[0] = d[0];
        m[n - 1] = d[n - 2];
        for (int i = 1; i < n - 1; ++i) m[i] = 0.5f * (d[i - 1] + d[i]);

        /* Fritsch-Carlson limiter: clip each tangent pair into the circle of radius 3
           around its secant, which is the condition for the segment to stay monotone. */
        for (int i = 0; i < n - 1; ++i) {
            if (d[i] == 0.0f) { m[i] = 0.0f; m[i + 1] = 0.0f; continue; }
            const float a = m[i] / d[i];
            const float b = m[i + 1] / d[i];
            if (a < 0.0f) m[i] = 0.0f;
            if (b < 0.0f) m[i + 1] = 0.0f;
            const float s = a * a + b * b;
            if (s > 9.0f) {
                const float t = 3.0f / std::sqrt(s);
                m[i]     = t * a * d[i];
                m[i + 1] = t * b * d[i];
            }
        }
    }

    /* Cubic Hermite inside the control range; straight-line continuation with the end
       slope outside it (see DOMAIN BEYOND WHITE above). */
    float eval(float s) const
    {
        if (identity) return s;
        if (s <= x[0])     return y[0] + m[0] * (s - x[0]);
        if (s >= x[n - 1]) return y[n - 1] + m[n - 1] * (s - x[n - 1]);
        int i = 0;
        while (i < n - 2 && s > x[i + 1]) ++i;
        const float h = x[i + 1] - x[i];
        if (h <= 0.0f) return y[i];
        const float t  = (s - x[i]) / h;
        const float t2 = t * t;
        const float t3 = t2 * t;
        const float h00 =  2.0f * t3 - 3.0f * t2 + 1.0f;
        const float h10 =         t3 - 2.0f * t2 + t;
        const float h01 = -2.0f * t3 + 3.0f * t2;
        const float h11 =         t3 -        t2;
        return h00 * y[i] + h10 * h * m[i] + h01 * y[i + 1] + h11 * h * m[i + 1];
    }
};

/* ---- The ONE shared encoding -------------------------------------------------------
   "ch:x,y,x,y,...;ch:..." with IDENTITY CHANNELS OMITTED, so an untouched image encodes
   to an empty string and an image with only an RGB curve carries only "0:...". The
   sidecar (EditStack::paramsToJson), the preset collector (collectScopeLeaves) and the
   preset applier (DevelopPresets::assignParam) all go through this pair, so the two key
   namespaces described in the develop-adjustment checklist cannot drift apart here.

   5 significant figures: below the precision a 1024-entry LUT can resolve, and it keeps
   the base64 sidecar blob short. */
inline QString encode(const int *n, const float x[][kMaxPts], const float y[][kMaxPts])
{
    QStringList chans;
    for (int c = 0; c < kChannels; ++c) {
        if (isIdentity(x[c], y[c], n[c])) continue;
        QStringList v;
        v.reserve(n[c] * 2);
        for (int i = 0; i < n[c]; ++i) {
            v << QString::number(x[c][i], 'g', 5) << QString::number(y[c][i], 'g', 5);
        }
        chans << QString::number(c) + ":" + v.join(",");
    }
    return chans.join(";");
}

/* Fills every channel: one absent from the string is reset to the diagonal, so decode()
   fully defines the state (a preset that carries "curves" replaces the whole curve set,
   it does not merge into it). Returns false if anything was malformed; the affected
   channel is left at identity either way. */
inline bool decode(const QString &s, int *n, float x[][kMaxPts], float y[][kMaxPts])
{
    for (int c = 0; c < kChannels; ++c) setIdentity(n[c], x[c], y[c]);
    if (s.isEmpty()) return true;
    bool ok = true;
    const QStringList chans = s.split(';', Qt::SkipEmptyParts);
    for (const QString &chan : chans) {
        const int colon = chan.indexOf(':');
        if (colon < 1) { ok = false; continue; }
        bool cOk = false;
        const int c = chan.left(colon).toInt(&cOk);
        if (!cOk || c < 0 || c >= kChannels) { ok = false; continue; }
        const QStringList v = chan.mid(colon + 1).split(',', Qt::SkipEmptyParts);
        const int count = v.size() / 2;
        if (v.size() % 2 || count < 2 || count > kMaxPts) { ok = false; continue; }
        float xs[kMaxPts], ys[kMaxPts];
        bool good = true;
        for (int i = 0; i < count && good; ++i) {
            bool xOk = false, yOk = false;
            xs[i] = v.at(i * 2).toFloat(&xOk);
            ys[i] = v.at(i * 2 + 1).toFloat(&yOk);
            good = xOk && yOk;
        }
        if (!good) { ok = false; continue; }
        int cn = count;
        sanitize(cn, xs, ys);
        n[c] = cn;
        for (int i = 0; i < cn; ++i) { x[c][i] = xs[i]; y[c][i] = ys[i]; }
    }
    return ok;
}

} // namespace ToneCurve

#endif // TONECURVE_H
