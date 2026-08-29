/*
    OutputTransform::ToImage -- the float -> 8-bit pack, and the transfer LUT that
    replaced its per-pixel pow.

    WHY THIS TEST EXISTS. The sRGB path is a TABLE plus linear interpolation, not the
    exact maths: it was ~90% of an interactive Develop tick (35 ms of 38 ms on a 5.9 MP
    proxy) and the table is ~4.8x faster. That trade is deliberate but it is also silent
    -- nothing on screen would show the table drifting -- so the bound it was accepted
    under is asserted here rather than left as a comment: every output byte within 1 of
    the exact computation, and only a hair of them off at all.

    The reference below re-implements the exact chain independently of the code under
    test, so a change to either side has to be made twice to pass.
*/
#include <QtTest>
#include <cmath>
#include <vector>
#include "Develop/outputtransform.h"
#include "Develop/workingimage.h"
#include "Develop/colorspace.h"

namespace {

float Clamp01Ref(float v) { return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v); }

/* Linear -> sRGB transfer (IEC 61966-2-1). */
float SrgbGammaRef(float v)
{
    v = Clamp01Ref(v);
    return v <= 0.0031308f ? 12.92f * v
                           : 1.055f * std::pow(v, 1.0f / 2.4f) - 0.055f;
}

/* ACES (Narkowicz) shoulder after a fixed +0.68 EV lift; scene-referred input only. */
float BaselineToneRef(float v)
{
    v *= 1.6f;
    if (v < 0.0f) v = 0.0f;
    const float a = 2.51f, b = 0.03f, c = 2.43f, d = 0.59f, e = 0.14f;
    return Clamp01Ref((v * (a * v + b)) / (v * (c * v + d) + e));
}

int ExactByte(float v, bool sceneReferred)
{
    return int(std::lround(SrgbGammaRef(sceneReferred ? BaselineToneRef(v)
                                                      : Clamp01Ref(v)) * 255.0f));
}

/* sRGB -> Display P3 primaries, in linear. Independently stated here so the reference
   does not borrow the matrix from the code under test. */
const float kSrgbToP3Ref[9] = {
    0.8224620f, 0.1775380f, 0.0000000f,
    0.0331942f, 0.9668058f, 0.0000000f,
    0.0170826f, 0.0723974f, 0.9105199f
};

void RolloffRef(float &r, float &g, float &b)
{
    const float mx = std::max(r, std::max(g, b));
    if (mx <= 1.0f) return;
    const float bleed = mx - 1.0f;
    r = std::min(1.0f, r + bleed);
    g = std::min(1.0f, g + bleed);
    b = std::min(1.0f, b + bleed);
}

/* The exact SPLIT chain: view curve, then the primaries matrix, then the rolloff, then
   the transfer. This is the path any non-sRGB output space takes -- and the path EVERY
   render will take once the working space is wider than the output space. */
void ExactP3Pixel(const float in[3], bool sceneReferred, int out[3])
{
    float v[3];
    for (int c = 0; c < 3; ++c)
        v[c] = sceneReferred ? BaselineToneRef(in[c]) : in[c];
    const float r = v[0], g = v[1], b = v[2];
    v[0] = kSrgbToP3Ref[0]*r + kSrgbToP3Ref[1]*g + kSrgbToP3Ref[2]*b;
    v[1] = kSrgbToP3Ref[3]*r + kSrgbToP3Ref[4]*g + kSrgbToP3Ref[5]*b;
    v[2] = kSrgbToP3Ref[6]*r + kSrgbToP3Ref[7]*g + kSrgbToP3Ref[8]*b;
    RolloffRef(v[0], v[1], v[2]);
    for (int c = 0; c < 3; ++c)
        out[c] = int(std::lround(SrgbGammaRef(v[c]) * 255.0f));
}

/* A ramp across the whole domain the LUT covers, laid out so consecutive pixels differ
   (the table is indexed per channel, so the three channels sample different points). */
WorkingImage makeRamp(int w, int h, float vMax, bool sceneReferred)
{
    WorkingImage img;
    img.width = w;
    img.height = h;
    img.white = 1.0f;
    img.sceneReferred = sceneReferred;
    img.rgb.resize(size_t(w) * size_t(h) * 3);
    const size_t n = size_t(w) * size_t(h);
    for (size_t i = 0; i < n; ++i) {
        const float t = float(i) / float(n - 1);
        img.rgb[i * 3 + 0] = vMax * t;
        img.rgb[i * 3 + 1] = vMax * (1.0f - t);
        img.rgb[i * 3 + 2] = vMax * std::fabs(0.5f - t) * 2.0f;
    }
    return img;
}

} // namespace

