/*
    PatchInpaint -- the Replace tool's Content-Aware fill (PatchMatch completion).

    Ported in part from LightCraft's suite (crates/inpaint/src/patchmatch.rs and lib.rs
    tests, MIT).

    What is pinned, and why each matters:

      o a blemish on a textured gradient is rebuilt close to the clean image -- the reason
        the tool exists;
      o a hole in a textured area beside a flat one fills TEXTURED (the texture features:
        colour alone matched the membrane-smoothed hole to the flat area, and the fill
        came out a smooth band -- measured on real photos, 2026-11-21);
      o stripes through the hole continue as stripes (structure, not a blur: what sets it
        apart from a membrane fill);
      o every pixel outside the hole is untouched, bit for bit;
      o the result is the same run to run and whatever the thread count (re-renders do not
        shimmer, preview and export agree);
      o a hole at the frame edge fills;
      o a hole covering everything, a tile smaller than a patch, an empty hole and a wrong
        type all return cleanly (false or unchanged), never crash;
      o the membrane fallback fills from the boundary and keeps a flat surround flat.

    WINNOW_PATCHINPAINT_BENCH=1 also times a 600 px tile with a 150 px hole.
*/

#include <QtTest>
#include <cmath>
#include <opencv2/core.hpp>
#include <opencv2/core/utility.hpp>
#include <opencv2/imgproc.hpp>

#include "Develop/patchinpaint.h"

namespace {

/* Smooth gradient plus fixed fine noise (LightCraft's texture()), scaled 0..1. */
cv::Mat Texture(int w, int h)
{
    cv::Mat m(h, w, CV_32FC3);
    cv::RNG rng(7);
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x) {
            const float n = float(rng.uniform(0.0, 1.0)) * 0.04f - 0.02f;
            m.at<cv::Vec3f>(y, x) = cv::Vec3f(0.3f + 0.004f * x + n, 0.5f + 0.002f * y + n, 0.4f + n);
        }
    return m;
}

cv::Mat Disc(int w, int h, float cx, float cy, float r)
{
    cv::Mat m(h, w, CV_8UC1, cv::Scalar(0));
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x)
            if (std::hypot(x - cx, y - cy) < r) m.at<uchar>(y, x) = 255;
    return m;
}

double HoleRmse(const cv::Mat &a, const cv::Mat &b, const cv::Mat &hole)
{
    double e = 0;
    int n = 0;
    for (int y = 0; y < a.rows; ++y)
        for (int x = 0; x < a.cols; ++x)
            if (hole.at<uchar>(y, x)) {
                const cv::Vec3f d = a.at<cv::Vec3f>(y, x) - b.at<cv::Vec3f>(y, x);
                e += d.dot(d);
                n += 3;
            }
    return std::sqrt(e / std::max(n, 1));
}

cv::Mat Blot(const cv::Mat &clean, const cv::Mat &hole, cv::Vec3f v)
{
    cv::Mat d = clean.clone();
    d.setTo(cv::Scalar(v[0], v[1], v[2]), hole);
    return d;
}

bool SameOutside(const cv::Mat &a, const cv::Mat &b, const cv::Mat &hole)
{
    for (int y = 0; y < a.rows; ++y)
        for (int x = 0; x < a.cols; ++x)
            if (!hole.at<uchar>(y, x) && a.at<cv::Vec3f>(y, x) != b.at<cv::Vec3f>(y, x))
                return false;
    return true;
}

} // namespace

class TestPatchInpaint : public QObject
{
    Q_OBJECT
private slots:
    void fillsABlemishFromTexture();
    void continuesStructure();
    void texturedSurroundFillsTextured();
    void deterministicWhateverTheThreadCount();
    void holeAtTheFrameEdgeFills();
    void hostileInputsReturnCleanly();
    void membraneFillsFromTheBoundary();
    void benchmark();
};

void TestPatchInpaint::fillsABlemishFromTexture()
{
    const cv::Mat clean = Texture(96, 96);
    const cv::Mat hole = Disc(96, 96, 48, 48, 9);
    const cv::Mat dirty = Blot(clean, hole, {0.02f, 0.02f, 0.02f});
    cv::Mat out = dirty.clone();
    QVERIFY(PatchInpaint::Complete(out, hole));
    const double before = HoleRmse(dirty, clean, hole), after = HoleRmse(out, clean, hole);
    QVERIFY2(after < 0.06 && after < before * 0.15,
             qPrintable(QString("rmse before %1 after %2").arg(before).arg(after)));
    QVERIFY(SameOutside(out, dirty, hole));
}

