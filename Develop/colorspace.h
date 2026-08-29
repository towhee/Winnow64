#ifndef COLORSPACE_H
#define COLORSPACE_H

#include <cmath>

/*
    Pure colour-space math: the primaries of every space the pipeline knows about, the
    3x3 conversions between them, and the luma weights that belong to each. No Qt, no
    OpenCV, so it stays header-only and testable in isolation -- the same shape as
    Develop/calibrate.h and Develop/colorgrade.h.

    WHY THIS EXISTS. Two things used to be spread through the pipeline by hand:

      1. The Rec.709 luma weights (0.2126 / 0.7152 / 0.0722) were written out at ~15
         sites in Develop -- develop.cpp, calibrate.h, colorgrade.h, whitebalance.cpp --
         and rangemask.h used the Rec.601 weights instead, which was an inconsistency
         rather than a decision. Luma is a property OF A COLOUR SPACE: hardcoding one
         space's weights is exactly the thing that makes widening the working space
         expensive.

      2. The primaries matrices lived in three files that did not know about each other
         (rawcolor.cpp's kRgbToXyz, inputtransform.cpp's kXyzToSrgbIn,
         outputtransform.cpp's EncodingFor).

    Everything routes through here now, so the working space is a value rather than an
    assumption.

    WHITE POINT. Every space below is D65. That is deliberate: Develop/whitebalance.h
    solves an absolute Kelvin + tint against D65-referenced loci throughout, so keeping
    one white point across the working space means no chromatic adaptation step hides
    inside a conversion. It is also why the working space is linear Rec.2020 rather than
    ACEScg -- AP1's D60 white would put a Bradford adaptation between the camera matrix
    and the white balance solver for no gain in gamut coverage that matters here.

    CameraNative is a space in the enum but has NO fixed matrix: its primaries are the
    sensor's, which vary per model and arrive at runtime in CameraColor::xyzToCam. It is
    a tag meaning "not yet converted", and asking for a matrix involving it is a
    programming error (matrix() returns identity so a mistake degrades rather than
    crashes, but the caller should have used the camera's own matrix).
*/
namespace ColorSpaceMath {

enum class ColorSpace {
    CameraNative,       // sensor primaries; matrix comes from CameraColor, not from here
    LinearSRGB,         // Rec.709 primaries, D65 -- the display/loupe default
    LinearP3,           // Display P3 primaries, D65
    LinearAdobeRGB,     // Adobe RGB (1998) primaries, D65
    LinearRec2020       // Rec.2020 primaries, D65
};

/*
    The working space every Develop op operates in. Named ONCE, here: every op derives
    its luma weights from it and the input/output stages derive their matrices from it,
    so widening the working space is this one line plus re-blessed test goldens.

    NOW LinearRec2020. It was held at LinearSRGB while the camera->working conversion was
    moved out of the decoder into Develop, so that the PLUMBING change and this PIXEL
    change could be attributed separately -- which paid off: two regressions found during
    that move were provably not the space.

    Why it widened. Rec.709 is narrower than every camera's own gamut, so a saturated
    sensor colour was being clipped at decode, before the user had touched the image and
    irrecoverably. The pipeline now carries it through to the output stage, which is the
    only stage that knows what gamut it is rendering INTO.

    Why this needed AgX first. Preserving wide-gamut data only helps if something
    compresses it gracefully on the way out; otherwise it just clips per-channel at the
    output matrix instead of at the decode, which is the hue-shift failure mode with extra
    steps. AgX's inset/outset is that compression. Filmic still clips per-channel -- an
    out-of-gamut colour is no worse than before under it, just no better.
*/
constexpr ColorSpace kWorking = ColorSpace::LinearRec2020;

/*
    RGB -> CIE XYZ for each space, row-major, D65. Row 1 of each is that space's luma
    weights by construction -- lumaWeights() just reads it back, so the two can never
    drift apart.

    EVERYTHING HERE IS constexpr. That is load-bearing, not tidiness: the ops used to
   hardcode their luma weights partly because a function call in a per-pixel loop is not
   free. Constexpr means kLumR/kLumG/kLumB in develop.cpp fold to the same literals the
   compiler saw before, so centralising the weights costs exactly nothing at runtime
   while making the working space a single-line change. */
struct Matrix3 { double m[3][3]; };

constexpr Matrix3 kIdentity3 = {{
    {1.0, 0.0, 0.0}, {0.0, 1.0, 0.0}, {0.0, 0.0, 1.0}
}};

constexpr Matrix3 rgbToXyz(ColorSpace s)
{
    constexpr Matrix3 kSrgb = {{
        {0.4123908, 0.3575843, 0.1804808},
        {0.2126390, 0.7151687, 0.0721923},
        {0.0193308, 0.1191948, 0.9505322}
    }};
    constexpr Matrix3 kP3 = {{
        {0.4865709, 0.2656677, 0.1982173},
        {0.2289746, 0.6917385, 0.0792869},
        {0.0000000, 0.0451134, 1.0439444}
    }};
    constexpr Matrix3 kAdobe = {{
        {0.5766690, 0.1855582, 0.1882286},
        {0.2973450, 0.6273636, 0.0752915},
        {0.0270313, 0.0706889, 0.9911085}
    }};
    constexpr Matrix3 kRec2020 = {{
        {0.6369580, 0.1446169, 0.1688810},
        {0.2627002, 0.6779981, 0.0593017},
        {0.0000000, 0.0280727, 1.0609851}
    }};
    switch (s) {
    case ColorSpace::LinearSRGB:     return kSrgb;
    case ColorSpace::LinearP3:       return kP3;
    case ColorSpace::LinearAdobeRGB: return kAdobe;
    case ColorSpace::LinearRec2020:  return kRec2020;
    case ColorSpace::CameraNative:   break;
    }
    return kIdentity3;
}

/* std::fabs is not constexpr until C++23 and this header must fold at compile time. */
constexpr double absd(double v) { return v < 0.0 ? -v : v; }

/* CIE XYZ -> RGB, the inverse of the above. Inverted numerically rather than tabulated
   so the two are guaranteed consistent to float precision. */
constexpr bool invert3x3(const double a[3][3], double inv[3][3])
{
    const double det =
        a[0][0] * (a[1][1] * a[2][2] - a[1][2] * a[2][1]) -
        a[0][1] * (a[1][0] * a[2][2] - a[1][2] * a[2][0]) +
        a[0][2] * (a[1][0] * a[2][1] - a[1][1] * a[2][0]);
    if (absd(det) < 1e-12) return false;
    const double idet = 1.0 / det;
    inv[0][0] = (a[1][1] * a[2][2] - a[1][2] * a[2][1]) * idet;
    inv[0][1] = (a[0][2] * a[2][1] - a[0][1] * a[2][2]) * idet;
    inv[0][2] = (a[0][1] * a[1][2] - a[0][2] * a[1][1]) * idet;
    inv[1][0] = (a[1][2] * a[2][0] - a[1][0] * a[2][2]) * idet;
    inv[1][1] = (a[0][0] * a[2][2] - a[0][2] * a[2][0]) * idet;
    inv[1][2] = (a[0][2] * a[1][0] - a[0][0] * a[1][2]) * idet;
    inv[2][0] = (a[1][0] * a[2][1] - a[1][1] * a[2][0]) * idet;
    inv[2][1] = (a[0][1] * a[2][0] - a[0][0] * a[2][1]) * idet;
    inv[2][2] = (a[0][0] * a[1][1] - a[0][1] * a[1][0]) * idet;
    return true;
}

constexpr Matrix3 xyzToRgb(ColorSpace s)
{
    Matrix3 out{};
    if (!invert3x3(rgbToXyz(s).m, out.m)) return kIdentity3;
    return out;
}

/* dst = a . b (row-major), the fold that keeps a run of matrix stages at one matrix. */
constexpr Matrix3 multiply(const Matrix3 &a, const Matrix3 &b)
{
    Matrix3 out{};
    for (int i = 0; i < 3; ++i)
        for (int j = 0; j < 3; ++j) {
            double s = 0.0;
            for (int k = 0; k < 3; ++k) s += a.m[i][k] * b.m[k][j];
            out.m[i][j] = s;
        }
    return out;
}

/* The 3x3 taking a colour FROM one space TO another. Both D65, so this is a pure
   primaries change with no adaptation. */
constexpr Matrix3 matrix(ColorSpace from, ColorSpace to)
{
    if (from == to || from == ColorSpace::CameraNative || to == ColorSpace::CameraNative)
        return kIdentity3;
    return multiply(xyzToRgb(to), rgbToXyz(from));
}

/* Float twin of matrix(), for the hot loops and for folding into PointCoeffs. */
inline void matrixF(ColorSpace from, ColorSpace to, float out[3][3])
{
    const Matrix3 m = matrix(from, to);
    for (int i = 0; i < 3; ++i)
        for (int j = 0; j < 3; ++j) out[i][j] = static_cast<float>(m.m[i][j]);
}

/*
    The space's luma weights -- row 1 of its RGB->XYZ matrix, i.e. how much each primary
    contributes to CIE Y. Ops that hold brightness steady while moving chroma (Calibrate's
    scaleChroma, ColorGrade, the local-contrast luma prep) must use the weights of the
    space they are running in, or "luma-preserving" quietly stops being true.
*/
struct Luma { float r, g, b; };

constexpr Luma lumaWeights(ColorSpace s)
{
    const Matrix3 m = rgbToXyz(s);
    return { static_cast<float>(m.m[1][0]),
             static_cast<float>(m.m[1][1]),
             static_cast<float>(m.m[1][2]) };
}

/* The working space's weights, as three named constants -- the drop-in replacement for
   the 0.2126 / 0.7152 / 0.0722 literals the ops used to carry. Compile-time folded. */
constexpr Luma  kWorkingLuma = lumaWeights(kWorking);
constexpr float kLumR = kWorkingLuma.r;
constexpr float kLumG = kWorkingLuma.g;
constexpr float kLumB = kWorkingLuma.b;

/* Y of one pixel in space s. The one-liner every op used to write out by hand. */
constexpr float luma(float r, float g, float b, ColorSpace s = kWorking)
{
    const Luma w = lumaWeights(s);
    return w.r * r + w.g * g + w.b * b;
}

/* Apply a row-major 3x3 in place. */
inline void apply(const float m[3][3], float &r, float &g, float &b)
{
    const float R = m[0][0] * r + m[0][1] * g + m[0][2] * b;
    const float G = m[1][0] * r + m[1][1] * g + m[1][2] * b;
    const float B = m[2][0] * r + m[2][1] * g + m[2][2] * b;
    r = R; g = G; b = B;
}

} // namespace ColorSpaceMath

#endif // COLORSPACE_H
