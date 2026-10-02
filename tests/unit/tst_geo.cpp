// Unit tests for Utilities/geo.h -- the Map module's coordinate parsing, Web Mercator
// projection, tile ranges and pin clustering.

#include <QtTest>
#include "Utilities/geo.h"

class TstGeo : public QObject
{
    Q_OBJECT
private slots:
    void parseDms_data();
    void parseDms();
    void parseRejects_data();
    void parseRejects();
    void mercatorRoundTrip();
    void mercatorKnownPoints();
    void tileRangeWraps();
    void clusterCounts();
    void placeEllipse();
    void placeRotatedEllipse();
    void placeConcavePolygon();
    void placeAntimeridianPolygon();
    void placeJsonRoundTrip();
    void formatCoordRoundTrip();
    void xmpCoordRoundTrip();
};

void TstGeo::parseDms_data()
{
    QTest::addColumn<QString>("s");
    QTest::addColumn<double>("lat");
    QTest::addColumn<double>("lon");
    // The exact shape GPS::decode builds: deg°min'sec" H deg°min'sec" H
    QTest::newRow("NW") << QString("49°13'13.477\" N 123°57'22.220\" W")
                        << 49.2204103 << -123.9561722;
    QTest::newRow("SE") << QString("33°51'54.000\" S 151°12'36.000\" E")
                        << -33.865 << 151.21;
    QTest::newRow("zero minutes") << QString("10°0'30.000\" N 20°0'0.000\" E")
                                  << 10.0083333 << 20.0;
    QTest::newRow("decimal") << QString("49.28,-123.12") << 49.28 << -123.12;
    QTest::newRow("decimal space") << QString(" -33.865, 151.21 ") << -33.865 << 151.21;
    QTest::newRow("lon first") << QString("123°57'22.220\" W 49°13'13.477\" N")
                               << 49.2204103 << -123.9561722;
}

void TstGeo::parseDms()
{
    QFETCH(QString, s);
    QFETCH(double, lat);
    QFETCH(double, lon);
    double a = 0, b = 0;
    QVERIFY(Geo::parseCoord(s, a, b));
    QVERIFY(qAbs(a - lat) < 1e-6);
    QVERIFY(qAbs(b - lon) < 1e-6);
}

void TstGeo::parseRejects_data()
{
    QTest::addColumn<QString>("s");
    QTest::newRow("empty") << QString();
    QTest::newRow("error") << QString("Error");
    QTest::newRow("null island") << QString("0°0'0.000\" N 0°0'0.000\" E");
    QTest::newRow("lat range") << QString("95.0,10.0");
    QTest::newRow("lon range") << QString("10.0,190.0");
    QTest::newRow("one axis") << QString("49°13'13.477\" N");
    QTest::newRow("two lats") << QString("49°13'13\" N 12°0'0\" S");
}

void TstGeo::parseRejects()
{
    QFETCH(QString, s);
    double a = 1, b = 1;
    QVERIFY(!Geo::parseCoord(s, a, b));
}

void TstGeo::mercatorRoundTrip()
{
    const double pts[][2] = {{49.28, -123.12}, {-33.865, 151.21}, {0.5, 0.5},
                             {84.0, 179.9}, {-84.0, -179.9}};
    for (double z : {0.0, 3.0, 12.5, 18.0}) {
        for (const auto &p : pts) {
            const QPointF w = Geo::lonLatToWorld(p[0], p[1], z);
            double lat = 0, lon = 0;
            Geo::worldToLonLat(w, z, lat, lon);
            QVERIFY2(qAbs(lat - p[0]) < 1e-9 && qAbs(lon - p[1]) < 1e-9,
                     qPrintable(QString("z=%1 %2,%3").arg(z).arg(p[0]).arg(p[1])));
        }
    }
}

void TstGeo::mercatorKnownPoints()
{
    // The world at zoom 0 is one 256px tile; 0,0 is its centre.
    QPointF c = Geo::lonLatToWorld(0, 0, 0);
    QVERIFY(qAbs(c.x() - 128) < 1e-9 && qAbs(c.y() - 128) < 1e-9);
    // The latitude limit maps to the top edge; beyond it is clamped there.
    QVERIFY(qAbs(Geo::lonLatToWorld(Geo::kMaxLat, 0, 0).y()) < 1e-6);
    QVERIFY(qAbs(Geo::lonLatToWorld(89.9, 0, 0).y()) < 1e-6);
    QCOMPARE(Geo::worldSize(2), 1024.0);
}

