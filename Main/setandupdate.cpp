#include "Main/mainwindow.h"
#include "Utilities/panelprobe.h"

void MW::setCentralMessage(QString message)
{
    IngestProbe::Scope _ip("MW::setCentralMessage");
    QString fun = "MW::setCentralMessage";
    if (G::isLogger) G::log(fun, message);
    centralLayout->setCurrentIndex(MessageTab);
    msg.msgLabel->setText(message);
    centralLayout->currentWidget()->repaint();
}

void MW::setCentralProgressMessage(QString message)
{
/*
    Report a load stage in the central widget, but only while the message pane is what
    the user is looking at.

    THE LOAD IS NOT OVER WHEN THE IMAGES ARE COUNTED IN. After the last row is inserted
    there is still the folder collation, the proxy sort, and the filter build -- seconds
    of it on a catalog scope of tens of thousands of rows -- and until now the central
    widget sat on "n of n images loading..." for all of it, which reads as a hang. Each
    of those stages says what it is doing through here.

    IT MUST NOT STEAL THE VIEW BACK. The image cache switches the central widget to the
    loupe the moment the first image is cached (MW::refreshViewsOnCacheChange), and that
    can happen while the filters are still being built. setCentralMessage would put the
    message pane back over the picture the user is finally looking at, so a stage that
    reports itself after the switch is silently dropped instead.
*/
    if (G::isLogger) G::log("MW::setCentralProgressMessage", message);
    if (centralLayout->currentIndex() != MessageTab) return;
    setCentralMessage(message);
}

QString MW::loadedMsg() const
{
    return QString::number(dm->rowCount()) + " images loaded.\n\n";
}

bool MW::showCentralMessageIfNoImages()
{
/*
    Reinstate the central message when there is nothing to show.

    The view keys (E / G / T) are also the Source workspace keys, so they work with no
    folder loaded -- that is the only way back out of the Develop layout when the user
    presses D before selecting a folder (see MW::enableSelectionDependentMenus).  Each
    view switch points centralLayout at its own tab though, which would replace the
    "select a folder" message with an empty view, so the message is put back here.
    Returns true when a message was shown.
*/
    if (G::isLogger) G::log("MW::showCentralMessageIfNoImages");
    if (dm->sf->rowCount() > 0) return false;

    /* Same wording as MW::nullFiltration, which reports the empty cases when the
       filtration changes. */
    QString text;
    if (dm->folderList.count() == 0)
        text = "Select from the Source or Bookmarks panels.";
    else if (dm->rowCount())
        text = "No images match the filtration.";
    else if (dm->folderList.count() == 1)
        text = "No images in the folder.";
    else
        text = "No images in the folders.";

    setCentralMessage(text);
    return true;
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
        /*  THE ONE PLACE THAT DECIDES THE THUMB STRIP HEIGHT.  Recorded as a request
            rather than a result, because resizeDocks is a request: QDockAreaLayout may
            refuse it outright when the area has no room, and the probe's settled snapshot
            on the next turn is what says whether it took.  See Utilities/panelprobe.h. */
        if (G::isPanelProbe) {
            PanelProbe &probe = PanelProbe::Instance();
            probe.NoteConstraint("thumbView", "setMin/MaximumHeight", minHt, maxHt);
            probe.NoteThumbFit(cellHt, thumbView->viewport()->height(), thumbDock->height(),
                               minHt, maxHt, "setThumbDockFeatures before resizeDocks");
            probe.NoteRequest("ThumbDock", "resizeDocks vertical", newThumbDockHeight);
        }
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
    /* No separate dock in Filter mode -- the Catalog is a scope of the Filter panel. */
    if (!catalogDock) return;
    catalogDock->setVisible(catalogDockVisibleAction->isChecked());
}

