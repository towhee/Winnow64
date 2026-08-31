#include "Main/mainwindow.h"

/*  *******************************************************************************************

    WORKSPACES

    Need to track:
        - workspace number (n) for shortcut, QSettings name
        - workspace menu description
        - workspace geometry
        - workspace state
        - workspace dock visibility and lock mode
        - thumb parameters (size, spacing, padding, label)

    The user can change the workspace menu name, reassign a menu item and delete
    menu items.

    The data for each workspace is held in a workspaceData struct.  Up to 10
    workspaces are contained in QList<workspaceData> workspaces.

    Read an item:  QString name = workspaces->at(n).name;
    Write an item: (*workspaces)[n].name = name;

    The current application state is also a workspace, that is saved in QSettings
    along with the list of workspaces created by the user. Application state
    parameters that are used in the menus, like isFolderDockVisible, are kept in
    Actions while the rest are normal variables, like thumbWidth.

    It is tricky to deal with the different window states in separate workspaces.
    Switching to/from a maximized workspace using setGeometry works, but does not
    when switching from a fullWindow workspace.
*/

void MW::newWorkspace()
{
    if (G::isLogger) G::log("MW::newWorkspace");

    int n = workspaces->count();
    if (n > 9) {
        QString msg = "Only ten workspaces allowed.  Use Manage Workspaces\n"
                      "to delete or reassign workspaces.";
        QMessageBox::information(this, "Oops", msg, QMessageBox::Ok);
        return;
    }

    bool ok;
    QInputDialog *wsNew = new QInputDialog;
    QString workspaceName = wsNew->getText(this, tr("New Workspace"),
                                           tr("Name:                                                            "),
                                           QLineEdit::Normal, "", &ok);

    // duplicate names illegal
    workspaceName = fixDupWorkspaceName(workspaceName);
    if (ok && !workspaceName.isEmpty() && n < 10) {
        workspaces->append(ws);
        populateWorkspace(n, workspaceName);
        // sync menu items
        workspaceActions.at(n)->setText(workspaceName);
        workspaceActions.at(n)->setObjectName("workspace" + QString::number(n));
        workspaceActions.at(n)->setToolTip("workspace" + QString::number(n));
        workspaceActions.at(n)->setShortcutVisibleInContextMenu(true);
        workspaceActions.at(n)->setShortcut(QKeySequence("Ctrl+" + QString::number(n)));
        workspaceActions.at(n)->setVisible(true);
        saveWorkspaces();
    }
}

QString MW::fixDupWorkspaceName(QString name)
{
/*
    Name is used to index workspaces, so duplicated are illegal.  If a duplicate is
    found then "_1" is appended.  The function is recursive since the original name
    with "_1" appended also might exist.
*/
    if (G::isLogger) G::log("MW::fixDupWorkspaceName");
    for (int i=0; i<workspaces->count(); i++) {
        if (workspaces->at(i).name == name) {
            name += "_1";
            fixDupWorkspaceName(name);
        }
    }
    return name;
}

void MW::invokeCurrentWorkspace()
{
/*
    Called from a QTimer::singleShot in MW::eventFilter QEvent::WindowStateChange
*/
    invokeWorkspace(ws);
}

void MW::invokeWorkspaceFromAction(QAction *workAction)
{
/*
    This is called from a workspace action. Since the workspace actions
    are a list of actions, the workspaceMenu triggered signal is captured, and the
    workspace with a matching name to the action is used.
*/
    if (G::isLogger) G::log("MW::invokeWorkspaceFromAction");
    for (int i = 0; i < workspaces->count(); i++) {
        if (workspaces->at(i).name == workAction->text()) {
            invokeWorkspace(workspaces->at(i));
            return;
        }
    }
}

