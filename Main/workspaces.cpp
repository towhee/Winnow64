#include "Main/mainwindow.h"
#include "Utilities/panelprobe.h"
#include "Utilities/titlebar.h"
#include "Utilities/screenfit.h"

/*  *******************************************************************************************

    WORKSPACES

    Need to track:
        - workspace number (n) for shortcut, QSettings name
        - workspace menu description
        - workspace geometry
        - workspace state
        - workspace dock visibility and lock mode
        - thumb parameters (size, spacing, padding, label)

    The user can change the workspace menu name and delete menu items.

    The data for each workspace is held in a workspaceData struct.  Up to 10 CUSTOM
    workspaces are contained in QList<workspaceData> workspaces.  Each belongs to the
    workflow it was saved in (WorkspaceData::workflow) and stands in for that workflow's
    layout when invoked -- see MW::invokeCustomWorkspace and the WORKFLOW WORKSPACES
    banner below.

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
        QString msg = "Only ten custom workspaces allowed.  Use Manage Custom Workspaces\n"
                      "to delete workspaces.";
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
        /* Copied from ws, which may be a workflow layout: a custom workspace owns its
           grid size (see MW::invokeWorkspace). */
        (*workspaces)[n].isWorkflowLayout = false;
        /* It belongs to the workflow it was saved in, and is the layout in use now. */
        (*workspaces)[n].workflow = currentWorkflow;
        activeCustomWs = n;
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
    invokeCustomWorkspace(i);
}

void MW::invokeCustomWorkspace(int i)
{
/*
    Enter custom workspace i's workflow by triggering the workflow's own action, so
    Develop enters Develop mode and the Module dock lights the workflow, with the custom
    layout standing in for the workflow's (MW::invokeWorkflowLayout).  A workflow that is
    not available says why, the way a disabled shortcut does.
*/
    if (G::isLogger) G::log("MW::invokeCustomWorkspace", QString::number(i));
    if (i < 0 || i >= workspaces->count()) return;
    enterWorkflowLayout(workspaces->at(i).workflow, i, false);
}

void MW::enterWorkflowLayout(int wf, int customWs, bool isWinnowDefault)
{
/*
    Enter workflow wf with a chosen layout: custom workspace customWs (-1 for none), or
    its Winnow default even when the user has saved a layout, or else the layout it
    resolves to.  The workflow's own action does the entering, with keepCustomWs set so
    the resolver (MW::invokeWorkflowLayout) honours the choice.
*/
    if (G::isLogger) G::log("MW::enterWorkflowLayout", QString::number(wf));
    QAction *a = workflowAction(wf);
    if (!a) return;
    if (!a->isEnabled()) {
        G::popup->showPopup(actionDisabledReason(a), 2000);
        return;
    }
    activeCustomWs = customWs;
    winnowDefaultWf = isWinnowDefault ? wf : -1;
    keepCustomWs = true;
    a->trigger();
    keepCustomWs = false;
}

void MW::showWorkflowContextMenu(int wf, QToolButton *btn, const QPoint &pos)
{
/*
    Right-click on a Module dock workflow button: the layouts that workflow can open
    with.  Winnow default, the user's own layout, Add current layout and Manage custom
    workspaces (this workflow's only), then the custom workspaces that belong to it.  The one in use (or, for another workflow, the one it would open with) is
    ticked.  Add current layout saves a custom workspace, so it needs the workflow to
    be the one on screen.
*/
    if (G::isLogger) G::log("MW::showWorkflowContextMenu", QString::number(wf));
    if (wf < 0 || wf >= WfCount || !btn) return;
    const int lay = workflowLayout(wf);
    const QString name = workflowNames().at(lay);
    const bool isCurrent = wf == currentWorkflow;
    const bool hasUser = hasUserLayout(lay);
    const bool isCustomInUse = isCurrent && activeCustomWs >= 0;
    const bool isDefaultInUse = !isCustomInUse
                                && (!hasUser || (isCurrent && winnowDefaultWf == wf));

    QMenu menu(this);
    menu.setToolTipsVisible(true);

    QAction *def = menu.addAction(tr("Winnow default"));
    def->setCheckable(true);
    def->setChecked(isDefaultInUse);
    def->setToolTip(tr("Open %1 with the layout Winnow ships with").arg(name));
    connect(def, &QAction::triggered, this, [this, wf] {
        enterWorkflowLayout(wf, -1, true);
    });

    QAction *user = menu.addAction(tr("User override"));
    user->setCheckable(true);
    user->setChecked(!isCustomInUse && !isDefaultInUse);
    user->setEnabled(hasUser);
    user->setToolTip(hasUser
        ? tr("Open %1 with the layout you saved").arg(name)
        : tr("No %1 layout saved yet.  Window > Workspace > Save Layout saves one.")
              .arg(name));
    connect(user, &QAction::triggered, this, [this, wf] {
        enterWorkflowLayout(wf, -1, false);
    });

    menu.addSeparator();

    QAction *add = menu.addAction(tr("Add current layout ..."));
    add->setEnabled(isCurrent);
    add->setToolTip(isCurrent
        ? tr("Save the current layout as a custom %1 workspace")
              .arg(customWorkflowName(wf))
        : tr("The layout on screen belongs to %1.  Open %2 first.")
              .arg(customWorkflowName(currentWorkflow), customWorkflowName(wf)));
    connect(add, &QAction::triggered, this, &MW::newWorkspace);

    int customCount = 0;
    for (int i = 0; i < workspaces->count(); ++i)
        if (workspaces->at(i).workflow == wf) ++customCount;
    QAction *manage = menu.addAction(tr("Manage custom workspaces ..."));
    manage->setEnabled(customCount > 0);
    manage->setToolTip(customCount > 0
        ? tr("Rename or delete the %1 custom workspaces").arg(customWorkflowName(wf))
        : tr("There are no %1 custom workspaces yet").arg(customWorkflowName(wf)));
    connect(manage, &QAction::triggered, this, [this, wf] { manageWorkspaces(wf); });

    bool isSeparated = false;
    for (int i = 0; i < workspaces->count(); ++i) {
        if (workspaces->at(i).workflow != wf) continue;
        if (!isSeparated) {
            menu.addSeparator();
            isSeparated = true;
        }
        /* The shortcut is shown, not assigned: the Workspace menu's action owns it. */
        const QString key = QKeySequence("Ctrl+" + QString::number(i))
                                .toString(QKeySequence::NativeText);
        QAction *c = menu.addAction(workspaces->at(i).name + "\t" + key);
        c->setCheckable(true);
        c->setChecked(isCurrent && activeCustomWs == i);
        connect(c, &QAction::triggered, this, [this, i] { invokeCustomWorkspace(i); });
    }

    menu.exec(btn->mapToGlobal(pos));
}

