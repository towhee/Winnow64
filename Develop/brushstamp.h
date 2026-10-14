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
#include <limits>
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

/* ---- Auto-mask: how a dab is confined ------------------------------------------
   HISTORY. The first cut compared each pixel's LUMINANCE with one reference from the
   stroke start, inside a fixed band, pixel by pixel: it painted a grey background of the
   same lightness as a brown cone, speckled any texture and painted matching pixels on the
   far side of an edge. The second cut (chroma, a tolerance adapted to the spread of a
   window at the dab centre, edge energy relative to the local texture, a cost fill)
   fixed those but failed on a DEFOCUSED edge -- textured gravel against smooth tan bokeh
   (2026-10-14), measured offline on the user's screenshot:
     - the tolerance followed whatever sat under the crosshair: a dab centred on dark
       pebbles read a wide spread and opened tol to ~41, and the background (~38 away)
       was inside it;
     - the edge term never fired: a defocused boundary rises ~1.5 per pixel, under the
       floor (4) its relative yardstick could not go below.
   Both are ABSOLUTE tests (a distance against one reference and a tolerance), and an
   absolute test cannot know how far the OTHER side is. Lightroom's own brush (a capture
   of the same stroke) does find this edge but holes the gravel on every pebble -- a
   per-pixel test with a tight band.

   THE MODEL NOW (per DAB, all of it in guide space) is RELATIVE: which class does a
   pixel look more like -- the one under the crosshair, or the other one in the brush?
     1. SEARCH ZONE = THE CORE. The fill and its decisions run inside the core radius rc
        (the cursor's inner ring = the falloff's half-coverage radius; the whole dab at
        feather 0). The feather band beyond it never adds area: a band pixel takes the
        acceptance of the core rim on its own radius, gated by its own class test, and
        the dab's falloff fades it. Without the gate, a rim crossing the edge at a slant
        projected out past it and sawtoothed the stroke by one spike per dab.
     2. ANALYSIS SCALE. Every decision reads the box blurred at a radius proportional to
        the core (kSmoothFrac), with the leftover per-pixel deviation kept as a TEXTURE
        plane -- averaged, a texture's noise collapses to a steady local mean, and sharp
        detail against an out-of-focus background differs in texture even where it does
        not in colour. Features: Y, sqrt(2) x Cb/Cr, and texture on a LOG scale
        (kTexLogK x log2(1 + tex/kTexLogT)). Texture contrast is a ratio: smooth-vs-
        textured (1 vs 7) must read far apart while rough-vs-rougher (12 vs 18) is one
        surface. A linear texture weight could not do both -- measured over noise seeds,
        any weight that separated a fine texture from a smooth field also holed a uniform
        rough surface (down to 0.55 accepted on some seeds).
     3. CLASSES. A small deterministic k-means (kClusters, farthest-point seeding from
        the median under the crosshair -- no RNG, so the field is a pure function for the
        render caches) over (a subsample of) the whole dab disc. Five classes, not four:
        gravel alone has three lightness modes, and with four the background had to share
        a cluster with the transition pixels on the ridge, whose centre then merged into
        the subject and painted one dab's worth of sky. Only FLAT pixels vote (feature
        gradient <= kGradK x the disc median): the blur turns a hard edge into a band of
        in-between colours whose own clusters chained the merge across a plain lightness
        edge (light wall / dark wall, 2026-10-14). The SUBJECT is every cluster
        holding kShareMin of the seed disc at the centre, plus the one nearest it, closed
        under a
        MERGE: two clusters closer than max(kMergeMin, kMergeSig x their spread) are one
        class. The merge distance weights Y by kMergeYW, because a surface's own light
        and shadow (pebbles in gravel, a sky gradient) is a lightness spread while what
        separates a subject from what lies beyond is mostly chroma and texture. A
        silhouette still splits: its lightness step is far above the merge distance.
     4. RELATIVE LIKENESS p = dO / (dS + dO), dS/dO the distance to the nearest
        subject/other centre. Acceptance ramps over p = kPLo..kPHi. With no OTHER class
        the dab is one surface and paints its whole core -- no holes in texture.
     5. COST FILL from the seed disc (a monotone dial queue = Dijkstra in integer
        buckets over a budget of 1.0, 4-connected so it cannot squeeze diagonally),
        each step onto a pixel with p < 0.5 paying kStepK*(1-2p)^2: connectivity, so a
        look-alike patch on the far side of the edge is not painted, and a soft edge is
        paid for across its whole transition. Acceptance smoothsteps off as the budget
        runs out.
   The result is a 0..1 field over the dab's box, bilinearly sampled by the dab's pixel
   loop, so the preview and the full-res render confine identically. Prototyped in Python
   on the gravel screenshot and synthetic cases (equal-lightness brown/grey, silhouette,
   texture vs smooth of one colour, blurred edge, sky gradient, uniform texture) before
   this port. */
