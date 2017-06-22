
#include "dircompleter.h"
#include "mainwindow.h"
#include "global.h"

#define THUMB_SIZE_MIN	40
#define THUMB_SIZE_MAX	160

MW::MW(QWidget *parent) : QMainWindow(parent)
{
    {
    #ifdef ISDEBUG
    qDebug() << "MW::MW";
    #endif
    }

    /* Note ISDEBUG is in globals.h
       Deactivate debug reporting by commenting ISDEBUG  */

    // use this to show thread activity
    G::isThreadTrackingOn = false;

    // used here for testing/debugging
    bool resetSettings = false;

    G::appName = "Winnow";
    //    GData::isDebug = true;        // is this used or just #ifdef ISDEBUG in global.h
    G::isTimer = false;
    isIconView = true;                  // rgh for now

    // Global timer for testing
    #ifdef ISDEBUG
    //    GData::t.start();
    #endif

    G::devicePixelRatio = 1;
    #ifdef Q_OS_MAC
    G::devicePixelRatio = 2;
    #endif

    workspaces = new QList<workspaceData>;
    G::setting = new QSettings("Winnow", "winnow_100");
    qDebug() << G::setting->fileName();
    // must come first so persistant action settings can be updated
    if (!resetSettings) loadSettings();
    createThumbView();
    createImageView();
    createActions();
    createMenus();
    createStatusBar();
    createFSTree();
    createBookmarks();
    updateExternalApps();
    loadShortcuts(true);            // dependent on createActions
    setupDocks();

    //    connect(qApp, SIGNAL(focusChanged(QWidget*, QWidget*)), this, SLOT(updateCache()));

    if (!resetSettings) restoreGeometry(G::setting->value("Geometry").toByteArray());
    if (!resetSettings) restoreState(G::setting->value("WindowState").toByteArray());

    #ifdef Q_OS_LINIX

    #endif
    #ifdef Q_OS_WIN
        setWindowIcon(QIcon(":/images/winnow.png"));
    #endif
    #ifdef Q_OS_MAC
        setWindowIcon(QIcon(":/images/winnow.icns"));
    #endif

    this->setWindowTitle("Winnow");
    this->setObjectName("WinnowMW");

    mainLayout = new QHBoxLayout;
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);
    mainLayout->addWidget(imageView);
    QWidget *centralWidget = new QWidget;
    centralWidget->setLayout(mainLayout);
    setCentralWidget(centralWidget);

//    if (isIconView) thumbDock->setWindowTitle("Thumbnails");
//    else thumbDock->setWindowTitle("Image Files");
    thumbDock->setWindowTitle(" ");

    // add error trapping for file io  rgh todo
    QFile fStyle(":/qss/teststyle.css");
    fStyle.open(QIODevice::ReadOnly);
    this->setStyleSheet(fStyle.readAll());

    handleStartupArgs();

    copyMoveToDialog = 0;       // rgh req'd?
    initComplete = true;
    interfaceDisabled = false;

//    if (GData::layoutMode == thumbViewIdx)
//     thumbView->setFocus(Qt::OtherFocusReason);

}

// Do we need this?  rgh
void MW::handleStartupArgs()
{
    {
    #ifdef ISDEBUG
    qDebug() << "MW::handleStartupArgs";
    #endif
    }
//    if (GData::startupDir == GData::specifiedDir)
//        GData::currentViewDir = GData::specifiedStartDir;
//    else if (GData::startupDir == GData::rememberLastDir)
//        GData::currentViewDir = GData::appSettings->value("lastDir").toString();
//    selectCurrentViewDir();   // rgh 2017-04-04
}

/**************************************************************************
 PROGRAM FLOW EVENT DISPATCH

 A new folder is selected which triggers folderSelectionChange:

 1.  A list of all eligible image files in the folder is generated in
     thumbView.

 2.  A thread is spawned to cache metadata for all the images.

 3.  A second thread is spawned by the metadata thread to cache all the
     thumbnails into thumbView.

 4.  A third thread is spawned by the metadata thread to cache as many full
     size images as assigned memory permits.

 5.  Selecting a new folder also causes the selection of the first image
     which fires a signal for fileSelectionChange.

 6.  The first image is loaded.  The metadata and thumbnail generation
     threads will still be running, but things should appear to be
     speedy for the user.

 7.  The metadata caching thread collects information required by the
     thumb and image cache threads.

8.   The thumb cache thread needs the file offset and length of the
     embedded thumb jpg that is used to create the icons in thumbview.

9    The image caching thread requires the offset and length for the
     full size embedded jpg, the image width and height in order to
     calculate memory requirements, build the image priority queues, the
     target range and limit the cache to the assigned maximum size.

 *************************************************************************/

// triggered when new folder selection
void MW::folderSelectionChange()
{
    {
    #ifdef ISDEBUG
        qDebug() << "MW::folderSelectionChange";
    #endif
    }
    // Stop any threads that might be running.
    metadataCacheThread->stopMetadateCache();
    thumbCacheThread->stopThumbCache();
    imageCacheThread->stopImageCache();

    QString dirPath = getSelectedPath();
    QDir testDir;
    testDir.setPath(dirPath);
    // check if unmounted USB drive
    if (!testDir.isReadable()) return;

    if (dirPath == "") {
        QMessageBox msgBox;
        msgBox.critical(this, tr("Error"), tr("The folder does not exist or is not available"));
        return;
    }

    // ignore if present folder is rechosen
    if (dirPath == G::currentViewDir) return;
    else G::currentViewDir = dirPath;

    // We do not want to update the imageCache while metadata is still being
    // loaded.  The imageCache update is triggered in fileSelectionChange,
    // which is also executed when the change file event is fired.
    metadataLoaded = false;

    // need to gather directory file info first (except icon/thumb) which is
    // loaded by loadThumbCache.  If no images in new folder then cleanup and
    // exit.
    if (!thumbView->load(subFoldersAction->isChecked())) {
        updateStatus("No images in this folder");
        infoView->clearInfo();
        imageView->clear();
        cacheLabel->setVisible(false);
        return;
    }

    // Must load metadata first, as it contains the file offsets and lengths
    // for the thumbnail and full size embedded jpgs and the image width
    // and height, req'd in imageCache to manage cache max size.  Triggers
    // loadThumbCache and loadImageCache when finishes metadata cache.
    // The thumb cache includes icons (thumbnails) for all the images in
    // the folder.
    // The image cache holds as many full size images in memory as possible.
    loadMetadataCache();
}

// triggered when file selection changes (folder change selects new image)
void MW::fileSelectionChange()
{
    {
    #ifdef ISDEBUG
    qDebug() << "MW::fileSelectionChange";
    #endif
    }

    // user clicks outside thumb but inside treeView dock
    if (thumbView->selectionModel()->selectedIndexes().isEmpty()) return;

    QString fPath = thumbView->currentIndex().data(thumbView->FileNameRole).toString();

    // use cache if image loaded, else read it from file
    if (imageView->loadImage(fPath)) {
        if (G::isThreadTrackingOn) qDebug()
            << "MW::fileSelectionChange - loaded image file "
            << fPath;
        updatePick();
        infoView->updateInfo(fPath);
    }
    else updateStatus("Could not read " + fPath);

    // If the metadataCache is finished then update the imageCache,
    // recalculating the target images to cache, decaching and caching to keep
    // the cache up-to-date with the current image selection.
    if (metadataLoaded) {
        imageCacheThread->updateImageCache(thumbView->
        thumbFileInfoList, fPath);
    }
}

void MW::loadMetadataCache()
{
    {
    #ifdef ISDEBUG
    qDebug() << "MW::loadMetadataCache";
    #endif
    }
    metadataCacheThread->stopMetadateCache();
    metadataCacheThread->loadMetadataCache();
}

void MW::loadThumbCache()
{
    {
    #ifdef ISDEBUG
    qDebug() << "MW::loadThumbCache";
    #endif
    }
    thumbCacheThread->stopThumbCache();
    thumbCacheThread->loadThumbCache();
}

void MW::loadImageCache()
{
    {
    #ifdef ISDEBUG
    qDebug() << "MW::loadImageCache";
    #endif
    }
//    qDebug() << "MW::loadImageCache";
    metadataLoaded = true;
    QModelIndexList indexesList = thumbView->selectionModel()->selectedIndexes();

    QString fPath = indexesList.first().data(thumbView->FileNameRole).toString();
    // imageChacheThread checks if already running and restarts
    imageCacheThread->initImageCache(thumbView->thumbFileInfoList, cacheSizeMB,
        isShowCacheStatus, cacheStatusWidth, cacheWtAhead);
    imageCacheThread->updateImageCache(thumbView->
        thumbFileInfoList, fPath);
}

// called by signal itemClicked in bookmark
void MW::bookmarkClicked(QTreeWidgetItem *item, int col)
{
    {
    #ifdef ISDEBUG
    qDebug() << "MW::bookmarkClicked";
    #endif
    }
    fsTree->setCurrentIndex(fsTree->fsModel->index(item->toolTip(col)));
    folderSelectionChange();
}

// called when signal rowsRemoved from FSTree
// does this get triggered if a drive goes offline???
// rgh this needs some TLC
void MW::checkDirState(const QModelIndex &, int, int)
{
    {
    #ifdef ISDEBUG
    qDebug() << "MW::checkDirState";
    #endif
    }
    if (!initComplete) {
        return;
    }

//    if (thumbView->busy)
//    {
//        thumbView->abort();
//    }

    if (!QDir().exists(G::currentViewDir))
    {
        G::currentViewDir = "";
//		QTimer::singleShot(0, this, SLOT(reloadThumbsSlot()));
//        reloadThumbsSlot();
    }
}

QString MW::getSelectedPath()
{
    {
    #ifdef ISDEBUG
    qDebug() << "MW::getSelectedPath";
    #endif
    }
    QModelIndexList selectedDirs = fsTree->selectionModel()->selectedRows();
    if (selectedDirs.size() && selectedDirs[0].isValid())
    {
        QFileInfo dirInfo = QFileInfo(fsTree->fsModel->filePath(selectedDirs[0]));
        return dirInfo.absoluteFilePath();
    }
    else
        return "";
}