QString MW::workspaceMenuName(const WorkspaceData &w) const
{
/*
    The Workspace menu text for a custom workspace: its name and workflow.  A trailing
    " *" flags a workspace that restores the window position and size as well as the
    layout.
*/
    QString text = w.name + "  —  " + customWorkflowName(w.workflow);
    return w.isGeometryIncluded ? text + " *" : text;
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
        - processes (color manage, caching) -- NOT the sort, see below


    It is called from menu actions, MW::toggleFullScreen and QEvent::WindowStateChange
    in MW::eventFilter.

    There is an issue when attempting to invoke a workspace while in FullScreen state if
    the new sworkspace is on a different screen.  The function showNormal() always shows
    the window in the same screen and this takes some time.  MW::eventFilter overrides the
    QEvent::WindowStateChange and calls invokeWorkspace after a delay to allow the showNormal
    function to complete drawing the normal window.
*/
    if (G::isLogger) G::log("MW::invokeWorkspace");

    /*  NO PAINT UNTIL THE LAYOUT IS COMPLETE.  Applying a workspace is not one step:
        updateState puts up the workspace's central view BEFORE restoreState moves the
        docks, and anything that pumps the event loop in between paints the half-applied
        window.  gridDisplay / tableDisplay's G::wait(100) did (since removed -- the hold
        stays, as the guard against the next pump).  Leaving Develop for a Grid
        layout (Library: D then E) showed the grid filling the whole window, the Develop
        dock already hidden and the thumb dock hidden by gridDisplay, before the Library
        docks came back.  Painting is held for the main window and its docked children
        and released (one full repaint) on every return path.  Floating docks are their
        own windows and are not held.  Restores the prior state, so it nests.

        THE RELEASE MUST POST ITS OWN UpdateRequest.  setUpdatesEnabled(true) alone left
        the window showing the OLD layout (D then E "did nothing").  An UpdateRequest
        handled during the hold (the G::wait pump) runs QWidgetRepaintManager::sync,
        which clears updateRequestSent, finds the top level updates-disabled and returns
        keeping the whole window dirty; the update() that re-enabling does then finds
        its rect "already dirty" and posts nothing (Qt 6.11 qwidgetrepaintmanager.cpp
        markDirty / paintAndFlush).  Posted, not repaint(): one paint when the queue
        turns, after the caller's own view switch (requestView). */
    struct PaintHold {
        QWidget *w;
        bool was;
        explicit PaintHold(QWidget *w) : w(w), was(w->updatesEnabled())
        { w->setUpdatesEnabled(false); }
        ~PaintHold()
        {
            if (!was) return;
            w->setUpdatesEnabled(true);
            QCoreApplication::postEvent(w, new QEvent(QEvent::UpdateRequest),
                                        Qt::LowEventPriority);
        }
    } paintHold(this);

    /*  The view this layout puts up (updateState -> setCentralView) is the LAYOUT'S, not
        a view key: loupeDisplay and the rest must not route it through requestView to
        leave Map.  They did once currentWorkflow stopped being cleared here, and
        Map -> Embellish recursed without end.  Restored, so it nests. */
    struct ApplyingFlag {
        bool &flag;
        bool was;
        explicit ApplyingFlag(bool &f) : flag(f), was(f) { flag = true; }
        ~ApplyingFlag() { flag = was; }
    } applying(isApplyingWorkspace);


    /*  Which panel was front in each tab group in the workspace being LEFT, so coming
        back to it comes back to the panel last used there and not to whatever was front
        when the layout was captured (see MW::restoreDockTabSelection).  Before ws is
        replaced below -- ws is the outgoing workspace until then.  An unnamed current
        workspace is the layout restored at startup, which is a Browse one. */
    const QString leaving = ws.name.isEmpty() ? workflowNames().at(WfSource) : ws.name;
    rememberDockTabSelection(leaving);
    /*  Reset Winnow defaults: after the remember above, since a reset of the layout on
        screen is leaving the very workspace it applies. */
    if (isFrontDockTabsReset) frontDockTabsByWorkspace.remove(w.name);

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
    collectionsDockVisibleAction->setChecked(w.isCollectionsDockVisible);
    queriesDockVisibleAction->setChecked(w.isQueriesDockVisible);
    placesDockVisibleAction->setChecked(w.isPlacesDockVisible);
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
    /*  THE GRID THUMBNAIL SIZE IS NOT PART OF A WORKFLOW LAYOUT (2026-10-27).  It is a
        user preference, persisted in QSettings "thumbWidthGrid" (MW::writeSettings),
        but every workflow layout carries one too and Browse's is re-applied at startup
        (MW::showEvent), so the shipped Library layout's 125 replaced the saved size on
        every launch, and again on every return from Develop or another workflow.  A
        CUSTOM workspace still applies it: the user saved that size on purpose. */
    if (!w.isWorkflowLayout) {
        gridView->iconWidth = w.thumbWidthGrid;
        gridView->iconHeight = w.thumbHeightGrid;
        gridView->assignedIconWidth = gridView->iconWidth;
    }
    gridView->labelFontSize = w.labelFontSizeGrid;
    gridView->showIconLabels = w.showThumbLabelsGrid;
    gridView->labelChoice = w.labelChoice;
    gridView->rejustify();
    gridView->setThumbParameters();
    // ImageView: see the Info overlay note above -- w.isImageInfoVisible is not applied.
    /*  COLOUR MANAGEMENT IS NOT PART OF A LAYOUT (removed 2026-10-26). It belongs to the
        screen, not the workflow, and the shipped defaults.json carried false for every
        workspace, so switching to Develop or Library silently turned it off -- the
        oversaturated Develop loupe of notes/Documentation.md 15.2.2. Settings files from
        older builds still hold isColorManage; it is ignored. */
    /*  THE SORT IS NOT PART OF A LAYOUT.  w.sortColumn / w.isReverseSort are still
        snapshotted and serialised (the QSettings and defaults.json shape is unchanged)
        but never applied -- the same decision as the Info overlay above.  Applying them
        made the sort follow the WORKFLOW: the shipped Develop layout carries column 0,
        so D threw away the user's Created sort and E brought back the Library layout's
        File Name (probe trace, 2026-10-01).  Every switch also re-sorted the whole
        model unconditionally (sortChange, 156-196 ms at 155k rows) even when the column
        had not changed, and the re-sort moved videos out of the icon chunk, where
        eviction blanked their thumbnails.  The sort belongs to the content: the sort
        menu, the table header, and the Library state restore. */
    updateState();
    workspaceChanged = true;
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
        // saved on another (larger) screen: keep it clear of the taskbar
        ScreenFit::fit(this);
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
    /* Unconditional, at every version -- see MW::placeShowHideBars. The collapse map
       belongs to the layout being left; the workspace's own collapse state is applied
       below, once the docks are where it puts them. */
    areaCollapsed.clear();
    areaCollapsedExtent.clear();
    placeShowHideBars();

    /*  Re-raise the panel last used in each tab group in this workspace, overriding the
        front tab the state blob carries.  Done here, synchronously, so a caller that
        raises a panel of its own afterwards (D raising the Develop panel as it enters
        Develop mode) still wins. */
    restoreDockTabSelection(w.name);

    /*  The sides this workspace was captured with collapsed.  Its state blob already
        has those panels hidden; this records them as the bars' so the triangle points
        the right way and a click brings back the panels, the front tab and the width
        the side had when it was captured.  After everything above that sets dock
        visibility, or one of those would undo it. */
    applyAreaCollapse(w.areaCollapse);

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

    /*  The layout put up a Browse view; in Map the page is the map.  Covers the paths
        that apply ws directly (leaving full screen) as well as invokeWorkflowLayout. */
    if (inMapModule()) showMapPage();

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
       this workspace, not part of the layout being snapshotted, so it survives an
       update (Window > Workspace > Update Custom Workspace). */

    // Visibility
    //wsd.isMenuBarVisible = menuBarVisibleAction->isChecked();
    wsd.isStatusBarVisible = statusBarVisibleAction->isChecked();
    wsd.isFolderDockVisible = folderDockVisibleAction->isChecked();
    wsd.isFavDockVisible = favDockVisibleAction->isChecked();
    wsd.isFilterDockVisible = filterDockVisibleAction->isChecked();
    wsd.isCatalogDockVisible = catalogDockVisibleAction->isChecked();
    wsd.isKeywordsDockVisible = keywordsDockVisibleAction->isChecked();
    wsd.isCollectionsDockVisible = collectionsDockVisibleAction->isChecked();
    wsd.isQueriesDockVisible = queriesDockVisibleAction->isChecked();
    wsd.isPlacesDockVisible = placesDockVisibleAction->isChecked();
    wsd.isModuleDockVisible = moduleDockVisibleAction->isChecked();
    wsd.isMetadataDockVisible = metadataDockVisibleAction->isChecked();
    wsd.isEmbelDockVisible = embelDockVisibleAction->isChecked();
    wsd.isDevelopDockVisible = developDockVisibleAction->isChecked();
    wsd.isHistoryDockVisible = historyDockVisibleAction->isChecked();
    wsd.isPresetsDockVisible = presetsDockVisibleAction->isChecked();
    wsd.isThumbDockVisible = thumbDockVisibleAction->isChecked();
    wsd.isImageInfoVisible = infoVisibleAction->isChecked();
    // which sides the show/hide bars hold collapsed
    snapshotAreaCollapse(wsd.areaCollapse);

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
    wsd.sortColumn = sortColumn;
    wsd.isReverseSort = sortReverseAction->isChecked();
}

