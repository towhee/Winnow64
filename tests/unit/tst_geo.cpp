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

QTEST_GUILESS_MAIN(TstGeo)
#include "tst_geo.moc"
