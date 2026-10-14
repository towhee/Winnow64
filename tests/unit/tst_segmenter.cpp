/*
    Segmenter -- the post-processing behind the Develop Segmenter mask tool
    (Utilities/segmenter.h): SAM 2's point-prompt candidates in, the photo's objects out.

    WHAT IS UNDER TEST. Not the model -- a synthetic decoder stands in for it, answering
    each point prompt with the three candidate masks SAM would give for a scene of known
    shapes. What must hold is what the canvas and the render rely on:

      - every object is found once (near-duplicates merge), with a whole object at
        Detail "Objects" and its parts only at "Parts",
      - the label grid answers "which object is under this point" at each level, and a
        frame whose aspect differs from SAM's square input still lines up,
      - a region the grid misses is picked up by the gap rounds when SAM is merely
        unsure of it, and handed over as a "remainder" when SAM cannot segment it at all,
      - a selection survives the JSON it is saved as, holes included (the outline rings
        are even-odd), and rasterizes to the area of the objects selected,
      - a cancelled search returns nothing.

    A green run says the bookkeeping is intact. Whether the objects look right on a
    photograph is only ever answered in the loupe.
*/
#include <QtTest>
#include <cmath>
#include <functional>
#include "Utilities/segmenter.h"

using namespace Segmenter;

namespace {

constexpr double kPi = 3.14159265358979323846;   // M_PI is not standard (MSVC)

/* A shape in mask-normalized coords. */
using Shape = std::function<bool(double x, double y)>;

Shape disk(double cx, double cy, double r)
{
    return [=](double x, double y) { return std::hypot(x - cx, y - cy) <= r; };
}
Shape rect(double x0, double y0, double x1, double y1)
{
    return [=](double x, double y) { return x >= x0 && x <= x1 && y >= y0 && y <= y1; };
}

/* SAM's low-res logits for a shape: a confident +8 inside, -8 outside. A step is the
   most stable mask there is, which is fine -- stability is not what is under test. */
std::vector<float> logits(const Shape &s)
{
    std::vector<float> l(size_t(kMaskRes) * kMaskRes);
    for (int y = 0; y < kMaskRes; ++y)
        for (int x = 0; x < kMaskRes; ++x)
            l[size_t(y) * kMaskRes + x] =
                s((x + 0.5) / kMaskRes, (y + 0.5) / kMaskRes) ? 8.0f : -8.0f;
    return l;
}

/*
    The scene: a disk A holding a smaller disk A1 (its part), a rectangle B, a ring R
    (a disk with a hole), and the background around them. A point prompt is answered
    the way SAM answers one -- the part, the whole, and the whole again -- and the
    background's confidence is the knob the tests turn.
*/
struct Scene {
    Shape A  = disk(0.25, 0.35, 0.15);
    Shape A1 = disk(0.25, 0.35, 0.06);
    Shape B  = rect(0.60, 0.15, 0.85, 0.45);
    Shape R  = [](double x, double y) {
        const double d = std::hypot(x - 0.65, y - 0.75);
        return d <= 0.16 && d >= 0.07;
    };
    Shape bg = [this](double x, double y) {
        return !A(x, y) && !B(x, y) && !R(x, y);
    };
    float bgIou = 0.9f;
    int decoded = 0;
    /* Each shape's logits, made once: a debug build spent its whole run re-making them
       for every prompt. */
    const std::vector<float> lA = logits(A), lA1 = logits(A1), lB = logits(B),
                             lR = logits(R), lBg = logits(bg);

    std::vector<Candidate> decode(const std::vector<QPointF> &pts)
    {
        std::vector<Candidate> out;
        for (const QPointF &p : pts) {
            ++decoded;
            const double x = p.x(), y = p.y();
            std::vector<std::pair<const std::vector<float> *, float>> three;
            if      (A1(x, y)) three = {{&lA1, 0.95f}, {&lA, 0.93f}, {&lA, 0.90f}};
            else if (A(x, y))  three = {{&lA, 0.94f}, {&lA, 0.92f}, {&lA, 0.90f}};
            else if (B(x, y))  three = {{&lB, 0.96f}, {&lB, 0.94f}, {&lB, 0.90f}};
            else if (R(x, y))  three = {{&lR, 0.95f}, {&lR, 0.93f}, {&lR, 0.90f}};
            else               three = {{&lBg, bgIou}, {&lBg, bgIou}, {&lBg, bgIou}};
            for (auto &t : three) {
                Candidate c;
                c.px = float(x);
                c.py = float(y);
                c.iou = t.second;
                c.logits = *t.first;
                out.push_back(std::move(c));
            }
        }
        return out;
    }
    DecodeFn fn() { return [this](const std::vector<QPointF> &p) { return decode(p); }; }
};

/* Index of the segment whose box holds n and whose area is nearest `area`. */
int segmentNear(const Segmentation &s, QPointF n, double area)
{
    int best = -1;
    double bestD = 1e9;
    for (int i = 0; i < s.segs.size(); ++i) {
        if (!s.segs[i].box.adjusted(-0.01, -0.01, 0.01, 0.01).contains(n)) continue;
        const double d = std::abs(s.segs[i].area - area);
        if (d < bestD) { bestD = d; best = i; }
    }
    return best;
}

double covered(const std::vector<float> &a)
{
    double s = 0.0;
    for (float v : a) s += v;
    return s / double(a.size());
}

}   // namespace

