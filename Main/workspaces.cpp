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
*/

void MW::newWorkspace()
{
    if (G::isLogger)
        G::log("MW::newWorkspace");
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

void MW::invokeWorkspaceFromAction(QAction *workAction)
{
    if (G::isLogger) G::log("MW::invokeWorkspaceFromAction");
    for (int i=0; i<workspaces->count(); i++) {
        if (workspaces->at(i).name == workAction->text()) {
            invokeWorkspace(workspaces->at(i));
            return;
        }
    }
}

void MW::invokeWorkspace(const workspaceData &w)
{
/*
    invokeWorkspace is called from a workspace action. Since the workspace actions
    are a list of actions, the workspaceMenu triggered signal is captured, and the
    workspace with a matching name to the action is used.
*/
    if (G::isLogger) G::log("MW::invokeWorkspace");
    restoreGeometry(w.geometry);
    restoreState(w.state);
    // two restoreState req'd for going from docked to floating docks
    restoreState(w.state);
    menuBarVisibleAction->setChecked(w.isMenuBarVisible);
    statusBarVisibleAction->setChecked(w.isStatusBarVisible);
    folderDockVisibleAction->setChecked(w.isFolderDockVisible);
    favDockVisibleAction->setChecked(w.isFavDockVisible);
    filterDockVisibleAction->setChecked(w.isFilterDockVisible);
    metadataDockVisibleAction->setChecked(w.isMetadataDockVisible);
    embelDockVisibleAction->setChecked(w.isEmbelDockVisible);
    thumbDockVisibleAction->setChecked(w.isThumbDockVisible);
    infoVisibleAction->setChecked(w.isImageInfoVisible);
    asLoupeAction->setChecked(w.isLoupeDisplay);
    asGridAction->setChecked(w.isGridDisplay);
    asTableAction->setChecked(w.isTableDisplay);
    asCompareAction->setChecked(w.isCompareDisplay);
    asCompareAction->setChecked(w.isEmbelDisplay);
    thumbView->iconWidth = w.thumbWidth;
    thumbView->iconHeight = w.thumbHeight;
    thumbView->labelFontSize = w.labelFontSize;
    thumbView->showIconLabels = w.showThumbLabels;
    gridView->iconWidth = w.thumbWidthGrid;
    gridView->iconHeight = w.thumbHeightGrid;
    gridView->labelFontSize = w.labelFontSizeGrid;
    gridView->showIconLabels = w.showThumbLabelsGrid;
    gridView->labelChoice = w.labelChoice;
    thumbView->rejustify();
    gridView->rejustify();
    thumbView->setThumbParameters();
    gridView->setThumbParameters();
    if (w.isColorManage) toggleColorManage(Tog::on);
    else toggleColorManage(Tog::off);
    cacheSizeStrategy = w.cacheSizeMethod;
    sortColumn = w.sortColumn;
    updateSortColumn(sortColumn);
    if (w.isReverseSort) toggleSortDirection(Tog::on);
    else toggleSortDirection(Tog::off);
    updateState();
    workspaceChanged = true;
    sortChange("MW::invokeWorkspace");
    // chk if a video file
    if (dm->sf->index(dm->currentSfRow, G::VideoColumn).data().toBool()) {
        centralLayout->setCurrentIndex(VideoTab);
    }
    // in case thumbdock visibility changed by status of wasThumbDockVisible in loupeDisplay etc
//    setThumbDockVisibity();
}

void MW::snapshotWorkspace(workspaceData &wsd)
{
    if (G::isLogger) G::log("MW::snapshotWorkspace");
    wsd.geometry = saveGeometry();
    wsd.state = saveState();
    wsd.isFullScreen = isFullScreen();
    wsd.isMenuBarVisible = menuBarVisibleAction->isChecked();
    wsd.isStatusBarVisible = statusBarVisibleAction->isChecked();
    wsd.isFolderDockVisible = folderDockVisibleAction->isChecked();
    wsd.isFavDockVisible = favDockVisibleAction->isChecked();
    wsd.isFilterDockVisible = filterDockVisibleAction->isChecked();
    wsd.isMetadataDockVisible = metadataDockVisibleAction->isChecked();
    wsd.isEmbelDockVisible = embelDockVisibleAction->isChecked();
    wsd.isThumbDockVisible = thumbDockVisibleAction->isChecked();
    wsd.isImageInfoVisible = infoVisibleAction->isChecked();

    wsd.isLoupeDisplay = asLoupeAction->isChecked();
    wsd.isGridDisplay = asGridAction->isChecked();
    wsd.isTableDisplay = asTableAction->isChecked();
    wsd.isCompareDisplay = asCompareAction->isChecked();

    wsd.thumbWidth = thumbView->iconWidth;
    wsd.thumbHeight = thumbView->iconHeight;
    wsd.labelFontSize = thumbView->labelFontSize;
    wsd.showThumbLabels = thumbView->showIconLabels;

    wsd.thumbWidthGrid = gridView->iconWidth;
    wsd.thumbHeightGrid = gridView->iconHeight;
    wsd.labelFontSizeGrid = gridView->labelFontSize;
    wsd.showThumbLabelsGrid = gridView->showIconLabels;
    wsd.labelChoice = gridView->labelChoice;

    wsd.isImageInfoVisible = infoVisibleAction->isChecked();

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
    connect(workspaceDlg, SIGNAL(deleteWorkspace(int)),
            this, SLOT(deleteWorkspace(int)));
    connect(workspaceDlg, SIGNAL(reassignWorkspace(int)),
            this, SLOT(reassignWorkspace(int)));
    connect(workspaceDlg, SIGNAL(renameWorkspace(int, QString)),
            this, SLOT(renameWorkspace(int, QString)));
    connect(workspaceDlg, SIGNAL(reportWorkspace(int)),
            this, SLOT(reportWorkspace(int)));
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

    // remove workspace from settings deleting all and then saving all
    settings->remove("Workspaces");
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
    reportWorkspace(n);
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
    menuBarVisibleAction->setChecked(true);
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

void MW::reportWorkspace(int n)
{
    if (G::isLogger) G::log("MW::reportWorkspace");
    ws = workspaces->at(n);
    qDebug() << G::t.restart() << "\t" << "\n\nName" << ws.name
             << "\nGeometry" << ws.geometry
             << "\nState" << ws.state
             << "\nisFullScreen" << ws.isFullScreen
             << "\nisWindowTitleBarVisible" << ws.isWindowTitleBarVisible
             << "\nisMenuBarVisible" << ws.isMenuBarVisible
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
                ;
}

void MW::loadWorkspaces()
{
    if (G::isLogger) G::log("MW::loadWorkspaces");
    if (!isSettings) return;
    int size = settings->beginReadArray("Workspaces");
    qDebug() << "MW::loadWorkspaces" << size;
    for (int i = 0; i < size; ++i) {
        settings->setArrayIndex(i);
        ws.name = settings->value("name").toString();

        ws.geometry = settings->value("geometry").toByteArray();
        ws.state = settings->value("state").toByteArray();
        ws.isFullScreen = settings->value("isFullScreen").toBool();
        ws.isWindowTitleBarVisible = settings->value("isWindowTitleBarVisible").toBool();
        ws.isMenuBarVisible = settings->value("isMenuBarVisible").toBool();
        ws.isStatusBarVisible = settings->value("isStatusBarVisible").toBool();
        ws.isFolderDockVisible = settings->value("isFolderDockVisible").toBool();
        ws.isFavDockVisible = settings->value("isFavDockVisible").toBool();
        ws.isFilterDockVisible = settings->value("isFilterDockVisible").toBool();
        ws.isMetadataDockVisible = settings->value("isMetadataDockVisible").toBool();
        ws.isEmbelDockVisible = settings->value("isEmbelDockVisible").toBool();
        ws.isThumbDockVisible = settings->value("isThumbDockVisible").toBool();
        ws.thumbSpacing = settings->value("thumbSpacing").toInt();
        ws.thumbPadding = settings->value("thumbPadding").toInt();
        ws.thumbWidth = settings->value("thumbWidth").toInt();
        ws.thumbHeight = settings->value("thumbHeight").toInt();
        ws.labelFontSize = settings->value("labelFontSize").toInt();
        ws.showThumbLabels = settings->value("showThumbLabels").toBool();
        ws.thumbSpacingGrid = settings->value("thumbSpacingGrid").toInt();
        ws.thumbPaddingGrid = settings->value("thumbPaddingGrid").toInt();
        ws.thumbWidthGrid = settings->value("thumbWidthGrid").toInt();
        ws.thumbHeightGrid = settings->value("thumbHeightGrid").toInt();
        ws.labelFontSizeGrid = settings->value("labelFontSizeGrid").toInt();
        ws.showThumbLabelsGrid = settings->value("showThumbLabelsGrid").toBool();
        ws.labelChoice = settings->value("labelChoice").toString();
        ws.isImageInfoVisible = settings->value("isImageInfoVisible").toBool();
        ws.isLoupeDisplay = settings->value("isLoupeDisplay").toBool();
        ws.isGridDisplay = settings->value("isGridDisplay").toBool();
        ws.isTableDisplay = settings->value("isTableDisplay").toBool();
        ws.isCompareDisplay = settings->value("isCompareDisplay").toBool();
        ws.isEmbelDisplay = settings->value("isEmbelDisplay").toBool();
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
    int size = workspaces->count();
    settings->beginWriteArray("Workspaces", size);
    qDebug() << "MW::saveWorkspaces" << size;
    for (int i = 0; i < size; ++i) {
        ws = workspaces->at(i);
        settings->setArrayIndex(i);
        settings->setValue("name", ws.name);
        settings->setValue("geometry", ws.geometry);
        settings->setValue("state", ws.state);
        settings->setValue("isFullScreen", ws.isFullScreen);
        settings->setValue("isWindowTitleBarVisible", ws.isWindowTitleBarVisible);
        settings->setValue("isMenuBarVisible", ws.isMenuBarVisible);
        settings->setValue("isStatusBarVisible", ws.isStatusBarVisible);
        settings->setValue("isFolderDockVisible", ws.isFolderDockVisible);
        settings->setValue("isFavDockVisible", ws.isFavDockVisible);
        settings->setValue("isFilterDockVisible", ws.isFilterDockVisible);
        settings->setValue("isMetadataDockVisible", ws.isMetadataDockVisible);
        settings->setValue("isEmbelDockVisible", ws.isEmbelDockVisible);
        settings->setValue("isThumbDockVisible", ws.isThumbDockVisible);
        settings->setValue("thumbSpacing", ws.thumbSpacing);
        settings->setValue("thumbPadding", ws.thumbPadding);
        settings->setValue("thumbWidth", ws.thumbWidth);
        settings->setValue("thumbHeight", ws.thumbHeight);
        settings->setValue("showThumbLabels", ws.showThumbLabels);
        settings->setValue("thumbSpacingGrid", ws.thumbSpacingGrid);
        settings->setValue("thumbPaddingGrid", ws.thumbPaddingGrid);
        settings->setValue("thumbWidthGrid", ws.thumbWidthGrid);
        settings->setValue("thumbHeightGrid", ws.thumbHeightGrid);
        settings->setValue("labelFontSizeGrid", ws.labelFontSizeGrid);
        settings->setValue("showThumbLabelsGrid", ws.showThumbLabelsGrid);
        settings->setValue("labelChoice", ws.labelChoice);
        settings->setValue("isImageInfoVisible", ws.isImageInfoVisible);
        settings->setValue("isLoupeDisplay", ws.isLoupeDisplay);
        settings->setValue("isGridDisplay", ws.isGridDisplay);
        settings->setValue("isTableDisplay", ws.isTableDisplay);
        settings->setValue("isCompareDisplay", ws.isCompareDisplay);
        settings->setValue("isEmbelDisplay", ws.isEmbelDisplay);
        settings->setValue("isColorManage", ws.isColorManage);
        settings->setValue("cacheSizeMethod", ws.cacheSizeMethod);
        settings->setValue("sortColumn", ws.sortColumn);
        settings->setValue("isReverseSort", ws.isReverseSort);
    }
    settings->endArray();
    //setting->setValue("Workpaces.size", size);
}
