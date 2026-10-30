#ifndef ARTBLUR_H
#define ARTBLUR_H

#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
#include <algorithm>
#include <cmath>

/*
    Effects > Blur: the ARTISTIC blurs (a depth / bokeh lens blur belongs to the Lens
    panel). One blur per scope, chosen by type: Gaussian, Motion, Spin, Zoom, Tilt-shift,
    Glow. Run by Develop::Blur on the scene-linear WorkingImage (CV_32FC3, interleaved),
    after Sharpen and before Grain, so grain stays crisp on top and a mask scope's blur is
    simply its layer, blended by coverage.

    Header-only, OpenCV only, so tst_artblur pins it without the app.

    RESOLUTION INDEPENDENCE. Every distance is a fraction of the FRAME's long edge, and
    every position is in frame pixels: Frame carries where this buffer sits in its frame
    (the Detail 1:1 patch is a crop of the full-res frame; a proxy or a settle render is
    the whole frame). So the proxy, the settle render and the patch all blur the same
    RELATIVE amount around the same point.

    SPIN and ZOOM by REPEATED HALVING. A spin blur averages the frame rotated about the
    centre over an arc; a polar remap would be exact but a full-res polar grid runs to
    gigabytes. Instead each pass averages the image with a copy of itself turned (or
    scaled) by half the previous step: after K passes the result is the mean of 2^K
    evenly spaced copies, and K is chosen so the last step is under a pixel at the
    frame's farthest corner -- ~10 warpAffine passes at full resolution.
*/
namespace ArtBlur {

enum Type { Gaussian = 0, Motion = 1, Spin = 2, Zoom = 3, TiltShift = 4, Glow = 5,
            TypeCount = 6 };

/* Ranges at amount 1, as fractions of the frame's long edge (or of the arc / radius). */
constexpr double kGaussianSigmaMax = 0.05;    // sigma
constexpr double kMotionLengthMax  = 0.10;    // streak length
constexpr double kSpinDegMax       = 60.0;    // arc
constexpr double kZoomMax          = 0.50;    // streak as a fraction of the radius
constexpr double kTiltSigmaMax     = 0.03;    // blur far from the band
/* The Tilt-shift band: its half-width and the ramp out to full blur, as fractions of the
   long edge, for size / feather 0..1. Shared with the canvas overlay (MW). */
constexpr double kTiltHalfMin      = 0.02;
constexpr double kTiltHalfRange    = 0.30;
constexpr double kTiltRampMin      = 0.02;
constexpr double kTiltRampRange    = 0.40;
inline double tiltHalf(double size)
{ return kTiltHalfMin + kTiltHalfRange * std::clamp(size, 0.0, 1.0); }
inline double tiltRamp(double feather)
{ return kTiltRampMin + kTiltRampRange * std::clamp(feather, 0.0, 1.0); }
constexpr double kGlowSigmaMin     = 0.005;   // glow radius at size 0
constexpr double kGlowSigmaMax     = 0.06;    // ... and at size 1
constexpr int    kMaxHalvings      = 13;

/* The settings, in SOURCE-frame terms (MW converts the stored, ORIENTED-frame centre
   and angle before the develop). */
struct Settings {
    int   type    = Gaussian;
    float amount  = 0.0f;      // 0..1
    float angle   = 0.0f;      // deg, y-down: Motion direction, Tilt-shift band
    float x = 0.5f, y = 0.5f;  // centre, normalized in the frame (Spin, Zoom, Tilt-shift)
    float size    = 0.5f;      // 0..1: Glow radius, Tilt-shift sharp-band width
    float feather = 0.5f;      // 0..1: Tilt-shift transition
    bool on() const { return amount > 0.0f; }
};

/* Where the buffer sits in its frame (pixels). fw == 0: the buffer IS the frame. */
struct Frame {
    int fx = 0, fy = 0, fw = 0, fh = 0;
};

namespace detail {

inline void resolveFrame(const cv::Mat &img, const Frame &f, double &fx, double &fy,
                         double &fw, double &fh)
{
    if (f.fw > 0 && f.fh > 0) { fx = f.fx; fy = f.fy; fw = f.fw; fh = f.fh; }
    else { fx = 0; fy = 0; fw = img.cols; fh = img.rows; }
}

/* Gaussian on a 3-channel float image, downscaled when wide (the result is smooth, so a
   blur of a smaller copy, scaled back, is indistinguishable and an order cheaper). */
inline void gaussian(const cv::Mat &src, cv::Mat &dst, double sigma)
{
    if (sigma < 0.3) { src.copyTo(dst); return; }
    const int k = std::max(1, int(sigma / 6.0));
    if (k < 2) {
        cv::GaussianBlur(src, dst, cv::Size(0, 0), sigma, sigma, cv::BORDER_REFLECT_101);
        return;
    }
    cv::Mat small;
    cv::resize(src, small, cv::Size(std::max(1, src.cols / k), std::max(1, src.rows / k)),
               0, 0, cv::INTER_AREA);
    cv::GaussianBlur(small, small, cv::Size(0, 0), sigma / k, sigma / k,
                     cv::BORDER_REFLECT_101);
    cv::resize(small, dst, src.size(), 0, 0, cv::INTER_LINEAR);
}

/* Distance from (cx, cy) to the farthest frame corner, in buffer pixels. */
inline double farthestCorner(double cx, double cy, double fx, double fy, double fw,
                             double fh)
{
    double r = 0.0;
    for (double X : {-fx, fw - fx})
        for (double Y : {-fy, fh - fy})
            r = std::max(r, std::hypot(X - cx, Y - cy));
    return r;
}

/* Mean of 2^K copies of img, each warped by the affine `step(t)` for t evenly spaced in
   [0, 1) -- by repeated halving. step(t) must compose: step(a) after step(b) = step(a+b)
   (a rotation or a scale about one centre). */
template <typename StepFn>
inline void halvingMean(cv::Mat &img, double spanPx, StepFn step)
{
    int K = 0;
    while (K < kMaxHalvings && spanPx / double(1 << K) > 1.0) ++K;
    const int W = img.cols, H = img.rows;
    /* Reflect-101 an integer coordinate into [0, n - 1] (as cv::BORDER_REFLECT_101). */
    auto refl = [](int v, int n) {
        if (n == 1) return 0;
        const int period = 2 * (n - 1);
        v %= period;
        if (v < 0) v += period;
        return v < n ? v : period - v;
    };
    /* One FUSED pass per halving: out = (img + img warped) / 2, sampled bilinearly in a
       single sweep -- a warpAffine then an addWeighted touched every pixel twice more,
       and the passes are memory-bound. */
    cv::Mat out(img.size(), img.type());
    for (int k = 1; k <= K; ++k) {
        const double t = 1.0 / double(1 << k);          // 1/2, 1/4, ...
        cv::Mat M = step(t), Mi;
        cv::invertAffineTransform(M, Mi);               // dst -> src, as warpAffine does
        const double a = Mi.at<double>(0, 0), b = Mi.at<double>(0, 1),
                     c = Mi.at<double>(0, 2), d = Mi.at<double>(1, 0),
                     e = Mi.at<double>(1, 1), f = Mi.at<double>(1, 2);
        cv::parallel_for_(cv::Range(0, H), [&](const cv::Range &rr) {
            for (int y = rr.start; y < rr.end; ++y) {
                const cv::Vec3f *src = img.ptr<cv::Vec3f>(y);
                cv::Vec3f *dst = out.ptr<cv::Vec3f>(y);
                double sx = b * y + c, sy = e * y + f;
                for (int x = 0; x < W; ++x, sx += a, sy += d) {
                    const int x0 = int(std::floor(sx)), y0 = int(std::floor(sy));
                    const float fx = float(sx - x0), fy = float(sy - y0);
                    const int xa = refl(x0, W), xb = refl(x0 + 1, W);
                    const int ya = refl(y0, H), yb = refl(y0 + 1, H);
                    const cv::Vec3f *r0 = img.ptr<cv::Vec3f>(ya);
                    const cv::Vec3f *r1 = img.ptr<cv::Vec3f>(yb);
                    const cv::Vec3f top = r0[xa] + (r0[xb] - r0[xa]) * fx;
                    const cv::Vec3f bot = r1[xa] + (r1[xb] - r1[xa]) * fx;
                    dst[x] = (src[x] + top + (bot - top) * fy) * 0.5f;
                }
            }
        });
        cv::swap(img, out);
    }
}

}  // namespace detail

/* Pixels of context a blur needs beyond a patch to be correct at its edges -- how far a
   Detail 1:1 patch is padded (MW caps it). Measured in the source frame (fw x fh). */
inline int reach(const Settings &s, int fw, int fh)
{
    if (!s.on() || fw <= 0 || fh <= 0) return 0;
    const double L = std::max(fw, fh), a = std::clamp(double(s.amount), 0.0, 1.0);
    switch (s.type) {
    case Gaussian:  return int(std::ceil(3.0 * a * kGaussianSigmaMax * L));
    case Motion:    return int(std::ceil(0.5 * a * kMotionLengthMax * L)) + 1;
    case Spin:
    case Zoom: {
        /* The arc / streak at the farthest corner -- an upper bound for any patch. */
        const double r = detail::farthestCorner(s.x * fw, s.y * fh, 0, 0, fw, fh);
        const double span = (s.type == Spin)
            ? r * a * kSpinDegMax * 3.141592653589793 / 180.0
            : r * a * kZoomMax;
        return int(std::ceil(span)) + 1;
    }
    case TiltShift: return int(std::ceil(3.0 * a * kTiltSigmaMax * L));
    case Glow: {
        const double sig = kGlowSigmaMin + std::clamp(double(s.size), 0.0, 1.0)
                                         * (kGlowSigmaMax - kGlowSigmaMin);
        return int(std::ceil(3.0 * sig * L));
    }
    }
    return 0;
}

/* Blur img (CV_32FC3, scene-linear) in place. */
inline void apply(cv::Mat &img, const Settings &s, const Frame &frame = Frame())
{
    if (!s.on() || img.empty() || img.type() != CV_32FC3) return;
    double fx, fy, fw, fh;
    detail::resolveFrame(img, frame, fx, fy, fw, fh);
    const double L = std::max(fw, fh);
    const double a = std::clamp(double(s.amount), 0.0, 1.0);
    /* The centre in THIS buffer's pixels (pixel centres at +0.5). */
    const double cx = s.x * fw - fx - 0.5, cy = s.y * fh - fy - 0.5;

    switch (s.type) {
    case Gaussian: {
        cv::Mat out;
        detail::gaussian(img, out, a * kGaussianSigmaMax * L);
        out.copyTo(img);
        return;
    }
    case Motion: {
        /* Turn the streak horizontal on a canvas big enough to hold every pixel, box it
           (cv::blur is O(1) per pixel), turn it back. One box = a constant exposure along
           the streak, which is what motion blur is. */
        const int len = int(std::lround(a * kMotionLengthMax * L));
        if (len < 2) return;
        const double ang = s.angle;
        const cv::Point2f c(img.cols * 0.5f, img.rows * 0.5f);
        cv::Mat R = cv::getRotationMatrix2D(c, ang, 1.0);
        const cv::Rect2f bb = cv::RotatedRect(c, img.size(), float(ang)).boundingRect2f();
        R.at<double>(0, 2) += bb.width * 0.5 - c.x;
        R.at<double>(1, 2) += bb.height * 0.5 - c.y;
        const cv::Size big(int(std::ceil(bb.width)), int(std::ceil(bb.height)));
        cv::Mat rot, back;
        cv::warpAffine(img, rot, R, big, cv::INTER_LINEAR, cv::BORDER_REFLECT_101);
        cv::blur(rot, rot, cv::Size(len, 1), cv::Point(-1, -1), cv::BORDER_REFLECT_101);
        cv::warpAffine(rot, back, R, img.size(), cv::INTER_LINEAR | cv::WARP_INVERSE_MAP,
                       cv::BORDER_REFLECT_101);
        back.copyTo(img);
        return;
    }
    case Spin: {
        const double deg = a * kSpinDegMax;
        const double r = detail::farthestCorner(cx, cy, fx, fy, fw, fh);
        const double span = r * deg * 3.141592653589793 / 180.0;
        if (span < 1.0) return;
        const cv::Point2f c{float(cx), float(cy)};
        detail::halvingMean(img, span, [&](double t) {
            return cv::getRotationMatrix2D(c, deg * t, 1.0);
        });
        /* The copies spanned [0, deg): turn back by half so the arc is centred on each
           pixel and the blur does not drift the picture round. */
        cv::Mat tmp;
        cv::warpAffine(img, tmp, cv::getRotationMatrix2D(c, -deg * 0.5, 1.0), img.size(),
                       cv::INTER_LINEAR, cv::BORDER_REFLECT_101);
        tmp.copyTo(img);
        return;
    }
    case Zoom: {
        /* Streaks OUTWARD from the centre: the mean of copies scaled by 1 .. 1 + z,
           geometrically spaced (scale composes by multiplication). */
        const double z = a * kZoomMax;
        const double r = detail::farthestCorner(cx, cy, fx, fy, fw, fh);
        const double span = r * z;
        if (span < 1.0) return;
        const cv::Point2f c{float(cx), float(cy)};
        const double logZ = std::log1p(z);
        detail::halvingMean(img, span, [&](double t) {
            return cv::getRotationMatrix2D(c, 0.0, std::exp(-logZ * t));
        });
        return;
    }
    case TiltShift: {
        /* Two blur levels, blended by the distance from the sharp band: inside the band
           the photo, then a smoothstep ramp out to the full blur. */
        const double sigma = a * kTiltSigmaMax * L;
        if (sigma < 0.3) return;
        cv::Mat b1, b2;
        detail::gaussian(img, b1, sigma * 0.5);
        detail::gaussian(img, b2, sigma);
        const double th = s.angle * 3.141592653589793 / 180.0;
        const double nx = -std::sin(th), ny = std::cos(th);      // band normal
        const double half = tiltHalf(s.size) * L;
        const double ramp = tiltRamp(s.feather) * L;
        cv::parallel_for_(cv::Range(0, img.rows), [&](const cv::Range &rr) {
            for (int y = rr.start; y < rr.end; ++y) {
                cv::Vec3f *p = img.ptr<cv::Vec3f>(y);
                const cv::Vec3f *q1 = b1.ptr<cv::Vec3f>(y);
                const cv::Vec3f *q2 = b2.ptr<cv::Vec3f>(y);
                for (int x = 0; x < img.cols; ++x) {
                    const double d = std::abs((x - cx) * nx + (y - cy) * ny);
                    double u = (d - half) / ramp;
                    if (u <= 0.0) continue;
                    u = std::min(1.0, u);
                    const float w = float(u * u * (3.0 - 2.0 * u)) * 2.0f;   // 0..2
                    p[x] = (w <= 1.0f) ? p[x] + (q1[x] - p[x]) * w
                                       : q1[x] + (q2[x] - q1[x]) * (w - 1.0f);
                }
            }
        });
        return;
    }
    case Glow: {
        /* A blurred copy laid back over the photo, lighten-style -- the glow only ever
           brightens, so detail survives under it (Effects > Orton, applyOrton, is the
           normal-blend soft focus). */
        const double sig = (kGlowSigmaMin + std::clamp(double(s.size), 0.0, 1.0)
                                          * (kGlowSigmaMax - kGlowSigmaMin)) * L;
        cv::Mat b;
        detail::gaussian(img, b, sig);
        const float k = float(a);
        cv::parallel_for_(cv::Range(0, img.rows), [&](const cv::Range &rr) {
            for (int y = rr.start; y < rr.end; ++y) {
                cv::Vec3f *p = img.ptr<cv::Vec3f>(y);
                const cv::Vec3f *q = b.ptr<cv::Vec3f>(y);
                for (int x = 0; x < img.cols; ++x)
                    for (int c = 0; c < 3; ++c)
                        p[x][c] += k * std::max(0.0f, q[x][c] - p[x][c]);
            }
        });
        return;
    }
    }
}

/*
    Effects > Orton: the Orton effect, a SEPARATE effect from the blurs above (its own
    EditParams ortonOpacity / ortonSize / ortonBrightness and its own eye), so it stacks
    with any of them. A copy of the picture is Gaussian-blurred, optionally brightened,
    and laid back over the picture at an opacity -- a NORMAL blend:

        out = in + opacity * (2^brightness * blur(in) - in)

    Unlike Glow's lighten blend (which only ever brightens, so highlights stay crisp),
    the normal blend softens everything, highlights included: bright detail spreads and
    dims, dark detail lifts, the sharp picture shows through the soft one. Brightness
    lifts the blurred copy first, the classic over-exposed slide of Michael Orton's
    sandwich. Scene-linear, like the blurs; Size is a fraction of the frame's long edge,
    so the proxy, the settle render and the Detail patch agree.
*/
constexpr double kOrtonSigmaMin    = 0.002;   // blur radius at size 0
constexpr double kOrtonSigmaMax    = 0.04;    // ... and at size 1
constexpr double kOrtonEvMax       = 2.0;     // brightness of the copy, EV

struct OrtonSettings {
    float opacity    = 0.0f;   // 0..1
    float size       = 0.5f;   // 0..1
    float brightness = 0.0f;   // EV, 0..kOrtonEvMax
    bool on() const { return opacity > 0.0f; }
};

inline double ortonSigma(double size, double longEdge)
{
    return (kOrtonSigmaMin + std::clamp(size, 0.0, 1.0)
                           * (kOrtonSigmaMax - kOrtonSigmaMin)) * longEdge;
}

/* Pixels of context Orton needs beyond a patch (see reach). */
inline int ortonReach(const OrtonSettings &s, int fw, int fh)
{
    if (!s.on() || fw <= 0 || fh <= 0) return 0;
    return int(std::ceil(3.0 * ortonSigma(s.size, std::max(fw, fh))));
}

/* Orton img (CV_32FC3, scene-linear) in place. */
inline void applyOrton(cv::Mat &img, const OrtonSettings &s, const Frame &frame = Frame())
{
    if (!s.on() || img.empty() || img.type() != CV_32FC3) return;
    double fx, fy, fw, fh;
    detail::resolveFrame(img, frame, fx, fy, fw, fh);
    cv::Mat b;
    detail::gaussian(img, b, ortonSigma(s.size, std::max(fw, fh)));
    const float o = std::clamp(s.opacity, 0.0f, 1.0f);
    const float gain = float(std::exp2(std::clamp(double(s.brightness), 0.0, kOrtonEvMax)));
    cv::parallel_for_(cv::Range(0, img.rows), [&](const cv::Range &rr) {
        for (int y = rr.start; y < rr.end; ++y) {
            cv::Vec3f *p = img.ptr<cv::Vec3f>(y);
            const cv::Vec3f *q = b.ptr<cv::Vec3f>(y);
            for (int x = 0; x < img.cols; ++x)
                p[x] += o * (gain * q[x] - p[x]);
        }
    });
}

}  // namespace ArtBlur

#endif // ARTBLUR_H
