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
#include <algorithm>
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

/* A frame W x H whose columns [0, rampFrom) are a flat fill at perceptual s=fill and
   whose remaining columns are the evenly-spread perceptual ramp, repeated on every row.
   The fill sets how much of the frame is bright, which is what Highlights - adapts to. */
WorkingImage makeFrame(int w, int h, int rampFrom, float fill)
{
    WorkingImage img;
    img.width = w; img.height = h; img.white = 1.0f;
    img.sceneReferred = false;
    img.space = ColorSpaceMath::kWorking;
    img.rgb.resize(size_t(w) * h * 3);
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x) {
            const float s = x < rampFrom ? fill
                                         : float(x - rampFrom) / float(w - 1 - rampFrom);
            const float v = std::pow(s, 2.2f);
            float *px = img.rgb.data() + (size_t(y) * w + x) * 3;
            px[0] = px[1] = px[2] = v;
        }
    return img;
}

/* Render any frame under p (None view, like render()) and return row 0, 8-bit red. */
std::vector<int> renderRow(WorkingImage img, const EditParams &pIn)
{
    EditParams p = pIn;
    p.viewTransform = int(OutputTransform::ViewTransform::None);
    Develop dev;
    if (!dev.Apply(img, p)) return {};
    QImage out;
    OutputTransform ot;
    if (!ot.ToImage(img, out, OutputTransform::Space::sRGB,
                    OutputTransform::ViewTransform::None)) return {};
    std::vector<int> v(img.width);
    const uchar *line = out.constScanLine(0);
    for (int i = 0; i < img.width; ++i) v[i] = line[i * 3];
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
    void shadowsIsAGainNotAnOffset();
    void highlightsRecoverAPushPastWhite();
    void highlightsRecoveryDoesNotCompound();
    void highlightsAdaptsToTheFrame();
    void highlightsStatisticIsTheFrameNotTheCrop();
    void highlightsPullsTheRegion();
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

    WHITE IS A WHITE REGION, not the ramp's last sample. Highlights - is local (2026-10-11):
    a pixel is pulled as its neighbourhood sits, and the last sample of a ramp is a white
    pixel whose neighbours are all darker, so it takes a small region pull (2 levels) by
    design -- the gradient's end is not the endpoint. The ramp therefore runs into a flat
    white tail, and the property is measured in the middle of the tail.
*/
void tst_toneregions::highlightsDoNotDragTheWhitePoint()
{
    EditParams base;
    EditParams hi = base; hi.highlights = -100.0f;

    constexpr int kTail = 64, W = kRampN + kTail;
    WorkingImage img = makeRamp();
    img.width = W;
    img.rgb.resize(size_t(W) * 3);
    std::fill(img.rgb.begin() + size_t(kRampN) * 3, img.rgb.end(), 1.0f);

    const std::vector<int> b = renderRow(img, base), h = renderRow(img, hi);
    QVERIFY(!b.empty() && !h.empty());

    const int x = kRampN + kTail / 2;
    QVERIFY2(std::abs(h[x] - b[x]) <= 1,
             qPrintable(QString("Highlights -100 moved WHITE by %1 levels (%2 -> %3); "
                                "the highlights region must not reach the endpoint")
                        .arg(h[x] - b[x]).arg(b[x]).arg(h[x])));
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
    4. THE REGIONS DO NOT SLIDE WITH CONTRAST. Highlights +100 lifts some band of the ramp.
    Where the PEAK of that lift falls must not move when Contrast changes, because the
    ToneRegionSlider handle that places it is drawn against the input histogram, not the
    post-contrast one. Measured as the ramp index of the largest lift, against the same
    ramp rendered at the same contrast WITHOUT the highlights slider, so the contrast
    slope's own redistribution is differenced out.

    MEASURED ON HIGHLIGHTS, NOT SHADOWS, since 2026-10-05. Shadows became a gain, and the
    peak of a gain's lift IN LEVELS depends on the value being scaled as well as on the
    weight -- Contrast +100 darkens the deep shadows, so the same gain lifts them fewer
    levels and that peak moves for a reason that has nothing to do with where the region
    sits. Highlights is still an additive band, whose lift is its weight, and both read
    their weights from the same pre-contrast position d0 in applyToneShape.
*/
void tst_toneregions::regionsDoNotSlideWithContrast()
{
    auto peakOfHighlightLift = [](float contrast) {
        EditParams base; base.contrast = contrast;
        EditParams sh = base; sh.highlights = 100.0f;
        const std::vector<int> b = render(base), s = render(sh);
        int peak = -1, best = -1;
        for (int i = 0; i < kRampN; ++i) {
            const int d = s[i] - b[i];
            if (d > best) { best = d; peak = i; }
        }
        return peak;
    };

    const int flat = peakOfHighlightLift(0.0f);
    const int up   = peakOfHighlightLift(100.0f);
    const int down = peakOfHighlightLift(-100.0f);

    QVERIFY(flat >= 0 && up >= 0 && down >= 0);
    /* Within 8 of 256 samples: tight enough to catch the old post-contrast sampling
       (which moved the peak by tens of samples at full contrast), loose enough that
       8-bit quantisation picking a neighbouring sample does not fail the test. */
    QVERIFY2(std::abs(up - flat) <= 8,
             qPrintable(QString("Contrast +100 slid the highlights region from sample %1 to %2")
                        .arg(flat).arg(up)));
    QVERIFY2(std::abs(down - flat) <= 8,
             qPrintable(QString("Contrast -100 slid the highlights region from sample %1 to %2")
                        .arg(flat).arg(down)));
}

/*
    5. SHADOWS IS A GAIN, NOT AN OFFSET. Lightroom's Shadows multiplies display-linear
    light (measured 2026-10-05: its -100 and +100 curves are mirror images in stops, and
    detail inside shadow regions grows 43% at +100). Winnow's used to ADD a display-space
    lift that peaked at level 5, which is a black-point raise: the deepest tones all rose
    by about the same amount, and the contrast between them was flattened.

    WHAT SEPARATES THE TWO IS RATIO, NOT RISE. Adding a constant to two dark tones
    collapses the ratio between their light; multiplying them keeps it (exactly, for a flat
    gain). A gain that FADES with brightness, as Lightroom's does, still shrinks the rise
    in levels between two shadow tones -- Lightroom's own curve does, 30 levels to 26
    between levels 10 and 40 -- so asserting the rise grows would pin something Lightroom
    does not do. Measured between display levels ~5 and ~20 of the base render, where
    the light differs by ~4.6x: the old offset kept 37% of that ratio, Lightroom keeps 56%.
    The 45% threshold sits between them with room for 8-bit rounding at level 5. And a
    gain leaves black at black, because 0 times anything is 0.
*/
void tst_toneregions::shadowsIsAGainNotAnOffset()
{
    EditParams base;
    EditParams up = base; up.shadows = 100.0f;

    const std::vector<int> b = render(base), u = render(up);
    QVERIFY(!b.empty() && !u.empty());

    int lo = -1, hi = -1;
    for (int i = 0; i < kRampN; ++i) {
        if (lo < 0 && b[i] >= 5)  lo = i;
        if (hi < 0 && b[i] >= 20) hi = i;
    }
    QVERIFY(lo >= 0 && hi > lo);

    auto lin = [](int level) {
        const float d = level / 255.0f;
        return d <= 0.04045f ? d / 12.92f : std::pow((d + 0.055f) / 1.055f, 2.4f);
    };
    const float inRatio  = lin(b[hi]) / lin(b[lo]);
    const float outRatio = lin(u[hi]) / lin(u[lo]);

    QVERIFY2(std::abs(u[0] - b[0]) <= 1,
             qPrintable(QString("Shadows +100 moved BLACK from %1 to %2")
                        .arg(b[0]).arg(u[0])));
    QVERIFY2(outRatio >= 0.45f * inRatio,
             qPrintable(QString("Shadows +100 collapsed the shadows: levels %1 and %2 "
                                "(light ratio %3) rendered at %4 and %5 (ratio %6); an "
                                "offset does that, a gain keeps the ratio")
                        .arg(b[lo]).arg(b[hi]).arg(inRatio, 0, 'f', 2)
                        .arg(u[lo]).arg(u[hi]).arg(outRatio, 0, 'f', 2)));
}

/*
    6a. HIGHLIGHTS -100 RECOVERS WHAT EXPOSURE PUSHED PAST WHITE. Exposure +3 pushes the
    top three stops of the ramp past white, and the data is still there -- the point pass
    carries it as linear values up to 8.0. The display-axis band cannot reach it (those
    values sit at display 1, where its weight is zero): before the recovery stage,
    Highlights -100 left every one of them at 255. Full slider must bring them all back
    below white, in order and distinct; -50 must bring back some of them.
*/
void tst_toneregions::highlightsRecoverAPushPastWhite()
{
    EditParams base; base.exposure = 3.0f;
    EditParams rec  = base; rec.highlights  = -100.0f;
    EditParams half = base; half.highlights = -50.0f;

    const std::vector<int> b = render(base), r = render(rec), h = render(half);
    QVERIFY(!b.empty() && !r.empty() && !h.empty());

    auto blown = [](const std::vector<int> &v) {
        return int(std::count(v.begin(), v.end(), 255));
    };
    const int firstBlown = int(std::find(b.begin(), b.end(), 255) - b.begin());
    QVERIFY2(blown(b) >= 60,
             qPrintable(QString("setup: Exposure +3 blew only %1 samples").arg(blown(b))));

    QVERIFY2(r[kRampN - 1] <= 252,
             qPrintable(QString("Highlights -100 left the top at %1; everything Exposure "
                                "pushed past white must come back").arg(r[kRampN - 1])));
    int distinct = 1;
    for (int i = 1; i < kRampN; ++i) {
        QVERIFY2(r[i] >= r[i - 1],
                 qPrintable(QString("recovered ramp inverts at sample %1 (%2 -> %3)")
                            .arg(i).arg(r[i - 1]).arg(r[i])));
        if (i > firstBlown && r[i] != r[i - 1]) ++distinct;
    }
    QVERIFY2(distinct >= 12,
             qPrintable(QString("the %1 blown samples recovered to only %2 distinct "
                                "levels; recovery must keep them apart")
                        .arg(kRampN - firstBlown).arg(distinct)));
    QVERIFY2(blown(h) < blown(b) && blown(h) > blown(r),
             qPrintable(QString("Highlights -50 should recover part of the blowout: "
                                "%1 blown at 0, %2 at -50, %3 at -100")
                        .arg(blown(b)).arg(blown(h)).arg(blown(r))));
}

/*
    6b. RECOVERY DOES NOT COMPOUND WITH THE BAND. A version that undid the push in the
    highlights outright -- back to their Exposure-0 tones -- passed 6a and then failed in
    the app: the band pulled the same tones again, a pushed snow scene came out DARKER
    than the unedited one (90th percentile 126 against 162) and its tonal range fell to a
    fifth. Recovery is "no longer blown", not "unexposed", so a positive push at
    Highlights -100 must not render a highlight darker than the unpushed, unedited ramp.
    Up to level 235: above it the pushed top lands just under white BY DESIGN, so the
    unedited ramp's own last few levels are squeezed beneath it (255 renders 244).
*/
void tst_toneregions::highlightsRecoveryDoesNotCompound()
{
    EditParams plain;
    EditParams rec; rec.exposure = 1.5f; rec.highlights = -100.0f;

    const std::vector<int> u = render(plain), r = render(rec);
    QVERIFY(!u.empty() && !r.empty());

    for (int i = 0; i < kRampN; ++i) {
        if (u[i] < 100 || u[i] > 235) continue;
        QVERIFY2(r[i] >= u[i] - 2,
                 qPrintable(QString("sample %1: Exposure +1.5 with Highlights -100 rendered "
                                    "%2, darker than the unedited %3; recovery and the band "
                                    "are pulling the same tones twice")
                            .arg(i).arg(r[i]).arg(u[i])));
    }
}

/*
    7. HIGHLIGHTS - ADAPTS TO THE FRAME. Lightroom's Highlights pulls a dark frame's few
    highlights hard and a bright frame's many highlights gently -- strength x the band's
    mean weight over the frame is roughly constant (the budget, develop.cpp). The fixed
    0.22 band this replaced pulled level ~200 of a snow field 52 levels where Lightroom
    pulled 11. Same ramp in both frames; one frame is otherwise dark (fill at s=0.15),
    the other otherwise bright (s=0.85). The SAME ramp level must be pulled at least
    twice as hard in the dark frame.
*/
void tst_toneregions::highlightsAdaptsToTheFrame()
{
    constexpr int W = 512, From = 256;
    EditParams base, hi; hi.highlights = -100.0f;
    const WorkingImage dark = makeFrame(W, 4, From, 0.15f);
    const WorkingImage bright = makeFrame(W, 4, From, 0.85f);
    const std::vector<int> d0 = renderRow(dark, base), d1 = renderRow(dark, hi);
    const std::vector<int> b0 = renderRow(bright, base), b1 = renderRow(bright, hi);
    QVERIFY(!d0.empty() && !d1.empty() && !b0.empty() && !b1.empty());

    const int x = From + int(0.80f * (W - 1 - From));          // ramp at s = 0.80
    const int pullDark = d0[x] - d1[x], pullBright = b0[x] - b1[x];
    QVERIFY2(pullBright > 0 && pullDark >= 2 * pullBright,
             qPrintable(QString("Highlights -100 pulled level %1 by %2 in the dark frame "
                                "and %3 in the bright one; a dark frame's highlights must "
                                "take at least twice the pull")
                        .arg(d0[x]).arg(pullDark).arg(pullBright)));
}

/*
    8. THE STRENGTH IS THE FRAME'S, NOT THE BUFFER'S. The Detail panel renders a CROP of
    the full image. Measured on its own pixels, a crop of the bright end of a mostly dark
    frame would see a bright image, take a gentle pull and render unlike the loupe. The
    crop shares the frame's statistics sample (Develop::StatSample, filled from the full
    image before copyMetadata) and must render each pixel within a level of the full
    frame. The control -- the same crop filling the sample from itself -- must differ,
    or this test could not see the bug.
*/
void tst_toneregions::highlightsStatisticIsTheFrameNotTheCrop()
{
    constexpr int W = 512, From = 256, CropX = 384;
    EditParams hi; hi.highlights = -100.0f;
    const WorkingImage full = makeFrame(W, 4, From, 0.15f);

    auto crop = [&](bool shareFrame) {
        WorkingImage c;
        c.width = W - CropX; c.height = full.height;
        if (shareFrame) {
            Develop::StatSample(full);
            Develop::LocalSample(full);
            copyMetadata(c, full);
            /* As cutDetailRoi does: where the crop sits, for the frame's local base. */
            c.frameX = CropX; c.frameY = 0;
            c.frameW = W;     c.frameH = full.height;
        }
        else {
            WorkingImage meta = full;                    // a fresh, unfilled slot
            meta.statSlot = std::make_shared<StatSampleSlot>();
            copyMetadata(c, meta);
        }
        c.rgb.resize(size_t(c.width) * c.height * 3);
        for (int y = 0; y < c.height; ++y)
            std::copy(full.rgb.begin() + (size_t(y) * W + CropX) * 3,
                      full.rgb.begin() + (size_t(y) * W + W) * 3,
                      c.rgb.begin() + size_t(y) * c.width * 3);
        return c;
    };

    const std::vector<int> f = renderRow(full, hi);
    const std::vector<int> shared = renderRow(crop(true), hi);
    const std::vector<int> own = renderRow(crop(false), hi);
    QVERIFY(!f.empty() && !shared.empty() && !own.empty());

    int worst = 0, ownWorst = 0;
    for (int i = 0; i < W - CropX; ++i) {
        worst = std::max(worst, std::abs(shared[i] - f[CropX + i]));
        ownWorst = std::max(ownWorst, std::abs(own[i] - f[CropX + i]));
    }
    QVERIFY2(worst <= 1,
             qPrintable(QString("the crop rendered up to %1 levels away from the same "
                                "pixels in the full frame").arg(worst)));
    QVERIFY2(ownWorst >= 3,
             qPrintable(QString("control: a crop measuring itself differed by only %1 "
                                "levels, so this test cannot see the bug").arg(ownWorst)));
}

/*
    9. HIGHLIGHTS - IS LOCAL WITHOUT HALOS (2026-10-11). Lightroom pulls a bright region
    as a region: texture inside it rides along (its fine contrast even rises) and nothing
    haloes at the region's edge. Winnow does it with a local Laplacian filter on the
    frame's LocalSample (develop.cpp, kHighsLocalSigma), which v4's guided-filter region
    did not: bright fronds beside dark trunks were left unpulled, a milky halo. On a dark
    frame holding a bright pool of low-contrast checker texture:
      - the checker keeps more of its contrast than under the per-pixel band -- the same
        frame rendered as a crop with no LocalSample, which is the per-pixel fallback;
      - a pool pixel 2 px inside the edge is pulled within a level of the pool's centre,
        and dark background 3 px outside within a level of background far away.
*/
void tst_toneregions::highlightsPullsTheRegion()
{
    constexpr int W = 512, H = 256;
    constexpr float kDark = 0.15f, kPool = 0.55f, kAmp = 0.03f;
    WorkingImage img;
    img.width = W; img.height = H; img.white = 1.0f;
    img.sceneReferred = false;
    img.space = ColorSpaceMath::kWorking;
    img.rgb.resize(size_t(W) * H * 3);
    for (int y = 0; y < H; ++y)
        for (int x = 0; x < W; ++x) {
            float s = kDark;
            if (x >= 128 && x < 384 && y >= 64 && y < 192)
                s = kPool + (((x / 2 + y / 2) & 1) ? kAmp : -kAmp);
            const float v = std::pow(s, 2.2f);
            float *px = img.rgb.data() + (size_t(y) * W + x) * 3;
            px[0] = px[1] = px[2] = v;
        }

    auto renderAll = [&](const EditParams &pIn, bool local) {
        EditParams p = pIn;
        p.viewTransform = int(OutputTransform::ViewTransform::None);
        WorkingImage w = img;
        w.statSlot = std::make_shared<StatSampleSlot>();
        if (!local) {
            /* A crop of an unfilled local slot: the per-pixel fallback, same frame. */
            Develop::StatSample(w);
            w.frameX = 0; w.frameY = 0; w.frameW = W; w.frameH = H;
        }
        Develop dev;
        QImage out;
        OutputTransform ot;
        if (!dev.Apply(w, p) ||
            !ot.ToImage(w, out, OutputTransform::Space::sRGB,
                        OutputTransform::ViewTransform::None)) return QImage();
        return out;
    };
    EditParams base, hi; hi.highlights = -100.0f;
    const QImage b = renderAll(base, true), r = renderAll(hi, true);
    const QImage perPixel = renderAll(hi, false);
    QVERIFY(!b.isNull() && !r.isNull() && !perPixel.isNull());
    auto at = [](const QImage &q, int x, int y) { return int(q.constScanLine(y)[x * 3]); };
    auto pull = [&](int x, int y) { return at(b, x, y) - at(r, x, y); };
    auto texture = [&](const QImage &q) { return std::abs(at(q, 256, 128) - at(q, 258, 128)); };

    QVERIFY2(texture(r) >= texture(perPixel) + 1,
             qPrintable(QString("the pool's checker kept %1 levels of contrast under the "
                                "local operator and %2 under the per-pixel band (from %3); "
                                "the region's texture must ride along")
                        .arg(texture(r)).arg(texture(perPixel)).arg(texture(b))));

    const int centre = pull(256, 128), edgeIn = pull(130, 128);
    QVERIFY2(std::abs(edgeIn - centre) <= 1,
             qPrintable(QString("a pool pixel 2 px inside the edge was pulled %1 levels, "
                                "the centre %2: the region's edge haloes")
                        .arg(edgeIn).arg(centre)));
    const int nearPull = pull(128 - 3, 128), farPull = pull(16, 16);
    QVERIFY2(std::abs(nearPull - farPull) <= 1,
             qPrintable(QString("dark background 3 px from the pool was pulled %1 levels "
                                "against %2 far away: the region leaks (a halo)")
                        .arg(nearPull).arg(farPull)));
}

QTEST_APPLESS_MAIN(tst_toneregions)
#include "tst_toneregions.moc"
