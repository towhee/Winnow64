/*
    Pins Datamodel/rowselection.h -- the linear replacements for
    QItemSelectionModel::selectedRows on the image selection. See "RECOVERED SELECTIONS
    ARE MERGED INTO RANGES" in notes/Documentation.md.

    Qt's own selectedRows() is the ORACLE: on whole-row selections RowSelection::Rows must
    return the same rows in the same order and RowSelection::Count the same number. The
    cases are the shapes that differ from one tidy range: overlapping and adjacent ranges,
    ranges added out of order, holes punched by deselecting, and a random mix of all of
    them. The last case is the one that beachballed: 150,000 rows with every other row
    selected, which Qt's selectedRows() takes O(rows x ranges) to walk, and which
    QItemSelectionModel::select with the Rows flag takes O(ranges^2) to make.

    RowSelection::Select is checked the same way: the rows it selects must be the rows a
    plain select(..., Rows) of the same rows selects.
*/

#include <QtTest>
#include <QAbstractTableModel>
#include <QElapsedTimer>
#include <QItemSelectionModel>
#include <QRandomGenerator>
#include <algorithm>
#include "Datamodel/rowselection.h"

// Cheap stand-in for the proxy: only rowCount, columnCount and index matter here.
class FakeTable : public QAbstractTableModel
{
public:
    FakeTable(int rows, int cols) : rows(rows), cols(cols) {}
    int rowCount(const QModelIndex &parent = QModelIndex()) const override
    { return parent.isValid() ? 0 : rows; }
    int columnCount(const QModelIndex &parent = QModelIndex()) const override
    { return parent.isValid() ? 0 : cols; }
    QVariant data(const QModelIndex &, int) const override { return QVariant(); }
private:
    int rows, cols;
};

class TstRowSelection : public QObject
{
    Q_OBJECT
private slots:
    void emptySelection();
    void nullModel();
    void selectAllIsOneRange();
    void overlappingRangesCountOnce();
    void adjacentRangesMerge();
    void keepsQtOrder();
    void deselectPunchesHoles();
    void columnArgument();
    void matchesQtOnRandomSelections();
    void rangesMergeAndSpanEveryColumn();
    void rangesDropOutOfRangeRows();
    void selectMatchesQtRowsFlag();
    void fragmentedSelectionIsFast();

private:
    static void selectRows(QItemSelectionModel &sm, int top, int bottom,
                           QItemSelectionModel::SelectionFlags flags
                           = QItemSelectionModel::Select);
    static QList<int> rowsOf(const QModelIndexList &list);
    static void compareWithQt(const QItemSelectionModel &sm);
};

void TstRowSelection::selectRows(QItemSelectionModel &sm, int top, int bottom,
                                 QItemSelectionModel::SelectionFlags flags)
{
    const QAbstractItemModel *m = sm.model();
    sm.select(QItemSelection(m->index(top, 0), m->index(bottom, 0)),
              flags | QItemSelectionModel::Rows);
}

QList<int> TstRowSelection::rowsOf(const QModelIndexList &list)
{
    QList<int> rows;
    for (const QModelIndex &idx : list) rows.append(idx.row());
    return rows;
}

void TstRowSelection::compareWithQt(const QItemSelectionModel &sm)
{
    const QModelIndexList qt = sm.selectedRows();
    QCOMPARE(RowSelection::Count(&sm), qt.count());
    QCOMPARE(rowsOf(RowSelection::Rows(&sm)), rowsOf(qt));
}

void TstRowSelection::emptySelection()
{
    FakeTable model(20, 5);
    QItemSelectionModel sm(&model);
    QCOMPARE(RowSelection::Count(&sm), 0);
    QVERIFY(RowSelection::Rows(&sm).isEmpty());
}

void TstRowSelection::nullModel()
{
    QCOMPARE(RowSelection::Count(nullptr), 0);
    QVERIFY(RowSelection::Rows(nullptr).isEmpty());
}

void TstRowSelection::selectAllIsOneRange()
{
    FakeTable model(1000, 5);
    QItemSelectionModel sm(&model);
    selectRows(sm, 0, 999);
    QCOMPARE(RowSelection::Count(&sm), 1000);
    const QModelIndexList rows = RowSelection::Rows(&sm);
    QCOMPARE(rows.count(), 1000);
    QCOMPARE(rows.first().row(), 0);
    QCOMPARE(rows.last().row(), 999);
    compareWithQt(sm);
}

