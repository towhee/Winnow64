#include "selection.h"

/*
    The DataModel selection model operations occur here.

    The selection model behavior is different than the default:

        • the selected rows must be greater than zero
        • the current row must be selected

        This behavior is managed in the DataModel views (thumbView and gridView)
        mouseReleaseEvent, which calls dm->chkForDeselection: check for attempt to
        deselect only selected row (must always be one selected). Also check to see if
        the current row in a selection range greater than one has been deselected. If so,
        then set current to the nearest selected row.

    Current Index:

        The current index is managed in the DataModel, NOT in the selection model.

        • currentDmIdx, currentDmSfIdx
        • currentDmRow, currentDmSfRow


*/

Selection::Selection(QWidget *parent, DataModel *dm, IconView *thumbView, IconView *gridView, TableView *tableView)
{
    if (G::isLogger) G::log("Selection::Selection");
    this->dm = dm;
    this->thumbView = thumbView;
    this->gridView = gridView;
    this->tableView = tableView;
    sm = dm->selectionModel;
    isDebug = false;
}

void Selection::select(QString &fPath)
{
    if (G::isLogger) G::log("Selection::select QString");
    select(dm->proxyIndexFromPath(fPath));
}

void Selection::select(int sfRow)
{
    if (G::isLogger) G::log("Selection::select row");
    select(dm->sf->index(sfRow, 0));
}

