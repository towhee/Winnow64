/*
    FSLocalAlign -- the focus stacker's tile flow after the global ECC fit.

    Ported in part from LightCraft's suite (crates/merge/src/flow.rs tests, MIT). Frames are
    8-bit gray built from a smooth band-limited texture, so a sub-pixel shift can be
    rendered exactly.

    What is pinned, and why each matters:

      o a parallax-like shift (the top 2.2 px off, the bottom not, a smooth band between)
        is found and undone -- the reason the option exists;
      o flat and empty frames give no shift (a textureless sky must not be pushed around);
      o clipped patches are not matched (their texture is the clip, not the scene);
      o fields accumulate along the stack, and an empty field counts as zero;
      o the composed warp is the affine's when the field is empty -- bit for bit, so the
        option OFF is exactly the old stacker -- and the affine plus the field otherwise;
      o the stack chain KEEPS a real shift that grows slice by slice (handheld parallax)
        and its summed field undoes it, while it DROPS steps that are only noise --
        independent noise and focus changes on a still scene leave every field empty,
        so the stack is exactly the affine-only one (the guard against the random walk
        measured on real OM-1 stacks);
      o every slice is decided exactly once, in order, for stacks of 1, 2 and more;
      o the result does not depend on the thread count.
*/

#include <QtTest>
#include <algorithm>
#include <cmath>
#include <functional>
#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>

#include "FocusStack/fslocalalign.h"

namespace {

/* A smooth texture, varied enough in 2-D for every patch to match uniquely. */
double Tex(double x, double y)
{
    const double v = 0.5 + 0.18 * std::sin(0.31 * x + 0.7 * std::sin(0.05 * y))
                         + 0.14 * std::cos(0.23 * y - 0.4 * std::cos(0.07 * x))
                         + 0.08 * std::sin(0.11 * (x + y)) * std::cos(0.17 * (x - 0.6 * y));
    return std::clamp(v, 0.0, 0.94);
}

cv::Mat Render(int w, int h, const std::function<cv::Point2d(double, double)> &shift)
{
    cv::Mat m(h, w, CV_8UC1);
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x) {
            const cv::Point2d s = shift(x + 0.5, y + 0.5);
            m.at<uchar>(y, x) = cv::saturate_cast<uchar>(255.0 * Tex(x + 0.5 + s.x, y + 0.5 + s.y));
        }
    return m;
}

/* The parallax of LightCraft's test: top shifted (2.2, -1.1), bottom not, smooth between. */
cv::Point2d Parallax(double, double y)
{
    double t = std::clamp((y - 300.0) / 200.0, 0.0, 1.0);
    const double s = t * t * (3.0 - 2.0 * t);
    return { (1.0 - s) * 2.2, (1.0 - s) * -1.1 };
}

double MeanError(const cv::Mat &a, const cv::Mat &b, int margin)
{
    double e = 0;
    int n = 0;
    for (int y = margin; y < a.rows - margin; y += 3)
        for (int x = margin; x < a.cols - margin; x += 3) {
            e += std::abs(int(a.at<uchar>(y, x)) - int(b.at<uchar>(y, x)));
            ++n;
        }
    return e / n;
}

cv::Mat Identity()
{
    return (cv::Mat_<float>(2, 3) << 1, 0, 0, 0, 1, 0);
}

} // namespace

class TestFsLocalAlign : public QObject
{
    Q_OBJECT

private slots:
    void findsAndUndoesALocalShift();
    void flatAndEmptyFramesGiveNoShift();
    void clippedPatchesAreNotMatched();
    void fieldsAccumulate();
    void emptyFieldWarpIsTheAffineExactly();
    void warpComposesTheAffineAndTheField();
    void chainKeepsARealGrowingShift();
    void chainDropsNoise();
    void chainDecidesEverySliceOnceInOrder();
    void resultIsIndependentOfThreadCount();
};

