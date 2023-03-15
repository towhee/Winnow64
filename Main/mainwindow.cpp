#include "Main/mainwindow.h"

/* Program notes
***********************************************************************************************

INITIALIZATION

Persistant settings are saved between sessions using QSettings. Persistant settings form two
categories at runtime: action settings and preference settings. Action settings are boolean
and maintained by the action item setChecked value. Preferences can be anything and are
maintained as public variables, mostly in MW (this class) and managed in the prefDlg class.

Not all settings can be read immediately as the classes that use them may not yet have been
created. Examples of this are IconView parameters or the recent folders menu list, since
IconView and the menus have not been created yet. Therefore some settings are assigned up
front and the rest are assigned as needed.

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
    • Ingest history folders
    • Workspaces
• Create actions and set checked based on persistant values from QSetting
• Create bookmarks with persistant values from QSettings
• Update external apps with persistant values from QSettings
• Load shortcuts (based on being able to edit shortcuts)
• Execute updateState function to implement all persistant state settings

• Select a folder
  • Load datamodel with QDir info on each image file
  • Add the rest of the metadata to datamodel (incl thumbs as icons)
  • Update the image cache

***********************************************************************************************

PROGRAM FLOW - LINEAR - NEW FOLDER SELECTED

    • A new folder is selected which triggers MW::selectionChange:

    • Housekeeping:
      - the DataModel instance is incremented.
      - if DataModel is loading it is aborted.
      - all threads that may be running are stopped.
      - set flags, progress and views to start condition.
      - folders and bookmarks synced.
      - filters are cleared (uncheckAllFilters).

    • All eligible image files (and associated QFileInfo: path, name, type, size, created
      date and last modified date) in the folder are added to the DataModel (dm).

    • // Currently resetting sort to file name in forward order
      Sort to current sort item and forward/reverse, as the datamodel always loads in the
      order from QFileInfo (file name in forward order).

    • Current row indexes set to -1 (invalid).

    • The first image thumbnail is selected in thumbView.

Following is out of date:

    • metadataCacheThread->loadNewFolder reads all the metadata for the first chunk of
      images. The signal loadMetadataCache2ndPass is emitted.

    • Based on the data collected in the first metadataCacheThread pass the icon
      dimension best fit is determined, which is required to calculate the number of icons
      visible in the thumbView and gridview.

    • metadataCacheThread->loadNewFolder2ndPass reads the requisite number of icons and
      emits loadImageCache.

    • The imageCacheThread is initialized. fileSelectionChange is called for the current
      image (the user may have already advanced).

    • fileSelectionChange synchronizes the views and starts three processes:
      1. ImageView::loadImage loads the full size image into the graphicsview scene.
      2. The metadataCacheThread is started to read the rest of the metadata and icons.
         If the number of images in the folder(s) is greater than the threshold only a
         chunck of metadata is collected.
      3. The imageCacheThread is started to cache as many full size images as assigned
         memory permits.

    • The first image is loaded. The metadata and thumbnail generation threads will still
      be running, but things should appear to be speedy for the user.

    • The metadata caching thread collects information required by the image cache
      thread. If the number of images in the folder(s) is greater than the threshold only
      a chunck of metadata is collected.

    • The image caching thread requires the offset and length for the full size embedded
      jpg, the image width and height in order to calculate memory requirements, update
      the image priority queues, the target range and limit the cache to the assigned
      maximum size.

PROGRAM FLOW - CONCURRENT - NEW FOLDER SELECTED

PROGRAM FLOW - FOLDER AND FILE SELECTED
PROGRAM FLOW - NEW IMAGE SELECTED

    A new image is selected which triggers fileSelectionChange:

    • If starting the program select the first image in the folder.

    • Record the current datamodel row and its file path.

    • Update the thumbView and gridView delegates.

    • Synchronize the thumb, grid and table views.   If in loupe mode, load the current
      image.

    • Update window title, statusbar, info panel and classification badges.

    • Update the metadata and image caching.

    • If the metadata has not been cached yet for the selected image (usually the first
      in a new folder) then load the thumbnail.

    • Update the cursor position on the image caching progress bar.

A new image is selected which triggers a scroll event

    • IconView selection change may trigger scroll signal, based on the current view:
      thumb, grid or table.  MW::thumb/grid/tableHasScrolled are signalled.

    • Update the icon range (firstVisible/lastVisible)

Idea: if change file and not all metadata read then restart readMeta if new file meta not
read, else just keep current readMeta going.  Have a separate thread to update icons when
scroll.

Flow by function call:

    MW::folderSelectionChange
    MW::stopAndClearAll
    DataModel::clearDataModel
    DataModel::load
    DataModel::addFileData
    MW::loadLinearNewFolder
    DataModel::addAllMetadata
    MW::updateIconBestFit
    MW::updateIconsVisible
    IconView::calcViewportRange
    MetadataCache::readIconChunk
    MW::loadImageCacheForNewFolder
    ImageCache::initImageCache
    ImageCache::buildImageCacheList
    IconView::selectionChanged              Call MW::fileSelectionChange ~ flags
    MW::fileSelectionChange
    ImageView::loadImage                    If image is cached
    MetadataCache::fileSelectionChange
    ImageCache::setCurrentPosition
    MetadataCache::readIconChunk
    MW::updateCachedStatus                  Calls ImageView::LoadImage if current just cached
    ImageView::loadImage


Flow Flags:

    G::isInitializing
    G::stop
    G::allMetadataLoaded
    G::allIconsLoaded
    G::isNewFolderLoaded
    dm->loadingModel
    dm->basicFileInfoLoaded  // not used
    G::isLinearLoading
    G::ignoreScrollSignal
    isCurrentFolderOkay
    isFilterChange
    G::isNewSelection

Current model row:

    currentRow
    dm->currentRow
    dm->currentFilePath
    currentDmIdx
    currentSfIdx



***********************************************************************************************

CACHING

• When a new folder or a new file is selected, or when a scroll event occurs in IconView
  then the metadata cache (metadataCacheThread) is started.

• If it is a new folder metadataCacheThread emits a signal back to
  MW::loadImageCacheForNewFolder, where some housekeeping occurs and the ImageCache is
  initialized and started in imageCacheThread.  The full size images are cached.

• If a new image is selected then MW::fileSelectionChange restarts metadataCacheThread to
  cache any additional metadata or icons, and then restarts the imageCacheThread to cache
  any additional full size images.

• If the user scrolls in the thumbView or gridView scroll events are sent to
  MW::thumbHasScrolled.  The delay insures that is the user is rapidly scrolling
  then the next scroll event supercedes the prior and the metadataCacheThread is only
  restarted when there is a pause or the scrolling is slow.  In this scenario the
  imageCacheThread is not restarted because a new image has not been selected.

* see top of mdcache.cpp comments for more detail

CACHE STATUS

* Two status lights on the right side of the status bar (metadataThreadRunningLabel and
  imageThreadRunningLabel) turn from green to red to indicate datamodel metadata and image
  caching respectively.  A status light on the lower right of each thumbnail is made
  visible when the associated image is cached.

* An image cache progress bar (progressBar) shows the status of every image in the folder:
  target range, isCached and cursor position.

* The flag isShowCacheProgressBar toggles cache progress updates and the visibility of the
  progress bar.

* isCached is stored in datamodel as dm->sf->setData(idx, isCached, G::CachedRole)

* Functions:
  MW::createStatusBar                       - add status labels to right side of status bar
  MW::setImageCacheParameters               - preferences calls to update cache status
  MW::setThreadRunStatusInactive            - sets caching activity status lights gray
  MW::updateImageCachingThreadRunStatus     - sets image caching activity status light green/red
  MW::updateMetadataCachingThreadRunStatus  - sets metadata caching activity status light green/red
  MW::setCacheStatusVisibility              - updateState calls to toggle progressBar visibility
  MW::updateImageCacheStatus                - ImageCache calls to update image cache status
  MW::setCachedStatus                       - sets isCached in datamodel and refreshes views

ICON CACHING

• Each icon is a 256px longside QIcon so we want to balance how many we load when weighing
  performance vs memory footprint.

• Icons are stored in the DataModel at dm->(row, 0).data(Qt::DecorationRole).  The loaded
  status can be determined two ways:

  1. dm->itemFromIndex(dmIdx)->icon().isNull()      // used up to 2023
  2. dmIdx.data(Qt::DecorationRole).isNull()        // switch to in 2023

• Cases:

  1.

• The default limit of icons to load is dm->defaultIconChunkSize, which is set in
  preferences.  dm->iconChunkSize (the variable used to manage the actual number of
  icons loaded) is initially set to dm->defaultIconChunkSize.

• If the number of images in the folder(s) is less than dm->defaultIconChunkSize, then
  all rows in the DataModel are loaded with an icon.

• When there are more DataModel rows than the dm->defaultIconChunkSize then only the
  default amount of icons are loaded.  As the user moves through the DataSet icons are
  removed and added.

• When there are more thumbnails (icons) visible in either thumbView, gridView or
  tableView than dm->defaultIconChunkSize, then dm->iconChunkSize is adjusted to the
  largest number in MW::updateIconRange.

• CASE X: When the app is resized or the thumbnails are resized the number visible may change
  and dm->iconChunkSize is adjusted in MW::updateIconRange.

• When dm->iconChunkSize changes or there is a scroll event then the metadata cache
  read process must be rerun to update the icons.
•
•
•


***********************************************************************************************

DATAMODEL CHANGES

Folder change
    MW::folderSelectionChange
        Housekeeping                    (stop cache threads, slideshow, initializing)
    DataModel::load                     (load basic metadata for every eligible image)
    MetadataCache::loadNewFolder
    MetadataCache::setRange
    MetadataCache::readMetadataIconChunk
    MW::loadMetadataCache2ndPass
    MW::updateIconBestFit
    IconView::bestAspect
    IconView::setThumbParameters
    MetadataCache::loadNewFolder2ndPass
    MetadataCache::setRange
    MetadataCache::readMetadataIconChunk
    MW::loadImageCacheForNewFolder
    // need to repeat best fit?
    MW::updateIconBestFit
    IconView::bestAspect
    IconView::setThumbParameters
    ImageCache::initImageCache
    ImageCache::buildImageCacheList
    ImageCache::updateImageCachePosition
    ImageCache::updateImageCacheList
    ImageCache::setPriorities
    ImageCache::setTargetRange
    ImageCache::run

Image change
    MW::fileSelectionChange
    MW::updateMetadataCacheIconviewState
    MetadataCache::fileSelectionChange
    MetadataCache::setRange
    MetadataCache::readMetadataIconChunk
    ImageCache::updateImageCachePosition
    ImageCache::updateImageCacheList
    ImageCache::setPriorities
    ImageCache::setTargetRange
    ImageCache::run

Filter change
    MW::filterChange
    MW::loadEntireMetadataCache
    DataModel::addAllMetadata               (instead of MetadataCache::loadAllMetadata)
    SortFilter::filterChange
    DataModel::filteredItemCount
    ImageCache::rebuildImageCacheParameters
    thumbView->selectThumb
    fileSelectionChange                     (same as image change above)

Sort change
    MW::sortThumbnails
    thumbView->sortThumbs
    ImageCache::rebuildImageCacheParameters
    MW::scrollToCurrentRow

***********************************************************************************************

SCROLLING

When the user scrolls a datamodel view (thumbView, gridView or tableView) the scrollbar
change signal is received by thumbHasScrolled, gridHasScrolled or tableHasScrolled. The
other views are scrolled to sync with the source view midVisibleRow. The first, mid and
last visible items are determined in MW::updateIconRange and the metadataCacheThread or
metaReadThread is called to update the metadata and icons in the range (chunk).

However, when the program syncs the views this generates more scrollbar signals that
would loop. This is prevented by the G::ignoreScrollSignal flag. This is also employed in
fileSelectionChange, where visible views are scrolled to center on the current selection.

Painting delays:

    When the user changes modes in MW (ie from Grid to Loupe) a IconView instance (either
    thumbView or gridView) can change state from hidden to visible. Since hidden widgets
    cannot be accessed we need to wait until the IconView becomes visible and fully
    repainted before attempting to scroll to the current index.

    Also, when an IconView resize event occurs: triggered by a change in the gridView
    cell size (thumbWidth) that requires justification; by a change in the thumbDock
    splitter; or a resize of the application window; we also need to wait until
    repainting the scrollbars is completed.

    The last paint event is identified by calculating the maximum of the scrollbar range
    and comparing it to the paint event, which updates the range each time. With larger
    datasets (ie 1500+ thumbs) it can take a number of paint events and hundreds of ms to
    complete. IconView::waitUntilScrollReady monitors this and returns when done.

Scrollbar change flow:

    • The ScrollBar signal valueChanged triggers MW::thumbHasScrolled, MW::thumbHasScrolled
      or MW::tableHasScrolled.

    • MW::updateIconRange updates the first, mid and last visible icons from all views to
      find the greatest range of visible icons by calling IconView::updateVisible and
      TableView::updateVisible. This is stored in the DataModel class as startIconRange,
      midIconRange amd endIconRange. The dm->iconChunkSize is adjusted if it is less
      than the visible icon range.

    • Concurrent metadata loading:

         MW::loadConcurrent(midVisibleIcon) is called and it loads any missing icons and
         cleans up any orphaned icons (not in the iconChunkRange).


QT VERSIONS

*/

MW::MW(const QString args, QWidget *parent) : QMainWindow(parent)
{
    if (G::isLogger) G::log("MW::MW", "START APPLICATION", true);
    setObjectName("MW");

    // Check if modifier key pressed while program opening
    isShiftOnOpen = false;
    Qt::KeyboardModifiers modifier = QGuiApplication::queryKeyboardModifiers();
    if (modifier & Qt::ShiftModifier) {
        isShiftOnOpen = true;
        G::isEmbellish = false;
    }
    /* modifier & Qt::ControlModifier
    if (modifier & Qt::ControlModifier) {
        G::isLogger = true;
        G::sendLogToConsole = false;  // write to winlog.txt
//        openLog();
        qDebug() << "MW::MW" << "command modifier";
    }
    //*/

    // check args to see if program was started by another process (winnet)
    QString delimiter = "\n";
    QStringList argList = args.split(delimiter);
    if (argList.length() > 1) isStartupArgs = true;
//    if (G::isFileLogger) Utilities::log("MW::MW", QString::number(argList.length()) + " arguments");

    /* TESTING / DEBUGGING FLAGS
       Note G::isLogger is in globals.cpp */
    G::showAllTableColumns = false;     // show all table fields for debugging
    simulateJustInstalled = false;
    isStressTest = false;
    G::isTimer = true;                  // Global timer
    G::isTest = false;                  // test performance timer

    // Initialize some variables
    initialize();

    // platform specific settings
    setupPlatform();

    // persistant settings between sessions
    setting = new QSettings("Winnow", "winnow_100");
    G::settings = setting;
    if (setting->contains("slideShowDelay") && !simulateJustInstalled) isSettings = true;
    else isSettings = false;
    loadSettings();             // except settings with dependencies ie for actions not created yet

    // update executable location - req'd by Winnets (see MW::handleStartupArgs)
    setting->setValue("appPath", qApp->applicationDirPath());

//    // Logger
//    if (G::isLogger && G::sendLogToConsole == false) openLog();

//    // Error Logger
    if (G::isErrorLogger) openErrLog();

    // app stylesheet and QSetting font size and background from last session
    createAppStyle();

    createCentralWidget();      // req'd by ImageView, CompareView
    createFilterView();         // req'd by DataModel (dm)
    createDataModel();          // dependent on FilterView, creates Metadata, Thumb
    createThumbView();          // dependent on QSetting, filterView
    createGridView();           // dependent on QSetting, filterView
    createTableView();          // dependent on centralWidget
    createSelectionModel();     // dependent on ThumbView, ImageView
    createSelection();          // dependent on DataModel, ThumbView, GridView, TableView
    createInfoString();         // dependent on QSetting, DataModel, EmbelProperties
    createInfoView();           // dependent on DataModel, Metadata, ThumbView
    createFrameDecoder();       // dependent on DataModel
    createImageCache();         // dependent on DataModel, Metadata, ThumbView
    createMDCache();            // dependent on DataModel, Metadata, ThumbView, VideoView, ImageCache
    createImageView();          // dependent on centralWidget, ThumbView, ImageCache
    createVideoView();          // dependent on centralWidget, ThumbView
    createCompareView();        // dependent on centralWidget
    createFSTree();             // dependent on Metadata
    createBookmarks();          // dependent on loadSettings, FSTree
    createDocks();              // dependent on FSTree, Bookmarks, ThumbView, Metadata,
                                //              InfoView, EmbelProperties
    createEmbel();              // dependent on EmbelView, EmbelDock
    createStatusBar();
    createMessageView();
    createActions();            // dependent on above
    createMenus();              // dependent on createActions and loadSettings



    loadShortcuts(true);        // dependent on createActions
    setupCentralWidget();

    // recall previous thumbDock state in case last closed in Grid mode
    if (wasThumbDockVisible) thumbDockVisibleAction->setChecked(wasThumbDockVisible);

    // intercept events to thumbView to monitor splitter resize of thumbDock
    qApp->installEventFilter(this);

    // if isShift then set "Do not Embellish".  Note isShift also used in MW::showEvent.
    if (isShiftOnOpen) {
        embelTemplatesActions.at(0)->setChecked(true);
        embelProperties->doNotEmbellish();
    }

    qRegisterMetaType<ImageCacheData::Cache>();
    qRegisterMetaType<ImageMetadata>();
    qRegisterMetaType<QVector<int>>();

    show();

    G::isInitializing = false;

    if (isStartupArgs) {
        handleStartupArgs(args);
    }
    else {
        // First use of app
        if (!isSettings) centralLayout->setCurrentIndex(StartTab);

        // process the persistant folder if available
        if (rememberLastDir && !isShiftOnOpen) {
            if (isFolderValid(lastDir, true, true)) {
                stop("MW::MW rememberLastDir");
                fsTree->select(lastDir);
                folderSelectionChange();
            }
        }

        // show start message
        else {
            QString msg = "Select a folder or bookmark to get started.";
            setCentralMessage(msg);
            prevMode = "Loupe";
        }

        if (setting->value("hasCrashed").toBool()) {
//            QFileInfo info(lastFileIfCrash);
//            QString lastFolder = QFileInfo(lastFileIfCrash).dir().path();
            int picks = pickLogCount();
            int ratings = ratingLogCount();
            int colors = colorClassLogCount();
            QString picksRecoverable = " picks recoverable";
            if (picks == 1) picksRecoverable = " pick recoverable";
            QString ratingsRecoverable = " ratings recoverable";
            if (picks == 1) ratingsRecoverable = " rating recoverable";
            QString colorsRecoverable = " color labels recoverable";
            if (picks == 1) colorsRecoverable = " color label recoverable";
            if (picks || ratings || colors) {
                QString msg = "It appears Winnow did not close properly.  Do you want to "
                              "recover the most recent picks, ratings and color categories?\n";
                msg += "\nFolder: " + QFileInfo(lastFileIfCrash).dir().path();
                msg += "\n";
                if (picks) msg += "\n" + QString::number(picks) + picksRecoverable;
                if (ratings) msg += "\n" + QString::number(ratings) + ratingsRecoverable;
                if (colors) msg += "\n" + QString::number(colors) + colorsRecoverable;
                msg += "\n";
                int ret = QMessageBox::warning(this, "Recover Prior State", msg,
                                               QMessageBox::Yes | QMessageBox::No);
                if (ret == QMessageBox::Yes) {
                    folderAndFileSelectionChange(lastFileIfCrash, "lastFileIfCrash");
                    recoverPickLog();
                    recoverRatingLog();
                    recoverColorClassLog();
                }
            }
        }

        // crash log
        setting->setValue("hasCrashed", true);
    }
}

void MW::restoreLastSessionGeometryState()
{
    if (G::isLogger) G::log("MW::restoreLastSessionGeometryState");
    if (isSettings) {
        restoreGeometry(setting->value("Geometry").toByteArray());
        restoreState(setting->value("WindowState").toByteArray());
    }
    else {
        defaultWorkspace();
    }
    setWindowOpacity(1);
}

bool MW::isDevelopment()
{
    if (G::isLogger) G::log("MW::isDevelopment");
//    qDebug() << "MW::isDevelopment" << QCoreApplication::applicationDirPath();
    if (QCoreApplication::applicationDirPath().contains("Winnow64"))
        return true;
    else return false;
}

//   EVENT HANDLERS

void MW::showEvent(QShowEvent *event)
{
    if (G::isLogger || G::isFlowLogger) G::log("MW::showEvent");

    QMainWindow::showEvent(event);

    if (isSettings) restoreLastSessionGeometryState();
    getDisplayProfile();

    // set thumbnail size to fit the thumbdock initial size
    thumbView->thumbsFitTopOrBottom();

    // create popup window used for messaging
    G::newPopUp(this, centralWidget);

    // set initial visibility
    embelTemplateChange(embelProperties->templateId);
    // size columns after show if device pixel ratio > 1
    embelProperties->resizeColumns();

    // check for updates
    if (checkIfUpdate && !isStartupArgs) QTimer::singleShot(100, this, SLOT(checkForUpdate()));

    // get the monitor screen for testing against movement to an new screen in setDisplayResolution()
    QPoint loc = centralWidget->window()->geometry().center();
    prevScreenName = qApp->screenAt(loc)->name();
//    currScreen = qApp->screenAt(loc);
//    connect(currScreen, &QScreen::logicalDotsPerInchChanged, this, &MW::setDisplayResolution);

    if (isShiftOnOpen) refreshFolders();

    // initial status bar icon state
    updateStatusBar();

//    // set initial visibility
//    embelTemplateChange(embelProperties->templateId);
//    // size columns after show if device pixel ratio > 1
//    embelProperties->resizeColumns();
}

void MW::closeEvent(QCloseEvent *event)
{
    if (G::isLogger) G::log("MW::closeEvent");

    // do not allow if there is a background ingest in progress
    if (G::isRunningBackgroundIngest) {
        QString msg =
                "There is a background ingest in progress.  When it<br>"
                "has completed Winnow will close."
                ;
        G::popUp->showPopup(msg, 0);
        while (G::isRunningBackgroundIngest) G::wait(100);
    }

    // for debugging crash test
//    if (testCrash) return;

    setCentralMessage("Closing Winnow ...");
    stop("MW::closeEvent");
//    metaReadThread->stop();
//    imageCacheThread->stop();
//    metadataCacheThread->stop();
    if (filterDock->isVisible()) {
        folderDock->raise();
        folderDockVisibleAction->setChecked(true);
    }
    if (!simulateJustInstalled) writeSettings();
    closeLog();
    closeErrLog();
    clearPickLog();
    clearRatingLog();
    clearColorClassLog();
    // crash log
    setting->setValue("hasCrashed", false);
    G::popUp->close();
    hide();
    if (!QApplication::clipboard()->image().isNull()) {
        QApplication::clipboard()->clear();
    }

    if (preferencesDlg != nullptr) {
        delete pref;
        delete preferencesDlg;
    }
    delete workspaces;
    delete recentFolders;
    delete ingestHistoryFolders;
    delete embel;
    event->accept();
}

void MW::moveEvent(QMoveEvent *event)
{
    /*
    When the main winnow window is moved the zoom dialog, if it is open, must be moved as
    well. Also we need to know if the app has been dragged onto another monitor, which may
    have different dimensions and a different icc profile (win only).
*/
    if (G::isLogger) G::log("MW::moveEvent");
    QMainWindow::moveEvent(event);
    setDisplayResolution();
    updateDisplayResolution();
    emit resizeMW(this->geometry(), centralWidget->geometry());
}

void MW::resizeEvent(QResizeEvent *event)
{
    if (G::isLogger) G::log("MW::moveEvent");
    QMainWindow::resizeEvent(event);
    // re-position zoom dialog
    emit resizeMW(this->geometry(), centralWidget->geometry());
    // prevent progressBar overlapping in statusBar
    int availSpace = availableSpaceForProgressBar();
    if (availSpace < progressWidthBeforeResizeWindow && availSpace > progressWidth)
        progressWidth = availSpace;
    updateProgressBarWidth();
}

void MW::mouseMoveEvent(QMouseEvent *event)
{
    QMainWindow::mouseMoveEvent(event);
//    qDebug() << "MW::mouseMoveEvent" << event;
}

void MW::keyPressEvent(QKeyEvent *event)
{
    if (G::isLogger) G::log("MW::keyPressEvent");

    // must process first
    QMainWindow::keyPressEvent(event);

    if (event->key() == Qt::Key_Return) {
        if (G::mode == "Loupe") {
            if (dm->sf->index(dm->currentSfRow, G::VideoColumn).data().toBool()) {
                if (G::useMultimedia) videoView->playOrPause();
            }
        }
        else {
            loupeDisplay();
        }
    }
}

void MW::keyReleaseEvent(QKeyEvent *event)
{
    if (G::isLogger) G::log("MW::keyReleaseEvent");

//    qDebug() << "MW::keyReleaseEvent" << event;

    if (event->key() == Qt::Key_Escape) {
        /* Cancel the current operation without exiting from full screen mode.  If no current
           operation, then okay to exit full screen.  escapeFullScreen must be the last option
           tested.
        */
        //qDebug() << "MW::keyReleaseEvent" << G::isNewFolderLoaded;
        G::popUp->end();
        // stop loading a new folder
        if (!G::isNewFolderLoaded /*&& G::isLinearLoading*/) stop("Escape key");
        // stop background ingest
        else if (G::isRunningBackgroundIngest) backgroundIngest->stop();
        // stop file copying
        else if (G::isCopyingFiles) G::stopCopyingFiles = true;
        // end stress test
        else if (isStressTest) isStressTest = false;
        // cancel slideshow
        else if (G::isSlideShow) slideShow();
        // quit loading datamodel
//        else if (dm->loadingModel) dm->abortLoadingModel = true;
        // quit adding thumbnails
        else if (thumb->insertingThumbnails) thumb->abort = true;
        // abort Embellish export process
        else if (G::isProcessingExportedImages) emit abortEmbelExport();
        // abort color analysis
        else if (G::isRunningColorAnalysis) emit abortHueReport();
        // abort stack operation
        else if (G::isRunningStackOperation) emit abortStackOperation();
        // stop building filters
        else if (filters->buildingFilters)  buildFilters->stop();
        // exit full screen mode
        else if (fullScreenAction->isChecked()) escapeFullScreen();
    }

    if (G::isSlideShow) {
        int n = event->key() - 48;
        QVector<int> delay {0,1,2,3,5,10,30,60,180,600};

        if (slideShowTimer->isActive()) {
            if (event->key() == Qt::Key_X) {
                nextSlide();
            }
            else if (event->key() == Qt::Key_Backspace) {
                prevRandomSlide();
            }
            else if (event->key() == Qt::Key_S) {
                //qDebug() << "MW::keyReleaseEvent" << "slideCount =" << slideCount << event;
                if (slideCount > 1) slideShow();
            }
            else if (event->key() == Qt::Key_W) {
                slideShowTimer->stop();
                isSlideShowWrap = !isSlideShowWrap;
                isSlideShowHelpVisible = true;
                QString msg;
                if (isSlideShowWrap) msg = "Slide wrapping is on.";
                else msg = "Slide wrapping is off.";
                G::popUp->showPopup(msg);
            }
            else if (event->key() == Qt::Key_H) {
                slideShowTimer->stop();
                slideshowHelpMsg();
            }
            else if (event->key() == Qt::Key_Space) {
                slideShowTimer->stop();
                G::popUp->showPopup("Slideshow is paused");
            }
            else if (event->key() == Qt::Key_R) {
                isSlideShowRandom = !isSlideShowRandom;
                slideShowResetSequence();
                QString msg;
                if (isSlideShowRandom) msg = "Random selection enabled.";
                else msg = "Sequential selection enabled.";
                G::popUp->showPopup(msg);
            }
            // quick change slideshow delay 1 - 9 seconds
            else if (n > 0 && n <= 9) {
                slideShowDelay = delay[n];
                slideShowResetDelay();
                QString msg = "Slideshow interval set to " + QString::number(slideShowDelay) + " seconds.";
                G::popUp->showPopup(msg);
            }
        }
        else {  // slideshow is inactive
            if (isSlideShowHelpVisible) {
                G::popUp->end();
                isSlideShowHelpVisible = false;
            }
            if (event->key() == Qt::Key_Space) {
                G::popUp->showPopup("Slideshow is active");
                nextSlide();
                slideShowTimer->start(slideShowDelay * 1000);
            }
        }
    }

    bool isVideoMode = centralLayout->currentIndex() == VideoTab;
    if (isVideoMode) {
        if (event->key() == Qt::Key_Space) {
            if (G::useMultimedia) videoView->playOrPause();
        }
    }

    QMainWindow::keyReleaseEvent(event);
}

