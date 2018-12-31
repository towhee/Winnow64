#include "Main/mainwindow.h"

MW::MW(QWidget *parent) : QMainWindow(parent)
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }

    {
    /* Note ISDEBUG is in globals.h
       Deactivate debug reporting by commenting ISDEBUG  */

//    qDebug() << G::t.restart() << "\t" << "isShift =" << isShift;
    isShift = false;

    // use this to show thread activity
    G::isThreadTrackingOn = false;

    // testing/debugging
    isLoadSettings = true;

    isStressTest = false;
    // Global timer
    G::isTimer = true;
    #ifdef ISPROFILE
    G::t.start();
    #endif

    }

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

    • Select a folder
      • Load datamodel with QDir info on each image file
      • Add the rest of the metadata to datamodel (incl thumbs as icons)
      • Build the image cache

    */

    // Initialize some variables etc
    initialize();

    // platform specific settings
    setupPlatform();

    // structure to hold persistant settings between sessions
    setting = new QSettings("Winnow", "winnow_100");

    createCentralWidget();      // req'd by ImageView, CompareView
    createFilterView();         // req'd by DataModel (dm)
    createDataModel();          // dependent on FilterView, creates Metadata, Thumb
    createThumbView();          // dependent on QSetting, filterView
    createGridView();           // dependent on QSetting, filterView
    createTableView();          // dependent on centralWidget
    createSelectionModel();     // dependent on ThumbView, ImageView
    createInfoView();           // dependent on Metadata
    createCaching();            // dependent on dm, Metadata, ThumbView
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

//    updateExternalApps();       // dependent on createActions
    handleStartupArgs();

    isSettings = false;
    // isLoadSettings used for debugging
    if (isLoadSettings) {
        isSettings = loadSettings();//dependent on bookmarks and actions
    }

    if (isSettings) {
        restoreGeometry(setting->value("Geometry").toByteArray());
        // restoreState sets docks which triggers setThumbDockFeatures prematurely
        restoreState(setting->value("WindowState").toByteArray());
        isFirstTimeNoSettings = false;
    }
    else {
        isFirstTimeNoSettings = true;
    }

    loadShortcuts(true);            // dependent on createActions
    setupCentralWidget();
    createAppStyle();
    setActualDevicePixelRatio();       // dependent on Settings

    // recall previous thumbDock state in case last closed in Grid mode
    thumbDockVisibleAction->setChecked(wasThumbDockVisible);

    if (isSettings)
        updateState();
    else
        defaultWorkspace();

    // send events to thumbView to monitor splitter resize of thumbDock
    qApp->installEventFilter(this);

    // process the persistant folder if available
    if (rememberLastDir && !isShift) folderSelectionChange();

    if (!isSettings) centralLayout->setCurrentIndex(StartTab);
    isSettings = true;
    isInitializing = false;
}

void MW::initialize()
{
    this->setWindowTitle("Winnow");
    QGuiApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    isInitializing = true;
    isSlideShowActive = false;
    workspaces = new QList<workspaceData>;
    recentFolders = new QStringList;
    popUp = new PopUp;
    hasGridBeenActivated = true;
    isDragDrop = false;
    setAcceptDrops(true);
    filterStatusLabel = new QLabel;
    subfolderStatusLabel = new QLabel;
    rawJpgStatusLabel = new QLabel;
    prevCentralView = 0;
    G::labelColors << "Red" << "Yellow" << "Green" << "Blue" << "Purple";
    G::ratings << "1" << "2" << "3" << "4" << "5";
    pickStack = new QStack<Pick>;
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
    #endif
    #ifdef Q_OS_MAC
        setWindowIcon(QIcon(":/images/winnow.icns"));
    #endif
}

//   EVENT HANDLERS

void MW::closeEvent(QCloseEvent *event)
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    imageCacheThread->exit();
    if (isLoadSettings) writeSettings();
    hide();
    if (!QApplication::clipboard()->image().isNull()) {
        QApplication::clipboard()->clear();
    }
    event->accept();
}

