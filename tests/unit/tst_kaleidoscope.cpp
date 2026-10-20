#include <QtTest>

#include "Develop/Transform/croptransform.h"
#include "Develop/editrotate.h"

/*
    The Transform panel's kaleidoscope (CropTransform::applyKaleidoscope), the fold that
    runs after the crop. Pins the properties the look depends on: the first wedge is the
    photo untouched, the next is its mirror image (so every seam matches), the point form
    agrees with the pixels, and the stored settings round-trip, sanitize and rotate with
    the photo.
*/

namespace {

/* Every pixel distinct enough that a wrong sample shows up. */
QImage pattern(int w, int h)
{
    QImage img(w, h, QImage::Format_ARGB32);
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x)
            img.setPixel(x, y, qRgb((x * 4) & 255, (y * 5) & 255, (x * y) & 255));
    return img;
}

/* No wrap-around anywhere, so a bilinear and a nearest sample of the same spot agree to
   within one pixel's step of the gradient. */
QImage smooth(int w, int h)
{
    QImage img(w, h, QImage::Format_ARGB32);
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x)
            img.setPixel(x, y, qRgb(x * 2, y * 3, 128));
    return img;
}

bool near(QRgb a, QRgb b, int tol = 1)
{
    return qAbs(qRed(a) - qRed(b)) <= tol && qAbs(qGreen(a) - qGreen(b)) <= tol &&
           qAbs(qBlue(a) - qBlue(b)) <= tol;
}

Geometry kaleido(int segments, double angle = 0.0, double source = 0.0)
{
    Geometry g;
    g.kaleidoSegments = segments;
    g.kaleidoAngle = angle;
    g.kaleidoSource = source;
    return g;
}

}  // namespace

class TestKaleidoscope : public QObject
{
    Q_OBJECT
private slots:
    void offIsIdentity();
    void twoSegmentsMirrorAcrossTheAngle();
    void firstWedgeIsThePhoto();
    void pointFormMatchesPixels();
    void keepsSizeAndDepth();
    void jsonRoundTripAndSanitize();
    void rotatesWithThePhoto();
};

void TestKaleidoscope::offIsIdentity()
{
    const QImage src = pattern(40, 30);
    const Geometry g;
    QVERIFY(!g.hasKaleido());
    QVERIFY(g.isIdentity());
    QCOMPARE(CropTransform::applyKaleidoscope(src, g), src);
    QCOMPARE(CropTransform::applyGeometry(src, g), src);
}

void TestKaleidoscope::twoSegmentsMirrorAcrossTheAngle()
{
    /* N = 2 at angle 0 is a mirror across the horizontal line through the centre: the
       lower half (y-down angles 0..180) is the photo, the upper half its reflection. */
    const int W = 64, H = 48;
    const QImage src = pattern(W, H);
    const QImage out = CropTransform::applyKaleidoscope(src, kaleido(2));
    QCOMPARE(out.size(), src.size());
    for (int x = 1; x < W - 1; ++x) {
        for (int d = 0; d < H / 2 - 1; ++d) {
            const int below = H / 2 + d, above = H / 2 - 1 - d;
            QVERIFY2(near(out.pixel(x, below), src.pixel(x, below)),
                     qPrintable(QString("lower half moved at %1,%2").arg(x).arg(below)));
            QVERIFY2(near(out.pixel(x, above), out.pixel(x, below)),
                     qPrintable(QString("not mirrored at %1,%2").arg(x).arg(above)));
        }
    }

    /* At angle 90 the mirror line is vertical: left half reflects the right. */
    const QImage v = CropTransform::applyKaleidoscope(src, kaleido(2, 90.0));
    for (int y = 1; y < H - 1; ++y)
        for (int d = 0; d < W / 2 - 1; ++d)
            QVERIFY(near(v.pixel(W / 2 - 1 - d, y), v.pixel(W / 2 + d, y)));
}

void TestKaleidoscope::firstWedgeIsThePhoto()
{
    /* Six segments: the wedge from the angle to angle + 60 degrees samples the photo
       where it is; the source angle turns the sampled slice away from that. */
    const int W = 80, H = 80;
    const QImage src = pattern(W, H);
    const QImage out = CropTransform::applyKaleidoscope(src, kaleido(6, 10.0));
    const double a = (10.0 + 30.0) * 0.017453292519943295;   // mid-wedge
    for (int r = 4; r < 30; r += 3) {
        const int x = int(40.0 + r * std::cos(a));
        const int y = int(40.0 + r * std::sin(a));
        QVERIFY2(near(out.pixel(x, y), src.pixel(x, y), 2),
                 qPrintable(QString("first wedge changed at %1,%2").arg(x).arg(y)));
    }
    const QImage turned = CropTransform::applyKaleidoscope(src, kaleido(6, 10.0, 25.0));
    int differ = 0;
    for (int r = 4; r < 30; r += 3) {
        const int x = int(40.0 + r * std::cos(a));
        const int y = int(40.0 + r * std::sin(a));
        if (!near(turned.pixel(x, y), src.pixel(x, y), 2)) ++differ;
    }
    QVERIFY(differ > 4);
}

