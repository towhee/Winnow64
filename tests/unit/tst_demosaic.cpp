/*
    Demosaic -- Bilinear, PPG and AHD on synthetic Bayer mosaics.

    The demosaicer sees only a mosaic and its CFA pattern, so each test samples a known RGB
    scene through the pattern (MosaicFromRgb), demosaics it, and compares against the
    scene. Ported alongside the algorithms from LightCraft's own suite
    (crates/raw/src/demosaic.rs tests, MIT), adapted to Winnow's uint16 RawImage.

    What is pinned, and why each matters:

      o a constant colour comes back exactly, for every algorithm and all four Bayer
        phases -- a phase mix-up or a colour-difference sign error shows here first;
      o every photosite keeps its own measured channel (no self-blur);
      o on a photo-like scene with sharp edges, PPG and AHD beat Bilinear by > 6 dB PSNR
        (the reason they exist), and nobody falls below a floor on a smooth scene;
      o flat noise does not turn into a maze under AHD (the outright-pick failure that
        LightCraft fixed with the blend);
      o the result does not depend on the thread count (AHD runs in independent tiles,
        PPG in independent rows) and tiles join without seams;
      o tiny mosaics and an abort behave.

    Set WINNOW_DEMOSAIC_BENCH=1 to also print 24 MP throughput.
*/

#include <QtTest>
#include <QElapsedTimer>
#include <cmath>
#include <functional>
#include <opencv2/core.hpp>

#include "ImageFormats/Raw/demosaic.h"

namespace {

constexpr uint16_t kWhite = 16383;   // 14-bit, a typical sensor

struct Rgb {
    int w = 0, h = 0;
    std::vector<float> d;            // interleaved RGB
    float at(int x, int y, int c) const { return d[(static_cast<size_t>(y) * w + x) * 3 + c]; }
};

Rgb FromFn(int w, int h, const std::function<void(int, int, float *)> &f)
{
    Rgb r{w, h, std::vector<float>(static_cast<size_t>(w) * h * 3)};
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x) f(x, y, &r.d[(static_cast<size_t>(y) * w + x) * 3]);
    return r;
}

Rgb Filled(int w, int h, float r, float g, float b)
{
    return FromFn(w, h, [=](int, int, float *p) { p[0] = r; p[1] = g; p[2] = b; });
}

Rgb SmoothScene(int w, int h)
{
    return FromFn(w, h, [=](int x, int y, float *p) {
        const float fx = float(x) / w, fy = float(y) / h;
        p[0] = 0.2f + 0.6f * fx * fy;
        p[1] = 0.3f + 0.4f * std::fabs(std::sin(fx * 3.0f)) * 0.5f + 0.2f * fy;
        p[2] = 0.1f + 0.5f * (1.0f - fx) * (0.5f + 0.5f * std::cos(fy * 5.0f));
    });
}

/* Sharp luminance structure (disc, stripes, diagonal edge, fine texture) modulating a
   smoothly varying colour -- edges are mostly luminance edges, as in natural images. */
Rgb EdgyScene(int w, int h)
{
    return FromFn(w, h, [=](int x, int y, float *p) {
        const float fx = float(x), fy = float(y);
        const float d = std::sqrt((fx - w * 0.35f) * (fx - w * 0.35f) + (fy - h * 0.4f) * (fy - h * 0.4f));
        float lum;
        if (d < w * 0.18f) lum = 0.85f;
        else if (x > w * 2 / 3 && (y / 3) % 2 == 0) lum = 0.15f;
        else if (fx + fy * 0.5f > w * 1.1f) lum = 0.7f;
        else lum = 0.35f + 0.1f * (std::sin(fx * 0.7f) * std::cos(fy * 0.9f));
        p[0] = lum * (0.9f + 0.1f * (fx / w));
        p[1] = lum * 0.8f;
        p[2] = lum * (0.55f + 0.3f * (fy / h));
    });
}

