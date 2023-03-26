#include "Main/mainwindow.h"

void MW::setCentralView()
{
    if (G::isLogger) G::log("MW::setCentralView");
    if (!isSettings) return;
    if (asLoupeAction->isChecked()) loupeDisplay();
    if (asGridAction->isChecked()) gridDisplay();
    if (asTableAction->isChecked()) tableDisplay();
    if (asCompareAction->isChecked()) compareDisplay();
    if (G::currRootFolder == "") {
        QString msg = "Select a folder or bookmark to get started.";
        setCentralMessage(msg);
        prevMode = "Loupe";
    }
}

void MW::loupeDisplay()
{
/*
    In the central widget show a loupe view of the image pointed to by the thumbView
    currentindex.

    Note: When the thumbDock thumbView is displayed it needs to be scrolled to the
    currentIndex since it has been "hidden". However, the scrollbars take a long time to paint
    after the view show event, so the ThumbView::scrollToCurrent function must be delayed.
    This is done by the eventFilter in MW, intercepting the scrollbar paint events. This is a
    bit of a cludge to get around lack of notification when the QListView has finished
    painting itself.
*/
    if (!G::isInitializing && (G::isLogger || G::isFlowLogger))
        G::log("MW::loupeDisplay", "wasThumbDockVisible = " + QVariant(wasThumbDockVisible).toString());

    G::mode = "Loupe";
    asLoupeAction->setChecked(true);
    updateStatus(true, "", "MW::loupeDisplay");
//    updateIconRange(-1, "MW::loupeDisplay");

    // save selection as tableView is hidden and not synced
    sel->save();

    /* show imageView or videoView in the central widget. This makes thumbView visible,
    and it updates the index to its previous state. The index update triggers
    fileSelectionChange */
    bool isVideo = dm->sf->index(dm->currentSfRow, G::VideoColumn).data().toBool();
    if (isVideo) {
        centralLayout->setCurrentIndex(VideoTab);
    }
    else {
        centralLayout->setCurrentIndex(LoupeTab);
    }
    prevCentralView = LoupeTab;

    /* recover thumbdock if it was visible before as gridView and full screen can
    hide the thumbdock */
    if (isNormalScreen && wasThumbDockVisible) {
//        if (!metaReadThread->isRunning()) {
            thumbDock->setVisible(true);
            thumbDockVisibleAction->setChecked(true);
//        }
//        sel->currentRow(dm->currentSfRow);
    }

    if (thumbView->isVisible()) thumbView->setFocus();
    else imageView->setFocus();

    QModelIndex idx = dm->sf->index(dm->currentSfRow, 0);
    thumbView->setCurrentIndex(idx);

    // do not show classification badge if no folder or nothing selected
    updateClassification();

    // req'd after compare mode to re-enable extended selection
    thumbView->setSelectionMode(QAbstractItemView::ExtendedSelection);

    // selection has been lost while tableView and possibly thumbView were hidden
    sel->recover();

    // req'd to show thumbs first time
    thumbView->setThumbParameters();

    // sync scrolling between modes (loupe, grid and table)
//    updateIconRange(-1, "MW::loupeDisplay1");
    if (prevMode == "Table") {
        if (tableView->isRowVisible(dm->currentSfRow)) scrollRow = dm->currentSfRow;
        else scrollRow = tableView->midVisibleRow;
    }
    if (prevMode == "Grid") {
        if(gridView->isRowVisible(dm->currentSfRow)) scrollRow = dm->currentSfRow;
        else scrollRow = gridView->midVisibleCell;
    }
    if (prevMode == "Compare") {
        qDebug() << "MW::loupeDisplay" << dm->currentSfRow;
        scrollRow = dm->currentSfRow;
    }
    G::ignoreScrollSignal = false;
    thumbView->scrollToRow(scrollRow, "MW::loupeDisplay");

    // If the zoom dialog was active, but hidden by gridView or tableView, then show it
    if (zoomDlg && isZoomDlgVisible) zoomDlg->setVisible(true);

    prevMode = "Loupe";

}

