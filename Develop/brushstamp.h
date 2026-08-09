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
    size/200 * longEdge = the OUTER extent). feather (feather/100) softens INWARD Lightroom-style:
    full-coverage core out to radius*(1-feather/100), smootherstep falloff to 0 at radius -- so
    feather=0 is a crisp edge at the size circle and feather=100 is fully soft (see coverage()).
*/

#include <vector>
#include <cmath>
#include <algorithm>
#include <memory>
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
   A small luminance map of the displayed image (output-normalized orientation), shared between the
   ImageView live preview and the develop render so auto-masked strokes evaluate identically. It is
   computed once (when the brush activates) from the loupe pixmap and registered by image path. */
struct Guide {
    std::vector<float> lum;     // 0..1 luminance, row-major, output-oriented
    int w = 0, h = 0;
    bool valid() const { return w > 0 && h > 0 && lum.size() == size_t(w) * h; }
};

inline QMutex &guideMutex() { static QMutex m; return m; }
inline QHash<QString, std::shared_ptr<const Guide>> &guideStore()
{ static QHash<QString, std::shared_ptr<const Guide>> s; return s; }

inline void putGuide(const QString &path, std::shared_ptr<const Guide> g)
{
    QMutexLocker lk(&guideMutex());
    if (guideStore().size() > 8) guideStore().clear();   // crude cap (each guide is a few MB)
    guideStore().insert(path, std::move(g));
}

inline std::shared_ptr<const Guide> getGuide(const QString &path)
{
    QMutexLocker lk(&guideMutex());
    auto it = guideStore().find(path);
    return it != guideStore().end() ? it.value() : nullptr;
}

/* ---- SAM auto-mask field (2nd auto-mask mode: "AI") ----
   For the "AI" auto-mask a stroke is confined to the SAM-segmented object under its START point,
   rather than to a luminance band. That object coverage is a 0..1 field in output-normalized space
   -- the same shape as a luminance Guide -- so it reuses the Guide struct. Unlike the single per-
   image luminance guide, each AI stroke has its OWN field (keyed by its seed point), decoded by
   MW::ensureBrushSamField (SAM 2 point prompt) and read here by rasterize(). Populated on the GUI
   thread; read (getSamField) from the render worker -- both mutex-guarded. Reuses guideMutex. */
inline QHash<QString, std::shared_ptr<const Guide>> &samFieldStore()
{ static QHash<QString, std::shared_ptr<const Guide>> s; return s; }

/* Stable key for a stroke's SAM field: path + rounded seed point (must match between the ImageView
   preview and the develop render so they sample the SAME field). */
inline QString samFieldKey(const QString &path, double onx, double ony)
{
    return path + "|sam|" + QString::number(onx, 'f', 4) + "," + QString::number(ony, 'f', 4);
}

inline void putSamField(const QString &key, std::shared_ptr<const Guide> f)
{
    QMutexLocker lk(&guideMutex());
    if (samFieldStore().size() > 16) samFieldStore().clear();   // crude cap (a few MB each)
    samFieldStore().insert(key, std::move(f));
}

inline std::shared_ptr<const Guide> getSamField(const QString &key)
{
    QMutexLocker lk(&guideMutex());
    auto it = samFieldStore().find(key);
    return it != samFieldStore().end() ? it.value() : nullptr;
}

/* Per-stroke auto-mask state: limit a dab to pixels whose luminance is near the stroke-start
   luminance (lumRef), within tol. degrees maps a TARGET pixel back to output-normalized (0 for the
   output-space preview; the render's EXIF degrees for work space). */
struct AutoMaskCtx {
    const float *guide = nullptr;
    int    gw = 0, gh = 0;
    float  lumRef = 0.0f;
    float  tol = 0.15f;
    int    degrees = 0;
    bool   on = false;
    bool   aiField = false;    // guide is a SAM object coverage field (0..1), not a luminance map
};

inline float guideLumAt(const AutoMaskCtx &a, double onx, double ony)
{
    int gx = std::clamp(int(onx * a.gw), 0, a.gw - 1);
    int gy = std::clamp(int(ony * a.gh), 0, a.gh - 1);
    return a.guide[size_t(gy) * a.gw + gx];
}

inline float edgeFactor(const AutoMaskCtx &a, int x, int y, int w, int h)
{
    const double tnx = (x + 0.5) / w, tny = (y + 0.5) / h;
    double onx, ony;
    switch (a.degrees) {                 // target-norm -> output-norm (CW), matches buildMaskBuffer
        case 90:  onx = 1.0 - tny; ony = tnx;       break;
        case 180: onx = 1.0 - tnx; ony = 1.0 - tny; break;
        case 270: onx = tny;       ony = 1.0 - tnx; break;
        default:  onx = tnx;       ony = tny;       break;
    }
    if (a.aiField)                       // AI mode: the field IS the confinement coverage (0..1)
        return guideLumAt(a, onx, ony);
    const double d = std::abs(double(guideLumAt(a, onx, ony)) - double(a.lumRef));
    const double half = a.tol * 0.5;
    if (d <= half)    return 1.0f;
    if (d >= a.tol)   return 0.0f;
    double s = (d - half) / half;
    s = s * s * s * (s * (s * 6.0 - 15.0) + 10.0);   // smootherstep falloff at the edge
    return float(1.0 - s);
}

