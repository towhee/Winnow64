#ifndef PROFILETONE_H
#define PROFILETONE_H

#include <cmath>
#include <vector>

/*
    A DNG camera profile's ProfileToneCurve -- the contrast shaping that comes with a
    creative "look" profile, sampled into a 1-D table and applied ratio-preservingly.
    Pure math, no Qt, so it unit-tests in isolation (the shape of Develop/calibrate.h and
    Develop/huesatmap.h).

    APPLIED TO THE VALUE, NOT PER CHANNEL. A curve run independently on R, G and B pulls
    colours toward the primaries as it steepens -- the familiar per-channel-curve hue
    shift. Winnow applies it to V (the max channel) and scales the triple by the ratio, so
    hue and saturation are untouched and the curve does what its name says: tone. That is
    also how it composes with the LookTable, which scales value in the same HSV frame.

    THE CURVE IS EXTENDED LINEARLY ABOVE WHITE, not clamped. A ProfileToneCurve is defined
    over 0..1 because DNG describes a display-referred pipeline; Winnow's working data is
    SCENE-REFERRED and carries stops of headroom that the view transform exists to roll
    off. Clamping at the top of the curve would flatten every specular highlight the moment
    a look was enabled -- the same failure the HueSatMap's value axis had to avoid, and the
    same resolution: below white the curve applies as written, above it the curve continues
    at the slope it ended with, so the function stays monotonic and continuous and the
    headroom survives.
*/
namespace ProfileTone {

/* Samples across 0..1. 1024 is well beyond the 32..256 control points real profiles
   carry, so the table is finer than the data it is built from and the linear reads
   between samples cost nothing in accuracy. */
constexpr int kSize = 1024;

struct Lut {
    std::vector<float> y;           // kSize samples of the curve over [0,1]
    float endSlope = 1.0f;          // dy/dx at x == 1, for the extension above white

    bool isEmpty() const { return y.size() != size_t(kSize); }

    /* The curve at v, extended linearly above 1. */
    float eval(float v) const {
        if (isEmpty()) return v;
        if (v <= 0.0f) return 0.0f;
        if (v >= 1.0f) return y[kSize - 1] + (v - 1.0f) * endSlope;
        const float f = v * float(kSize - 1);
        const int i = int(f);
        const float t = f - float(i);
        const int i1 = (i + 1 < kSize) ? i + 1 : i;
        return y[i] * (1.0f - t) + y[i1] * t;
    }
};

/*
    Build from the tag's flat run of x,y pairs (x ascending, both in 0..1). False when the
    data is unusable, in which case the caller applies no curve rather than a broken one.

    Sampled by LINEAR interpolation between the profile's own control points. Real profiles
    carry 32 to 256 of them -- dense enough that a smoother reconstruction would be
    inventing detail the profile did not specify, and every measured one runs exactly from
    (0,0) to (1,1).
*/
inline bool Build(const std::vector<float> &xy, Lut &out)
{
    out = Lut();
    const size_t pts = xy.size() / 2;
    if (pts < 2 || xy.size() % 2 != 0) return false;

    /* x must be non-decreasing for the walk below to terminate correctly. A profile that
       is not is corrupt, and a curve read out of order would be a wild tone change. */
    for (size_t i = 1; i < pts; ++i)
        if (xy[i * 2] < xy[(i - 1) * 2]) return false;

    out.y.resize(kSize);
    size_t seg = 0;
    for (int i = 0; i < kSize; ++i) {
        const float x = float(i) / float(kSize - 1);
        while (seg + 2 < pts && xy[(seg + 1) * 2] < x) ++seg;
        const float x0 = xy[seg * 2],     y0 = xy[seg * 2 + 1];
        const float x1 = xy[(seg + 1) * 2], y1 = xy[(seg + 1) * 2 + 1];
        const float dx = x1 - x0;
        const float t = (dx > 1e-9f) ? (x - x0) / dx : 0.0f;
        out.y[size_t(i)] = y0 + (y1 - y0) * (t < 0.0f ? 0.0f : (t > 1.0f ? 1.0f : t));
    }

    /* Slope at the top, from the last pair of samples. Used for everything above white. */
    const float dx = 1.0f / float(kSize - 1);
    out.endSlope = (out.y[kSize - 1] - out.y[kSize - 2]) / dx;
    if (!(out.endSlope > 0.0f)) out.endSlope = 1.0f;    // a flat or falling top would
                                                        // clamp the headroom after all
    return true;
}

/* Apply to one RGB triple, holding hue and saturation. */
inline void ApplyToValue(const Lut &l, float &r, float &g, float &b)
{
    if (l.isEmpty()) return;
    const float v = std::fmax(r, std::fmax(g, b));
    if (!(v > 0.0f)) return;                    // black has no tone to shape
    const float scale = l.eval(v) / v;
    r *= scale; g *= scale; b *= scale;
}

} // namespace ProfileTone

#endif // PROFILETONE_H
