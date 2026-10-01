// Unit tests for Develop/fillspot.h -- the spot heal coverage raster.
//
// Coverage is stored only over the rect a spot's strokes can touch (a full-frame W*H
// buffer per spot per render cost 817 of 925 ms for a 68-spot image in a Debug build).
// These tests pin that the local raster is IDENTICAL to a full-frame one at every pixel
// -- including paint/erase ordering and clipping at the frame edge -- and that it stays
// local.

#include <QtTest>
#include "Develop/fillspot.h"

using namespace FillSpotGeom;

namespace {

/* The reference: the same strokes rasterized into a full W*H frame, the way the
   coverage was built before it became spot-local. */
std::vector<float> fullFrame(const Parsed &p, int w, int h)
{
    std::vector<float> cov(size_t(w) * h, 0.0f);
    std::vector<Stroke> strokes = p.strokes;
    if (strokes.empty()) strokes.push_back({p.sizeFrac, false, p.pts});
    std::vector<float> tmp;
    int sx0, sy0, sx1, sy1;
    for (const Stroke &st : strokes) {
        if (!st.erase) {
            rasterizeStrokeMax(st, w, h, cov, 0, 0, w, h, sx0, sy0, sx1, sy1);
        } else {
            tmp.assign(size_t(w) * h, 0.0f);
            rasterizeStrokeMax(st, w, h, tmp, 0, 0, w, h, sx0, sy0, sx1, sy1);
            for (size_t i = 0; i < cov.size(); ++i) cov[i] *= 1.0f - tmp[i];
        }
    }
    return cov;
}

void compareEverywhere(const Parsed &p, int w, int h)
{
    Coverage c;
    rasterize(p, w, h, c);
    const std::vector<float> ref = fullFrame(p, w, h);
    int bx0 = w, by0 = h, bx1 = -1, by1 = -1;
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            const float r = ref[size_t(y) * w + x];
            if (c.at(x, y) != r)
                QFAIL(qPrintable(QString("coverage differs at %1,%2: %3 vs %4")
                                 .arg(x).arg(y).arg(c.at(x, y)).arg(r)));
            if (r > 0.0f) {
                bx0 = std::min(bx0, x); by0 = std::min(by0, y);
                bx1 = std::max(bx1, x); by1 = std::max(by1, y);
            }
        }
    }
    QCOMPARE(c.bx0, bx0); QCOMPARE(c.by0, by0);
    QCOMPARE(c.bx1, bx1); QCOMPARE(c.by1, by1);
}

}   // namespace

class TstFillSpot : public QObject
{
    Q_OBJECT
private slots:
    void spotMatchesFullFrame();
    void paintEraseMatchesFullFrame();
    void clipsAtFrameEdge();
    void storageIsLocal();
    void eraseEverythingIsEmpty();
};

void TstFillSpot::spotMatchesFullFrame()
{
    Parsed p = parse(toJson(0.0128, 40.0, {0.5405, 0.6409}, Spot));
    compareEverywhere(p, 640, 427);
}

void TstFillSpot::paintEraseMatchesFullFrame()
{
    /* Paint a stroke, erase across its middle, repaint part of it: order matters. */
    std::vector<Stroke> s = {
        {0.05, false, {0.2, 0.3, 0.6, 0.35, 0.7, 0.6}},
        {0.03, true,  {0.4, 0.2, 0.42, 0.5}},
        {0.02, false, {0.41, 0.33, 0.43, 0.36}},
    };
    Parsed p = parse(toJson(50.0, Fill, s));
    compareEverywhere(p, 500, 300);
}

void TstFillSpot::clipsAtFrameEdge()
{
    compareEverywhere(parse(toJson(0.06, 40.0, {0.0, 0.0}, Spot)), 400, 300);
    compareEverywhere(parse(toJson(0.06, 40.0, {1.0, 1.0}, Spot)), 400, 300);
}

void TstFillSpot::storageIsLocal()
{
    /* A 34 px spot on a 12 MP frame must not allocate the frame. */
    Coverage c;
    rasterize(parse(toJson(0.008, 40.0, {0.5, 0.5}, Spot)), 4256, 2832, c);
    QVERIFY(!c.empty());
    QVERIFY(c.data.size() < 4096);
    QCOMPARE(c.at(0, 0), 0.0f);
    QCOMPARE(c.at(2128, 1416), 1.0f);
}

void TstFillSpot::eraseEverythingIsEmpty()
{
    std::vector<Stroke> s = {
        {0.02, false, {0.5, 0.5}},
        {0.10, true,  {0.5, 0.5}},
    };
    Coverage c;
    rasterize(parse(toJson(50.0, Fill, s)), 400, 300, c);
    QVERIFY(c.empty());
}

QTEST_GUILESS_MAIN(TstFillSpot)
#include "tst_fillspot.moc"