void MW::setKeywordsDockVisibility()
{
    if (G::isLogger) G::log("MW::setKeywordsDockVisibility");
    if (!keywordsDock) return;
    keywordsDock->setVisible(keywordsDockVisibleAction->isChecked());
    /*  THE ROUTE THAT USED TO MISS. A dock restored VISIBLE from settings never goes
        through showKeywordsDock -- the user did not choose it this session, the saved
        state did -- so nothing loaded the vocabulary and the panel came up empty. */
    if (keywordsDockVisibleAction->isChecked()) ensureKeywordVocabLoaded();
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
    /* The History panel is part of the Develop tool -- it follows the Develop dock,
       action and all, so a later setHistoryDockVisibility() agrees rather than fighting
       it. Shown before Develop so Develop, not History, is the front tab of the group. */
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
    /* The bottom show/hide bar outranks the action here: while it has the area
       collapsed the thumbnails stay down, or every caller of this (a workspace change,
       a view-mode change) would silently undo the bar's click. Clicking the bar again
       re-shows whatever the action then says. */
    if (!isDockAreaCollapsed(Qt::BottomDockWidgetArea))
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
    closeHistoryDock();             // the two are one tool
}
void MW::closeHistoryDock()
{
    if (!historyDock || !historyDockVisibleAction) return;
    historyDock->setVisible(false);
    historyDockVisibleAction->setChecked(false);
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
void MW::closeKeywordsDock()
{
    if (!keywordsDock) return;
    keywordsDock->setVisible(false);
    keywordsDockVisibleAction->setChecked(false);
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

/*  Push the Library into LibTree: its folders, their counts, and which are offline.

    The folders are a GROUP BY over every catalogued image, and whether an anchor's volume
    is mounted is a stat that can stall on a dead network mount -- so both are read OFF
    THE GUI THREAD. One read at a time: a burst of catalog commits during a folder load
    asks for this once per commit, and they would all return the same answer, so a
    request that arrives while one is in flight just asks for one more pass afterwards.
*/
void MW::updateLibraryTree()
{
    if (G::isLogger) G::log("MW::updateLibraryTree");
    const qint64 n = Catalog::instance().isAvailable()
                         ? static_cast<qint64>(Catalog::instance().count())
                         : -1;

    if (libTree) {
        if (n < 0) libTree->setSources({}, -1);
        else if (libraryTreePending) libraryTreeAgain = true;
        else {
            libraryTreePending = true;
            QStringList anchors;
            for (const CatalogScopeEntry &e : catalogScopeEffective(true)) anchors << e.path;
            QPointer<MW> self(this);
            QThreadPool::globalInstance()->start([self, anchors] {
                LibrarySource src;
                src.anchors = anchors;
                src.folderCounts = Catalog::instance().liveFolderCounts();
                for (const QString &a : anchors)
                    if (!QFileInfo::exists(a)) src.offlineAnchors << a;
                /*  THE TOTAL IS THE SUM OF WHAT IS SHOWN, so the Library row adds up to
                    its folders. Catalog::count() includes demoted rows, which the tree
                    deliberately leaves out. */
                qint64 total = 0;
                for (int c : src.folderCounts) total += c;
                if (!self) return;
                QMetaObject::invokeMethod(self, [self, src, total] {
                    if (!self || !self->libTree) return;
                    self->libraryTreePending = false;
                    self->libTree->setSources({src}, total);
                    if (self->libraryTreeAgain) {
                        self->libraryTreeAgain = false;
                        self->updateLibraryTree();
                    }
                }, Qt::QueuedConnection);
            });
        }
    }

    /*  File > Open Library greys out when the index could not be opened, with the reason
        in its tooltip. Manage Catalog... stays enabled: choosing which folders are
        indexed and rescanning them is what the user would do about it. */
    const bool open = (n >= 0);
    if (openCatalogAction) {
        openCatalogAction->setEnabled(open);
        /*  ENABLED BUT EMPTY IS ITS OWN STATE, and the tooltip says so rather than
            promising a search of images that are not there. The command still works --
            it opens Manage Catalog, which is where an empty catalog is filled -- so
            disabling it would close the only door to the fix. */
        openCatalogAction->setToolTip(
            !open ? tr("The catalog is unavailable -- the local index database could not "
                       "be opened.")
            : n == 0 ? tr("Nothing is catalogued yet. Opens Manage Catalog, where you "
                          "choose which folders Winnow indexes.")
                     : tr("Browse the whole Library: every image Winnow has "
                          "catalogued, including folders that are not open."));
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

    Scope used to be private to the Filter dock, and the Folders/Bookmarks trees knew
    nothing about it -- so selecting a folder and searching the catalog behaved like two
    different applications, and the catalog was reachable only by someone who already knew
    the panel existed. It is now one fact with its views -- the Source panel's Folders |
    Library toggle and the tree beneath it, and the Filters title; every entry point
    routes here and this pushes the result back to all of them, so they cannot disagree.

    IT IS IDEMPOTENT AND RE-ENTRANT-SAFE. Pushing the state back sets widgets that emit on
    change, and those emissions come back here; the early return on "already in this
    scope" is what stops the loop, so it must stay ahead of everything else.
*/
    if (G::isLogger) G::log("MW::setScope", src);
    if (G::isInitializing) return;

    const bool changed = (G::scope != s);
    /*  Leaving the Library: remember how it was left, while it is still what is loaded. */
    if (changed && G::scope == G::Scope::Catalog) {
        saveLibraryState();
        /*  A Library filter restore still waiting for its build must not land on the
            folder being switched to. */
        if (libraryFilterRestorePending) {
            libraryFilterRestorePending = false;
            restoreFiltersPending = false;
        }
        libraryRestoreSortPending = false;
    }
    G::scope = s;

    if (G::isPerfProbe && changed && s == G::Scope::Catalog) {
        catalogSwitchClock.start();
        qDebug().noquote() << "[PERF] setScope(Catalog) START  rows loaded ="
                           << dm->rowCount() << " src =" << src
                           << " a11yActive =" << QAccessible::isActive();
    }

    const bool isLibrary = (s == G::Scope::Catalog);

    /*  REMEMBER WHAT THE FOLDERS VIEW HAD LOADED, so the toggle's Folders button can go
        back to it. Read from the model's own request -- the folder and whether its
        subtree came too -- before the Library replaces it. */
    if (changed && isLibrary && dm->scopeRequest().scope == G::Scope::Folders
        && !dm->scopeRequest().query.folder.isEmpty()) {
        lastFolderPath = dm->scopeRequest().query.folder;
        lastFolderRecurse = dm->scopeRequest().recurse;
    }

    /*  THE TOGGLE AND THE TREE BENEATH IT, re-asserted even when the scope did not
        change, so a click the scope refused (Library with nothing catalogued) puts the
        toggle back. Signals blocked: the buttons act on clicked, and this is not one. */
    if (sourceFoldersBtn && sourceLibraryBtn) {
        QSignalBlocker a(sourceFoldersBtn), b(sourceLibraryBtn);
        sourceLibraryBtn->setChecked(isLibrary);
        sourceFoldersBtn->setChecked(!isLibrary);
    }
    if (sourceStack && libTree)
        sourceStack->setCurrentWidget(isLibrary ? static_cast<QWidget *>(libTree)
                                                : static_cast<QWidget *>(fsTree));

    /*  WHERE THE FILTERS FOLDERS TREE STARTS: the catalog's include folders in the
        Library, so it reads like LibTree; in Folders, the folder that was loaded (the
        deepest folder the rows share). Pushed before the load its build follows. */
    if (filters) {
        QStringList anchors;
        if (isLibrary)
            for (const CatalogScopeEntry &e : catalogScopeEffective(true)) anchors << e.path;
        filters->setFolderAnchors(anchors);
    }
    if (!isLibrary) pendingLibraryFolderFilter = PendingFolderFilter();
    /*  Entering the Library re-reads its folders: a file moved or a folder renamed since
        the last read is otherwise shown where it was until the next scan. */
    if (changed && isLibrary) updateLibraryTree();

    /*  THE FOLDERS TREE SHOWS NO SELECTION WHILE THE LIBRARY IS THE SCOPE. It is out of
        sight behind LibTree then, but a lit folder waiting there would read as a second
        scope the moment the toggle went back -- the confusion the single scope was
        introduced to end. Clearing here rather than in the "changed" branch below so a
        folder selected in between (Reveal in Folders) is cleared too.

        Clearing the selection does not unload the folder: FSTree::selectionChanged only
        reschedules the folder watch, and the datamodel keeps what it has (dm->folderList
        is what a return to Folders scope re-selects from). */
    if (s == G::Scope::Catalog && fsTree && fsTree->selectionModel())
        fsTree->selectionModel()->clearSelection();

    /*  ASKING TO SEE THE LIBRARY IS ASKING WHETHER IT IS STILL TRUE. Folders appear and
        images are deleted outside Winnow, and the index only learns of it when somebody
        presses Scan. Here, where the user has just said "show me all of it", is the one
        place that is worth checking without being asked -- so the scan runs itself,
        throttled. See MW::maybeAutoScanCatalog, which declines far more often than it
        acts, and which defers the work so this selection does not wait on it.

        AHEAD OF THE "not changed" RETURN BELOW, deliberately: selecting Library again
        after browsing is exactly when a user wants it rechecked, and the throttle -- not
        whether the scope moved -- is what decides. */
    /*  `changed` is what says a load is coming: an unchanged scope re-asserts the panel
        state and loads nothing, so there would be no folderChangeCompleted to wait for. */
    if (s == G::Scope::Catalog) maybeAutoScanCatalog("MW::setScope", /*loadExpected*/ changed);

    /*  WHICH SET THE PANEL IS FILTERING, said where it stays visible. The Search category
        can be collapsed and the query is typed into a tree row, so there is no
        placeholder to carry it; the dock title is the one part of the panel that is
        always on screen. Re-asserted even when the scope did not change, for the same
        reason the toggle above is. */
    if (G::useFilterPanel && filterTitleBar)
        filterTitleBar->setTitle(s == G::Scope::Catalog ? "Filters (Library)"
                                                        : "Filters (Folders)");

    if (!changed) return;

    if (s == G::Scope::Catalog) {
        // the panel is where a catalog scope is actually used, so bring it up
        if (G::useFilterPanel && filterPanel) {
            filterDock->setVisible(true);
            filterDock->raise();
            filterDockVisibleAction->setChecked(true);
            filterPanel->setScope(FilterPanel::CatalogScope);
        }
    }
    else {
        if (G::useFilterPanel && filterPanel)
            filterPanel->setScope(FilterPanel::FolderScope);
    }

    // the menu item reads as the scope, not as a panel
    if (catalogDockVisibleAction)
        catalogDockVisibleAction->setChecked(s == G::Scope::Catalog);
}

void MW::setCatalogScopeWhole(QString src)
{
/*
    The WHOLE Library -- the toggle's Library button, File > Open Library, and "open the
    library at start". Nothing catalogued opens Manage Catalog instead, because an empty
    Library is a question, not a result (MW::catalogEmptyOpenManage).
*/
    if (G::isLogger) G::log("MW::setCatalogScopeWhole", src);
    if (catalogEmptyOpenManage(src)) return;
    setScope(G::Scope::Catalog, src);
}

void MW::showFoldersSource()
{
/*
    The toggle's Folders button. Back to the folder the Folders view had loaded before
    the Library was chosen, through FSTree::select -- the ordinary folder load, with its
    ordinary guards: un-ingested picks are asked about, and a refusal leaves the Library
    showing (setScope pushes the toggle back).

    WITH NOTHING TO GO BACK TO the Folders view comes up empty, waiting for a folder to be
    picked, rather than leaving the Library's rows on screen under a toggle that says
    Folders -- the toggle has to describe what is loaded.
*/
    if (G::isLogger) G::log("MW::showFoldersSource");
    if (G::scope == G::Scope::Folders) return;
    pendingLibraryFolderFilter = PendingFolderFilter();

    if (!lastFolderPath.isEmpty() && QFileInfo(lastFolderPath).isDir()) {
        if (!fsTree->select(lastFolderPath, lastFolderRecurse ? "Recurse" : "None",
                            "MW::showFoldersSource")) {
            setScope(G::scope, "MW::showFoldersSource refused");
        }
        return;
    }

    if (!okToDiscardPicks("Leaving the Library", "Leave the Library")) {
        setScope(G::scope, "MW::showFoldersSource refused");
        return;
    }
    stop("MW::showFoldersSource");
    setScope(G::Scope::Folders, "MW::showFoldersSource");
    setCentralMessage(tr("Select a folder."));
}

void MW::applyLibraryFolderFilter(const QStringList &includes, const QStringList &excludes)
{
/*
    A LibTree click. The Library is already loaded, so this FILTERS it: it sets the
    Filters panel's Folders category, which is the one folder filter, and LibTree is then
    re-synced from it like after any other filter change.

    A CLICK DURING THE LOAD WAITS. The Folders category is rebuilt from what lands, and
    BuildFilters::build runs on a THREAD, so checking items now would check ones about to
    be deleted. The request is kept and applied on BuildFilters::finishedBuildFilters --
    the same lesson the Catalog tree's year rows learned, where applying straight after
    asking for the build checked nothing at all.
*/
    if (G::isLogger) G::log("MW::applyLibraryFolderFilter");
    if (G::isInitializing || !filters) return;

    pendingLibraryFolderFilter = {true, includes, excludes};
    if (G::scope != G::Scope::Catalog) {
        setCatalogScopeWhole("LibTree");
        // nothing catalogued: Manage Catalog opened instead, and nothing is waiting
        if (G::scope != G::Scope::Catalog) pendingLibraryFolderFilter = PendingFolderFilter();
        return;
    }
    if (filters->filtersBuilt && !G::isModifyingDatamodel && !filters->buildingFilters) {
        applyPendingLibraryFolderFilter();
        return;
    }
    // a build is coming, and finishedBuildFilters applies the click
    if (G::isModifyingDatamodel || filters->buildingFilters) return;

    /*  NO BUILD IS COMING when the Filters panel is hidden: the filters are built lazily,
        only while it is on screen (see buildFiltersWhenModelReady), so the click would
        wait for ever with nothing to show for it. The folder filter lives in that panel,
        so it is brought up -- unless it shares a tab with the Source panel, where raising
        it would hide the very tree that was clicked. Then the click waits for the panel,
        and says so. */
    if (!filterDock->isVisible()) {
        if (tabifiedDockWidgets(folderDock).contains(filterDock)) {
            if (G::popup)
                G::popup->showPopup(tr("Open the Filters panel to filter the Library by "
                                       "folder."), 3000);
            return;
        }
        filterDock->setVisible(true);
        filterDockVisibleAction->setChecked(true);
    }
    buildFiltersWhenModelReady(dm->instance);
}

void MW::applyPendingLibraryFolderFilter()
{
    if (!pendingLibraryFolderFilter.pending || !filters) return;
    if (G::scope != G::Scope::Catalog) {
        pendingLibraryFolderFilter = PendingFolderFilter();
        return;
    }
    QStringList missing;
    if (!filters->setFolderFilter(pendingLibraryFolderFilter.includes,
                                  pendingLibraryFolderFilter.excludes, &missing)) {
        return;                                 // not built yet: still pending
    }
    pendingLibraryFolderFilter = PendingFolderFilter();
    syncLibTreeFromFilters();
    /*  A folder the Library lists and nothing loaded holds -- every image in it is a raw
        hidden behind its JPG, say -- would otherwise be a click that silently did
        nothing. */
    if (!missing.isEmpty() && G::popup)
        G::popup->showPopup(tr("No loaded images are in %1.")
                                .arg(QDir::toNativeSeparators(missing.first())));
}

void MW::restoreFiltersAfterFolderChange()
{
/*
    See MW::folderSelectionChange. A check whose value is no longer in the set (the
    removed folder, say) simply does not come back -- Filters::restore finds no item for
    it, which is the right answer.

    RE-APPLIED WHENEVER ANYTHING HAD BEEN CHECKED, not only when something still is: the
    proxy's compiled predicate holds the pre-rebuild checks until a filterChange
    recompiles it, so a check that did not come back would otherwise keep filtering.
*/
    if (!restoreFiltersPending || !filters) return;
    restoreFiltersPending = false;
    libraryFilterRestorePending = false;
    const bool hadChecks = filters->hasSavedStates();
    filters->restore();
    if (hadChecks || filters->isAnyFilter())
        filterChange("MW::restoreFiltersAfterFolderChange");
}

void MW::saveLibraryState()
{
/*
    Remember how the Library was left -- its sort and its filters -- for the next start
    (see restoreLibraryState). Called on leaving the Library for Folders and at quit;
    only while the Library is what is loaded, so a quit from Folders keeps the state the
    Library was last left in. The filter state is Filters::persistableState: checked
    items by {category, value, include/exclude}, the search text, the keyword any/all.
*/
    /*  --perfprobe: what is saved, or why nothing is. This feature fails SILENTLY -- the
        Library just opens the ordinary way -- so the line is the only evidence. */
    if (G::isPerfProbe) {
        const QVariantMap fp = filters ? filters->persistableState() : QVariantMap();
        qDebug().noquote() << "[PERF] saveLibraryState  enabled =" << restoreLibraryState
                           << " scopeIsCatalog =" << (G::scope == G::Scope::Catalog)
                           << " rows =" << (dm ? dm->rowCount() : -1)
                           << " sortColumn =" << sortColumn
                           << " reverse =" << sortReverseAction->isChecked()
                           << " items =" << fp.value("items").toStringList()
                                                .replaceInStrings(QChar(0x1f), "|");
    }
    if (!restoreLibraryState || G::scope != G::Scope::Catalog || !filters) return;
    /*  NEVER AN EMPTY LIBRARY. Once closeEvent's teardown has cleared the model and
        reset the filters and the sort, a second save (a second close, or anything else
        that reaches here late) would record THAT -- File Name, ascending, nothing
        checked -- over the state the user left, which is what the first end-to-end test
        of this found in settings.ini. A Library with no rows is not a state to reopen. */
    if (!dm || dm->rowCount() == 0) return;
    settings->beginGroup("LibraryState");
    settings->setValue("sortColumn", sortColumn);
    settings->setValue("isReverseSort", sortReverseAction->isChecked());
    settings->setValue("filters", filters->persistableState());
    settings->endGroup();
}

void MW::queueLibraryStateRestore()
{
/*
    At start, before the Library loads: read the saved state and queue both halves.

    THE SORT is applied by applyRestoredLibrarySort from metadataComplete, AFTER that
    function's updateSortColumn(G::NameColumn) -- which resets the sort menu to File Name
    on every completed load. Applied any earlier, the grid was sorted but the menu and
    sortColumn went back to File Name, and the next filter change re-sorted by name (the
    first cut of this did exactly that).

    THE FILTERS go to the after-build path a folder change uses (restoreFiltersPending ->
    MW::restoreFiltersAfterFolderChange -> Filters::restore + one filterChange). That path
    runs when the filter BUILD finishes, and the build only runs while the Filters panel
    is showing -- so a restored filter is never applied where it cannot be seen: with
    the panel hidden it lands the moment the panel is opened. (The first cut tested the
    dock's visibility HERE, in showEvent, where it is not yet settled, and so restored
    nothing.)
*/
    if (!settings->childGroups().contains("LibraryState")) return;
    settings->beginGroup("LibraryState");
    const int col = settings->value("sortColumn", G::NameColumn).toInt();
    const bool reverse = settings->value("isReverseSort", false).toBool();
    const QVariantMap f = settings->value("filters").toMap();
    settings->endGroup();
    if (G::isPerfProbe)
        qDebug().noquote() << "[PERF] queueLibraryStateRestore  sortColumn =" << col
                           << " reverse =" << reverse << " items ="
                           << f.value("items").toStringList()
                                  .replaceInStrings(QChar(0x1f), "|");

    if (col > 0 && col < G::TotalColumns) {
        libraryRestoreSortColumn = col;
        libraryRestoreReverse = reverse;
        libraryRestoreSortPending = true;
    }
    if (filters && !f.value("items").toStringList().isEmpty()) {
        filters->setStateToRestore(f);
        restoreFiltersPending = true;
        libraryFilterRestorePending = true;
    }
}

void MW::applyRestoredLibrarySort()
{
/*
    The restored sort, once per start, from metadataComplete after its reset of the sort
    menu to File Name: put the saved column and direction on the menu and in sortColumn,
    then sort. A filter restore that runs later finds the proxy already sorted by the
    same column and does not sort again.
*/
    if (!libraryRestoreSortPending) return;
    libraryRestoreSortPending = false;
    if (G::isPerfProbe)
        qDebug().noquote() << "[PERF] applyRestoredLibrarySort  column ="
                           << libraryRestoreSortColumn << " reverse =" << libraryRestoreReverse;
    if (G::scope != G::Scope::Catalog || !dm->rowCount()) return;
    sortColumn = libraryRestoreSortColumn;
    updateSortColumn(sortColumn);
    if (libraryRestoreReverse != sortReverseAction->isChecked())
        toggleSortDirection(libraryRestoreReverse ? Tog::on : Tog::off);
    sortChange("MW::applyRestoredLibrarySort");
}

void MW::applyDeferredSort()
{
/*
    See MW::sortChange. Put the deferred column and direction back on the menu and in
    sortColumn -- metadataComplete has just reset them to File Name -- and sort.
*/
    if (!sortDeferredForMetadata) return;
    if (!G::allMetadataAttempted) return;               // still not ready: keep it
    sortDeferredForMetadata = false;
    if (deferredSortColumn < 0 || deferredSortColumn >= G::TotalColumns) return;
    sortColumn = deferredSortColumn;
    updateSortColumn(sortColumn);
    if (deferredReverseSort != isReverseSort)
        toggleSortDirection(deferredReverseSort ? Tog::on : Tog::off);
    sortChange("MW::applyDeferredSort");
}

void MW::syncLibTreeFromFilters()
{
    if (!libTree || !filters) return;
    QStringList inc, exc;
    filters->folderFilterState(inc, exc);
    libTree->syncFromFilters(inc, exc);
}

void MW::showKeywordsDock()
{
/*
    Window > Keywords Panel. Shows it, raises it to the front of its tab group and loads
    the vocabulary -- reloading each time it is shown rather than on a timer, because the
    tree only changes when the user changes it or an import does, and a panel that is
    never open costs nothing to keep current.
*/
    if (G::isLogger) G::log("MW::showKeywordsDock");
    if (G::isInitializing) return;
    if (!keywordsDock) return;

    const bool wanted = keywordsDockVisibleAction->isChecked();
    keywordsDock->setVisible(wanted);
    if (!wanted) return;

    keywordsDock->raise();
    ensureKeywordVocabLoaded();
    /*  The dock was hidden, so the selection-driven refresh has been skipping it; fill
        the tags and the dots now that it is on screen. */
    refreshKeywordsDock();
}

void MW::showCatalogDock()
/*
    "Search Catalog" (Window > Search Catalog, File > Open Catalog / Shift+O).

    WITH THE FILTER DOCK this is not a second panel but a SCOPE: show the Filter dock,
    switch it to the Catalog scope and focus its box -- the same box the Folders scope
    types into, the same words, a different set to ask.

    Focusing the box is the point of the command: the user chose it to search, and a panel
    that appears with the cursor somewhere else just asks them to click. F2 then re-opens
    that box for as long as the Catalog stays the scope.
*/
{
    if (G::isLogger) G::log("MW::showCatalogDock");
    if (G::isInitializing) return;

    /*  Nothing catalogued: open the window that fills it rather than a search box that
        can only come back empty. See MW::catalogEmptyOpenManage. */
    if (catalogEmptyOpenManage("MW::showCatalogDock")) return;

    if (G::useFilterPanel) {
        if (!filterPanel) return;
        filterDock->setVisible(true);
        filterDock->raise();
        filterDockVisibleAction->setChecked(true);
        setScope(G::Scope::Catalog, "MW::showCatalogDock");
        filterPanel->focusSearch();
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

    /* Bring History up with Develop, BEFORE it: showing a tabified dock makes it the
       front tab, so it must be shown first and Develop raised last. */
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
    /* "H" means "show me the history", so open the section if it was folded away. */
    if (historyPanel) historyPanel->expandHistory();
}

void MW::showPresetsDock() {
/*
    "P", the View menu's Presets item and the Develop action row's colour-wheel button.
    Presets is a SECTION of the History panel, not a dock of its own, so this raises that
    panel and opens the section.
*/
    if (G::isLogger) G::log("MW::showPresetsDock");
    if (G::isInitializing) return;
    QDockWidget *dock = historyDock;
    if (!dock) return;
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
    if (historyPanel) historyPanel->expandPresets();
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
    IngestProbe::Scope _ip("MW::refreshViewsOnCacheChange");
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
        /*  THE OTHER END OF A LOUPE MISS. fileSelectionChange left the loupe blank
            because the image was not cached; this is the moment it is filled in, and the
            gap between the two is exactly how long the user looked at nothing. */
        if (G::isIngestProbe) IngestProbe::Instance().NoteLoupeRepair(fPath);
        centralLayout->setCurrentIndex(prevCentralView);
        imageView->loadImage(fPath, true, "MW::refreshViewsOnCacheChange");
        /*  DON'T FLASH THE UNDEVELOPED IMAGE. loadImage above paints the decode -- the
            picture WITHOUT the saved recipe. In Develop that is the one thing the user
            has already decided to change, and applyDevelopPreviewIfEdited below only
            SCHEDULES the render that fixes it, so the naked decode owned the loupe for
            the seconds that render took. A cropped recipe made it obvious: the frame
            jumped back to the original aspect and then snapped to the crop again.

            The cached develop preview is that render's result from last time and is
            already on disk, so paint it over the decode as an interim and let the render
            replace it. Repainting rather than skipping loadImage keeps its bookkeeping
            (isLoaded, currentImagePath, the captured zoom/pan) intact, and both paints
            happen in this one slot call, so the decode never reaches the screen.

            Same substitution, and the same reasoning, as the Develop entry hook in
            MW::setOperationMode and the cache-miss branch in MW::fileSelectionChange. */
        if (currentDevelopEditsVisible()) {
            const QImage cachedPreview = devPreview(fPath);
            if (!cachedPreview.isNull() && imageView->loadImageInterim(fPath, cachedPreview))
                developInterimIsDevPreview = true;
        }
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
    IngestProbe::Scope _ip("MW::updateClassification");
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
/*
    A sidecar now exists for fPath. Record it on the row.

    THE DATAMODEL ROW, NOT THE PROXY ROW. This mapped the path through the PROXY and wrote
    with setValSf, so an image the current filter excludes -- which is exactly the case
    when the user is tagging inside a filtered set -- mapped to row -1 and the write went
    nowhere. The flag then stayed false with a sidecar on disk, and
    DataModel::catalogRowFor reported sidecarMtime = 0, which is enough to make the
    catalog skip the row forever (see Metadata::writeXMP). The datamodel row exists
    whether or not the proxy is showing it.

    The icon refresh still goes through the proxy, because that is what a VIEW draws, and
    an invalid index there simply means there is nothing on screen to repaint.
*/
    IngestProbe::Scope _ip("MW::updateSidecarStatus");
    QString srcFun = "MW::updateSidecarStatus";
    if (G::isLogger) G::log(srcFun, fPath);

    const int dmRow = dm->rowFromPath(fPath);
    if (dmRow < 0) return;
    emit setValDm(dmRow, G::SidecarColumn, true, dm->instance, srcFun, Qt::EditRole);

    const QModelIndex sfIdx = dm->proxyIndexFromPath(fPath);
    if (sfIdx.isValid()) thumbView->refreshIcon(sfIdx, srcFun);
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
