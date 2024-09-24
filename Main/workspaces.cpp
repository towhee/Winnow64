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
    the window in the same screen and this taks some time.  MW::eventFilter overrides the
    QEvent::WindowStateChange and calls invokeWorkspace after a delay to allow the showNormal
    function to complete drawing the normal window.
*/
    if (G::isLogger) G::log("MW::invokeWorkspace");

    ws = w;     // current workspace ws

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
    metadataDockVisibleAction->setChecked(w.isMetadataDockVisible);
    embelDockVisibleAction->setChecked(w.isEmbelDockVisible);
    thumbDockVisibleAction->setChecked(w.isThumbDockVisible);
    infoVisibleAction->setChecked(w.isImageInfoVisible);
    // View
    asLoupeAction->setChecked(w.isLoupeDisplay);
    asGridAction->setChecked(w.isGridDisplay);
    asTableAction->setChecked(w.isTableDisplay);
    asCompareAction->setChecked(w.isCompareDisplay);
    // Thumbview
    thumbView->iconWidth = w.thumbWidth;
    thumbView->iconHeight = w.thumbHeight;
    thumbView->labelFontSize = w.labelFontSize;
    thumbView->showIconLabels = w.showThumbLabels;
    thumbView->rejustify();
    thumbView->setThumbParameters();
     // GridView
    gridView->iconWidth = w.thumbWidthGrid;
    gridView->iconHeight = w.thumbHeightGrid;
    gridView->labelFontSize = w.labelFontSizeGrid;
    gridView->showIconLabels = w.showThumbLabelsGrid;
    gridView->labelChoice = w.labelChoice;
    gridView->rejustify();
    gridView->setThumbParameters();
    // ImageView
    infoVisibleAction->setChecked(w.isImageInfoVisible);
    // Processes
    if (w.isColorManage != G::colorManage) {
        if (w.isColorManage) toggleColorManage(Tog::on);
        else toggleColorManage(Tog::off);
    }
    cacheSizeStrategy = w.cacheSizeMethod;
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

    restoreGeometry(w.geometry);
    restoreState(w.state);
    // second restoreState req'd for going from docked to floating docks
    restoreGeometry(w.geometry);
    restoreState(w.state);

    if (w.isMaximised) showMaximized();
}

void MW::snapshotWorkspace(WorkspaceData &wsd)
{
    QString fun = "MW::snapshotWorkspace";
    if (G::isLogger) G::log(fun);
    // qDebug() << "MW::snapshotWorkspace  geometry()" << geometry();

    // State
    wsd.geometry = saveGeometry();
    wsd.state = saveState();
    wsd.screen = screen();
    wsd.geometryRect = geometry();
    wsd.isFullScreen = isFullScreen();
    wsd.isMaximised = isMaximized();

    // Visibility
    //wsd.isMenuBarVisible = menuBarVisibleAction->isChecked();
    wsd.isStatusBarVisible = statusBarVisibleAction->isChecked();
    wsd.isFolderDockVisible = folderDockVisibleAction->isChecked();
    wsd.isFavDockVisible = favDockVisibleAction->isChecked();
    wsd.isFilterDockVisible = filterDockVisibleAction->isChecked();
    wsd.isMetadataDockVisible = metadataDockVisibleAction->isChecked();
    wsd.isEmbelDockVisible = embelDockVisibleAction->isChecked();
    wsd.isThumbDockVisible = thumbDockVisibleAction->isChecked();
    wsd.isImageInfoVisible = infoVisibleAction->isChecked();

    // View
    wsd.isLoupeDisplay = asLoupeAction->isChecked();
    wsd.isGridDisplay = asGridAction->isChecked();
    wsd.isTableDisplay = asTableAction->isChecked();
    wsd.isCompareDisplay = asCompareAction->isChecked();

    // Thumbview
    wsd.thumbWidth = thumbView->iconWidth;
    wsd.thumbHeight = thumbView->iconHeight;
    wsd.labelFontSize = thumbView->labelFontSize;
    wsd.showThumbLabels = thumbView->showIconLabels;

    // GridView
    wsd.thumbWidthGrid = gridView->iconWidth;
    wsd.thumbHeightGrid = gridView->iconHeight;
    wsd.labelFontSizeGrid = gridView->labelFontSize;
    wsd.showThumbLabelsGrid = gridView->showIconLabels;
    wsd.labelChoice = gridView->labelChoice;

    // ImageView
    wsd.isImageInfoVisible = infoVisibleAction->isChecked();

    // Processes
    wsd.isColorManage = G::colorManage;
    wsd.cacheSizeMethod = cacheSizeStrategy;
    wsd.sortColumn = sortColumn;
    wsd.isReverseSort = sortReverseAction->isChecked();
}

