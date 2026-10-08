#include <QtTest>
#include "Develop/maskrefine.h"

/*
    Pins Develop/maskrefine.h, the Refine Brush: a painted region in which the mask folded
    so far is re-matted against the photo's own colours. Each case is a synthetic scene
    built to provoke the failure one rule was added for (the rules were found on real
    pictures in the Python prototype, 2026-10-07):

      - Remove takes out what does NOT look like the mask colour, and leaves real sky
        under the stroke alone (painting over sky must change nothing);
      - Remove keeps sky that is PALER than the reference sky above the stroke -- a fixed
        threshold removed it on Joffrey Lake, which is why the split is adaptive (Otsu);
      - Add fills holes that look like the mask colour, never the foreground around them;
      - Add fills holes PALER than the mask colour (sky through branches, Paradise
        Meadows) with no masked pixel near the stroke, and leaves trees AND snow out --
        a fixed 0.08 "looks like the mask" added nothing there, which is why the split
        is relative (k-means in chroma space);
      - Add leaves lit snow out beside pale horizon sky -- same RGB lightness, told apart
        only by chroma;
      - Fix Edge moves a mis-registered soft edge back onto the real one;
      - nothing outside the stroke's region changes, bit for bit.
*/

namespace {

constexpr int W = 240, H = 180;
const cv::Vec3f kSky(0.40f, 0.60f, 0.90f);
const cv::Vec3f kPaleSky(0.52f, 0.68f, 0.92f);   // ~0.15 from kSky: paler horizon sky
const cv::Vec3f kSnow(0.95f, 0.95f, 0.95f);
const cv::Vec3f kTrees(0.10f, 0.18f, 0.08f);
const cv::Vec3f kHaze(0.60f, 0.66f, 0.80f);      // ~0.23 from kSky: sky through branches
const cv::Vec3f kHorizon(0.72f, 0.78f, 0.87f);   // pale horizon sky, near snow in RGB

cv::Mat solid(const cv::Vec3f &c) { return cv::Mat(H, W, CV_32FC3, cv::Scalar(c[0], c[1], c[2])); }

std::vector<float> disk(int cx, int cy, int r)
{
    std::vector<float> v(size_t(W) * H, 0.0f);
    for (int y = 0; y < H; ++y)
        for (int x = 0; x < W; ++x)
            if ((x - cx) * (x - cx) + (y - cy) * (y - cy) <= r * r) v[size_t(y) * W + x] = 1.0f;
    return v;
}

float meanWhere(const std::vector<float> &m, const std::vector<bool> &sel)
{
    double s = 0; int n = 0;
    for (size_t k = 0; k < m.size(); ++k) if (sel[k]) { s += m[k]; ++n; }
    return n ? float(s / n) : -1.0f;
}

}   // namespace

class TstMaskRefine : public QObject
{
    Q_OBJECT
private slots:
    void removeTakesIntruderKeepsSky();
    void removeKeepsPalerHorizonSky();
    void addFillsHolesNotTrees();
    void addFillsPaleHolesFarFromMask();
    void addLeavesSnowOut();
    void fixEdgeSnapsToRealEdge();
    void outsideRegionUntouched();
};

/* Whole frame is sky and masked; a snow patch inside it is wrongly masked too. The stroke
   covers the patch AND a ring of sky around it. */
void TstMaskRefine::removeTakesIntruderKeepsSky()
{
    cv::Mat rgb = solid(kSky);
    cv::circle(rgb, {120, 90}, 25, cv::Scalar(kSnow[0], kSnow[1], kSnow[2]), -1);
    std::vector<float> m(size_t(W) * H, 1.0f);
    const std::vector<float> roi = disk(120, 90, 45);
    MaskRefine::apply(m, roi, rgb, W, H, MaskRefine::Mode::Remove);

    std::vector<bool> snow(m.size()), skyUnder(m.size());
    for (int y = 0; y < H; ++y)
        for (int x = 0; x < W; ++x) {
            const int d2 = (x - 120) * (x - 120) + (y - 90) * (y - 90);
            snow[size_t(y) * W + x] = d2 <= 20 * 20;
            skyUnder[size_t(y) * W + x] = d2 >= 32 * 32 && d2 <= 40 * 40;
        }
    const float snowA = meanWhere(m, snow), skyA = meanWhere(m, skyUnder);
    QVERIFY2(snowA < 0.1f, qPrintable(QString("snow alpha %1").arg(snowA)));
    QVERIFY2(skyA > 0.9f, qPrintable(QString("sky under stroke alpha %1").arg(skyA)));
}

