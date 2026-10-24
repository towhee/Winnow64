#include "Develop/Transform/croptransform.h"

#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/core/utility.hpp>   // parallel_for_
#include <QTransform>
#include <QPolygonF>
#include <vector>
#include <cmath>
#include <cstring>
#include <algorithm>

namespace {

/* Largest all-valid (non-zero) axis-aligned rectangle in a binary mask, by the classic
   per-row histogram + monotonic-stack method (O(rows*cols)). mask is CV_8U (0 / non-zero). */
cv::Rect largestValidRect(const cv::Mat &mask)
{
    const int rows = mask.rows, cols = mask.cols;
    std::vector<int> h(cols, 0);
    int bestArea = 0;
    cv::Rect best(0, 0, 0, 0);
    std::vector<int> st;
    st.reserve(cols + 1);

    for (int y = 0; y < rows; ++y) {
        const uchar *m = mask.ptr<uchar>(y);
        for (int x = 0; x < cols; ++x) h[x] = m[x] ? h[x] + 1 : 0;

        st.clear();
        for (int x = 0; x <= cols; ++x) {
            const int curH = (x == cols) ? 0 : h[x];
            while (!st.empty() && h[st.back()] >= curH) {
                const int height = h[st.back()];
                st.pop_back();
                const int left = st.empty() ? 0 : st.back() + 1;
                const int width = x - left;
                const int area = height * width;
                if (area > bestArea) {
                    bestArea = area;
                    best = cv::Rect(left, y - height + 1, width, height);
                }
            }
            st.push_back(x);
        }
    }
    return best;
}

/* QImage -> owned BGRA cv::Mat (ARGB32's little-endian byte order IS B,G,R,A). */
cv::Mat toBgra(const QImage &img)
{
    const QImage c = img.convertToFormat(QImage::Format_ARGB32);
    cv::Mat view(c.height(), c.width(), CV_8UC4,
                 const_cast<uchar *>(c.bits()), c.bytesPerLine());
    return view.clone();
}

/* QImage -> owned 16-bit RGBA cv::Mat, the 16-bit twin of toBgra for a deep (export)
   image. RGBA64 is R,G,B,A as quint16 -- a different channel ORDER to toBgra's BGRA,
   which does not matter here: warpPerspective is geometric, and only channel 3 (alpha,
   the same index in both) is read, for the validity mask. */
cv::Mat toRgba64(const QImage &img)
{
    const QImage c = img.convertToFormat(QImage::Format_RGBA64);
    cv::Mat view(c.height(), c.width(), CV_16UC4,
                 const_cast<uchar *>(c.bits()), c.bytesPerLine());
    return view.clone();
}

/* True for the 16-bit-per-channel QImage formats the export path produces. Everything
   downstream that has to pick a working format branches on this so a 16-bit export is not
   silently flattened to 8 bits by an intermediate stage. */
bool isDeep(const QImage &img)
{
    return img.format() == QImage::Format_RGBX64 ||
           img.format() == QImage::Format_RGBA64 ||
           img.format() == QImage::Format_RGBA64_Premultiplied;
}

inline double dist(const cv::Point2f &a, const cv::Point2f &b)
{
    return std::hypot(double(a.x) - b.x, double(a.y) - b.y);
}

/* Fold a pixel coordinate back into [0, len - 1] by mirroring at the frame edges (edges
   at -0.5 and len - 0.5, so the reflection repeats the edge pixel like BORDER_REFLECT).
   Done here rather than by cv::remap's border mode because remap's fixed-point maps
   saturate at +/-32767, which a long reflected ray on a big frame can exceed. */
inline float reflectCoord(float v, int len)
{
    const float period = 2.0f * float(len);
    float m = std::fmod(v + 0.5f, period);
    if (m < 0.0f) m += period;
    if (m >= float(len)) m = period - m;
    m -= 0.5f;
    return std::min(std::max(m, 0.0f), float(len - 1));
}

/* The kaleidoscope fold for one point in output PIXEL coords (pixel centres at +0.5):
   the source pixel coords it samples, before the border reflection. */
struct KaleidoFold {
    float cx, cy, a, s, wedge, inv;     // inv = 1 / zoom: the source radius factor
    int n;
    KaleidoFold(const CropTransform::Kaleido &k, double W, double H)
        : cx(float(k.x * W)), cy(float(k.y * H)),
          a(float(k.angle * 0.017453292519943295)),
          s(float(k.source * 0.017453292519943295)),
          wedge(float(6.283185307179586 / std::max(2, k.segments))),
          inv(float(1.0 / std::clamp(k.zoom, 0.25, 8.0))),
          n(std::max(2, k.segments)) {}
    /* One strip's remap coordinates (rows y0 .. y0 + rows - 1 of a W x H frame), with
       the border reflection folded in. */
    void strip(int y0, int rows, int W, int H, cv::Mat &mapX, cv::Mat &mapY) const {
        mapX.create(rows, W, CV_32F);
        mapY.create(rows, W, CV_32F);
        for (int r = 0; r < rows; ++r) {
            float *mx = mapX.ptr<float>(r);
            float *my = mapY.ptr<float>(r);
            const float y = float(y0 + r) + 0.5f;
            for (int x = 0; x < W; ++x) {
                float sx, sy;
                map(float(x) + 0.5f, y, sx, sy);
                mx[x] = reflectCoord(sx - 0.5f, W);
                my[x] = reflectCoord(sy - 0.5f, H);
            }
        }
    }
    inline void map(float x, float y, float &sx, float &sy) const {
        const float dx = x - cx, dy = y - cy;
        const float r = std::sqrt(dx * dx + dy * dy);
        if (r < 1e-6f) { sx = cx; sy = cy; return; }
        const float u = (std::atan2(dy, dx) - a) / wedge;
        const float k = std::floor(u);
        float t = (u - k) * wedge;                  // angle into this wedge, [0, wedge)
        if (int(k) & 1) t = wedge - t;              // odd wedges are the mirror image
        const float phi = a + s + t;
        const float rs = r * inv;                   // the zoom: sample nearer the centre
        sx = cx + rs * std::cos(phi);
        sy = cy + rs * std::sin(phi);
    }
};

/* The MIP FOLDS (Hyperbolic, Polar, Droste) share one shape: map(x, y, px, py, foot)
   takes a point in output PIXEL coords (centres at +0.5) to the source PIXEL INDEX
   coords it samples (centres at integers, already inside the frame) and the
   footprint, and maxLevel caps the mips. applyMipFold runs any of them.

   The hyperbolic fold (Kaleido type 1): the source pixel coords and the FOOTPRINT --
   how many source pixels one output pixel spans there, which picks the mip level.
   Disk units: the disk is the unit circle about the centre, turned by -angle, so the
   fundamental triangle O-M-V has its 180/p corner O at the origin, its right angle M
   (the central polygon's edge midpoint) on the +x axis and its 180/q corner V (a
   polygon corner) on the ray at 180/p. Its three mirrors: the x axis, the ray to V,
   and the polygon edge -- a circle orthogonal to the rim. */
struct HyperFold {
    static constexpr int kMaxReflections = 400;
    double cx, cy, R;            // centre and disk radius, output pixels
    double ca, sa;               // cos / sin of the angle (output -> disk)
    double cs, ss;               // cos / sin of angle + source (triangle -> photo)
    double inv;                  // 1 / zoom: the source radius factor
    double nx, ny;               // outward normal of the mirror through O and V
    double ec, er2;              // the edge mirror: circle centre (ec, 0), radius^2
    int maxLevel;                // the coarsest mip level worth sampling
    int iw, ih;                  // the frame, for the border reflection
    HyperFold(const CropTransform::Kaleido &k, double W, double H) : iw(int(W)), ih(int(H)) {
        const double pi = 3.141592653589793;
        const int p = std::max(3, k.segments / 2);
        const int q = std::max(k.vertex, EditParams::kaleidoMinVertex(p));
        cx = k.x * W;
        cy = k.y * H;
        R = std::max(1.0, std::clamp(k.radius, 0.25, 4.0) * 0.5 * std::min(W, H));
        const double a = k.angle * 0.017453292519943295;
        const double s = (k.angle + k.source) * 0.017453292519943295;
        ca = std::cos(a); sa = std::sin(a);
        cs = std::cos(s); ss = std::sin(s);
        inv = 1.0 / std::clamp(k.zoom, 0.25, 8.0);
        nx = -std::sin(pi / p);
        ny = std::cos(pi / p);
        /* The right triangle: cosh|OM| = cos(pi/q) / sin(pi/p), and a hyperbolic
           distance d lies at tanh(d / 2) in the disk. The edge circle passes through
           M = (m, 0), centred on the x axis, orthogonal to the rim: (m + r)^2 = r^2 + 1. */
        const double m = std::tanh(0.5 * std::acosh(std::cos(pi / q) / std::sin(pi / p)));
        const double r = (1.0 - m * m) / (2.0 * m);
        ec = m + r;
        er2 = r * r;
        /* Beyond about the slice's own size a coarser level would blend in photo the
           tiling never shows, so the mips stop there. */
        maxLevel = std::max(0, int(std::floor(std::log2(std::max(1.0, m * R * inv)))));
    }
    inline void map(double x, double y, double &sx, double &sy, double &foot) const {
        const double dx = x - cx, dy = y - cy;
        double u = (dx * ca + dy * sa) / R;
        double v = (-dx * sa + dy * ca) / R;
        /* Each reflection in a circle multiplies the scale by its local magnification
           (r^2 / d^2), so scale ends as source pixels per output pixel, before zoom. */
        double scale = 1.0;
        const double r2 = u * u + v * v;
        if (r2 > 1.0) { u /= r2; v /= r2; scale /= r2; }   // beyond the rim: its mirror
        int i = 0;
        for (; i < kMaxReflections; ++i) {
            bool moved = false;
            if (v < 0.0) { v = -v; moved = true; }
            const double dn = u * nx + v * ny;
            if (dn > 0.0) { u -= 2.0 * dn * nx; v -= 2.0 * dn * ny; moved = true; }
            const double ex = u - ec, e2 = ex * ex + v * v;
            if (e2 < er2) {
                const double f = er2 / e2;
                u = ec + ex * f;
                v *= f;
                scale *= f;
                moved = true;
            }
            if (!moved) break;
        }
        const double kk = R * inv;
        sx = reflectCoord(float(cx + kk * (u * cs - v * ss) - 0.5), iw);
        sy = reflectCoord(float(cy + kk * (u * ss + v * cs) - 0.5), ih);
        /* Unresolved after the cap: right at the rim, where only the average shows. */
        foot = (i == kMaxReflections) ? 1.0e9 : scale * inv;
    }
};

/* The polar fold (Kaleido type 2, a "tiny planet"): the whole frame is wrapped round
   the centre -- its width along the angle, its height along the radius, the bottom row
   at the centre and the top row on a circle of zoom x half the short side (inside out:
   the top row at the centre). copies = segments / 2 go round the circle; each is the
   frame followed by its mirror image (the Mirror type's alternate reflection, so no
   seam), or with wrap the frame alone (for a 360-degree panorama whose ends meet).
   Source slides the frame along the angle. Beyond the circle the frame is reflected.
   The footprint is the geometric mean of the angular and radial scales. */
struct PolarFold {
    double cx, cy, Rp, ca, sa, shift;
    int n, iw, ih, maxLevel;
    bool wrap, insideOut;
    PolarFold(const CropTransform::Kaleido &k, double W, double H)
        : iw(int(W)), ih(int(H)) {
        cx = k.x * W;
        cy = k.y * H;
        Rp = std::max(1.0, std::clamp(k.zoom, 0.25, 8.0) * 0.5 * std::min(W, H));
        const double a = k.angle * 0.017453292519943295;
        ca = std::cos(a); sa = std::sin(a);
        shift = k.source / 360.0;
        n = std::max(1, k.segments / 2);
        wrap = k.wrap;
        insideOut = k.invert;
        /* The centre averages a ring of the frame's width, not the whole frame. */
        maxLevel = std::max(0, int(std::floor(std::log2(std::max(1.0, std::min(W, H) / 8.0)))));
    }
    inline void map(double x, double y, double &sx, double &sy, double &foot) const {
        const double twoPi = 6.283185307179586;
        const double dx = x - cx, dy = y - cy;
        const double u = (dx * ca + dy * sa) / Rp, v = (-dx * sa + dy * ca) / Rp;
        const double rho = std::sqrt(u * u + v * v);
        double th = std::atan2(v, u);
        if (th < 0.0) th += twoPi;                         // 0 .. 2pi from the spoke
        double fx;
        if (wrap) {
            double t = th * n / twoPi + shift;
            t -= std::floor(t);
            sx = std::min(t * iw - 0.5, double(iw) - 1.0);
            sx = std::max(sx, 0.0);
            fx = iw * n / twoPi;
        }
        else {
            const double w = th * (2 * n) / twoPi;          // wedges of pi / n
            const double kf = std::floor(w);
            double t = w - kf;
            if (int(kf) & 1) t = 1.0 - t;                   // odd wedges: the mirror image
            sx = reflectCoord(float((t + shift) * iw - 0.5), iw);
            fx = iw * (2 * n) / twoPi;
        }
        const double ry = insideOut ? rho : 1.0 - rho;
        sy = reflectCoord(float(ry * ih - 0.5), ih);
        /* Source pixels per output pixel: along the angle W n / (2 pi rho Rp) (twice
           that mirrored), along the radius H / Rp. */
        const double fa = fx / (std::max(rho, 1e-6) * Rp), fr = ih / Rp;
        foot = std::sqrt(fa * fr);
    }
};

/* The Droste fold (Kaleido type 3, Escher's "Print Gallery" spiral). The photo is
   nested in itself about the centre -- a copy at nest x the size in the middle, and so
   on without end -- and log-polar coords are twisted so that once round the centre
   steps strands = segments / 2 nestings: z -> z^alpha, alpha = 1 -+ i strands L / 2pi,
   L = ln(1 / nest). The nested photo repeats under a scale by 1 / nest, so the result
   is seamless. Each sample is scaled back into the ANNULUS (inside the frame, outside
   the nested copy). Invert turns the spiral the other way; zoom slides along it. */
struct DrosteFold {
    double cx, cy, ca, sa, cs, ss, inv, s, L, ai, absAlpha, target;
    double fx0, fx1, fy0, fy1;               // the frame about the centre, pixels
    int iw, ih, maxLevel;
    DrosteFold(const CropTransform::Kaleido &k, double W, double H)
        : iw(int(W)), ih(int(H)) {
        const double pi = 3.141592653589793;
        cx = k.x * W;
        cy = k.y * H;
        const double a = k.angle * 0.017453292519943295;
        const double b = (k.angle + k.source) * 0.017453292519943295;
        ca = std::cos(a); sa = std::sin(a);
        cs = std::cos(b); ss = std::sin(b);
        inv = 1.0 / std::clamp(k.zoom, 0.25, 8.0);
        s = 1.0 / std::clamp(k.nest, 0.05, 0.8);
        L = std::log(s);
        const int strands = std::max(1, k.segments / 2);
        ai = (k.invert ? 1.0 : -1.0) * strands * L / (2.0 * pi);
        absAlpha = std::sqrt(1.0 + ai * ai);
        /* The frame must hold a neighbourhood of the centre, or no nesting contains it. */
        fx0 = std::min(-cx, -1.0); fx1 = std::max(W - cx, 1.0);
        fy0 = std::min(-cy, -1.0); fy1 = std::max(H - cy, 1.0);
        target = std::log(0.5 * std::min(fx1 - fx0, fy1 - fy0));
        maxLevel = std::max(0, int(std::floor(std::log2(std::max(1.0, std::min(W, H) / 8.0)))));
    }
    inline bool inFrame(double x, double y, double f) const {
        return x >= fx0 * f && x < fx1 * f && y >= fy0 * f && y < fy1 * f;
    }
    inline void map(double x, double y, double &sx, double &sy, double &foot) const {
        const double dx = x - cx, dy = y - cy;
        const double u = (dx * ca + dy * sa) * inv, v = (-dx * sa + dy * ca) * inv;
        const double r2 = u * u + v * v;
        if (r2 < 1e-12) {                    // the centre itself: every nesting at once
            sx = std::clamp(cx - 0.5, 0.0, iw - 1.0);
            sy = std::clamp(cy - 0.5, 0.0, ih - 1.0);
            foot = 1.0e9;
            return;
        }
        const double lr = 0.5 * std::log(r2), th = std::atan2(v, u);
        /* (1 + i ai)(lr + i th), then a whole number of nestings off the real part so
           the point lands near the frame's size; the loop finishes the job. */
        double re = lr - ai * th;
        const double im = th + ai * lr;
        re -= std::floor((re - target) / L) * L;
        const double mag = std::exp(re);
        const double wx = mag * std::cos(im), wy = mag * std::sin(im);
        double px = wx * cs - wy * ss, py = wx * ss + wy * cs;
        double scale = 1.0;
        for (int i = 0; i < 64; ++i) {
            if (!inFrame(px, py, 1.0)) { px /= s; py /= s; scale /= s; }
            else if (inFrame(px, py, 1.0 / s)) { px *= s; py *= s; scale *= s; }
            else break;
        }
        sx = std::clamp(cx + px - 0.5, 0.0, iw - 1.0);
        sy = std::clamp(cy + py - 0.5, 0.0, ih - 1.0);
        /* |d w / d z| = |alpha| |w| / |z|, times the zoom's 1 / zoom. */
        foot = absAlpha * mag * scale / std::sqrt(r2) * inv;
    }
};

/* The wallpaper fold (Kaleido type 4): one of the 17 wallpaper groups tiles the plane
   about the centre. Output -> cell units (turned by -angle, divided by the tile size
   T); the group reduces the point to ONE canonical point of its orbit (inside the
   group's fundamental region), which samples the photo as the Mirror wedge does:
   centre + T / zoom x turn(angle + source). Square-lattice groups (0..11) work in
   fractional cell coords with the kaleidoscope's centre at a cell centre; hexagonal
   groups (12..16) subtract the nearest lattice point (all five are symmorphic, so the
   point group about it does the rest) and fold the angle. The mirror groups are
   seamless; groups with turns or glides but no mirror show where copies of the slice
   meet, as any such pattern made from a photo does. No minification: the footprint
   is just 1 / zoom. */
struct WallpaperFold {
    double cx, cy, T, ca, sa, cs, ss, inv;
    int g, iw, ih, maxLevel;
    WallpaperFold(const CropTransform::Kaleido &k, double W, double H)
        : iw(int(W)), ih(int(H)) {
        cx = k.x * W;
        cy = k.y * H;
        T = std::max(2.0, std::clamp(k.tile, 0.02, 1.0) * std::min(W, H));
        const double a = k.angle * 0.017453292519943295;
        const double b = (k.angle + k.source) * 0.017453292519943295;
        ca = std::cos(a); sa = std::sin(a);
        cs = std::cos(b); ss = std::sin(b);
        inv = 1.0 / std::clamp(k.zoom, 0.25, 8.0);
        g = std::clamp(k.group, 0, 16);
        maxLevel = 2;                                   // zoom down to 25%
    }
    /* Square-lattice groups, fractional cell coords (x, y) -> the canonical point. */
    static void squareRep(int g, double &x, double &y) {
        x -= std::floor(x);
        y -= std::floor(y);
        auto frac = [](double t) { return t - std::floor(t); };
        switch (g) {
        case 0:                                          // p1: translations only
            break;
        case 1:                                          // p2: half turn about (1/2, 1/2)
            if (y > 0.5) { x = 1.0 - x; y = 1.0 - y; }
            break;
        case 2:                                          // pm: mirrors x = 0, 1/2
            if (x > 0.5) x = 1.0 - x;
            break;
        case 3:                                          // pg: glide (x + 1/2, -y)
            if (y > 0.5) { x = frac(x + 0.5); y = 1.0 - y; }
            break;
        case 4:                                          // cm: centring + mirror
            if (y > 0.5) { x = frac(x + 0.5); y -= 0.5; }
            if (x > 0.5) x = 1.0 - x;
            break;
        case 5:                                          // pmm: two mirror families
            if (x > 0.5) x = 1.0 - x;
            if (y > 0.5) y = 1.0 - y;
            break;
        case 6:                                          // pmg: glide, then mirror
            if (y > 0.5) { x = frac(x + 0.5); y = 1.0 - y; }
            if (x > 0.5) x = 1.0 - x;
            break;
        case 7:                                          // pgg: two crossed glides
            if (y >= 0.5) { x = frac(0.5 - x); y -= 0.5; }
            if (x >= 0.5) { x -= 0.5; y = 0.5 - y; }
            break;
        case 8:                                          // cmm: centring + two mirrors
            if (y > 0.5) { x = frac(x + 0.5); y -= 0.5; }
            if (y > 0.25) { x = frac(x + 0.5); y = 0.5 - y; }
            if (x > 0.5) x = 1.0 - x;
            break;
        default: {                                       // p4 / p4m / p4g
            double u = x - 0.5, v = y - 0.5;             // about a 4-fold centre
            for (int i = 0; i < 4 && !(u >= 0.0 && v >= 0.0); ++i) {
                const double t = u; u = -v; v = t;       // quarter turn
            }
            if (g == 10 && v > u) std::swap(u, v);       // p4m: the diagonal mirror
            if (g == 11 && u + v > 0.5) {                // p4g: mirror u + v = 1/2
                const double t = u; u = 0.5 - v; v = 0.5 - t;
            }
            x = u + 0.5;
            y = v + 0.5;
        }
        }
    }
    /* Hexagonal groups, Cartesian cell units about a lattice point (lattice (1, 0),
       (1/2, sqrt3/2)) -> the canonical point. */
    static void hexRep(int g, double &u, double &v) {
        const double h = 0.8660254037844386;            // sqrt(3) / 2
        const double j0 = std::floor(v / h), i0 = std::floor(u - (v / h) * 0.5);
        double best = 1e300, bu = u, bv = v;
        for (int dj = 0; dj <= 1; ++dj) {
            for (int di = 0; di <= 1; ++di) {
                const double j = j0 + dj, i = i0 + di;
                const double du = u - (i + 0.5 * j), dv = v - h * j;
                const double d = du * du + dv * dv;
                if (d < best) { best = d; bu = du; bv = dv; }
            }
        }
        const double pi3 = 1.0471975511965976;          // 60 degrees
        const double r = std::sqrt(best);
        double th = std::atan2(bv, bu);
        auto modp = [](double t, double p) { t = std::fmod(t, p); return t < 0.0 ? t + p : t; };
        switch (g) {
        case 12: th = modp(th, 2.0 * pi3); break;                    // p3
        case 13:                                                     // p3m1: mirrors 30/90/150
            th = 0.5 * pi3 + modp(th - 0.5 * pi3, 2.0 * pi3);
            if (th > 1.5 * pi3) th = 3.0 * pi3 - th;
            break;
        case 14:                                                     // p31m: mirrors 0/60/120
            th = modp(th, 2.0 * pi3);
            if (th > pi3) th = 2.0 * pi3 - th;
            break;
        case 15: th = modp(th, pi3); break;                          // p6
        default:                                                     // p6m
            th = modp(th, pi3);
            if (th > 0.5 * pi3) th = pi3 - th;
        }
        u = r * std::cos(th);
        v = r * std::sin(th);
    }
    inline void map(double x, double y, double &sx, double &sy, double &foot) const {
        const double dx = x - cx, dy = y - cy;
        double u = (dx * ca + dy * sa) / T, v = (-dx * sa + dy * ca) / T;
        if (g >= 12) hexRep(g, u, v);
        else {
            double fx = u + 0.5, fy = v + 0.5;
            squareRep(g, fx, fy);
            u = fx - 0.5;
            v = fy - 0.5;
        }
        const double kk = T * inv;
        sx = reflectCoord(float(cx + kk * (u * cs - v * ss) - 0.5), iw);
        sy = reflectCoord(float(cy + kk * (u * ss + v * cs) - 0.5), ih);
        foot = inv;
    }
};

/* The fluted-glass fold (Kaleido type 5): the frame seen through reeded glass. Reeds
   of width w (flute x the short side) run SQUARE to the angle (the spoke points across
   them, so at angle 0 they are vertical), one centred on the centre;
   each is a cylindrical lens, so across a reed (t = -1/2 .. 1/2) the sample sits at
   the reed's centre + w x (t + 3 bend q(t)), q the barrel profile (t + sin(pi t) / 2)
   / 2 (q(1/2) = 1/2, steepest in the middle like a cylinder). bend 0 is flat glass
   (the photo untouched), 1 squeezes four reeds' width of the photo into each reed,
   -1 shows each reed's strip reversed. Along the reed nothing moves. gain() shades each reed as if lit from one
   side, dark to bright across it and back to 1 at its edges. Source turns and zoom
   scales what lies behind the glass, as elsewhere. */
struct FluteFold {
    double cx, cy, w, ca, sa, cs, ss, inv, bend3, shade;
    int iw, ih, maxLevel;
    FluteFold(const CropTransform::Kaleido &k, double W, double H)
        : iw(int(W)), ih(int(H)) {
        cx = k.x * W;
        cy = k.y * H;
        w = std::max(2.0, std::clamp(k.flute, 0.01, 0.25) * std::min(W, H));
        const double a = k.angle * 0.017453292519943295;
        const double b = (k.angle + k.source) * 0.017453292519943295;
        ca = std::cos(a); sa = std::sin(a);
        cs = std::cos(b); ss = std::sin(b);
        inv = 1.0 / std::clamp(k.zoom, 0.25, 8.0);
        bend3 = 3.0 * std::clamp(k.bend, -1.0, 1.0);
        shade = std::clamp(k.shade, 0.0, 1.0);
        maxLevel = 3;
    }
    /* Across-reed position: the reed index and t in [-1/2, 1/2). */
    inline void reed(double x, double y, double &along, double &across, double &kf,
                     double &t) const {
        const double dx = x - cx, dy = y - cy;
        across = dx * ca + dy * sa;
        along = -dx * sa + dy * ca;
        const double r = across / w;
        kf = std::floor(r + 0.5);
        t = r - kf;
    }
    inline void map(double x, double y, double &sx, double &sy, double &foot) const {
        const double pi = 3.141592653589793;
        double along, across, kf, t;
        reed(x, y, along, across, kf, t);
        const double q = 0.5 * (t + 0.5 * std::sin(pi * t));
        const double dq = 0.5 * (1.0 + 0.5 * pi * std::cos(pi * t));
        const double b = (kf + t + bend3 * q) * w;
        sx = reflectCoord(float(cx + inv * (b * cs - along * ss) - 0.5), iw);
        sy = reflectCoord(float(cy + inv * (b * ss + along * cs) - 0.5), ih);
        /* Across, |1 + 3 bend q'| source pixels per output pixel; along, 1: their mean. */
        foot = std::sqrt(std::max(std::abs(1.0 + bend3 * dq), 1e-3)) * inv;
    }
    inline float gain(double x, double y) const {
        if (shade <= 0.0) return 1.0f;
        double along, across, kf, t;
        reed(x, y, along, across, kf, t);
        return float(1.0 + 0.3 * shade * std::sin(2.0 * 3.141592653589793 * t));
    }
};

/* A mip pyramid (cv::pyrDown, 8-bit or 16-bit, 1 or 4 channels) and a trilinear sample
   from it, for the hyperbolic fold's shrinking copies. x, y are level-0 pixel coords
   (centres at integers); level L samples at (x + 0.5) / 2^L - 0.5. */
struct MipSampler {
    std::vector<cv::Mat> lv;
    int cn = 4;
    bool deep = false;
    MipSampler(const cv::Mat &base, int maxLevel) {
        lv.push_back(base);
        while (int(lv.size()) <= maxLevel && lv.back().cols > 1 && lv.back().rows > 1) {
            cv::Mat d;
            cv::pyrDown(lv.back(), d);
            lv.push_back(d);
        }
        cn = base.channels();
        deep = base.depth() == CV_16U;
    }
    template <typename T>
    void bilinear(const cv::Mat &m, double x, double y, float *o) const {
        x = std::clamp(x, 0.0, double(m.cols - 1));
        y = std::clamp(y, 0.0, double(m.rows - 1));
        const int x0 = int(x), y0 = int(y);
        const int x1 = std::min(x0 + 1, m.cols - 1), y1 = std::min(y0 + 1, m.rows - 1);
        const float fx = float(x - x0), fy = float(y - y0);
        const T *r0 = m.ptr<T>(y0);
        const T *r1 = m.ptr<T>(y1);
        for (int c = 0; c < cn; ++c) {
            const float t = r0[x0 * cn + c] + (r0[x1 * cn + c] - float(r0[x0 * cn + c])) * fx;
            const float b = r1[x0 * cn + c] + (r1[x1 * cn + c] - float(r1[x0 * cn + c])) * fx;
            o[c] = t + (b - t) * fy;
        }
    }
    void tap(int L, double x, double y, float *o) const {
        const double sc = std::ldexp(1.0, -L);
        const double lx = (x + 0.5) * sc - 0.5, ly = (y + 0.5) * sc - 0.5;
        if (deep) bilinear<quint16>(lv[size_t(L)], lx, ly, o);
        else      bilinear<uchar>(lv[size_t(L)], lx, ly, o);
    }
    /* foot: source pixels per output pixel; at or below 1 a plain bilinear tap. */
    void sample(double x, double y, double foot, float *o) const {
        const int top = int(lv.size()) - 1;
        const double level = foot > 1.0 ? std::min(std::log2(foot), double(top)) : 0.0;
        const int l0 = int(level);
        const float t = float(level - l0);
        tap(l0, x, y, o);
        if (t > 0.001f && l0 < top) {
            float o2[4];
            tap(l0 + 1, x, y, o2);
            for (int c = 0; c < cn; ++c) o[c] += (o2[c] - o[c]) * t;
        }
    }
};

/* A mip fold, whole frame (mask == nullptr) or masked: the masked form folds colour
   and coverage with the same map and blends by the folded coverage, exactly as
   applyKaleidoscopeMasked does. Every row on its own task; no maps. */
template <typename Fold>
QImage applyMipFold(const QImage &frame, const QImage *mask, const CropTransform::Kaleido &k)
{
    const int W = frame.width(), H = frame.height();
    const bool deep = isDeep(frame);
    const cv::Mat in = deep ? toRgba64(frame) : toBgra(frame);
    QImage g8;
    cv::Mat m;
    if (mask) {
        g8 = mask->convertToFormat(QImage::Format_Grayscale8);
        m = cv::Mat(H, W, CV_8U, const_cast<uchar *>(g8.bits()), g8.bytesPerLine());
    }
    QImage out(W, H, deep ? QImage::Format_RGBA64 : QImage::Format_ARGB32);
    cv::Mat dst(H, W, deep ? CV_16UC4 : CV_8UC4, out.bits(), out.bytesPerLine());

    const Fold fold(k, W, H);
    const MipSampler colour(in, fold.maxLevel);
    const MipSampler cover(mask ? m : cv::Mat(1, 1, CV_8U, cv::Scalar(255)),
                           mask ? fold.maxLevel : 0);
    const float op = float(std::clamp(k.opacity, 0.0, 1.0));
    cv::parallel_for_(cv::Range(0, H), [&](const cv::Range &range) {
        float c[4], a[4];
        for (int y = range.start; y < range.end; ++y) {
            for (int x = 0; x < W; ++x) {
                double px, py, foot;
                fold.map(x + 0.5, y + 0.5, px, py, foot);
                colour.sample(px, py, foot, c);
                /* A fold may shade what it shows (fluted glass): colour, not alpha. */
                if constexpr (requires { fold.gain(0.0, 0.0); }) {
                    const float gn = fold.gain(x + 0.5, y + 0.5);
                    for (int ch = 0; ch < 3; ++ch) c[ch] *= gn;
                }
                float t = op;
                if (mask) {
                    cover.sample(px, py, foot, a);
                    t *= a[0] * (1.0f / 255.0f);
                }
                if (deep) {
                    const quint16 *b = in.ptr<quint16>(y) + x * 4;
                    quint16 *o = dst.ptr<quint16>(y) + x * 4;
                    for (int ch = 0; ch < 4; ++ch)
                        o[ch] = cv::saturate_cast<quint16>(b[ch] + (c[ch] - b[ch]) * t);
                }
                else {
                    const uchar *b = in.ptr<uchar>(y) + x * 4;
                    uchar *o = dst.ptr<uchar>(y) + x * 4;
                    for (int ch = 0; ch < 4; ++ch)
                        o[ch] = cv::saturate_cast<uchar>(b[ch] + (c[ch] - b[ch]) * t);
                }
            }
        }
    });
    return out;
}

/* The Kaleido types that run through applyMipFold (all but Mirror). */
QImage applyMipType(const QImage &frame, const QImage *mask, const CropTransform::Kaleido &k)
{
    switch (k.type) {
    case 2:  return applyMipFold<PolarFold>(frame, mask, k);
    case 3:  return applyMipFold<DrosteFold>(frame, mask, k);
    case 4:  return applyMipFold<WallpaperFold>(frame, mask, k);
    case 5:  return applyMipFold<FluteFold>(frame, mask, k);
    default: return applyMipFold<HyperFold>(frame, mask, k);
    }
}

} // namespace