void MW::gridDisplay()
{
/*
    Note: When the gridView is displayed it needs to be scrolled to the currentIndex since it
    has been "hidden". However, the scrollbars take a long time to paint after the view show
    event, so the ThumbView::scrollToCurrent function must be delayed. This is done by the
    eventFilter in MW (installEventFilter), intercepted the scrollbar paint events. This is a
    bit of a cludge to get around lack of notification when the QListView has finished
    painting itself.
*/
    if (G::isLogger || G::isFlowLogger) G::log("MW::gridDisplay");
    qDebug() << "MW::gridDisplay" << "1";

    if (embelProperties->templateId > 0) {
        QString msg = "Only loupe mode is available while the Embellish Editor is active.";
        G::popUp->showPopup(msg, 2000);
        return;
    }

    G::mode = "Grid";
    asGridAction->setChecked(true);
    updateStatus(true, "", "MW::gridDisplay");
//    updateIconRange(-1, "MW::gridDisplay");

    // save selection as gridView is hidden and not synced
    sel->save();

//    int interruptedRow;
//    bool interrupted = false;
//    if (metaReadThread->isRunning()) {
//        interruptedRow = metaReadThread->interrupt();
//        interrupted = true;
//    }

//    bool wasVisible = thumbDock->isVisible();
    thumbDock->setVisible(false);
    thumbDockVisibleAction->setChecked(false);
//    wasThumbDockVisible = wasVisible;

//    // hide the thumbDock in grid mode as we don't need to see thumbs twice
//    if (!metaReadThread->isRunning()) {
//        thumbDock->setVisible(false);
//        thumbDockVisibleAction->setChecked(false);
//    }

    // show gridView in central widget
    centralLayout->setCurrentIndex(GridTab);
    prevCentralView = GridTab;

    QModelIndex idx = dm->sf->index(dm->currentSfRow, 0);
    gridView->setCurrentIndex(idx);
    thumbView->setCurrentIndex(idx);

    // req'd to show thumbs first time
//    gridView->setThumbParameters();

    // req'd after compare mode to re-enable extended selection
    gridView->setSelectionMode(QAbstractItemView::ExtendedSelection);

    // selection has been lost while tableView and possibly thumbView were hidden
    sel->recover();

    // req'd to show thumbs first time
//    gridView->setThumbParameters();

    // sync scrolling between modes (loupe, grid and table)
    if (prevMode == "Table") {
        if (tableView->isRowVisible(dm->currentSfRow)) scrollRow = dm->currentSfRow;
        else scrollRow = tableView->midVisibleRow;
    }
    if (prevMode == "Loupe" /*&& thumbView->isVisible() == true*/) {
        if (thumbView->isRowVisible(dm->currentSfRow)) scrollRow = dm->currentSfRow;
        else scrollRow = thumbView->midVisibleCell;
    }

    // when okToScroll scroll gridView to current row
    G::ignoreScrollSignal = false;
    G::wait(100);
    gridView->scrollToRow(scrollRow, "MW::gridDisplay");
//    updateIconRange(-1, "MW::gridDisplay1");

    if (gridView->justifyMargin() > 3) gridView->rejustify();

    // if the zoom dialog was open then hide it as no image visible to zoom
    if (zoomDlg && isZoomDlgVisible) zoomDlg->setVisible(false);

    gridView->setFocus();
    prevMode = "Grid";
    gridDisplayFirstOpen = false;

//    if (interrupted) metaReadThread->setCurrentRow(interruptedRow, "MW::gridDisplay");
}