void MW::invokeWorkspace(const WorkspaceData &w)
{
/*
    Changes the app appearance to conform with a workspace parameters which include:
        - window screen, location and size
        - application state
        - dock visibility and location
        - central widget view (loupe, grid, table, compare)
        - thumbView and gridView parameters
        - imageView show info
        - processes (color manage, caching, sorting)


    It is called from menu actions, MW::toggleFullScreen and QEvent::WindowStateChange
    in MW::eventFilter.

    There is an issue when attempting to invoke a workspace while in FullScreen state if
    the new sworkspace is on a different screen.  The function showNormal() always shows
    the window in the same screen and this takes some time.  MW::eventFilter overrides the
    QEvent::WindowStateChange and calls invokeWorkspace after a delay to allow the showNormal
    function to complete drawing the normal window.
*/
    if (G::isLogger) G::log("MW::invokeWorkspace");

    ws = w;     // current workspace ws

    /* Save current selection.  Since multiple saves occur in view mode and sortChange,
       make a separate copy here to recover later.  */
    sel->save("MW::invokeWorkspace");
    QModelIndexList selectedRows;
    foreach (QModelIndex dmIdx, sel->dmSelectedRows) selectedRows << dmIdx;

    /* Fullscreen was on different screen from new workspace.  Set flag, showNormal and return.
       In the QEvent::WindowStateChange override (MW::eventFilter) invokeWorkspace will be called
       again after the normal window has been completed,*/
    int screenNumber = QGuiApplication::screens().indexOf(screen());
    if (isFullScreen() && screenNumber != w.screenNumber) {
        wasFullSpaceOnDiffScreen = true;
        showNormal();
        return;
    }

    // Visibility
    statusBarVisibleAction->setChecked(w.isStatusBarVisible);
    folderDockVisibleAction->setChecked(w.isFolderDockVisible);
    favDockVisibleAction->setChecked(w.isFavDockVisible);
    filterDockVisibleAction->setChecked(w.isFilterDockVisible);
    catalogDockVisibleAction->setChecked(w.isCatalogDockVisible);
    metadataDockVisibleAction->setChecked(w.isMetadataDockVisible);
    embelDockVisibleAction->setChecked(w.isEmbelDockVisible);
    developDockVisibleAction->setChecked(w.isDevelopDockVisible);
    historyDockVisibleAction->setChecked(w.isHistoryDockVisible);
    presetsDockVisibleAction->setChecked(w.isPresetsDockVisible);
    thumbDockVisibleAction->setChecked(w.isThumbDockVisible);
    infoVisibleAction->setChecked(w.isImageInfoVisible);
    // View
    asLoupeAction->setChecked(w.isLoupeDisplay);
    asGridAction->setChecked(w.isGridDisplay);
    asTableAction->setChecked(w.isTableDisplay);
    asCompareAction->setChecked(w.isCompareDisplay);
    /* assignedIconWidth must track the restored iconWidth, otherwise rejustify() justifies
       from the previous reference width and ignores the workspace's saved size. */
    // Thumbview
    thumbView->iconWidth = w.thumbWidth;
    thumbView->iconHeight = w.thumbHeight;
    thumbView->labelFontSize = w.labelFontSize;
    thumbView->showIconLabels = w.showThumbLabels;
    thumbView->assignedIconWidth = thumbView->iconWidth;
    thumbView->rejustify();
    thumbView->setThumbParameters();
     // GridView
    gridView->iconWidth = w.thumbWidthGrid;
    gridView->iconHeight = w.thumbHeightGrid;
    gridView->labelFontSize = w.labelFontSizeGrid;
    gridView->showIconLabels = w.showThumbLabelsGrid;
    gridView->labelChoice = w.labelChoice;
    gridView->assignedIconWidth = gridView->iconWidth;
    gridView->rejustify();
    gridView->setThumbParameters();
    // ImageView
    infoVisibleAction->setChecked(w.isImageInfoVisible);
    // Processes
    if (w.isColorManage != G::colorManage) {
        if (w.isColorManage) toggleColorManage(Tog::on);
        else toggleColorManage(Tog::off);
    }
    if (sortColumn != w.sortColumn) {
        sortColumn = w.sortColumn;
        updateSortColumn(sortColumn);
    }
    if (w.isReverseSort != isReverseSort) {
        if (w.isReverseSort) toggleSortDirection(Tog::on);
        else toggleSortDirection(Tog::off);
    }
    updateState();
    workspaceChanged = true;
    sortChange("MW::invokeWorkspace");
    // chk if a video file
    if (dm->sf->index(dm->currentSfRow, G::VideoColumn).data().toBool()) {
        centralLayout->setCurrentIndex(VideoTab);
    }
    // in case thumbdock visibility changed by status of wasThumbDockVisible in loupeDisplay etc
    setThumbDockVisibity();

    // qDebug() << "ws.isMaximised =" << w.isMaximised;

    if (!w.isMaximised) {
        restoreGeometry(w.geometry);
        restoreState(w.state);
        // second restoreState req'd for going from docked to floating docks
        restoreGeometry(w.geometry);
        restoreState(w.state);
    }
    else {
        /* Maximised workspace.  restoreGeometry's maximised handling is
           unreliable (especially on macOS), so place the window on the
           workspace's saved screen explicitly, then maximise.  A maximised
           window ignores setGeometry, so drop to normal first.  The dock
           layout (sizes, tabbing, floating, positions) lives in w.state and
           must be restored or a multi-dock arrangement is lost. */
        QScreen *target = QGuiApplication::screens().value(w.screenNumber);
        if (target && QGuiApplication::screens().indexOf(screen()) != w.screenNumber) {
            showNormal();
            setGeometry(target->availableGeometry());
        }
        if (!isMaximized()) showMaximized();
        restoreState(w.state);
        // second restoreState req'd for going from docked to floating docks
        restoreState(w.state);
    }

    /* A workspace saved before a dock existed has no place for it, and Qt leaves such a
       dock loose in whatever area it is in. Dock it where it belongs (same migration the
       main window state gets -- see MW::placeDocksAddedSince). Workspace states stay
       UNVERSIONED so an old one still restores; w.stateVersion is what says which docks
       it predates. */
    if (w.stateVersion < winnowStateVersion) placeDocksAddedSince(w.stateVersion);

    // recover selection
    QItemSelection selection;
    foreach (QModelIndex dmIdx, selectedRows) {
        QModelIndex sfIdx = dm->sf->mapFromSource(dmIdx);
        selection.select(sfIdx, sfIdx);
    }
    sel->sm->select(selection, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);

    thumbView->scrollToCurrent("MW::invokeWorkSpace");

    // if (w.isMaximised) showMaximized();
}