QImage CropTransform::rectifyPerspective(const QImage &src, const QPointF quad[4],
                                         QRectF &outCropNorm)
{
    if (src.isNull()) return QImage();

    std::vector<cv::Point2f> srcPts = {
        {float(quad[0].x()), float(quad[0].y())},
        {float(quad[1].x()), float(quad[1].y())},
        {float(quad[2].x()), float(quad[2].y())},
        {float(quad[3].x()), float(quad[3].y())}
    };

    /* Destination rectangle size = average of opposite edge lengths (preserves the selected
       region's rough scale). Reject a degenerate quad. */
    const double wTop = dist(srcPts[0], srcPts[1]);
    const double wBot = dist(srcPts[3], srcPts[2]);
    const double hLft = dist(srcPts[0], srcPts[3]);
    const double hRgt = dist(srcPts[1], srcPts[2]);
    const double W = (wTop + wBot) / 2.0;
    const double H = (hLft + hRgt) / 2.0;
    if (W < 2.0 || H < 2.0) return QImage();

    const std::vector<cv::Point2f> dstPts = {
        {0.f, 0.f}, {float(W), 0.f}, {float(W), float(H)}, {0.f, float(H)}
    };
    const cv::Mat Hm = cv::getPerspectiveTransform(srcPts, dstPts);

    /* Warp the whole image: find the bounds of the four image corners under Hm so nothing is
       clipped, then translate the transform into a positive-origin output canvas. */
    const std::vector<cv::Point2f> imgCorners = {
        {0.f, 0.f}, {float(src.width()), 0.f},
        {float(src.width()), float(src.height())}, {0.f, float(src.height())}
    };
    std::vector<cv::Point2f> warpedCorners;
    cv::perspectiveTransform(imgCorners, warpedCorners, Hm);

    float minX = 1e9f, minY = 1e9f, maxX = -1e9f, maxY = -1e9f;
    for (const auto &p : warpedCorners) {
        minX = std::min(minX, p.x); minY = std::min(minY, p.y);
        maxX = std::max(maxX, p.x); maxY = std::max(maxY, p.y);
    }
    /* Clamp the output canvas so an extreme/near-degenerate warp can't blow up memory. */
    const int outW = std::min(20000, std::max(2, int(std::ceil(maxX - minX))));
    const int outH = std::min(20000, std::max(2, int(std::ceil(maxY - minY))));

    cv::Mat T = (cv::Mat_<double>(3, 3) << 1, 0, -double(minX),
                                            0, 1, -double(minY),
                                            0, 0, 1);
    const cv::Mat Hfull = T * Hm;

    /* Warp at the SOURCE's bit depth: a 16-bit export image goes through a CV_16UC4
       warp, an 8-bit one through CV_8UC4 as before. Routing everything through 8 bits
       here would silently flatten a 16-bit export as soon as the recipe held a warp. */
    const bool deep = isDeep(src);
    cv::Mat dst;
    cv::warpPerspective(deep ? toRgba64(src) : toBgra(src), dst, Hfull,
                        cv::Size(outW, outH), cv::INTER_LINEAR, cv::BORDER_CONSTANT,
                        cv::Scalar(0, 0, 0, 0));

    /* Validity mask = solidly opaque pixels (drop the interpolated semi-transparent
       fringe). The largest-inscribed-rectangle search runs on a downscaled mask for
       speed, then scales back. */
    std::vector<cv::Mat> ch;
    cv::split(dst, ch);
    cv::Mat mask = ch[3] >= (deep ? 250 * 257 : 250);   // same cut-off at either depth

    const int kMaxSearch = 1000;
    const double s = std::min(1.0, double(kMaxSearch) / std::max(outW, outH));
    cv::Rect best;
    if (s < 1.0) {
        cv::Mat lowRes;
        cv::resize(mask, lowRes, cv::Size(), s, s, cv::INTER_NEAREST);
        const cv::Rect r = largestValidRect(lowRes);
        best = cv::Rect(int(r.x / s), int(r.y / s),
                        int(r.width / s), int(r.height / s));
    } else {
        best = largestValidRect(mask);
    }
    best &= cv::Rect(0, 0, outW, outH);
    if (best.width <= 0 || best.height <= 0) best = cv::Rect(0, 0, outW, outH);

    outCropNorm = QRectF(double(best.x) / outW, double(best.y) / outH,
                         double(best.width) / outW, double(best.height) / outH);

    /* cv::Mat -> QImage, same byte order in both cases (BGRA -> ARGB32's little-endian
       bytes; RGBA-16 -> RGBA64). */
    QImage out(dst.cols, dst.rows, deep ? QImage::Format_RGBA64 : QImage::Format_ARGB32);
    const size_t rowBytes = size_t(dst.cols) * (deep ? 8 : 4);
    for (int y = 0; y < dst.rows; ++y)
        std::memcpy(out.scanLine(y), dst.ptr(y), rowBytes);
    return out;
}

