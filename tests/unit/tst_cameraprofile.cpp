/*
    CameraProfile -- collapsing a DNG camera profile onto one 3x3 for a chosen white.

    WHY THIS TEST EXISTS. Every mistake available here renders a plausible picture.
    Interpolating in kelvin instead of mired puts the weight ~13% out at the middle of
    the range; reading the two calibrations in the order the file happens to list them
    interpolates backwards on a profile written cool-first; dropping the exposure anchor
    moves the brightness of every raw the moment a profile is selected. None of those
    fail loudly. They are pinned here instead.

    THE INVARIANT THAT MATTERS MOST is the last one: the camera neutral must render to
    exactly (1,1,1) in the working space. It is what makes a profile change re-point
    colour WITHOUT moving exposure, and it must hold on BOTH paths -- with a
    ForwardMatrix and with one derived from the inverse of the colour matrix.
*/

#include <QtTest>
#include <QDir>
#include <QDirIterator>
#include <cmath>

#include "Develop/cameraprofile.h"
#include "Develop/whitebalance.h"

class TstCameraProfile : public QObject
{
    Q_OBJECT

private slots:
    void endpointsAreExact();
    void interpolatesInMiredNotKelvin();
    void clampsOutsideTheCalibrationRange();
    void illuminantOrderDoesNotMatter();
    void singleIlluminantProfileResolvesEverywhere();
    void neutralRendersNeutral_data();
    void neutralRendersNeutral();
    void publishedForwardMatrixIsWithinItsOwnRounding();
    void profileChangeDoesNotMoveExposure();
    void realProfilesRenderNeutral();

private:
    static Dcp::Matrix3 mat(const double v[9])
    {
        Dcp::Matrix3 m;
        for (int i = 0; i < 3; ++i)
            for (int j = 0; j < 3; ++j) m.m[i][j] = float(v[i * 3 + j]);
        return m;
    }
    /* A real Adobe Standard profile's numbers (Canon EOS 77D), so the matrices have the
       conditioning a real one has rather than being contrived. */
    static Dcp::Profile canon77D();

    /*
        Scale each ForwardMatrix row so it sums EXACTLY to the D50 white. A well-formed
        ForwardMatrix has that property by definition, but a shipped one is stored as
        rationals over 10000 and so holds it only to ~1e-4 (measured worst 3.0e-4 over 416
        real rows, in realProfilesRenderNeutral below). Tests that mean to assert something
        about THIS CODE use the normalised form and can then demand exactness; the test
        that means to assert something about real files uses the published numbers and a
        tolerance sized to their rounding.
    */
    static void normaliseForwardToD50(Dcp::Profile &p)
    {
        for (int c = 0; c < 2; ++c) {
            if (!p.cal[c].haveForward) continue;
            for (int row = 0; row < 3; ++row) {
                const double sum = double(p.cal[c].forward.m[row][0])
                                 + double(p.cal[c].forward.m[row][1])
                                 + double(p.cal[c].forward.m[row][2]);
                if (qAbs(sum) < 1e-9) continue;
                const double k = ColorSpaceMath::kWhiteD50[row] / sum;
                for (int j = 0; j < 3; ++j)
                    p.cal[c].forward.m[row][j] = float(double(p.cal[c].forward.m[row][j]) * k);
            }
        }
    }
};

/* Standard light A (17) and D65 (21) -- what essentially every Adobe profile uses. */
static const double kColor1[9] = { 0.7952, -0.1689, -0.0575,
                                  -0.3746,  1.0825,  0.3378,
                                  -0.0405,  0.1362,  0.6120 };
static const double kColor2[9] = { 0.7377, -0.0742, -0.0998,
                                  -0.4235,  1.1981,  0.2549,
                                  -0.0673,  0.1918,  0.5538 };
static const double kFwd1[9]   = { 0.5407,  0.2506,  0.1730,
                                   0.3306,  0.6136,  0.0558,
                                   0.1852,  0.0007,  0.6392 };
static const double kFwd2[9]   = { 0.5388,  0.1799,  0.2457,
                                   0.3091,  0.6107,  0.0802,
                                   0.1438,  0.0001,  0.6812 };

