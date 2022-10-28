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
    if (G::isLogger) G::log(CLASSFUNCTION);
    int n = workspaces->count();
    if (n > 9) {
        QString msg = "Only ten workspaces allowed.  Use Manage Workspaces\nto delete or reassign workspaces.";
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
        workspaceActions.at(n)->setShortcut(QKeySequence("Ctrl+" + QString::number(n)));
        workspaceActions.at(n)->setVisible(true);
    }
}

QString MW::fixDupWorkspaceName(QString name)
{
/*
    Name is used to index workspaces, so duplicated are illegal.  If a duplicate is
    found then "_1" is appended.  The function is recursive since the original name
    with "_1" appended also might exist.
*/
    if (G::isLogger) G::log(CLASSFUNCTION);
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
    if (G::isLogger) G::log(CLASSFUNCTION);
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
    if (G::isLogger) G::log(CLASSFUNCTION);
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
    cacheSizeMethod = w.cacheSizeMethod;
    sortColumn = w.sortColumn;
    updateSortColumn(sortColumn);
    if (w.isReverseSort) toggleSortDirection(Tog::on);
    else toggleSortDirection(Tog::off);
    updateState();
    workspaceChanged = true;
    sortChange(CLASSFUNCTION);
    // chk if a video file
    if (dm->sf->index(dm->currentSfRow, G::VideoColumn).data().toBool()) {
        centralLayout->setCurrentIndex(VideoTab);
    }
    // in case thumbdock visibility changed by status of wasThumbDockVisible in loupeDisplay etc
//    setThumbDockVisibity();
}

void MW::snapshotWorkspace(workspaceData &wsd)
{
    if (G::isLogger) G::log(CLASSFUNCTION);
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
    wsd.cacheSizeMethod = cacheSizeMethod;
    wsd.sortColumn = sortColumn;
    wsd.isReverseSort = sortReverseAction->isChecked();
}

void MW::manageWorkspaces()
{
/*
    Delete, rename and reassign workspaces.
*/
    if (G::isLogger) G::log(CLASSFUNCTION);
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
    if (G::isLogger) G::log(CLASSFUNCTION);
    if (workspaces->count() < 1) return;

    // remove workspace from list of workspaces
    workspaces->removeAt(n);

    // remove workspace from settings deleting all and then saving all
    setting->remove("Workspaces");
    saveWorkspaces();

    // sync menus by re-updating.  Tried to use indexes but had problems so
    // resorted to brute force solution
    syncWorkspaceMenu();
}

void MW::syncWorkspaceMenu()
{
    if (G::isLogger) G::log(CLASSFUNCTION);
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
    if (G::isLogger) G::log(CLASSFUNCTION);
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
    if (G::isLogger) G::log(CLASSFUNCTION);
    QRect desktop = QGuiApplication::screens().first()->geometry();
//    QRect desktop = qApp->desktop()->availableGeometry();
//    qDebug() << CLASSFUNCTION << desktop << desktop1;
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
    sortChange(CLASSFUNCTION);
    updateState();
}

void MW::renameWorkspace(int n, QString name)
{
    if (G::isLogger) G::log(CLASSFUNCTION);
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
    if (G::isLogger) G::log(CLASSFUNCTION);
    snapshotWorkspace((*workspaces)[n]);
    (*workspaces)[n].name = name;
}