void TestFsLocalAlign::findsAndUndoesALocalShift()
{
    const int w = 1200, h = 800;
    const cv::Mat reference = Render(w, h, [](double, double) { return cv::Point2d(0, 0); });
    /* the frame's pixel q shows the scene at q + shift(q.y) */
    const cv::Mat frame = Render(w, h, Parallax);
    const FSLocalAlign::Flow flow = FSLocalAlign::Estimate(reference, frame);
    const int tiles = flow.dx.cols * flow.dx.rows;
    QVERIFY2(flow.measured > tiles / 2, qPrintable(QString("%1 of %2").arg(flow.measured).arg(tiles)));
    /* reference p matches frame p + d with d = -shift */
    const cv::Point2f top = flow.at(600, 100), bottom = flow.at(600, 700);
    QVERIFY2(std::abs(top.x + 2.2f) < 0.15f && std::abs(top.y - 1.1f) < 0.15f,
             qPrintable(QString("top %1 %2").arg(top.x).arg(top.y)));
    QVERIFY2(std::abs(bottom.x) < 0.15f && std::abs(bottom.y) < 0.15f,
             qPrintable(QString("bottom %1 %2").arg(bottom.x).arg(bottom.y)));
    cv::Mat out;
    FSLocalAlign::Warp(frame, Identity(), flow, out);
    const double before = MeanError(frame, reference, 40), after = MeanError(out, reference, 40);
    QVERIFY2(after < before / 4.0, qPrintable(QString("mean error %1 unwarped, %2 warped").arg(before).arg(after)));
}

void TestFsLocalAlign::flatAndEmptyFramesGiveNoShift()
{
    const cv::Mat flat(300, 400, CV_8UC1, cv::Scalar(128));
    const FSLocalAlign::Flow f = FSLocalAlign::Estimate(flat, flat);
    QCOMPARE(f.measured, 0);
    QCOMPARE(f.maxShift(), 0.0f);
    QVERIFY(FSLocalAlign::Estimate(cv::Mat(), cv::Mat()).empty());
    QVERIFY(FSLocalAlign::Estimate(flat, cv::Mat()).empty());
    QVERIFY(FSLocalAlign::Estimate(flat, cv::Mat(200, 400, CV_8UC1)).empty());   // size mismatch
    QCOMPARE(FSLocalAlign::Flow().at(5, NAN), cv::Point2f(0, 0));
    QCOMPARE(f.at(INFINITY, 3), cv::Point2f(0, 0));
}

void TestFsLocalAlign::clippedPatchesAreNotMatched()
{
    const int w = 960, h = 640;
    cv::Mat reference = Render(w, h, [](double, double) { return cv::Point2d(0, 0); });
    cv::Mat frame = Render(w, h, [](double, double) { return cv::Point2d(1.5, 0); });
    reference.setTo(255);   // all clipped
    const FSLocalAlign::Flow f = FSLocalAlign::Estimate(reference, frame);
    QCOMPARE(f.measured, 0);
    QCOMPARE(f.maxShift(), 0.0f);
}

void TestFsLocalAlign::fieldsAccumulate()
{
    FSLocalAlign::Flow a;
    a.step = 50;
    a.dx = cv::Mat(4, 6, CV_32F, cv::Scalar(0.5f));
    a.dy = cv::Mat(4, 6, CV_32F, cv::Scalar(-0.25f));
    FSLocalAlign::Flow b = a;
    b.dx = cv::Mat(4, 6, CV_32F, cv::Scalar(0.25f));
    b.dy = cv::Mat(4, 6, CV_32F, cv::Scalar(1.0f));
    const FSLocalAlign::Flow s = FSLocalAlign::Add(a, b);
    QCOMPARE(s.at(120, 80), cv::Point2f(0.75f, 0.75f));
    QCOMPARE(FSLocalAlign::Add(FSLocalAlign::Flow(), b).at(10, 10), cv::Point2f(0.25f, 1.0f));
    QCOMPARE(FSLocalAlign::Add(a, FSLocalAlign::Flow()).at(10, 10), cv::Point2f(0.5f, -0.25f));
}

