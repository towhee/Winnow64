#include "Utilities/geo.h"

#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <algorithm>
#include <cmath>
#include <numbers>

namespace Geo {

using std::numbers::pi;     // not M_PI: MSVC needs _USE_MATH_DEFINES for that

bool parseCoord(const QString &s, double &lat, double &lon)
{
/*
    Signed decimal degrees from either form Winnow holds a location in:

      49°13'13.477" N 123°57'22.220" W     GPS::decode (every image format)
      49.28,-123.12                        a decimal pair (catalog tests, hand entry)

    The DMS form is matched one axis at a time -- a number with optional minutes and
    seconds, then a hemisphere letter -- so the degree/minute/second marks are optional
    and either order of the two axes works. S and W negate.

    Rejected, returning false: empty, GPS::decode's "Error", anything out of range, and
    exactly 0,0. A camera without a fix that still writes the GPS IFD writes zeros, and
    a pin in the Gulf of Guinea for every such image is noise, not a location.
*/
    lat = lon = 0;
    const QString t = s.trimmed();
    if (t.isEmpty()) return false;

    static const QRegularExpression decimalRe(
        R"(^\s*([-+]?\d+(?:\.\d+)?)\s*[,;\s]\s*([-+]?\d+(?:\.\d+)?)\s*$)");
    static const QRegularExpression axisRe(
        R"((\d+(?:\.\d+)?)\s*°?\s*(?:(\d+(?:\.\d+)?)\s*['′])?\s*)"
        R"((?:(\d+(?:\.\d+)?)\s*["″])?\s*([NSEWnsew])\b)");

    bool haveLat = false, haveLon = false;
    QRegularExpressionMatch dm = decimalRe.match(t);
    if (dm.hasMatch()) {
        lat = dm.captured(1).toDouble();
        lon = dm.captured(2).toDouble();
        haveLat = haveLon = true;
    }
    else {
        auto it = axisRe.globalMatch(t);
        while (it.hasNext()) {
            const QRegularExpressionMatch m = it.next();
            double v = m.captured(1).toDouble();
            if (!m.captured(2).isEmpty()) v += m.captured(2).toDouble() / 60.0;
            if (!m.captured(3).isEmpty()) v += m.captured(3).toDouble() / 3600.0;
            const QChar h = m.captured(4).at(0).toUpper();
            if (h == 'S' || h == 'W') v = -v;
            if (h == 'N' || h == 'S') {
                if (haveLat) return false;
                lat = v;
                haveLat = true;
            }
            else {
                if (haveLon) return false;
                lon = v;
                haveLon = true;
            }
        }
    }

    if (!haveLat || !haveLon) return false;
    if (!std::isfinite(lat) || !std::isfinite(lon)) return false;
    if (lat < -90 || lat > 90 || lon < -180 || lon > 180) return false;
    if (lat == 0 && lon == 0) return false;
    return true;
}

QString formatCoord(double lat, double lon)
{
    auto axis = [](double v, QChar pos, QChar neg) {
        const QChar h = v < 0 ? neg : pos;
        // in thousandths of a second, so rounding can never print 60.000"
        const qint64 ms = std::llround(std::abs(v) * 3600.0 * 1000.0);
        const qint64 d = ms / 3600000;
        const qint64 m = (ms / 60000) % 60;
        const double s = (ms % 60000) / 1000.0;
        return QString("%1°%2'%3\" %4").arg(d).arg(m).arg(s, 0, 'f', 3).arg(h);
    };
    return axis(lat, 'N', 'S') + " " + axis(lon, 'E', 'W');
}

QString toXmpCoord(double deg, bool isLat)
{
    const QChar h = isLat ? (deg < 0 ? 'S' : 'N') : (deg < 0 ? 'W' : 'E');
    const qint64 micro = std::llround(std::abs(deg) * 60.0 * 1e6);    // micro-minutes
    const qint64 d = micro / 60000000;
    const double m = (micro % 60000000) / 1e6;
    return QString("%1,%2%3").arg(d).arg(m, 0, 'f', 6).arg(h);
}