QImage CropTransform::applyGeometry(const QImage &src, const Geometry &g)
{
    if (src.isNull() || g.isIdentity()) return src;

    QImage img = src;

    /* 1) Straighten: rotate about the centre, expanding the canvas (the crop trims the
       corners). Convert to an ALPHA format first so the exposed wedges are TRANSPARENT
       (not black), matching the warp and letting straightenCropNorm / the view background
       behave. A 16-bit (export) image takes RGBA64 so the rotation does not flatten it to
       8 bits; everything else keeps ARGB32. */
    if (g.straighten != 0.0) {
        QTransform t;
        t.rotate(g.straighten);
        const QImage::Format alphaFmt =
            isDeep(img) ? QImage::Format_RGBA64 : QImage::Format_ARGB32;
        img = img.convertToFormat(alphaFmt).transformed(t, Qt::SmoothTransformation);
    }

    /* 2) Warp: rectify the 4-point quad (its corners are in this image's normalized space). */
    if (g.hasWarp) {
        QPointF quadPx[4];
        for (int i = 0; i < 4; ++i)
            quadPx[i] = QPointF(g.quad[i * 2] * img.width(), g.quad[i * 2 + 1] * img.height());
        QRectF ignore;
        const QImage warped = rectifyPerspective(img, quadPx, ignore);
        if (!warped.isNull()) img = warped;
    }

    /* 3) Crop: g.crop is normalized in the post-warp output space. */
    if (!g.cropIsIdentity()) {
        const QRect cri = QRectF(g.cropX * img.width(),  g.cropY * img.height(),
                                 g.cropW * img.width(),  g.cropH * img.height())
                              .toRect().intersected(img.rect());
        if (cri.isValid() && cri != img.rect()) img = img.copy(cri);
    }
    return img;
}

