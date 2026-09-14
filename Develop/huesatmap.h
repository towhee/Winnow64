#ifndef HUESATMAP_H
#define HUESATMAP_H

#include <cmath>
#include <vector>

/*
    The 3-D HSV correction table a DNG camera profile carries -- slice 3 of Phase 4 (see
    notes/Documentation.txt, "Camera Profiles (DCP) -- Phase 4 Plan"). Pure math, no Qt and
    no file reader, so it unit-tests in isolation -- the same shape as Develop/calibrate.h
    and Develop/colorgrade.h. ImageFormats/Dcp reads the numbers; Develop/cameraprofile
    blends the two illuminants' tables and supplies the bracketing matrices; this applies
    the result to a pixel.

    WHAT IT IS FOR. A 3x3 cannot fit a real sensor, because metameric error is nonlinear,
    so a profile's remaining per-patch error is baked into a table indexed by hue,
    saturation and value, each entry a (hue shift in DEGREES, saturation SCALE, value
    SCALE) triple. It is the part that makes "Adobe Standard" look like Adobe Standard
    rather than like a bare matrix, and the part most third-party raw developers omit.

    The LOOK TABLE of a creative profile ("Camera Vivid") is the identical structure at a
    different point in the pipeline, so it reuses this code -- it is simply not applied
    yet (slice 4).

    STORAGE ORDER, which is the thing to get wrong silently:

        index = ((val * hueDivs + hue) * satDivs + sat) * 3

    Value outermost, saturation innermost. Verified against 279 installed HueSatMaps and
    3 LookTables rather than taken from memory, by two independent structural properties:
    the saturation == 0 slice of every real HueSatMap is EXACTLY (sat 1.0, val 1.0) under
    this order and wildly not under the alternative (a neutral must stay neutral), and a
    real LookTable's value axis is smooth along the stride this order predicts and ~8x
    rougher along the one the alternative predicts.

    HUE WRAPS, saturation and value do not: the last hue division is adjacent to the
    first, so the interpolation closes the circle. valDivs == 1 is the common case (every
    installed HueSatMap measured) and means the table is effectively 2-D.
*/
namespace HueSatMap {

struct Table {
    int hueDivs = 0;
    int satDivs = 0;
    int valDivs = 0;
    /* ProfileHueSatMap/LookTableEncoding: 0 = linear, 1 = sRGB. Says which encoding of RGB
       the HSV conversion is meant to be done in, NOT which primaries -- the primaries are
       fixed by the specification (see CameraProfile's bracketing matrices). */
    int encoding = 0;
    std::vector<float> v;

