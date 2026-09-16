#include <QtTest>
#include <cmath>
#include "Develop/whitebalance.h"

/*
    White balance colour science (Develop/whitebalance.h).

    These guard the parts that are easy to get subtly wrong and hard to spot by eye:
    the TEMPERATURE CONVENTION (which sets what an untouched file READS as, and whether
    the number means the same thing as Lightroom's), the sign of both controls (a flipped
    tint feels wrong immediately but is invisible in code review), and the solver's
    round-trip accuracy (the dropper and Auto both depend on it).

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
                c.camToWorking[i][j] = (i == j) ? 1.0f : 0.0f;
            }
        }
        WhiteBalance::resolveAsShot(c);
        return c;
    }

    /*
        A REAL camera: the Nikon D850's libraw adobe_coeff matrix plus the as-shot
        multipliers from a real NEF. Built the way RawColor::Characterise builds it --
        camRgb = xyzToCam . rgbToXyz, row-normalised so a neutral scene maps to (1,1,1),
        then inverted -- because the solve walks that exact chain and a shortcut here
        would test a camera the pipeline never produces. Written out rather than linked
        in: this test compiles whitebalance.cpp alone (see tests/CMakeLists.txt).
    */
    static CameraColor d850Cam()
    {
        const double xyzToCam[3][3] = {
            { 1.0405, -0.3755, -0.1270},
            {-0.5461,  1.3787,  0.1793},
            {-0.1040,  0.2015,  0.6785}
        };
        const double rgbToXyz[3][3] = {
            {0.412453, 0.357580, 0.180423},
            {0.212671, 0.715160, 0.072169},
            {0.019334, 0.119193, 0.950227}
        };
        double camRgb[3][3];
        for (int i = 0; i < 3; ++i)
            for (int j = 0; j < 3; ++j) {
                double v = 0.0;
                for (int k = 0; k < 3; ++k) v += xyzToCam[i][k] * rgbToXyz[k][j];
                camRgb[i][j] = v;
            }
        for (int i = 0; i < 3; ++i) {
            const double n = camRgb[i][0] + camRgb[i][1] + camRgb[i][2];
            for (int j = 0; j < 3; ++j) camRgb[i][j] /= n;
        }
        double inv[3][3];
        const double det =
            camRgb[0][0] * (camRgb[1][1] * camRgb[2][2] - camRgb[1][2] * camRgb[2][1]) -
            camRgb[0][1] * (camRgb[1][0] * camRgb[2][2] - camRgb[1][2] * camRgb[2][0]) +
            camRgb[0][2] * (camRgb[1][0] * camRgb[2][1] - camRgb[1][1] * camRgb[2][0]);
        for (int i = 0; i < 3; ++i)
            for (int j = 0; j < 3; ++j) {
                const int a0 = (j + 1) % 3, a1 = (j + 2) % 3;
                const int b0 = (i + 1) % 3, b1 = (i + 2) % 3;
                inv[i][j] = (camRgb[a0][b0] * camRgb[a1][b1] -
                             camRgb[a0][b1] * camRgb[a1][b0]) / det;
            }

        CameraColor c;
        c.valid = true;
        const float mul[3] = {2.126953f, 1.0f, 1.246582f};   // 0x0C, green-normalised
        for (int i = 0; i < 3; ++i) {
            c.asShotMul[i] = mul[i];
            for (int j = 0; j < 3; ++j) {
                c.xyzToCam[i][j] = float(xyzToCam[i][j]);
                c.camToWorking[i][j] = float(inv[i][j]);
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

    /*
        An sRGB file is balanced to D65, and in ADOBE'S convention -- the DNG SDK's
        dng_temperature, Robertson on the PLANCKIAN locus -- D65 reads 6503 K / +9.8, NOT
        6500 / 0. Daylight genuinely sits about 0.003 Duv above the Planckian locus and
        Lightroom reports it the same way; a tint of 0 here would mean the old
        daylight-locus convention had come back, and with it a ~20-unit disagreement with
        every other raw developer.
    */
    void asShotResolvesToD65()
    {
        const CameraColor c = srgbCam();
        QVERIFY2(std::fabs(c.asShotK - 6503.0f) < 60.0f,
                 qPrintable(QString("as-shot K = %1").arg(c.asShotK)));
        QVERIFY2(std::fabs(c.asShotTint - 9.8f) < 1.5f,
                 qPrintable(QString("as-shot tint = %1").arg(c.asShotTint)));
    }

    /*
        The convention itself, against illuminants whose CCT and tint are known
        independently. These three numbers are what make Winnow's Temp/Tint comparable to
        Lightroom's; drifting off them is the whole bug this suite exists to catch.
    */
    void knownIlluminantsReadCorrectly_data()
    {
        QTest::addColumn<double>("x");
        QTest::addColumn<double>("y");
        QTest::addColumn<float>("kelvin");
        QTest::addColumn<float>("tint");
        QTest::newRow("D65")   << 0.31271 << 0.32902 << 6503.0f << 9.79f;
        QTest::newRow("D50")   << 0.34567 << 0.35850 << 5001.8f << 9.60f;
        QTest::newRow("Std A") << 0.44757 << 0.40745 << 2855.8f << 0.01f;
    }

    void knownIlluminantsReadCorrectly()
    {
        QFETCH(double, x); QFETCH(double, y);
        QFETCH(float, kelvin); QFETCH(float, tint);
        float k = 0, t = 0;
        WhiteBalance::tempTintFromXY(x, y, k, t);
        QVERIFY2(std::fabs(k - kelvin) / kelvin < 0.005,
                 qPrintable(QString("K %1, want %2").arg(k).arg(kelvin)));
        QVERIFY2(std::fabs(t - tint) < 0.5f,
                 qPrintable(QString("tint %1, want %2").arg(t).arg(tint)));
    }

    /* illuminantXYZ and tempTintFromXY are exact inverses, so the Temp/Tint the panel
       shows and the illuminant the render uses cannot drift apart. */
    void locusRoundTrips_data()
    {
        QTest::addColumn<float>("kelvin");
        QTest::addColumn<float>("tint");
        for (float k : {2000.f, 2850.f, 4000.f, 5500.f, 6500.f, 12000.f, 50000.f})
            for (float t : {-100.f, -50.f, 0.f, 10.f, 50.f, 100.f})
                QTest::newRow(qPrintable(QString("%1K/%2").arg(k).arg(t))) << k << t;
    }

    void locusRoundTrips()
    {
        QFETCH(float, kelvin);
        QFETCH(float, tint);
        double xyz[3];
        QVERIFY(WhiteBalance::illuminantXYZ(kelvin, tint, xyz));
        const double sum = xyz[0] + xyz[1] + xyz[2];
        float k = 0, t = 0;
        WhiteBalance::tempTintFromXY(xyz[0] / sum, xyz[1] / sum, k, t);
        QVERIFY2(std::fabs(k - kelvin) / kelvin < 1e-3,
                 qPrintable(QString("K %1 -> %2").arg(kelvin).arg(k)));
        QVERIFY2(std::fabs(t - tint) < 0.05f,
                 qPrintable(QString("tint %1 -> %2").arg(tint).arg(t)));
    }

    /*
        THE REGRESSION THIS SUITE WAS REWRITTEN FOR.

        A real Nikon D850 frame (2018-01-14_0001.NEF): MakerNote 0x0C gives as-shot
        multipliers R 2.126953 / G 1 / B 1.246582, and the model matrix is the libraw
        adobe_coeff row for the body. Lightroom reads that file as 6300 K / +3.

        Under the old daylight-locus convention Winnow solved it to 5959 K / -18 -- and,
        because the Apple Core Image engine left cam invalid, actually DISPLAYED
        6500 K / 0, the "no characterisation" fallback. Both are now gone.
    */
    void nikonD850AsShotMatchesLightroom()
    {
        const CameraColor c = d850Cam();
        QVERIFY2(std::fabs(c.asShotK - 6315.0f) < 25.0f,
                 qPrintable(QString("as-shot K = %1, want ~6315").arg(c.asShotK)));
        QVERIFY2(std::fabs(c.asShotTint - 1.9f) < 2.0f,
                 qPrintable(QString("as-shot tint = %1, want ~+1.9").arg(c.asShotTint)));
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

        /*
            OUT-OF-GAMUT ILLUMINANT. Adobe's tint scale is 3000 units per Duv, so the
            slider ends reach a lot further off the locus than the old 4000-per-Duv one
            did: 3400 K at +150 is off the chromaticity triangle entirely (z < 0), and
            20000 K at +150 is a cyan-green well outside the SYNTHETIC sRGB camera used
            here (a real camera matrix is wider). renderIlluminant clamps the dead
            channel to 1e-9, and inverting a gain that large gives back a colour with a
            channel at or below solve()'s own 1e-9 floor -- which it is documented to
            refuse, because the clamp destroyed the information.

            Keyed on that floor rather than on "the gain looks big": a clamp in one
            channel does NOT always ruin the round trip (2000 K carries a legitimately
            huge blue gain and still solves), so anything coarser fails the wrong cases.

            Nothing is lost for the user: gains() still returns finite gains and the
            render is well behaved at the slider end. It is the ROUND TRIP that is not
            defined. Assert the refusal rather than skipping, so this staying true is
            still checked.
        */
        const float back[3] = {1.0f / g[0], 1.0f / g[1], 1.0f / g[2]};
        if (back[0] <= 1e-9f || back[1] <= 1e-9f || back[2] <= 1e-9f) {
            QVERIFY(!WhiteBalance::solve(c, back[0], back[1], back[2], k2, t2));
            return;
        }

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

    /* The locus is a 31-row TABLE interpolated between rows, so every row boundary is a
       potential seam -- and a seam shows as a colour jump mid-drag. The old code had a
       crossfade between two loci across 4000-5000 K to watch here; the DNG table has row
       boundaries at 200 and 250 mired (5000 K and 4000 K), which is the same stretch of
       slider, so the case still lands where it is needed.

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