class TestOutputTransform : public QObject
{
    Q_OBJECT

private slots:
    void sceneReferredMatchesExactWithinOne();
    void displayReferredMatchesExactWithinOne();
    void blackAndWhiteAreExact();
    void overRangeSaturatesToWhite();
    void wideGamutSplitPathMatchesExactWithinOne();
    void viewTransformNoneIsPassThrough();
    void agxHoldsMidGrey();
    void agxKeepsNeutralsNeutral();
    void agxIsMonotonic();
    void agxDesaturatesHighlightsWithoutHueMarch();

private:
    /* Renders `img` and compares every byte with the reference. Returns the worst
       absolute difference; `offBy` counts the bytes that differ at all. */
    static int compare(const WorkingImage &img, qint64 &offBy, qint64 &total);
};

int TestOutputTransform::compare(const WorkingImage &img, qint64 &offBy, qint64 &total)
{
    QImage out;
    OutputTransform t;
    if (!t.ToImage(img, out)) return -1;

    int worst = 0;
    offBy = 0;
    total = 0;
    for (int y = 0; y < img.height; ++y) {
        const uchar *line = out.constScanLine(y);
        for (int x = 0; x < img.width; ++x) {
            const size_t o = (size_t(y) * size_t(img.width) + size_t(x)) * 3;
            for (int c = 0; c < 3; ++c) {
                const int got = line[x * 3 + c];
                const int want = ExactByte(img.rgb[o + c], img.sceneReferred);
                const int d = std::abs(got - want);
                if (d > worst) worst = d;
                if (d) ++offBy;
                ++total;
            }
        }
    }
    return worst;
}

/* RAW (scene-referred): the baseline tone curve runs, so the rolloff can never fire and
   EVERY pixel goes through the table. Domain covers BaselineTone's saturation at 4.53. */
void TestOutputTransform::sceneReferredMatchesExactWithinOne()
{
    const WorkingImage img = makeRamp(512, 384, 5.0f, /*sceneReferred*/true);
    qint64 offBy = 0, total = 0;
    const int worst = compare(img, offBy, total);
    QVERIFY(worst >= 0);                       // render succeeded
    QVERIFY2(worst <= 1, qPrintable(QString("worst delta %1").arg(worst)));
    /* The bound the table was accepted under; measured ~0.001%, so 0.05% is loose enough
       to absorb platform pow differences and tight enough to catch a real drift. */
    QVERIFY2(offBy * 10000 <= total * 5,
             qPrintable(QString("%1 of %2 bytes differ").arg(offBy).arg(total)));
}

/* Display-referred (JPEG/TIFF): no baseline curve, so a channel CAN exceed 1 and the
   cross-channel rolloff takes over. Values stay in range here so the table is exercised;
   the over-range fallback is covered by overRangeSaturatesToWhite. */
void TestOutputTransform::displayReferredMatchesExactWithinOne()
{
    const WorkingImage img = makeRamp(512, 384, 1.0f, /*sceneReferred*/false);
    qint64 offBy = 0, total = 0;
    const int worst = compare(img, offBy, total);
    QVERIFY(worst >= 0);
    QVERIFY2(worst <= 1, qPrintable(QString("worst delta %1").arg(worst)));
    QVERIFY2(offBy * 10000 <= total * 5,
             qPrintable(QString("%1 of %2 bytes differ").arg(offBy).arg(total)));
}

/* The two ends must be EXACT, not merely within one: a black frame that renders as 1, or
   a white one as 254, is visible. Both are the table's clamped edges. */
void TestOutputTransform::blackAndWhiteAreExact()
{
    WorkingImage img;
    img.width = 4;
    img.height = 1;
    img.white = 1.0f;
    img.sceneReferred = true;
    img.rgb = { 0.0f, 0.0f, 0.0f,        // black
                -1.0f, -1.0f, -1.0f,     // negative: clamps to black
                4.6f, 4.6f, 4.6f,        // the table's last entry
                1e30f, 1e30f, 1e30f };   // far past it
    QImage out;
    OutputTransform t;
    QVERIFY(t.ToImage(img, out));
    const uchar *line = out.constScanLine(0);
    QCOMPARE(int(line[0]), 0);
    QCOMPARE(int(line[3]), 0);
    QCOMPARE(int(line[6]), 255);
    QCOMPARE(int(line[9]), 255);
}

