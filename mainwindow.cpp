
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
    isStressTest = false;

    G::appName = "Winnow";
    G::isTimer = false;

    // Global timer for testing
    #ifdef ISDEBUG
    //    GData::t.start();
    #endif

    this->setWindowTitle("Winnow");
    this->setObjectName("WinnowMW");

//    QGuiApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    isInitializing = true;
    isSlideShowActive = false;
    maxThumbSpaceHeight = 160 + qApp->style()->pixelMetric(QStyle::PM_ScrollBarExtent);
    workspaces = new QList<workspaceData>;
    recentFolders = new QStringList;
    popUp = new PopUp;

    // platform specific settings
    setupPlatform();

/* Initialization process
*******************************************************************************
Persistant settings are saved between sessions using QSettings. Persistant
settings form two categories at runtime: action settings and preference
settings. Action settings are boolean and maintained by the action item
setChecked value.  Preferences can be anything and are maintained as public
variables in MW (this class) and managed in the prefDlg class.

• Load QSettings
• Set default values if no settings available (first time run)
• Set preference settings including
    • General (previous folder etc)
    • Slideshow
    • Cache
    • Full screen docks visible
    • List of external apps
    • Bookmarks
    • Recent folders
    • Workspaces
• Create actions and set checked based on persistant values from QSetting
• Create bookmarks with persistant values from QSettings
• Update external apps with persistant values from QSettings
• Load shortcuts (based on being able to edit shortcuts)
• Execute updateState function to implement all persistant state settings

*/

    // structure to hold persistant settings between sessions
    setting = new QSettings("Winnow", "winnow_100");

    createCentralWidget();      // req'd by ImageView, CompareView
    createFilterView();         // req'd by DataModel
    createDataModel();          // dependent on FilterView, creates Metadata
    createThumbView();          // dependent on QSetting, filterView
    createTableView();          // dependent on centralWidget
    createSelectionModel();     // dependent on ThumbView, ImageView
    createInfoView();           // dependent on metadata
    createCaching();            // dependent on dm, metadata, thumbView
    createImageView();          // dependent on centralWidget
    createCompareView();        // dependent on centralWidget
    createFSTree();
    createBookmarks();          // dependent on loadSettings
    createDocks();              // dependent on FSTree, Bookmarks, ThumbView, Metadata, InfoView
    createStatusBar();

    loadWorkspaces();           // req'd by actions and menu
    createActions();            // dependent on above
    createMenus();              // dependent on creatActions and loadSettings

    updateExternalApps();       // dependent on createActions
    handleStartupArgs();

    bool isSettings = loadSettings();//dependent on bookmarks and actions
    loadShortcuts(true);            // dependent on createActions

    if (!resetSettings) {
        restoreGeometry(setting->value("Geometry").toByteArray());
        restoreState(setting->value("WindowState").toByteArray());
    }

    setupCentralWidget();
    createAppStyle();

    if (isSettings && !resetSettings) updateState();
    else {
        defaultWorkspace();
        QString welcomeText = "<h3>Welcome to Winnow</h3>"
            + tr("<p>To view images select a folder from the Folders dock or from the menu File > Open Folder</p>");
        centralLabel->setAlignment(Qt::AlignCenter);
        centralLabel->setText(welcomeText);
        centralLayout->setCurrentIndex(0);
    }

    // process the persistant folder if available
    folderSelectionChange();
}

void MW::setupPlatform()
{
    {
    #ifdef ISDEBUG
    qDebug() << "MW::setupPlatform";
    #endif
    }
    #ifdef Q_OS_LINIX
        G::devicePixelRatio = 1;
    #endif
    #ifdef Q_OS_WIN
        G::devicePixelRatio = 1;
        setWindowIcon(QIcon(":/images/winnow.png"));
    #endif
    #ifdef Q_OS_MAC
        G::devicePixelRatio = 2;
        setWindowIcon(QIcon(":/images/winnow.icns"));
    #endif
}

//   EVENT HANDLERS

void MW::keyPressEvent(QKeyEvent *event)
{
//    qDebug() << "MW::keyPressEvent" << event;
//    QMainWindow::keyPressEvent(event);
}

void MW::keyReleaseEvent(QKeyEvent *event)
{
//    qDebug() << "MW::keyReleaseEvent" << event;
//    QMainWindow::keyReleaseEvent(event);
}

bool MW::eventFilter(QObject *obj, QEvent *event)
{
/*
Trap the thumbDock resize event and dynamically resize thumbnails as the
thumbdock is resized by the user when:
   - dock area is top or bottom
   - height change of dock changes
*/
    if (event->type() == QEvent::Resize && obj == thumbDock &&
      !thumbDock->isFloating() && !isInitializing)
    {
        if (dockWidgetArea(thumbDock) == Qt::BottomDockWidgetArea ||
            dockWidgetArea(thumbDock) == Qt::TopDockWidgetArea)
        {
            static int oldHt = 0;
            QResizeEvent *resizeEvent = static_cast<QResizeEvent*>(event);
            int newHt = resizeEvent->size().height();
            if (newHt != oldHt) {
//                qDebug() << "\nMW::eventFilter dock area\n"
//                         << "***  thumbView Ht =" << thumbView->height()
//                         << "thumbSpace Ht =" << thumbView->getThumbCellSize().height()
//                         << "thumbHeight =" << thumbView->thumbHeight << "\n"
//                         << "thumbDock Ht =" << thumbDock->height();
                 thumbView->thumbsFit(dockWidgetArea(thumbDock));
                 oldHt = newHt;
             }
        }
    }
    return QWidget::eventFilter(obj, event);
}

bool MW::event(QEvent *event)
{
    /* Previous stuff no longer req'd
    qDebug() << "MW::event" << event;

     Consume arrow key events when updating imageView to prevent runon
    if (event->type() == QEvent::ShortcutOverride && imageView->isBusy) {
        QKeyEvent *keyEvent = static_cast<QKeyEvent*>(event);
        if (keyEvent->key() == Qt::Key_Right ||
            keyEvent->key() == Qt::Key_Left ||
            keyEvent->key() == Qt::Key_Down ||
            keyEvent->key() == Qt::Key_Up)
        {
            keyEvent->accept();     // consume keystroke without doing anything
            return true;
        }

    }

    if (event->type() == QEvent::KeyRelease) {
        QKeyEvent *keyEvent = static_cast<QKeyEvent*>(event);

         Grid or Loupe views
        if (keyEvent->key() == Qt::Key_G ||
            keyEvent->key() == Qt::Key_E ||
            keyEvent->key() == Qt::Key_Return)
        {
//            qDebug() << "\n" << event << "\n";
            thumbView->selectThumb(thumbView->currentIndex());
        }
         */

        // slideshow
    if (event->type() == QEvent::KeyRelease) {
        QKeyEvent *keyEvent = static_cast<QKeyEvent*>(event);

        if (keyEvent->key() == Qt::Key_Escape) {
            if (isSlideShowActive) slideShow();     // toggles slideshow off
        }
        // change delay 1 - 9 seconds
        if (isSlideShowActive) {
            int n = keyEvent->key() - 48;
            if (n > 0 && n <=9) {
                slideShowDelay = n;
                slideShowTimer->setInterval(n * 1000);
                QString msg = "Reset slideshow interval to ";
                msg += QString::number(n) + " seconds";
                popUp->showPopup(this, msg, 1000, 0.5);
            }
        }
    }
    return QMainWindow::event(event);
}

// Do we need this?  rgh
void MW::handleStartupArgs()
{
    {
    #ifdef ISDEBUG
    qDebug() << "MW::handleStartupArgs";
    #endif
    }
}

/**************************************************************************
 PROGRAM FLOW EVENT DISPATCH

 A new folder is selected which triggers folderSelectionChange:

 1.  A list of all eligible image files in the folder is generated in
     DataModel (dm).

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
        qDebug() << "\n\nMW::folderSelectionChange"
                    "\n*************************************************************************";
    #endif
    }
    // Stop any threads that might be running.
    thumbCacheThread->stopThumbCache();
//    QThread::msleep(2000);        // no effect on crash
//    qDebug() << "thumbCacheThread->isRunning" << thumbCacheThread->isRunning();
    imageCacheThread->stopImageCache();
    metadataCacheThread->stopMetadateCache();

    G::isNewFolderLoaded = false;

    // stop slideshow if a new folder is selected
    if (isSlideShowActive && !isStressTest) slideShow();

    // if previously in compare mode switch to loupe mode
    if (asCompareAction->isChecked()) {
        asCompareAction->setChecked(false);
        asLoupeAction->setChecked(true);
        updateState();
    }

    QString dirPath;
    QDir testDir;
    if (isInitializing) {
        isInitializing = false;
        if (rememberLastDir) {
            dirPath = lastDir;
            // is drive still available and valid?
            QStorageInfo testStorage;
            testStorage.setPath(dirPath);
            if (!testStorage.isValid()) return;
            testDir.setPath(dirPath);
            // deleted or renamed or corrupted folder
            if (!testDir.exists() || !testDir.isReadable()) return;
            fsTree->setCurrentIndex(fsTree->fsModel->index(dirPath));
        }
    }
    else {
        dirPath = getSelectedPath();
        testDir.setPath(dirPath);
//        qDebug() << "MW::folderSelectionChange ^^^^^^^^^^^ " << dirPath;
    }

    if (!testDir.exists()) {
        QMessageBox msgBox;
        msgBox.critical(this, tr("Error"), tr("The folder does not exist or is not available"));
        return;
    }

    // check if unmounted USB drive
    if (!testDir.isReadable()) {
        QMessageBox msgBox;
        msgBox.critical(this, tr("Error"), tr("The folder is not readable"));
        return;
    }

    // ignore if present folder is rechosen
    if (dirPath == currentViewDir) {
        if (!subFoldersAction->isChecked()) return;
    }
    else {
        currentViewDir = dirPath;
        // cannot accidentally see all subfolders - must set after selecting folder
        subFoldersAction->setChecked(false);
    }

    // add to recent folders
    addRecentFolder(currentViewDir);

    /* We do not want to update the imageCache while metadata is still being
    loaded.  The imageCache update is triggered in fileSelectionChange,
    which is also executed when the change file event is fired. */
    metadataLoaded = false;

    /* Need to gather directory file info first (except icon/thumb) which is
    loaded by loadThumbCache.  If no images in new folder then cleanup and exit.
    MW::fileSelectionChange triggered by DataModel->load         */
    if (!dm->load(currentViewDir, subFoldersAction->isChecked())) {
        // MW::fileSelectionChange triggered by dm->load
        updateStatus(false, "No images in this folder");
        infoView->clearInfo();
        cacheLabel->setVisible(false);
        return;
    }
    thumbView->selectThumb(0);
    thumbView->sortThumbs(1, false);

    // no ratings or label color classes set yet so hide editsLabel
    imageView->editsLabel->setVisible(false);

     /* Must load metadata first, as it contains the file offsets and lengths
     for the thumbnail and full size embedded jpgs and the image width and
     height, req'd in imageCache to manage cache max size. Triggers
     loadThumbCache and loadImageCache when finished metadata cache. The thumb
     cache includes icons (thumbnails) for all the images in the folder. The
     image cache holds as many full size images in memory as possible. */
     loadMetadataCache();
}

void MW::fileSelectionChange(QModelIndex current, QModelIndex previous)
//void MW::fileSelectionChange()
{
/* Triggered when file selection changes (folder change selects new image, so
it also triggers this function). The new image is loaded, the pick status is
updated and the infoView metadata is updated. Update the imageCache if
necessary. The imageCache will not be updated if triggered by folderSelectionChange.
*/
    {
    #ifdef ISDEBUG
    qDebug() << "MW::fileSelectionChange";
    #endif
    }
    // rgh check if selection is working properly
//    QModelIndexList selected = thumbView->selectionModel()->selectedIndexes();
    QModelIndexList selected = selectionModel->selectedIndexes();

    // user clicks outside thumb but inside treeView dock
    if (selected.isEmpty()) return;

    // currentIndex must be column 0
    QModelIndex currentIndex = dm->sf->index(current.row(), 0);
    thumbView->setCurrentIndex(currentIndex);

    // sync thumbView delegate current item
    thumbView->thumbViewDelegate->currentIndex = currentIndex;

    // update imageView, use cache if image loaded, else read it from file
    QString fPath = currentIndex.data(G::FileNameRole).toString();
    if (imageView->loadImage(currentIndex, fPath)) {
        if (G::isThreadTrackingOn) qDebug()
            << "MW::fileSelectionChange - loaded image file " << fPath;
        updatePick();
        updateRating();
        updateColorClass();
        infoView->updateInfo(fPath);
    }

    /* If the metadataCache is finished then update the imageCache,
     the cache up-to-date with the current image selection. */
    if (metadataLoaded) {
        imageCacheThread->updateImageCache(fPath);
    }

/*   Other stuff tried and reported ...
 *
    qDebug() << "\n\nisNewFolderLoaded" << G::isNewFolderLoaded
             << "\nSelected.count:" << selected.count()
             << "\nSelected List: " << selected
             << "\nCurrent:     " << current
             << "\nPrevious:    " << previous
             << "\nSelection contains previous:"
             << selected.contains(previous) << "\n";

     selection is extended - do not change current index
        if(selected.count() > 1 && G::isNewFolderLoaded) {
    if(selected.count() > 1) {
        if(selected.contains(previous)) {
            thumbView->setCurrentIndex(previous);
            return;
        }
    }


     if top/bottom dock resize dock height if scrollbar is/not visible
    qDebug() << "\nMW::fileSelectionChange\n"
             << "***  thumbView Ht =" << thumbView->height()
             << "thumbSpace Ht =" << thumbView->getThumbCellSize().height()
             << "thumbHeight =" << thumbView->thumbHeight << "\n";

    qDebug() << "thumbView->thumbFileInfoList:";
    foreach (QFileInfo f, thumbView->thumbFileInfoList) {
        qDebug() << f.filePath();
    }
    qDebug() << "thumbViewFilter row = 0"
             << thumbView->thumbViewFilter->index(0,0).data(Qt::DisplayRole);

     debugging
    thumbView->reportThumb();
    */
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
/*
The thumbnail cache is loaded after the metadata for all the images in the
folder(s) has been loaded.  This function is called from the metadataCacheThread.
*/
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
/*
This is called from the metadataCacheThread after all the metadata has been
loaded.  It is called immediately after loadThumbCache so both the thumbCache and
full size imageCache are running simultaneously.  The imageCache loads images
until the assigned amount of memory has been consumed or all the images are
cached.
 */
    {
    #ifdef ISDEBUG
    qDebug() << "MW::loadImageCache";
    #endif
    }
    metadataLoaded = true;
    // now that metadata is loaded populate the data model
    dm->addMetadata();
    // have to wait for the data before resize table columns
    tableView->resizeColumnsToContents();
    tableView->setColumnWidth(G::PathColumn, 24+8);
    QModelIndexList indexesList = thumbView->selectionModel()->selectedIndexes();

    QString fPath = indexesList.first().data(G::FileNameRole).toString();
    // imageChacheThread checks if already running and restarts
    imageCacheThread->initImageCache(dm->imageFilePathList, cacheSizeMB,
        isShowCacheStatus, cacheStatusWidth, cacheWtAhead, isCachePreview,
        cachePreviewWidth, cachePreviewHeight);
    imageCacheThread->updateImageCache(fPath);
}

