#include <QtTest>
#include <cmath>
#include "Develop/tonecurve.h"

/*
    Tone-curve math (Develop/tonecurve.h) -- the spline behind the Curves panel. These
    guard the properties that are easy to break and invisible in review:

      * the 2-point diagonal really is the identity, so an untouched image is bit-
        identical and Develop::buildPointCoeffs can skip the curve entirely;
      * MONOTONICITY. Plain cubic interpolation overshoots between close control points,
        and an overshoot in a tone curve is a locally inverted tone -- a bright halo
        inside a shadow ramp. Fritsch-Carlson exists solely to prevent that, so if this
        regresses the whole choice of interpolation has been undone;
      * the encode/decode round trip, because it is the ONE encoding shared by the
        sidecar, the presets and the checklist collector -- a drift there silently loses
        a user's curve on the next folder open;
      * sanitize()'s repair of the states a hand-edited sidecar can produce.
*/
class tst_tonecurve : public QObject
{
    Q_OBJECT

    static ToneCurve::Spline diagonal()
    {
        const float x[2] = {0.0f, 1.0f};
        const float y[2] = {0.0f, 1.0f};
        ToneCurve::Spline s;
        s.build(x, y, 2);
        return s;
    }

private slots:

    /* The diagonal is the identity over the WHOLE sampled domain, including the
       highlight headroom above 1.0 that the tone LUT spans. */
    void diagonalIsIdentity()
    {
        const float x[2] = {0.0f, 1.0f};
        const float y[2] = {0.0f, 1.0f};
        QVERIFY(ToneCurve::isIdentity(x, y, 2));
        const ToneCurve::Spline s = diagonal();
        QVERIFY(s.identity);
        for (int i = 0; i <= 40; ++i) {
            const float v = i * 0.08f;                  // 0 .. 3.2, past white
            QVERIFY(std::fabs(s.eval(v) - v) < 1e-5f);
        }
    }

    /* A curve that moves is NOT reported as identity -- the gate that decides whether
       the pipeline builds a tone LUT at all. */
    void movedCurveIsNotIdentity()
    {
        const float x[3] = {0.0f, 0.5f, 1.0f};
        const float y[3] = {0.0f, 0.6f, 1.0f};
        QVERIFY(!ToneCurve::isIdentity(x, y, 3));
        ToneCurve::Spline s;
        s.build(x, y, 3);
        QVERIFY(!s.identity);
        QVERIFY(s.eval(0.5f) > 0.55f);
    }

    /* THE point of Fritsch-Carlson. A monotone but awkwardly spaced control set (a long
       flat run then a jump) is exactly what makes a plain cubic overshoot; the output
       must never go backwards, and must never leave the control points' own range. */
    void monotoneControlPointsGiveMonotoneCurve()
    {
        const float x[5] = {0.0f, 0.30f, 0.32f, 0.34f, 1.0f};
        const float y[5] = {0.0f, 0.10f, 0.10f, 0.80f, 1.0f};
        ToneCurve::Spline s;
        s.build(x, y, 5);
        float prev = s.eval(0.0f);
        for (int i = 1; i <= 1000; ++i) {
            const float v = s.eval(i / 1000.0f);
            QVERIFY2(v >= prev - 1e-5f, "tone curve went backwards (overshoot)");
            QVERIFY(v >= -1e-5f && v <= 1.0f + 1e-5f);
            prev = v;
        }
    }

    /* The curve passes through every control point it was given. */
    void interpolatesControlPoints()
    {
        const float x[4] = {0.0f, 0.25f, 0.75f, 1.0f};
        const float y[4] = {0.05f, 0.15f, 0.90f, 0.95f};
        ToneCurve::Spline s;
        s.build(x, y, 4);
        for (int i = 0; i < 4; ++i)
            QVERIFY(std::fabs(s.eval(x[i]) - y[i]) < 1e-5f);
    }

    /* Beyond x = 1 the curve continues with its END SLOPE rather than clamping, so
       pulling the white point down scales the ~3 stops of highlight headroom in the LUT
       instead of flattening it all into one value. */
    void extrapolatesPastWhiteWithEndSlope()
    {
        const float x[2] = {0.0f, 1.0f};
        const float y[2] = {0.0f, 0.5f};               // white point pulled down
        ToneCurve::Spline s;
        s.build(x, y, 2);
        QVERIFY(std::fabs(s.eval(1.0f) - 0.5f) < 1e-5f);
        QVERIFY(s.eval(2.0f) > s.eval(1.0f));          // still rising, not clamped
        QVERIFY(std::fabs(s.eval(2.0f) - 1.0f) < 1e-4f);   // slope 0.5 continued
    }