void MW::snapshotWorkspace(WorkspaceData &wsd)
{
    QString fun = "MW::snapshotWorkspace";
    if (G::isLogger) G::log(fun);
    // qDebug() << "MW::snapshotWorkspace  geometry()" << geometry();

    // State
    wsd.geometry = saveGeometry();
    wsd.state = saveState();
    wsd.stateVersion = winnowStateVersion;   // the dock set this layout was saved with
    wsd.screen = screen();
    wsd.screenNumber = QGuiApplication::screens().indexOf(screen());
    wsd.geometryRect = geometry();
    wsd.isFullScreen = isFullScreen();
    wsd.isMaximised = isMaximized();

    // Visibility
    //wsd.isMenuBarVisible = menuBarVisibleAction->isChecked();
    wsd.isStatusBarVisible = statusBarVisibleAction->isChecked();
    wsd.isFolderDockVisible = folderDockVisibleAction->isChecked();
    wsd.isFavDockVisible = favDockVisibleAction->isChecked();
    wsd.isFilterDockVisible = filterDockVisibleAction->isChecked();
    wsd.isCatalogDockVisible = catalogDockVisibleAction->isChecked();
    wsd.isMetadataDockVisible = metadataDockVisibleAction->isChecked();
    wsd.isEmbelDockVisible = embelDockVisibleAction->isChecked();
    wsd.isDevelopDockVisible = developDockVisibleAction->isChecked();
    wsd.isHistoryDockVisible = historyDockVisibleAction->isChecked();
    wsd.isPresetsDockVisible = presetsDockVisibleAction->isChecked();
    wsd.isThumbDockVisible = thumbDockVisibleAction->isChecked();
    wsd.isImageInfoVisible = infoVisibleAction->isChecked();

    // View
    wsd.isLoupeDisplay = asLoupeAction->isChecked();
    wsd.isGridDisplay = asGridAction->isChecked();
    wsd.isTableDisplay = asTableAction->isChecked();
    wsd.isCompareDisplay = asCompareAction->isChecked();

    /* Save assignedIconWidth (the stable user-intended reference size), not the justified,
       viewport-dependent iconWidth. See note in MW::writeSettings / MW::createGridView. */
    // Thumbview
    wsd.thumbWidth = thumbView->assignedIconWidth;
    wsd.thumbHeight = thumbView->iconHeight;
    wsd.labelFontSize = thumbView->labelFontSize;
    wsd.showThumbLabels = thumbView->showIconLabels;

    // GridView
    wsd.thumbWidthGrid = gridView->assignedIconWidth;
    wsd.thumbHeightGrid = gridView->iconHeight;
    wsd.labelFontSizeGrid = gridView->labelFontSize;
    wsd.showThumbLabelsGrid = gridView->showIconLabels;
    wsd.labelChoice = gridView->labelChoice;

    // ImageView
    wsd.isImageInfoVisible = infoVisibleAction->isChecked();

    // Processes
    wsd.isColorManage = G::colorManage;
    wsd.sortColumn = sortColumn;
    wsd.isReverseSort = sortReverseAction->isChecked();
}

void MW::manageWorkspaces()
{
/*
    Delete, rename and reassign workspaces, and update the Winnow default workspace
    (the layout invoked by Ctrl+Shift+W) to the current layout.
*/
    if (G::isLogger) G::log("MW::manageWorkspaces");
    // Update a list of workspace names for the manager dialog
    QList<QString> wsList;
    for (int i=0; i<workspaces->count(); i++)
        wsList.append(workspaces->at(i).name);
    workspaceDlg = new WorkspaceDlg(&wsList, this);
    connect(workspaceDlg, &WorkspaceDlg::deleteWorkspace, this, &MW::deleteWorkspace);
    connect(workspaceDlg, &WorkspaceDlg::reassignWorkspace, this, &MW::reassignWorkspace);
    connect(workspaceDlg, &WorkspaceDlg::renameWorkspace, this, &MW::renameWorkspace);
    connect(workspaceDlg, &WorkspaceDlg::updateDefaultWorkspace,
            this, &MW::updateDefaultWorkspace);
    connect(workspaceDlg, &WorkspaceDlg::reportWorkspaceNum, this, &MW::reportWorkspaceNum);

    // connect(workspaceDlg, SIGNAL(deleteWorkspace(int)),
    //         this, SLOT(deleteWorkspace(int)));
    // connect(workspaceDlg, SIGNAL(reassignWorkspace(int)),
    //         this, SLOT(reassignWorkspace(int)));
    // connect(workspaceDlg, SIGNAL(renameWorkspace(int, QString)),
    //         this, SLOT(renameWorkspace(int, QString)));
    // connect(workspaceDlg, SIGNAL(reportWorkspace(int)),
    //         this, SLOT(reportWorkspace(int)));
    workspaceDlg->exec();
    delete workspaceDlg;
}

void MW::deleteWorkspace(int n)
{
    if (G::isLogger)
        G::log("MW::deleteWorkspace");
    if (workspaces->count() < 1) return;

    // remove workspace from list of workspaces
    workspaces->removeAt(n);

    saveWorkspaces();

    // sync menus by re-updating.  Tried to use indexes but had problems so
    // resorted to brute force solution
    syncWorkspaceMenu();
}

void MW::syncWorkspaceMenu()
{
    if (G::isLogger) G::log("MW::syncWorkspaceMenu");
    int count = workspaces->count();
    for (int i = 0; i < 10; i++) {
        if (i < count) {
            workspaceActions.at(i)->setText(workspaces->at(i).name);
            workspaceActions.at(i)->setShortcut(QKeySequence("Ctrl+" + QString::number(i)));
            workspaceActions.at(i)->setVisible(true);
        }
        else {
            workspaceActions.at(i)->setText("Future workspace"  + QString::number(i));
            workspaceActions.at(i)->setVisible(false);
        }
    }
}

void MW::reassignWorkspace(int n)
{
    if (G::isLogger) G::log("MW::reassignWorkspace");
    QString name = workspaces->at(n).name;
    populateWorkspace(n, name);
    saveWorkspaces();
    // reportWorkspaceNum(n);
}

