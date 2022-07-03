#include "Main/mainwindow.h"

void MW::setCentralMessage(QString message)
{
    if (G::isLogger) G::log(CLASSFUNCTION);
    msg.msgLabel->setText(message);
    centralLayout->setCurrentIndex(MessageTab);
    QApplication::processEvents();
}

/**********************************************************************************************
 * HIDE/SHOW UI ELEMENTS
*/

void MW::setThumbDockFloatFeatures(bool isFloat)
{
    if (G::isLogger) G::log(CLASSFUNCTION, "isFloat = " + QString::number(isFloat));
    if (isFloat) {
        thumbView->setMaximumHeight(100000);
        thumbDock->setFeatures(QDockWidget::DockWidgetClosable |
                               QDockWidget::DockWidgetMovable  |
                               QDockWidget::DockWidgetFloatable);
//        thumbsWrapAction->setChecked(true);
        thumbView->setWrapping(true);
//        thumbView->isFloat = isFloat;
        thumbView->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    }
}

void MW:: setThumbDockHeight()
{
/*
    Helper slot to call setThumbDockFeatures when the dockWidgetArea is not known, which is
    the case when signalling from another class like thumbView after thumbnails have been
    resized.
*/
    if (G::isLogger) G::log(CLASSFUNCTION);
    setThumbDockFeatures(dockWidgetArea(thumbDock));
}

void MW::setThumbDockFeatures(Qt::DockWidgetArea area)
{
/*
    When the thumbDock is moved or when the thumbnails have been resized set the
    thumbDock features accordingly, based on settings in preferences for wrapping
    and titlebar visibility.

    Note that a floating thumbDock does not trigger this slot. The float
    condition is handled by setThumbDockFloatFeatures.

    Also note that the gridView is located in the central widget so this function only
    applies to thumbView (the docked version of IconView).

*/
    if (G::isLogger) G::log(CLASSFUNCTION);
    if (thumbDock->isFloating()) return;
    thumbView->setMaximumHeight(100000);

    /* Check if the thumbDock is docked top or bottom. If so, set the titlebar to vertical and
    the thumbDock to accomodate the height of the thumbs. Set horizontal scrollbar on all the
    time (to simplify resizing dock and thumbs). The vertical scrollbar depends on whether
    wrapping is checked in preferences.
    */
    if (area == Qt::BottomDockWidgetArea || area == Qt::TopDockWidgetArea) {
        thumbDock->setFeatures(QDockWidget::DockWidgetClosable |
                               QDockWidget::DockWidgetMovable  |
                               QDockWidget::DockWidgetFloatable |
                               QDockWidget::DockWidgetVerticalTitleBar);
        thumbView->setWrapping(false);

        // if thumbDock area changed then set dock height to cell size

        // get max icon height based on best aspect
        int hMax = G::iconHMax;

        // max and min cell heights (icon plus padding + name text)
        int maxHt = thumbView->iconViewDelegate->getCellSize(QSize(hMax, hMax)).height();
        int minHt = thumbView->iconViewDelegate->getCellSize(QSize(ICON_MIN, ICON_MIN)).height();
        // plus the scroll bar + 2 to make sure no vertical scroll bar is required
        maxHt += G::scrollBarThickness /*+ 2*/;
        minHt += G::scrollBarThickness;
//        thumbView->verticalScrollBar()->setVisible(false);

        if (maxHt <= minHt) maxHt = G::maxIconSize;

        // new cell height
        int cellHt = thumbView->iconViewDelegate->getCellHeightFromThumbHeight(thumbView->iconHeight);

        //  new dock height based on new cell size
        int newThumbDockHeight = cellHt + G::scrollBarThickness;
        if (newThumbDockHeight > maxHt) newThumbDockHeight = maxHt;

        thumbView->setMaximumHeight(maxHt);
        thumbView->setMinimumHeight(minHt);

        thumbView->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
        thumbView->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        resizeDocks({thumbDock}, {newThumbDockHeight}, Qt::Vertical);
        /*
        qDebug() << "\nMW::setThumbDockFeatures dock area =" << area << "\n"
             << "***  thumbView Ht =" << thumbView->height()
             << "maxHt ="  << maxHt << "minHt =" << minHt
             << "thumbHeight =" << thumbView->thumbHeight
             << "newThumbDockHeight" << newThumbDockHeight
             << "scrollBarHeight =" << G::scrollBarThickness;
//        */
    }

    /* Must be docked left or right or is floating.  Turn horizontal scrollbars off.  Turn
       wrapping on.
    */
    else {
        thumbDock->setFeatures(QDockWidget::DockWidgetClosable |
                               QDockWidget::DockWidgetMovable  |
                               QDockWidget::DockWidgetFloatable);
        thumbView->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        thumbView->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
        thumbView->setWrapping(true);
    }
}