void MW::createActions()
{
    {
    #ifdef ISDEBUG
    qDebug() << "MW::createActions";
    #endif
    }
    // File menu

    //rgh need slot function
    openAction = new QAction(tr("Open Folder"), this);
    openAction->setObjectName("openFolder");
//    openAction->setIcon(QIcon::fromTheme("document-open", QIcon(":/images/open.png")));
    connect(openAction, SIGNAL(triggered()), this, SLOT(openOp()));

    // temp hijack to test refresh thumbs
    connect(openAction, SIGNAL(triggered()), this, SLOT(openOp()));

    // rgh check out if dup
    openWithMenu = new QMenu(tr("Open With..."));
    openWithMenuAction = new QAction(tr("Open With..."), this);
    openWithMenuAction->setObjectName("openWithMenu");
    openWithMenuAction->setMenu(openWithMenu);

    chooseAppAction = new QAction(tr("Manage External Applications"), this);
    chooseAppAction->setObjectName("chooseApp");
    connect(chooseAppAction, SIGNAL(triggered()), this, SLOT(chooseExternalApp()));

    newAppAction = new QAction(tr("New app"), this);
    newAppAction->setObjectName("newApp");
//    connect(newAppAction, SIGNAL(triggered()), this, SLOT(chooseExternalApp()));

    deleteAppAction = new QAction(tr("Delete app"), this);
    deleteAppAction->setObjectName("deleteApp");
//    connect(deleteAppAction, SIGNAL(triggered()), this, SLOT(chooseExternalApp()));

    // Place keeper for now
    revealFileAction = new QAction(tr("Reveal in finder/explorer"), this);
    revealFileAction->setObjectName("openInFinder");
    connect(revealFileAction, SIGNAL(triggered()),
        this, SLOT(revealFile()));

    subFoldersAction = new QAction(tr("Include Sub-folders"), this);
    subFoldersAction->setObjectName("subFolders");
    subFoldersAction->setChecked(mwd.includeSubfolders);    // from QSettings
    connect(subFoldersAction, SIGNAL(triggered()), this, SLOT(setIncludeSubFolders()));

    addBookmarkAction = new QAction(tr("Add Bookmark"), this);
    addBookmarkAction->setObjectName("addBookmark");
    connect(addBookmarkAction, SIGNAL(triggered()), this, SLOT(addNewBookmark()));

    removeBookmarkAction = new QAction(tr("Remove Bookmark"), this);
    removeBookmarkAction->setObjectName("removeBookmark");
    // rgh where did removeBookmark slot function go?
    connect(removeBookmarkAction, SIGNAL(triggered()), this, SLOT(removeBookmark()));

    ingestAction = new QAction(tr("Ingest picks"), this);
    ingestAction->setObjectName("ingest");
    connect(ingestAction, SIGNAL(triggered()), this, SLOT(copyPicks()));

    // Place keeper for now
    renameAction = new QAction(tr("Rename selected images"), this);
    renameAction->setObjectName("renameImages");
    renameAction->setShortcut(Qt::Key_F2);
//    connect(renameAction, SIGNAL(triggered()), this, SLOT(renameImages()));

    // Place keeper for now
    runDropletAction = new QAction(tr("Run Droplet"), this);
    runDropletAction->setObjectName("runDroplet");
//    connect(runDropletAction, SIGNAL(triggered()), this, SLOT(runDroplet()));

    reportMetadataAction = new QAction(tr("Report Metadata"), this);
    reportMetadataAction->setObjectName("reportMetadata");
    connect(reportMetadataAction, SIGNAL(triggered()),
            this, SLOT(reportMetadata()));

    // Appears in Winnow menu in OSX
    exitAction = new QAction(tr("Exit"), this);
    exitAction->setObjectName("exit");
    connect(exitAction, SIGNAL(triggered()), this, SLOT(close()));

    // Edit Menu

    selectAllAction = new QAction(tr("Select All"), this);
    selectAllAction->setObjectName("selectAll");
    connect(selectAllAction, SIGNAL(triggered()), this, SLOT(selectAllThumbs()));

    invertSelectionAction = new QAction(tr("Invert Selection"), this);
    invertSelectionAction->setObjectName("invertSelection");
    connect(invertSelectionAction, SIGNAL(triggered()), thumbView,
            SLOT(invertSelection()));

    togglePickAction = new QAction(tr("Pick"), this);
    togglePickAction->setObjectName("togglePick");
    togglePickAction->setCheckable(true);
    togglePickAction->setChecked(false);
    connect(togglePickAction, SIGNAL(triggered()), this, SLOT(togglePick()));

    toggleFilterPickAction = new QAction(tr("Filter picks only"), this);
    toggleFilterPickAction->setObjectName("toggleFilterPick");
    toggleFilterPickAction->setCheckable(true);
    toggleFilterPickAction->setChecked(false);
    connect(toggleFilterPickAction, SIGNAL(triggered()), this, SLOT(toggleFilterPick()));

    // Place keeper for now
    copyImagesAction = new QAction(tr("Copy to clipboard"), this);
    copyImagesAction->setObjectName("copyImages");
    copyImagesAction->setShortcut(QKeySequence("Ctrl+C"));
    //    connect(copyImagesAction, SIGNAL(triggered()), this, SLOT(copyImagesorSomething()));

    rotateRightAction = new QAction(tr("Rotate CW"), this);
    rotateRightAction->setObjectName("rotateRight");
    connect(rotateRightAction, SIGNAL(triggered()), this, SLOT(rotateRight()));

    rotateLeftAction = new QAction(tr("Rotate CCW"), this);
    rotateLeftAction->setObjectName("rotateLeft");
    connect(rotateLeftAction, SIGNAL(triggered()), this, SLOT(rotateLeft()));

    prefAction = new QAction(tr("Preferences"), this);
    prefAction->setObjectName("settings");
    connect(prefAction, SIGNAL(triggered()), this, SLOT(preferences()));

    oldPrefAction = new QAction(tr("Old Preferences"), this);
    oldPrefAction->setObjectName("settings");
    connect(oldPrefAction, SIGNAL(triggered()), this, SLOT(oldPreferences()));

    // Go menu

    nextThumbAction = new QAction(tr("Next Image"), this);
    nextThumbAction->setObjectName("nextImage");
    nextThumbAction->setEnabled(true);
    connect(nextThumbAction, SIGNAL(triggered()), this, SLOT(loadNextImage()));

    prevThumbAction = new QAction(tr("Previous"), this);
    prevThumbAction->setObjectName("prevImage");
    connect(prevThumbAction, SIGNAL(triggered()), this, SLOT(loadPrevImage()));

    upThumbAction = new QAction(tr("Move Up"), this);
    upThumbAction->setObjectName("moveUp");
    connect(upThumbAction, SIGNAL(triggered()), this, SLOT(loadUpImage()));

    downThumbAction = new QAction(tr("Move Down"), this);
    downThumbAction->setObjectName("moveDown");
    connect(downThumbAction, SIGNAL(triggered()), this, SLOT(loadDownImage()));

    firstThumbAction = new QAction(tr("First"), this);
    firstThumbAction->setObjectName("firstImage");
    connect(firstThumbAction, SIGNAL(triggered()), this, SLOT(loadFirstImage()));

    lastThumbAction = new QAction(tr("Last"), this);
    lastThumbAction->setObjectName("lastImage");
    connect(lastThumbAction, SIGNAL(triggered()), this, SLOT(loadLastImage()));

    // Not a menu item - used by slide show
    randomImageAction = new QAction(tr("Random"), this);
    randomImageAction->setObjectName("randomImage");
    connect(randomImageAction, SIGNAL(triggered()), this, SLOT(loadRandomImage()));

    // Place keeper
    nextPickAction = new QAction(tr("Next Pick"), this);
    nextPickAction->setObjectName("nextPick");
//    connect(nextPickAction, SIGNAL(triggered()), this, SLOT(nextPick()));

    // Place keeper
    prevPickAction = new QAction(tr("Previous Pick"), this);
    prevPickAction->setObjectName("prevPick");
//    connect(prevPickAction, SIGNAL(triggered()), this, SLOT(prevPick()));

    // View menu
    slideShowAction = new QAction(tr("Slide Show"), this);
    slideShowAction->setObjectName("slideShow");
    connect(slideShowAction, SIGNAL(triggered()), this, SLOT(slideShow()));

    fullScreenAction = new QAction(tr("Full Screen"), this);
    fullScreenAction->setObjectName("fullScreenAct");
    fullScreenAction->setCheckable(true);
    connect(fullScreenAction, SIGNAL(triggered()), this, SLOT(toggleFullScreen()));

    escapeFullScreenAction = new QAction(tr("Escape Full Screen"), this);
    escapeFullScreenAction->setObjectName("escapeFullScreenAct");
    connect(escapeFullScreenAction, SIGNAL(triggered()), this, SLOT(escapeFullScreen()));

    zoomOutAction = new QAction(tr("Zoom Out"), this);
    zoomOutAction->setObjectName("zoomOut");
    connect(zoomOutAction, SIGNAL(triggered()), this, SLOT(zoomOut()));

    zoomInAction = new QAction(tr("Zoom In"), this);
    zoomInAction->setObjectName("zoomIn");
    connect(zoomInAction, SIGNAL(triggered()), this, SLOT(zoomIn()));

    zoomFitAction = new QAction(tr("Reset Zoom"), this);
    zoomFitAction->setObjectName("resetZoom");
    connect(zoomFitAction, SIGNAL(triggered()), this, SLOT(zoomToFit()));

    zoomOrigAction = new QAction(tr("Original Size"), this);
    zoomOrigAction->setObjectName("origZoom");
    connect(zoomOrigAction, SIGNAL(triggered()), this, SLOT(zoom100()));

    infoVisibleAction = new QAction(tr("Shooting Info"), this);
    infoVisibleAction->setObjectName("toggleInfo");
    infoVisibleAction->setCheckable(true);
    infoVisibleAction->setChecked(mwd.isImageInfoVisible);  // from QSettings
    connect(infoVisibleAction, SIGNAL(triggered()), this, SLOT(setShootingInfo()));


    asListAction = new QAction(tr("As list"), this);
    asListAction->setCheckable(true);
//    connect(asListAction, SIGNAL(triggered()), this, SLOT(thumbsEnlarge()));

    asThumbsAction = new QAction(tr("As thumbs"), this);
    asThumbsAction->setCheckable(true);
//    connect(thumbsEnlargeAction, SIGNAL(triggered()), this, SLOT(thumbsEnlarge()));

    iconGroupAction = new QActionGroup(this);
    iconGroupAction->setExclusive(true);
    iconGroupAction->addAction(asListAction);
    iconGroupAction->addAction(asThumbsAction);

    thumbsEnlargeAction = new QAction(tr("Enlarge thumbs"), this);
    thumbsEnlargeAction->setObjectName("enlargeThumbs");
    connect(thumbsEnlargeAction, SIGNAL(triggered()), thumbView, SLOT(thumbsEnlarge()));
    if (thumbView->thumbSize == THUMB_SIZE_MAX)
        thumbsEnlargeAction->setEnabled(false);

    thumbsShrinkAction = new QAction(tr("Shrink thumbs"), this);
    thumbsShrinkAction->setObjectName("shrinkThumbs");
    connect(thumbsShrinkAction, SIGNAL(triggered()), thumbView, SLOT(thumbsShrink()));
    if (thumbView->thumbSize == THUMB_SIZE_MIN)
            thumbsShrinkAction->setEnabled(false);

    // is this used - not in menu
    thumbsFitAction = new QAction(tr("Fit Current Thumbnail"), this);
    thumbsFitAction->setObjectName("thumbsZoomOut");
    connect(thumbsFitAction, SIGNAL(triggered()), thumbView, SLOT(thumbsFit()));
    if (thumbView->thumbSize == THUMB_SIZE_MIN)
            thumbsFitAction->setEnabled(false);

    showThumbLabelsAction = new QAction(tr("Thumb labels"), this);
    showThumbLabelsAction->setObjectName("showLabels");
    showThumbLabelsAction->setCheckable(true);
    showThumbLabelsAction->setChecked(thumbView->showThumbLabels);
    connect(showThumbLabelsAction, SIGNAL(triggered()), this, SLOT(setThumbLabels()));
    showThumbLabelsAction->setEnabled(true);

    reverseSortAction = new QAction(tr("Reverse order"), this);
    reverseSortAction->setObjectName("reverse");
    reverseSortAction->setCheckable(true);
    connect(reverseSortAction, SIGNAL(triggered()), this, SLOT(sortThumbnails()));

    // Window menu

    thumbDockVisibleAction = new QAction(tr("Thumbnails"), this);
    thumbDockVisibleAction->setCheckable(true);
    thumbDockVisibleAction->setChecked(true);
    connect(thumbDockVisibleAction, SIGNAL(triggered()), this, SLOT(setThumbDockVisibity()));

    folderDockVisibleAction = new QAction(tr("Folder"), this);
    folderDockVisibleAction->setObjectName("toggleFiless");
    folderDockVisibleAction->setCheckable(true);
    folderDockVisibleAction->setChecked(true);
    connect(folderDockVisibleAction, SIGNAL(triggered()), this, SLOT(setFolderDockVisibility()));

    favDockVisibleAction = new QAction(tr("Favourites"), this);
    favDockVisibleAction->setObjectName("toggleFavs");
    favDockVisibleAction->setCheckable(true);
    favDockVisibleAction->setChecked(true);
    connect(favDockVisibleAction, SIGNAL(triggered()), this, SLOT(setFavDockVisibility()));

    metadataDockVisibleAction = new QAction(tr("Metadata"), this);
    metadataDockVisibleAction->setObjectName("toggleMetadata");
    metadataDockVisibleAction->setCheckable(true);
    metadataDockVisibleAction->setChecked(true);
    connect(metadataDockVisibleAction, SIGNAL(triggered()), this, SLOT(setMetadataDockVisibility()));

    windowsTitleBarVisibleAction = new QAction(tr("Window Titlebar"), this);
    windowsTitleBarVisibleAction->setObjectName("toggleWindowsTitleBar");
    windowsTitleBarVisibleAction->setCheckable(true);
    windowsTitleBarVisibleAction->setChecked(mwd.isWindowTitleBarVisible);
    connect(windowsTitleBarVisibleAction, SIGNAL(triggered()), this, SLOT(setWindowsTitleBarVisibility()));

    menuBarVisibleAction = new QAction(tr("Menubar"), this);
    menuBarVisibleAction->setObjectName("toggleMenuBar");
    menuBarVisibleAction->setCheckable(true);
    menuBarVisibleAction->setChecked(mwd.isMenuBarVisible);
    connect(menuBarVisibleAction, SIGNAL(triggered()), this, SLOT(setMenuBarVisibility()));

    statusBarVisibleAction = new QAction(tr("Statusbar"), this);
    statusBarVisibleAction->setObjectName("toggleStatusBar");
    statusBarVisibleAction->setCheckable(true);
    statusBarVisibleAction->setChecked(mwd.isStatusBarVisible);
    connect(statusBarVisibleAction, SIGNAL(triggered()), this, SLOT(setStatusBarVisibility()));

    folderDockLockAction = new QAction(tr("Lock Files"), this);
    folderDockLockAction->setObjectName("lockDockFiles");
    folderDockLockAction->setCheckable(true);
    folderDockLockAction->setChecked(mwd.isFolderDockLocked);
    connect(folderDockLockAction, SIGNAL(triggered()), this, SLOT(setFolderDockLockMode()));

    favDockLockAction = new QAction(tr("Lock Favourites"), this);
    favDockLockAction->setObjectName("lockDockFavs");
    favDockLockAction->setCheckable(true);
    favDockLockAction->setChecked(mwd.isFavDockLocked);
    connect(favDockLockAction, SIGNAL(triggered()), this, SLOT(setFavDockLockMode()));

    metadataDockLockAction = new QAction(tr("Lock Metadata"), this);
    metadataDockLockAction->setObjectName("lockDockMetadata");
    metadataDockLockAction->setCheckable(true);
    metadataDockLockAction->setChecked(mwd.isMetadataDockLocked);
    connect(metadataDockLockAction, SIGNAL(triggered()), this, SLOT(setMetadataDockLockMode()));

    thumbDockLockAction = new QAction(tr("Lock Thumbs"), this);
    thumbDockLockAction->setObjectName("lockDockThumbs");
    thumbDockLockAction->setCheckable(true);
    thumbDockLockAction->setChecked(mwd.isThumbDockLocked);
    connect(thumbDockLockAction, SIGNAL(triggered()), this, SLOT(setThumbDockLockMode()));

    allDocksLockAction = new QAction(tr("Lock all docks"), this);
    allDocksLockAction->setObjectName("lockDocks");
    allDocksLockAction->setCheckable(true);
    if (mwd.isFolderDockLocked && mwd.isFavDockLocked && mwd.isMetadataDockLocked && mwd.isThumbDockLocked)
        allDocksLockAction->setChecked(true);
    connect(allDocksLockAction, SIGNAL(triggered()), this, SLOT(setAllDocksLockMode()));

    // Workspace submenu of Window menu
    defaultWorkspaceAction = new QAction(tr("Default Workspace"), this);
    defaultWorkspaceAction->setObjectName("defaultWorkspace");
    connect(defaultWorkspaceAction, SIGNAL(triggered()), this, SLOT(defaultWorkspace()));

    newWorkspaceAction = new QAction(tr("New Workspace"), this);
    newWorkspaceAction->setObjectName("newWorkspace");
    connect(newWorkspaceAction, SIGNAL(triggered()), this, SLOT(newWorkspace()));
    addAction(newWorkspaceAction);

    manageWorkspaceAction = new QAction(tr("Manage Workspaces ..."), this);
    manageWorkspaceAction->setObjectName("manageWorkspaces");
    connect(manageWorkspaceAction, SIGNAL(triggered()),
            this, SLOT(manageWorkspaces()));
    addAction(manageWorkspaceAction);

    // general connection to handle invoking new workspaces
    // MacOS will not allow runtime menu insertions.  Cludge workaround
    // add 10 dummy menu items and then hide until use.
    int n = workspaces->count();
    for (int i=0; i<10; i++) {
        QString name;
        QString objName = "";
        if (i < n) {
            name = workspaces->at(i).name;
            objName = "workspace" + QString::number(i);
        }
        else name = "Future Workspace" + QString::number(i);
        workspaceActions.append(new QAction(name, this));
        if (i < n) {
            workspaceActions.at(i)->setShortcut(QKeySequence("Ctrl+" + QString::number(i)));
            workspaceActions.at(i)->setObjectName(objName);
            workspaceActions.at(i)->setText(name);
            workspaceActions.at(i)->setVisible(true);
        }
        if (i >= n) workspaceActions.at(i)->setVisible(false);
        workspaceActions.at(i)->setShortcut(QKeySequence("Ctrl+" + QString::number(i)));
    }
    addActions(workspaceActions);
    // connection moved to after menu creation as will not work before
//    connect(workspaceMenu, SIGNAL(triggered(QAction*)),
//            SLOT(invokeWorkspace(QAction*)));

    //Help menu

    aboutAction = new QAction(tr("About"), this);
    aboutAction->setObjectName("about");
    connect(aboutAction, SIGNAL(triggered()), this, SLOT(about()));

    helpAction = new QAction(tr("Winnow Help"), this);
    helpAction->setObjectName("help");
//    connect(helpAction, SIGNAL(triggered()), this, SLOT(about()));

    // used in fsTree and bookmarks
    pasteAction = new QAction(tr("Paste files"), this);
    pasteAction->setObjectName("paste");
    //    connect(pasteAction, SIGNAL(triggered()), this, SLOT(about()));


    // Possibly needed actions


    // Sort actions
    actName = new QAction(tr("Name"), this);
    actName->setObjectName("name");
    actTime = new QAction(tr("Time"), this);
    actTime->setObjectName("time");
    actSize = new QAction(tr("Size"), this);
    actSize->setObjectName("size");
    actType = new QAction(tr("Type"), this);
    actType->setObjectName("type");
    actName->setCheckable(true);
    actTime->setCheckable(true);
    actSize->setCheckable(true);
    actType->setCheckable(true);
    connect(actName, SIGNAL(triggered()), this, SLOT(sortThumbnails()));
    connect(actTime, SIGNAL(triggered()), this, SLOT(sortThumbnails()));
    connect(actSize, SIGNAL(triggered()), this, SLOT(sortThumbnails()));
    connect(actType, SIGNAL(triggered()), this, SLOT(sortThumbnails()));

    // rgh figure out what this does
    if (thumbView->thumbsSortFlags & QDir::Time)
            actTime->setChecked(true);
    else if (thumbView->thumbsSortFlags & QDir::Size)
            actSize->setChecked(true);
    else if (thumbView->thumbsSortFlags & QDir::Type)
            actType->setChecked(true);
    else
            actName->setChecked(true);
    reverseSortAction->setChecked(thumbView->thumbsSortFlags & QDir::Reversed);


}

void MW::createMenus()
{
    {
    #ifdef ISDEBUG
    qDebug() << "MW::createMenus";
    #endif
    }
    fileMenu = menuBar()->addMenu(tr("&File"));
    fileMenu->addAction(openAction);
    recentFoldersMenu = fileMenu->addMenu(tr("Recent folders"));
    openWithMenu = fileMenu->addMenu(tr("Open with..."));
    openWithMenu->addAction(newAppAction);
    openWithMenu->addAction(deleteAppAction);
    fileMenu->addSeparator();
    fileMenu->addAction(revealFileAction);
    fileMenu->addAction(subFoldersAction);
    fileMenu->addAction(addBookmarkAction);
    fileMenu->addSeparator();
    fileMenu->addAction(ingestAction);
    fileMenu->addAction(renameAction);
    fileMenu->addAction(runDropletAction);
    fileMenu->addSeparator();
    fileMenu->addAction(reportMetadataAction);
    fileMenu->addSeparator();
    fileMenu->addAction(exitAction);    // Appears in Winnow menu in OSX

    editMenu = menuBar()->addMenu(tr("&Edit"));
    editMenu->addAction(selectAllAction);
    editMenu->addAction(invertSelectionAction);
    editMenu->addSeparator();
    editMenu->addAction(togglePickAction);
    editMenu->addAction(toggleFilterPickAction);
    editMenu->addSeparator();
    editMenu->addAction(copyImagesAction);
    editMenu->addSeparator();
    editMenu->addAction(rotateRightAction);
    editMenu->addAction(rotateLeftAction);
    editMenu->addSeparator();
    editMenu->addAction(prefAction);       // Appears in Winnow menu in OSX
    editMenu->addAction(oldPrefAction);

    goMenu = menuBar()->addMenu(tr("&Go"));
    goMenu->addAction(nextThumbAction);
    goMenu->addAction(prevThumbAction);
    goMenu->addAction(upThumbAction);
    goMenu->addAction(downThumbAction);
    goMenu->addAction(firstThumbAction);
    goMenu->addAction(lastThumbAction);
    goMenu->addSeparator();
    goMenu->addAction(nextPickAction);
    goMenu->addAction(prevPickAction);

    viewMenu = menuBar()->addMenu(tr("&View"));
    viewMenu->addAction(slideShowAction);
    viewMenu->addAction(fullScreenAction);
    viewMenu->addAction(escapeFullScreenAction);
    viewMenu->addSeparator();
    viewMenu->addAction(zoomInAction);
    viewMenu->addAction(zoomOutAction);
    viewMenu->addAction(zoomOrigAction);
    viewMenu->addAction(zoomFitAction);

    viewMenu->addSeparator();
    viewMenu->addAction(infoVisibleAction);
    viewMenu->addSeparator();
    viewMenu->addAction(asListAction);
    viewMenu->addAction(asThumbsAction);
    viewMenu->addSeparator();
    viewMenu->addAction(thumbsEnlargeAction);
    viewMenu->addAction(thumbsShrinkAction);
//    viewMenu->addAction(thumbsFitAction);
    viewMenu->addAction(showThumbLabelsAction);
    viewMenu->addAction(reverseSortAction);

/*    sortMenu = viewMenu->addMenu(tr("Sort By"));
    sortTypesGroup = new QActionGroup(this);
    sortTypesGroup->addAction(actName);
    sortTypesGroup->addAction(actTime);
    sortTypesGroup->addAction(actSize);
    sortTypesGroup->addAction(actType);
    sortMenu->addActions(sortTypesGroup->actions());
    sortMenu->addSeparator();
    sortMenu->addAction(actReverse);
    */

    windowMenu = menuBar()->addMenu(tr("&Window"));
    workspaceMenu = windowMenu->addMenu(tr("&Workspace"));
    windowMenu->addSeparator();
    workspaceMenu->addAction(defaultWorkspaceAction);
    workspaceMenu->addAction(newWorkspaceAction);
    workspaceMenu->addAction(manageWorkspaceAction);
    workspaceMenu->addSeparator();
    // add 10 dummy menu items for custom workspaces
    for (int i=0; i<10; i++) {
        workspaceMenu->addAction(workspaceActions.at(i));
    }
    connect(workspaceMenu, SIGNAL(triggered(QAction*)),
            SLOT(invokeWorkspace(QAction*)));
    windowMenu->addAction(folderDockVisibleAction);
    windowMenu->addAction(favDockVisibleAction);
    windowMenu->addAction(metadataDockVisibleAction);
    windowMenu->addAction(thumbDockVisibleAction);
    windowMenu->addSeparator();
    windowMenu->addAction(windowsTitleBarVisibleAction);
    windowMenu->addAction(menuBarVisibleAction);
    windowMenu->addAction(statusBarVisibleAction);
    windowMenu->addSeparator();
    windowMenu->addAction(folderDockLockAction);
    windowMenu->addAction(favDockLockAction);
    windowMenu->addAction(metadataDockLockAction);
    windowMenu->addAction(thumbDockLockAction);
    windowMenu->addSeparator();
    windowMenu->addAction(allDocksLockAction);

    helpMenu = menuBar()->addMenu(tr("&Help"));
    helpMenu->addAction(aboutAction);
    helpMenu->addAction(helpAction);

    menuBar()->setVisible(true);

    // thumbview context menu
    thumbView->addAction(openAction);
    thumbView->addAction(openWithMenuAction);
    thumbView->addAction(selectAllAction);
    thumbView->addAction(invertSelectionAction);
    addMenuSeparator(thumbView);
    thumbView->addAction(reportMetadataAction);
    thumbView->setContextMenuPolicy(Qt::ActionsContextMenu);

    // imageview context menu
    imageView->addAction(togglePickAction);
    imageView->addAction(toggleFilterPickAction);
    imageView->addAction(ingestAction);
    addMenuSeparator(imageView);

    QMenu *goSubMenu = new QMenu(imageView);
    QAction *goGroupAct = new QAction("Go", this);
    imageView->addAction(goGroupAct);
    goGroupAct->setMenu(goSubMenu);
    goSubMenu->addAction(nextThumbAction);
    goSubMenu->addAction(prevThumbAction);
    goSubMenu->addAction(firstThumbAction);
    goSubMenu->addAction(lastThumbAction);
    goSubMenu->addAction(upThumbAction);
    goSubMenu->addAction(downThumbAction);
    goSubMenu->addAction(randomImageAction);
    goSubMenu->addAction(slideShowAction);


    QMenu *zoomSubMenu = new QMenu(imageView);
    QAction *zoomGroupAct = new QAction("Zoom", this);
    imageView->addAction(zoomGroupAct);
    zoomGroupAct->setMenu(zoomSubMenu);
    zoomSubMenu->addAction(zoomInAction);
    zoomSubMenu->addAction(zoomOutAction);
    zoomSubMenu->addAction(zoomOrigAction);
    zoomSubMenu->addAction(zoomFitAction);

    QMenu *transformSubMenu = new QMenu(imageView);
    QAction *transformGroupAct = new QAction("Rotate", this);
    imageView->addAction(transformGroupAct);
    transformGroupAct->setMenu(transformSubMenu);
    transformSubMenu->addAction(rotateRightAction);
    transformSubMenu->addAction(rotateLeftAction);

    QMenu *thumbNailSubMenu = new QMenu(imageView);
    QAction *thumbnailGroupAct = new QAction("ThumbNails", this);
    imageView->addAction(thumbnailGroupAct);
    thumbnailGroupAct->setMenu(thumbNailSubMenu);
    thumbNailSubMenu->addAction(thumbsEnlargeAction);
    thumbNailSubMenu->addAction(thumbsShrinkAction);
    thumbNailSubMenu->addAction(showThumbLabelsAction);
    sortMenu = thumbNailSubMenu->addMenu(tr("Sort By"));
    sortTypesGroup = new QActionGroup(this);
    sortTypesGroup->addAction(actName);
    sortTypesGroup->addAction(actTime);
    sortTypesGroup->addAction(actSize);
    sortTypesGroup->addAction(actType);
    sortMenu->addActions(sortTypesGroup->actions());
    sortMenu->addSeparator();
    sortMenu->addAction(reverseSortAction);

    addMenuSeparator(imageView);

//    imageView->addAction(copyImageAction);
//    imageView->addAction(pasteImageAction);
//    imageView->addAction(closeImageAct);
//    imageView->addAction(keepTransformAct);
//    imageView->addAction(keepZoomAct);
//    imageView->addAction(refreshAction);
//    imageView->addAction(copyToAction);
//    imageView->addAction(moveToAction);

    QMenu *workspaceSubMenu = new QMenu(imageView);
    QAction *workspaceGroupAct = new QAction("Workspace", this);
    imageView->addAction(workspaceGroupAct);
    workspaceGroupAct->setMenu(workspaceSubMenu);
    workspaceSubMenu->addAction(defaultWorkspaceAction);
    workspaceSubMenu->addAction(newWorkspaceAction);
    workspaceSubMenu->addAction(manageWorkspaceAction);
    workspaceSubMenu->addSeparator();
    // add 10 dummy menu items for custom workspaces
    for (int i=0; i<10; i++) {
        workspaceSubMenu->addAction(workspaceActions.at(i));
    }
    connect(workspaceSubMenu, SIGNAL(triggered(QAction*)),
            SLOT(invokeWorkspace(QAction*)));

    QMenu *windowSubMenu = new QMenu(imageView);
    QAction *windowGroupAct = new QAction("Window", this);
    imageView->addAction(windowGroupAct);
    windowGroupAct->setMenu(windowSubMenu);
    windowSubMenu->addAction(folderDockVisibleAction);
    windowSubMenu->addAction(favDockVisibleAction);
    windowSubMenu->addAction(metadataDockVisibleAction);
    windowSubMenu->addAction(thumbDockVisibleAction);
    addMenuSeparator(windowSubMenu);
    windowSubMenu->addAction(windowsTitleBarVisibleAction);
    windowSubMenu->addAction(menuBarVisibleAction);
    windowSubMenu->addAction(statusBarVisibleAction);
    addMenuSeparator(windowSubMenu);
    windowSubMenu->addAction(folderDockLockAction);
    windowSubMenu->addAction(favDockLockAction);
    windowSubMenu->addAction(metadataDockLockAction);
    windowSubMenu->addAction(thumbDockLockAction);
    addMenuSeparator(windowSubMenu);
    windowSubMenu->addAction(allDocksLockAction);

    QMenu *viewSubMenu = new QMenu(imageView);
    QAction *viewGroupAct = new QAction("View", this);
    imageView->addAction(viewGroupAct);
    viewGroupAct->setMenu(viewSubMenu);
    viewSubMenu->addAction(slideShowAction);
    viewSubMenu->addAction(fullScreenAction);
    viewSubMenu->addAction(escapeFullScreenAction);
    viewSubMenu->addSeparator();
    viewSubMenu->addAction(infoVisibleAction);
    viewSubMenu->addSeparator();
    viewSubMenu->addAction(asListAction);
    viewSubMenu->addAction(asThumbsAction);
    viewSubMenu->addSeparator();
    viewSubMenu->addAction(reverseSortAction);

    QMenu *fileSubMenu = new QMenu(imageView);
    QAction *fileGroupAct = new QAction("File", this);
    imageView->addAction(fileGroupAct);
    fileGroupAct->setMenu(fileSubMenu);
    fileSubMenu->addAction(openAction);
    recentFoldersMenu = fileSubMenu->addMenu(tr("Recent folders"));
    openWithMenu = fileSubMenu->addMenu(tr("Open with..."));
    openWithMenu->addAction(newAppAction);
    openWithMenu->addAction(deleteAppAction);
    fileSubMenu->addAction(revealFileAction);
    fileSubMenu->addAction(subFoldersAction);
    fileSubMenu->addAction(addBookmarkAction);
    fileSubMenu->addSeparator();
    fileSubMenu->addAction(ingestAction);
    fileSubMenu->addAction(renameAction);
    fileSubMenu->addAction(runDropletAction);
    fileSubMenu->addSeparator();
    fileSubMenu->addAction(reportMetadataAction);
    addMenuSeparator(imageView);

    imageView->addAction(prefAction);
    imageView->addAction(oldPrefAction);
    addMenuSeparator(imageView);
    imageView->addAction(exitAction);

    imageView->setContextMenuPolicy(Qt::ActionsContextMenu);
}

