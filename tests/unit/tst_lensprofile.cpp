/*
    tst_lensprofile -- lens profile correction (Develop/lensdb.*, Develop/lensprofile.h)
    against liblensfun itself.

    lensprofile_oracle.h holds lensfun's own answers for five shots (ptlens / poly3 /
    poly5 distortion, poly3 TCA, pa vignetting; a full-frame lens on an APS-C body; a
    portrait frame; a fixed-lens compact), produced by tests/fixtures/lensfun_oracle.py.
    Winnow reimplements the models from their formulas, so this is the proof that it
    lands on the same pixels: the database loads the same cameras and lenses, the EXIF
    match picks the same lens, the interpolated terms agree, and every sampled output
    pixel maps to the same photo position and devignetting gain.
*/

#include <QtTest>

#include "Develop/lensdb.h"
#include "Develop/lensprofile.h"
#include "lensprofile_oracle.h"

using namespace LensDb;
using namespace LensProfile;

class TestLensProfile : public QObject
{
    Q_OBJECT

private slots:
    void databaseLoads();
    void words();
    void oracleMatchAndCoefficients();
    void oracleGeometry();
    void oracleVignetting();
    void autoScaleFillsFrame();
    void noMatchCases();
    void smallerSensorRefused();
    void regionMatchesWholeFrame();
    void identityLeavesPixels();
    void linearTca();
    void amountsScale();
    void manualWithoutProfile();
};

void TestLensProfile::databaseLoads()
{
    QElapsedTimer t;
    t.start();
    const Database &db = Database::instance();
    qInfo() << "lens database:" << db.cameraCount() << "cameras," << db.lensCount()
            << "lenses," << t.elapsed() << "ms";
    /* lensfun 0.3.4 loads the same files to 1057 cameras and 1571 lenses. */
    QCOMPARE(db.cameraCount(), 1057);
    QCOMPARE(db.lensCount(), 1571);
}

void TestLensProfile::words()
{
    /* The example from lensfun's manual. */
    const QStringList w = Database::words("smc PENTAX-DA 12-24mm F4 ED AL [IF]");
    QCOMPARE(w, QStringList({"smc", "pentax", "da", "12", "24", "mm", "4", "ed", "al", "if"}));
    QCOMPARE(Database::words("AF-S NIKKOR 24-70mm f/2.8G ED"),
             QStringList({"af", "s", "nikkor", "24", "70", "mm", "2", "8", "g", "ed"}));
}

static Shot shotOf(const OracleCase &c)
{
    Shot s;
    s.cameraMake = c.make;
    s.cameraModel = c.model;
    s.lens = c.lens;
    s.focal = c.focal;
    s.aperture = c.aperture;
    s.distance = c.distance;
    return s;
}

void TestLensProfile::oracleMatchAndCoefficients()
{
    for (const OracleCase &c : kOracle) {
        const Resolved r = resolve(shotOf(c));
        QVERIFY2(r.camera, c.model);
        QCOMPARE(r.camera->cropFactor, c.cameraCrop);
        QVERIFY2(r.lens, c.lens);
        QCOMPARE(r.lens->model, QString(c.lensModel));
        QVERIFY2(r.ok(), qPrintable(r.reason));
        QCOMPARE(r.coeffs.hasDistortion(), c.hasDist);
        QCOMPARE(r.coeffs.hasTca(), c.hasTca);
        QCOMPARE(r.coeffs.vig, c.hasVig);
        for (int i = 0; i < 3; ++i)
            QVERIFY2(std::abs(r.coeffs.d[i] - c.dist[i]) < 2e-6f, c.lensModel);
        if (c.hasTca)
            for (int i = 0; i < 6; ++i)
                QVERIFY2(std::abs(r.coeffs.t[i] - c.tca[i]) < 2e-6f, c.lensModel);
        if (c.hasVig)
            for (int i = 0; i < 3; ++i)
                QVERIFY2(std::abs(r.coeffs.v[i] - c.vig[i]) < 2e-5f, c.lensModel);
    }
}

void TestLensProfile::oracleGeometry()
{
    double worst = 0.0;
    for (const OracleCase &c : kOracle) {
        const Resolved r = resolve(shotOf(c));
        QVERIFY(r.ok());
        const Frame f(r.coeffs, c.W, c.H);
        const Amounts a;
        for (int i = 0; i < c.nGeom; ++i) {
            const OracleGeom &g = c.geom[i];
            const Lookup L = lookup(r.coeffs, a, f, g.x - f.cx, g.y - f.cy, 1.0);
            for (int ch = 0; ch < 3; ++ch) {
                const double e = std::max(std::abs(L.sx[ch] - g.s[ch][0]),
                                          std::abs(L.sy[ch] - g.s[ch][1]));
                worst = std::max(worst, e);
                if (e > 0.01)
                    qWarning() << c.lensModel << g.x << g.y << ch << "winnow"
                               << L.sx[ch] << L.sy[ch] << "lensfun" << g.s[ch][0]
                               << g.s[ch][1];
                QVERIFY(e < 0.01);       // a hundredth of a pixel
            }
        }
    }
    qInfo() << "worst geometry difference vs lensfun:" << worst << "px";
}

