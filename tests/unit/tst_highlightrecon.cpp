/*
    HighlightRecon::Reconstruct -- partly clipped channels rebuilt, fully clipped ones
    neutral, no colour borrowed from the wrong place, no step where a channel clips.

    Ported with the algorithm from LightCraft's suite (crates/raw/src/highlight/tests.rs,
    MIT). Scenes are built in WHITE-BALANCED terms and turned into sensor values (divided by
    daylight-like multipliers, clipped at 1): green clips first, red and blue keep rising.

    What is pinned, and why each matters:

      o fringes and dark branches do not tint a clipped sky (LightCraft #523): beside a
        branch the clipped sky is the same as away from it, the sky's colour on its way
        to neutral;
      o two clipped channels are not left at their clip levels (that reads magenta/red);
      o fully clipped pixels follow the colour model's white, a white that can't be right
        falls back to camera neutral, nothing overflows;
      o the rebuilt colour has no step as more channels clip, along a ramp, or along a
        thin line with no reliable colour around;
      o a pale sky by dark branches and bright foliage is not tinted (#523, Highlights -75);
      o unclipped pixels are left EXACTLY, and the count is the clipped pixels;
      o a clipped channel of a warm gradient is restored near its unclipped part;
      o the result does not depend on the thread count.

    Set WINNOW_HIGHLIGHT_BENCH=1 to also print 24 MP throughput.
*/

#include <QtTest>
#include <QElapsedTimer>
#include <array>
#include <cmath>
#include <functional>
#include <opencv2/core.hpp>

#include "ImageFormats/Raw/highlightrecon.h"

namespace {

using V3 = std::array<float, 3>;
const float kWb[3] = { 2.5f, 1.0f, 1.6f };      // daylight-like: green clips first
const float kOne[3] = { 1.0f, 1.0f, 1.0f };

struct Img {
    int w = 0, h = 0;
    std::vector<float> d;
    V3 Get(int x, int y) const {
        const size_t i = (static_cast<size_t>(y) * w + x) * 3;
        return { d[i], d[i + 1], d[i + 2] };
    }
};

Img FromFn(int w, int h, const std::function<V3(int, int)> &f)
{
    Img im{ w, h, std::vector<float>(static_cast<size_t>(w) * h * 3) };
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x) {
            const V3 v = f(x, y);
            const size_t i = (static_cast<size_t>(y) * w + x) * 3;
            im.d[i] = v[0]; im.d[i + 1] = v[1]; im.d[i + 2] = v[2];
        }
    return im;
}

V3 Balanced(const V3 &p, const float wb[3] = kWb) { return { p[0] * wb[0], p[1] * wb[1], p[2] * wb[2] }; }
V3 Sensor(const V3 &q) { return { std::min(q[0] / kWb[0], 1.0f), std::min(q[1] / kWb[1], 1.0f), std::min(q[2] / kWb[2], 1.0f) }; }
V3 Chroma(const V3 &q) { const float s = q[0] + q[1] + q[2]; return { q[0] / s, q[1] / s, q[2] / s }; }
float MaxDiff(const V3 &a, const V3 &b) { return std::max({ std::fabs(a[0] - b[0]), std::fabs(a[1] - b[1]), std::fabs(a[2] - b[2]) }); }

int Recon(Img &im, const float wb[3] = kWb, float clip = 0.99f, const float white[3] = kOne)
{
    return HighlightRecon::Reconstruct(im.d.data(), im.w, im.h, wb, clip, white);
}

/* How far chromaticity got lies off the way from `from` to neutral. */
float OffTheWayToNeutral(const V3 &got, const V3 &from)
{
    V3 d;
    float len = 0;
    for (int c = 0; c < 3; ++c) { d[c] = 1.0f / 3.0f - from[c]; len += d[c] * d[c]; }
    float t = 0;
    if (len > 0) {
        for (int c = 0; c < 3; ++c) t += (got[c] - from[c]) * d[c];
        t = std::clamp(t / len, 0.0f, 1.0f);
    }
    float m = 0;
    for (int c = 0; c < 3; ++c) m = std::max(m, std::fabs(got[c] - (from[c] + d[c] * t)));
    return m;
}

/* A smooth pink ramp [0.9k, k, 0.8k], k 0.8..1.35, in `rows` of an otherwise near-black
   picture: the rebuilt middle row (white-balanced) and where green first clips on it. */
