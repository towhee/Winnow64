#include "Main/mainwindow.h"
#include "Utilities/panelprobe.h"

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

    /*  A named workspace is not a workflow.  invokeWorkflowWorkspace re-stamps this
        after the layout is applied, so the workflow routes still record themselves. */
    currentWorkflow = -1;
    syncWorkflowSwitcher();

    /*  Which panel was front in each tab group in the workspace being LEFT, so coming
        back to it comes back to the panel last used there and not to whatever was front
        when the layout was captured (see MW::restoreDockTabSelection).  Before ws is
        replaced below -- ws is the outgoing workspace until then.  An unnamed current
        workspace is the layout restored at startup, which is the Source one. */
    const QString leaving = ws.name.isEmpty() ? workflowNames().at(WfSource) : ws.name;
    rememberDockTabSelection(leaving);

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
    moduleDockVisibleAction->setChecked(w.isModuleDockVisible);
    metadataDockVisibleAction->setChecked(w.isMetadataDockVisible);
    embelDockVisibleAction->setChecked(w.isEmbelDockVisible);
    developDockVisibleAction->setChecked(w.isDevelopDockVisible);
    historyDockVisibleAction->setChecked(w.isHistoryDockVisible);
    presetsDockVisibleAction->setChecked(w.isPresetsDockVisible);
    thumbDockVisibleAction->setChecked(w.isThumbDockVisible);
    /*  The Info overlay is NOT applied from the workspace.  It is a per-user display
        preference (the I key, View > Show Info Overlay), kept in QSettings
        "isImageInfoVisible" alongside the rating badge and icon number toggles -- see
        MW::writeSettings.  Applying it here made the toggle transient: every workspace
        bakes a value, the shipped defaults all bake TRUE, and E / G / C / K / D apply a
        workspace every time, so an overlay switched off came straight back on -- and
        showEvent's Source reassertion after a session left in Develop lost the choice
        across a restart as well.  The field is still snapshotted and serialised so the
        QSettings and defaults.json layout is unchanged; it is simply never applied. */
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
    // ImageView: see the Info overlay note above -- w.isImageInfoVisible is not applied.
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
    /* Unconditional, at every version -- see MW::placeShowHideBars. A workspace carries
       each panel's visibility, so any area the bars had collapsed is now expanded; the
       collapse map is dropped to match rather than left claiming otherwise. */
    areaCollapsed.clear();
    areaCollapsedExtent.clear();
    placeShowHideBars();

    /*  Re-raise the panel last used in each tab group in this workspace, overriding the
        front tab the state blob carries.  Done here, synchronously, so a caller that
        raises a panel of its own afterwards (D raising the Develop panel as it enters
        Develop mode) still wins. */
    restoreDockTabSelection(w.name);

    /*  A workspace switch is the other route to a panel the wrong size, and it uses the
        same restoreState the startup path does -- so it is marked the same way, and
        SETTLED as well as immediately: the dock area redistributes after this returns. */
    if (G::isPanelProbe) {
        PanelProbe::Instance().Mark("invokeWorkspace \"" + w.name + "\" restored");
        PanelProbe::Instance().MarkSettled("invokeWorkspace \"" + w.name + "\"");
    }

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
    wsd.isModuleDockVisible = moduleDockVisibleAction->isChecked();
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
        /* Unconditional, at every version: the bars are not user-movable, so the
           restored state has no placement of theirs worth honouring. */
        placeShowHideBars();
        /*  WHICH VERSION the saved layout came back at.  A state restored at an older
            version has had placeDocksAddedSince write dock positions that the user never
            chose, which is a candidate explanation for a panel opening narrow. */
        if (G::isPanelProbe)
            PanelProbe::Instance().Mark(
                QString("restoreWindowState succeeded at version %1 (current %2)")
                    .arg(v).arg(winnowStateVersion));
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
        {4, catalogDock, filterDock, catalogDockVisibleAction},
        {5, keywordsDock, filterDock, keywordsDockVisibleAction},
        /* Top area, never tabbed; MW::placeShowHideBars pins it there. */
        {8, moduleDock, nullptr, moduleDockVisibleAction},
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
    window before the Source layout is applied.  See MW::showEvent.
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
    /*  NO centreWindowOnPrimaryScreen here. This runs on a WORKFLOW switch (a workflow
        with no shipped layout -- Map, until one is captured), and a workflow workspace
        never owns the window position (see isGeometryIncluded). Centring here threw
        the window onto the primary display the first time Map was clicked, which on a
        second monitor looked like Winnow quitting. A first run centres the window
        itself (MW::showEvent) before it applies Source. */
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
    moduleDockVisibleAction->setChecked(true);
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
    if (G::isPanelProbe)
        PanelProbe::Instance().NoteRequest("FolderDock", "resizeDocks horizontal", 350);
    resizeDocks({folderDock}, {350}, Qt::Horizontal);

    // enable the folder dock (first one in tab)
    QList<QTabBar *> tabList = findChildren<QTabBar *>();
    QTabBar* widgetTabBar = tabList.at(0);
    widgetTabBar->setCurrentIndex(0);

    if (G::isPanelProbe)
        PanelProbe::Instance().NoteRequest("ThumbDock", "resizeDocks vertical", 100);
    resizeDocks({thumbDock}, {100}, Qt::Vertical);

    /* The default layout puts every panel back, so nothing is collapsed any more. */
    areaCollapsed.clear();
    areaCollapsedExtent.clear();
    placeShowHideBars();

    setThumbDockFeatures(dockWidgetArea(thumbDock));

    if (G::isPanelProbe) {
        PanelProbe::Instance().Mark("builtInDefaultWorkspace applied");
        PanelProbe::Instance().MarkSettled("builtInDefaultWorkspace");
    }

    asLoupeAction->setChecked(true);
    /* The Info overlay is the user's preference, not part of a layout (see the note in
       MW::invokeWorkspace), so the built-in layout leaves it as the user set it. */
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
            << "\n  isModuleDockVisible       " << G::s(ws.isModuleDockVisible)
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
        << "\nisModuleDockVisible" << ws.isModuleDockVisible
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
    m["isModuleDockVisible"] = wsd.isModuleDockVisible;
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
    /*  Absent from every workspace and shipped default saved before the Module dock
        existed: show it, since it is where the workflow is chosen. */
    wsd.isModuleDockVisible = m.value("isModuleDockVisible", true).toBool();
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
    workflow's key applies it: E / G / C (Source), D (Develop) and K (Keywords).
    Embellish and Slide Show are defined but have no key yet -- they are reached from
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
    Stable identifiers for QSettings groups and the JSON resource.  Not renamed lightly:
    an existing profile and every committed defaults.json are keyed on them, so a rename
    strands the user's captured override unless it is migrated.

    ONE HAS BEEN RENAMED.  "Library" became "Source" when the nomenclature was settled:
    SOURCE is where the images come from (a Catalog or Folders -- the two halves of the
    Source panel), and LIBRARY is the catalogue plus the keywords, which is not what this
    layout is about.  MW::migrateWorkflowKey moves an existing group across.