    bool isEmpty() const {
        return hueDivs < 1 || satDivs < 1 || valDivs < 1 ||
               v.size() != size_t(hueDivs) * size_t(satDivs) * size_t(valDivs) * 3;
    }
    int entries() const { return hueDivs * satDivs * valDivs; }
};

/* Encoding values, spelled out so call sites do not carry bare integers. */
constexpr int kEncodingLinear = 0;
constexpr int kEncodingSrgb   = 1;

/*
    Blend two same-shaped tables -- the profile's two illuminants -- with the weight the
    matrix interpolation used (0 = entirely the warmer, 1 = entirely the cooler).

    ONCE PER RENDER, NOT PER PIXEL. Sampling two tables and blending the results for every
    pixel would double the eight-tap lookup for a result identical to blending the tables
    first, because the interpolation is linear in the table values.

    Either side may be empty: a profile with a table for only one illuminant uses that one
    at every temperature, which is what a single-illuminant profile means.
*/
inline bool Blend(const Table &warm, const Table &cool, float weight, Table &out)
{
    const bool haveWarm = !warm.isEmpty();
    const bool haveCool = !cool.isEmpty();
    if (!haveWarm && !haveCool) { out = Table(); return false; }
    if (!haveCool) { out = warm; return true; }
    if (!haveWarm) { out = cool; return true; }
    /* Different shapes cannot be blended entry by entry. The two tables share ONE dims tag
       so this cannot happen in a well-formed profile; taking the warm one is a safe
       fallback rather than an assertion in a render path. */
    if (warm.hueDivs != cool.hueDivs || warm.satDivs != cool.satDivs ||
        warm.valDivs != cool.valDivs) { out = warm; return true; }

    const float t = weight < 0.0f ? 0.0f : (weight > 1.0f ? 1.0f : weight);
    if (t <= 0.0f) { out = warm; return true; }
    if (t >= 1.0f) { out = cool; return true; }

    out.hueDivs = warm.hueDivs;
    out.satDivs = warm.satDivs;
    out.valDivs = warm.valDivs;
    out.encoding = warm.encoding;
    out.v.resize(warm.v.size());
    for (size_t i = 0; i < warm.v.size(); ++i)
        out.v[i] = warm.v[i] * (1.0f - t) + cool.v[i] * t;
    return true;
}

/* ---- sRGB transfer, for a table whose encoding is 1 ---- */

inline float SrgbEncode(float v)
{
    if (v <= 0.0f) return 0.0f;
    return v <= 0.0031308f ? v * 12.92f
                           : 1.055f * std::pow(v, 1.0f / 2.4f) - 0.055f;
}

inline float SrgbDecode(float v)
{
    if (v <= 0.0f) return 0.0f;
    return v <= 0.04045f ? v / 12.92f
                         : std::pow((v + 0.055f) / 1.055f, 2.4f);
}

/* ---- RGB <-> HSV, with hue in DEGREES to match the table's units ---- */

inline void ToHsv(float r, float g, float b, float &h, float &s, float &v)
{
    const float mx = std::fmax(r, std::fmax(g, b));
    const float mn = std::fmin(r, std::fmin(g, b));
    v = mx;
    const float d = mx - mn;
    s = (mx > 0.0f) ? d / mx : 0.0f;
    if (d <= 0.0f) { h = 0.0f; return; }
    if (mx == r)      h = 60.0f * (0.0f + (g - b) / d);
    else if (mx == g) h = 60.0f * (2.0f + (b - r) / d);
    else              h = 60.0f * (4.0f + (r - g) / d);
    if (h < 0.0f) h += 360.0f;
}

inline void FromHsv(float h, float s, float v, float &r, float &g, float &b)
{
    if (s <= 0.0f) { r = g = b = v; return; }
    h /= 60.0f;
    const float i = std::floor(h);
    const float f = h - i;
    const float p = v * (1.0f - s);
    const float q = v * (1.0f - s * f);
    const float t = v * (1.0f - s * (1.0f - f));
    switch (int(i) % 6) {
    case 0: r = v; g = t; b = p; break;
    case 1: r = q; g = v; b = p; break;
    case 2: r = p; g = v; b = t; break;
    case 3: r = p; g = q; b = v; break;
    case 4: r = t; g = p; b = v; break;
    default: r = v; g = p; b = q; break;
    }
}

/*
    Apply the table to one RGB triple, IN THE TABLE'S OWN COLOUR SPACE -- the caller
    brackets this with the matrices that get there and back (CameraProfile::Tables).

    THE VALUE AXIS IS INDEXED CLAMPED AND SCALED UNCLAMPED, which is a deliberate
    departure from a literal reading of the specification and the most consequential
    decision in this file. DNG describes a display-referred pipeline where value lives in
    0..1; Winnow's working data is SCENE-REFERRED and carries several stops of highlight
    headroom above 1.0, which is exactly what the view transform (Filmic / AgX) exists to
    roll off. Clamping value here would throw that headroom away before the transform ever
    saw it -- blowing every specular highlight flat the moment a profile was selected. So
    a pixel above white is looked up at the top of the value axis (there is no table entry
    beyond it to interpolate towards) and its value scale is applied as a plain multiply.

    Saturation IS clamped to 0..1, because saturation is bounded by its own definition:
    s = (max - min) / max cannot exceed 1, and a scale that pushed it past 1 would produce
    a negative channel.
*/
inline void Apply(const Table &t, float &r, float &g, float &b)
{
    if (t.isEmpty()) return;

    const bool srgb = (t.encoding == kEncodingSrgb);
    float pr = r, pg = g, pb = b;
    if (srgb) { pr = SrgbEncode(pr); pg = SrgbEncode(pg); pb = SrgbEncode(pb); }

    float h, s, v;
    ToHsv(pr, pg, pb, h, s, v);
    if (v <= 0.0f) return;                      // black has no hue and no saturation

    /* HUE WRAPS: the index runs over hueDivs cells around the full circle, so the cell
       above the last one is the first. Saturation and value are clamped at their ends. */
    const float hf = (h / 360.0f) * float(t.hueDivs);
    int h0 = int(std::floor(hf));
    const float hFrac = hf - float(h0);
    h0 = ((h0 % t.hueDivs) + t.hueDivs) % t.hueDivs;
    const int h1 = (h0 + 1) % t.hueDivs;

    const float sClamped = s < 0.0f ? 0.0f : (s > 1.0f ? 1.0f : s);
    const float sf = sClamped * float(t.satDivs - 1);
    int s0 = int(std::floor(sf));
    if (s0 > t.satDivs - 2) s0 = t.satDivs > 1 ? t.satDivs - 2 : 0;
    if (s0 < 0) s0 = 0;
    const float sFrac = (t.satDivs > 1) ? sf - float(s0) : 0.0f;
    const int s1 = (t.satDivs > 1) ? s0 + 1 : s0;

    /* Indexed at the top of the axis for anything at or above white -- see the note. */
    const float vClamped = v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v);
    const float vf = vClamped * float(t.valDivs - 1);
    int v0 = int(std::floor(vf));
    if (v0 > t.valDivs - 2) v0 = t.valDivs > 1 ? t.valDivs - 2 : 0;
    if (v0 < 0) v0 = 0;
    const float vFrac = (t.valDivs > 1) ? vf - float(v0) : 0.0f;
    const int v1 = (t.valDivs > 1) ? v0 + 1 : v0;