void MW::setRatingBadgeVisibility() {
    if (G::isLogger) G::log(CLASSFUNCTION);
    isRatingBadgeVisible = ratingBadgeVisibleAction->isChecked();
    thumbView->refreshThumbs();
    gridView->refreshThumbs();
    updateClassification();
}

void MW::setShootingInfoVisibility() {
    if (G::isLogger) G::log(CLASSFUNCTION);
    imageView->infoOverlay->setVisible(infoVisibleAction->isChecked());
}

void MW::setFolderDockVisibility()
{
    if (G::isLogger) G::log(CLASSFUNCTION);
    folderDock->setVisible(folderDockVisibleAction->isChecked());
}

void MW::setFavDockVisibility()
{
    if (G::isLogger) G::log(CLASSFUNCTION);
    favDock->setVisible(favDockVisibleAction->isChecked());
}

void MW::setFilterDockVisibility()
{
    if (G::isLogger) G::log(CLASSFUNCTION);
    filterDock->setVisible(filterDockVisibleAction->isChecked());
}

void MW::setMetadataDockVisibility()
{
    if (G::isLogger) G::log(CLASSFUNCTION);
    if (useInfoView) metadataDock->setVisible(metadataDockVisibleAction->isChecked());
}

void MW::setEmbelDockVisibility()
{
    if (G::isLogger) G::log(CLASSFUNCTION);
    embelDock->setVisible(embelDockVisibleAction->isChecked());
}

void MW::setMetadataDockFixedSize()
{
    if (!useInfoView) return;
    if (G::isLogger) G::log(CLASSFUNCTION);
    if (metadataFixedSizeAction->isChecked()) {
        qDebug() << "variable size";
        metadataDock->setMinimumSize(200, 125);
        metadataDock->setMaximumSize(999999, 999999);
    }
    else {
        qDebug() << "fixed size";
        metadataDock->setFixedSize(metadataDock->size());
    }
}

void MW::setThumbDockVisibity()
{
    if (G::isLogger) G::log(CLASSFUNCTION);
    thumbDock->setVisible(thumbDockVisibleAction->isChecked());
    thumbView->selectThumb(currSfRow);
}

void MW::toggleFolderDockVisibility()
{
    if (G::isLogger) G::log(CLASSFUNCTION);
    if (G::isInitializing) return;
    QString dock = folderDockTabText;
    if (isDockTabified(dock) && !isSelectedDockTab(dock)) dockToggle = SetFocus;
    else if (folderDock->isVisible()) dockToggle = SetInvisible;
    else dockToggle = SetVisible;

    switch (dockToggle) {
    case SetFocus:
        folderDock->raise();
        folderDockVisibleAction->setChecked(true);
        break;
    case SetInvisible:
        folderDock->setVisible(false);
        folderDockVisibleAction->setChecked(false);
        break;
    case SetVisible:
        folderDock->setVisible(true);
        folderDock->raise();
        folderDockVisibleAction->setChecked(true);
    }
}

void MW::toggleFavDockVisibility() {
    if (G::isLogger) G::log(CLASSFUNCTION);
    if (G::isInitializing) return;
    QString dock = favDockTabText;
    if (isDockTabified(dock) && !isSelectedDockTab(dock)) dockToggle = SetFocus;
    else if (favDock->isVisible()) dockToggle = SetInvisible;
    else dockToggle = SetVisible;

    switch (dockToggle) {
    case SetFocus:
        favDock->raise();
        favDockVisibleAction->setChecked(true);
        break;
    case SetInvisible:
        favDock->setVisible(false);
        favDockVisibleAction->setChecked(false);
        break;
    case SetVisible:
        favDock->setVisible(true);
        favDock->raise();
        favDockVisibleAction->setChecked(true);
    }
}

