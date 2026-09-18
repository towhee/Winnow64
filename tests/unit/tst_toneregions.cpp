/*
    The Basic panel's four tone REGION controls -- blacks | shadows | highlights | whites.

    WHY THIS TEST EXISTS. Winnow's tone sliders do not behave like Lightroom's, and the
    reason is not the one that is easy to reach for. Lightroom's histogram has five
    draggable divisions to Winnow's four, but the missing one is Exposure -- which Winnow
    HAS (develop.cpp, a scene-linear 2^EV gain folded into the point pass's preMat) and
    which Lightroom's histogram zone merely drags. Neither app has a midtone region. The
    fifth division is a UI affordance and moves no pixels.

    What actually differs is the SHAPE OF THE OPERATORS, and this file pins the four
    properties that shape has to have. Each assertion below was written against the old
    implementation FIRST and observed to fail; a test for a property the code already had
    would not have been worth adding.

      1. WHITES AND BLACKS ARE ENDPOINT MOVES, NOT BUMPS. They used to be additive
         Gaussian lifts pinned at s=0 and s=1 with sigma 0.18, which is a fundamentally
         different operator: a bump brightens a BAND near the end and can never move the
         end itself. Lightroom's rescale where the curve reaches white and black, so the
         whole range below moves with them -- which is why LR's Whites pushes the
         histogram's right edge and clips while a bump only brightens and rolls off.

         THE DISCRIMINATING PROPERTY IS REACH AT MID-GREY, not clipping at the top, and
         that is a correction to the obvious test. "Whites +100 drives a sub-white input
         to 255" PASSES under the old Gaussians -- the lobe is centred at 1.0, so of
         course it pushes values near white over the line. It proves nothing. An endpoint
         move is an affine rescale of the WHOLE range, so it reaches mid-grey; the
         Gaussian at sigma 0.18 carries 1.2% of its weight there (0.6 of a level of 255,
         invisible). Measuring in the middle is what separates the two operators.

      2. THE REGIONS PARTITION RATHER THAN SUM. Four unnormalised Gaussians at 0 / 0.25 /
         0.75 / 1.0 overlap heavily: Blacks and Shadows together stacked to nearly twice
         one slider's full travel down in the shadows, and the highlights lobe still
         carried ~35% weight at s=1.0, so Highlights dragged the white point.

      3. THE COMPOSED CURVE IS MONOTONE. With opposing sliders the summed shifts could
         invert the LUT locally -- a brighter input rendering darker than a dimmer one,
         which reads as a halo inside a smooth ramp. It is the exact failure tonecurve.h
         chose Fritsch-Carlson to avoid on the point curve, and the parametric side had
         no equivalent guard.

      4. THE REGION A HANDLE SELECTS DOES NOT MOVE WITH CONTRAST. The weights used to be
         sampled AFTER the contrast slope, so raising Contrast slid the pixels that
         Shadows and Highlights acted on -- while the ToneRegionSlider handles stayed
         drawn against the input histogram. The handles and the pixels they lift have to
         agree, or the control is lying about what it does.

    WHAT THIS FILE DOES NOT DO is pin Winnow's rendering AGAINST Lightroom's. The
    constants those properties are tuned to are a separate, measured thing (see the tone
    region fit note in notes/Documentation.txt); the thresholds here are deliberately
    loose, asserting the operator is the RIGHT KIND, not that it is tuned. A test that
    pinned the tuning would have to be re-blessed every time the fit improved, which is
    how a characterization test becomes noise.
*/

#include <QtTest>
#include <vector>
#include <cmath>
#include <QImage>

#include "Develop/develop.h"
#include "Develop/workingimage.h"
#include "Develop/editparams.h"
#include "Develop/colorspace.h"
#include "Develop/outputtransform.h"

