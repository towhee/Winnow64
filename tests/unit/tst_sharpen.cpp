#include <QtTest>
#include <cmath>
#include "Develop/sharpen.h"

/*
    Capture-sharpening math (Develop/sharpen.h) -- the shared kernel behind the Detail
    panel's Sharpening group. These guard the properties that are easy to break and hard
    to see in review:

      * amount 0 is an EXACT no-op, so an untouched image is bit-identical (the pipeline
        relies on this alongside EditParams::isIdentity),
      * Masking really does protect flat areas -- the whole point of the control,
      * Detail suppresses small amplitudes without ever flipping their sign,
      * and, uniquely for this op, the radius is ABSOLUTE pixels, so effectiveSigma must
        track the render scale. That is what keeps the proxy preview honest against the
        full-res settle render; every other Develop spatial op scales by max(w,h) and
        needs no such thing.
*/
class tst_sharpen : public QObject
{
    Q_OBJECT

private slots:

    /* Amount 0 must leave the pixel untouched, whatever the other controls say -- the
       op early-returns on it and isIdentity() counts only sharpenAmount. */
    void zeroAmountIsExactNoOp()
    {
        const float yp = 0.42f;
        for (float detail : {0.0f, 0.5f, 1.0f}) {
            for (float masking : {0.0f, 0.5f, 1.0f}) {
                const float out = Sharpen::applyPixel(yp, 0.30f, 0.5f,
                                                      0.0f, detail, masking, 1.0f);
                QCOMPARE(out, yp);
            }
        }
    }

    /* A pixel sitting exactly on its blurred base has no high-frequency content, so
       sharpening it must change nothing regardless of strength. */
    void flatPixelIsUnchanged()
    {
        const float yp = 0.5f;
        const float out = Sharpen::applyPixel(yp, yp, 0.0f, 1.5f, 1.0f, 0.0f, 1.0f);
        QVERIFY(std::fabs(out - yp) < 1e-6f);
    }

    /* Positive amount on a pixel brighter than its surround pushes it FURTHER up (and a
       darker one further down) -- that is what an unsharp mask does. */
    void sharpeningExpandsAboutTheBase()
    {
        const float base = 0.40f;
        const float bright =
            Sharpen::applyPixel(0.50f, base, 1.0f, 1.0f, 1.0f, 0.0f, 1.0f);
        QVERIFY(bright > 0.50f);
        const float dark =
            Sharpen::applyPixel(0.30f, base, 1.0f, 1.0f, 1.0f, 0.0f, 1.0f);
        QVERIFY(dark < 0.30f);
    }

    /* More amount = more effect, monotonically. */
    void strongerAmountSharpensMore()
    {
        float prev = 0.50f;
        for (float amt : {0.25f, 0.5f, 1.0f, 1.5f}) {
            const float out =
                Sharpen::applyPixel(0.50f, 0.40f, 1.0f, amt, 1.0f, 0.0f, 1.0f);
            QVERIFY(out > prev);
            prev = out;
        }
    }

    /* MASKING, the control that makes sharpening usable. At full masking a flat area
       (no gradient) must be left alone, while a strong edge still sharpens. */
    void maskingProtectsFlatAreasAndKeepsEdges()
    {
        const float yp = 0.50f, base = 0.40f;
        const float flat = Sharpen::applyPixel(yp, base, 0.0f, 1.0f, 1.0f, 1.0f, 1.0f);
        QVERIFY(std::fabs(flat - yp) < 1e-6f);          // noise in the sky: untouched

        const float edge = Sharpen::applyPixel(yp, base, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f);
        QVERIFY(edge > yp + 0.05f);                     // a real edge: sharpened
    }

    /* Masking 0 opens the gate everywhere, so a flat area IS sharpened -- the opposite
       end of the same control. */
    void zeroMaskingSharpensEverything()
    {
        const float yp = 0.50f;
        const float out = Sharpen::applyPixel(yp, 0.40f, 0.0f, 1.0f, 1.0f, 0.0f, 1.0f);
        QVERIFY(out > yp + 0.05f);
    }

