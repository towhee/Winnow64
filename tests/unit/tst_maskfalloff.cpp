/*
    MaskFalloff -- the one feather profile every Develop mask tool shares.

    These are guard rails on the SHAPE MAP, not on pixels. The map (h and p from the
    feather) was solved numerically against a Lightroom reference, and the first attempt
    at it passed the LR anchor while quietly breaking two properties a slider must have:
    softness stopped increasing past feather 82, and low feathers came out far crisper
    than the number implied. Both are cheap to assert and impossible to eyeball in a
    screenshot, so they live here.
*/
#include <QtTest>
#include <cmath>
#include "Develop/maskfalloff.h"
#include "Develop/brushstamp.h"

class TestMaskFalloff : public QObject
{
    Q_OBJECT

private slots:
    void profileIsMonotoneAndAnchored();
    void lutMatchesReference();
    void softnessRisesWithFeather();
    void lightroomAnchor();
    void gradientCdf();
    void bandTaper();
    void brushMatchesProfile();
};

/* Coverage falls monotonically from 1 at the centre, passes 0.5 at h, and reaches 0. */
void TestMaskFalloff::profileIsMonotoneAndAnchored()
{
    for (double f : {5.0, 25.0, 50.0, 82.0, 100.0}) {
        const MaskFalloff::Shape s = MaskFalloff::shapeFor(f);
        QVERIFY2(s.h > 0.0 && s.h <= 1.0, qPrintable(QString("h out of range at %1").arg(f)));
        QCOMPARE(MaskFalloff::coverage(0.0, s), 1.0f);
        QVERIFY(std::abs(MaskFalloff::coverage(s.h, s) - 0.5f) < 0.001f);

        float prev = 1.0f;
        for (double t = 0.0; t < 3.0; t += 0.005) {
            const float c = MaskFalloff::coverage(t, s);
            QVERIFY2(c <= prev + 1e-6f, qPrintable(QString("not monotone at f=%1 t=%2")
                                                       .arg(f).arg(t)));
            prev = c;
        }
        QCOMPARE(prev, 0.0f);       // the tail does terminate
    }
}

/* The per-pixel loops read Lut, not coverage(); they must agree. */
void TestMaskFalloff::lutMatchesReference()
{
    for (double f : {1.0, 10.0, 50.0, 82.0, 100.0}) {
        MaskFalloff::Lut lut;
        lut.build(f);
        const MaskFalloff::Shape s = MaskFalloff::shapeFor(f);
        double worst = 0.0;
        for (double t = 0.0; t <= 2.5; t += 0.001)
            worst = std::max(worst, std::abs(double(lut.at(t))
                                             - double(MaskFalloff::coverage(t, s))));
        QVERIFY2(worst < 2e-4, qPrintable(QString("Lut drifts %1 from coverage() at feather %2")
                                              .arg(worst).arg(f)));
        QCOMPARE(lut.at(0.0), 1.0f);
        QCOMPARE(lut.at(lut.tMax() + 0.1), 0.0f);
    }
}

/* Raising the slider must always soften: a WIDER 10-90 transition and a GENTLER peak
   slope, all the way to 100. The first shape map failed this between 82 and 100. */
void TestMaskFalloff::softnessRisesWithFeather()
{
    auto width = [](double f) {
        MaskFalloff::Lut lut;
        lut.build(f);
        double t90 = 0.0, t10 = 0.0;
        for (double t = 0.0; t < 3.0; t += 0.0005) {
            const float c = lut.at(t);
            if (c > 0.9f) t90 = t;
            if (c > 0.1f) t10 = t;
        }
        return t10 - t90;
    };
    double prev = -1.0;
    for (double f = 2.0; f <= 100.0; f += 2.0) {
        const double w = width(f);
        QVERIFY2(w > prev, qPrintable(QString("feather %1 is not softer than %2 (%3 vs %4)")
                                          .arg(f).arg(f - 2).arg(w).arg(prev)));
        prev = w;
    }
    /* And the width tracks the slider ~proportionally, so the number means something. */
    QVERIFY(std::abs(width(25.0) / 0.25 - width(75.0) / 0.75) < 0.25);
}

