/*
    Greycard trial C1 + C2 -- Winnow Standard as a synthesised ColorMatrix-only DNG
    profile (CameraProfileStore::winnowStandard, Develop/greycard.h).

    WHY THIS TEST EXISTS. The trial is an A/B against the shipping path, so the first
    thing it must guarantee is that it is ABSENT when off: no profile, no key part, for
    every user who is not G::isRory whatever the stored settings say. Beyond that it
    pins what makes the comparison fair:
      - the StdA table pairs only with the D65 entry the render already uses;
      - the as-shot neutral still renders to (1,1,1), so turning the trial on re-points
        color without moving exposure;
      - at D65 the dual-illuminant profile IS the single-illuminant one (weight 1);
      - a DNG keeps only its own ColorMatrix pair -- no ForwardMatrix, no tables.
*/

#include <QtTest>
#include <cmath>

#include "Develop/cameraprofile.h"
#include "Develop/cameraprofilestore.h"
#include "Develop/editparams.h"
#include "Develop/greycard.h"
#include "Develop/whitebalance.h"
#include "Develop/workingimage.h"
#include "ImageFormats/Raw/cameramatrix.h"

class TstGreycardStandard : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void cleanup();
    void offMeansAbsent();
    void stdATablePairsWithTheD65Entry();
    void asShotNeutralRendersNeutral_data();
    void asShotNeutralRendersNeutral();
    void dualEqualsSingleAtD65();
    void dualDiffersUnderTungsten();
    void bodyWithoutPairFallsBackToSingle();
    void dngKeepsOnlyItsColorMatrices();
    void resolveRoutesWinnowStandard();
    void holdShowsShipping();
    void toolArgs();

private:
    bool savedRory = false;
    G::Greycard savedGreycard;

    /* A camera characterised the way a decoder does it: the table's D65 matrix, and an
       as-shot balance for a daylight-ish white solved through that same matrix. */
    static CameraColor sonyCam(const QString &model = QStringLiteral("Sony ILCE-1"),
                               float asShotKelvin = 5200.0f);
    static void neutralOf(const float xyzToCam[3][3], float kelvin, float tint, double n[3]);
    static void trialOn(G::Greycard::StandardColor sc)
    {
        G::isRory = true;
        G::greycard.enabled = true;
        G::greycard.standardColor = sc;
    }
};

void TstGreycardStandard::init()
{
    savedRory = G::isRory;
    savedGreycard = G::greycard;
}

void TstGreycardStandard::cleanup()
{
    G::isRory = savedRory;
    G::greycard = savedGreycard;
    G::Greycard::holdA.store(false);
}

void TstGreycardStandard::neutralOf(const float xyzToCam[3][3], float kelvin, float tint,
                                    double n[3])
{
    double xyz[3];
    QVERIFY(WhiteBalance::illuminantXYZ(kelvin, tint, xyz));
    for (int i = 0; i < 3; ++i)
        n[i] = xyzToCam[i][0] * xyz[0] + xyzToCam[i][1] * xyz[1] + xyzToCam[i][2] * xyz[2];
    for (int i : {0, 2}) n[i] /= n[1];
    n[1] = 1.0;
}

CameraColor TstGreycardStandard::sonyCam(const QString &model, float asShotKelvin)
{
    CameraColor cam;
    cam.valid = true;
    cam.cameraModel = model;
    if (!xyzToCamForModel(model, cam.xyzToCam)) return cam;
    double n[3];
    neutralOf(cam.xyzToCam, asShotKelvin, 0.0f, n);
    for (int i = 0; i < 3; ++i) cam.asShotMul[i] = float(1.0 / n[i]);
    return cam;
}

void TstGreycardStandard::offMeansAbsent()
{
    const CameraColor cam = sonyCam();

    /* Enabled in settings, but not the Rory build: nothing. */
    G::isRory = false;
    G::greycard.enabled = true;
    G::greycard.standardColor = G::Greycard::StandardColor::DualIlluminant;
    QVERIFY(!G::greycard.active());
    QVERIFY(G::greycard.key().isEmpty());
    QVERIFY(CameraProfileStore::winnowStandard(cam) == nullptr);

    /* The Rory build, master switch off: nothing. */
    G::isRory = true;
    G::greycard.enabled = false;
    QVERIFY(G::greycard.key().isEmpty());
    QVERIFY(CameraProfileStore::winnowStandard(cam) == nullptr);

    /* On, but the selector says Off: no profile, though the key says a trial is on. */
    G::greycard.enabled = true;
    G::greycard.standardColor = G::Greycard::StandardColor::Off;
    QVERIFY(CameraProfileStore::winnowStandard(cam) == nullptr);
    QVERIFY(!G::greycard.key().isEmpty());
}

