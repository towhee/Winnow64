#include "Utilities/geo.h"

#include <QHash>
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

} // namespace Geo
