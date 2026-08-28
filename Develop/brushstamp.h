#ifndef BRUSHSTAMP_H
#define BRUSHSTAMP_H

/*
    Shared brush-mask rasterization, used by both the ImageView live overlay (preview, output-
    oriented space) and the develop render (mainwindow buildMaskBuffer, work/pre-orientation space)
    so the two are pixel-identical. Header-only (all inline) -- no build-system entry needed.

    Model: each stroke is accumulated into its OWN coverage buffer by MAX of feathered dabs (so dab
    spacing / stroke speed do not change the result), then composited into the running mask with the
    stroke's flow: add  m = m + flow*cov*(1-m); erase m = m * (1 - flow*cov). Build-up therefore
    happens across strokes (and repeated passes), not within a single stroke.

    Stroke JSON: { pts:[x0,y0,x1,y1,...] normalized output coords, size, feather, flow (0..100),
                   erase, autoMask }. size = diameter as % of the image long edge (radius =
    size/200 * longEdge = the OUTER extent). feather (feather/100) softens INWARD
    Lightroom-style, through the shared MaskFalloff profile: feather=0 is a crisp edge at the
    size circle and feather=100 is fully soft, with the half-coverage radius pulling in as the
    feather rises and NO flat core in between (see coverage() and MaskFalloff).
*/

#include <vector>
#include <cmath>
#include <algorithm>
#include <memory>
#include "Develop/maskfalloff.h"
#include <QPointF>
#include <QJsonArray>
#include <QJsonObject>
#include <QHash>
#include <QString>
#include <QMutex>
#include <QVector>
#include <QFuture>
#include <QThreadPool>
#include <QtConcurrent>
#include <atomic>