void MW::defaultWorkspace()
{
/*
    The defaultWorkspace is used the first time the app is run on a new machine and
    there are not any QSettings to read.  It is also useful if part or all of the
    app is "stranded" on secondary monitors that are not attached.

    If the user has redefined the default workspace (Manage Workspaces > "Winnow default
    workspace" > Update to current layout) then that layout is used, otherwise the
    built-in Winnow layout is used.
*/
    if (G::isLogger) G::log("MW::defaultWorkspace");

    if (isCustomDefaultWorkspace) {
        invokeWorkspace(defaultWs);
        return;
    }
    builtInDefaultWorkspace();
}

bool MW::restoreWindowState(const QByteArray &state)
{
/*
    Restore the main window dock layout saved by MW::writeSettings.

    The state is tagged with MW::winnowStateVersion, which names the dock set that wrote
    it. A state written by an older build is still perfectly good for every dock that
    existed then, so it is restored AT ITS OWN VERSION and the docks added since are
    placed afterwards (placeDocksAddedSince). Previously such a state was simply rejected,
    which silently reset the user's whole layout -- most visibly, thumbDock fell back to
    the left area under the folder group -- every time a dock was added.

    Returns false only if the state is unreadable at any version; the caller then falls
    back to the default workspace.
*/
    if (G::isLogger) G::log("MW::restoreWindowState");

    if (state.isEmpty()) return false;

    for (int v = winnowStateVersion; v >= 0; --v) {
        if (!restoreState(state, v)) continue;
        // second restoreState req'd for going from docked to floating docks
        restoreState(state, v);
        if (v < winnowStateVersion) placeDocksAddedSince(v);
        return true;
    }
    return false;
}

void MW::placeDocksAddedSince(int stateVersion)
{
/*
    Dock the panels that did not exist when a restored layout was saved.

    A dock missing from a restored state is not dropped: Qt leaves it wherever it happens
    to sit, untabbed, so it shows up as a stray panel squatting in an area (and, when it
    is the front of an empty group, as the flickering zombie tab bars developDock once
    produced). Each entry below says which group its dock belongs to, so the migrated
    layout matches what createDocks() would have built. Visibility comes from the dock's
    action -- ie the user's saved preference, or off for a panel that has never existed
    for this user -- never from the stale state.

    BUMP winnowStateVersion and add a row here whenever a dock is added.
*/
    if (G::isLogger) G::log("MW::placeDocksAddedSince");

    struct AddedDock {
        int version;                // winnowStateVersion that introduced the dock
        DockWidget *dock;
        DockWidget *tabWith;        // group it belongs to, nullptr = leave where it is
        QAction *visibleAction;
    };
    const QVector<AddedDock> added {
        {1, developDock, hideEmbellish ? nullptr : embelDock, developDockVisibleAction},
        {2, historyDock, developDock, historyDockVisibleAction},
        {3, presetsDock, developDock, presetsDockVisibleAction},
        {4, catalogDock, filterDock, catalogDockVisibleAction},
    };

    for (const AddedDock &a : added) {
        if (stateVersion >= a.version) continue;
        if (a.dock == nullptr) continue;
        /* Do not drag a floating target back into a dock area -- tabifying onto it would
           do exactly that. The new dock keeps the area Qt left it in. */
        if (a.tabWith && !a.tabWith->isFloating() && !a.dock->isFloating())
            tabifyDockWidget(a.tabWith, a.dock);
        if (a.visibleAction) a.dock->setVisible(a.visibleAction->isChecked());
    }
}

void MW::builtInDefaultWorkspace()
{
/*
    The layout Winnow ships with.  See MW::defaultWorkspace.
*/
    if (G::isLogger) G::log("MW::builtInDefaultWorkspace");
    QRect desktop = QGuiApplication::screens().first()->geometry();
//    QRect desktop = qApp->desktop()->availableGeometry();
//    qDebug() << "MW::defaultWorkspace" << desktop << desktop1;
    resize(static_cast<int>(0.75 * desktop.width()),
           static_cast<int>(0.75 * desktop.height()));
    setGeometry( QStyle::alignedRect(Qt::LeftToRight, Qt::AlignCenter,
        size(), desktop));
//    menuBarVisibleAction->setChecked(true);
    statusBarVisibleAction->setChecked(true);

    folderDockVisibleAction->setChecked(true);
    favDockVisibleAction->setChecked(true);
    filterDockVisibleAction->setChecked(true);
    /* Off in the shipped layout: an empty catalog has nothing to show, and the
       left group is already four tabs deep. Shift+F2 or the Window menu opens it. */
    catalogDockVisibleAction->setChecked(false);
    metadataDockVisibleAction->setChecked(true);
    embelDockVisibleAction->setChecked(false);
    thumbDockVisibleAction->setChecked(true);

//    thumbView->iconPadding = 0;
    thumbView->iconWidth = 100;
    thumbView->iconHeight = 100;
    thumbView->labelFontSize = 10;
    thumbView->showIconLabels = false;
    thumbView->showZoomFrame = true;

//    gridView->iconPadding = 0;
    gridView->iconWidth = 160;
    gridView->iconHeight = 160;
    gridView->labelFontSize = 10;
    gridView->showIconLabels = true;

    /* assignedIconWidth must track iconWidth so rejustify() honors these defaults instead
       of justifying from the previous reference width. */
    thumbView->assignedIconWidth = thumbView->iconWidth;
    gridView->assignedIconWidth = gridView->iconWidth;

    thumbView->setWrapping(false);
    thumbView->setThumbParameters();
    gridView->setThumbParameters();
    thumbView->rejustify();
    gridView->rejustify();

    folderDock->setFloating(false);
    favDock->setFloating(false);
    filterDock->setFloating(false);
    if (G::useInfoView) metadataDock->setFloating(false);
    embelDock->setFloating(false);
    thumbDock->setFloating(false);

    addDockWidget(Qt::LeftDockWidgetArea, folderDock);
    addDockWidget(Qt::LeftDockWidgetArea, favDock);
    addDockWidget(Qt::LeftDockWidgetArea, filterDock);
    if (G::useInfoView) addDockWidget(Qt::LeftDockWidgetArea, metadataDock);
//    addDockWidget(Qt::RightDockWidgetArea, embelDock);
    addDockWidget(Qt::BottomDockWidgetArea, thumbDock);

    MW::setTabPosition(Qt::LeftDockWidgetArea, QTabWidget::North);
    MW::tabifyDockWidget(folderDock, favDock);
    MW::tabifyDockWidget(favDock, filterDock);
    if (G::useInfoView) MW::tabifyDockWidget(filterDock, metadataDock);

    folderDock->show();
    folderDock->raise();
    resizeDocks({folderDock}, {350}, Qt::Horizontal);

    // enable the folder dock (first one in tab)
    QList<QTabBar *> tabList = findChildren<QTabBar *>();
    QTabBar* widgetTabBar = tabList.at(0);
    widgetTabBar->setCurrentIndex(0);

    resizeDocks({thumbDock}, {100}, Qt::Vertical);

    setThumbDockFeatures(dockWidgetArea(thumbDock));

    asLoupeAction->setChecked(true);
    infoVisibleAction->setChecked(true);
    sortReverseAction->setChecked(false);
    sortColumn = 0;
    sortChange("MW::builtInDefaultWorkspace");
    ws.name = WorkspaceDlg::defaultWorkspaceName;
    updateState();
}

