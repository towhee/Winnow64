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

PROGRAM FLOW

A new folder is selected which triggers folderSelectionChange:

• Housekeeping:
  - all threads that may be running are stopped.
  - set flags to start condition.
  - folders and bookmarks synced.
  - filters are cleared (uncheckAllFilters).

• All eligible image files (and associated QFileInfo: path, name, type, size, created date and
  last modified date) in the folder are added to the DataModel (dm).

• Sort to current sort item and forward/reverse, as the datamodel always loads in the order
  from QFileInfo - by file name in forward order.

• Current row indexes set to 0.

• The first image thumbnail is selected in thumbView.

• metadataCacheThread->loadNewFolder reads all the metadata for the first chunk of images. The
  signal loadMetadataCache2ndPass is emitted.

• Based on the data collected in the first metadataCacheThread pass the icon dimension best fit
  is determined, which is required to calculate the number of icons visible in the thumbView
  and gridview.

• metadataCacheThread->loadNewFolder2ndPass reads the requisite number of icons and emits
  loadImageCache.

• The imageCacheThread is initialized.  fileSelectionChange is called for the current image
  (the user may have already advanced).

• fileSelectionChange synchronizes the views and starts three processes:
  1. ImageView::loadImage loads the full size image into the graphicsview scene.
  2. The metadataCacheThread is started to read the rest of the metadata and icons.   If the
     number of images in the folder(s) is greater than the threshold only a chunck of metadata
     is collected.
  3. The imageCacheThread is started to cache as many full size images as
     assigned memory permits.

• The first image is loaded. The metadata and thumbnail generation threads will still be
  running, but things should appear to be speedy for the user.

• The metadata caching thread collects information required by the image cache thread. If the
  number of images in the folder(s) is greater than the threshold only a chunck of metadata is
  collected.

• The image caching thread requires the offset and length for the full size embedded jpg, the
  image width and height in order to calculate memory requirements, update the image priority
  queues, the target range and limit the cache to the assigned maximum size.

A new image is selected which triggers fileSelectionChange

• If starting the program select the first image in the folder.

• Record the current datamodel row and its file path.

• Update the thumbView and gridView delegates.

• Synchronize the thumb, grid and table views.   If in loupe mode load the current image.

• Update window title, statusbar, info panel and classification badges.

• Update the metadata and image caching.

• If the metadata has not been cached yet for the selected image (usually the first in a new
  folder) then load the thumbnail.

• Update the cursor position on the image caching progress bar.

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
    MW::updateMetadataCacheIconviewState
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

When the user scrolls a datamodel view (thumbView, gridView or tableView) the scrollbar change
signal is received by thumbHasScrolled, gridHasScrolled or tableHasScrolled. The other views
are scrolled to sync with the source view midVisibleRow.  The first and last visible items are
determined in updateMetadataCacheIconviewState and the metadataCacheThread is called to update
the metadata and icons in the range (chunk).

However, when the program syncs the views this generates more scrollbar signals that would
loop.  This is prevented by the G::ignoreScrollSignal flag.  This is also employed in
fileSelectionChange, where visible views are scrolled to center on the current selection.

When the user changes modes in MW (ie from Grid to Loupe) a IconView instance (either
thumbView or gridView) can change state from hidden to visible. Since hidden widgets cannot be
accessed we need to wait until the IconView becomes visible and fully repainted before
attempting to scroll to the current index. MW::eventFilter monitors the IconView scrollBar
paint event and when the last paint event occurs the scrollToCurrent function is called. The
last paint event is identified by calculating the maximum of the scrollbar range and comparing
it to the paint event, which updates the range each time. With larger datasets (ie 1500+
thumbs) it can take a number of paint events and 100s of ms to complete. A flag is assigned
(scrollWhenReady) to show when we need to monitor so not checking needlessly. Unfortunately
there does not appear to be any signal or event when ListView is finished hence this cludge.

*/

MW::MW(const QString args, QWidget *parent) : QMainWindow(parent)
{
    if (G::isLogger) G::log(__FUNCTION__, "START APPLICATION", true);
    setObjectName("WinnowMainWindow");

    // Check if modifier key pressed while program opening
    isShift = false;
    Qt::KeyboardModifiers modifier = QGuiApplication::queryKeyboardModifiers();
    if (modifier & Qt::ShiftModifier) {
        isShift = true;
        G::isEmbellish = false;
        qDebug() << __FUNCTION__ << "isShift == true";
    }
    /* modifier & Qt::ControlModifier
    if (modifier & Qt::ControlModifier) {
        G::isLogger = true;
        G::sendLogToConsole = false;  // write to winlog.txt
//        openLog();
        qDebug() << __FUNCTION__ << "command modifier";
    }
    //*/

    // check args to see if program was started by another process (winnet)
    QString delimiter = "\n";
    QStringList argList = args.split(delimiter);
    if (argList.length() > 1) isStartupArgs = true;
    Utilities::log(__FUNCTION__, QString::number(argList.length()) + " arguments");

    /* TESTING / DEBUGGING FLAGS
       Note ISDEBUG is in globals.h
       Deactivate debug reporting by commenting ISDEBUG  */
    G::showAllTableColumns = false;     // show all table fields for debugging
    simulateJustInstalled = false;
    isStressTest = false;
    G::isTimer = true;                  // Global timer
    G::isTest = false;                  // test performance timer

    // testing control
    useImageCache = true;
    useImageView = true;
    useInfoView = true;
    useUpdateStatus = true;
    useFilterView = true;

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
    createInfoString();         // dependent on QSetting, DataModel, EmbelProperties
    createInfoView();           // dependent on DataModel, Metadata, ThumbView
    createCaching();            // dependent on DataModel, Metadata, ThumbView
    createImageView();          // dependent on centralWidget
    createCompareView();        // dependent on centralWidget
    createFSTree();             // dependent on Metadata
    createBookmarks();          // dependent on loadSettings
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
    if (isShift) {
        embelTemplatesActions.at(0)->setChecked(true);
        embelProperties->doNotEmbellish();
    }

    // if previous sort was not by filename or reverse order then sort
    updateSortColumn(sortColumn);
    if (isReverseSort) toggleSortDirection(Tog::on);
    else toggleSortDirection(Tog::off);
    if (sortColumn != G::NameColumn || isReverseSort) sortChange(__FUNCTION__);

    qRegisterMetaType<ImageCacheData::Cache>();
    qRegisterMetaType<ImageMetadata>();
    qRegisterMetaType<QVector<int>>();

    show();

    if (isStartupArgs) {
        handleStartupArgs(args);
    }
    else {
        // First use of app
        if (!isSettings) centralLayout->setCurrentIndex(StartTab);

        // process the persistant folder if available
        if (rememberLastDir && !isShift) {
            if (isFolderValid(lastDir, true, true)) {
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

//        show();

        if (setting->value("hasCrashed").toBool()) {
            int picks = pickLogCount();
            int ratings = ratingLogCount();
            int colors = colorClassLogCount();
            if (picks || ratings || colors) {
                QString msg = "It appears Winnow did not close properly.  Do you want to "
                     "recover the most recent picks and ratings?\n";
                if (picks) msg += "\n" + QString::number(picks) + " picks recoverable";
                if (ratings) msg += "\n" + QString::number(ratings) + " ratings recoverable";
                if (colors) msg += "\n" + QString::number(colors) + " color labels recoverable";
                int ret = QMessageBox::warning(this, "Recover Prior State", msg,
                                               QMessageBox::Yes | QMessageBox::No);
                if (ret == QMessageBox::Yes) {
                    folderAndFileSelectionChange(lastFileIfCrash);
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

bool MW::isDevelopment()
{
    if (G::isLogger) G::log(__FUNCTION__);
//    qDebug() << __FUNCTION__ << QCoreApplication::applicationDirPath();
    if (QCoreApplication::applicationDirPath().contains("Winnow64"))
        return true;
    else return false;
}

void MW::initialize()
{
    if (G::isLogger) G::log(__FUNCTION__);
    this->setWindowTitle(winnowWithVersion);
    G::isDev = isDevelopment();
    G::isInitializing = true;
    G::actDevicePixelRatio = 1;
    G::dpi = 96;
    G::ptToPx = G::dpi / 72;
    isNormalScreen = true;
    G::isSlideShow = false;
    workspaces = new QList<workspaceData>;
    recentFolders = new QStringList;
    ingestHistoryFolders = new QStringList;
    hasGridBeenActivated = true;
    isDragDrop = false;
    setAcceptDrops(true);
    setMouseTracking(true);
    QString msg;
    colorManageToggleBtn = new BarBtn();
    msg = "Toggle color manage on/off.";
    colorManageToggleBtn->setToolTip(msg);
    connect(colorManageToggleBtn, &BarBtn::clicked, this, &MW::toggleColorManageClick);
    cacheMethodBtn = new BarBtn();
    msg = "Toggle cache size Thrifty > Moderate > Greedy > Thrifty... \n"
          "Ctrl + Click to open cache preferences.";
    cacheMethodBtn->setToolTip(msg);
    connect(cacheMethodBtn, &BarBtn::clicked, this, &MW::toggleImageCacheMethod);
    reverseSortBtn = new BarBtn();
    reverseSortBtn ->setToolTip("Sort direction.  Shortcut to toggle: Opt/Alt + S");
    connect(reverseSortBtn, &BarBtn::clicked, this, &MW::toggleSortDirectionClick);
    filterStatusLabel = new QLabel;
    filterStatusLabel->setToolTip("The images have been filtered");
    subfolderStatusLabel = new QLabel;
    subfolderStatusLabel->setToolTip("Showing contents of all subfolders");
    rawJpgStatusLabel = new QLabel;
    rawJpgStatusLabel->setToolTip("Raw and Jpg files are combined for viewing.  "
                                  "Shortcut to toggle: Opt/Alt + J");
    slideShowStatusLabel = new QLabel;
    slideShowStatusLabel->setToolTip("Slideshow is active");
    prevCentralView = 0;
    G::labelColors << "Red" << "Yellow" << "Green" << "Blue" << "Purple";
    G::ratings << "1" << "2" << "3" << "4" << "5";
    pickStack = new QStack<Pick>;
    slideshowRandomHistoryStack = new QStack<QString>;
    scrollRow = 0;
}

void MW::setupPlatform()
{
    if (G::isLogger) G::log(__FUNCTION__);
    #ifdef Q_OS_LINIX
        G::actDevicePixelRatio = 1;
    #endif
    #ifdef Q_OS_WIN
        setWindowIcon(QIcon(":/images/winnow.png"));
        Win::collectScreensInfo();
        Win::availableMemory();
    #endif
    #ifdef Q_OS_MAC
        setWindowIcon(QIcon(":/images/winnow.icns"));
        Mac::availableMemory();
    #endif
}

//   EVENT HANDLERS

void MW::showEvent(QShowEvent *event)
{
    if (G::isLogger) G::log(__FUNCTION__);
    QMainWindow::showEvent(event);

    if (isSettings) {
        restoreGeometry(setting->value("Geometry").toByteArray());
        // run restoreGeometry second time if display has been scaled
        if (G::actDevicePixelRatio > 1.0)
            restoreGeometry(setting->value("Geometry").toByteArray());
        // correct for highdpi
//        double dpiFactor = 1.0 / G::actDevicePixelRatio;
//        resize(width() * dpiFactor, height() * dpiFactor);
//        resize(width() * G::actDevicePixelRatio, height() * G::actDevicePixelRatio);
//        #endif
        //*/
        // restoreState sets docks which triggers setThumbDockFeatures prematurely
        restoreState(setting->value("WindowState").toByteArray());
        /*
        // do not start with filterDock open
//        if (filterDock->isVisible()) {
//            folderDock->raise();
//            folderDockVisibleAction->setChecked(true);
//        }
        //*/
        updateState();
        /*
        // if embel dock visible then set mode to embelView
//        embelDockVisibilityChange();    // rgh not being used
        //*/
    }
    else {
        defaultWorkspace();
    }

    G::newPopUp(this, centralWidget);

    // set initial visibility
    embelTemplateChange(embelProperties->templateId);
    // size columns after show if device pixel ratio > 1
    embelProperties->resizeColumns();

    // check for updates
    if(checkIfUpdate && !isStartupArgs) QTimer::singleShot(100, this, SLOT(checkForUpdate()));

    // show image count in folder panel
    QString src = __FUNCTION__;
    QTimer::singleShot(50, [this, src]() {fsTree->getVisibleImageCount(src);});

    // get the monitor screen for testing against movement to an new screen in setDisplayResolution()
    QPoint loc = centralWidget->window()->geometry().center();
    prevScreenName = qApp->screenAt(loc)->name();
//    currScreen = qApp->screenAt(loc);
//    connect(currScreen, &QScreen::logicalDotsPerInchChanged, this, &MW::setDisplayResolution);

    if (isShift) refreshFolders();

    // initial status bar icon state
    updateStatusBar();

//    // set initial visibility
//    embelTemplateChange(embelProperties->templateId);
//    // size columns after show if device pixel ratio > 1
//    embelProperties->resizeColumns();

    G::isInitializing = false;
//    thumbView->selectFirst();
}

void MW::closeEvent(QCloseEvent *event)
{
    if (G::isLogger) G::log(__FUNCTION__);

    // do not allow if there is a background ingest in progress
    if (G::isRunningBackgroundIngest) {
        QString msg =
                "There is a background ingest in progress.  When it<br>"
                "has completed Winnow will close."
                ;
        G::popUp->showPopup(msg, 0);
        while (G::isRunningBackgroundIngest) G::wait(100);
    }

    setCentralMessage("Closing Winnow ...");
    metadataCacheThread->stopMetadataCache();
    imageCacheThread->stopImageCache();
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

    delete workspaces;
    delete recentFolders;
    delete ingestHistoryFolders;
    delete embel;
    event->accept();
}

void MW::ingestFinished()
{
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
    if (G::isLogger) G::log(__FUNCTION__);
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

void MW::moveEvent(QMoveEvent *event)
{
/*
    When the main winnow window is moved the zoom dialog, if it is open, must be moved as
    well. Also we need to know if the app has been dragged onto another monitor, which may
    have different dimensions and a different icc profile (win only).
*/
    QMainWindow::moveEvent(event);
    setDisplayResolution();
    updateDisplayResolution();
    emit resizeMW(this->geometry(), centralWidget->geometry());
}

void MW::resizeEvent(QResizeEvent *event)
{
    if (G::isLogger) G::log(__FUNCTION__);
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
//    qDebug() << __FUNCTION__ << event;
}

void MW::keyPressEvent(QKeyEvent *event)
{
    if (G::isLogger) G::log(__FUNCTION__);

    // must process first
    QMainWindow::keyPressEvent(event);

    if (event->key() == Qt::Key_Return) {
        loupeDisplay();
    }
}

void MW::keyReleaseEvent(QKeyEvent *event)
{
    if (G::isLogger) G::log(__FUNCTION__);
    if (event->key() == Qt::Key_Escape) {
        /* Cancel the current operation without exiting from full screen mode.  If no current
           operation, then okay to exit full screen.  escapeFullScreen must be the last option
           tested.
        */
        G::popUp->hide();
        if (!G::isNewFolderLoaded) stopAndClearAll();
        // cancel slideshow
        else if (G::isSlideShow) slideShow();
        // quit loading datamodel
        else if (dm->loadingModel) dm->timeToQuit = true;
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
        // end stress test
        else if (isStressTest) isStressTest = false;
        // exit full screen mode
        else if (fullScreenAction->isChecked()) escapeFullScreen();
    }

    if (G::isSlideShow) {
        int n = event->key() - 48;
        QVector<int> delay {0,1,2,3,5,10,30,60,180,600};

        if (event->key() == Qt::Key_Backspace) {
            isSlideshowPaused = false;
            prevRandomSlide();
        }
        else if (event->key() == Qt::Key_W) {
            isSlideShowWrap = !isSlideShowWrap;
            isSlideshowPaused = false;
            QString msg;
            if (isSlideShowWrap) msg = "Slide wrapping is on.";
            else msg = "Slide wrapping is off.";
            G::popUp->showPopup(msg);
        }
        else if (event->key() == Qt::Key_H) {
            isSlideshowPaused = true;
            slideshowHelpMsg();
        }
        else if (event->key() == Qt::Key_Space) {
            isSlideshowPaused = !isSlideshowPaused;
            QString msg;
            if (isSlideshowPaused) msg = "Slideshow is paused";
            else msg = "Slideshow is restarted";
            G::popUp->showPopup(msg);
        }
        else if (event->key() == Qt::Key_R) {
            isSlideShowRandom = !isSlideShowRandom;
            isSlideshowPaused = false;
            slideShowResetSequence();
            QString msg;
            if (isSlideShowRandom) msg = "Random selection enabled.";
            else msg = "Sequential selection enabled.";
            G::popUp->showPopup(msg);
        }
        // quick change slideshow delay 1 - 9 seconds
        else if (n > 0 && n <= 9) {
            isSlideshowPaused = false;
            slideShowDelay = delay[n];
            slideShowResetDelay();
            QString msg = "Slideshow interval set to " + QString::number(slideShowDelay) + " seconds.";
            G::popUp->showPopup(msg);
        }
//        else {
//            isSlideshowPaused = true;
//            slideshowHelpMsg();
//        }
    }

    QMainWindow::keyReleaseEvent(event);
}

//bool MW::event(QEvent *event)
//{
//    return QMainWindow::event(event);
//}

bool MW::eventFilter(QObject *obj, QEvent *event)
{
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
        qDebug() << __FUNCTION__
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
        G::log(__FUNCTION__, "Performance profiling");
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
            qDebug() << __FUNCTION__
                     << event << "\t"
                     << event->type() << "\t"
                     << obj << "\t"
                     << obj->objectName();
//            qDebug() << event;
        }
    }
//    */

    /* EMBEL DOCK TITLE ***********************************************************************

    Set dock title if not tabified

    if (obj->objectName() == "embelDock" && event->type() == QEvent::Enter) {
        qDebug() << event;
        if (!embelDockTabActivated) embelTitleBar->setTitle("Embellish");
        else embelTitleBar->setTitle("");
        embelDockTabActivated = false;
    }
    */


    /* CONTEXT MENU ***************************************************************************

    Intercept context menu

    Intercept context menu to enable/disable:
    - eject usb drive menu item
    - add bookmarks menu item
    mouseOverFolder is used in folder related context menu actions instead of
    currentViewDir
    */

    if (event->type() == QEvent::ContextMenu) {
        addBookmarkAction->setEnabled(true);
        revealFileActionFromContext->setEnabled(true);
        copyPathActionFromContext->setEnabled(true);
        if(obj == fsTree->viewport()) {
            QContextMenuEvent *e = static_cast<QContextMenuEvent *>(event);
            QModelIndex idx = fsTree->indexAt(e->pos());
            mouseOverFolder = idx.data(QFileSystemModel::FilePathRole).toString();
            enableEjectUsbMenu(mouseOverFolder);
            // in folders or bookmarks but not on folder item
            if(mouseOverFolder == "") {
                addBookmarkAction->setEnabled(false);
                revealFileActionFromContext->setEnabled(false);
                copyPathActionFromContext->setEnabled(false);
            }
        }
        else if(obj == bookmarks->viewport()) {
            QContextMenuEvent *e = static_cast<QContextMenuEvent *>(event);
            QModelIndex idx = bookmarks->indexAt(e->pos());
            mouseOverFolder = idx.data(Qt::ToolTipRole).toString();
            enableEjectUsbMenu(mouseOverFolder);
            // in folders or bookmarks but not on folder item
            if(mouseOverFolder == "") {
                addBookmarkAction->setEnabled(false);
                revealFileActionFromContext->setEnabled(false);
                copyPathActionFromContext->setEnabled(false);
            }
        }
        else {
            enableEjectUsbMenu(currentViewDirPath);
            if(currentViewDirPath == "") {
                addBookmarkAction->setEnabled(false);
                revealFileActionFromContext->setEnabled(false);
                copyPathActionFromContext->setEnabled(false);
            }
        }
    }

    /* THUMBVIEW ZOOMCURSOR **************************************************************
    Turn the cursor into a frame showing the ImageView zoom amount in the thumbnail.
    */

    if (obj == thumbView->viewport() && event->type() == QEvent::MouseMove) {
        QMouseEvent *e = static_cast<QMouseEvent *>(event);
        const QModelIndex idx = thumbView->indexAt(e->pos());
        if (idx.isValid()) {
            thumbView->zoomCursor(idx, /*forceUpdate=*/false, e->pos());
        }
    }

    /* DOCK TAB TOOLTIPS *****************************************************************
    Show a tooltip for docked widget tabs.
    */
    static int prevTabIndex = -1;
    if (QString(obj->metaObject()->className()) == "QTabBar") {
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
                QPoint locPos = geometry().topLeft() + e->pos() + QPoint(0, h*2);
                QToolTip::hideText();
                QToolTip::showText(locPos, tip, tabBar);
            }
        }
        if (event->type() == QEvent::Leave) {
            prevTabIndex = -1;
        }
    }

    /* THUMBDOCK SPLITTER ****************************************************************

    A splitter resize of top/bottom thumbDock is happening:

    Events are filtered from qApp here by an installEventFilter in the MW contructor to
    monitor the splitter resize of the thumbdock when it is docked horizontally. In this
    situation, as the vertical height of the thumbDock changes the size of the thumbnails is
    modified to fit the thumbDock by calling thumbsFitTopOrBottom. The mouse events determine
    when a mouseDrag operation is happening in combination with thumbDock resizing. The
    thumbDock is referenced from the parent because thumbView is a friend class to MW.
    */

    if (event->type() == QEvent::MouseButtonPress) {
        QMouseEvent *e = static_cast<QMouseEvent *>(event);
        if (e->button() == Qt::LeftButton) isLeftMouseBtnPressed = true;
    }

    if (event->type() == QEvent::MouseButtonRelease) {
        QMouseEvent *e = static_cast<QMouseEvent *>(event);
        if (e->button() == Qt::LeftButton) {
            isLeftMouseBtnPressed = false;
            isMouseDrag = false;
        }
    }

    if (event->type() == QEvent::MouseMove && obj->objectName() == "WinnowMainWindowWindow") {
//        qDebug() << __FUNCTION__ << "MouseMove" << obj->objectName();
        if (isLeftMouseBtnPressed) isMouseDrag = true;
    }

    if (event->type() == QEvent::MouseButtonDblClick) {
        isMouseDrag = false;
    }

    // make thumbs fit the resized thumb dock
    if (obj == thumbDock) {
        if (event->type() == QEvent::Resize) {
            if (isMouseDrag) {
                if (!thumbDock->isFloating()) {
                    Qt::DockWidgetArea area = dockWidgetArea(thumbDock);
                    if (area == Qt::BottomDockWidgetArea
                    || area == Qt::TopDockWidgetArea
                    || !thumbView->isWrapping()) {
//                        QTimer::singleShot(250, thumbView, SLOT(thumbsFitTopOrBottom()));
                        thumbView->thumbsFitTopOrBottom();
                    }
                }
            }
        }
    }

    return QWidget::eventFilter(obj, event);
}

void MW::focusChange(QWidget *previous, QWidget *current)
{
    if (G::isLogger) {
        QString s;
        if (previous != nullptr) s = "Previous = " + previous->objectName();
        if (current != nullptr) s += " Current = " + current->objectName();
        G::log(__FUNCTION__, s);
    }
//    qDebug() << __FUNCTION__ << previous << current;
    if (current == nullptr) return;
    if (current->objectName() == "DisableGoActions") enableGoKeyActions(false);
    else enableGoKeyActions(true);
    if (previous == nullptr) return;    // suppress compiler warning
}

void MW::resetFocus()
{
    if (G::isLogger) G::log(__FUNCTION__);
    activateWindow();
    setFocus();
}

// DRAG AND DROP

void MW::dragEnterEvent(QDragEnterEvent *event)
{
    if (G::isLogger) G::log(__FUNCTION__);
    event->acceptProposedAction();
}

void MW::dropEvent(QDropEvent *event)
{
    if (G::isLogger) G::log(__FUNCTION__);
    QString fPath = event->mimeData()->urls().at(0).toLocalFile();
    QFileInfo info(fPath);
    QDir incoming = info.dir();
    qDebug() << __FUNCTION__ << fPath;
    if (incoming != currentViewDirPath && event->mimeData()->hasUrls())
        folderAndFileSelectionChange(fPath);
}

void MW::handleDrop(QString fPath)
//void MW::handleDrop(QDropEvent *event)
//void MW::handleDrop(const QMimeData *mimeData)
{
    if (G::isLogger) G::log(__FUNCTION__);
//    QFileInfo info(event->mimeData()->urls().at(0).toLocalFile());
    QFileInfo info(fPath);
    QDir incoming = info.dir();
    QDir current(currentViewDirPath);
    qDebug() << __FUNCTION__ << fPath;
    bool isSameFolder = incoming == current;
    if (!isSameFolder) folderAndFileSelectionChange(fPath);
}

void MW::checkForUpdate()
{
/* Called from the help menu and from the main window show event if (isCheckUpdate = true)
   The Qt maintenancetool, an executable in the Winnow folder, checks to see if there is an
   update on the Winnow server.  If there is then Winnow is closed and the maintenancetool
   performs the install of the update.  When that is completed the maintenancetool opens
   Winnow again.
*/
    if (G::isLogger) G::log(__FUNCTION__);
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
//    qDebug() << __FUNCTION__ << "maintenancePathToUse" << maintenancePathToUse;

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
    qDebug() << __FUNCTION__ << "process.error() =" << process.error();
    qDebug() << __FUNCTION__ << "noUpdataAvailable =" << noUpdataAvailable;
    qDebug() << __FUNCTION__ << "data =" << data;
    qDebug() << __FUNCTION__ << "exitCode() =" << process.exitCode();
    qDebug() << __FUNCTION__ << "readAllStandardError() =" << process.readAllStandardError();

    Utilities::log(__FUNCTION__, "process.error() = " + QString::number(process.error()));
    Utilities::log(__FUNCTION__, "noUpdataAvailable = " + noUpdataAvailable.toString());
    Utilities::log(__FUNCTION__, "data = " + data);
    Utilities::log(__FUNCTION__, "readAllStandardError() = " + process.readAllStandardError());
    Utilities::log(__FUNCTION__, "exitCode() = " + QString::number(process.exitCode()));
//    */
    G::log(__FUNCTION__, "after check");

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
       arg[1+] = path to each image to view in Winnow.  Only arg[1] is used to determine the
       directory to open in Winnow.

    Winnets are small executables that act like photoshop droplets. They reside in
    QStandardPaths::AppDataLocation (Windows: user/AppData/Roaming/Winnow/Winnets). They send
    a list of files and a template name to Winnow to be embellished. For example, in order for
    Winnow to embellish a series of files that have been exported from lightroom, Winnow needs
    to know which embellish template to use. Instead of sending the files directly to Winnow,
    thay are sent to an intermediary program (a Winnet) that is named after the template. The
    Winnet (ie Zen2048) receives the list of files, inserts the strings "Embellish" and the
    template name "Zen2048" and then resends to Winnow.
*/
    if (G::isLogger) G::log(__FUNCTION__, args);

    if (args.length() < 2) return;
    QString delimiter = "\n";
    QStringList argList = args.split(delimiter);
    /*
    QString a = "";
    for (QString s : argList) a += s + "\n";
    QMessageBox::information(this, "MW::handleStartupArgs", a);
    //*/
    Utilities::log("MW::handleStartupArgs Winnow Location", qApp->applicationDirPath());
    Utilities::log("MW::handleStartupArgs", args);

    /* log
    Utilities::log(__FUNCTION__, QString::number(argList.length()) + " arguments");
    Utilities::log(__FUNCTION__, args);
    //*/

    QStringList pathList;
    QString templateName;
    if (argList.at(1) == "Embellish") {
        /* show main window now.  If we don't, then the update progress popup will not be
        visible.  If there is a significant delay, when a lot of images have to be processed,
        this would be confusing for the user.  */
        show();
        qApp->processEvents();
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
        Utilities::log(__FUNCTION__, "Template to use: " + templateName);
        //*/

        // get the folder where the files to embellish are located
        QFileInfo info(argList.at(3));
        QString folderPath = info.dir().absolutePath();
//        QMessageBox::information(this, "MW::handleStartupArgs", folderPath);
//        Utilities::log("MW::handleStartupArgs", args);

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
            qDebug() << __FUNCTION__
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
            Utilities::log(__FUNCTION__, msg);
            //*/
        }

        /* log
        Utilities::log(__FUNCTION__, QString::number(dir.entryInfoList().size()) + " files " +
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
                Utilities::log(__FUNCTION__, msg);
                //*/
            }
        }

        // create an instance of EmbelExport and process the incoming
        EmbelExport embelExport(metadata, dm, icd, embelProperties);

        // get the location for the embellished files
        QString fPath = embelExport.exportRemoteFiles(templateName, pathList);
        info.setFile(fPath);
        QString fDir = info.dir().absolutePath();
        fsTree->getImageCount(fDir, true, __FUNCTION__);
        // go there ...
        fsTree->select(fDir);
        folderAndFileSelectionChange(fPath);
//        loupeDisplay();
//        thumbView->selectThumb(fPath);
    }
    else {
        QFileInfo f(argList.at(1));
        f.dir().path();
        fsTree->select(f.dir().path());
        folderSelectionChange();
    }
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
    if (G::isLogger) G::log(__FUNCTION__);
//    qDebug() << __FUNCTION__ << currentViewDirPath;
    if (currentViewDirPath == "") return;
    QFileInfo info(currentViewDirPath);
    if (info.exists()) return;
    folderSelectionChange();
}

void MW::folderSelectionChange()
{
/*
   This is invoked when there is a folder selection change in the folder or bookmark views.
   See PROGRAM FLOW at top of file for more information.
*/
    // ignore if selection change triggered by deletion of prior selected folder
    if (ignoreFolderSelectionChange) {
        ignoreFolderSelectionChange = false;
        fsTree->selectionModel()->clear();
        return;
    }

    currentViewDirPath = getSelectedPath();
    setting->setValue("lastDir", currentViewDirPath);

    if (G::isLogger || G::isFlowLogger) {
        G::log(__FUNCTION__, currentViewDirPath);
    }

    stopAndClearAll("folderSelectionChange");

    // do not embellish
    if (turnOffEmbellish) embelProperties->invokeFromAction(embelTemplatesActions.at(0));

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
    if (!isFolderValid(currentViewDirPath, true /*report*/, false /*isRemembered*/)) {
        stopAndClearAll();
        G::isInitializing = false;
        setWindowTitle(winnowWithVersion);
        if (G::isLogger) Utilities::log(__FUNCTION__, "Invalid folder " + currentViewDirPath);
        return;
    }

    // sync the favs / bookmarks with the folders view fsTree
    bookmarks->select(currentViewDirPath);

    // add to recent folders
    addRecentFolder(currentViewDirPath);

    // sync the folders tree with the current folder
    fsTree->scrollToCurrent();

    // update menu
    enableEjectUsbMenu(currentViewDirPath);

    /* We do not want to update the imageCache while metadata is still being loaded. The
    imageCache update is triggered in fileSelectionChange, which is also executed when the
    change file event is fired.
    */
    metadataLoaded = false;

    /* The first time a folder is selected the datamodel is loaded with all the
    supported images.  If there are no supported images then do some cleanup. If
    this function has been executed by the load subfolders command then all the
    subfolders will be recursively loaded into the datamodel.
    */

    uncheckAllFilters();

//    if (G::isTest) testTime.restart();

    if (!dm->load(currentViewDirPath, subFoldersAction->isChecked())) {
        qWarning() << "Datamodel Failed To Load for" << currentViewDirPath;
        stopAndClearAll();
        enableSelectionDependentMenus();
        if (dm->timeToQuit) {
            updateStatus(false, "Image loading has been cancelled", __FUNCTION__);
            setCentralMessage("Image loading has been cancelled");
            return;
        }
        QDir dir(currentViewDirPath);
        if (dir.isRoot()) {
            updateStatus(false, "No supported images in this drive", __FUNCTION__);
            setCentralMessage("The root folder \"" + currentViewDirPath + "\" does not have any eligible images");
        }
        else {
            updateStatus(false, "No supported images in this folder", __FUNCTION__);
            setCentralMessage("The folder \"" + currentViewDirPath + "\" does not have any eligible images");
        }
        G::isInitializing = false;
        return;
    }

    // update FSTree count column for folder in case it has changed
    fsTree->updateFolderImageCount(currentViewDirPath);

    /* update sort if necessary (DataModel loads sorted by name in ascending order)
    qDebug() << __FUNCTION__ << "Sort new folder if necessary"
             << "sortColumn =" << sortColumn
             << "sortReverseAction->isChecked() =" << sortReverseAction->isChecked();
    //*/
    if (sortColumn != G::NameColumn || sortReverseAction->isChecked()) {
        setCentralMessage("Sorting the data model.");
//        qApp->processEvents();
        sortChange("folderSelectionChange");
        setCentralMessage("");
    }

    // datamodel loaded - initialize indexes
    currentRow = 0;
    currentSfIdx = dm->sf->index(currentRow, 0);
    dm->currentRow = currentRow;
    currentDmIdx = dm->sf->mapToSource(currentSfIdx);

    // made it this far, folder must have eligible images and is good-to-go
    isCurrentFolderOkay = true;

    // folder change triggered by dragdrop event
    bool dragFileSelected = false;
    if (isDragDrop) {
        if (dragDropFilePath.length() > 0) {
            QFileInfo info(dragDropFilePath);
            QString fileType = info.suffix().toLower();
            if (metadata->supportedFormats.contains(fileType)) {
                thumbView->selectThumb(dragDropFilePath);
                dragFileSelected = true;
            }
        }
        isDragDrop = false;
    }

    if (!dragFileSelected) {
        thumbView->selectThumb(0);
    }

    // Load folder progress
    setCentralMessage("Gathering metadata and thumbnails for images in folder.");
//    qApp->processEvents();
    updateStatus(false, "Collecting metadata for all images in folder(s)", __FUNCTION__);

    /* Must load metadata first, as it contains the file offsets and lengths for the thumbnail
    and full size embedded jpgs and the image width and height, req'd in imageCache to manage
    cache max size. The metadataCaching thread also loads the thumbnails. It triggers the
    loadImageCache when it is finished. The image cache is limited by the amount of memory
    allocated.

    While still initializing, the window show event has not happened yet, so the
    thumbsPerPage, used to figure out how many icons to cache, is unknown. 250 is the default.
    */

    // rgh isRefreshingDM ??
    metadataCacheThread->loadNewFolder(isRefreshingDM);

    // format pickMemSize as bytes, KB, MB or GB
    pickMemSize = Utilities::formatMemory(memoryReqdForPicks());
    updateStatus(true, "", __FUNCTION__);

    G::isInitializing = false;
}

void MW::fileSelectionChange(QModelIndex current, QModelIndex /*previous*/)
{
/*
    Triggered when file selection changes (folder change selects new image, so it also
    triggers this function). The new image is loaded, the pick status is updated and the
    infoView metadata is updated. The imageCache is updated if necessary. The imageCache will
    not be updated if triggered by folderSelectionChange since a new one will be Update. The
    metadataCache is updated to include metadata and icons for all the visible thumbnails.

    Note that the datamodel includes multiple columns for each row and the index sent to
    fileSelectionChange could be for a column other than 0 (from tableView) so scrollTo and
    delegate use of the current index must check the column.
*/
    if (G::isLogger) G::log(__FUNCTION__, current.data(G::PathRole).toString());
    if (G::stop) return;

   /*
    qDebug() << __FUNCTION__
             << "G::isInitializing =" << G::isInitializing
             << "G::isNewFolderLoaded =" << G::isNewFolderLoaded
             << "isFirstImageNewFolder =" << imageView->isFirstImageNewFolder
             << "isFilterChange =" << isFilterChange
             << "isCurrentFolderOkay =" << isCurrentFolderOkay
             << "row =" << current.row()
             << "icon row =" << thumbView->currentIndex().row()
             << dm->sf->index(current.row(), 0).data(G::PathRole).toString()
                ;
                //*/
//    bool isStart = false;

    if(!isCurrentFolderOkay
            || G::isInitializing
            || isFilterChange
            || !G::isNewFolderLoaded)
        return;

    if (!currentViewDir.exists()) {
        refreshFolders();
        folderSelectionChange();
        return;
    }

    // if starting program, set first image to display
    if (current.row() == -1) {
//        isStart = true;
        thumbView->selectThumb(0);
        return;
    }

    // Check if anything selected.  If not disable menu items dependent on selection
    enableSelectionDependentMenus();

//    if (isStart) return;

    if (G::isFlowLogger) G::log(__FUNCTION__, current.data(G::PathRole).toString());
    /*
    if (isDragDrop && dragDropFilePath.length() > 0) {
        thumbView->selectThumb(dragDropFilePath);
        isDragDrop = false;
    }
    */

    // user clicks outside thumb but inside treeView dock
//    QModelIndexList selected = selectionModel->selectedIndexes();
//    if (selected.isEmpty() && !G::isInitializing) return;

    // record current proxy row (dm->sf) as it is used to sync everything
    currentRow = current.row();
    // also record in datamodel so can be accessed by MdCache
    // proxy index for col 0
    currentSfIdx = dm->sf->index(currentRow, 0);
    dm->currentRow = currentRow;
    currentDmIdx = dm->sf->mapToSource(currentSfIdx);
    // the file path is used as an index in ImageView
    QString fPath = currentSfIdx.data(G::PathRole).toString();
    // also update datamodel, used in MdCache
    dm->currentFilePath = fPath;
    setting->setValue("lastFileSelection", fPath);
//    G::track(__FUNCTION__, "icd->imCache.find(fPath, image) " + fPath);

    // update delegates so they can highlight the current item
    thumbView->iconViewDelegate->currentRow = currentRow;
    gridView->iconViewDelegate->currentRow = currentRow;

    // don't scroll mouse click source (screws up double clicks and disorients users)
    if(G::fileSelectionChangeSource == "TableMouseClick") {
        G::ignoreScrollSignal = true;
        if (gridView->isVisible()) gridView->scrollToRow(currentRow, __FUNCTION__);
        if (thumbView->isVisible()) thumbView->scrollToRow(currentRow, __FUNCTION__);
    }

    else if(G::fileSelectionChangeSource == "ThumbMouseClick") {
        G::ignoreScrollSignal = true;
        if (gridView->isVisible()) gridView->scrollToRow(currentRow, __FUNCTION__);
        if (tableView->isVisible()) tableView->scrollToRow(currentRow, __FUNCTION__);
    }

    else if(G::fileSelectionChangeSource == "GridMouseClick") {
        G::ignoreScrollSignal = true;
        if (thumbView->isVisible()) thumbView->scrollToRow(currentRow, __FUNCTION__);
        if (tableView->isVisible()) tableView->scrollToRow(currentRow, __FUNCTION__);
    }

    else {
        if (gridView->isVisible()) gridView->scrollToRow(currentRow, __FUNCTION__);
        if (thumbView->isVisible()) thumbView->scrollToRow(currentRow, __FUNCTION__);
        if (tableView->isVisible()) tableView->scrollToRow(currentRow, __FUNCTION__);
    }

    // reset table, grid or thumb item clicked
    G::fileSelectionChangeSource = "";
    G::ignoreScrollSignal = false;

    if (G::isSlideShow && isSlideShowRandom) metadataCacheThread->stopMetadataCache();

    // updates ********************************************************************************

    // new file name appended to window title
    setWindowTitle(winnowWithVersion + "   " + fPath);

    if (!G::isSlideShow) progressLabel->setVisible(isShowCacheProgressBar);

    /*
    // update imageView immediately unless weird sort or folder not loaded yet
    qDebug() << __FUNCTION__
             << "sortColumn =" << sortColumn
             << "G::NameColumn =" << G::NameColumn
             << "G::ModifiedColumn =" << G::ModifiedColumn
                ;
    bool okToLoadImage = (sortColumn == G::NameColumn
                      || sortColumn == G::ModifiedColumn);
    if (okToLoadImage && useImageView) {
    */
    if (useImageView) {
        if (imageView->loadImage(fPath, __FUNCTION__)) {
            updateClassification();
            centralLayout->setCurrentIndex(prevCentralView);
        }
    }

//    G::track(__FUNCTION__, "Return from imageView->loadImage " + fPath);
    // update caching if folder has been loaded
    if (G::isNewFolderLoaded) {
        fsTree->scrollToCurrent();          // req'd for first folder when Winnow opens
        updateIconsVisible(true);
        metadataCacheThread->fileSelectionChange();
//        G::track(__FUNCTION__, "Return from metadataCacheThread->fileSelectionChange() " + fPath);

        // Do not image cache if there is an active random slide show or a
        // modifier key is pressed
        // (turn off image caching for testing with useImageCache = false. set in header)
        Qt::KeyboardModifiers key = QApplication::queryKeyboardModifiers();
        /*
        qDebug() << __FUNCTION__ << "IMAGECACHE"
                 << "isNoModifier =" << (key == Qt::NoModifier)
                 << "isShiftModifier =" << (key == Qt::ShiftModifier)
                 << "isControlModifier =" << (key == Qt::ControlModifier)
                 << "isAltModifier =" << (key == Qt::AltModifier)
                 << "isShiftAltModifier =" << (key == (Qt::AltModifier | Qt::ShiftModifier))
                    ;
        //*/
        if (G::isNewFolderLoaded
            && !(G::isSlideShow && isSlideShowRandom)
            && (key == Qt::NoModifier || key == Qt::KeypadModifier || workspaceChanged)
            && (G::mode != "Compare")
            && useImageCache
           )
        {
            emit setImageCachePosition(dm->currentFilePath);
        }
    }

    workspaceChanged = false;
    G::isNewSelection = false;

    // update the metadata panel
    if (useInfoView) infoView->updateInfo(currentRow);

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
    cacheProgressBar->updateCursor(currentRow, dm->sf->rowCount());

//    if (!G::isInitializing) centralLayout->setCurrentIndex(prevCentralView);

    if (G::isLogger) G::log(__FUNCTION__, "Finished " + fPath);
//    G::track(__FUNCTION__, "Finished " + fPath);

}

void MW::folderAndFileSelectionChange(QString fPath)
{
/*
    Loads the folder containing the image and then selects the image.  Used by
    MW::handleStartupArgs and MW::handleDrop.  After the folder change a delay is req'd
    before the initial metadata has been cached and the image can be selected.
*/
    if (G::isLogger) G::log(__FUNCTION__, fPath);
    setCentralMessage("Loading " + fPath + " ...");

    embelProperties->setCurrentTemplate("Do not Embellish");
    QFileInfo info(fPath);
    QString folder = info.dir().absolutePath();
    /*
    qDebug() << __FUNCTION__
             << "isStartupArgs =" << isStartupArgs
             << "folder =" << folder
             << "currentViewDir =" << currentViewDir
                ;
                //*/
    /*
    if (!isStartupArgs && (folder == currentViewDir)) {
        qDebug() << __FUNCTION__ << "returning: isStartupArgs =" << isStartupArgs;
        return;
    }
    //*/

    if (!fsTree->select(folder)) {
        qWarning() << __FUNCTION__ << "fsTree failed to select" << fPath;
        return;
    }

    // path to image, used in loadImageCacheForNewFolder to select image
    folderAndFileChangePath = fPath;
    folderSelectionChange();

    centralLayout->setCurrentIndex(LoupeTab);
    thumbView->selectionModel()->clear();
    return;
}

void MW::stopAndClearAll(QString src)
{
/*
    Called when folderSelectionChange and invalid folder (no folder, no eligible images).
    Can be triggered when the user picks a folder in the folder panel or open menu, picks
    a bookmark or ejects a drive and the resulting folder does not have any eligible images.
*/
    if (G::isLogger) G::log(__FUNCTION__);
//    qDebug() << __FUNCTION__ << "COMMENCE STOPANDCLEARALL";

    G::stop = true;

    imageView->clear();
    setCentralMessage("Halting folder load.");
    setWindowTitle(winnowWithVersion);
    G::isNewFolderLoaded = false;
    G::allMetadataLoaded = false;
    G::isNewFolderLoadedAndInfoViewUpToDate = false;
    // Stop any threads that might be running.
    metadataCacheThread->stopMetadataCache();
    imageCacheThread->stopImageCache();
    buildFilters->stop();
    imageView->clear();
    if (useInfoView) {
        infoView->clearInfo();
        updateDisplayResolution();
    }
    setThreadRunStatusInactive();                      // turn thread activity buttons gray
    isDragDrop = false;

    updateStatus(false, "", __FUNCTION__);
    cacheProgressBar->clearProgress();
    progressLabel->setVisible(false);
    updateClassification();
    selectionModel->clear();
    dm->clearDataModel();
    currentRow = 0;

    if (src == "folderSelectionChange")
        setCentralMessage("Loading folder.");
    else
        setCentralMessage("Select a folder.");
    G::stop = false;
}

void MW::nullFiltration()
{
    if (G::isLogger) G::log(__FUNCTION__);
    updateStatus(false, "No images match the filtration", __FUNCTION__);
    setCentralMessage("No images match the filtration");
    infoView->clearInfo();
    imageView->clear();
    progressLabel->setVisible(false);
    isDragDrop = false;
}

bool MW::isCurrentThumbVisible()
{
/*
    rgh not being used

    This function is used to determine if it is worthwhile to cache more metadata and/or
    icons. If the image/thumb that has just become the current image is already visible then
    do not invoke metadata caching.
*/
    if (G::isLogger) G::log(__FUNCTION__);
    updateIconsVisible(false);
    return (currentRow > metadataCacheThread->firstIconVisible &&
            metadataCacheThread->lastIconVisible);
}

void MW::updateIconsVisible(bool useCurrentRow)
{
/*
    Polls both thumbView and gridView to determine the first, mid and last thumbnail visible.
    This is used in the metadataCacheThread to determine the range of files to cache.
*/
    if (G::isLogger) G::log(__FUNCTION__);
    int first = dm->sf->rowCount();
    int last = 0;

    /*
    Alternate where calculate first / last for case where you need to know in advance what
    they will be, not what they are now.
    int _first;
    int _last;
    thumbView->viewportRange(currentRow, _first, _last);
    if (_first < first) first = _first;
    gridView->viewportRange(currentRow, _first, _last);
    if (_first < first) first = _first;
    */
//    // Grid might not be selected in CentralWidget
    if (G::mode == "Grid") centralLayout->setCurrentIndex(GridTab);

    if (thumbView->isVisible()) {
        if (useCurrentRow) thumbView->calcViewportRange(currentRow);
        else thumbView->scannedViewportRange();
        if (thumbView->firstVisibleCell < first) first = thumbView->firstVisibleCell;
        if (thumbView->lastVisibleCell > last) last = thumbView->lastVisibleCell;
    }

    if (gridView->isVisible()) {
        if (useCurrentRow) gridView->calcViewportRange(currentRow);
        else gridView->scannedViewportRange();
        if (gridView->firstVisibleCell < first) first = gridView->firstVisibleCell;
        if (gridView->lastVisibleCell > last) last = gridView->lastVisibleCell;
    }

    if (tableView->isVisible()) {
        tableView->setViewportParameters();
        if (tableView->firstVisibleRow < first) first = tableView->firstVisibleRow;
        if (tableView->lastVisibleRow > last) last = tableView->lastVisibleRow;
    }

    /* qDebug()
         << __FUNCTION__
         << "\n\tthumbView->firstVisibleCell =" << thumbView->firstVisibleCell
         << "thumbView->lastVisibleCell =" << thumbView->lastVisibleCell
         << "\n\tgridView->firstVisibleCell =" << gridView->firstVisibleCell
         << "gridView->lastVisibleCell =" << gridView->lastVisibleCell
         << "\n\ttableView->firstVisibleCell =" << tableView->firstVisibleRow
         << "tableView->lastVisibleCell =" << tableView->lastVisibleRow
         << "\n\tfirst =" << first
             << "last =" << last;
//        */

    metadataCacheThread->firstIconVisible = first;
    metadataCacheThread->midIconVisible = (first + last) / 2;// rgh qCeil ??
    metadataCacheThread->lastIconVisible = last;
    metadataCacheThread->visibleIcons = last - first + 1;

    if (G::isInitializing || !G::isNewFolderLoaded) return;
//    metadataCacheThread->sizeChange(__FUNCTION__);
}

void MW::loadMetadataCache2ndPass()
{
    if (G::isLogger || G::isFlowLogger) G::log(__FUNCTION__);
//    qDebug() << __FUNCTION__;
    updateIconBestFit();
    updateIconsVisible(true);
    metadataCacheThread->loadNewFolder2ndPass();
}

void MW::thumbHasScrolled()
{
/*
    This function is called after a thumbView scrollbar change signal.

    If the change was caused by the user scrolling then we want to process it, as defined by
    G::ignoreScrollSignal == false. However, if the scroll change was caused by syncing with
    another view then we do not want to process again and get into a loop.

    MW::updateMetadataCacheIconviewState polls setViewportParameters() in all visible views
    (thumbView, gridView and tableView) to assign the firstVisibleRow, midVisibleRow and
    lastVisibleRow in metadataCacheThread.

    The gridView and tableView, if visible, are scrolled to sync with gridView.

    Finally metadataCacheThread->scrollChange is called to load any necessary metadata and
    icons within the cache range.

    Scrolling used to use a singleshot timer triggered by MW::loadMetadataCacheAfterDelay to
    call MW::loadMetadataChunk which, in turn, finally called the metadataCacheThread. This
    was to prevent many scroll calls from bunching up. The new approach just aborts an
    existing thread and starts over. It is simpler and faster. Keeping the old process until
    the new one is proven to work all the time.
*/
    if (G::isLogger) G::log(__FUNCTION__);
    if (G::isInitializing || !G::isNewFolderLoaded) return;

    if (G::ignoreScrollSignal == false) {
        G::ignoreScrollSignal = true;
        updateIconsVisible(false);
        gridView->scrollToRow(thumbView->midVisibleCell, __FUNCTION__);
        updateIconsVisible(false);
        if (tableView->isVisible())
            tableView->scrollToRow(thumbView->midVisibleCell, __FUNCTION__);
        // only call metadataCacheThread->scrollChange if scroll without fileSelectionChange
        if (!G::isNewSelection) metadataCacheThread->scrollChange(__FUNCTION__);
        // update thumbnail zoom frame cursor
        QModelIndex idx = thumbView->indexAt(thumbView->mapFromGlobal(QCursor::pos()));
        if (idx.isValid()) {
            thumbView->zoomCursor(idx);
        }
    }
    G::ignoreScrollSignal = false;
}

void MW::gridHasScrolled()
{
/*
    This function is called after a gridView scrollbar change signal.

    If the change was caused by the user scrolling then we want to process it, as defined by
    G::ignoreScrollSignal == false. However, if the scroll change was caused by syncing with
    another view then we do not want to process again and get into a loop.

    MW::updateMetadataCacheIconviewState polls setViewportParameters() in all visible views
    (thumbView, gridView and tableView) to assign the firstVisibleRow, midVisibleRow and
    lastVisibleRow in metadataCacheThread.

    The thumbView, if visible, is scrolled to sync with gridView.

    Finally metadataCacheThread->scrollChange is called to load any necessary metadata and
    icons within the cache range.

    Scrolling used to use a singleshot timer triggered by MW::loadMetadataCacheAfterDelay
    to call MW::loadMetadataChunk which, in turn, finally called the metadataCacheThread.
    This was to prevent many scroll calls from bunching up.  The new approach just aborts an
    existing thread and starts over.  It is simpler and faster.  Keeping the old process until
    the new one is proven to work all the time.
*/
    if (G::isLogger) G::log(__FUNCTION__); 
    if (G::isInitializing || !G::isNewFolderLoaded) return;

    if (G::ignoreScrollSignal == false) {
        G::ignoreScrollSignal = true;
        updateIconsVisible(false);
        thumbView->scrollToRow(gridView->midVisibleCell, __FUNCTION__);
        // only call metadataCacheThread->scrollChange if scroll without fileSelectionChange
        if (!G::isNewSelection) metadataCacheThread->scrollChange(__FUNCTION__);
    }
    G::ignoreScrollSignal = false;
}

void MW::tableHasScrolled()
{
/*
    This function is called after a tableView scrollbar change signal.

    If the change was caused by the user scrolling then we want to process it, as defined by
    G::ignoreScrollSignal == false. However, if the scroll change was caused by syncing with
    another view then we do not want to process again and get into a loop.

    MW::updateMetadataCacheIconviewState polls setViewportParameters() in all visible views
    (thumbView, gridView and tableView) to assign the firstVisibleRow, midVisibleRow and
    lastVisibleRow in metadataCacheThread.

    The thumbView, if visible, is scrolled to sync with gridView.

    Finally metadataCacheThread->scrollChange is called to load any necessary metadata and
    icons within the cache range.

    Scrolling used to use a singleshot timer triggered by MW::loadMetadataCacheAfterDelay to
    call MW::loadMetadataChunk which, in turn, finally called the metadataCacheThread. This
    was to prevent many scroll calls from bunching up. The new approach just aborts an
    existing thread and starts over. It is simpler and faster. Keeping the old process until
    the new one is proven to work all the time.
*/
    if (G::isLogger) G::log(__FUNCTION__); 
    if (G::isInitializing || !G::isNewFolderLoaded) return;

    if (G::ignoreScrollSignal == false) {
        G::ignoreScrollSignal = true;
        updateIconsVisible(false);
        if (thumbView->isVisible())
            thumbView->scrollToRow(tableView->midVisibleRow, __FUNCTION__);
        // only call metadataCacheThread->scrollChange if scroll without fileSelectionChange
        if (!G::isNewSelection) metadataCacheThread->scrollChange(__FUNCTION__);
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
    if (G::isLogger) G::log(__FUNCTION__);
    if (G::isInitializing || !G::isNewFolderLoaded) return;
    updateIconsVisible(true);
    metadataCacheThread->sizeChange(__FUNCTION__);
}

void MW::loadMetadataCacheAfterDelay()
{
/*
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
    if (G::isLogger) G::log(__FUNCTION__);
    static int previousRow = 0;

    if (G::isInitializing || !G::isNewFolderLoaded) return;

    // has a new image been selected.  Caching will be started from MW::fileSelectionChange
    if (previousRow != currentRow) {
        previousRow = currentRow;
        return;
    }

    metadataCacheScrollTimer->start(cacheDelay);
}

void MW::loadMetadataChunk()
{
/*
    If there has not been another call to this function in cacheDelay ms then the
    metadataCacheThread is restarted at the row of the first visible thumb after the
    scrolling.
*/
    if (G::isLogger) G::log(__FUNCTION__);
    metadataCacheThread->scrollChange(__FUNCTION__);
}

void MW::loadEntireMetadataCache(QString source)
{
/*
    This is called before a filter or sort operation, which only makes sense if all the
    metadata has been loaded. This function does not load the icons. It is not run in a
    separate thread as the filter and sort operations cannot commence until all the metadata
    has been loaded.
*/
    if (G::isLogger || G::isFlowLogger) G::log(__FUNCTION__, "Source: " + source);
    qDebug() << __FUNCTION__
             << "Source: " << source
             << "G::isInitializing: " << G::isInitializing
             ;
    if (G::isInitializing) return;
    if (metadataCacheThread->isAllMetadataLoaded()) return;

    updateIconsVisible(true);

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
        G::log(__FUNCTION__, s);
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
//             */

    // show cache amount ie "4.2 of 16.1GB (4 threads)" in info panel
    QString cacheAmount = QString::number(double(cache.currMB)/1024,'f',1)
            + " of "
            + QString::number(double(cache.maxMB)/1024,'f',1)
            + "GB ("
            + QString::number(cache.decoderCount)
            + " threads)"
            ;
    if (useInfoView) {
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
            if (dm->sf->index(i, G::PathColumn).data(G::CachedRole).toBool())
                cacheProgressBar->updateProgress(i, i + 1, rows,
                                            cacheProgressBar->imageCacheColorGradient);
        }

        // cursor
        cacheProgressBar->updateCursor(currentRow, rows);
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
    if (G::isLogger) G::log(__FUNCTION__);
    gridView->bestAspect();
    thumbView->bestAspect();
}

void MW::loadImageCacheForNewFolder()
{
/*
    This function is signaled from the metadataCacheThread after the metadata chunk has been
    loaded for a new folder selection. The imageCache loads images until the assigned amount
    of memory has been consumed or all the images are cached.
*/
    if (G::isLogger || G::isFlowLogger) G::log(__FUNCTION__);
    // now that metadata is loaded populate the data model
    if(isShowCacheProgressBar) {
        cacheProgressBar->clearProgress();
//        qApp->processEvents();
    }
//    setCentralMessage("updateIconBestFit.");
    updateIconBestFit();      // rgh make sure req'd
//    setCentralMessage("updateIconBestFit done.");

    // had to wait for the data before resize table columns
    tableView->resizeColumnsToContents();
    tableView->setColumnWidth(G::PathColumn, 24+8);

    // set image cache parameters and build image cacheItemList
    int netCacheMBSize = cacheMaxMB - G::metaCacheMB;
    if (netCacheMBSize < cacheMinMB) netCacheMBSize = cacheMinMB;
    imageCacheThread->initImageCache(netCacheMBSize, cacheMinMB,
        isShowCacheProgressBar, cacheWtAhead);

    // have to wait until image caching thread running before setting flag
    metadataLoaded = true;

    G::isNewFolderLoaded = true;

    /* Trigger MW::fileSelectionChange.  This must be done to initialize many things
    including current index and file path req'd by mdCache and EmbelProperties...  If
    folderAndFileSelectionChange has been executed then folderAndFileChangePath will be
    the file to select in the new folder; otherwise the first file in dm->sf will be
    selected. */
    QString fPath = folderAndFileChangePath;
    folderAndFileChangePath = "";
    if (fPath != "" && dm->proxyIndexFromPath(fPath).isValid()) {
        thumbView->selectThumb(fPath);
        gridView->selectThumb(fPath);
        currentSfIdx = dm->proxyIndexFromPath(fPath);
    }
    else {
        thumbView->selectFirst();
        gridView->selectFirst();
        currentSfIdx = dm->sf->index(0,0);
    }
    fileSelectionChange(currentSfIdx, currentSfIdx);

    /* now okay to write to xmp sidecar, as metadata is loaded and initial updates to
       InfoView by fileSelectionChange have been completed.  Otherwise, InfoView::dataChanged
       would prematurally trigger Metadata::writeXMP */
    G::isNewFolderLoadedAndInfoViewUpToDate = true;
}

void MW::bookmarkClicked(QTreeWidgetItem *item, int col)
{
/*
    Called by signal itemClicked in bookmark.
*/
    if (G::isLogger) G::log(__FUNCTION__);
    const QString fPath =  item->toolTip(col);
    isCurrentFolderOkay = isFolderValid(fPath, true, false);

    if (isCurrentFolderOkay) {
        QModelIndex idx = fsTree->fsModel->index(fPath);
        QModelIndex filterIdx = fsTree->fsFilter->mapFromSource(idx);
        fsTree->setCurrentIndex(filterIdx);
        fsTree->scrollTo(filterIdx, QAbstractItemView::PositionAtCenter);
        folderSelectionChange();
    }
    else {
        stopAndClearAll();
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
    qDebug() << __FUNCTION__ << dirPath;
    if (G::isLogger) G::log(__FUNCTION__);
    if (G::isInitializing) return;

    if (!QDir().exists(currentViewDirPath)) {
        currentViewDirPath = "";
    }
}

QString MW::getSelectedPath()
{
    if (G::isLogger) G::log(__FUNCTION__);
    if (isDragDrop)  return dragDropFolderPath;

    if (fsTree->selectionModel()->selectedRows().size() == 0) return "";

    QModelIndex idx = fsTree->selectionModel()->selectedRows().at(0);
    if (!idx.isValid()) return "";

    QString path = idx.data(QFileSystemModel::FilePathRole).toString();
    QFileInfo dirInfo = QFileInfo(path);
    currentViewDir = dirInfo.dir();
    return dirInfo.absoluteFilePath();
}

void MW::createActions()
{
    if (G::isLogger) G::log(__FUNCTION__);

    // disable go keys when property editors have focus
    connect(qApp, &QApplication::focusChanged, this, &MW::focusChange);

    int n;          // used to populate action lists

    // File menu

    openAction = new QAction(tr("Open Folder"), this);
    openAction->setObjectName("openFolder");
    openAction->setShortcutVisibleInContextMenu(true);
    addAction(openAction);
    connect(openAction, &QAction::triggered, this, &MW::openFolder);

    refreshCurrentAction = new QAction(tr("Refresh Current Folder"), this);
    refreshCurrentAction->setObjectName("refreshFolder");
    refreshCurrentAction->setShortcutVisibleInContextMenu(true);
    addAction(refreshCurrentAction);
    connect(refreshCurrentAction, &QAction::triggered, this, &MW::refreshCurrentFolder);

    openUsbAction = new QAction(tr("Open Usb Folder"), this);
    openUsbAction->setObjectName("openUsbFolder");
    openUsbAction->setShortcutVisibleInContextMenu(true);
    addAction(openUsbAction);
    connect(openUsbAction, &QAction::triggered, this, &MW::openUsbFolder);

    openWithMenu = new QMenu(tr("Open With..."));

    openWithMenuAction = new QAction(tr("Open With..."), this);
    openWithMenuAction->setObjectName("openWithMenu");
    openWithMenuAction->setShortcutVisibleInContextMenu(true);
    addAction(openWithMenuAction);
    openWithMenuAction->setMenu(openWithMenu);

    manageAppAction = new QAction(tr("Manage External Applications"), this);
    manageAppAction->setObjectName("chooseApp");
    manageAppAction->setShortcutVisibleInContextMenu(true);
    addAction(manageAppAction);
    connect(manageAppAction, &QAction::triggered, this, &MW::externalAppManager);

    /* read external apps from QSettings */
    if (isSettings) {
        setting->beginGroup("ExternalApps");
        QStringList names = setting->childKeys();
        n = names.size();

        for (int i = 0; i < 10; ++i) {
            if (i < n) {
                /* The preferred sort order is prepended to the app names
                in writeSettings as QSettings does not honor order.  When
                loading the settings (here) we have to strip the sort char
                which is defined in the vector xAppShortcut.
                */
                QString name = names.at(i);
                externalApp.name = name.remove(0, 1);
                externalApp.path = setting->value(names.at(i)).toString();
                externalApps.append(externalApp);
            }
            else externalApp.name = "Future app" + QString::number(i);

            appActions.append(new QAction(externalApp.name, this));
            if (i < n) {
                appActions.at(i)->setShortcut(QKeySequence("Alt+" + xAppShortcut[i]));
                appActions.at(i)->setShortcutVisibleInContextMenu(true);
                appActions.at(i)->setText(externalApp.name);
                appActions.at(i)->setVisible(true);
                addAction(appActions.at(i));
            }
            if (i >= n) appActions.at(i)->setVisible(false);
            appActions.at(i)->setShortcut(QKeySequence("Alt+" + xAppShortcut[i]));
            connect(appActions.at(i), &QAction::triggered, this, &MW::runExternalApp);
        }
        addActions(appActions);
        setting->endGroup();
    }
    else {
        for (int i = 0; i < 10; ++i) {
            externalApp.name = "Future app" + QString::number(i);
            appActions.append(new QAction(externalApp.name, this));
            appActions.at(i)->setShortcut(QKeySequence("Alt+" + xAppShortcut[i]));
            connect(appActions.at(i), &QAction::triggered, this, &MW::runExternalApp);
        }
    }

    recentFoldersMenu = new QMenu(tr("Recent folders..."));
    recentFoldersAction = new QAction(tr("Recent folders..."), this);
    recentFoldersAction->setObjectName("recentFoldersAction");
    recentFoldersAction->setShortcutVisibleInContextMenu(true);
    addAction(recentFoldersAction);
    recentFoldersAction->setMenu(recentFoldersMenu);

    // general connection to handle invoking new recent folders
    // MacOS will not allow runtime menu insertions.  Cludge workaround
    // add 20 dummy menu items and then hide until use.
    n = recentFolders->count();
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
            addAction(recentFolderActions.at(i));
        }
        if (i >= n) recentFolderActions.at(i)->setVisible(false);
        // see invokeRecentFolder
//        connect(recentFolderActions.at(i), &QAction::triggered, this, &MW::openFolderFromRecent);
    }
    addActions(recentFolderActions);

    QString revealText = "Reveal";
    #ifdef Q_OS_WIN
        revealText = "Open in Explorer";
    #endif
    #ifdef Q_OS_MAC
        revealText = "Reveal in Finder";
    #endif

    saveAsFileAction = new QAction("Save Preview as...", this);
    saveAsFileAction->setObjectName("saveAs");
    saveAsFileAction->setShortcutVisibleInContextMenu(true);
    addAction(saveAsFileAction);
    connect(saveAsFileAction, &QAction::triggered, this, &MW::saveAsFile);

    revealFileAction = new QAction(revealText, this);
    revealFileAction->setObjectName("openInFinder");
    revealFileAction->setShortcutVisibleInContextMenu(true);
    addAction(revealFileAction);
    connect(revealFileAction, &QAction::triggered, this, &MW::revealFile);

    revealFileActionFromContext = new QAction(revealText, this);
    revealFileActionFromContext->setObjectName("openInFinderFromContext");
    revealFileActionFromContext->setShortcutVisibleInContextMenu(true);
    addAction(revealFileActionFromContext);
    connect(revealFileActionFromContext, &QAction::triggered, this, &MW::revealFileFromContext);

    copyPathActionFromContext = new QAction("Copy folder path", this);
    copyPathActionFromContext->setObjectName("copyPathFromContext");
    copyPathActionFromContext->setShortcutVisibleInContextMenu(true);
    addAction(copyPathActionFromContext);
    connect(copyPathActionFromContext, &QAction::triggered, this, &MW::copyFolderPathFromContext);

    subFoldersAction = new QAction(tr("Include Sub-folders"), this);
    subFoldersAction->setObjectName("subFolders");
    subFoldersAction->setShortcutVisibleInContextMenu(true);
    subFoldersAction->setCheckable(true);
    subFoldersAction->setChecked(false);
    addAction(subFoldersAction);
    connect(subFoldersAction, &QAction::triggered, this, &MW::updateStatusBar);

    refreshFoldersAction = new QAction(tr("Refresh all"), this);
    refreshFoldersAction->setObjectName("refreshFolders");
    refreshFoldersAction->setShortcutVisibleInContextMenu(true);
    addAction(refreshFoldersAction);
    connect(refreshFoldersAction, &QAction::triggered, this, &MW::refreshFolders);

    collapseFoldersAction = new QAction(tr("Collapse all folders"), this);
    collapseFoldersAction->setObjectName("collapseFolders");
    collapseFoldersAction->setShortcutVisibleInContextMenu(true);
    addAction(collapseFoldersAction);
    connect(collapseFoldersAction, &QAction::triggered, this, &MW::collapseAllFolders);

    showImageCountAction = new QAction(tr("Show image count"), this);
    showImageCountAction->setObjectName("showImageCount");
    showImageCountAction->setShortcutVisibleInContextMenu(true);
    showImageCountAction->setCheckable(true);
//    showImageCountAction->setChecked(setting->value("showImageCount").toBool());
    addAction(showImageCountAction);
    connect(showImageCountAction, &QAction::triggered, this, &MW::setShowImageCount);

    updateImageCountAction = new QAction(tr("Update image counts"), this);
    updateImageCountAction->setObjectName("updateImageCount");
    updateImageCountAction->setShortcutVisibleInContextMenu(true);
    addAction(updateImageCountAction);
    connect(updateImageCountAction, &QAction::triggered, fsTree, &FSTree::updateVisibleImageCount);

    addBookmarkAction = new QAction(tr("Add Bookmark"), this);
    addBookmarkAction->setObjectName("addBookmark");
    addBookmarkAction->setShortcutVisibleInContextMenu(true);
    addAction(addBookmarkAction);
    connect(addBookmarkAction, &QAction::triggered, this, &MW::addNewBookmarkFromMenu);

    addBookmarkActionFromContext = new QAction(tr("Add Bookmark"), this);
    addBookmarkActionFromContext->setObjectName("addBookmark");
    addBookmarkActionFromContext->setShortcutVisibleInContextMenu(true);
    addAction(addBookmarkActionFromContext);
    connect(addBookmarkActionFromContext, &QAction::triggered, this, &MW::addNewBookmarkFromContextMenu);

    removeBookmarkAction = new QAction(tr("Remove Bookmark"), this);
    removeBookmarkAction->setObjectName("removeBookmark");
    removeBookmarkAction->setShortcutVisibleInContextMenu(true);
    addAction(removeBookmarkAction);
    connect(removeBookmarkAction, &QAction::triggered, this, &MW::removeBookmark);

    refreshBookmarkAction = new QAction(tr("Refresh Bookmarks"), this);
    refreshBookmarkAction->setObjectName("refreshBookmarks");
    refreshBookmarkAction->setShortcutVisibleInContextMenu(true);
    addAction(refreshBookmarkAction);
    connect(refreshBookmarkAction, &QAction::triggered, this, &MW::refreshBookmarks);

    ingestAction = new QAction(tr("Ingest picks"), this);
    ingestAction->setObjectName("ingest");
    ingestAction->setShortcutVisibleInContextMenu(true);
    addAction(ingestAction);
    connect(ingestAction, &QAction::triggered, this, &MW::ingest);

    ingestHistoryFoldersMenu = new QMenu(tr("Ingest History folders..."));
    ingestHistoryFoldersAction = new QAction(tr("Ingest History..."), this);
    ingestHistoryFoldersAction->setObjectName("ingestHistoryFoldersAction");
    ingestHistoryFoldersAction->setShortcutVisibleInContextMenu(true);
    addAction(ingestHistoryFoldersAction);
    ingestHistoryFoldersAction->setMenu(ingestHistoryFoldersMenu);

    // general connection to add ingest history list as menu items
    // MacOS will not allow runtime menu insertions.  Cludge workaround
    // add 20 dummy menu items and then hide until use.
    n = ingestHistoryFolders->count();
    for (int i = 0; i < maxIngestHistoryFolders; i++) {
        QString name;
        QString objName = "";
        if (i < n) {
            name = ingestHistoryFolders->at(i);
            objName = "ingestHistoryFolder" + QString::number(i);
        }
        else name = "Future ingest history folder" + QString::number(i);
        ingestHistoryFolderActions.append(new QAction(name, this));
        if (i < n) {
            ingestHistoryFolderActions.at(i)->setObjectName(objName);
            ingestHistoryFolderActions.at(i)->setText(name);
            ingestHistoryFolderActions.at(i)->setVisible(true);
            addAction(ingestHistoryFolderActions.at(i));
        }
        if (i >= n) ingestHistoryFolderActions.at(i)->setVisible(false);
    }
    addActions(ingestHistoryFolderActions);

    ejectAction = new QAction(tr("Eject Usb Drive"), this);
    ejectAction->setObjectName("ingest");
    ejectAction->setShortcutVisibleInContextMenu(true);
    addAction(ejectAction);
    connect(ejectAction, &QAction::triggered, this, &MW::ejectUsbFromMainMenu);

    ejectActionFromContextMenu = new QAction(tr("Eject Usb Drive"), this);
    ejectActionFromContextMenu->setObjectName("ingest");
    ejectActionFromContextMenu->setShortcutVisibleInContextMenu(true);
    addAction(ejectActionFromContextMenu);
    connect(ejectActionFromContextMenu, &QAction::triggered, this, &MW::ejectUsbFromContextMenu);

    colorManageAction = new QAction(tr("Color manage"), this);
    colorManageAction->setObjectName("colorManage");
    colorManageAction->setShortcutVisibleInContextMenu(true);
    colorManageAction->setCheckable(true);
    colorManageAction->setChecked(G::colorManage);
    addAction(colorManageAction);
    connect(colorManageAction, &QAction::triggered, this, &MW::toggleColorManageClick);

    combineRawJpgAction = new QAction(tr("Combine Raw+Jpg"), this);
    combineRawJpgAction->setObjectName("combineRawJpg");
    combineRawJpgAction->setShortcutVisibleInContextMenu(true);
    combineRawJpgAction->setCheckable(true);
    if (isSettings && setting->contains("combineRawJpg")) combineRawJpgAction->setChecked(setting->value("combineRawJpg").toBool());
    else combineRawJpgAction->setChecked(true);
    addAction(combineRawJpgAction);
    connect(combineRawJpgAction, &QAction::triggered, this, &MW::setCombineRawJpg);

    combineRawJpgAction = new QAction(tr("Combine Raw+Jpg"), this);
    combineRawJpgAction->setObjectName("combineRawJpg");
    combineRawJpgAction->setShortcutVisibleInContextMenu(true);
    combineRawJpgAction->setCheckable(true);
    if (isSettings && setting->contains("combineRawJpg")) combineRawJpgAction->setChecked(setting->value("combineRawJpg").toBool());
    else combineRawJpgAction->setChecked(true);
    addAction(combineRawJpgAction);
    connect(combineRawJpgAction, &QAction::triggered, this, &MW::setCombineRawJpg);

    // Place keeper for now
    renameAction = new QAction(tr("Rename selected images"), this);
    renameAction->setObjectName("renameImages");
    renameAction->setShortcutVisibleInContextMenu(true);
//    renameAction->setShortcut(Qt::Key_F2);
    addAction(renameAction);
//    connect(renameAction, &QAction::triggered, this, &MW::renameImages);

    // Place keeper for now
    runDropletAction = new QAction(tr("Run Droplet"), this);
    runDropletAction->setObjectName("runDroplet");
    runDropletAction->setShortcutVisibleInContextMenu(true);
    runDropletAction->setShortcut(QKeySequence("A"));
    addAction(runDropletAction);

    reportMetadataAction = new QAction(tr("Report Metadata"), this);
    reportMetadataAction->setObjectName("reportMetadata");
    reportMetadataAction->setShortcutVisibleInContextMenu(true);
    addAction(reportMetadataAction);
    connect(reportMetadataAction, &QAction::triggered, this, &MW::reportMetadata);

    mediaReadSpeedAction = new QAction(tr("Media read speed"), this);
    mediaReadSpeedAction->setObjectName("mediaReadSpeed");
    mediaReadSpeedAction->setShortcutVisibleInContextMenu(true);
    addAction(mediaReadSpeedAction);
    connect(mediaReadSpeedAction, &QAction::triggered, this, &MW::mediaReadSpeed);

    reportHueCountAction = new QAction(tr("Report hue count"), this);
    reportHueCountAction->setObjectName("reportHueCount");
    reportHueCountAction->setShortcutVisibleInContextMenu(true);
    addAction(reportHueCountAction);
    connect(reportHueCountAction, &QAction::triggered, this, &MW::reportHueCount);

    meanStackAction = new QAction(tr("Mean stack"), this);
    meanStackAction->setObjectName("meanStack");
    meanStackAction->setShortcutVisibleInContextMenu(true);
    addAction(meanStackAction);
    connect(meanStackAction, &QAction::triggered, this, &MW::generateMeanStack);

    // Appears in Winnow menu in OSX
    exitAction = new QAction(tr("Exit"), this);
    exitAction->setObjectName("exit");
    exitAction->setShortcutVisibleInContextMenu(true);
    addAction(exitAction);
    connect(exitAction, &QAction::triggered, this, &MW::close);

    // Edit Menu

    selectAllAction = new QAction(tr("Select All"), this);
    selectAllAction->setObjectName("selectAll");
    selectAllAction->setShortcutVisibleInContextMenu(true);
    addAction(selectAllAction);
    connect(selectAllAction, &QAction::triggered, this, &MW::selectAllThumbs);

    invertSelectionAction = new QAction(tr("Invert Selection"), this);
    invertSelectionAction->setObjectName("invertSelection");
    invertSelectionAction->setShortcutVisibleInContextMenu(true);
    addAction(invertSelectionAction);
    connect(invertSelectionAction, &QAction::triggered,
            thumbView, &IconView::invertSelection);

    rejectAction = new QAction(tr("Reject"), this);
    rejectAction->setObjectName("Reject");
    rejectAction->setShortcutVisibleInContextMenu(true);
    addAction(rejectAction);
    connect(rejectAction, &QAction::triggered, this, &MW::toggleReject);

    refineAction = new QAction(tr("Refine"), this);
    refineAction->setObjectName("Refine");
    refineAction->setShortcutVisibleInContextMenu(true);
    addAction(refineAction);
    connect(refineAction, &QAction::triggered, this, &MW::refine);

    pickAction = new QAction(tr("Pick"), this);
    pickAction->setObjectName("togglePick");
    pickAction->setShortcutVisibleInContextMenu(true);
    addAction(pickAction);
    connect(pickAction, &QAction::triggered, this, &MW::togglePick);

    pick1Action = new QAction(tr("Pick"), this);  // added for shortcut "P"
    addAction(pick1Action);
    connect(pick1Action, &QAction::triggered, this, &MW::togglePick);

    pickMouseOverAction = new QAction(tr("Pick at cursor"), this);  // IconView context menu
    pickMouseOverAction->setObjectName("toggleMouseOverPick");
    pickAction->setShortcutVisibleInContextMenu(true);
    addAction(pickMouseOverAction);
    connect(pickMouseOverAction, &QAction::triggered, this, &MW::togglePickMouseOver);

    pickUnlessRejectedAction = new QAction(tr("Pick unless rejected"), this);
    pickUnlessRejectedAction->setObjectName("pickUnlessRejected");
    pickUnlessRejectedAction->setShortcutVisibleInContextMenu(true);
    addAction(pickUnlessRejectedAction);
    connect(pickUnlessRejectedAction, &QAction::triggered, this, &MW::togglePickUnlessRejected);

    filterPickAction = new QAction(tr("Filter picks only"), this);
    filterPickAction->setObjectName("toggleFilterPick");
    filterPickAction->setShortcutVisibleInContextMenu(true);
    filterPickAction->setCheckable(true);
    filterPickAction->setChecked(false);
    addAction(filterPickAction);
    connect(filterPickAction, &QAction::triggered, filters, &Filters::checkPicks);

    popPickHistoryAction = new QAction(tr("Recover prior pick state"), this);
    popPickHistoryAction->setObjectName("togglePick");
    popPickHistoryAction->setShortcutVisibleInContextMenu(true);
    addAction(popPickHistoryAction);
    connect(popPickHistoryAction, &QAction::triggered, this, &MW::popPick);

    // Copy, Cut
    deleteAction = new QAction(tr("Delete"), this);
    deleteAction->setObjectName("deleteFiles");
    deleteAction->setShortcutVisibleInContextMenu(true);
    deleteAction->setShortcut(QKeySequence("Delete"));
    addAction(deleteAction);
    connect(deleteAction, &QAction::triggered, this, &MW::deleteFiles);

    deleteActiveFolderAction = new QAction(tr("Delete Folder"), this);
    deleteActiveFolderAction->setObjectName("deleteActiveFolder");
    addAction(deleteActiveFolderAction);
    connect(deleteActiveFolderAction, &QAction::triggered, this, &MW::deleteFolder);

    // not being used due to risk of folder containing many subfolders with no indication to user
    deleteBookmarkFolderAction = new QAction(tr("Delete Folder"), this);
    deleteBookmarkFolderAction->setObjectName("deleteBookmarkFolder");
    addAction(deleteBookmarkFolderAction);
    connect(deleteBookmarkFolderAction, &QAction::triggered, this, &MW::deleteFolder);

    deleteFSTreeFolderAction = new QAction(tr("Delete Folder"), this);
    deleteFSTreeFolderAction->setObjectName("deleteFSTreeFolder");
    addAction(deleteFSTreeFolderAction);
    connect(deleteFSTreeFolderAction, &QAction::triggered, this, &MW::deleteFolder);

    copyAction = new QAction(tr("Copy file(s)"), this);
    copyAction->setObjectName("copyFiles");
    copyAction->setShortcutVisibleInContextMenu(true);
    copyAction->setShortcut(QKeySequence("Ctrl+C"));
    addAction(copyAction);
    connect(copyAction, &QAction::triggered, this, &MW::copy);

    copyImageAction = new QAction(tr("Copy image"), this);
    copyImageAction->setObjectName("copyImage");
    copyImageAction->setShortcutVisibleInContextMenu(true);
    copyImageAction->setShortcut(QKeySequence("Ctrl+Shift+C"));
    addAction(copyImageAction);
    connect(copyImageAction, &QAction::triggered, imageView, &ImageView::copyImage);

    searchTextEditAction = new QAction(tr("Edit search text"), this);
    searchTextEditAction->setObjectName("searchTextEdit");
    searchTextEditAction->setShortcutVisibleInContextMenu(true);
    addAction(searchTextEditAction);
    connect(searchTextEditAction, &QAction::triggered, this, &MW::searchTextEdit);

    rate0Action = new QAction(tr("Clear rating"), this);
    rate0Action->setObjectName("Rate0");
    rate0Action->setShortcutVisibleInContextMenu(true);
    addAction(rate0Action);
    connect(rate0Action, &QAction::triggered, this, &MW::setRating);

    rate1Action = new QAction(tr("Set rating to 1"), this);
    rate1Action->setObjectName("Rate1");
    rate1Action->setShortcutVisibleInContextMenu(true);
    addAction(rate1Action);
    connect(rate1Action, &QAction::triggered, this, &MW::setRating);

    rate2Action = new QAction(tr("Set rating to 2"), this);
    rate2Action->setObjectName("Rate2");
    rate2Action->setShortcutVisibleInContextMenu(true);
    addAction(rate2Action);
    connect(rate2Action, &QAction::triggered, this, &MW::setRating);

    rate3Action = new QAction(tr("Set rating to 3"), this);
    rate3Action->setObjectName("Rate3");
    rate3Action->setShortcutVisibleInContextMenu(true);
    addAction(rate3Action);
    connect(rate3Action, &QAction::triggered, this, &MW::setRating);

    rate4Action = new QAction(tr("Set rating to 4"), this);
    rate4Action->setObjectName("Rate4");
    rate4Action->setShortcutVisibleInContextMenu(true);
    addAction(rate4Action);
    connect(rate4Action, &QAction::triggered, this, &MW::setRating);

    rate5Action = new QAction(tr("Set rating to 5"), this);
    rate5Action->setObjectName("Rate5");
    rate5Action->setShortcutVisibleInContextMenu(true);
    addAction(rate5Action);
    connect(rate5Action, &QAction::triggered, this, &MW::setRating);

    label0Action = new QAction(tr("Clear colour"), this);
    label0Action->setObjectName("Label0");
    label0Action->setShortcutVisibleInContextMenu(true);
    addAction(label0Action);
    connect(label0Action, &QAction::triggered, this, &MW::setColorClass);

    label1Action = new QAction(tr("Set to Red"), this);
    label1Action->setObjectName("Label1");
    label1Action->setShortcutVisibleInContextMenu(true);
    addAction(label1Action);
    connect(label1Action, &QAction::triggered, this, &MW::setColorClass);

    label2Action = new QAction(tr("Set to Yellow"), this);
    label2Action->setObjectName("Label2");
    label2Action->setShortcutVisibleInContextMenu(true);
    addAction(label2Action);
    connect(label2Action, &QAction::triggered, this, &MW::setColorClass);

    label3Action = new QAction(tr("Set to Green"), this);
    label3Action->setObjectName("Label3");
    label3Action->setShortcutVisibleInContextMenu(true);
    addAction(label3Action);
    connect(label3Action, &QAction::triggered, this, &MW::setColorClass);

    label4Action = new QAction(tr("Set to Blue"), this);
    label4Action->setObjectName("Label4");
    label4Action->setShortcutVisibleInContextMenu(true);
    addAction(label4Action);
    connect(label4Action, &QAction::triggered, this, &MW::setColorClass);

    label5Action = new QAction(tr("Set to Purple"), this);
    label5Action->setObjectName("Label5");
    label5Action->setShortcutVisibleInContextMenu(true);
    addAction(label5Action);
    connect(label5Action, &QAction::triggered, this, &MW::setColorClass);

    addThumbnailsAction = new QAction(tr("Fix missing thumbnails in JPG"), this);
    addThumbnailsAction->setObjectName("addThumbnails");
    addThumbnailsAction->setShortcutVisibleInContextMenu(true);
    addAction(addThumbnailsAction);
    connect(addThumbnailsAction, &QAction::triggered, this, &MW::insertThumbnails);

    rotateRightAction = new QAction(tr("Rotate CW"), this);
    rotateRightAction->setObjectName("rotateRight");
    rotateRightAction->setShortcutVisibleInContextMenu(true);
    addAction(rotateRightAction);
    connect(rotateRightAction, &QAction::triggered, this, &MW::rotateRight);

    rotateLeftAction = new QAction(tr("Rotate CCW"), this);
    rotateLeftAction->setObjectName("rotateLeft");
    rotateLeftAction->setShortcutVisibleInContextMenu(true);
    addAction(rotateLeftAction);
    connect(rotateLeftAction, &QAction::triggered, this, &MW::rotateLeft);

    prefAction = new QAction(tr("Preferences"), this);
    prefAction->setObjectName("settings");
    prefAction->setShortcutVisibleInContextMenu(true);
    addAction(prefAction);
    connect(prefAction, &QAction::triggered, [this]() {preferences("");} );

    prefInfoAction = new QAction(tr("Hide or show info fields"), this);
    prefInfoAction->setObjectName("infosettings");
    prefInfoAction->setShortcutVisibleInContextMenu(true);
    addAction(prefInfoAction);
    connect(prefInfoAction, &QAction::triggered, this, &MW::infoViewPreferences);

    // Go menu

    keyRightAction = new QAction(tr("Next"), this);
    keyRightAction->setObjectName("nextImage");
    keyRightAction->setShortcutVisibleInContextMenu(true);
    keyRightAction->setEnabled(true);
    addAction(keyRightAction);
    connect(keyRightAction, &QAction::triggered, this, &MW::keyRight);

    keyLeftAction = new QAction(tr("Previous"), this);
    keyLeftAction->setObjectName("prevImage");
    keyLeftAction->setShortcutVisibleInContextMenu(true);
    addAction(keyLeftAction);
    connect(keyLeftAction, &QAction::triggered, this, &MW::keyLeft);

    keyUpAction = new QAction(tr("Up"), this);
    keyUpAction->setObjectName("moveUp");
    keyUpAction->setShortcutVisibleInContextMenu(true);
    addAction(keyUpAction);
    connect(keyUpAction, &QAction::triggered, this, &MW::keyUp);

    keyDownAction = new QAction(tr("Down"), this);
    keyDownAction->setObjectName("moveDown");
    keyDownAction->setShortcutVisibleInContextMenu(true);
    addAction(keyDownAction);
    connect(keyDownAction, &QAction::triggered, this, &MW::keyDown);

    keyPageUpAction = new QAction(tr("Page Up"), this);
    keyPageUpAction->setObjectName("movePageUp");
    keyPageUpAction->setShortcutVisibleInContextMenu(true);
    addAction(keyPageUpAction);
    connect(keyPageUpAction, &QAction::triggered, this, &MW::keyPageUp);

    keyPageDownAction = new QAction(tr("Page Down"), this);
    keyPageDownAction->setObjectName("movePageDown");
    keyPageDownAction->setShortcutVisibleInContextMenu(true);
    addAction(keyPageDownAction);
    connect(keyPageDownAction, &QAction::triggered, this, &MW::keyPageDown);

    keyHomeAction = new QAction(tr("First"), this);
    keyHomeAction->setObjectName("firstImage");
    keyHomeAction->setShortcutVisibleInContextMenu(true);
    addAction(keyHomeAction);
    connect(keyHomeAction, &QAction::triggered, this, &MW::keyHome);

    keyEndAction = new QAction(tr("Last"), this);
    keyEndAction->setObjectName("lastImage");
    keyEndAction->setShortcutVisibleInContextMenu(true);
    addAction(keyEndAction);
    connect(keyEndAction, &QAction::triggered, this, &MW::keyEnd);

    keyScrollLeftAction = new QAction(tr("Scroll  Left"), this);
    keyScrollLeftAction->setObjectName("scrollLeft");
    keyScrollLeftAction->setShortcutVisibleInContextMenu(true);
    addAction(keyScrollLeftAction);
    connect(keyScrollLeftAction, &QAction::triggered, this, &MW::keyScrollUp);

    keyScrollRightAction = new QAction(tr("Scroll  Right"), this);
    keyScrollRightAction->setObjectName("scrollRight");
    keyScrollRightAction->setShortcutVisibleInContextMenu(true);
    addAction(keyScrollRightAction);
    connect(keyScrollRightAction, &QAction::triggered, this, &MW::keyScrollDown);

    keyScrollUpAction = new QAction(tr("Scroll  up"), this);
    keyScrollUpAction->setObjectName("scrollUp");
    keyScrollUpAction->setShortcutVisibleInContextMenu(true);
    addAction(keyScrollUpAction);
    connect(keyScrollUpAction, &QAction::triggered, this, &MW::keyScrollUp);

    keyScrollDownAction = new QAction(tr("Scroll  Down"), this);
    keyScrollDownAction->setObjectName("scrollDown");
    keyScrollDownAction->setShortcutVisibleInContextMenu(true);
    addAction(keyScrollDownAction);
    connect(keyScrollDownAction, &QAction::triggered, this, &MW::keyScrollDown);

    keyScrollPageLeftAction = new QAction(tr("Scroll Page Left"), this);
    keyScrollPageLeftAction->setObjectName("scrollPageLeft");
    keyScrollPageLeftAction->setShortcutVisibleInContextMenu(true);
    addAction(keyScrollPageLeftAction);
    connect(keyScrollPageLeftAction, &QAction::triggered, this, &MW::keyScrollPageUp);

    keyScrollPageRightAction = new QAction(tr("Scroll Page Right"), this);
    keyScrollPageRightAction->setObjectName("scrollPageRight");
    keyScrollPageRightAction->setShortcutVisibleInContextMenu(true);
    addAction(keyScrollPageRightAction);
    connect(keyScrollPageRightAction, &QAction::triggered, this, &MW::keyScrollPageDown);

    keyScrollPageUpAction = new QAction(tr("Scroll Page up"), this);
    keyScrollPageUpAction->setObjectName("scrollPageUp");
    keyScrollPageUpAction->setShortcutVisibleInContextMenu(true);
    addAction(keyScrollPageUpAction);
    connect(keyScrollPageUpAction, &QAction::triggered, this, &MW::keyScrollPageUp);

    keyScrollPageDownAction = new QAction(tr("Scroll Page down"), this);
    keyScrollPageDownAction->setObjectName("scrollPageDown");
    keyScrollPageDownAction->setShortcutVisibleInContextMenu(true);
    addAction(keyScrollPageDownAction);
    connect(keyScrollPageDownAction, &QAction::triggered, this, &MW::keyScrollPageDown);

    // Not a menu item - used by slide show
    randomImageAction = new QAction(tr("Random"), this);
    randomImageAction->setObjectName("randomImage");
    randomImageAction->setShortcutVisibleInContextMenu(true);
    addAction(randomImageAction);
    connect(randomImageAction, &QAction::triggered, thumbView, &IconView::selectRandom);

    nextPickAction = new QAction(tr("Next Pick"), this);
    nextPickAction->setObjectName("nextPick");
    nextPickAction->setShortcutVisibleInContextMenu(true);
    addAction(nextPickAction);
    connect(nextPickAction, &QAction::triggered, thumbView, &IconView::selectNextPick);

    prevPickAction = new QAction(tr("Previous Pick"), this);
    prevPickAction->setObjectName("prevPick");
    prevPickAction->setShortcutVisibleInContextMenu(true);
    addAction(prevPickAction);
    connect(prevPickAction, &QAction::triggered, thumbView, &IconView::selectPrevPick);

    // Filters

    clearAllFiltersAction = new QAction(tr("Clear all filters"), this);
    clearAllFiltersAction->setObjectName("uncheckAllFilters");
    clearAllFiltersAction->setShortcutVisibleInContextMenu(true);
    addAction(clearAllFiltersAction);
    connect(clearAllFiltersAction, &QAction::triggered, this, &MW::clearAllFilters);

    expandAllFiltersAction = new QAction(tr("Expand all filters"), this);
    expandAllFiltersAction->setObjectName("expandAll");
    expandAllFiltersAction->setShortcutVisibleInContextMenu(true);
    addAction(expandAllFiltersAction);
    connect(expandAllFiltersAction, &QAction::triggered, filters, &Filters::expandAllFilters);

    collapseAllFiltersAction = new QAction(tr("Collapse all filters"), this);
    collapseAllFiltersAction->setObjectName("collapseAll");
    collapseAllFiltersAction->setShortcutVisibleInContextMenu(true);
    addAction(collapseAllFiltersAction);
    connect(collapseAllFiltersAction, &QAction::triggered, filters, &Filters::collapseAllFilters);

    filterSearchAction = new QAction(tr("Filter by search text"), this);
    filterSearchAction->setCheckable(true);
    filterSearchAction->setShortcutVisibleInContextMenu(true);
    addAction(filterSearchAction);
    connect(filterSearchAction, &QAction::triggered, this, &MW::quickFilter);

    filterRating1Action = new QAction(tr("Filter by rating 1"), this);
    filterRating1Action->setCheckable(true);
    filterRating1Action->setShortcutVisibleInContextMenu(true);
    addAction(filterRating1Action);
    connect(filterRating1Action, &QAction::triggered, this, &MW::quickFilter);

    filterRating2Action = new QAction(tr("Filter by rating 2"), this);
    filterRating2Action->setCheckable(true);
    filterRating2Action->setShortcutVisibleInContextMenu(true);
    addAction(filterRating2Action);
    connect(filterRating2Action,  &QAction::triggered, this, &MW::quickFilter);

    filterRating3Action = new QAction(tr("Filter by rating 3"), this);
    filterRating3Action->setCheckable(true);
    filterRating3Action->setShortcutVisibleInContextMenu(true);
    addAction(filterRating3Action);
    connect(filterRating3Action,  &QAction::triggered, this, &MW::quickFilter);

    filterRating4Action = new QAction(tr("Filter by rating 4"), this);
    filterRating4Action->setCheckable(true);
    filterRating4Action->setShortcutVisibleInContextMenu(true);
    addAction(filterRating4Action);
    connect(filterRating4Action,  &QAction::triggered, this, &MW::quickFilter);

    filterRating5Action = new QAction(tr("Filter by rating 5"), this);
    filterRating5Action->setCheckable(true);
    filterRating5Action->setShortcutVisibleInContextMenu(true);
    addAction(filterRating5Action);
    connect(filterRating5Action,  &QAction::triggered, this, &MW::quickFilter);

    filterRedAction = new QAction(tr("Filter by Red"), this);
    filterRedAction->setShortcutVisibleInContextMenu(true);
    filterRedAction->setCheckable(true);
    addAction(filterRedAction);
    connect(filterRedAction,  &QAction::triggered, this, &MW::quickFilter);

    filterYellowAction = new QAction(tr("Filter by Yellow"), this);
    filterYellowAction->setShortcutVisibleInContextMenu(true);
    filterYellowAction->setCheckable(true);
    addAction(filterYellowAction);
    connect(filterYellowAction,  &QAction::triggered, this, &MW::quickFilter);

    filterGreenAction = new QAction(tr("Filter by Green"), this);
    filterGreenAction->setShortcutVisibleInContextMenu(true);
    filterGreenAction->setCheckable(true);
    addAction(filterGreenAction);
    connect(filterGreenAction,  &QAction::triggered, this, &MW::quickFilter);

    filterBlueAction = new QAction(tr("Filter by Blue"), this);
    filterBlueAction->setShortcutVisibleInContextMenu(true);
    filterBlueAction->setCheckable(true);
    addAction(filterBlueAction);
    connect(filterBlueAction,  &QAction::triggered, this, &MW::quickFilter);

    filterPurpleAction = new QAction(tr("Filter by Purple"), this);
    filterPurpleAction->setShortcutVisibleInContextMenu(true);
    filterPurpleAction->setCheckable(true);
    addAction(filterPurpleAction);
    connect(filterPurpleAction,  &QAction::triggered, this, &MW::quickFilter);

    filterInvertAction = new QAction(tr("Invert Filter"), this);
    filterInvertAction->setShortcutVisibleInContextMenu(true);
    filterInvertAction->setCheckable(false);
    addAction(filterInvertAction);
    connect(filterInvertAction,  &QAction::triggered, this, &MW::invertFilters);

    filterLastDayAction = new QAction(tr("Most recent day"), this);
    filterLastDayAction->setShortcutVisibleInContextMenu(true);
    filterLastDayAction->setCheckable(true);
    addAction(filterLastDayAction);
    connect(filterLastDayAction,  &QAction::triggered, this, &MW::filterLastDay);

    filterUpdateAction = new QAction(tr("Update all filters"), this);
    filterUpdateAction->setShortcutVisibleInContextMenu(true);
    addAction(filterUpdateAction);
    connect(filterUpdateAction,  &QAction::triggered, this, &MW::launchBuildFilters);

    filterSoloAction = new QAction(tr("Solo mode"), this);
    filterSoloAction->setShortcutVisibleInContextMenu(true);
    filterSoloAction->setCheckable(true);
    addAction(filterSoloAction);
    if (!setting->value("isSoloFilters").isValid() || simulateJustInstalled)
        filterSoloAction->setChecked(true);
    else
        filterSoloAction->setChecked(setting->value("isSoloFilters").toBool());
    setFilterSolo();    // set solo in filters and save to settings
    connect(filterSoloAction,  &QAction::triggered, this, &MW::setFilterSolo);

    // Sort Menu

    sortFileNameAction = new QAction(tr("Sort by file name"), this);
    sortFileNameAction->setShortcutVisibleInContextMenu(true);
    sortFileNameAction->setCheckable(true);
    addAction(sortFileNameAction);
    connect(sortFileNameAction, &QAction::triggered, this, &MW::sortChangeFromAction);

    sortFileTypeAction = new QAction(tr("Sort by file type"), this);
    sortFileTypeAction->setShortcutVisibleInContextMenu(true);
    sortFileTypeAction->setCheckable(true);
    addAction(sortFileTypeAction);
    connect(sortFileTypeAction, &QAction::triggered, this, &MW::sortChangeFromAction);

    sortFileSizeAction = new QAction(tr("Sort by file size"), this);
    sortFileSizeAction->setShortcutVisibleInContextMenu(true);
    sortFileSizeAction->setCheckable(true);
    addAction(sortFileSizeAction);
    connect(sortFileSizeAction, &QAction::triggered, this, &MW::sortChangeFromAction);

    sortCreateAction = new QAction(tr("Sort by created time"), this);
    sortCreateAction->setShortcutVisibleInContextMenu(true);
    sortCreateAction->setCheckable(true);
    addAction(sortCreateAction);
    connect(sortCreateAction, &QAction::triggered, this, &MW::sortChangeFromAction);

    sortModifyAction = new QAction(tr("Sort by last modified time"), this);
    sortModifyAction->setShortcutVisibleInContextMenu(true);
    sortModifyAction->setCheckable(true);
    addAction(sortModifyAction);
    connect(sortModifyAction, &QAction::triggered, this, &MW::sortChangeFromAction);

    sortPickAction = new QAction(tr("Sort by picked status"), this);
    sortPickAction->setShortcutVisibleInContextMenu(true);
    sortPickAction->setCheckable(true);
    addAction(sortPickAction);
    connect(sortPickAction, &QAction::triggered, this, &MW::sortChangeFromAction);

    sortLabelAction = new QAction(tr("Sort by label"), this);
    sortLabelAction->setShortcutVisibleInContextMenu(true);
    sortLabelAction->setCheckable(true);
    addAction(sortLabelAction);
    connect(sortLabelAction, &QAction::triggered, this, &MW::sortChangeFromAction);

    sortRatingAction = new QAction(tr("Sort by rating"), this);
    sortRatingAction->setShortcutVisibleInContextMenu(true);
    sortRatingAction->setCheckable(true);
    addAction(sortRatingAction);
    connect(sortRatingAction, &QAction::triggered, this, &MW::sortChangeFromAction);

    sortMegaPixelsAction = new QAction(tr("Sort by megapixels"), this);
    sortMegaPixelsAction->setShortcutVisibleInContextMenu(true);
    sortMegaPixelsAction->setCheckable(true);
    addAction(sortMegaPixelsAction);
    connect(sortMegaPixelsAction, &QAction::triggered, this, &MW::sortChangeFromAction);

    sortDimensionsAction = new QAction(tr("Sort by dimensions"), this);
    sortDimensionsAction->setShortcutVisibleInContextMenu(true);
    sortDimensionsAction->setCheckable(true);
    addAction(sortDimensionsAction);
    connect(sortDimensionsAction, &QAction::triggered, this, &MW::sortChangeFromAction);

    sortApertureAction = new QAction(tr("Sort by aperture"), this);
//    sortApertureAction->setObjectName("SortAperture");
    sortApertureAction->setShortcutVisibleInContextMenu(true);
    sortApertureAction->setCheckable(true);
    addAction(sortApertureAction);
    connect(sortApertureAction, &QAction::triggered, this, &MW::sortChangeFromAction);

    sortShutterSpeedAction = new QAction(tr("Sort by shutter speed"), this);
    sortShutterSpeedAction->setShortcutVisibleInContextMenu(true);
    sortShutterSpeedAction->setCheckable(true);
    addAction(sortShutterSpeedAction);
    connect(sortShutterSpeedAction, &QAction::triggered, this, &MW::sortChangeFromAction);

    sortISOAction = new QAction(tr("Sort by ISO"), this);
    sortISOAction->setShortcutVisibleInContextMenu(true);
    sortISOAction->setCheckable(true);
    addAction(sortISOAction);
    connect(sortISOAction, &QAction::triggered, this, &MW::sortChangeFromAction);

    sortModelAction = new QAction(tr("Sort by camera model"), this);
    sortModelAction->setShortcutVisibleInContextMenu(true);
    sortModelAction->setCheckable(true);
    addAction(sortModelAction);
    connect(sortModelAction, &QAction::triggered, this, &MW::sortChangeFromAction);

    sortFocalLengthAction = new QAction(tr("Sort by focal length"), this);
    sortFocalLengthAction->setShortcutVisibleInContextMenu(true);
    sortFocalLengthAction->setCheckable(true);
    addAction(sortFocalLengthAction);
    connect(sortFocalLengthAction, &QAction::triggered, this, &MW::sortChangeFromAction);

    sortTitleAction = new QAction(tr("Sort by title"), this);
    sortTitleAction->setShortcutVisibleInContextMenu(true);
    sortTitleAction->setCheckable(true);
    addAction(sortTitleAction);
    connect(sortTitleAction, &QAction::triggered, this, &MW::sortChangeFromAction);

    sortLensAction = new QAction(tr("Sort by lens"), this);
    sortLensAction->setShortcutVisibleInContextMenu(true);
    sortLensAction->setCheckable(true);
    addAction(sortLensAction);
    connect(sortLensAction, &QAction::triggered, this, &MW::sortChangeFromAction);

    sortCreatorAction = new QAction(tr("Sort by creator"), this);
    sortCreatorAction->setShortcutVisibleInContextMenu(true);
    sortCreatorAction->setCheckable(true);
    addAction(sortCreatorAction);
    connect(sortCreatorAction, &QAction::triggered, this, &MW::sortChangeFromAction);

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
    sortGroupAction->addAction(sortLensAction);
    sortGroupAction->addAction(sortFocalLengthAction);
    sortGroupAction->addAction(sortTitleAction);
    sortGroupAction->addAction(sortCreatorAction);

    updateSortColumn(sortColumn);

    sortReverseAction = new QAction(tr("Reverse sort order"), this);
    sortReverseAction->setObjectName("reverse");
    sortReverseAction->setShortcutVisibleInContextMenu(true);
    sortReverseAction->setCheckable(true);
    addAction(sortReverseAction);
    connect(sortReverseAction, &QAction::triggered, this, &MW::toggleSortDirectionClick);

    // Embellish menu

    embelExportAction = new QAction(tr("Export embellished image"), this);
    embelExportAction->setObjectName("embelExportAct");
    embelExportAction->setShortcutVisibleInContextMenu(true);
    addAction(embelExportAction);
    connect(embelExportAction, &QAction::triggered, this, &MW::exportEmbel);

    embelNewTemplateAction = new QAction(tr("New template"), this);
    embelNewTemplateAction->setObjectName("newEmbelTemplateAct");
    embelNewTemplateAction->setShortcutVisibleInContextMenu(true);
    addAction(embelNewTemplateAction);
    connect(embelNewTemplateAction, &QAction::triggered, this, &MW::newEmbelTemplate);

    embelReadTemplateAction = new QAction(tr("Read template from file"), this);
    embelReadTemplateAction->setObjectName("embelExportAct");
    embelReadTemplateAction->setShortcutVisibleInContextMenu(true);
    addAction(embelReadTemplateAction);
    connect(embelReadTemplateAction, &QAction::triggered, embelProperties, &EmbelProperties::readTemplateFromFile);

    embelSaveTemplateAction = new QAction(tr("Save current template to file"), this);
    embelSaveTemplateAction->setObjectName("embelExportAct");
    embelSaveTemplateAction->setShortcutVisibleInContextMenu(true);
    addAction(embelSaveTemplateAction);
    connect(embelSaveTemplateAction, &QAction::triggered, embelProperties, &EmbelProperties::saveTemplateToFile);

    embelManageTilesAction = new QAction(tr("Manage tiles"), this);
    embelManageTilesAction->setObjectName("embelManageTilesAct");
    embelManageTilesAction->setShortcutVisibleInContextMenu(true);
    addAction(embelManageTilesAction);
    connect(embelManageTilesAction, &QAction::triggered, embelProperties, &EmbelProperties::manageTiles);

    embelManageGraphicsAction = new QAction(tr("Manage graphics"), this);
    embelManageGraphicsAction->setObjectName("embelManageGraphicsAct");
    embelManageGraphicsAction->setShortcutVisibleInContextMenu(true);
    addAction(embelManageGraphicsAction);
    connect(embelManageGraphicsAction, &QAction::triggered, embelProperties, &EmbelProperties::manageGraphics);

    #ifdef Q_OS_WIN
        revealText = "Show Winnets in Explorer";
    #endif
    #ifdef Q_OS_MAC
        revealText = "Show Winnets in Finder";
    #endif

    embelRevealWinnetsAction = new QAction(revealText, this);
    embelRevealWinnetsAction->setObjectName("RevealWinnetsAct");
    embelRevealWinnetsAction->setShortcutVisibleInContextMenu(true);
    addAction(embelRevealWinnetsAction);
    connect(embelRevealWinnetsAction, &QAction::triggered, this, &MW::revealWinnets);

    /* general connection to handle invoking new embellish templates
     MacOS will not allow runtime menu insertions.  Cludge workaround
     add 30 dummy menu items and then hide until use.

     embelTemplatesActions are executed by EmbelProperties::invokeFromAction which in turn
     triggers EmbelProperties::itemChange.

     ie to execute "Do not embellish" (always the first action)
     EmbelProperties::invokeFromAction(embelTemplatesActions.at(0))
    */
    embelGroupAction = new QActionGroup(this);
    embelGroupAction->setExclusive(true);
    n = embelProperties->templateList.count();
    for (int i = 0; i < 30; i++) {
        QString name;
        QString objName = "";
        if (i < n) {
            name = embelProperties->templateList.at(i);
            objName = "template" + QString::number(i);
        }
        else name = "Future Template" + QString::number(i);

        embelTemplatesActions.append(new QAction(name, this));

        if (i < n) {
            embelTemplatesActions.at(i)->setObjectName(objName);
            embelTemplatesActions.at(i)->setCheckable(true);
            embelTemplatesActions.at(i)->setText(name);
        }

        if (i == 0) {
            embelTemplatesActions.at(i)->setShortcut(QKeySequence("N"));
            embelTemplatesActions.at(i)->setShortcutVisibleInContextMenu(true);
        }

//        if (i < 10 && i < n) {
//            embelTemplatesActions.at(i)->setShortcut(QKeySequence("Alt+Shift+" + QString::number(i)));
//            embelTemplatesActions.at(i)->setShortcutVisibleInContextMenu(true);
//            embelTemplatesActions.at(i)->setVisible(true);
//            addAction(embelTemplatesActions.at(i));
//        }

        if (i < n) addAction(embelTemplatesActions.at(i));

        if (i >= n) embelTemplatesActions.at(i)->setVisible(false);
//        embelTemplatesActions.at(i)->setShortcut(QKeySequence("Ctrl+Shift+" + QString::number(i)));
        embelGroupAction->addAction(embelTemplatesActions.at(i));
    }
    addActions(embelTemplatesActions);
    // sync menu with QSettings last active embel template
    if (embelProperties->templateId < n)
        embelTemplatesActions.at(embelProperties->templateId)->setChecked(true);

    // View menu
    slideShowAction = new QAction(tr("Slide Show"), this);
    slideShowAction->setObjectName("slideShow");
    slideShowAction->setShortcutVisibleInContextMenu(true);
    addAction(slideShowAction);
    connect(slideShowAction, &QAction::triggered, this, &MW::slideShow);

    fullScreenAction = new QAction(tr("Full Screen"), this);
    fullScreenAction->setObjectName("fullScreenAct");
    fullScreenAction->setShortcutVisibleInContextMenu(true);
    fullScreenAction->setCheckable(true);
    if (isSettings && setting->contains("isFullScreen")) fullScreenAction->setChecked(setting->value("isFullScreen").toBool());
    else fullScreenAction->setChecked(false);
    addAction(fullScreenAction);
    connect(fullScreenAction, &QAction::triggered, this, &MW::toggleFullScreen);

    escapeFullScreenAction = new QAction(tr("Escape Full Screen"), this);
    escapeFullScreenAction->setObjectName("escapeFullScreenAct");
    escapeFullScreenAction->setShortcutVisibleInContextMenu(true);
    addAction(escapeFullScreenAction);
    connect(escapeFullScreenAction, &QAction::triggered, this, &MW::escapeFullScreen);

    ratingBadgeVisibleAction = new QAction(tr("Show Rating Badge"), this);
    ratingBadgeVisibleAction->setObjectName("toggleRatingBadge");
    ratingBadgeVisibleAction->setShortcutVisibleInContextMenu(true);
    ratingBadgeVisibleAction->setCheckable(true);
    ratingBadgeVisibleAction->setChecked(false);
    // isRatingBadgeVisible = false by default, loadSettings updates value before createActions
    ratingBadgeVisibleAction->setChecked(isRatingBadgeVisible);
//    if (isRatingBadgeVisible) ratingBadgeVisibleAction->setChecked(true);
//    else ratingBadgeVisibleAction->setChecked(false);
    addAction(ratingBadgeVisibleAction);
    connect(ratingBadgeVisibleAction, &QAction::triggered, this, &MW::setRatingBadgeVisibility);

    infoVisibleAction = new QAction(tr("Show Shooting Info"), this);
    infoVisibleAction->setObjectName("toggleInfo");
    infoVisibleAction->setShortcutVisibleInContextMenu(true);
    infoVisibleAction->setCheckable(true);
    addAction(infoVisibleAction);
    connect(infoVisibleAction, &QAction::triggered, this, &MW::setShootingInfoVisibility);

    infoSelectAction = new QAction(tr("Token editor"), this);
    infoSelectAction->setShortcutVisibleInContextMenu(true);
    infoSelectAction->setObjectName("selectInfo");
    if (isSettings && setting->contains("isImageInfoVisible")) infoVisibleAction->setChecked(setting->value("isImageInfoVisible").toBool());
    else infoVisibleAction->setChecked(false);
    addAction(infoSelectAction);
    connect(infoSelectAction, &QAction::triggered, this, &MW::tokenEditor);

    asLoupeAction = new QAction(tr("Loupe"), this);
    asLoupeAction->setShortcutVisibleInContextMenu(true);
    asLoupeAction->setCheckable(true);
    if (isSettings && setting->contains("isLoupeDisplay"))
        asLoupeAction->setChecked(setting->value("isLoupeDisplay").toBool()
                                  || setting->value("isCompareDisplay").toBool());
//                                  || setting->value("isEmbelDisplay").toBool()
    else asLoupeAction->setChecked(false);
    addAction(asLoupeAction);
    connect(asLoupeAction, &QAction::triggered, this, &MW::loupeDisplay);

    asGridAction = new QAction(tr("Grid"), this);
    asGridAction->setShortcutVisibleInContextMenu(true);
    asGridAction->setCheckable(true);
    if (isSettings && setting->contains("isGridDisplay")) asGridAction->setChecked(setting->value("isGridDisplay").toBool());
    else asGridAction->setChecked(true);
    addAction(asGridAction);
    connect(asGridAction, &QAction::triggered, this, &MW::gridDisplay);

    asTableAction = new QAction(tr("Table"), this);
    asTableAction->setShortcutVisibleInContextMenu(true);
    asTableAction->setCheckable(true);
    if (isSettings && setting->contains("isTableDisplay")) asTableAction->setChecked(setting->value("isTableDisplay").toBool());
    else asTableAction->setChecked(false);
    addAction(asTableAction);
    connect(asTableAction, &QAction::triggered, this, &MW::tableDisplay);

    asCompareAction = new QAction(tr("Compare"), this);
    asCompareAction->setShortcutVisibleInContextMenu(true);
    asCompareAction->setCheckable(true);
    asCompareAction->setChecked(false); // never start with compare set true
    addAction(asCompareAction);
    connect(asCompareAction, &QAction::triggered, this, &MW::compareDisplay);

    centralGroupAction = new QActionGroup(this);
    centralGroupAction->setExclusive(true);
    centralGroupAction->addAction(asLoupeAction);
    centralGroupAction->addAction(asGridAction);
    centralGroupAction->addAction(asTableAction);
    centralGroupAction->addAction(asCompareAction);

    zoomToAction = new QAction(tr("Zoom"), this);
    zoomToAction->setObjectName("zoomDlg");
    zoomToAction->setShortcutVisibleInContextMenu(true);
    addAction(zoomToAction);
    connect(zoomToAction, &QAction::triggered, this, &MW::toggleZoomDlg);

    zoomOutAction = new QAction(tr("Zoom Out"), this);
    zoomOutAction->setObjectName("zoomOut");
    zoomOutAction->setShortcutVisibleInContextMenu(true);
    addAction(zoomOutAction);
    connect(zoomOutAction, &QAction::triggered, this, &MW::zoomOut);

    zoomInAction = new QAction(tr("Zoom In"), this);
    zoomInAction->setObjectName("zoomIn");
    zoomInAction->setShortcutVisibleInContextMenu(true);
    addAction(zoomInAction);
    connect(zoomInAction, &QAction::triggered, this, &MW::zoomIn);

    zoomToggleAction = new QAction(tr("Zoom fit <-> 100%"), this);
    zoomToggleAction->setObjectName("resetZoom");
    zoomToggleAction->setShortcutVisibleInContextMenu(true);
    addAction(zoomToggleAction);
    connect(zoomToggleAction, &QAction::triggered, this, &MW::zoomToggle);

    thumbsEnlargeAction = new QAction(tr("Enlarge thumbs"), this);
    thumbsEnlargeAction->setObjectName("enlargeThumbs");
    thumbsEnlargeAction->setShortcutVisibleInContextMenu(true);
    addAction(thumbsEnlargeAction);
    connect(thumbsEnlargeAction, &QAction::triggered, this, &MW::thumbsEnlarge);

    thumbsShrinkAction = new QAction(tr("Shrink thumbs"), this);
    thumbsShrinkAction->setObjectName("shrinkThumbs");
    thumbsShrinkAction->setShortcutVisibleInContextMenu(true);
    addAction(thumbsShrinkAction);
    connect(thumbsShrinkAction, &QAction::triggered, this, &MW::thumbsShrink);

//#ifdef Q_OS_WIN
    menuBarVisibleAction = new QAction(tr("Menubar"), this);
    menuBarVisibleAction->setObjectName("toggleMenuBar");
    menuBarVisibleAction->setShortcutVisibleInContextMenu(true);
    menuBarVisibleAction->setCheckable(true);
    if (isSettings && setting->contains("isMenuBarVisible")) menuBarVisibleAction->setChecked(setting->value("isMenuBarVisible").toBool());
    else menuBarVisibleAction->setChecked(true);
    addAction(menuBarVisibleAction);
    connect(menuBarVisibleAction, &QAction::triggered, this, &MW::setMenuBarVisibility);
//#endif

    statusBarVisibleAction = new QAction(tr("Statusbar"), this);
    statusBarVisibleAction->setObjectName("toggleStatusBar");
    statusBarVisibleAction->setShortcutVisibleInContextMenu(true);
    statusBarVisibleAction->setCheckable(true);
    if (isSettings && setting->contains("isStatusBarVisible")) statusBarVisibleAction->setChecked(setting->value("isStatusBarVisible").toBool());
    else statusBarVisibleAction->setChecked(true);
    addAction(statusBarVisibleAction);
    connect(statusBarVisibleAction, &QAction::triggered, this, &MW::setStatusBarVisibility);

    folderDockVisibleAction = new QAction(tr("Folder"), this);
    folderDockVisibleAction->setObjectName("toggleFiless");
    folderDockVisibleAction->setShortcutVisibleInContextMenu(true);
    folderDockVisibleAction->setCheckable(true);
    if (isSettings && setting->contains("isFolderDockVisible")) folderDockVisibleAction->setChecked(setting->value("isFolderDockVisible").toBool());
    else folderDockVisibleAction->setChecked(true);
    addAction(folderDockVisibleAction);
    connect(folderDockVisibleAction, &QAction::triggered, this, &MW::toggleFolderDockVisibility);

    favDockVisibleAction = new QAction(tr("Bookmarks"), this);
    favDockVisibleAction->setObjectName("toggleFavs");
    favDockVisibleAction->setShortcutVisibleInContextMenu(true);
    favDockVisibleAction->setCheckable(true);
    if (isSettings && setting->contains("isFavDockVisible")) favDockVisibleAction->setChecked(setting->value("isFavDockVisible").toBool());
    else favDockVisibleAction->setChecked(true);
    addAction(favDockVisibleAction);
    connect(favDockVisibleAction, &QAction::triggered, this, &MW::toggleFavDockVisibility);

    filterDockVisibleAction = new QAction(tr("Filters"), this);
    filterDockVisibleAction->setObjectName("toggleFilters");
    filterDockVisibleAction->setShortcutVisibleInContextMenu(true);
    filterDockVisibleAction->setCheckable(true);
    if (isSettings && setting->contains("isFilterDockVisible")) filterDockVisibleAction->setChecked(setting->value("isFilterDockVisible").toBool());
    else filterDockVisibleAction->setChecked(true);
    addAction(filterDockVisibleAction);
    connect(filterDockVisibleAction, &QAction::triggered, this, &MW::toggleFilterDockVisibility);

    metadataDockVisibleAction = new QAction(tr("Metadata"), this);
    metadataDockVisibleAction->setObjectName("toggleMetadata");
    metadataDockVisibleAction->setShortcutVisibleInContextMenu(true);
    metadataDockVisibleAction->setCheckable(true);
    if (isSettings && setting->contains("isMetadataDockVisible")) metadataDockVisibleAction->setChecked(setting->value("isMetadataDockVisible").toBool());
    else metadataDockVisibleAction->setChecked(true);
    addAction(metadataDockVisibleAction);
    connect(metadataDockVisibleAction, &QAction::triggered, this, &MW::toggleMetadataDockVisibility);

    thumbDockVisibleAction = new QAction(tr("Thumbnails"), this);
    thumbDockVisibleAction->setObjectName("toggleThumbs");
    thumbDockVisibleAction->setShortcutVisibleInContextMenu(true);
    thumbDockVisibleAction->setCheckable(true);
    if (isSettings && setting->contains("isThumbDockVisible")) thumbDockVisibleAction->setChecked(setting->value("isThumbDockVisible").toBool());
    else thumbDockVisibleAction->setChecked(true);
    addAction(thumbDockVisibleAction);
    connect(thumbDockVisibleAction, &QAction::triggered, this, &MW::toggleThumbDockVisibity);

    embelDockVisibleAction = new QAction(tr("Embellish Editor"), this);
    embelDockVisibleAction->setObjectName("toggleEmbelDock");
    embelDockVisibleAction->setShortcutVisibleInContextMenu(true);
    embelDockVisibleAction->setCheckable(true);
    if (isSettings && setting->contains("isEmbelDockVisible")) embelDockVisibleAction->setChecked(setting->value("isEmbelDockVisible").toBool());
    else embelDockVisibleAction->setChecked(false);
    addAction(embelDockVisibleAction);
    connect(embelDockVisibleAction, &QAction::triggered, this, &MW::toggleEmbelDockVisibility);

    // rgh delete this ?
    metadataFixedSizeAction = new QAction(tr("Metadata Panel Fix Size"), this);
    metadataFixedSizeAction->setObjectName("metadataFixedSize");
    metadataFixedSizeAction->setShortcutVisibleInContextMenu(true);
    metadataFixedSizeAction->setCheckable(true);
    addAction(metadataFixedSizeAction);
    connect(metadataFixedSizeAction, &QAction::triggered, this, &MW::setMetadataDockFixedSize);

    // Workspace submenu of Window menu
    defaultWorkspaceAction = new QAction(tr("Default Workspace"), this);
    defaultWorkspaceAction->setObjectName("defaultWorkspace");
    defaultWorkspaceAction->setShortcutVisibleInContextMenu(true);
    addAction(defaultWorkspaceAction);
    connect(defaultWorkspaceAction, &QAction::triggered, this, &MW::defaultWorkspace);

    newWorkspaceAction = new QAction(tr("New Workspace"), this);
    newWorkspaceAction->setObjectName("newWorkspace");
    newWorkspaceAction->setShortcutVisibleInContextMenu(true);
    addAction(newWorkspaceAction);
    connect(newWorkspaceAction, &QAction::triggered, this, &MW::newWorkspace);

    manageWorkspaceAction = new QAction(tr("Manage Workspaces ..."), this);
    manageWorkspaceAction->setObjectName("manageWorkspaces");
    manageWorkspaceAction->setShortcutVisibleInContextMenu(true);
    addAction(manageWorkspaceAction);
    connect(manageWorkspaceAction, &QAction::triggered, this, &MW::manageWorkspaces);

    // general connection to handle invoking new workspaces
    // MacOS will not allow runtime menu insertions.  Cludge workaround
    // add 10 dummy menu items and then hide until use.
    n = workspaces->count();
    for (int i = 0; i < 10; i++) {
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
            workspaceActions.at(i)->setShortcutVisibleInContextMenu(true);
            workspaceActions.at(i)->setText(name);
            workspaceActions.at(i)->setVisible(true);
            addAction(workspaceActions.at(i));
        }
        if (i >= n) workspaceActions.at(i)->setVisible(false);
        workspaceActions.at(i)->setShortcut(QKeySequence("Ctrl+" + QString::number(i)));
    }
    addActions(workspaceActions);

// connection moved to after menu creation as will not work before menu created
//    connect(workspaceMenu, SIGNAL(triggered(QAction*)),
//            SLOT(invokeWorkspace(QAction*)));

    //Help menu

    checkForUpdateAction = new QAction(tr("Check for updates"), this);
    checkForUpdateAction->setObjectName("CheckForUpdate");
    checkForUpdateAction->setShortcutVisibleInContextMenu(true);
    addAction(checkForUpdateAction);
    connect(checkForUpdateAction, &QAction::triggered, this, &MW::checkForUpdate);
//    connect(checkForUpdateAction, SIGNAL(triggered(bool)), this, checkForUpdate(false));

    aboutAction = new QAction(tr("About"), this);
    aboutAction->setObjectName("about");
    aboutAction->setShortcutVisibleInContextMenu(true);
    addAction(aboutAction);
    connect(aboutAction, &QAction::triggered, this, &MW::about);

    helpAction = new QAction(tr("Winnow Help"), this);
    helpAction->setObjectName("help");
    helpAction->setShortcutVisibleInContextMenu(true);
    addAction(helpAction);
    connect(helpAction, &QAction::triggered, this, &MW::help);

    helpShortcutsAction = new QAction(tr("Winnow Shortcuts"), this);
    helpShortcutsAction->setObjectName("helpShortcuts");
    helpShortcutsAction->setShortcutVisibleInContextMenu(true);
    addAction(helpShortcutsAction);
    connect(helpShortcutsAction, &QAction::triggered, this, &MW::helpShortcuts);

    helpWelcomeAction = new QAction(tr("Show welcome screen"), this);
    helpWelcomeAction->setObjectName("helpWelcome");
    helpWelcomeAction->setShortcutVisibleInContextMenu(true);
    addAction(helpWelcomeAction);
    connect(helpWelcomeAction, &QAction::triggered, this, &MW::helpWelcome);

    helpRevealLogFileAction = new QAction("Send log file to Rory", this);
    helpRevealLogFileAction->setObjectName("RevealLogFileAct");
    helpRevealLogFileAction->setShortcutVisibleInContextMenu(true);
    addAction(helpRevealLogFileAction);
    connect(helpRevealLogFileAction, &QAction::triggered, this, &MW::revealLogFile);

    // Help Diagnostics

    diagnosticsAllAction = new QAction(tr("All Diagnostics"), this);
    diagnosticsAllAction->setObjectName("diagnosticsAll");
    diagnosticsAllAction->setShortcutVisibleInContextMenu(true);
    addAction(diagnosticsAllAction);
    connect(diagnosticsAllAction, &QAction::triggered, this, &MW::diagnosticsAll);

    diagnosticsCurrentAction = new QAction(tr("Current Row Diagnostics"), this);
    diagnosticsCurrentAction->setObjectName("diagnosticsCurrent");
    diagnosticsCurrentAction->setShortcutVisibleInContextMenu(true);
    addAction(diagnosticsCurrentAction);
    connect(diagnosticsCurrentAction, &QAction::triggered, this, &MW::diagnosticsCurrent);

    diagnosticsErrorsAction = new QAction(tr("Errors"), this);
    diagnosticsErrorsAction->setObjectName("diagnosticsErrorsAction");
    diagnosticsErrorsAction->setShortcutVisibleInContextMenu(true);
    addAction(diagnosticsErrorsAction);
    connect(diagnosticsErrorsAction, &QAction::triggered, this, &MW::errorReport);

    diagnosticsMainAction = new QAction(tr("Main"), this);
    diagnosticsMainAction->setObjectName("diagnosticsMain");
    diagnosticsMainAction->setShortcutVisibleInContextMenu(true);
    addAction(diagnosticsMainAction);
    connect(diagnosticsMainAction, &QAction::triggered, this, &MW::diagnosticsMain);

    diagnosticsGridViewAction = new QAction(tr("GridView"), this);
    diagnosticsGridViewAction->setObjectName("diagnosticsGridView");
    diagnosticsGridViewAction->setShortcutVisibleInContextMenu(true);
    addAction(diagnosticsGridViewAction);
    connect(diagnosticsGridViewAction, &QAction::triggered, this, &MW::diagnosticsGridView);

    diagnosticsThumbViewAction = new QAction(tr("ThumbView"), this);
    diagnosticsThumbViewAction->setObjectName("diagnosticsThumbView");
    diagnosticsThumbViewAction->setShortcutVisibleInContextMenu(true);
    addAction(diagnosticsThumbViewAction);
    connect(diagnosticsThumbViewAction, &QAction::triggered, this, &MW::diagnosticsThumbView);

    diagnosticsImageViewAction = new QAction(tr("ImageView"), this);
    diagnosticsImageViewAction->setObjectName("diagnosticsImageView");
    diagnosticsImageViewAction->setShortcutVisibleInContextMenu(true);
    addAction(diagnosticsImageViewAction);
    connect(diagnosticsImageViewAction, &QAction::triggered, this, &MW::diagnosticsImageView);

    diagnosticsMetadataAction = new QAction(tr("Metadata"), this);
    diagnosticsMetadataAction->setObjectName("diagnosticsMetadata");
    diagnosticsMetadataAction->setShortcutVisibleInContextMenu(true);
    addAction(diagnosticsMetadataAction);
    connect(diagnosticsMetadataAction, &QAction::triggered, this, &MW::diagnosticsMetadata);

    diagnosticsDataModelAction = new QAction(tr("DataModel"), this);
    diagnosticsDataModelAction->setObjectName("diagnosticsDataModel");
    diagnosticsDataModelAction->setShortcutVisibleInContextMenu(true);
    addAction(diagnosticsDataModelAction);
    connect(diagnosticsDataModelAction, &QAction::triggered, this, &MW::diagnosticsDataModel);

    diagnosticsImageCacheAction = new QAction(tr("ImageCache"), this);
    diagnosticsImageCacheAction->setObjectName("diagnosticsImageCache");
    diagnosticsImageCacheAction->setShortcutVisibleInContextMenu(true);
    addAction(diagnosticsImageCacheAction);
    connect(diagnosticsImageCacheAction, &QAction::triggered, this, &MW::diagnosticsImageCache);

    diagnosticsEmbellishAction = new QAction(tr("Embellish"), this);
    diagnosticsEmbellishAction->setObjectName("diagnosticsEmbellish");
    diagnosticsEmbellishAction->setShortcutVisibleInContextMenu(true);
    addAction(diagnosticsEmbellishAction);
    connect(diagnosticsEmbellishAction, &QAction::triggered, this, &MW::diagnosticsEmbellish);

    // Non- Menu actions
    thriftyCacheAction = new QAction(tr("Thrifty Cache"), this);
    addAction(thriftyCacheAction);
    thriftyCacheAction->setShortcut(QKeySequence("F10"));
    connect(thriftyCacheAction, &QAction::triggered, this, &MW::thriftyCache);

    moderateCacheAction = new QAction(tr("Moderate Cache"), this);
    addAction(moderateCacheAction);
    moderateCacheAction->setShortcut(QKeySequence("F11"));
    connect(moderateCacheAction, &QAction::triggered, this, &MW::moderateCache);

    greedyCacheAction = new QAction(tr("Greedy Cache"), this);
    addAction(greedyCacheAction);
    greedyCacheAction->setShortcut(QKeySequence("F12"));
    connect(greedyCacheAction, &QAction::triggered, this, &MW::greedyCache);




    // Testing

    testAction = new QAction(tr("Test"), this);
    testAction->setObjectName("test");
    testAction->setShortcutVisibleInContextMenu(true);
    addAction(testAction);
    testAction->setShortcut(QKeySequence("Shift+Ctrl+Alt+T"));
    connect(testAction, &QAction::triggered, this, &MW::test);

    testNewFileFormatAction = new QAction(tr("Test Metadata"), this);
    testNewFileFormatAction->setObjectName("testNewFileFormat");
    testNewFileFormatAction->setShortcutVisibleInContextMenu(true);
    addAction(testNewFileFormatAction);
    testNewFileFormatAction->setShortcut(QKeySequence("Shift+Ctrl+Alt+F"));
    connect(testNewFileFormatAction, &QAction::triggered, this, &MW::testNewFileFormat);

    // Possibly needed actions

//    enterAction = new QAction(tr("Enter"), this);
//    enterAction->setObjectName("enterAction");
//    enterAction->setShortcut(QKeySequence("X"));
//    connect(enterAction, SIGNAL(triggered()), this, SLOT(enter()));

    // used in fsTree and bookmarks
    pasteAction = new QAction(tr("Paste files"), this);
    pasteAction->setObjectName("paste");
    pasteAction->setShortcutVisibleInContextMenu(true);
    addAction(pasteAction);
//        connect(pasteAction, SIGNAL(triggered()), this, SLOT(about()));
}

void MW::createMenus()
{
    if (G::isLogger) G::log(__FUNCTION__);
    // Main Menu

    // File Menu

    QMenu *fileMenu = new QMenu(this);
    QAction *fileGroupAct = new QAction("File", this);
    fileGroupAct->setMenu(fileMenu);
    fileMenu->addAction(openAction);
    fileMenu->addAction(openUsbAction);
    openWithMenu = fileMenu->addMenu(tr("Open with..."));
    openWithMenu->addAction(manageAppAction);
    openWithMenu->addSeparator();
    // add 10 dummy menu items for external apps
    for (int i = 0; i < 10; i++) {
        openWithMenu->addAction(appActions.at(i));
    }
    recentFoldersMenu = fileMenu->addMenu(tr("Recent folders"));
    // add maxRecentFolders dummy menu items for custom workspaces
    for (int i = 0; i < maxRecentFolders; i++) {
        recentFoldersMenu->addAction(recentFolderActions.at(i));
    }
    connect(recentFoldersMenu, SIGNAL(triggered(QAction*)),
            SLOT(invokeRecentFolder(QAction*)));
    fileMenu->addAction(revealFileAction);

    fileMenu->addSeparator();
    fileMenu->addAction(refreshCurrentAction);
    fileMenu->addAction(refreshFoldersAction);

    fileMenu->addSeparator();
    fileMenu->addAction(ingestAction);
    ingestHistoryFoldersMenu = fileMenu->addMenu(tr("Ingest History"));
    // add maxIngestHistoryFolders dummy menu items for custom workspaces
    for (int i = 0; i < maxIngestHistoryFolders; i++) {
        ingestHistoryFoldersMenu->addAction(ingestHistoryFolderActions.at(i));
    }
    connect(ingestHistoryFoldersMenu, SIGNAL(triggered(QAction*)),
            SLOT(invokeIngestHistoryFolder(QAction*)));
    fileMenu->addAction(ejectAction);

    fileMenu->addSeparator();
    fileMenu->addAction(colorManageAction);
    fileMenu->addAction(combineRawJpgAction);
    fileMenu->addAction(subFoldersAction);
     fileMenu->addAction(addBookmarkAction);
    fileMenu->addSeparator();
    fileMenu->addAction(saveAsFileAction);
    fileMenu->addSeparator();
    fileMenu->addAction(reportMetadataAction);
    fileMenu->addAction(mediaReadSpeedAction);
    fileMenu->addSeparator();
    fileMenu->addAction(exitAction);    // Appears in Winnow menu in OSX

    // Edit Menu

    QMenu *editMenu = new QMenu(this);
    QAction *editGroupAct = new QAction("Edit", this);
    editGroupAct->setMenu(editMenu);
    editMenu->addAction(selectAllAction);
    editMenu->addAction(invertSelectionAction);
    editMenu->addSeparator();
    editMenu->addAction(copyImageAction);
    editMenu->addAction(copyAction);
    editMenu->addAction(deleteAction);
    editMenu->addAction(deleteActiveFolderAction);
    editMenu->addSeparator();
    editMenu->addAction(pickAction);
    editMenu->addAction(rejectAction);
    editMenu->addAction(refineAction);
    editMenu->addAction(pickUnlessRejectedAction);
//    editMenu->addAction(filterPickAction);
    editMenu->addAction(popPickHistoryAction);
    editMenu->addSeparator();
    editMenu->addAction(searchTextEditAction);
    editMenu->addSeparator();
    ratingsMenu = editMenu->addMenu("Ratings");
    ratingsMenu->addAction(rate0Action);
    ratingsMenu->addAction(rate1Action);
    ratingsMenu->addAction(rate2Action);
    ratingsMenu->addAction(rate3Action);
    ratingsMenu->addAction(rate4Action);
    ratingsMenu->addAction(rate5Action);
//    editMenu->addSeparator();
    labelsMenu = editMenu->addMenu("Color labels");
    labelsMenu->addAction(label0Action);
    labelsMenu->addAction(label1Action);
    labelsMenu->addAction(label2Action);
    labelsMenu->addAction(label3Action);
    labelsMenu->addAction(label4Action);
    labelsMenu->addAction(label5Action);
    editMenu->addSeparator();
    editMenu->addAction(rotateRightAction);
    editMenu->addAction(rotateLeftAction);
    editMenu->addSeparator();
    utilitiesMenu = editMenu->addMenu("Utilities");
    utilitiesMenu->addAction(addThumbnailsAction);
    utilitiesMenu->addAction(reportHueCountAction);
    utilitiesMenu->addAction(meanStackAction);
    editMenu->addSeparator();
    editMenu->addAction(prefAction);       // Appears in Winnow menu in OSX
//    editMenu->addAction(oldPrefAction);

    // Go Menu

    QMenu *goMenu = new QMenu(this);
    QAction *goGroupAct = new QAction("Go", this);
    goGroupAct->setMenu(goMenu);
    goMenu->addAction(keyRightAction);
    goMenu->addAction(keyLeftAction);
    goMenu->addAction(keyUpAction);
    goMenu->addAction(keyDownAction);
    goMenu->addAction(keyPageUpAction);
    goMenu->addAction(keyPageDownAction);
    goMenu->addAction(keyHomeAction);
    goMenu->addAction(keyEndAction);
    goMenu->addSeparator();
    goMenu->addAction(keyScrollLeftAction);
    goMenu->addAction(keyScrollRightAction);
    goMenu->addAction(keyScrollUpAction);
    goMenu->addAction(keyScrollDownAction);
    goMenu->addAction(keyScrollPageLeftAction);
    goMenu->addAction(keyScrollPageRightAction);
    goMenu->addAction(keyScrollPageUpAction);
    goMenu->addAction(keyScrollPageDownAction);
    goMenu->addSeparator();
    goMenu->addAction(nextPickAction);
    goMenu->addAction(prevPickAction);
    goMenu->addSeparator();
    goMenu->addAction(randomImageAction);

    // Filter Menu

    QMenu *filterMenu = new QMenu(this);
    QAction *filterGroupAct = new QAction("Filter", this);
    filterGroupAct->setMenu(filterMenu);
    filterMenu->addAction(filterUpdateAction);
    filterMenu->addAction(clearAllFiltersAction);
    filterMenu->addSeparator();
    filterMenu->addAction(filterPickAction);
    filterMenu->addSeparator();
    filterMenu->addAction(filterSearchAction);
    filterMenu->addSeparator();
    filterMenu->addAction(filterRating1Action);
    filterMenu->addAction(filterRating2Action);
    filterMenu->addAction(filterRating3Action);
    filterMenu->addAction(filterRating4Action);
    filterMenu->addAction(filterRating5Action);
    filterMenu->addSeparator();
    filterMenu->addAction(filterRedAction);
    filterMenu->addAction(filterYellowAction);
    filterMenu->addAction(filterGreenAction);
    filterMenu->addAction(filterBlueAction);
    filterMenu->addAction(filterPurpleAction);
    filterMenu->addSeparator();
    filterMenu->addAction(filterLastDayAction);
    filterMenu->addSeparator();
//    filterMenu->addAction(filterUpdateAction);
//    filterMenu->addAction(filterInvertAction);
    filterMenu->addAction(filterLastDayAction);

    // Sort Menu

    /*QMenu **/sortMenu = new QMenu(this);
    QAction *sortGroupAct = new QAction("Sort", this);
    sortGroupAct->setMenu(sortMenu);
    sortMenu->addActions(sortGroupAction->actions());
    sortMenu->addSeparator();
    sortMenu->addAction(sortReverseAction);

    // Embellish Menu

    QMenu *embelMenu = new QMenu(this);
//    embelMenu->setIcon(QIcon(":/images/icon16/lightning.png"));
    embelMenu->addAction(embelExportAction);
    embelMenu->addSeparator();
    embelMenu->addAction(embelNewTemplateAction);
    embelMenu->addAction(embelReadTemplateAction);
    embelMenu->addAction(embelSaveTemplateAction);
    embelMenu->addSeparator();
//    embelMenu->addAction(embelTileAction);
    embelMenu->addAction(embelManageTilesAction);
    embelMenu->addAction(embelManageGraphicsAction);
//    #ifdef Q_OS_WIN
    embelMenu->addSeparator();
    embelMenu->addAction(embelRevealWinnetsAction);
//    #endif
    QAction *embelGroupAct = new QAction("Embellish", this);
    embelGroupAct->setMenu(embelMenu);
    embelMenu->addSeparator();
    embelMenu->addActions(embelGroupAction->actions());
    connect(embelMenu, &QMenu::triggered, embelProperties, &EmbelProperties::invokeFromAction);

    // View Menu

    /*QMenu **/viewMenu = new QMenu(this);
    QAction *viewGroupAct = new QAction("View", this);
    viewGroupAct->setMenu(viewMenu);
    viewMenu->addActions(centralGroupAction->actions());
    viewMenu->addSeparator();
    viewMenu->addAction(slideShowAction);
    viewMenu->addAction(fullScreenAction);
    viewMenu->addAction(escapeFullScreenAction);
    viewMenu->addSeparator();
    viewMenu->addAction(ratingBadgeVisibleAction);
    viewMenu->addAction(infoVisibleAction);
    viewMenu->addAction(infoSelectAction);
    viewMenu->addSeparator();
    viewMenu->addAction(zoomToAction);
    viewMenu->addAction(zoomInAction);
    viewMenu->addAction(zoomOutAction);
    viewMenu->addAction(zoomToggleAction);
    viewMenu->addSeparator();
//    viewMenu->addAction(thumbsWrapAction);
    viewMenu->addAction(thumbsEnlargeAction);
    viewMenu->addAction(thumbsShrinkAction);
//    viewMenu->addAction(thumbsFitAction);
//    viewMenu->addAction(showThumbLabelsAction);

    // Window Menu

    QMenu *windowMenu = new QMenu(this);
    QAction *windowGroupAct = new QAction("Window", this);
    windowGroupAct->setMenu(windowMenu);
    workspaceMenu = windowMenu->addMenu(tr("&Workspace"));
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
    windowMenu->addSeparator();
    windowMenu->addAction(folderDockVisibleAction);
    windowMenu->addAction(favDockVisibleAction);
    windowMenu->addAction(filterDockVisibleAction);
    windowMenu->addAction(metadataDockVisibleAction);
    windowMenu->addAction(thumbDockVisibleAction);
    if (!hideEmbellish) windowMenu->addAction(embelDockVisibleAction);
    windowMenu->addSeparator();
//    windowMenu->addAction(windowTitleBarVisibleAction);
#ifdef Q_OS_WIN
    windowMenu->addAction(menuBarVisibleAction);
#endif
    windowMenu->addAction(statusBarVisibleAction);

    // Help Menu

    QMenu *helpMenu = new QMenu(this);
    QAction *helpGroupAct = new QAction("Help", this);
    helpGroupAct->setMenu(helpMenu);
    helpMenu->addAction(checkForUpdateAction);
    helpMenu->addSeparator();
    helpMenu->addAction(aboutAction);
//    helpMenu->addAction(helpAction);
    helpMenu->addAction(helpShortcutsAction);
    helpMenu->addAction(helpWelcomeAction);
//    helpMenu->addSeparator();
//    helpMenu->addAction(helpRevealLogFileAction);
    helpMenu->addSeparator();
    helpDiagnosticsMenu = helpMenu->addMenu(tr("&Diagnostics"));
    helpDiagnosticsMenu->addAction(diagnosticsAllAction);
    helpDiagnosticsMenu->addAction(diagnosticsCurrentAction);
    helpDiagnosticsMenu->addAction(diagnosticsErrorsAction);
    helpDiagnosticsMenu->addAction(diagnosticsMainAction);
    helpDiagnosticsMenu->addAction(diagnosticsGridViewAction);
    helpDiagnosticsMenu->addAction(diagnosticsThumbViewAction);
    helpDiagnosticsMenu->addAction(diagnosticsImageViewAction);
    helpDiagnosticsMenu->addAction(diagnosticsMetadataAction);
    helpDiagnosticsMenu->addAction(diagnosticsDataModelAction);
    helpDiagnosticsMenu->addAction(diagnosticsImageCacheAction);
    helpDiagnosticsMenu->addAction(diagnosticsEmbellishAction);

    // Separator Action
    QAction *separatorAction = new QAction(this);
    separatorAction->setSeparator(true);
    QAction *separatorAction1 = new QAction(this);
    separatorAction1->setSeparator(true);
    QAction *separatorAction2 = new QAction(this);
    separatorAction2->setSeparator(true);
    QAction *separatorAction3 = new QAction(this);
    separatorAction3->setSeparator(true);
    QAction *separatorAction4 = new QAction(this);
    separatorAction4->setSeparator(true);
    QAction *separatorAction5 = new QAction(this);
    separatorAction5->setSeparator(true);

    // FSTree context menu
    fsTreeActions = new QList<QAction *>;
//    QList<QAction *> *fsTreeActions = new QList<QAction *>;
    fsTreeActions->append(refreshFoldersAction);
    fsTreeActions->append(updateImageCountAction);
    fsTreeActions->append(collapseFoldersAction);
    fsTreeActions->append(ejectActionFromContextMenu);
    fsTreeActions->append(separatorAction);
//    fsTreeActions->append(showImageCountAction);
    fsTreeActions->append(revealFileActionFromContext);
    fsTreeActions->append(copyPathActionFromContext);
    fsTreeActions->append(deleteFSTreeFolderAction);
    fsTreeActions->append(separatorAction1);
    fsTreeActions->append(pasteAction);
    fsTreeActions->append(separatorAction2);
    fsTreeActions->append(addBookmarkActionFromContext);
    fsTreeActions->append(separatorAction3);

    // bookmarks context menu
    QList<QAction *> *favActions = new QList<QAction *>;
    favActions->append(refreshBookmarkAction);
    favActions->append(revealFileActionFromContext);
    favActions->append(copyPathActionFromContext);
//    favActions->append(pasteAction);
    favActions->append(separatorAction);
    favActions->append(removeBookmarkAction);
    // not being used due to risk of folder containing many subfolders with no indication to user
//    favActions->append(deleteBookmarkFolderAction);
//    favActions->append(separatorAction1);

    // filters context menu
    filterActions = new QList<QAction *>;
//    QList<QAction *> *filterActions = new QList<QAction *>;
    filterActions->append(filterUpdateAction);
    filterActions->append(clearAllFiltersAction);
    filterActions->append(searchTextEditAction);
    filterActions->append(separatorAction);
    filterActions->append(expandAllFiltersAction);
    filterActions->append(collapseAllFiltersAction);
    filterActions->append(filterSoloAction);
//    filterActions->append(separatorAction1);

    // metadata context menu
    QList<QAction *> *metadataActions = new QList<QAction *>;
//    metadataActions->append(infoView->copyInfoAction);
    metadataActions->append(reportMetadataAction);
    metadataActions->append(prefInfoAction);
    metadataActions->append(separatorAction);
//    metadataActions->append(metadataDockLockAction);
    metadataActions->append(metadataFixedSizeAction);

    // Open with Menu (used in thumbview context menu)
    QAction *openWithGroupAct = new QAction(tr("Open with..."), this);
    openWithGroupAct->setMenu(openWithMenu);

    // thumbview context menu
    QList<QAction *> *thumbViewActions = new QList<QAction *>;
    thumbViewActions->append(pickMouseOverAction);
    thumbViewActions->append(revealFileAction);
    thumbViewActions->append(openWithGroupAct);
    thumbViewActions->append(separatorAction);
    thumbViewActions->append(selectAllAction);
    thumbViewActions->append(invertSelectionAction);
    thumbViewActions->append(separatorAction1);
    thumbViewActions->append(rotateRightAction);
    thumbViewActions->append(rotateLeftAction);
    thumbViewActions->append(separatorAction2);
//    thumbViewActions->append(thumbsWrapAction);
    thumbViewActions->append(thumbsEnlargeAction);
    thumbViewActions->append(thumbsShrinkAction);
    thumbViewActions->append(separatorAction3);
    thumbViewActions->append(sortReverseAction);
    thumbViewActions->append(separatorAction4);
    thumbViewActions->append(copyImageAction);
    thumbViewActions->append(saveAsFileAction);
    thumbViewActions->append(deleteAction);
    thumbViewActions->append(separatorAction5);
    thumbViewActions->append(reportMetadataAction);
    thumbViewActions->append(diagnosticsCurrentAction);
    thumbViewActions->append(diagnosticsErrorsAction);

//    // imageview/tableview/gridview/compareview context menu
//    imageView->addAction(pickAction);
//    imageView->addAction(filterPickAction);
//    imageView->addAction(ingestAction);
//    addMenuSeparator(imageView);


//    QMenu *zoomSubMenu = new QMenu(imageView);
//    QAction *zoomGroupAct = new QAction("Zoom", this);
//    imageView->addAction(zoomGroupAct);
//    zoomGroupAct->setMenu(zoomSubMenu);
//    zoomSubMenu->addAction(zoomInAction);
//    zoomSubMenu->addAction(zoomOutAction);
//    zoomSubMenu->addAction(zoomToggleAction);

//    QMenu *transformSubMenu = new QMenu(imageView);
//    QAction *transformGroupAct = new QAction("Rotate", this);
//    imageView->addAction(transformGroupAct);
//    transformGroupAct->setMenu(transformSubMenu);
//    transformSubMenu->addAction(rotateRightAction);
//    transformSubMenu->addAction(rotateLeftAction);

//    QMenu *thumbNailSubMenu = new QMenu(imageView);
//    QAction *thumbnailGroupAct = new QAction("ThumbNails", this);
//    imageView->addAction(thumbnailGroupAct);
//    thumbnailGroupAct->setMenu(thumbNailSubMenu);
//    thumbNailSubMenu->addAction(thumbsEnlargeAction);
//    thumbNailSubMenu->addAction(thumbsShrinkAction);
////    thumbNailSubMenu->addAction(showThumbLabelsAction);
//    sortMenu = thumbNailSubMenu->addMenu(tr("Sort By"));
//    sortMenu->addActions(sortGroupAction->actions());
//    sortMenu->addSeparator();
//    sortMenu->addAction(sortReverseAction);

//    addMenuSeparator(imageView);

    // main menu
    menuBar()->addAction(fileGroupAct);
    menuBar()->addAction(editGroupAct);
    menuBar()->addMenu(goMenu);
    menuBar()->addAction(goGroupAct);
    menuBar()->addAction(filterGroupAct);
    menuBar()->addAction(sortGroupAct);
    menuBar()->addAction(embelGroupAct);
    menuBar()->addAction(viewGroupAct);
    menuBar()->addAction(windowGroupAct);
    menuBar()->addAction(helpGroupAct);
    menuBar()->setVisible(true);

    // main context menu
    mainContextActions = new QList<QAction *>;
    mainContextActions->append(fileGroupAct);
    mainContextActions->append(editGroupAct);
    mainContextActions->append(goGroupAct);
    mainContextActions->append(filterGroupAct);
    mainContextActions->append(sortGroupAct);
    mainContextActions->append(embelGroupAct);
    mainContextActions->append(viewGroupAct);
    mainContextActions->append(windowGroupAct);
    mainContextActions->append(helpGroupAct);

    // Central Widget mode context menu
    centralWidget->addActions(*mainContextActions);
    centralWidget->setContextMenuPolicy(Qt::ActionsContextMenu);

    /*
    imageView->addActions(*mainContextActions);
    imageView->setContextMenuPolicy(Qt::ActionsContextMenu);

    tableView->addActions(*mainContextActions);
    tableView->setContextMenuPolicy(Qt::ActionsContextMenu);

    gridView->addActions(*mainContextActions);
    gridView->setContextMenuPolicy(Qt::ActionsContextMenu);

    compareImages->addActions(*mainContextActions);
    compareImages->setContextMenuPolicy(Qt::ActionsContextMenu);
    //*/

    // docking panels context menus
    fsTree->addActions(*fsTreeActions);
    fsTree->setContextMenuPolicy(Qt::ActionsContextMenu);

    bookmarks->addActions(*favActions);
    bookmarks->setContextMenuPolicy(Qt::ActionsContextMenu);

    filters->addActions(*filterActions);
    filters->setContextMenuPolicy(Qt::ActionsContextMenu);

    if (useInfoView) {
        infoView->addActions(*metadataActions);
        infoView->setContextMenuPolicy(Qt::ActionsContextMenu);
    }

    thumbView->addActions(*thumbViewActions);
    thumbView->setContextMenuPolicy(Qt::ActionsContextMenu);

    QLabel *label = new QLabel;
    label->setText(" TEST ");
    label->setStyleSheet("QLabel{color:yellow;}");

//    menuBar()->setCornerWidget(label, Qt::TopRightCorner);
//    menuBar()->setCornerWidget(filterLabel, Qt::TopRightCorner);

    QToolBar *toolBar = new QToolBar;
    toolBar->addWidget(label);
//    menuBar()->setCornerWidget(toolBar);
}

void MW::addMenuSeparator(QWidget *widget)
{
    QAction *separator = new QAction(this);
    separator->setSeparator(true);
    widget->addAction(separator);
}

void MW::enableEjectUsbMenu(QString path)
{
    if (G::isLogger) G::log(__FUNCTION__);
    if(Usb::isUsb(path)) ejectAction->setEnabled(true);
    else ejectAction->setEnabled(false);
}

void MW::enableSelectionDependentMenus()
{
/*
    rgh check if still working
*/
    if (G::isLogger) G::log(__FUNCTION__);

    if(selectionModel->selectedRows().count() > 0) {
        openWithMenu->setEnabled(true);
        ingestAction->setEnabled(true);
        revealFileAction->setEnabled(true);
//        subFoldersAction->setEnabled(true);
        addBookmarkAction->setEnabled(true);
        reportMetadataAction->setEnabled(true);
        selectAllAction->setEnabled(true);
        invertSelectionAction->setEnabled(true);
        refineAction->setEnabled(true);
        pickAction->setEnabled(true);
        filterPickAction->setEnabled(true);
        popPickHistoryAction->setEnabled(true);
        copyAction->setEnabled(true);
        rate0Action->setEnabled(true);
        rate1Action->setEnabled(true);
        rate2Action->setEnabled(true);
        rate3Action->setEnabled(true);
        rate4Action->setEnabled(true);
        rate5Action->setEnabled(true);
        label0Action->setEnabled(true);
        label1Action->setEnabled(true);
        label2Action->setEnabled(true);
        label3Action->setEnabled(true);
        label4Action->setEnabled(true);
        label5Action->setEnabled(true);
        rotateRightAction->setEnabled(true);
        rotateLeftAction->setEnabled(true);
        keyRightAction->setEnabled(true);
        keyLeftAction->setEnabled(true);
        keyUpAction->setEnabled(true);
        keyDownAction->setEnabled(true);
        keyHomeAction->setEnabled(true);
        keyEndAction->setEnabled(true);
        nextPickAction->setEnabled(true);
        prevPickAction->setEnabled(true);
//        clearAllFiltersAction->setEnabled(true);
        filterPickAction->setEnabled(true);
        filterRating1Action->setEnabled(true);
        filterRating2Action->setEnabled(true);
        filterRating3Action->setEnabled(true);
        filterRating4Action->setEnabled(true);
        filterRating5Action->setEnabled(true);
        filterRedAction->setEnabled(true);
        filterYellowAction->setEnabled(true);
        filterGreenAction->setEnabled(true);
        filterBlueAction->setEnabled(true);
        filterPurpleAction->setEnabled(true);
//        filterInvertAction->setEnabled(true);
        sortGroupAction->setEnabled(true);
        sortReverseAction->setEnabled(true);
        zoomToAction->setEnabled(true);
        zoomInAction->setEnabled(true);
        zoomOutAction->setEnabled(true);
        zoomToggleAction->setEnabled(true);
//        thumbsWrapAction->setEnabled(true);
        thumbsEnlargeAction->setEnabled(true);
        thumbsShrinkAction->setEnabled(true);
    }
    else {
        openWithMenu->setEnabled(false);
        ingestAction->setEnabled(false);
        revealFileAction->setEnabled(false);
//        subFoldersAction->setEnabled(false);
        addBookmarkAction->setEnabled(false);
        reportMetadataAction->setEnabled(false);
        selectAllAction->setEnabled(false);
        invertSelectionAction->setEnabled(false);
        refineAction->setEnabled(false);
        pickAction->setEnabled(false);
        filterPickAction->setEnabled(false);
        popPickHistoryAction->setEnabled(false);
        copyAction->setEnabled(false);
        rate0Action->setEnabled(false);
        rate1Action->setEnabled(false);
        rate2Action->setEnabled(false);
        rate3Action->setEnabled(false);
        rate4Action->setEnabled(false);
        rate5Action->setEnabled(false);
        label0Action->setEnabled(false);
        label1Action->setEnabled(false);
        label2Action->setEnabled(false);
        label3Action->setEnabled(false);
        label4Action->setEnabled(false);
        label5Action->setEnabled(false);
        rotateRightAction->setEnabled(false);
        rotateLeftAction->setEnabled(false);
        keyRightAction->setEnabled(false);
        keyLeftAction->setEnabled(false);
        keyUpAction->setEnabled(false);
        keyDownAction->setEnabled(false);
        keyHomeAction->setEnabled(false);
        keyEndAction->setEnabled(false);
        nextPickAction->setEnabled(false);
        prevPickAction->setEnabled(false);
//        clearAllFiltersAction->setEnabled(false);
//        filterPickAction->setEnabled(false);
//        filterRating1Action->setEnabled(false);
//        filterRating2Action->setEnabled(false);
//        filterRating3Action->setEnabled(false);
//        filterRating4Action->setEnabled(false);
//        filterRating5Action->setEnabled(false);
//        filterRedAction->setEnabled(false);
//        filterYellowAction->setEnabled(false);
//        filterGreenAction->setEnabled(false);
//        filterBlueAction->setEnabled(false);
//        filterPurpleAction->setEnabled(false);
//        filterInvertAction->setEnabled(false);
//        sortGroupAction->setEnabled(false);
//        sortReverseAction->setEnabled(false);

        zoomToAction->setEnabled(false);
        zoomInAction->setEnabled(false);
        zoomOutAction->setEnabled(false);
        zoomToggleAction->setEnabled(false);
//        thumbsWrapAction->setEnabled(false);
        thumbsEnlargeAction->setEnabled(false);
        thumbsShrinkAction->setEnabled(false);
    }
}

void MW::createCentralWidget()
{
    if (G::isLogger) G::log(__FUNCTION__);
    // centralWidget required by ImageView/CompareView constructors
    centralWidget = new QWidget(this);
    centralWidget->setObjectName("centralWidget");
    // stack layout for loupe, table, compare and grid displays
    centralLayout = new QStackedLayout;
    centralLayout->setContentsMargins(0, 0, 0, 0);
}

void MW::setupCentralWidget()
{
    if (G::isLogger) G::log(__FUNCTION__);
    welcome = new QScrollArea;
    Ui::welcomeScrollArea ui;
    ui.setupUi(welcome);

    centralLayout->addWidget(imageView);
    centralLayout->addWidget(compareImages);
    centralLayout->addWidget(tableView);
    centralLayout->addWidget(gridView);
    centralLayout->addWidget(welcome);     // first time open program tips
    centralLayout->addWidget(messageView);
    centralWidget->setLayout(centralLayout);
    setCentralWidget(centralWidget);
}

void MW::createDataModel()
{
    if (G::isLogger) G::log(__FUNCTION__);
    metadata = new Metadata;
    cacheProgressBar = new ProgressBar(this);

    // loadSettings not run yet
    if (isSettings && setting->contains("combineRawJpg")) combineRawJpg = setting->value("combineRawJpg").toBool();
    else combineRawJpg = true;
    dm = new DataModel(this, metadata, filters, combineRawJpg);
    thumb = new Thumb(this, dm, metadata);

    // show appropriate count column in filters
    if (combineRawJpg) {
        filters->hideColumn(3);
        filters->showColumn(4);
    }
    else {
        filters->hideColumn(4);
        filters->showColumn(3);
    }

    connect(filters, &Filters::searchStringChange, dm, &DataModel::searchStringChange);
    connect(dm, &DataModel::updateClassification, this, &MW::updateClassification);
    connect(dm, &DataModel::msg, this, &MW::setCentralMessage);
    connect(dm, &DataModel::updateStatus, this, &MW::updateStatus);

    buildFilters = new BuildFilters(this, dm, metadata, filters, combineRawJpg);

    connect(buildFilters, &BuildFilters::updateProgress, filters, &Filters::updateProgress);
    connect(buildFilters, &BuildFilters::finishedBuildFilters, filters, &Filters::finishedBuildFilters);
}

void MW::createSelectionModel()
{
/*
    The application only has one selection model which is shared by ThumbView, GridView and
    TableView, insuring that each view is in sync, except when a view is hidden.
*/
    if (G::isLogger) G::log(__FUNCTION__);
    // set a common selection model for all views
    selectionModel = new QItemSelectionModel(dm->sf);
    thumbView->setSelectionModel(selectionModel);
    tableView->setSelectionModel(selectionModel);
    gridView->setSelectionModel(selectionModel);

    // whenever currentIndex changes do a file selection change
    connect(selectionModel, &QItemSelectionModel::currentChanged,
            this, &MW::fileSelectionChange);
}

void MW::createCaching()
{
    if (G::isLogger) G::log(__FUNCTION__);
    metadataCacheThread = new MetadataCache(this, dm, metadata);

    if (isSettings) {
        if (setting->contains("cacheAllMetadata")) metadataCacheThread->cacheAllMetadata = setting->value("cacheAllMetadata").toBool();
        if (setting->contains("cacheAllIcons")) metadataCacheThread->cacheAllIcons = setting->value("cacheAllIcons").toBool();
    }
    else {
        metadataCacheThread->cacheAllMetadata = true;
        metadataCacheThread->cacheAllIcons = false;
    }
    metadataCacheThread->metadataChunkSize = 3000;

    /* When a new folder is selected the metadataCacheThread is started to load all the
    metadata and thumbs for each image. If the user scrolls during the cache process then the
    metadataCacheThread is restarted at the first visible thumb to speed up the display of the
    thumbs for the user. However, if every scroll event triggered a restart it would be
    inefficient, so this timer is used to wait for a pause in the scrolling before triggering
    a restart at the new place.
    */
    metadataCacheScrollTimer = new QTimer(this);
    metadataCacheScrollTimer->setSingleShot(true);
    // next connect to update
    connect(metadataCacheScrollTimer, SIGNAL(timeout()), this,
            SLOT(loadMetadataChunk()));

    connect(metadataCacheThread, SIGNAL(updateIsRunning(bool,bool,QString)),
            this, SLOT(updateMetadataThreadRunStatus(bool,bool,QString)));

    connect(metadataCacheThread, SIGNAL(updateIconBestFit()),
            this, SLOT(updateIconBestFit()));

    connect(metadataCacheThread, SIGNAL(selectFirst()),
            thumbView, SLOT(selectFirst()));

    connect(metadataCacheThread, SIGNAL(finished2ndPass()),
            this, SLOT(launchBuildFilters()));

//    connect(metadataCacheThread, SIGNAL(refreshCurrentAfterReload()),
//            this, SLOT(refreshCurrentAfterReload()));

    // show progress bar when executing loadEntireMetadataCache
    connect(metadataCacheThread, &MetadataCache::showCacheStatus,
            this, &MW::setCentralMessage);

//    connect(metadataCacheThread, &MetadataCache::scrollToCurrent, this, &MW::scrollToCurrentRow);

    icd = new ImageCacheData(this);
    imageCacheThread = new ImageCache(this, icd, dm, metadata);

    /* Image caching is triggered from the metadataCacheThread to avoid the two threads
       running simultaneously and colliding */

    // load image cache for a new folder
    connect(metadataCacheThread, SIGNAL(loadImageCache()),
            this, SLOT(loadImageCacheForNewFolder()));

    // 2nd pass loading image cache for a new folder
    connect(metadataCacheThread, SIGNAL(loadMetadataCache2ndPass()),
            this, SLOT(loadMetadataCache2ndPass()));

    connect(imageCacheThread, SIGNAL(updateIsRunning(bool,bool)),
            this, SLOT(updateImageCachingThreadRunStatus(bool,bool)));

    // Update the cache status progress bar when changed in ImageCache
    connect(imageCacheThread, &ImageCache::showCacheStatus,
            this, &MW::updateImageCacheStatus);

    // Signal from ImageCache::run() to update cache status in datamodel
    connect(imageCacheThread, SIGNAL(updateCacheOnThumbs(QString,bool)),
            this, SLOT(setCachedStatus(QString,bool)));

    // Signal to ImageCache new image selection
    connect(this, SIGNAL(setImageCachePosition(QString)),
            imageCacheThread, SLOT(setCurrentPosition(QString)));
}

void MW::createThumbView()
{
    if (G::isLogger) G::log(__FUNCTION__);
    thumbView = new IconView(this, dm, "Thumbnails");
    thumbView->setObjectName("Thumbnails");
//    thumbView->setSpacing(0);                // thumbView not visible without this
    thumbView->setAutoScroll(false);
    thumbView->firstVisibleCell = 0;
    thumbView->showZoomFrame = true;            // may have settings but not showZoomFrame yet
    if (isSettings) {
        // loadSettings has not run yet (dependencies, but QSettings has been opened
        if (setting->contains("thumbWidth")) thumbView->iconWidth = setting->value("thumbWidth").toInt();
        if (setting->contains("thumbHeight")) thumbView->iconHeight = setting->value("thumbHeight").toInt();
        if (setting->contains("labelFontSize")) thumbView->labelFontSize = setting->value("labelFontSize").toInt();
        if (setting->contains("showThumbLabels")) thumbView->showIconLabels = setting->value("showThumbLabels").toBool();
        if (setting->contains("showZoomFrame")) thumbView->showZoomFrame = setting->value("showZoomFrame").toBool();
        if (setting->contains("classificationBadgeInThumbDiameter")) thumbView->badgeSize = setting->value("classificationBadgeInThumbDiameter").toInt();
        if (setting->contains("thumbsPerPage")) thumbView->visibleCells = setting->value("thumbsPerPage").toInt();
    }
    else {
        thumbView->iconWidth = 100;
        thumbView->iconHeight = 100;
        thumbView->labelFontSize = 12;
        thumbView->showIconLabels = false;
        thumbView->showZoomFrame = true;
        thumbView->badgeSize = classificationBadgeInThumbDiameter;
        thumbView->visibleCells = width() / thumbView->iconWidth;
    }
    // double mouse click fires displayLoupe
    connect(thumbView, SIGNAL(displayLoupe()), this, SLOT(loupeDisplay()));

    // back and forward mouse buttons toggle pick
    connect(thumbView, &IconView::togglePick, this, &MW::togglePick);

//    connect(thumbView, SIGNAL(updateThumbDockHeight()),
//            this, SLOT(setThumbDockHeight()));

    connect(thumbView, &IconView::updateStatus, this, &MW::updateStatus);

    connect(thumbView->verticalScrollBar(), SIGNAL(valueChanged(int)),
            this, SLOT(thumbHasScrolled()));

    connect(thumbView->horizontalScrollBar(), SIGNAL(valueChanged(int)),
            this, SLOT(thumbHasScrolled()));
}

void MW::createGridView()
{
    if (G::isLogger) G::log(__FUNCTION__);
    gridView = new IconView(this, dm, "Grid");
    gridView->setObjectName("Grid");
    gridView->setSpacing(0);                // gridView not visible without this
    gridView->setWrapping(true);
    gridView->setAutoScroll(false);
    gridView->firstVisibleCell = 0;

    if (isSettings) {
        if (setting->contains("thumbWidthGrid")) gridView->iconWidth = setting->value("thumbWidthGrid").toInt();
        if (setting->contains("thumbHeightGrid")) gridView->iconHeight = setting->value("thumbHeightGrid").toInt();
        if (setting->contains("labelFontSizeGrid")) gridView->labelFontSize = setting->value("labelFontSizeGrid").toInt();
        if (setting->contains("showThumbLabelsGrid")) gridView->showIconLabels = setting->value("showThumbLabelsGrid").toBool();
        if (setting->contains("classificationBadgeInThumbDiameter")) gridView->badgeSize = setting->value("classificationBadgeInThumbDiameter").toInt();
        if (setting->contains("thumbsPerPage")) gridView->visibleCells = setting->value("thumbsPerPage").toInt();
    }
    else {
        gridView->iconWidth = 200;
        gridView->iconHeight = 200;
        gridView->labelFontSize = 10;
        gridView->showIconLabels = true;
        gridView->badgeSize = classificationBadgeInThumbDiameter;
        // rgh has window size been assigned yet
        gridView->visibleCells = (width() / 200) * (height() / 200);
    }

    // double mouse click fires displayLoupe
    connect(gridView, &IconView::displayLoupe, this, &MW::loupeDisplay);
    // update metadata and icons if not loaded for new images when scroll
    connect(gridView->verticalScrollBar(), &QScrollBar::valueChanged,
            this, &MW::gridHasScrolled);
}

void MW::createTableView()
{
/*
    TableView includes all the metadata used for each image. It is useful for sorting on any
    column and to check for information to filter. Creation is dependent on datamodel and
    thumbView.
*/
    if (G::isLogger) G::log(__FUNCTION__);
    tableView = new TableView(dm);
    tableView->setAutoScroll(false);

    if (isSettings) {
        /* read TableView okToShow fields */
        setting->beginGroup("TableFields");
        QStringList setFields = setting->childKeys();
        QList<QStandardItem *> itemList;
        setFields = setting->childKeys();
        for (int i = 0; i <setFields.size(); ++i) {
            QString setField = setFields.at(i);
            bool okToShow = setting->value(setField).toBool();
            itemList = tableView->ok->findItems(setField);
            if (itemList.length()) {
                int row = itemList[0]->row();
                QModelIndex idx = tableView->ok->index(row, 1);
                tableView->ok->setData(idx, okToShow, Qt::EditRole);
            }
        }
        setting->endGroup();
    }

    // update menu "sort by" to match tableView sort change
    connect(tableView->horizontalHeader(), &QHeaderView::sortIndicatorChanged,
            this, &MW::sortIndicatorChanged);
    // change to loupe view if double click or enter in tableview
    connect(tableView, &TableView::displayLoupe, this, &MW::loupeDisplay);
    // sync scrolling between tableview and thumbview
    connect(tableView->verticalScrollBar(), &QScrollBar::valueChanged,
            this, &MW::tableHasScrolled);
}

void MW::createImageView()
{
/*
    ImageView displays the image in the central widget. The image is either from the image
    cache or read from the file if the cache is unavailable. Creation is dependent on
    metadata, imageCacheThread, thumbView, datamodel and settings.
*/
    if (G::isLogger) G::log(__FUNCTION__);
    /* This is the info displayed on top of the image in loupe view. It is
       dependent on template data stored in QSettings */
    // start with defaults
    infoString->loupeInfoTemplate = "Default info";
    if (isSettings) {
        // load info templates
        setting->beginGroup("InfoTemplates");
        QStringList keys = setting->childKeys();
        for (int i = 0; i < keys.size(); ++i) {
            QString key = keys.at(i);
            infoString->infoTemplates[key] = setting->value(key).toString();
        }
        setting->endGroup();
        // if loupeInfoTemplate is in QSettings and info templates then assign
        if (setting->contains("loupeInfoTemplate")) {
            QString displayInfoTemplate = setting->value("loupeInfoTemplate").toString();
            if (infoString->infoTemplates.contains(displayInfoTemplate))
                infoString->loupeInfoTemplate = displayInfoTemplate;
        }
    }

    // prep pass values: first use of program vs settings have been saved
    if (isSettings) {
        if (setting->contains("isImageInfoVisible")) isImageInfoVisible = setting->value("isImageInfoVisible").toBool();
        if (setting->contains("infoOverlayFontSize")) infoOverlayFontSize = setting->value("infoOverlayFontSize").toInt();
    }
    else {
        // parameters already defined in loadSettings
    }

    imageView = new ImageView(this,
                              centralWidget,
                              metadata,
                              dm,
                              icd,
                              thumbView,
                              infoString,
                              setting->value("isImageInfoVisible").toBool(),
                              setting->value("isRatingBadgeVisible").toBool(),
                              setting->value("classificationBadgeInImageDiameter").toInt(),
                              setting->value("infoOverlayFontSize").toInt());

    if (isSettings) {
        if (setting->contains("limitFit100Pct")) imageView->limitFit100Pct = setting->value("limitFit100Pct").toBool();
        if (setting->contains("infoOverlayFontSize")) imageView->infoOverlayFontSize = setting->value("infoOverlayFontSize").toInt();
        if (setting->contains("lastPrefPage")) lastPrefPage = setting->value("lastPrefPage").toInt();
        qreal tempZoom = setting->value("toggleZoomValue").toReal();
        if (tempZoom > 3) tempZoom = 1.0;
        if (tempZoom < 0.25) tempZoom = 1.0;
        imageView->toggleZoom = tempZoom;
    }
    else {
        imageView->limitFit100Pct = true;
        imageView->toggleZoom = 1.0;
        imageView->infoOverlayFontSize = infoOverlayFontSize;   // defined in loadSettings
    }

    connect(imageView, &ImageView::togglePick, this, &MW::togglePick);
    connect(imageView, &ImageView::updateStatus, this, &MW::updateStatus);
    connect(thumbView, &IconView::thumbClick, imageView, &ImageView::thumbClick);
    connect(imageView, &ImageView::handleDrop, this, &MW::handleDrop);
    connect(imageView, &ImageView::killSlideshow, this, &MW::slideShow);
}

void MW::createCompareView()
{
    if (G::isLogger) G::log(__FUNCTION__);
    compareImages = new CompareImages(this, centralWidget, metadata, dm, thumbView, icd);

    if (isSettings) {
        if (setting->contains("lastPrefPage")) lastPrefPage = setting->value("lastPrefPage").toInt();
        qreal tempZoom = setting->value("toggleZoomValue").toReal();
        if (tempZoom > 3) tempZoom = 1;
        if (tempZoom < 0.25) tempZoom = 1;
        compareImages->toggleZoom = tempZoom;
    }
    else {
        imageView->toggleZoom = 1.0;
    }

    connect(compareImages, &CompareImages::updateStatus, this, &MW::updateStatus);

    connect(compareImages, &CompareImages::togglePick, this, &MW::togglePick);
}

void MW::createInfoString()
{
/*
    This is the info displayed on top of the image in loupe view. It is
    dependent on template data stored in QSettings
*/
    if (G::isLogger) G::log(__FUNCTION__);
    infoString = new InfoString(this, dm, setting/*, embelProperties*/);

}

void MW::createInfoView()
{
/*
    InfoView shows basic metadata in a dock widget.
*/
    if (!useInfoView) return;
    if (G::isLogger) G::log(__FUNCTION__);
    infoView = new InfoView(this, dm, metadata, thumbView);
    infoView->setMaximumWidth(folderMaxWidth);

    if (isSettings) {
        /* read InfoView okToShow fields */
        setting->beginGroup("InfoFields");
        QStringList setFields = setting->childKeys();
        QList<QStandardItem *> itemList;
        QStandardItemModel *k = infoView->ok;
        // go through every setting in QSettings
        bool isFound;
        for (int i = 0; i < setFields.size(); ++i) {
            isFound = false;
            // Get a field and boolean
            QString setField = setFields.at(i);
            bool okToShow = setting->value(setField).toBool();
            int row;
            // search for the matching item in infoView
            for (row = 0; row < k->rowCount(); row++) {
                isFound = false;
                QModelIndex idParent = k->index(row, 0);
                QString fieldName = qvariant_cast<QString>(idParent.data());
                // find the match
    //            qDebug() << G::t.restart() << "\t" << "Comparing parent" << fieldName << "to"<< setField;
                if (fieldName == setField) {
                    QModelIndex idParentChk = k->index(row, 2);
                    // set the flag whether to display or not
                    k->setData(idParentChk, okToShow, Qt::EditRole);
    //                qDebug() << G::t.restart() << "\t" << "Parent match so set to" << okToShow
    //                         << idParent.data().toString() << "\n";
                    isFound = true;
                    break;
                }
                for (int childRow = 0; childRow < k->rowCount(idParent); childRow++) {
                    QModelIndex idChild = k->index(childRow, 0, idParent);
                    QString fieldName = qvariant_cast<QString>(idChild.data());
                    // find the match
    //                qDebug() << G::t.restart() << "\t" << "Comparing child" << fieldName << "to"<< setField;
                    if (fieldName == setField) {
                        QModelIndex idChildChk = k->index(childRow, 2, idParent);
                        // set the flag whether to display or not
                        k->setData(idChildChk, okToShow, Qt::EditRole);
    //                    qDebug() << G::t.restart() << "\t" << "Child match so set to" << okToShow
    //                             << idChild.data().toString() << "\n";
                        isFound = true;
                        break;
                    }
                    if (isFound) break;
                }
                if (isFound) break;
            }
        }
        setting->endGroup();
    }

    /* read InfoView okToShow fields */
//    qDebug() << G::t.restart() << "\t" << "\nread InfoView okToShow fields\n";
    setting->beginGroup("InfoFields");
    QStringList setFields = setting->childKeys();
    QList<QStandardItem *> itemList;
    QStandardItemModel *k = infoView->ok;
    // go through every setting in QSettings
    bool isFound;
    for (int i = 0; i < setFields.size(); ++i) {
        isFound = false;
        // Get a field and boolean
        QString setField = setFields.at(i);
        bool okToShow = setting->value(setField).toBool();
        int row;
        // search for the matching item in infoView
        for (row = 0; row < k->rowCount(); row++) {
            isFound = false;
            QModelIndex idParent = k->index(row, 0);
            QString fieldName = qvariant_cast<QString>(idParent.data());
            // find the match
//            qDebug() << G::t.restart() << "\t" << "Comparing parent" << fieldName << "to"<< setField;
            if (fieldName == setField) {
                QModelIndex idParentChk = k->index(row, 2);
                // set the flag whether to display or not
                k->setData(idParentChk, okToShow, Qt::EditRole);
//                qDebug() << G::t.restart() << "\t" << "Parent match so set to" << okToShow
//                         << idParent.data().toString() << "\n";
                break;
            }
            for (int childRow = 0; childRow < k->rowCount(idParent); childRow++) {
                QModelIndex idChild = k->index(childRow, 0, idParent);
                QString fieldName = qvariant_cast<QString>(idChild.data());
                // find the match
//                qDebug() << G::t.restart() << "\t" << "Comparing child" << fieldName << "to"<< setField;
                if (fieldName == setField) {
                    QModelIndex idChildChk = k->index(childRow, 2, idParent);
                    // set the flag whether to display or not
                    k->setData(idChildChk, okToShow, Qt::EditRole);
//                    qDebug() << G::t.restart() << "\t" << "Child match so set to" << okToShow
//                             << idChild.data().toString() << "\n";
                    isFound = true;
                    break;
                }
                if (isFound) break;
            }
            if (isFound) break;
        }
    }
    setting->endGroup();

    connect(infoView->ok, SIGNAL(itemChanged(QStandardItem*)),
            this, SLOT(metadataChanged(QStandardItem*)));

}

void MW::createFolderDock()
{
    if (G::isLogger) G::log(__FUNCTION__);
    folderDockTabText = "  📁  ";
    folderDock = new DockWidget(folderDockTabText, this);  // Folders 📁
    folderDock->setObjectName("FoldersDock");
    folderDock->setWidget(fsTree);
    // customize the folderDock titlebar
    QHBoxLayout *folderTitleLayout = new QHBoxLayout();
    folderTitleLayout->setContentsMargins(0, 0, 0, 0);
    folderTitleLayout->setSpacing(0);
    folderTitleBar = new DockTitleBar("Folders", folderTitleLayout);
    folderDock->setTitleBarWidget(folderTitleBar);

    // add widgets to the right side of the title bar layout
    // toggle expansion button
    BarBtn *folderRefreshBtn = new BarBtn();
    folderRefreshBtn->setIcon(QIcon(":/images/icon16/refresh.png"));
    folderRefreshBtn->setToolTip("Refresh and collapse");
    connect(folderRefreshBtn, &BarBtn::clicked, this, &MW::refreshFolders);
    folderTitleLayout->addWidget(folderRefreshBtn);

    // Spacer
    folderTitleLayout->addSpacing(5);

    // preferences button
    BarBtn *folderGearBtn = new BarBtn();
    folderGearBtn->setIcon(QIcon(":/images/icon16/gear.png"));
    folderGearBtn->setToolTip("Preferences");
    connect(folderGearBtn, &BarBtn::clicked, this, &MW::allPreferences);
    folderTitleLayout->addWidget(folderGearBtn);

    // Spacer
    folderTitleLayout->addSpacing(10);

    // close button
    BarBtn *folderCloseBtn = new BarBtn();
    folderCloseBtn->setIcon(QIcon(":/images/icon16/close.png"));
    folderCloseBtn->setToolTip("Hide the Folders Panel");
    connect(folderCloseBtn, &BarBtn::clicked, this, &MW::toggleFolderDockVisibility);
    folderTitleLayout->addWidget(folderCloseBtn);

    // Spacer
    folderTitleLayout->addSpacing(5);

    if (isSettings) {
        setting->beginGroup(("FolderDock"));
        if (setting->contains("screen")) folderDock->dw.screen = setting->value("screen").toInt();
        if (setting->contains("pos")) folderDock->dw.pos = setting->value("pos").toPoint();
        if (setting->contains("size")) folderDock->dw.size = setting->value("size").toSize();
        if (setting->contains("devicePixelRatio")) folderDock->dw.devicePixelRatio = setting->value("devicePixelRatio").toReal();
        setting->endGroup();
    }

    connect(folderDock, &QDockWidget::visibilityChanged, this, &MW::folderDockVisibilityChange);
}

void MW::createFavDock()
{
    if (G::isLogger) G::log(__FUNCTION__);
    favDockTabText = "  🔖  ";
    favDock = new DockWidget(favDockTabText, this);  // Bookmarks📗 🔖 🏷️ 🗂️
    favDock->setObjectName("Bookmarks");
    favDock->setWidget(bookmarks);

    // customize the favDock titlebar
    QHBoxLayout *favTitleLayout = new QHBoxLayout();
    favTitleLayout->setContentsMargins(0, 0, 0, 0);
    favTitleLayout->setSpacing(0);
    favTitleBar = new DockTitleBar("Bookmarks", favTitleLayout);
    favDock->setTitleBarWidget(favTitleBar);

    // add widgets to the right side of the title bar layout
    // refresh button
    BarBtn *favRefreshBtn = new BarBtn();
    favRefreshBtn->setIcon(QIcon(":/images/icon16/refresh.png"));
    favRefreshBtn->setToolTip("Refresh");
    connect(favRefreshBtn, &BarBtn::clicked, this, &MW::refreshFolders);
    favTitleLayout->addWidget(favRefreshBtn);

    // Spacer
    favTitleLayout->addSpacing(5);

    // preferences button
    BarBtn *favGearBtn = new BarBtn();
    favGearBtn->setIcon(QIcon(":/images/icon16/gear.png"));
    favGearBtn->setToolTip("Preferences");
    connect(favGearBtn, &BarBtn::clicked, this, &MW::allPreferences);
    favTitleLayout->addWidget(favGearBtn);

    // Spacer
    favTitleLayout->addSpacing(10);

    // close button
    BarBtn *favCloseBtn = new BarBtn();
    favCloseBtn->setIcon(QIcon(":/images/icon16/close.png"));
    favCloseBtn->setToolTip("Hide the Bookmarks Panel");
    connect(favCloseBtn, &BarBtn::clicked, this, &MW::toggleFavDockVisibility);
    favTitleLayout->addWidget(favCloseBtn);

    // Spacer
    favTitleLayout->addSpacing(5);

    if (isSettings) {
        setting->beginGroup(("FavDock"));
        if (setting->contains("screen")) favDock->dw.screen = setting->value("screen").toInt();
        if (setting->contains("pos")) favDock->dw.pos = setting->value("pos").toPoint();
        if (setting->contains("size")) favDock->dw.size = setting->value("size").toSize();
        if (setting->contains("devicePixelRatio")) favDock->dw.devicePixelRatio = setting->value("devicePixelRatio").toReal();
        setting->endGroup();
    }
}

void MW::createFilterDock()
{
    if (G::isLogger) G::log(__FUNCTION__);
    filterDockTabText = "  🤏  ";
    filterDock = new DockWidget(filterDockTabText, this);  // Filters 🤏♆🔻 🕎  <font color=\"red\"><b>♆</b></font> does not work
    filterDock->setObjectName("Filters");

    // customize the filterDock titlebar
    QHBoxLayout *filterTitleLayout = new QHBoxLayout();
    filterTitleLayout->setContentsMargins(0, 0, 0, 0);
    filterTitleLayout->setSpacing(0);
    filterTitleBar = new DockTitleBar("Filters", filterTitleLayout);
    filterDock->setTitleBarWidget(filterTitleBar);

    // add widgets to the right side of the title bar layout
    // toggle expansion button
    BarBtn *toggleExpansionBtn = new BarBtn();
    toggleExpansionBtn->setIcon(QIcon(":/images/icon16/foldertree.png"));
    toggleExpansionBtn->setToolTip("Toggle expand all / collapse all");
    connect(toggleExpansionBtn, &BarBtn::clicked, filters, &Filters::toggleExpansion);
    filterTitleLayout->addWidget(toggleExpansionBtn);

    // Spacer
    filterTitleLayout->addSpacing(5);

    // preferences button
    BarBtn *filterGearBtn = new BarBtn();
    filterGearBtn->setIcon(QIcon(":/images/icon16/gear.png"));
    filterGearBtn->setToolTip("Preferences");
    connect(filterGearBtn, &BarBtn::clicked, this, &MW::allPreferences);
    filterTitleLayout->addWidget(filterGearBtn);

    // Spacer
    filterTitleLayout->addSpacing(10);

    // close button
    BarBtn *filterCloseBtn = new BarBtn();
    filterCloseBtn->setIcon(QIcon(":/images/icon16/close.png"));
    filterCloseBtn->setToolTip("Hide the Filters Panel");
    connect(filterCloseBtn, &BarBtn::clicked, this, &MW::toggleFilterDockVisibility);
    filterTitleLayout->addWidget(filterCloseBtn);

    // Spacer
    filterTitleLayout->addSpacing(5);

    // Inside dock set layout for a text label, a progress bar and the filters tree
    QVBoxLayout *msgLayout = new QVBoxLayout();
    msgLayout->setContentsMargins(0, 0, 0, 0);
    msgLayout->setSpacing(0);
    msgLayout->addWidget(filters->filterLabel);
    msgLayout->addWidget(filters->bfProgressBar);
    filters->msgFrame = new QFrame;
    filters->msgFrame->setLayout(msgLayout);
    filters->msgFrame->setVisible(false);

    QVBoxLayout *filterLayout = new QVBoxLayout();
    filterLayout->setContentsMargins(0, 0, 0, 0);
    filterLayout->addWidget(filters->msgFrame);
    filterLayout->addWidget(filters);
    QFrame *frame = new QFrame;
    frame->setLayout(filterLayout);
    filterDock->setWidget(frame);

    connect(filterDock, &QDockWidget::visibilityChanged, this, &MW::filterDockVisibilityChange);

    if (isSettings) {
        setting->beginGroup(("FilterDock"));
        if (setting->contains("screen")) filterDock->dw.screen = setting->value("screen").toInt();
        if (setting->contains("pos")) filterDock->dw.pos = setting->value("pos").toPoint();
        if (setting->contains("size")) filterDock->dw.size = setting->value("size").toSize();
        if (setting->contains("devicePixelRatio")) filterDock->dw.devicePixelRatio = setting->value("devicePixelRatio").toReal();
        setting->endGroup();
    }
}

void MW::createMetadataDock()
{
    if (!useInfoView) return;
    if (G::isLogger) G::log(__FUNCTION__);
    metadataDockTabText = "  📷  ";
    metadataDock = new DockWidget(metadataDockTabText, this);    // Metadata
    metadataDock->setObjectName("Image Info");
    metadataDock->setWidget(infoView);

    // customize the metadataDock titlebar
    QHBoxLayout *metaTitleLayout = new QHBoxLayout();
    metaTitleLayout->setContentsMargins(0, 0, 0, 0);
    metaTitleLayout->setSpacing(0);
    metaTitleBar = new DockTitleBar("Metadata", metaTitleLayout);
    metadataDock->setTitleBarWidget(metaTitleBar);

    // add widgets to the right side of the title bar layout
    // preferences button
    BarBtn *metaGearBtn = new BarBtn();
    metaGearBtn->setIcon(QIcon(":/images/icon16/gear.png"));
    metaGearBtn->setToolTip("Edit preferences including which items to show in this panel");
    connect(metaGearBtn, &BarBtn::clicked, this, &MW::infoViewPreferences);
    metaTitleLayout->addWidget(metaGearBtn);

    // Spacer
    metaTitleLayout->addSpacing(10);

    // close button
    BarBtn *metaCloseBtn = new BarBtn();
    metaCloseBtn->setIcon(QIcon(":/images/icon16/close.png"));
    metaCloseBtn->setToolTip("Hide the Metadata Panel");
    connect(metaCloseBtn, &BarBtn::clicked, this, &MW::toggleMetadataDockVisibility);
    metaTitleLayout->addWidget(metaCloseBtn);

    // Spacer
    metaTitleLayout->addSpacing(5);

    if (isSettings) {
        setting->beginGroup(("MetadataDock"));
        if (setting->contains("screen")) metadataDock->dw.screen = setting->value("screen").toInt();
        if (setting->contains("pos")) metadataDock->dw.pos = setting->value("pos").toPoint();
        if (setting->contains("size")) metadataDock->dw.size = setting->value("size").toSize();
        if (setting->contains("devicePixelRatio")) metadataDock->dw.devicePixelRatio = setting->value("devicePixelRatio").toReal();
        setting->endGroup();
    }
}

void MW::createThumbDock()
{
    if (G::isLogger) G::log(__FUNCTION__);
    thumbDockTabText = "Thumbnails";
    thumbDock = new DockWidget(thumbDockTabText, this);  // Thumbnails
    thumbDock->setObjectName("thumbDock");
    thumbDock->setWidget(thumbView);
    thumbDock->installEventFilter(this);

    if (isSettings) {
        setting->beginGroup(("ThumbDockFloat"));
        if (setting->contains("screen")) thumbDock->dw.screen = setting->value("screen").toInt();
        if (setting->contains("pos")) thumbDock->dw.pos = setting->value("pos").toPoint();
        if (setting->contains("size")) thumbDock->dw.size = setting->value("size").toSize();
        if (setting->contains("devicePixelRatio")) thumbDock->dw.devicePixelRatio = setting->value("devicePixelRatio").toReal();
        setting->endGroup();
    }
//    else thumbDock->dw.size = QSize(600, 600);

    connect(thumbDock, SIGNAL(dockLocationChanged(Qt::DockWidgetArea)),
            this, SLOT(setThumbDockFeatures(Qt::DockWidgetArea)));

    connect(thumbDock, SIGNAL(topLevelChanged(bool)),
            this, SLOT(setThumbDockFloatFeatures(bool)));
}

void MW::createEmbelDock()
{
    if (G::isLogger) G::log(__FUNCTION__);
    embelProperties = new EmbelProperties(this, setting);

    connect (embelProperties, &EmbelProperties::templateChanged, this, &MW::embelTemplateChange);
    connect (embelProperties, &EmbelProperties::syncEmbellishMenu, this, &MW::syncEmbellishMenu);

    embelDockTabText = "  🎨  ";
    embelDock = new DockWidget(embelDockTabText, this);  // Embellish
    embelDock->setObjectName("embelDock");
    embelDock->setWidget(embelProperties);
    embelDock->setFloating(false);
    embelDock->setVisible(true);
    // prevent MW splitter resizing embelDock so cannot see header - and + buttons in embellish
    embelDock->setMinimumWidth(275);

    connect(embelDock, &QDockWidget::visibilityChanged, this, &MW::embelDockVisibilityChange);

    // customize the embelDock titlebar
    QHBoxLayout *embelTitleLayout = new QHBoxLayout();
    embelTitleLayout->setContentsMargins(0, 0, 0, 0);
    embelTitleLayout->setSpacing(0);

    // add spaces to create a minimum width for the embel dock so control buttons in
    // embellish tree are always visible
    embelTitleBar = new DockTitleBar("Embellish", embelTitleLayout);
//    embelTitleBar = new DockTitleBar("Embellish                             ", embelTitleLayout);
    embelDock->setTitleBarWidget(embelTitleBar);

    // add widgets to the right side of the title bar layout

    // run template button
    embelRunBtn = new BarBtn();
    embelRunBtn->setIcon(QIcon(":/images/icon16/lightning1.png"));
    embelRunBtn->setToolTip("Export the selected images.");
    connect(embelRunBtn, &BarBtn::clicked, this, &MW::exportEmbel);
    embelTitleLayout->addWidget(embelRunBtn);

    // Spacer
    embelTitleLayout->addSpacing(10);

    // tile button
    BarBtn *embelTileBtn = new BarBtn();
    embelTileBtn->setIcon(QIcon(":/images/icon16/tile.png"));
    embelTileBtn->setToolTip("Manage tile extraction, deletion and renaming");
    connect(embelTileBtn, &BarBtn::clicked, embelProperties, &EmbelProperties::manageTiles);
    embelTitleLayout->addWidget(embelTileBtn);

    // Spacer
    embelTitleLayout->addSpacing(10);

    /*
    // anchor button
    BarBtn *embelAnchorBtn = new BarBtn();
    embelAnchorBtn->setIcon(QIcon(":/images/icon16/anchor.png"));
    embelAnchorBtn->setToolTip("Add anchors to pin text, rectangles and graphics");
    //    connect(embelAnchorBtn, &DockTitleBtn::clicked, imageView, &ImageView::activateRubberBand);
    embelTitleLayout->addWidget(embelAnchorBtn);

    // text button
    BarBtn *embelTextBtn = new BarBtn();
    embelTextBtn->setIcon(QIcon(":/images/icon16/text.png"));
    embelTextBtn->setToolTip("Add a Text Item");
    //    connect(embelTextBtn, &DockTitleBtn::clicked, this, &MW::infoViewPreferences);
    embelTitleLayout->addWidget(embelTextBtn);

    // rectangle button
    BarBtn *embelRectangleBtn = new BarBtn();
    embelRectangleBtn->setIcon(QIcon(":/images/icon16/rectangle.png"));
    embelRectangleBtn->setToolTip("Add a rectangle");
//    connect(embelRectangleBtn, &DockTitleBtn::clicked, imageView, &ImageView::activateRubberBand);
    embelTitleLayout->addWidget(embelRectangleBtn);

    // graphic button
    BarBtn *embelGraphicBtn = new BarBtn();
    embelGraphicBtn->setIcon(QIcon(":/images/icon16/graphic.png"));
    embelGraphicBtn->setToolTip("Add a graphic like a logo or signature PNG");
    //    connect(embelGraphicBtn, &DockTitleBtn::clicked, imageView, &ImageView::activateRubberBand);
    embelTitleLayout->addWidget(embelGraphicBtn);

    // Spacer
    embelTitleLayout->addSpacing(10);

    */

    // question mark button
    BarBtn *embelQuestionBtn = new BarBtn();
    embelQuestionBtn->setIcon(":/images/icon16/questionmark.png", G::iconOpacity);
    embelQuestionBtn->setToolTip("Embel coordinate and container system.");
    connect(embelQuestionBtn, &BarBtn::clicked, embelProperties, &EmbelProperties::coordHelp);
    embelTitleLayout->addWidget(embelQuestionBtn);

    // Spacer
    embelTitleLayout->addSpacing(5);

    // close button
    BarBtn *embelCloseBtn = new BarBtn();
    embelCloseBtn->setIcon(":/images/icon16/close.png", G::iconOpacity);
    embelCloseBtn->setToolTip("Hide the Embellish Panel");
    connect(embelCloseBtn, &BarBtn::clicked, this, &MW::toggleEmbelDockVisibility);
    embelTitleLayout->addWidget(embelCloseBtn);

    // Spacer
    embelTitleLayout->addSpacing(5);
}

void MW::createDocks()
{
    if (G::isLogger) G::log(__FUNCTION__);
    createFolderDock();
    createFavDock();
    createFilterDock();
    if (useInfoView) createMetadataDock();
    createThumbDock();
    createEmbelDock();

    connect(this, &MW::tabifiedDockWidgetActivated, this, &MW::embelDockActivated);

    addDockWidget(Qt::LeftDockWidgetArea, folderDock);
    addDockWidget(Qt::LeftDockWidgetArea, favDock);
    addDockWidget(Qt::LeftDockWidgetArea, filterDock);
    if (useInfoView) addDockWidget(Qt::LeftDockWidgetArea, metadataDock);
    addDockWidget(Qt::LeftDockWidgetArea, thumbDock);
    if (!hideEmbellish) addDockWidget(Qt::RightDockWidgetArea, embelDock);

    MW::setTabPosition(Qt::LeftDockWidgetArea, QTabWidget::North);
    MW::tabifyDockWidget(folderDock, favDock);
    MW::tabifyDockWidget(favDock, filterDock);
    if (useInfoView) MW::tabifyDockWidget(filterDock, metadataDock);
    if (!hideEmbellish) if (useInfoView) MW::tabifyDockWidget(metadataDock, embelDock);
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
    if (G::isLogger) G::log(__FUNCTION__);
    if (folderDock->isVisible()) {
        fsTree->scrollToCurrent();
    }
}

void MW::embelDockVisibilityChange()
{
    if (G::isLogger) G::log(__FUNCTION__);
    // rgh ??
    //    if (embelDock->isVisible()) newEmbelTemplate();
}

void MW::embelDockActivated(QDockWidget *dockWidget)
{
    if (G::isLogger) G::log(__FUNCTION__);
//    if (dockWidget->objectName() == "embelDock") embelDisplay();
    // enable the folder dock (first one in tab)
    embelDockTabActivated = true;
    QList<QTabBar*> tabList = findChildren<QTabBar*>();
    QTabBar* widgetTabBar = tabList.at(0);
    widgetTabBar->setCurrentIndex(4);
//    qDebug() << __FUNCTION__ << dockWidget->objectName() << widgetTabBar->currentIndex();

}

void MW::embelTemplateChange(int id)
{
    if (G::isLogger) G::log(__FUNCTION__);
    embelTemplatesActions.at(id)->setChecked(true);
    if (id == 0) {
        embelRunBtn->setVisible(false);
        setRatingBadgeVisibility();
        setShootingInfoVisibility();
    }
    else {
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
    if (G::isLogger) G::log(__FUNCTION__);
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

void MW::createEmbel()
{
    if (G::isLogger) G::log(__FUNCTION__);
    QString src = "Internal";
    embel = new Embel(imageView->scene, imageView->pmItem, embelProperties, dm, icd);
    connect(imageView, &ImageView::embellish, embel, &Embel::build);
    connect(embel, &Embel::done, imageView, &ImageView::resetFitZoom);
    if (useInfoView) {
        connect(infoView, &InfoView::dataEdited, embel, &Embel::refreshTexts);
    }
}

void MW::createFSTree()
{
    if (G::isLogger) G::log(__FUNCTION__);
    // loadSettings has not run yet (dependencies, but QSettings has been opened
    fsTree = new FSTree(this, metadata);
    fsTree->setMaximumWidth(folderMaxWidth);
    fsTree->setShowImageCount(true);
    fsTree->combineRawJpg = combineRawJpg;

    // watch for drive removal (not working)
//    connect(fsTree->watch, &QFileSystemWatcher::directoryChanged, this, &MW::checkDirState);

    // counting eligible image files in folders
    connect(fsTree, &FSTree::updateFileCount, this, &MW::setCentralMessage);

    // selection change check if triggered by ejecting USB drive
    connect(fsTree, &FSTree::selectionChange, this, &MW::watchCurrentFolder);

    // this works for touchpad tap
//    connect(fsTree, SIGNAL(pressed(const QModelIndex&)), this, SLOT(folderSelectionChange()));
    connect(fsTree, &FSTree::pressed, this, &MW::folderSelectionChange);

    // this does not work for touchpad tap
//    connect(fsTree, SIGNAL(clicked(const QModelIndex&)), this, SLOT(folderSelectionChange()));

    // this does not work to detect ejecting a drive
//    connect(fsTree->fsModel, SIGNAL(rowsRemoved(const QModelIndex &, int, int)),
//            this, SLOT(checkDirState(const QModelIndex &, int, int)));

//    connect(fsTree, SIGNAL(dropOp(Qt::KeyboardModifiers, bool, QString)),
//            this, SLOT(dropOp(Qt::KeyboardModifiers, bool, QString)));
}

void MW::createFilterView()
{
    if (G::isLogger) G::log(__FUNCTION__);
    filters = new Filters(this);
    filters->setMaximumWidth(folderMaxWidth);

    /* Not using SIGNAL(itemChanged(QTreeWidgetItem*,int) because it triggers
       for every item in Filters */
    connect(filters, &Filters::filterChange, this, &MW::filterChange);
}

void MW::createBookmarks()
{
    if (G::isLogger) G::log(__FUNCTION__);
    bookmarks = new BookMarks(this, metadata, true /*showImageCount*/, combineRawJpg);

    if (isSettings) {
        setting->beginGroup("Bookmarks");
        QStringList paths = setting->childKeys();
        for (int i = 0; i < paths.size(); ++i) {
            bookmarks->bookmarkPaths.insert(setting->value(paths.at(i)).toString());
        }
        bookmarks->reloadBookmarks();
        setting->endGroup();
    }
    else {
        bookmarks->bookmarkPaths.insert(QDir::homePath());
    }

    bookmarks->setMaximumWidth(folderMaxWidth);

    // this does work for touchpad tap
    connect(bookmarks, SIGNAL(itemPressed(QTreeWidgetItem *, int)),
            this, SLOT(bookmarkClicked(QTreeWidgetItem *, int)));

    connect(bookmarks, SIGNAL(dropOp(Qt::KeyboardModifiers, bool, QString)),
            this, SLOT(dropOp(Qt::KeyboardModifiers, bool, QString)));
}

void MW::createAppStyle()
{
    if (G::isLogger) G::log(__FUNCTION__);
    widgetCSS.fontSize = G::fontSize.toInt();
    int bg = G::backgroundShade;
    widgetCSS.widgetBackgroundColor = QColor(bg,bg,bg);
    css = widgetCSS.css();
    G::css = css;
    this->setStyleSheet(css);
    return;
}

void MW::createStatusBar()
{
    if (G::isLogger) G::log(__FUNCTION__);
    statusBar()->setObjectName("WinnowStatusBar");
    statusBar()->setStyleSheet("QStatusBar::item { border: none; };");

    // cache status on right side of status bar

    // label to hold QPixmap showing progress
    progressLabel = new QLabel();

    // progressBar created in MW::createDataModel, where it is first req'd

    // set up pixmap that shows progress in the cache
    if (isSettings && setting->contains("cacheStatusWidth"))
        progressWidth = setting->value("cacheStatusWidth").toInt();
    if (progressWidth < 100 || progressWidth > 1000) progressWidth = 200;
    progressPixmap = new QPixmap(4000, 25);   // cacheprogress
    progressPixmap->scaled(progressWidth, 25);
    progressPixmap->fill(widgetCSS.widgetBackgroundColor);
    progressLabel->setFixedWidth(progressWidth);
    progressLabel->setPixmap(*progressPixmap);

    // tooltip
    QString progressToolTip = "Image cache status for current folder:\n";
    progressToolTip += "  • LightGray:  \tbackground for all images in folder\n";
    progressToolTip += "  • DarkGray:   \timages targeted to be cached\n";
    progressToolTip += "  • Green:      \timages that are cached\n";
    progressToolTip += "  • LightGreen: \tcurrent image";
    progressLabel->setToolTip(progressToolTip);
    progressLabel->setToolTipDuration(100000);
    statusBar()->addPermanentWidget(progressLabel, 1);

    // end progressbar

    statusBarSpacer = new QLabel;
    statusBarSpacer->setText(" ");
    statusBar()->addPermanentWidget(statusBarSpacer);

    // label to show metadataThreadRunning status
    int runLabelWidth = 13;
    metadataThreadRunningLabel = new QLabel;
    QString mtrl = "Turns red when metadata/icon caching in progress";
    metadataThreadRunningLabel->setToolTip(mtrl);
    metadataThreadRunningLabel->setFixedWidth(runLabelWidth);
    updateMetadataThreadRunStatus(false, true, __FUNCTION__);
    statusBar()->addPermanentWidget(metadataThreadRunningLabel);

    // label to show imageThreadRunning status
    imageThreadRunningLabel = new QLabel;
    statusBar()->addPermanentWidget(imageThreadRunningLabel);
    QString itrl = "Turns red when image caching in progress";
    imageThreadRunningLabel->setToolTip(itrl);
    imageThreadRunningLabel->setFixedWidth(runLabelWidth);

    // label to show cache amount
//    cacheMethodBtn->setIcon(QIcon(":/images/icon16/thrifty.png"));
    // setImageCacheSize sets icon
    setImageCacheSize(cacheSizeMethod);
    statusBar()->addPermanentWidget(cacheMethodBtn);
    cacheMethodBtn->show();

    // add process progress bar to left side of statusBar
    progressBar = new QProgressBar;
    progressBar->setStyleSheet(
                "QProgressBar {"
                    "border: 1px solid gray;"
                "}"
                );
    progressBar->setFixedSize(50, 8);
    progressBar->setTextVisible(false);
    progressBar->setVisible(false);
    statusBar()->addWidget(progressBar);

    // add status icons to left side of statusBar
    statusBar()->addWidget(colorManageToggleBtn);
    statusBar()->addWidget(reverseSortBtn);
    filterStatusLabel->setPixmap(QPixmap(":/images/icon16/filter.png"));
    filterStatusLabel->setAlignment(Qt::AlignVCenter);
    statusBar()->addWidget(filterStatusLabel);
    subfolderStatusLabel->setPixmap(QPixmap(":/images/icon16/subfolders.png"));
    statusBar()->addWidget(subfolderStatusLabel);
    rawJpgStatusLabel->setPixmap(QPixmap(":/images/icon16/link.png"));
    statusBar()->addWidget(rawJpgStatusLabel);
    slideShowStatusLabel->setPixmap(QPixmap(":/images/icon16/slideshow.png"));
    statusBar()->addWidget(slideShowStatusLabel);

    setThreadRunStatusInactive();

    // general status on left side of status bar
    statusLabel = new QLabel;
    statusBar()->addWidget(statusLabel);
}

void MW::updateStatusBar()
{
    if (G::isLogger) G::log(__FUNCTION__);

    if (G::colorManage) colorManageToggleBtn->setIcon(QIcon(":/images/icon16/rainbow1.png"));
    else colorManageToggleBtn->setIcon(QIcon(":/images/icon16/norainbow1.png"));

    if (sortReverseAction->isChecked()) reverseSortBtn->setIcon(QIcon(":/images/icon16/Z-A.png"));
    else reverseSortBtn->setIcon(QIcon(":/images/icon16/A-Z.png"));

    filterStatusLabel->setVisible(filters->isAnyFilter());
    subfolderStatusLabel->setVisible(subFoldersAction->isChecked());
    rawJpgStatusLabel->setVisible(combineRawJpgAction->isChecked());
    slideShowStatusLabel->setVisible(G::isSlideShow);

//    updateProgressBarWidth();

//    if (!(G::isSlideShow && isSlideShowRandom)) progressLabel->setVisible(isShowCacheProgressBar);// rghcachechange
//    else progressLabel->setVisible(false);
}

int MW::availableSpaceForProgressBar()
{
    if (G::isLogger) G::log(__FUNCTION__);
    int w = 0;
    int s = statusBar()->layout()->spacing();
    if (colorManageToggleBtn->isVisible()) w += s + colorManageToggleBtn->width();
    if (reverseSortBtn->isVisible()) w += s + reverseSortBtn->width();
    if (rawJpgStatusLabel->isVisible()) w += s + rawJpgStatusLabel->width();
    if (filterStatusLabel->isVisible()) w += s + filterStatusLabel->width();
    if (subfolderStatusLabel->isVisible()) w += s + subfolderStatusLabel->width();
    if (slideShowStatusLabel->isVisible()) w += s + slideShowStatusLabel->width();
    if (statusLabel->isVisible()) w += s + statusLabel->width();
    if (metadataThreadRunningLabel->isVisible()) w += s + metadataThreadRunningLabel->width();
    if (imageThreadRunningLabel->isVisible()) w += s + imageThreadRunningLabel->width();
    if (statusBarSpacer->isVisible()) w += s + statusBarSpacer->width();
    w += s;
    return statusBar()->width() - w - 30;
}

void MW::updateProgressBarWidth()
{
    if (G::isLogger) G::log(__FUNCTION__);
    if (dm->rowCount() && progressLabel->isVisible()) {
        int availableSpace = availableSpaceForProgressBar();
        if (availableSpace < progressWidth) progressWidth = availableSpace;
        progressLabel->setFixedWidth(progressWidth);
        updateImageCacheStatus("Update all rows", icd->cache, __FUNCTION__);
    }
}

void MW::toggleImageCacheMethod()
{
/*
    Called by cacheSizeBtn press
*/
    if (G::isLogger) G::log(__FUNCTION__);

    if (qApp->keyboardModifiers() == Qt::ControlModifier) {
        cachePreferences();
        return;
    }

    if (cacheSizeMethod == "Thrifty") setImageCacheSize("Moderate");
    else if (cacheSizeMethod == "Moderate") setImageCacheSize("Greedy");
    else if (cacheSizeMethod == "Greedy") setImageCacheSize("Thrifty");
    setImageCacheParameters();
}

void MW::thriftyCache()
{
/*
    Connected to F10
*/
    if (G::isLogger) G::log(__FUNCTION__);
    setImageCacheSize("Thrifty");
    setImageCacheParameters();
}

void MW::moderateCache()
{
/*
    Connected to F11
*/
    if (G::isLogger) G::log(__FUNCTION__);
    setImageCacheSize("Moderate");
    setImageCacheParameters();
}

void MW::greedyCache()
{
/*
    Connected to F12
*/
    if (G::isLogger) G::log(__FUNCTION__);
    setImageCacheSize("Greedy");
    setImageCacheParameters();
}

void MW::setImageCacheMinSize(QString size)
{
/*

*/
    if (G::isLogger) G::log(__FUNCTION__);
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
    if (G::isLogger) G::log(__FUNCTION__);
//    qDebug() << __FUNCTION__ << method;


    // deal with possible deprecated settings
    if (method != "Thrifty" && method != "Moderate" && method != "Greedy")
        method = "Thrifty";

    cacheSizeMethod = method;

    if (cacheSizeMethod == "Thrifty") {
        cacheMaxMB = static_cast<int>(G::availableMemoryMB * 0.10);
        cacheMethodBtn->setIcon(QIcon(":/images/icon16/thrifty.png"));
    }
    if (cacheSizeMethod == "Moderate") {
        cacheMaxMB = static_cast<int>(G::availableMemoryMB * 0.50);
        cacheMethodBtn->setIcon(QIcon(":/images/icon16/moderate.png"));
    }
    if (cacheSizeMethod == "Greedy") {
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
    if (G::isLogger) G::log(__FUNCTION__);

    int cacheNetMB = cacheMaxMB - static_cast<int>(G::metaCacheMB);
    if (cacheNetMB < cacheMinMB) cacheNetMB = cacheMinMB;

    imageCacheThread->updateImageCacheParam(cacheNetMB, cacheMinMB,
             isShowCacheProgressBar, cacheWtAhead);

    QString fPath = thumbView->currentIndex().data(G::PathRole).toString();
    // set position in image cache
    if (fPath.length())
        imageCacheThread->setCurrentPosition(fPath);

    // cache progress bar
    progressLabel->setVisible(isShowCacheProgressBar);

    // thumbnail cache status indicators
    thumbView->refreshThumbs();
    gridView->refreshThumbs();

    if (G::isNewFolderLoaded) imageCacheThread->cacheSizeChange();
}

QString MW::getPosition()
{
/*
    This function is used by MW::updateStatus to report the current image relative to all the
    images in the folder ie 17 of 80.

    It is displayed on the status bar and in the infoView.
*/
    if (G::isLogger) G::log(__FUNCTION__);
    QString fileCount = "";
    QModelIndex idx = thumbView->currentIndex();
    if (!idx.isValid()) return "";
    long rowCount = dm->sf->rowCount();
    if (rowCount <= 0) return "";
    int row = idx.row() + 1;
    fileCount = QString::number(row) + " of "
        + QString::number(rowCount);
    if (subFoldersAction->isChecked()) fileCount += " including subfolders";
    return fileCount;
}

QString MW::getZoom()
{
/*

*/
    if (G::isLogger) G::log(__FUNCTION__);
    if (G::mode != "Loupe" &&
        G::mode != "Compare") return "N/A";
    qreal zoom;
    if (G::mode == "Compare") zoom = compareImages->zoomValue;
    else zoom = imageView->zoom;
    if (zoom <= 0 || zoom > 10) return "";
//    qDebug() << __FUNCTION__ << zoom;
    return QString::number(qRound(zoom*100)) + "%"; // + "% zoom";
}

QString MW::getPicked()
{
/*
    Returns a string like "16 (38MB)"
*/
    if (G::isLogger) G::log(__FUNCTION__);
    int count = 0;
    for (int row = 0; row < dm->sf->rowCount(); row++)
        if (dm->sf->index(row, G::PickColumn).data() == "true") count++;
    QString image = count == 1 ? " image, " : " images, ";

    if (count == 0) return "Nothing";
    return QString::number(count) + " ("  + pickMemSize + ")";
}

QString MW::getSelectedFileSize()
{
/*
Returns a string like "12 (165 MB)"
*/
    if (G::isLogger) G::log(__FUNCTION__);
    QString selected = QString::number(selectionModel->selectedRows().count());
    QString selMemSize = Utilities::formatMemory(memoryReqdForSelection());
    return selected + " (" + selMemSize + ")";
}

void MW::updateStatus(bool keepBase, QString s, QString source)
{
/*
    Reports status information on the status bar and in InfoView.  If keepBase = true
    then ie "1 of 80   60% zoom   2.1 MB picked" is prepended to the status message s.
*/
    if (!useUpdateStatus) return;
    if (G::isLogger) G::log(__FUNCTION__);
    /*
    QString ms = QString("%L1").arg(testTime.nsecsElapsed() / 1000000) + " ms";
    testTime.restart();
    */

//    qDebug() << __FUNCTION__ << s << source;
    // check if null filter
    if (dm->sf->rowCount() == 0) {
        statusLabel->setText("");
        if (useInfoView)  {
        QStandardItemModel *k = infoView->ok;
            k->setData(k->index(infoView->PositionRow, 1, infoView->statusInfoIdx), "");
            k->setData(k->index(infoView->ZoomRow, 1, infoView->statusInfoIdx), "");
            k->setData(k->index(infoView->SelectedRow, 1, infoView->statusInfoIdx), "");
            k->setData(k->index(infoView->PickedRow, 1, infoView->statusInfoIdx), "");
        }
        updateStatusBar();
        return;
    }

    // check for file error first
    QString fPath = thumbView->getCurrentFilePath();
    int row = dm->fPathRow[fPath];
    bool imageUnavailable = dm->index(row, G::OffsetFullColumn).data() == 0;
    bool thumbUnavailable = dm->index(row, G::OffsetThumbColumn).data() == 0;
    if (imageUnavailable || thumbUnavailable) {
        // rgh may want to set the error here as well as report
//        QString err = dm->index(row, G::ErrColumn).data().toString();
//        statusLabel->setText(err);
//        qWarning("Image or thumb unavailable");
//        return;
    }

    QString status;
    QString fileCount = "";
    QString zoomPct = "";
    QString base = "";
    QString spacer = "   ";

    /* Possible status info

QString fName = idx.data(Qt::EditRole).toString();
QString fPath = idx.data(G::PathRole).toString();
QString shootingInfo = metadata->getShootingInfo(fPath);
QString err = metadata->getErr(fPath);
QString magnify = "🔎🔍";
QString fileSym = "📁📂📗🕎📷🎨👍";
QString camera = "📈📌🔈📎🔗🔑🧾🛈";
//    https://www.vertex42.com/ExcelTips/unicode-symbols.html
QString sym = "⚡🌈🌆🌸🍁🍄🎁🎹💥💭🏃🏸💻🔆🔴🔵🔶🔷🔸🔹🔺🔻🖐🧲🛑⛬";
//        */

    // update G::availableMemory
#ifdef Q_OS_WIN
    Win::availableMemory();
    double availMemGB = static_cast<double>(G::availableMemoryMB) / 1024;
    QString mem = QString::number(availMemGB, 'f', 1) + " GB";
#endif

    // show embellish if active
    if (G::isEmbellish) {
        base = "<font color=\"red\">" + embelProperties->templateName + "</font>"
                + "&nbsp;" + "&nbsp;" + "&nbsp;";
    }
    else base = "";

    // image of total: fileCount
    if (keepBase && isCurrentFolderOkay) {
//        base += "Mem: " + mem + spacer;
        if (G::mode == "Loupe" || G::mode == "Compare" || G::mode == "Embel")
            base += "Zoom: " + getZoom();
        base += spacer + "Pos: " + getPosition();
        if (source != "nextSlide") {
        QString s = QString::number(thumbView->getSelectedCount());
            base += spacer +"Selected: " + s;
            base += spacer + "Picked: " + getPicked();
        }
    }

    status = " " + base + s;
    statusLabel->setText(status);
    /*
    status = " " + base + s + spacer + ms;
    qDebug() << "Status:" << status;
    */

    // update InfoView - flag updates so itemChanged is ignored in MW::metadataChanged
    if (useInfoView)  {
        infoView->isNewImageDataChange = true;
        QStandardItemModel *k = infoView->ok;
        if (keepBase) {
            k->setData(k->index(infoView->PositionRow, 1, infoView->statusInfoIdx), getPosition());
            k->setData(k->index(infoView->ZoomRow, 1, infoView->statusInfoIdx), getZoom());
            k->setData(k->index(infoView->SelectedRow, 1, infoView->statusInfoIdx), getSelectedFileSize());
            k->setData(k->index(infoView->PickedRow, 1, infoView->statusInfoIdx), getPicked());
        }
        else {
            k->setData(k->index(infoView->PositionRow, 1, infoView->statusInfoIdx), "");
            k->setData(k->index(infoView->ZoomRow, 1, infoView->statusInfoIdx), "");
            k->setData(k->index(infoView->SelectedRow, 1, infoView->statusInfoIdx), "");
            k->setData(k->index(infoView->PickedRow, 1, infoView->statusInfoIdx), "");
        }
        infoView->isNewImageDataChange = false;
    }
}

void MW::clearStatus()
{
    if (G::isLogger) G::log(__FUNCTION__);
    statusLabel->setText("");
}

void MW::updateMetadataThreadRunStatus(bool isRunning,bool showCacheLabel, QString calledBy)
{
//    return;  //rghmacdelay
    if (G::isLogger) G::log(__FUNCTION__);
    if (isRunning) {
        metadataThreadRunningLabel->setStyleSheet("QLabel {color:Red;}");
        #ifdef Q_OS_WIN
        metadataThreadRunningLabel->setStyleSheet("QLabel {color:Red;font-size: 24px;}");
        #endif
    }
    else {
        metadataThreadRunningLabel->setStyleSheet("QLabel {color:Green;}");
        #ifdef Q_OS_WIN
        metadataThreadRunningLabel->setStyleSheet("QLabel {color:Green;font-size: 24px;}");
        #endif
    }
    metadataThreadRunningLabel->setText("◉");
    if (isShowCacheProgressBar && !G::isSlideShow) progressLabel->setVisible(showCacheLabel);
    calledBy = "";  // suppress compiler warning
}

void MW::updateImageCachingThreadRunStatus(bool isRunning, bool showCacheLabel)
{
//    return;  //rghmacdelay
    if (G::isLogger) G::log(__FUNCTION__);
    if (isRunning) {
        if (G::isTest) testTime.restart();
        imageThreadRunningLabel->setStyleSheet("QLabel {color:Red;}");
        #ifdef Q_OS_WIN
        imageThreadRunningLabel->setStyleSheet("QLabel {color:Red; font-size: 24px;}");
        #endif
    }
    else {
        if (G::isTest) {
            int ms = testTime.elapsed();
            int n = icd->imCache.count();
            if (n)
            qDebug() << __FUNCTION__
                    << "Total time to fill cache =" << ms
                    << "   Images cached =" << n
                    << "   ms per image =" << ms / n
                       ;
        }
        imageThreadRunningLabel->setStyleSheet("QLabel {color:Green;}");
        #ifdef Q_OS_WIN
        imageThreadRunningLabel->setStyleSheet("QLabel {color:Green; font-size: 24px;}");
        #endif
    }
    imageThreadRunningLabel->setText("◉");
    if (isShowCacheProgressBar) progressLabel->setVisible(showCacheLabel);
}

void MW::setThreadRunStatusInactive()
{
    if (G::isLogger) G::log(__FUNCTION__);
    metadataThreadRunningLabel->setStyleSheet("QLabel {color:Gray;}");
    imageThreadRunningLabel->setStyleSheet("QLabel {color:Gray;}");
    #ifdef Q_OS_WIN
    metadataThreadRunningLabel->setStyleSheet("QLabel {color:Gray;font-size: 24px;}");
    imageThreadRunningLabel->setStyleSheet("QLabel {color:Gray;font-size: 24px;}");
    #endif
    metadataThreadRunningLabel->setText("◉");
    imageThreadRunningLabel->setText("◉");
}

void MW::resortImageCache()
{
    if (G::isLogger) G::log(__FUNCTION__);
    if (!dm->sf->rowCount()) return;
    QString currentFilePath = currentDmIdx.data(G::PathRole).toString();
    imageCacheThread->rebuildImageCacheParameters(currentFilePath, __FUNCTION__);
    // change to ImageCache
    imageCacheThread->setCurrentPosition(dm->currentFilePath);
//    emit setImageCachePosition(dm->currentFilePath);
}

void MW::sortIndicatorChanged(int column, Qt::SortOrder sortOrder)
{
/*
    This slot function is triggered by the tableView->horizontalHeader sortIndicatorChanged
    signal being emitted, which tells us that the data model sort/filter has been re-sorted.
    As a consequence we need to update the menu checked status for the correct column and also
    resync the image cache. However, changing the menu checked state for any of the menu sort
    actions triggers a sort, which needs to be suppressed while syncing the menu actions with
    tableView.
*/
    if (G::isLogger) G::log(__FUNCTION__);
//    QString columnName = tableView->model()->headerData(column, Qt::Horizontal).toString();
//    qDebug() << __FUNCTION__ << column << columnName << sortOrder << sortColumn;
    if (!G::allMetadataLoaded && column > G::DimensionsColumn) loadEntireMetadataCache("SortChange");

    sortMenuUpdateToMatchTable = true; // suppress sorting to update menu
    switch (column) {
    case G::NameColumn: sortFileNameAction->setChecked(true); break;
    case G::PickColumn: sortPickAction->setChecked(true); break;
    case G::LabelColumn: sortLabelAction->setChecked(true); break;
    case G::RatingColumn: sortRatingAction->setChecked(true); break;
    case G::TypeColumn: sortFileTypeAction->setChecked(true); break;
    case G::SizeColumn: sortFileSizeAction->setChecked(true); break;
    case G::CreatedColumn: sortCreateAction->setChecked(true); break;
    case G::ModifiedColumn: sortModifyAction->setChecked(true); break;
    case G::CreatorColumn: sortCreatorAction->setChecked(true); break;
    case G::MegaPixelsColumn: sortMegaPixelsAction->setChecked(true); break;
    case G::DimensionsColumn: sortDimensionsAction->setChecked(true); break;
    case G::ApertureColumn: sortApertureAction->setChecked(true); break;
    case G::ShutterspeedColumn: sortShutterSpeedAction->setChecked(true); break;
    case G::ISOColumn: sortISOAction->setChecked(true); break;
    case G::CameraModelColumn: sortModelAction->setChecked(true); break;
    case G::LensColumn: sortLensAction->setChecked(true); break;
    case G::FocalLengthColumn: sortFocalLengthAction->setChecked(true); break;
    case G::TitleColumn: sortTitleAction->setChecked(true); break;
    default:
        // table column clicked and sorted that is not a menu sort item - uncheck all menu items
        if (sortFileNameAction->isChecked()) sortFileNameAction->setChecked(false);
        if (sortFileTypeAction->isChecked()) sortFileTypeAction->setChecked(false);
        if (sortFileSizeAction->isChecked()) sortFileSizeAction->setChecked(false);
        if (sortCreateAction->isChecked()) sortCreateAction->setChecked(false);
        if (sortModifyAction->isChecked()) sortModifyAction->setChecked(false);
        if (sortPickAction->isChecked()) sortPickAction->setChecked(false);
        if (sortLabelAction->isChecked()) sortLabelAction->setChecked(false);
        if (sortRatingAction->isChecked()) sortRatingAction->setChecked(false);
        if (sortMegaPixelsAction->isChecked()) sortMegaPixelsAction->setChecked(false);
        if (sortDimensionsAction->isChecked()) sortDimensionsAction->setChecked(false);
        if (sortApertureAction->isChecked()) sortApertureAction->setChecked(false);
        if (sortShutterSpeedAction->isChecked()) sortShutterSpeedAction->setChecked(false);
        if (sortISOAction->isChecked()) sortISOAction->setChecked(false);
        if (sortModelAction->isChecked()) sortModelAction->setChecked(false);
        if (sortFocalLengthAction->isChecked()) sortFocalLengthAction->setChecked(false);
        if (sortTitleAction->isChecked()) sortTitleAction->setChecked(false);
    }
    if(sortOrder == Qt::DescendingOrder) sortReverseAction->setChecked(true);
    else sortReverseAction->setChecked(false);
    sortMenuUpdateToMatchTable = false;

    // get the current selected item
    currentRow = dm->sf->mapFromSource(currentDmIdx).row();
    thumbView->iconViewDelegate->currentRow = currentRow;
    gridView->iconViewDelegate->currentRow = currentRow;

    scrollToCurrentRow();

    resortImageCache();
}

/*  *******************************************************************************************

    FILTERS & SORTING

    Filters have two types:

        1. Metadata known by the operating system (name, suffix, size, modified date)
        2. Metadata read from the file by Winnow (ie cameera model, aperture, ...)

    When a folder is first selected only type 1 information is known for all the files in the
    folder unless the folder is small enough so all the files were read in one pass. Filtering
    and sorting can only occur when the filter item is known for all the files. This is
    tracked by G::allMetadataLoaded.

    Type 2 Filtration steps:

        * Make sure all metadata has been loaded for all files
        * Build the filters
        * Filter based on criteria selected
*/

void MW::filterDockVisibilityChange(bool isVisible)
{
    if (G::isLogger) G::log(__FUNCTION__);
//    qDebug() << __FUNCTION__ << isVisible;
    if (isVisible && !G::isInitializing) launchBuildFilters();
}

void MW::launchBuildFilters()
{
    if (G::isLogger) G::log(__FUNCTION__);
    if (G::isInitializing) return;
    if (filterDock->visibleRegion().isNull()) {
        filters->setSoloMode(filterSoloAction->isChecked());
//        G::popUp->showPopup("Filters will only be updated when the filters panel is visible.");
//        return;
    }
    if (filters->filtersBuilt) {
        G::popUp->showPopup("Filters are up-to-date.");
        return;
    }
    if (dm->loadingModel) {
        G::popUp->showPopup("Not all data required for filtering has been loaded yet.");
        return;
    }

    filters->msgFrame->setVisible(true);
    buildFilters->build();
}

void MW::filterChange(QString source)
{
/*
    All filter changes should be routed to here as a central clearing house. The datamodel
    filter is refreshed, the filter panel counts are updated, the current index is updated,
    the image cache is rebuilt to match the current filter, any prior selection that is still
    available is set, the thumb and grid first/last/thumbsPerPage parameters are recalculated
    and icons are loaded if necessary.
*/
    if (G::isLogger || G::isFlowLogger) G::log(__FUNCTION__, "Src: " + source);
    qDebug() << __FUNCTION__ << "called from:" << source;
    // ignore if new folder is being loaded
    if (!G::isNewFolderLoaded) {
        G::popUp->showPopup("Please wait for the folder to complete loading...", 2000);
        return;
    }

    if (G::stop) return;

    // update filter checkbox
    qApp->processEvents();

    // if filter chnage source is the filter panel then sync menu actions isChecked property
    if (source == "Filters::itemClickedSignal") filterSyncActionsWithFilters();

//    imageCacheThread->pauseImageCache();

    // Need all metadata loaded before filtering
    if (source != "MW::clearAllFilters") {
        dm->forceBuildFilters = true;
        if (!G::allMetadataLoaded) dm->addAllMetadata();
        // failed to load all metadata - maybe terminated by user pressing ESC
        if (!G::allMetadataLoaded) return;
    }

    QApplication::setOverrideCursor(Qt::WaitCursor);

    // refresh the proxy sort/filter, which updates the selectionIndex, which triggers a
    // scroll event and the metadataCache updates the icons and thumbnails
    isFilterChange = true;      // prevent unwanted fileSelectionChange()
    dm->sf->filterChange();

    // update filter panel image count by filter item
    buildFilters->updateCountFiltered();
    if (source == "Filters::itemChangedSignal search text change") buildFilters->unfilteredItemSearchCount();

    // recover sort after filtration
    sortChange("filterChange");

    isFilterChange = false;     // allow fileSelectionChange()

    // update the status panel filtration status
    updateStatusBar();

    // if filter has eliminated all rows so nothing to show
    if (!dm->sf->rowCount()) {
        nullFiltration();
        QApplication::restoreOverrideCursor();
        return;
    }

    // get the current selected item
    currentRow = dm->sf->mapFromSource(currentDmIdx).row();
    // check if still in filtered set, if not select first item in filtered set
    if (currentRow == -1) currentRow = 0;
    thumbView->iconViewDelegate->currentRow = currentRow;
    gridView->iconViewDelegate->currentRow = currentRow;
    thumbView->selectThumb(currentRow);
    QModelIndex idx = dm->sf->index(currentRow, 0);
    // the file path is used as an index in ImageView
    QString fPath = dm->sf->index(currentRow, 0).data(G::PathRole).toString();
    // also update datamodel, used in MdCache
    dm->currentFilePath = fPath;

//    centralLayout->setCurrentIndex(prevCentralView);
    updateStatus(true, "", __FUNCTION__);

    // sync image cache with datamodel filtered proxy dm->sf
    imageCacheThread->rebuildImageCacheParameters(fPath, __FUNCTION__);
    /*
    if (thumbView->waitUntilOkToScroll()) {
        // setting the selection index also triggers fileSelectionChange()
        selectionModel->setCurrentIndex(idx, QItemSelectionModel::Current);
        thumbView->selectThumb(idx);
    }
//    */
//    qDebug() << __FUNCTION__ << idx.data() << "Calling fileSelectionChange(idx, idx)";

    QApplication::restoreOverrideCursor();

    fileSelectionChange(idx, idx);
    source = "";    // suppress compiler warning
}

void MW::quickFilter()
{
    if (G::isLogger) G::log(__FUNCTION__);

    // make sure the filters have been built
    if (!filters->filtersBuilt) buildFilters->build();

    // checked
    if (filterSearchAction->isChecked()) filters->searchTrue->setCheckState(0, Qt::Checked);
    if (filterRating1Action->isChecked()) filters->ratings1->setCheckState(0, Qt::Checked);
    if (filterRating2Action->isChecked()) filters->ratings2->setCheckState(0, Qt::Checked);
    if (filterRating3Action->isChecked()) filters->ratings3->setCheckState(0, Qt::Checked);
    if (filterRating4Action->isChecked()) filters->ratings4->setCheckState(0, Qt::Checked);
    if (filterRating5Action->isChecked()) filters->ratings5->setCheckState(0, Qt::Checked);
    if (filterRedAction->isChecked()) filters->labelsRed->setCheckState(0, Qt::Checked);
    if (filterYellowAction->isChecked()) filters->labelsYellow->setCheckState(0, Qt::Checked);
    if (filterGreenAction->isChecked()) filters->labelsGreen->setCheckState(0, Qt::Checked);
    if (filterBlueAction->isChecked()) filters->labelsBlue->setCheckState(0, Qt::Checked);
    if (filterPurpleAction->isChecked()) filters->labelsPurple->setCheckState(0, Qt::Checked);

    // unchecked
    if (!filterRating1Action->isChecked()) filters->ratings1->setCheckState(0, Qt::Unchecked);
    if (!filterRating2Action->isChecked()) filters->ratings2->setCheckState(0, Qt::Unchecked);
    if (!filterRating3Action->isChecked()) filters->ratings3->setCheckState(0, Qt::Unchecked);
    if (!filterRating4Action->isChecked()) filters->ratings4->setCheckState(0, Qt::Unchecked);
    if (!filterRating5Action->isChecked()) filters->ratings5->setCheckState(0, Qt::Unchecked);
    if (!filterRedAction->isChecked()) filters->labelsRed->setCheckState(0, Qt::Unchecked);
    if (!filterYellowAction->isChecked()) filters->labelsYellow->setCheckState(0, Qt::Unchecked);
    if (!filterGreenAction->isChecked()) filters->labelsGreen->setCheckState(0, Qt::Unchecked);
    if (!filterBlueAction->isChecked()) filters->labelsBlue->setCheckState(0, Qt::Unchecked);
    if (!filterPurpleAction->isChecked()) filters->labelsPurple->setCheckState(0, Qt::Unchecked);

    filterChange("MW::quickFilter");
}

void MW::filterSyncActionsWithFilters()
{
    if (G::isLogger) G::log(__FUNCTION__);
    filterRating1Action->setChecked(filters->ratings1->checkState(0));
    filterRating2Action->setChecked(filters->ratings2->checkState(0));
    filterRating3Action->setChecked(filters->ratings3->checkState(0));
    filterRating4Action->setChecked(filters->ratings4->checkState(0));
    filterRating5Action->setChecked(filters->ratings5->checkState(0));
    filterRedAction->setChecked(filters->labelsRed->checkState(0));
    filterYellowAction->setChecked(filters->labelsYellow->checkState(0));
    filterGreenAction->setChecked(filters->labelsGreen->checkState(0));
    filterBlueAction->setChecked(filters->labelsBlue->checkState(0));
    filterPurpleAction->setChecked(filters->labelsPurple->checkState(0));
    filterLastDayAction->setChecked(filters->isOnlyMostRecentDayChecked());
}

void MW::invertFilters()
{
/*

*/
    if (G::isLogger) G::log(__FUNCTION__);
    if (!G::allMetadataLoaded) loadEntireMetadataCache("FilterChange");

    if (dm->rowCount() == 0) {
        G::popUp->showPopup("No images available to invert filtration", 2000);
        filterLastDayAction->setChecked(false);
        return;
    }
    if (dm->rowCount() == dm->sf->rowCount()) {
        G::popUp->showPopup("No filters assigned - null inversion would result", 2000);
        filterLastDayAction->setChecked(false);
        return;
    }
    filters->invertFilters();

    filterChange("MW::invertFilters");
}

void MW::uncheckAllFilters()
{
    if (G::isLogger) G::log(__FUNCTION__);
    filters->uncheckAllFilters();
    filterPickAction->setChecked(false);
    filterRating1Action->setChecked(false);
    filterRating2Action->setChecked(false);
    filterRating3Action->setChecked(false);
    filterRating4Action->setChecked(false);
    filterRating5Action->setChecked(false);
    filterRedAction->setChecked(false);
    filterYellowAction->setChecked(false);
    filterGreenAction->setChecked(false);
    filterBlueAction->setChecked(false);
    filterPurpleAction->setChecked(false);
    filterLastDayAction->setChecked(false);
}

void MW::clearAllFilters()
{
    if (G::isLogger) G::log(__FUNCTION__);
    if (!G::allMetadataLoaded) loadEntireMetadataCache("FilterChange");   // rgh is this reqd
    uncheckAllFilters();
    filters->searchString = "";
    dm->searchStringChange("");
    filterChange("MW::clearAllFilters");    
}

void MW::setFilterSolo()
{
    if (G::isLogger) G::log(__FUNCTION__);
    filters->setSoloMode(filterSoloAction->isChecked());
    setting->setValue("isSoloFilters", filterSoloAction->isChecked());
}

void MW::filterLastDay()
{
/*
.
*/
    if (G::isLogger) G::log(__FUNCTION__);
    if (dm->sf->rowCount() == 0) {
        G::popUp->showPopup("No images available to filter", 2000);
        filterLastDayAction->setChecked(false);
        return;
    }

    // if the additional filters have not been built then do an update
    if (!filters->filtersBuilt) {
        qDebug() << __FUNCTION__ << "build filters";
        launchBuildFilters();
        G::popUp->showPopup("Building filters.", 0);
        buildFilters->wait();
        G::popUp->hide();
//    if (!filters->days->childCount()) launchBuildFilters();
    }

    // if there still are no days then tell user and return
    int last = filters->days->childCount();
    if (filters->days->childCount() == 0) {
        G::popUp->showPopup("No days are available to filter", 2000);
        filterLastDayAction->setChecked(false);
        return;
    }

    if (filterLastDayAction->isChecked()) {
        filters->days->child(last - 1)->setCheckState(0, Qt::Checked);
    }
    else {
        filters->days->child(last - 1)->setCheckState(0, Qt::Unchecked);
    }

    filterChange("MW::filterLastDay");
}

void MW::refine()
{
/*
    Clears refine for all rows, sets refine = true if pick = true, and clears pick
    for all rows.
*/
    if (G::isLogger) G::log(__FUNCTION__);
    // if slideshow then do not refine
    if (G::isSlideShow) return;

    // Are there any picks to refine?
    bool isPick = false;
    for (int row = 0; row < dm->rowCount(); ++row) {
        if (dm->index(row, G::PickColumn).data() == "true") {
            isPick = true;
            break;
        }
    }

    if (!isPick) {
        G::popUp->showPopup("There are no picks to refine", 2000);
        return;
    }

    QMessageBox msgBox;
    int msgBoxWidth = 300;
    msgBox.setWindowTitle("Refine Picks");
    msgBox.setText("This operation will filter to show only your picks and then clear your picks.");
    msgBox.setInformativeText("Do you want continue?");
    msgBox.setStandardButtons(QMessageBox::Yes | QMessageBox::Cancel);
    msgBox.setDefaultButton(QMessageBox::Yes);
    msgBox.setIcon(QMessageBox::Warning);
    QString s = "QWidget{font-size: 12px; background-color: rgb(85,85,85); color: rgb(229,229,229);}"
                "QPushButton:default {background-color: rgb(68,95,118);}";
    msgBox.setStyleSheet(s);
    QSpacerItem* horizontalSpacer = new QSpacerItem(msgBoxWidth, 0, QSizePolicy::Minimum, QSizePolicy::Expanding);
    QGridLayout* layout = static_cast<QGridLayout*>(msgBox.layout());
    layout->addItem(horizontalSpacer, layout->rowCount(), 0, 1, layout->columnCount());
    int ret = msgBox.exec();
    if (ret == QMessageBox::Cancel) return;

    clearAllFilters();

    // clear refine = pick
    pushPick("Begin multiple select");
    for (int row = 0; row < dm->rowCount(); ++row) {
        if (dm->index(row, G::PickColumn).data() == "true") {
            // save pick history
            QString fPath = dm->sf->index(row, G::PathColumn).data(G::PathRole).toString();
            pushPick(fPath, "true");
            // clear picks
            dm->setData(dm->index(row, G::RefineColumn), true);
            dm->setData(dm->index(row, G::PickColumn), "false");
        }
        else dm->setData(dm->index(row, G::RefineColumn), false);
    }
    pushPick("End multiple select");

    // reset filters
    filters->uncheckAllFilters();
    filters->refineTrue->setCheckState(0, Qt::Checked);

    filterChange("MW::refine");
}

void MW::sortChangeFromAction()
{
/*
    When a sort change menu item is picked this function is called, and then redirects to
    sortChange.
*/
    if (G::isLogger) G::log(__FUNCTION__);

    if (sortFileNameAction->isChecked()) sortColumn = G::NameColumn;        // core
    if (sortFileTypeAction->isChecked()) sortColumn = G::TypeColumn;        // core
    if (sortFileSizeAction->isChecked()) sortColumn = G::SizeColumn;        // core
    if (sortCreateAction->isChecked()) sortColumn = G::CreatedColumn;       // core
    if (sortModifyAction->isChecked()) sortColumn = G::ModifiedColumn;      // core
    if (sortPickAction->isChecked()) sortColumn = G::PickColumn;            // core
    if (sortLabelAction->isChecked()) sortColumn = G::LabelColumn;          // core
    if (sortRatingAction->isChecked()) sortColumn = G::RatingColumn;        // core
    if (sortMegaPixelsAction->isChecked()) sortColumn = G::MegaPixelsColumn;
    if (sortDimensionsAction->isChecked()) sortColumn = G::DimensionsColumn;
    if (sortApertureAction->isChecked()) sortColumn = G::ApertureColumn;
    if (sortShutterSpeedAction->isChecked()) sortColumn = G::ShutterspeedColumn;
    if (sortISOAction->isChecked()) sortColumn = G::ISOColumn;
    if (sortModelAction->isChecked()) sortColumn = G::CameraModelColumn;
    if (sortFocalLengthAction->isChecked()) sortColumn = G::FocalLengthColumn;
    if (sortTitleAction->isChecked()) sortColumn = G::TitleColumn;
    sortChange("Action");
}

void MW::sortChange(QString source)
{
/*
    Triggered by a menu sort item, a new folder or a new workspace. Core sort items (QFileInfo
    items) are always loaded into the datamodel, so we can sort on them at any time. Non-core
    items, read from the image file metadata, are only loaded on demand. All non-core items
    must be loaded in order to sort on them.

    The sort order (ascending or descending) can be set by the menu, the button icon on the
    statusbar or a workspace change.
*/
    if (G::isLogger || G::isFlowLogger) G::log(__FUNCTION__, "Src: " + source);

    if (G::isInitializing) return;

        /*
    qDebug() << __FUNCTION__ << "source =" << source
             << "G::isNewFolderLoaded =" << G::isNewFolderLoaded
             << "G::isInitializing =" << G::isInitializing
             << "prevSortColumn =" << prevSortColumn
             << "sortColumn =" << sortColumn
             << "isReverseSort =" << isReverseSort
             << "prevIsReverseSort =" << prevIsReverseSort
                ;
//                */

    // reset sort to file name if was sorting on non-core metadata while folder still loading
    if (!G::isNewFolderLoaded && sortColumn > G::CreatedColumn) {
        prevSortColumn = G::NameColumn;
        updateSortColumn(G::NameColumn);
    }

    // check if sorting has changed
    bool sortHasChanged = false;
    if (sortColumn != prevSortColumn || isReverseSort != prevIsReverseSort) {
        sortHasChanged = true;
        prevSortColumn = sortColumn;
        prevIsReverseSort = isReverseSort;
    }

    // do not sort conditions
    bool doNotSort = false;
    if (sortMenuUpdateToMatchTable)
        doNotSort = true;
    if (!G::isNewFolderLoaded && sortColumn > G::CreatedColumn)
        doNotSort = true;
    if (!G::isNewFolderLoaded && sortColumn == G::NameColumn && !sortReverseAction->isChecked())
        doNotSort = true;
    if (G::isNewFolderLoaded && !sortHasChanged)
        doNotSort = true;
    if (doNotSort) return;

    // Need all metadata loaded before sorting non-core metadata
    // rgh all metadata always loaded now - change this?
    if (!G::allMetadataLoaded && sortColumn > G::CreatedColumn)
        loadEntireMetadataCache("SortChange");

    // failed to load all metadata, restore prior sort in menu and return
    if (!G::allMetadataLoaded && sortColumn > G::CreatedColumn) {
        /*
        qDebug() << __FUNCTION__ << "failed"
                 << "sortColumn =" << sortColumn
                 << "prevSortColumn =" << prevSortColumn
                 ;
//                 */
        updateSortColumn(prevSortColumn);
        return;
    }

    prevSortColumn = sortColumn;
    /*
    qDebug() << __FUNCTION__ << "succeeded"
             << "sortColumn =" << sortColumn
             << "prevSortColumn =" << prevSortColumn
             << "  Commencing sort"
             ;
//             */

    if (G::isNewFolderLoaded) {
        G::popUp->showPopup("Sorting...", 0);
    }
    else {
        setCentralMessage("Sorting images");
    }

    thumbView->sortThumbs(sortColumn, isReverseSort);

//    if (!G::allMetadataLoaded) return;

    // get the current selected item
    currentRow = dm->sf->mapFromSource(currentDmIdx).row();
//    if (G::isNewFolderLoaded) currentRow = dm->sf->mapFromSource(currentDmIdx).row();
//    else currentRow = 0;

    thumbView->iconViewDelegate->currentRow = currentRow;
    gridView->iconViewDelegate->currentRow = currentRow;
    QModelIndex idx = dm->sf->index(currentRow, 0);
    selectionModel->setCurrentIndex(idx, QItemSelectionModel::Current);
    // the file path is used as an index in ImageView
    QString fPath = dm->sf->index(currentRow, 0).data(G::PathRole).toString();
    // also update datamodel, used in MdCache and EmbelProperties
    dm->currentFilePath = fPath;

//    if (!G::allMetadataLoaded) return;

    centralLayout->setCurrentIndex(prevCentralView);
    updateStatus(true, "", __FUNCTION__);

    // sync image cache with datamodel filtered proxy unless sort has been triggered by a
    // filter change, which will do its own rebuildImageCacheParameters
    if (source != "filterChange")
        imageCacheThread->rebuildImageCacheParameters(fPath, "SortChange");

    /* if the previous selected image is also part of the filtered datamodel then the
       selected index does not change and fileSelectionChange will not be signalled.
       Therefore we call it here to force the update to caching and icons */
//    qDebug() << __FUNCTION__ << idx.data() << "Calling fileSelectionChange(idx, idx)";
    fileSelectionChange(idx, idx);

    scrollToCurrentRow();
    G::popUp->hide();

}

void MW::updateSortColumn(int sortColumn)
{
    if (G::isLogger) G::log(__FUNCTION__);
    /*
    qDebug() << __FUNCTION__
             << "sortColumn =" << sortColumn
             << "prevSortColumn =" << prevSortColumn
             ;
    //    */

    if (sortColumn == 0) sortColumn = G::NameColumn;

    if (sortColumn == G::NameColumn) sortFileNameAction->setChecked(true);
    if (sortColumn == G::TypeColumn) sortFileTypeAction->setChecked(true);
    if (sortColumn == G::SizeColumn) sortFileSizeAction->setChecked(true);
    if (sortColumn == G::CreatedColumn) sortCreateAction->setChecked(true);
    if (sortColumn == G::ModifiedColumn) sortModifyAction->setChecked(true);
    if (sortColumn == G::PickColumn) sortPickAction->setChecked(true);
    if (sortColumn == G::LabelColumn) sortLabelAction->setChecked(true);
    if (sortColumn == G::RatingColumn) sortRatingAction->setChecked(true);
    if (sortColumn == G::MegaPixelsColumn) sortMegaPixelsAction->setChecked(true);
    if (sortColumn == G::DimensionsColumn) sortDimensionsAction->setChecked(true);
    if (sortColumn == G::ApertureColumn) sortApertureAction->setChecked(true);
    if (sortColumn == G::ShutterspeedColumn) sortShutterSpeedAction->setChecked(true);
    if (sortColumn == G::ISOColumn) sortISOAction->setChecked(true);
    if (sortColumn == G::CameraModelColumn) sortModelAction->setChecked(true);
    if (sortColumn == G::FocalLengthColumn) sortFocalLengthAction->setChecked(true);
    if (sortColumn == G::TitleColumn) sortTitleAction->setChecked(true);
}

void MW::toggleSortDirectionClick()
{
/*
    This is called by connect signals from the menu action and the reverse sort button.  The
    call is redirected to toggleSortDirection, which has a parameter which is not supported
    by the action and button signals.
*/
    if (G::isLogger) G::log(__FUNCTION__);
    toggleSortDirection(Tog::toggle);
    sortChange(__FUNCTION__);
}

void MW::toggleSortDirection(Tog n)
{
    if (G::isLogger) G::log(__FUNCTION__);
    if (prevIsReverseSort == isReverseSort)
    prevIsReverseSort = isReverseSort;
    if (n == Tog::toggle) isReverseSort = !isReverseSort;
    if (n == Tog::off) isReverseSort = false;
    if (n == Tog::on) isReverseSort = true;
    if (isReverseSort) {
        sortReverseAction->setChecked(true);
        reverseSortBtn->setIcon(QIcon(":/images/icon16/Z-A.png"));
    }
    else {
        sortReverseAction->setChecked(false);
        reverseSortBtn->setIcon(QIcon(":/images/icon16/A-Z.png"));
    }
}

void MW::toggleColorManageClick()
{
/*
    This is called by connect signals from the menu action and the color manage button.  The
    call is redirected to toggleColorManage, which has a parameter which is not supported
    by the action and button signals.
*/
    if (G::isLogger) G::log(__FUNCTION__);
    toggleColorManage(Tog::toggle);
}

void MW::toggleColorManage(Tog n)
{
/*
    The image cache is rebuilt to show the color manage option.
*/
    if (G::isLogger) G::log(__FUNCTION__);
    if (n == Tog::toggle) G::colorManage = !G::colorManage;
    if (n == Tog::off) G::colorManage = false;
    if (n == Tog::on) G::colorManage = true;
    colorManageAction->setChecked(G::colorManage);
    if (G::colorManage) {
        colorManageToggleBtn->setIcon(QIcon(":/images/icon16/rainbow1.png"));
    }
    else {
        colorManageToggleBtn->setIcon(QIcon(":/images/icon16/norainbow1.png"));
    }

    if (dm->rowCount() == 0) return;

//    imageView->loadImage(dm->currentFilePath, __FUNCTION__, true/*refresh*/);

    // set the isCached indicator on thumbnails to false (shows red dot on bottom right)
    for (int row = 0; row < dm->rowCount(); ++row) {
        QString fPath = dm->index(row, G::PathColumn).data(G::PathRole).toString();
        setCachedStatus(fPath, false);
    }
    // reload image cache
    imageCacheThread->colorManageChange();
}

void MW::scrollToCurrentRow()
{
/*
    Called after a sort or when thumbs shrink/enlarge. The current image may no longer be
    visible hence need to scroll to the current row.
*/
    if (G::isLogger) G::log(__FUNCTION__);
    currentRow = dm->sf->mapFromSource(currentDmIdx).row();
    QModelIndex idx = dm->sf->index(currentRow, 0);
//    G::wait(100);

    G::ignoreScrollSignal = true;
    if (thumbView->isVisible()) thumbView->scrollToRow(currentRow, __FUNCTION__);
    if (gridView->isVisible()) gridView->scrollToRow(currentRow, __FUNCTION__);
    if (tableView->isVisible()) tableView->scrollTo(idx,
         QAbstractItemView::ScrollHint::PositionAtCenter);
    G::ignoreScrollSignal = false;

    updateIconsVisible(true);
    metadataCacheThread->scrollChange(__FUNCTION__);
}

void MW::showHiddenFiles()
{
    // rgh ??
    if (G::isLogger) G::log(__FUNCTION__);
//    fsTree->setModelFlags();
}

void MW::thumbsEnlarge()
{
    if (G::isLogger) G::log(__FUNCTION__);
    if (G::mode == "Grid") gridView->justify(IconView::JustifyAction::Enlarge);
    else {
        if (thumbView->isWrapping()) thumbView->justify(IconView::JustifyAction::Enlarge);
        else thumbView->thumbsEnlarge();
    }
    scrollToCurrentRow();
    // may be less icons to cache
    numberIconsVisibleChange();

    // if thumbView visible and zoomed in imageView then may need to redo the zoomFrame
    QModelIndex idx = thumbView->indexAt(thumbView->mapFromGlobal(QCursor::pos()));
    if (idx.isValid()) {
        thumbView->zoomCursor(idx, /*forceUpdate=*/true);
    }
}

void MW::thumbsShrink()
{
    if (G::isLogger) G::log(__FUNCTION__);
    if (G::mode == "Grid") gridView->justify(IconView::JustifyAction::Shrink);
    else {
        if (thumbView->isWrapping()) thumbView->justify(IconView::JustifyAction::Shrink);
        else thumbView->thumbsShrink();
    }
    scrollToCurrentRow();
    // may be more icons to cache
    numberIconsVisibleChange();

    // if thumbView visible and zoomed in imageView then may need to redo the zoomFrame
    QModelIndex idx = thumbView->indexAt(thumbView->mapFromGlobal(QCursor::pos()));
    if (idx.isValid()) {
        thumbView->zoomCursor(idx, /*forceUpdate=*/true);
    }
}

void MW::addRecentFolder(QString fPath)
{
    if (G::isLogger) G::log(__FUNCTION__);
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
    if (G::isLogger) G::log(__FUNCTION__);
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
    if (G::isLogger) G::log(__FUNCTION__);
    QString dirPath = recentFolderActions->text();
    fsTree->select(dirPath);
    folderSelectionChange();
}

void MW::invokeIngestHistoryFolder(QAction *ingestHistoryFolderActions)
{
    if (G::isLogger) G::log(__FUNCTION__);
    QString dirPath = ingestHistoryFolderActions->text();
    fsTree->select(dirPath);
    folderSelectionChange();
//    revealInFileBrowser(dirPath);
}

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
    if (G::isLogger) G::log(__FUNCTION__);
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
    if (G::isLogger) G::log(__FUNCTION__);
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
    if (G::isLogger) G::log(__FUNCTION__);
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
    if (G::isLogger) G::log(__FUNCTION__);
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
    sortChange(__FUNCTION__);
    // in case thumbdock visibility changed by status of wasThumbDockVisible in loupeDisplay etc
//    setThumbDockVisibity();
}

void MW::snapshotWorkspace(workspaceData &wsd)
{
    if (G::isLogger) G::log(__FUNCTION__);
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
    if (G::isLogger) G::log(__FUNCTION__);
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
    if (G::isLogger) G::log(__FUNCTION__);
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
    if (G::isLogger) G::log(__FUNCTION__);
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
    if (G::isLogger) G::log(__FUNCTION__);
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
    if (G::isLogger) G::log(__FUNCTION__);
    QRect desktop = QGuiApplication::screens().first()->geometry();
//    QRect desktop = qApp->desktop()->availableGeometry();
//    qDebug() << __FUNCTION__ << desktop << desktop1;
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
    if (useInfoView) metadataDock->setFloating(false);
    embelDock->setFloating(false);
    thumbDock->setFloating(false);

    addDockWidget(Qt::LeftDockWidgetArea, folderDock);
    addDockWidget(Qt::LeftDockWidgetArea, favDock);
    addDockWidget(Qt::LeftDockWidgetArea, filterDock);
    if (useInfoView) addDockWidget(Qt::LeftDockWidgetArea, metadataDock);
//    addDockWidget(Qt::RightDockWidgetArea, embelDock);
    addDockWidget(Qt::BottomDockWidgetArea, thumbDock);

    MW::setTabPosition(Qt::LeftDockWidgetArea, QTabWidget::North);
    MW::tabifyDockWidget(folderDock, favDock);
    MW::tabifyDockWidget(favDock, filterDock);
    if (useInfoView) MW::tabifyDockWidget(filterDock, metadataDock);

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
    sortChange(__FUNCTION__);
    updateState();
}

void MW::renameWorkspace(int n, QString name)
{
    if (G::isLogger) G::log(__FUNCTION__);
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
    if (G::isLogger) G::log(__FUNCTION__);
    snapshotWorkspace((*workspaces)[n]);
    (*workspaces)[n].name = name;
}

void MW::reportWorkspace(int n)
{
    if (G::isLogger) G::log(__FUNCTION__);
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
    if (G::isLogger) G::log(__FUNCTION__);
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
    if (G::isLogger) G::log(__FUNCTION__);
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

void MW::reportState()
{
    if (G::isLogger) G::log(__FUNCTION__);
    workspaceData w;
    snapshotWorkspace(w);
    qDebug() << G::t.restart() << "\t" << "\nisMaximized" << w.isFullScreen
             << "\nisWindowTitleBarVisible" << w.isWindowTitleBarVisible
             << "\nisMenuBarVisible" << w.isMenuBarVisible
             << "\nisStatusBarVisible" << w.isStatusBarVisible
             << "\nisFolderDockVisible" << w.isFolderDockVisible
             << "\nisFavDockVisible" << w.isFavDockVisible
             << "\nisFilterDockVisible" << w.isFilterDockVisible
             << "\nisMetadataDockVisible" << w.isMetadataDockVisible
             << "\nisEmbelDockVisible" << w.isEmbelDockVisible
             << "\nisThumbDockVisible" << w.isThumbDockVisible
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
             << "\nshowShootingInfo" << w.isImageInfoVisible
             << "\nisLoupeDisplay" << w.isLoupeDisplay
             << "\nisGridDisplay" << w.isGridDisplay
             << "\nisTableDisplay" << w.isTableDisplay
             << "\nisCompareDisplay" << w.isCompareDisplay
             << "\nisEmbelDisplay" << w.isEmbelDisplay
             << "\nisColorManage" << ws.isColorManage
             << "\ncacheSizeMethod" << ws.cacheSizeMethod
                ;
}

void MW::reportMetadata()
{
    if (G::isLogger) G::log(__FUNCTION__);
    diagnosticsMetadata();
}

// Diagnostic Reports

//QString MW::d(QVariant x)
//// helper function to convert variable values to a string for diagnostic reporting
//{
//    return QVariant(x).toString();
//}

void MW::diagnosticsAll()
{
    if (G::isLogger) G::log(__FUNCTION__);
    QString reportString;
    QTextStream rpt;
    rpt.setString(&reportString);
    rpt << this->diagnostics();
    rpt << gridView->diagnostics();
    rpt << thumbView->diagnostics();
    rpt << imageView->diagnostics();
    rpt << embelProperties->diagnostics();
    rpt << metadata->diagnostics(dm->currentFilePath);
    rpt << dm->diagnostics();
    diagnosticsReport(reportString);
}

void MW::diagnosticsCurrent()
{
    if (G::isLogger) G::log(__FUNCTION__);
    QString reportString;
    QTextStream rpt;
    rpt.setString(&reportString);
    rpt << dm->diagnosticsForCurrentRow();
    rpt << metadata->diagnostics(dm->currentFilePath);
    diagnosticsReport(reportString);
}

QString MW::diagnostics()
{
    if (G::isLogger) G::log(__FUNCTION__);
    QString reportString;
    QTextStream rpt;
    rpt.setString(&reportString);
    rpt << Utilities::centeredRptHdr('=', "MainWindow Diagnostics");
    rpt << "\n";

    rpt << "\n" << "version = " << G::s(version);
    rpt << "\n" << "isShift = " << G::s(isShift);
    rpt << "\n" << "ignoreSelectionChange = " << G::s(ignoreSelectionChange);
    rpt << "\n" << "lastPrefPage = " << G::s(lastPrefPage);
//    rpt << "\n" << "mouseClickScroll = " << G::s(mouseClickScroll);
    rpt << "\n" << "displayPhysicalHorizontalPixels = " << G::s(G::displayPhysicalHorizontalPixels);
    rpt << "\n" << "displayPhysicalVerticalPixels = " << G::s(G::displayPhysicalVerticalPixels);
    rpt << "\n" << "checkIfUpdate = " << G::s(checkIfUpdate);
    rpt << "\n" << "isRatingBadgeVisible = " << G::s(isRatingBadgeVisible);
    rpt << "\n" << "classificationBadgeInImageDiameter = " << G::s(classificationBadgeInImageDiameter);
    rpt << "\n" << "classificationBadgeInThumbDiameter = " << G::s(classificationBadgeInThumbDiameter);
    rpt << "\n" << "rememberLastDir = " << G::s(rememberLastDir);
    rpt << "\n" << "lastDir = " << G::s(lastDir);
    rpt << "\n" << "ingestRootFolder = " << G::s(ingestRootFolder);
    rpt << "\n" << "ingestRootFolder2 = " << G::s(ingestRootFolder2);
    rpt << "\n" << "pathTemplateSelected = " << G::s(pathTemplateSelected);
    rpt << "\n" << "pathTemplateSelected2 = " << G::s(pathTemplateSelected2);
    rpt << "\n" << "filenameTemplateSelected = " << G::s(filenameTemplateSelected);
    rpt << "\n" << "manualFolderPath = " << G::s(manualFolderPath);
    rpt << "\n" << "manualFolderPath2 = " << G::s(manualFolderPath2);
    rpt << "\n" << "combineRawJpg = " << G::s(combineRawJpg);
    rpt << "\n" << "autoIngestFolderPath = " << G::s(autoIngestFolderPath);
    rpt << "\n" << "autoEjectUsb = " << G::s(autoEjectUsb);
    rpt << "\n" << "integrityCheck = " << G::s(integrityCheck);
    rpt << "\n" << "isBackgroundIngest = " << G::s(isBackgroundIngest);
    rpt << "\n" << "isBackgroundIngestBeep = " << G::s(isBackgroundIngestBeep);
    rpt << "\n" << "ingestIncludeXmpSidecar = " << G::s(ingestIncludeXmpSidecar);
    rpt << "\n" << "backupIngest = " << G::s(backupIngest);
    rpt << "\n" << "gotoIngestFolder = " << G::s(gotoIngestFolder);
    rpt << "\n" << "lastIngestLocation = " << G::s(lastIngestLocation);
    rpt << "\n" << "slideShowDelay = " << G::s(slideShowDelay);
    rpt << "\n" << "slideShowRandom = " << G::s(isSlideShowRandom);
    rpt << "\n" << "slideShowWrap = " << G::s(isSlideShowWrap);
    rpt << "\n" << "cacheSizeMB = " << G::s(cacheMaxMB);
    rpt << "\n" << "showCacheStatus = " << G::s(isShowCacheProgressBar);
    rpt << "\n" << "cacheDelay = " << G::s(cacheDelay);
    rpt << "\n" << "isShowCacheThreadActivity = " << G::s(isShowCacheProgressBar);
    rpt << "\n" << "progressWidth = " << G::s(progressWidth);
    rpt << "\n" << "cacheWtAhead = " << G::s(cacheWtAhead);
    rpt << "\n" << "isCachePreview = " << G::s(isCachePreview);
    rpt << "\n" << "cachePreviewWidth = " << G::s(cachePreviewWidth);
    rpt << "\n" << "cachePreviewHeight = " << G::s(cachePreviewHeight);
    rpt << "\n" << "fullScreenDocks.isFolders = " << G::s(fullScreenDocks.isFolders);
    rpt << "\n" << "fullScreenDocks.isFavs = " << G::s(fullScreenDocks.isFavs);
    rpt << "\n" << "fullScreenDocks.isFilters = " << G::s(fullScreenDocks.isFilters);
    rpt << "\n" << "fullScreenDocks.isMetadata = " << G::s(fullScreenDocks.isMetadata);
    rpt << "\n" << "fullScreenDocks.isThumbs = " << G::s(fullScreenDocks.isThumbs);
    rpt << "\n" << "fullScreenDocks.isStatusBar = " << G::s(fullScreenDocks.isStatusBar);
    rpt << "\n" << "isNormalScreen = " << G::s(isNormalScreen);
    rpt << "\n" << "currentViewDir = " << G::s(currentViewDirPath);
    rpt << "\n" << "prevMode = " << G::s(prevMode);
    rpt << "\n" << "currentRow = " << G::s(currentRow);
    rpt << "\n" << "scrollRow = " << G::s(scrollRow);
    rpt << "\n" << "currentDmIdx = row" << G::s(currentDmIdx.row()) << " col " << G::s(currentDmIdx.column());
    rpt << "\n" << "allIconsLoaded = " << G::s(allIconsLoaded);
    rpt << "\n" << "modeChangeJustHappened = " << G::s(modeChangeJustHappened);
    rpt << "\n" << "justUpdatedBestFit = " << G::s(justUpdatedBestFit);
    rpt << "\n" << "sortColumn = " << G::s(sortColumn);
    rpt << "\n" << "showImageCount = " << G::s(showImageCount);
    rpt << "\n" << "isCurrentFolderOkay = " << G::s(isCurrentFolderOkay);
    rpt << "\n" << "G::isSlideShow = " << G::s(G::isSlideShow);
    rpt << "\n" << "copyOp = " << G::s(copyOp);
    rpt << "\n" << "isDragDrop = " << G::s(isDragDrop);
    rpt << "\n" << "dragDropFilePath = " << G::s(dragDropFilePath);
    rpt << "\n" << "dragDropFolderPath = " << G::s(dragDropFolderPath);
    rpt << "\n" << "maxThumbSpaceHeight = " << G::s(maxThumbSpaceHeight);
    rpt << "\n" << "pickMemSize = " << G::s(pickMemSize);
    rpt << "\n" << "metadataLoaded = " << G::s(metadataLoaded);
    rpt << "\n" << "ignoreDockResize = " << G::s(ignoreDockResize);
    rpt << "\n" << "wasThumbDockVisible = " << G::s(wasThumbDockVisible);
    rpt << "\n" << "workspaceChange = " << G::s(workspaceChange);
    rpt << "\n" << "isUpdatingState = " << G::s(isUpdatingState);
    rpt << "\n" << "isFilterChange = " << G::s(isFilterChange);
    rpt << "\n" << "isRefreshingDM = " << G::s(isRefreshingDM);
    rpt << "\n" << "refreshCurrentPath = " << G::s(refreshCurrentPath);
    rpt << "\n" << "simulateJustInstalled = " << G::s(simulateJustInstalled);
    rpt << "\n" << "isSettings = " << G::s(isSettings);
    rpt << "\n" << "isStressTest = " << G::s(isStressTest);
    rpt << "\n" << "hasGridBeenActivated = " << G::s(hasGridBeenActivated);
    rpt << "\n" << "isLeftMouseBtnPressed = " << G::s(isLeftMouseBtnPressed);
    rpt << "\n" << "isMouseDrag = " << G::s(isMouseDrag);
//    rpt << "\n" << "timeToQuit = " << G::s(timeToQuit);
    rpt << "\n" << "sortMenuUpdateToMatchTable = " << G::s(sortMenuUpdateToMatchTable);
    rpt << "\n" << "imageCacheFilePath = " << G::s(imageCacheFilePath);
    rpt << "\n" << "newScrollSignal = " << G::s(newScrollSignal);
    rpt << "\n" << "prevCentralView = " << G::s(prevCentralView);
    rpt << "\n" << "mouseOverFolder = " << G::s(mouseOverFolder);
    rpt << "\n" << "rating = " << G::s(rating);
    rpt << "\n" << "colorClass = " << G::s(colorClass);
    rpt << "\n" << "isPick = " << G::s(isPick);

    rpt << "\n\n" ;
    return reportString;
}

void MW::diagnosticsMain() {diagnosticsReport(this->diagnostics());}
void MW::diagnosticsGridView() {diagnosticsReport(gridView->diagnostics());}
void MW::diagnosticsThumbView() {diagnosticsReport(thumbView->diagnostics());}
void MW::diagnosticsImageView() {diagnosticsReport(imageView->diagnostics());}
void MW::diagnosticsInfoView() {}
void MW::diagnosticsTableView() {}
void MW::diagnosticsCompareView() {}
void MW::diagnosticsMetadata()
{
    dm->imMetadata(dm->currentFilePath, true);
    diagnosticsReport(metadata->diagnostics(dm->currentFilePath));
}
void MW::diagnosticsXMP() {}
void MW::diagnosticsMetadataCache() {}
void MW::diagnosticsImageCache() {diagnosticsReport(imageCacheThread->diagnostics());}
void MW::diagnosticsDataModel() {diagnosticsReport(dm->diagnostics());}
void MW::diagnosticsErrors() {diagnosticsReport(dm->diagnosticsErrors());}
void MW::diagnosticsEmbellish() {diagnosticsReport(embelProperties->diagnostics());}
void MW::diagnosticsFilters() {}
void MW::diagnosticsFileTree() {}
void MW::diagnosticsBookmarks() {}
void MW::diagnosticsPixmap() {}
void MW::diagnosticsThumb() {}
void MW::diagnosticsIngest() {}
void MW::diagnosticsZoom() {}

void MW::diagnosticsReport(QString reportString)
{
    if (G::isLogger) G::log(__FUNCTION__);
    QDialog *dlg = new QDialog;
    dlg->setStyleSheet(G::css);
    Ui::metadataReporttDlg md;
    md.setupUi(dlg);
    md.textBrowser->setStyleSheet(G::css);
    md.textBrowser->setText(reportString);
    md.textBrowser->setWordWrapMode(QTextOption::NoWrap);
    QFontMetrics metrics(md.textBrowser->font());
    md.textBrowser->setTabStopDistance(3 * metrics.horizontalAdvance(' '));
    dlg->show();
//    std::cout << reportString.toStdString() << std::flush;
}

void MW::errorReport()
{
    if (G::isLogger) G::log(__FUNCTION__);
    QDialog *dlg = new QDialog;
    dlg->setStyleSheet(G::css);
    Ui::metadataReporttDlg md;
    md.setupUi(dlg);
    dlg->setWindowTitle("Winnow Error Log");
    md.textBrowser->setStyleSheet(G::css);
    md.textBrowser->setWordWrapMode(QTextOption::NoWrap);
    G::errlogFile.seek(0);
    QString errString(G::errlogFile.readAll());
//    qDebug() << __FUNCTION__ << G::errlogFile.isOpen() << errString;
    md.textBrowser->setText(errString);
    dlg->show();
}

void MW::about()
{
    if (G::isLogger) G::log(__FUNCTION__);
    QString qtVersion = QT_VERSION_STR;
    qtVersion.prepend("Qt: ");
    aboutDlg = new AboutDlg(this, version, qtVersion);
    aboutDlg->exec();
}

void MW::externalAppManager()
{
/*
   This function opens a dialog that allows the user to add and delete external executables
   that can be passed image files. externalApps is a QList that holds string pairs: the
   program name and the path to the external program executable. This list is passed as a
   reference to the appdlg, which modifies it and then after the dialog is closed the
   appActions are rebuilt.
*/
    if (G::isLogger) G::log(__FUNCTION__);
    Appdlg *appdlg = new Appdlg(externalApps, this);

    if(appdlg->exec()) {
        /*
        Menus cannot be added/deleted at runtime in MacOS so 10 menu items are created
        in the MW constructor and then edited here based on changes made in appDlg.
        */
        setting->beginGroup("ExternalApps");
        setting->remove("");
        for (int i = 0; i < 10; ++i) {
            if (i < externalApps.length()) {
                QString shortcut = "Alt+" + xAppShortcut[i];
                appActions.at(i)->setShortcut(QKeySequence(shortcut));
                appActions.at(i)->setText(externalApps.at(i).name);
                appActions.at(i)->setVisible(true);
                // save to settings
                QString sortPrefix = xAppShortcut[i];
                if (sortPrefix == "0") sortPrefix = "X";
                setting->setValue(sortPrefix + externalApps.at(i).name, externalApps.at(i).path);
            }
            else {
                appActions.at(i)->setVisible(false);
                appActions.at(i)->setText("");
            }
        }
        setting->endGroup();
    }
}

void MW::cleanupSender()
{
/*
   When the process responsible for running the external program is finished a signal is
   received here to delete the process.
*/
    if (G::isLogger) G::log(__FUNCTION__);
    delete QObject::sender();
}

void MW::externalAppError(QProcess::ProcessError /*err*/)
{
    if (G::isLogger) G::log(__FUNCTION__);
    QMessageBox msgBox;
    msgBox.critical(this, tr("Error"), tr("Failed to start external application."));
}

QString MW::enquote(QString &s)
{
    // rgh used Utilities ??
    return QChar('\"') + s + QChar('\"');
}

void MW::runExternalApp()
{
    if (G::isLogger) G::log(__FUNCTION__);
    // this works:
//    QDesktopServices::openUrl(QUrl("file:///Users/roryhill/Pictures/4K/2017-01-25_0030-Edit.jpg"));
//    return;

    QString appPath = "";
    QString appName = (static_cast<QAction*>(sender()))->text();
    for(int i = 0; i < externalApps.length(); ++i) {
        if(externalApps.at(i).name == appName) {
            appPath = externalApps.at(i).path;
            break;
        }
    }
    if(appPath == "") return;       // add err handling
//    QFileInfo appInfo = appPath;  // qt6.2
    QFileInfo appInfo;              // qt6.2
    appInfo.setFile(appPath);       // qt6.2
    QString appExecutable = appInfo.fileName();

//    app = externalApps[((QAction*) sender())->text()];
    QModelIndexList selectedIdxList = selectionModel->selectedRows();

    /*
    app = "/Applications/Adobe Photoshop CC 2018/Adobe Photoshop CC 2018.app/Contents/MacOS/Adobe Photoshop CC 2018";
    app = "/Applications/Adobe Photoshop CS6/Adobe Photoshop CS6.app/Contents/MacOS/Adobe Photoshop CS6";
    QString x = "/Applications/Adobe Photoshop CS6/Adobe Photoshop CS6.app";
    app = "\"/Applications/Adobe Photoshop CS6/Adobe Photoshop CS6.app\"";
    std::cout << appPath.toStdString() << std::endl << std::flush;
    // */

    int nFiles = selectedIdxList.size();

    if (nFiles < 1) {
        G::popUp->showPopup("No images have been selected", 2000);
        return;
    }

    if (nFiles > 5) {
        QMessageBox msgBox;
        int msgBoxWidth = 300;
        msgBox.setText(QString::number(nFiles) + " files have been selected.");
        msgBox.setInformativeText("Do you want continue?");
        msgBox.setStandardButtons(QMessageBox::Yes | QMessageBox::Cancel);
        msgBox.setDefaultButton(QMessageBox::Yes);
        msgBox.setIcon(QMessageBox::Warning);
        QString s = "QWidget{font-size: 12px; background-color: rgb(85,85,85); color: rgb(229,229,229);}"
                    "QPushButton:default {background-color: rgb(68,95,118);}";
        msgBox.setStyleSheet(s);
        QSpacerItem* horizontalSpacer = new QSpacerItem(msgBoxWidth, 0, QSizePolicy::Minimum, QSizePolicy::Expanding);
        QGridLayout* layout = static_cast<QGridLayout*>(msgBox.layout());
        layout->addItem(horizontalSpacer, layout->rowCount(), 0, 1, layout->columnCount());
        int ret = msgBox.exec();
        if (ret == QMessageBox::Cancel) return;
    }

    QStringList arguments;
    if (!getSelection(arguments)) return;
    QString folderPath;
//    QFileInfo fInfo = arguments.at(0);    // qt6.2
    QFileInfo fInfo;                        // qt6.2
    fInfo.setFile(arguments.at(0));         // qt6.2
    folderPath = fInfo.dir().absolutePath() + "/";
    /*
    for (int tn = 0; tn < nFiles ; ++tn) {
        QString fPath = selectedIdxList[tn].data(G::PathRole).toString();
        QFileInfo fInfo = fPath;
        folderPath = fInfo.dir().absolutePath() + "/";
        QString fileName = fInfo.fileName();

        // Update arguments
        arguments << fPath;

        // write sidecar in case external app can read the metadata
        QString destBaseName = fInfo.baseName();
        QString suffix = fInfo.suffix().toLower();
        QString destinationPath = fPath;

        // Sidecar being overwritten if it already exists!!  Cancel for now.

        // buffer to hold file with edited xmp data
        QByteArray buffer;

        if (metadata->writeMetadata(fPath, dm->getMetadata(fPath), buffer)
        && metadata->sidecarFormats.contains(suffix)) {

            if (metadata->internalXmpFormats.contains(suffix)) {
                // write xmp data into image file
                QFile newFile(destinationPath);
                newFile.open(QIODevice::WriteOnly);
                newFile.write(buffer);
                newFile.close();
            }
            else {
                // write the sidecar xmp file
                QFile sidecarFile(folderPath + destBaseName + ".xmp");
                sidecarFile.open(QIODevice::WriteOnly);
                sidecarFile.write(buffer);
                sidecarFile.close();
            }
        }
    }
    //    */

    #ifdef Q_OS_WIN
    arguments.replaceInStrings("/", "\\");
    #endif

    QProcess *process = new QProcess();
    connect(process, SIGNAL(finished(int, QProcess::ExitStatus)),
            this, SLOT(cleanupSender()));
    connect(process, SIGNAL(error(QProcess::ProcessError)),
            this, SLOT(externalAppError(QProcess::ProcessError)));

    process->setArguments(arguments);
    process->setProgram(appPath);
    process->setWorkingDirectory(folderPath);
    process->start();
    /*
    //this works in terminal"
    // open "/Users/roryhill/Pictures/4K/2017-01-25_0030-Edit.jpg" -a "Adobe Photoshop CS6"
    //*/
}

void MW::allPreferences()
{
    if (G::isLogger) G::log(__FUNCTION__);
    preferences();
}

void MW::infoViewPreferences()
{
    if (G::isLogger) G::log(__FUNCTION__);
    preferences("MetadataPanelHeader");
}

void MW::cachePreferences()
{
    if (G::isLogger) G::log(__FUNCTION__);
    preferences("CacheHeader");
}

void MW::preferences(QString text)
{
/*

*/
    if (G::isLogger) G::log(__FUNCTION__);
    pref = new Preferences(this);
    if (text != "") pref->expandBranch(text);
    preferencesDlg = new PreferencesDlg(this, isSoloPrefDlg, pref, css);
    preferencesDlg->exec();
    delete pref;
    delete preferencesDlg;

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
    if (G::isLogger) G::log(__FUNCTION__);
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
    if (G::isLogger) G::log(__FUNCTION__);
    G::fontSize = QString::number(fontPixelSize);
    widgetCSS.fontSize = fontPixelSize;
//    emit widgetCSS.fontSizeChange(fontPixelSize);
    css = widgetCSS.css();
    G::css = css;
    setStyleSheet(css);

    if (useInfoView) infoView->refreshLayout();                   // triggers sizehint!
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
    if (G::isLogger) G::log(__FUNCTION__);
    G::backgroundShade = shade;
    int a = shade + 5;
    int b = shade - 15;
//    css = "QWidget {font-size: " + G::fontSize + "px;}" + cssBase;
    widgetCSS.widgetBackgroundColor = QColor(shade,shade,shade);
    css = widgetCSS.css();
    G::css = css;
    this->setStyleSheet(css);

    if (useInfoView) {
        infoView->updateInfo(currentRow);                           // triggers sizehint!
        infoView->verticalScrollBar()->setStyleSheet(css);          // triggers sizehint!
    }
    bookmarks->setStyleSheet(css);
    bookmarks->verticalScrollBar()->setStyleSheet(css);
    fsTree->setStyleSheet(css);
    fsTree->verticalScrollBar()->setStyleSheet(css);
    filters->setStyleSheet(css);
    filters->verticalScrollBar()->setStyleSheet(css);
    filters->setCategoryBackground(a, b);
//    if (useInfoView) infoView->setStyleSheet(css);
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
    if (G::isLogger) G::log(__FUNCTION__);
    /* imageView->infoOverlayFontSize already defined in preferences - just call
       so can redraw  */
    imageView->setShootingInfo(imageView->shootingInfo);
}

void MW::setClassificationBadgeImageDiam(int d)
{
    if (G::isLogger) G::log(__FUNCTION__);
    classificationBadgeInImageDiameter = d;
    imageView->setClassificationBadgeImageDiam(d);
}

void MW::setClassificationBadgeThumbDiam(int d)
{
    if (G::isLogger) G::log(__FUNCTION__);
    classificationBadgeInThumbDiameter = d;
    thumbView->badgeSize = d;
    gridView->badgeSize = d;
    thumbView->setThumbParameters();
    gridView->setThumbParameters();
}

void MW::setPrefPage(int page)
{
    if (G::isLogger) G::log(__FUNCTION__);
    lastPrefPage = page;
}

void MW::updateDisplayResolution()
{
    if (G::isLogger) G::log(__FUNCTION__);
    QString monitorScale = QString::number(G::actDevicePixelRatio * 100) + "%";
    QString dimensions = QString::number(G::displayPhysicalHorizontalPixels) + "x"
            + QString::number(G::displayPhysicalVerticalPixels)
            + " @ " + monitorScale;
    QString toolTip = dimensions + " (Monitor is scaled to " + monitorScale + ")";
    if (!useInfoView) return;
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
    if (G::isLogger) G::log(__FUNCTION__);
    bool monitorChanged = false;
    bool devicePixelRatioChanged = false;
    // ignore until show event
    if (!isVisible()) return;

    // Screen info
    QPoint loc = centralWidget->window()->geometry().center();
    /*
    if (loc == prevScreenLoc) return;
    prevScreenLoc = loc;
//    */
    QScreen *screen = qApp->screenAt(loc);
    if (screen == nullptr) return;
    monitorChanged = screen->name() != prevScreenName;
    /*
    qDebug() << __FUNCTION__ << "1"
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
    qDebug() << __FUNCTION__ << "2"
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
    G::displayVirtualHorizontalPixels = screen->geometry().width();
    G::displayVirtualVerticalPixels = screen->geometry().height();
    G::displayPhysicalHorizontalPixels = screen->geometry().width() * G::actDevicePixelRatio;
    G::displayPhysicalVerticalPixels = screen->geometry().height() * G::actDevicePixelRatio;

    /*
    double physicalWidth = screen->physicalSize().width();
    double dpmm = G::displayPhysicalHorizontalPixels * 1.0 / physicalWidth ;
    qDebug() << __FUNCTION__
             << "G::actDevicePixelRatio =" << G::actDevicePixelRatio
             << "screen->actDevicePixelRatio() =" << screen->devicePixelRatio()
             << "VirtualHorPixels =" << G::displayVirtualHorizontalPixels
             << "PhysicalHorPixels =" << G::displayPhysicalHorizontalPixels
//             << "screen->physicalSize() =" << screen->physicalSize()
             << "px per mm =" << dpmm
                ;
//    */

    if (devicePixelRatioChanged) {
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
    qDebug() << __FUNCTION__ << "MONITOR HAS CHANGED"
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
//        qDebug() << __FUNCTION__ << "RESIZE TO:" << w << h;
        resize(w, h);
    }

    // color manage for new monitor
    #ifdef Q_OS_WIN
    if (G::winScreenHash.contains(screen->name()))
        G::winOutProfilePath = "C:/Windows/System32/spool/drivers/color/" +
            G::winScreenHash[screen->name()].profile;
    ICC::setOutProfile();
    #endif
    #ifdef Q_OS_MAC
    G::winOutProfilePath = Mac::getDisplayProfileURL();
//    G::winOutProfilePath = "/users/roryhill/Library/ColorSync/Profiles/MacBook LCD_06-02-2021.icc";
    ICC::setOutProfile();
    #endif

    cachePreviewWidth = G::displayPhysicalHorizontalPixels;
    cachePreviewHeight = G::displayPhysicalVerticalPixels;
    /*
    qDebug() << __FUNCTION__
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

double MW::macActualDevicePixelRatio(QPoint loc, QScreen *screen)
{
/*
    Apple makes it hard to get the display native pixel resolution, which is necessary
    to determine the true device pixel ratio to ensure images at 100% are 1:1 image and
    display pixels.
*/
    if (G::isLogger) G::log(__FUNCTION__);
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
    if (G::isLogger) G::log(__FUNCTION__);
    fullScreenAction->setChecked(false);
    toggleFullScreen();
}

void MW::toggleFullScreen()
{
    if (G::isLogger) G::log(__FUNCTION__);
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
        if (useInfoView) {
            metadataDockVisibleAction->setChecked(fullScreenDocks.isMetadata);
            metadataDock->setVisible(fullScreenDocks.isMetadata);
        }
        embelDockVisibleAction->setChecked(fullScreenDocks.isMetadata);
        embelDock->setVisible(fullScreenDocks.isMetadata);
        thumbDockVisibleAction->setChecked(fullScreenDocks.isThumbs);
        thumbDock->setVisible(fullScreenDocks.isThumbs);
//        thumbView->selectThumb(currentRow);
//        gridView->selectThumb(currentRow);
        menuBarVisibleAction->setChecked(false);
        setMenuBarVisibility();
        statusBarVisibleAction->setChecked(fullScreenDocks.isStatusBar);
        setStatusBarVisibility();
//        fileSelectionChange(currentSfIdx, currentSfIdx);
    }
    else
    {
        isNormalScreen = true;
        showNormal();
        invokeWorkspace(ws);
//        fileSelectionChange(currentSfIdx, currentSfIdx);
//        thumbView->selectThumb(currentRow);
//        gridView->selectThumb(currentRow);
//        imageView->setCursorHiding(false);
    }
}

void MW::selectAllThumbs()
{
    if (G::isLogger) G::log(__FUNCTION__);
    thumbView->selectAll();
}

void MW::toggleZoomDlg()
{
/*
    This function provides a dialog to change scale and to set the toggleZoom value, which is
    the amount of zoom to toggle with zoomToFit scale. The user can zoom to 100% (for example)
    with a click of the mouse, and with another click, return to the zoomToFit scale. Here the
    user can set the amount of zoom when toggled.

    The dialog is non-modal and floats at the bottom of the central widget. Adjustments are
    made when the main window resizes or is moved or when a different workspace is invoked.
    This only applies when a mode that can be zoomed is visible, so table and grid are not
    applicable.

    When the zoom dialog is created, zoomDlg->show makes the dialog visible and also gives it
    the focus, but the design is for the zoomDlg to only have focus when a mouseover occurs.
    The focus is set to MW when the zoomDlg leaveEvent fires and after zoomDlg->show.

    NOTE: the dialog window flag is Qt::WindowStaysOnTopHint. When The app focus changes to
    another app, the zoom dialog is hidden so it does not float on top of other apps (this is
    triggered in the slot MW::appStateChange). The windows flag Qt::WindowStaysOnTopHint is
    not changed as this automatically hides the window - it is easier to just hide it. The
    prior state of ZoomDlg is held in isZoomDlgVisible.

    When the zoom is changed this is signalled to ImageView and CompareImages, which in turn
    make the scale changes to the image. Conversely, changes in scale originating from
    toggleZoom mouse clicking in ImageView or CompareView, or scale changes originating from
    the zoomInAction and zoomOutAction are signaled and updated here. This can cause a
    circular message, which is prevented by variance checking. If the zoom factor has not
    changed more than can be accounted for in int/qreal conversions then the signal is not
    propagated.

*/
    if (G::isLogger) G::log(__FUNCTION__);
    qDebug() << __FUNCTION__ /*<< isZoomDlgVisible*/;
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
    if (G::isLogger) G::log(__FUNCTION__);
    if (asLoupeAction) imageView->zoomIn();
    if (asCompareAction) compareImages->zoomIn();
    // if thumbView visible and zoom change in imageView then may need to redo the zoomFrame
    QModelIndex idx = thumbView->indexAt(thumbView->mapFromGlobal(QCursor::pos()));
    if (idx.isValid()) {
        thumbView->zoomCursor(idx, /*forceUpdate=*/true);
    }
}

void MW::zoomOut()
{
    if (G::isLogger) G::log(__FUNCTION__);
    if (asLoupeAction) imageView->zoomOut();
    if (asCompareAction) compareImages->zoomOut();
    // if thumbView visible and zoom change in imageView then may need to redo the zoomFrame
    QModelIndex idx = thumbView->indexAt(thumbView->mapFromGlobal(QCursor::pos()));
    if (idx.isValid()) {
        thumbView->zoomCursor(idx, /*forceUpdate=*/true);
    }
}

void MW::zoomToFit()
{
    if (G::isLogger) G::log(__FUNCTION__);
    if (asLoupeAction) imageView->zoomToFit();
    if (asCompareAction) compareImages->zoomToFit();
}

void MW::zoomToggle()
{
    if (G::isLogger) G::log(__FUNCTION__);
    if (asLoupeAction) imageView->zoomToggle();
    if (asCompareAction) compareImages->zoomToggle();
}

void MW::rotateLeft()
{
    if (G::isLogger) G::log(__FUNCTION__);
    setRotation(270);
//    imageView->refresh();
}

void MW::rotateRight()
{
    if (G::isLogger) G::log(__FUNCTION__);
    setRotation(90);
//    imageView->refresh();
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
    if (G::isLogger) G::log(__FUNCTION__);
    // rotate current loupe view image
    imageView->rotateImage(degrees);

    // iterate selection
    QModelIndexList selection = selectionModel->selectedRows();
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
}

bool MW::isValidPath(QString &path)
{
    if (G::isLogger) G::log(__FUNCTION__);
    QDir checkPath(path);
    if (!checkPath.exists() || !checkPath.isReadable()) {
        return false;
    }
    return true;
}

void MW::removeBookmark()
{
    if (G::isLogger) G::log(__FUNCTION__);
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
    if (G::isLogger) G::log(__FUNCTION__); 
    bookmarks->count();
}

void MW::openLog()
{
    if (G::isLogger) G::log(__FUNCTION__);
    QString path = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/Log";
    qDebug() << __FUNCTION__ << path;
    QDir dir(path);
    if (!dir.exists()) dir.mkdir(path);
    if (G::logFile.isOpen()) G::logFile.close();
    QString fPath = path + "/WinnowLog.txt";
    G::logFile.setFileName(fPath);
    // erase content if over one week since last modified
    QFileInfo info(G::logFile);
    QDateTime lastModified = info.lastModified();
    QDateTime oneWeekAgo = QDateTime::currentDateTime().addDays(-7);
    if (lastModified < oneWeekAgo) clearLog();
    if (G::logFile.open(QIODevice::ReadWrite)) {
        G::logFile.readAll();
    }
}

void MW::closeLog()
{
    if (G::isLogger) G::log(__FUNCTION__);
    if (G::logFile.isOpen()) G::logFile.close();
}

void MW::clearLog()
{
    if (G::isLogger) G::log(__FUNCTION__);
    if (!G::logFile.isOpen()) openLog();
    G::logFile.resize(0);
}

void MW::openErrLog()
{
    if (G::isLogger) G::log(__FUNCTION__);
    QString path = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/Log";
    QDir dir(path);
    if (!dir.exists()) dir.mkdir(path);
    if (G::errlogFile.isOpen()) G::errlogFile.close();
    QString fPath = path + "/WinnowErrorLog.txt";
//    qDebug() << __FUNCTION__ << fPath;
    G::errlogFile.setFileName(fPath);
    // erase content if over one week since last modified
    QFileInfo info(G::errlogFile);
    QDateTime lastModified = info.lastModified();
    QDateTime oneWeekAgo = QDateTime::currentDateTime().addDays(-7);
    if (lastModified < oneWeekAgo) clearErrLog();
    if (G::errlogFile.open(QIODevice::ReadWrite)) {
//        QString errString(G::errlogFile.readAll());
//        qDebug() << __FUNCTION__ << errString;
    }
}

void MW::closeErrLog()
{
    if (G::isLogger) G::log(__FUNCTION__);
    if (G::errlogFile.isOpen()) G::errlogFile.close();
}

void MW::clearErrLog()
{
    if (G::isLogger) G::log(__FUNCTION__);
    if (!G::errlogFile.isOpen()) openLog();
    G::errlogFile.resize(0);
}

/*********************************************************************************************
 * READ/WRITE PREFERENCES
*/

void MW::writeSettings()
{
/*
    This function is called when exiting the application.

    Using QSetting, write the persistent application settings so they can be re-established
    when the application is re-opened.
*/
    if (G::isLogger) G::log(__FUNCTION__);

    // general
    setting->setValue("lastPrefPage", lastPrefPage);
//    setting->setValue("mouseClickScroll", mouseClickScroll);
    setting->setValue("toggleZoomValue", imageView->toggleZoom);
    setting->setValue("limitFit100Pct", imageView->limitFit100Pct);
    setting->setValue("sortColumn", sortColumn);
    setting->setValue("sortReverse", sortReverseAction->isChecked());
    setting->setValue("autoAdvance", autoAdvance);
    setting->setValue("turnOffEmbellish", turnOffEmbellish);
    setting->setValue("deleteWarning", deleteWarning);
    setting->setValue("modifySourceFiles", G::modifySourceFiles);
    setting->setValue("useSidecar", G::useSidecar);
    setting->setValue("embedTifThumb", G::embedTifThumb);
    setting->setValue("isLogger", G::isLogger);
    setting->setValue("isErrorLogger", G::isErrorLogger);

    // datamodel
    setting->setValue("maxIconSize", G::maxIconSize);

    // appearance
    setting->setValue("backgroundShade", G::backgroundShade);
    setting->setValue("fontSize", G::fontSize);
    setting->setValue("classificationBadgeInImageDiameter", classificationBadgeInImageDiameter);
    setting->setValue("classificationBadgeInThumbDiameter", thumbView->badgeSize);
    setting->setValue("infoOverlayFontSize", imageView->infoOverlayFontSize);

    // files
    setting->setValue("colorManage", G::colorManage);
    setting->setValue("rememberLastDir", rememberLastDir);
    setting->setValue("checkIfUpdate", checkIfUpdate);
    setting->setValue("lastDir", currentViewDirPath);
    setting->setValue("includeSubfolders", subFoldersAction->isChecked());
    setting->setValue("combineRawJpg", combineRawJpg);

    /* ingest (moved to MW::ingest)
    */

    // thumbs
    setting->setValue("thumbWidth", thumbView->iconWidth);
    setting->setValue("thumbHeight", thumbView->iconHeight);
    setting->setValue("labelFontSize", thumbView->labelFontSize);
    setting->setValue("showThumbLabels", thumbView->showIconLabels);
    setting->setValue("showZoomFrame", thumbView->showZoomFrame);

    // grid
    setting->setValue("thumbWidthGrid", gridView->iconWidth);
    setting->setValue("thumbHeightGrid", gridView->iconHeight);
    setting->setValue("labelFontSizeGrid", gridView->labelFontSize);
    setting->setValue("showThumbLabelsGrid", gridView->showIconLabels);

    setting->setValue("thumbsPerPage", metadataCacheThread->visibleIcons);

    // slideshow
    setting->setValue("slideShowDelay", slideShowDelay);
    setting->setValue("isSlideShowRandom", isSlideShowRandom);
    setting->setValue("isSlideShowWrap", isSlideShowWrap);

    // metadata and icon cache
    setting->setValue("cacheAllMetadata", metadataCacheThread->cacheAllMetadata);
    setting->setValue("cacheAllIcons", metadataCacheThread->cacheAllIcons);
    setting->setValue("metadataChunkSize", metadataCacheThread->metadataChunkSize);

    // image cache
    setting->setValue("cacheSizeMethod", cacheSizeMethod);
    setting->setValue("cacheMinSize", cacheMinSize);
    setting->setValue("isShowCacheStatus", isShowCacheProgressBar);
    setting->setValue("cacheDelay", cacheDelay);
    setting->setValue("cacheStatusWidth", progressWidth);
    setting->setValue("cacheWtAhead", cacheWtAhead);
    setting->setValue("isCachePreview", isCachePreview);
    setting->setValue("cachePreviewWidth", cachePreviewWidth);
    setting->setValue("cachePreviewHeight", cachePreviewHeight);

    // full screen
    setting->setValue("isFullScreenFolders", fullScreenDocks.isFolders);
    setting->setValue("isFullScreenFavs", fullScreenDocks.isFavs);
    setting->setValue("isFullScreenFilters", fullScreenDocks.isFilters);
    setting->setValue("isFullScreenMetadata", fullScreenDocks.isMetadata);
    setting->setValue("isFullScreenThumbs", fullScreenDocks.isThumbs);
    setting->setValue("isFullScreenStatusBar", fullScreenDocks.isStatusBar);

    // state
    setting->setValue("Geometry", saveGeometry());
    setting->setValue("WindowState", saveState());
    setting->setValue("isFullScreen", isFullScreen());

    setting->setValue("isRatingBadgeVisible", ratingBadgeVisibleAction->isChecked());
    setting->setValue("isImageInfoVisible", infoVisibleAction->isChecked());
    setting->setValue("isLoupeDisplay", asLoupeAction->isChecked());
    setting->setValue("isGridDisplay", asGridAction->isChecked());
    setting->setValue("isTableDisplay", asTableAction->isChecked());
    setting->setValue("isCompareDisplay", asCompareAction->isChecked());

    setting->setValue("wasThumbDockVisible", wasThumbDockVisible);

    setting->setValue("isMenuBarVisible", menuBarVisibleAction->isChecked());
    setting->setValue("isStatusBarVisible", statusBarVisibleAction->isChecked());
    setting->setValue("isFolderDockVisible", folderDockVisibleAction->isChecked());
    setting->setValue("isFavDockVisible", favDockVisibleAction->isChecked());
    setting->setValue("isFilterDockVisible", filterDockVisibleAction->isChecked());
    setting->setValue("isMetadataDockVisible", metadataDockVisibleAction->isChecked());
    setting->setValue("isEmbelDockVisible", embelDockVisibleAction->isChecked());
    setting->setValue("isThumbDockVisible", thumbDockVisibleAction->isChecked());

    /* Property Editor */
    setting->setValue("isSoloPrefDlg", isSoloPrefDlg);

     /* FolderDock floating info */
    setting->beginGroup(("FolderDock"));
    setting->setValue("screen", folderDock->dw.screen);
    setting->setValue("pos", folderDock->dw.pos);
    setting->setValue("size", folderDock->dw.size);
    setting->setValue("devicePixelRatio", folderDock->dw.devicePixelRatio);
    setting->endGroup();

    /* FavDock floating info */
    setting->beginGroup(("FavDock"));
    setting->setValue("screen", favDock->dw.screen);
    setting->setValue("pos", favDock->dw.pos);
    setting->setValue("size", favDock->dw.size);
    setting->setValue("devicePixelRatio", favDock->dw.devicePixelRatio);
    setting->endGroup();

    /* MetadataDock floating info */
//    if (useInfoView) {
        setting->beginGroup(("MetadataDock"));
        setting->setValue("screen", metadataDock->dw.screen);
        setting->setValue("pos", metadataDock->dw.pos);
        setting->setValue("size", metadataDock->dw.size);
        setting->setValue("devicePixelRatio", metadataDock->dw.devicePixelRatio);
        setting->endGroup();
//    }

    /* EmbelDock floating info */
    setting->beginGroup(("EmbelDock"));
    setting->setValue("screen", embelDock->dw.screen);
    setting->setValue("pos", embelDock->dw.pos);
    setting->setValue("size", embelDock->dw.size);
    setting->setValue("devicePixelRatio", embelDock->dw.devicePixelRatio);
    setting->endGroup();

    /* FilterDock floating info */
    setting->beginGroup(("FilterDock"));
    setting->setValue("screen", filterDock->dw.screen);
    setting->setValue("pos", filterDock->dw.pos);
    setting->setValue("size", filterDock->dw.size);
    setting->setValue("devicePixelRatio", filterDock->dw.devicePixelRatio);
    setting->endGroup();

    /* ThumbDock floating info */
    setting->beginGroup(("ThumbDockFloat"));
    setting->setValue("screen", thumbDock->dw.screen);
    setting->setValue("pos", thumbDock->dw.pos);
    setting->setValue("size", thumbDock->dw.size);
    setting->setValue("devicePixelRatio", thumbDock->dw.devicePixelRatio);
    setting->endGroup();

    /* InfoView okToShow fields */
    if (useInfoView) {
        setting->beginGroup("InfoFields");
        setting->remove("");
        QStandardItemModel *k = infoView->ok;
        for(int row = 0; row < k->rowCount(); row++) {
            QModelIndex parentIdx = k->index(row, 0);
            QString field = k->index(row, 0).data().toString();
            bool showField = k->index(row, 2).data().toBool();
            setting->setValue(field, showField);
            for (int childRow = 0; childRow < k->rowCount(parentIdx); childRow++) {
                QString field = k->index(childRow, 0, parentIdx).data().toString();
                bool showField = k->index(childRow, 2, parentIdx).data().toBool();
                setting->setValue(field, showField);
            }
        }
        setting->endGroup();
    }

    /* TableView okToShow fields */
    setting->beginGroup("TableFields");
    setting->remove("");
    for(int row = 0; row < tableView->ok->rowCount(); row++) {
        QString field = tableView->ok->index(row, 0).data().toString();
        bool showField = tableView->ok->index(row, 1).data().toBool();
        setting->setValue(field, showField);
    }
    setting->endGroup();

    /* Tokens used for ingest operations */
    setting->beginGroup("PathTokens");
    setting->remove("");
    // save path templates
    QMapIterator<QString, QString> pathIter(pathTemplates);
    while (pathIter.hasNext()) {
        pathIter.next();
        setting->setValue(pathIter.key(), pathIter.value());
    }
    setting->endGroup();

    // save filename templates
    setting->beginGroup("FileNameTokens");
    setting->remove("");
    QMapIterator<QString, QString> filenameIter(filenameTemplates);
    while (filenameIter.hasNext()) {
        filenameIter.next();
        setting->setValue(filenameIter.key(), filenameIter.value());
    }
    setting->endGroup();

    /* Token templates used for shooting information shown in ImageView */
    setting->setValue("loupeInfoTemplate", infoString->loupeInfoTemplate);
    setting->beginGroup("InfoTemplates");
        setting->remove("");
        QMapIterator<QString, QString> infoIter(infoString->infoTemplates);
        while (infoIter.hasNext()) {
            infoIter.next();
            setting->setValue(infoIter.key(), infoIter.value());
        }
    setting->endGroup();

    /* External apps */
//    setting->beginGroup("ExternalApps");
//    setting->remove("");
//    for (int i = 0; i < externalApps.length(); ++i) {
//        QString sortPrefix = xAppShortcut[i];
//        if (sortPrefix == "0") sortPrefix = "X";
//        setting->setValue(sortPrefix + externalApps.at(i).name, externalApps.at(i).path);
//    }
//    setting->endGroup();

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
    QString leadingZero;
    for (int i = 0; i < recentFolders->count(); i++) {
        i < 9 ? leadingZero = "0" : leadingZero = "";
        setting->setValue("recentFolder" + leadingZero + QString::number(i+1),
                          recentFolders->at(i));
    }
    setting->endGroup();

    // moved to ingest()
//    /* save ingest history folders */
//    setting->beginGroup("IngestHistoryFolders");
//    setting->remove("");
//    for (int i = 0; i < ingestHistoryFolders->count(); i++) {
//        setting->setValue("ingestHistoryFolder" + QString::number(i+1),
//                          ingestHistoryFolders->at(i));
//    }
//    setting->endGroup();

//    /* save ingest description completer list */
//    setting->beginGroup("IngestDescriptionCompleter");
//    for (const auto& i : ingestDescriptionCompleter) {
//        setting->setValue(i, "");
//    }
//    setting->endGroup();

    saveWorkspaces();
}

bool MW::loadSettings()
{
/*
    Persistant settings from QSettings fall into two categories:
    1.  Action settings
    2.  Preferences

    Not all settings are loaded here in this function, since this function is called before the
    actions and many object, such as imageView, are created.

    Preferences are located in the prefdlg class and updated here.
*/
    if (G::isLogger) G::log(__FUNCTION__);

    /* Default values for first time use (settings does not yet exist).  Apply defaults even
       when there are settings in case a new setting has been added to the application to make
       sure the default value is assigned for first time use of the new setting.
    */

    if (!isSettings || simulateJustInstalled) {
        // general
        combineRawJpg = true;
        prevMode = "Loupe";
        G::mode = "Loupe";
//        G::isLogger = false;
        G::isErrorLogger = false;

        // appearance
        G::backgroundShade = 50;
        G::fontSize = "12";
        infoOverlayFontSize = 24;
        classificationBadgeInImageDiameter = 32;
        classificationBadgeInThumbDiameter = 16;
        isRatingBadgeVisible = false;

        // datamodel
        G::maxIconSize = 256;
        sortColumn = G::NameColumn;

        // files
        G::colorManage = true;
        rememberLastDir = false;
        checkIfUpdate = true;
        lastDir = "";
        deleteWarning = true;
        G::modifySourceFiles = false;
        G::useSidecar = false;
        G::embedTifThumb = false;

        // ingest
        autoIngestFolderPath = false;
        autoEjectUsb = false;
        integrityCheck = false;
        isBackgroundIngest = false;
        isBackgroundIngestBeep = false;
        ingestIncludeXmpSidecar = true;
        gotoIngestFolder = false;
        backupIngest = false;
        pathTemplateSelected = 0;
        pathTemplateSelected2 = 0;
        filenameTemplateSelected = 0;

        // preferences
        isSoloPrefDlg = true;

        // slideshow
        slideShowDelay = 5;
        isSlideShowRandom = false;
        isSlideShowWrap = true;

        // filters

        // cache
        cacheSizeMethod = "Moderate";
        cacheMinSize = "  2 GB";
        cacheSizePercentOfAvailable = 50;
        cacheMaxMB = static_cast<int>(G::availableMemoryMB * 0.5);
        isShowCacheProgressBar = true;
        progressWidth = 200;
        progressWidthBeforeResizeWindow = progressWidth;
        cacheWtAhead = 7;
        isCachePreview = false;
        cachePreviewWidth = 2000;
        cachePreviewHeight = 1600;

        if (!isSettings || simulateJustInstalled) return true;
    }
    // end default settings

    // Get settings saved from last session

    // general
    sortColumn = setting->value("sortColumn").toInt();
    prevSortColumn = sortColumn;
    isReverseSort = setting->value("sortReverse").toBool();
    autoAdvance = setting->value("autoAdvance").toBool();
    turnOffEmbellish = setting->value("turnOffEmbellish").toBool();
//    if (setting->contains("isLogger"))
//        G::isLogger = setting->value("isLogger").toBool();
//    else
//        G::isLogger = false;
    if (setting->contains("isErrorLogger"))
        G::isErrorLogger = setting->value("isErrorLogger").toBool();
    else
        G::isLogger = false;
    if (setting->contains("deleteWarning"))
        deleteWarning = setting->value("deleteWarning").toBool();
    else
        deleteWarning = true;
    if (setting->contains("modifySourceFiles"))
        G::modifySourceFiles = setting->value("modifySourceFiles").toBool();
    else
        G::modifySourceFiles = false;
    if (setting->contains("useSidecar"))
        G::useSidecar = setting->value("useSidecar").toBool();
    else
        G::useSidecar = false;
    if (setting->contains("embedTifThumb"))
        G::embedTifThumb = setting->value("embedTifThumb").toBool();
    else
        G::embedTifThumb = false;
    lastFileIfCrash = setting->value("lastFileSelection").toString();

    // appearance
    if (setting->contains("backgroundShade")) {
        G::backgroundShade = setting->value("backgroundShade").toInt();
        if (G::backgroundShade < 20) G::backgroundShade = 50;
        G::backgroundColor = QColor(G::backgroundShade,G::backgroundShade,G::backgroundShade);
    }
    if (setting->contains("fontSize")) {
        G::fontSize = setting->value("fontSize").toString();
        if (G::fontSize == "") G::fontSize = "12";
    }

    // thumbdock
    if (setting->contains("wasThumbDockVisible")) wasThumbDockVisible = setting->value("wasThumbDockVisible").toBool();

    // load imageView->infoOverlayFontSize later as imageView has not been created yet
    if (setting->contains("classificationBadgeInImageDiameter")) classificationBadgeInImageDiameter = setting->value("classificationBadgeInImageDiameter").toInt();
    if (setting->contains("classificationBadgeInThumbDiameter")) classificationBadgeInThumbDiameter = setting->value("classificationBadgeInThumbDiameter").toInt();
    if (setting->contains("isRatingBadgeVisible")) isRatingBadgeVisible = setting->value("isRatingBadgeVisible").toBool();

    // datamodel
    if (setting->contains("maxIconSize")) G::maxIconSize = setting->value("maxIconSize").toInt();

    // files
    if (setting->contains("colorManage")) G::colorManage = setting->value("colorManage").toBool();
    if (setting->contains("rememberLastDir")) rememberLastDir = setting->value("rememberLastDir").toBool();
    if (setting->contains("checkIfUpdate")) checkIfUpdate = setting->value("checkIfUpdate").toBool();
    if (setting->contains("lastDir")) lastDir = setting->value("lastDir").toString();

    // ingest
    if (setting->contains("autoIngestFolderPath")) autoIngestFolderPath = setting->value("autoIngestFolderPath").toBool();
    if (setting->contains("autoEjectUSB")) autoEjectUsb = setting->value("autoEjectUSB").toBool();
    if (setting->contains("integrityCheck")) integrityCheck = setting->value("integrityCheck").toBool();
    if (setting->contains("isBackgroundIngest")) isBackgroundIngest = setting->value("isBackgroundIngest").toBool();
    if (setting->contains("isBackgroundIngestBeep")) isBackgroundIngestBeep = setting->value("isBackgroundIngestBeep").toBool();
    if (setting->contains("ingestIncludeXmpSidecar")) ingestIncludeXmpSidecar = setting->value("ingestIncludeXmpSidecar").toBool();
    if (setting->contains("backupIngest")) backupIngest = setting->value("backupIngest").toBool();
    if (setting->contains("gotoIngestFolder")) gotoIngestFolder = setting->value("gotoIngestFolder").toBool();
    if (setting->contains("ingestRootFolder")) ingestRootFolder = setting->value("ingestRootFolder").toString();
    if (setting->contains("ingestRootFolder2")) ingestRootFolder2 = setting->value("ingestRootFolder2").toString();
    if (setting->contains("pathTemplateSelected")) pathTemplateSelected = setting->value("pathTemplateSelected").toInt();
    if (setting->contains("pathTemplateSelected2")) pathTemplateSelected2 = setting->value("pathTemplateSelected2").toInt();
    if (setting->contains("filenameTemplateSelected")) filenameTemplateSelected = setting->value("filenameTemplateSelected").toInt();
    if (setting->contains("manualFolderPath")) manualFolderPath = setting->value("manualFolderPath").toString();
    if (setting->contains("manualFolderPath2")) manualFolderPath2 = setting->value("manualFolderPath2").toString();
    if (setting->contains("ingestCount")) G::ingestCount = setting->value("ingestCount").toInt();
    if (setting->contains("ingestLastSeqDate")) G::ingestLastSeqDate = setting->value("ingestLastSeqDate").toDate();
    if (G::ingestLastSeqDate != QDate::currentDate()) G::ingestCount = 0;

    // preferences
    if (setting->contains("isSoloPrefDlg")) isSoloPrefDlg = setting->value("isSoloPrefDlg").toBool();

    // slideshow
    if (setting->contains("slideShowDelay")) slideShowDelay = setting->value("slideShowDelay").toInt();
    if (setting->contains("isSlideShowRandom")) isSlideShowRandom = setting->value("isSlideShowRandom").toBool();
    if (setting->contains("isSlideShowWrap")) isSlideShowWrap = setting->value("isSlideShowWrap").toBool();

    // metadata and icon cache loaded when metadataCacheThread created in MW::createCaching

    // image cache
    if (setting->contains("cacheSizePercentOfAvailable")) cacheSizePercentOfAvailable = setting->value("cacheSizePercentOfAvailable").toInt();
    if (setting->contains("cacheSizeMethod")) setImageCacheSize(setting->value("cacheSizeMethod").toString());
    else setImageCacheSize("Moderate");
    if (setting->contains("cacheMinSize")) setImageCacheMinSize(setting->value("cacheMinSize").toString());
    else setImageCacheMinSize("4 GB");
        /*
//        cacheSizeMethod = setting->value("cacheSizeMethod").toString();
//        if (cacheSizeMethod == "Thrifty") cacheSizeBtn->setIcon(QIcon(":/images/icon16/thrifty.png"));
//        if (cacheSizeMethod == "Moderate") cacheSizeBtn->setIcon(QIcon(":/images/icon16/moderate.png"));
//        if (cacheSizeMethod == "Greedy") cacheSizeBtn->setIcon(QIcon(":/images/icon16/greedy.png"));
//        if (cacheSizeMethod == "Thrifty") cacheSizeMB = static_cast<int>(G::availableMemoryMB * 0.10);
//        if (cacheSizeMethod == "Moderate") cacheSizeMB = static_cast<int>(G::availableMemoryMB * 0.50);
//        if (cacheSizeMethod == "Greedy") cacheSizeMB = static_cast<int>(G::availableMemoryMB * 0.90);
//        if (cacheSizeMethod == "Percent of available")
//            cacheSizeMB = (static_cast<int>(G::availableMemoryMB) * cacheSizePercentOfAvailable) / 100;
//        if (cacheSizeMethod == "MB") cacheSizeMB = setting->value("cacheSizeMB").toInt();
        //*/
    if (setting->contains("isShowCacheStatus")) isShowCacheProgressBar = setting->value("isShowCacheStatus").toBool();
    if (setting->contains("cacheStatusWidth")) progressWidth = setting->value("cacheStatusWidth").toInt();
    progressWidthBeforeResizeWindow = progressWidth;
    if (setting->contains("cacheWtAhead")) cacheWtAhead = setting->value("cacheWtAhead").toInt();
    if (setting->contains("isCachePreview")) isCachePreview = setting->value("isCachePreview").toBool();
    if (setting->contains("cachePreviewWidth")) cachePreviewWidth = setting->value("cachePreviewWidth").toInt();
    if (setting->contains("cachePreviewHeight")) cachePreviewHeight = setting->value("cachePreviewHeight").toInt();

    // full screen
    if (setting->contains("isFullScreenFolders")) fullScreenDocks.isFolders = setting->value("isFullScreenFolders").toBool();
    if (setting->contains("isFullScreenFavs")) fullScreenDocks.isFavs = setting->value("isFullScreenFavs").toBool();
    if (setting->contains("isFullScreenFilters")) fullScreenDocks.isFilters = setting->value("isFullScreenFilters").toBool();
    if (setting->contains("isFullScreenMetadata")) fullScreenDocks.isMetadata = setting->value("isFullScreenMetadata").toBool();
    if (setting->contains("isFullScreenThumbs")) fullScreenDocks.isThumbs = setting->value("isFullScreenThumbs").toBool();
    if (setting->contains("isFullScreenStatusBar")) fullScreenDocks.isStatusBar = setting->value("isFullScreenStatusBar").toBool();


    /* read external apps */
    /* moved to createActions as required to populate open with ... menu */

    /* read ingest token templates */
    setting->beginGroup("PathTokens");
    QStringList keys = setting->childKeys();
    for (int i = 0; i < keys.size(); ++i) {
        QString key = keys.at(i);
        pathTemplates[key] = setting->value(key).toString();
    }
    setting->endGroup();

    setting->beginGroup("FilenameTokens");
    keys = setting->childKeys();
    for (int i = 0; i < keys.size(); ++i) {
        QString key = keys.at(i);
        filenameTemplates[key] = setting->value(key).toString();
    }
    setting->endGroup();

    /* read recent folders */
    setting->beginGroup("RecentFolders");
    QStringList recentList = setting->childKeys();
    for (int i = 0; i < recentList.size(); ++i) {
        recentFolders->append(setting->value(recentList.at(i)).toString());
    }
    setting->endGroup();

    /* read ingest history folders */
    setting->beginGroup("IngestHistoryFolders");
    QStringList ingestHistoryList = setting->childKeys();
    for (int i = 0; i < ingestHistoryList.size(); ++i) {
        ingestHistoryFolders->append(setting->value(ingestHistoryList.at(i)).toString());
     }
    setting->endGroup();

    /* read ingest description completer list
       Can use a simpler format to read the list (compared to read recent folders)
       because there will not be any "/" or "\" characters which the windows registry
       does not like.
    */
    setting->beginGroup("IngestDescriptionCompleter");
    ingestDescriptionCompleter = setting->childKeys();
    setting->endGroup();

    loadWorkspaces();
    // moved read workspaces to separate function as req'd by actions while the
    // rest of QSettings are dependent on actions being defined first.

    return true;
}

void MW::loadShortcuts(bool defaultShortcuts)
{
    if (G::isLogger) G::log(__FUNCTION__);
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
        refineAction->setShortcut(QKeySequence("R"));
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
    Called when program starting or when a workspace is invoked. Based on the condition of
    actions sets the visibility of all window components.
*/
    if (G::isLogger) G::log(__FUNCTION__);
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
    if (G::isLogger) G::log(__FUNCTION__);
    bool showImageCount = fsTree->isShowImageCount();
    fsTree->refreshModel();
    fsTree->setShowImageCount(showImageCount);
    fsTree->updateVisibleImageCount();

    // make folder panel visible and set focus
    folderDock->raise();
    folderDockVisibleAction->setChecked(true);

    // set sort forward (not reverse)
    if (sortReverseAction->isChecked()) {
        sortReverseAction->setChecked(false);
        sortChange(__FUNCTION__);
        reverseSortBtn->setIcon(QIcon(":/images/icon16/A-Z.png"));
    }

    // do not embellish
    embelProperties->invokeFromAction(embelTemplatesActions.at(0));
}

/**********************************************************************************************
 * HIDE/SHOW UI ELEMENTS
*/

void MW::setThumbDockFloatFeatures(bool isFloat)
{
    if (G::isLogger) G::log(__FUNCTION__, "isFloat = " + QString::number(isFloat));
    if (isFloat) {
        thumbView->setMaximumHeight(100000);
        thumbDock->setFeatures(QDockWidget::DockWidgetClosable |
                               QDockWidget::DockWidgetMovable  |
                               QDockWidget::DockWidgetFloatable);
//        thumbsWrapAction->setChecked(true);
        thumbView->setWrapping(true);
//        thumbView->isFloat = isFloat;
        thumbView->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    }
}

void MW:: setThumbDockHeight()
{
/*
    Helper slot to call setThumbDockFeatures when the dockWidgetArea is not known, which is
    the case when signalling from another class like thumbView after thumbnails have been
    resized.
*/
    if (G::isLogger) G::log(__FUNCTION__);
    setThumbDockFeatures(dockWidgetArea(thumbDock));
}

void MW::setThumbDockFeatures(Qt::DockWidgetArea area)
{
/*
    When the thumbDock is moved or when the thumbnails have been resized set the
    thumbDock features accordingly, based on settings in preferences for wrapping
    and titlebar visibility.

    Note that a floating thumbDock does not trigger this slot. The float
    condition is handled by setThumbDockFloatFeatures.

    Also note that the gridView is located in the central widget so this function only
    applies to thumbView (the docked version of IconView).

*/
    if (G::isLogger) G::log(__FUNCTION__);
    if (thumbDock->isFloating()) return;
    thumbView->setMaximumHeight(100000);

    /* Check if the thumbDock is docked top or bottom. If so, set the titlebar to vertical and
    the thumbDock to accomodate the height of the thumbs. Set horizontal scrollbar on all the
    time (to simplify resizing dock and thumbs). The vertical scrollbar depends on whether
    wrapping is checked in preferences.
    */
    if (area == Qt::BottomDockWidgetArea || area == Qt::TopDockWidgetArea) {
        thumbDock->setFeatures(QDockWidget::DockWidgetClosable |
                               QDockWidget::DockWidgetMovable  |
                               QDockWidget::DockWidgetFloatable |
                               QDockWidget::DockWidgetVerticalTitleBar);
        thumbView->setWrapping(false);

        // if thumbDock area changed then set dock height to cell size

        // get max icon height based on best aspect
        int hMax = G::iconHMax;

        // max and min cell heights (icon plus padding + name text)
        int maxHt = thumbView->iconViewDelegate->getCellSize(QSize(hMax, hMax)).height();
        int minHt = thumbView->iconViewDelegate->getCellSize(QSize(ICON_MIN, ICON_MIN)).height();
        // plus the scroll bar + 2 to make sure no vertical scroll bar is required
        maxHt += G::scrollBarThickness /*+ 2*/;
        minHt += G::scrollBarThickness;
//        thumbView->verticalScrollBar()->setVisible(false);

        if (maxHt <= minHt) maxHt = G::maxIconSize;

        // new cell height
        int cellHt = thumbView->iconViewDelegate->getCellHeightFromThumbHeight(thumbView->iconHeight);

        //  new dock height based on new cell size
        int newThumbDockHeight = cellHt + G::scrollBarThickness;
        if (newThumbDockHeight > maxHt) newThumbDockHeight = maxHt;

        thumbView->setMaximumHeight(maxHt);
        thumbView->setMinimumHeight(minHt);

        thumbView->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
        thumbView->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        resizeDocks({thumbDock}, {newThumbDockHeight}, Qt::Vertical);
        /*
        qDebug() << "\nMW::setThumbDockFeatures dock area =" << area << "\n"
             << "***  thumbView Ht =" << thumbView->height()
             << "maxHt ="  << maxHt << "minHt =" << minHt
             << "thumbHeight =" << thumbView->thumbHeight
             << "newThumbDockHeight" << newThumbDockHeight
             << "scrollBarHeight =" << G::scrollBarThickness;
//        */
    }

    /* Must be docked left or right or is floating.  Turn horizontal scrollbars off.  Turn
       wrapping on.
    */
    else {
        thumbDock->setFeatures(QDockWidget::DockWidgetClosable |
                               QDockWidget::DockWidgetMovable  |
                               QDockWidget::DockWidgetFloatable);
        thumbView->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        thumbView->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
        thumbView->setWrapping(true);
    }
}

void MW::newEmbelTemplate()
{
    if (G::isLogger) G::log(__FUNCTION__);
    embelDock->setVisible(true);
    embelDock->raise();
    embelDockVisibleAction->setChecked(true);
    embelProperties->newTemplate();
    loupeDisplay();
}

void MW::loupeDisplay()
{
/*
    In the central widget show a loupe view of the image pointed to by the thumbView
    currentindex.

    Note: When the thumbDock thumbView is displayed it needs to be scrolled to the
    currentIndex since it has been "hidden". However, the scrollbars take a long time to paint
    after the view show event, so the ThumbView::scrollToCurrent function must be delayed.
    This is done by the eventFilter in MW, intercepting the scrollbar paint events. This is a
    bit of a cludge to get around lack of notification when the QListView has finished
    painting itself.
*/
    if (G::isLogger || G::isFlowLogger) G::log(__FUNCTION__);
    G::mode = "Loupe";
    asLoupeAction->setChecked(true);
    updateStatus(true, "", __FUNCTION__);
    updateIconsVisible(false);

    // save selection as tableView is hidden and not synced
    saveSelection();

    /* show imageView in the central widget. This makes thumbView visible, and
    it updates the index to its previous state.  The index update triggers
    fileSelectionChange  */
    centralLayout->setCurrentIndex(LoupeTab);
    prevCentralView = LoupeTab;

    // recover thumbdock if it was visible before as gridView and full screen can
    // hide the thumbdock
    if(isNormalScreen && wasThumbDockVisible) {
        thumbDock->setVisible(true);
        thumbDockVisibleAction->setChecked(wasThumbDockVisible);
        thumbView->selectThumb(currentRow);
    }

    if (thumbView->isVisible()) thumbView->setFocus();
    else imageView->setFocus();

    QModelIndex idx = dm->sf->index(currentRow, 0);
    thumbView->setCurrentIndex(idx);

    // update imageView, use cache if image loaded, else read it from file
//lzwrgh
//    QString fPath = idx.data(G::PathRole).toString();
//    if (imageView->isVisible() && fPath.length() > 0) {
//        imageView->loadImage(fPath, __FUNCTION__);
//    }
    // do not show classification badge if no folder or nothing selected
    updateClassification();

    // req'd after compare mode to re-enable extended selection
    thumbView->setSelectionMode(QAbstractItemView::ExtendedSelection);

    // selection has been lost while tableView and possibly thumbView were hidden
    recoverSelection();

    // req'd to show thumbs first time
    thumbView->setThumbParameters();

    // sync scrolling between modes (loupe, grid and table)
    updateIconsVisible(false);
    if (prevMode == "Table") {
        if (tableView->isRowVisible(currentRow)) scrollRow = currentRow;
        else scrollRow = tableView->midVisibleRow;
    }
    if (prevMode == "Grid") {
        if(gridView->isRowVisible(currentRow)) scrollRow = currentRow;
        else scrollRow = gridView->midVisibleCell;
    }
    if (prevMode == "Compare") {
        qDebug() << __FUNCTION__ << currentRow;
        scrollRow = currentRow;
    }
    G::ignoreScrollSignal = false;
//    thumbView->waitUntilOkToScroll();
    thumbView->scrollToRow(scrollRow, __FUNCTION__);
//    updateIconsVisible(false);

    // If the zoom dialog was active, but hidden by gridView or tableView, then show it
    if (zoomDlg && isZoomDlgVisible) zoomDlg->setVisible(true);

    prevMode = "Loupe";

}

void MW::gridDisplay()
{
/*
    Note: When the gridView is displayed it needs to be scrolled to the currentIndex since it
    has been "hidden". However, the scrollbars take a long time to paint after the view show
    event, so the ThumbView::scrollToCurrent function must be delayed. This is done by the
    eventFilter in MW (installEventFilter), intercepted the scrollbar paint events. This is a
    bit of a cludge to get around lack of notification when the QListView has finished
    painting itself.
*/
    if (G::isLogger || G::isFlowLogger) G::log(__FUNCTION__);
    G::mode = "Grid";
    asGridAction->setChecked(true);
    updateStatus(true, "", __FUNCTION__);
    updateIconsVisible(false);

    // save selection as gridView is hidden and not synced
    saveSelection();

    // hide the thumbDock in grid mode as we don't need to see thumbs twice
    thumbDock->setVisible(false);
    thumbDockVisibleAction->setChecked(false);

    // show gridView in central widget
    centralLayout->setCurrentIndex(GridTab);
    prevCentralView = GridTab;

    QModelIndex idx = dm->sf->index(currentRow, 0);
    gridView->setCurrentIndex(idx);
    thumbView->setCurrentIndex(idx);

    // req'd to show thumbs first time
    gridView->setThumbParameters();

    // req'd after compare mode to re-enable extended selection
    gridView->setSelectionMode(QAbstractItemView::ExtendedSelection);

    // selection has been lost while tableView and possibly thumbView were hidden
    recoverSelection();

    // req'd to show thumbs first time
    gridView->setThumbParameters();

    // sync scrolling between modes (loupe, grid and table)
    if (prevMode == "Table") {
        if (tableView->isRowVisible(currentRow)) scrollRow = currentRow;
        else scrollRow = tableView->midVisibleRow;
    }
    if (prevMode == "Loupe" /*&& thumbView->isVisible() == true*/) {
        if (thumbView->isRowVisible(currentRow)) scrollRow = currentRow;
        else scrollRow = thumbView->midVisibleCell;
    }

    // when okToScroll scroll gridView to current row
    G::ignoreScrollSignal = false;
//    gridView->waitUntilOkToScroll();
     gridView->scrollToRow(scrollRow, __FUNCTION__);
    updateIconsVisible(false);

    if (gridView->justifyMargin() > 3) gridView->rejustify();

    // if the zoom dialog was open then hide it as no image visible to zoom
    if (zoomDlg && isZoomDlgVisible) zoomDlg->setVisible(false);

    gridView->setFocus();
    prevMode = "Grid";
    gridDisplayFirstOpen = false;
}

void MW::tableDisplay()
{
    if (G::isLogger || G::isFlowLogger) G::log(__FUNCTION__);
    G::mode = "Table";
    asTableAction->setChecked(true);
    updateStatus(true, "", __FUNCTION__);
    updateIconsVisible(false);

    // save selection as tableView is hidden and not synced
    saveSelection();

    // change to the table view
    centralLayout->setCurrentIndex(TableTab);
    prevCentralView = TableTab;

    /* thumbView, gridView and tableView share the same datamodel and selection
       model, so when one changes due to user interaction they all change, unless
       they are not visible.  Therefore we must do a manual update of the current
       index (currentRow) and selection every time there is a mode change between
       Loupe, Grid, Table and Compare.

       Changes to the current index signal the slot fileSelectionChange, which in
       turn updates currentRow to the current index.
    */
    // get the current index from currentRow
    QModelIndex idx = dm->sf->index(currentRow, 0);
    // set the current index for all views that could be visible
    tableView->setCurrentIndex(idx);
    thumbView->setCurrentIndex(idx);

    // recover thumbdock if it was visible before as gridView and full screen can
    // hide the thumbdock
    if(isNormalScreen){
        if(wasThumbDockVisible && !thumbDock->isVisible()) {
            thumbDock->setVisible(true);
            thumbDockVisibleAction->setChecked(wasThumbDockVisible);
            thumbView->selectThumb(currentRow);
        }
        if(!wasThumbDockVisible && thumbDock->isVisible()) {
            thumbDock->setVisible(false);
            thumbDockVisibleAction->setChecked(wasThumbDockVisible);
        }

    }

    // req'd after compare mode to re-enable extended selection
    tableView->setSelectionMode(QAbstractItemView::ExtendedSelection);
    thumbView->setSelectionMode(QAbstractItemView::ExtendedSelection);

    // selection has been lost while tableView and possibly thumbView were hidden
    recoverSelection();

    // req'd to show thumbs first time
    thumbView->setThumbParameters();

    // sync scrolling between modes (loupe, grid and table)
//    updateMetadataCacheIconviewState(false);
    if (prevMode == "Grid") {
        if (gridView->isRowVisible(currentRow)) scrollRow = currentRow;
        else scrollRow = gridView->midVisibleCell;
    }
    if (prevMode == "Loupe") {
        if(thumbView->isRowVisible(currentRow)) scrollRow = currentRow;
        else scrollRow = thumbView->midVisibleCell;
    }
    G::ignoreScrollSignal = false;
//    G::wait(100);
    tableView->scrollToRow(scrollRow, __FUNCTION__);
    if (thumbView->isVisible()) thumbView->scrollToRow(scrollRow, __FUNCTION__);
    updateIconsVisible(false);
//    qDebug() << __FUNCTION__ << scrollRow << tableView->midVisibleRow;

    // if the zoom dialog was open then hide it as no image visible to zoom
    if (zoomDlg && isZoomDlgVisible) zoomDlg->setVisible(false);

    tableView->setFocus();
    prevMode = "Table";
}

void MW::compareDisplay()
{
    if (G::isLogger) G::log(__FUNCTION__);
    asCompareAction->setChecked(true);
    updateStatus(true, "", __FUNCTION__);
    int n = selectionModel->selectedRows().count();
    if (n < 2) {
        G::popUp->showPopup("Select more than one image to compare.");
        return;
    }
    if (n > 16) {
        QString msg = QString::number(n);
        msg += " images have been selected.  Only the first 16 will be compared.";
        G::popUp->showPopup(msg, 2000);
    }

    /* If thumbdock was visible and enter grid mode, make selection, and then
       compare the thumbdock gets frozen (cannot use splitter) at about 1/2 ht.
       Not sure what causes this, but by making the thumbdock visible before
       entered compare mode avoids this.  After enter compare mode revert
       thumbdocK to prior visibility (wasThumbDockVisible).
    */
    thumbDock->setVisible(true);
    thumbDock->raise();
//    thumbView->selectThumb(currentRow);

    G::mode = "Compare";
    // centralLayout->setCurrentIndex clears selectionModel
    saveSelection();
    centralLayout->setCurrentIndex(CompareTab);
    recoverSelection();
    prevCentralView = CompareTab;
    compareImages->load(centralWidget->size(), isRatingBadgeVisible, selectionModel);

    // restore thumbdock to previous state
    thumbDock->setVisible(wasThumbDockVisible);
    thumbDockVisibleAction->setChecked(wasThumbDockVisible);

    // If the zoom dialog was active, but hidden by gridView or tableView, then show it
    if (zoomDlg && isZoomDlgVisible) zoomDlg->setVisible(true);

    hasGridBeenActivated = false;
    prevMode = "Compare";
}

void MW::saveSelection()
{
/*
    This function saves the current selection. This is required, even though the three views
    (thumbView, gridView and tableViews) share the same selection model, because when a view
    is hidden it loses the current index and selection, which has to be re-established each
    time it is made visible.
*/
    if (G::isLogger) G::log(__FUNCTION__);
    selectedRows = selectionModel->selectedRows();
    currentIdx = selectionModel->currentIndex();
}

void MW::recoverSelection()
{
    if (G::isLogger) G::log(__FUNCTION__);
    QItemSelection selection;
    QModelIndex idx;
    foreach (idx, selectedRows)
        selection.select(idx, idx);
    selectionModel->select(selection, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
}

bool MW::getSelection(QStringList &list)
{
/*
    Adds each image that is selected or picked as a file path to list. If there are picks and
    a selection then a dialog offers the user a choice to use.
*/
    if (G::isLogger) G::log(__FUNCTION__);

    bool usePicks = false;

    // nothing picked or selected
    if (thumbView->isPick() && selectionModel->selectedRows().size() == 0) {
        QMessageBox::information(this,
            "Oops", "There are no picks or selected images to report.    ",
            QMessageBox::Ok);
        return false;
    }

    // picks = selection
    bool picksEqualsSelection = true;
    for (int row = 0; row < dm->sf->rowCount(); row++) {
        bool isPicked = dm->sf->index(row, G::PickColumn).data(Qt::EditRole).toString() == "true";
        bool isSelected = selectionModel->isSelected(dm->sf->index(row, 0));
        if (isPicked != isSelected) {
            picksEqualsSelection = false;
            break;
        }
    }

    if (!picksEqualsSelection) {
        // use picks or selected
        if (thumbView->isPick() && selectionModel->selectedRows().size() > 1) {
            SelectionOrPicksDlg::Option option;
            SelectionOrPicksDlg dlg(option);
            dlg.exec();
            if (option == SelectionOrPicksDlg::Option::Cancel) return false;
            if (option == SelectionOrPicksDlg::Option::Picks) usePicks = true;
        }
        else if (thumbView->isPick()) usePicks = true;
    }

    if (usePicks) {
        for (int row = 0; row < dm->sf->rowCount(); row++) {
            if (dm->sf->index(row, G::PickColumn).data(Qt::EditRole).toString() == "true") {
                QModelIndex idx = dm->sf->index(row, 0);
                list << idx.data(G::PathRole).toString();
            }
        }
    }
    else {
        QModelIndexList idxList = selectionModel->selectedRows();
        for (int i = 0; i < idxList.size(); ++i) {
            int row = idxList.at(i).row();
            QModelIndex idx = dm->sf->index(row, 0);
            list << idx.data(G::PathRole).toString();
        }
    }

    return true;
}

void MW::setCentralView()
{
    if (G::isLogger) G::log(__FUNCTION__);
    if (!isSettings) return;
    if (asLoupeAction->isChecked()) loupeDisplay();
    if (asGridAction->isChecked()) gridDisplay();
    if (asTableAction->isChecked()) tableDisplay();
    if (asCompareAction->isChecked()) compareDisplay();
    if (currentViewDirPath == "") {
        QString msg = "Select a folder or bookmark to get started.";
        setCentralMessage(msg);
        prevMode = "Loupe";
    }
}

void MW::tokenEditor()
{
    if (G::isLogger) G::log(__FUNCTION__);
    infoString->editTemplates();
    // display new info
    QModelIndex idx = thumbView->currentIndex();
    QString fPath = thumbView->getCurrentFilePath();
    QString sel = infoString->getCurrentInfoTemplate();
    QString info = infoString->parseTokenString(infoString->infoTemplates[sel],
                                        fPath, idx);
    imageView->setShootingInfo(info);
//    qDebug() << __FUNCTION__ << "call  updateMetadataTemplateList";
    embelProperties->updateMetadataTemplateList();
//    qDebug() << __FUNCTION__ << "updateMetadataTemplateList did not crash";
}

void MW::setRatingBadgeVisibility() {
    if (G::isLogger) G::log(__FUNCTION__);
    isRatingBadgeVisible = ratingBadgeVisibleAction->isChecked();
    thumbView->refreshThumbs();
    gridView->refreshThumbs();
    updateClassification();
}

void MW::setShootingInfoVisibility() {
    if (G::isLogger) G::log(__FUNCTION__);
    imageView->infoOverlay->setVisible(infoVisibleAction->isChecked());
}

void MW::setFolderDockVisibility()
{
    if (G::isLogger) G::log(__FUNCTION__);
    folderDock->setVisible(folderDockVisibleAction->isChecked());
}

void MW::setFavDockVisibility()
{
    if (G::isLogger) G::log(__FUNCTION__);
    favDock->setVisible(favDockVisibleAction->isChecked());
}

void MW::setFilterDockVisibility()
{
    if (G::isLogger) G::log(__FUNCTION__);
    filterDock->setVisible(filterDockVisibleAction->isChecked());
}

void MW::setMetadataDockVisibility()
{
    if (G::isLogger) G::log(__FUNCTION__);
    if (useInfoView) metadataDock->setVisible(metadataDockVisibleAction->isChecked());
}

void MW::setEmbelDockVisibility()
{
    if (G::isLogger) G::log(__FUNCTION__);
    embelDock->setVisible(embelDockVisibleAction->isChecked());
}

void MW::setMetadataDockFixedSize()
{
    if (!useInfoView) return;
    if (G::isLogger) G::log(__FUNCTION__);
    if (metadataFixedSizeAction->isChecked()) {
        qDebug() << "variable size";
        metadataDock->setMinimumSize(200, 125);
        metadataDock->setMaximumSize(999999, 999999);
    }
    else {
        qDebug() << "fixed size";
        metadataDock->setFixedSize(metadataDock->size());
    }
}

void MW::setThumbDockVisibity()
{
    if (G::isLogger) G::log(__FUNCTION__);
    thumbDock->setVisible(thumbDockVisibleAction->isChecked());
    thumbView->selectThumb(currentRow);
}

void MW::toggleFolderDockVisibility()
{
    if (G::isLogger) G::log(__FUNCTION__);
    if (G::isInitializing) return;
    QString dock = folderDockTabText;
    if (isDockTabified(dock) && !isSelectedDockTab(dock)) dockToggle = SetFocus;
    else if (folderDock->isVisible()) dockToggle = SetInvisible;
    else dockToggle = SetVisible;

    switch (dockToggle) {
    case SetFocus:
        folderDock->raise();
        folderDockVisibleAction->setChecked(true);
        break;
    case SetInvisible:
        folderDock->setVisible(false);
        folderDockVisibleAction->setChecked(false);
        break;
    case SetVisible:
        folderDock->setVisible(true);
        folderDock->raise();
        folderDockVisibleAction->setChecked(true);
    }
}

void MW::toggleFavDockVisibility() {
    if (G::isLogger) G::log(__FUNCTION__);
    if (G::isInitializing) return;
    QString dock = favDockTabText;
    if (isDockTabified(dock) && !isSelectedDockTab(dock)) dockToggle = SetFocus;
    else if (favDock->isVisible()) dockToggle = SetInvisible;
    else dockToggle = SetVisible;

    switch (dockToggle) {
    case SetFocus:
        favDock->raise();
        favDockVisibleAction->setChecked(true);
        break;
    case SetInvisible:
        favDock->setVisible(false);
        favDockVisibleAction->setChecked(false);
        break;
    case SetVisible:
        favDock->setVisible(true);
        favDock->raise();
        favDockVisibleAction->setChecked(true);
    }
}

void MW::toggleFilterDockVisibility() {
    if (G::isLogger) G::log(__FUNCTION__);
    if (G::isInitializing) return;
    QString dock = filterDockTabText;
    if (isDockTabified(dock) && !isSelectedDockTab(dock)) dockToggle = SetFocus;
    else if (filterDock->isVisible()) dockToggle = SetInvisible;
    else dockToggle = SetVisible;

    switch (dockToggle) {
    case SetFocus:
        filterDock->raise();
        filterDockVisibleAction->setChecked(true);
        break;
    case SetInvisible:
        filterDock->setVisible(false);
        filterDockVisibleAction->setChecked(false);
        break;
    case SetVisible:
        filterDock->setVisible(true);
        filterDock->raise();
        filterDockVisibleAction->setChecked(true);
    }
}

void MW::toggleMetadataDockVisibility() {
    if (!useInfoView) return;
    if (G::isLogger) G::log(__FUNCTION__);
    if (G::isInitializing) return;
    QString dock = metadataDockTabText;
    if (isDockTabified(dock) && !isSelectedDockTab(dock)) dockToggle = SetFocus;
    else if (metadataDock->isVisible()) dockToggle = SetInvisible;
    else dockToggle = SetVisible;

    switch (dockToggle) {
    case SetFocus:
        metadataDock->raise();
        metadataDockVisibleAction->setChecked(true);
        break;
    case SetInvisible:
        metadataDock->setVisible(false);
        metadataDockVisibleAction->setChecked(false);
        break;
    case SetVisible:
        metadataDock->setVisible(true);
        metadataDock->raise();
        metadataDockVisibleAction->setChecked(true);
    }
}

void MW::toggleThumbDockVisibity()
{
    if (G::isLogger) G::log(__FUNCTION__);
    if (G::isInitializing) return;
    qDebug() << __FUNCTION__;
    QString dock = thumbDockTabText;
    if (isDockTabified(dock) && !isSelectedDockTab(dock)) dockToggle = SetFocus;
    else if (thumbDock->isVisible()) dockToggle = SetInvisible;
    else dockToggle = SetVisible;

    switch (dockToggle) {
    case SetFocus:
        thumbDock->raise();
        thumbDockVisibleAction->setChecked(true);
        break;
    case SetInvisible:
        thumbDock->setVisible(false);
        thumbDockVisibleAction->setChecked(false);
        break;
    case SetVisible:
        thumbDock->setVisible(true);
        thumbDock->raise();
        thumbDockVisibleAction->setChecked(true);
        qDebug() << __FUNCTION__ << currentSfIdx.data() << "Calling fileSelectionChange(currentSfIdx, currentSfIdx)";
        fileSelectionChange(currentSfIdx, currentSfIdx);
    }

    if (G::mode != "Grid" && isNormalScreen) {
        wasThumbDockVisible = thumbDock->isVisible();
    }
/*    qDebug() << __FUNCTION__
             << "wasThumbDockVisible =" << wasThumbDockVisible
             << "G::mode =" << G::mode
             << "isNormalScreen =" << isNormalScreen
             << "thumbDock->isVisible() =" << thumbDock->isVisible();*/
}

void MW::toggleEmbelDockVisibility() {
    if (G::isLogger) G::log(__FUNCTION__);
    if (G::isInitializing) return;
    QString dock = embelDockTabText;
    if (isDockTabified(dock) && !isSelectedDockTab(dock)) dockToggle = SetFocus;
    else if (embelDock->isVisible()) dockToggle = SetInvisible;
    else dockToggle = SetVisible;

    switch (dockToggle) {
    case SetFocus:
        embelDock->raise();
        embelDockVisibleAction->setChecked(true);
        break;
    case SetInvisible:
        embelDock->setVisible(false);
        embelDockVisibleAction->setChecked(false);
        break;
    case SetVisible:
        embelDock->setVisible(true);
        embelDock->raise();
        embelDockVisibleAction->setChecked(true);
    }
}

void MW::setMenuBarVisibility()
{
    if (G::isLogger) G::log(__FUNCTION__);
    menuBar()->setVisible(menuBarVisibleAction->isChecked());
}

void MW::setStatusBarVisibility()
{
    if (G::isLogger) G::log(__FUNCTION__);
    statusBar()->setVisible(statusBarVisibleAction->isChecked());
}

void MW::setCacheStatusVisibility()
{
    if (G::isLogger) G::log(__FUNCTION__);
    if (isShowCacheProgressBar && !G::isSlideShow)
        progressLabel->setVisible(isShowCacheProgressBar);
}

void MW::setProgress(int value)
{
    if (G::isLogger) G::log(__FUNCTION__);
    if (value < 0 || value > 100) {
        progressBar->setVisible(false);
        return;
    }
    progressBar->setValue(value);
    progressBar->setVisible(true);
    progressBar->repaint();
}

// not used rgh ??
void MW::setStatus(QString state)
{
    if (G::isLogger) G::log(__FUNCTION__);
    statusLabel->setText("    " + state + "    ");
}

void MW::setIngested()
{
    if (G::isLogger) G::log(__FUNCTION__);
    for (int row = 0; row < dm->sf->rowCount(); ++row) {
        if (dm->sf->index(row, G::PickColumn).data().toString() == "true") {
            dm->sf->setData(dm->sf->index(row, G::IngestedColumn), "true");
            dm->sf->setData(dm->sf->index(row, G::PickColumn), "false");
        }
    }
}

void MW::toggleReject()
{
/*
    If the selection has any images that are not rejected then reject them all.
    If the entire selection was already rejected then unreject them all.
    If the entire selection is nor rejected then reject them all.
*/
    if (G::isLogger) G::log(__FUNCTION__);
    QModelIndex idx;
    QModelIndexList idxList = selectionModel->selectedRows();
    QString pickStatus;

    // add multiple selection flag to pick history
    if (idxList.length() > 1) pushPick("Begin multiple select");

    bool foundFalse = false;
    // check if any images are not rejected in the selection
    foreach (idx, idxList) {
        QModelIndex pickIdx = dm->sf->index(idx.row(), G::PickColumn);
        pickStatus = qvariant_cast<QString>(pickIdx.data(Qt::EditRole));
        foundFalse = (pickStatus != "reject");
        if (foundFalse) break;
    }
    foundFalse ? pickStatus = "reject" : pickStatus = "false";

    // set pick status for selection
    foreach (idx, idxList) {
        // save pick history
        QString fPath = dm->sf->index(idx.row(), G::PathColumn).data(G::PathRole).toString();
        QString priorPickStatus = dm->sf->index(idx.row(), G::PickColumn).data().toString();
        pushPick(fPath, priorPickStatus);
        // set pick status
        QModelIndex pickIdx = dm->sf->index(idx.row(), G::PickColumn);
        dm->sf->setData(pickIdx, pickStatus, Qt::EditRole);
    }
    if (idxList.length() > 1) pushPick("End multiple select");

    updateClassification();
    thumbView->refreshThumbs();
    gridView->refreshThumbs();

    pickMemSize = Utilities::formatMemory(memoryReqdForPicks());
    updateStatus(true, "", __FUNCTION__);

    // update filter counts
    buildFilters->updateCountFiltered();
}

void MW::togglePickUnlessRejected()
{
/*
    If the selection has any images that are not picked then pick them all.
    If the entire selection was already picked then unpick them all.
    If the entire selection is unpicked then pick them all.
    Push the changes onto the pick history stack.
*/
    if (G::isLogger) G::log(__FUNCTION__);
    QModelIndex idx;
    QModelIndexList idxList = selectionModel->selectedRows();
    QString pickStatus;
    QString newPickStatus;

    bool foundFalse = false;
    // check if any images are not picked in the selection
    foreach (idx, idxList) {
        QModelIndex pickIdx = dm->sf->index(idx.row(), G::PickColumn);
        pickStatus = qvariant_cast<QString>(pickIdx.data(Qt::EditRole));
        foundFalse = (pickStatus == "false");
        if (foundFalse) break;
    }
    foundFalse ? newPickStatus = "true" : newPickStatus = "false";

    // add multiple selection flag to pick history
    if (idxList.length() > 1) pushPick("Begin multiple select");

    // set pick status for selection
    foreach (idx, idxList) {
        // save pick history
        QString fPath = dm->sf->index(idx.row(), G::PathColumn).data(G::PathRole).toString();
        QString priorPickStatus = dm->sf->index(idx.row(), G::PickColumn).data().toString();
        pushPick(fPath, priorPickStatus);
        // set pick status
        QModelIndex pickIdx = dm->sf->index(idx.row(), G::PickColumn);
        pickStatus = qvariant_cast<QString>(pickIdx.data(Qt::EditRole));
        if (pickStatus != "reject") {
            dm->sf->setData(pickIdx, newPickStatus, Qt::EditRole);
        }
    }
    if (idxList.length() > 1) pushPick("End multiple select");

    updateClassification();
    thumbView->refreshThumbs();
    gridView->refreshThumbs();

    pickMemSize = Utilities::formatMemory(memoryReqdForPicks());
    updateStatus(true, "", __FUNCTION__);

    // update filter counts
    buildFilters->updateCountFiltered();
}

void MW::togglePickMouseOver()
{
/*
    Toggles the pick status item the mouse is over is toggled, but the selection is not
    changed.

    Triggered by ThumbView context menu MW::pickMouseOverAction.  ThumbView mousePressEvent
    stores the mouse click indexAt(position).  Use this to call togglePickMouseOverItem.
*/
    if (G::isLogger) G::log(__FUNCTION__);
    togglePickMouseOverItem(thumbView->mouseOverIndex);
}

void MW::togglePickMouseOverItem(QModelIndex idx)
{
/*
    This is called from IconView forward or back mouse click. The pick status item the mouse is
    over is toggled, but the selection is not changed.
*/
    if (G::isLogger) G::log(__FUNCTION__);
    QModelIndex pickIdx = dm->sf->index(idx.row(), G::PickColumn);
    QString pickStatus = qvariant_cast<QString>(pickIdx.data(Qt::EditRole));
    pickStatus == "false" ? pickStatus = "true" : pickStatus = "false";
    dm->sf->setData(pickIdx, pickStatus, Qt::EditRole);

    updateClassification();
    thumbView->refreshThumbs();
    gridView->refreshThumbs();

    pickMemSize = Utilities::formatMemory(memoryReqdForPicks());
    updateStatus(true, "", __FUNCTION__);

    // update filter counts
    buildFilters->updateCountFiltered();
}

void MW::togglePick()
{
/*
    If the selection has any images that are not picked then pick them all.
    If the entire selection was already picked then unpick them all.
    If the entire selection is unpicked then pick them all.
    Push the changes onto the pick history stack.
*/
    if (G::isLogger) G::log(__FUNCTION__);
    QModelIndex idx;
    QModelIndexList idxList = selectionModel->selectedRows();
    QString pickStatus;

    bool foundFalse = false;
    // check if any images are not picked in the selection
    foreach (idx, idxList) {
        QModelIndex pickIdx = dm->sf->index(idx.row(), G::PickColumn);
        pickStatus = qvariant_cast<QString>(pickIdx.data(Qt::EditRole));
        foundFalse = (pickStatus != "true");
        if (foundFalse) break;
    }
    foundFalse ? pickStatus = "true" : pickStatus = "false";

    // add multiple selection flag to pick history
    if (idxList.length() > 1) pushPick("Begin multiple select");

    // set pick status for selection
    foreach (idx, idxList) {
        // save pick history
        QString fPath = dm->sf->index(idx.row(), G::PathColumn).data(G::PathRole).toString();
        QString priorPickStatus = dm->sf->index(idx.row(), G::PickColumn).data().toString();
        pushPick(fPath, priorPickStatus);
        // set pick status
        QModelIndex pickIdx = dm->sf->index(idx.row(), G::PickColumn);
        dm->sf->setData(pickIdx, pickStatus, Qt::EditRole);
        updatePickLog(fPath, pickStatus);
    }
    if (idxList.length() > 1) pushPick("End multiple select");

    updateClassification();
    thumbView->refreshThumbs();
    gridView->refreshThumbs();

    pickMemSize = Utilities::formatMemory(memoryReqdForPicks());
    updateStatus(true, "", __FUNCTION__);

    // update filter counts
    buildFilters->updateCountFiltered();

    // auto advance
    if (autoAdvance) thumbView->selectNext();
}

int MW::pickLogCount()
{
    if (G::isLogger) G::log(__FUNCTION__);
    setting->beginGroup("PickLog");
    int count = setting->allKeys().size();
    setting->endGroup();
    return count;
}

void MW::recoverPickLog()
{
    if (G::isLogger) G::log(__FUNCTION__);
    setting->beginGroup("PickLog");
    QStringList keys = setting->allKeys();
    for (int i = 0; i < keys.length(); ++i) {
        QString fPath = keys.at(i);
        fPath.replace("🔸", "/");
        QString pickStatus = setting->value(keys.at(i)).toString();
        QModelIndex idx = dm->proxyIndexFromPath(fPath);
        if (idx.isValid()) {
            QModelIndex pickIdx = dm->sf->index(idx.row(), G::PickColumn);
            dm->sf->setData(pickIdx, pickStatus, Qt::EditRole);
            qDebug() << __FUNCTION__ << pickStatus << fPath << "updated";
        }
        else {
            qDebug() << __FUNCTION__ << fPath << "not found";
        }
    }
    setting->endGroup();
    thumbView->refreshThumbs();
    gridView->refreshThumbs();
}

void MW::clearPickLog()
{
    if (G::isLogger) G::log(__FUNCTION__);
    setting->beginGroup("PickLog");
    QStringList keys = setting->allKeys();
    for (int i = 0; i < keys.length(); ++i) {
        setting->remove(keys.at(i));
    }
    setting->endGroup();
}

void MW::updatePickLog(QString fPath, QString pickStatus)
{
    if (G::isLogger) G::log(__FUNCTION__);
    setting->beginGroup("PickLog");
    QString sKey = fPath;
    sKey.replace("/", "🔸");
    if (pickStatus == "true") {
//        qDebug() << __FUNCTION__ << "adding" << sKey;
        setting->setValue(sKey, pickStatus);
    }
    else {
//        qDebug() << __FUNCTION__ << "removing" << sKey;
        setting->remove(sKey);
    }
    setting->endGroup();
}

void MW::pushPick(QString fPath, QString status)
{
/*
    Adds a pick action (either to pick or unpick) to the pickStack history. This is used to
    recover a prior pick history state if the picks have been lost due to an accidental
    erasure.
*/
    if (G::isLogger) G::log(__FUNCTION__);
    pick.path = fPath;
    pick.status = status;
    pickStack->push(pick);
}

void MW::popPick()
{
    if (G::isLogger) G::log(__FUNCTION__);
    if (pickStack->isEmpty()) return;
    pick = pickStack->pop();
    if (pick.path != "End multiple select") {
        updatePickFromHistory(pick.path, pick.status);
    }
    else {
        while (!pickStack->isEmpty()) {
            pick = pickStack->pop();
            if (pick.path == "Begin multiple select") break;
            updatePickFromHistory(pick.path, pick.status);
        }
    }
}

void MW::updatePickFromHistory(QString fPath, QString status)
{
    if (G::isLogger) G::log(__FUNCTION__);
    if (dm->fPathRow.contains(fPath)) {
        int row = dm->fPathRow[fPath];
        QModelIndex pickIdx = dm->sf->index(row, G::PickColumn);
        dm->sf->setData(pickIdx, status, Qt::EditRole);
//        dm->sf->filterChange();
        thumbView->refreshThumbs();
        gridView->refreshThumbs();

        pickMemSize = Utilities::formatMemory(memoryReqdForPicks());
        updateStatus(true, "", __FUNCTION__);

        // update filter counts
        buildFilters->updateCountFiltered();
    }
}

qulonglong MW::memoryReqdForPicks()
{
    if (G::isLogger) G::log(__FUNCTION__);
    qulonglong memTot = 0;
    for(int row = 0; row < dm->sf->rowCount(); row++) {
        QModelIndex idx = dm->sf->index(row, G::PickColumn);
        if(qvariant_cast<QString>(idx.data(Qt::EditRole)) == "true") {
            idx = dm->sf->index(row, G::SizeColumn);
            memTot += idx.data(Qt::EditRole).toULongLong();
        }
    }
    return memTot;
}

qulonglong MW::memoryReqdForSelection()
{
    if (G::isLogger) G::log(__FUNCTION__);
    qulonglong memTot = 0;
    QModelIndexList selection = selectionModel->selectedRows();
    for(int row = 0; row < selection.count(); row++) {
        QModelIndex idx = dm->sf->index(row, G::SizeColumn);
        memTot += idx.data(Qt::EditRole).toULongLong();
    }
    return memTot;
}

void MW::exportEmbel()
{
/*

*/
    if (G::isLogger) G::log(__FUNCTION__);
    QStringList picks;

    // build QStringList of picks
    if (thumbView->isPick()) {
        for (int row = 0; row < dm->sf->rowCount(); ++row) {
            QModelIndex pickIdx = dm->sf->index(row, G::PickColumn);
            QModelIndex idx = dm->sf->index(row, 0);
            // only picks
            if (pickIdx.data(Qt::EditRole).toString() == "true") {
                picks << idx.data(G::PathRole).toString();
            }
        }
    }

    // build QStringList of selected images
    else if (selectionModel->selectedRows().size() > 0) {
        QModelIndexList idxList = selectionModel->selectedRows();
        for (int i = 0; i < idxList.size(); ++i) {
            int row = idxList.at(i).row();
            QModelIndex idx = dm->sf->index(row, 0);
            picks << idx.data(G::PathRole).toString();
        }
    }

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

        This function keeps track of the presSourceFolder and baseFolderDescription during
        subsequent calls.  It uses this to effect the behavior of the IngestDlg, which is
        called.  When IngestDlg closes persistent ingest data is saving in settings.  If the
        isBackgroundIngest flag = true then a backgroundIngest instantiation of Ingest is
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

    if (G::isLogger) G::log(__FUNCTION__);

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
    qDebug() << __FUNCTION__
             << "prevSourceFolder" << prevSourceFolder
             << "currentViewDirPath" << currentViewDirPath
             << "baseFolderDescription" << baseFolderDescription
                ;
    if (prevSourceFolder != currentViewDirPath) baseFolderDescription = "";

    QString folderPath;        // req'd by backgroundIngest
    QString folderPath2;       // req'd by backgroundIngest
    bool combinedIncludeJpg;   // req'd by backgroundIngest
    int seqStart = 1;          // req'd by backgroundIngest

    if (thumbView->isPick()) {
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

        if (!okToIngest) return;

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

        prevSourceFolder = currentViewDirPath;
        qDebug() << __FUNCTION__
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

        // set the ingested flags and clear the pick flags
        setIngested();

        updateStatus(true, "", __FUNCTION__);
    }
    else QMessageBox::information(this,
         "Oops", "There are no picks to ingest.    ", QMessageBox::Ok);
    //*/
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
    if (G::isLogger) G::log(__FUNCTION__);

    // if current folder is on USB drive to be ejected then stop caching
    QStorageInfo ejectDrive(path);
    QStorageInfo currentDrive(currentViewDirPath);
    /*
    qDebug() << __FUNCTION__
             << "currentViewDir =" << currentViewDir
             << "path =" << path
             << "currentDrive.name() =" << currentDrive.name()
             << "currentDrive.rootPath() =" << currentDrive.rootPath()
             << "ejectDrive.name() =" << ejectDrive.name()
             << "ejectDrive.rootPath() =" << ejectDrive.rootPath()
                ;
                //*/
    if (currentDrive.rootPath() == ejectDrive.rootPath()) {
        /*
        qDebug() << __FUNCTION__
                 << "currentDrive.rootPath() =" << currentDrive.rootPath()
                 << "ejectDrive.rootPath() =" << ejectDrive.rootPath()
                    ;
                    //*/
        stopAndClearAll();
    }

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
    if (Usb::isUsb(path)) {
        dm->load(driveName, false);
        refreshFolders();
        int result = Usb::eject(driveName);
        if (result < 2) {
            G::popUp->showPopup("Ejecting drive " + driveName, 2000);
            folderSelectionChange();
        }
        else
            G::popUp->showPopup("Failed to eject drive " + driveName, 2000);
    }
    else {
        QString d = currentViewDirPath.at(0);
        G::popUp->showPopup("Drive " + d
              + " is not removable and cannot be ejected", 2000);
    }
}

void MW::ejectUsbFromMainMenu()
{
    if (G::isLogger) G::log(__FUNCTION__);
    ejectUsb(currentViewDirPath);
}

void MW::ejectUsbFromContextMenu()
{
    if (G::isLogger) G::log(__FUNCTION__);
    ejectUsb(mouseOverFolder);
}

void MW::setCombineRawJpg()
{
    if (G::isLogger) G::log(__FUNCTION__);
    if (!G::isNewFolderLoaded) {
        QString msg = "Folder is still loading.  Try again when the folder has loaded.";
        G::popUp->showPopup(msg, 1000);
        return;
    }

    QString msg;

    // flag used in MW, dm and sf, fsTree, bookmarks
    combineRawJpg = combineRawJpgAction->isChecked();

    if (combineRawJpg) msg = "Combining Raw + Jpg pairs.  This could take a moment.";
    else msg = "Separating Raw + Jpg pairs.  This could take a moment.";
    G::popUp->showPopup(msg);

    fsTree->combineRawJpg = combineRawJpg;
    bookmarks->combineRawJpg = combineRawJpg;
    refreshBookmarks();

    // show appropriate count column in filters
    if (combineRawJpg) {
        filters->hideColumn(3);
        filters->showColumn(4);
    }
    else {
        filters->hideColumn(4);
        filters->showColumn(3);
    }

    // update the datamodel type column
    for (int row = 0; row < dm->rowCount(); ++row) {
        QModelIndex idx = dm->index(row, 0);
        if (idx.data(G::DupIsJpgRole).toBool()) {
            QString rawType = idx.data(G::DupRawTypeRole).toString();
            QModelIndex typeIdx = dm->index(row, G::TypeColumn);
            if (combineRawJpg) dm->setData(typeIdx, "JPG+" + rawType);
            else dm->setData(typeIdx, "JPG");
        }
    }

    // refresh the proxy sort/filter
    dm->sf->filterChange();
    dm->rebuildTypeFilter();
    filterChange(__FUNCTION__);
    updateStatusBar();

    G::popUp->close();
}

void MW::setCachedStatus(QString fPath, bool isCached)
{
/*
    When an image is added or removed from the image cache in ImageCache a signal triggers
    this slot to update the datamodel cache status role. After the datamodel update the
    thumbView and gridView thumbnail is refreshed to update the cache badge.

    Note that the datamodel is used (dm), not the proxy (dm->sf). If the proxy is used and
    the user then sorts or filters the index could go out of range and the app will crash.

    Make sure the file path exists in the datamodel. The most likely failure will be if a new
    folder has been selected but the image cache has not been rebuilt.
*/
    if (G::isLogger) G::log(__FUNCTION__, fPath);
    QModelIndex idx = dm->proxyIndexFromPath(fPath);
    if (idx.isValid()) {
        dm->sf->setData(idx, isCached, G::CachedRole);
        if (isCached && idx.row() == currentRow) {
            imageView->loadImage(fPath, __FUNCTION__);
            updateClassification();
            centralLayout->setCurrentIndex(prevCentralView);
        }
        thumbView->refreshThumb(idx, G::CachedRole);
        gridView->refreshThumb(idx, G::CachedRole);
    }
    return;
}

void MW::insertThumbnails()
{
    if (G::isLogger) G::log(__FUNCTION__);
    QModelIndexList selection = selectionModel->selectedRows();
    thumb->insertThumbnails(selection);
}

void MW::searchTextEdit()
{
    if (G::isLogger) G::log(__FUNCTION__);
    // set visibility
    if (!filterDock->isVisible()) {
        filterDock->setVisible(true);
    }
    // set focus
    if (filterDock->visibleRegion().isEmpty()) {
        filterDock->raise();
    }
    // set menu status for filterDock in window menu
    filterDockVisibleAction->setChecked(true);

    // Goto item and edit
    filters->scrollToItem(filters->search);
    filters->expandItem(filters->search);
    filters->editItem(filters->searchTrue, 0);
    return;
}

void MW::updateClassification()
{
/*
    Each image in the datamodel can be assigned a variety of classifications:
        - picked
        - rating (1 - 5)
        - color class (some programs like lightroom call this "label"

    The classifications are combined in a badge (a circle pixmap).  This function updates
    the badge based on the values in the datamodel.

    The function is called when the user changes a classification and when a new folder
    is selected.  If the previous folder active image had a visible classification badge
    and then the user switches to a folder with no images or ejects the drive then make
    sure the classification label is not visible.
*/
    if (G::isLogger) G::log(__FUNCTION__);
    // check if still in a folder with images
    if (dm->rowCount() < 1) {
        imageView->classificationLabel->setVisible(false);
    }
    int row = thumbView->currentIndex().row();
    isPick = dm->sf->index(row, G::PickColumn).data(Qt::EditRole).toString() == "true";
    isReject = dm->sf->index(row, G::PickColumn).data(Qt::EditRole).toString() == "reject";
    rating = dm->sf->index(row, G::RatingColumn).data(Qt::EditRole).toString();
    colorClass = dm->sf->index(row, G::LabelColumn).data(Qt::EditRole).toString();
    if (rating == "0") rating = "";
    imageView->classificationLabel->setPick(isPick);
    imageView->classificationLabel->setReject(isReject);
    imageView->classificationLabel->setColorClass(colorClass);
    imageView->classificationLabel->setRating(rating);
    imageView->classificationLabel->setRatingColorVisibility(isRatingBadgeVisible);
    imageView->classificationLabel->refresh();

    if (G::mode == "Compare")
        compareImages->updateClassification(isPick, rating, colorClass,
                                            isRatingBadgeVisible,
                                            thumbView->currentIndex());
}

void MW::setRating()
{
/*
    Resolve the source menu action that called (could be any rating) and then set the rating
    for all the selected thumbs.
*/
    if (G::isLogger) G::log(__FUNCTION__);
    // do not set rating if slideshow is on
    if (G::isSlideShow) return;

    // make sure classification badges are visible
    if (!isRatingBadgeVisible) {
        ratingBadgeVisibleAction->setChecked(true);
        isRatingBadgeVisible = true;
        thumbView->refreshThumbs();
        gridView->refreshThumbs();
    }

    QObject* obj = sender();
    QString s = obj->objectName();
    if (s == "Rate0") rating = "";
    if (s == "Rate1") rating = "1";
    if (s == "Rate2") rating = "2";
    if (s == "Rate3") rating = "3";
    if (s == "Rate4") rating = "4";
    if (s == "Rate5") rating = "5";

    QModelIndexList selection = selectionModel->selectedRows();
    // check if selection is entirely rating already - if so set no rating
    bool isAlreadyRating = true;
    for (int i = 0; i < selection.count(); ++i) {
        QModelIndex idx = dm->sf->index(selection.at(i).row(), G::RatingColumn);
        if(idx.data(Qt::EditRole) != rating) {
            isAlreadyRating = false;
        }
    }
    if (isAlreadyRating) rating = "";     // invert the label(s)

    int n = selection.count();
    if (G::useSidecar) {
        G::popUp->setProgressVisible(true);
        G::popUp->setProgressMax(n + 1);
        QString txt = "Writing to XMP sidecar for " + QString::number(n) + " images." +
                      "<p>Press <font color=\"red\"><b>Esc</b></font> to abort.";
        G::popUp->showPopup(txt, 0, true, 1);
    }

    // set the rating in the datamodel
    for (int i = 0; i < n; ++i) {
        int row = selection.at(i).row();
        QModelIndex ratingIdx = dm->sf->index(row, G::RatingColumn);
        dm->sf->setData(ratingIdx, rating, Qt::EditRole);
        // update rating crash log
        QString fPath = dm->sf->index(row, G::PathColumn).data(G::PathRole).toString();
        updateRatingLog(fPath, rating);
        // check if combined raw+jpg and also set the rating for the hidden raw file
        if (combineRawJpg) {
            QModelIndex idx = dm->sf->index(selection.at(i).row(), 0);
            // is this part of a raw+jpg pair
            if(idx.data(G::DupIsJpgRole).toBool()) {
                // set rating for raw file row as well
                QModelIndex rawIdx = qvariant_cast<QModelIndex>(idx.data(G::DupOtherIdxRole));
                row = rawIdx.row();
                ratingIdx = dm->index(row, G::RatingColumn);
                dm->setData(ratingIdx, rating, Qt::EditRole);
                // update rating crash log
                QString jpgPath  = dm->sf->index(row, G::PathColumn).data(G::PathRole).toString();
                updateRatingLog(jpgPath, rating);
            }
        }
        // write to sidecar
        if (G::useSidecar) {
            dm->imMetadata(fPath, true);    // true = update metadata->m struct for image
            metadata->writeXMP(metadata->sidecarPath(fPath), __FUNCTION__);
            G::popUp->setProgress(i+1);
        }
    }

    thumbView->refreshThumbs();
    gridView->refreshThumbs();

    // update ImageView classification badge
    updateClassification();

    // refresh the filter
    dm->sf->filterChange();

    // update filter counts
    buildFilters->updateCountFiltered();    

    if (G::useSidecar) {
        G::popUp->setProgressVisible(false);
        G::popUp->hide();
    }
}

int MW::ratingLogCount()
{
    if (G::isLogger) G::log(__FUNCTION__);
    setting->beginGroup("RatingLog");
    int count = setting->allKeys().size();
    setting->endGroup();
    return count;
}

void MW::recoverRatingLog()
{
    if (G::isLogger) G::log(__FUNCTION__);
    setting->beginGroup("RatingLog");
    QStringList keys = setting->allKeys();
    for (int i = 0; i < keys.length(); ++i) {
        QString fPath = keys.at(i);
        fPath.replace("🔸", "/");
        QString pickStatus = setting->value(keys.at(i)).toString();
        QModelIndex idx = dm->proxyIndexFromPath(fPath);
        if (idx.isValid()) {
            QModelIndex ratingIdx = dm->sf->index(idx.row(), G::RatingColumn);
            dm->sf->setData(ratingIdx, pickStatus, Qt::EditRole);
//            qDebug() << __FUNCTION__ << pickStatus << fPath << "updated";
        }
        else {
//            qDebug() << __FUNCTION__ << fPath << "not found";
        }
    }
    setting->endGroup();
    thumbView->refreshThumbs();
    gridView->refreshThumbs();
}

void MW::clearRatingLog()
{
    if (G::isLogger) G::log(__FUNCTION__);
    setting->beginGroup("RatingLog");
    QStringList keys = setting->allKeys();
    for (int i = 0; i < keys.length(); ++i) {
        setting->remove(keys.at(i));
    }
    setting->endGroup();
}

void MW::updateRatingLog(QString fPath, QString rating)
{
    if (G::isLogger) G::log(__FUNCTION__);
    setting->beginGroup("RatingLog");
    QString sKey = fPath;
    sKey.replace("/", "🔸");
    if (rating == "") {
        /*
        qDebug() << __FUNCTION__ << "removing" << sKey;
        //*/
        setting->remove(sKey);
    }
    else {
        /*
        qDebug() << __FUNCTION__ << "adding" << sKey;
        //*/
        setting->setValue(sKey, rating);
    }
    setting->endGroup();
}

void MW::setColorClass()
{
/*
    Resolve the source menu action that called (could be any color class) and then set the
    color class for all the selected thumbs.
*/
    if (G::isLogger) G::log(__FUNCTION__); 
    // do not set color class if slideshow is on
    if (G::isSlideShow) return;

    // make sure classification badges are visible
    if (!isRatingBadgeVisible) {
        ratingBadgeVisibleAction->setChecked(true);
        isRatingBadgeVisible = true;
        thumbView->refreshThumbs();
        gridView->refreshThumbs();
    }

    QObject* obj = sender();
    QString s = obj->objectName();
    if (s == "Label0") colorClass = "";
    if (s == "Label1") colorClass = "Red";
    if (s == "Label2") colorClass = "Yellow";
    if (s == "Label3") colorClass = "Green";
    if (s == "Label4") colorClass = "Blue";
    if (s == "Label5") colorClass = "Purple";

    QModelIndexList selection = selectionModel->selectedRows();
    // check if selection is entirely label color already - if so set no label
    bool isAlreadyLabel = true;
    for (int i = 0; i < selection.count(); ++i) {
        QModelIndex idx = dm->sf->index(selection.at(i).row(), G::LabelColumn);
        if(idx.data(Qt::EditRole) != colorClass) {
            isAlreadyLabel = false;
        }
    }
    if(isAlreadyLabel) colorClass = "";     // invert the label

    int n = selection.count();
    if (G::useSidecar) {
        G::popUp->setProgressVisible(true);
        G::popUp->setProgressMax(n + 1);
        QString txt = "Writing to XMP sidecar for " + QString::number(n) + " images." +
                      "<p>Press <font color=\"red\"><b>Esc</b></font> to abort.";
        G::popUp->showPopup(txt, 0, true, 1);
    }

    // update the data model
    for (int i = 0; i < n; ++i) {
        int row = selection.at(i).row();
        QModelIndex labelIdx = dm->sf->index(row, G::LabelColumn);
        dm->sf->setData(labelIdx, colorClass, Qt::EditRole);
        // update color class crash log
        QString fPath = dm->sf->index(row, G::PathColumn).data(G::PathRole).toString();
        updateColorClassLog(fPath, colorClass);
        // check if combined raw+jpg and also set the rating for the hidden raw file
        if (combineRawJpg) {
            QModelIndex idx = dm->sf->index(selection.at(i).row(), 0);
            // is this part of a raw+jpg pair
            if(idx.data(G::DupIsJpgRole).toBool()) {
                // set color class (label) for raw file row as well
                QModelIndex rawIdx = qvariant_cast<QModelIndex>(idx.data(G::DupOtherIdxRole));
                row = rawIdx.row();
                labelIdx = dm->index(row, G::LabelColumn);
                dm->setData(labelIdx, colorClass, Qt::EditRole);
                // update color class crash log
                QString jpgPath = dm->sf->index(row, G::PathColumn).data(G::PathRole).toString();
                updateColorClassLog(jpgPath, colorClass);
            }
        }
        // write to sidecar
        if (G::useSidecar) {
            dm->imMetadata(fPath, true);    // true = update metadata->m struct for image
            metadata->writeXMP(metadata->sidecarPath(fPath), __FUNCTION__);
            G::popUp->setProgress(i+1);
        }
    }

    thumbView->refreshThumbs();
    gridView->refreshThumbs();
    tableView->resizeColumnToContents(G::LabelColumn);

    // update ImageView classification badge
    updateClassification();

    // refresh the filter
    dm->sf->filterChange();

    // update filter counts
    buildFilters->updateCountFiltered();
    dm->sf->filterChange();

    if (G::useSidecar) {
        G::popUp->setProgressVisible(false);
        G::popUp->hide();
    }
}

int MW::colorClassLogCount()
{
    if (G::isLogger) G::log(__FUNCTION__);
    setting->beginGroup("ColorClassLog");
    int count = setting->allKeys().size();
    setting->endGroup();
    return count;
}

void MW::recoverColorClassLog()
{
    if (G::isLogger) G::log(__FUNCTION__);
    setting->beginGroup("ColorClassLog");
    QStringList keys = setting->allKeys();
    for (int i = 0; i < keys.length(); ++i) {
        QString fPath = keys.at(i);
        fPath.replace("🔸", "/");
        QString colorClassStatus = setting->value(keys.at(i)).toString();
        QModelIndex idx = dm->proxyIndexFromPath(fPath);
        if (idx.isValid()) {
            QModelIndex colorClassIdx = dm->sf->index(idx.row(), G::LabelColumn);
            dm->sf->setData(colorClassIdx, colorClassStatus, Qt::EditRole);
//            qDebug() << __FUNCTION__ << colorClassStatus << fPath << "updated";
        }
        else {
//            qDebug() << __FUNCTION__ << fPath << "not found";
        }
    }
    setting->endGroup();
    thumbView->refreshThumbs();
    gridView->refreshThumbs();
}

void MW::clearColorClassLog()
{
    if (G::isLogger) G::log(__FUNCTION__);
    setting->beginGroup("ColorClassLog");
    QStringList keys = setting->allKeys();
    for (int i = 0; i < keys.length(); ++i) {
        setting->remove(keys.at(i));
    }
    setting->endGroup();
}

void MW::updateColorClassLog(QString fPath, QString label)
{
    if (G::isLogger) G::log(__FUNCTION__);
    setting->beginGroup("ColorClassLog");
    QString sKey = fPath;
    sKey.replace("/", "🔸");
    if (label == "") {
//        qDebug() << __FUNCTION__ << "removing" << sKey;
        setting->remove(sKey);
    }
    else {
//        qDebug() << __FUNCTION__ << "adding" << sKey;
        setting->setValue(sKey, label);
    }
    setting->endGroup();
}

void MW::metadataChanged(QStandardItem* item)
{
/*
    This slot is called when there is a data change in InfoView.

    If the change was a result of a new image selection then ignore.

    If the metadata in the tags section of the InfoView panel has been editied (title,
    creator, copyright, email or url) then all selected image tag(s) are updated to the new
    value(s) in the data model. If xmp edits are enabled the new tag(s) are embedded in the
    image metadata, either internally or as a sidecar when ingesting. If raw+jpg are combined
    then the raw file rows are also updated in the data model.
*/
     if (!useInfoView) return;
    if (G::isLogger) G::log(__FUNCTION__);
    // if new folder is invalid no relevent data has changed
    if(!isCurrentFolderOkay) return;
     if (useInfoView) if (infoView->isNewImageDataChange) return;

    QModelIndex par = item->index().parent();
     if (useInfoView) if (par != infoView->tagInfoIdx) return;

    QString tagValue = item->data(Qt::DisplayRole).toString();
    QModelIndexList selection = selectionModel->selectedRows();
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

    for (int i = 0; i < selection.count(); ++i) {
        int row = selection.at(i).row();
        // build list of files to send to Metadata::writeMetadata
        paths << dm->sf->index(row, G::PathColumn).data().toString();
        // update data model
        QModelIndex idx = dm->sf->index(row, col[tagName]);
        dm->sf->setData(idx, tagValue, Qt::EditRole);
        // check if combined raw+jpg and also set the tag item for the hidden raw file
        if (combineRawJpg) {
            QModelIndex idx = dm->sf->index(selection.at(i).row(), 0);
            // is this part of a raw+jpg pair
            if(idx.data(G::DupIsJpgRole).toBool()) {
                // set tag item for raw file row as well
                QModelIndex rawIdx = qvariant_cast<QModelIndex>(idx.data(G::DupOtherIdxRole));
                idx = dm->index(rawIdx.row(), col[tagName]);
                dm->setData(idx, tagValue, Qt::EditRole);
            }
        }
    }

    // update shooting info
    imageView->updateShootingInfo();
}

void MW::getSubfolders(QString fPath)
{
    if (G::isLogger) G::log(__FUNCTION__);
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

void MW::enableGoKeyActions(bool ok)
{
    if (G::isLogger) G::log(__FUNCTION__);
    if (ok) {
        keyRightAction->setEnabled(true);
        keyLeftAction->setEnabled(true);
        keyUpAction->setEnabled(true);
        keyDownAction->setEnabled(true);
        keyHomeAction->setEnabled(true);
        keyEndAction->setEnabled(true);
        keyPageUpAction->setEnabled(true);
        keyPageDownAction->setEnabled(true);
    }
    else {
        keyRightAction->setEnabled(false);
        keyLeftAction->setEnabled(false);
        keyUpAction->setEnabled(false);
        keyDownAction->setEnabled(false);
        keyHomeAction->setEnabled(false);
        keyEndAction->setEnabled(false);
        keyPageUpAction->setEnabled(false);
        keyPageDownAction->setEnabled(false);
    }
}

void MW::keyRight()
{
/*

*/
    if (G::isLogger) G::log(__FUNCTION__);
    if (G::mode == "Compare") compareImages->go("Right");
    else thumbView->selectNext();
}

void MW::keyLeft()
{
/*

*/
    if (G::isLogger) G::log(__FUNCTION__);
    if (G::mode == "Compare") compareImages->go("Left");
    else thumbView->selectPrev();
}

void MW::keyUp()
{
/*

*/
    if (G::isLogger) G::log(__FUNCTION__);
    if (G::mode == "Loupe") thumbView->selectUp();
    if (G::mode == "Table") thumbView->selectUp();
    if (G::mode == "Grid") gridView->selectUp();
}

void MW::keyDown()
{
/*

*/
    if (G::isLogger) G::log(__FUNCTION__);
    if (G::mode == "Loupe") thumbView->selectDown();
    if (G::mode == "Table") thumbView->selectDown();
    if (G::mode == "Grid") gridView->selectDown();
}

void MW::keyPageUp()
{
    if (G::isLogger) G::log(__FUNCTION__);
    if (G::mode == "Loupe") thumbView->selectPageUp();
    if (G::mode == "Table") tableView->selectPageUp();
    if (G::mode == "Grid") gridView->selectPageUp();
}

void MW::keyPageDown()
{
    if (G::isLogger) G::log(__FUNCTION__);
    if (G::mode == "Loupe") thumbView->selectPageDown();
    if (G::mode == "Table") tableView->selectPageDown();
    if (G::mode == "Grid") gridView->selectPageDown();
}

void MW::keyHome()
{
/*

*/
    if (G::isLogger) G::log(__FUNCTION__);
    if (G::isNewFolderLoaded /*&& !G::isInitializing*/) {
        if (G::mode == "Compare") compareImages->go("Home");
        if (G::mode == "Grid") gridView->selectFirst();
        else {
            thumbView->selectFirst();
        }
    }
}

void MW::keyEnd()
{
/*

*/
    if (G::isLogger) G::log(__FUNCTION__);
    if (G::isNewFolderLoaded /*&& !G::isInitializing*/) {
        metadataCacheThread->stopMetadataCache();
        if (G::mode == "Compare") compareImages->go("End");
        if (G::mode == "Grid") gridView->selectLast();
        else {
            thumbView->selectLast();
        }
    }
}

void MW::keyScrollDown()
{
    if (G::isLogger) G::log(__FUNCTION__);
    if(G::mode == "Grid") gridView->scrollDown(0);
    if(thumbView->isVisible()) thumbView->scrollDown(0);
}

void MW::keyScrollUp()
{
    if (G::isLogger) G::log(__FUNCTION__);
    if(G::mode == "Grid") gridView->scrollUp(0);
    if(thumbView->isVisible()) thumbView->scrollUp(0);
}

void MW::keyScrollPageDown()
{
    if (G::isLogger) G::log(__FUNCTION__);
    if(G::mode == "Grid") gridView->scrollPageDown(0);
    if(thumbView->isVisible()) thumbView->scrollPageDown(0);
}

void MW::keyScrollPageUp()
{
    if (G::isLogger) G::log(__FUNCTION__);
    if(G::mode == "Grid") gridView->scrollPageUp(0);
    if(thumbView->isVisible()) thumbView->scrollPageUp(0);
}

void MW::stressTest(int ms)
{
    if (G::isLogger) G::log(__FUNCTION__);
    qDebug() << __FUNCTION__ << ms;
    G::wait(1000);        // time to release modifier keys for shortcut (otherwise select many)
    isStressTest = true;
    bool isForward = true;
    int count = 0;
    QElapsedTimer t;
    t.start();
    qDebug() << __FUNCTION__ << "-1";
    while (isStressTest) {
        G::wait(ms);
        ++count;
        if (isForward && currentRow == dm->sf->rowCount() - 1) isForward = false;
        if (!isForward && currentRow == 0) isForward = true;
        if (isForward) keyRight();
        else keyLeft();
    }
    qint64 msElapsed = t.elapsed();
    double seconds = msElapsed * 0.001;
    double msPerImage = msElapsed * 1.0 / count;
    int imagePerSec = count * 1.0 / seconds;
    qDebug() << __FUNCTION__ << "Executed stress test" << count << "times.  "
             << msElapsed << "ms elapsed  "
             << ms << "ms delay  "
             << imagePerSec << "images per second  "
             << msPerImage << "ms per image."
                ;
    return;
    if (G::isLogger) G::log(__FUNCTION__);
    getSubfolders("/users/roryhill/pictures");
    QString fPath;
    int r = static_cast<int>(QRandomGenerator::global()->generate());
    fPath = subfolders->at(r % (subfolders->count()));
}

void MW::slideShow()
{
    if (G::isLogger) G::log(__FUNCTION__);
    if (G::isSlideShow) {
        // stop slideshow
        G::popUp->showPopup("Slideshow has been terminated.");
        G::isSlideShow = false;
        useImageCache = true;
        imageView->setCursor(Qt::ArrowCursor);
        slideShowStatusLabel->setText("");
        updateStatus(true, "", __FUNCTION__);
        updateStatusBar();
        slideShowAction->setText(tr("Slide Show"));
        slideShowTimer->stop();
        delete slideShowTimer;
        cacheProgressBar->setVisible(true);
        // change to ImageCache
        if (useImageCache) imageCacheThread->setCurrentPosition(dm->currentFilePath);
        // enable main window QAction shortcuts
        QList<QAction*> actions = findChildren<QAction*>();
        for (QAction *a : actions) a->setShortcutContext(Qt::WindowShortcut);
    }
    else {
        // start slideshow
        imageView->setCursor(Qt::BlankCursor);
        G::isSlideShow = true;
        isSlideshowPaused = false;
        updateStatusBar();
        QString msg = "<h2>Press <font color=\"red\"><b>Esc</b></font> to exit slideshow</h2><p>";
        msg += "Press <font color=\"red\"><b>H</b></font> during slideshow for tips"
                      "<p>Starting slideshow";
        msg += "<p>Current settings:<p>";
        msg += "Interval = " + QString::number(slideShowDelay) + " second(s)";
        if (isSlideShowRandom)  msg += "<br>Random selection";
        else msg += "<br>Sequential selection";
        if (isSlideShowWrap) msg += "<br>Wrap at end of slides";
        else msg += "<br>Stop at end of slides";

        G::popUp->showPopup(msg, 4000, true, 0.75, Qt::AlignLeft);

        // No image caching if random slide show
        if (isSlideShowRandom) useImageCache = false;
        else useImageCache = true;

        // disable main window QAction shortcuts
        QList<QAction*> actions = findChildren<QAction*>();
        for (QAction *a : actions) a->setShortcutContext(Qt::WidgetShortcut);

        if (isStressTest) getSubfolders("/users/roryhill/pictures");

        slideShowAction->setText(tr("Stop Slide Show"));
        slideShowTimer = new QTimer(this);
        connect(slideShowTimer, SIGNAL(timeout()), this, SLOT(nextSlide()));
        slideCount = 0;
        nextSlide();
        slideShowTimer->start(slideShowDelay * 1000);
    }
}

void MW::nextSlide()
{
    if (G::isLogger) G::log(__FUNCTION__);
    if (isSlideshowPaused) return;
    slideCount++;
    if (isSlideShowRandom) {
        // push previous image path onto the slideshow history stack
        int row = thumbView->currentIndex().row();
        QString fPath = dm->sf->index(row, 0).data(G::PathRole).toString();
        slideshowRandomHistoryStack->push(fPath);
        thumbView->selectRandom();
    }
    else {
        if (currentRow == dm->sf->rowCount() - 1) {
            if (isSlideShowWrap) thumbView->selectFirst();
            else slideShow();
        }
        else thumbView->selectNext();
    }

    QString msg = "  Slide # "+ QString::number(slideCount) + "  (press H for slideshow shortcuts)";
    updateStatus(true, msg, __FUNCTION__);

}

void MW::prevRandomSlide()
{
    if (G::isLogger) G::log(__FUNCTION__);
    if (slideshowRandomHistoryStack->isEmpty()) {
        G::popUp->showPopup("End of random slide history");
        return;
    }
    isSlideshowPaused = true;
    QString prevPath = slideshowRandomHistoryStack->pop();
    thumbView->selectThumb(prevPath);
    updateStatus(false,
                 "Slideshow random history."
                 "  Press <font color=\"white\"><b>Spacebar</b></font> to continue slideshow, "
                 "press <font color=\"white\"><b>Esc</b></font> to quit slideshow."
                 , __FUNCTION__);
    // hide popup if showing
    G::popUp->hide();
}

void MW::slideShowResetDelay()
{
    if (G::isLogger) G::log(__FUNCTION__);
    slideShowTimer->setInterval(slideShowDelay * 1000);
}

void MW::slideShowResetSequence()
{
/*
    Called from MW::keyReleaseEvent when R is pressed and isSlideShow == true.
    The slideshow is toggled between sequential and random progress.
*/
    if (G::isLogger) G::log(__FUNCTION__);
    QString msg = "Setting slideshow progress to ";
    if (isSlideShowRandom) {
        msg += "random";
        progressLabel->setVisible(false);
    }
    else {
        msg = msg + "sequential";
        progressLabel->setVisible(true);
    }
    G::popUp->showPopup(msg);
}

void MW::slideshowHelpMsg()
{
    if (G::isLogger) G::log(__FUNCTION__);
    QString selection;
    if (isSlideShowRandom)  selection = "Random selection";
    else selection = "Sequential selection";
    QString wrap;
    if (isSlideShowWrap)  wrap = "Wrap at end of slides";
    else wrap = "Stop at end of slides";
    QString msg =
        "<p><b>Slideshow Shortcuts:</b><br/></p>"
        "<table border=\"0\" style=\" margin-top:0px; margin-bottom:0px; margin-left:0px; margin-right:0px;\" cellspacing=\"2\" cellpadding=\"0\">"
        "<tr><td width=\"120\"><font color=\"red\"><b>Esc</b></font></td><td>Exit slideshow</td></tr>"
        "<tr><td><font color=\"red\"><b>  W       </b></font></td><td>Toggle wrapping on and off.</td></tr>"
        "<tr><td><font color=\"red\"><b>  R       </b></font></td><td>Toggle random vs sequential slide selection.</td></tr>"
        "<tr><td><font color=\"red\"><b>Backspace </b></font></td><td>Go back to a previous random slide</td></tr>"
        "<tr><td><font color=\"red\"><b>Spacebar  </b></font></td><td>Pause/Continue slideshow</td></tr>"
        "<tr><td><font color=\"red\"><b>  H       </b></font></td><td>Show this popup message</td></tr>"
        "</table>"
        "<p>Change the slideshow interval.  Go to preferences to set other interval.</p>"
        "<table border=\"0\" style=\" margin-top:0px; margin-bottom:0px; margin-left:0px; margin-right:0px;\" cellspacing=\"2\" cellpadding=\"0\">"
        "<tr><td width=\"120\"><font color=\"red\"><b>1</b></font></td><td>1 second</td></tr>"
        "<tr><td><font color=\"red\"><b>  2  </b></font></td><td>2 seconds</td></tr>"
        "<tr><td><font color=\"red\"><b>  3  </b></font></td><td>3 seconds</td></tr>"
        "<tr><td><font color=\"red\"><b>  4  </b></font></td><td>5 seconds</td></tr>"
        "<tr><td><font color=\"red\"><b>  5  </b></font></td><td>10 seconds</td></tr>"
        "<tr><td><font color=\"red\"><b>  6  </b></font></td><td>30 seconds</td></tr>"
        "<tr><td><font color=\"red\"><b>  7  </b></font></td><td>1 minute</td></tr>"
        "<tr><td><font color=\"red\"><b>  8  </b></font></td><td>3 minutes</td></tr>"
        "<tr><td><font color=\"red\"><b>  9  </b></font></td><td>10 minutes</td></tr>"
        "</table>"
        "<p>Current settings:<p>"
        "<ul style=\"line-height:50%; list-style-type:none;\""
        "<li>Interval  = "  + QString::number(slideShowDelay) + " seconds</li>"
        "<li>Selection = " + selection + "</li>"
        "<li>Wrap      = " + wrap + "</li>"
        "</ul><p><p>"
        "Press <font color=\"red\"><b>Space Bar</b></font> continue slideshow and close this message";
    G::popUp->showPopup(msg, 0, true, 1.0, Qt::AlignLeft);
}

void MW::dropOp(Qt::KeyboardModifiers keyMods, bool dirOp, QString cpMvDirPath)
{
    if (G::isLogger) G::log(__FUNCTION__);
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

    if (destDir == 	currentViewDirPath) {
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
    if (G::isLogger) G::log(__FUNCTION__);
    QModelIndex idx = fsTree->fsModel->index(currentViewDirPath);
    if (idx.isValid()){
        fsTree->expand(idx);
        fsTree->setCurrentIndex(idx);
    }
}

void MW::wheelEvent(QWheelEvent *event)
{
    // rgh ??
    if (G::isLogger) G::log(__FUNCTION__);
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
    if (G::isLogger) G::log(__FUNCTION__);
    QMessageBox msgBox;
    msgBox.warning(parent, tr("Warning"), tr("Cannot perform action with temporary image."));
}

void MW::addNewBookmarkFromMenu()
{
    if (G::isLogger) G::log(__FUNCTION__);
    addBookmark(currentViewDirPath);
}

void MW::addNewBookmarkFromContextMenu()
{
    if (G::isLogger) G::log(__FUNCTION__);
    addBookmark(mouseOverFolder);
}

void MW::addBookmark(QString path)
{
    if (G::isLogger) G::log(__FUNCTION__);
    bookmarks->bookmarkPaths.insert(path);
    bookmarks->reloadBookmarks();
}

void MW::openFolder()
{
    if (G::isLogger) G::log(__FUNCTION__);
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
    if (G::isLogger) G::log(__FUNCTION__);
    isRefreshingDM = true;
    refreshCurrentPath = dm->sf->index(currentRow, 0).data(G::PathRole).toString();
    if (dm->hasFolderChanged() && dm->modifiedFiles.count()) {
        for (int i = 0; i < dm->modifiedFiles.count(); ++i) {
            QString fPath = dm->modifiedFiles.at(i).filePath();
            if (!dm->fPathRow.contains(fPath)) continue;
            int dmRow = dm->fPathRow[fPath];
            int sfRow = dm->sf->mapFromSource(dm->index(dmRow, 0)).row();
            // update file size and modified date
            dm->updateFileData(dm->modifiedFiles.at(i));

            // update metadata
            QString ext = dm->modifiedFiles.at(i).suffix().toLower();
            if (metadata->getMetadataFormats.contains(ext)) {
                if (metadata->loadImageMetadata(dm->modifiedFiles.at(i), true, true, false, true, __FUNCTION__)) {
                    metadata->m.row = dmRow;
                    dm->addMetadataForItem(metadata->m);
                }
            }

            // update image cache in case image has changed
            if (icd->imCache.contains(fPath)) icd->imCache.remove(fPath);
            if (dm->currentFilePath == fPath) {
                if (imageView->loadImage(fPath, __FUNCTION__)) {
                    updateClassification();
                }
            }

            // update thumbnail in case image has changed
            QImage image;
            bool thumbLoaded = thumb->loadThumb(fPath, image, "MW::refreshCurrentFolder");
            if (thumbLoaded) {
                QPixmap pm = QPixmap::fromImage(image.scaled(G::maxIconSize, G::maxIconSize, Qt::KeepAspectRatio));
                dm->itemFromIndex(dm->index(dmRow, 0))->setIcon(pm);
            }
        }
         if (useInfoView) infoView->updateInfo(currentRow);
//        metadataCacheThread->loadNewFolder(true);
        refreshCurrentAfterReload();
    }
    else folderSelectionChange();
}

void MW::refreshCurrentAfterReload()
{
/*
    This slot is triggered after the metadataCache thread has run and isRefreshingDM = true to
    complete the refresh current folder process by selecting the previously selected thumb.
*/
    if (G::isLogger) G::log(__FUNCTION__);
    int dmRow = 0;
    if (dm->fPathRow.contains(refreshCurrentPath))
        dmRow = dm->fPathRow[refreshCurrentPath];
    int sfRow = dm->sf->mapFromSource(dm->index(dmRow, 0)).row();
    /*
    qDebug() << __FUNCTION__ << refreshCurrentPath
             << "dmRow =" << dmRow
             << "sfRow =" << sfRow;
//             */
    thumbView->iconViewDelegate->currentRow = sfRow;
    gridView->iconViewDelegate->currentRow = sfRow;
    thumbView->selectThumb(sfRow);
    isRefreshingDM = false;
}

void MW::saveAsFile()
{
    if (G::isLogger) G::log(__FUNCTION__);
    QModelIndexList selection = selectionModel->selectedRows();
    if (selection.length() == 0) {
        G::popUp->showPopup("No images selected for save as operation", 1500);
        return;
    }
    saveAsDlg = new SaveAsDlg(selection, metadata, dm);
    saveAsDlg->setStyleSheet(css);
    saveAsDlg->exec();
}

void MW::copy()
{
    if (G::isLogger) G::log(__FUNCTION__);
    thumbView->copyThumbs();
}

void MW::deleteFiles()
{
/*
    Build a QStringList of the selected files, delete from disk, remove from datamodel, remove
    from ImageCache and update the image cache status bar.
*/
    if (G::isLogger) G::log(__FUNCTION__);
    if (deleteWarning) {
        QMessageBox msgBox;
        int msgBoxWidth = 300;
        msgBox.setWindowTitle("Delete Images");
        msgBox.setText("This operation will delete all selected images.");
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

    QModelIndexList selection = thumbView->selectionModel()->selectedRows();
    if (selection.isEmpty()) return;

    /* save the index to the first row in selection (order depends on how selection was
       made) to insure the correct index is selected after deletion.  */
    int lowRow = 999999;
    for (int i = 0; i < selection.count(); ++i) {
        if (selection.at(i).row() < lowRow) {
            selectionModel->setCurrentIndex(selection.at(i), QItemSelectionModel::Current);
            lowRow = selection.at(i).row();
        }
    }

    // convert selection to stringlist
    QStringList sl, sldm, slDir;
    for (int i = 0; i < selection.count(); ++i) {
        QString fPath = selection.at(i).data(G::PathRole).toString();
        sl.append(fPath);
        QFileInfo info(fPath);
        QString dirPath = info.dir().path();
        if (!slDir.contains(dirPath)) slDir.append(dirPath);
    }
    thumbView->selectionModel()->clearSelection();

    // delete file(s) in folder on disk, including any xmp sidecars
    for (int i = 0; i < sl.count(); ++i) {
        QString fPath = sl.at(i);
        QString sidecarPath = metadata->sidecarPath(fPath);
        if (QFile::remove(fPath)) {
            sldm.append(fPath);
        }
        if (QFile(sidecarPath).exists()) {
            QFile::remove(sidecarPath);
        }
    }

    // refresh image count in folders and bookmarks
    fsTree->updateFolderImageCount(currentViewDirPath);
    bookmarks->count();

    // if all images in folder were deleted
    if (sldm.count() == dm->rowCount()) {
        stopAndClearAll();
        folderSelectionChange();
        return;
    }

    // remove fPath from datamodel dm if successfully deleted
    for (int i = 0; i < sldm.count(); ++i) {
        QString fPath = sldm.at(i);
        dm->remove(fPath);
    }

    // refresh datamodel fPathRow hash
    dm->refreshRowFromPath();

    // remove selected from imageCache
    imageCacheThread->removeFromCache(sldm);

    // update cursor position on progressBar
    imageCacheThread->updateStatus("Update all rows", __FUNCTION__);

    // update current index
    if (lowRow >= dm->sf->rowCount()) lowRow = dm->sf->rowCount() - 1;
    QModelIndex sfIdx = dm->sf->index(lowRow, 0);
    thumbView->setCurrentIndex(sfIdx);
    fileSelectionChange(sfIdx, sfIdx);
}

void MW::deleteFolder()
{
    QString dirToDelete;
    QString senderObject = (static_cast<QAction*>(sender()))->objectName();
    if (senderObject == "deleteActiveFolder") {
        dirToDelete = currentViewDirPath;
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
        msgBox.setText("This operation will delete the folder<br>"
                       "<font color=\"red\"><b>" + dirToDelete +
                       "</b></font>");
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

    if (currentViewDirPath == dirToDelete) {
        ignoreFolderSelectionChange = true;
        stopAndClearAll();
    }

    QDir dir(dirToDelete);
    dir.removeRecursively();

    if (bookmarks->bookmarkPaths.contains(dirToDelete)) {
        bookmarks->bookmarkPaths.remove(dirToDelete);
        bookmarks->reloadBookmarks();
    }
}

void MW::openUsbFolder()
{
/*

*/
    if (G::isLogger) G::log(__FUNCTION__);
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

    }

    refreshFolders();
    bookmarks->selectionModel()->clear();
    bool wasSubFoldersChecked = subFoldersAction->isChecked();
    if (!wasSubFoldersChecked) subFoldersAction->setChecked(true);
    updateStatusBar();
    QString fPath = usbMap[drive].rootPath;
    fsTree->select(fPath);
    isCurrentFolderOkay = isFolderValid(fPath, true, false);
    if (isCurrentFolderOkay) {
        QModelIndex idx = fsTree->fsModel->index(fPath);
        QModelIndex filterIdx = fsTree->fsFilter->mapFromSource(idx);
        fsTree->setCurrentIndex(filterIdx);
        fsTree->scrollTo(filterIdx, QAbstractItemView::PositionAtCenter);
        folderSelectionChange();
        thumbView->selectThumb(0);
        if (!wasSubFoldersChecked) subFoldersAction->setChecked(false);
        updateStatusBar();
    }
    else {
        setWindowTitle(winnowWithVersion);
    }
}

void MW::copyFolderPathFromContext()
{
    if (G::isLogger) G::log(__FUNCTION__);
    QApplication::clipboard()->setText(mouseOverFolder);
    QString msg = "Copied " + mouseOverFolder + " to the clipboard";
    G::popUp->showPopup(msg, 1500);
}

void MW::revealLogFile()
{
    if (G::isLogger) G::log(__FUNCTION__);

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
    if (G::isLogger) G::log(__FUNCTION__);
    QString dirPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    dirPath += "/Winnets";
    revealInFileBrowser(dirPath);
}

void MW::revealFile()
{
    if (G::isLogger) G::log(__FUNCTION__);
    QString fPath = dm->sf->index(currentRow, 0).data(G::PathRole).toString();
    revealInFileBrowser(fPath);
}

void MW::revealFileFromContext()
{
    if (G::isLogger) G::log(__FUNCTION__);
    revealInFileBrowser(mouseOverFolder);
}

void MW::revealInFileBrowser(QString path)
{
/*
    See http://stackoverflow.com/questions/3490336/how-to-reveal-in-finder-or-show-in-explorer-with-qt
    for details
*/
    if (G::isLogger) G::log(__FUNCTION__);
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
    if (G::isLogger) G::log(__FUNCTION__);
    fsTree->collapseAll();
}

void MW::openInFinder()
{
    if (G::isLogger) G::log(__FUNCTION__);
    takeCentralWidget();
    setCentralWidget(imageView);
}

void MW::openInExplorer()
{
    if (G::isLogger) G::log(__FUNCTION__);
    QString path = "C:/exampleDir/example.txt";

    QStringList args;

    args << "/select," << QDir::toNativeSeparators(path);

    QProcess *process = new QProcess(this);
    process->start("explorer.exe", args);
}

bool MW::isFolderValid(QString dirPath, bool report, bool isRemembered)
{
    if (G::isLogger) G::log(__FUNCTION__, dirPath);
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
            msg = "The folder (" + dirPath + ") is not readable.  Perhaps it was a USB drive that is not currently mounted or that has been ejected.";
            statusLabel->setText("");
            setCentralMessage(msg);
        }
        return false;
    }

    return true;
}

void MW::generateMeanStack()
{
    if (G::isLogger) G::log(__FUNCTION__);
    QStringList selection;
    if (!getSelection(selection)) return;
    meanStack = new Stack(selection, dm, metadata, icd);
    connect(this, &MW::abortStackOperation, meanStack, &Stack::stop);
    QString fPath = meanStack->mean();
    if (fPath != "") {
        dm->insert(fPath);
        imageCacheThread->rebuildImageCacheParameters(fPath, __FUNCTION__);
        if (G::mode == "Grid") {
            gridView->selectThumb(fPath);
        }
        else {
            thumbView->selectThumb(fPath);
        }
    }
}

void MW::reportHueCount()
{
    if (G::isLogger) G::log(__FUNCTION__);

    QStringList selection;
    if (!getSelection(selection)) return;
    ColorAnalysis hueReport;
    connect(this, &MW::abortHueReport, &hueReport, &ColorAnalysis::abortHueReport);
    hueReport.process(selection);
}

void MW::mediaReadSpeed()
{
    if (G::isLogger) G::log(__FUNCTION__);
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

void MW::createMessageView()
{
    if (G::isLogger) G::log(__FUNCTION__);
    messageView = new QWidget;
    msg.setupUi(messageView);
//    Ui::message ui;
//    ui.setupUi(messageView);
}

void MW::setCentralMessage(QString message)
{
    if (G::isLogger) G::log(__FUNCTION__);
    msg.msgLabel->setText(message);
    centralLayout->setCurrentIndex(MessageTab);
}

void MW::help()
{
    if (G::isLogger) G::log(__FUNCTION__);
    QWidget *helpDoc = new QWidget;
    Ui::helpForm ui;
    ui.setupUi(helpDoc);
    helpDoc->show();
}

void MW::helpShortcuts()
{
    if (G::isLogger) G::log(__FUNCTION__);
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
    if (G::isLogger) G::log(__FUNCTION__);
    centralLayout->setCurrentIndex(StartTab);
}

void MW::testNewFileFormat()    // shortcut = "Shift+Ctrl+Alt+F"
{
    zoomDlg->setWindowFlags(windowFlags() & ~Qt::WindowStaysOnTopHint);
    zoomDlg->show();
    return;

    QString fPath = "D:/Pictures/_DNG/DngNikonD850FromLightroom.dng";
    metadata->testNewFileFormat(fPath);
}

void MW::test() // shortcut = "Shift+Ctrl+Alt+T"
{
        stressTest(50);
//    qDebug() << __FUNCTION__ << G::ingestCount << G::ingestLastSeqDate;
}
// End MW