namespace BrushStamp {

/* ---- Auto-mask guide ----
   A small COLOUR map of the displayed image (output-normalized orientation), shared between the
   ImageView live preview and the develop render so auto-masked strokes evaluate identically. Built
   once per image (ImageView::ensureAutoGuide) and registered by path.

   YCbCr, not luminance: the auto-mask has to separate a brown cone from a grey background of the
   SAME lightness, which a luminance band cannot do at all -- it is the chroma that differs. Y and
   the two chroma channels are stored interleaved as bytes (a few MB even at a high guide
   resolution, where floats would be tens of MB). */
struct Guide {
    std::vector<uchar> ycc;     // interleaved Y,Cb,Cr (0..255), row-major, output-oriented
    int w = 0, h = 0;
    bool valid() const { return w > 0 && h > 0 && ycc.size() == size_t(w) * h * 3; }
};

inline QMutex &guideMutex() { static QMutex m; return m; }
inline QHash<QString, std::shared_ptr<const Guide>> &guideStore()
{ static QHash<QString, std::shared_ptr<const Guide>> s; return s; }

inline void putGuide(const QString &path, std::shared_ptr<const Guide> g)
{
    QMutexLocker lk(&guideMutex());
    if (guideStore().size() > 3) guideStore().clear();   // crude cap (a guide is tens of MB)
    guideStore().insert(path, std::move(g));
}

inline std::shared_ptr<const Guide> getGuide(const QString &path)
{
    QMutexLocker lk(&guideMutex());
    auto it = guideStore().find(path);
    return it != guideStore().end() ? it.value() : nullptr;
}

/* ---- Auto-mask: how a dab is confined ---------------------------------------------------
   Lightroom's model, and the one thing the first implementation got wrong three ways at once.
   It used to be: one reference LUMINANCE sampled at the stroke's START, a fixed +-0.15 band,
   and the test applied per pixel INDEPENDENTLY. On a brown cone against a grey background of
   the same lightness that paints the background (equal luminance), speckles the object (a
   fixed band cannot cover a textured surface), and -- because a lone pixel needs no route
   back to the brush -- happily paints matching pixels on the FAR side of the edge.

   The replacement, per DAB (all of it in guide space):
     1. REFERENCE = the median colour of a small window at the dab centre, re-sampled for
        every dab as the brush travels (Lightroom re-seeds under the crosshair; a stroke-start
        reference goes stale the moment the brush moves onto a different part of the subject).
     2. TOLERANCE adapts to that window's own spread (75th percentile of the distance to the
        reference, x kTolK, clamped): a busy texture widens it and a smooth area keeps it
        tight, which is what stops the speckling without letting the mask run.
     3. DISTANCE is Y + double-weighted chroma, so equal-lightness / different-hue reads as
        far apart.
     4. CONNECTIVITY: a flood fill from the dab centre keeps only the accepted region the
        brush centre can actually REACH. This is what holds the mask on the near side of an
        edge -- matching pixels across the boundary are no longer painted just for matching.
   The result is a 0..1 acceptance field over the dab's box, bilinearly sampled by the dab's
   pixel loop, so the preview and the full-res render confine identically. */
constexpr double kAutoChromaWeight = 2.0;    // chroma counts double against Y in the distance
constexpr double kTolK             = 2.5;    // tolerance = kTolK * local spread
constexpr double kTolFloor         = 10.0;   // ... never tighter than this (sensor noise)
constexpr double kTolCeil          = 70.0;   // ... never looser than this (or it masks nothing)
constexpr float  kAcceptCut        = 0.35f;  // acceptance treated as "in" for the flood fill

struct AutoMaskCtx {
    const Guide *g = nullptr;
    int    degrees = 0;        // target-norm -> output-norm rotation (0 for the preview)
    bool   on = false;
};

/* One dab's acceptance field, in GUIDE pixels. bypass = paint the dab unconfined (no guide,
   or the seed fell outside its own region -- better to paint than to swallow the stroke). */
struct DabField {
    int x0 = 0, y0 = 0, w = 0, h = 0;
    std::vector<float> a;
    bool bypass = true;
    /* Bilinear sample at a continuous guide-pixel coordinate; 0 outside the box. */
    float at(double gx, double gy) const
    {
        if (bypass) return 1.0f;
        const double u = gx - 0.5 - x0, v = gy - 0.5 - y0;
        if (u <= -1.0 || v <= -1.0 || u >= w || v >= h) return 0.0f;
        const int i0 = int(std::floor(u)), j0 = int(std::floor(v));
        const double tx = u - i0, ty = v - j0;
        auto px = [&](int i, int j) -> float {
            if (i < 0 || j < 0 || i >= w || j >= h) return 0.0f;
            return a[size_t(j) * w + i];
        };
        const float top = float(px(i0, j0) * (1 - tx) + px(i0 + 1, j0) * tx);
        const float bot = float(px(i0, j0 + 1) * (1 - tx) + px(i0 + 1, j0 + 1) * tx);
        return float(top * (1 - ty) + bot * ty);
    }
};

inline double yccDist(const uchar *p, const double ref[3])
{
    const double dy = double(p[0]) - ref[0];
    const double db = double(p[1]) - ref[1];
    const double dr = double(p[2]) - ref[2];
    return std::sqrt(dy * dy + kAutoChromaWeight * (db * db + dr * dr));
}

/* Build the acceptance field for one dab centred at (gcx,gcy) with radius rg, all in guide px. */
inline void buildDabField(const Guide &g, double gcx, double gcy, double rg, DabField &f)
{
    f.bypass = true;
    const double r = std::max(rg, 2.0);          // a sub-pixel dab still needs a box to grow in
    const int x0 = std::max(0,      int(std::floor(gcx - r)) - 1);
    const int x1 = std::min(g.w - 1, int(std::ceil (gcx + r)) + 1);
    const int y0 = std::max(0,      int(std::floor(gcy - r)) - 1);
    const int y1 = std::min(g.h - 1, int(std::ceil (gcy + r)) + 1);
    const int bw = x1 - x0 + 1, bh = y1 - y0 + 1;
    if (bw < 3 || bh < 3) return;
    const int ix = std::clamp(int(gcx), x0, x1), iy = std::clamp(int(gcy), y0, y1);

    /* 1 + 2: reference colour and tolerance, from a small window at the dab centre. */
    const int wr = std::max(1, int(r * 0.15));
    const int wx0 = std::max(0, ix - wr), wx1 = std::min(g.w - 1, ix + wr);
    const int wy0 = std::max(0, iy - wr), wy1 = std::min(g.h - 1, iy + wr);
    static thread_local std::vector<uchar> chan;
    static thread_local std::vector<double> dists;
    const int n = (wx1 - wx0 + 1) * (wy1 - wy0 + 1);
    double ref[3];
    for (int c = 0; c < 3; ++c) {
        chan.clear(); chan.reserve(n);
        for (int y = wy0; y <= wy1; ++y) {
            const uchar *row = g.ycc.data() + (size_t(y) * g.w + wx0) * 3 + c;
            for (int x = wx0; x <= wx1; ++x, row += 3) chan.push_back(*row);
        }
        auto mid = chan.begin() + chan.size() / 2;
        std::nth_element(chan.begin(), mid, chan.end());
        ref[c] = double(*mid);
    }
    dists.clear(); dists.reserve(n);
    for (int y = wy0; y <= wy1; ++y) {
        const uchar *row = g.ycc.data() + (size_t(y) * g.w + wx0) * 3;
        for (int x = wx0; x <= wx1; ++x, row += 3) dists.push_back(yccDist(row, ref));
    }
    double spread = 0.0;
    if (dists.size() > 3) {
        auto q = dists.begin() + (dists.size() * 3) / 4;
        std::nth_element(dists.begin(), q, dists.end());
        spread = *q;
    }
    const double tol = std::clamp(kTolK * spread, kTolFloor, kTolCeil);
    const double ramp = std::max(tol * 0.5, 1.0);

    /* 3: per-pixel acceptance over the box. */
    f.x0 = x0; f.y0 = y0; f.w = bw; f.h = bh;
    f.a.assign(size_t(bw) * bh, 0.0f);
    for (int y = 0; y < bh; ++y) {
        const uchar *row = g.ycc.data() + (size_t(y + y0) * g.w + x0) * 3;
        float *out = f.a.data() + size_t(y) * bw;
        for (int x = 0; x < bw; ++x, row += 3) {
            const double d = yccDist(row, ref);
            out[x] = d <= tol ? 1.0f : float(std::max(0.0, 1.0 - (d - tol) / ramp));
        }
    }

    /* 4: flood fill from the dab centre -- drop every accepted pixel it cannot reach. */
    const int si = ix - x0, sj = iy - y0;
    if (f.a[size_t(sj) * bw + si] <= kAcceptCut) return;      // seed is an outlier: unconfined
    static thread_local std::vector<uchar> seen;
    static thread_local std::vector<int> stack;
    seen.assign(size_t(bw) * bh, 0);
    stack.clear();
    stack.push_back(sj * bw + si);
    seen[size_t(sj) * bw + si] = 1;
    while (!stack.empty()) {
        const int k = stack.back(); stack.pop_back();
        const int cy = k / bw, cx = k - cy * bw;
        for (int dy = -1; dy <= 1; ++dy) {
            const int ny = cy + dy;
            if (ny < 0 || ny >= bh) continue;
            for (int dx = -1; dx <= 1; ++dx) {
                const int nx = cx + dx;
                if ((dx | dy) == 0 || nx < 0 || nx >= bw) continue;
                const size_t nk = size_t(ny) * bw + nx;
                if (seen[nk] || f.a[nk] <= kAcceptCut) continue;
                seen[nk] = 1;
                stack.push_back(int(nk));
            }
        }
    }
    for (size_t k = 0; k < f.a.size(); ++k) if (!seen[k]) f.a[k] = 0.0f;

    /* 3x3 box blur: the fill's boundary is a hard staircase at guide resolution, and the dab
       samples this field bilinearly at a much finer target resolution. */
    static thread_local std::vector<float> tmp;
    tmp = f.a;
    for (int y = 0; y < bh; ++y) {
        for (int x = 0; x < bw; ++x) {
            float sum = 0.0f; int cnt = 0;
            for (int dy = -1; dy <= 1; ++dy) {
                const int ny = y + dy;
                if (ny < 0 || ny >= bh) continue;
                for (int dx = -1; dx <= 1; ++dx) {
                    const int nx = x + dx;
                    if (nx < 0 || nx >= bw) continue;
                    sum += tmp[size_t(ny) * bw + nx]; ++cnt;
                }
            }
            f.a[size_t(y) * bw + x] = cnt ? sum / cnt : 0.0f;
        }
    }
    f.bypass = false;
}

/* Target pixel -> guide pixel. `degrees` maps a TARGET pixel back to output-normalized (0 for
   the output-space preview; the render's EXIF degrees for work space), matching buildMaskBuffer.
   The map is AFFINE in every case, so it is solved once per dab into
       gx = m[0] + m[1]*tx + m[2]*ty        gy = m[3] + m[4]*tx + m[5]*ty
   rather than re-deriving two divisions and a switch for each of the dab's pixels. */
struct GuideMap { double m[6]; };

inline GuideMap guideMapFor(const AutoMaskCtx &a, int w, int h)
{
    const double gw = a.g->w, gh = a.g->h;
    const double kx = gw / w, ky = gh / h;      // if the axes are NOT swapped
    const double sx = gw / h, sy = gh / w;      // if they are (90 / 270)
    GuideMap g{};
    switch (a.degrees) {
        case 90:  g = {{ gw, 0.0,  -sx, 0.0, sy, 0.0 }}; break;   // gx=(1-ty/h)gw, gy=(tx/w)gh
        case 180: g = {{ gw, -kx,  0.0,  gh, 0.0, -ky }}; break;
        case 270: g = {{ 0.0, 0.0,  sx,  gh, -sy, 0.0 }}; break;  // gx=(ty/h)gw, gy=(1-tx/w)gh
        default:  g = {{ 0.0, kx,  0.0, 0.0, 0.0,  ky }}; break;
    }
    return g;
}

inline void targetToGuide(const GuideMap &g, double tx, double ty, double &gx, double &gy)
{
    gx = g.m[0] + g.m[1] * tx + g.m[2] * ty;
    gy = g.m[3] + g.m[4] * tx + g.m[5] * ty;
}

/* ---- Dab cost counters (diagnostic) ----
   Two relaxed atomic adds per DAB (not per pixel), so the cost is nothing next to the dab
   itself. MW's [DevTime] line reports and resets them around a mask build, which is the
   only way to tell "too many dabs" from "each dab is the whole image" -- the two have very
   different fixes and static reading cannot distinguish them. Renders on the GUI thread and
   the settle worker share the counters, so a settle landing mid-drag can inflate one line. */
struct DabStats { std::atomic<quint64> dabs{0}; std::atomic<quint64> pixels{0}; };
inline DabStats &dabStats() { static DabStats s; return s; }

/* Coverage 0..1 of a dab at distance `dist` (px) from centre -- LIGHTROOM model:
   `radius` is the OUTER extent (the brush SIZE) and the dab is clipped there, so a dab
   never grows with feather. Feather f=feather/100 softens INWARD through the shared
   MaskFalloff profile: f=0 is a hard edge at radius, and as f rises the half-coverage
   radius pulls in to radius*Shape::h with a rounder, Gaussian falloff -- no flat core
   (see MaskFalloff for why). The cursor's inner ring is that same half-coverage
   radius. */
inline float coverage(double dist, double radius, double f)
{
    if (radius <= 0.0)   return 0.0f;
    if (dist >= radius)  return 0.0f;
    if (f <= 0.0)        return 1.0f;                      // hard edge at radius
    return MaskFalloff::coverage(dist / radius, MaskFalloff::shapeFor(f * 100.0));
}

/* Accumulate a single dab into a per-stroke coverage buffer by MAX. With an active AutoMaskCtx the
   coverage is attenuated by the edge factor (paints only near-luminance pixels).

   THIS IS THE BRUSH HOT LOOP. A dab's box is (2*radius)^2 pixels clamped to the buffer,
   so a large brush touches most of the image PER DAB, and the develop render replays
   every dab of the stroke on every interactive tick. Two things keep it honest:

     - SQUARED-DISTANCE GATES. The outside test uses d^2, so a pixel beyond the dab pays
       no sqrt at all, and at feather 0 (the default) NO pixel does -- the whole dab is a
       flat disc. Inside, the profile is read from a table built ONCE per dab
       (MaskFalloff::Lut), never a pow per pixel -- which tracks coverage() (kept for the
       callers that need a distance-based value) to ~1e-5, the table's interpolation error.
     - ROW-PARALLEL. Bands own disjoint rows, so the MAX accumulation cannot race.
       Serial below a threshold, where a big brush is not the case and dispatch would
       dominate -- ImageView stamps one dab per mouse-move on a small buffer and must
       not pay for a thread hop. */
inline void dabMax(float *cov, int w, int h, double cx, double cy, double radius, double f,
                   const AutoMaskCtx *am = nullptr)
{
    if (radius <= 0.0) return;
    const int x0 = std::max(0,     int(std::floor(cx - radius)));   // outer extent = radius (LR)
    const int x1 = std::min(w - 1, int(std::ceil (cx + radius)));
    const int y0 = std::max(0,     int(std::floor(cy - radius)));
    const int y1 = std::min(h - 1, int(std::ceil (cy + radius)));
    if (x1 < x0 || y1 < y0) return;
    dabStats().dabs.fetch_add(1, std::memory_order_relaxed);
    dabStats().pixels.fetch_add(quint64(x1 - x0 + 1) * quint64(y1 - y0 + 1),
                                std::memory_order_relaxed);
    /* Auto-mask: build this dab's acceptance field ONCE (guide space, see buildDabField),
       then the pixel loop just samples it. Consecutive dabs along a stroke sit a quarter
       radius apart and often land on the same guide pixel, so the field is cached and
       reused when the centre has not moved a guide pixel -- the fill is the only part of
       the brush that is not embarrassingly parallel. thread_local: the render worker and
       the GUI both stamp dabs. */
    const bool auto_ = am && am->on && am->g && am->g->valid();
    static thread_local DabField field;
    static thread_local const Guide *fieldGuide = nullptr;
    static thread_local double fieldCx = 0, fieldCy = 0, fieldR = -1;
    double gcx = 0, gcy = 0, grad = 0;
    GuideMap gmap{};
    if (auto_) {
        gmap = guideMapFor(*am, w, h);
        targetToGuide(gmap, cx, cy, gcx, gcy);
        grad = radius * double(std::max(am->g->w, am->g->h)) / std::max(w, h);
        if (fieldGuide != am->g || std::abs(grad - fieldR) > 0.01 ||
            std::abs(gcx - fieldCx) > 0.5 || std::abs(gcy - fieldCy) > 0.5) {
            buildDabField(*am->g, gcx, gcy, grad, field);
            fieldGuide = am->g; fieldCx = gcx; fieldCy = gcy; fieldR = grad;
        }
    }
    const DabField *fld = auto_ ? &field : nullptr;
    const double r2    = radius * radius;
    const bool   hard  = (f <= 0.0);                  // flat disc, no profile to sample
    const double invR  = 1.0 / radius;
    MaskFalloff::Lut lut;
    if (!hard) lut.build(f * 100.0);

    auto rows = [=, &lut](int ya, int yb) {
        for (int y = ya; y <= yb; ++y) {
            float *row = cov + size_t(y) * w;
            const double dy = y + 0.5 - cy;
            const double dy2 = dy * dy;
            /* Row constants of the affine guide map (the x term is added per pixel). */
            const double gx0 = gmap.m[0] + gmap.m[1] * 0.5 + gmap.m[2] * (y + 0.5);
            const double gy0 = gmap.m[3] + gmap.m[4] * 0.5 + gmap.m[5] * (y + 0.5);
            for (int x = x0; x <= x1; ++x) {
                const double dx = x + 0.5 - cx;
                const double d2 = dx * dx + dy2;
                if (d2 >= r2) continue;               // outside the dab
                float c;
                if (hard) c = 1.0f;                   // feather 0: no sqrt, no table
                else {
                    c = lut.at(std::sqrt(d2) * invR);
                    if (c <= 0.0f) continue;
                }
                if (auto_) {
                    c *= fld->at(gx0 + gmap.m[1] * x, gy0 + gmap.m[4] * x);
                    if (c <= 0.0f) continue;
                }
                if (c > row[x]) row[x] = c;
            }
        }
    };

    const int nRows = y1 - y0 + 1;
    const size_t area = size_t(x1 - x0 + 1) * size_t(nRows);
    const int maxThreads = qMax(1, QThreadPool::globalInstance()->maxThreadCount());
    if (maxThreads == 1 || area < (size_t(1) << 16) || nRows < 2) {
        rows(y0, y1);
        return;
    }
    const int chunks = qMin(maxThreads, nRows);
    const int per = (nRows + chunks - 1) / chunks;
    QVector<QFuture<void>> futs;
    futs.reserve(chunks);
    for (int k = 0; k < chunks; ++k) {
        const int ya = y0 + k * per, yb = qMin(y1, ya + per - 1);
        if (ya > yb) break;
        futs.append(QtConcurrent::run(QThreadPool::globalInstance(),
                                      [=]{ rows(ya, yb); }));
    }
    for (QFuture<void> &fu : futs) fu.waitForFinished();
}

/* Dab spacing along a stroke, as a fraction of the brush RADIUS. Dabs composite by MAX
   and overlap heavily, so this is a cost/quality knob rather than a correctness one: at a
   quarter of the radius the union's edge scallops by well under 1% of the radius. It is
   also the single biggest cost lever in the brush path -- a stroke costs
   (arc length / (radius*this)) full dab boxes, each (2*radius)^2 pixel evaluations. */
inline constexpr double kDabSpacing = 0.25;

/* Accumulate a stroke segment p0->p1 (px), placing dabs every radius*kDabSpacing of ARC
   LENGTH. `carry` is how far the stroke has travelled since the last dab was stamped
   (from the previous segment); the return value carries forward into the next one.

   Spacing is tracked ACROSS segments deliberately. A mouse delivers moves a few pixels
   apart, and this used to compute `n = max(1, len/step)` PER SEGMENT -- so a 3px move
   still stamped two full-radius dabs, and a stroke cost two dab boxes per mouse-move
   instead of one per quarter-radius travelled. On a 2 MP mask with a 20% brush that
   measured ~900 ms per render tick, since the render replays every dab of the whole
   stroke. Carrying the residual makes cost proportional to the LENGTH of the stroke, not
   to how many events the mouse happened to deliver. See notes/Documentation.txt
   "MASK-DRAG LATENCY". */
inline double segmentMax(float *cov, int w, int h, QPointF p0, QPointF p1, double radius,
                         double f, const AutoMaskCtx *am = nullptr, double carry = 0.0)
{
    const double len = std::hypot(p1.x() - p0.x(), p1.y() - p0.y());
    if (len <= 0.0 || radius <= 0.0) return carry;
    const double step = std::max(1.0, radius * kDabSpacing);
    double u = step - carry;                 // distance into this segment to the next dab
    if (u > len) return carry + len;         // segment too short to earn a dab
    double last = u;
    while (u <= len) {
        const double t = u / len;
        dabMax(cov, w, h, p0.x() + (p1.x()-p0.x())*t, p0.y() + (p1.y()-p0.y())*t,
               radius, f, am);
        last = u;
        u += step;
    }
    return len - last;
}

/* Composite a per-stroke coverage buffer into the running mask with flow (0..1). */
inline void composite(float *mask, const float *cov, size_t n, double flow, bool erase)
{
    for (size_t k = 0; k < n; ++k) {
        const float a = float(flow) * cov[k];
        if (a <= 0.0f) continue;
        if (erase) mask[k] *= (1.0f - a);
        else       mask[k]  = mask[k] + a * (1.0f - mask[k]);
    }
}

/* Map a stroke point index (output-normalized) to target-pixel coords. `degrees` is the EXIF
   rotation applied to produce the displayed/output image from the target buffer's space (0 for the
   output-oriented preview; the render passes the actual degrees so output-norm -> work-norm). */
inline QPointF point(const QJsonArray &pts, int i, int degrees, int w, int h)
{
    const double onx = pts.at(i*2).toDouble(), ony = pts.at(i*2 + 1).toDouble();
    double tnx, tny;
    switch (degrees) {                       // inverse of work-norm -> output-norm (CW)
        case 90:  tnx = ony;       tny = 1.0 - onx; break;
        case 180: tnx = 1.0 - onx; tny = 1.0 - ony; break;
        case 270: tnx = 1.0 - ony; tny = onx;       break;
        default:  tnx = onx;       tny = ony;       break;
    }
    return QPointF(tnx * w, tny * h);
}

/* Rasterize all committed strokes into mask (w*h, pre-zeroed or accumulated). scratch is a reusable
   w*h buffer for the per-stroke coverage. `guide` (optional) enables auto-mask on strokes flagged
   for it. */
inline void rasterize(const QJsonArray &strokes, float *mask, std::vector<float> &scratch,
                      int w, int h, int degrees, const Guide *guide = nullptr)
{
    if (w <= 0 || h <= 0) return;
    const double longEdge = std::max(w, h);
    scratch.assign(size_t(w) * h, 0.0f);
    for (const QJsonValue &sv : strokes) {
        const QJsonObject so = sv.toObject();
        const QJsonArray pts = so.value("pts").toArray();
        if (pts.size() < 2) continue;
        const double size = so.value("size").toDouble(20);
        const double f    = std::clamp(so.value("feather").toDouble(0) / 100.0, 0.0, 1.0);
        const double flow = std::clamp(so.value("flow").toDouble(100) / 100.0, 0.0, 1.0);
        const bool  erase = so.value("erase").toBool(false);
        const double radius = (size / 200.0) * longEdge;

        /* autoMaskMode ("lum" | "ai") is a RETIRED key: there is one auto-mask now (see
           the Auto-mask section above). Old strokes carrying it still read, and get the
           new confinement. */
        AutoMaskCtx am;
        if (so.value("autoMask").toBool(false) && guide && guide->valid()) {
            am.on = true; am.g = guide; am.degrees = degrees;
        }
        const AutoMaskCtx *amp = am.on ? &am : nullptr;

        std::fill(scratch.begin(), scratch.end(), 0.0f);
        QPointF prev = point(pts, 0, degrees, w, h);
        dabMax(scratch.data(), w, h, prev.x(), prev.y(), radius, f, amp);
        double carry = 0.0;
        for (int i = 1; i*2 + 1 < pts.size(); ++i) {
            const QPointF cur = point(pts, i, degrees, w, h);
            carry = segmentMax(scratch.data(), w, h, prev, cur, radius, f, amp, carry);
            prev = cur;
        }
        /* Always stamp the LAST point. Spaced dabs can stop up to one step short of it,
           and mid-stroke that point is where the CURSOR is -- the painted area would
           visibly trail the brush. ImageView's live preview stamps its tip the same way
           (ImageView::brushStampTo), so the two stay in step. */
        if (carry > 0.0) dabMax(scratch.data(), w, h, prev.x(), prev.y(), radius, f, amp);
        composite(mask, scratch.data(), size_t(w) * h, flow, erase);
    }
}

} // namespace BrushStamp

#endif // BRUSHSTAMP_H