class TestSegmenter : public QObject
{
    Q_OBJECT

private slots:
    void findsObjectsOnceWithTheirParts();
    void labelGridAnswersPerLevel();
    void unsureRegionIsFoundByTheGapRounds();
    void unsegmentableRegionBecomesARemainder();
    void selectionRoundTripsThroughJson();
    void ringHoleStaysEmpty();
    void emptySelectionIsNoSelection();
    void cancelledSearchReturnsNothing();
};

void TestSegmenter::findsObjectsOnceWithTheirParts()
{
    Scene sc;
    auto seg = assemble(sc.fn(), 400, 300);
    QVERIFY(seg);
    /* A, A1, B, R and the background: once each, however many prompts landed on them. */
    QCOMPARE(int(seg->segs.size()), 5);
    for (const Segment &s : seg->segs) QVERIFY(!s.remainder);
    /* Area-descending, as the label maps are painted. */
    for (int i = 1; i < seg->segs.size(); ++i)
        QVERIFY(seg->segs[i - 1].area >= seg->segs[i].area);

    const int a  = segmentNear(*seg, {0.25, 0.35}, kPi * 0.15 * 0.15);
    const int a1 = segmentNear(*seg, {0.25, 0.35}, kPi * 0.06 * 0.06);
    QVERIFY(a >= 0 && a1 >= 0 && a != a1);
    QCOMPARE(seg->segs[a].depth, 0);                    // a whole object
    QCOMPARE(seg->segs[a1].depth, 1);                   // a part of one
    QVERIFY(std::abs(seg->segs[a].area - kPi * 0.15 * 0.15) < 0.006);
    QCOMPARE(seg->count(Objects), 4);
    QCOMPARE(seg->count(Parts), 5);
}

void TestSegmenter::labelGridAnswersPerLevel()
{
    Scene sc;
    /* A label grid wider than SAM's square input: mask-normalized coords must still land
       on the same objects. */
    auto seg = assemble(sc.fn(), 480, 270);
    QVERIFY(seg);
    const int a  = segmentNear(*seg, {0.25, 0.35}, kPi * 0.15 * 0.15);
    const int a1 = segmentNear(*seg, {0.25, 0.35}, kPi * 0.06 * 0.06);
    const int b  = segmentNear(*seg, {0.72, 0.30}, 0.25 * 0.30);
    QVERIFY(a >= 0 && a1 >= 0 && b >= 0);
    /* The centre of A1 is A at Objects, A1 at Parts (the smallest offered wins). */
    QCOMPARE(seg->at({0.25, 0.35}, Objects), a);
    QCOMPARE(seg->at({0.25, 0.35}, Parts), a1);
    QCOMPARE(seg->at({0.25, 0.35}, Details), a1);
    /* Inside A but outside A1 is A at every level. */
    QCOMPARE(seg->at({0.25, 0.47}, Parts), a);
    QCOMPARE(seg->at({0.72, 0.30}, Objects), b);
    /* Outside the frame: nothing. */
    QCOMPARE(seg->at({1.2, 0.5}, Objects), -1);
    QCOMPARE(seg->at({-0.1, 0.5}, Objects), -1);
}

void TestSegmenter::unsureRegionIsFoundByTheGapRounds()
{
    /* SAM half-sure of the background: below the grid's bar, above the gap rounds'. */
    Scene sc;
    sc.bgIou = 0.6f;
    auto seg = assemble(sc.fn(), 400, 300);
    QVERIFY(seg);
    QVERIFY(sc.decoded > 256);                          // the gap rounds ran
    bool bgFound = false;
    for (const Segment &s : seg->segs)
        if (!s.remainder && s.area > 0.5) bgFound = true;
    QVERIFY(bgFound);
}

