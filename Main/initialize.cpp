#include "Main/mainwindow.h"

void MW::initialize()
{    
    if (G::isLogger) G::log("MW::initialize");
    //connect(windowHandle(), &QWindow::screenChanged, this, &MW::restoreLastSessionGeometryState);
    setWindowTitle(winnowWithVersion);
    G::stop = false;
    G::dmEmpty = true;
    G::isProcessingExportedImages = false;
    G::isInitializing = true;
    G::actDevicePixelRatio = 1;
    G::dpi = 72;
    G::ptToPx = G::dpi / 72;
    #ifdef  Q_OS_WIN
    G::ptToPx *= 1.2;
    #endif
    isNormalScreen = true;
    G::isSlideShow = false;
    stopped["MetaRead"] = true;
    stopped["MDCache"] = true;
    stopped["FrameDecoder"] = true;
    stopped["ImageCache"] = true;
    stopped["BuildFilters"] = true;
    workspaces = new QList<workspaceData>;
    recentFolders = new QStringList;
    ingestHistoryFolders = new QStringList;
    hasGridBeenActivated = true;

    // window
    isDragDrop = false;
    setAcceptDrops(true);
    setMouseTracking(true);
    QString msg;

    // status bar buttons / labels
    modifyImagesBtn = new BarBtn();
    msg = "Toggle permit modify images on/off.";
    modifyImagesBtn->setToolTip(msg);
    connect(modifyImagesBtn, &BarBtn::clicked, this, &MW::toggleModifyImagesClick);

    colorManageToggleBtn = new BarBtn();
    msg = "Toggle color manage on/off.";
    colorManageToggleBtn->setToolTip(msg);
    connect(colorManageToggleBtn, &BarBtn::clicked, this, &MW::toggleColorManageClick);

    cacheMethodBtn = new BarBtn();
    msg = "Toggle cache size Thrifty > Moderate > Greedy > Thrifty... \n"
          "Ctrl + Click to open cache preferences.";
    cacheMethodBtn->setToolTip(msg);
    connect(cacheMethodBtn, &BarBtn::clicked, this, &MW::toggleImageCacheStrategy);

    reverseSortBtn = new BarBtn();
    reverseSortBtn ->setToolTip("Toggle sort direction: Mouse click or shortcut Opt/Alt + S");
    connect(reverseSortBtn, &BarBtn::clicked, this, &MW::toggleSortDirectionClick);

    filterStatusLabel = new QLabel;
    filterStatusLabel->setToolTip("The images have been filtered");

    subfolderStatusLabel = new QLabel;
    subfolderStatusLabel->setToolTip("Showing contents of all subfolders");

    rawJpgStatusLabel = new QLabel;
    rawJpgStatusLabel->setToolTip("Raw and Jpg files are combined for viewing.  "
                                  "Toggle shortcut: Opt/Alt + J");
    slideShowStatusLabel = new QLabel;
    slideShowStatusLabel->setToolTip("Slideshow is active");
    slideCount = 0;

    pickClick = new QSoundEffect;
    QString path = "/Users/roryhill/Qt/6.7.0/Src/qtmultimedia/tests/auto/integration/qmediaplayerbackend/testdata/_test.wav";
    //pickClick->setSource(QUrl::fromLocalFile(path));
    path = "C:/Users/hillr/Downloads/click.wav";
    pickClick->setSource(QUrl::fromLocalFile(path));
    //pickClick->setSource(QUrl::fromLocalFile(":/Sounds/ingest.wav"));
    pickClick->setLoopCount(10);
    pickClick->setVolume(0.25);

//    pickClick = new QMediaPlayer;
//    audioOutput = new QAudioOutput;
//    pickClick->setAudioOutput(audioOutput);
//    pickClick->setSource(QUrl::fromLocalFile(":/Sounds/ingest.wav"));
//    audioOutput->setVolume(50);

    prevCentralView = 0;
    G::labelColors << "Red" << "Yellow" << "Green" << "Blue" << "Purple";
    G::ratings << "1" << "2" << "3" << "4" << "5";
    pickStack = new QStack<Pick>;
    slideshowRandomHistoryStack = new QStack<QString>;
    scrollRow = 0;
    sortColumn = G::NameColumn;
    isReverseSort = false;
}

void MW::setupPlatform()
{
    if (G::isLogger) G::log("MW::setupPlatform");
    #ifdef Q_OS_LINIX
        G::actDevicePixelRatio = 1;
    #endif
    #ifdef Q_OS_WIN
        setWindowIcon(QIcon(":/images/winnow.png"));
        Win::collectScreensInfo();
        Win::availableMemory();
        Win::setTitleBarColor(winId(), G::backgroundColor);
    #endif
    #ifdef Q_OS_MAC
        setWindowIcon(QIcon(":/images/winnow.icns"));
        Mac::availableMemory();
        Mac::joinAllSpaces(window()->winId());
    #endif
}

void MW::checkRecoveredGeometry(const QRect &availableGeometry, QRect *restoredGeometry,
                               int frameHeight)
{
    // compare with restored geometry's height increased by frameHeight
    const int height = restoredGeometry->height() + frameHeight;

    // Step 1: Resize if necessary:
    // make height / width 2px smaller than screen, because an exact match would be fullscreen
    if (availableGeometry.height() <= height)
        restoredGeometry->setHeight(availableGeometry.height() - 2 - frameHeight);
    if (availableGeometry.width() <= restoredGeometry->width())
        restoredGeometry->setWidth(availableGeometry.width() - 2);

    // Step 2: Move if necessary:
    // Construct a rectangle from restored Geometry adjusted by frameHeight
    const QRect restored = restoredGeometry->adjusted(0, -frameHeight, 0, 0);

    // Return if restoredGeometry (including frame) fits into screen
    if (availableGeometry.contains(restored))
        return;

    // (size is correct, but at least one edge is off screen)

    // Top out of bounds => move down
    if (restored.top() <= availableGeometry.top()) {
        restoredGeometry->moveTop(availableGeometry.top() + 1 + frameHeight);
    } else if (restored.bottom() >= availableGeometry.bottom()) {
        // Bottom out of bounds => move up
        restoredGeometry->moveBottom(availableGeometry.bottom() - 1);
    }

    // Left edge out of bounds => move right
    if (restored.left() <= availableGeometry.left()) {
        restoredGeometry->moveLeft(availableGeometry.left() + 1);
    } else if (restored.right() >= availableGeometry.right()) {
        // Right edge out of bounds => move left
        restoredGeometry->moveRight(availableGeometry.right() - 1);
    }
}