QImage CropTransform::applyKaleidoscope(const QImage &src, const Kaleido &k)
{
    if (src.isNull() || !k.on()) return src;
    const int W = src.width(), H = src.height();
    if (W < 2 || H < 2) return src;
    if (k.usesMips()) return applyMipType(src, nullptr, k);

    const bool deep = isDeep(src);
    const cv::Mat in = deep ? toRgba64(src) : toBgra(src);
    QImage out(W, H, deep ? QImage::Format_RGBA64 : QImage::Format_ARGB32);
    cv::Mat dst(H, W, deep ? CV_16UC4 : CV_8UC4, out.bits(), out.bytesPerLine());

    /* Strips, each building its own small coordinate maps and remapping straight into
       its rows of dst: no full-frame map pair (2 x 4 bytes x pixels), and every strip
       runs on its own thread. */
    const KaleidoFold fold(k, W, H);
    const double op = std::clamp(k.opacity, 0.0, 1.0);
    const int kStrip = 32;
    const int nStrips = (H + kStrip - 1) / kStrip;
    cv::parallel_for_(cv::Range(0, nStrips), [&](const cv::Range &range) {
        cv::Mat mapX, mapY;
        for (int si = range.start; si < range.end; ++si) {
            const int y0 = si * kStrip;
            const int rows = std::min(kStrip, H - y0);
            fold.strip(y0, rows, W, H, mapX, mapY);
            cv::Mat roi = dst.rowRange(y0, y0 + rows);
            cv::remap(in, roi, mapX, mapY, cv::INTER_LINEAR, cv::BORDER_REPLICATE);
            /* Below full opacity: lerp the strip back toward the unfolded frame. */
            if (op < 1.0)
                cv::addWeighted(roi, op, in.rowRange(y0, y0 + rows), 1.0 - op, 0.0, roi);
        }
    });
    return out;
}