void TestKaleidoscope::pointFormMatchesPixels()
{
    const int W = 90, H = 60;
    const QImage src = smooth(W, H);
    Geometry g = kaleido(8, 33.0, -12.0);
    g.kaleidoX = 0.35; g.kaleidoY = 0.6;
    const QImage out = CropTransform::applyKaleidoscope(src, g);
    for (int y = 3; y < H; y += 7) {
        for (int x = 3; x < W; x += 7) {
            const QPointF n((x + 0.5) / W, (y + 0.5) / H);
            const QPointF sn = CropTransform::kaleidoSourceNorm(n, W, H, g);
            QVERIFY(sn.x() >= 0.0 && sn.x() <= 1.0 && sn.y() >= 0.0 && sn.y() <= 1.0);
            const int sx = qBound(0, int(sn.x() * W), W - 1);
            const int sy = qBound(0, int(sn.y() * H), H - 1);
            /* Bilinear vs nearest: allow a neighbour's worth of the gradient. */
            QVERIFY2(near(out.pixel(x, y), src.pixel(sx, sy), 4),
                     qPrintable(QString("point form disagrees at %1,%2").arg(x).arg(y)));
        }
    }
}

void TestKaleidoscope::keepsSizeAndDepth()
{
    /* A 16-bit export must not be flattened to 8 bits by the fold. */
    QImage deep(50, 40, QImage::Format_RGBA64);
    deep.fill(QColor::fromRgba64(30000, 20000, 10000, 65535));
    const QImage out = CropTransform::applyKaleidoscope(deep, kaleido(4, 45.0));
    QCOMPARE(out.size(), deep.size());
    QCOMPARE(out.format(), QImage::Format_RGBA64);
    QCOMPARE(out.pixelColor(10, 10).rgba64().red(), quint16(30000));

    /* With a crop, the fold runs on the cropped frame. */
    Geometry g = kaleido(6);
    g.cropX = 0.25; g.cropY = 0.25; g.cropW = 0.5; g.cropH = 0.5;
    QCOMPARE(CropTransform::applyGeometry(pattern(80, 60), g).size(), QSize(40, 30));
}

void TestKaleidoscope::jsonRoundTripAndSanitize()
{
    /* Off: nothing written, so existing recipes keep their JSON (and hash). */
    const QJsonObject off = EditStack::geometryToJson(Geometry());
    QVERIFY(!off.contains("kaleidoSegments"));

    Geometry g = kaleido(10, -45.5, 12.0);
    g.kaleidoX = 0.2; g.kaleidoY = 0.7;
    const Geometry back = EditStack::geometryFromJson(EditStack::geometryToJson(g));
    QVERIFY(back.kaleidoEquals(g));
    QVERIFY(!back.isIdentity());

    /* Odd counts round up to even; out-of-range is switched off. */
    EditStack s;
    s.geometry = kaleido(5, 400.0);
    s.geometry.kaleidoX = 3.0;
    QStringList issues;
    EditStack::sanitize(s, &issues);
    QCOMPARE(s.geometry.kaleidoSegments, 6);
    QCOMPARE(s.geometry.kaleidoAngle, 180.0);
    QCOMPARE(s.geometry.kaleidoX, 1.0);
    QVERIFY(!issues.isEmpty());

    s.geometry = kaleido(40);
    EditStack::sanitize(s);
    QCOMPARE(s.geometry.kaleidoSegments, 0);
}

void TestKaleidoscope::rotatesWithThePhoto()
{
    EditStack s;
    s.geometry = kaleido(6, 100.0, 7.0);
    s.geometry.kaleidoX = 0.2; s.geometry.kaleidoY = 0.3;
    QVERIFY(EditRotate::stack(s, 90));
    QCOMPARE(s.geometry.kaleidoX, 0.7);
    QCOMPARE(s.geometry.kaleidoY, 0.2);
    QCOMPARE(s.geometry.kaleidoAngle, -170.0);   // 190 wrapped
    QCOMPARE(s.geometry.kaleidoSource, 7.0);
    EditRotate::stack(s, 270);
    QCOMPARE(s.geometry.kaleidoX, 0.2);
    QCOMPARE(s.geometry.kaleidoY, 0.3);
    QCOMPARE(s.geometry.kaleidoAngle, 100.0);
}

QTEST_GUILESS_MAIN(TestKaleidoscope)
#include "tst_kaleidoscope.moc"
