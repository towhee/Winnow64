#ifndef LENSBLUR_H
#define LENSBLUR_H

#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
#include <algorithm>
#include <cmath>
#include <vector>

/*
    Lens > Lens Blur: a depth-of-field blur that imitates a wide aperture. Unlike the
    artistic blurs (Develop/artblur.h) the blur varies across the frame: every pixel is
    blurred by its CIRCLE OF CONFUSION (CoC), which grows with its distance in depth from
    the focal plane, and with the lens's bokeh shape.

    Header-only, OpenCV only, so tst_lensblur pins it without the app. MW drives it on the
    composited frame (MW::applyLensBlur): depth comes from the AI depth field
    (Develop/depthmask.h, Depth Anything V2), and mask scopes refine it (refine()).

    DEPTH AND THE CoC. The depth field is 0 = near .. 1 = far, min-max normalised
    DISPARITY (inverse depth), and a lens's CoC is proportional to the disparity
    difference from the focal plane -- so the CoC is simply linear in the field:

        coc = amount * kMaxCoc * longEdge * e,   e = sign(d) * max(0, |d| - range / 2)

    with d = depth - focus. Signed: positive behind the focal plane, negative in front of
    it, which is what decides who occludes whom.

    THE RENDER: LAYERS BY DEPTH. A per-pixel variable blur cannot be one convolution, and
    a plain gather bleeds a sharp subject into the blurred background behind it (the halo
    every cheap fake shows). So the frame is split into SLICES of signed CoC (tent
    memberships, so a slope blends smoothly between neighbours), each slice is blurred
    with the bokeh kernel at its radius, and the slices are composited back to front
    with "over" -- premultiplied, colour and coverage together. Background slices never
    contain the subject, so it cannot bleed into them; a blurred foreground spreads over
    whatever is behind it, as a real lens does. Where the slices leave a pixel partly
    uncovered (behind an occluder, which a photo never recorded) the result is
    renormalised by the accumulated coverage instead of going dark.

    COST. A slice of radius r is blurred at scale min(1, kernelCap / r), so its kernel is
    never more than kernelCap pixels: the large, far slices are tiny images and the cost
    is dominated by the few slices near focus. Slice radii are spaced geometrically.

    SHAPES. The kernel is rasterized (4x4 supersampled) from the shape's indicator:
    circle, 5- and 6-blade polygons, cat's eye, soap bubble (a bright rim) and
    anamorphic (a tall oval). The cat's eye varies across the frame -- round in the
    middle, clipped to a tangential almond toward the corners, the mechanical vignetting
    of a fast lens -- so it is convolved per tile on an 8 x 8 grid and the tiles are
    blended bilinearly.

    HIGHLIGHT BOOST. A photo's highlights are clipped, so a blurred street light is a dim
    grey disc instead of a bright one. Boost multiplies the brightest pixels before they
    are spread, in the blurred slices only (an in-focus highlight is untouched).

    Everything is in LINEAR light (the caller decodes the display transfer first).
*/
namespace LensBlur {

enum Shape { Circle = 0, Blade5 = 1, Blade6 = 2, CatsEye = 3, SoapBubble = 4,
             Anamorphic = 5, ShapeCount = 6 };

/* The CoC RADIUS at amount 1 for a full-range depth difference, as a fraction of the
   frame's long edge. */
constexpr double kMaxCoc = 0.05;
/* Slices under this radius (px) are not blurred at all. */
constexpr double kSharpRadius = 0.5;
constexpr int kCatTiles = 8;
constexpr double kAnamorphicWidth = 0.55;   // the oval's width / height

struct Settings {
    float amount = 0.0f;     // 0..1
    float focus  = 0.0f;     // 0..1 depth of the focal plane (0 near .. 1 far)
    float range  = 0.0f;     // 0..1 depth range kept sharp, centred on focus
    int   shape  = Circle;
    float boost  = 0.0f;     // 0..1 highlight boost
    bool on() const { return amount > 0.0f; }
};

/* Render quality: the interactive tick trades slice count and kernel size for speed. */
struct Quality {
    double kernelCap = 10.0; // max kernel radius (px) at a slice's own scale
    double step      = 1.3;  // ratio between neighbouring slice radii
    static Quality draft() { return {6.0, 1.55}; }
    static Quality final() { return {10.0, 1.3}; }
};

/* --- the CoC map ------------------------------------------------------------------- */

/* Signed CoC radius in pixels for depth d (0..1), longEdge = the frame's long edge in
   the pixels the render runs at. */
inline float cocAt(float d, const Settings &s, double longEdge)
{
    const double half = std::clamp(double(s.range), 0.0, 1.0) * 0.5;
    const double dd = double(d) - double(s.focus);
    const double e = dd > 0 ? std::max(0.0, dd - half) : std::min(0.0, dd + half);
    return float(std::clamp(double(s.amount), 0.0, 1.0) * kMaxCoc * longEdge * e);
}

/* The CoC map for a depth map (CV_32F, 0..1) the size of the frame. */
inline cv::Mat cocMap(const cv::Mat &depth, const Settings &s, double longEdge)
{
    cv::Mat coc(depth.size(), CV_32F);
    cv::parallel_for_(cv::Range(0, depth.rows), [&](const cv::Range &rr) {
        for (int y = rr.start; y < rr.end; ++y) {
            const float *d = depth.ptr<float>(y);
            float *c = coc.ptr<float>(y);
            for (int x = 0; x < depth.cols; ++x) c[x] = cocAt(d[x], s, longEdge);
        }
    });
    return coc;
}

/* The radius "Blur" refine pushes toward: a strong defocus at this amount. */
inline double refineBlurRadius(const Settings &s, double longEdge)
{
    return std::clamp(double(s.amount), 0.0, 1.0) * kMaxCoc * longEdge * 0.6;
}

/* A mask scope's refine, over its coverage (CV_32F 0..1, the frame's size): k < 0 pulls
   the CoC toward sharp (-1 = in focus), k > 0 pushes it toward refineBlurRadius (+1 =
   that strong a blur). The sign (front / behind) is kept; a refined in-focus pixel
   counts as behind. */
inline void refine(cv::Mat &coc, const cv::Mat &cov, float k, double blurRadius)
{
    if (k == 0.0f || cov.empty() || cov.size() != coc.size()) return;
    const float kk = std::clamp(k, -1.0f, 1.0f);
    const float R = float(blurRadius);
    cv::parallel_for_(cv::Range(0, coc.rows), [&](const cv::Range &rr) {
        for (int y = rr.start; y < rr.end; ++y) {
            float *c = coc.ptr<float>(y);
            const float *m = cov.ptr<float>(y);
            for (int x = 0; x < coc.cols; ++x) {
                const float w = std::clamp(m[x], 0.0f, 1.0f);
                if (w <= 0.0f) continue;
                if (kk < 0.0f) c[x] *= 1.0f - w * -kk;
                else {
                    const float mag = std::abs(c[x]);
                    if (mag < R) {
                        const float to = mag + w * kk * (R - mag);
                        c[x] = c[x] < 0.0f ? -to : to;
                    }
                }
            }
        }
    });
}

/* --- kernels ----------------------------------------------------------------------- */

namespace detail {

/* Weight of the unit shape at (u, v) (y down), 0 outside. cat = the cat's eye's second
   disc centre, in radii. */
inline float shapeWeight(int shape, double u, double v, cv::Point2d cat)
{
    const double rr = u * u + v * v;
    switch (shape) {
    case Blade5:
    case Blade6: {
        const int n = shape == Blade5 ? 5 : 6;
        const double apothem = std::cos(M_PI / n);
        /* Edge normals of a regular n-gon with a vertex straight up. */
        for (int k = 0; k < n; ++k) {
            const double t = -M_PI / 2.0 + M_PI / n + k * 2.0 * M_PI / n;
            if (u * std::cos(t) + v * std::sin(t) > apothem) return 0.0f;
        }
        return 1.0f;
    }
    case CatsEye: {
        if (rr > 1.0) return 0.0f;
        const double du = u - cat.x, dv = v - cat.y;
        return du * du + dv * dv <= 1.0 ? 1.0f : 0.0f;
    }
    case SoapBubble:
        if (rr > 1.0) return 0.0f;
        return float(0.25 + 0.75 * std::pow(rr, 4.0));       // a bright rim
    case Anamorphic: {
        const double a = u / kAnamorphicWidth;
        return a * a + v * v <= 1.0 ? 1.0f : 0.0f;
    }
    case Circle:
    default:
        return rr <= 1.0 ? 1.0f : 0.0f;
    }
}

}   // namespace detail

/* The bokeh kernel at radius r px (CV_32F, odd square, sums to 1). */
inline cv::Mat kernel(int shape, double r, cv::Point2d cat = {0, 0})
{
    r = std::max(r, 0.5);
    const int R = int(std::ceil(r + 0.5));
    cv::Mat k(2 * R + 1, 2 * R + 1, CV_32F, cv::Scalar(0));
    constexpr int ss = 4;
    double sum = 0.0;
    for (int y = -R; y <= R; ++y) {
        float *row = k.ptr<float>(y + R);
        for (int x = -R; x <= R; ++x) {
            double w = 0.0;
            for (int sy = 0; sy < ss; ++sy)
                for (int sx = 0; sx < ss; ++sx) {
                    const double u = (x - 0.5 + (sx + 0.5) / ss) / r;
                    const double v = (y - 0.5 + (sy + 0.5) / ss) / r;
                    w += detail::shapeWeight(shape, u, v, cat);
                }
            row[x + R] = float(w);
            sum += w;
        }
    }
    if (sum <= 0.0) { k.setTo(0); k.at<float>(R, R) = 1.0f; }
    else k /= sum;
    return k;
}

/* The cat's eye's second disc centre for a point at frame-normalized offset (vx, vy)
   from the centre (corner = length 1): toward the centre, more clipped further out. */
inline cv::Point2d catOffset(double vx, double vy)
{
    const double m = std::sqrt(vx * vx + vy * vy);
    if (m < 1e-6) return {0, 0};
    const double off = 1.1 * std::pow(std::min(1.0, m), 1.3);
    return {-vx / m * off, -vy / m * off};
}

namespace detail {

/* filter2D in parallel horizontal strips. OpenCV's filter2D switches to a DFT for any
   kernel of 9 x 9 or more and runs it on one thread (150 ms for a 3 MP four-channel
   slice); strips of a standalone src read their real neighbours above and below (a ROI
   is not isolated), so the result is identical and ~10x faster. */
inline void filterPar(const cv::Mat &src, cv::Mat &dst, const cv::Mat &k)
{
    dst.create(src.size(), src.type());
    const int n = std::max(1, std::min(cv::getNumThreads() * 2, src.rows / 48));
    cv::parallel_for_(cv::Range(0, n), [&](const cv::Range &rr) {
        for (int i = rr.start; i < rr.end; ++i) {
            const int y0 = int(int64_t(src.rows) * i / n);
            const int y1 = int(int64_t(src.rows) * (i + 1) / n);
            if (y1 <= y0) continue;
            cv::Mat out = dst.rowRange(y0, y1);
            cv::filter2D(src.rowRange(y0, y1), out, -1, k, cv::Point(-1, -1), 0,
                         cv::BORDER_REFLECT_101);
        }
    });
}

/* Convolve src (CV_32FC4, premultiplied, a standalone Mat) with the shape at radius r
   into dst. For the cat's eye the kernel follows position in the FRAME: src covers
   [ox, ox + src.cols) x [oy, oy + src.rows) of a fw x fh frame (all at src's scale), the
   frame is cut into a kCatTiles grid, each tile's region is convolved with the kernel
   for the tile centre, and the tiles are blended bilinearly between their centres. */
inline void convolve(const cv::Mat &src, cv::Mat &dst, int shape, double r,
                     double ox = 0, double oy = 0, double fw = 0, double fh = 0)
{
    if (shape != CatsEye || r < 3.0) {
        filterPar(src, dst, kernel(shape == CatsEye ? Circle : shape, r));
        return;
    }
    if (fw <= 0 || fh <= 0) { fw = src.cols; fh = src.rows; }
    const int W = src.cols, H = src.rows, G = kCatTiles;
    dst.create(src.size(), src.type());
    dst.setTo(cv::Scalar::all(0));
    const double tw = fw / G, th = fh / G;
    const double halfDiag = 0.5 * std::sqrt(fw * fw + fh * fh);
    /* Hat weight of tile centre i along an axis of n tiles of size t, at frame
       coordinate p (flat beyond the outer centres, so the weights sum to 1). */
    auto hat = [](int i, int n, double t, double p) {
        const double c = (i + 0.5) * t;
        if ((i == 0 && p <= c) || (i == n - 1 && p >= c)) return 1.0;
        return std::max(0.0, 1.0 - std::abs(p - c) / t);
    };
    /* Each tile's convolution in parallel (one tile per task: a nested parallel_for
       would serialise anyway), then the blend in parallel over rows. */
    struct Part { cv::Rect roi; int i, j; cv::Mat m; };
    std::vector<Part> parts;
    for (int j = 0; j < G; ++j) {
        for (int i = 0; i < G; ++i) {
            /* The hat's support, from the previous centre to the next, in src pixels. */
            const double fx0 = i == 0 ? -1e9 : (i - 0.5) * tw;
            const double fx1 = i == G - 1 ? 1e9 : (i + 1.5) * tw;
            const double fy0 = j == 0 ? -1e9 : (j - 0.5) * th;
            const double fy1 = j == G - 1 ? 1e9 : (j + 1.5) * th;
            const int x0 = std::max(0, int(std::floor(fx0 - ox)));
            const int x1 = std::min(W, int(std::ceil(fx1 - ox)));
            const int y0 = std::max(0, int(std::floor(fy0 - oy)));
            const int y1 = std::min(H, int(std::ceil(fy1 - oy)));
            if (x1 <= x0 || y1 <= y0) continue;
            parts.push_back({cv::Rect(x0, y0, x1 - x0, y1 - y0), i, j, cv::Mat()});
        }
    }
    cv::parallel_for_(cv::Range(0, int(parts.size())), [&](const cv::Range &rr) {
        for (int n = rr.start; n < rr.end; ++n) {
            Part &pt = parts[size_t(n)];
            const double vx = ((pt.i + 0.5) * tw - fw * 0.5) / halfDiag;
            const double vy = ((pt.j + 0.5) * th - fh * 0.5) / halfDiag;
            /* filter2D on a ROI reads the pixels around it in src, so the tile edges
               see their real neighbours. */
            cv::filter2D(src(pt.roi), pt.m, -1, kernel(CatsEye, r, catOffset(vx, vy)),
                         cv::Point(-1, -1), 0, cv::BORDER_REFLECT_101);
        }
    });
    cv::parallel_for_(cv::Range(0, H), [&](const cv::Range &rr) {
        for (int y = rr.start; y < rr.end; ++y) {
            cv::Vec4f *d = dst.ptr<cv::Vec4f>(y);
            for (const Part &pt : parts) {
                if (y < pt.roi.y || y >= pt.roi.y + pt.roi.height) continue;
                const double wy = hat(pt.j, G, th, oy + y + 0.5);
                if (wy <= 0.0) continue;
                const cv::Vec4f *p = pt.m.ptr<cv::Vec4f>(y - pt.roi.y);
                for (int x = 0; x < pt.roi.width; ++x) {
                    const int X = pt.roi.x + x;
                    const float w = float(wy * hat(pt.i, G, tw, ox + X + 0.5));
                    if (w > 0.0f) d[X] += p[x] * w;
                }
            }
        }
    });
}

}   // namespace detail

/* The slice radii (signed, ascending) covering [cmin, cmax]: 0, then geometric. */
inline std::vector<double> sliceRadii(double cmin, double cmax, const Quality &q)
{
    std::vector<double> pos, neg;
    for (double r = 1.0; ; r *= q.step) {
        if (r < cmax * q.step) pos.push_back(r);
        if (r < -cmin * q.step) neg.push_back(-r);
        if (r >= std::max(cmax, -cmin)) break;
    }
    std::vector<double> s(neg.rbegin(), neg.rend());
    s.push_back(0.0);
    s.insert(s.end(), pos.begin(), pos.end());
    return s;
}

/* Blur lin (CV_32FC3, linear light) by the signed CoC map coc (CV_32F, px, same size).
   Returns CV_32FC3. */
inline cv::Mat render(const cv::Mat &lin, const cv::Mat &coc, int shape, float boost,
                      const Quality &q = Quality::final())
{
    CV_Assert(lin.type() == CV_32FC3 && coc.type() == CV_32F && lin.size() == coc.size());
    const int W = lin.cols, H = lin.rows;
    double cmin = 0.0, cmax = 0.0;
    cv::minMaxLoc(coc, &cmin, &cmax);
    if (cmax < kSharpRadius && -cmin < kSharpRadius) return lin.clone();

    const std::vector<double> s = sliceRadii(cmin, cmax, q);
    const int K = int(s.size());

    /* Each pixel's place among the slices, once: the slice below it (lower) and how far
       toward the next (t), so a slice's membership is 1 - t where it is the lower one
       and t where it is the upper. And each slice's bounding box, so a slice of a few
       pixels (the in-focus subject, a sliver of foreground) costs a few pixels. */
    cv::Mat lower(H, W, CV_16S), frac(H, W, CV_32F);
    struct Box { int x0, y0, x1, y1; };
    std::vector<Box> box(size_t(K), Box{W, H, -1, -1});
    cv::Mutex boxMutex;
    cv::parallel_for_(cv::Range(0, H), [&](const cv::Range &rr) {
        std::vector<Box> b(size_t(K), Box{W, H, -1, -1});
        auto grow = [&](int j, int x, int y) {
            Box &e = b[size_t(j)];
            e.x0 = std::min(e.x0, x); e.x1 = std::max(e.x1, x);
            e.y0 = std::min(e.y0, y); e.y1 = std::max(e.y1, y);
        };
        for (int y = rr.start; y < rr.end; ++y) {
            const float *c = coc.ptr<float>(y);
            short *lo = lower.ptr<short>(y);
            float *fr = frac.ptr<float>(y);
            for (int x = 0; x < W; ++x) {
                const double v = c[x];
                int j = int(std::upper_bound(s.begin(), s.end(), v) - s.begin()) - 1;
                double t = 0.0;
                if (j < 0) j = 0;                               // below the first slice
                else if (j >= K - 1) j = K - 1;                 // at / beyond the last
                else t = (v - s[size_t(j)]) / (s[size_t(j + 1)] - s[size_t(j)]);
                lo[x] = short(j);
                fr[x] = float(t);
                if (t < 1.0) grow(j, x, y);
                if (t > 0.0) grow(j + 1, x, y);
            }
        }
        cv::AutoLock l(boxMutex);
        for (int j = 0; j < K; ++j) {
            const Box &e = b[size_t(j)];
            if (e.x1 < 0) continue;
            Box &g = box[size_t(j)];
            g.x0 = std::min(g.x0, e.x0); g.x1 = std::max(g.x1, e.x1);
            g.y0 = std::min(g.y0, e.y0); g.y1 = std::max(g.y1, e.y1);
        }
    });

    cv::Mat acc(H, W, CV_32FC4, cv::Scalar::all(0));       // premultiplied, + coverage
    cv::Mat slice, small, blurred, up;

    /* Back to front: the most positive CoC (furthest behind) first. */
    for (int j = K - 1; j >= 0; --j) {
        const Box &bx = box[size_t(j)];
        if (bx.x1 < 0) continue;
        const double r = std::abs(s[size_t(j)]);
        const bool blur = r >= kSharpRadius;
        /* The slice's pixels, plus how far its blur reaches. */
        const int reach = blur ? int(std::ceil(r)) + 2
                               : 0;
        const cv::Rect roi = cv::Rect(bx.x0 - reach, bx.y0 - reach,
                                      bx.x1 - bx.x0 + 1 + 2 * reach,
                                      bx.y1 - bx.y0 + 1 + 2 * reach)
                             & cv::Rect(0, 0, W, H);
        /* Boost the bright pixels of a blurred slice, fading in with its radius. */
        const float bw = boost > 0.0f
            ? boost * float(std::clamp((r - 1.5) / 3.0, 0.0, 1.0)) : 0.0f;

        slice.create(roi.size(), CV_32FC4);
        cv::parallel_for_(cv::Range(0, roi.height), [&](const cv::Range &rr) {
            for (int y = rr.start; y < rr.end; ++y) {
                const short *lo = lower.ptr<short>(roi.y + y) + roi.x;
                const float *fr = frac.ptr<float>(roi.y + y) + roi.x;
                const cv::Vec3f *p = lin.ptr<cv::Vec3f>(roi.y + y) + roi.x;
                cv::Vec4f *o = slice.ptr<cv::Vec4f>(y);
                for (int x = 0; x < roi.width; ++x) {
                    const float a = lo[x] == j ? 1.0f - fr[x]
                                  : lo[x] == j - 1 ? fr[x] : 0.0f;
                    if (a <= 0.0f) { o[x] = cv::Vec4f(0, 0, 0, 0); continue; }
                    cv::Vec3f px = p[x];
                    if (bw > 0.0f) {
                        const float m = std::max({px[0], px[1], px[2]});
                        const float t = std::clamp((m - 0.7f) / 0.3f, 0.0f, 1.0f);
                        px *= 1.0f + bw * 3.0f * t * t;
                    }
                    o[x] = cv::Vec4f(px[0] * a, px[1] * a, px[2] * a, a);
                }
            }
        });

        const cv::Mat *res = &slice;
        if (blur) {
            const double f = std::min(1.0, q.kernelCap / r);
            if (f < 0.999) {
                const cv::Size sz(std::max(1, int(std::lround(roi.width * f))),
                                  std::max(1, int(std::lround(roi.height * f))));
                cv::resize(slice, small, sz, 0, 0, cv::INTER_AREA);
                const double fx = double(sz.width) / roi.width;
                const double fy = double(sz.height) / roi.height;
                detail::convolve(small, blurred, shape, r * f, roi.x * fx, roi.y * fy,
                                 W * fx, H * fy);
                cv::resize(blurred, up, roi.size(), 0, 0, cv::INTER_LINEAR);
                res = &up;
            }
            else {
                detail::convolve(slice, blurred, shape, r, roi.x, roi.y, W, H);
                res = &blurred;
            }
        }

        /* acc = layer over acc. */
        const cv::Mat &L = *res;
        cv::parallel_for_(cv::Range(0, roi.height), [&](const cv::Range &rr) {
            for (int y = rr.start; y < rr.end; ++y) {
                const cv::Vec4f *l = L.ptr<cv::Vec4f>(y);
                cv::Vec4f *a = acc.ptr<cv::Vec4f>(roi.y + y) + roi.x;
                for (int x = 0; x < roi.width; ++x) {
                    const float t = 1.0f - std::clamp(l[x][3], 0.0f, 1.0f);
                    a[x] = l[x] + a[x] * t;
                }
            }
        });
    }

    /* Renormalise by the coverage the slices left. */
    cv::Mat out(H, W, CV_32FC3);
    cv::parallel_for_(cv::Range(0, H), [&](const cv::Range &rr) {
        for (int y = rr.start; y < rr.end; ++y) {
            const cv::Vec4f *a = acc.ptr<cv::Vec4f>(y);
            const cv::Vec3f *p = lin.ptr<cv::Vec3f>(y);
            cv::Vec3f *o = out.ptr<cv::Vec3f>(y);
            for (int x = 0; x < W; ++x) {
                const float al = a[x][3];
                if (al < 1e-4f) { o[x] = p[x]; continue; }
                const float inv = 1.0f / al;
                o[x] = cv::Vec3f(a[x][0] * inv, a[x][1] * inv, a[x][2] * inv);
            }
        }
    });
    return out;
}

}   // namespace LensBlur

#endif // LENSBLUR_H