void MW::renameWorkspace(int n, QString name)
{
    if (G::isLogger)
        G::log("MW::renameWorkspace");
    // do not rename if duplicate
    if (workspaces->count() > 0) {
        for (int i=1; i<workspaces->count(); i++) {
            if (workspaces->at(i).name == name) return;
        }
        (*workspaces)[n].name = name;
        syncWorkspaceMenu();
    }
    saveWorkspaces();
}

void MW::populateWorkspace(int n, QString name)
{
    if (G::isLogger) G::log("MW::populateWorkspace");
    snapshotWorkspace((*workspaces)[n]);
    (*workspaces)[n].name = name;
}

QString MW::reportWorkspaces()
{
    QString reportString;
    QTextStream rpt;
    rpt.setString(&reportString);
    rpt << Utilities::centeredRptHdr('=', "Workspaces Diagnostics");
    rpt << "\n\n";
    int n = workspaces->count();
    rpt << "Workspaces count = " << n;
    rpt << "\n";
    for (int i = 0; i < n; i++) {
        ws = workspaces->at(i);
        RecoverGeometry r;
        recoverGeometry(ws.geometry, r);
        // rpt
        rpt
            << "\nWorkspace: " << i
            << "\n  Name                      " << ws.name
            << "\nRestoreGeometryByteArray:"
            << "\n  frameGeometry             " << G::s(r.frameGeometry)
            // << "\n  Geometry                " << G::s(r.geometry)
            << "\n  normalGeometry            " << G::s(r.normalGeometry)
            << "\n  screenNumber              " << G::s(r.screenNumber)
            << "\n  maximized                 " << G::s(r.maximized)
            << "\n  fullScreen                " << G::s(r.fullScreen)
            << "\nState:"
            << "\n  geometryRect              " << G::s(ws.geometryRect)
            << "\n  stateVersion              " << G::s(ws.stateVersion)
            << "\n  screenNumber              " << G::s(ws.screenNumber)
            << "\n  isFullScreen              " << G::s(ws.isFullScreen)
            << "\n  isMaximised               " << G::s(ws.isMaximised)
            << "\nVisibility:"
            << "\n  isWindowTitleBarVisible   " << G::s(ws.isWindowTitleBarVisible)
            //<< "\nisMenuBarVisible" << ws.isMenuBarVisible
            << "\n  isStatusBarVisible        " << G::s(ws.isStatusBarVisible)
            << "\n  isFolderDockVisible       " << G::s(ws.isFolderDockVisible)
            << "\n  isFavDockVisible          " << G::s(ws.isFavDockVisible)
            << "\n  isFilterDockVisible       " << G::s(ws.isFilterDockVisible)
            << "\n  isCatalogDockVisible      " << G::s(ws.isCatalogDockVisible)
            << "\n  isMetadataDockVisible     " << G::s(ws.isMetadataDockVisible)
            << "\n  isEmbelDockVisible        " << G::s(ws.isEmbelDockVisible)
            << "\n  isDevelopDockVisible      " << G::s(ws.isDevelopDockVisible)
            << "\n  isHistoryDockVisible      " << G::s(ws.isHistoryDockVisible)
            << "\n  isPresetsDockVisible      " << G::s(ws.isPresetsDockVisible)
            << "\n  isThumbDockVisible        " << G::s(ws.isThumbDockVisible)
            << "\nView:"
            << "\n  isLoupeDisplay            " << G::s(ws.isLoupeDisplay)
            << "\n  isGridDisplay             " << G::s(ws.isGridDisplay)
            << "\n  isTableDisplay            " << G::s(ws.isTableDisplay)
            << "\n  isCompareDisplay          " << G::s(ws.isCompareDisplay)
            << "\nThumbView:"
            << "\n  thumbSpacing              " << G::s(ws.thumbSpacing)
            << "\n  thumbPadding              " << G::s(ws.thumbPadding)
            << "\n  thumbWidth                " << G::s(ws.thumbWidth)
            << "\n  thumbHeight               " << G::s(ws.thumbHeight)
            << "\n  labelFontSize             " << G::s(ws.labelFontSize)
            << "\n  showThumbLabels           " << G::s(ws.showThumbLabels)
            << "\nGridView:"
            << "\n  thumbSpacingGrid          " << G::s(ws.thumbSpacingGrid)
            << "\n  thumbPaddingGrid          " << G::s(ws.thumbPaddingGrid)
            << "\n  thumbWidthGrid            " << G::s(ws.thumbWidthGrid)
            << "\n  thumbHeightGrid           " << G::s(ws.thumbHeightGrid)
            << "\n  labelFontSizeGrid         " << G::s(ws.labelFontSizeGrid)
            << "\n  showThumbLabelsGrid       " << G::s(ws.showThumbLabelsGrid)
            << "\n  gridViewLabelChoice      " << G::s(ws.labelChoice)
            << "\nImageView:"
            << "\n  showShootingInfo          " << G::s(ws.isImageInfoVisible)
            // << "\n  isEmbelDisplay            " << G::s(ws.isEmbelDisplay)
            << "\nProcesses:"
            << "\n  isColorManage             " << G::s(ws.isColorManage)
            << "\n  sortColumn                " << G::s(ws.sortColumn)
            << "\n  isReverseSort             " << G::s(ws.isReverseSort)
            << "\n"
            //*/
            ;
    }
    return reportString;
}

