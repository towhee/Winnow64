#include "Main/mainwindow.h"

void MW::setCentralView()
{
    if (G::isLogger) G::log("MW::setCentralView");
    if (!isSettings) return;
    if (asLoupeAction->isChecked()) loupeDisplay("MW::setCentralView");
    if (asGridAction->isChecked()) gridDisplay();
    if (asTableAction->isChecked()) tableDisplay();
    if (asCompareAction->isChecked()) compareDisplay();
    /* The display functions above do this for themselves now, but prevMode must also be
       reset when there is nothing loaded. */
    if (showCentralMessageIfNoImages()) prevMode = "Loupe";
    enableSelectionDependentMenus();
}

void MW::loupeDisplay(const QString src)
{
/*
    In the central widget show a loupe view of the image pointed to by the thumbView
    currentindex.

    Note: When the thumbDock thumbView is displayed it needs to be scrolled to the
    currentIndex since it has been "hidden". However, the scrollbars take a long
    time to paint after the view show event, so the ThumbView::scrollToCurrent
    function must be delayed. This is done by the eventFilter in MW, intercepting
    the scrollbar paint events. This is a bit of a cludge to get around lack of
    notification when the QListView has finished painting itself.
*/
    /*
    if (!G::isInitializing && G::isLogger)
        qDebug() << "MW::loupeDisplay"
                 << "src =" << src
                 << "wasThumbDockVisible =" << QVariant(wasThumbDockVisible).toString()
                 << "G::isInitializing =" << G::isInitializing
                    ; //*/
    if (G::isLogger || G::isFlowLogger)
        G::log("MW::loupeDisplay", "src = " + src);

    /*  ALREADY IN LOUPE: A NEW IMAGE, NOT A MODE SWITCH.

        MW::fileSelectionChange calls this for every selection made in Loupe, and it
        used to run the whole switch below each time: filters->enable (a walk of the
        filter tree), sel->save/recover (a ClearAndSelect that re-emits the selection
        just made), setThumbParameters (whose setSpacing makes QListView schedule a
        relayout, which the next scrollToRow's visualRect then forces over EVERY row),
        and enableSelectionDependentMenus a second time. At 148,567 rows that was ~20
        of a ~30 ms keypress, sampled -- the relayout alone ~12 ms.

        None of it changes when the mode has not: the tree is only disabled by Compare
        (a mode switch), the selection is the one just made, the thumb parameters
        change only through a mode switch, a workspace or a preference (all of which
        call this with their own src), and fileSelectionChange gated the menus a
        moment ago. What does follow the IMAGE is kept: the status bar, the
        classification badge, the thumb strip's scroll, and the no-images message. */
    if (src == "MW::fileSelectionChange" && G::mode == "Loupe" && prevMode == "Loupe"
        && centralLayout->currentIndex() == LoupeTab)
    {
        updateStatus(true, "", "MW::loupeDisplay");
        updateClassification();
        thumbView->scrollToRow(dm->scrollToIcon, "MW::loupeDisplay");
        showCentralMessageIfNoImages();
        return;
    }

    /* Capture the shared scroll anchor before any view is shown/hidden. Showing a view
       emits scroll signals from its stale position, whose handlers overwrite
       dm->scrollToIcon; using the captured value keeps the new view's scroll in sync. */
    int scrollAnchor = dm->scrollToIcon;
    G::mode = "Loupe";
    filterDock->setEnabled(true);
    filters->enable();
    thumbDockVisibleAction->setEnabled(true);
    asLoupeAction->setChecked(true);
    updateStatus(true, "", "MW::loupeDisplay");

    // save selection as tableView is hidden and not synced
    sel->save("MW::loupeDisplay");

    /* show imageView or videoView in the central widget. This makes thumbView visible,
    and it updates the index to its previous state. The index update triggers
    fileSelectionChange */
    bool isVideo = dm->sf->index(dm->currentSfRow, G::VideoColumn).data().toBool();
    if (isVideo) {
        centralLayout->setCurrentIndex(VideoTab);
        /* (Re)load the video unless the same clip is already loaded and paused (i.e.
           visible on the first frame). Entering loupe from grid/table with a video
           selected leaves videoView stopped, because MW::fileSelectionChange calls
           videoView->stop() but only reloads the video when already in Loupe mode (or
           on double-click). Without a reload the VideoTab shows blank. stop() does not
           clear the source, so the source alone is not enough - we also require the
           paused state, which is the displayed state set by VideoView::load(). This
           guard skips the redundant double-load when fileSelectionChange has just
           loaded the same clip (e.g. double-click into loupe). */
        QMediaPlayer *mp = videoView->video->mediaPlayer;
        bool sameSourceShown = mp->source().toLocalFile() == dm->currentFilePath
                               && mp->playbackState() == QMediaPlayer::PausedState;
        if (!sameSourceShown) {
            videoView->load(dm->currentFilePath);
        }
    }
    else {
        centralLayout->setCurrentIndex(LoupeTab);
    }
    prevCentralView = LoupeTab;

    /* recover thumbdock if it was visible before as gridView and full screen can
       hide the thumbdock. NOT while the bottom show/hide bar is holding that area
       collapsed: the user put the thumbnails away deliberately, and a mode change is
       not them asking for them back. */
    if (!isFullScreen() && wasThumbDockVisible
        && !isDockAreaCollapsed(Qt::BottomDockWidgetArea)) {
            thumbDock->setVisible(true);
            thumbDockVisibleAction->setChecked(true);
    }

    // if (thumbView->isVisible()) thumbView->setFocus();
    // else imageView->setFocus();

    QModelIndex idx = dm->sf->index(dm->currentSfRow, 0);
    // thumbView->setCurrentIndex(idx);

    // do not show classification badge if no folder or nothing selected
    updateClassification();

    // req'd after compare mode to re-enable extended selection
    thumbView->setSelectionMode(QAbstractItemView::ExtendedSelection);

    // selection has been lost while tableView and possibly thumbView were hidden
    sel->recover("MW::loupeDisplay");

    // req'd to show thumbs first time
    thumbView->setThumbParameters();

    /* Sync the thumb strip to the shared scroll anchor (not the selection) so it keeps
       the scroll position of the previous view. Matches MW::tableDisplay's thumb sync. */
    thumbView->scrollToRow(scrollAnchor, "MW::loupeDisplay");

    // If the zoom dialog was active, but hidden by gridView or tableView, then show it
    if (zoomDlg && isZoomDlgVisible) zoomDlg->setVisible(true);

    prevMode = "Loupe";

    /* Nothing to show (no folder, or none surviving the filter): put the message back,
       as the loupe tab has just replaced it with an empty view. */
    showCentralMessageIfNoImages();

    enableSelectionDependentMenus();
}