QRect MW::recoverGeometry(const QByteArray &geometry)
{
    QDataStream stream(geometry);
    stream.setVersion(QDataStream::Qt_4_0);

    const quint32 magicNumber = 0x1D9D0CB;
    quint32 storedMagicNumber;
    stream >> storedMagicNumber;
//    if (storedMagicNumber != magicNumber)
//        return false;

    const quint16 currentMajorVersion = 3;
    quint16 majorVersion = 0;
    quint16 minorVersion = 0;

    stream >> majorVersion >> minorVersion;

    QRect restoredFrameGeometry;
    QRect restoredGeometry;
    QRect restoredNormalGeometry;
    qint32 restoredScreenNumber;
    quint8 maximized;
    quint8 fullScreen;
    qint32 restoredScreenWidth = 0;

    stream >> restoredFrameGeometry // Only used for sanity checks in version 0
        >> restoredNormalGeometry
        >> restoredScreenNumber
        >> maximized
        >> fullScreen;

    if (majorVersion > 1)
        stream >> restoredScreenWidth;
    if (majorVersion > 2)
        stream >> restoredGeometry;
    return restoredGeometry;
}

void MW::createCentralWidget()
{
    if (G::isLogger) G::log("MW::createCentralWidget");
    // centralWidget required by ImageView/CompareView constructors
    centralWidget = new QWidget(this);
    centralWidget->setObjectName("centralWidget");
    // stack layout for loupe, table, compare and grid displays
    centralLayout = new QStackedLayout;
    centralLayout->setContentsMargins(0, 0, 0, 0);
}

void MW::setupCentralWidget()
{
    if (G::isLogger) G::log("MW::setupCentralWidget");
    welcome = new QScrollArea;
    Ui::welcomeScrollArea ui;
    ui.setupUi(welcome);

    centralLayout->addWidget(imageView);
    centralLayout->addWidget(videoView);
    centralLayout->addWidget(compareImages);
    centralLayout->addWidget(tableView);
    centralLayout->addWidget(gridView);
    centralLayout->addWidget(welcome);     // first time open program tips
    centralLayout->addWidget(messageView);
    centralLayout->addWidget(blankView);
    centralWidget->setLayout(centralLayout);
    setCentralWidget(centralWidget);
}

void MW::createFilterView()
{
    if (G::isLogger) G::log("MW::createFilterView");
    // G::fontSize req'd for row heights in filter view
//    if (setting->contains("fontSize"))
//        G::fontSize = setting->value("fontSize").toString();
//    else
//        G::fontSize = "12";
//    qDebug() << "MW::createFilterView  G::fontSize =" << G::fontSize;
    filters = new Filters(this);
    filters->setObjectName("Filters");
    filters->setMaximumWidth(folderMaxWidth);
//    qApp->installEventFilter(filters);

    /* Not using SIGNAL(itemChanged(QTreeWidgetItem*,int) because it triggers
       for every item in Filters */
    connect(filters, &Filters::filterChange, this, &MW::filterChange);
}

void MW::createDataModel()
{
    if (G::isLogger) G::log("MW::createDataModel");
    icd = new ImageCacheData(this);
    iconCacheData = new IconCacheData(this);
    metadata = new Metadata;
    cacheProgressBar = new ProgressBar(this);
    bool onTopOfCache = G::showProgress == G::ShowProgress::ImageCache;
    cacheProgressBar->setMetaProgressStyle(onTopOfCache);

    // loadSettings not run yet
    if (isSettings && settings->contains("combineRawJpg"))
        combineRawJpg = settings->value("combineRawJpg").toBool();
    else combineRawJpg = false;

    dm = new DataModel(this, metadata, filters, combineRawJpg);

    if (settings->contains("showThumbNailSymbolHelp"))
        dm->showThumbNailSymbolHelp = settings->value("showThumbNailSymbolHelp").toBool();
    else dm->showThumbNailSymbolHelp = true;

    connect(filters, &Filters::searchStringChange, dm, &DataModel::searchStringChange);
    connect(dm, &DataModel::updateClassification, this, &MW::updateClassification);
    connect(dm, &DataModel::centralMsg, this, &MW::setCentralMessage);
    connect(dm, &DataModel::updateStatus, this, &MW::updateStatus);
    connect(dm, &DataModel::updateProgress, filters, &Filters::updateProgress);
    connect(this, &MW::setValue, dm, &DataModel::setValue);
    connect(this, &MW::setValueSf, dm, &DataModel::setValueSf);
    connect(this, &MW::setIcon, dm, &DataModel::setIcon, Qt::BlockingQueuedConnection);

    buildFilters = new BuildFilters(this, dm, metadata, filters);

    connect(this, &MW::abortBuildFilters, buildFilters, &BuildFilters::stop);
    connect(buildFilters, &BuildFilters::addToDatamodel, dm, &DataModel::addMetadataForItem,
            Qt::BlockingQueuedConnection);
    connect(buildFilters, &BuildFilters::stopped, this, &MW::reset);
    connect(buildFilters, &BuildFilters::updateProgress, filters, &Filters::updateProgress);
    connect(buildFilters, &BuildFilters::finishedBuildFilters, filters, &Filters::finishedBuildFilters);
    connect(buildFilters, &BuildFilters::quickFilter, this, &MW::quickFilterComplete);
    connect(buildFilters, &BuildFilters::filterLastDay, this, &MW::filterLastDay);
    connect(buildFilters, &BuildFilters::searchTextEdit, this, &MW::searchTextEdit);
    connect(buildFilters, &BuildFilters::filterChange, this, &MW::filterChange);

    // test how to add to datamodel from another thread
    connect(this, &MW::testAddToDM, dm, &DataModel::addMetadataForItem);
}

void MW::createSelectionModel()
{
/*
    The application only has one selection model which is shared by ThumbView, GridView and
    TableView, insuring that each view is in sync, except when a view is hidden.
*/
    if (G::isLogger) G::log("MW::createSelectionModel");
    // set a common selection model for all views
//    selectionModel = new QItemSelectionModel(dm->sf);
    thumbView->setSelectionModel(dm->selectionModel);
    tableView->setSelectionModel(dm->selectionModel);
    gridView->setSelectionModel(dm->selectionModel);

    // when selection changes update views
}

void MW::createFrameDecoder()
{
/*
    Manage a number of FrameDecoder threads that send thumbnails to the DataModel.
*/
    if (G::isLogger) G::log("MW::createFrameDecoder");
    frameDecoder = new FrameDecoder(this);
    connect(this, &MW::abortFrameDecoder, frameDecoder, &FrameDecoder::stop);
    connect(frameDecoder, &FrameDecoder::stopped, this, &MW::reset);
    connect(frameDecoder, &FrameDecoder::setFrameIcon, dm, &DataModel::setIconFromVideoFrame/*,
            Qt::BlockingQueuedConnection*/);
    thumb = new Thumb(dm, metadata, frameDecoder);
}

