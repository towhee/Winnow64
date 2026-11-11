#include <QtTest>
#include <random>

#include "Develop/Transform/croptransform.h"
#include "Develop/editstack.h"
#include "Export/exportsettings.h"

/*
    A crop made at a chosen ratio (Geometry::cropAspect) keeps that ratio to the export.
    The stored rect is normalized, so on its own its rounding to whole pixels decides the
    shape, and the export's long-edge resize rounds each side again: a locked 16 x 9 crop
    exported at 3840 came out 2159 or 2161 high about one time in eleven. Pins
    CropTransform::cropPixelRect (the crop in pixels, the ratio to the nearest pixel),
    ExportSettings::outputSize (the short side from the ratio) and the JSON round trip.
*/

namespace {

Geometry crop(double x, double y, double w, double h, double aspect)
{
    Geometry g;
    g.cropX = x; g.cropY = y; g.cropW = w; g.cropH = h;
    g.cropAspect = aspect;
    return g;
}

ExportSettings longEdge(int px, bool dontEnlarge = true)
{
    ExportSettings s;
    s.sizing = ExportSettings::LongEdge;
    s.longEdgePx = px;
    s.dontEnlarge = dontEnlarge;
    return s;
}

}   // namespace

class TestCropAspect : public QObject
{
    Q_OBJECT

private slots:
    /* No ratio: the rect rounded as before (QRectF::toRect). */
    void freeCropIsRoundedRect()
    {
        const Geometry g = crop(0.1, 0.2, 0.5, 0.3, 0.0);
        const QRect r = CropTransform::cropPixelRect(6000, 4000, g);
        QCOMPARE(r, QRectF(600, 800, 3000, 1200).toRect());
    }

    /* The short side follows the long one, so the pixels are the ratio to the nearest
       pixel however the normalized rect was rounded -- and exactly it when the long
       side allows. */
    void ratioDecidesTheShortSide()
    {
        const double a = 16.0 / 9.0;
        // 0.5626 high instead of 0.5625: a rect from the proxy, slightly off the ratio
        const QRect r = CropTransform::cropPixelRect(
            6000, 4000, crop(0.0, 0.1, 1.0, 6000 * 0.5626 / 4000, a));
        QCOMPARE(r.width(), 6000);
        QCOMPARE(r.height(), 3375);

        const QRect p = CropTransform::cropPixelRect(
            4000, 6000, crop(0.1, 0.0, 3376.4 / 4000, 1.0, 9.0 / 16.0));
        QCOMPARE(p.height(), 6000);
        QCOMPARE(p.width(), 3375);
    }

    /* A rect at the canvas edge whose ratio needs a pixel more than the canvas has is
       shrunk, not let out of the image. */
    void ratioStaysInsideTheCanvas()
    {
        const double a = 16.0 / 9.0;
        const QRect r = CropTransform::cropPixelRect(1600, 899, crop(0, 0, 1, 1, a));
        QVERIFY(QRect(0, 0, 1600, 899).contains(r));
        QCOMPARE(r.height(), 899);
        QCOMPARE(r.width(), qRound(899 * a));
    }

    void exportShortSideFromRatio()
    {
        const double a = 16.0 / 9.0;
        const ExportSettings s = longEdge(3840);
        QCOMPARE(s.outputSize(QSize(6000, 3376), a), QSize(3840, 2160));
        QCOMPARE(s.outputSize(QSize(6000, 3376)), QSize(3840, 2161));   // the old answer
        QCOMPARE(s.outputSize(QSize(3376, 6000), 9.0 / 16.0), QSize(2160, 3840));
        // a picture that is not the crop (an uncropped preview) ignores the ratio
        QCOMPARE(s.outputSize(QSize(6000, 4000), a), QSize(3840, 2560));
        // no resize: the rendered crop goes out as it is
        QCOMPARE(ExportSettings().outputSize(QSize(6000, 3375), a), QSize(6000, 3375));
    }

    /* The end-to-end property: any locked 16 x 9 crop, anywhere, any size, exported at
       a 3840 long edge, is 3840 x 2160 -- upscaled small crops included. */
    void locked16x9AlwaysExports3840x2160()
    {
        const double a = 16.0 / 9.0;
        const ExportSettings s = longEdge(3840, /*dontEnlarge*/false);
        std::mt19937 rng(7);
        std::uniform_real_distribution<double> u(0.0, 1.0);
        const QSize frames[] = {{6000, 4000}, {6048, 4024}, {8256, 5504}, {4032, 3024}};
        for (const QSize &f : frames) {
            for (int i = 0; i < 20000; ++i) {
                const double wn = 0.1 + 0.9 * u(rng);
                // the ratio as the proxy saw it: up to 0.1% off in normalized space
                const double hn = wn * f.width() / a / f.height() * (1.0 + 0.002 * (u(rng) - 0.5));
                if (hn > 1.0) continue;
                const Geometry g = crop(u(rng) * (1 - wn), u(rng) * (1 - hn), wn, hn, a);
                const QRect r = CropTransform::cropPixelRect(f.width(), f.height(), g);
                QCOMPARE(s.outputSize(r.size(), a), QSize(3840, 2160));
            }
        }
    }

    void jsonRoundTripAndSanitize()
    {
        Geometry g = crop(0.1, 0.1, 0.8, 0.45, 16.0 / 9.0);
        Geometry back = EditStack::geometryFromJson(EditStack::geometryToJson(g));
        QCOMPARE(back.cropAspect, g.cropAspect);

        // a free crop writes no key, and a sidecar without one reads as free
        g.cropAspect = 0.0;
        QVERIFY(!EditStack::geometryToJson(g).contains("cropAspect"));
        QCOMPARE(EditStack::geometryFromJson(EditStack::geometryToJson(g)).cropAspect, 0.0);

        EditStack s;
        s.geometry = crop(0.1, 0.1, 0.8, 0.45, -3.0);
        EditStack::sanitize(s);
        QCOMPARE(s.geometry.cropAspect, 0.0);
    }
};

QTEST_GUILESS_MAIN(TestCropAspect)
#include "tst_cropaspect.moc"
