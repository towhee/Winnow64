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
        "kind": "spot" | "fill" | "object",   // Replace panel mode; engine choice
        "pts": [x0,y0, x1,y1, ...],           // single stroke, output-normalized 0..1
        "strokes": [ { "size": <double>, "erase": <bool>,
                       "pts": [x0,y0, ...] }, ... ] }

    Two geometry forms: "pts" is one brush stroke (Spot click / Object drag, and legacy
    spots); "strokes" is a PAINTED AREA (Fill mode) -- an ordered list of strokes, each
    with its own brush size and an erase flag (Opt/Alt while painting), composited in
    order: add = max, erase = multiply by (1 - alpha). When both are present "strokes"
    wins.

    kind decides the heal engine (lamafill.cpp): "spot"/"fill" heal by exemplar CLONE
    (real neighbouring texture) with the model as fallback; "object" is a regenerative
    model fill (an object leaves no clean source to clone). Absent in legacy spots ->
    "fill".
*/
namespace FillSpotGeom {

enum Kind { Spot = 0, Fill = 1, Object = 2 };

inline const char *kindName(int k)
{
    return k == Spot ? "spot" : k == Object ? "object" : "fill";
}

struct Stroke {
    double sizeFrac = 0.04;     // this stroke's brush diameter / long edge
    bool   erase    = false;    // subtract from the painted area instead of adding
    std::vector<double> pts;    // flat x,y in output-normalized 0..1
};

struct Parsed {
    double sizeFrac   = 0.04;   // diameter / long edge (single-stroke form)
    double featherPct = 50.0;
    int    kind       = Fill;   // Kind; legacy spots parse as Fill
    std::vector<double> pts;    // single-stroke form: flat x,y 0..1
    std::vector<Stroke> strokes;// painted-area form (takes precedence when non-empty)
    bool valid() const {
        for (const Stroke &s : strokes)
            if (!s.erase && s.pts.size() >= 2) return true;
        return strokes.empty() && pts.size() >= 2;
    }
};

inline Parsed parse(const QString &paramsJson)
{
    Parsed p;
    const QJsonObject o = QJsonDocument::fromJson(paramsJson.toUtf8()).object();
    p.sizeFrac   = o.value("size").toDouble(p.sizeFrac);
    p.featherPct = o.value("feather").toDouble(p.featherPct);
    const QString k = o.value("kind").toString();
    p.kind = (k == "spot") ? Spot : (k == "object") ? Object : Fill;
    const QJsonArray a = o.value("pts").toArray();
    p.pts.reserve(a.size());
    for (const QJsonValue &v : a) p.pts.push_back(v.toDouble());
    for (const QJsonValue &sv : o.value("strokes").toArray()) {
        const QJsonObject so = sv.toObject();
        Stroke st;
        st.sizeFrac = so.value("size").toDouble(st.sizeFrac);
        st.erase    = so.value("erase").toBool(false);
        const QJsonArray sp = so.value("pts").toArray();
        st.pts.reserve(sp.size());
        for (const QJsonValue &v : sp) st.pts.push_back(v.toDouble());
        if (st.pts.size() >= 2) p.strokes.push_back(std::move(st));
    }
    return p;
}

inline QString toJson(double sizeFrac, double featherPct, const std::vector<double> &pts,
                      int kind = Fill)
{
    QJsonArray a;
    for (double v : pts) a.append(v);
    QJsonObject o;
    o["size"] = sizeFrac; o["feather"] = featherPct; o["kind"] = kindName(kind); o["pts"] = a;
    return QString::fromUtf8(QJsonDocument(o).toJson(QJsonDocument::Compact));
}

/* Painted-area form (Fill mode): the ordered add/erase stroke list. */
inline QString toJson(double featherPct, int kind, const std::vector<Stroke> &strokes)
{
    QJsonArray sa;
    for (const Stroke &st : strokes) {
        QJsonArray sp;
        for (double v : st.pts) sp.append(v);
        QJsonObject so;
        so["size"] = st.sizeFrac; so["erase"] = st.erase; so["pts"] = sp;
        sa.append(so);
    }
    QJsonObject o;
    o["feather"] = featherPct; o["kind"] = kindName(kind); o["strokes"] = sa;
    return QString::fromUtf8(QJsonDocument(o).toJson(QJsonDocument::Compact));
}

/* Centroid of the ADD coverage (for the on-canvas pin), in normalized coords. */
inline bool centroid(const Parsed &p, double &cx, double &cy)
{
    double sx = 0, sy = 0;
    int n = 0;
    if (!p.strokes.empty()) {
        for (const Stroke &st : p.strokes) {
            if (st.erase) continue;
            for (size_t i = 0; i + 1 < st.pts.size(); i += 2) {
                sx += st.pts[i]; sy += st.pts[i + 1]; ++n;
            }
        }
    } else {
        for (size_t i = 0; i + 1 < p.pts.size(); i += 2) {
            sx += p.pts[i]; sy += p.pts[i + 1]; ++n;
        }
    }
    if (n == 0) return false;
    cx = sx / n; cy = sy / n;
    return true;
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
    Rasterize one stroke's alpha into `field` (row-major w*h) as max(field, alpha).
    radius_px = 0.5*size*maxDim. The coverage is HARD -- exactly the brushed area, with
    only a 1px smoothstep edge for anti-aliasing: feathering for a seamless heal is the
    ENGINE's job (it derives a transition band from the heal size at composite time, see
    lamafill.cpp featherFromHole), not a property of the brush. Per-SEGMENT raster:
    alpha is monotone in distance, so max over segments == the alpha of the min distance
    over the whole polyline, and each segment only touches its own r-padded sub-bbox --
    a long painted stroke stays O(length * radius^2) instead of O(stroke bbox *
    segments). Also returns the stroke's padded clipped bbox (inclusive; empty when
    x1<x0).
