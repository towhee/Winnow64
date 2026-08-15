#include <QtTest>
#include <cmath>
#include "Develop/calibrate.h"

/*
    Camera-calibration math (Develop/calibrate.h) -- the primary-rotation kernel behind
    the Calibrate panel. These guard the properties easy to break and invisible in
    review: that all-zero really is the identity (so an untouched image is bit-identical
    and the pipeline can skip the stage), and above all that NEUTRALS STAY NEUTRAL. The
    whole point of rotating about the (1,1,1) axis rather than scaling channels is that
    grey must not tint -- if that regresses, every calibrated image gets a cast that no
    white-balance setting can remove.
*/
class tst_calibrate : public QObject
{
    Q_OBJECT

    /* Apply a row-major 3x3 to an RGB triple. */
    static void apply(const float m[9], const float in[3], float out[3])
    {
        out[0] = m[0] * in[0] + m[1] * in[1] + m[2] * in[2];
        out[1] = m[3] * in[0] + m[4] * in[1] + m[5] * in[2];
        out[2] = m[6] * in[0] + m[7] * in[1] + m[8] * in[2];
    }

    /* Rec.709 luma -- NOT what the calibration maths preserves (it preserves the
       equal-weight sum), which is exactly what one of the tests below pins down. */
    static float luma(const float v[3])
    {
        return 0.2126f * v[0] + 0.7152f * v[1] + 0.0722f * v[2];
    }

private slots:

    /* All six at zero must produce the exact identity, which is what lets
       Develop::buildPointCoeffs skip the block (calActive == false). */
    void zeroIsIdentity()
    {
        QVERIFY(Calibrate::isIdentity(0, 0, 0, 0, 0, 0));
        float m[9];
        Calibrate::buildMatrix(0, 0, 0, 0, 0, 0, m);
        const float expect[9] = {1, 0, 0, 0, 1, 0, 0, 0, 1};
        for (int i = 0; i < 9; ++i)
            QCOMPARE(m[i], expect[i]);
    }

    /* Any non-zero slider must report active, so the stage is not skipped by mistake. */
    void nonZeroIsActive()
    {
        QVERIFY(!Calibrate::isIdentity(1, 0, 0, 0, 0, 0));
        QVERIFY(!Calibrate::isIdentity(0, 0, 0, 0, 0, -1));
    }

    /* THE load-bearing property: grey in, the SAME grey out, for every combination of
       primary hue and saturation. The matrix columns are built from rotations about the
       neutral axis and luma-preserving chroma scales, so the columns must sum to (1,1,1).
       A regression here tints every neutral in the image. */
    void neutralsStayNeutral()
    {
        const float settings[][6] = {
            { 100,   0,    0,   0,    0,   0},
            {   0, 100,    0,   0,    0,   0},
            { -80,  60,   40, -30,   70, -90},
            { 100, 100,  100, 100,  100, 100},
            {-100,-100, -100,-100, -100,-100},
        };
        for (const auto &s : settings) {
            float m[9];
            Calibrate::buildMatrix(s[0], s[1], s[2], s[3], s[4], s[5], m);
            for (float grey : {0.05f, 0.18f, 0.5f, 1.0f}) {
                const float in[3] = {grey, grey, grey};
                float out[3];
                apply(m, in, out);
                QVERIFY(std::fabs(out[0] - grey) < 1e-5f);
                QVERIFY(std::fabs(out[1] - grey) < 1e-5f);
                QVERIFY(std::fabs(out[2] - grey) < 1e-5f);
            }
        }
    }

    /* A rotation about the (1,1,1) axis preserves the component ALONG that axis -- the
       equal-weight sum r+g+b. It does NOT preserve Rec.709 luma, which is weighted
       differently; conflating the two is the easy mistake, so pin both directions. */
    void hueShiftPreservesNeutralComponent()
    {
        const float red[3] = {1, 0, 0};
        float rot[3];
        Calibrate::rotateAboutNeutral(red, 0.4f, rot);
        const float sumIn  = red[0] + red[1] + red[2];
        const float sumOut = rot[0] + rot[1] + rot[2];
        QVERIFY(std::fabs(sumOut - sumIn) < 1e-5f);
        QVERIFY(std::fabs(luma(rot) - luma(red)) > 1e-3f);   // Rec.709 luma DOES move
    }

