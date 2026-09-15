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
        /* A new workspace owns the window position and size, the behaviour before it was
           optional.  Manage Workspaces turns it off. */
        (*workspaces)[n].isGeometryIncluded = true;
        // sync menu items
        workspaceActions.at(n)->setText(workspaceMenuName(workspaces->at(n)));
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
    This is called from a workspace action. Since the workspace actions are a list of
    actions, the workspaceMenu triggered signal is captured, and the workspace at the
    same position in the list as the action is used.  The action text is NOT the
    workspace name -- see MW::workspaceMenuName -- so it cannot be matched on.
*/
    if (G::isLogger) G::log("MW::invokeWorkspaceFromAction");
    int i = workspaceActions.indexOf(workAction);
    if (i < 0 || i >= workspaces->count()) return;
    invokeWorkspace(workspaces->at(i));
}

QString MW::workspaceMenuName(const WorkspaceData &w) const
{
/*
    The Workspace menu text for a workspace.  A trailing " *" flags a workspace that
    restores the window position and size as well as the layout.
*/
    return w.isGeometryIncluded ? w.name + " *" : w.name;
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
    if (w.isGeometryIncluded && isFullScreen() && screenNumber != w.screenNumber) {
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
    keywordsDockVisibleAction->setChecked(w.isKeywordsDockVisible);
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

    /* Window position and size are optional.  When the workspace does not own them the
       dock layout is still restored (w.state), so the panels rearrange inside whatever
       window the user currently has, and the window itself is left alone -- including
       its maximised / fullscreen state. */
    if (!w.isGeometryIncluded) {
        restoreState(w.state);
        // second restoreState req'd for going from docked to floating docks
        restoreState(w.state);
    }
    else if (!w.isMaximised) {
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
    /* wsd.isGeometryIncluded is deliberately NOT set here: it is the user's choice for
       this workspace, not part of the layout being snapshotted, so it survives a
       reassign (Manage Workspaces > Update to current layout). */

    // Visibility
    //wsd.isMenuBarVisible = menuBarVisibleAction->isChecked();
    wsd.isStatusBarVisible = statusBarVisibleAction->isChecked();
    wsd.isFolderDockVisible = folderDockVisibleAction->isChecked();
    wsd.isFavDockVisible = favDockVisibleAction->isChecked();
    wsd.isFilterDockVisible = filterDockVisibleAction->isChecked();
    wsd.isCatalogDockVisible = catalogDockVisibleAction->isChecked();
    wsd.isKeywordsDockVisible = keywordsDockVisibleAction->isChecked();
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
    Delete, rename and reassign the user's saved workspaces.  The per-workflow default
    and override layouts are managed from Window > Workspace, not here.
*/
    if (G::isLogger) G::log("MW::manageWorkspaces");
    // Update a list of workspace names for the manager dialog
    QList<QString> wsList;
    QList<bool> wsGeometry;
    for (int i=0; i<workspaces->count(); i++) {
        wsList.append(workspaces->at(i).name);
        wsGeometry.append(workspaces->at(i).isGeometryIncluded);
    }
    workspaceDlg = new WorkspaceDlg(&wsList, wsGeometry, this);
    connect(workspaceDlg, &WorkspaceDlg::deleteWorkspace, this, &MW::deleteWorkspace);
    connect(workspaceDlg, &WorkspaceDlg::reassignWorkspace, this, &MW::reassignWorkspace);
    connect(workspaceDlg, &WorkspaceDlg::renameWorkspace, this, &MW::renameWorkspace);
    connect(workspaceDlg, &WorkspaceDlg::reportWorkspaceNum, this, &MW::reportWorkspaceNum);
    connect(workspaceDlg, &WorkspaceDlg::setWorkspaceGeometryIncluded,
            this, &MW::setWorkspaceGeometryIncluded);

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
    syncWorkflowWorkspaceMenus();
    int count = workspaces->count();
    for (int i = 0; i < 10; i++) {
        if (i < count) {
            workspaceActions.at(i)->setText(workspaceMenuName(workspaces->at(i)));
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

void MW::setWorkspaceGeometryIncluded(int n, bool isIncluded)
{
/*
    Turn the window position and size on or off for a workspace.  Called from the Manage
    Workspaces dialog checkbox.  n is the index into workspaces.
*/
    if (G::isLogger) G::log("MW::setWorkspaceGeometryIncluded");

    if (n < 0 || n >= workspaces->count()) return;
    (*workspaces)[n].isGeometryIncluded = isIncluded;
    saveWorkspaces();
    syncWorkspaceMenu();
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
        {5, keywordsDock, filterDock, keywordsDockVisibleAction},
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

void MW::centreWindowOnPrimaryScreen()
{
/*
    Three quarters of the primary screen, centred.  Split out of
    MW::builtInDefaultWorkspace because a workflow workspace deliberately does not own
    the window position and size, so a first run (no saved Geometry) has to size the
    window before the Library layout is applied.  See MW::showEvent.
*/
    if (G::isLogger) G::log("MW::centreWindowOnPrimaryScreen");
    QRect desktop = QGuiApplication::screens().first()->geometry();
    resize(static_cast<int>(0.75 * desktop.width()),
           static_cast<int>(0.75 * desktop.height()));
    setGeometry(QStyle::alignedRect(Qt::LeftToRight, Qt::AlignCenter, size(), desktop));
}

void MW::builtInDefaultWorkspace()
{
/*
    The layout Winnow ships with: the last-resort fallback when a workflow has no
    shipped default.  See MW::invokeWorkflowWorkspace.
*/
    if (G::isLogger) G::log("MW::builtInDefaultWorkspace");
    centreWindowOnPrimaryScreen();
//    menuBarVisibleAction->setChecked(true);
    statusBarVisibleAction->setChecked(true);

    folderDockVisibleAction->setChecked(true);
    favDockVisibleAction->setChecked(true);
    filterDockVisibleAction->setChecked(true);
    /* Off in the shipped layout: an empty catalog has nothing to show, and the
       left group is already four tabs deep. The Window menu opens it. */
    catalogDockVisibleAction->setChecked(false);
    /*  Off, for the reason the Catalog panel is off: the left group is already four
        tabs deep and a fifth most sessions never open costs every session the space. */
    keywordsDockVisibleAction->setChecked(false);
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

    // Workflow workspaces (see MW::invokeWorkflowWorkspace)
    rpt << "Workflow workspaces:";
    for (int wf = 0; wf < WfCount; ++wf) {
        const bool isDef = wf < isWorkflowDefault.count() && isWorkflowDefault.at(wf);
        const bool isOvr = wf < isWorkflowOverride.count() && isWorkflowOverride.at(wf);
        rpt << "\n  " << workflowNames().at(wf).leftJustified(14)
            << " shipped default: " << G::s(isDef)
            << "   user layout captured: " << G::s(hasWorkflowOverride(wf))
            << "   override in use: " << G::s(isOvr);
    }
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
            << "\n  isGeometryIncluded        " << G::s(ws.isGeometryIncluded)
            << "\nVisibility:"
            << "\n  isWindowTitleBarVisible   " << G::s(ws.isWindowTitleBarVisible)
            //<< "\nisMenuBarVisible" << ws.isMenuBarVisible
            << "\n  isStatusBarVisible        " << G::s(ws.isStatusBarVisible)
            << "\n  isFolderDockVisible       " << G::s(ws.isFolderDockVisible)
            << "\n  isFavDockVisible          " << G::s(ws.isFavDockVisible)
            << "\n  isFilterDockVisible       " << G::s(ws.isFilterDockVisible)
            << "\n  isCatalogDockVisible      " << G::s(ws.isCatalogDockVisible)
            << "\n  isKeywordsDockVisible     " << G::s(ws.isKeywordsDockVisible)
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
        << "\nisKeywordsDockVisible" << ws.isKeywordsDockVisible
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

/*  *******************************************************************************************

    WORKSPACE SERIALISATION

    ONE field list, in MW::workspaceToMap / MW::workspaceFromMap, serves every place a
    workspace is written out: the QSettings array of user workspaces, the QSettings group
    of a workflow override, and the JSON resource of shipped workflow defaults.  Add a
    new WorkspaceData field there -- plus MW::snapshotWorkspace and MW::invokeWorkspace,
    which move it to and from the live app -- and all three follow.

    JSON is not as rich as QVariant, so the two QByteArray fields are base64 encoded and
    the one QRect is written as [x, y, w, h].  Those are the only keys that need special
    handling; everything else is bool, int or QString and survives the round trip.
*/

static const QStringList kByteArrayKeys{"geometry", "state"};
static const QStringList kRectKeys{"geometryRect"};

QVariantMap MW::workspaceToMap(const WorkspaceData &wsd) const
{
    QVariantMap m;
    // Workspace
    m["name"] = wsd.name;
    // State
    m["geometry"] = wsd.geometry;
    m["state"] = wsd.state;
    m["stateVersion"] = wsd.stateVersion;
    m["screenNumber"] = wsd.screenNumber;
    m["geometryRect"] = wsd.geometryRect;
    m["isFullScreen"] = wsd.isFullScreen;
    m["isMaximised"] = wsd.isMaximised;
    m["isGeometryIncluded"] = wsd.isGeometryIncluded;
    // Visibility
    m["isWindowTitleBarVisible"] = wsd.isWindowTitleBarVisible;
    m["isStatusBarVisible"] = wsd.isStatusBarVisible;
    m["isFolderDockVisible"] = wsd.isFolderDockVisible;
    m["isFavDockVisible"] = wsd.isFavDockVisible;
    m["isFilterDockVisible"] = wsd.isFilterDockVisible;
    m["isCatalogDockVisible"] = wsd.isCatalogDockVisible;
    m["isKeywordsDockVisible"] = wsd.isKeywordsDockVisible;
    m["isMetadataDockVisible"] = wsd.isMetadataDockVisible;
    m["isEmbelDockVisible"] = wsd.isEmbelDockVisible;
    m["isDevelopDockVisible"] = wsd.isDevelopDockVisible;
    m["isHistoryDockVisible"] = wsd.isHistoryDockVisible;
    m["isPresetsDockVisible"] = wsd.isPresetsDockVisible;
    m["isThumbDockVisible"] = wsd.isThumbDockVisible;
    // View
    m["isLoupeDisplay"] = wsd.isLoupeDisplay;
    m["isGridDisplay"] = wsd.isGridDisplay;
    m["isTableDisplay"] = wsd.isTableDisplay;
    m["isCompareDisplay"] = wsd.isCompareDisplay;
    // ThumbView
    m["thumbSpacing"] = wsd.thumbSpacing;
    m["thumbPadding"] = wsd.thumbPadding;
    m["thumbWidth"] = wsd.thumbWidth;
    m["thumbHeight"] = wsd.thumbHeight;
    m["labelFontSize"] = wsd.labelFontSize;
    m["showThumbLabels"] = wsd.showThumbLabels;
    // GridView
    m["thumbSpacingGrid"] = wsd.thumbSpacingGrid;
    m["thumbPaddingGrid"] = wsd.thumbPaddingGrid;
    m["thumbWidthGrid"] = wsd.thumbWidthGrid;
    m["thumbHeightGrid"] = wsd.thumbHeightGrid;
    m["labelFontSizeGrid"] = wsd.labelFontSizeGrid;
    m["showThumbLabelsGrid"] = wsd.showThumbLabelsGrid;
    m["labelChoice"] = wsd.labelChoice;
    // ImageView
    m["isImageInfoVisible"] = wsd.isImageInfoVisible;
    // Processes
    m["isColorManage"] = wsd.isColorManage;
    m["sortColumn"] = wsd.sortColumn;
    m["isReverseSort"] = wsd.isReverseSort;
    return m;
}

void MW::workspaceFromMap(const QVariantMap &m, WorkspaceData &wsd) const
{
    // Workspace
    wsd.name = m.value("name").toString();

    // State
    wsd.geometry = m.value("geometry").toByteArray();
    wsd.state = m.value("state").toByteArray();
    // absent = saved before the key existed, ie predates every versioned dock
    wsd.stateVersion = m.value("stateVersion", 0).toInt();
    /* screenNumber is derived from the geometry blob, not read back: the saved value is
       written for diagnostics only. */
    RecoverGeometry r;
    recoverGeometry(wsd.geometry, r);
    wsd.screenNumber = r.screenNumber;
    wsd.geometryRect = m.value("geometryRect").toRect();
    wsd.isFullScreen = m.value("isFullScreen").toBool();
    wsd.isMaximised = m.value("isMaximised").toBool();
    /*  Absent from a workspace saved before the window position/size became optional,
        which defaults to true -- the original behaviour. */
    wsd.isGeometryIncluded = m.value("isGeometryIncluded", true).toBool();

    // Visibility
    wsd.isWindowTitleBarVisible = m.value("isWindowTitleBarVisible").toBool();
    wsd.isStatusBarVisible = m.value("isStatusBarVisible").toBool();
    wsd.isFolderDockVisible = m.value("isFolderDockVisible").toBool();
    wsd.isFavDockVisible = m.value("isFavDockVisible").toBool();
    wsd.isFilterDockVisible = m.value("isFilterDockVisible").toBool();
    wsd.isCatalogDockVisible = m.value("isCatalogDockVisible").toBool();
    /*  Absent from a workspace saved before the Keywords dock existed, which reads as
        false -- the same as its default, so an old workspace needs no migration. */
    wsd.isKeywordsDockVisible = m.value("isKeywordsDockVisible").toBool();
    wsd.isMetadataDockVisible = m.value("isMetadataDockVisible").toBool();
    wsd.isEmbelDockVisible = m.value("isEmbelDockVisible").toBool();
    wsd.isDevelopDockVisible = m.value("isDevelopDockVisible").toBool();
    wsd.isHistoryDockVisible = m.value("isHistoryDockVisible").toBool();
    wsd.isPresetsDockVisible = m.value("isPresetsDockVisible").toBool();
    wsd.isThumbDockVisible = m.value("isThumbDockVisible").toBool();

    // View
    wsd.isLoupeDisplay = m.value("isLoupeDisplay").toBool();
    wsd.isGridDisplay = m.value("isGridDisplay").toBool();
    wsd.isTableDisplay = m.value("isTableDisplay").toBool();
    wsd.isCompareDisplay = m.value("isCompareDisplay").toBool();

    // ThumbView
    wsd.thumbSpacing = m.value("thumbSpacing").toInt();
    wsd.thumbPadding = m.value("thumbPadding").toInt();
    wsd.thumbWidth = m.value("thumbWidth").toInt();
    wsd.thumbHeight = m.value("thumbHeight").toInt();
    wsd.labelFontSize = m.value("labelFontSize").toInt();
    wsd.showThumbLabels = m.value("showThumbLabels").toBool();

    // GridView
    wsd.thumbSpacingGrid = m.value("thumbSpacingGrid").toInt();
    wsd.thumbPaddingGrid = m.value("thumbPaddingGrid").toInt();
    wsd.thumbWidthGrid = m.value("thumbWidthGrid").toInt();
    wsd.thumbHeightGrid = m.value("thumbHeightGrid").toInt();
    wsd.labelFontSizeGrid = m.value("labelFontSizeGrid").toInt();
    wsd.showThumbLabelsGrid = m.value("showThumbLabelsGrid").toBool();
    wsd.labelChoice = m.value("labelChoice").toString();

    // ImageView
    wsd.isImageInfoVisible = m.value("isImageInfoVisible").toBool();

    // Processes
    wsd.isColorManage = m.value("isColorManage").toBool();
    wsd.sortColumn = m.value("sortColumn").toInt();
    /* Sanitize a persisted sortColumn that is out of range (e.g. saved by a build with a
       different column layout; G::TotalColumns is one past the last real column). Left
       unchecked it reaches dm->sf->sort() as a phantom column — see IconView::sortThumbs. */
    if (wsd.sortColumn < 0 || wsd.sortColumn >= G::TotalColumns) wsd.sortColumn = G::NameColumn;
    wsd.isReverseSort = m.value("isReverseSort").toBool();
}

QJsonObject MW::workspaceToJson(const WorkspaceData &wsd) const
{
    QJsonObject o;
    const QVariantMap m = workspaceToMap(wsd);
    for (auto it = m.cbegin(); it != m.cend(); ++it) {
        const QString &key = it.key();
        if (kByteArrayKeys.contains(key)) {
            o.insert(key, QString::fromLatin1(it.value().toByteArray().toBase64()));
        }
        else if (kRectKeys.contains(key)) {
            const QRect r = it.value().toRect();
            o.insert(key, QJsonArray{r.x(), r.y(), r.width(), r.height()});
        }
        else {
            o.insert(key, QJsonValue::fromVariant(it.value()));
        }
    }
    return o;
}

void MW::workspaceFromJson(const QJsonObject &o, WorkspaceData &wsd) const
{
    QVariantMap m;
    for (auto it = o.constBegin(); it != o.constEnd(); ++it) {
        const QString &key = it.key();
        if (kByteArrayKeys.contains(key)) {
            m.insert(key, QByteArray::fromBase64(it.value().toString().toLatin1()));
        }
        else if (kRectKeys.contains(key)) {
            const QJsonArray a = it.value().toArray();
            if (a.size() == 4)
                m.insert(key, QRect(a.at(0).toInt(), a.at(1).toInt(),
                                    a.at(2).toInt(), a.at(3).toInt()));
        }
        else {
            m.insert(key, it.value().toVariant());
        }
    }
    workspaceFromMap(m, wsd);
}

void MW::readWorkspaceSettings(WorkspaceData &wsd)
{
/*
    Read one workspace from the current QSettings position (array index or group).
    Used by MW::loadWorkspaces and MW::loadWorkflowOverrides.
*/
    QVariantMap m;
    const QStringList keys = settings->childKeys();
    for (const QString &key : keys) m.insert(key, settings->value(key));
    workspaceFromMap(m, wsd);
}

void MW::writeWorkspaceSettings(const WorkspaceData &wsd)
{
/*
    Write one workspace to the current QSettings position (array index or group).
    Used by MW::saveWorkspaces and MW::saveWorkflowOverride.
*/
    const QVariantMap m = workspaceToMap(wsd);
    for (auto it = m.cbegin(); it != m.cend(); ++it) settings->setValue(it.key(), it.value());
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

    loadWorkflowOverrides();
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

void MW::recoverGeometry(const QByteArray &geometry, RecoverGeometry &r) const
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

/*  *******************************************************************************************

    WORKFLOW WORKSPACES

    A layout per workflow rather than one "default workspace" for the whole app.  The
    workflow's key applies it: E / G / C (Library), D (Develop) and K (Keywords).
    Embellish and Focus Stack are defined but have no key yet -- they are reached from
    Window > Workspace.

    Each workflow has TWO possible layouts:

    - THE SHIPPED DEFAULT, read from the resource ":/Workspaces/defaults.json".  Rory
      captures these (Window > Workspace > Default > "Set Default from Current
      Layout ...", a branch hidden unless G::isRory) and the write goes back into the
      SOURCE TREE, so the layout ships with the next build rather than living in one
      machine's QSettings.  A workflow with no entry in the resource falls back to
      MW::builtInDefaultWorkspace, so an empty defaults.json is a working state.

    - THE USER OVERRIDE, captured from the current layout when the user ticks the
      workflow in Window > Workspace > User override, and saved in the QSettings group
      "WorkflowWorkspaces/<key>".  Unticking goes back to the shipped default and KEEPS
      the user's copy, so re-ticking restores it; "Update Override from Current
      Layout ..." re-captures it.

    A workflow workspace deliberately does NOT own the window position and size
    (isGeometryIncluded false): switching workflow is about the panels, and pulling the
    window somewhere else on every E / G / C / D / K would be intolerable.  That is why
    a first run has to size the window itself -- see MW::centreWindowOnPrimaryScreen.
*/

const QStringList &MW::workflowKeys()
{
/*
    Stable identifiers for QSettings groups and the JSON resource.  NEVER renamed: an
    existing profile and every committed defaults.json are keyed on them.
*/
    static const QStringList keys{"Library", "Develop", "Keywords", "Embellish", "FocusStack"};
    return keys;
}

QStringList MW::workflowNames()
{
    return {tr("Library"), tr("Develop"), tr("Keywords"), tr("Embellish"), tr("Focus Stack")};
}

void MW::loadWorkflowDefaults()
{
/*
    Read the shipped per-workflow layouts from the resource.  A workflow the resource
    does not mention keeps isWorkflowDefault false and falls back to the built-in
    layout, which is the state a freshly cloned repo is in.
*/
    if (G::isLogger) G::log("MW::loadWorkflowDefaults");

    workflowDefaultWs.clear();
    isWorkflowDefault.clear();
    workflowUserWs.clear();
    isWorkflowOverride.clear();
    for (int wf = 0; wf < WfCount; ++wf) {
        workflowDefaultWs.append(WorkspaceData());
        isWorkflowDefault.append(false);
        workflowUserWs.append(WorkspaceData());
        isWorkflowOverride.append(false);
    }

    QFile f(":/Workspaces/defaults.json");
    if (!f.open(QIODevice::ReadOnly)) return;
    QJsonParseError err;
    const QJsonDocument doc = QJsonDocument::fromJson(f.readAll(), &err);
    f.close();
    if (err.error != QJsonParseError::NoError || !doc.isObject()) {
        G::issue("Warning", "Could not parse " + err.errorString(), "MW::loadWorkflowDefaults");
        return;
    }

    const QJsonObject root = doc.object();
    for (int wf = 0; wf < WfCount; ++wf) {
        const QString key = workflowKeys().at(wf);
        if (!root.contains(key) || !root.value(key).isObject()) continue;
        workspaceFromJson(root.value(key).toObject(), workflowDefaultWs[wf]);
        workflowDefaultWs[wf].name = workflowNames().at(wf);
        /* A workflow workspace never moves the window, whatever was captured. */
        workflowDefaultWs[wf].isGeometryIncluded = false;
        isWorkflowDefault[wf] = true;
    }
}

void MW::loadWorkflowOverrides()
{
/*
    Read the user's per-workflow layouts.  Called from MW::loadWorkspaces, ie after
    MW::loadWorkflowDefaults has sized the lists.
*/
    if (G::isLogger) G::log("MW::loadWorkflowOverrides");
    if (!isSettings) return;
    if (workflowUserWs.count() < WfCount) return;

    for (int wf = 0; wf < WfCount; ++wf) {
        settings->beginGroup("WorkflowWorkspaces/" + workflowKeys().at(wf));
        const bool isCaptured = settings->contains("state");
        if (isCaptured) {
            readWorkspaceSettings(workflowUserWs[wf]);
            workflowUserWs[wf].name = workflowNames().at(wf);
            workflowUserWs[wf].isGeometryIncluded = false;
            isWorkflowOverride[wf] = settings->value("isOverride", true).toBool();
        }
        settings->endGroup();
    }
}

void MW::saveWorkflowOverride(int wf)
{
    if (G::isLogger) G::log("MW::saveWorkflowOverride");
    if (wf < 0 || wf >= WfCount) return;

    const QString group = "WorkflowWorkspaces/" + workflowKeys().at(wf);
    settings->remove(group);
    settings->beginGroup(group);
    writeWorkspaceSettings(workflowUserWs.at(wf));
    settings->setValue("isOverride", isWorkflowOverride.at(wf));
    settings->endGroup();
}

bool MW::hasWorkflowOverride(int wf) const
{
/*
    Has the user ever captured a layout for this workflow?  The captured state blob is
    the tell -- a workspace that has one has been snapshotted.
*/
    if (wf < 0 || wf >= workflowUserWs.count()) return false;
    return !workflowUserWs.at(wf).state.isEmpty();
}

void MW::invokeWorkflowWorkspace(int wf)
{
/*
    Apply the layout for a workflow: the user's override when it is ticked, else the
    layout Winnow ships with, else the built-in layout.
*/
    if (G::isLogger) G::log("MW::invokeWorkflowWorkspace");
    if (wf < 0 || wf >= WfCount) return;

    if (wf < isWorkflowOverride.count() && isWorkflowOverride.at(wf) && hasWorkflowOverride(wf)) {
        invokeWorkspace(workflowUserWs.at(wf));
        return;
    }
    invokeWorkflowDefault(wf);
}

void MW::invokeWorkflowDefault(int wf)
{
/*
    Apply the layout Winnow ships with for a workflow, ignoring any user override.  This
    is what the Rory-only Default branch invokes, and the fallback for a workflow the
    resource does not define is the built-in layout.
*/
    if (G::isLogger) G::log("MW::invokeWorkflowDefault");
    if (wf < 0 || wf >= WfCount) return;

    if (wf < isWorkflowDefault.count() && isWorkflowDefault.at(wf)) {
        invokeWorkspace(workflowDefaultWs.at(wf));
        return;
    }
    builtInDefaultWorkspace();
}

void MW::toggleWorkflowOverride(int wf, bool isOverride)
{
/*
    Tick / untick a workflow in Window > Workspace > User override.

    Ticking a workflow that has never been captured SNAPSHOTS THE CURRENT LAYOUT -- the
    common case is "I have the panels how I want them for culling, make that my Library
    layout", and making that one click rather than two is the point.  Ticking one that
    has already been captured restores the kept copy instead, so unticking is not
    destructive.  "Update Override from Current Layout ..." is how a kept copy is
    replaced.
*/
    if (G::isLogger) G::log("MW::toggleWorkflowOverride");
    if (wf < 0 || wf >= WfCount) return;

    if (isOverride && !hasWorkflowOverride(wf)) {
        snapshotWorkspace(workflowUserWs[wf]);
        workflowUserWs[wf].name = workflowNames().at(wf);
        workflowUserWs[wf].isGeometryIncluded = false;
    }
    isWorkflowOverride[wf] = isOverride;
    saveWorkflowOverride(wf);
    syncWorkflowWorkspaceMenus();
}

void MW::captureWorkflowOverride()
{
/*
    Replace the user's layout for a workflow with the current one, and tick it.  The
    workflow is chosen in a list dialog rather than getting five more menu items.
*/
    if (G::isLogger) G::log("MW::captureWorkflowOverride");

    bool ok;
    const QStringList names = workflowNames();
    const QString name = QInputDialog::getItem(this, tr("Update Override"),
        tr("Save the current layout as the workspace for:"), names, 0, false, &ok);
    if (!ok) return;
    const int wf = names.indexOf(name);
    if (wf < 0) return;

    snapshotWorkspace(workflowUserWs[wf]);
    workflowUserWs[wf].name = names.at(wf);
    workflowUserWs[wf].isGeometryIncluded = false;
    isWorkflowOverride[wf] = true;
    saveWorkflowOverride(wf);
    syncWorkflowWorkspaceMenus();
}

void MW::captureWorkflowDefault()
{
/*
    Rory only.  Replace the SHIPPED layout for a workflow with the current one and write
    the whole resource back to the source tree, so the next build carries it.  Falling
    back to the desktop when the source tree is not there keeps the capture from being
    silently lost in a deployed build.
*/
    if (G::isLogger) G::log("MW::captureWorkflowDefault");

    bool ok;
    const QStringList names = workflowNames();
    const QString name = QInputDialog::getItem(this, tr("Set Workflow Default"),
        tr("Save the current layout as the shipped default for:"), names, 0, false, &ok);
    if (!ok) return;
    const int wf = names.indexOf(name);
    if (wf < 0) return;

    snapshotWorkspace(workflowDefaultWs[wf]);
    workflowDefaultWs[wf].name = names.at(wf);
    workflowDefaultWs[wf].isGeometryIncluded = false;
    isWorkflowDefault[wf] = true;

    QString path;
    QString err;
    if (writeWorkflowDefaultsJson(path, err)) {
        QMessageBox::information(this, tr("Workflow Default Saved"),
            tr("The %1 layout was written to\n\n%2\n\nRebuild to ship it.").arg(name, path));
    }
    else {
        QMessageBox::warning(this, tr("Workflow Default Not Saved"),
            tr("Could not write the workflow defaults:\n\n%1").arg(err));
    }
    syncWorkflowWorkspaceMenus();
}

bool MW::writeWorkflowDefaultsJson(QString &path, QString &err) const
{
/*
    Write every captured workflow default to Workspaces/defaults.json in the source tree
    (WINNOW_SOURCE_DIR, defined by CMake), so the file that is compiled into the resource
    is the file that is updated.  Without a source tree -- a deployed build, or a build
    system that does not define it -- the file goes to the desktop and the caller says
    where.
*/
    QString dir;
    #ifdef WINNOW_SOURCE_DIR
    const QString sourceDir = QString(WINNOW_SOURCE_DIR) + "/Workspaces";
    if (QFileInfo::exists(QString(WINNOW_SOURCE_DIR) + "/winnow.qrc")) {
        QDir().mkpath(sourceDir);
        dir = sourceDir;
    }
    #endif
    if (dir.isEmpty())
        dir = QStandardPaths::writableLocation(QStandardPaths::DesktopLocation);
    path = dir + "/defaults.json";

    QJsonObject root;
    for (int wf = 0; wf < WfCount; ++wf) {
        if (wf >= isWorkflowDefault.count() || !isWorkflowDefault.at(wf)) continue;
        root.insert(workflowKeys().at(wf), workspaceToJson(workflowDefaultWs.at(wf)));
    }

    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        err = f.errorString() + " (" + path + ")";
        return false;
    }
    f.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    f.close();
    return true;
}

void MW::syncWorkflowWorkspaceMenus()
{
/*
    Keep the two workflow branches of the Workspace menu in step with the data:

    - The Default branch exists only for Rory (G::isRory is false in a release build),
      and MW::rory re-runs this when it is toggled at runtime.
    - A Default item is greyed, with the reason in its tooltip, when the resource has no
      layout for that workflow -- it would apply the generic built-in layout instead, so
      saying so beats silently doing something else.
    - An override item shows ticked when the override is in use, and its tooltip says
      whether a layout has been captured for it.
*/
    if (G::isLogger) G::log("MW::syncWorkflowWorkspaceMenus");
    if (workspaceDefaultMenuAction == nullptr) return;

    workspaceDefaultMenuAction->setVisible(G::isRory);

    for (int wf = 0; wf < workflowDefaultActions.count(); ++wf) {
        QAction *a = workflowDefaultActions.at(wf);
        const bool isDefined = wf < isWorkflowDefault.count() && isWorkflowDefault.at(wf);
        a->setEnabled(isDefined);
        a->setToolTip(isDefined
            ? tr("Apply the %1 layout Winnow ships with").arg(workflowNames().at(wf))
            : tr("No %1 layout has been captured yet — Winnow's built-in layout is used")
                  .arg(workflowNames().at(wf)));
    }

    for (int wf = 0; wf < workflowOverrideActions.count(); ++wf) {
        QAction *a = workflowOverrideActions.at(wf);
        QSignalBlocker blocker(a);
        a->setChecked(wf < isWorkflowOverride.count() && isWorkflowOverride.at(wf));
        a->setToolTip(hasWorkflowOverride(wf)
            ? tr("Use your own %1 layout instead of the one Winnow ships with")
                  .arg(workflowNames().at(wf))
            : tr("Tick to save the current layout as your %1 workspace")
                  .arg(workflowNames().at(wf)));
    }
}