RawImage MosaicFromRgb(const Rgb &img, CfaPattern pat)
{
    RawImage raw;
    raw.width = img.w;
    raw.height = img.h;
    raw.pattern = pat;
    raw.white = kWhite;
    raw.cfa.resize(static_cast<size_t>(img.w) * img.h);
    for (int y = 0; y < img.h; ++y)
        for (int x = 0; x < img.w; ++x) {
            const float v = img.at(x, y, Demosaic::BayerColorAt(pat, y, x));
            raw.cfa[static_cast<size_t>(y) * img.w + x] =
                static_cast<uint16_t>(std::lround(std::clamp(v, 0.0f, 1.0f) * kWhite));
        }
    return raw;
}

Rgb Run(const RawImage &raw, Demosaic::Algorithm a)
{
    Rgb out{raw.width, raw.height, {}};
    Demosaic dm;
    if (!dm.Run(raw, out.d, a)) out.d.clear();
    return out;
}

/* PSNR (dB, peak 1) over the interior, border pixels excluded. */
double Psnr(const Rgb &a, const Rgb &b, int border)
{
    double se = 0;
    size_t n = 0;
    for (int y = border; y < a.h - border; ++y)
        for (int x = border; x < a.w - border; ++x)
            for (int c = 0; c < 3; ++c) {
                const double e = a.at(x, y, c) - b.at(x, y, c);
                se += e * e;
                ++n;
            }
    if (n == 0 || se == 0) return INFINITY;
    return 10.0 * std::log10(1.0 / (se / n));
}

/* Mean anisotropy of fine luminance detail: how much more each pixel's 3x3 high-pass
   correlates with its horizontal neighbours than its vertical ones (or the reverse).
   Noise has none; a maze of short H/V strokes has a lot. */
double Anisotropy(const Rgb &img)
{
    const int w = img.w, h = img.h;
    std::vector<float> lum(static_cast<size_t>(w) * h), hp(lum.size(), 0.0f);
    for (int i = 0; i < w * h; ++i)
        lum[i] = 0.25f * img.d[i * 3] + 0.5f * img.d[i * 3 + 1] + 0.25f * img.d[i * 3 + 2];
    for (int y = 1; y < h - 1; ++y)
        for (int x = 1; x < w - 1; ++x) {
            float s = 0;
            for (int k = 0; k < 9; ++k) s += lum[(y + k / 3 - 1) * w + x + k % 3 - 1];
            hp[y * w + x] = lum[y * w + x] - s / 9.0f;
        }
    double sum = 0, n = 0;
    for (int y = 2; y < h - 2; ++y)
        for (int x = 2; x < w - 2; ++x) {
            const int i = y * w + x;
            sum += std::fabs(hp[i] * (hp[i - 1] + hp[i + 1] - hp[i - w] - hp[i + w]));
            n += 1;
        }
    return sum / n;
}

const CfaPattern kPhases[] = { CfaPattern::RGGB, CfaPattern::BGGR, CfaPattern::GRBG, CfaPattern::GBRG };
const Demosaic::Algorithm kAlgos[] = { Demosaic::Bilinear, Demosaic::Ppg, Demosaic::Ahd };

} // namespace

class TestDemosaic : public QObject
{
    Q_OBJECT

private slots:
    void constantColourIsExactForAllAlgorithmsAndPhases();
    void knownSamplesArePreserved();
    void psnrThresholdsAndRanking();
    void flatNoiseDoesNotTurnIntoAMaze();
    void resultIsIndependentOfThreadCount();
    void tinyMosaicsFallBackAndStayFinite();
    void abortStopsTheRun();
    void throughput();
};