void MW::createThumbView()
{
    {
    #ifdef ISDEBUG
    qDebug() << "MW::createThumbView";
    #endif
    }
    metadata = new Metadata;
    thumbView = new ThumbView(this, metadata, true);
    thumbView->setObjectName("ImageView");  //rgh need to fix??
    thumbView->thumbSpacing = mwd.thumbSpacing;
    thumbView->thumbPadding = mwd.thumbPadding;
    thumbView->thumbWidth = mwd.thumbWidth;
    thumbView->thumbHeight = mwd.thumbHeight;
    thumbView->labelFontSize = mwd.labelFontSize;
    thumbView->showThumbLabels = mwd.showThumbLabels;
    metadataCacheThread = new MetadataCache(this, thumbView, metadata);
    thumbCacheThread = new ThumbCache(this, thumbView, metadata);
    infoView = new InfoView(this, metadata);
    thumbView->thumbsSortFlags = (QDir::SortFlags)G::setting->value("thumbsSortFlags").toInt();
    thumbView->thumbsSortFlags |= QDir::IgnoreCase;

    connect(metadataCacheThread, SIGNAL(loadThumbCache()),
            this, SLOT(loadThumbCache()));

    connect(metadataCacheThread, SIGNAL(loadImageCache()),
            this, SLOT(loadImageCache()));

    metadataDock = new QDockWidget(tr("  Metadata  "), this);
    metadataDock->setObjectName("Image Info");
    metadataDock->setWidget(infoView);
}

void MW::addMenuSeparator(QWidget *widget)
{
    QAction *separator = new QAction(this);
    separator->setSeparator(true);
    widget->addAction(separator);
}

void MW:: createImageView()
{
    {
    #ifdef ISDEBUG
    qDebug() << "MW::createImageView";
    #endif
    }
    imageCacheThread = new ImageCache(this, metadata);
    imageView = new ImageView(this, metadata, imageCacheThread, mwd.isImageInfoVisible);
//    connect(copyImageAction, SIGNAL(triggered()), imageView, SLOT(copyImage()));
//    connect(pasteImageAction, SIGNAL(triggered()), imageView, SLOT(pasteImage()));
    connect(metadataCacheThread, SIGNAL(updateIsRunning(bool)),
            this, SLOT(updateMetadataThreadRunStatus(bool)));
    connect(thumbCacheThread, SIGNAL(updateIsRunning(bool)),
            this, SLOT(updateThumbThreadRunStatus(bool)));
    connect(imageCacheThread, SIGNAL(updateIsRunning(bool)),
            this, SLOT(updateImageThreadRunStatus(bool)));
    connect(imageCacheThread, SIGNAL(showCacheStatus(const QImage, QString)),
            this, SLOT(showCacheStatus(const QImage, QString)));
    connect(imageView, SIGNAL(togglePick()), this, SLOT(togglePick()));
    connect(imageView, SIGNAL(updateStatus(QString)),
            this, SLOT(updateStatus(QString)));
    connect(thumbView, SIGNAL(thumbClick(float,float)), imageView, SLOT(thumbClick(float,float)));
}

void MW::createStatusBar()
{
    {
    #ifdef ISDEBUG
    qDebug() << "MW::createStatusBar";
    #endif
    }
    cacheLabel = new QLabel();
    QString cacheStatus = "Image cache status for current folder:\n";
    cacheStatus += "  • LightGray:\tbackground for all images in folder\n";
    cacheStatus += "  • DarkGray: \tto be cached\n";
    cacheStatus += "  • Green:    \tcached\n";
    cacheStatus += "  • Red:      \tcurrent image";
    cacheLabel->setToolTip(cacheStatus);
    cacheLabel->setToolTipDuration(100000);
    statusBar()->addPermanentWidget(cacheLabel);

    int runLabelWidth = 13;
    metadataThreadRunningLabel = new QLabel;
    QString mtrl = "Metadata cache thread running status";
    metadataThreadRunningLabel->setToolTip(mtrl);
    metadataThreadRunningLabel->setFixedWidth(runLabelWidth);
    updateMetadataThreadRunStatus(false);
    statusBar()->addPermanentWidget(metadataThreadRunningLabel);

    thumbThreadRunningLabel = new QLabel;
    QString ttrl = "Thumb cache thread running status";
    thumbThreadRunningLabel->setToolTip(ttrl);
    thumbThreadRunningLabel->setFixedWidth(runLabelWidth);
    updateThumbThreadRunStatus(false);
    statusBar()->addPermanentWidget(thumbThreadRunningLabel);

    imageThreadRunningLabel = new QLabel;
    statusBar()->addPermanentWidget(imageThreadRunningLabel);
    QString itrl = "Image cache thread running status";
    imageThreadRunningLabel->setToolTip(itrl);
    imageThreadRunningLabel->setFixedWidth(runLabelWidth);
    updateImageThreadRunStatus(false);

    stateLabel = new QLabel;
    statusBar()->addWidget(stateLabel);
}

void MW::setCacheParameters(int size, bool show, int width, int wtAhead)
{
    cacheSizeMB = size;
    isShowCacheStatus = show;
    cacheStatusWidth = width;
    cacheWtAhead = wtAhead;
}

void MW::updateStatus(QString s)
{
    QString status = "";
    QString spacer = "   ";

    // Possible status info
    QModelIndex idx = thumbView->currentIndex();
    QString fName = idx.data(Qt::EditRole).toString();
    QString fPath = idx.data(thumbView->FileNameRole).toString();
    QString shootingInfo = metadata->getShootingInfo(fPath);
    QString err = metadata->getErr(fPath);
    long rowCount = thumbView->thumbViewFilter->rowCount();
    QString fileCount = "";
    if (rowCount > 0) {
        int row = idx.row() + 1;
        fileCount = QString::number(row) + " of "
            + QString::number(rowCount);
    }
    QString magnify = "🔎";
//    QString fileSym = "📁";
    QString fileSym = "📷";

    status = " " + fileCount + spacer + " " + s + " zoom";
    stateLabel->setText(status);
}

void MW::updateMetadataThreadRunStatus(bool isRunning)
{
    if (isRunning)
        metadataThreadRunningLabel->setStyleSheet("QLabel {color:Green;}");
    else
        metadataThreadRunningLabel->setStyleSheet("QLabel {color:Gray;}");
    metadataThreadRunningLabel->setText("◉");
}

void MW::updateThumbThreadRunStatus(bool isRunning)
{
    if (isRunning)
        thumbThreadRunningLabel->setStyleSheet("QLabel {color:Green;}");
    else
        thumbThreadRunningLabel->setStyleSheet("QLabel {color:Gray;}");
    thumbThreadRunningLabel->setText("◉");
}

void MW::updateImageThreadRunStatus(bool isRunning)
{
    if (isRunning)
        imageThreadRunningLabel->setStyleSheet("QLabel {color:Green;}");
    else
        imageThreadRunningLabel->setStyleSheet("QLabel {color:Gray;}");
    imageThreadRunningLabel->setText("◉");
}

// used by ImageCache thread to show image cache building progress
void MW::showCacheStatus(const QImage &imCacheStatus, QString mbCacheSize)
{
    {
    #ifdef ISDEBUG
    qDebug() << "MW::showCacheStatus";
    #endif
    }
    cacheLabel->setVisible(true);
    cacheLabel->setPixmap(QPixmap::fromImage(imCacheStatus));
//    updateStatus(mbCacheSize);
}

void MW::createFSTree()
{
    {
    #ifdef ISDEBUG
    qDebug() << "MW::createFSTree";
    #endif
    }
    folderDock = new QDockWidget(tr("  Folders  "), this);
    folderDock->setObjectName("File System");

    fsTree = new FSTree(folderDock);
    folderDock->setWidget(fsTree);
    addDockWidget(Qt::LeftDockWidgetArea, folderDock);

    // Context menu
    fsTree->addAction(openAction);
    addMenuSeparator(fsTree);
    fsTree->addAction(pasteAction);
    addMenuSeparator(fsTree);
    fsTree->addAction(openWithMenuAction);
    fsTree->addAction(addBookmarkAction);
    fsTree->setContextMenuPolicy(Qt::ActionsContextMenu);

    connect(fsTree, SIGNAL(clicked(const QModelIndex&)), this, SLOT(folderSelectionChange()));

    connect(fsTree->fsModel, SIGNAL(rowsRemoved(const QModelIndex &, int, int)),
            this, SLOT(checkDirState(const QModelIndex &, int, int)));

    connect(fsTree, SIGNAL(dropOp(Qt::KeyboardModifiers, bool, QString)),
            this, SLOT(dropOp(Qt::KeyboardModifiers, bool, QString)));

//    fsTree->setCurrentIndex(fsTree->fsModel->index(QDir::currentPath())); // rgh 2017-04-04

//    connect(fsTree->selectionModel(), SIGNAL(selectionChanged(QItemSelection, QItemSelection)),
//            this, SLOT(updateActions()));
}

void MW::createBookmarks()
{
    {
    #ifdef ISDEBUG
    qDebug() << "MW::createBookmarks";
    #endif
    }
    favDock = new QDockWidget(tr("  Fav  "), this);
    favDock->setObjectName("Bookmarks");
    bookmarks = new BookMarks(favDock);
    favDock->setWidget(bookmarks);

    connect(bookmarks, SIGNAL(itemClicked(QTreeWidgetItem *, int)),
            this, SLOT(bookmarkClicked(QTreeWidgetItem *, int)));
    connect(removeBookmarkAction, SIGNAL(triggered()),
            bookmarks, SLOT(removeBookmark()));
    connect(bookmarks, SIGNAL(dropOp(Qt::KeyboardModifiers, bool, QString)),
            this, SLOT(dropOp(Qt::KeyboardModifiers, bool, QString)));
    addDockWidget(Qt::LeftDockWidgetArea, favDock);

    bookmarks->addAction(pasteAction);
    bookmarks->addAction(removeBookmarkAction);
    bookmarks->setContextMenuPolicy(Qt::ActionsContextMenu);
}

void MW::sortThumbnails()
{
    {
    #ifdef ISDEBUG
    qDebug() << "MW::sortThumbnails";
    #endif
    }
    thumbView->thumbsSortFlags = QDir::IgnoreCase;

    if (actName->isChecked())
        thumbView->thumbsSortFlags |= QDir::Name;
    else if (actTime->isChecked())
        thumbView->thumbsSortFlags |= QDir::Time;
    else if (actSize->isChecked())
        thumbView->thumbsSortFlags |= QDir::Size;
    else if (actType->isChecked())
        thumbView->thumbsSortFlags |= QDir::Type;

    if (reverseSortAction->isChecked())
        thumbView->thumbsSortFlags |= QDir::Reversed;

//    refreshThumbs(false);
}

void MW::setIncludeSubFolders() // rgh not req'd
{
    {
    #ifdef ISDEBUG
    qDebug() << "MW::setIncludeSubFolders";
    #endif
    }
//    G::includeSubFolders = subFoldersAction->isChecked();
}

void MW::showHiddenFiles()
{
    {
    #ifdef ISDEBUG
    qDebug() << "MW::showHiddenFiles";
    #endif
    }
    fsTree->setModelFlags();
}

void MW::setThumbLabels()   // move to thumbView
{
    {
    #ifdef ISDEBUG
    qDebug() << "MW::showLabels";
    #endif
    }
//    G::showThumbLabels = showThumbLabelsAction->isChecked();
}

/* WORKSPACES
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
    {
    #ifdef ISDEBUG
    qDebug() << "MW::newWorkspace";
    #endif
    }
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
    {
    #ifdef ISDEBUG
    qDebug() << "MW::fixDupWorkspaceName";
    #endif
    }
    for (int i=0; i<workspaces->count(); i++) {
        if (workspaces->at(i).name == name) {
            name += "_1";
            fixDupWorkspaceName(name);
        }
    }
    return name;
}

void MW::invokeWorkspace(QAction *workAction)
{
/*
invokeWorkspace is called from a workspace action. Since the workspace actions
are a list of actions, the workspaceMenu triggered signal is captured, and the
workspace with a matching name to the action is used.
*/
    {
    #ifdef ISDEBUG
    qDebug() << "MW::invokeWorkspace";
    #endif
    }
    qDebug() << "Invoke" << workAction;     // rgh remove when debugged
    for (int i=0; i<workspaces->count(); i++) {
        if (workspaces->at(i).name == workAction->text()) {
            ws = workspaces->at(i);
            restoreGeometry(ws.geometry);
            restoreState(ws.state);
            windowsTitleBarVisibleAction->setChecked(ws.isWindowTitleBarVisible);
            menuBarVisibleAction->setChecked(ws.isMenuBarVisible);
            statusBarVisibleAction->setChecked(ws.isStatusBarVisible);
            folderDockVisibleAction->setChecked(ws.isFolderDockVisible);
            favDockVisibleAction->setChecked(ws.isFavDockVisible);
            metadataDockVisibleAction->setChecked(ws.isMetadataDockVisible);
            thumbDockVisibleAction->setChecked(ws.isThumbDockVisible);
            folderDockLockAction->setChecked(ws.isFolderDockLocked);
            favDockLockAction->setChecked(ws.isFavDockLocked);
            metadataDockLockAction->setChecked(ws.isMetadataDockLocked);
            thumbDockLockAction->setChecked( ws.isThumbDockLocked);
            infoVisibleAction->setChecked(ws.isImageInfoVisible);
            updateState();
            thumbView->thumbSpacing = ws.thumbSpacing;
            thumbView->thumbPadding = ws.thumbPadding;
            thumbView->thumbWidth = ws.thumbWidth;
            thumbView->thumbHeight = ws.thumbHeight;
            thumbView->labelFontSize = ws.labelFontSize;
            thumbView->showThumbLabels = ws.showThumbLabels;
            thumbView->refreshThumbs();
            reportWorkspace(i);         // rgh remove after debugging
        }
    }
}

void MW::manageWorkspaces()
{
/*
Delete, rename and reassign workspaces.
 */
    {
    #ifdef ISDEBUG
    qDebug() << "MW::manageWorkspaces";
    #endif
    }
    // rgh remove after debugging
    foreach(QAction *act, workspaceMenu->actions()) {
        qDebug() << act->objectName() << act;
    }
    // build a list of workspace names for the manager dialog
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
    workspaceDlg->exec();
    delete workspaceDlg;
}

