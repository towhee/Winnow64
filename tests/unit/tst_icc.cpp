#include <QtTest>
#include <QTemporaryDir>
#include <QRandomGenerator>

#include "Utilities/icc.h"
#include "Main/global.h"

/*
    ICC::transform is on every decoded image's path, so it was rebuilt for speed
    (2026-10-26): cached transforms, an identity skip, and a folded lookup kernel for
    matrix/TRC profiles. These pin that it still produces what lcms produces -- within
    one level, the kernel's measured error -- on the fast path, on the lcms fallback (a
    CLUT source profile), and for an embedded wide-gamut source; that an sRGB display
    and a missing display profile cost nothing; and that alpha and format survive.
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

cmsHPROFILE rgbProfile(const cmsCIExyYTRIPLE &prim, double gamma)
{
    const cmsCIExyY d65 = {0.3127, 0.3290, 1.0};
    cmsToneCurve *c;
    if (gamma > 0) {
        c = cmsBuildGamma(nullptr, gamma);
    }
    else {
        const cmsFloat64Number srgb[5] = {2.4, 1.0 / 1.055, 0.055 / 1.055, 1.0 / 12.92,
                                          0.04045};
        c = cmsBuildParametricToneCurve(nullptr, 4, srgb);
    }
    cmsToneCurve *t[3] = {c, c, c};
    cmsHPROFILE p = cmsCreateRGBProfile(&d65, &prim, t);
    cmsFreeToneCurve(c);
    return p;
}

const cmsCIExyYTRIPLE kP3 = {{0.680, 0.320, 1.0}, {0.265, 0.690, 1.0}, {0.150, 0.060, 1.0}};
const cmsCIExyYTRIPLE kAdobe = {{0.64, 0.33, 1.0}, {0.21, 0.71, 1.0}, {0.15, 0.06, 1.0}};

/* An sRGB-like INPUT profile whose A2B0 is a CLUT, so lcms (and therefore transform())
   must take the table, not the matrix -- the kernel's fallback case. */
cmsInt32Number sampleSrgbToLab(const cmsUInt16Number in[], cmsUInt16Number out[], void *c)
{
    cmsDoTransform(static_cast<cmsHTRANSFORM>(c), in, out, 1);
    return TRUE;
}

QByteArray clutProfile()
{
    cmsHPROFILE srgb = cmsCreate_sRGBProfile();
    cmsHPROFILE lab = cmsCreateLab4Profile(nullptr);
    cmsHTRANSFORM toLab = cmsCreateTransform(srgb, TYPE_RGB_16, lab, TYPE_Lab_16,
                                             INTENT_RELATIVE_COLORIMETRIC, 0);
    cmsPipeline *pipe = cmsPipelineAlloc(nullptr, 3, 3);
    cmsStage *clut = cmsStageAllocCLut16bit(nullptr, 33, 3, 3, nullptr);
    cmsStageSampleCLut16bit(clut, sampleSrgbToLab, toLab, 0);
    /* An A2B tag is curves -> CLUT -> curves; a bare CLUT does not survive the save. */
    cmsPipelineInsertStage(pipe, cmsAT_END, cmsStageAllocToneCurves(nullptr, 3, nullptr));
    cmsPipelineInsertStage(pipe, cmsAT_END, clut);
    cmsPipelineInsertStage(pipe, cmsAT_END, cmsStageAllocToneCurves(nullptr, 3, nullptr));
    cmsSetPCS(srgb, cmsSigLabData);
    cmsWriteTag(srgb, cmsSigAToB0Tag, pipe);
    cmsWriteTag(srgb, cmsSigAToB1Tag, pipe);
    cmsPipelineFree(pipe);
    cmsDeleteTransform(toLab);
    cmsCloseProfile(lab);
    return bytesOf(srgb);
}

QImage randomImage(int w, int h, QImage::Format f, quint32 seed)
{
    QImage im(w, h, QImage::Format_ARGB32);
    QRandomGenerator rng(seed);
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x)
            im.setPixel(x, y, qRgba(rng.bounded(256), rng.bounded(256), rng.bounded(256),
                                    255));
    return im.convertToFormat(f);
}

/* What transform() must reproduce: a fresh lcms transform, exactly as the old code. */
QImage reference(const QByteArray &src, const QString &displayPath, QImage im)
{
    cmsHPROFILE in = src.isEmpty()
        ? cmsCreate_sRGBProfile()
        : cmsOpenProfileFromMem(src.constData(), cmsUInt32Number(src.size()));
    cmsHPROFILE out = cmsOpenProfileFromFile(QFile::encodeName(displayPath).constData(), "r");
    cmsHTRANSFORM t = cmsCreateTransform(in, TYPE_BGRA_8, out, TYPE_BGRA_8,
                                         INTENT_PERCEPTUAL, 0);
    for (int y = 0; y < im.height(); ++y)
        cmsDoTransform(t, im.constScanLine(y), im.scanLine(y), cmsUInt32Number(im.width()));
    cmsDeleteTransform(t);
    cmsCloseProfile(in);
    cmsCloseProfile(out);
    return im;
}