void TstGeo::tileRangeWraps()
{
    // Zoom 2: a 4x4 tile world, 1024px. A rect straddling the antimeridian.
    Geo::TileRange r = Geo::tileRange(QRectF(-100, 300, 300, 200), 2);
    QCOMPARE(r.x0, -1);
    QCOMPARE(r.x1, 0);
    QCOMPARE(r.y0, 1);
    QCOMPARE(r.y1, 1);
    QCOMPARE(Geo::wrapTileX(-1, 2), 3);
    QCOMPARE(Geo::wrapTileX(4, 2), 0);
    QCOMPARE(Geo::wrapTileX(9, 2), 1);
    // y is clamped to the world, which does not wrap vertically.
    r = Geo::tileRange(QRectF(0, -500, 10, 2000), 2);
    QCOMPARE(r.y0, 0);
    QCOMPARE(r.y1, 3);
}

void TstGeo::clusterCounts()
{
    QList<Geo::Point> pts{
        {0, QPointF(10, 10)}, {1, QPointF(20, 30)}, {2, QPointF(59, 59)},   // one cell
        {3, QPointF(61, 10)},                                              // next cell
        {4, QPointF(500, 500)}, {5, QPointF(510, 505)},                    // far cell
    };
    const QList<Geo::Cluster> cs = Geo::cluster(pts, 60);
    QCOMPARE(cs.size(), 3);
    QCOMPARE(cs.at(0).ids, QList<int>({0, 1, 2}));
    QCOMPARE(cs.at(1).ids, QList<int>({3}));
    QCOMPARE(cs.at(2).ids, QList<int>({4, 5}));
    QVERIFY(qAbs(cs.at(0).world.x() - 29.6666667) < 1e-6);
    QVERIFY(qAbs(cs.at(2).world.y() - 502.5) < 1e-9);
    QVERIFY(Geo::cluster({}, 60).isEmpty());
}

void TstGeo::placeEllipse()
{
    // A circle of 1 zoom-0 pixel (~1.4 degrees of longitude) around Vancouver.
    Geo::Place p;
    p.lat = 49.28;
    p.lon = -123.12;
    p.rx = p.ry = 1.0;
    QVERIFY(p.isValid());
    QVERIFY(Geo::contains(p, 49.28, -123.12));
    QVERIFY(Geo::contains(p, 49.28, -122.0));      // 1.12 deg east: 0.8 px
    QVERIFY(!Geo::contains(p, 49.28, -121.5));     // 1.62 deg east: 1.15 px
    QVERIFY(!Geo::contains(p, -33.865, 151.21));
    p.rx = 0;
    QVERIFY(!p.isValid());
    QVERIFY(!Geo::contains(p, 49.28, -123.12));
}

void TstGeo::placeRotatedEllipse()
{
    // Long east-west (rx 4), thin (ry 0.5), on the equator: a point 3 px east is in.
    Geo::Place p;
    p.lat = 0.1;
    p.lon = 10;
    p.rx = 4;
    p.ry = 0.5;
    const double degPerPx = 360.0 / 256.0;
    QVERIFY(Geo::contains(p, 0.1, 10 + 3 * degPerPx));
    // Turned 90 degrees, the same point is outside and one 3 px "south" is in.
    p.angleDeg = 90;
    QVERIFY(!Geo::contains(p, 0.1, 10 + 3 * degPerPx));
    double lat = 0, lon = 0;
    const QPointF c = Geo::lonLatToWorld(0.1, 10, 0);
    Geo::worldToLonLat(c + QPointF(0, 3), 0, lat, lon);
    QVERIFY(Geo::contains(p, lat, lon));
}

void TstGeo::placeConcavePolygon()
{
    // A "U": the notch at the top middle is outside.
    Geo::Place p;
    p.shape = Geo::Place::Polygon;
    p.verts = {{0, 10}, {3, 10}, {3, 2}, {2, 2}, {2, 8}, {1, 8}, {1, 2}, {0, 2}};
    QVERIFY(p.isValid());
    QVERIFY(Geo::contains(p, 5, 0.5));            // left arm
    QVERIFY(Geo::contains(p, 5, 2.5));            // right arm
    QVERIFY(Geo::contains(p, 9, 1.5));            // the base
    QVERIFY(!Geo::contains(p, 5, 1.5));           // the notch
    QVERIFY(!Geo::contains(p, 20, 1.5));
    p.verts.resize(2);
    QVERIFY(!p.isValid());
}