void TestFsLocalAlign::emptyFieldWarpIsTheAffineExactly()
{
    const cv::Mat src = Render(500, 300, [](double, double) { return cv::Point2d(0, 0); });
    const cv::Mat t = (cv::Mat_<float>(2, 3) << 1.002f, 0.003f, 1.7f, -0.002f, 0.999f, -0.8f);
    cv::Mat a, b;
    cv::warpAffine(src, a, t, src.size(), cv::INTER_CUBIC, cv::BORDER_REFLECT);   // FSAlign::applyTransform
    FSLocalAlign::Warp(src, t, FSLocalAlign::Flow(), b);
    QVERIFY(cv::norm(a, b, cv::NORM_INF) == 0);
}

void TestFsLocalAlign::warpComposesTheAffineAndTheField()
{
    /* A translation affine and a uniform field: out(p) = src(inverse(T)(p + d)). With
       T = translate(+3, -2) and d = (0.5, 1.0) that is src(p + (-2.5, 3.0)). */
    const int w = 400, h = 300;
    const cv::Mat src = Render(w, h, [](double, double) { return cv::Point2d(0, 0); });
    const cv::Mat t = (cv::Mat_<float>(2, 3) << 1, 0, 3, 0, 1, -2);
    FSLocalAlign::Flow f;
    f.step = 50;
    f.dx = cv::Mat(h / 50, w / 50, CV_32F, cv::Scalar(0.5f));
    f.dy = cv::Mat(h / 50, w / 50, CV_32F, cv::Scalar(1.0f));
    cv::Mat out;
    FSLocalAlign::Warp(src, t, f, out);
    const cv::Mat expect = Render(w, h, [](double, double) { return cv::Point2d(-2.5, 3.0); });
    const double e = MeanError(out, expect, 12);
    QVERIFY2(e < 1.0, qPrintable(QString("mean error %1 levels").arg(e)));
}

namespace {
/* Slice k: the scene shifted k * (half the parallax), blurred a little differently per
   slice (focus), with optional independent noise. */
cv::Mat Slice(int k, double shiftScale, double noise, uint64_t seed)
{
    cv::Mat m = Render(1200, 800, [&](double x, double y) { return Parallax(x, y) * (0.5 * k * shiftScale); });
    const double sigma = 0.6 + 0.3 * (k % 3);
    cv::GaussianBlur(m, m, cv::Size(0, 0), sigma);
    if (noise > 0) {
        cv::Mat n(m.size(), CV_32F);
        cv::RNG rng(seed + k);
        rng.fill(n, cv::RNG::NORMAL, 0.0, noise);
        cv::Mat f;
        m.convertTo(f, CV_32F);
        f += n;
        f.convertTo(m, CV_8U);
    }
    return m;
}
} // namespace