void MW::manageWorkspaces()
{
/*
    Delete, rename and reassign workspaces.
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
*/
    if (G::isLogger) G::log("MW::defaultWorkspace");
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
    sortChange("MW::defaultWorkspace");
    ws.name = "Default";
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
            << "\n  isMetadataDockVisible     " << G::s(ws.isMetadataDockVisible)
            << "\n  isEmbelDockVisible        " << G::s(ws.isEmbelDockVisible)
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
            << "\n  cacheSizeMethod           " << G::s(ws.cacheSizeMethod)
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
        << "\nisMetadataDockVisible" << ws.isMetadataDockVisible
        << "\nisEmbelDockVisible" << ws.isEmbelDockVisible
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
        << "\ncacheSizeMethod" << ws.cacheSizeMethod
        << "\nsortColumn" << ws.sortColumn
        << "\nisReverseSort" << ws.isReverseSort
        //*/
        ;
}

void MW::loadWorkspaces()
{
    if (G::isLogger) G::log("MW::loadWorkspaces");
    if (!isSettings) return;

    // replace with the current list of workspaces
    int size = settings->beginReadArray("Workspaces");
    //qDebug() << "MW::loadWorkspaces" << size;
    for (int i = 0; i < size; ++i) {
        // Workspace
        settings->setArrayIndex(i);
        ws.name = settings->value("name").toString();

        // State
        ws.geometry = settings->value("geometry").toByteArray();
        ws.state = settings->value("state").toByteArray();
        RecoverGeometry r;
        recoverGeometry(ws.geometry, r);
        ws.screenNumber = r.screenNumber;
        ws.geometryRect = settings->value("geometryRect").toRect();
        ws.isFullScreen = settings->value("isFullScreen").toBool();
        ws.isMaximised = settings->value("isMaximised").toBool();

        // Visibility
        ws.isWindowTitleBarVisible = settings->value("isWindowTitleBarVisible").toBool();
        //ws.isMenuBarVisible = settings->value("isMenuBarVisible").toBool();
        ws.isStatusBarVisible = settings->value("isStatusBarVisible").toBool();
        ws.isFolderDockVisible = settings->value("isFolderDockVisible").toBool();
        ws.isFavDockVisible = settings->value("isFavDockVisible").toBool();
        ws.isFilterDockVisible = settings->value("isFilterDockVisible").toBool();
        ws.isMetadataDockVisible = settings->value("isMetadataDockVisible").toBool();
        ws.isEmbelDockVisible = settings->value("isEmbelDockVisible").toBool();
        ws.isThumbDockVisible = settings->value("isThumbDockVisible").toBool();

        // View
        ws.isLoupeDisplay = settings->value("isLoupeDisplay").toBool();
        ws.isGridDisplay = settings->value("isGridDisplay").toBool();
        ws.isTableDisplay = settings->value("isTableDisplay").toBool();
        ws.isCompareDisplay = settings->value("isCompareDisplay").toBool();

        // ThumbView
        ws.thumbSpacing = settings->value("thumbSpacing").toInt();
        ws.thumbPadding = settings->value("thumbPadding").toInt();
        ws.thumbWidth = settings->value("thumbWidth").toInt();
        ws.thumbHeight = settings->value("thumbHeight").toInt();
        ws.labelFontSize = settings->value("labelFontSize").toInt();
        ws.showThumbLabels = settings->value("showThumbLabels").toBool();

        // GridView
        ws.thumbSpacingGrid = settings->value("thumbSpacingGrid").toInt();
        ws.thumbPaddingGrid = settings->value("thumbPaddingGrid").toInt();
        ws.thumbWidthGrid = settings->value("thumbWidthGrid").toInt();
        ws.thumbHeightGrid = settings->value("thumbHeightGrid").toInt();
        ws.labelFontSizeGrid = settings->value("labelFontSizeGrid").toInt();
        ws.showThumbLabelsGrid = settings->value("showThumbLabelsGrid").toBool();
        ws.labelChoice = settings->value("labelChoice").toString();

        // ImageView
        ws.isImageInfoVisible = settings->value("isImageInfoVisible").toBool();
        // ws.isEmbelDisplay = settings->value("isEmbelDisplay").toBool();

        // Processes
        ws.isColorManage = settings->value("isColorManage").toBool();
        ws.cacheSizeMethod = settings->value("cacheSizeMethod").toString();
        ws.sortColumn = settings->value("sortColumn").toInt();
        ws.isReverseSort = settings->value("isReverseSort").toBool();
        workspaces->append(ws);
    }
    settings->endArray();
}

