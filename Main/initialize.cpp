#include "Main/mainwindow.h"

void MW::initialize()
{
    if (G::isLogger) G::log(CLASSFUNCTION);
    this->setWindowTitle(winnowWithVersion);
    G::stop = false;
    G::dmEmpty = true;
    G::isProcessingExportedImages = false;
    G::isDev = isDevelopment();
    G::isInitializing = true;
    G::actDevicePixelRatio = 1;
    G::dpi = 72;
//    G::dpi = 96;
    G::ptToPx = G::dpi / 72;
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
    slideCount = 0;
    prevCentralView = 0;
    G::labelColors << "Red" << "Yellow" << "Green" << "Blue" << "Purple";
    G::ratings << "1" << "2" << "3" << "4" << "5";
    pickStack = new QStack<Pick>;
    slideshowRandomHistoryStack = new QStack<QString>;
    scrollRow = 0;

}

void MW::setupPlatform()
{
    if (G::isLogger) G::log(CLASSFUNCTION);
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

void MW::createCentralWidget()
{
    if (G::isLogger) G::log(CLASSFUNCTION);
    // centralWidget required by ImageView/CompareView constructors
    centralWidget = new QWidget(this);
    centralWidget->setObjectName("centralWidget");
    // stack layout for loupe, table, compare and grid displays
    centralLayout = new QStackedLayout;
    centralLayout->setContentsMargins(0, 0, 0, 0);
}

void MW::setupCentralWidget()
{
    if (G::isLogger) G::log(CLASSFUNCTION);
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
    centralWidget->setLayout(centralLayout);
    setCentralWidget(centralWidget);
}

void MW::createFilterView()
{
    if (G::isLogger) G::log(CLASSFUNCTION);
    // G::fontSize req'd for row heights in filter view
//    if (setting->contains("fontSize"))
//        G::fontSize = setting->value("fontSize").toString();
//    else
//        G::fontSize = "12";
    qDebug() << "MW::createFilterView  G::fontSize =" << G::fontSize;
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
    if (G::isLogger) G::log(CLASSFUNCTION);
    icd = new ImageCacheData(this);
    iconCacheData = new IconCacheData(this);
    metadata = new Metadata;
    cacheProgressBar = new ProgressBar(this);

    // loadSettings not run yet
    if (isSettings && setting->contains("combineRawJpg"))
        combineRawJpg = setting->value("combineRawJpg").toBool();
    else combineRawJpg = false;

    dm = new DataModel(this, metadata, filters, combineRawJpg);
//    thumb = new Thumb(dm, metadata, frameDecoder);

    dm->iconChunkSize = 3000;

    // show appropriate count column in filters
//    if (combineRawJpg) {
//        filters->hideColumn(3);
//        filters->showColumn(4);
//    }
//    else {
//        filters->hideColumn(4);
//        filters->showColumn(3);
//    }
    filters->totalColumnToUse(combineRawJpg);

    connect(filters, &Filters::searchStringChange, dm, &DataModel::searchStringChange);
    connect(dm, &DataModel::updateClassification, this, &MW::updateClassification);
    connect(dm, &DataModel::centralMsg, this, &MW::setCentralMessage);
    connect(dm, &DataModel::updateStatus, this, &MW::updateStatus);
    connect(this, &MW::setValue, dm, &DataModel::setValue);
    connect(this, &MW::setValueSf, dm, &DataModel::setValueSf);

    buildFilters = new BuildFilters(this, dm, metadata, filters, combineRawJpg);

    connect(this, &MW::abortBuildFilters, buildFilters, &BuildFilters::stop);
    connect(buildFilters, &BuildFilters::addToDatamodel, dm, &DataModel::addMetadataForItem,
            Qt::BlockingQueuedConnection);
    connect(buildFilters, &BuildFilters::stopped, this, &MW::reset);
    connect(buildFilters, &BuildFilters::updateProgress, filters, &Filters::updateProgress);
    connect(buildFilters, &BuildFilters::finishedBuildFilters, filters, &Filters::finishedBuildFilters);

    // test how to add to datamodel from another thread
    connect(this, &MW::testAddToDM, dm, &DataModel::addMetadataForItem);
}

void MW::createSelectionModel()
{
/*
    The application only has one selection model which is shared by ThumbView, GridView and
    TableView, insuring that each view is in sync, except when a view is hidden.
*/
    if (G::isLogger) G::log(CLASSFUNCTION);
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
    if (G::isLogger) G::log(CLASSFUNCTION);
    frameDecoder = new FrameDecoder(this);
    connect(this, &MW::abortFrameDecoder, frameDecoder, &FrameDecoder::stop);
    connect(frameDecoder, &FrameDecoder::stopped, this, &MW::reset);
    connect(frameDecoder, &FrameDecoder::setFrameIcon, dm, &DataModel::setIconFromVideoFrame);
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
    if (G::isLogger) G::log(CLASSFUNCTION);
    metadataCacheThread = new MetadataCache(this, dm, metadata, frameDecoder);

    if (isSettings) {
        if (setting->contains("cacheAllMetadata")) metadataCacheThread->cacheAllMetadata = setting->value("cacheAllMetadata").toBool();
        if (setting->contains("cacheAllIcons")) metadataCacheThread->cacheAllIcons = setting->value("cacheAllIcons").toBool();
    }
    else {
        metadataCacheThread->cacheAllMetadata = true;
        metadataCacheThread->cacheAllIcons = false;
    }

    if (setting->contains("iconChunkSize")) {
        metadataCacheThread->metadataChunkSize = setting->value("iconChunkSize").toInt();
    }
    else {
        metadataCacheThread->metadataChunkSize = 3000;
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
            this, &MW::setCentralMessage, Qt::DirectConnection);

    connect(metadataCacheThread, &MetadataCache::selectFirst,
            thumbView, &IconView::selectFirst);


    // MetaRead
    metaReadThread = new MetaRead(this, dm, metadata, frameDecoder);
//    metaRead = new MetaRead(this, dm, metadata, frameDecoder);
//    metaRead->moveToThread(&metaReadThread);

    metaReadThread->iconChunkSize = 2000;
    if (setting->contains("iconChunkSize")) {
        dm->iconChunkSize = setting->value("iconChunkSize").toInt();
        metaReadThread->iconChunkSize = setting->value("iconChunkSize").toInt();
//        metaRead->iconChunkSize = setting->value("iconChunkSize").toInt();
    }
    else {
        dm->iconChunkSize = 3000;
    }

//    // delete thread when finished
//    connect(metaReadThread, &QThread::finished, metaReadThread, &QObject::deleteLater);
    // signal to stop MetaRead
    connect(this, &MW::abortMetaRead, metaReadThread, &MetaRead::stop);
    // signal stopped when abort completed
    connect(metaReadThread, &MetaRead::stopped, this, &MW::reset);
    // read metadata
    connect(this, &MW::startMetaRead, metaReadThread, &MetaRead::setCurrentRow);
    // add metadata to datamodel
    connect(metaReadThread, &MetaRead::addToDatamodel, dm, &DataModel::addMetadataForItem,
            Qt::BlockingQueuedConnection);
    // add icon to datamodel
    connect(metaReadThread, &MetaRead::setIcon, dm, &DataModel::setIcon);
    // message metadata reading completed
    connect(metaReadThread, &MetaRead::done, this, &MW::loadConcurrentMetaDone);
    // Signal to MW::loadConcurrentStartImageCache to prep and run fileSelectionChange
    connect(metaReadThread, &MetaRead::triggerImageCache, this, &MW::loadConcurrentStartImageCache);
    // check icons visible is correct
    connect(metaReadThread, &MetaRead::updateIconBestFit, this, &MW::updateIconBestFit);
    // update statusbar metadata active light
    connect(metaReadThread, &MetaRead::runStatus, this, &MW::updateMetadataThreadRunStatus);
    // pause waits until isRunning == false
    connect(this, &MW::interruptMetaRead, metaReadThread, &MetaRead::interrupt/*, Qt::BlockingQueuedConnection*/);

    connect(metaReadThread, &MetaRead::finished, metaReadThread, &MetaRead::testFinished);

//    metaReadThread.start();
}

void MW::createImageCache()
{
    if (G::isLogger) G::log(CLASSFUNCTION);

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
    connect(metadataCacheThread, SIGNAL(loadImageCache()),
            this, SLOT(loadImageCacheForNewFolder()));

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

    // concurrent load
    // add to image cache list
    // signal to ImageCache new image selection req'd?
    connect(metaReadThread, &MetaRead::addToImageCache,
            imageCacheThread, &ImageCache::addCacheItemImageMetadata);
    // signal ImageCache refresh
    connect(this, &MW::refreshImageCache, imageCacheThread, &ImageCache::refreshImageCache);
//    connect(metaRead, &MetaRead::setImageCachePosition,
//            imageCacheThread, &ImageCache::setCurrentPosition);

}

void MW::createThumbView()
{
    if (G::isLogger) G::log(CLASSFUNCTION);
    thumbView = new IconView(this, dm, icd, "Thumbnails");
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
        if (setting->contains("labelChoice")) thumbView->labelChoice = setting->value("labelChoice").toString();
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

    // trigger fileSelectionChange
    connect(thumbView, &IconView::fileSelectionChange, this, &MW::fileSelectionChange);

    connect(thumbView, &IconView::updateStatus, this, &MW::updateStatus);

    connect(thumbView->verticalScrollBar(), SIGNAL(valueChanged(int)),
            this, SLOT(thumbHasScrolled()));

    connect(thumbView->horizontalScrollBar(), SIGNAL(valueChanged(int)),
            this, SLOT(thumbHasScrolled()));
}

void MW::createGridView()
{
    if (G::isLogger) G::log(CLASSFUNCTION);
    gridView = new IconView(this, dm, icd, "Grid");
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
        if (setting->contains("labelChoice")) gridView->labelChoice = setting->value("labelChoice").toString();
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
    if (G::isLogger) G::log(CLASSFUNCTION);
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

void MW::createVideoView()
{
    if (G::isLogger) G::log(CLASSFUNCTION);
    videoView = new VideoView(this, thumbView);

    // back and forward mouse buttons toggle pick
    connect(videoView, &VideoView::togglePick, this, &MW::togglePick);
    // drop event
    connect(videoView, &VideoView::handleDrop, this, &MW::handleDrop);
}

void MW::createImageView()
{
/*
    ImageView displays the image in the central widget. The image is either from the image
    cache or read from the file if the cache is unavailable. Creation is dependent on
    metadata, imageCacheThread, thumbView, datamodel and settings.
*/
    if (G::isLogger) G::log(CLASSFUNCTION);
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
    connect(imageCacheThread, &ImageCache::loadImage, imageView, &ImageView::loadImage);
}

void MW::createCompareView()
{
    if (G::isLogger) G::log(CLASSFUNCTION);
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
    if (G::isLogger) G::log(CLASSFUNCTION);
    infoString = new InfoString(this, dm, setting/*, embelProperties*/);

}

void MW::createInfoView()
{
/*
    InfoView shows basic metadata in a dock widget.
*/
    if (!G::useInfoView) return;
    if (G::isLogger) G::log(CLASSFUNCTION);
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

void MW::createEmbel()
{
    if (G::isLogger) G::log(CLASSFUNCTION);
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
    if (G::isLogger) G::log(CLASSFUNCTION);
    // loadSettings has not run yet (dependencies, but QSettings has been opened
    fsTree = new FSTree(this, metadata);
    fsTree->setMaximumWidth(folderMaxWidth);
    fsTree->setShowImageCount(true);
    fsTree->combineRawJpg = combineRawJpg;

    // selection change check if triggered by ejecting USB drive
//    connect(fsTree, &FSTree::selectionChange, this, &MW::watchCurrentFolder);

    // this works for touchpad tap
    connect(fsTree, &FSTree::pressed, this, &MW::folderSelectionChange);

    // reselect folder after external program drop onto FSTree
    connect(fsTree, &FSTree::folderSelection, this, &MW::folderSelectionChange);

    // if move drag and drop then delete files from source folder(s)
    connect(fsTree, &FSTree::deleteFiles, this, &MW::deleteFiles);

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
    if (G::isLogger) G::log(CLASSFUNCTION);
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

    connect(fsTree->fsModel, &FSModel::update, bookmarks, &BookMarks::update);

    // if move drag and drop then delete files from source folder(s)
    connect(bookmarks, &BookMarks::deleteFiles, this, &MW::deleteFiles);

    // refresh FSTree count after drag and drop to BookMarks
    connect(bookmarks, &BookMarks::refreshFSTree, fsTree, &FSTree::refreshModel);

    // reselect folder after external program drop onto BookMarks
    connect(bookmarks, &BookMarks::folderSelection, this, &MW::folderSelectionChange);
}

void MW::createAppStyle()
{
    if (G::isLogger) G::log(CLASSFUNCTION);
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
    if (G::isLogger) G::log(CLASSFUNCTION);
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
    updateMetadataThreadRunStatus(false, true, CLASSFUNCTION);
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

void MW::createFolderDock()
{
    if (G::isLogger) G::log(CLASSFUNCTION);
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
    if (G::isLogger) G::log(CLASSFUNCTION);
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
    if (G::isLogger) G::log(CLASSFUNCTION);
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
    BarBtn *updateFiltersBtn = new BarBtn();
    updateFiltersBtn->setIcon(QIcon(":/images/icon16/refresh.png"));
    updateFiltersBtn->setToolTip("Update filters");
    connect(updateFiltersBtn, &BarBtn::clicked, buildFilters, &BuildFilters::build);
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
    if (!G::useInfoView) return;
    if (G::isLogger) G::log(CLASSFUNCTION);
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
    if (G::isLogger) G::log(CLASSFUNCTION);
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
    if (G::isLogger) G::log(CLASSFUNCTION);
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
    if (G::isLogger) G::log(CLASSFUNCTION);
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
    MW::tabifyDockWidget(folderDock, favDock);
    MW::tabifyDockWidget(favDock, filterDock);
    if (G::useInfoView) MW::tabifyDockWidget(filterDock, metadataDock);
    if (!hideEmbellish) if (G::useInfoView) MW::tabifyDockWidget(metadataDock, embelDock);
}

void MW::createMessageView()
{
    if (G::isLogger) G::log(CLASSFUNCTION);
    messageView = new QWidget;
    msg.setupUi(messageView);
//    Ui::message ui;
//    ui.setupUi(messageView);
}

