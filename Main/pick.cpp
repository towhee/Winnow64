#include "Main/mainwindow.h"

void MW::togglePickUnlessRejected()
{
/*
    If the selection has any images that are not picked then pick them all.
    If the entire selection was already picked then unpick them all.
    If the entire selection is unpicked then pick them all.
    Push the changes onto the pick history stack.
*/
    if (G::isLogger) G::log("MW::togglePickUnlessRejected");
    QModelIndex idx;
    QModelIndexList idxList = dm->selectionModel->selectedRows();
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
    foundFalse ? newPickStatus = "Picked" : newPickStatus = "Unpicked";

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
        if (pickStatus != "Rejected") {
            emit setValueSf(pickIdx, newPickStatus, dm->instance, "MW::togglePickUnlessRejected", Qt::EditRole);
        }
    }
    if (idxList.length() > 1) pushPick("End multiple select");

    updateClassification();
    thumbView->refreshThumbs();
    gridView->refreshThumbs();

    pickMemSize = Utilities::formatMemory(memoryReqdForPicks());
    updateStatus(true, "", "MW::togglePickUnlessRejected");

    // update filter counts
    buildFilters->build(BuildFilters::Action::PickEdit);
}

void MW::togglePickMouseOver()
{
/*
    Toggles the pick status item the mouse is over is toggled, but the selection is not
    changed.

    Triggered by ThumbView context menu MW::pickMouseOverAction.  ThumbView mousePressEvent
    stores the mouse click indexAt(position).  Use this to call togglePickMouseOverItem.
*/
    if (G::isLogger) G::log("MW::togglePickMouseOver");
    togglePickMouseOverItem(thumbView->mouseOverIndex);
}

void MW::togglePickMouseOverItem(QModelIndex idx)
{
/*
    This is called from IconView forward or back mouse click. The pick status item the mouse is
    over is toggled, but the selection is not changed.
*/
    if (G::isLogger) G::log("MW::togglePickMouseOverItem");
    QModelIndex pickIdx = dm->sf->index(idx.row(), G::PickColumn);
    QString pickStatus = qvariant_cast<QString>(pickIdx.data(Qt::EditRole));
    pickStatus == "false" ? pickStatus = "Picked" : pickStatus = "Unpicked";
    emit setValueSf(pickIdx, pickStatus, dm->instance, "MW::togglePickMouseOverItem", Qt::EditRole);

    updateClassification();
    thumbView->refreshThumbs();
    gridView->refreshThumbs();

    pickMemSize = Utilities::formatMemory(memoryReqdForPicks());
    updateStatus(true, "", "MW::togglePickMouseOverItem");

    // update filter counts
    buildFilters->build(BuildFilters::Action::PickEdit);
}

void MW::togglePick()
{
/*
    If the selection has any images that are not picked then pick them all.
    If the entire selection was already picked then unpick them all.
    If the entire selection is unpicked then pick them all.
    Push the changes onto the pick history stack.

    The pick status is a string in dm->sf->index(idx.row(), G::PickColumn)
        - "Unpicked"
        - "Picked"
        - "Rejected"

    The settings pickLog (for crash recovery) values:
        - "picked"      datamodel pick = "picked"
        - "ingested"    datamodel pick = "false" and datamodel ingested = true

*/
    if (G::isLogger) G::log("MW::togglePick");
    QModelIndex idx;
    QModelIndexList idxList = dm->selectionModel->selectedRows();
    QString pickStatus;

    bool foundFalse = false;
    // check if any images are not picked in the selection
    foreach (idx, idxList) {
        QModelIndex pickIdx = dm->sf->index(idx.row(), G::PickColumn);
        pickStatus = qvariant_cast<QString>(pickIdx.data(Qt::EditRole));
        foundFalse = (pickStatus == "Unpicked");
        if (foundFalse) break;
    }
    foundFalse ? pickStatus = "Picked" : pickStatus = "Unpicked";
//    foundFalse ? pickStatus = "true" : pickStatus = "false";

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
        emit setValueSf(pickIdx, pickStatus, dm->instance, "MW::togglePick", Qt::EditRole);
        updatePickLog(fPath, pickStatus);
    }
    if (idxList.length() > 1) pushPick("End multiple select");

    updateClassification();
    thumbView->refreshThumbs();
    gridView->refreshThumbs();

    pickMemSize = Utilities::formatMemory(memoryReqdForPicks());
    updateStatus(true, "", "MW::togglePick");

    // update filter counts
    buildFilters->build(BuildFilters::Action::PickEdit);

    // auto advance
    if (autoAdvance) sel->next();
}