void TestLensProfile::oracleVignetting()
{
    for (const OracleCase &c : kOracle) {
        if (!c.hasVig) continue;
        const Resolved r = resolve(shotOf(c));
        const Frame f(r.coeffs, c.W, c.H);
        for (int i = 0; i < c.nGain; ++i) {
            const OracleVig &v = c.gain[i];
            const double g = vignettingGain(r.coeffs,
                                            std::hypot(v.x - f.cx, v.y - f.cy) * f.pxToVig);
            QVERIFY2(std::abs(g - v.gain) < 1e-3 * v.gain, c.lensModel);
        }
    }
}

void TestLensProfile::autoScaleFillsFrame()
{
    for (const OracleCase &c : kOracle) {
        const Resolved r = resolve(shotOf(c));
        const Amounts a;
        const double s = autoScale(r.coeffs, a, c.W, c.H);
        QVERIFY(s > 0.5 && s < 3.0);
        const Frame f(r.coeffs, c.W, c.H);
        /* Every border pixel samples inside the photo. */
        for (int x = 0; x < c.W; x += 7)
            for (int y : {0, c.H - 1}) {
                const Lookup L = lookup(r.coeffs, a, f, x - f.cx, y - f.cy, 1.0 / s);
                for (int ch = 0; ch < 3; ++ch) {
                    QVERIFY(L.sx[ch] > -0.3 && L.sx[ch] < c.W - 0.7);
                    QVERIFY(L.sy[ch] > -0.3 && L.sy[ch] < c.H - 0.7);
                }
            }
    }
}

void TestLensProfile::noMatchCases()
{
    Shot s;
    s.cameraMake = "Nikon Corporation";
    s.cameraModel = "Nikon D810";
    s.focal = 35;
    s.aperture = 5.6f;
    s.lens = "Frobnitz Wondercam 123";
    QVERIFY(!resolve(s).lens);
    /* Same words, wrong aperture: numbers must agree exactly. */
    s.lens = "AF-S NIKKOR 24-70mm f/3.5G ED";
    QVERIFY(!resolve(s).lens);
    s.lens.clear();
    QCOMPARE(resolve(s).reason, QString("no lens in EXIF"));
    /* Outside the zoom range. */
    s.lens = "AF-S NIKKOR 24-70mm f/2.8G ED";
    s.focal = 200;
    QVERIFY(!resolve(s).lens);
}

void TestLensProfile::smallerSensorRefused()
{
    /* A DX (APS-C) lens chosen by hand on a full-frame body: lensfun will not stretch a
       calibration past the image circle it was measured on. */
    const Database &db = Database::instance();
    const Lens *dx = nullptr;
    for (const Lens *L : db.lensesFor(nullptr))
        if (L->cropFactor > 1.4f && L->mounts.contains("Nikon F AF")) { dx = L; break; }
    QVERIFY(dx);
    Shot s;
    s.cameraMake = "Nikon Corporation";
    s.cameraModel = "Nikon D810";
    s.focal = dx->minFocal;
    s.aperture = 5.6f;
    const Resolved r = resolve(s, dx->id());
    QVERIFY(!r.ok());
    QCOMPARE(r.reason, QString("profiled on a smaller sensor"));
}

static std::vector<float> testImage(int W, int H)
{
    std::vector<float> img(size_t(W) * H * 3);
    for (int y = 0; y < H; ++y)
        for (int x = 0; x < W; ++x) {
            float *p = &img[(size_t(y) * W + x) * 3];
            p[0] = 0.5f + 0.4f * std::sin(x * 0.13f) * std::cos(y * 0.07f);
            p[1] = 0.5f + 0.4f * std::sin(x * 0.05f + y * 0.11f);
            p[2] = float(x + y) / float(W + H);
        }
    return img;
}

void TestLensProfile::regionMatchesWholeFrame()
{
    const OracleCase &c = kOracle[0];
    const Resolved r = resolve(shotOf(c));
    const Amounts a;
    const int W = c.W, H = c.H;
    const std::vector<float> src = testImage(W, H);
    const double s = autoScale(r.coeffs, a, W, H);
    std::vector<float> whole(src.size());
    applyRegion(src.data(), W, H, whole.data(), 0, 0, W, H, r.coeffs, a, s);
    const int rx = 37, ry = 211, rw = 90, rh = 64;
    std::vector<float> part(size_t(rw) * rh * 3);
    applyRegion(src.data(), W, H, part.data(), rx, ry, rw, rh, r.coeffs, a, s);
    for (int y = 0; y < rh; ++y)
        for (int x = 0; x < rw; ++x)
            for (int ch = 0; ch < 3; ++ch)
                QCOMPARE(part[(size_t(y) * rw + x) * 3 + ch],
                         whole[(size_t(y + ry) * W + x + rx) * 3 + ch]);
}

