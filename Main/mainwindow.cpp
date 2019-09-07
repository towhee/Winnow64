#include "Main/mainwindow.h"

/*
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

• All eligible image files in the folder are added DataModel (dm).

• A thread is started to cache metadata and thumbs for all the images.

• A second thread is started by the metadata thread to cache as many full size images as
  assigned memory permits.

• Selecting a new folder also causes the selection of the first image which fires a signal
  for fileSelectionChange.

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
    MW::updateImageCachePositionAfterDelay
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

MW::MW(QWidget *parent) : QMainWindow(parent)
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }

    setObjectName("WinnowMainWindow");
    isShift = false;

    /* testing/debugging
       Note ISDEBUG is in globals.h
       Deactivate debug reporting by commenting ISDEBUG  */
    G::showAllTableColumns = false;     // show all table fields for debugging
    simulateJustInstalled = false;
    isStressTest = false;
    G::isTimer = true;                  // Global timer

    // Initialize some variables etc
    initialize();

    // platform specific settings
    setupPlatform();

    // structure to hold persistant settings between sessions
    setting = new QSettings("Winnow", "winnow_101");
    if (setting->contains("cacheSizeMB") && !simulateJustInstalled) isSettings = true;
    else isSettings = false;
    loadSettings();    //dependent on bookmarks and actions, infoView

    // app stylesheet and QSetting font size from last session
    createAppStyle();

    createCentralWidget();      // req'd by ImageView, CompareView
    createFilterView();         // req'd by DataModel (dm)
    createDataModel();          // dependent on FilterView, creates Metadata, Thumb
    createThumbView();          // dependent on QSetting, filterView
    createGridView();           // dependent on QSetting, filterView
    createTableView();          // dependent on centralWidget
    createSelectionModel();     // dependent on ThumbView, ImageView
    createInfoView();           // dependent on Metadata
    createCaching();            // dependent on DataModel, Metadata, ThumbView
    createImageView();          // dependent on centralWidget
    createCompareView();        // dependent on centralWidget
    createFSTree();             // dependent on Metadata
    createBookmarks();          // dependent on loadSettings
    createDocks();              // dependent on FSTree, Bookmarks, ThumbView, Metadata, InfoView
    createStatusBar();
    createMessageView();

    loadWorkspaces();           // req'd by actions and menu
    createActions();            // dependent on above
    createMenus();              // dependent on createActions and loadSettings

    handleStartupArgs();

    loadShortcuts(true);            // dependent on createActions
    setupCentralWidget();
    setActualDevicePixelRatio();

    // recall previous thumbDock state in case last closed in Grid mode
    thumbDockVisibleAction->setChecked(wasThumbDockVisible);

    // intercept events to thumbView to monitor splitter resize of thumbDock
    qApp->installEventFilter(this);

    // process the persistant folder if available
    if (rememberLastDir && !isShift) folderSelectionChange();

    if (!isSettings) centralLayout->setCurrentIndex(StartTab);
    else {
        QString msg = "Select a folder or bookmark to get started.";
        setCentralMessage(msg);
        prevMode = "Loupe";
    }

    qRegisterMetaType<ImageMetadata>();
    qRegisterMetaType<QVector<int>>();

    // if no fsTree expansion then invoke getImageCount the first time
//    fsTree->getImageCount();
}

void MW::initialize()
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    this->setWindowTitle("Winnow");

//    qDebug() << "font" << font().family();
//    QFont f = font();
//    f.setPointSize(16);
//    QGuiApplication::setFont(f);

    QGuiApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    G::isInitializing = true;
    isNormalScreen = true;
    G::isSlideShow = false;
    workspaces = new QList<workspaceData>;
    recentFolders = new QStringList;
    ingestHistoryFolders = new QStringList;
    G::newPopUp(this);
    hasGridBeenActivated = true;
    isDragDrop = false;
    setAcceptDrops(true);
    filterStatusLabel = new QLabel;
    filterStatusLabel->setToolTip("The images have been filtered");
    subfolderStatusLabel = new QLabel;
    subfolderStatusLabel->setToolTip("Showing contents of all subfolders");
    rawJpgStatusLabel = new QLabel;
    rawJpgStatusLabel->setToolTip("Raw and Jpg files are combined for viewing");
    slideShowStatusLabel = new QLabel;
    slideShowStatusLabel->setToolTip("Slideshow is active");
    prevCentralView = 0;
    G::labelColors << "Red" << "Yellow" << "Green" << "Blue" << "Purple";
    G::ratings << "1" << "2" << "3" << "4" << "5";
    pickStack = new QStack<Pick>;
    slideshowRandomHistoryStack = new QStack<QString>;
    scrollRow = 0;
    ICC::setInProfile(nullptr);
//    G::newPopUp();
}

void MW::setupPlatform()
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    #ifdef Q_OS_LINIX
//        G::devicePixelRatio = 1;
    #endif
    #ifdef Q_OS_WIN
        setWindowIcon(QIcon(":/images/winnow.png"));
        Win::collectScreensInfo();
        Win::availableMemory();
    #endif
    #ifdef Q_OS_MAC
        setWindowIcon(QIcon(":/images/winnow.icns"));
    #endif
}

//   EVENT HANDLERS

void MW::showEvent(QShowEvent *event)
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    if (isSettings) {
        restoreGeometry(setting->value("Geometry").toByteArray());
        // restoreState sets docks which triggers setThumbDockFeatures prematurely
        restoreState(setting->value("WindowState").toByteArray());
        updateState();
    }
    else {
        defaultWorkspace();
    }

    QMainWindow::showEvent(event);

    // check for updates
    if(checkIfUpdate) QTimer::singleShot(50, this, SLOT(checkForUpdate()));

    // show image count in folder panel if no folder selected
    if (!rememberLastDir) QTimer::singleShot(50, fsTree, SLOT(getImageCount()));

    // get the monitor screen for testing against movement to an new screen in setDisplayResolution()
    QPoint loc = centralWidget->window()->geometry().center();
    prevScreen = qApp->screenAt(loc)->name();

    G::isInitializing = false;
}

void MW::closeEvent(QCloseEvent *event)
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    metadataCacheThread->stopMetadateCache();
    imageCacheThread->stopImageCache();
    if (!simulateJustInstalled) writeSettings();
    G::popUp->close();
    hide();
    if (!QApplication::clipboard()->image().isNull()) {
        QApplication::clipboard()->clear();
    }
    event->accept();
}

void MW::moveEvent(QMoveEvent *event)
{
/*
When the main winnow window is moved the zoom dialog, if it is open, must be moved as well.
Also we need to know if the app has been dragged onto another monitor, which may have different
dimensions and a different icc profile (win only).
*/
    QMainWindow::moveEvent(event);
    emit resizeMW(this->geometry(), centralWidget->geometry());
    setDisplayResolution();
    QString dimensions = QString::number(displayHorizontalPixels) + "x"
            + QString::number(displayVerticalPixels);
    QStandardItemModel *k = infoView->ok;
    k->setData(k->index(infoView->MonitorRow, 1, infoView->statusInfoIdx), dimensions);
}

void MW::resizeEvent(QResizeEvent *event)
{
    QMainWindow::resizeEvent(event);
    emit resizeMW(this->geometry(), centralWidget->geometry());
}

void MW::keyPressEvent(QKeyEvent *event)
{

//    if(event->modifiers() & Qt::ShiftModifier) isShift = true;
//    else isShift = false;
//    qDebug() << "MW::keyPressEvent" << event << isShift;

//    dm->timeToQuit = true;

    QMainWindow::keyPressEvent(event);

    if (event->key() == Qt::Key_Return) {
        loupeDisplay();
    }
    if (event->key() == Qt::Key_Escape) {
        // cancel slideshow
        if (G::isSlideShow) {
            slideShow();     // toggles slideshow off
            return;
        }

        if(fullScreenAction->isChecked()) {
            escapeFullScreen();
        }
        else {
            dm->timeToQuit = true;
        }
    }

    QMainWindow::keyPressEvent(event);
}

void MW::keyReleaseEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Escape) {
        // hide a popup message
        if (G::popUp->isVisible()) {
            G::popUp->hide();
            return;
        }
        // hide preferences
        if (preferencesHasFocus) {
            propertiesDock->setVisible(false);
        }
    }

    if (G::isSlideShow) {
        if (event->key() == Qt::Key_Backspace) {
            isSlideshowPaused = false;
            prevRandomSlide();
        }
        if (event->key() == Qt::Key_W) {
            isSlideShowWrap = !isSlideShowWrap;
            isSlideshowPaused = false;
            QString msg;
            if (isSlideShowWrap) msg = "Slide wrapping is on.";
            else msg = "Slide wrapping is off.";
            G::popUp->showPopup(msg);
        }
        if (event->key() == Qt::Key_H) {
            isSlideshowPaused = true;
            slideshowHelpMsg();
        }
        if (event->key() == Qt::Key_Space) {
            isSlideshowPaused = !isSlideshowPaused;
            QString msg;
            if (isSlideshowPaused) msg = "Slideshow is paused";
            else msg = "Slideshow is restarted";
            G::popUp->showPopup(msg);
        }
        if (event->key() == Qt::Key_R) {
            isSlideShowRandom = !isSlideShowRandom;
            isSlideshowPaused = false;
            slideShowResetSequence();
            QString msg;
            if (isSlideShowRandom) msg = "Random selection enabled.";
            else msg = "Sequential selection enabled.";
            G::popUp->showPopup(msg);
        }
        // change slideshow delay 1 - 9 seconds
        int n = event->key() - 48;
        if (n > 0 && n <= 9) {
            isSlideshowPaused = false;
            slideShowDelay = n;
            slideShowResetDelay();
        }
    }

    QMainWindow::keyReleaseEvent(event);
}

bool MW::event(QEvent *event)
{
/*    if (event->type() == QEvent::KeyRelease) {
        QKeyEvent *keyEvent = static_cast<QKeyEvent*>(event);

        if (keyEvent->key() == Qt::Key_Escape) {
            if (G::isSlideShow) slideShow();     // toggles slideshow off
        }
        // change delay 1 - 9 seconds
        if (G::isSlideShow) {
            int n = keyEvent->key() - 48;
            if (n > 0 && n <=9) {
            }
        }
    }*/
    return QMainWindow::event(event);
}

bool MW::eventFilter(QObject *obj, QEvent *event)
{

// use to show all events being filtered - handy to figure out which to intercept
/*    if (event->type()        != QEvent::Paint
            && event->type() != QEvent::UpdateRequest
            && event->type() != QEvent::ZeroTimerEvent
            && event->type() != QEvent::Timer)
    {
        qDebug() << event << "\t"
                 << event->type() << "\t"
                 << obj << "\t"
                 << obj->objectName();
    }*/


    // figure out key presses
/*    if(event->type() == QEvent::ShortcutOverride && obj->objectName() == "MWClassWindow") {
        G::track(__FUNCTION__, "Performance profiling");
        qDebug() << event <<  obj;
    };
    */

    /* FILTERS ***************************************************************************

    Filters are only run on demand as they can take time to generate and the user will
    not always need to filter.  The triggers to build the filters are:

    - A filter is selected from the Filters menu actions

    - The Filters panel is activated (this is detected here)
       - they have not been built yet
       - the filters panel is visible
       - the ChildRemoved event fires for the filters widget
    */

    if (!dm->filtersBuilt && !G::buildingFilters && !G::isInitializing) {
        if (!filterDock->visibleRegion().isNull()) {
            if(obj->objectName() == "Filters") {
                if (event->type() == QEvent::ChildRemoved) {
                    /*
                    qDebug() << "building filters from event filter"
                             << "G::isInitializing" << G::isInitializing;
                    */
                    buildFilters();
                }
            }
        }
    }

    /* ICONVIEW SCROLLING ****************************************************************

    IconView is ready-to-go and can scroll to wherever we want:

    When the user changes modes in MW (ie from Grid to Loupe) a IconView instance (either
    thumbView or gridView) can change state from hidden to visible. Since hidden widgets
    cannot be accessed we need to wait until the IconView becomes visible and fully repainted
    before attempting to scroll to the current index. The IconView scrollBar paint events are
    monitored and when the last paint event occurs the scrollToCurrent function is called. The
    last paint event is identified by calculating the maximum of the scrollbar range and
    comparing it to the paint event, which updates the range each time. With larger datasets
    (ie 1500+ thumbs) it can take a number of paint events and 100s of ms to complete. A flag
    is assigned (scrollWhenReady) to show when we need to monitor so not checking needlessly.
    Unfortunately there does not appear to be any signal or event when ListView is finished
    hence this cludge.
    */

//    if(event->type() == QEvent::Paint
//            && thumbView->scrollWhenReady
//            && (obj->objectName() == "IconViewVerticalScrollBar"
//            || obj->objectName() == "IconViewHorizontalScrollBar"))
//    {
//        if (obj->objectName() == "IconViewHorizontalScrollBar") {
//            /*
//            qDebug() << G::t.restart() << "\t" << objectName() << "HorScrollCurrentMax / FinalMax:"
//                     << thumbView->horizontalScrollBar()->maximum()
//                     << thumbView->getHorizontalScrollBarMax();*/
//            if (thumbView->horizontalScrollBar()->maximum() > 0.95 * thumbView->getHorizontalScrollBarMax()) {
//                /*
//                qDebug() << objectName()
//                 << ": Event Filter sending row =" << currentRow
//                 << "horizontalScrollBarMax Qt vs Me"
//                 << thumbView->horizontalScrollBar()->maximum()
//                 << thumbView->getHorizontalScrollBarMax();*/
//                thumbView->scrollWhenReady = false;
//                thumbView->scrollToRow(scrollRow, __FUNCTION__);
//            }
//        }
//        if (obj->objectName() == "IconViewVerticalScrollBar") {
//             /*
//             qDebug() << G::t.restart() << "\t" << objectName() << "VerScrollCurrentMax / FinalMax:"
//                      << thumbView->verticalScrollBar()->maximum()
//                      << thumbView->getVerticalScrollBarMax();*/
//             if (thumbView->verticalScrollBar()->maximum() > 0.95 * thumbView->getVerticalScrollBarMax()){
//                /*
//                 qDebug() << G::t.restart() << "\t" << objectName()
//                          << ": Event Filter sending row =" << currentRow
//                          << "verticalScrollBarMax Qt vs Me"
//                          << thumbView->verticalScrollBar()->maximum()
//                          << thumbView->getVerticalScrollBarMax();*/
//                 thumbView->scrollWhenReady = false;
//                 thumbView->scrollToRow(scrollRow, __FUNCTION__);
//             }
//         }
//        if (obj->objectName() == "TableViewHorizontalScrollBar") {
//            qDebug() << "TableViewHorizontalScrollBar";
//        }
//    }

//    if(event->type() == QEvent::Paint
//            && gridView->scrollWhenReady
//            && (obj->objectName() == "IconViewVerticalScrollBar"))
//    {
//         if (gridView->verticalScrollBar()->maximum() > 0.95 * gridView->getVerticalScrollBarMax()){
//             /*
//             qDebug() << "XXXXXXXXXXXXXXX      " << objectName()
//                      << ": Event Filter sending row =" << currentRow
//                      << "verticalScrollBarMax Qt vs Me"
//                      << gridView->verticalScrollBar()->maximum()
//                      << gridView->getVerticalScrollBarMax();
//             */
//             gridView->scrollWhenReady = false;
//             gridView->scrollToRow(scrollRow, __FUNCTION__);
//         }
//    }

    /* CONTEXT MENU **********************************************************************

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
        if(obj == fsTree->viewport()) {
            QContextMenuEvent *e = static_cast<QContextMenuEvent *>(event);
            QModelIndex idx = fsTree->indexAt(e->pos());
            mouseOverFolder = idx.data(QFileSystemModel::FilePathRole).toString();
            enableEjectUsbMenu(mouseOverFolder);
            // in folders or bookmarks but not on folder item
            if(mouseOverFolder == "") {
                addBookmarkAction->setEnabled(false);
//                revealFileAction->setEnabled(false);
                revealFileActionFromContext->setEnabled(false);
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
//                revealFileAction->setEnabled(false);
                revealFileActionFromContext->setEnabled(false);
            }
        }
        else {
            enableEjectUsbMenu(currentViewDir);
            if(currentViewDir == "") {
                addBookmarkAction->setEnabled(false);
//                revealFileAction->setEnabled(false);
                revealFileActionFromContext->setEnabled(false);
            }
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

    if (event->type() == QEvent::MouseMove) {
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
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    if (current == nullptr) return;
    if (current->objectName() == "DisableGoActions") enableGoKeyActions(false);
    else enableGoKeyActions(true);
    if (previous == nullptr) return;    // suppress compiler warning
}

// DRAG AND DROP

void MW::dragEnterEvent(QDragEnterEvent *event)
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
//    QString dir = event->mimeData()->urls().at(0).toLocalFile();
    QFileInfo info(event->mimeData()->urls().at(0).toLocalFile());
    QDir incoming = info.dir();
    QDir current(currentViewDir);
    bool isSameFolder = incoming == current;
    qDebug() << __FUNCTION__ << incoming << current;
    if (!isSameFolder) event->acceptProposedAction();
}

void MW::dropEvent(QDropEvent *event)
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    handleDrop(event->mimeData());
}

void MW::handleDrop(const QMimeData *mimeData)
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    if (mimeData->hasUrls())
    {
        dragDropFilePath = mimeData->urls().at(0).toLocalFile();
        QFileInfo fInfo = QFileInfo(dragDropFilePath);
        if (fInfo.isDir()) {
            dragDropFolderPath = fInfo.absoluteFilePath();
            dragDropFilePath = "";
        }
        else {
            dragDropFolderPath = fInfo.absoluteDir().absolutePath();
        }
        isDragDrop = true;
        fsTree->select(dragDropFolderPath);
        folderSelectionChange();
    }

}

bool MW::checkForUpdate()
{
/* Called from the help menu and from the main window show event if (ifCheckUpdate = true)
   The Qt maintenancetool, an executable in the Winnow folder, checks to see if there is an
   update on the Winnow server.  If there is then Winnow is closed and the maintenancetool
   performs the install of the update.  When that is completed the maintenancetool opens
   Winnow again.
*/
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }

#ifdef Q_OS_MAC
    return false;
#endif

#ifdef Q_OS_LINIX
    rerurn false;
#endif

#ifdef Q_OS_WIN
    QProcess process;
    process.start("maintenancetool --checkupdates");

    // Wait until the update tool is finished
    process.waitForFinished();

//    qDebug() << "process.error() =" << process.error();
    if(process.error() != QProcess::UnknownError)
    {
        QString msg = "Error checking for updates";
        G::popUp->showPopup(msg, 1500);
        isStartSilentCheckForUpdates = false;
        return false;
    }

    // Read the output
    QByteArray data = process.readAllStandardOutput();

    /* No output means no updates available
     Note that the exit code will also be 1, but we don't use that
     Also note that we should parse the output instead of just checking if it is empty if we want specific update info
    */
    if(data.isEmpty())
    {
        QString msg = "No updates available";
        if(!isStartSilentCheckForUpdates) G::popUp->showPopup(msg, 1500);
        isStartSilentCheckForUpdates = false;
        return false;
    }

    updateAppDlg = new UpdateApp(version, css);
    int ret = updateAppDlg->exec();
    if(ret == QDialog::Rejected) {
        process.close();
        return false;
    }

    // Call the maintenance tool binary
    // Note: we start it detached because this application needs to close for the update
    QStringList args("--updater");
    bool startMaintenanceTool = QProcess::startDetached("maintenancetool", args);

    // Close Winnow
    if (startMaintenanceTool) qApp->closeAllWindows();
    else if(!isStartSilentCheckForUpdates)
        G::popUp->showPopup("The maintenance tool failed to open", 2000);

    isStartSilentCheckForUpdates = false;

    // prevent compiler warning
    return false;
#endif
}

// Do we need this?  rgh
void MW::handleStartupArgs()
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
}

void MW::folderSelectionChange()
{
    {
    #ifdef ISDEBUG
    G::track("\n======================================================================================================");
    G::track(__FUNCTION__);
    #endif
    }
     // Stop any threads that might be running.
    imageCacheThread->stopImageCache();
    metadataCacheThread->stopMetadateCache();
    G::allMetadataLoaded = false;

//    qDebug() << __FUNCTION__ << "Stopped threads";
    statusBar()->showMessage("Collecting file information for all images in folder(s)", 1000);
    qApp->processEvents();

    // used by SortFilter, set true when ImageCacheThread starts
    G::isNewFolderLoaded = false;

    // ImageView set zoom = fit for the first image of a new folder
    imageView->isFirstImageNewFolder = true;

    // used by updateStatus
    isCurrentFolderOkay = false;
    pickMemSize = "";

    // in case no eligible images, therefore no caching
    setThreadRunStatusInactive();

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
        if (prevMode == "Loupe") asGridAction->setChecked(true);
        if (prevMode == "Grid") asLoupeAction->setChecked(true);
        if (prevMode == "Table") asTableAction->setChecked(true);
        if (prevMode == "Compare") asLoupeAction->setChecked(true);
    }

    // If just opened application
    if (G::isInitializing) {
        if (rememberLastDir) {
            // lastDir is from QSettings for persistent memory between sessions
            if (isFolderValid(lastDir, true, true)) {
                currentViewDir = lastDir;
                fsTree->select(currentViewDir);
            }
            else {
               QModelIndex idx = fsTree->fsModel->index(currentViewDir);
               if (fsTree->fsModel->hasIndex(idx.row(), idx.column(), idx.parent()))
                   qDebug() << __FUNCTION__;
                   fsTree->setCurrentIndex(fsTree->fsFilter->mapFromSource(idx));
               G::isInitializing = false;
               return;
            }
        }
    }

    // folder selected from Folders or Bookmarks(Favs)
    if (!rememberLastDir) {
        currentViewDir = getSelectedPath();
    }

//    // folder selected from Folders or Bookmarks(Favs)
//    if (!G::isInitializing || !rememberLastDir) {
//        currentViewDir = getSelectedPath();
//    }

    // sync the favs / bookmarks with the folders view fsTree
    bookmarks->select(currentViewDir);

    // confirm folder exists and is readable, report if not and do not process
    qDebug() << __FUNCTION__ << "currentViewDir =" << currentViewDir;
    if (!isFolderValid(currentViewDir, true, false)) {
        clearAll();
        G::isInitializing = false;
        return;
    }

    // add to recent folders
    addRecentFolder(currentViewDir);

    // sync the folders tree with the current folder
    fsTree->scrollToCurrent();

    // show image count in Folders (fsTree) if showImageCountAction isChecked
    if (showImageCount) {
        fsTree->setShowImageCount(true);
    }

    // update menu
    enableEjectUsbMenu(currentViewDir);

    /* We do not want to update the imageCache while metadata is still being loaded. The
    imageCache update is triggered in metadataCache, which is also executed when the change
    file event is fired.
    */
    metadataLoaded = false;

    /* The first time a folder is selected the datamodel is loaded with all the
    supported images.  If there are no supported images then do some cleanup. If
    this function has been executed by the load subfolders command then all the
    subfolders will be recursively loaded into the datamodel.
    */

    uncheckAllFilters();
//    sortFileNameAction->setChecked(true);

    /* When a new folder is loaded it takes time to update the views and they will not respond
    to scroll commands until they are finished. Set flags that are updated in eventFilter.
    */
//    thumbView->scrollWhenReady = true;
//    gridView->scrollWhenReady = true;

    if (!dm->load(currentViewDir, subFoldersAction->isChecked())) {
        qDebug() << "Datamodel Failed To Load for" << currentViewDir;
        clearAll();
        enableSelectionDependentMenus();
        if (dm->timeToQuit) {
            updateStatus(false, "Image loading has been cancelled");
            setCentralMessage("Image loading has been cancelled");
            return;
        }
        QDir dir(currentViewDir);
        if (dir.isRoot()) {
            updateStatus(false, "No supported images in this drive");
            setCentralMessage("The drive \"" + currentViewDir + "\" does not have any eligible images");
        }
        else {
            updateStatus(false, "No supported images in this folder");
            setCentralMessage("The folder \"" + currentViewDir + "\" does not have any eligible images");
        }
        G::isInitializing = false;
        return;
    }
    centralLayout->setCurrentIndex(prevCentralView);    // rgh req'd?

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


//    popUp->close();   // rgh is this needed
    updateStatus(false, "Collecting metadata for all images in folder(s)");

    /* Must load metadata first, as it contains the file offsets and lengths for the thumbnail
    and full size embedded jpgs and the image width and height, req'd in imageCache to manage
    cache max size. The metadataCaching thread also loads the thumbnails. It triggers the
    loadImageCache when it is finished. The image cache is limited by the amount of memory
    allocated.

    While still initializing, the window show event has not happened yet, so the
    thumbsPerPage, used to figure out how many icons to cache, is unknown. 250 is the default.
    */
    if (isRefreshingDM) {
        refreshCurrentAfterReload();
    }
    else {
        // default sort by file name
//        thumbView->sortThumbs(1, false);
        thumbView->selectThumb(0);
        metadataCacheThread->loadNewFolder();
    }

    // format pickMemSize as bytes, KB, MB or GB
    pickMemSize = Utilities::formatMemory(memoryReqdForPicks());
    updateStatus(true);

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