void TestSegmenter::unsegmentableRegionBecomesARemainder()
{
    /* SAM has no idea what the background is: nothing qualifies, even in the gap rounds,
       so the uncovered region is handed over as a remainder -- still clickable. */
    Scene sc;
    sc.bgIou = 0.2f;
    auto seg = assemble(sc.fn(), 400, 300);
    QVERIFY(seg);
    double remainderArea = 0.0;
    for (const Segment &s : seg->segs)
        if (s.remainder) { remainderArea += s.area; QCOMPARE(s.depth, 0); }
    QVERIFY(remainderArea > 0.6);
    /* A point in the background is covered at every level. */
    const int r = seg->at({0.1, 0.9}, Objects);
    QVERIFY(r >= 0);
    QVERIFY(seg->segs[r].remainder);
}

void TestSegmenter::selectionRoundTripsThroughJson()
{
    Scene sc;
    auto seg = assemble(sc.fn(), 400, 300);
    QVERIFY(seg);
    const int a = segmentNear(*seg, {0.25, 0.35}, kPi * 0.15 * 0.15);
    const int b = segmentNear(*seg, {0.72, 0.30}, 0.25 * 0.30);
    QJsonArray sel;
    sel.append(segmentJson(seg->segs[a]));
    sel.append(segmentJson(seg->segs[b]));
    const QString params = paramsWith(sel);
    QVERIFY(hasSelection(params));
    QCOMPARE(selection(params).size(), 2);

    /* The saved outlines rasterize to the two objects' area, at any resolution. */
    const double expect = kPi * 0.15 * 0.15 + 0.25 * 0.30;
    for (QSize sz : {QSize(400, 300), QSize(1200, 675)}) {
        std::vector<float> alpha;
        QVERIFY(rasterize(params, sz.width(), sz.height(), alpha));
        QCOMPARE(alpha.size(), size_t(sz.width()) * sz.height());
        QVERIFY2(std::abs(covered(alpha) - expect) < 0.01,
                 qPrintable(QString("covered %1 expected %2").arg(covered(alpha)).arg(expect)));
    }
    /* And the path the canvas hit-tests with is the same shape. */
    const QPainterPath p = pathFromJson(sel.at(0).toObject());
    QVERIFY(p.contains(QPointF(0.25, 0.35)));
    QVERIFY(!p.contains(QPointF(0.45, 0.35)));
}

void TestSegmenter::ringHoleStaysEmpty()
{
    Scene sc;
    auto seg = assemble(sc.fn(), 400, 300);
    QVERIFY(seg);
    const double ringArea = kPi * (0.16 * 0.16 - 0.07 * 0.07);
    const int r = segmentNear(*seg, {0.65, 0.75}, ringArea);
    QVERIFY(r >= 0);
    QVERIFY(seg->segs[r].rings.size() >= 2);            // the outside and the hole
    const QString params = paramsWith(QJsonArray{segmentJson(seg->segs[r])});
    std::vector<float> alpha;
    QVERIFY(rasterize(params, 400, 300, alpha));
    auto at = [&](double x, double y) { return alpha[size_t(y * 300) * 400 + size_t(x * 400)]; };
    QVERIFY(at(0.65, 0.75) < 0.01);                     // the hole
    QVERIFY(at(0.65 + 0.115, 0.75) > 0.99);             // the ring itself
    QVERIFY(std::abs(covered(alpha) - ringArea) < 0.006);
}

void TestSegmenter::emptySelectionIsNoSelection()
{
    const QString empty = paramsWith(QJsonArray());
    QVERIFY(!hasSelection(empty));
    QVERIFY(!hasSelection(QString()));
    std::vector<float> alpha;
    QVERIFY(!rasterize(empty, 100, 100, alpha));
    QVERIFY(alpha.empty());
}

void TestSegmenter::cancelledSearchReturnsNothing()
{
    Scene sc;
    Progress prog;
    prog.cancel = true;
    QVERIFY(!assemble(sc.fn(), 400, 300, &prog));
    /* And a completed one reports 100%. */
    Progress done;
    QVERIFY(assemble(sc.fn(), 400, 300, &done));
    QCOMPARE(done.pct.load(), 100);
}

QTEST_MAIN(TestSegmenter)
#include "tst_segmenter.moc"
