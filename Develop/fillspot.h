#ifndef FILLSPOT_H
#define FILLSPOT_H

#include <vector>
#include <cmath>
#include <algorithm>
#include <QString>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

/*
    Shared geometry for the Develop regenerative-fill "spot" tool. A spot is a brush stroke:
    a polyline of output-normalized points (0..1), a diameter (fraction of the image long
    edge), and a feather (0..100). This header rasterizes that stroke to a soft coverage mask
    and is included by BOTH the ImageView live preview and the render (miganfill.cpp), so the
    spot the user paints is pixel-identical to the one healed -- the same preview==render
    discipline as objectmask.h.

    paramsJson schema (opaque in EditStack::FillSpot):
      { "size": <double diameter / long edge, 0..1>,
        "feather": <double 0..100>,
        "pts": [x0,y0, x1,y1, ...] }        // output-normalized 0..1
*/
namespace FillSpotGeom {

struct Parsed {
    double sizeFrac   = 0.04;   // diameter / long edge
    double featherPct = 50.0;
    std::vector<double> pts;    // flat x,y in output-normalized 0..1
    bool valid() const { return pts.size() >= 2; }
};

inline Parsed parse(const QString &paramsJson)
{
    Parsed p;
    const QJsonObject o = QJsonDocument::fromJson(paramsJson.toUtf8()).object();
    p.sizeFrac   = o.value("size").toDouble(p.sizeFrac);
    p.featherPct = o.value("feather").toDouble(p.featherPct);
    const QJsonArray a = o.value("pts").toArray();
    p.pts.reserve(a.size());
    for (const QJsonValue &v : a) p.pts.push_back(v.toDouble());
    return p;
}

inline QString toJson(double sizeFrac, double featherPct, const std::vector<double> &pts)
{
    QJsonArray a;
    for (double v : pts) a.append(v);
    QJsonObject o;
    o["size"] = sizeFrac; o["feather"] = featherPct; o["pts"] = a;
    return QString::fromUtf8(QJsonDocument(o).toJson(QJsonDocument::Compact));
}

/* Distance from (px,py) to segment (ax,ay)-(bx,by). */
inline double distSeg(double px, double py, double ax, double ay, double bx, double by)
{
    const double dx = bx - ax, dy = by - ay;
    const double L2 = dx * dx + dy * dy;
    double t = L2 > 0 ? ((px - ax) * dx + (py - ay) * dy) / L2 : 0.0;
    t = std::clamp(t, 0.0, 1.0);
    const double cx = ax + t * dx, cy = ay + t * dy;
    return std::hypot(px - cx, py - cy);
}

/*
    Rasterize the stroke to a soft coverage mask at (w,h). radius_px = 0.5*size*maxDim; the
    feather band (featherPct of the radius) softens the edge with a smoothstep. Fills cov
    (row-major w*h, 0..1) and the inclusive nonzero bbox [bx0,by0,bx1,by1]; empty when bx1<bx0.
*/
inline void rasterize(const Parsed &p, int w, int h, std::vector<float> &cov,
                      int &bx0, int &by0, int &bx1, int &by1)
{
    cov.assign(size_t(w) * size_t(h), 0.0f);
    bx0 = w; by0 = h; bx1 = -1; by1 = -1;                 // empty
    if (!p.valid() || w <= 0 || h <= 0) return;

    const double maxDim = std::max(w, h);
    const double r    = 0.5 * std::max(p.sizeFrac, 1e-4) * maxDim;
    const double band = std::max(1.0, r * std::clamp(p.featherPct, 0.0, 100.0) / 100.0);
    const int n = int(p.pts.size() / 2);

    double minx = 1e18, miny = 1e18, maxx = -1e18, maxy = -1e18;
    for (int i = 0; i < n; ++i) {
        const double x = p.pts[2 * i] * w, y = p.pts[2 * i + 1] * h;
        minx = std::min(minx, x); miny = std::min(miny, y);
        maxx = std::max(maxx, x); maxy = std::max(maxy, y);
    }
    const int ix0 = std::max(0,     int(std::floor(minx - r - band)));
    const int iy0 = std::max(0,     int(std::floor(miny - r - band)));
    const int ix1 = std::min(w - 1, int(std::ceil (maxx + r + band)));
    const int iy1 = std::min(h - 1, int(std::ceil (maxy + r + band)));

    for (int y = iy0; y <= iy1; ++y) {
        for (int x = ix0; x <= ix1; ++x) {
            double d = 1e18;
            if (n == 1) {
                d = std::hypot(x + 0.5 - p.pts[0] * w, y + 0.5 - p.pts[1] * h);
            } else {
                for (int i = 1; i < n; ++i)
                    d = std::min(d, distSeg(x + 0.5, y + 0.5,
                                            p.pts[2 * (i - 1)] * w, p.pts[2 * (i - 1) + 1] * h,
                                            p.pts[2 * i]       * w, p.pts[2 * i + 1]       * h));
            }
            float a;
            if      (d <= r - band) a = 1.0f;
            else if (d >= r)        a = 0.0f;
            else { const double t = (r - d) / band; a = float(t * t * (3.0 - 2.0 * t)); }
            if (a > 0.0f) {
                cov[size_t(y) * w + x] = a;
                bx0 = std::min(bx0, x); by0 = std::min(by0, y);
                bx1 = std::max(bx1, x); by1 = std::max(by1, y);
            }
        }
    }
}

} // namespace FillSpotGeom

#endif // FILLSPOT_H