void MW::gridDisplay()
{
/*
    Note: When the gridView is displayed it needs to be scrolled to the currentIndex
    since it has been "hidden". However, the scrollbars take a long time to paint after
    the view show event, so the ThumbView::scrollToCurrent function must be delayed. This
    is done by the eventFilter in MW (installEventFilter), intercepted the scrollbar
    paint events. This is a bit of a cludge to get around lack of notification when the
    QListView has finished painting itself.
*/
    if (G::isLogger || G::isFlowLogger) G::log("MW::gridDisplay");

    /*
    qDebug() << "MW::gridDisplay"
             << "gridView->iconWidth =" << gridView->iconWidth
             << "gridView->iconHeight =" << gridView->iconHeight;
                //*/

    // if (embelProperties->templateId > 0) {
    //     QString msg = "Only loupe mode is available while the Embellish Editor is active.<br>"
    //                   "Select template \"Do not Embellish\" to deactivate Embellish Editor<br>"
    //                   "and then you can switch to grid view<p>"
    //                   "Press ESC to continue"
    //         ;
    //     G::popUp->showPopup(msg, 0);
    //     return;
    // }

    /* Capture the shared scroll anchor before any view is shown/hidden (see note in
       MW::loupeDisplay). */
    int scrollAnchor = dm->scrollToIcon;
    G::mode = "Grid";
    filterDock->setEnabled(true);
    filters->enable();
    thumbDockVisibleAction->setEnabled(false);
    asGridAction->setChecked(true);
    updateStatus(true, "", "MW::gridDisplay");

    // save selection as gridView is hidden and not synced
    sel->save("MW::gridisplay");

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

    gridView->refreshIcons("MW::gridDisplay");

    QModelIndex idx = dm->sf->index(dm->currentSfRow, 0);
    gridView->setCurrentIndex(idx);
    thumbView->setCurrentIndex(idx);

    // req'd to show thumbs first time
//    gridView->setThumbParameters();

    // req'd after compare mode to re-enable extended selection
    gridView->setSelectionMode(QAbstractItemView::ExtendedSelection);

    // selection has been lost while tableView and possibly thumbView were hidden
    sel->recover("MW::gridDisplay");

    // req'd to show thumbs first time
//    gridView->setThumbParameters();

    // // sync scrolling between modes (loupe, grid and table)
    // if (prevMode == "Table") {
    //     if (tableView->isRowVisible(dm->currentSfRow)) scrollRow = dm->currentSfRow;
    //     else scrollRow = tableView->midVisibleRow;
    // }
    // if (prevMode == "Loupe" /*&& thumbView->isVisible() == true*/) {
    //     if (thumbView->isCellVisible(dm->currentSfRow)) scrollRow = dm->currentSfRow;
    //     else scrollRow = thumbView->midVisibleCell;
    // }

    // when okToScroll scroll gridView to current row
    G::ignoreScrollSignal = false;
    G::wait(100);

    /* Sync to the shared scroll anchor (mid-visible row of the last-scrolled view), not
       the current selection, so the grid keeps the scroll position of the previous view.
       Matches MW::tableDisplay. */
    gridView->scrollToRow(scrollAnchor, "MW::gridDisplay");


    // if the zoom dialog was open then hide it as no image visible to zoom
    if (zoomDlg && isZoomDlgVisible) zoomDlg->setVisible(false);

    gridView->setFocus();
    prevMode = "Grid";
    gridDisplayFirstOpen = false;

    showCentralMessageIfNoImages();     // see MW::loupeDisplay

    enableSelectionDependentMenus();
//    if (interrupted) metaReadThread->setCurrentRow(interruptedRow, "MW::gridDisplay");
}

