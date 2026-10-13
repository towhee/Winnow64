#ifndef LENSPROFILE_H
#define LENSPROFILE_H

/*
    LENS PROFILE CORRECTION: distortion, transverse chromatic aberration (TCA) and
    vignetting from a lensfun calibration (Develop/lensdb.h finds and interpolates it;
    this header applies it).

    THE MODELS are lensfun's, written here from their published formulas -- Winnow ships
    lensfun's DATABASE (CC BY-SA), not its LGPL library. Every model maps an UNDISTORTED
    (corrected) radius to the DISTORTED radius in the photo, so correcting is a direct
    lookup: for each output pixel, compute where it came from and sample there. Nothing
    is inverted.

      distortion  r_d = r_u * D(r_u)
                    poly3   D = 1 - k1 + k1 r^2
                    poly5   D = 1 + k1 r^2 + k2 r^4
                    ptlens  D = a r^3 + b r^2 + c r + 1 - a - b - c
      TCA         r_c = r_d * T_c(r_d), c = red or blue (green is the reference)
                    linear  T = k
                    poly3   T = b r^2 + c r + v
      vignetting  C_corrected = C_photo / (1 + k1 r^2 + k2 r^4 + k3 r^6)   ("pa")

    THE CENTRE SCALE IS LEFT ALONE. ptlens and poly3 do not map the centre 1:1 -- their
    slope at r = 0 is d = 1 - a - b - c -- so the corrected frame comes out magnified by
    about 1/d. Lensfun's master branch divides that out; lensfun 0.3.x (the release
    every raw developer ships, and the oracle tests/unit/tst_lensprofile.cpp checks
    against) applies the Hugin formula as written. Winnow follows 0.3.x, and the
    difference does not survive the auto-scale anyway: the zoom that fills the frame
    absorbs it.

    UNITS. The calibration was made on the LENS's calibration sensor (Coeffs::lensCrop,
    lensAspect); the photo is on the CAMERA's (cameraCrop). Both are related through
    millimetres on the sensor, 35 mm full frame having a 43.27 mm diagonal:
      photo:           mm per pixel   = 43.27 / cameraCrop / hypot(W - 1, H - 1)
      distortion/TCA:  r = 1 at       = half the SHORT side of the calibration frame
                                      = 43.27 / lensCrop / hypot(aspect, 1) / 2  mm
      vignetting:      r = 1 at       = the calibration frame's CORNER
                                      = 43.27 / lensCrop / 2  mm
    W, H are the frame's own pixels, so a proxy and a full-resolution render apply the
    same correction at their own sizes: the maths is resolution independent. (W - 1:
    the frame is measured between its outermost pixel CENTRES, as lensfun 0.3.x does.)

    ORDER, as lensfun applies it: devignette the PHOTO (its radius is a photo radius),
    then look up output pixel -> scale -> distortion -> per-channel TCA -> photo. Here the
    two happen in one pass: each channel is sampled at its own TCA position and then
    devignetted at that position.

    MANUAL CORRECTIONS (Amounts::manual*, Lightroom's Manual tab) ride the same pass and
    need no profile -- a lens lensfun has never calibrated still gets them. They are
    defined on the FRAME, not on a calibration sensor: rn = 1 at the frame's corner
    (half its diagonal), about the same centre as the profile.
      distortion  the output position is first scaled by M = 1 - kManualDistK * m * rn^2,
                  then handed to the profile (manual is a correction ON TOP of it).
                  m > 0 corrects barrel (pulls the corners out), m < 0 pincushion.
      vignetting  gain = 2^(v * kManualVigEV * rn^p), p = 2^(3 * midpoint): midpoint 0
                  reaches far into the frame, 1 only the corners. Evaluated at the PHOTO
                  position, like the profile's devignette. v > 0 brightens the corners.
    Both are in the auto-scale, so manual distortion never shows empty corners either.

    Header-only, OpenCV core only, no Qt -- tests/unit/tst_lensprofile.cpp.
*/

#include "opencv2/core.hpp"
#include <algorithm>
#include <cmath>

namespace LensProfile {

enum class DistModel { None, Poly3, Poly5, PTLens };
enum class TcaModel  { None, Linear, Poly3 };

/* 35 mm full frame's diagonal (36 x 24 mm), in mm. */
constexpr double kFullFrameDiagMm = 43.266615305567875;

/*
    One shot's profile: the calibration interpolated to its focal length / aperture /
    distance (LensDb::Database::interpolate) plus the two sensors it relates.
    d[] holds the model's terms exactly as the XML spells them (poly3: k1; poly5: k1, k2;
    ptlens: a, b, c); t[] likewise (linear: kr, kb; poly3: vr, vb, cr, cb, br, bb).
*/
struct Coeffs {
    DistModel dist = DistModel::None;
    float d[3] = {0.0f, 0.0f, 0.0f};
    TcaModel tca = TcaModel::None;
    float t[6] = {1.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f};
    bool vig = false;
    float v[3] = {0.0f, 0.0f, 0.0f};

