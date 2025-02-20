#include "Main/mainwindow.h"

void MW::initialize()
{
    if (G::isLogger || G::isFlowLogger) G::log("MW::initialize");
    // connect(this, &QWindow::windowStateChanged, this, &MW::onWindowStateChanged);

    setWindowTitle(winnowWithVersion);
    G::stop = false;
    G::removingFolderFromDM = false;
    G::isProcessingExportedImages = false;
    G::isInitializing = true;
    G::actDevicePixelRatio = 1;
    G::dpi = 72;
    G::ptToPx = G::dpi / 72;
    #ifdef  Q_OS_WIN
    G::ptToPx *= 1.2;
    #endif
    G::isSlideShow = false;
    stopped["MetaRead"] = true;
    stopped["MDCache"] = true;
    stopped["FrameDecoder"] = true;
    stopped["ImageCache"] = true;
    stopped["BuildFilters"] = true;
    workspaces = new QList<WorkspaceData>;
    recentFolders = new QStringList;
    ingestHistoryFolders = new QStringList;
    hasGridBeenActivated = true;

    // drag text
    dragLabel->setWindowFlags(Qt::FramelessWindowHint | Qt::Tool | Qt::WindowStaysOnTopHint);
    dragLabel->setAttribute(Qt::WA_TranslucentBackground);
    dragLabel->setStyleSheet("background: rgba(0, 0, 0, 150); color: white; padding: 5px; border-radius: 5px;");
    dragLabel->hide();

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

    includeSidecarsToggleBtn = new BarBtn();
    msg = "Toggle include sidecars when dragging on/off.";
    includeSidecarsToggleBtn->setToolTip(msg);
    connect(includeSidecarsToggleBtn, &BarBtn::clicked, this, &MW::toggleIncludeSidecarsClick);

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

    QString path = ":/Sounds/ingest.wav";
    // pickClick = new QSoundEffect();
    pickClick->setSource(QUrl::fromLocalFile(path));
    pickClick->setLoopCount(10);
    pickClick->setVolume(pickClickvolume);   // default value overridden in loadSettings

    prevCentralView = 0;
    G::labelColors << "Red" << "Yellow" << "Green" << "Blue" << "Purple";
    G::ratings << "1" << "2" << "3" << "4" << "5";
    pickStack = new QStack<Pick>;
    slideshowRandomHistoryStack = new QStack<QString>;
    scrollRow = 0;
    sortColumn = G::NameColumn;
    isReverseSort = false;

    // Initialize G::relay
    G::relay = new G::SignalRelay(this);
    // Connect the relay signal to the updateStatus slot
    connect(G::relay, &G::SignalRelay::updateStatus, this, &MW::updateStatus);
    connect(G::relay, &G::SignalRelay::showPopUp, this, &MW::showPopUp);

    // Temp until resolve tiff decoding crash issues with some compression methods
    #ifdef  Q_OS_WIN
    G::useMyTiff = true;
    #endif

    // prevent some warnings from Qt
    QLoggingCategory::setFilterRules(QStringLiteral("qt.gui.imageio.jpeg.debug=false\nqt.gui.imageio.jpeg.warning=false"));
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
        G::trash = "Recycle Bin";
    #endif
    #ifdef Q_OS_MAC
        setWindowIcon(QIcon(":/images/winnow.icns"));
        Mac::availableMemory();
        Mac::joinAllSpaces(window()->winId());
        G::trash = "Trash";
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

    centralLayout->addWidget(imageView);        // 0
    centralLayout->addWidget(videoView);        // 1
    centralLayout->addWidget(compareImages);    // 2
    centralLayout->addWidget(tableView);        // 3
    centralLayout->addWidget(gridView);         // 4
    centralLayout->addWidget(welcome);          // 5 first time open program tips
    centralLayout->addWidget(messageView);      // 6
    centralLayout->addWidget(blankView);        // 7
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
    // iconCacheData = new IconCacheData(this);
    metadata = new Metadata;
    cacheProgressBar = new ProgressBar(this);
    // bool onTopOfCache = G::showProgress == G::ShowProgress::ImageCache;
    // cacheProgressBar->setMetaProgressStyle(onTopOfCache);
    cacheProgressBar->setMetaProgressStyle(true);

    // loadSettings not run yet
    if (isSettings && settings->contains("combineRawJpg"))
        combineRawJpg = settings->value("combineRawJpg").toBool();
    else combineRawJpg = false;

    dm = new DataModel(nullptr, metadata, filters, combineRawJpg);
    // dm = new DataModel(this, metadata, filters, combineRawJpg);
    // enable global access to datamodel
    G::setDM(dm);

    if (settings->contains("showThumbNailSymbolHelp"))
        dm->showThumbNailSymbolHelp = settings->value("showThumbNailSymbolHelp").toBool();
    else dm->showThumbNailSymbolHelp = true;

    connect(dm, &DataModel::stop, this, &MW::stop, Qt::BlockingQueuedConnection);
    connect(dm, &DataModel::done, this, &MW::loadDone);
    connect(dm, &DataModel::addedFolderToDM, this, &MW::loadChanged);
    connect(dm, &DataModel::removedFolderFromDM, this, &MW::loadChanged);
    connect(filters, &Filters::searchStringChange, dm, &DataModel::searchStringChange);
    connect(dm, &DataModel::updateClassification, this, &MW::updateClassification);
    connect(dm, &DataModel::centralMsg, this, &MW::setCentralMessage);
    connect(dm, &DataModel::updateStatus, this, &MW::updateStatus);
    connect(dm, &DataModel::updateProgress, filters, &Filters::updateProgress);
    connect(this, &MW::updateCurrent, dm, &DataModel::setCurrentSF);
    connect(this, &MW::setValueDm, dm, &DataModel::setValueDm);
    connect(this, &MW::setValueSf, dm, &DataModel::setValueSf);
    connect(this, &MW::setValueSf, dm, &DataModel::setValueSf);
    connect(this, &MW::setIcon, dm, &DataModel::setIcon, Qt::BlockingQueuedConnection);

    buildFilters = new BuildFilters(this, dm, metadata, filters);

    connect(this, &MW::abortBuildFilters, buildFilters, &BuildFilters::stop);
    connect(buildFilters, &BuildFilters::addToDatamodel, dm, &DataModel::addMetadataForItem,
            Qt::BlockingQueuedConnection);
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
    connect(frameDecoder, &FrameDecoder::setFrameIcon, dm, &DataModel::setIconFromVideoFrame);
    thumb = new Thumb(dm, metadata, frameDecoder);
}

