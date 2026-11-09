#ifndef ROWSELECTION_H
#define ROWSELECTION_H

#include <QItemSelectionModel>
#include <QList>
#include <algorithm>
#include <utility>
#include <vector>

/*
    LINEAR REPLACEMENTS FOR QItemSelectionModel::selectedRows ON THE IMAGE SELECTION
    (dm->selectionModel, shared by thumbView, gridView and tableView).

    Qt's selectedRows() tests every row of every range against every range, because a
    range might cover only some columns -- O(rows x ranges). A fragmented selection (every
    other row, which a filter change can produce) of 152,559 rows is ~2.3e10 tests, and
    Selection::count() runs it on every selectionChanged: a beachball of many minutes.

    Every Winnow image selection is ENTIRE ROWS (selected with QItemSelectionModel::Rows),
    so a row is selected iff some range covers it and the column test can be skipped.
    Rows() keeps Qt's order (range order, then top to bottom, duplicates dropped);
    Count() merges the ranges and sums them, O(ranges log ranges).

    Not for the folder, bookmark or keyword trees: their selections are not row-wise.

    SELECTING HAS THE SAME TRAP. QItemSelectionModel::select with the Rows flag widens
    every range to full width and de-duplicates as it goes, also O(ranges^2): 75,000
    one-row ranges took 10.9 s, 20,000 took 0.77 s. Handed full-width ranges WITHOUT the
    flag, the same 20,000 took 3 ms. Select() does that, and merges runs of adjacent rows
    first. Use it for any selection of many rows; one range (shift-click, select all) is
    no cost either way.
*/
namespace RowSelection {

// Sorted, de-duplicated rows as one FULL-WIDTH range per run of adjacent rows.
inline QItemSelection Ranges(const QAbstractItemModel *model, QList<int> rows)
{
    QItemSelection selection;
    if (!model) return selection;
    const int rowCount = model->rowCount();
    const int lastColumn = qMax(model->columnCount() - 1, 0);
    std::sort(rows.begin(), rows.end());
    int i = 0;
    const int n = rows.count();
    while (i < n && rows.at(i) < 0) ++i;
    while (i < n && rows.at(i) < rowCount) {
        const int top = rows.at(i);
        int bottom = top;
        while (++i < n && rows.at(i) <= bottom + 1 && rows.at(i) < rowCount)
            bottom = qMax(bottom, rows.at(i));
        selection.append(QItemSelectionRange(model->index(top, 0),
                                             model->index(bottom, lastColumn)));
    }
    return selection;
}

/* Select whole rows: the Rows flag is stripped because Ranges() is already full width
   (see above). Out-of-range rows are dropped. */
inline void Select(QItemSelectionModel *sm, const QList<int> &rows,
                   QItemSelectionModel::SelectionFlags flags)
{
    if (!sm || !sm->model()) return;
    sm->select(Ranges(sm->model(), rows), flags & ~QItemSelectionModel::Rows);
}

inline QModelIndexList Rows(const QItemSelectionModel *sm, int column = 0)
{
    QModelIndexList rows;
    if (!sm || !sm->model()) return rows;
    const QAbstractItemModel *model = sm->model();
    const int rowCount = model->rowCount();
    std::vector<bool> seen(rowCount, false);
    for (const QItemSelectionRange &range : sm->selection()) {
        if (!range.isValid() || range.parent().isValid()) continue;
        const int bottom = qMin(range.bottom(), rowCount - 1);
        for (int row = qMax(range.top(), 0); row <= bottom; ++row) {
            if (seen[row]) continue;
            seen[row] = true;
            rows.append(model->index(row, column));
        }
    }
    return rows;
}

inline int Count(const QItemSelectionModel *sm)
{
    if (!sm || !sm->model()) return 0;
    std::vector<std::pair<int, int>> spans;
    for (const QItemSelectionRange &range : sm->selection()) {
        if (!range.isValid() || range.parent().isValid()) continue;
        spans.emplace_back(range.top(), range.bottom());
    }
    std::sort(spans.begin(), spans.end());
    int count = 0;
    int top = -1, bottom = -2;
    for (const auto &span : spans) {
        if (span.first > bottom + 1) {
            count += bottom - top + 1;
            top = span.first;
            bottom = span.second;
        }
        else bottom = qMax(bottom, span.second);
    }
    count += bottom - top + 1;
    return count;
}

} // namespace RowSelection

#endif // ROWSELECTION_H