void MW::tableDisplay()
{
    if (G::isLogger || G::isFlowLogger) G::log(" MW::tableDisplay");
    // if (G::isLogger || G::isFlowLogger)
        // qDebug() << "MW::tableDisplay";

    // if (embelProperties->templateId > 0) {
    //     QString msg = "Only loupe mode is available while the Embellish Editor is active.<br>"
    //                   "Select template \"Do not Embellish\" to deactivate Embellish Editor<br>"
    //                   "and then you can switch to table view<p>"
    //                   "Press ESC to continue"
    //         ;
    //     G::popUp->showPopup(msg, 0);
    //     return;
    // }

    /* Capture the shared scroll anchor before any view is shown/hidden (see note in
       MW::loupeDisplay). */
    int scrollAnchor = dm->scrollToIcon;
    G::mode = "Table";
    filterDock->setEnabled(true);
    filters->enable();
    thumbDockVisibleAction->setEnabled(true);
    asTableAction->setChecked(true);
    updateStatus(true, "", "MW::tableDisplay");

    // save selection as tableView is hidden and not synced
    sel->save("MW::tableDisplay");

    // change to the table view
    centralLayout->setCurrentIndex(TableTab);
    prevCentralView = TableTab;
    // if (isFirstTimeTableViewVisible) tableView->resizeColumns();
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
    if (!isFullScreen() && !isDockAreaCollapsed(Qt::BottomDockWidgetArea)) {
        if (wasThumbDockVisible && !thumbDock->isVisible()) {
            thumbDock->setVisible(true);
            thumbDockVisibleAction->setChecked(wasThumbDockVisible);
            // sel->setCurrentRow(dm->currentSfRow);
        }
        if (!wasThumbDockVisible && thumbDock->isVisible()) {
            thumbDock->setVisible(false);
            thumbDockVisibleAction->setChecked(wasThumbDockVisible);
        }
    }

    // req'd after compare mode to re-enable extended selection
    tableView->setSelectionMode(QAbstractItemView::ExtendedSelection);
    thumbView->setSelectionMode(QAbstractItemView::ExtendedSelection);

    // selection has been lost while tableView and possibly thumbView were hidden
    sel->recover("MW::tableDisplay");

    // req'd to show thumbs first time
    thumbView->setThumbParameters();

    // // sync scrolling between modes (loupe, grid and table)
    // if (prevMode == "Grid") {
    //     if (gridView->isCellVisible(dm->currentSfRow)) scrollRow = dm->currentSfRow;
    //     else scrollRow = dm->scrollToIcon;
    //     // else scrollRow = gridView->midVisibleCell;
    // }
    // if (prevMode == "Loupe") {
    //     if(thumbView->isCellVisible(dm->currentSfRow)) scrollRow = dm->currentSfRow;
    //     else scrollRow = thumbView->midVisibleCell;
    // }
    // G::ignoreScrollSignal = false;
    G::wait(100);
    scrollRow = scrollAnchor;
    tableView->scrollToRow(scrollRow, "MW::tableDisplay");
    if (thumbView->isVisible()) thumbView->scrollToRow(scrollRow, "MW::tableDisplay");

    // if the zoom dialog was open then hide it as no image visible to zoom
    if (zoomDlg && isZoomDlgVisible) zoomDlg->setVisible(false);

    tableView->setFocus();
    prevMode = "Table";

    showCentralMessageIfNoImages();     // see MW::loupeDisplay

    enableSelectionDependentMenus();
}