void MW::createMDCache()
{
/*
    When a new folder is selected the metadataReadThread is started to load all the
    metadata and thumbs for each image. If the user scrolls during the cache process then
    the metadataReadThread is restarted at the first visible thumb to speed up the
    display of the thumbs for the user.
*/
    if (G::isLogger) G::log("MW::createMDCache");

    // MetaRead
    if (isSettings) {
       if (settings->contains("iconChunkSize")) {
           dm->defaultIconChunkSize = settings->value("iconChunkSize").toInt();
       }
    }
    else {
       settings->setValue("iconChunkSize", dm->defaultIconChunkSize);
    }

    // dm->defaultIconChunkSize = 200;
    // dm->defaultIconChunkSize = G::maxIconChunk;

    dm->setChunkSize(dm->defaultIconChunkSize);

    // Runs multiple reader threads to load metadata and thumbnails
    metaRead = new MetaRead(this, dm, metadata, frameDecoder, imageCache);
    // metaReadThread->iconChunkSize = dm->iconChunkSize;
    // metadataCacheThread->metadataChunkSize = dm->iconChunkSize;

    // signal to stop MetaRead
    connect(this, &MW::abortMetaRead, metaRead, &MetaRead::stop);

    // update thumbView in case scrolling has occurred
    connect(metaRead, &MetaRead::updateScroll, thumbView, &IconView::repaintView,
            Qt::BlockingQueuedConnection);
    // update gridView in case scrolling has occurred
    connect(metaRead, &MetaRead::updateScroll, gridView, &IconView::repaintView,
            Qt::BlockingQueuedConnection);
    // loading image metadata into datamodel, okay to select
    connect(metaRead, &MetaRead::okToSelect, sel, &Selection::okToSelect);
    // message metadata reading completed
    connect(metaRead, &MetaRead::done, this, &MW::loadDone);
    // Signal to change selection, fileSelectionChange, update ImageCache
    connect(metaRead, &MetaRead::fileSelectionChange, this, &MW::fileSelectionChange);
    // update statusbar metadata active light
    connect(metaRead, &MetaRead::runStatus, this, &MW::updateMetadataThreadRunStatus);
    // update loading metadata in central window
    connect(metaRead, &MetaRead::centralMsg, this, &MW::setCentralMessage);
    // update filters MetaRead progress
    connect(metaRead, &MetaRead::updateProgressInFilter, filters, &Filters::updateProgress);
    // update loading metadata in statusbar
    connect(metaRead, &MetaRead::updateProgressInStatusbar,
            cacheProgressBar, &ProgressBar::updateMetadataCacheProgress);
    // save time to read image metadata and icon to the datamodel
    connect(metaRead, &MetaRead::setMsToRead, dm, &DataModel::setValueDm);
    // reset imagecache targets after folder added or removed from datamodel
    connect(metaRead, &MetaRead::dispatchIsFinished,
            imageCache, &ImageCache::datamodelFolderCountChange);

}