void TstRowSelection::overlappingRangesCountOnce()
{
    /* One QItemSelection holding overlapping ranges, selected in one call, is the shape
       QItemSelection::select builds when it is handed overlapping rows: it does not
       merge. 2-5 and 4-8 cover 2..8, seven rows, not eight. */
    FakeTable model(20, 5);
    QItemSelectionModel sm(&model);
    QItemSelection selection;
    selection.select(model.index(2, 0), model.index(5, 0));
    selection.select(model.index(4, 0), model.index(8, 0));
    selection.select(model.index(3, 0), model.index(3, 0));     // inside both
    sm.select(selection, QItemSelectionModel::Select | QItemSelectionModel::Rows);
    QCOMPARE(RowSelection::Count(&sm), 7);
    // Qt reorders overlapping ranges inside select(): the set is fixed, the order is Qt's
    QList<int> rows = rowsOf(RowSelection::Rows(&sm));
    std::sort(rows.begin(), rows.end());
    QCOMPARE(rows, QList<int>({2, 3, 4, 5, 6, 7, 8}));
    compareWithQt(sm);
}

void TstRowSelection::adjacentRangesMerge()
{
    FakeTable model(20, 5);
    QItemSelectionModel sm(&model);
    selectRows(sm, 0, 3);
    selectRows(sm, 4, 6);       // touches the first
    selectRows(sm, 10, 10);     // a gap before it
    QCOMPARE(RowSelection::Count(&sm), 8);
    compareWithQt(sm);
}

void TstRowSelection::keepsQtOrder()
{
    /* Rows in range order, not row order: a caller such as Compare (the first 16
       selected) saw this order from Qt and must keep seeing it. */
    FakeTable model(20, 5);
    QItemSelectionModel sm(&model);
    selectRows(sm, 12, 13);
    selectRows(sm, 2, 2);
    selectRows(sm, 7, 8);
    QCOMPARE(rowsOf(RowSelection::Rows(&sm)), QList<int>({12, 13, 2, 7, 8}));
    compareWithQt(sm);
}

void TstRowSelection::deselectPunchesHoles()
{
    FakeTable model(50, 5);
    QItemSelectionModel sm(&model);
    selectRows(sm, 0, 49);
    selectRows(sm, 10, 19, QItemSelectionModel::Deselect);
    selectRows(sm, 30, 30, QItemSelectionModel::Deselect);
    QCOMPARE(RowSelection::Count(&sm), 39);
    compareWithQt(sm);
}

void TstRowSelection::columnArgument()
{
    FakeTable model(10, 5);
    QItemSelectionModel sm(&model);
    selectRows(sm, 3, 4);
    const QModelIndexList rows = RowSelection::Rows(&sm, 2);
    QCOMPARE(rows.count(), 2);
    for (const QModelIndex &idx : rows) QCOMPARE(idx.column(), 2);
    QCOMPARE(rowsOf(rows), rowsOf(sm.selectedRows(2)));
}

void TstRowSelection::matchesQtOnRandomSelections()
{
    /* Select, deselect and toggle random runs, the way shift-click, Cmd-click and a
       filter recovery build a selection, and check against Qt after every step. Small
       enough that Qt's quadratic walk is no cost. */
    QRandomGenerator rng(20261107);
    const int rowCount = 200;
    FakeTable model(rowCount, 4);
    for (int trial = 0; trial < 50; ++trial) {
        QItemSelectionModel sm(&model);
        for (int step = 0; step < 30; ++step) {
            const int top = rng.bounded(rowCount);
            const int bottom = qMin(rowCount - 1, top + rng.bounded(12));
            QItemSelectionModel::SelectionFlags flag;
            switch (rng.bounded(4)) {
            case 0: flag = QItemSelectionModel::Deselect; break;
            case 1: flag = QItemSelectionModel::Toggle; break;
            default: flag = QItemSelectionModel::Select; break;
            }
            selectRows(sm, top, bottom, flag);
            compareWithQt(sm);
            if (QTest::currentTestFailed()) {
                qWarning() << "trial" << trial << "step" << step;
                return;
            }
        }
    }
}

void TstRowSelection::rangesMergeAndSpanEveryColumn()
{
    FakeTable model(20, 6);
    const QItemSelection ranges = RowSelection::Ranges(&model, {9, 3, 4, 5, 4, 12, 13, 0});
    QCOMPARE(ranges.count(), 4);        // 0 | 3-5 | 9 | 12-13
    const QList<QPair<int, int>> want = {{0, 0}, {3, 5}, {9, 9}, {12, 13}};
    for (int i = 0; i < ranges.count(); ++i) {
        QCOMPARE(ranges.at(i).top(), want.at(i).first);
        QCOMPARE(ranges.at(i).bottom(), want.at(i).second);
        QCOMPARE(ranges.at(i).left(), 0);
        QCOMPARE(ranges.at(i).right(), 5);
    }
}