void MW::saveWorkspaces()
{
    if (G::isLogger) G::log("MW::saveWorkspaces");

    // first remove the existing array of workspaces
    settings->remove("Workspaces");

    int size = workspaces->count();
    settings->beginWriteArray("Workspaces", size);
    qDebug() << "MW::saveWorkspaces" << size;
    for (int i = 0; i < size; ++i) {
        // Workspace
        ws = workspaces->at(i);
        settings->setArrayIndex(i);
        settings->setValue("name", ws.name);

        // State
        settings->setValue("geometry", ws.geometry);
        settings->setValue("state", ws.state);
        settings->setValue("screenNumber", QGuiApplication::screens().indexOf(screen()));
        settings->setValue("geometryRect", ws.geometryRect);                        // need?
        settings->setValue("isFullScreen", ws.isFullScreen);                        // need?
        settings->setValue("isMaximised", ws.isMaximised);                          // need?

        // Visibility
        settings->setValue("isWindowTitleBarVisible", ws.isWindowTitleBarVisible);  // need? Not used.
        //settings->setValue("isMenuBarVisible", ws.isMenuBarVisible);
        settings->setValue("isStatusBarVisible", ws.isStatusBarVisible);
        settings->setValue("isFolderDockVisible", ws.isFolderDockVisible);
        settings->setValue("isFavDockVisible", ws.isFavDockVisible);
        settings->setValue("isFilterDockVisible", ws.isFilterDockVisible);
        settings->setValue("isMetadataDockVisible", ws.isMetadataDockVisible);
        settings->setValue("isEmbelDockVisible", ws.isEmbelDockVisible);
        settings->setValue("isThumbDockVisible", ws.isThumbDockVisible);

        // View
        settings->setValue("isLoupeDisplay", ws.isLoupeDisplay);
        settings->setValue("isGridDisplay", ws.isGridDisplay);
        settings->setValue("isTableDisplay", ws.isTableDisplay);
        settings->setValue("isCompareDisplay", ws.isCompareDisplay);

        // ThumbView
        settings->setValue("thumbSpacing", ws.thumbSpacing);                        // need?
        settings->setValue("thumbPadding", ws.thumbPadding);
        settings->setValue("thumbWidth", ws.thumbWidth);
        settings->setValue("thumbHeight", ws.thumbHeight);
        settings->setValue("showThumbLabels", ws.showThumbLabels);

        // GridView
        settings->setValue("thumbSpacingGrid", ws.thumbSpacingGrid);
        settings->setValue("thumbPaddingGrid", ws.thumbPaddingGrid);
        settings->setValue("thumbWidthGrid", ws.thumbWidthGrid);
        settings->setValue("thumbHeightGrid", ws.thumbHeightGrid);
        settings->setValue("labelFontSizeGrid", ws.labelFontSizeGrid);
        settings->setValue("showThumbLabelsGrid", ws.showThumbLabelsGrid);
        settings->setValue("labelChoice", ws.labelChoice);

        // ImageView
        settings->setValue("isImageInfoVisible", ws.isImageInfoVisible);
        // settings->setValue("isEmbelDisplay", ws.isEmbelDisplay);                    // need?

        // Processes
        settings->setValue("isColorManage", ws.isColorManage);
        settings->setValue("cacheSizeMethod", ws.cacheSizeMethod);
        settings->setValue("sortColumn", ws.sortColumn);
        settings->setValue("isReverseSort", ws.isReverseSort);
    }
    settings->endArray();
    //setting->setValue("Workpaces.size", size);
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