It has proven quite difficult to sync the thumbView, tableView and gridView, keeping the
currentIndex in the center position (scrolling). The issue is timing as it can take the
scrollbars a while to finish painting and there isn't an event to notify the view is
ready for action. One to many scrollbar paint events are fired, and the
scrollbar->maximum() value is tested against a calculated maximum to determine when the
last paint event has fired. The scrollToCurrent function is called. It has also been
necessary to write custom scrollbar functions because the scrollTo(idx, positionAtCenter)
does not always work. Finally, when QAbstractItemView accepts a mouse press it adds a
100ms delay to avoid double clicks but this plays havoc with the scrolling, so a flag is
used to track which actions are triggering changes in the datamodel index. If it is a
mouse press then a singeShot timer in invoked to delay the scrolling to after
QAbstractItemView is finished.

Note that the datamodel includes multiple columns for each row and the index sent to
fileSelectionChange could be for a column other than 0 (from tableView) so scrollTo and
delegate use of the current index must check the column.
*/
    {
    #ifdef ISDEBUG
    G::track("\n-----------------------------------------------------------------------------");
    G::track(__FUNCTION__, current.data(G::PathRole).toString());
    #endif
    }
    qDebug() << __FUNCTION__
             << "G::isInitializing =" << G::isInitializing
             << "G::isNewFolderLoaded =" << G::isNewFolderLoaded
             << "isFirstImageNewFolder =" << imageView->isFirstImageNewFolder;

    bool isStart = false;
    if(!isCurrentFolderOkay) return;

    if (imageView->isFirstImageNewFolder) thumbView->selectThumb(0);

    // if starting program, set first image to display
    if (current.row() == -1) {
        qDebug() << __FUNCTION__ << "isStart";
        isStart = true;
        thumbView->selectThumb(0);
    }

    // Check if anything selected.  If not disable menu items dependent on selection
    enableSelectionDependentMenus();

    if (isStart) return;

    /*
    if (isDragDrop && dragDropFilePath.length() > 0) {
        thumbView->selectThumb(dragDropFilePath);
        isDragDrop = false;
    }
    */

    // user clicks outside thumb but inside treeView dock
    QModelIndexList selected = selectionModel->selectedIndexes();
    if (selected.isEmpty() && !G::isInitializing) return;

    // record current row as it is used to sync everything
    currentRow = current.row();
    // also record in datamodel so can be accessed by MdCache
    // proxy index for col 0
    currentSfIdx = dm->sf->index(currentRow, 0);
    dm->currentRow = currentRow;
    currentDmIdx = dm->sf->mapToSource(currentSfIdx);
    // the file path is used as an index in ImageView
    QString fPath = currentSfIdx.data(G::PathRole).toString();
//    QString fPath = dm->sf->index(currentRow, 0).data(G::PathRole).toString();
    // also update datamodel, used in MdCache
    dm->currentFilePath = fPath;

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

    if (G::isSlideShow && isSlideShowRandom) metadataCacheThread->stopMetadateCache();

    // check and load metadata for the current image ******************************************

    /* check metadata loaded for current image (might not be if random slideshow)
       to prevent a conflict with the metadataCacheThread*/
    int dmRow = dm->fPathRow[fPath];
    if (!dm->index(dmRow, G::MetadataLoadedColumn).data().toBool()) {
        QFileInfo fileInfo(fPath);
        QString ext = fileInfo.suffix().toLower();
        if (metadata->getMetadataFormats.contains(ext)) {
            if (metadata->loadImageMetadata(fileInfo, true, true, false, true, __FUNCTION__)) {
                metadata->imageMetadata.row = dmRow;
                dm->addMetadataForItem(metadata->imageMetadata);
            }
        }
    }

    // updates ********************************************************************************

    // new file name appended to window title
    setWindowTitle(winnowWithVersion + "   " + fPath);

    if (!G::isSlideShow) progressLabel->setVisible(isShowCacheStatus);

    // update imageView, use cache if image loaded, else read it from file
    if (G::mode == "Loupe") {
        qDebug() << __FUNCTION__
                 << "imageView->loadImage(fPath)   imageView->isFirstImageNewFolder ="
                 << imageView->isFirstImageNewFolder;
        if (imageView->loadImage(fPath)) {
            updateClassification();
        }
    }

    // update caching when folder has been loaded
    if (G::isNewFolderLoaded ) {
        updateIconsVisible(true);
        metadataCacheThread->fileSelectionChange(updateImageCacheWhenFileSelectionChange);
    }

    // update the metadata panel
    infoView->updateInfo(currentRow);

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

    // load thumbnail if not cached yet (when loading new folder)
//    if (!thumbView->isThumb(currentRow)) {
//        QImage image;
//        thumb->loadThumb(fPath, image);
//        thumbView->setIcon(currentRow, image);
////        updateIconBestFit();
//    }

    // update cursor position on progressBar
    updateImageCacheStatus("Update cursor", currentRow, "MW::fileSelectionChange");
}

void MW::clearAll()
{
/*
Called when folderSelectionChange and invalid folder (no folder, no eligible images).
Can be triggered when the user picks a folder in the folder panel or open menu, picks
a bookmark or ejects a drive and the resulting folder does not have any eligible images.
*/
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    // Stop any threads that might be running.
    imageCacheThread->stopImageCache();
    metadataCacheThread->stopMetadateCache();
    G::allMetadataLoaded = false;
    dm->clear();
    currentRow = 0;
    infoView->clearInfo();
    metadata->clear();
    imageView->clear();
//    progressLabel->setVisible(false);
    setThreadRunStatusInactive();                      // turn thread activity buttons gray
    isDragDrop = false;

    updateStatus(false, "");
    progressLabel->setVisible(false);
    updateClassification();
}

void MW::nullFiltration()
{
    updateStatus(false, "No images match the filtration");
    setCentralMessage("No images match the filtration");
    infoView->clearInfo();
    metadata->clear();
    imageView->clear();
    progressLabel->setVisible(false);
//    G::isInitializing = false;
    isDragDrop = false;
}

bool MW::isCurrentThumbVisible()
{
/*  rgh not being used
This function is used to determine if it is worthwhile to cache more metadata and/or icons.  If
the image/thumb that has just become the current image is already visible then do not invoke
metadata caching.
*/
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    updateIconsVisible(false);
    return (currentRow > metadataCacheThread->firstIconVisible &&
            metadataCacheThread->lastIconVisible);
}

void MW::updateIconsVisible(bool useCurrentRow)
{
/*
This function polls both thumbView and gridView to determine the first, mid and last thumbnail
visible.  This is used in the metadataCacheThread to determine the range of files to cache.
*/
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
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

/*    qDebug() << __FUNCTION__
             << "\n\tthumbView->firstVisibleCell =" << thumbView->firstVisibleCell
             << "thumbView->lastVisibleCell =" << thumbView->lastVisibleCell
             << "\n\tgridView->firstVisibleCell =" << gridView->firstVisibleCell
             << "gridView->lastVisibleCell =" << gridView->lastVisibleCell
             << "\n\ttableView->firstVisibleCell =" << tableView->firstVisibleRow
             << "tableView->lastVisibleCell =" << tableView->lastVisibleRow
             << "\n\tfirst =" << first
             << "last =" << last;
*/
    metadataCacheThread->firstIconVisible = first;
    metadataCacheThread->midIconVisible = (first + last) / 2;// rgh qCeil ??
    metadataCacheThread->lastIconVisible = last;
    metadataCacheThread->visibleIcons = last - first + 1;
}

void MW::updateImageCachePositionAfterDelay()
{
/*
Signalled from the imageCacheThread when a new image is selected or on a filtration change.

The imageCacheTimer calls ImageCache::updateImageCachePosition, which updates the cache for
the current image in the data model. The cache key is set, forward or backward progress is
determined and the target range is updated. Image caching is reactivated.

THERE IS A PERFORMANCE ISSUE IF THIS IS CALLED DIRECTLY FROM MW::fileSelectionchange.
Apparently there needs to be a slight delay before calling.

*/
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    imageCacheTimer->start(50);
}

void MW::loadMetadataCache2ndPass()
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    updateIconBestFit();
//    currentRow = 0;
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

Finally metadataCacheThread->scrollChange is called to load any necessary metadata and icons
within the cache range.
*/
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    /*
       Scrolling used to use a singleshot timer triggered by MW::loadMetadataCacheAfterDelay
    to call MW::loadMetadataChunk which, in turn, finally called the metadataCacheThread.
    This was to prevent many scroll calls from bunching up.  The new approach just aborts an
    existing thread and starts over.  It is simpler and faster.  Keeping the old process until
    the new one is proven to work all the time.
      */

//    qDebug() << __FUNCTION__ << G::ignoreScrollSignal;
    if (G::isInitializing || !G::isNewFolderLoaded) return;

    if (G::ignoreScrollSignal == false) {
        G::ignoreScrollSignal = true;
        updateIconsVisible(false);
//        if (gridView->isVisible())
            gridView->scrollToRow(thumbView->midVisibleCell, __FUNCTION__);
        if (tableView->isVisible())
            tableView->scrollToRow(thumbView->midVisibleCell, __FUNCTION__);
        metadataCacheThread->scrollChange();
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

Finally metadataCacheThread->scrollChange is called to load any necessary metadata and icons
within the cache range.
*/
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    /*
       Scrolling used to use a singleshot timer triggered by MW::loadMetadataCacheAfterDelay
    to call MW::loadMetadataChunk which, in turn, finally called the metadataCacheThread.
    This was to prevent many scroll calls from bunching up.  The new approach just aborts an
    existing thread and starts over.  It is simpler and faster.  Keeping the old process until
    the new one is proven to work all the time.
      */

//    qDebug() << __FUNCTION__ << G::ignoreScrollSignal;
    if (G::isInitializing || !G::isNewFolderLoaded) return;

    if (G::ignoreScrollSignal == false) {
        G::ignoreScrollSignal = true;
        updateIconsVisible(false);
//        if (thumbView->isVisible())
            thumbView->scrollToRow(gridView->midVisibleCell, __FUNCTION__);
        metadataCacheThread->scrollChange();
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

Finally metadataCacheThread->scrollChange is called to load any necessary metadata and icons
within the cache range.
*/
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    /*
       Scrolling used to use a singleshot timer triggered by MW::loadMetadataCacheAfterDelay
    to call MW::loadMetadataChunk which, in turn, finally called the metadataCacheThread.
    This was to prevent many scroll calls from bunching up.  The new approach just aborts an
    existing thread and starts over.  It is simpler and faster.  Keeping the old process until
    the new one is proven to work all the time.
      */

//    qDebug() << __FUNCTION__ << G::ignoreScrollSignal;
    if (G::isInitializing || !G::isNewFolderLoaded) return;

    if (G::ignoreScrollSignal == false) {
        G::ignoreScrollSignal = true;
        updateIconsVisible(false);
        if (thumbView->isVisible())
            thumbView->scrollToRow(tableView->midVisibleRow, __FUNCTION__);
        metadataCacheThread->scrollChange();
    }
    G::ignoreScrollSignal = false;
}

void MW::numberIconsVisibleChange()
{
/*
If there has not been another call to this function in cacheDelay ms then the
metadataCacheThread is restarted at the row of the first visible thumb after the scrolling.
*/
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    if (G::isInitializing || !G::isNewFolderLoaded) return;
    updateIconsVisible(true);
    metadataCacheThread->sizeChange();
}

void MW::loadMetadataCacheAfterDelay()
{
/*
See MetadataCache::run comments in mdcache.cpp. A 100ms singleshot timer insures that the
metadata caching is not restarted until there is a pause in the scolling.

The timer is fired from MW::eventFilter, which monitors the value change signal in the
gridView vertical scrollbar (does not have a horizontal scrollbar) and the thumbView vertical
and horizontal scrollbars. It is also connected to the resize event in IconView, as a resize
could increase the number of thumbs visible (ie resize to full screen) that require metadata
or icons to be cached.

After the delay, if another singleshot has not been fired, loadMetadataChunk is called.

Since this function is called from scroll or resize events the selected image does not change
and there is no need to update the image cache.  The image cache is updated from
MW::fileSelectionChange.
*/
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
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
metadataCacheThread is restarted at the row of the first visible thumb after the scrolling.
*/
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
//    if (/*G::isInitializing || */dm->sf->rowCount() == 0/* || !G::isNewFolderLoaded*/) return;
//    if (dm->sf->rowCount() == 0) return;
//    updateMetadataCacheIconviewState();
    metadataCacheThread->scrollChange();
}

void MW::loadEntireMetadataCache()
{
/*
This is called before a filter or sort operation, which only makes sense if all the metadata
has been loaded. This function does not load the icons. It is not run in a separate thread as
the filter and sort operations cannot commence until all the metadata has been loaded.
*/
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    if (G::isInitializing) return;
    if (metadataCacheThread->isAllMetadataLoaded()) return;

    updateIconsVisible(true);

    bool resumeImageCaching = false;
    if (imageCacheThread->isRunning()) {
        imageCacheThread->pauseImageCache();
        resumeImageCaching = true;
    }
//    popup("It may take a moment to load all the metadata...\n"
//          "This is required before any filtering or sorting of metadata can be done.",
//          0, 0.75, true);
//    updateStatus(false, "Loading metadata for all images");
    QApplication::setOverrideCursor(Qt::WaitCursor);
    progressBar->saveProgressState();
    progressBar->clearProgress();
    QApplication::processEvents();
    int rows = dm->rowCount();
    // update progress for already loaded metadata
    for (int row = 0; row < rows; ++row) {
        if (dm->index(row, G::MetadataLoadedColumn).data().toBool()) {
            progressBar->updateProgress(row, row + 1, rows, G::progressAddMetadataColor, "");
        }
    }
    QApplication::processEvents();

    /* adding all metadata in dm slightly slower than using metadataCacheThread but progress
       bar does not update from separate thread
    */
    dm->addAllMetadata(true);
    // metadataCacheThread->loadAllMetadata();
    // metadataCacheThread->wait();

//    dm->buildFilters();

    progressBar->recoverProgressState();
    if (resumeImageCaching) imageCacheThread->resumeImageCache();
    QApplication::restoreOverrideCursor();
//    popup("Metadata loaded.", 500, 0.75, true);
//    QApplication::processEvents();
}

void MW::refreshCurrentAfterReload()
{
/*

*/
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    G::isNewFolderLoaded = true;

    int dmRow = dm->fPathRow[refreshCurrentPath];
    loadImageCacheForNewFolder();

    // wait for thumbView and gridView to be ready for selection and scrolling
    G::wait(300);

    // return to selected image before refresh.  This triggers an update to the metadata and
    // imageg caches
    thumbView->selectThumb(dmRow);
    isRefreshingDM = false;
}

void MW::updateMetadataCacheStatus(int row, bool clear)
{
    /* Displays a statusbar showing the metadata cache status.

    If clear = true then just repaint the progress bar gray and return.
    If clear = false then update the progress for the row that has been cached.
    */
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }

    if(!isShowCacheStatus) return;

    if(clear) {
        progressBar->clearProgress();
        return;
    }

    // show the rectangle for the current cached
    progressBar->updateProgress(row, row + 1, dm->rowCount(),
                                G::progressAddMetadataColor,
                                "");
    return;
}

void MW::updateImageCacheStatus(QString instruction, int row, QString source)
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    /* Displays a statusbar showing the metadata cache status.  Also shows the cache
    size in the info panel.
    */
//    qDebug() << __FUNCTION__;
    if (G::isSlideShow && isSlideShowRandom) return;

    source = "";    // suppress compiler warning
/*    qDebug() << "MW::updateImageCacheStatus  Instruction ="
             << instruction
             << "row =" << row
             << "source =" << source;
             */

    // show cache amount ie "4.2 of 16GB" in info panel
    QString cacheAmount = QString::number(double(imageCacheThread->cache.currMB)/1024,'f',1)
            + " of "
            + QString::number(imageCacheThread->cache.maxMB/1024) + "GB";
    QStandardItemModel *k = infoView->ok;
    k->setData(k->index(infoView->CacheRow, 1, infoView->statusInfoIdx), cacheAmount);

    if(!isShowCacheStatus) return;

    // just repaint the progress bar gray and return.
    if(instruction == "Clear") {
        progressBar->clearProgress();
        return;
    }

    // create a short alias to keep code shorter
    ImageCache *ic = imageCacheThread;
    int rows = ic->cache.totFiles;

    if (instruction == "Update cursor") {
        progressBar->updateCursor(row, rows, G::progressCurrentColor, G::progressImageCacheColor);
        return;
    }

    if(instruction == "Update all rows") {
        // clear progress
        progressBar->clearProgress();
        // target range
        int tFirst = ic->cache.targetFirst;
        int tLast = ic->cache.targetLast + 1;
        progressBar->updateProgress(tFirst, tLast, rows, G::progressTargetColor,
                                    "");
        // cached
        for (int i = 0; i < rows; ++i) {
            // bail if list has been cleared because new folder selected
            if (ic->cacheItemList.isEmpty()) return;
            if (ic->cacheItemList.at(i).isCached)
                progressBar->updateProgress(i, i + 1, rows, G::progressImageCacheColor,
                                            "");
        }
        // cursor
        row = currentRow;
        progressBar->updateCursor(row, rows, G::progressCurrentColor, G::progressImageCacheColor);
        return;
    }

    return;
}

void MW::updateIconBestFit()
{
/*
This function is signalled from MetadataCache after each chunk of icons have been processed.
The best aspect of the thumbnail cells is calculated. The repainting of the thumbs triggers an
unwanted scrolling, so the first visible thumbnail is recorded before the bestAspect is called
and the IconView is returned to its previous position after.
*/
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    gridView->bestAspect();
    thumbView->bestAspect();
}

void MW::loadImageCacheForNewFolder()
{
/*
This function is signaled from the metadataCacheThread after the metadata chunk has been
loaded for a new folder selection. The imageCache loads images until the assigned amount of
memory has been consumed or all the images are cached.
*/
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    // now that metadata is loaded populate the data model
    if(isShowCacheStatus) progressBar->clearProgress();
    qApp->processEvents();
    updateIconBestFit();

    // had to wait for the data before resize table columns
    tableView->resizeColumnsToContents();
    tableView->setColumnWidth(G::PathColumn, 24+8);
    QModelIndexList indexesList = selectionModel->selectedIndexes();
    QString fPath;
    if (indexesList.isEmpty())
        fPath = dm->sf->index(0, G::PathColumn).data().toString();
    else
        fPath = indexesList.first().data(G::PathRole).toString();

    // the folder does not have any eligible images (probably from previous session and the
    // drive has been removed or did have "include subfolders" activated
    if (fPath == "") return;

    // set image cache parameters and build image cacheItemList
    imageCacheThread->initImageCache(cacheSizeMB,
        isShowCacheStatus, cacheWtAhead, isCachePreview,
        cachePreviewWidth, cachePreviewHeight);

    // have to wait until image caching thread running before setting flag
    metadataLoaded = true;

    // tell image cache new position
    imageCacheThread->updateImageCachePosition(/*fPath*/);

    G::isNewFolderLoaded = true;

    // set focus when program opens
    if (G::mode == "Loupe") thumbView->setFocus();
    if (G::mode == "Grid") gridView->setFocus();
    if (G::mode == "Table") tableView->setFocus();
}

// called by signal itemClicked in bookmark
void MW::bookmarkClicked(QTreeWidgetItem *item, int col)
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
//    QString fPath = item->text(0);
    const QString fPath =  item->toolTip(col);
    isCurrentFolderOkay = isFolderValid(fPath, true, false);

    if (isCurrentFolderOkay) {
        QModelIndex idx = fsTree->fsModel->index(fPath);
//        QModelIndex idx = fsTree->fsModel->index(item->toolTip(col));
        QModelIndex filterIdx = fsTree->fsFilter->mapFromSource(idx);
        fsTree->setCurrentIndex(filterIdx);
        fsTree->scrollTo(filterIdx, QAbstractItemView::PositionAtCenter);
        folderSelectionChange();
    }
    else {
        clearAll();
        enableSelectionDependentMenus();
    }
}

// called when signal rowsRemoved from FSTree
// does this get triggered if a drive goes offline???
// rgh this needs some TLC
void MW::checkDirState(const QModelIndex &, int, int)
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    if (G::isInitializing) return;

    if (!QDir().exists(currentViewDir)) {
        currentViewDir = "";
    }
}

QString MW::getSelectedPath()
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    if (isDragDrop)  return dragDropFolderPath;

    if (fsTree->selectionModel()->selectedRows().size() == 0) return "";

    QModelIndex idx = fsTree->selectionModel()->selectedRows().at(0);
    if (!idx.isValid()) return "";

    QString path = idx.data(QFileSystemModel::FilePathRole).toString();
    QFileInfo dirInfo = QFileInfo(path);
    return dirInfo.absoluteFilePath();
}

void MW::createActions()
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }

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
    connect(refreshCurrentAction, &QAction::triggered, this, &MW::refreshCurrent);

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

    /* read external apps from QStettings */
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

    subFoldersAction = new QAction(tr("Include Sub-folders"), this);
    subFoldersAction->setObjectName("subFolders");
    subFoldersAction->setShortcutVisibleInContextMenu(true);
    subFoldersAction->setCheckable(true);
    subFoldersAction->setChecked(false);
    addAction(subFoldersAction);
    connect(subFoldersAction, &QAction::triggered, this, &MW::updateStatusBar);

    refreshFoldersAction = new QAction(tr("Refresh folders"), this);
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

    addBookmarkAction = new QAction(tr("Add Bookmark"), this);
    addBookmarkAction->setObjectName("addBookmark");
    addBookmarkAction->setShortcutVisibleInContextMenu(true);
    addAction(addBookmarkAction);
    connect(addBookmarkAction, &QAction::triggered, this, &MW::addNewBookmark);

    addBookmarkActionFromContext = new QAction(tr("Add Bookmark"), this);
    addBookmarkActionFromContext->setObjectName("addBookmark");
    addBookmarkActionFromContext->setShortcutVisibleInContextMenu(true);
    addAction(addBookmarkActionFromContext);
    connect(addBookmarkActionFromContext, &QAction::triggered, this, &MW::addNewBookmarkFromContext);

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

    combineRawJpgAction = new QAction(tr("Combine Raw+Jpg"), this);
    combineRawJpgAction->setObjectName("combineRawJpg");
    combineRawJpgAction->setShortcutVisibleInContextMenu(true);
    combineRawJpgAction->setCheckable(true);
    if (isSettings) combineRawJpgAction->setChecked(setting->value("combineRawJpg").toBool());
    else combineRawJpgAction->setChecked(true);
    addAction(combineRawJpgAction);
    connect(combineRawJpgAction, &QAction::triggered, this, &MW::setCombineRawJpg);

    // Place keeper for now
    renameAction = new QAction(tr("Rename selected images"), this);
    renameAction->setObjectName("renameImages");
    renameAction->setShortcutVisibleInContextMenu(true);
    renameAction->setShortcut(Qt::Key_F2);
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

    pick1Action = new QAction(tr("Pick"), this);
    addAction(pick1Action);
    connect(pick1Action, &QAction::triggered, this, &MW::togglePick);

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

    // Place keeper for now
    copyAction = new QAction(tr("Copy to clipboard"), this);
    copyAction->setObjectName("copyImages");
    copyAction->setShortcutVisibleInContextMenu(true);
    copyAction->setShortcut(QKeySequence("Ctrl+C"));
    addAction(copyAction);
    connect(copyAction, &QAction::triggered, this, &MW::copy);

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
    connect(prefAction, &QAction::triggered, [this]() {preferences(-1);} );

    prefInfoAction = new QAction(tr("Hide or show info fields"), this);
    prefInfoAction->setObjectName("infosettings");
    prefInfoAction->setShortcutVisibleInContextMenu(true);
    addAction(prefInfoAction);
    connect(prefInfoAction, &QAction::triggered, [this]() {preferences(7);} );

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

    expandAllAction = new QAction(tr("Expand all filters"), this);
    expandAllAction->setObjectName("expandAll");
    expandAllAction->setShortcutVisibleInContextMenu(true);
    addAction(expandAllAction);
    connect(expandAllAction, &QAction::triggered, filters, &Filters::expandAllFilters);

    collapseAllAction = new QAction(tr("Collapse all filters"), this);
    collapseAllAction->setObjectName("collapseAll");
    collapseAllAction->setShortcutVisibleInContextMenu(true);
    addAction(collapseAllAction);
    connect(collapseAllAction, &QAction::triggered, filters, &Filters::collapseAllFilters);

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
    connect(filterUpdateAction,  &QAction::triggered, this, &MW::buildFilters);

    // Sort Menu

    sortFileNameAction = new QAction(tr("Sort by file name"), this);
    sortFileNameAction->setShortcutVisibleInContextMenu(true);
    sortFileNameAction->setCheckable(true);
    addAction(sortFileNameAction);
    connect(sortFileNameAction, &QAction::triggered, this, &MW::sortChange);

    sortFileTypeAction = new QAction(tr("Sort by file type"), this);
    sortFileTypeAction->setShortcutVisibleInContextMenu(true);
    sortFileTypeAction->setCheckable(true);
    addAction(sortFileTypeAction);
    connect(sortFileTypeAction, &QAction::triggered, this, &MW::sortChange);

    sortFileSizeAction = new QAction(tr("Sort by file size"), this);
    sortFileSizeAction->setShortcutVisibleInContextMenu(true);
    sortFileSizeAction->setCheckable(true);
    addAction(sortFileSizeAction);
    connect(sortFileSizeAction, &QAction::triggered, this, &MW::sortChange);

    sortCreateAction = new QAction(tr("Sort by created time"), this);
    sortCreateAction->setShortcutVisibleInContextMenu(true);
    sortCreateAction->setCheckable(true);
    addAction(sortCreateAction);
    connect(sortCreateAction, &QAction::triggered, this, &MW::sortChange);

    sortModifyAction = new QAction(tr("Sort by last modified time"), this);
    sortModifyAction->setShortcutVisibleInContextMenu(true);
    sortModifyAction->setCheckable(true);
    addAction(sortModifyAction);
    connect(sortModifyAction, &QAction::triggered, this, &MW::sortChange);

    sortPickAction = new QAction(tr("Sort by picked status"), this);
    sortPickAction->setShortcutVisibleInContextMenu(true);
    sortPickAction->setCheckable(true);
    addAction(sortPickAction);
    connect(sortPickAction, &QAction::triggered, this, &MW::sortChange);

    sortLabelAction = new QAction(tr("Sort by label"), this);
    sortLabelAction->setShortcutVisibleInContextMenu(true);
    sortLabelAction->setCheckable(true);
    addAction(sortLabelAction);
    connect(sortLabelAction, &QAction::triggered, this, &MW::sortChange);

    sortRatingAction = new QAction(tr("Sort by rating"), this);
    sortRatingAction->setShortcutVisibleInContextMenu(true);
    sortRatingAction->setCheckable(true);
    addAction(sortRatingAction);
    connect(sortRatingAction, &QAction::triggered, this, &MW::sortChange);

    sortMegaPixelsAction = new QAction(tr("Sort by megapixels"), this);
    sortMegaPixelsAction->setShortcutVisibleInContextMenu(true);
    sortMegaPixelsAction->setCheckable(true);
    addAction(sortMegaPixelsAction);
    connect(sortMegaPixelsAction, &QAction::triggered, this, &MW::sortChange);

    sortDimensionsAction = new QAction(tr("Sort by dimensions"), this);
    sortDimensionsAction->setShortcutVisibleInContextMenu(true);
    sortDimensionsAction->setCheckable(true);
    addAction(sortDimensionsAction);
    connect(sortDimensionsAction, &QAction::triggered, this, &MW::sortChange);

    sortApertureAction = new QAction(tr("Sort by aperture"), this);
