#include "Main/mainwindow.h"

void MW::setCentralMessage(QString message)
{
    if (G::isLogger) G::log("MW::setCentralMessage", message);
    centralLayout->setCurrentIndex(MessageTab);
    msg.msgLabel->setText(message);
}

/**********************************************************************************************
 * HIDE/SHOW UI ELEMENTS
*/

void MW::setThumbDockFloatFeatures(bool isFloat)
{
    if (G::isLogger) G::log("MW::setThumbDockFloatFeatures", "isFloat = " + QString::number(isFloat));
    qDebug() << "MW::setThumbDockFloatFeatures" << "isFloat =" << isFloat;
    if (isFloat) {
        // thumbDock->restore();
        thumbView->setMaximumHeight(100000);
        thumbDock->setFeatures(QDockWidget::DockWidgetClosable |
                               QDockWidget::DockWidgetMovable  |
                               QDockWidget::DockWidgetFloatable);
        thumbView->setWrapping(true);
        thumbView->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        thumbView->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
        #ifdef Q_OS_WIN
        Win::setTitleBarColor(thumbDock->winId(), G::backgroundColor);
        #endif
    }
    else {
    }
}

void MW:: setThumbDockHeight()
{
/*
    Helper slot to call setThumbDockFeatures when the dockWidgetArea is not known, which is
    the case when signalling from another class like thumbView after thumbnails have been
    resized.
*/
    if (G::isLogger) G::log("MW::setThumbDockHeight");
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
    if (G::isLogger) G::log("MW::setThumbDockFeatures");
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

        // get max icon height
        int max = G::maxIconSize;

        // max and min cell heights (icon plus padding + name text)
        int maxHt = thumbView->iconViewDelegate->getCellSize(QSize(max, max)).height();
        int minHt = thumbView->iconViewDelegate->getCellSize(QSize(ICON_MIN, ICON_MIN)).height();
        // plus the scroll bar + 2 to make sure no vertical scroll bar is required
        maxHt += G::scrollBarThickness /*+ 2*/;
        minHt += G::scrollBarThickness;

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
        qDebug()
             << "MW::setThumbDockFeatures  dock area =" << area
             << "thumbView Ht =" << thumbView->height()
             << "maxHt ="  << maxHt << "minHt =" << minHt
             << "G::maxIconSize =" << G::maxIconSize
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
    if (G::isLogger) G::log("MW::setRatingBadgeVisibility");
    isRatingBadgeVisible = ratingBadgeVisibleAction->isChecked();
    thumbView->refreshThumbs();
    gridView->refreshThumbs();
    updateClassification();
}

void MW::setIconNumberVisibility() {
    if (G::isLogger) G::log("MW::setIconNumberVisibility");
    isIconNumberVisible = iconNumberVisibleAction->isChecked();
    thumbView->refreshThumbs();
    gridView->refreshThumbs();
}

void MW::setShootingInfoVisibility() {
    if (G::isLogger) G::log("MW::setShootingInfoVisibility");
    imageView->infoOverlay->setVisible(infoVisibleAction->isChecked());
}

void MW::setFolderDockVisibility()
{
    if (G::isLogger) G::log("MW::setFolderDockVisibility");
    folderDock->setVisible(folderDockVisibleAction->isChecked());
}

void MW::setFavDockVisibility()
{
    if (G::isLogger) G::log("MW::setFavDockVisibility");
    favDock->setVisible(favDockVisibleAction->isChecked());
}

void MW::setFilterDockVisibility()
{
    if (G::isLogger) G::log("MW::setFilterDockVisibility");
    filterDock->setVisible(filterDockVisibleAction->isChecked());
}

void MW::setMetadataDockVisibility()
{
    if (G::isLogger) G::log("MW::setMetadataDockVisibility");
    if (G::useInfoView) metadataDock->setVisible(metadataDockVisibleAction->isChecked());
}

void MW::setEmbelDockVisibility()
{
    if (G::isLogger) G::log("MW::setEmbelDockVisibility");
    embelDock->setVisible(embelDockVisibleAction->isChecked());
}

void MW::setMetadataDockFixedSize()
{
    if (!G::useInfoView) return;
    if (G::isLogger) G::log("MW::setMetadataDockFixedSize");
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
    if (G::isLogger) G::log("MW::setThumbDockVisibity");
    thumbDock->setVisible(thumbDockVisibleAction->isChecked());
    sel->setCurrentRow(dm->currentSfRow);
}

void MW::focusOnDock(DockWidget *dockWidget)
{
    if (G::isLogger) G::log("MW::focusOnDock", dockWidget->objectName());
    dockWidget->raise();
    dockWidget->setVisible(true);
}

