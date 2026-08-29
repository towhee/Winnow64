#include <QtTest>
#include <cmath>
#include "Develop/colorgrade.h"
#include "Develop/colorspace.h"

/*
    Colour-grading math (Develop/colorgrade.h) -- the shared kernel behind the Color Grade
    panel's tonal-range tinting. These guard the two things easy to get subtly wrong and
    invisible in review: that a tint pushes CHROMA without shifting luma (so grading does
    not double as an exposure control), and that the tonal windows form a clean partition
    (weights sum to 1, each range peaks where it should) so shadows/mid/highlights stay
    independent. The strengths match the pipeline but the maths is strength-agnostic.
*/
class tst_colorgrade : public QObject
{
    Q_OBJECT

    /* WORKING-SPACE luma, shared with the code under test rather than restated here.
       gradeTintVector produces a push with zero luma AS THE WORKING SPACE DEFINES IT, so
       a test that measures with its own hardcoded Rec.709 triple is asserting against a
       different definition -- it agreed only while both were hardcoded, and broke the
       moment the working space widened to Rec.2020. Sharing the definition is the point
       of the assertion: the tint must not shift brightness in whatever space it runs in. */
    static float luma(const float v[3])
    {
        return ColorSpaceMath::luma(v[0], v[1], v[2]);
    }

    /* The pipeline's tuning constants, mirrored so the split-point tests can assert the
       exact values develop.cpp will feed in. Kept in sync by hand -- if they drift, the
       defaultBlendingMatchesLegacySplit test below is what catches it. */
    static constexpr float kBaseShadowEnd = 0.5f;
    static constexpr float kBaseHighStart = 0.5f;
    static constexpr float kBlendRange    = 0.30f;
    static constexpr float kBalanceRange  = 0.20f;
    static constexpr float kSplitLo       = 0.05f;
    static constexpr float kSplitHi       = 0.95f;

    static void splits(float blending, float balance, float &se, float &hs)
    {
        ColorGrade::gradeSplitPoints(blending, balance, kBaseShadowEnd, kBaseHighStart,
                                     kBlendRange, kBalanceRange, kSplitLo, kSplitHi,
                                     se, hs);
    }

private slots:

    /* THE compatibility guard. Blending 50 / Balance 0 are the defaults every image
       carries, including ones graded before these sliders existed. They must reproduce
       the fixed split the panel used then, EXACTLY -- anything else silently re-renders
       every previously graded photo the first time it is reopened. */
    void defaultBlendingMatchesLegacySplit()
    {
        float se, hs;
        splits(50.0f, 0.0f, se, hs);
        QCOMPARE(se, kBaseShadowEnd);
        QCOMPARE(hs, kBaseHighStart);
    }

    /* Blending below 50 pulls the edges APART (a wider pure-midtone band); above 50
       pushes them together into more overlap. */
    void blendingWidensAndNarrowsTheWindows()
    {
        float seLow, hsLow, seHigh, hsHigh;
        splits(0.0f,   0.0f, seLow,  hsLow);
        splits(100.0f, 0.0f, seHigh, hsHigh);
        QVERIFY(seLow  < kBaseShadowEnd);      // shadow window closes earlier
        QVERIFY(hsLow  > kBaseHighStart);      // highlight window opens later
        QVERIFY(seHigh > kBaseShadowEnd);      // and the reverse above 50
        QVERIFY(hsHigh < kBaseHighStart);
        QVERIFY(hsLow - seLow > hsHigh - seHigh);   // the gap really did shrink
    }

    /* Balance slides BOTH edges the same way. Lightroom's sense: positive favours the
       highlight range, so both splits move DOWN and more of the image reads as highlight.
       A sign flip here would invert the control, which is invisible in review. */
    void balanceSlidesTheSplitTowardHighlights()
    {
        float sePos, hsPos, seNeg, hsNeg;
        splits(50.0f,  100.0f, sePos, hsPos);
        splits(50.0f, -100.0f, seNeg, hsNeg);
        QVERIFY(sePos < kBaseShadowEnd);
        QVERIFY(hsPos < kBaseHighStart);
        QVERIFY(seNeg > kBaseShadowEnd);
        QVERIFY(hsNeg > kBaseHighStart);

        /* Confirm the CONSEQUENCE, not just the numbers: at positive balance a mid-grey
           pixel must carry more highlight weight than at negative. */
        float wS, wM, wH, wS2, wM2, wH2;
        ColorGrade::gradeTonalWeights(0.5f, sePos, hsPos, wS, wM, wH);
        ColorGrade::gradeTonalWeights(0.5f, seNeg, hsNeg, wS2, wM2, wH2);
        QVERIFY(wH > wH2);
    }

