#include <QtTest>
#include <cmath>
#include "Develop/whitebalance.h"

/*
    White balance colour science (Develop/whitebalance.h).

    These guard the parts that are easy to get subtly wrong and hard to spot by eye:
    the locus choice (which sets what an untouched file READS as), the sign of both
    controls (a flipped tint feels wrong immediately but is invisible in code review),
    and the solver's round-trip accuracy (the dropper and Auto both depend on it).

    Every case runs against a synthetic camera, so no image fixtures are needed.
*/
class tst_whitebalance : public QObject
{
    Q_OBJECT

private:
    /* A display-referred "camera": XYZ -> linear sRGB, nothing baked in. The same
       characterisation InputTransform builds for a JPEG, whose white point is D65. */
    static CameraColor srgbCam()
    {
        CameraColor c;
        const float m[3][3] = {
            { 3.2404542f, -1.5371385f, -0.4985314f},
            {-0.9692660f,  1.8760108f,  0.0415560f},
            { 0.0556434f, -0.2040259f,  1.0572252f}
        };
        c.valid = true;
        for (int i = 0; i < 3; ++i) {
            c.asShotMul[i] = 1.0f;
            for (int j = 0; j < 3; ++j) {
                c.xyzToCam[i][j] = m[i][j];
                c.camToSrgb[i][j] = (i == j) ? 1.0f : 0.0f;
            }
        }
        WhiteBalance::resolveAsShot(c);
        return c;
    }

    /* A body balanced for warm light: as-shot multipliers well away from unity. */
    static CameraColor warmShotCam()
    {
        CameraColor c = srgbCam();
        c.asShotMul[0] = 0.60f;
        c.asShotMul[1] = 1.00f;
        c.asShotMul[2] = 1.90f;
        WhiteBalance::resolveAsShot(c);
        return c;
    }

private slots:

    /* An sRGB file is balanced to D65, which lives on the CIE DAYLIGHT locus. Solving
       it against the Planckian locus instead lands ~550 K and ~22 tint units out, so an
       untouched JPEG would open reading "7050 K, -22" rather than "6500, 0". */
    void asShotResolvesToD65()
    {
        const CameraColor c = srgbCam();
        QVERIFY2(std::fabs(c.asShotK - 6504.0f) < 250.0f,
                 qPrintable(QString("as-shot K = %1").arg(c.asShotK)));
        QVERIFY2(std::fabs(c.asShotTint) < 6.0f,
                 qPrintable(QString("as-shot tint = %1").arg(c.asShotTint)));
    }

    void warmBalancedBodyResolvesCool()
    {
        const CameraColor c = warmShotCam();
        QVERIFY2(c.asShotK < 4000.0f,
                 qPrintable(QString("as-shot K = %1").arg(c.asShotK)));
    }

    /* EditParams::temp == 0 means "as shot" and MUST render as an exact no-op, even
       though asShotK came from a bisection fit and carries residual error. */
    void asShotIsExactIdentity()
    {
        for (const CameraColor &c : {srgbCam(), warmShotCam()}) {
            float g[3];
            WhiteBalance::relativeGains(c, c.asShotK, c.asShotTint, g);
            for (int i = 0; i < 3; ++i)
                QVERIFY2(std::fabs(g[i] - 1.0f) < 1e-3f,
                         qPrintable(QString("gain[%1] = %2").arg(i).arg(g[i])));
        }
    }

    /* Both controls describe the LIGHT, so the image moves the opposite way: a higher
       Kelvin says the light was bluer and renders warmer. A flipped sign here is the
       single most user-visible way this module can break. */
    void temperatureDirection()
    {
        const CameraColor c = srgbCam();
        float g[3];
        WhiteBalance::relativeGains(c, 2850, 0, g);         // tungsten
        QVERIFY(g[0] < 1.0f && g[2] > 1.0f);                // cools
        WhiteBalance::relativeGains(c, 9000, 0, g);         // shade
        QVERIFY(g[0] > 1.0f && g[2] < 1.0f);                // warms
    }