constexpr double kChromaW2   = 2.0;     // chroma weight in the squared feature distance
constexpr double kTexLogK    = 10.0;    // texture = kTexLogK * log2(1 + tex/kTexLogT)
constexpr double kTexLogT    = 2.0;
constexpr double kSmoothFrac = 0.08;    // analysis blur radius, as a fraction of the core
constexpr int    kSmoothMax  = 6;       // ... capped (a big brush must not go soft)
constexpr double kAnalysisDiameter = 160.0;  // analysis px across a dab
constexpr int    kDecMax     = 4;       // ... and the most the guide may be decimated
constexpr int    kClusters   = 5;       // k-means classes over the dab disc
constexpr int    kKmIters    = 8;       // Lloyd iterations
constexpr int    kKmSamples  = 2048;    // disc pixels k-means is run on, at most
constexpr double kGradK      = 3.0;     // a pixel above this x the median gradient ...
constexpr double kGradFloor  = 1.0;     // ... (and this) is a boundary: it does not vote
constexpr double kSeedFrac   = 0.3;     // seed disc radius, as a fraction of the core
constexpr double kShareMin   = 0.2;     // seed-disc share that makes a cluster subject
constexpr double kMergeYW    = 0.4;     // Y weight in the merge distance
constexpr double kMergeSig   = 2.5;     // merge when closer than this x the spread ...
constexpr double kMergeMin   = 6.0;     // ... or than this, whichever is larger
constexpr float  kPLo        = 0.4f;    // likeness where acceptance starts ...
constexpr float  kPHi        = 0.6f;    // ... and where it is full
constexpr double kStepK      = 3.0;     // fill step cost at likeness p: kStepK*(1-2p)^2
constexpr int    kCostLevels = 64;      // dial-queue quantization of the 0..1 cost budget

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

/* Build the acceptance field for one dab centred at (gcx,gcy), outer radius rg and core
   radius rcg, all in guide px. See the Auto-mask section above for the model. */