void TestFsLocalAlign::chainKeepsARealGrowingShift()
{
    const int N = 5;
    std::vector<cv::Mat> slices;
    for (int k = 0; k < N; ++k) slices.push_back(Slice(k, 1.0, 0.0, 0));
    FSLocalAlign::Chain chain;
    std::vector<FSLocalAlign::Chain::Decision> all;
    for (const auto &m : slices)
        for (const auto &d : chain.Add(m)) all.push_back(d);
    for (const auto &d : chain.Finish()) all.push_back(d);
    QCOMPARE(int(all.size()), N);
    for (int k = 1; k < N; ++k) {
        const auto &d = all[k];
        QCOMPARE(d.slice, k);
        QVERIFY2(d.kept, qPrintable(QString("slice %1 dropped: summed %2, off by %3").arg(k).arg(d.signal).arg(d.disagreement)));
        /* reference (slice 0) p matches slice k at p - k * shift/2. Within 10 % + 0.15 px:
           the parabola's sub-pixel peak leans toward whole pixels, a few percent a step,
           and the steps add (slice 4 here: -4.05 for -4.4). */
        const cv::Point2f top = d.field.at(600, 100), bottom = d.field.at(600, 700);
        const float tolX = 0.1f * 1.1f * k + 0.15f, tolY = 0.1f * 0.55f * k + 0.15f;
        QVERIFY2(std::abs(top.x + 1.1f * k) < tolX && std::abs(top.y - 0.55f * k) < tolY,
                 qPrintable(QString("slice %1 top %2 %3").arg(k).arg(top.x).arg(top.y)));
        QVERIFY2(std::abs(bottom.x) < 0.25f && std::abs(bottom.y) < 0.25f,
                 qPrintable(QString("slice %1 bottom %2 %3").arg(k).arg(bottom.x).arg(bottom.y)));
    }
    /* and the last slice warped by its field lines up with slice 0 */
    cv::Mat out;
    FSLocalAlign::Warp(slices[N - 1], Identity(), all[N - 1].field, out);
    const double before = MeanError(slices[N - 1], slices[0], 60), after = MeanError(out, slices[0], 60);
    QVERIFY2(after < before / 3.0, qPrintable(QString("mean error %1 unwarped, %2 warped").arg(before).arg(after)));
}

void TestFsLocalAlign::chainDropsNoise()
{
    FSLocalAlign::Chain chain;
    std::vector<FSLocalAlign::Chain::Decision> all;
    for (int k = 0; k < 6; ++k)
        for (const auto &d : chain.Add(Slice(k, 0.0, 4.0, 1234))) all.push_back(d);
    for (const auto &d : chain.Finish()) all.push_back(d);
    QCOMPARE(int(all.size()), 6);
    for (const auto &d : all)
        QVERIFY2(!d.kept && d.field.empty(),
                 qPrintable(QString("slice %1 kept: summed %2, off by %3, field max %4")
                            .arg(d.slice).arg(d.signal).arg(d.disagreement).arg(d.field.maxShift())));
}

void TestFsLocalAlign::chainDecidesEverySliceOnceInOrder()
{
    for (int N : { 1, 2, 3, 4 }) {
        FSLocalAlign::Chain chain;
        std::vector<int> order;
        for (int k = 0; k < N; ++k)
            for (const auto &d : chain.Add(Slice(k, 1.0, 0.0, 0))) order.push_back(d.slice);
        for (const auto &d : chain.Finish()) order.push_back(d.slice);
        QCOMPARE(int(order.size()), N);
        for (int k = 0; k < N; ++k) QCOMPARE(order[k], k);
    }
}

void TestFsLocalAlign::resultIsIndependentOfThreadCount()
{
    const int w = 1200, h = 800;
    const cv::Mat reference = Render(w, h, [](double, double) { return cv::Point2d(0, 0); });
    const cv::Mat frame = Render(w, h, Parallax);
    const int saved = cv::getNumThreads();
    cv::setNumThreads(1);
    const FSLocalAlign::Flow a = FSLocalAlign::Estimate(reference, frame);
    cv::Mat wa;
    FSLocalAlign::Warp(frame, Identity(), a, wa);
    cv::setNumThreads(saved);
    const FSLocalAlign::Flow b = FSLocalAlign::Estimate(reference, frame);
    cv::Mat wb;
    FSLocalAlign::Warp(frame, Identity(), b, wb);
    QCOMPARE(a.measured, b.measured);
    QVERIFY(cv::norm(a.dx, b.dx, cv::NORM_INF) == 0 && cv::norm(a.dy, b.dy, cv::NORM_INF) == 0);
    QVERIFY(cv::norm(wa, wb, cv::NORM_INF) == 0);
}

QTEST_GUILESS_MAIN(TestFsLocalAlign)
#include "tst_fslocalalign.moc"
