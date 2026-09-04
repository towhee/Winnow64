#include "Main/mainwindow.h"

void MW::setCentralMessage(QString message)
{
    QString fun = "MW::setCentralMessage";
    if (G::isLogger) G::log(fun, message);
    centralLayout->setCurrentIndex(MessageTab);
    msg.msgLabel->setText(message);
    centralLayout->currentWidget()->repaint();
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
    thumbView->refreshIcons("MW::setRatingBadgeVisibility");
    gridView->refreshIcons("MW::setRatingBadgeVisibility");
    updateClassification();
}

void MW::setIconNumberVisibility() {
    if (G::isLogger) G::log("MW::setIconNumberVisibility");
    isIconNumberVisible = iconNumberVisibleAction->isChecked();
    thumbView->refreshIcons("MW::setIconNumberVisibility");
    gridView->refreshIcons("MW::setIconNumberVisibility");
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

void MW::setCatalogDockVisibility()
{
    if (G::isLogger) G::log("MW::setCatalogDockVisibility");
    /* No separate dock in Find mode -- the Catalog is a scope of the Find panel. */
    if (!catalogDock) return;
    catalogDock->setVisible(catalogDockVisibleAction->isChecked());
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

void MW::setDevelopDockVisibility()
{
    if (G::isLogger) G::log("MW::setDevelopDockVisibility");
    const bool on = developDockVisibleAction->isChecked();
    /* History and Presets are part of the Develop tool -- they follow the Develop dock,
       actions and all, so a later setHistoryDockVisibility() / setPresetsDockVisibility()
       agrees rather than fighting it. Shown before Develop so Develop, not one of them,
       is the front tab of the group. */
    if (presetsDock && presetsDockVisibleAction) {
        presetsDockVisibleAction->setChecked(on);
        presetsDock->setVisible(on);
    }
    if (historyDock && historyDockVisibleAction) {
        historyDockVisibleAction->setChecked(on);
        historyDock->setVisible(on);
    }
    developDock->setVisible(on);
    if (on) developDock->raise();
}

void MW::setHistoryDockVisibility()
{
    if (G::isLogger) G::log("MW::setHistoryDockVisibility");
    if (!historyDock || !historyDockVisibleAction) return;
    historyDock->setVisible(historyDockVisibleAction->isChecked());
}

void MW::setPresetsDockVisibility()
{
    if (G::isLogger) G::log("MW::setPresetsDockVisibility");
    if (!presetsDock || !presetsDockVisibleAction) return;
    presetsDock->setVisible(presetsDockVisibleAction->isChecked());
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

void MW::closeThumbDock()
{
    thumbDock->setVisible(false);
    thumbDockVisibleAction->setChecked(false);
}

void MW::closeEmbelDock()
{
    embelDock->setVisible(false);
    embelDockVisibleAction->setChecked(false);
}
void MW::closeDevelopDock()
{
    developDock->setVisible(false);
    developDockVisibleAction->setChecked(false);
    closeHistoryDock();             // the three are one tool
    closePresetsDock();
}
void MW::closeHistoryDock()
{
    if (!historyDock || !historyDockVisibleAction) return;
    historyDock->setVisible(false);
    historyDockVisibleAction->setChecked(false);
}
void MW::closePresetsDock()
{
    if (!presetsDock || !presetsDockVisibleAction) return;
    presetsDock->setVisible(false);
    presetsDockVisibleAction->setChecked(false);
}
void MW::closeFolderDock()
{
    folderDock->setVisible(false);
    folderDockVisibleAction->setChecked(false);
}
void MW::closeFavDock()
{
    favDock->setVisible(false);
    favDockVisibleAction->setChecked(false);
}
void MW::closeFilterDock()
{
    filterDock->setVisible(false);
    filterDockVisibleAction->setChecked(false);
}
void MW::closeCatalogDock()
{
    if (!catalogDock) return;
    catalogDock->setVisible(false);
    catalogDockVisibleAction->setChecked(false);
}
void MW::closeMetadataDock()
{
    metadataDock->setVisible(false);
    metadataDockVisibleAction->setChecked(false);
}

void MW::showFolderDock()
{
    if (G::isLogger) G::log("MW::toggleFolderDockVisibility");
    qDebug() << "MW::toggleFolderDockVisibility";
    if (G::isInitializing) return;
    QDockWidget *dock = folderDock;
    if (isDockTabified(dock) && !isSelectedDockTab(dock)) dockOption = SetFocus;
    else dockOption = SetVisible;

    switch (dockOption) {
    case SetFocus:
        folderDock->raise();
        folderDockVisibleAction->setChecked(true);
        break;
    case SetVisible:
        folderDock->setVisible(true);
        folderDock->raise();
        folderDockVisibleAction->setChecked(true);
    }
}

void MW::showFavDock() {
    if (G::isLogger) G::log("MW::toggleFavDockVisibility");
    if (G::isInitializing) return;
    qDebug() << "MW::toggleFavDockVisibility";
    QDockWidget *dock = favDock;
    if (isDockTabified(dock) && !isSelectedDockTab(dock)) dockOption = SetFocus;
    else dockOption = SetVisible;

    switch (dockOption) {
    case SetFocus:
        favDock->raise();
        favDockVisibleAction->setChecked(true);
        break;
    case SetVisible:
        favDock->setVisible(true);
        favDock->raise();
        favDockVisibleAction->setChecked(true);
    }
}

/*  Push the catalogued image count onto both Catalog rows.

    The rows say how much is behind them, so "Catalog 84,102" answers "is there
    anything in there?" without opening the panel -- the discoverability point of
    putting the row in the tree at all. A count of -1 (index not open) shows the
    bare name: an unopened index and an empty one are different facts.
*/
void MW::updateCatalogScopeRows()
{
    if (G::isLogger) G::log("MW::updateCatalogScopeRows");
    const qint64 n = Catalog::instance().isAvailable()
                         ? static_cast<qint64>(Catalog::instance().count())
                         : -1;
    if (folderCatalogScopeRow) folderCatalogScopeRow->setImageCount(n);
    if (favCatalogScopeRow)    favCatalogScopeRow->setImageCount(n);

    /*  File > Open Catalog greys out when the index could not be opened, with the reason
        in its tooltip -- the same fact the panel's Catalog button used to carry by being
        disabled. Manage Catalog... stays enabled: choosing which folders are indexed and
        rescanning them is what the user would do about it. */
    const bool open = (n >= 0);
    if (openCatalogAction) {
        openCatalogAction->setEnabled(open);
        openCatalogAction->setToolTip(open
            ? tr("Browse and search every image Winnow has catalogued, including "
                 "folders that are not open.")
            : tr("The catalog is unavailable -- the local index database could not "
                 "be opened."));
        /*  The same property enableSelectionDependentMenus' gate() writes, so the
            Disabled Shortcut Feedback path can say why. Set here rather than there
            because this depends on the catalog, not on the selection. */
        openCatalogAction->setProperty("disabledReason",
            QString("the local index database could not be opened"));
    }
}

void MW::setScope(G::Scope s, QString src)
{
/*
    THE ONE PLACE G::scope CHANGES.

    Scope used to be private to the Find dock, and the Folders/Bookmarks trees knew
    nothing about it -- so selecting a folder and searching the catalog behaved like two
    different applications, and the catalog was reachable only by someone who already knew
    the panel existed. It is now one fact with three views (the Catalog row above each of
    the two trees, and the panel's Folders|Catalog buttons); every entry point routes here
    and this pushes the result back to all of them, so they cannot disagree.

    IT IS IDEMPOTENT AND RE-ENTRANT-SAFE. Pushing the state back sets widgets that emit on
    change, and those emissions come back here; the early return on "already in this
    scope" is what stops the loop, so it must stay ahead of everything else.
*/
    if (G::isLogger) G::log("MW::setScope", src);
    if (G::isInitializing) return;

    const bool changed = (G::scope != s);
    G::scope = s;

    /*  RE-ASSERT THE ROWS EVEN WHEN THE SCOPE DID NOT CHANGE. They are checkable
        QToolButtons, so clicking the Catalog row while Catalog is already current
        toggles it OFF before this runs -- and an early return would leave the row
        unchecked while the catalog is still the scope. setChecked emits toggled,
        not clicked, and the rows are connected on clicked, so putting them back
        cannot loop. */
    if (folderCatalogScopeRow) folderCatalogScopeRow->setChecked(s == G::Scope::Catalog);
    if (favCatalogScopeRow)    favCatalogScopeRow->setChecked(s == G::Scope::Catalog);

    if (!changed) return;

    /*  A folder stays SELECTED in the tree while the catalog is the scope. It is still
        the folder that is loaded, and clearing the selection would leave the user with no
        way back to it except by finding it again. The Catalog row being lit is what says
        which of the two is current. */

    if (s == G::Scope::Catalog) {
        // the panel is where a catalog scope is actually used, so bring it up
        if (G::useFindDock && findPanel) {
            filterDock->setVisible(true);
            filterDock->raise();
            filterDockVisibleAction->setChecked(true);
            findPanel->setScope(FindPanel::CatalogScope);
        }
    }
    else {
        if (G::useFindDock && findPanel)
            findPanel->setScope(FindPanel::FolderScope);
    }

    // the menu item reads as the scope, not as a panel
    if (catalogDockVisibleAction)
        catalogDockVisibleAction->setChecked(s == G::Scope::Catalog);
}

void MW::showCatalogDock()
/*
    "Search Catalog" (Shift+F2, Window > Catalog Panel).

    WITH THE FIND DOCK this is not a second panel but a SCOPE: show the Find dock, switch
    it to the Catalog scope and focus its box. That is what makes the F2 / Shift+F2
    pairing literally true -- the same box, the same words, a different set to ask.

    Focusing the box is the point of the shortcut either way: the user pressed it to
    search, and a panel that appears with the cursor somewhere else just asks them to
    click.
*/
{
    if (G::isLogger) G::log("MW::showCatalogDock");
    if (G::isInitializing) return;

    if (G::useFindDock) {
        if (!findPanel) return;
        filterDock->setVisible(true);
        filterDock->raise();
        filterDockVisibleAction->setChecked(true);
        setScope(G::Scope::Catalog, "MW::showCatalogDock");
        findPanel->focusSearch();
        return;
    }

    catalogDock->setVisible(true);
    catalogDock->raise();
    catalogDockVisibleAction->setChecked(true);
    catalogView->refresh();
    catalogView->focusSearch();
}

void MW::showFilterDock()
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

    QDockWidget *dock = filterDock;
    if (isDockTabified(dock) && !isSelectedDockTab(dock)) dockOption = SetFocus;
    else dockOption = SetVisible;
    qDebug() << "MW::toggleFilterDockVisibility dockToggle =" << dockOption;

    switch (dockOption) {
    case SetFocus:
        filterDock->raise();
        filterDockVisibleAction->setChecked(true);
        if (!filters->filtersBuilt) {
            buildFiltersWhenModelReady(dm->instance);
        }
        break;
    case SetVisible:
        filterDock->setVisible(true);
        filterDock->raise();
        filterDockVisibleAction->setChecked(true);
        if (!filters->filtersBuilt) {
            buildFiltersWhenModelReady(dm->instance);
        }
    }
}

void MW::showMetadataDock() {
    if (!G::useInfoView) return;
    if (G::isLogger) G::log("MW::toggleMetadataDockVisibility");
    if (G::isInitializing) return;
    QDockWidget *dock = metadataDock;
    if (isDockTabified(dock) && !isSelectedDockTab(dock)) dockOption = SetFocus;
    else dockOption = SetVisible;

    switch (dockOption) {
    case SetFocus:
        metadataDock->raise();
        metadataDockVisibleAction->setChecked(true);
        break;
    case SetVisible:
        metadataDock->setVisible(true);
        metadataDock->raise();
        metadataDockVisibleAction->setChecked(true);
    }
}

void MW::showThumbDock()
{
    if (G::isLogger) G::log("MW::toggleThumbDockVisibity");

    if (G::isInitializing) {
        G::popup->showPopup("Please wait until initialization is completed.", 2000);
        return;
    }

    QDockWidget *dock = thumbDock;
    if (isDockTabified(dock) && !isSelectedDockTab(dock)) dockOption = SetFocus;
    else dockOption = SetVisible;

    switch (dockOption) {
    case SetFocus:
        thumbDock->raise();
        thumbDockVisibleAction->setChecked(true);
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

void MW::showEmbelDock() {
    if (G::isLogger) G::log("MW::toggleEmbelDockVisibility");
    if (G::isInitializing) return;
    QDockWidget *dock = embelDock;
    if (isDockTabified(dock) && !isSelectedDockTab(dock)) dockOption = SetFocus;
    else dockOption = SetVisible;

    switch (dockOption) {
    case SetFocus:
        embelDock->raise();
        embelDockVisibleAction->setChecked(true);
        break;
    case SetVisible:
        embelDock->setVisible(true);
        embelDock->raise();
        embelDockVisibleAction->setChecked(true);
    }
}

void MW::showDevelopDock() {
    if (G::isLogger) G::log("MW::toggleDevelopDockVisibility");
    if (G::isInitializing) return;
    QDockWidget *dock = developDock;
    if (isDockTabified(dock) && !isSelectedDockTab(dock)) dockOption = SetFocus;
    else dockOption = SetVisible;

    /* Bring History + Presets up with Develop, BEFORE it: showing a tabified dock makes
       it the front tab, so they must be shown first and Develop raised last. */
    if (presetsDock && !presetsDock->isVisible()) {
        presetsDock->setVisible(true);
        presetsDockVisibleAction->setChecked(true);
    }
    if (historyDock && !historyDock->isVisible()) {
        historyDock->setVisible(true);
        historyDockVisibleAction->setChecked(true);
    }

    switch (dockOption) {
    case SetFocus:
        developDock->raise();
        developDockVisibleAction->setChecked(true);
        break;
    case SetVisible:
        developDock->setVisible(true);
        developDock->raise();
        developDockVisibleAction->setChecked(true);
    }
}

void MW::showHistoryDock() {
    if (G::isLogger) G::log("MW::showHistoryDock");
    if (G::isInitializing) return;
    QDockWidget *dock = historyDock;
    if (isDockTabified(dock) && !isSelectedDockTab(dock)) dockOption = SetFocus;
    else dockOption = SetVisible;

    switch (dockOption) {
    case SetFocus:
        historyDock->raise();
        historyDockVisibleAction->setChecked(true);
        break;
    case SetVisible:
        historyDock->setVisible(true);
        historyDock->raise();
        historyDockVisibleAction->setChecked(true);
    }
}

void MW::showPresetsDock() {
    if (G::isLogger) G::log("MW::showPresetsDock");
    if (G::isInitializing) return;
    QDockWidget *dock = presetsDock;
    if (isDockTabified(dock) && !isSelectedDockTab(dock)) dockOption = SetFocus;
    else dockOption = SetVisible;

    switch (dockOption) {
    case SetFocus:
        presetsDock->raise();
        presetsDockVisibleAction->setChecked(true);
        break;
    case SetVisible:
        presetsDock->setVisible(true);
        presetsDock->raise();
        presetsDockVisibleAction->setChecked(true);
    }
    /* raise() may not emit visibilityChanged (a tabified dock was already "visible"). */
    updateDevelopPresetBtn();
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
    /* Progress manages its own container visibility from its row content (see
       Progress::updateContainerVisibility), so nothing to toggle here. The cache
       rows are gated by the preference via setCacheProgressEnabled. */
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
            emit setValSf(sfRow, G::IngestedColumn, true, dm->instance,
                          "MW::setIngested", Qt::EditRole);
            emit setValSf(sfRow, G::PickColumn, "Ingested", dm->instance,
                          "MW::setIngested", Qt::EditRole);
            // update pickLog
            sKey.replace("/", "🔸");
                              settings->setValue(sKey, "ingested");
        }
    }
    settings->endGroup();

    // update filter counts
    buildFilters->updateCategory(BuildFilters::PickEdit, BuildFilters::NoAfterAction);

    // picks are now "Ingested"; disable ingest if nothing left picked
    updatePickDependentActions();
}

void MW::setCombineRawJpg()
{
    if (G::isLogger)
        G::log("MW::setCombineRawJpg");

    /* Block toggling only while a folder is loading (rows exist but metadata is
       still being read); toggling is allowed when no folder is loaded.  The trigger
       (menu action or status-bar button) has already flipped combineRawJpgAction's
       checked state, so restore it to match combineRawJpg, keeping the two in sync;
       otherwise the action desyncs from the flag and a later click is wasted
       re-aligning them instead of toggling. */
    if (dm->rowCount() && !G::allMetadataAttempted) {
        combineRawJpgAction->setChecked(combineRawJpg);
        QString msg = "Folder is still loading.  Try again when the folder has loaded.";
        G::popup->showPopup(msg, 2000);
        updateStatusBar();
        return;
    }

    // flag used in MW, dm and sf, fsTree, bookmarks
    combineRawJpg = combineRawJpgAction->isChecked();
    settings->setValue("combineRawJpg", combineRawJpg);
    G::combineRawJpg = combineRawJpg;
    dm->sf->combineRawJpg = combineRawJpg;
    fsTree->combineRawJpg = combineRawJpg;
    bookmarks->combineRawJpg = combineRawJpg;

    if (!dm->rowCount()) {
        /* No folder loaded: record the setting and refresh the folder/bookmark image
           counts so they reflect the new pairing; skip the datamodel/proxy rebuild. */
        fsTree->refreshModel();
        refreshBookmarks();
        updateStatusBar();
        return;
    }

    QString msg;
    if (combineRawJpg) msg = "Combining Raw + Jpg pairs.  This could take a moment.";
    else msg = "Separating Raw + Jpg pairs.  This could take a moment.";
    G::popup->showPopup(msg);
    if (G::useProcessEvents) qApp->processEvents();

    // prevent crash when there are videos (did not work)
    // stop();

    updateStatusBar();

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
                             dm->instance, src, Qt::EditRole);
           }
           else {
               emit setValDm(dmRow, G::TypeColumn, "JPG",
                             dm->instance, src, Qt::EditRole);
           }
       }
   }

   // update elements available to sort and filter
   dm->rebuildTypeFilter();

   // redo the filter to either combine or separate the raw and jpg files
   filterChange("MW::setCombineRawJpg");

   /* The proxy baseline changed (raw+jpg pairs collapsed or expanded) without a
      Filters-tree change, so the unfiltered counts must be recomputed as well as the
      filtered counts.  filterChange only refreshes filtered counts. */
   buildFilters->updateAllCounts();

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
    QString srcFun = "MW::refreshViewsOnCacheChange";

    int sfRow = dm->proxyRowFromPath(fPath, "MW::refreshViewsOnCacheChange");

    if (sfRow == -1) {
        QString msg = "No sfRow for fPath = " + fPath;
        qWarning() << "WARNING:" << srcFun << msg;
        return;
    }

    /* Compare by path, not row, to decide whether the loupe must reload. MW::refresh()
       can re-sort the proxy and recompute dm->currentSfRow (via currentDmIdx), leaving
       it off by one relative to the row that was just (re)cached. currentFilePath tracks
       the selected image and is not disturbed by that re-sort, so a same-path content
       change (e.g. a re-embellished image already in the folder) is correctly seen as
       the current image and the loupe is refreshed. */
    bool isCurrent = (fPath == dm->currentFilePath);
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
        applyDevelopPreviewIfEdited();   // overlay saved develop edits once the decode is cached
        updateClassification();
        /* Hide Info until first image shown.  If Winnow is interrupted by an OS
           permission request then we do not want an info to show */
        if (isFirstImageSelected) {
            // qDebug() << fun << "isFirstImageSelected =" << isFirstImageSelected;
            setShootingInfoVisibility();
            isFirstImageSelected = false;
        }
    }

    thumbView->refreshIcon(sfIdx, srcFun);
    gridView->refreshIcon(sfIdx, srcFun);

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

void MW::updateSidecarStatus(QString fPath)
{
    QString srcFun = "MW::updateSidecarStatus";
    if (G::isLogger) G::log(srcFun, fPath);
    qDebug() << srcFun<< fPath;

    QModelIndex sfIdx = dm->proxyIndexFromPath(fPath);
    emit  setValSf(sfIdx.row(), G::SidecarColumn, true, G::dmInstance,
                  srcFun, Qt::EditRole);
    thumbView->refreshIcon(sfIdx, srcFun);
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