    /* Positive tint renders MORE MAGENTA. Green is pinned to 1 by construction, so it
       is red and blue that rise -- a test comparing the green gain would always pass. */
    void tintDirection()
    {
        const CameraColor c = srgbCam();
        float gp[3], gm[3];
        WhiteBalance::relativeGains(c, c.asShotK,  60, gp);
        WhiteBalance::relativeGains(c, c.asShotK, -60, gm);
        QVERIFY(gp[0] + gp[2] > gm[0] + gm[2]);
        QVERIFY(gp[0] > 1.0f && gp[2] > 1.0f);
    }

    /* solve() is the inverse of gains(): feed back the colour that gains() neutralises
       and the original pair must come out. Covers strong tints, where an alternating
       (rather than nested) bisection drifts badly -- +80 tint at 3400 K once came back
       as 2742 K / +42. */
    void solveRoundTrips_data()
    {
        QTest::addColumn<float>("kelvin");
        QTest::addColumn<float>("tint");
        for (float k : {2000.f, 2500.f, 3400.f, 4000.f, 4500.f, 5500.f,
                        6500.f, 9000.f, 20000.f, 50000.f})
            for (float t : {-150.f, -90.f, -20.f, 0.f, 25.f, 80.f, 150.f})
                QTest::newRow(qPrintable(QString("%1K/%2").arg(k).arg(t))) << k << t;
    }

    void solveRoundTrips()
    {
        QFETCH(float, kelvin);
        QFETCH(float, tint);
        const CameraColor c = srgbCam();

        float g[3];
        WhiteBalance::gains(c, kelvin, tint, g);
        float k2 = 0, t2 = 0;
        QVERIFY(WhiteBalance::solve(c, 1.0f / g[0], 1.0f / g[1], 1.0f / g[2], k2, t2));
        QVERIFY2(std::fabs(k2 - kelvin) / kelvin < 0.01,
                 qPrintable(QString("K %1 -> %2").arg(kelvin).arg(k2)));
        QVERIFY2(std::fabs(t2 - tint) < 1.0f,
                 qPrintable(QString("tint %1 -> %2").arg(tint).arg(t2)));
    }

    /* The dropper's contract: "this pixel should have been grey". */
    void dropperNeutralisesTheSample()
    {
        const CameraColor c = srgbCam();
        float k = 0, t = 0, g[3];

        QVERIFY(WhiteBalance::solve(c, 0.80f, 0.90f, 1.20f, k, t));  // bluish sample
        WhiteBalance::relativeGains(c, k, t, g);
        QVERIFY(g[0] > 1.0f && g[2] < 1.0f);                         // -> warms

        QVERIFY(WhiteBalance::solve(c, 1.25f, 0.95f, 0.70f, k, t));  // warm sample
        WhiteBalance::relativeGains(c, k, t, g);
        QVERIFY(g[0] < 1.0f && g[2] > 1.0f);                         // -> cools

        /* An already-neutral sample must leave the image where it was. */
        QVERIFY(WhiteBalance::solve(c, 0.5f, 0.5f, 0.5f, k, t));
        QVERIFY(std::fabs(k - c.asShotK) / c.asShotK < 0.01f);
    }

    /* A black or blown sample carries no colour: rejected, not silently wrong. */
    void degenerateSamplesRejected()
    {
        const CameraColor c = srgbCam();
        float k = 0, t = 0;
        QVERIFY(!WhiteBalance::solve(c, 0.0f, 0.5f, 0.5f, k, t));
        QVERIFY(!WhiteBalance::solve(c, 0.5f, 0.0f, 0.5f, k, t));

        /* An unvalidated camera cannot resolve anything, and must not pretend to. */
        CameraColor invalid;
        QVERIFY(!WhiteBalance::solve(invalid, 0.5f, 0.5f, 0.5f, k, t));
        float g[3];
        WhiteBalance::gains(invalid, 5500, 0, g);
        QCOMPARE(g[0], 1.0f);
        QCOMPARE(g[1], 1.0f);
        QCOMPARE(g[2], 1.0f);
    }

