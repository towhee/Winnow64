#ifndef CAMERACURVE_H
#define CAMERACURVE_H

#include <QString>
#include <QtGlobal>
#include <cmath>
#include <utility>
#include <vector>
#include "Develop/cameracurvedata.h"

/*
    CAMERA-STYLE TONE CURVES WITH WINNOW'S ROLL-OFF -- the two tone mappings that take
    their shape from a camera's own rendering: "Camera contrast" (a per-maker average
    curve, Develop/cameracurvedata.h) and "Profile curve" (the selected DNG profile's
    ProfileToneCurve). Pure maths, no pipeline state, so it unit-tests in isolation.

    WHY NOT JUST USE THE CAMERA CURVE. A camera curve (and a DCP ProfileToneCurve) is
    defined over 0..1 and puts SENSOR WHITE AT DISPLAY WHITE: by input 0.9 a typical one
    is at 0.99. It has no headroom, because a camera's JPEG engine never sees anything
    above sensor white. Winnow's data does -- exposure, a brightened mask, a Highlights
    push -- and a curve that has already reached white turns all of it into a flat white
    patch. That was the "Camera Neutral +3 EV" blowout.

    THE SHAPE. Below the knee the camera curve applies exactly as written, so shadows and
    midtones carry the camera's contrast. Above it the camera's own shoulder is REPLACED
    by a power-law roll-off that keeps rising toward 1 without reaching it:

        y = C(u)                                    u <= uk
        y = yk + (1 - yk) (1 - (1 + t)^-p)          u >  uk
        t = s (u - uk) / (p (1 - yk))

    where u = gain * x, yk = C(uk) = kKneeY, s = C'(uk) and p = kTailPower. Continuous
    with a continuous slope at the knee (C1), monotonic, asymptotic to white.

    THE TWO CONSTANTS WERE CHOSEN AGAINST STANDARD ROLL-OFF, by display level of 255
    (generic curve / D7200 Camera Neutral / Standard roll-off):

        x (1 = sensor white)     1        2        4        8       64
        this                   237/235  244/243  248/247  250/250  254/254
        Standard roll-off        236      247      253      255      255

    Sensor white lands where Standard roll-off put it, and above it the tail is GENTLER
    -- Standard roll-off clipped to 255 by 2.6 stops over, this is still separating at
    six. (Those Standard roll-off numbers are its 2026-09-17 constants; the 2026-10-04
    refit puts sensor white at 247 and white at 1.3 stops over, and the per-camera
    BaselineExposure now applied at stage 0 lifts every curve here alike.)
    The first attempt (knee at 0.80, a 1/t tail) put sensor white at ~250 and squeezed
    1.4x..3.8x white into 250..254: the +3 EV face was still nearly flat white, which
    tst_inputprofile::profileCurveRollsOffAPushedExposure caught. The knee at OUTPUT 0.60
    is well above mid grey (~0.4 linear on these curves), so the camera's shadows and
    midtones -- its contrast -- are untouched. Chosen by comparison, NOT fitted to a
    Lightroom render; refit if one says otherwise.

    APPLIED HUE-PRESERVINGLY (ApplyRGBTone), the way the DNG SDK applies a
    ProfileToneCurve (RefBaselineRGBTone): the curve is evaluated on the largest and
    smallest channels and the middle one is interpolated between them, so hue is held
    while saturation falls as the shoulder compresses the top -- bright colours desaturate
    toward white rather than marching in hue the way a per-channel curve makes them.
*/
namespace CameraCurve {

constexpr float kKneeY     = 0.60f;  // output level where the camera curve hands over
constexpr float kTailPower = 0.60f;  // roll-off tail: 1 - y ~ t^-p (smaller = gentler)
constexpr int   kLutSize = 4096;
/* The table's log2 window: 2^-16 (deep shadow) .. 2^6 (six stops over white), where the
   roll-off is within ~1% of white. A log axis gives the shadows, where camera curves
   bend hardest, the same resolution per stop as the highlights. */
constexpr float kLogMin = -16.0f;
constexpr float kLogMax =   6.0f;

/* The camera curve over 0..1 plus the knee it hands over at. */
struct Shape {
    std::vector<float> xs, ys;      // ascending x, both 0..1
    float gain = 1.0f;              // u = gain * x
    float uk = 1.0f, yk = 1.0f, s = 1.0f;

    float camera(float u) const {
        if (u <= xs.front()) return ys.front();
        if (u >= xs.back())  return ys.back();
        size_t lo = 0, hi = xs.size() - 1;
        while (hi - lo > 1) {
            const size_t m = (lo + hi) / 2;
            (xs[m] <= u ? lo : hi) = m;
        }
        const float dx = xs[hi] - xs[lo];
        const float t = dx > 1e-9f ? (u - xs[lo]) / dx : 0.0f;
        return ys[lo] + (ys[hi] - ys[lo]) * t;
    }

