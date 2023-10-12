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
    QModelIndexList selection = dm->selectionModel->selectedRows();
    qint64 n = selection.count();

    // copy selection to list of dm rows (proxy filter changes during iteration when change datamodel)
    QList<int> rows;
    for (int i = 0; i < n; ++i) {
        int dmRow = dm->modelRowFromProxyRow(selection.at(i).row());
        rows.append(dmRow);
    }

    QString pickStatus;
    QString newPickStatus;

    bool foundFalse = false;
    // check if any images are not picked in the selection
    int dmRow;
    foreach (dmRow, rows) {
        QModelIndex pickIdx = dm->index(dmRow, G::PickColumn);
        pickStatus = qvariant_cast<QString>(pickIdx.data(Qt::EditRole));
        foundFalse = (pickStatus == "Unpicked");
        if (foundFalse) break;
    }
    foundFalse ? newPickStatus = "Picked" : newPickStatus = "Unpicked";

    // add multiple selection flag to pick history
    if (n > 1) pushPick("Begin multiple select");

    // set pick status for selection
    foreach (dmRow, rows) {
        // save pick history
        QString fPath = dm->index(dmRow, G::PathColumn).data(G::PathRole).toString();
        QString priorPickStatus = dm->index(dmRow, G::PickColumn).data().toString();
        pushPick(fPath, priorPickStatus);
        // set pick status
        QModelIndex pickIdx = dm->index(dmRow, G::PickColumn);
        pickStatus = qvariant_cast<QString>(pickIdx.data(Qt::EditRole));
        if (pickStatus != "Rejected") {
            emit setValue(pickIdx, newPickStatus, dm->instance, "MW::togglePickUnlessRejected", Qt::EditRole);
        }
    }
    if (n > 1) pushPick("End multiple select");

    updateClassification();
    thumbView->refreshThumbs();
    gridView->refreshThumbs();

    pickMemSize = Utilities::formatMemory(memoryReqdForPicks());
    updateStatus(true, "", "MW::togglePickUnlessRejected");

    // update filter counts
    buildFilters->updateCategory(BuildFilters::PickEdit);
}

void MW::togglePickMouseOverItem(QModelIndex idx)
{
/*
    This is called from IconView forward or back mouse click. The pick status item the mouse is
    over is toggled, but the selection is not changed.
*/
    if (G::isLogger) G::log("MW::togglePickMouseOverItem");
    qDebug() << "MW::togglePickMouseOverItem" << idx;
    QModelIndex pickIdx = dm->sf->index(idx.row(), G::PickColumn);
    QString pickStatus = qvariant_cast<QString>(pickIdx.data(Qt::EditRole));
    pickStatus == "Unpicked" ? pickStatus = "Picked" : pickStatus = "Unpicked";
    emit setValueSf(pickIdx, pickStatus, dm->instance, "MW::togglePickMouseOverItem", Qt::EditRole);

    updateClassification();
    thumbView->refreshThumbs();
    gridView->refreshThumbs();

    pickMemSize = Utilities::formatMemory(memoryReqdForPicks());
    updateStatus(true, "", "MW::togglePickMouseOverItem");

    // update filter counts
    buildFilters->updateCategory(BuildFilters::PickEdit);
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
    QModelIndexList selection = dm->selectionModel->selectedRows();
    qint64 n = selection.count();

    // copy selection to list of dm rows (proxy filter changes during iteration when change datamodel)
    QList<int> rows;
    for (int i = 0; i < n; ++i) {
        int dmRow = dm->modelRowFromProxyRow(selection.at(i).row());
        rows.append(dmRow);
    }

    QString pickStatus;

    bool foundFalse = false;
    // check if any images are not picked in the selection
    int dmRow;
    foreach (dmRow, rows) {
        QModelIndex pickIdx = dm->index(dmRow, G::PickColumn);
        pickStatus = qvariant_cast<QString>(pickIdx.data(Qt::EditRole));
        foundFalse = (pickStatus == "Unpicked");
        if (foundFalse) break;
    }
    foundFalse ? pickStatus = "Picked" : pickStatus = "Unpicked";

    // add multiple selection flag to pick history
    if (n > 1) pushPick("Begin multiple select");

    // set pick status for selection
    foreach (dmRow, rows) {
        // save pick history
        QString fPath = dm->index(dmRow, G::PathColumn).data(G::PathRole).toString();
        QString priorPickStatus = dm->index(dmRow, G::PickColumn).data().toString();
        pushPick(fPath, priorPickStatus);
        // set pick status
        QModelIndex pickIdx = dm->index(dmRow, G::PickColumn);
        emit setValue(pickIdx, pickStatus, dm->instance, "MW::togglePick", Qt::EditRole,
                      Qt::AlignCenter);
        updatePickLog(fPath, pickStatus);
    }
    if (n > 1) pushPick("End multiple select");

    updateClassification();
    thumbView->refreshThumbs();
    gridView->refreshThumbs();

    pickMemSize = Utilities::formatMemory(memoryReqdForPicks());
    updateStatus(true, "", "MW::togglePick");

    // update filter counts
    buildFilters->updateCategory(BuildFilters::PickEdit);

    // auto advance
    if (autoAdvance) sel->next();
}

