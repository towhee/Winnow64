#ifndef COLORMIXER_H
#define COLORMIXER_H

/*
    COLOR MIXER -- Lightroom's Color Mixer > Mixer (Adjust: HSL), the Color panel's
    "Mix" mode.

    Eight fixed hue bands (Red, Orange, Yellow, Green, Aqua, Blue, Purple, Magenta),
    each with a Hue, Saturation and Luminance slider (-100..100, 0 = no change). Unlike
    Point Color, a band selects by HUE ALONE: every pixel of that hue moves, whatever
    its lightness or saturation (near-neutrals excepted, whose hue is noise).

    THE BANDS. Lightroom names its bands by the HSV hue of a fully saturated colour (Red
    0, Orange 30, Yellow 60, Green 120, Aqua 180, Blue 240, Purple 270, Magenta 300
    degrees). Those centres are converted ONCE to OKLab hue (bandHue) -- the space the
    work is done in -- so a band still sits on the colour its name says, while the
    adjustment is perceptually even across it.

    THE BLEND. A pixel between two neighbouring centres takes a raised-cosine mix of
    their two slider values, so the eight weights always sum to one (moving every
    band by the same amount moves every hue by that amount) and there is no seam. The
    three per-hue results are baked into tables of kHueN bins (buildTables); the hot
    loop does one atan2 and three interpolated lookups.

    THE ADJUSTMENT, in OKLCh of the display-linear value:
      Hue         rotates the hue toward the neighbouring band: +100 moves it
                  kHueReach of the way to the next centre (Red -> Orange), -100 to the
                  previous one (Red -> Magenta).
      Saturation  scales chroma: -100 = grey, +100 = x(1 + kSatUpMax).
      Luminance   moves L toward white (+) or black (-) by up to kLumMax of the way.
    All three are faded in by chroma (gate) so greys and near-greys stay put.

    WHERE IT RUNS: the fused point pass, after HSL and BEFORE Point Color, on the same
    display axis (gradeFwd / gradeInv) and in the same OKLab as Point Color -- so a Point
    Color sample taken off the displayed image sees the mixed colour, as in Lightroom.

    NOT YET CALIBRATED against Lightroom: kHueReach, kSatUpMax, kLumMax and the gate are
    first estimates (the Point Color tuning was the starting point).

    Header-only, no Qt beyond QString for the key names.
*/

#include <cmath>
#include <QString>
#include "Develop/pointcolor.h"

