#include <QtTest>
#include <cmath>
#include <vector>
#include "Develop/develop.h"
#include "Develop/workingimage.h"
#include "Develop/localcontrast.h"

/*
    CHARACTERIZATION TEST for the local-contrast family, plus unit tests for the shared
    kernel in Develop/localcontrast.h.

    WHY THIS EXISTS. Texture, Clarity and Dehaze-stage-1 are the same band operation at
    three radii, and they were extracted onto one shared kernel + one shared
    perceptual-luma pass. That refactor rewrote ops that are already shaping users' saved
    edits, so the golden values below were captured from the build IMMEDIATELY BEFORE
    the extraction, and pin what Texture and Dehaze rendered then.

    If one of these fails, a develop recipe someone already saved now renders differently.
    That is the whole point of the test -- do not re-bless the numbers to make it pass
    without first proving the change is intentional and desirable.

    TOLERANCE. Texture kept its exact expression and should be bit-identical. Dehaze was
    re-expressed from `yp + k*(yp-base)` to the shared `base + (1+k)*(yp-base)`: equal
    in exact arithmetic, ~1e-7 apart in float. kTol sits well below one 8-bit code value
    (1/255 = 0.0039) so a real behavioural change still fails loudly.
*/
namespace {

constexpr int   kW   = 96;
constexpr int   kH   = 96;
constexpr float kTol = 1e-5f;

/* Sample points across the ramp, hard edge, flat patch and checker. */
const int kSX[6] = {3, 17, 41, 58, 73, 89};
const int kSY[6] = {5, 23, 44, 61, 70, 88};

/* Deterministic synthetic scene: a diagonal luminance ramp, a hard vertical edge (halo
   bait), a flat patch (where midtone weighting and masking show), and a fine checker (the
   high-frequency band). Scene-linear with white = 1.0, as InputTransform hands Develop.
   MUST match the generator that produced the goldens -- changing it invalidates them. */
WorkingImage makeScene()
{
    WorkingImage img;
    img.width = kW; img.height = kH; img.white = 1.0f; img.sceneReferred = false;
    img.rgb.resize(static_cast<size_t>(kW) * kH * 3);
    for (int y = 0; y < kH; ++y) {
        for (int x = 0; x < kW; ++x) {
            float v = 0.15f + 0.55f * (float(x) / (kW - 1)) * 0.5f
                            + 0.55f * (float(y) / (kH - 1)) * 0.5f;
            if (x > kW * 3 / 5) v += 0.25f;                  // hard edge
            if (y > kH / 2 && y < kH * 3 / 5) v = 0.42f;     // flat patch
            if (((x / 2) + (y / 2)) % 2 == 0) v += 0.02f;    // fine checker
            const size_t i = (static_cast<size_t>(y) * kW + x) * 3;
            img.rgb[i + 0] = v * 1.00f;                      // slight warm tint, so a
            img.rgb[i + 1] = v * 0.93f;                      // ratio-preserving op can be
            img.rgb[i + 2] = v * 0.86f;                      // told from one that is not
        }
    }
    return img;
}

/* The 18 sampled channel values (6 points x RGB) after applying p. */
std::vector<float> renderSamples(const EditParams &p)
{
    WorkingImage img = makeScene();
    Develop d;
    d.Apply(img, p, nullptr);
    std::vector<float> out;
    out.reserve(18);
    for (int s = 0; s < 6; ++s) {
        const size_t i = (static_cast<size_t>(kSY[s]) * kW + kSX[s]) * 3;
        for (int c = 0; c < 3; ++c) out.push_back(img.rgb[i + c]);
    }
    return out;
}

static const float k_texturePos[18] = {
    0.165306434f, 0.153734997f, 0.142163545f,
    0.257810056f, 0.239763364f, 0.221716657f,
    0.424456209f, 0.394744277f, 0.365032345f,
    0.812490404f, 0.755616069f, 0.698741794f,
    0.805792689f, 0.749387205f, 0.692981720f,
    0.940678418f, 0.874830961f, 0.808983445f
};
static const float k_textureNeg[18] = {
    0.180304125f, 0.167682841f, 0.155061558f,
    0.272993803f, 0.253884226f, 0.234774664f,
    0.408659697f, 0.380053490f, 0.351447344f,
    0.686726868f, 0.638655961f, 0.590585113f,
    0.821233749f, 0.763747394f, 0.706261039f,
    0.925015569f, 0.860264480f, 0.795513451f
};
static const float k_textureFull[18] = {
    0.160182357f, 0.148969591f, 0.137756824f,
    0.252564073f, 0.234884605f, 0.217205122f,
    0.430109620f, 0.400001943f, 0.369894296f,
    0.859642029f, 0.799467087f, 0.739292204f,
    0.800380945f, 0.744354248f, 0.688327610f,
    0.946240783f, 0.880003929f, 0.813767076f
};
static const float k_dehazePos[18] = {
    0.193408296f, 0.177231044f, 0.161053777f,
    0.317541808f, 0.290981650f, 0.264421463f,
    0.543248236f, 0.497809231f, 0.452370346f,
    1.101879716f, 1.009715080f, 0.917550564f,
    1.087630033f, 0.996657312f, 0.905684590f,
    1.276904106f, 1.170099974f, 1.063295722f
};
static const float k_dehazeNeg[18] = {
    0.157959655f, 0.148475453f, 0.138991252f,
    0.230600342f, 0.216754660f, 0.202908963f,
    0.338641763f, 0.318309069f, 0.297976345f,
    0.552228630f, 0.519071758f, 0.485914856f,
    0.644563079f, 0.605862260f, 0.567161441f,
    0.725261450f, 0.681715369f, 0.638169289f
};
static const float k_dehazeFull[18] = {
    0.201607227f, 0.183586270f, 0.165565327f,
    0.340464264f, 0.310031414f, 0.279598534f,
    0.605010033f, 0.550930321f, 0.496850669f,
    1.292436719f, 1.176910400f, 1.061384201f,
    1.218193531f, 1.109303474f, 1.000413537f,
    1.445190907f, 1.316010594f, 1.186830163f
};
static const float k_combined[18] = {
    0.174479425f, 0.161068186f, 0.147656932f,
    0.282020152f, 0.260342836f, 0.238665521f,
    0.486838132f, 0.449417621f, 0.411997169f,
    0.994133651f, 0.917720199f, 0.841306746f,
    0.935580492f, 0.863667727f, 0.791754901f,
    1.106494188f, 1.021444201f, 0.936394215f
};

/* Sharpen (#8.5) op-level goldens. tst_sharpen covers the header kernel; these pin the
   OP -- its prep, blur, Sobel gate and fold-back -- which the shared-pass extraction
   rewrote. Captured after that extraction was verified bit-identical against a
   reconstructed pre-refactor build. */
static const float k_sharpenMid[18] = {
    0.172241122f, 0.160184249f, 0.148127362f,
    0.265172720f, 0.246610612f, 0.228048533f,
    0.416487902f, 0.387333751f, 0.358179599f,
    0.801082492f, 0.745006740f, 0.688930988f,
    0.813744545f, 0.756782413f, 0.699820280f,
    0.932551444f, 0.867272854f, 0.801994264f
};
static const float k_sharpenMasked[18] = {
    0.172872394f, 0.160771325f, 0.148670271f,
    0.265599906f, 0.247007906f, 0.228415906f,
    0.416184813f, 0.387051880f, 0.357918948f,
    0.791099072f, 0.735722125f, 0.680345237f,
    0.813886225f, 0.756914139f, 0.699942112f,
    0.932423532f, 0.867153883f, 0.801884234f
};
static const float k_sharpenFull[18] = {
    0.165064991f, 0.153510436f, 0.141955897f,
    0.257648319f, 0.239612952f, 0.221577570f,
    0.424546391f, 0.394828141f, 0.365109891f,
    0.859642029f, 0.799467087f, 0.739292204f,
    0.807041585f, 0.750548661f, 0.694055796f,
    0.938959301f, 0.873232186f, 0.807505012f
};

/* Clarity (#6.5) op-level goldens. */
static const float k_clarityPos[18] = {
    0.169341519f, 0.157487616f, 0.145633712f,
    0.261891454f, 0.243559048f, 0.225226656f,
    0.419946074f, 0.390549839f, 0.361153632f,
    0.766401172f, 0.712753057f, 0.659105003f,
    0.811404407f, 0.754606128f, 0.697807789f,
    0.934266806f, 0.868868113f, 0.803469479f
};
static const float k_clarityNeg[18] = {
    0.178082317f, 0.165616557f, 0.153150797f,
    0.270796061f, 0.251840323f, 0.232884616f,
    0.411125898f, 0.382347077f, 0.353568286f,
    0.717067420f, 0.666872680f, 0.616677999f,
    0.817190289f, 0.759986997f, 0.702783644f,
    0.929955304f, 0.864858449f, 0.799761593f
};
static const float k_clarityFull[18] = {
    0.166253492f, 0.154615745f, 0.142977998f,
    0.258725464f, 0.240614682f, 0.222503901f,
    0.423146337f, 0.393526107f, 0.363905877f,
    0.784600198f, 0.729678154f, 0.674756229f,
    0.809327066f, 0.752674162f, 0.696021259f,
    0.935821533f, 0.870314002f, 0.804806530f
};

}   // namespace

