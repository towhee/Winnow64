/*
    HotPixel::Suppress -- hot photosites found and replaced in the mosaic, real light kept.

    Ported with the algorithm from LightCraft's suite (crates/raw/src/hotpixel.rs tests,
    MIT). Every case starts from a flat 14-bit RGGB mosaic (black 512, white 16383, grey
    2000 counts above black, deterministic noise of sigma ~30) and plants defects in it.

    What is pinned, and why each matters:

      o flat noise is left alone, bit for bit -- the thresholds must sit above what noise
        reaches, or every image would lose real texture;
      o a single hot photosite of each color is replaced by ~its surroundings, and so is
        an adjacent hot pair (hot cells come in pairs; one lit neighbour isn't "real");
      o a hot photosite in a dark area is found (read noise there is smaller: the noise
        model must follow the level);
      o a small star (lit 3x3) is kept, and a dense star field keeps its stars but still
        loses its hot pixels (real light doesn't count against the share limit);
      o the share limit is the caller's, a NaN limit replaces nothing, and an image with
        too many candidates changes nothing;
      o X-Trans is handled; an Unknown pattern and a tiny image are left alone;
      o the result does not depend on the thread count.

    Set WINNOW_HOTPIXEL_BENCH=1 to also print 45 MP throughput.
*/

#include <QtTest>
#include <QElapsedTimer>
#include <cmath>
#include <opencv2/core.hpp>

#include "ImageFormats/Raw/hotpixel.h"

namespace {

RawImage Mosaic(int w, int h, uint64_t seed = 0x2545f4914f6cdd1dULL)
{
    auto noise = [&seed]() {
        /* sum of 4 uniforms, about N(0, 1) */
        float s = 0;
        for (int k = 0; k < 4; ++k) {
            seed ^= seed << 13;
            seed ^= seed >> 7;
            seed ^= seed << 17;
            s += static_cast<float>(seed >> 11) / static_cast<float>(1ULL << 53) - 0.5f;
        }
        return s * 1.732f;
    };
    RawImage r;
    r.width = w;
    r.height = h;
    r.pattern = CfaPattern::RGGB;
    r.white = 16383;
    for (auto &b : r.black) b = 512;
    r.cfa.resize(static_cast<size_t>(w) * h);
    for (auto &v : r.cfa) v = static_cast<uint16_t>(std::lround(512.0f + 2000.0f + 30.0f * noise()));
    return r;
}

uint16_t &At(RawImage &r, int x, int y) { return r.cfa[static_cast<size_t>(y) * r.width + x]; }

void PlantStar(RawImage &r, int cx, int cy)
{
    /* a point of light over a 3x3 block: brightest in the middle, its neighbours lit too */
    for (int dy = -1; dy <= 1; ++dy)
        for (int dx = -1; dx <= 1; ++dx) At(r, cx + dx, cy + dy) = (dx == 0 && dy == 0) ? 12000 : 5000;
}

} // namespace

class TestHotPixel : public QObject
{
    Q_OBJECT

private slots:
    void flatNoiseIsLeftAlone();
    void singleHotPhotositesOfEachColorAreReplaced();
    void anAdjacentHotPairIsReplaced();
    void aSmallStarIsKept();
    void aHotPhotositeInADarkAreaIsReplaced();
    void aDenseStarFieldKeepsItsStarsAndLosesItsHotPixels();
    void theLimitIsTheCallers();
    void tooManyCandidatesChangeNothing();
    void xtransIsHandled();
    void unknownPatternAndTinyImagesAreLeftAlone();
    void resultIsIndependentOfThreadCount();
    void throughput();
};

void TestHotPixel::flatNoiseIsLeftAlone()
{
    RawImage r = Mosaic(256, 256);
    const auto before = r.cfa;
    QCOMPARE(HotPixel::Suppress(r), 0);
    QVERIFY(r.cfa == before);
}

void TestHotPixel::singleHotPhotositesOfEachColorAreReplaced()
{
    RawImage r = Mosaic(256, 256);
    /* red (even, even), green (odd, even), blue (odd, odd) */
    const int sites[3][2] = { {100, 100}, {101, 60}, {151, 151} };
    for (auto &s : sites) At(r, s[0], s[1]) = 9000;
    QCOMPARE(HotPixel::Suppress(r), 3);
    for (auto &s : sites) {
        const float v = At(r, s[0], s[1]);
        QVERIFY2(std::fabs(v - 2512.0f) < 120.0f, qPrintable(QString("(%1,%2) = %3").arg(s[0]).arg(s[1]).arg(v)));
    }
}

void TestHotPixel::anAdjacentHotPairIsReplaced()
{
    RawImage r = Mosaic(256, 256);
    At(r, 80, 80) = 9000;
    At(r, 80, 81) = 8000;
    QCOMPARE(HotPixel::Suppress(r), 2);
    QVERIFY(At(r, 80, 80) < 2700 && At(r, 80, 81) < 2700);
}

void TestHotPixel::aSmallStarIsKept()
{
    RawImage r = Mosaic(256, 256);
    PlantStar(r, 120, 90);
    const auto before = r.cfa;
    QCOMPARE(HotPixel::Suppress(r), 0);
    QVERIFY(r.cfa == before);
}

void TestHotPixel::aHotPhotositeInADarkAreaIsReplaced()
{
    RawImage r = Mosaic(256, 256);
    /* a dark patch near black (noise there is read noise, smaller) */
    for (int y = 32; y < 96; ++y)
        for (int x = 32; x < 96; ++x) At(r, x, y) = static_cast<uint16_t>(512 + 20 + (x * 7 + y * 13) % 11);
    At(r, 64, 64) = 1500;
    QCOMPARE(HotPixel::Suppress(r), 1);
    QVERIFY(At(r, 64, 64) < 600);
}