void TestPatchInpaint::continuesStructure()
{
    /* Vertical stripes 4 px wide: the fill must keep them (a blur would sit at 0.5). */
    cv::Mat clean(80, 80, CV_32FC3);
    for (int y = 0; y < 80; ++y)
        for (int x = 0; x < 80; ++x) {
            const float v = (x / 4) % 2 == 0 ? 0.2f : 0.8f;
            clean.at<cv::Vec3f>(y, x) = cv::Vec3f(v, v, v);
        }
    const cv::Mat hole = Disc(80, 80, 40, 40, 7);
    cv::Mat out = Blot(clean, hole, {0.5f, 0.5f, 0.5f});
    QVERIFY(PatchInpaint::Complete(out, hole));
    const double err = HoleRmse(out, clean, hole);
    QVERIFY2(err < 0.15, qPrintable(QString::number(err)));
}

void TestPatchInpaint::texturedSurroundFillsTextured()
{
    /* Left: fine grain (blurred noise, sd ~0.05) around 0.5; right: flat 0.5. A 24 px
       stroke runs down the textured side, close to the flat side. */
    const int W = 240, H = 200;
    cv::Mat img(H, W, CV_32FC3);
    cv::randn(img, cv::Scalar(0.5, 0.5, 0.5), cv::Scalar(0.09, 0.09, 0.09));
    cv::GaussianBlur(img, img, cv::Size(0, 0), 0.8);
    img(cv::Rect(W / 2, 0, W / 2, H)).setTo(cv::Scalar(0.5, 0.5, 0.5));
    cv::Mat hole(H, W, CV_8UC1, cv::Scalar(0));
    cv::line(hole, {W / 2 - 20, 20}, {W / 2 - 20, H - 20}, 255, 24);
    auto holeSd = [&](const cv::Mat &m) {
        cv::Scalar mu, sd;
        cv::meanStdDev(m, mu, sd, hole);
        return (sd[0] + sd[1] + sd[2]) / 3.0;
    };
    const double truth = holeSd(img);
    PatchInpaint::Params colourOnly;
    colourOnly.textureWeight = 0.0f;
    cv::Mat a = Blot(img, hole, {0.5f, 0.5f, 0.5f}), b = a.clone();
    QVERIFY(PatchInpaint::Complete(a, hole));
    QVERIFY(PatchInpaint::Complete(b, hole, colourOnly));
    const double withTex = holeSd(a), without = holeSd(b);
    qInfo("hole sd: surround %.4f, texture features %.4f, colour only %.4f", truth, withTex, without);
    QVERIFY2(withTex > 0.6 * truth,
             qPrintable(QString("fill sd %1 vs surround %2").arg(withTex).arg(truth)));
}

void TestPatchInpaint::deterministicWhateverTheThreadCount()
{
    /* Big enough (>= 128x128) that the solver runs its parallel bands. */
    const cv::Mat clean = Texture(220, 200);
    const cv::Mat hole = Disc(220, 200, 110, 100, 30);
    const cv::Mat dirty = Blot(clean, hole, {0.9f, 0.1f, 0.1f});
    const int threads = cv::getNumThreads();
    cv::Mat a = dirty.clone(), b = dirty.clone(), c = dirty.clone();
    QVERIFY(PatchInpaint::Complete(a, hole));
    QVERIFY(PatchInpaint::Complete(b, hole));
    cv::setNumThreads(1);
    QVERIFY(PatchInpaint::Complete(c, hole));
    cv::setNumThreads(threads);
    QCOMPARE(cv::norm(a, b, cv::NORM_INF), 0.0);
    QCOMPARE(cv::norm(a, c, cv::NORM_INF), 0.0);
    QVERIFY(SameOutside(a, dirty, hole));
}

void TestPatchInpaint::holeAtTheFrameEdgeFills()
{
    const cv::Mat clean = Texture(96, 96);
    const cv::Mat hole = Disc(96, 96, 0, 48, 10);
    const cv::Mat dirty = Blot(clean, hole, {0.02f, 0.02f, 0.02f});
    cv::Mat out = dirty.clone();
    QVERIFY(PatchInpaint::Complete(out, hole));
    const double before = HoleRmse(dirty, clean, hole), after = HoleRmse(out, clean, hole);
    QVERIFY2(after < before * 0.2, qPrintable(QString("before %1 after %2").arg(before).arg(after)));
}