/* Display-referred over-range takes the exact rolloff branch, not the table. */
void TestOutputTransform::overRangeSaturatesToWhite()
{
    WorkingImage img;
    img.width = 1;
    img.height = 1;
    img.white = 1.0f;
    img.sceneReferred = false;
    img.rgb = { 0.5f, 2.0f, 0.25f };     // green over range: bleeds into red and blue
    QImage out;
    OutputTransform t;
    QVERIFY(t.ToImage(img, out));
    const uchar *line = out.constScanLine(0);
    QCOMPARE(int(line[1]), 255);                 // the over-range channel clips
    QVERIFY(int(line[0]) == 255);                // and bleeds the excess into the others
    QVERIFY(int(line[2]) == 255);
}

/*
    THE SPLIT PATH: a non-identity primaries matrix sits between the view curve and the
    transfer, so the two cannot fuse into one table. This path used to run exact pow maths
    because it was export-only; it is now driven by two tables (view, then transfer) and
    must hold the same within-one bound the fused path is held to.

    It matters more than "export only" suggests: the moment the working space is wider
    than the output space, EVERY render takes this path.
*/
void TestOutputTransform::wideGamutSplitPathMatchesExactWithinOne()
{
    for (bool sceneReferred : {true, false}) {
        const WorkingImage img = makeRamp(256, 192, sceneReferred ? 5.0f : 1.0f,
                                          sceneReferred);
        QImage out;
        OutputTransform t;
        QVERIFY(t.ToImage(img, out, OutputTransform::Space::DisplayP3));

        int worst = 0;
        qint64 offBy = 0, total = 0;
        for (int y = 0; y < img.height; ++y) {
            const uchar *line = out.constScanLine(y);
            for (int x = 0; x < img.width; ++x) {
                const size_t o = (size_t(y) * size_t(img.width) + size_t(x)) * 3;
                const float in[3] = {img.rgb[o], img.rgb[o + 1], img.rgb[o + 2]};
                int want[3];
                ExactP3Pixel(in, sceneReferred, want);
                for (int c = 0; c < 3; ++c) {
                    const int d = std::abs(int(line[x * 3 + c]) - want[c]);
                    if (d > worst) worst = d;
                    if (d) ++offBy;
                    ++total;
                }
            }
        }
        QVERIFY2(worst <= 1, qPrintable(QString("P3 sceneReferred=%1 worst delta %2")
                                            .arg(sceneReferred).arg(worst)));
        QVERIFY2(offBy * 100 <= total * 2,
                 qPrintable(QString("P3 sceneReferred=%1: %2 of %3 bytes differ")
                                .arg(sceneReferred).arg(offBy).arg(total)));
    }
}

/*
    ViewTransform::None must be a true pass-through, NOT a tabulated identity. The
    distinction is load-bearing: its table would stop at 1.0, so tabulating it would clamp
    away exactly the over-range values HighlightRolloff exists to redistribute, and a
    brightened highlight would clip flat instead of desaturating.

    Asserted on the split path, where the pass-through and the rolloff both run.
*/
void TestOutputTransform::viewTransformNoneIsPassThrough()
{
    WorkingImage img;
    img.width = 2;
    img.height = 1;
    img.white = 1.0f;
    img.sceneReferred = true;              // so None is honoured, not forced
    img.rgb = {0.4f, 0.4f, 0.4f,           // in range
               2.0f, 0.5f, 0.5f};          // well over white: the rolloff must engage

    QImage out;
    OutputTransform t;
    QVERIFY(t.ToImage(img, out, OutputTransform::Space::DisplayP3,
                      OutputTransform::ViewTransform::None));

    const uchar *line = out.constScanLine(0);
    int want0[3], want1[3];
    const float p0[3] = {0.4f, 0.4f, 0.4f};
    const float p1[3] = {2.0f, 0.5f, 0.5f};
    ExactP3Pixel(p0, /*sceneReferred*/false, want0);   // false => no view curve
    ExactP3Pixel(p1, /*sceneReferred*/false, want1);
    for (int c = 0; c < 3; ++c) {
        QVERIFY(std::abs(int(line[c]) - want0[c]) <= 1);
        QVERIFY(std::abs(int(line[3 + c]) - want1[c]) <= 1);
    }
    /* The over-range pixel must have bled into the other channels rather than clipping
       one at a time: with 2.0 in red, green and blue are pushed to white too. */
    QVERIFY(int(line[3 + 1]) > int(line[1]));
    QVERIFY(int(line[3 + 2]) > int(line[2]));
}