    /* The Temp slider crossfades the Planckian and daylight loci between 4000 and
       5000 K. A discontinuity there would show as a colour jump mid-drag.

       Tested as an OUTLIER check, not a fixed threshold: the gain curve is legitimately
       steep at low Kelvin (a 100 K step near 3800 K moves blue more than a fixed
       tolerance allows), so what identifies a seam is one step being far larger than
       its neighbours -- which is exactly how a step change presents. */
    void locusBlendIsContinuous()
    {
        const CameraColor c = srgbCam();
        const int step = 25;
        QVector<double> deltas[3];
        float prev[3];
        WhiteBalance::gains(c, 3600, 0, prev);
        for (int k = 3600 + step; k <= 5400; k += step) {
            float g[3];
            WhiteBalance::gains(c, float(k), 0, g);
            for (int i = 0; i < 3; ++i) deltas[i].append(std::fabs(g[i] - prev[i]));
            std::copy(g, g + 3, prev);
        }
        for (int i = 0; i < 3; ++i) {
            QVector<double> sorted = deltas[i];
            std::sort(sorted.begin(), sorted.end());
            const double median = sorted[sorted.size() / 2];
            const double worst  = sorted.last();
            QVERIFY2(worst < median * 4.0 + 1e-6,
                     qPrintable(QString("channel %1: worst step %2 vs median %3")
                                    .arg(i).arg(worst).arg(median)));
        }
    }

    /* Skin sampling drives the sample onto the skin HUE LINE while leaving its
       saturation alone -- so light and deep skin both land on the line rather than
       both being forced to one colour. Chroma is measured in the same (warmth, green)
       plane the solver uses, where neutral is the origin. */
    void skinLandsOnTheHueLine_data()
    {
        QTest::addColumn<float>("r");
        QTest::addColumn<float>("g");
        QTest::addColumn<float>("b");
        /* Linear-sRGB chromaticities of the ColorChecker light- and dark-skin patches
           (G normalised to 1), and a mid tone between them. */
        QTest::newRow("light skin") << 1.769f << 1.0f << 0.732f;
        QTest::newRow("dark skin")  << 2.032f << 1.0f << 0.685f;
        QTest::newRow("mid skin")   << 1.900f << 1.0f << 0.710f;
    }

    void skinLandsOnTheHueLine()
    {
        QFETCH(float, r); QFETCH(float, g); QFETCH(float, b);
        const CameraColor c = srgbCam();

        /* Throw a cast on the skin, then ask the dropper to undo it. */
        const float castR = 1.18f, castB = 0.80f;      // warm/orange cast
        const float sr = r * castR, sg = g, sb = b * castB;

        float k = 0, t = 0, hueErr = -1;
        QCOMPARE(WhiteBalance::solveSkin(c, sr, sg, sb, k, t, &hueErr),
                 WhiteBalance::SkinPick::Ok);

        float gain[3];
        WhiteBalance::relativeGains(c, k, t, gain);
        const double cr = sr * gain[0], cg = sg * gain[1], cb = sb * gain[2];

        /* Corrected sample must sit ON the skin hue line. */
        const double w  = std::log(cr / cb);
        const double gg = std::log(cg / std::sqrt(cr * cb));
        const double ang = std::atan2(gg, w) * 57.29577951308232;
        QVERIFY2(std::fabs(ang - WhiteBalance::kSkinHueDeg) < 1.5,
                 qPrintable(QString("corrected hue %1 deg, want %2")
                                .arg(ang).arg(WhiteBalance::kSkinHueDeg)));
    }

    /* The saturation of the sample must SURVIVE: deep skin stays deeper than light
       skin. A fixed-reference target would collapse both onto one chromaticity. */
    void skinPreservesSaturation()
    {
        const CameraColor c = srgbCam();
        auto correctedChroma = [&](float r, float g, float b) {
            float k = 0, t = 0;
            if (WhiteBalance::solveSkin(c, r, g, b, k, t) != WhiteBalance::SkinPick::Ok)
                return -1.0;
            float gain[3];
            WhiteBalance::relativeGains(c, k, t, gain);
            const double cr = r * gain[0], cg = g * gain[1], cb = b * gain[2];
            return std::hypot(std::log(cr / cb), std::log(cg / std::sqrt(cr * cb)));
        };
        const double light = correctedChroma(1.769f, 1.0f, 0.732f);
        const double dark  = correctedChroma(2.032f, 1.0f, 0.685f);
        QVERIFY(light > 0 && dark > 0);
        QVERIFY2(dark > light * 1.10,
                 qPrintable(QString("dark chroma %1 vs light %2 -- deep skin was "
                                    "flattened toward the light reference")
                                .arg(dark).arg(light)));
    }