void MW::compareDisplay()
{
    if (G::isLogger) G::log("MW::compareDisplay");

    if (embelProperties->templateId > 0) {
        QString msg = "Only loupe mode is available while the Embellish Editor is active.<br>"
                      "Select template \"Do not Embellish\" to deactivate Embellish Editor<br>"
                      "and then you can switch to compare view<p>"
                      "Press ESC to continue"
            ;
        G::popup->showPopup(msg, 0);
        return;
    }

    int n = dm->selectionModel->selectedRows().count();
    for (int i = 0; i < n; ++i) {
        QModelIndex idx = dm->selectionModel->selectedRows().at(i);
        if (dm->sf->index(idx.row(), G::VideoColumn).data().toBool()) {
            G::popup->showPopup(
                "Compare mode is not available if a video is part of "
                "the selection.", 2000);
            return;
        }
    }

    // filters->disableAllItems(true);
    filterDock->setEnabled(false);
    filters->disable();
    thumbDockVisibleAction->setEnabled(true);
    asCompareAction->setChecked(true);
    updateStatus(true, "", "MW::compareDisplay");
    if (n < 2) {
        G::popup->showPopup("Select more than one image to compare.");
        return;
    }
    if (n > 16) {
        QString msg = QString::number(n);
        msg += " images have been selected.  Only the first 16 will be compared.";
        G::popup->showPopup(msg, 4000);
    }

    /* If thumbdock was visible and enter grid mode, make selection, and then
       compare the thumbdock gets frozen (cannot use splitter) at about 1/2 ht.
       Not sure what causes this, but by making the thumbdock visible before
       entered compare mode avoids this.  After enter compare mode revert
       thumbdocK to prior visibility (wasThumbDockVisible).
    */
    if (!isDockAreaCollapsed(Qt::BottomDockWidgetArea)) {
        thumbDock->setVisible(true);
        thumbDock->raise();
    }
//    thumbView->selectThumb(currentRow);

    G::mode = "Compare";
    // centralLayout->setCurrentIndex clears selectionModel
    sel->save("MW::compareDisplay");
    centralLayout->setCurrentIndex(CompareTab);
    sel->recover("MW::compareDisplay");
    prevCentralView = CompareTab;
    compareImages->load(centralWidget->size(), isRatingBadgeVisible, dm->selectionModel);

    // restore thumbdock to previous state (unless the show/hide bar has it collapsed)
    if (!isDockAreaCollapsed(Qt::BottomDockWidgetArea)) {
        thumbDock->setVisible(wasThumbDockVisible);
        thumbDockVisibleAction->setChecked(wasThumbDockVisible);
    }

    // If the zoom dialog was active, but hidden by gridView or tableView, then show it
    if (zoomDlg && isZoomDlgVisible) zoomDlg->setVisible(true);

    hasGridBeenActivated = false;
    prevMode = "Compare";

    enableSelectionDependentMenus();
}

