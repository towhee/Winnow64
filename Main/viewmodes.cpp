#include "Main/mainwindow.h"

void MW::setCentralView()
{
    if (G::isLogger) G::log(CLASSFUNCTION);
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
    if (G::isLogger || G::isFlowLogger) G::log(CLASSFUNCTION);

    if (!G::isInitializing && !G::allMetadataLoaded) {
        G::mode = "Loupe";
        asLoupeAction->setChecked(true);
        centralLayout->setCurrentIndex(LoupeTab);
        prevCentralView = LoupeTab;
        return;
    }

    G::mode = "Loupe";
    asLoupeAction->setChecked(true);
    updateStatus(true, "", CLASSFUNCTION);
    updateIconRange(-1);

    // save selection as tableView is hidden and not synced
    saveSelection();

    /* show imageView or videoView in the central widget. This makes thumbView visible,
    and it updates the index to its previous state. The index update triggers
    fileSelectionChange */
    bool isVideo = dm->sf->index(currSfRow, G::VideoColumn).data().toBool();
    if (isVideo) {
        centralLayout->setCurrentIndex(VideoTab);
    }
    else {
        centralLayout->setCurrentIndex(LoupeTab);
    }
    prevCentralView = LoupeTab;

    /* recover thumbdock if it was visible before as gridView and full screen can
    hide the thumbdock */
    if(isNormalScreen && wasThumbDockVisible) {
        thumbDock->setVisible(true);
        thumbDockVisibleAction->setChecked(wasThumbDockVisible);
        thumbView->selectThumb(currSfRow);
    }

    if (thumbView->isVisible()) thumbView->setFocus();
    else imageView->setFocus();

    QModelIndex idx = dm->sf->index(currSfRow, 0);
    thumbView->setCurrentIndex(idx);

    // do not show classification badge if no folder or nothing selected
    updateClassification();

    // req'd after compare mode to re-enable extended selection
    thumbView->setSelectionMode(QAbstractItemView::ExtendedSelection);

    // selection has been lost while tableView and possibly thumbView were hidden
    recoverSelection();

    // req'd to show thumbs first time
    thumbView->setThumbParameters();

    // sync scrolling between modes (loupe, grid and table)
    updateIconRange(-1);
    if (prevMode == "Table") {
        if (tableView->isRowVisible(currSfRow)) scrollRow = currSfRow;
        else scrollRow = tableView->midVisibleRow;
    }
    if (prevMode == "Grid") {
        if(gridView->isRowVisible(currSfRow)) scrollRow = currSfRow;
        else scrollRow = gridView->midVisibleCell;
    }
    if (prevMode == "Compare") {
        qDebug() << CLASSFUNCTION << currSfRow;
        scrollRow = currSfRow;
    }
    G::ignoreScrollSignal = false;
    thumbView->scrollToRow(scrollRow, CLASSFUNCTION);

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
    if (G::isLogger || G::isFlowLogger) G::log(CLASSFUNCTION);

    if (embelProperties->templateId > 0) {
        QString msg = "Only loupe mode is available while the Embellish Editor is active.";
        G::popUp->showPopup(msg, 2000);
        return;
    }

//    if (!G::allMetadataLoaded) {
//        G::mode = "Grid";
//        asGridAction->setChecked(true);
//        centralLayout->setCurrentIndex(GridTab);
//        prevCentralView = GridTab;
//        return;
//    }

    G::mode = "Grid";
    asGridAction->setChecked(true);
    updateStatus(true, "", CLASSFUNCTION);
    updateIconRange(-1);

    // save selection as gridView is hidden and not synced
    saveSelection();

    // hide the thumbDock in grid mode as we don't need to see thumbs twice
    thumbDock->setVisible(false);
    thumbDockVisibleAction->setChecked(false);

    // show gridView in central widget
    centralLayout->setCurrentIndex(GridTab);
    prevCentralView = GridTab;

    QModelIndex idx = dm->sf->index(currSfRow, 0);
    gridView->setCurrentIndex(idx);
    thumbView->setCurrentIndex(idx);

    // req'd to show thumbs first time
//    gridView->setThumbParameters();

    // req'd after compare mode to re-enable extended selection
    gridView->setSelectionMode(QAbstractItemView::ExtendedSelection);

    // selection has been lost while tableView and possibly thumbView were hidden
    recoverSelection();

    // req'd to show thumbs first time
//    gridView->setThumbParameters();

    // sync scrolling between modes (loupe, grid and table)
    if (prevMode == "Table") {
        if (tableView->isRowVisible(currSfRow)) scrollRow = currSfRow;
        else scrollRow = tableView->midVisibleRow;
    }
    if (prevMode == "Loupe" /*&& thumbView->isVisible() == true*/) {
        if (thumbView->isRowVisible(currSfRow)) scrollRow = currSfRow;
        else scrollRow = thumbView->midVisibleCell;
    }

    // when okToScroll scroll gridView to current row
    G::ignoreScrollSignal = false;
    G::wait(100);
//    gridView->waitUntilOkToScroll();
     gridView->scrollToRow(scrollRow, CLASSFUNCTION);
    updateIconRange(-1);

    if (gridView->justifyMargin() > 3) gridView->rejustify();

    // if the zoom dialog was open then hide it as no image visible to zoom
    if (zoomDlg && isZoomDlgVisible) zoomDlg->setVisible(false);

    gridView->setFocus();
    prevMode = "Grid";
    gridDisplayFirstOpen = false;
}