*/
    static const QStringList keys{"Source", "Develop", "Keywords", "Embellish", "SlideShow",
                                  "Map"};
    return keys;
}

QStringList MW::workflowNames()
{
    /*  "Browse", not "Source": the SOURCE is Library | Folders (the Source panel), which
        every workflow works on. The key stays "Source" (see workflowKeys), so this is a
        display rename only. The name also keys the session-only front-tab memory
        (MW::rememberDockTabSelection), which is why the rename costs nothing there. */
    return {tr("Browse"), tr("Develop"), tr("Keywords"), tr("Embellish"), tr("Slide Show"),
            tr("Map")};
}

void MW::migrateWorkflowKey(const QString &from, const QString &to)
{
/*
    Move a captured workflow override from an old key to its new one.

    A workflow's key names its QSettings group ("WorkflowWorkspaces/<key>"), so renaming
    the key would otherwise orphan the layout the user captured: loadWorkflowOverrides
    would find nothing under the new name and silently fall back to the shipped default,
    which reads as "my workspace was thrown away".

    The captured state blob is the tell that a group is really there, the same test
    MW::hasWorkflowOverride uses.  Copy every key across and drop the old group -- a
    one-shot, because the new group exists afterwards and the guard below is false from
    then on.  Never overwrites: a user who has already captured under the new name keeps
    that one.
*/
    if (G::isLogger) G::log("MW::migrateWorkflowKey", from + " -> " + to);
    const QString oldGroup = "WorkflowWorkspaces/" + from;
    const QString newGroup = "WorkflowWorkspaces/" + to;
    if (!settings->contains(oldGroup + "/state")) return;
    if (settings->contains(newGroup + "/state")) return;

    settings->beginGroup(oldGroup);
    const QStringList keys = settings->allKeys();
    QVariantMap values;
    for (const QString &key : keys) values.insert(key, settings->value(key));
    settings->endGroup();

    /*  "name" is the one field that carries the OLD name across: it is the display name
        of the workspace, and copying it verbatim would leave "name=Library" sitting in
        the migrated group.  Harmless -- loadWorkflowOverrides stamps the name from
        workflowNames() straight after reading -- but a stale word in the settings file
        is exactly what a rename is supposed to remove, so it is restamped here too. */
    const int wf = workflowKeys().indexOf(to);
    if (wf >= 0 && values.contains("name")) values.insert("name", workflowNames().at(wf));

    settings->beginGroup(newGroup);
    for (auto i = values.cbegin(); i != values.cend(); ++i) settings->setValue(i.key(), i.value());
    settings->endGroup();

    /* remove() on a group path drops the whole group -- see MW::saveWorkflowOverride. */
    settings->remove(oldGroup);
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

    /*  Before anything is read: an override captured under a key that has since been
        renamed still belongs to the user.  See MW::workflowKeys. */
    migrateWorkflowKey("Library", "Source");

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
    if (G::isPanelProbe)
        PanelProbe::Instance().Mark(QString("invokeWorkflowWorkspace(%1) enter").arg(wf));

    if (wf < isWorkflowOverride.count() && isWorkflowOverride.at(wf) && hasWorkflowOverride(wf))
        invokeWorkspace(workflowUserWs.at(wf));
    else
        invokeWorkflowDefault(wf);
    currentWorkflow = wf;
    syncWorkflowSwitcher();
    /* A workspace carries a Browse view (Loupe, Grid ...) and invokeWorkspace just put
       it up; the Map workflow's page is the map instead. Here rather than in
       invokeMapWorkflow so Reset Layout and a resumed session get the map too. */
    if (wf == WfMap) showMapPage();
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
    common case is "I have the panels how I want them for culling, make that my Source
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

/*  THE UI MODEL: SOURCE x WORKFLOW x VIEW *****************************************

    Three settings, each with one home and its own keys, none changing another:

    SOURCE    Library | Folders.  The Source panel's toggle, Ctrl+Shift+L / Ctrl+Shift+F,
              View > Library / Folders.  Every workflow works on the current source and
              selection, so switching source never changes the workflow.
    WORKFLOW  Browse, Develop, Keywords, Embellish, Slide Show, Map.  The Module dock, D
              and K.
              The only setting that owns a workspace (panel layout).
    VIEW      Loupe, Grid, Table, Compare.  E / G / T / C.  These are the BROWSE views:
              a view key always lands in Browse (MW::requestView).

    The Slide Show module is a workflow only while its show runs: its button starts the
    show and stopping it returns to the previous workflow.  S starts and stops a show
    without changing workflow.  Window > Workspace > Reset Layout is how a layout that
    has got away from the user is recovered.
*/

void MW::requestView(int view)
{
/*
    E / G / T / C: show a central view, in Browse.

    From another workflow -- or from Develop mode under any layout -- this leaves it: out
    of Develop first (setOperationMode restores the Preview decode / read-ahead and greys
    the Develop panel before the view is shown), then the Browse layout, then the view.

    ALREADY IN BROWSE IT ONLY CHANGES THE VIEW.  The view keys used to reassert the Browse
    layout every time, as the way to recover a stranded panel; that made a view key move
    panels as a side effect, and it is now MW::resetLayout's job.  A named workspace
    (currentWorkflow -1) is the user's own layout and is left alone the same way.
*/
    if (G::isLogger) G::log("MW::requestView", QString::number(view));
    const bool inDevelop = G::operationMode == G::OperationMode::Develop;
    const bool otherWorkflow = currentWorkflow >= 0 && currentWorkflow != WfSource;
    setOperationMode(G::OperationMode::Preview);
    if (otherWorkflow || inDevelop) invokeWorkflowWorkspace(WfSource);

    switch (view) {
    case CvLoupe:   loupeDisplay("MW::requestView"); break;
    case CvGrid:    gridDisplay();                  break;
    case CvTable:   tableDisplay();                 break;
    case CvCompare: compareDisplay();               break;
    default: return;
    }
    browseView = view;
}

void MW::invokeBrowseWorkflow()
{
/*
    The switcher's Browse button and View > Browse Mode: Browse, in the view it was last
    shown in.  Unlike a view key this applies the Browse layout from a named workspace
    too -- asking for Browse by name is asking for its layout.
*/
    if (G::isLogger) G::log("MW::invokeBrowseWorkflow");
    setOperationMode(G::OperationMode::Preview);
    if (currentWorkflow != WfSource) invokeWorkflowWorkspace(WfSource);
    requestView(browseView);
}

void MW::invokeEmbellishWorkflow()
{
/*
    The Embellish workflow layout, the way K applies Keywords.  Leaves Develop first; a
    no-op when already in Preview.
*/
    if (G::isLogger) G::log("MW::invokeEmbellishWorkflow");
    setOperationMode(G::OperationMode::Preview);
    invokeWorkflowWorkspace(WfEmbellish);
}

void MW::invokeSlideShowWorkflow()
{
/*
    The Slide Show module button (and View > Slide Show Mode): apply the Slide Show
    layout and start the show. The module is the show -- a lit Slide Show button with
    nothing playing would be a mode with nothing to do -- so stopping the show (Esc, S,
    the end of an unwrapped sequence) returns to the workflow it was entered from, via
    MW::leaveSlideShowWorkflow.

    ALWAYS THE SHIPPED DEFAULT LAYOUT (MW::invokeWorkflowDefault), never the user
    override: a show is a presentation, so it looks the same every time. The override
    is still what Reset Layout applies while in the Slide Show module.

    Clicked while a show is running, it stops it. S alone still starts and stops a show
    in whatever layout is up, and does not change workflow.
*/
    if (G::isLogger) G::log("MW::invokeSlideShowWorkflow");
    if (G::isSlideShow) {
        slideShow();
        return;
    }
    if (!slideShowAction->isEnabled()) return;
    const int from = currentWorkflow;
    if (from == -1) slideShowReturnWs = ws;     // invokeWorkspace overwrites ws
    setOperationMode(G::OperationMode::Preview);
    invokeWorkflowDefault(WfSlideShow);
    currentWorkflow = WfSlideShow;      // invokeWorkspace stamped -1 (a named workspace)
    syncWorkflowSwitcher();
    slideShowReturnWorkflow = from == WfSlideShow ? WfSource : from;
    slideShow();
}

void MW::leaveSlideShowWorkflow()
{
/*
    Called by MW::slideShow as a show stops: go back to the workflow the Slide Show
    module was entered from, the way its own button would (so Develop re-enters Develop
    mode). Nothing when the show was started by S, or when the layout has already been
    changed away from Slide Show.
*/
    const int wf = slideShowReturnWorkflow;
    slideShowReturnWorkflow = kNoSlideShowReturn;
    if (wf == kNoSlideShowReturn || currentWorkflow != WfSlideShow) return;
    if (G::isLogger) G::log("MW::leaveSlideShowWorkflow", QString::number(wf));

    QAction *a = nullptr;
    switch (wf) {
    case -1:          invokeWorkspace(slideShowReturnWs); return;   // named workspace
    case WfSource:    invokeBrowseWorkflow(); return;
    case WfDevelop:   a = operationModeAction; break;
    case WfKeywords:  a = keywordsWorkspaceAction; break;
    case WfEmbellish: a = embellishWorkspaceAction; break;
    case WfMap:       a = mapWorkspaceAction; break;
    default: break;
    }
    if (a && a->isEnabled()) a->trigger();
    else invokeBrowseWorkflow();
}

void MW::invokeMapWorkflow()
{
/*
    The Map module: its workflow layout, with the map as the central page (put up by
    invokeWorkflowWorkspace). Leaves Develop first, like every Preview workflow.
*/
    if (G::isLogger) G::log("MW::invokeMapWorkflow");
    setOperationMode(G::OperationMode::Preview);
    invokeWorkflowWorkspace(WfMap);
}

void MW::showMapPage()
{
/*
    Put the map up as the central page. G::mode is left as the Browse view it was (it
    is Loupe / Grid / Table / Compare everywhere else, compared in some fifty places);
    the paths that would otherwise take the page back on their own -- a selection
    change, an image arriving in the cache, a re-sort -- check inMapModule instead.
*/
    if (G::isLogger) G::log("MW::showMapPage");
    if (!mapView) return;
    centralLayout->setCurrentIndex(MapTab);
}

bool MW::inMapModule() const
{
/*
    The WORKFLOW, not the page: while a folder loads the page is the message page, and
    MW::fileSelectionChange uses this to put the map back up afterwards. Every way out
    of Map changes currentWorkflow (invokeWorkspace, invokeWorkflowWorkspace).
*/
    return mapView && currentWorkflow == WfMap;
}

void MW::applyMapProvider()
{
/*
    The map style (MW::mapStyle, chosen from the map's style menu) picks the tiles:
    one of the keyless built-ins, or "custom", the URL template in Preferences > Map.
    Custom with no URL falls back to Standard. A custom template that points at an
    OpenStreetMap server -- openstreetmap.org, or a local chapter's such as
    openstreetmap.fr -- keeps the 2-connection limit those volunteer-run servers ask
    for.
*/
    if (!mapView) return;
    const bool customAvailable = !mapTileUrl.trimmed().isEmpty();
    if (mapStyle == "custom" && !customAvailable) mapStyle = "standard";

    MapTileSource::Provider p;
    if (mapStyle == "cyclosm") p = MapTileSource::Provider::cyclOsm();
    else if (mapStyle == "opentopo") p = MapTileSource::Provider::openTopoMap();
    else if (mapStyle == "custom") {
        p.urlTemplate = mapTileUrl.trimmed();
        p.apiKey = mapTileKey.trimmed();
        p.attribution = mapAttribution.trimmed();
        p.maxZoom = std::clamp(mapMaxZoom, 1, 22);
        if (p.urlTemplate.contains("openstreetmap.")) {
            p.maxConnections = 2;
            if (p.attribution.isEmpty()) p.attribution = "© OpenStreetMap contributors";
        }
    }
    else {
        mapStyle = "standard";
        p = MapTileSource::Provider::openStreetMap();
    }
    mapView->setProvider(p);
    mapView->setStyleState(mapStyle, customAvailable);
}

void MW::resetLayout()
{
/*
    Window > Workspace > Reset Layout: reapply the current workflow's layout (the user's
    override when ticked, else the shipped one), or the named workspace in use.  The
    recovery for a panel left open or a dock stranded on a monitor that is gone.

    Neither the operation mode nor the view changes: a workflow layout carries a central
    view of its own, so Browse's view is put back afterwards.
*/
    if (G::isLogger) G::log("MW::resetLayout");
    if (currentWorkflow < 0) {
        invokeWorkspace(ws);
        return;
    }
    const int wf = currentWorkflow;
    invokeWorkflowWorkspace(wf);
    if (wf == WfSource) requestView(browseView);
}

void MW::syncWorkflowSwitcher()
{
/*
    Light the button for currentWorkflow in the Module dock and in the (hidden)
    status-bar switcher; none while a named workspace is in use.  Set programmatically,
    so a click the workflow refused shows the truth again.
*/
    for (const QList<QToolButton *> *btns : {&moduleBtns, &workflowBtns}) {
        for (int wf = 0; wf < btns->count(); ++wf) {
            QToolButton *btn = btns->at(wf);
            if (!btn) continue;
            QSignalBlocker block(btn);
            btn->setChecked(wf == currentWorkflow);
        }
    }
}

void MW::styleWorkflowSwitcher()
{
/*
    The Library | Folders look (MW::segmentedOptionCss) for both workflow rows: the Module
    dock at 1.5x G::fontSize, the hidden status-bar switcher at the normal size.  From
    the palette and the font size, so MW::setBackgroundShade and MW::setFontSize call
    this again.

    The Module dock's height is FIXED to its text afterwards: the top dock area has a
    splitter against the central widget, and a strip of buttons has no use for being
    dragged taller.
*/
    /* Not bold, in either row: at the Module dock's size the yellow alone is enough. */
    const QString css = segmentedOptionCss(0, /*boldSelected*/ false);
    for (QToolButton *btn : std::as_const(workflowBtns))
        if (btn) btn->setStyleSheet(css);

    if (!moduleDock || !moduleDock->content()) return;
    moduleDock->content()->setStyleSheet(segmentedOptionCss(qRound(1.5 * G::fontSize), false));
    moduleDock->content()->adjustSize();
    /* widget() is the content's FrameLineBox; its hint includes any frameLine inset. */
    moduleDock->widget()->adjustSize();
    moduleDock->setFixedHeight(moduleDock->widget()->sizeHint().height());
}

void MW::updateWindowTitle()
{
/*
    "Winnow <version>   Library  <file>": the source is named in the title as well as in
    the Source panel, which may be hidden (the Develop layout hides it).  titleFilePath is
    the current file, set by the selection and cleared by a new load.
*/
    QString title = winnowWithVersion + "   "
                  + (G::scope == G::Scope::Catalog ? tr("Library") : tr("Folders"));
    if (!titleFilePath.isEmpty()) title += "   " + titleFilePath;
    setWindowTitle(title);
}
