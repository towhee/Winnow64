#include "Main/mainwindow.h"

void MW::saveSelection()
{
/*
    This function saves the current selection. This is required, even though the three views
    (thumbView, gridView and tableViews) share the same selection model, because when a view
    is hidden it loses the current index and selection, which has to be re-established each
    time it is made visible.
*/
    if (G::isLogger) G::log(__FUNCTION__);
    selectedRows = selectionModel->selectedRows();
    currentIdx = selectionModel->currentIndex();
}

void MW::recoverSelection()
{
    if (G::isLogger) G::log(__FUNCTION__);
    QItemSelection selection;
    QModelIndex idx;
    foreach (idx, selectedRows)
        selection.select(idx, idx);
    selectionModel->select(selection, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
}

bool MW::getSelection(QStringList &list)
{
/*
    Adds each image that is selected or picked as a file path to list. If there are picks and
    a selection then a dialog offers the user a choice to use.
*/
    if (G::isLogger) G::log(__FUNCTION__);

    bool usePicks = false;

    // nothing picked or selected
    if (thumbView->isPick() && selectionModel->selectedRows().size() == 0) {
        QMessageBox::information(this,
            "Oops", "There are no picks or selected images to report.    ",
            QMessageBox::Ok);
        return false;
    }

    // picks = selection
    bool picksEqualsSelection = true;
    for (int row = 0; row < dm->sf->rowCount(); row++) {
        bool isPicked = dm->sf->index(row, G::PickColumn).data(Qt::EditRole).toString() == "true";
        bool isSelected = selectionModel->isSelected(dm->sf->index(row, 0));
        if (isPicked != isSelected) {
            picksEqualsSelection = false;
            break;
        }
    }

    if (!picksEqualsSelection) {
        // use picks or selected
        if (thumbView->isPick() && selectionModel->selectedRows().size() > 1) {
            SelectionOrPicksDlg::Option option;
            SelectionOrPicksDlg dlg(option);
            dlg.exec();
            if (option == SelectionOrPicksDlg::Option::Cancel) return false;
            if (option == SelectionOrPicksDlg::Option::Picks) usePicks = true;
        }
        else if (thumbView->isPick()) usePicks = true;
    }

    if (usePicks) {
        for (int row = 0; row < dm->sf->rowCount(); row++) {
            if (dm->sf->index(row, G::PickColumn).data(Qt::EditRole).toString() == "true") {
                QModelIndex idx = dm->sf->index(row, 0);
                list << idx.data(G::PathRole).toString();
            }
        }
    }
    else {
        QModelIndexList idxList = selectionModel->selectedRows();
        for (int i = 0; i < idxList.size(); ++i) {
            int row = idxList.at(i).row();
            QModelIndex idx = dm->sf->index(row, 0);
            list << idx.data(G::PathRole).toString();
        }
    }

    return true;
}

QStringList MW::getSelectionOrPicks()
{
    if (G::isLogger) G::log(__FUNCTION__);

    QStringList picks;

    // build QStringList of picks
    if (thumbView->isPick()) {
        for (int row = 0; row < dm->sf->rowCount(); ++row) {
            QModelIndex pickIdx = dm->sf->index(row, G::PickColumn);
            QModelIndex idx = dm->sf->index(row, 0);
            // only picks
            if (pickIdx.data(Qt::EditRole).toString() == "true") {
                picks << idx.data(G::PathRole).toString();
            }
        }
    }

    // build QStringList of selected images
    else if (selectionModel->selectedRows().size() > 0) {
        QModelIndexList idxList = selectionModel->selectedRows();
        for (int i = 0; i < idxList.size(); ++i) {
            int row = idxList.at(i).row();
            QModelIndex idx = dm->sf->index(row, 0);
            picks << idx.data(G::PathRole).toString();
        }
    }

    return picks;
}

void MW::togglePickUnlessRejected()
{
/*
    If the selection has any images that are not picked then pick them all.
    If the entire selection was already picked then unpick them all.
    If the entire selection is unpicked then pick them all.
    Push the changes onto the pick history stack.
*/
    if (G::isLogger) G::log(__FUNCTION__);
    QModelIndex idx;
    QModelIndexList idxList = selectionModel->selectedRows();
    QString pickStatus;
    QString newPickStatus;

    bool foundFalse = false;
    // check if any images are not picked in the selection
    foreach (idx, idxList) {
        QModelIndex pickIdx = dm->sf->index(idx.row(), G::PickColumn);
        pickStatus = qvariant_cast<QString>(pickIdx.data(Qt::EditRole));
        foundFalse = (pickStatus == "false");
        if (foundFalse) break;
    }
    foundFalse ? newPickStatus = "true" : newPickStatus = "false";

    // add multiple selection flag to pick history
    if (idxList.length() > 1) pushPick("Begin multiple select");

    // set pick status for selection
    foreach (idx, idxList) {
        // save pick history
        QString fPath = dm->sf->index(idx.row(), G::PathColumn).data(G::PathRole).toString();
        QString priorPickStatus = dm->sf->index(idx.row(), G::PickColumn).data().toString();
        pushPick(fPath, priorPickStatus);
        // set pick status
        QModelIndex pickIdx = dm->sf->index(idx.row(), G::PickColumn);
        pickStatus = qvariant_cast<QString>(pickIdx.data(Qt::EditRole));
        if (pickStatus != "reject") {
            emit setValueSf(pickIdx, newPickStatus, Qt::EditRole);
//            dm->sf->setData(pickIdx, newPickStatus, Qt::EditRole);
        }
    }
    if (idxList.length() > 1) pushPick("End multiple select");

    updateClassification();
    thumbView->refreshThumbs();
    gridView->refreshThumbs();

    pickMemSize = Utilities::formatMemory(memoryReqdForPicks());
    updateStatus(true, "", __FUNCTION__);

    // update filter counts
    buildFilters->updateCountFiltered();
}

void MW::togglePickMouseOver()
{
/*
    Toggles the pick status item the mouse is over is toggled, but the selection is not
    changed.

    Triggered by ThumbView context menu MW::pickMouseOverAction.  ThumbView mousePressEvent
    stores the mouse click indexAt(position).  Use this to call togglePickMouseOverItem.
*/
    if (G::isLogger) G::log(__FUNCTION__);
    togglePickMouseOverItem(thumbView->mouseOverIndex);
}

void MW::togglePickMouseOverItem(QModelIndex idx)
{
/*
    This is called from IconView forward or back mouse click. The pick status item the mouse is
    over is toggled, but the selection is not changed.
*/
    if (G::isLogger) G::log(__FUNCTION__);
    QModelIndex pickIdx = dm->sf->index(idx.row(), G::PickColumn);
    QString pickStatus = qvariant_cast<QString>(pickIdx.data(Qt::EditRole));
    pickStatus == "false" ? pickStatus = "true" : pickStatus = "false";
    emit setValueSf(pickIdx, pickStatus, Qt::EditRole);
//    dm->sf->setData(pickIdx, pickStatus, Qt::EditRole);

    updateClassification();
    thumbView->refreshThumbs();
    gridView->refreshThumbs();

    pickMemSize = Utilities::formatMemory(memoryReqdForPicks());
    updateStatus(true, "", __FUNCTION__);

    // update filter counts
    buildFilters->updateCountFiltered();
}

void MW::togglePick()
{
/*
    If the selection has any images that are not picked then pick them all.
    If the entire selection was already picked then unpick them all.
    If the entire selection is unpicked then pick them all.
    Push the changes onto the pick history stack.
*/
    if (G::isLogger) G::log(__FUNCTION__);
    QModelIndex idx;
    QModelIndexList idxList = selectionModel->selectedRows();
    QString pickStatus;

    bool foundFalse = false;
    // check if any images are not picked in the selection
    foreach (idx, idxList) {
        QModelIndex pickIdx = dm->sf->index(idx.row(), G::PickColumn);
        pickStatus = qvariant_cast<QString>(pickIdx.data(Qt::EditRole));
        foundFalse = (pickStatus != "true");
        if (foundFalse) break;
    }
    foundFalse ? pickStatus = "true" : pickStatus = "false";

    // add multiple selection flag to pick history
    if (idxList.length() > 1) pushPick("Begin multiple select");

    // set pick status for selection
    foreach (idx, idxList) {
        // save pick history
        QString fPath = dm->sf->index(idx.row(), G::PathColumn).data(G::PathRole).toString();
        QString priorPickStatus = dm->sf->index(idx.row(), G::PickColumn).data().toString();
        pushPick(fPath, priorPickStatus);
        // set pick status
        QModelIndex pickIdx = dm->sf->index(idx.row(), G::PickColumn);
        emit setValueSf(pickIdx, pickStatus, Qt::EditRole);
//        dm->sf->setData(pickIdx, pickStatus, Qt::EditRole);
        updatePickLog(fPath, pickStatus);
    }
    if (idxList.length() > 1) pushPick("End multiple select");

    updateClassification();
    thumbView->refreshThumbs();
    gridView->refreshThumbs();

    pickMemSize = Utilities::formatMemory(memoryReqdForPicks());
    updateStatus(true, "", __FUNCTION__);

    // update filter counts
    buildFilters->updateCountFiltered();

    // auto advance
    if (autoAdvance) thumbView->selectNext();
}

int MW::pickLogCount()
{
    if (G::isLogger) G::log(__FUNCTION__);
    setting->beginGroup("PickLog");
    int count = setting->allKeys().size();
    setting->endGroup();
    return count;
}

void MW::recoverPickLog()
{
    if (G::isLogger) G::log(__FUNCTION__);
    setting->beginGroup("PickLog");
    QStringList keys = setting->allKeys();
    for (int i = 0; i < keys.length(); ++i) {
        QString fPath = keys.at(i);
        fPath.replace("🔸", "/");
        QString pickStatus = setting->value(keys.at(i)).toString();
        QModelIndex idx = dm->proxyIndexFromPath(fPath);
        if (idx.isValid()) {
            QModelIndex pickIdx = dm->sf->index(idx.row(), G::PickColumn);
            emit setValueSf(pickIdx, pickStatus, Qt::EditRole);
//            dm->sf->setData(pickIdx, pickStatus, Qt::EditRole);
            qDebug() << __FUNCTION__ << pickStatus << fPath << "updated";
        }
        else {
            qDebug() << __FUNCTION__ << fPath << "not found";
        }
    }
    setting->endGroup();
    thumbView->refreshThumbs();
    gridView->refreshThumbs();
}

void MW::clearPickLog()
{
    if (G::isLogger) G::log(__FUNCTION__);
    setting->beginGroup("PickLog");
    QStringList keys = setting->allKeys();
    for (int i = 0; i < keys.length(); ++i) {
        setting->remove(keys.at(i));
    }
    setting->endGroup();
}

void MW::updatePickLog(QString fPath, QString pickStatus)
{
    if (G::isLogger) G::log(__FUNCTION__);
    setting->beginGroup("PickLog");
    QString sKey = fPath;
    sKey.replace("/", "🔸");
    if (pickStatus == "true") {
//        qDebug() << __FUNCTION__ << "adding" << sKey;
        setting->setValue(sKey, pickStatus);
    }
    else {
//        qDebug() << __FUNCTION__ << "removing" << sKey;
        setting->remove(sKey);
    }
    setting->endGroup();
}

void MW::pushPick(QString fPath, QString status)
{
/*
    Adds a pick action (either to pick or unpick) to the pickStack history. This is used to
    recover a prior pick history state if the picks have been lost due to an accidental
    erasure.
*/
    if (G::isLogger) G::log(__FUNCTION__);
    pick.path = fPath;
    pick.status = status;
    pickStack->push(pick);
}

void MW::popPick()
{
    if (G::isLogger) G::log(__FUNCTION__);
    if (pickStack->isEmpty()) return;
    pick = pickStack->pop();
    if (pick.path != "End multiple select") {
        updatePickFromHistory(pick.path, pick.status);
    }
    else {
        while (!pickStack->isEmpty()) {
            pick = pickStack->pop();
            if (pick.path == "Begin multiple select") break;
            updatePickFromHistory(pick.path, pick.status);
        }
    }
}

void MW::updatePickFromHistory(QString fPath, QString status)
{
    if (G::isLogger) G::log(__FUNCTION__);
    if (dm->fPathRow.contains(fPath)) {
        int row = dm->fPathRow[fPath];
        QModelIndex pickIdx = dm->sf->index(row, G::PickColumn);
        emit setValueSf(pickIdx, status, Qt::EditRole);
//        dm->sf->setData(pickIdx, status, Qt::EditRole);
//        dm->sf->filterChange();
        thumbView->refreshThumbs();
        gridView->refreshThumbs();

        pickMemSize = Utilities::formatMemory(memoryReqdForPicks());
        updateStatus(true, "", __FUNCTION__);

        // update filter counts
        buildFilters->updateCountFiltered();
    }
}

qulonglong MW::memoryReqdForPicks()
{
    if (G::isLogger) G::log(__FUNCTION__);
    qulonglong memTot = 0;
    for(int row = 0; row < dm->sf->rowCount(); row++) {
        QModelIndex idx = dm->sf->index(row, G::PickColumn);
        if(qvariant_cast<QString>(idx.data(Qt::EditRole)) == "true") {
            idx = dm->sf->index(row, G::SizeColumn);
            memTot += idx.data(Qt::EditRole).toULongLong();
        }
    }
    return memTot;
}

qulonglong MW::memoryReqdForSelection()
{
    if (G::isLogger) G::log(__FUNCTION__);
    qulonglong memTot = 0;
    QModelIndexList selection = selectionModel->selectedRows();
    for(int row = 0; row < selection.count(); row++) {
        QModelIndex idx = dm->sf->index(row, G::SizeColumn);
        memTot += idx.data(Qt::EditRole).toULongLong();
    }
    return memTot;
}

