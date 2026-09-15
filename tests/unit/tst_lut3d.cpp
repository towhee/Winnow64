/*
    Lut3d -- the RGB look-up table a film-look LUT renders through.

    WHY THIS TEST EXISTS. A 3-D LUT fails silently in every direction that matters. An
    index transposition is a red/blue swap that looks deliberate; an off-by-one at the top
    lattice cell reads out of bounds; and the choice of interpolator is invisible until
    someone notices that every grey in the picture has a faint cast. So the properties
    here are the ones that have no other guard:

      o an identity table must be an EXACT no-op, at several lattice sizes;
      o tetrahedral and trilinear must agree exactly at lattice points -- that is what
        makes trilinear usable as an oracle for the sampler that actually renders;
      o the grey axis must survive tetrahedral sampling, and must MEASURABLY NOT survive
        trilinear. The second half is the point: it is a mutation guard, so quietly
        swapping Sample() back to the obvious implementation cannot pass.
*/

#include <QtTest>
#include <cmath>
#include <limits>
#include <vector>

#include "Develop/lut3d.h"

class TstLut3d : public QObject
{
    Q_OBJECT

private slots:
    void identityIsANoOp_data();
    void identityIsANoOp();
    void tetraEqualsTrilinearAtLatticePoints();
    void tetraHoldsTheGreyAxisAndTrilinearDoesNot();
    void domainIsRemappedAndClamped();
    void outOfRangeAndNonFiniteAreSafe();
    void monotoneTableStaysMonotone();
    void goldenOffLatticeProbe();
    void oneDimensionalTable();
    void invalidTableIsANoOp();
};

/* ---------------------------------------------------------------------------------------
   Builders
   ------------------------------------------------------------------------------------ */

/* The identity: entry (r,g,b) holds its own normalised coordinate. */
static Lut3d::Table identity(int n)
{
    Lut3d::Table t;
    t.size = n;
    t.v.resize(size_t(n) * n * n * 3);
    const float d = float(n - 1);
    for (int b = 0; b < n; ++b)
        for (int g = 0; g < n; ++g)
            for (int r = 0; r < n; ++r) {
                float *e = &t.v[(size_t((b * n + g) * n + r)) * 3];
                e[0] = float(r) / d;
                e[1] = float(g) / d;
                e[2] = float(b) / d;
            }
    return t;
}

/*
    Neutral on the diagonal, strongly not neutral off it. Every lattice entry keeps its
    own grey level; the further a colour is from grey, the more red it gains and blue it
    loses. So ANY sampler is neutral AT a lattice point, and only an interpolator that
    stays on the diagonal is neutral BETWEEN them.
*/
static Lut3d::Table neutralAxisSaturatedOffAxis(int n)
{
    Lut3d::Table t = identity(n);
    const float d = float(n - 1);
    for (int b = 0; b < n; ++b)
        for (int g = 0; g < n; ++g)
            for (int r = 0; r < n; ++r) {
                const float fr = float(r) / d, fg = float(g) / d, fb = float(b) / d;
                const float sat = std::max({fr, fg, fb}) - std::min({fr, fg, fb});
                float *e = &t.v[(size_t((b * n + g) * n + r)) * 3];
                e[0] = fr + 0.30f * sat;
                e[2] = fb - 0.30f * sat;
            }
    return t;
}

/* ---------------------------------------------------------------------------------------
   Cases
   ------------------------------------------------------------------------------------ */

void TstLut3d::identityIsANoOp_data()
{
    QTest::addColumn<int>("n");
    /* 2 is the degenerate lattice (one cell, all eight corners); 17/33 are the sizes real
       .cube files use; 64 is a large one. */
    QTest::newRow("n=2")  << 2;
    QTest::newRow("n=17") << 17;
    QTest::newRow("n=33") << 33;
    QTest::newRow("n=64") << 64;
}

void TstLut3d::identityIsANoOp()
{
    QFETCH(int, n);
    const Lut3d::Table t = identity(n);
    QVERIFY(t.is3D());

    /* A dense sweep, deliberately NOT aligned to the lattice -- a sampler can be exact at
       its own nodes and wrong everywhere between them. */
    int probes = 0;
    for (int i = 0; i <= 23; ++i)
        for (int j = 0; j <= 23; ++j)
            for (int k = 0; k <= 23; ++k) {
                const float x = float(i) / 23.0f;
                const float y = float(j) / 23.0f;
                const float z = float(k) / 23.0f;
                float r = x, g = y, b = z;
                Lut3d::Apply(t, r, g, b);
                QVERIFY2(std::fabs(r - x) < 1e-6f &&
                         std::fabs(g - y) < 1e-6f &&
                         std::fabs(b - z) < 1e-6f,
                         qPrintable(QString("identity moved (%1,%2,%3) to (%4,%5,%6)")
                                        .arg(x).arg(y).arg(z).arg(r).arg(g).arg(b)));
                ++probes;
            }
    QCOMPARE(probes, 24 * 24 * 24);
}