inline void buildDabField(const Guide &g, double gcx, double gcy, double rg, double rcg,
                          DabField &f)
{
    f.bypass = true;
    const double r = std::max(rg, 2.0);      // a sub-pixel dab still needs a box
    const int x0 = std::max(0,      int(std::floor(gcx - r)) - 1);
    const int x1 = std::min(g.w - 1, int(std::ceil (gcx + r)) + 1);
    const int y0 = std::max(0,      int(std::floor(gcy - r)) - 1);
    const int y1 = std::min(g.h - 1, int(std::ceil (gcy + r)) + 1);
    const int gbw = x1 - x0 + 1, gbh = y1 - y0 + 1;
    if (gbw < 3 || gbh < 3) return;

    /* ANALYSIS RESOLUTION. The guide is built so a brush DIAMETER spans at least
       kGuidePxPerDab guide pixels (ImageView::ensureAutoGuide), but its long edge has a
       1024px FLOOR -- so a big brush arrives with several times the resolution this
       model was tuned at, and pays for all of it in every pass below and in the fill,
       which is the one part of a dab that cannot be parallelised. Decimating a large dab
       back to ~kAnalysisDiameter pixels across costs nothing in mask quality (the field
       is blurred and bilinearly resampled on the way out, and the brush in question is
       enormous) and takes the worst case from quadratic-in-the-clamp to flat. */
    const int dec = std::clamp(int(std::lround(2.0 * r / kAnalysisDiameter)), 1, kDecMax);
    const int bw = (gbw + dec - 1) / dec, bh = (gbh + dec - 1) / dec;
    if (bw < 3 || bh < 3) return;
    const size_t n = size_t(bw) * bh;
    const double rf  = r / dec;                             // dab radius, analysis px
    const double rcf = std::clamp(rcg, 1.0, r) / dec;       // core radius, likewise
    const double cxa = (gcx - x0) / dec - 0.5;              // dab centre, analysis pixels
    const double cya = (gcy - y0) / dec - 0.5;

    /* Model step 2: the analysis planes -- the box blurred at the core's scale, and the
       texture energy left over. */
    const int ks  = std::clamp(int(std::lround(rcf * kSmoothFrac)), 1, kSmoothMax);
    const int pad = ks + 1;      // the box rim is outside the dab: exactness buys nothing
    const int ew = bw + 2 * pad, eh = bh + 2 * pad;
    const size_t en = size_t(ew) * eh;
    static thread_local std::vector<float> ext, blr, tex, tmpbuf;
    ext.assign(en * 3, 0.0f);
    const float invCell = 1.0f / float(dec * dec);
    for (int y = 0; y < eh; ++y) {
        float *o = ext.data() + size_t(y) * ew * 3;
        if (dec == 1) {                             // the common case: a straight copy
            const uchar *src = g.ycc.data()
                             + size_t(std::clamp(y0 + y - pad, 0, g.h - 1)) * g.w * 3;
            for (int x = 0; x < ew; ++x, o += 3) {
                const uchar *p = src + size_t(std::clamp(x0 + x - pad, 0, g.w - 1)) * 3;
                o[0] = p[0]; o[1] = p[1]; o[2] = p[2];
            }
            continue;
        }
        for (int x = 0; x < ew; ++x, o += 3) {
            float a0 = 0, a1 = 0, a2 = 0;           // dec x dec guide block, clamped
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
    /* TEXTURE at the FINE scale: each pixel's distance from its own 3x3 mean, then
       averaged over the analysis window. The first cut took the distance from the
       analysis BLUR instead, and a plain step edge reads as texture under that -- the
       blur smears the step, so a band ks wide either side of every hard edge came out
       "textured", formed a class of its own, and stopped the fill short of a clean
       wall-to-wall edge by 10-30 px (2026-10-14). Against its 3x3 mean a step deviates
       only on its own two pixel columns, which the averaging then dilutes, while grain,
       gravel and fur deviate at every pixel. */
    static thread_local std::vector<float> fine;
    fine.resize(en);
    for (size_t k = 0; k < en; ++k) fine[k] = ext[k * 3];
    boxBlur(fine.data(), tmpbuf, ew, eh, 1, 1);
    tex.assign(en, 0.0f);
    for (size_t k = 0; k < en; ++k) tex[k] = std::abs(ext[k * 3] - fine[k]);
    boxBlur(tex.data(), tmpbuf, ew, eh, 1, ks);

    /* The FEATURE vector of every box pixel, cropped out of the padded planes: the
       weights are folded in here so every distance below is a plain Euclidean one. */
    constexpr int kF = 4;
    const float sc = float(std::sqrt(kChromaW2));
    const float tk = float(kTexLogK), invT = float(1.0 / kTexLogT);
    static thread_local std::vector<float> feat;
    feat.assign(n * kF, 0.0f);
    for (int y = 0; y < bh; ++y) {
        const float *si = blr.data() + (size_t(y + pad) * ew + pad) * 3;
        const float *ti = tex.data() +  size_t(y + pad) * ew + pad;
        float *o = feat.data() + size_t(y) * bw * kF;
        for (int x = 0; x < bw; ++x, si += 3, o += kF) {
            o[0] = si[0]; o[1] = sc * si[1]; o[2] = sc * si[2];
            o[3] = tk * std::log2(1.0f + ti[x] * invT);
        }
    }
    auto fv = [&](size_t k) { return feat.data() + k * kF; };
    auto d2 = [](const float *a, const float *b) {
        float s = 0.0f;
        for (int c = 0; c < kF; ++c) { const float q = a[c] - b[c]; s += q * q; }
        return s;
    };

    /* Zones: radius of every box pixel from the centre. disc = the whole dab (the
       classes are learned over it), core = the search zone (model step 1), seed = the
       small disc under the crosshair that names the subject. */
    static thread_local std::vector<float> rad;
    rad.assign(n, 0.0f);
    for (int y = 0; y < bh; ++y)
        for (int x = 0; x < bw; ++x)
            rad[size_t(y) * bw + x] = float(std::hypot(x - cxa, y - cya));
    const float rDisc = float(rf), rCore = float(rcf);
    const float rSeed = float(std::max(1.5, kSeedFrac * rcf));

    /* Reference = per-feature median over the seed disc. */
    static thread_local std::vector<size_t> seedIdx, discIdx;
    seedIdx.clear(); discIdx.clear();
    for (size_t k = 0; k < n; ++k) {
        if (rad[k] <= rSeed) seedIdx.push_back(k);
        if (rad[k] <= rDisc) discIdx.push_back(k);
    }
    if (seedIdx.empty() || discIdx.size() < size_t(kClusters)) return;
    float ref[kF];
    static thread_local std::vector<float> chan;
    for (int c = 0; c < kF; ++c) {
        chan.clear();
        for (size_t k : seedIdx) chan.push_back(fv(k)[c]);
        auto mid = chan.begin() + long(chan.size() / 2);
        std::nth_element(chan.begin(), mid, chan.end());
        ref[c] = *mid;
    }

    /* The classes are learned from FLAT pixels only: a pixel whose feature gradient is
       above kGradK x the disc's median sits on a boundary, not in a class. The analysis
       blur turns a hard edge into a band of in-between colours, and k-means gave that
       band clusters of its own, close enough to each side to CHAIN the merge across a
       plain lightness edge (a light wall against a dark one painted both). The
       transition pixels are still classified below; they just do not vote. A sky
       gradient is uniform, so nothing is dropped there; if over half the disc would be
       dropped the test is skipped. */
    static thread_local std::vector<float> grad;
    grad.assign(n, 0.0f);
    for (int y = 0; y < bh; ++y) {
        for (int x = 0; x < bw; ++x) {
            const float *l = fv(size_t(y) * bw + std::max(0, x - 1));
            const float *r = fv(size_t(y) * bw + std::min(bw - 1, x + 1));
            const float *u = fv(size_t(std::max(0, y - 1)) * bw + x);
            const float *d = fv(size_t(std::min(bh - 1, y + 1)) * bw + x);
            grad[size_t(y) * bw + x] = 0.5f * std::sqrt(d2(r, l) + d2(d, u));
        }
    }
    chan.clear();
    for (size_t k : discIdx) chan.push_back(grad[k]);
    auto gmid = chan.begin() + long((chan.size() - 1) / 2);
    std::nth_element(chan.begin(), gmid, chan.end());
    const float flatMax = std::max(float(kGradK) * *gmid, float(kGradFloor));
    static thread_local std::vector<size_t> flatIdx;
    flatIdx.clear();
    for (size_t k : discIdx) if (grad[k] <= flatMax) flatIdx.push_back(k);
    const std::vector<size_t> &learn =
        flatIdx.size() * 2 >= discIdx.size() ? flatIdx : discIdx;

    /* Model step 3: deterministic k-means over (a subsample of) the flat disc pixels.
       Farthest-point seeding from the reference: no RNG, so the same dab always gives
       the same field. */
    const size_t stride = (learn.size() + kKmSamples - 1) / kKmSamples;
    static thread_local std::vector<size_t> sIdx;
    sIdx.clear();
    for (size_t i = 0; i < learn.size(); i += stride) sIdx.push_back(learn[i]);
    const size_t ns = sIdx.size();
    float C[kClusters][kF];
    std::copy(ref, ref + kF, C[0]);
    static thread_local std::vector<float> nearest;
    nearest.assign(ns, std::numeric_limits<float>::max());
    for (int j = 1; j < kClusters; ++j) {
        size_t far = 0;
        float farD = -1.0f;
        for (size_t i = 0; i < ns; ++i) {
            nearest[i] = std::min(nearest[i], d2(fv(sIdx[i]), C[j - 1]));
            if (nearest[i] > farD) { farD = nearest[i]; far = i; }
        }
        std::copy(fv(sIdx[far]), fv(sIdx[far]) + kF, C[j]);
    }
    static thread_local std::vector<int> lab;
    lab.assign(ns, 0);
    auto assign = [&]() {
        for (size_t i = 0; i < ns; ++i) {
            const float *p = fv(sIdx[i]);
            int best = 0;
            float bd = d2(p, C[0]);
            for (int j = 1; j < kClusters; ++j) {
                const float dj = d2(p, C[j]);
                if (dj < bd) { bd = dj; best = j; }
            }
            lab[i] = best;
        }
    };
    for (int it = 0; it < kKmIters; ++it) {
        assign();
        double sum[kClusters][kF] = {};
        int cnt[kClusters] = {};
        for (size_t i = 0; i < ns; ++i) {
            const float *p = fv(sIdx[i]);
            for (int c = 0; c < kF; ++c) sum[lab[i]][c] += p[c];
            ++cnt[lab[i]];
        }
        for (int j = 0; j < kClusters; ++j)           // an empty cluster keeps its centre
            if (cnt[j]) for (int c = 0; c < kF; ++c) C[j][c] = float(sum[j][c] / cnt[j]);
    }
    assign();

    /* Spread of each cluster in the MERGE metric (Y down-weighted), and the merge
       distance between two centres in that metric. */
    const float wm[kF] = { float(kMergeYW), 1.0f, 1.0f, 1.0f };
    double spread[kClusters] = {};
    {
        double ss[kClusters] = {};
        int cnt[kClusters] = {};
        for (size_t i = 0; i < ns; ++i) {
            const float *p = fv(sIdx[i]);
            double s = 0.0;
            for (int c = 0; c < kF; ++c) {
                const double q = (p[c] - C[lab[i]][c]) * wm[c];
                s += q * q;
            }
            ss[lab[i]] += s;
            ++cnt[lab[i]];
        }
        for (int j = 0; j < kClusters; ++j)
            spread[j] = cnt[j] ? std::sqrt(ss[j] / cnt[j]) : 0.0;
    }
    auto mergeDist = [&](int a, int b) {
        double s = 0.0;
        for (int c = 0; c < kF; ++c) {
            const double q = (C[a][c] - C[b][c]) * wm[c];
            s += q * q;
        }
        return std::sqrt(s);
    };

    /* The SUBJECT: clusters holding kShareMin of the seed disc, plus the one nearest it
       on average, closed under the merge. */
    int share[kClusters] = {};
    double seedMean[kClusters] = {};
    for (size_t k : seedIdx) {
        const float *p = fv(k);
        int best = 0;
        float bd = std::numeric_limits<float>::max();
        for (int j = 0; j < kClusters; ++j) {
            const float dj = d2(p, C[j]);
            seedMean[j] += std::sqrt(dj);
            if (dj < bd) { bd = dj; best = j; }
        }
        ++share[best];
    }
    bool subj[kClusters] = {};
    int nearestJ = 0;
    for (int j = 0; j < kClusters; ++j) {
        if (share[j] >= kShareMin * double(seedIdx.size())) subj[j] = true;
        if (seedMean[j] < seedMean[nearestJ]) nearestJ = j;
    }
    subj[nearestJ] = true;
    for (bool grew = true; grew; ) {
        grew = false;
        for (int j = 0; j < kClusters; ++j) {
            if (subj[j]) continue;
            for (int s = 0; s < kClusters; ++s) {
                if (!subj[s]) continue;
                const double lim =
                    std::max(kMergeMin, kMergeSig * 0.5 * (spread[s] + spread[j]));
                if (mergeDist(j, s) < lim) { subj[j] = true; grew = true; break; }
            }
        }
    }
    const bool anyOther = std::find(subj, subj + kClusters, false) != subj + kClusters;

    f.x0 = x0; f.y0 = y0; f.w = bw; f.h = bh; f.dec = dec;
    f.a.assign(n, 0.0f);
    /* accOwn = each pixel's own class acceptance (model step 4), kept for the feather
       band; 1 everywhere when the dab holds one surface only. */
    static thread_local std::vector<float> accOwn;
    accOwn.assign(n, 1.0f);

    if (!anyOther) {
        for (size_t k = 0; k < n; ++k) if (rad[k] <= rCore) f.a[k] = 1.0f;
        /* No other class was LEARNED -- but one can still sit in the feather band as a
           sliver too thin to have a flat pixel (every pixel of it is on the boundary
           and did not vote). So the band's gate is absolute here: a band pixel must lie
           within the merge distance of a subject centre, fading out to twice it. */
        double lim = kMergeMin;
        for (int j = 0; j < kClusters; ++j)
            lim = std::max(lim, kMergeSig * spread[j]);
        const float invLim = float(1.0 / lim);
        for (size_t k = 0; k < n; ++k) {
            if (rad[k] <= rCore) continue;               // only the band reads accOwn
            const float *p = fv(k);
            double best = std::numeric_limits<double>::max();
            for (int j = 0; j < kClusters; ++j) {
                double s2 = 0.0;
                for (int c = 0; c < kF; ++c) {
                    const double q = (p[c] - C[j][c]) * wm[c];
                    s2 += q * q;
                }
                best = std::min(best, s2);
            }
            accOwn[k] = std::clamp(2.0f - float(std::sqrt(best)) * invLim, 0.0f, 1.0f);
        }
    }
    else {
        /* Model step 4: relative likeness, its acceptance, and the fill's step cost. */
        static thread_local std::vector<int> step;
        step.assign(n, 0);
        const float invRamp = 1.0f / (kPHi - kPLo);
        for (size_t k = 0; k < n; ++k) {
            const float *p = fv(k);
            float dS = std::numeric_limits<float>::max(), dO = dS;
            for (int j = 0; j < kClusters; ++j) {
                const float dj = d2(p, C[j]);
                if (subj[j]) dS = std::min(dS, dj); else dO = std::min(dO, dj);
            }
            dS = std::sqrt(dS); dO = std::sqrt(dO);
            const float pl = dO / (dS + dO + 1e-6f);
            accOwn[k] = std::clamp((pl - kPLo) * invRamp, 0.0f, 1.0f);
            const double e = std::max(0.0, 1.0 - 2.0 * pl);
            step[k] = int(std::lround(kStepK * e * e * kCostLevels));
        }

        /* Model step 5: cost fill inside the core -- a monotone dial queue (all costs
           >= 0, so buckets are visited in order and a pixel is final when its bucket
           comes up): O(box), no heap. */
        static thread_local std::vector<int> cost;
        static thread_local std::vector<std::vector<int>> bucket;
        const int kUnreached = kCostLevels + 1;
        cost.assign(n, kUnreached);
        if (int(bucket.size()) != kCostLevels + 1) bucket.resize(kCostLevels + 1);
        for (auto &b : bucket) b.clear();
        auto open = [&](size_t k) { return rad[k] <= rCore && accOwn[k] > 0.0f; };
        for (size_t k : seedIdx) {
            if (!open(k) || accOwn[k] <= 0.5f) continue;
            cost[k] = 0;
            bucket[0].push_back(int(k));
        }
        if (bucket[0].empty()) return;       // the crosshair is on no class: unconfined

        static const int dxs[4] = { 1, -1, 0, 0 }, dys[4] = { 0, 0, 1, -1 };
        for (int b = 0; b <= kCostLevels; ++b) {
            /* Index, not iterator: a zero-cost step appends to this bucket. */
            for (size_t qi = 0; qi < bucket[b].size(); ++qi) {
                const int k = bucket[b][qi];
                if (cost[k] != b) continue;           // superseded by a cheaper route
                const int cy = k / bw, cx = k - cy * bw;
                for (int t = 0; t < 4; ++t) {
                    const int nx = cx + dxs[t], ny = cy + dys[t];
                    if (nx < 0 || ny < 0 || nx >= bw || ny >= bh) continue;
                    const size_t nk = size_t(ny) * bw + nx;
                    if (!open(nk)) continue;
                    const int nc = b + step[nk];
                    if (nc > kCostLevels || nc >= cost[nk]) continue;
                    cost[nk] = nc;
                    bucket[nc].push_back(int(nk));
                }
            }
            bucket[b].clear();                        // never pushed to again (nc >= b)
        }

        /* Acceptance: how much of the budget the cheapest route had left, capped by the
           class ramp, smoothstepped so the mask fades as the budget runs out rather than
           ending on a contour of its own. */
        for (size_t k = 0; k < n; ++k) {
            if (cost[k] > kCostLevels) continue;
            const float left = 1.0f - float(cost[k]) / float(kCostLevels);
            f.a[k] = std::min(accOwn[k], left * left * (3.0f - 2.0f * left));
        }
    }

    /* Model step 1, the FEATHER BAND: each pixel beyond the core takes the core value on
       its own radius just inside the rim, gated by its own class acceptance. Written into
       a copy so the band only ever reads core values. */
    if (rcf < rf - 0.5) {
        static thread_local std::vector<float> core;
        core = f.a;
        const double rin = std::max(rcf - 1.5, 0.0);
        for (int y = 0; y < bh; ++y) {
            for (int x = 0; x < bw; ++x) {
                const size_t k = size_t(y) * bw + x;
                if (rad[k] <= rCore) continue;
                const double s = rin / std::max(double(rad[k]), 1e-6);
                const int px =
                    std::clamp(int(std::lround(cxa + (x - cxa) * s)), 0, bw - 1);
                const int py =
                    std::clamp(int(std::lround(cya + (y - cya) * s)), 0, bh - 1);
                f.a[k] = std::min(core[size_t(py) * bw + px], accOwn[k]);
            }
        }
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
       the GUI both stamp dabs. The CORE radius is the falloff's half-coverage radius --
       the cursor's inner ring -- and is the auto-mask's search zone (see the model). */
    const bool auto_ = am && am->on && am->g && am->g->valid();
    static thread_local DabField field;
    static thread_local const Guide *fieldGuide = nullptr;
    static thread_local double fieldCx = 0, fieldCy = 0, fieldR = -1, fieldRc = -1;
    double gcx = 0, gcy = 0, grad = 0;
    GuideMap gmap{};
    if (auto_) {
        gmap = guideMapFor(*am, w, h);
        targetToGuide(gmap, cx, cy, gcx, gcy);
        grad = radius * double(std::max(am->g->w, am->g->h)) / std::max(w, h);
        const double grc = f > 0.0 ? grad * MaskFalloff::shapeFor(f * 100.0).h : grad;
        if (fieldGuide != am->g || std::abs(grad - fieldR) > 0.01 ||
            std::abs(grc - fieldRc) > 0.01 ||
            std::abs(gcx - fieldCx) > 0.5 || std::abs(gcy - fieldCy) > 0.5) {
            buildDabField(*am->g, gcx, gcy, grad, grc, field);
            fieldGuide = am->g; fieldCx = gcx; fieldCy = gcy;
            fieldR = grad; fieldRc = grc;
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
