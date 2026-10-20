#include <QtTest>

#include "Develop/Transform/croptransform.h"
#include "Develop/editrotate.h"

/*
    Effects > Kaleidoscope (CropTransform::applyKaleidoscope / applyKaleidoscopeMasked),
    the fold that runs after the crop. Pins the properties the look depends on: the first
    wedge is the photo untouched, the next is its mirror image (so every seam matches),
    the point form agrees with the pixels, a mask scope folds ONLY its masked pixels over
    the frame, and the stored settings (per scope, in EditParams) round-trip, sanitize,
    fold out under the eye and rotate with the photo.
*/

namespace {

using CropTransform::Kaleido;

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

/* A coverage image: 255 where inside(x, y), else 0. */
template <typename F>
QImage maskOf(int w, int h, F inside)
{
    QImage m(w, h, QImage::Format_Grayscale8);
    for (int y = 0; y < h; ++y) {
        uchar *row = m.scanLine(y);
        for (int x = 0; x < w; ++x) row[x] = inside(x, y) ? 255 : 0;
    }
    return m;
}

bool near(QRgb a, QRgb b, int tol = 1)
{
    return qAbs(qRed(a) - qRed(b)) <= tol && qAbs(qGreen(a) - qGreen(b)) <= tol &&
           qAbs(qBlue(a) - qBlue(b)) <= tol;
}

Kaleido kaleido(int segments, double angle = 0.0, double source = 0.0)
{
    Kaleido k;
    k.segments = segments;
    k.angle = angle;
    k.source = source;
    return k;
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
    void maskedFoldsOnlyTheMaskedPixels();
    void maskedKeepsDepth();
    void paramsJsonRoundTripAndSanitize();
    void eyeFoldsItOut();
    void rotatesWithThePhoto();
};

void TestKaleidoscope::offIsIdentity()
{
    const QImage src = pattern(40, 30);
    const Kaleido k;
    QVERIFY(!k.on());
    QCOMPARE(CropTransform::applyKaleidoscope(src, k), src);
    QCOMPARE(CropTransform::applyKaleidoscopeMasked(src, maskOf(40, 30, [](int, int) {
                 return true; }), k), src);
    /* Not part of the geometry any more: a plain Geometry stays identity. */
    QVERIFY(Geometry().isIdentity());
    QVERIFY(EditParams().isIdentity());
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
    Kaleido k = kaleido(8, 33.0, -12.0);
    k.x = 0.35; k.y = 0.6;
    const QImage out = CropTransform::applyKaleidoscope(src, k);
    for (int y = 3; y < H; y += 7) {
        for (int x = 3; x < W; x += 7) {
            const QPointF n((x + 0.5) / W, (y + 0.5) / H);
            const QPointF sn = CropTransform::kaleidoSourceNorm(n, W, H, k);
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

    /* After a crop, the fold keeps the cropped frame's size. */
    Geometry g;
    g.cropX = 0.25; g.cropY = 0.25; g.cropW = 0.5; g.cropH = 0.5;
    const QImage cropped = CropTransform::applyGeometry(pattern(80, 60), g);
    QCOMPARE(cropped.size(), QSize(40, 30));
    QCOMPARE(CropTransform::applyKaleidoscope(cropped, kaleido(6)).size(), QSize(40, 30));
}

void TestKaleidoscope::maskedFoldsOnlyTheMaskedPixels()
{
    /* N = 2 at angle 90: the mirror line is vertical, the LEFT half (y-down 90..270) is
       the sampled slice and the right half its reflection. */
    const int W = 64, H = 48;
    const QImage src = pattern(W, H);
    const Kaleido k = kaleido(2, 90.0);
    const QImage full = CropTransform::applyKaleidoscope(src, k);

    /* Mask = the sampled half: the masked content is all there is to fold, so the
       result is the plain fold. */
    const QImage left = CropTransform::applyKaleidoscopeMasked(
        src, maskOf(W, H, [&](int x, int) { return x < W / 2; }), k);
    for (int y = 1; y < H - 1; ++y)
        for (int x = 1; x < W - 1; ++x)
            QVERIFY2(near(left.pixel(x, y), full.pixel(x, y), 2),
                     qPrintable(QString("left-masked fold differs at %1,%2").arg(x).arg(y)));

    /* Mask = the OTHER half: the slice the fold samples is unmasked -- transparent --
       so nothing is laid over the frame. Away from the seam's bilinear tap, the frame
       is untouched. */
    const QImage right = CropTransform::applyKaleidoscopeMasked(
        src, maskOf(W, H, [&](int x, int) { return x >= W / 2; }), k);
    for (int y = 1; y < H - 1; ++y)
        for (int x = 1; x < W - 1; ++x) {
            if (qAbs(x - W / 2) <= 1) continue;
            QVERIFY2(near(right.pixel(x, y), src.pixel(x, y)),
                     qPrintable(QString("unmasked slice leaked at %1,%2").arg(x).arg(y)));
        }

    /* A mask over a small subject: its reflection lands on the far side, the rest of
       the frame stays the photo. */
    const QImage blob = CropTransform::applyKaleidoscopeMasked(
        src, maskOf(W, H, [&](int x, int y) {
            return x >= 8 && x < 16 && y >= 20 && y < 28; }), k);
    QVERIFY(near(blob.pixel(W - 1 - 12, 24), src.pixel(12, 24), 2));   // mirrored subject
    QVERIFY(near(blob.pixel(40, 5), src.pixel(40, 5)));                 // elsewhere
    QVERIFY(near(blob.pixel(12, 24), src.pixel(12, 24), 2));            // the subject
}

void TestKaleidoscope::maskedKeepsDepth()
{
    QImage deep(40, 30, QImage::Format_RGBA64);
    deep.fill(QColor::fromRgba64(40000, 20000, 1000, 65535));
    const QImage out = CropTransform::applyKaleidoscopeMasked(
        deep, maskOf(40, 30, [](int x, int) { return x < 20; }), kaleido(4));
    QCOMPARE(out.format(), QImage::Format_RGBA64);
    QCOMPARE(out.pixelColor(5, 5).rgba64().red(), quint16(40000));
    /* A mask of the wrong size is refused rather than misread. */
    QCOMPARE(CropTransform::applyKaleidoscopeMasked(deep, QImage(10, 10,
                 QImage::Format_Grayscale8), kaleido(4)), deep);
}

void TestKaleidoscope::paramsJsonRoundTripAndSanitize()
{
    /* Off: nothing written, so existing recipes keep their JSON (and hash). */
    const QJsonObject off = EditStack::paramsToJson(EditParams());
    QVERIFY(!off.contains("kaleidoSegments"));
    QVERIFY(!off.contains("kaleidoX"));

    EditParams p;
    p.kaleidoSegments = 10;
    p.kaleidoX = 0.25f; p.kaleidoY = 0.75f;
    p.kaleidoAngle = -45.5f; p.kaleidoSource = 12.0f;
    QVERIFY(!p.isIdentity());
    QVERIFY(!EditParams::groupIsDefault(p, EditParams::Group::Effects));
    const EditParams back = EditStack::paramsFromJson(EditStack::paramsToJson(p));
    QCOMPARE(back.kaleidoSegments, 10);
    QCOMPARE(back.kaleidoX, 0.25f);
    QCOMPARE(back.kaleidoY, 0.75f);
    QCOMPARE(back.kaleidoAngle, -45.5f);
    QCOMPARE(back.kaleidoSource, 12.0f);

    /* Reset Effects takes it with the rest of the section. */
    EditParams r = p;
    EditParams::resetGroup(r, EditParams::Group::Effects);
    QVERIFY(!r.hasKaleido());
    QCOMPARE(r.kaleidoX, 0.5f);

    /* Odd counts round up to even; out-of-range is switched off. */
    EditStack s;
    EditScope scope;
    scope.params.kaleidoSegments = 5;
    scope.params.kaleidoAngle = 400.0f;
    scope.params.kaleidoX = 3.0f;
    s.scopes.append(scope);
    QStringList issues;
    EditStack::sanitize(s, &issues);
    QCOMPARE(s.scopes[0].params.kaleidoSegments, 6);
    QCOMPARE(s.scopes[0].params.kaleidoAngle, 180.0f);    // clamped into range
    QCOMPARE(s.scopes[0].params.kaleidoX, 1.0f);
    QVERIFY(!issues.isEmpty());

    s.scopes[0].params.kaleidoSegments = 40;
    EditStack::sanitize(s);
    QCOMPARE(s.scopes[0].params.kaleidoSegments, 0);
}

void TestKaleidoscope::eyeFoldsItOut()
{
    EditScope l;
    l.params.kaleidoSegments = 6;
    l.params.kaleidoAngle = 30.0f;
    QVERIFY(effectiveScopeParams(l).hasKaleido());
    l.showKaleido = false;                         // the dropdown row's eye
    QVERIFY(!effectiveScopeParams(l).hasKaleido());
    QCOMPARE(effectiveScopeParams(l).kaleidoAngle, 30.0f);   // settings kept
    l.showKaleido = true;
    l.showEffects = false;                         // the Effects header's eye
    QVERIFY(!effectiveScopeParams(l).hasKaleido());
    QCOMPARE(l.params.kaleidoSegments, 6);         // stored value untouched
}

void TestKaleidoscope::rotatesWithThePhoto()
{
    EditStack s;
    EditScope g, m;
    g.params.kaleidoSegments = 6;
    g.params.kaleidoAngle = 100.0f;
    g.params.kaleidoSource = 7.0f;
    g.params.kaleidoX = 0.2f; g.params.kaleidoY = 0.3f;
    m.params.kaleidoSegments = 4;                  // a mask scope's own kaleidoscope
    m.params.kaleidoX = 0.5f; m.params.kaleidoY = 0.25f;
    s.scopes.append(g);
    s.scopes.append(m);
    QVERIFY(EditRotate::stack(s, 90));
    QCOMPARE(s.scopes[0].params.kaleidoX, 0.7f);
    QCOMPARE(s.scopes[0].params.kaleidoY, 0.2f);
    QCOMPARE(s.scopes[0].params.kaleidoAngle, -170.0f);   // 190 wrapped
    QCOMPARE(s.scopes[0].params.kaleidoSource, 7.0f);
    QCOMPARE(s.scopes[1].params.kaleidoX, 0.75f);
    QCOMPARE(s.scopes[1].params.kaleidoY, 0.5f);
    EditRotate::stack(s, 270);
    QCOMPARE(s.scopes[0].params.kaleidoX, 0.2f);
    QCOMPARE(s.scopes[0].params.kaleidoY, 0.3f);
    QCOMPARE(s.scopes[0].params.kaleidoAngle, 100.0f);

    /* An untouched recipe is not "moved" (no sidecar write for nothing), and its
       kaleidoscope settings stay at their defaults -- or Effects would read edited. */
    EditStack plain;
    plain.scopes.append(EditScope());
    QVERIFY(!EditRotate::stack(plain, 90));
    QVERIFY(EditParams::groupIsDefault(plain.scopes[0].params, EditParams::Group::Effects));
}

QTEST_GUILESS_MAIN(TestKaleidoscope)
#include "tst_kaleidoscope.moc"