void MW::moveEvent(QMoveEvent *event)
{
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

//QSize MW::screen()
//{
//    QWindow *win = new QWindow;
//    QPoint loc = centralWidget->window()->geometry().center();
//    win->setScreen(qApp->screenAt(loc));
//    win->showFullScreen();
////    qDebug() << G::t.restart() << "\t" << "MW::Test  Width =" << win->width() << "Height =" << win->height()
////             << qApp->screenAt(loc)->name();
//    win->close();
//    return QSize(win->width(), win->height());
//}

//void MW::mousePressEvent(QMouseEvent *event)
//{
////    qDebug() << G::t.restart() << "\t" << event;
////    if (event->button() == Qt::LeftButton) isLeftMouseBtnPressed = true;
////    if (event->button() == Qt::LeftButton) qDebug() << G::t.restart() << "\t" << "mousePressEvent  isLeftMouseBtnPressed" << isLeftMouseBtnPressed;
////    QMainWindow::mousePressEvent(event);
//    lastIndexChangeEvent = "MouseClick";
//    QAbstractItemView::mousePressEvent(event);
//}

//void MW::mouseMoveEvent(QMouseEvent *event)
//{
//    qDebug() << G::t.restart() << "\t" << event;
//    if (isLeftMouseBtnPressed) {
//        isMouseDrag = true;
//    }
//    QMainWindow::mouseReleaseEvent(event);
////    event->ignore();
//}

void MW::mouseReleaseEvent(QMouseEvent *event)
{
//    QMainWindow::mouseReleaseEvent(event);
}

void MW::keyPressEvent(QKeyEvent *event)
{

//    if(event->modifiers() & Qt::ShiftModifier) isShift = true;
//    else isShift = false;
//    qDebug() << G::t.restart() << "\t" << "MW::keyPressEvent" << event << isShift;

//    dm->timeToQuit = true;

    QMainWindow::keyPressEvent(event);
    if (event->key() == Qt::Key_Return) {
        loupeDisplay();
    }
    if (event->key() == Qt::Key_Escape) {
        if(fullScreenAction->isChecked()) {
            escapeFullScreen();
        }
        else dm->timeToQuit = true;

    }
}

void MW::keyReleaseEvent(QKeyEvent *event)
{

//    isShift = false;

//    qDebug() << G::t.restart() << "\t" << "MW::keyReleaseEvent" << event << isShift;
    QMainWindow::keyReleaseEvent(event);
}

//bool MW::event(QObject *obj, QEvent *event)
bool MW::eventFilter(QObject *obj, QEvent *event)
{
     /* ThumbView is ready-to-go and can scroll to wherever we want:

    When the user changes modes in MW (ie from Grid to Loupe) a ThumbView
    instance (either thumbView or gridView) can change state from hidden to
    visible. Since hidden widgets cannot be accessed we need to wait until the
    ThumbView becomes visible and fully repainted before attempting to scroll
    to the current index. The thumbView scrollBar paint events are monitored
    and when the last paint event occurs the scrollToCurrent function is
    called. The last paint event is identified by calculating the maximum of
    the scrollbar range and comparing it to the paint event, which updates the
    range each time. With larger datasets (ie 1500+ thumbs) it can take a
    number of paint events and 100s of ms to complete. A flag is assigned
    (readyToScroll) to show when we need to monitor so not checking needlessly.
    Unfortunately there does not appear to be any signal or event when
    ThumbView is finished hence this cludge.
    */

//    if (event->type() == QEvent::KeyPress) {
//        QKeyEvent *event = static_cast<QKeyEvent *>(event);
//        qDebug() << "\nMW::eventFilter  keyPressEvent" << event;
//        if (event->key() == Qt::Key_Escape) {
//            dm->timeToQuit = true;
//        }
//    }

    if(event->type() == QEvent::Paint
            && thumbView->readyToScroll
            && (obj->objectName() == "ThumbViewVerticalScrollBar"
            || obj->objectName() == "ThumbViewHorizontalScrollBar"))
    {

//        qDebug() << G::t.restart() << "\t" << "\nThumbView event filter" << objectName() << obj << event;

        if (obj->objectName() == "ThumbViewHorizontalScrollBar") {
            /*
            qDebug() << G::t.restart() << "\t" << objectName() << "HorScrollCurrentMax / FinalMax:"
                     << horizontalScrollBar()->maximum()
                     << getHorizontalScrollBarMax();
                     */
            if (thumbView->horizontalScrollBar()->maximum() > 0.95 * thumbView->getHorizontalScrollBarMax()) {
                /*
                 qDebug() << G::t.restart() << "\t" << objectName()
                     << ": Event Filter sending row =" << currentRow
                     << "horizontalScrollBarMax Qt vs Me"
                     << thumbView->horizontalScrollBar()->maximum()
                     << thumbView->getHorizontalScrollBarMax();     */

                thumbView->scrollToCurrent(currentRow);
            }
        }
        if (obj->objectName() == "ThumbViewVerticalScrollBar") {
             /*
             qDebug() << G::t.restart() << "\t" << objectName() << "VerScrollCurrentMax / FinalMax:"
                      << thumbView->verticalScrollBar()->maximum()
                      << thumbView->getVerticalScrollBarMax();*/

             if (thumbView->verticalScrollBar()->maximum() > 0.95 * thumbView->getVerticalScrollBarMax()){
                /*
                 qDebug() << G::t.restart() << "\t" << objectName()
                          << ": Event Filter sending row =" << currentRow
                          << "verticalScrollBarMax Qt vs Me"
                          << thumbView->verticalScrollBar()->maximum()
                          << thumbView->getVerticalScrollBarMax();*/

                 thumbView->scrollToCurrent(currentRow);
             }
         }
    }

    /* A splitter resize of top/bottom thumbDock is happening:

    Events are filtered from qApp here by an installEventFilter in the MW
    contructor to monitor the splitter resize of the thumbdock when it is
    docked horizontally. In this situation, as the vertical height of the
    thumbDock changes the size of the thumbnails is modified to fit the
    thumbDock by calling thumbsFitTopOrBottom. The mouse events determine when
    a mouseDrag operation is happening in combination with thumbDock resizing.
    The thumbDock is referenced from the parent because thumbView is a friend
    class to MW.
    */

    if (event->type() == QEvent::MouseButtonPress) {
        QMouseEvent *e = (QMouseEvent *)event;
        if (e->button() == Qt::LeftButton) isLeftMouseBtnPressed = true;
    }

    if (event->type() == QEvent::MouseButtonRelease) {
        QMouseEvent *e = (QMouseEvent *)event;
        if (e->button() == Qt::LeftButton) {
            isLeftMouseBtnPressed = false;
            isMouseDrag = false;
        }
    }

    if (event->type() == QEvent::MouseMove) {
        if (isLeftMouseBtnPressed) isMouseDrag = true;
        QMouseEvent *e = (QMouseEvent *)event;
        if(obj == fsTree->viewport()) {
            QModelIndex idx = fsTree->indexAt(e->pos());
            mouseOverFolder = idx.data(QFileSystemModel::FilePathRole).toString();
//            qDebug() << obj << e->pos()
//                     << "Path =" << path;
        }
    }

    if (event->type() == QEvent::MouseButtonDblClick) {
        isMouseDrag = false;
    }

    // if a lot of stuff make thumbs fit the resized thumb dock
    if (obj == thumbDock) {
        if (event->type() == QEvent::Resize) {
            if (isMouseDrag) {
                if (!thumbDock->isFloating()) {
                    Qt::DockWidgetArea area = dockWidgetArea(thumbDock);
                    if (area == Qt::BottomDockWidgetArea
                    || area == Qt::TopDockWidgetArea
                    || !thumbView->wrapThumbs) {
                        thumbView->thumbsFitTopOrBottom();
                    }
                }
            }
        }
    }

    return QWidget::eventFilter(obj, event);
}

bool MW::event(QEvent *event)
{
//    qDebug() << G::t.restart() << "\t" << event;

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

// DRAG AND DROP

void MW::dragEnterEvent(QDragEnterEvent *event)
{
    event->acceptProposedAction();
}

void MW::dropEvent(QDropEvent *event)
{
    handleDrop(event->mimeData());
}

void MW::handleDrop(const QMimeData *mimeData)
{
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
    QProcess process;
    process.start("maintenancetool --checkupdates");

    // Wait until the update tool is finished
    process.waitForFinished();

    if(process.error() != QProcess::UnknownError)
    {
        QString msg = "Error checking for updates";
        popUp->showPopup(this, msg, 1500, .5);
        qDebug() << "Error checking for updates";
        return false;
    }

    // Read the output
    QByteArray data = process.readAllStandardOutput();

    // No output means no updates available
    // Note that the exit code will also be 1, but we don't use that
    // Also note that we should parse the output instead of just checking if it is empty if we want specific update info
    if(data.isEmpty())
    {
        QString msg = "No updates available";
        popUp->showPopup(this, msg, 1500, .5);
        qDebug() << "No updates available";
        return false;
    }

    // Call the maintenance tool binary
    // Note: we start it detached because this application need to close for the update
    QStringList args("--updater");
    bool startMaintenanceTool = QProcess::startDetached("maintenancetool", args);

    // Close the application
    if (startMaintenanceTool) qApp->closeAllWindows();
    else popUp->showPopup(this, "The maintenance tool failed to open", 2000, .75);

    // prevent compiler warning
    return false;
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



/**************************************************************************
 PROGRAM FLOW EVENT DISPATCH

 A new folder is selected which triggers folderSelectionChange:

 -   A list of all eligible image files in the folder is generated in
     DataModel (dm).

 -   A thread is spawned to cache metadata and thumbs for all the images.

 -   A second thread is spawned by the metadata thread to cache as many full
     size images as assigned memory permits.

 -   Selecting a new folder also causes the selection of the first image
     which fires a signal for fileSelectionChange.

 -   The first image is loaded.  The metadata and thumbnail generation
     threads will still be running, but things should appear to be
     speedy for the user.

 -   The metadata caching thread collects information required by the
     image cache thread.

 -   The image caching thread requires the offset and length for the
     full size embedded jpg, the image width and height in order to
     calculate memory requirements, build the image priority queues, the
     target range and limit the cache to the assigned maximum size.

 *************************************************************************/

// triggered when new folder selection
void MW::folderSelectionChange()
{
    {
    #ifdef ISDEBUG
    G::track("\n=============================================================================");
    G::track(__FUNCTION__, getSelectedPath());
    #endif
    }
    // Check if same folder
    QString dirPath;
    if (isInitializing && !rememberLastDir) {
//        centralLayout->setCurrentIndex(StartTab);
//        return;
     }

    // Stop any threads that might be running.
    imageCacheThread->stopImageCache();
    metadataCacheThread->stopMetadateCache();
    allMetadataLoaded = false;

    if (!isInitializing) {
//        dm->removeRows(0, dm->rowCount());
//        popUp->showPopup(this, "Collecting file information", 0, 0.5);
        updateStatus(false, "Collecting file information for all images in folder(s)");
        qApp->processEvents();
    }

    // used by SortFilter, set true when ImageCacheThread starts
    G::isNewFolderLoaded = false;

    // used by updateStatus
    isCurrentFolderOkay = false;
    pickMemSize = "";

    // in case no eligible images, therefore no caching
    inactiveThreadRunStatus();

    // stop slideshow if a new folder is selected
    if (isSlideShowActive && !isStressTest) slideShow();

    // if previously in compare mode switch to loupe mode
    if (asCompareAction->isChecked()) {
        asCompareAction->setChecked(false);
        asLoupeAction->setChecked(true);
        updateState();
    }

    // if at welcome or message screen and then select a folder
    if (centralLayout->currentIndex() == StartTab
            || centralLayout->currentIndex() == MessageTab) {
        if(prevMode == "Loupe") asGridAction->setChecked(true);
        if(prevMode == "Grid") asLoupeAction->setChecked(true);
        if(prevMode == "Table") asTableAction->setChecked(true);
        if(prevMode == "Compare") asLoupeAction->setChecked(true);
//        updateState();
    }

    // If just opened application
    if (isInitializing) {
        if (rememberLastDir) {
            // lastDir is from QSettings for persistent memory between sessions
            if (isFolderValid(lastDir, true, true)) {
                dirPath = lastDir;
//                fsTree->setCurrentIndex(fsTree->fsFilter->mapFromSource(fsTree->fsModel->index(dirPath)));
                fsTree->select(dirPath);
            }
            else {
               isInitializing = false;
               QModelIndex idx = fsTree->fsModel->index(dirPath);
               if (fsTree->fsModel->hasIndex(idx.row(), idx.column(), idx.parent()))
                    fsTree->setCurrentIndex(fsTree->fsFilter->mapFromSource(idx));
               return;
            }
        }
    }

    // folder selected from Folders or Bookmarks(Favs)
    if (!isInitializing || !rememberLastDir) {
        dirPath = getSelectedPath();
    }

    currentViewDir = dirPath;
    qDebug() << "currentViewDir =" << currentViewDir;

    // sync the favs / bookmarks with the folders view fsTree
    bookmarks->select(dirPath);

    // confirm folder exists and is readable, report if not and do not process
    if (!isFolderValid(dirPath, true, false)) {
        if (popUp->isVisible()) popUp->close();
        return;
    }

    // add to recent folders
    addRecentFolder(currentViewDir);

    // show image count in Folders (fsTree) if showImageCountAction isChecked
    if (showImageCountAction->isChecked()) {
        fsTree->setShowImageCount(true);
//        fsTree->fsModel->fetchMore(fsTree->rootIndex());
    }

    /* We do not want to update the imageCache while metadata is still being
    loaded. The imageCache update is triggered in metadataCache, which is also
    executed when the change file event is fired.
    */
    metadataLoaded = false;

    /*  The first time a folder is selected the datamodel is loaded with all the
    supported images.  If there are no supported images then do some cleanup. If
    this function has been executed by the load subfolders command then all the
    subfolders will be recursively loaded into the datamodel.
    */
    if (!dm->load(currentViewDir, subFoldersAction->isChecked())) {
        QDir dir(currentViewDir);
        if (dir.isRoot()) {
            updateStatus(false, "No supported images in this drive");
            setCentralMessage("The drive \"" + currentViewDir + "\" does not have any eligible images");
        }
        else {
            updateStatus(false, "No supported images in this folder");
            setCentralMessage("The folder \"" + currentViewDir + "\" does not have any eligible images");
        }
        popUp->close();
        infoView->clearInfo();
        metadata->clear();
        imageView->clear();
        cacheLabel->setVisible(false);
        isInitializing = false;
        isDragDrop = false;
        return;
    }
    centralLayout->setCurrentIndex(prevCentralView);

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

    // default sort by file name
    thumbView->sortThumbs(1, false);

    // no ratings or label color classes set yet so hide classificationLabel
//    imageView->classificationLabel->setVisible(false);

    popUp->close();
    updateStatus(false, "Collecting metadata for all images in folder(s)");

     /* Must load metadata first, as it contains the file offsets and lengths
     for the thumbnail and full size embedded jpgs and the image width and
     height, req'd in imageCache to manage cache max size. The metadataCaching
     thread also loads the thumbnails. It triggers the loadImageCache when it
     is finished. The image cache is limited by the amount of memory allocated. */

     metadataCacheThread->loadMetadataCache(0, isShowCacheStatus, cacheStatusWidth);

     // format pickMemSize as bytes, KB, MB or GB
     pickMemSize = Utilities::formatMemory(memoryReqdForPicks());
     updateStatus(true);
}

void MW::fileSelectionChange(QModelIndex current, QModelIndex previous)
{
/*
Triggered when file selection changes (folder change selects new image, so it
also triggers this function). The new image is loaded, the pick status is
updated and the infoView metadata is updated. The imageCache is updated if
necessary. The imageCache will not be updated if triggered by
folderSelectionChange since a new one will be build.

It has proven quite difficult to sync the thumbView, tableView and gridView,
keeping the currentIndex in the center position (scrolling). The issue is
timing as it can take the scrollbars a while to finish painting and there isn't
an event to notify the view is ready for action. One to many scrollbar paint
events are fired, and the scrollbar->maximum() value is tested against a
calculated maximum to determine when the last paint event has fired. The
scrollToCurrent function is called. It has also been necessary to write custom
scrollbar functions because the scrollTo(idx, positionAtCenter) does not always
work. Finally, when QAbstractItemView accepts a mouse press it adds a 100ms
delay to avoid double clicks but this plays havoc with the scrolling, so a flag
is used to track which actions are triggering changes in the datamodel index.
If it is a mouse press then a singeShot timer in invoked to delay the scrolling
to after QAbstractItemView is finished.

Note that the datamodel includes multiple columns for each row and the index
sent to fileSelectionChange could be for a column other than 0 (from tableView)
so scrollTo and delegate use of the current index must check the row.
*/
    {
    #ifdef ISDEBUG
    G::track("\n-----------------------------------------------------------------------------");
    G::track(__FUNCTION__, current.data(G::PathRole).toString());
    #endif
    }
    // if starting program, set first image to display
    if (current.row() == -1) {
        thumbView->selectThumb(0);
        return;
    }

//    qDebug() << "MW::fileSelectionChange  dm->sf->rowCount() =" << dm->sf->rowCount()
//             << "selectionModel->selectedIndexes().size() =" << selectionModel->selectedIndexes().size();

//    if (isDragDrop && dragDropFilePath.length() > 0) {
//        thumbView->selectThumb(dragDropFilePath);
//        isDragDrop = false;
//    }

    // user clicks outside thumb but inside treeView dock
    QModelIndexList selected = selectionModel->selectedIndexes();
    if (selected.isEmpty() && !isInitializing) return;

    /* When a mode change occurs, part of the activating process to make the new
       view visible includes setting the currentIndex to what the view was at
       last time it was active.  Consequently we have to save the latest index
       and reapply here.
       */
    if (modeChangeJustHappened) {
        modeChangeJustHappened = false;
        thumbView->setCurrentIndex(previous);
    }
    else thumbView->setCurrentIndex(current);

    // record current row as it is used to sync everything
    currentRow = current.row();
    /*
//    qDebug() << G::t.restart() << "\t" << "\nfileSelectionChange  mode =" << G::mode
//             << "prevMode =" << prevMode
//             << "currentRow = " << currentRow
//             << "previousRow = " << previous.row()
//             << "current" << current;
//    qDebug() << G::t.restart() << "\t" << "thumbView: Visible" << thumbView->isVisible() << "Row =" << thumbView->currentIndex().row() << "Col =" << thumbView->currentIndex().column();
//    qDebug() << G::t.restart() << "\t" << "gridView : Visible" << gridView->isVisible() << "Row =" << gridView->currentIndex().row() << "Col =" << gridView->currentIndex().column();
//    qDebug() << G::t.restart() << "\t" << "tableView: Visible" << tableView->isVisible() << "Row =" << tableView->currentIndex().row() << "Col =" << tableView->currentIndex().column();
//    if (!thumbView->isWrapping()) thumbView->getHorizontalScrollBarOffset(currentRow);
//    gridView->scrollTo(current, QAbstractItemView::ScrollHint::PositionAtCenter);
//    if (gridView->isVisible()) gridView->getVerticalScrollBarOffset(currentRow);
//    thumbView->getVerticalScrollBarMax();
*/

    // update delegates so they can highlight the current item
    thumbView->thumbViewDelegate->currentRow = currentRow;
    gridView->thumbViewDelegate->currentRow = currentRow;

    /* Scroll all visible views to the current row. Qt has 100ms delay in
    QAbstractItemView::mousePressEvent for double-clicks - aarg!
    */
    if (G::lastThumbChangeEvent == "MouseClick") {
        if (mouseClickScroll)
            QTimer::singleShot(1, this, SLOT(delayScroll()));
    }
    else {
        if (gridView->isVisible()) gridView->scrollToCurrent(currentRow);
        if (tableView->isVisible()) tableView->scrollTo(thumbView->currentIndex(),
                 QAbstractItemView::ScrollHint::PositionAtCenter);
    }
    G::lastThumbChangeEvent = "";

    // the file path is used as an index in ImageView and Metadata
    QString fPath = dm->sf->index(currentRow, 0).data(G::PathRole).toString();
    setWindowTitle("Winnow - " + fPath);

    // update the matadata panel
    infoView->updateInfo(fPath);

    // update the status bar with ie "5 of 290   30% zoom   3.6GB picked"
    updateStatus(true);

    // update imageView, use cache if image loaded, else read it from file
    if (G::mode == "Loupe") {
        if (imageView->loadImage(current, fPath)) {
            if (G::isThreadTrackingOn) qDebug()
                << "MW::fileSelectionChange - loaded image file " << fPath;
            updateClassification();
        }
    }

    /* If the metadataCache is finished then update the imageCache, and keep it
    up-to-date with the current image selection. The image caching is delayed
    on a singleshot QTimer to avoid jamming the caching when rapidly proceeding
    through images (ie with arrow key held down) */

    if (metadataLoaded) {
        imageCacheFilePath = fPath;

        // appears to impact performance on slower media
//        imageCacheThread->updateCacheStatusCurrentImagePosition(imageCacheFilePath);

        imageCacheTimer->start(250);
    }

    // terminate initializing flag (set when new folder selected)
    if (isInitializing) {
        isInitializing = false;
        if (dockWidgetArea(thumbDock) == Qt::BottomDockWidgetArea ||
            dockWidgetArea(thumbDock) == Qt::TopDockWidgetArea)
        {
            thumbsWrapAction->setChecked(false);
            thumbView->setWrapping(false);
        }
        thumbsWrapAction->setChecked(true);
        if(thumbDock->isFloating()) thumbView->setWrapping(true);
    }
//    qDebug() << G::t.restart() << "\t" << "MW::fileSelectionChange   End of function";

    // load thumbnail if not done yet
    if (thumbView->isThumb(currentRow)) {
        {
        #ifdef ISDEBUG
        QString s = "Loading thumb for row " + QString::number((currentRow))
                + " " + fPath;
        G::track(__FUNCTION__, s);
        #endif
        }
        QImage image;
        thumb->loadThumb(fPath, image);
        thumbView->setIcon(currentRow, image);
    }
}

void MW::nullFiltration()
{
    updateStatus(false, "No images match the filtration");
    setCentralMessage("No images match the filtration");
    infoView->clearInfo();
    metadata->clear();
    imageView->clear();
    cacheLabel->setVisible(false);
//    isInitializing = false;
    isDragDrop = false;
}

void MW::noFolderSelected()
{
    // Stop any threads that might be running.
    imageCacheThread->stopImageCache();
    metadataCacheThread->stopMetadateCache();
    allMetadataLoaded = false;
    updateStatus(false, "");
    dm->clear();
    infoView->clearInfo();
    metadata->clear();
    imageView->clear();
    cacheLabel->setVisible(false);
    isDragDrop = false;
}

void MW::updateAllMetadataLoaded(bool isLoaded)
{
/*
This slot is signalled from the metadataCacheThread when all the metadata has
been cached (including the thumbs).
*/
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    allMetadataLoaded = isLoaded;
}

void MW::loadMetadataCacheThumbScrollEvent()
{
/*
See MetadataCache::run comments in mdcache.cpp.  A 300ms singleshot timer
insures that the metadata caching is not restarted until there is a pause in
the scolling.

This function is connected to the value change signal in both the thumbView
horizontal and vertical scrollbars.
*/
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    if (allMetadataLoaded)  return;
    metadataCacheStartRow = thumbView->getFirstVisible();
    metadataCacheScrollTimer->start(cacheDelay);
}

void MW::loadMetadataCacheGridScrollEvent()
{
/*
See MetadataCache::run comments in mdcache.cpp.  A 300ms singleshot timer
insures that the metadata caching is not restarted until there is a pause in
the scolling.

This function is connected to the value change signal in the gridView
vertical scrollbar (does not have a horizontal scrollbar).
*/
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    if (allMetadataLoaded)  return;
    metadataCacheStartRow = gridView->getFirstVisible();
    metadataCacheScrollTimer->start(cacheDelay);
}

void MW::delayProcessLoadMetadataCacheScrollEvent()
{
/*
If there has not been another call to this function in 300ms then the
metadataCacheThread is restarted at the row of the first visible thumb after
the scrolling.
*/
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    if (!allMetadataLoaded && metadataCacheThread->isRunning()) {
        if (metadataCacheStartRow > 0)
            metadataCacheThread->loadMetadataCache(metadataCacheStartRow,
                  isShowCacheStatus, cacheStatusWidth);
    }
}

void MW::loadMetadataCache(int startRow)
{
    {
    #ifdef ISDEBUG
    QString s = "startRow = " + QString::number(startRow);
    G::track(__FUNCTION__, s);
    #endif
    }
    metadataCacheThread->stopMetadateCache();

    // startRow in case user scrolls ahead and thumbs not yet loaded
    metadataCacheThread->loadMetadataCache(startRow, isShowCacheStatus, cacheStatusWidth);
}

void MW::updateImageCache()
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    imageCacheThread->updateImageCachePosition(imageCacheFilePath);
}

void MW::loadImageCache()
{
/*
This is called from the metadataCacheThread after all the metadata has been
loaded. The imageCache loads images until the assigned amount of memory has
been consumed or all the images are cached.
 */
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    metadataLoaded = true;
    // now that metadata is loaded populate the data model
    dm->addMetadata();
    // have to wait for the data before resize table columns
    tableView->resizeColumnsToContents();
    tableView->setColumnWidth(G::PathColumn, 24+8);
    QModelIndexList indexesList = thumbView->selectionModel()->selectedIndexes();

    QString fPath = indexesList.first().data(G::PathRole).toString();
    // imageCacheThread checks if already running and restarts
    imageCacheThread->initImageCache(dm->imageFilePathList, cacheSizeMB,
        isShowCacheStatus, cacheStatusWidth, cacheWtAhead, isCachePreview,
        cachePreviewWidth, cachePreviewHeight);
    imageCacheThread->updateImageCachePosition(fPath);
}

void MW::loadFilteredImageCache()
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    QModelIndex idx = thumbView->currentIndex();
    QString fPath = idx.data(G::PathRole).toString();
    thumbView->selectThumb(idx);

    dm->updateImageList();

    // imageChacheThread checks if already running and restarts
    imageCacheThread->initImageCache(dm->imageFilePathList, cacheSizeMB,
        isShowCacheStatus, cacheStatusWidth, cacheWtAhead, isCachePreview,
        cachePreviewWidth, cachePreviewHeight);
    imageCacheThread->updateImageCachePosition(fPath);
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
    if (isFolderValid(fPath, true, false)) {
        QModelIndex idx = fsTree->fsModel->index(item->toolTip(col));
        QModelIndex filterIdx = fsTree->fsFilter->mapFromSource(idx);
        fsTree->setCurrentIndex(filterIdx);
        fsTree->scrollTo(filterIdx, QAbstractItemView::PositionAtCenter);
        folderSelectionChange();
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
    if (isInitializing) return;

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

//    QAction::setShortcutVisibleInContextMenu(true);

    int n;          // used to populate action lists

    // File menu

    openAction = new QAction(tr("Open Folder"), this);
    openAction->setObjectName("openFolder");
    openAction->setShortcutVisibleInContextMenu(true);
    addAction(openAction);
    connect(openAction, &QAction::triggered, this, &MW::openFolder);

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
    connect(manageAppAction, &QAction::triggered, this, &MW::openWithProgramManagement);

    /* read external apps from QStettings */
    setting->beginGroup("ExternalApps");
    QStringList extApps = setting->childKeys();
    n = extApps.size();
    for (int i = 0; i < 10; ++i) {
        QString name;
        QString objName = "";
        if (i < n) {
            externalApps[extApps.at(i)] = setting->value(extApps.at(i)).toString();
            name = extApps.at(i);
            objName = "app" + QString::number(i);
        }
        else name = "Future app" + QString::number(i);

        appActions.append(new QAction(name, this));
        if (i < n) {
            appActions.at(i)->setShortcut(QKeySequence("Alt+" + QString::number(i)));
            appActions.at(i)->setObjectName(objName);
            appActions.at(i)->setText(name);
            appActions.at(i)->setVisible(true);
            addAction(appActions.at(i));
        }
        if (i >= n) appActions.at(i)->setVisible(false);
        appActions.at(i)->setShortcut(QKeySequence("Alt+" + QString::number(i)));
        connect(appActions.at(i), &QAction::triggered, this, &MW::runExternalApp);
    }
    addActions(appActions);
    setting->endGroup();

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
    }
    addActions(recentFolderActions);

    QString reveal = "Reveal";
    #ifdef Q_OS_WIN
        reveal = "Open in Explorer";
    #endif
    #ifdef Q_OS_MAC
        reveal = "Reveal in Finder";
    #endif

    revealFileAction = new QAction(reveal, this);
    revealFileAction->setObjectName("openInFinder");
    revealFileAction->setShortcutVisibleInContextMenu(true);
    addAction(revealFileAction);
    connect(revealFileAction, &QAction::triggered, this, &MW::revealFile);

    subFoldersAction = new QAction(tr("Include Sub-folders"), this);
    subFoldersAction->setObjectName("subFolders");
    subFoldersAction->setShortcutVisibleInContextMenu(true);
    subFoldersAction->setCheckable(true);
    subFoldersAction->setChecked(false);
    addAction(subFoldersAction);
    connect(subFoldersAction, &QAction::triggered, this, &MW::updateSubfolderStatus);

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
    addAction(showImageCountAction);
    connect(showImageCountAction, &QAction::triggered, this, &MW::setShowImageCount);

    addBookmarkAction = new QAction(tr("Add Bookmark"), this);
    addBookmarkAction->setObjectName("addBookmark");
    addBookmarkAction->setShortcutVisibleInContextMenu(true);
    addAction(addBookmarkAction);
    connect(addBookmarkAction, &QAction::triggered, this, &MW::addNewBookmark);

    removeBookmarkAction = new QAction(tr("Remove Bookmark"), this);
    removeBookmarkAction->setObjectName("removeBookmark");
    removeBookmarkAction->setShortcutVisibleInContextMenu(true);
    // rgh where did removeBookmark slot function go?
    addAction(removeBookmarkAction);
    connect(removeBookmarkAction, &QAction::triggered, this, &MW::removeBookmark);

    ingestAction = new QAction(tr("Ingest picks"), this);
    ingestAction->setObjectName("ingest");
    ingestAction->setShortcutVisibleInContextMenu(true);
    addAction(ingestAction);
    connect(ingestAction, &QAction::triggered, this, &MW::ingest);

    ejectAction = new QAction(tr("Eject Usb Drive"), this);
    ejectAction->setObjectName("ingest");
    ejectAction->setShortcutVisibleInContextMenu(true);
    addAction(ejectAction);
    connect(ejectAction, &QAction::triggered, this, &MW::ejectUsbFromContextMenu);

    combineRawJpgAction = new QAction(tr("Combine Raw+Jpg"), this);
    combineRawJpgAction->setObjectName("combineRawJpg");
    combineRawJpgAction->setShortcutVisibleInContextMenu(true);
    combineRawJpgAction->setCheckable(true);
    combineRawJpgAction->setChecked(true);
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
            thumbView, &ThumbView::invertSelection);

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

    filterPickAction = new QAction(tr("Filter picks only"), this);
    filterPickAction->setObjectName("toggleFilterPick");
    filterPickAction->setShortcutVisibleInContextMenu(true);
    filterPickAction->setCheckable(true);
    filterPickAction->setChecked(false);
    addAction(filterPickAction);
    connect(filterPickAction, &QAction::triggered, filters, &Filters::checkPicks);

    popPickHistoryAction = new QAction(tr("Undo pick history"), this);
    popPickHistoryAction->setObjectName("togglePick");
    popPickHistoryAction->setShortcutVisibleInContextMenu(true);
    addAction(popPickHistoryAction);
    connect(popPickHistoryAction, &QAction::triggered, this, &MW::popPick);

    // Place keeper for now
    copyImagesAction = new QAction(tr("Copy to clipboard"), this);
    copyImagesAction->setObjectName("copyImages");
    copyImagesAction->setShortcutVisibleInContextMenu(true);
    copyImagesAction->setShortcut(QKeySequence("Ctrl+C"));
    addAction(copyImagesAction);
    connect(copyImagesAction, &QAction::triggered, thumbView, &ThumbView::copyThumbs);

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
    addAction(rotateLeftAction);
    connect(rotateLeftAction, &QAction::triggered, this, &MW::rotateLeft);

    prefAction = new QAction(tr("Preferences"), this);
    prefAction->setObjectName("settings");
    prefAction->setShortcutVisibleInContextMenu(true);
    addAction(prefAction);
//    connect(prefAction, &QAction::triggered, this, &MW::preferences);
    // use lambda to send default -1 which loads previous open page in pref dialog
    connect(prefAction, &QAction::triggered, [this]() {preferences(-1);} );

    // Go menu

    keyRightAction = new QAction(tr("Next Image"), this);
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

    keyUpAction = new QAction(tr("Move Up"), this);
    keyUpAction->setObjectName("moveUp");
    keyUpAction->setShortcutVisibleInContextMenu(true);
    addAction(keyUpAction);
    connect(keyUpAction, &QAction::triggered, this, &MW::keyUp);

    keyDownAction = new QAction(tr("Move Down"), this);
    keyDownAction->setObjectName("moveDown");
    keyDownAction->setShortcutVisibleInContextMenu(true);
    addAction(keyDownAction);
    connect(keyDownAction, &QAction::triggered, this, &MW::keyDown);

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

    // Not a menu item - used by slide show
    randomImageAction = new QAction(tr("Random"), this);
    randomImageAction->setObjectName("randomImage");
    randomImageAction->setShortcutVisibleInContextMenu(true);
    addAction(randomImageAction);
    connect(randomImageAction, &QAction::triggered, thumbView, &ThumbView::selectRandom);

    nextPickAction = new QAction(tr("Next Pick"), this);
    nextPickAction->setObjectName("nextPick");
    nextPickAction->setShortcutVisibleInContextMenu(true);
    addAction(nextPickAction);
    connect(nextPickAction, &QAction::triggered, thumbView, &ThumbView::selectNextPick);

    prevPickAction = new QAction(tr("Previous Pick"), this);
    prevPickAction->setObjectName("prevPick");
    prevPickAction->setShortcutVisibleInContextMenu(true);
    addAction(prevPickAction);
    connect(prevPickAction, &QAction::triggered, thumbView, &ThumbView::selectPrevPick);

    // Filters

    uncheckAllFiltersAction = new QAction(tr("Uncheck all filters"), this);
    uncheckAllFiltersAction->setObjectName("uncheckAllFilters");
    uncheckAllFiltersAction->setShortcutVisibleInContextMenu(true);
    addAction(uncheckAllFiltersAction);
    connect(uncheckAllFiltersAction, &QAction::triggered, this, &MW::uncheckAllFilters);

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
    filterInvertAction->setCheckable(true);
    addAction(filterInvertAction);
    connect(filterInvertAction,  &QAction::triggered, this, &MW::invertFilters);

    // Sort Menu

    sortFileNameAction = new QAction(tr("Sort by file name"), this);
    sortFileNameAction->setShortcutVisibleInContextMenu(true);
    sortFileNameAction->setCheckable(true);
    addAction(sortFileNameAction);
    connect(sortFileNameAction, &QAction::triggered, this, &MW::sortThumbnails);

    sortFileTypeAction = new QAction(tr("Sort by file type"), this);
    sortFileTypeAction->setShortcutVisibleInContextMenu(true);
    sortFileTypeAction->setCheckable(true);
    addAction(sortFileTypeAction);
    connect(sortFileTypeAction, &QAction::triggered, this, &MW::sortThumbnails);

    sortFileSizeAction = new QAction(tr("Sort by file size"), this);
    sortFileSizeAction->setShortcutVisibleInContextMenu(true);
    sortFileSizeAction->setCheckable(true);
    addAction(sortFileSizeAction);
    connect(sortFileSizeAction, &QAction::triggered, this, &MW::sortThumbnails);

    sortCreateAction = new QAction(tr("Sort by created time"), this);
    sortCreateAction->setShortcutVisibleInContextMenu(true);
    sortCreateAction->setCheckable(true);
    addAction(sortCreateAction);
    connect(sortCreateAction, &QAction::triggered, this, &MW::sortThumbnails);

    sortModifyAction = new QAction(tr("Sort by last modified time"), this);
    sortModifyAction->setShortcutVisibleInContextMenu(true);
    sortModifyAction->setCheckable(true);
    addAction(sortModifyAction);
    connect(sortModifyAction, &QAction::triggered, this, &MW::sortThumbnails);

    sortPickAction = new QAction(tr("Sort by picked status"), this);
    sortPickAction->setShortcutVisibleInContextMenu(true);
    sortPickAction->setCheckable(true);
    addAction(sortPickAction);
    connect(sortPickAction, &QAction::triggered, this, &MW::sortThumbnails);

    sortLabelAction = new QAction(tr("Sort by label"), this);
    sortLabelAction->setShortcutVisibleInContextMenu(true);
    sortLabelAction->setCheckable(true);
    addAction(sortLabelAction);
    connect(sortLabelAction, &QAction::triggered, this, &MW::sortThumbnails);

    sortRatingAction = new QAction(tr("Sort by rating"), this);
    sortRatingAction->setShortcutVisibleInContextMenu(true);
    sortRatingAction->setCheckable(true);
    addAction(sortRatingAction);
    connect(sortRatingAction, &QAction::triggered, this, &MW::sortThumbnails);

    sortMegaPixelsAction = new QAction(tr("Sort by megapixels"), this);
    sortMegaPixelsAction->setShortcutVisibleInContextMenu(true);
    sortMegaPixelsAction->setCheckable(true);
    addAction(sortMegaPixelsAction);
    connect(sortMegaPixelsAction, &QAction::triggered, this, &MW::sortThumbnails);

    sortDimensionsAction = new QAction(tr("Sort by dimensions"), this);
    sortDimensionsAction->setShortcutVisibleInContextMenu(true);
    sortDimensionsAction->setCheckable(true);
    addAction(sortDimensionsAction);
    connect(sortDimensionsAction, &QAction::triggered, this, &MW::sortThumbnails);

    sortApertureAction = new QAction(tr("Sort by aperture"), this);
//    sortApertureAction->setObjectName("SortAperture");
    sortApertureAction->setShortcutVisibleInContextMenu(true);
    sortApertureAction->setCheckable(true);
    addAction(sortApertureAction);
    connect(sortApertureAction, &QAction::triggered, this, &MW::sortThumbnails);

    sortShutterSpeedAction = new QAction(tr("Sort by shutter speed"), this);
    sortShutterSpeedAction->setShortcutVisibleInContextMenu(true);
    sortShutterSpeedAction->setCheckable(true);
    addAction(sortShutterSpeedAction);
    connect(sortShutterSpeedAction, &QAction::triggered, this, &MW::sortThumbnails);

    sortISOAction = new QAction(tr("Sort by ISO"), this);
    sortISOAction->setShortcutVisibleInContextMenu(true);
    sortISOAction->setCheckable(true);
    addAction(sortISOAction);
    connect(sortISOAction, &QAction::triggered, this, &MW::sortThumbnails);

    sortModelAction = new QAction(tr("Sort by camera model"), this);
    sortModelAction->setShortcutVisibleInContextMenu(true);
    sortModelAction->setCheckable(true);
    addAction(sortModelAction);
    connect(sortModelAction, &QAction::triggered, this, &MW::sortThumbnails);

    sortFocalLengthAction = new QAction(tr("Sort by focal length"), this);
    sortFocalLengthAction->setShortcutVisibleInContextMenu(true);
    sortFocalLengthAction->setCheckable(true);
    addAction(sortFocalLengthAction);
    connect(sortFocalLengthAction, &QAction::triggered, this, &MW::sortThumbnails);

    sortTitleAction = new QAction(tr("Sort by title"), this);
    sortTitleAction->setShortcutVisibleInContextMenu(true);
    sortTitleAction->setCheckable(true);
    addAction(sortTitleAction);
    connect(sortTitleAction, &QAction::triggered, this, &MW::sortThumbnails);

    sortLensAction = new QAction(tr("Sort by lens"), this);
    sortLensAction->setShortcutVisibleInContextMenu(true);
    sortLensAction->setCheckable(true);
    addAction(sortLensAction);
    connect(sortLensAction, &QAction::triggered, this, &MW::sortThumbnails);

    sortCreatorAction = new QAction(tr("Sort by creator"), this);
    sortCreatorAction->setShortcutVisibleInContextMenu(true);
    sortCreatorAction->setCheckable(true);
    addAction(sortCreatorAction);
    connect(sortCreatorAction, &QAction::triggered, this, &MW::sortThumbnails);

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
    connect(sortReverseAction, &QAction::triggered, this, &MW::sortThumbnails);

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
//    fullScreenAction->setChecked(setting->value("isFullScreen").toBool());
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
    addAction(infoSelectAction);
    connect(infoSelectAction, &QAction::triggered, this, &MW::selectShootingInfo);

    asLoupeAction = new QAction(tr("Loupe"), this);
    asLoupeAction->setShortcutVisibleInContextMenu(true);
    asLoupeAction->setCheckable(true);
    addAction(asLoupeAction);
    connect(asLoupeAction, &QAction::triggered, this, &MW::loupeDisplay);

    asGridAction = new QAction(tr("Grid"), this);
    asGridAction->setShortcutVisibleInContextMenu(true);
    asGridAction->setCheckable(true);
//    asGridAction->setChecked(setting->value("isGridDisplay").toBool());
    addAction(asGridAction);
    connect(asGridAction, &QAction::triggered, this, &MW::gridDisplay);

    asTableAction = new QAction(tr("Table"), this);
    asTableAction->setShortcutVisibleInContextMenu(true);
    asTableAction->setCheckable(true);
    addAction(asTableAction);
    connect(asTableAction, &QAction::triggered, this, &MW::tableDisplay);

    asCompareAction = new QAction(tr("Compare"), this);
    asCompareAction->setShortcutVisibleInContextMenu(true);
    asCompareAction->setCheckable(true);
//    asCompareAction->setChecked(false); // never start with compare set true
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

    thumbsWrapAction = new QAction(tr("Wrap thumbs"), this);
    thumbsWrapAction->setObjectName("wrapThumbs");
    thumbsWrapAction->setShortcutVisibleInContextMenu(true);
    thumbsWrapAction->setCheckable(true);
    addAction(thumbsWrapAction);
    connect(thumbsWrapAction, &QAction::triggered, this, &MW::toggleThumbWrap);

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

    thumbsFitAction = new QAction(tr("Fit thumbs"), this);
    thumbsFitAction->setObjectName("thumbsZoomOut");
    thumbsFitAction->setShortcutVisibleInContextMenu(true);
    addAction(thumbsFitAction);
    connect(thumbsFitAction, &QAction::triggered, this, &MW::setDockFitThumbs);

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
    addAction(menuBarVisibleAction);
    connect(menuBarVisibleAction, &QAction::triggered, this, &MW::setMenuBarVisibility);
//#endif

    statusBarVisibleAction = new QAction(tr("Statusbar"), this);
    statusBarVisibleAction->setObjectName("toggleStatusBar");
    statusBarVisibleAction->setShortcutVisibleInContextMenu(true);
    statusBarVisibleAction->setCheckable(true);
    addAction(statusBarVisibleAction);
    connect(statusBarVisibleAction, &QAction::triggered, this, &MW::setStatusBarVisibility);

    folderDockVisibleAction = new QAction(tr("Folder"), this);
    folderDockVisibleAction->setObjectName("toggleFiless");
    folderDockVisibleAction->setShortcutVisibleInContextMenu(true);
    folderDockVisibleAction->setCheckable(true);
    addAction(folderDockVisibleAction);
    connect(folderDockVisibleAction, &QAction::triggered, this, &MW::toggleFolderDockVisibility);

    favDockVisibleAction = new QAction(tr("Favourites"), this);
    favDockVisibleAction->setObjectName("toggleFavs");
    favDockVisibleAction->setShortcutVisibleInContextMenu(true);
    favDockVisibleAction->setCheckable(true);
    addAction(favDockVisibleAction);
    connect(favDockVisibleAction, &QAction::triggered, this, &MW::toggleFavDockVisibility);

    filterDockVisibleAction = new QAction(tr("Filters"), this);
    filterDockVisibleAction->setObjectName("toggleFilters");
    filterDockVisibleAction->setShortcutVisibleInContextMenu(true);
    filterDockVisibleAction->setCheckable(true);
    addAction(filterDockVisibleAction);
    connect(filterDockVisibleAction, &QAction::triggered, this, &MW::toggleFilterDockVisibility);

    metadataDockVisibleAction = new QAction(tr("Metadata"), this);
    metadataDockVisibleAction->setObjectName("toggleMetadata");
    metadataDockVisibleAction->setShortcutVisibleInContextMenu(true);
    metadataDockVisibleAction->setCheckable(true);
    addAction(metadataDockVisibleAction);
    connect(metadataDockVisibleAction, &QAction::triggered, this, &MW::toggleMetadataDockVisibility);

    thumbDockVisibleAction = new QAction(tr("Thumbnails"), this);
    thumbDockVisibleAction->setObjectName("toggleThumbs");
    thumbDockVisibleAction->setShortcutVisibleInContextMenu(true);
    thumbDockVisibleAction->setCheckable(true);
    addAction(thumbDockVisibleAction);
    connect(thumbDockVisibleAction, &QAction::triggered, this, &MW::toggleThumbDockVisibity);

    // Window menu focus actions

//    folderDockFocusAction = new QAction(tr("Focus on Folders"), this);
//    folderDockFocusAction->setObjectName("FocusFolders");
//    folderDockFocusAction->setShortcutVisibleInContextMenu(true);
//    addAction(folderDockFocusAction);
//    connect(folderDockFocusAction, &QAction::triggered, this, &MW::setFolderDockFocus);

//    favDockFocusAction = new QAction(tr("Focus on Favourites"), this);
//    favDockFocusAction->setObjectName("FocusFavourites");
//    favDockFocusAction->setShortcutVisibleInContextMenu(true);
//    addAction(favDockFocusAction);
//    connect(favDockFocusAction, &QAction::triggered, this, &MW::setFavDockFocus);

//    filterDockFocusAction = new QAction(tr("Focus on Filters"), this);
//    filterDockFocusAction->setObjectName("FocusFilters");
//    filterDockFocusAction->setShortcutVisibleInContextMenu(true);
//    addAction(filterDockFocusAction);
//    connect(filterDockFocusAction, &QAction::triggered, this, &MW::setFilterDockFocus);

//    metadataDockFocusAction = new QAction(tr("Focus on Metadata"), this);
//    metadataDockFocusAction->setObjectName("FocusMetadata");
//    metadataDockFocusAction->setShortcutVisibleInContextMenu(true);
//    addAction(metadataDockFocusAction);
//    connect(metadataDockFocusAction, &QAction::triggered, this, &MW::setMetadataDockFocus);

//    thumbDockFocusAction = new QAction(tr("Focus on Thumbs"), this);
//    thumbDockFocusAction->setObjectName("FocusThumbs");
//    thumbDockFocusAction->setShortcutVisibleInContextMenu(true);
//    addAction(thumbDockFocusAction);
//    connect(thumbDockFocusAction, &QAction::triggered, this, &MW::setThumbDockFocus);

    // Lock docks (hide titlebar) actions
    folderDockLockAction = new QAction(tr("Hide Folder Titlebar"), this);
    folderDockLockAction->setObjectName("lockDockFiles");
    folderDockLockAction->setShortcutVisibleInContextMenu(true);
    folderDockLockAction->setCheckable(true);
    addAction(folderDockLockAction);
    connect(folderDockLockAction, &QAction::triggered, this, &MW::setFolderDockLockMode);

    favDockLockAction = new QAction(tr("Hide Favourite titlebar"), this);
    favDockLockAction->setObjectName("lockDockFavs");
    favDockLockAction->setShortcutVisibleInContextMenu(true);
    favDockLockAction->setCheckable(true);
    addAction(favDockLockAction);
    connect(favDockLockAction, &QAction::triggered, this, &MW::setFavDockLockMode);

    filterDockLockAction = new QAction(tr("Hide Filter titlebars"), this);
    filterDockLockAction->setObjectName("lockDockFilters");
    filterDockLockAction->setShortcutVisibleInContextMenu(true);
    filterDockLockAction->setCheckable(true);
    addAction(filterDockLockAction);
    connect(filterDockLockAction, &QAction::triggered, this, &MW::setFilterDockLockMode);

    metadataDockLockAction = new QAction(tr("Hide Metadata Titlebar"), this);
    metadataDockLockAction->setObjectName("lockDockMetadata");
    metadataDockLockAction->setShortcutVisibleInContextMenu(true);
    metadataDockLockAction->setCheckable(true);
    addAction(metadataDockLockAction);
    connect(metadataDockLockAction, &QAction::triggered, this, &MW::setMetadataDockLockMode);

    thumbDockLockAction = new QAction(tr("Hide Thumbs Titlebar"), this);
    thumbDockLockAction->setObjectName("lockDockThumbs");
    thumbDockLockAction->setShortcutVisibleInContextMenu(true);
    thumbDockLockAction->setCheckable(true);
    addAction(thumbDockLockAction);
    connect(thumbDockLockAction, &QAction::triggered, this, &MW::setThumbDockLockMode);

    allDocksLockAction = new QAction(tr("Hide All Titlebars"), this);
    allDocksLockAction->setObjectName("lockDocks");
    allDocksLockAction->setShortcutVisibleInContextMenu(true);
    allDocksLockAction->setCheckable(true);
    addAction(allDocksLockAction);
    connect(allDocksLockAction, &QAction::triggered, this, &MW::setAllDocksLockMode);

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

    // Testing

    testAction = new QAction(tr("Test Metadata"), this);
    testAction->setObjectName("test");
    testAction->setShortcutVisibleInContextMenu(true);
    addAction(testAction);
    testAction->setShortcut(QKeySequence("Shift+Ctrl+Alt+T"));
    connect(testAction, &QAction::triggered, this, &MW::test);

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
    fileMenu->addAction(refreshFoldersAction);
    fileMenu->addSeparator();
    fileMenu->addAction(ingestAction);
    fileMenu->addAction(combineRawJpgAction);
    fileMenu->addSeparator();
    fileMenu->addAction(revealFileAction);
    fileMenu->addAction(subFoldersAction);
    fileMenu->addAction(showImageCountAction);
    fileMenu->addAction(addBookmarkAction);
//    fileMenu->addSeparator();
//    fileMenu->addAction(renameAction);
//    fileMenu->addAction(runDropletAction);
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
    editMenu->addAction(refineAction);
    editMenu->addAction(pickAction);
    editMenu->addAction(filterPickAction);
    editMenu->addAction(popPickHistoryAction);
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
    goMenu->addAction(keyHomeAction);
    goMenu->addAction(keyEndAction);
    goMenu->addSeparator();
    goMenu->addAction(nextPickAction);
    goMenu->addAction(prevPickAction);

    // Filter Menu

    QMenu *filterMenu = new QMenu(this);
    QAction *filterGroupAct = new QAction("Filter", this);
    filterGroupAct->setMenu(filterMenu);
    filterMenu->addAction(uncheckAllFiltersAction);
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
    filterMenu->addAction(filterInvertAction);

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
    viewMenu->addAction(thumbsWrapAction);
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
    fsTreeActions->append(ejectAction);
    fsTreeActions->append(separatorAction);
    fsTreeActions->append(showImageCountAction);
    fsTreeActions->append(revealFileAction);
    fsTreeActions->append(separatorAction1);
    fsTreeActions->append(pasteAction);
    fsTreeActions->append(separatorAction2);
    fsTreeActions->append(openWithMenuAction);
    fsTreeActions->append(addBookmarkAction);
    fsTreeActions->append(separatorAction3);
    fsTreeActions->append(folderDockLockAction);

    // filters context menu
    filterActions = new QList<QAction *>;
//    QList<QAction *> *filterActions = new QList<QAction *>;
    filterActions->append(uncheckAllFiltersAction);
    filterActions->append(separatorAction);
    filterActions->append(expandAllAction);
    filterActions->append(collapseAllAction);
    filterActions->append(separatorAction1);
    filterActions->append(filterDockLockAction);

    // bookmarks context menu
    QList<QAction *> *favActions = new QList<QAction *>;
    favActions->append(pasteAction);
    favActions->append(removeBookmarkAction);
    favActions->append(separatorAction);
    favActions->append(favDockLockAction);

    // metadata context menu
    QList<QAction *> *metadataActions = new QList<QAction *>;
    metadataActions->append(infoView->copyInfoAction);
    metadataActions->append(reportMetadataAction);
    metadataActions->append(prefAction);
    metadataActions->append(separatorAction);
    metadataActions->append(metadataDockLockAction);

    // thumbview context menu
    QList<QAction *> *thumbViewActions = new QList<QAction *>;
    thumbViewActions->append(revealFileAction);
    thumbViewActions->append(openWithMenuAction);
    thumbViewActions->append(separatorAction);
    thumbViewActions->append(selectAllAction);
    thumbViewActions->append(invertSelectionAction);
    thumbViewActions->append(separatorAction1);
    thumbViewActions->append(rotateRightAction);
    thumbViewActions->append(rotateLeftAction);
    thumbViewActions->append(separatorAction2);
    thumbViewActions->append(thumbsWrapAction);
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
    // loadSettings not run yet
    combineRawJpg = setting->value("combineRawJpg").toBool();
    dm = new DataModel(this, metadata, filters, combineRawJpg);
    thumb = new Thumb(this, metadata);

    connect(dm->sf, &SortFilter::reloadImageCache, this, &MW::loadFilteredImageCache);
//    connect(dm->sf, &SortFilter::nullFilter, this, &MW::nullSelection);
    connect(dm, &DataModel::updateClassification, this, &MW::updateClassification);
    connect(dm, &DataModel::popup, this, &MW::popup);
    connect(dm, &DataModel::closePopup, this, &MW::closePopup);
    connect(dm, &DataModel::msg, this, &MW::setCentralMessage);
}