/* The reference sky (outside the stroke) is kSky; under the stroke the sky is paler, and
   there is a snow intruder. The paler sky is ~0.15 away from the reference -- past a
   fixed 0.08 threshold -- but much closer than the snow, so it must stay. */
void TstMaskRefine::removeKeepsPalerHorizonSky()
{
    cv::Mat rgb = solid(kSky);
    cv::rectangle(rgb, {60, 50}, {180, 130}, cv::Scalar(kPaleSky[0], kPaleSky[1], kPaleSky[2]), -1);
    cv::rectangle(rgb, {100, 80}, {140, 110}, cv::Scalar(kSnow[0], kSnow[1], kSnow[2]), -1);
    std::vector<float> m(size_t(W) * H, 1.0f);
    std::vector<float> roi(m.size(), 0.0f);
    for (int y = 45; y <= 135; ++y) for (int x = 55; x <= 185; ++x) roi[size_t(y) * W + x] = 1.0f;
    MaskRefine::apply(m, roi, rgb, W, H, MaskRefine::Mode::Remove);

    std::vector<bool> snow(m.size()), pale(m.size());
    for (int y = 0; y < H; ++y)
        for (int x = 0; x < W; ++x) {
            snow[size_t(y) * W + x] = x >= 104 && x <= 136 && y >= 84 && y <= 106;
            pale[size_t(y) * W + x] = x >= 64 && x <= 176 && y >= 54 && y <= 126
                                      && !(x >= 96 && x <= 144 && y >= 76 && y <= 114);
        }
    const float snowA = meanWhere(m, snow), paleA = meanWhere(m, pale);
    QVERIFY2(snowA < 0.1f, qPrintable(QString("snow alpha %1").arg(snowA)));
    QVERIFY2(paleA > 0.9f, qPrintable(QString("pale sky alpha %1").arg(paleA)));
}

/* Upper half sky (masked), lower half trees (unmasked) with sky-coloured holes the mask
   missed. A stroke across the treeline adds the holes and leaves the trees out. */
void TstMaskRefine::addFillsHolesNotTrees()
{
    cv::Mat rgb = solid(kSky);
    cv::rectangle(rgb, {0, 90}, {W - 1, H - 1}, cv::Scalar(kTrees[0], kTrees[1], kTrees[2]), -1);
    const int holes[3][2] = {{70, 110}, {120, 115}, {170, 108}};
    for (auto &c : holes) cv::circle(rgb, {c[0], c[1]}, 6, cv::Scalar(kSky[0], kSky[1], kSky[2]), -1);
    std::vector<float> m(size_t(W) * H, 0.0f);
    for (int y = 0; y < 90; ++y) for (int x = 0; x < W; ++x) m[size_t(y) * W + x] = 1.0f;
    std::vector<float> roi(m.size(), 0.0f);
    for (int y = 80; y <= 130; ++y) for (int x = 40; x <= 200; ++x) roi[size_t(y) * W + x] = 1.0f;
    MaskRefine::apply(m, roi, rgb, W, H, MaskRefine::Mode::Add);

    std::vector<bool> hole(m.size()), tree(m.size());
    for (int y = 0; y < H; ++y)
        for (int x = 0; x < W; ++x) {
            bool inHole = false;
            for (auto &c : holes)
                inHole |= (x - c[0]) * (x - c[0]) + (y - c[1]) * (y - c[1]) <= 3 * 3;
            hole[size_t(y) * W + x] = inHole;
            bool nearHole = false;
            for (auto &c : holes)
                nearHole |= (x - c[0]) * (x - c[0]) + (y - c[1]) * (y - c[1]) <= 10 * 10;
            tree[size_t(y) * W + x] = y >= 96 && y <= 126 && x >= 46 && x <= 194 && !nearHole;
        }
    const float holeA = meanWhere(m, hole), treeA = meanWhere(m, tree);
    QVERIFY2(holeA > 0.9f, qPrintable(QString("hole alpha %1").arg(holeA)));
    QVERIFY2(treeA < 0.05f, qPrintable(QString("tree alpha %1").arg(treeA)));
}