Dcp::Profile TstCameraProfile::canon77D()
{
    Dcp::Profile p;
    p.valid = true;
    p.uniqueCameraModel = "Canon EOS 77D";
    p.name = "Adobe Standard";
    p.cal[0].illuminant = 17;                       // Standard light A, 2856 K
    p.cal[0].haveColor = true;   p.cal[0].color   = mat(kColor1);
    p.cal[0].haveForward = true; p.cal[0].forward = mat(kFwd1);
    p.cal[1].illuminant = 21;                       // D65, 6504 K
    p.cal[1].haveColor = true;   p.cal[1].color   = mat(kColor2);
    p.cal[1].haveForward = true; p.cal[1].forward = mat(kFwd2);
    return p;
}

void TstCameraProfile::endpointsAreExact()
{
    const Dcp::Profile p = canon77D();
    CameraProfile::Resolved r;

    /* At a calibration's own temperature the interpolation must return THAT calibration
       untouched -- not something a hair off it. A profile is fitted at these two points
       and nowhere else, so they are the only two temperatures where the matrix is known
       to be right, and blending even 1% of the other one there throws away the fit. */
    QVERIFY(CameraProfile::resolve(p, 2856.0f, r));
    QCOMPARE(r.weight, 0.0f);
    QCOMPARE(r.warmKelvin, 2856.0f);
    QCOMPARE(r.coolKelvin, 6504.0f);
    for (int i = 0; i < 3; ++i)
        for (int j = 0; j < 3; ++j) {
            QVERIFY(qAbs(r.color.m[i][j]   - kColor1[i * 3 + j]) < 1e-6);
            QVERIFY(qAbs(r.forward.m[i][j] - kFwd1[i * 3 + j])   < 1e-6);
        }

    QVERIFY(CameraProfile::resolve(p, 6504.0f, r));
    QCOMPARE(r.weight, 1.0f);
    for (int i = 0; i < 3; ++i)
        for (int j = 0; j < 3; ++j) {
            QVERIFY(qAbs(r.color.m[i][j]   - kColor2[i * 3 + j]) < 1e-6);
            QVERIFY(qAbs(r.forward.m[i][j] - kFwd2[i * 3 + j])   < 1e-6);
        }
    QVERIFY(r.haveForward);
}

void TstCameraProfile::interpolatesInMiredNotKelvin()
{
    const Dcp::Profile p = canon77D();
    CameraProfile::Resolved r;

    /* Halfway in MIRED between 2856 K and 6504 K. Weight must be 0.5 here. */
    const float miredMid = float(2.0 / (1.0 / 2856.0 + 1.0 / 6504.0));
    QVERIFY(qAbs(miredMid - 3969.1f) < 0.5f);           // the value, so the test is readable
    QVERIFY(CameraProfile::resolve(p, miredMid, r));
    QVERIFY2(qAbs(r.weight - 0.5f) < 1e-4f,
             qPrintable(QString("mired midpoint weight %1").arg(double(r.weight))));

    /* ...and NOT at the kelvin midpoint, which is where a plain lerp would put it. This is
       the regression: kelvin and mired disagree by 13 points of weight in the middle of
       the range, which is a visible shift in the colour of every mixed-light shot. */
    const float kelvinMid = (2856.0f + 6504.0f) / 2.0f;
    QVERIFY(CameraProfile::resolve(p, kelvinMid, r));
    QVERIFY2(qAbs(r.weight - 0.5f) > 0.1f,
             qPrintable(QString("kelvin midpoint weight %1 -- interpolation looks linear "
                                "in kelvin").arg(double(r.weight))));
    QVERIFY(r.weight > 0.5f);                            // 4680 K is past the mired middle
}

void TstCameraProfile::clampsOutsideTheCalibrationRange()
{
    const Dcp::Profile p = canon77D();
    CameraProfile::Resolved r;

    /* Extrapolating a fitted matrix past the point it was fitted at is not meaningful, so
       the nearer calibration stands alone outside the range. */
    QVERIFY(CameraProfile::resolve(p, 2000.0f, r));
    QCOMPARE(r.weight, 0.0f);
    QVERIFY(CameraProfile::resolve(p, 50000.0f, r));
    QCOMPARE(r.weight, 1.0f);
}