namespace ColorMixer {

constexpr int kBands = 8;
constexpr int kKinds = 3;                   // Hue, Saturation, Luminance
enum Kind { Hue = 0, Sat = 1, Lum = 2 };

/* Band names: the panel captions AND the middle of the EditParams field / sidecar key
   ("mixRedHue"), so they are a published format -- never rename. */
inline const char *bandName(int band)
{
    static const char *k[kBands] = {"Red", "Orange", "Yellow", "Green",
                                    "Aqua", "Blue", "Purple", "Magenta"};
    return k[band];
}
inline const char *kindName(int kind)
{
    static const char *k[kKinds] = {"Hue", "Sat", "Lum"};
    return k[kind];
}
/* The EditParams field name, which is also the sidecar / preset key and the panel's
   slider key: "mix" + band + kind, e.g. "mixRedHue", "mixAquaSat". */
inline QString key(int kind, int band)
{
    return QStringLiteral("mix") + QLatin1String(bandName(band)) +
           QLatin1String(kindName(kind));
}

/* Lightroom's band centres, HSV hue of the saturated colour, degrees. */
constexpr float kHsvCentreDeg[kBands] = {0, 30, 60, 120, 180, 240, 270, 300};

/* ---- Tuning ----------------------------------------------------------------------- */

constexpr float kHueReach = 0.6f;           // Hue +/-100: fraction of the gap to the
                                            //   neighbouring band's centre
constexpr float kSatUpMax = 1.0f;           // Saturation +100 = chroma x (1 + this)
constexpr float kLumMax   = 0.5f;           // Luminance +/-100: fraction of the way
                                            //   toward white / black in OKLab L
constexpr float kGateC0   = 0.010f;         // chroma gate: no effect below this C
constexpr float kGateC1   = 0.050f;         //   full effect above this C
constexpr int   kHueN     = 360;            // table bins over the hue circle
constexpr float kPi       = PointColor::kPi;

/* HSV(h, 1, 1) as sRGB-encoded rgb. */
inline void hsvRgb(float hDeg, float &r, float &g, float &b)
{
    const float h = std::fmod(hDeg, 360.0f) / 60.0f;
    const float x = 1.0f - std::fabs(std::fmod(h, 2.0f) - 1.0f);
    r = g = b = 0.0f;
    switch (static_cast<int>(h)) {
    case 0:  r = 1; g = x; break;
    case 1:  r = x; g = 1; break;
    case 2:  g = 1; b = x; break;
    case 3:  g = x; b = 1; break;
    case 4:  r = x; b = 1; break;
    default: r = 1; b = x; break;
    }
}

/* Each band's centre as an OKLab hue, radians in [0, 2pi), ascending. */
inline void bandHues(float out[kBands])
{
    for (int i = 0; i < kBands; ++i) {
        float r, g, b, L, C, h;
        hsvRgb(kHsvCentreDeg[i], r, g, b);
        PointColor::rgbLch(r, g, b, L, C, h);
        out[i] = h < 0.0f ? h + 2.0f * kPi : h;
    }
}

/* Where hue h (radians, any range) sits between the band centres: the band at or
   below it (lo), the next one up (hi, wrapping), and the raised-cosine weight of hi. */
inline void bracket(const float centre[kBands], float h, int &lo, int &hi, float &w)
{
    h = std::fmod(h, 2.0f * kPi);
    if (h < 0.0f) h += 2.0f * kPi;
    lo = kBands - 1;                         // below the first centre: wrap from the last
    for (int i = 0; i < kBands; ++i)
        if (centre[i] <= h) lo = i;
    hi = (lo + 1) % kBands;
    float span = centre[hi] - centre[lo];
    float d = h - centre[lo];
    if (span <= 0.0f) span += 2.0f * kPi;
    if (d < 0.0f) d += 2.0f * kPi;
    const float t = span > 0.0f ? d / span : 0.0f;
    w = 0.5f - 0.5f * std::cos(kPi * (t < 0.0f ? 0.0f : (t > 1.0f ? 1.0f : t)));
}

/* Signed angular gap from centre a to centre b, going UP (b after a), radians. */
inline float gapUp(const float centre[kBands], int a)
{
    float g = centre[(a + 1) % kBands] - centre[a];
    return g <= 0.0f ? g + 2.0f * kPi : g;
}

inline bool isIdentity(const float hue[kBands], const float sat[kBands],
                       const float lum[kBands])
{
    for (int i = 0; i < kBands; ++i)
        if (hue[i] != 0.0f || sat[i] != 0.0f || lum[i] != 0.0f) return false;
    return true;
}

/* Bake the per-hue tables, bin k centred on hue (k + 0.5) / kHueN * 2pi:
     rot  hue rotation, radians
     sat  chroma multiplier
     lum  signed lightness fraction (see adjust) */
inline void buildTables(const float hue[kBands], const float sat[kBands],
                        const float lum[kBands], float rot[kHueN], float satMul[kHueN],
                        float lumF[kHueN])
{
    float centre[kBands];
    bandHues(centre);
    /* Per band: its hue slider as a rotation, toward the next centre (+) or the
       previous one (-), and its saturation slider as a multiplier. */
    float bRot[kBands], bSat[kBands], bLum[kBands];
    for (int i = 0; i < kBands; ++i) {
        const float v = hue[i] / 100.0f;
        const float gap = v >= 0.0f ? gapUp(centre, i)
                                    : gapUp(centre, (i + kBands - 1) % kBands);
        bRot[i] = v * kHueReach * gap;
        const float s = sat[i] / 100.0f;
        bSat[i] = s >= 0.0f ? 1.0f + s * kSatUpMax : 1.0f + s;
        bLum[i] = lum[i] / 100.0f * kLumMax;
    }
    for (int k = 0; k < kHueN; ++k) {
        const float h = (k + 0.5f) / kHueN * 2.0f * kPi;
        int lo, hi;
        float w;
        bracket(centre, h, lo, hi, w);
        rot[k]    = bRot[lo] + (bRot[hi] - bRot[lo]) * w;
        satMul[k] = bSat[lo] + (bSat[hi] - bSat[lo]) * w;
        lumF[k]   = bLum[lo] + (bLum[hi] - bLum[lo]) * w;
    }
}

/* Read a hue table at h (radians, from atan2), linear between bins, wrapping. */
inline float lookup(const float t[kHueN], float h)
{
    float f = h / (2.0f * kPi) * kHueN - 0.5f;
    if (f < 0.0f) f += kHueN;
    int i = static_cast<int>(f);
    const float u = f - i;
    i %= kHueN;
    const int j = (i + 1) % kHueN;
    return t[i] + (t[j] - t[i]) * u;
}

/* The chroma gate: 0 at a neutral, 1 once the colour is clearly a colour. */
inline float gate(float C)
{
    float g = (C - kGateC0) / (kGateC1 - kGateC0);
    g = g < 0.0f ? 0.0f : (g > 1.0f ? 1.0f : g);
    return g * g * (3.0f - 2.0f * g);
}

/* Mix one OKLab colour in place through the baked tables. False = untouched (a
   neutral), so the caller can skip the trip back. */
inline bool adjust(const float rot[kHueN], const float satMul[kHueN],
                   const float lumF[kHueN], float lab[3])
{
    const float C = std::sqrt(lab[1] * lab[1] + lab[2] * lab[2]);
    const float g = gate(C);
    if (g <= 0.0f) return false;
    const float h = std::atan2(lab[2], lab[1]);
    const float r = lookup(rot, h) * g;
    const float s = 1.0f + (lookup(satMul, h) - 1.0f) * g;
    const float l = lookup(lumF, h) * g;
    const float L = lab[0];
    lab[0] = l >= 0.0f ? L + (1.0f - L) * l : L * (1.0f + l);
    const float cs = std::cos(r), sn = std::sin(r);
    const float a = lab[1], b = lab[2];
    lab[1] = (a * cs - b * sn) * s;
    lab[2] = (a * sn + b * cs) * s;
    return true;
}

}   // namespace ColorMixer

#endif // COLORMIXER_H