//bool MW::event(QEvent *event)
//{
//    return QMainWindow::event(event);
//}

bool MW::eventFilter(QObject *obj, QEvent *event)
{

     // uncomment below:
    /* use to show all events being filtered - handy to figure out which to intercept
    if (event->type()
                             != QEvent::Paint
            && event->type() != QEvent::UpdateRequest
            && event->type() != QEvent::ZeroTimerEvent
            && event->type() != QEvent::Timer
            && event->type() != QEvent::MouseMove
            && event->type() != QEvent::HoverMove
            )
    {
        qDebug() << "MW::eventFilter"
                 << "event:" <<event << "\t"
                 << "event->type:" << event->type() << "\t"
                 << "obj:" << obj << "\t"
                 << "obj->objectName:" << obj->objectName()
                 << "object->metaObject()->className:" << obj->metaObject()->className()
                    ;
//        return QWidget::eventFilter(obj, event);
    }
//*/

    /* figure out key presses
    if(event->type() == QEvent::ShortcutOverride && obj->objectName() == "MWClassWindow")
    {
        G::log("MW::eventFilter", "Performance profiling");
        qDebug() << event <<  obj;
    }
//    */

    /* Specific event
    if (event->type() == QEvent::FocusIn) {
            qDebug() << event << obj << embelDock->hasFocus();
    }*/

    /* Specific objectName
    if (obj->objectName() == "GraphicsEffect") {
//        if (event->type()        != QEvent::Paint
//                && event->type() != QEvent::UpdateRequest
//                && event->type() != QEvent::ZeroTimerEvent
//                && event->type() != QEvent::Timer
//                && event->type() == QEvent::Enter
//                )
        {
            qDebug() << "MW::eventFilter"
                     << event << "\t"
                     << event->type() << "\t"
                     << obj << "\t"
                     << obj->objectName();
//            qDebug() << event;
        }
    }
//    */

    /* EMBEL DOCK TITLE

    Set dock title if not tabified

    if (obj->objectName() == "embelDock" && event->type() == QEvent::Enter) {
        qDebug() << event;
        if (!embelDockTabActivated) embelTitleBar->setTitle("Embellish");
        else embelTitleBar->setTitle("");
        embelDockTabActivated = false;
    }
    */

    // CONTEXT MENU
    {
    /*
        - Intercept context menu to enable/disable:
            - eject usb drive menu item
            - add bookmarks menu item
        - mouseOverFolder is used in folder related context menu actions instead of
          currentViewDir
    */

    if (event->type() == QEvent::ContextMenu) {
        addBookmarkAction->setEnabled(true);
        revealFileActionFromContext->setEnabled(true);
        copyFolderPathFromContextAction->setEnabled(true);
        if (obj == fsTree->viewport()) {
            QContextMenuEvent *e = static_cast<QContextMenuEvent *>(event);
            QModelIndex idx = fsTree->indexAt(e->pos());
            mouseOverFolderPath = idx.data(QFileSystemModel::FilePathRole).toString();
            enableEjectUsbMenu(mouseOverFolderPath);
            // in folders or bookmarks but not on folder item
            if(mouseOverFolderPath == "") {
                addBookmarkAction->setEnabled(false);
                revealFileActionFromContext->setEnabled(false);
                copyFolderPathFromContextAction->setEnabled(false);
            }
        }
        else if (obj == bookmarks->viewport()) {
            QContextMenuEvent *e = static_cast<QContextMenuEvent *>(event);
            QModelIndex idx = bookmarks->indexAt(e->pos());
            mouseOverFolderPath = idx.data(Qt::ToolTipRole).toString();
            enableEjectUsbMenu(mouseOverFolderPath);
            // in folders or bookmarks but not on folder item
            if (mouseOverFolderPath == "") {
                addBookmarkAction->setEnabled(false);
                revealFileActionFromContext->setEnabled(false);
                copyFolderPathFromContextAction->setEnabled(false);
            }
        }
        else {
            enableEjectUsbMenu(G::currRootFolder);
            if (G::currRootFolder == "") {
                addBookmarkAction->setEnabled(false);
                revealFileActionFromContext->setEnabled(false);
                copyFolderPathFromContextAction->setEnabled(false);
            }
        }
    }
    } // end section

    // THUMBVIEW ZOOMCURSOR
    {
    /*
    Turn the cursor into a frame showing the ImageView zoom amount in the thumbnail.  The
    ImageView must be zoomed and no modifier keys pressed to show zoom cursor.
    */

    if (thumbView->mouseOverThumbView) {
        if (event->type() == QEvent::KeyPress || event->type() == QEvent::KeyRelease) {
            QKeyEvent *e = static_cast<QKeyEvent *>(event);
//            if (G::isModifier) {
            if (e->modifiers() != 0) {
                thumbView->setCursor(Qt::ArrowCursor);
//                qDebug() << "MW::eventFilter" << "Modifier pressed" << event;
            }
            else {
                QPoint pos = thumbView->mapFromGlobal(QCursor::pos());
                QModelIndex idx = thumbView->indexAt(pos);
//                qDebug() << "MW::eventFilter" << "Modifier not pressed" << event << pos << idx;
                if (idx.isValid()) {
                    QString src = "MW::eventFilter ZoomCursor";
                    thumbView->zoomCursor(idx, src, /*forceUpdate=*/true, pos);
                }
            }
        }
    }

    if (obj == thumbView->viewport()) {
        if (event->type() == QEvent::MouseMove) {
            QMouseEvent *e = static_cast<QMouseEvent *>(event);
            bool noModifiers = e->modifiers() == 0;
            const QModelIndex idx = thumbView->indexAt(e->pos());
            if (idx.isValid() && noModifiers) {
                QString src = "MW::eventFilter: ";
                thumbView->zoomCursor(idx, src, /*forceUpdate=*/false, e->pos());
            }
            else {
                thumbView->setCursor(Qt::ArrowCursor);
            }
        }
    }
    } // end section

    // DOCK TAB TOOLTIPS
    {
    /*
    Show a tooltip for docked widget tabs.
    */
    static int prevTabIndex = -1;
    if (QString(obj->metaObject()->className()) == "QTabBar") {
        if (event->type() == QEvent::MouseButtonPress) {      // HoverMove / MouseMove work
            QTabBar *tabBar = qobject_cast<QTabBar *>(obj);
            QMouseEvent *e = static_cast<QMouseEvent *>(event);
            int i = tabBar->tabAt(e->pos());
            if (tabBar->tabText(i) == filterDockTabText) {
                // qDebug() << "MW::eventFilter FILTERDOCK CLICKED";
                filterDockTabMousePress();
            }
        }
        if (event->type() == QEvent::MouseMove) {      // HoverMove / MouseMove work
            QTabBar *tabBar = qobject_cast<QTabBar *>(obj);
            QMouseEvent *e = static_cast<QMouseEvent *>(event);
            int i = tabBar->tabAt(e->pos());
            if (i >= 0 && i != prevTabIndex) {
                QString tip = "";
                if (tabBar->tabText(i) == folderDockTabText) tip = "System Folders Panel";
                if (tabBar->tabText(i) == favDockTabText) tip = "Bookmarks Panel";
                if (tabBar->tabText(i) == filterDockTabText) tip = "Filter Panel";
                if (tabBar->tabText(i) == metadataDockTabText) tip = "Metadata Panel";
                if (tabBar->tabText(i) == embelDockTabText) tip = "Embellish Panel";
                prevTabIndex = i;
                QFontMetrics fm(QToolTip::font());
                int h = fm.boundingRect(tip).height();
                QPoint locPos = geometry().topLeft() + e->pos() + QPoint(0, h);
                QToolTip::hideText();
                QToolTip::showText(locPos, tip, tabBar);
            }
        }
        if (event->type() == QEvent::Leave) {
            prevTabIndex = -1;
        }
    }
    } // end section

    // THUMBDOCK SPLITTER
    {
    /*
    A splitter resize of top/bottom thumbDock is happening:

    Events are filtered from qApp here by an installEventFilter in the MW contructor to
    monitor the splitter resize of the thumbdock when it is docked horizontally. In this
    situation, as the vertical height of the thumbDock changes the size of the thumbnails is
    modified to fit the thumbDock by calling thumbsFitTopOrBottom. The mouse events determine
    when a mouseDrag operation is happening in combination with thumbDock resizing. The
    thumbDock is referenced from the parent because thumbView is a friend class to MW.
    */

    if (event->type() == QEvent::MouseButtonPress) {
        if (obj->objectName() == "FiltersViewport") return false;
        QMouseEvent *e = static_cast<QMouseEvent *>(event);
        if (e->button() == Qt::LeftButton) isLeftMouseBtnPressed = true;
        /*
        qDebug() << "MW::eventFilter" << "MouseButtonPress"
                 << "isLeftMouseBtnPressed =" << isLeftMouseBtnPressed
                 << "isMouseDrag" << isMouseDrag
                 << obj->objectName();
                    //*/
    }

    if (event->type() == QEvent::MouseButtonRelease) {
        QMouseEvent *e = static_cast<QMouseEvent *>(event);
        if (e->button() == Qt::LeftButton) {
            isLeftMouseBtnPressed = false;
            isMouseDrag = false;
            if (thumbView->thumbSplitDrag) {
                thumbView->scrollToRow(thumbView->midVisibleCell, "MW::eventFilter thumbSplitter");
                thumbView->thumbSplitDrag = false;
            }
            /*
            qDebug() << "MW::eventFilter" << "MouseButtonRelease"
                     << "isLeftMouseBtnPressed =" << isLeftMouseBtnPressed
                     << "isMouseDrag" << isMouseDrag
                     << obj->objectName();
                        //*/
        }
    }

    if (event->type() == QEvent::MouseMove /*&& obj->objectName() == "MWWindow"*/) {
        if (isLeftMouseBtnPressed) isMouseDrag = true;
        /*
        qDebug() << "MW::eventFilter" << "MouseMove"
                 << "isLeftMouseBtnPressed =" << isLeftMouseBtnPressed
                 << "isMouseDrag" << isMouseDrag
                 << obj->objectName();
                 //*/
    }

    if (event->type() == QEvent::MouseButtonDblClick) {
        isMouseDrag = false;
    }

    // make thumbs fit the resized thumb dock
    if (obj == thumbDock) {
        if (event->type() == QEvent::Resize) {
            /*
            qDebug() << "MW::eventFilter" << "thumbDock::Resize"
                     << "isLeftMouseBtnPressed =" << isLeftMouseBtnPressed
                     << "isMouseDrag" << isMouseDrag
                     << obj->objectName();
                     //*/
            if (isMouseDrag) {
                if (!thumbDock->isFloating()) {
                    Qt::DockWidgetArea area = dockWidgetArea(thumbDock);
                    if (area == Qt::BottomDockWidgetArea
                        || area == Qt::TopDockWidgetArea
                        || !thumbView->isWrapping())
                    {
                        thumbView->thumbSplitDrag = true;
                        thumbView->thumbsFitTopOrBottom();
                    }
                }
            }
        }
    }
    } // end section

    /*
    if (obj->objectName() == "FiltersViewport") {
        if (event->type() == QEvent::MouseButtonPress) {
            QMouseEvent *e = static_cast<QMouseEvent *>(event);
            if (e->button() == Qt::LeftButton) {
                qDebug() << "MW::eventFilter" << obj << obj->objectName(); // << event;
                QPoint p = e->pos();
                QModelIndex idx = filters->indexAt(p);
                bool isCtrlModifier = e->modifiers() & Qt::ControlModifier;
                bool isLeftBtn = e->button() == Qt::LeftButton;
                bool isHdr = idx.parent() == QModelIndex();
                bool notIndentation = p.x() >= 10;
                bool isValid = idx.isValid();
                qDebug() << "Filters::eventFilter" << p
                         << "isLeftBtn =" << isLeftBtn
                         << "isHdr =" << isHdr
                         << "notIndentation =" << notIndentation
                         << "isValid =" << isValid
                            ;
                if (isHdr && isValid) {
                    if (filters->isSolo && !isCtrlModifier) {
                        if (filters->isExpanded(idx)) {
                            bool otherHdrWasExpanded = filters->otherHdrExpanded(idx);
                            filters->collapseAll();
                            if (otherHdrWasExpanded) filters->expand(idx);
                        }
                        else {
                            filters->collapseAll();
                            filters->expand(idx);
                        }
                    }
                    else {
                        filters->isExpanded(idx) ? filters->collapse(idx) : filters->expand(idx);
                    }
                    return false;
                }
            }
        }
    }
    //*/

    return QWidget::eventFilter(obj, event);
}

void MW::focusChange(QWidget *previous, QWidget *current)
{
    if (G::isLogger) {
        QString s;
        if (previous != nullptr) s = "Previous = " + previous->objectName();
        if (current != nullptr) s += " Current = " + current->objectName();
        G::log("MW::focusChange", s);
    }
//    qDebug() << "MW::focusChange" << previous << current;
    if (current == nullptr) return;
    if (current->objectName() == "DisableGoActions") enableGoKeyActions(false);
    else enableGoKeyActions(true);
    if (previous == nullptr) return;    // suppress compiler warning
}

void MW::resetFocus()
{
    if (G::isLogger) G::log("MW::resetFocus");
    activateWindow();
    setFocus();
}

void MW::ingestFinished()
{
    if (G::isLogger) G::log("MW::ingestFinished");
    qDebug() << "MW::ingestFinished";
    delete backgroundIngest;
    backgroundIngest = nullptr;
    G::isRunningBackgroundIngest = false;
    // Audible to signal completion
    QApplication::beep();
}

void MW::appStateChange(Qt::ApplicationState state)
{
/*
    If operating system focus changes to another app then hide the zoom dialog if it is
    visible.  If the focus is returning to Winnow then check if the zoom dialog was visible
    before and make it visible again.
*/
    if (G::isLogger) G::log("MW::appStateChange");
    if (!zoomDlg) return;
    if (state == Qt::ApplicationActive)
    {
        if (isZoomDlgVisible) zoomDlg->setVisible(true);
        resetFocus();
    }
    else {
        zoomDlg->setVisible(false);
    }
}

void MW::checkForUpdate()
{
/* Called from the help menu and from the main window show event if (isCheckUpdate = true)
   The Qt maintenancetool, an executable in the Winnow folder, checks to see if there is an
   update on the Winnow server.  If there is then Winnow is closed and the maintenancetool
   performs the install of the update.  When that is completed the maintenancetool opens
   Winnow again.
*/
    if (G::isLogger) G::log("MW::checkForUpdate");
    /* Checking for updates requires the maintenancetool.exe to be in the Winnow.exe folder,
       which is only true for the installed version of Winnow in the "Program Files".  In
       order to simulate for testing during development, the maintenancetool.exe path must
       be set to "c:/program files/winnow/maintenancetool.exe".
      */
    QString maintanceToolPath = qApp->applicationDirPath() + "/maintenancetool";
    QString maintenancePathToUse;
    if (qApp->applicationDirPath().toLower().contains("release")) {
        maintenancePathToUse = "c:/program files/winnow/maintenancetool";
        // RETURN UNLESS TESTING UPDATER IN DEV
        return;
    }
    else
        maintenancePathToUse = maintanceToolPath;
//    qDebug() << "MW::checkForUpdate" << "maintenancePathToUse" << maintenancePathToUse;

#ifdef Q_OS_MAC
//    return false;
#endif

#ifdef Q_OS_LINIX
    rerurn false;
#endif

#ifdef Q_OS_WIN
    QProcess process;
    process.setProgram(maintenancePathToUse);
    QStringList chkArgs("check-updates");
    process.setArguments(chkArgs);
    process.start();
    // Wait op to 3 seconds for the update tool to finish
    process.waitForFinished(3000);

    // bail if failed
    if (process.error() != QProcess::UnknownError)
    {
        QString msg = "Error checking for updates";
        if (!isStartingWhileUpdating) G::popUp->showPopup(msg, 1500);
        isStartingWhileUpdating = false;
        return;
    }

    // Read the output
    QByteArray data = process.readAllStandardOutput();
    QVariant noUpdataAvailable = data.contains("no updates available");
    /*
    qDebug() << "MW::checkForUpdate" << "process.error() =" << process.error();
    qDebug() << "MW::checkForUpdate" << "noUpdataAvailable =" << noUpdataAvailable;
    qDebug() << "MW::checkForUpdate" << "data =" << data;
    qDebug() << "MW::checkForUpdate" << "exitCode() =" << process.exitCode();
    qDebug() << "MW::checkForUpdate" << "readAllStandardError() =" << process.readAllStandardError();

    if (G::isFileLogger) Utilities::log("MW::checkForUpdate", "process.error() = " + QString::number(process.error()));
    if (G::isFileLogger) Utilities::log("MW::checkForUpdate", "noUpdataAvailable = " + noUpdataAvailable.toString());
    if (G::isFileLogger) Utilities::log("MW::checkForUpdate", "data = " + data);
    if (G::isFileLogger) Utilities::log("MW::checkForUpdate", "readAllStandardError() = " + process.readAllStandardError());
    if (G::isFileLogger) Utilities::log("MW::checkForUpdate", "exitCode() = " + QString::number(process.exitCode()));
//    */
    G::log("MW::checkForUpdate", "after check");

    if (noUpdataAvailable.toBool())
    {
        QString msg = "No updates available";
        if(!isStartingWhileUpdating) G::popUp->showPopup(msg, 1500);
        return;
    }

    updateAppDlg = new UpdateApp(version, css);
    int ret = updateAppDlg->exec();
    if (ret == QDialog::Rejected) {
        process.close();
        return;
    }

    /* Call the maintenance tool binary
       Note: we start it detached because this application needs to close for the update
       */
    QStringList args("--updater");
    bool startMaintenanceTool = QProcess::startDetached(maintenancePathToUse, args);

    // Close Winnow
    if (startMaintenanceTool) {
        QString msg = "Updating Winnow.  Winnow will reopen when update is completed.";
        G::popUp->showPopup(msg, 2000);
        G::wait(2000);
        qApp->closeAllWindows();
    }
    else if(!isStartingWhileUpdating)
        G::popUp->showPopup("The maintenance tool failed to open", 2000);

#endif
}

void MW::handleStartupArgs(const QString &args)
{
/*
    Argument options:

    arg[0]     = system parameter - usually path to executing program (Winnow)

    if arg[1]  = "Embellish" then
       arg[2]  = templateName (also name on Winnet used to send to Winnow)
       arg[3+] = path to each image being exported to be embellished

    else
       arg[1+] = path to each image to view in Winnow. Only arg[1] is used to
       determine the directory to open in Winnow.

    Winnets are small executables that act like photoshop droplets. They reside in
    QStandardPaths::AppDataLocation (Windows: user/AppData/Roaming/Winnow/Winnets and
    Mac::/Users/user/Library/Application Support/Winnow/Winnets). They send a list of
    files and a template name to Winnow to be embellished. For example, in order for
    Winnow to embellish a series of files that have been exported from lightroom, Winnow
    needs to know which embellish template to use. Instead of sending the files directly
    to Winnow, thay are sent to an intermediary program (a Winnet) that is named after
    the template. The Winnet (ie Zen2048) receives the list of files, inserts the strings
    "Embellish" and the template name "Zen2048" and then resends to Winnow.
*/
    if (G::isLogger) G::log("MW::handleStartupArgs", args);

    if (args.length() < 2) return;

    stop("MW::handleStartupArgs");

    QString delimiter = "\n";
    QStringList argList = args.split(delimiter);

    qDebug() << "MW::handleStartupArgs" << argList;
    /*
    QString a = "";
    for (QString s : argList) a += s + "\n";
    QMessageBox::information(this, "MW::handleStartupArgs", a);
    //*/
    if (argList.length() > 1) {
        if (G::isFileLogger) Utilities::log("MW::handleStartupArgs Winnow Location", qApp->applicationDirPath());
        if (G::isFileLogger) Utilities::log("MW::handleStartupArgs", argList.join(" | "));
    }

    QStringList pathList;
    QString templateName;
    if (argList.at(1) == "Embellish") {
        /* show main window now.  If we don't, then the update progress popup will not be
        visible.  If there is a significant delay, when a lot of images have to be processed,
        this would be confusing for the user.  */
        show();
//        qApp->processEvents();
        /* activate Winnow when receiving arguments - not working ...
        raise();
        setWindowFlags(Qt::WindowStaysOnTopHint);
        this->activateWindow();
        this->setFocus();
        */

        // check if any image path sent, if not, return
        if (argList.length() < 4) return;
        // get the embellish template to use
        templateName = argList.at(2);

        /* log
        if (G::isFileLogger) Utilities::log("MW::handleStartupArgs", "Template to use: " + templateName);
        //*/

        // get the folder where the files to embellish are located
        QFileInfo info(argList.at(3));
        QString folderPath = info.dir().absolutePath();
//        QMessageBox::information(this, "MW::handleStartupArgs", folderPath);
//        if (G::isFileLogger) Utilities::log("MW::handleStartupArgs", args);

        // list of all supported files in the folder
        QStringList fileFilters;
        foreach (const QString &str, metadata->supportedFormats)
                fileFilters.append("*." + str);
        QDir dir;
        dir.setNameFilters(fileFilters);
        dir.setFilter(QDir::Files);
        dir.setSorting(QDir::Time /*| QDir::Reversed*/);
        dir.setPath(folderPath);

        /* Get earliest lastModified time (t) for incoming files, then choose all files in the
        folder that are Winnow supported formats and have been modified after (t). This allows
        unlimited files to be received, getting around the command argument buffer limited
        size. */

        /* The earliest modified date for incoming files is a little bit tricky.  The incoming
        files have been saved to the folder folderPath by the exporting program (ie lightroom).
        However, this folder might already have existing files.  If the command argument
        buffer has been exceeded then the argument list may not contain the earliest modified
        file.  To determine which files are part of the incoming the modified date of the first
        file in the command argument buffer is used as a seed value, and any file with a
        modified date up to 10 seconds earlier becomes the new seed value.  After reviewing all
        the eligible files in folderPath the seed value will be the earliest modified incoming
        file.   */

        // get seed time (t) to start
        info = dir.entryInfoList().at(0);
        QDateTime t = info.lastModified();
        // tMinus10 is ten seconds earlier
        QDateTime tMinus10;
        for (int i = 0; i < dir.entryInfoList().size(); ++i) {
            QString fPath = dir.entryInfoList().at(i).absoluteFilePath();
            info.setFile(fPath);
            QDateTime tThis = info.lastModified();
            QDateTime tOld = t;
            // time first file last modified
            tMinus10 = t.addSecs(-10);
            if (tThis < t && tThis > tMinus10) t = tThis;
            /* log
            qDebug() << "MW::handleStartupArgs"
                     << i
                     << "tOld =" << tOld.toString("yyyy-MM-dd hh:mm:ss")
                     << "tMinus10 =" << tMinus10.toString("yyyy-MM-dd hh:mm:ss")
                     << "tThis =" << tThis.toString("yyyy-MM-dd hh:mm:ss")
                     << "t =" << t.toString("yyyy-MM-dd hh:mm:ss")
                     << fPath
                        ;
            QString msg = QString::number(i).rightJustified(3) +
                          " tOld = " + tOld.toString("yyyy-MM-dd hh:mm:ss") +
                          " tMinus10 = " + tMinus10.toString("yyyy-MM-dd hh:mm:ss") +
                          " tThis = " + tThis.toString("yyyy-MM-dd hh:mm:ss") +
                          " t = " + t.toString("yyyy-MM-dd hh:mm:ss") +
                          "  " + fPath
                          ;
            if (G::isFileLogger) Utilities::log("MW::handleStartupArgs", msg);
            //*/
        }

        /* log
        if (G::isFileLogger) Utilities::log("MW::handleStartupArgs", QString::number(dir.entryInfoList().size()) + " files " +
                       folderPath + "  Cutoff = " + t.toString("yyyy-MM-dd hh:mm:ss"));
        //*/

        // add the recently modified incoming files to pathList
        for (int i = 0; i < dir.entryInfoList().size(); ++i) {
            info = dir.entryInfoList().at(i);
            // only add files just modified
            if (info.lastModified() >= t) {
                pathList << info.filePath();
                /* log
                QString msg = QString::number(i) +
                        " Adding " + info.lastModified().toString("yyyy-MM-dd hh:mm:ss") +
                        " " + info.filePath();
                if (G::isFileLogger) Utilities::log("MW::handleStartupArgs", msg);
                //*/
            }
        }

        setCentralMessage("Loading Embellished ...");
//        QApplication::processEvents();

        // create an instance of EmbelExport
        EmbelExport embelExport(metadata, dm, icd, embelProperties);

        // export get the location for the last embellished file
        QString fPath = embelExport.exportRemoteFiles(templateName, pathList);
        if (G::isFileLogger) Utilities::log("MW::handleStartupArgs", "lastFileExportedThumbPath = " + fPath);

        info.setFile(fPath);
        QString fDir = info.dir().absolutePath();
//        fsTree->getFolderImageCount(fDir, true, "MW::handleStartupArgs");
        // go there ...
        fsTree->select(fDir);
        // refresh FSTree counts
        fsTree->refreshModel();
        folderAndFileSelectionChange(fPath, "handleStartupArgs");
    }

    // startup not triggered by embellish winnet
    else {
        QFileInfo f(argList.at(1));
        f.dir().path();
        fsTree->select(f.dir().path());
//        selectionChange();
        folderSelectionChange();
         if (G::isFileLogger) Utilities::log("MW::handleStartupArgs", "startup not triggered by embellish winnet");
    }

    if (G::isFileLogger) Utilities::log("MW::handleStartupArgs", "done");
    return;
}

void MW::watchCurrentFolder()
{
/*
    Not working.
    This slot is signalled from FSTree when a folder selection changes.  We are interested
    in selection changes caused by a drive being ejected.  If the current folder being
    cached no longer exists (ejected) then make a folderSelectionChange.
*/
    if (G::isLogger) G::log("MW::watchCurrentFolder");
//    qDebug() << "MW::watchCurrentFolder" << currentViewDirPath;
    if (G::currRootFolder == "") return;
    QFileInfo info(G::currRootFolder);
    if (info.exists()) return;
//    selectionChange();
    folderSelectionChange();
}