void Selection::select(QModelIndex sfIdx)
{
    if (G::isLogger) G::log("Selection::select QModelIndex");
    /*
    if (isDebug)
        qDebug() << "Selection::select QModelIndex" << "instance =" << dm->instance
                          << "idx =" << sfIdx
                          << dm->currentFolderPath;
    //*/
    if (sfIdx.isValid()) {
        thumbView->setCurrentIndex(sfIdx);
        gridView->setCurrentIndex(sfIdx);
        tableView->setCurrentIndex(sfIdx);
        sm->setCurrentIndex(sfIdx, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
        // MW::fileSelectionChange
        emit currentChanged(sfIdx, QModelIndex(), true, "Selection::select");
    }
}

void Selection::next()
{
    if (G::isLogger) G::log("Selection::next");
    int row = dm->currentSfRow;
    if (row < dm->sf->rowCount() - 1)
        row++;
    select(row);
}

void Selection::prev()
{
    if (G::isLogger) G::log("Selection::prev");
    int row = dm->currentSfRow;
    if (row > 0)
        row--;
    select(row);
}

void Selection::up()
{
    if (G::isLogger) G::log("Selection::up");
    if (gridView->isVisible()) {
        select(gridView->upIndex());
        return;
    }
    if (tableView->isVisible()) {
        prev();
        return;
    }
    select(thumbView->pageUpIndex());
}

void Selection::down()
{
    if (G::isLogger) G::log("Selection::down");
    if (gridView->isVisible()) {
        select(gridView->downIndex());
        return;
    }
    if (tableView->isVisible()) {
        next();
        return;
    }
//    select(thumbView->pageDownIndex());
}

void Selection::first()
{
    if (G::isLogger) G::log("Selection::home");
    select(0);
}

void Selection::last()
{
    if (G::isLogger) G::log("Selection::end");
    select(dm->sf->rowCount() - 1);
}

void Selection::prevPage()
{
    if (G::isLogger) G::log("Selection::prevPage");
    if (gridView->isVisible()) {
        select(gridView->pageUpIndex());
        return;
    }
    if (tableView->isVisible()) {
        select(tableView->pageUpIndex());
        return;
    }
    select(thumbView->pageUpIndex());
}

void Selection::nextPage()
{
    if (G::isLogger) G::log("Selection::nextPage");
    if (gridView->isVisible()) {
        select(gridView->pageDownIndex());
        return;
    }
    if (tableView->isVisible()) {
        select(tableView->pageDownIndex());
        return;
    }
    select(thumbView->pageDownIndex());
}

void Selection::nextPick()
{
    if (G::isLogger) G::log("Selection::nextPick");
    select(dm->nextPick());
}

void Selection::prevPick()
{
    if (G::isLogger) G::log("Selection::prevPick");
    select(dm->prevPick());
}

void Selection::all()
{
    if (G::isLogger) G::log("Selection::all");
    QItemSelection selection;
    QModelIndex first = dm->sf->index(0, 0);
    QModelIndex last = dm->sf->index(dm->sf->rowCount() - 1, 0);
    selection.select(first, last);
    sm->select(selection,
               QItemSelectionModel::Clear |
               QItemSelectionModel::Select |
               QItemSelectionModel::Current |
               QItemSelectionModel::Rows);
}

void Selection::random()
{
    if (G::isLogger) G::log("Selection::random");
    select(QRandomGenerator::global()->generate() % static_cast<int>(dm->sf->rowCount()));
}

QModelIndex Selection::nearestSelectedIndex(int sfRow)
{
    if (G::isLogger) G::log("Selection::nearestSelectedIndex");
    int frwd = sfRow;
    int back = frwd;
    int rowCount = dm->sf->rowCount();
    QModelIndex idx;
    while (back >= 0 || frwd < rowCount) {
        if (back >= 0) idx = dm->sf->index(back, 0);
        if (sm->selectedIndexes().contains(idx)) return idx;
        if (frwd < rowCount) idx = dm->sf->index(frwd, 0);
        if (sm->selectedIndexes().contains(idx)) return idx;
        --back;
        ++frwd;
    }
    // not found, return invalid index
    return QModelIndex();

}

void Selection::invert()
{
    if (G::isLogger) G::log("Selection::invert");
    QItemSelection toggleSelection;
    QModelIndex firstIndex = dm->sf->index(0, 0);
    QModelIndex lastIndex = dm->sf->index(dm->sf->rowCount() - 1, 0);
    toggleSelection.select(firstIndex, lastIndex);
    sm->select(toggleSelection, QItemSelectionModel::Toggle);
    QModelIndex idx = nearestSelectedIndex(dm->currentSfRow);
    // MW::fileSelectionChange (updates DataModel current indexes, rows)
    if (idx.isValid()) emit currentChanged(idx);
}

bool Selection::isSelected(int sfRow)
{
    if (G::isLogger) G::log("Selection::isSelectedn");
    return sm->isSelected(dm->sf->index(sfRow, 0));
}

void Selection::chkForDeselection(int sfRow)
/*
    The selection change to sfRow has already occurred.  Check attempt to deselect
    only selected row (must always be one selected).  Also check to see if the
    current row in a selection range greater than one has been deselected.  If so,
    then select the nearest selected row.
*/
{
    if (G::isLogger) G::log("Selection::chkForDeselection");

    QModelIndex idx = dm->sf->index(sfRow, 0);
    int count = sm->selectedRows().count();
    bool isCurrent = idx == dm->currentSfIdx;

    qDebug() << "DataModel::chkForDeselection"
             << "row =" << sfRow
             << "isSelected =" << isSelected(sfRow)
             << "isCurrent =" << isCurrent
             << "count =" << count
                ;

    if (!isSelected(idx.row())) {
        // reselect if no selected rows
        if (sm->selectedRows().count() == 0) {
            sm->select(idx, QItemSelectionModel::Select  | QItemSelectionModel::Rows);
        }
        //
        else {
            if (idx == dm->currentSfIdx) {
                // just deselected current index, move current index to nearest selected
                QModelIndex nearestIdx = nearestSelectedIndex(sfRow);
                emit currentChanged(nearestIdx, QModelIndex(), false);
            }
        }
    }
}

void Selection::save()
{
/*
    This function saves the current selection. This is required, even though the three views
    (thumbView, gridView and tableViews) share the same selection model, because when a view
    is hidden it loses the current index and selection, which has to be re-established each
    time it is made visible.
*/
    if (G::isLogger) G::log("Selection::save");
    selectedRows = sm->selectedRows();
    dm->currentSfIdx = sm->currentIndex();
}

void Selection::recover()
{
    if (G::isLogger) G::log("Selection::recover");
    QItemSelection selection;
    QModelIndex idx;
    foreach (idx, selectedRows)
        selection.select(idx, idx);
    sm->select(selection, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
}