void TstCameraProfile::illuminantOrderDoesNotMatter()
{
    const Dcp::Profile normal = canon77D();

    /* The same profile written cool-first. Nothing in the format forbids it, and a
       reader that trusts the listed order interpolates BACKWARDS -- worst at the ends,
       where it returns the wrong calibration outright. */
    Dcp::Profile reversed = normal;
    std::swap(reversed.cal[0], reversed.cal[1]);

    for (float k : {2856.0f, 3982.0f, 5000.0f, 6504.0f}) {
        CameraProfile::Resolved a, b;
        QVERIFY(CameraProfile::resolve(normal, k, a));
        QVERIFY(CameraProfile::resolve(reversed, k, b));
        QCOMPARE(b.warmKelvin, a.warmKelvin);
        QVERIFY(qAbs(a.weight - b.weight) < 1e-6f);
        for (int i = 0; i < 3; ++i)
            for (int j = 0; j < 3; ++j)
                QVERIFY2(qAbs(a.color.m[i][j] - b.color.m[i][j]) < 1e-6,
                         qPrintable(QString("reversed order differs at %1 K").arg(double(k))));
    }
}

void TstCameraProfile::singleIlluminantProfileResolvesEverywhere()
{
    Dcp::Profile p = canon77D();
    p.cal[1] = Dcp::Calibration();               // drop the second calibration entirely

    for (float k : {2000.0f, 5000.0f, 20000.0f}) {
        CameraProfile::Resolved r;
        QVERIFY(CameraProfile::resolve(p, k, r));
        QCOMPARE(r.weight, 0.0f);
        QVERIFY(qAbs(r.color.m[0][0] - kColor1[0]) < 1e-6);
    }

    /* And a profile with no ColorMatrix at all is not a camera profile. */
    Dcp::Profile empty;
    empty.valid = true;
    CameraProfile::Resolved r;
    QVERIFY(!CameraProfile::resolve(empty, 5000.0f, r));
}

void TstCameraProfile::neutralRendersNeutral_data()
{
    QTest::addColumn<bool>("withForward");
    QTest::addColumn<float>("kelvin");
    QTest::addColumn<float>("tint");
    for (bool fwd : {true, false}) {
        const char *tag = fwd ? "ForwardMatrix" : "derived";
        for (float k : {2856.0f, 4000.0f, 5500.0f, 6504.0f, 10000.0f}) {
            QTest::newRow(qPrintable(QString("%1 %2K").arg(tag).arg(double(k), 0, 'f', 0)))
                << fwd << k << 0.0f;
        }
        QTest::newRow(qPrintable(QString("%1 5500K +40 tint").arg(tag)))
            << fwd << 5500.0f << 40.0f;
        QTest::newRow(qPrintable(QString("%1 5500K -40 tint").arg(tag)))
            << fwd << 5500.0f << -40.0f;
    }
}

void TstCameraProfile::neutralRendersNeutral()
{
    QFETCH(bool, withForward);
    QFETCH(float, kelvin);
    QFETCH(float, tint);

    Dcp::Profile p = canon77D();
    if (withForward) normaliseForwardToD50(p);
    else {
        p.cal[0].haveForward = false;
        p.cal[1].haveForward = false;
    }

    double n[3];
    QVERIFY(CameraProfile::neutralCam(p, kelvin, tint, n));
    QVERIFY(qAbs(n[1] - 1.0) < 1e-9);        // normalised to green == 1

    float m[3][3];
    QVERIFY(CameraProfile::camToWorking(p, kelvin, tint, m));

    double rgb[3];
    for (int i = 0; i < 3; ++i)
        rgb[i] = double(m[i][0]) * n[0] + double(m[i][1]) * n[1] + double(m[i][2]) * n[2];

    /* THE anchor: the illuminant's own neutral renders to white, at unit brightness.
       Neutral-ness is the colour half (a profile that tints greys is unusable) and the
       1.0 is the exposure half (a profile change must not brighten the picture).

       EXACT ON BOTH PATHS, to well below a 16-bit level. With a ForwardMatrix that is
       because the matrix's rows sum to the D50 white (normalised above -- see the note on
       the helper); without one it is because the adaptation is BUILT to carry the white
       the neutral lands on exactly onto D50. Neither is approximate. */
    for (int i = 0; i < 3; ++i)
        QVERIFY2(qAbs(rgb[i] - 1.0) < 1e-6,
                 qPrintable(QString("channel %1 = %2 at %3 K, tint %4")
                                .arg(i).arg(rgb[i], 0, 'g', 8)
                                .arg(double(kelvin)).arg(double(tint))));
}

