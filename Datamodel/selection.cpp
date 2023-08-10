#include "selection.h"

/*
    The DataModel selection model (dm->selectionModel) operations occur here.

    The selection model behavior is different than the default:

        • the selected rows must be greater than zero
        • the current row must be selected

        This behavior is enfored in Selection::currentIndex and Selection::toggle.
        If multiple rows are selected and the current row is deselected then the
        nearest selected row will be set as the current index.

    Current Index:

        The current index is managed in the DataModel, NOT in the selection model.

        • currentDmIdx, currentSfIdx
        • currentDmRow, currentSfRow

        IconViewDelegate::paint uses dm->currentSfIdx to identify the current index and
        paints it with a yellow border.

    Views

        There are three views of the DataModel: ThumbView, GridView and TableView.

        The views all use the same dm->selectionModel (sm) and are updated by:

            • sm->select
            • sm->setCurrentIndex
*/

Selection::Selection(QWidget *parent,
                     DataModel *dm,
                     IconView *thumbView,
                     IconView *gridView,
                     TableView *tableView) : QObject(parent)
{
    if (G::isLogger) G::log("Selection::Selection");
    this->dm = dm;
    this->thumbView = thumbView;
    this->gridView = gridView;
    this->tableView = tableView;
    sm = dm->selectionModel;
    isDebug = false;
}

void Selection::setCurrentPath(QString &fPath)
{
    if (G::isLogger || isDebug) G::log("Selection::current QString");
    setCurrentIndex(dm->proxyIndexFromPath(fPath));
}

void Selection::setCurrentRow(int sfRow)
{
    if (G::isLogger || isDebug) G::log("Selection::current row");
    if (G::isFlowLogger2) qDebug() << "Selection::currentRow" << "row =" << sfRow;
    setCurrentIndex(dm->sf->index(sfRow, 0));
}

void Selection::setCurrentIndex(QModelIndex sfIdx)
/*
    This is the start for the core program flow (see top of mainwindow.cpp)
*/
{
    if (G::isFlowLogger2) qDebug() << "Selection::currentIndex" << "row =" << sfIdx.row();
    if (!G::isInitializing && (G::isLogger || G::isFlowLogger))
        G::log("Selection::currentIndex", "row = " + QString::number(sfIdx.row()));
    if (isDebug)
        G::log("Selection::currentIndex", "row = " + QString::number(sfIdx.row()));

    if (!sfIdx.isValid()) return;

    updateCurrentIndex(sfIdx);
//    /*
    qDebug() << "Selection::setCurrentIndex"
             << "sfIdx =" << sfIdx
        ;
        //*/

    sm->setCurrentIndex(sfIdx, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);

    bool fileSelectionChangeTriggered = false;
    if (dm->sf->index(dm->currentSfRow, G::MetadataLoadedColumn).data().toBool()) {
        emit fileSelectionChange(sfIdx, QModelIndex(), true, "Selection::select");
        fileSelectionChangeTriggered = true;
    }
    else {
        if (G::isLoadConcurrent) {
            bool scrollOnly = false;
            emit loadConcurrent(sfIdx.row(), scrollOnly, fileSelectionChangeTriggered);
        }
    }
}

void Selection::updateCurrentIndex(QModelIndex sfIdx)
{
    if (G::isLogger || isDebug) G::log("Selection::updateCurrentIndex");

    // update cursor
    dm->currentSfIdx = sfIdx;
    dm->currentSfRow = sfIdx.row();
    dm->currentDmIdx = dm->sf->mapToSource(dm->currentSfIdx);
    dm->currentDmRow = dm->currentDmIdx.row();
    shiftAnchorIndex = sfIdx;
    shiftExtendIndex = sfIdx;
}

void Selection::select(QString &fPath, Qt::KeyboardModifiers modifiers)
{
    if (G::isLogger || isDebug) G::log("Selection::select QString");
    select(dm->proxyIndexFromPath(fPath), modifiers);
}

void Selection::select(int sfRow, Qt::KeyboardModifiers modifiers)
{
    if (G::isLogger || isDebug) G::log("Selection::select row");
    QModelIndex sfIdx = dm->sf->index(sfRow, 0);
    qDebug() << "Selection::select_row  sfRow =" << sfRow << "sfIdx =" << sfIdx << modifiers;
    select(sfIdx, modifiers);
}

