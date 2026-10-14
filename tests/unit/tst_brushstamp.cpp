/*
    BrushStamp auto-mask -- the per-dab acceptance field (buildDabField).

    The model is RELATIVE (which class under the brush does a pixel look like?) and runs
    its search inside the CORE ring only; see the Auto-mask section of brushstamp.h. These
    pin the behaviours that were each a field failure once: a soft edge between two real
    classes must stop the dab (the defocused-gravel case, 2026-10-14), a single textured
    surface must fill without holes, equal-lightness colours must still separate (the
    brown cone on grey), and the feather band must never add area past an edge the core
    did not see. Synthetic images, a fixed noise seed, so every number is reproducible.
*/
#include <QtTest>
#include <cmath>
#include <random>
#include "Develop/brushstamp.h"

namespace {

struct Rgb { double r, g, b; };

/* A w x h image split at row `edge` (top colour above, bottom below), the split
   softened by a box blur of radius `soft` (0 = hard), plus per-half luminance noise of
   amplitude nTop / nBot, lightly smoothed so it reads as texture rather than as single
   hot pixels. Converted to the guide's YCbCr exactly as ImageView::ensureAutoGuide
   does. */
BrushStamp::Guide makeGuide(int w, int h, int edge, Rgb top, Rgb bot, double nTop,
                            double nBot, int soft = 0)
{
    std::vector<double> rgb(size_t(w) * h * 3);
    for (int y = 0; y < h; ++y) {
        double t = y < edge ? 0.0 : 1.0;
        if (soft > 0) t = std::clamp((y - edge + soft + 0.5) / (2.0 * soft + 1.0), 0.0, 1.0);
        for (int x = 0; x < w; ++x) {
            double *p = &rgb[(size_t(y) * w + x) * 3];
            p[0] = top.r + (bot.r - top.r) * t;
            p[1] = top.g + (bot.g - top.g) * t;
            p[2] = top.b + (bot.b - top.b) * t;
        }
    }
    std::mt19937 rng(12345);
    std::normal_distribution<double> nd(0.0, 1.0);
    std::vector<double> noise(size_t(w) * h), sm(size_t(w) * h, 0.0);
    for (double &v : noise) v = nd(rng);
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x) {
            double s = 0.0;
            for (int dy = -1; dy <= 1; ++dy)
                for (int dx = -1; dx <= 1; ++dx)
                    s += noise[size_t(std::clamp(y + dy, 0, h - 1)) * w
                               + std::clamp(x + dx, 0, w - 1)];
            sm[size_t(y) * w + x] = s / 3.0;        // sum of 9 unit normals / 3 = unit sd
        }

    BrushStamp::Guide g;
    g.w = w; g.h = h; g.ycc.resize(size_t(w) * h * 3);
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x) {
            const size_t k = size_t(y) * w + x;
            const double n = sm[k] * (y < edge ? nTop : nBot);
            const double r = std::clamp(rgb[k * 3] + n, 0.0, 255.0);
            const double gr = std::clamp(rgb[k * 3 + 1] + n, 0.0, 255.0);
            const double b = std::clamp(rgb[k * 3 + 2] + n, 0.0, 255.0);
            const double Y = 0.299 * r + 0.587 * gr + 0.114 * b;
            g.ycc[k * 3]     = uchar(std::clamp(Y, 0.0, 255.0));
            g.ycc[k * 3 + 1] = uchar(std::clamp(128.0 + 0.564 * (b - Y), 0.0, 255.0));
            g.ycc[k * 3 + 2] = uchar(std::clamp(128.0 + 0.713 * (r - Y), 0.0, 255.0));
        }
    return g;
}

/* Mean field value over guide pixels within `rMax` of (cx,cy) whose row lies in
   [y0, y1). */