void MW::selectionChange()
{
/*
   This is invoked when there is a folder selection change in the folder or bookmark views.

   - folder change triggers MW::selectionChange
   - return (ignore) if prior folder selection change is still being stopped
   - call MW::stop
   - all worker threads are stopped
   - resets all necessary info
   - Finally, MW::folderSelectionChange is called

Flow Flags:

    G::isInitializing
    G::stop
    G::allMetadataLoaded
    G::allIconsLoaded
    G::isNewFolderLoaded
    dm->loadingModel
    dm->basicFileInfoLoaded  // not used
    G::isLinearLoading
    G::ignoreScrollSignal
    isCurrentFolderOkay
    isFilterChange
    G::isNewSelection

Current model row:

    currentRow
    dm->currentRow
    dm->currentFilePath
    currentDmIdx
    currentSfIdx

*/
    QSignalBlocker blocker(bookmarks);

    if (G::isLogger || G::isFlowLogger) {
        G::log("skipline");
        G::log("MW::selectionChange");
    }
    qDebug() << "MW::selectionChange" << "instance" << dm->instance;
    qDebug() << " ";

    testTime.restart();
    if (G::isInitializing) {
        qDebug() << "MW::selectionChange" << "Ignore: G::isInitializing = true";
        return;
    }

    // ignore if very rapid selection and current folder is still at stopAndClearAll

    if (G::stop) {
        qDebug() << "MW::selectionChange" << "Ignore: G::stop = true";
        return;
    }
    if (dm->loadingModel) {
        qDebug() << "MW::selectionChange" << "Ignore: dm->loadingModel = true";
        return;
    }

    qDebug() << "MW::selectionChange" << getSelectedPath();

    if (stop("MW::selectionChange")) {
        qDebug() << "Time to stop =" << testTime.elapsed();
        folderSelectionChange();
    }
    else {
        qDebug() << "MW::selectionChange" << "IGNORE - BUSY" << dm->instance;
    }

    blocker.unblock();
}

void MW::folderSelectionChange()
{
/*
    This is invoked when there is a folder selection change in the folder or bookmark views.
*/
    QSignalBlocker bookmarkBlocker(bookmarks);
    QSignalBlocker fsTreeBlocker(fsTree);

    if (G::stop) return;

    if (G::isLogger || G::isFlowLogger) {
        G::log("skipline");
        G::log("MW::folderSelectionChange");
    }

    G::t.restart();

    stop("MW::folderSelectionChange()");

    dm->abortLoadingModel = false;
    G::currRootFolder = getSelectedPath();
    setting->setValue("lastDir", G::currRootFolder);

    setCentralMessage("Loading information for folder " + G::currRootFolder);
//    qDebug() << " ";
//    qDebug() << "MW::folderSelectionChange" << "New folder =" << G::currRootFolder;
//    if (G::isFileLogger) Utilities::log("MW::folderSelectionChange", G::currRootFolder);

    // do not embellish
    if (turnOffEmbellish) embelProperties->doNotEmbellish();

    // ImageView set zoom = fit for the first image of a new folder
    imageView->isFirstImageNewFolder = true;

    // Prevent build filter if taking too long
    dm->forceBuildFilters = false;

    // used by updateStatus
    isCurrentFolderOkay = false;
    pickMemSize = "";

    // stop slideshow if a new folder is selected
    if (G::isSlideShow && !isStressTest) slideShow();

    // if previously in compare mode switch to loupe mode
    if (asCompareAction->isChecked()) {
        asCompareAction->setChecked(false);
        asLoupeAction->setChecked(true);
        updateState();
    }

    // if at welcome or message screen and then select a folder
    if (centralLayout->currentIndex() == StartTab
            || centralLayout->currentIndex() == MessageTab) {
        if (prevMode == "Loupe") asLoupeAction->setChecked(true);
        else if (prevMode == "Grid") asGridAction->setChecked(true);
        else if (prevMode == "Table") asTableAction->setChecked(true);
        else if (prevMode == "Compare") asLoupeAction->setChecked(true);
        else {
            prevMode = "Loupe";
            asLoupeAction->setChecked(true);
        }
    }

    // confirm folder exists and is readable, report if not and do not process
    if (!isFolderValid(G::currRootFolder, true /*report*/, false /*isRemembered*/)) {
        stop("Invalid folder");
        setWindowTitle(winnowWithVersion);
        if (G::isLogger) if (G::isFileLogger) Utilities::log("MW::folderSelectionChange", "Invalid folder " + G::currRootFolder);
        return;
    }

    // sync the bookmarks with the folders view fsTree
    bookmarks->select(G::currRootFolder);

    // add to recent folders
    addRecentFolder(G::currRootFolder);

    // sync the folders tree with the current folder
    fsTree->scrollToCurrent();

    // update menu
    enableEjectUsbMenu(G::currRootFolder);

    // clear filters
    uncheckAllFilters();

    // update metadata read status light
    updateMetadataThreadRunStatus(true, true, "MW::folderSelectionChange");

    // load datamodel
    if (!dm->load(G::currRootFolder, G::includeSubfolders)) {
        updateMetadataThreadRunStatus(false, true, "MW::folderSelectionChange");
        qWarning() << "WARNING" << "Datamodel Failed To Load for" << G::currRootFolder;
        enableSelectionDependentMenus();
        if (dm->abortLoadingModel) {
            updateStatus(false, "Image loading has been cancelled", "MW::folderSelectionChange");
            setCentralMessage("Image loading has been cancelled");
            return;
        }
        QDir dir(G::currRootFolder);
        if (dir.isRoot()) {
            updateStatus(false, "No supported images in this drive", "MW::folderSelectionChange");
            setCentralMessage("The root folder \"" + G::currRootFolder + "\" does not have any eligible images");
        }
        else {
            updateStatus(false, "No supported images in this folder", "MW::folderSelectionChange");
            setCentralMessage("The folder \"" + G::currRootFolder + "\" does not have any eligible images");
        }
        return;
    }

    // turn off include subfolders to prevent accidental loading a humungous number of files
    G::includeSubfolders = false;
    subFoldersAction->setChecked(false);
    updateStatusBar();

    // datamodel loaded - invalidate indexes (set in MW::fileSelectionChange)
    dm->currentSfRow = -1;
    dm->currentSfIdx = dm->sf->index(-1, -1);
    dm->currentDmIdx = dm->index(-1, -1);

    // made it this far, folder must have eligible images and is good-to-go
    isCurrentFolderOkay = true;

    // folder change triggered by dragdrop event (see main/draganddrop.cpp)
    bool dragFileSelected = false;
    if (isDragDrop) {
        if (dragDropFilePath.length() > 0) {
            QFileInfo info(dragDropFilePath);
            QString fileType = info.suffix().toLower();
            if (metadata->supportedFormats.contains(fileType)) {
                sel->current(dragDropFilePath);
//                dm->select(dragDropFilePath);
                dragFileSelected = true;
            }
        }
        isDragDrop = false;
    }

//    if (!dragFileSelected) {
//        dm->selectThumb(0);
//    }

    // format pickMemSize as bytes, KB, MB or GB
    pickMemSize = Utilities::formatMemory(memoryReqdForPicks());
    updateStatus(true, "", "MW::folderSelectionChange");

    // Load folder progress
    QString msg = "Gathering metadata and thumbnails for images in folder " + G::currRootFolder;
    if (subFoldersAction->isChecked()) msg += " and all subfolders";
    setCentralMessage(msg);
    updateStatus(false, "Collecting metadata for all images in folder(s)", "MW::folderSelectionChange");

    /*
    Must load metadata first, as it contains the file offsets and lengths for
    the thumbnail and full size embedded jpgs and the image width and height,
    req'd in imageCache to manage cache max size. The metadataCaching thread
    also loads the thumbnails. It triggers the loadImageCache when it is
    finished. The image cache is limited by the amount of memory allocated.

    While still initializing, the window show event has not happened yet, so
    the thumbsPerPage, used to figure out how many icons to cache, is unknown.
    250 is the default.
    */

    bookmarkBlocker.unblock();
    fsTreeBlocker.unblock();

    // start loading new folder
    G::t.restart();
    buildFilters->reset();
    if (G::isLinearLoading) {
        // metadata and icons loaded in GUI thread
        //qDebug() << "MW::folderSelectionChange loadLinearNewFolder";
        loadLinearNewFolder();
    }
    else {
        // metadata and icons read using multiple threaded readers
        //qDebug() << "MW::folderSelectionChange loadConcurrentNewFolder";
        loadConcurrentNewFolder();
    }

//    G::stop = false;
}

void MW::fileSelectionChange(QModelIndex current, QModelIndex previous, bool clearSelection, QString src)
{
/*
    Triggered when file selection changes (folder change selects new image, so it also
    triggers this function). The new image is loaded, the pick status is updated and the
    infoView metadata is updated. The imageCache is updated if necessary. The imageCache
    will not be updated if triggered by folderSelectionChange since a new one will be
    created. The metadataCache is updated to include metadata and icons for all the
    visible thumbnails.

    Note that the datamodel includes multiple columns for each row and the index sent to
    fileSelectionChange could be for a column other than 0 (from tableView) so scrollTo
    and delegate use of the current index must check the column.
*/
    if (G::isLogger || G::isFlowLogger) G::log("MW::fileSelectionChange", src + " " + current.data(G::PathRole).toString());

    if (G::stop) return;
    G::isNewSelection = false;

   /* debug
    qDebug() << "MW::fileSelectionChange"
             << "src =" << src
             << "G::fileSelectionChangeSource =" << G::fileSelectionChangeSource
             << "current =" << current
             << "row =" << current.row()
             << "dm->currentDmIdx =" << dm->currentDmIdx
             << "G::isInitializing =" << G::isInitializing
             << "G::isNewFolderLoaded =" << G::isNewFolderLoaded
//             << "isFirstImageNewFolder =" << imageView->isFirstImageNewFolder
             << "isFilterChange =" << isFilterChange
//             << "isCurrentFolderOkay =" << isCurrentFolderOkay
//             << "icon row =" << thumbView->currentIndex().row()
             << dm->sf->index(current.row(), 0).data(G::PathRole).toString()
                ;
                //*/

    if (!rememberLastDir)
        if (!isCurrentFolderOkay
                || G::isInitializing
                || isFilterChange
                || !G::isNewFolderLoaded)
            return;

    if (!currRootDir.exists()) {
        refreshFolders();
        folderSelectionChange();
        return;
    }

    // if starting program, set first image to display
    if (current.row() == -1) {
        sel->current(0);
        return;
    }

    // check if current selection = current index.  If so, nothng to do
    if (current == dm->currentSfIdx) {
        // qDebug() << "MW::fileSelectionChange  current selection = current index";
        return;
    }
    /*
    qDebug() << "MW::fileSelectionChange"
             << "src =" << src
             << "row =" << current.row()
             << dm->sf->index(current.row(), 0).data(G::PathRole).toString()
                ;  //*/

    // Check if anything selected.  If not disable menu items dependent on selection
    enableSelectionDependentMenus();

    /*
    if (isDragDrop && dragDropFilePath.length() > 0) {
        thumbView->selectThumb(dragDropFilePath);
        isDragDrop = false;
    }
    */

    // record current proxy row (dm->sf) as it is used to sync everything
    dm->currentSfRow = current.row();
    dm->currentSfIdx = dm->sf->index(current.row(), 0);
    dm->currentDmIdx = dm->sf->mapToSource(dm->currentSfIdx);
    dm->currentDmRow = dm->currentDmIdx.row();
    // select
    if (clearSelection) sel->current(current);
    // the file path is used as an index in ImageView
    QString fPath = dm->currentSfIdx.data(G::PathRole).toString();
    // also update datamodel, used in MdCache
    dm->currentFilePath = fPath;
    setting->setValue("lastFileSelection", fPath);

    // update delegates so they can highlight the current item
    thumbView->iconViewDelegate->currentRow = dm->currentSfRow;
    gridView->iconViewDelegate->currentRow = dm->currentSfRow;

    // update anchor for shift click mouse contiguous selections
    thumbView->shiftAnchorIndex = current;
    gridView->shiftAnchorIndex = current;

    // don't scroll if mouse click source (screws up double clicks and disorients users)
    if (G::fileSelectionChangeSource == "TableMouseClick") {
        G::ignoreScrollSignal = true;
        if (gridView->isVisible()) gridView->scrollToCurrent();
        if (thumbView->isVisible()) thumbView->scrollToCurrent();
    }
    else if (G::fileSelectionChangeSource == "ThumbMouseClick") {
        G::ignoreScrollSignal = true;
        if (gridView->isVisible()) gridView->scrollToCurrent();
        if (tableView->isVisible()) tableView->scrollToCurrent();
    }
    else if (G::fileSelectionChangeSource == "GridMouseClick") {
        G::ignoreScrollSignal = true;
        if (thumbView->isVisible()) thumbView->scrollToCurrent();
        if (tableView->isVisible()) tableView->scrollToCurrent();
    }
    else {
        if (gridView->isVisible()) gridView->scrollToCurrent();
        if (thumbView->isVisible())  thumbView->scrollToCurrent();
        if (tableView->isVisible()) tableView->scrollToCurrent();
    }

    // update delegates to show current and set focus to enable shift + direction keys
    if (gridView->isVisible()) gridView->updateView();
    if (thumbView->isVisible()) thumbView->updateView();

    // set focus to enable shift + direction keys
    if (gridView->isVisible()) gridView->setFocus();
    if (thumbView->isVisible()) thumbView->setFocus();

//    // icons may require resizing to fit
//    if (thumbView->isVisible()) thumbView->repaint();
//    if (gridView->isVisible()) gridView->repaint();

    // reset table, grid or thumb item clicked
    G::fileSelectionChangeSource = "";
    G::ignoreScrollSignal = false;

    if (G::isSlideShow && isSlideShowRandom) metadataCacheThread->stop();

    // new file name appended to window title
    setWindowTitle(winnowWithVersion + "   " + fPath);

    if (!G::isSlideShow) progressLabel->setVisible(isShowCacheProgressBar);

    // update loupe view
    if (G::useImageView) {
        if (G::useMultimedia) videoView->stop();
        bool isVideo = dm->sf->index(dm->currentSfRow, G::VideoColumn).data().toBool();
        if (isVideo) {
            if (G::useMultimedia) {
                videoView->load(fPath);
                if (G::mode == "Loupe") centralLayout->setCurrentIndex(VideoTab);
            }
        }
        else {
            if (imageView->loadImage(fPath, "MW::fileSelectionChange")) {
                updateClassification();
                if (G::mode == "Loupe") centralLayout->setCurrentIndex(LoupeTab);
            }
            else {
//                qWarning() << "WARNING" << "MW::fileSelectionChange" << "loadImage failed for" << fPath;
            }
        }
    }

    /* force scroll in case not up-to-date after holding nav key down
    if (thumbView->isVisible()) {
        thumbView->scrollToRow(dm->currentSfRow, "MW::fileSelectionChange");
    }
    */

    // update caching if folder has been loaded
    if (G::isNewFolderLoaded) {
        fsTree->scrollToCurrent();          // req'd for first folder when Winnow opens
        updateIconRange(dm->currentSfRow, "MW::fileSelectionChange");
        if (G::isLinearLoading) {
            metadataCacheThread->fileSelectionChange();
        }

        Qt::KeyboardModifiers key = QApplication::queryKeyboardModifiers();

        /* Do not image cache if there is an active random slide show or a modifier key
        is pressed. (turn off image caching for testing with G::useImageCache = false. set in
        global.cpp) */

        /*
        qDebug() << "MW::fileSelectionChange" << "IMAGECACHE"
                 << "isNoModifier =" << (key == Qt::NoModifier)
                 << "isShiftModifier =" << (key == Qt::ShiftModifier)
                 << "isControlModifier =" << (key == Qt::ControlModifier)
                 << "isAltModifier =" << (key == Qt::AltModifier)
                 << "isShiftAltModifier =" << (key == (Qt::AltModifier | Qt::ShiftModifier))
                    ;
        //*/
        if (!(G::isSlideShow && isSlideShowRandom)
//            && (key == Qt::NoModifier || key == Qt::KeypadModifier)
            && (!workspaceChanged)
            && (G::mode != "Compare")
            && (G::useImageCache)
           )
        {
            //qDebug() << "MW::fileSelectionChange emit setImageCachePosition" << dm->currentFilePath;
            emit setImageCachePosition(dm->currentFilePath, "MW::fileSelectionChange");
        }
    }

    workspaceChanged = false;

    // update the metadata panel
    if (G::useInfoView) infoView->updateInfo(dm->currentSfRow);

    // initialize the thumbDock if just opened app
    if (G::isInitializing) {
        if (dockWidgetArea(thumbDock) == Qt::BottomDockWidgetArea ||
            dockWidgetArea(thumbDock) == Qt::TopDockWidgetArea)
        {
            thumbView->setWrapping(false);
        }
        else {
            thumbView->setWrapping(true);
        }
        if (thumbDock->isFloating()) thumbView->setWrapping(true);
    }

    // update cursor position on progressBar
    cacheProgressBar->updateCursor(dm->currentSfRow, dm->sf->rowCount());

    fsTree->scrollToCurrent();

    if (G::isLogger) G::log("MW::fileSelectionChange", "Finished " + fPath);
}

void MW::tryLoadImageAgain(QString fPath)
{
/*
    Not being used.

    This is triggered from ImageView::loadImage when attempting to load a cache image
    that is still being cached.  After a delay, try to load ImageView again.
*/
    if (G::isLogger) G::log("MW::tryLoadImageAgain", fPath);
    QTimer::singleShot(500, [this, fPath]() {
                        if (!imageView->isBusy)
                            imageView->loadImage(fPath, "MW::tryLoadImageAgain");
                      });
}

void MW::folderAndFileSelectionChange(QString fPath, QString src)
{
/*
    Loads the folder containing the image and then selects the image.  Used by
    MW::handleStartupArgs and MW::handleDrop.  After the folder change a delay
    is req'd before the initial metadata has been cached and the image can be
    selected.
*/
    if (G::isLogger) G::log("MW::folderAndFileSelectionChange", fPath);
    setCentralMessage("Loading " + fPath + " ...");

    embelProperties->setCurrentTemplate("Do not Embellish");
    QFileInfo info(fPath);
    QString folder = info.dir().absolutePath();

    if (src == "handleDropOnCentralView") {
        if (folder == G::currRootFolder) {
            if (dm->proxyIndexFromPath(fPath).isValid()) {
                sel->current(fPath);
                dm->currentSfIdx = dm->proxyIndexFromPath(fPath);
                fileSelectionChange(dm->currentSfIdx, dm->currentSfIdx, true, "MW::folderAndFileSelectionChange");
            }
            return;
        }
    }

    /*
    qDebug() << "MW::folderAndFileSelectionChange"
             << "isStartupArgs =" << isStartupArgs
             << "folder =" << folder
             << "currentViewDir =" << currentViewDir
                ;
                //*/

    if (!fsTree->select(folder)) {
        qWarning() << "WARNING" << "MW::folderAndFileSelectionChange" << "fsTree failed to select" << fPath;
        if (G::isFileLogger) Utilities::log("MW::folderAndFileSelectionChange", "fsTree failed to select " + fPath);
        return;
    }

    if (centralLayout->currentIndex() == CompareTab) {
        centralLayout->setCurrentIndex(LoupeTab);
    }
    dm->selectionModel->clear();

    // path to image, used in loadImageCacheForNewFolder to select image
    folderAndFileChangePath = fPath;
    if (G::isFileLogger) Utilities::log("MW::folderAndFileSelectionChange", "call folderSelectionChange for " + folderAndFileChangePath);
    qDebug() << "MW::folderAndFileSelectionChange" << folderAndFileChangePath;
    folderSelectionChange();

    return;
}

bool MW::stop(QString src)
{
/*
    Stops and clears all folder loading processes and data.

    Triggered when:
    - a folder is selected in the folder panel or open menu.
    - a bookmark is selected.
    - a drive is ejected and the resulting folder does not have any eligible images.
    - ESC is pressed while folder is loading.

    DataModel instances:

    The datamodel instance, dm->instance, starts at zero and is incremented when a
    new folder is selected.  G::dmInstance is the global variant.  This is required
    as the image decoders, running in separate threads, may still be decoding an
    image from a prior folder.  See ImageCache::fillCache.

*/
    if (G::isLogger || G::isFlowLogger) G::log("MW::stop", G::currRootFolder);

    dm->instance++;
    G::dmInstance = dm->instance;
    QString oldFolder = G::currRootFolder;

    // clear queued signals from MetaRead
#ifdef METAREAD
    disconnect(metaReadThread, &MetaRead::setIcon, dm, &DataModel::setIcon);
    connect(metaReadThread, &MetaRead::setIcon, dm, &DataModel::setIcon, Qt::BlockingQueuedConnection);
    disconnect(metaReadThread, &MetaRead::addToDatamodel, dm, &DataModel::addMetadataForItem);
    connect(metaReadThread, &MetaRead::addToDatamodel, dm, &DataModel::addMetadataForItem, Qt::BlockingQueuedConnection);
    disconnect(metaReadThread, &MetaRead::addToImageCache, imageCacheThread, &ImageCache::addCacheItemImageMetadata);
    connect(metaReadThread, &MetaRead::addToImageCache, imageCacheThread, &ImageCache::addCacheItemImageMetadata, Qt::BlockingQueuedConnection);
#endif
#ifdef METAREAD2
    disconnect(metaReadThread, &MetaRead2::setIcon, dm, &DataModel::setIcon);
    connect(metaReadThread, &MetaRead2::setIcon, dm, &DataModel::setIcon/*, Qt::BlockingQueuedConnection*/);
    disconnect(metaReadThread, &MetaRead2::addToDatamodel, dm, &DataModel::addMetadataForItem);
    connect(metaReadThread, &MetaRead2::addToDatamodel, dm, &DataModel::addMetadataForItem/*, Qt::BlockingQueuedConnection*/);
    disconnect(metaReadThread, &MetaRead2::addToImageCache, imageCacheThread, &ImageCache::addCacheItemImageMetadata);
    connect(metaReadThread, &MetaRead2::addToImageCache, imageCacheThread, &ImageCache::addCacheItemImageMetadata/*, Qt::BlockingQueuedConnection*/);
#endif
    /*
       Program interrupt flags used to stop all folder loading activity.  If
       the activity is happening in another thread then it stops when the thread
       is aborted.  However, after MW::stop has executed, exucution returns to
       any GUI functions that were interrupted, such as MW::loadLinearNewFolder.
       The flag "G::dmEmpty is checked to terminate these processes.
    */
    G::stop = true;
    dm->abortLoad();
    G::dmEmpty = true;

//    G::t.restart();
    buildFilters->stop();
//    qDebug() << "MW::stop" << "Stop buildFilters:        " << G::t.nsecsElapsed() << "ns";

    G::t.restart();
    metaReadThread->stop();
//    qDebug() << "MW::stop" << "Stop metaReadThread:      " << G::t.nsecsElapsed() << "ns";

    G::t.restart();
    metadataCacheThread->stop();
//    qDebug() << "MW::stop" << "Stop metadataCacheThread: " << G::t.nsecsElapsed() << "ns";

    G::t.restart();
    imageCacheThread->stop();
//    qDebug() << "MW::stop" << "Stop imageCacheThread:    " << G::t.nsecsElapsed() << "ns";

    G::t.restart();
    frameDecoder->stop();
//    qDebug() << "MW::stop" << "Stop frameDecoder:        " << G::t.nsecsElapsed() << "ns";

    reset("MW::stop");

    if (src == "Escape key") {
        setCentralMessage("Image loading has been aborted for\n" + oldFolder);
        qApp->processEvents();
    }

    return true;
}

bool MW::reset(QString src)
{
/*
    Resets everything prior to a folder change.
*/
    if (G::isLogger) G::log("MW::reset", "Source: " + src);

    if (!G::dmEmpty /*|| !G::stop*/) return false;

    G::dmEmpty = true;
    G::allMetadataLoaded = false;
    G::allIconsLoaded = false;
    G::isNewFolderLoaded = false;
    G::isNewSelection = false;
    G::currRootFolder = "";

    imageView->clear();
    setWindowTitle(winnowWithVersion);
    imageView->clear();
    if (G::useInfoView) {
        infoView->clearInfo();
        updateDisplayResolution();
    }
    isDragDrop = false;

    updateStatus(false, "", "MW::reset");
    cacheProgressBar->clearProgress();
    progressLabel->setVisible(false);
    filterStatusLabel->setVisible(false);
    updateClassification();
    dm->selectionModel->clear();
    thumbView->setUpdatesEnabled(false);
    gridView->setUpdatesEnabled(false);
    tableView->setUpdatesEnabled(false);
    tableView->setSortingEnabled(false);
    frameDecoder->clear();
    thumbView->setUpdatesEnabled(true);
    gridView->setUpdatesEnabled(true);
    tableView->setUpdatesEnabled(true);
    tableView->setSortingEnabled(true);
    imageView->isFit = true;                // req'd for initial zoom cursor condition
    dm->currentSfRow = 0;
    dm->clearDataModel();

    // turn thread activity buttons gray
    setThreadRunStatusInactive();

    setCentralMessage("Image loading has been aborted");

    G::stop = false;
    return true;
}

void MW::nullFiltration()
{
    if (G::isLogger) G::log("MW::nullFiltration");
    updateStatus(false, "No images match the filtration", "MW::nullFiltration");
    setCentralMessage("No images match the filtration");
    infoView->clearInfo();
    imageView->clear();
    progressLabel->setVisible(false);
    isDragDrop = false;
}

void MW::updateDefaultIconChunkSize(int size)
{
    if (G::isLogger) G::log("MW::updateDefaultIconChunkSize");
    // update settings
    setting->setValue("iconChunkSize", size);
    dm->defaultIconChunkSize = size;
    if (size > dm->iconChunkSize) {
        dm->iconChunkSize = size;
        metadataCacheThread->metadataChunkSize = size;
        metaReadThread->iconChunkSize = size;
    }
}

