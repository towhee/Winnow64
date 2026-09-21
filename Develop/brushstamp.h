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
   Lightroom's model. The first implementation compared each pixel's LUMINANCE against a
   single reference sampled at the STROKE START, within a fixed +-0.15 band, INDEPENDENTLY
   per pixel: on a brown cone against a grey background of the same lightness that painted
   the background (equal luminance), speckled the object (a fixed band cannot cover a
   textured surface), and painted matching pixels on the FAR side of the edge (a lone pixel
   needs no route back to the brush). Chroma, an adaptive tolerance and a flood fill fixed
   those three.

   The flood fill was still only a BINARY reachability test, and that is what failed on fur:
   a squirrel's head against a soft brown background is the hardest case for it. The fur is
   so textured that the adaptive tolerance pinned itself to the ceiling, the blurred
   background then fell INSIDE that tolerance, and the fill -- which crosses any accepted
   pixel for free -- walked out through the hairy edge and kept going. Nothing in the model
   knew an edge had been crossed at all.

   The current model, per DAB (all of it in guide space):
     1. SMOOTHED SOURCE. Every decision reads a 3x3-smoothed copy of the guide box, so a
        single noisy hair neither sets the reference nor punches a hole through an edge.
     2. REFERENCE = the median colour of a small window at the dab centre, re-sampled for
        every dab as the brush travels (Lightroom re-seeds under the crosshair; a
        stroke-start reference goes stale the moment the brush moves onto a different part
        of the subject).
     3. TOLERANCE adapts to that window's own spread (75th percentile of the distance to the
        reference, x kTolK, clamped): a busy texture widens it, a smooth area keeps it tight.
        Deliberately TIGHTER than the first cut (2.5 / ceiling 70 let fur swallow a
        same-family background); the cost fill below, not a wide band, is what keeps a
        textured subject whole.
     4. DISTANCE is Y + double-weighted chroma, so equal-lightness / different-hue reads as
        far apart.
     5. EDGE ENERGY, measured RELATIVE TO THE LOCAL TEXTURE LEVEL (E / the window's own 60th
        percentile). This is the part that tells fur from a boundary: fur is uniformly busy,
        so its texture raises the yardstick with it, while the head's outline stands well
        above the yardstick everywhere along it.
     6. COST FILL instead of a binary flood. A monotone dial-queue (Dijkstra with integer
        buckets, O(box)) grows from a small disc at the dab centre, and each step PAYS:
            edge      kEdgeK      * (E/Eref - 1)^2    -- crossing a boundary
            colour    kColorHard  * (d/tol - 1)^2     -- stepping outside the tolerance
            drift     kDrift/r    * (d/tol)^2         -- creeping away within it
        A pixel is reached only while the cheapest route to it stays inside a budget of 1.0,
        and acceptance falls off smoothly as that budget runs out. A hard edge stops the
        fill in one or two pixels; a BLURRED edge -- the case the binary fill could not see
        at all -- is paid for over every pixel of the transition and stops it just as surely;
        and drift keeps a long free run across an in-tolerance gradient from paying nothing.
        4-connectivity, so the fill cannot squeeze diagonally through a one-pixel gap.
   The result is a 0..1 acceptance field over the dab's box, bilinearly sampled by the dab's
   pixel loop, so the preview and the full-res render confine identically. */
constexpr double kAutoChromaWeight = 2.0;    // chroma counts double against Y in the distance
constexpr double kTolK             = 2.4;    // tolerance = kTolK * local spread
constexpr double kTolFloor         = 8.0;    // ... never tighter than this (sensor noise)
constexpr double kTolCeil          = 45.0;   // ... never looser than this (or it masks nothing)
constexpr double kEdgeK            = 0.14;   // cost of crossing one pixel of a strong edge
constexpr double kEdgeFloor        = 4.0;    // floor on the local texture level E is judged against
constexpr double kColorHard        = 0.75;   // cost of stepping onto an out-of-tolerance pixel
constexpr double kDrift            = 0.35;   // cost of creeping one full radius at d == tol
constexpr double kTexWeight        = 2.0;    // texture-energy difference counts like chroma
constexpr double kSmoothFrac       = 0.08;   // analysis blur radius, as a fraction of the dab radius
constexpr int    kSmoothMax        = 6;      // ... capped (a big brush must not go soft)
constexpr double kAnalysisDiameter = 160.0;  // analysis pixels across a dab (see buildDabField)
constexpr int    kDecMax           = 4;      // ... and the most the guide may be decimated for it
constexpr float  kAcceptCut        = 0.5f;   // colour acceptance a seed pixel must clear
constexpr int    kCostLevels       = 64;     // dial-queue quantization of the 0..1 cost budget

