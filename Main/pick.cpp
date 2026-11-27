#include "Main/mainwindow.h"
#include "Datamodel/savedpicks.h"
#include "Undo/undocommands.h"

void MW::togglePickUnlessRejected()
{
/*
    If the selection has any images that are not picked then pick them all.
    If the entire selection was already picked then unpick them all.
    If the entire selection is unpicked then pick them all.
    The change is one Edit > Undo step (recordPickUndo).
*/
    if (G::isLogger) G::log("MW::togglePickUnlessRejected");
    QModelIndexList selection = RowSelection::Rows(dm->selectionModel);
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

    // set pick status for selection, recording each change for Edit > Undo
    PathValues before, after;
    foreach (dmRow, rows) {
        QString fPath = dm->index(dmRow, G::PathColumn).data(G::KeyRole).toString();
        QModelIndex pickIdx = dm->index(dmRow, G::PickColumn);
        pickStatus = qvariant_cast<QString>(pickIdx.data(Qt::EditRole));
        if (pickStatus != "Rejected") {
            before.append({fPath, pickStatus});
            after.append({fPath, newPickStatus});
            emit setValDm(dmRow, G::PickColumn, newPickStatus, dm->instance, "MW::togglePickUnlessRejected", Qt::EditRole);
        }
    }
    recordPickUndo(newPickStatus == "Picked" ? "Pick" : "Unpick", before, after);

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
    alone. The change is one Edit > Undo step, which restores the whole set.
    DataModel::setData writes each
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

    PathValues before, after;
    for (int dmRow : rows) {
        QString fPath = dm->index(dmRow, G::PathColumn).data(G::KeyRole).toString();
        before.append({fPath, QString("Picked")});
        after.append({fPath, QString("Unpicked")});
        emit setValDm(dmRow, G::PickColumn, "Unpicked", dm->instance, "MW::clearPicks",
                      Qt::EditRole);
    }
    recordPickUndo("Clear Picks", before, after);

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

void MW::rejectUnpickedClearPicks()
{
/*
    Lightroom's Library > Refine Photos (Cmd/Ctrl + Opt/Alt + R): every Unpicked image
    becomes Rejected and every Picked image becomes Unpicked, ready for the next round
    of picking. Rejected and Ingested images are left alone.

    THE FILTERED DATASET ONLY (dm->sf), unlike clearPicks: images hidden by the current
    filter keep their pick status. The change is one Edit > Undo step, which restores
    the whole set.

    Triggered by rejectUnpickedClearPicksAction (Ingest menu and the thumbnail context
    menu), which updatePickDependentActions enables only while a VISIBLE image is
    picked -- otherwise it would reject every visible image. The same test is repeated
    here in case the action's state is stale (the shortcut fires on it directly).

    REFUSED WHEN A PICK IS HIDDEN. If any picked image is outside the filtered view,
    refining would leave it Picked while its visible companions are cleared, mixing two
    rounds of picking. warnHiddenPicks explains and nothing changes.

    CONFIRMED FIRST. One keystroke can reject a whole folder, so okToRejectUnpicked shows
    the counts and nothing changes on Cancel.
*/
    if (G::isLogger) G::log("MW::rejectUnpickedClearPicks");
    if (!dm->isAnyPick()) return;

    // collect first: setting the pick can change the proxy while we walk
    QList<int> rows;
    QStringList priors;
    bool isVisiblePick = false;
    const int n = dm->sf->rowCount();
    for (int sfRow = 0; sfRow < n; ++sfRow) {
        const int dmRow = dm->modelRowFromProxyRow(sfRow);
        if (dmRow < 0) continue;
        QString status = dm->index(dmRow, G::PickColumn).data(Qt::EditRole).toString();
        if (status == "Picked" || status == "Unpicked") {
            rows.append(dmRow);
            priors.append(status);
            if (status == "Picked") isVisiblePick = true;
        }
    }
    if (!isVisiblePick) return;

    const int hiddenPicks = dm->pickCount() - priors.count("Picked");
    if (hiddenPicks > 0) {
        warnHiddenPicks(hiddenPicks);
        return;
    }

    const int toReject = priors.count("Unpicked");
    if (!okToRejectUnpicked(toReject, priors.count() - toReject)) return;

    PathValues before, after;
    for (int i = 0; i < rows.count(); ++i) {
        const int dmRow = rows.at(i);
        QString fPath = dm->index(dmRow, G::PathColumn).data(G::KeyRole).toString();
        QString newStatus = priors.at(i) == "Picked" ? "Unpicked" : "Rejected";
        before.append({fPath, priors.at(i)});
        after.append({fPath, newStatus});
        emit setValDm(dmRow, G::PickColumn, newStatus, dm->instance,
                      "MW::rejectUnpickedClearPicks", Qt::EditRole);
    }
    recordPickUndo("Reject Unpicked", before, after);

    // same refresh tail as togglePick (see the suspension comment there)
    bool picksBeingFiltered = filters->isAnyCatItemChecked(filters->picks);
    buildFilters->updateCategory(BuildFilters::PickEdit, BuildFilters::NoAfterAction,
                                 picksBeingFiltered);
    if (picksBeingFiltered) {
        filterChange("MW::rejectUnpickedClearPicks");
    }
    else {
        dm->sf->suspend(false, "MW::rejectUnpickedClearPicks: no filter change");
        thumbView->refreshIcons("MW::rejectUnpickedClearPicks");
        gridView->refreshIcons("MW::rejectUnpickedClearPicks");
    }

    pickMemSize = Utilities::formatMemory(memoryReqdForPicks());
    updatePickDependentActions();
    updateStatus(true, "", "MW::rejectUnpickedClearPicks");
    updateClassification();
}

void MW::warnHiddenPicks(int hiddenPicks)
{
/*
    Why rejectUnpickedClearPicks did nothing: picked images are hidden by the current
    filter. Test mode skips the dialog (nobody to answer); the refine is still refused.
*/
    if (G::isLogger) G::log("MW::warnHiddenPicks");
    if (QStandardPaths::isTestModeEnabled()) return;

    QMessageBox msgBox(this);
    msgBox.setWindowTitle("Reject Unpicked, Clear Picks");
    msgBox.setIcon(QMessageBox::Warning);
    msgBox.setText(hiddenPicks == 1
                   ? QString("1 picked image is not in the filtered view.")
                   : QString("%1 picked images are not in the filtered view.")
                         .arg(hiddenPicks));
    msgBox.setInformativeText("Nothing was changed. Refine works on the filtered view "
                              "only, so it would leave the hidden picks picked.\n\n"
                              "Clear the filters, or unpick the hidden images, and try "
                              "again.");
    msgBox.setStandardButtons(QMessageBox::Ok);
    msgBox.setStyleSheet(G::css);
    QSpacerItem *horizontalSpacer = new QSpacerItem(350, 0, QSizePolicy::Minimum,
                                                    QSizePolicy::Expanding);
    QGridLayout *layout = static_cast<QGridLayout*>(msgBox.layout());
    layout->addItem(horizontalSpacer, layout->rowCount(), 0, 1, layout->columnCount());
    // gap between the informative text and the buttons
    if (QDialogButtonBox *buttonBox = msgBox.findChild<QDialogButtonBox*>())
        buttonBox->setContentsMargins(0, 16, 0, 0);
    msgBox.exec();
    resetFocus();
}

bool MW::okToRejectUnpicked(int toReject, int toUnpick)
{
/*
    Confirmation for rejectUnpickedClearPicks, before anything is changed: how many
    images will be rejected and how many picks cleared. False = Cancel (also the escape
    button). Reject is the default, so Enter confirms. Test mode skips it: --selftest,
    --soaktest and --devtest have nobody to answer.
*/
    if (G::isLogger) G::log("MW::okToRejectUnpicked");
    if (QStandardPaths::isTestModeEnabled()) return true;

    auto images = [](int n) {
        return n == 1 ? QString("1 image") : QString("%1 images").arg(n);
    };

    QMessageBox msgBox(this);
    const int msgBoxWidth = 350;
    msgBox.setWindowTitle("Reject Unpicked, Clear Picks");
    msgBox.setIcon(QMessageBox::Warning);
    msgBox.setText(toReject == 0
                   ? QString("No unpicked images will be rejected.")
                   : QString("%1 will be rejected.").arg(images(toReject)));
    QString info = toUnpick == 1
                   ? QString("1 picked image will be unpicked.")
                   : QString("%1 picked images will be unpicked.").arg(toUnpick);
    info += "\n\nOnly images in the filtered view are changed. "
            "Undo with Edit > Undo.";
    msgBox.setInformativeText(info);
    QPushButton *rejectBtn = msgBox.addButton(toReject == 0 ? QString("Clear Picks")
                                                            : QString("Reject"),
                                              QMessageBox::DestructiveRole);
    QPushButton *cancelBtn = msgBox.addButton(QMessageBox::Cancel);
    msgBox.setDefaultButton(rejectBtn);
    msgBox.setEscapeButton(cancelBtn);
    msgBox.setStyleSheet(G::css);
    // pad the width and widen the buttons to their text, as okToCloseWithPicks does
    QSpacerItem *horizontalSpacer = new QSpacerItem(msgBoxWidth, 0, QSizePolicy::Minimum,
                                                    QSizePolicy::Expanding);
    QGridLayout *layout = static_cast<QGridLayout*>(msgBox.layout());
    layout->addItem(horizontalSpacer, layout->rowCount(), 0, 1, layout->columnCount());
    for (QAbstractButton *b : msgBox.buttons()) {
        QString plain = b->text();
        plain.remove(QChar('&'));
        const int textWidth = b->fontMetrics().horizontalAdvance(plain);
        b->setMinimumWidth(qMax(b->sizeHint().width(), textWidth + 40));
    }
    // gap between the informative text and the buttons
    if (QDialogButtonBox *buttonBox = msgBox.findChild<QDialogButtonBox*>())
        buttonBox->setContentsMargins(0, 16, 0, 0);
    msgBox.exec();
    resetFocus();
    return msgBox.clickedButton() == rejectBtn;
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
    const QString priorStatus = pickStatus;
    const QString fPath = dm->sf->index(sfRow, G::PathColumn).data(G::KeyRole).toString();
    pickStatus == "Unpicked" ? pickStatus = "Picked" : pickStatus = "Unpicked";
    emit setValSf(sfRow, G::PickColumn, pickStatus, dm->instance, "MW::togglePickMouseOverItem", Qt::EditRole);
    recordPickUndo(pickStatus == "Picked" ? "Pick" : "Unpick", {{fPath, priorStatus}},
                   {{fPath, pickStatus}});

    updateClassification();
    thumbView->refreshIcons("MW::togglePickMouseOverItem");
    gridView->refreshIcons("MW::togglePickMouseOverItem");

    if (settings->value("playPickAudio").toBool()) playPickClick();

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
    The change is one Edit > Undo step (recordPickUndo).

    The pick status is a string in dm->sf->index(idx.row(), G::PickColumn)
        - "Unpicked"
        - "Picked"
        - "Rejected"

    DataModel::setData writes every change through to the saved picks (Datamodel/
    savedpicks.h), so a pick survives a quit, a crash or a power loss.

*/
    if (G::isLogger) G::log("MW::togglePick");
    QModelIndexList selection = RowSelection::Rows(dm->selectionModel);
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

    if (settings->value("playPickAudio").toBool()) playPickClick();

    // set pick status for selection, recording each change for Edit > Undo
    PathValues before, after;
    foreach (dmRow, rows) {
        QString fPath = dm->index(dmRow, G::PathColumn).data(G::KeyRole).toString();
        QString priorPickStatus = dm->index(dmRow, G::PickColumn).data(Qt::EditRole).toString();
        before.append({fPath, priorPickStatus});
        after.append({fPath, pickStatus});
        emit setValDm(dmRow, G::PickColumn, pickStatus, dm->instance, "MW::togglePick",
                      Qt::EditRole);
    }
    recordPickUndo(pickStatus == "Picked" ? "Pick" : "Unpick", before, after);
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
    QModelIndexList idxList = RowSelection::Rows(dm->selectionModel);
    QString pickStatus;

    bool foundFalse = false;
    // check if any images are not rejected in the selection
    foreach (idx, idxList) {
        QModelIndex pickIdx = dm->sf->index(idx.row(), G::PickColumn);
        pickStatus = qvariant_cast<QString>(pickIdx.data(Qt::EditRole));
        foundFalse = (pickStatus != "Rejected");
        if (foundFalse) break;
    }
    foundFalse ? pickStatus = "Rejected" : pickStatus = "Unpicked";

    if (settings->value("playPickAudio").toBool()) playPickClick();

    /*  set pick status for selection, recording each change for Edit > Undo. By KEY,
        captured before any write: setValSf re-filters the proxy as it goes, so a proxy
        row read after the first write can name a different image. */
    PathValues before, after;
    for (const QModelIndex &i : std::as_const(idxList)) {
        const int sfRow = i.row();
        const QString fPath = dm->sf->index(sfRow, G::PathColumn).data(G::KeyRole).toString();
        before.append({fPath, dm->sf->index(sfRow, G::PickColumn).data(Qt::EditRole)
                                  .toString()});
        after.append({fPath, pickStatus});
    }
    for (const auto &pv : std::as_const(after)) {
        const int dmRow = dm->rowFromKey(pv.first);
        if (dmRow < 0) continue;
        emit setValDm(dmRow, G::PickColumn, pickStatus, dm->instance, "MW::toggleReject",
                      Qt::EditRole);
    }
    recordPickUndo(pickStatus == "Rejected" ? "Reject" : "Unreject", before, after);

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

void MW::recordPickUndo(const QString &what, const PathValues &before,
                        const PathValues &after)
{
/*
    One pick edit as one Edit > Undo step. Replaces the pick history stack and
    "Recover prior pick state" (Cmd+Opt+Z): the same restore, now on the app-wide
    undo with redo, and for every pick path. Rows whose status did not change are
    dropped, so a step never claims images it did not touch.
*/
    if (!undoManager || undoManager->isApplying()) return;
    PathValues b = before, a = after;
    pruneUnchanged(b, a);
    if (b.isEmpty()) return;
    undoManager->push(new PathValuesCommand(undoStepText(what, int(b.size())), b, a,
                      [this](const PathValues &v) { applyPickStates(v); }));
}

void MW::applyPickStates(const PathValues &statusByPath)
{
/*
    Edit > Undo / Redo of a pick edit: set each image's pick status back. A loaded image
    goes through the model, which writes the saved picks (DataModel::setData). An image
    no longer loaded has only its saved pick to restore, and only Picked is saved -- a
    reject lives in the model alone, as it always has.
*/
    if (G::isLogger) G::log("MW::applyPickStates");
    if (!dm) return;
    const QString src = "MW::applyPickStates";
    for (const auto &pv : statusByPath) {
        const QString status = pv.second.toString();
        const int dmRow = dm->rowFromKey(pv.first);
        if (dmRow >= 0) {
            emit setValDm(dmRow, G::PickColumn, status, dm->instance, src, Qt::EditRole);
        }
        else if (SavedPicks::IsOpen()) {
            SavedPicks::Set(pv.first, status == "Picked");
        }
    }

    // same refresh tail as togglePick (see the suspension comment there)
    bool picksBeingFiltered = filters->isAnyCatItemChecked(filters->picks);
    buildFilters->updateCategory(BuildFilters::PickEdit, BuildFilters::NoAfterAction,
                                 picksBeingFiltered);
    if (picksBeingFiltered) {
        filterChange(src);
    }
    else {
        dm->sf->suspend(false, "MW::applyPickStates: no filter change");
        thumbView->refreshIcons(src);
        gridView->refreshIcons(src);
    }
    pickMemSize = Utilities::formatMemory(memoryReqdForPicks());
    updatePickDependentActions();
    updateStatus(true, "", src);
    updateClassification();
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
    QModelIndexList selection = RowSelection::Rows(dm->selectionModel);
    for(int row = 0; row < selection.count(); row++) {
        QModelIndex idx = dm->sf->index(row, G::ByteSizeColumn);
        memTot += idx.data(Qt::EditRole).toULongLong();
    }
    return memTot;
}


void MW::playPickClick()
{
/*
    Plays the pick/reject click.  The QSoundEffect is created here, not at startup,
    and deleted 3 s after the last play (pickClickReleaseTimer).  Qt 6.11 holds an open
    audio stream for the life of a QSoundEffect with a source set, and on macOS that
    stream crashes (QCoreAudioSinkStream::removeDisconnectListener, freed listener
    block) when its output device disconnects.  Holding the stream only around a
    click narrows that window from the whole session to a few seconds.
*/
    if (!pickClickReleaseTimer) {
        pickClickReleaseTimer = new QTimer(this);
        pickClickReleaseTimer->setSingleShot(true);
        pickClickReleaseTimer->setInterval(3000);
        connect(pickClickReleaseTimer, &QTimer::timeout, this, [this]() {
            if (!pickClick) return;
            if (pickClick->isPlaying()) {
                pickClickReleaseTimer->start();
                return;
            }
            pickClick->deleteLater();
            pickClick = nullptr;
        });
    }
    if (!pickClick) {
        pickClick = new QSoundEffect(this);
        pickClick->setSource(QUrl::fromLocalFile(":/Sounds/ingest.wav"));
        pickClick->setLoopCount(10);
    }
    pickClick->setVolume(pickClickvolume);
    pickClick->play();      // plays once loaded if the sample is still loading
    pickClickReleaseTimer->start();
}
