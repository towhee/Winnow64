#include <QtTest>
#include <cmath>
#include "Develop/colorgrade.h"

/*
    Colour-grading math (Develop/colorgrade.h) -- the measured Lightroom model behind the
    Color Grade panel. These guard the properties the calibration established and that a
    later "tidy" could silently break: the push direction is zero-sum and points the right
    way, saturation scales it linearly, each window peaks in its own tonal region, and
    Blending / Balance are the identity at their defaults and move the windows the way
    Lightroom's sliders do.
*/
class tst_colorgrade : public QObject
{
    Q_OBJECT

    static float amp(int range, float L, float blending = 50.0f, float balance = 0.0f)
    {
        return ColorGrade::gradeWindowAmp(range, L, blending, balance);
    }

private slots:

    /* The direction is HSV - mean, so it sums to zero for every hue: it moves chroma,
       and the per-range lightness offset is the only thing that moves brightness. */
    void pushDirIsZeroSum_data()
    {
        QTest::addColumn<float>("hue");
        for (float h = 0.f; h < 360.f; h += 15.f)
            QTest::newRow(qPrintable(QString::number(h))) << h;
    }

    void pushDirIsZeroSum()
    {
        QFETCH(float, hue);
        float d[3];
        ColorGrade::gradePushDir(hue, d);
        QVERIFY2(std::fabs(d[0] + d[1] + d[2]) < 1e-5f,
                 qPrintable(QString("hue %1: sum %2").arg(hue).arg(d[0] + d[1] + d[2])));
    }

    /* Hue points the push the right way: red raises R and lowers G/B, cyan the reverse.
       A flipped conversion would swap warm and cool grades. */
    void hueDirection()
    {
        float red[3], cyan[3];
        ColorGrade::gradePushDir(0.0f,   red);
        ColorGrade::gradePushDir(180.0f, cyan);
        QVERIFY(red[0] > 0.0f && red[1] < 0.0f && red[2] < 0.0f);
        QVERIFY(cyan[0] < 0.0f && cyan[1] > 0.0f && cyan[2] > 0.0f);
    }

    /* The three primaries push equally hard: Lightroom's strength is hue-independent in
       the push space, which is what identified that space. */
    void primariesPushEqually()
    {
        float r[3], g[3], b[3];
        ColorGrade::gradePushDir(0.0f,   r);
        ColorGrade::gradePushDir(120.0f, g);
        ColorGrade::gradePushDir(240.0f, b);
        const float nr = std::hypot(std::hypot(r[0], r[1]), r[2]);
        const float ng = std::hypot(std::hypot(g[0], g[1]), g[2]);
        const float nb = std::hypot(std::hypot(b[0], b[1]), b[2]);
        QVERIFY(std::fabs(nr - ng) < 1e-5f && std::fabs(nr - nb) < 1e-5f);
    }

    /* Each window peaks in its own region and is small in the others, so a shadow tint
       stays off the highlights (the teal / orange split depends on it). */
    void windowsPeakInRegion()
    {
        QVERIFY(amp(0, 30.f) > amp(1, 30.f) && amp(0, 30.f) > amp(2, 30.f));
        QVERIFY(amp(1, 55.f) > amp(0, 55.f) && amp(1, 55.f) > amp(2, 55.f));
        QVERIFY(amp(2, 85.f) > amp(0, 85.f) && amp(2, 85.f) > amp(1, 85.f));
        QVERIFY(amp(2, 20.f) < 0.1f * amp(2, 85.f));
        QVERIFY(amp(0, 90.f) < 0.1f * amp(0, 30.f));
    }

    /* Global is its own broad window and is not tone-selective in the panel's sense:
       Blending / Balance shape the three tonal ranges, never Global. */
    void globalIgnoresBlendingAndBalance()
    {
        for (float L : {0.f, 25.f, 50.f, 75.f, 100.f})
            QCOMPARE(amp(3, L, 80.f, -40.f), amp(3, L));
    }