void MW::createSelectionModel()
{
/*
The application only has one selection model which is shared by ThumbView,
GridView and TableView, insuring that each view is in sync.
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
//    connect(selectionModel, SIGNAL(currentChanged(QModelIndex, QModelIndex)),
//            this, SLOT(fileSelectionChange(QModelIndex, QModelIndex)));
}

void MW::createCaching()
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    metadataCacheThread = new MetadataCache(this, dm, metadata);
    imageCacheThread = new ImageCache(this, metadata);

    /* When a new folder is selected the metadataCacheThread is started to
       load all the metadata and thumbs for each image.  If the user scrolls
       during the cahe process then the metadataCacheThread is restarted at the
       first visible thumb to speed up the display of the thumbs for the user.
       However, if every scroll event triggered a restart it would be
       inefficient, so this timer is used to wait for a pause in the scrolling
       before triggering a restart at the new place.
    */
    metadataCacheScrollTimer = new QTimer(this);
    metadataCacheScrollTimer->setSingleShot(true);
    // rgh next connect to update
    connect(metadataCacheScrollTimer, SIGNAL(timeout()), this,
            SLOT(delayProcessLoadMetadataCacheScrollEvent()));

    connect(metadataCacheThread, SIGNAL(updateFilterCount()),
            dm, SLOT(filterItemCount()));

    connect(metadataCacheThread, SIGNAL(updateAllMetadataLoaded(bool)),
            this, SLOT(updateAllMetadataLoaded(bool)));

    connect(metadataCacheThread, SIGNAL(loadImageCache()),
            this, SLOT(loadImageCache()));

    connect(metadataCacheThread, SIGNAL(updateIsRunning(bool)),
            this, SLOT(updateMetadataThreadRunStatus(bool)));

    connect(metadataCacheThread, SIGNAL(setIcon(int, QImage)),
            thumbView, SLOT(setIcon(int, QImage)));

    connect(metadataCacheThread, SIGNAL(refreshThumbs()),
            thumbView, SLOT(refreshThumbs()));

    connect(metadataCacheThread, SIGNAL(showCacheStatus(const QImage)),
            this, SLOT(showMetadataCacheStatus(const QImage)));

    imageCacheTimer = new QTimer(this);
    imageCacheTimer->setSingleShot(true);

    connect(imageCacheTimer, SIGNAL(timeout()), this,
            SLOT(updateImageCache()));

    connect(imageCacheThread, SIGNAL(updateIsRunning(bool)),
            this, SLOT(updateImageThreadRunStatus(bool)));

    connect(imageCacheThread, SIGNAL(showCacheStatus(const QImage)),
            this, SLOT(showCacheStatus(const QImage)));

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
    thumbView = new ThumbView(this, dm, "Thumbnails");
    thumbView->setObjectName("Thumbnails");
    thumbView->setAutoScroll(false);

    // loadSettings has not run yet (dependencies, but QSettings has been opened
    thumbView->thumbSpacing = setting->value("thumbSpacing").toInt();
    thumbView->thumbPadding = setting->value("thumbPadding").toInt();
    thumbView->thumbWidth = setting->value("thumbWidth").toInt();
    thumbView->thumbHeight = setting->value("thumbHeight").toInt();
    thumbView->labelFontSize = setting->value("labelFontSize").toInt();
    thumbView->showThumbLabels = setting->value("showThumbLabels").toBool();
    thumbView->wrapThumbs = setting->value("wrapThumbs").toBool();

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
            this, SLOT(loadMetadataCacheThumbScrollEvent()));

    connect(thumbView->horizontalScrollBar(), SIGNAL(valueChanged(int)),
            this, SLOT(loadMetadataCacheThumbScrollEvent()));
}