void Selection::select(QModelIndex sfIdx, Qt::KeyboardModifiers modifiers)
{
    if (G::isLogger || isDebug) G::log("Selection::select QModelIndex");
    /*
    bool isNoModifier = modifiers & Qt::NoModifier;
    bool isControlModifier = modifiers & Qt::ControlModifier;
    bool isShiftModifier = modifiers & Qt::ShiftModifier;
    bool isAltModifier = modifiers & Qt::AltModifier;
    bool isMetaModifier = modifiers & Qt::MetaModifier;
    bool isKeypadModifier = modifiers & Qt::KeypadModifier;
    Qt::KeyboardModifiers m_modifiers = modifiers ^ Qt::KeypadModifier;
    qDebug() << "Selection::select" << "sfIdx =" << sfIdx << modifiers
             << "isNoModifier =" << isNoModifier
             << "isControlModifier =" << isControlModifier
             << "isShiftModifier =" << isShiftModifier
             << "isAltModifier =" << isAltModifier
             << "isMetaModifier =" << isMetaModifier
             << "isKeypadModifier =" << isKeypadModifier
                ;
            //*/
    if (modifiers & Qt::ControlModifier) {
        toggleSelect(sfIdx);
        // if is selected set as new shiftAnchorIndex
        if (sm->isSelected(sfIdx)) {
            shiftAnchorIndex = sfIdx;
            shiftExtendIndex = sfIdx;
        }
        qDebug() << "Selection::select  ControlModifier  shiftAnchorIndex =" << shiftAnchorIndex;
        return;
    }
    if (modifiers & Qt::ShiftModifier) {
        qDebug() << "Selection::select  ShiftModifier from" << shiftAnchorIndex.row() << "to" << sfIdx.row();
        shiftExtendIndex = sfIdx;
        QItemSelection selection;
        selection.select(shiftAnchorIndex, shiftExtendIndex);
        sm->select(selection, QItemSelectionModel::Select | QItemSelectionModel::Rows);
        return;
    }
    qDebug() << "Selection::select  Fall through";
    sm->clear();
    setCurrentIndex(sfIdx);
}


void Selection::toggleSelect(QModelIndex sfIdx)
{
    if (G::isLogger || isDebug) G::log("Selection::toggleSelect");
    /*
    bool test = sfIdx == dm->currentSfIdx;
    qDebug() << "Selection::toggleSelect  nextIdx"
             << "sfIdx =" << sfIdx
             << "dm->currentSfIdx =" << dm->currentSfIdx
             << "Equal =" << test
                ;
                //*/
    // current index is always selected
    if (sfIdx == dm->currentSfIdx) {
        QModelIndex nextIdx = nearestSelectedIndex(sfIdx.row());
        //qDebug() << "Selection::toggleSelect  nextIdx =" << nextIdx;
        if (nextIdx.isValid()) {
            updateCurrentIndex(nextIdx);
            sm->setCurrentIndex(nextIdx, QItemSelectionModel::Select | QItemSelectionModel::Rows);

            // update delegates to show current and set focus to enable shift + direction keys
            if (thumbView->isVisible()) thumbView->updateView();
            if (gridView->isVisible()) gridView->updateView();

            emit fileSelectionChange(nextIdx, QModelIndex(), false, "Selection::toggleSelect");
        }
        else return;
    }
    QItemSelection toggleSelection;
    toggleSelection.select(sfIdx, sfIdx);
    sm->select(toggleSelection, QItemSelectionModel::Toggle | QItemSelectionModel::Rows);

}

void Selection::next(Qt::KeyboardModifiers modifiers)
{
    if (G::isLogger || isDebug) G::log("Selection::next");
    G::fileSelectionChangeSource = "Key_Right";
    if (modifiers & Qt::ShiftModifier) {
        while (sm->isSelected(shiftExtendIndex)) {
            int row = shiftExtendIndex.row();
            if (row == dm->sf->rowCount() - 1) break;
            shiftExtendIndex = dm->sf->index(++row, 0);
        }
        select(shiftExtendIndex, modifiers);
    }
    else {
        int row = dm->currentSfRow;
        if (row < dm->sf->rowCount() - 1) row++;
        qDebug() << "Selection::next row =" << row << modifiers;
        select(row, modifiers);
    }
}

void Selection::prev(Qt::KeyboardModifiers modifiers)
{
    G::fileSelectionChangeSource = "Key_Left";
    if (modifiers & Qt::ShiftModifier) {
        while (sm->isSelected(shiftExtendIndex)) {
            int row = shiftExtendIndex.row();
            if (row == 0) break;
            shiftExtendIndex = dm->sf->index(--row, 0);
        }
        select(shiftExtendIndex, modifiers);
    }
    else {
        if (G::isLogger || isDebug) G::log("Selection::prev");
        int row = dm->currentSfRow;
        if (row > 0) row--;
        select(row, modifiers);
    }
}

void Selection::up(Qt::KeyboardModifiers modifiers)
{
    if (G::isLogger || isDebug) G::log("Selection::up");
    G::fileSelectionChangeSource = "Key_Up";
    if (gridView->isVisible()) {
        select(gridView->upIndex(), modifiers);
    }
    if (tableView->isVisible()) {
        qDebug() << "Selection::up" << "row before =" << dm->currentSfRow;
        prev(modifiers);
    }
//    setCurrentIndex(thumbView->pageUpIndex());
}

void Selection::down(Qt::KeyboardModifiers modifiers)
{
    if (G::isLogger || isDebug) G::log("Selection::down");
    G::fileSelectionChangeSource = "Key_Down";
    if (gridView->isVisible()) {
        select(gridView->downIndex(), modifiers);
        return;
    }
    if (tableView->isVisible()) {
        next(modifiers);
        return;
    }
//    select(thumbView->pageDownIndex());
}