std::pair<std::vector<V3>, int> RebuiltRamp(int r0, int r1, int h)
{
    const int w = 1600;
    auto k = [&](int x) { return 0.8f + 0.55f * x / float(w - 1); };
    const Img img0 = FromFn(w, h, [&](int x, int y) -> V3 {
        if (y >= r0 && y < r1) return { std::min(0.9f * k(x), 1.0f), std::min(k(x), 1.0f), std::min(0.8f * k(x), 1.0f) };
        return { 0.01f, 0.01f, 0.01f };
    });
    Img img = img0;
    Recon(img);
    const int y = (r0 + r1) / 2;
    std::vector<V3> row(w);
    for (int x = 0; x < w; ++x) row[x] = Balanced(img.Get(x, y));
    int first = 0;
    for (int x = 0; x < w; ++x) if (img0.Get(x, y)[1] >= 0.99f) { first = x; break; }
    return { row, first };
}

void AssertContinuous(const std::vector<V3> &row, int first, float atClip, const QString &what)
{
    const V3 a = row[first - 1], b = row[first];
    for (int c = 0; c < 3; ++c)
        QVERIFY2(std::fabs(b[c] / a[c] - 1.0f) < atClip, qPrintable(what + ": step at the first clip"));
    float worst = 0;
    for (size_t x = 1; x < row.size(); ++x)
        for (int c = 0; c < 3; ++c) worst = std::max(worst, std::fabs(row[x][c] / row[x - 1][c] - 1.0f));
    QVERIFY2(worst < 0.03f, qPrintable(what + QString(": largest step %1").arg(worst)));
}

} // namespace

class TestHighlightRecon : public QObject
{
    Q_OBJECT

private slots:
    void fringesAndDarkBranchesDoNotTintAClippedSky();
    void twoClippedChannelsAreNotLeftAtTheirClipLevels();
    void fullyClippedPixelsFollowTheColourModelWhite();
    void rebuiltColourIsContinuousAsMoreChannelsClip();
    void rebuiltRampIsContinuousFromTheFirstClipToTheLast();
    void thinLineWithoutReliableColourIsContinuousWhereItClips();
    void paleSkyByDarkBranchesAndFoliageIsNotTinted();
    void unclippedPixelsAreLeftExactly();
    void reconstructRestoresAClippedChannel();
    void fullyClippedAndNoClipping();
    void resultIsIndependentOfThreadCount();
    void throughput();
};

void TestHighlightRecon::fringesAndDarkBranchesDoNotTintAClippedSky()
{
    /* a pale blue sky whose green clipped, an unclipped strip of it along the top, and a
       dark branch with purple fringes standing in the clipped part */
    const V3 sky = { 1.10f, 1.25f, 1.45f };
    Img img = FromFn(192, 128, [&](int x, int y) {
        if (y < 24) return Sensor({ sky[0] * 0.7f, sky[1] * 0.7f, sky[2] * 0.7f });
        if (x >= 90 && x < 100 && y >= 40) return Sensor({ 0.06f, 0.05f, 0.03f });
        if ((x == 89 || x == 100) && y >= 40) return Sensor({ 0.75f, 0.25f, 0.96f });
        return Sensor(sky);
    });
    QVERIFY(Recon(img) > 0);
    float worst = 0;
    for (int y = 40; y < 128; ++y) {
        const V3 away = Chroma(Balanced(img.Get(30, y)));
        QVERIFY2(away == Chroma(Balanced(img.Get(170, y))), qPrintable(QString("row %1").arg(y)));
        const float off = OffTheWayToNeutral(away, Chroma(sky));
        QVERIFY2(off < 0.005f, qPrintable(QString("row %1: clipped sky off the sky's way to neutral by %2").arg(y).arg(off)));
        for (int x : { 84, 86, 88, 101, 103, 105 })
            worst = std::max(worst, MaxDiff(Chroma(Balanced(img.Get(x, y))), away));
    }
    QVERIFY2(worst < 0.01f, qPrintable(QString("clipped sky beside the branch is off by %1").arg(worst)));
}