void MW::reportWorkspaceNum(int n)
{
    if (G::isLogger) G::log("MW::reportWorkspace");
    ws = workspaces->at(n);
    reportWorkspace(ws);
}

void MW::reportWorkspace(WorkspaceData &ws, QString src)
{
    return;
    if (G::isLogger) G::log("MW::reportWorkspace");
    // ws = workspaces->at(n);
    qDebug() << "\n\nName" << ws.name << "  Src:" << src;
    RecoverGeometry r;
    recoverGeometry(ws.geometry, r);
    qDebug()
        << "RecoverGeometry from QByteArray:"
        << "\n   FrameGeometry" << r.frameGeometry
        << "\n   NormalGeometry" << r.normalGeometry
        << "\n   screenNumber" << r.screenNumber
        << "\n   maximized" << r.maximized
        << "\n   fullScreen" << r.fullScreen
        << "screenNum" << ws.screenNumber
        << "isFullScreen" << ws.isFullScreen
        << "\nisMaximised" << ws.isMaximised
        // /*
        << "\nisWindowTitleBarVisible" << ws.isWindowTitleBarVisible
        //<< "\nisMenuBarVisible" << ws.isMenuBarVisible
        << "\nisStatusBarVisible" << ws.isStatusBarVisible
        << "\nisFolderDockVisible" << ws.isFolderDockVisible
        << "\nisFavDockVisible" << ws.isFavDockVisible
        << "\nisFilterDockVisible" << ws.isFilterDockVisible
        << "\nisCatalogDockVisible" << ws.isCatalogDockVisible
        << "\nisMetadataDockVisible" << ws.isMetadataDockVisible
        << "\nisEmbelDockVisible" << ws.isEmbelDockVisible
        << "\nisDevelopDockVisible" << ws.isDevelopDockVisible
        << "\nisHistoryDockVisible" << ws.isHistoryDockVisible
        << "\nisPresetsDockVisible" << ws.isPresetsDockVisible
        << "\nisThumbDockVisible" << ws.isThumbDockVisible
        << "\nthumbSpacing" << ws.thumbSpacing
        << "\nthumbPadding" << ws.thumbPadding
        << "\nthumbWidth" << ws.thumbWidth
        << "\nthumbHeight" << ws.thumbHeight
        << "\nlabelFontSize" << ws.labelFontSize
        << "\nshowThumbLabels" << ws.showThumbLabels
        << "\nthumbSpacingGrid" << ws.thumbSpacingGrid
        << "\nthumbPaddingGrid" << ws.thumbPaddingGrid
        << "\nthumbWidthGrid" << ws.thumbWidthGrid
        << "\nthumbHeightGrid" << ws.thumbHeightGrid
        << "\nlabelFontSizeGrid" << ws.labelFontSizeGrid
        << "\nshowThumbLabelsGrid" << ws.showThumbLabelsGrid
        << "\nsgridViewLabelChoice" << ws.labelChoice
        << "\nshowShootingInfo" << ws.isImageInfoVisible
        << "\nisLoupeDisplay" << ws.isLoupeDisplay
        << "\nisGridDisplay" << ws.isGridDisplay
        << "\nisTableDisplay" << ws.isTableDisplay
        << "\nisCompareDisplay" << ws.isCompareDisplay
        << "\nisEmbelDisplay" << ws.isEmbelDisplay
        << "\nisColorManage" << ws.isColorManage
        << "\nsortColumn" << ws.sortColumn
        << "\nisReverseSort" << ws.isReverseSort
        //*/
        ;
}