void MW::createImageCache()
{
    if (G::isLogger) G::log("MW::createImageCache");

    /* When a new folder is selected the metadataReadThread is started to load all the
    metadata and thumbs for each image. If the user scrolls during the cache process then the
    metadataReadThread is restarted at the first visible thumb to speed up the display of the
    thumbs for the user. However, if every scroll event triggered a restart it would be
    inefficient, so this timer is used to wait for a pause in the scrolling before triggering
    a restart at the new place.
    */

    imageCache = new ImageCache(this, icd, dm);
    // imageCache->moveToThread(&imageCacheThread);

    /* Image caching is triggered from the metadataReadThread to avoid the two threads
       running simultaneously and colliding */

    connect(&imageCacheThread, &QThread::finished,
            imageCache, &QObject::deleteLater);

    // Signal to ImageCache start
    connect(this, &MW::startImageCache,
            imageCache, &ImageCache::start);

    // Signal to stop (quit) the ImageCache
    connect(this, &MW::stopImageCache,
            imageCache, &ImageCache::stop);

    // Signal to abort the ImageCache
    connect(this, &MW::abortImageCache,
            imageCache, &ImageCache::abortProcessing);

    // Signal to ImageCache filterChange
    connect(this, &MW::imageCacheFilterChange,
            imageCache, &ImageCache::filterChange);

    // Signal to ImageCache color manage change
    connect(this, &MW::imageCacheColorManageChange,
            imageCache, &ImageCache::colorManageChange);

    // Signal to initialize ImageCache
    connect(this, &MW::initImageCache,
            imageCache, &ImageCache::initImageCache);

    // Signal to update imageCache parameters
    connect(this, &MW::imageCacheChangeParam,
            imageCache, &ImageCache::updateImageCacheParam);

    // Signal to ImageCache new image selection
    connect(this, &MW::setImageCachePosition,
            imageCache, &ImageCache::setCurrentPosition);

    connect(imageCache, &ImageCache::updateIsRunning,
            this, &MW::updateImageCachingThreadRunStatus);

    // // signal stopped when abort completed
    // connect(imageCacheThread, &ImageCache::stopped, this, &MW::reset);

    // Update the cache status progress bar when changed in ImageCache
    connect(imageCache, &ImageCache::showCacheStatus,
            this, &MW::updateImageCacheStatus);

    // Signal from ImageCache::run() to update cache status in datamodel
    connect(imageCache, &ImageCache::refreshViews,
            this, &MW::refreshViewsOnCacheChange);

    // Send message to setCentralMsg
    connect(imageCache, &ImageCache::centralMsg,
            this, &MW::setCentralMessage);

    // set values in the datamodel
    connect(imageCache, &ImageCache::setValueSf,
            dm, &DataModel::setValueSf);

    imageCache->start();
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
    // connect(thumbView, SIGNAL(displayLoupe()), this, SLOT(loupeDisplay()));
    // connect(thumbView, &IconView::displayLoupe, this, &MW::loupeDisplay);

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

    // double mouse click fires displayLoupe (signal not req'd)
    // connect(gridView, &IconView::displayLoupe, this, &MW::loupeDisplay);
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
    connect(sel, &Selection::loadConcurrent, this, &MW::load);
    connect(sel->sm, &QItemSelectionModel::selectionChanged, sel, &Selection::selectionChanged);
    connect(sel, &Selection::updateStatus, this, &MW::updateStatus);
    connect(sel, &Selection::updateCurrent, dm, &DataModel::setCurrentSF);
    connect(sel, &Selection::updateMissingThumbnails, this, &MW::renameEmbedThumbsContextMenu);
}