*/
/* The image-px rect a stroke can touch (its r-padded polyline bbox), clipped to the
   frame; empty when x1<x0. Sizes a spot-local coverage buffer before rasterizing. */
inline void strokeBounds(const Stroke &st, int w, int h,
                         int &x0, int &y0, int &x1, int &y1)
{
    x0 = w; y0 = h; x1 = -1; y1 = -1;
    const int n = int(st.pts.size() / 2);
    if (n < 1) return;
    const double pad = 0.5 * std::max(st.sizeFrac, 1e-4) * std::max(w, h) + 1.0;
    double ax0 = 1e300, ay0 = 1e300, ax1 = -1e300, ay1 = -1e300;
    for (int i = 0; i < n; ++i) {
        ax0 = std::min(ax0, st.pts[2 * i] * w);  ax1 = std::max(ax1, st.pts[2 * i] * w);
        ay0 = std::min(ay0, st.pts[2 * i + 1] * h); ay1 = std::max(ay1, st.pts[2 * i + 1] * h);
    }
    x0 = std::max(0,     int(std::floor(ax0 - pad)));
    y0 = std::max(0,     int(std::floor(ay0 - pad)));
    x1 = std::min(w - 1, int(std::ceil (ax1 + pad)));
    y1 = std::min(h - 1, int(std::ceil (ay1 + pad)));
}

/*
    `field` covers only the image rect [fx0, fx0+fw) x [fy0, fy0+fh), row-major with
    stride fw; the stroke is clipped to it. The full-frame form passes (0, 0, w, h).
*/
inline void rasterizeStrokeMax(const Stroke &st, int w, int h,
                               std::vector<float> &field,
                               int fx0, int fy0, int fw, int fh,
                               int &sx0, int &sy0, int &sx1, int &sy1)
{
    sx0 = w; sy0 = h; sx1 = -1; sy1 = -1;
    const int n = int(st.pts.size() / 2);
    if (n < 1) return;
    const double maxDim = std::max(w, h);
    const double r    = 0.5 * std::max(st.sizeFrac, 1e-4) * maxDim;
    const double band = 1.0;            // anti-alias edge only; feather is engine-side
    const double pad  = r + band;

    auto segment = [&](double ax, double ay, double bx, double by) {
        const int ix0 = std::max(fx0,          int(std::floor(std::min(ax, bx) - pad)));
        const int iy0 = std::max(fy0,          int(std::floor(std::min(ay, by) - pad)));
        const int ix1 = std::min(fx0 + fw - 1, int(std::ceil (std::max(ax, bx) + pad)));
        const int iy1 = std::min(fy0 + fh - 1, int(std::ceil (std::max(ay, by) + pad)));
        if (ix1 < ix0 || iy1 < iy0) return;
        sx0 = std::min(sx0, ix0); sy0 = std::min(sy0, iy0);
        sx1 = std::max(sx1, ix1); sy1 = std::max(sy1, iy1);
        for (int y = iy0; y <= iy1; ++y) {
            float *row = field.data() + size_t(y - fy0) * fw;
            for (int x = ix0; x <= ix1; ++x) {
                const double d = distSeg(x + 0.5, y + 0.5, ax, ay, bx, by);
                float a;
                if      (d <= r - band) a = 1.0f;
                else if (d >= r)        a = 0.0f;
                else { const double t = (r - d) / band; a = float(t * t * (3.0 - 2.0 * t)); }
                if (a > row[x - fx0]) row[x - fx0] = a;
            }
        }
    };
    if (n == 1)
        segment(st.pts[0] * w, st.pts[1] * h, st.pts[0] * w, st.pts[1] * h);
    else
        for (int i = 1; i < n; ++i)
            segment(st.pts[2 * (i - 1)] * w, st.pts[2 * (i - 1) + 1] * h,
                    st.pts[2 * i] * w,       st.pts[2 * i + 1] * h);
}