class tst_localcontrast : public QObject
{
    Q_OBJECT

    /* Compare a render against its golden, reporting the first offending channel. */
    static void checkAgainstGolden(const char *label, const EditParams &p,
                                   const float *golden)
    {
        const std::vector<float> got = renderSamples(p);
        QCOMPARE(int(got.size()), 18);
        for (int i = 0; i < 18; ++i) {
            if (std::fabs(got[i] - golden[i]) > kTol) {
                QFAIL(qPrintable(QStringLiteral(
                    "%1: sample %2 drifted %3 -> %4 (delta %5, tol %6). A saved develop "
                    "recipe now renders differently.")
                    .arg(label).arg(i)
                    .arg(double(golden[i]), 0, 'g', 9)
                    .arg(double(got[i]), 0, 'g', 9)
                    .arg(double(got[i] - golden[i]), 0, 'g', 3)
                    .arg(double(kTol))));
            }
        }
    }

private slots:

    /* GUARD. If the scene or the sampling ever degenerated so that an edit changed
       nothing at the sample points, every characterization test below would pass
       vacuously. Assert the goldens are genuinely far apart FIRST, so a green run
       means something. */
    void goldenCasesAreDistinct()
    {
        struct Row { const char *name; const float *v; };
        const Row rows[] = {
            {"texturePos", k_texturePos}, {"textureNeg", k_textureNeg},
            {"textureFull", k_textureFull}, {"dehazePos", k_dehazePos},
            {"dehazeNeg", k_dehazeNeg}, {"dehazeFull", k_dehazeFull},
            {"combined", k_combined}, {"sharpenMid", k_sharpenMid},
            {"sharpenMasked", k_sharpenMasked}, {"sharpenFull", k_sharpenFull},
            {"clarityPos", k_clarityPos}, {"clarityNeg", k_clarityNeg},
            {"clarityFull", k_clarityFull},
        };
        const int n = int(sizeof(rows) / sizeof(Row));
        for (int a = 0; a < n; ++a) {
            for (int b = a + 1; b < n; ++b) {
                float worst = 0.0f;
                for (int i = 0; i < 18; ++i)
                    worst = std::max(worst, std::fabs(rows[a].v[i] - rows[b].v[i]));
                QVERIFY2(worst > 100.0f * kTol,
                         qPrintable(QStringLiteral("%1 vs %2 differ by only %3")
                             .arg(rows[a].name).arg(rows[b].name).arg(double(worst))));
            }
        }
    }