double meanIn(const BrushStamp::DabField &f, double cx, double cy, double rMax,
              int y0, int y1)
{
    double s = 0.0;
    int n = 0;
    for (int y = y0; y < y1; ++y)
        for (int x = int(cx - rMax); x <= int(cx + rMax); ++x) {
            if (std::hypot(x + 0.5 - cx, y + 0.5 - cy) > rMax) continue;
            s += f.at(x + 0.5, y + 0.5);
            ++n;
        }
    return n ? s / n : -1.0;
}

const Rgb kGrey  { 128, 128, 128 };
const Rgb kBrown { 170, 120,  60 };     // the cone: ~the grey's lightness, other chroma

}   // namespace

class TestBrushStamp : public QObject
{
    Q_OBJECT

private slots:
    void softEdgeStopsTheDab();
    void equalLightnessSeparates();
    void lightnessStepReachesEdge();
    void silhouetteSeparates();
    void textureSeparatesFromSmooth();
    void uniformTextureFillsCore();
    void gradientFillsCore();
    void featherBandAddsNoArea();
    void deterministic();
};

/* The defocused-edge case: two classes joined by a ~13px ramp, the brush centred in the
   lower one with its core over the ramp. Nothing beyond the ramp is painted, everything
   on the brush's side is. */
void TestBrushStamp::softEdgeStopsTheDab()
{
    const auto g = makeGuide(200, 200, 100, kGrey, kBrown, 4, 4, 6);
    BrushStamp::DabField f;
    BrushStamp::buildDabField(g, 100, 115, 50, 42, f);
    QVERIFY(!f.bypass);
    QVERIFY2(meanIn(f, 100, 115, 42, 0, 92) < 0.02, "painted past a soft edge");
    QVERIFY2(meanIn(f, 100, 115, 42, 108, 200) > 0.95, "holes on the brush's side");
}

void TestBrushStamp::equalLightnessSeparates()
{
    const auto g = makeGuide(200, 200, 100, kGrey, kBrown, 5, 5);
    BrushStamp::DabField f;
    BrushStamp::buildDabField(g, 100, 115, 50, 42, f);
    QVERIFY(!f.bypass);
    QVERIFY(meanIn(f, 100, 115, 42, 0, 96) < 0.02);
    QVERIFY(meanIn(f, 100, 115, 42, 104, 200) > 0.95);
}

/* A light wall against a dark one: a pure lightness step, no chroma, both sides smooth,
   the dark side brightening gently away from the edge. The fill must reach the edge.
   The field case (2026-10-14) stopped 10-30 px short: the analysis blur smeared the
   step into a band that read as TEXTURE, and its in-between colours formed clusters that
   either rejected the dark side's own rim or chained the merge across. */
void TestBrushStamp::lightnessStepReachesEdge()
{
    auto g = makeGuide(260, 260, 100, Rgb{105, 105, 105}, Rgb{60, 60, 60}, 1.5, 1.5, 1);
    for (int y = 100; y < g.h; ++y)                 // dark side: +12 levels over 120 px
        for (int x = 0; x < g.w; ++x) {
            uchar &Y = g.ycc[(size_t(y) * g.w + x) * 3];
            Y = uchar(std::min(255, Y + (y - 100) / 10));
        }
    BrushStamp::DabField f;
    BrushStamp::buildDabField(g, 130, 160, 75, 72, f);
    QVERIFY(!f.bypass);
    QVERIFY2(meanIn(f, 130, 160, 72, 104, 112) > 0.95, "stopped short of the edge");
    QVERIFY2(meanIn(f, 130, 160, 72, 0, 96) < 0.02, "painted the light wall");
}

/* Same hue, far apart in lightness: the merge's down-weighted Y must not join them. */
void TestBrushStamp::silhouetteSeparates()
{
    const auto g = makeGuide(200, 200, 100, Rgb{180, 190, 200}, Rgb{40, 45, 50}, 3, 3);
    BrushStamp::DabField f;
    BrushStamp::buildDabField(g, 100, 115, 50, 42, f);
    QVERIFY(!f.bypass);
    QVERIFY(meanIn(f, 100, 115, 42, 0, 96) < 0.02);
    QVERIFY(meanIn(f, 100, 115, 42, 104, 200) > 0.95);
}