void MW::createVideoView()
{
    if (G::isLogger) G::log("MW::createVideoView");
    if (!G::useMultimedia) return;

    videoView = new VideoView(this, thumbView, sel);

    // back and forward mouse buttons toggle pick
    connect(videoView, &VideoView::togglePick, this, &MW::togglePick);
    connect(videoView, &VideoView::mouseSideKeyPress, this, &MW::mouseSideKeyPress);
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
    connect(imageView, &ImageView::mouseSideKeyPress, this, &MW::mouseSideKeyPress);
    // connect(imageCache, &ImageCache::loadImage, imageView, &ImageView::loadImage);
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
    infoString = new InfoString(this, dm, settings);

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
    fsTree = new FSTree(this, dm, metadata);
    fsTree->setMaximumWidth(folderMaxWidth);
    fsTree->setShowImageCount(true);
    fsTree->combineRawJpg = combineRawJpg;

    // watch folders for external deletion
    connect(&folderWatcher, &DirWatcher::folderDeleted, this, &MW::currentFolderDeletedExternally);

    // watch volumes for ejection / mounting
    #ifdef Q_OS_WIN
    WinNativeEventFilter *wnef = new WinNativeEventFilter(this);
    connect(wnef, &WinNativeEventFilter::refreshFileSystem, fsTree, &FSTree::refreshModel);
    #endif

    // this works for touchpad tap (replaced with folderSelection)
    // connect(fsTree, &FSTree::pressed, this, &MW::folderSelectionChangeNoParam);

    // add/remove folder to the DataModel processing queue
    // connect(fsTree, &FSTree::addToDataModel, this, &MW::loadConcurrentAddFolder);
    // connect(fsTree, &FSTree::removeFromDataModel, this, &MW::loadConcurrentRemoveFolder);
    // connect(fsTree, &FSTree::datamodelQueue, dm, &DataModel::enqueueFolderSelection);

    // reselect folder after external program drop onto FSTree or a selectionChange
    // connect(fsTree, &FSTree::folderSelection, this, &MW::folderSelectionChange);

    // select a folder including modifier keys
    connect(fsTree, &FSTree::folderSelectionChange, this, &MW::folderSelectionChange);

    // refresh datamodel after dragdrop operation
    connect(fsTree, &FSTree::refreshDataModel, this, &MW::refreshDataModel);

    // if move drag and drop then delete files from source folder(s)
    connect(fsTree, &FSTree::deleteFiles, this, &MW::deleteFiles);

    // rename menu item "Eject USB drive <x>" and enable/disable
    connect(fsTree, &FSTree::renameEjectAction, this, &MW::renameEjectUsbMenu);

    // rename menu item "Erase mem card images" and enable/disable for context menu only
    connect(fsTree, &FSTree::renameEraseMemCardContextAction, this, &MW::renameEraseMemCardFromContextMenu);

    // rename menu item "Copy folder path" and enable/disable for context menu only
    connect(fsTree, &FSTree::renameCopyFolderPathAction, this, &MW::renameCopyFolderPathAction);

    // rename menu item "Reveal in finder" and enable/disable for context menu only
    connect(fsTree, &FSTree::renameRevealFileAction, this, &MW::renameRevealFileAction);

    // add menu item "Add Bookmark folder name"
    // connect(fsTree, &FSTree::addBookmarkAction, this, &MW::addBookmarkAction);

    // rename menu item "Paste files" and enable/disable for context menu only
    connect(fsTree, &FSTree::renamePasteContextAction, this, &MW::renamePasteFilesAction);

    // rename menu item "Move folder to trash/recycle bin" and enable/disable for context menu only
    connect(fsTree, &FSTree::renameDeleteFolderAction, this, &MW::renameDeleteFolderAction);

    // update status in status bar
    connect(fsTree, &FSTree::status, this, &MW::updateStatus);

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
    bookmarks = new BookMarks(this, dm, metadata, true /*showImageCount*/, combineRawJpg);

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
    // qDebug() << "END CREATE BOOKMARKS)";

    bookmarks->setMaximumWidth(folderMaxWidth);

    // refresh datamodel after dragdrop operation
    connect(bookmarks, &BookMarks::refreshDataModel, this, &MW::refreshDataModel);

    // this works for touchpad tap
    // triggers MW::bookmarkClicked > fsTree sync > MW::folderSelectionChange
    connect(bookmarks, &BookMarks::itemPressed, this, &MW::bookmarkClicked);

    // update folder image counts
    connect(fsTree->fsModel, &FSModel::update, bookmarks, &BookMarks::updateBookmarks);

    connect(bookmarks, SIGNAL(dropOp(Qt::KeyboardModifiers, bool, QString)),
            this, SLOT(dropOp(Qt::KeyboardModifiers, bool, QString)));

    // if move drag and drop then delete files from source folder(s)
    connect(bookmarks, &BookMarks::deleteFiles, this, &MW::deleteFiles);

    // refresh FSTree count after drag and drop to BookMarks
    connect(bookmarks, &BookMarks::refreshFSTree, fsTree, &FSTree::refreshModel);

    // reselect folder after external program drop onto BookMarks
    connect(bookmarks, &BookMarks::folderSelection, fsTree, &FSTree::select);

    // rename menu item "Eject USB drive <x>" and enable/disable
    connect(bookmarks, &BookMarks::renameEjectAction, this, &MW::renameEjectUsbMenu);

    // rename menu item "Erase mem card images" and enable/disable for context menu only
    connect(bookmarks, &BookMarks::renameEraseMemCardContextAction, this, &MW::renameEraseMemCardFromContextMenu);

    // rename menu item "Remove Bookmark folder name"
    connect(bookmarks, &BookMarks::renameRemoveBookmarkAction, this, &MW::renameRemoveBookmarkAction);

    // rename menu item "Copy folder path" and enable/disable for context menu only
    connect(bookmarks, &BookMarks::renameCopyFolderPathAction, this, &MW::renameCopyFolderPathAction);

    // rename menu item "Reveal in finder" and enable/disable for context menu only
    connect(bookmarks, &BookMarks::renameRevealFileAction, this, &MW::renameRevealFileAction);

    // update status in status bar
    connect(bookmarks, &BookMarks::status, this, &MW::updateStatus);

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
                                 "}");

    // progressBar created in MW::createDataModel, where it is first req'd

    // set up pixmap that shows progress in the cache
    if (isSettings && settings->contains("cacheStatusWidth"))
        cacheBarProgressWidth = settings->value("cacheStatusWidth").toInt();
    else cacheBarProgressWidth = 100;
    if (cacheBarProgressWidth < 100 || cacheBarProgressWidth > 1000) cacheBarProgressWidth = 200;
    progressPixmap = new QPixmap(4000, 25);   // cacheprogress
    progressPixmap->scaled(cacheBarProgressWidth, 25);
    progressPixmap->fill(widgetCSS.widgetBackgroundColor);
    progressLabel->setFixedWidth(cacheBarProgressWidth);
    progressLabel->setPixmap(*progressPixmap);

    // progress tooltip
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
    metadataThreadRunningLabel->setFixedWidth(runLabelWidth);
    updateMetadataThreadRunStatus(false, true, "MW::createStatusBar");
    statusBar()->addPermanentWidget(metadataThreadRunningLabel);
    QString tip = "Metadata and Icon caching:\n";
    tip += "\n";
    tip += "  • Green:    \tAll cached\n";
    tip += "  • Red:      \tCaching in progress\n";
    tip += "  • Orange:   \tCaching had a hickup\n";
    metadataThreadRunningLabel->setToolTip(tip);

    // label to show imageThreadRunning status
    imageThreadRunningLabel = new QLabel;
    statusBar()->addPermanentWidget(imageThreadRunningLabel);
    tip = "Image caching:\n";
    tip += "\n";
    tip += "  • Green:    \tAll cached\n";
    tip += "  • Red:      \tCaching in progress\n";
    tip += "  • Gray:     \tEmpty folder, no images to cache\n";
    tip += "\n";
    tip += "Click to go to cache preferences.";
    imageThreadRunningLabel->setObjectName("ImageCacheStatus");
    imageThreadRunningLabel->setToolTip(tip);
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
    statusBar()->addWidget(includeSidecarsToggleBtn);
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
    folderDockTabText = "Folders";
    // folderDockTabText = "  📁  ";
    QPixmap pm(":/images/icon16/anchor.png");
    folderDockTabRichText = "test";
