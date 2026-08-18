/*
    WorkingImageCache::renderStack -- the INTERACTIVE RESUME path.

    A Develop mask drag re-renders the scope stack many times a second. The resume
    mechanism (StackResume) lets a tick skip every scope below the one being edited by
    reusing the accumulator captured on the previous tick, and skip the develop pass for
    the edited scope itself when only its MASK moved. That is a correctness-critical
    shortcut: get it wrong and the loupe shows a stale composite that only resolves on the
    settle render, which is exactly the class of bug that is hard to see and easy to ship.

    So every test here asserts the same thing: a RESUMED render is bit-for-bit identical
    to the cold render of the same stack.

    See notes/Documentation.txt "SLIDER-DRAG LATENCY" and Develop/developstackcache.h.
*/

#include <QtTest>
#include <QImage>
#include <vector>
#include <memory>

#include "Develop/workingimagecache.h"
#include "Develop/workingimage.h"
#include "Develop/editparams.h"

namespace {

/* A deterministic scene-linear test image: a diagonal ramp with the channels offset so
   every develop op (per-channel or luminance-weighted) has something to bite on. */
WorkingImage makeWork(int w, int h)
{
    WorkingImage work;
    work.width = w;
    work.height = h;
    work.sceneReferred = false;      // display-referred: no baseline tone curve
    work.rgb.resize(size_t(w) * size_t(h) * 3);
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            const size_t j = (size_t(y) * w + x) * 3;
            const float u = float(x) / float(w - 1);
            const float v = float(y) / float(h - 1);
            work.rgb[j + 0] = 0.10f + 0.80f * u;
            work.rgb[j + 1] = 0.10f + 0.80f * v;
            work.rgb[j + 2] = 0.10f + 0.40f * (u + v);
        }
    }
    return work;
}

/* A horizontal ramp mask, shifted by `phase` so a "drag" can move it. */
std::shared_ptr<const std::vector<float>> makeMask(int w, int h, float phase)
{
    auto m = std::make_shared<std::vector<float>>(size_t(w) * size_t(h));
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x) {
            float t = float(x) / float(w - 1) + phase;
            t = t < 0.0f ? 0.0f : (t > 1.0f ? 1.0f : t);
            (*m)[size_t(y) * w + x] = t;
        }
    return m;
}

EditParams params(float exposure, float contrast, float saturation)
{
    EditParams p;
    p.exposure = exposure;
    p.contrast = contrast;
    p.saturation = saturation;
    return p;
}

/* Three masked scopes, the top one (index 2) carrying the mask a "drag" moves. */
std::vector<WorkingImageCache::StackScope> makeScopes(int w, int h, float topPhase,
                                                      const EditParams &topParams)
{
    std::vector<WorkingImageCache::StackScope> scopes(3);
    scopes[0].params = params(0.5f, 20.0f, 0.0f);
    scopes[0].mask   = makeMask(w, h, 0.0f);
    scopes[1].params = params(-0.4f, 0.0f, 30.0f);
    scopes[1].mask   = makeMask(w, h, -0.3f);
    scopes[2].params = topParams;
    scopes[2].mask   = makeMask(w, h, topPhase);
    return scopes;
}

} // namespace

class TestRenderStack : public QObject
{
    Q_OBJECT

private slots:
    void capturingDoesNotChangeOutput();
    void maskOnlyEditResumesIdentically();
    void paramsEditAtHotScopeRebuildsLayer();
    void resumeIsIgnoredWhenPrefixIsWrongSize();
    void scratchBackedMatchesLocal();
    void scratchSurvivesRepeatedTicks();

private:
    static constexpr int W = 61;      // not a multiple of the thread chunking
    static constexpr int H = 43;
};

/* Asking renderStack to CAPTURE intermediates must not perturb the render itself. */
void TestRenderStack::capturingDoesNotChangeOutput()
{
    const WorkingImage work = makeWork(W, H);
    const EditParams base = params(0.2f, 0.0f, 0.0f);
    const auto scopes = makeScopes(W, H, 0.0f, params(1.0f, -15.0f, 0.0f));

    QImage cold;
    QVERIFY(WorkingImageCache::renderStack(work, base, scopes, cold));

    WorkingImageCache::StackResume res;
    res.capture = 2;
    QImage withCapture;
    QVERIFY(WorkingImageCache::renderStack(work, base, scopes, withCapture, nullptr,
                                           WorkingImageCache::OutDepth::Eight,
                                           WorkingImageCache::Space::sRGB, &res));

    QCOMPARE(withCapture, cold);
    QVERIFY(res.outPrefix && res.outPrefix->isValid());
    QVERIFY(res.outLayer  && res.outLayer->isValid());
}