void TestPatchInpaint::hostileInputsReturnCleanly()
{
    /* A hole covering everything: nothing to copy from. */
    cv::Mat all(16, 16, CV_32FC3, cv::Scalar(0.5, 0.5, 0.5));
    const cv::Mat allSnap = all.clone();
    QVERIFY(!PatchInpaint::Complete(all, cv::Mat(16, 16, CV_8UC1, cv::Scalar(255))));
    QCOMPARE(cv::norm(all, allSnap, cv::NORM_INF), 0.0);
    /* A tile smaller than a patch. */
    cv::Mat tiny(2, 2, CV_32FC3, cv::Scalar(0.5, 0.5, 0.5));
    cv::Mat tinyHole(2, 2, CV_8UC1, cv::Scalar(0));
    tinyHole.at<uchar>(0, 0) = 255;
    PatchInpaint::Complete(tiny, tinyHole);          // either answer, no crash
    /* No hole: true, unchanged. */
    cv::Mat img = Texture(32, 32);
    const cv::Mat snap = img.clone();
    QVERIFY(PatchInpaint::Complete(img, cv::Mat(32, 32, CV_8UC1, cv::Scalar(0))));
    QCOMPARE(cv::norm(img, snap, cv::NORM_INF), 0.0);
    /* Wrong type / size. */
    cv::Mat u8(32, 32, CV_8UC3, cv::Scalar(1, 2, 3));
    QVERIFY(!PatchInpaint::Complete(u8, cv::Mat(32, 32, CV_8UC1, cv::Scalar(255))));
    QVERIFY(!PatchInpaint::Complete(img, cv::Mat(31, 32, CV_8UC1, cv::Scalar(255))));
    cv::Mat empty;
    QVERIFY(!PatchInpaint::Complete(empty, cv::Mat()));
}

void TestPatchInpaint::membraneFillsFromTheBoundary()
{
    /* Flat surround: the fill is that value everywhere. */
    cv::Mat flat(64, 64, CV_32FC3, cv::Scalar(0.3, 0.6, 0.9));
    const cv::Mat hole = Disc(64, 64, 32, 32, 15);
    flat.setTo(cv::Scalar(0, 0, 0), hole);
    PatchInpaint::Membrane(flat, hole);
    cv::Mat ref(64, 64, CV_32FC3, cv::Scalar(0.3, 0.6, 0.9));
    QVERIFY(cv::norm(flat, ref, cv::NORM_INF) < 2e-3);
    /* A horizontal ramp: harmonic, so the ramp continues through the hole. */
    cv::Mat ramp(64, 64, CV_32FC3);
    for (int y = 0; y < 64; ++y)
        for (int x = 0; x < 64; ++x) ramp.at<cv::Vec3f>(y, x) = cv::Vec3f(x / 63.f, x / 63.f, x / 63.f);
    cv::Mat r = ramp.clone();
    r.setTo(cv::Scalar(0, 0, 0), hole);
    PatchInpaint::Membrane(r, hole);
    QVERIFY(HoleRmse(r, ramp, hole) < 0.01);
}

void TestPatchInpaint::benchmark()
{
    if (qEnvironmentVariableIntValue("WINNOW_PATCHINPAINT_BENCH") == 0)
        QSKIP("set WINNOW_PATCHINPAINT_BENCH=1 to time a 600 px tile with a 150 px hole");
    cv::Mat clean(600, 600, CV_32FC3);
    cv::randn(clean, cv::Scalar(0.5, 0.5, 0.5), cv::Scalar(0.05, 0.05, 0.05));
    cv::GaussianBlur(clean, clean, cv::Size(0, 0), 1.2);
    const cv::Mat hole = Disc(600, 600, 300, 300, 75);
    QElapsedTimer t;
    for (int k = 0; k < 3; ++k) {
        cv::Mat d = Blot(clean, hole, {0, 0, 0});
        t.start();
        QVERIFY(PatchInpaint::Complete(d, hole));
        qInfo("600x600, 150 px hole: %lld ms", t.elapsed());
    }
}

QTEST_APPLESS_MAIN(TestPatchInpaint)
#include "tst_patchinpaint.moc"
