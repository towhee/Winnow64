/*
    AutoTone -- the Basic panel's one-click Auto (Develop/autotone.h).

    The tuning is heuristic and will be calibrated against Lightroom, so these tests pin
    BEHAVIOUR rather than numbers: a dark frame is brightened, a bright one darkened, a
    flat one gains contrast and full clip points, a grey frame gets no colour push, and
    the answer does not depend on where the tone sliders were left.
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
    void greyFrameGetsNoColourPush();
    void colourPushIsContinuous();
    void independentOfCurrentSliders();
};

void TstAutoTone::percentilesInterpolate()
{
    QImage img(256, 1, QImage::Format_RGB888);
    for (int x = 0; x < 256; ++x) {
        uchar *p = img.scanLine(0) + x * 3;
        p[0] = p[1] = p[2] = uchar(x);
    }
    const AutoTone::Stats s = AutoTone::Measure(img, false);
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

void TstAutoTone::greyFrameGetsNoColourPush()
{
    const AutoTone::Result r = solve(makeScene(0.4f, 0.0f));
    QCOMPARE(r.vibrance, 0.0f);
    QCOMPARE(r.saturation, 0.0f);
}

void TstAutoTone::colourPushIsContinuous()
{
    /* Chroma stepped finely across the monochrome threshold: Vibrance must ramp, never
       jump by more than a few units between neighbours. */
    float prev = solve(makeScene(0.4f, 0.0f)).vibrance;
    for (float c = 0.05f; c <= 0.61f; c += 0.05f) {
        const float v = solve(makeScene(0.4f, c)).vibrance;
        QVERIFY2(std::abs(v - prev) <= 8.0f, qPrintable(QString("%1 -> %2 at chroma %3")
                                                        .arg(prev).arg(v).arg(c)));
        prev = v;
    }
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

QTEST_GUILESS_MAIN(TstAutoTone)
#include "tst_autotone.moc"
