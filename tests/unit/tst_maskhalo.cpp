/*
    MaskHalo -- the halo suppression behind the Develop Halo slider.

    WHAT IS ACTUALLY UNDER TEST. Not pixel values: the exact falloff is a tuning decision
    and pinning it would just cement today's constants. What must hold is the CONTRACT the
    slider makes to the user:

      - the rim between the mask's boundary and the picture's edge -- the halo itself --
        stops being fully adjusted,
      - the solid interior of a mask is left alone even where the picture is full of
        detail, because a halo control that punches holes in a textured subject is worse
        than the halo,
      - amount 0 is a bit-exact identity, because every mask in every existing sidecar
        renders through this code with amount 0, and
      - the SAME scene at proxy and at full resolution suppresses the same way, which is
        the preview == render contract every Develop spatial op signs.

    A CAUTION LEARNED THE HARD WAY. These fixtures are hard step edges, and a synthetic
    step is the easiest case there is. The rejected guided-filter approach (see the REFINE
    note in maskhalo.h) passed a displacement test on exactly this kind of fixture and
    then failed on a photograph, because a soft, low-contrast fur boundary is nothing like
    a step. Treat a green run here as "the arithmetic is intact", never as "it looks
    right" -- that question is only ever answered in the loupe.
*/
#include <QtTest>
#include <algorithm>
#include <cmath>
#include <vector>
#include "Develop/maskhalo.h"

/* Serial dispatcher: these buffers are small and the arithmetic, not the threading, is
   what is under test. */
static const auto serialPar = [](int n, auto &&fn) { fn(0, n); };

class TestMaskHalo : public QObject
{
    Q_OBJECT

private slots:
    void zeroAmountIsIdentity();
    void fadesTheRim();
    void strongerAmountFadesMore();
    void sparesTexturedInterior();
    void sparesCleanExterior();
    void alphaStaysInRange();
    void resolutionIndependence();
};

namespace {

struct Scene {
    std::vector<float> alpha, guide;
    int w = 0, h = 0;
    int guideEdge = 0;    // where the picture's edge is
    int alphaEdge = 0;    // where the mask's edge is (the error is the difference)
};

/* A vertical picture edge at guideEdge, and a mask whose edge overshoots it by errPx --
   the halo case: the mask covers a strip of "background" past the real boundary.
   Both edges are in BUFFER pixels; the caller scales when simulating a proxy. */
Scene halo(int w, int h, int guideEdge, int errPx)
{
    Scene s;
    s.w = w; s.h = h;
    s.guideEdge = guideEdge;
    s.alphaEdge = guideEdge + errPx;
    s.alpha.assign(size_t(w) * h, 0.0f);
    s.guide.assign(size_t(w) * h, 0.0f);
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x) {
            s.guide[size_t(y)*w + x] = (x < guideEdge) ? 0.15f : 0.85f;
            s.alpha[size_t(y)*w + x] = (x < s.alphaEdge) ? 1.0f : 0.0f;
        }
    return s;
}

/* Mean coverage over the rim -- the strip the mask should not have claimed. */
double rimCoverage(const Scene &s)
{
    const size_t row = size_t(s.h / 2) * s.w;
    double sum = 0.0;
    for (int x = s.guideEdge; x < s.alphaEdge; ++x) sum += s.alpha[row + x];
    return sum / double(s.alphaEdge - s.guideEdge);
}

} // namespace

/* Every mask in every existing sidecar renders through here with amount 0. */
void TestMaskHalo::zeroAmountIsIdentity()
{
    Scene s = halo(120, 16, 60, 6);
    const std::vector<float> before = s.alpha;
    MaskHalo::apply(s.alpha, s.guide, s.w, s.h, 0.0f, 1.0, serialPar);
    QCOMPARE(s.alpha, before);
    /* ...and anything under the no-op threshold, which is the same promise. */
    MaskHalo::apply(s.alpha, s.guide, s.w, s.h, MaskHalo::kMinAmount * 0.5f, 1.0,
                    serialPar);
    QCOMPARE(s.alpha, before);
}

/* THE HALO FIX, asserted directly: the strip of background the mask wrongly claimed
   stops being fully adjusted. */