//    folderDockTabRichText = Utilities::pixmapToString(pm);
    dockTextNames << folderDockTabText;
    folderDock = new DockWidget(folderDockTabText, "FolderDock", this);  // Folders 📁
    // folderDock->setObjectName("FoldersDock");
    folderDock->setWidget(fsTree);
    connect(folderDock, &DockWidget::focus, this, &MW::focusOnDock);
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

    // question mark button
    BarBtn *folderQuestionBtn = new BarBtn();
    folderQuestionBtn->setIcon(QIcon(":/images/icon16/questionmark.png"));
    folderQuestionBtn->setToolTip("How this works");
    connect(folderQuestionBtn, &BarBtn::clicked, fsTree, &FSTree::howThisWorks);
    folderTitleLayout->addWidget(folderQuestionBtn);

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

    connect(folderDock, &QDockWidget::visibilityChanged, this, &MW::folderDockVisibilityChange);
}

void MW::createFavDock()
{
    if (G::isLogger) G::log("MW::createFavDock");
    favDockTabText = "Bookmarks";
    // favDockTabText = "  🔖  ";
    dockTextNames << favDockTabText;
    favDock = new DockWidget(favDockTabText, "BookmarkDock", this);  // Bookmarks📗 🔖 🏷️ 🗂️
    // favDock->setObjectName("Bookmarks");
    favDock->setWidget(bookmarks);
    connect(favDock, &DockWidget::focus, this, &MW::focusOnDock);

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
}

