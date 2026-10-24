#include <QtTest>
#include <functional>

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

/* A hyperbolic {p,q} kaleidoscope (segments = 2p). */
Kaleido hyper(int p, int q, double angle = 0.0, double source = 0.0)
{
    Kaleido k = kaleido(2 * p, angle, source);
    k.type = 1;
    k.vertex = q;
    return k;
}

/* A Polar / Droste kaleidoscope: type 2 / 3, count copies or strands. */
Kaleido polar(int copies, bool wrap = false, bool insideOut = false)
{
    Kaleido k = kaleido(2 * copies);
    k.type = 2;
    k.wrap = wrap;
    k.invert = insideOut;
    return k;
}
Kaleido droste(int strands, double nest = 0.25, bool reverse = false)
{
    Kaleido k = kaleido(2 * strands);
    k.type = 3;
    k.nest = nest;
    k.invert = reverse;
    return k;
}

/* |OM| of the {p,q} tiling's fundamental triangle in disk units (the central
   polygon's inradius), and its edge circle (centre on the x axis, radius). */
double hyperInradius(int p, int q)
{
    const double pi = 3.141592653589793;
    return std::tanh(0.5 * std::acosh(std::cos(pi / q) / std::sin(pi / p)));
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
    void zoomMagnifiesAboutTheCentre();
    void opacityBlendsWithTheFrame();
    void maskedFoldsOnlyTheMaskedPixels();
    void maskedKeepsDepth();
    void paramsJsonRoundTripAndSanitize();
    void eyeFoldsItOut();
    void rotatesWithThePhoto();
    void hyperbolicMinVertex();
    void hyperbolicCentreIsTheMirror();
    void hyperbolicMirrorsAcrossTheEdge();
    void hyperbolicOutsideMirrorsTheRim();
    void hyperbolicRimIsSmooth();
    void hyperbolicKeepsSizeDepthAndMask();
    void hyperbolicParamsJsonAndSanitize();
    void polarRadiusIsTheHeight();
    void polarMirroredSeamsMatch();
    void polarWrapGoesRoundOnce();
    void drosteIsSeamlessAcrossTheCut();
    void drosteSamplesTheAnnulus();
    void mipTypesPointFormMatchesPixels();
    void polarDrosteParamsJsonAndSanitize();
    void wallpaperGroupsHaveTheirSymmetries();
    void wallpaperRegionIsThePhoto();
    void wallpaperParamsJsonAndSanitize();
    void fluteFlatGlassIsIdentity();
    void fluteReedsRepeatAndBend();
    void fluteShadingAndParams();
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
    k.zoom = 1.7;                       // the point form must carry the zoom too
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

void TestKaleidoscope::zoomMagnifiesAboutTheCentre()
{
    /* N = 2 at angle 0: the lower half is the photo. At zoom 2 a pixel d below the
       centre samples the photo d / 2 below it -- the slice is magnified about the
       centre, which itself does not move. */
    const int W = 64, H = 64;
    const QImage src = smooth(W, H);
    Kaleido k = kaleido(2);
    k.zoom = 2.0;
    const QImage out = CropTransform::applyKaleidoscope(src, k);
    for (int d = 2; d < 28; d += 4) {
        const QRgb got = out.pixel(20, 32 + d);
        /* x moves toward the centre too: 32 + (20.5 - 32) / 2. */
        const QRgb want = src.pixel(26, 32 + d / 2);
        QVERIFY2(near(got, want, 4),
                 qPrintable(QString("zoom sample off at d=%1").arg(d)));
    }
    /* Zoom 1 is the plain fold. */
    k.zoom = 1.0;
    QCOMPARE(CropTransform::applyKaleidoscope(src, k),
             CropTransform::applyKaleidoscope(src, kaleido(2)));
}

void TestKaleidoscope::opacityBlendsWithTheFrame()
{
    const int W = 64, H = 48;
    const QImage src = pattern(W, H);
    Kaleido k = kaleido(2);
    const QImage full = CropTransform::applyKaleidoscope(src, k);
    k.opacity = 0.0;
    QVERIFY(near(CropTransform::applyKaleidoscope(src, k).pixel(20, 5), src.pixel(20, 5)));
    k.opacity = 0.5;
    const QImage half = CropTransform::applyKaleidoscope(src, k);
    for (int x = 4; x < W; x += 9) {
        const QRgb a = src.pixel(x, 5), b = full.pixel(x, 5), h = half.pixel(x, 5);
        QVERIFY(qAbs(qRed(h) - (qRed(a) + qRed(b)) / 2) <= 1);
        QVERIFY(qAbs(qGreen(h) - (qGreen(a) + qGreen(b)) / 2) <= 1);
    }
    /* The masked fold: half opacity lays half the mirrored subject over the photo. */
    Kaleido kv = kaleido(2, 90.0);
    const QImage m = maskOf(W, H, [&](int x, int) { return x < W / 2; });
    const QImage mFull = CropTransform::applyKaleidoscopeMasked(src, m, kv);
    kv.opacity = 0.5;
    const QImage mHalf = CropTransform::applyKaleidoscopeMasked(src, m, kv);
    const QRgb a = src.pixel(50, 20), b = mFull.pixel(50, 20), h = mHalf.pixel(50, 20);
    QVERIFY(qAbs(qRed(h) - (qRed(a) + qRed(b)) / 2) <= 2);
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
    p.kaleidoZoom = 2.5f;
    p.kaleidoOpacity = 0.4f;
    QVERIFY(!p.isIdentity());
    QVERIFY(!EditParams::groupIsDefault(p, EditParams::Group::Effects));
    const EditParams back = EditStack::paramsFromJson(EditStack::paramsToJson(p));
    QCOMPARE(back.kaleidoSegments, 10);
    QCOMPARE(back.kaleidoX, 0.25f);
    QCOMPARE(back.kaleidoY, 0.75f);
    QCOMPARE(back.kaleidoAngle, -45.5f);
    QCOMPARE(back.kaleidoSource, 12.0f);
    QCOMPARE(back.kaleidoZoom, 2.5f);
    QCOMPARE(back.kaleidoOpacity, 0.4f);
    /* Zoom 1 (the default) is not written even when on. */
    EditParams z = p;
    z.kaleidoZoom = 1.0f;
    QVERIFY(!EditStack::paramsToJson(z).contains("kaleidoZoom"));

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

void TestKaleidoscope::hyperbolicMinVertex()
{
    /* (p - 2)(q - 2) > 4: the smallest hyperbolic corner count for each polygon. */
    QCOMPARE(EditParams::kaleidoMinVertex(3), 7);
    QCOMPARE(EditParams::kaleidoMinVertex(4), 5);
    QCOMPARE(EditParams::kaleidoMinVertex(5), 4);
    QCOMPARE(EditParams::kaleidoMinVertex(6), 4);
    QCOMPARE(EditParams::kaleidoMinVertex(7), 3);
    QCOMPARE(EditParams::kaleidoMinVertex(1), 7);        // below a triangle reads as one
}

void TestKaleidoscope::hyperbolicCentreIsTheMirror()
{
    /* Inside the central polygon nothing but the wedge fold applies, so a {4,5} tiling
       samples exactly where the Mirror type with 8 segments does. */
    const double W = 300, H = 200;
    Kaleido h = hyper(4, 5, 20.0, -35.0);
    h.x = 0.45; h.y = 0.55;
    h.zoom = 1.5;
    Kaleido m = h;
    m.type = 0;
    const double R = 0.5 * std::min(W, H);
    const double rIn = 0.9 * hyperInradius(4, 5) * R;   // pixels, inside the polygon
    for (int i = 0; i < 24; ++i) {
        const double a = i * 0.29, r = rIn * (i + 1) / 24.0;
        const QPointF n((h.x * W + r * std::cos(a)) / W, (h.y * H + r * std::sin(a)) / H);
        const QPointF sh = CropTransform::kaleidoSourceNorm(n, W, H, h);
        const QPointF sm = CropTransform::kaleidoSourceNorm(n, W, H, m);
        QVERIFY2(std::abs(sh.x() - sm.x()) < 1e-4 && std::abs(sh.y() - sm.y()) < 1e-4,
                 qPrintable(QString("centre differs at sample %1").arg(i)));
    }
}

void TestKaleidoscope::hyperbolicMirrorsAcrossTheEdge()
{
    /* A point just inside the central polygon's edge and its reflection in the edge
       (inversion in the edge circle) sample the same photo pixel: the neighbouring
       polygon is the mirror image, so the seam matches. */
    const double W = 400, H = 400;
    for (const auto &pq : {std::pair<int, int>{4, 5}, {3, 7}, {6, 4}, {5, 5}}) {
        const Kaleido k = hyper(pq.first, pq.second);
        const double m = hyperInradius(pq.first, pq.second);
        const double r = (1.0 - m * m) / (2.0 * m), ec = m + r;
        const double R = 0.5 * W;
        for (double off : {0.02, 0.05}) {
            for (double vy : {0.0, 0.03}) {
                const double ux = m - off, uy = vy;                 // inside, disk units
                const double ex = ux - ec, e2 = ex * ex + uy * uy;
                const double ox = ec + ex * r * r / e2, oy = uy * r * r / e2;   // mirrored
                const QPointF a = CropTransform::kaleidoSourceNorm(
                    QPointF((0.5 * W + ux * R) / W, (0.5 * H + uy * R) / H), W, H, k);
                const QPointF b = CropTransform::kaleidoSourceNorm(
                    QPointF((0.5 * W + ox * R) / W, (0.5 * H + oy * R) / H), W, H, k);
                QVERIFY2(std::abs(a.x() - b.x()) < 1e-4 && std::abs(a.y() - b.y()) < 1e-4,
                         qPrintable(QString("{%1,%2} edge seam off").arg(pq.first)
                                        .arg(pq.second)));
            }
        }
    }
}

void TestKaleidoscope::hyperbolicOutsideMirrorsTheRim()
{
    /* Beyond the disk is its mirror image across the rim: rho and 1 / rho along the
       same ray sample the same pixel. */
    const double W = 500, H = 500, R = 250;
    const Kaleido k = hyper(5, 4, 10.0);
    for (double rho : {0.3, 0.6, 0.85}) {
        for (double a : {0.1, 1.3, 2.9, 4.4}) {
            const QPointF in((250 + rho * R * std::cos(a)) / W, (250 + rho * R * std::sin(a)) / H);
            const QPointF out((250 + R / rho * std::cos(a)) / W,
                              (250 + R / rho * std::sin(a)) / H);
            const QPointF si = CropTransform::kaleidoSourceNorm(in, W, H, k);
            const QPointF so = CropTransform::kaleidoSourceNorm(out, W, H, k);
            QVERIFY(std::abs(si.x() - so.x()) < 1e-4 && std::abs(si.y() - so.y()) < 1e-4);
        }
    }
}

void TestKaleidoscope::hyperbolicRimIsSmooth()
{
    /* Near the rim the copies are far smaller than a pixel; the mip sampling averages
       them, so neighbouring pixels agree instead of sparkling. pattern() wraps every
       few pixels, so at full size (the central polygon) neighbours differ a lot; a
       point-sampled rim would differ MORE. Measured 2026-10-22 ({4,5}, 400 px): centre
       75, rim 20 with the mips, rim 204 with them off. */
    const int W = 400, H = 400;
    const QImage out = CropTransform::applyKaleidoscope(pattern(W, H), hyper(4, 5));
    auto step = [&](double lo, double hi) {
        double sum = 0.0;
        int n = 0;
        for (int y = 1; y < H; ++y) {
            for (int x = 1; x < W; ++x) {
                const double rho = std::hypot(x + 0.5 - 200, y + 0.5 - 200) / 200.0;
                if (rho < lo || rho > hi) continue;
                const QRgb a = out.pixel(x, y), b = out.pixel(x - 1, y);
                sum += qAbs(qRed(a) - qRed(b)) + qAbs(qGreen(a) - qGreen(b)) +
                       qAbs(qBlue(a) - qBlue(b));
                ++n;
            }
        }
        return n > 100 ? sum / n : -1.0;
    };
    const double centre = step(0.0, 0.2), rim = step(0.97, 0.995);
    QVERIFY(centre > 0.0 && rim >= 0.0);
    QVERIFY2(rim < centre * 0.4,
             qPrintable(QString("rim step %1 vs centre %2").arg(rim).arg(centre)));
}

void TestKaleidoscope::hyperbolicKeepsSizeDepthAndMask()
{
    QImage deep(60, 40, QImage::Format_RGBA64);
    deep.fill(QColor::fromRgba64(30000, 20000, 10000, 65535));
    const QImage out = CropTransform::applyKaleidoscope(deep, hyper(4, 5));
    QCOMPARE(out.size(), deep.size());
    QCOMPARE(out.format(), QImage::Format_RGBA64);
    QCOMPARE(out.pixelColor(5, 5).rgba64().red(), quint16(30000));

    /* An empty mask folds nothing: the frame comes back as it was. */
    const QImage src = pattern(80, 60);
    const QImage none = maskOf(80, 60, [](int, int){ return false; });
    const QImage m = CropTransform::applyKaleidoscopeMasked(src, none, hyper(3, 7));
    for (int y = 0; y < 60; y += 5)
        for (int x = 0; x < 80; x += 5)
            QVERIFY(near(m.pixel(x, y), src.pixel(x, y), 0));
    /* A full mask is the whole-frame fold. */
    const QImage all = maskOf(80, 60, [](int, int){ return true; });
    const QImage f = CropTransform::applyKaleidoscope(src, hyper(3, 7));
    const QImage fm = CropTransform::applyKaleidoscopeMasked(src, all, hyper(3, 7));
    for (int y = 0; y < 60; y += 5)
        for (int x = 0; x < 80; x += 5)
            QVERIFY(near(fm.pixel(x, y), f.pixel(x, y), 1));
}

void TestKaleidoscope::hyperbolicParamsJsonAndSanitize()
{
    /* Mirror (type 0): no hyperbolic keys, so existing recipes keep their JSON. */
    EditParams p;
    p.kaleidoSegments = 8;
    QVERIFY(!EditStack::paramsToJson(p).contains("kaleidoType"));
    QVERIFY(!EditStack::paramsToJson(p).contains("kaleidoVertex"));

    p.kaleidoType = 1;
    p.kaleidoVertex = 6;
    p.kaleidoRadius = 1.75f;
    const EditParams back = EditStack::paramsFromJson(EditStack::paramsToJson(p));
    QCOMPARE(back.kaleidoType, 1);
    QCOMPARE(back.kaleidoVertex, 6);
    QCOMPARE(back.kaleidoRadius, 1.75f);

    EditParams r = p;
    EditParams::resetGroup(r, EditParams::Group::Effects);
    QCOMPARE(r.kaleidoType, 0);
    QCOMPARE(r.kaleidoVertex, EditParams().kaleidoVertex);
    QCOMPARE(r.kaleidoRadius, 1.0f);

    /* Hyperbolic needs a triangle at least, and enough polygons at each corner. */
    EditStack s;
    EditScope scope;
    scope.params.kaleidoType = 1;
    scope.params.kaleidoSegments = 4;
    scope.params.kaleidoVertex = 4;
    scope.params.kaleidoRadius = 9.0f;
    s.scopes.append(scope);
    EditStack::sanitize(s);
    QCOMPARE(s.scopes[0].params.kaleidoSegments, 6);
    QCOMPARE(s.scopes[0].params.kaleidoVertex, 7);
    QCOMPARE(s.scopes[0].params.kaleidoRadius, 4.0f);
    s.scopes[0].params.kaleidoType = 9;                   // not a type
    EditStack::sanitize(s);
    QCOMPARE(s.scopes[0].params.kaleidoType, 0);
}

void TestKaleidoscope::polarRadiusIsTheHeight()
{
    /* The bottom row at the centre, the top row on the planet's edge (zoom x half the
       short side); inside out swaps them. */
    const double W = 300, H = 200, R = 100;
    for (bool io : {false, true}) {
        const Kaleido k = polar(1, false, io);
        for (double rho : {0.05, 0.5, 0.95}) {
            const QPointF n((150 + rho * R * std::cos(0.7)) / W,
                            (100 + rho * R * std::sin(0.7)) / H);
            const QPointF sn = CropTransform::kaleidoSourceNorm(n, W, H, k);
            const double want = io ? rho : 1.0 - rho;
            QVERIFY2(std::abs(sn.y() - want) < 0.01,
                     qPrintable(QString("rho %1 -> y %2").arg(rho).arg(sn.y())));
        }
    }
    /* Zoom 2: the planet is twice as large. */
    Kaleido z = polar(1);
    z.zoom = 2.0;
    const QPointF sn = CropTransform::kaleidoSourceNorm(QPointF((150 + 100) / W, 0.5), W, H, z);
    QVERIFY(std::abs(sn.y() - 0.5) < 0.01);
}

void TestKaleidoscope::polarMirroredSeamsMatch()
{
    /* Mirrored copies: either side of every seam samples the same column. */
    const double W = 300, H = 300, r = 80;
    const int copies = 3;
    const Kaleido k = polar(copies);
    for (int seam = 0; seam < 2 * copies; ++seam) {
        const double a = seam * 3.141592653589793 / copies;
        auto at = [&](double t) {
            return CropTransform::kaleidoSourceNorm(
                QPointF((150 + r * std::cos(t)) / W, (150 + r * std::sin(t)) / H), W, H, k);
        };
        const QPointF lo = at(a - 0.002), hi = at(a + 0.002);
        QVERIFY2(std::abs(lo.x() - hi.x()) < 0.01 && std::abs(lo.y() - hi.y()) < 0.01,
                 qPrintable(QString("seam %1 off").arg(seam)));
    }
}

void TestKaleidoscope::polarWrapGoesRoundOnce()
{
    /* Wrap, one copy: the frame's width goes once round, from the spoke (angle 0). */
    const double W = 360, H = 200, r = 60;
    const Kaleido k = polar(1, true);
    for (double deg : {10.0, 90.0, 200.0, 350.0}) {
        const double t = deg * 0.017453292519943295;
        const QPointF sn = CropTransform::kaleidoSourceNorm(
            QPointF((180 + r * std::cos(t)) / W, (100 + r * std::sin(t)) / H), W, H, k);
        QVERIFY2(std::abs(sn.x() - deg / 360.0) < 0.01,
                 qPrintable(QString("%1 deg -> x %2").arg(deg).arg(sn.x())));
    }
}

void TestKaleidoscope::drosteIsSeamlessAcrossTheCut()
{
    /* The log's branch cut (straight out to the left of the centre) must not show:
       just above and just below it sample the same pixel, except where the sample
       sits on the nested copy's own edge (the picture-in-picture border, which is
       part of the look). */
    const double W = 400, H = 300;
    for (const Kaleido &k : {droste(1), droste(2, 0.4), droste(3, 0.2, true)}) {
        int same = 0, n = 0;
        for (double r = 6.0; r < 190.0; r += 3.7) {
            auto at = [&](double t) {
                return CropTransform::kaleidoSourceNorm(
                    QPointF((200 + r * std::cos(t)) / W, (150 + r * std::sin(t)) / H),
                    W, H, k);
            };
            const QPointF a = at(3.141592653589793 - 1e-4), b = at(-3.141592653589793 + 1e-4);
            ++n;
            if (std::abs(a.x() - b.x()) * W < 1.5 && std::abs(a.y() - b.y()) * H < 1.5) ++same;
        }
        QVERIFY2(same >= n * 9 / 10, qPrintable(QString("%1 of %2").arg(same).arg(n)));
    }
}

void TestKaleidoscope::drosteSamplesTheAnnulus()
{
    /* Every sample lies inside the frame and outside the nested copy. */
    const double W = 400, H = 300, nest = 0.3;
    Kaleido k = droste(2, nest);
    k.x = 0.4; k.y = 0.6;
    const double cx = 160, cy = 180;
    for (int i = 0; i < 200; ++i) {
        const QPointF n(((i * 37) % 400 + 0.5) / W, ((i * 53) % 300 + 0.5) / H);
        const QPointF sn = CropTransform::kaleidoSourceNorm(n, W, H, k);
        const double px = sn.x() * W - cx, py = sn.y() * H - cy;
        const bool inner = px > -cx * nest + 1 && px < (W - cx) * nest - 1 &&
                           py > -cy * nest + 1 && py < (H - cy) * nest - 1;
        QVERIFY2(!inner, qPrintable(QString("sample %1 inside the nested copy").arg(i)));
    }
    /* Size and depth kept. */
    QImage deep(60, 40, QImage::Format_RGBA64);
    deep.fill(QColor::fromRgba64(30000, 20000, 10000, 65535));
    for (const Kaleido &kk : {droste(1), polar(2)}) {
        const QImage out = CropTransform::applyKaleidoscope(deep, kk);
        QCOMPARE(out.size(), deep.size());
        QCOMPARE(out.format(), QImage::Format_RGBA64);
        QCOMPARE(out.pixelColor(7, 9).rgba64().green(), quint16(20000));
    }
}

void TestKaleidoscope::mipTypesPointFormMatchesPixels()
{
    /* The point form and the pixels agree where the copies are not much smaller than
       a pixel (on a linear gradient a mip average is the centre value anyway). */
    const int W = 120, H = 80;         // smooth() wraps beyond 127 x 85
    const QImage src = smooth(W, H);
    struct Case { Kaleido k; double rMin, rMax; };
    const Case cases[] = {
        {polar(1), 0.3, 0.9},
        {polar(2, true, true), 0.3, 0.9},
        {droste(1, 0.35), 0.5, 0.95},
    };
    for (const Case &c : cases) {
        const QImage out = CropTransform::applyKaleidoscope(src, c.k);
        int bad = 0, n = 0;
        for (int y = 1; y < H; y += 3) {
            for (int x = 1; x < W; x += 3) {
                const double rho = std::hypot(x + 0.5 - 60, y + 0.5 - 40) / 40.0;
                if (rho < c.rMin || rho > c.rMax) continue;
                const QPointF sn = CropTransform::kaleidoSourceNorm(
                    QPointF((x + 0.5) / W, (y + 0.5) / H), W, H, c.k);
                const int sx = qBound(0, int(sn.x() * W), W - 1);
                const int sy = qBound(0, int(sn.y() * H), H - 1);
                ++n;
                if (!near(out.pixel(x, y), src.pixel(sx, sy), 8)) ++bad;
            }
        }
        QVERIFY(n > 50);
        QVERIFY2(bad <= n / 20, qPrintable(QString("type %1: %2 of %3 disagree")
                                               .arg(c.k.type).arg(bad).arg(n)));
    }
}

void TestKaleidoscope::polarDrosteParamsJsonAndSanitize()
{
    EditParams p;
    p.kaleidoSegments = 4;
    p.kaleidoType = 3;
    QJsonObject o = EditStack::paramsToJson(p);
    QVERIFY(!o.contains("kaleidoNest"));                 // defaults are not written
    QVERIFY(!o.contains("kaleidoInvert"));
    p.kaleidoNest = 0.4f;
    p.kaleidoInvert = 1;
    p.kaleidoWrap = 1;
    const EditParams back = EditStack::paramsFromJson(EditStack::paramsToJson(p));
    QCOMPARE(back.kaleidoType, 3);
    QCOMPARE(back.kaleidoNest, 0.4f);
    QCOMPARE(back.kaleidoInvert, 1);
    QCOMPARE(back.kaleidoWrap, 1);

    EditParams r = p;
    EditParams::resetGroup(r, EditParams::Group::Effects);
    QCOMPARE(r.kaleidoNest, 0.25f);
    QCOMPARE(r.kaleidoInvert, 0);
    QCOMPARE(r.kaleidoWrap, 0);

    EditStack s;
    EditScope scope;
    scope.params = p;
    scope.params.kaleidoNest = 2.0f;
    scope.params.kaleidoInvert = 7;
    s.scopes.append(scope);
    EditStack::sanitize(s);
    QCOMPARE(s.scopes[0].params.kaleidoNest, 0.8f);
    QCOMPARE(s.scopes[0].params.kaleidoInvert, 1);
    QCOMPARE(s.scopes[0].params.kaleidoType, 3);         // 3 is a real type now
    QCOMPARE(s.scopes[0].params.kaleidoSegments, 4);     // Droste keeps 2 strands
}

void TestKaleidoscope::wallpaperGroupsHaveTheirSymmetries()
{
    /* Every group: the pattern is unchanged by its lattice translations and its own
       turns / mirrors / glides (about the centre, in tile units). Points are spread
       over a few cells; a point right on a region edge may pick the other side's copy
       where the group has no mirror there, so allow a few. */
    using Op = std::function<QPointF(QPointF)>;
    const double h = 0.8660254037844386;
    const Op tx = [](QPointF p){ return p + QPointF(1, 0); };
    const Op ty = [](QPointF p){ return p + QPointF(0, 1); };
    const Op hx = [h](QPointF p){ return p + QPointF(0.5, h); };
    auto rot = [](double deg) {
        const double a = deg * 0.017453292519943295;
        return Op([a](QPointF p){ return QPointF(p.x() * std::cos(a) - p.y() * std::sin(a),
                                                 p.x() * std::sin(a) + p.y() * std::cos(a)); });
    };
    const Op mx = [](QPointF p){ return QPointF(-p.x(), p.y()); };
    const Op my = [](QPointF p){ return QPointF(p.x(), -p.y()); };
    const Op diag = [](QPointF p){ return QPointF(p.y(), p.x()); };
    const Op cent = [](QPointF p){ return p + QPointF(0.5, 0.5); };
    const Op glideX = [](QPointF p){ return QPointF(p.x() + 0.5, -p.y()); };
    const Op glidePgg = [](QPointF p){ return QPointF(p.x() + 0.5, 0.5 - p.y()); };
    const Op p4gMirror = [](QPointF p){ return QPointF(0.5 - p.y(), 0.5 - p.x()); };
    const QVector<QVector<Op>> ops = {
        {tx, ty},                                   // p1
        {tx, ty, rot(180)},                         // p2
        {tx, ty, mx},                               // pm
        {tx, ty, glideX},                           // pg
        {tx, ty, mx, cent},                         // cm
        {tx, ty, mx, my},                           // pmm
        {tx, ty, mx, glideX},                       // pmg
        {tx, ty, rot(180), glidePgg},               // pgg
        {tx, ty, mx, my, cent},                     // cmm
        {tx, ty, rot(90)},                          // p4
        {tx, ty, rot(90), diag},                    // p4m
        {tx, ty, rot(90), p4gMirror},               // p4g
        {tx, hx, rot(120)},                         // p3
        {tx, hx, rot(120), mx},                     // p3m1
        {tx, hx, rot(120), my},                     // p31m
        {tx, hx, rot(60)},                          // p6
        {tx, hx, rot(60), my},                      // p6m
    };
    const double W = 1000, H = 1000, T = 100;       // tile 0.1 of the short side
    for (int g = 0; g < 17; ++g) {
        Kaleido k = kaleido(2);
        k.type = 4;
        k.group = g;
        k.tile = 0.1;
        auto f = [&](QPointF c) {
            return CropTransform::kaleidoSourceNorm(
                QPointF((500 + c.x() * T) / W, (500 + c.y() * T) / H), W, H, k);
        };
        int bad = 0, n = 0;
        for (int i = 0; i < 60; ++i) {
            const QPointF z(((i * 37) % 97) / 97.0 * 3.0 - 1.5, ((i * 61) % 89) / 89.0 * 3.0 - 1.5);
            const QPointF a = f(z);
            for (const Op &op : ops.at(g)) {
                const QPointF b = f(op(z));
                ++n;
                if (std::abs(a.x() - b.x()) * W > 0.5 || std::abs(a.y() - b.y()) * H > 0.5) ++bad;
            }
        }
        QVERIFY2(bad <= n / 50, qPrintable(QString("group %1: %2 of %3 differ")
                                               .arg(g).arg(bad).arg(n)));
    }

    /* And not the symmetries a group lacks, so the check above can fail: p1 has no
       half turn, p4 and p6 no mirror, pm no half turn, p3 no sixth turn. */
    const QVector<std::pair<int, Op>> lacks = {
        {0, rot(180)}, {9, mx}, {15, my}, {2, rot(180)}, {12, rot(60)},
    };
    for (const auto &gl : lacks) {
        Kaleido k = kaleido(2);
        k.type = 4;
        k.group = gl.first;
        k.tile = 0.1;
        auto f = [&](QPointF c) {
            return CropTransform::kaleidoSourceNorm(
                QPointF((500 + c.x() * T) / W, (500 + c.y() * T) / H), W, H, k);
        };
        int differ = 0;
        for (int i = 0; i < 60; ++i) {
            const QPointF z(((i * 37) % 97) / 97.0 * 3.0 - 1.5, ((i * 61) % 89) / 89.0 * 3.0 - 1.5);
            const QPointF a = f(z), b = f(gl.second(z));
            if (std::abs(a.x() - b.x()) * W > 0.5 || std::abs(a.y() - b.y()) * H > 0.5) ++differ;
        }
        QVERIFY2(differ >= 30, qPrintable(QString("group %1 has a symmetry it should lack")
                                              .arg(gl.first)));
    }
}

void TestKaleidoscope::wallpaperRegionIsThePhoto()
{
    /* Just beside the centre, inside each group's own region, the photo is untouched
       (angle, source 0, zoom 1). The p6m region is the thin wedge just above the
       +x axis; a point at (0.2, 0.05) tiles is inside it and inside p4m's (quarter
       turns + the diagonal: u >= v >= 0). */
    const double W = 1000, H = 1000;
    for (int g : {10, 16}) {
        Kaleido k = kaleido(2);
        k.type = 4;
        k.group = g;
        k.tile = 0.1;
        const QPointF n(0.52, 0.505);
        const QPointF sn = CropTransform::kaleidoSourceNorm(n, W, H, k);
        QVERIFY2(std::abs(sn.x() - n.x()) < 1e-3 && std::abs(sn.y() - n.y()) < 1e-3,
                 qPrintable(QString("group %1 moved the photo").arg(g)));
    }
}

void TestKaleidoscope::wallpaperParamsJsonAndSanitize()
{
    EditParams p;
    p.kaleidoSegments = 2;
    p.kaleidoType = 4;
    QVERIFY(!EditStack::paramsToJson(p).contains("kaleidoGroup"));     // default p6m
    p.kaleidoGroup = 7;
    p.kaleidoTile = 0.5f;
    const EditParams back = EditStack::paramsFromJson(EditStack::paramsToJson(p));
    QCOMPARE(back.kaleidoType, 4);
    QCOMPARE(back.kaleidoGroup, 7);
    QCOMPARE(back.kaleidoTile, 0.5f);

    EditStack s;
    EditScope scope;
    scope.params = p;
    scope.params.kaleidoGroup = 30;
    scope.params.kaleidoTile = 0.0f;
    s.scopes.append(scope);
    EditStack::sanitize(s);
    QCOMPARE(s.scopes[0].params.kaleidoGroup, 16);
    QCOMPARE(s.scopes[0].params.kaleidoTile, 0.02f);
    QCOMPARE(s.scopes[0].params.kaleidoType, 4);
}

/* Fluted glass: reeds w = flute x short side wide, vertical at angle 0. */
Kaleido flute(double width, double bend, double shade = 0.0)
{
    Kaleido k = kaleido(2);
    k.type = 5;
    k.flute = width;
    k.bend = bend;
    k.shade = shade;
    return k;
}

void TestKaleidoscope::fluteFlatGlassIsIdentity()
{
    /* Refraction 0, no shading: flat glass, the photo as it was. */
    const int W = 120, H = 80;
    const QImage src = smooth(W, H);
    const Kaleido k = flute(0.1, 0.0);
    const QImage out = CropTransform::applyKaleidoscope(src, k);
    for (int y = 0; y < H; y += 7)
        for (int x = 0; x < W; x += 7) {
            const QPointF n((x + 0.5) / W, (y + 0.5) / H);
            const QPointF sn = CropTransform::kaleidoSourceNorm(n, W, H, k);
            QVERIFY(std::abs(sn.x() - n.x()) < 1e-4 && std::abs(sn.y() - n.y()) < 1e-4);
            QVERIFY(near(out.pixel(x, y), src.pixel(x, y), 1));
        }
}

void TestKaleidoscope::fluteReedsRepeatAndBend()
{
    /* One reed over, the sample moves exactly one reed over (across = x at angle 0),
       and nothing moves along the reed. Positive refraction squeezes more photo into
       a reed (the sample runs faster than the pixel); negative runs it backwards. */
    const double W = 1000, H = 1000, w = 50;               // flute 0.05
    const Kaleido k = flute(0.05, 0.5);
    auto f = [&](const Kaleido &kk, double x, double y) {
        return CropTransform::kaleidoSourceNorm(QPointF(x / W, y / H), W, H, kk);
    };
    for (double x : {510.0, 523.0, 547.0}) {
        const QPointF a = f(k, x, 400), b = f(k, x + w, 400);
        QVERIFY(std::abs((b.x() - a.x()) * W - w) < 1e-2);
        QVERIFY(std::abs(a.y() * H - 400.0) < 1e-2);            // along: unmoved
    }
    /* Within the reed centred on the centre (x 475..525). */
    const double d = f(k, 510, 400).x() - f(k, 505, 400).x();
    QVERIFY(d * W > 5.0);                                      // magnified
    const double r = f(flute(0.05, -1.0), 510, 400).x() - f(flute(0.05, -1.0), 505, 400).x();
    QVERIFY(r < 0.0);                                          // reversed
    /* Angle 90: the reeds lie horizontal, so x no longer matters. */
    Kaleido t = k;
    t.angle = 90.0;
    QVERIFY(std::abs(f(t, 505, 470).x() - 0.505) < 1e-4);
}

void TestKaleidoscope::fluteShadingAndParams()
{
    /* Flat grey through shaded flat glass: brighter on one side of each reed, darker
       on the other, untouched at the reed's middle. */
    QImage grey(200, 100, QImage::Format_ARGB32);
    grey.fill(qRgb(128, 128, 128));
    const QImage out = CropTransform::applyKaleidoscope(grey, flute(0.25, 0.0, 1.0));
    /* w = 25 px (the widest), the centre reed spans x 87.5..112.5: a quarter in on
       each side, and its middle (pixel 100's centre is 1/50 of a reed off it). */
    QVERIFY(qRed(out.pixel(106, 50)) > 155);
    QVERIFY(qRed(out.pixel(94, 50)) < 100);
    QVERIFY(qAbs(qRed(out.pixel(100, 50)) - 128) <= 6);

    EditParams p;
    p.kaleidoSegments = 2;
    p.kaleidoType = 5;
    QVERIFY(!EditStack::paramsToJson(p).contains("kaleidoFlute"));
    p.kaleidoFlute = 0.1f;
    p.kaleidoBend = -0.3f;
    p.kaleidoShade = 0.0f;
    const EditParams back = EditStack::paramsFromJson(EditStack::paramsToJson(p));
    QCOMPARE(back.kaleidoFlute, 0.1f);
    QCOMPARE(back.kaleidoBend, -0.3f);
    QCOMPARE(back.kaleidoShade, 0.0f);
    EditStack s;
    EditScope scope;
    scope.params = p;
    scope.params.kaleidoFlute = 0.9f;
    scope.params.kaleidoBend = -4.0f;
    s.scopes.append(scope);
    EditStack::sanitize(s);
    QCOMPARE(s.scopes[0].params.kaleidoFlute, 0.25f);
    QCOMPARE(s.scopes[0].params.kaleidoBend, -1.0f);
    QCOMPARE(s.scopes[0].params.kaleidoType, 5);
}

QTEST_GUILESS_MAIN(TestKaleidoscope)
#include "tst_kaleidoscope.moc"