    /* Scene-linear in, display-linear 0..1 out. */
    float eval(float x) const {
        if (!(x > 0.0f)) return 0.0f;           // negated: catches NaN too
        const float u = gain * x;
        if (u <= uk) return camera(u);
        const float t = s * (u - uk) / (kTailPower * (1.0f - yk));
        return yk + (1.0f - yk) * (1.0f - std::pow(1.0f + t, -kTailPower));
    }
};

/* Find the knee: the first u where the curve reaches kKneeY, and its slope there. */
inline void FindKnee(Shape &sh)
{
    sh.uk = 1.0f;
    for (int i = 1; i <= 10000; ++i) {
        const float u = float(i) / 10000.0f;
        if (sh.camera(u) >= kKneeY) { sh.uk = u; break; }
    }
    sh.yk = sh.camera(sh.uk);
    if (sh.yk >= 0.999f) sh.yk = 0.999f;        // a curve that never bends: keep t finite
    const float h = 0.005f;
    const float a = sh.camera(qMax(0.0f, sh.uk - h)), b = sh.camera(sh.uk + h);
    sh.s = (b - a) / (2.0f * h);
    if (!(sh.s > 0.05f)) sh.s = 0.05f;          // flat or falling: a sane minimum slope
}

/* A built curve: the shape tabulated on the log2 axis for the per-pixel path. */
struct Curve {
    std::vector<float> lut;         // kLutSize + 1 samples, display-linear
    Shape shape;

    bool isEmpty() const { return lut.size() != size_t(kLutSize + 1); }

    float eval(float v) const {
        if (!(v > 0.0f)) return 0.0f;
        const float lmin = std::exp2(kLogMin);
        if (v <= lmin) return lut[0] * (v / lmin);        // linear into black
        const float f = (std::log2(v) - kLogMin) / (kLogMax - kLogMin) * float(kLutSize);
        if (f >= float(kLutSize)) return lut[kLutSize];
        const int i = int(f);
        const float fr = f - float(i);
        return lut[i] + (lut[i + 1] - lut[i]) * fr;
    }
};

inline void Tabulate(Curve &c)
{
    c.lut.resize(kLutSize + 1);
    for (int i = 0; i <= kLutSize; ++i) {
        const float l = kLogMin + (kLogMax - kLogMin) * float(i) / float(kLutSize);
        c.lut[i] = c.shape.eval(std::exp2(l));
    }
}

/*
    Build from a DCP ProfileToneCurve's flat run of x,y pairs (x non-decreasing, both
    0..1) and the gain to apply to the input first. False when the data is unusable, in
    which case the caller has no profile curve rather than a broken one.
*/
inline bool BuildFromPairs(const std::vector<float> &xy, float gain, Curve &out)
{
    out = Curve();
    const size_t n = xy.size() / 2;
    if (n < 2 || xy.size() % 2 != 0) return false;
    Shape sh;
    sh.gain = gain > 0.0f ? gain : 1.0f;
    for (size_t i = 0; i < n; ++i) {
        if (i && xy[i * 2] < xy[(i - 1) * 2]) return false;   // out of order: corrupt
        sh.xs.push_back(xy[i * 2]);
        sh.ys.push_back(xy[i * 2 + 1]);
    }
    FindKnee(sh);
    out.shape = sh;
    Tabulate(out);
    return true;
}

/* The CameraCurveData grid, x = (i/32)^2. */
inline Curve BuildFromSamples(const float *y)
{
    Curve c;
    Shape sh;
    for (int i = 0; i < CameraCurveData::kSamples; ++i) {
        const float t = float(i) / float(CameraCurveData::kSamples - 1);
        sh.xs.push_back(t * t);
        sh.ys.push_back(y[i]);
    }
    FindKnee(sh);
    c.shape = sh;
    Tabulate(c);
    return c;
}

inline const Curve &Generic()
{
    static const Curve c = BuildFromSamples(CameraCurveData::kGeneric);
    return c;
}

/*
    The Camera contrast curve for a body, by the maker word that leads the canonical
    camera model ("Nikon D7200", "Olympus E-M1 Mark III"; see ImageFormats/Raw/
    cameramodel.h). Adobe's "OM Digital Solutions" spelling counts as Olympus. Anything
    unlisted -- Fujifilm, an unknown body, an empty model -- gets the generic curve.
    Built once per maker for the life of the process.
*/
inline const Curve &ForCameraModel(const QString &cameraModel)
{
    using CameraCurveData::kMakers;
    constexpr int n = int(sizeof(kMakers) / sizeof(kMakers[0]));
    static const std::vector<Curve> curves = []{
        std::vector<Curve> v;
        for (int i = 0; i < n; ++i) v.push_back(BuildFromSamples(kMakers[i].y));
        return v;
    }();
    QString maker = cameraModel.section(' ', 0, 0);
    if (maker.compare("OM", Qt::CaseInsensitive) == 0) maker = "Olympus";
    for (int i = 0; i < n; ++i)
        if (maker.compare(QLatin1String(kMakers[i].name), Qt::CaseInsensitive) == 0)
            return curves[size_t(i)];
    return Generic();
}

/*
    The DNG SDK's RGBTone: curve the largest and smallest channels, interpolate the middle
    one between them in proportion. Hue is preserved exactly (the middle channel keeps its
    relative position); saturation follows the curve's compression.
*/
inline void ApplyRGBTone(const Curve &c, float &r, float &g, float &b)
{
    float *hi = &r, *mid = &g, *lo = &b;
    if (*hi < *mid) std::swap(hi, mid);
    if (*mid < *lo) std::swap(mid, lo);
    if (*hi < *mid) std::swap(hi, mid);
    const float h = *hi, m = *mid, l = *lo;
    const float th = c.eval(h);
    const float tl = c.eval(l);
    const float tm = (h > l) ? tl + (th - tl) * (m - l) / (h - l) : th;
    *hi = th; *mid = tm; *lo = tl;
}

} // namespace CameraCurve

#endif // CAMERACURVE_H