void MW::reportWorkspace(int n)
{
    if (G::isLogger) G::log(CLASSFUNCTION);
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
    if (G::isLogger) G::log(CLASSFUNCTION);
    if (!isSettings) return;
    int size = setting->beginReadArray("Workspaces");
    for (int i = 0; i < size; ++i) {
        setting->setArrayIndex(i);
        ws.name = setting->value("name").toString();
        ws.geometry = setting->value("geometry").toByteArray();
        ws.state = setting->value("state").toByteArray();
        ws.isFullScreen = setting->value("isFullScreen").toBool();
        ws.isWindowTitleBarVisible = setting->value("isWindowTitleBarVisible").toBool();
        ws.isMenuBarVisible = setting->value("isMenuBarVisible").toBool();
        ws.isStatusBarVisible = setting->value("isStatusBarVisible").toBool();
        ws.isFolderDockVisible = setting->value("isFolderDockVisible").toBool();
        ws.isFavDockVisible = setting->value("isFavDockVisible").toBool();
        ws.isFilterDockVisible = setting->value("isFilterDockVisible").toBool();
        ws.isMetadataDockVisible = setting->value("isMetadataDockVisible").toBool();
        ws.isEmbelDockVisible = setting->value("isEmbelDockVisible").toBool();
        ws.isThumbDockVisible = setting->value("isThumbDockVisible").toBool();
        ws.thumbSpacing = setting->value("thumbSpacing").toInt();
        ws.thumbPadding = setting->value("thumbPadding").toInt();
        ws.thumbWidth = setting->value("thumbWidth").toInt();
        ws.thumbHeight = setting->value("thumbHeight").toInt();
        ws.labelFontSize = setting->value("labelFontSize").toInt();
        ws.showThumbLabels = setting->value("showThumbLabels").toBool();
        ws.thumbSpacingGrid = setting->value("thumbSpacingGrid").toInt();
        ws.thumbPaddingGrid = setting->value("thumbPaddingGrid").toInt();
        ws.thumbWidthGrid = setting->value("thumbWidthGrid").toInt();
        ws.thumbHeightGrid = setting->value("thumbHeightGrid").toInt();
        ws.labelFontSizeGrid = setting->value("labelFontSizeGrid").toInt();
        ws.showThumbLabelsGrid = setting->value("showThumbLabelsGrid").toBool();
        ws.labelChoice = setting->value("labelChoice").toString();
        ws.isImageInfoVisible = setting->value("isImageInfoVisible").toBool();
        ws.isLoupeDisplay = setting->value("isLoupeDisplay").toBool();
        ws.isGridDisplay = setting->value("isGridDisplay").toBool();
        ws.isTableDisplay = setting->value("isTableDisplay").toBool();
        ws.isCompareDisplay = setting->value("isCompareDisplay").toBool();
        ws.isEmbelDisplay = setting->value("isEmbelDisplay").toBool();
        ws.isColorManage = setting->value("isColorManage").toBool();
        ws.cacheSizeMethod = setting->value("cacheSizeMethod").toString();
        ws.sortColumn = setting->value("sortColumn").toInt();
        ws.isReverseSort = setting->value("isReverseSort").toBool();
        workspaces->append(ws);
    }
    setting->endArray();
}

void MW::saveWorkspaces()
{
    if (G::isLogger) G::log(CLASSFUNCTION);
    setting->beginWriteArray("Workspaces");
    for (int i = 0; i < workspaces->count(); ++i) {
        ws = workspaces->at(i);
        setting->setArrayIndex(i);
        setting->setValue("name", ws.name);
        setting->setValue("geometry", ws.geometry);
        setting->setValue("state", ws.state);
        setting->setValue("isFullScreen", ws.isFullScreen);
        setting->setValue("isWindowTitleBarVisible", ws.isWindowTitleBarVisible);
        setting->setValue("isMenuBarVisible", ws.isMenuBarVisible);
        setting->setValue("isStatusBarVisible", ws.isStatusBarVisible);
        setting->setValue("isFolderDockVisible", ws.isFolderDockVisible);
        setting->setValue("isFavDockVisible", ws.isFavDockVisible);
        setting->setValue("isFilterDockVisible", ws.isFilterDockVisible);
        setting->setValue("isMetadataDockVisible", ws.isMetadataDockVisible);
        setting->setValue("isEmbelDockVisible", ws.isEmbelDockVisible);
        setting->setValue("isThumbDockVisible", ws.isThumbDockVisible);
        setting->setValue("thumbSpacing", ws.thumbSpacing);
        setting->setValue("thumbPadding", ws.thumbPadding);
        setting->setValue("thumbWidth", ws.thumbWidth);
        setting->setValue("thumbHeight", ws.thumbHeight);
        setting->setValue("showThumbLabels", ws.showThumbLabels);
        setting->setValue("thumbSpacingGrid", ws.thumbSpacingGrid);
        setting->setValue("thumbPaddingGrid", ws.thumbPaddingGrid);
        setting->setValue("thumbWidthGrid", ws.thumbWidthGrid);
        setting->setValue("thumbHeightGrid", ws.thumbHeightGrid);
        setting->setValue("labelFontSizeGrid", ws.labelFontSizeGrid);
        setting->setValue("showThumbLabelsGrid", ws.showThumbLabelsGrid);
        setting->setValue("labelChoice", ws.labelChoice);
        setting->setValue("isImageInfoVisible", ws.isImageInfoVisible);
        setting->setValue("isLoupeDisplay", ws.isLoupeDisplay);
        setting->setValue("isGridDisplay", ws.isGridDisplay);
        setting->setValue("isTableDisplay", ws.isTableDisplay);
        setting->setValue("isCompareDisplay", ws.isCompareDisplay);
        setting->setValue("isEmbelDisplay", ws.isEmbelDisplay);
        setting->setValue("isColorManage", ws.isColorManage);
        setting->setValue("cacheSizeMethod", ws.cacheSizeMethod);
        setting->setValue("sortColumn", ws.sortColumn);
        setting->setValue("isReverseSort", ws.isReverseSort);
    }
    setting->endArray();
}