void TstRowSelection::rangesDropOutOfRangeRows()
{
    FakeTable model(10, 3);
    const QItemSelection ranges = RowSelection::Ranges(&model, {-1, -5, 0, 1, 9, 10, 42});
    QCOMPARE(ranges.count(), 2);        // 0-1 | 9
    QCOMPARE(ranges.at(0).top(), 0);
    QCOMPARE(ranges.at(0).bottom(), 1);
    QCOMPARE(ranges.at(1).top(), 9);
    QCOMPARE(ranges.at(1).bottom(), 9);
    QVERIFY(RowSelection::Ranges(&model, {}).isEmpty());
    QVERIFY(RowSelection::Ranges(nullptr, {1, 2}).isEmpty());
}

void TstRowSelection::selectMatchesQtRowsFlag()
{
    /* Random rows, selected with ClearAndSelect / Select / Toggle / Deselect both ways
       into two models that start equal: what ends up selected must match after every
       step. */
    QRandomGenerator rng(1107);
    const int rowCount = 150;
    FakeTable model(rowCount, 5);
    for (int trial = 0; trial < 40; ++trial) {
        QItemSelectionModel qt(&model);
        QItemSelectionModel ours(&model);
        for (int step = 0; step < 12; ++step) {
            QList<int> rows;
            const int k = rng.bounded(30);
            for (int i = 0; i < k; ++i) rows.append(rng.bounded(rowCount));
            QItemSelectionModel::SelectionFlags flag;
            switch (rng.bounded(4)) {
            case 0: flag = QItemSelectionModel::ClearAndSelect; break;
            case 1: flag = QItemSelectionModel::Toggle; break;
            case 2: flag = QItemSelectionModel::Deselect; break;
            default: flag = QItemSelectionModel::Select; break;
            }
            /* Qt's way, one deduplicated range per row (Toggle twice on one row would
               cancel itself, which is not what selecting a row set means). */
            QList<int> unique = rows;
            std::sort(unique.begin(), unique.end());
            unique.erase(std::unique(unique.begin(), unique.end()), unique.end());
            QItemSelection perRow;
            for (int r : unique) perRow.select(model.index(r, 0), model.index(r, 0));
            qt.select(perRow, flag | QItemSelectionModel::Rows);
            RowSelection::Select(&ours, rows, flag | QItemSelectionModel::Rows);

            QList<int> a = rowsOf(qt.selectedRows());
            QList<int> b = rowsOf(ours.selectedRows());
            std::sort(a.begin(), a.end());
            std::sort(b.begin(), b.end());
            QCOMPARE(b, a);
            if (QTest::currentTestFailed()) {
                qWarning() << "trial" << trial << "step" << step;
                return;
            }
        }
    }
}

void TstRowSelection::fragmentedSelectionIsFast()
{
    /* The beachball shape: 150,000 rows, every other one selected, one range each.
       Qt's selectedRows() is ~5.6e9 tests here, and select() with the Rows flag took
       10.9 s to make it; Select, Count and Rows are all near linear. The bound is loose
       (a debug build on a loaded machine), a regression to quadratic is not. */
    const int rowCount = 150000;
    FakeTable model(rowCount, 30);
    QItemSelectionModel sm(&model);
    QList<int> every2nd;
    for (int row = 0; row < rowCount; row += 2) every2nd.append(row);

    QElapsedTimer t;
    t.start();
    RowSelection::Select(&sm, every2nd, QItemSelectionModel::ClearAndSelect);
    const qint64 selectMs = t.restart();
    const int count = RowSelection::Count(&sm);
    const QModelIndexList rows = RowSelection::Rows(&sm);
    const qint64 readMs = t.elapsed();

    QCOMPARE(sm.selection().count(), rowCount / 2);     // still fragmented
    QCOMPARE(count, rowCount / 2);
    QCOMPARE(rows.count(), rowCount / 2);
    QCOMPARE(rows.at(1).row(), 2);
    QCOMPARE(rows.last().row(), rowCount - 2);
    QVERIFY2(selectMs < 2000, qPrintable(QString("Select took %1 ms").arg(selectMs)));
    QVERIFY2(readMs < 2000, qPrintable(QString("Count + Rows took %1 ms").arg(readMs)));
}

QTEST_MAIN(TstRowSelection)
#include "tst_rowselection.moc"