bool fromXmpCoord(const QString &s, bool isLat, double &deg)
{
    deg = 0;
    QString t = s.trimmed().toUpper();
    if (t.isEmpty()) return false;
    const QChar h = t.back();
    const bool hasRef = h == 'N' || h == 'S' || h == 'E' || h == 'W';
    if (hasRef) {
        if (isLat != (h == 'N' || h == 'S')) return false;
        t.chop(1);
    }
    const QStringList parts = t.split(',');
    if (parts.isEmpty() || parts.size() > 3) return false;
    bool ok = true;
    double v = 0;
    const double scale[3] = {1.0, 60.0, 3600.0};
    for (int i = 0; i < parts.size() && ok; ++i)
        v += parts.at(i).trimmed().toDouble(&ok) / scale[i];
    if (!ok || !std::isfinite(v)) return false;
    if (h == 'S' || h == 'W') v = -v;
    if (std::abs(v) > (isLat ? 90.0 : 180.0)) return false;
    deg = v;
    return true;
}

double worldSize(double zoom)
{
    return kTileSize * std::pow(2.0, zoom);
}

QPointF lonLatToWorld(double lat, double lon, double zoom)
{
    lat = std::clamp(lat, -kMaxLat, kMaxLat);
    const double size = worldSize(zoom);
    const double x = (lon + 180.0) / 360.0 * size;
    const double r = lat * pi / 180.0;
    const double y = (1.0 - std::log(std::tan(r) + 1.0 / std::cos(r)) / pi) / 2.0 * size;
    return QPointF(x, y);
}

void worldToLonLat(const QPointF &world, double zoom, double &lat, double &lon)
{
    const double size = worldSize(zoom);
    lon = world.x() / size * 360.0 - 180.0;
    const double n = pi - 2.0 * pi * world.y() / size;
    lat = 180.0 / pi * std::atan(std::sinh(n));
}

TileRange tileRange(const QRectF &worldRect, int zoom)
{
    TileRange r;
    const int n = 1 << zoom;
    r.x0 = static_cast<int>(std::floor(worldRect.left() / kTileSize));
    r.x1 = static_cast<int>(std::floor((worldRect.right() - 1e-9) / kTileSize));
    r.y0 = std::max(0, static_cast<int>(std::floor(worldRect.top() / kTileSize)));
    r.y1 = std::min(n - 1,
                    static_cast<int>(std::floor((worldRect.bottom() - 1e-9) / kTileSize)));
    return r;
}

int wrapTileX(int x, int zoom)
{
    const int n = 1 << zoom;
    return ((x % n) + n) % n;
}

QList<Cluster> cluster(const QList<Point> &points, double cellPx)
{
    QList<Cluster> out;
    if (cellPx <= 0) cellPx = 1;
    QHash<quint64, int> cellIndex;          // cell -> index in out
    QList<QPointF> sums;                    // running sum of member positions
    for (const Point &p : points) {
        const qint64 cx = static_cast<qint64>(std::floor(p.world.x() / cellPx));
        const qint64 cy = static_cast<qint64>(std::floor(p.world.y() / cellPx));
        const quint64 key = (static_cast<quint64>(cx) << 32) ^ static_cast<quint32>(cy);
        auto it = cellIndex.constFind(key);
        if (it == cellIndex.constEnd()) {
            cellIndex.insert(key, out.size());
            out.append(Cluster{p.world, {p.id}});
            sums.append(p.world);
        }
        else {
            Cluster &c = out[*it];
            c.ids.append(p.id);
            sums[*it] += p.world;
            c.world = sums.at(*it) / c.ids.size();
        }
    }
    return out;
}

/* ---------------------------------------------------------------------------------
   Places
   --------------------------------------------------------------------------------- */

static double unwrapLon(double lon, double anchor)
{
    while (lon - anchor > 180.0) lon -= 360.0;
    while (lon - anchor < -180.0) lon += 360.0;
    return lon;
}

static double anchorLon(const Place &p)
{
    if (p.shape == Place::Ellipse) return p.lon;
    return p.verts.isEmpty() ? 0.0 : p.verts.first().x();
}