void TestLensProfile::identityLeavesPixels()
{
    const int W = 64, H = 48;
    const std::vector<float> src = testImage(W, H);
    std::vector<float> dst(src.size());
    Coeffs k;                                   // no model at all
    k.dist = DistModel::Poly5;                  // ... or a model with zero terms
    applyRegion(src.data(), W, H, dst.data(), 0, 0, W, H, k, Amounts(), 1.0);
    for (size_t i = 0; i < src.size(); ++i) QCOMPARE(dst[i], src[i]);
}

void TestLensProfile::linearTca()
{
    Coeffs k;
    k.tca = TcaModel::Linear;
    k.t[0] = 1.002f;                            // red imaged 0.2% larger
    k.t[1] = 0.999f;
    const int W = 600, H = 400;
    const Frame f(k, W, H);
    const Lookup L = lookup(k, Amounts(), f, 250.0, 150.0, 1.0);
    QVERIFY(std::abs((L.sx[0] - f.cx) - 250.0 * double(k.t[0])) < 1e-9);
    QVERIFY(std::abs((L.sx[2] - f.cx) - 250.0 * double(k.t[1])) < 1e-9);
    QVERIFY(std::abs((L.sx[1] - f.cx) - 250.0) < 1e-9);
    Amounts off;
    off.tca = false;
    const Lookup L0 = lookup(k, off, f, 250.0, 150.0, 1.0);
    QCOMPARE(L0.sx[0], L0.sx[1]);
}

void TestLensProfile::amountsScale()
{
    Coeffs k;
    k.dist = DistModel::Poly5;
    k.d[0] = -0.05f;
    QCOMPARE(distortionFactor(k, 0.8, 0.0), 1.0);
    const double full = distortionFactor(k, 0.8, 1.0) - 1.0;
    QVERIFY(std::abs((distortionFactor(k, 0.8, 2.0) - 1.0) - 2.0 * full) < 1e-12);
    k.vig = true;
    k.v[0] = -0.3f;
    const double g = vignettingGain(k, 0.9);
    QVERIFY(g > 1.0);
    QCOMPARE(vignettingGain(k, 0.9, 0.0), 1.0);
    QVERIFY(std::abs(vignettingGain(k, 0.9, 2.0) - g * g) < 1e-9);
}

/* The Manual corrections need no profile: an empty Coeffs plus manual amounts is active,
   corrects by the documented formulas, auto-scales to fill, and is identity at zero. */
void TestLensProfile::manualWithoutProfile()
{
    const Coeffs k;                             // no profile at all
    const int W = 600, H = 400;
    const Frame f(k, W, H);
    QVERIFY(!active(k, Amounts()));

    /* Distortion: + corrects barrel -- the corner samples INSIDE its own radius. */
    Amounts a;
    a.manualDistortion = 1.0f;
    QVERIFY(geometryActive(k, a));
    const double cdx = (W - 1) / 2.0, cdy = (H - 1) / 2.0;      // the corner, rn = 1
    const Lookup L = lookup(k, a, f, cdx, cdy, 1.0);
    QVERIFY(std::abs((L.sx[1] - f.cx) - cdx * (1.0 - kManualDistK)) < 1e-9);
    QCOMPARE(L.sx[0], L.sx[1]);                 // no TCA: one position for all channels
    /* Barrel correction keeps every border sample inside at s = 1, so the fill zoom
       goes BELOW 1 (less of the photo thrown away); pincushion needs a zoom-in. */
    QVERIFY(autoScale(k, a, W, H) < 1.0);
    a.manualDistortion = -1.0f;
    QVERIFY(autoScale(k, a, W, H) > 1.0);

    /* Vignetting: centre untouched, corner gets the full kManualVigEV; a higher
       midpoint confines it to the corners. */
    Amounts v;
    v.manualVignetting = 0.5f;
    QVERIFY(vignettingActive(k, v) && active(k, v));
    QVERIFY(!geometryActive(k, v));
    QCOMPARE(totalVignettingGain(k, v, f, 0.0, 0.0), 1.0);
    QVERIFY(std::abs(totalVignettingGain(k, v, f, cdx, cdy)
                     - std::exp2(0.5 * kManualVigEV)) < 1e-9);
    Amounts lo = v, hi = v;
    lo.manualMidpoint = 0.0f;
    hi.manualMidpoint = 1.0f;
    QVERIFY(totalVignettingGain(k, lo, f, cdx / 2, cdy / 2)
            > totalVignettingGain(k, hi, f, cdx / 2, cdy / 2));

    /* And the pixels: a + manual vignetting brightens a corner, leaves the centre. */
    const std::vector<float> src = testImage(W, H);
    std::vector<float> dst(src.size());
    applyRegion(src.data(), W, H, dst.data(), 0, 0, W, H, k, v, 1.0);
    const size_t corner = 0, centre = (size_t(H / 2) * W + W / 2) * 3;
    QVERIFY(dst[corner] > src[corner] || src[corner] == 0.0f);
    QVERIFY(std::abs(dst[centre] - src[centre]) <= std::abs(src[centre]) * 1e-3f);
}

QTEST_GUILESS_MAIN(TestLensProfile)
#include "tst_lensprofile.moc"
