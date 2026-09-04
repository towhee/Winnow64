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

    RE-BLESSED 2026-09-04, for two changes that are both deliberate and both already
    documented where they landed. Every number below was recaptured together; each cause
    was isolated by reverting it alone and watching the ORIGINAL goldens go green again.

      1. The working space widened from LinearSRGB to LinearRec2020 (Develop/colorspace.h,
         "Added Views, including AgX"). Every band op weights its perceptual luma with the
         working space's weights, which moved from .2126/.7152/.0722 to .2627/.6780/.0593,
         so Clarity, Dehaze and Sharpen all shifted. The shift is small (~1e-4) because
         these ops are ratio-preserving and the band is measured against its own blur, so
         a change in the luma weights largely cancels; Texture moved by ~1e-7, under kTol.

      2. Sharpen's Masking knee was re-calibrated from linear to `m*(0.10 + 0.40*m^2)`
         (Develop/sharpen.h, note dated 2026-08-21) so the top of the slider has real
         travel. It protects MORE above the middle of the slider, which is why the biggest
         move here is sharpenMasked (masking 0.7) at -0.025 -- more gate, less sharpening.

    Neither cause is an accident of the toolchain: OpenCV and the compiler were unchanged
    across the drift, and both reverts reproduced the old numbers exactly.
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
    0.165306494f, 0.153735042f, 0.142163590f,
    0.257810175f, 0.239763454f, 0.221716747f,
    0.424456269f, 0.394744307f, 0.365032405f,
    0.812490106f, 0.755615771f, 0.698741555f,
    0.805792451f, 0.749386966f, 0.692981482f,
    0.940678418f, 0.874830961f, 0.808983445f
};
static const float k_textureNeg[18] = {
    0.180304125f, 0.167682841f, 0.155061558f,
    0.272993654f, 0.253884107f, 0.234774560f,
    0.408659637f, 0.380053461f, 0.351447284f,
    0.686726928f, 0.638656020f, 0.590585172f,
    0.821233869f, 0.763747454f, 0.706261098f,
    0.925015569f, 0.860264480f, 0.795513451f
};
static const float k_textureFull[18] = {
    0.160182416f, 0.148969650f, 0.137756884f,
    0.252564222f, 0.234884724f, 0.217205226f,
    0.430109680f, 0.400001973f, 0.369894326f,
    0.859641671f, 0.799466729f, 0.739291847f,
    0.800380647f, 0.744354010f, 0.688327312f,
    0.946240783f, 0.880003929f, 0.813767076f
};
static const float k_dehazePos[18] = {
    0.193355739f, 0.177168056f, 0.160980403f,
    0.317414343f, 0.290840566f, 0.264266729f,
    0.542969882f, 0.497512609f, 0.452055395f,
    1.101192713f, 1.009001255f, 0.916809916f,
    1.086954117f, 0.995954692f, 0.904955268f,
    1.276084304f, 1.169251084f, 1.062417746f
};
static const float k_dehazeNeg[18] = {
    0.157986268f, 0.148506790f, 0.139027327f,
    0.230657339f, 0.216817454f, 0.202977583f,
    0.338748366f, 0.318422824f, 0.298097312f,
    0.552441239f, 0.519293725f, 0.486146241f,
    0.644823611f, 0.606132925f, 0.567442298f,
    0.725564480f, 0.682029307f, 0.638494074f
};
static const float k_dehazeFull[18] = {
    0.201527327f, 0.183490083f, 0.165452823f,
    0.340264052f, 0.309809506f, 0.279354930f,
    0.604555368f, 0.550446033f, 0.496336639f,
    1.291258454f, 1.175687194f, 1.060116172f,
    1.217095613f, 1.108162165f, 0.999228716f,
    1.443845868f, 1.314617753f, 1.185389638f
};
static const float k_combined[18] = {
    0.174457863f, 0.161042109f, 0.147626355f,
    0.281966448f, 0.260283321f, 0.238600180f,
    0.486716807f, 0.449288398f, 0.411860019f,
    0.993827283f, 0.917402148f, 0.840977192f,
    0.935296297f, 0.863372326f, 0.791448236f,
    1.106145620f, 1.021083355f, 0.936020911f
};

/* Sharpen (#8.5) op-level goldens. tst_sharpen covers the header kernel; these pin the
   OP -- its prep, blur, Sobel gate and fold-back -- which the shared-pass extraction
   rewrote. Captured after that extraction was verified bit-identical against a
   reconstructed pre-refactor build. */
static const float k_sharpenMid[18] = {
    0.172237590f, 0.160180956f, 0.148124337f,
    0.265170276f, 0.246608362f, 0.228046447f,
    0.416489691f, 0.387335390f, 0.358181149f,
    0.801166594f, 0.745084882f, 0.689003289f,
    0.813743770f, 0.756781697f, 0.699819624f,
    0.932552099f, 0.867273510f, 0.801994860f
};
static const float k_sharpenMasked[18] = {
    0.172880322f, 0.160778701f, 0.148677081f,
    0.265603125f, 0.247010887f, 0.228418678f,
    0.416183919f, 0.387051046f, 0.357918173f,
    0.766485274f, 0.712831318f, 0.659177363f,
    0.813886344f, 0.756914318f, 0.699942231f,
    0.932423651f, 0.867154002f, 0.801884353f
};
static const float k_sharpenFull[18] = {
    0.165212974f, 0.153648064f, 0.142083168f,
    0.257748038f, 0.239705667f, 0.221663311f,
    0.424490690f, 0.394776314f, 0.365061998f,
    0.850630820f, 0.791086614f, 0.731542528f,
    0.807048082f, 0.750554681f, 0.694061339f,
    0.938956857f, 0.873229861f, 0.807502925f
};

/* Clarity (#6.5) op-level goldens. */
static const float k_clarityPos[18] = {
    0.169340745f, 0.157486886f, 0.145633042f,
    0.261892110f, 0.243559659f, 0.225227207f,
    0.419942468f, 0.390546471f, 0.361150533f,
    0.766284823f, 0.712644875f, 0.659004986f,
    0.811422646f, 0.754623055f, 0.697823465f,
    0.934235752f, 0.868839264f, 0.803442776f
};
static const float k_clarityNeg[18] = {
    0.178083375f, 0.165617540f, 0.153151706f,
    0.270795166f, 0.251839519f, 0.232883841f,
    0.411130607f, 0.382351458f, 0.353572339f,
    0.717210174f, 0.667005420f, 0.616800785f,
    0.817166865f, 0.759965181f, 0.702763498f,
    0.929994822f, 0.864895165f, 0.799795568f
};
static const float k_clarityFull[18] = {
    0.166252077f, 0.154614434f, 0.142976791f,
    0.258726716f, 0.240615845f, 0.222504973f,
    0.423139662f, 0.393519878f, 0.363900125f,
    0.784385979f, 0.729478955f, 0.674571991f,
    0.809360206f, 0.752705038f, 0.696049809f,
    0.935765147f, 0.870261610f, 0.804758072f
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