/* ---- Dab cost counters (diagnostic) ----
   Two relaxed atomic adds per DAB (not per pixel), so the cost is nothing next to the dab
   itself. MW's [DevTime] line reports and resets them around a mask build, which is the
   only way to tell "too many dabs" from "each dab is the whole image" -- the two have very
   different fixes and static reading cannot distinguish them. Renders on the GUI thread and
   the settle worker share the counters, so a settle landing mid-drag can inflate one line. */
struct DabStats { std::atomic<quint64> dabs{0}; std::atomic<quint64> pixels{0}; };
inline DabStats &dabStats() { static DabStats s; return s; }

/* Coverage 0..1 of a dab at distance `dist` (px) from centre -- LIGHTROOM model: `radius` is the
   OUTER extent (the brush SIZE; coverage reaches 0 there and does NOT grow with feather). Feather
   f=feather/100 softens INWARD: the full-coverage core boundary is radius*(1-f), so f=0 is a hard
   edge at radius and f=1 is fully soft (core collapses to the centre). Smootherstep falloff from the
   core to the outer edge. (Cursor's inner ring = the half-coverage radius radius*(1-f/2).) */
inline float coverage(double dist, double radius, double f)
{
    if (radius <= 0.0)   return 0.0f;
    if (dist >= radius)  return 0.0f;
    const double inner = radius * (1.0 - f);               // full-coverage core boundary
    if (dist <= inner)   return 1.0f;
    double s = (dist - inner) / (radius - inner);          // 0..1 across the feather band
    s = s * s * s * (s * (s * 6.0 - 15.0) + 10.0);         // smootherstep
    return float(1.0 - s);
}

/* Accumulate a single dab into a per-stroke coverage buffer by MAX. With an active AutoMaskCtx the
   coverage is attenuated by the edge factor (paints only near-luminance pixels).

   THIS IS THE BRUSH HOT LOOP. A dab's box is (2*radius)^2 pixels clamped to the buffer,
   so a large brush touches most of the image PER DAB, and the develop render replays
   every dab of the stroke on every interactive tick. Two things keep it honest:

     - SQUARED-DISTANCE GATES. The outside and full-core tests use d^2, so the sqrt is
       paid only by pixels actually in the feather band. At feather 0 (the default)
       inner == radius and NO pixel pays it. Numerically identical to coverage(), which
       is kept for the callers that need a distance-based value.
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
    const bool auto_ = am && am->on && am->guide;
    const double r2    = radius * radius;
    const double inner = radius * (1.0 - f);          // full-coverage core boundary
    const double in2   = inner * inner;
    const double band  = radius - inner;              // feather width; 0 when f == 0

    auto rows = [=](int ya, int yb) {
        for (int y = ya; y <= yb; ++y) {
            float *row = cov + size_t(y) * w;
            const double dy = y + 0.5 - cy;
            const double dy2 = dy * dy;
            for (int x = x0; x <= x1; ++x) {
                const double dx = x + 0.5 - cx;
                const double d2 = dx * dx + dy2;
                if (d2 >= r2) continue;               // outside the dab
                float c;
                if (d2 <= in2) c = 1.0f;              // full-coverage core: no sqrt
                else {
                    double s = (std::sqrt(d2) - inner) / band;
                    s = s * s * s * (s * (s * 6.0 - 15.0) + 10.0);   // smootherstep
                    c = float(1.0 - s);
                    if (c <= 0.0f) continue;
                }
                if (auto_) { c *= edgeFactor(*am, x, y, w, h); if (c <= 0.0f) continue; }
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
                      int w, int h, int degrees, const Guide *guide = nullptr,
                      const QString &fPath = QString())
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

        AutoMaskCtx am;
        std::shared_ptr<const Guide> samHold;    // keep the AI field alive across this stroke's dabs
        if (so.value("autoMask").toBool(false)) {
            if (so.value("autoMaskMode").toString("lum") == "ai") {
                /* AI mode: confine to the SAM object under the stroke's seed (per-stroke field). If
                   the field is not decoded yet (cold render), paint unconfined. */
                samHold = getSamField(samFieldKey(fPath, pts.at(0).toDouble(), pts.at(1).toDouble()));
                if (samHold && samHold->valid()) {
                    am.on = true; am.aiField = true;
                    am.guide = samHold->lum.data(); am.gw = samHold->w; am.gh = samHold->h;
                    am.degrees = degrees;
                }
            }
            else if (guide && guide->valid()) {
                am.on = true; am.guide = guide->lum.data(); am.gw = guide->w; am.gh = guide->h;
                am.degrees = degrees;
                am.lumRef = guideLumAt(am, pts.at(0).toDouble(), pts.at(1).toDouble());
            }
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