void MW::toggleFolderDockVisibility()
{
    if (G::isLogger) G::log("MW::toggleFolderDockVisibility");
    qDebug() << "MW::toggleFolderDockVisibility";
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
    if (G::isLogger) G::log("MW::toggleFavDockVisibility");
    if (G::isInitializing) return;
    qDebug() << "MW::toggleFavDockVisibility";
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

void MW::toggleFilterDockVisibility()
/*
    Called from folterDockVisibleAction.

    NOTE: When the filter tab is mouse clicked, MW::eventFilter calls
    MW::filterDockTabMousePress which triggers buildFilters->build().

    Do not attempt to build filters when the filter panel is not visible, as this
    can cause a crash if there are any videos in the mix.
*/
{
    if (G::isLogger) G::log("MW::toggleFilterDockVisibility");
    if (G::isInitializing) return;

    QString dock = filterDockTabText;
    if (isDockTabified(dock) && !isSelectedDockTab(dock)) dockToggle = SetFocus;
    else if (filterDock->isVisible()) dockToggle = SetInvisible;
    else dockToggle = SetVisible;
    qDebug() << "MW::toggleFilterDockVisibility dockToggle =" << dockToggle;

    switch (dockToggle) {
    case SetFocus:
        filterDock->raise();
        filterDockVisibleAction->setChecked(true);
        if (!filters->filtersBuilt) {
            buildFilters->build();
        }
        break;
    case SetInvisible:
        filterDock->setVisible(false);
        filterDockVisibleAction->setChecked(false);
        break;
    case SetVisible:
        filterDock->setVisible(true);
        filterDock->raise();
        filterDockVisibleAction->setChecked(true);
        if (!filters->filtersBuilt) {
            buildFilters->build();
        }
    }
}

void MW::toggleMetadataDockVisibility() {
    if (!G::useInfoView) return;
    if (G::isLogger) G::log("MW::toggleMetadataDockVisibility");
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
    if (G::isLogger) G::log("MW::toggleThumbDockVisibity");

    if (G::isInitializing) {
        G::popup->showPopup("Please wait until initialization is completed.", 2000);
        return;
    }

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
        thumbView->scrollToRow(dm->scrollToIcon, "MW::toggleThumbDockVisibity");
        thumbDockVisibleAction->setChecked(true);
    }

    if (G::mode != "Grid" && !isFullScreen()) {
        wasThumbDockVisible = thumbDock->isVisible();
    }
//    /*
      qDebug() << "MW::toggleThumbDockVisibity"
             << "wasThumbDockVisible =" << wasThumbDockVisible
             << "G::mode =" << G::mode
             << "isNormalScreen =" << !isFullScreen()
             << "thumbDock->isVisible() =" << thumbDock->isVisible();
    //*/
}

void MW::toggleEmbelDockVisibility() {
    if (G::isLogger) G::log("MW::toggleEmbelDockVisibility");
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
    if (G::isLogger) G::log("MW::setMenuBarVisibility");
    // menuBar()->setVisible(menuBarVisibleAction->isChecked());
}

void MW::setStatusBarVisibility()
{
    if (G::isLogger) G::log("MW::setStatusBarVisibility");
    statusBar()->setVisible(statusBarVisibleAction->isChecked());
}

void MW::setCacheStatusVisibility()
{
    if (G::isLogger) G::log("MW::setCacheStatusVisibility");
    if (isShowCacheProgressBar && !G::isSlideShow)
        progressLabel->setVisible(isShowCacheProgressBar);
}

void MW::setProgress(int value)
{
/*
    Used by ingest to show progress on left side of status bar.
*/
    if (G::isLogger) G::log("MW::setProgress");
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
    if (G::isLogger) G::log("MW::setStatus");
    statusLabel->setText("    " + state + "    ");
}

void MW::setIngested()
{
/*
    Called after ingest to update the DataModel, filter and the settings piclLog.
    The pickLog is used to recover the picked/ingested values in the datamodel
    after a crash recovery.
*/
    if (G::isLogger) G::log("MW::setIngested");
    settings->beginGroup("PickLog");
    for (int sfRow = 0; sfRow < dm->sf->rowCount(); ++sfRow) {
        QString sKey = dm->sf->index(sfRow, 0).data(G::PathRole).toString();
        if (dm->sf->index(sfRow, G::PickColumn).data().toString() == "Picked") {
            emit setValSf(sfRow, G::IngestedColumn, "true", dm->instance,
                          "MW::setIngested", Qt::EditRole);
            emit setValSf(sfRow, G::PickColumn, "Ingested", dm->instance,
                          "MW::setIngested", Qt::EditRole, Qt::AlignCenter);
            // update pickLog
            sKey.replace("/", "🔸");
                              settings->setValue(sKey, "ingested");
        }
    }
    settings->endGroup();

    // update filter counts
    buildFilters->updateCategory(BuildFilters::PickEdit, BuildFilters::NoAfterAction);
}

