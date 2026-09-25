#ifndef GEO_H
#define GEO_H

#include <QList>
#include <QPointF>
#include <QRectF>
#include <QString>

/*
    GEO: the pure math behind the Map module (Views/Map). No Qt widgets, no network, no
    DataModel, so all of it is unit-tested (tests/unit/tst_geo.cpp).

    COORDINATES. Winnow stores an image's location as the string GPS::decode builds from
    the EXIF GPS IFD -- degrees, minutes, seconds and a hemisphere letter per axis, ie
    49°13'13.477" N 123°57'22.220" W -- and not as numbers (G::GPSCoordColumn, the
    catalog's gpscoord column). parseCoord turns that string, or a plain "lat,lon"
    decimal pair, into signed decimal degrees.

    WEB MERCATOR. The projection every raster tile provider serves (EPSG:3857). WORLD
    space is pixels at a zoom: the whole world is 256 * 2^zoom pixels square, x east from
    the antimeridian and y south from ~85.0511 N. Zoom may be fractional. Latitudes
    beyond +-kMaxLat cannot be projected and are clamped.

    CLUSTERING. Pins that land within a screen cell of each other are drawn as one pin
    with a count, the way Lightroom does. The cells are in world pixels at the current
    zoom, which IS screen space, so a cluster is always about cellPx on screen whatever
    the zoom. O(n) with a hash of cells.
*/

namespace Geo {

constexpr double kMaxLat = 85.05112878;
constexpr int kTileSize = 256;

bool parseCoord(const QString &s, double &lat, double &lon);

double worldSize(double zoom);
QPointF lonLatToWorld(double lat, double lon, double zoom);
void worldToLonLat(const QPointF &world, double zoom, double &lat, double &lon);

/* The tiles covering a world rect at an integer zoom. x may run past either edge of
   the world (the map wraps east-west): wrapTileX folds it back into 0 .. 2^z-1. y is
   clamped to the world, which does not wrap. */
struct TileRange { int x0 = 0, y0 = 0, x1 = -1, y1 = -1; };
TileRange tileRange(const QRectF &worldRect, int zoom);
int wrapTileX(int x, int zoom);

struct Point {
    int id;             // caller's key, ie a proxy row
    QPointF world;      // at the zoom the clusters are built for
};

struct Cluster {
    QPointF world;      // centroid of its members, world pixels
    QList<int> ids;     // members, in input order
};

QList<Cluster> cluster(const QList<Point> &points, double cellPx);

} // namespace Geo

#endif // GEO_H