QImage CropTransform::applyKaleidoscopeMasked(const QImage &frame, const QImage &mask,
                                              const Kaleido &k)
{
    if (frame.isNull() || !k.on() || mask.size() != frame.size()) return frame;
    const int W = frame.width(), H = frame.height();
    if (W < 2 || H < 2) return frame;
    if (k.usesMips()) return applyMipType(frame, &mask, k);

    const bool deep = isDeep(frame);
    const cv::Mat in = deep ? toRgba64(frame) : toBgra(frame);
    const QImage g8 = mask.convertToFormat(QImage::Format_Grayscale8);
    const cv::Mat m(H, W, CV_8U, const_cast<uchar *>(g8.bits()), g8.bytesPerLine());
    QImage out(W, H, deep ? QImage::Format_RGBA64 : QImage::Format_ARGB32);
    cv::Mat dst(H, W, deep ? CV_16UC4 : CV_8UC4, out.bits(), out.bytesPerLine());

    /* Same strips as applyKaleidoscope; each folds its rows of colour AND coverage with
       one map pair, then blends: out = frame + (fold(frame) - frame) * fold(mask). */
    const KaleidoFold fold(k, W, H);
    /* The opacity scales the folded coverage: 0..255 as an integer weight. */
    const int op255 = int(std::clamp(k.opacity, 0.0, 1.0) * 255.0 + 0.5);
    const int kStrip = 32;
    const int nStrips = (H + kStrip - 1) / kStrip;
    cv::parallel_for_(cv::Range(0, nStrips), [&](const cv::Range &range) {
        cv::Mat mapX, mapY, fc, fm;
        for (int si = range.start; si < range.end; ++si) {
            const int y0 = si * kStrip;
            const int rows = std::min(kStrip, H - y0);
            fold.strip(y0, rows, W, H, mapX, mapY);
            cv::remap(in, fc, mapX, mapY, cv::INTER_LINEAR, cv::BORDER_REPLICATE);
            cv::remap(m,  fm, mapX, mapY, cv::INTER_LINEAR, cv::BORDER_REPLICATE);
            for (int r = 0; r < rows; ++r) {
                const uchar *a = fm.ptr<uchar>(r);
                if (deep) {
                    const quint16 *f = fc.ptr<quint16>(r);
                    const quint16 *b = in.ptr<quint16>(y0 + r);
                    quint16 *o = dst.ptr<quint16>(y0 + r);
                    for (int x = 0; x < W; ++x) {
                        const float t = a[x] * op255 * (1.0f / (255.0f * 255.0f));
                        for (int c = 0; c < 4; ++c) {
                            const int i = x * 4 + c;
                            o[i] = quint16(b[i] + (float(f[i]) - b[i]) * t + 0.5f);
                        }
                    }
                }
                else {
                    const uchar *f = fc.ptr<uchar>(r);
                    const uchar *b = in.ptr<uchar>(y0 + r);
                    uchar *o = dst.ptr<uchar>(y0 + r);
                    for (int x = 0; x < W; ++x) {
                        const int t = (a[x] * op255 + 127) / 255;   // 0..255 blend
                        for (int c = 0; c < 4; ++c) {
                            const int i = x * 4 + c;
                            o[i] = uchar((b[i] * (255 - t) + f[i] * t + 127) / 255);
                        }
                    }
                }
            }
        }
    });
    return out;
}

