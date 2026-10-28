/*
    AutoTone -- the Basic panel's one-click Auto (Develop/autotone.h).

    The constants are fitted to Lightroom's Auto and will be refitted, so these tests pin
    BEHAVIOUR rather than numbers: a dark frame is brightened, a bright one darkened, a
    flat one gains contrast and full clip points, a frame that stays dark gets more
    Shadows lift, a raw a stronger Highlights pull than a JPEG, the colour push does not
    depend on the colour content (Lightroom's does not), and the answer does not depend
    on where the tone sliders were left.
*/

#include <QtTest>
#include <QImage>
#include <cmath>

#include "Develop/autotone.h"
#include "Develop/workingimage.h"
#include "Develop/editparams.h"

namespace {

/* A display-referred (JPEG-like) scene: a smooth luminance field with a coloured
   gradient over it, every value scaled by `gain` (linear), so one scene can be shot
   "under" or "over". chroma 0 makes it grey. */
WorkingImage makeScene(float gain, float chroma = 0.25f, float lo = 0.02f, float hi = 0.9f)
{
    const int w = 600, h = 400;
    WorkingImage img;
    img.width = w;
    img.height = h;
    img.sceneReferred = false;
    img.rgb.resize(size_t(w) * h * 3);
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            const float u = float(x) / (w - 1), v = float(y) / (h - 1);
            const float t = 0.5f + 0.5f * std::sin(6.0f * u + 3.0f * v);
            const float L = lo + (hi - lo) * t * t;            // skewed toward dark
            const size_t j = (size_t(y) * w + x) * 3;
            img.rgb[j + 0] = gain * L * (1.0f + chroma * (u - 0.5f));
            img.rgb[j + 1] = gain * L;
            img.rgb[j + 2] = gain * L * (1.0f + chroma * (v - 0.5f));
        }
    }
    return img;
}

AutoTone::Result solve(const WorkingImage &img, const EditParams &base = EditParams())
{
    AutoTone::Result r;
    if (!AutoTone::Solve(img, base, r)) qFatal("AutoTone::Solve failed");
    qInfo("  exp %+.2f con %+.0f hi %+.0f sh %+.0f wh %+.0f bl %+.0f vib %+.0f sat %+.0f"
          "  (%d renders)", r.exposure, r.contrast, r.highlights, r.shadows, r.whites,
          r.blacks, r.vibrance, r.saturation, r.renders);
    return r;
}

} // namespace

class TstAutoTone : public QObject
{
    Q_OBJECT
private slots:
    void percentilesInterpolate();
    void darkFrameIsBrightened();
    void brightFrameIsDarkened();
    void exposureOrdersWithGain();
    void flatFrameGetsContrastAndClipPoints();
    void colourPushIgnoresContent();
    void darkFrameGetsMoreShadowLift();
    void rawGetsStrongerHighlightPull();
    void independentOfCurrentSliders();
    void presetAutoToneMatchesSolve();
    void presetAutoWbNeedsCameraColour();
    void benchmarkFullFrame();
};

void TstAutoTone::percentilesInterpolate()
{
    QImage img(256, 1, QImage::Format_RGB888);
    for (int x = 0; x < 256; ++x) {
        uchar *p = img.scanLine(0) + x * 3;
        p[0] = p[1] = p[2] = uchar(x);
    }
    const AutoTone::Stats s = AutoTone::Measure(img);
    QCOMPARE(s.total, 256.0);
    QVERIFY(std::abs(s.percentile(0.5) - 0.5f) < 0.01f);
    QVERIFY(s.percentile(0.25) < s.percentile(0.26));    // continuous, not stepped by 0
    QVERIFY(std::abs(s.fractionAbove(0.75f) - 0.25f) < 0.01f);
    QVERIFY(std::abs(s.fractionBelow(0.25f) - 0.25f) < 0.01f);
}

void TstAutoTone::darkFrameIsBrightened()
{
    const AutoTone::Result r = solve(makeScene(0.12f));
    QVERIFY(r.exposure > 0.5f);
}

void TstAutoTone::brightFrameIsDarkened()
{
    const AutoTone::Result r = solve(makeScene(1.0f, 0.25f, 0.45f, 1.0f));
    QVERIFY(r.exposure < 0.0f);
    QVERIFY(r.highlights <= 0.0f);                      // never lifts the top
}

void TstAutoTone::exposureOrdersWithGain()
{
    const float a = solve(makeScene(0.1f)).exposure;
    const float b = solve(makeScene(0.3f)).exposure;
    const float c = solve(makeScene(0.9f)).exposure;
    QVERIFY(a > b);
    QVERIFY(b > c);
}

void TstAutoTone::flatFrameGetsContrastAndClipPoints()
{
    /* A hazy frame: nothing below ~25% or above ~60% grey. */
    const AutoTone::Result r = solve(makeScene(1.0f, 0.25f, 0.08f, 0.30f));
    QVERIFY(r.contrast > 0.0f);
    QVERIFY(r.whites > 0.0f);                           // stretch the top to white
    QVERIFY(r.blacks < 0.0f);                           // and the bottom to black
}

void TstAutoTone::colourPushIgnoresContent()
{
    /* Lightroom's Auto gives Vibrance ~+15 and Saturation ~0 to a grey frame and a
       saturated one alike. */
    const AutoTone::Result grey = solve(makeScene(0.4f, 0.0f));
    const AutoTone::Result vivid = solve(makeScene(0.4f, 0.6f));
    QVERIFY(grey.vibrance > 0.0f);
    QCOMPARE(grey.vibrance, vivid.vibrance);
    QCOMPARE(grey.saturation, 0.0f);
    QCOMPARE(vivid.saturation, 0.0f);
}