void MW::createGridView()
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    gridView = new ThumbView(this, dm, "Grid");
    gridView->setObjectName("Grid");
    gridView->setWrapping(true);
    gridView->setAutoScroll(false);

    gridView->thumbSpacing = setting->value("thumbSpacingGrid").toInt();
    gridView->thumbPadding = setting->value("thumbPaddingGrid").toInt();
    gridView->thumbWidth = setting->value("thumbWidthGrid").toInt();
    gridView->thumbHeight = setting->value("thumbHeightGrid").toInt();
    gridView->labelFontSize = setting->value("labelFontSizeGrid").toInt();
    gridView->showThumbLabels = setting->value("showThumbLabelsGrid").toBool();

    // double mouse click fires displayLoupe
    connect(gridView, SIGNAL(displayLoupe()), this, SLOT(loupeDisplay()));

    connect(gridView->verticalScrollBar(), SIGNAL(valueChanged(int)),
            this, SLOT(loadMetadataCacheGridScrollEvent()));
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
    G::track(__FUNCTION__);
    #endif
    }
     /* This is the info displayed on top of the image in loupe view. It is
     dependent on template data stored in QSettings */
    infoString = new InfoString(this, metadata, dm);

    infoString->currentInfoTemplate = setting->value("currentInfoTemplate").toString();
    setting->beginGroup("InfoTokens");
    QStringList keys = setting->childKeys();
    for (int i = 0; i < keys.size(); ++i) {
        QString key = keys.at(i);
        infoString->infoTemplates[key] = setting->value(key).toString();
    }
    setting->endGroup();
    if (!infoString->infoTemplates.contains(" Default"))
        infoString->infoTemplates[" Default"] =
        "{Model} {FocalLength}  {ShutterSpeed} at f/{Aperture}, ISO {ISO}\n{Title}";

    imageView = new ImageView(this,
                              centralWidget,
                              metadata,
                              imageCacheThread,
                              thumbView,
                              infoString,
                              setting->value("isImageInfoVisible").toBool(),
                              setting->value("isRatingBadgeVisible").toBool());

    connect(imageView, SIGNAL(togglePick()), this, SLOT(togglePick()));

    connect(imageView, SIGNAL(updateStatus(bool, QString)),
            this, SLOT(updateStatus(bool, QString)));

    connect(thumbView, SIGNAL(thumbClick(float,float)),
            imageView, SLOT(thumbClick(float,float)));

    connect(imageView, SIGNAL(handleDrop(const QMimeData*)),
            this, SLOT(handleDrop(const QMimeData*)));
}

void MW::createCompareView()
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    compareImages = new CompareImages(this, centralWidget, metadata, dm, thumbView, imageCacheThread);

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
    infoView = new InfoView(this, metadata);
    infoView->setMaximumWidth(folderMaxWidth);

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
    G::track(__FUNCTION__);
    #endif
    }
    // loadSettings has not run yet (dependencies, but QSettings has been opened
    fsTree = new FSTree(this, metadata);
    fsTree->setMaximumWidth(folderMaxWidth);
    fsTree->setShowImageCount(setting->value("showImageCount").toBool());

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

    connect(filters, &Filters::itemClicked, this, &MW::filterChange);
    connect(filters, &Filters::filterChange, this, &MW::filterChange);
}

void MW::createBookmarks()
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    bookmarks = new BookMarks(this, metadata, setting->value("showImageCount").toBool());

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
    // add error trapping for file io  rgh todo
    QFile fStyle(":/qss/winnow.css");
    fStyle.open(QIODevice::ReadOnly);
    this->setStyleSheet(fStyle.readAll());
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

    cacheLabel = new QLabel();
    QString cacheStatusToolTip = "Image cache status for current folder:\n";
    cacheStatusToolTip += "  • LightGray:  \tbackground for all images in folder\n";
    cacheStatusToolTip += "  • DarkGray:   \tto be cached\n";
    cacheStatusToolTip += "  • Green:      \tcached\n";
    cacheStatusToolTip += "  • LightGreen: \tcurrent image";
    cacheLabel->setToolTip(cacheStatusToolTip);
    cacheLabel->setToolTipDuration(100000);
    statusBar()->addPermanentWidget(cacheLabel);

    QLabel *spacer = new QLabel;
    spacer->setText(" ");
    statusBar()->addPermanentWidget(spacer);

    int runLabelWidth = 13;
    metadataThreadRunningLabel = new QLabel;
    QString mtrl = "Turns red when metadata caching in progress";
    metadataThreadRunningLabel->setToolTip(mtrl);
    metadataThreadRunningLabel->setFixedWidth(runLabelWidth);
    updateMetadataThreadRunStatus(false);
    statusBar()->addPermanentWidget(metadataThreadRunningLabel);

    imageThreadRunningLabel = new QLabel;
    statusBar()->addPermanentWidget(imageThreadRunningLabel);
    QString itrl = "Turns red when image caching in progress";
    imageThreadRunningLabel->setToolTip(itrl);
    imageThreadRunningLabel->setFixedWidth(runLabelWidth);
    updateImageThreadRunStatus(false);

//    filterLabel = new QLabel;
    filterStatusLabel->setStyleSheet("QLabel{color:red;}");
    statusBar()->addWidget(filterStatusLabel);
    subfolderStatusLabel->setStyleSheet("QLabel{color:red;}");
    statusBar()->addWidget(subfolderStatusLabel);
    rawJpgStatusLabel->setStyleSheet("QLabel{color:red;}");
    statusBar()->addWidget(rawJpgStatusLabel);

    inactiveThreadRunStatus();
    stateLabel = new QLabel;
    statusBar()->addWidget(stateLabel);
}