    /* An identity recipe must leave the scene untouched -- Apply early-returns on
       isIdentity(), and every op early-returns at 0. */
    void identityIsExactNoOp()
    {
        const WorkingImage before = makeScene();
        WorkingImage after = makeScene();
        Develop d;
        EditParams p;
        d.Apply(after, p, nullptr);
        for (size_t i = 0; i < before.rgb.size(); ++i)
            QCOMPARE(after.rgb[i], before.rgb[i]);
    }

    void texturePositiveMatchesGolden()
    {
        EditParams p; p.texture = 60.0f;
        checkAgainstGolden("texturePos", p, k_texturePos);
    }

    void textureNegativeMatchesGolden()
    {
        EditParams p; p.texture = -80.0f;
        checkAgainstGolden("textureNeg", p, k_textureNeg);
    }

    void textureFullMatchesGolden()
    {
        EditParams p; p.texture = 100.0f;
        checkAgainstGolden("textureFull", p, k_textureFull);
    }

    void dehazePositiveMatchesGolden()
    {
        EditParams p; p.dehaze = 70.0f;
        checkAgainstGolden("dehazePos", p, k_dehazePos);
    }

    void dehazeNegativeMatchesGolden()
    {
        EditParams p; p.dehaze = -50.0f;
        checkAgainstGolden("dehazeNeg", p, k_dehazeNeg);
    }

    void dehazeFullMatchesGolden()
    {
        EditParams p; p.dehaze = 100.0f;
        checkAgainstGolden("dehazeFull", p, k_dehazeFull);
    }

    void combinedMatchesGolden()
    {
        EditParams p; p.texture = 45.0f; p.dehaze = 35.0f;
        checkAgainstGolden("combined", p, k_combined);
    }

    void sharpenMidMatchesGolden()
    {
        EditParams p; p.sharpenAmount = 0.8f; p.sharpenDetail = 0.25f;
        checkAgainstGolden("sharpenMid", p, k_sharpenMid);
    }

    void sharpenMaskedMatchesGolden()
    {
        EditParams p; p.sharpenAmount = 0.8f; p.sharpenMasking = 0.7f;
        p.sharpenDetail = 0.25f;
        checkAgainstGolden("sharpenMasked", p, k_sharpenMasked);
    }

    void sharpenFullMatchesGolden()
    {
        EditParams p; p.sharpenAmount = 1.5f; p.sharpenMasking = 0.4f;
        p.sharpenDetail = 0.9f;
        checkAgainstGolden("sharpenFull", p, k_sharpenFull);
    }