void TstAutoTone::darkFrameGetsMoreShadowLift()
{
    /* Exposure only partly corrects a very dark frame; what is still dark afterwards
       gets more Shadows and a lighter Blacks than a well-lit frame. */
    const AutoTone::Result dark = solve(makeScene(0.03f));
    const AutoTone::Result lit = solve(makeScene(1.0f, 0.25f, 0.45f, 1.0f));
    QVERIFY(dark.shadows > lit.shadows);
    QVERIFY(dark.blacks > lit.blacks);
}

void TstAutoTone::rawGetsStrongerHighlightPull()
{
    /* A JPEG already carries its camera's highlight roll-off. */
    WorkingImage raw = makeScene(0.4f);
    raw.sceneReferred = true;
    const AutoTone::Result r = solve(raw);
    const AutoTone::Result j = solve(makeScene(0.4f));
    QVERIFY(r.highlights < j.highlights);
    QVERIFY(j.highlights < 0.0f);
}

void TstAutoTone::independentOfCurrentSliders()
{
    const WorkingImage img = makeScene(0.3f);
    EditParams messy;
    messy.exposure = 2.0f;
    messy.contrast = -50.0f;
    messy.highlights = 80.0f;
    messy.shadows = -70.0f;
    messy.whites = 40.0f;
    messy.blacks = 30.0f;
    messy.vibrance = 60.0f;
    messy.saturation = -40.0f;
    const AutoTone::Result a = solve(img);
    const AutoTone::Result b = solve(img, messy);
    QCOMPARE(a.exposure, b.exposure);
    QCOMPARE(a.contrast, b.contrast);
    QCOMPARE(a.whites, b.whites);
    QCOMPARE(a.blacks, b.blacks);
    QCOMPARE(a.vibrance, b.vibrance);
}

/* A preset's "Auto tone" (ApplyPresetAutos) is the button's solve, written over
   whatever tone values the preset's literal merge left -- so a preset carrying both
   ends up with the solved ones. */
void TstAutoTone::presetAutoToneMatchesSolve()
{
    const WorkingImage img = makeScene(0.3f);
    const AutoTone::Result r = solve(img);
    EditParams p;
    p.exposure = 2.0f;                  // a literal value the preset also carried
    p.whites = -40.0f;
    p.texture = 25.0f;                  // not Auto's: must survive
    const AutoTone::AutosResult res = AutoTone::ApplyPresetAutos(img, false, true, p);
    QVERIFY(res.ok());
    QCOMPARE(p.exposure, r.exposure);
    QCOMPARE(p.contrast, r.contrast);
    QCOMPARE(p.highlights, r.highlights);
    QCOMPARE(p.shadows, r.shadows);
    QCOMPARE(p.whites, r.whites);
    QCOMPARE(p.blacks, r.blacks);
    QCOMPARE(p.vibrance, r.vibrance);
    QCOMPARE(p.saturation, r.saturation);
    QCOMPARE(p.texture, 25.0f);
}

/* Auto white balance solves through the camera characterisation; without one (this
   synthetic frame has none) it must fail CLEANLY: WB left as merged, reported, and Auto
   tone still run. */
void TstAutoTone::presetAutoWbNeedsCameraColour()
{
    const WorkingImage img = makeScene(0.3f);
    EditParams p;
    p.temp = 5200.0f;
    p.tint = 7.0f;
    const AutoTone::AutosResult res = AutoTone::ApplyPresetAutos(img, true, true, p);
    QVERIFY(!res.wbOk);
    QVERIFY(res.toneOk);
    QVERIFY(!res.ok());
    QCOMPARE(p.temp, 5200.0f);
    QCOMPARE(p.tint, 7.0f);
    QVERIFY(p.exposure > 0.0f);         // the dark frame was still brightened
}

/* NOT a test: the solve's stage timings on a 50 MP frame (8672 x 5784, a Sony ILCE-1M2
   raw), for choosing where to optimise. Skipped unless WINNOW_BENCH=1, so ctest stays
   fast. */
void TstAutoTone::benchmarkFullFrame()
{
    if (qEnvironmentVariableIntValue("WINNOW_BENCH") != 1)
        QSKIP("benchmark: set WINNOW_BENCH=1");
    const int w = 8672, h = 5784;
    WorkingImage img;
    img.width = w;
    img.height = h;
    img.sceneReferred = false;
    img.rgb.resize(size_t(w) * h * 3);
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x) {
            const float u = float(x) / (w - 1), v = float(y) / (h - 1);
            const float t = 0.5f + 0.5f * std::sin(6.0f * u + 3.0f * v);
            const float L = 0.3f * (0.02f + 0.88f * t * t);
            const size_t j = (size_t(y) * w + x) * 3;
            img.rgb[j + 0] = L * (1.0f + 0.25f * (u - 0.5f));
            img.rgb[j + 1] = L;
            img.rgb[j + 2] = L * (1.0f + 0.25f * (v - 0.5f));
        }
    for (int run = 0; run < 3; ++run) {
        img.statSlot = std::make_shared<StatSampleSlot>();   // a fresh base each run
        QElapsedTimer t;
        t.start();
        AutoTone::Result r;
        QVERIFY(AutoTone::Solve(img, EditParams(), r));
        qInfo("  run %d: total %lld ms = proxy %d + statSample %d + localSample %d + "
              "%d renders %d ms", run, t.elapsed(), r.proxyMs, r.sampleMs, r.localMs,
              r.renders, r.rendersMs);
    }
}

QTEST_GUILESS_MAIN(TstAutoTone)
#include "tst_autotone.moc"