void MW::readWorkspaceSettings(WorkspaceData &wsd)
{
/*
    Read one workspace from the current QSettings position (array index or group).
    Used by MW::loadWorkspaces and MW::loadDefaultWorkspace.
*/
    // Workspace
    wsd.name = settings->value("name").toString();

    // State
    wsd.geometry = settings->value("geometry").toByteArray();
    wsd.state = settings->value("state").toByteArray();
    // absent = saved before the key existed, ie predates every versioned dock
    wsd.stateVersion = settings->value("stateVersion", 0).toInt();
    RecoverGeometry r;
    recoverGeometry(wsd.geometry, r);
    wsd.screenNumber = r.screenNumber;
    wsd.geometryRect = settings->value("geometryRect").toRect();
    wsd.isFullScreen = settings->value("isFullScreen").toBool();
    wsd.isMaximised = settings->value("isMaximised").toBool();

    // Visibility
    wsd.isWindowTitleBarVisible = settings->value("isWindowTitleBarVisible").toBool();
    //wsd.isMenuBarVisible = settings->value("isMenuBarVisible").toBool();
    wsd.isStatusBarVisible = settings->value("isStatusBarVisible").toBool();
    wsd.isFolderDockVisible = settings->value("isFolderDockVisible").toBool();
    wsd.isFavDockVisible = settings->value("isFavDockVisible").toBool();
    wsd.isFilterDockVisible = settings->value("isFilterDockVisible").toBool();
    wsd.isCatalogDockVisible = settings->value("isCatalogDockVisible").toBool();
    wsd.isMetadataDockVisible = settings->value("isMetadataDockVisible").toBool();
    wsd.isEmbelDockVisible = settings->value("isEmbelDockVisible").toBool();
    wsd.isDevelopDockVisible = settings->value("isDevelopDockVisible").toBool();
    wsd.isHistoryDockVisible = settings->value("isHistoryDockVisible").toBool();
    wsd.isPresetsDockVisible = settings->value("isPresetsDockVisible").toBool();
    wsd.isThumbDockVisible = settings->value("isThumbDockVisible").toBool();

    // View
    wsd.isLoupeDisplay = settings->value("isLoupeDisplay").toBool();
    wsd.isGridDisplay = settings->value("isGridDisplay").toBool();
    wsd.isTableDisplay = settings->value("isTableDisplay").toBool();
    wsd.isCompareDisplay = settings->value("isCompareDisplay").toBool();

    // ThumbView
    wsd.thumbSpacing = settings->value("thumbSpacing").toInt();
    wsd.thumbPadding = settings->value("thumbPadding").toInt();
    wsd.thumbWidth = settings->value("thumbWidth").toInt();
    wsd.thumbHeight = settings->value("thumbHeight").toInt();
    wsd.labelFontSize = settings->value("labelFontSize").toInt();
    wsd.showThumbLabels = settings->value("showThumbLabels").toBool();

    // GridView
    wsd.thumbSpacingGrid = settings->value("thumbSpacingGrid").toInt();
    wsd.thumbPaddingGrid = settings->value("thumbPaddingGrid").toInt();
    wsd.thumbWidthGrid = settings->value("thumbWidthGrid").toInt();
    wsd.thumbHeightGrid = settings->value("thumbHeightGrid").toInt();
    wsd.labelFontSizeGrid = settings->value("labelFontSizeGrid").toInt();
    wsd.showThumbLabelsGrid = settings->value("showThumbLabelsGrid").toBool();
    wsd.labelChoice = settings->value("labelChoice").toString();

    // ImageView
    wsd.isImageInfoVisible = settings->value("isImageInfoVisible").toBool();
    // wsd.isEmbelDisplay = settings->value("isEmbelDisplay").toBool();

    // Processes
    wsd.isColorManage = settings->value("isColorManage").toBool();
    wsd.sortColumn = settings->value("sortColumn").toInt();
    /* Sanitize a persisted sortColumn that is out of range (e.g. saved by a build with a
       different column layout; G::TotalColumns is one past the last real column). Left
       unchecked it reaches dm->sf->sort() as a phantom column — see IconView::sortThumbs. */
    if (wsd.sortColumn < 0 || wsd.sortColumn >= G::TotalColumns) wsd.sortColumn = G::NameColumn;
    wsd.isReverseSort = settings->value("isReverseSort").toBool();
}

