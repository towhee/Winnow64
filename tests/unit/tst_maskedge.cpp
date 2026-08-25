/*
    MaskEdge -- the grow/shrink morphology behind the Develop Edge slider.

    The slider's entire contract is "1 unit = 1 pixel", so these are guard rails on
    DISPLACEMENT, not on pixel values. Two things here are impossible to eyeball and easy
    to break:

      - the van Herk / Gil-Werman block arithmetic, whose boundary handling is subtle
        enough that a wrong reset rule shows up only near the ends of a line, and
      - the octagon decomposition, whose whole point is that a boundary moves by r at
        EVERY angle. A separable square would sail through an axis-aligned test and be 41%
        wrong at 45 degrees, which is exactly the regression to catch.

    Displacement is measured on a LINEAR RAMP, where dilation is an exact translation (and
    the fractional-radius lerp is exact too, since lerping two translates of a ramp is a
    translate by the lerped distance). A hard step edge would quantise the crossing and
    prove nothing.
*/
#include <QtTest>
#include <cmath>
#include <random>
#include <vector>
#include "Develop/maskedge.h"

/* Serial dispatcher: these are small buffers and the pass order, not the threading, is
   what is under test. */
static const auto serialPar = [](int n, auto &&fn) { fn(0, n); };

class TestMaskEdge : public QObject
{
    Q_OBJECT

private slots:
    void runningExtremumMatchesNaive();
    void axisAlignedRampMovesByRadius();
    void diagonalRampMovesByRadius();
    void displacementHoldsAtEveryAngle();
    void zeroRadiusIsIdentity();
    void coverageIsMonotoneInRadius();
    void softEdgeRoundTrips();
    void hardDiscSurvivesRoundTrip();
};

namespace {

/* Naive O(r) window extremum, clipped at the ends -- the definition run1D must match. */
template <bool DoMax>
std::vector<uint16_t> naive1D(const std::vector<uint16_t> &a, int R)
{
    const int n = int(a.size());
    std::vector<uint16_t> out(size_t(n), 0);   // 2-arg: 1-arg is a vexing parse
    for (int i = 0; i < n; ++i) {
        int v = DoMax ? 0 : 65535;
        for (int k = std::max(0, i - R); k <= std::min(n - 1, i + R); ++k)
            v = DoMax ? std::max(v, int(a[size_t(k)])) : std::min(v, int(a[size_t(k)]));
        out[size_t(i)] = uint16_t(v);
    }
    return out;
}

/* Sub-pixel x where row y crosses 0.5, by linear interpolation between samples. */
double crossingX(const std::vector<uint16_t> &b, int w, int y)
{
    for (int x = 1; x < w; ++x) {
        const double a = b[size_t(y) * w + x - 1] / 65535.0;
        const double c = b[size_t(y) * w + x] / 65535.0;
        if ((a - 0.5) * (c - 0.5) <= 0.0 && a != c) return (x - 1) + (0.5 - a) / (c - a);
    }
    return -1.0;
}

/* A ramp whose 0.5 level sits on the line dot((x,y), n) == c0, n a unit vector. Coverage
   rises with the projection, so the mask is the far side and GROWING moves the crossing
   back toward the origin. */
std::vector<uint16_t> ramp(int w, int h, double nx, double ny, double c0, double slope)
{
    std::vector<uint16_t> b(size_t(w) * h);
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x) {
            double v = 0.5 + (x * nx + y * ny - c0) * slope;
            v = std::max(0.0, std::min(1.0, v));
            b[size_t(y) * w + x] = MaskEdge::toU16(float(v));
        }
    return b;
}

double coverageSum(const std::vector<uint16_t> &b)
{
    double s = 0.0;
    for (uint16_t v : b) s += v / 65535.0;
    return s;
}

std::vector<uint16_t> disc(int w, int h, double cx, double cy, double r)
{
    std::vector<uint16_t> b(size_t(w) * h, 0);
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x)
            if (std::hypot(x - cx, y - cy) <= r) b[size_t(y) * w + x] = MaskEdge::kOne;
    return b;
}

} // namespace

/* The O(1) block arithmetic must agree with the O(r) definition everywhere, including on
   lines shorter than one window and at every end. */
void TestMaskEdge::runningExtremumMatchesNaive()
{
    std::mt19937 rng(7);
    std::uniform_int_distribution<int> d(0, 65535);
    for (int R : {1, 2, 3, 5, 8, 17, 40}) {
        for (int n : {1, 2, 7, 64, 101}) {
            std::vector<uint16_t> a(size_t(n), 0);
            for (uint16_t &v : a) v = uint16_t(d(rng));
            MaskEdge::Scratch s;
            std::vector<uint16_t> t = a;
            MaskEdge::run1D<true>(t.data(), n, R, s);
            QCOMPARE(t, naive1D<true>(a, R));
            t = a;
            MaskEdge::run1D<false>(t.data(), n, R, s);
            QCOMPARE(t, naive1D<false>(a, R));
        }
    }
}

/* THE CONTRACT: +r moves an axis-aligned boundary out by exactly r px, -r pulls it in.
   Fractional radii included -- a 0.3 px nudge is the normal case in a screen proxy. */