void MW::tableDisplay()
{
    if (G::isLogger || G::isFlowLogger) G::log(CLASSFUNCTION);

    if (embelProperties->templateId > 0) {
        G::popUp->showPopup(
            "Only loupe mode is available while the Embellish Editor "
            "is active.", 2000);
        return;
    }

    G::mode = "Table";
    asTableAction->setChecked(true);
    updateStatus(true, "", CLASSFUNCTION);
    updateIconRange(-1);

    // save selection as tableView is hidden and not synced
    saveSelection();

    // change to the table view
    centralLayout->setCurrentIndex(TableTab);
    prevCentralView = TableTab;

    /* thumbView, gridView and tableView share the same datamodel and selection
       model, so when one changes due to user interaction they all change, unless
       they are not visible.  Therefore we must do a manual update of the current
       index (currentRow) and selection every time there is a mode change between
       Loupe, Grid, Table and Compare.

       Changes to the current index signal the slot fileSelectionChange, which in
       turn updates currentRow to the current index.
    */
    // get the current index from currentRow
    QModelIndex idx = dm->sf->index(currSfRow, 0);
    // set the current index for all views that could be visible
    tableView->setCurrentIndex(idx);
    thumbView->setCurrentIndex(idx);

    // recover thumbdock if it was visible before as gridView and full screen can
    // hide the thumbdock
    if(isNormalScreen){
        if(wasThumbDockVisible && !thumbDock->isVisible()) {
            thumbDock->setVisible(true);
            thumbDockVisibleAction->setChecked(wasThumbDockVisible);
            thumbView->selectThumb(currSfRow);
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
    recoverSelection();

    // req'd to show thumbs first time
    thumbView->setThumbParameters();

    // sync scrolling between modes (loupe, grid and table)
//    updateMetadataCacheIconviewState(false);
    if (prevMode == "Grid") {
        if (gridView->isRowVisible(currSfRow)) scrollRow = currSfRow;
        else scrollRow = gridView->midVisibleCell;
    }
    if (prevMode == "Loupe") {
        if(thumbView->isRowVisible(currSfRow)) scrollRow = currSfRow;
        else scrollRow = thumbView->midVisibleCell;
    }
    G::ignoreScrollSignal = false;
    G::wait(100);
//    qDebug() << CLASSFUNCTION << "scrollRow =" << scrollRow;
    tableView->scrollToRow(scrollRow, CLASSFUNCTION);
    if (thumbView->isVisible()) thumbView->scrollToRow(scrollRow, CLASSFUNCTION);
    updateIconRange(-1);
//    qDebug() << CLASSFUNCTION << scrollRow << tableView->midVisibleRow;

    // if the zoom dialog was open then hide it as no image visible to zoom
    if (zoomDlg && isZoomDlgVisible) zoomDlg->setVisible(false);

    tableView->setFocus();
    prevMode = "Table";
}

void MW::compareDisplay()
{
    if (G::isLogger) G::log(CLASSFUNCTION);

    if (embelProperties->templateId > 0) {
        G::popUp->showPopup(
            "Only loupe mode is available while the Embellish Editor "
            "is active.", 2000);
        return;
    }

    int n = selectionModel->selectedRows().count();
    for (int i = 0; i < n; ++i) {
        QModelIndex idx = selectionModel->selectedRows().at(i);
        if (dm->sf->index(idx.row(), G::VideoColumn).data().toBool()) {
            G::popUp->showPopup(
                "Compare mode is not available if a video is part of "
                "the selection.", 2000);
            return;
        }
    }

    asCompareAction->setChecked(true);
    updateStatus(true, "", CLASSFUNCTION);
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
    saveSelection();
    centralLayout->setCurrentIndex(CompareTab);
    recoverSelection();
    prevCentralView = CompareTab;
    compareImages->load(centralWidget->size(), isRatingBadgeVisible, selectionModel);

    // restore thumbdock to previous state
    thumbDock->setVisible(wasThumbDockVisible);
    thumbDockVisibleAction->setChecked(wasThumbDockVisible);

    // If the zoom dialog was active, but hidden by gridView or tableView, then show it
    if (zoomDlg && isZoomDlgVisible) zoomDlg->setVisible(true);

    hasGridBeenActivated = false;
    prevMode = "Compare";
}