void MW::toggleReject()
{
/*
    If the selection has any images that are not rejected then reject them all.
    If the entire selection was already rejected then unreject them all.
    If the entire selection is nor rejected then reject them all.
*/
    if (G::isLogger) G::log("MW::toggleReject");
    QModelIndex idx;
    QModelIndexList idxList = dm->selectionModel->selectedRows();
    QString pickStatus;

    // add multiple selection flag to pick history
    if (idxList.length() > 1) pushPick("Begin multiple select");

    bool foundFalse = false;
    // check if any images are not rejected in the selection
    foreach (idx, idxList) {
        QModelIndex pickIdx = dm->sf->index(idx.row(), G::PickColumn);
        pickStatus = qvariant_cast<QString>(pickIdx.data(Qt::EditRole));
        foundFalse = (pickStatus != "Rejected");
        if (foundFalse) break;
    }
    foundFalse ? pickStatus = "Rejected" : pickStatus = "Unpicked";

    // set pick status for selection
    foreach (idx, idxList) {
        // save pick history
        QString fPath = dm->sf->index(idx.row(), G::PathColumn).data(G::PathRole).toString();
        QString priorPickStatus = dm->sf->index(idx.row(), G::PickColumn).data().toString();
        pushPick(fPath, priorPickStatus);
        // set pick status
        QModelIndex pickIdx = dm->sf->index(idx.row(), G::PickColumn);
        emit setValueSf(pickIdx, pickStatus, dm->instance, "MW::toggleReject", Qt::EditRole);
    }
    if (idxList.length() > 1) pushPick("End multiple select");

    updateClassification();
    thumbView->refreshThumbs();
    gridView->refreshThumbs();

    pickMemSize = Utilities::formatMemory(memoryReqdForPicks());
    updateStatus(true, "", "MW::toggleReject");

    // update filter counts
    buildFilters->updateCategory(BuildFilters::PickEdit);

    // auto advance
    if (autoAdvance) sel->next();
}

int MW::pickLogCount()
{
    if (G::isLogger) G::log("MW::pickLogCount");
    settings->beginGroup("PickLog");
    int count = settings->allKeys().size();
    settings->endGroup();
    return count;
}

void MW::recoverPickLog()
{
    if (G::isLogger) G::log("MW::recoverPickLog");
    settings->beginGroup("PickLog");
    QStringList keys = settings->allKeys();
    for (int i = 0; i < keys.length(); ++i) {
        QString fPath = keys.at(i);
        fPath.replace("🔸", "/");
                            QString pickStatus = settings->value(keys.at(i)).toString();
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
    settings->endGroup();
    thumbView->refreshThumbs();
    gridView->refreshThumbs();
}

void MW::clearPickLog()
{
    if (G::isLogger) G::log("MW::clearPickLog");
    settings->beginGroup("PickLog");
    QStringList keys = settings->allKeys();
    for (int i = 0; i < keys.length(); ++i) {
        settings->remove(keys.at(i));
    }
    settings->endGroup();
}

void MW::updatePickLog(QString fPath, QString pickStatus)
/*
    Called from MW::togglePick
*/
{
    if (G::isLogger) G::log("MW::updatePickLog");
    settings->beginGroup("PickLog");
    QString sKey = fPath;
    sKey.replace("/", "🔸");
    if (pickStatus == "Picked") {
//        qDebug() << "MW::updatePickLog" << "adding" << sKey;
        settings->setValue(sKey, pickStatus);
    }
    else {
//        qDebug() << "MW::updatePickLog" << "removing" << sKey;
        settings->remove(sKey);
    }
    settings->endGroup();
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
    buildFilters->updateCategory(BuildFilters::PickEdit);
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