/*
    AgX INVARIANTS.

    AgX's sigmoid here is OURS, not a port (see outputtransform.cpp), so there is no
    reference render to diff against. What can be pinned is the set of properties the
    transform is chosen FOR -- and those are the things a future tweak to the sigmoid or
    the pivot could silently break.
*/

/* Render one linear RGB triple through a 1x1 image and hand back the 8-bit result. */
static void renderOne(const float in[3], OutputTransform::ViewTransform vt, int out[3])
{
    WorkingImage img;
    img.width = 1;
    img.height = 1;
    img.white = 1.0f;
    img.sceneReferred = true;
    img.rgb = {in[0], in[1], in[2]};
    QImage q;
    OutputTransform t;
    QVERIFY(t.ToImage(img, q, OutputTransform::Space::sRGB, vt));
    const uchar *line = q.constScanLine(0);
    for (int c = 0; c < 3; ++c) out[c] = int(line[c]);
}

/* Same, at 16 bits. ToImage16 runs the EXACT maths rather than the 8-bit table, so this
   is the probe for anything whose error is smaller than one part in 255 -- which both of
   the properties below are. */
static void renderOne16(const float in[3], OutputTransform::ViewTransform vt, int out[3],
                        ColorSpaceMath::ColorSpace from = ColorSpaceMath::ColorSpace::LinearSRGB)
{
    WorkingImage img;
    img.width = 1;
    img.height = 1;
    img.white = 1.0f;
    img.sceneReferred = true;
    img.space = from;
    img.rgb = {in[0], in[1], in[2]};
    QImage q;
    OutputTransform t;
    QVERIFY(t.ToImage16(img, q, OutputTransform::Space::sRGB, vt));
    const quint16 *line = reinterpret_cast<const quint16*>(q.constScanLine(0));
    for (int c = 0; c < 3; ++c) out[c] = int(line[c]);
}

/*
    18% scene grey must render as 18% display grey. This is the one number that anchors
    everything the user does: if the pivot drifts, every exposure value they have dialled
    changes meaning, and the transform stops being comparable with Filmic at all.
    CHECKED AT 16 BITS. At 8 bits a wrong pivot lands one level away and slips through a
    within-one tolerance -- measured: mis-deriving the pivot as 0.18^(1/2.4) put mid grey
    at 119 instead of 118 and an 8-bit assertion did not notice. At 16 bits the same error
    is ~279 levels, so the tolerance below is loose enough for LUT-free float rounding and
    still an order of magnitude tighter than any real drift.
*/
void TestOutputTransform::agxHoldsMidGrey()
{
    const float grey[3] = {0.18f, 0.18f, 0.18f};
    const int want = int(std::lround(SrgbGammaRef(0.18f) * 65535.0f));
    int got[3];
    renderOne16(grey, OutputTransform::ViewTransform::AgX, got);
    for (int c = 0; c < 3; ++c)
        QVERIFY2(std::abs(got[c] - want) <= 64,
                 qPrintable(QString("mid grey ch %1 = %2, want %3 (+/-64)")
                                .arg(c).arg(got[c]).arg(want)));

    /* Same anchor in the production configuration (working-space source -> sRGB): the
       extra primaries matrix must not move mid grey, or the pivot is only right for
       images that happen to arrive already in sRGB. */
    int prod[3];
    renderOne16(grey, OutputTransform::ViewTransform::AgX, prod, ColorSpaceMath::kWorking);
    for (int c = 0; c < 3; ++c)
        QVERIFY2(std::abs(prod[c] - want) <= 64,
                 qPrintable(QString("mid grey from kWorking ch %1 = %2, want %3")
                                .arg(c).arg(prod[c]).arg(want)));
}