void TestHotPixel::aDenseStarFieldKeepsItsStarsAndLosesItsHotPixels()
{
    RawImage r = Mosaic(256, 256);
    /* 225 small stars: more candidates than 0.2 % of the photosites, all real light */
    for (int sy = 0; sy < 15; ++sy)
        for (int sx = 0; sx < 15; ++sx) PlantStar(r, 16 + 16 * sx, 16 + 16 * sy);
    const int hot[3][2] = { {24, 24}, {105, 72}, {201, 153} };
    for (auto &s : hot) At(r, s[0], s[1]) = 9000;
    QCOMPARE(HotPixel::Suppress(r), 3);
    for (auto &s : hot) QVERIFY(At(r, s[0], s[1]) < 2700);
    QCOMPARE(At(r, 16, 16), uint16_t(12000));
}

void TestHotPixel::theLimitIsTheCallers()
{
    RawImage r = Mosaic(256, 256);
    const int hot[4][2] = { {40, 40}, {101, 60}, {151, 151}, {200, 30} };
    for (auto &s : hot) At(r, s[0], s[1]) = 9000;
    const auto before = r.cfa;
    /* 4 of 248 x 248 photosites: above a limit of 0.005 %, within 0.01 % */
    QCOMPARE(HotPixel::Suppress(r, 5e-5), 0);
    QVERIFY(r.cfa == before);
    RawImage nanCopy = r;
    QCOMPARE(HotPixel::Suppress(nanCopy, std::nan("")), 0);
    QCOMPARE(HotPixel::Suppress(r, 1e-4), 4);
}

void TestHotPixel::tooManyCandidatesChangeNothing()
{
    RawImage r = Mosaic(128, 128);
    for (int y = 8; y < 120; y += 4)
        for (int x = 8; x < 120; x += 4) At(r, x, y) = 12000;
    const auto before = r.cfa;
    QCOMPARE(HotPixel::Suppress(r), 0);
    QVERIFY(r.cfa == before);
}

void TestHotPixel::xtransIsHandled()
{
    /* Fuji X-Trans 6x6 color map (0=R 1=G 2=B) */
    static const uint8_t kXT[6][6] = {
        {1, 1, 0, 1, 1, 2}, {1, 1, 2, 1, 1, 0}, {2, 0, 1, 0, 2, 1},
        {1, 1, 2, 1, 1, 0}, {1, 1, 0, 1, 1, 2}, {0, 2, 1, 2, 0, 1} };
    RawImage r = Mosaic(240, 240);
    r.pattern = CfaPattern::XTrans;
    std::memcpy(r.xtrans, kXT, sizeof(kXT));
    const auto before = r.cfa;
    QCOMPARE(HotPixel::Suppress(r), 0);
    QVERIFY(r.cfa == before);
    /* red at (2,0) of the period, blue at (5,0), green at (0,0) */
    const int sites[3][2] = { {62, 60}, {125, 120}, {180, 180} };
    for (auto &s : sites) At(r, s[0], s[1]) = 9000;
    QCOMPARE(HotPixel::Suppress(r), 3);
    for (auto &s : sites) QVERIFY(At(r, s[0], s[1]) < 2700);
}

void TestHotPixel::unknownPatternAndTinyImagesAreLeftAlone()
{
    RawImage r = Mosaic(64, 64);
    At(r, 30, 30) = 9000;
    r.pattern = CfaPattern::Unknown;
    QCOMPARE(HotPixel::Suppress(r), 0);
    QCOMPARE(At(r, 30, 30), uint16_t(9000));

    RawImage tiny = Mosaic(12, 12);
    At(tiny, 6, 6) = 9000;
    QCOMPARE(HotPixel::Suppress(tiny), 0);

    RawImage invalid;
    QCOMPARE(HotPixel::Suppress(invalid), 0);
}

void TestHotPixel::resultIsIndependentOfThreadCount()
{
    RawImage a = Mosaic(512, 384, 0x9e3779b97f4a7c15ULL);
    for (int k = 0; k < 40; ++k) At(a, 10 + (k * 37) % 490, 10 + (k * 53) % 360) = 9000;
    RawImage b = a;
    const int saved = cv::getNumThreads();
    cv::setNumThreads(1);
    const int na = HotPixel::Suppress(a);
    cv::setNumThreads(saved);
    const int nb = HotPixel::Suppress(b);
    QCOMPARE(na, nb);
    QVERIFY(na >= 38);
    QVERIFY(a.cfa == b.cfa);
}

void TestHotPixel::throughput()
{
    if (qEnvironmentVariableIsEmpty("WINNOW_HOTPIXEL_BENCH"))
        QSKIP("set WINNOW_HOTPIXEL_BENCH=1 to measure 45 MP throughput");
    RawImage r = Mosaic(8256, 5504);
    for (int k = 0; k < 500; ++k) At(r, 10 + (k * 7919) % 8200, 10 + (k * 104729) % 5480) = 9000;
    QElapsedTimer t;
    t.start();
    const int n = HotPixel::Suppress(r);
    qInfo("45 MP: %d replaced in %.0f ms", n, t.nsecsElapsed() / 1e6);
}

QTEST_GUILESS_MAIN(TestHotPixel)
#include "tst_hotpixel.moc"