void TestDemosaic::constantColourIsExactForAllAlgorithmsAndPhases()
{
    /* 17x13 and 300x140: odd sizes, and wide enough for several AHD tiles. */
    for (auto [w, h] : { std::pair{17, 13}, std::pair{300, 140} }) {
        const Rgb img = Filled(w, h, 0.3f, 0.55f, 0.8f);
        for (CfaPattern pat : kPhases) {
            const RawImage raw = MosaicFromRgb(img, pat);
            /* expected = the quantised value the mosaic actually holds */
            float want[3];
            for (int c = 0; c < 3; ++c)
                want[c] = std::lround(img.d[c] * kWhite) / float(kWhite);
            for (auto a : kAlgos) {
                const Rgb out = Run(raw, a);
                QCOMPARE(out.d.size(), img.d.size());
                for (size_t i = 0; i < out.d.size(); ++i)
                    QVERIFY2(std::fabs(out.d[i] - want[i % 3]) < 1e-5f,
                             qPrintable(QString("pattern %1 algo %2 pixel %3 got %4")
                                        .arg(int(pat)).arg(int(a)).arg(i / 3).arg(out.d[i])));
            }
        }
    }
}

void TestDemosaic::knownSamplesArePreserved()
{
    const Rgb img = EdgyScene(40, 32);
    const RawImage raw = MosaicFromRgb(img, CfaPattern::GRBG);
    for (auto a : kAlgos) {
        const Rgb out = Run(raw, a);
        for (int y = 0; y < 32; ++y)
            for (int x = 0; x < 40; ++x) {
                const int c = Demosaic::BayerColorAt(CfaPattern::GRBG, y, x);
                const float v = raw.cfa[y * 40 + x] / float(kWhite);
                QVERIFY2(std::fabs(out.at(x, y, c) - v) < 1e-6f, qPrintable(QString("algo %1").arg(int(a))));
            }
    }
}

void TestDemosaic::psnrThresholdsAndRanking()
{
    const Rgb smooth = SmoothScene(96, 80);
    const Rgb edgy = EdgyScene(96, 80);
    for (CfaPattern pat : kPhases) {
        auto p = [&](const Rgb &img, Demosaic::Algorithm a) {
            return Psnr(img, Run(MosaicFromRgb(img, pat), a), 4);
        };
        const double sb = p(smooth, Demosaic::Bilinear), sp = p(smooth, Demosaic::Ppg),
                     sa = p(smooth, Demosaic::Ahd);
        const double eb = p(edgy, Demosaic::Bilinear), ep = p(edgy, Demosaic::Ppg),
                     ea = p(edgy, Demosaic::Ahd);
        qInfo("pattern %d  smooth: bilinear %.1f ppg %.1f ahd %.1f   edgy: bilinear %.1f ppg %.1f ahd %.1f",
              int(pat), sb, sp, sa, eb, ep, ea);
        QVERIFY(sb > 38.0 && sp > 38.0 && sa > 38.0);
        QVERIFY(eb > 20.0);
        QVERIFY2(ep > eb + 6.0, "PPG should beat bilinear by > 6 dB on edges");
        QVERIFY2(ea > eb + 6.0, "AHD should beat bilinear by > 6 dB on edges");
    }
}

void TestDemosaic::flatNoiseDoesNotTurnIntoAMaze()
{
    /* A flat grey patch with sensor-like noise (sum of four uniforms, sigma ~0.02). */
    const int w = 192, h = 160;
    uint64_t seed = 0x2545f4914f6cdd1dULL;
    auto uni = [&]() {
        seed ^= seed << 13;
        seed ^= seed >> 7;
        seed ^= seed << 17;
        return float(seed >> 40) / float(1ULL << 24) - 0.5f;
    };
    RawImage raw;
    raw.width = w;
    raw.height = h;
    raw.pattern = CfaPattern::RGGB;
    raw.white = kWhite;
    raw.cfa.resize(static_cast<size_t>(w) * h);
    for (auto &v : raw.cfa) {
        const float s = 0.2f + (uni() + uni() + uni() + uni()) * 0.035f;
        v = static_cast<uint16_t>(std::lround(std::clamp(s, 0.0f, 1.0f) * kWhite));
    }
    const double ahd = Anisotropy(Run(raw, Demosaic::Ahd));
    const double bil = Anisotropy(Run(raw, Demosaic::Bilinear));
    qInfo("anisotropy: ahd %.3e bilinear %.3e (ratio %.1f)", ahd, bil, ahd / bil);
    /* LightCraft measured ~11x for the outright pick, ~7.5x with the blend. */
    QVERIFY2(ahd < 9.0 * bil, "AHD turned flat noise into a maze");
}