void MW::loadFilteredImageCache()
{
    {
    #ifdef ISDEBUG
    qDebug() << "MW::loadFilteredImageCache";
    #endif
    }
    qDebug() << "MW::loadFilteredImageCache";
    QModelIndex idx = thumbView->currentIndex();
    QString fPath = idx.data(G::FileNameRole).toString();
    thumbView->selectThumb(idx);

    dm->updateImageList();

    // imageChacheThread checks if already running and restarts
    imageCacheThread->initImageCache(dm->imageFilePathList, cacheSizeMB,
        isShowCacheStatus, cacheStatusWidth, cacheWtAhead, isCachePreview,
        cachePreviewWidth, cachePreviewHeight);
    imageCacheThread->updateImageCache(fPath);
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
    if (isInitializing) return;

    if (!QDir().exists(currentViewDir))
    {
        currentViewDir = "";
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
    connect(openAction, SIGNAL(triggered()), this, SLOT(openFolder()));

    openWithMenu = new QMenu(tr("Open With..."));
    openWithMenuAction = new QAction(tr("Open With..."), this);
    openWithMenuAction->setObjectName("openWithMenu");
    openWithMenuAction->setMenu(openWithMenu);

    chooseAppAction = new QAction(tr("Manage External Applications"), this);
    chooseAppAction->setObjectName("chooseApp");
    connect(chooseAppAction, SIGNAL(triggered()), this, SLOT(chooseExternalApp()));
//    chooseAppAction->setMenu(openWithMenu);

    newAppAction = new QAction(tr("New app"), this);
    newAppAction->setObjectName("newApp");
//    connect(newAppAction, SIGNAL(triggered()), this, SLOT(chooseExternalApp()));

    deleteAppAction = new QAction(tr("Delete app"), this);
    deleteAppAction->setObjectName("deleteApp");
//    connect(deleteAppAction, SIGNAL(triggered()), this, SLOT(chooseExternalApp()));

    recentFoldersMenu = new QMenu(tr("Recent folders..."));
    recentFoldersAction = new QAction(tr("Recent folders..."), this);
    recentFoldersAction->setObjectName("recentFoldersAction");
    recentFoldersAction->setMenu(recentFoldersMenu);

    // general connection to handle invoking new recent folders
    // MacOS will not allow runtime menu insertions.  Cludge workaround
    // add 10 dummy menu items and then hide until use.
    int n = recentFolders->count();
    for (int i = 0; i < maxRecentFolders; i++) {
        QString name;
        QString objName = "";
        if (i < n) {
            name = recentFolders->at(i);
            objName = "recentFolder" + QString::number(i);
        }
        else name = "Future recent folder" + QString::number(i);
        recentFolderActions.append(new QAction(name, this));
        if (i < n) {
//            recentFolderActions.at(i)->setShortcut(QKeySequence("Ctrl+" + QString::number(i)));
            recentFolderActions.at(i)->setObjectName(objName);
            recentFolderActions.at(i)->setText(name);
            recentFolderActions.at(i)->setVisible(true);
        }
        if (i >= n) recentFolderActions.at(i)->setVisible(false);
//        recentFolderActions.at(i)->setShortcut(QKeySequence("Ctrl+" + QString::number(i)));
    }
    addActions(recentFolderActions);

    revealFileAction = new QAction(tr("Reveal in finder/explorer"), this);
    revealFileAction->setObjectName("openInFinder");
    connect(revealFileAction, SIGNAL(triggered()),
        this, SLOT(revealFile()));

    subFoldersAction = new QAction(tr("Include Sub-folders"), this);
    subFoldersAction->setObjectName("subFolders");
    subFoldersAction->setCheckable(true);
    subFoldersAction->setChecked(false);
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
    runDropletAction->setShortcut(QKeySequence("A"));
    connect(runDropletAction, SIGNAL(triggered()), this, SLOT(test()));
//    connect(runDropletAction, SIGNAL(triggered()), this, SLOT(reportState()));
//    connect(runDropletAction, SIGNAL(triggered()), this, SLOT(runDroplet()));

    reportMetadataAction = new QAction(tr("Report Metadata"), this);
    reportMetadataAction->setObjectName("reportMetadata");
    connect(reportMetadataAction, SIGNAL(triggered()),
            this, SLOT(reportMetadata()));
//    connect(reportMetadataAction, SIGNAL(triggered()),
//            this, SLOT(thumbTable()));

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
    connect(toggleFilterPickAction, SIGNAL(triggered(bool)),
            thumbView, SLOT(toggleFilterPick(bool)));
//    connect(toggleFilterPickAction, SIGNAL(triggered()), this, SLOT(toggleFilterPick()));

    // Place keeper for now
    copyImagesAction = new QAction(tr("Copy to clipboard"), this);
    copyImagesAction->setObjectName("copyImages");
    copyImagesAction->setShortcut(QKeySequence("Ctrl+C"));
    connect(copyImagesAction, SIGNAL(triggered()), thumbView, SLOT(copyThumbs()));

    rate0Action = new QAction(tr("Clear rating"), this);
    rate0Action->setObjectName("Rate0");
    connect(rate0Action, SIGNAL(triggered()), this, SLOT(setRating()));

    rate1Action = new QAction(tr("Set rating to 1"), this);
    rate1Action->setObjectName("Rate1");
    connect(rate1Action, SIGNAL(triggered()), this, SLOT(setRating()));

    rate2Action = new QAction(tr("Set rating to 2"), this);
    rate2Action->setObjectName("Rate2");
    connect(rate2Action, SIGNAL(triggered()), this, SLOT(setRating()));

    rate3Action = new QAction(tr("Set rating to 3"), this);
    rate3Action->setObjectName("Rate3");
    connect(rate3Action, SIGNAL(triggered()), this, SLOT(setRating()));

    rate4Action = new QAction(tr("Set rating to 4"), this);
    rate4Action->setObjectName("Rate4");
    connect(rate4Action, SIGNAL(triggered()), this, SLOT(setRating()));

    rate5Action = new QAction(tr("Set rating to 5"), this);
    rate5Action->setObjectName("Rate5");
    connect(rate5Action, SIGNAL(triggered()), this, SLOT(setRating()));

    label0Action = new QAction(tr("Clear label colour"), this);
    label0Action->setObjectName("Label0");
    connect(label0Action, SIGNAL(triggered()), this, SLOT(setColorClass()));

    label1Action = new QAction(tr("Set label to Red"), this);
    label1Action->setObjectName("Label1");
    connect(label1Action, SIGNAL(triggered()), this, SLOT(setColorClass()));

    label2Action = new QAction(tr("Set label to Yellow"), this);
    label2Action->setObjectName("Label2");
    connect(label2Action, SIGNAL(triggered()), this, SLOT(setColorClass()));

    label3Action = new QAction(tr("Set label to Green"), this);
    label3Action->setObjectName("Label3");
    connect(label3Action, SIGNAL(triggered()), this, SLOT(setColorClass()));

    label4Action = new QAction(tr("Set label to Blue"), this);
    label4Action->setObjectName("Label4");
    connect(label4Action, SIGNAL(triggered()), this, SLOT(setColorClass()));

    label5Action = new QAction(tr("Set label to Purple"), this);
    label5Action->setObjectName("Label5");
    connect(label5Action, SIGNAL(triggered()), this, SLOT(setColorClass()));

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
    connect(nextThumbAction, SIGNAL(triggered()), thumbView, SLOT(selectNext()));

    prevThumbAction = new QAction(tr("Previous"), this);
    prevThumbAction->setObjectName("prevImage");
    connect(prevThumbAction, SIGNAL(triggered()), thumbView, SLOT(selectPrev()));

    upThumbAction = new QAction(tr("Move Up"), this);
    upThumbAction->setObjectName("moveUp");
    connect(upThumbAction, SIGNAL(triggered()), thumbView, SLOT(selectUp()));

    downThumbAction = new QAction(tr("Move Down"), this);
    downThumbAction->setObjectName("moveDown");
    connect(downThumbAction, SIGNAL(triggered()), thumbView, SLOT(selectDown()));

    firstThumbAction = new QAction(tr("First"), this);
    firstThumbAction->setObjectName("firstImage");
    connect(firstThumbAction, SIGNAL(triggered()), thumbView, SLOT(selectFirst()));

    lastThumbAction = new QAction(tr("Last"), this);
    lastThumbAction->setObjectName("lastImage");
    connect(lastThumbAction, SIGNAL(triggered()), thumbView, SLOT(selectLast()));

    // Not a menu item - used by slide show
    randomImageAction = new QAction(tr("Random"), this);
    randomImageAction->setObjectName("randomImage");
    connect(randomImageAction, SIGNAL(triggered()), thumbView, SLOT(selectRandom()));

    nextPickAction = new QAction(tr("Next Pick"), this);
    nextPickAction->setObjectName("nextPick");
    connect(nextPickAction, SIGNAL(triggered()), thumbView, SLOT(selectNextPick()));

    prevPickAction = new QAction(tr("Previous Pick"), this);
    prevPickAction->setObjectName("prevPick");
    connect(prevPickAction, SIGNAL(triggered()), thumbView, SLOT(selectPrevPick()));

    // Filters

    uncheckAllAction = new QAction(tr("Uncheck all filters"), this);
    uncheckAllAction->setObjectName("uncheckAll");
    connect(uncheckAllAction, SIGNAL(triggered()), filters, SLOT(uncheckAllFilters()));

    expandAllAction = new QAction(tr("Expand all filters"), this);
    expandAllAction->setObjectName("expandAll");
    connect(expandAllAction, SIGNAL(triggered()), filters, SLOT(expandAllFilters()));

    collapseAllAction = new QAction(tr("Collapse all filters"), this);
    collapseAllAction->setObjectName("collapseAll");
    connect(collapseAllAction, SIGNAL(triggered()), filters, SLOT(collapseAllFilters()));

    // Sort Menu

    sortFileNameAction = new QAction(tr("Sort by file name"), this);
    sortFileNameAction->setCheckable(true);
    connect(sortFileNameAction, SIGNAL(triggered()), this, SLOT(sortThumbnails()));

    sortFileTypeAction = new QAction(tr("Sort by file type"), this);
    sortFileTypeAction->setCheckable(true);
    connect(sortFileTypeAction, SIGNAL(triggered()), this, SLOT(sortThumbnails()));

    sortFileSizeAction = new QAction(tr("Sort by file size"), this);
    sortFileSizeAction->setCheckable(true);
    connect(sortFileSizeAction, SIGNAL(triggered()), this, SLOT(sortThumbnails()));

    sortCreateAction = new QAction(tr("Sort by created time"), this);
    sortCreateAction->setCheckable(true);
    connect(sortCreateAction, SIGNAL(triggered()), this, SLOT(sortThumbnails()));

    sortModifyAction = new QAction(tr("Sort by last modified time"), this);
    sortModifyAction->setCheckable(true);
    connect(sortModifyAction, SIGNAL(triggered()), this, SLOT(sortThumbnails()));

    sortPickAction = new QAction(tr("Sort by picked status"), this);
    sortPickAction->setCheckable(true);
    connect(sortPickAction, SIGNAL(triggered()), this, SLOT(sortThumbnails()));

    sortLabelAction = new QAction(tr("Sort by label"), this);
    sortLabelAction->setCheckable(true);
    connect(sortLabelAction, SIGNAL(triggered()), this, SLOT(sortThumbnails()));

    sortRatingAction = new QAction(tr("Sort by rating"), this);
    sortRatingAction->setCheckable(true);
    connect(sortRatingAction, SIGNAL(triggered()), this, SLOT(sortThumbnails()));

    sortMegaPixelsAction = new QAction(tr("Sort by megapixels"), this);
    sortMegaPixelsAction->setCheckable(true);
    connect(sortMegaPixelsAction, SIGNAL(triggered()), this, SLOT(sortThumbnails()));

    sortDimensionsAction = new QAction(tr("Sort by dimensions"), this);
    sortDimensionsAction->setCheckable(true);
    connect(sortDimensionsAction, SIGNAL(triggered()), this, SLOT(sortThumbnails()));

    sortApertureAction = new QAction(tr("Sort by aperture"), this);
//    sortApertureAction->setObjectName("SortAperture");
    sortApertureAction->setCheckable(true);
    connect(sortApertureAction, SIGNAL(triggered()), this, SLOT(sortThumbnails()));

    sortShutterSpeedAction = new QAction(tr("Sort by shutter speed"), this);
    sortShutterSpeedAction->setCheckable(true);
    connect(sortShutterSpeedAction, SIGNAL(triggered()), this, SLOT(sortThumbnails()));

    sortISOAction = new QAction(tr("Sort by ISO"), this);
    sortISOAction->setCheckable(true);
    connect(sortISOAction, SIGNAL(triggered()), this, SLOT(sortThumbnails()));

    sortModelAction = new QAction(tr("Sort by camera model"), this);
    sortModelAction->setCheckable(true);
    connect(sortModelAction, SIGNAL(triggered()), this, SLOT(sortThumbnails()));

    sortFocalLengthAction = new QAction(tr("Sort by focal length"), this);
    sortFocalLengthAction->setCheckable(true);
    connect(sortFocalLengthAction, SIGNAL(triggered()), this, SLOT(sortThumbnails()));

    sortTitleAction = new QAction(tr("Sort by title"), this);
    sortTitleAction->setCheckable(true);
    connect(sortTitleAction, SIGNAL(triggered()), this, SLOT(sortThumbnails()));

    sortGroupAction = new QActionGroup(this);
    sortGroupAction->setExclusive(true);

    sortGroupAction->addAction(sortFileNameAction);
    sortGroupAction->addAction(sortFileTypeAction);
    sortGroupAction->addAction(sortFileSizeAction);
    sortGroupAction->addAction(sortCreateAction);
    sortGroupAction->addAction(sortModifyAction);
    sortGroupAction->addAction(sortPickAction);
    sortGroupAction->addAction(sortLabelAction);
    sortGroupAction->addAction(sortRatingAction);
    sortGroupAction->addAction(sortMegaPixelsAction);
    sortGroupAction->addAction(sortDimensionsAction);
    sortGroupAction->addAction(sortApertureAction);
    sortGroupAction->addAction(sortShutterSpeedAction);
    sortGroupAction->addAction(sortISOAction);
    sortGroupAction->addAction(sortModelAction);
    sortGroupAction->addAction(sortFocalLengthAction);
    sortGroupAction->addAction(sortTitleAction);

    sortFileNameAction->setChecked(true);

    sortReverseAction = new QAction(tr("Reverse sort order"), this);
    sortReverseAction->setObjectName("reverse");
    sortReverseAction->setCheckable(true);
//    sortReverseAction->setChecked(false);
    connect(sortReverseAction, SIGNAL(triggered()), this, SLOT(sortThumbnails()));

    // View menu
    slideShowAction = new QAction(tr("Slide Show"), this);
    slideShowAction->setObjectName("slideShow");
    connect(slideShowAction, SIGNAL(triggered()), this, SLOT(slideShow()));

    fullScreenAction = new QAction(tr("Full Screen"), this);
    fullScreenAction->setObjectName("fullScreenAct");
    fullScreenAction->setCheckable(true);
//    fullScreenAction->setChecked(setting->value("isFullScreen").toBool());
    connect(fullScreenAction, SIGNAL(triggered()), this, SLOT(toggleFullScreen()));

    escapeFullScreenAction = new QAction(tr("Escape Full Screen"), this);
    escapeFullScreenAction->setObjectName("escapeFullScreenAct");
    connect(escapeFullScreenAction, SIGNAL(triggered()), this, SLOT(escapeFullScreen()));

    infoVisibleAction = new QAction(tr("Shooting Info"), this);
    infoVisibleAction->setObjectName("toggleInfo");
    infoVisibleAction->setCheckable(true);
//    infoVisibleAction->setChecked(setting->value("isImageInfoVisible").toBool());  // from QSettings
    connect(infoVisibleAction, SIGNAL(triggered()), this, SLOT(setShootingInfo()));


    asLoupeAction = new QAction(tr("Loupe"), this);
    asLoupeAction->setCheckable(true);
//    asLoupeAction->setChecked(setting->value("isLoupeDisplay").toBool() ||
//                              setting->value("isCompareDisplay").toBool());
    // add secondary shortcut (primary defined in loadShortcuts)
    QShortcut *enter = new QShortcut(QKeySequence("Return"), this);
    connect(enter, SIGNAL(activated()), asLoupeAction, SLOT(trigger()));
    connect(asLoupeAction, SIGNAL(triggered()), this, SLOT(loupeDisplay()));

    asGridAction = new QAction(tr("Grid"), this);
    asGridAction->setCheckable(true);
//    asGridAction->setChecked(setting->value("isGridDisplay").toBool());
    connect(asGridAction, SIGNAL(triggered()), this, SLOT(gridDisplay()));

    asTableAction = new QAction(tr("Table"), this);
    asTableAction->setCheckable(true);
    connect(asTableAction, SIGNAL(triggered()), this, SLOT(tableDisplay()));

    asCompareAction = new QAction(tr("Compare"), this);
    asCompareAction->setCheckable(true);
//    asCompareAction->setChecked(false); // never start with compare set true
    connect(asCompareAction, SIGNAL(triggered()), this, SLOT(compareDisplay()));

    centralGroupAction = new QActionGroup(this);
    centralGroupAction->setExclusive(true);
    centralGroupAction->addAction(asLoupeAction);
    centralGroupAction->addAction(asGridAction);
    centralGroupAction->addAction(asTableAction);
    centralGroupAction->addAction(asCompareAction);

    zoomOutAction = new QAction(tr("Zoom Out"), this);
    zoomOutAction->setObjectName("zoomOut");
    connect(zoomOutAction, SIGNAL(triggered()), this, SLOT(zoomOut()));

    zoomInAction = new QAction(tr("Zoom In"), this);
    zoomInAction->setObjectName("zoomIn");
    connect(zoomInAction, SIGNAL(triggered()), this, SLOT(zoomIn()));

    zoomToggleAction = new QAction(tr("Zoom fit <-> 100%"), this);
    zoomToggleAction->setObjectName("resetZoom");
    connect(zoomToggleAction, SIGNAL(triggered()), this, SLOT(zoomToggle()));

    zoom50PctAction = new QAction(tr("Click zoom 50%"), this);
    zoom50PctAction->setObjectName("50PctZoom");
    zoom50PctAction->setCheckable(true);
    connect(zoom50PctAction, SIGNAL(triggered()), this, SLOT(zoom50()));

    zoom100PctAction = new QAction(tr("Click zoom 100%"), this);
    zoom100PctAction->setObjectName("100PctZoom");
    zoom100PctAction->setCheckable(true);
    zoom100PctAction->setChecked(true);     // temp until add to QSettings
    connect(zoom100PctAction, SIGNAL(triggered()), this, SLOT(zoom100()));

    zoom200PctAction = new QAction(tr("Click zoom 200%"), this);
    zoom200PctAction->setObjectName("200PctZoom");
    zoom200PctAction->setCheckable(true);
    connect(zoom200PctAction, SIGNAL(triggered()), this, SLOT(zoom200()));

    zoomGroupAction = new QActionGroup(this);
    zoomGroupAction->setExclusive(true);
    zoomGroupAction->addAction(zoom50PctAction);
    zoomGroupAction->addAction(zoom100PctAction);
    zoomGroupAction->addAction(zoom200PctAction);

    thumbsEnlargeAction = new QAction(tr("Enlarge thumbs"), this);
    thumbsEnlargeAction->setObjectName("enlargeThumbs");
    connect(thumbsEnlargeAction, SIGNAL(triggered()), thumbView, SLOT(thumbsEnlarge()));
//    if (thumbView->thumbSize == THUMB_SIZE_MAX)
//        thumbsEnlargeAction->setEnabled(false);
//    addAction(thumbsEnlargeAction);

    thumbsShrinkAction = new QAction(tr("Shrink thumbs"), this);
    thumbsShrinkAction->setObjectName("shrinkThumbs");
    connect(thumbsShrinkAction, SIGNAL(triggered()), thumbView, SLOT(thumbsShrink()));
//    if (thumbView->thumbSize == THUMB_SIZE_MIN)
//            thumbsShrinkAction->setEnabled(false);
//    addAction(thumbsShrinkAction);

    // is this used - not in menu
    thumbsFitAction = new QAction(tr("Fit thumbs"), this);
    thumbsFitAction->setObjectName("thumbsZoomOut");
    connect(thumbsFitAction, SIGNAL(triggered()), this, SLOT(setDockFitThumbs()));
//    if (thumbView->thumbSize == THUMB_SIZE_MIN)
//            thumbsFitAction->setEnabled(false);

    showThumbLabelsAction = new QAction(tr("Thumb labels"), this);
    showThumbLabelsAction->setObjectName("showLabels");
    showThumbLabelsAction->setCheckable(true);
//    showThumbLabelsAction->setChecked(thumbView->showThumbLabels);
    connect(showThumbLabelsAction, SIGNAL(triggered()), this, SLOT(setThumbLabels()));
    showThumbLabelsAction->setEnabled(true);

    // Window menu

    windowTitleBarVisibleAction = new QAction(tr("Window Titlebar"), this);
    windowTitleBarVisibleAction->setObjectName("toggleWindowsTitleBar");
    windowTitleBarVisibleAction->setCheckable(true);
    connect(windowTitleBarVisibleAction, SIGNAL(triggered()), this, SLOT(setWindowsTitleBarVisibility()));

    menuBarVisibleAction = new QAction(tr("Menubar"), this);
    menuBarVisibleAction->setObjectName("toggleMenuBar");
    menuBarVisibleAction->setCheckable(true);
    connect(menuBarVisibleAction, SIGNAL(triggered()), this, SLOT(setMenuBarVisibility()));

    statusBarVisibleAction = new QAction(tr("Statusbar"), this);
    statusBarVisibleAction->setObjectName("toggleStatusBar");
    statusBarVisibleAction->setCheckable(true);
    connect(statusBarVisibleAction, SIGNAL(triggered()), this, SLOT(setStatusBarVisibility()));

    folderDockVisibleAction = new QAction(tr("Folder"), this);
    folderDockVisibleAction->setObjectName("toggleFiless");
    folderDockVisibleAction->setCheckable(true);
    connect(folderDockVisibleAction, SIGNAL(triggered()), this, SLOT(setFolderDockVisibility()));

    favDockVisibleAction = new QAction(tr("Favourites"), this);
    favDockVisibleAction->setObjectName("toggleFavs");
    favDockVisibleAction->setCheckable(true);
    connect(favDockVisibleAction, SIGNAL(triggered()), this, SLOT(setFavDockVisibility()));

    filterDockVisibleAction = new QAction(tr("Filters"), this);
    filterDockVisibleAction->setObjectName("toggleFilters");
    filterDockVisibleAction->setCheckable(true);
    connect(filterDockVisibleAction, SIGNAL(triggered()), this, SLOT(setFilterDockVisibility()));

    metadataDockVisibleAction = new QAction(tr("Metadata"), this);
    metadataDockVisibleAction->setObjectName("toggleMetadata");
    metadataDockVisibleAction->setCheckable(true);
    connect(metadataDockVisibleAction, SIGNAL(triggered()), this, SLOT(setMetadataDockVisibility()));

    thumbDockVisibleAction = new QAction(tr("Thumbnails"), this);
    thumbDockVisibleAction->setObjectName("toggleThumbs");
    thumbDockVisibleAction->setCheckable(true);
    connect(thumbDockVisibleAction, SIGNAL(triggered()), this, SLOT(setThumbDockVisibity()));

    folderDockLockAction = new QAction(tr("Hide Files Titlebar"), this);
    folderDockLockAction->setObjectName("lockDockFiles");
    folderDockLockAction->setCheckable(true);
    connect(folderDockLockAction, SIGNAL(triggered()), this, SLOT(setFolderDockLockMode()));

    favDockLockAction = new QAction(tr("Hide Favourite titlebars"), this);
    favDockLockAction->setObjectName("lockDockFavs");
    favDockLockAction->setCheckable(true);
    connect(favDockLockAction, SIGNAL(triggered()), this, SLOT(setFavDockLockMode()));

    filterDockLockAction = new QAction(tr("Hide Filter titlebars"), this);
    filterDockLockAction->setObjectName("lockDockFilters");
    filterDockLockAction->setCheckable(true);
    connect(filterDockLockAction, SIGNAL(triggered()), this, SLOT(setFilterDockLockMode()));

    metadataDockLockAction = new QAction(tr("Hide Metadata Titlebar"), this);
    metadataDockLockAction->setObjectName("lockDockMetadata");
    metadataDockLockAction->setCheckable(true);
    connect(metadataDockLockAction, SIGNAL(triggered()), this, SLOT(setMetadataDockLockMode()));

    thumbDockLockAction = new QAction(tr("Hide Thumbs Titlebar"), this);
    thumbDockLockAction->setObjectName("lockDockThumbs");
    thumbDockLockAction->setCheckable(true);
    connect(thumbDockLockAction, SIGNAL(triggered()), this, SLOT(setThumbDockLockMode()));

    allDocksLockAction = new QAction(tr("Hide All Titlebars"), this);
    allDocksLockAction->setObjectName("lockDocks");
    allDocksLockAction->setCheckable(true);
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
    n = workspaces->count();
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
    connect(helpAction, SIGNAL(triggered()), this, SLOT(help()));

    // Possibly needed actions

//    enterAction = new QAction(tr("Enter"), this);
//    enterAction->setObjectName("enterAction");
//    enterAction->setShortcut(QKeySequence("X"));
//    connect(enterAction, SIGNAL(triggered()), this, SLOT(enter()));

    // used in fsTree and bookmarks
    pasteAction = new QAction(tr("Paste files"), this);
    pasteAction->setObjectName("paste");
//        connect(pasteAction, SIGNAL(triggered()), this, SLOT(about()));
}

void MW::createMenus()
{
    {
    #ifdef ISDEBUG
    qDebug() << "MW::createMenus";
    #endif
    }
    // Main Menu
    fileMenu = menuBar()->addMenu(tr("&File"));
    fileMenu->addAction(openAction);
    openWithMenu = fileMenu->addMenu(tr("Open with..."));
    openWithMenu->addAction(chooseAppAction);
    openWithMenu->addAction(newAppAction);
    openWithMenu->addAction(deleteAppAction);
    fileMenu->addSeparator();
    fileMenu->addAction(revealFileAction);
    fileMenu->addAction(subFoldersAction);
    fileMenu->addAction(addBookmarkAction);
    recentFoldersMenu = fileMenu->addMenu(tr("Recent folders"));
    // add 10 dummy menu items for custom workspaces
    for (int i = 0; i < maxRecentFolders; i++) {
        recentFoldersMenu->addAction(recentFolderActions.at(i));
    }
    connect(recentFoldersMenu, SIGNAL(triggered(QAction*)),
            SLOT(invokeRecentFolder(QAction*)));

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
    editMenu->addAction(rate0Action);
    editMenu->addAction(rate1Action);
    editMenu->addAction(rate2Action);
    editMenu->addAction(rate3Action);
    editMenu->addAction(rate4Action);
    editMenu->addAction(rate5Action);
    editMenu->addSeparator();
    editMenu->addAction(label0Action);
    editMenu->addAction(label1Action);
    editMenu->addAction(label2Action);
    editMenu->addAction(label3Action);
    editMenu->addAction(label4Action);
    editMenu->addAction(label5Action);
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

    sortMenu = menuBar()->addMenu(tr("Sort"));
    sortMenu->addActions(sortGroupAction->actions());
    sortMenu->addSeparator();
    sortMenu->addAction(sortReverseAction);

    viewMenu = menuBar()->addMenu(tr("&View"));
    viewMenu->addAction(slideShowAction);
    viewMenu->addAction(fullScreenAction);
    viewMenu->addAction(escapeFullScreenAction);
    viewMenu->addSeparator();
    viewMenu->addAction(infoVisibleAction);
    viewMenu->addSeparator();
    viewMenu->addActions(centralGroupAction->actions());
    viewMenu->addSeparator();
    viewMenu->addAction(zoomInAction);
    viewMenu->addAction(zoomOutAction);
    viewMenu->addAction(zoomToggleAction);
    viewMenu->addSeparator();
    viewMenu->addAction(zoom50PctAction);
    viewMenu->addAction(zoom100PctAction);
    viewMenu->addAction(zoom200PctAction);
    viewMenu->addSeparator();
    viewMenu->addAction(thumbsEnlargeAction);
    viewMenu->addAction(thumbsShrinkAction);
    viewMenu->addAction(thumbsFitAction);
    viewMenu->addAction(showThumbLabelsAction);
//    viewMenu->addAction(sortReverseAction);
    viewMenu->addSeparator();

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
            SLOT(invokeWorkspaceFromAction(QAction*)));
    windowMenu->addAction(folderDockVisibleAction);
    windowMenu->addAction(favDockVisibleAction);
    windowMenu->addAction(filterDockVisibleAction);
    windowMenu->addAction(metadataDockVisibleAction);
    windowMenu->addAction(thumbDockVisibleAction);
    windowMenu->addSeparator();
    windowMenu->addAction(windowTitleBarVisibleAction);
    windowMenu->addAction(menuBarVisibleAction);
    windowMenu->addAction(statusBarVisibleAction);
    windowMenu->addSeparator();
    windowMenu->addAction(folderDockLockAction);
    windowMenu->addAction(favDockLockAction);
    windowMenu->addAction(filterDockLockAction);
    windowMenu->addAction(metadataDockLockAction);
    windowMenu->addAction(thumbDockLockAction);
    windowMenu->addSeparator();
    windowMenu->addAction(allDocksLockAction);

    helpMenu = menuBar()->addMenu(tr("&Help"));
    helpMenu->addAction(aboutAction);
    helpMenu->addAction(helpAction);

    menuBar()->setVisible(true);

    // FSTree context menu
    fsTree->addAction(openAction);
    addMenuSeparator(fsTree);
    fsTree->addAction(pasteAction);
    addMenuSeparator(fsTree);
    fsTree->addAction(openWithMenuAction);
    fsTree->addAction(addBookmarkAction);
    fsTree->setContextMenuPolicy(Qt::ActionsContextMenu);

    // filters context menu
    filters->addAction(uncheckAllAction);
    addMenuSeparator(filters);
    filters->addAction(expandAllAction);
    filters->addAction(collapseAllAction);
    filters->setContextMenuPolicy(Qt::ActionsContextMenu);

    // bookmarks context menu
    bookmarks->addAction(pasteAction);
    bookmarks->addAction(removeBookmarkAction);
    bookmarks->setContextMenuPolicy(Qt::ActionsContextMenu);

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
    zoomSubMenu->addAction(zoomToggleAction);

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
    sortMenu->addActions(sortGroupAction->actions());
    sortMenu->addSeparator();
    sortMenu->addAction(sortReverseAction);

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
//    connect(workspaceSubMenu, SIGNAL(triggered(QAction*)),
//            SLOT(invokeWorkspaceFromAction(QAction*)));

    QMenu *windowSubMenu = new QMenu(imageView);
    QAction *windowGroupAct = new QAction("Window", this);
    imageView->addAction(windowGroupAct);
    windowGroupAct->setMenu(windowSubMenu);
    windowSubMenu->addAction(folderDockVisibleAction);
    windowSubMenu->addAction(favDockVisibleAction);
    windowSubMenu->addAction(filterDockVisibleAction);
    windowSubMenu->addAction(metadataDockVisibleAction);
    windowSubMenu->addAction(thumbDockVisibleAction);
    addMenuSeparator(windowSubMenu);
    windowSubMenu->addAction(windowTitleBarVisibleAction);
    windowSubMenu->addAction(menuBarVisibleAction);
    windowSubMenu->addAction(statusBarVisibleAction);
    addMenuSeparator(windowSubMenu);
    windowSubMenu->addAction(folderDockLockAction);
    windowSubMenu->addAction(favDockLockAction);
    windowSubMenu->addAction(filterDockLockAction);
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
//    viewSubMenu->addAction(asListAction);
//    viewSubMenu->addAction(asIconsAction);
//    viewSubMenu->addSeparator();
    viewSubMenu->addAction(sortReverseAction);

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

void MW::addMenuSeparator(QWidget *widget)
{
    QAction *separator = new QAction(this);
    separator->setSeparator(true);
    widget->addAction(separator);
}

void MW::createCentralWidget()
{
    {
    #ifdef ISDEBUG
    qDebug() << "MW::createCentralWidget";
    #endif
    }
    // centralWidget required by ImageView/CompareView constructors
    centralWidget = new QWidget(this);
    centralWidget->setObjectName("centralWidget");
    // stack layout for loupe, table, compare and grid displays
    centralLayout = new QStackedLayout;
    centralLayout->setContentsMargins(0, 0, 0, 0);
}

void MW::setupCentralWidget()
{
    {
    #ifdef ISDEBUG
    qDebug() << "MW::setupCentralWidget";
    #endif
    }
    centralLabel = new QLabel;
    centralLayout->addWidget(centralLabel);     // first time open program
    centralLayout->addWidget(imageView);
    centralLayout->addWidget(compareImages);
    centralLayout->addWidget(tableView);
    centralLayout->setCurrentIndex(0);
    centralWidget->setLayout(centralLayout);
    setCentralWidget(centralWidget);
}

void MW::createDataModel()
{
    {
    #ifdef ISDEBUG
    qDebug() << "MW::createDataModel";
    #endif
    }
    metadata = new Metadata;
    dm = new DataModel(this, metadata, filters);

    connect(dm->sf, SIGNAL(reloadImageCache()),
            this, SLOT(loadFilteredImageCache()));
}

void MW::createSelectionModel()
{
/*
The application only has one selection model which is shared by ThumbView and
TableView, insuring that each view is in sync.
*/
    {
    #ifdef ISDEBUG
    qDebug() << "MW::createSelectionModel";
    #endif
    }
    // set a common selection model for all views
    selectionModel = new QItemSelectionModel(dm->sf);
    thumbView->setSelectionModel(selectionModel);
    tableView->setSelectionModel(selectionModel);

    connect(selectionModel, SIGNAL(currentChanged(QModelIndex, QModelIndex)),
            this, SLOT(fileSelectionChange(QModelIndex, QModelIndex)));
}

void MW::createCaching()
{
    {
    #ifdef ISDEBUG
    qDebug() << "MW::createDataModel";
    #endif
    }
    metadataCacheThread = new MetadataCache(this, dm, metadata);
    thumbCacheThread = new ThumbCache(this, dm, metadata);
    imageCacheThread = new ImageCache(this, metadata);

    connect(metadataCacheThread, SIGNAL(loadThumbCache()),
            this, SLOT(loadThumbCache()));

    connect(metadataCacheThread, SIGNAL(loadImageCache()),
            this, SLOT(loadImageCache()));

    connect(metadataCacheThread, SIGNAL(updateIsRunning(bool)),
            this, SLOT(updateMetadataThreadRunStatus(bool)));

    connect(thumbCacheThread, SIGNAL(setIcon(QStandardItem*, QImage, QString)),
            thumbView, SLOT(setIcon(QStandardItem*, QImage, QString)));

    connect(thumbCacheThread, SIGNAL(updateStatus(bool, QString)),
            this, SLOT(updateStatus(bool, QString)));

    connect(thumbCacheThread, SIGNAL(refreshThumbs()),
            thumbView, SLOT(refreshThumbs()));

    connect(thumbCacheThread, SIGNAL(updateIsRunning(bool)),
            this, SLOT(updateThumbThreadRunStatus(bool)));

    connect(imageCacheThread, SIGNAL(updateIsRunning(bool)),
            this, SLOT(updateImageThreadRunStatus(bool)));

    connect(imageCacheThread, SIGNAL(showCacheStatus(const QImage)),
            this, SLOT(showCacheStatus(const QImage)));
}

void MW::createThumbView()
{
    {
    #ifdef ISDEBUG
    qDebug() << "MW::createThumbView";
    #endif
    }
    thumbView = new ThumbView(this, dm);
    thumbView->setObjectName("ThumbView");  //rgh need to fix??

    thumbView->thumbSpacing = setting->value("thumbSpacing").toInt();
    thumbView->thumbPadding = setting->value("thumbPadding").toInt();
    thumbView->thumbWidth = setting->value("thumbWidth").toInt();
    thumbView->thumbHeight = setting->value("thumbHeight").toInt();
    thumbView->labelFontSize = setting->value("labelFontSize").toInt();
    thumbView->showThumbLabels = setting->value("showThumbLabels").toBool();

    thumbView->thumbSpacingGrid = setting->value("thumbSpacingGrid").toInt();
    thumbView->thumbPaddingGrid = setting->value("thumbPaddingGrid").toInt();
    thumbView->thumbWidthGrid = setting->value("thumbWidthGrid").toInt();
    thumbView->thumbHeightGrid = setting->value("thumbHeightGrid").toInt();
    thumbView->labelFontSizeGrid = setting->value("labelFontSizeGrid").toInt();
    thumbView->showThumbLabelsGrid = setting->value("showThumbLabelsGrid").toBool();

    thumbView->isThumbWrapWhenTopOrBottomDock = setting->value("isThumbWrapWhenTopOrBottomDock").toBool();
    thumbView->isAutoFit = setting->value("isAutoFit").toBool();

    connect(thumbView, SIGNAL(displayLoupe()), this, SLOT(loupeDisplay()));

    connect(thumbView, SIGNAL(updateThumbDock()),
            this, SLOT(setThumbDockHeight()));

    connect(thumbView, SIGNAL(updateStatus(bool, QString)),
            this, SLOT(updateStatus(bool, QString)));
}

void MW::createTableView()
{
/*
TableView includes all the metadata used for each image. It is useful for
sorting on any column and to check for information to filter.  Creation is
dependent on datamodel and thumbView.
*/
    {
    #ifdef ISDEBUG
    qDebug() << "MW::createTableView";
    #endif
    }
    tableView = new TableView(dm, thumbView);

    // update menu "sort by" to match tableView sort change
    connect(tableView->horizontalHeader(),
            SIGNAL(sortIndicatorChanged(int,Qt::SortOrder)),
            this, SLOT(sortIndicatorChanged(int,Qt::SortOrder)));

    connect(tableView, SIGNAL(displayLoupe()), this, SLOT(loupeDisplay()));
}

void MW::createImageView()
{
/*
ImageView displays the image in the central widget.  The image is either from
the image cache or read from the file if the cache is unavailable.  Creation is
dependent on metadata, imageCacheThread, thumbView, datamodel and settings.
*/
    {
    #ifdef ISDEBUG
    qDebug() << "MW::createImageView";
    #endif
    }
    imageView = new ImageView(this, centralWidget, metadata, imageCacheThread, thumbView,
                              setting->value("isImageInfoVisible").toBool());

    connect(imageView, SIGNAL(togglePick()), this, SLOT(togglePick()));

    connect(imageView, SIGNAL(updateStatus(bool, QString)),
            this, SLOT(updateStatus(bool, QString)));

    connect(thumbView, SIGNAL(thumbClick(float,float)),
            imageView, SLOT(thumbClick(float,float)));
}

void MW::createCompareView()
{
    {
    #ifdef ISDEBUG
    qDebug() << "MW::createCompareView";
    #endif
    }
    compareImages = new CompareImages(this, centralWidget, metadata, thumbView, imageCacheThread);
}

void MW::createInfoView()
{
/*
InfoView shows basic metadata in a dock widget.
*/
    {
    #ifdef ISDEBUG
    qDebug() << "MW::createInfoView";
    #endif
    }
    infoView = new InfoView(this, metadata);
    infoView->setMaximumWidth(folderMaxWidth);
}

void MW::createDocks()
{
    {
    #ifdef ISDEBUG
        qDebug() << "MW::setupDocks";
    #endif
    }
    folderDock = new QDockWidget(tr("  Folders  "), this);
    folderDock->setObjectName("File System");
    folderDock->setWidget(fsTree);

    favDock = new QDockWidget(tr("  Fav  "), this);
    favDock->setObjectName("Bookmarks");
    favDock->setWidget(bookmarks);

    metadataDock = new QDockWidget(tr("  Metadata  "), this);
    metadataDock->setObjectName("Image Info");
    metadataDock->setWidget(infoView);

    filterDock = new QDockWidget(tr("  Filters  "), this);
    filterDock->setObjectName("Filters");
    filterDock->setWidget(filters);

    thumbDock = new QDockWidget(tr("Thumbnails"), this);
    thumbDock->setObjectName("thumbDock");
    thumbDock->setWidget(thumbView);
    thumbDock->setWindowTitle("Thumbs");
    thumbDock->installEventFilter(this);

    addDockWidget(Qt::LeftDockWidgetArea, folderDock);
    addDockWidget(Qt::LeftDockWidgetArea, favDock);
    addDockWidget(Qt::LeftDockWidgetArea, filterDock);
    addDockWidget(Qt::LeftDockWidgetArea, metadataDock);
    addDockWidget(Qt::LeftDockWidgetArea, thumbDock);

    connect(thumbDock, SIGNAL(dockLocationChanged(Qt::DockWidgetArea)),
            this, SLOT(setThumbDockFeatures(Qt::DockWidgetArea)));

    connect(thumbDock, SIGNAL(topLevelChanged(bool)),
            this, SLOT(setThumbDockFloatFeatures(bool)));

    folderDockOrigWidget = folderDock->titleBarWidget();
    favDockOrigWidget = favDock->titleBarWidget();
    filterDockOrigWidget = filterDock->titleBarWidget();
    metadataDockOrigWidget = metadataDock->titleBarWidget();
    thumbDockOrigWidget = thumbDock->titleBarWidget();
    folderDockEmptyWidget = new QWidget;
    favDockEmptyWidget = new QWidget;
    filterDockEmptyWidget = new QWidget;
    metadataDockEmptyWidget = new QWidget;
    thumbDockEmptyWidget = new QWidget;

    MW::setTabPosition(Qt::LeftDockWidgetArea, QTabWidget::North);
    MW::tabifyDockWidget(folderDock, favDock);
    MW::tabifyDockWidget(favDock, metadataDock);
    MW::tabifyDockWidget(metadataDock, filterDock);
}

void MW::createFSTree()
{
    {
    #ifdef ISDEBUG
    qDebug() << "MW::createFSTree";
    #endif
    }
    fsTree = new FSTree(folderDock);
    fsTree->setMaximumWidth(folderMaxWidth);

    connect(fsTree, SIGNAL(clicked(const QModelIndex&)), this, SLOT(folderSelectionChange()));

    connect(fsTree->fsModel, SIGNAL(rowsRemoved(const QModelIndex &, int, int)),
            this, SLOT(checkDirState(const QModelIndex &, int, int)));

    connect(fsTree, SIGNAL(dropOp(Qt::KeyboardModifiers, bool, QString)),
            this, SLOT(dropOp(Qt::KeyboardModifiers, bool, QString)));
}

void MW::createFilterView()
{
    {
    #ifdef ISDEBUG
    qDebug() << "MW::createFilterView";
    #endif
    }
    filters = new Filters(filterDock);
    filters->setMaximumWidth(folderMaxWidth);
}

void MW::createBookmarks()
{
    {
    #ifdef ISDEBUG
    qDebug() << "MW::createBookmarks";
    #endif
    }
    bookmarks = new BookMarks(favDock);

    bookmarks->setMaximumWidth(folderMaxWidth);

    connect(bookmarks, SIGNAL(itemClicked(QTreeWidgetItem *, int)),
            this, SLOT(bookmarkClicked(QTreeWidgetItem *, int)));

//    connect(removeBookmarkAction, SIGNAL(triggered()),
//            bookmarks, SLOT(removeBookmark()));

    connect(bookmarks, SIGNAL(dropOp(Qt::KeyboardModifiers, bool, QString)),
            this, SLOT(dropOp(Qt::KeyboardModifiers, bool, QString)));
}

void MW::createAppStyle()
{
    {
    #ifdef ISDEBUG
    qDebug() << "MW::createAppStyle";
    #endif
    }
    // add error trapping for file io  rgh todo
    QFile fStyle(":/qss/teststyle.css");
    fStyle.open(QIODevice::ReadOnly);
    this->setStyleSheet(fStyle.readAll());
}

void MW::createStatusBar()
{
    {
    #ifdef ISDEBUG
    qDebug() << "MW::createStatusBar";
    #endif
    }
    statusBar()->setStyleSheet("QStatusBar::item { border: none; };");

    cacheLabel = new QLabel();
    QString cacheStatus = "Image cache status for current folder:\n";
    cacheStatus += "  • LightGray:\tbackground for all images in folder\n";
    cacheStatus += "  • DarkGray: \tto be cached\n";
    cacheStatus += "  • Green:    \tcached\n";
    cacheStatus += "  • Red:      \tcurrent image";
    cacheLabel->setToolTip(cacheStatus);
    cacheLabel->setToolTipDuration(100000);
    statusBar()->addPermanentWidget(cacheLabel);

    QLabel *spacer = new QLabel;
    spacer->setText(" ");
    statusBar()->addPermanentWidget(spacer);

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

void MW::setThumbDockParameters(bool isThumbWrapWhenTopOrBottomDock,
                                bool isAutoFit, bool isVerticalTitle)
{
/*
Signal from prefDlg when thumbWrap or verticalTitle changed
*/
    {
    #ifdef ISDEBUG
    qDebug() << "MW::setThumbDockParameters";
    #endif
    }
    qDebug() << "MW::setThumbDockParameters";
    thumbView->isThumbWrapWhenTopOrBottomDock = isThumbWrapWhenTopOrBottomDock;       // is this needed?
    thumbView->isAutoFit = isAutoFit;
    isThumbDockVerticalTitle = isVerticalTitle;
    setThumbDockFeatures(dockWidgetArea(thumbDock));
}

void MW::setCacheParameters(int size, bool show, int width, int wtAhead,
                            bool usePreview, int previewWidth, int previewHeight)
{
    {
    #ifdef ISDEBUG
    qDebug() << "MW::setCacheParameters";
    #endif
    }
    cacheSizeMB = size;
    isShowCacheStatus = show;
    cacheStatusWidth = width;
    cacheWtAhead = wtAhead;
    isCachePreview = usePreview;
    cachePreviewWidth = previewWidth;
    cachePreviewHeight = previewHeight;
    imageCacheThread->updateImageCacheParam(size, show, width, wtAhead,
             usePreview, previewWidth, previewHeight);
    QString fPath = thumbView->currentIndex().data(G::FileNameRole).toString();
    imageCacheThread->updateImageCache(fPath);
}

void MW::updateStatus(bool keepBase, QString s)
{
    {
    #ifdef ISDEBUG
    qDebug() << "MW::updateStatus";
    #endif
    }
    QString status;
    QString fileCount = "";
    QString zoomPct = "";
    QString base = "";
    QString spacer = "   ";

/* Possible status info

QString fName = idx.data(Qt::EditRole).toString();
QString fPath = idx.data(G::FileNameRole).toString();
QString shootingInfo = metadata->getShootingInfo(fPath);
QString err = metadata->getErr(fPath);
QString magnify = "🔎";
QString fileSym = "📁";
QString fileSym = "📷";
*/

    // image of total: fileCount
    if (keepBase) {
        QModelIndex idx = thumbView->currentIndex();
        long rowCount = dm->sf->rowCount();
        if (rowCount > 0) {
            int row = idx.row() + 1;
            fileCount = QString::number(row) + " of "
                + QString::number(rowCount);
        }
        if (subFoldersAction->isChecked()) fileCount += " including subfolders";
        zoomPct = QString::number(imageView->zoom*100, 'f', 0) + "% zoom";
        base = fileCount + spacer + zoomPct + spacer;
    }

    status = " " + base + s;
    stateLabel->setText(status);
}

void MW::updateMetadataThreadRunStatus(bool isRunning)
{
    {
    #ifdef ISDEBUG
    qDebug() << "MW::updataMetadataThreadRunStatus";
    #endif
    }
    if (isRunning)
        metadataThreadRunningLabel->setStyleSheet("QLabel {color:Green;}");
    else
        metadataThreadRunningLabel->setStyleSheet("QLabel {color:Gray;}");

    metadataThreadRunningLabel->setText("◉");
}

void MW::updateThumbThreadRunStatus(bool isRunning)
{
    {
    #ifdef ISDEBUG
    qDebug() << "MW::updateThumbThreadRunningStatus";
    #endif
    }
    if (isRunning)
        thumbThreadRunningLabel->setStyleSheet("QLabel {color:Green;}");
    else
        thumbThreadRunningLabel->setStyleSheet("QLabel {color:Gray;}");
    thumbThreadRunningLabel->setText("◉");
}

void MW::updateImageThreadRunStatus(bool isRunning)
{
    {
    #ifdef ISDEBUG
    qDebug() << "MW::updateImageThreadRunStatus";
    #endif
    }
    if (isRunning)
        imageThreadRunningLabel->setStyleSheet("QLabel {color:Green;}");
    else
        imageThreadRunningLabel->setStyleSheet("QLabel {color:Gray;}");
    imageThreadRunningLabel->setText("◉");
}

// used by ImageCache thread to show image cache building progress
void MW::showCacheStatus(const QImage &imCacheStatus)
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

void MW::reindexImageCache()
{
    {
    #ifdef ISDEBUG
    qDebug() << "ThumbView::reindexImageCache";
    #endif
    }
    qDebug() << "ThumbView::reindexImageCache";
    int sfRowCount = dm->sf->rowCount();
    if (!sfRowCount) return;
    dm->updateImageList();
    QModelIndex idx = thumbView->currentIndex();
    QString currentFilePath = idx.data(G::FileNameRole).toString();
    qDebug() << "reindexImageCache current" << currentFilePath;
    if(!dm->imageFilePathList.contains(currentFilePath)) {
        idx = dm->sf->index(0, 0);
        currentFilePath = idx.data(G::FileNameRole).toString();
        qDebug() << "reindexImageCache updated" << currentFilePath;
    }
//    QString currentFileName = dm->sf->index(0, 1).data(Qt::EditRole).toString();
    thumbView->selectThumb(idx);
    imageCacheThread->reindexImageCache(dm->imageFilePathList, currentFilePath);
}

void MW::sortIndicatorChanged(int column, Qt::SortOrder sortOrder)
{
/*
This slot function is triggered by the tableView->horizontalHeader
sortIndicatorChanged signal being emitted, which tells us that the data model
sort/filter has been re-sorted. As a consequence we need to update the menu
checked status for the correct column and also resync the image cache. However,
changing the menu checked state for any of the menu sort actions triggers a
sort, which needs to be suppressed while syncing the menu actions with
tableView.
*/
    {
    #ifdef ISDEBUG
    qDebug() << "ThumbView::sortIndicatorChanged" << column << sortOrder;
    #endif
    }
    sortMenuUpdateToMatchTable = true; // suppress sorting to update menu
    switch (column) {
    case 1: sortFileNameAction->setChecked(true); break;
    case 2: sortFileTypeAction->setChecked(true); break;
    case 3: sortFileSizeAction->setChecked(true); break;
    case 4: sortCreateAction->setChecked(true); break;
    case 5: sortModifyAction->setChecked(true); break;
    case 6: sortPickAction->setChecked(true); break;
    case 7: sortLabelAction->setChecked(true); break;
    case 8: sortRatingAction->setChecked(true); break;
    case 9: sortMegaPixelsAction->setChecked(true); break;
    case 10: sortDimensionsAction->setChecked(true); break;
    case 11: sortApertureAction->setChecked(true); break;
    case 12: sortShutterSpeedAction->setChecked(true); break;
    case 13: sortISOAction->setChecked(true); break;
    case 14: sortModelAction->setChecked(true); break;
    case 15: sortFocalLengthAction->setChecked(true); break;
    case 16: sortTitleAction->setChecked(true); break;
    }
    if(sortOrder == Qt::DescendingOrder) sortReverseAction->setChecked(true);
    else sortReverseAction->setChecked(false);
    sortMenuUpdateToMatchTable = false;
    reindexImageCache();
}

void MW::sortThumbnails()
{
    {
    #ifdef ISDEBUG
    qDebug() << "MW::sortThumbnails";
    #endif
    }
    if(sortMenuUpdateToMatchTable) return;

    int sortColumn;

    if (sortFileNameAction->isChecked()) sortColumn = G::NameColumn;
    if (sortFileTypeAction->isChecked()) sortColumn = G::TypeColumn;
    if (sortFileSizeAction->isChecked()) sortColumn = G::SizeColumn;
    if (sortCreateAction->isChecked()) sortColumn = G::CreatedColumn;
    if (sortModifyAction->isChecked()) sortColumn = G::ModifiedColumn;
    if (sortPickAction->isChecked()) sortColumn = G::PickedColumn;
    if (sortLabelAction->isChecked()) sortColumn = G::LabelColumn;
    if (sortRatingAction->isChecked()) sortColumn = G::RatingColumn;
    if (sortMegaPixelsAction->isChecked()) sortColumn = G::MegaPixelsColumn;
    if (sortDimensionsAction->isChecked()) sortColumn = G::DimensionsColumn;
    if (sortApertureAction->isChecked()) sortColumn = G::ApertureColumn;
    if (sortShutterSpeedAction->isChecked()) sortColumn = G::ShutterspeedColumn;
    if (sortISOAction->isChecked()) sortColumn = G::ISOColumn;
    if (sortModelAction->isChecked()) sortColumn = G::CameraModelColumn;
    if (sortFocalLengthAction->isChecked()) sortColumn = G::FocalLengthColumn;
    if (sortTitleAction->isChecked()) sortColumn = G::TitleColumn;

    thumbView->sortThumbs(sortColumn, sortReverseAction->isChecked());
    reindexImageCache();
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

void MW::setDockFitThumbs()
{
    {
    #ifdef ISDEBUG
    qDebug() << "MW::setThuumbsFit";
    #endif
    }
    setThumbDockFeatures(dockWidgetArea(thumbDock));
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

/* RECENT MENU
 *
 */

void MW::addRecentFolder(QString fPath)
{
    {
    #ifdef ISDEBUG
    qDebug() << "MW::addRecentFolder";
    #endif
    }
    if (!recentFolders->contains(fPath))
        recentFolders->prepend(fPath);
        // sync menu items
        syncRecentFoldersMenu();
}

void MW::invokeRecentFolder(QAction *recentFolderActions)
{
    {
    #ifdef ISDEBUG
    qDebug() << "MW::invokeRecentFolder";
    #endif
    }
    qDebug() << recentFolderActions->text();
    QString dir = recentFolderActions->text();
    fsTree->setCurrentIndex(fsTree->fsModel->index(dir));
    folderSelectionChange();
}

void MW::syncRecentFoldersMenu()
{
    {
    #ifdef ISDEBUG
    qDebug() << "MW::syncRecentFoldersMenu";
    #endif
    }
    // trim excess items
    while (recentFolders->count() > maxRecentFolders) {
        recentFolders->removeAt(recentFolders->count() - 1);
    }
    int count = recentFolders->count();
    for (int i = 0; i < maxRecentFolders; i++) {
        if (i < count) {
            recentFolderActions.at(i)->setText(recentFolders->at(i));
            recentFolderActions.at(i)->setVisible(true);
        }
        else {
            recentFolderActions.at(i)->setText("Future recent folder" + QString::number(i));
            recentFolderActions.at(i)->setVisible(false);
        }
//        qDebug() << "sync menu" << i << recentFolderActions.at(i)->text();
    }
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

void MW::invokeWorkspaceFromAction(QAction *workAction)
{
    {
    #ifdef ISDEBUG
    qDebug() << "MW::invokeWorkspaceFromAction";
    #endif
    }
    for (int i=0; i<workspaces->count(); i++) {
        if (workspaces->at(i).name == workAction->text()) {
            invokeWorkspace(workspaces->at(i));
//            reportWorkspace(i);         // rgh remove after debugging
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
    {
    #ifdef ISDEBUG
    qDebug() << "MW::invokeWorkspace";
    #endif
    }
    if (fullScreenAction->isChecked() != w.isFullScreen)
        fullScreenAction->setChecked(w.isFullScreen);
    setFullNormal();
    restoreGeometry(w.geometry);
    restoreState(w.state);
    windowTitleBarVisibleAction->setChecked(w.isWindowTitleBarVisible);
    menuBarVisibleAction->setChecked(w.isMenuBarVisible);
    statusBarVisibleAction->setChecked(w.isStatusBarVisible);
    folderDockVisibleAction->setChecked(w.isFolderDockVisible);
    favDockVisibleAction->setChecked(w.isFavDockVisible);
    filterDockVisibleAction->setChecked(w.isFilterDockVisible);
    metadataDockVisibleAction->setChecked(w.isMetadataDockVisible);
    thumbDockVisibleAction->setChecked(w.isThumbDockVisible);
    folderDockLockAction->setChecked(w.isFolderDockLocked);
    favDockLockAction->setChecked(w.isFavDockLocked);
    filterDockLockAction->setChecked(w.isFilterDockLocked);
    metadataDockLockAction->setChecked(w.isMetadataDockLocked);
    thumbDockLockAction->setChecked( w.isThumbDockLocked);
    infoVisibleAction->setChecked(w.isImageInfoVisible);
    asLoupeAction->setChecked(w.isLoupeDisplay);
    asGridAction->setChecked(w.isGridDisplay);
    asTableAction->setChecked(w.isTableDisplay);
    asCompareAction->setChecked(w.isCompareDisplay);
    thumbView->thumbWidth = w.thumbWidth,
    thumbView->thumbHeight = w.thumbHeight,
    thumbView->thumbSpacing = w.thumbSpacing,
    thumbView->thumbPadding = w.thumbPadding,
    thumbView->labelFontSize = w.labelFontSize,
    thumbView->showThumbLabels = w.showThumbLabels;
    thumbView->thumbWidthGrid = w.thumbWidthGrid,
    thumbView->thumbHeightGrid = w.thumbHeightGrid,
    thumbView->thumbSpacingGrid = w.thumbSpacingGrid,
    thumbView->thumbPaddingGrid = w.thumbPaddingGrid,
    thumbView->labelFontSizeGrid = w.labelFontSizeGrid,
    thumbView->showThumbLabelsGrid = w.showThumbLabelsGrid;
    thumbView->isThumbWrapWhenTopOrBottomDock = w.isThumbWrapWhenTopOrBottomDock;
    thumbView->isAutoFit = w.isAutoFit;
//    qDebug() << "\nMW::invokeWorkspace before calling setThumbParameters" << "\n"
//             << "***  thumbView Ht =" << thumbView->height()
//             << "thumbSpace Ht =" << thumbView->getThumbCellSize().height()
//             << "thumbHeight =" << thumbView->thumbHeight << "\n";
    thumbView->setThumbParameters();
    // if in grid view override normal behavior if workspace invoked
    isThumbDockVisibleBeforeGridViewInvoked = w.isThumbDockVisible;
    updateState();
}

void MW::snapshotWorkspace(workspaceData &wsd)
{
    {
    #ifdef ISDEBUG
    qDebug() << "MW::snapshotWorkspace";
    #endif
    }
    wsd.geometry = saveGeometry();
    wsd.state = saveState();
    wsd.isFullScreen = isFullScreen();
//    wsd.isFullScreen = fullScreenAction->isChecked();
    wsd.isWindowTitleBarVisible = windowTitleBarVisibleAction->isChecked();
    wsd.isMenuBarVisible = menuBarVisibleAction->isChecked();
    wsd.isStatusBarVisible = statusBarVisibleAction->isChecked();
    wsd.isFolderDockVisible = folderDockVisibleAction->isChecked();
    wsd.isFavDockVisible = favDockVisibleAction->isChecked();
    wsd.isFilterDockVisible = filterDockVisibleAction->isChecked();
    wsd.isMetadataDockVisible = metadataDockVisibleAction->isChecked();
    wsd.isThumbDockVisible = thumbDockVisibleAction->isChecked();
    wsd.isFolderDockLocked = folderDockLockAction->isChecked();
    wsd.isFavDockLocked = favDockLockAction->isChecked();
    wsd.isFilterDockLocked = filterDockLockAction->isChecked();
    wsd.isMetadataDockLocked = metadataDockLockAction->isChecked();
    wsd.isThumbDockLocked = thumbDockLockAction->isChecked();
    wsd.isImageInfoVisible = infoVisibleAction->isChecked();
//    wsd.isIconDisplay = asIconsAction->isChecked();

    wsd.isLoupeDisplay = asLoupeAction->isChecked();
    wsd.isGridDisplay = asGridAction->isChecked();
    wsd.isTableDisplay = asTableAction->isChecked();
    wsd.isCompareDisplay = asCompareAction->isChecked();

    wsd.thumbSpacing = thumbView->thumbSpacing;
    wsd.thumbPadding = thumbView->thumbPadding;
    wsd.thumbWidth = thumbView->thumbWidth;
    wsd.thumbHeight = thumbView->thumbHeight;
    wsd.labelFontSize = thumbView->labelFontSize;
    wsd.showThumbLabels = thumbView->showThumbLabels;
    wsd.isThumbWrapWhenTopOrBottomDock = thumbView->isThumbWrapWhenTopOrBottomDock;
    wsd.isAutoFit = thumbView->isAutoFit;
    wsd.isVerticalTitle = isThumbDockVerticalTitle; // rgh thumbDock->titleBarWidget()->is;

    wsd.thumbSpacingGrid = thumbView->thumbSpacingGrid;
    wsd.thumbPaddingGrid = thumbView->thumbPaddingGrid;
    wsd.thumbWidthGrid = thumbView->thumbWidthGrid;
    wsd.thumbHeightGrid = thumbView->thumbHeightGrid;
    wsd.labelFontSizeGrid = thumbView->labelFontSizeGrid;
    wsd.showThumbLabelsGrid = thumbView->showThumbLabelsGrid;

    wsd.isImageInfoVisible = infoVisibleAction->isChecked();
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
    {
    #ifdef ISDEBUG
    qDebug() << "MW::manageWorkspace";
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
    connect(workspaceDlg, SIGNAL(reportWorkspace(int)),
            this, SLOT(reportWorkspace(int)));
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
    syncWorkspaceMenu();
}

void MW::syncWorkspaceMenu()
{
    {
    #ifdef ISDEBUG
    qDebug() << "MW::syncWorkspaceMenu";
    #endif
    }
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
        qDebug() << "sync menu" << i << workspaceActions.at(i)->text();
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
    reportWorkspace(n);
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
    windowTitleBarVisibleAction->setChecked(true);
    menuBarVisibleAction->setChecked(true);
    statusBarVisibleAction->setChecked(true);

    folderDockVisibleAction->setChecked(true);
    favDockVisibleAction->setChecked(true);
    filterDockVisibleAction->setChecked(true);
    metadataDockVisibleAction->setChecked(true);
    thumbDockVisibleAction->setChecked(true);

    folderDockLockAction->setChecked(false);
    favDockLockAction->setChecked(false);
    filterDockLockAction->setChecked(false);
    metadataDockLockAction->setChecked(false);
    thumbDockLockAction->setChecked(false);

    thumbView->thumbSpacing = 0;
    thumbView->thumbPadding = 0;
    thumbView->thumbWidth = 120;
    thumbView->thumbHeight = 120;
    thumbView->labelFontSize = 8;
    thumbView->showThumbLabels = true;

    thumbView->thumbSpacingGrid = 0;
    thumbView->thumbPaddingGrid = 0;
    thumbView->thumbWidthGrid = 160;
    thumbView->thumbHeightGrid = 160;
    thumbView->labelFontSizeGrid = 8;
    thumbView->showThumbLabelsGrid = true;

    thumbView->setWrapping(false);
    thumbView->isAutoFit = false;

//    qDebug() << "\nMW::defaultWorkspace before calling setThumbParameters" << "\n"
//             << "***  thumbView Ht =" << thumbView->height()
//             << "thumbSpace Ht =" << thumbView->getThumbCellSize().height()
//             << "thumbHeight =" << thumbView->thumbHeight << "\n";
    thumbView->setThumbParameters();

    folderDock->setFloating(false);
    favDock->setFloating(false);
    filterDock->setFloating(false);
    metadataDock->setFloating(false);
    thumbDock->setFloating(false);

    addDockWidget(Qt::LeftDockWidgetArea, folderDock);
    addDockWidget(Qt::LeftDockWidgetArea, favDock);
    addDockWidget(Qt::LeftDockWidgetArea, filterDock);
    addDockWidget(Qt::LeftDockWidgetArea, metadataDock);
    addDockWidget(Qt::BottomDockWidgetArea, thumbDock);

//    MW::setTabPosition(Qt::LeftDockWidgetArea, QTabWidget::North);
//    MW::tabifyDockWidget(folderDock, favDock);

//    // enable the folder dock (first one in tab)
//    QList<QTabBar *> tabList = findChildren<QTabBar *>();
//    QTabBar* widgetTabBar = tabList.at(0);
//    widgetTabBar->setCurrentIndex(0);

    resizeDocks({thumbDock}, {160}, Qt::Vertical);
    setDockFitThumbs();
    asLoupeAction->setChecked(true);
    updateState();
}

void MW::renameWorkspace(int n, QString name)
{
    {
    #ifdef ISDEBUG
    qDebug() << "MW::renameWorkspace";
    #endif
    }
    qDebug() << "MW::renameWorkspace" << n << name;
    // do not rename if duplicate
    if (workspaces->count() > 0) {
        for (int i=1; i<workspaces->count(); i++) {
            if (workspaces->at(i).name == name) return;
        }
        (*workspaces)[n].name = name;
        syncWorkspaceMenu();
    }
}

void MW::populateWorkspace(int n, QString name)
{
    {
    #ifdef ISDEBUG
    qDebug() << "MW::populateWorkspace";
    #endif
    }
    snapshotWorkspace((*workspaces)[n]);
    (*workspaces)[n].accelNum = QString::number(n);
    (*workspaces)[n].name = name;
}

void MW::reportWorkspace(int n)
{
    {
    #ifdef ISDEBUG
    qDebug() << "MW::reportWorkspace";
    #endif
    }
    ws = workspaces->at(n);
    qDebug() << "\n\nName" << ws.name
             << "\nAccel#" << ws.accelNum
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
             << "\nisThumbDockVisible" << ws.isThumbDockVisible
             << "\nisFolderLocked" << ws.isFolderDockLocked
             << "\nisFavLocked" << ws.isFavDockLocked
             << "\nisFilterLocked" << ws.isFilterDockLocked
             << "\nisMetadataLocked" << ws.isMetadataDockLocked
             << "\nisThumbsLocked" << ws.isThumbDockLocked
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
             << "\nisThumbWrap" << ws.isThumbWrapWhenTopOrBottomDock
             << "\nisAutoFit" << ws.isAutoFit
             << "\nisVerticalTitle" << ws.isVerticalTitle
             << "\nshowShootingInfo" << ws.isImageInfoVisible
             << "\nisLoupeDisplay" << ws.isLoupeDisplay
             << "\nisGridDisplay" << ws.isGridDisplay
             << "\nisTableDisplay" << ws.isTableDisplay
             << "\nisCompareDisplay" << ws.isCompareDisplay;
}

void MW::loadWorkspaces()
{
    int size = setting->beginReadArray("workspaces");
    for (int i = 0; i < size; ++i) {
        setting->setArrayIndex(i);
        ws.accelNum = setting->value("accelNum").toString();
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
        ws.isThumbDockVisible = setting->value("isThumbDockVisible").toBool();
        ws.isFolderDockLocked = setting->value("isFolderDockLocked").toBool();
        ws.isFavDockLocked = setting->value("isFavDockLocked").toBool();
        ws.isFilterDockLocked = setting->value("isFilterDockLocked").toBool();
        ws.isMetadataDockLocked = setting->value("isMetadataDockLocked").toBool();
        ws.isThumbDockLocked = setting->value("isThumbDockLocked").toBool();
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
        ws.isThumbWrapWhenTopOrBottomDock = setting->value("isThumbWrapWhenTopOrBottomDock").toBool();
        ws.isAutoFit = setting->value("isAutoFit").toBool();
        ws.isVerticalTitle = setting->value("isVerticalTitle").toBool();
        ws.isImageInfoVisible = setting->value("isImageInfoVisible").toBool();
        ws.isLoupeDisplay = setting->value("isLoupeDisplay").toBool();
        ws.isGridDisplay = setting->value("isGridDisplay").toBool();
        ws.isTableDisplay = setting->value("isTableDisplay").toBool();
        ws.isCompareDisplay = setting->value("isCompareDisplay").toBool();
        workspaces->append(ws);
    }
    setting->endArray();
}

void MW::reportState()
{
    {
    #ifdef ISDEBUG
    qDebug() << "MW::reportState";
    #endif
    }
    workspaceData w;
    snapshotWorkspace(w);
    qDebug() << "\nisMaximized" << w.isFullScreen
             << "\nisWindowTitleBarVisible" << w.isWindowTitleBarVisible
             << "\nisMenuBarVisible" << w.isMenuBarVisible
             << "\nisStatusBarVisible" << w.isStatusBarVisible
             << "\nisFolderDockVisible" << w.isFolderDockVisible
             << "\nisFavDockVisible" << w.isFavDockVisible
             << "\nisFilterDockVisible" << w.isFilterDockVisible
             << "\nisMetadataDockVisible" << w.isMetadataDockVisible
             << "\nisThumbDockVisible" << w.isThumbDockVisible
             << "\nisFolderLocked" << w.isFolderDockLocked
             << "\nisFavLocked" << w.isFavDockLocked
             << "\nisFilterLocked" << w.isFilterDockLocked
             << "\nisMetadataLocked" << w.isMetadataDockLocked
             << "\nisThumbsLocked" << w.isThumbDockLocked
             << "\nthumbSpacing" << w.thumbSpacing
             << "\nthumbPadding" << w.thumbPadding
             << "\nthumbWidth" << w.thumbWidth
             << "\nthumbHeight" << w.thumbHeight
             << "\nlabelFontSize" << w.labelFontSize
             << "\nshowThumbLabels" << w.showThumbLabels
             << "\nthumbSpacingGrid" << w.thumbSpacingGrid
             << "\nthumbPaddingGrid" << w.thumbPaddingGrid
             << "\nthumbWidthGrid" << w.thumbWidthGrid
             << "\nthumbHeightGrid" << w.thumbHeightGrid
             << "\nlabelFontSizeGrid" << w.labelFontSizeGrid
             << "\nshowThumbLabelsGrid" << w.showThumbLabelsGrid
             << "\nisThumbWrap" << w.isThumbWrapWhenTopOrBottomDock
             << "\nisAutoFit" << w.isAutoFit
             << "\nisVerticalTitle" << isThumbDockVerticalTitle
             << "\nshowShootingInfo" << w.isImageInfoVisible
             << "\nisLoupeDisplay" << w.isLoupeDisplay
             << "\nisGridDisplay" << w.isGridDisplay
             << "\nisTableDisplay" << w.isTableDisplay
             << "\nisCompareDisplay" << w.isCompareDisplay;
}

void MW::reportMetadata()
{
    {
    #ifdef ISDEBUG
    qDebug() << "MW::reportMetadata";
    #endif
    }
    thumbView->setCurrentIndex(dm->sf->index(0, 0));
    return;

    QTreeWidgetItemIterator it(filters);
    while (*it) {
        QString parentName;
        int dataModelColumn;
        bool isMatch = false;
        if ((*it)->parent()) {
            parentName = (*it)->parent()->text(0);
            qDebug() << "item text" << (*it)->text(0)
                     << "item value" << (*it)->data(1, Qt::EditRole)
                     << "parent" << parentName
                     << "data column" << dataModelColumn
                     << "check state" << (*it)->checkState(0);
        }
        else {
            dataModelColumn = (*it)->data(0, G::ColumnRole).toInt();
        }
        ++it;
    }

//    filters->iterateFilters();
//    metadata->readMetadata(true, thumbView->getCurrentFilename());
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
    execCommand = externalApps[((QAction*) sender())->text()];



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
                    thumbView->thumbViewModel->item(selectedIdxList[tn].row())->data(G::FileNameRole).toString();
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
    QMapIterator<QString, QString> eaIter(externalApps);

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

    qDebug() << "MW::chooseExternalApp";

    // in terminal this works:
    //open -a 'Adobe Photoshop CC 2015.5.app' /Users/roryhill/Pictures/4K/2017-01-25_0030-Edit.jpg

    // this launches photoshop but does not open jpg
    QProcess *process = new QProcess(this);
//    QString program = "/Applications/Preview.app";
//    QString program = "/Applications/Adobe Photoshop CC 2015.5/Adobe Photoshop CC 2015.5.app";
    QString program = "/Applications/Google Chrome.app";
    QStringList args;
//    args << "/Users/roryhill/Pictures/4K/2017-01-25_0030-Edit.jpg";
    args << "/Users/roryhill/Pictures/Eva/2016-10-25_0198.jpg";
    process->start(program, args);
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
    Prefdlg *prefdlg = new Prefdlg(this, lastPrefPage);
    connect(prefdlg, SIGNAL(updatePage(int)),
        this, SLOT(setPrefPage(int)));
    connect(prefdlg, SIGNAL(updateRememberFolder(bool)),
            this, SLOT(setRememberLastDir(bool)));
    connect(prefdlg, SIGNAL(updateMaxRecentFolders(int)),
            this, SLOT(setMaxRecentFolders(int)));
    connect(prefdlg, SIGNAL(updateThumbParameters(int,int,int,int,int,bool)),
            thumbView, SLOT(setThumbParameters(int, int, int, int, int, bool)));
    connect(prefdlg, SIGNAL(updateThumbGridParameters(int,int,int,int,int,bool)),
            thumbView, SLOT(setThumbGridParameters(int, int, int, int, int, bool)));
    connect(prefdlg, SIGNAL(updateThumbDockParameters(bool, bool, bool)),
            this, SLOT(setThumbDockParameters(bool, bool, bool)));
    connect(prefdlg, SIGNAL(updateSlideShowParameters(int, bool)),
            this, SLOT(setSlideShowParameters(int, bool)));
    connect(prefdlg, SIGNAL(updateCacheParameters(int, bool, int, int, bool, int, int)),
            this, SLOT(setCacheParameters(int, bool, int, int, bool, int, int)));
    connect(prefdlg, SIGNAL(updateFullScreenDocks(bool,bool,bool,bool,bool)),
            this, SLOT(setFullScreenDocks(bool,bool,bool,bool,bool)));
    prefdlg->exec();
}

void MW::setIncludeSubFolders()
{
    {
    #ifdef ISDEBUG
    qDebug() << "MW::setIncludeSubFolders";
    #endif
    }
    inclSubfolders = subFoldersAction->isChecked();
    folderSelectionChange();
}

void MW::setPrefPage(int page)
{
    {
    #ifdef ISDEBUG
    qDebug() << "MW::setPrefPage";
    #endif
    }
    lastPrefPage = page;
}

void MW::setRememberLastDir(bool prefRememberFolder)
{
    {
    #ifdef ISDEBUG
    qDebug() << "MW::setRememberLastDir";
    #endif
    }
    rememberLastDir = prefRememberFolder;
}

void MW::setMaxRecentFolders(int prefMaxRecentFolders)
{
    {
    #ifdef ISDEBUG
    qDebug() << "MW::setMaxRecentFolders";
    #endif
    }
    maxRecentFolders = prefMaxRecentFolders;
    syncRecentFoldersMenu();
}

void MW::setFullScreenDocks(bool isFolders, bool isFavs, bool isMetadata, bool isThumbs, bool isStatusBar)
{
    {
    #ifdef ISDEBUG
    qDebug() << "MW::setFullScreenDocks";
    #endif
    }
    fullScreenDocks.isFolders = isFolders;
    fullScreenDocks.isFavs = isFavs;
    fullScreenDocks.isMetadata = isMetadata;
    fullScreenDocks.isThumbs = isThumbs;
    fullScreenDocks.isStatusBar = isStatusBar;
}

//void MW::setGeneralParameters(bool prefRememberFolder, bool prefInclSubfolders,
//                              int prefMaxRecentFolders)
//{
//    rememberLastDir = prefRememberFolder;
//    inclSubfolders = prefInclSubfolders;
//    maxRecentFolders = prefMaxRecentFolders;
//}

void MW::oldPreferences()
{
//    SettingsDialog *dialog = new SettingsDialog(this);
//    dialog->exec();
//    delete dialog;
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
    if (fullScreenAction->isChecked())
    {
        qDebug() << "fullScreenDocks.isThumbs" << fullScreenDocks.isThumbs;
        snapshotWorkspace(ws);
        showFullScreen();
        imageView->setCursorHiding(true);
        folderDockVisibleAction->setChecked(fullScreenDocks.isFolders);
        setFolderDockVisibility();
        favDockVisibleAction->setChecked(fullScreenDocks.isFavs);
        setFavDockVisibility();
        filterDockVisibleAction->setChecked(fullScreenDocks.isFavs);
        setFilterDockVisibility();
        metadataDockVisibleAction->setChecked(fullScreenDocks.isMetadata);
        setMetadataDockVisibility();
        thumbDockVisibleAction->setChecked(fullScreenDocks.isThumbs);
        setThumbDockVisibity();
        menuBarVisibleAction->setChecked(false);
        setMenuBarVisibility();
        statusBarVisibleAction->setChecked(fullScreenDocks.isThumbs);
        setStatusBarVisibility();
    }
    else
    {
        showNormal();
        invokeWorkspace(ws);
//        imageView->setCursorHiding(false);
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

void MW::zoomIn()
{
    {
    #ifdef ISDEBUG
    qDebug() << "MW::zoomIn";
    #endif
    }
    if (asLoupeAction) imageView->zoomIn();
    if (asCompareAction) compareImages->zoomIn();
}

void MW::zoomOut()
{
    {
    #ifdef ISDEBUG
    qDebug() << "MW::zoomOut";
    #endif
    }
    if (asLoupeAction) imageView->zoomOut();
    if (asCompareAction) compareImages->zoomOut();
}

void MW::zoomToFit()
{
    {
    #ifdef ISDEBUG
    qDebug() << "MW::zoomToFit";
    #endif
    }
    if (asLoupeAction) imageView->zoomToFit();
    if (asCompareAction) compareImages->zoomToFit();
}

void MW::zoom50()
{
    {
    #ifdef ISDEBUG
    qDebug() << "MW::zoom50";
    #endif
    }
    if (asLoupeAction) imageView->zoom50();
    if (asCompareAction) compareImages->zoom50();
    popUp->showPopup(this, "Click zoom set to 50%", 1000, 0.5);
}

void MW::zoom100()
{
    {
    #ifdef ISDEBUG
    qDebug() << "MW::zoom100";
    #endif
    }
    if (asLoupeAction) imageView->zoom100();
    if (asCompareAction) compareImages->zoomToFit();
    popUp->showPopup(this, "Click zoom set to 100%", 1000, 0.5);
}

void MW::zoom200()
{
    {
    #ifdef ISDEBUG
    qDebug() << "MW::zoom200";
    #endif
    }
    if (asLoupeAction) imageView->zoom200();
    if (asCompareAction) compareImages->zoomToFit();
    popUp->showPopup(this, "Click zoom set to 200%", 1000, 0.5);
}

void MW::zoomToggle()
{
    {
    #ifdef ISDEBUG
    qDebug() << "MW::zoomToggle";
    #endif
    }
    if (asLoupeAction) imageView->zoomToggle();
    if (asCompareAction) compareImages->zoomToggle();
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
    qDebug() << "MW::removeBookmark";
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

// rgh used?
void MW::setThumbsFilter()
{
    {
    #ifdef ISDEBUG
    qDebug() << "MW::setThumbsFilter";
    #endif
    }
    thumbView->filterStr = filterBar->text();
//    refreshThumbs(true);
}

// rgh used?
void MW::clearThumbsFilter()
{
    {
    #ifdef ISDEBUG
    qDebug() << "MW::clearThumbsFilter";
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
    // general
    setting->setValue("lastPrefPage", (int)lastPrefPage);
    // files
//    setting->setValue("showHiddenFiles", (bool)G::showHiddenFiles);
    setting->setValue("rememberLastDir", rememberLastDir);
    setting->setValue("lastDir", currentViewDir);
    setting->setValue("includeSubfolders", subFoldersAction->isChecked());
    setting->setValue("maxRecentFolders", maxRecentFolders);
    // thumbs
    setting->setValue("thumbSpacing", thumbView->thumbSpacing);
    setting->setValue("thumbPadding", thumbView->thumbPadding);
    setting->setValue("thumbWidth", thumbView->thumbWidth);
//    qDebug() << "MW::writeSettings thumbView->thumbHeight" << thumbView->thumbHeight;
    setting->setValue("thumbHeight", thumbView->thumbHeight);
    setting->setValue("labelFontSize", thumbView->labelFontSize);
    setting->setValue("showLabels", (bool)showThumbLabelsAction->isChecked());
    setting->setValue("thumbSpacingGrid", thumbView->thumbSpacingGrid);
    setting->setValue("thumbPaddingGrid", thumbView->thumbPaddingGrid);
    setting->setValue("thumbWidthGrid", thumbView->thumbWidthGrid);
    setting->setValue("thumbHeightGrid", thumbView->thumbHeightGrid);
    setting->setValue("labelFontSizeGrid", thumbView->labelFontSizeGrid);
    setting->setValue("showLabelsGrid", (bool)thumbView->showThumbLabelsGrid);
    setting->setValue("isThumbWrapWhenTopOrBottomDock", (bool)thumbView->isThumbWrapWhenTopOrBottomDock);
    setting->setValue("isAutoFit", (bool)thumbView->isAutoFit);
    setting->setValue("isVerticalTitle", (bool)isThumbDockVerticalTitle);
    // slideshow
    setting->setValue("slideShowDelay", (int)slideShowDelay);
    setting->setValue("slideShowRandom", (bool)slideShowRandom);
    setting->setValue("slideShowWrap", (bool)slideShowWrap);
    // cache
    setting->setValue("cacheSizeMB", (int)cacheSizeMB);
    setting->setValue("isShowCacheStatus", (bool)isShowCacheStatus);
    setting->setValue("cacheStatusWidth", (int)cacheStatusWidth);
    setting->setValue("cacheWtAhead", (int)cacheWtAhead);
    setting->setValue("isCachePreview", (int)isCachePreview);
    setting->setValue("cachePreviewWidth", (int)cachePreviewWidth);
    setting->setValue("cachePreviewHeight", (int)cachePreviewHeight);
    // full screen
    setting->setValue("isFullScreenFolders", (bool)fullScreenDocks.isFolders);
    setting->setValue("isFullScreenFavs", (bool)fullScreenDocks.isFavs);
    setting->setValue("isFullScreenMetadata", (bool)fullScreenDocks.isMetadata);
    setting->setValue("isFullScreenThumbs", (bool)fullScreenDocks.isThumbs);
    setting->setValue("isFullScreenStatusBar", (bool)fullScreenDocks.isStatusBar);
    // state
    setting->setValue("Geometry", saveGeometry());
    setting->setValue("WindowState", saveState());
    setting->setValue("isFullScreen", (bool)isFullScreen());
//    setting->setValue("isFullScreen", (bool)fullScreenAction->isChecked());

    setting->setValue("isImageInfoVisible", (bool)infoVisibleAction->isChecked());
    setting->setValue("isLoupeDisplay", (bool)asLoupeAction->isChecked());
    setting->setValue("isGridDisplay", (bool)asGridAction->isChecked());
    setting->setValue("isTableDisplay", (bool)asTableAction->isChecked());
    setting->setValue("isCompareDisplay", (bool)asCompareAction->isChecked());

    setting->setValue("isWindowTitleBarVisible", (bool)windowTitleBarVisibleAction->isChecked());
    setting->setValue("isMenuBarVisible", (bool)menuBarVisibleAction->isChecked());
    setting->setValue("isStatusBarVisible", (bool)statusBarVisibleAction->isChecked());
    setting->setValue("isFolderDockVisible", (bool)folderDockVisibleAction->isChecked());
    setting->setValue("isFavDockVisible", (bool)favDockVisibleAction->isChecked());
    setting->setValue("isFilterDockVisible", (bool)filterDockVisibleAction->isChecked());
    setting->setValue("isMetadataDockVisible", (bool)metadataDockVisibleAction->isChecked());
    setting->setValue("isThumbDockVisible", (bool)thumbDockVisibleAction->isChecked());
    setting->setValue("isFolderDockLocked", (bool)folderDockLockAction->isChecked());
    setting->setValue("isFavDockLocked", (bool)favDockLockAction->isChecked());
    setting->setValue("isFilterDockLocked", (bool)filterDockLockAction->isChecked());
    setting->setValue("isMetadataDockLocked", (bool)metadataDockLockAction->isChecked());
    setting->setValue("isThumbDockLocked", (bool)thumbDockLockAction->isChecked());

    // not req'd
//    setting->setValue("thumbsSortFlags", (int)thumbView->thumbsSortFlags);

//    setting->setValue("thumbsZoomVal", (int)thumbView->thumbSize);

    /* Action shortcuts */
//    GData::appSettings->beginGroup("Shortcuts");
//    QMapIterator<QString, QAction *> scIter(actionKeys);
//    while (scIter.hasNext()) {
//        scIter.next();
//        GData::appSettings->setValue(scIter.key(), scIter.value()->shortcut().toString());
//    }
//    GData::appSettings->endGroup();

    /* External apps */
    setting->beginGroup("ExternalApps");
    setting->remove("");
    QMapIterator<QString, QString> eaIter(externalApps);
    while (eaIter.hasNext()) {
        eaIter.next();
        setting->setValue(eaIter.key(), eaIter.value());
    }
    setting->endGroup();

    /* save bookmarks */
    int idx = 0;
    setting->beginGroup("Bookmarks");
    setting->remove("");
    QSetIterator<QString> pathsIter(bookmarks->bookmarkPaths);
    while (pathsIter.hasNext()) {
        setting->setValue("path" + QString::number(++idx), pathsIter.next());
    }
    setting->endGroup();

    /* save recent folders */
    setting->beginGroup("RecentFolders");
    setting->remove("");
    for (int i=0; i < recentFolders->count(); i++) {
        setting->setValue("recentFolder" + QString::number(i+1),
                          recentFolders->at(i));
    }
    setting->endGroup();

    // save workspaces
    setting->beginWriteArray("workspaces");
    for (int i = 0; i < workspaces->count(); ++i) {
        ws = workspaces->at(i);
        setting->setArrayIndex(i);
        setting->setValue("accelNum", ws.accelNum);
        setting->setValue("name", ws.name);
        setting->setValue("geometry", ws.geometry);
        setting->setValue("state", ws.state);
        setting->setValue("isFullScreen", ws.isFullScreen);
        setting->setValue("isWindowTitleBarVisible", ws.isWindowTitleBarVisible);
        setting->setValue("isMenuBarBarVisible", ws.isMenuBarVisible);
        setting->setValue("isStatusBarVisible", ws.isStatusBarVisible);
        setting->setValue("isFolderDockVisible", ws.isFolderDockVisible);
        setting->setValue("isFavDockVisible", ws.isFavDockVisible);
        setting->setValue("isFilterDockVisible", ws.isFilterDockVisible);
        setting->setValue("isMetadataDockVisible", ws.isMetadataDockVisible);
        setting->setValue("isThumbDockVisible", ws.isThumbDockVisible);
        setting->setValue("isFolderDockLocked", ws.isFolderDockLocked);
        setting->setValue("isFavDockLocked", ws.isFavDockLocked);
        setting->setValue("isFilterDockLocked", ws.isFilterDockLocked);
        setting->setValue("isMetadataDockLocked", ws.isMetadataDockLocked);
        setting->setValue("isThumbDockLocked", ws.isThumbDockLocked);
        setting->setValue("thumbSpacing", ws.thumbSpacing);
        setting->setValue("thumbPadding", ws.thumbPadding);
        setting->setValue("thumbWidth", ws.thumbWidth);
        setting->setValue("thumbHeight", ws.thumbHeight);
        setting->setValue("labelFontSize", ws.labelFontSize);
        setting->setValue("showThumbLabels", ws.showThumbLabels);
        setting->setValue("thumbSpacingGrid", ws.thumbSpacingGrid);
        setting->setValue("thumbPaddingGrid", ws.thumbPaddingGrid);
        setting->setValue("thumbWidthGrid", ws.thumbWidthGrid);
        setting->setValue("thumbHeightGrid", ws.thumbHeightGrid);
        setting->setValue("labelFontSizeGrid", ws.labelFontSizeGrid);
        setting->setValue("showThumbLabelsGrid", ws.showThumbLabelsGrid);
        setting->setValue("isThumbWrapWhenTopOrBottomDock", ws.isThumbWrapWhenTopOrBottomDock);
        setting->setValue("isAutoFit", ws.isAutoFit);
        setting->setValue("isVerticalTitle", ws.isVerticalTitle);
        setting->setValue("isImageInfoVisible", ws.isImageInfoVisible);
        setting->setValue("isLoupeDisplay", ws.isLoupeDisplay);
        setting->setValue("isGridDisplay", ws.isGridDisplay);
        setting->setValue("isTableDisplay", ws.isTableDisplay);
        setting->setValue("isCompareDisplay", ws.isCompareDisplay);
    }
    setting->endArray();
}

bool MW::loadSettings()
{
/* Persistant settings from QSettings fall into two categories:
1.  Action settings
2.  Preferences

Action settings are maintained by the actions ie action->isChecked();
They are updated on creation.

Preferences are located in the prefdlg class and updated here.
*/
    {
    #ifdef ISDEBUG
    qDebug() << "MW::loadSettings";
    #endif
    }
    needThumbsRefresh = false;

    // default values for first time use
    if (!setting->contains("cacheSizeMB")) {
        // general
        lastPrefPage = 0;
        rememberLastDir = true;
        maxRecentFolders = 10;
        bookmarks->bookmarkPaths.insert(QDir::homePath());

      // slideshow
        slideShowDelay = 5;
        slideShowRandom = false;
        slideShowWrap = true;

        // cache
        cacheSizeMB = 2000;
        isShowCacheStatus = true;
        cacheStatusWidth = 200;
        cacheWtAhead = 5;
        isCachePreview = true;
        cachePreviewWidth = 2000;
        cachePreviewHeight = 1600;

        return false;
    }

    // general
    lastPrefPage = setting->value("lastPrefPage").toInt();

    // files
//    G::showHiddenFiles = setting->value("showHiddenFiles").toBool();
    rememberLastDir = setting->value("rememberLastDir").toBool();
    lastDir = setting->value("lastDir").toString();
    maxRecentFolders = setting->value("maxRecentFolders").toInt();

    // thumbs (set in thumbView creation)
    isThumbDockVerticalTitle = setting->value("isVerticalTitle").toBool();

// rgh make sure to add thumb sort to workspaces when implemented
//    setting->setValue("thumbsSortFlags", (int)0);

    // slideshow
    slideShowDelay = setting->value("slideShowDelay").toInt();
    slideShowRandom = setting->value("slideShowRandom").toBool();
    slideShowWrap = setting->value("slideShowWrap").toBool();
    // cache
    cacheSizeMB = setting->value("cacheSizeMB").toInt();
    isShowCacheStatus = setting->value("isShowCacheStatus").toBool();
    cacheStatusWidth = setting->value("cacheStatusWidth").toInt();
    cacheWtAhead = setting->value("cacheWtAhead").toInt();
    isCachePreview = setting->value("isCachePreview").toBool();
    cachePreviewWidth = setting->value("cachePreviewWidth").toInt();
    cachePreviewHeight = setting->value("cachePreviewHeight").toInt();
    // full screen
    fullScreenDocks.isFolders = setting->value("isFullScreenFolders").toBool();
    fullScreenDocks.isFavs = setting->value("isFullScreenFavs").toBool();
    fullScreenDocks.isMetadata = setting->value("isFullScreenMetadata").toBool();
    fullScreenDocks.isThumbs = setting->value("isFullScreenThumbs").toBool();
    fullScreenDocks.isStatusBar = setting->value("isFullScreenStatusBar").toBool();

    // load state (action->setChecked in action creation)
    fullScreenAction->setChecked(setting->value("isFullScreen").toBool());
    infoVisibleAction->setChecked(setting->value("isImageInfoVisible").toBool());  // from QSettings
    asLoupeAction->setChecked(setting->value("isLoupeDisplay").toBool() ||
                              setting->value("isCompareDisplay").toBool());
    asGridAction->setChecked(setting->value("isGridDisplay").toBool());
    asTableAction->setChecked(setting->value("isTableDisplay").toBool());
    asCompareAction->setChecked(false); // never start with compare set true
    showThumbLabelsAction->setChecked(thumbView->showThumbLabels);

    windowTitleBarVisibleAction->setChecked(setting->value("isWindowTitleBarVisible").toBool());
    menuBarVisibleAction->setChecked(setting->value("isMenuBarVisible").toBool());
    statusBarVisibleAction->setChecked(setting->value("isStatusBarVisible").toBool());
    folderDockVisibleAction->setChecked(setting->value("isFolderDockVisible").toBool());
    favDockVisibleAction->setChecked(setting->value("isFavDockVisible").toBool());
    filterDockVisibleAction->setChecked(setting->value("isFilterDockVisible").toBool());
    metadataDockVisibleAction->setChecked(setting->value("isMetadataDockVisible").toBool());
    thumbDockVisibleAction->setChecked(setting->value("isThumbDockVisible").toBool());
    folderDockLockAction->setChecked(setting->value("isFolderDockLocked").toBool());
    favDockLockAction->setChecked(setting->value("isFavDockLocked").toBool());
    filterDockLockAction->setChecked(setting->value("isFilterDockLocked").toBool());
    metadataDockLockAction->setChecked(setting->value("isMetadataDockLocked").toBool());
    thumbDockLockAction->setChecked(setting->value("isThumbDockLocked").toBool());
    if (folderDockLockAction->isChecked() &&
        favDockLockAction->isChecked() &&
        filterDockLockAction->isChecked() &&
        metadataDockLockAction->isChecked() &&
        thumbDockLockAction->isChecked())
        allDocksLockAction->setChecked(true);
    isThumbDockVisibleBeforeGridViewInvoked = thumbDockVisibleAction->isChecked();

    /* read external apps */
    setting->beginGroup("ExternalApps");
    QStringList extApps = setting->childKeys();
    for (int i = 0; i < extApps.size(); ++i) {
        externalApps[extApps.at(i)] = setting->value(extApps.at(i)).toString();
    }
    setting->endGroup();

    /* read bookmarks */
    setting->beginGroup("Bookmarks");
    QStringList paths = setting->childKeys();
    for (int i = 0; i < paths.size(); ++i) {
        bookmarks->bookmarkPaths.insert(setting->value(paths.at(i)).toString());
    }
    bookmarks->reloadBookmarks();
    setting->endGroup();

    /* read recent folders */
    setting->beginGroup("RecentFolders");
    QStringList recentList = setting->childKeys();
    for (int i = 0; i < recentList.size(); ++i) {
        recentFolders->append(setting->value(recentList.at(i)).toString());
    }
    setting->endGroup();

    // moved read workspaces to separate function as req'd by actions while the
    // rest of QSettings are dependent on actions being defined first.

    /* read workspaces */
//    int size = setting->beginReadArray("workspaces");
//    for (int i = 0; i < size; ++i) {
//        setting->setArrayIndex(i);
//        ws.accelNum = setting->value("accelNum").toString();
//        ws.name = setting->value("name").toString();
//        ws.geometry = setting->value("geometry").toByteArray();
//        ws.state = setting->value("state").toByteArray();
//        ws.isFullScreen = setting->value("isFullScreen").toBool();
//        ws.isWindowTitleBarVisible = setting->value("isWindowTitleBarVisible").toBool();
//        ws.isMenuBarVisible = setting->value("isMenuBarVisible").toBool();
//        ws.isStatusBarVisible = setting->value("isStatusBarVisible").toBool();
//        ws.isFolderDockVisible = setting->value("isFolderDockVisible").toBool();
//        ws.isFavDockVisible = setting->value("isFavDockVisible").toBool();
//        ws.isMetadataDockVisible = setting->value("isMetadataDockVisible").toBool();
//        ws.isThumbDockVisible = setting->value("isThumbDockVisible").toBool();
//        ws.isFolderDockLocked = setting->value("isFolderDockLocked").toBool();
//        ws.isFavDockLocked = setting->value("isFavDockLocked").toBool();
//        ws.isMetadataDockLocked = setting->value("isMetadataDockLocked").toBool();
//        ws.isThumbDockLocked = setting->value("isThumbDockLocked").toBool();
//        ws.thumbSpacing = setting->value("thumbSpacing").toInt();
//        ws.thumbPadding = setting->value("thumbPadding").toInt();
//        ws.thumbWidth = setting->value("thumbWidth").toInt();
//        ws.thumbHeight = setting->value("thumbHeight").toInt();
//        ws.labelFontSize = setting->value("labelFontSize").toInt();
//        ws.showThumbLabels = setting->value("showThumbLabels").toBool();
//        ws.thumbSpacingGrid = setting->value("thumbSpacingGrid").toInt();
//        ws.thumbPaddingGrid = setting->value("thumbPaddingGrid").toInt();
//        ws.thumbWidthGrid = setting->value("thumbWidthGrid").toInt();
//        ws.thumbHeightGrid = setting->value("thumbHeightGrid").toInt();
//        ws.labelFontSizeGrid = setting->value("labelFontSizeGrid").toInt();
//        ws.showThumbLabelsGrid = setting->value("showThumbLabelsGrid").toBool();
//        ws.isThumbWrapWhenTopOrBottomDock = setting->value("isThumbWrapWhenTopOrBottomDock").toBool();
//        ws.isAutoFit = setting->value("isAutoFit").toBool();
//        ws.isVerticalTitle = setting->value("isVerticalTitle").toBool();
//        ws.isImageInfoVisible = setting->value("isImageInfoVisible").toBool();
//        ws.isLoupeDisplay = setting->value("isLoupeDisplay").toBool();
//        ws.isGridDisplay = setting->value("isGridDisplay").toBool();
//        ws.isTableDisplay = setting->value("isTableDisplay").toBool();
//        ws.isCompareDisplay = setting->value("isCompareDisplay").toBool();
//        workspaces->append(ws);
//    }
//    setting->endArray();

    return true;
}

void MW::loadShortcuts(bool defaultShortcuts)
{
    {
    #ifdef ISDEBUG
    qDebug() << "MW::loadShortcuts";
    #endif
    }
    // Add customizable key shortcut actions
    actionKeys[fullScreenAction->objectName()] = fullScreenAction;
    actionKeys[escapeFullScreenAction->objectName()] = escapeFullScreenAction;
    actionKeys[prefAction->objectName()] = prefAction;
    actionKeys[exitAction->objectName()] = exitAction;
    actionKeys[thumbsEnlargeAction->objectName()] = thumbsEnlargeAction;
    actionKeys[thumbsShrinkAction->objectName()] = thumbsShrinkAction;
//    actionKeys[cutAction->objectName()] = cutAction;
//    actionKeys[copyAction->objectName()] = copyAction;
    actionKeys[nextThumbAction->objectName()] = nextThumbAction;
    actionKeys[prevThumbAction->objectName()] = prevThumbAction;
    actionKeys[downThumbAction->objectName()] = downThumbAction;
    actionKeys[upThumbAction->objectName()] = upThumbAction;
//    actionKeys[keepTransformAct->objectName()] = keepTransformAct;
//    actionKeys[keepZoomAct->objectName()] = keepZoomAct;
//    actionKeys[copyImageAction->objectName()] = copyImageAction;
//    actionKeys[pasteImageAction->objectName()] = pasteImageAction;
//    actionKeys[refreshAction->objectName()] = refreshAction;
//    actionKeys[pasteAction->objectName()] = pasteAction;
    actionKeys[togglePickAction->objectName()] = togglePickAction;
    actionKeys[toggleFilterPickAction->objectName()] = toggleFilterPickAction;
    actionKeys[ingestAction->objectName()] = ingestAction;
    actionKeys[reportMetadataAction->objectName()] = reportMetadataAction;
    actionKeys[slideShowAction->objectName()] = slideShowAction;
    actionKeys[firstThumbAction->objectName()] = firstThumbAction;
    actionKeys[lastThumbAction->objectName()] = lastThumbAction;
    actionKeys[randomImageAction->objectName()] = randomImageAction;
    actionKeys[openAction->objectName()] = openAction;
    actionKeys[zoomOutAction->objectName()] = zoomOutAction;
    actionKeys[zoomInAction->objectName()] = zoomInAction;
    actionKeys[zoomToggleAction->objectName()] = zoomToggleAction;
    actionKeys[rotateLeftAction->objectName()] = rotateLeftAction;
    actionKeys[rotateRightAction->objectName()] = rotateRightAction;
//    actionKeys[moveRightAct->objectName()] = moveRightAct;
//    actionKeys[moveLeftAct->objectName()] = moveLeftAct;
//    actionKeys[copyToAction->objectName()] = copyToAction;
//    actionKeys[moveToAction->objectName()] = moveToAction;
//    actionKeys[keepTransformAct->objectName()] = keepTransformAct;
    actionKeys[infoVisibleAction->objectName()] = infoVisibleAction;
//    actionKeys[toggleAllDocksAct->objectName()] = toggleAllDocksAct;
    actionKeys[folderDockVisibleAction->objectName()] = folderDockVisibleAction;
    actionKeys[favDockVisibleAction->objectName()] = favDockVisibleAction;
    actionKeys[filterDockVisibleAction->objectName()] = filterDockVisibleAction;
    actionKeys[metadataDockVisibleAction->objectName()] = metadataDockVisibleAction;
    actionKeys[thumbDockVisibleAction->objectName()] = thumbDockVisibleAction;
    actionKeys[windowTitleBarVisibleAction->objectName()] = windowTitleBarVisibleAction;
    actionKeys[menuBarVisibleAction->objectName()] = menuBarVisibleAction;
    actionKeys[statusBarVisibleAction->objectName()] = statusBarVisibleAction;
//    actionKeys[toggleIconsListAction->objectName()] = toggleIconsListAction;
    actionKeys[allDocksLockAction->objectName()] = allDocksLockAction;

    setting->beginGroup("Shortcuts");
    QStringList groupKeys = setting->childKeys();

    if (groupKeys.size() && !defaultShortcuts)
    {
        if (groupKeys.contains(exitAction->text())) //rgh find a better way
        {
            QMapIterator<QString, QAction *> key(actionKeys);
            while (key.hasNext()) {
                key.next();
                if (groupKeys.contains(key.value()->text()))
                {
                    key.value()->setShortcut(setting->value(key.value()->text()).toString());
                    setting->remove(key.value()->text());
                    setting->setValue(key.key(), key.value()->shortcut().toString());
                }
            }
        }
        else
        {
            for (int i = 0; i < groupKeys.size(); ++i)
            {
                if (actionKeys.value(groupKeys.at(i)))
                    actionKeys.value(groupKeys.at(i))->setShortcut
                                    (setting->value(groupKeys.at(i)).toString());
            }
        }
    }
    else    // default shortcuts
    {
//        qDebug() << "Default shortcuts";
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
        subFoldersAction->setShortcut(QKeySequence("B"));
        fullScreenAction->setShortcut(QKeySequence("F"));
        escapeFullScreenAction->setShortcut(QKeySequence("Esc"));
        prefAction->setShortcut(QKeySequence("Ctrl+P"));
        exitAction->setShortcut(QKeySequence("Ctrl+Q"));
        togglePickAction->setShortcut(QKeySequence("`"));
        toggleFilterPickAction->setShortcut(QKeySequence("Ctrl+`"));
        ingestAction->setShortcut(QKeySequence("Q"));
        rate0Action->setShortcut(QKeySequence("!"));
        rate1Action->setShortcut(QKeySequence("1"));
        rate2Action->setShortcut(QKeySequence("2"));
        rate3Action->setShortcut(QKeySequence("3"));
        rate4Action->setShortcut(QKeySequence("4"));
        rate5Action->setShortcut(QKeySequence("5"));
        label0Action->setShortcut(QKeySequence("^"));
        label1Action->setShortcut(QKeySequence("6"));
        label2Action->setShortcut(QKeySequence("7"));
        label3Action->setShortcut(QKeySequence("8"));
        label4Action->setShortcut(QKeySequence("9"));
        label5Action->setShortcut(QKeySequence("0"));
        reportMetadataAction->setShortcut(QKeySequence("Ctrl+R"));
        slideShowAction->setShortcut(QKeySequence("S"));
        thumbsFitAction->setShortcut(QKeySequence("Alt+}"));
        thumbsEnlargeAction->setShortcut(QKeySequence("}"));
        thumbsShrinkAction->setShortcut(QKeySequence("{"));
        nextThumbAction->setShortcut(QKeySequence("Right"));
        prevThumbAction->setShortcut(QKeySequence("Left"));
        firstThumbAction->setShortcut(QKeySequence("Home"));
        lastThumbAction->setShortcut(QKeySequence("End"));
        downThumbAction->setShortcut(QKeySequence("Down"));
        upThumbAction->setShortcut(QKeySequence("Up"));
        randomImageAction->setShortcut(QKeySequence("Ctrl+Alt+Right"));
        nextPickAction->setShortcut(QKeySequence("Alt+Right"));
        prevPickAction->setShortcut(QKeySequence("Alt+Left"));
        openAction->setShortcut(QKeySequence("O"));
        asLoupeAction->setShortcut(QKeySequence("E"));
        asGridAction->setShortcut(QKeySequence("G"));
        asTableAction->setShortcut(QKeySequence("T"));
        asCompareAction->setShortcut(QKeySequence("C"));
        revealFileAction->setShortcut(QKeySequence("Ctrl+F"));
        zoomOutAction->setShortcut(QKeySequence("-"));
        zoomInAction->setShortcut(QKeySequence("+"));
        zoomToggleAction->setShortcut(QKeySequence("Z"));
//        zoom50PctAction->setShortcut(QKeySequence("Ctrl+5"));
//        zoom100PctAction->setShortcut(QKeySequence("Ctrl+0"));
//        zoom200PctAction->setShortcut(QKeySequence("Ctrl+2"));
        rotateLeftAction->setShortcut(QKeySequence("["));
        rotateRightAction->setShortcut(QKeySequence("]"));
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
        folderDockVisibleAction->setShortcut(QKeySequence("F3"));
        favDockVisibleAction->setShortcut(QKeySequence("F4"));
        filterDockVisibleAction->setShortcut(QKeySequence("F5"));
        metadataDockVisibleAction->setShortcut(QKeySequence("F6"));
        thumbDockVisibleAction->setShortcut(QKeySequence("F7"));
        windowTitleBarVisibleAction->setShortcut(QKeySequence("F8"));
        menuBarVisibleAction->setShortcut(QKeySequence("F9"));
        statusBarVisibleAction->setShortcut(QKeySequence("F10"));
        folderDockLockAction->setShortcut(QKeySequence("Shift+F3"));
        favDockLockAction->setShortcut(QKeySequence("Shift+F4"));
        filterDockLockAction->setShortcut(QKeySequence("Shift+F5"));
        metadataDockLockAction->setShortcut(QKeySequence("Shift+F6"));
        thumbDockLockAction->setShortcut(QKeySequence("Shift+F7"));
        allDocksLockAction->setShortcut(QKeySequence("Ctrl+L"));
        helpAction->setShortcut(QKeySequence("?"));
//        toggleIconsListAction->setShortcut(QKeySequence("Ctrl+T"));
    }

    setting->endGroup();
}

void MW::updateState()
{
/*
Called when program starting and when a workspace is invoked.  Based on the
condition of actions sets the visibility of all window components. */
    {
    #ifdef ISDEBUG
    qDebug() << "MW::updateState";
    #endif
    }
    // set flag so
    isUpdatingState = true;
    setMenuBarVisibility();
    setStatusBarVisibility();
    setFolderDockVisibility();
    setFavDockVisibility();
    setFilterDockVisibility();
    setMetadataDockVisibility();
    setThumbDockVisibity();
    setFolderDockLockMode();
    setFavDockLockMode();
    setFilterDockLockMode();
    setMetadataDockLockMode();
    setThumbDockLockMode();
    setShootingInfo();
    setCentralView();
    setThumbDockFeatures(dockWidgetArea(thumbDock));
    isUpdatingState = false;
//    reportState();
}

/*****************************************************************************************
 * HIDE/SHOW UI ELEMENTS
 * **************************************************************************************/

void MW::setThumbDockFloatFeatures(bool isFloat)
{
    {
    #ifdef ISDEBUG
    qDebug() << "MW::setThumbDockFloatFeatures";
    #endif
    }
    if (isFloat) {
        thumbView->setMaximumHeight(100000);
        thumbDock->setFeatures(QDockWidget::DockWidgetClosable |
                               QDockWidget::DockWidgetMovable  |
                               QDockWidget::DockWidgetFloatable);
        thumbView->setWrapping(true);
        thumbView->isFloat = isFloat;
    }
}

void MW::setThumbDockHeight()
{
/*
Helper slot to call setThumbDockFeatures when the dockWidgetArea is not known,
which is the case when calling from another class like thumbView after thumbnails
have been resized.
*/
    {
    #ifdef ISDEBUG
    qDebug() << "MW::setThumbDockHeight";
    #endif
    }
    setThumbDockFeatures(dockWidgetArea(thumbDock));
}

void MW::setThumbDockFeatures(Qt::DockWidgetArea area)
{
/*
When the thumbDock is moved or when the thumbnails have been resized set the
thumbDock features accordingly, based on settings in preferences for wrapping
and titlebar visibility.
*/
    {
    #ifdef ISDEBUG
    qDebug() << "MW::setThumbDockFeatures";
    #endif
    }
//    qDebug() << "DockWidgetArea =" << area;

    thumbView->setMaximumHeight(100000);

    if ((area == Qt::BottomDockWidgetArea || area == Qt::TopDockWidgetArea)
         && isThumbDockVerticalTitle)
    {
        thumbDock->setFeatures(QDockWidget::DockWidgetClosable |
                               QDockWidget::DockWidgetMovable  |
                               QDockWidget::DockWidgetFloatable |
                               QDockWidget::DockWidgetVerticalTitleBar);
        // if thumbDock area changed then set dock height to thumb sizw
        if (!thumbView->isThumbWrapWhenTopOrBottomDock &&
            !thumbDock->isFloating() &&
            !asGridAction->isChecked()) // && dm->sf->rowCount() > 0)
        {
            // make thumbDock height fit thumbs
            int maxHt = thumbView->getThumbSpaceMax();
            int minHt = thumbView->getThumbSpaceMin();
            int scrollBarHeight = qApp->style()->pixelMetric(QStyle::PM_ScrollBarExtent);;

            int newThumbDockHeight = thumbView->getThumbCellSize().height();

            // horizontal scrollBar?
            int thumbCellWidth = thumbView->getThumbCellSize().width();
            int maxThumbsBeforeScrollReqd = thumbView->viewport()->width() / thumbCellWidth;
            int thumbsCount = dm->sf->rowCount();
            int thumbDockWidth = thumbView->viewport()->width();
            bool isScrollBar = thumbsCount > maxThumbsBeforeScrollReqd;

            if (isScrollBar) {
                maxHt += scrollBarHeight;// + 1;
                minHt += scrollBarHeight;// + 1;
                newThumbDockHeight += scrollBarHeight;// + 1;
            }

            thumbView->setMaximumHeight(maxHt);
            thumbView->setMinimumHeight(minHt);

//            qDebug() << "maxHt, minHt, newThumbDockHeight" << maxHt << minHt << newThumbDockHeight;

            resizeDocks({thumbDock}, {newThumbDockHeight}, Qt::Vertical);

//            qDebug() << "\nMW::setThumbDockFeatures dock area =" << area << "\n"
//                     << "***  thumbView Ht =" << thumbView->height()
//                     << "thumbSpace Ht =" << thumbView->getThumbCellSize().height()
//                     << "thumbHeight =" << thumbView->thumbHeight
//                     << "newThumbDockHeight" << newThumbDockHeight
//                     << "scrollBarHeight =" << scrollBarHeight << isScrollBar;
        }
    }
    else {
        thumbDock->setFeatures(QDockWidget::DockWidgetClosable |
                               QDockWidget::DockWidgetMovable  |
                               QDockWidget::DockWidgetFloatable);
    }

    if (area == Qt::LeftDockWidgetArea || area == Qt::RightDockWidgetArea)
        thumbView->setWrapping(true);
    if (area == Qt::BottomDockWidgetArea || area == Qt::TopDockWidgetArea) {
        thumbView->setWrapping(thumbView->isThumbWrapWhenTopOrBottomDock);
        thumbView->isTopOrBottomDock = true;
    }
    else thumbView->isTopOrBottomDock = false;

    /*
    thumbView->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
    thumbView->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    thumbView->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    thumbView->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
    */

    if (asGridAction->isChecked()) thumbView->setWrapping(true);
    if (thumbView->isAutoFit) thumbView->thumbsFit(area);
}

void MW::loupeDisplay()
{
    {
    #ifdef ISDEBUG
    qDebug() << "MW::loupeDisplay";
    #endif
    }
    centralLayout->setCurrentIndex(LoupeTab);
    thumbView->thumbViewDelegate->isCompare = false;
//    thumbView->setSelectionMode(QAbstractItemView::ExtendedSelection);

    // in case was grid display move thumbView back to dock from central widget
//    saveSelection();
    thumbDockVisibleAction->setChecked(isThumbDockVisibleBeforeGridViewInvoked);
    thumbView->isGrid = false;
    thumbDock->setWidget(thumbView);
    thumbView->setThumbParameters();
//    setThumbDockFeatures(dockWidgetArea(thumbDock));
//    qDebug() << "\nMW::loupeDisplay before calling setThumbParameters" << "\n"
//             << "***  thumbView Ht =" << thumbView->height()
//             << "thumbSpace Ht =" << thumbView->getThumbCellSize().height()
//             << "thumbHeight =" << thumbView->thumbHeight << "\n";

//    if (!isUpdatingState) thumbView->thumbsFit(dockWidgetArea(thumbDock));
    setThumbDockVisibity();
//    recoverSelection();
    if (thumbDockVisibleAction->isChecked() || thumbView->isGrid)
        thumbView->setFocus();
}

void MW::gridDisplay()
{
    {
    #ifdef ISDEBUG
    qDebug() << "MW::gridDisplay";
    #endif
    }
    qDebug() << "MW::gridDisplay";
    // move thumbView from thumbDeck to central widget
    isThumbDockVisibleBeforeGridViewInvoked = thumbDockVisibleAction->isChecked();
    centralLayout->addWidget(thumbView); // rghx
    centralLayout->setCurrentIndex(GridTab);
    imageView->setVisible(false);
    thumbView->thumbViewDelegate->isCompare = false;
//    thumbView->setSelectionMode(QAbstractItemView::ExtendedSelection);

//    saveSelection();
    thumbDockVisibleAction->setChecked(false); // rghx
//    qDebug() << "\nMW::gridDisplay before calling setThumbParameters" << "\n"
//             << "***  thumbView Ht =" << thumbView->height()
//             << "thumbSpace Ht =" << thumbView->getThumbCellSize().height()
//             << "thumbHeight =" << thumbView->thumbHeight << "\n";
    thumbView->setWrapping(true);
    thumbView->isGrid = true;
    thumbView->setThumbParameters();
//    thumbDock->setFeatures(QDockWidget::DockWidgetClosable |
//                           QDockWidget::DockWidgetMovable  |
//                           QDockWidget::DockWidgetFloatable);
//    thumbView->setMaximumHeight(100000);
    setThumbDockVisibity();
//    recoverSelection();
    if (thumbDockVisibleAction->isChecked() || thumbView->isGrid)
        thumbView->setFocus();
}

void MW::tableDisplay()
{
    {
    #ifdef ISDEBUG
    qDebug() << "MW::tableDisplay";
    #endif
    }
    // show tableView in central widget
    centralLayout->setCurrentIndex(TableTab);
    thumbView->thumbViewDelegate->isCompare = false;

    /* in case was grid display move thumbView back to dock from central widget.
    Moving the widget clears its settings, so must call setThumbParameters.
    After the thumb parameters are reset setThumbParameters emits a signal to
    execute MW::setThumbDockFeatures, where the thumbDock, if top or bottom, is
    resized to fit the thumbs.
    */
    thumbDock->setWidget(thumbView);
    thumbDockVisibleAction->setChecked(isThumbDockVisibleBeforeGridViewInvoked);
    thumbView->isGrid = false;
/*    qDebug() << "\nMW::tableDisplay before calling setThumbParameters" << "\n"
             << "***  thumbView Ht =" << thumbView->height()
             << "thumbSpace Ht =" << thumbView->getThumbCellSize().height()
             << "thumbHeight =" << thumbView->thumbHeight << "\n";
             */
    thumbView->setThumbParameters();
    setThumbDockVisibity();
    if (thumbDockVisibleAction->isChecked() || thumbView->isGrid)
        thumbView->setFocus();
}

void MW::compareDisplay()
{
    {
    #ifdef ISDEBUG
    qDebug() << "MW::compareDisplay";
    #endif
    }
    int n = thumbView->selectionModel()->selectedRows().count();
    if (n < 2) {
        popUp->showPopup(this, "Select more than one image to compare.", 1000, 0.75);
        return;
    }
    if (n > 9) {
        QString msg = QString::number(n);
        msg += " images have been selected.  Only the first 9 will be compared.";
        popUp->showPopup(this, msg, 2000, 0.75);
    }

    if (thumbView->isGrid) {
        saveSelection();
        thumbDock->setWidget(thumbView);
        setThumbDockFeatures(dockWidgetArea(thumbDock));
        thumbDockVisibleAction->setChecked(true);
        thumbView->isGrid = false;
//        qDebug() << "\nMW::compareDisplay before calling setThumbParameters" << "\n"
//                 << "***  thumbView Ht =" << thumbView->height()
//                 << "thumbSpace Ht =" << thumbView->getThumbCellSize().height()
//                 << "thumbHeight =" << thumbView->thumbHeight << "\n";
        thumbView->setThumbParameters();
        setThumbDockVisibity();
        recoverSelection();
    }
    thumbView->thumbViewDelegate->isCompare = true;
    thumbView->setSelectionMode(QAbstractItemView::NoSelection);

    centralLayout->setCurrentIndex(CompareTab);
    compareImages->load(centralWidget->size());

    if (thumbDockVisibleAction->isChecked() || thumbView->isGrid)
        thumbView->setFocus();
}

void MW::saveSelection()
{
    {
    #ifdef ISDEBUG
    qDebug() << "MW::saveSelection";
    #endif
    }
    selectedImages = thumbView->selectionModel()->selectedIndexes();
    currentIdx = thumbView->selectionModel()->currentIndex();
}

void MW::recoverSelection()
{
    {
    #ifdef ISDEBUG
    qDebug() << "MW::recoverSelection";
    #endif
    }
    QItemSelection *selection = new QItemSelection();
    thumbView->selectionModel()->clear();

    QModelIndex idx;
//    thumbView->setCurrentIndex(currentIdx);
    // sync thumbView delegate current item
    thumbView->thumbViewDelegate->currentIndex = currentIdx;

    thumbView->selectionModel()->setCurrentIndex(currentIdx, QItemSelectionModel::Select);
    foreach (idx, selectedImages)
      if (idx != currentIdx) selection->select(idx, idx);

    thumbView->selectionModel()->select(*selection, QItemSelectionModel::Select);
    qDebug() << "thumbView->selectionModel()->currentIndex()" << thumbView->selectionModel()->currentIndex();
    qDebug() << "thumbView->selectionModel()->selectedIndexes()" << thumbView->selectionModel()->selectedIndexes();

}

void MW::setFullNormal()
{
    {
    #ifdef ISDEBUG
    qDebug() << "MW::setFullNormal";
    #endif
    }
    if (fullScreenAction->isChecked()) showFullScreen();
    else showNormal();
}

void MW::setCentralView()
{
    {
    #ifdef ISDEBUG
    qDebug() << "MW::setCentralView";
    #endif
    }
    if (asLoupeAction->isChecked()) loupeDisplay();
    if (asGridAction->isChecked()) gridDisplay();
    if (asTableAction->isChecked()) tableDisplay();
    if (asCompareAction->isChecked()) compareDisplay();
}

void MW::setShootingInfo() {
    {
    #ifdef ISDEBUG
    qDebug() << "MW::setShootingInfo";
    #endif
    }
    imageView->infoDropShadow->setVisible(infoVisibleAction->isChecked());
}

void MW::setFolderDockVisibility() {
    {
    #ifdef ISDEBUG
    qDebug() << "MW::setFolderDockVisibility";
    #endif
    }
    folderDock->setVisible(folderDockVisibleAction->isChecked());
}

void MW::setFavDockVisibility() {
    {
    #ifdef ISDEBUG
    qDebug() << "MW::setFavDockVisibility";
    #endif
    }
    favDock->setVisible(favDockVisibleAction->isChecked());
}

void MW::setFilterDockVisibility() {
    {
    #ifdef ISDEBUG
    qDebug() << "MW::setFilterDockVisibility";
    #endif
    }
    filterDock->setVisible(filterDockVisibleAction->isChecked());
}

void MW::setMetadataDockVisibility() {
    {
    #ifdef ISDEBUG
    qDebug() << "MW::setMetadataDockVisibility";
    #endif
    }
    metadataDock->setVisible(metadataDockVisibleAction->isChecked());
}

void MW::setThumbDockVisibity()
{
    {
    #ifdef ISDEBUG
    qDebug() << "MW::setThumbDockVisibity";
    #endif
    }
    thumbDock->setVisible(thumbDockVisibleAction->isChecked());
//    setThumbDockLockMode();
}

void MW::setMenuBarVisibility() {

    {
    #ifdef ISDEBUG
    qDebug() << "MW::setMenuBarVisibility";
    #endif
    }
    menuBar()->setVisible(menuBarVisibleAction->isChecked());
}

void MW::setStatusBarVisibility() {

    {
    #ifdef ISDEBUG
    qDebug() << "MW::setStatusBarVisibility";
    #endif
    }
    statusBar()->setVisible(statusBarVisibleAction->isChecked());
//    G::isStatusBarVisible = statusBarVisibleAction->isChecked();
}

void MW::setWindowsTitleBarVisibility() {

    {
    #ifdef ISDEBUG
    qDebug() << "MW::setWindowsTitleBarVisibility";
    #endif
    }
    if(windowTitleBarVisibleAction->isChecked()) {
        hide();
        setWindowFlags(windowFlags() & ~Qt::FramelessWindowHint);
        show();    }
    else {
        hide();
        setWindowFlags(windowFlags() | Qt::FramelessWindowHint);
        show();
    }
}

void MW::setFolderDockLockMode()
{
    {
    #ifdef ISDEBUG
    qDebug() << "MW::setFolderDockLockMode";
    #endif
    }
    if (folderDockLockAction->isChecked()) {
        folderDock->setTitleBarWidget(folderDockEmptyWidget);
    }
    else {
        folderDock->setTitleBarWidget(folderDockOrigWidget);
    }
}

void MW::setFavDockLockMode()
{
    {
    #ifdef ISDEBUG
    qDebug() << "MW::setFavDockLockMode";
    #endif
    }
    if (favDockLockAction->isChecked()) {
        favDock->setTitleBarWidget(favDockEmptyWidget);
    }
    else {
        favDock->setTitleBarWidget(favDockOrigWidget);
    }
}

void MW::setFilterDockLockMode()
{
    {
    #ifdef ISDEBUG
    qDebug() << "MW::setfilterDockLockMode";
    #endif
    }
    if (filterDockLockAction->isChecked()) {
        filterDock->setTitleBarWidget(filterDockEmptyWidget);
    }
    else {
        filterDock->setTitleBarWidget(filterDockOrigWidget);
    }
}

void MW::setMetadataDockLockMode()
{
    {
    #ifdef ISDEBUG
    qDebug() << "MW::setMetadataDockLockMode";
    #endif
    }
    if (metadataDockLockAction->isChecked()) {
        metadataDock->setTitleBarWidget(metadataDockEmptyWidget);
    }
    else {
        metadataDock->setTitleBarWidget(metadataDockOrigWidget);
    }
}

void MW::setThumbDockLockMode()
{
    {
    #ifdef ISDEBUG
    qDebug() << "MW::setThumbDockLockMode";
    #endif
    }
    if (thumbDockLockAction->isChecked()) {
        thumbDock->setTitleBarWidget(thumbDockEmptyWidget);
    }
    else {
        thumbDock->setTitleBarWidget(thumbDockOrigWidget);
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
        filterDock->setTitleBarWidget(filterDockEmptyWidget);
        metadataDock->setTitleBarWidget(metadataDockEmptyWidget);
        thumbDock->setTitleBarWidget(thumbDockEmptyWidget);
        folderDockLockAction->setChecked(true);
        favDockLockAction->setChecked(true);
        filterDockLockAction->setChecked(true);
        metadataDockLockAction->setChecked(true);
        thumbDockLockAction->setChecked(true);
//        G::isLockAllDocks = true;
    } else {
        folderDock->setTitleBarWidget(folderDockOrigWidget);
        favDock->setTitleBarWidget(favDockOrigWidget);
        filterDock->setTitleBarWidget(filterDockOrigWidget);
        metadataDock->setTitleBarWidget(metadataDockOrigWidget);
        thumbDock->setTitleBarWidget(thumbDockOrigWidget);
        folderDockLockAction->setChecked(false);
        favDockLockAction->setChecked(false);
        filterDockLockAction->setChecked(false);
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

    QModelIndex idx;
    QModelIndexList idxList = thumbView->selectionModel()->selectedRows();
    QString pickStatus;

    foreach (idx, idxList) {
        int srcRow = dm->sf->mapToSource(idx).row();
        QModelIndex srcIdx = dm->index(srcRow, G::PickedColumn);
        pickStatus = (qvariant_cast<QString>(srcIdx.data(Qt::EditRole))
                      == "true") ? "false" : "true";
        dm->setData(srcIdx, pickStatus, Qt::EditRole);
    }
    idx = thumbView->currentIndex();
    bool isPick = (pickStatus == "true");

    imageView->pickLabel->setVisible(isPick);
    if (asCompareAction->isChecked()) compareImages->pick(isPick, idx);
    thumbView->refreshThumbs();
}

void MW::updatePick()
{
/*
When a new image is selected and shown in imageView update the visibility
of the thumbs up icon that highlights if the image has been picked.
*/
    {
    #ifdef ISDEBUG
    qDebug() << "MW::updatePick";
    #endif
    }
    int row = thumbView->currentIndex().row();
    QModelIndex idx = dm->index(row, G::PickedColumn);
    bool isPick = qvariant_cast<bool>(idx.data(Qt::EditRole));
    if (asLoupeAction->isChecked())
        (isPick) ? imageView->pickLabel->setVisible(true)
                 : imageView->pickLabel->setVisible(false);
    if (asCompareAction->isChecked()) compareImages->pick(isPick, idx);
}

void MW::copyPicks()
{
    {
    #ifdef ISDEBUG
    qDebug() << "MW::copyPicks";
    #endif
    }
    if (thumbView->isPick()) {
        QFileInfoList imageList = thumbView->getPicks();
        copyPickDlg = new CopyPickDlg(this, imageList, metadata);
        copyPickDlg->exec();
        delete copyPickDlg;
    }
    else QMessageBox::information(this,
         "Oops", "There are no picks to ingest.    ", QMessageBox::Ok);
}

void MW::setRating()
{
/*
Resolve the source menu action that called (could be any rating) and then set
the rating for all the selected thumbs.
*/
    {
    #ifdef ISDEBUG
    qDebug() << "MW::setRating";
    #endif
    }
    QObject* obj = sender();
    QString s = obj->objectName();
    if (s == "Rate0") rating = "";
    if (s == "Rate1") rating = "1";
    if (s == "Rate2") rating = "2";
    if (s == "Rate3") rating = "3";
    if (s == "Rate4") rating = "4";
    if (s == "Rate5") rating = "5";

    QModelIndexList selection = thumbView->selectionModel()->selectedRows();
    // check if selection is entirely rating already - if so set to no rating
    bool isAlreadyRating = true;
    for (int i = 0; i < selection.count(); ++i) {
        QModelIndex idx = dm->index(selection.at(i).row(), G::RatingColumn);
        if(idx.data(Qt::EditRole) != rating) {
            isAlreadyRating = false;
        }
    }
    if(isAlreadyRating) rating = "";     // invert the label

    // set the image edits label
    imageView->editsLabel->setText(rating);
    if (labelColor == "" && rating == "") imageView->editsLabel->setVisible(false);
    else imageView->editsLabel->setVisible(true);

    // set the rating
    for (int i = 0; i < selection.count(); ++i) {
        QModelIndex idx = dm->index(selection.at(i).row(), G::RatingColumn);
        dm->setData(idx, rating, Qt::EditRole);
    }
    thumbView->refreshThumbs();
}

void MW::updateRating()
{
/*
When a new image is selected and shown in imageView update the rating on
imageView and visibility (true if either rating or color class set).
*/
    {
    #ifdef ISDEBUG
    qDebug() << "MW::updateRating";
    #endif
    }
    int row = thumbView->currentIndex().row();
    QModelIndex idx = dm->index(row, G::RatingColumn);
    rating = idx.data(Qt::EditRole).toString();
    imageView->editsLabel->setText(rating);
    if (labelColor == "" && rating == "") imageView->editsLabel->setVisible(false);
    else imageView->editsLabel->setVisible(true);
}

void MW::setColorClass()
{
/*
Resolve the source menu action that called (could be any color class) and then
set the color class for all the selected thumbs.
*/
    {
    #ifdef ISDEBUG
    qDebug() << "MW::setLabelColor";
    #endif
    }
    QObject* obj = sender();
    QString s = obj->objectName();
    if (s == "Label0") labelColor = "";
    if (s == "Label1") labelColor = "Red";
    if (s == "Label2") labelColor = "Yellow";
    if (s == "Label3") labelColor = "Green";
    if (s == "Label4") labelColor = "Blue";
    if (s == "Label5") labelColor = "Purple";

    QModelIndexList selection = thumbView->selectionModel()->selectedRows();
    // check if selection is entirely label color already - if so set no label
    bool isAlreadyLabel = true;
    for (int i = 0; i < selection.count(); ++i) {
        QModelIndex idx = dm->index(selection.at(i).row(), G::LabelColumn);
        if(idx.data(Qt::EditRole) != labelColor) {
            isAlreadyLabel = false;
        }
    }
    if(isAlreadyLabel) labelColor = "";     // invert the label

    // set the image edits label
    if (labelColor == "") imageView->editsLabel->setBackgroundColor(G::labelNoneColor);
    if (labelColor == "Red") imageView->editsLabel->setBackgroundColor(G::labelRedColor);
    if (labelColor == "Yellow") imageView->editsLabel->setBackgroundColor(G::labelYellowColor);
    if (labelColor == "Green") imageView->editsLabel->setBackgroundColor(G::labelGreenColor);
    if (labelColor == "Blue") imageView->editsLabel->setBackgroundColor(G::labelBlueColor);
    if (labelColor == "Purple") imageView->editsLabel->setBackgroundColor(G::labelPurpleColor);

    if (labelColor == "" && rating == "") imageView->editsLabel->setVisible(false);
    else imageView->editsLabel->setVisible(true);

    // update the data model
    for (int i = 0; i < selection.count(); ++i) {
        QModelIndex idx = dm->index(selection.at(i).row(), G::LabelColumn);
        dm->setData(idx, labelColor, Qt::EditRole);
    }
    thumbView->refreshThumbs();
    tableView->resizeColumnToContents(G::LabelColumn);
}

void MW::updateColorClass()
{
/*
When a new image is selected and shown in imageView update the color class on
imageView and visibility (true if either rating or color class set). Note that
label and color class are the same thing in this program. In lightroom the
color class is called label.
*/
    {
    #ifdef ISDEBUG
    qDebug() << "MW::updateColorClass";
    #endif
    }
    int row = thumbView->currentIndex().row();
    QModelIndex idx = dm->index(row, G::LabelColumn);
    labelColor = idx.data(Qt::EditRole).toString();
    if (labelColor == "") imageView->editsLabel->setBackgroundColor(G::labelNoneColor);
    if (labelColor == "Red") imageView->editsLabel->setBackgroundColor(G::labelRedColor);
    if (labelColor == "Yellow") imageView->editsLabel->setBackgroundColor(G::labelYellowColor);
    if (labelColor == "Green") imageView->editsLabel->setBackgroundColor(G::labelGreenColor);
    if (labelColor == "Blue") imageView->editsLabel->setBackgroundColor(G::labelBlueColor);
    if (labelColor == "Purple") imageView->editsLabel->setBackgroundColor(G::labelPurpleColor);
    if (labelColor == "" && rating == "") imageView->editsLabel->setVisible(false);
    else imageView->editsLabel->setVisible(true);

}

void MW::getSubfolders(QString fPath)
{
    {
    #ifdef ISDEBUG
    qDebug() << "MW::getSubfolders";
    #endif
    }
    subfolders = new QStringList;
    subfolders->append(fPath);
    qDebug() << "Appending" << fPath;
    QDirIterator iterator(fPath, QDirIterator::Subdirectories);
    while (iterator.hasNext()) {
        iterator.next();
        fPath = iterator.filePath();
        if (iterator.fileInfo().isDir() && iterator.fileName() != "." && iterator.fileName() != "..") {
            subfolders->append(fPath);
            qDebug() << "Appending" << fPath;
        }
    }
}

void MW::stressTest()
{
    {
    #ifdef ISDEBUG
    qDebug() << "MW::stressTest";
    #endif
    }
    getSubfolders("/users/roryhill/pictures");
    QString fPath;
    fPath = subfolders->at(qrand() % (subfolders->count()));
}

void MW::setSlideShowParameters(int delay, bool isRandom)
{
    {
    #ifdef ISDEBUG
    qDebug() << "MW::setSlideShowParameters";
    #endif
    }
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
    if (isSlideShowActive) {
        isSlideShowActive = false;
        slideShowAction->setText(tr("Slide Show"));
        popUp->showPopup(this, "Stopping slideshow", 1000, 0.5);
        slideShowTimer->stop();
        delete slideShowTimer;
//        delete popUp;
    } else {
        isSlideShowActive = true;
        QString msg = "Starting slideshow";
        msg += "\nInterval = " + QString::number(slideShowDelay) + " second(s)";
        if (slideShowRandom)  msg += "\nRandom selection";
        else msg += "\nLinear selection";
        if (slideShowWrap) msg += "\nWrap at end of slides";
        else msg += "\nStop at end of slides";
        popUp->showPopup(this, msg, 4000, 0.5);

        if (isStressTest) getSubfolders("/users/roryhill/pictures");

        slideShowAction->setText(tr("Stop Slide Show"));
        slideShowTimer = new QTimer(this);
        connect(slideShowTimer, SIGNAL(timeout()), this, SLOT(nextSlide()));
//        if (isStressTest) slideShowDelay = 0.9;
        slideShowTimer->start(slideShowDelay * 1000);
//        nextSlide();
    }
}

void MW::nextSlide()
{
    {
    #ifdef ISDEBUG
    qDebug() << "MW::nextSlide";
    #endif
    }
    static int counter = 0;
    int totSlides = dm->sf->rowCount();
    if (slideShowRandom) thumbView->selectRandom();
    else {
        if (thumbView->getCurrentRow() == totSlides - 1)
            thumbView->selectFirst();
        else thumbView->selectNext();
    }
    counter++;
    updateStatus(true, "Slide # "+ QString::number(counter));

    if (isStressTest) {
        if (counter % 10 == 0) {
            int n = qrand() % (subfolders->count());
            QString fPath = subfolders->at(n);
            fsTree->setCurrentIndex(fsTree->fsModel->index(fPath));
//            qDebug() << "STRESS TEST NEW FOLDER:" << fPath;
            folderSelectionChange();
        }
    }
}

void MW::dropOp(Qt::KeyboardModifiers keyMods, bool dirOp, QString cpMvDirPath)
{
    {
    #ifdef ISDEBUG
    qDebug() << "MW::dropOp";
    #endif
    }
    QApplication::restoreOverrideCursor();
    copyOp = (keyMods == Qt::ControlModifier);
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

    if (destDir == 	currentViewDir) {
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

    QModelIndex idx = fsTree->fsModel->index(currentViewDir);
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
//        if (event->modifiers() == Qt::ControlModifier)
//        {
//            if (event->delta() < 0)
//                imageView->zoomOut();
//            else
//                imageView->zoomIn();
//        }
//        else if (nextThumbAction->isEnabled())
//        {
//            if (event->delta() < 0)
//                thumbView->selectNext();
//            else
//                thumbView->selectPrev();
//        }
    QMainWindow::wheelEvent(event);
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

    bookmarks->bookmarkPaths.insert(path);
    bookmarks->reloadBookmarks();
}

void MW::openFolder()
{
    {
    #ifdef ISDEBUG
    qDebug() << "MW::openOp";
    #endif
    }
    QString dir = QFileDialog::getExistingDirectory(this, tr("Open Folder"),
         "/home", QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);
    fsTree->setCurrentIndex(fsTree->fsModel->index(dir));
    folderSelectionChange();
}

void MW::revealFile()
{
    {
    #ifdef ISDEBUG
    qDebug() << "MW::revealFile";
    #endif
    }

    // See http://stackoverflow.com/questions/3490336/how-to-reveal-in-finder-or-show-in-explorer-with-qt
    // for details

    QString pathToReveal = thumbView->getCurrentFilename();

    // Mac, Windows support folder or file.
#if defined(Q_OS_WIN)
//    const QString explorer = Environment::systemEnvironment().searchInPath(QLatin1String("explorer.exe"));
//    if (explorer.isEmpty()) {
//        QMessageBox::warning(this,
//                             tr("Launching Windows Explorer failed"),
//                             tr("Could not find explorer.exe in path to launch Windows Explorer."));
//        return;
//    }
//    QString param;
//    if (!QFileInfo(pathIn).isDir())
//        param = QLatin1String("/select,");
//    param += QDir::toNativeSeparators(pathIn);
//    QString command = explorer + " " + param;
//    QString command = explorer + " " + param;
//    QProcess::startDetached(command);

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

void MW::help()
{
    QWidget *helpDoc = new QWidget;
    Ui::helpForm ui;
    ui.setupUi(helpDoc);
    helpDoc->show();
}

void MW::test()
{
    recoverSelection();
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