void TstGreycardStandard::stdATablePairsWithTheD65Entry()
{
    /* Both tables answer for the same KEY, chosen by cameramatrix.cpp's longest-prefix
       rule: the A1 II has its own row and must not borrow the A1's partner. */
    QCOMPARE(QString(cameraMatrixKey("Sony ILCE-1")), QString("Sony ILCE-1"));
    QCOMPARE(QString(cameraMatrixKey("Sony ILCE-1M2")), QString("Sony ILCE-1M2"));
    QVERIFY(cameraMatrixKey("Nosuchmaker X1") == nullptr);
    float a1[3][3], a1m2[3][3];
    QVERIFY(xyzToCamStdAForModel("Sony ILCE-1", a1));
    if (xyzToCamStdAForModel("Sony ILCE-1M2", a1m2))
        QVERIFY(a1[0][0] != a1m2[0][0]);

    float a[3][3], d65[3][3];
    QVERIFY(xyzToCamStdAForModel("Sony ILCE-1", a));
    QVERIFY(xyzToCamForModel("Sony ILCE-1", d65));
    /* rawler's published StdA for the A1 (rawler/data/cameras/sony/a1.toml). */
    QCOMPARE(a[0][0], 0.8933f);
    QCOMPARE(a[2][2], 0.7616f);
    /* A tungsten matrix is not the daylight one. */
    QVERIFY(std::fabs(a[0][1] - d65[0][1]) > 0.05f);

    /* A model with no matrix at all has no partner either. */
    QVERIFY(!xyzToCamStdAForModel("Nosuchmaker X1", a));
}

void TstGreycardStandard::asShotNeutralRendersNeutral_data()
{
    QTest::addColumn<int>("mode");
    QTest::addColumn<float>("asShotK");
    QTest::newRow("camera WB, daylight") << 1 << 5200.0f;
    QTest::newRow("camera WB, tungsten") << 1 << 3000.0f;
    QTest::newRow("dual, daylight")      << 2 << 5200.0f;
    QTest::newRow("dual, tungsten")      << 2 << 3000.0f;
    QTest::newRow("dual, shade")         << 2 << 7500.0f;
}

void TstGreycardStandard::asShotNeutralRendersNeutral()
{
/*
    The exposure anchor, end to end the way the render does it: re-solve as shot against
    the synthesised profile (WorkingImageCache::AttachProfile), build the matrix at that
    white (Develop::InputMatrixUnexposed via WhiteBalance::resolve at temp 0), and apply
    it to the file's own as-shot neutral. It must land on (1,1,1): the trial may move
    color, never brightness or the neutral axis.
*/
    QFETCH(int, mode);
    QFETCH(float, asShotK);
    trialOn(static_cast<G::Greycard::StandardColor>(mode));

    CameraColor cam = sonyCam(QStringLiteral("Sony ILCE-1"), asShotK);
    const auto prof = CameraProfileStore::winnowStandard(cam);
    QVERIFY(prof);
    QCOMPARE(prof->dual(), mode == 2);
    QVERIFY(prof->cal[0].haveForward == false && prof->lookTable.isEmpty());

    QVERIFY(CameraProfile::resolveAsShot(*prof, cam));
    float kelvin = 0.0f, tint = 0.0f;
    WhiteBalance::resolve(cam, 0.0f, 0.0f, kelvin, tint);
    float m[3][3];
    QVERIFY(CameraProfile::camToWorking(*prof, kelvin, tint, m));

    const double n[3] = {1.0 / cam.asShotMul[0], 1.0, 1.0 / cam.asShotMul[2]};
    for (int i = 0; i < 3; ++i) {
        const double out = m[i][0] * n[0] + m[i][1] * n[1] + m[i][2] * n[2];
        QVERIFY2(std::fabs(out - 1.0) < 2e-3,
                 qPrintable(QString("channel %1 = %2 at %3 K").arg(i).arg(out).arg(kelvin)));
    }
}