void TstLut3d::tetraEqualsTrilinearAtLatticePoints()
{
    /* At a lattice point every interpolator must return that entry exactly, so the two
       agree. This is what licenses trilinear as the oracle everywhere else. */
    const int n = 9;
    const Lut3d::Table t = neutralAxisSaturatedOffAxis(n);
    const float d = float(n - 1);
    for (int b = 0; b < n; ++b)
        for (int g = 0; g < n; ++g)
            for (int r = 0; r < n; ++r) {
                const float x = float(r) / d, y = float(g) / d, z = float(b) / d;
                float ar = x, ag = y, ab = z;
                float br = x, bg = y, bb = z;
                Lut3d::Sample(t, ar, ag, ab);
                Lut3d::SampleTrilinear(t, br, bg, bb);
                QVERIFY(std::fabs(ar - br) < 1e-6f);
                QVERIFY(std::fabs(ag - bg) < 1e-6f);
                QVERIFY(std::fabs(ab - bb) < 1e-6f);
                /* and it really is the stored entry */
                const float *e = &t.v[(size_t((b * n + g) * n + r)) * 3];
                QVERIFY(std::fabs(ar - e[0]) < 1e-6f);
                QVERIFY(std::fabs(ag - e[1]) < 1e-6f);
                QVERIFY(std::fabs(ab - e[2]) < 1e-6f);
            }
}

void TstLut3d::tetraHoldsTheGreyAxisAndTrilinearDoesNot()
{
    const int n = 9;
    const Lut3d::Table t = neutralAxisSaturatedOffAxis(n);

    float worstTetra = 0.0f;
    float worstTri   = 0.0f;
    /* Sample ON the diagonal but BETWEEN lattice nodes -- the only place the two
       interpolators can disagree about a neutral. */
    for (int i = 1; i < 200; ++i) {
        const float x = float(i) / 200.0f;
        float ar = x, ag = x, ab = x;
        Lut3d::Sample(t, ar, ag, ab);
        worstTetra = std::max(worstTetra, std::max(std::fabs(ar - ag),
                                                   std::fabs(ag - ab)));
        float br = x, bg = x, bb = x;
        Lut3d::SampleTrilinear(t, br, bg, bb);
        worstTri = std::max(worstTri, std::max(std::fabs(br - bg),
                                               std::fabs(bg - bb)));
    }

    QVERIFY2(worstTetra < 1e-6f,
             qPrintable(QString("tetrahedral tinted a neutral by %1").arg(worstTetra)));
    /* THE MUTATION GUARD. If this ever passes, Sample() has been swapped for something
       that blends off-axis corners and the property above no longer means anything. */
    QVERIFY2(worstTri > 1e-3f,
             qPrintable(QString("trilinear held the grey axis (%1) -- the neutral case "
                                "above no longer proves the sampler").arg(worstTri)));
}

void TstLut3d::domainIsRemappedAndClamped()
{
    /* A table indexed over 0.2..0.8: input 0.2 must land on the first entry and 0.8 on
       the last, with everything outside pinned to those. */
    Lut3d::Table t = identity(5);
    for (int c = 0; c < 3; ++c) { t.domainMin[c] = 0.2f; t.domainMax[c] = 0.8f; }

    float r = 0.2f, g = 0.2f, b = 0.2f;
    Lut3d::Apply(t, r, g, b);
    QVERIFY(std::fabs(r) < 1e-6f);

    r = 0.8f; g = 0.8f; b = 0.8f;
    Lut3d::Apply(t, r, g, b);
    QVERIFY(std::fabs(r - 1.0f) < 1e-6f);

    /* Midpoint of the domain is the midpoint of the table. */
    r = 0.5f; g = 0.5f; b = 0.5f;
    Lut3d::Apply(t, r, g, b);
    QVERIFY(std::fabs(r - 0.5f) < 1e-6f);

    /* Outside clamps rather than extrapolating. */
    r = -5.0f; g = -5.0f; b = -5.0f;
    Lut3d::Apply(t, r, g, b);
    QVERIFY(std::fabs(r) < 1e-6f);
    r = 99.0f; g = 99.0f; b = 99.0f;
    Lut3d::Apply(t, r, g, b);
    QVERIFY(std::fabs(r - 1.0f) < 1e-6f);

    /* A degenerate domain must not divide by zero. */
    Lut3d::Table bad = identity(5);
    for (int c = 0; c < 3; ++c) { bad.domainMin[c] = 0.5f; bad.domainMax[c] = 0.5f; }
    r = 0.5f; g = 0.5f; b = 0.5f;
    Lut3d::Apply(bad, r, g, b);
    QVERIFY(std::isfinite(r) && std::isfinite(g) && std::isfinite(b));
}