void MW::toggleFilterDockVisibility() {
    if (G::isLogger) G::log(CLASSFUNCTION);
    if (G::isInitializing) return;
    QString dock = filterDockTabText;
    if (isDockTabified(dock) && !isSelectedDockTab(dock)) dockToggle = SetFocus;
    else if (filterDock->isVisible()) dockToggle = SetInvisible;
    else dockToggle = SetVisible;

    switch (dockToggle) {
    case SetFocus:
        filterDock->raise();
        filterDockVisibleAction->setChecked(true);
        break;
    case SetInvisible:
        filterDock->setVisible(false);
        filterDockVisibleAction->setChecked(false);
        break;
    case SetVisible:
        filterDock->setVisible(true);
        filterDock->raise();
        filterDockVisibleAction->setChecked(true);
    }
}

void MW::toggleMetadataDockVisibility() {
    if (!useInfoView) return;
    if (G::isLogger) G::log(CLASSFUNCTION);
    if (G::isInitializing) return;
    QString dock = metadataDockTabText;
    if (isDockTabified(dock) && !isSelectedDockTab(dock)) dockToggle = SetFocus;
    else if (metadataDock->isVisible()) dockToggle = SetInvisible;
    else dockToggle = SetVisible;

    switch (dockToggle) {
    case SetFocus:
        metadataDock->raise();
        metadataDockVisibleAction->setChecked(true);
        break;
    case SetInvisible:
        metadataDock->setVisible(false);
        metadataDockVisibleAction->setChecked(false);
        break;
    case SetVisible:
        metadataDock->setVisible(true);
        metadataDock->raise();
        metadataDockVisibleAction->setChecked(true);
    }
}

void MW::toggleThumbDockVisibity()
{
    if (G::isLogger) G::log(CLASSFUNCTION);
    if (G::isInitializing) return;
    qDebug() << CLASSFUNCTION;
    QString dock = thumbDockTabText;
    if (isDockTabified(dock) && !isSelectedDockTab(dock)) dockToggle = SetFocus;
    else if (thumbDock->isVisible()) dockToggle = SetInvisible;
    else dockToggle = SetVisible;

    switch (dockToggle) {
    case SetFocus:
        thumbDock->raise();
        thumbDockVisibleAction->setChecked(true);
        break;
    case SetInvisible:
        thumbDock->setVisible(false);
        thumbDockVisibleAction->setChecked(false);
        break;
    case SetVisible:
        thumbDock->setVisible(true);
        thumbDock->raise();
        thumbDockVisibleAction->setChecked(true);
        qDebug() << CLASSFUNCTION << currentSfIdx.data() << "Calling fileSelectionChange(currentSfIdx, currentSfIdx)";
        fileSelectionChange(currentSfIdx, currentSfIdx, CLASSFUNCTION);
    }

    if (G::mode != "Grid" && isNormalScreen) {
        wasThumbDockVisible = thumbDock->isVisible();
    }
/*    qDebug() << CLASSFUNCTION
             << "wasThumbDockVisible =" << wasThumbDockVisible
             << "G::mode =" << G::mode
             << "isNormalScreen =" << isNormalScreen
             << "thumbDock->isVisible() =" << thumbDock->isVisible();*/
}

void MW::toggleEmbelDockVisibility() {
    if (G::isLogger) G::log(CLASSFUNCTION);
    if (G::isInitializing) return;
    QString dock = embelDockTabText;
    if (isDockTabified(dock) && !isSelectedDockTab(dock)) dockToggle = SetFocus;
    else if (embelDock->isVisible()) dockToggle = SetInvisible;
    else dockToggle = SetVisible;

    switch (dockToggle) {
    case SetFocus:
        embelDock->raise();
        embelDockVisibleAction->setChecked(true);
        break;
    case SetInvisible:
        embelDock->setVisible(false);
        embelDockVisibleAction->setChecked(false);
        break;
    case SetVisible:
        embelDock->setVisible(true);
        embelDock->raise();
        embelDockVisibleAction->setChecked(true);
    }
}

void MW::setMenuBarVisibility()
{
    if (G::isLogger) G::log(CLASSFUNCTION);
    menuBar()->setVisible(menuBarVisibleAction->isChecked());
}