void TestMaskEdge::axisAlignedRampMovesByRadius()
{
    const int w = 400, h = 200;
    for (double r : {1.0, 2.5, 7.0, 13.75, 40.0, -3.0, -11.5}) {
        std::vector<uint16_t> b = ramp(w, h, 1.0, 0.0, 180.0, 1.0 / 60.0);
        const double before = crossingX(b, w, h / 2);
        MaskEdge::apply(b, w, h, r, serialPar);
        const double moved = before - crossingX(b, w, h / 2);
        QVERIFY2(std::abs(moved - r) < 0.06,
                 qPrintable(QString("r=%1 moved %2").arg(r).arg(moved)));
    }
}

/* The octagon's reason for existing. A separable square would move this boundary by
   r*sqrt(2) and fail by 41%. */
void TestMaskEdge::diagonalRampMovesByRadius()
{
    const int w = 400, h = 200;
    const double k = 1.0 / std::sqrt(2.0);
    for (double r : {5.0, 20.0, -9.0}) {
        std::vector<uint16_t> b = ramp(w, h, k, k, 200.0, 1.0 / 60.0);
        auto cross = [&](int y) {
            const double x = crossingX(b, w, y);
            return (x + y) * k;                       // projection onto the normal
        };
        const double before = cross(h / 2);
        MaskEdge::apply(b, w, h, r, serialPar);
        const double moved = before - cross(h / 2);
        QVERIFY2(std::abs(moved - r) < 0.085 * std::abs(r),
                 qPrintable(QString("r=%1 moved %2").arg(r).arg(moved)));
    }
}

/*
    The octagon's guarantee across the WHOLE angle sweep, not just the two angles it is
    exact at. Measured, this traces the octagon's support function: 1.000 at 0 and 45
    degrees, peaking at ~1.081 near 22.5 (the vertex). A separable square would read
    1.000 at 0 and 1.414 at 45 and blow the bound wide open; a bug in the a/b split would
    move the peak or break the symmetry about 22.5.
*/
void TestMaskEdge::displacementHoldsAtEveryAngle()
{
    const int w = 700, h = 700;
    const double r = 30.0, slope = 1.0 / 80.0, c0 = 350.0;
    double worst = 0.0;
    for (double deg = 0.0; deg <= 45.0; deg += 5.0) {
        const double th = deg * M_PI / 180.0, nx = std::cos(th), ny = std::sin(th);
        std::vector<uint16_t> b = ramp(w, h, nx, ny, c0, slope);
        /* Project the row's 0.5 crossing onto the boundary normal. */
        auto proj = [&]() {
            const int y = h / 2;
            const double x = crossingX(b, w, y);
            return x * nx + y * ny;
        };
        const double before = proj();
        MaskEdge::apply(b, w, h, r, serialPar);
        const double ratio = (before - proj()) / r;
        worst = std::max(worst, std::abs(ratio - 1.0));
        QVERIFY2(std::abs(ratio - 1.0) <= 0.0824 + 1e-3,
                 qPrintable(QString("%1 deg moved %2x r").arg(deg).arg(ratio)));
    }
    /* And it really does reach the bound -- a kernel that quietly did nothing would also
       pass the test above. */
    QVERIFY2(worst > 0.05, "octagon never reached its vertex overshoot");
}

void TestMaskEdge::zeroRadiusIsIdentity()
{
    const int w = 64, h = 48;
    std::mt19937 rng(11);
    std::uniform_int_distribution<int> d(0, 65535);
    std::vector<uint16_t> b(size_t(w) * h);
    for (uint16_t &v : b) v = uint16_t(d(rng));
    const std::vector<uint16_t> before = b;
    MaskEdge::apply(b, w, h, 0.0, serialPar);
    QCOMPARE(b, before);
    MaskEdge::apply(b, w, h, 0.01, serialPar);        // below kMinRadius
    QCOMPARE(b, before);
}

/* Guards the fractional lerp: a slider dragged through 0.25 px steps must never jump
   backwards, or the preview stutters under the cursor. */
void TestMaskEdge::coverageIsMonotoneInRadius()
{
    const int w = 200, h = 160;
    double prev = -1.0;
    for (double r = 0.0; r <= 6.0; r += 0.25) {
        std::vector<uint16_t> b = disc(w, h, 100, 80, 40);
        MaskEdge::apply(b, w, h, r, serialPar);
        const double sum = coverageSum(b);
        QVERIFY2(sum >= prev - 1e-6, qPrintable(QString("dropped at r=%1").arg(r)));
        prev = sum;
    }
}

/* Grow then shrink by the same amount must put a soft boundary back where it started. */
void TestMaskEdge::softEdgeRoundTrips()
{
    const int w = 400, h = 200;
    std::vector<uint16_t> b = ramp(w, h, 1.0, 0.0, 180.0, 1.0 / 60.0);
    const double before = crossingX(b, w, h / 2);
    MaskEdge::apply(b, w, h, 12.0, serialPar);
    MaskEdge::apply(b, w, h, -12.0, serialPar);
    QVERIFY(std::abs(crossingX(b, w, h / 2) - before) < 0.05);
}

/* A hard-edged disc is NOT octagon-open, so the round trip facets its boundary slightly;
   what must not happen is a systematic area loss. */
void TestMaskEdge::hardDiscSurvivesRoundTrip()
{
    const int w = 400, h = 200;
    std::vector<uint16_t> b = disc(w, h, 200, 100, 50);
    const double before = coverageSum(b);
    MaskEdge::apply(b, w, h, 10.0, serialPar);
    MaskEdge::apply(b, w, h, -10.0, serialPar);
    QVERIFY(std::abs(coverageSum(b) / before - 1.0) < 0.02);
}

QTEST_MAIN(TestMaskEdge)
#include "tst_maskedge.moc"