    const float *base = t.v.data();
    const int hs = t.satDivs;
    const int vs = t.hueDivs * t.satDivs;

    /* Trilinear over the eight surrounding cells. Accumulated as (hueShift, satScale,
       valScale) rather than fetched one component at a time so the eight reads are
       contiguous triples. */
    float acc[3] = {0.0f, 0.0f, 0.0f};
    const int hIdx[2] = {h0, h1};
    const int sIdx[2] = {s0, s1};
    const int vIdx[2] = {v0, v1};
    const float hW[2] = {1.0f - hFrac, hFrac};
    const float sW[2] = {1.0f - sFrac, sFrac};
    const float vW[2] = {1.0f - vFrac, vFrac};
    for (int iv = 0; iv < 2; ++iv) {
        if (vW[iv] == 0.0f) continue;
        for (int ih = 0; ih < 2; ++ih) {
            if (hW[ih] == 0.0f) continue;
            for (int is = 0; is < 2; ++is) {
                const float w = vW[iv] * hW[ih] * sW[is];
                if (w == 0.0f) continue;
                const float *e = base + size_t(vIdx[iv] * vs + hIdx[ih] * hs + sIdx[is]) * 3;
                acc[0] += w * e[0];
                acc[1] += w * e[1];
                acc[2] += w * e[2];
            }
        }
    }

    h += acc[0];                                // degrees
    if (h < 0.0f || h >= 360.0f) h -= 360.0f * std::floor(h / 360.0f);
    s *= acc[1];
    if (s < 0.0f) s = 0.0f;
    if (s > 1.0f) s = 1.0f;
    v *= acc[2];                                // NOT clamped -- headroom survives

    FromHsv(h, s, v, pr, pg, pb);
    if (srgb) { pr = SrgbDecode(pr); pg = SrgbDecode(pg); pb = SrgbDecode(pb); }
    r = pr; g = pg; b = pb;
}

} // namespace HueSatMap

#endif // HUESATMAP_H