/* THE case this machinery exists for: the top scope's mask moved, nothing else changed.
   Resuming with both the cached prefix AND the cached layer -- so no develop pass runs at
   all -- must land on exactly the cold render. */
void TestRenderStack::maskOnlyEditResumesIdentically()
{
    const WorkingImage work = makeWork(W, H);
    const EditParams base = params(0.2f, 0.0f, 0.0f);
    const EditParams top = params(1.0f, -15.0f, 0.0f);

    /* Tick 1: cold render of the stack as it stands, capturing scope 2. */
    WorkingImageCache::StackResume cap;
    cap.capture = 2;
    QImage tick1;
    QVERIFY(WorkingImageCache::renderStack(work, base, makeScopes(W, H, 0.0f, top), tick1,
                                           nullptr, WorkingImageCache::OutDepth::Eight,
                                           WorkingImageCache::Space::sRGB, &cap));
    QVERIFY(cap.outPrefix);
    QVERIFY(cap.outLayer);

    /* Tick 2: the drag moved scope 2's mask. Params unchanged, so the layer is reused. */
    const auto moved = makeScopes(W, H, 0.35f, top);

    WorkingImageCache::StackResume res;
    res.start   = 2;
    res.prefix  = cap.outPrefix;
    res.layer   = cap.outLayer;
    res.capture = 2;
    QImage resumed;
    QVERIFY(WorkingImageCache::renderStack(work, base, moved, resumed, nullptr,
                                           WorkingImageCache::OutDepth::Eight,
                                           WorkingImageCache::Space::sRGB, &res));

    QImage cold;
    QVERIFY(WorkingImageCache::renderStack(work, base, moved, cold));
    QCOMPARE(resumed, cold);

    /* And the re-armed intermediates must still drive a correct THIRD tick -- the drag
       does not stop after one move. */
    const auto moved2 = makeScopes(W, H, 0.7f, top);
    WorkingImageCache::StackResume res2;
    res2.start   = 2;
    res2.prefix  = res.outPrefix;
    res2.layer   = res.outLayer;
    res2.capture = 2;
    QImage resumed2;
    QVERIFY(WorkingImageCache::renderStack(work, base, moved2, resumed2, nullptr,
                                           WorkingImageCache::OutDepth::Eight,
                                           WorkingImageCache::Space::sRGB, &res2));
    QImage cold2;
    QVERIFY(WorkingImageCache::renderStack(work, base, moved2, cold2));
    QCOMPARE(resumed2, cold2);
}

/* A SLIDER drag on the edited scope: the prefix is still valid (nothing below
   changed) but the cached layer is not, so the caller withholds it and renderStack
   must re-develop. */
void TestRenderStack::paramsEditAtHotScopeRebuildsLayer()
{
    const WorkingImage work = makeWork(W, H);
    const EditParams base = params(0.2f, 0.0f, 0.0f);

    WorkingImageCache::StackResume cap;
    cap.capture = 2;
    QImage tick1;
    const auto before = makeScopes(W, H, 0.0f, params(1.0f, -15.0f, 0.0f));
    QVERIFY(WorkingImageCache::renderStack(work, base, before, tick1, nullptr,
                                           WorkingImageCache::OutDepth::Eight,
                                           WorkingImageCache::Space::sRGB, &cap));

    const auto edited = makeScopes(W, H, 0.0f, params(-0.8f, 40.0f, -20.0f));

    WorkingImageCache::StackResume res;
    res.start   = 2;
    res.prefix  = cap.outPrefix;
    res.layer   = nullptr;          // params moved: must NOT offer the cached layer
    res.capture = 2;
    QImage resumed;
    QVERIFY(WorkingImageCache::renderStack(work, base, edited, resumed, nullptr,
                                           WorkingImageCache::OutDepth::Eight,
                                           WorkingImageCache::Space::sRGB, &res));

    QImage cold;
    QVERIFY(WorkingImageCache::renderStack(work, base, edited, cold));
    QCOMPARE(resumed, cold);
}

/* A prefix left over from a different proxy size must be rejected outright rather than
   indexed into -- the proxy is rebuilt on resize, zoom and image change. */