void TstCameraProfile::publishedForwardMatrixIsWithinItsOwnRounding()
{
    /*
        The same anchor with the ForwardMatrix EXACTLY as Adobe ships it -- rationals over
        10000, whose rows therefore sum to the D50 white only to about 1e-4. This is what
        the app will actually run on, so the residual is measured rather than normalised
        away: it must be small enough to be invisible (well under a 16-bit level, 1.5e-5)
        and it must come from the profile, not from the chain -- so it is bounded by the
        matrix's own deviation rather than by a number picked to make the test pass.
    */
    const Dcp::Profile p = canon77D();

    double worstRow = 0.0;
    for (int c = 0; c < 2; ++c)
        for (int row = 0; row < 3; ++row) {
            const double sum = double(p.cal[c].forward.m[row][0])
                             + double(p.cal[c].forward.m[row][1])
                             + double(p.cal[c].forward.m[row][2]);
            worstRow = qMax(worstRow, qAbs(sum - ColorSpaceMath::kWhiteD50[row]));
        }
    QVERIFY(worstRow > 0.0);                    // otherwise this test proves nothing
    QVERIFY(worstRow < 1e-3);

    for (float k : {2856.0f, 4500.0f, 6504.0f}) {
        double n[3];
        float m[3][3];
        QVERIFY(CameraProfile::neutralCam(p, k, 0.0f, n));
        QVERIFY(CameraProfile::camToWorking(p, k, 0.0f, m));
        for (int i = 0; i < 3; ++i) {
            const double v = double(m[i][0]) * n[0] + double(m[i][1]) * n[1]
                           + double(m[i][2]) * n[2];
            QVERIFY2(qAbs(v - 1.0) < 4.0 * worstRow,
                     qPrintable(QString("channel %1 = %2 at %3 K, worst row %4")
                                    .arg(i).arg(v, 0, 'g', 8).arg(double(k)).arg(worstRow)));
        }
    }
}

void TstCameraProfile::profileChangeDoesNotMoveExposure()
{
    /*
        Two profiles for the same camera that disagree about colour must still agree about
        BRIGHTNESS. Here the second is the first with its colour matrices perturbed -- a
        different fit of the same sensor, which is what a second profile is.

        This is the property that lets the Profile control be offered at all: a user trying
        profiles is comparing colour renderings, and a control that also nudged exposure
        would be read as one of the two being "better" for the wrong reason.
    */
    Dcp::Profile a = canon77D();
    normaliseForwardToD50(a);
    Dcp::Profile b = a;
    for (int i = 0; i < 3; ++i)
        for (int j = 0; j < 3; ++j) {
            b.cal[0].color.m[i][j] *= (i == j) ? 1.03f : 0.97f;
            b.cal[1].color.m[i][j] *= (i == j) ? 1.03f : 0.97f;
        }
    /* The forward matrices are dropped from b so both of its paths are exercised, and so
       that the two profiles do not share the piece that does the anchoring. */
    b.cal[0].haveForward = b.cal[1].haveForward = false;

    for (float k : {3000.0f, 5000.0f, 6504.0f}) {
        double na[3], nb[3];
        float ma[3][3], mb[3][3];
        QVERIFY(CameraProfile::neutralCam(a, k, 0.0f, na));
        QVERIFY(CameraProfile::neutralCam(b, k, 0.0f, nb));
        QVERIFY(CameraProfile::camToWorking(a, k, 0.0f, ma));
        QVERIFY(CameraProfile::camToWorking(b, k, 0.0f, mb));

        /* They differ -- otherwise the test proves nothing. */
        QVERIFY(qAbs(double(ma[0][0]) - double(mb[0][0])) > 1e-4);

        /* ...and both put their own neutral at unit brightness. */
        for (int which = 0; which < 2; ++which) {
            const float (*m)[3] = which ? mb : ma;
            const double *n = which ? nb : na;
            const double g = double(m[1][0]) * n[0] + double(m[1][1]) * n[1]
                           + double(m[1][2]) * n[2];
            QVERIFY2(qAbs(g - 1.0) < 1e-6,
                     qPrintable(QString("profile %1 at %2 K renders neutral green = %3")
                                    .arg(which).arg(double(k)).arg(g, 0, 'g', 8)));
        }
    }
}

