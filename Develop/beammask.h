#ifndef BEAMMASK_H
#define BEAMMASK_H

#include <QJsonObject>
#include <QPointF>
#include <algorithm>
#include <cmath>
#include <limits>

/*
    Beam Gradient mask (MaskTool::Beam): a band along a straight SPINE, fully covered on
    the spine and falling off symmetrically on both sides with the same feather. It runs
    across the whole frame, like the Linear gradient, and may TAPER: its width changes
    linearly along the spine, so the two edges are straight lines that converge toward
    one end and can meet in a point.

    paramsJson (forward-tolerant; every key has a default):
        x1,y1,x2,y2  the spine's two end handles, mask-normalized (0..1 of the frame), the
                     same form and rotation rule as the Linear gradient's p1/p2
        width        HALF-width at the spine's centre, as a fraction of the frame's SHORT
                     side -- invariant under a quarter turn, so Cmd+[ / Cmd+] need only
                     move the endpoints (Develop/editrotate.h)
        taper        -100..100. Positive narrows the beam toward (x2,y2), negative toward
                     (x1,y1); +-100 brings the edges together exactly at that handle.

    The falloff is the Radial gradient's: t = (distance from the spine) / (local half
    width), 0 on the spine and 1 at the drawn edge, fed to MaskFalloff (a feather of 0 is
    a hard step at t = 1). Header-only and shared by the render (MW parseMaskComp /
    evalMaskComp) and the loupe overlay (ImageView), so the preview is the render.
*/

namespace BeamMask {

constexpr double kDefaultWidth = 0.12;

struct Shape {
    bool   valid = false;
    double cx = 0, cy = 0;          // spine centre, frame px
    double ux = 0, uy = 1;          // unit spine direction (p1 -> p2), frame px
    double hw = 0;                  // half-width at the centre, frame px (Edge included)
    double slope = 0;               // d(half-width)/d(s), s = px along the spine from c
    double edgePx = 0;              // Edge: grows/shrinks the half-width everywhere

    /* Half-width at axial position s, or <= 0 past the point where the edges meet. */
    double halfWidthAt(double s) const
    {
        const double base = hw + slope * s;
        if (base <= 1e-9) return 0.0;               // at/past the apex: no beam
        return std::max(base + edgePx, 1e-3);
    }

    /* The falloff coordinate at a frame-px point: 0 on the spine, 1 at the edge,
       +inf where the beam has narrowed to nothing. */
    double t(double px, double py) const
    {
        const double dx = px - cx, dy = py - cy;
        const double s = dx * ux + dy * uy;         // along the spine
        const double d = std::abs(-dx * uy + dy * ux);   // across it
        const double w = halfWidthAt(s);
        if (w <= 0.0) return std::numeric_limits<double>::infinity();
        return d / w;
    }
};

/* p1, p2 normalized; W, H the frame in px (any resolution: the shape scales with it). */
inline Shape make(QPointF p1, QPointF p2, double width, double taper,
                  double W, double H, double edgePx = 0.0)
{
    Shape b;
    if (W <= 0.0 || H <= 0.0) return b;
    const double x1 = p1.x() * W, y1 = p1.y() * H;
    const double x2 = p2.x() * W, y2 = p2.y() * H;
    const double dx = x2 - x1, dy = y2 - y1;
    const double len = std::hypot(dx, dy);
    if (len <= 1e-9) return b;
    b.cx = (x1 + x2) / 2.0;
    b.cy = (y1 + y2) / 2.0;
    b.ux = dx / len;
    b.uy = dy / len;
    const double k = std::clamp(taper, -100.0, 100.0) / 100.0;
    b.hw = std::max(0.0, width) * std::min(W, H);
    b.slope = -k * b.hw / (len / 2.0);              // k = 1: zero at p2 (s = +len/2)
    b.edgePx = edgePx;
    b.valid = b.hw > 0.0;
    return b;
}

inline Shape fromJson(const QJsonObject &o, double W, double H, double edgePx = 0.0)
{
    if (!o.contains("x1") || !o.contains("y1") || !o.contains("x2") || !o.contains("y2"))
        return Shape();
    return make(QPointF(o["x1"].toDouble(), o["y1"].toDouble()),
                QPointF(o["x2"].toDouble(), o["y2"].toDouble()),
                o.value("width").toDouble(kDefaultWidth),
                o.value("taper").toDouble(0.0), W, H, edgePx);
}

} // namespace BeamMask

#endif // BEAMMASK_H