void MW::manageWorkspaces(int wf)
{
/*
    Rename and delete the user's custom workspaces, and choose whether each restores the
    window position and size.  The dialog is a table; nothing is applied until its
    Update button, so closing it any other way changes nothing.  A custom workspace is
    updated by invoking it and choosing Window > Workspace > Update Custom Workspace.
    The workflow layouts are managed from Window > Workspace, not here.

    wf -1 lists every custom workspace (Window > Workspace).  A workflow lists only its
    own (the Module dock context menu, MW::showWorkflowContextMenu); the names of the
    others still count when the dialog checks a rename for duplicates.
*/
    if (G::isLogger) G::log("MW::manageWorkspaces", QString::number(wf));
    QList<WorkspaceDlg::Row> rows;
    QList<int> index;               // row -> index into workspaces
    QStringList otherNames;         // not listed, but a rename must not take one
    for (int i = 0; i < workspaces->count(); i++) {
        if (wf >= 0 && workspaces->at(i).workflow != wf) {
            otherNames << workspaces->at(i).name;
            continue;
        }
        WorkspaceDlg::Row row;
        row.name = workspaces->at(i).name;
        row.isGeometryIncluded = workspaces->at(i).isGeometryIncluded;
        rows.append(row);
        index.append(i);
    }
    const QString title = wf >= 0
        ? tr("Manage %1 Custom Workspaces").arg(customWorkflowName(wf))
        : tr("Manage Custom Workspaces");
    workspaceDlg = new WorkspaceDlg(rows, otherNames, title, this);
    const bool isUpdate = workspaceDlg->exec() == QDialog::Accepted;
    if (isUpdate) rows = workspaceDlg->rows();
    delete workspaceDlg;
    workspaceDlg = nullptr;
    /* Closed without Update: nothing changes.  The dialog has checked the names. */
    if (!isUpdate || rows.count() != index.count()) return;

    for (int r = 0; r < rows.count(); ++r) {
        (*workspaces)[index.at(r)].name = rows.at(r).name;
        (*workspaces)[index.at(r)].isGeometryIncluded = rows.at(r).isGeometryIncluded;
    }
    // last first, so the indexes still to be deleted do not move
    for (int r = rows.count() - 1; r >= 0; --r)
        if (rows.at(r).isDelete) deleteWorkspace(index.at(r));
    saveWorkspaces();
    syncWorkspaceMenu();
}