void TestHighlightRecon::twoClippedChannelsAreNotLeftAtTheirClipLevels()
{
    const V3 light = { 3.5f, 2.6f, 0.8f };
    Img img = FromFn(64, 64, [&](int x, int y) -> V3 {
        const float d = std::hypot(x - 32.0f, y - 32.0f);
        return d < 20.0f ? Sensor(light) : V3{ 0.02f, 0.02f, 0.02f };
    });
    Recon(img);
    const V3 q = Balanced(img.Get(32, 32));
    QVERIFY2(std::fabs(q[2] - 0.8f) < 0.1f, "the unclipped channel is about kept");
    const float ratio = q[1] / q[0];
    QVERIFY2(ratio >= 0.6f && ratio <= 1.05f, qPrintable(QString("green/red %1: not a warm white").arg(ratio)));
}

void TestHighlightRecon::fullyClippedPixelsFollowTheColourModelWhite()
{
    const float white[3] = { 1.03f, 0.98f, 1.06f };
    Img img = FromFn(8, 8, [](int x, int y) -> V3 { return (x == 0 && y == 0) ? V3{ 0.3f, 0.6f, 0.4f } : V3{ 1, 1, 1 }; });
    Recon(img, kWb, 0.99f, white);
    const V3 q = Balanced(img.Get(4, 4));
    const float k = q[1] / white[1];
    for (int c = 0; c < 3; ++c) QVERIFY2(std::fabs(q[c] - white[c] * k) < 1e-4f * k, "not along the white");
    QVERIFY2(k >= 2.5f * 0.99f - 1e-4f, "clipped neutral at least as bright as the highest clip level");
    QCOMPARE(img.Get(0, 0), (V3{ 0.3f, 0.6f, 0.4f }));

    /* a white that cannot be right falls back to camera neutral, and nothing overflows */
    const float bad[4][3] = { { NAN, 1, 1 }, { 2.5e38f, 0.74f, 0.74f }, { 1, 0, 1 }, { 40, 1, 1 } };
    for (const auto &b : bad) {
        Img im = FromFn(4, 4, [](int x, int y) -> V3 { return (x == 0 && y == 0) ? V3{ 1, 1, 0.5f } : V3{ 1, 1, 1 }; });
        Recon(im, kWb, 0.99f, b);
        const V3 qq = Balanced(im.Get(1, 1));
        QVERIFY(std::fabs(qq[0] - qq[1]) < 1e-4f && std::fabs(qq[2] - qq[1]) < 1e-4f);
        for (float v : im.d) QVERIFY(std::isfinite(v));
    }
}

void TestHighlightRecon::rebuiltColourIsContinuousAsMoreChannelsClip()
{
    /* red rises through its clip across a sky whose green and blue clipped */
    const V3 blue = { 1.0f, 1.3f, 1.9f };
    Img img = FromFn(240, 40, [&](int x, int y) {
        if (y < 8) return Sensor({ blue[0] * 0.5f, blue[1] * 0.5f, blue[2] * 0.5f });
        const float level = 0.6f + 2.2f * x / 239.0f;
        return Sensor({ level * kWb[0] * 0.4f, 1.6f, 2.0f });
    });
    Recon(img);
    float step = 0;
    V3 prev = Chroma(Balanced(img.Get(0, 30)));
    for (int x = 1; x < 240; ++x) {
        const V3 cur = Chroma(Balanced(img.Get(x, 30)));
        step = std::max(step, MaxDiff(cur, prev));
        prev = cur;
    }
    QVERIFY2(step < 0.01f, qPrintable(QString("largest chromaticity step %1").arg(step)));
    for (float v : prev) QVERIFY2(std::fabs(v - 1.0f / 3.0f) < 1e-3f, "fully clipped end is not neutral");
}

void TestHighlightRecon::rebuiltRampIsContinuousFromTheFirstClipToTheLast()
{
    const auto [row, first] = RebuiltRamp(0, 32, 32);
    QVERIFY(first > 0);
    AssertContinuous(row, first, 0.002f, "full ramp");
}

void TestHighlightRecon::thinLineWithoutReliableColourIsContinuousWhereItClips()
{
    for (int height : { 32, 512, 1024 })
        for (int width : { 1, 4, 16 }) {
            const int top = (height - width) / 2;
            const auto [row, first] = RebuiltRamp(top, top + width, height);
            const QString what = QString("%1 rows of %2").arg(width).arg(height);
            AssertContinuous(row, first, 0.002f, what);
            /* well past the first clip the neutral guess has taken over */
            const V3 far = row[1500];
            QVERIFY2(far[1] > 0.95f * far[2], qPrintable(what + ": neutral guess has not taken over"));
        }
}