void MW::setCacheParameters(int size, bool show, int delay, int width, int wtAhead,
           bool usePreview, bool activity)
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
    cacheSizeMB = size * 1000;      // Entered as GB in pref dlg
    isShowCacheStatus = show;
    cacheDelay = delay;
    cacheStatusWidth = width;
    cacheWtAhead = wtAhead;
    isCachePreview = usePreview;
    // moved to MW::setDisplayresolution
//    cachePreviewWidth = previewWidth;
//    cachePreviewHeight = previewHeight;
    isShowCacheThreadActivity = activity;
    imageCacheThread->updateImageCacheParam(size, show, width, wtAhead,
             usePreview, displayHorizontalPixels, displayVerticalPixels);
    QString fPath = thumbView->currentIndex().data(G::PathRole).toString();

    if (fPath.length())
        imageCacheThread->updateImageCachePosition(fPath);

    // update visibility if preferences have been changed
    cacheLabel->setVisible(isShowCacheStatus);
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
    qreal zoom;
    if (G::mode == "Compare") zoom = compareImages->zoomValue;
    else zoom = imageView->zoom;
    if (zoom < 0 || zoom > 4) return "";
    zoom *= G::actualDevicePixelRatio;
    return QString::number(qRound(zoom*100)) + "%"; // + "% zoom";
}

QString MW::getPicked()
{
/*

*/
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    QModelIndex idx;
    int count = 0;
    for (int row = 0; row < dm->sf->rowCount(); row++)
        if (dm->sf->index(row, G::PickColumn).data() == "true") count++;
    QString image = count == 1 ? " image, " : " images, ";

    if (count == 0) return "Nothing";
    return QString::number(count) + image  + pickMemSize;
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
        stateLabel->setText("");
        QStandardItemModel *k = infoView->ok;
        k->setData(k->index(infoView->PositionRow, 1, infoView->statusInfoIdx), "");
        k->setData(k->index(infoView->ZoomRow, 1, infoView->statusInfoIdx), "");
        k->setData(k->index(infoView->PickedRow, 1, infoView->statusInfoIdx), "");
        return;
    }

    // check for file error first
    QString fPath = thumbView->getCurrentFilename();
    if (metadata->getThumbUnavailable(fPath) || metadata->getImageUnavailable(fPath)) {
        stateLabel->setText(metadata->getErr(fPath));
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
        base = getPosition();
        if (G::mode == "Loupe" || G::mode == "Compare")
            base += spacer + getZoom() + " zoom";
        base += spacer + getPicked() + " picked";
        base += spacer;
    }

    status = " " + base + s;
    stateLabel->setText(status);

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
    stateLabel->setText("");
}

void MW::updateFilterStatus(bool isFilter)
{
    /*
    The filter status is shown in the status bar.
    */
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    if (!isFilter) {
        filterStatusLabel->setText("");
        return;
    }
    if (filters->isAnyFilter())
        filterStatusLabel->setText("FILTERS ");
    else
        filterStatusLabel->setText("");
}

void MW::updateSubfolderStatus()
{
/*
The include subfolder status is shown in the status bar.
*/
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    if (subFoldersAction->isChecked())
        subfolderStatusLabel->setText("SUBFOLDERS ");
    else
        subfolderStatusLabel->setText("");
}

void MW::updateRawJpgStatus()
{
    /*
The include subfolder status is shown in the status bar.
*/
    {
#ifdef ISDEBUG
        G::track(__FUNCTION__);
#endif
    }
    if (combineRawJpgAction->isChecked())
        rawJpgStatusLabel->setText("RAW/JPG ");
    else
        rawJpgStatusLabel->setText("");
}

void MW::updateMetadataThreadRunStatus(bool isRunning)
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
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
}

void MW::updateImageThreadRunStatus(bool isRunning)
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
}

void MW::inactiveThreadRunStatus()
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

void MW::showCacheStatus(const QImage &imCacheStatus)
{
/*
Used by ImageCache thread to show image cache building
progress.
*/
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    cacheLabel->setVisible(true);
    cacheLabel->setPixmap(QPixmap::fromImage(imCacheStatus));
//  following crashes app
    #ifdef ISDEBUG
    G::track(__FUNCTION__, "Completed");
    #endif
}

void MW::showMetadataCacheStatus(const QImage &imCacheStatus)
{
/*
Used by MetadataCache thread to show image cache building
progress.
*/
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    cacheLabel->setVisible(true);
    cacheLabel->setPixmap(QPixmap::fromImage(imCacheStatus));
//  following crashes app
    #ifdef ISDEBUG
    G::track(__FUNCTION__, "Completed");
    #endif
}

void MW::popup(QString msg, int ms, float opacity)
{
/*
This slot is available for other classes to signal in order to show popup
messages, such as DataModel, which does not have access to MW, which is required
by popUp to center itself in the app window.
*/
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    popUp->showPopup(this, msg, ms, opacity);
}

void MW::closePopup()
{
    /*
    This slot is available for other classes to signal in order to close popup
    messages, such as DataModel, which does not have access to MW, which is required
    by popUp to center itself in the app window.
    */
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    popUp->close();
}

void MW::resortImageCache()
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
//    qDebug() << G::t.restart() << "\t" << "ThumbView::reindexImageCache";
    int sfRowCount = dm->sf->rowCount();
    if (!sfRowCount) return;
    dm->updateImageList();
    QModelIndex idx = thumbView->currentIndex();
    QString currentFilePath = idx.data(G::PathRole).toString();
//    qDebug() << G::t.restart() << "\t" << "reindexImageCache current" << currentFilePath;
    if(!dm->imageFilePathList.contains(currentFilePath)) {
        idx = dm->sf->index(0, 0);
        currentFilePath = idx.data(G::PathRole).toString();
        qDebug() << G::t.restart() << "\t" << "reindexImageCache updated" << currentFilePath;
    }
//    QString currentFileName = dm->sf->index(0, 1).data(Qt::EditRole).toString();
    thumbView->selectThumb(idx);
    imageCacheThread->resortImageCache(dm->imageFilePathList, currentFilePath);
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
    G::track(__FUNCTION__);
    #endif
    }
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

void MW::filterChange(bool isFilter)
{
/*
All filter changes should be routed to here as a central clearing house.
*/
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    // refresh the proxy sort/filter
    dm->sf->filterChange();
    // update filter panel image count by filter item
    dm->filterItemCount();
    // update the status panel filtration status
    updateFilterStatus(isFilter);
    // update the image list to match dm->sf filration
    dm->updateImageList();
    // get the current selected item
    QModelIndex idx = thumbView->currentIndex();
    QString currentFilePath = idx.data(G::PathRole).toString();
    // filter the image cache
    imageCacheThread->filterImageCache(dm->imageFilePathList, currentFilePath);

    if (dm->sf->rowCount()) {
        // if filtered but no selection
        if (!thumbView->selectionModel()->selectedRows().count()) {
            thumbView->selectFirst();
            centralLayout->setCurrentIndex(prevCentralView);
        }
        updateStatus(true);
    }
    // if filter has eliminated all rows so nothing to show
    else nullFiltration();
}

void MW::quickFilter()
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
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

    filterChange();
}

void MW::invertFilters()
{
/*
Currently this is just clearing filters ...  rgh what to do?
*/
    if (filterRating1Action->isChecked()) filters->ratings1->setCheckState(0, Qt::Unchecked);
    if (filterRating2Action->isChecked()) filters->ratings2->setCheckState(0, Qt::Unchecked);
    if (filterRating3Action->isChecked()) filters->ratings3->setCheckState(0, Qt::Unchecked);
    if (filterRating4Action->isChecked()) filters->ratings4->setCheckState(0, Qt::Unchecked);
    if (filterRating5Action->isChecked()) filters->ratings5->setCheckState(0, Qt::Unchecked);
    if (filterRedAction->isChecked()) filters->labelsRed->setCheckState(0, Qt::Unchecked);
    if (filterYellowAction->isChecked()) filters->labelsYellow->setCheckState(0, Qt::Unchecked);
    if (filterGreenAction->isChecked()) filters->labelsGreen->setCheckState(0, Qt::Unchecked);
    if (filterBlueAction->isChecked()) filters->labelsBlue->setCheckState(0, Qt::Unchecked);
    if (filterPurpleAction->isChecked()) filters->labelsPurple->setCheckState(0, Qt::Unchecked);

    filterChange();
}

void MW::uncheckAllFilters()
{
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

    filterChange(false);
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
    uncheckAllFilters();

    // Are there any picks to refine?
    bool isPick = false;
    for (int row = 0; row < dm->rowCount(); ++row) {
        if (dm->index(row, G::PickColumn).data() == "true") {
            isPick = true;
            break;
        }
    }

    if (!isPick) {
        popup("There are no picks to refine", 2000, 0.75);
        return;
    }

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

//    // clear all picks
//    for (int row = 0; row < dm->rowCount(); ++row)
//        dm->setData(dm->index(row, G::PickColumn), "false");

    // reset filters
    filters->uncheckAllFilters();
    filters->refineTrue->setCheckState(0, Qt::Checked);

    filterChange(false);
}

void MW::sortThumbnails()
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    if(sortMenuUpdateToMatchTable) return;

    int sortColumn;

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

    thumbView->sortThumbs(sortColumn, sortReverseAction->isChecked());
    resortImageCache();
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

void MW::delayScroll()
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    // scroll views to center on currentIndex
    if (thumbView->isVisible()) thumbView->scrollToCurrent(currentRow);
    if (gridView->isVisible()) gridView->scrollToCurrent(currentRow);
    if (tableView->isVisible()) tableView->scrollTo(thumbView->currentIndex(),
             QAbstractItemView::ScrollHint::PositionAtCenter);
}

void MW::setDockFitThumbs()
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
//    qDebug() << G::t.restart() << "\t" << "Calling setThumbDockFeatures from MW::setThumbsFit";
    setThumbDockFeatures(dockWidgetArea(thumbDock));
}

void MW::thumbsEnlarge()
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    if (G::mode == "Grid") gridView->thumbsEnlarge();
    else thumbView->thumbsEnlarge();
}

void MW::thumbsShrink()
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    if (G::mode == "Grid") gridView->thumbsShrink();
    else thumbView->thumbsShrink();
}

void MW::setThumbLabels()   // move to thumbView
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
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
    G::track(__FUNCTION__);
    #endif
    }
    if (!recentFolders->contains(fPath)) recentFolders->prepend(fPath);
    // sync menu items
    // trim excess items
//    while (recentFolders->count() > maxRecentFolders) {
//        recentFolders->removeAt(recentFolders->count() - 1);
//    }
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
    }
}

void MW::invokeRecentFolder(QAction *recentFolderActions)
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    qDebug() << G::t.restart() << "\t" << recentFolderActions->text();
    QString dirPath = recentFolderActions->text();
//    fsTree->setCurrentIndex(fsTree->fsFilter->mapFromSource(fsTree->fsModel->index(dirPath)));
    fsTree->select(dirPath);
    folderSelectionChange();
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
//    qDebug() << "\n======================================= Invoke Workspace ============================================";
//    if (fullScreenAction->isChecked() != w.isFullScreen)
//        fullScreenAction->setChecked(w.isFullScreen);
//    setFullNormal();
//    if(w.isFullScreen) showFullScreen();
//    else showNormal();
//    showNormal();
    restoreGeometry(w.geometry);
    restoreState(w.state);
    // two restoreState req'd for going from docked to floating docks
    restoreState(w.state);
//    windowTitleBarVisibleAction->setChecked(w.isWindowTitleBarVisible);
//    menuBarVisibleAction->setChecked(true);
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
    thumbView->thumbWidth = w.thumbWidth;
    thumbView->thumbHeight = w.thumbHeight;
    thumbView->thumbSpacing = w.thumbSpacing;
    thumbView->thumbPadding = w.thumbPadding;
    thumbView->labelFontSize = w.labelFontSize;
    thumbView->showThumbLabels = w.showThumbLabels;
    thumbsWrapAction->setChecked(w.wrapThumbs);
    thumbView->wrapThumbs = w.wrapThumbs;
    thumbView->setWrapping(w.wrapThumbs);
    gridView->thumbWidth = w.thumbWidthGrid;
    gridView->thumbHeight = w.thumbHeightGrid;
    gridView->thumbSpacing = w.thumbSpacingGrid;
    gridView->thumbPadding = w.thumbPaddingGrid;
    gridView->labelFontSize = w.labelFontSizeGrid;
    gridView->showThumbLabels = w.showThumbLabelsGrid;
    thumbView->setThumbParameters();
    gridView->setThumbParameters();
    // if in grid view override normal behavior if workspace invoked
    wasThumbDockVisible = w.isThumbDockVisible;
    updateState();
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
//    wsd.isWindowTitleBarVisible = windowTitleBarVisibleAction->isChecked();
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

    wsd.thumbSpacing = thumbView->thumbSpacing;
    wsd.thumbPadding = thumbView->thumbPadding;
    wsd.thumbWidth = thumbView->thumbWidth;
    wsd.thumbHeight = thumbView->thumbHeight;
    wsd.labelFontSize = thumbView->labelFontSize;
    wsd.showThumbLabels = thumbView->showThumbLabels;
    wsd.wrapThumbs = thumbView->wrapThumbs;

    wsd.thumbSpacingGrid = gridView->thumbSpacing;
    wsd.thumbPaddingGrid = gridView->thumbPadding;
    wsd.thumbWidthGrid = gridView->thumbWidth;
    wsd.thumbHeightGrid = gridView->thumbHeight;
    wsd.labelFontSizeGrid = gridView->labelFontSize;
    wsd.showThumbLabelsGrid = gridView->showThumbLabels;

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
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    // rgh remove after debugging
    foreach(QAction *act, workspaceMenu->actions()) {
        qDebug() << G::t.restart() << "\t" << act->objectName() << act;
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
    G::track(__FUNCTION__);
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
        qDebug() << G::t.restart() << "\t" << "sync menu" << i << workspaceActions.at(i)->text();
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
    QRect desktop = qApp->desktop()->availableGeometry();
    resize(0.75 * desktop.width(), 0.75 * desktop.height());
    setGeometry( QStyle::alignedRect(Qt::LeftToRight, Qt::AlignCenter,
        size(), desktop));
//    windowTitleBarVisibleAction->setChecked(true);
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

    thumbView->thumbSpacing = 0;
    thumbView->thumbPadding = 0;
    thumbView->thumbWidth = 100;
    thumbView->thumbHeight = 100;
    thumbView->labelFontSize = 8;
    thumbView->showThumbLabels = true;
    thumbView->wrapThumbs = false;

    gridView->thumbSpacing = 0;
    gridView->thumbPadding = 0;
    gridView->thumbWidth = 160;
    gridView->thumbHeight = 160;
    gridView->labelFontSize = 8;
    gridView->showThumbLabels = true;

    thumbsWrapAction->setChecked(false);
    thumbView->setWrapping(false);

//    qDebug() << G::t.restart() << "\t" << "\nMW::defaultWorkspace before calling setThumbParameters" << "\n"
//             << "***  thumbView Ht =" << thumbView->height()
//             << "thumbSpace Ht =" << thumbView->getThumbCellSize().height()
//             << "thumbHeight =" << thumbView->thumbHeight << "\n";
//    qDebug() << G::t.restart() << "\t" << "Calling setThumbParameters from MW::defaultWorkspace thumbView.thumbWidth" << thumbView->thumbWidth << "gridView.thumbWidth" << gridView->thumbWidth;
    qDebug() << G::t.restart() << "\t" << "🔎🔎🔎 Calling setThumbParameters from MW::defaultWorkspace     thumbHeight ="  << thumbView->thumbHeight;
    thumbView->setThumbParameters();
    gridView->setThumbParameters();

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

//    // enable the folder dock (first one in tab)
//    QList<QTabBar *> tabList = findChildren<QTabBar *>();
//    QTabBar* widgetTabBar = tabList.at(0);
//    widgetTabBar->setCurrentIndex(0);

    resizeDocks({thumbDock}, {100}, Qt::Vertical);
    setDockFitThumbs();
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
    qDebug() << G::t.restart() << "\t" << "MW::renameWorkspace" << n << name;
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
             << "\nwrapThumbs" << ws.wrapThumbs
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
        ws.wrapThumbs = setting->value("wrapThumbs").toBool();
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
             << "\nwrapThumbs" << w.wrapThumbs
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
    metadata->readMetadata(true, thumbView->getCurrentFilename());
}

void MW::about()
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    QString aboutString = "<h2>Winnow version " + version + "</h2>"
        + tr("<p>Image viewer and ingester</p>")
        + "Qt v" + QT_VERSION_STR
        + "<p></p>"
        + "<p>Author: Rory Hill."
        + "<p>Latest change: " + versionDetail
        + "<p>Winnow is licensed under the GNU General Public License version 3</p>";

    QMessageBox::about(this, tr("About") + " Winnow", aboutString);
}

void MW::openWithProgramManagement()
{
    Appdlg *appdlg = new Appdlg(externalApps, this);
    appdlg->exec();
    updateExternalApps();
}

void MW::cleanupSender()
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    delete QObject::sender();
}

