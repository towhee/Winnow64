#include <QtTest>
#include <QImage>
#include <QRandomGenerator>

#include "Develop/inputtransform.h"
#include "Develop/colorspace.h"
#include "Lcms2/lcms2.h"

/*
    InputTransform reads a display-referred file through its EMBEDDED profile. Before, it
    assumed sRGB, so an Adobe RGB JPEG was developed as though its numbers were sRGB
    (desaturated, wrong exports). These pin: an embedded sRGB profile gives what the
    no-profile path gives; a wide-gamut profile lands on the analytic working-space value;
    anything lcms cannot use as RGB falls back to sRGB rather than failing; the white
    balance still reads as-shot D65; and the strip-parallel conversion honours row
    padding.
*/
namespace {

QByteArray profileBytes(cmsHPROFILE p)
{
    cmsUInt32Number n = 0;
    cmsSaveProfileToMem(p, nullptr, &n);
    QByteArray b(int(n), '\0');
    cmsSaveProfileToMem(p, b.data(), &n);
    cmsCloseProfile(p);
    return b;
}

/* Adobe RGB (1998): its primaries, D65, a pure 563/256 gamma. */
QByteArray adobeRgb()
{
    const cmsCIExyY d65 = {0.3127, 0.3290, 1.0};
    const cmsCIExyYTRIPLE prim = {{0.64, 0.33, 1.0}, {0.21, 0.71, 1.0}, {0.15, 0.06, 1.0}};
    cmsToneCurve *g = cmsBuildGamma(nullptr, 563.0 / 256.0);
    cmsToneCurve *c[3] = {g, g, g};
    const QByteArray b = profileBytes(cmsCreateRGBProfile(&d65, &prim, c));
    cmsFreeToneCurve(g);
    return b;
}

QImage solid(int w, int h, QColor c)
{
    QImage im(w, h, QImage::Format_RGB888);
    im.fill(c);
    return im;
}

const float *px(const WorkingImage &w, int x, int y)
{
    return w.rgb.data() + (size_t(y) * w.width + x) * 3;
}

} // namespace

class TstInputTransform : public QObject
{
    Q_OBJECT

private slots:
    void embeddedSrgbMatchesTheDefault()
    {
        /* Odd width (RGB888 rows padded) and tall enough for many 64-row strips. */
        QImage im(37, 300, QImage::Format_RGB888);
        QRandomGenerator rng(7);
        for (int y = 0; y < im.height(); ++y)
            for (int x = 0; x < im.width(); ++x)
                im.setPixel(x, y, qRgb(rng.bounded(256), rng.bounded(256), rng.bounded(256)));

        WorkingImage a, b;
        InputTransform in;
        QVERIFY(in.FromImage(im, a));
        QVERIFY(in.FromImage(im, b, profileBytes(cmsCreate_sRGBProfile())));
        QCOMPARE(b.rgb.size(), a.rgb.size());
        float worst = 0.0f;
        for (size_t i = 0; i < a.rgb.size(); ++i)
            worst = std::max(worst, std::abs(a.rgb[i] - b.rgb[i]));
        QVERIFY2(worst < 2e-3f, qPrintable(QString("max diff %1").arg(worst)));
    }

    void adobeRgbKeepsItsGamut()
    {
        /* Full Adobe RGB green: linear (0,1,0) in Adobe RGB, so the working value is the
           Adobe -> working matrix's middle column. Read as sRGB it would be sRGB green,
           visibly less saturated. */
        WorkingImage w, asSrgb;
        InputTransform in;
        QVERIFY(in.FromImage(solid(4, 4, QColor(0, 255, 0)), w, adobeRgb()));
        QVERIFY(in.FromImage(solid(4, 4, QColor(0, 255, 0)), asSrgb));
        const ColorSpaceMath::Matrix3 m = ColorSpaceMath::matrix(
            ColorSpaceMath::ColorSpace::LinearAdobeRGB, ColorSpaceMath::kWorking);
        const float *p = px(w, 1, 1);
        for (int c = 0; c < 3; ++c)
            QVERIFY2(std::abs(p[c] - float(m.m[c][1])) < 3e-3f,
                     qPrintable(QString("channel %1: %2 vs %3").arg(c).arg(p[c])
                                    .arg(m.m[c][1])));
        /* And it differs from the sRGB reading by a real amount. */
        const float *q = px(asSrgb, 1, 1);
        QVERIFY(std::abs(p[0] - q[0]) + std::abs(p[2] - q[2]) > 0.02f);
    }

    void unusableProfilesFallBackToSrgb()
    {
        const QImage im = solid(5, 3, QColor(200, 120, 60));
        WorkingImage want;
        InputTransform in;
        QVERIFY(in.FromImage(im, want));

        cmsToneCurve *g = cmsBuildGamma(nullptr, 2.2);
        const cmsCIExyY d65 = {0.3127, 0.3290, 1.0};
        const QByteArray gray = profileBytes(cmsCreateGrayProfile(&d65, g));
        cmsFreeToneCurve(g);
        for (const QByteArray &icc : {gray, QByteArray("not a profile at all")}) {
            WorkingImage got;
            QVERIFY(in.FromImage(im, got, icc));
            QCOMPARE(got.rgb, want.rgb);
        }
    }

    void asShotStaysD65()
    {
        WorkingImage plain, tagged;
        InputTransform in;
        QVERIFY(in.FromImage(solid(4, 4, Qt::gray), plain));
        QVERIFY(in.FromImage(solid(4, 4, Qt::gray), tagged, adobeRgb()));
        QVERIFY(tagged.cam.valid);
        QVERIFY(tagged.cam.asShotK > 0.0f);
        QVERIFY2(std::abs(tagged.cam.asShotK - plain.cam.asShotK) < 25.0f,
                 qPrintable(QString("%1 K vs %2 K").arg(tagged.cam.asShotK)
                                .arg(plain.cam.asShotK)));
        QVERIFY(std::abs(tagged.cam.asShotTint - plain.cam.asShotTint) < 1.0f);
    }
};

QTEST_MAIN(TstInputTransform)
#include "tst_inputtransform.moc"