int worstDiff(const QImage &a, const QImage &b)
{
    int worst = 0;
    for (int y = 0; y < a.height(); ++y)
        for (int x = 0; x < a.width(); ++x) {
            const QRgb p = a.pixel(x, y), q = b.pixel(x, y);
            worst = std::max({worst, std::abs(qRed(p) - qRed(q)),
                              std::abs(qGreen(p) - qGreen(q)),
                              std::abs(qBlue(p) - qBlue(q))});
        }
    return worst;
}

} // namespace

class TstIcc : public QObject
{
    Q_OBJECT

    QTemporaryDir dir;
    QString p3, srgb;

    void display(const QString &path)
    {
        G::winOutProfilePath = path;
        ICC::setOutProfile();
    }

private slots:
    void initTestCase()
    {
        QVERIFY(dir.isValid());
        p3 = dir.filePath("p3.icc");
        cmsSaveProfileToFile(rgbProfile(kP3, 0), QFile::encodeName(p3).constData());
        srgb = dir.filePath("srgb.icc");
        cmsHPROFILE s = cmsCreate_sRGBProfile();
        cmsSaveProfileToFile(s, QFile::encodeName(srgb).constData());
        cmsCloseProfile(s);
    }

    void noDisplayProfileIsANoOp()
    {
        display(QString());
        QCOMPARE(ICC::pathFor(QByteArray()), ICC::Path::None);
        QImage im = randomImage(9, 9, QImage::Format_ARGB32, 1);
        const QImage before = im.copy();
        ICC::transform(QByteArray(), im);
        QCOMPARE(im, before);
    }

    void srgbOnAnSrgbDisplayIsSkipped()
    {
        display(srgb);
        QCOMPARE(ICC::pathFor(QByteArray()), ICC::Path::Identity);
        QImage im = randomImage(9, 9, QImage::Format_ARGB32, 2);
        const QImage before = im.copy();
        ICC::transform(QByteArray(), im);
        QCOMPARE(im, before);
    }

    void fastPathMatchesLcms_data()
    {
        QTest::addColumn<QByteArray>("source");
        QTest::newRow("untagged (sRGB)") << QByteArray();
        QTest::newRow("embedded Adobe RGB") << bytesOf(rgbProfile(kAdobe, 563.0 / 256.0));
    }

    void fastPathMatchesLcms()
    {
        QFETCH(QByteArray, source);
        display(p3);
        QCOMPARE(ICC::pathFor(source), ICC::Path::Fast);
        /* Odd width, tall enough for many strips. */
        QImage im = randomImage(37, 333, QImage::Format_ARGB32, 3);
        const QImage want = reference(source, p3, im);
        ICC::transform(source, im);
        QVERIFY2(worstDiff(im, want) <= 1, qPrintable(QString::number(worstDiff(im, want))));
    }

    void clutSourceFallsBackToLcms()
    {
        display(p3);
        const QByteArray clut = clutProfile();
        QCOMPARE(ICC::pathFor(clut), ICC::Path::Lcms);
        QImage im = randomImage(37, 200, QImage::Format_ARGB32, 4);
        const QImage want = reference(clut, p3, im);
        ICC::transform(clut, im);
        QCOMPARE(worstDiff(im, want), 0);
    }

    void alphaAndFormatSurvive()
    {
        display(p3);
        QImage im(5, 5, QImage::Format_ARGB32);
        im.fill(QColor(200, 100, 50, 77));
        ICC::transform(QByteArray(), im);
        QCOMPARE(qAlpha(im.pixel(2, 2)), 77);
        QVERIFY(qRed(im.pixel(2, 2)) != 200 || qGreen(im.pixel(2, 2)) != 100);

        /* Not 32-bit: converted, then transformed (embelexport passes a plain load). */
        QImage rgb = randomImage(11, 7, QImage::Format_RGB888, 5);
        const QImage want = reference(QByteArray(), p3,
                                      rgb.convertToFormat(QImage::Format_ARGB32));
        ICC::transform(QByteArray(), rgb);
        QCOMPARE(rgb.depth(), 32);
        QVERIFY(worstDiff(rgb, want) <= 1);
    }
};

QTEST_MAIN(TstIcc)
#include "tst_icc.moc"