    /* Saturation scales chroma about the primary's own Rec.709 luma, so it must not move
       that luma -- and at scale 0 the result is the neutral grey of that luma. */
    void satScaleIsLumaPreserving()
    {
        float v[3] = {0.8f, 0.3f, 0.1f};
        const float y0 = luma(v);
        Calibrate::scaleChroma(v, 1.7f);
        QVERIFY(std::fabs(luma(v) - y0) < 1e-5f);

        float w[3] = {0.8f, 0.3f, 0.1f};
        const float yw = luma(w);
        Calibrate::scaleChroma(w, 0.0f);
        QVERIFY(std::fabs(w[0] - yw) < 1e-5f);
        QVERIFY(std::fabs(w[1] - yw) < 1e-5f);
        QVERIFY(std::fabs(w[2] - yw) < 1e-5f);
    }

    /* Every ROW sums to 1 -- the invariant white preservation actually rests on. Pinned
       separately from neutralsStayNeutral so a regression names the cause, and because it
       is easy to mistake this for the COLUMN sums, which prove nothing about white. */
    void rowsSumToOne()
    {
        float m[9];
        Calibrate::buildMatrix(-80, 60, 40, -30, 70, -90, m);
        for (int r = 0; r < 3; ++r) {
            const float s = m[r * 3 + 0] + m[r * 3 + 1] + m[r * 3 + 2];
            QVERIFY(std::fabs(s - 1.0f) < 1e-5f);
        }
    }

    /* A positive red hue must actually MOVE red, and move it the way the HSL hue slider
       turns (anticlockwise about neutral: red gains green). Guards a sign flip that would
       silently invert the control. Note red's own channel stays at 1: with only red set,
       row 0 is (k,0,0) and normalises to (1,0,0), so the shift shows up in G and B. */
    void redHueMovesRedTowardGreen()
    {
        float m[9];
        Calibrate::buildMatrix(100, 0, 0, 0, 0, 0, m);
        const float in[3] = {1, 0, 0};
        float out[3];
        apply(m, in, out);
        QVERIFY(out[1] > 0.01f);            // red picked up a green component
        QVERIFY(out[2] < -0.01f);           // and shed blue, the opposite way round
    }

    /* An untouched primary keeps its DIRECTION -- only its magnitude is rescaled by the
       white-preserving normalisation. The columns are not fully independent (they cannot
       be, see calibrate.h), but a red-only setting must not swing green off its own axis
       into some other hue. */
    void untouchedPrimaryKeepsItsDirection()
    {
        float m[9];
        Calibrate::buildMatrix(75, -40, 0, 0, 0, 0, m);
        QCOMPARE(m[0 * 3 + 1], 0.0f);       // green column still lies along green
        QCOMPARE(m[2 * 3 + 1], 0.0f);
        QVERIFY(m[1 * 3 + 1] > 0.0f);
        QCOMPARE(m[0 * 3 + 2], 0.0f);       // blue column still lies along blue
        QCOMPARE(m[1 * 3 + 2], 0.0f);
        QVERIFY(m[2 * 3 + 2] > 0.0f);
    }

    /* Red's own column must still move MOST when only red is adjusted, or the control
       does not read as a red control. */
    void redSliderMovesRedMost()
    {
        float m[9];
        Calibrate::buildMatrix(75, -40, 0, 0, 0, 0, m);
        const float ident[9] = {1, 0, 0, 0, 1, 0, 0, 0, 1};
        auto colShift = [&](int c) {
            float d = 0.0f;
            for (int r = 0; r < 3; ++r)
                d += std::fabs(m[r * 3 + c] - ident[r * 3 + c]);
            return d;
        };
        const float dR = colShift(0), dG = colShift(1), dB = colShift(2);
        QVERIFY(dR > 0.05f);
        QVERIFY(dG < dR);
        QVERIFY(dB < dR);
    }
};

QTEST_APPLESS_MAIN(tst_calibrate)
#include "tst_calibrate.moc"