QPointF CropTransform::kaleidoSourceNorm(QPointF outN, double W, double H,
                                         const Kaleido &k)
{
    if (!k.on() || W < 2.0 || H < 2.0) return outN;
    if (k.usesMips()) {
        /* These maps already land on source pixel indices inside the frame. */
        const double x = outN.x() * W, y = outN.y() * H;
        double px = 0.0, py = 0.0, foot;
        switch (k.type) {
        case 2:  PolarFold(k, W, H).map(x, y, px, py, foot); break;
        case 3:  DrosteFold(k, W, H).map(x, y, px, py, foot); break;
        case 4:  WallpaperFold(k, W, H).map(x, y, px, py, foot); break;
        case 5:  FluteFold(k, W, H).map(x, y, px, py, foot); break;
        default: HyperFold(k, W, H).map(x, y, px, py, foot); break;
        }
        return QPointF((px + 0.5) / W, (py + 0.5) / H);
    }
    float sx, sy;
    KaleidoFold(k, W, H).map(float(outN.x() * W), float(outN.y() * H), sx, sy);
    const int iw = int(W), ih = int(H);
    return QPointF((reflectCoord(sx - 0.5f, iw) + 0.5) / W,
                   (reflectCoord(sy - 0.5f, ih) + 0.5) / H);
}