bool MW::updateIconRange(int row, QString src)
{
/*
    Polls thumbView, gridView and tableView to determine the first and last thumbnail
    visible. This is used in the metadataCacheThread or metaReadThread to determine the
    range of thumbnails to cache.

    If row == -1 (scrolled to unknown row) then the isVisible() function uses the scrollbar
    position to determine the visible thumbnails.

    The number of thumbnails to cache in the DataModel (dm->iconChunkSize) is increased if
    it is less than the visible thumbnails.
*/
    if (G::isLogger || G::isFlowLogger) G::log("MW::updateIconRange", "src = " + src + " row = " + QString::number(row));
    int firstVisible = dm->sf->rowCount();
    int lastVisible = 0;
    static int chunkSize = dm->defaultIconChunkSize;
    bool chunkSizeChanged;

    // Grid might not be selected in CentralWidget
    if (G::mode == "Grid") centralLayout->setCurrentIndex(GridTab);

    if (thumbView->isVisible()) {
        thumbView->updateVisible(row);
        if (thumbView->firstVisibleCell < firstVisible) firstVisible = thumbView->firstVisibleCell;
        if (thumbView->lastVisibleCell > lastVisible) lastVisible = thumbView->lastVisibleCell;
    }

    if (gridView->isVisible()) {
        gridView->updateVisible(row);
        if (gridView->firstVisibleCell < firstVisible) firstVisible = gridView->firstVisibleCell;
        if (gridView->lastVisibleCell > lastVisible) lastVisible = gridView->lastVisibleCell;
    }

    if (tableView->isVisible()) {
        tableView->updateVisible();
        if (tableView->firstVisibleRow < firstVisible) firstVisible = tableView->firstVisibleRow;
        if (tableView->lastVisibleRow > lastVisible) lastVisible = tableView->lastVisibleRow;
    }

    // chunk size
    int visibleIcons = lastVisible - firstVisible;
    if (dm->iconChunkSize < visibleIcons) dm->iconChunkSize = visibleIcons;
    if (dm->iconChunkSize > chunkSize) {
        chunkSize = dm->iconChunkSize;
        chunkSizeChanged = true;
    }
    else {
        chunkSizeChanged = false;
    }

    // MDCache (Linear metadata loading) icon range to use:
    metadataCacheThread->firstIconVisible = firstVisible;
    metadataCacheThread->midIconVisible = (firstVisible + lastVisible) / 2;   // rgh qCeil ??
    metadataCacheThread->lastIconVisible = lastVisible;
    metadataCacheThread->visibleIcons = visibleIcons;

    // DataModel (Concurrent metadata loading) icon range
    int firstIconRow = dm->currentSfRow - dm->iconChunkSize / 2;
    if (firstIconRow < 0) firstIconRow = 0;
    int lastIconRow = firstIconRow + dm->iconChunkSize;
    if (lastIconRow >= dm->sf->rowCount()) lastIconRow = dm->sf->rowCount() - 1;
    dm->setIconRange(firstVisible, lastVisible, firstIconRow, lastIconRow);

    /* debug
    qDebug()
         << "MW::updateIconRange" << "row =" << row << "src =" << src
//         << "dm->iconChunkSize =" << dm->iconChunkSize
//         << "G::loadOnlyVisibleIcons =" << G::loadOnlyVisibleIcons
//         << "\n\tthumbView->firstVisibleCell =" << thumbView->firstVisibleCell
//         << "thumbView->lastVisibleCell =" << thumbView->lastVisibleCell
//         << "\n\tgridView->firstVisibleCell =" << gridView->firstVisibleCell
//         << "gridView->lastVisibleCell =" << gridView->lastVisibleCell
//         << "\n\ttableView->firstVisibleCell =" << tableView->firstVisibleRow
//         << "tableView->lastVisibleCell =" << tableView->lastVisibleRow
//         << "\n\tfirstVisible =" << firstVisible
//         << "lastVisible =" << lastVisible
//         << "\n\tdm->startIconRange =" << dm->startIconRange
//         << "dm->endIconRange =" << dm->endIconRange
            ;
//        */

    return chunkSizeChanged;
}

void MW::loadConcurrentNewFolder()
{
    if (G::isLogger || G::isFlowLogger) G::log("MW::loadConcurrentNewFolder");

    QString src = "MW::loadConcurrentNewFolder ";

    // reset for bestAspect calc
    G::iconWMax = G::minIconSize;
    G::iconHMax = G::minIconSize;

    /* The memory required for the datamodel (metadata + icons) has to be estimated since the
       ImageCache is starting before all the metadata has been read.  Icons average ~180K and
       metadata ~20K  */
    int rows = dm->rowCount();
    int maxIconsToLoad = rows < dm->iconChunkSize ? rows : dm->iconChunkSize;
    G::metaCacheMB = (maxIconsToLoad * 0.18) + (rows * 0.02);

    // target image
    if (folderAndFileChangePath != "") {
        dm->currentSfRow = dm->rowFromPath(folderAndFileChangePath);
        dm->currentFilePath = folderAndFileChangePath;
    }
    else {
        dm->currentSfRow = 0;
        dm->currentFilePath = dm->sf->index(0, 0).data(G::PathRole).toString();
    }
    if (reset(src)) return;
    updateIconRange(dm->currentSfRow, "MW::loadConcurrentNewFolder");
    if (reset(src)) return;

    // set image cache parameters and build image cacheItemList
    int netCacheMBSize = cacheMaxMB - G::metaCacheMB;
    if (netCacheMBSize < cacheMinMB) netCacheMBSize = cacheMinMB;
    if (reset(src)) return;
    imageCacheThread->initImageCache(netCacheMBSize, cacheMinMB,
        isShowCacheProgressBar, cacheWtAhead);

    // no sorting or filtering until all metadata loaded
    filters->setEnabled(false);
    filterMenu->setEnabled(false);
    sortMenu->setEnabled(false);
    if (reset(src)) return;

    // read metadata using MetaRead
    metaReadThread->initialize();     // only when change folders
    if (reset(src)) return;
    if (G::isFileLogger) Utilities::log("MW::loadConcurrentNewFolder", "metaReadThread->setCurrentRow");
    metaReadThread->setCurrentRow(dm->currentSfRow, "MW::loadConcurrentNewFolder");
}

void MW::loadConcurrent(int sfRow)
/*
    Called after a scroll event in IconView or TableView by thumbHasScrolled,
    gridHasScrolled or tableHasScrolled.  updateIconRange has been called.
*/
{
    if (G::isLogger || G::isFlowLogger) G::log("MW::loadConcurrent", "Row = " + QString::number(sfRow));
    //qDebug() << "MW::loadConcurrent  sfRow =" << sfRow;
    if (!G::allMetadataLoaded || !G::allIconsLoaded) {
        if (!dm->abortLoadingModel) {
            frameDecoder->clear();
            updateMetadataThreadRunStatus(true, true, "MW::loadConcurrent");
            metaReadThread->setCurrentRow(sfRow, "MW::loadConcurrent"); // also emit option
            // emit startMetaRead(sfRow, "MW::loadConcurrent");
        }
    }
}

void MW::loadConcurrentMetaDone()
{
/*
    Signalled from MetaRead::run when finished
*/
//    QSignalBlocker blocker(bookmarks);

    if (G::isLogger || G::isFlowLogger) G::log("MW::loadConcurrentMetaDone");
//    /*
    qDebug() << "MW::loadConcurrentMetaDone" << G::t.elapsed() << "ms"
             << dm->currentFolderPath
                ;
                //*/

    if (!ignoreAddThumbnailsDlg && !G::autoAddMissingThumbnails)
        chkMissingEmbeddedThumbnails();

    if (reset()) return;
    // double check all visible icons loaded, depending on best fit
    updateIconBestFit();
    if (reset()) return;

    dm->setAllMetadataLoaded(true);                 // sets G::allMetadataLoaded = true;
    G::allIconsLoaded = dm->allIconsLoaded();

    // finalize image cache target range. Probably started to cache images before
    // all the metadata was read in MetaRead, so icd->cacheItemList not finished.
    emit setImageCachePosition(dm->currentFilePath, "MW::loadConcurrentMetaDone");

    /* now okay to write to xmp sidecar, as metadata is loaded and initial
    updates to InfoView by fileSelectionChange have been completed. Otherwise,
    InfoView::dataChanged would prematurely trigger Metadata::writeXMP
    It is also okay to filter.  */

    filters->setEnabled(true);
    filterMenu->setEnabled(true);
    sortMenu->setEnabled(true);
    if (reset()) return;
    if (!filterDock->visibleRegion().isNull() && !filters->filtersBuilt) {
        qDebug() << "MW::loadConcurrentMetaDone launchBuildFilters())";
        launchBuildFilters();   // new folder = true
    }

    if (reset()) return;
    updateMetadataThreadRunStatus(false, true, "MW::loadConcurrentMetaDone");
    // resize table columns with all data loaded
    if (reset()) return;
    tableView->resizeColumnsToContents();
    tableView->setColumnWidth(G::PathColumn, 24+8);

    // clean up possible stragglers in ImageCache::addCacheItemImageMetadata
//    emit refreshImageCache();
//    if (reset()) return;
//    if (G::isFileLogger) Utilities::log("MW::loadConcurrentMetaDone", "emit setImageCachePosition for " + dm->currentFilePath);
//    emit setImageCachePosition(dm->currentFilePath, "MW::loadConcurrentMetaDone");

//    blocker.unblock();
}

void MW::loadConcurrentStartImageCache(QString src)
{
/*
    Signalled from MetaRead after delay of MetaRead::imageCacheTriggerCount and again
    when MetaRead::run is finished.
*/
//    QSignalBlocker blocker(bookmarks);

    if (G::isLogger || G::isFlowLogger) G::log("MW::loadConcurrentStartImageCache");
    //qDebug() << "MW::loadConcurrentStartImageCache" << "src =" << src;

    if (isShowCacheProgressBar) {
        cacheProgressBar->clearProgress();
    }

    // preliminary resize table columns
//    tableView->resizeColumnsToContents();
//    tableView->setColumnWidth(G::PathColumn, 24+8);

    G::isNewFolderLoaded = true;

    /* Trigger MW::fileSelectionChange.  This must be done to initialize many things
    including current index and file path req'd by mdCache and EmbelProperties...  If
    folderAndFileSelectionChange has been executed then folderAndFileChangePath will be
    the file to select in the new folder; otherwise the first file in dm->sf will be
    selected. */
    QString fPath = "";
    if (src == "Initial") {
        fPath = folderAndFileChangePath;
        // qDebug() << "MW::loadConcurrentStartImageCache  folderAndFileChangePath =" << folderAndFileChangePath;
        folderAndFileChangePath = "";
        if (fPath != "" && dm->proxyIndexFromPath(fPath).isValid()) {
            sel->current(fPath);
            dm->currentSfIdx = dm->proxyIndexFromPath(fPath);
        }
        else {
            sel->first();
            dm->currentSfIdx = dm->sf->index(0,0);
        }

        fileSelectionChange(dm->currentSfIdx, dm->currentSfIdx, true, "MW::loadConcurrentStartImageCache");
    }

    // update image cache after all metadata has been read and cache item list is complete
    if (src == "Final") {
        /*
        qDebug() << "MW::loadConcurrentStartImageCache  src = Final"
                 << dm->currentFilePath
                    ;
                    //*/
        emit setImageCachePosition(dm->currentFilePath, "MW::loadConcurrentStartImageCache");
    }

//    blocker.unblock();
}

void MW::loadLinearNewFolder()
{
/*
    Load metadata and thumbnails in GUI thread instead of MdCache.  metadataCacheThread is
    still used (as a separate thread) for updating icons when scroll, resize or change icon
    selection.
*/
    if (G::isLogger || G::isFlowLogger) G::log("skipline");
    if (G::isLogger || G::isFlowLogger) G::log("MW::loadLinearNewFolder");

    QString src = "MW::loadLinearNewFolder ";

    // load all metadata except icons
    setCentralMessage("Reading all metadata.");
    updateMetadataThreadRunStatus(true, true, "MW::loadLinearNewFolder");

    // no sorting or filtering until all metadata loaded
    filterMenu->setEnabled(false);
    sortMenu->setEnabled(false);
    if (reset()) return;

    // add all metadata to datamodel
    dm->addAllMetadata();
    if (!dm->isAllMetadataLoaded()) {
        qWarning() << "WARNING" << "MW::loadLinearNewFolder" << "Not all metadata loaded";
//        return;
    }

    if (reset()) return;
    if (dm->abortLoadingModel /*|| !G::allMetadataLoaded*/) {
        updateStatus(false, "Image loading has been cancelled", "MW::loadLinearNewFolder");
        setCentralMessage("Image loading has been cancelled 2.");
        QApplication::processEvents();
        return;
    }

    // re-enable sorting and filtering
    if (filters->isVisible()) {
        //qDebug() << "MW::loadLinearNewFolder launchBuildFilters())";
        launchBuildFilters();
    }
    filters->setEnabled(true);
    filterMenu->setEnabled(true);
    sortMenu->setEnabled(true);

    // load icons
    qDebug() << "MW::loadLinearNewFolder  load icons";
    setCentralMessage("Reading icons.");
    QApplication::processEvents();
    if (reset()) return;

    MetadataCache *mct = metadataCacheThread;
    mct->isRefreshFolder = isRefreshingDM;
    mct->iconsCached.clear();
    mct->foundItemsToLoad = true;
    mct->startRow = 0;
    mct->instance = dm->instance;
    int rowCount = dm->sf->rowCount();
    // rgh fix (are we going to read all metadata all of the time?)
    if (mct->metadataChunkSize > rowCount) {
        mct->endRow = rowCount;
        mct->lastIconVisible = rowCount;
    }
    else {
        mct->endRow = mct->metadataChunkSize;
        mct->lastIconVisible = mct->metadataChunkSize;
    }
    // reset for bestAspect calc
    G::iconWMax = G::minIconSize;
    G::iconHMax = G::minIconSize;

    // read icons
    if (reset()) return;
    updateIconRange(dm->currentSfRow, "MW::loadLinearNewFolder");
    if (reset()) return;
//    setCentralMessage("Reading icons.");
//    QApplication::processEvents();
//    if (reset()) return;
    mct->readIconChunk();
    if (reset()) return;
    updateIconBestFit();
//    thumbView->thumbsFitTopOrBottom();
    G::allIconsLoaded = dm->allIconsLoaded();
    updateMetadataThreadRunStatus(false, true, "MW::loadLinearNewFolder");

    // check for missing thumbnails
    if (reset()) return;
    if (!ignoreAddThumbnailsDlg || G::modifySourceFiles)
        chkMissingEmbeddedThumbnails();

    // start image cache
    if (reset()) return;
    G::metaCacheMB = mct->memRequired();
    loadImageCacheForNewFolder();

    // set folder load flags
    G::isNewFolderLoaded = true;

    if (dm->isMissingEmbeddedThumb) {
        metadata->missingThumbnailWarning();
    }

    if (G::isLogger || G::isFlowLogger) G::log("MW::loadLinearNewFolder", "DONE");
    if (G::isLogger || G::isFlowLogger) G::log("skipline");
}

void MW::loadImageCacheForNewFolder()
{
/*
    This function is called from MW::loadLinearNewFolder after the metadata chunk has
    been loaded for a new folder selection. The imageCache loads images until the
    assigned amount of memory has been consumed or all the images are cached.
*/
    if (G::isLogger || G::isFlowLogger) G::log("skipline");
    if (G::isLogger || G::isFlowLogger) G::log("MW::loadImageCacheForNewFolder");

    // clear the central
    setCentralMessage("");

    // clear the cache progress bar
    if (isShowCacheProgressBar) {
        cacheProgressBar->clearProgress();
    }

    // set image cache parameters and build image cacheItemList
    int netCacheMBSize = cacheMaxMB - G::metaCacheMB;
    if (netCacheMBSize < cacheMinMB) netCacheMBSize = cacheMinMB;
    imageCacheThread->initImageCache(netCacheMBSize, cacheMinMB,
        isShowCacheProgressBar, cacheWtAhead);

    // have to wait until image caching thread running before setting flag
    G::isNewFolderLoaded = true;

    /* Trigger MW::fileSelectionChange.  This must be done to initialize many things
    including current index and file path req'd by mdCache and EmbelProperties...  If
    folderAndFileSelectionChange has been executed then folderAndFileChangePath will be
    the file to select in the new folder; otherwise the first file in dm->sf will be
    selected. */
    QString fPath = folderAndFileChangePath;
    if (G::isFileLogger) Utilities::log("MW::loadImageCacheForNewFolder", "set fPath to " + fPath);
    folderAndFileChangePath = "";
    if (fPath != "" && dm->proxyIndexFromPath(fPath).isValid()) {
        sel->current(fPath);
        dm->currentSfIdx = dm->proxyIndexFromPath(fPath);
        if (G::isFileLogger) Utilities::log("MW::loadImageCacheForNewFolder", "set fPath to " + fPath);
    }
    else {
        sel->first();
        dm->currentSfIdx = dm->sf->index(0,0);
        if (G::isFileLogger) Utilities::log("MW::loadImageCacheForNewFolder", "set fPath to first image");
    }
}

void MW::scrollChange(int sfRow, QString src)
{
//    emit startMetaRead(sfRow, "MW::scrollChange");
//    if (metaRead->isRunning) {
//        metaRead->stop();
//        G::wait(0);
//        while (metaRead->isRunning && G::wait(1) < 5);
//    }
//    iconCache->stop();
//    G::wait(0);
//    while (iconCache->isRunning && G::wait(1) < 5);
//    emit startIconCache(sfRow, src);
}

void MW::thumbHasScrolled()
{
/*
    This function is called after a thumbView scrollbar change signal. The visible
    thumbnails are determined and loaded if necessary.

    If the change was caused by the user scrolling then we want to process it, as defined
    by G::ignoreScrollSignal == false. However, if the scroll change was caused by
    syncing with another view then we do not want to process again and get into a loop.

    Also, we do not need to process scrolling if it was the result of a new selection,
    which will trigger a thumbnail update in MW::fileSelectionChange. G::isNewSelection
    is set true in IconView when a selection is made, and set false in
    fileSelectionChange.

    MW::updateIconRange polls updateVisible in all visible views (thumbView, gridView and
    tableView) to assign the firstVisibleRow, midVisibleRow and lastVisibleRow in
    metadataCacheThread (Linear) or metaReadThread (Concurrent).

    The gridView and tableView, if visible, are scrolled to sync with thumbView.

    Finally, either metadataCacheThread->scrollChange (Linear) or
    metaReadThread->setCurrentRow (Concurrent), is called to load any necessary metadata
    and icons within the cache range.

    Scrolling used to use a singleshot timer triggered by MW::loadMetadataCacheAfterDelay
    to call MW::loadMetadataChunk which, in turn, finally called the metadataCacheThread.
    This was to prevent many scroll calls from bunching up. The new approach just aborts
    the metadataCacheThread thread and starts over. It is simpler and faster.
*/
    if (G::isLogger || G::isFlowLogger) G::log("MW::thumbHasScrolled");
    if (G::isInitializing || !G::isNewFolderLoaded) return;
//    qDebug() << "MW::thumbHasScrolled  G::ignoreScrollSignal =" << G::ignoreScrollSignal;

    if (G::ignoreScrollSignal == false) {
        G::ignoreScrollSignal = true;
        updateIconRange(-1, "MW::thumbHasScrolled");
        int mvc = thumbView->midVisibleCell;
        if (gridView->isVisible())
            gridView->scrollToRow(thumbView->midVisibleCell, "MW::thumbHasScrolled");
        if (tableView->isVisible())
            tableView->scrollToRow(thumbView->midVisibleCell, "MW::thumbHasScrolled");
        // only call metadataCacheThread->scrollChange if scroll without fileSelectionChange
        if (!G::isNewSelection) {
            if (G::isLinearLoading) metadataCacheThread->scrollChange("MW::thumbHasScrolled");
            else loadConcurrent(mvc);
            // else QTimer::singleShot(50, [this, mvc]() {loadConcurrent(mvc);});
        }
        // update thumbnail zoom frame cursor
        QModelIndex idx = thumbView->indexAt(thumbView->mapFromGlobal(QCursor::pos()));
        if (idx.isValid()) {
            thumbView->zoomCursor(idx, "MW::thumbHasScrolled");
        }
    }
    G::ignoreScrollSignal = false;
}

void MW::gridHasScrolled()
{
/*
    This function is triggered after a gridView scrollbar change signal. The visible
    thumbnails are determined and loaded if necessary.

    If the change was caused by the user scrolling then we want to process it, as defined by
    G::ignoreScrollSignal == false. However, if the scroll change was caused by syncing with
    another view then we do not want to process again and get into a loop.

    Also, we do not need to process scrolling if it was the result of a new selection, which
    will trigger a thumbnail update in MW::fileSelectionChange.  G::isNewSelection is set true
    in IconView when a selection is made, and set false in fileSelectionChange.

    MW::updateIconRange polls updateVisible in all visible views (thumbView, gridView and
    tableView) to assign the firstVisibleRow, midVisibleRow and lastVisibleRow in
    metadataCacheThread (Linear) or metaReadThread (Concurrent).

    The thumbView and tableView, if visible, are scrolled to sync with gridView.

    Finally metadataCacheThread->scrollChange is called to load any necessary metadata and
    icons within the cache range.

    Scrolling used to use a singleshot timer triggered by MW::loadMetadataCacheAfterDelay to
    call MW::loadMetadataChunk which, in turn, finally called the metadataCacheThread. This
    was to prevent many scroll calls from bunching up. The new approach just aborts the
    metadataCacheThread thread and starts over. It is simpler and faster.
*/
    if (G::isLogger || G::isFlowLogger) G::log("MW::gridHasScrolled", "Visible (0 = false) = " + QString::number(gridView->isVisible()));
    if (G::isInitializing || !G::isNewFolderLoaded) return;
    if (gridView->isHidden()) return;

    if (G::ignoreScrollSignal == false) {
        G::ignoreScrollSignal = true;
        updateIconRange(-1, "MW::gridHasScrolled");
        if (thumbView->isVisible())
            thumbView->scrollToRow(gridView->midVisibleCell, "MW::gridHasScrolled");
        // only call metadataCacheThread->scrollChange if scroll without fileSelectionChange
        if (!G::isNewSelection && gridView->isVisible()) {
            if (G::isLinearLoading) metadataCacheThread->scrollChange("MW::gridHasScrolled");
            else loadConcurrent(gridView->midVisibleCell);
        }
    }
    G::ignoreScrollSignal = false;
}

void MW::tableHasScrolled()
{
/*
    This function is called after a tableView scrollbar change signal. The visible
    thumbnails are determined and loaded if necessary.

    If the change was caused by the user scrolling then we want to process it, as defined
    by G::ignoreScrollSignal == false. However, if the scroll change was caused by
    syncing with another view then we do not want to process again and get into a loop.

    Also, we do not need to process scrolling if it was the result of a new selection, which
    will trigger a thumbnail update in MW::fileSelectionChange.  G::isNewSelection is set true
    in IconView when a selection is made, and set false in fileSelectionChange.

    MW::updateIconRange polls updateVisible in all visible views (thumbView, gridView and
    tableView) to assign the firstVisibleRow, midVisibleRow and lastVisibleRow in
    metadataCacheThread (Linear) or metaReadThread (Concurrent).

    The gridView and thumbView, if visible, are scrolled to sync with tableView.

    Finally, either metadataCacheThread->scrollChange (Linear) or
    metaReadThread->setCurrentRow (Concurrent), is called to load any necessary metadata
    and icons within the cache range.

    Scrolling used to use a singleshot timer triggered by MW::loadMetadataCacheAfterDelay to
    call MW::loadMetadataChunk which, in turn, finally called the metadataCacheThread. This
    was to prevent many scroll calls from bunching up. The new approach just aborts the
    metadataCacheThread thread and starts over. It is simpler and faster.
*/
    if (G::isLogger || G::isFlowLogger) G::log("MW::tableHasScrolled");
    if (G::isInitializing || !G::isNewFolderLoaded) return;

    if (G::ignoreScrollSignal == false) {
        G::ignoreScrollSignal = true;
        updateIconRange(-1, "MW::tableHasScrolled");
        if (thumbView->isVisible())
            thumbView->scrollToRow(tableView->midVisibleRow, "MW::tableHasScrolled");
        // only call metadataCacheThread->scrollChange if scroll without fileSelectionChange
        if (!G::isNewSelection) {
            if (G::isLinearLoading) metadataCacheThread->scrollChange("MW::tableHasScrolled");
            else loadConcurrent(tableView->midVisibleRow);
//            else scrollChange(tableView->midVisibleRow, "MW::tableHasScrolled");
        }
    }
    G::ignoreScrollSignal = false;
}

void MW::numberIconsVisibleChange()
{
/*
    If there has not been another call to this function in cacheDelay ms then the
    metadataCacheThread is restarted at the row of the first visible thumb after the
    scrolling.
*/
    if (G::isLogger) G::log("MW::numberIconsVisibleChange");
    if (G::isInitializing || !G::isNewFolderLoaded) return;
    bool chunkSizeChanged = updateIconRange(dm->currentSfRow, "MW::numberIconsVisibleChange");
    /*
    qDebug() << "MW::numberIconsVisibleChange"
             << "dm->iconChunkSize =" << dm->iconChunkSize
             << "change =" << chunkSizeChanged
                ;
                */
    if (chunkSizeChanged) {
        if (G::isLinearLoading)
            metadataCacheThread->sizeChange("MW::numberIconsVisibleChange");
        else
            loadConcurrent(dm->midIconRange);
//        loadConcurrent(dm->currentSfRow);
    }
}

void MW::loadMetadataCacheAfterDelay()
{
/*
    Not being used.  Replaced by stopping MetadataCache thread.

    See MetadataCache::run comments in mdcache.cpp. A 100ms singleshot timer insures that the
    metadata caching is not restarted until there is a pause in the scolling.

    The timer is fired from MW::eventFilter, which monitors the value change signal in the
    gridView vertical scrollbar (does not have a horizontal scrollbar) and the thumbView
    vertical and horizontal scrollbars. It is also connected to the resize event in IconView,
    as a resize could increase the number of thumbs visible (ie resize to full screen) that
    require metadata or icons to be cached.

    After the delay, if another singleshot has not been fired, loadMetadataChunk is called.

    Since this function is called from scroll or resize events the selected image does not
    change and there is no need to update the image cache. The image cache is updated from
    MW::fileSelectionChange.
*/
    if (G::isLogger) G::log("MW::loadMetadataCacheAfterDelay");
    static int previousRow = 0;

    if (G::isInitializing || !G::isNewFolderLoaded) return;

    // has a new image been selected.  Caching will be started from MW::fileSelectionChange
    if (previousRow != dm->currentSfRow) {
        previousRow = dm->currentSfRow;
        return;
    }

    metadataCacheScrollTimer->start(cacheDelay);
}

void MW::loadMetadataChunk()  // rgh being used??
{
/*
    If there has not been another call to this function in cacheDelay ms then the
    metadataCacheThread is restarted at the row of the first visible thumb after the
    scrolling.
*/
    if (G::isLogger) G::log("MW::loadMetadataChunk");
    metadataCacheThread->scrollChange("MW:loadMetadataChunk");
}

void MW::loadEntireMetadataCache(QString source)
{
/*
    This is called before a filter or sort operation, which only makes sense if all the
    metadata has been loaded. This function does not load the icons. It is not run in a
    separate thread as the filter and sort operations cannot commence until all the metadata
    has been loaded.
*/
    if (G::isLogger || G::isFlowLogger) G::log("MW::loadEntireMetadataCache", "Source: " + source);
    qDebug() << "MW::loadEntireMetadataCache"
             << "Source: " << source
             << "G::isInitializing: " << G::isInitializing
             ;
    if (G::isInitializing) return;
    if (dm->isAllMetadataLoaded()) return;

    updateIconRange(dm->currentSfRow, "MW::loadEntireMetadataCache");

    QApplication::setOverrideCursor(Qt::WaitCursor);

    /* adding all metadata in dm slightly slower than using metadataCacheThread but progress
       bar does not update from separate thread
    */
    dm->addAllMetadata();
//    if (!G::allMetadataLoaded && source == "SortChange") updateSortColumn(prevSortColumn);
    // metadataCacheThread->loadAllMetadata();
    // metadataCacheThread->wait();

    QApplication::restoreOverrideCursor();

}

