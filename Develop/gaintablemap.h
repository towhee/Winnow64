#ifndef GAINTABLEMAP_H
#define GAINTABLEMAP_H

#include <QByteArray>
#include <QtGlobal>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <vector>

/*
    DNG PROFILE GAIN TABLE MAP -- a spatially varying tone map stored in the file.

    DNG 1.6 tag ProfileGainTableMap (52525, Raw IFD) and DNG 1.7's ProfileGainTableMap2
    (52544, IFD 0). iPhone ProRAW carries one: it is how Apple's "local tone mapping"
    travels with the raw. The file's BaselineExposure (+5.16 EV on an iPhone 17 Pro) is
    the other half of it -- the gains fall from ~0.6 in the shadows to 2^-5.16 at white,
    exactly undoing the lift there -- so applying either without the other renders the
    picture blown out (BaselineExposure alone) or flat and dark (neither).

    WHAT THE SPEC SAYS (DNG 1.7.1.0, pp. 71-73 and 86-88), and this implements:
      - a MapPointsV x MapPointsH grid of 1-D gain tables, MapPointsN entries each, at
        relative positions origin + k * spacing (1.0 = the image's height / width), with
        PIXEL-CENTRED sampling and edge replication outside the grid;
      - the table input t = clamp(dot((R, G, B, min, max), MapInputWeights), 0, 1) on
        LINEAR RIMM (linear ProPhoto, D50) values AFTER BaselineExposure; ^Gamma for the
        version-2 tag;
      - table index = t * MapPointsN, linearly interpolated, tables bilinearly blended;
      - the gain multiplies all three channels, and the result may exceed 1 (kept).

    VERIFIED 2026-11-09 against Core Image's own application (CIRAWFilter local tone map
    on vs off, an iPhone 17 Pro DNG): shadows agree to ~0.05 EV, which pins the
    coordinate convention (rows top-down), the colour space and the index. t * N versus
    t * (N - 1) measured indistinguishable. Apple departs from the bare spec above the
    shadows by an amount that depends on its EDR setting, so Apple is not a bit-exact
    reference for the mid-tones.

    Header-only: Develop applies it (Develop::ApplyGainTableMap), the DNG reader parses
    it, and neither the unit-test closure nor the app gains a translation unit.
*/
struct GainTableMap {
    int pointsV = 0;
    int pointsH = 0;
    int pointsN = 0;
    double spacingV = 0.0;
    double spacingH = 0.0;
    double originV = 0.0;
    double originH = 0.0;
    float weights[5] = {0, 0, 0, 0, 0};     // R, G, B, min(R,G,B), max(R,G,B)
    float gamma = 1.0f;                     // version 2 only; 1 = NOP
    std::vector<float> gains;               // pointsV * pointsH * pointsN, row-major

    bool isValid() const {
        return pointsV > 0 && pointsH > 0 && pointsN > 1 &&
               gains.size() == size_t(pointsV) * size_t(pointsH) * size_t(pointsN);
    }

