#include <QtTest>
#include <QFile>

#include "Metadata/colorspaces.h"
#include "Lcms2/lcms2.h"

/*
    ColorSpaces classifies an image's colour space into the published lookup table that
    G::ICCSpaceColumn, the catalog index and the "colorspace" query share. These pin the
    ids (a published format), that an embedded profile is identified by its colorants
    whatever it calls itself, that sRGB additionally needs sRGB's curve (the readers DROP
    an sRGB profile's bytes, so a linear one must not be mistaken for it), and the EXIF
    and HEIC nclx fallbacks.
*/
namespace {

QByteArray bytesOf(cmsHPROFILE p)
{
    cmsUInt32Number n = 0;
    cmsSaveProfileToMem(p, nullptr, &n);
    QByteArray b(int(n), '\0');
    cmsSaveProfileToMem(p, b.data(), &n);
    cmsCloseProfile(p);
    return b;
}

QByteArray rgb(const cmsCIExyY &white, const cmsCIExyYTRIPLE &prim, double gamma)
{
    cmsToneCurve *g = cmsBuildGamma(nullptr, gamma);
    cmsToneCurve *c[3] = {g, g, g};
    const QByteArray b = bytesOf(cmsCreateRGBProfile(&white, &prim, c));
    cmsFreeToneCurve(g);
    return b;
}

const cmsCIExyY kD65 = {0.3127, 0.3290, 1.0};
const cmsCIExyY kD50 = {0.3457, 0.3585, 1.0};

} // namespace

class TstColorSpaces : public QObject
{
    Q_OBJECT

private slots:
    void idsArePublished()
    {
        /*  Stored in the catalog and the library snapshot: renumbering would relabel
            every image already indexed. */
        QCOMPARE(int(ColorSpaces::sRGB), 1);
        QCOMPARE(int(ColorSpaces::AdobeRGB), 2);
        QCOMPARE(int(ColorSpaces::DisplayP3), 3);
        QCOMPARE(int(ColorSpaces::ProPhotoRGB), 4);
        QCOMPARE(int(ColorSpaces::Rec2020), 5);
        QCOMPARE(int(ColorSpaces::Gray), 6);
        QCOMPARE(int(ColorSpaces::CMYK), 7);
        QCOMPARE(int(ColorSpaces::OtherProfile), 8);
        QCOMPARE(int(ColorSpaces::Uncalibrated), 9);
        QCOMPARE(ColorSpaces::names().size(), int(ColorSpaces::Count) - 1);
        QCOMPARE(ColorSpaces::name(1), QString("sRGB"));
        QCOMPARE(ColorSpaces::name(3), QString("Display P3"));
        QVERIFY(ColorSpaces::name(0).isEmpty());
        QVERIFY(ColorSpaces::name(99).isEmpty());
    }

    void profilesByColorants()
    {
        QCOMPARE(ColorSpaces::fromIcc(bytesOf(cmsCreate_sRGBProfile())),
                 int(ColorSpaces::sRGB));
        QCOMPARE(ColorSpaces::fromIcc(rgb(kD65, {{0.64, 0.33, 1}, {0.21, 0.71, 1},
                                                 {0.15, 0.06, 1}}, 563.0 / 256)),
                 int(ColorSpaces::AdobeRGB));
        QCOMPARE(ColorSpaces::fromIcc(rgb(kD65, {{0.680, 0.320, 1}, {0.265, 0.690, 1},
                                                 {0.150, 0.060, 1}}, 2.2)),
                 int(ColorSpaces::DisplayP3));
        QCOMPARE(ColorSpaces::fromIcc(rgb(kD50, {{0.7347, 0.2653, 1}, {0.1596, 0.8404, 1},
                                                 {0.0366, 0.0001, 1}}, 1.8)),
                 int(ColorSpaces::ProPhotoRGB));
        QCOMPARE(ColorSpaces::fromIcc(rgb(kD65, {{0.708, 0.292, 1}, {0.170, 0.797, 1},
                                                 {0.131, 0.046, 1}}, 2.4)),
                 int(ColorSpaces::Rec2020));
        /*  Some other gamut entirely (wide-gamut "Apple RGB"-like primaries). */
        QCOMPARE(ColorSpaces::fromIcc(rgb(kD65, {{0.625, 0.340, 1}, {0.280, 0.595, 1},
                                                 {0.155, 0.070, 1}}, 1.8)),
                 int(ColorSpaces::OtherProfile));
    }