void MW::deleteWorkspace(int n)
{
    if (G::isLogger)
        G::log("MW::deleteWorkspace");
    if (workspaces->count() < 1) return;

    // remove workspace from list of workspaces
    workspaces->removeAt(n);
    // the indexes after n shift down one
    if (activeCustomWs == n) activeCustomWs = -1;
    else if (activeCustomWs > n) --activeCustomWs;
    if (slideShowReturnCustomWs == n) slideShowReturnCustomWs = -1;
    else if (slideShowReturnCustomWs > n) --slideShowReturnCustomWs;

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
        {9, collectionsDock, favDock, collectionsDockVisibleAction},
        {10, queriesDock, collectionsDock, queriesDockVisibleAction},
        {11, placesDock, queriesDock, placesDockVisibleAction},
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
    // usable area: the taskbar / Dock / menu bar excluded
    QRect desktop = QGuiApplication::screens().first()->availableGeometry();
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
    /*  Off, as Keywords is: Window > Collections Panel turns it on, and a workspace saved
        with it on keeps it. */
    collectionsDockVisibleAction->setChecked(false);
    queriesDockVisibleAction->setChecked(false);
    placesDockVisibleAction->setChecked(false);
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

    /* No grid thumbnail size: this is a workflow layout, and those leave the user's
       size alone (see MW::invokeWorkspace). */
    gridView->labelFontSize = 10;
    gridView->showIconLabels = true;

    /* assignedIconWidth must track iconWidth so rejustify() honors these defaults instead
       of justifying from the previous reference width. */
    thumbView->assignedIconWidth = thumbView->iconWidth;

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
       MW::invokeWorkspace), so the built-in layout leaves it as the user set it.  The
       sort likewise (see "THE SORT IS NOT PART OF A LAYOUT" there): this used to reset
       it to column 0 for any workflow without a shipped layout. */
    ws.name = WorkspaceDlg::defaultWorkspaceName;
    updateState();
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
    rpt << "Workflow layouts:";
    for (int lay : workflowLayoutOrder()) {
        const bool isDef = lay < layoutSlots.count() && layoutSlots.at(lay).hasShipped;
        rpt << "\n  " << workflowNames().at(lay).leftJustified(16)
            << " Winnow default: " << G::s(isDef)
            << "   user layout: " << G::s(hasUserLayout(lay));
    }
    rpt << "\n  currentWorkflow = " << currentWorkflow
        << "   activeCustomWs = " << activeCustomWs;
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
            << "\n  Workflow                  " << customWorkflowName(ws.workflow)
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
            << "\n  isCollectionsDockVisible  " << G::s(ws.isCollectionsDockVisible)
            << "\n  isQueriesDockVisible      " << G::s(ws.isQueriesDockVisible)
            << "\n  isPlacesDockVisible       " << G::s(ws.isPlacesDockVisible)
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
        << "\nisCollectionsDockVisible" << ws.isCollectionsDockVisible
        << "\nisQueriesDockVisible" << ws.isQueriesDockVisible
        << "\nisPlacesDockVisible" << ws.isPlacesDockVisible
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
    // custom workspaces only: a workflow layout belongs to its slot
    if (wsd.workflow >= 0) m["workflow"] = workflowKeyFor(wsd.workflow);
    // Visibility
    m["isWindowTitleBarVisible"] = wsd.isWindowTitleBarVisible;
    m["isStatusBarVisible"] = wsd.isStatusBarVisible;
    m["isFolderDockVisible"] = wsd.isFolderDockVisible;
    m["isFavDockVisible"] = wsd.isFavDockVisible;
    m["isFilterDockVisible"] = wsd.isFilterDockVisible;
    m["isCatalogDockVisible"] = wsd.isCatalogDockVisible;
    m["isKeywordsDockVisible"] = wsd.isKeywordsDockVisible;
    m["isCollectionsDockVisible"] = wsd.isCollectionsDockVisible;
    m["isQueriesDockVisible"] = wsd.isQueriesDockVisible;
    m["isPlacesDockVisible"] = wsd.isPlacesDockVisible;
    m["isModuleDockVisible"] = wsd.isModuleDockVisible;
    m["isMetadataDockVisible"] = wsd.isMetadataDockVisible;
    m["isEmbelDockVisible"] = wsd.isEmbelDockVisible;
    m["isDevelopDockVisible"] = wsd.isDevelopDockVisible;
    m["isHistoryDockVisible"] = wsd.isHistoryDockVisible;
    m["isPresetsDockVisible"] = wsd.isPresetsDockVisible;
    m["isThumbDockVisible"] = wsd.isThumbDockVisible;
    areaCollapseToMap(wsd.areaCollapse, m);
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
    // absent from a workflow layout, and from a custom workspace saved before custom
    // workspaces had a workflow (MW::loadWorkspaces infers one)
    wsd.workflow = workflowFromKey(m.value("workflow").toString());

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
    // likewise absent before the Collections dock, and likewise off by default
    wsd.isCollectionsDockVisible = m.value("isCollectionsDockVisible").toBool();
    wsd.isQueriesDockVisible = m.value("isQueriesDockVisible").toBool();
    // absent before the Places dock: off, as the shipped Map layout turns it on
    wsd.isPlacesDockVisible = m.value("isPlacesDockVisible").toBool();
    /*  Absent from every workspace and shipped default saved before the Module dock
        existed: show it, since it is where the workflow is chosen. */
    wsd.isModuleDockVisible = m.value("isModuleDockVisible", true).toBool();
    wsd.isMetadataDockVisible = m.value("isMetadataDockVisible").toBool();
    wsd.isEmbelDockVisible = m.value("isEmbelDockVisible").toBool();
    wsd.isDevelopDockVisible = m.value("isDevelopDockVisible").toBool();
    wsd.isHistoryDockVisible = m.value("isHistoryDockVisible").toBool();
    wsd.isPresetsDockVisible = m.value("isPresetsDockVisible").toBool();
    wsd.isThumbDockVisible = m.value("isThumbDockVisible").toBool();
    // absent from a workspace saved before collapse was recorded: all sides expanded
    areaCollapseFromMap(m, wsd.areaCollapse);

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
    wsd.isWorkflowLayout = false;   // runtime only; the reuse of ws must not carry it
    wsd.thumbHeightGrid = m.value("thumbHeightGrid").toInt();
    wsd.labelFontSizeGrid = m.value("labelFontSizeGrid").toInt();
    wsd.showThumbLabelsGrid = m.value("showThumbLabelsGrid").toBool();
    wsd.labelChoice = m.value("labelChoice").toString();

    // ImageView
    wsd.isImageInfoVisible = m.value("isImageInfoVisible").toBool();

    // Processes
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
    Used by MW::loadWorkspaces and MW::loadWorkflowLayouts.
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
    Used by MW::saveWorkspaces and MW::saveWorkflowLayout.
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
    bool isInferred = false;
    for (int i = 0; i < size; ++i) {
        settings->setArrayIndex(i);
        readWorkspaceSettings(ws);
        /*  Saved before a custom workspace belonged to a workflow: the docks it shows
            are the tell. */
        if (ws.workflow < 0) {
            ws.workflow = ws.isDevelopDockVisible ? int(WfDevelop)
                        : ws.isEmbelDockVisible ? int(WfEmbellish)
                        : ws.isPlacesDockVisible ? int(WfMap)
                        : int(WfSource);
            isInferred = true;
        }
        workspaces->append(ws);
    }
    settings->endArray();
    if (isInferred) saveWorkspaces();

    loadWorkflowLayouts();
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
    workflow's key applies it: E / G / T / C (Browse), D (Develop) and K (Keywords).
    Embellish, Slide Show and Map have no key yet -- they are reached from the Module
    dock.

    BROWSE HAS TWO LAYOUTS, Library and Folders, and uses the one that matches the
    source (MW::workflowLayout); switching source inside Browse switches layout
    (MW::chooseSource).  So the slots below are indexed by LAYOUT (WorkflowLayout):
    one per Workflow, WfSource's being the Folders layout, then WsLibrary.

    EACH SLOT RESOLVES AS  USER LAYOUT ?? WINNOW DEFAULT ?? BUILT-IN  (layoutSlots):

    - THE WINNOW DEFAULT, read from the resource ":/Workspaces/defaults.json".  With
      G::isRory on, Window > Workspace > "Save as Winnow Default for <layout>" writes it
      back into the SOURCE TREE, so the layout ships with the next build rather than
      living in one machine's QSettings.  A slot with no entry in the resource falls
      back to MW::builtInDefaultWorkspace, so an empty defaults.json is a working state.

    - THE USER LAYOUT, saved by "Save Layout for <layout>" in the QSettings group
      "WorkflowWorkspaces/<key>".  It exists or it does not: "Revert <layout> to Winnow
      Default" deletes it.

    Every Window > Workspace action works on the CURRENT slot, so there is no list of
    layouts to pick from.

    A CUSTOM WORKSPACE (Ctrl+0..9) belongs to the workflow it was saved in and stands in
    for that workflow's layout (activeCustomWs).  Invoking it enters the workflow through
    the workflow's own action; Reset Layout and a Browse source switch keep it; any other
    workflow switch, the workflow's own button or key included, goes back to the slot.

    A workflow workspace deliberately does NOT own the window position and size
    (isGeometryIncluded false): switching workflow is about the panels, and pulling the
    window somewhere else on every E / G / C / D / K would be intolerable.  That is why
    a first run has to size the window itself -- see MW::centreWindowOnPrimaryScreen.
    A custom workspace may own it (Manage Custom Workspaces).
*/

const QStringList &MW::workflowKeys()
{
/*
    Stable identifiers for QSettings groups and the JSON resource.  Not renamed lightly:
    an existing profile and every committed defaults.json are keyed on them, so a rename
    strands the user's captured override unless it is migrated.

    THE BROWSE KEY HAS MOVED TWICE.  "Library" became "Source" when the Browse layout
    was the one layout for both sources; then Browse split into a Library and a Folders
    layout, and "Source" became "Folders" (the layout it had mostly been captured in),
    with "Library" the new, second one.  MW::migrateWorkflowKey moves "Source" across.
*/
    static const QStringList keys{"Folders", "Develop", "Keywords", "Embellish",
                                  "SlideShow", "Map", "Library"};
    return keys;
}

QStringList MW::workflowNames()
{
    /*  The names also key the session-only front-tab memory
        (MW::rememberDockTabSelection), so renaming one costs nothing there. */
    return {tr("Browse Folders"), tr("Develop"), tr("Keywords"), tr("Embellish"),
            tr("Slide Show"), tr("Map"), tr("Browse Library")};
}

const QList<int> &MW::workflowLayoutOrder()
{
    static const QList<int> order{WsLibrary, WfSource, WfDevelop, WfKeywords, WfEmbellish,
                                  WfSlideShow, WfMap};
    return order;
}

int MW::workflowLayout(int wf) const
{
    if (wf == WfSource && G::scope == G::Scope::Catalog) return WsLibrary;
    return wf;
}

int MW::currentLayoutSlot() const
{
    return workflowLayout(currentWorkflow);
}

QString MW::workflowKeyFor(int wf)
{
/*
    The key a custom workspace stores its workflow under.  Browse is one workflow with
    two layouts, so it is "Browse", not the "Folders" layout key.
*/
    if (wf == WfSource) return "Browse";
    if (wf < 0 || wf >= WfCount) return QString();
    return workflowKeys().at(wf);
}