void MW::setStatusBarVisibility()
{
    if (G::isLogger) G::log(CLASSFUNCTION);
    statusBar()->setVisible(statusBarVisibleAction->isChecked());
}

void MW::setCacheStatusVisibility()
{
    if (G::isLogger) G::log(CLASSFUNCTION);
    if (isShowCacheProgressBar && !G::isSlideShow)
        progressLabel->setVisible(isShowCacheProgressBar);
}

void MW::setProgress(int value)
{
    if (G::isLogger) G::log(CLASSFUNCTION);
    if (value < 0 || value > 100) {
        progressBar->setVisible(false);
        return;
    }
    progressBar->setValue(value);
    progressBar->setVisible(true);
    progressBar->repaint();
}

// not used rgh ??
void MW::setStatus(QString state)
{
    if (G::isLogger) G::log(CLASSFUNCTION);
    statusLabel->setText("    " + state + "    ");
}

void MW::setIngested()
{
    if (G::isLogger) G::log(CLASSFUNCTION);
    for (int row = 0; row < dm->sf->rowCount(); ++row) {
        if (dm->sf->index(row, G::PickColumn).data().toString() == "true") {
            emit setValueSf(dm->sf->index(row, G::IngestedColumn), "true", Qt::EditRole);
            emit setValueSf(dm->sf->index(row, G::PickColumn), "false", Qt::EditRole);
//            dm->sf->setData(dm->sf->index(row, G::IngestedColumn), "true");
//            dm->sf->setData(dm->sf->index(row, G::PickColumn), "false");
        }
    }
}

void MW::toggleReject()
{
/*
    If the selection has any images that are not rejected then reject them all.
    If the entire selection was already rejected then unreject them all.
    If the entire selection is nor rejected then reject them all.
*/
    if (G::isLogger) G::log(CLASSFUNCTION);
    QModelIndex idx;
    QModelIndexList idxList = selectionModel->selectedRows();
    QString pickStatus;

    // add multiple selection flag to pick history
    if (idxList.length() > 1) pushPick("Begin multiple select");

    bool foundFalse = false;
    // check if any images are not rejected in the selection
    foreach (idx, idxList) {
        QModelIndex pickIdx = dm->sf->index(idx.row(), G::PickColumn);
        pickStatus = qvariant_cast<QString>(pickIdx.data(Qt::EditRole));
        foundFalse = (pickStatus != "reject");
        if (foundFalse) break;
    }
    foundFalse ? pickStatus = "reject" : pickStatus = "false";

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
    }
    if (idxList.length() > 1) pushPick("End multiple select");

    updateClassification();
    thumbView->refreshThumbs();
    gridView->refreshThumbs();

    pickMemSize = Utilities::formatMemory(memoryReqdForPicks());
    updateStatus(true, "", CLASSFUNCTION);

    // update filter counts
    buildFilters->updateCountFiltered();
}

void MW::setCombineRawJpg()
{
    if (G::isLogger) G::log(CLASSFUNCTION);
    if (!G::isNewFolderLoaded) {
        QString msg = "Folder is still loading.  Try again when the folder has loaded.";
        G::popUp->showPopup(msg, 1000);
        return;
    }

    QString msg;

    // flag used in MW, dm and sf, fsTree, bookmarks
    combineRawJpg = combineRawJpgAction->isChecked();

    if (combineRawJpg) msg = "Combining Raw + Jpg pairs.  This could take a moment.";
    else msg = "Separating Raw + Jpg pairs.  This could take a moment.";
    G::popUp->showPopup(msg);

    fsTree->combineRawJpg = combineRawJpg;
    bookmarks->combineRawJpg = combineRawJpg;
    fsTree->refreshModel();
    refreshBookmarks();

    // show appropriate count column in filters
    if (combineRawJpg) {
        filters->hideColumn(3);
        filters->showColumn(4);
    }
    else {
        filters->hideColumn(4);
        filters->showColumn(3);
    }

    // update the datamodel type column
    for (int row = 0; row < dm->rowCount(); ++row) {
        QModelIndex idx = dm->index(row, 0);
        if (idx.data(G::DupIsJpgRole).toBool()) {
            QString rawType = idx.data(G::DupRawTypeRole).toString();
            QModelIndex typeIdx = dm->index(row, G::TypeColumn);
            if (combineRawJpg) emit setValue(typeIdx, "JPG+" + rawType, Qt::EditRole);
            else emit setValue(typeIdx, "JPG", Qt::EditRole);
//            if (combineRawJpg) dm->setData(typeIdx, "JPG+" + rawType);
//            else dm->setData(typeIdx, "JPG");
        }
    }

    // refresh the proxy sort/filter
    dm->sf->filterChange();
    dm->rebuildTypeFilter();
    filterChange(CLASSFUNCTION);
    updateStatusBar();

    G::popUp->close();
}