void Selection::first(Qt::KeyboardModifiers modifiers)
{
    if (G::isLogger || isDebug) G::log("Selection::home");
    G::fileSelectionChangeSource = "Key_Home";
    select(0, modifiers);
}

void Selection::last(Qt::KeyboardModifiers modifiers)
{
    if (G::isLogger || isDebug) G::log("Selection::end");
    G::fileSelectionChangeSource = "Key_End";
    select(dm->sf->rowCount() - 1, modifiers);
}

void Selection::prevPage(Qt::KeyboardModifiers modifiers)
{
    if (G::isLogger || isDebug) G::log("Selection::prevPage");
    qDebug() << "Selection::prevPage";
    G::fileSelectionChangeSource = "Key_PageDown";
    int fromRow = dm->currentSfRow;
    if (modifiers & Qt::ShiftModifier) fromRow = shiftExtendIndex.row();
    if (gridView->isVisible()) {
        select(gridView->pageDownIndex(fromRow), modifiers);
        return;
    }
    if (tableView->isVisible()) {
        select(tableView->pageDownIndex(fromRow), modifiers);
        return;
    }
//    select(thumbView->pageDownIndex(), modifiers);
}

void Selection::nextPage(Qt::KeyboardModifiers modifiers)
{
    if (G::isLogger || isDebug) G::log("Selection::nextPage");
    qDebug() << "Selection::nextPage";
    G::fileSelectionChangeSource = "Key_PageUp";
    int fromRow = dm->currentSfRow;
    if (modifiers & Qt::ShiftModifier) fromRow = shiftExtendIndex.row();
    if (gridView->isVisible()) {
        select(gridView->pageUpIndex(fromRow), modifiers);
        return;
    }
    if (tableView->isVisible()) {
        select(tableView->pageUpIndex(fromRow), modifiers);
        return;
    }
//    select(thumbView->pageUpIndex(), modifiers);
}

void Selection::nextPick(Qt::KeyboardModifiers modifiers)
{
    if (G::isLogger || isDebug) G::log("Selection::nextPick");
    select(dm->nextPick(), modifiers);
}

void Selection::prevPick(Qt::KeyboardModifiers modifiers)
{
    if (G::isLogger || isDebug) G::log("Selection::prevPick");
    select(dm->prevPick(), modifiers);
}

void Selection::all()
{
    if (G::isLogger || isDebug) G::log("Selection::all");
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
    if (G::isLogger || isDebug) G::log("Selection::random");
    setCurrentRow(QRandomGenerator::global()->generate() % static_cast<int>(dm->sf->rowCount()));
}

QModelIndex Selection::nearestSelectedIndex(int sfRow)
{
    if (G::isLogger || isDebug) G::log("Selection::nearestSelectedIndex", "row =" + QString::number(sfRow));
    int frwd = sfRow + 1;
    int back = sfRow - 1;
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
    if (G::isLogger || isDebug) G::log("Selection::invert");
    QItemSelection toggleSelection;
    QModelIndex firstIndex = dm->sf->index(0, 0);
    QModelIndex lastIndex = dm->sf->index(dm->sf->rowCount() - 1, 0);
    toggleSelection.select(firstIndex, lastIndex);
    sm->select(toggleSelection, QItemSelectionModel::Toggle);
    QModelIndex idx = nearestSelectedIndex(dm->currentSfRow);
    // MW::fileSelectionChange (updates DataModel current indexes, rows)
    // qDebug() << "Selection::invert" << idx.isValid() << dm->currentSfIdx;
    if (idx.isValid()) emit fileSelectionChange(idx);
    if (sm->selectedRows().isEmpty()) select(dm->currentSfIdx);
}

bool Selection::isSelected(int sfRow)
{
    if (G::isLogger || isDebug) G::log("Selection::isSelectedn");
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
    if (G::isLogger || isDebug) G::log("Selection::chkForDeselection");

    QModelIndex idx = dm->sf->index(sfRow, 0);
    int count = sm->selectedRows().count();
    bool isCurrent = idx == dm->currentSfIdx;

//    /*
    qDebug() << "DataModel::chkForDeselection"
             << "row =" << sfRow
             << "isSelected =" << isSelected(sfRow)
             << "isCurrent =" << isCurrent
             << "count =" << count
                ;
                //*/

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
                emit fileSelectionChange(nearestIdx, QModelIndex(), false);
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
    if (G::isLogger || isDebug) G::log("Selection::save");
    selectedRows = sm->selectedRows();
    dm->currentSfIdx = sm->currentIndex();
}

void Selection::recover()
{
    if (G::isLogger || isDebug) G::log("Selection::recover");
    QItemSelection selection;
    QModelIndex idx;
    sm->clear();
    foreach (idx, selectedRows) {
        selection.select(idx, idx);
    }
    sm->select(selection, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
}

void Selection::selectionChanged(const QItemSelection &selected, const QItemSelection &deselected)
{
    // update number selected on status bar
    emit updateStatus(true, "", "Selection::selectionChanged");
}