namespace {

/* A NEUTRAL ramp, in the working space, display-referred. Neutral because the output
   stage's primaries conversion has rows summing to 1, so a grey stays grey and an 8-bit
   readback measures tone alone rather than tone plus a colour shift. Display-referred so
   ViewTransform::None is legitimate (a view transform is forced off for such data anyway,
   per OutputTransform::EffectiveView) and the ACES shoulder cannot colour the result --
   the shoulder is a real difference from Lightroom, but it is not the one under test. */
constexpr int kRampN = 256;

WorkingImage makeRamp()
{
    WorkingImage img;
    img.width  = kRampN;
    img.height = 1;
    img.white  = 1.0f;
    img.sceneReferred = false;
    img.space  = ColorSpaceMath::kWorking;
    img.rgb.resize(static_cast<size_t>(kRampN) * 3);
    for (int i = 0; i < kRampN; ++i) {
        /* Linear values whose PERCEPTUAL positions are evenly spread, so the ramp has as
           many samples in the shadows as in the highlights -- a linear ramp would put
           almost every sample above mid-grey and barely probe the blacks region. */
        const float s = static_cast<float>(i) / (kRampN - 1);
        const float v = std::pow(s, 2.2f);
        img.rgb[static_cast<size_t>(i) * 3 + 0] = v;
        img.rgb[static_cast<size_t>(i) * 3 + 1] = v;
        img.rgb[static_cast<size_t>(i) * 3 + 2] = v;
    }
    return img;
}

/* The index into the ramp whose perceptual position is s. */
int atPerceptual(float s) { return qBound(0, int(s * (kRampN - 1) + 0.5f), kRampN - 1); }

/* Mid-grey: 18% scene linear, i.e. perceptual 0.18^(1/2.2) = 0.4663. The pivot the
   contrast slope turns about, and the tone the whole pipeline is anchored on. */
const int kMidIdx = atPerceptual(std::pow(0.18f, 1.0f / 2.2f));

/* Render the ramp under p and return the 8-bit red channel (== green == blue: neutral).

   THE RECIPE'S VIEW TRANSFORM AND THE RENDER'S MUST AGREE, and forcing that here is not
   test bookkeeping -- it is the rule the pipeline already runs on ("any call site that
   renders a recipe must pass OutputTransform::ViewFromInt(edit.viewTransform)"). It became
   load-bearing when the tone regions started being placed on the display axis: the LUT is
   now built against a particular view transform, so building it for Filmic and then
   rendering with None puts the regions in the wrong place. EditParams defaults to Filmic,
   so a test that renders with None has to say so in the params too. This test caught that
   coupling by failing -- Highlights -100 dragged white 11 levels -- which is worth more
   than the line of setup it costs. */
std::vector<int> render(const EditParams &pIn)
{
    EditParams p = pIn;
    p.viewTransform = int(OutputTransform::ViewTransform::None);
    WorkingImage img = makeRamp();
    Develop dev;
    if (!dev.Apply(img, p)) return {};
    QImage out;
    OutputTransform ot;
    if (!ot.ToImage(img, out, OutputTransform::Space::sRGB,
                    OutputTransform::ViewTransform::None)) return {};
    std::vector<int> v(kRampN);
    const uchar *line = out.constScanLine(0);
    for (int i = 0; i < kRampN; ++i) v[i] = line[i * 3];
    return v;
}

} // namespace

class tst_toneregions : public QObject
{
    Q_OBJECT
private slots:
    void whitesMoveTheWholeRangeNotJustTheTop();
    void blacksMoveTheWholeRangeNotJustTheBottom();
    void whitesAndBlacksLeaveTheOppositeEndAlone();
    void highlightsDoNotDragTheWhitePoint();
    void twoControlsDoNotCompound();
    void opposingSlidersStayMonotone();
    void regionsDoNotSlideWithContrast();
};

/*
    1a. WHITES REACHES MID-GREY. An endpoint move rescales everything below the white
    point, so mid-grey moves with it; a Gaussian centred at 1.0 does not. The threshold is
    8 levels of 255 -- far above the old operator's 0.6, far below any white-point range
    that could plausibly match Lightroom, so it discriminates the operator without pinning
    the tuning.
*/
void tst_toneregions::whitesMoveTheWholeRangeNotJustTheTop()
{
    EditParams base;
    EditParams up = base;   up.whites   = 100.0f;
    EditParams down = base; down.whites = -100.0f;

    const std::vector<int> b = render(base), u = render(up), d = render(down);
    QVERIFY(!b.empty() && !u.empty() && !d.empty());

    QVERIFY2(u[kMidIdx] - b[kMidIdx] >= 8,
             qPrintable(QString("Whites +100 lifted mid-grey by only %1 levels (%2 -> %3); "
                                "an endpoint move rescales the whole range, a bump does not")
                        .arg(u[kMidIdx] - b[kMidIdx]).arg(b[kMidIdx]).arg(u[kMidIdx])));
    QVERIFY2(b[kMidIdx] - d[kMidIdx] >= 8,
             qPrintable(QString("Whites -100 lowered mid-grey by only %1 levels (%2 -> %3)")
                        .arg(b[kMidIdx] - d[kMidIdx]).arg(b[kMidIdx]).arg(d[kMidIdx])));
}