void MW::createMDCache()
{
/*
    When a new folder is selected the metadataCacheThread is started to load all the
    metadata and thumbs for each image. If the user scrolls during the cache process then
    the metadataCacheThread is restarted at the first visible thumb to speed up the
    display of the thumbs for the user. However, if every scroll event triggered a
    restart it would be inefficient, so this timer is used to wait for a pause in the
    scrolling before triggering a restart at the new place.
*/
    if (G::isLogger) G::log("MW::createMDCache");
    metadataCacheThread = new MetadataCache(this, dm, metadata, frameDecoder);

    if (isSettings) {
        if (settings->contains("cacheAllMetadata")) metadataCacheThread->cacheAllMetadata = settings->value("cacheAllMetadata").toBool();
        if (settings->contains("cacheAllIcons")) metadataCacheThread->cacheAllIcons = settings->value("cacheAllIcons").toBool();
    }
    else {
        metadataCacheThread->cacheAllMetadata = true;
        metadataCacheThread->cacheAllIcons = false;
    }

    // not being used
    metadataCacheScrollTimer = new QTimer(this);
    metadataCacheScrollTimer->setSingleShot(true);
    // next connect to update
    connect(metadataCacheScrollTimer, &QTimer::timeout, this, &MW::loadMetadataChunk);

    // signal to stop MetaCache
    connect(this, &MW::abortMDCache, metadataCacheThread, &MetadataCache::stop);

    // signal stopped when abort completed
    connect(metadataCacheThread, &MetadataCache::stopped, this, &MW::reset);

    // add metadata to datamodel
    connect(metadataCacheThread, &MetadataCache::addToDatamodel, dm, &DataModel::addMetadataForItem,
            Qt::BlockingQueuedConnection);

    // update icon in datamodel
    connect(metadataCacheThread, &MetadataCache::setIcon, dm, &DataModel::setIcon,
            Qt::DirectConnection);

    connect(metadataCacheThread, &MetadataCache::updateIsRunning,
            this, &MW::updateMetadataThreadRunStatus);

    connect(metadataCacheThread, &MetadataCache::showCacheStatus,
            this, &MW::setCentralMessage);


//    connect(metadataCacheThread, &MetadataCache::showCacheStatus,
//            this, &MW::setCentralMessage, Qt::DirectConnection);

//    connect(metadataCacheThread, &MetadataCache::selectFirst,
//            thumbView, &IconView::selectFirst);
//    connect(metadataCacheThread, &MetadataCache::selectFirst,
//            sel, &Selection::first);

    // MetaRead and MetaRead2
//    if (isSettings) {
//        if (settings->contains("iconChunkSize")) {
//            dm->defaultIconChunkSize = settings->value("iconChunkSize").toInt();
//        }
//    }
//    else {
//        settings->setValue("iconChunkSize", dm->defaultIconChunkSize);
//    }

    dm->defaultIconChunkSize = 20000;

    dm->setChunkSize(dm->defaultIconChunkSize);

    /*
        Switching between MetaRead and MetaRead2:
            - METAREAD vs METAREAD2 in global.h
    */

#ifdef METAREAD
    // MetaRead
    // Runs a single thread to load metadata and thumbnails
    G::metaReadInUse = "Concurrent metadata and thumbnail loading";
    metaReadThread = new MetaRead(this, dm, metadata, frameDecoder);
    //metaReadThread->iconChunkSize = dm->iconChunkSize;
    metadataCacheThread->metadataChunkSize = dm->iconChunkSize;

    // signal to stop MetaRead
    connect(this, &MW::abortMetaRead, metaReadThread, &MetaRead::stop);
    // add metadata to datamodel
    connect(metaReadThread, &MetaRead::addToDatamodel, dm, &DataModel::addMetadataForItem,
            Qt::BlockingQueuedConnection);
    // add icon image to datamodel
    connect(metaReadThread, &MetaRead::setIcon, dm, &DataModel::setIcon,
            Qt::BlockingQueuedConnection);
    // add metadata to image cache item list
    connect(metaReadThread, &MetaRead::addToImageCache,
            imageCacheThread, &ImageCache::addCacheItemImageMetadata);
    // update thumbView in case scrolling has occurred
    connect(metaReadThread, &MetaRead::updateScroll, thumbView, &IconView::repaintView,
            Qt::BlockingQueuedConnection);
    // update gridView in case scrolling has occurred
    connect(metaReadThread, &MetaRead::updateScroll, gridView, &IconView::repaintView,
            Qt::BlockingQueuedConnection);
    // message metadata reading completed
    connect(metaReadThread, &MetaRead::done, this, &MW::loadConcurrentDone);
    // Signal to change selection, fileSelectionChange, update ImageCache
    connect(metaReadThread, &MetaRead::fileSelectionChange, this, &MW::fileSelectionChange);
    // update statusbar metadata active light
    connect(metaReadThread, &MetaRead::runStatus, this, &MW::updateMetadataThreadRunStatus);
    // update loading metadata in central window
    connect(metaReadThread, &MetaRead::centralMsg, this, &MW::setCentralMessage);
    // update filters metaread progress
    connect(metaReadThread, &MetaRead::updateProgressInFilter, filters, &Filters::updateProgress);
    // update loading metadata in statusbar
    connect(metaReadThread, &MetaRead::updateProgressInStatusbar,
            cacheProgressBar, &ProgressBar::updateMetadataCacheProgress);
#endif

#ifdef METAREAD2
    // MetaRead2
    // Runs multiple reader threads to load metadata and thumbnails
    G::metaReadInUse = "Concurrent multi-threaded metadata and thumbnail loading";
    metaReadThread = new MetaRead2(this, dm, metadata, frameDecoder, imageCacheThread);
    // metaReadThread->iconChunkSize = dm->iconChunkSize;
    metadataCacheThread->metadataChunkSize = dm->iconChunkSize;

    // signal to stop MetaRead2
    connect(this, &MW::abortMetaRead, metaReadThread, &MetaRead2::stop);

    // update thumbView in case scrolling has occurred
    connect(metaReadThread, &MetaRead2::updateScroll, thumbView, &IconView::repaintView,
            Qt::BlockingQueuedConnection);
    // update gridView in case scrolling has occurred
    connect(metaReadThread, &MetaRead2::updateScroll, gridView, &IconView::repaintView,
            Qt::BlockingQueuedConnection);
    // loading image metadata into datamodel, okay to select
    connect(metaReadThread, &MetaRead2::okToSelect, sel, &Selection::okToSelect);
    // message metadata reading completed
    connect(metaReadThread, &MetaRead2::done, this, &MW::loadConcurrentDone);
    // Signal to change selection, fileSelectionChange, update ImageCache
    connect(metaReadThread, &MetaRead2::fileSelectionChange, this, &MW::fileSelectionChange);
    // update statusbar metadata active light
    connect(metaReadThread, &MetaRead2::runStatus, this, &MW::updateMetadataThreadRunStatus);
    // update loading metadata in central window
    connect(metaReadThread, &MetaRead2::centralMsg, this, &MW::setCentralMessage);
    // update filters MetaRead2 progress
    connect(metaReadThread, &MetaRead2::updateProgressInFilter, filters, &Filters::updateProgress);
    // update loading metadata in statusbar
    connect(metaReadThread, &MetaRead2::updateProgressInStatusbar,
            cacheProgressBar, &ProgressBar::updateMetadataCacheProgress);

    // not being used:
    // read metadata
    //connect(this, &MW::startMetaRead, metaReadThread, &MetaRead2::setCurrentRow);
    // pause waits until isRunning == false
    //connect(this, &MW::interruptMetaRead, metaReadThread, &MetaRead2::interrupt);
#endif // end of MetaRead2

#ifdef METAREAD3
    // MetaRead3
    // Runs multiple reader threads to load metadata and thumbnails
    G::metaReadInUse = "Concurrent multi-threaded metadata and thumbnail loading";
    metaReadThread = new MetaRead3(this, dm, metadata, frameDecoder, imageCacheThread);
    // metaReadThread->iconChunkSize = dm->iconChunkSize;
    metadataCacheThread->metadataChunkSize = dm->iconChunkSize;

    // signal to stop MetaRead3
    connect(this, &MW::abortMetaRead, metaReadThread, &MetaRead3::stop);

    // update thumbView in case scrolling has occurred
    connect(metaReadThread, &MetaRead3::updateScroll, thumbView, &IconView::repaintView,
            Qt::BlockingQueuedConnection);
    // update gridView in case scrolling has occurred
    connect(metaReadThread, &MetaRead3::updateScroll, gridView, &IconView::repaintView,
            Qt::BlockingQueuedConnection);
    // loading image metadata into datamodel, okay to select
    connect(metaReadThread, &MetaRead3::okToSelect, sel, &Selection::okToSelect);
    // message metadata reading completed
    connect(metaReadThread, &MetaRead3::done, this, &MW::loadConcurrentDone);
    // Signal to change selection, fileSelectionChange, update ImageCache
    connect(metaReadThread, &MetaRead3::fileSelectionChange, this, &MW::fileSelectionChange);
    // update statusbar metadata active light
    connect(metaReadThread, &MetaRead3::runStatus, this, &MW::updateMetadataThreadRunStatus);
    // update loading metadata in central window
    connect(metaReadThread, &MetaRead3::centralMsg, this, &MW::setCentralMessage);
    // update filters MetaRead3 progress
    connect(metaReadThread, &MetaRead3::updateProgressInFilter, filters, &Filters::updateProgress);
    // update loading metadata in statusbar
    connect(metaReadThread, &MetaRead3::updateProgressInStatusbar,
            cacheProgressBar, &ProgressBar::updateMetadataCacheProgress);

// not being used:
// read metadata
//connect(this, &MW::startMetaRead, metaReadThread, &MetaRead3::setCurrentRow);
// pause waits until isRunning == false
//connect(this, &MW::interruptMetaRead, metaReadThread, &MetaRead3::interrupt);
#endif // end of MetaRead3

}

