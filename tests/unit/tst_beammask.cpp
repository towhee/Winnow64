/*
    BeamMask -- the Beam Gradient submask's geometry (Develop/beammask.h), shared by the
    render (MW evalMaskComp) and the loupe overlay (ImageView::beamEnsureTint). Pins the
    shape, not pixels: the spine is fully covered, both sides fall off alike, the beam
    crosses the whole frame, and the taper narrows the named end to a point.
*/
#include <QtTest>
#include <cmath>
#include "Develop/beammask.h"

class TestBeamMask : public QObject
{
    Q_OBJECT

private slots:
    void spineIsCentreAndEdgeIsOne();
    void bothSidesMatch();
    void untaperedRunsTheWholeFrame();
    void taperNarrowsTowardTheNamedEnd();
    void pastTheApexIsEmpty();
    void edgeGrowsTheHalfWidth();
    void widthIsShortSideFraction();
    void degenerateSpineIsInvalid();
};

namespace {
/* A vertical beam through the centre of a 1000 x 500 frame, half-width 50 px. */
BeamMask::Shape vertical(double taper = 0.0, double edgePx = 0.0)
{
    return BeamMask::make(QPointF(0.5, 0.2), QPointF(0.5, 0.8), 0.1, taper,
                          1000.0, 500.0, edgePx);
}
}

void TestBeamMask::spineIsCentreAndEdgeIsOne()
{
    const BeamMask::Shape b = vertical();
    QVERIFY(b.valid);
    QCOMPARE(b.t(500.0, 250.0), 0.0);
    QVERIFY(std::abs(b.t(550.0, 250.0) - 1.0) < 1e-9);
    QVERIFY(std::abs(b.t(525.0, 250.0) - 0.5) < 1e-9);
}

void TestBeamMask::bothSidesMatch()
{
    const BeamMask::Shape b = vertical(40.0);
    for (double d = 0.0; d < 120.0; d += 7.0)
        for (double y : {30.0, 250.0, 470.0})
            QVERIFY(std::abs(b.t(500.0 + d, y) - b.t(500.0 - d, y)) < 1e-9);
}

void TestBeamMask::untaperedRunsTheWholeFrame()
{
    /* The handles sit at 20% and 80%, but the beam does not stop there. */
    const BeamMask::Shape b = vertical();
    QVERIFY(std::abs(b.t(550.0, 0.0) - 1.0) < 1e-9);
    QVERIFY(std::abs(b.t(550.0, 500.0) - 1.0) < 1e-9);
}

void TestBeamMask::taperNarrowsTowardTheNamedEnd()
{
    /* + narrows toward p2 (y = 400 px), - toward p1 (y = 100 px); 100 meets there. */
    const BeamMask::Shape pos = vertical(100.0);
    QVERIFY(pos.halfWidthAt(150.0) < 1e-9);         // s = +150 is p2
    QVERIFY(std::abs(pos.halfWidthAt(-150.0) - 100.0) < 1e-9);
    const BeamMask::Shape neg = vertical(-50.0);
    QVERIFY(std::abs(neg.halfWidthAt(-150.0) - 25.0) < 1e-9);
    QVERIFY(std::abs(neg.halfWidthAt(150.0) - 75.0) < 1e-9);
    /* The centre width never moves with the taper. */
    QVERIFY(std::abs(pos.halfWidthAt(0.0) - 50.0) < 1e-9);
    QVERIFY(std::abs(neg.halfWidthAt(0.0) - 50.0) < 1e-9);
}

void TestBeamMask::pastTheApexIsEmpty()
{
    const BeamMask::Shape b = vertical(100.0);
    QVERIFY(std::isinf(b.t(500.0, 450.0)));         // beyond p2: the edges have met
    QVERIFY(std::isinf(vertical(100.0, 10.0).t(500.0, 450.0)));   // Edge does not revive it
}

void TestBeamMask::edgeGrowsTheHalfWidth()
{
    QVERIFY(std::abs(vertical(0.0, 10.0).halfWidthAt(0.0) - 60.0) < 1e-9);
    QVERIFY(std::abs(vertical(0.0, -20.0).halfWidthAt(0.0) - 30.0) < 1e-9);
    QVERIFY(vertical(0.0, -500.0).halfWidthAt(0.0) > 0.0);   // clamped, never invalid
}

void TestBeamMask::widthIsShortSideFraction()
{
    /* The same params on the quarter-turned frame give the same pixel width. */
    const BeamMask::Shape a = BeamMask::make(QPointF(0.5, 0.2), QPointF(0.5, 0.8),
                                             0.1, 0.0, 1000.0, 500.0);
    const BeamMask::Shape b = BeamMask::make(QPointF(0.2, 0.5), QPointF(0.8, 0.5),
                                             0.1, 0.0, 500.0, 1000.0);
    QVERIFY(std::abs(a.hw - b.hw) < 1e-9);
}

void TestBeamMask::degenerateSpineIsInvalid()
{
    QVERIFY(!BeamMask::make(QPointF(0.5, 0.5), QPointF(0.5, 0.5), 0.1, 0.0,
                            100.0, 100.0).valid);
    QVERIFY(!BeamMask::fromJson(QJsonObject(), 100.0, 100.0).valid);
}

QTEST_MAIN(TestBeamMask)
#include "tst_beammask.moc"