void MW::updateImageCacheStatus(QString instruction,
                                ImageCacheData::Cache cache,
                                QString source)
{
/*
    Displays a statusbar showing the image cache status. Also shows the cache size in the info
    panel. All status info is passed by copy to prevent collisions on source data, which is
    being continuously updated by ImageCache
*/

    if (G::isLogger) {
        QString s = "Instruction: " + instruction + "  Source: " + source;
        G::log("MW::updateImageCacheStatus", s);
    }
    if (G::isSlideShow && isSlideShowRandom) return;

    /*
    QString msg = "   currMB: " + QString::number(cache.currMB) +
                  "   minMB: "  + QString::number(cache.minMB) +
                  "   maxMB: "  + QString::number(cache.maxMB);
    updateStatus(true, msg);
    //*/

    /*
   qDebug() << "MW::updateImageCacheStatus  Instruction ="
             << instruction
             << "row =" << row
             << "source =" << source;
             //*/

    // show cache amount ie "4.2 of 16.1GB (4 threads)" in info panel
    QString cacheAmount = QString::number(double(cache.currMB)/1024,'f',1)
            + " of "
            + QString::number(double(cache.maxMB)/1024,'f',1)
            + "GB ("
            + QString::number(cache.decoderCount)
            + " threads)"
            ;
    if (G::useInfoView) {
        QStandardItemModel *k = infoView->ok;
        k->setData(k->index(infoView->CacheRow, 1, infoView->statusInfoIdx), cacheAmount);
    }

    if (!isShowCacheProgressBar) return;

    // just repaint the progress bar gray and return.
    if (instruction == "Clear") {
        cacheProgressBar->clearProgress();
        return;
    }

    int rows = cache.totFiles;

    if (instruction == "Update all rows") {
        // clear progress
        cacheProgressBar->clearProgress();
        // target range
        int tFirst = cache.targetFirst;
        int tLast = cache.targetLast + 1;
        cacheProgressBar->updateProgress(tFirst, tLast, rows,
                                    cacheProgressBar->targetColorGradient);
        // cached
        for (int i = 0; i < rows; ++i) {
//            bool metaLoaded = dm->sf->index(i, G::MetadataLoadedColumn).data().toBool();
//            bool cached = dm->sf->index(i, G::PathColumn).data(G::CachedRole).toBool();
            if (icd->cacheItemList.at(i).isCached)
                cacheProgressBar->updateProgress(i, i + 1, rows,
                                  cacheProgressBar->imageCacheColorGradient);
        }

        // cursor
        cacheProgressBar->updateCursor(dm->currentSfRow, rows);
        return;
    }

    return;
}

void MW::updateIconBestFit()
{
/*
    This function is signalled from MetadataCache after each chunk of icons have been
    processed. The best aspect of the thumbnail cells is calculated. The repainting of the
    thumbs triggers an unwanted scrolling, so the first visible thumbnail is recorded before
    the bestAspect is called and the IconView is returned to its previous position after.
*/
    if (G::isLogger || G::isFlowLogger) G::log("MW::updateIconBestFit");
    if (G::stop) return;
    gridView->bestAspect();
    thumbView->bestAspect();
}

void MW::bookmarkClicked(QTreeWidgetItem *item, int col)
{
/*
    Called by signal itemClicked in bookmark.
*/
    if (G::isLogger) G::log("MW::bookmarkClicked");
    const QString fPath =  item->toolTip(col);
    isCurrentFolderOkay = isFolderValid(fPath, true, false);

    if (isCurrentFolderOkay) {
        QModelIndex idx = fsTree->fsModel->index(fPath);
        QModelIndex filterIdx = fsTree->fsFilter->mapFromSource(idx);
        fsTree->setCurrentIndex(filterIdx);
        fsTree->select(fPath);
        fsTree->scrollTo(filterIdx, QAbstractItemView::PositionAtCenter);
//        selectionChange();
        folderSelectionChange();
    }
    else {
        stop("Bookmark clicked");
        setWindowTitle(winnowWithVersion);
        enableSelectionDependentMenus();
    }
}

void MW::checkDirState(const QString &dirPath)
{
/*
    called when signal rowsRemoved from FSTree
    does this get triggered if a drive goes offline???
    rgh this needs some TLC
*/
    qDebug() << "MW::checkDirState" << dirPath;
    if (G::isLogger) G::log("MW::checkDirState");
    if (G::isInitializing) return;

    if (!QDir().exists(G::currRootFolder)) {
        G::currRootFolder = "";
    }
}

QString MW::getSelectedPath()
{
    if (G::isLogger) G::log("MW::getSelectedPath");
    if (isDragDrop)  return dragDropFolderPath;

    if (fsTree->selectionModel()->selectedRows().size() == 0) return "";

    QModelIndex idx = fsTree->selectionModel()->selectedRows().at(0);
    if (!idx.isValid()) return "";

    QString path = idx.data(QFileSystemModel::FilePathRole).toString();
    QFileInfo dirInfo = QFileInfo(path);
    currRootDir = dirInfo.dir();
    return dirInfo.absoluteFilePath();
}

QTabBar* MW::tabifiedBar()
{

    // find the tabbar containing the dock widgets
    QTabBar* widgetTabBar = nullptr;
    QList<QTabBar *> tabList = findChildren<QTabBar *>();
    for (int i = 0; i < tabList.count(); i++) {
        if (tabList.at(i)->currentIndex() != -1) {
            widgetTabBar = tabList.at(i);
            break;
        }
    }
    return widgetTabBar;
}

bool MW::isDockTabified(QString tabText)
{

    QTabBar* widgetTabBar = tabifiedBar();
    bool found = false;
    if (widgetTabBar != nullptr) {
        int idx = widgetTabBar->currentIndex();
        for (int i = 0; i < widgetTabBar->count(); i++) {
            if (widgetTabBar->tabText(i) == tabText) {
                found = true;
                break;
            }
        }
    }
    return found;
}

bool MW::isSelectedDockTab(QString tabText)
{

    QTabBar* widgetTabBar = tabifiedBar();
    bool selected = false;
    if (widgetTabBar != nullptr) {
        int idx = widgetTabBar->currentIndex();
        for (int i = 0; i < widgetTabBar->count(); i++) {
            if (widgetTabBar->tabText(i) == tabText) {
                if (i == idx) {
                    selected = true;
                    break;
                }
            }
        }
    }
    return selected;
}

void MW::folderDockVisibilityChange()
{
    if (G::isLogger) G::log("MW::folderDockVisibilityChange");
    if (folderDock->isVisible()) {
        fsTree->scrollToCurrent();
    }
}

void MW::embelDockVisibilityChange()
{
    if (G::isLogger) G::log("MW::embelDockVisibilityChange");
    loupeDisplay();
    if (turnOffEmbellish) embelProperties->doNotEmbellish();
}

void MW::embelDockActivated(QDockWidget *dockWidget)
{
    if (G::isLogger) G::log("MW::embelDockActivated");
//    if (dockWidget->objectName() == "embelDock") embelDisplay();
    // enable the folder dock (first one in tab)
    embelDockTabActivated = true;
    QList<QTabBar*> tabList = findChildren<QTabBar*>();
    QTabBar* widgetTabBar = tabList.at(0);
    widgetTabBar->setCurrentIndex(4);
//    qDebug() << "MW::embelDockActivated" << dockWidget->objectName() << widgetTabBar->currentIndex();

}

void MW::embelTemplateChange(int id)
{
    if (G::isLogger) G::log("MW::embelTemplateChange");
    embelTemplatesActions.at(id)->setChecked(true);
    if (id == 0) {
        embelRunBtn->setVisible(false);
        setRatingBadgeVisibility();
        setShootingInfoVisibility();
    }
    else {
        loupeDisplay();
        embelRunBtn->setVisible(true);
        isRatingBadgeVisible = false;
        thumbView->refreshThumbs();
        gridView->refreshThumbs();
        updateClassification();
        imageView->infoOverlay->setVisible(false);
    }
}

void MW::syncEmbellishMenu()
{
    if (G::isLogger) G::log("MW::syncEmbellishMenu");
    int count = embelProperties->templateList.length();
    for (int i = 0; i < 30; i++) {
        if (i < count) {
            embelTemplatesActions.at(i)->setText(embelProperties->templateList.at(i));
            embelTemplatesActions.at(i)->setVisible(true);
        }
        else {
            embelTemplatesActions.at(i)->setText("Future Template"  + QString::number(i));
            embelTemplatesActions.at(i)->setVisible(false);
        }
    }
}

void MW::setCacheMethod(QString method)
{
/*

*/
    if (G::isLogger) G::log("MW::setCacheMethod");

    cacheMethod = method;
    if (method == "Linear") {
        G::isLinearLoading = true;
        G::metaReadInUse = "Linear metadata and thumbnail loading";
        QString mtrl = "Turns yellow when metadata/icon caching in progress\n" +
                G::metaReadInUse;
        metadataThreadRunningLabel->setToolTip(mtrl);
    }
    else {
        G::isLinearLoading = false;
        #ifdef METAREAD
        G::metaReadInUse = "Concurrent metadata and thumbnail loading";
        #endif
        #ifdef METAREAD2
        G::metaReadInUse = "Concurrent2 multi-threaded metadata and thumbnail loading";
        #endif
        QString mtrl = "Turns red when metadata/icon caching in progress\n" +
                G::metaReadInUse;
        metadataThreadRunningLabel->setToolTip(mtrl);
    }
}

void MW::thriftyCache()
{
/*
    Connected to F10
*/
    if (G::isLogger) G::log("MW::thriftyCache");
    setImageCacheSize("Thrifty");
    setImageCacheParameters();
}

void MW::moderateCache()
{
/*
    Connected to F11
*/
    if (G::isLogger) G::log("MW::moderateCache");
    setImageCacheSize("Moderate");
    setImageCacheParameters();
}

void MW::greedyCache()
{
/*
    Connected to F12
*/
    if (G::isLogger) G::log("MW::greedyCache");
    setImageCacheSize("Greedy");
    setImageCacheParameters();
}

void MW::setImageCacheMinSize(QString size)
{
/*

*/
    if (G::isLogger) G::log("MW::setImageCacheMinSize");
    cacheMinSize = size;
    if (size == "0.5 GB") cacheMinMB = 500;
    else if (size == "1 GB") cacheMinMB = 1000;
    else if (size == "2 GB") cacheMinMB = 2000;
    else if (size == "4 GB") cacheMinMB = 4000;
    else if (size == "6 GB") cacheMinMB = 6000;
    else if (size == "8 GB") cacheMinMB = 8000;
    else if (size == "12 GB") cacheMinMB = 12000;
    else if (size == "16 GB") cacheMinMB = 16000;
    else if (size == "24 GB") cacheMinMB = 24000;
    else if (size == "32 GB") cacheMinMB = 32000;
    else if (size == "48 GB") cacheMinMB = 48000;
    else if (size == "64 GB") cacheMinMB = 64000;
    if (cacheMaxMB < cacheMinMB) cacheMaxMB = cacheMinMB;
}

void MW::setImageCacheSize(QString method)
{
/*

*/
    if (G::isLogger) G::log("MW::setImageCacheSize");
//    qDebug() << "MW::setImageCacheSize" << method;


    // deal with possible deprecated settings
    if (method != "Thrifty" && method != "Moderate" && method != "Greedy")
        method = "Thrifty";

    cacheSizeStrategy = method;

    if (cacheSizeStrategy == "Thrifty") {
        cacheMaxMB = static_cast<int>(G::availableMemoryMB * 0.10);
        cacheMethodBtn->setIcon(QIcon(":/images/icon16/thrifty.png"));
    }
    if (cacheSizeStrategy == "Moderate") {
        cacheMaxMB = static_cast<int>(G::availableMemoryMB * 0.50);
        cacheMethodBtn->setIcon(QIcon(":/images/icon16/moderate.png"));
    }
    if (cacheSizeStrategy == "Greedy") {
        cacheMaxMB = static_cast<int>(G::availableMemoryMB * 0.90);
        cacheMethodBtn->setIcon(QIcon(":/images/icon16/greedy.png"));
    }

    if (cacheMaxMB < cacheMinMB) cacheMaxMB = cacheMinMB;

//    if (cacheMaxMB > 0 && cacheMaxMB < 1000) cacheMaxMB = G::availableMemoryMB;
}

void MW::setImageCacheParameters()
{
/*
    This slot is called from the preferences dialog with changes to the cache
    parameters.  Any visibility changes are executed.
*/
    if (G::isLogger) G::log("MW::setImageCacheParameters");

    int cacheNetMB = cacheMaxMB - static_cast<int>(G::metaCacheMB);
    if (cacheNetMB < cacheMinMB) cacheNetMB = cacheMinMB;

    imageCacheThread->updateImageCacheParam(cacheNetMB, cacheMinMB,
             isShowCacheProgressBar, cacheWtAhead);

    QString fPath = dm->currentFilePath;
//    QString fPath = thumbView->currentIndex().data(G::PathRole).toString();
    // set position in image cache
    if (fPath.length() && G::useImageCache)
        imageCacheThread->setCurrentPosition(fPath, "MW::setImageCacheParameters");

    // cache progress bar
    progressLabel->setVisible(isShowCacheProgressBar);

    // thumbnail cache status indicators
    thumbView->refreshThumbs();
    gridView->refreshThumbs();

    if (G::isNewFolderLoaded) imageCacheThread->cacheSizeChange();
}

void MW::scrollToCurrentRow()
{
/*
    Called after a sort or when thumbs shrink/enlarge. The current image may no longer be
    visible hence need to scroll to the current row.
*/
    if (G::isLogger) G::log("MW::scrollToCurrentRow");
    dm->currentSfRow = dm->sf->mapFromSource(dm->currentDmIdx).row();
    QModelIndex idx = dm->sf->index(dm->currentSfRow, 0);
//    G::wait(100);

    G::ignoreScrollSignal = true;
    if (thumbView->isVisible()) thumbView->scrollToRow(dm->currentSfRow, "MW::scrollToCurrentRow");
    if (gridView->isVisible()) gridView->scrollToRow(dm->currentSfRow, "MW::scrollToCurrentRow");
    if (tableView->isVisible()) tableView->scrollTo(idx,
         QAbstractItemView::ScrollHint::PositionAtCenter);
    G::ignoreScrollSignal = false;

    updateIconRange(dm->currentSfRow, "MW::scrollToCurrentRow");
    metadataCacheThread->scrollChange("MW::scrollToCurrentRow");
}

void MW::showHiddenFiles()
{
    // rgh ??
    if (G::isLogger) G::log("MW::showHiddenFiles");
//    fsTree->setModelFlags();
}

void MW::thumbsEnlarge()
{

    if (G::isLogger) G::log("MW::thumbsEnlarge");
    if (gridView->isVisible()) gridView->justify(IconView::JustifyAction::Enlarge);
    if (thumbView->isVisible())  {
        if (thumbView->isWrapping()) thumbView->justify(IconView::JustifyAction::Enlarge);
        else thumbView->thumbsEnlarge();
    }
//    return;
//    scrollToCurrentRow();
    // may be less icons to cache
    numberIconsVisibleChange();

    // if thumbView visible and zoomed in imageView then may need to redo the zoomFrame
    if (thumbView->isVisible())  {
        QModelIndex idx = thumbView->indexAt(thumbView->mapFromGlobal(QCursor::pos()));
        if (idx.isValid()) {
            thumbView->zoomCursor(idx, "MW::thumbsEnlarge", /*forceUpdate=*/true);
        }
    }
}

void MW::thumbsShrink()
{
    if (G::isLogger) G::log("MW::thumbsShrink");
    if (gridView->isVisible()) gridView->justify(IconView::JustifyAction::Shrink);
    if (thumbView->isVisible()) {
        if (thumbView->isWrapping()) thumbView->justify(IconView::JustifyAction::Shrink);
        else thumbView->thumbsShrink();
    }
    return;
    scrollToCurrentRow();
    // may be more icons to cache
    numberIconsVisibleChange();

    // if thumbView visible and zoomed in imageView then may need to redo the zoomFrame
    if (thumbView->isVisible())  {
        QModelIndex idx = thumbView->indexAt(thumbView->mapFromGlobal(QCursor::pos()));
        if (idx.isValid()) {
            thumbView->zoomCursor(idx, "MW::thumbsShrink", /*forceUpdate=*/true);
        }
    }
}

void MW::addRecentFolder(QString fPath)
{
    if (G::isLogger) G::log("MW::addRecentFolder");
    if (recentFolders->contains(fPath) || fPath == "") return;
    recentFolders->prepend(fPath);
    int n = recentFolders->count();
    for (int i = 0; i < maxRecentFolders; i++) {
        if (i < n) {
            recentFolderActions.at(i)->setText(recentFolders->at(i));
            recentFolderActions.at(i)->setVisible(true);
        }
        else {
            recentFolderActions.at(i)->setText("Future recent folder" + QString::number(i));
            recentFolderActions.at(i)->setVisible(false);
        }
    }
    // if already maxRecentFolders trim excess items
    if (n > maxRecentFolders) {
        for (int i = n; i > maxRecentFolders; --i) {
            recentFolders->removeAt(i - 1);
        }
    }
}

void MW::addIngestHistoryFolder(QString fPath)
{
    if (G::isLogger) G::log("MW::addIngestHistoryFolder");
    // keep track of ingest location if gotoIngestFolder == true
    lastIngestLocation = fPath;

    if (!ingestHistoryFolders->contains(fPath) && fPath != "")
        ingestHistoryFolders->prepend(fPath);
    int count = ingestHistoryFolders->count();
    for (int i = 0; i < maxIngestHistoryFolders; i++) {
        if (i < count) {
            ingestHistoryFolderActions.at(i)->setText(ingestHistoryFolders->at(i));
            ingestHistoryFolderActions.at(i)->setVisible(true);
        }
        else {
            ingestHistoryFolderActions.at(i)->setText("Future ingest history folder" + QString::number(i));
            ingestHistoryFolderActions.at(i)->setVisible(false);
        }
    }
}

void MW::invokeRecentFolder(QAction *recentFolderActions)
{
    if (G::isLogger) G::log("MW::invokeRecentFolder");
    QString dirPath = recentFolderActions->text();
    fsTree->select(dirPath);
//    selectionChange();
    folderSelectionChange();
}

void MW::invokeIngestHistoryFolder(QAction *ingestHistoryFolderActions)
{
    if (G::isLogger) G::log("MW::invokeIngestHistoryFolder");
    QString dirPath = ingestHistoryFolderActions->text();
    fsTree->select(dirPath);
//    selectionChange();
    folderSelectionChange();
//    revealInFileBrowser(dirPath);
}

void MW::about()
{
    if (G::isLogger) G::log("MW::about");
    QString qtVersion = QT_VERSION_STR;
    qtVersion.prepend("Qt: ");
    aboutDlg = new AboutDlg(this, version, qtVersion);
    aboutDlg->exec();
}

void MW::allPreferences()
{
    if (G::isLogger) G::log("MW::allPreferences");
    preferences();
}

void MW::infoViewPreferences()
{
    if (G::isLogger) G::log("MW::infoViewPreferences");
    preferences("MetadataPanelHeader");
}

void MW::cachePreferences()
{
    if (G::isLogger) G::log("MW::cachePreferences");
    preferences("CacheHeader");
}

void MW::preferences(QString text)
{
/*

*/
    if (G::isLogger) G::log("MW::preferences");
    if (preferencesDlg == nullptr) {
        pref = new Preferences(this);
        if (text != "") pref->expandBranch(text);
        preferencesDlg = new PreferencesDlg(this, isSoloPrefDlg, pref, css);
    }
    // non-modal
    preferencesDlg->setWindowModality(Qt::NonModal);
    preferencesDlg->show();
    // modal
    /*
    preferencesDlg->exec();
    delete pref;
    delete preferencesDlg;
    //*/

    /* Create a preferences tree as a docking panel:
    propertiesDock = new DockWidget(tr("  Preferencess  "), this);
    propertiesDock->setObjectName("Preferences");
    propertiesDock->setWidget(pref);
    propertiesDock->setFloating(true);
    propertiesDock->setGeometry(2000,600,400,800);
    propertiesDock->setVisible(true);
    propertiesDock->raise();
    return;
    //*/
}

void MW::setShowImageCount()
{
    if (G::isLogger) G::log("MW::setShowImageCount");
    if (!fsTree->isVisible()) {
        G::popUp->showPopup("Show image count is only available when the Folders Panel is visible",
              1500);
    }
    bool isShow = showImageCountAction->isChecked();
    fsTree->setShowImageCount(isShow);
    fsTree->resizeColumns();
    fsTree->repaint();
    if (isShow) fsTree->fsModel->fetchMore(fsTree->rootIndex());
}

void MW::setFontSize(int fontPixelSize)
{
/*
    The font size and container sizes are updated for all objects in Winnow.  For most objects
    updating the font size in the QStyleSheet is sufficient.  For items in list and tree views
    the sizehint needs to be triggered, either by refreshing all the values or calling
    scheduleDelayedItemsLayout().
*/
    if (G::isLogger) G::log("MW::setFontSize");
    G::fontSize = fontPixelSize;
    G::strFontSize = QString::number(fontPixelSize);
    widgetCSS.fontSize = fontPixelSize;
//    emit widgetCSS.fontSizeChange(fontPixelSize);
    css = widgetCSS.css();
    G::css = css;
    setStyleSheet(css);

    if (G::useInfoView) infoView->refreshLayout();                   // triggers sizehint!
//    infoView->updateInfo(currentRow);                           // triggers sizehint!
    bookmarks->setStyleSheet(css);
    fsTree->setStyleSheet(css);
    filters->setStyleSheet(css);
    tableView->setStyleSheet(css);
    statusLabel->setStyleSheet(css);
    folderTitleBar->setStyle();
    favTitleBar->setStyle();
    filterTitleBar->setStyle();
    metaTitleBar->setStyle();
    embelTitleBar->setStyle();
    embelProperties->fontSizeChanged(fontPixelSize);
    pref->fontSizeChanged(fontPixelSize);
}

void MW::setBackgroundShade(int shade)
{
    if (G::isLogger) G::log("MW::setBackgroundShade");
    G::backgroundShade = shade;
    int a = shade + 5;
    int b = shade - 15;
//    css = "QWidget {font-size: " + G::fontSize + "px;}" + cssBase;
    widgetCSS.widgetBackgroundColor = QColor(shade,shade,shade);
    css = widgetCSS.css();
    G::css = css;
    this->setStyleSheet(css);

    if (G::useInfoView) {
        infoView->updateInfo(dm->currentSfRow);                           // triggers sizehint!
        infoView->verticalScrollBar()->setStyleSheet(css);          // triggers sizehint!
    }
    bookmarks->setStyleSheet(css);
    bookmarks->verticalScrollBar()->setStyleSheet(css);
    fsTree->setStyleSheet(css);
    fsTree->verticalScrollBar()->setStyleSheet(css);
    filters->setStyleSheet(css);
    filters->verticalScrollBar()->setStyleSheet(css);
    filters->setCategoryBackground(a, b);
//    if (G::useInfoView) infoView->setStyleSheet(css);
    imageView->setBackgroundColor(widgetCSS.widgetBackgroundColor);
    thumbView->setStyleSheet(css);
    thumbView->horizontalScrollBar()->setStyleSheet(css);
    thumbView->verticalScrollBar()->setStyleSheet(css);
    gridView->setStyleSheet(css);
    tableView->setStyleSheet(css);
    gridView->verticalScrollBar()->setStyleSheet(css);
    messageView->setStyleSheet(css);
    welcome->setStyleSheet(css);
    cacheProgressBar->setBackgroundColor(widgetCSS.widgetBackgroundColor);
    folderTitleBar->setStyle();
    favTitleBar->setStyle();
    filterTitleBar->setStyle();
    metaTitleBar->setStyle();
    statusBar()->setStyleSheet(css);
    statusBar()->setStyleSheet("QStatusBar::item { border: none; };");
}

void MW::setInfoFontSize()
{
    if (G::isLogger) G::log("MW::setInfoFontSize");
    /* imageView->infoOverlayFontSize already defined in preferences - just call
       so can redraw  */
    imageView->setShootingInfo(imageView->shootingInfo);
}

void MW::setClassificationBadgeImageDiam(int d)
{
    if (G::isLogger) G::log("MW::setClassificationBadgeImageDiam");
    classificationBadgeInImageDiameter = d;
    imageView->setClassificationBadgeImageDiam(d);
}

void MW::setClassificationBadgeSizeFactor(int d)
{
    if (G::isLogger) G::log("MW::setClassificationBadgeThumbDiam");
    qDebug() << "MW::setClassificationBadgeThumbDiam";
    classificationBadgeSizeFactor = d;
    thumbView->badgeSize = d;
    gridView->badgeSize = d;
    thumbView->setThumbParameters();
    gridView->setThumbParameters();
}

void MW::setIconNumberSize(int d)
{
    if (G::isLogger) G::log("MW::setIconNumberSize");
    qDebug() << "MW::setIconNumberSize";
    iconNumberSize = d;
    thumbView->iconNumberSize = d;
    gridView->iconNumberSize = d;
    thumbView->setThumbParameters();
    gridView->setThumbParameters();
}

void MW::setPrefPage(int page)
{
    if (G::isLogger) G::log("MW::setPrefPage");
    lastPrefPage = page;
}

void MW::updateDisplayResolution()
{
    if (G::isLogger) G::log("MW::updateDisplayResolution");
    QString monitorScale = QString::number(G::actDevicePixelRatio * 100) + "%";
    QString dimensions = QString::number(G::displayPhysicalHorizontalPixels) + "x"
            + QString::number(G::displayPhysicalVerticalPixels)
            + " @ " + monitorScale;
    QString toolTip = dimensions + " (Monitor is scaled to " + monitorScale + ")";
    if (!G::useInfoView) return;
    QStandardItemModel *k = infoView->ok;
    k->setData(k->index(infoView->MonitorRow, 1, infoView->statusInfoIdx), dimensions);
    k->setData(k->index(infoView->MonitorRow, 1, infoView->statusInfoIdx), toolTip, Qt::ToolTipRole);
}