//    sortApertureAction->setObjectName("SortAperture");
    sortApertureAction->setShortcutVisibleInContextMenu(true);
    sortApertureAction->setCheckable(true);
    addAction(sortApertureAction);
    connect(sortApertureAction, &QAction::triggered, this, &MW::sortChange);

    sortShutterSpeedAction = new QAction(tr("Sort by shutter speed"), this);
    sortShutterSpeedAction->setShortcutVisibleInContextMenu(true);
    sortShutterSpeedAction->setCheckable(true);
    addAction(sortShutterSpeedAction);
    connect(sortShutterSpeedAction, &QAction::triggered, this, &MW::sortChange);

    sortISOAction = new QAction(tr("Sort by ISO"), this);
    sortISOAction->setShortcutVisibleInContextMenu(true);
    sortISOAction->setCheckable(true);
    addAction(sortISOAction);
    connect(sortISOAction, &QAction::triggered, this, &MW::sortChange);

    sortModelAction = new QAction(tr("Sort by camera model"), this);
    sortModelAction->setShortcutVisibleInContextMenu(true);
    sortModelAction->setCheckable(true);
    addAction(sortModelAction);
    connect(sortModelAction, &QAction::triggered, this, &MW::sortChange);

    sortFocalLengthAction = new QAction(tr("Sort by focal length"), this);
    sortFocalLengthAction->setShortcutVisibleInContextMenu(true);
    sortFocalLengthAction->setCheckable(true);
    addAction(sortFocalLengthAction);
    connect(sortFocalLengthAction, &QAction::triggered, this, &MW::sortChange);

    sortTitleAction = new QAction(tr("Sort by title"), this);
    sortTitleAction->setShortcutVisibleInContextMenu(true);
    sortTitleAction->setCheckable(true);
    addAction(sortTitleAction);
    connect(sortTitleAction, &QAction::triggered, this, &MW::sortChange);

    sortLensAction = new QAction(tr("Sort by lens"), this);
    sortLensAction->setShortcutVisibleInContextMenu(true);
    sortLensAction->setCheckable(true);
    addAction(sortLensAction);
    connect(sortLensAction, &QAction::triggered, this, &MW::sortChange);

    sortCreatorAction = new QAction(tr("Sort by creator"), this);
    sortCreatorAction->setShortcutVisibleInContextMenu(true);
    sortCreatorAction->setCheckable(true);
    addAction(sortCreatorAction);
    connect(sortCreatorAction, &QAction::triggered, this, &MW::sortChange);

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

    sortFileNameAction->setChecked(true);

    sortReverseAction = new QAction(tr("Reverse sort order"), this);
    sortReverseAction->setObjectName("reverse");
    sortReverseAction->setShortcutVisibleInContextMenu(true);
    sortReverseAction->setCheckable(true);
//    sortReverseAction->setChecked(false);
    addAction(sortReverseAction);
    connect(sortReverseAction, &QAction::triggered, this, &MW::sortChange);

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
    if (isSettings) fullScreenAction->setChecked(setting->value("isFullScreen").toBool());
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
    if (isSettings) ratingBadgeVisibleAction->setChecked(setting->value("isRatingBadgeVisible").toBool());
    else ratingBadgeVisibleAction->setChecked(true);
    addAction(ratingBadgeVisibleAction);
    connect(ratingBadgeVisibleAction, &QAction::triggered, this, &MW::setRatingBadgeVisibility);

    infoVisibleAction = new QAction(tr("Show Shooting Info"), this);
    infoVisibleAction->setObjectName("toggleInfo");
    infoVisibleAction->setShortcutVisibleInContextMenu(true);
    infoVisibleAction->setCheckable(true);
    addAction(infoVisibleAction);
    connect(infoVisibleAction, &QAction::triggered, this, &MW::setShootingInfoVisibility);

    infoSelectAction = new QAction(tr("Select or edit Shooting Info"), this);
    infoSelectAction->setShortcutVisibleInContextMenu(true);
    infoSelectAction->setObjectName("selectInfo");
    if (isSettings) infoVisibleAction->setChecked(setting->value("isImageInfoVisible").toBool());
    else infoVisibleAction->setChecked(false);
    addAction(infoSelectAction);
    connect(infoSelectAction, &QAction::triggered, this, &MW::selectShootingInfo);

    asLoupeAction = new QAction(tr("Loupe"), this);
    asLoupeAction->setShortcutVisibleInContextMenu(true);
    asLoupeAction->setCheckable(true);
    if (isSettings) asLoupeAction->setChecked(setting->value("isLoupeDisplay").toBool() ||
                              setting->value("isCompareDisplay").toBool());
    else asLoupeAction->setChecked(false);
    addAction(asLoupeAction);
    connect(asLoupeAction, &QAction::triggered, this, &MW::loupeDisplay);

    asGridAction = new QAction(tr("Grid"), this);
    asGridAction->setShortcutVisibleInContextMenu(true);
    asGridAction->setCheckable(true);
    if (isSettings) asGridAction->setChecked(setting->value("isGridDisplay").toBool());
    else asGridAction->setChecked(true);
    addAction(asGridAction);
    connect(asGridAction, &QAction::triggered, this, &MW::gridDisplay);

    asTableAction = new QAction(tr("Table"), this);
    asTableAction->setShortcutVisibleInContextMenu(true);
    asTableAction->setCheckable(true);
    if (isSettings) asTableAction->setChecked(setting->value("isTableDisplay").toBool());
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

    zoomToAction = new QAction(tr("Zoom To"), this);
    zoomToAction->setObjectName("zoomTo");
    zoomToAction->setShortcutVisibleInContextMenu(true);
    addAction(zoomToAction);
    connect(zoomToAction, &QAction::triggered, this, &MW::updateZoom);

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

//    thumbsWrapAction = new QAction(tr("Wrap thumbs"), this);
//    thumbsWrapAction->setObjectName("wrapThumbs");
//    thumbsWrapAction->setShortcutVisibleInContextMenu(true);
//    thumbsWrapAction->setCheckable(true);
//    addAction(thumbsWrapAction);
//    connect(thumbsWrapAction, &QAction::triggered, this, &MW::toggleThumbWrap);

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

//    thumbsFitAction = new QAction(tr("Fit thumbs"), this);
//    thumbsFitAction->setObjectName("thumbsZoomOut");
//    thumbsFitAction->setShortcutVisibleInContextMenu(true);
//    addAction(thumbsFitAction);
//    connect(thumbsFitAction, &QAction::triggered, this, &MW::setDockFitThumbs);

//    showThumbLabelsAction = new QAction(tr("Thumb labels"), this);
//    showThumbLabelsAction->setObjectName("showLabels");
//    showThumbLabelsAction->setCheckable(true);
////    showThumbLabelsAction->setChecked(thumbView->showThumbLabels);
//    connect(showThumbLabelsAction, SIGNAL(triggered()), this, SLOT(setThumbLabels()));
//    showThumbLabelsAction->setEnabled(true);

    // Window menu Visibility actions

//    windowTitleBarVisibleAction = new QAction(tr("Window Titlebar"), this);
//    windowTitleBarVisibleAction->setObjectName("toggleWindowsTitleBar");
//    windowTitleBarVisibleAction->setShortcutVisibleInContextMenu(true);
//    windowTitleBarVisibleAction->setCheckable(true);
//    addAction(windowTitleBarVisibleAction);
//    connect(windowTitleBarVisibleAction, &QAction::triggered, this, &MW::setWindowsTitleBarVisibility);

//#ifdef Q_OS_WIN
    menuBarVisibleAction = new QAction(tr("Menubar"), this);
    menuBarVisibleAction->setObjectName("toggleMenuBar");
    menuBarVisibleAction->setShortcutVisibleInContextMenu(true);
    menuBarVisibleAction->setCheckable(true);
    if (isSettings) menuBarVisibleAction->setChecked(setting->value("isMenuBarVisible").toBool());
    else menuBarVisibleAction->setChecked(true);
    addAction(menuBarVisibleAction);
    connect(menuBarVisibleAction, &QAction::triggered, this, &MW::setMenuBarVisibility);
//#endif

    statusBarVisibleAction = new QAction(tr("Statusbar"), this);
    statusBarVisibleAction->setObjectName("toggleStatusBar");
    statusBarVisibleAction->setShortcutVisibleInContextMenu(true);
    statusBarVisibleAction->setCheckable(true);
    if (isSettings) statusBarVisibleAction->setChecked(setting->value("isStatusBarVisible").toBool());
    else statusBarVisibleAction->setChecked(true);
    addAction(statusBarVisibleAction);
    connect(statusBarVisibleAction, &QAction::triggered, this, &MW::setStatusBarVisibility);

    folderDockVisibleAction = new QAction(tr("Folder"), this);
    folderDockVisibleAction->setObjectName("toggleFiless");
    folderDockVisibleAction->setShortcutVisibleInContextMenu(true);
    folderDockVisibleAction->setCheckable(true);
    if (isSettings) folderDockVisibleAction->setChecked(setting->value("isFolderDockVisible").toBool());
    else folderDockVisibleAction->setChecked(true);
    addAction(folderDockVisibleAction);
    connect(folderDockVisibleAction, &QAction::triggered, this, &MW::toggleFolderDockVisibility);

    favDockVisibleAction = new QAction(tr("Bookmarks"), this);
    favDockVisibleAction->setObjectName("toggleFavs");
    favDockVisibleAction->setShortcutVisibleInContextMenu(true);
    favDockVisibleAction->setCheckable(true);
    if (isSettings) favDockVisibleAction->setChecked(setting->value("isFavDockVisible").toBool());
    else favDockVisibleAction->setChecked(true);
    addAction(favDockVisibleAction);
    connect(favDockVisibleAction, &QAction::triggered, this, &MW::toggleFavDockVisibility);

    filterDockVisibleAction = new QAction(tr("Filters"), this);
    filterDockVisibleAction->setObjectName("toggleFilters");
    filterDockVisibleAction->setShortcutVisibleInContextMenu(true);
    filterDockVisibleAction->setCheckable(true);
    if (isSettings) filterDockVisibleAction->setChecked(setting->value("isFilterDockVisible").toBool());
    else filterDockVisibleAction->setChecked(true);
    addAction(filterDockVisibleAction);
    connect(filterDockVisibleAction, &QAction::triggered, this, &MW::toggleFilterDockVisibility);

    metadataDockVisibleAction = new QAction(tr("Metadata"), this);
    metadataDockVisibleAction->setObjectName("toggleMetadata");
    metadataDockVisibleAction->setShortcutVisibleInContextMenu(true);
    metadataDockVisibleAction->setCheckable(true);
    if (isSettings) metadataDockVisibleAction->setChecked(setting->value("isMetadataDockVisible").toBool());
    else metadataDockVisibleAction->setChecked(true);
    addAction(metadataDockVisibleAction);
    connect(metadataDockVisibleAction, &QAction::triggered, this, &MW::toggleMetadataDockVisibility);

    thumbDockVisibleAction = new QAction(tr("Thumbnails"), this);
    thumbDockVisibleAction->setObjectName("toggleThumbs");
    thumbDockVisibleAction->setShortcutVisibleInContextMenu(true);
    thumbDockVisibleAction->setCheckable(true);
    if (isSettings) thumbDockVisibleAction->setChecked(setting->value("isThumbDockVisible").toBool());
    else thumbDockVisibleAction->setChecked(true);
    addAction(thumbDockVisibleAction);
    connect(thumbDockVisibleAction, &QAction::triggered, this, &MW::toggleThumbDockVisibity);

/*    Window menu focus actions replaced by three way toggle focus/visibility

    folderDockFocusAction = new QAction(tr("Focus on Folders"), this);
    folderDockFocusAction->setObjectName("FocusFolders");
    folderDockFocusAction->setShortcutVisibleInContextMenu(true);
    addAction(folderDockFocusAction);
    connect(folderDockFocusAction, &QAction::triggered, this, &MW::setFolderDockFocus);

    favDockFocusAction = new QAction(tr("Focus on Favourites"), this);
    favDockFocusAction->setObjectName("FocusFavourites");
    favDockFocusAction->setShortcutVisibleInContextMenu(true);
    addAction(favDockFocusAction);
    connect(favDockFocusAction, &QAction::triggered, this, &MW::setFavDockFocus);

    filterDockFocusAction = new QAction(tr("Focus on Filters"), this);
    filterDockFocusAction->setObjectName("FocusFilters");
    filterDockFocusAction->setShortcutVisibleInContextMenu(true);
    addAction(filterDockFocusAction);
    connect(filterDockFocusAction, &QAction::triggered, this, &MW::setFilterDockFocus);

    metadataDockFocusAction = new QAction(tr("Focus on Metadata"), this);
    metadataDockFocusAction->setObjectName("FocusMetadata");
    metadataDockFocusAction->setShortcutVisibleInContextMenu(true);
    addAction(metadataDockFocusAction);
    connect(metadataDockFocusAction, &QAction::triggered, this, &MW::setMetadataDockFocus);

    thumbDockFocusAction = new QAction(tr("Focus on Thumbs"), this);
    thumbDockFocusAction->setObjectName("FocusThumbs");
    thumbDockFocusAction->setShortcutVisibleInContextMenu(true);
    addAction(thumbDockFocusAction);
    connect(thumbDockFocusAction, &QAction::triggered, this, &MW::setThumbDockFocus);
*/

    // Lock docks (hide titlebar) actions
    folderDockLockAction = new QAction(tr("Hide Folder Titlebar"), this);
    folderDockLockAction->setObjectName("lockDockFiles");
    folderDockLockAction->setShortcutVisibleInContextMenu(true);
    folderDockLockAction->setCheckable(true);
    if (isSettings) folderDockLockAction->setChecked(setting->value("isFolderDockLocked").toBool());
    else folderDockLockAction->setChecked(false);
    addAction(folderDockLockAction);
    connect(folderDockLockAction, &QAction::triggered, this, &MW::setFolderDockLockMode);

    favDockLockAction = new QAction(tr("Hide Favourite titlebar"), this);
    favDockLockAction->setObjectName("lockDockFavs");
    favDockLockAction->setShortcutVisibleInContextMenu(true);
    favDockLockAction->setCheckable(true);
    if (isSettings) favDockLockAction->setChecked(setting->value("isFavDockLocked").toBool());
    else favDockLockAction->setChecked(false);
    addAction(favDockLockAction);
    connect(favDockLockAction, &QAction::triggered, this, &MW::setFavDockLockMode);

    filterDockLockAction = new QAction(tr("Hide Filter titlebars"), this);
    filterDockLockAction->setObjectName("lockDockFilters");
    filterDockLockAction->setShortcutVisibleInContextMenu(true);
    filterDockLockAction->setCheckable(true);
    if (isSettings) filterDockLockAction->setChecked(setting->value("isFilterDockLocked").toBool());
    else filterDockLockAction->setChecked(false);
    addAction(filterDockLockAction);
    connect(filterDockLockAction, &QAction::triggered, this, &MW::setFilterDockLockMode);

    metadataDockLockAction = new QAction(tr("Hide Metadata Titlebar"), this);
    metadataDockLockAction->setObjectName("lockDockMetadata");
    metadataDockLockAction->setShortcutVisibleInContextMenu(true);
    metadataDockLockAction->setCheckable(true);
    if (isSettings) metadataDockLockAction->setChecked(setting->value("isMetadataDockLocked").toBool());
    else metadataDockLockAction->setChecked(false);
    addAction(metadataDockLockAction);
    connect(metadataDockLockAction, &QAction::triggered, this, &MW::setMetadataDockLockMode);

    thumbDockLockAction = new QAction(tr("Hide Thumbs Titlebar"), this);
    thumbDockLockAction->setObjectName("lockDockThumbs");
    thumbDockLockAction->setShortcutVisibleInContextMenu(true);
    thumbDockLockAction->setCheckable(true);
    if (isSettings) thumbDockLockAction->setChecked(setting->value("isThumbDockLocked").toBool());
    else thumbDockLockAction->setChecked(true);
    addAction(thumbDockLockAction);
    connect(thumbDockLockAction, &QAction::triggered, this, &MW::setThumbDockLockMode);

    allDocksLockAction = new QAction(tr("Hide All Titlebars"), this);
    allDocksLockAction->setObjectName("lockDocks");
    allDocksLockAction->setShortcutVisibleInContextMenu(true);
    allDocksLockAction->setCheckable(true);
    addAction(allDocksLockAction);
    connect(allDocksLockAction, &QAction::triggered, this, &MW::setAllDocksLockMode);

    if (folderDockLockAction->isChecked() &&
        favDockLockAction->isChecked() &&
        filterDockLockAction->isChecked() &&
        metadataDockLockAction->isChecked() &&
        thumbDockLockAction->isChecked())
        allDocksLockAction->setChecked(true);
    if (isSettings) wasThumbDockVisible = setting->value("wasThumbDockVisible").toBool();
    else wasThumbDockVisible = true;

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

// connection moved to after menu creation as will not work before
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

    // Help Diagnostics

    diagnosticsAllAction = new QAction(tr("All Diagnostics"), this);
    diagnosticsAllAction->setObjectName("diagnosticsAll");
    diagnosticsAllAction->setShortcutVisibleInContextMenu(true);
    addAction(diagnosticsAllAction);
    connect(diagnosticsAllAction, &QAction::triggered, this, &MW::diagnosticsAll);

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
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    // Main Menu

    // File Menu

    QMenu *fileMenu = new QMenu(this);
    QAction *fileGroupAct = new QAction("File", this);
    fileGroupAct->setMenu(fileMenu);
    fileMenu->addAction(openAction);
    fileMenu->addAction(refreshCurrentAction);
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
//    fileMenu->addAction(showImageCountAction);
    fileMenu->addAction(combineRawJpgAction);
    fileMenu->addAction(subFoldersAction);
     fileMenu->addAction(addBookmarkAction);
    fileMenu->addSeparator();
    fileMenu->addAction(revealFileAction);
    fileMenu->addAction(refreshFoldersAction);
    fileMenu->addAction(collapseFoldersAction);
    fileMenu->addSeparator();
    fileMenu->addAction(reportMetadataAction);
    fileMenu->addSeparator();
    fileMenu->addAction(exitAction);    // Appears in Winnow menu in OSX

    // Edit Menu

    QMenu *editMenu = new QMenu(this);
    QAction *editGroupAct = new QAction("Edit", this);
    editGroupAct->setMenu(editMenu);
    editMenu->addAction(selectAllAction);
    editMenu->addAction(invertSelectionAction);
    editMenu->addSeparator();
    editMenu->addAction(copyAction);
    editMenu->addSeparator();
    editMenu->addAction(refineAction);
    editMenu->addAction(pickAction);
//    editMenu->addAction(filterPickAction);
    editMenu->addAction(popPickHistoryAction);
    editMenu->addSeparator();
//    editMenu->addAction(copyImagesAction);
//    editMenu->addSeparator();
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
    editMenu->addSeparator();
    editMenu->addAction(rotateRightAction);
    editMenu->addAction(rotateLeftAction);
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
    filterMenu->addAction(clearAllFiltersAction);
    filterMenu->addSeparator();
    filterMenu->addAction(filterPickAction);
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
    filterMenu->addAction(filterInvertAction);
    filterMenu->addAction(filterLastDayAction);

    // Sort Menu

    QMenu *sortMenu = new QMenu(this);
    QAction *sortGroupAct = new QAction("Sort", this);
    sortGroupAct->setMenu(sortMenu);
    sortMenu->addActions(sortGroupAction->actions());
    sortMenu->addSeparator();
    sortMenu->addAction(sortReverseAction);

    // View Menu

    QMenu *viewMenu = new QMenu(this);
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
//    windowMenu->addAction(windowTitleBarVisibleAction);
#ifdef Q_OS_WIN
    windowMenu->addAction(menuBarVisibleAction);
#endif
    windowMenu->addAction(statusBarVisibleAction);
//    windowMenu->addSeparator();
//    windowMenu->addAction(folderDockFocusAction);
//    windowMenu->addAction(favDockFocusAction);
//    windowMenu->addAction(filterDockFocusAction);
//    windowMenu->addAction(metadataDockFocusAction);
//    windowMenu->addAction(thumbDockFocusAction);
    windowMenu->addSeparator();
    windowMenu->addAction(folderDockLockAction);
    windowMenu->addAction(favDockLockAction);
    windowMenu->addAction(filterDockLockAction);
    windowMenu->addAction(metadataDockLockAction);
    windowMenu->addAction(thumbDockLockAction);
    windowMenu->addSeparator();
    windowMenu->addAction(allDocksLockAction);

    // Help Menu

    QMenu *helpMenu = new QMenu(this);
    QAction *helpGroupAct = new QAction("Help", this);
    helpGroupAct->setMenu(helpMenu);
    helpMenu->addAction(checkForUpdateAction);
    helpMenu->addSeparator();
    helpMenu->addAction(aboutAction);
    helpMenu->addAction(helpAction);
    helpMenu->addAction(helpShortcutsAction);
    helpMenu->addAction(helpWelcomeAction);

    helpDiagnosticsMenu = helpMenu->addMenu(tr("&Diagnostics"));
    helpDiagnosticsMenu->addAction(diagnosticsAllAction);
    helpDiagnosticsMenu->addAction(diagnosticsMainAction);
    helpDiagnosticsMenu->addAction(diagnosticsGridViewAction);
    helpDiagnosticsMenu->addAction(diagnosticsThumbViewAction);
    helpDiagnosticsMenu->addAction(diagnosticsImageViewAction);
    helpDiagnosticsMenu->addAction(diagnosticsMetadataAction);
    helpDiagnosticsMenu->addAction(diagnosticsDataModelAction);
    helpDiagnosticsMenu->addAction(diagnosticsImageCacheAction);

    // Separator Action
    QAction *separatorAction = new QAction(this);
    separatorAction->setSeparator(true);
    QAction *separatorAction1 = new QAction(this);
    separatorAction1->setSeparator(true);
    QAction *separatorAction2 = new QAction(this);
    separatorAction2->setSeparator(true);
    QAction *separatorAction3 = new QAction(this);
    separatorAction3->setSeparator(true);

    // FSTree context menu
    fsTreeActions = new QList<QAction *>;
//    QList<QAction *> *fsTreeActions = new QList<QAction *>;
    fsTreeActions->append(refreshFoldersAction);
    fsTreeActions->append(collapseFoldersAction);
    fsTreeActions->append(ejectActionFromContextMenu);
    fsTreeActions->append(separatorAction);