void MW::tableDisplay()
{
    if (G::isLogger || G::isFlowLogger) G::log("MW::tableDisplay");

    if (embelProperties->templateId > 0) {
        G::popUp->showPopup(
            "Only loupe mode is available while the Embellish Editor "
            "is active.", 2000);
        return;
    }

    G::mode = "Table";
    asTableAction->setChecked(true);
    updateStatus(true, "", "MW::tableDisplay");
//    updateIconRange(-1, "MW::tableDisplay");

    // save selection as tableView is hidden and not synced
    sel->save();

    // change to the table view
    centralLayout->setCurrentIndex(TableTab);
    prevCentralView = TableTab;
    if (isFirstTimeTableViewVisible) tableView->resizeColumns();
    isFirstTimeTableViewVisible = false;

    /* thumbView, gridView and tableView share the same datamodel and selection
       model, so when one changes due to user interaction they all change, unless
       they are not visible.  Therefore we must do a manual update of the current
       index (currentRow) and selection every time there is a mode change between
       Loupe, Grid, Table and Compare.

       Changes to the current index signal the slot fileSelectionChange, which in
       turn updates currentRow to the current index.
    */
    // get the current index from currentRow
    QModelIndex idx = dm->sf->index(dm->currentSfRow, 0);
    // set the current index for all views that could be visible
    tableView->setCurrentIndex(idx);
    thumbView->setCurrentIndex(idx);

    // recover thumbdock if it was visible before as gridView and full screen can
    // hide the thumbdock
    if(isNormalScreen){
        if(wasThumbDockVisible && !thumbDock->isVisible()) {
            thumbDock->setVisible(true);
            thumbDockVisibleAction->setChecked(wasThumbDockVisible);
            sel->currentRow(dm->currentSfRow);
        }
        if(!wasThumbDockVisible && thumbDock->isVisible()) {
            thumbDock->setVisible(false);
            thumbDockVisibleAction->setChecked(wasThumbDockVisible);
        }

    }

    // req'd after compare mode to re-enable extended selection
    tableView->setSelectionMode(QAbstractItemView::ExtendedSelection);
    thumbView->setSelectionMode(QAbstractItemView::ExtendedSelection);

    // selection has been lost while tableView and possibly thumbView were hidden
    sel->recover();

    // req'd to show thumbs first time
    thumbView->setThumbParameters();

    // sync scrolling between modes (loupe, grid and table)
    if (prevMode == "Grid") {
        if (gridView->isRowVisible(dm->currentSfRow)) scrollRow = dm->currentSfRow;
        else scrollRow = gridView->midVisibleCell;
    }
    if (prevMode == "Loupe") {
        if(thumbView->isRowVisible(dm->currentSfRow)) scrollRow = dm->currentSfRow;
        else scrollRow = thumbView->midVisibleCell;
    }
    G::ignoreScrollSignal = false;
    G::wait(100);
//    qDebug() << "MW::tableDisplay" << "scrollRow =" << scrollRow;
    tableView->scrollToRow(scrollRow, "MW::tableDisplay");
    if (thumbView->isVisible()) thumbView->scrollToRow(scrollRow, "MW::tableDisplay");

    // if the zoom dialog was open then hide it as no image visible to zoom
    if (zoomDlg && isZoomDlgVisible) zoomDlg->setVisible(false);

    tableView->setFocus();
    prevMode = "Table";
}

void MW::compareDisplay()
{
    if (G::isLogger) G::log("MW::compareDisplay");

    if (embelProperties->templateId > 0) {
        G::popUp->showPopup(
            "Only loupe mode is available while the Embellish Editor "
            "is active.", 2000);
        return;
    }

    int n = dm->selectionModel->selectedRows().count();
    for (int i = 0; i < n; ++i) {
        QModelIndex idx = dm->selectionModel->selectedRows().at(i);
        if (dm->sf->index(idx.row(), G::VideoColumn).data().toBool()) {
            G::popUp->showPopup(
                "Compare mode is not available if a video is part of "
                "the selection.", 2000);
            return;
        }
    }

    asCompareAction->setChecked(true);
    updateStatus(true, "", "MW::compareDisplay");
    if (n < 2) {
        G::popUp->showPopup("Select more than one image to compare.");
        return;
    }
    if (n > 16) {
        QString msg = QString::number(n);
        msg += " images have been selected.  Only the first 16 will be compared.";
        G::popUp->showPopup(msg, 2000);
    }

    /* If thumbdock was visible and enter grid mode, make selection, and then
       compare the thumbdock gets frozen (cannot use splitter) at about 1/2 ht.
       Not sure what causes this, but by making the thumbdock visible before
       entered compare mode avoids this.  After enter compare mode revert
       thumbdocK to prior visibility (wasThumbDockVisible).
    */
    thumbDock->setVisible(true);
    thumbDock->raise();
//    thumbView->selectThumb(currentRow);

    G::mode = "Compare";
    // centralLayout->setCurrentIndex clears selectionModel
    sel->save();
    centralLayout->setCurrentIndex(CompareTab);
    sel->recover();
    prevCentralView = CompareTab;
    compareImages->load(centralWidget->size(), isRatingBadgeVisible, dm->selectionModel);

    // restore thumbdock to previous state
    thumbDock->setVisible(wasThumbDockVisible);
    thumbDockVisibleAction->setChecked(wasThumbDockVisible);

    // If the zoom dialog was active, but hidden by gridView or tableView, then show it
    if (zoomDlg && isZoomDlgVisible) zoomDlg->setVisible(true);

    hasGridBeenActivated = false;
    prevMode = "Compare";
}