QTransform CropTransform::geometryTransform(double srcW, double srcH, const Geometry &g,
                                            QSizeF *outSize)
{
    /* Mirrors applyGeometry step for step, in points instead of pixels. QTransform
       composes in APPLICATION order (p * A * B), so each stage appends on the right. */
    QTransform t;
    double W = srcW, H = srcH;
    if (W <= 0.0 || H <= 0.0) { if (outSize) *outSize = QSizeF(); return t; }

    /* 1) Straighten. QImage::transformed() rotates and then shifts the result into a
       positive-origin canvas -- the rotation centre drops out of that pair, so rotating
       about the origin and translating by -boundingRect.topLeft() gives the same map. */
    if (g.straighten != 0.0) {
        QTransform r;
        r.rotate(g.straighten);
        const QRectF bb = r.mapRect(QRectF(0.0, 0.0, W, H));
        QTransform shift;
        shift.translate(-bb.left(), -bb.top());
        t = t * r * shift;
        W = bb.width(); H = bb.height();
    }

    /* 2) Warp: the same quad -> upright-rectangle homography rectifyPerspective builds,
       plus its translation into the positive-origin output canvas. */
    if (g.hasWarp) {
        QPolygonF srcQuad;
        for (int i = 0; i < 4; ++i)
            srcQuad << QPointF(g.quad[i * 2] * W, g.quad[i * 2 + 1] * H);
        auto len = [](const QPointF &a, const QPointF &b) {
            return std::hypot(a.x() - b.x(), a.y() - b.y());
        };
        const double dW = (len(srcQuad[0], srcQuad[1]) + len(srcQuad[3], srcQuad[2])) / 2.0;
        const double dH = (len(srcQuad[0], srcQuad[3]) + len(srcQuad[1], srcQuad[2])) / 2.0;
        QTransform h;
        if (dW >= 2.0 && dH >= 2.0) {
            QPolygonF dstQuad;
            dstQuad << QPointF(0, 0) << QPointF(dW, 0) << QPointF(dW, dH) << QPointF(0, dH);
            if (QTransform::quadToQuad(srcQuad, dstQuad, h)) {
                const QRectF bb = h.mapRect(QRectF(0.0, 0.0, W, H));
                QTransform shift;
                shift.translate(-bb.left(), -bb.top());
                t = t * h * shift;
                W = std::min(20000.0, std::max(2.0, std::ceil(bb.width())));
                H = std::min(20000.0, std::max(2.0, std::ceil(bb.height())));
            }
        }
    }

    /* 3) Crop: g.crop is normalized in the post-warp output space. */
    if (!g.cropIsIdentity()) {
        QTransform c;
        c.translate(-g.cropX * W, -g.cropY * H);
        t = t * c;
        W *= g.cropW; H *= g.cropH;
    }

    if (outSize) *outSize = QSizeF(W, H);
    return t;
}