void TstGreycardStandard::dualEqualsSingleAtD65()
{
    const CameraColor cam = sonyCam();
    trialOn(G::Greycard::StandardColor::CameraWb);
    const auto single = CameraProfileStore::winnowStandard(cam);
    trialOn(G::Greycard::StandardColor::DualIlluminant);
    const auto dual = CameraProfileStore::winnowStandard(cam);
    QVERIFY(single && dual && dual->dual() && !single->dual());

    float ms[3][3], md[3][3];
    QVERIFY(CameraProfile::camToWorking(*single, 6504.0f, 0.0f, ms));
    QVERIFY(CameraProfile::camToWorking(*dual, 6504.0f, 0.0f, md));
    for (int i = 0; i < 3; ++i)
        for (int j = 0; j < 3; ++j)
            QVERIFY2(std::fabs(ms[i][j] - md[i][j]) < 1e-4f,
                     qPrintable(QString("[%1][%2] %3 vs %4")
                                    .arg(i).arg(j).arg(ms[i][j]).arg(md[i][j])));
}

void TstGreycardStandard::dualDiffersUnderTungsten()
{
    /* The point of C1: under tungsten the matrix is a different one. */
    const CameraColor cam = sonyCam();
    trialOn(G::Greycard::StandardColor::CameraWb);
    const auto single = CameraProfileStore::winnowStandard(cam);
    trialOn(G::Greycard::StandardColor::DualIlluminant);
    const auto dual = CameraProfileStore::winnowStandard(cam);

    float ms[3][3], md[3][3];
    QVERIFY(CameraProfile::camToWorking(*single, 2856.0f, 0.0f, ms));
    QVERIFY(CameraProfile::camToWorking(*dual, 2856.0f, 0.0f, md));
    double maxDiff = 0.0;
    for (int i = 0; i < 3; ++i)
        for (int j = 0; j < 3; ++j)
            maxDiff = std::max(maxDiff, double(std::fabs(ms[i][j] - md[i][j])));
    QVERIFY2(maxDiff > 0.02, qPrintable(QString("max diff %1").arg(maxDiff)));
}

void TstGreycardStandard::bodyWithoutPairFallsBackToSingle()
{
    /* The decoder used a matrix that is NOT the table's (a file-supplied one): its StdA
       partner would belong to a different D65, so dual mode must not pair them. */
    CameraColor cam = sonyCam();
    cam.xyzToCam[0][0] += 0.05f;
    trialOn(G::Greycard::StandardColor::DualIlluminant);
    const auto p = CameraProfileStore::winnowStandard(cam);
    QVERIFY(p);
    QVERIFY(!p->dual());
    QCOMPARE(p->cal[0].illuminant, 21);
    QCOMPARE(p->cal[0].color.m[0][0], cam.xyzToCam[0][0]);

    /* No matrix for the model at all: nothing to characterise, shipping path. */
    CameraColor none;
    none.valid = true;
    none.cameraModel = QStringLiteral("Nosuchmaker X1");
    QVERIFY(CameraProfileStore::winnowStandard(none) == nullptr);
}

void TstGreycardStandard::dngKeepsOnlyItsColorMatrices()
{
    auto e = std::make_shared<Dcp::Profile>();
    e->valid = true;
    e->analogBalance[0] = 1.5f;
    for (int c = 0; c < 2; ++c) {
        e->cal[c].illuminant = c == 0 ? 17 : 21;
        e->cal[c].haveColor = true;
        e->cal[c].color.m[0][0] = c == 0 ? 0.9f : 0.8f;
        e->cal[c].haveForward = true;
        e->cal[c].hueSatMap.hueDivs = 1;
        e->cal[c].hueSatMap.v = {0.0f, 1.0f, 1.0f};
    }
    e->lookTable.v = {0.0f, 1.0f, 1.0f};
    e->toneCurve = {0.0f, 0.0f, 1.0f, 1.0f};

    CameraColor cam = sonyCam();
    cam.embeddedProfile = e;

    trialOn(G::Greycard::StandardColor::DualIlluminant);
    auto p = CameraProfileStore::winnowStandard(cam);
    QVERIFY(p && p->dual());
    QCOMPARE(p->analogBalance[0], 1.5f);
    QCOMPARE(p->cal[0].color.m[0][0], 0.9f);
    QCOMPARE(p->cal[1].color.m[0][0], 0.8f);
    for (int c = 0; c < 2; ++c) {
        QVERIFY(!p->cal[c].haveForward);
        QVERIFY(p->cal[c].hueSatMap.isEmpty());
    }
    QVERIFY(p->lookTable.isEmpty());
    QVERIFY(p->toneCurve.empty());

    /* Single: ColorMatrix2, the one dng.cpp gives Winnow Standard. */
    trialOn(G::Greycard::StandardColor::CameraWb);
    p = CameraProfileStore::winnowStandard(cam);
    QVERIFY(p && !p->dual());
    QCOMPARE(p->cal[0].illuminant, 21);
    QCOMPARE(p->cal[0].color.m[0][0], 0.8f);
}