    float lensCrop = 1.0f;      // crop factor of the calibration sensor
    float lensAspect = 1.5f;    // long / short side of the calibration frame
    float centerX = 0.0f;       // optical-axis offset, in half short sides
    float centerY = 0.0f;
    float cameraCrop = 1.0f;    // crop factor of the camera that took the photo

    bool hasDistortion() const { return dist != DistModel::None; }
    bool hasTca() const { return tca != TcaModel::None; }
    bool any() const { return hasDistortion() || hasTca() || vig; }
};

/* What the user asked for. distortion / vignetting are 0..2 (1 = the profile as
   calibrated); tca is whether the profile's TCA is used (the Remove CA checkbox).
   manual* need no profile: manualDistortion / manualVignetting -1..1 (0 = none),
   manualMidpoint 0..1 (see the header comment). */
struct Amounts {
    float distortion = 1.0f;
    float vignetting = 1.0f;
    bool  tca = true;
    float manualDistortion = 0.0f;
    float manualVignetting = 0.0f;
    float manualMidpoint = 0.5f;
};

/* Manual correction strength at full slider (+-100): the corner's radial scale and its
   exposure. Tuning constants -- recipes store the slider, so changing these re-renders
   every saved manual correction. */
constexpr double kManualDistK = 0.10;
constexpr double kManualVigEV = 2.0;

/* ------------------------------------------------------------------------------------
   The radial functions
   ------------------------------------------------------------------------------------ */

/* D(r) = r_d / r_u at undistorted radius r (distortion units), scaled toward identity
   by `amount`. */
inline double distortionFactor(const Coeffs &k, double r, double amount = 1.0)
{
    double D = 1.0;
    switch (k.dist) {
    case DistModel::Poly3:
        D = 1.0 - k.d[0] + k.d[0] * r * r;
        break;
    case DistModel::Poly5:
        D = 1.0 + k.d[0] * r * r + k.d[1] * r * r * r * r;
        break;
    case DistModel::PTLens: {
        const double a = k.d[0], b = k.d[1], c = k.d[2];
        D = a * r * r * r + b * r * r + c * r + 1.0 - a - b - c;
        break;
    }
    case DistModel::None:
        return 1.0;
    }
    return 1.0 + amount * (D - 1.0);
}

/* T_c(r): channel 0 = red, 1 = blue. 1 for green and when there is no TCA. */
inline double tcaFactor(const Coeffs &k, int redOrBlue, double r)
{
    switch (k.tca) {
    case TcaModel::Linear:
        return k.t[redOrBlue];
    case TcaModel::Poly3: {
        const double v = k.t[0 + redOrBlue], c = k.t[2 + redOrBlue], b = k.t[4 + redOrBlue];
        return b * r * r + c * r + v;
    }
    case TcaModel::None:
        break;
    }
    return 1.0;
}

/* Devignetting gain at vignetting radius r, raised to `amount` (0 = none, 1 = the
   profile, 2 = twice). The denominator is floored so a wild extrapolation past the
   calibrated corner cannot blow a pixel up. */
inline double vignettingGain(const Coeffs &k, double r, double amount = 1.0)
{
    if (!k.vig || amount == 0.0) return 1.0;
    const double r2 = r * r;
    const double c = 1.0 + k.v[0] * r2 + k.v[1] * r2 * r2 + k.v[2] * r2 * r2 * r2;
    const double g = 1.0 / std::max(c, 0.05);
    return amount == 1.0 ? g : std::pow(g, amount);
}

/* Manual distortion's radial scale at frame radius rn (1 = the corner). */
inline double manualDistortionFactor(const Amounts &a, double rn)
{
    return 1.0 - kManualDistK * double(a.manualDistortion) * rn * rn;
}

/* Manual devignetting gain at frame radius rn. */
inline double manualVignettingGain(const Amounts &a, double rn)
{
    if (a.manualVignetting == 0.0f) return 1.0;
    const double p = std::exp2(3.0 * std::clamp(double(a.manualMidpoint), 0.0, 1.0));
    return std::exp2(double(a.manualVignetting) * kManualVigEV * std::pow(rn, p));
}

/* ------------------------------------------------------------------------------------
   Geometry of one frame
   ------------------------------------------------------------------------------------ */

/* Pixel <-> model units for a W x H frame. */
struct Frame {
    double cx = 0.0, cy = 0.0;      // optical centre, pixels
    double pxToDist = 1.0;          // pixel distance -> distortion/TCA units
    double pxToVig = 1.0;           // pixel distance -> vignetting units
    double pxToManual = 1.0;        // pixel distance -> frame radius (1 = the corner)