/*
    1b. BLACKS REACHES MID-GREY, for the same reason and with the same threshold. The old
    Gaussian at sigma 0.18 carried 3.5% of its weight at the pivot -- about 1.8 levels.
*/
void tst_toneregions::blacksMoveTheWholeRangeNotJustTheBottom()
{
    EditParams base;
    EditParams up = base;   up.blacks   = 100.0f;
    EditParams down = base; down.blacks = -100.0f;

    const std::vector<int> b = render(base), u = render(up), d = render(down);
    QVERIFY(!b.empty() && !u.empty() && !d.empty());

    QVERIFY2(u[kMidIdx] - b[kMidIdx] >= 8,
             qPrintable(QString("Blacks +100 lifted mid-grey by only %1 levels (%2 -> %3)")
                        .arg(u[kMidIdx] - b[kMidIdx]).arg(b[kMidIdx]).arg(u[kMidIdx])));
    QVERIFY2(b[kMidIdx] - d[kMidIdx] >= 8,
             qPrintable(QString("Blacks -100 lowered mid-grey by only %1 levels (%2 -> %3)")
                        .arg(b[kMidIdx] - d[kMidIdx]).arg(b[kMidIdx]).arg(d[kMidIdx])));
}

/*
    1c. AND EACH LEAVES THE OTHER END PINNED. The counterweight to the two assertions
    above: an endpoint move with too much reach becomes an exposure slider. Whites must
    not move black, Blacks must not move white. Both held under the old Gaussians too --
    kept because the new operator is the one that could plausibly break them.
*/
void tst_toneregions::whitesAndBlacksLeaveTheOppositeEndAlone()
{
    EditParams base;
    EditParams w = base; w.whites = 100.0f;
    EditParams k = base; k.blacks = -100.0f;

    const std::vector<int> b = render(base), rw = render(w), rk = render(k);
    QVERIFY(!b.empty() && !rw.empty() && !rk.empty());

    QVERIFY2(std::abs(rw[0] - b[0]) <= 1,
             qPrintable(QString("Whites +100 moved BLACK by %1 levels").arg(rw[0] - b[0])));
    QVERIFY2(std::abs(rk[kRampN - 1] - b[kRampN - 1]) <= 1,
             qPrintable(QString("Blacks -100 moved WHITE by %1 levels")
                        .arg(rk[kRampN - 1] - b[kRampN - 1])));
}

/*
    2a. HIGHLIGHTS DOES NOT DRAG THE WHITE POINT. The old highlights lobe sat at 0.75 with
    sigma 0.18, which still carries ~38% of its weight at s=1.0 -- so the control that is
    meant to shape the upper midtones was also an endpoint move nobody asked for. Once the
    regions partition, the highlights weight has fallen to zero by the time white arrives.

    MEASURED IN THE NEGATIVE DIRECTION, and that is not arbitrary. Highlights +100 cannot
    show this defect at all: white is already 255 and an upward lift has nowhere to go, so
    the assertion passes on a clamp rather than on the property. Only a downward pull
    reveals whether the lobe reaches the endpoint -- it dragged white to ~235 under the
    old operator. A test that can only fail in one direction has to be pointed that way.
*/
void tst_toneregions::highlightsDoNotDragTheWhitePoint()
{
    EditParams base;
    EditParams hi = base; hi.highlights = -100.0f;

    const std::vector<int> b = render(base), h = render(hi);
    QVERIFY(!b.empty() && !h.empty());

    QVERIFY2(std::abs(h[kRampN - 1] - b[kRampN - 1]) <= 1,
             qPrintable(QString("Highlights -100 moved WHITE by %1 levels (%2 -> %3); "
                                "the highlights region must not reach the endpoint")
                        .arg(h[kRampN - 1] - b[kRampN - 1])
                        .arg(b[kRampN - 1]).arg(h[kRampN - 1])));
}

/*
    2b. TWO CONTROLS NEVER COMPOUND. Shadows and Highlights overlap in the midtones, so a
    tone there receives some of each, and their shifts ADD. That is correct and is what
    Lightroom does -- its shadows band reaches level 160 and its highlights band starts at
    48. What must never happen is SUPER-additivity: the pair doing more than the sum of the
    two alone, which is the signature of a weight applied twice or a band counted in both
    the endpoint and the region path.

    THIS TEST USED TO ASSERT THE OPPOSITE -- that the regions have disjoint supports and no
    tone can receive both lifts. That property was invented here, not measured, and the
    Lightroom fit overturned it: the bands genuinely overlap, and forcing them apart was
    part of what made Highlights reach a quarter of Lightroom's range. The assertion was
    replaced rather than loosened, because "they do not overlap" and "they do not compound"
    are different claims and only the second one is true.
*/
void tst_toneregions::twoControlsDoNotCompound()
{
    EditParams base;
    EditParams onlySh = base;  onlySh.shadows    = 100.0f;
    EditParams onlyHi = base;  onlyHi.highlights = 100.0f;
    EditParams both   = base;  both.shadows = 100.0f; both.highlights = 100.0f;

    const std::vector<int> b = render(base), rs = render(onlySh),
                           rh = render(onlyHi), rboth = render(both);
    QVERIFY(!b.empty() && !rs.empty() && !rh.empty() && !rboth.empty());

    for (int i = 0; i < kRampN; ++i) {
        const int dSh = rs[i] - b[i], dHi = rh[i] - b[i], dBoth = rboth[i] - b[i];
        /* +3 of slack for 8-bit rounding: three renders each round independently, and the
           sum of two rounded shifts can legitimately sit a level either side of one. */
        QVERIFY2(dBoth <= dSh + dHi + 3,
                 qPrintable(QString("sample %1: shadows %2 + highlights %3 compounded to %4")
                            .arg(i).arg(dSh).arg(dHi).arg(dBoth)));
    }
}