//    fsTreeActions->append(showImageCountAction);
    fsTreeActions->append(revealFileActionFromContext);
    fsTreeActions->append(separatorAction1);
    fsTreeActions->append(pasteAction);
    fsTreeActions->append(separatorAction2);
    fsTreeActions->append(addBookmarkActionFromContext);
    fsTreeActions->append(separatorAction3);
    fsTreeActions->append(folderDockLockAction);

    // bookmarks context menu
    QList<QAction *> *favActions = new QList<QAction *>;
    favActions->append(refreshBookmarkAction);
    favActions->append(revealFileActionFromContext);
//    favActions->append(pasteAction);
    favActions->append(separatorAction);
    favActions->append(removeBookmarkAction);
    favActions->append(separatorAction1);
    favActions->append(favDockLockAction);

    // filters context menu
    filterActions = new QList<QAction *>;
//    QList<QAction *> *filterActions = new QList<QAction *>;
    filterActions->append(clearAllFiltersAction);
//    filterActions->append(filterUpdateAction);
    filterActions->append(separatorAction);
    filterActions->append(expandAllAction);
    filterActions->append(collapseAllAction);
    filterActions->append(separatorAction1);
    filterActions->append(filterDockLockAction);

    // metadata context menu
    QList<QAction *> *metadataActions = new QList<QAction *>;
//    metadataActions->append(infoView->copyInfoAction);
    metadataActions->append(reportMetadataAction);
    metadataActions->append(prefInfoAction);
    metadataActions->append(separatorAction);
    metadataActions->append(metadataDockLockAction);
    metadataActions->append(metadataFixedSizeAction);

    // Open with Menu (used in thumbview context menu)
    QAction *openWithGroupAct = new QAction(tr("Open with..."), this);
    openWithGroupAct->setMenu(openWithMenu);

    // thumbview context menu
    QList<QAction *> *thumbViewActions = new QList<QAction *>;
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
    thumbViewActions->append(reportMetadataAction);

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
    menuBar()->addAction(goGroupAct);
    menuBar()->addAction(filterGroupAct);
    menuBar()->addAction(sortGroupAct);
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
    mainContextActions->append(viewGroupAct);
    mainContextActions->append(windowGroupAct);
    mainContextActions->append(helpGroupAct);

    // Central Widget mode context menu
    centralWidget->addActions(*mainContextActions);
    centralWidget->setContextMenuPolicy(Qt::ActionsContextMenu);

//    imageView->addActions(*mainContextActions);
//    imageView->setContextMenuPolicy(Qt::ActionsContextMenu);

//    tableView->addActions(*mainContextActions);
//    tableView->setContextMenuPolicy(Qt::ActionsContextMenu);

//    gridView->addActions(*mainContextActions);
//    gridView->setContextMenuPolicy(Qt::ActionsContextMenu);

//    compareImages->addActions(*mainContextActions);
//    compareImages->setContextMenuPolicy(Qt::ActionsContextMenu);

    // docking panels context menus
    fsTree->addActions(*fsTreeActions);
    fsTree->setContextMenuPolicy(Qt::ActionsContextMenu);

    bookmarks->addActions(*favActions);
    bookmarks->setContextMenuPolicy(Qt::ActionsContextMenu);

    filters->addActions(*filterActions);
    filters->setContextMenuPolicy(Qt::ActionsContextMenu);

    infoView->addActions(*metadataActions);
    infoView->setContextMenuPolicy(Qt::ActionsContextMenu);

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
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    if(Usb::isUsb(path)) ejectAction->setEnabled(true);
    else ejectAction->setEnabled(false);
}

void MW::enableSelectionDependentMenus()
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }

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
        clearAllFiltersAction->setEnabled(true);
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
        clearAllFiltersAction->setEnabled(false);
        filterPickAction->setEnabled(false);
        filterRating1Action->setEnabled(false);
        filterRating2Action->setEnabled(false);
        filterRating3Action->setEnabled(false);
        filterRating4Action->setEnabled(false);
        filterRating5Action->setEnabled(false);
        filterRedAction->setEnabled(false);
        filterYellowAction->setEnabled(false);
        filterGreenAction->setEnabled(false);
        filterBlueAction->setEnabled(false);
        filterPurpleAction->setEnabled(false);
        filterInvertAction->setEnabled(false);
        sortGroupAction->setEnabled(false);
        sortReverseAction->setEnabled(false);
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
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
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
    G::track(__FUNCTION__);
    #endif
    }
    welcome = new QScrollArea;
    Ui::welcomeScrollArea ui;
    ui.setupUi(welcome);
//    QString style = "font-size: " + G::fontSize + "pt; color:red;";
//    ui.frame->setStyleSheet(style)
//    ui.welcomeLabel->setStyleSheet(style);
//    qDebug() << __FUNCTION__ << ui.tipsLabel->text();

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
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    metadata = new Metadata;
    progressBar = new ProgressBar(this);

    // loadSettings not run yet
    if (isSettings) combineRawJpg = setting->value("combineRawJpg").toBool();
    else combineRawJpg = true;
    dm = new DataModel(this, metadata, progressBar, filters, combineRawJpg);
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

    connect(dm, &DataModel::updateClassification, this, &MW::updateClassification);
    connect(dm, &DataModel::msg, this, &MW::setCentralMessage);
}

void MW::createSelectionModel()
{
/*
The application only has one selection model which is shared by ThumbView, GridView and
TableView, insuring that each view is in sync, except when a view is hidden.
*/
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
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
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    imageCacheThread = new ImageCache(this, dm, metadata);
    metadataCacheThread = new MetadataCache(this, dm, metadata, imageCacheThread);

    if (isSettings) {
        metadataCacheThread->cacheAllMetadata = setting->value("cacheAllMetadata").toBool();
        metadataCacheThread->cacheAllIcons = setting->value("cacheAllIcons").toBool();
        metadataCacheThread->metadataChunkSize = setting->value("metadataChunkSize").toInt();
    }
    else {
        metadataCacheThread->cacheAllMetadata = true;
        metadataCacheThread->cacheAllIcons = false;
        metadataCacheThread->metadataChunkSize = 250;
    }

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

    // show progress bar when executing loadEntireMetadataCache
    connect(metadataCacheThread, SIGNAL(showCacheStatus(int,bool)),
            this, SLOT(updateMetadataCacheStatus(int,bool)));

//    connect(metadataCacheThread, &MetadataCache::scrollToCurrent, this, &MW::scrollToCurrentRow);

    /* Image caching is triggered from the metadataCacheThread to avoid the two threads
       running simultaneously and colliding */

    // load image cache for a new folder
    connect(metadataCacheThread, SIGNAL(loadImageCache()),
            this, SLOT(loadImageCacheForNewFolder()));

    // 2nd pass loading image cache for a new folder
    connect(metadataCacheThread, SIGNAL(loadMetadataCache2ndPass()),
            this, SLOT(loadMetadataCache2ndPass()));

    // when a new image has been selected trigger a delayed update to image cache
    connect(metadataCacheThread, SIGNAL(updateImageCachePositionAfterDelay()),
            this, SLOT(updateImageCachePositionAfterDelay()));

    /* This singleshot timer signals the image cache that the position has moved in the
    file selection. The delay is used to queue many quick changes to the image and avoid
    updating the image cache until there is a pause in image selection changes.  This allows
    the user to rapidly move from one image to the next until they get to the current image
    cache limit.
    */
    imageCacheTimer = new QTimer(this);
    imageCacheTimer->setSingleShot(true);

    // connect timer to update image cache position
    connect(imageCacheTimer, SIGNAL(timeout()), imageCacheThread,
            SLOT(updateImageCachePosition()));

    connect(imageCacheThread, SIGNAL(updateIsRunning(bool,bool)),
            this, SLOT(updateImageCachingThreadRunStatus(bool,bool)));

    // Update the cache status progress bar when changed in ImageCache
    connect(imageCacheThread, SIGNAL(showCacheStatus(QString,int,QString)),
            this, SLOT(updateImageCacheStatus(QString,int,QString)));

    // Signal from ImageCache::run() to update cache status in datamodel
    connect(imageCacheThread, SIGNAL(updateCacheOnThumbs(QString,bool)),
            this, SLOT(setCachedStatus(QString,bool)));
    }

void MW::createThumbView()
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    thumbView = new IconView(this, dm, "Thumbnails");
    thumbView->setObjectName("Thumbnails");
//    thumbView->setSpacing(0);                // thumbView not visible without this
    thumbView->setAutoScroll(false);
//    thumbView->setWrapping(false);
    thumbView->firstVisibleCell = 0;

    if (isSettings) {
        // loadSettings has not run yet (dependencies, but QSettings has been opened
        thumbView->iconWidth = setting->value("thumbWidth").toInt();
        thumbView->iconHeight = setting->value("thumbHeight").toInt();
        thumbView->labelFontSize = setting->value("labelFontSize").toInt();
        thumbView->showIconLabels = setting->value("showThumbLabels").toBool();
        thumbView->badgeSize = setting->value("classificationBadgeInThumbDiameter").toInt();
        thumbView->visibleCells = setting->value("thumbsPerPage").toInt();
    }
    else {
        thumbView->iconWidth = 100;
        thumbView->iconHeight = 100;
        thumbView->labelFontSize = 12;
        thumbView->showIconLabels = true;
        thumbView->badgeSize = classificationBadgeInThumbDiameter;
        thumbView->visibleCells = width() / thumbView->iconWidth;
    }
    // double mouse click fires displayLoupe
    connect(thumbView, SIGNAL(displayLoupe()), this, SLOT(loupeDisplay()));

    // back and forward mouse buttons toggle pick
    connect(thumbView, SIGNAL(togglePick()), this, SLOT(togglePick()));
//    connect(thumbView, &ThumbView::togglePick, this, &MW::togglePick);

    connect(thumbView, SIGNAL(updateThumbDockHeight()),
            this, SLOT(setThumbDockHeight()));

    connect(thumbView, SIGNAL(updateStatus(bool, QString)),
            this, SLOT(updateStatus(bool, QString)));

    connect(thumbView->verticalScrollBar(), SIGNAL(valueChanged(int)),
            this, SLOT(thumbHasScrolled()));

    connect(thumbView->horizontalScrollBar(), SIGNAL(valueChanged(int)),
            this, SLOT(thumbHasScrolled()));

//    connect(thumbView->verticalScrollBar(), SIGNAL(valueChanged(int)),
//            this, SLOT(loadMetadataCacheAfterDelay()));

//    connect(thumbView->horizontalScrollBar(), SIGNAL(valueChanged(int)),
//            this, SLOT(loadMetadataCacheAfterDelay()));
}

void MW::createGridView()
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    gridView = new IconView(this, dm, "Grid");
    gridView->setObjectName("Grid");
    gridView->setSpacing(0);                // gridView not visible without this
    gridView->setWrapping(true);
    gridView->setAutoScroll(false);
    gridView->firstVisibleCell = 0;

    if (isSettings) {
        gridView->iconWidth = setting->value("thumbWidthGrid").toInt();
        gridView->iconHeight = setting->value("thumbHeightGrid").toInt();
        gridView->labelFontSize = setting->value("labelFontSizeGrid").toInt();
        gridView->showIconLabels = setting->value("showThumbLabelsGrid").toBool();
        gridView->badgeSize = setting->value("classificationBadgeInThumbDiameter").toInt();
        gridView->visibleCells = setting->value("thumbsPerPage").toInt();
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
    connect(gridView, SIGNAL(displayLoupe()), this, SLOT(loupeDisplay()));

    connect(gridView->verticalScrollBar(), SIGNAL(valueChanged(int)),
            this, SLOT(gridHasScrolled()));
//    connect(gridView->verticalScrollBar(), SIGNAL(valueChanged(int)),
//            this, SLOT(loadMetadataCacheAfterDelay()));
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
    G::track(__FUNCTION__);
    #endif
    }
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
    connect(tableView->horizontalHeader(),
            SIGNAL(sortIndicatorChanged(int,Qt::SortOrder)),
            this, SLOT(sortIndicatorChanged(int,Qt::SortOrder)));

    connect(tableView, SIGNAL(displayLoupe()), this, SLOT(loupeDisplay()));

    connect(tableView->verticalScrollBar(), SIGNAL(valueChanged(int)),
            this, SLOT(tableHasScrolled()));

//    connect(tableView->verticalScrollBar(), SIGNAL(valueChanged(int)),
//            this, SLOT(loadMetadataCacheAfterDelay()));
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
    G::track(__FUNCTION__);
    #endif
    }
     /* This is the info displayed on top of the image in loupe view. It is
     dependent on template data stored in QSettings */
    infoString = new InfoString(this, dm);

    if (isSettings) {
        infoString->currentInfoTemplate = setting->value("currentInfoTemplate").toString();
        setting->beginGroup("InfoTokens");
        QStringList keys = setting->childKeys();
        for (int i = 0; i < keys.size(); ++i) {
            QString key = keys.at(i);
            infoString->infoTemplates[key] = setting->value(key).toString();
        }
        setting->endGroup();
        if (!infoString->infoTemplates.contains(" Default"))
            if (!infoString->infoTemplates.contains("Default"))
                infoString->infoTemplates["Default"] =
                    "{Model} {FocalLength}  {ShutterSpeed} at {Aperture}, ISO {ISO}\n{Title}";
    }
    else {
        infoString->currentInfoTemplate = "Default";
    }

    // prep pass values: first use of program vs settings have been saved
    if (isSettings) {
        isImageInfoVisible = setting->value("isImageInfoVisible").toBool();
        isRatingBadgeVisible = setting->value("isRatingBadgeVisible").toBool();
        classificationBadgeInImageDiameter = setting->value("classificationBadgeInImageDiameter").toInt();
    }
    else {
        // parameters already defined in loadSettings
    }

    imageView = new ImageView(this,
                              centralWidget,
                              metadata,
                              dm,
                              imageCacheThread,
                              thumbView,
                              infoString,
                              setting->value("isImageInfoVisible").toBool(),
                              setting->value("isRatingBadgeVisible").toBool(),
                              setting->value("classificationBadgeInImageDiameter").toInt());

    if (isSettings) {
        imageView->useWheelToScroll = setting->value("useWheelToScroll").toBool();
        imageView->infoOverlayFontSize = setting->value("infoOverlayFontSize").toInt();
        lastPrefPage = setting->value("lastPrefPage").toInt();
//        mouseClickScroll = setting->value("mouseClickScroll").toBool();
        qreal tempZoom = setting->value("toggleZoomValue").toReal();
        if (tempZoom > 3) tempZoom = 1;
        if (tempZoom < 0.25) tempZoom = 1;
        imageView->toggleZoom = tempZoom;
    }
    else {
        imageView->useWheelToScroll = false;
        imageView->toggleZoom = 1;
        imageView->infoOverlayFontSize = infoOverlayFontSize;   // defined in loadSettings
    }

    connect(imageView, SIGNAL(togglePick()), this, SLOT(togglePick()));

    connect(imageView, SIGNAL(updateStatus(bool, QString)),
            this, SLOT(updateStatus(bool, QString)));

    connect(thumbView, SIGNAL(thumbClick(float,float)),
            imageView, SLOT(thumbClick(float,float)));

    connect(imageView, SIGNAL(handleDrop(const QMimeData*)),
            this, SLOT(handleDrop(const QMimeData*)));

    connect(imageView, SIGNAL(killSlideshow()),
            this, SLOT(slideShow()));
}

void MW::createCompareView()
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    compareImages = new CompareImages(this, centralWidget, metadata, dm, thumbView, imageCacheThread);

    if (isSettings) {
        lastPrefPage = setting->value("lastPrefPage").toInt();
//        mouseClickScroll = setting->value("mouseClickScroll").toBool();
        qreal tempZoom = setting->value("toggleZoomValue").toReal();
        if (tempZoom > 3) tempZoom = 1;
        if (tempZoom < 0.25) tempZoom = 1;
        compareImages->toggleZoom = tempZoom;
    }
    else {
        imageView->useWheelToScroll = false;
        imageView->toggleZoom = 1;
    }

    connect(compareImages, SIGNAL(updateStatus(bool, QString)),
            this, SLOT(updateStatus(bool, QString)));

    connect(compareImages, &CompareImages::togglePick, this, &MW::togglePick);
}

void MW::createInfoView()
{
/*
InfoView shows basic metadata in a dock widget.
*/
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    infoView = new InfoView(this, dm, metadata);
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

    connect(infoView->ok, SIGNAL(itemChanged(QStandardItem*)),
            this, SLOT(metadataChanged(QStandardItem*)));

}

void MW::createDocks()
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    folderDock = new DockWidget(tr(" Folders "), this);
    folderDock->setObjectName("File System");
    folderDock->setWidget(fsTree);

    if (isSettings) {
        setting->beginGroup(("FolderDockFloat"));
        folderDock->dw.screen = setting->value("screen").toInt();
        folderDock->dw.pos = setting->value("pos").toPoint();
        folderDock->dw.size = setting->value("size").toSize();
        setting->endGroup();
    }

    favDock = new DockWidget(tr(" Bookmarks  "), this);
    favDock->setObjectName("Bookmarks");
    favDock->setWidget(bookmarks);

    if (isSettings) {
        setting->beginGroup(("FavDockFloat"));
        favDock->dw.screen = setting->value("screen").toInt();
        favDock->dw.pos = setting->value("pos").toPoint();
        favDock->dw.size = setting->value("size").toSize();
        setting->endGroup();
    }

    filterDock = new DockWidget(tr("  Filters  "), this);
    filterDock->setObjectName("Filters");
    filterDock->setWidget(filters);

    if (isSettings) {
        setting->beginGroup(("FilterDock"));
        filterDock->dw.screen = setting->value("screen").toInt();
        filterDock->dw.pos = setting->value("pos").toPoint();
        filterDock->dw.size = setting->value("size").toSize();
        setting->endGroup();
    }

    metadataDock = new DockWidget(tr("  Metadata  "), this);
    metadataDock->setObjectName("Image Info");
    metadataDock->setWidget(infoView);

    if (isSettings) {
        setting->beginGroup(("MetadataDockFloat"));
        metadataDock->dw.screen = setting->value("screen").toInt();
        metadataDock->dw.pos = setting->value("pos").toPoint();
        metadataDock->dw.size = setting->value("size").toSize();
        setting->endGroup();
    }

    thumbDock = new DockWidget(tr("Thumbnails"), this);
    thumbDock->setObjectName("thumbDock");
    thumbDock->setWidget(thumbView);
    thumbDock->setWindowTitle("Thumbs");
    thumbDock->installEventFilter(this);

    if (isSettings) {
        setting->beginGroup(("ThumbDockFloat"));
        thumbDock->dw.screen = setting->value("screen").toInt();
        thumbDock->dw.pos = setting->value("pos").toPoint();
        thumbDock->dw.size = setting->value("size").toSize();
        setting->endGroup();
    }
//    else thumbDock->dw.size = QSize(600, 600);

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

//    propertiesDock = new DockWidget(tr("  Properties  "), this);
//    propertiesDock->setObjectName("Properties");
//    propertiesDock->setWidget(infoView);
//    propertiesDock->setFloating(true);
//    propertiesDock->setVisible(false);
}

void MW::createFSTree()
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    // loadSettings has not run yet (dependencies, but QSettings has been opened
    fsTree = new FSTree(this, metadata);
    fsTree->setMaximumWidth(folderMaxWidth);
    fsTree->setShowImageCount(true);
//    fsTree->setShowImageCount(setting->value("showImageCount").toBool());

    connect(fsTree, SIGNAL(clicked(const QModelIndex&)), this, SLOT(folderSelectionChange()));

//    connect(fsTree->fsModel, SIGNAL(rowsRemoved(const QModelIndex &, int, int)),
//            this, SLOT(checkDirState(const QModelIndex &, int, int)));

//    connect(fsTree, SIGNAL(dropOp(Qt::KeyboardModifiers, bool, QString)),
//            this, SLOT(dropOp(Qt::KeyboardModifiers, bool, QString)));
}

void MW::createFilterView()
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    filters = new Filters(this);
    filters->setMaximumWidth(folderMaxWidth);

    /* Not using SIGNAL(itemChanged(QTreeWidgetItem*,int) because it triggers
       for every item in Filters */

//    connect(filters, &Filters::itemClicked, this, &MW::filterChange);
    connect(filters, &Filters::filterChange, this, &MW::filterChange);
}

void MW::createBookmarks()
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    bookmarks = new BookMarks(this, metadata, true /*setting->value("showImageCount").toBool()*/);

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
    G::track(__FUNCTION__);
    #endif
    }
/*
   All the stylesheet text is saved in "/qss/winnow.css" except parameters we want
   to change programmatically, which is formulated in css1.
*/
    // add error trapping for file io  rgh todo
    QFile fStyle(":/qss/winnow.css");
    fStyle.open(QIODevice::ReadOnly);
    cssBase += fStyle.readAll();

    if (isSettings) {
        G::fontSize = setting->value("fontSize").toString();
    }
    else {
        G::fontSize = "16";
    }
    qDebug() << __FUNCTION__ << "G::fontSize =" << G::fontSize;
    css1 = "QWidget {font-size: " + G::fontSize + "px;}";       // rgh px or pt
    css = css1 + cssBase;
    this->setStyleSheet(css);
    //    QApplication::setStyle(QStyleFactory::create("fusion"));
}

void MW::createStatusBar()
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    statusBar()->setObjectName("WinnowStatusBar");
    statusBar()->setStyleSheet("QStatusBar::item { border: none; };");

    // start pregressbar

    // label to hold QPixmap showing progress
    progressLabel = new QLabel();

    // progressBar created in MW::createDataModel, where it is first req'd

    // set up pixmap that shows progress in the cache
    if (isSettings) progressWidth = setting->value("cacheStatusWidth").toInt();
    progressPixmap = new QPixmap(1000, 25);
    progressPixmap->scaled(progressWidth, 25);
    QColor cacheBGColor = QColor(85,85,85);
//    QColor cacheBGColor = QColor(85,85,85);
    progressPixmap->fill(cacheBGColor);
    progressLabel->setPixmap(*progressPixmap);
    progressLabel->setFixedWidth(progressWidth);

    // tooltip
    QString progressToolTip = "Image cache status for current folder:\n";
    progressToolTip += "  • LightGray:  \tbackground for all images in folder\n";
    progressToolTip += "  • DarkGray:   \timages targeted to be cached\n";
    progressToolTip += "  • Green:      \timages cache progress\n";
    progressToolTip += "  • Purple:     \tmetadata cache progress\n";
    progressToolTip += "  • Torquois:   \tmetadata incorporation progress\n";
    progressToolTip += "  • LightGreen: \tcurrent image";
    progressLabel->setToolTip(progressToolTip);
    progressLabel->setToolTipDuration(100000);
    statusBar()->addPermanentWidget(progressLabel, 1);

    // end progressbar

    QLabel *spacer = new QLabel;
    spacer->setText(" ");
    statusBar()->addPermanentWidget(spacer);

    // label to show metadataThreadRunning status
    int runLabelWidth = 13;
    metadataThreadRunningLabel = new QLabel;
    QString mtrl = "Turns red when metadata caching in progress";
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
    updateImageCachingThreadRunStatus(false, true);

    // labels to show various status
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
    statusLabel = new QLabel;
    statusBar()->addWidget(statusLabel);

//    updateStatusBar();
}

void MW::updateStatusBar()
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    if (!filterStatusLabel->isHidden()) statusBar()->removeWidget(filterStatusLabel);
    if (!subfolderStatusLabel->isHidden()) statusBar()->removeWidget(subfolderStatusLabel);
    if (!rawJpgStatusLabel->isHidden()) statusBar()->removeWidget(rawJpgStatusLabel);
    if (!slideShowStatusLabel->isHidden()) statusBar()->removeWidget(slideShowStatusLabel);
    if (!statusLabel->isHidden()) statusBar()->removeWidget(statusLabel);
    if (filters->isAnyFilter()) {
        statusBar()->addWidget(filterStatusLabel);
        filterStatusLabel->show();
    }
    if (subFoldersAction->isChecked()) {
        statusBar()->addWidget(subfolderStatusLabel);
        subfolderStatusLabel->show();
    }
    if (combineRawJpgAction->isChecked()) {
        statusBar()->addWidget(rawJpgStatusLabel);
        rawJpgStatusLabel->show();
    }
    if (G::isSlideShow) {
        statusBar()->addWidget(slideShowStatusLabel);
        slideShowStatusLabel->show();
    }
    statusBar()->addWidget(statusLabel);
    statusLabel->show();
    if (updateImageCacheWhenFileSelectionChange) progressLabel->setVisible(true);
    else progressLabel->setVisible(false);
}

void MW::setCacheParameters()
{
/*
This slot signalled from the preferences dialog with changes to the cache
parameters.  Any visibility changes are executed.
*/
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }

    imageCacheThread->updateImageCacheParam(cacheSizeMB, isShowCacheStatus, cacheWtAhead,
             isCachePreview, displayHorizontalPixels, displayVerticalPixels);

    progressLabel->setFixedWidth(progressWidth);
    progressPixmap->scaled(progressWidth, 25);

    QString fPath = thumbView->currentIndex().data(G::PathRole).toString();
    if (fPath.length())
        imageCacheThread->updateImageCachePosition(/*fPath*/);

    // update visibility if preferences have been changed
    progressLabel->setVisible(isShowCacheThreadActivity);