void MW::setDisplayResolution()
{
/*
    This is triggered by the mainwindow move event at startup, when the operating system
    display scale is changed and when the app window is dragged to another monitor. The loupe
    view always shows native pixel resolution (one image pixel = one physical monitor pixel),
    therefore the zoom has to be factored by the device pixel ratio.

    However, on Mac the device pixel ratio is arbitrary, mostly = 2.0 no matter which display
    scaling is selected, so there are two ratios defined here:

    G::actDevicePixelRatio - the actual ratio from actual to vertual pixels
    G::sysDevicePixelRatio - the system reported device pixel ratio
*/
    if (G::isLogger) G::log("MW::setDisplayResolution");

    bool monitorChanged = false;
    bool devicePixelRatioChanged = false;

    // ignore until show event
    if (!isVisible()) {
        return;
    }

    // Screen info
    QPoint loc = centralWidget->window()->geometry().center();
    QScreen *screen = qApp->screenAt(loc);
    if (screen == nullptr) return;
    monitorChanged = screen->name() != prevScreenName;

    /*
    qDebug() << "MW::setDisplayResolution" << "1"
             << "G::isInitializing  =" << G::isInitializing
             << "isVisible()  =" << isVisible()
             << "prevDevicePixelRatio =" << prevDevicePixelRatio
             << "screen->name() =" << screen->name()
             << "prevScreenName =" << prevScreenName
             << "monitorChanged =" << monitorChanged
             << "devicePixelRatioChanged =" << devicePixelRatioChanged;
//             */
    prevScreenName = screen->name();

    // Device Pixel Ratios
    G::sysDevicePixelRatio = screen->devicePixelRatio();
    #ifdef Q_OS_WIN
    G::actDevicePixelRatio = screen->devicePixelRatio();
    #endif
    #ifdef Q_OS_MAC
    G::actDevicePixelRatio = macActualDevicePixelRatio(loc, screen);
    #endif
    devicePixelRatioChanged = !qFuzzyCompare(G::actDevicePixelRatio, prevDevicePixelRatio);
    /*
    qDebug() << "MW::setDisplayResolution" << "2"
             << "G::isInitializing  =" << G::isInitializing
             << "isVisible()  =" << isVisible()
             << "prevDevicePixelRatio =" << prevDevicePixelRatio
             << "screen->name() =" << screen->name()
             << "prevScreenName =" << prevScreenName
             << "monitorChanged =" << monitorChanged
             << "devicePixelRatioChanged =" << devicePixelRatioChanged;
//             */
    prevDevicePixelRatio = G::actDevicePixelRatio;

    if (!monitorChanged && !devicePixelRatioChanged) return;

    // Device Pixel Ratio or Monitor change has occurred (default set in initialize())
    G::dpi = screen->logicalDotsPerInch();
    G::ptToPx = G::dpi / 72;
    /*
    qDebug() << "MW::setDisplayResolution"
             << "G::dpi =" << G::dpi
             << "G::ptToPx =" << G::ptToPx
                ; //*/
    G::displayVirtualHorizontalPixels = screen->geometry().width();
    G::displayVirtualVerticalPixels = screen->geometry().height();
    G::displayPhysicalHorizontalPixels = screen->geometry().width() * G::actDevicePixelRatio;
    G::displayPhysicalVerticalPixels = screen->geometry().height() * G::actDevicePixelRatio;

    /*
    double physicalWidth = screen->physicalSize().width();
    double dpmm = G::displayPhysicalHorizontalPixels * 1.0 / physicalWidth ;
    qDebug() << "MW::setDisplayResolution"
             << "G::actDevicePixelRatio =" << G::actDevicePixelRatio
             << "screen->actDevicePixelRatio() =" << screen->devicePixelRatio()
             << "VirtualHorPixels =" << G::displayVirtualHorizontalPixels
             << "PhysicalHorPixels =" << G::displayPhysicalHorizontalPixels
//             << "screen->physicalSize() =" << screen->physicalSize()
             << "px per mm =" << dpmm
                ;
//    */

    if (devicePixelRatioChanged && !G::isInitializing) {
        // refresh loupe / compare views to new scale
        if (G::mode == "Loupe") {
            // reload to force complete refresh
            imageView->loadImage(dm->currentFilePath, "DevicePixelRatioChange");
        }
        if (G::mode == "Compare") compareImages->zoomTo(imageView->zoom/* / G::actDevicePixelRatio*/);
    }

    // if monitor has not changed then only scale change, return
    if (!monitorChanged) return;

    // monitor has changed

    // resize if winnow window too big for new screen
    int w = this->geometry().width();
    int h = this->geometry().height();
    double fitW = w * 1.0 / screen->geometry().width();
    double fitH = h * 1.0 / screen->geometry().height();
    /*
    qDebug() << "MW::setDisplayResolution" << "MONITOR HAS CHANGED"
             << "w =" << w
             << "ScreenW =" << screen->geometry().width()
             << "fitW =" << fitW
             << "h =" << h
             << "ScreenH=" << screen->geometry().height()
             << "fitH =" << fitH
                ;
//                    */
    // does winnow fit in new screen?
    if (fitW > 1.0 || fitH > 1.0) {
        // does not fit, does width or height require the larger adjustment?
        if (fitW < fitH) {
            // width is larger adjustment
            w = static_cast<int>(w * 0.75 / fitW);
            h = static_cast<int>(h * 0.75 / fitW);
        }
        else {
            // height is larger adjustment
            w = static_cast<int>(w * 0.75 / fitH);
            h = static_cast<int>(h * 0.75 / fitH);
        }
//        qDebug() << "MW::setDisplayResolution" << "RESIZE TO:" << w << h;
        resize(w, h);
    }

    // color manage for new monitor
    getDisplayProfile();

    cachePreviewWidth = G::displayPhysicalHorizontalPixels;
    cachePreviewHeight = G::displayPhysicalVerticalPixels;
    /*
    qDebug() << "MW::setDisplayResolution"
             << "screen->name() =" << screen->name()
             << "G::actDevicePixelRatio =" << G::actDevicePixelRatio
             << "loc =" << loc
             << "G::dpi =" << G::dpi
             << "G::ptToPx =" << G::ptToPx
             << "G::displayVirtualHorizontalPixels =" << G::displayVirtualHorizontalPixels
             << "G::displayVirtualVerticalPixels =" << G::displayVirtualVerticalPixels
                ;
//                */

    screen = nullptr;
}

void MW::getDisplayProfile()
{
/*
    This is required for color management.  It is called after the show event when the
    progam is opening and when the main window is moved to a different screen.
*/
    if (G::isLogger) G::log("MW::getDisplayProfile");
    #ifdef Q_OS_WIN
    if (G::winScreenHash.contains(screen()->name()))
        G::winOutProfilePath = "C:/Windows/System32/spool/drivers/color/" +
            G::winScreenHash[screen()->name()].profile;
    ICC::setOutProfile();
    #endif
    #ifdef Q_OS_MAC
    G::winOutProfilePath = Mac::getDisplayProfileURL();
    ICC::setOutProfile();
    #endif
}



double MW::macActualDevicePixelRatio(QPoint loc, QScreen *screen)
{
/*
    Apple makes it hard to get the display native pixel resolution, which is necessary
    to determine the true device pixel ratio to ensure images at 100% are 1:1 image and
    display pixels.
*/
    if (G::isLogger) G::log("MW::macActualDevicePixelRatio");
    #ifdef Q_OS_MAC
    // get displayID for monitor at point
    const int maxDisplays = 64;                     // 64 should be enough for any system
    CGDisplayCount displayCount;                    // Total number of display IDs
    CGDirectDisplayID displayIDs[maxDisplays];      // Array of display IDs
    CGPoint point = loc.toCGPoint();
    CGGetDisplaysWithPoint(point, maxDisplays, displayIDs, &displayCount);
    auto displayID = displayIDs[0];
    if (displayCount != 1) displayID = CGMainDisplayID();

    // get list of all display modes for the monitor
    auto modes = CGDisplayCopyAllDisplayModes(displayID, nullptr);
    auto count = CFArrayGetCount(modes);
    CGDisplayModeRef mode;

    // start with virtual pixel width from QScreen (does not know actual width in pixels)
    int screenW = screen->geometry().width();
    int screenH = screen->geometry().height();

    // the native resolution is the largest display mode
    for (long c = count; c--;) {
        mode = static_cast<CGDisplayModeRef>(const_cast<void *>(CFArrayGetValueAtIndex(modes, c)));
        int w = static_cast<int>(CGDisplayModeGetWidth(mode));
        int h = static_cast<int>(CGDisplayModeGetHeight(mode));
        if (w > screenW) screenW = w;
        if (h > screenH) screenH = h;
    }

    // The device pixel ratio
    return screenW * 1.0 / screen->geometry().width();
    #endif
    // dummy return to satisfy compiler on PC
    return 0;

  /*  MacOS Screen information
#if defined(Q_OS_MAC)
       int screenWidth = CGDisplayPixelsWide(CGMainDisplayID());
       qDebug() << "screenWidth" << screenWidth << QPaintDevice::actDevicePixelRatio();
        float bSF = QtMac::macBackingScaleFactor();
        qDebug() << G::t.restart() << "\t" << "QtMac::BackingScaleFactor()" << bSF;
#endif

        qDebug() << G::t.restart() << "\t" << "QGuiApplication::primaryScreen()->actDevicePixelRatio()"
                << QGuiApplication::primaryScreen()->actDevicePixelRatio();
        qreal dpr = QGuiApplication::primaryScreen()->actDevicePixelRatio();

        QRect rect = QGuiApplication::primaryScreen()->geometry();
        qreal screenMax = qMax(rect.width(), rect.height());

        G::actDevicePixelRatio = 1;
        G::actDevicePixelRatio = 2880 / screenMax;

        int realScreenMax = QGuiApplication::primaryScreen()->physicalSize().width();
        qreal logicalDpi = QGuiApplication::primaryScreen()->logicalDotsPerInch();
        qreal physicalDpi = QGuiApplication::primaryScreen()->physicalDotsPerInch();
        QSizeF physicalSize = QGuiApplication::primaryScreen()->physicalSize();

        qDebug() << G::t.restart() << "\t" << "\nQGuiApplication::primaryScreen()->geometry()" << QGuiApplication::primaryScreen()->geometry().width()
                 << "\nQGuiApplication::primaryScreen()->physicalSize()" << QGuiApplication::primaryScreen()->physicalSize().width()
                 << "\ndevicePixelRatio" << dpr
                 << "\nlogicalDpi" << QGuiApplication::primaryScreen()->logicalDotsPerInch()
                 << "\nphysicalDpi" << QGuiApplication::primaryScreen()->physicalDotsPerInch()
                 << "\nphysicalSize" << QGuiApplication::primaryScreen()->physicalSize()
                 << "\nQApplication::desktop()->availableGeometry(this)"<< QApplication::desktop()->availableGeometry(this)
                 << "\n";
//                 */
}

void MW::escapeFullScreen()
{
    if (G::isLogger) G::log("MW::escapeFullScreen");
    fullScreenAction->setChecked(false);
    toggleFullScreen();
}

void MW::toggleFullScreen()
{
    if (G::isLogger) G::log("MW::toggleFullScreen");
    if (fullScreenAction->isChecked())
    {
        snapshotWorkspace(ws);
        isNormalScreen = false;
        showFullScreen();
        folderDockVisibleAction->setChecked(fullScreenDocks.isFolders);
        folderDock->setVisible(fullScreenDocks.isFolders);
        favDockVisibleAction->setChecked(fullScreenDocks.isFavs);
        favDock->setVisible(fullScreenDocks.isFavs);
        filterDockVisibleAction->setChecked(fullScreenDocks.isFilters);
        filterDock->setVisible(fullScreenDocks.isFilters);
        if (G::useInfoView) {
            metadataDockVisibleAction->setChecked(fullScreenDocks.isMetadata);
            metadataDock->setVisible(fullScreenDocks.isMetadata);
        }
        embelDockVisibleAction->setChecked(fullScreenDocks.isMetadata);
        embelDock->setVisible(fullScreenDocks.isMetadata);
        thumbDockVisibleAction->setChecked(fullScreenDocks.isThumbs);
        thumbDock->setVisible(fullScreenDocks.isThumbs);
        menuBarVisibleAction->setChecked(false);
        setMenuBarVisibility();
        statusBarVisibleAction->setChecked(fullScreenDocks.isStatusBar);
        setStatusBarVisibility();
    }
    else
    {
        isNormalScreen = true;
        showNormal();
        invokeWorkspace(ws);
    }
}

void MW::selectAllThumbs()
{
    if (G::isLogger) G::log("MW::selectAllThumbs");
    sel->all();
}

void MW::toggleZoomDlg()
{
/*
    This function provides a dialog to change scale and to set the toggleZoom value,
    which is the amount of zoom to toggle with zoomToFit scale. The user can zoom to 100%
    (for example) with a click of the mouse, and with another click, return to the
    zoomToFit scale. Here the user can set the amount of zoom when toggled.

    The dialog is non-modal and floats at the bottom of the central widget. Adjustments
    are made when the main window resizes or is moved or when a different workspace is
    invoked. This only applies when a mode that can be zoomed is visible, so table and
    grid are not applicable.

    When the zoom dialog is created, zoomDlg->show makes the dialog visible and also
    gives it the focus, but the design is for the zoomDlg to only have focus when a
    mouseover occurs. The focus is set to MW when the zoomDlg leaveEvent fires and after
    zoomDlg->show.

    NOTE: the dialog window flag is Qt::WindowStaysOnTopHint. When The app focus changes
    to another app, the zoom dialog is hidden so it does not float on top of other apps
    (this is triggered in the slot MW::appStateChange). The windows flag
    Qt::WindowStaysOnTopHint is not changed as this automatically hides the window - it
    is easier to just hide it. The prior state of ZoomDlg is held in isZoomDlgVisible.

    When the zoom is changed this is signalled to ImageView and CompareImages, which in
    turn make the scale changes to the image. Conversely, changes in scale originating
    from toggleZoom mouse clicking in ImageView or CompareView, or scale changes
    originating from the zoomInAction and zoomOutAction are signaled and updated here.
    This can cause a circular message, which is prevented by variance checking. If the
    zoom factor has not changed more than can be accounted for in int/qreal conversions
    then the signal is not propagated.

*/
    if (G::isLogger) G::log("MW::toggleZoomDlg");
    // toggle zoomDlg (if open then close)
    if (isZoomDlgVisible) {
        if (zoomDlg->isVisible()) {
            isZoomDlgVisible = false;
            zoomDlg->close();
            return;
        }
    }

    // only makes sense to zoom when in loupe or compare view
    if (G::mode == "Table" || G::mode == "Grid") {
        G::popUp->showPopup("The zoom dialog is only available in loupe view", 2000);
        return;
    }

    // the dialog positions itself relative to the main window and central widget.
    QRect a = this->geometry();
    QRect c = centralWidget->geometry();
    zoomDlg = new ZoomDlg(this, imageView->zoom, a, c);
    isZoomDlgVisible = true;

    // update the imageView and compareView classes if there is a zoom change
    connect(zoomDlg, &ZoomDlg::zoom, imageView, &ImageView::zoomTo);
    connect(zoomDlg, &ZoomDlg::zoom, compareImages, &CompareImages::zoomTo);

    // update the imageView and compareView classes if there is a toggleZoomValue change
    connect(zoomDlg, SIGNAL(updateToggleZoom(qreal)), imageView, SLOT(updateToggleZoom(qreal)));
    connect(zoomDlg, SIGNAL(updateToggleZoom(qreal)), compareImages, SLOT(updateToggleZoom(qreal)));

    // if zoom dialog signals to close (Return or Z shortcut) then update using this function
    connect(zoomDlg, &ZoomDlg::closeZoom, this, &MW::toggleZoomDlg);

    // if zoom change in parent send it to the zoom dialog
    connect(imageView, &ImageView::zoomChange, zoomDlg, &ZoomDlg::zoomChange);
    connect(compareImages, &CompareImages::zoomChange, zoomDlg, &ZoomDlg::zoomChange);

    // if main window resized then re-position zoom dialog
    connect(this, SIGNAL(resizeMW(QRect,QRect)), zoomDlg, SLOT(positionWindow(QRect,QRect)));

    // if main window loses focus, hide ZoomDlg because stayOnTop shows over other apps
    QGuiApplication *app = qobject_cast<QGuiApplication *>(QCoreApplication::instance());
    connect(app, &QGuiApplication::applicationStateChanged, this, &MW::appStateChange);

    // if view change other than loupe then close zoomDlg
    connect(this, &MW::closeZoomDlg, zoomDlg, &ZoomDlg::closeZoomDlg);

    // reset focus to main window if the mouse cursor leaves the zoom dialog
    connect(zoomDlg, &ZoomDlg::leaveZoom, this, &MW::resetFocus);

    // use show() so dialog will be non-modal
    zoomDlg->show();

    // reset the focus to the main window
    resetFocus();
}

void MW::zoomIn()
{
    if (G::isLogger) G::log("MW::zoomIn");
    if (asLoupeAction) imageView->zoomIn();
    if (asCompareAction) compareImages->zoomIn();
    // if thumbView visible and zoom change in imageView then may need to redo the zoomFrame
    QModelIndex idx = thumbView->indexAt(thumbView->mapFromGlobal(QCursor::pos()));
    if (idx.isValid()) {
        thumbView->zoomCursor(idx, "MW::zoomIn", /*forceUpdate=*/true);
    }
}

void MW::zoomOut()
{
    if (G::isLogger) G::log("MW::zoomOut");
    if (asLoupeAction) imageView->zoomOut();
    if (asCompareAction) compareImages->zoomOut();
    // if thumbView visible and zoom change in imageView then may need to redo the zoomFrame
    QModelIndex idx = thumbView->indexAt(thumbView->mapFromGlobal(QCursor::pos()));
    if (idx.isValid()) {
        thumbView->zoomCursor(idx, "MW::zoomOut", /*forceUpdate=*/true);
    }
}

void MW::zoomToFit()
{
    if (G::isLogger) G::log("MW::zoomToFit");
    if (asLoupeAction) imageView->zoomToFit();
    if (asCompareAction) compareImages->zoomToFit();
}

void MW::zoomToggle()
{
    if (G::isLogger) G::log("MW::zoomToggle");
    if (asLoupeAction) imageView->zoomToggle();
    if (asCompareAction) compareImages->zoomToggle();
}

void MW::rotateLeft()
{
    if (G::isLogger) G::log("MW::rotateLeft");
    setRotation(270);
}

void MW::rotateRight()
{
    if (G::isLogger) G::log("MW::rotateRight");
    setRotation(90);
}

void MW::setRotation(int degrees)
{
/*
    Rotate all selected thumbnails by the specified degrees. 90 = rotate right and
    270 = rotate left.

    Rotate all selected cached full size images by the specified degrees.

    When images are added to the image cache (ImageCache) they are rotated by the
    metadata orientation + the edited rotation.  Newly cached images are always
    rotation up-to-date.

    When there is a rotation action (rotateLeft or rotateRight) the current
    rotation amount (in degrees) is updated in the datamodel.

    The rotation is updated in the image file EXIF using exifTool in separate threads.
*/
    if (G::isLogger) G::log("MW::setRotation");
    qDebug() << "MW::setRotation degrees =" << degrees;

    // rotate current loupe view image
    imageView->rotateImage(degrees);

    // iterate selection
    QModelIndexList selection = dm->selectionModel->selectedRows();
    for (int i = 0; i < selection.count(); ++i) {
        // update rotation amount in the data model
        int row = selection.at(i).row();
        QModelIndex orientationIdx = dm->sf->index(row, G::OrientationColumn);
        int orientation = orientationIdx.data(Qt::EditRole).toInt();
        int prevRotation = 0;
        switch (orientation) {
        case 6: prevRotation = 90;  break;
        case 3: prevRotation = 180; break;
        case 8: prevRotation = 270; break;
        }

        int newRotation = prevRotation + degrees;
        if (newRotation >= 360) newRotation = newRotation - 360;
        int newOrientation = 0;
        switch (newRotation) {
        case 90:  newOrientation = 6; break;
        case 180: newOrientation = 3; break;
        case 270: newOrientation = 8; break;
        }

        emit setValueSf(orientationIdx, newOrientation, dm->instance,
                        "MW::setRotation", Qt::EditRole);

        // rotate thumbnail(s)
        QTransform trans;
        trans.rotate(degrees);
        QModelIndex thumbIdx = dm->sf->index(row, G::PathColumn);
        QStandardItem *item = new QStandardItem;
        item = dm->itemFromIndex(dm->sf->mapToSource(thumbIdx));
        QPixmap pm = item->icon().pixmap(G::maxIconSize, G::maxIconSize);
        pm = pm.transformed(QTransform().rotate(degrees));
        item->setIcon(pm);

        // rotate selected cached full size images
        QString fPath = thumbIdx.data(G::PathRole).toString();
        QImage image;
        if (icd->imCache.find(fPath, image)) {
            image = image.transformed(QTransform().rotate(degrees), Qt::SmoothTransformation);
            icd->imCache.insert(fPath, image);
        }

        // update exif in image
        QString orient;
        switch (newRotation) {
            case 0:   orient = "1"; break;
            case 90:  orient = "6"; break;
            case 180: orient = "3"; break;
            case 270: orient = "8"; break;
        }
        if (orient.length() && G::modifySourceFiles) {
            // note that Metadata::writeOrientation must be static!
            QtConcurrent::run(&Metadata::writeOrientation, fPath, orient);
        }
    }

    /* release 1.32 version
    if (G::isLogger) G::log(__FUNCTION__);
    // rotate current loupe view image
    imageView->rotateImage(degrees);

    // iterate selection
    QModelIndexList selection = dm->selectionModel->selectedRows();
    for (int i = 0; i < selection.count(); ++i) {
        // update rotation amount in the data model
        int row = selection.at(i).row();
        QModelIndex orientationIdx = dm->sf->index(row, G::OrientationColumn);
        int orientation = orientationIdx.data(Qt::EditRole).toInt();
        int prevRotation = 0;
        switch (orientation) {
        case 6: prevRotation = 90;  break;
        case 3: prevRotation = 180; break;
        case 8: prevRotation = 270; break;
        }

        int newRotation = prevRotation + degrees;
        if (newRotation >= 360) newRotation = newRotation - 360;
        int newOrientation = 0;
        switch (newRotation) {
        case 90:  newOrientation = 6; break;
        case 180: newOrientation = 3; break;
        case 270: newOrientation = 8; break;
        }

        dm->sf->setData(orientationIdx, newOrientation);

        // rotate thumbnail(s)
        QTransform trans;
        trans.rotate(degrees);
        QModelIndex thumbIdx = dm->sf->index(row, G::PathColumn);
        QString fPath = thumbIdx.data(G::PathRole).toString();
        QStandardItem *item = new QStandardItem;
        item = dm->itemFromIndex(dm->sf->mapToSource(thumbIdx));
        QPixmap pm = item->icon().pixmap(G::maxIconSize, G::maxIconSize);
        pm = pm.transformed(trans, Qt::SmoothTransformation);
        item->setIcon(pm);
        thumbView->refreshThumbs();
        QApplication::processEvents();

        // rotate selected cached full size images
        QImage image;
        if (icd->imCache.find(fPath, image)) {
            image = image.transformed(trans, Qt::SmoothTransformation);
            icd->imCache.insert(fPath, image);
        }

        // update exif in image
        QString orient;
        switch (newRotation) {
        case 0:   orient = "1"; break;
        case 90:  orient = "6"; break;
        case 180: orient = "3"; break;
        case 270: orient = "8"; break;
        }
        if (orient.length() && G::modifySourceFiles) {
            // note that Metadata::writeOrientation must be static!
            QtConcurrent::run(&Metadata::writeOrientation, fPath, orient);
        }
    }
    //*/
}

bool MW::isValidPath(QString &path)
{
    if (G::isLogger) G::log("MW::isValidPath");
    QDir checkPath(path);
    if (!checkPath.exists() || !checkPath.isReadable()) {
        return false;
    }
    return true;
}

void MW::removeBookmark()
{
    if (G::isLogger) G::log("MW::removeBookmark");
    if (QApplication::focusWidget() == bookmarks) {
        bookmarks->removeBookmark();
        return;
    }
}

void MW::refreshBookmarks()
{
/*
    This is run from the bookmarks (fav) panel context menu to update the image count for each
    bookmark folder.
*/
    if (G::isLogger) G::log("MW::refreshBookmarks");
    bookmarks->count();
}

