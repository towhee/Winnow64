#include <QtTest>
#include <cmath>
#include "Develop/colorgrade.h"

/*
    Colour-grading math (Develop/colorgrade.h) -- the shared kernel behind the Color Mix
    panel's tonal-range tinting. These guard the two things easy to get subtly wrong and
    invisible in review: that a tint pushes CHROMA without shifting luma (so grading does
    not double as an exposure control), and that the tonal windows form a clean partition
    (weights sum to 1, each range peaks where it should) so shadows/mid/highlights stay
    independent. The strengths match the pipeline but the maths is strength-agnostic.
*/
class tst_colorgrade : public QObject
{
    Q_OBJECT

    static float luma(const float v[3])
    {
        return 0.2126f * v[0] + 0.7152f * v[1] + 0.0722f * v[2];
    }

private slots:

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
