#ifndef EDITROTATE_H
#define EDITROTATE_H

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPointF>
#include <QString>

#include "Develop/editstack.h"

/*
    Keep a Develop recipe glued to the photo when the image is ROTATED (Cmd+[ / Cmd+]).

    Everything positional in an EditStack is stored normalized (0..1) in the ORIENTED
    image -- the frame the user sees and paints in, and the input to the geometry stage:
    mask gradients and strokes, Replace spots, the perspective quad, and the crop (in the
    post-warp frame, which rotates with it). A rotation changes that frame, so without
    this every crop, mask and spot lands on different content afterwards -- for raw as
    much as for JPEG, since a raw's masks are mapped from output coords too.

    degrees is the rotation just applied: 90 = clockwise (rotate right), 270 = counter-
    clockwise, 180. Anything else is a no-op.

    WHAT CHANGES, per stored form
      points (gradients, strokes, spots, quad)   rotated
      quad corner ORDER (TL,TR,BR,BL)             relabelled, so TL is the new top-left
      crop rect                                   rotated as a rectangle
      radial rx / ry                              SWAPPED on a quarter turn: rx is a
                                                  fraction of W and ry of H, W and H swap,
                                                  and an ellipse (a,b) turned 90 degrees
                                                  is the ellipse (b,a) at the same angle,
                                                  so the angle never changes
      brush / spot sizes                          unchanged (fraction of the long edge)
      straighten                                  unchanged (a rotation about the centre
                                                  commutes with a quarter turn)
      colour / luminance / depth ranges, AI masks unchanged (not positional)

    Pure and header-only, so tst_editrotate can pin it.
*/
namespace EditRotate {

inline bool isRotation(int degrees)
{
    return degrees == 90 || degrees == 180 || degrees == 270;
}

// One normalized point, in the frame BEFORE the rotation, into the frame after it.
inline QPointF point(double x, double y, int degrees)
{
    switch (degrees) {
    case 90:  return QPointF(1.0 - y, x);           // clockwise
    case 180: return QPointF(1.0 - x, 1.0 - y);
    case 270: return QPointF(y, 1.0 - x);           // counter-clockwise
    default:  return QPointF(x, y);
    }
}

// A flat [x0,y0, x1,y1, ...] array, as brush, object and spot strokes store them.
inline QJsonArray points(const QJsonArray &pts, int degrees)
{
    QJsonArray out;
    for (int i = 0; i + 1 < pts.size(); i += 2) {
        const QPointF p = point(pts.at(i).toDouble(), pts.at(i + 1).toDouble(), degrees);
        out.append(p.x());
        out.append(p.y());
    }
    return out;
}

// Every "pts" in a "strokes" list; everything else in each stroke is kept.
inline QJsonArray strokes(const QJsonArray &list, int degrees)
{
    QJsonArray out;
    for (const QJsonValue &v : list) {
        QJsonObject s = v.toObject();
        if (s.contains("pts")) s["pts"] = points(s.value("pts").toArray(), degrees);
        out.append(s);
    }
    return out;
}

inline QString toJson(const QJsonObject &o)
{
    return QString::fromUtf8(QJsonDocument(o).toJson(QJsonDocument::Compact));
}

/* A mask component's paramsJson. Unknown tools and unparseable blobs come back
   unchanged: there is nothing positional to move, or nothing safe to touch. */
inline QString maskParams(int tool, const QString &json, int degrees)
{
    if (!isRotation(degrees) || json.isEmpty()) return json;
    const QJsonDocument d = QJsonDocument::fromJson(json.toUtf8());
    if (!d.isObject()) return json;
    QJsonObject o = d.object();

    switch (MaskTool(tool)) {
    case MaskTool::LinearGradient: {
        if (!o.contains("x1") || !o.contains("x2")) return json;
        const QPointF a = point(o.value("x1").toDouble(), o.value("y1").toDouble(), degrees);
        const QPointF b = point(o.value("x2").toDouble(), o.value("y2").toDouble(), degrees);
        o["x1"] = a.x(); o["y1"] = a.y();
        o["x2"] = b.x(); o["y2"] = b.y();
        return toJson(o);
    }
    case MaskTool::RadialGradient: {
        if (!o.contains("cx")) return json;
        const QPointF c = point(o.value("cx").toDouble(), o.value("cy").toDouble(), degrees);
        o["cx"] = c.x(); o["cy"] = c.y();
        if (degrees != 180) {
            const QJsonValue rx = o.value("rx");
            o["rx"] = o.value("ry");
            o["ry"] = rx;
        }
        return toJson(o);
    }
    case MaskTool::Brush:
    case MaskTool::Object:
        if (!o.contains("strokes")) return json;
        o["strokes"] = strokes(o.value("strokes").toArray(), degrees);
        return toJson(o);
    default:
        return json;
    }
}

// A Replace spot's paramsJson: the single stroke "pts" and/or the painted "strokes".
inline QString spotParams(const QString &json, int degrees)
{
    if (!isRotation(degrees) || json.isEmpty()) return json;
    const QJsonDocument d = QJsonDocument::fromJson(json.toUtf8());
    if (!d.isObject()) return json;
    QJsonObject o = d.object();
    if (o.contains("pts")) o["pts"] = points(o.value("pts").toArray(), degrees);
    if (o.contains("strokes")) o["strokes"] = strokes(o.value("strokes").toArray(), degrees);
    return toJson(o);
}

inline void geometry(Geometry &g, int degrees)
{
    if (!isRotation(degrees)) return;

    // crop: the rectangle's corners move; its extent follows the frame
    const double x = g.cropX, y = g.cropY, w = g.cropW, h = g.cropH;
    switch (degrees) {
    case 90:  g.cropX = 1.0 - (y + h); g.cropY = x;             g.cropW = h; g.cropH = w; break;
    case 180: g.cropX = 1.0 - (x + w); g.cropY = 1.0 - (y + h);                         break;
    case 270: g.cropX = y;             g.cropY = 1.0 - (x + w); g.cropW = h; g.cropH = w; break;
    }

    /* quad: rotate each corner, then relabel. A clockwise turn carries the old BOTTOM-
       left corner to the top-left, so new[i] = old[(i + 3) % 4]; counter-clockwise
       brings the old top-RIGHT there, (i + 1) % 4; a half turn swaps opposite corners. */
    const int shift = degrees == 90 ? 3 : degrees == 180 ? 2 : 1;
    double q[8];
    for (int i = 0; i < 4; ++i) {
        const int from = (i + shift) % 4;
        const QPointF p = point(g.quad[from * 2], g.quad[from * 2 + 1], degrees);
        q[i * 2] = p.x();
        q[i * 2 + 1] = p.y();
    }
    for (int i = 0; i < 8; ++i) g.quad[i] = q[i];
}

/* The whole recipe. Returns whether anything positional was there to move, so the
   caller can skip a sidecar write for a recipe that is only tone and colour. */
inline bool stack(EditStack &s, int degrees)
{
    if (!isRotation(degrees)) return false;
    bool moved = false;
    for (EditScope &scope : s.scopes) {
        for (MaskComponent &m : scope.components) {
            const QString after = maskParams(m.tool, m.paramsJson, degrees);
            if (after != m.paramsJson) { m.paramsJson = after; moved = true; }
        }
    }
    for (FillSpot &spot : s.spots) {
        const QString after = spotParams(spot.paramsJson, degrees);
        if (after != spot.paramsJson) { spot.paramsJson = after; moved = true; }
    }
    /* Always turned, even when inactive: the default crop and quad map onto themselves,
       and a quad kept while its warp is switched off must still match the photo when
       it is switched back on. */
    const Geometry before = s.geometry;
    geometry(s.geometry, degrees);
    moved |= before.cropX != s.geometry.cropX || before.cropY != s.geometry.cropY
             || before.cropW != s.geometry.cropW || before.cropH != s.geometry.cropH;
    for (int i = 0; i < 8; ++i) moved |= before.quad[i] != s.geometry.quad[i];
    return moved;
}

}  // namespace EditRotate

#endif // EDITROTATE_H