    /* Already-correct skin must be left alone (no gratuitous shift). */
    void skinAlreadyCorrectIsNearlyNoOp()
    {
        const CameraColor c = srgbCam();
        float k = 0, t = 0;
        QCOMPARE(WhiteBalance::solveSkin(c, 1.769f, 1.0f, 0.732f, k, t),
                 WhiteBalance::SkinPick::Ok);
        float gain[3];
        WhiteBalance::relativeGains(c, k, t, gain);
        /* The reference sits ~0.2 deg off the nominal line, so allow a small move. */
        for (int i = 0; i < 3; ++i) QVERIFY(std::fabs(gain[i] - 1.0f) < 0.06f);
    }

    /* The guard: gross mis-clicks are refused, plausible skin is not. Hue alone cannot
       separate skin from a red/orange object -- that is a documented limit, not a bug. */
    void skinRejectsNonSkin()
    {
        const CameraColor c = srgbCam();
        float k = 0, t = 0, e = 0;
        using SP = WhiteBalance::SkinPick;
        /* Blue sky and foliage: far off the line. */
        QCOMPARE(WhiteBalance::solveSkin(c, 0.30f, 0.45f, 1.00f, k, t, &e), SP::NotSkin);
        QCOMPARE(WhiteBalance::solveSkin(c, 0.20f, 0.45f, 0.10f, k, t, &e), SP::NotSkin);
        /* A neutral patch has no hue to read. */
        QCOMPARE(WhiteBalance::solveSkin(c, 0.50f, 0.50f, 0.50f, k, t, &e), SP::TooNeutral);
        /* Black / clipped. */
        QCOMPARE(WhiteBalance::solveSkin(c, 0.0f, 0.5f, 0.5f, k, t, &e), SP::Degenerate);
        /* Skin under a strong tungsten cast must still be ACCEPTED -- refusing it would
           reject exactly the images the tool exists for. */
        QCOMPARE(WhiteBalance::solveSkin(c, 2.60f, 1.0f, 0.42f, k, t, &e), SP::Ok);
    }

    /* Picking a neutral via the skin path is the same maths with T = (1,1,1); this
       pins the shared reduction so the two dropper modes cannot drift apart. */
    void skinReducesToNeutralSolve()
    {
        const CameraColor c = srgbCam();
        float k1 = 0, t1 = 0, k2 = 0, t2 = 0;
        /* A colour exactly ON the skin line: the projection is a no-op, so solveSkin
           must agree with solving for the residual s/T directly. */
        const float r = 1.769f, g = 1.0f, b = 0.732f;
        QCOMPARE(WhiteBalance::solveSkin(c, r, g, b, k1, t1), WhiteBalance::SkinPick::Ok);
        QVERIFY(WhiteBalance::solve(c, r / 1.769f, g, b / 0.732f, k2, t2));
        QVERIFY(std::fabs(k1 - k2) / k2 < 0.02f);
        QVERIFY(std::fabs(t1 - t2) < 2.0f);
    }

    void presetTable()
    {
        float k = 0, t = 0;
        QVERIFY(WhiteBalance::presetValues(WhiteBalance::Preset::Daylight, k, t));
        QCOMPARE(k, 5500.0f);
        QVERIFY(WhiteBalance::presetValues(WhiteBalance::Preset::Tungsten, k, t));
        QCOMPARE(k, 2850.0f);
        /* As Shot / Auto / Custom are not fixed illuminants. */
        QVERIFY(!WhiteBalance::presetValues(WhiteBalance::Preset::AsShot, k, t));
        QVERIFY(!WhiteBalance::presetValues(WhiteBalance::Preset::Auto, k, t));
        QVERIFY(!WhiteBalance::presetValues(WhiteBalance::Preset::Custom, k, t));

        QCOMPARE(WhiteBalance::presetFromName("Tungsten"), WhiteBalance::Preset::Tungsten);
        QCOMPARE(WhiteBalance::presetFromName("As Shot"),  WhiteBalance::Preset::AsShot);
        /* Anything unrecognised falls back to the safe, non-destructive default. */
        QCOMPARE(WhiteBalance::presetFromName("nonsense"), WhiteBalance::Preset::AsShot);
    }
};

QTEST_APPLESS_MAIN(tst_whitebalance)
#include "tst_whitebalance.moc"