void MW::setCombineRawJpg()
{
    if (G::isLogger)
        G::log("MW::setCombineRawJpg");

    if (!dm->rowCount()) return;

    if (dm->rowCount() && !G::allMetadataLoaded) {
        QString msg = "Folder is still loading.  Try again when the folder has loaded.";
        G::popup->showPopup(msg, 2000);
        return;
    }

    // flag used in MW, dm and sf, fsTree, bookmarks
    combineRawJpg = combineRawJpgAction->isChecked();

    QString msg;
    if (combineRawJpg) msg = "Combining Raw + Jpg pairs.  This could take a moment.";
    else msg = "Separating Raw + Jpg pairs.  This could take a moment.";
    G::popup->showPopup(msg);
    qApp->processEvents();

    // prevent crash when there are videos (did not work)
    stop();
    // reset();

    settings->setValue("combineRawJpg", combineRawJpg);
    updateStatusBar();

    G::combineRawJpg = combineRawJpg;
    dm->sf->combineRawJpg = combineRawJpg;
    fsTree->combineRawJpg = combineRawJpg;
    bookmarks->combineRawJpg = combineRawJpg;
    // update image counts
    fsTree->refreshModel();
    refreshBookmarks();
    // qDebug() << "MW::setCombineRawJpg combineRawJpg =" << combineRawJpg;

   // update the datamodel type column
   QString src = "setCombinedRawJpg";
   for (int dmRow = 0; dmRow < dm->rowCount(); ++dmRow) {
       QModelIndex idx = dm->index(dmRow, 0);
       if (idx.data(G::DupIsJpgRole).toBool()) {
           QString rawType = idx.data(G::DupRawTypeRole).toString();
           QModelIndex typeIdx = dm->index(dmRow, G::TypeColumn);
           if (combineRawJpg) {
               emit setValDm(dmRow, G::TypeColumn, "JPG+" + rawType,
                             dm->instance, src, Qt::EditRole, Qt::AlignCenter);
           }
           else {
               emit setValDm(dmRow, G::TypeColumn, "JPG",
                             dm->instance, src, Qt::EditRole, Qt::AlignCenter);
           }
       }
   }

   // // refresh the proxy sort/filter
   // dm->sf->filterChange();

   // update elements available to sort and filter
   dm->rebuildTypeFilter();

   // redo the filter to either combine or separate the raw and jpg files
   filterChange("MW::setCombineRawJpg");

   updateStatusBar();

   G::popup->close();
}

void MW::refreshViewsOnCacheChange(QString fPath, bool isCached, QString src)
{
/*
    When an image is added or removed from the image cache in ImageCache a signal
    triggers this slot. The thumbView and gridView thumbnail is refreshed to update the
    cache badge.

    If the image is the current one, then imageView is called.

*/
    int sfRow = dm->proxyRowFromPath(fPath, "MW::refreshViewsOnCacheChange");
    bool isCurrent = sfRow == dm->currentSfRow;
    QModelIndex sfIdx = dm->sf->index(sfRow, 0);
    bool isVideo = dm->sf->index(sfRow, G::VideoColumn).data().toBool();

    if (G::isLogger) {
        QString msg = "Row " + QString::number(sfRow) + " " + fPath;
        G::log("MW::refreshViewsOnCacheChange", msg);
    }
    /*
    qDebug() << "MW::refreshViewsOnCacheChange"
             << "sfRow =" << sfRow
             << "isCached =" << isCached
             << "isCurrent =" << isCurrent
             << "src =" << src
                ; //*/

    if (sfRow == -1) {
        QString msg = "Image not found, maybe sudden folder change.";
        G::issue("Warning", msg, "MW::refreshViewsOnCacheChange");
        return;
    }

    if (isCached && isCurrent && !isVideo) {
        // qDebug() << "MW::refreshViewsOnCacheChange call imageView->loadImage" << fPath;
        centralLayout->setCurrentIndex(prevCentralView);
        imageView->loadImage(fPath, true, "MW::refreshViewsOnCacheChange");
        updateClassification();
    }

    thumbView->refreshThumb(sfIdx);
    gridView->refreshThumb(sfIdx);

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
    if (G::isLogger) G::log("MW::updateClassification");
    // check if still in a folder with images
    if (dm->rowCount() < 1) {
        imageView->classificationLabel->setVisible(false);
    }
    int row = thumbView->currentIndex().row();
    isPick = dm->sf->index(row, G::PickColumn).data(Qt::EditRole).toString() == "Picked";
    isReject = dm->sf->index(row, G::PickColumn).data(Qt::EditRole).toString() == "Rejected";
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

void MW::setIgnoreAddThumbnailsDlg(bool ignore)
{
    qDebug() << "MW::setIgnoreAddThumbnailsDlg";
    settings->setValue("ignoreAddThumbnailsDlg", ignore);
    pref->setItemValue("ignoreAddThumbnailsDlg", !ignore); // means to show in preferences
    ignoreAddThumbnailsDlg = ignore;
}

void MW::setBackupModifiedFiles(bool isBackup)
{
    qDebug() << "MW::setBackupModifiedFiles";
    G::backupBeforeModifying = isBackup;
}

void MW::showMouseCursor()
{
    setCursor(QCursor(Qt::ArrowCursor));
}

void MW::hideMouseCursor()
{
    setCursor(QCursor(Qt::BlankCursor));
}