void TestHighlightRecon::paleSkyByDarkBranchesAndFoliageIsNotTinted()
{
    const V3 sky = { 1.0f, 1.07f, 1.2f };
    const int w = 320, h = 96;
    auto k = [&](int x) { return 0.7f * std::exp2(1.2f * x / float(w - 1)); };
    const int branches[3] = { 150, 200, 250 };
    auto onBranch = [&](int x) { for (int b : branches) if (x >= b - 1 && x <= b + 2) return true; return false; };
    const Img img0 = FromFn(w, h, [&](int x, int y) {
        if (y >= 72) return Sensor({ 0.5f, 0.6f, 0.22f });
        for (int b : branches) if (y >= 40 && (x == b || x == b + 1)) return Sensor({ 0.1f, 0.09f, 0.04f });
        return Sensor({ sky[0] * k(x), sky[1] * k(x), sky[2] * k(x) });
    });
    Img img = img0;
    Recon(img);
    auto at = [&](int x, int y) { return Chroma(Balanced(img.Get(x, y))); };
    int first = 0;
    for (int x = 0; x < w; ++x) if (img0.Get(x, 20)[1] >= 0.99f) { first = x; break; }
    QVERIFY(first > 0 && first < 150);
    for (int y : { 20, 50 }) {
        const float step = MaxDiff(at(first - 1, y), at(first, y));
        QVERIFY2(step < 0.001f, qPrintable(QString("row %1: step of %2 where green clips").arg(y).arg(step)));
        for (int x = first; x < w; ++x) {
            const float off = OffTheWayToNeutral(at(x, y), Chroma(sky));
            QVERIFY2(onBranch(x) || off < 0.005f, qPrintable(QString("row %1 x %2 off the sky's way by %3").arg(y).arg(x).arg(off)));
            const float beside = MaxDiff(at(x, y), at(x, 20));
            QVERIFY2(onBranch(x) || beside < 0.005f, qPrintable(QString("row %1 x %2 beside a branch differs by %3").arg(y).arg(x).arg(beside)));
        }
        const V3 end = at(w - 1, y);
        for (float v : end) QVERIFY2(std::fabs(v - 1.0f / 3.0f) < 0.002f, qPrintable(QString("row %1: end not neutral").arg(y)));
    }
}

void TestHighlightRecon::unclippedPixelsAreLeftExactly()
{
    uint64_t seed = 0x2545f4914f6cdd1dULL;
    auto rnd = [&]() {
        seed ^= seed << 13;
        seed ^= seed >> 7;
        seed ^= seed << 17;
        return float(seed >> 40) / float(1ULL << 24);
    };
    const float wb[3] = { 2.1f, 1.0f, 1.6f }, white[3] = { 1.02f, 0.99f, 1.05f };
    for (auto [w, h] : { std::pair{1, 1}, std::pair{3, 1}, std::pair{7, 5}, std::pair{64, 48}, std::pair{129, 77} }) {
        std::vector<float> noise(static_cast<size_t>(w) * h);
        for (auto &v : noise) v = rnd();
        const Img img = FromFn(w, h, [&](int x, int y) -> V3 {
            const float t = (std::sin(x * 0.21f) + std::cos(y * 0.17f)) * 0.5f;
            const V3 base = { 0.5f + 0.4f * t, 0.45f - 0.2f * t, 0.3f + 0.1f * t };
            const float r = noise[static_cast<size_t>(y) * w + x];
            if (r < 0.1f) return { 1, 1, 1 };
            if (r < 0.4f) return { 1, base[1] * 1.6f, base[2] };
            if (r > 0.995f) return { 0, 0, 0.00001f };
            return base;
        });
        Img out = img;
        const int n = Recon(out, wb, 0.99f, white);
        int clipped = 0;
        for (int i = 0; i < w * h; ++i) {
            const V3 a = img.Get(i % w, i / w), b = out.Get(i % w, i / w);
            const bool cl = a[0] >= 0.99f || a[1] >= 0.99f || a[2] >= 0.99f;
            clipped += cl;
            if (!cl) QVERIFY2(a == b, qPrintable(QString("%1x%2 pixel %3 changed").arg(w).arg(h).arg(i)));
            else for (float v : b) QVERIFY(std::isfinite(v) && v >= 0.0f);
        }
        QCOMPARE(n, clipped);
    }
}