/*
    3. OPPOSING SLIDERS STAY MONOTONE. A brighter input must never render darker than a
    dimmer one. Swept across the pairs that pull hardest against each other, with contrast
    at both extremes since the slope scales every shift with it. 8-bit quantisation makes
    adjacent samples legitimately EQUAL, so the assertion is non-decreasing, not strictly
    increasing -- the same allowance tst_outputtransform makes for the top of the filmic
    domain.
*/
void tst_toneregions::opposingSlidersStayMonotone()
{
    struct Case { const char *name; float bk, sh, hi, wh, ct; };
    const Case cases[] = {
        {"blacks up / shadows down",      100, -100,    0,    0,    0},
        {"shadows up / blacks down",     -100,  100,    0,    0,    0},
        {"highlights up / whites down",     0,    0,  100, -100,    0},
        {"whites up / highlights down",     0,    0, -100,  100,    0},
        {"all four alternating",          100, -100,  100, -100,    0},
        {"all four alternating, +contrast",100, -100,  100, -100,  100},
        {"all four alternating, -contrast",100, -100,  100, -100, -100},
        {"all four opposed, +contrast",  -100,  100, -100,  100,  100},
    };

    for (const Case &c : cases) {
        EditParams p;
        p.blacks = c.bk; p.shadows = c.sh;
        p.highlights = c.hi; p.whites = c.wh; p.contrast = c.ct;

        const std::vector<int> r = render(p);
        QVERIFY2(!r.empty(), c.name);
        for (int i = 1; i < kRampN; ++i) {
            QVERIFY2(r[i] >= r[i - 1],
                     qPrintable(QString("%1: tone inverted at sample %2 (%3 -> %4)")
                                .arg(c.name).arg(i).arg(r[i - 1]).arg(r[i])));
        }
    }
}

/*
    4. THE REGIONS DO NOT SLIDE WITH CONTRAST. Shadows +100 lifts some band of the ramp.
    Where the PEAK of that lift falls must not move when Contrast changes, because the
    ToneRegionSlider handle that places it is drawn against the input histogram, not the
    post-contrast one. Measured as the ramp index of the largest lift, against the same
    ramp rendered at the same contrast WITHOUT the shadows slider, so the contrast
    slope's own redistribution is differenced out.
*/
void tst_toneregions::regionsDoNotSlideWithContrast()
{
    auto peakOfShadowLift = [](float contrast) {
        EditParams base; base.contrast = contrast;
        EditParams sh = base; sh.shadows = 100.0f;
        const std::vector<int> b = render(base), s = render(sh);
        int peak = -1, best = -1;
        for (int i = 0; i < kRampN; ++i) {
            const int d = s[i] - b[i];
            if (d > best) { best = d; peak = i; }
        }
        return peak;
    };

    const int flat = peakOfShadowLift(0.0f);
    const int up   = peakOfShadowLift(100.0f);
    const int down = peakOfShadowLift(-100.0f);

    QVERIFY(flat >= 0 && up >= 0 && down >= 0);
    /* Within 8 of 256 samples: tight enough to catch the old post-contrast sampling
       (which moved the peak by tens of samples at full contrast), loose enough that
       8-bit quantisation picking a neighbouring sample does not fail the test. */
    QVERIFY2(std::abs(up - flat) <= 8,
             qPrintable(QString("Contrast +100 slid the shadows region from sample %1 to %2")
                        .arg(flat).arg(up)));
    QVERIFY2(std::abs(down - flat) <= 8,
             qPrintable(QString("Contrast -100 slid the shadows region from sample %1 to %2")
                        .arg(flat).arg(down)));
}

QTEST_APPLESS_MAIN(tst_toneregions)
#include "tst_toneregions.moc"
