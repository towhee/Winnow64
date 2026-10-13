#ifndef POINTCOLOR_H
#define POINTCOLOR_H

/*
    POINT COLOR -- Lightroom's Color Mixer > Point Color, without the Mixer half.

    The user samples a colour from the image with the eyedropper (up to kMaxPts of them)
    and, for each sample, shifts the hue, saturation and luminance of every pixel that
    is CLOSE to it. Unlike the 8-band HSL mixer that Winnow deliberately does not have
    (see the "Color Mix cancelled" decision), a point is a 3-D target -- hue AND
    saturation AND lightness -- so "this skin" can be moved without moving "that
    orange wall" of the same hue.

    WHERE IT RUNS. In the fused point pass, after HSL and before Color Grade, on the
    DISPLAY axis: each pixel goes scene-linear -> display-linear through the active
    view transform (the same gradeFwd / gradeInv tables Color Grade uses), the work is
    done in OKLab of that display-linear value, and only the CHANGE is carried back to
    scene-linear. The sample comes off the displayed image, so the selection has to be
    measured on what the user sees -- keyed on scene values it would select the wrong
    pixels whenever the view transform compresses them.

    THE SELECTION is a separable falloff in OKLCh about the sample:

        w = f(dHue / hueWidth) * f(dChroma / satWidth) * f(dL / lumWidth) * gate(C)

    with f(t) = (1 - t^2)^2 inside |t| < 1 (smooth, compact support, so a pixel far from
    every sample costs a compare and nothing else). Range scales all three widths;
    Hue / Saturation / Luminance Range scale one each. gate() fades out near-neutral
    pixels, whose hue is noise.

    MULTIPLE SAMPLES. A point may carry up to kMaxSamples samples (the first plus
    kMaxExtra added ones). Together they define an ENVELOPE in OKLCh -- the shortest hue
    arc covering every chromatic sample, and the chroma and lightness intervals covering
    them all -- and each distance above is measured from the envelope rather than from a
    single colour: zero inside, the usual falloff beyond its edges. So a second sample
    of the same skin in shadow widens the hue (and lightness) a point reaches without
    widening the falloff, which a bigger Range would. A one-sample point's envelope is
    the sample itself, so it selects exactly as before. A near-neutral sample adds to the
    chroma and lightness extent only: its hue is noise.

    THE ADJUSTMENT. Variance first spreads (positive) or pulls together (negative) the
    selected colours about the envelope's centre in hue and chroma -- negative is the
    "even out skin" control. Then Hue Shift rotates, Saturation Shift scales chroma and Luminance
    Shift moves L. The pixel moves toward that target by w. Every point's weight is
    measured on the INCOMING pixel and the deltas summed, so points do not depend on
    their order.

    NOT YET CALIBRATED against Lightroom: kHueShiftMaxDeg, the widths and the luminance
    response are first estimates, pending reference exports (see Documentation.txt
    "Point Color Panel").

    Header-only, no Qt beyond QString for the shared encoding.
*/

#include <cmath>
#include <QString>
#include <QStringList>
#include "Develop/colorspace.h"

namespace PointColor {

constexpr int kMaxPts = 8;
constexpr int kMaxExtra = 7;                // added samples per point
constexpr int kMaxSamples = 1 + kMaxExtra;

/* One sampled point. The sample is stored as the DISPLAYED sRGB (encoded, 0..1) the
   eyedropper read, not as OKLCh: the selection space is an implementation choice that
   calibration may change, and an sRGB triple stays meaningful whatever it becomes.
   Slider values are stored exactly as the panel shows them. */
struct Point {
    float r = 0.0f, g = 0.0f, b = 0.0f;     // sampled colour, sRGB-encoded 0..1
    float hueShift = 0.0f;                  // -100..100
    float satShift = 0.0f;                  // -100..100
    float lumShift = 0.0f;                  // -100..100
    float variance = 0.0f;                  // -100..100, 0 = unchanged spread
    float range    = 50.0f;                 // 0..100, all three widths
    float hueRange = 50.0f;                 // 0..100
    float satRange = 50.0f;                 // 0..100
    float lumRange = 50.0f;                 // 0..100
    /* Added samples that broaden the selection (see MULTIPLE SAMPLES), sRGB-encoded
       r,g,b triples. Slots past extraN are ALWAYS zero, so operator== never sees stale
       data (sanitize enforces it). */
    int   extraN = 0;
    float extra[kMaxExtra * 3] = {};
    bool operator==(const Point &) const = default;