/*
    Rasterize the spot to a HARD coverage mask at (w,h) -- exactly the brushed area (the
    stored featherPct is legacy metadata; the heal engines derive their own transition
    band). The single stroke ("pts"), or the painted area ("strokes") composited IN
    ORDER -- add strokes max into the coverage, erase strokes multiply it by
    (1 - alpha), so paint/erase/repaint scopes the way it was painted. Fills cov
    (row-major w*h, 0..1) and the inclusive nonzero bbox [bx0,by0,bx1,by1]; empty when
    bx1<bx0 (e.g. everything painted was erased).
*/
/*
    One spot's coverage, stored only over the rect its strokes can touch -- a spot is a
    few dozen px on a 12+ MP frame, and the old full-frame W*H buffer had to be cleared
    and scanned per spot per render: 817 of 925 ms for a 68-spot image in a Debug build
    (55 of 105 ms optimised). at() reads 0 outside the stored rect, so callers address it
    in frame coordinates exactly as they did the full-frame buffer.
*/
struct Coverage {
    int w = 0, h = 0;                        // frame size the spot was rasterized at
    int x0 = 0, y0 = 0, x1 = -1, y1 = -1;    // stored rect, inclusive, frame px
    int bx0 = 0, by0 = 0, bx1 = -1, by1 = -1; // tight nonzero bbox; empty when bx1<bx0
    std::vector<float> data;                 // (x1-x0+1) x (y1-y0+1), row-major

    bool empty() const { return bx1 < bx0 || by1 < by0; }
    int stride() const { return x1 - x0 + 1; }
    float at(int x, int y) const {
        if (x < x0 || x > x1 || y < y0 || y > y1) return 0.0f;
        return data[size_t(y - y0) * stride() + (x - x0)];
    }
};

/*
    Rasterize the spot to a HARD coverage mask at (w,h) -- exactly the brushed area (the
    stored featherPct is legacy metadata; the heal engines derive their own transition
    band). The single stroke ("pts"), or the painted area ("strokes") composited IN
    ORDER -- add strokes max into the coverage, erase strokes multiply it by
    (1 - alpha), so paint/erase/repaint scopes the way it was painted. Erases only ever
    remove coverage, so the stored rect is the union of the ADD strokes' bounds.
*/
inline void rasterize(const Parsed &p, int w, int h, Coverage &cov)
{
    cov = Coverage();
    cov.w = w; cov.h = h;
    if (!p.valid() || w <= 0 || h <= 0) return;

    /* Normalize to a stroke list: the single-stroke form is one add stroke. */
    std::vector<Stroke> strokes = p.strokes;
    if (strokes.empty()) strokes.push_back({p.sizeFrac, false, p.pts});

    int ux0 = w, uy0 = h, ux1 = -1, uy1 = -1;             // union of ADD stroke bounds
    for (const Stroke &st : strokes) {
        if (st.erase) continue;
        int x0, y0, x1, y1;
        strokeBounds(st, w, h, x0, y0, x1, y1);
        if (x1 < x0 || y1 < y0) continue;
        ux0 = std::min(ux0, x0); uy0 = std::min(uy0, y0);
        ux1 = std::max(ux1, x1); uy1 = std::max(uy1, y1);
    }
    if (ux1 < ux0 || uy1 < uy0) return;
    cov.x0 = ux0; cov.y0 = uy0; cov.x1 = ux1; cov.y1 = uy1;
    const int fw = ux1 - ux0 + 1, fh = uy1 - uy0 + 1;
    cov.data.assign(size_t(fw) * size_t(fh), 0.0f);

    std::vector<float> tmp;                               // erase-stroke alpha (reused)
    for (const Stroke &st : strokes) {
        int sx0, sy0, sx1, sy1;
        if (!st.erase) {
            rasterizeStrokeMax(st, w, h, cov.data, ux0, uy0, fw, fh, sx0, sy0, sx1, sy1);
        } else {
            tmp.assign(size_t(fw) * size_t(fh), 0.0f);
            rasterizeStrokeMax(st, w, h, tmp, ux0, uy0, fw, fh, sx0, sy0, sx1, sy1);
            for (int y = sy0; y <= sy1; ++y) {
                float *c = cov.data.data() + size_t(y - uy0) * fw;
                const float *e = tmp.data() + size_t(y - uy0) * fw;
                for (int x = sx0 - ux0; x <= sx1 - ux0; ++x) c[x] *= 1.0f - e[x];
            }
        }
    }

    /* Tight nonzero bbox. */
    cov.bx0 = w; cov.by0 = h; cov.bx1 = -1; cov.by1 = -1;
    for (int y = uy0; y <= uy1; ++y) {
        const float *c = cov.data.data() + size_t(y - uy0) * fw;
        for (int x = ux0; x <= ux1; ++x) {
            if (c[x - ux0] > 0.0f) {
                cov.bx0 = std::min(cov.bx0, x); cov.by0 = std::min(cov.by0, y);
                cov.bx1 = std::max(cov.bx1, x); cov.by1 = std::max(cov.by1, y);
            }
        }
    }
}

} // namespace FillSpotGeom

#endif // FILLSPOT_H