void TestRenderStack::resumeIsIgnoredWhenPrefixIsWrongSize()
{
    const WorkingImage work = makeWork(W, H);
    const EditParams base = params(0.2f, 0.0f, 0.0f);
    const auto scopes = makeScopes(W, H, 0.1f, params(0.6f, 10.0f, 0.0f));

    WorkingImageCache::StackResume res;
    res.start  = 2;
    res.prefix = std::make_shared<const WorkingImage>(makeWork(W / 2, H / 2));
    QImage resumed;
    QVERIFY(WorkingImageCache::renderStack(work, base, scopes, resumed, nullptr,
                                           WorkingImageCache::OutDepth::Eight,
                                           WorkingImageCache::Space::sRGB, &res));

    QImage cold;
    QVERIFY(WorkingImageCache::renderStack(work, base, scopes, cold));
    QCOMPARE(resumed, cold);
}


/* The render may be handed REUSED storage for its intermediates instead of allocating
   them (StackResume::accScratch / layScratch / outScratch). That exists purely to keep
   the allocator off the hot path -- an interactive tick was measured spending 72% of its
   time releasing these buffers -- so it must not change a single output pixel. */
void TestRenderStack::scratchBackedMatchesLocal()
{
    const WorkingImage work = makeWork(W, H);
    const EditParams base = params(0.2f, 0.0f, 0.0f);
    const auto scopes = makeScopes(W, H, 0.15f, params(0.9f, -12.0f, 8.0f));

    QImage local;
    QVERIFY(WorkingImageCache::renderStack(work, base, scopes, local));

    WorkingImageCache::StackResume res;
    res.capture     = 2;
    res.accScratch  = std::make_shared<WorkingImage>();
    res.layScratch  = std::make_shared<WorkingImage>();
    res.outScratch  = std::make_shared<WorkingImage>();
    res.preScratch  = std::make_shared<WorkingImage>();
    QImage scratched;
    QVERIFY(WorkingImageCache::renderStack(work, base, scopes, scratched, nullptr,
                                           WorkingImageCache::OutDepth::Eight,
                                           WorkingImageCache::Space::sRGB, &res));
    QCOMPARE(scratched, local);
    QVERIFY(res.outPrefix && res.outPrefix->isValid());
    QVERIFY(res.outLayer  && res.outLayer->isValid());
}

/* A drag re-enters with the SAME scratch tick after tick, which is the case the buffers
   exist for and the one where a stale size or a buffer written while still being served
   would show up. Each tick must still equal a cold render, including the tick that
   resumes from the previous tick's captured intermediates. */
void TestRenderStack::scratchSurvivesRepeatedTicks()
{
    const WorkingImage work = makeWork(W, H);
    const EditParams base = params(0.2f, 0.0f, 0.0f);

    auto accS = std::make_shared<WorkingImage>();
    auto layS = std::make_shared<WorkingImage>();
    /* Two output buffers, alternated exactly as DevelopStackCache::outScratch() does:
       never hand the render the buffer it is currently serving back as `layer`. */
    auto outA = std::make_shared<WorkingImage>();
    auto outB = std::make_shared<WorkingImage>();
    /* And the same for the prefix snapshot, which alternates on its own schedule -- a
       Global drag rejects the cached prefix every tick and captures a fresh one. */
    auto preA = std::make_shared<WorkingImage>();
    auto preB = std::make_shared<WorkingImage>();

    std::shared_ptr<const WorkingImage> prefix, layer;
    for (int tick = 0; tick < 4; ++tick) {
        const EditParams top = params(0.5f + 0.3f * tick, -10.0f * tick, 0.0f);
        const auto scopes = makeScopes(W, H, 0.1f * tick, top);

        WorkingImageCache::StackResume res;
        res.start      = 2;
        res.prefix     = prefix;
        res.layer      = nullptr;          // params move every tick: withhold the layer
        res.capture    = 2;
        res.accScratch = accS;
        res.layScratch = layS;
        res.outScratch = (layer == outA) ? outB : outA;
        res.preScratch = (prefix == preA) ? preB : preA;

        QImage got;
        QVERIFY(WorkingImageCache::renderStack(work, base, scopes, got, nullptr,
                                               WorkingImageCache::OutDepth::Eight,
                                               WorkingImageCache::Space::sRGB, &res));
        QImage cold;
        QVERIFY(WorkingImageCache::renderStack(work, base, scopes, cold));
        QVERIFY2(got == cold, qPrintable(QString("tick %1 differs from cold").arg(tick)));

        prefix = res.outPrefix;
        layer  = res.outLayer;
    }
}

QTEST_MAIN(TestRenderStack)
#include "tst_renderstack.moc"