//    progressLabel->setVisible(isShowCacheStatus);

    metadataThreadRunningLabel->setVisible(isShowCacheThreadActivity);
    imageThreadRunningLabel->setVisible(isShowCacheThreadActivity);
}

QString MW::getPosition()
{
/*
This function is used by MW::updateStatus to report the current image
relative to all the images in the folder ie 17 of 80.

It is displayed on the status bar and in the infoView.
*/
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
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
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    if (G::mode != "Loupe" && G::mode != "Compare") return "N/A";
    qreal zoom;
    if (G::mode == "Compare") zoom = compareImages->zoomValue;
    else zoom = imageView->zoom;
    if (zoom <= 0 || zoom > 4) return "";
    zoom *= G::actualDevicePixelRatio;
    return QString::number(qRound(zoom*100)) + "%"; // + "% zoom";
}

QString MW::getPicked()
{
/*
Returns a string like "16 (38MB)"
*/
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    int count = 0;
    for (int row = 0; row < dm->sf->rowCount(); row++)
        if (dm->sf->index(row, G::PickColumn).data() == "true") count++;
    QString image = count == 1 ? " image, " : " images, ";

    if (count == 0) return "Nothing";
    return QString::number(count) + " ("  + pickMemSize + ")";
}

void MW::updateStatus(bool keepBase, QString s)
{
/*
Reports status information on the status bar and in InfoView.  If keepBase = true
then ie "1 of 80   60% zoom   2.1 MB picked" is prepended to the status message.
*/
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }

    // check if null filter
    if (dm->sf->rowCount() == 0) {
        statusLabel->setText("");
        QStandardItemModel *k = infoView->ok;
        k->setData(k->index(infoView->PositionRow, 1, infoView->statusInfoIdx), "");
        k->setData(k->index(infoView->ZoomRow, 1, infoView->statusInfoIdx), "");
        k->setData(k->index(infoView->PickedRow, 1, infoView->statusInfoIdx), "");
        return;
    }

    // check for file error first
    QString fPath = thumbView->getCurrentFilePath();
    int row = dm->fPathRow[fPath];
    bool imageUnavailable = dm->index(row, G::LengthFullJPGColumn).data() == 0;
    bool thumbUnavailable = dm->index(row, G::LengthThumbJPGColumn).data() == 0;
    if (imageUnavailable || thumbUnavailable) {
        // rgh may want to set the error here as well as report
        QString err = dm->index(row, G::ErrColumn).data().toString();
        statusLabel->setText(err);
        return;
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
QString magnify = "🔎";
QString fileSym = "📁";
QString fileSym = "📷";
*/

    // image of total: fileCount
    if (keepBase && isCurrentFolderOkay) {
        if (G::mode == "Loupe" || G::mode == "Compare")
            base += "Zoom: " + getZoom();
        base += spacer + "Pos: " + getPosition();
        QString s = QString::number(thumbView->getSelectedCount());
        base += spacer +"Selected: " + s;
        base += spacer + "Picked: " + getPicked();
        base += spacer;
    }

    status = " " + base + s;
    statusLabel->setText(status);
    updateStatusBar();

//    qDebug() << "Status:" << status;

    // update InfoView - flag updates so itemChanged be ignored in MW::metadataChanged
    infoView->isNewImageDataChange = true;

    QStandardItemModel *k = infoView->ok;
    if (keepBase) {
        k->setData(k->index(infoView->PositionRow, 1, infoView->statusInfoIdx), getPosition());
        k->setData(k->index(infoView->ZoomRow, 1, infoView->statusInfoIdx), getZoom());
        k->setData(k->index(infoView->PickedRow, 1, infoView->statusInfoIdx), getPicked());
    }
    else {
        k->setData(k->index(infoView->PositionRow, 1, infoView->statusInfoIdx), "");
        k->setData(k->index(infoView->ZoomRow, 1, infoView->statusInfoIdx), "");
        k->setData(k->index(infoView->PickedRow, 1, infoView->statusInfoIdx), "");
    }
    infoView->isNewImageDataChange = false;
}

void MW::clearStatus()
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    statusLabel->setText("");
}

//void MW::updateImageCachePosition()
//{
///* This slot is connected to a singleshot timer to ping the image cache in case the position
//   has moved.  This is a work-around.  Calling imageCacheThread->updateImageCachePosition
//   from fileSelectionChange resulted in a significant delay, so the singleshot short delay
//   avoids this.
//   */
//    {
//    #ifdef ISDEBUG
//    G::track(__FUNCTION__);
//    #endif
//    }
//    imageCacheThread->updateImageCachePosition(/*dm->currentFilePath*/);
//}

void MW::updateMetadataThreadRunStatus(bool isRunning,bool showCacheLabel, QString calledBy)
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__, calledBy);
    #endif
    }
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
    if (isShowCacheThreadActivity && !G::isSlideShow) progressLabel->setVisible(showCacheLabel);
    calledBy = "";  // suppress compiler warning
}

void MW::updateImageCachingThreadRunStatus(bool isRunning, bool showCacheLabel)
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    if (isRunning) {
        imageThreadRunningLabel->setStyleSheet("QLabel {color:Red;}");
        #ifdef Q_OS_WIN
        imageThreadRunningLabel->setStyleSheet("QLabel {color:Red;font-size: 24px;}");
        #endif
    }
    else {
        imageThreadRunningLabel->setStyleSheet("QLabel {color:Green;}");
        #ifdef Q_OS_WIN
        imageThreadRunningLabel->setStyleSheet("QLabel {color:Green;font-size: 24px;}");
        #endif
    }
    imageThreadRunningLabel->setText("◉");
//    G::track(__FUNCTION__, "tiger");
    if (isShowCacheThreadActivity) progressLabel->setVisible(showCacheLabel);
}

void MW::setThreadRunStatusInactive()
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
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
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    if (!dm->sf->rowCount()) return;
    QString currentFilePath = currentDmIdx.data(G::PathRole).toString();
    imageCacheThread->rebuildImageCacheParameters(currentFilePath);
//    imageCacheThread->resortImageCache();
    imageCacheThread->updateImageCachePosition();
}

void MW::sortIndicatorChanged(int column, Qt::SortOrder sortOrder)
{
/*
This slot function is triggered by the tableView->horizontalHeader sortIndicatorChanged signal
being emitted, which tells us that the data model sort/filter has been re-sorted. As a
consequence we need to update the menu checked status for the correct column and also resync
the image cache. However, changing the menu checked state for any of the menu sort actions
triggers a sort, which needs to be suppressed while syncing the menu actions with tableView.
*/
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    if (!G::allMetadataLoaded) loadEntireMetadataCache();

    sortMenuUpdateToMatchTable = true; // suppress sorting to update menu
    switch (column) {
    case 1: sortFileNameAction->setChecked(true); break;
    case 2: sortPickAction->setChecked(true); break;
    case 3: sortLabelAction->setChecked(true); break;
    case 4: sortRatingAction->setChecked(true); break;
    case 5: sortFileTypeAction->setChecked(true); break;
    case 6: sortFileSizeAction->setChecked(true); break;
    case 7: sortCreateAction->setChecked(true); break;
    case 8: sortModifyAction->setChecked(true); break;
    case 9: sortCreatorAction->setChecked(true); break;
    case 10: sortMegaPixelsAction->setChecked(true); break;
    case 11: sortDimensionsAction->setChecked(true); break;
    case 12: sortApertureAction->setChecked(true); break;
    case 13: sortShutterSpeedAction->setChecked(true); break;
    case 14: sortISOAction->setChecked(true); break;
    case 15: sortModelAction->setChecked(true); break;
    case 16: sortLensAction->setChecked(true); break;
    case 17: sortFocalLengthAction->setChecked(true); break;
    case 18: sortTitleAction->setChecked(true); break;
    }
    if(sortOrder == Qt::DescendingOrder) sortReverseAction->setChecked(true);
    else sortReverseAction->setChecked(false);
    sortMenuUpdateToMatchTable = false;
    resortImageCache();
}

/*
FILTERS & SORTING

Filters have two types:

    1. Metadata known by the operating system (name, suffix, size, modified date)
    2. Metadata read from the file by Winnow (ie cameera model, aperture, ...)

When a folder is first selected only type 1 information is known for all the files in the
folder unless the folder is small enough so all the files were read in one pass. Filtering and
sorting can only occur when the filter item is known for all the files. This is tracked by
G::allMetadataLoaded.

Type 2 Filtration steps:

    * Make sure all metadata has been loaded for all files
    * Build the filters
    * Filter based on criteria selected
*/

void MW::buildFilters()
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    // check if filters have already been build
    if (filters->days->childCount()) return;

    G::buildingFilters = true;      // req'd for user messaging in load all metadata
    G::popUp->setPopUpSize(800, 100);
    QString msg = "Building filters.  This could take a while to complete.\n";
    G::popUp->showPopup(msg, 0);
    updateStatus(false, "Building filters: loading metadata for all images ...");
    if (!G::allMetadataLoaded) {
        loadEntireMetadataCache();
    }
//    qApp->processEvents();
    dm->buildFilters();
//    progressBar->recoverProgressState();
//    imageCacheThread->filterImageCache(dm->currentFilePath);
    updateStatus(true);
    G::buildingFilters = false;
    G::popUp->showPopup("Filters completed");
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
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    // ignore if new folder is being loaded
    if (!G::isNewFolderLoaded) return;

    // Need all metadata loaded before filtering
    if (!G::allMetadataLoaded) dm->addAllMetadata(true);

//    qDebug() << __FUNCTION__ << "dm->sf->filterChange()";
    // refresh the proxy sort/filter
    dm->sf->filterChange();

    // update filter panel image count by filter item
    dm->filteredItemCount();
    dm->unfilteredItemCount();

    // update the status panel filtration status
    updateStatusBar();

    // if filter has eliminated all rows so nothing to show
    if (!dm->sf->rowCount()) {
        nullFiltration();
        return;
    }

    // get the current selected item
    /*
    qDebug() << __FUNCTION__
             << "thumbView->currentIndex().row() =" << thumbView->currentIndex().row()
             << "currentDmIdx).row() =" << currentDmIdx.row()
             << "dm->sf->mapFromSource(currentDmIdx).row() =" << dm->sf->mapFromSource(currentDmIdx).row();
            */
    currentRow = dm->sf->mapFromSource(currentDmIdx).row();
    thumbView->iconViewDelegate->currentRow = currentRow;
    gridView->iconViewDelegate->currentRow = currentRow;
    QModelIndex idx = dm->sf->index(currentRow, 0);
    selectionModel->setCurrentIndex(idx, QItemSelectionModel::Current);
    // the file path is used as an index in ImageView
    QString fPath = dm->sf->index(currentRow, 0).data(G::PathRole).toString();
    // also update datamodel, used in MdCache
    dm->currentFilePath = fPath;

    centralLayout->setCurrentIndex(prevCentralView);
    updateStatus(true);

    // sync image cache with datamodel filtered proxy
    imageCacheThread->rebuildImageCacheParameters(fPath);

    // rgh temp fix 300ms delay before scroll to currentRow
    G::wait(300);
    thumbView->selectThumb(currentRow);
    /* if the previous selected image is also part of the filtered datamodel then the
    selected index does not change and fileSelectionChange will not be signalled.
    Therefore we call it here to force the update to caching and icons */
    fileSelectionChange(idx, idx);

    source = "";    // suppress compiler warning
}

void MW::quickFilter()
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    // checked
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

void MW::invertFilters()
{
/*

*/
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    if (!G::allMetadataLoaded) loadEntireMetadataCache();

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
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
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
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }

    if (!G::allMetadataLoaded) loadEntireMetadataCache();
    uncheckAllFilters();

    filterChange("MW::clearAllFilters");
}

void MW::filterLastDay()
{
/*
.
*/
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    if (dm->rowCount() == 0) {
        G::popUp->showPopup("No images available to filter", 2000);
        filterLastDayAction->setChecked(false);
        return;
    }

    // if the additional filters have not been built then do an update
    if (!filters->days->childCount()) buildFilters();

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
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
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
            dm->setData(dm->index(row, G::RefineColumn), true);
            dm->setData(dm->index(row, G::PickColumn), "false");
            // save pick history
            QString fPath = dm->sf->index(row, G::PathColumn).data(G::PathRole).toString();
            pushPick(fPath, "false");
        }
        else dm->setData(dm->index(row, G::RefineColumn), false);
    }
    pushPick("End multiple select");

    // reset filters
    filters->uncheckAllFilters();
    filters->refineTrue->setCheckState(0, Qt::Checked);

    filterChange("MW::refine");
}

void MW::sortChange()
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    qDebug() << __FUNCTION__ << "G::isNewFolderLoaded =" << G::isNewFolderLoaded;
    if(sortMenuUpdateToMatchTable/* || !G::isNewFolderLoaded*/) return;

    if (sortFileNameAction->isChecked()) sortColumn = G::NameColumn;
    if (sortFileTypeAction->isChecked()) sortColumn = G::TypeColumn;
    if (sortFileSizeAction->isChecked()) sortColumn = G::SizeColumn;
    if (sortCreateAction->isChecked()) sortColumn = G::CreatedColumn;
    if (sortModifyAction->isChecked()) sortColumn = G::ModifiedColumn;
    if (sortPickAction->isChecked()) sortColumn = G::PickColumn;
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

    // Need all metadata loaded before sorting non-core metadata
    if (!G::allMetadataLoaded && sortColumn > G::ModifiedColumn) loadEntireMetadataCache();

    thumbView->sortThumbs(sortColumn, sortReverseAction->isChecked());

    // get the current selected item
    currentRow = dm->sf->mapFromSource(currentDmIdx).row();
    thumbView->iconViewDelegate->currentRow = currentRow;
    gridView->iconViewDelegate->currentRow = currentRow;
    QModelIndex idx = dm->sf->index(currentRow, 0);
    selectionModel->setCurrentIndex(idx, QItemSelectionModel::Current);
    // the file path is used as an index in ImageView
    QString fPath = dm->sf->index(currentRow, 0).data(G::PathRole).toString();
    // also update datamodel, used in MdCache
    dm->currentFilePath = fPath;

    centralLayout->setCurrentIndex(prevCentralView);
    updateStatus(true);

    // sync image cache with datamodel filtered proxy
//    resortImageCache();
    imageCacheThread->rebuildImageCacheParameters(fPath);

    scrollToCurrentRow();
}

void MW::scrollToCurrentRow()
{
/*
Called after a sort or when thumbs shrink/enlarge.  The current image may no longer be visible
hence need to scroll to the current row.
*/
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
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
    metadataCacheThread->scrollChange();
}

void MW::showHiddenFiles()
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
//    fsTree->setModelFlags();
}

void MW::thumbsEnlarge()
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    if (G::mode == "Grid") gridView->justify(IconView::JustifyAction::Enlarge);
    else {
        if (thumbView->isWrapping()) thumbView->justify(IconView::JustifyAction::Enlarge);
        else thumbView->thumbsEnlarge();
    }
    scrollToCurrentRow();
    // may be less icons to cache
    numberIconsVisibleChange();
}

void MW::thumbsShrink()
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    if (G::mode == "Grid") gridView->justify(IconView::JustifyAction::Shrink);
    else {
        if (thumbView->isWrapping()) thumbView->justify(IconView::JustifyAction::Shrink);
        else thumbView->thumbsShrink();
    }
    scrollToCurrentRow();
    // may be more icons to cache
    numberIconsVisibleChange();
//    loadMetadataCacheAfterDelay();
}

void MW::addRecentFolder(QString fPath)
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
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
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
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
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    QString dirPath = recentFolderActions->text();
    fsTree->select(dirPath);
    folderSelectionChange();
}

void MW::invokeIngestHistoryFolder(QAction *ingestHistoryFolderActions)
{
    {
#ifdef ISDEBUG
        G::track(__FUNCTION__);
#endif
    }
    QString dirPath = ingestHistoryFolderActions->text();
    fsTree->select(dirPath);
    folderSelectionChange();
//    revealInFileBrowser(dirPath);
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
    G::track(__FUNCTION__);
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
    G::track(__FUNCTION__);
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
    G::track(__FUNCTION__);
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
    G::track(__FUNCTION__);
    #endif
    }
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
    thumbView->iconWidth = w.thumbWidth;
    thumbView->iconHeight = w.thumbHeight;
//    thumbView->iconSpacing = w.thumbSpacing;
//    thumbView->iconPadding = w.thumbPadding;
    thumbView->labelFontSize = w.labelFontSize;
    thumbView->showIconLabels = w.showThumbLabels;
    gridView->iconWidth = w.thumbWidthGrid;
    gridView->iconHeight = w.thumbHeightGrid;
//    gridView->iconSpacing = w.thumbSpacingGrid;
//    gridView->iconPadding = w.thumbPaddingGrid;
    gridView->labelFontSize = w.labelFontSizeGrid;
    gridView->showIconLabels = w.showThumbLabelsGrid;
    thumbView->rejustify();
    gridView->rejustify();
    thumbView->setThumbParameters();
    gridView->setThumbParameters();
    updateState();
    // in case thumbdock visibility changed by status of wasThumbDockVisible in loupeDisplay etc
//    setThumbDockVisibity();
}

void MW::snapshotWorkspace(workspaceData &wsd)
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    wsd.geometry = saveGeometry();
    wsd.state = saveState();
    wsd.isFullScreen = isFullScreen();
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

    wsd.isLoupeDisplay = asLoupeAction->isChecked();
    wsd.isGridDisplay = asGridAction->isChecked();
    wsd.isTableDisplay = asTableAction->isChecked();
    wsd.isCompareDisplay = asCompareAction->isChecked();

//    wsd.thumbSpacing = thumbView->iconSpacing;
//    wsd.thumbPadding = thumbView->iconPadding;
    wsd.thumbWidth = thumbView->iconWidth;
    wsd.thumbHeight = thumbView->iconHeight;
    wsd.labelFontSize = thumbView->labelFontSize;
    wsd.showThumbLabels = thumbView->showIconLabels;
//    wsd.wrapThumbs = thumbView->wrapThumbs;

//    wsd.thumbSpacingGrid = gridView->iconSpacing;
//    wsd.thumbPaddingGrid = gridView->iconPadding;
    wsd.thumbWidthGrid = gridView->iconWidth;
    wsd.thumbHeightGrid = gridView->iconHeight;
    wsd.labelFontSizeGrid = gridView->labelFontSize;
    wsd.showThumbLabelsGrid = gridView->showIconLabels;

    wsd.isImageInfoVisible = infoVisibleAction->isChecked();
}

void MW::manageWorkspaces()
{
/*
Delete, rename and reassign workspaces.
*/
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
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
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    if (workspaces->count() < 1) return;

    // remove workspace from list of workspaces
    workspaces->removeAt(n);

    // sync menus by re-updating.  Tried to use indexes but had problems so
    // resorted to brute force solution
    syncWorkspaceMenu();
}

void MW::syncWorkspaceMenu()
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
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
    }
}

void MW::reassignWorkspace(int n)
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
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
    G::track(__FUNCTION__);
    #endif
    }
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
    thumbDockVisibleAction->setChecked(true);

    folderDockLockAction->setChecked(true);
    favDockLockAction->setChecked(true);
    filterDockLockAction->setChecked(true);
    metadataDockLockAction->setChecked(true);
    thumbDockLockAction->setChecked(false);

//    thumbView->iconPadding = 0;
    thumbView->iconWidth = 100;
    thumbView->iconHeight = 100;
    thumbView->labelFontSize = 10;
    thumbView->showIconLabels = true;

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
    metadataDock->setFloating(false);
    thumbDock->setFloating(false);

    addDockWidget(Qt::LeftDockWidgetArea, folderDock);
    addDockWidget(Qt::LeftDockWidgetArea, favDock);
    addDockWidget(Qt::LeftDockWidgetArea, filterDock);
    addDockWidget(Qt::LeftDockWidgetArea, metadataDock);
    addDockWidget(Qt::BottomDockWidgetArea, thumbDock);

    MW::setTabPosition(Qt::LeftDockWidgetArea, QTabWidget::North);
    MW::tabifyDockWidget(folderDock, favDock);
    MW::tabifyDockWidget(favDock, filterDock);
    MW::tabifyDockWidget(filterDock, metadataDock);

    folderDock->show();
    folderDock->raise();
    resizeDocks({folderDock}, {275}, Qt::Horizontal);

/*    enable the folder dock (first one in tab)
    QList<QTabBar *> tabList = findChildren<QTabBar *>();
    QTabBar* widgetTabBar = tabList.at(0);
    widgetTabBar->setCurrentIndex(0);*/

    resizeDocks({thumbDock}, {100}, Qt::Vertical);

    setThumbDockFeatures(dockWidgetArea(thumbDock));

    asLoupeAction->setChecked(true);
    infoVisibleAction->setChecked(true);
    updateState();
}

void MW::renameWorkspace(int n, QString name)
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
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
    G::track(__FUNCTION__);
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
    G::track(__FUNCTION__);
    #endif
    }
    ws = workspaces->at(n);
    qDebug() << G::t.restart() << "\t" << "\n\nName" << ws.name
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
//             << "\nwrapThumbs" << ws.wrapThumbs
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
             << "\nisCompareDisplay" << ws.isCompareDisplay;
}

void MW::loadWorkspaces()
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    if (!isSettings) return;
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
//        ws.wrapThumbs = setting->value("wrapThumbs").toBool();
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
        workspaces->append(ws);
    }
    setting->endArray();
}

void MW::reportState()
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
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
//             << "\nwrapThumbs" << w.wrapThumbs
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
             << "\nisCompareDisplay" << w.isCompareDisplay;
}

void MW::reportMetadata()
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    diagnosticsMetadata();
//    QString fPath = dm->sf->index(currentRow, 0).data(G::PathRole).toString();
//    metadata->readMetadata(true, fPath);
}

// Diagnostic Reports

//QString MW::d(QVariant x)
//// helper function to convert variable values to a string for diagnostic reporting
//{
//    return QVariant(x).toString();
//}

void MW::diagnosticsAll()
{
    QString reportString;
    QTextStream rpt;
    rpt.setString(&reportString);
    rpt << this->diagnostics();
    rpt << gridView->diagnostics();
    rpt << thumbView->diagnostics();
    rpt << imageView->diagnostics();
    rpt << metadata->diagnostics(dm->currentFilePath);
    rpt << dm->diagnostics();
    diagnosticsReport(reportString);
}

QString MW::diagnostics()
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
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
    rpt << "\n" << "displayHorizontalPixels = " << G::s(displayHorizontalPixels);
    rpt << "\n" << "displayVerticalPixels = " << G::s(displayVerticalPixels);
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
    rpt << "\n" << "backupIngest = " << G::s(backupIngest);
    rpt << "\n" << "gotoIngestFolder = " << G::s(gotoIngestFolder);
    rpt << "\n" << "lastIngestLocation = " << G::s(lastIngestLocation);
    rpt << "\n" << "slideShowDelay = " << G::s(slideShowDelay);
    rpt << "\n" << "slideShowRandom = " << G::s(isSlideShowRandom);
    rpt << "\n" << "slideShowWrap = " << G::s(isSlideShowWrap);
    rpt << "\n" << "cacheSizeMB = " << G::s(cacheSizeMB);
    rpt << "\n" << "isShowCacheStatus = " << G::s(isShowCacheStatus);
    rpt << "\n" << "cacheDelay = " << G::s(cacheDelay);
    rpt << "\n" << "isShowCacheThreadActivity = " << G::s(isShowCacheThreadActivity);
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
    rpt << "\n" << "currentViewDir = " << G::s(currentViewDir);
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
    rpt << "\n" << "isStartSilentCheckForUpdates = " << G::s(isStartSilentCheckForUpdates);
    rpt << "\n" << "isStressTest = " << G::s(isStressTest);
    rpt << "\n" << "hasGridBeenActivated = " << G::s(hasGridBeenActivated);
    rpt << "\n" << "isLeftMouseBtnPressed = " << G::s(isLeftMouseBtnPressed);
    rpt << "\n" << "isMouseDrag = " << G::s(isMouseDrag);
    rpt << "\n" << "timeToQuit = " << G::s(timeToQuit);
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
void MW::diagnosticsMetadata() {diagnosticsReport(metadata->diagnostics(dm->currentFilePath));}
void MW::diagnosticsXMP() {}
void MW::diagnosticsMetadataCache() {}
void MW::diagnosticsImageCache() {diagnosticsReport(imageCacheThread->diagnostics());}
void MW::diagnosticsDataModel() {diagnosticsReport(dm->diagnostics());}
void MW::diagnosticsFilters() {}
void MW::diagnosticsFileTree() {}
void MW::diagnosticsBookmarks() {}
void MW::diagnosticsPixmap() {}
void MW::diagnosticsThumb() {}
void MW::diagnosticsIngest() {}
void MW::diagnosticsZoom() {}

void MW::diagnosticsReport(QString reportString)
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    QDialog *dlg = new QDialog;
    Ui::metadataReporttDlg md;
    md.setupUi(dlg);
    md.textBrowser->setText(reportString);
    md.textBrowser->setWordWrapMode(QTextOption::NoWrap);
    dlg->show();
//    std::cout << reportString.toStdString() << std::flush;
}