void MW::externalAppError(QProcess::ProcessError err)
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    QMessageBox msgBox;
    qDebug() << G::t.restart() << "\t" << "externalAppError  err =" << err;
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

    QString app;
    app = externalApps[((QAction*) sender())->text()];
//    app = enquote(externalApps[((QAction*) sender())->text()]);
    QModelIndexList selectedIdxList = thumbView->selectionModel()->selectedRows();

//    app = "/Applications/Adobe Photoshop CC 2018/Adobe Photoshop CC 2018.app/Contents/MacOS/Adobe Photoshop CC 2018";
//    app = "/Applications/Adobe Photoshop CS6/Adobe Photoshop CS6.app/Contents/MacOS/Adobe Photoshop CS6";
//    QString x = "/Applications/Adobe Photoshop CS6/Adobe Photoshop CS6.app";
//    app = "\"/Applications/Adobe Photoshop CS6/Adobe Photoshop CS6.app\"";

    std::cout << app.toStdString() << std::endl << std::flush;
//    app = "/Applications/Preview.app";
//    std::cout << app.toStdString() << std::endl << std::flush;

    if (selectedIdxList.size() < 1)
    {
        popup("No images have been selected", 2000, 0.75);
        return;
    }

    QStringList arguments;
//    for (int tn = selectedIdxList.size() - 1; tn >= 0 ; --tn) {
//        QString s = selectedIdxList[tn].data(G::PathRole).toString();
//        arguments << s;
////        arguments << enquote(s);
//    }
    arguments << "/Users/roryhill/Pictures/4K/2017-01-25_0030-Edit.jpg";
//    arguments << "/Users/roryhill/Pictures/Eva/2016-06-21_0002.jpg";

//    std::cout << "MW::runExternalApp()  " << app.toStdString()
//              << " " << arguments.at(0).toStdString() << std::flush;

    QProcess *process = new QProcess();
    connect(process, SIGNAL(finished(int, QProcess::ExitStatus)), this, SLOT(cleanupSender()));
    connect(process, SIGNAL(error(QProcess::ProcessError)), this, SLOT(externalAppError(QProcess::ProcessError)));

    process->setArguments(arguments);
    process->setProgram(app);
    process->start();

//    process->start(app);
//     process->start(app, arguments);
//     process->start(app, {"/Users/roryhill/Pictures/4K/2017-01-25_0030-Edit.jpg"});

//    if ( !process->waitForFinished(-1))
//        qDebug() << G::t.restart() << "\t" << process->readAllStandardError();

    //this works in terminal"
    // open "/Users/roryhill/Pictures/4K/2017-01-25_0030-Edit.jpg" -a "Adobe Photoshop CS6"
}

void MW::updateExternalApps()
{
/*
Menus cannot be added/deleted at runtime in MacOS so 10 menu items are created
in the MW constructor and then edited here based on changes made in processdlg.
*/
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    QMapIterator<QString, QString> eaIter(externalApps);
    int i = 0;
    while (eaIter.hasNext()) {
        eaIter.next();
        appActions.at(i)->setObjectName("app" + QString::number(i));
        appActions.at(i)->setText(eaIter.key());
        appActions.at(i)->setVisible(true);
        i++;
    }

    int ii = i;
    for (i = ii; i < 10; i++) {
        appActions.at(i)->setVisible(false);
        appActions.at(i)->setText("");
    }
}

void MW::chooseExternalApp()
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }

//    qDebug() << G::t.restart() << "\t" << "MW::chooseExternalApp";

    // in terminal this works:
    //open -a 'Adobe Photoshop CC 2015.5.app' /Users/roryhill/Pictures/4K/2017-01-25_0030-Edit.jpg

    // this launches photoshop but does not open jpg
    QProcess *process = new QProcess(this);

    QString a = "C:/Program Files/Adobe/Adobe Photoshop CC 2018/photoshop.exe";
    a = QChar('\"') + a + QChar('\"');
    QString c = "D:/Pictures/Business Card/2006-05-06_0005-Edit.psd";
    c = QChar('\"') + c + QChar('\"');

    QString program = a;
    QStringList args;
    args << c;

    process->start(program, args);
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
    if(page == -1) page = lastPrefPage;
    Prefdlg *prefdlg = new Prefdlg(this, page);
    connect(prefdlg, SIGNAL(updatePage(int)),
            this, SLOT(setPrefPage(int)));
    connect(prefdlg, SIGNAL(updateRememberFolder(bool)),
            this, SLOT(setRememberLastDir(bool)));
    connect(prefdlg, SIGNAL(updateMouseClickScroll(bool)),
            this, SLOT(setMouseClickScroll(bool)));
    connect(prefdlg, SIGNAL(updateTrackpadScroll(bool)),
            this, SLOT(setTrackpadScroll(bool)));
    connect(prefdlg, SIGNAL(updateThumbParameters(int,int,int,int,int,bool,bool)),
            thumbView, SLOT(setThumbParameters(int,int,int,int,int,bool,bool)));
    connect(prefdlg, SIGNAL(updateThumbGridParameters(int,int,int,int,int,bool,bool)),
            gridView, SLOT(setThumbParameters(int, int, int, int, int, bool, bool)));
    connect(prefdlg, SIGNAL(updateSlideShowParameters(int, bool)),
            this, SLOT(setSlideShowParameters(int, bool)));
    connect(prefdlg, SIGNAL(updateCacheParameters(int, bool, int, int, int, bool, bool)),
            this, SLOT(setCacheParameters(int, bool, int, int, int, bool, bool)));
    connect(prefdlg, SIGNAL(updateFullScreenDocks(bool,bool,bool,bool,bool,bool)),
            this, SLOT(setFullScreenDocks(bool,bool,bool,bool,bool,bool)));
    prefdlg->exec();
}

void MW::setShowImageCount()
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    if (!fsTree->isVisible()) {
        popUp->showPopup(this, "Show image count is only available when the Folders Panel is visible", 1500, 0.75);
    }
    bool isShow = showImageCountAction->isChecked();
    fsTree->setShowImageCount(isShow);
    fsTree->resizeColumns();
    fsTree->repaint();
    if (isShow) fsTree->fsModel->fetchMore(fsTree->rootIndex());
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

void MW::setRememberLastDir(bool prefRememberFolder)
{
    rememberLastDir = prefRememberFolder;
}

void MW::setMouseClickScroll(bool prefMouseClickScroll)
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    mouseClickScroll = prefMouseClickScroll;
}

void MW::setTrackpadScroll(bool trackpadScroll)
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    imageView->useWheelToScroll = trackpadScroll;
}

void MW::setDisplayResolution()
{
    QPoint loc = centralWidget->window()->geometry().center();

#ifdef Q_OS_WIN
//    G::devicePixelRatio = 1;
    displayHorizontalPixels = qApp->screenAt(loc)->geometry().width();
    displayVerticalPixels = qApp->screenAt(loc)->geometry().height();
#endif

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
    displayHorizontalPixels, displayVerticalPixels = 0;

    // the native resolution is the largest display mode
    for(auto c = count; c--;) {
        mode = (CGDisplayModeRef)CFArrayGetValueAtIndex(modes, c);
        auto w = CGDisplayModeGetWidth(mode);
        auto h = CGDisplayModeGetHeight(mode);
        if (w > displayHorizontalPixels) displayHorizontalPixels = (int)w;
        if (h > displayVerticalPixels) displayVerticalPixels = (int)h;
    }
#endif

    cachePreviewWidth = displayHorizontalPixels;
    cachePreviewHeight = displayVerticalPixels;
    setActualDevicePixelRatio();
}

void MW::setActualDevicePixelRatio()
{
    int virtualWidth = QGuiApplication::primaryScreen()->geometry().width();
    if (displayHorizontalPixels > 0)
        G::actualDevicePixelRatio = (float)displayHorizontalPixels / virtualWidth;
    else
        G::actualDevicePixelRatio = QPaintDevice::devicePixelRatio();

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

void MW::setIngestRootFolder(QString rootFolder, bool isAuto)
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    ingestRootFolder = rootFolder;
    autoIngestFolderPath = isAuto;
}

void MW::setFullScreenDocks(bool isFolders, bool isFavs, bool isFilters,
                            bool isMetadata, bool isThumbs, bool isStatusBar)
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    fullScreenDocks.isFolders = isFolders;
    fullScreenDocks.isFavs = isFavs;
    fullScreenDocks.isFilters = isFilters;
    fullScreenDocks.isMetadata = isMetadata;
    fullScreenDocks.isThumbs = isThumbs;
    fullScreenDocks.isStatusBar = isStatusBar;
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
        menuBarVisibleAction->setChecked(false);
        setMenuBarVisibility();
        statusBarVisibleAction->setChecked(fullScreenDocks.isStatusBar);
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
        popUp->showPopup(this, "The zoom dialog is only available in loupe view", 2000, 0.75);
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
    // rotate current loupe view image
    imageView->rotate(degrees);

    // iterate selection
    QModelIndexList selection = thumbView->selectionModel()->selectedRows();
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
        QModelIndex idx = dm->sf->index(row, G::PathColumn);
        QString fPath = idx.data(G::PathRole).toString();
        metadata->setRotation(fPath, newRotation);

        // rotate thumbnail(s)
        QTransform trans;
        trans.rotate(degrees);
        QModelIndex thumbIdx = dm->sf->index(row, G::PathColumn);
        QStandardItem *item = new QStandardItem;
        item = dm->itemFromIndex(dm->sf->mapToSource(thumbIdx));
        QPixmap pm = item->icon().pixmap(THUMB_MAX, THUMB_MAX);
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
    G::track(__FUNCTION__);
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
    G::track(__FUNCTION__);
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
    setting->setValue("lastPrefPage", (int)lastPrefPage);
    setting->setValue("mouseClickScroll", (bool)mouseClickScroll);
    setting->setValue("toggleZoomValue", imageView->toggleZoom);
//    setting->setValue("displayHorizontalPixels", displayHorizontalPixels);
//    setting->setValue("displayVerticalPixels", displayVerticalPixels);
    setting->setValue("autoIngestFolderPath", autoIngestFolderPath);
    setting->setValue("autoEjectUSB", autoEjectUsb);

    // files
//    setting->setValue("showHiddenFiles", (bool)G::showHiddenFiles);
    setting->setValue("rememberLastDir", rememberLastDir);
    setting->setValue("lastDir", currentViewDir);
    setting->setValue("includeSubfolders", subFoldersAction->isChecked());
    setting->setValue("showImageCount", showImageCountAction->isChecked());
    setting->setValue("combineRawJpg", combineRawJpg);
//    setting->setValue("maxRecentFolders", maxRecentFolders);
    setting->setValue("useWheelToScroll", imageView->useWheelToScroll);
    setting->setValue("ingestRootFolder", ingestRootFolder);

    // thumbs
    setting->setValue("thumbSpacing", thumbView->thumbSpacing);
    setting->setValue("thumbPadding", thumbView->thumbPadding);
    setting->setValue("thumbWidth", thumbView->thumbWidth);
    setting->setValue("thumbHeight", thumbView->thumbHeight);
    setting->setValue("labelFontSize", thumbView->labelFontSize);
    setting->setValue("showThumbLabels", (bool)thumbView->showThumbLabels);
    setting->setValue("wrapThumbs", (bool)thumbView->wrapThumbs);

    setting->setValue("thumbSpacingGrid", gridView->thumbSpacing);
    setting->setValue("thumbPaddingGrid", gridView->thumbPadding);
    setting->setValue("thumbWidthGrid", gridView->thumbWidth);
    setting->setValue("thumbHeightGrid", gridView->thumbHeight);
    setting->setValue("labelFontSizeGrid", gridView->labelFontSize);
    setting->setValue("showThumbLabelsGrid", (bool)gridView->showThumbLabels);
    // slideshow
    setting->setValue("slideShowDelay", (int)slideShowDelay);
    setting->setValue("slideShowRandom", (bool)slideShowRandom);
    setting->setValue("slideShowWrap", (bool)slideShowWrap);
    // cache
    setting->setValue("cacheSizeMB", (int)cacheSizeMB);
    setting->setValue("isShowCacheStatus", (bool)isShowCacheStatus);
    setting->setValue("cacheDelay", (int)cacheDelay);
    setting->setValue("isShowCacheThreadActivity", (bool)isShowCacheThreadActivity);
    setting->setValue("cacheStatusWidth", (int)cacheStatusWidth);
    setting->setValue("cacheWtAhead", (int)cacheWtAhead);
    setting->setValue("isCachePreview", (int)isCachePreview);
    setting->setValue("cachePreviewWidth", (int)cachePreviewWidth);
    setting->setValue("cachePreviewHeight", (int)cachePreviewHeight);
    // full screen
    setting->setValue("isFullScreenFolders", (bool)fullScreenDocks.isFolders);
    setting->setValue("isFullScreenFavs", (bool)fullScreenDocks.isFavs);
    setting->setValue("isFullScreenFilters", (bool)fullScreenDocks.isFilters);
    setting->setValue("isFullScreenMetadata", (bool)fullScreenDocks.isMetadata);
    setting->setValue("isFullScreenThumbs", (bool)fullScreenDocks.isThumbs);
    setting->setValue("isFullScreenStatusBar", (bool)fullScreenDocks.isStatusBar);
    // state
    setting->setValue("Geometry", saveGeometry());
    setting->setValue("WindowState", saveState());
    setting->setValue("isFullScreen", (bool)isFullScreen());
//    setting->setValue("isFullScreen", (bool)fullScreenAction->isChecked());

    setting->setValue("isRatingBadgeVisible", (bool)ratingBadgeVisibleAction->isChecked());
    setting->setValue("isImageInfoVisible", (bool)infoVisibleAction->isChecked());
    setting->setValue("isLoupeDisplay", (bool)asLoupeAction->isChecked());
    setting->setValue("isGridDisplay", (bool)asGridAction->isChecked());
    setting->setValue("isTableDisplay", (bool)asTableAction->isChecked());
    setting->setValue("isCompareDisplay", (bool)asCompareAction->isChecked());

//    setting->setValue("isWindowTitleBarVisible", (bool)windowTitleBarVisibleAction->isChecked());
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
    setting->setValue("wasThumbDockVisible", wasThumbDockVisible);

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
//    for(int row = 0; row < infoView->ok->rowCount(); row++) {
//        QString field = infoView->ok->index(row, 1).data().toString();
//        bool showField = infoView->ok->index(row, 0).data().toBool();
//        setting->setValue(field, showField);
//    }
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
    setting->setValue("pathTemplateSelected", (int)pathTemplateSelected);
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
    setting->setValue("filenameTemplateSelected", (int)filenameTemplateSelected);
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
        setting->setValue("wrapThumbs", ws.wrapThumbs);
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

Action settings are maintained by the actions ie action->isChecked();
They are updated on creation.

Preferences are located in the prefdlg class and updated here.
*/
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    needThumbsRefresh = false;

    // default values for first time use
    if (!setting->contains("cacheSizeMB")) {
        // general
        lastPrefPage = 0;
        imageView->toggleZoom = 1.0;
        rememberLastDir = true;
        mouseClickScroll = false;
//        maxRecentFolders = 10;
        bookmarks->bookmarkPaths.insert(QDir::homePath());
        imageView->useWheelToScroll = true;
        imageView->toggleZoom = 1.0;
        compareImages->toggleZoom = 1.0;
        autoIngestFolderPath = false;
        autoEjectUsb = false;
        combineRawJpg = true;

      // slideshow
        slideShowDelay = 5;
        slideShowRandom = false;
        slideShowWrap = true;

        // cache
        cacheSizeMB = 2000;
        isShowCacheStatus = false;
        cacheDelay = 250;
        isShowCacheThreadActivity = false;
        cacheStatusWidth = 200;
        cacheWtAhead = 5;
        isCachePreview = true;
        cachePreviewWidth = 2000;
        cachePreviewHeight = 1600;

        return false;
    }

    // general
    lastPrefPage = setting->value("lastPrefPage").toInt();
    mouseClickScroll = setting->value("mouseClickScroll").toBool();
    qreal tempZoom = setting->value("toggleZoomValue").toReal();
    if (tempZoom > 3) tempZoom = 1;
    if (tempZoom < 0.25) tempZoom = 1;
    imageView->toggleZoom = tempZoom;
    compareImages->toggleZoom = tempZoom;
//    displayHorizontalPixels = setting->value("displayHorizontalPixels").toInt();
//    displayVerticalPixels = setting->value("displayVerticalPixels").toInt();
    autoIngestFolderPath = setting->value("autoIngestFolderPath").toBool();
    autoEjectUsb = setting->value("autoEjectUSB").toBool();

    // files
//    G::showHiddenFiles = setting->value("showHiddenFiles").toBool();
    rememberLastDir = setting->value("rememberLastDir").toBool();
    lastDir = setting->value("lastDir").toString();
    showImageCountAction->setChecked(setting->value("showImageCount").toBool());
    combineRawJpgAction->setChecked(setting->value("combineRawJpg").toBool());