    Frame() = default;
    Frame(const Coeffs &k, int W, int H)
    {
        const double mmPerPx = kFullFrameDiagMm / std::max(0.01f, k.cameraCrop)
                               / std::hypot(double(W - 1), double(H - 1));
        const double lensDiagMm = kFullFrameDiagMm / std::max(0.01f, k.lensCrop);
        const double distMm = lensDiagMm / std::hypot(double(std::max(1.0f, k.lensAspect)),
                                                      1.0) / 2.0;
        const double vigMm = lensDiagMm / 2.0;
        pxToDist = mmPerPx / distMm;
        pxToVig = mmPerPx / vigMm;
        pxToManual = 2.0 / std::max(1.0, std::hypot(double(W - 1), double(H - 1)));
        const double shortHalf = (std::min(W, H) - 1) / 2.0;
        cx = (W - 1) / 2.0 + shortHalf * k.centerX;
        cy = (H - 1) / 2.0 + shortHalf * k.centerY;
    }
};

/* Where output offset (dx, dy) -- pixels from the optical centre, before the output
   scale -- samples the photo, per channel (0 = R, 1 = G, 2 = B). */
struct Lookup {
    double sx[3], sy[3];
};

inline Lookup lookup(const Coeffs &k, const Amounts &a, const Frame &f,
                     double dx, double dy, double invScale)
{
    Lookup L;
    double qx = dx * invScale, qy = dy * invScale;
    if (a.manualDistortion != 0.0f) {
        const double M = manualDistortionFactor(a, std::hypot(qx, qy) * f.pxToManual);
        qx *= M;
        qy *= M;
    }
    const double ru = std::hypot(qx, qy) * f.pxToDist;
    const double D = distortionFactor(k, ru, a.distortion);
    const double ddx = qx * D, ddy = qy * D;
    const double rd = ru * D;
    L.sx[1] = f.cx + ddx;
    L.sy[1] = f.cy + ddy;
    for (int c = 0; c < 2; ++c) {
        const double T = a.tca ? tcaFactor(k, c, rd) : 1.0;
        const int ch = c == 0 ? 0 : 2;
        L.sx[ch] = f.cx + ddx * T;
        L.sy[ch] = f.cy + ddy * T;
    }
    return L;
}

inline bool geometryActive(const Coeffs &k, const Amounts &a)
{
    return (k.hasDistortion() && a.distortion != 0.0f) || (k.hasTca() && a.tca)
           || a.manualDistortion != 0.0f;
}

inline bool vignettingActive(const Coeffs &k, const Amounts &a)
{
    return (k.vig && a.vignetting != 0.0f) || a.manualVignetting != 0.0f;
}

/* Profile and manual devignetting combined, at photo offset (dx, dy) from the centre. */
inline double totalVignettingGain(const Coeffs &k, const Amounts &a, const Frame &f,
                                  double dx, double dy)
{
    const double r = std::hypot(dx, dy);
    return vignettingGain(k, r * f.pxToVig, a.vignetting)
           * manualVignettingGain(a, r * f.pxToManual);
}

/*
    The output scale that makes the corrected frame FILL the output: the smallest zoom
    at which every border pixel of the output samples inside the photo, so there are no
    empty corners (barrel correction pulls the edges in) and no more of the photo is
    thrown away than that takes (pincushion correction can zoom OUT, below 1). Lensfun's
    auto-scale. Bisection over the border samples; the mapping is monotonic in the zoom.
*/
inline double autoScale(const Coeffs &k, const Amounts &a, int W, int H)
{
    if (!geometryActive(k, a) || W < 2 || H < 2) return 1.0;
    const Frame f(k, W, H);
    const double tol = 0.25;               // a quarter pixel outside still counts as in
    auto inside = [&](double s) {
        const double inv = 1.0 / s;
        constexpr int N = 64;
        for (int i = 0; i <= N; ++i) {
            const double t = double(i) / N;
            const double pts[4][2] = {
                {t * (W - 1), 0.0}, {t * (W - 1), double(H - 1)},
                {0.0, t * (H - 1)}, {double(W - 1), t * (H - 1)}};
            for (const auto &p : pts) {
                const Lookup L = lookup(k, a, f, p[0] - f.cx, p[1] - f.cy, inv);
                for (int c = 0; c < 3; ++c)
                    if (L.sx[c] < -tol || L.sx[c] > W - 1 + tol ||
                        L.sy[c] < -tol || L.sy[c] > H - 1 + tol)
                        return false;
            }
        }
        return true;
    };
    double lo = 0.5, hi = 3.0;
    if (!inside(hi)) return hi;            // pathological profile: zoom as far as allowed
    if (inside(lo)) return lo;
    for (int it = 0; it < 40; ++it) {
        const double mid = 0.5 * (lo + hi);
        if (inside(mid)) hi = mid; else lo = mid;
    }
    return hi;
}

/* ------------------------------------------------------------------------------------
   Apply
   ------------------------------------------------------------------------------------ */

template <typename F>
inline void forRows(int rows, F fn)
{
    cv::parallel_for_(cv::Range(0, rows), [&](const cv::Range &r) {
        for (int y = r.start; y < r.end; ++y) fn(y);
    });
}

/* Bilinear sample of channel c of an interleaved RGB float image, clamped at the
   edges (the auto-scale keeps lookups inside; without it the edge pixel repeats). */
inline float sampleChannel(const float *src, int W, int H, int c, double x, double y)
{
    x = std::clamp(x, 0.0, double(W - 1));
    y = std::clamp(y, 0.0, double(H - 1));
    const int x0 = std::min(int(x), W - 2 < 0 ? 0 : W - 2);
    const int y0 = std::min(int(y), H - 2 < 0 ? 0 : H - 2);
    const int x1 = std::min(x0 + 1, W - 1), y1 = std::min(y0 + 1, H - 1);
    const float fx = float(x - x0), fy = float(y - y0);
    const float a = src[(size_t(y0) * W + x0) * 3 + c];
    const float b = src[(size_t(y0) * W + x1) * 3 + c];
    const float d0 = src[(size_t(y1) * W + x0) * 3 + c];
    const float d1 = src[(size_t(y1) * W + x1) * 3 + c];
    const float top = a + (b - a) * fx, bot = d0 + (d1 - d0) * fx;
    return top + (bot - top) * fy;
}

/*
    Correct a region of a frame. src is the WHOLE frame (W x H, interleaved RGB float,
    linear); dst receives the region (x0, y0, w, h) of the corrected frame, interleaved,
    w*h*3 floats, and must not alias src. scale is the output zoom (autoScale, or 1).
    The whole-frame call is region (0, 0, W, H); the Detail panel's 1:1 patch passes its
    own rectangle so the patch shows exactly what the loupe shows there.
*/
inline void applyRegion(const float *src, int W, int H, float *dst,
                        int x0, int y0, int w, int h,
                        const Coeffs &k, const Amounts &a, double scale)
{
    if (!src || !dst || W < 2 || H < 2 || w <= 0 || h <= 0) return;
    const Frame f(k, W, H);
    const bool geom = geometryActive(k, a);
    const bool vig = vignettingActive(k, a);
    const double inv = 1.0 / std::max(1e-6, scale);
    forRows(h, [&](int yy) {
        const int y = y0 + yy;
        float *o = dst + size_t(yy) * w * 3;
        for (int xx = 0; xx < w; ++xx) {
            const int x = x0 + xx;
            if (!geom) {
                const size_t si = (size_t(std::clamp(y, 0, H - 1)) * W
                                   + size_t(std::clamp(x, 0, W - 1))) * 3;
                const double g = vig ? totalVignettingGain(k, a, f, x - f.cx, y - f.cy)
                                     : 1.0;
                for (int c = 0; c < 3; ++c) o[xx * 3 + c] = float(src[si + c] * g);
                continue;
            }
            /* The zoom, like the model, is about the OPTICAL centre (lensfun's order). */
            const Lookup L = lookup(k, a, f, x - f.cx, y - f.cy, inv);
            for (int c = 0; c < 3; ++c) {
                float v = sampleChannel(src, W, H, c, L.sx[c], L.sy[c]);
                if (vig)
                    v = float(v * totalVignettingGain(k, a, f, L.sx[c] - f.cx,
                                                      L.sy[c] - f.cy));
                o[xx * 3 + c] = v;
            }
        }
    });
}

inline bool active(const Coeffs &k, const Amounts &a)
{
    return geometryActive(k, a) || vignettingActive(k, a);
}

}   // namespace LensProfile

#endif // LENSPROFILE_H