QRectF CropTransform::straightenCropNorm(double W, double H, double deg)
{
    if (W <= 0.0 || H <= 0.0 || deg == 0.0) return QRectF(0, 0, 1, 1);

    const double a = deg * 0.017453292519943295;   // radians
    const double c = std::abs(std::cos(a));
    const double s = std::abs(std::sin(a));
    const double canvasW = W * c + H * s;           // bounding box of the rotated frame
    const double canvasH = W * s + H * c;

    /* rotatedRectWithMaxArea: largest axis-aligned rect (canvas axes) of pure content. */
    const double longSide  = std::max(W, H);
    const double shortSide = std::min(W, H);
    const bool   wLonger   = (W >= H);
    double wr, hr;
    if (shortSide <= 2.0 * s * c * longSide || std::abs(s - c) < 1e-10) {
        const double x = 0.5 * shortSide;
        if (wLonger) { wr = (s > 1e-9) ? x / s : W; hr = (c > 1e-9) ? x / c : H; }
        else         { wr = (c > 1e-9) ? x / c : W; hr = (s > 1e-9) ? x / s : H; }
    } else {
        const double cos2a = c * c - s * s;
        wr = (W * c - H * s) / cos2a;
        hr = (H * c - W * s) / cos2a;
    }
    wr = std::max(0.0, std::min(wr, canvasW));
    hr = std::max(0.0, std::min(hr, canvasH));

    return QRectF((canvasW - wr) / 2.0 / canvasW, (canvasH - hr) / 2.0 / canvasH,
                  wr / canvasW, hr / canvasH);
}