/* One colour, sharp texture below and smooth above (sharp detail against bokeh): only
   the texture plane tells them apart. */
void TestBrushStamp::textureSeparatesFromSmooth()
{
    const Rgb c { 150, 110, 70 };
    const auto g = makeGuide(200, 200, 100, c, c, 1, 18);
    BrushStamp::DabField f;
    BrushStamp::buildDabField(g, 100, 115, 50, 42, f);
    QVERIFY(!f.bypass);
    QVERIFY(meanIn(f, 100, 115, 42, 0, 96) < 0.05);
    QVERIFY(meanIn(f, 100, 115, 42, 104, 200) > 0.95);
}

/* A single heavily textured surface is ONE class: no holes (Lightroom's brush holes
   gravel on every pebble; the merge is what prevents it here). */
void TestBrushStamp::uniformTextureFillsCore()
{
    const Rgb c { 160, 140, 120 };
    const auto g = makeGuide(200, 200, 100, c, c, 14, 14);
    BrushStamp::DabField f;
    BrushStamp::buildDabField(g, 100, 100, 50, 42, f);
    QVERIFY(!f.bypass);
    QVERIFY(meanIn(f, 100, 100, 41, 0, 200) > 0.98);
}

/* A smooth sky gradient across the whole brush is one surface too. */
void TestBrushStamp::gradientFillsCore()
{
    const auto g = makeGuide(200, 200, 200, Rgb{150, 200, 255}, Rgb{150, 200, 255}, 1, 1);
    BrushStamp::Guide gg = g;
    for (int y = 0; y < gg.h; ++y)                  // Y falls 60 levels down the frame
        for (int x = 0; x < gg.w; ++x)
            gg.ycc[(size_t(y) * gg.w + x) * 3] =
                uchar(std::clamp(int(gg.ycc[(size_t(y) * gg.w + x) * 3]) - y * 60 / 200,
                                 0, 255));
    BrushStamp::DabField f;
    BrushStamp::buildDabField(gg, 100, 100, 50, 42, f);
    QVERIFY(!f.bypass);
    QVERIFY(meanIn(f, 100, 100, 41, 0, 200) > 0.98);
}

/* The edge sits in the FEATHER BAND only (rows < 58; the core reaches row 58): the core
   sees one class and paints, but the band must not carry that across the edge. */
void TestBrushStamp::featherBandAddsNoArea()
{
    const auto g = makeGuide(200, 200, 56, kGrey, kBrown, 3, 3);
    BrushStamp::DabField f;
    BrushStamp::buildDabField(g, 100, 100, 50, 42, f);
    QVERIFY(!f.bypass);
    QVERIFY2(meanIn(f, 100, 100, 50, 0, 54) < 0.02, "the feather band crossed the edge");
    QVERIFY(meanIn(f, 100, 100, 41, 60, 200) > 0.98);
}

/* A pure function of its inputs: the render caches and the preview/render identity
   both assume it (k-means is farthest-point seeded, no RNG). */
void TestBrushStamp::deterministic()
{
    const auto g = makeGuide(200, 200, 100, kGrey, kBrown, 8, 8, 4);
    BrushStamp::DabField a, b;
    BrushStamp::buildDabField(g, 97.3, 112.6, 47, 30, a);
    BrushStamp::buildDabField(g, 97.3, 112.6, 47, 30, b);
    QCOMPARE(a.bypass, b.bypass);
    QCOMPARE(a.a.size(), b.a.size());
    QVERIFY(a.a == b.a);
}

QTEST_GUILESS_MAIN(TestBrushStamp)
#include "tst_brushstamp.moc"