void MW::updateCachedStatus(QString fPath, bool isCached, QString src)
{
/*
    When an image is added or removed from the image cache in ImageCache a signal triggers
    this slot to update the datamodel cache status role. After the datamodel update the
    thumbView and gridView thumbnail is refreshed to update the cache badge.

    Note that the datamodel is used (dm), not the proxy (dm->sf). If the proxy is used and
    the user then sorts or filters the index could go out of range and the app will crash.

    Make sure the file path exists in the datamodel. The most likely failure will be if a new
    folder has been selected but the image cache has not been rebuilt.
*/
    int dmRow = dm->fPathRow[fPath];

    if (G::isLogger) {
        int row = dm->sf->mapFromSource(dm->index(dmRow, 0)).row();
        QString msg = "Row " + QString::number(row) + " " + fPath;
        G::log(CLASSFUNCTION, msg);
    }

    if (dmRow == -1) {
        qWarning() << CLASSFUNCTION << "dm->fPathrow does not contain" << fPath;
        return;
    }

    QModelIndex sfIdx = dm->sf->mapFromSource(dm->index(dmRow, 0));

    if (sfIdx.isValid()/* && metaLoaded*/) {
        emit setValueSf(sfIdx, isCached, G::CachedRole);
        if (isCached) {
            if (sfIdx.row() == currSfRow) {
                if (G::isFlowLogger) G::log(CLASSFUNCTION, fPath);
                imageView->loadImage(fPath, CLASSFUNCTION);
                updateClassification();
                centralLayout->setCurrentIndex(prevCentralView);
            }
        }
        thumbView->refreshThumb(sfIdx, G::CachedRole);
        gridView->refreshThumb(sfIdx, G::CachedRole);
    }
    else {
        qWarning() << CLASSFUNCTION << "INVALID INDEX FOR" << sfIdx;
    }
    return;
}

void MW::updateClassification()
{
/*
    Each image in the datamodel can be assigned a variety of classifications:
        - picked
        - rating (1 - 5)
        - color class (some programs like lightroom call this "label"

    The classifications are combined in a badge (a circle pixmap).  This function updates
    the badge based on the values in the datamodel.

    The function is called when the user changes a classification and when a new folder
    is selected.  If the previous folder active image had a visible classification badge
    and then the user switches to a folder with no images or ejects the drive then make
    sure the classification label is not visible.
*/
    if (G::isLogger) G::log(CLASSFUNCTION);
    // check if still in a folder with images
    if (dm->rowCount() < 1) {
        imageView->classificationLabel->setVisible(false);
    }
    int row = thumbView->currentIndex().row();
    isPick = dm->sf->index(row, G::PickColumn).data(Qt::EditRole).toString() == "true";
    isReject = dm->sf->index(row, G::PickColumn).data(Qt::EditRole).toString() == "reject";
    rating = dm->sf->index(row, G::RatingColumn).data(Qt::EditRole).toString();
    colorClass = dm->sf->index(row, G::LabelColumn).data(Qt::EditRole).toString();
    if (rating == "0") rating = "";
    imageView->classificationLabel->setPick(isPick);
    imageView->classificationLabel->setReject(isReject);
    imageView->classificationLabel->setColorClass(colorClass);
    imageView->classificationLabel->setRating(rating);
    imageView->classificationLabel->setRatingColorVisibility(isRatingBadgeVisible);
    imageView->classificationLabel->refresh();

    if (G::mode == "Compare")
        compareImages->updateClassification(isPick, rating, colorClass,
                                            isRatingBadgeVisible,
                                            thumbView->currentIndex());
}