void TestDemosaic::resultIsIndependentOfThreadCount()
{
    /* 300x270: AHD tiles of 128 leave partial tiles on both axes, so seams, edges and
       corners are all exercised; the single-threaded run is the reference. */
    const RawImage raw = MosaicFromRgb(EdgyScene(300, 270), CfaPattern::GBRG);
    const int saved = cv::getNumThreads();
    for (auto a : { Demosaic::Ppg, Demosaic::Ahd }) {
        cv::setNumThreads(1);
        const Rgb one = Run(raw, a);
        cv::setNumThreads(saved);
        const Rgb many = Run(raw, a);
        QCOMPARE(many.d.size(), one.d.size());
        QVERIFY2(std::memcmp(many.d.data(), one.d.data(), one.d.size() * sizeof(float)) == 0,
                 qPrintable(QString("algo %1 differs across thread counts").arg(int(a))));
    }
    cv::setNumThreads(saved);

    /* No seam: a tile boundary (x = 128) is no worse than its neighbourhood. */
    const Rgb img = SmoothScene(300, 270);
    const Rgb out = Run(MosaicFromRgb(img, CfaPattern::RGGB), Demosaic::Ahd);
    double seam = 0, away = 0;
    for (int y = 4; y < 266; ++y)
        for (int c = 0; c < 3; ++c) {
            seam += std::fabs(out.at(127, y, c) - img.at(127, y, c)) + std::fabs(out.at(128, y, c) - img.at(128, y, c));
            away += std::fabs(out.at(60, y, c) - img.at(60, y, c)) + std::fabs(out.at(61, y, c) - img.at(61, y, c));
        }
    QVERIFY2(seam <= away * 2.0 + 1e-3, qPrintable(QString("seam %1 away %2").arg(seam).arg(away)));
}

void TestDemosaic::tinyMosaicsFallBackAndStayFinite()
{
    for (auto [w, h] : { std::pair{1, 1}, std::pair{2, 2}, std::pair{3, 5}, std::pair{5, 3},
                         std::pair{1, 7}, std::pair{4, 4} }) {
        const RawImage raw = MosaicFromRgb(Filled(w, h, 0.5f, 0.5f, 0.5f), CfaPattern::RGGB);
        for (auto a : kAlgos) {
            const Rgb out = Run(raw, a);
            QCOMPARE(out.d.size(), static_cast<size_t>(w) * h * 3);
            for (float v : out.d) QVERIFY(std::isfinite(v));
        }
    }
}

void TestDemosaic::abortStopsTheRun()
{
    const RawImage raw = MosaicFromRgb(SmoothScene(256, 256), CfaPattern::RGGB);
    QAtomicInt abort(1);
    for (auto a : kAlgos) {
        std::vector<float> rgb;
        Demosaic dm;
        QVERIFY(!dm.Run(raw, rgb, a, &abort));
    }
}

void TestDemosaic::throughput()
{
    if (qEnvironmentVariableIsEmpty("WINNOW_DEMOSAIC_BENCH"))
        QSKIP("set WINNOW_DEMOSAIC_BENCH=1 to measure 24 MP throughput");
    const RawImage raw = MosaicFromRgb(SmoothScene(6000, 4000), CfaPattern::RGGB);
    for (auto a : kAlgos) {
        std::vector<float> rgb;
        Demosaic dm;
        QElapsedTimer t;
        t.start();
        QVERIFY(dm.Run(raw, rgb, a));
        const double ms = t.nsecsElapsed() / 1e6;
        qInfo("algo %d: %.0f ms for 24 MP (%.1f MP/s)", int(a), ms, 24000.0 / ms);
    }
}

QTEST_GUILESS_MAIN(TestDemosaic)
#include "tst_demosaic.moc"