/* lonLatToWorld without the wrap: x is linear in lon, so an unwrapped longitude past
   +-180 lands past the edge of the world, which is what keeps a seam-crossing place
   whole. */
static QPointF project0(double lat, double lon)
{
    return lonLatToWorld(lat, lon, 0);
}

bool Place::isValid() const
{
    if (shape == Ellipse)
        return rx > 0 && ry > 0 && std::isfinite(lat) && std::isfinite(lon);
    return verts.size() >= 3;
}

QPolygonF outlineWorld0(const Place &p, int segments)
{
    QPolygonF poly;
    const double a = anchorLon(p);
    if (p.shape == Place::Ellipse) {
        const QPointF c = project0(p.lat, p.lon);
        const double t = p.angleDeg * pi / 180.0;
        const double ct = std::cos(t), st = std::sin(t);
        segments = std::max(8, segments);
        for (int i = 0; i < segments; ++i) {
            const double u = 2.0 * pi * i / segments;
            const double lx = p.rx * std::cos(u), ly = p.ry * std::sin(u);
            poly << QPointF(c.x() + lx * ct - ly * st, c.y() + lx * st + ly * ct);
        }
        return poly;
    }
    for (const QPointF &v : p.verts) poly << project0(v.y(), unwrapLon(v.x(), a));
    return poly;
}

bool contains(const Place &p, double lat, double lon)
{
    if (!p.isValid()) return false;
    const QPointF w = project0(lat, unwrapLon(lon, anchorLon(p)));
    if (p.shape == Place::Ellipse) {
        const QPointF c = project0(p.lat, p.lon);
        const double dx = w.x() - c.x(), dy = w.y() - c.y();
        const double t = p.angleDeg * pi / 180.0;
        const double lx = dx * std::cos(t) + dy * std::sin(t);
        const double ly = -dx * std::sin(t) + dy * std::cos(t);
        const double ex = lx / p.rx, ey = ly / p.ry;
        return ex * ex + ey * ey <= 1.0;
    }
    // even-odd ray cast, the rule QPainterPath fills with
    const QPolygonF poly = outlineWorld0(p);
    bool in = false;
    for (int i = 0, j = poly.size() - 1; i < poly.size(); j = i++) {
        const QPointF &a = poly.at(i), &b = poly.at(j);
        if ((a.y() > w.y()) != (b.y() > w.y())) {
            const double x =
                a.x() + (w.y() - a.y()) * (b.x() - a.x()) / (b.y() - a.y());
            if (w.x() < x) in = !in;
        }
    }
    return in;
}

QString toJson(const Place &p)
{
    QJsonObject o;
    o["v"] = 1;
    if (p.shape == Place::Ellipse) {
        o["shape"] = "ellipse";
        o["lat"] = p.lat;
        o["lon"] = p.lon;
        o["rx"] = p.rx;
        o["ry"] = p.ry;
        o["angle"] = p.angleDeg;
    }
    else {
        o["shape"] = "polygon";
        QJsonArray pts;
        for (const QPointF &v : p.verts)
            pts.append(QJsonArray{v.y(), v.x()});           // [lat, lon]
        o["points"] = pts;
    }
    return QString::fromUtf8(QJsonDocument(o).toJson(QJsonDocument::Compact));
}

bool fromJson(const QString &json, Place &p)
{
    p = Place();
    const QJsonObject o = QJsonDocument::fromJson(json.toUtf8()).object();
    const QString shape = o.value("shape").toString();
    if (shape == "ellipse") {
        p.shape = Place::Ellipse;
        p.lat = o.value("lat").toDouble();
        p.lon = o.value("lon").toDouble();
        p.rx = o.value("rx").toDouble();
        p.ry = o.value("ry").toDouble();
        p.angleDeg = o.value("angle").toDouble();
    }
    else if (shape == "polygon") {
        p.shape = Place::Polygon;
        for (const QJsonValue &v : o.value("points").toArray()) {
            const QJsonArray a = v.toArray();
            if (a.size() >= 2) p.verts << QPointF(a.at(1).toDouble(), a.at(0).toDouble());
        }
    }
    else return false;
    return p.isValid();
}

} // namespace Geo