void MW::createImageCache()
{
    if (G::isLogger) G::log("MW::createImageCache");

    /* When a new folder is selected the metadataCacheThread is started to load all the
    metadata and thumbs for each image. If the user scrolls during the cache process then the
    metadataCacheThread is restarted at the first visible thumb to speed up the display of the
    thumbs for the user. However, if every scroll event triggered a restart it would be
    inefficient, so this timer is used to wait for a pause in the scrolling before triggering
    a restart at the new place.
    */

//    icd = new ImageCacheData(this);
    imageCacheThread = new ImageCache(this, icd, dm);

    /* Image caching is triggered from the metadataCacheThread to avoid the two threads
       running simultaneously and colliding */

    // LINEAR LOAD PROCESS CONNECTIONS

    // load image cache for a new folder
//    connect(metadataCacheThread, SIGNAL(loadImageCache()),
//            this, SLOT(loadImageCacheForNewFolder()));
//    connect(metadataCacheThread, &MetadataCache::loadImageCache,
//            this, &MW::loadImageCacheForNewFolder);

    connect(imageCacheThread, SIGNAL(updateIsRunning(bool,bool)),
            this, SLOT(updateImageCachingThreadRunStatus(bool,bool)));

    // signal to stop the ImageCache
    connect(this, &MW::abortImageCache, imageCacheThread, &ImageCache::stop);

    // signal stopped when abort completed
    connect(imageCacheThread, &ImageCache::stopped, this, &MW::reset);

    // Update the cache status progress bar when changed in ImageCache
    connect(imageCacheThread, &ImageCache::showCacheStatus,
            this, &MW::updateImageCacheStatus);

    // Signal from ImageCache::run() to update cache status in datamodel
    connect(imageCacheThread, &ImageCache::updateCacheOnThumbs,
            this, &MW::updateCachedStatus);

    // Signal from ImageCache::run() update central view
    connect(imageCacheThread, &ImageCache::imageCachePrevCentralView,
            this, &MW::imageCachePrevCentralView);

    // Signal to ImageCache new image selection
    connect(this, &MW::setImageCachePosition,
            imageCacheThread, &ImageCache::setCurrentPosition);

    // Send message to setCentralMsg
    connect(imageCacheThread, &ImageCache::centralMsg,
            this, &MW::setCentralMessage);

    // set values in the datamodel
    connect(imageCacheThread, &ImageCache::setValue, dm, &DataModel::setValue);
    connect(imageCacheThread, &ImageCache::setValueSf, dm, &DataModel::setValueSf);
    connect(imageCacheThread, &ImageCache::setValuePath, dm, &DataModel::setValuePath);

    // start image cache
    connect(infoView, &InfoView::setCurrentPosition,
            imageCacheThread, &ImageCache::setCurrentPosition);
    // add to image cache list
//    connect(infoView, &InfoView::addToImageCache,
//            imageCacheThread, &ImageCache::addCacheItemImageMetadata);

    // signal ImageCache refresh
    connect(this, &MW::refreshImageCache, imageCacheThread, &ImageCache::refreshImageCache);
//    connect(metaRead, &MetaRead::setImageCachePosition,
//            imageCacheThread, &ImageCache::setCurrentPosition);

}

void MW::createThumbView()
{
    if (G::isLogger) G::log("MW::createThumbView");
    thumbView = new IconView(this, dm, "Thumbnails");
    thumbView->setObjectName("Thumbnails");
//    thumbView->setSpacing(0);                // thumbView not visible without this
    thumbView->setAutoScroll(false);
    thumbView->firstVisibleCell = 0;
    thumbView->showZoomFrame = true;            // may have settings but not showZoomFrame yet
    if (isSettings) {
        // loadSettings has not run yet (dependencies, but QSettings has been opened
        if (settings->contains("thumbWidth")) thumbView->iconWidth = settings->value("thumbWidth").toInt();
        if (settings->contains("thumbHeight")) thumbView->iconHeight = settings->value("thumbHeight").toInt();
        if (settings->contains("labelFontSize")) thumbView->labelFontSize = settings->value("labelFontSize").toInt();
        if (settings->contains("showThumbLabels")) thumbView->showIconLabels = settings->value("showThumbLabels").toBool();
        if (settings->contains("labelChoice")) thumbView->labelChoice = settings->value("labelChoice").toString();
        if (settings->contains("showZoomFrame")) thumbView->showZoomFrame = settings->value("showZoomFrame").toBool();
        if (settings->contains("classificationBadgeSizeFactor")) thumbView->badgeSize = settings->value("classificationBadgeSizeFactor").toInt();
        if (settings->contains("iconNumberSize")) thumbView->iconNumberSize = settings->value("iconNumberSize").toInt();
    }
    else {
        thumbView->iconWidth = 100;
        thumbView->iconHeight = 100;
        thumbView->labelFontSize = 12;
        thumbView->showIconLabels = false;
        thumbView->showZoomFrame = true;
        thumbView->badgeSize = 13;
        thumbView->iconNumberSize = 24;
    }
    // double mouse click fires displayLoupe
    connect(thumbView, SIGNAL(displayLoupe()), this, SLOT(loupeDisplay()));

    // back and forward mouse buttons toggle pick
//    connect(thumbView, &IconView::togglePick, this, &MW::togglePick);

    // scrolling
    connect(thumbView->verticalScrollBar(), SIGNAL(valueChanged(int)),
            this, SLOT(thumbHasScrolled()));
    connect(thumbView->horizontalScrollBar(), SIGNAL(valueChanged(int)),
            this, SLOT(thumbHasScrolled()));
}