    void srgbNeedsItsCurve()
    {
        /*  sRGB primaries, LINEAR curve: not sRGB, or the reader would drop the profile
            and the image would be decoded as gamma-encoded. A 2.2 gamma is close enough
            to show identically and counts. */
        const cmsCIExyYTRIPLE srgbPrim = {{0.64, 0.33, 1}, {0.30, 0.60, 1},
                                          {0.15, 0.06, 1}};
        QCOMPARE(ColorSpaces::fromIcc(rgb(kD65, srgbPrim, 1.0)),
                 int(ColorSpaces::OtherProfile));
        QCOMPARE(ColorSpaces::fromIcc(rgb(kD65, srgbPrim, 2.2)), int(ColorSpaces::sRGB));
    }

    void nonRgbAndJunk()
    {
        cmsToneCurve *g = cmsBuildGamma(nullptr, 2.2);
        QCOMPARE(ColorSpaces::fromIcc(bytesOf(cmsCreateGrayProfile(&kD65, g))),
                 int(ColorSpaces::Gray));
        cmsFreeToneCurve(g);
        QCOMPARE(ColorSpaces::fromIcc(bytesOf(cmsCreateLab4Profile(nullptr))),
                 int(ColorSpaces::OtherProfile));
        QCOMPARE(ColorSpaces::fromIcc(QByteArray()), int(ColorSpaces::None));
        QCOMPARE(ColorSpaces::fromIcc(QByteArray(40, 'x')), int(ColorSpaces::None));
        QCOMPARE(ColorSpaces::fromIcc(QByteArray(400, 'x')),
                 int(ColorSpaces::OtherProfile));
    }

    void realVendorProfiles_data()
    {
        QTest::addColumn<QString>("path");
        QTest::addColumn<int>("id");
        const QString d = "/System/Library/ColorSync/Profiles/";
        QTest::newRow("Apple sRGB") << d + "sRGB Profile.icc" << int(ColorSpaces::sRGB);
        QTest::newRow("Apple Adobe RGB") << d + "AdobeRGB1998.icc"
                                         << int(ColorSpaces::AdobeRGB);
        QTest::newRow("Apple Display P3") << d + "Display P3.icc"
                                          << int(ColorSpaces::DisplayP3);
        QTest::newRow("Apple ROMM") << d + "ROMM RGB.icc" << int(ColorSpaces::ProPhotoRGB);
        QTest::newRow("Apple ITU-2020") << d + "ITU-2020.icc" << int(ColorSpaces::Rec2020);
        QTest::newRow("Apple DCI-P3") << d + "DCI(P3) RGB.icc"
                                      << int(ColorSpaces::OtherProfile);
    }

    void realVendorProfiles()
    {
        /*  Real profiles written by someone else's tools -- the colorant tolerance has to
            hold for them, not just for lcms's own. macOS only; elsewhere skipped. */
        QFETCH(QString, path);
        QFETCH(int, id);
        QFile f(path);
        if (!f.open(QIODevice::ReadOnly)) QSKIP("system profile not present");
        QCOMPARE(ColorSpaces::fromIcc(f.readAll()), id);
    }

    void exifAndNclx()
    {
        QCOMPARE(ColorSpaces::fromExif(1), int(ColorSpaces::sRGB));
        QCOMPARE(ColorSpaces::fromExif(2), int(ColorSpaces::AdobeRGB));
        QCOMPARE(ColorSpaces::fromExif(0xFFFF), int(ColorSpaces::Uncalibrated));
        QCOMPARE(ColorSpaces::fromExif(-1), int(ColorSpaces::None));
        QCOMPARE(ColorSpaces::fromNclx(1), int(ColorSpaces::sRGB));
        QCOMPARE(ColorSpaces::fromNclx(12), int(ColorSpaces::DisplayP3));
        QCOMPARE(ColorSpaces::fromNclx(9), int(ColorSpaces::Rec2020));
        QCOMPARE(ColorSpaces::fromNclx(11), int(ColorSpaces::OtherProfile));
        QCOMPARE(ColorSpaces::fromNclx(2), int(ColorSpaces::None));
        QCOMPARE(ColorSpaces::fromDngPreview(2), int(ColorSpaces::sRGB));
        QCOMPARE(ColorSpaces::fromDngPreview(3), int(ColorSpaces::AdobeRGB));
        QCOMPARE(ColorSpaces::fromDngPreview(4), int(ColorSpaces::ProPhotoRGB));
        QCOMPARE(ColorSpaces::fromDngPreview(1), int(ColorSpaces::Gray));
        QCOMPARE(ColorSpaces::fromDngPreview(-1), int(ColorSpaces::None));
    }
};

QTEST_MAIN(TstColorSpaces)
#include "tst_colorspaces.moc"