    /*
        Parse the tag's bytes. bigEndian is the FILE's byte order (the tag is UNDEFINED,
        so it is stored in the byte order of the file that carries it). v2 selects the
        ProfileGainTableMap2 layout: DataType, Gamma, GainMin, GainMax after the weights,
        and gains as uint8 / uint16 (scaled into GainMin..GainMax), float16 or float32.
        Returns false on anything malformed: a bad map is ignored, not half-applied.
    */
    static bool Parse(const QByteArray &d, bool bigEndian, bool v2, GainTableMap &out)
    {
        out = GainTableMap();
        const uchar *p = reinterpret_cast<const uchar *>(d.constData());
        const qsizetype n = d.size();
        auto u32 = [&](qsizetype o) -> quint32 {
            return bigEndian ? (quint32(p[o]) << 24 | quint32(p[o + 1]) << 16 |
                                quint32(p[o + 2]) << 8 | quint32(p[o + 3]))
                             : (quint32(p[o + 3]) << 24 | quint32(p[o + 2]) << 16 |
                                quint32(p[o + 1]) << 8 | quint32(p[o]));
        };
        auto u16 = [&](qsizetype o) -> quint16 {
            return bigEndian ? quint16(p[o] << 8 | p[o + 1]) : quint16(p[o + 1] << 8 | p[o]);
        };
        auto f32 = [&](qsizetype o) { const quint32 b = u32(o); float f; std::memcpy(&f, &b, 4); return f; };
        auto f64 = [&](qsizetype o) {
            const quint64 hi = u32(bigEndian ? o : o + 4), lo = u32(bigEndian ? o + 4 : o);
            const quint64 b = hi << 32 | lo;
            double v; std::memcpy(&v, &b, 8); return v;
        };
        const qsizetype header = v2 ? 80 : 64;
        if (n < header) return false;

        out.pointsV = int(u32(0));
        out.pointsH = int(u32(4));
        out.spacingV = f64(8);
        out.spacingH = f64(16);
        out.originV = f64(24);
        out.originH = f64(32);
        out.pointsN = int(u32(40));
        for (int i = 0; i < 5; ++i) out.weights[i] = f32(44 + 4 * i);
        if (out.pointsV <= 0 || out.pointsH <= 0 || out.pointsN <= 1 ||
            out.pointsV > 4096 || out.pointsH > 4096 || out.pointsN > 65536) return false;
        if ((out.pointsV > 1 && !(out.spacingV > 0.0)) ||
            (out.pointsH > 1 && !(out.spacingH > 0.0))) return false;

        int dataType = 3;                   // version 1: always 32-bit float
        float gainMin = 0.0f, gainMax = 1.0f;
        if (v2) {
            dataType = int(u32(64));
            out.gamma = f32(68);
            gainMin = f32(72);
            gainMax = f32(76);
            if (dataType < 0 || dataType > 3) return false;
            if (!(out.gamma >= 0.25f && out.gamma <= 4.0f)) return false;
        }
        const int bytes = dataType == 0 ? 1 : dataType == 3 ? 4 : 2;
        const size_t count = size_t(out.pointsV) * size_t(out.pointsH) * size_t(out.pointsN);
        if (qsizetype(header + count * size_t(bytes)) > n) return false;

        out.gains.resize(count);
        for (size_t i = 0; i < count; ++i) {
            const qsizetype o = header + qsizetype(i) * bytes;
            float g = 0.0f;
            switch (dataType) {
            case 0: g = gainMin + (p[o] / 255.0f) * (gainMax - gainMin); break;
            case 1: g = gainMin + (u16(o) / 65535.0f) * (gainMax - gainMin); break;
            case 2: g = halfToFloat(u16(o)); break;
            default: g = f32(o); break;
            }
            if (!std::isfinite(g) || g < 0.0f) g = 0.0f;    // "must be non-negative"
            out.gains[i] = g;
        }
        return true;
    }

    /* One axis of the grid: a pixel-centred relative position -> the lower grid index
       and the weight of the next one, with edge replication. */
    static void Axis(double rel, double origin, double spacing, int points, int &i0, float &w)
    {
        if (points <= 1) { i0 = 0; w = 0.0f; return; }
        double f = (rel - origin) / spacing;
        f = std::clamp(f, 0.0, double(points - 1));
        i0 = std::min(int(f), points - 2);
        w = float(f - i0);
    }

    /* The interpolated gain at grid cell (y0, x0) + weights, for input t (already clamped
       to [0,1] and gamma'd). */
    float Gain(int y0, float wy, int x0, float wx, float t) const
    {
        float fi = t * float(pointsN);
        fi = std::clamp(fi, 0.0f, float(pointsN - 1));
        const int i0 = std::min(int(fi), pointsN - 2);
        const float wi = fi - float(i0);
        const int y1 = pointsV > 1 ? y0 + 1 : y0;
        const int x1 = pointsH > 1 ? x0 + 1 : x0;
        auto tab = [&](int y, int x) {
            const float *g = gains.data() + (size_t(y) * size_t(pointsH) + size_t(x)) * size_t(pointsN) + i0;
            return g[0] + (g[1] - g[0]) * wi;
        };
        const float top = tab(y0, x0) + (tab(y0, x1) - tab(y0, x0)) * wx;
        const float bot = tab(y1, x0) + (tab(y1, x1) - tab(y1, x0)) * wx;
        return top + (bot - top) * wy;
    }

    /* The table input for one linear RIMM pixel. */
    float Input(float r, float g, float b) const
    {
        float t = weights[0] * r + weights[1] * g + weights[2] * b +
                  weights[3] * std::min({r, g, b}) + weights[4] * std::max({r, g, b});
        t = std::clamp(t, 0.0f, 1.0f);
        if (gamma != 1.0f) t = std::pow(t, gamma);
        return t;
    }

private:
    static float halfToFloat(quint16 h)
    {
        const quint32 sign = quint32(h & 0x8000) << 16;
        int exp = (h >> 10) & 0x1f;
        quint32 mant = h & 0x3ff;
        quint32 bits;
        if (exp == 0) {
            if (mant == 0) bits = sign;
            else {                                          // subnormal: normalise
                exp = 1;
                while (!(mant & 0x400)) { mant <<= 1; --exp; }
                mant &= 0x3ff;
                bits = sign | quint32(exp + 112) << 23 | mant << 13;
            }
        }
        else if (exp == 31) bits = sign | 0x7f800000u | mant << 13;
        else bits = sign | quint32(exp + 112) << 23 | mant << 13;
        float f; std::memcpy(&f, &bits, 4); return f;
    }
};

#endif // GAINTABLEMAP_H