int MW::workflowFromKey(const QString &key)
{
    if (key == "Browse") return WfSource;
    const int wf = workflowKeys().indexOf(key);
    return wf >= 0 && wf < WfCount ? wf : -1;
}

QString MW::customWorkflowName(int wf) const
{
    /* A custom Browse workspace serves both sources, so it is plain "Browse". */
    if (wf == WfSource) return tr("Browse");
    if (wf < 0 || wf >= WfCount) return QString();
    return workflowNames().at(wf);
}

void MW::migrateWorkflowKey(const QString &from, const QString &to)
{
/*
    Move a captured workflow override from an old key to its new one.

    A workflow's key names its QSettings group ("WorkflowWorkspaces/<key>"), so renaming
    the key would otherwise orphan the layout the user captured: loadWorkflowLayouts
    would find nothing under the new name and silently fall back to the shipped default,
    which reads as "my workspace was thrown away".

    The captured state blob is the tell that a group is really there, the same test
    MW::hasUserLayout uses.  Copy every key across and drop the old group -- a
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
        the migrated group.  Harmless -- loadWorkflowLayouts stamps the name from
        workflowNames() straight after reading -- but a stale word in the settings file
        is exactly what a rename is supposed to remove, so it is restamped here too. */
    const int wf = workflowKeys().indexOf(to);
    if (wf >= 0 && values.contains("name")) values.insert("name", workflowNames().at(wf));

    settings->beginGroup(newGroup);
    for (auto i = values.cbegin(); i != values.cend(); ++i) settings->setValue(i.key(), i.value());
    settings->endGroup();

    /* remove() on a group path drops the whole group -- see MW::saveWorkflowLayout. */
    settings->remove(oldGroup);
}

void MW::loadWorkflowDefaults()
{
/*
    Read the shipped per-workflow layouts from the resource.  A slot the resource does
    not mention keeps hasShipped false and falls back to the built-in layout, which is
    the state a freshly cloned repo is in.
*/
    if (G::isLogger) G::log("MW::loadWorkflowDefaults");

    layoutSlots.clear();
    layoutSlots.resize(WsCount);

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
    for (int lay = 0; lay < WsCount; ++lay) {
        const QString key = workflowKeys().at(lay);
        if (!root.contains(key) || !root.value(key).isObject()) continue;
        LayoutSlot &slot = layoutSlots[lay];
        workspaceFromJson(root.value(key).toObject(), slot.shipped);
        slot.shipped.name = workflowNames().at(lay);
        /* A workflow workspace never moves the window, whatever was captured. */
        slot.shipped.isGeometryIncluded = false;
        slot.hasShipped = true;
    }
}

void MW::loadWorkflowLayouts()
{
/*
    Read the user's per-workflow layouts.  Called from MW::loadWorkspaces, ie after
    MW::loadWorkflowDefaults has sized the slots.

    A layout used to have an on/off tick ("isOverride") separate from the captured copy.
    A copy that was ticked off is not in use, so its group is dropped and the slot falls
    back to the Winnow default, as it already did; a ticked one simply loses the key.
    Both are one-shots: the key is gone afterwards.
*/
    if (G::isLogger) G::log("MW::loadWorkflowLayouts");
    if (!isSettings) return;
    if (layoutSlots.count() < WsCount) return;

    /*  Before anything is read: a layout saved under a key that has since been renamed
        still belongs to the user.  See MW::workflowKeys. */
    migrateWorkflowKey("Source", "Folders");

    for (int lay = 0; lay < WsCount; ++lay) {
        const QString group = "WorkflowWorkspaces/" + workflowKeys().at(lay);
        if (settings->value(group + "/isOverride", true).toBool() == false) {
            settings->remove(group);
            continue;
        }
        settings->remove(group + "/isOverride");
        settings->beginGroup(group);
        if (settings->contains("state")) {
            WorkspaceData &user = layoutSlots[lay].user;
            readWorkspaceSettings(user);
            user.name = workflowNames().at(lay);
            user.isGeometryIncluded = false;
        }
        settings->endGroup();
    }
}

void MW::saveWorkflowLayout(int lay)
{
/*
    Write a slot's user layout to QSettings, or drop its group when it has none.
*/
    if (G::isLogger) G::log("MW::saveWorkflowLayout");
    if (lay < 0 || lay >= layoutSlots.count()) return;

    const QString group = "WorkflowWorkspaces/" + workflowKeys().at(lay);
    settings->remove(group);
    if (!hasUserLayout(lay)) return;
    settings->beginGroup(group);
    writeWorkspaceSettings(layoutSlots.at(lay).user);
    settings->endGroup();
}

bool MW::hasUserLayout(int lay) const
{
/*
    Has the user saved a layout for this slot?  The captured state blob is the tell --
    a workspace that has one has been snapshotted.
*/
    if (lay < 0 || lay >= layoutSlots.count()) return false;
    return !layoutSlots.at(lay).user.state.isEmpty();
}

void MW::invokeWorkflowWorkspace(int wf)
{
/*
    Apply the layout for a workflow.  Browse's layout is the one for the current source
    (MW::workflowLayout).  See MW::invokeWorkflowLayout.
*/
    if (G::isLogger) G::log("MW::invokeWorkflowWorkspace");
    if (wf < 0 || wf >= WfCount) return;
    if (G::isPanelProbe)
        PanelProbe::Instance().Mark(QString("invokeWorkflowWorkspace(%1) enter").arg(wf));

    invokeWorkflowLayout(wf, workflowLayout(wf));
}

void MW::invokeWorkflowLayout(int wf, int lay)
{
/*
    Apply layout `lay` as workflow `wf`'s.  Only Browse has a choice -- MW::chooseSource
    applies the layout for the source it is ABOUT to switch to, before G::scope says so.

    THE ONE RESOLVER.  The custom workspace standing in for wf when keepCustomWs says
    to keep it (see MW::enterWorkflowLayout); otherwise the slot's user layout -- unless
    the Winnow default was chosen for wf (winnowDefaultWf), kept the same way -- else
    its Winnow default, else the built-in layout.
*/
    if (wf < 0 || wf >= WfCount || lay < 0 || lay >= layoutSlots.count()) return;
    const bool isCustom = keepCustomWs && activeCustomWs >= 0
                          && activeCustomWs < workspaces->count()
                          && workspaces->at(activeCustomWs).workflow == wf;
    const bool isWinnowDefault = !isCustom && keepCustomWs && winnowDefaultWf == wf;
    if (!isWinnowDefault) winnowDefaultWf = -1;
    /*  Stamped BEFORE the layout is applied, so everything the apply runs sees the
        workflow being entered, not the one being left (in Map, the views route
        elsewhere -- see MW::inMapModule). */
    currentWorkflow = wf;
    if (isCustom) {
        invokeWorkspace(workspaces->at(activeCustomWs));
    }
    else {
        activeCustomWs = -1;
        const LayoutSlot &slot = layoutSlots.at(lay);
        const bool useUser = hasUserLayout(lay) && !isWinnowDefault;
        if (useUser || slot.hasShipped) {
            WorkspaceData layout = useUser ? slot.user : slot.shipped;
            layout.isWorkflowLayout = true;         // no grid size: see invokeWorkspace
            invokeWorkspace(layout);
        }
        else builtInDefaultWorkspace();
    }
    if (wf == WfSource) browseLayoutApplied = lay;
    syncWorkflowSwitcher();
    if (wf == WfKeywords) focusKeywordsFilters();
    /* A workspace carries a Browse view (Loupe, Grid ...) and invokeWorkspace just put
       it up; the Map workflow's page is the map instead. Here rather than in
       invokeMapWorkflow so Reset Layout and a resumed session get the map too. */
    if (wf == WfMap) showMapPage();
}

