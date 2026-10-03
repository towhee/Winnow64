#ifndef GEO_H
#define GEO_H

#include <QList>
#include <QPolygonF>
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
/* The other way: decimal degrees as the string GPS::decode builds, so a location set in
   Winnow reads exactly like one from a camera -- 49°13'13.477" N 123°57'22.220" W. */
QString formatCoord(double lat, double lon);
/* XMP's GPSCoordinate (exif:GPSLatitude / exif:GPSLongitude in a sidecar):
   "DDD,MM.mmmmmmK" with K = N/S or E/W. fromXmpCoord also reads "DDD,MM,SSK". */
QString toXmpCoord(double deg, bool isLat);
bool fromXmpCoord(const QString &s, bool isLat, double &deg);

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

/*  PLACES: a user-drawn area on the map (the Places panel, Main/mwplaces.cpp), an ellipse
    or a polygon, that filters to the images inside it.

    WHAT YOU SEE IS WHAT FILTERS. The map draws in Web Mercator, so containment is tested
    there too, in world pixels at zoom 0 (the world is kTileSize square): an ellipse is a
    true ellipse ON THE MAP and a polygon's edges are the straight lines drawn between its
    corners. An ellipse's semi-axes are therefore in zoom-0 world pixels, not metres.

    THE ANTIMERIDIAN. A place may straddle 180 degrees. Every longitude -- the corners and
    the point tested -- is unwrapped to within 180 degrees of the place's anchor (its
    centre, or its first corner) before it is projected, so a place drawn across the
    seam is one shape, not two.

    THE JSON (toJson / fromJson) IS A PUBLISHED FORMAT, stored in userdata.db as the
    place's node definition: keys may be added, never renamed or rescaled. */
struct Place {
    enum Shape { Ellipse = 0, Polygon = 1 };
    Shape shape = Ellipse;
    double lat = 0, lon = 0;    // ellipse centre, decimal degrees
    double rx = 0, ry = 0;      // ellipse semi-axes, zoom-0 world pixels
    double angleDeg = 0;        // ellipse rotation, clockwise on screen
    QList<QPointF> verts;       // polygon corners, x = lon, y = lat

    bool isValid() const;
};

bool contains(const Place &place, double lat, double lon);
/* The place's outline as zoom-0 world points (an ellipse as `segments` points), its
   longitudes unwrapped about the anchor, so x may run past either edge of the world. */
QPolygonF outlineWorld0(const Place &place, int segments = 64);
QString toJson(const Place &place);
bool fromJson(const QString &json, Place &place);

} // namespace Geo

#endif // GEO_H