void TstGeo::placeAntimeridianPolygon()
{
    // A square straddling 180: from 170 E to 170 W.
    Geo::Place p;
    p.shape = Geo::Place::Polygon;
    p.verts = {{170, 10}, {-170, 10}, {-170, -10}, {170, -10}};
    QVERIFY(Geo::contains(p, 0.5, 179.0));
    QVERIFY(Geo::contains(p, 0.5, -179.0));
    QVERIFY(Geo::contains(p, 0.5, 180.0));
    QVERIFY(!Geo::contains(p, 0.5, 0.0));
    QVERIFY(!Geo::contains(p, 0.5, 160.0));
    QVERIFY(!Geo::contains(p, 0.5, -160.0));
    // An ellipse centred on the seam works the same way.
    Geo::Place e;
    e.lat = 0.5;
    e.lon = 179.5;
    e.rx = e.ry = 2;
    QVERIFY(Geo::contains(e, 0.5, -179.0));
    QVERIFY(!Geo::contains(e, 0.5, 0.0));
}

void TstGeo::placeJsonRoundTrip()
{
    Geo::Place e;
    e.lat = 49.28;
    e.lon = -123.12;
    e.rx = 0.3;
    e.ry = 0.125;
    e.angleDeg = 33;
    Geo::Place back;
    QVERIFY(Geo::fromJson(Geo::toJson(e), back));
    QCOMPARE(back.shape, Geo::Place::Ellipse);
    QCOMPARE(back.lat, e.lat);
    QCOMPARE(back.lon, e.lon);
    QCOMPARE(back.rx, e.rx);
    QCOMPARE(back.ry, e.ry);
    QCOMPARE(back.angleDeg, e.angleDeg);

    Geo::Place p;
    p.shape = Geo::Place::Polygon;
    p.verts = {{-123, 49}, {-122, 49}, {-122.5, 50}};
    QVERIFY(Geo::fromJson(Geo::toJson(p), back));
    QCOMPARE(back.shape, Geo::Place::Polygon);
    QCOMPARE(back.verts, p.verts);
    // The published shape of the JSON: [lat, lon] pairs.
    QVERIFY(Geo::toJson(p).contains("[49,-123]"));

    QVERIFY(!Geo::fromJson("", back));
    QVERIFY(!Geo::fromJson("{\"shape\":\"star\"}", back));
}

void TstGeo::formatCoordRoundTrip()
{
    // The exact shape GPS::decode builds, and back through parseCoord
    QCOMPARE(Geo::formatCoord(49.2204103, -123.9561722),
             QString("49°13'13.477\" N 123°57'22.220\" W"));
    QCOMPARE(Geo::formatCoord(-33.865, 151.21),
             QString("33°51'54.000\" S 151°12'36.000\" E"));
    // a second that rounds up carries into the minute, never 60.000"
    QCOMPARE(Geo::formatCoord(10.0166666, 20.0), QString("10°1'0.000\" N 20°0'0.000\" E"));
    const double pts[][2] = {{49.28, -123.12}, {-0.5, 0.25}, {84.9, 179.99}};
    for (const auto &p : pts) {
        double la = 0, lo = 0;
        QVERIFY(Geo::parseCoord(Geo::formatCoord(p[0], p[1]), la, lo));
        QVERIFY(qAbs(la - p[0]) < 1e-6 && qAbs(lo - p[1]) < 1e-6);
    }
}

void TstGeo::xmpCoordRoundTrip()
{
    QCOMPARE(Geo::toXmpCoord(49.2204103, true), QString("49,13.224618N"));
    QCOMPARE(Geo::toXmpCoord(-123.9561722, false), QString("123,57.370332W"));
    double v = 0;
    QVERIFY(Geo::fromXmpCoord("49,13.224618N", true, v));
    QVERIFY(qAbs(v - 49.2204103) < 1e-6);
    QVERIFY(Geo::fromXmpCoord("123,57,22.22W", false, v));      // DDD,MM,SS form
    QVERIFY(qAbs(v + 123.9561722) < 1e-6);
    QVERIFY(Geo::fromXmpCoord("33,51.9S", true, v));
    QVERIFY(qAbs(v + 33.865) < 1e-9);
    QVERIFY(!Geo::fromXmpCoord("", true, v));
    QVERIFY(!Geo::fromXmpCoord("49,13N", false, v));            // a latitude as longitude
    QVERIFY(!Geo::fromXmpCoord("95,0N", true, v));
    QVERIFY(!Geo::fromXmpCoord("abc", true, v));
}

QTEST_GUILESS_MAIN(TstGeo)
#include "tst_geo.moc"