void MW::focusKeywordsFilters()
{
    if (G::isLogger) G::log("MW::focusKeywordsFilters");
    QTimer::singleShot(0, this, [this] {
        if (filters && currentWorkflow == WfKeywords) filters->focusKeywordsCategory();
    });
}

void MW::saveCurrentLayout()
{
/*
    Window > Workspace > Save Layout.  What is saved depends on what is on screen:

    - A custom workspace in use: update it (its name, workflow and window position
      choice are kept).
    - Otherwise, with G::isRory on: the WINNOW DEFAULT for the current slot.  The whole
      resource is written back to the source tree, so the next build carries it, and
      any user layout for the slot is deleted, so what Rory sees is what ships.
    - Otherwise: the user layout for the current slot.
*/
    if (G::isLogger) G::log("MW::saveCurrentLayout");

    if (activeCustomWs >= 0 && activeCustomWs < workspaces->count()) {
        populateWorkspace(activeCustomWs, workspaces->at(activeCustomWs).name);
        saveWorkspaces();
        return;
    }

    const int lay = currentLayoutSlot();
    if (lay < 0 || lay >= layoutSlots.count()) return;
    LayoutSlot &slot = layoutSlots[lay];

    winnowDefaultWf = -1;       // what is saved is what is used from now on
    if (!G::isRory) {
        snapshotWorkspace(slot.user);
        slot.user.name = workflowNames().at(lay);
        slot.user.isGeometryIncluded = false;
        saveWorkflowLayout(lay);
        syncWorkflowWorkspaceMenus();
        return;
    }

    snapshotWorkspace(slot.shipped);
    slot.shipped.name = workflowNames().at(lay);
    slot.shipped.isGeometryIncluded = false;
    slot.hasShipped = true;
    slot.user = WorkspaceData();
    saveWorkflowLayout(lay);

    QString path;
    QString err;
    if (writeWorkflowDefaultsJson(path, err)) {
        QMessageBox::information(this, tr("Winnow Default Saved"),
            tr("The %1 layout was written to\n\n%2\n\nRebuild to ship it.")
                .arg(workflowNames().at(lay), path));
    }
    else {
        QMessageBox::warning(this, tr("Winnow Default Not Saved"),
            tr("Could not write the workflow defaults:\n\n%1").arg(err));
    }
    syncWorkflowWorkspaceMenus();
}

void MW::revertCurrentLayout()
{
/*
    Window > Workspace > Revert <layout> to Winnow Default.  Delete the user layout for
    the current slot and apply the Winnow default.  A custom workspace in use is left
    for the slot's layout, like any workflow switch.

    The front tab of each tab group is reset too: MW::restoreDockTabSelection would
    otherwise raise the panel last used in this layout instead of the captured one.
    Because the reset is LEAVING the same workspace it applies, isFrontDockTabsReset
    makes invokeWorkspace drop the entry again after it remembers it.
*/
    if (G::isLogger) G::log("MW::revertCurrentLayout");
    const int lay = currentLayoutSlot();
    if (lay < 0 || lay >= layoutSlots.count()) return;

    layoutSlots[lay].user = WorkspaceData();
    saveWorkflowLayout(lay);
    frontDockTabsByWorkspace.remove(workflowNames().at(lay));
    isFrontDockTabsReset = true;
    invokeWorkflowLayout(currentWorkflow, lay);
    isFrontDockTabsReset = false;
    if (currentWorkflow == WfSource) requestView(browseView);
    syncWorkflowWorkspaceMenus();
}

void MW::revertAllLayouts()
{
/*
    Window > Workspace > Revert All Layouts to Winnow Defaults: MW::revertCurrentLayout
    for every slot.  Custom workspaces are not touched, and keep their front tabs.
*/
    if (G::isLogger) G::log("MW::revertAllLayouts");

    for (int lay = 0; lay < layoutSlots.count(); ++lay) {
        if (!hasUserLayout(lay)) continue;
        layoutSlots[lay].user = WorkspaceData();
        saveWorkflowLayout(lay);
    }
    for (const QString &name : workflowNames()) frontDockTabsByWorkspace.remove(name);
    isFrontDockTabsReset = true;
    invokeWorkflowWorkspace(currentWorkflow);
    isFrontDockTabsReset = false;
    if (currentWorkflow == WfSource) requestView(browseView);
    syncWorkflowWorkspaceMenus();
}

bool MW::writeWorkflowDefaultsJson(QString &path, QString &err) const
{
/*
    Write every Winnow default to Workspaces/defaults.json in the source tree
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
    for (int lay = 0; lay < layoutSlots.count(); ++lay) {
        if (!layoutSlots.at(lay).hasShipped) continue;
        root.insert(workflowKeys().at(lay), workspaceToJson(layoutSlots.at(lay).shipped));
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
    The three layout actions name what they will act on: the current slot, or the
    custom workspace in use.  Run when the Workspace menu is about to show, and by
    MW::rory, since Save's target follows G::isRory.  Revert is greyed, with the reason,
    when there is no user layout to revert.
*/
    if (G::isLogger) G::log("MW::syncWorkflowWorkspaceMenus");
    if (saveLayoutAction == nullptr) return;

    const int lay = currentLayoutSlot();
    if (lay < 0 || lay >= layoutSlots.count()) return;
    const QString name = workflowNames().at(lay);
    const bool isCustom = activeCustomWs >= 0 && activeCustomWs < workspaces->count();

    if (isCustom) {
        const QString wsName = workspaces->at(activeCustomWs).name;
        saveLayoutAction->setText(tr("Update Custom Workspace \"%1\"").arg(wsName));
        saveLayoutAction->setToolTip(
            tr("Save the current layout as the custom workspace \"%1\"").arg(wsName));
    }
    else if (G::isRory) {
        saveLayoutAction->setText(tr("Save as Winnow Default for %1").arg(name));
        saveLayoutAction->setToolTip(
            tr("Write the current layout to Workspaces/defaults.json, the %1 layout "
               "Winnow ships with, and delete your own %1 layout").arg(name));
    }
    else {
        saveLayoutAction->setText(tr("Save Layout for %1").arg(name));
        saveLayoutAction->setToolTip(
            tr("Use the current layout whenever %1 is opened").arg(name));
    }

    /*  A custom workspace on screen is not the Winnow default either: Revert leaves it
        (MW::revertCurrentLayout applies the slot, which clears activeCustomWs). */
    /*  Reset Layout re-applies whatever is in use (MW::resetLayout); say which. */
    const QString inUse = isCustom
        ? tr("the custom workspace \"%1\"").arg(workspaces->at(activeCustomWs).name)
        : winnowDefaultWf == currentWorkflow || !hasUserLayout(lay)
              ? tr("the %1 layout Winnow ships with").arg(name)
              : tr("your %1 layout").arg(name);
    resetLayoutAction->setToolTip(
        tr("Put the panels back the way %1 has them: a panel left open, moved, or "
           "stranded on a monitor that is gone.  Nothing is saved or deleted.")
            .arg(inUse));

    const bool hasUser = hasUserLayout(lay);
    const bool canRevert = hasUser || isCustom;
    const QString notReason =
        tr("%1 is already using the layout Winnow ships with").arg(name);
    revertLayoutAction->setText(tr("Revert %1 to Winnow Default").arg(name));
    revertLayoutAction->setEnabled(canRevert);
    if (isCustom && !hasUser)
        revertLayoutAction->setToolTip(
            tr("Leave the custom workspace \"%1\" and use the %2 layout Winnow ships with")
                .arg(workspaces->at(activeCustomWs).name, name));
    else if (hasUser)
        revertLayoutAction->setToolTip(
            tr("Delete your %1 layout and use the one Winnow ships with").arg(name));
    else
        revertLayoutAction->setToolTip(notReason);
    revertLayoutAction->setProperty("disabledReason", canRevert ? QString() : notReason);

    bool hasAnyUser = false;
    for (int i = 0; i < layoutSlots.count(); ++i) hasAnyUser |= hasUserLayout(i);
    revertAllLayoutsAction->setEnabled(hasAnyUser || isCustom);
    revertAllLayoutsAction->setToolTip(hasAnyUser || isCustom
        ? tr("Delete every layout you have saved and use the ones Winnow ships with")
        : tr("Every workflow is already using the layout Winnow ships with"));
}