void TestMaskHalo::fadesTheRim()
{
    Scene s = halo(200, 16, 100, 6);
    QCOMPARE(rimCoverage(s), 1.0);                 // the fixture really does build a halo
    MaskHalo::apply(s.alpha, s.guide, s.w, s.h, 100.0f, 1.0, serialPar);
    QVERIFY2(rimCoverage(s) < 0.5,
             qPrintable(QString("rim still at mean coverage %1").arg(rimCoverage(s))));
}

/* The slider has to be a control, not a switch: more must mean more. */
void TestMaskHalo::strongerAmountFadesMore()
{
    double prev = 1.01;
    for (int amt = 0; amt <= 100; amt += 20) {
        Scene s = halo(200, 16, 100, 6);
        MaskHalo::apply(s.alpha, s.guide, s.w, s.h, float(amt), 1.0, serialPar);
        const double cov = rimCoverage(s);
        QVERIFY2(cov <= prev + 1e-6,
                 qPrintable(QString("amount %1 raised rim coverage to %2 (was %3)")
                            .arg(amt).arg(cov).arg(prev)));
        prev = cov;
    }
    QVERIFY(prev < 0.5);           // a control that never moved would pass monotonicity
}

/* The suppression keys off the guide's gradient, and a subject's interior is full of
   gradient. Damping there would punch holes in exactly the detailed subjects masks are
   used on -- the band term exists to prevent it. */
void TestMaskHalo::sparesTexturedInterior()
{
    Scene s = halo(200, 16, 100, 6);
    for (int y = 0; y < s.h; ++y)                       // hard texture, well inside
        for (int x = 20; x < 50; ++x)
            s.guide[size_t(y)*s.w + x] = (x % 2) ? 0.95f : 0.05f;
    MaskHalo::apply(s.alpha, s.guide, s.w, s.h, 100.0f, 1.0, serialPar);
    const size_t row = size_t(s.h / 2) * s.w;
    for (int x = 20; x < 50; ++x)
        QVERIFY2(s.alpha[row + x] > 0.99f,
                 qPrintable(QString("interior x=%1 dropped to %2")
                            .arg(x).arg(s.alpha[row + x])));
}

/* Nothing outside the mask may become masked: this only ever takes coverage away. */
void TestMaskHalo::sparesCleanExterior()
{
    Scene s = halo(200, 16, 100, 6);
    MaskHalo::apply(s.alpha, s.guide, s.w, s.h, 100.0f, 1.0, serialPar);
    const size_t row = size_t(s.h / 2) * s.w;
    for (int x = s.alphaEdge; x < s.w; ++x) QCOMPARE(s.alpha[row + x], 0.0f);
}

/* A mask buffer outside 0..1 is a compositing bug wherever it lands downstream. */
void TestMaskHalo::alphaStaysInRange()
{
    Scene s = halo(128, 64, 60, 5);
    /* Roughen both fields so the pass meets noise, not just clean steps. */
    for (size_t k = 0; k < s.alpha.size(); ++k) {
        s.guide[k] = std::clamp(s.guide[k] + 0.3f * std::sin(float(k) * 0.7f),
                                0.0f, 1.0f);
        s.alpha[k] = std::clamp(s.alpha[k] + 0.2f * std::cos(float(k) * 1.3f),
                                0.0f, 1.0f);
    }
    MaskHalo::apply(s.alpha, s.guide, s.w, s.h, 100.0f, 1.0, serialPar);
    for (float v : s.alpha) QVERIFY(v >= 0.0f && v <= 1.0f);
}

/* PREVIEW == RENDER. The same picture at a 0.4 proxy and at full resolution must suppress
   the rim to the same degree. Get this wrong and the halo visibly changes the moment the
   settle render lands. */
void TestMaskHalo::resolutionIndependence()
{
    const double scale = 0.4;

    Scene full = halo(400, 12, 200, 20);
    MaskHalo::apply(full.alpha, full.guide, full.w, full.h, 100.0f, 1.0, serialPar);

    /* The same scene sampled at 0.4: every distance in it scales too. */
    Scene prox = halo(160, 12, 80, 8);
    MaskHalo::apply(prox.alpha, prox.guide, prox.w, prox.h, 100.0f, scale, serialPar);

    QVERIFY2(std::abs(rimCoverage(full) - rimCoverage(prox)) <= 0.1,
             qPrintable(QString("rim coverage full %1 vs proxy %2")
                        .arg(rimCoverage(full)).arg(rimCoverage(prox))));
}

QTEST_MAIN(TestMaskHalo)
#include "tst_maskhalo.moc"