void MW::loadShortcuts(bool defaultShortcuts)
{
    if (G::isLogger) G::log("MW::loadShortcuts");
    // Add customizable key shortcut actions
    actionKeys[fullScreenAction->objectName()] = fullScreenAction;
    actionKeys[escapeFullScreenAction->objectName()] = escapeFullScreenAction;
    actionKeys[prefAction->objectName()] = prefAction;
    actionKeys[exitAction->objectName()] = exitAction;
    actionKeys[thumbsEnlargeAction->objectName()] = thumbsEnlargeAction;
    actionKeys[thumbsShrinkAction->objectName()] = thumbsShrinkAction;
//    actionKeys[cutAction->objectName()] = cutAction;
//    actionKeys[copyAction->objectName()] = copyAction;
    actionKeys[keyRightAction->objectName()] = keyRightAction;
    actionKeys[keyLeftAction->objectName()] = keyLeftAction;
    actionKeys[keyDownAction->objectName()] = keyDownAction;
    actionKeys[keyUpAction->objectName()] = keyUpAction;
//    actionKeys[keepTransformAct->objectName()] = keepTransformAct;
//    actionKeys[keepZoomAct->objectName()] = keepZoomAct;
//    actionKeys[copyImageAction->objectName()] = copyImageAction;
//    actionKeys[pasteImageAction->objectName()] = pasteImageAction;
//    actionKeys[refreshAction->objectName()] = refreshAction;
//    actionKeys[pasteAction->objectName()] = pasteAction;
    actionKeys[pickAction->objectName()] = pickAction;
    actionKeys[filterPickAction->objectName()] = filterPickAction;
    actionKeys[ingestAction->objectName()] = ingestAction;
    actionKeys[reportMetadataAction->objectName()] = reportMetadataAction;
    actionKeys[slideShowAction->objectName()] = slideShowAction;
    actionKeys[keyHomeAction->objectName()] = keyHomeAction;
    actionKeys[keyEndAction->objectName()] = keyEndAction;
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
    actionKeys[ratingBadgeVisibleAction->objectName()] = ratingBadgeVisibleAction;
    actionKeys[iconNumberVisibleAction->objectName()] = iconNumberVisibleAction;
    actionKeys[infoVisibleAction->objectName()] = infoVisibleAction;
//    actionKeys[toggleAllDocksAct->objectName()] = toggleAllDocksAct;
    actionKeys[folderDockVisibleAction->objectName()] = folderDockVisibleAction;
    actionKeys[favDockVisibleAction->objectName()] = favDockVisibleAction;
    actionKeys[filterDockVisibleAction->objectName()] = filterDockVisibleAction;
    actionKeys[metadataDockVisibleAction->objectName()] = metadataDockVisibleAction;
    actionKeys[thumbDockVisibleAction->objectName()] = thumbDockVisibleAction;
//    actionKeys[windowTitleBarVisibleAction->objectName()] = windowTitleBarVisibleAction;
    actionKeys[menuBarVisibleAction->objectName()] = menuBarVisibleAction;
    actionKeys[statusBarVisibleAction->objectName()] = statusBarVisibleAction;
//    actionKeys[toggleIconsListAction->objectName()] = toggleIconsListAction;
//    actionKeys[allDocksLockAction->objectName()] = allDocksLockAction;

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
        //    formats to set shortcut
        //    keyRightAction->setShortcut(QKeySequence("Right"));
        //    keyRightAction->setShortcut((Qt::Key_Right);

        //        cutAction->setShortcut(QKeySequence("Ctrl+X"));
        //        copyAction->setShortcut(QKeySequence("Ctrl+C"));
        //        copyImageAction->setShortcut(QKeySequence("Ctrl+Shift+C"));
        //        pasteImageAction->setShortcut(QKeySequence("Ctrl+Shift+V"));
        //        refreshAction->setShortcut(QKeySequence("Ctrl+F5"));
        //        pasteAction->setShortcut(QKeySequence("Ctrl+V"));

        // File
        openAction->setShortcut(QKeySequence("O"));
        refreshCurrentAction->setShortcut(QKeySequence("Alt+F5"));
        openUsbAction->setShortcut(QKeySequence("Ctrl+O"));
        ingestAction->setShortcut(QKeySequence("Q"));
        showImageCountAction->setShortcut(QKeySequence("\\"));
        combineRawJpgAction->setShortcut(QKeySequence("Alt+J"));
        subFoldersAction->setShortcut(QKeySequence("B"));
        revealFileAction->setShortcut(QKeySequence("Ctrl+R"));
        refreshFoldersAction->setShortcut(QKeySequence("F5"));
        collapseFoldersAction->setShortcut(QKeySequence("Alt+C"));
        reportMetadataAction->setShortcut(QKeySequence("Ctrl+Shift+Alt+M"));
        diagnosticsCurrentAction->setShortcut(QKeySequence("Ctrl+Shift+Alt+D"));
        exitAction->setShortcut(QKeySequence("Ctrl+Q"));

        // Edit
        selectAllAction->setShortcut(QKeySequence("Ctrl+A"));
        invertSelectionAction->setShortcut(QKeySequence("Shift+Ctrl+A"));
        rejectAction->setShortcut(QKeySequence("X"));
        pickAction->setShortcut(QKeySequence("`"));
        pickUnlessRejectedAction->setShortcut(QKeySequence("Shift+Ctrl+`"));
        pick1Action->setShortcut(QKeySequence("P"));
        popPickHistoryAction->setShortcut(QKeySequence("Alt+Ctrl+Z"));

        searchTextEditAction->setShortcut(QKeySequence("F2"));

        rate1Action->setShortcut(QKeySequence("1"));
        rate2Action->setShortcut(QKeySequence("2"));
        rate3Action->setShortcut(QKeySequence("3"));
        rate4Action->setShortcut(QKeySequence("4"));
        rate5Action->setShortcut(QKeySequence("5"));

        label1Action->setShortcut(QKeySequence("6"));
        label2Action->setShortcut(QKeySequence("7"));
        label3Action->setShortcut(QKeySequence("8"));
        label4Action->setShortcut(QKeySequence("9"));
        label5Action->setShortcut(QKeySequence("0"));

        rotateLeftAction->setShortcut(QKeySequence("Ctrl+["));
        rotateRightAction->setShortcut(QKeySequence("Ctrl+]"));

        prefAction->setShortcut(QKeySequence("Ctrl+,"));

        // Go
        keyRightAction->setShortcut(QKeySequence("Right"));
        keyLeftAction->setShortcut(QKeySequence("Left"));
        keyHomeAction->setShortcut(QKeySequence("Home"));
        keyEndAction->setShortcut(QKeySequence("End"));
        keyDownAction->setShortcut(QKeySequence("Down"));
        keyUpAction->setShortcut(QKeySequence("Up"));
        keyPageUpAction->setShortcut(QKeySequence("PgUp"));
        keyPageDownAction->setShortcut(QKeySequence("PgDown"));

        keyScrollLeftAction->setShortcut(QKeySequence("Ctrl+Left"));
        keyScrollRightAction->setShortcut(QKeySequence("Ctrl+Right"));
        keyScrollUpAction->setShortcut(QKeySequence("Ctrl+Up"));
        keyScrollDownAction->setShortcut(QKeySequence("Ctrl+Down"));

        keyScrollPageLeftAction->setShortcut(QKeySequence("Ctrl+Alt+Left"));
        keyScrollPageRightAction->setShortcut(QKeySequence("Ctrl+Alt+Right"));
        keyScrollPageUpAction->setShortcut(QKeySequence("Ctrl+Alt+Up"));
        keyScrollPageDownAction->setShortcut(QKeySequence("Ctrl+Alt+Down"));

        nextPickAction->setShortcut(QKeySequence("Ctrl+Shift+Alt+Right"));
        prevPickAction->setShortcut(QKeySequence("Ctrl+Shift+Alt+Left"));
        randomImageAction->setShortcut(QKeySequence("Shift+Ctrl+Right"));

        // Filters
        filterUpdateAction->setShortcut(QKeySequence("Shift+F"));
        clearAllFiltersAction->setShortcut(QKeySequence("Shift+C"));
        filterPickAction->setShortcut(QKeySequence("Shift+`"));

        filterSearchAction->setShortcut(QKeySequence("Shift+S"));

        filterRating1Action->setShortcut(QKeySequence("Shift+1"));
        filterRating2Action->setShortcut(QKeySequence("Shift+2"));
        filterRating3Action->setShortcut(QKeySequence("Shift+3"));
        filterRating4Action->setShortcut(QKeySequence("Shift+4"));
        filterRating5Action->setShortcut(QKeySequence("Shift+5"));

        filterRedAction->setShortcut(QKeySequence("Shift+6"));
        filterYellowAction->setShortcut(QKeySequence("Shift+7"));
        filterGreenAction->setShortcut(QKeySequence("Shift+8"));
        filterBlueAction->setShortcut(QKeySequence("Shift+9"));
        filterPurpleAction->setShortcut(QKeySequence("Shift+0"));

        filterLastDayAction->setShortcut(QKeySequence("Shift+D"));

        // Sort
        sortReverseAction->setShortcut(QKeySequence("alt+S"));

        // View
        asLoupeAction->setShortcut(QKeySequence("E"));
        asGridAction->setShortcut(QKeySequence("G"));
        asTableAction->setShortcut(QKeySequence("T"));
        asCompareAction->setShortcut(QKeySequence("C"));

        slideShowAction->setShortcut(QKeySequence("S"));
        fullScreenAction->setShortcut(QKeySequence("F"));
//        escapeFullScreenAction->setShortcut(QKeySequence("Esc")); // see MW::eventFilter

        ratingBadgeVisibleAction->setShortcut(QKeySequence("Ctrl+I"));
        iconNumberVisibleAction->setShortcut(QKeySequence("Ctrl+Shift+I"));
        infoVisibleAction->setShortcut(QKeySequence("I"));

        zoomToAction->setShortcut(QKeySequence("Z"));
        zoomInAction->setShortcut(QKeySequence("+"));
        zoomOutAction->setShortcut(QKeySequence("-"));
//        zoomToggleAction->setShortcut(QKeySequence("Space"));
        zoomToggleAction->setShortcut(Qt::Key_Space);

//        thumbsFitAction->setShortcut(QKeySequence("Alt+]"));
        thumbsEnlargeAction->setShortcut(QKeySequence("]"));
        thumbsShrinkAction->setShortcut(QKeySequence("["));


        // Window
        newWorkspaceAction->setShortcut(QKeySequence("W"));
        manageWorkspaceAction->setShortcut(QKeySequence("Ctrl+W"));
        defaultWorkspaceAction->setShortcut(QKeySequence("Ctrl+Shift+W"));

        folderDockVisibleAction->setShortcut(QKeySequence("Shift+F3"));
        favDockVisibleAction->setShortcut(QKeySequence("Shift+F4"));
        filterDockVisibleAction->setShortcut(QKeySequence("Shift+F5"));
        metadataDockVisibleAction->setShortcut(QKeySequence("Shift+F6"));
        thumbDockVisibleAction->setShortcut(QKeySequence("Shift+F7"));
        menuBarVisibleAction->setShortcut(QKeySequence("Shift+F9"));
        statusBarVisibleAction->setShortcut(QKeySequence("Shift+F10"));

//        folderDockLockAction->setShortcut(QKeySequence("Shift+Alt+F3"));
//        favDockLockAction->setShortcut(QKeySequence("Shift+Alt+F4"));
//        filterDockLockAction->setShortcut(QKeySequence("Shift+Alt+F5"));
//        metadataDockLockAction->setShortcut(QKeySequence("Shift+Alt+F6"));
//        thumbDockLockAction->setShortcut(QKeySequence("Shift+Alt+F7"));
//        allDocksLockAction->setShortcut(QKeySequence("Ctrl+L"));

        // Help
        helpAction->setShortcut(QKeySequence("?"));
//        toggleIconsListAction->setShortcut(QKeySequence("Ctrl+T"));
    }

    setting->endGroup();
}

void MW::updateState()
{
/*
    Called when program starting or when a workspace is invoked. Based on the condition
    of actions sets the visibility of all window components.
*/
    if (G::isLogger) G::log("MW::updateState");
    // set flag so
    isUpdatingState = true;
//    setWindowsTitleBarVisibility();   // problem with full screen toggling
    // setCentralView has to precede setting visibility to docks
    setCentralView();
    setMenuBarVisibility();
    setStatusBarVisibility();
    setCacheStatusVisibility();
    setFolderDockVisibility();
    setFavDockVisibility();
    setFilterDockVisibility();
    setMetadataDockVisibility();
    setEmbelDockVisibility();
    setThumbDockVisibity();
    setShootingInfoVisibility();
    updateStatusBar();
//    setActualDevicePixelRation();
    isUpdatingState = false;
//    reportState();
}

void MW::refreshFolders()
{
    if (G::isLogger) G::log("MW::refreshFolders");
    bool showImageCount = fsTree->isShowImageCount();
    fsTree->refreshModel();
    fsTree->setShowImageCount(showImageCount);
    return;

    // make folder panel visible and set focus
    folderDock->raise();
    folderDockVisibleAction->setChecked(true);

    // set sort forward (not reverse)
    if (sortReverseAction->isChecked()) {
        sortReverseAction->setChecked(false);
        sortChange("MW::refreshFolders");
        reverseSortBtn->setIcon(QIcon(":/images/icon16/A-Z.png"));
    }

    // do not embellish
    embelProperties->invokeFromAction(embelTemplatesActions.at(0));
}

void MW::newEmbelTemplate()
{
    if (G::isLogger) G::log("MW::newEmbelTemplate");
    embelDock->setVisible(true);
    embelDock->raise();
    embelDockVisibleAction->setChecked(true);
    embelProperties->newTemplate();
    loupeDisplay();
}

void MW::tokenEditor()
{
    if (G::isLogger) G::log("MW::tokenEditor");
    infoString->editTemplates();
    // display new info
    QModelIndex idx = dm->currentSfIdx;  //thumbView->currentIndex();
    QString fPath = dm->currentFilePath;  //thumbView->getCurrentFilePath();
    QString sel = infoString->getCurrentInfoTemplate();
    QString info = infoString->parseTokenString(infoString->infoTemplates[sel],
                                        fPath, idx);
    imageView->setShootingInfo(info);
//    qDebug() << "MW::tokenEditor" << "call  updateMetadataTemplateList";
    embelProperties->updateMetadataTemplateList();
//    qDebug() << "MW::tokenEditor" << "updateMetadataTemplateList did not crash";
}

void MW::exportEmbelFromAction(QAction *embelExportAction)
{
/*
    Called from main menu Embellish > Export > Export action.  The embellish editor
    may not be active, but the embellish template has been chosen by the action.
*/
    if (G::isLogger) G::log("MW::exportEmbelFromAction");

    QStringList picks = dm->getSelectionOrPicks();

    if (picks.size() == 0)  {
        QMessageBox::information(this,
            "Oops", "There are no picks or selected images to export.    ",
            QMessageBox::Ok);
        return;
    }

    EmbelExport embelExport(metadata, dm, icd, embelProperties);
    connect(this, &MW::abortEmbelExport, &embelExport, &EmbelExport::abortEmbelExport);

//    embelExport.exportRemoteFiles(embelExportAction->text(), picks);

    embelProperties->setCurrentTemplate(embelExportAction->text());
    G::isProcessingExportedImages = true;
    bool isRemote = true;
    embelExport.exportImages(picks, isRemote);
    embelProperties->doNotEmbellish();
    G::isProcessingExportedImages = false;
    bookmarks->count();
}

void MW::exportEmbel()
{
/*
    Called when embellish editor is active and an embellish template has been selected.
*/
    if (G::isLogger) G::log("MW::exportEmbel");

    QStringList picks = dm->getSelectionOrPicks();

    if (picks.size() == 0)  {
        QMessageBox::information(this,
            "Oops", "There are no picks or selected images to export.    ",
            QMessageBox::Ok);
        return;
    }

    EmbelExport embelExport(metadata, dm, icd, embelProperties);
    connect(this, &MW::abortEmbelExport, &embelExport, &EmbelExport::abortEmbelExport);

    embelExport.exportImages(picks);
}

void MW::ingest()
{
/*
    Copies images from a source location (usually a camera media card) to one or more
    destinations.  Ingestion comprises of three components:

    1.  MW::ingest()

        This function keeps track of the prevSourceFolder and baseFolderDescription during
        subsequent calls.  It uses this to effect the behavior of the IngestDlg, which is
        called.  When IngestDlg closes persistent ingest data is saved in settings.  If the
        isBackgroundIngest flag == true then a backgroundIngest instantiation of Ingest is
        created and run.

    2.  IngestDlg

        When this dialog is invoked the files that have been picked are copied to a primary
        destination folder, and optionally, to a backup location. This process is known as
        ingestion: selecting and copying images from a camera card to a computer. If backround
        ingesting is selected then the dialog closes and the user can continue working while
        the ingest happens in the background in another thread, using all the IngestDlg
        settings.

        The destination folder can be selected/created manually or automatically.  If
        automatic, a root folder is selected/created, a path to the destination folder
        is defined using tokens, and the destination folder description defined.  The
        picked images can be renamed during this process.

        Files are copied to a destination based on building a file path consisting of:

              Root Folder                   (rootFolderPath)
            + Path to base folder           (fromRootToBaseFolder) source pathTemplateString
            + Base Folder Description       (baseFolderDescription)
            + File Name                     (fileBaseName)     source filenameTemplateString
            + File Suffix                   (fileSuffix)

            ie E:/2018/201802/2018-02-08 Rory birthday/2018-02-08_0001.NEF

            rootFolderPath:         ie "E:/" where all the images are located
            fromRootToBaseFolder:   ie "2018/201802/2018-02-08" from the path template
            baseFolderDescription:  ie " Rory birthday" a description appended to the
                                        pathToBaseFolder
            fileBaseName:           ie "2018-02-08_0001" from the filename template
            fileSuffix:             ie ".NEF"

            folderPath:             ie "E:/2018/201802/2018-02-08 Rory birthday/"
                = rootFolderPath + fromRootToBaseFolder + baseFolderDescription + "/"
                = the copy to destination

            The strings fromRootToBaseFolder and the fileBaseName can be tokenized in
            TokenDlg, allowing the user to automate the construction of the entire destination
            file path.

    3. Ingest (class)

        Ingest duplicates the actual ingest process that runs in the IngestDlg, but runs in a
        separate thread after the IngestDlg closes.  Progress is updated in progressBar at the
        extreme left in the status bar.
*/

    if (G::isLogger) G::log("MW::ingest");

    // check if background ingest in progress
//    bool backgroundIngestInProgress = backgroundIngest != nullptr;
//    if (isBackgroundIngest && backgroundIngestInProgress) {
    if (G::isRunningBackgroundIngest) {
        QString msg =
                "There is a background ingest in progress.  When it<br>"
                "has completed the progress bar on the left side of<br>"
                "the status bar will disappear and you can make another<br>"
                "ingest."
                ;
        G::popUp->showPopup(msg, 5000);
        return;
    }

    static QString prevSourceFolder = "";
    static QString baseFolderDescription = "";
    /*
    qDebug() << "MW::ingest"
             << "prevSourceFolder" << prevSourceFolder
             << "currentViewDirPath" << G::currRootFolder
             << "baseFolderDescription" << baseFolderDescription
                ;  //*/
    if (prevSourceFolder != G::currRootFolder) baseFolderDescription = "";

    QString folderPath;        // req'd by backgroundIngest
    QString folderPath2;       // req'd by backgroundIngest
    bool combinedIncludeJpg;   // req'd by backgroundIngest
    int seqStart = 1;          // req'd by backgroundIngest

    if (dm->isPick()) {
        ingestDlg = new IngestDlg(this,
                                  combineRawJpg,
                                  combinedIncludeJpg,
                                  autoEjectUsb,
                                  integrityCheck,
                                  isBackgroundIngest,
                                  isBackgroundIngestBeep,
                                  ingestIncludeXmpSidecar,
                                  backupIngest,
                                  gotoIngestFolder,
                                  seqStart,
                                  metadata,
                                  dm,
                                  ingestRootFolder,
                                  ingestRootFolder2,
                                  manualFolderPath,
                                  manualFolderPath2,
                                  folderPath,
                                  folderPath2,
                                  baseFolderDescription,
                                  pathTemplates,
                                  filenameTemplates,
                                  pathTemplateSelected,
                                  pathTemplateSelected2,
                                  filenameTemplateSelected,
                                  ingestDescriptionCompleter,
                                  autoIngestFolderPath,
                                  css);

        bool okToIngest = ingestDlg->exec();
        delete ingestDlg;

        // update ingest history folders
        // get rid of "/" at end of path for history (in file menu)
        QString historyPath = folderPath.left(folderPath.length() - 1);
        addIngestHistoryFolder(historyPath);

        // save ingest history folders
        setting->beginGroup("IngestHistoryFolders");
        setting->remove("");
        for (int i = 0; i < ingestHistoryFolders->count(); i++) {
            setting->setValue("ingestHistoryFolder" + QString::number(i+1),
                              ingestHistoryFolders->at(i));
        }
        setting->endGroup();

        // save ingest description completer list
        setting->beginGroup("IngestDescriptionCompleter");
        for (const auto& i : ingestDescriptionCompleter) {
            setting->setValue(i, "");
        }
        setting->endGroup();

        // save ingest settings
        setting->setValue("autoIngestFolderPath", autoIngestFolderPath);
        setting->setValue("autoEjectUSB", autoEjectUsb);
        setting->setValue("integrityCheck", integrityCheck);
        setting->setValue("isBackgroundIngest", isBackgroundIngest);
        setting->setValue("isBackgroundIngestBeep", isBackgroundIngestBeep);
        setting->setValue("ingestIncludeXmpSidecar", ingestIncludeXmpSidecar);
        setting->setValue("backupIngest", backupIngest);
        setting->setValue("gotoIngestFolder", gotoIngestFolder);
        setting->setValue("ingestRootFolder", ingestRootFolder);
        setting->setValue("ingestRootFolder2", ingestRootFolder2);
        setting->setValue("pathTemplateSelected", pathTemplateSelected);
        setting->setValue("pathTemplateSelected2", pathTemplateSelected2);
        setting->setValue("manualFolderPath", manualFolderPath);
        setting->setValue("manualFolderPath2", manualFolderPath2);
        setting->setValue("filenameTemplateSelected", filenameTemplateSelected);
        setting->setValue("ingestCount", G::ingestCount);
        setting->setValue("ingestLastSeqDate", G::ingestLastSeqDate);

        if (!okToIngest) {
            qWarning() << "WARNING" << "MW::ingest" << "Not ok to ingest";
            return;
        }

        // start background ingest
        if (isBackgroundIngest) {
            backgroundIngest = new Ingest(this,
                                          combineRawJpg,
                                          combinedIncludeJpg,
                                          integrityCheck,
                                          ingestIncludeXmpSidecar,
                                          backupIngest,
                                          seqStart,
                                          metadata,
                                          dm,
                                          folderPath,
                                          folderPath2,
                                          filenameTemplates,
                                          filenameTemplateSelected);

            connect(backgroundIngest, &Ingest::updateProgress, this, &MW::setProgress);
            connect(backgroundIngest, &Ingest::ingestFinished, this, &MW::ingestFinished);
            backgroundIngest->commence();
            G::isRunningBackgroundIngest = true;
        }

        prevSourceFolder = G::currRootFolder;
        qDebug() << "MW::ingest"
                 << "gotoIngestFolder =" << gotoIngestFolder
                 << "isBackgroundIngest =" << isBackgroundIngest
                 << "lastIngestLocation =" << lastIngestLocation
                    ;

        // if background ingesting do not jump to the ingest destination folder
        if (gotoIngestFolder && !isBackgroundIngest) {
            fsTree->select(lastIngestLocation);
            folderSelectionChange();
            return;
        }

        // set the ingested flags, clear the pick flags and update pickLog
        setIngested();

        updateStatus(true, "", "MW::ingest");
    }
    else QMessageBox::information(this,
         "Oops", "There are no picks to ingest.    ", QMessageBox::Ok);
}

void MW::ejectUsb(QString path)
{
/*
    If the current folder is on the drive to be ejected, attempts to read subsequent
    files will cause a crash. This is avoided by stopping any further activity in the
    metadataCacheThread and imageCacheThread, preventing any file reading attempts to a
    non-existent drive.

    This works fine here, but not if the drive is ejected externally ie from Finder.
    FSTree emits a signal selectionChange that is connected to MW::watchCurrentFolder
    each time the FSTree QTreeView selection changes.  If the current folder being
    cached no longer exists (ejected) then make a folderSelectionChange in
    MW::watchCurrentFolder.
*/
    if (G::isLogger) G::log("MW::ejectUsb");

    // if current folder is on the USB drive to be ejected then stop caching
    QStorageInfo ejectDrive(path);
    QStorageInfo currentDrive(G::currRootFolder);
    bool ejectDriveIsCurrent = currentDrive.rootPath() == ejectDrive.rootPath();
    /*
    qDebug() << "MW::ejectUsb"
             << "currentViewDir =" << currentViewDir
             << "path =" << path
             << "currentDrive.name() =" << currentDrive.name()
             << "currentDrive.rootPath() =" << currentDrive.rootPath()
             << "ejectDrive.name() =" << ejectDrive.name()
             << "ejectDrive.rootPath() =" << ejectDrive.rootPath()
                ;
                //*/
    if (ejectDriveIsCurrent) {
        /*
        qDebug() << "MW::ejectUsb"
                 << "currentDrive.rootPath() =" << currentDrive.rootPath()
                 << "ejectDrive.rootPath() =" << ejectDrive.rootPath()
                    ;
                    //*/
        stop("ejectUSB");
    }

    // get the drive name
    QString driveName = ejectDrive.name();      // ie WIN "D:\" or MAC "Untitled"
#if defined(Q_OS_WIN)
    driveName = ejectDrive.rootPath();
#elif defined(Q_OS_MAC)
    driveName = ejectDrive.name();
    /*
    int start = path.indexOf("/Volumes/", 0);
    if (start != 0) return;                   // should start with "/Volumes/"
    int pos = path.indexOf("/", start + 9);
    if (pos == -1) pos = path.length();
    driveName = path.mid(9, pos - 9);
    //*/
#endif

    // confirm this is an ejectable drive
    if (Usb::isUsb(path)) {
        // eject USD drive
        int result = Usb::eject(driveName);
        // drive was ejected
        if (result < 2) {
            G::popUp->showPopup("Ejecting drive " + driveName, 2000);
            // setCentralMessage("Removable drive " + driveName + " has been ejected.");
        }
        // drive ejection failed
        else
            G::popUp->showPopup("Failed to eject drive " + driveName, 2000);
    }
    // drive not ejectable
    else {
        G::popUp->showPopup("Drive " + driveName
              + " is not removable and cannot be ejected", 2000);
    }
}

void MW::ejectUsbFromMainMenu()
{
    if (G::isLogger) G::log("MW::ejectUsbFromMainMenu");
    ejectUsb(G::currRootFolder);
}

void MW::ejectUsbFromContextMenu()
{
    if (G::isLogger) G::log("MW::ejectUsbFromContextMenu");
    ejectUsb(mouseOverFolderPath);
}

void MW::embedThumbnailsFromAction()
{
    if (G::isLogger) G::log("MW::embedThumbnailsFromAction");
    chkMissingEmbeddedThumbnails("FromAction");
}

void MW::chkMissingEmbeddedThumbnails(QString src)
{
    if (G::isLogger) G::log("MW::chkMissingEmbeddedThumbnails");

    if (!dm->isMissingEmbeddedThumb) return;
//    if (G::modifySourceFiles) return;

    if (!ignoreAddThumbnailsDlg && src == "FromLoading") {
        AddThumbnailsDlg *dlg = new AddThumbnailsDlg;
        connect (dlg, &AddThumbnailsDlg::ignore, this, &MW::setIgnoreAddThumbnailsDlg);
        connect (dlg, &AddThumbnailsDlg::backup, this, &MW::setBackupModifiedFiles);
        bool ok = dlg->exec();
        disconnect (dlg, &AddThumbnailsDlg::ignore, this, &MW::setIgnoreAddThumbnailsDlg);
        disconnect (dlg, &AddThumbnailsDlg::backup, this, &MW::setBackupModifiedFiles);
        delete dlg;
        if (!ok) return;
        sel->all();
    }

    QString result = embedThumbnails();
    if (src == "FromLoading") sel->current(dm->currentSfIdx);
    thumbView->refreshThumbs();
    G::popUp->showPopup(result, 3000);
}

QString MW::embedThumbnails()
{
/*
    MENU:  Edit > Utilities > Embed missing thumbnails
           Also in thumbView context menu

    There are two routines to do this:

        • Thumb::insertThumbnailsInJpg inserts thumbnails for a batch of files at
          a time using ExifTool.

        • Tiff::encodeThumbnail inserts thumbnails one open file at a time.

*/
    if (G::isLogger) G::log("MW::insertThumbnails");

    QModelIndexList selection = dm->selectionModel->selectedRows();
    if (selection.isEmpty()) return "Nothing selected";
    int n = selection.size();

    QString txt = "Embedding thumbnail(s) for " + QString::number(selection.size()) +
                  " images <p>Press <font color=\"red\"><b>Esc</b></font> to abort.";
    G::popUp->setProgressVisible(true);
    G::popUp->showPopup(txt, 0, true, 1);
    qApp->processEvents();

    // copy selection to list of dm rows (proxy filter changes during iteration when change datamodel)
    QList<int> rows;
    for (int i = 0; i < n; ++i) {
        int dmRow = dm->modelRowFromProxyRow(selection.at(i).row());
        rows.append(dmRow);
    }

    bool lockEncountered = false;
    bool embeddingHappened = false;

    // set flags for Tiff::parse to call Tiff::encodeThumbnail
    bool okToModify = G::modifySourceFiles;
    bool autoAdd = G::autoAddMissingThumbnails;
    G::modifySourceFiles = true;
    G::autoAddMissingThumbnails = true;
    for (int i = 0; i < n; i++) {
        G::popUp->setProgress(i+1);
        qApp->processEvents();
        int dmRow = rows.at(i);
        QString fileType = dm->index(dmRow, G::TypeColumn).data().toString().toLower();
        if (!metadata->canEmbedThumb.contains(fileType)) continue;
        QString fPath = dm->index(dmRow, G::PathColumn).data(G::PathRole).toString();
        bool isMissing = dm->index(dmRow, G::MissingThumbColumn).data().toBool();
        bool isReadWrite = dm->index(dmRow, G::ReadWriteColumn).data().toBool();
        if (!isReadWrite) lockEncountered = true;
        if (isMissing && isReadWrite) {
            if (G::backupBeforeModifying) {
                Utilities::backup(fPath, "backup");
            }
            dm->refreshMetadataForItem(dmRow, dm->instance);
            embeddingHappened = true;
        }
    }
    // reset flags
    G::modifySourceFiles = okToModify;
    G::autoAddMissingThumbnails = autoAdd;

    refreshBookmarks();

    // update filter list and counts
    buildFilters->updateCategory(BuildFilters::MissingThumbEdit, BuildFilters::NoAfterAction);

    G::popUp->setProgressVisible(false);
    G::popUp->end();

    QString msg;
    QString br;
    if (embeddingHappened) msg += "Thumbnail insertion completed";
    if (!embeddingHappened && !lockEncountered) msg += "No missing thumbnails in selection";
    if (msg.length()) br = "<br>";
    if (lockEncountered) msg += br + "Unable to embed thumbnails in locked files";

    //embedThumbnailsAction->setEnabled(false);
    return msg;
}

void MW::metadataChanged(QStandardItem* item)
{
/*
    This slot is called when there is a data change in InfoView.

    If the change was a result of a new image selection then ignore.

    If the metadata in the tags section of the InfoView panel has been editied (title,
    creator, copyright, email or url) then all selected image tag(s) are updated to the
    new value(s) in the data model. If xmp edits are enabled the new tag(s) are embedded
    in the image metadata, either internally or as a sidecar when ingesting. If raw+jpg
    are combined then the raw file rows are also updated in the data model.
*/
     if (!G::useInfoView) return;
    if (G::isLogger) G::log("MW::metadataChanged");
    // if new folder is invalid no relevent data has changed
    if(!isCurrentFolderOkay) return;
     if (G::useInfoView) if (infoView->ignoreDataChange) return;

    QModelIndex par = item->index().parent();
     if (G::useInfoView) if (par != infoView->tagInfoIdx) return;

    QString tagValue = item->data(Qt::DisplayRole).toString();
    QModelIndexList selection = dm->selectionModel->selectedRows();
    int row = item->index().row();
    QModelIndex tagIdx = infoView->ok->index(row, 0, par);
    QString tagName = tagIdx.data().toString();

    QHash<QString,int> col;
    col["Title"] = G::TitleColumn;
    col["Creator"] = G::CreatorColumn;
    col["Copyright"] = G::CopyrightColumn;
    col["Email"] = G::EmailColumn;
    col["Url"] = G::UrlColumn;

    // list of file paths to send to Metadata::writeMetadata
    QStringList paths;

    QString src = "MW::metadataChanged";
    for (int i = 0; i < selection.count(); ++i) {
        int row = selection.at(i).row();
        // build list of files to send to Metadata::writeMetadata
        paths << dm->sf->index(row, G::PathColumn).data().toString();
        // update data model
        QModelIndex dmIdx = dm->sf->mapToSource(dm->sf->index(row, col[tagName]));
        emit setValue(dmIdx, tagValue, dm->instance, src, Qt::EditRole, Qt::AlignLeft);
        // check if combined raw+jpg and also set the tag item for the hidden raw file
        if (combineRawJpg) {
            // is this part of a raw+jpg pair
            if (dmIdx.data(G::DupIsJpgRole).toBool()) {
                // set tag item for raw file row as well
                QModelIndex rawIdx = qvariant_cast<QModelIndex>(dmIdx.data(G::DupOtherIdxRole));
                QModelIndex idx = dm->index(rawIdx.row(), col[tagName]);
                emit setValue(idx, tagValue, dm->instance, src, Qt::EditRole, Qt::AlignCenter);
            }
        }
    }

    // update shooting info
    imageView->updateShootingInfo();
}