    /* Blending 50 / Balance 0 read the measured tables unwarped -- the calibration is
       only valid at the defaults if this holds. */
    void defaultsAreTheMeasuredWindows()
    {
        for (int r = 0; r < 4; ++r)
            for (int i = 0; i < ColorGrade::kWindowN; ++i)
                QCOMPARE(amp(r, 5.0f * i), ColorGrade::kWindow[r][i]);
    }

    /* The lightness offset is the measured default at 50 / 0 and grows (darkens) with
       Blending above 50 and with |Balance|, as Lightroom's does. */
    void offsetGrowsAwayFromDefaults()
    {
        for (int r = 0; r < 3; ++r) {
            const float base = ColorGrade::gradeLightOffset(r, 50.f, 0.f);
            QCOMPARE(base, ColorGrade::kLightOffset[r]);
            QVERIFY(ColorGrade::gradeLightOffset(r, 100.f, 0.f) > base);
            QVERIFY(ColorGrade::gradeLightOffset(r, 50.f, -100.f) > base);
            QVERIFY(ColorGrade::gradeLightOffset(r, 50.f, 100.f) > base);
        }
    }

    /* Balance +100 squeezes the midtone window down into the shadows (measured: it then
       peaks near L* 10 and is ~0 above L* 50). */
    void balanceSqueezesMidtonesIntoShadows()
    {
        QVERIFY(amp(1, 12.f, 50.f, 100.f) > amp(1, 12.f));
        QVERIFY(amp(1, 60.f, 50.f, 100.f) < 0.1f * amp(1, 60.f));
    }

    /* Blending above 50 widens the windows (more overlap), below narrows them: read away
       from a window's centre, high blending must give MORE amplitude than low. */
    void blendingWidensWindows()
    {
        QVERIFY(amp(1, 25.f, 100.f) > amp(1, 25.f, 0.f));
        QVERIFY(amp(1, 85.f, 100.f) > amp(1, 85.f, 0.f));
        QVERIFY(amp(0, 60.f, 100.f) > amp(0, 60.f, 0.f));
    }

    /* Balance in Lightroom's sense: positive favours the highlight range. */
    void balanceFavoursHighlights()
    {
        QVERIFY(amp(2, 60.f, 50.f, 100.f) > amp(2, 60.f, 50.f, -100.f));
        QVERIFY(amp(0, 40.f, 50.f, 100.f) < amp(0, 40.f, 50.f, -100.f));
    }

    /* The two matrices are inverses and both keep neutral neutral (rows sum to 1). */
    void matricesInvertAndHoldNeutral()
    {
        const float *a = ColorGrade::kRec2020ToProPhoto;
        const float *b = ColorGrade::kProPhotoToRec2020;
        for (int i = 0; i < 3; ++i) {
            QVERIFY(std::fabs(a[3 * i] + a[3 * i + 1] + a[3 * i + 2] - 1.0f) < 1e-5f);
            QVERIFY(std::fabs(b[3 * i] + b[3 * i + 1] + b[3 * i + 2] - 1.0f) < 1e-5f);
            for (int j = 0; j < 3; ++j) {
                float s = 0.0f;
                for (int k = 0; k < 3; ++k) s += b[3 * i + k] * a[3 * k + j];
                QVERIFY(std::fabs(s - (i == j ? 1.0f : 0.0f)) < 1e-5f);
            }
        }
    }

    /* L* endpoints and mid-grey (Y 0.184 -> L* ~50). */
    void lstar()
    {
        QCOMPARE(ColorGrade::lstarFromY(0.0f), 0.0f);
        QCOMPARE(ColorGrade::lstarFromY(1.0f), 100.0f);
        QVERIFY(std::fabs(ColorGrade::lstarFromY(0.1842f) - 50.0f) < 0.1f);
    }
};

QTEST_APPLESS_MAIN(tst_colorgrade)
#include "tst_colorgrade.moc"