    /* The gate is monotonic in edge strength: a stronger edge is never sharpened less. */
    void edgeGateRisesWithEdgeStrength()
    {
        float prev = -1.0f;
        for (float g : {0.0f, 0.02f, 0.05f, 0.1f, 0.5f}) {
            const float gate = Sharpen::edgeGate(g, 1.0f, 1.0f);
            QVERIFY(gate >= prev - 1e-6f);
            prev = gate;
        }
        QVERIFY(std::fabs(Sharpen::edgeGate(0.0f, 0.0f, 1.0f) - 1.0f) < 1e-6f);
    }

    /* DETAIL soft-thresholds small amplitudes. At detail 1 the high pass is untouched;
       lower values shrink a small amplitude but must never flip its sign, and must leave
       a large amplitude essentially alone. */
    void detailSuppressesSmallAmplitudesOnly()
    {
        const float small = 0.01f;
        QCOMPARE(Sharpen::shapeDetail(small, 1.0f, 1.0f), small);

        const float shaped = Sharpen::shapeDetail(small, 0.0f, 1.0f);
        QVERIFY(shaped < small);
        QVERIFY(shaped >= 0.0f);                        // sign preserved, not inverted

        const float negShaped = Sharpen::shapeDetail(-small, 0.0f, 1.0f);
        QVERIFY(negShaped > -small);
        QVERIFY(negShaped <= 0.0f);

        const float big = 0.9f;                         // well above the knee
        QVERIFY(std::fabs(Sharpen::shapeDetail(big, 0.0f, 1.0f) - big) < 1e-3f);
    }

    /* Detail is monotonic: raising it never suppresses MORE of a given amplitude. */
    void detailIsMonotonic()
    {
        float prev = -1.0f;
        for (float d : {0.0f, 0.25f, 0.5f, 0.75f, 1.0f}) {
            const float v = Sharpen::shapeDetail(0.03f, d, 1.0f);
            QVERIFY(v >= prev - 1e-6f);
            prev = v;
        }
    }

    /* THE SCALE-AWARE PART. sharpenRadius is absolute pixels, so a half-size proxy must
       use half the sigma -- this is what keeps the proxy preview honest against the
       full-res settle render. */
    void effectiveSigmaTracksRenderScale()
    {
        QVERIFY(std::fabs(Sharpen::effectiveSigma(2.0f, 1.0f) - 2.0f) < 1e-6f);
        QVERIFY(std::fabs(Sharpen::effectiveSigma(2.0f, 0.5f) - 1.0f) < 1e-6f);
        QVERIFY(std::fabs(Sharpen::effectiveSigma(3.0f, 0.25f) - 0.75f) < 1e-6f);
    }

    /* A tiny proxy must not collapse the kernel to nothing, or the slider looks dead
       during a drag. */
    void effectiveSigmaHasAFloor()
    {
        QVERIFY(Sharpen::effectiveSigma(0.5f, 0.01f) >= Sharpen::kMinSigma);
    }

    /* The radius is clamped to the range the panel offers, so a hand-edited sidecar
       cannot feed a 500 px blur to the pipeline (sanitizeParams clamps too -- this is
       the kernel's own guard). */
    void effectiveSigmaClampsRadius()
    {
        QCOMPARE(Sharpen::effectiveSigma(99.0f, 1.0f), Sharpen::kMaxRadius);
        QCOMPARE(Sharpen::effectiveSigma(0.01f, 1.0f), Sharpen::kMinRadius);
    }

    /* A degenerate render scale must not divide the sigma away or NaN the result. */
    void nonPositiveRenderScaleFallsBackToFullRes()
    {
        QCOMPARE(Sharpen::effectiveSigma(2.0f, 0.0f), 2.0f);
        QCOMPARE(Sharpen::effectiveSigma(2.0f, -1.0f), 2.0f);
    }

    /* The result feeds pow(s, gamma), so it must never go negative however hard a
       negative high pass pulls. */
    void resultIsNeverNegative()
    {
        const float out = Sharpen::applyPixel(0.01f, 0.99f, 1.0f, 1.5f, 1.0f, 0.0f, 1.0f);
        QVERIFY(out >= 0.0f);
    }
};

QTEST_APPLESS_MAIN(tst_sharpen)
#include "tst_sharpen.moc"