void MW::createFilterDock()
{
    if (G::isLogger) G::log("MW::createFilterDock");
    filterDockTabText = "Filters";
    // filterDockTabText = "  🤏  ";
    dockTextNames << filterDockTabText;
    filterDock = new DockWidget(filterDockTabText, "FilterDock", this);  // Filters 🤏♆🔻 🕎  <font color=\"red\"><b>♆</b></font> does not work

    // customize the filterDock titlebar
    QHBoxLayout *filterTitleLayout = new QHBoxLayout();
    filterTitleLayout->setContentsMargins(0, 0, 0, 0);
    filterTitleLayout->setSpacing(0);
    filterTitleBar = new DockTitleBar("Filters", filterTitleLayout);
    filterDock->setTitleBarWidget(filterTitleBar);
    connect(filterDock, &DockWidget::focus, this, &MW::focusOnDock);

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
    filterTitleLayout->addSpacing(10);

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
}

void MW::createMetadataDock()
{
    if (!G::useInfoView) return;
    if (G::isLogger) G::log("MW::createMetadataDock");
    // this does not work
//    QPixmap pixmap(":/images/icon16/anchor.png");
//    metadataDockTabText = Utilities::pixmapToString(pixmap);
    metadataDockTabText = "Metadata";
    // metadataDockTabText = "  📷  ";
    dockTextNames << metadataDockTabText;
    metadataDock = new DockWidget(metadataDockTabText, "MetadataDock", this);    // Metadata
    metadataDock->setWidget(infoView);
    connect(metadataDock, &DockWidget::focus, this, &MW::focusOnDock);

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
}

