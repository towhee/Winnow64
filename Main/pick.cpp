#include "Main/mainwindow.h"
#include "Datamodel/savedpicks.h"

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
        QString fPath = dm->index(dmRow, G::PathColumn).data(G::KeyRole).toString();
        QString priorPickStatus = dm->index(dmRow, G::PickColumn).data().toString();
        pushPick(fPath, priorPickStatus);
        // set pick status
        QModelIndex pickIdx = dm->index(dmRow, G::PickColumn);
        pickStatus = qvariant_cast<QString>(pickIdx.data(Qt::EditRole));
        if (pickStatus != "Rejected") {
            emit setValDm(dmRow, G::PickColumn, newPickStatus, dm->instance, "MW::togglePickUnlessRejected", Qt::EditRole);
        }
    }
    if (n > 1) pushPick("End multiple select");

    // refresh filtration if any filter pick items (picked, unpicked, rejected) are selected
    bool picksBeingFiltered = filters->isAnyCatItemChecked(filters->picks);
    // update filter counts before filterChange.  runSync when a filterChange will follow,
    // otherwise the worker thread rebuilds the category tree while filterChange un-suspends
    // and re-runs filterAcceptsRow over it - a use-after-free crash.
    buildFilters->updateCategory(BuildFilters::PickEdit, BuildFilters::NoAfterAction,
                                 picksBeingFiltered);
    if (picksBeingFiltered) {
        filterChange("MW::togglePick");
    }
    else {
        /*  LIFT THE PROXY SUSPENSION OURSELVES ON THIS PATH.

            BuildFilters::updateCategory suspends filtering unconditionally, and the only
            things that lift it are MW::filterChange, DataModel::endLoad and
            InfoView::dataChanged. Skipping filterChange therefore left suspendFiltering
            TRUE and SortFilter::filterAcceptsRow accepting every row until something else
            happened to call filterChange -- the exact latch failure MW::filterChange's own
            comment warns about ("filtering that works once and then stops"). It was masked
            because clicking anything in the Filters panel lifts it. */
        dm->sf->suspend(false, "MW::togglePickUnlessRejected: no filter change");
        thumbView->refreshIcons("MW::togglePickUnlessRejected");
        gridView->refreshIcons("MW::togglePickUnlessRejected");
    }

    pickMemSize = Utilities::formatMemory(memoryReqdForPicks());
    updatePickDependentActions();
    updateStatus(true, "", "MW::togglePickUnlessRejected");
    updateClassification();
    updateClassification();
}

void MW::clearPicks()
{
/*
    Unpick every picked image in the datamodel -- the UNFILTERED model, like isAnyPick
    and pickCount, so a pick hidden by the current filter is cleared too. Rejects are left
    alone. The change goes onto the pick history stack as one multiple-select group, so
    "Recover prior pick state" restores the whole set. DataModel::setData writes each
    unpick through to the saved picks (Datamodel/savedpicks.h).

    THE LOADED IMAGES ONLY. A saved pick in a folder that is not loaded stays picked;
    the close dialog's "Clear Picks and Close" is what clears every one.

    Triggered by clearPicksAction (Ingest menu and the Clear picks button under the
    Browse panels), which updatePickDependentActions enables only while something is
    picked.
*/
    if (G::isLogger) G::log("MW::clearPicks");
    int remaining = dm->pickCount();
    if (remaining == 0) return;

    // collect first: setting the pick can change the proxy while we walk
    QList<int> rows;
    const int n = dm->rowCount();
    for (int dmRow = 0; dmRow < n && remaining > 0; ++dmRow) {
        if (dm->index(dmRow, G::PickColumn).data(Qt::EditRole).toString() == "Picked") {
            rows.append(dmRow);
            --remaining;
        }
    }

    if (rows.count() > 1) pushPick("Begin multiple select");
    for (int dmRow : rows) {
        QString fPath = dm->index(dmRow, G::PathColumn).data(G::KeyRole).toString();
        pushPick(fPath, "Picked");
        emit setValDm(dmRow, G::PickColumn, "Unpicked", dm->instance, "MW::clearPicks",
                      Qt::EditRole);
    }
    if (rows.count() > 1) pushPick("End multiple select");

    // same refresh tail as togglePick (see the suspension comment there)
    bool picksBeingFiltered = filters->isAnyCatItemChecked(filters->picks);
    buildFilters->updateCategory(BuildFilters::PickEdit, BuildFilters::NoAfterAction,
                                 picksBeingFiltered);
    if (picksBeingFiltered) {
        filterChange("MW::clearPicks");
    }
    else {
        dm->sf->suspend(false, "MW::clearPicks: no filter change");
        thumbView->refreshIcons("MW::clearPicks");
        gridView->refreshIcons("MW::clearPicks");
    }

    pickMemSize = Utilities::formatMemory(memoryReqdForPicks());
    updatePickDependentActions();
    updateStatus(true, "", "MW::clearPicks");
    updateClassification();
}

