/*
    Pins Views/thumbsizesteps.h -- the discrete thumbnail sizes that the [ and ] keys
    and the status bar's thumbnail size slider both step through. See "Thumbnail Size
    Steps" in notes/Documentation.md.

    Wrapping views: across a sweep of row widths and paddings, every step's cell is no
    larger than maxIconSize and its thumb no smaller than ICON_MIN (a view too narrow for
    one minimum thumb has exactly one step). One more or one fewer per row breaks a
    bound, so the range is as wide as it can be. Every step is a distinct size whose
    cell fits exactly tpr times, and step <-> tpr round-trips.

    Filmstrip ladder: it starts at ICON_MIN, ends exactly at maxIconSize, rises strictly,
    and up / down / nearest agree with it, so ] then [ returns to the same size.
*/

#include <QtTest>
#include <algorithm>
#include "Views/thumbsizesteps.h"

using namespace ThumbSizeSteps;

static constexpr int minThumb = 60;     // ICON_MIN
static constexpr int maxIcon = 256;     // G::maxIconSize

class TestThumbSizeSteps : public QObject
{
    Q_OBJECT
private slots:
    void wrapRangeBounds();
    void wrapStepRoundTrip();
    void ladder();
};

void TestThumbSizeSteps::wrapRangeBounds()
{
    for (int pad2 : {4, 10, 20}) {
        for (int wRow = 40; wRow <= 6000; wRow += 7) {
            const int lo = tprMin(wRow, maxIcon);
            const int hi = tprMax(wRow, pad2, minThumb, maxIcon);
            QVERIFY(lo >= 1);
            QVERIFY(hi >= lo);
            const std::vector<int> tprs = wrapTprs(wRow, pad2, minThumb, maxIcon);
            QVERIFY(!tprs.empty());
            QCOMPARE(wrapStepCount(wRow, pad2, minThumb, maxIcon), int(tprs.size()));

            for (int tpr : tprs) {
                QVERIFY(tpr >= lo && tpr <= hi);
                const int cell = wRow / tpr;
                QVERIFY2(cell <= maxIcon || tpr == wRow,
                         qPrintable(QString("wRow %1 tpr %2 cell %3")
                                        .arg(wRow).arg(tpr).arg(cell)));
                if (tprs.size() > 1)
                    QVERIFY2(cell - pad2 >= minThumb,
                             qPrintable(QString("wRow %1 tpr %2 thumb %3")
                                            .arg(wRow).arg(tpr).arg(cell - pad2)));
            }
            // As wide as it can be: one more per row is below the minimum thumb...
            QVERIFY(wRow / (hi + 1) - pad2 < minThumb || hi == lo);
            // ...and one fewer per row is above the maximum cell.
            if (lo > 1) QVERIFY(wRow / (lo - 1) > maxIcon);
        }
    }
}

void TestThumbSizeSteps::wrapStepRoundTrip()
{
    for (int pad2 : {4, 10, 20}) {
        for (int wRow = 300; wRow <= 6000; wRow += 13) {
            const int n = wrapStepCount(wRow, pad2, minThumb, maxIcon);
            int prevCell = -1;
            for (int step = 0; step < n; ++step) {
                const int tpr = tprFromWrapStep(step, wRow, pad2, minThumb, maxIcon);
                QCOMPARE(wrapStepFromTpr(tpr, wRow, pad2, minThumb, maxIcon), step);
                /* A justified row reads back the same tpr from its cell width. This is
                   the case that got ] stuck: wRow 4289, tpr 66 gives cell 64, which
                   fits 67 times, so 66 is not a step. */
                QCOMPARE(currentTpr(wRow, wRow / tpr), tpr);
                // Every step is a larger cell than the one below it.
                QVERIFY(wRow / tpr > prevCell);
                prevCell = wRow / tpr;
            }
            // Out-of-range steps clamp to the ends.
            QCOMPARE(tprFromWrapStep(-5, wRow, pad2, minThumb, maxIcon),
                     tprFromWrapStep(0, wRow, pad2, minThumb, maxIcon));
            QCOMPARE(tprFromWrapStep(n + 5, wRow, pad2, minThumb, maxIcon),
                     tprFromWrapStep(n - 1, wRow, pad2, minThumb, maxIcon));
        }
    }
    // The stuck case itself: 66 per row is not a step at 4289 px.
    const std::vector<int> tprs = wrapTprs(4289, 4, minThumb, maxIcon);
    QVERIFY(std::find(tprs.begin(), tprs.end(), 66) == tprs.end());
}

void TestThumbSizeSteps::ladder()
{
    const int n = ladderCount(minThumb, maxIcon);
    QVERIFY(n > 2);
    QCOMPARE(ladderSize(0, minThumb, maxIcon), minThumb);
    QCOMPARE(ladderSize(n - 1, minThumb, maxIcon), maxIcon);
    // No sliver at the top: the last step is a real step up.
    QVERIFY(ladderSize(n - 1, minThumb, maxIcon)
            > 1.05 * ladderSize(n - 2, minThumb, maxIcon));
    for (int k = 1; k < n; ++k)
        QVERIFY(ladderSize(k, minThumb, maxIcon) > ladderSize(k - 1, minThumb, maxIcon));

    for (int k = 0; k < n; ++k) {
        const int s = ladderSize(k, minThumb, maxIcon);
        QCOMPARE(ladderNearest(s, minThumb, maxIcon), k);
        QCOMPARE(ladderUp(s, minThumb, maxIcon), k + 1 < n ? k + 1 : -1);
        QCOMPARE(ladderDown(s, minThumb, maxIcon), k > 0 ? k - 1 : -1);
        // ] then [ returns to the same size.
        if (k + 1 < n) {
            const int up = ladderSize(ladderUp(s, minThumb, maxIcon), minThumb, maxIcon);
            QCOMPARE(ladderSize(ladderDown(up, minThumb, maxIcon), minThumb, maxIcon), s);
        }
    }

    // A size between steps (a dock drag) goes up / down to the neighboring steps.
    const int a = ladderSize(3, minThumb, maxIcon);
    const int b = ladderSize(4, minThumb, maxIcon);
    const int mid = (a + b) / 2;
    QCOMPARE(ladderUp(mid, minThumb, maxIcon), 4);
    QCOMPARE(ladderDown(mid, minThumb, maxIcon), 3);

    // Outside the range.
    QCOMPARE(ladderDown(minThumb - 10, minThumb, maxIcon), -1);
    QCOMPARE(ladderUp(maxIcon + 10, minThumb, maxIcon), -1);
    QCOMPARE(ladderUp(10, minThumb, maxIcon), 0);
}

QTEST_GUILESS_MAIN(TestThumbSizeSteps)
#include "tst_thumbsizesteps.moc"