/*
    A neutral in must be a neutral out, at every level. The inset/outset pair is a change
    of primaries, and a matrix whose rows do not sum to 1 tints greys -- which no white
    balance can then remove. The published inset is rounded, so the code normalises it.

    CHECKED AT 16 BITS, and this is not fussiness: the un-normalised rows are off by only
    1.4e-4, which is 0.04 of an 8-bit level -- an 8-bit assertion cannot see it at all
    (measured: skipping the normalisation passed an 8-bit version of this test). At 16
    bits the same tint is ~2 levels of channel spread, so the bound is 1.
*/
void TestOutputTransform::agxKeepsNeutralsNeutral()
{
    for (float v : {0.02f, 0.09f, 0.18f, 0.5f, 1.0f, 4.0f, 16.0f}) {
        const float grey[3] = {v, v, v};
        int got[3];
        renderOne16(grey, OutputTransform::ViewTransform::AgX, got);
        QVERIFY2(std::abs(got[0] - got[1]) <= 1 && std::abs(got[1] - got[2]) <= 1,
                 qPrintable(QString("grey %1 rendered (%2,%3,%4) at 16-bit")
                                .arg(v).arg(got[0]).arg(got[1]).arg(got[2])));

        /* And in the PRODUCTION configuration: a working-space source rendered to sRGB.
           Once the working space is wider than the output space this is the path every
           render takes, and it carries an extra primaries matrix that the sRGB-source
           case above does not. Both spaces are D65 with rows summing to 1, so a neutral
           must survive the conversion exactly. */
        int prod[3];
        renderOne16(grey, OutputTransform::ViewTransform::AgX, prod, ColorSpaceMath::kWorking);
        QVERIFY2(std::abs(prod[0] - prod[1]) <= 1 && std::abs(prod[1] - prod[2]) <= 1,
                 qPrintable(QString("grey %1 from kWorking rendered (%2,%3,%4)")
                                .arg(v).arg(prod[0]).arg(prod[1]).arg(prod[2])));
    }
}

/* A tone curve that is not monotonic has a locally INVERTED tone -- a bright band inside
   a shadow ramp. Checked across the whole 16.5-stop window the transform covers. */
void TestOutputTransform::agxIsMonotonic()
{
    int prev = -1;
    for (int i = 0; i <= 400; ++i) {
        const float v = std::pow(2.0f, -12.5f + 16.6f * float(i) / 400.0f);
        const float grey[3] = {v, v, v};
        int got[3];
        renderOne(grey, OutputTransform::ViewTransform::AgX, got);
        QVERIFY2(got[0] >= prev, qPrintable(QString("non-monotonic at v=%1: %2 after %3")
                                                .arg(v).arg(got[0]).arg(prev)));
        prev = got[0];
    }
}

/*
    THE POINT OF AgX. A saturated colour driven up in exposure must desaturate toward
    WHITE, with its other two channels rising together. Per-channel filmic instead pins
    the hot channel and leaves the others behind, which is the hue march (saturated red ->
    orange) AgX exists to avoid.

    Asserted two ways: the off-channels must climb steadily with exposure, and they must
    stay level with EACH OTHER -- a spread between them IS a hue shift.
*/
void TestOutputTransform::agxDesaturatesHighlightsWithoutHueMarch()
{
    int prevOff = -1;
    for (float ev : {0.0f, 2.0f, 4.0f, 6.0f}) {
        const float red[3] = {std::pow(2.0f, ev), 0.0f, 0.0f};
        int got[3];
        renderOne(red, OutputTransform::ViewTransform::AgX, got);
        QVERIFY2(got[1] > prevOff,
                 qPrintable(QString("off-channels not rising at %1 EV: (%2,%3,%4)")
                                .arg(ev).arg(got[0]).arg(got[1]).arg(got[2])));
        QVERIFY2(std::abs(got[1] - got[2]) <= 2,
                 qPrintable(QString("hue march at %1 EV: G=%2 B=%3")
                                .arg(ev).arg(got[1]).arg(got[2])));
        prevOff = got[1];
    }

    /* And the contrast with Filmic, which is the reason to offer AgX at all: at high
       exposure the per-channel curve leaves the off-channels far darker. */
    const float hot[3] = {64.0f, 0.0f, 0.0f};
    int agx[3], filmic[3];
    renderOne(hot, OutputTransform::ViewTransform::AgX, agx);
    renderOne(hot, OutputTransform::ViewTransform::Filmic, filmic);
    QVERIFY2(agx[1] > filmic[1] + 20,
             qPrintable(QString("AgX G=%1 vs Filmic G=%2").arg(agx[1]).arg(filmic[1])));
}

QTEST_MAIN(TestOutputTransform)
#include "tst_outputtransform.moc"