struct AutoMaskCtx {
    const Guide *g = nullptr;
    int    degrees = 0;        // target-norm -> output-norm rotation (0 for the preview)
    bool   on = false;
};

/* One dab's acceptance field. Its origin (x0,y0) is in GUIDE pixels and each of its own
   pixels spans `dec` of them (see buildDabField: a big brush is analysed coarser, since the
   guide is only ever built to ~kGuidePxPerDab across a DAB and a 1024px floor leaves a large
   brush with far more resolution than the model was tuned for). bypass = paint the dab
   unconfined (no guide, or the seed fell outside its own region -- better to paint than to
   swallow the stroke). */
struct DabField {
    int x0 = 0, y0 = 0, w = 0, h = 0, dec = 1;
    std::vector<float> a;
    bool bypass = true;
    /* Bilinear sample at a continuous guide-pixel coordinate; 0 outside the box. */
    float at(double gx, double gy) const
    {
        if (bypass) return 1.0f;
        const double id = 1.0 / dec;
        const double u = (gx - x0) * id - 0.5, v = (gy - y0) * id - 0.5;
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

/* The same metric between two pixels, SQUARED -- the edge pass wants a horizontal and a
   vertical difference combined, and sqrt(a)^2 + sqrt(b)^2 under one sqrt is one root per
   pixel instead of three. */
inline double yccDiffSq(const uchar *a, const uchar *b)
{
    const double dy = double(a[0]) - b[0];
    const double db = double(a[1]) - b[1];
    const double dr = double(a[2]) - b[2];
    return dy * dy + kAutoChromaWeight * (db * db + dr * dr);
}

/* Percentile of an UNSORTED scratch vector (partially sorts it in place). */
inline double percentileOf(std::vector<double> &v, double p)
{
    if (v.empty()) return 0.0;
    size_t k = size_t(double(v.size() - 1) * p);
    auto it = v.begin() + k;
    std::nth_element(v.begin(), it, v.end());
    return *it;
}

/* Separable box blur of a `ch`-channel planar-interleaved float buffer, radius k, edges
   clamped. Two passes of this approximate a Gaussian for a fraction of the cost, and the
   analysis planes (see buildDabField step 1) are the only thing in the brush that needs one.
   `tmp` is a caller-owned scratch buffer so the hot path allocates nothing. */
inline void boxBlur(float *buf, std::vector<float> &tmp, int w, int h, int ch, int k)
{
    if (k < 1 || w < 1 || h < 1) return;
    const float inv = 1.0f / float(2 * k + 1);
    tmp.assign(size_t(w) * h * ch, 0.0f);
    for (int y = 0; y < h; ++y) {                       // horizontal
        const float *in = buf + size_t(y) * w * ch;
        float *out = tmp.data() + size_t(y) * w * ch;
        for (int c = 0; c < ch; ++c) {
            float sum = 0.0f;
            for (int x = -k; x <= k; ++x) sum += in[size_t(std::clamp(x, 0, w - 1)) * ch + c];
            for (int x = 0; x < w; ++x) {
                out[size_t(x) * ch + c] = sum * inv;
                sum += in[size_t(std::clamp(x + k + 1, 0, w - 1)) * ch + c]
                     - in[size_t(std::clamp(x - k,     0, w - 1)) * ch + c];
            }
        }
    }
    /* Vertical pass, streamed by ROW rather than by column: a column walk of a buffer this
       size misses the cache on every access, and this blur runs three times per dab. One
       running sum per column, advanced a row at a time. */
    static thread_local std::vector<float> run;
    run.assign(size_t(w) * ch, 0.0f);
    const size_t stride = size_t(w) * ch;
    auto rowOf = [&](int y) { return tmp.data() + size_t(std::clamp(y, 0, h - 1)) * stride; };
    for (int y = -k; y <= k; ++y) {
        const float *in = rowOf(y);
        for (size_t i = 0; i < stride; ++i) run[i] += in[i];
    }
    for (int y = 0; y < h; ++y) {
        float *out = buf + size_t(y) * stride;
        for (size_t i = 0; i < stride; ++i) out[i] = run[i] * inv;
        const float *add = rowOf(y + k + 1), *sub = rowOf(y - k);
        for (size_t i = 0; i < stride; ++i) run[i] += add[i] - sub[i];
    }
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
    const int gbw = x1 - x0 + 1, gbh = y1 - y0 + 1;
    if (gbw < 3 || gbh < 3) return;

    /* ANALYSIS RESOLUTION. The guide is built so a brush DIAMETER spans at least
       kGuidePxPerDab guide pixels (ImageView::ensureAutoGuide), but its long edge has a
       1024px FLOOR -- so a big brush arrives with several times the resolution this model
       was tuned at, and pays for all of it in every pass below and in the fill, which is the
       one part of a dab that cannot be parallelised. Decimating a large dab back to
       ~kAnalysisDiameter pixels across costs nothing in mask quality (the field is blurred
       and bilinearly resampled on the way out, and the brush in question is enormous) and
       takes the worst case from quadratic-in-the-clamp to flat. */
    const int dec = std::clamp(int(std::lround(2.0 * r / kAnalysisDiameter)), 1, kDecMax);
    const int bw = (gbw + dec - 1) / dec, bh = (gbh + dec - 1) / dec;
    if (bw < 3 || bh < 3) return;
    const size_t n = size_t(bw) * bh;
    const double rf = r / dec;                              // dab radius in ANALYSIS pixels
    const int cxb = std::clamp(int(std::lround((gcx - x0) / dec - 0.5)), 0, bw - 1);
    const int cyb = std::clamp(int(std::lround((gcy - y0) / dec - 0.5)), 0, bh - 1);

    /* 1: ANALYSIS SCALE. Everything below reads a copy of the box blurred at a radius
       proportional to the DAB, not a fixed 3x3 -- the single change that makes fur work.
       Fur's per-pixel spread is far wider than the colour STEP from fur to a soft
       background of the same family (the screenshot's case), so no per-pixel test can
       separate them: the tolerance has to open wide enough to hold the fur together, and
       the background is then inside it. Averaged over a few pixels the fur's noise collapses
       and its local MEAN is steady, so the step across the boundary becomes the largest
       thing in the box -- which is what the tolerance and the edge term below can act on.
       TEXTURE ENERGY (how far the raw pixels sit from that local mean) is kept as a
       feature in its own right: sharp fur against an out-of-focus background differs in
       texture even where it does not differ in colour, and nothing else in the model sees
       that. */
    const int ks  = std::clamp(int(std::lround(rf * kSmoothFrac)), 1, kSmoothMax);
    const int pad = ks + 1;      // the box rim is already outside the dab; exactness there buys nothing
    const int ew = bw + 2 * pad, eh = bh + 2 * pad;
    const size_t en = size_t(ew) * eh;
    static thread_local std::vector<float> ext, blr, tex, tmpbuf;
    ext.assign(en * 3, 0.0f);
    const float invCell = 1.0f / float(dec * dec);
    for (int y = 0; y < eh; ++y) {
        float *o = ext.data() + size_t(y) * ew * 3;
        if (dec == 1) {                                     // the common case: a straight copy
            const uchar *src = g.ycc.data()
                             + size_t(std::clamp(y0 + y - pad, 0, g.h - 1)) * g.w * 3;
            for (int x = 0; x < ew; ++x, o += 3) {
                const uchar *p = src + size_t(std::clamp(x0 + x - pad, 0, g.w - 1)) * 3;
                o[0] = p[0]; o[1] = p[1]; o[2] = p[2];
            }
            continue;
        }
        for (int x = 0; x < ew; ++x, o += 3) {
            float a0 = 0, a1 = 0, a2 = 0;                   // dec x dec guide block, clamped
            for (int sy = 0; sy < dec; ++sy) {
                const int gy2 = std::clamp(y0 + (y - pad) * dec + sy, 0, g.h - 1);
                for (int sx = 0; sx < dec; ++sx) {
                    const int gx2 = std::clamp(x0 + (x - pad) * dec + sx, 0, g.w - 1);
                    const uchar *p = g.ycc.data() + (size_t(gy2) * g.w + gx2) * 3;
                    a0 += p[0]; a1 += p[1]; a2 += p[2];
                }
            }
            o[0] = a0 * invCell; o[1] = a1 * invCell; o[2] = a2 * invCell;
        }
    }
    blr = ext;
    boxBlur(blr.data(), tmpbuf, ew, eh, 3, ks);
    boxBlur(blr.data(), tmpbuf, ew, eh, 3, ks);     // twice ~= Gaussian, cheaper
    tex.assign(en, 0.0f);
    for (size_t k = 0; k < en; ++k) tex[k] = std::abs(ext[k * 3] - blr[k * 3]);
    boxBlur(tex.data(), tmpbuf, ew, eh, 1, ks);

    /* Crop the two analysis planes to the box. */
    static thread_local std::vector<uchar> sm;
    static thread_local std::vector<float> texture;
    sm.assign(n * 3, 0);
    texture.assign(n, 0.0f);
    for (int y = 0; y < bh; ++y) {
        const float *si = blr.data() + (size_t(y + pad) * ew + pad) * 3;
        const float *ti = tex.data() +  size_t(y + pad) * ew + pad;
        uchar *o = sm.data() + size_t(y) * bw * 3;
        float *to = texture.data() + size_t(y) * bw;
        for (int x = 0; x < bw; ++x, si += 3, o += 3) {
            o[0] = uchar(std::clamp(si[0], 0.0f, 255.0f));
            o[1] = uchar(std::clamp(si[1], 0.0f, 255.0f));
            o[2] = uchar(std::clamp(si[2], 0.0f, 255.0f));
            to[x] = ti[x];
        }
    }
    auto px = [&](int x, int y) -> const uchar * { return sm.data() + (size_t(y) * bw + x) * 3; };

    /* 2: reference -- per-channel median colour, and median texture, of a small window at
       the dab centre. */
    const int wr  = std::max(1, int(rf * 0.15));
    const int wx0 = std::clamp(cxb - wr, 0, bw - 1), wx1 = std::clamp(cxb + wr, 0, bw - 1);
    const int wy0 = std::clamp(cyb - wr, 0, bh - 1), wy1 = std::clamp(cyb + wr, 0, bh - 1);
    static thread_local std::vector<uchar> chan;
    static thread_local std::vector<double> scratch;
    const int wn = (wx1 - wx0 + 1) * (wy1 - wy0 + 1);
    double ref[3];
    for (int c = 0; c < 3; ++c) {
        chan.clear(); chan.reserve(wn);
        for (int y = wy0; y <= wy1; ++y)
            for (int x = wx0; x <= wx1; ++x) chan.push_back(px(x, y)[c]);
        auto mid = chan.begin() + chan.size() / 2;
        std::nth_element(chan.begin(), mid, chan.end());
        ref[c] = double(*mid);
    }
    scratch.clear(); scratch.reserve(wn);
    for (int y = wy0; y <= wy1; ++y)
        for (int x = wx0; x <= wx1; ++x) scratch.push_back(texture[size_t(y) * bw + x]);
    const double refTex = percentileOf(scratch, 0.50);

    /* 3 + 4: distance to the reference over the whole box -- colour PLUS texture -- and the
       tolerance from the window's own spread. */
    static thread_local std::vector<float> dist;
    dist.assign(n, 0.0f);
    for (int y = 0; y < bh; ++y) {
        const uchar *row = px(0, y);
        const float *trow = texture.data() + size_t(y) * bw;
        float *out = dist.data() + size_t(y) * bw;
        for (int x = 0; x < bw; ++x, row += 3) {
            const double dc = yccDist(row, ref);
            const double dt = trow[x] - refTex;
            out[x] = float(std::sqrt(dc * dc + kTexWeight * dt * dt));
        }
    }
    scratch.clear();
    for (int y = wy0; y <= wy1; ++y)
        for (int x = wx0; x <= wx1; ++x) scratch.push_back(dist[size_t(y) * bw + x]);
    const double tol  = std::clamp(kTolK * percentileOf(scratch, 0.75), kTolFloor, kTolCeil);
    const double ramp = std::max(tol * 0.5, 1.0);

    /* 5: edge energy, and the local level it is judged against. At the analysis scale a
       boundary -- even a blurred one -- stands above the subject's own variation. */
    static thread_local std::vector<float> edge;
    edge.assign(n, 0.0f);
    for (int y = 0; y < bh; ++y) {
        for (int x = 0; x < bw; ++x) {
            const double gx = yccDiffSq(px(std::min(bw - 1, x + 1), y), px(std::max(0, x - 1), y));
            const double gy = yccDiffSq(px(x, std::min(bh - 1, y + 1)), px(x, std::max(0, y - 1)));
            edge[size_t(y) * bw + x] = float(0.5 * std::sqrt(gx + gy));
        }
    }
    scratch.clear();
    for (int y = wy0; y <= wy1; ++y)
        for (int x = wx0; x <= wx1; ++x) scratch.push_back(edge[size_t(y) * bw + x]);
    const double eRef = std::max(percentileOf(scratch, 0.60), kEdgeFloor);

    /* Per-pixel COLOUR ACCEPTANCE (1 inside the tolerance, ramping off outside it) and the
       cost of STEPPING ONTO each pixel, both resolved once into arrays. The fill below visits
       a pixel from up to four neighbours and is the one part of the dab that is not
       parallel, so nothing inside it recomputes what a flat pass can hand it. The step cost
       is quantized into kCostLevels buckets over a budget of 1.0. */
    const double invTol  = 1.0 / tol;
    const double invERef = 1.0 / eRef;
    const double drift   = kDrift / std::max(rf, 1.0);      // per pixel, so it scales with the brush
    static thread_local std::vector<float> acc;
    static thread_local std::vector<int> step;
    acc.assign(n, 0.0f);
    step.assign(n, 0);
    const double rf2 = rf * rf;
    for (size_t k = 0; k < n; ++k) {
        const int ky = int(k / bw), kx = int(k) - ky * bw;
        const double ox = kx - cxb, oy = ky - cyb;
        if (ox * ox + oy * oy > rf2) continue;   // outside the dab: never sampled, never filled
        const double d = dist[k];
        acc[k] = d <= tol ? 1.0f : float(std::max(0.0, 1.0 - (d - tol) / ramp));
        const double dn = d * invTol;
        const double en = edge[k] * invERef;
        const double dc = std::min(dn, 1.0);
        double c = drift * dc * dc;
        if (dn > 1.0) { const double e = dn - 1.0; c += kColorHard * e * e; }
        if (en > 1.0) { const double e = en - 1.0; c += kEdgeK     * e * e; }
        step[k] = int(std::lround(c * kCostLevels));
    }

    /* 6: cost fill -- a monotone dial queue (all costs >= 0, so buckets are visited in order
       and a pixel is final when its bucket comes up): O(box), no heap. */

    static thread_local std::vector<int> cost;
    static thread_local std::vector<std::vector<int>> bucket;
    const int kUnreached = kCostLevels + 1;
    cost.assign(n, kUnreached);
    if (int(bucket.size()) != kCostLevels + 1) bucket.resize(kCostLevels + 1);
    for (auto &b : bucket) b.clear();

    /* Seed from a small disc at the dab centre, not a single pixel: one outlier under the
       crosshair should not decide the whole dab. */
    const double sr = std::max(1.0, rf * 0.12);
    const int si0 = std::max(0, cxb - int(sr)), si1 = std::min(bw - 1, cxb + int(sr));
    const int sj0 = std::max(0, cyb - int(sr)), sj1 = std::min(bh - 1, cyb + int(sr));
    for (int y = sj0; y <= sj1; ++y) {
        for (int x = si0; x <= si1; ++x) {
            const double dx = x - cxb, dy = y - cyb;
            if (dx * dx + dy * dy > sr * sr) continue;
            const size_t k = size_t(y) * bw + x;
            if (acc[k] <= kAcceptCut) continue;
            cost[k] = 0;
            bucket[0].push_back(int(k));
        }
    }
    if (bucket[0].empty()) return;               // nothing under the brush matches: unconfined

    static const int dxs[4] = { 1, -1, 0, 0 }, dys[4] = { 0, 0, 1, -1 };
    for (int b = 0; b <= kCostLevels; ++b) {
        /* Index, not iterator: a zero-cost step appends to this same bucket as we walk it. */
        for (size_t qi = 0; qi < bucket[b].size(); ++qi) {
            const int k = bucket[b][qi];
            if (cost[k] != b) continue;                       // superseded by a cheaper route
            const int cy = k / bw, cx = k - cy * bw;
            for (int t = 0; t < 4; ++t) {
                const int nx = cx + dxs[t], ny = cy + dys[t];
                if (nx < 0 || ny < 0 || nx >= bw || ny >= bh) continue;
                const size_t nk = size_t(ny) * bw + nx;
                if (acc[nk] <= 0.0f) continue;                // hard colour gate
                const int nc = b + step[nk];
                if (nc > kCostLevels || nc >= cost[nk]) continue;
                cost[nk] = nc;
                bucket[nc].push_back(int(nk));
            }
        }
        bucket[b].clear();                                    // never pushed to again (nc >= b)
    }

    /* Acceptance: how much of the budget the cheapest route to this pixel had left, capped
       by the colour ramp. Smoothstepped, so the mask fades out as the fill runs out of
       budget rather than ending on a hard contour of its own. */
    f.x0 = x0; f.y0 = y0; f.w = bw; f.h = bh; f.dec = dec;
    f.a.assign(n, 0.0f);
    for (size_t k = 0; k < n; ++k) {
        if (cost[k] > kCostLevels) continue;
        const float left = 1.0f - float(cost[k]) / float(kCostLevels);
        f.a[k] = std::min(acc[k], left * left * (3.0f - 2.0f * left));
    }

    /* 3x3 blur: the fill's boundary is a staircase at analysis resolution, and the dab
       samples this field bilinearly at a much finer target resolution. */
    boxBlur(f.a.data(), tmpbuf, bw, bh, 1, 1);
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