//    combineRawJpg = setting->value("combineRawJpg").toBool();
    ingestRootFolder = setting->value("ingestRootFolder").toString();
    // trackpad
    imageView->useWheelToScroll = setting->value("useWheelToScroll").toBool();
    // slideshow
    slideShowDelay = setting->value("slideShowDelay").toInt();
    slideShowRandom = setting->value("slideShowRandom").toBool();
    slideShowWrap = setting->value("slideShowWrap").toBool();
    // cache
    cacheSizeMB = setting->value("cacheSizeMB").toInt();
    isShowCacheStatus = setting->value("isShowCacheStatus").toBool();
    cacheDelay = setting->value("cacheDelay").toInt();
    isShowCacheThreadActivity = setting->value("isShowCacheThreadActivity").toBool();
    cacheStatusWidth = setting->value("cacheStatusWidth").toInt();
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

    // load state (action->setChecked in action creation)
    fullScreenAction->setChecked(setting->value("isFullScreen").toBool());
    ratingBadgeVisibleAction->setChecked(setting->value("isRatingBadgeVisible").toBool());
    isRatingBadgeVisible = setting->value("isRatingBadgeVisible").toBool();
    infoVisibleAction->setChecked(setting->value("isImageInfoVisible").toBool());
    asLoupeAction->setChecked(setting->value("isLoupeDisplay").toBool() ||
                              setting->value("isCompareDisplay").toBool());
    asGridAction->setChecked(setting->value("isGridDisplay").toBool());
    asTableAction->setChecked(setting->value("isTableDisplay").toBool());
    asCompareAction->setChecked(false); // never start with compare set true
//    showThumbLabelsAction->setChecked(thumbView->showThumbLabels);

//    windowTitleBarVisibleAction->setChecked(setting->value("isWindowTitleBarVisible").toBool());
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
    wasThumbDockVisible = setting->value("wasThumbDockVisible").toBool();

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

    /* read TableView okToShow fields */
    setting->beginGroup("TableFields");
    setFields = setting->childKeys();
//    QList<QStandardItem *> itemList;
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

    /* read external apps */
    /* moved to createActions as required to populate open with ... menu */

    /* read ingest token templates */
    pathTemplateSelected = setting->value("pathTemplateSelected").toInt();
    setting->beginGroup("PathTokens");
    QStringList keys = setting->childKeys();
    for (int i = 0; i < keys.size(); ++i) {
        QString key = keys.at(i);
        pathTemplates[key] = setting->value(key).toString();
    }
    setting->endGroup();

    filenameTemplateSelected = setting->value("filenameTemplateSelected").toInt();
    setting->beginGroup("FilenameTokens");
    keys = setting->childKeys();
    for (int i = 0; i < keys.size(); ++i) {
        QString key = keys.at(i);
        filenameTemplates[key] = setting->value(key).toString();
    }
    setting->endGroup();

    /* read info token templates moved to createImageView as is reqd early*/
//    currentInfoTemplate = setting->value("currentInfoTemplate").toString();
//    setting->beginGroup("InfoTokens");
//    keys = setting->childKeys();
//    for (int i = 0; i < keys.size(); ++i) {
//        QString key = keys.at(i);
//        infoTemplates[key] = setting->value(key).toString();
//    }
//    setting->endGroup();

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

    /* read ingest description completer list */
    setting->beginGroup("IngestDescriptionCompleter");
    ingestDescriptionCompleter = setting->childKeys();
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
//        qDebug() << G::t.restart() << "\t" << "Default shortcuts";
        //    formats to set shortcut
        //    keyRightAction->setShortcut(QKeySequence("Right"));
        //    keyRightAction->setShortcut((Qt::Key_Right);

//        thumbsGoTopAct->setShortcut(QKeySequence("Ctrl+Home"));
//        thumbsGoBottomAct->setShortcut(QKeySequence("Ctrl+End"));
//        closeImageAct->setShortcut(Qt::Key_Escape);
        //        cutAction->setShortcut(QKeySequence("Ctrl+X"));
        //        copyAction->setShortcut(QKeySequence("Ctrl+C"));
        //        copyImageAction->setShortcut(QKeySequence("Ctrl+Shift+C"));
        //        pasteImageAction->setShortcut(QKeySequence("Ctrl+Shift+V"));
        //        refreshAction->setShortcut(QKeySequence("Ctrl+F5"));
        //        pasteAction->setShortcut(QKeySequence("Ctrl+V"));
        refreshFoldersAction->setShortcut(QKeySequence("Alt+R"));
        subFoldersAction->setShortcut(QKeySequence("B"));
        showImageCountAction->setShortcut(QKeySequence("\\"));
        fullScreenAction->setShortcut(QKeySequence("F"));
//        escapeFullScreenAction->setShortcut(QKeySequence("Esc"));
        prefAction->setShortcut(QKeySequence("Ctrl+,"));
        exitAction->setShortcut(QKeySequence("Ctrl+Q"));
        selectAllAction->setShortcut(QKeySequence("Ctrl+A"));
        invertSelectionAction->setShortcut(QKeySequence("Shift+Ctrl+A"));
        refineAction->setShortcut(QKeySequence("R"));
        pickAction->setShortcut(QKeySequence("`"));
        filterPickAction->setShortcut(QKeySequence("Shift+`"));
        popPickHistoryAction->setShortcut(QKeySequence("Alt+Ctrl+Z"));
        ingestAction->setShortcut(QKeySequence("Q"));
        combineRawJpgAction->setShortcut(QKeySequence("Alt+J"));
//        rate0Action->setShortcut(QKeySequence("!"));
        uncheckAllFiltersAction->setShortcut(QKeySequence("Shift+Ctrl+0"));
        rate1Action->setShortcut(QKeySequence("1"));
        rate2Action->setShortcut(QKeySequence("2"));
        rate3Action->setShortcut(QKeySequence("3"));
        rate4Action->setShortcut(QKeySequence("4"));
        rate5Action->setShortcut(QKeySequence("5"));
        filterRating1Action->setShortcut(QKeySequence("Shift+1"));
        filterRating2Action->setShortcut(QKeySequence("Shift+2"));
        filterRating3Action->setShortcut(QKeySequence("Shift+3"));
        filterRating4Action->setShortcut(QKeySequence("Shift+4"));
        filterRating5Action->setShortcut(QKeySequence("Shift+5"));
//        label0Action->setShortcut(QKeySequence("^"));
        label1Action->setShortcut(QKeySequence("6"));
        label2Action->setShortcut(QKeySequence("7"));
        label3Action->setShortcut(QKeySequence("8"));
        label4Action->setShortcut(QKeySequence("9"));
        label5Action->setShortcut(QKeySequence("0"));
        filterRedAction->setShortcut(QKeySequence("Shift+6"));
        filterYellowAction->setShortcut(QKeySequence("Shift+7"));
        filterGreenAction->setShortcut(QKeySequence("Shift+8"));
        filterBlueAction->setShortcut(QKeySequence("Shift+9"));
        filterPurpleAction->setShortcut(QKeySequence("Shift+0"));
        reportMetadataAction->setShortcut(QKeySequence("Shift+Ctrl+R"));
        slideShowAction->setShortcut(QKeySequence("S"));
        thumbsFitAction->setShortcut(QKeySequence("Alt+]"));
        thumbsEnlargeAction->setShortcut(QKeySequence("]"));
        thumbsShrinkAction->setShortcut(QKeySequence("["));
        keyRightAction->setShortcut(QKeySequence("Right"));
        keyLeftAction->setShortcut(QKeySequence("Left"));
        keyHomeAction->setShortcut(QKeySequence("Home"));
        keyEndAction->setShortcut(QKeySequence("End"));
        keyDownAction->setShortcut(QKeySequence("Down"));
        keyUpAction->setShortcut(QKeySequence("Up"));
        randomImageAction->setShortcut(QKeySequence("Shift+Ctrl+Right"));
        nextPickAction->setShortcut(QKeySequence("Alt+Right"));
        prevPickAction->setShortcut(QKeySequence("Alt+Left"));
        openAction->setShortcut(QKeySequence("O"));
        asLoupeAction->setShortcut(QKeySequence("E"));
        asGridAction->setShortcut(QKeySequence("G"));
        asTableAction->setShortcut(QKeySequence("T"));
        asCompareAction->setShortcut(QKeySequence("C"));
        revealFileAction->setShortcut(QKeySequence("Ctrl+R"));
        zoomOutAction->setShortcut(QKeySequence("-"));
        zoomInAction->setShortcut(QKeySequence("+"));
        zoomToggleAction->setShortcut(QKeySequence("Space"));
        zoomToAction->setShortcut(QKeySequence("Z"));
        rotateLeftAction->setShortcut(QKeySequence("Ctrl+["));
        rotateRightAction->setShortcut(QKeySequence("Ctrl+]"));
        ratingBadgeVisibleAction->setShortcut(QKeySequence("Ctrl+I"));
        infoVisibleAction->setShortcut(QKeySequence("I"));
        newWorkspaceAction->setShortcut(QKeySequence("W"));
        manageWorkspaceAction->setShortcut(QKeySequence("Ctrl+W"));
        defaultWorkspaceAction->setShortcut(QKeySequence("Ctrl+Shift+W"));
        folderDockVisibleAction->setShortcut(QKeySequence("F3"));
        favDockVisibleAction->setShortcut(QKeySequence("F4"));
        filterDockVisibleAction->setShortcut(QKeySequence("F5"));
        metadataDockVisibleAction->setShortcut(QKeySequence("F6"));
        thumbDockVisibleAction->setShortcut(QKeySequence("F7"));
//        windowTitleBarVisibleAction->setShortcut(QKeySequence("F8"));
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
    setCentralView();
    updateRawJpgStatus();
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
//    qDebug() << G::t.restart() << "\t" << "Floating";
    if (isFloat) {
        thumbView->setMaximumHeight(100000);
        thumbDock->setFeatures(QDockWidget::DockWidgetClosable |
                               QDockWidget::DockWidgetMovable  |
                               QDockWidget::DockWidgetFloatable);
        thumbsWrapAction->setChecked(true);
        thumbView->setWrapping(true);
        thumbView->isFloat = isFloat;
        thumbView->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    }
}

void MW::setThumbDockHeight()
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
    */
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
//    qDebug() << G::t.restart() << "\t" << "~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~MW::setThumbDockFeatures________________________________________";
    if(isInitializing) {
//        qDebug() << G::t.restart() << "\t" << "Still initializing - cancel setThumbDockFeatures";
//        return;
    }

    thumbView->setMaximumHeight(100000);

    /* Check if the thumbDock is docked top or bottom.  If so, set the titlebar
       to vertical and the thumbDock to accomodate the height of the thumbs. Set
       horizontal scrollbar on all the time (to simplify resizing dock and
       thumbs).  The vertical scrollbar depends on whether wrapping is checked
       in preferences.
    */
    if (area == Qt::BottomDockWidgetArea || area == Qt::TopDockWidgetArea || !thumbView->wrapThumbs)
    {
        thumbDock->setFeatures(QDockWidget::DockWidgetClosable |
                               QDockWidget::DockWidgetMovable  |
                               QDockWidget::DockWidgetFloatable |
                               QDockWidget::DockWidgetVerticalTitleBar);
        thumbsWrapAction->setChecked(false);
        thumbView->setWrapping(false);
//        qDebug() << G::t.restart() << "\t" << "MW::setThumbDockFeatures   thumbView->setWrapping(false);";
        thumbView->isTopOrBottomDock = true;
        // if thumbDock area changed then set dock height to thumb sizw
        if (!thumbDock->isFloating() && !asGridAction->isChecked()) {
            // make thumbDock height fit thumbs
            int maxHt = thumbView->getThumbSpaceMax();
            int minHt = thumbView->getThumbSpaceMin();
//            int scrollBarHeight = 12;
//            int scrollBarHeight = qApp->style()->pixelMetric(QStyle::PM_ScrollBarExtent);
//            int test = qApp->style()->pixelMetric(QStyle::PM_ScrollBarExtent);
//            qDebug() << G::t.restart() << "\t" << "qApp->style()->pixelMetric(QStyle::PM_ScrollBarExtent)" << test;

            int thumbCellHt = thumbView->getThumbCellSize().height();

            maxHt += G::scrollBarThickness;
            minHt += G::scrollBarThickness;
            int newThumbDockHeight = thumbCellHt + G::scrollBarThickness;
//            if (newThumbDockHeight > maxHt) newThumbDockHeight = maxHt;

            thumbView->setMaximumHeight(maxHt);
            thumbView->setMinimumHeight(minHt);

            thumbView->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
            resizeDocks({thumbDock}, {newThumbDockHeight}, Qt::Vertical);
/*
               qDebug() << G::t.restart() << "\t" << "\nMW::setThumbDockFeatures dock area =" << area << "\n"
                     << "***  thumbView Ht =" << thumbView->height()
                     << "thumbSpace Ht =" << thumbView->getThumbCellSize().height()
                     << "thumbHeight =" << thumbView->thumbHeight
                     << "newThumbDockHeight" << newThumbDockHeight
                     << "scrollBarHeight =" << G::scrollBarThickness << isScrollBar;
                     */
        }
    }
    /* must be docked left or right.  Turn horizontal scrollbars off.  Turn
       wrapping on
    */
    else {
        thumbDock->setFeatures(QDockWidget::DockWidgetClosable |
                               QDockWidget::DockWidgetMovable  |
                               QDockWidget::DockWidgetFloatable);
        thumbView->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        thumbsWrapAction->setChecked(true);
        thumbView->setWrapping(true);
        thumbView->isTopOrBottomDock = false;
    }
}

void MW::loupeDisplay()
{
/*
In the central widget show a loupe view of the image pointed to by the thumbView
currentindex.

Note: When the thumbDock thumbView is displayed it needs to be scrolled to the
currentIndex since it has been "hidden". However, the scrollbars take a long
time to paint after the view show event, so the ThumbView::scrollToCurrent
function must be delayed. This is done by the eventFilter in MW, intercepting
the scrollbar paint events. This is a bit of a cludge to get around lack of
notification when the QListView has finished painting itself.
*/
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
//    qDebug() << G::t.restart() << "\t" << "\n======================================= Loupe ===========================================";
//    qDebug() << G::t.restart() << "\t" << "Loupe: thumbView->isVisible()" << thumbView->isVisible() << "currentRow" << currentRow;

    G::mode = "Loupe";
    updateStatus(true);

    // save the current row as it will be lost when ThumbView becomes visible
    int previousRow = currentRow;

    // flag so can adjust index in fileSelectionChange as well
    modeChangeJustHappened = true;

    /* show imageView in the central widget. This makes thumbView visible, and
    it updates the index to its previous state.  The index update triggers
    fileSelectionChange  */
    centralLayout->setCurrentIndex(LoupeTab);
    prevCentralView = LoupeTab;
    modeChangeJustHappened = false;

    // recover the current index
    thumbView->setCurrentIndex(dm->sf->index(previousRow, 0));
    currentRow = previousRow;       // used by eventFilter in ThumbView

    // if was in grid mode then restore thumbdock to previous state
    if (hasGridBeenActivated) {
        if(!thumbDock->isVisible() && wasThumbDockVisible) toggleThumbDockVisibity();
        hasGridBeenActivated = false;
    }

    thumbView->setSelectionMode(QAbstractItemView::ExtendedSelection);

    // update imageView, use cache if image loaded, else read it from file
    QModelIndex idx = thumbView->currentIndex();
    QString fPath = idx.data(G::PathRole).toString();
    if (imageView->isVisible()) {
        if (imageView->loadImage(idx, fPath)) {
            if (G::isThreadTrackingOn) qDebug()
                << "MW::fileSelectionChange - loaded image file " << fPath;
            updateClassification();
//            updatePick();
//            updateRating();
//            updateColorClass();
        }
    }

    // req'd to show thumbs first time
    thumbView->setThumbParameters();

    /* flag to intercept scrollbar paint events in ThumbView::eventFilter and
    scroll to position when the painting is completed */
    thumbView->readyToScroll = true;

    prevMode = "Loupe";
}

void MW::gridDisplay()
{
/*
Note: When the gridView is displayed it needs to be scrolled to the
currentIndex since it has been "hidden". However, the scrollbars take a long
time to paint after the view show event, so the ThumbView::scrollToCurrent
function must be delayed. This is done by the eventFilter in ThumbView,
intercepted the scrollbar paint events. This is a bit of a cludge to get around
lack of notification when the QListView has finished painting itself.
*/
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
//    qDebug() << G::t.restart() << "\t" << "\n======================================= Grid ============================================";
    G::mode = "Grid";
    updateStatus(true);
    hasGridBeenActivated = true;
    // remember previous state of thumbDock so can recover if change mode
    wasThumbDockVisible = thumbDockVisibleAction->isChecked();
    // hide the thumbDock in grid mode as we don't need to see thumbs twice
    if(thumbDock->isVisible()) toggleThumbDockVisibity();
    // show tableView in central widget
    centralLayout->setCurrentIndex(GridTab);
    prevCentralView = GridTab;

    gridView->setThumbParameters();
    gridView->setSelectionMode(QAbstractItemView::ExtendedSelection);
    // limit time spent intercepting paint events to call scrollToCurrent
    gridView->readyToScroll = true;

    // if the zoom dialog was open then close it as no image visible to zoom
    emit closeZoomDlg();

    prevMode = "Grid";
}

