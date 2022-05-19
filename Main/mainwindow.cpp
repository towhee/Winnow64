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

Flow by function call:

    MW::folderSelectionChange
    MW::stopAndClearAll
    DataModel::clearDataModel
    DataModel::load
    DataModel::addFileData
    FSTree::updateFolderImageCount
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

    G::allMetadataLoaded
    G::allIconsLoaded
    G::isNewFolderLoaded
    G::isNewFolderLoadedAndInfoViewUpToDate
    dm->loadingModel

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

QT VERSIONS

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
    createMDCache();            // dependent on DataModel, Metadata, ThumbView
    createImageCache();         // dependent on DataModel, Metadata, ThumbView
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

//   EVENT HANDLERS

void MW::showEvent(QShowEvent *event)
{
    if (G::isLogger) G::log(__FUNCTION__);
    QMainWindow::showEvent(event);

    getDisplayProfile();

    if (isSettings) {
        restoreGeometry(setting->value("Geometry").toByteArray());
        // run restoreGeometry second time if display has been scaled
        if (G::actDevicePixelRatio > 1.0)
            restoreGeometry(setting->value("Geometry").toByteArray());
        /* correct for highdpi
//        double dpiFactor = 1.0 / G::actDevicePixelRatio;
//        resize(width() * dpiFactor, height() * dpiFactor);
//        resize(width() * G::actDevicePixelRatio, height() * G::actDevicePixelRatio);
//        #endif
        //*/
        // restoreState sets docks which triggers setThumbDockFeatures prematurely
        restoreState(setting->value("WindowState").toByteArray());
        /*
        /* do not start with filterDock open
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
    metaRead->stop();
    imageCacheThread->stop();
    metadataCacheThread->stop();
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

void MW::moveEvent(QMoveEvent *event)
{
/*
    When the main winnow window is moved the zoom dialog, if it is open, must be moved as
    well. Also we need to know if the app has been dragged onto another monitor, which may
    have different dimensions and a different icc profile (win only).
*/
    if (G::isLogger) G::log(__FUNCTION__);
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
        // end stress test
        else if (isStressTest) isStressTest = false;
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

void MW::ingestFinished()
{
    if (G::isLogger) G::log(__FUNCTION__);
    qDebug() << __FUNCTION__;
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

        setCentralMessage("Loading Embellished ...");
        QApplication::processEvents();

        // create an instance of EmbelExport
        EmbelExport embelExport(metadata, dm, icd, embelProperties);

        // export get the location for the embellished files
        QString fPath = embelExport.exportRemoteFiles(templateName, pathList);
        info.setFile(fPath);
        QString fDir = info.dir().absolutePath();
        fsTree->getImageCount(fDir, true, __FUNCTION__);
        // go there ...
        fsTree->select(fDir);
        folderAndFileSelectionChange(fPath);
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

    G::currRootFolder = getSelectedPath();
    qDebug() << "stopAndClearAll" << currentViewDirPath;
    stopAndClearAll("folderSelectionChange");

    currentViewDirPath = getSelectedPath();
    setting->setValue("lastDir", currentViewDirPath);

    setCentralMessage("Loading information for folder " + currentViewDirPath);

    if (G::isLogger || G::isFlowLogger) {
        qDebug();
        G::log(__FUNCTION__, currentViewDirPath);
    }
//    G::log(__FUNCTION__, currentViewDirPath);

//    stopAndClearAll("folderSelectionChange");

    // do not embellish
//    if (turnOffEmbellish) embelProperties->invokeFromAction(embelTemplatesActions.at(0));
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

    qDebug() << "uncheckAllFilters" << currentViewDirPath;
    uncheckAllFilters();

    // load datamodel
    qDebug() << "load datamodel" << currentViewDirPath;
    if (!dm->load(currentViewDirPath, subFoldersAction->isChecked())) {
        qWarning() << "Datamodel Failed To Load for" << currentViewDirPath;
        stopAndClearAll();
        enableSelectionDependentMenus();
        if (dm->timeToQuit) {
            updateStatus(false, "Image loading has been cancelled", __FUNCTION__);
            setCentralMessage("Image loading has been cancelled");
//            QApplication::processEvents();
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

//    if (!dragFileSelected) {
//        thumbView->selectThumb(0);
//    }

    // Load folder progress
    setCentralMessage("Gathering metadata and thumbnails for images in folder.");
    updateStatus(false, "Collecting metadata for all images in folder(s)", __FUNCTION__);

    /* Must load metadata first, as it contains the file offsets and lengths for the thumbnail
    and full size embedded jpgs and the image width and height, req'd in imageCache to manage
    cache max size. The metadataCaching thread also loads the thumbnails. It triggers the
    loadImageCache when it is finished. The image cache is limited by the amount of memory
    allocated.

    While still initializing, the window show event has not happened yet, so the
    thumbsPerPage, used to figure out how many icons to cache, is unknown. 250 is the default.
    */

    // start loading new folder
//    metadataCacheThread->loadNewFolder(isRefreshingDM);
    G::t.restart();
    if (G::useLinearLoading) {
        // metadata and icons loaded in GUI thread
        loadLinearNewFolder();
    }
    else {
        // metadata and icons read using multiple threaded readers
        loadConcurrentNewFolder();
    }

    // format pickMemSize as bytes, KB, MB or GB
    pickMemSize = Utilities::formatMemory(memoryReqdForPicks());
    updateStatus(true, "", __FUNCTION__);

    G::isInitializing = false;
}

void MW::fileSelectionChange(QModelIndex current, QModelIndex previous, QString src)
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
    if (G::isLogger || G::isFlowLogger) G::log(__FUNCTION__, src + " " + current.data(G::PathRole).toString());
    qDebug() << __FUNCTION__ << G::stop << src << current.data(G::PathRole).toString();
    if (G::stop) return;
    G::isNewSelection = false;
//    if (current == previous) return;

   /*
    qDebug() << "\n" << __FUNCTION__
             << "src =" << src
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

    if (!isCurrentFolderOkay
            || G::isInitializing
            || isFilterChange
            || !G::isNewFolderLoaded)
        return;

    if (!currentViewDir.exists()) {
        refreshFolders();
        folderSelectionChange();
        return;
    }

//    qDebug() << __FUNCTION__ << "current.row() =" << current.row();
//    qDebug() << __FUNCTION__ << "thumbView->selectionModel()->selection() ="
//             << thumbView->selectionModel()->selection();
    // if starting program, set first image to display
    if (current.row() == -1) {
        thumbView->selectThumb(0);
        return;
    }

    // Check if anything selected.  If not disable menu items dependent on selection
    enableSelectionDependentMenus();

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

    if (G::isSlideShow && isSlideShowRandom) metadataCacheThread->stop();

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
    qDebug() << __FUNCTION__ << "G::isNewFolderLoaded =" << G::isNewFolderLoaded;
    if (G::isNewFolderLoaded) {
        fsTree->scrollToCurrent();          // req'd for first folder when Winnow opens
        updateIconsVisible(currentRow);
        if (G::useLinearLoading)
            metadataCacheThread->fileSelectionChange();
//        else
//            metaRead->read(MetaRead::FileSelection);
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
//            if (G::useLinearLoading)
                emit setImageCachePosition(dm->currentFilePath, __FUNCTION__);
//            else
//                emit setImageCachePosition2(dm->currentFilePath);
        }
    }

    workspaceChanged = false;

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

    if (folder == currentViewDirPath) {
        if (dm->proxyIndexFromPath(fPath).isValid()) {
            thumbView->selectThumb(fPath);
            gridView->selectThumb(fPath);
            currentSfIdx = dm->proxyIndexFromPath(fPath);
            fileSelectionChange(currentSfIdx, currentSfIdx, __FUNCTION__);
        }
        return;
    }
    /*
    qDebug() << __FUNCTION__
             << "isStartupArgs =" << isStartupArgs
             << "folder =" << folder
             << "currentViewDir =" << currentViewDir
                ;
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
    if (G::isLogger || G::isFlowLogger) G::log(__FUNCTION__);
    qDebug() << __FUNCTION__ << "COMMENCE STOPANDCLEARALL";

    G::stop = true;
    // Stop any threads that might be running.
    imageCacheThread->stop();
    if (!G::useLinearLoading) metaRead->stop();
    metadataCacheThread->stop();
    buildFilters->stop();

    imageView->clear();
    setWindowTitle(winnowWithVersion);
    G::isNewFolderLoaded = false;
    G::allMetadataLoaded = false;
    G::isNewFolderLoadedAndInfoViewUpToDate = false;
    imageView->clear();
    if (useInfoView) {
        infoView->clearInfo();
        updateDisplayResolution();
    }
    // turn thread activity buttons gray
    setThreadRunStatusInactive();
    isDragDrop = false;

    updateStatus(false, "", __FUNCTION__);
    cacheProgressBar->clearProgress();
    progressLabel->setVisible(false);
    updateClassification();
    selectionModel->clear();
    thumbView->setUpdatesEnabled(false);
    gridView->setUpdatesEnabled(false);
    tableView->setUpdatesEnabled(false);
    tableView->setSortingEnabled(false);
    dm->clearDataModel();
    thumbView->setUpdatesEnabled(true);
    gridView->setUpdatesEnabled(true);
    tableView->setUpdatesEnabled(true);
    tableView->setSortingEnabled(true);
    currentRow = 0;

    if (src == "folderSelectionChange")
        setCentralMessage("Loading folder.");
    else
        setCentralMessage("Select a folder.");

//    G::wait(1000);
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
    updateIconsVisible(-1);
    return (currentRow > metadataCacheThread->firstIconVisible &&
            metadataCacheThread->lastIconVisible);
}

void MW::updateIconsVisible(int row)
{
/*
    Polls both thumbView and gridView to determine the first, mid and last thumbnail visible.
    This is used in the metadataCacheThread to determine the range of files to cache.
*/
    if (G::isLogger || G::isFlowLogger) G::log(__FUNCTION__, QString::number(row));
    int firstVisible = dm->sf->rowCount();
    int lastVisible = 0;

//    // Grid might not be selected in CentralWidget
    if (G::mode == "Grid") centralLayout->setCurrentIndex(GridTab);

    if (thumbView->isVisible()) {
        if (row >= 0) thumbView->calcViewportRange(row);
        else thumbView->scannedViewportRange();
        if (thumbView->firstVisibleCell < firstVisible) firstVisible = thumbView->firstVisibleCell;
        if (thumbView->lastVisibleCell > lastVisible) lastVisible = thumbView->lastVisibleCell;
    }

    if (gridView->isVisible()) {
        if (row >= 0) gridView->calcViewportRange(row);
        else gridView->scannedViewportRange();
        if (gridView->firstVisibleCell < firstVisible) firstVisible = gridView->firstVisibleCell;
        if (gridView->lastVisibleCell > lastVisible) lastVisible = gridView->lastVisibleCell;
    }

    if (tableView->isVisible()) {
        tableView->setViewportParameters();
        if (tableView->firstVisibleRow < firstVisible) firstVisible = tableView->firstVisibleRow;
        if (tableView->lastVisibleRow > lastVisible) lastVisible = tableView->lastVisibleRow;
    }

    /*
    qDebug()
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

    int visibleIcons = lastVisible - firstVisible + 1;

    /*
    // icons to range based on caching icons for prev, curr and next page of thumbnails
    int toCacheIcons = visibleIcons * 3;
    int firstIconRow = firstVisible - visibleIcons;
    int lastIconRow = lastVisible + visibleIcons;
    if (firstIconRow < 0) firstIconRow = 0;
    if (lastIconRow >= dm->sf->rowCount()) lastIconRow = dm->sf->rowCount();

    // icons to range based on iconChunkSize
    int firstSuggested = row - dm->iconChunkSize / 2;
    if (firstSuggested < 0) firstSuggested == 0;
    int lastSuggested = firstSuggested + dm->iconChunkSize;
    if (lastSuggested >= dm->sf->rowCount()) lastSuggested = dm->sf->rowCount() - 1;

    // icon range to use:
    */

    metadataCacheThread->firstIconVisible = firstVisible;
    metadataCacheThread->midIconVisible = (firstVisible + lastVisible) / 2;// rgh qCeil ??
    metadataCacheThread->lastIconVisible = lastVisible;
    metadataCacheThread->visibleIcons = visibleIcons;

    dm->firstVisibleRow = firstVisible;
    dm->lastVisibleRow = lastVisible;
    /*
    dm->firstIconRow = firstIconRow;
    dm->lastIconRow = lastIconRow;
    */

}

void MW::loadConcurrent(MetaRead::Action action, int sfRow, QString src)
{
    if (G::isLogger || G::isFlowLogger) G::log(__FUNCTION__);
    if (!G::allMetadataLoaded || !G::allIconsLoaded) {
        updateMetadataThreadRunStatus(true, true, __FUNCTION__);
        metaRead->read(action, sfRow, src);
    }
}

void MW::loadConcurrentNewFolder()
{
    if (G::isLogger || G::isFlowLogger) G::log(__FUNCTION__);
    G::allMetadataLoaded = false;
    dm->loadingModel = true;
    // reset for bestAspect calc
    G::iconWMax = G::minIconSize;
    G::iconHMax = G::minIconSize;
    /* The memory required for the datamodel (metadata + icons) has to be estimated since the
       ImageCache is starting before all the metadata has been read.  Icons average ~180K and
       metadata ~20K  */
    int rows = dm->rowCount();
    int maxIconsToLoad = rows < metaRead->iconChunkSize ? rows : metaRead->iconChunkSize;
    G::metaCacheMB = (maxIconsToLoad * 0.18) + (rows * 0.02);
    // target image
    if (folderAndFileChangePath != "") {
        currentRow = dm->rowFromPath(folderAndFileChangePath);
        qDebug() << __FUNCTION__ << currentRow;
    }
    updateIconsVisible(currentRow);
    // set image cache parameters and build image cacheItemList
    int netCacheMBSize = cacheMaxMB - G::metaCacheMB;
    if (netCacheMBSize < cacheMinMB) netCacheMBSize = cacheMinMB;
    imageCacheThread->initImageCache(netCacheMBSize, cacheMinMB,
        isShowCacheProgressBar, cacheWtAhead);
    // no sorting or filtering until all metadata loaded
    filters->setEnabled(false);
    filterMenu->setEnabled(false);
    sortMenu->setEnabled(false);
    // read metadata
    metaRead->initialize();     // only when change folders
    loadConcurrent(MetaRead::FileSelection, currentRow, __FUNCTION__);
}

void MW::loadConcurrentMetaDone()
{
    if (G::isLogger || G::isFlowLogger) G::log(__FUNCTION__);
    if (G::stop) return;

    // double check all visible icons loaded, depending on best fit
    updateIconBestFit();
    updateIconsVisible(-1);
    if (dm->firstVisibleRow < metaRead->firstVisible ||
        dm->lastVisibleRow > metaRead->lastVisible)
    {
        metaRead->read(MetaRead::SizeChange);
        return;
    }

    /* now okay to write to xmp sidecar, as metadata is loaded and initial updates to
       InfoView by fileSelectionChange have been completed.  Otherwise, InfoView::dataChanged
       would prematurally trigger Metadata::writeXMP */
    G::isNewFolderLoadedAndInfoViewUpToDate = true;
    G::isNewFolderLoaded = true;
    dm->setAllMetadataLoaded(true);    //G::allMetadataLoaded = true;
    G::allIconsLoaded = dm->allIconsLoaded();
    filters->setEnabled(true);
    filterMenu->setEnabled(true);
    sortMenu->setEnabled(true);
    dm->loadingModel = false;
    if (!filterDock->visibleRegion().isNull()) {
        launchBuildFilters();
    }

    updateMetadataThreadRunStatus(false, true, __FUNCTION__);
    // resize table columns with all data loaded
    tableView->resizeColumnsToContents();
    tableView->setColumnWidth(G::PathColumn, 24+8);
}

void MW::loadConcurrentStartImageCache()
{
    if (G::isLogger || G::isFlowLogger) G::log(__FUNCTION__);
    if (isShowCacheProgressBar) {
        cacheProgressBar->clearProgress();
    }

    // preliminary resize table columns
    tableView->resizeColumnsToContents();
    tableView->setColumnWidth(G::PathColumn, 24+8);

    G::isNewFolderLoaded = true;
    dm->loadingModel = false;

    /* Trigger MW::fileSelectionChange.  This must be done to initialize many things
    including current index and file path req'd by mdCache and EmbelProperties...  If
    folderAndFileSelectionChange has been executed then folderAndFileChangePath will be
    the file to select in the new folder; otherwise the first file in dm->sf will be
    selected. */
    QString fPath = folderAndFileChangePath;
    folderAndFileChangePath = "";
    if (fPath != "" && dm->proxyIndexFromPath(fPath).isValid()) {
        qDebug() << __FUNCTION__ << "valid folderAndFileChangePath";
        if (thumbView->isVisible()) thumbView->selectThumb(fPath);
        if (gridView->isVisible()) gridView->selectThumb(fPath);

        gridView->selectThumb(fPath);
        currentSfIdx = dm->proxyIndexFromPath(fPath);
    }
    else {
        thumbView->selectFirst();
        gridView->selectFirst();
        currentSfIdx = dm->sf->index(0,0);
    }
    fileSelectionChange(currentSfIdx, currentSfIdx, __FUNCTION__);

    /* now okay to write to xmp sidecar, as metadata is loaded and initial updates to
       InfoView by fileSelectionChange have been completed.  Otherwise, InfoView::dataChanged
       would prematurally trigger Metadata::writeXMP */
//    G::isNewFolderLoadedAndInfoViewUpToDate = true;
}

void MW::loadLinearNewFolder()
{
/*
    Load metadata and thumbnails in GUI thread instead of MdCache.  metadataCacheThread is
    still used (as a separate thread) for updating icons when scroll, resize or change icon
    selection.
*/
    if (G::isLogger || G::isFlowLogger) G::log(__FUNCTION__);
    MetadataCache *mct = metadataCacheThread;
    G::allMetadataLoaded = false;
    mct->isRefreshFolder = isRefreshingDM;
    mct->iconsCached.clear();
    mct->foundItemsToLoad = true;
    mct->startRow = 0;
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

    dm->loadingModel = true;

    // no sorting or filtering until all metadta loaded
    filterMenu->setEnabled(false);
    sortMenu->setEnabled(false);

    setCentralMessage("Reading all metadata.");
    updateMetadataThreadRunStatus(true, true, __FUNCTION__);
    dm->addAllMetadata();

    if (dm->timeToQuit) {
        updateStatus(false, "Image loading has been cancelled", __FUNCTION__);
        setCentralMessage("Image loading has been cancelled");
//        QApplication::processEvents();
        return;
    }

    // have to wait for the data before resize table columns
    tableView->resizeColumnsToContents();
    tableView->setColumnWidth(G::PathColumn, 24+8);

    // read icons
    updateIconBestFit();
    updateIconsVisible(currentRow);

    setCentralMessage("Reading icons.");
    mct->readIconChunk();
    updateMetadataThreadRunStatus(false, true, __FUNCTION__);

    // re-enable sorting and filtering
    filterMenu->setEnabled(true);
    sortMenu->setEnabled(true);

    // start image cache
    G::metaCacheMB = mct->memRequired();
    loadImageCacheForNewFolder();
}

void MW::loadImageCacheForNewFolder()
{
/*
    This function is called from MW::loadLinearNewFolder after the metadata chunk has been
    loaded for a new folder selection. The imageCache loads images until the assigned amount
    of memory has been consumed or all the images are cached.
*/
    if (G::isLogger || G::isFlowLogger) G::log(__FUNCTION__);

    // clear the cache progress bar
    if(isShowCacheProgressBar) {
        cacheProgressBar->clearProgress();
    }

    // set image cache parameters and build image cacheItemList
    int netCacheMBSize = cacheMaxMB - G::metaCacheMB;
    if (netCacheMBSize < cacheMinMB) netCacheMBSize = cacheMinMB;
    imageCacheThread->initImageCache(netCacheMBSize, cacheMinMB,
        isShowCacheProgressBar, cacheWtAhead);

    // have to wait until image caching thread running before setting flag
    G::isNewFolderLoaded = true;
    dm->loadingModel = false;

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
//    fileSelectionChange(currentSfIdx, currentSfIdx, __FUNCTION__);

    /* now okay to write to xmp sidecar, as metadata is loaded and initial updates to
       InfoView by fileSelectionChange have been completed.  Otherwise, InfoView::dataChanged
       would prematurally trigger Metadata::writeXMP */
    G::isNewFolderLoadedAndInfoViewUpToDate = true;
}

//void MW::loadMetadataCache2ndPass()
//{
//    if (G::isLogger || G::isFlowLogger) G::log(__FUNCTION__);
////    qDebug() << __FUNCTION__;
//    updateIconBestFit();
//    updateIconsVisible(currentRow);
//    metadataCacheThread->loadNewFolder2ndPass();
//}

void MW::thumbHasScrolled()
{
/*
    This function is called after a thumbView scrollbar change signal.  The visible thumbnails
    are determined and loaded if necessary.

    If the change was caused by the user scrolling then we want to process it, as defined by
    G::ignoreScrollSignal == false. However, if the scroll change was caused by syncing with
    another view then we do not want to process again and get into a loop.

    Also, we do not need to process scrolling if it was the result of a new selection, which
    will trigger a thumbnail update in MW::fileSelectionChange.  G::isNewSelection is set true
    in IconView when a selection is made, and set false in fileSelectionChange.

    MW::updateMetadataCacheIconviewState polls setViewportParameters() in all visible views
    (thumbView, gridView and tableView) to assign the firstVisibleRow, midVisibleRow and
    lastVisibleRow in metadataCacheThread.

    The gridView and tableView, if visible, are scrolled to sync with thumbView.

    Finally metadataCacheThread->scrollChange is called to load any necessary metadata and
    icons within the cache range.

    Scrolling used to use a singleshot timer triggered by MW::loadMetadataCacheAfterDelay to
    call MW::loadMetadataChunk which, in turn, finally called the metadataCacheThread. This
    was to prevent many scroll calls from bunching up. The new approach just aborts the
    metadataCacheThread thread and starts over. It is simpler and faster.
*/
    if (G::isLogger) G::log(__FUNCTION__);
    if (G::isInitializing || !G::isNewFolderLoaded) return;

    if (G::ignoreScrollSignal == false) {
        G::ignoreScrollSignal = true;
        updateIconsVisible(-1);
        gridView->scrollToRow(thumbView->midVisibleCell, __FUNCTION__);
        updateIconsVisible(-1);
        if (tableView->isVisible())
            tableView->scrollToRow(thumbView->midVisibleCell, __FUNCTION__);
        // only call metadataCacheThread->scrollChange if scroll without fileSelectionChange
        if (!G::isNewSelection) {
            if (G::useLinearLoading) metadataCacheThread->scrollChange(__FUNCTION__);
            else loadConcurrent(MetaRead::Scroll, thumbView->midVisibleCell, __FUNCTION__);
        }
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
    This function is called after a thumbView scrollbar change signal.  The visible thumbnails
    are determined and loaded if necessary.

    If the change was caused by the user scrolling then we want to process it, as defined by
    G::ignoreScrollSignal == false. However, if the scroll change was caused by syncing with
    another view then we do not want to process again and get into a loop.

    Also, we do not need to process scrolling if it was the result of a new selection, which
    will trigger a thumbnail update in MW::fileSelectionChange.  G::isNewSelection is set true
    in IconView when a selection is made, and set false in fileSelectionChange.

    MW::updateMetadataCacheIconviewState polls setViewportParameters() in all visible views
    (thumbView, gridView and tableView) to assign the firstVisibleRow, midVisibleRow and
    lastVisibleRow in metadataCacheThread.

    The gridView and tableView, if visible, are scrolled to sync with thumbView.

    Finally metadataCacheThread->scrollChange is called to load any necessary metadata and
    icons within the cache range.

    Scrolling used to use a singleshot timer triggered by MW::loadMetadataCacheAfterDelay to
    call MW::loadMetadataChunk which, in turn, finally called the metadataCacheThread. This
    was to prevent many scroll calls from bunching up. The new approach just aborts the
    metadataCacheThread thread and starts over. It is simpler and faster.
*/
    if (G::isLogger) G::log(__FUNCTION__);
//    qDebug() << __FUNCTION__ << "0";
    if (G::isInitializing || !G::isNewFolderLoaded) return;

    if (G::ignoreScrollSignal == false) {
        G::ignoreScrollSignal = true;
        updateIconsVisible(-1);
        thumbView->scrollToRow(gridView->midVisibleCell, __FUNCTION__);
        // only call metadataCacheThread->scrollChange if scroll without fileSelectionChange
        if (!G::isNewSelection && gridView->isVisible()) {
            qDebug() << __FUNCTION__ << "1";
            if (G::useLinearLoading) metadataCacheThread->scrollChange(__FUNCTION__);
            else loadConcurrent(MetaRead::Scroll, gridView->midVisibleCell, __FUNCTION__);
//            metadataCacheThread->scrollChange(__FUNCTION__);
        }
    }
    G::ignoreScrollSignal = false;
}

void MW::tableHasScrolled()
{
/*
    This function is called after a thumbView scrollbar change signal.  The visible thumbnails
    are determined and loaded if necessary.

    If the change was caused by the user scrolling then we want to process it, as defined by
    G::ignoreScrollSignal == false. However, if the scroll change was caused by syncing with
    another view then we do not want to process again and get into a loop.

    Also, we do not need to process scrolling if it was the result of a new selection, which
    will trigger a thumbnail update in MW::fileSelectionChange.  G::isNewSelection is set true
    in IconView when a selection is made, and set false in fileSelectionChange.

    MW::updateMetadataCacheIconviewState polls setViewportParameters() in all visible views
    (thumbView, gridView and tableView) to assign the firstVisibleRow, midVisibleRow and
    lastVisibleRow in metadataCacheThread.

    The gridView and tableView, if visible, are scrolled to sync with thumbView.

    Finally metadataCacheThread->scrollChange is called to load any necessary metadata and
    icons within the cache range.

    Scrolling used to use a singleshot timer triggered by MW::loadMetadataCacheAfterDelay to
    call MW::loadMetadataChunk which, in turn, finally called the metadataCacheThread. This
    was to prevent many scroll calls from bunching up. The new approach just aborts the
    metadataCacheThread thread and starts over. It is simpler and faster.
*/
    if (G::isLogger) G::log(__FUNCTION__);
    if (G::isInitializing || !G::isNewFolderLoaded) return;

    if (G::ignoreScrollSignal == false) {
        G::ignoreScrollSignal = true;
        updateIconsVisible(-1);
        if (thumbView->isVisible())
            thumbView->scrollToRow(tableView->midVisibleRow, __FUNCTION__);
        // only call metadataCacheThread->scrollChange if scroll without fileSelectionChange
        if (!G::isNewSelection) {
            qDebug() << __FUNCTION__;
            if (G::useLinearLoading) metadataCacheThread->scrollChange(__FUNCTION__);
            else loadConcurrent(MetaRead::Scroll, tableView->midVisibleRow, __FUNCTION__);
//            metadataCacheThread->scrollChange(__FUNCTION__);
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
    if (G::isLogger) G::log(__FUNCTION__);
    if (G::isInitializing || !G::isNewFolderLoaded) return;
    updateIconsVisible(currentRow);
//    if (G::useLinearLoading)
//        metadataCacheThread->sizeChange(__FUNCTION__);
//    else
//        loadConcurrent(MetaRead::SizeChange, __FUNCTION__);
    metadataCacheThread->scrollChange(__FUNCTION__);
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
    if (dm->isAllMetadataLoaded()) return;

    updateIconsVisible(currentRow);

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
//            bool metaLoaded = dm->sf->index(i, G::MetadataLoadedColumn).data().toBool();
            bool cached = dm->sf->index(i, G::PathColumn).data(G::CachedRole).toBool();
            if (/*metaLoaded && */cached)
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
    if (G::isLogger || G::isFlowLogger) G::log(__FUNCTION__);
    if (G::stop) return;
    gridView->bestAspect();
    thumbView->bestAspect();
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
    loupeDisplay();
    if (turnOffEmbellish) embelProperties->doNotEmbellish();
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
        imageCacheThread->setCurrentPosition(fPath, __FUNCTION__);

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

void MW::updateMetadataThreadRunStatus(bool isRunning, bool showCacheLabel, QString calledBy)
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
    imageCacheThread->setCurrentPosition(dm->currentFilePath, __FUNCTION__);
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
        updateCachedStatus(fPath, false, "MW::toggleColorManage");
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

    updateIconsVisible(currentRow);
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
    getDisplayProfile();

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

void MW::getDisplayProfile()
{
/*
    This is required for color management.  It is called after the show event when the
    progam is opening and when the main window is moved to a different screen.
*/
    if (G::isLogger) G::log(__FUNCTION__);
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

        emit setValueSf(orientationIdx, newOrientation, Qt::EditRole);
//        dm->sf->setData(orientationIdx, newOrientation);

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
//        QApplication::processEvents();

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
    setting->setValue("tryConcurrentLoading", !G::useLinearLoading);
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
        G::useLinearLoading = true;

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
    if (setting->contains("tryConcurrentLoading"))
        G::useLinearLoading = !setting->value("tryConcurrentLoading").toBool();
    else
        G::useLinearLoading = true;

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

void MW::newEmbelTemplate()
{
    if (G::isLogger) G::log(__FUNCTION__);
    embelDock->setVisible(true);
    embelDock->raise();
    embelDockVisibleAction->setChecked(true);
    embelProperties->newTemplate();
    loupeDisplay();
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

void MW::exportEmbelFromAction(QAction *embelExportAction)
{
/*
    Called from main menu Embellish > Export > Export action.  The embellish editor
    may not be active, but the embellish template has been chosen by the action.
*/
    if (G::isLogger) G::log(__FUNCTION__);

    QStringList picks = getSelectionOrPicks();

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
    fsTree->updateVisibleImageCount();
    bookmarks->count();
}

void MW::exportEmbel()
{
/*
    Called when embellish editor is active and an embellish template has been selected.
*/
    if (G::isLogger) G::log(__FUNCTION__);

    QStringList picks = getSelectionOrPicks();

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

void MW::insertThumbnails()
{
    if (G::isLogger) G::log(__FUNCTION__);
    QModelIndexList selection = selectionModel->selectedRows();
    thumb->insertThumbnails(selection);
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
        QModelIndex dmIdx = dm->sf->mapToSource(dm->sf->index(row, col[tagName]));
        emit setValue(dmIdx, tagValue, Qt::EditRole);
//        QModelIndex idx = dm->sf->index(row, col[tagName]);
//        dm->sf->setData(idx, tagValue, Qt::EditRole);
        // check if combined raw+jpg and also set the tag item for the hidden raw file
        if (combineRawJpg) {
//            QModelIndex idx = dm->sf->index(selection.at(i).row(), 0);
            // is this part of a raw+jpg pair
            if (dmIdx.data(G::DupIsJpgRole).toBool()) {
                // set tag item for raw file row as well
                QModelIndex rawIdx = qvariant_cast<QModelIndex>(dmIdx.data(G::DupOtherIdxRole));
                QModelIndex idx = dm->index(rawIdx.row(), col[tagName]);
                emit setValue(idx, tagValue, Qt::EditRole);
//                dm->setData(idx, tagValue, Qt::EditRole);
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

void MW::stressTest(int ms)
{
    if (G::isLogger) G::log(__FUNCTION__);
    qDebug() << __FUNCTION__ << ms;
    ms = QInputDialog::getInt(this, "Enter ms delay between images", "Delay (1-1000 ms) ",
                              50, 1, 1000);

//    G::wait(1000);        // time to release modifier keys for shortcut (otherwise select many)
    isStressTest = true;
    bool isForward = true;
    slideCount = 0;
    QElapsedTimer t;
    t.start();
    qDebug() << __FUNCTION__ << "-1";
    while (isStressTest) {
        G::wait(ms);
        ++slideCount;
        qDebug() << slideCount << "times.  "
                    ;
        if (isForward && currentRow == dm->sf->rowCount() - 1) isForward = false;
        if (!isForward && currentRow == 0) isForward = true;
        if (isForward) keyRight();
        else keyLeft();
    }
    qint64 msElapsed = t.elapsed();
    double seconds = msElapsed * 0.001;
    double msPerImage = msElapsed * 1.0 / slideCount;
    int imagePerSec = slideCount * 1.0 / seconds;
    QString msg = "Executed stress test " + QString::number(slideCount) + " times.<br>" +
                  QString::number(msElapsed) + " ms elapsed.<br>" +
                  QString::number(ms) + " ms delay.<br>" +
                  QString::number(imagePerSec) + " images per second.<br>" +
                  QString::number(msPerImage) + " ms per image."
//                  + "<br><br>Press <font color=\"red\"><b>Esc</b></font> to cancel this popup."
                  ;
    G::popUp->showPopup(msg, 0);
    qDebug() << __FUNCTION__ << "Executed stress test" << slideCount << "times.  "
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
            if (metadata->hasMetadataFormats.contains(ext)) {
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
    Build a QStringList of the selected files, delete from disk, remove from datamodel,
    remove from ImageCache and update the image cache status bar.
*/
    if (G::isLogger) G::log(__FUNCTION__);
    // make sure datamodel is loaded
    if (!G::allMetadataLoaded) {
        G::popUp->showPopup("Cannot delete images before all images have been added, 1500");
        return;
    }

    if (deleteWarning) {
        QMessageBox msgBox;
        int msgBoxWidth = 300;
        msgBox.setWindowTitle("Delete Images");
        msgBox.setText("This operation will delete all selected images.");
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
//    thumbView->selectionModel()->clearSelection();

    G::ignoreScrollSignal = true;
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

    // remove row from MetaRead::iconsLoaded
    for (int i = 0; i < sldm.count(); ++i) {
        QString fPath = sldm.at(i);
        metaRead->dmRowRemoved(dm->rowFromPath(fPath));
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

    G::ignoreScrollSignal = false;

    // update current index
    if (lowRow >= dm->sf->rowCount()) lowRow = dm->sf->rowCount() - 1;
    QModelIndex sfIdx = dm->sf->index(lowRow, 0);
    thumbView->setCurrentIndex(sfIdx);
    fileSelectionChange(sfIdx, sfIdx, __FUNCTION__);

    // make sure all thumbs/icons are updated
//    if (!G::isNewSelection) {
//        if (G::useLinearLoading) metadataCacheThread->scrollChange(__FUNCTION__);
//        else loadConcurrent(MetaRead::Scroll, thumbView->midVisibleCell, __FUNCTION__);
//    }
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
        G::popUp->showPopup("No USB Drives available");
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
        if (!wasSubFoldersChecked) subFoldersAction->setChecked(true);
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

// End MW