    int sampleCount() const { return 1 + extraN; }
    /* Sample i (0 = the first, r/g/b) as sRGB-encoded rgb. */
    void sample(int i, float &sr, float &sg, float &sb) const {
        if (i <= 0 || i > extraN) { sr = r; sg = g; sb = b; return; }
        const float *e = extra + 3 * (i - 1);
        sr = e[0]; sg = e[1]; sb = e[2];
    }
    bool addSample(float sr, float sg, float sb) {
        if (extraN >= kMaxExtra) return false;
        float *e = extra + 3 * extraN++;
        e[0] = sr; e[1] = sg; e[2] = sb;
        return true;
    }
    void removeLastSample() {
        if (extraN <= 0) return;
        --extraN;
        float *e = extra + 3 * extraN;
        e[0] = e[1] = e[2] = 0.0f;
    }
    void clearExtraSamples() {
        extraN = 0;
        for (float &v : extra) v = 0.0f;
    }

    /* A point that moves nothing. Its ranges do not matter until something moves. */
    bool isInert() const {
        return hueShift == 0.0f && satShift == 0.0f && lumShift == 0.0f &&
               variance == 0.0f;
    }
};

/* ---- Tuning ----------------------------------------------------------------------- */

constexpr float kHueShiftMaxDeg = 60.0f;    // Hue Shift +/-100
constexpr float kSatUpMax       = 1.0f;     // Sat Shift +100 = chroma x (1 + this)
constexpr float kLumMax         = 0.5f;     // Lum Shift +/-100: fraction of the way
                                            //   toward white / black in OKLab L
constexpr float kHueWidthDeg    = 30.0f;    // half-width at every range = 50
constexpr float kSatWidth       = 0.75f;    // relative chroma half-width (dC / Cs)
constexpr float kSatWidthMinC   = 0.03f;    // floor for Cs in that ratio
constexpr float kLumWidth       = 0.30f;    // L half-width
constexpr float kGateC0         = 0.008f;   // neutral gate: no weight below this C
constexpr float kGateC1         = 0.030f;   //   full weight above this C
constexpr float kPi             = 3.14159265358979f;

inline float qMinF(float a, float b) { return a < b ? a : b; }

/* A range slider (0..100, 50 = nominal) as a width multiplier, x0.25 .. x4. */
inline float rangeScale(float v) { return std::exp2((v - 50.0f) / 25.0f); }

/* ---- OKLab ------------------------------------------------------------------------
   Björn Ottosson's OKLab, from LINEAR sRGB. The pipeline hands us display-linear
   values in the working (Rec.2020) primaries, so the forward matrix folds Rec.2020 ->
   sRGB in front of OKLab's M1, and the inverse folds sRGB -> Rec.2020 behind M1^-1. */

constexpr float kM1[9] = {0.4122214708f, 0.5363325363f, 0.0514459929f,
                          0.2119034982f, 0.6806995451f, 0.1073969566f,
                          0.0883024619f, 0.2817188376f, 0.6299787005f};
constexpr float kM2[9] = {0.2104542553f, 0.7936177850f, -0.0040720468f,
                          1.9779984951f, -2.4285922050f, 0.4505937099f,
                          0.0259040371f, 0.7827717662f, -0.8086757660f};
constexpr float kM2Inv[9] = {1.0f, 0.3963377774f, 0.2158037573f,
                             1.0f, -0.1055613458f, -0.0638541728f,
                             1.0f, -0.0894841775f, -1.2914855480f};
constexpr float kM1Inv[9] = {4.0767416621f, -3.3077115913f, 0.2309699292f,
                             -1.2684380046f, 2.6097574011f, -0.3413193965f,
                             -0.0041960863f, -0.7034186147f, 1.7076147010f};

inline void mul3(const float m[9], const float in[3], float out[3])
{
    out[0] = m[0] * in[0] + m[1] * in[1] + m[2] * in[2];
    out[1] = m[3] * in[0] + m[4] * in[1] + m[5] * in[2];
    out[2] = m[6] * in[0] + m[7] * in[1] + m[8] * in[2];
}

/* The working space's display-linear RGB <-> LMS, with the gamut change folded in.
   Built once per buildPointCoeffs (nine multiplies of a constexpr pair). */
inline void workingToLms(float out[9])
{
    float w2s[3][3];
    ColorSpaceMath::matrixF(ColorSpaceMath::kWorking, ColorSpaceMath::ColorSpace::LinearSRGB,
                            w2s);
    for (int r = 0; r < 3; ++r)
        for (int c = 0; c < 3; ++c)
            out[3 * r + c] = kM1[3 * r] * w2s[0][c] + kM1[3 * r + 1] * w2s[1][c]
                           + kM1[3 * r + 2] * w2s[2][c];
}
inline void lmsToWorking(float out[9])
{
    float s2w[3][3];
    ColorSpaceMath::matrixF(ColorSpaceMath::ColorSpace::LinearSRGB, ColorSpaceMath::kWorking,
                            s2w);
    for (int r = 0; r < 3; ++r)
        for (int c = 0; c < 3; ++c)
            out[3 * r + c] = s2w[r][0] * kM1Inv[c] + s2w[r][1] * kM1Inv[3 + c]
                           + s2w[r][2] * kM1Inv[6 + c];
}

/* LINEAR rgb (through the given rgb->LMS matrix) -> OKLab. */
inline void toLab(const float toLms[9], const float rgb[3], float lab[3])
{
    float lms[3];
    mul3(toLms, rgb, lms);
    for (int k = 0; k < 3; ++k) lms[k] = std::cbrt(lms[k]);
    mul3(kM2, lms, lab);
}
/* OKLab -> LINEAR rgb (through the given LMS->rgb matrix). */
inline void fromLab(const float fromLms[9], const float lab[3], float rgb[3])
{
    float lms[3];
    mul3(kM2Inv, lab, lms);
    for (int k = 0; k < 3; ++k) lms[k] = lms[k] * lms[k] * lms[k];
    mul3(fromLms, lms, rgb);
}

inline float srgbDecode(float e)
{
    return e <= 0.04045f ? e / 12.92f : std::pow((e + 0.055f) / 1.055f, 2.4f);
}
inline float srgbEncode(float v)
{
    if (v <= 0.0f) return 0.0f;
    if (v >= 1.0f) return 1.0f;
    return v <= 0.0031308f ? v * 12.92f : 1.055f * std::pow(v, 1.0f / 2.4f) - 0.055f;
}

/* An sRGB-encoded colour as OKLCh: L, C, h (radians). */
inline void rgbLch(float r, float g, float b, float &L, float &C, float &h)
{
    const float lin[3] = {srgbDecode(r), srgbDecode(g), srgbDecode(b)};
    float lab[3];
    toLab(kM1, lin, lab);
    L = lab[0];
    C = std::hypot(lab[1], lab[2]);
    h = std::atan2(lab[2], lab[1]);
}

/* The FIRST sample (sRGB-encoded) as OKLCh: L, C, h (radians). */
inline void sampleLch(const Point &p, float &L, float &C, float &h)
{
    rgbLch(p.r, p.g, p.b, L, C, h);
}

inline float wrapPi(float a);

/* The region every sample of a point spans in OKLCh (see MULTIPLE SAMPLES). hc +/- hh
   is the shortest hue arc covering the chromatic samples; hueGate says there is at
   least one (otherwise hue does not select at all, as for a neutral single sample). */
struct Envelope {
    float Lmin = 0.0f, Lmax = 0.0f;
    float Cmin = 0.0f, Cmax = 0.0f;
    float hc = 0.0f, hh = 0.0f;
    bool  hueGate = false;
    float Lc() const { return 0.5f * (Lmin + Lmax); }
    float Cc() const { return 0.5f * (Cmin + Cmax); }
};

inline Envelope envelope(const Point &p)
{
    Envelope e;
    const int n = p.sampleCount();
    float hues[kMaxSamples];
    int nh = 0;
    for (int i = 0; i < n; ++i) {
        float r, g, b, L, C, h;
        p.sample(i, r, g, b);
        rgbLch(r, g, b, L, C, h);
        if (i == 0) { e.Lmin = e.Lmax = L; e.Cmin = e.Cmax = C; }
        else {
            e.Lmin = L < e.Lmin ? L : e.Lmin;  e.Lmax = L > e.Lmax ? L : e.Lmax;
            e.Cmin = C < e.Cmin ? C : e.Cmin;  e.Cmax = C > e.Cmax ? C : e.Cmax;
        }
        if (C >= kGateC1) hues[nh++] = h;   // a hue worth selecting on
    }
    if (nh == 0) {
        /* No chromatic sample: hue never gates, but keep the first sample's hue as the
           centre so Variance and the panel's field still have one. */
        float L, C;
        sampleLch(p, L, C, e.hc);
        return e;
    }
    e.hueGate = true;
    if (nh == 1) { e.hc = hues[0]; return e; }
    /* The shortest covering arc is the complement of the largest gap between
       neighbouring hues around the circle. */
    for (int i = 1; i < nh; ++i)                       // insertion sort, nh <= 8
        for (int j = i; j > 0 && hues[j] < hues[j - 1]; --j) {
            const float t = hues[j]; hues[j] = hues[j - 1]; hues[j - 1] = t;
        }
    int gapAfter = nh - 1;
    float gap = hues[0] + 2.0f * kPi - hues[nh - 1];
    for (int i = 0; i + 1 < nh; ++i)
        if (hues[i + 1] - hues[i] > gap) { gap = hues[i + 1] - hues[i]; gapAfter = i; }
    const float start = hues[(gapAfter + 1) % nh];     // arc runs start -> end, CCW
    const float span = 2.0f * kPi - gap;
    e.hh = 0.5f * span;
    e.hc = wrapPi(start + e.hh);
    return e;
}

/* OKLCh -> sRGB-encoded, clipped -- for the panel's swatches and field. */
inline void lchToSrgb(float L, float C, float h, float &r, float &g, float &b)
{
    const float lab[3] = {L, C * std::cos(h), C * std::sin(h)};
    float lin[3];
    fromLab(kM1Inv, lab, lin);
    r = srgbEncode(lin[0]);
    g = srgbEncode(lin[1]);
    b = srgbEncode(lin[2]);
}

/* ---- The adjustment, per point ---------------------------------------------------- */

inline float wrapPi(float a)
{
    while (a >  kPi) a -= 2.0f * kPi;
    while (a < -kPi) a += 2.0f * kPi;
    return a;
}

/* Where Hue / Sat / Lum Shift take a colour (L, C, h). Variance is NOT applied here: it
   acts on a pixel's distance from the sample, so it has no effect on the sample itself.
   Shared by the render and the panel's target dot and after-swatch. */
inline float shiftedHue(const Point &p, float h)
{
    return wrapPi(h + p.hueShift / 100.0f * kHueShiftMaxDeg * kPi / 180.0f);
}
inline float shiftedChroma(const Point &p, float C)
{
    const float s = p.satShift / 100.0f;
    return C * (s >= 0.0f ? 1.0f + s * kSatUpMax : 1.0f + s);
}
inline float shiftedLightness(const Point &p, float L)
{
    const float l = p.lumShift / 100.0f * kLumMax;
    return l >= 0.0f ? L + (1.0f - L) * l : L * (1.0f + l);
}

/* The inverses, for dragging the panel's target dot: the slider value that moves the
   sample's own (C, h, L) to the dragged one, clamped to the slider range. */
inline float hueShiftFor(float hs, float ht)
{
    const float v = wrapPi(ht - hs) * 180.0f / kPi / kHueShiftMaxDeg * 100.0f;
    return v < -100.0f ? -100.0f : (v > 100.0f ? 100.0f : v);
}
inline float satShiftFor(float Cs, float Ct)
{
    if (Cs <= 1e-6f) return 0.0f;
    const float k = Ct / Cs;
    const float v = (k >= 1.0f ? (k - 1.0f) / kSatUpMax : k - 1.0f) * 100.0f;
    return v < -100.0f ? -100.0f : (v > 100.0f ? 100.0f : v);
}
inline float lumShiftFor(float Ls, float Lt)
{
    float l;
    if (Lt >= Ls) l = (Ls < 1.0f) ? (Lt - Ls) / (1.0f - Ls) : 0.0f;
    else          l = (Ls > 0.0f) ? Lt / Ls - 1.0f : 0.0f;
    const float v = l / kLumMax * 100.0f;
    return v < -100.0f ? -100.0f : (v > 100.0f ? 100.0f : v);
}

/* Everything the hot loop needs for one point, precomputed. The selection is measured
   from the envelope [Lmin,Lmax] x [Cmin,Cmax] x hs +/- hh (all collapse to the sample
   for a one-sample point); Cs is the chroma Variance pivots about. */
struct Coef {
    float Ls = 0.0f, Cs = 0.0f, hs = 0.0f;
    float Lmin = 0.0f, Lmax = 0.0f, Cmin = 0.0f, Cmax = 0.0f, hh = 0.0f;
    float invHueW = 0.0f, invSatW = 0.0f, invLumW = 0.0f;
    float satRefLo = 1.0f;       // the chroma a distance below Cmin is relative to
    float satRefHi = 1.0f;       //   and above Cmax
    bool  hueGate = true;        // some sample has a hue worth selecting on
    float hueRot = 0.0f;         // radians
    float satMul = 1.0f;
    float lum = 0.0f;            // signed fraction, see shiftedLightness
    float vf = 1.0f;             // variance factor on the distance from the sample
};

inline Coef makeCoef(const Point &p)
{
    Coef k;
    const Envelope e = envelope(p);
    k.Lmin = e.Lmin; k.Lmax = e.Lmax;
    k.Cmin = e.Cmin; k.Cmax = e.Cmax;
    k.Ls = e.Lc(); k.Cs = e.Cc(); k.hs = e.hc; k.hh = e.hh;
    const float all = rangeScale(p.range);
    const float hw = qMinF(kHueWidthDeg * all * rangeScale(p.hueRange), 180.0f);
    k.invHueW = 1.0f / (hw * kPi / 180.0f);
    k.satRefLo = k.Cmin > kSatWidthMinC ? k.Cmin : kSatWidthMinC;
    k.satRefHi = k.Cmax > kSatWidthMinC ? k.Cmax : kSatWidthMinC;
    k.invSatW = 1.0f / (kSatWidth * all * rangeScale(p.satRange));
    k.invLumW = 1.0f / (kLumWidth * all * rangeScale(p.lumRange));
    k.hueGate = e.hueGate;
    k.hueRot = p.hueShift / 100.0f * kHueShiftMaxDeg * kPi / 180.0f;
    const float s = p.satShift / 100.0f;
    k.satMul = s >= 0.0f ? 1.0f + s * kSatUpMax : 1.0f + s;
    k.lum = p.lumShift / 100.0f * kLumMax;
    k.vf = 1.0f + p.variance / 100.0f;
    return k;
}

/* The compact falloff: 1 at t = 0, 0 (with zero slope) at |t| >= 1. */
inline float falloff(float t)
{
    t = t < 0.0f ? -t : t;
    if (t >= 1.0f) return 0.0f;
    const float u = 1.0f - t * t;
    return u * u;
}

/* How much the pixel (L, C, h) belongs to the point: full inside the envelope, the
   falloff on the distance beyond its edge. */
inline float weight(const Coef &k, float L, float C, float h)
{
    const float dl = L < k.Lmin ? k.Lmin - L : (L > k.Lmax ? L - k.Lmax : 0.0f);
    const float wl = falloff(dl * k.invLumW);
    if (wl <= 0.0f) return 0.0f;
    const float dc = C < k.Cmin ? (k.Cmin - C) / k.satRefLo
                                : (C > k.Cmax ? (C - k.Cmax) / k.satRefHi : 0.0f);
    const float ws = falloff(dc * k.invSatW);
    if (ws <= 0.0f) return 0.0f;
    float wh = 1.0f;
    if (k.hueGate) {
        float dh = wrapPi(h - k.hs);
        dh = dh < 0.0f ? -dh : dh;
        dh = dh > k.hh ? dh - k.hh : 0.0f;
        wh = falloff(dh * k.invHueW);
        if (wh <= 0.0f) return 0.0f;
        /* A near-neutral pixel's hue is noise: fade it out rather than let it flicker
           in and out of a hue window. */
        float g = (C - kGateC0) / (kGateC1 - kGateC0);
        g = g < 0.0f ? 0.0f : (g > 1.0f ? 1.0f : g);
        wh *= g * g * (3.0f - 2.0f * g);
    }
    return wl * ws * wh;
}

/* Add the point's weighted change of pixel (L, a, b) into d[3]; returns the weight. */
inline float accumulate(const Coef &k, const float lab[3], float C, float h, float d[3])
{
    const float w = weight(k, lab[0], C, h);
    if (w <= 0.0f) return 0.0f;
    /* Variance about the sample, in hue and chroma. */
    float ht = h, Ct = C;
    if (k.vf != 1.0f) {
        if (k.hueGate) ht = k.hs + wrapPi(h - k.hs) * k.vf;
        Ct = k.Cs + (C - k.Cs) * k.vf;
        if (Ct < 0.0f) Ct = 0.0f;
    }
    ht += k.hueRot;
    Ct *= k.satMul;
    const float L = lab[0];
    const float Lt = k.lum >= 0.0f ? L + (1.0f - L) * k.lum : L * (1.0f + k.lum);
    d[0] += w * (Lt - L);
    d[1] += w * (Ct * std::cos(ht) - lab[1]);
    d[2] += w * (Ct * std::sin(ht) - lab[2]);
    return w;
}

/* ---- The shared encoding ----------------------------------------------------------
   "r,g,b,hue,sat,lum,var,range,hueR,satR,lumR[,r2,g2,b2...];..." one group per point,
   in order; a point's added samples follow its eleven fields as rgb triples (none for a
   one-sample point, so its group is unchanged). An empty string is no points, so an
   untouched image carries nothing. The sidecar
   (EditStack::paramsToJson), the preset collector and DevelopPresets::assignParam all
   go through this pair -- see the develop-adjustment checklist. */
constexpr int kFields = 11;

inline QString encode(int n, const Point *pts)
{
    QStringList out;
    for (int i = 0; i < n && i < kMaxPts; ++i) {
        const Point &p = pts[i];
        const float v[kFields] = {p.r, p.g, p.b, p.hueShift, p.satShift, p.lumShift,
                                  p.variance, p.range, p.hueRange, p.satRange, p.lumRange};
        QStringList f;
        for (float x : v) f << QString::number(x, 'g', 5);
        for (int k = 0; k < 3 * p.extraN && k < 3 * kMaxExtra; ++k)
            f << QString::number(p.extra[k], 'g', 5);
        out << f.join(",");
    }
    return out.join(";");
}

/* Clamp n into 0..kMaxPts, every live value into its slider range (a NaN goes to the
   field's default), and reset the tail past n to defaults so EditParams::operator==
   never sees stale data. Returns true if anything changed. */
inline bool sanitize(int &n, Point *pts)
{
    bool changed = false;
    if (n < 0 || n > kMaxPts) { n = n < 0 ? 0 : kMaxPts; changed = true; }
    const Point def;
    auto fix = [&changed](float &v, float lo, float hi, float d) {
        if (!(v == v)) { v = d; changed = true; return; }
        if (v < lo) { v = lo; changed = true; }
        else if (v > hi) { v = hi; changed = true; }
    };
    for (int i = 0; i < n; ++i) {
        Point &p = pts[i];
        fix(p.r, 0.0f, 1.0f, 0.0f);
        fix(p.g, 0.0f, 1.0f, 0.0f);
        fix(p.b, 0.0f, 1.0f, 0.0f);
        fix(p.hueShift, -100.0f, 100.0f, def.hueShift);
        fix(p.satShift, -100.0f, 100.0f, def.satShift);
        fix(p.lumShift, -100.0f, 100.0f, def.lumShift);
        fix(p.variance, -100.0f, 100.0f, def.variance);
        fix(p.range,    0.0f, 100.0f, def.range);
        fix(p.hueRange, 0.0f, 100.0f, def.hueRange);
        fix(p.satRange, 0.0f, 100.0f, def.satRange);
        fix(p.lumRange, 0.0f, 100.0f, def.lumRange);
        if (p.extraN < 0 || p.extraN > kMaxExtra) {
            p.extraN = p.extraN < 0 ? 0 : kMaxExtra;
            changed = true;
        }
        for (int k = 0; k < 3 * kMaxExtra; ++k) {
            if (k < 3 * p.extraN) fix(p.extra[k], 0.0f, 1.0f, 0.0f);
            else if (p.extra[k] != 0.0f) { p.extra[k] = 0.0f; changed = true; }
        }
    }
    for (int i = n; i < kMaxPts; ++i)
        if (!(pts[i] == def)) { pts[i] = def; changed = true; }
    return changed;
}

/* Fills ALL kMaxPts slots (the tail is reset to defaults). Returns false if anything
   was malformed; a malformed group is dropped, and values are clamped by sanitize. */
inline bool decode(const QString &s, int &n, Point *pts)
{
    n = 0;
    for (int i = 0; i < kMaxPts; ++i) pts[i] = Point();
    if (s.isEmpty()) return true;
    bool ok = true;
    for (const QString &grp : s.split(';', Qt::SkipEmptyParts)) {
        if (n >= kMaxPts) { ok = false; break; }
        const QStringList f = grp.split(',');
        const int extraFields = int(f.size()) - kFields;
        if (extraFields < 0 || extraFields % 3 != 0) { ok = false; continue; }
        float v[kFields];
        bool good = true;
        for (int i = 0; i < kFields && good; ++i) v[i] = f.at(i).toFloat(&good);
        if (!good) { ok = false; continue; }
        float ex[3 * kMaxExtra] = {};
        int exN = extraFields / 3;
        if (exN > kMaxExtra) { exN = kMaxExtra; ok = false; }
        for (int i = 0; i < 3 * exN && good; ++i) ex[i] = f.at(kFields + i).toFloat(&good);
        if (!good) { ok = false; continue; }
        Point &p = pts[n++];
        p.r = v[0]; p.g = v[1]; p.b = v[2];
        p.hueShift = v[3]; p.satShift = v[4]; p.lumShift = v[5]; p.variance = v[6];
        p.range = v[7]; p.hueRange = v[8]; p.satRange = v[9]; p.lumRange = v[10];
        p.extraN = exN;
        for (int i = 0; i < 3 * exN; ++i) p.extra[i] = ex[i];
    }
    if (sanitize(n, pts)) ok = false;
    return ok;
}

/* True when no point moves anything (an image whose samples are all at zero shift is
   still identity -- the samples alone change no pixel). */
inline bool isIdentity(int n, const Point *pts)
{
    for (int i = 0; i < n && i < kMaxPts; ++i)
        if (!pts[i].isInert()) return false;
    return true;
}

} // namespace PointColor

#endif // POINTCOLOR_H