void MW::createThumbDock()
{
    if (G::isLogger) G::log("MW::createThumbDock");
    thumbDockTabText = "Thumbnails";
    dockTextNames << thumbDockTabText;
    thumbDock = new DockWidget(thumbDockTabText, "ThumbDock", this);  // Thumbnails
    // thumbDock = new QDockWidget("Thumbs", this);  // Thumbnails
    // thumbDock->setObjectName("ThumbDock");
    thumbDock->setWidget(thumbView);

    // thumbDock->titleBarWidget()->installEventFilter(thumbDock);

    // QWidget *thumbTitleBar = new QWidget();
    // thumbDock->setTitleBarWidget(thumbTitleBar);

    // customize the embelDock titlebar
    // QHBoxLayout *thumbTitleLayout = new QHBoxLayout();
    // thumbTitleLayout->setContentsMargins(0, 0, 0, 0);
    // thumbTitleLayout->setSpacing(0);
    // DockTitleBar *thumbTitleBar = new DockTitleBar("Thumbs", thumbTitleLayout);
    // thumbDock->setTitleBarWidget(thumbTitleBar);

    connect(thumbDock, &DockWidget::focus, this, &MW::focusOnDock);
    connect(thumbDock, &DockWidget::dockLocationChanged, this, &MW::setThumbDockFeatures);
    connect(thumbDock, &DockWidget::topLevelChanged, this, &MW::setThumbDockFloatFeatures);
    connect(thumbDock, &DockWidget::closeFloatingDock, this, &MW::toggleThumbDockVisibity);

    // connect(thumbDock, &QDockWidget::dockLocationChanged, this, &MW::setThumbDockFeatures);
    // connect(thumbDock, &QDockWidget::topLevelChanged, this, &MW::setThumbDockFloatFeatures);
}

void MW::createEmbelDock()
{
    if (G::isLogger) G::log("MW::createEmbelDock");
    embelProperties = new EmbelProperties(this, settings);

    connect (embelProperties, &EmbelProperties::templateChanged, this, &MW::embelTemplateChange);
    connect (embelProperties, &EmbelProperties::centralMsg, this, &MW::setCentralMessage);
    connect (embelProperties, &EmbelProperties::syncEmbellishMenu, this, &MW::syncEmbellishMenu);

    embelDockTabText = "Embellish";
    // embelDockTabText = "  🎨  ";
    dockTextNames << embelDockTabText;
    embelDock = new DockWidget(embelDockTabText, "EmbelDock", this);  // Embellish
    embelDock->setObjectName("EmbelDock");
    embelDock->setWidget(embelProperties);
    embelDock->setFloating(false);
    embelDock->setVisible(true);
    // prevent MW splitter resizing embelDock so cannot see header - and + buttons in embellish
    embelDock->setMinimumWidth(275);
    connect(embelDock, &DockWidget::focus, this, &MW::focusOnDock);

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

    // connect(this, &MW::tabifiedDockWidgetActivated, this, &MW::embelDockActivated);

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
}

void MW::createPreferences()
{
    if (G::isLogger) G::log("MW::createPreferences");
    pref = new Preferences(this);
}

void MW::createStressTest()
{
    if (G::isLogger) G::log("MW::createPreferences");
    stressTest = new StressTest(this, dm, bookmarks, fsTree);
    connect(stressTest, &StressTest::next, sel, &Selection::next);
    connect(stressTest, &StressTest::prev, sel, &Selection::prev);
    connect(stressTest, &StressTest::selectFolder, fsTree, &FSTree::select);
    connect(stressTest, &StressTest::selectBookmark, bookmarks, &BookMarks::select);

}