void MW::getSubfolders(QString fPath)
{
    if (G::isLogger) G::log("MW::getSubfolders");
    subfolders = new QStringList;
    subfolders->append(fPath);
    QDirIterator iterator(fPath, QDirIterator::Subdirectories);
    while (iterator.hasNext()) {
        iterator.next();
        fPath = iterator.filePath();
        if (iterator.fileInfo().isDir() && iterator.fileName() != "." && iterator.fileName() != "..") {
            subfolders->append(fPath);
        }
    }
}

void MW::selectCurrentViewDir()
{
    if (G::isLogger) G::log("MW::selectCurrentViewDir");
    QModelIndex idx = fsTree->fsModel->index(G::currRootFolder);
    if (idx.isValid()){
        fsTree->expand(idx);
        fsTree->setCurrentIndex(idx);
    }
}

void MW::wheelEvent(QWheelEvent *event)
{
    // rgh ??
    if (G::isLogger) G::log("MW::wheelEvent");
//        if (event->modifiers() == Qt::ControlModifier)
//        {
//            if (event->delta() < 0)
//                imageView->zoomOut();
//            else
//                imageView->zoomIn();
//        }
//        else if (keyRightAction->isEnabled())
//        {
//            if (event->delta() < 0)
//                thumbView->selectNext();
//            else
//                thumbView->selectPrev();
//        }
    QMainWindow::wheelEvent(event);
}

// not req'd rgh ??
void MW::showNewImageWarning(QWidget *parent)
// called from runExternalApp
{
    if (G::isLogger) G::log("MW::showNewImageWarning");
    QMessageBox msgBox;
    msgBox.warning(parent, tr("Warning"), tr("Cannot perform action with temporary image."));
}

void MW::addNewBookmarkFromMenu()
{
    if (G::isLogger) G::log("MW::addNewBookmarkFromMenu");
    addBookmark(G::currRootFolder);
}

void MW::addNewBookmarkFromContextMenu()
{
    if (G::isLogger) G::log("MW::addNewBookmarkFromContextMenu");
    addBookmark(mouseOverFolderPath);
}

void MW::addBookmark(QString path)
{
    if (G::isLogger) G::log("MW::addBookmark");
    bookmarks->bookmarkPaths.insert(path);
    bookmarks->reloadBookmarks();
}

void MW::openFolder()
{
    if (G::isLogger) G::log("MW::openFolder");
    QString dirPath = QFileDialog::getExistingDirectory(this, tr("Open Folder"),
         "/home", QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);
    if (dirPath == "") return;
    fsTree->select(dirPath);
    folderSelectionChange();
}

void MW::refreshCurrentFolder()
{
/*
    Reloads the current folder and returns to the same image.  This is handy when some of the
    folder images have changed.
*/
    if (G::isLogger) G::log("MW::refreshCurrentFolder");
    QString src = "MW::refreshCurrentFolder";
    isRefreshingDM = true;
    refreshCurrentPath = dm->sf->index(dm->currentSfRow, 0).data(G::PathRole).toString();
    if (dm->hasFolderChanged() && dm->modifiedFiles.count()) {
        for (int i = 0; i < dm->modifiedFiles.count(); ++i) {
            QString fPath = dm->modifiedFiles.at(i).filePath();
            int dmRow = dm->rowFromPath(fPath);
            if (dmRow == -1) continue;
            int sfRow = dm->sf->mapFromSource(dm->index(dmRow, 0)).row();
            // update file size and modified date
            dm->updateFileData(dm->modifiedFiles.at(i));

            // update metadata
            QString ext = dm->modifiedFiles.at(i).suffix().toLower();
            if (metadata->hasMetadataFormats.contains(ext)) {
                if (metadata->loadImageMetadata(dm->modifiedFiles.at(i), dm->instance, true, true, false, true, "MW::refreshCurrentFolder")) {
                    metadata->m.row = dmRow;
                    metadata->m.instance = dm->instance;
                    dm->addMetadataForItem(metadata->m, "MW::refreshCurrentFolder");
                }
            }

            // update image cache in case image has changed
            if (icd->imCache.contains(fPath)) icd->imCache.remove(fPath);
            if (dm->currentFilePath == fPath) {
                if (imageView->loadImage(fPath, "MW::refreshCurrentFolder")) {
                    updateClassification();
                }
            }

            // update thumbnail in case image has changed
            QImage image;
            bool thumbLoaded = thumb->loadThumb(fPath, image, dm->instance, "MW::refreshCurrentFolder");
            if (thumbLoaded) {
                QPixmap pm = QPixmap::fromImage(image.scaled(G::maxIconSize, G::maxIconSize, Qt::KeepAspectRatio));
                QModelIndex dmIdx = dm->index(dmRow, 0);
                dm->setIcon(dmIdx, pm, dm->instance, src);
            }
        }
         if (G::useInfoView) infoView->updateInfo(dm->currentSfRow);
//        metadataCacheThread->loadNewFolder(true);
        refreshCurrentAfterReload();
    }
    else selectionChange();
}

void MW::refreshCurrentAfterReload()
{
/*
    This slot is triggered after the metadataCache thread has run and isRefreshingDM = true to
    complete the refresh current folder process by selecting the previously selected thumb.
*/
    if (G::isLogger) G::log("MW::refreshCurrentAfterReload");
    int dmRow = dm->rowFromPath(refreshCurrentPath);
    if (dmRow == -1) return;
    int sfRow = dm->sf->mapFromSource(dm->index(dmRow, 0)).row();
    /*
    qDebug() << "MW::refreshCurrentAfterReload" << refreshCurrentPath
             << "dmRow =" << dmRow
             << "sfRow =" << sfRow;
//             */
    thumbView->iconViewDelegate->currentRow = sfRow;
    gridView->iconViewDelegate->currentRow = sfRow;
    sel->current(sfRow);
    isRefreshingDM = false;
}

void MW::saveAsFile()
{
    if (G::isLogger) G::log("MW::saveAsFile");
    QModelIndexList selection = dm->selectionModel->selectedRows();
    if (selection.length() == 0) {
        G::popUp->showPopup("No images selected for save as operation", 1500);
        return;
    }
    saveAsDlg = new SaveAsDlg(selection, metadata, dm);
    saveAsDlg->setStyleSheet(css);
    saveAsDlg->exec();
}

void MW::shareFiles()
{
    if (G::isLogger) G::log("MW::copy");

    QModelIndexList selection = dm->selectionModel->selectedRows();
    if (selection.isEmpty()) return;

    QList<QUrl> urls;
    for (int i = 0; i < selection.count(); ++i) {
        QString fPath = selection.at(i).data(G::PathRole).toString();
        urls << QUrl::fromLocalFile(fPath);
        qDebug() << "IconView::shareFiles" << fPath;
    }

    // Convert the file urls to native NSURLs  src snippets and chatGPT
    Mac::share(urls);
}

void MW::copyFiles()
{
    if (G::isLogger) G::log("MW::copy");
    QModelIndexList selection = dm->selectionModel->selectedRows();
    if (selection.isEmpty()) return;

    int n = selection.count();
    QClipboard *clipboard = QGuiApplication::clipboard();
    QMimeData *mimeData = new QMimeData;
    QList<QUrl> urls;
    for (int i = 0; i < n; ++i) {
        QString fPath = selection.at(i).data(G::PathRole).toString();
        urls << QUrl::fromLocalFile(fPath);
        qDebug() << "IconView::copyFiles" << fPath;
    }
    mimeData->setUrls(urls);
    clipboard->setMimeData(mimeData);

    QString nPaths;
    if (n == 1) nPaths = "1 file";
    else nPaths = QString::number(n) + " files";
    QString msg = "Copied " + nPaths + " to the clipboard";
    G::popUp->showPopup(msg, 1500);
}

void MW::copyFolderPathFromContext()
{
    if (G::isLogger) G::log("MW::copyFolderPathFromContext");
    QApplication::clipboard()->setText(mouseOverFolderPath);
    QString msg = "Copied " + mouseOverFolderPath + " to the clipboard";
    G::popUp->showPopup(msg, 1500);
}

void MW::copyImagePathFromContext()
{
    if (G::isLogger) G::log("MW::copyImagePathFromContext");
    QModelIndexList selection = dm->selectionModel->selectedRows();
    int n = selection.count();
    QString paths;
    for (int i = 0; i < n; ++i) {
        paths += selection.at(i).data(G::PathRole).toString();
        if (i < n - 1) paths += "\n";
    }
    QApplication::clipboard()->setText(paths);

    QString nPaths;
    if (n == 1) nPaths = "1 path";
    else nPaths = QString::number(n) + " paths";
    QString msg = "Copied " + nPaths + " to the clipboard";
    G::popUp->showPopup(msg, 1500);
}

void MW::deleteSelectedFiles()
{
/*
    Build a QStringList of the selected files and call MW::deleteFiles.
*/
    if (G::isLogger) G::log("MW::deleteSelectedFiles");
    // make sure datamodel is loaded
    if (!G::allMetadataLoaded) {
        G::popUp->showPopup("Cannot delete images before all images have been added, 1500");
        return;
    }

    if (deleteWarning) {
        QMessageBox msgBox;
        int msgBoxWidth = 300;
        msgBox.setWindowTitle("Delete Images");
        #ifdef Q_OS_WIN
        msgBox.setText("This operation will move all selected images to the recycle bin.");
        #endif
        #ifdef Q_OS_MAC
        msgBox.setText("This operation will move all selected images to the trash.");
        #endif
        msgBox.setInformativeText("Do you want continue?");
        msgBox.setStandardButtons(QMessageBox::Yes | QMessageBox::Cancel);
        msgBox.setDefaultButton(QMessageBox::Yes);
        msgBox.setIcon(QMessageBox::Warning);
        QString s = "QWidget{font-size: 12px; background-color: rgb(85,85,85); color: rgb(229,229,229);}"
                    "QPushButton:default {background-color: rgb(68,95,118);}";
        msgBox.setStyleSheet(css);
        QSpacerItem* horizontalSpacer = new QSpacerItem(msgBoxWidth, 0, QSizePolicy::Minimum, QSizePolicy::Expanding);
        QGridLayout* layout = static_cast<QGridLayout*>(msgBox.layout());
        layout->addItem(horizontalSpacer, layout->rowCount(), 0, 1, layout->columnCount());
        int ret = msgBox.exec();
        if (ret == QMessageBox::Cancel) return;
    }

    QModelIndexList selection = dm->selectionModel->selectedRows();
    if (selection.isEmpty()) return;

    // convert selection to stringlist
    QStringList paths;
    for (int i = 0; i < selection.count(); ++i) {
        QString fPath = selection.at(i).data(G::PathRole).toString();
        paths.append(fPath);
    }

    deleteFiles(paths);
}

void MW::deleteFiles(QStringList paths)
{
/*
    Delete from disk, remove from datamodel, remove from ImageCache and update the
    image cache status bar.
*/
    if (G::isLogger) G::log("MW::deleteFiles");

    /* save the index to the first row in selection (order depends on how selection was
       made) to insure the correct index is selected after deletion.  */
    int lowRow = 999999;
    for (int i = 0; i < paths.count(); ++i) {
        int row = dm->rowFromPath(paths.at(i));
        if (row < lowRow) {
            lowRow = row;
        }
    }

    G::ignoreScrollSignal = true;

    // delete file(s) in folder on disk, including any xmp sidecars
    QStringList sldm;   // paths of successfully deleted files to remove in datamodel
    bool fileWasLocked = false;
    for (int i = 0; i < paths.count(); ++i) {
        QString fPath = paths.at(i);
        ImageMetadata m = dm->imMetadata(fPath);
        if (!m.isReadWrite) fileWasLocked = true;
        if (QFile(fPath).moveToTrash()) {
            sldm.append(fPath);
        }
        QStringList srcSidecarPaths = Utilities::getPossibleSidecars(fPath);
        foreach (QString sidecarPath, srcSidecarPaths) {
            if (QFile(sidecarPath).exists()) {
                QFile(sidecarPath).moveToTrash();
            }
        }
    }
    if (fileWasLocked) G::popUp->showPopup("Locked file(s) were not deleted", 3000);

    // refresh image count in folders and bookmarks: fsTree is signalled by the OS,
    // updates count and then signals bookmarks to recount all bookmarks
    bookmarks->count();

    // if all images in folder were deleted
    if (sldm.count() == dm->rowCount()) {
        stop("deleteFiles");
        folderSelectionChange();
        return;
    }

    // remove fPath from datamodel dm if successfully deleted
    for (int i = 0; i < sldm.count(); ++i) {
        QString fPath = sldm.at(i);
        dm->remove(fPath);
    }

    // refresh datamodel fPathRow hash
//    dm->refreshRowFromPathHash();

    // cleanup G::rowsWithIcon
    if (!G::isLinearLoading) metaReadThread->cleanupIcons();

    // remove deleted files from imageCache
    imageCacheThread->removeFromCache(sldm);

    G::ignoreScrollSignal = false;

    // update current index
    if (lowRow >= dm->sf->rowCount()) lowRow = dm->sf->rowCount() - 1;
    QModelIndex sfIdx = dm->sf->index(lowRow, 0);
    sel->current(sfIdx);

    // update filters
    qDebug() << "MW::deleteFiles launchBuildFilters())";
    launchBuildFilters();
}

void MW::deleteFolder()
{
    QString dirToDelete;
    QString senderObject = (static_cast<QAction*>(sender()))->objectName();
    if (senderObject == "deleteActiveFolder") {
        dirToDelete = G::currRootFolder;
    }
    else if (senderObject == "deleteBookmarkFolder") {
        dirToDelete = bookmarks->rightMouseClickPath;
    }
    else if (senderObject == "deleteFSTreeFolder") {
        dirToDelete = fsTree->rightMouseClickPath;
    }

    if (deleteWarning) {
        QMessageBox msgBox;
        int msgBoxWidth = 300;
        msgBox.setWindowTitle("Delete Folder");
        msgBox.setTextFormat(Qt::RichText);
        #ifdef Q_OS_WIN
        QString trash = "recycle bin";
        #endif
        #ifdef Q_OS_MAC
        #endif
        QString trash = "trash";
        msgBox.setText("This operation will move the folder<br>"
                       + dirToDelete +
                       "<br>to the " + trash);
        msgBox.setInformativeText("Do you want continue?");
        msgBox.setStandardButtons(QMessageBox::Yes | QMessageBox::Cancel);
        msgBox.setDefaultButton(QMessageBox::Cancel);
        msgBox.setIcon(QMessageBox::Warning);
        QString s = "QWidget{font-size: 12px; background-color: rgb(85,85,85); color: rgb(229,229,229);}"
                    "QPushButton:default {background-color: rgb(68,95,118);}";
        msgBox.setStyleSheet(css);
        QSpacerItem* horizontalSpacer = new QSpacerItem(msgBoxWidth, 0, QSizePolicy::Minimum, QSizePolicy::Expanding);
        QGridLayout* layout = static_cast<QGridLayout*>(msgBox.layout());
        layout->addItem(horizontalSpacer, layout->rowCount(), 0, 1, layout->columnCount());
        int ret = msgBox.exec();
        if (ret == QMessageBox::Cancel) return;
    }

    if (G::currRootFolder == dirToDelete) {
        stop("deleteFolder");
    }

    // okay to delete
    QFile(dirToDelete).moveToTrash();
//    QDir dir(dirToDelete);
//    dir.removeRecursively();

    if (bookmarks->bookmarkPaths.contains(dirToDelete)) {
        bookmarks->bookmarkPaths.remove(dirToDelete);
        bookmarks->reloadBookmarks();
    }
}

void MW::openUsbFolder()
{
/*
    A list of available USB drives are listed in a dialog for the user.  Show all subfolders
    is set and all images on the USB drive are loaded.
*/
    if (G::isLogger) G::log("MW::openUsbFolder");
    struct  UsbInfo {
        QString rootPath;
        QString name;
        QString description;
    };
    UsbInfo usbInfo;

    QMap<QString, UsbInfo> usbMap;
    QStringList usbDrives;
    int n = 0;
    foreach (const QStorageInfo &storage, QStorageInfo::mountedVolumes()) {
        if (storage.isValid() && storage.isReady()) {
            if (!storage.isReadOnly()) {
                if (Usb::isUsb(storage.rootPath())) {
                    usbInfo.rootPath = storage.rootPath();
                    usbInfo.name = storage.name();
                    QString count = QString::number(n) + ". ";
                    if (usbInfo.name.length() > 0)
                        usbInfo.description = count + usbInfo.name + " (" + usbInfo.rootPath + ")";
                    else
                        usbInfo.description = count + usbInfo.rootPath;
                    usbMap.insert(usbInfo.description, usbInfo);

                    usbDrives << usbInfo.description;
                    n++;
                }
            }
        }
    }

    QString drive;

    if (usbDrives.length() > 1) {
        loadUsbDlg = new LoadUsbDlg(this, usbDrives, drive);
        loadUsbDlg->exec();
    }
    else if (usbDrives.length() == 1) {
        drive = usbDrives.at(0);
    }
    else if (usbDrives.length() == 0) {
        G::popUp->showPopup("No USB Drives available");
    }

    refreshFolders();
    bookmarks->selectionModel()->clear();
//    bool wasSubFoldersChecked = subFoldersAction->isChecked();
//    if (!wasSubFoldersChecked) subFoldersAction->setChecked(true);
//    updateStatusBar();
    G::includeSubfolders = true;
    QString fPath = usbMap[drive].rootPath;
    fsTree->select(fPath);
    isCurrentFolderOkay = isFolderValid(fPath, true, false);
    if (isCurrentFolderOkay) {
        QModelIndex idx = fsTree->fsModel->index(fPath);
        QModelIndex filterIdx = fsTree->fsFilter->mapFromSource(idx);
        fsTree->setCurrentIndex(filterIdx);
        fsTree->scrollTo(filterIdx, QAbstractItemView::PositionAtCenter);
        folderSelectionChange();
    }
    else {
        setWindowTitle(winnowWithVersion);
        setCentralMessage("Unable to access " + fPath);
    }
}

void MW::revealLogFile()
{
    if (G::isLogger) G::log("MW::revealLogFile");

    // message dialog explaining process to user
    QMessageBox msgBox;
    int msgBoxWidth = 300;
    msgBox.setWindowTitle("Email Log File to Winnow Developer (Rory)");
    msgBox.setText("If you continue two windows will be opened: \n"
                   "\n1. File explorer or finder showing the file 'WinnowLog.txt'. "
                   "\n2. A selection of your email clients."
                   "\n\nPlease select an email client.  It will open and "
                   "the 'to:' and 'subject: will be filled.  The body will have some "
                   "instructions to add 'WinnowLog.txt' as an attachment."
                  );
    msgBox.setInformativeText("Do you want continue?");
    msgBox.setStandardButtons(QMessageBox::Yes | QMessageBox::Cancel);
    msgBox.setDefaultButton(QMessageBox::Cancel);
    msgBox.setIcon(QMessageBox::Warning);
    QString s = "QWidget{font-size: 12px; background-color: rgb(85,85,85); color: rgb(229,229,229);}"
                "QPushButton:default {background-color: rgb(68,95,118);}";
    msgBox.setStyleSheet(css);
    QSpacerItem* horizontalSpacer = new QSpacerItem(msgBoxWidth, 0, QSizePolicy::Minimum, QSizePolicy::Expanding);
    QGridLayout* layout = static_cast<QGridLayout*>(msgBox.layout());
    layout->addItem(horizontalSpacer, layout->rowCount(), 0, 1, layout->columnCount());
    int ret = msgBox.exec();
    if (ret == QMessageBox::Cancel) return;

    // open explorer/finder at WinnowLog.txt location
    QString dirPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    dirPath += "/Log";
    revealInFileBrowser(dirPath);

    // open email to send to Rory
    QString to = "winnowimageviewer@outlook.com";
    QString subject = "Winnow log file";
    QString body = "Please add the file 'WinnowLog.txt' that Winnow has revealed in "
                   "Explorer/Finder to this email as an attachment.  Also, please add some "
                   "text explaining the issue.  \n\nThanks very much.  \nRory";
    QDesktopServices::openUrl(QUrl("mailto:" + to + "?subject=" + subject + "&body=" + body, QUrl::TolerantMode));

}

void MW::revealWinnets()
{
    if (G::isLogger) G::log("MW::revealWinnets");
    QString dirPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    dirPath += "/Winnets";
    revealInFileBrowser(dirPath);
}

void MW::revealFile()
{
    if (G::isLogger) G::log("MW::revealFile");
    QString fPath = dm->sf->index(dm->currentSfRow, 0).data(G::PathRole).toString();
    revealInFileBrowser(fPath);
}

void MW::revealFileFromContext()
{
    if (G::isLogger) G::log("MW::revealFileFromContext");
    revealInFileBrowser(mouseOverFolderPath);
}

void MW::revealInFileBrowser(QString path)
{
/*
    See http://stackoverflow.com/questions/3490336/how-to-reveal-in-finder-or-show-in-explorer-with-qt
    for details
*/
    if (G::isLogger) G::log("MW::revealInFileBrowser");
//    QString path = thumbView->getCurrentFilename();
//    QString path = (mouseOverFolder == "") ? currentViewDir : mouseOverFolder;
    QFileInfo info(path);
#if defined(Q_OS_WIN)
    QStringList args;
    if (!info.isDir())
        args << "/select,";
    args << QDir::toNativeSeparators(path);
    if (QProcess::startDetached("explorer", args))
        return;
#elif defined(Q_OS_MAC)
    QStringList args;
    args << "-e";
    args << "tell application \"Finder\"";
    args << "-e";
    args << "activate";
    args << "-e";
    args << "select POSIX file \"" + path + "\"";
    args << "-e";
    args << "end tell";
    if (!QProcess::execute("/usr/bin/osascript", args))
        return;
#endif
    QDesktopServices::openUrl(QUrl::fromLocalFile(info.isDir()? path : info.path()));
}

void MW::collapseAllFolders()
{
    if (G::isLogger) G::log("MW::collapseAllFolders");
    fsTree->collapseAll();
}

void MW::openInFinder()
{
    if (G::isLogger) G::log("MW::openInFinder");
    takeCentralWidget();
    setCentralWidget(imageView);
}

void MW::openInExplorer()
{
    if (G::isLogger) G::log("MW::openInExplorer");
    QString path = "C:/exampleDir/example.txt";

    QStringList args;

    args << "/select," << QDir::toNativeSeparators(path);

    QProcess *process = new QProcess(this);
    process->start("explorer.exe", args);
}

bool MW::isFolderValid(QString dirPath, bool report, bool isRemembered)
{
    if (G::isLogger) G::log("MW::isFolderValid", dirPath);
    QString msg;
    QDir testDir(dirPath);

    if (dirPath.length() == 0) {
        if (report) {
            if (isRemembered)
                msg = "The last folder from your previous session is unavailable";
            else
                if (testDir.exists())
                    msg = "No images available in this folder";
                else
                    msg = "The folder (" + dirPath + ") does not exist or is not available";

            statusLabel->setText("");
            setCentralMessage(msg);
        }
        return false;
    }

    if (!testDir.exists()) {
        if (report) {
//            QMessageBox::critical(this, tr("Error"), tr("The folder does not exist or is not available"));
            if (isRemembered)
                msg = "The last folder from your previous session (" + dirPath + ") does not exist or is not available";
            else
                msg = "The folder (" + dirPath + ") does not exist or is not available";

            statusLabel->setText("");
            setCentralMessage(msg);
        }
        return false;
    }

    // check if unmounted USB drive
    if (!testDir.isReadable()) {
        if (report) {
            msg = "The folder " + Utilities::enquote(dirPath) + " is not readable.\n\nPerhaps it was a USB drive that is not currently mounted or that has been ejected, \nor you may not have permission to view this folder.";
            statusLabel->setText("");
            setCentralMessage(msg);
        }
        return false;
    }

    return true;
}

void MW::generateMeanStack()
{
    if (G::isLogger) G::log("MW::generateMeanStack");
    QStringList selection;
    if (!dm->getSelection(selection)) return;
    meanStack = new Stack(selection, dm, metadata, icd);
    connect(this, &MW::abortStackOperation, meanStack, &Stack::stop);
    QString fPath = meanStack->mean();
    if (fPath != "") {
        dm->insert(fPath);
        int sfRow = dm->rowFromPath(fPath);
        metadataCacheThread->loadIcon(sfRow);
        imageCacheThread->rebuildImageCacheParameters(fPath, "MW::generateMeanStack");
        QModelIndex idx = dm->proxyIndexFromPath(fPath);
        fileSelectionChange(idx, idx, true, "MW::generateMeanStack");
        sel->current(fPath);
    }
}

void MW::reportHueCount()
{
    if (G::isLogger) G::log("MW::reportHueCount");

    QStringList selection;
    if (!dm->getSelection(selection)) return;
    ColorAnalysis hueReport;
    connect(this, &MW::abortHueReport, &hueReport, &ColorAnalysis::abortHueReport);
    hueReport.process(selection);
}

void MW::mediaReadSpeed()
{
    if (G::isLogger) G::log("MW::mediaReadSpeed");
    QString fPath = QFileDialog::getOpenFileName(this,
                                                 tr("Select file for speed test"),
                                                 "/home"
                                                 );
    QFile file(fPath);
    double MBPerSec = Performance::mediaReadSpeed(file) * 1024 / 8;
    if (static_cast<int>(MBPerSec) == -1) return;  // err
    QString msg = "Media read speed: : " + QString::number(MBPerSec, 'f', 0) +
                  " MB/sec.   Press Esc to continue.";
    G::popUp->showPopup(msg, 0);
}

void MW::help()
{
    if (G::isLogger) G::log("MW::help");
    QWidget *helpDoc = new QWidget;
    Ui::helpForm ui;
    ui.setupUi(helpDoc);
    helpDoc->show();
}

void MW::helpShortcuts()
{
    if (G::isLogger) G::log("MW::helpShortcuts");
    QScrollArea *helpShortcuts = new QScrollArea;
    Ui::shortcutsForm ui;
    ui.setupUi(helpShortcuts);
    ui.treeWidget->setColumnWidth(0, 300);
    ui.treeWidget->setColumnWidth(1, 250);
    ui.treeWidget->setColumnWidth(2, 250);
    ui.treeWidget->expandAll();
    ui.scrollAreaWidgetContents->setStyleSheet(css);
    helpShortcuts->show();
}

void MW::helpWelcome()
{
    if (G::isLogger) G::log("MW::helpWelcome");
    centralLayout->setCurrentIndex(StartTab);
}

// End MW