/*  THE UI MODEL: SOURCE x WORKFLOW x VIEW *****************************************

    Three settings, each with one home and its own keys, none changing another:

    SOURCE    Library | Folders.  The left end of the Module dock, Ctrl+Shift+L /
              Ctrl+Shift+F, View > Library / Folders.  Every workflow works on the
              current source and selection, so Library never changes the workflow;
              Folders goes to Browse.  In Browse it switches the layout, since Browse
              has one per source (MW::chooseSource).
    WORKFLOW  Browse, Develop, Keywords, Embellish, Slide Show, Map.  The Module dock's
              centred modes, D and K.  Owns the workspace (panel layout).
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
    panels as a side effect, and it is now MW::resetLayout's job.  A custom Browse
    workspace in use is left alone the same way.
*/
    if (G::isLogger) G::log("MW::requestView", QString::number(view));
    const bool inDevelop = G::operationMode == G::OperationMode::Develop;
    const bool otherWorkflow = currentWorkflow != WfSource;
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
    shown in.  Unlike a view key this applies the Browse layout from a custom workspace
    too -- asking for Browse by name is asking for its layout -- and it re-applies it
    when the source has changed since (a folder click out of the Library), so the
    layout matches the source again.

    Entering a custom Browse workspace (keepCustomWs) always applies it, and keeps the
    view it was saved with.
*/
    if (G::isLogger) G::log("MW::invokeBrowseWorkflow");
    setOperationMode(G::OperationMode::Preview);
    if (keepCustomWs) {
        invokeWorkflowWorkspace(WfSource);
        return;
    }
    if (currentWorkflow != WfSource || activeCustomWs >= 0 || winnowDefaultWf >= 0
        || browseLayoutApplied != workflowLayout(WfSource))
        invokeWorkflowWorkspace(WfSource);
    requestView(browseView);
}

void MW::chooseSource(bool library, const QString &src)
{
/*
    Library | Folders at the left of the Module dock, and View > Library / Folders
    (Ctrl+Shift+L / Ctrl+Shift+F).  The scope change is MW::setCatalogScopeWhole /
    MW::showFoldersSource, exactly as the Source panel's toggle ran it.

    IN BROWSE THE LAYOUT FOLLOWS: Browse has a Library layout and a Folders layout.  It
    is applied BEFORE the scope changes, so no panel moves under a load that has
    started; a refused change (unsaved picks, an empty catalog opening Manage Catalog)
    puts the old layout back.  Any other workflow keeps its layout.

    FOLDERS ALSO MEANS BROWSE: a running slide show is stopped, then from any other
    workflow (or Develop mode) Folders leaves it for Browse with the Folders layout, applied the same way before the scope
    changes.  A refused change stays in Browse, with the layout for the source that is
    still loaded.  Library keeps the workflow.

    Other routes that change the scope -- a folder or bookmark clicked out of the
    Library, File > Open Library -- leave the layout alone; the Browse button re-syncs
    it (MW::invokeBrowseWorkflow).
*/
    if (G::isLogger) G::log("MW::chooseSource", src);
    /*  A running show stops first.  Its return workflow is dropped so stopping applies
        no layout of its own: the Browse layout below is the only switch. */
    if (!library && G::isSlideShow) {
        slideShowReturnWorkflow = kNoSlideShowReturn;
        slideShow();
    }
    const G::Scope target = library ? G::Scope::Catalog : G::Scope::Folders;
    const int before = browseLayoutApplied;
    const int lay = library ? int(WsLibrary) : int(WfSource);
    const bool inBrowse = currentWorkflow == WfSource
                          && G::operationMode != G::OperationMode::Develop;
    const bool toBrowse = !library && !inBrowse;
    /*  A custom Browse workspace serves both sources, so it stays put. */
    const bool isCustomBrowse = inBrowse && activeCustomWs >= 0;
    const bool follow = toBrowse
                        || (inBrowse && !isCustomBrowse && G::scope != target
                            && before != lay);
    /*  COVERED FROM THE CLICK. The Browse layout switch below takes about a second
        (timeline probe: 1.1 s), and until setCatalogScopeWhole puts up its message the
        folder being left sat on screen while the panels moved around it. Raised here;
        lifted below if the Library is not entered after all (picks kept, empty catalog),
        otherwise it stays until the Library is ready (see loadCurtainUp). */
    const bool coverForLibrary = library && G::scope != G::Scope::Catalog
                                 && !G::isInitializing;
    if (coverForLibrary) {
        setCentralMessage(tr("Loading the library."));
        raiseLoadCurtain();
    }

    /*  keepCustomWs: a Winnow default chosen for Browse stays chosen for the other
        source.  From another workflow nothing matches WfSource, so it is cleared. */
    if (follow) {
        if (toBrowse) setOperationMode(G::OperationMode::Preview);
        keepCustomWs = true;
        invokeWorkflowLayout(WfSource, lay);
        keepCustomWs = false;
        requestView(browseView);
    }

    if (library) setCatalogScopeWhole(src);
    else showFoldersSource();
    if (coverForLibrary && G::scope != G::Scope::Catalog)
        lowerLoadCurtain("MW::chooseSource Library refused");

    if (follow && G::scope != target) {
        keepCustomWs = true;
        invokeWorkflowLayout(WfSource, toBrowse ? workflowLayout(WfSource) : before);
        keepCustomWs = false;
        requestView(browseView);
    }
    /*  setScope pushes the buttons back, but an empty catalog returns before it is
        called; say what is actually loaded. */
    syncSourceButtons();
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

    The Slide Show layout resolves like any other (MW::invokeWorkflowLayout), so a
    saved Slide Show layout is honoured.  A custom workspace in use is recorded so the
    show returns to it.

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
    slideShowReturnCustomWs = activeCustomWs;   // both cleared by the switch below
    slideShowReturnWinnowDefault = winnowDefaultWf == from && from >= 0;
    setOperationMode(G::OperationMode::Preview);
    invokeWorkflowWorkspace(WfSlideShow);
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
    const int customWs = slideShowReturnCustomWs;
    const bool isWinnowDefault = slideShowReturnWinnowDefault;
    slideShowReturnWorkflow = kNoSlideShowReturn;
    slideShowReturnCustomWs = -1;
    slideShowReturnWinnowDefault = false;
    if (wf == kNoSlideShowReturn || currentWorkflow != WfSlideShow) return;
    if (G::isLogger) G::log("MW::leaveSlideShowWorkflow", QString::number(wf));

    // back to the custom workspace, or the chosen Winnow default, the show left
    if (customWs >= 0 && customWs < workspaces->count()
        && workspaces->at(customWs).workflow == wf) {
        invokeCustomWorkspace(customWs);
        return;
    }
    if (isWinnowDefault) {
        enterWorkflowLayout(wf, -1, true);
        return;
    }

    QAction *a = nullptr;
    switch (wf) {
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
    of Map changes currentWorkflow (invokeWorkflowLayout).
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
    layout, else the Winnow default), or the custom workspace in use.  The recovery for
    a panel left open or a dock stranded on a monitor that is gone.

    Neither the operation mode nor the view changes: a workflow layout carries a central
    view of its own, so Browse's view is put back afterwards.
*/
    if (G::isLogger) G::log("MW::resetLayout");
    const int wf = currentWorkflow;
    keepCustomWs = true;
    invokeWorkflowWorkspace(wf);
    keepCustomWs = false;
    if (wf == WfSource) requestView(browseView);
}