int MW::pickLogCount()
{
    if (G::isLogger) G::log("MW::pickLogCount");
    setting->beginGroup("PickLog");
    int count = setting->allKeys().size();
    setting->endGroup();
    return count;
}

void MW::recoverPickLog()
{
    if (G::isLogger) G::log("MW::recoverPickLog");
    setting->beginGroup("PickLog");
    QStringList keys = setting->allKeys();
    for (int i = 0; i < keys.length(); ++i) {
        QString fPath = keys.at(i);
        fPath.replace("🔸", "/");
        QString pickStatus = setting->value(keys.at(i)).toString();
        QModelIndex idx = dm->proxyIndexFromPath(fPath);
        if (idx.isValid()) {
            if (pickStatus == "Picked") {
                QModelIndex idxPick = dm->sf->index(idx.row(), G::PickColumn);
                emit setValueSf(idxPick, "Picked", dm->instance, "MW::recoverPickLog", Qt::EditRole);
            }
            if (pickStatus == "ingested") {
                QModelIndex idxIngest = dm->sf->index(idx.row(), G::IngestedColumn);
                emit setValueSf(idxIngest, true, dm->instance, "MW::recoverPickLog", Qt::EditRole);
            }
//            qDebug() << "MW::recoverPickLog" << pickStatus << fPath << "updated";
        }
        else {
//            qDebug() << "MW::recoverPickLog" << fPath << "not found";
        }
    }
    setting->endGroup();
    thumbView->refreshThumbs();
    gridView->refreshThumbs();
}

void MW::clearPickLog()
{
    if (G::isLogger) G::log("MW::clearPickLog");
    setting->beginGroup("PickLog");
    QStringList keys = setting->allKeys();
    for (int i = 0; i < keys.length(); ++i) {
        setting->remove(keys.at(i));
    }
    setting->endGroup();
}

void MW::updatePickLog(QString fPath, QString pickStatus)
/*
    Called from MW::togglePick
*/
{
    if (G::isLogger) G::log("MW::updatePickLog");
    setting->beginGroup("PickLog");
    QString sKey = fPath;
    sKey.replace("/", "🔸");
    if (pickStatus == "Picked") {
//        qDebug() << "MW::updatePickLog" << "adding" << sKey;
        setting->setValue(sKey, pickStatus);
    }
    else {
//        qDebug() << "MW::updatePickLog" << "removing" << sKey;
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
    if (G::isLogger) G::log("MW::pushPick");
    pick.path = fPath;
    pick.status = status;
    pickStack->push(pick);
}

void MW::popPick()
{
    if (G::isLogger) G::log("MW::popPick");
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
    if (G::isLogger) G::log("MW::updatePickFromHistory");
    int dmRow = dm->rowFromPath(fPath);
    if (dmRow == -1) return;

    QModelIndex pickIdx = dm->sf->index(dmRow, G::PickColumn);
    emit setValueSf(pickIdx, status, dm->instance, "MW::updatePickFromHistory", Qt::EditRole);
    thumbView->refreshThumbs();
    gridView->refreshThumbs();

    pickMemSize = Utilities::formatMemory(memoryReqdForPicks());
    updateStatus(true, "", "MW::updatePickFromHistory");

    // update filter counts
    buildFilters->build(BuildFilters::Action::PickEdit);
}

qulonglong MW::memoryReqdForPicks()
{
    if (G::isLogger) G::log("MW::memoryReqdForPicks");
    qulonglong memTot = 0;
    for(int row = 0; row < dm->sf->rowCount(); row++) {
        QModelIndex idx = dm->sf->index(row, G::PickColumn);
        if(qvariant_cast<QString>(idx.data(Qt::EditRole)) == "Picked") {
            idx = dm->sf->index(row, G::SizeColumn);
            memTot += idx.data(Qt::EditRole).toULongLong();
        }
    }
    return memTot;
}

qulonglong MW::memoryReqdForSelection()
{
    if (G::isLogger) G::log("MW::memoryReqdForSelection");
    qulonglong memTot = 0;
    QModelIndexList selection = dm->selectionModel->selectedRows();
    for(int row = 0; row < selection.count(); row++) {
        QModelIndex idx = dm->sf->index(row, G::SizeColumn);
        memTot += idx.data(Qt::EditRole).toULongLong();
    }
    return memTot;
}