    void clarityPositiveMatchesGolden()
    {
        EditParams p; p.clarity = 55.0f;
        checkAgainstGolden("clarityPos", p, k_clarityPos);
    }

    void clarityNegativeMatchesGolden()
    {
        EditParams p; p.clarity = -70.0f;
        checkAgainstGolden("clarityNeg", p, k_clarityNeg);
    }

    void clarityFullMatchesGolden()
    {
        EditParams p; p.clarity = 100.0f;
        checkAgainstGolden("clarityFull", p, k_clarityFull);
    }

    /* ---- the shared kernel (Develop/localcontrast.h) ------------------------------ */

    /* factor 1 is a no-op, 0 collapses onto the base, 2 doubles the excursion. */
    void applyBandScalesAboutTheBase()
    {
        QCOMPARE(LocalContrast::applyBand(0.7f, 0.5f, 1.0f), 0.7f);
        QCOMPARE(LocalContrast::applyBand(0.7f, 0.5f, 0.0f), 0.5f);
        QCOMPARE(LocalContrast::applyBand(0.7f, 0.5f, 2.0f), 0.9f);
        QCOMPARE(LocalContrast::applyBand(0.3f, 0.5f, 2.0f), 0.1f);
    }

    /* amount 0 must be exactly neutral, and a full negative must land ON the base
       rather than inverting the band into negative detail. */
    void bandFactorIsNeutralAtZeroAndClampsBelow()
    {
        QCOMPARE(LocalContrast::bandFactor(0.0f, 1.5f), 1.0f);
        QCOMPARE(LocalContrast::bandFactor(-1.0f, 1.0f), 0.0f);
        QCOMPARE(LocalContrast::bandFactor(-5.0f, 1.0f), 0.0f);   // clamped, not negative
        QVERIFY(LocalContrast::bandFactor(1.0f, 1.5f) > 1.0f);
    }

    /* Peaks at mid-grey, falls to 0 at both ends, and is symmetric about 0.5 -- the
       property that keeps Clarity off the blacks AND the highlights. */
    void midtoneWeightPeaksAtMidGrey()
    {
        QVERIFY(std::fabs(LocalContrast::midtoneWeight(0.5f) - 1.0f) < 1e-6f);
        QCOMPARE(LocalContrast::midtoneWeight(0.0f), 0.0f);
        QCOMPARE(LocalContrast::midtoneWeight(1.0f), 0.0f);
        for (float d : {0.1f, 0.25f, 0.4f}) {
            const float lo = LocalContrast::midtoneWeight(0.5f - d);
            const float hi = LocalContrast::midtoneWeight(0.5f + d);
            QVERIFY(std::fabs(lo - hi) < 1e-6f);                  // symmetric
            QVERIFY(lo < 1.0f);                                   // and below the peak
        }
    }

    /* Full effect on a flat area, rolling off as the excursion grows -- never to zero
       and never negative, so it attenuates rather than cutting. */
    void haloGuardRollsOffWithExcursion()
    {
        QCOMPARE(LocalContrast::haloGuard(0.0f, 0.15f), 1.0f);
        QVERIFY(std::fabs(LocalContrast::haloGuard(0.15f, 0.15f) - 0.5f) < 1e-6f);
        float prev = 2.0f;
        for (float d : {0.0f, 0.05f, 0.15f, 0.4f, 1.0f}) {
            const float g = LocalContrast::haloGuard(d, 0.15f);
            QVERIFY(g < prev);                                    // strictly decreasing
            QVERIFY(g > 0.0f);
            prev = g;
        }
        QCOMPARE(LocalContrast::haloGuard(0.9f, 0.0f), 1.0f);     // knee 0 disables it
        QCOMPARE(LocalContrast::haloGuard(-0.15f, 0.15f),
                 LocalContrast::haloGuard(0.15f, 0.15f));         // sign-independent
    }

    /* Texture and Dehaze are ratio-preserving on LUMINANCE: the scene carries a warm
       tint, so if an op ever started touching chroma the channel ratios would move. */
    void bandOpsPreserveChroma()
    {
        const WorkingImage src = makeScene();
        for (float tex : {-80.0f, 60.0f}) {
            WorkingImage img = makeScene();
            EditParams p; p.texture = tex;
            Develop d; d.Apply(img, p, nullptr);
            for (int s = 0; s < 6; ++s) {
                const size_t i = (static_cast<size_t>(kSY[s]) * kW + kSX[s]) * 3;
                const float rgSrc = src.rgb[i + 1] / src.rgb[i + 0];
                const float rgOut = img.rgb[i + 1] / img.rgb[i + 0];
                QVERIFY(std::fabs(rgSrc - rgOut) < 1e-4f);
            }
        }
    }
};

QTEST_APPLESS_MAIN(tst_localcontrast)
#include "tst_localcontrast.moc"