void TstGreycardStandard::resolveRoutesWinnowStandard()
{
    const CameraColor cam = sonyCam();
    const QString builtIn = QLatin1String(EditParams::kBuiltInProfile);

    G::isRory = false;
    QVERIFY(CameraProfileStore::instance().resolve(cam, builtIn) == nullptr);
    QVERIFY(CameraProfileStore::instance().resolve(cam, QString()) == nullptr);

    trialOn(G::Greycard::StandardColor::DualIlluminant);
    QVERIFY(CameraProfileStore::instance().resolve(cam, builtIn) != nullptr);
    QVERIFY(CameraProfileStore::instance().resolve(cam, QString()) != nullptr);
    /* A name that no longer resolves renders as Winnow Standard -- the trial's. */
    QVERIFY(CameraProfileStore::instance().resolve(cam, "No Such Profile") != nullptr);
}

void TstGreycardStandard::holdShowsShipping()
{
/*
    Hold-to-compare (MW::setGreycardHold): while held, every trial is A -- no profile, no
    key part, so the frame is the shipping render AND would key as one -- but the trials
    still count as on for the badge and the key handler.
*/
    const CameraColor cam = sonyCam();
    trialOn(G::Greycard::StandardColor::DualIlluminant);
    const QString keyB = G::greycard.key();
    QVERIFY(!keyB.isEmpty());
    QVERIFY(CameraProfileStore::winnowStandard(cam) != nullptr);

    G::Greycard::holdA.store(true);
    QVERIFY(G::greycard.trialsOn());
    QVERIFY(!G::greycard.active());
    QVERIFY(G::greycard.key().isEmpty());
    QCOMPARE(int(G::greycard.effectiveStandardColor()), int(G::Greycard::StandardColor::Off));
    QVERIFY(CameraProfileStore::winnowStandard(cam) == nullptr);

    G::Greycard::holdA.store(false);
    QCOMPARE(G::greycard.key(), keyB);
}

void TstGreycardStandard::toolArgs()
{
    /* The headless tools are not the Rory build: greycard=1 is their opt-in. */
    G::isRory = false;
    G::Greycard g;
    QString err;
    bool input = true;
    QCOMPARE(G::parseToolArg(g, "exposure", "1", err, input), 0);
    QVERIFY(!input);
    QVERIFY(!g.active());

    QCOMPARE(G::parseToolArg(g, "greycard", "1", err, input), 1);
    QVERIFY(g.active());
    QVERIFY(!g.trialsOn());             // no badge, no hold key: not the app

    QCOMPARE(G::parseToolArg(g, "gc.sc", "camwb", err, input), 1);
    QVERIFY(input);                     // C1/C2 act before a .wkg dump is written
    QCOMPARE(int(g.standardColor), int(G::Greycard::StandardColor::CameraWb));
    QCOMPARE(G::parseToolArg(g, "gc.sc", "2", err, input), 1);
    QCOMPARE(int(g.standardColor), int(G::Greycard::StandardColor::DualIlluminant));
    QCOMPARE(G::parseToolArg(g, "gc.sc", "OFF", err, input), 1);
    QCOMPARE(int(g.standardColor), int(G::Greycard::StandardColor::Off));

    QCOMPARE(G::parseToolArg(g, "gc.sc", "bogus", err, input), -1);
    QVERIFY(err.contains("gc.sc"));
    QCOMPARE(G::parseToolArg(g, "gc.nosuch", "1", err, input), -1);

    QCOMPARE(G::parseToolArg(g, "greycard", "yes", err, input), -1);
    QCOMPARE(G::parseToolArg(g, "greycard", "0", err, input), 1);
    QVERIFY(!g.active());
}

QTEST_GUILESS_MAIN(TstGreycardStandard)
#include "tst_greycardstandard.moc"