    /* The shared encoding: identity encodes to nothing (so an untouched image adds no
       sidecar weight), and a real curve survives the round trip on every channel. */
    void encodeDecodeRoundTrip()
    {
        int n[ToneCurve::kChannels];
        float x[ToneCurve::kChannels][ToneCurve::kMaxPts];
        float y[ToneCurve::kChannels][ToneCurve::kMaxPts];
        for (int c = 0; c < ToneCurve::kChannels; ++c)
            ToneCurve::setIdentity(n[c], x[c], y[c]);
        QCOMPARE(ToneCurve::encode(n, x, y), QString());

        n[0] = 3; x[0][0] = 0.0f; y[0][0] = 0.02f;
                  x[0][1] = 0.4f; y[0][1] = 0.35f;
                  x[0][2] = 1.0f; y[0][2] = 0.98f;
        n[2] = 2; x[2][0] = 0.0f; y[2][0] = 0.1f;
                  x[2][1] = 1.0f; y[2][1] = 0.9f;
        const QString enc = ToneCurve::encode(n, x, y);
        QVERIFY(!enc.isEmpty());

        int n2[ToneCurve::kChannels];
        float x2[ToneCurve::kChannels][ToneCurve::kMaxPts];
        float y2[ToneCurve::kChannels][ToneCurve::kMaxPts];
        QVERIFY(ToneCurve::decode(enc, n2, x2, y2));
        for (int c = 0; c < ToneCurve::kChannels; ++c) {
            QCOMPARE(n2[c], n[c]);
            for (int i = 0; i < n[c]; ++i) {
                QVERIFY(std::fabs(x2[c][i] - x[c][i]) < 1e-4f);
                QVERIFY(std::fabs(y2[c][i] - y[c][i]) < 1e-4f);
            }
        }
    }

    /* An empty string is a valid encoding (every channel identity), not a parse error. */
    void decodeEmptyGivesIdentity()
    {
        int n[ToneCurve::kChannels];
        float x[ToneCurve::kChannels][ToneCurve::kMaxPts];
        float y[ToneCurve::kChannels][ToneCurve::kMaxPts];
        for (int c = 0; c < ToneCurve::kChannels; ++c) { n[c] = 7; x[c][0] = 0.5f; }
        QVERIFY(ToneCurve::decode(QString(), n, x, y));
        for (int c = 0; c < ToneCurve::kChannels; ++c)
            QVERIFY(ToneCurve::isIdentity(x[c], y[c], n[c]));
    }

    /* Garbage in a hand-edited sidecar must degrade to the diagonal, never to a curve
       that renders something arbitrary. */
    void decodeRejectsMalformed()
    {
        int n[ToneCurve::kChannels];
        float x[ToneCurve::kChannels][ToneCurve::kMaxPts];
        float y[ToneCurve::kChannels][ToneCurve::kMaxPts];
        QVERIFY(!ToneCurve::decode("0:0,0,1", n, x, y));            // odd value count
        QVERIFY(ToneCurve::isIdentity(x[0], y[0], n[0]));
        QVERIFY(!ToneCurve::decode("9:0,0,1,1", n, x, y));           // no such channel
        QVERIFY(!ToneCurve::decode("nonsense", n, x, y));
    }

    /* sanitize repairs what it can (clamping, re-pinning the endpoints) and falls back
       to the diagonal for what it cannot (unordered x, too few points). */
    void sanitizeRepairsOrRevertsToDiagonal()
    {
        {   // out of range + unpinned endpoints: repaired in place
            int n = 3;
            float x[ToneCurve::kMaxPts] = {0.1f, 0.5f, 0.9f};
            float y[ToneCurve::kMaxPts] = {-0.2f, 0.5f, 1.4f};
            QVERIFY(ToneCurve::sanitize(n, x, y));
            QCOMPARE(n, 3);
            QCOMPARE(x[0], 0.0f);
            QCOMPARE(x[2], 1.0f);
            QCOMPARE(y[0], 0.0f);
            QCOMPARE(y[2], 1.0f);
        }
        {   // x not increasing: cannot be guessed, so revert
            int n = 3;
            float x[ToneCurve::kMaxPts] = {0.0f, 0.8f, 0.4f};
            float y[ToneCurve::kMaxPts] = {0.0f, 0.5f, 1.0f};
            QVERIFY(ToneCurve::sanitize(n, x, y));
            QVERIFY(ToneCurve::isIdentity(x, y, n));
        }
        {   // too few points: revert
            int n = 1;
            float x[ToneCurve::kMaxPts] = {0.5f};
            float y[ToneCurve::kMaxPts] = {0.5f};
            QVERIFY(ToneCurve::sanitize(n, x, y));
            QVERIFY(ToneCurve::isIdentity(x, y, n));
        }
        {   // already valid: left alone
            int n = 2;
            float x[ToneCurve::kMaxPts] = {0.0f, 1.0f};
            float y[ToneCurve::kMaxPts] = {0.0f, 1.0f};
            QVERIFY(!ToneCurve::sanitize(n, x, y));
        }
    }
};

QTEST_APPLESS_MAIN(tst_tonecurve)
#include "tst_tonecurve.moc"