void MW::createGridView()
{
    if (G::isLogger) G::log("MW::createGridView");
    gridView = new IconView(this, dm, "Grid");
    gridView->setObjectName("Grid");
    gridView->setSpacing(0);                // gridView not visible without this
    gridView->setWrapping(true);
    gridView->setAutoScroll(false);
    gridView->firstVisibleCell = 0;

    if (isSettings) {
        if (settings->contains("thumbWidthGrid")) gridView->iconWidth = settings->value("thumbWidthGrid").toInt();
        if (settings->contains("thumbHeightGrid")) gridView->iconHeight = settings->value("thumbHeightGrid").toInt();
        if (settings->contains("labelFontSizeGrid")) gridView->labelFontSize = settings->value("labelFontSizeGrid").toInt();
        if (settings->contains("showThumbLabelsGrid")) gridView->showIconLabels = settings->value("showThumbLabelsGrid").toBool();
        if (settings->contains("labelChoice")) gridView->labelChoice = settings->value("labelChoice").toString();
        if (settings->contains("classificationBadgeSizeFactor")) gridView->badgeSize = settings->value("classificationBadgeSizeFactor").toInt();
        if (settings->contains("iconNumberSize")) gridView->iconNumberSize = settings->value("iconNumberSize").toInt();
    }
    else {
        gridView->iconWidth = 200;
        gridView->iconHeight = 200;
        gridView->labelFontSize = 10;
        gridView->showIconLabels = true;
        gridView->badgeSize = classificationBadgeSizeFactor;
        gridView->iconNumberSize = iconNumberSize;
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
    if (G::isLogger) G::log("MW::createTableView");
    tableView = new TableView(this, dm);
    tableView->setAutoScroll(false);

    if (isSettings) {
        /* read TableView okToShow fields */
        settings->beginGroup("TableFields");
        QStringList setFields = settings->childKeys();
        QList<QStandardItem *> itemList;
        setFields = settings->childKeys();
        for (int i = 0; i <setFields.size(); ++i) {
            QString setField = setFields.at(i);
            bool okToShow = settings->value(setField).toBool();
            itemList = tableView->ok->findItems(setField);
            if (itemList.length()) {
                int row = itemList[0]->row();
                QModelIndex idx = tableView->ok->index(row, 1);
                tableView->ok->setData(idx, okToShow, Qt::EditRole);
            }
        }
        settings->endGroup();
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

void MW::createSelection()
{
    if (G::isLogger) G::log("MW::createSelection");
    sel = new Selection(this, dm, thumbView, gridView, tableView);
    connect(sel, &Selection::fileSelectionChange, this, &MW::fileSelectionChange);
    connect(sel, &Selection::loadConcurrent, this, &MW::loadConcurrent);
    connect(sel->sm, &QItemSelectionModel::selectionChanged, sel, &Selection::selectionChanged);
    connect(sel, &Selection::updateStatus, this, &MW::updateStatus);
    connect(sel, &Selection::updateCurrent, dm, &DataModel::setCurrent);
}

void MW::createVideoView()
{
    if (G::isLogger) G::log("MW::createVideoView");
    if (!G::useMultimedia) return;

    videoView = new VideoView(this, thumbView, sel);

    // back and forward mouse buttons toggle pick
    connect(videoView, &VideoView::togglePick, this, &MW::togglePick);
    // drop event
    connect(videoView, &VideoView::handleDrop, this, &MW::handleDrop);
    // show mouse cursor
    connect(videoView, &VideoView::showMouseCursor, this, &MW::showMouseCursor);
    // hide mouse cursor
    connect(videoView, &VideoView::hideMouseCursor, this, &MW::hideMouseCursor);
}

void MW::createImageView()
{
/*
    ImageView displays the image in the central widget. The image is either from the image
    cache or read from the file if the cache is unavailable. Creation is dependent on
    metadata, imageCacheThread, thumbView, datamodel and settings.
*/
    if (G::isLogger) G::log("MW::createImageView");
    /* This is the info displayed on top of the image in loupe view. It is
       dependent on template data stored in QSettings */
    // start with defaults
    infoString->loupeInfoTemplate = "Default info";
    if (isSettings) {
        // load info templates
        settings->beginGroup("InfoTemplates");
        QStringList keys = settings->childKeys();
        for (int i = 0; i < keys.size(); ++i) {
            QString key = keys.at(i);
            infoString->infoTemplates[key] = settings->value(key).toString();
        }
        settings->endGroup();
        // if loupeInfoTemplate is in QSettings and info templates then assign
        if (settings->contains("loupeInfoTemplate")) {
            QString displayInfoTemplate = settings->value("loupeInfoTemplate").toString();
            if (infoString->infoTemplates.contains(displayInfoTemplate))
                infoString->loupeInfoTemplate = displayInfoTemplate;
        }
    }

    // prep pass values: first use of program vs settings have been saved
    if (isSettings) {
        if (settings->contains("isImageInfoVisible")) isImageInfoVisible = settings->value("isImageInfoVisible").toBool();
        if (settings->contains("infoOverlayFontSize")) infoOverlayFontSize = settings->value("infoOverlayFontSize").toInt();
    }
    else {
        // parameters already defined in loadSettings
    }

    imageView = new ImageView(this,
                              centralWidget,
                              metadata,
                              dm,
                              icd,
                              sel,
                              thumbView,
                              infoString,
                              settings->value("isImageInfoVisible").toBool(),
                              settings->value("isRatingBadgeVisible").toBool(),
                              settings->value("classificationBadgeInImageDiameter").toInt(),
                              settings->value("infoOverlayFontSize").toInt());

    if (isSettings) {
        if (settings->contains("limitFit100Pct")) imageView->limitFit100Pct = settings->value("limitFit100Pct").toBool();
        if (settings->contains("infoOverlayFontSize")) imageView->infoOverlayFontSize = settings->value("infoOverlayFontSize").toInt();
        if (settings->contains("lastPrefPage")) lastPrefPage = settings->value("lastPrefPage").toInt();
        qreal tempZoom = settings->value("toggleZoomValue").toReal();
        if (tempZoom > 3) tempZoom = 1.0;
        if (tempZoom < 0.25) tempZoom = 1.0;
        imageView->toggleZoom = tempZoom;
    }
    else {
        imageView->limitFit100Pct = true;
        imageView->toggleZoom = 1.0;
        imageView->infoOverlayFontSize = infoOverlayFontSize;   // defined in loadSettings
    }

    connect(imageView, &ImageView::tryAgain, this, &MW::tryLoadImageAgain);
    connect(imageView, &ImageView::togglePick, this, &MW::togglePick);
    connect(imageView, &ImageView::updateStatus, this, &MW::updateStatus);
    connect(imageView, &ImageView::setCentralMessage, this, &MW::setCentralMessage);
    connect(thumbView, &IconView::thumbClick, imageView, &ImageView::thumbClick);
    connect(imageView, &ImageView::handleDrop, this, &MW::handleDrop);
    connect(imageView, &ImageView::killSlideshow, this, &MW::slideShow);
    connect(imageView, &ImageView::keyPress, this, &MW::keyPressEvent);
    connect(imageCacheThread, &ImageCache::loadImage, imageView, &ImageView::loadImage);
}

void MW::createCompareView()
{
    if (G::isLogger) G::log("MW::createCompareView");
    compareImages = new CompareImages(this, centralWidget, metadata, dm, sel, thumbView, icd);

    if (isSettings) {
        if (settings->contains("lastPrefPage")) lastPrefPage = settings->value("lastPrefPage").toInt();
        qreal tempZoom = settings->value("toggleZoomValue").toReal();
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
    if (G::isLogger) G::log("MW::createInfoString");
    infoString = new InfoString(this, dm, settings/*, embelProperties*/);

}

void MW::createInfoView()
{
/*
    InfoView shows basic metadata in a dock widget.
*/
    if (!G::useInfoView) return;
    if (G::isLogger) G::log("MW::createInfoView");
    infoView = new InfoView(this, dm, metadata, thumbView, filters, buildFilters);
    infoView->setMaximumWidth(folderMaxWidth);

    if (isSettings) {
        /* read InfoView okToShow fields */
        settings->beginGroup("InfoFields");
        QStringList setFields = settings->childKeys();
        QList<QStandardItem *> itemList;
        QStandardItemModel *k = infoView->ok;
        // go through every setting in QSettings
        bool isFound;
        for (int i = 0; i < setFields.size(); ++i) {
            isFound = false;
            // Get a field and boolean
            QString setField = setFields.at(i);
            bool okToShow = settings->value(setField).toBool();
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
        settings->endGroup();
    }

    /* read InfoView okToShow fields */
//    qDebug() << G::t.restart() << "\t" << "\nread InfoView okToShow fields\n";
    settings->beginGroup("InfoFields");
    QStringList setFields = settings->childKeys();
    QList<QStandardItem *> itemList;
    QStandardItemModel *k = infoView->ok;
    // go through every setting in QSettings
    bool isFound;
    for (int i = 0; i < setFields.size(); ++i) {
        isFound = false;
        // Get a field and boolean
        QString setField = setFields.at(i);
        bool okToShow = settings->value(setField).toBool();
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
    settings->endGroup();

    connect(infoView->ok, SIGNAL(itemChanged(QStandardItem*)),
            this, SLOT(metadataChanged(QStandardItem*)));
    // update filters
    connect(infoView, &InfoView::updateFilter, buildFilters, &BuildFilters::updateCategory);
    connect(infoView, &InfoView::filterChange, this, &MW::filterChange);
}

void MW::createEmbel()
{
    if (G::isLogger) G::log("MW::createEmbel");
    QString src = "Internal";
    embel = new Embel(imageView->scene, imageView->pmItem, embelProperties, dm, icd);
    connect(imageView, &ImageView::embellish, embel, &Embel::build);
    connect(embel, &Embel::done, imageView, &ImageView::resetFitZoom);
    if (G::useInfoView) {
        connect(infoView, &InfoView::dataEdited, embel, &Embel::refreshTexts);
    }
}

void MW::createFSTree()
{
    if (G::isLogger) G::log("MW::createFSTree");
    // loadSettings has not run yet (dependencies, but QSettings has been opened
    fsTree = new FSTree(this, metadata);
    fsTree->setMaximumWidth(folderMaxWidth);
    fsTree->setShowImageCount(true);
    fsTree->combineRawJpg = combineRawJpg;

    // watch volumes for ejection / mounting
    #ifdef Q_OS_WIN
    WinNativeEventFilter *wnef = new WinNativeEventFilter(this);
    connect(wnef, &WinNativeEventFilter::refreshFileSystem, fsTree, &FSTree::refreshModel);
    #endif

    // this works for touchpad tap
    connect(fsTree, &FSTree::pressed, this, &MW::folderSelectionChangeNoParam);

    // reselect folder after external program drop onto FSTree
    connect(fsTree, &FSTree::folderSelection, this, &MW::folderSelectionChange);

    // if move drag and drop then delete files from source folder(s)
    connect(fsTree, &FSTree::deleteFiles, this, &MW::deleteFiles);

    // rename menu item "Eject USB drive <x>" and enable/disable
    connect(fsTree, &FSTree::renameEjectAction, this, &MW::renameEjectUsbMenu);

    // rename menu item "Erase mem card images" and enable/disable for context menu only
    connect(fsTree, &FSTree::renameEraseMemCardContextAction, this, &MW::renameEraseMemCardFromContextMenu);

    // rename menu item "Paste files" and enable/disable for context menu only
    connect(fsTree, &FSTree::renamePasteContextAction, this, &MW::renamePasteFilesMenu);

    // watch for drive removal (not working)
//    connect(fsTree->watch, &QFileSystemWatcher::directoryChanged, this, &MW::checkDirState);
    // this does not work to detect ejecting a drive
//    connect(fsTree->fsModel, SIGNAL(rowsRemoved(const QModelIndex &, int, int)),
//            this, SLOT(checkDirState(const QModelIndex &, int, int)));

//    connect(fsTree, SIGNAL(dropOp(Qt::KeyboardModifiers, bool, QString)),
//            this, SLOT(dropOp(Qt::KeyboardModifiers, bool, QString)));
}

void MW::createBookmarks()
{
    if (G::isLogger) G::log("MW::createBookmarks");
    bookmarks = new BookMarks(this, metadata, true /*showImageCount*/, combineRawJpg);

    if (isSettings) {
        settings->beginGroup("Bookmarks");
        QStringList paths = settings->childKeys();
        for (int i = 0; i < paths.size(); ++i) {
            bookmarks->bookmarkPaths.insert(settings->value(paths.at(i)).toString());
        }
        bookmarks->reloadBookmarks();
        settings->endGroup();
    }
    else {
        bookmarks->bookmarkPaths.insert(QDir::homePath());
    }

    bookmarks->setMaximumWidth(folderMaxWidth);

    // this does work for touchpad tap
    // triggers MW::bookmarkClicked > fsTree sync > MW::folderSelectionChange
    connect(bookmarks, SIGNAL(itemPressed(QTreeWidgetItem *, int)),
            this, SLOT(bookmarkClicked(QTreeWidgetItem *, int)));

    // update folder image counts
    connect(fsTree->fsModel, &FSModel::update, bookmarks, &BookMarks::update);

    connect(bookmarks, SIGNAL(dropOp(Qt::KeyboardModifiers, bool, QString)),
            this, SLOT(dropOp(Qt::KeyboardModifiers, bool, QString)));

    // if move drag and drop then delete files from source folder(s)
    connect(bookmarks, &BookMarks::deleteFiles, this, &MW::deleteFiles);

    // refresh FSTree count after drag and drop to BookMarks
    connect(bookmarks, &BookMarks::refreshFSTree, fsTree, &FSTree::refreshModel);

    // reselect folder after external program drop onto BookMarks
    connect(bookmarks, &BookMarks::folderSelection, this, &MW::folderSelectionChange);

    // rename menu item "Eject USB drive <x>" and enable/disable
    connect(bookmarks, &BookMarks::renameEjectAction, this, &MW::renameEjectUsbMenu);

    // rename menu item "Erase mem card images" and enable/disable for context menu only
    connect(bookmarks, &BookMarks::renameEraseMemCardContextAction, this, &MW::renameEraseMemCardFromContextMenu);
}

void MW::createAppStyle()
{
    if (G::isLogger) G::log("MW::createAppStyle");
    widgetCSS.fontSize = G::strFontSize.toInt();
    int bg = G::backgroundShade;
    widgetCSS.widgetBackgroundColor = QColor(bg,bg,bg);
    css = widgetCSS.css();
    G::css = css;
    this->setStyleSheet(css);
    return;
}

void MW::createStatusBar()
{
    if (G::isLogger) G::log("MW::createStatusBar");
    statusBar()->setObjectName("WinnowStatusBar");
    statusBar()->setStyleSheet("QStatusBar::item { border: none; };");

    // cache status on right side of status bar

    // label to hold QPixmap showing progress
    progressLabel = new QLabel();
    progressLabel->setObjectName("StatusProgressLabel");
    progressLabel->setStyleSheet("QToolTip {"
                                 "opacity: 200;"         // nada windows
                                 "color: #ffffff;"       // nada windows
//                                 "background-color: blue;"
//                                   "border-width: 1px;"
//                                   "border-style: solid;"
//                                   "border-color: " + QColor(l5,l5,l5).name() + ";"
//                                   "margin: 2px;"
//                                   "font-size:" + QString::number(fontSize) + "pt;"
                                   "}");

    // progressBar created in MW::createDataModel, where it is first req'd

    // set up pixmap that shows progress in the cache
    if (isSettings && settings->contains("cacheStatusWidth") && G::isRory)
        cacheBarProgressWidth = settings->value("cacheStatusWidth").toInt();
    else cacheBarProgressWidth = 100;
    if (cacheBarProgressWidth < 100 || cacheBarProgressWidth > 1000) cacheBarProgressWidth = 200;
    progressPixmap = new QPixmap(4000, 25);   // cacheprogress
    progressPixmap->scaled(cacheBarProgressWidth, 25);
    progressPixmap->fill(widgetCSS.widgetBackgroundColor);
    progressLabel->setFixedWidth(cacheBarProgressWidth);
    progressLabel->setPixmap(*progressPixmap);

    // tooltip
    QString progressToolTip = "Image cache status for current folder:\n";
    progressToolTip += "  • LightGray:  \tbackground for all images in folder\n";
    progressToolTip += "  • DarkGray:   \timages targeted to be cached\n";
    progressToolTip += "  • Green:      \timages that are cached\n";
    progressToolTip += "  • LightGreen: \tcurrent image";
    progressToolTip += "\n\nMouse click on cache status progress to open cache preferences.";
    progressLabel->setToolTip(progressToolTip);
    progressLabel->setToolTipDuration(100000);
    statusBar()->addPermanentWidget(progressLabel, 1);

    // end progressbar

    statusBarSpacer = new QLabel;
    statusBarSpacer->setText(" ");
    statusBar()->addPermanentWidget(statusBarSpacer);

    // label to show metadataThreadRunning status
    int runLabelWidth = 13;
    //metadataThreadRunningLabel = new QLabel;
    // sets tooltip
    setCacheMethod(cacheMethod);
    metadataThreadRunningLabel->setFixedWidth(runLabelWidth);
    updateMetadataThreadRunStatus(false, true, "MW::createStatusBar");
    statusBar()->addPermanentWidget(metadataThreadRunningLabel);

    // label to show imageThreadRunning status
    imageThreadRunningLabel = new QLabel;
    statusBar()->addPermanentWidget(imageThreadRunningLabel);
    QString itrl = "Turns red when image caching in progress";
    imageThreadRunningLabel->setToolTip(itrl);
    imageThreadRunningLabel->setFixedWidth(runLabelWidth);

    // label to show cache amount
    //    setImageCacheSize(cacheSizeStrategy);
    //    statusBar()->addPermanentWidget(cacheMethodBtn);
    //    cacheMethodBtn->show();

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
    statusBarSpacer1 = new QLabel;
    statusBarSpacer1->setPixmap(QPixmap(1,5));
    statusBar()->addWidget(statusBarSpacer1);
    statusBar()->addWidget(modifyImagesBtn);
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

void MW::createFolderDock()
{
    if (G::isLogger) G::log("MW::createFolderDock");
    folderDockTabText = "  📁  ";
    QPixmap pm(":/images/icon16/anchor.png");
    folderDockTabRichText = "test";
//    folderDockTabRichText = Utilities::pixmapToString(pm);
    dockTextNames << folderDockTabText;
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
    folderRefreshBtn->setToolTip("Refresh folders and image counts");
    //folderRefreshBtn->setStyleSheet("QToolTip { color: red;}");
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
        settings->beginGroup(("FolderDock"));
        if (settings->contains("screen")) folderDock->dw.screen = settings->value("screen").toInt();
        if (settings->contains("pos")) folderDock->dw.pos = settings->value("pos").toPoint();
        if (settings->contains("size")) folderDock->dw.size = settings->value("size").toSize();
        if (settings->contains("devicePixelRatio")) folderDock->dw.devicePixelRatio = settings->value("devicePixelRatio").toReal();
        settings->endGroup();
    }

    connect(folderDock, &QDockWidget::visibilityChanged, this, &MW::folderDockVisibilityChange);
}

void MW::createFavDock()
{
    if (G::isLogger) G::log("MW::createFavDock");
    favDockTabText = "  🔖  ";
    dockTextNames << favDockTabText;
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
    favRefreshBtn->setToolTip("Refresh bookmarks and image counts");
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
        settings->beginGroup(("FavDock"));
        if (settings->contains("screen")) favDock->dw.screen = settings->value("screen").toInt();
        if (settings->contains("pos")) favDock->dw.pos = settings->value("pos").toPoint();
        if (settings->contains("size")) favDock->dw.size = settings->value("size").toSize();
        if (settings->contains("devicePixelRatio")) favDock->dw.devicePixelRatio = settings->value("devicePixelRatio").toReal();
        settings->endGroup();
    }
}

void MW::createFilterDock()
{
    if (G::isLogger) G::log("MW::createFilterDock");
    filterDockTabText = "  🤏  ";
    dockTextNames << filterDockTabText;
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
    BarBtn *updateFiltersBtn = new BarBtn();
    updateFiltersBtn->setIcon(QIcon(":/images/icon16/refresh.png"));
    updateFiltersBtn->setToolTip("Update filters");
//    connect(updateFiltersBtn, &BarBtn::clicked, buildFilters, &BuildFilters::build);
    filterTitleLayout->addWidget(updateFiltersBtn);

    // Spacer
    filterTitleLayout->addSpacing(5);

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
    filterTitleLayout->addSpacing(5);

    // question mark button
    BarBtn *filterQuestionBtn = new BarBtn();
    filterQuestionBtn->setIcon(QIcon(":/images/icon16/questionmark.png"));
    filterQuestionBtn->setToolTip("How this works");
    connect(filterQuestionBtn, &BarBtn::clicked, filters, &Filters::howThisWorks);
    filterTitleLayout->addWidget(filterQuestionBtn);

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

    if (isSettings) {
        settings->beginGroup(("FilterDock"));
        if (settings->contains("screen")) filterDock->dw.screen = settings->value("screen").toInt();
        if (settings->contains("pos")) filterDock->dw.pos = settings->value("pos").toPoint();
        if (settings->contains("size")) filterDock->dw.size = settings->value("size").toSize();
        if (settings->contains("devicePixelRatio")) filterDock->dw.devicePixelRatio = settings->value("devicePixelRatio").toReal();
        settings->endGroup();
    }
}

void MW::createMetadataDock()
{
    if (!G::useInfoView) return;
    if (G::isLogger) G::log("MW::createMetadataDock");
    // this does not work
//    QPixmap pixmap(":/images/icon16/anchor.png");
//    metadataDockTabText = Utilities::pixmapToString(pixmap);
    metadataDockTabText = "  📷  ";
    dockTextNames << metadataDockTabText;
    metadataDock = new DockWidget(metadataDockTabText, this);    // Metadata
    metadataDock->setObjectName("ImageInfo");
    metadataDock->setWidget(infoView);

    /* Experimenting touse rich text in QTabWidget for docks
    RichTextTabBar *tabBar = new RichTextTabBar();
    RichTextTabWidget *tabWidget = new RichTextTabWidget(metadataDock);
    tabWidget->setRichTextTabBar(tabBar);
    metadataDock->setTabIcon
    //*/

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
        settings->beginGroup(("MetadataDock"));
        if (settings->contains("screen")) metadataDock->dw.screen = settings->value("screen").toInt();
        if (settings->contains("pos")) metadataDock->dw.pos = settings->value("pos").toPoint();
        if (settings->contains("size")) metadataDock->dw.size = settings->value("size").toSize();
        if (settings->contains("devicePixelRatio")) metadataDock->dw.devicePixelRatio = settings->value("devicePixelRatio").toReal();
        settings->endGroup();
    }
}

void MW::createThumbDock()
{
    if (G::isLogger) G::log("MW::createThumbDock");
    thumbDockTabText = "Thumbnails";
    dockTextNames << thumbDockTabText;
    thumbDock = new DockWidget(thumbDockTabText, this);  // Thumbnails
    thumbDock->setObjectName("thumbDock");
    thumbDock->setWidget(thumbView);
    thumbDock->installEventFilter(this);

    if (isSettings) {
        settings->beginGroup(("ThumbDockFloat"));
        if (settings->contains("screen")) thumbDock->dw.screen = settings->value("screen").toInt();
        if (settings->contains("pos")) thumbDock->dw.pos = settings->value("pos").toPoint();
        if (settings->contains("size")) thumbDock->dw.size = settings->value("size").toSize();
        if (settings->contains("devicePixelRatio")) thumbDock->dw.devicePixelRatio = settings->value("devicePixelRatio").toReal();
        settings->endGroup();
    }
//    else thumbDock->dw.size = QSize(600, 600);

    connect(thumbDock, SIGNAL(dockLocationChanged(Qt::DockWidgetArea)),
            this, SLOT(setThumbDockFeatures(Qt::DockWidgetArea)));

    connect(thumbDock, SIGNAL(topLevelChanged(bool)),
            this, SLOT(setThumbDockFloatFeatures(bool)));
}

void MW::createEmbelDock()
{
    if (G::isLogger) G::log("MW::createEmbelDock");
    embelProperties = new EmbelProperties(this, settings);

    connect (embelProperties, &EmbelProperties::templateChanged, this, &MW::embelTemplateChange);
    connect (embelProperties, &EmbelProperties::centralMsg, this, &MW::setCentralMessage);
    connect (embelProperties, &EmbelProperties::syncEmbellishMenu, this, &MW::syncEmbellishMenu);

    embelDockTabText = "  🎨  ";
    dockTextNames << embelDockTabText;
    embelDock = new DockWidget(embelDockTabText, this);  // Embellish
    embelDock->setObjectName("EmbelDock");
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
    embelTitleBar = new DockTitleBar("Embellish Editor", embelTitleLayout);
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
    if (G::isLogger) G::log("MW::createDocks");
    createFolderDock();
    createFavDock();
    createFilterDock();
    if (G::useInfoView) createMetadataDock();
    createThumbDock();
    createEmbelDock();

    connect(this, &MW::tabifiedDockWidgetActivated, this, &MW::embelDockActivated);

    addDockWidget(Qt::LeftDockWidgetArea, folderDock);
    addDockWidget(Qt::LeftDockWidgetArea, favDock);
    addDockWidget(Qt::LeftDockWidgetArea, filterDock);
    if (G::useInfoView) addDockWidget(Qt::LeftDockWidgetArea, metadataDock);
    addDockWidget(Qt::LeftDockWidgetArea, thumbDock);
    if (!hideEmbellish) addDockWidget(Qt::RightDockWidgetArea, embelDock);

    MW::setTabPosition(Qt::LeftDockWidgetArea, QTabWidget::North);
    MW::setTabPosition(Qt::RightDockWidgetArea, QTabWidget::North);
    MW::tabifyDockWidget(folderDock, favDock);
    MW::tabifyDockWidget(favDock, filterDock);
    if (G::useInfoView) MW::tabifyDockWidget(filterDock, metadataDock);
    if (!hideEmbellish) if (G::useInfoView) MW::tabifyDockWidget(metadataDock, embelDock);
}

void MW::createMessageView()
{
    if (G::isLogger) G::log("MW::createMessageView");
    messageView = new QWidget;
    msg.setupUi(messageView);
//    Ui::message ui;
//    ui.setupUi(messageView);
}