/* Paradise Meadows in miniature: trees with snow clumps and hazy sky holes, and the only
   masked sky (deeper blue) in a far corner, outside the stroke's context box. */
void TstMaskRefine::addFillsPaleHolesFarFromMask()
{
    cv::Mat rgb = solid(kTrees);
    cv::rectangle(rgb, {200, 0}, {W - 1, 30}, cv::Scalar(kSky[0], kSky[1], kSky[2]), -1);
    const int holes[3][2] = {{50, 100}, {90, 130}, {120, 95}};
    const int snow[2][2] = {{70, 110}, {110, 140}};
    for (auto &c : holes) cv::circle(rgb, {c[0], c[1]}, 7, cv::Scalar(kHaze[0], kHaze[1], kHaze[2]), -1);
    for (auto &c : snow) cv::circle(rgb, {c[0], c[1]}, 7, cv::Scalar(kSnow[0], kSnow[1], kSnow[2]), -1);
    std::vector<float> m(size_t(W) * H, 0.0f);
    for (int y = 0; y <= 30; ++y) for (int x = 200; x < W; ++x) m[size_t(y) * W + x] = 1.0f;
    std::vector<float> roi(m.size(), 0.0f);
    for (int y = 80; y <= 155; ++y) for (int x = 30; x <= 140; ++x) roi[size_t(y) * W + x] = 1.0f;
    MaskRefine::apply(m, roi, rgb, W, H, MaskRefine::Mode::Add);

    auto within = [](int x, int y, const int (*c)[2], int n, int r) {
        for (int i = 0; i < n; ++i)
            if ((x - c[i][0]) * (x - c[i][0]) + (y - c[i][1]) * (y - c[i][1]) <= r * r) return true;
        return false;
    };
    std::vector<bool> hole(m.size()), snowSel(m.size()), tree(m.size());
    for (int y = 85; y <= 150; ++y)
        for (int x = 35; x <= 135; ++x) {
            const size_t k = size_t(y) * W + x;
            hole[k] = within(x, y, holes, 3, 4);
            snowSel[k] = within(x, y, snow, 2, 4);
            tree[k] = !within(x, y, holes, 3, 11) && !within(x, y, snow, 2, 11);
        }
    const float holeA = meanWhere(m, hole), snowA = meanWhere(m, snowSel);
    const float treeA = meanWhere(m, tree);
    QVERIFY2(holeA > 0.9f, qPrintable(QString("hole alpha %1").arg(holeA)));
    QVERIFY2(snowA < 0.05f, qPrintable(QString("snow alpha %1").arg(snowA)));
    QVERIFY2(treeA < 0.05f, qPrintable(QString("tree alpha %1").arg(treeA)));

    // Pure function of its inputs: the render caches depend on it (k-means is seeded).
    std::vector<float> m2(size_t(W) * H, 0.0f);
    for (int y = 0; y <= 30; ++y) for (int x = 200; x < W; ++x) m2[size_t(y) * W + x] = 1.0f;
    MaskRefine::apply(m2, roi, rgb, W, H, MaskRefine::Mode::Add);
    QVERIFY(m2 == m);
}