void MW::about()
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }

    QString qtVersion = QT_VERSION_STR;
    qtVersion.prepend("Qt: ");
    aboutDlg = new AboutDlg(this, version, qtVersion);
    aboutDlg->exec();
}

void MW::externalAppManager()
{
/* This function opens a dialog that allows the user to add and delete external executables
that can be passed image files.  externalApps is a QList that holds string pairs: the program
name and the path to the external program executable.  This list is passed as a reference to
the appdlg, which modifies it and then after the dialog is closed the appActions are rebuilt.
*/
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    Appdlg *appdlg = new Appdlg(externalApps, this);

    if(appdlg->exec()) {
        /*
        Menus cannot be added/deleted at runtime in MacOS so 10 menu items are created
        in the MW constructor and then edited here based on changes made in appDlg.
        */
        for(int i = 0; i <10; ++i) {
            if(i < externalApps.length()) {
                QString shortcut = "Alt+" + xAppShortcut[i];
                appActions.at(i)->setShortcut(QKeySequence(shortcut));
                appActions.at(i)->setText(externalApps.at(i).name);
                appActions.at(i)->setVisible(true);
            }
            else {
                appActions.at(i)->setVisible(false);
                appActions.at(i)->setText("");
            }
        }
    }
}

void MW::cleanupSender()
{
/* When the process responsible for running the external program is finished a signal
is received here to delete the process.
*/
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    delete QObject::sender();
}

void MW::externalAppError(QProcess::ProcessError /*err*/)
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    QMessageBox msgBox;
    msgBox.critical(this, tr("Error"), tr("Failed to start external application."));
}

QString MW::enquote(QString &s)
{
    return QChar('\"') + s + QChar('\"');
}

void MW::runExternalApp()
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
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
    QFileInfo appInfo = appPath;
    QString appExecutable = appInfo.fileName();

//    app = externalApps[((QAction*) sender())->text()];
    QModelIndexList selectedIdxList = selectionModel->selectedRows();

//    app = "/Applications/Adobe Photoshop CC 2018/Adobe Photoshop CC 2018.app/Contents/MacOS/Adobe Photoshop CC 2018";
//    app = "/Applications/Adobe Photoshop CS6/Adobe Photoshop CS6.app/Contents/MacOS/Adobe Photoshop CS6";
//    QString x = "/Applications/Adobe Photoshop CS6/Adobe Photoshop CS6.app";
//    app = "\"/Applications/Adobe Photoshop CS6/Adobe Photoshop CS6.app\"";

    std::cout << appPath.toStdString() << std::endl << std::flush;
//    app = "/Applications/Preview.app";
//    std::cout << app.toStdString() << std::endl << std::flush;

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
    for (int tn = 0; tn < nFiles ; ++tn) {
        QString fPath = selectedIdxList[tn].data(G::PathRole).toString();
        QFileInfo fInfo = fPath;
        QString folderPath = fInfo.dir().absolutePath() + "/";

        // Update arguments
        if(appExecutable == "Photo Mechanic.exe") arguments << folderPath;
        else arguments << fPath;

        // write sidecar in case external app can read the metadata
        QString destBaseName = fInfo.baseName();
        QString suffix = fInfo.suffix().toLower();
        QString destinationPath = fPath;

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

    QProcess *process = new QProcess();
    connect(process, SIGNAL(finished(int, QProcess::ExitStatus)),
            this, SLOT(cleanupSender()));
    connect(process, SIGNAL(error(QProcess::ProcessError)),
            this, SLOT(externalAppError(QProcess::ProcessError)));

    process->setArguments(arguments);
    process->setProgram(appPath);
    process->start();

//    process->start(appPath);
//     process->start(appPath, arguments);
//     process->start(appPath, {"/Users/roryhill/Pictures/4K/2017-01-25_0030-Edit.jpg"});

//    if ( !process->waitForFinished(-1))
//        qDebug() << G::t.restart() << "\t" << process->readAllStandardError();

    //this works in terminal"
    // open "/Users/roryhill/Pictures/4K/2017-01-25_0030-Edit.jpg" -a "Adobe Photoshop CS6"
}

void MW::preferences(int page)
{
/*

*/
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    page = 0;   // suppress compiler warning
    Preferences *pref = new Preferences(this);
    preferencesDlg = new PreferencesDlg(this, isSoloPrefDlg, pref, css);
    preferencesDlg->exec();
//    propertiesDock = new DockWidget(tr("  Preferencess  "), this);
//    propertiesDock->setObjectName("Preferences");
//    propertiesDock->setWidget(pref);
//    propertiesDock->setFloating(true);
//    propertiesDock->setGeometry(2000,600,400,800);
//    propertiesDock->setVisible(true);
//    propertiesDock->raise();
//    return;

//    if(page == -1) page = lastPrefPage;
//    Prefdlg *prefdlg = new Prefdlg(this, page);
//    connect(prefdlg, SIGNAL(updatePage(int)),
//            this, SLOT(setPrefPage(int)));
//    prefdlg->exec();
}

void MW::setShowImageCount()
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
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
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    G::fontSize = QString::number(fontPixelSize);
    css = "QWidget {font-size: " + G::fontSize + "px;}" + cssBase;
    this->setStyleSheet(css);
/*
    infoView->resize(infoView->width(), infoView->height());    // does not trigger sizeHint
    infoView->layout()->update();                               // does not trigger sizeHint
    infoView->tweakHeaders();                                   // does not trigger sizeHint
    infoView->repaint();                                        // does not trigger sizeHint
    infoView->updateGeometry();
*/
    infoView->updateInfo(currentRow);                           // triggers sizehint!
    bookmarks->setStyleSheet(css);
    fsTree->setStyleSheet(css);
    filters->setStyleSheet(css);
    tableView->setStyleSheet(css);
    statusLabel->setStyleSheet(css);
}

void MW::setInfoFontSize()
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    /* imageView->infoOverlayFontSize already defined in preferences - just call
       so can redraw  */
    imageView->moveShootingInfo(imageView->shootingInfo);
}

void MW::setClassificationBadgeImageDiam(int d)
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    classificationBadgeInImageDiameter = d;
    imageView->setClassificationBadgeImageDiam(d);
}

void MW::setClassificationBadgeThumbDiam(int d)
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    classificationBadgeInThumbDiameter = d;
    thumbView->badgeSize = d;
    gridView->badgeSize = d;
    thumbView->setThumbParameters();
    gridView->setThumbParameters();
}



void MW::setPrefPage(int page)
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    lastPrefPage = page;
}

void MW::setDisplayResolution()
{
/*
This is called at startup and when the app window is dragged to another monitor.  The screen
resolution is used to calculate and report the zoom in ImageView.
*/
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    QPoint loc = centralWidget->window()->geometry().center();

//#ifdef Q_OS_WIN
    QScreen *screen = qApp->screenAt(loc);
    // make sure the centroid is on a screen
    if(screen != nullptr && screen->name() != prevScreen) {
        prevScreen = screen->name();
        displayHorizontalPixels = screen->geometry().width();
        displayVerticalPixels = screen->geometry().height();
#ifdef Q_OS_WIN
        if (G::winScreenHash.contains(screen->name()))
            G::winOutProfilePath = "C:/Windows/System32/spool/drivers/color/" +
                G::winScreenHash[screen->name()].profile;
        ICC::setOutProfile();
#endif
    }
//#endif

#ifdef Q_OS_MAC
    CGPoint point = loc.toCGPoint();

    // get displayID for monitor at point
    const int maxDisplays = 64;                     // 64 should be enough for any system
    CGDisplayCount displayCount;                    // Total number of display IDs
    CGDirectDisplayID displayIDs[maxDisplays];      // Array of display IDs
    CGGetDisplaysWithPoint (point, maxDisplays, displayIDs, &displayCount);
    auto displayID = displayIDs[0];
    if (displayCount != 1) displayID = CGMainDisplayID();

    // get list of all display modes for the monitor
    auto modes = CGDisplayCopyAllDisplayModes(displayID, nullptr);
    auto count = CFArrayGetCount(modes);
    CGDisplayModeRef mode;
//    displayHorizontalPixels, displayVerticalPixels = 0;

    // the native resolution is the largest display mode
    for (auto c = count; c--;) {
//        mode = *(static_cast<const CGDisplayModeRef>(CFArrayGetValueAtIndex(modes, c)));
        mode = (CGDisplayModeRef)CFArrayGetValueAtIndex(modes, c);
        int w = static_cast<int>(CGDisplayModeGetWidth(mode));
        int h = static_cast<int>(CGDisplayModeGetHeight(mode));
        qDebug() << __FUNCTION__ << w << h;
        if (w > displayHorizontalPixels) displayHorizontalPixels = w;
        if (h > displayVerticalPixels) displayVerticalPixels = h;
    }
#endif

    cachePreviewWidth = displayHorizontalPixels;
    cachePreviewHeight = displayVerticalPixels;
    setActualDevicePixelRatio();
}

void MW::setActualDevicePixelRatio()
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }

    QPoint loc = centralWidget->window()->geometry().center();
    QScreen *screen = qApp->screenAt(loc);

    /* If the previous session was closed with the window mostly off screen, or a previous
       screen (monitor) is no longer available, then relocate the app to the center of the
       desktop */
    if (screen == nullptr && G::isInitializing) {
        G::actualDevicePixelRatio = 1;
        QRect desktop = QGuiApplication::screens().first()->geometry();
        resize(static_cast<int>(0.75 * desktop.width()),
               static_cast<int>(0.75 * desktop.height()));
        setGeometry( QStyle::alignedRect(Qt::LeftToRight, Qt::AlignCenter,
            size(), desktop));
        qDebug() <<  "screen == nullptr && G::isInitializing"
                  << desktop;
        return;
    }

    // Prevent calculations while moving the app between and off monitors
    if (screen == nullptr) return;

    int virtualWidth = qApp->screenAt(loc)->geometry().width();

    if (virtualWidth > 0)
        G::actualDevicePixelRatio =
            static_cast<int>(static_cast<float>(displayHorizontalPixels) / virtualWidth);
    else
        G::actualDevicePixelRatio = QPaintDevice::devicePixelRatio();

    if (G::actualDevicePixelRatio == 0) G::actualDevicePixelRatio = 1;

/*  MacOS Screen information
#if defined(Q_OS_MAC)
       int screenWidth = CGDisplayPixelsWide(CGMainDisplayID());
       qDebug() << G::t.restart() << "\t" << "screenWidth" << screenWidth << QPaintDevice::devicePixelRatio();
        float bSF = QtMac::macBackingScaleFactor();
        qDebug() << G::t.restart() << "\t" << "QtMac::BackingScaleFactor()" << bSF;
#endif

        qDebug() << G::t.restart() << "\t" << "QGuiApplication::primaryScreen()->devicePixelRatio()"
                << QGuiApplication::primaryScreen()->devicePixelRatio();
        qreal dpr = QGuiApplication::primaryScreen()->devicePixelRatio();

        QRect rect = QGuiApplication::primaryScreen()->geometry();
        qreal screenMax = qMax(rect.width(), rect.height());

        G::actualDevicePixelRatio = 1;
        G::actualDevicePixelRatio = 2880 / screenMax;

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
                 */
}

void MW::escapeFullScreen()
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    fullScreenAction->setChecked(false);
    toggleFullScreen();
}

void MW::toggleFullScreen()
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
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
        metadataDockVisibleAction->setChecked(fullScreenDocks.isMetadata);
        metadataDock->setVisible(fullScreenDocks.isMetadata);
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
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    thumbView->selectAll();
}

void MW::updateZoom()
{
/*
This function provides a dialog to change scale and to set the toggleZoom
value, which is the amount of zoom to toggle with zoomToFit scale. The user can
zoom to 100% (for example) with a click of the mouse, and with another click,
return to the zoomToFit scale. Here the user can set the amount of zoom when
toggled.

The dialog is non-modal and floats at the bottom of the central widget.
Adjustments are made when the main window resizes or is moved or the mode
changes or when a different workspace is invoked.

When the zoom is changed this is signalled to ImageView and CompareImages,
which in turn make the scale changes to the image. Conversely, changes in scale
originating from toggleZoom mouse clicking in ImageView or CompareView, or
scale changes originating from the zoomInAction and zoomOutAction are signaled
and updated here. This can cause a circular message, which is prevented by
variance checking. If the zoom factor has not changed more than can be
accounted for in int/qreal conversions then the signal is not propagated.

This only applies when a mode that can be zoomed is visible, so table and grid
are not applicable.
*/
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    // only makes sense to zoom when in loupe or compare view
    if (G::mode == "Table" || G::mode == "Grid") {
        G::popUp->showPopup("The zoom dialog is only available in loupe view", 2000);
        return;
    }

    // toggle zoomDlg (if open then close)
    if (zoomDlg) {
        if (zoomDlg->isVisible()) {
            zoomDlg->close();
            return;
        }
    }

    // the dialog positions itself relative to the main window and central widget.
    QRect a = this->geometry();
    QRect c = centralWidget->geometry();
    zoomDlg = new ZoomDlg(this, imageView->zoom, a, c);
//    ZoomDlg *zoomDlg = new ZoomDlg(this, imageView->zoom, a, c);

    // update the imageView and compareView classes if there is a zoom change
    connect(zoomDlg, SIGNAL(zoom(qreal)), imageView, SLOT(zoomTo(qreal)));
    connect(zoomDlg, SIGNAL(zoom(qreal)), compareImages, SLOT(zoomTo(qreal)));

    // update the imageView and compareView classes if there is a toggleZoomValue change
    connect(zoomDlg, SIGNAL(updateToggleZoom(qreal)),
            imageView, SLOT(updateToggleZoom(qreal)));
    connect(zoomDlg, SIGNAL(updateToggleZoom(qreal)),
            compareImages, SLOT(updateToggleZoom(qreal)));

    // if zoom change in parent send it to the zoom dialog
    connect(imageView, SIGNAL(zoomChange(qreal)), zoomDlg, SLOT(zoomChange(qreal)));
    connect(compareImages, SIGNAL(zoomChange(qreal)), zoomDlg, SLOT(zoomChange(qreal)));

    // if main window resized then re-position zoom dialog
    connect(this, SIGNAL(resizeMW(QRect,QRect)), zoomDlg, SLOT(positionWindow(QRect,QRect)));

    // if view change other than loupe then close zoomDlg
    connect(this, SIGNAL(closeZoomDlg()), zoomDlg, SLOT(close()));

    // use show so dialog will be non-modal
    zoomDlg->show();
}

void MW::zoomIn()
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    if (asLoupeAction) imageView->zoomIn();
    if (asCompareAction) compareImages->zoomIn();
}

void MW::zoomOut()
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    if (asLoupeAction) imageView->zoomOut();
    if (asCompareAction) compareImages->zoomOut();
}

void MW::zoomToFit()
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    if (asLoupeAction) imageView->zoomToFit();
    if (asCompareAction) compareImages->zoomToFit();
}

void MW::zoomToggle()
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    if (asLoupeAction) imageView->zoomToggle();
    if (asCompareAction) compareImages->zoomToggle();
}

void MW::rotateLeft()
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    setRotation(270);
//    imageView->refresh();
}

void MW::rotateRight()
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    setRotation(90);
//    imageView->refresh();
}

void MW::setRotation(int degrees)
{
/*
Rotate the loupe view image.

Rotate all selected thumbnails by the specified degrees. 90 = rotate right and
270 = rotate left.

Rotate all selected cached full size imagesby the specified degrees.

When images are added to the image cache (ImageCache) they are rotated by the
metadata orientation + the edited rotation.  Newly cached images are always
rotation up-to-date.

When there is a rotation action (rotateLeft or rotateRight) the current
rotation amount (in degrees) is updated in the datamodel and Metadata. Metadata
is updated, duplicating the datamodel because the Metadata class is already
being accessed by ImageCache but the datamodel is not.

Also, the orientation metadata must be updated for any images ingested.
*/
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    // rotate current loupe view image
    imageView->rotate(degrees);

    // iterate selection
    QModelIndexList selection = selectionModel->selectedRows();
    for (int i = 0; i < selection.count(); ++i) {
        // update rotation amount in the data model
        int row = selection.at(i).row();
        QModelIndex rotIdx = dm->sf->index(row, G::RotationColumn);
        int prevRotation = rotIdx.data(Qt::EditRole).toInt();
        int newRotation = prevRotation + degrees;
        if (newRotation > 360) newRotation = newRotation - 360;
        dm->sf->setData(rotIdx, newRotation);

        // update rotation amount in the Metadata
        // metadata is easier to access than the datamodel in ImageCache so
        // duplicate saving rotation in Metadata
//        QModelIndex idx = dm->sf->index(row, G::PathColumn);
//        QString fPath = idx.data(G::PathRole).toString();
//        metadata->setRotation(fPath, newRotation);

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
        if (imageCacheThread->imCache.contains(fPath)) {
            QImage *image = &imageCacheThread->imCache[fPath];
            *image = image->transformed(trans, Qt::SmoothTransformation);
        }
    }
}

bool MW::isValidPath(QString &path)
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
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
    G::track(__FUNCTION__);
    #endif
    }
    if (QApplication::focusWidget() == bookmarks) {
        bookmarks->removeBookmark();
        return;
    }
}

void MW::refreshBookmarks()
{
/*
This is run from the bookmarks (fav) panel conext menu to update the image count
for each bookmark folder.
*/
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    bookmarks->count();
}

/*****************************************************************************************
 * READ/WRITE PREFERENCES
 * **************************************************************************************/