/* The one measured point: Lightroom's radial at feather 82 is a Gaussian of sigma
   0.450R, i.e. half coverage at 0.525R with p = 2. Everything else interpolates. */
void TestMaskFalloff::lightroomAnchor()
{
    const MaskFalloff::Shape s = MaskFalloff::shapeFor(82.0);
    QVERIFY(std::abs(s.h - 0.525) < 0.005);
    QVERIFY(std::abs(s.p - 2.0) < 0.05);
    /* sigma = h / sqrt(2 ln2) for a Gaussian. */
    QVERIFY(std::abs(s.h / 1.17741 - 0.450) < 0.01);
    /* No flat core, and still alive at the drawn boundary -- the two things that made
       the old smootherstep read wrong against LR. */
    QVERIFY(MaskFalloff::coverage(0.10, s) < 0.99f);
    QVERIFY(MaskFalloff::coverage(1.00, s) > 0.02f);
}

void TestMaskFalloff::gradientCdf()
{
    QVERIFY(std::abs(MaskFalloff::cdf(0.0) - 0.5f) < 1e-4f);
    QVERIFY(std::abs(MaskFalloff::cdf(-1.0) + MaskFalloff::cdf(1.0) - 1.0f) < 1e-4f);
    QVERIFY(std::abs(MaskFalloff::cdf(-2.0) - 0.02275f) < 1e-3f);
    QCOMPARE(MaskFalloff::cdf(-99.0), 0.0f);
    QCOMPARE(MaskFalloff::cdf(99.0), 1.0f);
    float prev = 0.0f;
    for (double x = -5.0; x <= 5.0; x += 0.01) {
        const float c = MaskFalloff::cdf(x);
        QVERIFY(c >= prev - 1e-6f);
        prev = c;
    }
    QCOMPARE(MaskFalloff::gradientSigma(0.0), 0.0);      // feather 0 -> hard step
    QVERIFY(MaskFalloff::gradientSigma(100.0) > MaskFalloff::gradientSigma(50.0));
}

void TestMaskFalloff::bandTaper()
{
    QCOMPARE(MaskFalloff::taper(0.0), 1.0f);
    QVERIFY(std::abs(MaskFalloff::taper(0.5) - 0.5f) < 0.002f);   // as smootherstep was
    QCOMPARE(MaskFalloff::taper(1.0), 0.0f);
    QCOMPARE(MaskFalloff::taper(2.0), 0.0f);
    QVERIFY(MaskFalloff::taper(0.02) < 1.0f);                     // falls from step 1
    float prev = 1.0f;
    for (double u = 0.0; u <= 1.0; u += 0.005) {
        const float c = MaskFalloff::taper(u);
        QVERIFY(c <= prev + 1e-6f);
        prev = c;
    }
}

/* The brush dab rides the same profile, scaled by the brush radius, and is clipped at
   the size circle so feather never grows the footprint. */
void TestMaskFalloff::brushMatchesProfile()
{
    const double radius = 40.0;
    QCOMPARE(BrushStamp::coverage(radius + 0.1, radius, 0.5), 0.0f);   // clipped at size
    QCOMPARE(BrushStamp::coverage(0.0, radius, 0.5), 1.0f);

    for (double d = 0.0; d < radius; d += 0.5)                   // feather 0 = flat disc
        QCOMPARE(BrushStamp::coverage(d, radius, 0.0), 1.0f);

    const MaskFalloff::Shape s = MaskFalloff::shapeFor(60.0);
    for (double d = 0.0; d < radius; d += 0.5)
        QVERIFY(std::abs(BrushStamp::coverage(d, radius, 0.6)
                         - MaskFalloff::coverage(d / radius, s)) < 1e-6f);
}

QTEST_MAIN(TestMaskFalloff)
#include "tst_maskfalloff.moc"