/* Masked sky above, snowy treetops below with pale horizon sky between them. */
void TstMaskRefine::addLeavesSnowOut()
{
    cv::Mat rgb = solid(kSky);
    cv::rectangle(rgb, {0, 90}, {W - 1, H - 1}, cv::Scalar(kSnow[0], kSnow[1], kSnow[2]), -1);
    const int holes[3][2] = {{70, 110}, {120, 115}, {170, 108}};
    for (auto &c : holes)
        cv::circle(rgb, {c[0], c[1]}, 6, cv::Scalar(kHorizon[0], kHorizon[1], kHorizon[2]), -1);
    std::vector<float> m(size_t(W) * H, 0.0f);
    for (int y = 0; y < 90; ++y) for (int x = 0; x < W; ++x) m[size_t(y) * W + x] = 1.0f;
    std::vector<float> roi(m.size(), 0.0f);
    for (int y = 80; y <= 130; ++y) for (int x = 40; x <= 200; ++x) roi[size_t(y) * W + x] = 1.0f;
    MaskRefine::apply(m, roi, rgb, W, H, MaskRefine::Mode::Add);

    std::vector<bool> hole(m.size()), snowSel(m.size());
    for (int y = 0; y < H; ++y)
        for (int x = 0; x < W; ++x) {
            bool inHole = false, nearHole = false;
            for (auto &c : holes) {
                const int d2 = (x - c[0]) * (x - c[0]) + (y - c[1]) * (y - c[1]);
                inHole |= d2 <= 3 * 3;
                nearHole |= d2 <= 10 * 10;
            }
            hole[size_t(y) * W + x] = inHole;
            snowSel[size_t(y) * W + x] = y >= 96 && y <= 126 && x >= 46 && x <= 194 && !nearHole;
        }
    const float holeA = meanWhere(m, hole), snowA = meanWhere(m, snowSel);
    QVERIFY2(holeA > 0.9f, qPrintable(QString("hole alpha %1").arg(holeA)));
    QVERIFY2(snowA < 0.05f, qPrintable(QString("snow alpha %1").arg(snowA)));
}

/* A crisp sky/ridge step at y = 90, but the mask's edge is soft and sits 4 px too low --
   the misregistration that reads as a halo. Fix Edge must put it back. */
void TstMaskRefine::fixEdgeSnapsToRealEdge()
{
    cv::Mat rgb = solid(kSky);
    cv::rectangle(rgb, {0, 90}, {W - 1, H - 1}, cv::Scalar(kTrees[0], kTrees[1], kTrees[2]), -1);
    std::vector<float> m(size_t(W) * H), truth(m.size());
    for (int y = 0; y < H; ++y)
        for (int x = 0; x < W; ++x) {
            const float t = std::clamp((94.0f - float(y)) / 8.0f + 0.5f, 0.0f, 1.0f);
            m[size_t(y) * W + x] = t;
            truth[size_t(y) * W + x] = y < 90 ? 1.0f : 0.0f;
        }
    std::vector<float> roi(m.size(), 0.0f);
    for (int y = 70; y <= 110; ++y) for (int x = 20; x <= 220; ++x) roi[size_t(y) * W + x] = 1.0f;
    auto err = [&](const std::vector<float> &a) {
        double s = 0; int n = 0;
        for (int y = 80; y <= 100; ++y)
            for (int x = 40; x <= 200; ++x) { s += std::fabs(a[size_t(y) * W + x] - truth[size_t(y) * W + x]); ++n; }
        return s / n;
    };
    const double before = err(m);
    MaskRefine::apply(m, roi, rgb, W, H, MaskRefine::Mode::Edge);
    const double after = err(m);
    QVERIFY2(after < 0.5 * before, qPrintable(QString("edge error %1 -> %2").arg(before).arg(after)));
}

/* Every mode leaves the mask outside the stroke's bounding box + context
   bit-identical. */
void TstMaskRefine::outsideRegionUntouched()
{
    cv::Mat rgb = solid(kSky);
    cv::rectangle(rgb, {0, 90}, {W - 1, H - 1}, cv::Scalar(kTrees[0], kTrees[1], kTrees[2]), -1);
    std::vector<float> m0(size_t(W) * H);
    for (int y = 0; y < H; ++y)
        for (int x = 0; x < W; ++x)
            m0[size_t(y) * W + x] = std::clamp((94.0f - float(y)) / 8.0f + 0.5f, 0.0f, 1.0f);
    const std::vector<float> roi = disk(60, 90, 12);
    for (MaskRefine::Mode mode : {MaskRefine::Mode::Add, MaskRefine::Mode::Remove,
                                  MaskRefine::Mode::Edge}) {
        std::vector<float> m = m0;
        MaskRefine::apply(m, roi, rgb, W, H, mode);
        for (int y = 0; y < H; ++y)
            for (int x = 0; x < W; ++x) {
                /* inside the stroke's box + context margin: may change */
                if (std::abs(x - 60) + std::abs(y - 90) < 12 + 2 * 16 + 8) continue;
                QCOMPARE(m[size_t(y) * W + x], m0[size_t(y) * W + x]);
            }
    }
}

QTEST_MAIN(TstMaskRefine)
#include "tst_maskrefine.moc"