void MW::syncSourceButtons()
{
/*
    Library | Folders, in the Module dock and the title bar (macOS), say what G::scope
    holds. Called by MW::setScope and MW::chooseSource, so a refused click shows the
    truth again. Signals blocked: the buttons act on clicked, and this is not one.
*/
    const bool lib = G::scope == G::Scope::Catalog;
    const QList<QPair<QToolButton *, QToolButton *>> pairs{
        {sourceLibraryBtn, sourceFoldersBtn},               // Module dock
        {titleSourceLibraryBtn, titleSourceFoldersBtn},     // title bar (macOS)
    };
    for (const auto &p : pairs) {
        if (!p.first || !p.second) continue;
        QSignalBlocker a(p.first), b(p.second);
        p.first->setChecked(lib);
        p.second->setChecked(!lib);
    }
}

void MW::syncWorkflowSwitcher()
{
/*
    Light the button for currentWorkflow in the Module dock, the title bar (macOS) and
    the (hidden) status-bar switcher.  Set programmatically, so a click the workflow
    refused shows the truth again.
*/
    for (const QList<QToolButton *> *btns : {&moduleBtns, &workflowBtns, &titleModeBtns}) {
        for (int wf = 0; wf < btns->count(); ++wf) {
            QToolButton *btn = btns->at(wf);
            if (!btn) continue;
            QSignalBlocker block(btn);
            btn->setChecked(wf == currentWorkflow);
        }
    }
}

QAction *MW::workflowAction(int wf) const
{
    /* The action a workflow button triggers and takes its enabled state from. Null
       until createActions has run. */
    switch (wf) {
    case WfSource:    return browseWorkflowAction;
    case WfDevelop:   return operationModeAction;
    case WfKeywords:  return keywordsWorkspaceAction;
    case WfEmbellish: return embellishWorkspaceAction;
    case WfSlideShow: return slideShowWorkspaceAction;
    case WfMap:       return mapWorkspaceAction;
    default:          return nullptr;
    }
}

void MW::syncWorkflowButtonEnabled(int wf)
{
/*
    A workflow button, in the Module dock and the hidden status-bar switcher, is enabled
    only when its action is, the DataModel has rows (a folder or catalog is loaded -- so
    every button starts greyed when Winnow opens), and no catalog fill is running
    (setCatalogLoading). A greyed button says why in its tooltip.
*/
    QAction *a = workflowAction(wf);
    if (!a) return;
    const QString key = wf == WfSource ? tr("E / G / T / C")
                      : wf == WfDevelop ? tr("D")
                      : wf == WfKeywords ? tr("K") : QString();
    QString tip = a->text();
    if (!key.isEmpty()) tip += "  (" + key + ")";
    // discoverable: MW::showWorkflowContextMenu
    tip += "\n" + tr("Right-click for its layouts and custom workspaces.");
    const bool hasRows = dm && dm->rowCount() > 0;
    if (isCatalogLoading)
        tip += "\n" + tr("Not available while the catalog is loading.");
    else if (!hasRows)
        tip += "\n" + tr("Not available until a folder or catalog is loaded.");
    else if (!a->isEnabled()) {
        const QString why = a->property("disabledReason").toString();
        if (!why.isEmpty()) tip += "\n" + why;
    }
    for (QToolButton *btn : {workflowBtns.value(wf), moduleBtns.value(wf),
                             titleModeBtns.value(wf)}) {
        if (!btn) continue;
        btn->setEnabled(a->isEnabled() && hasRows && !isCatalogLoading);
        btn->setToolTip(tip);
    }
}

void MW::setCatalogLoading(bool loading)
{
/*
    Grey every workflow button for the length of a catalog fill. Raised by
    loadCatalogScope; lowered by folderChanged (the fill is in, or was aborted) and by
    stop(), so an Esc or a new load never leaves the buttons dead. Always re-syncs,
    even when the flag is unchanged: folderChanged relies on it to enable the buttons
    once a folder's rows are in.
*/
    isCatalogLoading = loading;
    syncWorkflowButtonsEnabled();
}

void MW::syncWorkflowButtonsEnabled()
{
    /* Every workflow button: after the model gains rows or is cleared. */
    for (int wf = 0; wf < WfCount; ++wf) syncWorkflowButtonEnabled(wf);
}

void MW::styleWorkflowSwitcher()
{
/*
    MW::segmentedOptionCss's look for both workflow rows: the Module
    dock at 1.5x G::fontSize, the hidden status-bar switcher at the normal size.  From
    the palette and the font size, so MW::setBackgroundShade and MW::setFontSize call
    this again.

    The Module dock's height is FIXED to its text afterwards: the top dock area has a
    splitter against the central widget, and a strip of buttons has no use for being
    dragged taller.
*/
    /* THE MODULE COLOURS, for the modes and for Library | Folders beside them (all in
       the dock's content, so one sheet styles both). Appended after the shared CSS so
       they win; the selected (yellow) and disabled colours still come from
       segmentedOptionCss. */
    int l = G::textShade - 40;
    int m = G::backgroundShade + 20;
    const QColor unselectedColor = QColor(l,l,l);  //G::textColor;    // an unselected module name
    const QColor separatorColor  = QColor(m,m,m);    // the " | " between module names
    const QString moduleCss =
        "QToolButton:!checked:enabled { color:" + unselectedColor.name() + "; }"
        "QToolButton:!checked:enabled:hover { color:white; }"   // hover highlight
        "QLabel { color:" + separatorColor.name() + "; }";

    /* Not bold, in either row: at the Module dock's size the yellow alone is enough. */
    const QString css = segmentedOptionCss(0, /*boldSelected*/ false) + moduleCss;
    for (QToolButton *btn : std::as_const(workflowBtns))
        if (btn) btn->setStyleSheet(css);

    /* The title bar copy (macOS): the Module dock's size and colours, with no vertical
       padding. The strip grows the bar if the text is taller (TitleBarStrip::place). */
    if (titleModeBar) {
        titleModeBar->setStyleSheet(
            segmentedOptionCss(qRound(1.5 * G::fontSize), false, 0) + moduleCss);
        titleModeBar->relayout();
    }

    if (!moduleDock || !moduleDock->content()) return;
    moduleDock->content()->setStyleSheet(
        segmentedOptionCss(qRound(1.5 * G::fontSize), false) + moduleCss);
    moduleDock->content()->adjustSize();
    /* widget() is the content's FrameLineBox; its hint includes any frameLine inset. */
    moduleDock->widget()->adjustSize();
    moduleDock->setFixedHeight(moduleDock->widget()->sizeHint().height());

    /* THE WINDOW'S MINIMUM WIDTH: the Module dock's (Library | Folders, the modes and
       Masters | All at their least) plus 125 px of slack, so the three groups never
       crowd. Here, so a font size change moves it too. */
    setMinimumWidth(moduleDock->widget()->minimumSizeHint().width() + 125);
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