void MW::writeWorkspaceSettings(const WorkspaceData &wsd)
{
/*
    Write one workspace to the current QSettings position (array index or group).
    Used by MW::saveWorkspaces and MW::saveDefaultWorkspace.
*/
    // Workspace
    settings->setValue("name", wsd.name);

    // State
    settings->setValue("geometry", wsd.geometry);
    settings->setValue("state", wsd.state);
    settings->setValue("stateVersion", wsd.stateVersion);
    settings->setValue("screenNumber", wsd.screenNumber);
    settings->setValue("geometryRect", wsd.geometryRect);                        // need?
    settings->setValue("isFullScreen", wsd.isFullScreen);                        // need?
    settings->setValue("isMaximised", wsd.isMaximised);                          // need?

    // Visibility
    settings->setValue("isWindowTitleBarVisible", wsd.isWindowTitleBarVisible);  // need? Not used.
    //settings->setValue("isMenuBarVisible", wsd.isMenuBarVisible);
    settings->setValue("isStatusBarVisible", wsd.isStatusBarVisible);
    settings->setValue("isFolderDockVisible", wsd.isFolderDockVisible);
    settings->setValue("isFavDockVisible", wsd.isFavDockVisible);
    settings->setValue("isFilterDockVisible", wsd.isFilterDockVisible);
    settings->setValue("isCatalogDockVisible", wsd.isCatalogDockVisible);
    settings->setValue("isMetadataDockVisible", wsd.isMetadataDockVisible);
    settings->setValue("isEmbelDockVisible", wsd.isEmbelDockVisible);
    settings->setValue("isDevelopDockVisible", wsd.isDevelopDockVisible);
    settings->setValue("isHistoryDockVisible", wsd.isHistoryDockVisible);
    settings->setValue("isPresetsDockVisible", wsd.isPresetsDockVisible);
    settings->setValue("isThumbDockVisible", wsd.isThumbDockVisible);

    // View
    settings->setValue("isLoupeDisplay", wsd.isLoupeDisplay);
    settings->setValue("isGridDisplay", wsd.isGridDisplay);
    settings->setValue("isTableDisplay", wsd.isTableDisplay);
    settings->setValue("isCompareDisplay", wsd.isCompareDisplay);

    // ThumbView
    settings->setValue("thumbSpacing", wsd.thumbSpacing);                        // need?
    settings->setValue("thumbPadding", wsd.thumbPadding);
    settings->setValue("thumbWidth", wsd.thumbWidth);
    settings->setValue("thumbHeight", wsd.thumbHeight);
    settings->setValue("labelFontSize", wsd.labelFontSize);
    settings->setValue("showThumbLabels", wsd.showThumbLabels);

    // GridView
    settings->setValue("thumbSpacingGrid", wsd.thumbSpacingGrid);
    settings->setValue("thumbPaddingGrid", wsd.thumbPaddingGrid);
    settings->setValue("thumbWidthGrid", wsd.thumbWidthGrid);
    settings->setValue("thumbHeightGrid", wsd.thumbHeightGrid);
    settings->setValue("labelFontSizeGrid", wsd.labelFontSizeGrid);
    settings->setValue("showThumbLabelsGrid", wsd.showThumbLabelsGrid);
    settings->setValue("labelChoice", wsd.labelChoice);

    // ImageView
    settings->setValue("isImageInfoVisible", wsd.isImageInfoVisible);
    // settings->setValue("isEmbelDisplay", wsd.isEmbelDisplay);                 // need?

    // Processes
    settings->setValue("isColorManage", wsd.isColorManage);
    settings->setValue("sortColumn", wsd.sortColumn);
    settings->setValue("isReverseSort", wsd.isReverseSort);
}

void MW::loadWorkspaces()
{
    if (G::isLogger) G::log("MW::loadWorkspaces");
    if (!isSettings) return;

    // replace with the current list of workspaces
    int size = settings->beginReadArray("Workspaces");
    for (int i = 0; i < size; ++i) {
        settings->setArrayIndex(i);
        readWorkspaceSettings(ws);
        workspaces->append(ws);
    }
    settings->endArray();

    loadDefaultWorkspace();
}

void MW::saveWorkspaces()
{
    if (G::isLogger) G::log("MW::saveWorkspaces");

    // first remove the existing array of workspaces
    settings->remove("Workspaces");

    int size = workspaces->count();
    settings->beginWriteArray("Workspaces", size);
    for (int i = 0; i < size; ++i) {
        settings->setArrayIndex(i);
        writeWorkspaceSettings(workspaces->at(i));
    }
    settings->endArray();
}

void MW::loadDefaultWorkspace()
{
/*
    Read the user defined default workspace (see MW::updateDefaultWorkspace).  If it has
    never been defined then the built-in Winnow layout is used instead.
*/
    if (G::isLogger) G::log("MW::loadDefaultWorkspace");
    if (!isSettings) return;

    settings->beginGroup("DefaultWorkspace");
    isCustomDefaultWorkspace = settings->contains("state");
    if (isCustomDefaultWorkspace) readWorkspaceSettings(defaultWs);
    settings->endGroup();
    defaultWs.name = WorkspaceDlg::defaultWorkspaceName;
}

void MW::saveDefaultWorkspace()
{
    if (G::isLogger) G::log("MW::saveDefaultWorkspace");

    settings->remove("DefaultWorkspace");
    settings->beginGroup("DefaultWorkspace");
    writeWorkspaceSettings(defaultWs);
    settings->endGroup();
}

void MW::updateDefaultWorkspace()
{
/*
    Save the current layout as the default workspace (Ctrl+Shift+W).  Called from the
    Manage Workspaces dialog when "Winnow default workspace" is selected and the
    "Update to current layout" button is pressed.
*/
    if (G::isLogger) G::log("MW::updateDefaultWorkspace");

    snapshotWorkspace(defaultWs);
    defaultWs.name = WorkspaceDlg::defaultWorkspaceName;
    isCustomDefaultWorkspace = true;
    saveDefaultWorkspace();
}

void MW::recoverGeometry(const QByteArray &geometry, RecoverGeometry &r)
/*
    From Qwidget::restoreGeometry(const QByteArray &geometry)

    This is used to recover the app geometry from the QByteArray generated by
    QWidget::saveGeometry without running QWidget::recoverGeometry.
*/
{
    QDataStream stream(geometry);
    stream.setVersion(QDataStream::Qt_4_0);
    quint32 magicNumber;
    quint16 majorVersion = 0;
    quint16 minorVersion = 0;
    stream >> magicNumber
        >> majorVersion
        >> minorVersion
        >> r.frameGeometry
        >> r.normalGeometry
        >> r.screenNumber
        >> r.maximized
        >> r.fullScreen;

    /*
    qDebug() << "MW::recoverGeometry"
             << "\n\tQByteArray geometry =" << geometry
             << "\n\tFrameGeometry       =" << r.frameGeometry
             << "\n\tNormalGeometry      =" << r.normalGeometry
             << "\n\tscreenNumber        =" << r.screenNumber
             << "\n\tmaximized           =" << r.maximized
             << "\n\tfullScreen          =" << r.fullScreen
        ;
        //*/
}