void TstLut3d::outOfRangeAndNonFiniteAreSafe()
{
    const Lut3d::Table t = identity(17);
    const float nan = std::numeric_limits<float>::quiet_NaN();
    const float inf = std::numeric_limits<float>::infinity();
    const float probes[] = {nan, inf, -inf, -1.0f, 2.0f, 0.0f, 1.0f};
    for (float x : probes)
        for (float y : probes)
            for (float z : probes) {
                float r = x, g = y, b = z;
                Lut3d::Apply(t, r, g, b);
                QVERIFY2(std::isfinite(r) && std::isfinite(g) && std::isfinite(b),
                         "a non-finite input produced a non-finite output");
                QVERIFY(r >= -1e-6f && r <= 1.0f + 1e-6f);
                QVERIFY(g >= -1e-6f && g <= 1.0f + 1e-6f);
                QVERIFY(b >= -1e-6f && b <= 1.0f + 1e-6f);
            }
}

void TstLut3d::monotoneTableStaysMonotone()
{
    /* A per-channel gamma baked into the table: monotone in, monotone out. */
    const int n = 17;
    Lut3d::Table t = identity(n);
    for (size_t i = 0; i < t.v.size(); ++i) t.v[i] = std::pow(t.v[i], 2.2f);

    float prev = -1.0f;
    for (int i = 0; i <= 500; ++i) {
        const float x = float(i) / 500.0f;
        float r = x, g = x, b = x;
        Lut3d::Apply(t, r, g, b);
        QVERIFY2(r >= prev - 1e-6f, "a monotone table produced a non-monotone result");
        prev = r;
    }
}

void TstLut3d::goldenOffLatticeProbe()
{
    /*
        Hand-computed, so it fails if the tetrahedron selection changes at all.

        A 2x2x2 table: every corner is the identity EXCEPT (1,1,1), which is pulled to
        mid-grey. Probe (0.7, 0.2, 0.5): fr > fg and fr > fb, so the walk is
        c000 -> c100 -> c101 -> c111 with weights (fr, fb, fg) = (0.7, 0.5, 0.2):

            out = c000 + 0.7*(c100 - c000) + 0.5*(c101 - c100) + 0.2*(c111 - c101)
                = 0.7*(1,0,0) + 0.5*(0,0,1) + 0.2*((0.5,0.5,0.5) - (1,0,1))
                = (0.7, 0, 0) + (0, 0, 0.5) + (-0.1, 0.1, -0.1)
                = (0.6, 0.1, 0.4)
    */
    Lut3d::Table t = identity(2);
    float *c111 = &t.v[(size_t((1 * 2 + 1) * 2 + 1)) * 3];
    c111[0] = 0.5f; c111[1] = 0.5f; c111[2] = 0.5f;

    float r = 0.7f, g = 0.2f, b = 0.5f;
    Lut3d::Apply(t, r, g, b);
    QVERIFY(std::fabs(r - 0.6f) < 1e-6f);
    QVERIFY(std::fabs(g - 0.1f) < 1e-6f);
    QVERIFY(std::fabs(b - 0.4f) < 1e-6f);
}

void TstLut3d::oneDimensionalTable()
{
    /* Per-channel curve: red inverted, green identity, blue halved. */
    Lut3d::Table t;
    t.size1D = 2;
    t.v1D = {1.0f, 0.0f, 0.0f,
             0.0f, 1.0f, 0.5f};
    QVERIFY(t.is1D());
    QVERIFY(!t.is3D());
    QVERIFY(t.isValid());

    float r = 0.25f, g = 0.25f, b = 0.25f;
    Lut3d::Apply(t, r, g, b);
    QVERIFY(std::fabs(r - 0.75f) < 1e-6f);
    QVERIFY(std::fabs(g - 0.25f) < 1e-6f);
    QVERIFY(std::fabs(b - 0.125f) < 1e-6f);
}

void TstLut3d::invalidTableIsANoOp()
{
    /* A table that failed to parse must leave the pixel alone rather than corrupt it --
       the render path is not where a bad file is discovered. */
    Lut3d::Table empty;
    QVERIFY(empty.isEmpty());
    float r = 0.3f, g = 0.6f, b = 0.9f;
    Lut3d::Apply(empty, r, g, b);
    QCOMPARE(r, 0.3f);
    QCOMPARE(g, 0.6f);
    QCOMPARE(b, 0.9f);

    /* Right size field, wrong vector length -- the shape a truncated read produces. */
    Lut3d::Table truncated;
    truncated.size = 8;
    truncated.v.resize(10);
    QVERIFY(truncated.isEmpty());
    r = 0.3f;
    Lut3d::Apply(truncated, r, g, b);
    QCOMPARE(r, 0.3f);
}

QTEST_MAIN(TstLut3d)
#include "tst_lut3d.moc"
