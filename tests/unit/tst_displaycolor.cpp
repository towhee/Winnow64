#include <QtTest>
#include <QTemporaryDir>
#include <QImage>

#include "Utilities/displaycolor.h"
#include "Main/global.h"
#include "Lcms2/lcms2.h"

/*
    DisplayColor converts the sRGB frames Winnow renders itself (the Develop loupe) for a
    wide-gamut display. These pin the three things the loupe depends on: it is a no-op
    (the SAME image, no copy) whenever there is nothing to convert; on a wide-gamut
    display saturated colours come back less saturated, which is the whole fix; and every
    pixel layout the render paths produce converts identically, alpha and row padding
    included, across the parallel strips.
*/
class TstDisplayColor : public QObject
{
    Q_OBJECT

private:
    QTemporaryDir dir;
    QString p3Path;
    QString srgbPath;

    static QString saveProfile(cmsHPROFILE p, const QString &path)
    {
        cmsSaveProfileToFile(p, QFile::encodeName(path).constData());
        cmsCloseProfile(p);
        return path;
    }

    void use(bool manage, const QString &path)
    {
        G::colorManage = manage;
        G::winOutProfilePath = path;
        DisplayColor::Rebuild();
    }

    static QImage filled(int w, int h, QImage::Format f, QRgb c)
    {
        QImage im(w, h, f);
        im.fill(QColor::fromRgba(c));
        return im;
    }

private slots:
    void initTestCase()
    {
        QVERIFY(dir.isValid());
        // Display P3: P3 primaries, D65, the sRGB tone curve (a MacBook's display)
        const cmsCIExyY d65 = {0.3127, 0.3290, 1.0};
        const cmsCIExyYTRIPLE p3 = {{0.680, 0.320, 1.0}, {0.265, 0.690, 1.0},
                                    {0.150, 0.060, 1.0}};
        const cmsFloat64Number srgbCurve[5] = {2.4, 1.0 / 1.055, 0.055 / 1.055, 1.0 / 12.92,
                                               0.04045};
        cmsToneCurve *trc = cmsBuildParametricToneCurve(nullptr, 4, srgbCurve);
        cmsToneCurve *trcs[3] = {trc, trc, trc};
        p3Path = saveProfile(cmsCreateRGBProfile(&d65, &p3, trcs), dir.filePath("p3.icc"));
        cmsFreeToneCurve(trc);
        srgbPath = saveProfile(cmsCreate_sRGBProfile(), dir.filePath("srgb.icc"));
    }

    void inactiveWhenColorManagementOff()
    {
        use(false, p3Path);
        QVERIFY(!DisplayColor::IsActive());
        const QImage im = filled(8, 8, QImage::Format_RGB32, qRgb(255, 0, 0));
        QCOMPARE(DisplayColor::ToDisplay(im).cacheKey(), im.cacheKey());
    }

    void wideGamutIsReportedEvenWhenOff()
    {
        /* What the Develop alert row keys on: the display differs from sRGB, and colour
           management is off so nothing corrects it. */
        use(false, p3Path);
        QVERIFY(DisplayColor::DisplayDiffersFromSrgb());
        use(true, p3Path);
        QVERIFY(DisplayColor::DisplayDiffersFromSrgb());
        use(false, srgbPath);
        QVERIFY(!DisplayColor::DisplayDiffersFromSrgb());
        use(false, QString());
        QVERIFY(!DisplayColor::DisplayDiffersFromSrgb());
    }

    void inactiveWithoutAProfile()
    {
        use(true, QString());           // Linux: no display profile lookup
        QVERIFY(!DisplayColor::IsActive());
    }

    void inactiveForAnSrgbDisplay()
    {
        use(true, srgbPath);
        QVERIFY(!DisplayColor::IsActive());
        const QImage im = filled(8, 8, QImage::Format_RGB888, qRgb(200, 120, 90));
        QCOMPARE(DisplayColor::ToDisplay(im).cacheKey(), im.cacheKey());
    }

    void wideGamutDesaturates()
    {
        use(true, p3Path);
        QVERIFY(DisplayColor::IsActive());

        /* sRGB red sits well inside P3: about (234, 51, 35) there. */
        const QImage red = DisplayColor::ToDisplay(
            filled(4, 4, QImage::Format_RGB32, qRgb(255, 0, 0)));
        const QRgb r = red.pixel(1, 1);
        QVERIFY2(qRed(r) < 245 && qGreen(r) > 30 && qBlue(r) > 20,
                 qPrintable(QString("red -> %1 %2 %3")
                                .arg(qRed(r)).arg(qGreen(r)).arg(qBlue(r))));

        /* A skin tone: the red-blue spread (its saturation) narrows. */
        const QImage skin = DisplayColor::ToDisplay(
            filled(4, 4, QImage::Format_RGB32, qRgb(224, 160, 140)));
        const QRgb s = skin.pixel(1, 1);
        QVERIFY(qRed(s) - qBlue(s) < 224 - 140);

        /* Neutrals are untouched: same white point, same tone curve. */
        const QImage grey = DisplayColor::ToDisplay(
            filled(4, 4, QImage::Format_RGB32, qRgb(128, 128, 128)));
        const QRgb g = grey.pixel(1, 1);
        QVERIFY(qAbs(qRed(g) - 128) <= 1 && qAbs(qGreen(g) - 128) <= 1
                && qAbs(qBlue(g) - 128) <= 1);
    }

    void everyLayoutConvertsAlike()
    {
        use(true, p3Path);
        /* Odd width: RGB888 rows are padded to 4 bytes, so a stride slip shows. Tall
           enough for many 64-row strips, so the parallel split is exercised. */
        const int W = 37, H = 333;
        QImage ref(W, H, QImage::Format_ARGB32);
        for (int y = 0; y < H; ++y)
            for (int x = 0; x < W; ++x)
                ref.setPixel(x, y, qRgba((x * 7) & 255, (y * 3) & 255, (x * y) & 255, 255));
        const QImage want = DisplayColor::ToDisplay(ref);
        QCOMPARE(want.format(), QImage::Format_ARGB32);

        const QImage::Format formats[] = {
            QImage::Format_RGB888, QImage::Format_RGB32, QImage::Format_RGBX8888,
            QImage::Format_RGBA8888, QImage::Format_ARGB32_Premultiplied,
            QImage::Format_RGBA8888_Premultiplied, QImage::Format_RGBX64};
        for (QImage::Format f : formats) {
            const QImage got = DisplayColor::ToDisplay(ref.convertToFormat(f));
            QCOMPARE(got.size(), ref.size());
            const QImage a = got.convertToFormat(QImage::Format_ARGB32);
            int worst = 0;
            for (int y = 0; y < H; ++y)
                for (int x = 0; x < W; ++x) {
                    const QRgb p = a.pixel(x, y), q = want.pixel(x, y);
                    worst = qMax(worst, qAbs(qRed(p) - qRed(q)));
                    worst = qMax(worst, qAbs(qGreen(p) - qGreen(q)));
                    worst = qMax(worst, qAbs(qBlue(p) - qBlue(q)));
                }
            QVERIFY2(worst <= 1, qPrintable(QString("format %1 differs by %2")
                                                .arg(int(f)).arg(worst)));
        }
    }

    void alphaIsCarried()
    {
        use(true, p3Path);
        QImage im(5, 5, QImage::Format_ARGB32);
        im.fill(QColor(200, 100, 50, 77));
        const QImage out = DisplayColor::ToDisplay(im);
        QCOMPARE(qAlpha(out.pixel(2, 2)), 77);
    }
};

QTEST_MAIN(TstDisplayColor)
#include "tst_displaycolor.moc"