void MW::loadSavedPicks()
{
/*
    Start the saved picks (Datamodel/savedpicks.h): read them into memory and write
    through from here on. Once at startup, ahead of the first fill. Skipped under
    QStandardPaths test mode: --selftest and --soaktest expect every row to start
    Unpicked, and must not change the user's picks.

    The QSettings "PickLog" group was the crash-recovery copy of the picks; the saved
    picks replace it, so a leftover one is dropped.
*/
    if (G::isLogger) G::log("MW::loadSavedPicks");
    settings->remove("PickLog");
    if (QStandardPaths::isTestModeEnabled()) return;
    SavedPicks::Open();
}

void MW::restoreSavedPicks()
{
/*
    After a fill: the rows' picks made to agree with the saved picks (DataModel::
    syncSavedPicks), then everything that reads the pick count brought up to date.
    Called from MW::folderChanged.
*/
    if (G::isLogger) G::log("MW::restoreSavedPicks");
    if (!dm->syncSavedPicks()) return;
    thumbView->refreshIcons("MW::restoreSavedPicks");
    gridView->refreshIcons("MW::restoreSavedPicks");
    pickMemSize = Utilities::formatMemory(memoryReqdForPicks());
    updatePickDependentActions();
}

void MW::togglePickMouseOverItem(QModelIndex idx)
{
/*
    This is called from IconView forward or back mouse click. The pick status item the mouse is
    over is toggled, but the selection is not changed.
*/
    if (G::isLogger) G::log("MW::togglePickMouseOverItem");
    qDebug() << "MW::togglePickMouseOverItem" << idx;
    int sfRow = idx.row();
    QModelIndex pickIdx = dm->sf->index(sfRow, G::PickColumn);
    QString pickStatus = qvariant_cast<QString>(pickIdx.data(Qt::EditRole));
    pickStatus == "Unpicked" ? pickStatus = "Picked" : pickStatus = "Unpicked";
    emit setValSf(sfRow, G::PickColumn, pickStatus, dm->instance, "MW::togglePickMouseOverItem", Qt::EditRole);

    updateClassification();
    thumbView->refreshIcons("MW::togglePickMouseOverItem");
    gridView->refreshIcons("MW::togglePickMouseOverItem");

    if (settings->value("playPickAudio").toBool()) {
        qDebug() << "MW::togglePickMouseOverItem pickClick"
                 << pickClick->source() << pickClick->status()
                 << "volume =" << pickClick->volume()
                 << "loopCount =" << pickClick->loopCount()
            ;
        pickClick->play();
    }

    pickMemSize = Utilities::formatMemory(memoryReqdForPicks());
    updatePickDependentActions();
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

    DataModel::setData writes every change through to the saved picks (Datamodel/
    savedpicks.h), so a pick survives a quit, a crash or a power loss.

*/
    if (G::isLogger) G::log("MW::togglePick");
    QModelIndexList selection = dm->selectionModel->selectedRows();
    qint64 n = selection.count();
    if (G::isIngestProbe) IngestProbe::Instance().BeginEdit("pick", int(n));

    // copy selection to list of dm rows (proxy filter changes during iteration when change datamodel)
    QList<int> rows;
    for (int i = 0; i < n; ++i) {
        int dmRow = dm->modelRowFromProxyRow(selection.at(i).row());
        rows.append(dmRow);
    }

    qDebug() << "MW::togglePick selected rows =" << rows;

    QString pickStatus;

    bool foundFalse = false;
    // check if any images are not picked in the selection
    int dmRow;
    foreach (dmRow, rows) {
        QModelIndex pickIdx = dm->index(dmRow, G::PickColumn);
        pickStatus = pickIdx.data(Qt::EditRole).toString();
        foundFalse = (pickStatus == "Unpicked");
        if (foundFalse) break;
    }
    foundFalse ? pickStatus = "Picked" : pickStatus = "Unpicked";

    if (settings->value("playPickAudio").toBool()) {
        /*
        qDebug() << "MW::togglePick pickClick"
                 << pickClick->source() << pickClick->status()
                 << "volume =" << pickClick->volume()
                 << "loopCount =" << pickClick->loopCount()
            ; //*/
        pickClick->play();
    }

    // add multiple selection flag to pick history
    if (n > 1) pushPick("Begin multiple select");

    // set pick status for selection
    foreach (dmRow, rows) {
        // save pick history
        QString fPath = dm->index(dmRow, G::PathColumn).data(G::KeyRole).toString();
        QString priorPickStatus = dm->index(dmRow, G::PickColumn).data().toString();
        pushPick(fPath, priorPickStatus);
        // set pick status
        emit setValDm(dmRow, G::PickColumn, pickStatus, dm->instance, "MW::togglePick",
                      Qt::EditRole);
    }
    if (n > 1) pushPick("End multiple select");
    if (G::isIngestProbe) IngestProbe::Instance().MarkEdit("model");

    // avoid null proxy filter
    // buildFilters->updateZeroCountCheckedItems(filters->picks, G::PickColumn);

    // refresh filtration if any filter pick items (picked, unpicked, rejected) are selected
    bool picksBeingFiltered = filters->isAnyCatItemChecked(filters->picks);
    // update filter counts before filterChange.  runSync when a filterChange will follow,
    // otherwise the worker thread rebuilds the category tree while filterChange un-suspends
    // and re-runs filterAcceptsRow over it - a use-after-free crash.
    buildFilters->updateCategory(BuildFilters::PickEdit, BuildFilters::NoAfterAction,
                                 picksBeingFiltered);
    if (G::isIngestProbe) IngestProbe::Instance().MarkEdit("buildFilters");
    if (picksBeingFiltered) {
        filterChange("MW::togglePick");
        if (G::isIngestProbe) IngestProbe::Instance().MarkEdit("filterChange");
    }
    else {
        /*  LIFT THE PROXY SUSPENSION OURSELVES ON THIS PATH.

            BuildFilters::updateCategory suspends filtering unconditionally, and the only
            things that lift it are MW::filterChange, DataModel::endLoad and
            InfoView::dataChanged. Skipping filterChange therefore left suspendFiltering
            TRUE and SortFilter::filterAcceptsRow accepting every row until something else
            happened to call filterChange -- the exact latch failure MW::filterChange's own
            comment warns about ("filtering that works once and then stops"). It was masked
            because clicking anything in the Filters panel lifts it. */
        dm->sf->suspend(false, "MW::togglePick: no filter change");
        thumbView->refreshIcons("MW::togglePick");
        gridView->refreshIcons("MW::togglePick");
        if (G::isIngestProbe) IngestProbe::Instance().MarkEdit("refreshIcons");
    }

    pickMemSize = Utilities::formatMemory(memoryReqdForPicks());
    updatePickDependentActions();
    updateStatus(true, "", "MW::togglePick");
    updateClassification();
    if (G::isIngestProbe) IngestProbe::Instance().EndEdit();

    // auto advance
    if (G::autoAdvance && !picksBeingFiltered) sel->next();
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

    if (settings->value("playPickAudio").toBool()) {
        qDebug() << "MW::toggleReject pickClick"
                 << pickClick->source() << pickClick->status()
                 << "volume =" << pickClick->volume()
                 << "loopCount =" << pickClick->loopCount()
            ;
        pickClick->play();
    }

    // set pick status for selection
    foreach (idx, idxList) {
        // save pick history
        int sfRow = idx.row();
        QString fPath = dm->sf->index(sfRow, G::PathColumn).data(G::KeyRole).toString();
        QString priorPickStatus = dm->sf->index(sfRow, G::PickColumn).data().toString();
        pushPick(fPath, priorPickStatus);
        // set pick status
        emit setValSf(sfRow, G::PickColumn, pickStatus, dm->instance, "MW::toggleReject", Qt::EditRole);
    }
    if (idxList.length() > 1) pushPick("End multiple select");

    // refresh filtration if any filter pick items (picked, unpicked, rejected) are selected
    bool picksBeingFiltered = filters->isAnyCatItemChecked(filters->picks);
    // update filter counts before filterChange.  runSync when a filterChange will follow,
    // otherwise the worker thread rebuilds the category tree while filterChange un-suspends
    // and re-runs filterAcceptsRow over it - a use-after-free crash.
    buildFilters->updateCategory(BuildFilters::PickEdit, BuildFilters::NoAfterAction,
                                 picksBeingFiltered);
    if (picksBeingFiltered) {
        filterChange("MW::togglePick");
    }
    else {
        /*  LIFT THE PROXY SUSPENSION OURSELVES ON THIS PATH.

            BuildFilters::updateCategory suspends filtering unconditionally, and the only
            things that lift it are MW::filterChange, DataModel::endLoad and
            InfoView::dataChanged. Skipping filterChange therefore left suspendFiltering
            TRUE and SortFilter::filterAcceptsRow accepting every row until something else
            happened to call filterChange -- the exact latch failure MW::filterChange's own
            comment warns about ("filtering that works once and then stops"). It was masked
            because clicking anything in the Filters panel lifts it. */
        dm->sf->suspend(false, "MW::toggleReject: no filter change");
        thumbView->refreshIcons("MW::toggleReject");
        gridView->refreshIcons("MW::toggleReject");
    }

    pickMemSize = Utilities::formatMemory(memoryReqdForPicks());
    updatePickDependentActions();
    updateStatus(true, "", "MW::toggleReject");
    updateClassification();
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
    int dmRow = dm->rowFromKey(fPath);
    if (dmRow == -1) return;

    emit setValDm(dmRow, G::PickColumn, status, dm->instance, "MW::updatePickFromHistory", Qt::EditRole);
    thumbView->refreshIcons("MW::updatePickFromHistory");
    gridView->refreshIcons("MW::updatePickFromHistory");

    pickMemSize = Utilities::formatMemory(memoryReqdForPicks());
    updatePickDependentActions();
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
            idx = dm->sf->index(row, G::ByteSizeColumn);
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
        QModelIndex idx = dm->sf->index(row, G::ByteSizeColumn);
        memTot += idx.data(Qt::EditRole).toULongLong();
    }
    return memTot;
}