void MW::writeSettings()
{
/*
This function is called when exiting the application.

Using QSetting, write the persistent application settings so they can be
re-established when the application is re-opened.
*/
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    // general
    setting->setValue("lastPrefPage", lastPrefPage);
//    setting->setValue("mouseClickScroll", mouseClickScroll);
    setting->setValue("toggleZoomValue", imageView->toggleZoom);

    // datamodel
    setting->setValue("maxIconSize", G::maxIconSize);

    // appearance
    setting->setValue("fontSize", G::fontSize);
    setting->setValue("classificationBadgeInImageDiameter", classificationBadgeInImageDiameter);
    setting->setValue("classificationBadgeInThumbDiameter", thumbView->badgeSize);
    setting->setValue("infoOverlayFontSize", imageView->infoOverlayFontSize);

    // files
    setting->setValue("rememberLastDir", rememberLastDir);
    setting->setValue("checkIfUpdate", checkIfUpdate);
    setting->setValue("lastDir", currentViewDir);
    setting->setValue("includeSubfolders", subFoldersAction->isChecked());
    setting->setValue("combineRawJpg", combineRawJpg);
    setting->setValue("useWheelToScroll", imageView->useWheelToScroll);

    // ingest
    setting->setValue("autoIngestFolderPath", autoIngestFolderPath);
    setting->setValue("autoEjectUSB", autoEjectUsb);
    setting->setValue("backupIngest", backupIngest);
    setting->setValue("gotoIngestFolder", gotoIngestFolder);
    setting->setValue("ingestRootFolder", ingestRootFolder);
    setting->setValue("ingestRootFolder2", ingestRootFolder2);
    setting->setValue("pathTemplateSelected", pathTemplateSelected);
    setting->setValue("pathTemplateSelected2", pathTemplateSelected2);
    setting->setValue("manualFolderPath", manualFolderPath);
    setting->setValue("manualFolderPath2", manualFolderPath2);
    setting->setValue("filenameTemplateSelected", filenameTemplateSelected);

    // thumbs
    setting->setValue("thumbWidth", thumbView->iconWidth);
    setting->setValue("thumbHeight", thumbView->iconHeight);
    setting->setValue("labelFontSize", thumbView->labelFontSize);
    setting->setValue("showThumbLabels", thumbView->showIconLabels);

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
    setting->setValue("cacheSizePercentOfAvailable", cacheSizePercentOfAvailable);
    setting->setValue("cacheSizeMB", cacheSizeMB);
    setting->setValue("isShowCacheStatus", isShowCacheStatus);
    setting->setValue("cacheDelay", cacheDelay);
    setting->setValue("isShowCacheThreadActivity", isShowCacheThreadActivity);
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

    setting->setValue("isMenuBarVisible", menuBarVisibleAction->isChecked());
    setting->setValue("isStatusBarVisible", statusBarVisibleAction->isChecked());
    setting->setValue("isFolderDockVisible", folderDockVisibleAction->isChecked());
    setting->setValue("isFavDockVisible", favDockVisibleAction->isChecked());
    setting->setValue("isFilterDockVisible", filterDockVisibleAction->isChecked());
    setting->setValue("isMetadataDockVisible", metadataDockVisibleAction->isChecked());
    setting->setValue("isThumbDockVisible", thumbDockVisibleAction->isChecked());
    setting->setValue("isFolderDockLocked", folderDockLockAction->isChecked());
    setting->setValue("isFavDockLocked", favDockLockAction->isChecked());
    setting->setValue("isFilterDockLocked", filterDockLockAction->isChecked());
    setting->setValue("isMetadataDockLocked", metadataDockLockAction->isChecked());
    setting->setValue("isThumbDockLocked", thumbDockLockAction->isChecked());
    setting->setValue("wasThumbDockVisible", wasThumbDockVisible);

    /* Property Editor */
    setting->setValue("isSoloPrefDlg", isSoloPrefDlg);

     /* FolderDock floating info */
    setting->beginGroup(("FolderDock"));
    setting->setValue("screen", folderDock->dw.screen);
    setting->setValue("pos", folderDock->dw.pos);
    setting->setValue("size", folderDock->dw.size);
    setting->endGroup();

    /* FavDock floating info */
    setting->beginGroup(("FavDock"));
    setting->setValue("screen", favDock->dw.screen);
    setting->setValue("pos", favDock->dw.pos);
    setting->setValue("size", favDock->dw.size);
    setting->endGroup();

    /* MetadataDock floating info */
    setting->beginGroup(("MetadataDock"));
    setting->setValue("screen", metadataDock->dw.screen);
    setting->setValue("pos", metadataDock->dw.pos);
    setting->setValue("size", metadataDock->dw.size);
    setting->endGroup();

    /* FilterDock floating info */
    setting->beginGroup(("FilterDock"));
    setting->setValue("screen", filterDock->dw.screen);
    setting->setValue("pos", filterDock->dw.pos);
    setting->setValue("size", filterDock->dw.size);
    setting->endGroup();

    /* ThumbDock floating info */
    setting->beginGroup(("ThumbDockFloat"));
    setting->setValue("screen", thumbDock->dw.screen);
    setting->setValue("pos", thumbDock->dw.pos);
    setting->setValue("size", thumbDock->dw.size);
    setting->endGroup();

    /* InfoView okToShow fields */
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
    setting->setValue("currentInfoTemplate", infoString->currentInfoTemplate);
    setting->beginGroup("InfoTokens");
    setting->remove("");
    QMapIterator<QString, QString> infoIter(infoString->infoTemplates);
    while (infoIter.hasNext()) {
        infoIter.next();
        setting->setValue(infoIter.key(), infoIter.value());
    }
    setting->endGroup();

    /* External apps */
    setting->beginGroup("ExternalApps");
    setting->remove("");
    for (int i = 0; i < externalApps.length(); ++i) {
        QString sortPrefix = xAppShortcut[i];
        if(sortPrefix == "0") sortPrefix = "X";
        setting->setValue(sortPrefix + externalApps.at(i).name, externalApps.at(i).path);
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
    QString leadingZero;
    for (int i = 0; i < recentFolders->count(); i++) {
        i < 9 ? leadingZero = "0" : leadingZero = "";
        setting->setValue("recentFolder" + leadingZero + QString::number(i+1),
                          recentFolders->at(i));
    }
    setting->endGroup();

    /* save ingest history folders */
    setting->beginGroup("IngestHistoryFolders");
    setting->remove("");
    for (int i=0; i < ingestHistoryFolders->count(); i++) {
        setting->setValue("ingestHistoryFolder" + QString::number(i+1),
                          ingestHistoryFolders->at(i));
    }
    setting->endGroup();

    /* save ingest description completer list */
    setting->beginGroup("IngestDescriptionCompleter");
    for (const auto& i : ingestDescriptionCompleter) {
        setting->setValue(i, "");
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
        setting->setValue("isMenuBarVisible", ws.isMenuBarVisible);
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
        setting->setValue("showThumbLabels", ws.showThumbLabels);
//        setting->setValue("wrapThumbs", ws.wrapThumbs);
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
    }
    setting->endArray();
}

bool MW::loadSettings()
{
/* Persistant settings from QSettings fall into two categories:
1.  Action settings
2.  Preferences

Not all settings are loaded here in this function, since this function is called before the
actions and many object, such as imageView, are created.

Preferences are located in the prefdlg class and updated here.
*/
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    // default values for first time use (settings does not yet exist)
    if (!isSettings || simulateJustInstalled) {

        // general
        lastPrefPage = 0;
//        mouseClickScroll = true;
        combineRawJpg = true;
        prevMode = "Loupe";
        G::mode = "Loupe";

        // appearance
        G::fontSize = "16";
        infoOverlayFontSize = 24;
        classificationBadgeInImageDiameter = 32;
        classificationBadgeInThumbDiameter = 16;
        isRatingBadgeVisible = true;

        // datamodel
        G::maxIconSize = 256;

        // files
        rememberLastDir = false;
        checkIfUpdate = true;
        lastDir = "";

        // ingest
        autoIngestFolderPath = false;
        autoEjectUsb = false;

        // preferences
        isSoloPrefDlg = true;

        // slideshow
        slideShowDelay = 5;
        isSlideShowRandom = false;
        isSlideShowWrap = true;

        // cache        
        cacheSizeMethod = "Moderate";
        cacheSizePercentOfAvailable = 50;
        cacheSizeMB = static_cast<int>(G::availableMemoryMB * 0.5);
        isShowCacheStatus = true;
        isShowCacheThreadActivity = true;
        progressWidth = 200;
        cacheWtAhead = 7;
        isCachePreview = false;
        cachePreviewWidth = 2000;
        cachePreviewHeight = 1600;

        return false;
    }

    // general

    // appearance
    G::fontSize = setting->value("fontSize").toString();
    // load imageView->infoOverlayFontSize later as imageView has not been created yet
    classificationBadgeInImageDiameter = setting->value("classificationBadgeInImageDiameter").toInt();
    classificationBadgeInThumbDiameter = setting->value("classificationBadgeInThumbDiameter").toInt();
    isRatingBadgeVisible = setting->value("isRatingBadgeVisible").toBool();

    // datamodel
    G::maxIconSize = setting->value("maxIconSize").toInt();

    // files
    rememberLastDir = setting->value("rememberLastDir").toBool();
    checkIfUpdate = setting->value("checkIfUpdate").toBool();
    lastDir = setting->value("lastDir").toString();

    // ingest
    autoIngestFolderPath = setting->value("autoIngestFolderPath").toBool();
    autoEjectUsb = setting->value("autoEjectUSB").toBool();
    backupIngest = setting->value("backupIngest").toBool();
    gotoIngestFolder = setting->value("gotoIngestFolder").toBool();
    ingestRootFolder = setting->value("ingestRootFolder").toString();
    ingestRootFolder2 = setting->value("ingestRootFolder2").toString();
    pathTemplateSelected = setting->value("pathTemplateSelected").toInt();
    pathTemplateSelected2 = setting->value("pathTemplateSelected2").toInt();
    filenameTemplateSelected = setting->value("filenameTemplateSelected").toInt();
    manualFolderPath = setting->value("manualFolderPath").toString();
    manualFolderPath2 = setting->value("manualFolderPath2").toString();

    // preferences
    isSoloPrefDlg = setting->value("isSoloPrefDlg").toBool();

    // slideshow
    slideShowDelay = setting->value("slideShowDelay").toInt();
    isSlideShowRandom = setting->value("isSlideShowRandom").toBool();
    isSlideShowWrap = setting->value("isSlideShowWrap").toBool();

    // metadata and icon cache loaded when metadataCacheThread created in MW::createCaching

    // image cache
    cacheSizeMethod = setting->value("cacheSizeMethod").toString();
    cacheSizePercentOfAvailable = setting->value("cacheSizePercentOfAvailable").toInt();
    if (cacheSizeMethod == "Thrifty") cacheSizeMB = static_cast<int>(G::availableMemoryMB * 0.10);
    if (cacheSizeMethod == "Moderate") cacheSizeMB = static_cast<int>(G::availableMemoryMB * 0.50);
    if (cacheSizeMethod == "Greedy") cacheSizeMB = static_cast<int>(G::availableMemoryMB * 0.90);
    if (cacheSizeMethod == "Percent of available")
        cacheSizeMB = (static_cast<int>(G::availableMemoryMB) * cacheSizePercentOfAvailable) / 100;
    if (cacheSizeMethod == "MB") cacheSizeMB = setting->value("cacheSizeMB").toInt();
    isShowCacheStatus = setting->value("isShowCacheStatus").toBool();
    isShowCacheThreadActivity = setting->value("isShowCacheThreadActivity").toBool();
    progressWidth = setting->value("cacheStatusWidth").toInt();
    cacheWtAhead = setting->value("cacheWtAhead").toInt();
    isCachePreview = setting->value("isCachePreview").toBool();
    cachePreviewWidth = setting->value("cachePreviewWidth").toInt();
    cachePreviewHeight = setting->value("cachePreviewHeight").toInt();

    // full screen
    fullScreenDocks.isFolders = setting->value("isFullScreenFolders").toBool();
    fullScreenDocks.isFavs = setting->value("isFullScreenFavs").toBool();
    fullScreenDocks.isFilters = setting->value("isFullScreenFilters").toBool();
    fullScreenDocks.isMetadata = setting->value("isFullScreenMetadata").toBool();
    fullScreenDocks.isThumbs = setting->value("isFullScreenThumbs").toBool();
    fullScreenDocks.isStatusBar = setting->value("isFullScreenStatusBar").toBool();


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

    // moved read workspaces to separate function as req'd by actions while the
    // rest of QSettings are dependent on actions being defined first.

    return true;
}

void MW::loadShortcuts(bool defaultShortcuts)
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
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
        manageAppAction->setShortcut(QKeySequence("Alt+O"));
        ingestAction->setShortcut(QKeySequence("Q"));
        showImageCountAction->setShortcut(QKeySequence("\\"));
        combineRawJpgAction->setShortcut(QKeySequence("Alt+J"));
        subFoldersAction->setShortcut(QKeySequence("B"));
        revealFileAction->setShortcut(QKeySequence("Ctrl+R"));
        refreshFoldersAction->setShortcut(QKeySequence("Ctrl+F5"));
        collapseFoldersAction->setShortcut(QKeySequence("Alt+C"));
        reportMetadataAction->setShortcut(QKeySequence("Ctrl+Shift+Alt+M"));
        exitAction->setShortcut(QKeySequence("Ctrl+Q"));

        // Edit
        selectAllAction->setShortcut(QKeySequence("Ctrl+A"));
        invertSelectionAction->setShortcut(QKeySequence("Shift+Ctrl+A"));
        refineAction->setShortcut(QKeySequence("R"));
        pickAction->setShortcut(QKeySequence("`"));
        pick1Action->setShortcut(QKeySequence("P"));
        popPickHistoryAction->setShortcut(QKeySequence("Alt+Ctrl+Z"));

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

        keyScrollLeftAction->setShortcut(QKeySequence("Alt+Left"));
        keyScrollRightAction->setShortcut(QKeySequence("Alt+Right"));
        keyScrollUpAction->setShortcut(QKeySequence("Alt+Up"));
        keyScrollDownAction->setShortcut(QKeySequence("Alt+Down"));

        keyScrollPageLeftAction->setShortcut(QKeySequence("Ctrl+Alt+Left"));
        keyScrollPageRightAction->setShortcut(QKeySequence("Ctrl+Alt+Right"));
        keyScrollPageUpAction->setShortcut(QKeySequence("Ctrl+Alt+Up"));
        keyScrollPageDownAction->setShortcut(QKeySequence("Ctrl+Alt+Down"));

        nextPickAction->setShortcut(QKeySequence("Ctrl+Shift+Alt+Right"));
        prevPickAction->setShortcut(QKeySequence("Ctrl+Shift+Alt+Left"));
        randomImageAction->setShortcut(QKeySequence("Shift+Ctrl+Right"));

        // Filters
        clearAllFiltersAction->setShortcut(QKeySequence("Shift+C"));
        filterPickAction->setShortcut(QKeySequence("Shift+`"));

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
        zoomToggleAction->setShortcut(QKeySequence("Space"));

//        thumbsFitAction->setShortcut(QKeySequence("Alt+]"));
        thumbsEnlargeAction->setShortcut(QKeySequence("]"));
        thumbsShrinkAction->setShortcut(QKeySequence("["));


        // Window
        newWorkspaceAction->setShortcut(QKeySequence("W"));
        manageWorkspaceAction->setShortcut(QKeySequence("Ctrl+W"));
        defaultWorkspaceAction->setShortcut(QKeySequence("Ctrl+Shift+W"));

        folderDockVisibleAction->setShortcut(QKeySequence("F3"));
        favDockVisibleAction->setShortcut(QKeySequence("F4"));
        filterDockVisibleAction->setShortcut(QKeySequence("F5"));
        metadataDockVisibleAction->setShortcut(QKeySequence("F6"));
        thumbDockVisibleAction->setShortcut(QKeySequence("F7"));
        menuBarVisibleAction->setShortcut(QKeySequence("F9"));
        statusBarVisibleAction->setShortcut(QKeySequence("F10"));

        folderDockLockAction->setShortcut(QKeySequence("Shift+F3"));
        favDockLockAction->setShortcut(QKeySequence("Shift+F4"));
        filterDockLockAction->setShortcut(QKeySequence("Shift+F5"));
        metadataDockLockAction->setShortcut(QKeySequence("Shift+F6"));
        thumbDockLockAction->setShortcut(QKeySequence("Shift+F7"));
        allDocksLockAction->setShortcut(QKeySequence("Ctrl+L"));

        // Help
        helpAction->setShortcut(QKeySequence("?"));
//        toggleIconsListAction->setShortcut(QKeySequence("Ctrl+T"));
    }

    setting->endGroup();
}

void MW::updateState()
{
/*
Called when program starting or when a workspace is invoked.  Based on the
condition of actions sets the visibility of all window components. */
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
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
    setThumbDockVisibity();
    setFolderDockLockMode();
    setFavDockLockMode();
    setFilterDockLockMode();
    setMetadataDockLockMode();
    setThumbDockLockMode();
    setShootingInfoVisibility();
    updateStatusBar();
//    setActualDevicePixelRation();
    isUpdatingState = false;
//    reportState();
}

void MW::refreshFolders()
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    bool showImageCount = fsTree->getShowImageCount();
    fsTree->refreshModel();
    fsTree->setShowImageCount(showImageCount);
}

/*****************************************************************************************
 * HIDE/SHOW UI ELEMENTS
 * **************************************************************************************/

void MW::setThumbDockFloatFeatures(bool isFloat)
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
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
Helper slot to call setThumbDockFeatures when the dockWidgetArea is not known,
which is the case when signalling from another class like thumbView after
thumbnails have been resized.
*/
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
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

    Note that a floating thumbDock does not trigger this slot. The float
    condition is handled by setThumbDockFloatFeatures.

    Also note that the gridView is located in the central widget so this function only
    applies to thumbView (the docked version of IconView).

    */
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
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
//        thumbsWrapAction->setChecked(false);
        thumbView->setWrapping(false);

        // if thumbDock area changed then set dock height to cell size

        // get max icon height based on best aspect
        int hMax = G::iconHMax;

        // max and min cell heights (icon plus padding + name text)
        int maxHt = thumbView->iconViewDelegate->getCellSize(QSize(hMax, hMax)).height();
        int minHt = thumbView->iconViewDelegate->getCellSize(QSize(ICON_MIN, ICON_MIN)).height();
        // plus the scroll bar
        maxHt += G::scrollBarThickness;
        minHt += G::scrollBarThickness;

        if (maxHt <= minHt) maxHt = G::maxIconSize;

        // new cell height
        int cellHt = thumbView->iconViewDelegate->getCellHeightFromThumbHeight(thumbView->iconHeight);

        //  new dock height based on new cell size
        int newThumbDockHeight = cellHt + G::scrollBarThickness;
        if (newThumbDockHeight > maxHt) newThumbDockHeight = maxHt;

        thumbView->setMaximumHeight(maxHt);
        thumbView->setMinimumHeight(minHt);

        thumbView->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
        resizeDocks({thumbDock}, {newThumbDockHeight}, Qt::Vertical);
        /*
        qDebug() << "\nMW::setThumbDockFeatures dock area =" << area << "\n"
             << "***  thumbView Ht =" << thumbView->height()
             << "maxHt ="  << maxHt << "minHt =" << minHt
             << "thumbHeight =" << thumbView->thumbHeight
             << "newThumbDockHeight" << newThumbDockHeight
             << "scrollBarHeight =" << G::scrollBarThickness;
        */
    }

    /* Must be docked left or right or is floating.  Turn horizontal scrollbars off.  Turn
       wrapping on.
    */
    else {
        thumbDock->setFeatures(QDockWidget::DockWidgetClosable |
                               QDockWidget::DockWidgetMovable  |
                               QDockWidget::DockWidgetFloatable);
        thumbView->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
//        thumbsWrapAction->setChecked(true);
        thumbView->setWrapping(true);
    }
}

void MW::loupeDisplay()
{
/*
In the central widget show a loupe view of the image pointed to by the thumbView currentindex.

Note: When the thumbDock thumbView is displayed it needs to be scrolled to the currentIndex
since it has been "hidden". However, the scrollbars take a long time to paint after the view
show event, so the ThumbView::scrollToCurrent function must be delayed. This is done by the
eventFilter in MW, intercepting the scrollbar paint events. This is a bit of a cludge to get
around lack of notification when the QListView has finished painting itself.
*/
    {
    #ifdef ISDEBUG
    qDebug() << "\n\n\n";
    G::track(__FUNCTION__);
    #endif
    }
    G::mode = "Loupe";
    updateStatus(true);
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
        thumbView->selectThumb(currentRow);
    }

    if (thumbView->isVisible()) thumbView->setFocus();
    else imageView->setFocus();

    QModelIndex idx = dm->sf->index(currentRow, 0);
    thumbView->setCurrentIndex(idx);

    // update imageView, use cache if image loaded, else read it from file
    QString fPath = idx.data(G::PathRole).toString();
    qDebug() << __FUNCTION__ << "fPath =" << fPath;
    if (imageView->isVisible() && fPath.length() > 0) {
        if (imageView->loadImage(fPath)) {
            updateClassification();
        }
    }

    // req'd after compare mode to re-enable extended selection
    thumbView->setSelectionMode(QAbstractItemView::ExtendedSelection);

    // selection has been lost while tableView and possibly thumbView were hidden
    recoverSelection();

    // req'd to show thumbs first time
    thumbView->setThumbParameters();

    // sync scrolling between modes (loupe, grid and table)
    updateIconsVisible(false);
    if (prevMode == "Table") {
        if(tableView->isRowVisible(currentRow)) scrollRow = currentRow;
        else scrollRow = tableView->midVisibleRow;
    }
    if (prevMode == "Grid") {
        if(gridView->isRowVisible(currentRow)) scrollRow = currentRow;
        else scrollRow = gridView->midVisibleCell;
    }
    G::ignoreScrollSignal = false;
//    thumbView->waitUntilOkToScroll();
    thumbView->scrollToRow(scrollRow, __FUNCTION__);
//    updateIconsVisible(false);

    prevMode = "Loupe";

}

void MW::gridDisplay()
{
/*
Note: When the gridView is displayed it needs to be scrolled to the currentIndex since it has
been "hidden". However, the scrollbars take a long time to paint after the view show event, so
the ThumbView::scrollToCurrent function must be delayed. This is done by the eventFilter in MW
(installEventFilter), intercepted the scrollbar paint events. This is a bit of a cludge to get
around lack of notification when the QListView has finished painting itself.
*/
    {
    #ifdef ISDEBUG
    qDebug() << "\n\n\n";
    G::track(__FUNCTION__, "<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<");
    #endif
    }
    G::mode = "Grid";
    updateStatus(true);
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

    // sync scrolling between modes (loupe, grid and table)
    if (prevMode == "Table") {
        if(tableView->isRowVisible(currentRow)) scrollRow = currentRow;
        else scrollRow = tableView->midVisibleRow;
    }
    if (prevMode == "Loupe" /*&& thumbView->isVisible() == true*/) {
        if(thumbView->isRowVisible(currentRow)) scrollRow = currentRow;
        else scrollRow = thumbView->midVisibleCell;
    }

    // when okToScroll scroll gridView to current row
    G::ignoreScrollSignal = false;
//    gridView->waitUntilOkToScroll();
     gridView->scrollToRow(scrollRow, __FUNCTION__);
    updateIconsVisible(false);

    // if the zoom dialog was open then close it as no image visible to zoom
    emit closeZoomDlg();

    gridView->setFocus();
    prevMode = "Grid";
    gridDisplayFirstOpen = false;
}

void MW::tableDisplay()
{
    {
    #ifdef ISDEBUG
        qDebug() << "\n\n\n";
        G::track(__FUNCTION__, "<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<");
    #endif
    }    
    G::mode = "Table";
    updateStatus(true);
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
            thumbView->selectThumb(currentRow);
        }
        if(!wasThumbDockVisible && thumbDock->isVisible()) thumbDock->setVisible(false);
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
    G::wait(100);
    tableView->scrollToRow(scrollRow, __FUNCTION__);
    if (thumbView->isVisible()) thumbView->scrollToRow(scrollRow, __FUNCTION__);
    updateIconsVisible(false);
//    qDebug() << __FUNCTION__ << scrollRow << tableView->midVisibleRow;

    // if the zoom dialog was open then close it as no image visible to zoom
    emit closeZoomDlg();

    tableView->setFocus();
    prevMode = "Table";
}

void MW::compareDisplay()
{
    {
    #ifdef ISDEBUG
        G::track(__FUNCTION__, "<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<");
    #endif
    }
    updateStatus(true);
    int n = selectionModel->selectedRows().count();
    if (n < 2) {
        G::popUp->showPopup("Select more than one image to compare.");
        return;
    }
    if (n > 9) {
        QString msg = QString::number(n);
        msg += " images have been selected.  Only the first 9 will be compared.";
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
    if(!wasThumbDockVisible) toggleThumbDockVisibity();
    hasGridBeenActivated = false;
}

void MW::saveSelection()
{
/* This function saves the current selection.  This is required, even though the three
   views (thumbView, gridView and tableViews) share the same selection model, because
   when a view is hidden it loses the current index and selection, which has to be
   re-established each time it is made visible.
*/
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    selectedRows = selectionModel->selectedRows();
    currentIdx = selectionModel->currentIndex();
}

void MW::recoverSelection()
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    QItemSelection selection;
    QModelIndex idx;
    foreach (idx, selectedRows)
        selection.select(idx, idx);
    selectionModel->select(selection, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
}

void MW::setCentralView()
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    if (!isSettings) return;
    if (asLoupeAction->isChecked()) loupeDisplay();
    if (asGridAction->isChecked()) gridDisplay();
    if (asTableAction->isChecked()) tableDisplay();
    if (asCompareAction->isChecked()) compareDisplay();
    if (currentViewDir == "") {
        QString msg = "Select a folder or bookmark to get started.";
        setCentralMessage(msg);
        prevMode = "Loupe";
    }
}

void MW::selectShootingInfo()
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    infoString->editTemplates();
    // display new info
    QModelIndex idx = thumbView->currentIndex();
    QString fPath = thumbView->getCurrentFilePath();
    QString sel = infoString->getCurrentInfoTemplate();
    QString info = infoString->parseTokenString(infoString->infoTemplates[sel],
                                        fPath, idx);
    imageView->moveShootingInfo(info);
}

void MW::setRatingBadgeVisibility() {
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    isRatingBadgeVisible = ratingBadgeVisibleAction->isChecked();
    thumbView->refreshThumbs();
    gridView->refreshThumbs();
    updateClassification();
}

void MW::setShootingInfoVisibility() {
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    imageView->infoOverlay->setVisible(infoVisibleAction->isChecked());
}

void MW::setFolderDockVisibility()
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    folderDock->setVisible(folderDockVisibleAction->isChecked());
}

void MW::setFavDockVisibility()
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    favDock->setVisible(favDockVisibleAction->isChecked());
}

void MW::setFilterDockVisibility()
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    filterDock->setVisible(filterDockVisibleAction->isChecked());
}

void MW::setMetadataDockVisibility()
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    metadataDock->setVisible(metadataDockVisibleAction->isChecked());
}

void MW::setMetadataDockFixedSize()
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
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
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    thumbDock->setVisible(thumbDockVisibleAction->isChecked());
    thumbView->selectThumb(currentRow);
}

void MW::toggleFolderDockVisibility()
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    if (G::isInitializing) return;

    if (folderDock->isVisible() && folderDock->visibleRegion().isEmpty()) dockToggle = SetFocus;
    if (folderDock->isVisible() && !folderDock->visibleRegion().isEmpty()) dockToggle = SetInvisible;
    if (!folderDock->isVisible()) dockToggle = SetVisible;

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
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    G::track(__FUNCTION__);
    if (G::isInitializing) return;

    if (favDock->isVisible() && favDock->visibleRegion().isEmpty()) dockToggle = SetFocus;
    if (favDock->isVisible() && !favDock->visibleRegion().isEmpty()) dockToggle = SetInvisible;
    if (!favDock->isVisible()) dockToggle = SetVisible;

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
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    if (G::isInitializing) return;

    if (filterDock->isVisible() && filterDock->visibleRegion().isEmpty()) dockToggle = SetFocus;
    if (filterDock->isVisible() && !filterDock->visibleRegion().isEmpty()) dockToggle = SetInvisible;
    if (!filterDock->isVisible()) dockToggle = SetVisible;

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
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    if (G::isInitializing) return;

    if (metadataDock->isVisible() && metadataDock->visibleRegion().isEmpty()) dockToggle = SetFocus;
    if (metadataDock->isVisible() && !metadataDock->visibleRegion().isEmpty()) dockToggle = SetInvisible;
    if (!metadataDock->isVisible()) dockToggle = SetVisible;

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
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    if (G::isInitializing) return;
    if (thumbDock->isVisible() && thumbDock->visibleRegion().isEmpty()) dockToggle = SetFocus;
    if (thumbDock->isVisible() && !thumbDock->visibleRegion().isEmpty()) dockToggle = SetInvisible;
    if (!thumbDock->isVisible()) dockToggle = SetVisible;

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

void MW::setMenuBarVisibility() {

    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    menuBar()->setVisible(menuBarVisibleAction->isChecked());
}

void MW::setStatusBarVisibility() {

    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    statusBar()->setVisible(statusBarVisibleAction->isChecked());
//    G::isStatusBarVisible = statusBarVisibleAction->isChecked();
}

//void MW::setWindowsTitleBarVisibility() {

//    {
//    #ifdef ISDEBUG
//    G::track(__FUNCTION__);
//    #endif
//    }
//    G::track(__FUNCTION__);
//    if(windowTitleBarVisibleAction->isChecked()) {
//        hide();
//        setWindowFlags(windowFlags() & ~Qt::FramelessWindowHint);
//        show();    }
//    else {
//        hide();
//        setWindowFlags(windowFlags() | Qt::FramelessWindowHint);
//        show();
//    }
//}

void MW::setFolderDockLockMode()
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
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
    G::track(__FUNCTION__);
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
    G::track(__FUNCTION__);
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
    G::track(__FUNCTION__);
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
    G::track(__FUNCTION__);
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
    G::track(__FUNCTION__);
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

void MW::setCacheStatusVisibility()
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    if (isShowCacheThreadActivity && !G::isSlideShow)
        progressLabel->setVisible(isShowCacheStatus);
    metadataThreadRunningLabel->setVisible(isShowCacheThreadActivity);
    imageThreadRunningLabel->setVisible(isShowCacheThreadActivity);
}

// not used
void MW::setStatus(QString state)
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    statusLabel->setText("    " + state + "    ");
}

void MW::setIngested()
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
//    ignoreSelectionChange = true;
//    int prevRow = currentRow;
//    saveSelection();
//    selectionModel->clear();
    for (int row = 0; row < dm->sf->rowCount(); ++row) {
        if (dm->sf->index(row, G::PickColumn).data().toString() == "true") {
//            QModelIndex idx = dm->sf->index(row, G::IngestedColumn);
//            dm->sf->setData(idx, "true");
            dm->sf->setData(dm->sf->index(row, G::IngestedColumn), "true");
            dm->sf->setData(dm->sf->index(row, G::PickColumn), "false");
//            QModelIndex idx = dm->sf->index(row, 0);
//            selectionModel->select(idx, QItemSelectionModel::Select | QItemSelectionModel::Rows);
        }
    }
//    togglePick();
//    recoverSelection();
//    thumbView->selectThumb(prevRow);
//    ignoreSelectionChange = false;
}

void MW::togglePick()
{
/*
If the selection has any images that are not picked then pick them all.
If the entire selection was already picked then unpick them all.
If the entire selection is unpicked then pick them all.
Push the changes onto the pick history stack.
*/
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    QModelIndex idx;
    QModelIndexList idxList = selectionModel->selectedRows();
    QString pickStatus;

    bool foundFalse = false;
    // check if any images are not picked in the selection
    foreach (idx, idxList) {
        QModelIndex pickIdx = dm->sf->index(idx.row(), G::PickColumn);
        pickStatus = qvariant_cast<QString>(pickIdx.data(Qt::EditRole));
        foundFalse = (pickStatus == "false");
        if (foundFalse) break;
    }
    foundFalse ? pickStatus = "true" : pickStatus = "false";

    // add multiple selection flag to pick history
    if (idxList.length() > 1) pushPick("Begin multiple select");

    // set pick status for selection
    foreach (idx, idxList) {
        QModelIndex pickIdx = dm->sf->index(idx.row(), G::PickColumn);
        dm->sf->setData(pickIdx, pickStatus, Qt::EditRole);
        // save pick history
        QString fPath = dm->sf->index(idx.row(), G::PathColumn).data(G::PathRole).toString();
        pushPick(fPath, pickStatus);
    }
    if (idxList.length() > 1) pushPick("End multiple select");

    updateClassification();
    thumbView->refreshThumbs();
    gridView->refreshThumbs();

    pickMemSize = Utilities::formatMemory(memoryReqdForPicks());
    updateStatus(true, "");

    // update filter counts
    dm->filteredItemCount();
}

