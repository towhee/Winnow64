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
    size/200 * longEdge). feather is the dab edge softness fraction (feather/100).
*/

#include <vector>
#include <cmath>
#include <algorithm>
#include <QPointF>
#include <QJsonArray>
#include <QJsonObject>

namespace BrushStamp {

/* Coverage 0..1 of a dab at distance `dist` (px) from centre: solid core out to radius*(1-f), then
   a smootherstep falloff to 0 at radius. */
inline float coverage(double dist, double radius, double f)
{
    if (radius <= 0.0) return 0.0f;
    const double inner = radius * (1.0 - f);
    if (dist <= inner)  return 1.0f;
    if (dist >= radius) return 0.0f;
    double s = (dist - inner) / (radius - inner);          // 0..1 across the feather band
    s = s * s * s * (s * (s * 6.0 - 15.0) + 10.0);         // smootherstep
    return float(1.0 - s);
}

/* Accumulate a single dab into a per-stroke coverage buffer by MAX. */
inline void dabMax(float *cov, int w, int h, double cx, double cy, double radius, double f)
{
    if (radius <= 0.0) return;
    const int x0 = std::max(0,     int(std::floor(cx - radius)));
    const int x1 = std::min(w - 1, int(std::ceil (cx + radius)));
    const int y0 = std::max(0,     int(std::floor(cy - radius)));
    const int y1 = std::min(h - 1, int(std::ceil (cy + radius)));
    for (int y = y0; y <= y1; ++y) {
        float *row = cov + size_t(y) * w;
        const double dy = y + 0.5 - cy;
        for (int x = x0; x <= x1; ++x) {
            const double dx = x + 0.5 - cx;
            const float c = coverage(std::sqrt(dx*dx + dy*dy), radius, f);
            if (c > row[x]) row[x] = c;
        }
    }
}

/* Accumulate a stroke segment p0->p1 (px) into the coverage buffer, dabs spaced ~radius/4. */
inline void segmentMax(float *cov, int w, int h, QPointF p0, QPointF p1, double radius, double f)
{
    const double len = std::hypot(p1.x() - p0.x(), p1.y() - p0.y());
    const double step = std::max(1.0, radius * 0.25);
    const int n = std::max(1, int(len / step));
    for (int i = 0; i <= n; ++i) {
        const double t = double(i) / n;
        dabMax(cov, w, h, p0.x() + (p1.x()-p0.x())*t, p0.y() + (p1.y()-p0.y())*t, radius, f);
    }
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
   w*h buffer for the per-stroke coverage. */
inline void rasterize(const QJsonArray &strokes, float *mask, std::vector<float> &scratch,
                      int w, int h, int degrees)
{
    if (w <= 0 || h <= 0) return;
    const double longEdge = std::max(w, h);
    scratch.assign(size_t(w) * h, 0.0f);
    for (const QJsonValue &sv : strokes) {
        const QJsonObject so = sv.toObject();
        const QJsonArray pts = so.value("pts").toArray();
        if (pts.size() < 2) continue;
        const double size = so.value("size").toDouble(20);
        const double f    = std::clamp(so.value("feather").toDouble(50) / 100.0, 0.0, 1.0);
        const double flow = std::clamp(so.value("flow").toDouble(50) / 100.0, 0.0, 1.0);
        const bool  erase = so.value("erase").toBool(false);
        const double radius = (size / 200.0) * longEdge;

        std::fill(scratch.begin(), scratch.end(), 0.0f);
        QPointF prev = point(pts, 0, degrees, w, h);
        dabMax(scratch.data(), w, h, prev.x(), prev.y(), radius, f);
        for (int i = 1; i*2 + 1 < pts.size(); ++i) {
            const QPointF cur = point(pts, i, degrees, w, h);
            segmentMax(scratch.data(), w, h, prev, cur, radius, f);
            prev = cur;
        }
        composite(mask, scratch.data(), size_t(w) * h, flow, erase);
    }
}

} // namespace BrushStamp

#endif // BRUSHSTAMP_H