void MW::deleteWorkspace(int n)
{
    {
    #ifdef ISDEBUG
    qDebug() << "MW::deleteWorkspace";
    #endif
    }
    if (workspaces->count() < 1) return;

    // remove workspace from list of workspaces
    workspaces->removeAt(n);

    // sync menus by rebuilding.  Tried to use indexes but had problems so
    // resorted to brute force solution
    int count = workspaces->count();
    for (int i=0; i<10; i++) {
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
    {
    #ifdef ISDEBUG
    qDebug() << "MW::reassignWorkspace";
    #endif
    }
    QString name = workspaces->at(n).name;
    populateWorkspace(n, name);
}

void MW::defaultWorkspace()
{
/*
The defaultWorkspace is used the first time the app is run on a new machine and
there are not any QSettings to read.  It is also useful if part or all of the
app is "stranded" on secondary monitors that are not attached.
 */
    {
    #ifdef ISDEBUG
    qDebug() << "MW::defaultWorkspace";
    #endif
    }
    QRect desktop = qApp->desktop()->availableGeometry();
    resize(0.75 * desktop.width(), 0.75 * desktop.height());
    setGeometry( QStyle::alignedRect(Qt::LeftToRight, Qt::AlignCenter,
        size(), desktop));
    windowsTitleBarVisibleAction->setChecked(true);
    menuBarVisibleAction->setChecked(true);
    statusBarVisibleAction->setChecked(true);

    folderDockVisibleAction->setChecked(true);
    favDockVisibleAction->setChecked(true);
    metadataDockVisibleAction->setChecked(true);
    thumbDockVisibleAction->setChecked(true);

    folderDockLockAction->setChecked(false);
    favDockLockAction->setChecked(false);
    metadataDockLockAction->setChecked(false);
    thumbDockLockAction->setChecked(false);

    // sync app state with menu checked status
    updateState();

//    thumbDock->setFeatures(QDockWidget::DockWidgetVerticalTitleBar |
//                           QDockWidget::DockWidgetMovable);
    folderDock->setFloating(false);
    favDock->setFloating(false);
    metadataDock->setFloating(false);
    thumbDock->setFloating(false);

    addDockWidget(Qt::LeftDockWidgetArea, folderDock, Qt::Vertical);
    addDockWidget(Qt::LeftDockWidgetArea, favDock, Qt::Vertical);
    addDockWidget(Qt::LeftDockWidgetArea, metadataDock, Qt::Vertical);
    addDockWidget(Qt::BottomDockWidgetArea, thumbDock, Qt::Vertical);

    resizeDocks({thumbDock}, {160}, Qt::Vertical);

    thumbView->thumbSpacing = 0;
    thumbView->thumbPadding = 0;
    thumbView->thumbWidth = 120;
    thumbView->thumbHeight = 120;
    thumbView->labelFontSize = 8;
    thumbView->showThumbLabels = true;
}

void MW::renameWorkspace(int n, QString name)
{
    {
    #ifdef ISDEBUG
    qDebug() << "MW::renameWorkspace";
    #endif
    }
    qDebug() << "MW::renameWorkspace";
    if (workspaces->count() > 0) {
        (*workspaces)[n].name = name;
        qDebug() << "Rename workspace" << n << workspaces->at(n).name;
        workspaceActions.at(n)->setText(name);
    }
}

void MW::populateWorkspace(int n, QString name)
{
    (*workspaces)[n].accelNum = QString::number(n);
    (*workspaces)[n].name = name;
    (*workspaces)[n].geometry = saveGeometry();
    (*workspaces)[n].state = saveState();
    (*workspaces)[n].isWindowTitleBarVisible = windowsTitleBarVisibleAction->isChecked();
    (*workspaces)[n].isMenuBarVisible = menuBarVisibleAction->isChecked();
    (*workspaces)[n].isStatusBarVisible = statusBarVisibleAction->isChecked();
    (*workspaces)[n].isFolderDockVisible = folderDockVisibleAction->isChecked();
    (*workspaces)[n].isFavDockVisible = favDockVisibleAction->isChecked();
    (*workspaces)[n].isMetadataDockVisible = metadataDockVisibleAction->isChecked();
    (*workspaces)[n].isThumbDockVisible = thumbDockVisibleAction->isChecked();
    (*workspaces)[n].isFolderDockLocked = folderDockLockAction->isChecked();
    (*workspaces)[n].isFavDockLocked = favDockLockAction->isChecked();
    (*workspaces)[n].isMetadataDockLocked = metadataDockLockAction->isChecked();
    (*workspaces)[n].isThumbDockLocked = thumbDockLockAction;
    (*workspaces)[n].thumbSpacing = thumbView->thumbSpacing;
    (*workspaces)[n].thumbPadding = thumbView->thumbPadding;
    (*workspaces)[n].thumbWidth = thumbView->thumbWidth;
    (*workspaces)[n].thumbHeight = thumbView->thumbHeight;
    (*workspaces)[n].labelFontSize = thumbView->labelFontSize;
    (*workspaces)[n].showThumbLabels = thumbView->showThumbLabels;
    (*workspaces)[n].isImageInfoVisible = infoVisibleAction->isChecked();
}

void MW::reportWorkspace(int n)
{
    ws = workspaces->at(n);
    qDebug() << "\n\nName" << ws.name
             << "\nAccel#" << ws.accelNum
             << "\nGeometry" << ws.geometry
             << "\nState" << ws.state
             << "\nisWindowTitleBarVisible" << ws.isWindowTitleBarVisible
             << "\nisMenuBarVisible" << ws.isMenuBarVisible
             << "\nisStatusBarVisible" << ws.isStatusBarVisible
             << "\nisFolderDockVisible" << ws.isFolderDockVisible
             << "\nisFavDockVisible" << ws.isFavDockVisible
             << "\nisMetadataDockVisible" << ws.isMetadataDockVisible
             << "\nisThumbDockVisible" << ws.isThumbDockVisible
             << "\nisFolderLocked" << ws.isFolderDockLocked
             << "\nisFavLocked" << ws.isFavDockLocked
             << "\nisMetadataLocked" << ws.isMetadataDockLocked
             << "\nisThumbsLocked" << ws.isThumbDockLocked
             << "\nthumbSpacing" << ws.thumbSpacing
             << "\nthumbPadding" << ws.thumbPadding
             << "\nthumbWidth" << ws.thumbWidth
             << "\nthumbHeight" << ws.thumbHeight
             << "\nlabelFontSize" << ws.labelFontSize
             << "\nshowThumbLabels" << ws.showThumbLabels
             << "\nshowShootingInfo" << ws.isImageInfoVisible;
}


void MW::reportMetadata()
{
    {
    #ifdef ISDEBUG
    qDebug() << "MW::reportMetadata";
    #endif
    }
//    QModelIndexList indexesList = thumbView->selectionModel()->selectedIndexes();
//    const QString imagePath = indexesList.first().data(thumbView->FileNameRole).toString();

    QString imagePath = thumbView->currentIndex().data(thumbView->FileNameRole).toString();
    metadata->readMetadata(true, imagePath);

//    QString hdr = "Test header";
//    std::stringstream os;
//    os << "test" << endl;
//    os << "\n*******************************\n"
//              << hdr.toStdString() << "\n"
//              << "*******************************\n";
//    os << "\n  Offset  tagId  tagType  tagCount  tagValue   tagDescription\n";
//    QString txt = QString::fromStdString(os.str());

//    QMessageBox msg;
//    msg.setText(txt);
//   msg.exec();

}

void MW::about()
{
    {
    #ifdef ISDEBUG
    qDebug() << "MW::about";
    #endif
    }
    QString aboutString = "<h2>Winnow v1.0</h2>"
        + tr("<p>Image viewer and ingester</p>")
        + "Qt v" + QT_VERSION_STR
        + "<p></p>"
        + "<p>Author: Rory Hill."
        + "<p>Some code is based on Phototonic by Ofer Kashayov."
        + "<p>Winnow is licensed under the GNU General Public License version 3</p>";

    QMessageBox::about(this, tr("About") + " Winnow", aboutString);
}

void MW::cleanupSender()
{
    {
    #ifdef ISDEBUG
    qDebug() << "MW::cleanupSender";
    #endif
    }
    delete QObject::sender();
}

void MW::externalAppError()
{
    {
    #ifdef ISDEBUG
    qDebug() << "MW::externalAppError";
    #endif
    }
    QMessageBox msgBox;
    msgBox.critical(this, tr("Error"), tr("Failed to start external application."));
}

// rgh requires tweaking ??
void MW::runExternalApp()
{
    {
    #ifdef ISDEBUG
    qDebug() << "MW::runExternalApp";
    #endif
    }
    QString execCommand;
    QString selectedFileNames("");
    execCommand = G::externalApps[((QAction*) sender())->text()];



//        Change imageView->currentImagePath to use treeView->currentIndex()...
//        execCommand += " \"" + imageView->currentImagePath + "\"";
        if (QApplication::focusWidget() == fsTree) {
            selectedFileNames += " \"" + getSelectedPath() + "\"";
        } else {

            QModelIndexList selectedIdxList = thumbView->selectionModel()->selectedIndexes();
            if (selectedIdxList.size() < 1)
            {
                setStatus(tr("Invalid selection."));
                return;
            }

            selectedFileNames += " ";
            for (int tn = selectedIdxList.size() - 1; tn >= 0 ; --tn)
            {
                selectedFileNames += "\"" +
                    thumbView->thumbViewModel->item(selectedIdxList[tn].row())->data(thumbView->FileNameRole).toString();
                if (tn)
                    selectedFileNames += "\" ";
            }
        }

        execCommand += selectedFileNames;
        qDebug() << "MW::cleanupSender  " << execCommand;

    QProcess *externalProcess = new QProcess();
    connect(externalProcess, SIGNAL(finished(int, QProcess::ExitStatus)), this, SLOT(cleanupSender()));
    connect(externalProcess, SIGNAL(error(QProcess::ProcessError)), this, SLOT(externalAppError()));
    externalProcess->start(execCommand);
}

void MW::updateExternalApps()
{
    {
    #ifdef ISDEBUG
    qDebug() << "MW::updateExternalApps";
    #endif
    }
    int actionNum = 0;
    QMapIterator<QString, QString> eaIter(G::externalApps);

    QList<QAction*> actionList = openWithMenu->actions();
    if (!actionList.empty()) {

        for (int i = 0; i < actionList.size(); ++i)
        {
            QAction *action = actionList.at(i);
            if (action->isSeparator())
                break;
            openWithMenu->removeAction(action);
            imageView->removeAction(action);
            delete action;
        }

        openWithMenu->clear();
    }

    while (eaIter.hasNext())
    {
        ++actionNum;
        eaIter.next();
        QAction *extAppAct = new QAction(eaIter.key(), this);
        if (actionNum < 10)
            extAppAct->setShortcut(QKeySequence("Alt+" + QString::number(actionNum)));
        extAppAct->setIcon(QIcon::fromTheme(eaIter.key()));
        connect(extAppAct, SIGNAL(triggered()), this, SLOT(runExternalApp()));
        openWithMenu->addAction(extAppAct);
        imageView->addAction(extAppAct);
    }

    openWithMenu->addSeparator();
    openWithMenu->addAction(chooseAppAction);
}

void MW::chooseExternalApp()
{
    {
    #ifdef ISDEBUG
    qDebug() << "MW::chooseExternalApp";
    #endif
    }
    AppMgmtDialog *dialog = new AppMgmtDialog(this);

    if (G::slideShowActive)
        slideShow();
    imageView->setCursorHiding(false);

    dialog->exec();
    updateExternalApps();
    delete(dialog);

//    if (isFullScreen()) {
//        imageView->setCursorHiding(true);
//    }
}

void MW::preferences()
{
/*

*/
    {
    #ifdef ISDEBUG
    qDebug() << "MW::preferences";
    #endif
    }
    Prefdlg *prefdlg = new Prefdlg(this);
    connect(prefdlg, SIGNAL(updateThumbParameters(int,int,int,int,int,bool)),
        thumbView, SLOT(setThumbParameters(int, int, int, int, int, bool)));
    connect(prefdlg, SIGNAL(updateSlideShowParameters(int,bool)),
            this, SLOT(setSlideShowParameters(int,bool)));
    connect(prefdlg, SIGNAL(updateCacheParameters(int,bool,int,int)),
            this, SLOT(setCacheParameters(int,bool,int,int)));
    prefdlg->exec();
}

void MW::oldPreferences()
{
    SettingsDialog *dialog = new SettingsDialog(this);
    dialog->exec();
    delete dialog;
}

void MW::escapeFullScreen()
{
    {
    #ifdef ISDEBUG
    qDebug() << "MW::escapeFullScreen";
    #endif
    }
    qDebug() << "MW::escapeFullScreen";
    fullScreenAction->setChecked(false);
    toggleFullScreen();
}

// rgh maybe separate this into two functions:
// 1. toggle show only imageview
// 2. toggle full screen
// or just use workspaces and toggle full screen
void MW::toggleFullScreen()
{
    {
    #ifdef ISDEBUG
    qDebug() << "MW::toggleFullScreen";
    #endif
    }
    qDebug() << "test";
    if (fullScreenAction->isChecked())
    {
        shouldMaximize = isMaximized();
        showFullScreen();
        imageView->setCursorHiding(true);
        folderDockVisibleAction->setChecked(false);       setFolderDockVisibility();
        folderDockVisibleAction->setChecked(false);       setFolderDockVisibility();
        favDockVisibleAction->setChecked(false);        setFavDockVisibility();
        metadataDockVisibleAction->setChecked(false);    setMetadataDockVisibility();
        menuBarVisibleAction->setChecked(false);     setMenuBarVisibility();
        statusBarVisibleAction->setChecked(false);   setStatusBarVisibility();
        allDocksLockAction->setChecked(true);    setAllDocksLockMode();
    }
    else
    {
        showNormal();
        if (shouldMaximize) showMaximized();
        imageView->setCursorHiding(false);
        folderDockVisibleAction->setChecked(true);       setFolderDockVisibility();
        folderDockVisibleAction->setChecked(true);       setFolderDockVisibility();
        favDockVisibleAction->setChecked(true);        setFavDockVisibility();
        metadataDockVisibleAction->setChecked(true);    setMetadataDockVisibility();
        menuBarVisibleAction->setChecked(true);     setMenuBarVisibility();
        statusBarVisibleAction->setChecked(true);   setStatusBarVisibility();
        allDocksLockAction->setChecked(false);  setAllDocksLockMode();
    }
}

void MW::selectAllThumbs()
{
    {
    #ifdef ISDEBUG
    qDebug() << "MW::selectAllThumbs";
    #endif
    }
    thumbView->selectAll();
}


void MW::zoomOut()
{
    {
    #ifdef ISDEBUG
    qDebug() << "MW::zoomOut";
    #endif
    }
    imageView->zoomOut();
}

void MW::zoomIn()
{
    {
    #ifdef ISDEBUG
    #endif
    }
    qDebug() << "MW::zoomIn";
    imageView->zoomIn();
}

void MW::zoomToFit()
// zooms to fit full image in viewImage window
{
    {
    #ifdef ISDEBUG
    qDebug() << "MW::resetZoom";
    #endif
    }
    imageView->zoomToFit();
}

void MW::zoom100()
// zooms imageView so one image pixel = 1 screen pixel
{
    {
    #ifdef ISDEBUG
    qDebug() << "MW::origZoom";
    #endif
    }
    imageView->zoom100();
}

void MW::rotateLeft()
{
    {
    #ifdef ISDEBUG
    qDebug() << "MW::rotateLeft";
    #endif
    }
//    GData::rotation -= 90;
//    if (GData::rotation < 0)
//        GData::rotation = 270;
//    imageView->refresh();
//    imageView->setFeedback(tr("Rotation %1°").arg(QString::number(GData::rotation)));
}

void MW::rotateRight()
{
    {
    #ifdef ISDEBUG
    qDebug() << "MW::rotateRight";
    #endif
    }
//    GData::rotation += 90;
//    if (GData::rotation > 270)
//        GData::rotation = 0;
//    imageView->refresh();
//    imageView->setFeedback(tr("Rotation %1°").arg(QString::number(GData::rotation)));
}

bool MW::isValidPath(QString &path)
{
    {
    #ifdef ISDEBUG
    qDebug() << "MW::isValidPath";
    #endif
    }
    QDir checkPath(path);
    if (!checkPath.exists() || !checkPath.isReadable()) {
        return false;
    }
    return true;
}

void MW::removeBookmark()
{

    {
    #ifdef ISDEBUG
    qDebug() << "MW::deleteOp";
    #endif
    }
//    if (QApplication::focusWidget() == thumbView->imageTags->tagsTree) {
//        thumbView->imageTags->removeTag();
//        return;
//    }

    if (QApplication::focusWidget() == bookmarks) {
        bookmarks->removeBookmark();
        return;
    }
}

void MW::setThumbsFilter()
{
    {
    #ifdef ISDEBUG
    qDebug() << "MW::setThumbspick";
    #endif
    }
    thumbView->filterStr = filterBar->text();
//    refreshThumbs(true);
}

void MW::clearThumbsFilter()
{
    {
    #ifdef ISDEBUG
    qDebug() << "MW::clearThumbspick";
    #endif
    }
    if (filterBar->text() == "")
    {
        thumbView->filterStr = filterBar->text();
//        refreshThumbs(true);
    }
}


/*****************************************************************************************
 * READ/WRITE PREFERENCES
 * **************************************************************************************/

void MW::writeSettings()
{
    {
    #ifdef ISDEBUG
    qDebug() << "MW::writeSettings";
    #endif
    }
    G::setting->setValue("Geometry", saveGeometry());
    G::setting->setValue("WindowState", saveState());
    // files
//    G::setting->setValue("showHiddenFiles", (bool)G::showHiddenFiles);
    G::setting->setValue("rememberLastDir", rememberLastDir);
    G::setting->setValue("lastDir", lastDir);
    G::setting->setValue("includeSubfolders", subFoldersAction->isChecked());
    // thumbs
    G::setting->setValue("thumbSpacing", thumbView->thumbSpacing);
    G::setting->setValue("thumbPadding", thumbView->thumbPadding);
    G::setting->setValue("thumbWidth", thumbView->thumbWidth);
    G::setting->setValue("thumbHeight", thumbView->thumbHeight);
    G::setting->setValue("labelFontSize", thumbView->labelFontSize);
    G::setting->setValue("showLabels", (bool)showThumbLabelsAction->isChecked());
    // slideshow
    G::setting->setValue("slideShowDelay", (int)slideShowDelay);
    G::setting->setValue("slideShowRandom", (bool)slideShowRandom);
    // cache
    G::setting->setValue("cacheSizeMB", (int)cacheSizeMB);
    G::setting->setValue("isShowCacheStatus", (bool)isShowCacheStatus);
    G::setting->setValue("cacheStatusWidth", (int)cacheStatusWidth);
    G::setting->setValue("cacheWtAhead", (int)cacheWtAhead);
    // state
    G::setting->setValue("isWindowTitleBarVisible", (bool)windowsTitleBarVisibleAction->isChecked());
    G::setting->setValue("isMenuBarVisible", (bool)menuBarVisibleAction->isChecked());
    G::setting->setValue("isStatusBarVisible", (bool)statusBarVisibleAction->isChecked());
    G::setting->setValue("isFolderDockVisible", (bool)folderDockVisibleAction->isChecked());
    G::setting->setValue("isFavDockVisible", (bool)favDockVisibleAction->isChecked());
    G::setting->setValue("isMetadaDockVisible", (bool)metadataDockVisibleAction->isChecked());
    G::setting->setValue("isThumbDockVisible", (bool)thumbDockVisibleAction->isChecked());
    G::setting->setValue("isFolderDockLocked", (bool)folderDockLockAction->isChecked());
    G::setting->setValue("isFavDockLocked", (bool)favDockLockAction->isChecked());
    G::setting->setValue("isMetadaDockLocked", (bool)metadataDockLockAction->isChecked());
    G::setting->setValue("isThumbDockLocked", (bool)thumbDockLockAction->isChecked());
//    GData::setting->setValue("LockDocks", (bool)GData::isLockAllDocks);
    G::setting->setValue("isImageInfoVisible", (bool)infoVisibleAction->isChecked());

    // not req'd
    G::setting->setValue("shouldMaximize", (bool)isMaximized());
    G::setting->setValue("thumbsSortFlags", (int)thumbView->thumbsSortFlags);
    G::setting->setValue("thumbsZoomVal", (int)thumbView->thumbSize);

    /* Action shortcuts */
//    GData::appSettings->beginGroup("Shortcuts");
//    QMapIterator<QString, QAction *> scIter(GData::actionKeys);
//    while (scIter.hasNext()) {
//        scIter.next();
//        GData::appSettings->setValue(scIter.key(), scIter.value()->shortcut().toString());
//    }
//    GData::appSettings->endGroup();

    /* External apps */
    G::setting->beginGroup("ExternalApps");
    G::setting->remove("");
    QMapIterator<QString, QString> eaIter(G::externalApps);
    while (eaIter.hasNext()) {
        eaIter.next();
        G::setting->setValue(eaIter.key(), eaIter.value());
    }
    G::setting->endGroup();

    /* save bookmarks */
    int idx = 0;
    G::setting->beginGroup("CopyMoveToPaths");
    G::setting->remove("");
    QSetIterator<QString> pathsIter(G::bookmarkPaths);
    while (pathsIter.hasNext()) {
        G::setting->setValue("path" + QString::number(++idx), pathsIter.next());
    }
    G::setting->endGroup();

    // save workspaces
    G::setting->beginWriteArray("workspaces");
    for (int i = 0; i < workspaces->count(); ++i) {
        ws = workspaces->at(i);
        G::setting->setArrayIndex(i);
        G::setting->setValue("accelNum", ws.accelNum);
        G::setting->setValue("name", ws.name);
        G::setting->setValue("geometry", ws.geometry);
        G::setting->setValue("state", ws.state);
        G::setting->setValue("isWindowTitleBarVisible", ws.isWindowTitleBarVisible);
        G::setting->setValue("isMenuBarBarVisible", ws.isMenuBarVisible);
        G::setting->setValue("isStatusBarVisible", ws.isStatusBarVisible);
        G::setting->setValue("isFolderDockVisible", ws.isFolderDockVisible);
        G::setting->setValue("isFavDockVisible", ws.isFavDockVisible);
        G::setting->setValue("isMetadataDockVisible", ws.isMetadataDockVisible);
        G::setting->setValue("isThumbDockVisible", ws.isThumbDockVisible);
        G::setting->setValue("isFolderDockLocked", ws.isFolderDockLocked);
        G::setting->setValue("isFavDockLocked", ws.isFavDockLocked);
        G::setting->setValue("isMetadataDockLocked", ws.isMetadataDockLocked);
        G::setting->setValue("isThumbDockLocked", ws.isWindowTitleBarVisible);
        G::setting->setValue("thumbSpacing", ws.thumbSpacing);
        G::setting->setValue("thumbPadding", ws.thumbPadding);
        G::setting->setValue("thumbWidth", ws.thumbWidth);
        G::setting->setValue("thumbHeight", ws.thumbHeight);
        G::setting->setValue("labelFontSize", ws.labelFontSize);
        G::setting->setValue("showThumbLabels", ws.showThumbLabels);
        G::setting->setValue("isImageInfoVisible", ws.isImageInfoVisible);
    }
    G::setting->endArray();
}

void MW::loadSettings()
{
    {
    #ifdef ISDEBUG
    qDebug() << "MW::loadSettings";
    #endif
    }
    initComplete = false;
    needThumbsRefresh = false;

    // default values for first time use
    if (!G::setting->contains("cacheSizeMB")) {
        resize(800, 600);
        G::setting->setValue("thumbsSortFlags", (int)0);
        G::setting->setValue("thumbsZoomVal", (int)150);
        // thumbs
        G::setting->setValue("thumbSpacing", (int)10);
        G::setting->setValue("thumbPadding", (int)6);
        G::setting->setValue("thumbWidth", (int)160);
        G::setting->setValue("thumbHeight", (int)120);
        G::setting->setValue("labelFontSize", (int)12);
        G::setting->setValue("showThumbLabels", (bool)true);
        // slideshow
        G::setting->setValue("slideShowDelay", (int)5);
        G::setting->setValue("slideShowRandom", (bool)false);
        // cache
        G::setting->setValue("cacheSizeMB", (int)1000);
        G::setting->setValue("showCacheStatus", (bool)true);
        G::setting->setValue("cacheStatusWidth", (int)200);
        G::setting->setValue("cacheWtAhead", (int)5);
        // state
        G::setting->setValue("showHiddenFiles", (bool)false);
        G::setting->setValue("fsDockVisible", (bool)true);
        G::setting->setValue("bmDockVisible", (bool)true);
        G::setting->setValue("iiDockVisible", (bool)true);
        G::setting->setValue("pvDockVisible", (bool)true);
        G::setting->setValue("isImageInfoVisible", (bool)true);
        G::setting->setValue("fsDockLocked", (bool)false);
        G::setting->setValue("bmDockLocked", (bool)false);
        G::setting->setValue("iiDockLocked", (bool)false);
        G::setting->setValue("thumbDockLocked", (bool)false);
        G::setting->setValue("LockDocks", (bool)false);
        G::bookmarkPaths.insert(QDir::homePath());
    }

    // files
//    G::showHiddenFiles = G::setting->value("showHiddenFiles").toBool();
    rememberLastDir = G::setting->value("rememberLastDir").toBool();
    lastDir = G::setting->value("lastDir").toString();
    // thumbs
    mwd.thumbSpacing = G::setting->value("thumbSpacing").toInt();
    mwd.thumbPadding = G::setting->value("thumbPadding").toInt();
    mwd.thumbWidth = G::setting->value("thumbWidth").toInt();
    mwd.thumbHeight = G::setting->value("thumbHeight").toInt();
    mwd.labelFontSize = G::setting->value("labelFontSize").toInt();
    mwd.showThumbLabels = G::setting->value("showThumbLabels").toBool();
    // slideshow
    slideShowDelay = G::setting->value("slideShowDelay").toInt();
    slideShowRandom = G::setting->value("slideShowRandom").toBool();
    // cache
    cacheSizeMB = G::setting->value("cacheSizeMB").toInt();
    isShowCacheStatus = G::setting->value("isShowCacheStatus").toBool();
    cacheStatusWidth = G::setting->value("cacheStatusWidth").toInt();
    cacheWtAhead = G::setting->value("cacheWtAhead").toInt();

    // load state
    shouldMaximize = G::setting->value("shouldMaximize").toBool();  // req'd? Only in MW

    mwd.isWindowTitleBarVisible = G::setting->value("isWindowTitleBarVisible").toBool();
    mwd.isMenuBarVisible = G::setting->value("isMenuBarBarVisible").toBool();
    mwd.isStatusBarVisible = G::setting->value("isStatusBarVisible").toBool();

    mwd.isFolderDockVisible = G::setting->value("isFolderDockVisible").toBool();
    mwd.isFavDockVisible = G::setting->value("isFavDockVisible").toBool();
    mwd.isMetadataDockVisible = G::setting->value("isMetadataDockVisible").toBool();
    mwd.isThumbDockVisible = G::setting->value("isThumbDockVisible").toBool();

    mwd.isFolderDockLocked = G::setting->value("isFolderDockLocked").toBool();
    mwd.isFavDockLocked = G::setting->value("isFavDockLocked").toBool();
    mwd.isMetadataDockLocked = G::setting->value("isMetadataDockLocked").toBool();
    mwd.isThumbDockLocked = G::setting->value("isThumbDockLocked").toBool();

    mwd.includeSubfolders = G::setting->value("includeSubfolders").toBool();
    mwd.isImageInfoVisible = G::setting->value("isImageInfoVisible").toBool();

    /* read external apps */
    G::setting->beginGroup("ExternalApps");
    QStringList extApps = G::setting->childKeys();
    for (int i = 0; i < extApps.size(); ++i) {
        G::externalApps[extApps.at(i)] = G::setting->value(extApps.at(i)).toString();
    }
    G::setting->endGroup();

    /* read bookmarks */
    G::setting->beginGroup("CopyMoveToPaths");
    QStringList paths = G::setting->childKeys();
    for (int i = 0; i < paths.size(); ++i) {
        G::bookmarkPaths.insert(G::setting->value(paths.at(i)).toString());
    }
    G::setting->endGroup();

    /* read workspaces */
    int size = G::setting->beginReadArray("workspaces");
    for (int i = 0; i < size; ++i) {
        G::setting->setArrayIndex(i);
        ws.accelNum = G::setting->value("accelNum").toString();
        ws.name = G::setting->value("name").toString();
        ws.geometry = G::setting->value("geometry").toByteArray();
        ws.state = G::setting->value("state").toByteArray();
        ws.isWindowTitleBarVisible = G::setting->value("isWindowTitleBarVisible").toBool();
        ws.isMenuBarVisible = G::setting->value("isMenuBarVisible").toBool();
        ws.isStatusBarVisible = G::setting->value("isStatusBarVisible").toBool();
        ws.isFolderDockVisible = G::setting->value("isFolderDockVisible").toBool();
        ws.isFavDockVisible = G::setting->value("isFavDockVisible").toBool();
        ws.isMetadataDockVisible = G::setting->value("isMetadaDockVisible").toBool();
        ws.isThumbDockVisible = G::setting->value("isThumbDockVisible").toBool();
        ws.isFolderDockLocked = G::setting->value("isFolderDockLocked").toBool();
        ws.isFavDockLocked = G::setting->value("isFavDockLocked").toBool();
        ws.isMetadataDockLocked = G::setting->value("isMetadaDockLocked").toBool();
        ws.isThumbDockLocked = G::setting->value("isThumbDockLocked").toBool();
        ws.thumbSpacing = G::setting->value("thumbSpacing").toInt();
        ws.thumbPadding = G::setting->value("thumbPadding").toInt();
        ws.thumbWidth = G::setting->value("thumbWidth").toInt();
        ws.thumbHeight = G::setting->value("thumbHeight").toInt();
        ws.labelFontSize = G::setting->value("labelFontSize").toInt();
        ws.showThumbLabels = G::setting->value("showThumbLabels").toBool();
        ws.isImageInfoVisible = G::setting->value("isImageInfoVisible").toBool();
        workspaces->append(ws);
    }
    G::setting->endArray();
}

void MW::loadShortcuts(bool defaultShortcuts)
{
    {
    #ifdef ISDEBUG
    qDebug() << "MW::loadShortcuts";
    #endif
    }
    // Add customizable key shortcut actions
//    GData::actionKeys[thumbsGoTopAct->objectName()] = thumbsGoTopAct;
//    GData::actionKeys[thumbsGoBottomAct->objectName()] = thumbsGoBottomAct;
//    GData::actionKeys[closeImageAct->objectName()] = closeImageAct;
    G::actionKeys[fullScreenAction->objectName()] = fullScreenAction;
    G::actionKeys[escapeFullScreenAction->objectName()] = escapeFullScreenAction;
    G::actionKeys[prefAction->objectName()] = prefAction;
    G::actionKeys[exitAction->objectName()] = exitAction;
    G::actionKeys[thumbsEnlargeAction->objectName()] = thumbsEnlargeAction;
    G::actionKeys[thumbsShrinkAction->objectName()] = thumbsShrinkAction;
//    GData::actionKeys[cutAction->objectName()] = cutAction;
//    GData::actionKeys[copyAction->objectName()] = copyAction;
    G::actionKeys[nextThumbAction->objectName()] = nextThumbAction;
    G::actionKeys[prevThumbAction->objectName()] = prevThumbAction;
    G::actionKeys[downThumbAction->objectName()] = downThumbAction;
    G::actionKeys[upThumbAction->objectName()] = upThumbAction;
//    GData::actionKeys[keepTransformAct->objectName()] = keepTransformAct;
//    GData::actionKeys[keepZoomAct->objectName()] = keepZoomAct;
//    GData::actionKeys[copyImageAction->objectName()] = copyImageAction;
//    GData::actionKeys[pasteImageAction->objectName()] = pasteImageAction;
//    GData::actionKeys[refreshAction->objectName()] = refreshAction;
//    GData::actionKeys[pasteAction->objectName()] = pasteAction;
    G::actionKeys[togglePickAction->objectName()] = togglePickAction;
    G::actionKeys[toggleFilterPickAction->objectName()] = toggleFilterPickAction;
    G::actionKeys[ingestAction->objectName()] = ingestAction;
    G::actionKeys[reportMetadataAction->objectName()] = reportMetadataAction;
    G::actionKeys[slideShowAction->objectName()] = slideShowAction;
    G::actionKeys[firstThumbAction->objectName()] = firstThumbAction;
    G::actionKeys[lastThumbAction->objectName()] = lastThumbAction;
    G::actionKeys[randomImageAction->objectName()] = randomImageAction;
    G::actionKeys[openAction->objectName()] = openAction;
    G::actionKeys[zoomOutAction->objectName()] = zoomOutAction;
    G::actionKeys[zoomInAction->objectName()] = zoomInAction;
    G::actionKeys[zoomFitAction->objectName()] = zoomFitAction;
    G::actionKeys[zoomOrigAction->objectName()] = zoomOrigAction;
    G::actionKeys[rotateLeftAction->objectName()] = rotateLeftAction;
    G::actionKeys[rotateRightAction->objectName()] = rotateRightAction;
//    GData::actionKeys[moveRightAct->objectName()] = moveRightAct;
//    GData::actionKeys[moveLeftAct->objectName()] = moveLeftAct;
//    GData::actionKeys[copyToAction->objectName()] = copyToAction;
//    GData::actionKeys[moveToAction->objectName()] = moveToAction;
//    GData::actionKeys[keepTransformAct->objectName()] = keepTransformAct;
    G::actionKeys[infoVisibleAction->objectName()] = infoVisibleAction;
//    GData::actionKeys[toggleAllDocksAct->objectName()] = toggleAllDocksAct;
    G::actionKeys[folderDockVisibleAction->objectName()] = folderDockVisibleAction;
    G::actionKeys[favDockVisibleAction->objectName()] = favDockVisibleAction;
    G::actionKeys[metadataDockVisibleAction->objectName()] = metadataDockVisibleAction;
    G::actionKeys[thumbDockVisibleAction->objectName()] = thumbDockVisibleAction;
    G::actionKeys[windowsTitleBarVisibleAction->objectName()] = windowsTitleBarVisibleAction;
    G::actionKeys[menuBarVisibleAction->objectName()] = menuBarVisibleAction;
    G::actionKeys[statusBarVisibleAction->objectName()] = statusBarVisibleAction;
//    GData::actionKeys[toggleIconsListAction->objectName()] = toggleIconsListAction;
    G::actionKeys[allDocksLockAction->objectName()] = allDocksLockAction;

    G::setting->beginGroup("Shortcuts");
    QStringList groupKeys = G::setting->childKeys();

    if (groupKeys.size() && !defaultShortcuts)
    {
        if (groupKeys.contains(exitAction->text())) //rgh find a better way
        {
            QMapIterator<QString, QAction *> key(G::actionKeys);
            while (key.hasNext()) {
                key.next();
                if (groupKeys.contains(key.value()->text()))
                {
                    key.value()->setShortcut(G::setting->value(key.value()->text()).toString());
                    G::setting->remove(key.value()->text());
                    G::setting->setValue(key.key(), key.value()->shortcut().toString());
                }
            }
        }
        else
        {
            for (int i = 0; i < groupKeys.size(); ++i)
            {
                if (G::actionKeys.value(groupKeys.at(i)))
                    G::actionKeys.value(groupKeys.at(i))->setShortcut
                                    (G::setting->value(groupKeys.at(i)).toString());
            }
        }
    }
    else    // default shortcuts
    {
        qDebug() << "Default shortcuts";
        //    formats to set shortcut
        //    nextThumbAction->setShortcut(QKeySequence("Right"));
        //    nextThumbAction->setShortcut((Qt::Key_Right);

//        thumbsGoTopAct->setShortcut(QKeySequence("Ctrl+Home"));
//        thumbsGoBottomAct->setShortcut(QKeySequence("Ctrl+End"));
//        closeImageAct->setShortcut(Qt::Key_Escape);
        //        cutAction->setShortcut(QKeySequence("Ctrl+X"));
        //        copyAction->setShortcut(QKeySequence("Ctrl+C"));
        //        copyImageAction->setShortcut(QKeySequence("Ctrl+Shift+C"));
        //        pasteImageAction->setShortcut(QKeySequence("Ctrl+Shift+V"));
        //        refreshAction->setShortcut(QKeySequence("Ctrl+F5"));
        //        pasteAction->setShortcut(QKeySequence("Ctrl+V"));
        fullScreenAction->setShortcut(QKeySequence("F"));
        escapeFullScreenAction->setShortcut(QKeySequence("Esc"));
        prefAction->setShortcut(QKeySequence("Ctrl+P"));
        exitAction->setShortcut(QKeySequence("Ctrl+Q"));
        togglePickAction->setShortcut(QKeySequence("`"));
        toggleFilterPickAction->setShortcut(QKeySequence("Ctrl+`"));
        ingestAction->setShortcut(QKeySequence("C"));
        reportMetadataAction->setShortcut(QKeySequence("Ctrl+R"));
        slideShowAction->setShortcut(QKeySequence("Ctrl+E"));
        thumbsEnlargeAction->setShortcut(QKeySequence("}"));
        thumbsShrinkAction->setShortcut(QKeySequence("{"));
        nextThumbAction->setShortcut(QKeySequence("Right"));
        prevThumbAction->setShortcut(QKeySequence("Left"));
        firstThumbAction->setShortcut(QKeySequence("Home"));
        lastThumbAction->setShortcut(QKeySequence("End"));
        downThumbAction->setShortcut(QKeySequence("Down"));
        upThumbAction->setShortcut(QKeySequence("Up"));
        randomImageAction->setShortcut(QKeySequence("Ctrl+D"));
        openAction->setShortcut(QKeySequence("Return"));
        revealFileAction->setShortcut(QKeySequence("Ctrl+F"));
        zoomOutAction->setShortcut(QKeySequence("-"));
        zoomInAction->setShortcut(QKeySequence("+"));
        zoomFitAction->setShortcut(QKeySequence("*"));
        zoomOrigAction->setShortcut(QKeySequence("/"));
        rotateLeftAction->setShortcut(QKeySequence("Ctrl+Left"));
        rotateRightAction->setShortcut(QKeySequence("Ctrl+Right"));
//        moveLeftAct->setShortcut(QKeySequence("Left"));
//        moveRightAct->setShortcut(QKeySequence("Right"));
//        copyToAction->setShortcut(QKeySequence("Ctrl+Y"));
//        moveToAction->setShortcut(QKeySequence("Ctrl+M"));
//        keepTransformAct->setShortcut(QKeySequence("Ctrl+K"));
        infoVisibleAction->setShortcut(QKeySequence("I"));
//        toggleAllDocksAct->setShortcut(QKeySequence("F3"));
        newWorkspaceAction->setShortcut(QKeySequence("W"));
        manageWorkspaceAction->setShortcut(QKeySequence("Ctrl+W"));
        defaultWorkspaceAction->setShortcut(QKeySequence("Ctrl+Shift+W"));
        folderDockVisibleAction->setShortcut(QKeySequence("F4"));
        favDockVisibleAction->setShortcut(QKeySequence("F5"));
        metadataDockVisibleAction->setShortcut(QKeySequence("F6"));
        thumbDockVisibleAction->setShortcut(QKeySequence("F7"));
        windowsTitleBarVisibleAction->setShortcut(QKeySequence("F8"));
        menuBarVisibleAction->setShortcut(QKeySequence("F9"));
        statusBarVisibleAction->setShortcut(QKeySequence("F10"));
        folderDockLockAction->setShortcut(QKeySequence("Shift+F4"));
        favDockLockAction->setShortcut(QKeySequence("Shift+F5"));
        metadataDockLockAction->setShortcut(QKeySequence("Shift+F6"));
        thumbDockLockAction->setShortcut(QKeySequence("Shift+F7"));
        allDocksLockAction->setShortcut(QKeySequence("Ctrl+L"));
//        toggleIconsListAction->setShortcut(QKeySequence("Ctrl+T"));
    }

    G::setting->endGroup();
}

void MW::setupDocks()
{
    {
    #ifdef ISDEBUG
    qDebug() << "MW::setupDocks";
    #endif
    }
    thumbDock = new QDockWidget(tr("Thumbnails"), this);
    thumbDock->setObjectName("Viewer");

    imageViewContainer = new QVBoxLayout;
    imageViewContainer->setContentsMargins(0, 0, 0, 0);
    imageViewContainer->addWidget(thumbView);
    QWidget *imageViewContainerWidget = new QWidget;
    imageViewContainerWidget->setLayout(imageViewContainer);
    thumbDock->setWidget(imageViewContainerWidget);

    addDockWidget(Qt::LeftDockWidgetArea, thumbDock);
    addDockWidget(Qt::LeftDockWidgetArea, metadataDock);

    fsDockOrigWidget = folderDock->titleBarWidget();
    bmDockOrigWidget = favDock->titleBarWidget();
    iiDockOrigWidget = metadataDock->titleBarWidget();
    pvDockOrigWidget = thumbDock->titleBarWidget();
    folderDockEmptyWidget = new QWidget;
    favDockEmptyWidget = new QWidget;
    metadataDockEmptyWidget = new QWidget;
    thumbDockEmptyWidget = new QWidget;

    MW::setTabPosition(Qt::LeftDockWidgetArea, QTabWidget::North);
    MW::tabifyDockWidget(folderDock, favDock);
    MW::tabifyDockWidget(favDock, metadataDock);

    // match opening state from loadSettings
    updateState();
//    setWindowsTitleBarVisibility();  // image area shrinks

//    // rgh chk if docs were locked and open the same way
//    if (allDocksLockAction->isChecked()) {
//        setAllDocksLockMode();
//    }
}

void MW::updateState()
{
    setMenuBarVisibility();
    setStatusBarVisibility();
    setFolderDockVisibility();
    setFavDockVisibility();
    setMetadataDockVisibility();
    setThumbDockVisibity();
    setFolderDockLockMode();
    setFavDockLockMode();
    setMetadataDockLockMode();
    setThumbDockLockMode();
    setShootingInfo();
}

/*****************************************************************************************
 * HIDE/SHOW UI ELEMENTS
 * **************************************************************************************/

void MW::setShootingInfo() {
    {
    #ifdef ISDEBUG
    qDebug() << "MW::toggleInfo";
    #endif
    }
    qDebug() << "MW::toggleInfo";
    imageView->infoDropShadow->setVisible(infoVisibleAction->isChecked());
}

void MW::setThumbDockVisibity()
{
    {
    #ifdef ISDEBUG
    qDebug() << "MW::toggleThumbs";
    #endif
    }
    qDebug() << "MW::toggleThumbs";
    thumbDock->setVisible(thumbDockVisibleAction->isChecked());
    setThumbDockLockMode();
}

void MW::setFolderDockVisibility() {
    {
    #ifdef ISDEBUG
    qDebug() << "MW::toggleFiles";
    #endif
    }
    folderDock->setVisible(folderDockVisibleAction->isChecked());
}

void MW::setFavDockVisibility() {
    {
    #ifdef ISDEBUG
    qDebug() << "MW::toggleFavs";
    #endif
    }
    favDock->setVisible(favDockVisibleAction->isChecked());
}

void MW::setMetadataDockVisibility() {
    {
    #ifdef ISDEBUG
    qDebug() << "MW::toggleMetadata";
    #endif
    }
    metadataDock->setVisible(metadataDockVisibleAction->isChecked());
}

// rgh think about this one
//void MW::toggleAllDocks() {
//    {
//    #ifdef ISDEBUG
//    qDebug() << "MW::toggleNonThumbs";
//    #endif
//    }
//    toggleThumbsAction->setChecked(!toggleThumbsAction->isChecked());
//    thumbDock->setVisible(toggleThumbsAction->isChecked());
//    toggleFilesAction->setChecked(!toggleFilesAction->isChecked());
//    fsDock->setVisible(toggleFilesAction->isChecked());
//    toggleFavsAction->setChecked(!toggleFavsAction->isChecked());
//    bmDock->setVisible(toggleFavsAction->isChecked());
//    toggleMetadataAction->setChecked(!toggleMetadataAction->isChecked());
//    iiDock->setVisible(toggleMetadataAction->isChecked());
////    tagsDock->setVisible(!tagsDock->isVisible());
//}

void MW::setMenuBarVisibility() {

    {
    #ifdef ISDEBUG
    qDebug() << "MW::toggleMenuBar";
    #endif
    }
    menuBar()->setVisible(menuBarVisibleAction->isChecked());
}

void MW::setStatusBarVisibility() {

    {
    #ifdef ISDEBUG
    qDebug() << "MW::toggleStatusBar";
    #endif
    }
    statusBar()->setVisible(statusBarVisibleAction->isChecked());
//    G::isStatusBarVisible = statusBarVisibleAction->isChecked();
}

void MW::setWindowsTitleBarVisibility() {

    {
    #ifdef ISDEBUG
    qDebug() << "MW::toggleWindowsTitleBar";
    #endif
    }
    if(windowsTitleBarVisibleAction->isChecked()) {
        hide();
        setWindowFlags(windowFlags() & ~Qt::FramelessWindowHint);
        show();    }
    else {
        hide();
        setWindowFlags(windowFlags() | Qt::FramelessWindowHint);
        show();
    }
}

// replace this with isList and isThumbs

//void MW::toggleIconsList() {
//    {
//    #ifdef ISDEBUG
//    qDebug() << "MW::toggleIconsList";
//    #endif
//    }
//    if(toggleIconsListAction->isChecked()) {
//        isIconView = false;    }
//    else {
//        isIconView = true;
//    }
//}

void MW::setFolderDockLockMode()
{
    if (folderDockLockAction->isChecked()) {
        folderDock->setTitleBarWidget(folderDockEmptyWidget);
//        G::isFolderDockLocked = true;
    }
    else {
        folderDock->setTitleBarWidget(fsDockOrigWidget);
//        G::isFolderDockLocked = false;
    }
}

void MW::setFavDockLockMode()
{
    if (favDockLockAction->isChecked()) {
        favDock->setTitleBarWidget(favDockEmptyWidget);
//        G::isFavsDockLocked = true;
    }
    else {
        favDock->setTitleBarWidget(bmDockOrigWidget);
//        G::isFavsDockLocked = false;
    }
}

void MW::setMetadataDockLockMode()
{
    if (metadataDockLockAction->isChecked()) {
        metadataDock->setTitleBarWidget(metadataDockEmptyWidget);
//        G::isMetadataDockLocked = true;
    }
    else {
        metadataDock->setTitleBarWidget(iiDockOrigWidget);
//        G::isMetadataDockLocked = false;
    }
}

void MW::setThumbDockLockMode()
{
    if (thumbDockLockAction->isChecked()) {
        thumbDock->setTitleBarWidget(thumbDockEmptyWidget);
//        G::isThumbDockLocked = true;
    }
    else {
        thumbDock->setTitleBarWidget(pvDockOrigWidget);
//        G::isThumbDockLocked = false;
    }
}

void MW::setAllDocksLockMode()
{
    {
    #ifdef ISDEBUG
    qDebug() << "MW::lockDocks";
    #endif
    }
//    if (initComplete)
//            GData::LockDocks = lockDocksAct->isChecked();

    if (allDocksLockAction->isChecked()) {
        folderDock->setTitleBarWidget(folderDockEmptyWidget);
        favDock->setTitleBarWidget(favDockEmptyWidget);
        metadataDock->setTitleBarWidget(metadataDockEmptyWidget);
        thumbDock->setTitleBarWidget(thumbDockEmptyWidget);
        folderDockLockAction->setChecked(true);
        favDockLockAction->setChecked(true);
        metadataDockLockAction->setChecked(true);
        thumbDockLockAction->setChecked(true);
//        G::isLockAllDocks = true;
    } else {
        folderDock->setTitleBarWidget(fsDockOrigWidget);
        favDock->setTitleBarWidget(bmDockOrigWidget);
        metadataDock->setTitleBarWidget(iiDockOrigWidget);
        thumbDock->setTitleBarWidget(pvDockOrigWidget);
        folderDockLockAction->setChecked(false);
        favDockLockAction->setChecked(false);
        metadataDockLockAction->setChecked(false);
        thumbDockLockAction->setChecked(false);
//        G::isLockAllDocks = false;
    }
}

void MW::closeEvent(QCloseEvent *event)
{
    {
    #ifdef ISDEBUG
    qDebug() << "MW::closeEvent";
    #endif
    }
    imageCacheThread->exit();
//    thumbView->abort();
    writeSettings();
    hide();
    if (!QApplication::clipboard()->image().isNull()) {
        QApplication::clipboard()->clear();
    }
    event->accept();
}

// not used
void MW::setStatus(QString state)
{
    {
    #ifdef ISDEBUG
    qDebug() << "MW::setStatus";
    #endif
    }
    stateLabel->setText("    " + state + "    ");
}


void MW::togglePick()
{
    {
    #ifdef ISDEBUG
    qDebug() << "MW::togglepick";
    #endif
    }

    QModelIndex index;
    QModelIndexList indexesList = thumbView->selectionModel()->selectedIndexes();
    QString pickStatus;

    foreach (index, indexesList) {
        QModelIndex srcIdx = thumbView->thumbViewFilter->mapToSource(index);
        pickStatus = (qvariant_cast<QString>(srcIdx.data(thumbView->PickedRole))
                      == "true") ? "false" : "true";
        thumbView->thumbViewModel->item(srcIdx.row())
                 ->setData(pickStatus, thumbView->UserRoles::PickedRole);
    }

    if (indexesList.length() == 1) {
        if (pickStatus == "true") {
//            thumbView->thumbViewModel->item(index.row())->setBackground(Qt::red);
            imageView->pickLabel->setVisible(true);
        } else {
//            thumbView->thumbViewModel->item(index.row())->setBackground(Qt::darkGray);
            imageView->pickLabel->setVisible(false);
        }
    }
}

// When a new image is selected and shown in imageView update the visibility
// of the thumbs up icon that highlights if the image has been picked.
void MW::updatePick()
{
    {
    #ifdef ISDEBUG
    qDebug() << "MW::updatepick";
    #endif
    }
    QModelIndex idx = thumbView->currentIndex();
    bool isPick = qvariant_cast<bool>(idx.data(thumbView->PickedRole));
    (isPick) ? imageView->pickLabel->setVisible(true)
             : imageView->pickLabel->setVisible(false);
}

void MW::toggleFilterPick()
{
    {
    #ifdef ISDEBUG
    qDebug() << "MW::toggleFilterPick";
    #endif
    }
    thumbView->pickFilter = toggleFilterPickAction->isChecked();
    if (toggleFilterPickAction->isChecked()) {
        int row = thumbView->getNearestPick();
//        qDebug() << "Nearest pick = row" << row;
        if (row > 0) thumbView->selectThumb(row);
        thumbView->thumbViewFilter->setFilterRegExp("true");// show only picked items
    }
    else
        thumbView->thumbViewFilter->setFilterRegExp("");    // no filter - show all
}

void MW::copyPicks()
{
    if (thumbView->isPick()) {
        // rgh2017-04-30 commented out because crashing on OSX
        QFileInfoList imageList = thumbView->getPicks();
        copyPickDlg = new CopyPickDlg(this, imageList, metadata);
        copyPickDlg->exec();
        delete copyPickDlg;
    }
    else QMessageBox::information(this,
         "Oops", "There are no picks to copy.    ", QMessageBox::Ok);
}

void MW::setSlideShowParameters(int delay, bool isRandom)
{
    slideShowDelay = delay;
    slideShowRandom = isRandom;
}

void MW::slideShow()
{
    {
    #ifdef ISDEBUG
    qDebug() << "MW::slideShow";
    #endif
    }
    if (G::slideShowActive) {
        G::slideShowActive = false;
        slideShowAction->setText(tr("Slide Show"));
//        imageView->setFeedback(tr("Slide show stopped"));

        SlideShowTimer->stop();
        delete SlideShowTimer;
        slideShowAction->setIcon(QIcon::fromTheme("media-playback-start", QIcon(":/images/play.png")));
    } else {
        if (thumbView->thumbViewModel->rowCount() <= 0) {
            return;
        }

//        if (GData::layoutMode == thumbViewIdx) {
            QModelIndexList indexesList = thumbView->selectionModel()->selectedIndexes();
            if (indexesList.size() != 1) {
//                    thumbView->setCurrentRow(0);
                    thumbView->selectThumb(0);
            } else {
//                    thumbView->setCurrentRow(indexesList.first().row());
                    thumbView->selectThumb(indexesList.first().row());
            }

//            showViewer();
//        }

        G::slideShowActive = true;

        SlideShowTimer = new QTimer(this);
        connect(SlideShowTimer, SIGNAL(timeout()), this, SLOT(slideShowHandler()));
        SlideShowTimer->start(slideShowDelay * 1000);

        slideShowAction->setText(tr("Stop Slide Show"));
//        imageView->setFeedback(tr("Slide show started"));
        slideShowAction->setIcon(QIcon::fromTheme("media-playback-stop", QIcon(":/images/stop.png")));

        slideShowHandler();
    }
}

// rgh redo based on tweaked thumbview
void MW::slideShowHandler()
{
    {
    #ifdef ISDEBUG
    qDebug() << "MW::slideShowHandler";
    #endif
    }
//    if (GData::slideShowActive) {
//        if (GData::slideShowRandom) {
//            thumbView->selectRandom();
//        } else {
//            int currentRow = thumbView->currentIndex().row();
//            imageView->loadImage(thumbView->thumbViewModel->item(currentRow)->data(thumbView->FileNameRole).toString());
//            if (thumbView->getNextRow() > 0) {
//                thumbView->selectThumb(thumbView->getNextRow());
//            } else {
//                if (GData::wrapImageList) {
//                    thumbView->selectThumb(0);
//                } else {
//                    slideShow();
//                }
//            }
//        }
//    }
}

void MW::loadNextImage()
{
    {
    #ifdef ISDEBUG
    qDebug() << "MW::loadNextImage";
    #endif
    }
    thumbView->selectNext();
}

void MW::loadPrevImage()
{
    {
    #ifdef ISDEBUG
    qDebug() << "MW::loadPrevImage";
    #endif
    }
    thumbView->selectPrev();
}

void MW::loadUpImage()
{
    {
    #ifdef ISDEBUG
    qDebug() << "MW::loadUpImage";
    #endif
    }
    thumbView->selectUp();
}

void MW::loadDownImage()
{
    {
    #ifdef ISDEBUG
    qDebug() << "MW::loadDownImage";
    #endif
    }
    thumbView->selectDown();
}

void MW::loadFirstImage()
{
    {
    #ifdef ISDEBUG
    qDebug() << "MW::loadFirstImage";
    #endif
    }
    if (thumbView->thumbViewFilter->rowCount() <= 0) {
        return;
    }
    thumbView->selectFirst();
}

void MW::loadLastImage()
{
    {
    #ifdef ISDEBUG
    qDebug() << "MW::loadLastImage";
    #endif
    }
    thumbView->selectLast();
}

void MW::loadRandomImage()
{
    {
    #ifdef ISDEBUG
    qDebug() << "MW::loadRandomImage";
    #endif
    }
    if (thumbView->thumbViewFilter->rowCount() <= 0) {
        return;
    }
    thumbView->selectRandom();
}

void MW::dropOp(Qt::KeyboardModifiers keyMods, bool dirOp, QString cpMvDirPath)
{
    {
    #ifdef ISDEBUG
    qDebug() << "MW::dropOp";
    #endif
    }
    QApplication::restoreOverrideCursor();
    G::copyOp = (keyMods == Qt::ControlModifier);
    QMessageBox msgBox;
    QString destDir;

    if (QObject::sender() == fsTree) {
        destDir = getSelectedPath();
    } else if (QObject::sender() == bookmarks) {
        if (bookmarks->currentItem()) {
            destDir = bookmarks->currentItem()->toolTip(0);
        } else {
            addBookmark(cpMvDirPath);
            return;
        }
    } else {
        // Unknown sender
        return;
    }

    if (!isValidPath(destDir)) {
        msgBox.critical(this, tr("Error"), tr("Can not move or copy images to this folder."));
        selectCurrentViewDir();
        return;
    }

    if (destDir == 	G::currentViewDir) {
        msgBox.critical(this, tr("Error"), tr("Destination folder is same as source."));
        return;
    }

    if (dirOp) {
        QString dirOnly =
            cpMvDirPath.right(cpMvDirPath.size() - cpMvDirPath.lastIndexOf(QDir::separator()) - 1);

        QString question = tr("Move \"%1\" to \"%2\"?").arg(dirOnly).arg(destDir);
        int ret = QMessageBox::question(this, tr("Move folder"), question,
                            QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel);

        if (ret == QMessageBox::Yes) {
            QFile dir(cpMvDirPath);
            bool ok = dir.rename(destDir + QDir::separator() + dirOnly);
            if (!ok) {
                QMessageBox msgBox;
                msgBox.critical(this, tr("Error"), tr("Failed to move folder."));
            }
            setStatus(tr("Folder moved"));
        }
    }
//    else {
//        CpMvDialog *cpMvdialog = new CpMvDialog(this);
//        GData::copyCutIdxList = thumbView->selectionModel()->selectedIndexes();
//        cpMvdialog->exec(thumbView, destDir, false);

//        if (!GData::copyOp) {
//            int row = cpMvdialog->latestRow;
//            if (thumbView->thumbViewModel->rowCount()) {
//                if (row >= thumbView->thumbViewModel->rowCount()) {
//                    row = thumbView->thumbViewModel->rowCount() -1 ;
//                }

//                thumbView->setCurrentRow(row);
//                thumbView->selectThumbByRow(row);
//            }
//        }

//        QString state = QString((GData::copyOp? tr("Copied") : tr("Moved")) + " " +
//                            tr("%n image(s)", "", cpMvdialog->nfiles));
//        setStatus(state);
//        delete(cpMvdialog);
//    }

//    thumbView->loadVisibleThumbs();
}

void MW::selectCurrentViewDir()
{
    {
    #ifdef ISDEBUG
    qDebug() << "MW::selectCurrentViewDir";
    #endif
    }

    QModelIndex idx = fsTree->fsModel->index(G::currentViewDir);
    if (idx.isValid()){
        fsTree->expand(idx);
        fsTree->setCurrentIndex(idx);
    }
}

void MW::wheelEvent(QWheelEvent *event)
{
    {
    #ifdef ISDEBUG
    qDebug() << "MW::wheelEvent";
    #endif
    }
        if (event->modifiers() == Qt::ControlModifier)
        {
            if (event->delta() < 0)
                imageView->zoomOut();
            else
                imageView->zoomIn();
        }
        else if (nextThumbAction->isEnabled())
        {
            if (event->delta() < 0)
                loadNextImage();
            else
                loadPrevImage();
        }
}

// not req'd
void MW::showNewImageWarning(QWidget *parent)
// called from runExternalApp
{
    {
    #ifdef ISDEBUG
    qDebug() << "MW::showNewImageWarning";
    #endif
    }
    QMessageBox msgBox;
    msgBox.warning(parent, tr("Warning"), tr("Cannot perform action with temporary image."));
}

void MW::addNewBookmark()
{
    {
    #ifdef ISDEBUG
    qDebug() << "MW::addNewBookmark";
    #endif
    }
    addBookmark(getSelectedPath());
}

void MW::addBookmark(QString path)
{
    {
    #ifdef ISDEBUG
    qDebug() << "MW::addBookmark";
    #endif
    }
    G::bookmarkPaths.insert(path);
    bookmarks->reloadBookmarks();
}

// not implemented yet, temp use for refresh thumbs test
void MW::openOp()
{
    {
    #ifdef ISDEBUG
    qDebug() << "MW::openOp";
    #endif
    }

    takeCentralWidget();
    setCentralWidget(thumbView);
//    thumbView->refreshThumbs();

//    if (GData::layoutMode == imageViewIdx) {
//        hideViewer();
//        return;
//    }

//    if (QApplication::focusWidget() == fsTree) {
//        folderSelectionChange();
//        return;
//    } else if (QApplication::focusWidget() == thumbView
//                || QApplication::focusWidget() == imageView->scrlArea)
//    {
//        QModelIndex idx;
//        QModelIndexList indexesList = thumbView->selectionModel()->selectedIndexes();
//        if (indexesList.size() > 0) {
//            idx = indexesList.first();
//        } else {
//            if (thumbView->thumbViewModel->rowCount() == 0) {
//                setStatus(tr("No images"));
//                return;
//            }
//            // rgh change to call thumbView::selectThumbByRow
//            idx = thumbView->thumbViewModel->indexFromItem(thumbView->thumbViewModel->item(0));
//            thumbView->selectionModel()->select(idx, QItemSelectionModel::Toggle);
////            thumbView->setCurrentRow(0);
//        }

////        loadImagefromThumb(idx);
//        return;
//    }
}

void MW::revealFile()
{

    // See http://stackoverflow.com/questions/3490336/how-to-reveal-in-finder-or-show-in-explorer-with-qt
    // for details

    QString pathToReveal = thumbView->getCurrentFilename();

    // Mac, Windows support folder or file.
#if defined(Q_OS_WIN)
    const QString explorer = Environment::systemEnvironment().searchInPath(QLatin1String("explorer.exe"));
    if (explorer.isEmpty()) {
        QMessageBox::warning(this,
                             tr("Launching Windows Explorer failed"),
                             tr("Could not find explorer.exe in path to launch Windows Explorer."));
        return;
    }
    QString param;
    if (!QFileInfo(pathIn).isDir())
        param = QLatin1String("/select,");
    param += QDir::toNativeSeparators(pathIn);
    QString command = explorer + " " + param;
    QString command = explorer + " " + param;
    QProcess::startDetached(command);

#elif defined(Q_OS_MAC)
    Q_UNUSED(this)
    QStringList scriptArgs;
    scriptArgs << QLatin1String("-e")
            << QString::fromLatin1("tell application \"Finder\" to reveal POSIX file \"%1\"")
            .arg(pathToReveal);
    QProcess::execute(QLatin1String("/usr/bin/osascript"), scriptArgs);
    scriptArgs.clear();
    scriptArgs << QLatin1String("-e")
            << QLatin1String("tell application \"Finder\" to activate");
    QProcess::execute("/usr/bin/osascript", scriptArgs);
#else
    // we cannot select a file here, because no file browser really supports it...
    const QFileInfo fileInfo(pathIn);
    const QString folder = fileInfo.absoluteFilePath();
    const QString app = Utils::UnixUtils::fileBrowser(Core::ICore::instance()->settings());
    QProcess browserProc;
    const QString browserArgs = Utils::UnixUtils::substituteFileBrowserParameters(app, folder);
    if (debug)
        qDebug() <<  browserArgs;
    bool success = browserProc.startDetached(browserArgs);
    const QString error = QString::fromLocal8Bit(browserProc.readAllStandardError());
    success = success && error.isEmpty();
    if (!success)
        showGraphicalShellError(parent, app, error);
#endif

}

void MW::openInFinder()
{
    takeCentralWidget();
    setCentralWidget(imageView);
}

void MW::openInExplorer()
{
    QString path = "C:/exampleDir/example.txt";

    QStringList args;

    args << "/select," << QDir::toNativeSeparators(path);

    QProcess *process = new QProcess(this);
    process->start("explorer.exe", args);
}


// End MW




//void MW::refreshThumbsFromCache(QModelIndex &idx, QImage &thumb)
//void MW::refreshThumbsFromCache()
//{
////    thumbView->setWrapping(true);
////    qDebug() << "MW thumb length ="; //<< thumb.size();
////    QStandardItem *item = new QStandardItem;
////    item = thumbView->thumbViewModel->itemFromIndex(idx);
////    item->setIcon(QPixmap::fromImage(thumb));
////    thumbCacheThread->restart = true;
//    thumbView->update();
////    qDebug() << "MW::refreshThumbsFromCache()";
//}

//void MW::refreshThumbs(bool scrollToTop)
//{
//    {
//    #ifdef ISDEBUG
//    qDebug() << "MW::refreshThumbs";
//    #endif
//    }
////    thumbView->setNeedScroll(scrollToTop);
////	QTimer::singleShot(0, this, SLOT(reloadThumbsSlot()));

////    reloadThumbsSlot();
//}

// this is called via singleshot qtimer until all thumbs loaded
//void MW::reloadThumbsSlot()
//{
//    {
//    #ifdef ISDEBUG
//    qDebug() << "MW::reloadThumbsSlot";
//    #endif
//    }
////    if (thumbView->busy || !initComplete)
////    {
////        thumbView->abort();
////        QTimer::singleShot(0, this, SLOT(reloadThumbsSlot()));
////        reloadThumbsSlot();
////        return;
////    }

//    if (GData::currentViewDir == "")
//    {
//        GData::currentViewDir = getSelectedPath();
//        if (GData::currentViewDir == "")
//            return;
//    }

//    QDir checkPath(GData::currentViewDir);
//    if (!checkPath.exists() || !checkPath.isReadable())
//    {
//        QMessageBox msgBox;
//        msgBox.critical(this, tr("Error"), tr("Failed to open folder:") + " " + GData::currentViewDir);
//        return;
//    }

//    // Don't think this is used any more DELETE??? rgh
////    thumbView->infoView->clear();
//    if (GData::layoutMode == thumbViewIdx && thumbDock->isVisible()) {
//        QString ImagePath(":/images/no_image.png");
//        imageView->loadImage(ImagePath);
//    }

////	pathBar->setText(GData::currentViewDir);
////	recordHistory(GData::currentViewDir);
////	if (currentHistoryIdx > 0)
////		goBackAction->setEnabled(true);

//    if (GData::layoutMode == thumbViewIdx)
//    {
//        setAppWindowTitle();
//    }

////	thumbView->busy = true;

//    thumbView->load();
//}

// EVENTS

//bool MW::event(QEvent *event)
//{
//    #ifdef ISDEBUG
////    qDebug() << "MW::event - " << event->type();
//    #endif
//    // check this - may be triggered more often than necessary ***rgh
//    if (event->type() == QEvent::ActivationChange ||
//     (GData::layoutMode == thumbViewIdx && event->type() == QEvent::MouseButtonRelease)) {

//        {
////        #ifdef ISDEBUG
////        qDebug() << "MW::event - MouseButtonRelease";
////        #endif
//        }
//    }

//    // coordinate menu state with dock state after initial app window has been drawn
//    if (event->type() == QEvent::Show) {

//        {
////        #ifdef ISDEBUG
////        qDebug() << "MW::event - Show";
////        #endif
//        }
//        if (GData::LockDocks) lockDocks();
//        toggleThumbsAct->setChecked(thumbDock->isVisible());
//        toggleFilesAct->setChecked(fsDock->isVisible());
//        toggleFavsAct->setChecked(bmDock->isVisible());
//        toggleMetadataAct->setChecked(iiDock->isVisible());
//    }

//    if (event->type() == QEvent::WindowActivate) {
//    /*  make sure window gets painted before updating the cache
//        which can take a while
//        qApp->processEvents();
//        updateCache();*/
//    }

//    return QMainWindow::event(event);
//}

// Unused actions

//    thumbsGoTopAct = new QAction(tr("Top"), this);
//    thumbsGoTopAct->setObjectName("thumbsGoTop");
//    thumbsGoTopAct->setIcon(QIcon::fromTheme("go-top", QIcon(":/images/top.png")));
//    connect(thumbsGoTopAct, SIGNAL(triggered()), this, SLOT(goTop()));

//    thumbsGoBottomAct = new QAc$tion(tr("Bottom"), this);
//    thumbsGoBottomAct->setObjectName("thumbsGoBottom");
//    thumbsGoBottomAct->setIcon(QIcon::fromTheme("go-bottom", QIcon(":/images/bottom.png")));
//    connect(thumbsGoBottomAct, SIGNAL(triggered()), this, SLOT(goBottom()));

//    closeImageAct = new QAction(tr("Close Image"), this);
//    closeImageAct->setObjectName("closeImage");
//    connect(closeImageAct, SIGNAL(triggered()), this, SLOT(hideViewer()));

//    cutAction = new QAction(tr("Cut"), this);
//    cutAction->setObjectName("cut");
//    cutAction->setIcon(QIcon::fromTheme("edit-cut", QIcon(":/images/cut.png")));
//    connect(cutAction, SIGNAL(triggered()), this, SLOT(cutThumbs()));
//    cutAction->setEnabled(false);

//    copyAction = new QAction(tr("Copy"), this);
//    copyAction->setObjectName("copy");
//    copyAction->setIcon(QIcon::fromTheme("edit-copy", QIcon(":/images/copy.png")));
//    connect(copyAction, SIGNAL(triggered()), this, SLOT(copyThumbs()));
//    copyAction->setEnabled(false);

//    copyToAction = new QAction(tr("Copy to..."), this);
//    copyToAction->setObjectName("copyTo");
//    connect(copyToAction, SIGNAL(triggered()), this, SLOT(copyImagesTo()));

//    moveToAction = new QAction(tr("Move to..."), this);
//    moveToAction->setObjectName("moveTo");
//    connect(moveToAction, SIGNAL(triggered()), this, SLOT(moveImagesTo()));

//    deleteAction = new QAction(tr("Delete"), this);
//    deleteAction->setObjectName("delete");
//    deleteAction->setIcon(QIcon::fromTheme("edit-delete", QIcon(":/images/delete.png")));
//    connect(deleteAction, SIGNAL(triggered()), this, SLOT(deleteOp()));

//    copyImageAction = new QAction(tr("Copy Image Data"), this);
//    copyImageAction->setObjectName("copyImage");
//    pasteImageAction = new QAction(tr("Paste Image Data"), this);
//    pasteImageAction->setObjectName("pasteImage");


//    reportMetadataAction = new QAction(tr("Report all metadata"), this);
//    reportMetadataAction->setObjectName("settings");
//    reportMetadataAction->setIcon(QIcon::fromTheme("preferences-other", QIcon(":/images/settings.png")));
//    connect(reportMetadataAction, SIGNAL(triggered()), this, SLOT(reportMetadata()));

//    actShowHidden = new QAction(tr("Show Hidden Files"), this);
//    actShowHidden->setObjectName("showHidden");
//    actShowHidden->setCheckable(true);
//    actShowHidden->setChecked(GData::showHiddenFiles);
//    connect(actShowHidden, SIGNAL(triggered()), this, SLOT(showHiddenFiles()));

//    toggleAllDocksAct = new QAction(tr("All docks"), this);
//    toggleAllDocksAct->setObjectName("toggleAllDocks");
//    toggleAllDocksAct->setCheckable(true);
//    toggleAllDocksAct->setChecked(true);
//    connect(toggleAllDocksAct, SIGNAL(triggered()), this, SLOT(toggleAllDocks()));


    // to be removed rgh
//    actClassic = new QAction(tr("Classic Thumbs"), this);
//    actClassic->setObjectName("classic");
//    actCompact = new QAction(tr("Compact"), this);
//    actCompact->setObjectName("compact");
//    actSquarish = new QAction(tr("Squarish"), this);
//    actSquarish->setObjectName("squarish");
//    connect(actClassic, SIGNAL(triggered()), this, SLOT(setClassicThumbs()));
//    connect(actCompact, SIGNAL(triggered()), this, SLOT(setCompactThumbs()));
//    connect(actSquarish, SIGNAL(triggered()), this, SLOT(setSquarishThumbs()));
//    actClassic->setCheckable(true);
//    actCompact->setCheckable(true);
//    actSquarish->setCheckable(true);
//    actClassic->setChecked(GData::thumbsLayout == ThumbView::Classic);
//    actCompact->setChecked(GData::thumbsLayout == ThumbView::Compact);
//    actSquarish->setChecked(GData::thumbsLayout == ThumbView::Squares);

    // rgh not used
//    refreshAction = new QAction(tr("Reload"), this);
//    refreshAction->setObjectName("refresh");
//    refreshAction->setIcon(QIcon::fromTheme("view-refresh", QIcon(":/images/refresh.png")));
//    connect(refreshAction, SIGNAL(triggered()), this, SLOT(reload()));

//    pasteAction = new QAction(tr("Paste Here"), this);
//    pasteAction->setObjectName("paste");
//    pasteAction->setIcon(QIcon::fromTheme("edit-paste", QIcon(":/images/paste.png")));
//    connect(pasteAction, SIGNAL(triggered()), this, SLOT(pasteThumbs()));
//    pasteAction->setEnabled(false);

//    toggleFilterAction = new QAction(tr("Filter"), this);
//    toggleFilterAction->setObjectName("toggleFilter");
//    toggleFilterAction->setCheckable(true);
//    toggleFilterAction->setChecked(false);
//    connect(toggleFilterAction, SIGNAL(triggered()), this, SLOT(toggleFilter()));
//    toggleFilterAction->setIcon(QIcon::fromTheme("media-playback-start", QIcon(":/images/play.png")));

//    keepZoomAct = new QAction(tr("Keep Zoom"), this);
//    keepZoomAct->setObjectName("keepZoom");
//    keepZoomAct->setCheckable(true);
//    connect(keepZoomAct, SIGNAL(triggered()), this, SLOT(keepZoom()));

//    keepTransformAct = new QAction(tr("Lock Transformations"), this);
//    keepTransformAct->setObjectName("keepTransform");
//    keepTransformAct->setCheckable(true);
//    connect(keepTransformAct, SIGNAL(triggered()), this, SLOT(keepTransformClicked()));

    // replace with asList and asThumbs
//    toggleIconsListAction = new QAction(tr("Thumbnails or File List"), this);
//    toggleIconsListAction->setObjectName("toggleIconsList");
//    toggleIconsListAction->setCheckable(true);
//    toggleIconsListAction->setChecked(true);
//    connect(toggleIconsListAction, SIGNAL(triggered()), this, SLOT(toggleIconsList()));

    // duplication of prevImageAction
//    moveLeftAct = new QAction(tr("Move Left"), this);
//    moveLeftAct->setObjectName("moveLeft");
//    connect(moveLeftAct, SIGNAL(triggered()), this, SLOT(loadPrevImage()));
//    connect(moveLeftAct, SIGNAL(triggered()), this, SLOT(moveLeft()));

    // duplication of nextImageAction
//    moveRightAct = new QAction(tr("Move Right"), this);
//    moveRightAct->setObjectName("moveRight");
//    connect(moveRightAct, SIGNAL(triggered()), this, SLOT(loadNextImage()));
//    connect(moveRightAct, SIGNAL(triggered()), this, SLOT(moveRight()));

//    connect(thumbView, SIGNAL(setStatus(QString)), this, SLOT(setStatus(QString)));
//    connect(thumbView, SIGNAL(showBusy(bool)), this, SLOT(showBusyStatus(bool)));

//    connect(thumbCacheThread, SIGNAL(updateThumbs()),
//            this, SLOT(refreshThumbsFromCache()));

//    connect(thumbCacheThread, SIGNAL(loadImageCache()),
//            this, SLOT(loadImageCache()));

//    connect(thumbCacheThread, SIGNAL(updateThumbs(QModelIndex idx, QImage thumb)),
//            this, SLOT(refreshThumbsFromCache(QModelIndex &idx, QImage &thumb)));

    //    connect(thumbView->selectionModel(), SIGNAL(selectionChanged(QItemSelection, QItemSelection)),
//                this, SLOT(updateActions()));

//void MW::createImageTags()
//{
//    {
//    #ifdef ISDEBUG
//    qDebug() << "MW::createImageTags";
//    #endif
//    }
//    tagsDock = new QDockWidget(tr("  Tags  "), this);
//    tagsDock->setObjectName("Tags");
//    thumbView->imageTags = new ImageTags(tagsDock, thumbView, metadata);
//    tagsDock->setWidget(thumbView->imageTags);

//    connect(tagsDock->toggleViewAction(), SIGNAL(triggered()),
//            this, SLOT(setTagsDockVisibility()));
//    connect(tagsDock, SIGNAL(visibilityChanged(bool)),
//            this, SLOT(setTagsDockVisibility()));
//    addDockWidget(Qt::LeftDockWidgetArea, tagsDock);

//    connect(thumbView->imageTags, SIGNAL(setStatus(QString)), this, SLOT(setStatus(QString)));
//    connect(thumbView->imageTags, SIGNAL(reloadThumbs()), this, SLOT(reloadThumbsSlot()));
//	connect(thumbView->imageTags->removeTagAction, SIGNAL(triggered()), this, SLOT(deleteOp()));
//}

//rgh required?
//void MW::reload()
//{
//    {
//    #ifdef ISDEBUG
//    qDebug() << "MW::reload";
//    #endif
//    }
////    findDupesAction->setChecked(false);
//    if (GData::layoutMode == thumbViewIdx) {
//        if (GData::isDebug) qDebug() << "MW::reload  refreshThumbs(false)";
////        refreshThumbs(false);
//    } else {
//        if (GData::isDebug) qDebug() << "MW::reload  imageView->reload()";
//        imageView->reload();
//    }
//}

//void MW::keepZoom()
//{
//    {
//    #ifdef ISDEBUG
//    qDebug() << "MW::keepZoom";
//    #endif
//    }
//    GData::keepZoomFactor = keepZoomAct->isChecked();
//    if (GData::keepZoomFactor)
//        imageView->setFeedback(tr("Zoom Locked"));
//    else
//        imageView->setFeedback(tr("Zoom Unlocked"));
//}

//void MW::keepTransformClicked()
//{
//    {
//    #ifdef ISDEBUG
//    qDebug() << "MW::keepTransformClicked";
//    #endif
//    }
//    GData::keepTransform = keepTransformAct->isChecked();

//    if (GData::keepTransform) {
//        imageView->setFeedback(tr("Transformations Locked"));
////        if (cropDialog)
////            cropDialog->applyCrop(0);
//    } else {
////        GData::cropLeftPercent = GData::cropTopPercent = GData::cropWidthPercent = GData::cropHeightPercent = 0;
//        imageView->setFeedback(tr("Transformations Unlocked"));
//    }

//    imageView->refresh();
//}

//void MW::moveRight()
//{
//    {
//    #ifdef ISDEBUG
//    qDebug() << "MW::moveRight";
//    #endif
//    }
//    imageView->keyMoveEvent(ImageView::MoveRight);
//    qDebug() << "MW::moveRight (triggered by arrow in preview mode";
//}

//void MW::moveLeft()
//{
//    {
//    #ifdef ISDEBUG
//    qDebug() << "MW::moveLeft";
//    #endif
//    }
//    imageView->keyMoveEvent(ImageView::MoveLeft);
//}

//void MW::moveUp()
//{
//    {
//    #ifdef ISDEBUG
//    qDebug() << "MW::moveUp";
//    #endif
//    }
//    imageView->keyMoveEvent(ImageView::MoveUp);
//}

//void MW::moveDown()
//{
//    {
//    #ifdef ISDEBUG
//    qDebug() << "MW::moveDown";
//    #endif
//    }
//    imageView->keyMoveEvent(ImageView::MoveDown);
//}

// COPY AND PASTE OLD CODE

//void MW::copyOrCutThumbs(bool copy)
//{
//    {
//    #ifdef ISDEBUG
//    qDebug() << "MW::copyOrCutThumbs";
//    #endif
//    }
//    GData::copyCutIdxList = thumbView->selectionModel()->selectedIndexes();
//    copyCutCount = GData::copyCutIdxList.size();

//    GData::copyCutFileList.clear();
//    for (int tn = 0; tn < copyCutCount; ++tn)
//    {
//        GData::copyCutFileList.append(thumbView->thumbViewModel->item(GData::copyCutIdxList[tn].
//                                        row())->data(thumbView->FileNameRole).toString());
//    }

//    GData::copyOp = copy;
//    pasteAction->setEnabled(true);
//    if (GData::isDebug) qDebug() << "MW::copyOrCutThumbs ";
//}

//void MW::cutThumbs()
//{
//    {
//    #ifdef ISDEBUG
//    qDebug() << "MW::cutThumbs";
//    #endif
//    }
//    copyOrCutThumbs(false);
//}

//void MW::copyThumbs()
//{
//    {
//    #ifdef ISDEBUG
//    qDebug() << "MW::copyThumbs";
//    #endif
//    }
//    copyOrCutThumbs(true);
//}

//void MW::copyImagesTo()
//{
//    {
//    #ifdef ISDEBUG
//    qDebug() << "MW::copyImagesTo";
//    #endif
//    }
//    copyMoveImages(false);
//}

//void MW::moveImagesTo()
//{
//    {
//    #ifdef ISDEBUG
//    qDebug() << "MW::moveImagesTo";
//    #endif
//    }
//    copyMoveImages(true);
//}

//void MW::copyMoveImages(bool move)
//{
//    {
//    #ifdef ISDEBUG
//    qDebug() << "MW::copyMoveImages";
//    #endif
//    }
//    if (GData::slideShowActive) {
//        slideShow();
//    }
//    imageView->setCursorHiding(false);

//    copyMoveToDialog = new CopyMoveToDialog(this, getSelectedPath(), move);
//    if (copyMoveToDialog->exec()) {
//        if (GData::layoutMode == thumbViewIdx) {
//            if (copyMoveToDialog->copyOp) {
//                copyThumbs();
//            } else {
//                cutThumbs();
//            }

//            pasteThumbs();
//        } else {
//            if (imageView->isNewImage()) {
//                showNewImageWarning(this);
//                if (isFullScreen()) {
//                    imageView->setCursorHiding(true);
//                }

//                return;
//            }

//            QFileInfo fileInfo = QFileInfo(imageView->currentImageFullPath);
//            QString fileName = fileInfo.fileName();
//            QString destFile = copyMoveToDialog->selectedPath + QDir::separator() + fileInfo.fileName();

//            int res = cpMvFile(copyMoveToDialog->copyOp, fileName, imageView->currentImageFullPath,
//                                                    destFile, copyMoveToDialog->selectedPath);

//            if (!res) {
//                QMessageBox msgBox;
//                msgBox.critical(this, tr("Error"), tr("Failed to copy or move image."));
//            } else {
//                if (!copyMoveToDialog->copyOp) {
//                    int currentRow = thumbView->getCurrentRow();
//                    thumbView->thumbViewModel->removeRow(currentRow);
//                    loadCurrentImage(currentRow);
//                }
//            }
//        }
//    }

//    bookmarks->reloadBookmarks();
//    delete(copyMoveToDialog);
//    copyMoveToDialog = 0;

//    if (isFullScreen()) {
//        imageView->setCursorHiding(true);
//    }
//}

//void MW::pasteThumbs()
//{
//    {
//    #ifdef ISDEBUG
//    qDebug() << "MW::pasteThumbs";
//    #endif
//    }
//    if (!copyCutCount)
//        return;

//    QString destDir;
//    if (copyMoveToDialog)
//        destDir = copyMoveToDialog->selectedPath;
//    else {
//        if (QApplication::focusWidget() == bookmarks) {
//            if (bookmarks->currentItem()) {
//                destDir = bookmarks->currentItem()->toolTip(0);
//            }
//        } else {
//            destDir = getSelectedPath();
//        }
//    }

//    if (!isValidPath(destDir)) {
//        QMessageBox msgBox;
//        msgBox.critical(this, tr("Error"), tr("Can not copy or move to ") + destDir);
//        selectCurrentViewDir();
//        return;
//    }

//    bool pasteInCurrDir = (GData::currentViewDir == destDir);

//    QFileInfo fileInfo;
//    if (!GData::copyOp && pasteInCurrDir) {
//        for (int tn = 0; tn < GData::copyCutFileList.size(); ++tn) {
//            fileInfo = QFileInfo(GData::copyCutFileList[tn]);
//            if (fileInfo.absolutePath() == destDir) {
//                QMessageBox msgBox;
//                msgBox.critical(this, tr("Error"), tr("Can not copy or move to the same folder"));
//                return;
//            }
//        }
//    }

//    CpMvDialog *dialog = new CpMvDialog(this);
//    dialog->exec(thumbView, destDir, pasteInCurrDir);
//    if (pasteInCurrDir) {
//        for (int tn = 0; tn < GData::copyCutFileList.size(); ++tn) {
//            thumbView->addThumb(GData::copyCutFileList[tn]);
//        }
//    } else {
//        int row = dialog->latestRow;
//        if (thumbView->thumbViewModel->rowCount()) {
//            if (row >= thumbView->thumbViewModel->rowCount()) {
//                row = thumbView->thumbViewModel->rowCount() -1 ;
//            }

//            thumbView->setCurrentRow(row);
//            thumbView->selectThumbByRow(row);
//        }
//    }

//    QString state = QString((GData::copyOp? tr("Copied") : tr("Moved")) + " " +
//                                tr("%n image(s)", "", dialog->nfiles));
//    setStatus(state);
//    delete(dialog);
//    selectCurrentViewDir();

//    copyCutCount = 0;
//    GData::copyCutIdxList.clear();
//    GData::copyCutFileList.clear();
//    pasteAction->setEnabled(false);

//    thumbView->loadVisibleThumbs();
//    if (GData::isDebug) qDebug() << "MW::pasteThumbs";
//}

// not used
//void MW::loadCurrentImage(int currentRow)
//{
//    {
//    #ifdef ISDEBUG
//    qDebug() << "MW::loadCurrentImage";
//    #endif
//    }
//    bool wrapImageListTmp = GData::wrapImageList;
//    GData::wrapImageList = false;

//    if (GData::isDebug) qDebug() << "MW::loadCurrentImage Enter function - current row =" << currentRow;

//    if (currentRow == thumbView->thumbViewModel->rowCount()) {
////        thumbView->setCurrentRow(currentRow - 1);
//        thumbView->selectThumb(currentRow - 1);

//    }

//    if (thumbView->getNextRow() < 0 && currentRow > 0) {
//        imageView->loadImage(thumbView->thumbViewModel->item(currentRow - 1)->
//                                            data(thumbView->FileNameRole).toString());
//    } else {
//        if (thumbView->thumbViewModel->rowCount() == 0)
//        {
//            hideViewer();
//            refreshThumbs(true);
//            return;
//        }

//        if (currentRow > (thumbView->thumbViewModel->rowCount() - 1))
//            currentRow = thumbView->thumbViewModel->rowCount() - 1;

//        imageView->loadImage(thumbView->thumbViewModel->item(currentRow)->
//                                            data(thumbView->FileNameRole).toString());
//    }

//    GData::wrapImageList = wrapImageListTmp;
//    if (GData::isDebug) qDebug() << "MW::loadCurrentImage Exit function - current row =" << currentRow;
//}

//void MW::setCopyCutActions(bool setEnabled)
//{
//    {
//    #ifdef ISDEBUG
//    qDebug() << "MW::setCopyCutActions";
//    #endif
//    }
//    cutAction->setEnabled(setEnabled);
//    copyAction->setEnabled(setEnabled);
//}

//// do we still need? rgh
//void MW::setDeleteAction(bool setEnabled)
//{
//    {
//    #ifdef ISDEBUG
//    qDebug() << "MW::setDeleteAction";
//    #endif
//    }
//    deleteAction->setEnabled(setEnabled);
//}

//void MW::mouseDoubleClickEvent(QMouseEvent *event)
//{
//    {
//    #ifdef ISDEBUG
//    qDebug() << "MW::mouseDoubleClickEvent";
//    #endif
//    }
//    //	if (interfaceDisabled)
////		return;

//    if (event->button() == Qt::LeftButton) {
////		if (GData::layoutMode == imageViewIdx) {
////			if (GData::reverseMouseBehavior)
////			{
////				fullScreenAct->setChecked(!(fullScreenAct->isChecked()));
////				toggleFullScreen();
////				event->accept();
////			}
////			else if (closeImageAct->isEnabled())
////			{
////				hideViewer();
////				event->accept();
////			}
////		} else {
////			if (QApplication::focusWidget() == imageView->scrlArea) {
////				openOp();
////			}
////		}
//    }
//}

//void MW::mousePressEvent(QMouseEvent *event)
//{
//    {
//    #ifdef ISDEBUG
//    qDebug() << "MW::mousePressEvent";
//    #endif
//    }
    //    if (interfaceDisabled)
    //        return;

    //    if (GData::layoutMode == imageViewIdx) {
    //        if (event->button() == Qt::MiddleButton) {

    //            if (event->modifiers() == Qt::ShiftModifier) {
    //                origZoom();
    //                event->accept();
    //                return;
    //            }
    //            if (event->modifiers() == Qt::ControlModifier) {
    //                resetZoom();
    //                event->accept();
    //                return;
    //            }

    //            if (GData::reverseMouseBehavior && closeImageAct->isEnabled()) {
    //                hideViewer();
    //                event->accept();
    //            } else {
    //                fullScreenAction->setChecked(!(fullScreenAction->isChecked()));
    //                toggleFullScreen();
    //                event->accept();
    //            }
    //        }
    //    } else {
    //        if (QApplication::focusWidget() == imageView->scrlArea) {
    //            if (event->button() == Qt::MiddleButton && GData::reverseMouseBehavior) {
    //                openOp();
    //            }
    //        }
    //    }
//}

//void MW::setFsDockVisibility()
//{
//    {
//    #ifdef ISDEBUG
//    qDebug() << "MW::setFsDockVisibility";
//    #endif
//    }
//    if (GData::layoutMode != imageViewIdx) {
//        GData::fsDockVisible = fsDock->isVisible();
//    }
//}

//void MW::setBmDockVisibility()
//{
//    {
//    #ifdef ISDEBUG
//    qDebug() << "MW::setBmDockVisibility";
//    #endif
//    }
//    if (GData::layoutMode != imageViewIdx) {
//        GData::bmDockVisible = bmDock->isVisible();
//    }
//}

//void MW::setTagsDockVisibility()
//{
//    {
//    #ifdef ISDEBUG
//    qDebug() << "MW::setTagsDockVisibility";
//    #endif
//    }
//    if (GData::layoutMode != imageViewIdx) {
//        GData::tagsDockVisible = tagsDock->isVisible();
//    }
//}

//void MW::setIiDockVisibility()
//{
//    {
//    #ifdef ISDEBUG
//    qDebug() << "MW::setIiDockVisibility";
//    #endif
//    }
//    if (GData::layoutMode != imageViewIdx) {
//        GData::iiDockVisible = iiDock->isVisible();
//    }
//}

//void MW::setPvDockVisibility()
//{
//    {
//    #ifdef ISDEBUG
//    qDebug() << "MW::setPvDockVisibility";
//    #endif
//    }
//    if (GData::layoutMode != imageViewIdx) {
//        GData::pvDockVisible = thumbDock->isVisible();
//    }
//}

//void MW::showBusyStatus(bool busy)
//{
//    {
//    #ifdef ISDEBUG
//    qDebug() << "MW::showBusyStatus";
//    #endif
//    }
//    static int busyStatus = 0;

//    if (busy) {
//        ++busyStatus;
//    } else {
//        --busyStatus;
//    }

//    if (busyStatus > 0) {
//        busyMovie->start();
//        busyLabel->setVisible(true);
//    } else {
//        busyLabel->setVisible(false);
//        busyMovie->stop();
//        busyStatus = 0;
//    }
//}

//void MW::loadImagefromThumb(const QModelIndex &idx)
//{
//    {
//    #ifdef ISDEBUG
//    qDebug() << "MW::loadImagefromThumb";
//    #endif
//    }
////    thumbView->setCurrentRow(idx.row());
//    showViewer();
//    imageView->loadImage(thumbView->thumbViewModel->item(idx.row())->data(thumbView->FileNameRole).toString());
//}

//void MW::setViewerKeyEventsEnabled(bool enabled)
//{
//    {
//    #ifdef ISDEBUG
//    qDebug() << "MW::setViewerKeyEventsEnabled";
//    #endif
//    }
//    nextThumbAction->setEnabled(enabled);
//    prevThumbAction->setEnabled(enabled);
//    upThumbAction->setEnabled(enabled);
//    downThumbAction->setEnabled(enabled);
//}

//void MW::updateIndexByViewerImage()
//{
//    {
//    #ifdef ISDEBUG
//    qDebug() << "MW::updateIndexByViewerImage";
//    #endif
//    }
////    if (thumbView->thumbViewFilter->rowCount() > 0 &&

////        thumbView->setCurrentIndexByName(imageView->currentImageFullPath))
////    {
////        thumbView->selectCurrentIndex();
////    }
//    thumbView->selectThumb(imageView->currentImageFullPath);
//}

//void MW::hideViewer()
//{
//    {
//    #ifdef ISDEBUG
//    qDebug() << "MW::hideViewer";
//    #endif
//    }
//    if (GData::exitInsteadOfClose) {
//        close();
//        return;
//    }

////    showBusyStatus(true);

//    if (isFullScreen())	{
//        showNormal();
//        if (shouldMaximize) {
//            showMaximized();
//        }
//        imageView->setCursorHiding(false);
//    }

////    QApplication::processEvents();

//    GData::layoutMode = thumbViewIdx;
//    mainLayout->removeWidget(imageView);

//    imageViewContainer->addWidget(imageView);
//    setDocksVisibility(true);
//    while (QApplication::overrideCursor()) {
//        QApplication::restoreOverrideCursor();
//    }

//    if (GData::slideShowActive) {
//        slideShow();
//    }

//    thumbView->setResizeMode(QListView::Fixed);
//    thumbView->setVisible(true);
//    for (int i = 0; i <= 100 && qApp->hasPendingEvents(); ++i) {
////        QApplication::processEvents();
//    }
////    setAppWindowTitle();

//    fsDock->setMaximumHeight(QWIDGETSIZE_MAX);
//    bmDock->setMaximumHeight(QWIDGETSIZE_MAX);
////    tagsDock->setMaximumHeight(QWIDGETSIZE_MAX);
//    iiDock->setMaximumHeight(QWIDGETSIZE_MAX);
//    thumbDock->setMaximumHeight(QWIDGETSIZE_MAX);
//    fsDock->setMaximumWidth(QWIDGETSIZE_MAX);
//    bmDock->setMaximumWidth(QWIDGETSIZE_MAX);
////    tagsDock->setMaximumWidth(QWIDGETSIZE_MAX);
//    iiDock->setMaximumWidth(QWIDGETSIZE_MAX);
//    thumbDock->setMaximumWidth(QWIDGETSIZE_MAX);

////    if (thumbView->thumbViewModel->rowCount() > 0) {
////        if (thumbView->setCurrentIndexByName(imageView->currentImageFullPath))
////            thumbView->selectCurrentIndex();
////    }
//    thumbView->selectThumb(imageView->currentImageFullPath);
//    thumbView->setResizeMode(QListView::Adjust);

//    if (needThumbsRefresh) {
//        needThumbsRefresh = false;
////        refreshThumbs(true);
//    } else {
////        thumbView->loadVisibleThumbs();
//    }

//    thumbView->setFocus(Qt::OtherFocusReason);
////    showBusyStatus(false);
//    setContextMenuPolicy(Qt::DefaultContextMenu);

//    updateActions();
//}

//void MW::newImage()
//{
//    {
//    #ifdef ISDEBUG
//    qDebug() << "MW::newImage";
//    #endif
//    }
//    if (GData::layoutMode == thumbViewIdx)
//        showViewer();

//    imageView->loadImage("");
//}

//// not being used
//void MW::showViewer()
//{
//    {
//    #ifdef ISDEBUG
//    qDebug() << "MW::showViewer";
//    #endif
//    }
//    if (GData::layoutMode == thumbViewIdx) {
//        GData::layoutMode = imageViewIdx;
//        GData::appSettings->setValue("Geometry", saveGeometry());
//        GData::appSettings->setValue("WindowState", saveState());

//        imageViewContainer->removeWidget(imageView);
//        mainLayout->addWidget(imageView);
//        imageView->setVisible(true);
//        thumbView->setVisible(false);
//        setDocksVisibility(false);
//        if (GData::isFullScreen == true) {
//            shouldMaximize = isMaximized();
//            showFullScreen();
//            imageView->setCursorHiding(true);
//        }
//        imageView->adjustSize();

//        updateActions();
//    }
//}

//void MW::goBottom()
//{
//    {
//    #ifdef ISDEBUG
//    qDebug() << "MW::goBottom";
//    #endif
//    }
//    thumbView->scrollToBottom();
//}

//void MW::goTop()
//{
//    {
//    #ifdef ISDEBUG
//    qDebug() << "MW::goTop";
//    #endif
//    }
//    thumbView->scrollToTop();
//}

//void MW::setAppWindowTitle()
//{
//    {
//    #ifdef ISDEBUG
//    qDebug() << "MW::setAppWindowTitle";
//    #endif
//    }

//    setWindowTitle(GData::appName + ":  " + GData::currentViewDir);
//}

//void MW::setInterfaceEnabled(bool enable)
//{
//    {
//    #ifdef ISDEBUG
//    qDebug() << "MW::setInterfaceEnabled";
//    #endif
//    }
//    // actions
////    closeImageAct->setEnabled(enable);
//    nextThumbAction->setEnabled(enable);
//    prevThumbAction->setEnabled(enable);
//    firstThumbAction->setEnabled(enable);
//    lastThumbAction->setEnabled(enable);
//    randomImageAction->setEnabled(enable);
//    slideShowAction->setEnabled(enable);
////    copyToAction->setEnabled(enable);
////    moveToAction->setEnabled(enable);
//    prefAction->setEnabled(enable);
//    openAction->setEnabled(enable);

//    // other
//    thumbView->setEnabled(enable);
//    fsTree->setEnabled(enable);
//    bookmarks->setEnabled(enable);
////    thumbView->imageTags->setEnabled(enable);
//    menuBar()->setEnabled(enable);
//    interfaceDisabled = !enable;

//    if (enable) {
//        if (isFullScreen()) {
//            imageView->setCursorHiding(true);
//        }
//        updateActions();
//    } else {
//        imageView->setCursorHiding(false);
//    }
//}

//// rgh review if required, test with bookmark removal
//void MW::updateActions()
//{
//    {
//    #ifdef ISDEBUG
//    qDebug() << "MW::updateActions";
//    #endif
//    }
//    if (GData::isDebug) qDebug() << "MW::updateActions - focus widget =" << QApplication::focusWidget();
//    if (QApplication::focusWidget() == thumbView) {
////		setCopyCutActions(thumbView->selectionModel()->selectedIndexes().size());
////		setDeleteAction(thumbView->selectionModel()->selectedIndexes().size());
//    } else if (QApplication::focusWidget() == bookmarks) {
////        setCopyCutActions(false);
////        setDeleteAction(bookmarks->selectionModel()->selectedIndexes().size());
//    } else if (QApplication::focusWidget() == fsTree) {
////		setCopyCutActions(false);
////		setDeleteAction(fsTree->selectionModel()->selectedIndexes().size());
//    } else if (GData::layoutMode == imageViewIdx || QApplication::focusWidget() == imageView->scrlArea) {
////		setCopyCutActions(false);
////		setDeleteAction(true);
//    } else {
////        setCopyCutActions(false);
////        setDeleteAction(false);
//    }

//}

//void MW::setDocksVisibility(bool visible)
//{
//    {
//    #ifdef ISDEBUG
//    qDebug() << "MW::setDocksVisibility";
//    #endif
//    }
//    if (!visible) {
//        fsDock->setMaximumHeight(fsDock->height());
//        bmDock->setMaximumHeight(bmDock->height());
//        iiDock->setMaximumHeight(iiDock->height());
//        thumbDock->setMaximumHeight(thumbDock->height());
//        fsDock->setMaximumWidth(fsDock->width());
//        bmDock->setMaximumWidth(bmDock->width());
//        iiDock->setMaximumWidth(iiDock->width());
//        thumbDock->setMaximumWidth(thumbDock->width());
//    }

//    fsDock->setVisible(visible? G::isFolderDockVisible : false);
//    bmDock->setVisible(visible? G::isFavsDockVisible : false);
//    iiDock->setVisible(visible? G::isMetadataDockVisible : false);
//    thumbDock->setVisible(visible? G::isThumbDockVisible : false);

//    menuBar()->setVisible(visible);
//    menuBar()->setDisabled(!visible);
//    statusBar()->setVisible(visible);

//    setContextMenuPolicy(Qt::PreventContextMenu);
//}