    /* The splits feed divisions inside gradeTonalWeights, so they must never reach 0 or
       1 however hard the two sliders are pushed. */
    void splitsStayInsideTheOpenInterval()
    {
        for (float b : {0.f, 25.f, 50.f, 75.f, 100.f}) {
            for (float bal : {-100.f, -50.f, 0.f, 50.f, 100.f}) {
                float se, hs;
                splits(b, bal, se, hs);
                QVERIFY(se >= kSplitLo && se <= kSplitHi);
                QVERIFY(hs >= kSplitLo && hs <= kSplitHi);
            }
        }
    }

    /* sat 0 is a no-op regardless of hue: an unset range must never tint. */
    void zeroSatIsNoTint()
    {
        for (float hue : {0.f, 90.f, 200.f, 359.f}) {
            float t[3];
            ColorGrade::gradeTintVector(hue, 0.0f, 0.5f, t);
            QCOMPARE(t[0], 0.0f);
            QCOMPARE(t[1], 0.0f);
            QCOMPARE(t[2], 0.0f);
        }
    }

    /* The tint carries no luminance: adding it to a pixel shifts colour, not brightness.
       (Rec.709 luma of the tint vector must be ~0 for every hue.) */
    void tintIsZeroLuma_data()
    {
        QTest::addColumn<float>("hue");
        for (float h = 0.f; h < 360.f; h += 15.f)
            QTest::newRow(qPrintable(QString::number(h))) << h;
    }

    void tintIsZeroLuma()
    {
        QFETCH(float, hue);
        float t[3];
        ColorGrade::gradeTintVector(hue, 1.0f, 0.5f, t);
        QVERIFY2(std::fabs(luma(t)) < 1e-4f,
                 qPrintable(QString("hue %1: tint luma %2").arg(hue).arg(luma(t))));
    }

    /* Hue points the tint the right way: red pushes R up and G/B down; the complementary
       cyan does the opposite. A flipped conversion would swap warm and cool grades. */
    void hueDirection()
    {
        float red[3], cyan[3];
        ColorGrade::gradeTintVector(0.0f,   1.0f, 0.5f, red);    // red
        ColorGrade::gradeTintVector(180.0f, 1.0f, 0.5f, cyan);   // cyan
        QVERIFY(red[0] > 0.0f && red[1] < 0.0f && red[2] < 0.0f);
        QVERIFY(cyan[0] < 0.0f && cyan[1] > 0.0f);
        QVERIFY(red[0] > cyan[0]);
    }

    /* Saturation scales the tint linearly: half sat = half the push. */
    void satScalesLinearly()
    {
        float full[3], half[3];
        ColorGrade::gradeTintVector(90.0f, 1.0f, 0.5f, full);
        ColorGrade::gradeTintVector(90.0f, 0.5f, 0.5f, half);
        for (int i = 0; i < 3; ++i)
            QVERIFY2(std::fabs(half[i] - full[i] * 0.5f) < 1e-5f,
                     qPrintable(QString("channel %1: %2 vs %3").arg(i).arg(half[i]).arg(full[i])));
    }

    /* The tonal windows partition tone: at every lightness the three weights are
       non-negative and sum to 1, so grading never gains or loses energy. */
    void weightsPartitionUnity_data()
    {
        QTest::addColumn<float>("L");
        for (float L = 0.f; L <= 1.0001f; L += 0.05f)
            QTest::newRow(qPrintable(QString::number(L, 'f', 2))) << L;
    }

    void weightsPartitionUnity()
    {
        QFETCH(float, L);
        float wS, wM, wH;
        ColorGrade::gradeTonalWeights(L, 0.5f, 0.5f, wS, wM, wH);
        QVERIFY(wS >= 0.0f && wM >= 0.0f && wH >= 0.0f);
        QVERIFY2(std::fabs(wS + wM + wH - 1.0f) < 1e-4f,
                 qPrintable(QString("L %1: sum %2").arg(L).arg(wS + wM + wH)));
    }

    /* Each window dominates its own tonal region: pure black is all-shadow, pure white
       all-highlight, and mid-grey leans midtone. This is what keeps a shadow tint off the
       highlights (the teal/orange split depends on it). */
    void weightsPeakInRegion()
    {
        float wS, wM, wH;
        ColorGrade::gradeTonalWeights(0.0f, 0.5f, 0.5f, wS, wM, wH);
        QVERIFY(wS > wM && wS > wH);          // black -> shadows
        ColorGrade::gradeTonalWeights(1.0f, 0.5f, 0.5f, wS, wM, wH);
        QVERIFY(wH > wM && wH > wS);          // white -> highlights
        ColorGrade::gradeTonalWeights(0.5f, 0.5f, 0.5f, wS, wM, wH);
        QVERIFY(wM >= wS && wM >= wH);        // mid-grey -> midtones
    }
};

QTEST_APPLESS_MAIN(tst_colorgrade)
#include "tst_colorgrade.moc"