void MW::tableDisplay()
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
//    qDebug() << G::t.restart() << "\t" << "\n======================================= Table===========================================";
    G::mode = "Table";
    updateStatus(true);

    // show tableView in central widget
    centralLayout->setCurrentIndex(TableTab);
    prevCentralView = TableTab;

    // if was in grid mode then restore thumbdock to previous state
    if (hasGridBeenActivated) {
        if(!thumbDock->isVisible() && wasThumbDockVisible) toggleThumbDockVisibity();
        hasGridBeenActivated = false;
    }

    thumbView->setThumbParameters();
    thumbView->setSelectionMode(QAbstractItemView::ExtendedSelection);

    tableView->scrollToCurrent();
    if (thumbView->isVisible()) thumbView->readyToScroll = true;

    // if the zoom dialog was open then close it as no image visible to zoom
    emit closeZoomDlg();
}

void MW::compareDisplay()
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
//    qDebug() << G::t.restart() << "\t" << "\n======================================= Compare =========================================";
    updateStatus(true);
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

    // if was in grid mode make sure thumbDock is visible in compare mode
    if (G::mode == "Grid") {
//        qDebug() << G::t.restart() << "\t" << "Calling setThumbDockFeatures from MW::MW::compareDisplay";
        setThumbDockFeatures(dockWidgetArea(thumbDock));
        if(!thumbDock->isVisible() && wasThumbDockVisible) toggleThumbDockVisibity();
        thumbView->setThumbParameters();  // reqd?
    }

    G::mode = "Compare";
    centralLayout->setCurrentIndex(CompareTab);
    prevCentralView = CompareTab;
    compareImages->load(centralWidget->size(), isRatingBadgeVisible);

    thumbView->setSelectionMode(QAbstractItemView::NoSelection);
//    thumbView->selectionModel()->clear();
}

void MW::saveSelection()
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    selectedImages = thumbView->selectionModel()->selectedIndexes();
    currentIdx = thumbView->selectionModel()->currentIndex();
}

void MW::recoverSelection()
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    QItemSelection *selection = new QItemSelection();
    thumbView->selectionModel()->clear();

    QModelIndex idx;
//    thumbView->setCurrentIndex(currentIdx);
    // sync thumbView delegate current item
//    thumbView->thumbViewDelegate->currentIndex = currentIdx;

    thumbView->selectionModel()->setCurrentIndex(currentIdx, QItemSelectionModel::Select);
    foreach (idx, selectedImages)
      if (idx != currentIdx) selection->select(idx, idx);

    thumbView->selectionModel()->select(*selection, QItemSelectionModel::Select);
    qDebug() << G::t.restart() << "\t" << "thumbView->selectionModel()->currentIndex()" << thumbView->selectionModel()->currentIndex();
    qDebug() << G::t.restart() << "\t" << "thumbView->selectionModel()->selectedIndexes()" << thumbView->selectionModel()->selectedIndexes();

}

void MW::setFullNormal()
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    if (fullScreenAction->isChecked()) showFullScreen();
    else showNormal();
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
}

void MW::selectShootingInfo()
{
    infoString->editTemplates();
    // display new info
    QModelIndex idx = thumbView->currentIndex();
    QString fPath = thumbView->getCurrentFilename();
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
    folderDock->setVisible(folderDockVisibleAction->isChecked());
}

void MW::setFavDockVisibility()
{
    favDock->setVisible(favDockVisibleAction->isChecked());
}

void MW::setFilterDockVisibility()
{
    filterDock->setVisible(filterDockVisibleAction->isChecked());
}

void MW::setMetadataDockVisibility()
{
    metadataDock->setVisible(metadataDockVisibleAction->isChecked());
}

void MW::setThumbDockVisibity()
{
    thumbDock->setVisible(thumbDockVisibleAction->isChecked());
}

void MW::toggleFolderDockVisibility() {
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    if (isInitializing) return;

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
    if (isInitializing) return;

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
    qDebug() << "favDockVisibleAction->isChecked()" << favDockVisibleAction->isChecked();
}

void MW::toggleFilterDockVisibility() {
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    if (isInitializing) return;

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
    if (isInitializing) return;

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
    if (isInitializing) return;

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
    }

    if (G::mode != "Grid")
        wasThumbDockVisible = thumbDockVisibleAction->isChecked();
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
    cacheLabel->setVisible(isShowCacheStatus);
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
    stateLabel->setText("    " + state + "    ");
}

void MW::toggleThumbWrap()
{
    thumbView->wrapThumbs = !thumbView->wrapThumbs;
    thumbsWrapAction->setChecked(thumbView->wrapThumbs);
    thumbView->setWrapping(thumbView->wrapThumbs);
}

void MW::togglePick()
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    qDebug() << "MW::togglePick()";
    QModelIndex idx;
    QModelIndexList idxList = thumbView->selectionModel()->selectedRows();
    QString pickStatus;

    /* If the selection has any images that are not picked then pick them all.
       If the entire selection was already picked then unpick them all.  If
       the entire selection is unpicked then pick them all.
    */
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
    dm->filterItemCount();
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
    qDebug() << "Pushed: " << pickStack;
}

void MW::popPick()
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    qDebug() << pickStack;
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
        dm->filterItemCount();
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
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    if (thumbView->isPick()) {
        ingestDlg = new IngestDlg(this,
                                  combineRawJpg,
                                  autoEjectUsb,
                                  metadata,
                                  dm,
                                  ingestRootFolder,
                                  pathTemplates,
                                  filenameTemplates,
                                  pathTemplateSelected,
                                  filenameTemplateSelected,
                                  ingestDescriptionCompleter,
                                  autoIngestFolderPath);
        connect(ingestDlg, SIGNAL(updateIngestParameters(QString,bool)),
                this, SLOT(setIngestRootFolder(QString,bool)));
        if(ingestDlg->exec()) ejectUsb(currentViewDir);;
        delete ingestDlg;
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
        driveRoot = "Untitled";
        dm->load(driveRoot, false);
        refreshFolders();
        int result = Usb::eject(driveRoot);
        if(result < 2)
            popUp->showPopup(this, "Ejecting drive " + driveRoot, 2000, 0.75);
        else
            popUp->showPopup(this, "Failed to eject drive " + driveRoot, 2000, 0.75);
    }
    else {
        popUp->showPopup(this, "Drive " + currentViewDir[0]
                + "is not removable and cannot be ejected", 2000, 0.75);
    }
    noFolderSelected();
}

void MW::ejectUsbFromContextMenu()
{
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

    // resize TableView columns to accomodate new types
    tableView->resizeColumnToContents(G::TypeColumn);

    // update status bar
    updateRawJpgStatus();

    // trigger update to image list and update image cache
    filterChange();
}

void MW::setCachedStatus(QString fPath, bool isCached)
{
/*
When an image is cached in ImageCache a signal triggers this slot to update the
datamodel cache status role.  After the datamodel update the thumbView and
gridView thumbnail is refreshed to update the cache badge.

Note that the datamodel is used (dm), not the proxy (dm->sf). If the proxy is
used and the user then sorts or filters the index could go out of range and the
app will crash.

Make sure the file path exists in the datamodel.  The most likely failure will
be if a new folder has been selected but the image cache has not yet redirected.
*/
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    QModelIndexList idxList = dm->match(dm->index(0, 0), G::PathRole, fPath);
    if (idxList.length()) {
        QModelIndex idx = idxList[0];
        if (idx.isValid()) {
            dm->setData(idx, isCached, G::CachedRole);
            thumbView->refreshThumb(idx, G::CachedRole);
            gridView->refreshThumb(idx, G::CachedRole);
        }
    }
}

void MW::updateClassification()
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
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
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__, "Completed");
    #endif
    }
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
    QObject* obj = sender();
    QString s = obj->objectName();
    if (s == "Rate0") rating = "";
    if (s == "Rate1") rating = "1";
    if (s == "Rate2") rating = "2";
    if (s == "Rate3") rating = "3";
    if (s == "Rate4") rating = "4";
    if (s == "Rate5") rating = "5";

    QModelIndexList selection = thumbView->selectionModel()->selectedRows();
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
        metadata->setRating(fPath, rating);

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
                metadata->setRating(fPath, rating);
            }
        }
    }

    // update metadata
    metadata->setRating(thumbView->getCurrentFilename(), rating);

    thumbView->refreshThumbs();
    gridView->refreshThumbs();

    // update ImageView classification badge
    updateClassification();

    // refresh the filter
    dm->sf->filterChange();

    // update filter counts
    dm->filterItemCount();
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
    QObject* obj = sender();
    QString s = obj->objectName();
    if (s == "Label0") colorClass = "";
    if (s == "Label1") colorClass = "Red";
    if (s == "Label2") colorClass = "Yellow";
    if (s == "Label3") colorClass = "Green";
    if (s == "Label4") colorClass = "Blue";
    if (s == "Label5") colorClass = "Purple";

    QModelIndexList selection = thumbView->selectionModel()->selectedRows();
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
        metadata->setLabel(fPath, colorClass);

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
                metadata->setLabel(fPath, colorClass);
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
    dm->filterItemCount();
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
    QString s = "infoView->isNewImageDataChange = ";
    s += infoView->isNewImageDataChange ? "true" : "false";
    G::track(__FUNCTION__, s);
    #endif
    }

    if (infoView->isNewImageDataChange) return;
    QModelIndex par = item->index().parent();
    if (par != infoView->tagInfoIdx) return;

    QString tagValue = item->data(Qt::DisplayRole).toString();
    QModelIndexList selection = thumbView->selectionModel()->selectedRows();
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
        if (tagName == "Title") metadata->setTitle(fPath, tagValue);
        if (tagName == "Creator") metadata->setCreator(fPath, tagValue);
        if (tagName == "Copyright") metadata->setCopyright(fPath, tagValue);
        if (tagName == "Email") metadata->setEmail(fPath, tagValue);
        if (tagName == "Url") metadata->setUrl(fPath, tagValue);
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
    qDebug() << G::t.restart() << "\t" << "Appending" << fPath;
    QDirIterator iterator(fPath, QDirIterator::Subdirectories);
    while (iterator.hasNext()) {
        iterator.next();
        fPath = iterator.filePath();
        if (iterator.fileInfo().isDir() && iterator.fileName() != "." && iterator.fileName() != "..") {
            subfolders->append(fPath);
            qDebug() << G::t.restart() << "\t" << "Appending" << fPath;
        }
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
    if (G::mode == "Loupe") thumbView->selectDown();
    if (G::mode == "Table") thumbView->selectDown();
    if (G::mode == "Grid") gridView->selectDown();
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
    else thumbView->selectFirst();
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
    else {
        thumbView->selectLast();
        gridView->setVisible(true);
        gridView->selectLast();
    }
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

void MW::setSlideShowParameters(int delay, bool isRandom)
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    slideShowDelay = delay;
    slideShowRandom = isRandom;
}

void MW::slideShow()
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
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
    G::track(__FUNCTION__);
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
//            qDebug() << G::t.restart() << "\t" << "STRESS TEST NEW FOLDER:" << fPath;
            folderSelectionChange();
        }
    }
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
    addBookmark(getSelectedPath());
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
    qDebug() << G::t.restart() << "\t" << "New folder is" << dirPath;
//    fsTree->setCurrentIndex(fsTree->fsFilter->mapFromSource(fsTree->fsModel->index(dirPath)));
    fsTree->select(dirPath);
    folderSelectionChange();
}

void MW::revealFile()
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
    QString path = thumbView->getCurrentFilename();
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
    qDebug() << "MW::collapseAllFolders";
    fsTree->collapseAll();
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

bool MW::isFolderValid(QString fPath, bool report, bool isRemembered)
{
    QDir testDir(fPath);
    QString msg;

    if (fPath.length() == 0) {
        if (report) {
//            qDebug() << G::t.restart() << "\t" << "MW::isFolderValid  fPath.length() == 0" << fPath;
//            QMessageBox::critical(this, tr("Error"), tr("The assigned folder name is blank"));
            msg = "The selected folder \"" + fPath + "\" does not have any images that can be shown in Winnow.";
            updateStatus(false, msg);
            setCentralMessage(msg);
        }
        return false;
    }

    if (!testDir.exists()) {
        if (report) {
//            QMessageBox::critical(this, tr("Error"), tr("The folder does not exist or is not available"));
            if (isRemembered)
                msg = "The last folder from your previous session (" + fPath + ") does not exist or is not available";
            else
                msg = "The folder (" + fPath + ") does not exist or is not available";

            updateStatus(false, msg);
            setCentralMessage(msg);
        }
        return false;
    }

    // check if unmounted USB drive
    if (!testDir.isReadable()) {
        if (report) {
            msg = "The folder (" + fPath + ") is not readable.  Perhaps it was a USB drive that is not currently mounted.";
            updateStatus(false, msg);
            setCentralMessage(msg);
        }
        return false;
    }

    return true;
}

void MW::createMessageView()
{
    messageView = new QWidget;
    msg.setupUi(messageView);
//    Ui::message ui;
//    ui.setupUi(messageView);
}

void MW::setCentralMessage(QString message)
{
    msg.msgLabel->setText(message);
    centralLayout->setCurrentIndex(MessageTab);
}

void MW::help()
{
    QWidget *helpDoc = new QWidget;
    Ui::helpForm ui;
    ui.setupUi(helpDoc);
    helpDoc->show();
}

void MW::helpShortcuts()
{
    QScrollArea *helpShortcuts = new QScrollArea;
    Ui::shortcutsForm ui;
    ui.setupUi(helpShortcuts);

    ui.treeWidget->setColumnWidth(0, 250);
    ui.treeWidget->setColumnWidth(1, 100);
    ui.treeWidget->setColumnWidth(2, 250);
    ui.treeWidget->header()->setMinimumHeight(20);
    ui.treeWidget->expandAll();
    QFile fStyle(":/qss/winnow.css");
    fStyle.open(QIODevice::ReadOnly);
    ui.scrollAreaWidgetContents->setStyleSheet(fStyle.readAll());
//    ui.scrollAreaWidgetContents->setStyleSheet("QTreeView::item { height: 20px;}");

    helpShortcuts->show();
}

void MW::helpWelcome()
{
    centralLayout->setCurrentIndex(StartTab);
}

void MW::test()
{
    // shortcut = "Shift+Ctrl+Alt+T"

//    qDebug() << "Attempting to eject drive " << currentViewDir[0] << "currentViewDir =" << currentViewDir
//             << "Unicode =" << currentViewDir[0].unicode();;
//    char driveLetter = currentViewDir[0].unicode();
//    if(Usb::isUsb(driveLetter)) {
//        QString driveRoot = currentViewDir.left(3);
//        dm->load(driveRoot, false);
//        refreshFolders();
//        bool result = Usb::eject(driveLetter);
//        qDebug() << "eject result =" << result;

//    }
//    else {
//        qDebug() << "Drive " << currentViewDir[0] << "is not removable and cannot be ejected";
//    }
//    return;


//    char driveLetter = 'P';
//    char devicepath[7];
//    char format[] = "\\\\.\\?:";
//    strcpy_s(devicepath, format);
//    devicepath[4] = driveLetter;
//    DWORD dwRet = 0;
//    wchar_t wtext[7];
//    size_t textlen = 7;
//    mbstowcs_s(&textlen, wtext, devicepath,strlen(devicepath)+1);

//    HANDLE handle = CreateFile(wtext, GENERIC_READ, FILE_SHARE_WRITE, 0, OPEN_EXISTING, 0, 0);
//    DWORD bytes = 0;
//    DeviceIoControl(handle, FSCTL_LOCK_VOLUME, 0, 0, 0, 0, &bytes, 0);
//    DeviceIoControl(handle, FSCTL_DISMOUNT_VOLUME, 0, 0, 0, 0, &bytes, 0);
//    DeviceIoControl(handle, IOCTL_STORAGE_EJECT_MEDIA, 0, 0, 0, 0, &bytes, 0);
//    CloseHandle(handle);

//    qDebug() << "Ejected ";

//    qDebug() << QImageReader::supportedImageFormats();
//    helpShortcuts();

//    metadata->reportMetadataCache(thumbView->getCurrentFilename());

    //    QString s = thumbView->getCurrentFilename();
//    qDebug() << "MW::test thumbView->getCurrentFilename() =" << s;

//    QLabel *label = new QLabel;
//    label->setText(" TEST ");
//    label->setStyleSheet("QLabel{color:yellow;}");
//    menuBar()->setCornerWidget(label, Qt::TopRightCorner);
}

// End MW