void TestHighlightRecon::reconstructRestoresAClippedChannel()
{
    /* a warm gradient whose red channel clips at its bright end */
    const Img truth = FromFn(256, 16, [](int x, int) -> V3 { const float v = 0.3f + x / 255.0f; return { v, v * 0.6f, v * 0.35f }; });
    Img img = FromFn(256, 16, [&](int x, int y) { V3 p = truth.Get(x, y); for (float &v : p) v = std::min(v, 1.0f); return p; });
    int first = 0;
    for (int x = 0; x < 256; ++x) if (img.Get(x, 8)[0] >= 0.999f) { first = x; break; }
    auto err = [&](const Img &im) { float e = 0; for (int x = first; x < first + 12; ++x) e += std::fabs(im.Get(x, 8)[0] - truth.Get(x, 8)[0]); return e; };
    const float before = err(img);
    QVERIFY(Recon(img, kOne, 0.999f) > 0);
    const float after = err(img);
    QVERIFY2(after < before * 0.2f, qPrintable(QString("before %1 after %2").arg(before).arg(after)));
    QCOMPARE(img.Get(0, 0), truth.Get(0, 0));
    QCOMPARE(img.Get(first - 1, 8), truth.Get(first - 1, 8));
}

void TestHighlightRecon::fullyClippedAndNoClipping()
{
    const float wb[3] = { 2.0f, 1.0f, 1.5f };
    Img img = FromFn(4, 4, [](int, int) -> V3 { return { 1, 1, 1 }; });
    QCOMPARE(Recon(img, wb), 16);
    const V3 p = img.Get(0, 0);
    QVERIFY(std::fabs(p[0] * 2.0f - p[1]) < 1e-5f && std::fabs(p[2] * 1.5f - p[1]) < 1e-5f);
    Img grey = FromFn(4, 4, [](int, int) -> V3 { return { 0.5f, 0.5f, 0.5f }; });
    QCOMPARE(Recon(grey, wb), 0);
    QCOMPARE(HighlightRecon::Reconstruct(nullptr, 0, 0, wb, 0.99f, kOne), 0);
}

void TestHighlightRecon::resultIsIndependentOfThreadCount()
{
    const V3 sky = { 1.10f, 1.25f, 1.45f };
    const Img base = FromFn(611, 437, [&](int x, int y) {
        const float t = 0.6f + 0.9f * std::fabs(std::sin(x * 0.013f) * std::cos(y * 0.021f));
        if ((x / 37 + y / 29) % 5 == 0) return Sensor({ 0.4f, 0.5f, 0.2f });
        return Sensor({ sky[0] * t, sky[1] * t, sky[2] * t });
    });
    Img a = base, b = base;
    const int saved = cv::getNumThreads();
    cv::setNumThreads(1);
    const int na = Recon(a);
    cv::setNumThreads(saved);
    const int nb = Recon(b);
    QCOMPARE(na, nb);
    QVERIFY(na > 1000);
    QVERIFY(std::memcmp(a.d.data(), b.d.data(), a.d.size() * sizeof(float)) == 0);
}

void TestHighlightRecon::throughput()
{
    if (qEnvironmentVariableIsEmpty("WINNOW_HIGHLIGHT_BENCH"))
        QSKIP("set WINNOW_HIGHLIGHT_BENCH=1 to measure 24 MP throughput");
    const Img img = FromFn(6000, 4000, [](int x, int y) -> V3 {
        const float v = 0.4f + 0.8f * std::fabs(std::sin(x * 0.003f) * std::cos(y * 0.002f));
        return { std::min(v, 1.0f), std::min(v * 0.7f, 1.0f), std::min(v * 0.5f, 1.0f) };
    });
    const float wb[3] = { 2.0f, 1.0f, 1.5f };
    double best = 1e9;
    for (int r = 0; r < 3; ++r) {
        Img im = img;
        QElapsedTimer t;
        t.start();
        Recon(im, wb);
        best = std::min(best, t.nsecsElapsed() / 1e6);
    }
    qInfo("reconstruct 24 MP: %.0f ms", best);
}

QTEST_GUILESS_MAIN(TestHighlightRecon)
#include "tst_highlightrecon.moc"