void MW::pushPick(QString fPath, QString status)
{
/*
Adds a pick action (either to pick or unpick) to the pickStack history.  This is
used to recover a prior pick history state if the picks have been lost due to an
accidental erasure.
*/
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__, fPath + ": " + status);
    #endif
    }
    pick.path = fPath;
    pick.status = status;
    pickStack->push(pick);
}

void MW::popPick()
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
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
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__, fPath + ": " + status);
    #endif
    }
    QString pickStatus;
    status == "true" ? pickStatus = "false" : pickStatus = "true";
    QModelIndexList idxList = dm->sf->match(dm->sf->index(0, 0), G::PathRole, fPath);
    if (idxList.length() == 0) return;
    QModelIndex idx = idxList[0];
    if(idx.isValid()) {
        QModelIndex pickIdx = dm->sf->index(idx.row(), G::PickColumn);
        dm->sf->setData(pickIdx, pickStatus, Qt::EditRole);
//        dm->sf->filterChange();
        thumbView->refreshThumbs();
        gridView->refreshThumbs();

        pickMemSize = Utilities::formatMemory(memoryReqdForPicks());
        updateStatus(true, "");

        // update filter counts
        dm->filteredItemCount();
    }
}

qulonglong MW::memoryReqdForPicks()
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
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

void MW::ingest()
{
/*

*/
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    static QString prevSourceFolder = "";
    static QString baseFolderDescription = "";
    if (prevSourceFolder != currentViewDir) baseFolderDescription = "";

    if (thumbView->isPick()) {
        ingestDlg = new IngestDlg(this,
                                  combineRawJpg,
                                  autoEjectUsb,
                                  backupIngest,
                                  gotoIngestFolder,
                                  metadata,
                                  dm,
                                  ingestRootFolder,
                                  ingestRootFolder2,
                                  manualFolderPath,
                                  manualFolderPath2,
                                  baseFolderDescription,
                                  pathTemplates,
                                  filenameTemplates,
                                  pathTemplateSelected,
                                  pathTemplateSelected2,
                                  filenameTemplateSelected,
                                  ingestDescriptionCompleter,
                                  autoIngestFolderPath,
                                  css);

        connect(ingestDlg, SIGNAL(updateIngestHistory(QString)),
                this, SLOT(addIngestHistoryFolder(QString)));

        connect(ingestDlg, SIGNAL(revealIngestLocation(QString)),
                this, SLOT(revealInFileBrowser(QString)));

        // make sure the font is not too big for the ingest dialof
        int n = G::fontSize.toInt();
        if (n > 17) {
            QString s = "QWidget {font-size: 17px;}" + cssBase;
            ingestDlg->setStyleSheet(s);
        }

        bool ingested = ingestDlg->exec();
        delete ingestDlg;

        if (!ingested) return;

        if (autoEjectUsb) ejectUsb(currentViewDir);;

        prevSourceFolder = currentViewDir;

        if(gotoIngestFolder) {
            fsTree->select(lastIngestLocation);
            folderSelectionChange();
            return;
        }

        // set the ingested flags and clear the pick flags
        setIngested();
    }
    else QMessageBox::information(this,
         "Oops", "There are no picks to ingest.    ", QMessageBox::Ok);
}

void MW::ejectUsb(QString path)
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    QString driveRoot;      // ie WIN "D:\" or MAC "Untitled"
#if defined(Q_OS_WIN)
    driveRoot = path.left(3);
#elif defined(Q_OS_MAC)
    // ie extract "Untitled" from "/Volumes/Untitled/DCIM"
    int start = path.indexOf("/Volumes/", 0);
    if(start != 0) return;                   // should start with "/Volumes/"
    int pos = path.indexOf("/", start + 9);
    if(pos == -1) pos = path.length();
    driveRoot = path.mid(9, pos - 9);
#endif
    if(Usb::isUsb(path)) {
        dm->load(driveRoot, false);
        refreshFolders();
        int result = Usb::eject(driveRoot);
        if(result < 2) {
            G::popUp->showPopup("Ejecting drive " + driveRoot, 2000);
            folderSelectionChange();
//            noFolderSelected();
//            currentViewDir = "";
        }
        else
            G::popUp->showPopup("Failed to eject drive " + driveRoot, 2000);
    }
    else {
        G::popUp->showPopup("Drive " + currentViewDir[0]
              + " is not removable and cannot be ejected", 2000);
    }
}

void MW::ejectUsbFromMainMenu()
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    ejectUsb(currentViewDir);
}

void MW::ejectUsbFromContextMenu()
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    ejectUsb(mouseOverFolder);
}

void MW::setCombineRawJpg()
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    // flag used in MW, dm and sf
    combineRawJpg = combineRawJpgAction->isChecked();

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
    filters->uncheckTypesFilters();
    dm->sf->filterChange();

    // rebuild filter for types
    QMap<QVariant, QString> typesMap;
    filters->types->takeChildren();
    for (int row = 0; row < dm->sf->rowCount(); ++row) {
        QString type = dm->sf->index(row, G::TypeColumn).data().toString();
        if (!typesMap.contains(type)) typesMap[type] = type;
    }
    qDebug() << __FUNCTION__ << typesMap;
    filters->addCategoryFromData(typesMap, filters->types);

    // trigger update to image list and update image cache
    filterChange("MW::setCombineRawJpg");

    // resize TableView columns to accomodate new types
    tableView->resizeColumnToContents(G::TypeColumn);

    // update status bar
    updateStatusBar();
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
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
//    int row = dm->fPathRow[fPath];
//    QModelIndex idx = dm->sf->mapFromSource(dm->index(row, 0));
    QModelIndex idx = dm->proxyIndexFromPath(fPath);
    if (idx.isValid()) {
        dm->sf->setData(idx, isCached, G::CachedRole);
        thumbView->refreshThumb(idx, G::CachedRole);
        gridView->refreshThumb(idx, G::CachedRole);
    }
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
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    // check if still in a folder with images
    if (dm->rowCount() < 1) {
        imageView->classificationLabel->setVisible(false);
    }
    int row = thumbView->currentIndex().row();
    isPick = dm->sf->index(row, G::PickColumn).data(Qt::EditRole).toString() == "true";
    rating = dm->sf->index(row, G::RatingColumn).data(Qt::EditRole).toString();
    colorClass = dm->sf->index(row, G::LabelColumn).data(Qt::EditRole).toString();
    if (rating == "0") rating = "";
    imageView->classificationLabel->setPick(isPick);
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
Resolve the source menu action that called (could be any rating) and then set
the rating for all the selected thumbs.
*/
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
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
    if(isAlreadyRating) rating = "";     // invert the label(s)

    // set the rating in the datamodel
    for (int i = 0; i < selection.count(); ++i) {
        QModelIndex ratingIdx = dm->sf->index(selection.at(i).row(), G::RatingColumn);
        dm->sf->setData(ratingIdx, rating, Qt::EditRole);

        // update metadata
        QModelIndex fPathIdx = dm->sf->index(selection.at(i).row(), G::PathColumn);
        QString fPath = fPathIdx.data(G::PathRole).toString();
//        metadata->setRating(fPath, rating);

        // check if combined raw+jpg and also set the rating for the hidden raw file
        if (combineRawJpg) {
            QModelIndex idx = dm->sf->index(selection.at(i).row(), 0);
            // is this part of a raw+jpg pair
            if(idx.data(G::DupIsJpgRole).toBool()) {
                QModelIndex rawIdx = qvariant_cast<QModelIndex>(idx.data(G::DupRawIdxRole));
                ratingIdx = dm->index(rawIdx.row(), G::RatingColumn);
                dm->setData(ratingIdx, rating, Qt::EditRole);

                // update metadata
                fPathIdx = dm->index(rawIdx.row(), G::PathColumn);
                fPath = fPathIdx.data(G::PathRole).toString();
//                metadata->setRating(fPath, rating);
            }
        }
    }

    // update metadata
//    metadata->setRating(thumbView->getCurrentFilePath(), rating);

    thumbView->refreshThumbs();
    gridView->refreshThumbs();

    // update ImageView classification badge
    updateClassification();

    // refresh the filter
    dm->sf->filterChange();

    // update filter counts
    dm->filteredItemCount();
}

void MW::setColorClass()
{
/*
Resolve the source menu action that called (could be any color class) and then
set the color class for all the selected thumbs.
*/
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
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

    // update the data model
    for (int i = 0; i < selection.count(); ++i) {
        QModelIndex labelIdx = dm->sf->index(selection.at(i).row(), G::LabelColumn);
        dm->sf->setData(labelIdx, colorClass, Qt::EditRole);

        // update metadata
        QModelIndex fPathIdx = dm->sf->index(selection.at(i).row(), G::PathColumn);
        QString fPath = fPathIdx.data(G::PathRole).toString();
//        metadata->setLabel(fPath, colorClass);

        // check if combined raw+jpg and also set the rating for the hidden raw file
        if (combineRawJpg) {
            QModelIndex idx = dm->sf->index(selection.at(i).row(), 0);
            // is this part of a raw+jpg pair
            if(idx.data(G::DupIsJpgRole).toBool()) {
                QModelIndex rawIdx = qvariant_cast<QModelIndex>(idx.data(G::DupRawIdxRole));
                labelIdx = dm->index(rawIdx.row(), G::LabelColumn);
                dm->setData(labelIdx, colorClass, Qt::EditRole);

                // update metadata
                fPathIdx = dm->index(rawIdx.row(), G::PathColumn);
                fPath = fPathIdx.data(G::PathRole).toString();
//                metadata->setLabel(fPath, colorClass);
            }
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
    dm->filteredItemCount();
    dm->unfilteredItemCount();
}

void MW::metadataChanged(QStandardItem* item)
{
/*
This slot is called when there is a data change in InfoView.

If the change was a result of a new image selection then ignore.

If the metadata in the tags section of the InfoView panel has been editied
(title, creator, copyright, email or url) then all selected image tag(s) are
updated to the new value(s). Both the data model and metadata are updated. If
xmp edits are enabled the new tag(s) are embedded in the image metadata, either
internally or as a sidecar when ingesting.
*/
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    // if new folder is invalid no relevent data has changed
    if(!isCurrentFolderOkay) return;
    if (infoView->isNewImageDataChange) return;

    QModelIndex par = item->index().parent();
    if (par != infoView->tagInfoIdx) return;

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

    for (int i = 0; i < selection.count(); ++i) {
        // update data model
        QModelIndex idx = dm->sf->index(selection.at(i).row(), col[tagName]);
        dm->sf->setData(idx, tagValue, Qt::EditRole);
        idx = dm->sf->index(selection.at(i).row(), G::PathColumn);
        QString fPath = idx.data(G::PathRole).toString();
        // update metadata
//        if (tagName == "Title") metadata->setTitle(fPath, tagValue);
//        if (tagName == "Creator") metadata->setCreator(fPath, tagValue);
//        if (tagName == "Copyright") metadata->setCopyright(fPath, tagValue);
//        if (tagName == "Email") metadata->setEmail(fPath, tagValue);
//        if (tagName == "Url") metadata->setUrl(fPath, tagValue);
    }
}

void MW::getSubfolders(QString fPath)
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
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
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
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
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    if (G::mode == "Compare") compareImages->go("Right");
    else thumbView->selectNext();
}

void MW::keyLeft()
{
/*

*/
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    if (G::mode == "Compare") compareImages->go("Left");
    else thumbView->selectPrev();
}

void MW::keyUp()
{
/*

*/
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    if (G::mode == "Loupe") thumbView->selectUp();
    if (G::mode == "Table") thumbView->selectUp();
    if (G::mode == "Grid") gridView->selectUp();
}

void MW::keyDown()
{
/*

*/
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    if (G::mode == "Loupe") thumbView->selectNext();
    if (G::mode == "Table") thumbView->selectNext();
    if (G::mode == "Grid") gridView->selectDown();
}

void MW::keyPageUp()
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    if (G::mode == "Loupe") thumbView->selectPageUp();
    if (G::mode == "Table") tableView->selectPageUp();
    if (G::mode == "Grid") gridView->selectPageUp();
}

void MW::keyPageDown()
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    if (G::mode == "Loupe") thumbView->selectPageDown();
    if (G::mode == "Table") tableView->selectPageDown();
    if (G::mode == "Grid") gridView->selectPageDown();
}

void MW::keyHome()
{
/*

*/
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    if (G::mode == "Compare") compareImages->go("Home");
    if (G::mode == "Grid") gridView->selectFirst();
    else {
        thumbView->selectFirst();
    }
}

void MW::keyEnd()
{
/*

*/
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    if (G::mode == "Compare") compareImages->go("End");
    if (G::mode == "Grid") gridView->selectLast();
    else {
        thumbView->selectLast();
    }
}

void MW::keyScrollDown()
{
    if(G::mode == "Grid") gridView->scrollDown(0);
    if(thumbView->isVisible()) thumbView->scrollDown(0);
}

void MW::keyScrollUp()
{
    if(G::mode == "Grid") gridView->scrollUp(0);
    if(thumbView->isVisible()) thumbView->scrollUp(0);
}

void MW::keyScrollPageDown()
{
    if(G::mode == "Grid") gridView->scrollPageDown(0);
    if(thumbView->isVisible()) thumbView->scrollPageDown(0);
}

void MW::keyScrollPageUp()
{
    if(G::mode == "Grid") gridView->scrollPageUp(0);
    if(thumbView->isVisible()) thumbView->scrollPageUp(0);
}

void MW::stressTest()
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    getSubfolders("/users/roryhill/pictures");
    QString fPath;
    fPath = subfolders->at(qrand() % (subfolders->count()));
}

void MW::slideShow()
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    if (G::isSlideShow) {
        // stop slideshow
        imageView->setCursor(Qt::ArrowCursor);
        slideShowStatusLabel->setText("");
        G::isSlideShow = false;
        updateStatus(true);
        updateStatusBar();
        slideShowAction->setText(tr("Slide Show"));
        G::popUp->showPopup("Stopping slideshow");
        slideShowTimer->stop();
        delete slideShowTimer;
        updateImageCacheWhenFileSelectionChange = true;
        progressBar->setVisible(true);
        imageCacheThread->updateImageCachePosition();
        // enable main window QAction shortcuts
        QList<QAction*> actions = findChildren<QAction*>();
        for (QAction *a : actions) a->setShortcutContext(Qt::WindowShortcut);
    } else {
        // start slideshow
        imageView->setCursor(Qt::BlankCursor);
        G::isSlideShow = true;
        if (isSlideShowRandom) updateImageCacheWhenFileSelectionChange = false;
        updateStatusBar();
        QString msg = "<h2>Press <font color=\"red\"><b>Esc</b></font> to exit slideshow</h2><p>";
        msg += "Press <font color=\"red\"><b>H</b></font> during slideshow for tips"
                      "<p>Starting slideshow";
        msg += "<br>Interval = " + QString::number(slideShowDelay) + " second(s)";
        if (isSlideShowRandom)  msg += "<br>Random selection";
        else msg += "<br>Sequential selection";
        if (isSlideShowWrap) msg += "<br>Wrap at end of slides";
        else msg += "<br>Stop at end of slides";

        G::popUp->showPopup(msg, 4000, true, 0.75, Qt::AlignLeft);

        // No image caching if random slide show
        if (imageCacheThread->isRunning() && isSlideShowRandom) {
            imageCacheThread->pauseImageCache();
            imageCacheThread->clearImageCache();
            progressBar->setVisible(false);
        }

        // disable main window QAction shortcuts
        QList<QAction*> actions = findChildren<QAction*>();
        for (QAction *a : actions) a->setShortcutContext(Qt::WidgetShortcut);

        if (isStressTest) getSubfolders("/users/roryhill/pictures");

        slideShowAction->setText(tr("Stop Slide Show"));
        slideShowTimer = new QTimer(this);
        connect(slideShowTimer, SIGNAL(timeout()), this, SLOT(nextSlide()));
        slideShowTimer->start(slideShowDelay * 1000);
    }
}

void MW::nextSlide()
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    static int counter = 0;
//    qDebug() << "\n" << __FUNCTION__ << "\ncounter =" << counter;

    if (isStressTest) {
//        slideShowTimer->stop();
        int waitTime = 0;
        while (!G::isNewFolderLoaded) {
            slideShowTimer->stop();
            G::wait(1000);
            waitTime += 1000;
            if (waitTime > 15000) {
                slideShow();
                return;
            }
        }
        if (counter % 2 == 0) {
            getSubfolders("D:/Pictures");                     // on PC
//            getSubfolders("/users/roryhill/pictures");        // on mac
            uint r = QRandomGenerator::global()->generate();
            int n = subfolders->count();
            int x = static_cast<int>(r) % n;
            QString fPath = subfolders->at(x);
//            fsTree->setCurrentIndex(fsTree->fsModel->index(fPath));
            QModelIndex idx = fsTree->fsModel->index(fPath);
            QModelIndex filterIdx = fsTree->fsFilter->mapFromSource(idx);
            fsTree->setCurrentIndex(filterIdx);
            fsTree->scrollTo(filterIdx, QAbstractItemView::PositionAtCenter);
            qDebug() << "STRESS TEST NEW FOLDER:"
                     << "r =" << r
                     << "n =" << n
                     << "x =" << x
                     << fPath;
            folderSelectionChange();
//            slideShowTimer->start(slideShowDelay * 1000);
        }
    }

    if (isSlideshowPaused) return;

    counter++;
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

    QString msg = "Slide # "+ QString::number(counter) + "     (press H for slideshow shortcuts)";
    updateStatus(true, msg);

}

void MW::prevRandomSlide()
{
    if (slideshowRandomHistoryStack->isEmpty()) {
        G::popUp->showPopup("End of random slide history");
        return;
    }
    isSlideshowPaused = true;
    QString prevPath = slideshowRandomHistoryStack->pop();
    thumbView->selectThumb(prevPath);
    updateStatus(false, "Slideshow random history."
                        "  Press <font color=\"white\"><b>Spacebar</b></font> to continue slideshow, "
                        "press <font color=\"white\"><b>Esc</b></font> to quit slideshow.");
    // hide popup if showing
    G::popUp->hide();
}

void MW::slideShowResetDelay()
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    slideShowTimer->setInterval(slideShowDelay * 1000);
    QString msg = "Reset slideshow interval to ";
    msg += QString::number(slideShowDelay) + " seconds";
    G::popUp->showPopup(msg);
}

void MW::slideShowResetSequence()
{
/*
Called from MW::keyReleaseEvent when R is pressed and isSlideShow == true.
The slideshow is toggled between sequential and random progress.
*/
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    QString msg = "Setting slideshow progress to ";
    if (isSlideShowRandom) {
        msg = msg + "random";
        if (imageCacheThread->isRunning()) {
            imageCacheThread->pauseImageCache();
            imageCacheThread->clearImageCache();
        }
        updateImageCacheWhenFileSelectionChange = false;
        progressLabel->setVisible(false);
    }
    else {
        msg = msg + "sequential";
        updateImageCacheWhenFileSelectionChange = true;
        progressLabel->setVisible(true);
    }
    G::popUp->showPopup(msg);
}

void MW::slideshowHelpMsg()
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
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
        "<tr><td><font color=\"red\"><b>  1 to 9  </b></font></td><td>Change the slideshow interval (seconds)</td></tr>"
        "<tr><td><font color=\"red\"><b>  W       </b></font></td><td>Toggle wrapping on and off.</td></tr>"
        "<tr><td><font color=\"red\"><b>  R       </b></font></td><td>Toggle random vs sequential slide selection.</td></tr>"
        "<tr><td><font color=\"red\"><b>Backspace </b></font></td><td>Go back to a previous random slide</td></tr>"
        "<tr><td><font color=\"red\"><b>Spacebar  </b></font></td><td>Pause/Continue slideshow</td></tr>"
        "<tr><td><font color=\"red\"><b>  H       </b></font></td><td>Show this popup message</td></tr>"
        "</table>"
        "<p>Current settings:<p>"
        "<ul style=\"line-height:50%; list-style-type:none;\""
        "<li>Interval  = "  + QString::number(slideShowDelay) + " seconds</li>"
        "<li>Selection = " + selection + "</li>"
        "<li>Wrap      = " + wrap + "</li>"
        "</ul><p><p>"
        "Press <font color=\"red\"><b>Esc</b></font> to close this message";
    G::popUp->showPopup(msg, 0, true, 1.0, Qt::AlignLeft);
}

void MW::dropOp(Qt::KeyboardModifiers keyMods, bool dirOp, QString cpMvDirPath)
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
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
    G::track(__FUNCTION__);
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
    G::track(__FUNCTION__);
    #endif
    }
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

// not req'd
void MW::showNewImageWarning(QWidget *parent)
// called from runExternalApp
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    QMessageBox msgBox;
    msgBox.warning(parent, tr("Warning"), tr("Cannot perform action with temporary image."));
}

void MW::addNewBookmark()
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    addBookmark(currentViewDir);
//    addBookmark(getSelectedPath());
}

void MW::addNewBookmarkFromContext()
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    addBookmark(mouseOverFolder);
}

void MW::addBookmark(QString path)
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }

    bookmarks->bookmarkPaths.insert(path);
    bookmarks->reloadBookmarks();
}

void MW::openFolder()
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    QString dirPath = QFileDialog::getExistingDirectory(this, tr("Open Folder"),
         "/home", QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);
//    fsTree->setCurrentIndex(fsTree->fsFilter->mapFromSource(fsTree->fsModel->index(dirPath)));
    fsTree->select(dirPath);
    folderSelectionChange();
}

void MW::refreshCurrent()
{
/*

*/
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    isRefreshingDM = true;
    refreshCurrentPath = dm->sf->index(currentRow, 0).data(G::PathRole).toString();
    folderSelectionChange();
}

void MW::copy()
{
    thumbView->copyThumbs();
//    imageView->copyImage();
}

void MW::openUsbFolder()
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
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
    subFoldersAction->setChecked(true);
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
        subFoldersAction->setChecked(false);
        updateStatusBar();
    }
}

void MW::revealFile()
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    QString fPath = dm->sf->index(currentRow, 0).data(G::PathRole).toString();
    revealInFileBrowser(fPath);
}

void MW::revealFileFromContext()
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    revealInFileBrowser(mouseOverFolder);
}

void MW::revealInFileBrowser(QString path)
{
/*
See http://stackoverflow.com/questions/3490336/how-to-reveal-in-finder-or-show-in-explorer-with-qt
for details
*/
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
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
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    fsTree->collapseAll();
}

void MW::openInFinder()
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    takeCentralWidget();
    setCentralWidget(imageView);
}

void MW::openInExplorer()
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    QString path = "C:/exampleDir/example.txt";

    QStringList args;

    args << "/select," << QDir::toNativeSeparators(path);

    QProcess *process = new QProcess(this);
    process->start("explorer.exe", args);
}

bool MW::isFolderValid(QString fPath, bool report, bool isRemembered)
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    QString msg;

    if (fPath.length() == 0) {
        if (report) {
            msg = "No folder selected.";
            statusLabel->setText("");
            setCentralMessage(msg);
        }
        return false;
    }

    QDir testDir(fPath);

    if (!testDir.exists()) {
        if (report) {
//            QMessageBox::critical(this, tr("Error"), tr("The folder does not exist or is not available"));
            if (isRemembered)
                msg = "The last folder from your previous session (" + fPath + ") does not exist or is not available";
            else
                msg = "The folder (" + fPath + ") does not exist or is not available";

            statusLabel->setText("");
            setCentralMessage(msg);
        }
        return false;
    }

    // check if unmounted USB drive
    if (!testDir.isReadable()) {
        if (report) {
            msg = "The folder (" + fPath + ") is not readable.  Perhaps it was a USB drive that is not currently mounted or that has been ejected.";
            statusLabel->setText("");
            setCentralMessage(msg);
        }
        return false;
    }

    return true;
}

void MW::createMessageView()
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    messageView = new QWidget;
    msg.setupUi(messageView);
//    Ui::message ui;
//    ui.setupUi(messageView);
}

void MW::setCentralMessage(QString message)
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__, message);
    #endif
    }
    msg.msgLabel->setText(message);
    centralLayout->setCurrentIndex(MessageTab);
}

void MW::help()
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    QWidget *helpDoc = new QWidget;
    Ui::helpForm ui;
    ui.setupUi(helpDoc);
    helpDoc->show();
}

void MW::helpShortcuts()
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
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
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    centralLayout->setCurrentIndex(StartTab);
}

void MW::test2()
{
    qDebug() << "Watcher reports finished";
    loadImageCacheForNewFolder();
}

void MW::testNewFileFormat()    // shortcut = "Shift+Ctrl+Alt+F"
{
    QString fPath = "D:/Pictures/_DNG/DngNikonD850FromLightroom.dng";
    metadata->testNewFileFormat(fPath);
}

void MW::test() // shortcut = "Shift+Ctrl+Alt+T"
{
    qDebug() << G::availableMemoryMB;
}
// End MW