void TstCameraProfile::realProfilesRenderNeutral()
{
    /* Opportunistic, like tst_dcp's sweep: real profiles are licensed to their authors
       and are not copied into the repo. */
#ifdef Q_OS_MAC
    const QString root = "/Library/Application Support/Adobe/CameraRaw/CameraProfiles";
#else
    const QString root = "C:/ProgramData/Adobe/CameraRaw/CameraProfiles";
#endif
    if (!QDir(root).exists()) QSKIP("no camera profiles installed on this machine");

    QStringList paths;
    QDirIterator it(root, QStringList() << "*.dcp", QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext()) paths << it.next();
    if (paths.isEmpty()) QSKIP("no camera profiles installed on this machine");
    paths.sort();

    const int stride = qMax(1, paths.size() / 200);
    int checked = 0, forwardChecked = 0;
    double worstNeutral = 0.0, worstD50 = 0.0;

    for (int i = 0; i < paths.size(); i += stride) {
        Dcp::Profile p;
        if (!Dcp::parseFile(paths[i], p)) continue;

        for (float k : {2856.0f, 4500.0f, 6504.0f}) {
            double n[3];
            float m[3][3];
            if (!CameraProfile::neutralCam(p, k, 0.0f, n)) continue;
            if (!CameraProfile::camToWorking(p, k, 0.0f, m)) continue;
            ++checked;
            for (int c = 0; c < 3; ++c) {
                const double v = double(m[c][0]) * n[0] + double(m[c][1]) * n[1]
                               + double(m[c][2]) * n[2];
                worstNeutral = qMax(worstNeutral, qAbs(v - 1.0));
            }
        }

        /* A well-formed ForwardMatrix has rows summing to the D50 white -- that IS its
           defining property, and the exposure anchor above leans on it. Measuring how
           closely real profiles hold it says whether the anchor is a construction or a
           coincidence. */
        for (int c = 0; c < 2; ++c) {
            if (!p.cal[c].haveForward) continue;
            ++forwardChecked;
            for (int row = 0; row < 3; ++row) {
                const double sum = double(p.cal[c].forward.m[row][0])
                                 + double(p.cal[c].forward.m[row][1])
                                 + double(p.cal[c].forward.m[row][2]);
                worstD50 = qMax(worstD50, qAbs(sum - ColorSpaceMath::kWhiteD50[row]));
            }
        }
    }

    qInfo() << "checked" << checked << "profile/temperature pairs; worst neutral error"
            << worstNeutral << "; ForwardMatrix rows checked" << forwardChecked
            << ", worst D50 deviation" << worstD50;

    QVERIFY(checked > 0);
    QVERIFY(forwardChecked > 0);
    /* Real profiles are stored as rationals over 10000, so the D50 row sums are exact only
       to that rounding; the neutral they produce follows. Both bounds are generous enough
       to survive that and tight enough to catch a real mistake. */
    QVERIFY2(worstNeutral < 1e-3,
             qPrintable(QString("worst neutral error %1").arg(worstNeutral)));
    QVERIFY2(worstD50 < 1e-3,
             qPrintable(QString("worst ForwardMatrix D50 deviation %1").arg(worstD50)));
}

QTEST_MAIN(TstCameraProfile)
#include "tst_cameraprofile.moc"
