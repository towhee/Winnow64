#include "Main/mainwindow.h"
#include "Develop/workingimagecache.h"
#include "Utilities/fileops.h"
#include "Cache/devpreviewcache.h"

void MW::initialize()
{
    if (G::isLogger || G::isFlowLogger) G::log("MW::initialize");
    // connect(this, &QWindow::windowStateChanged, this, &MW::onWindowStateChanged);

    setWindowTitle(winnowWithVersion);
    G::stop = false;
    G::removingFolderFromDM = false;
    G::isProcessingExportedImages = false;
    G::isInitializing = true;
    G::isModifyingDatamodel = false;
    G::isEmbellish = false;
    G::actDevicePixelRatio = 1;
    G::dpi = 72;
    G::ptToPx = G::dpi / 72;
    #ifdef  Q_OS_WIN
    G::ptToPx *= 1.2;
    #endif
    G::isSlideShow = false;
    stopped["MetaRead"] = true;
    stopped["ImageCache"] = true;
    stopped["BuildFilters"] = true;
    workspaces = new QList<WorkspaceData>;
    recentFolders = new QStringList;
    ingestHistoryFolders = new QStringList;
    hasGridBeenActivated = true;
    wasThumbDockVisible = true;

    // drag text not used but keep as example
    // dragLabel->setWindowFlags(Qt::FramelessWindowHint | Qt::Tool | Qt::WindowStaysOnTopHint);
    // dragLabel->setAttribute(Qt::WA_TranslucentBackground);
    // dragLabel->setStyleSheet("background: rgba(0, 0, 0, 150); color: white; padding: 5px; border-radius: 5px;");
    // dragLabel->hide();

    // window
    isDragDrop = false;
    setAcceptDrops(true);
    setMouseTracking(true);
    QString msg;

    // status bar buttons / labels
    modifyImagesBtn = new BarBtn();
    msg = "Toggle on/off to permit modify images.";
    modifyImagesBtn->setToolTip(msg);
    connect(modifyImagesBtn, &BarBtn::clicked, this, &MW::toggleModifyImagesClick);

    includeSidecarsToggleBtn = new BarBtn();
    msg = "Toggle on/off to include sidecars when dragging.";
    includeSidecarsToggleBtn->setToolTip(msg);
    connect(includeSidecarsToggleBtn, &BarBtn::clicked, this, &MW::toggleIncludeSidecarsClick);

    colorManageToggleBtn = new BarBtn();
    msg = "Toggle \"Color Manage\" on/off.";
    // colorManageToggleBtn->setToolTip(msg);
    connect(colorManageToggleBtn, &BarBtn::clicked, this, &MW::toggleColorManageClick);

    panToFocusToggleBtn = new BarBtn();
    msg = "Toggle \"Pan To Predicted Focus\" on/off.  Mouse click or shortcut B";
    panToFocusToggleBtn->setToolTip(msg);
    connect(panToFocusToggleBtn, &BarBtn::clicked, this, [this]() { panFocusToggleAction->trigger(); });

    reverseSortBtn = new BarBtn();
    reverseSortBtn ->setToolTip("Toggle sort direction: Mouse click or shortcut Opt/Alt + S");
    connect(reverseSortBtn, &BarBtn::clicked, this, &MW::toggleSortDirectionClick);

    useRawBtn = new BarBtn();
    msg = "Toggle \"Decode Raw\" on/off.";
    useRawBtn->setToolTip(msg);
    connect(useRawBtn, &BarBtn::clicked, this, &MW::toggleUseRawClick);

    filterStatusLabel = new QLabel;
    filterStatusLabel->setToolTip("The images have been filtered");

    subfolderStatusLabel = new QLabel;
    subfolderStatusLabel->setToolTip("Showing contents of all subfolders");

    rawJpgStatusBtn = new BarBtn();
    rawJpgStatusBtn->setToolTip("Toggle \"Combine Raw+Jpg\" on/off.  Shortcut: Opt/Alt + J");
    connect(rawJpgStatusBtn, &BarBtn::clicked, this, [this]() {
        combineRawJpgAction->toggle();  // flip checked state without re-emitting triggered
        setCombineRawJpg();
    });
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
        G::trash = "Trash";
    #endif

    // Scale memoryAbortMB to host RAM. Must run before MetaRead start and
    // the GUI memory watchdog so the cap is correct on first read.
    quint64 totalMB = 0;
    #ifdef Q_OS_MAC
        totalMB = static_cast<quint64>(Mac::totalMemoryMB());
    #elif defined(Q_OS_WIN)
        totalMB = Win::totalMemoryMB();
    #endif
    if (totalMB > 0) G::memoryAbortMB = G::computeMemoryAbortMB(totalMB);
    /*
    qDebug().nospace() << "MW::setupPlatform Memory: total=" << totalMB
                       << " MB, available=" << G::availableMemoryMB
                       << " MB, abortMB=" << G::memoryAbortMB;
                          // */
}

void MW::checkRecoveredGeometry(const QRect &availableGeometry, QRect *restoredGeometry,
                                int frameHeight)
{
/*
    Not being used.
*/
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
    /*
    // G::fontSize req'd for row heights in filter view
    if (G::settings->contains("fontSize"))
       G::fontSize = G::settings->value("fontSize").toString();
    else
       G::fontSize = 12;
    */
    filters = new Filters(this);
    filters->setObjectName("Filters");
    filters->setMaximumWidth(folderMaxWidth);

    /* Not using SIGNAL(itemChanged(QTreeWidgetItem*,int) because it triggers
       for every item in Filters */
    connect(filters, &Filters::filterChange, this, &MW::filterChange);
}

void MW::createDataModel()
{
    if (G::isLogger) G::log("MW::createDataModel");
    icd = new ImageCacheData(this);
    metadata = new Metadata;
    progress = new Progress(this);

    // loadSettings not run yet
    if (isSettings && settings->contains("combineRawJpg"))
        combineRawJpg = settings->value("combineRawJpg").toBool();
    else combineRawJpg = false;
    G::combineRawJpg = combineRawJpg;

    dm = new DataModel(nullptr, metadata, filters, combineRawJpg);

    // enable global access to datamodel
    G::setDM(dm);

    if (settings->contains("showThumbNailSymbolHelp"))
        dm->showThumbNailSymbolHelp = settings->value("showThumbNailSymbolHelp").toBool();
    else dm->showThumbNailSymbolHelp = true;

    connect(dm, &DataModel::stop, this, &MW::stop, Qt::BlockingQueuedConnection);
    connect(dm, &DataModel::folderChange, this, &MW::folderChanged);
    connect(filters, &Filters::searchStringChange, dm, &DataModel::searchStringChange);
    connect(dm, &DataModel::updateClassification, this, &MW::updateClassification);
    connect(dm, &DataModel::centralMsg, this, &MW::setCentralMessage);
    connect(dm, &DataModel::updateStatus, this, &MW::updateStatus);
    connect(dm, &DataModel::updateProgress, filters, &Filters::updateProgress);
    connect(dm, &DataModel::refreshViewsOnCacheChange, this, &MW::refreshViewsOnCacheChange);
    connect(dm, &DataModel::iconChunkResized, this, &MW::reloadIconChunk);

    connect(metadata, &Metadata::updateSidecarStatus, this, &MW::updateSidecarStatus);

    connect(this, &MW::updateCurrent, dm, &DataModel::setCurrentSF);
    connect(this, &MW::setValDm, dm, &DataModel::setValDm);
    connect(this, &MW::setValSf, dm, &DataModel::setValSf);

    buildFilters = new BuildFilters(this, dm, metadata, filters);

    // connect(this, &MW::abortBuildFilters, buildFilters, &BuildFilters::stop);
    // connect(buildFilters, &BuildFilters::stopped, this, &MW::aborted);
    // connect(buildFilters, &BuildFilters::addToDatamodel, dm, &DataModel::addMetadataForItem,
    //         Qt::BlockingQueuedConnection);
    connect(this, &MW::abortBuildFilters, buildFilters, &BuildFilters::abortProcessing);
    connect(buildFilters, &BuildFilters::updateProgress, filters, &Filters::updateProgress);
    connect(buildFilters, &BuildFilters::finishedBuildFilters, filters, &Filters::finishedBuildFilters);
    connect(buildFilters, &BuildFilters::updateFilterMenu, this, &MW::updateFilterMenu);
    connect(buildFilters, &BuildFilters::quickFilter, this, &MW::quickFilterComplete);
    connect(buildFilters, &BuildFilters::filterLastDay, this, &MW::filterLastDay);
    connect(buildFilters, &BuildFilters::searchTextEdit, this, &MW::searchTextEdit);
    connect(buildFilters, &BuildFilters::filterChange, this, &MW::filterChange);
}

void MW::createSelectionModel()
{
/*
    The application only has one selection model which is shared by ThumbView, GridView and
    TableView, insuring that each view is in sync, except when a view is hidden.
*/
    if (G::isLogger) G::log("MW::createSelectionModel");
    thumbView->setSelectionModel(dm->selectionModel);
    tableView->setSelectionModel(dm->selectionModel);
    tableView->frozenView->setSelectionModel(dm->selectionModel);
    gridView->setSelectionModel(dm->selectionModel);
}

void MW::createMetaRead()
{
/*
    When a new folder is selected the metadataReadThread is started to load all the
    metadata and thumbs for each image. If the user scrolls during the cache process then
    the metadataReadThread is restarted at the first visible thumb to speed up the
    display of the thumbs for the user.
*/
    if (G::isLogger) G::log("MW::createMDCache");

    // Runs multiple reader threads to load metadata and thumbnails
    metaRead = new MetaRead(this, dm, metadata, imageCache);

    /* MetaRead reads G::showCacheProgress directly (loaded in MW::loadSettings),
       so no per-instance progress flag needs setting here. */

    // Release MetaRead's navigation gate as soon as the awaited row decodes.
    // Wired here (not in createImageCache) because metaRead is created after imageCache.
    connect(imageCache, &ImageCache::setCached,
            metaRead, &MetaRead::onRowCached, Qt::QueuedConnection);

    // set a value in dm->sf proxy
    connect(metaRead, &MetaRead::setValSf, dm, &DataModel::setValSf);

    // cleanup icons outside icon chunk range
    connect(metaRead, &MetaRead::cleanupIcons, dm, &DataModel::clearIconsOutsideChunkRange);

    // update thumbView in case scrolling has occurred
    connect(metaRead, &MetaRead::updateScroll, thumbView, &IconView::repaintView,
            Qt::BlockingQueuedConnection);

    // update gridView in case scrolling has occurred
    connect(metaRead, &MetaRead::updateScroll, gridView, &IconView::repaintView,
            Qt::BlockingQueuedConnection);

    // loading image metadata into datamodel, okay to select
    connect(metaRead, &MetaRead::okToSelect, sel, &Selection::okToSelect);

    // selectCurrentIndex from MetaRead
    connect(metaRead, &MetaRead::select, sel, &Selection::setCurrentIndex, Qt::QueuedConnection);

    // MetaRead thread abort (when MW::stop)
    connect(this, &MW::abortMetaRead, metaRead, &MetaRead::abortProcessing);

    // message metadata thread stopped when close Winnow
    // connect(metaRead, &MetaRead::stopped, this, &MW::aborted);

    // message metadata reading completed
    connect(metaRead, &MetaRead::done, this, &MW::folderChangeCompleted);

    // Signal to change selection, fileSelectionChange, update ImageCache
    connect(metaRead, &MetaRead::fileSelectionChange, this, &MW::fileSelectionChange, Qt::QueuedConnection);

    // update statusbar metadata active light
    connect(metaRead, &MetaRead::runStatus, this, &MW::updateMetadataThreadRunStatus);

    // update loading metadata in central window
    connect(metaRead, &MetaRead::centralMsg, this, &MW::setCentralMessage);

    // update filters MetaRead progress
    connect(metaRead, &MetaRead::updateProgressInFilter, filters, &Filters::updateProgress);

    // update loading metadata in statusbar
    connect(metaRead, &MetaRead::updateProgressInStatusbar, this,
            [this](int item, int items, QColor color) {
                progress->updateProgress(progressMetaReadRow, item, items, color);
            });

    // memory overrun guardrail: surface a critical dialog and abort the
    // in-flight folder load when MetaRead's footprint probe trips.
    connect(metaRead, &MetaRead::memoryOverrun,
            this, &MW::onMemoryOverrun, Qt::QueuedConnection);

    // same path for the DataModel hot-path probe (fires from the GUI thread
    // when buried processing queued addMetadataForItem events).
    connect(dm, &DataModel::memoryOverrun,
            this, &MW::onMemoryOverrun, Qt::QueuedConnection);

    metaRead->metaReadThread.start();
}

void MW::createCatalogScanner()
{
/*
    The background scan over the user's designated roots. It owns its own low-priority
    thread (Main/catalogscanner.h), so nothing here belongs on the GUI thread but the
    signal wiring.
*/
    if (G::isLogger) G::log("MW::createCatalogScanner");

    catalogScanner = new CatalogScanner;

    connect(catalogScanner, &CatalogScanner::progress, this,
            [this](int done, int total) {
                if (progress && total > 0)
                    progress->updateProgress(progressCatalogRow, done, total,
                                             QColor("#3fa8a0"));
            }, Qt::QueuedConnection);

    connect(catalogScanner, &CatalogScanner::finished, this,
            [this](int scanned, int indexed, bool aborted) {
                Q_UNUSED(scanned)
                if (progress) progress->clearProgress(progressCatalogRow);
                if (catalogView) catalogView->setScanning(false);
                if (findPanel) findPanel->setScanning(false);
                if (catalogRootsDlg) catalogRootsDlg->setScanning(false);
                /* The row already said it was happening and the panel shows the result,
                   so a background scan finishes silently -- the same rule the devPreview
                   build follows. Only the counts the user can act on are surfaced, and
                   those are in the Catalog panel. */
                if (catalogView && catalogDock && catalogDock->isVisible())
                    catalogView->refresh();
                if (findPanel && filterDock->isVisible()) findPanel->refresh();
                if (G::isLogger)
                    G::log("MW::createCatalogScanner",
                           "catalog scan finished, indexed = " +
                           QString::number(indexed) +
                           (aborted ? " (stopped early)" : ""));
            }, Qt::QueuedConnection);

    connect(catalogScanner, &CatalogScanner::status, this, [this](const QString &msg) {
        if (progress) progress->setRowText(progressCatalogRow, msg);
    }, Qt::QueuedConnection);
}

void MW::createImageCache()
{
    if (G::isLogger) G::log("MW::createImageCache");

    imageCache = new ImageCache(this, icd, dm);

    /*
    Control of the ImageCache size is either automatic or manual.

    If automatic:
        setAutoMaxMB(true, strategy)
    If manual:
        setAutoMaxMB(false, strategy = Ignore)
        setMaxMB(mb)
    */

    ImageCache::AutoStrategy as = ImageCache::AutoStrategy::Ignore;  // default
    // load settings
    if (!isSettings || simulateJustInstalled) {
        as = ImageCache::AutoStrategy::Moderate;
        imageCache->setAutoMaxMB(true, as);
    }
    else {
        bool isAuto = false;
        if (settings->contains("autoMaxMB")) {
            isAuto = settings->value("autoMaxMB").toBool();
        }

        if (settings->contains("autoMaxMBStrategy")) {
            QString strategy = settings->value("autoMaxMBStrategy").toString();
            if (strategy == "Frugal") as = ImageCache::AutoStrategy::Frugal;
            if (strategy == "Moderate") as = ImageCache::AutoStrategy::Moderate;
            if (strategy == "Greedy") as = ImageCache::AutoStrategy::Greedy;
        }

        quint64 cacheMaxMB = 1024; // default
        if (settings->contains("cacheMaxMB")) {
            cacheMaxMB = settings->value("cacheMaxMB").toULongLong();
        }

        imageCache->setAutoMaxMB(isAuto, as);
        if (!isAuto) {
            imageCache->setMaxMB(cacheMaxMB);
        }
    }

    connect(&imageCacheThread, &QThread::finished,
            imageCache, &QObject::deleteLater);

    // Signal to ImageCache start
    connect(this, &MW::startImageCache,
            imageCache, &ImageCache::start);

    // Signal to stop (quit) the ImageCache
    connect(this, &MW::stopImageCache,
            imageCache, &ImageCache::stop);

    // Signal to abort the ImageCache
    connect(this, &MW::abortImageCache, imageCache, &ImageCache::abortProcessing);

    // message metadata thread stopped when close Winnow
    // connect(imageCache, &ImageCache::stopped, this, &MW::aborted);

    // Signal to ImageCache filterChange
    connect(this, &MW::imageCacheFilterChange,
            imageCache, &ImageCache::filterChange);

    // Signal to ImageCache color manage change
    connect(this, &MW::imageCacheColorManageChange,
            imageCache, &ImageCache::colorManageChange);

    /* Signal to initialize ImageCache Changed from BlockingQueuedConnection to
    QueuedConnection: BQC froze the UI thread whenever ImageCache's event loop was
    occupied by an in-flight fillCache for the prior folder instance — the UI waited for
    the ImageCache thread to return to its event loop, which can take many seconds with
    14 parallel 8MP decoders. QueuedConnection preserves ordering against the subsequent
    (already-queued) setImageCachePosition emit in MW::folderChangeCompleted. */
    connect(this, &MW::initializeImageCache,
            imageCache, &ImageCache::initialize,
            Qt::QueuedConnection);

    // // Signal to update imageCache parameters
    // connect(this, &MW::imageCacheChangeParam,
    //         imageCache, &ImageCache::updateImageCacheParam);

    // Signal to update imageCache auto cache size
    // connect(this, &MW::setAutoMaxMB,
    //         imageCache, &ImageCache::setAutoMaxMB, Qt::BlockingQueuedConnection);

    // Signal to update imageCache maxMB
    connect(this, &MW::setMaxMB,
            imageCache, &ImageCache::setMaxMB, Qt::BlockingQueuedConnection);

    // Signal to ImageCache new image selection
    connect(this, &MW::setImageCachePosition,
            imageCache, &ImageCache::setCurrentPosition);

    // Signal ImageCache is processing
    connect(imageCache, &ImageCache::updateIsRunning,
            this, &MW::updateImageCachingThreadRunStatus);

    // Signal datamodel to signal back when some row is loaded
    connect(imageCache, &ImageCache::setCached,
            dm, &DataModel::setCached);

    /* RAW demosaic progress -> the "Demosaic" status-bar row (MW gates on current image +
       Winnow + Auto-run off). Clear it when the current image finishes caching. */
    connect(imageCache, &ImageCache::demosaicProgress, this, &MW::onDemosaicProgress);
    connect(imageCache, &ImageCache::setCached, this, [this](int sfRow, bool isCached, int){
        if (isCached && dm && sfRow == dm->currentSfRow)
            progress->clearProgress(progressDemosaicRow);
    });

    // Signal datamodel to signal back when some row is loaded
    connect(imageCache, &ImageCache::waitingForRow,
            dm, &DataModel::imageCacheWaiting);

    // Update the cache status progress bar when changed in ImageCache
    connect(imageCache, &ImageCache::showCacheStatus,
            this, &MW::updateImageCacheStatus);

    // Signal from ImageCache::run() to update cache status in datamodel
    connect(imageCache, &ImageCache::refreshViews,
            this, &MW::refreshViewsOnCacheChange);

    // set values in the datamodel
    connect(imageCache, &ImageCache::setValSf, dm, &DataModel::setValSf);

    imageCache->start();
}

void MW::createThumbView()
{
    if (G::isLogger) G::log("MW::createThumbView");
    thumbView = new IconView(this, dm, "Thumbnails");
    thumbView->setObjectName("Thumbnails");
    // thumbView->setSpacing(0);                // thumbView not visible without this
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

    /* assignedIconWidth is the reference width persisted by snapshotWorkspace and reapplied
       on workspace/fullscreen restore. Seed it from the loaded iconWidth; the IconView ctor
       set it from iconWidth before iconWidth was assigned here, so it is otherwise stale. */
    thumbView->assignedIconWidth = thumbView->iconWidth;

    // scrolling
    connect(thumbView->verticalScrollBar(), SIGNAL(valueChanged(int)),
            this, SLOT(thumbHasScrolled()));
    connect(thumbView->horizontalScrollBar(), SIGNAL(valueChanged(int)),
            this, SLOT(thumbHasScrolled()));

    // Force thumbView to update whenever the proxy model layout changes
    // connect(dm->sf, &QAbstractItemModel::layoutChanged, thumbView, qOverload<>(&QListView::update));

    // Also repaint when rows are moved or inserted, just to be safe
    // connect(dm->sf, &QAbstractItemModel::rowsMoved, thumbView, qOverload<>(&QListView::update));
    // connect(dm->sf, &QAbstractItemModel::rowsInserted, thumbView, qOverload<>(&QListView::update));
    // connect(dm->sf, &QAbstractItemModel::modelReset, thumbView, qOverload<>(&QListView::update));

    // // Force thumbView to update whenever the proxy model layout changes
    // connect(dm->sf, &QAbstractItemModel::layoutChanged,
    //         thumbView, static_cast<void (QWidget::*)()>(&QWidget::update));

    // // Also repaint when rows are moved or inserted, just to be safe
    // connect(dm->sf, &QAbstractItemModel::rowsMoved,
    //         thumbView, static_cast<void (QWidget::*)()>(&QWidget::update));
    // connect(dm->sf, &QAbstractItemModel::rowsInserted,
    //         thumbView, static_cast<void (QWidget::*)()>(&QWidget::update));
    // connect(dm->sf, &QAbstractItemModel::modelReset,
    //         thumbView, static_cast<void (QWidget::*)()>(&QWidget::update));
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

    /* assignedIconWidth is the reference width rejustify() uses to find the closest
       justified size for the current window. It must track the restored/default
       iconWidth, otherwise rejustify() ignores the persisted size and the grid icon
       size is inconsistent between sessions. */
    gridView->assignedIconWidth = gridView->iconWidth;

    /*
    qDebug() << "MW::createGridView"
             << "gridView->iconWidth =" << gridView->iconWidth
             << "gridView->iconHeight =" << gridView->iconHeight;
                //*/

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
            /*
            qDebug() << "MW::createTableView"
                     << "Table field =" << setField
                     << "okToShow =" << okToShow
                ; //*/
            itemList = tableView->ok->findItems(setField);
            if (itemList.length()) {
                int row = itemList[0]->row();
                QModelIndex idx = tableView->ok->index(row, 1);
                tableView->ok->setData(idx, okToShow, Qt::EditRole);
            }
        }
        settings->endGroup();
    }

    // reset frozen columns
    // tableView->resizeColumns();

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
    connect(sel, &Selection::updateChange, this, &MW::updateChange);
    connect(sel->sm, &QItemSelectionModel::selectionChanged, sel, &Selection::selectionChanged);
    /* Re-evaluate selection-count dependent actions (focusStack, meanStack, asCompare,
       Open With apps) whenever the selection set changes, not just when the current
       index changes (fileSelectionChange). */
    connect(sel->sm, &QItemSelectionModel::selectionChanged,
            this, &MW::enableSelectionDependentMenus);
    /* Develop edits apply to the whole selection, so the dock's red multi-image alert
       tracks the selection set (and any queued propagation lands before it changes). */
    connect(sel->sm, &QItemSelectionModel::selectionChanged,
            this, &MW::updateDevelopSelectionWarning);
    connect(sel, &Selection::updateStatus, this, &MW::updateStatus);
    connect(sel, &Selection::updateCurrent, dm, &DataModel::setCurrentSF);
}

void MW::createVideoView()
{
    if (G::isLogger) G::log("MW::createVideoView");
    if (!G::useMultimedia) return;

    videoView = new VideoView(this, thumbView, sel);

    // back and forward mouse buttons toggle pick
    connect(videoView, &VideoView::togglePick, this, &MW::togglePick);

    // mouse side key press
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

    connect(imageView, &ImageView::togglePick, this, &MW::togglePick);
    connect(imageView, &ImageView::updateStatus, this, &MW::updateStatus);
    connect(imageView, &ImageView::setCentralMessage, this, &MW::setCentralMessage);
    connect(thumbView, &IconView::thumbClick, imageView, &ImageView::panTo);
    connect(imageView, &ImageView::handleDrop, this, &MW::handleDrop);
    connect(imageView, &ImageView::killSlideshow, this, &MW::slideShow);
    connect(imageView, &ImageView::keyPress, this, &MW::keyPressEvent);
    connect(imageView, &ImageView::mouseSideKeyPress,
            this, &MW::mouseSideKeyPress);
    connect(imageView, &ImageView::focusClick,
            focusPointTrainer, &FocusPointTrainer::focus);
    connect(imageView, &ImageView::loupeRect,
            thumbView, &IconView::loupeRect);
    connect(imageView, &ImageView::showLoupeRect,
            thumbView, &IconView::showLoupeRect);
}

void MW::createCompareView()
{
    if (G::isLogger) G::log("MW::createCompareView");
    compareImages = new CompareImages(this, centralWidget, metadata, dm, sel,
                                      thumbView, icd, imageView, infoView);

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
    infoView->enable(false);

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
                if (fieldName == setField) {
                    QModelIndex idParentChk = k->index(row, 2);
                    // set the flag whether to display or not
                    k->setData(idParentChk, okToShow, Qt::EditRole);
                    isFound = true;
                    break;
                }
                for (int childRow = 0; childRow < k->rowCount(idParent); childRow++) {
                    QModelIndex idChild = k->index(childRow, 0, idParent);
                    QString fieldName = qvariant_cast<QString>(idChild.data());
                    // find the match
                    if (fieldName == setField) {
                        QModelIndex idChildChk = k->index(childRow, 2, idParent);
                        // set the flag whether to display or not
                        k->setData(idChildChk, okToShow, Qt::EditRole);
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
            if (fieldName == setField) {
                QModelIndex idParentChk = k->index(row, 2);
                // set the flag whether to display or not
                k->setData(idParentChk, okToShow, Qt::EditRole);
                break;
            }
            for (int childRow = 0; childRow < k->rowCount(idParent); childRow++) {
                QModelIndex idChild = k->index(childRow, 0, idParent);
                QString fieldName = qvariant_cast<QString>(idChild.data());
                // find the match
                if (fieldName == setField) {
                    QModelIndex idChildChk = k->index(childRow, 2, idParent);
                    // set the flag whether to display or not
                    k->setData(idChildChk, okToShow, Qt::EditRole);
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
            this, SLOT(infoViewChanged(QStandardItem*)));

    // update filters (lambda so the signal's 2 args map onto updateCategory's 3,
    // leaving runSync at its default of false)
    connect(infoView, &InfoView::updateFilter, buildFilters,
            [this](BuildFilters::Category category, BuildFilters::AfterAction newAction) {
                buildFilters->updateCategory(category, newAction);
            });

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
}void MW::createFSTree()
{
    if (G::isLogger) G::log("MW::createFSTree");
    // loadSettings has not run yet (dependencies, but QSettings has been opened
    fsTree = new FSTree(this, dm, metadata, this);
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

    // reselect folder after external program drop onto FSTree or a selectionChange
    // connect(fsTree, &FSTree::folderSelection, this, &MW::folderSelectionChange);

    // select a folder including modifier keys
    connect(fsTree, &FSTree::folderSelectionChange, this, &MW::folderSelectionChange);

    // enable/disable "Collapse all folders" based on tree expansion state
    connect(fsTree, &QTreeView::expanded, this, &MW::updateCollapseFoldersAction);
    connect(fsTree, &QTreeView::collapsed, this, &MW::updateCollapseFoldersAction);

    // refresh datamodel after dragdrop operation
    connect(fsTree, &FSTree::updateCounts, this, &MW::refresh);
    // connect(fsTree, &FSTree::updateCounts, this, &MW::updateImageCount);

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

    // update status in the central MessageTab
    connect(fsTree, &FSTree::centralMsg, this, &MW::setCentralMessage);


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

    // this works for touchpad tap
    // triggers MW::bookmarkClicked > fsTree sync > MW::folderSelectionChange
    connect(bookmarks, &BookMarks::itemPressed, this, &MW::bookmarkClicked);

    // update folder image counts
    // connect(fsTree->fsModel, &FSModel::update, bookmarks, &BookMarks::updateBookmarks);

    // connect(bookmarks, SIGNAL(dropOp(Qt::KeyboardModifiers, bool, QString)),
    //         this, SLOT(dropOp(Qt::KeyboardModifiers, bool, QString)));

    // if move drag and drop then delete files from source folder(s)
    connect(bookmarks, &BookMarks::deleteFiles, this, &MW::deleteFiles,
            Qt::BlockingQueuedConnection);

    // refresh FSTree count after drag and drop to BookMarks
    connect(bookmarks, &BookMarks::updateCounts, this, &MW::updateImageCount);

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
    G::css = widgetCSS.css();
    this->setStyleSheet(G::css);

    // fix tooltips for windows (still works in MacOS)
    #ifdef Q_OS_WIN
    QPalette pal;
    int b = G::backgroundShade;
    pal.setColor(QPalette::ToolTipBase, QColor(b,b,b));
    pal.setColor(QPalette::ToolTipText, QColor(255, 255, 255));
    QToolTip::setPalette(pal);
    #endif
}

void MW::createStatusBar()
{
    if (G::isLogger) G::log("MW::createStatusBar");
    statusBar()->setObjectName("WinnowStatusBar");

    // statusBar()->setFixedHeight(50);  test this works

    // cache status (Progress widget) on right side of status bar

    // progress width (Progress is created in MW::createDataModel, where first req'd)
    if (isSettings && settings->contains("cacheStatusWidth"))
        cacheBarProgressWidth = settings->value("cacheStatusWidth").toInt();
    else cacheBarProgressWidth = 100;
    if (cacheBarProgressWidth < 100 || cacheBarProgressWidth > 1000) cacheBarProgressWidth = 200;

    // progress tooltip
    QString progressToolTip = "Cache status for current folder(s):\n";
    progressToolTip += "  • DarkGray:   \tbackground for all images in folder\n";
    progressToolTip += "  • Red:        \tmetadata loaded (visible while loading)\n";
    progressToolTip += "  • Green:      \timages that are cached\n";
    progressToolTip += "  • LightGreen: \tcurrent image";
    progressToolTip += "\n\nMouse click on cache status progress to open cache preferences.";

    // Progress widget setup
    progress->setContainerWidth(cacheBarProgressWidth);
    progress->setBackgroundColor(widgetCSS.widgetBackgroundColor);
    progress->setToolTip(progressToolTip);
    progress->setToolTipDuration(100000);

    /* Add the runtime rows this window drives (ImageCache is created by Progress
       itself). MetaRead items complete out of order -> Fill::Cell; FocusStack and
       RawDenoise are sequential -> Fill::FromStart. */
    progressMetaReadRow = progress->addRow("MetaRead", 4, progress->metaReadCacheColor,
                                           Progress::Fill::Cell);
    progress->setRowText(progressMetaReadRow, "Metadata");
    progressFocusStackRow = progress->addRow("FocusStack", 2, QColor(Qt::darkYellow),
                                             Progress::Fill::FromStart);
    progress->setRowText(progressFocusStackRow, "Focus Stack");
    /* Visible orange (G::darkorange "#1a0d00" is a near-black slider-gradient end,
       not a display color, so it vanishes against the status-bar background). */
    progressRawDenoiseRow = progress->addRow("RawDenoise", 2, QColor(G::lightorange),
                                             Progress::Fill::FromStart);
    progress->setRowText(progressRawDenoiseRow, "Raw Denoise");
    /* Winnow raw demosaic progress on develop-open when auto-run denoise is off (the
       denoise path shows its own row). Blue to distinguish from the orange denoise. */
    progressDemosaicRow = progress->addRow("Demosaic", 2, QColor("#3a97c9"),
                                           Progress::Fill::FromStart);
    progress->setRowText(progressDemosaicRow, "Demosaicing");
    /* Building devPreviews for images edited in an earlier session (Develop > Build
       Developed Previews, or the background preference). Minutes of rendering that the
       user is not waiting on, so it belongs in the status bar next to the other caching
       work rather than under a popup parked over the image. Violet: unclaimed by the
       rows above. */
    progressDevPreviewRow = progress->addRow("DevPreview", 2, QColor("#8f7fd1"),
                                             Progress::Fill::FromStart);
    progress->setRowText(progressDevPreviewRow, "Dev Previews");
    /* The background catalog scan over the user's designated roots. Minutes to hours of
       reading the user is not waiting on, so it reports here beside the other background
       work rather than under a popup. Teal: unclaimed by the rows above. */
    progressCatalogRow = progress->addRow("Catalog", 2, QColor("#3fa8a0"),
                                          Progress::Fill::FromStart);
    progress->setRowText(progressCatalogRow, "Catalog");
    connect(progress, &Progress::clicked, this, [this]() {
        preferences("CacheHeader");
    });
    connect(progress, &Progress::heightChanged, this, [this](int h) {
        /* Never shrink the status bar below the height it had before Progress
           was added (captured at the end of createStatusBar); only grow it when
           Progress needs more room.

           QStatusBar centers its widgets and reserves a small vertical margin.
           When Progress's preferred height equals the bar height it gets
           compressed by that margin, squeezing out its reserved top/bottom
           padding. Request a little extra height so Progress always receives its
           full preferred height (its sizeHint caps it, so the extra just becomes
           QStatusBar's outer margin). */
        int overhead = 4;
        if (statusBar()->layout()) {
            QMargins m = statusBar()->layout()->contentsMargins();
            overhead = qMax(overhead, m.top() + m.bottom());
        }
        statusBar()->setMinimumHeight(qMax(statusBarBaseHeight, h + overhead));
    });
    statusBar()->addPermanentWidget(progress, 1);

    // end progressbar

    statusBarSpacer = new QLabel;
    statusBarSpacer->setText(" ");
    statusBar()->addPermanentWidget(statusBarSpacer);

    // label to show metadataThreadRunning status
    // QFontMetrics fm(metadataThreadRunningLabel->font());
    // int charWidth = fm.horizontalAdvance(QStringLiteral("◉")) * 1.3;
    // qDebug() << "charWidth =" << charWidth;
    // metadataThreadRunningLabel->setFixedWidth(charWidth);
    metadataThreadRunningLabel->setObjectName("MetadataCacheStatus");
    statusBar()->addPermanentWidget(metadataThreadRunningLabel);
    QString tip = "Metadata and Icon caching:\n";
    tip += "\n";
    tip += "  • Green:    \tAll cached\n";
    tip += "  • Red:      \tCaching in progress\n";
    tip += "  • Orange:   \tCaching had a hickup\n";
    tip += "  • Gray:     \tEmpty folder, no images to cache\n";
    metadataThreadRunningLabel->setToolTip(tip);

    // label to show imageThreadRunning status
    imageThreadRunningLabel = new QLabel;
    statusBar()->addPermanentWidget(imageThreadRunningLabel);
    bool isAutoSize = imageCache->getAutoMaxMB();
    quint64 maxMB = imageCache->getMaxMB();
    imageThreadRunningLabel->setObjectName("ImageCacheStatus");
    tip = getImageCacheRunningTip(isAutoSize, maxMB);
    imageThreadRunningLabel->setToolTip(tip);
    // imageThreadRunningLabel->setFixedWidth(charWidth);

    setCacheRunningLightsWidth();

    /* Which PICTURE the grid and the loupe show, at the EXTREME LEFT of the status bar:

         0 Original   the camera's picture, as shot     G::PreviewSource::Original
         1 Developed  the develop recipe rendered       G::PreviewSource::Developed

       The rows ARE the G::PreviewSource values, so no mapping table is needed.

       DELIBERATELY NOT A MODE CONTROL. Develop mode is entered with D (operationModeAction)
       and left with E / G / T, and it is a different kind of thing from these two -- an
       editing mode, not a choice of picture. Putting it in here made the third item read
       as a peer of the other two when it is not.

       DISABLED IN DEVELOP MODE, where it reads Developed: Develop always shows the
       developed image, so the choice does not exist there. MW::syncPreviewSourceEnabled
       owns that, and setPreviewSource / setOperationMode call it. */
    previewSourceCombo = new QComboBox;
    previewSourceCombo->setObjectName("previewSourceCombo");
    previewSourceCombo->addItem("Original");
    previewSourceCombo->addItem("Developed");
    previewSourceCombo->setCurrentIndex(int(G::previewSource));
    previewSourceCombo->setFocusPolicy(Qt::NoFocus);
    previewSourceCombo->setToolTip(
        "What you are looking at:\n"
        "  Original — the camera's picture, as shot\n"
        "  Developed — your develop edits applied\n"
        "Shortcut: Y\n\n"
        "Develop mode (D) always shows the developed image.");
    /* Pin the width to the widest item via the STYLESHEET min-width/max-width. The global widget
       CSS (widgetcss.cpp) sets "QComboBox { min-width: 6em }", and in Qt a stylesheet min-width
       OVERRIDES setFixedWidth() -- so the width must be a stylesheet property here to win. Width =
       widest bold label + padding(12) + arrow(14) + slack. */
    QFont opModeBold = previewSourceCombo->font(); opModeBold.setBold(true);
    const int opModeW = qMax(QFontMetrics(opModeBold).horizontalAdvance("Original"),
                             QFontMetrics(opModeBold).horizontalAdvance("Developed")) + 12 + 4;
    previewSourceCombo->setStyleSheet(QString(
        "QComboBox {background-color:#445f76; color:white;"
        " padding:1px 6px; border:none; border-radius:3px; margin:0 4px;"
        " min-width:%1px; max-width:%1px;}"
        "QComboBox::drop-down {border:none; width:14px;} "
        "QComboBox QAbstractItemView {background-color:#445f76; color:white;"
        " selection-background-color:#445f76;}").arg(opModeW));
    connect(previewSourceCombo, &QComboBox::activated, this, [this](int i) {
        setPreviewSource(i == int(G::PreviewSource::Developed)
                             ? G::PreviewSource::Developed
                             : G::PreviewSource::Original);
    });
    statusBar()->addWidget(previewSourceCombo);

    // add process progress bar to left side of statusBar
    progressBar = new QProgressBar;
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
    /* useRawBtn removed from the status bar: raw-vs-preview decode is now owned by the Operation
       Mode (Preview=preview, Develop=raw; overridable in Develop via the "Edit source" selector).
       The button is still created + wired (toggleUseRawClick) in case we bring it back. */
    // statusBar()->addWidget(useRawBtn);
    statusBar()->addWidget(panToFocusToggleBtn);
    filterStatusLabel->setPixmap(QPixmap(":/images/icon16/filter.png"));
    filterStatusLabel->setAlignment(Qt::AlignVCenter);
    statusBar()->addWidget(filterStatusLabel);
    // deprecated
    // subfolderStatusLabel->setPixmap(QPixmap(":/images/icon16/subfolders.png"));
    // statusBar()->addWidget(subfolderStatusLabel);
    statusBar()->addWidget(rawJpgStatusBtn);
    slideShowStatusLabel->setPixmap(QPixmap(":/images/icon16/slideshow.png"));
    statusBar()->addWidget(slideShowStatusLabel);

    setThreadRunStatusInactive();

    // general status on left side of status bar
    statusLabel = new QLabel;
    statusLabel->setObjectName("statusLabel");
    statusBar()->addWidget(statusLabel);

    /* Apply the show-caching-progress preference (G::showCacheProgress, loaded in
       MW::loadSettings). ImageCache and MetaRead read G::showCacheProgress directly.
       Gate the cache rows (ImageCache + MetaRead) on the preference so they stay
       hidden when "show caching progress" is off. Progress manages its own container
       visibility from its row content, so we don't toggle the widget here. */
    setCacheProgressEnabled(G::showCacheProgress);

    /* Capture the status bar height with all the other widgets but without
       Progress. The heightChanged handler keeps the bar at least this tall so
       the other status text is never clipped when Progress collapses. */
    bool wasVisible = progress->isVisibleTo(statusBar());
    progress->setVisible(false);
    statusBarBaseHeight = statusBar()->sizeHint().height();
    progress->setVisible(wasVisible);
    statusBar()->setMinimumHeight(qMax(statusBarBaseHeight, progress->preferredHeight()));
}

void MW::createFolderDock()
{
    if (G::isLogger) G::log("MW::createFolderDock");
    folderDockTabText = "Folders";
    // folderDockTabText = "  📁  ";
    QPixmap pm(":/images/icon16/anchor.png");
    folderDockTabRichText = "test";
    // folderDockTabRichText = Utilities::pixmapToString(pm);
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
    folderTitleBar->setToolTip(dockTabToolTip(folderDockTabText));
    // The folders tab starts with its text title; when G::useDockTitleGraphic
    // is on, MW::updateDockTabGraphics swaps text<->graphic per available width.

    // add widgets to the right side of the title bar layout
    // toggle expansion button
    BarBtn *folderRefreshBtn = new BarBtn();
    folderRefreshBtn->setIcon(":/images/icon16/refresh.png", G::iconOpacity);
    folderRefreshBtn->setToolTip("Refresh folders and image counts");
    //folderRefreshBtn->setStyleSheet("QToolTip { color: red;}");
    connect(folderRefreshBtn, &BarBtn::clicked, this, &MW::refresh);
    folderTitleLayout->addWidget(folderRefreshBtn);

    // Spacer
    folderTitleLayout->addSpacing(5);

    // preferences button
    BarBtn *folderGearBtn = new BarBtn();
    folderGearBtn->setIcon(":/images/icon16/gear.png", G::iconOpacity);
    folderGearBtn->setToolTip("Preferences");
    connect(folderGearBtn, &BarBtn::clicked, this, &MW::allPreferences);
    folderTitleLayout->addWidget(folderGearBtn);

    // Spacer
    folderTitleLayout->addSpacing(10);

    // question mark button
    BarBtn *folderQuestionBtn = new BarBtn();
    folderQuestionBtn->setIcon(":/images/icon16/questionmark.png", G::iconOpacity);
    folderQuestionBtn->setToolTip("How this works: folder selection tips");
    connect(folderQuestionBtn, &BarBtn::clicked, fsTree, &FSTree::howThisWorks);
    folderTitleLayout->addWidget(folderQuestionBtn);

    // Spacer
    folderTitleLayout->addSpacing(10);

    // collapse/expand body button
    if (G::useDWCollapse) {
        BarBtn *folderCollapseBtn = new BarBtn();
        folderCollapseBtn->setIcon(":/images/icon16/collapse.png", G::iconOpacity);
        folderCollapseBtn->setToolTip("Collapse panel.");
        connect(folderCollapseBtn, &BarBtn::clicked, folderDock, &DockWidget::toggleCollapsed);
        connect(folderDock, &DockWidget::collapsedChanged, folderCollapseBtn, [folderCollapseBtn](bool c){
            folderCollapseBtn->setIcon(QIcon(c ? ":/images/icon16/expand.png" : ":/images/icon16/collapse.png"));
            folderCollapseBtn->setToolTip(c ? "Expand panel." : "Collapse panel.");
        });
        folderTitleLayout->addWidget(folderCollapseBtn);

        // Spacer
        folderTitleLayout->addSpacing(10);
    }

    // close button
    BarBtn *folderCloseBtn = new BarBtn();
    folderCloseBtn->setIcon(":/images/icon16/close.png", G::iconOpacity);
    folderCloseBtn->setToolTip("Hide the Folders Panel");
    connect(folderCloseBtn, &BarBtn::clicked, this, &MW::closeFolderDock);
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
    favTitleBar->setToolTip(dockTabToolTip(favDockTabText));

    // add widgets to the right side of the title bar layout
    // refresh button
    BarBtn *favRefreshBtn = new BarBtn();
    favRefreshBtn->setIcon(":/images/icon16/refresh.png", G::iconOpacity);
    favRefreshBtn->setToolTip("Refresh bookmarks and image counts");
    connect(favRefreshBtn, &BarBtn::clicked, this, &MW::refresh);
    favTitleLayout->addWidget(favRefreshBtn);

    // Spacer
    favTitleLayout->addSpacing(5);

    // preferences button
    BarBtn *favGearBtn = new BarBtn();
    favGearBtn->setIcon(":/images/icon16/gear.png", G::iconOpacity);
    favGearBtn->setToolTip("Preferences");
    connect(favGearBtn, &BarBtn::clicked, this, &MW::allPreferences);
    favTitleLayout->addWidget(favGearBtn);

    // Spacer
    favTitleLayout->addSpacing(10);

    // question mark button
    BarBtn *favQuestionBtn = new BarBtn();
    favQuestionBtn->setIcon(":/images/icon16/questionmark.png", G::iconOpacity);
    favQuestionBtn->setToolTip("How this works: bookmark tips");
    connect(favQuestionBtn, &BarBtn::clicked, bookmarks, &BookMarks::howThisWorks);
    favTitleLayout->addWidget(favQuestionBtn);

    // Spacer
    favTitleLayout->addSpacing(10);

    // collapse/expand body button
    if (G::useDWCollapse) {
        BarBtn *favCollapseBtn = new BarBtn();
        favCollapseBtn->setIcon(":/images/icon16/collapse.png", G::iconOpacity);
        favCollapseBtn->setToolTip("Collapse panel.");
        connect(favCollapseBtn, &BarBtn::clicked, favDock, &DockWidget::toggleCollapsed);
        connect(favDock, &DockWidget::collapsedChanged, favCollapseBtn, [favCollapseBtn](bool c){
            favCollapseBtn->setIcon(QIcon(c ? ":/images/icon16/expand.png" : ":/images/icon16/collapse.png"));
            favCollapseBtn->setToolTip(c ? "Expand panel." : "Collapse panel.");
        });
        favTitleLayout->addWidget(favCollapseBtn);

        // Spacer
        favTitleLayout->addSpacing(10);
    }

    // close button
    BarBtn *favCloseBtn = new BarBtn();
    favCloseBtn->setIcon(":/images/icon16/close.png", G::iconOpacity);
    favCloseBtn->setToolTip("Hide the Bookmarks Panel");
    connect(favCloseBtn, &BarBtn::clicked, this, &MW::closeFavDock);
    favTitleLayout->addWidget(favCloseBtn);

    // Spacer
    favTitleLayout->addSpacing(5);
}

void MW::createFilterDock()
{
/*
    The Filters dock, or -- with G::useFindDock -- the FIND dock: the same DockWidget and
    the same "FilterDock" objectName, re-titled and with the Catalog panel's function
    folded in behind a scope switch. See Views/findpanel.h.

    IT IS THE SAME DOCK OBJECT DELIBERATELY. Everything that reaches for filterDock -- the
    full-screen dock set, workspaces, solo mode, the view-mode enables, the
    collapsed-state restore, MW::showFilterDock -- keeps working untouched, and a
    WindowState saved before this change still restores, because the objectName Qt keys
    that state on has not changed. Only the visible tab TEXT differs, which is a label
    rather than an identity.
*/
    if (G::isLogger) G::log("MW::createFilterDock");
    filterDockTabText = G::useFindDock ? "Find" : "Filters";
    // filterDockTabText = "  🤏  ";
    dockTextNames << filterDockTabText;
    filterDock = new DockWidget(filterDockTabText, "FilterDock", this);  // Filters 🤏♆🔻 🕎  <font color=\"red\"><b>♆</b></font> does not work

    // customize the filterDock titlebar
    QHBoxLayout *filterTitleLayout = new QHBoxLayout();
    filterTitleLayout->setContentsMargins(0, 0, 0, 0);
    filterTitleLayout->setSpacing(0);
    filterTitleBar = new DockTitleBar("Filters", filterTitleLayout);
    filterDock->setTitleBarWidget(filterTitleBar);
    filterTitleBar->setToolTip(dockTabToolTip(filterDockTabText));
    connect(filterDock, &DockWidget::focus, this, &MW::focusOnDock);

    // add widgets to the right side of the title bar layout
    // toggle expansion button
    BarBtn *updateFiltersBtn = new BarBtn();
    updateFiltersBtn->setIcon(":/images/icon16/refresh.png", G::iconOpacity);
    updateFiltersBtn->setToolTip("Update filters");
    connect(updateFiltersBtn, &BarBtn::clicked, this, &MW::updateAllFilters);
    filterTitleLayout->addWidget(updateFiltersBtn);

    // Spacer
    filterTitleLayout->addSpacing(5);

    // toggle expansion button
    BarBtn *toggleExpansionBtn = new BarBtn();
    toggleExpansionBtn->setIcon(QIcon(":/images/icon16/foldertree.png"));
    toggleExpansionBtn->setToolTip("Toggle expand all / collapse all in the Filters panel.");
    connect(toggleExpansionBtn, &BarBtn::clicked, filters, &Filters::toggleExpansion);
    filterTitleLayout->addWidget(toggleExpansionBtn);

    // Spacer
    filterTitleLayout->addSpacing(5);

    // preferences button
    BarBtn *filterGearBtn = new BarBtn();
    filterGearBtn->setIcon(":/images/icon16/gear.png", G::iconOpacity);
    filterGearBtn->setToolTip("Preferences");
    connect(filterGearBtn, &BarBtn::clicked, this, &MW::allPreferences);
    filterTitleLayout->addWidget(filterGearBtn);

    // Spacer
    filterTitleLayout->addSpacing(10);

    // question mark button
    BarBtn *filterQuestionBtn = new BarBtn();
    filterQuestionBtn->setIcon(":/images/icon16/questionmark.png", G::iconOpacity);
    filterQuestionBtn->setToolTip("How this works");
    connect(filterQuestionBtn, &BarBtn::clicked, filters, &Filters::howThisWorks);
    filterTitleLayout->addWidget(filterQuestionBtn);

    // Spacer
    filterTitleLayout->addSpacing(10);

    // collapse/expand body button
    if (G::useDWCollapse) {
        BarBtn *filterCollapseBtn = new BarBtn();
        filterCollapseBtn->setIcon(":/images/icon16/collapse.png", G::iconOpacity);
        filterCollapseBtn->setToolTip("Collapse panel.");
        connect(filterCollapseBtn, &BarBtn::clicked, filterDock, &DockWidget::toggleCollapsed);
        connect(filterDock, &DockWidget::collapsedChanged, filterCollapseBtn, [filterCollapseBtn](bool c){
            filterCollapseBtn->setIcon(QIcon(c ? ":/images/icon16/expand.png" : ":/images/icon16/collapse.png"));
            filterCollapseBtn->setToolTip(c ? "Expand panel." : "Collapse panel.");
        });
        filterTitleLayout->addWidget(filterCollapseBtn);

        // Spacer
        filterTitleLayout->addSpacing(10);
    }

    // close button
    BarBtn *filterCloseBtn = new BarBtn();
    filterCloseBtn->setIcon(":/images/icon16/close.png", G::iconOpacity);
    filterCloseBtn->setToolTip("Hide the Filters Panel");
    connect(filterCloseBtn, &BarBtn::clicked, this, &MW::closeFilterDock);
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

    if (G::useFindDock) {
        /* FindPanel re-parents the filters tree into its own layout, so the tree is the
           SAME widget in both scopes rather than a copy that could drift. */
        findPanel = new FindPanel(filters);
        filterLayout->addWidget(findPanel);
        connect(findPanel, &FindPanel::loadResults, this, &MW::loadCatalogResults);
        connect(findPanel, &FindPanel::manageRootsRequested, this,
                &MW::manageCatalogRoots);
        /* Returning to Folders: the tree is holding catalog values, so rebuild it from
           the datamodel. buildFilters->reset() clears the catalog items (and the checks
           that went with them) before build() repopulates from the model. */
        connect(findPanel, &FindPanel::rebuildFolderCategoriesRequested, this, [this]{
            if (G::isInitializing) return;
            buildFilters->reset(false /*collapse*/);
            buildFiltersWhenModelReady(dm->instance);
            filterChange("FindPanel::rebuildFolderCategoriesRequested");
        });
        /* Refresh when the dock is actually shown rather than on every folder load:
           re-reading the catalog categories is a query per category, and it is only worth
           doing for a panel someone is looking at. */
        connect(filterDock, &QDockWidget::visibilityChanged, this, [this](bool visible){
            if (visible && findPanel) findPanel->refresh();
        });
    }
    else {
        filterLayout->addWidget(filters);
    }

    QFrame *frame = new QFrame;
    frame->setLayout(filterLayout);
    filterDock->setWidget(frame);
}

void MW::createCatalogDock()
{
/*
    The Catalog dock -- search every image Winnow has catalogued, not just the folder
    that is loaded. See notes/Documentation.txt "The Catalog (Cross-folder Search)".

    Its results are LOADED rather than filtered, so it is a sibling of the Folders and
    Bookmarks docks (things that put images in the model) as much as of Filters (which
    narrows what is already there). It is tabbed with them on the left for that reason.
*/
    if (G::isLogger) G::log("MW::createCatalogDock");

    /* With the Find dock there is no separate Catalog panel: its search box, keyword
       category and Load button are the Catalog scope of the one panel. catalogDock and
       catalogView stay NULL, and every entry point that used to show this dock
       (Shift+F2, Window > Catalog Panel, the full-screen dock set) switches the Find
       dock's scope instead. */
    if (G::useFindDock) return;

    catalogDockTabText = "Catalog";
    dockTextNames << catalogDockTabText;
    catalogDock = new DockWidget(catalogDockTabText, "CatalogDock", this);

    QHBoxLayout *catalogTitleLayout = new QHBoxLayout();
    catalogTitleLayout->setContentsMargins(0, 0, 0, 0);
    catalogTitleLayout->setSpacing(0);
    catalogTitleBar = new DockTitleBar("Catalog", catalogTitleLayout);
    catalogDock->setTitleBarWidget(catalogTitleBar);
    catalogTitleBar->setToolTip(dockTabToolTip(catalogDockTabText));
    connect(catalogDock, &DockWidget::focus, this, &MW::focusOnDock);

    // refresh button -- re-read the keyword categories and the catalog size
    BarBtn *catalogRefreshBtn = new BarBtn();
    catalogRefreshBtn->setIcon(":/images/icon16/refresh.png", G::iconOpacity);
    catalogRefreshBtn->setToolTip("Update the catalog keyword list");
    connect(catalogRefreshBtn, &BarBtn::clicked, this, [this]{ catalogView->refresh(); });
    catalogTitleLayout->addWidget(catalogRefreshBtn);

    catalogTitleLayout->addSpacing(10);

    BarBtn *catalogGearBtn = new BarBtn();
    catalogGearBtn->setIcon(":/images/icon16/gear.png", G::iconOpacity);
    catalogGearBtn->setToolTip("Preferences");
    connect(catalogGearBtn, &BarBtn::clicked, this, &MW::allPreferences);
    catalogTitleLayout->addWidget(catalogGearBtn);

    catalogTitleLayout->addSpacing(10);

    if (G::useDWCollapse) {
        BarBtn *catalogCollapseBtn = new BarBtn();
        catalogCollapseBtn->setIcon(":/images/icon16/collapse.png", G::iconOpacity);
        catalogCollapseBtn->setToolTip("Collapse panel.");
        connect(catalogCollapseBtn, &BarBtn::clicked, catalogDock,
                &DockWidget::toggleCollapsed);
        connect(catalogDock, &DockWidget::collapsedChanged, catalogCollapseBtn,
                [catalogCollapseBtn](bool c){
                    catalogCollapseBtn->setIcon(
                        QIcon(c ? ":/images/icon16/expand.png"
                                : ":/images/icon16/collapse.png"));
                    catalogCollapseBtn->setToolTip(c ? "Expand panel."
                                                    : "Collapse panel.");
                });
        catalogTitleLayout->addWidget(catalogCollapseBtn);
        catalogTitleLayout->addSpacing(10);
    }

    BarBtn *catalogCloseBtn = new BarBtn();
    catalogCloseBtn->setIcon(":/images/icon16/close.png", G::iconOpacity);
    catalogCloseBtn->setToolTip("Hide the Catalog Panel");
    connect(catalogCloseBtn, &BarBtn::clicked, this, &MW::closeCatalogDock);
    catalogTitleLayout->addWidget(catalogCloseBtn);

    catalogTitleLayout->addSpacing(5);

    catalogView = new CatalogView;
    catalogDock->setWidget(catalogView);

    /* The view never touches the datamodel: it reports paths and whether the user asked
       to Load (replace) or Add (append), and MW decides what either means. Both end in
       DataModel::addPaths, which emits folderChange, so MetaRead and then
       folderChangeCompleted run and the filters are rebuilt over the new row count
       without this having to arrange it. */
    connect(catalogView, &CatalogView::loadResults, this, &MW::loadCatalogResults);

    /* Which folders are indexed is CONFIGURATION and lives in its own dialog now, not in
       the lower third of the search panel. MW still owns the list and its persistence. */
    connect(catalogView, &CatalogView::manageRootsRequested, this,
            &MW::manageCatalogRoots);

    /* Refresh the categories when the dock is actually shown, rather than on every folder
       load: rebuilding the keyword tree is a query plus a tree build, and it is only
       worth doing for a panel someone is looking at. */
    connect(catalogDock, &QDockWidget::visibilityChanged, this, [this](bool visible){
        if (visible) catalogView->refresh();
    });
}

void MW::createMetadataDock()
{
    if (!G::useInfoView) return;
    if (G::isLogger) G::log("MW::createMetadataDock");

    metadataDockTabText = "Metadata";
    // metadataDockTabText = "  📷  ";
    dockTextNames << metadataDockTabText;
    metadataDock = new DockWidget(metadataDockTabText, "MetadataDock", this);    // Metadata
    metadataDock->setWidget(infoView);
    connect(metadataDock, &DockWidget::focus, this, &MW::focusOnDock);

    /* Experimenting to use rich text in QTabWidget for docks
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
    metaTitleBar->setToolTip(dockTabToolTip(metadataDockTabText));

    // add widgets to the right side of the title bar layout
    // preferences button
    BarBtn *metaGearBtn = new BarBtn();
    metaGearBtn->setIcon(":/images/icon16/gear.png", G::iconOpacity);
    metaGearBtn->setToolTip("Edit preferences including which items to show in this panel");
    connect(metaGearBtn, &BarBtn::clicked, this, &MW::infoViewPreferences);
    metaTitleLayout->addWidget(metaGearBtn);

    // Spacer
    metaTitleLayout->addSpacing(10);

    // question mark button
    BarBtn *metaQuestionBtn = new BarBtn();
    metaQuestionBtn->setIcon(":/images/icon16/questionmark.png", G::iconOpacity);
    metaQuestionBtn->setToolTip("How this works: metadata panel guide");
    connect(metaQuestionBtn, &BarBtn::clicked, infoView, &InfoView::howThisWorks);
    metaTitleLayout->addWidget(metaQuestionBtn);

    // Spacer
    metaTitleLayout->addSpacing(10);

    // collapse/expand body button
    if (G::useDWCollapse) {
        BarBtn *metaCollapseBtn = new BarBtn();
        metaCollapseBtn->setIcon(":/images/icon16/collapse.png", G::iconOpacity);
        metaCollapseBtn->setToolTip("Collapse panel.");
        connect(metaCollapseBtn, &BarBtn::clicked, metadataDock, &DockWidget::toggleCollapsed);
        connect(metadataDock, &DockWidget::collapsedChanged, metaCollapseBtn, [metaCollapseBtn](bool c){
            metaCollapseBtn->setIcon(c ? ":/images/icon16/expand.png" : ":/images/icon16/collapse.png", G::iconOpacity);
            metaCollapseBtn->setToolTip(c ? "Expand panel." : "Collapse panel.");
        });
        metaTitleLayout->addWidget(metaCollapseBtn);

        // Spacer
        metaTitleLayout->addSpacing(10);
    }

    // close button
    BarBtn *metaCloseBtn = new BarBtn();
    metaCloseBtn->setIcon(":/images/icon16/close.png", G::iconOpacity);
    metaCloseBtn->setToolTip("Hide the Metadata Panel");
    connect(metaCloseBtn, &BarBtn::clicked, this, &MW::closeMetadataDock);
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
    connect(thumbDock, &DockWidget::closeFloatingDock, this, &MW::closeThumbDock);

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
    embelTitleBar->setToolTip(dockTabToolTip(embelDockTabText));

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
    embelQuestionBtn->setToolTip("Embel Overview.");
    connect(embelQuestionBtn, &BarBtn::clicked, embelProperties, &EmbelProperties::coordHelp);
    embelTitleLayout->addWidget(embelQuestionBtn);

    // Spacer
    embelTitleLayout->addSpacing(5);

    // collapse/expand body button
    if (G::useDWCollapse) {
        BarBtn *embelCollapseBtn = new BarBtn();
        embelCollapseBtn->setIcon(":/images/icon16/collapse.png", G::iconOpacity);
        embelCollapseBtn->setToolTip("Collapse panel.");
        connect(embelCollapseBtn, &BarBtn::clicked, embelDock, &DockWidget::toggleCollapsed);
        connect(embelDock, &DockWidget::collapsedChanged, embelCollapseBtn, [embelCollapseBtn](bool c){
            embelCollapseBtn->setIcon(c ? ":/images/icon16/expand.png" : ":/images/icon16/collapse.png", G::iconOpacity);
            embelCollapseBtn->setToolTip(c ? "Expand panel." : "Collapse panel.");
        });
        embelTitleLayout->addWidget(embelCollapseBtn);

        // Spacer
        embelTitleLayout->addSpacing(10);
    }

    // close button
    BarBtn *embelCloseBtn = new BarBtn();
    embelCloseBtn->setIcon(":/images/icon16/close.png", G::iconOpacity);
    embelCloseBtn->setToolTip("Hide the Embellish Panel");
    connect(embelCloseBtn, &BarBtn::clicked, this, &MW::closeEmbelDock);
    embelTitleLayout->addWidget(embelCloseBtn);

    // Spacer
    embelTitleLayout->addSpacing(5);
}

void MW::createDevelopDock()
{
    if (G::isLogger) G::log("MW::createDevelopDock");
    developProperties = new DevelopProperties(this, settings);

    /* Every file operation flushes debounced Develop edits through this hook before it
       touches a file, so a pending sidecar write cannot land at the old path after a
       rename or resurrect a sidecar that was just deleted. Registered here because this
       is where developProperties comes into existence. See Utilities/fileops.h. */
    FileOps::setFlushHook([this]{
        if (developProperties) developProperties->flushAll();
    });

    /* devPreviews are a byproduct of a render that already happened, never a reason to
       decode a raw. Two frames can supply them:

         developFullFrame  the full-resolution settle render (onDevelopFullResReady). This
                           is the one that matters -- a devPreview encoded from it is the
                           real picture at sensor resolution, so the loupe can browse and
                           zoom it without decoding the raw at all.
         developFrame      the screen-resolution proxy, the fallback while the settle
                           render has not landed yet (or was superseded). Still correct,
                           just soft at 100%.

       Either way the tiers are a scale plus a JPEG encode -- a few ms. Declining when the
       requested image is not the one on screen is what makes multi-image propagation
       targets clear their stale preview instead of keeping one.
       See DevelopProperties::flushImage. */
    developProperties->setDevPreviewProvider(
        [this](const QString &fPath, QByteArray &thumbJpg, QByteArray &loupeJpg) -> bool {
            if (fPath.isEmpty()) return false;

            /* The frame on screen is not always what the recipe says. With the Transform
               preview eye off it is UNCROPPED and unstraightened; with the Replace eye
               off the heals are missing; on a History hover it is a different stack
               altogether. Caching any of those against the stored recipe's hash would
               key a preview to a picture it does not depict. Decline instead, which
               clears the stale preview -- the next faithful render writes a real one. */
            /* ... and a frame only depicts the recipe it was RENDERED from. Nothing
               replaces a retained frame when an edit is made -- only a render that
               completes and is still current does -- so after an edit inside the settle
               window (leave Develop, switch image, quit) the frame on hand is the
               PREVIOUS recipe's, with the same path and the same faithful flag. Caching
               it would stamp old pixels with the new recipe's hash, and since every
               staleness check downstream is that hash, the wrong picture would then look
               correct forever. Compare the recipes and decline on a mismatch. */
            const QByteArray recipe = developRecipeKey(fPath);
            const bool haveFull = fPath == developFullFramePath
                                  && !developFullFrame.isNull() && developFullFrameFaithful
                                  && !recipe.isEmpty() && developFullFrameRecipe == recipe;
            const bool haveProxy = fPath == developFramePath
                                   && !developFrame.isNull() && developFrameFaithful
                                   && !recipe.isEmpty() && developFrameRecipe == recipe;
            if (!haveFull && !haveProxy) return false;
            const QImage &src = haveFull ? developFullFrame : developFrame;

            auto encode = [](const QImage &im, int quality, QByteArray &out) {
                QBuffer buf(&out);
                if (!buf.open(QIODevice::WriteOnly)) return false;
                return im.save(&buf, "JPG", quality);
            };

            /* Thumbnail tier: G::maxIconSize is what Thumb scales embedded thumbs to, so
               matching it means the grid never up-scales a devPreview. */
            const QImage thumb = src.scaled(G::maxIconSize, G::maxIconSize,
                                            Qt::KeepAspectRatio,
                                            Qt::SmoothTransformation);
            if (!encode(thumb, 85, thumbJpg)) thumbJpg.clear();

            /* devPreview tier: the frame as rendered, capped only if the user asked for a
               cap. At the default (Full) this is a full sensor-resolution JPEG -- several
               MB rather than a few hundred KB, which is why the cache cap is measured in
               tens of GB. Quality is the "Developed preview quality" preference and
               defaults HIGH (90, not the 85 of the thumbnail tier) because these pixels
               are displayed at 100%, not just used as a placeholder during a decode.

               THE PROXY IS NOT GOOD ENOUGH FOR THIS TIER. The thumbnail above is 256 px,
               so the interactive proxy (sized to the VIEW, typically ~1600 px) serves it
               perfectly -- but this preview is what the loupe shows INSTEAD of decoding
               the raw, at 100%, and a proxy-sized one is visibly softer than the camera
               preview it replaces. It looked current forever, too: staleness is keyed on
               the recipe hash, which says nothing about resolution. And it was the common
               case rather than a corner: the retained full frame carries the recipe it
               was rendered from, so ANY edit inside the settle window (leave Develop,
               switch image, quit) failed the haveFull test and fell through to the proxy.

               So: full frame, or a proxy that already meets a finite cap. Otherwise
               decline the tier -- flushImage then drops any older entry, the loupe
               decodes the raw and shows the right pixels, and the build queued below
               writes a proper one. */
            const int cap = G::devPreviewMaxEdge;
            const QImage *loupeSrc = nullptr;
            if (haveFull) loupeSrc = &developFullFrame;
            else if (cap > 0 && qMax(developFrame.width(), developFrame.height()) >= cap)
                loupeSrc = &developFrame;

            if (loupeSrc) {
                QImage loupe = *loupeSrc;
                if (cap > 0 && qMax(loupe.width(), loupe.height()) > cap) {
                    loupe = loupe.scaled(cap, cap, Qt::KeepAspectRatio,
                                         Qt::SmoothTransformation);
                }
                if (!encode(loupe, G::devPreviewQuality, loupeJpg)) loupeJpg.clear();
            }
            else {
                /* Render one properly, off the GUI thread, through the same builder the
                   menu and the folder-load sweep use (developPixelSource decodes and
                   composites at full resolution). Queued, not called: this runs inside a
                   flush, which happens on folder change and on quit. The builder yields
                   while Develop is open, so an edit session is never interleaved. */
                QMetaObject::invokeMethod(this, [this, fPath]{
                    buildDevPreviews(QStringList(fPath), "flush");
                }, Qt::QueuedConnection);
            }

            return !thumbJpg.isEmpty() || !loupeJpg.isEmpty();
        });

    connect(developProperties, &DevelopProperties::devPreviewUpdated,
            this, &MW::devPreviewUpdated);

    connect(developProperties, &DevelopProperties::paramsChanged, this, &MW::developParamsChange);
    /* History hover preview: PROXY render only. developParamsChange would also arm the
       full-res settle render, and a cursor passing down the History list must not queue
       one heavy render per row -- the click (applyHistoryEntry) does the full path. */
    connect(developProperties, &DevelopProperties::historyPreviewChanged, this,
            [this]{ renderDevelopPreview(false); });
    /* The dock's "Edit: Raw / Embedded Preview" selector drives G::useRaw through the same path as
       the status-bar button (toggleUseRaw is a private slot, hence the signal hop). */
    connect(developProperties, &DevelopProperties::useRawRequested, this,
            [this](bool useRaw){ toggleUseRaw(useRaw ? Tog::on : Tog::off); });

    /* The dock's "Demosaic" combo selects the RAW decode engine (Apple Core Image vs in-house
       Winnow). Set G::decodeRawEngine, drop the current image's cached scene-linear WorkingImage so
       it re-decodes with the new engine, and re-render. NOTE (verify next session): the visible
       re-decode relies on renderDevelopPreview's raw re-decode path (gated on isFileRaw && useRaw). */
    connect(developProperties, &DevelopProperties::demosaicEngineChanged, this,
            [this](bool useApple){
#ifndef Q_OS_MAC
                /* The Apple engine is macOS-only and its decode branch is compiled out
                   here, so setting it off-mac would not merely be ignored: it matches
                   NEITHER engine test, which silently disables the winnow-only PMRID raw
                   denoise. Reachable off-mac by applying a preset made on a Mac. */
                if (useApple) {
                    useApple = false;
                    if (G::popup)
                        G::popup->showPopup("The Apple raw decoder is macOS only.<br>"
                                            "Using the Winnow decoder.", 4000);
                }
#endif
                G::decodeRawEngine = useApple ? G::DecodeRawEngine::appleDecodeRawEngine
                                              : G::DecodeRawEngine::winnowDecodeRawEngine;
                /* Clear the WHOLE WorkingImageCache, not just the current file: the
                   clean bases are engine-specific and ImageDecoder::load() now reuses
                   ANY cached scene-linear entry (the redundant-decode short-circuit), so
                   a stale non-current entry would otherwise be served after a switch. */
                WorkingImageCache::instance().clear();
                developParamsChange();
            });

    /* The dock's "auto run denoise" checkbox + "Run Denoise" button. When auto is off the
       heavy PMRID denoise runs only on the button (see the gated ensureRawDenoise call
       sites); the button forces a run regardless. */
    connect(developProperties, &DevelopProperties::autoRunDenoiseToggled,
            this, &MW::onAutoRunDenoiseToggled);
    connect(developProperties, &DevelopProperties::runRawDenoiseRequested,
            this, &MW::runRawDenoiseNow);
    connect(developProperties, &DevelopProperties::clearRawDenoiseRequested,
            this, &MW::clearRawDenoiseNow);

    /* Mask editing handshake: the dock activates a spatial mask tool and ImageView draws/edits its
       overlay, sending dragged geometry back to be persisted into the MaskComponent. */
    connect(developProperties, &DevelopProperties::maskEditBegin, imageView, &ImageView::beginMaskEdit);
    connect(developProperties, &DevelopProperties::maskEditBegin, this, &MW::onAiMaskEditBegin);
    connect(developProperties, &DevelopProperties::maskEditEnd,   imageView, &ImageView::endMaskEdit);
    /* Regenerative spot fill: arm/disarm the ImageView brush; stroke -> FillSpot. */
    connect(developProperties, &DevelopProperties::spotEditBegin, imageView, &ImageView::beginSpotEdit);
    connect(developProperties, &DevelopProperties::spotEditEnd,   imageView, &ImageView::endSpotEdit);
    connect(imageView, &ImageView::spotStrokeCommitted,
            developProperties, &DevelopProperties::onSpotStrokeCommitted);
    connect(developProperties, &DevelopProperties::spotPinsChanged,
            imageView, &ImageView::setSpotPins);
    connect(imageView, &ImageView::spotRemoveRequested,
            developProperties, &DevelopProperties::onSpotRemoveRequested);
    /* White-balance dropper: arm/disarm the ImageView pick mode; a click comes back as a
       normalized point for the dock to solve into a Kelvin/tint. */
    connect(developProperties, &DevelopProperties::wbDropperBegin,
            imageView, &ImageView::beginWbPick);
    connect(developProperties, &DevelopProperties::wbDropperEnd,
            imageView, &ImageView::endWbPick);
    connect(imageView, &ImageView::wbSampled,
            developProperties, &DevelopProperties::onWbSampled);
    connect(imageView, &ImageView::wbPickExited,
            developProperties, &DevelopProperties::cancelWbDropper);
    /* Detail 1:1 preview: the same arrangement as the dropper above -- the dock arms the
       loupe's pick mode, and a click comes back as a normalized point.
       onDetailPointPicked disarms and re-renders; the nudge is the drag INSIDE the
       preview, which MW resolves because it owns the image's orientation. */
    connect(developProperties, &DevelopProperties::detailPickBegin,
            imageView, &ImageView::beginDetailPick);
    connect(developProperties, &DevelopProperties::detailPickEnd,
            imageView, &ImageView::endDetailPick);
    connect(imageView, &ImageView::detailPointPicked,
            this, &MW::onDetailPointPicked);
    connect(imageView, &ImageView::detailPickExited,
            developProperties, &DevelopProperties::cancelDetailPick);
    connect(developProperties, &DevelopProperties::detailRoiNeeded,
            this, &MW::onDetailRoiNeeded);
    connect(developProperties, &DevelopProperties::detailPointNudged,
            this, &MW::onDetailPointNudged);
    /* The dropper / Auto WB need the pre-develop WorkingImage before the image has been
       edited, which for a display-referred file nothing has built yet. DIRECT (same
       thread), so the dock can use the result the moment emit returns. */
    connect(developProperties, &DevelopProperties::workingImageNeeded,
            this, &MW::ensureWorkingImageNow, Qt::DirectConnection);
    connect(imageView, &ImageView::spotToolExited, developProperties,
            [this]{ developProperties->onSpotToolToggled(false); });
    /* Whole-mask coverage tint: rebuild/clear when the mask selection changes
       (begin/end). MW composites the active scope's tools. These two are one-shot,
       so they stay direct. paramsChanged does NOT connect here -- it fires on every
       drag tick and the rebuild is two full mask composites, so it rides the
       coalescing proxy render timer below instead. */
    connect(developProperties, &DevelopProperties::maskEditBegin, this, &MW::updateMaskOverlayTint);
    connect(developProperties, &DevelopProperties::maskEditEnd,   this, &MW::updateMaskOverlayTint);
    /* Veil-only rebuild: the previewed combine op or the overlay colour changed, so the
       image itself is untouched (no re-render). */
    connect(developProperties, &DevelopProperties::maskOverlayRefreshRequested,
            this, &MW::updateMaskOverlayTint);
    /* View-only change (overlay grayscale): no veil rebuild, just a repaint. */
    connect(developProperties, &DevelopProperties::maskOverlayRepaintRequested,
            this, [this]{ imageView->viewport()->update(); });
    connect(imageView, &ImageView::maskGeometryChanged, developProperties, &DevelopProperties::setActiveMaskParams);
    connect(developProperties, &DevelopProperties::maskFeatherChanged, imageView, &ImageView::setMaskFeather);
    connect(developProperties, &DevelopProperties::maskInvertChanged, imageView, &ImageView::setMaskInverted);
    connect(developProperties, &DevelopProperties::maskRangeChanged, imageView, &ImageView::setMaskRangeParams);
    connect(developProperties, &DevelopProperties::maskBrushSettingsChanged, imageView, &ImageView::setMaskBrushSettings);
    connect(imageView, &ImageView::maskBrushSizeRequested, developProperties, &DevelopProperties::setActiveBrushSize);
    connect(imageView, &ImageView::maskFeatherRequested, developProperties, &DevelopProperties::setActiveMaskFeather);
    connect(imageView, &ImageView::maskBrushAutoMaskRequested, developProperties, &DevelopProperties::setActiveBrushAutoMask);
    /* Adjustment slider changed while a mask overlay is shown -> hide coverage tint. */
    connect(developProperties, &DevelopProperties::maskTintHideRequested,
            imageView, &ImageView::hideMaskTint);
    /* Scope selected -> un-hide the tint so that scope's combined mask shows. */
    connect(developProperties, &DevelopProperties::maskTintShowRequested,
            imageView, &ImageView::showMaskTint);
    /* Scope menu "Show mask overlay" <-> ImageView's tint state (also flipped by "O"). */
    connect(developProperties, &DevelopProperties::maskOverlayToggleRequested,
            this, &MW::toggleMaskOverlay);
    connect(imageView, &ImageView::maskTintVisibilityChanged,
            developProperties, &DevelopProperties::setMaskOverlayShown);
    /* Becoming visible must REBUILD. While the veil is hidden MW skips building it
       entirely, so whatever is stored is stale (or was never built for this mask) by the
       time "O" turns it back on. */
    connect(imageView, &ImageView::maskTintVisibilityChanged, this,
            [this](bool visible){ if (visible) updateMaskOverlayTint(); });
    /* A stroke starting/finishing flips what Opt means (erase vs subtract), so
       re-read the combine modifiers on each edge. */
    connect(imageView, &ImageView::maskStrokeStateChanged, this,
            [this](bool){ syncPendingMaskOp(); });
    /* Opt held during a Detail-panel Masking drag -> show the sharpening gate in grayscale
       over the photo (and drop it when the key or the handle is released). */
    connect(developProperties, &DevelopProperties::sharpenMaskPreviewChanged,
            this, [this](bool){ updateSharpenMaskPreview(); });
    /* A shaping action (stroke / handle drag) began: the modifier held at that instant
       becomes what the submask DOES, and stays put when the key is released. */
    connect(imageView, &ImageView::maskOpActionStarted,
            developProperties, &DevelopProperties::latchMaskOp);

    /* Develop preview render timers (see MW::developParamsChange). The proxy timer
       coalesces a burst of slider ticks into one screen-resolution render; the full-res
       timer fires once the drag settles for the crisp final image. */
    developProxyRenderTimer = new QTimer(this);
    developProxyRenderTimer->setSingleShot(true);
    /* One tick = one veil rebuild + one proxy render, in that order (the order the two
       direct connections used to run in), so a drag repaints the loupe ONCE per turn
       instead of twice and the veil composite is not re-run per mouse-move event. */
    connect(developProxyRenderTimer, &QTimer::timeout, this, [this]{
        updateMaskOverlayTint();
        renderDevelopPreview(false);
    });
    developFullResTimer = new QTimer(this);
    developFullResTimer->setSingleShot(true);
    connect(developFullResTimer, &QTimer::timeout, this, [this]{ renderDevelopFullResAsync(); });

    /* Dedicated single-thread pool that DRIVES the full-res settle render off the GUI thread (it
       blocks on the global pool's per-row parallelism internally, so it must not occupy a global-
       pool slot itself). One driver thread is enough: only one full-res render runs at a time. */
    developRenderPool = new QThreadPool(this);
    developRenderPool->setMaxThreadCount(1);

    /* The interactive proxy render gets its OWN single thread. Sharing developRenderPool
       would let a settled full-res render (~0.4-1.3 s) block every interactive tick
       behind it, which is the opposite of the point. Both only DRIVE the render -- the
       per-row parallelism inside runs on the global pool. */
    developProxyPool = new QThreadPool(this);
    developProxyPool->setMaxThreadCount(1);

    developDockTabText = "Develop";
    dockTextNames << developDockTabText;
    developDock = new DockWidget(developDockTabText, "DevelopDock", this);  // Develop
    developDock->setObjectName("DevelopDock");
    developDockFeatures = developDock->features();   // remember for setDevelopPanelEnabled()

    /* Dock content = live scopes strip (histogram + vectorscope) pinned above the property tree.
       The scopes keep a fixed height; developProperties takes the remaining (stretch) space.
       Visibility is user-toggled from the action row above and persisted. */
    developScopesVisible = settings->value("Develop/scopesVisible", true).toBool();
    /* Which scopes the strip shows (picked from the strip's right-click menu or the
       Develop menu's Scopes submenu: both / histogram / vectorscope). */
    developScopesLayout = settings->value("Develop/scopesLayout", ScopesView::Both).toInt();
    if (developScopesLayout < ScopesView::Both ||
        developScopesLayout > ScopesView::VectorscopeOnly)
        developScopesLayout = ScopesView::Both;
    G::autoRunDenoise = settings->value("Develop/autoRunDenoise", true).toBool();
    QWidget *developContainer = new QWidget(developDock);
    QVBoxLayout *developContainerLayout = new QVBoxLayout(developContainer);
    developContainerLayout->setContentsMargins(0, 0, 0, 0);
    developContainerLayout->setSpacing(0);
    /* Action row: the tool buttons (Scopes, Crop, Spot, Preset, Export) live here, in
       their own centered row directly below the dock title bar, instead of competing
       with the title text for space in the title bar itself. Only the ? and X buttons
       remain in the title bar. The buttons are created further down (with the rest of
       the title bar) and added to developActionLayout, which is flanked by stretches so
       the group stays centered as the dock is resized. */
    developActionRow = new QWidget(developContainer);
    developActionRow->setObjectName("developActionRow");
    /* The same bottom separator every Develop panel carries (G::panelBorderHeight in
       G::tabWidgetBorderColor); the ID selector keeps it off the child buttons and the
       extra bottom margin reserves the rule's space. */
    developActionRow->setStyleSheet(
        QString("QWidget#developActionRow { border-bottom: %1px solid %2; }")
            .arg(G::panelBorderHeight).arg(G::tabWidgetBorderColor.name()));
    QHBoxLayout *developActionLayout = new QHBoxLayout(developActionRow);
    developActionLayout->setContentsMargins(0, 0, 0, 8 + G::panelBorderHeight);
    developActionLayout->setSpacing(0);
    developActionLayout->addStretch(1);
    developContainerLayout->addWidget(developActionRow);

    /* Alert rows, directly BELOW the action row: one row per condition the user needs to
       know about before editing (multiple images selected, a video that Develop cannot
       touch, ...). Each is bright red text on the panel background rather than a filled
       banner -- the panel reads as one surface and the colour alone carries the alarm.
       setDevelopAlerts fills the rows; the container hides itself when there is nothing
       to say. It is NOT greyed with the rest of the panel (setDevelopPanelEnabled greys
       the panel and the action row individually) -- an alert must stay readable
       precisely when the panel is disabled. */
    developAlertRows = new QWidget(developContainer);
    developAlertRows->setObjectName("developAlertRows");
    /* The app stylesheet fills a plain QWidget opaquely, which would paint a slab behind
       the alerts instead of letting the panel background show through (the same trap
       developScroll's viewport hits below). */
    developAlertRows->setStyleSheet("QWidget#developAlertRows { background: transparent; }");
    developAlertRowsLayout = new QVBoxLayout(developAlertRows);
    developAlertRowsLayout->setContentsMargins(6, 4, 6, 4);
    developAlertRowsLayout->setSpacing(2);
    developAlertRows->setVisible(false);
    developContainerLayout->addWidget(developAlertRows);

    scopesView = new ScopesView(developContainer);
    scopesView->setScopeLayout(static_cast<ScopesView::ScopeLayout>(developScopesLayout));
    scopesView->setVisible(developScopesVisible);
    developContainerLayout->addWidget(scopesView);
    /* Transform (crop + perspective) strip sits directly below the scopes and above the tree.
       ALWAYS starts hidden on launch (its visibility is not restored): a visible panel means the
       crop tool is active, which should never be the case before the user asks for it. */
    developTransformVisible = false;
    transformPanel = new TransformPanel(developContainer, settings);
    transformPanel->setVisible(developTransformVisible);
    developContainerLayout->addWidget(transformPanel);
    /* Drive the ImageView crop overlay from the Transform panel: aspect changes + the lock toggle
       re-fit the live crop frame. */
    connect(transformPanel, &TransformPanel::aspectChanged, this,
            [this](const QString &, double ratio){
                if (imageView) imageView->setCropAspect(ratio, transformPanel->isAspectLocked(),
                                                        transformPanel->isAspectFlipped());
            });
    connect(transformPanel, &TransformPanel::aspectLockToggled, this,
            [this](bool locked){
                if (imageView) imageView->setCropAspect(transformPanel->aspectRatio(), locked,
                                                        transformPanel->isAspectFlipped());
            });
    connect(transformPanel, &TransformPanel::aspectFlipToggled, this,
            [this](bool flipped){
                if (imageView) imageView->setCropAspectFlip(flipped);
            });
    /* Rectify button: store the 4-point warp + suggested crop into the image geometry and show the
       corrected canvas (two-phase warp; non-destructive). */
    connect(transformPanel, &TransformPanel::rectifyRequested, this, &MW::rectifyDevelopCrop);
    /* Warp mode commit: Enter/Return or a double-click on the traced quad (ImageView overrides those
       keys/clicks while warping) rectifies it. */
    connect(imageView, &ImageView::warpCommitRequested, this, &MW::rectifyDevelopCrop);
    /* Transform mode toggle (Crop / Level / Warp): arm the matching ImageView tool. */
    connect(transformPanel, &TransformPanel::modeChanged, this, &MW::setDevelopTransformMode);
    /* A level line drawn on the image adds to the straighten. */
    connect(imageView, &ImageView::levelAngleChanged, this, &MW::applyDevelopLevel);
    /* An angle typed into the Level field sets the straighten directly (absolute). */
    connect(transformPanel, &TransformPanel::levelAngleEntered, this, &MW::setDevelopLevelAngle);
    /* Per-row reset: clear just the crop / straighten / warp contribution. */
    connect(transformPanel, &TransformPanel::resetModeRequested, this, &MW::resetDevelopTransformMode);
    /* Transform Preview eye: a LIVE result toggle while the crop tool is active. ON = commit the
       overlay's crop, drop the overlay and render the cropped/warped RESULT; OFF = back to full-frame
       editing with the overlay. (The panel is only visible while crop-editing, so that is the only
       case to handle.) */
    connect(transformPanel, &TransformPanel::previewToggled, this, [this](bool shown){
        if (!imageView || !developProperties || !developCropEditing) return;
        developCropShowResult = shown;
        if (shown) {
            const QRectF crop = imageView->cropRect();
            Geometry g = developProperties->currentGeometry();
            g.cropX = crop.x(); g.cropY = crop.y(); g.cropW = crop.width(); g.cropH = crop.height();
            developProperties->setCurrentGeometry(g);
            imageView->endCropEdit();               // drop the overlay, restore the normal view
            renderDevelopPreview(false);            // geometry applied -> cropped result
        }
        else {
            renderDevelopPreview(false);            // full frame (geometry suppressed) for the overlay
            const Geometry g = developProperties->currentGeometry();
            imageView->beginCropEdit(transformPanel->aspectRatio(),
                                     transformPanel->isAspectLocked(),
                                     transformPanel->isAspectFlipped(),
                                     QRectF(g.cropX, g.cropY, g.cropW, g.cropH));
        }
    });
    /* Transform [X]: close the panel exactly as the action-row button / "R" does --
       commit the crop session, hide the panel and update the action/button state. */
    connect(transformPanel, &TransformPanel::closeRequested, this, [this]{
        if (developTransformVisible) toggleDevelopTransform();
    });
    /* Transform Reset: clear crop/straighten/warp back to identity (destructive) and
       return to editing on the full frame. */
    connect(transformPanel, &TransformPanel::resetRequested, this, [this]{
        if (!developProperties) return;
        developProperties->setCurrentGeometry(Geometry());      // identity
        transformPanel->setAspectAsShot();      // free aspect, else full frame re-fits it
        transformPanel->setLevelAngle(0.0);     // clear the straighten field
        developCropShowResult = false;
        transformPanel->setPreviewShown(false);
        renderDevelopPreview(false);
        if (developCropEditing && imageView) {
            imageView->beginCropEdit(transformPanel->aspectRatio(),
                                     transformPanel->isAspectLocked(),
                                     transformPanel->isAspectFlipped(), QRectF(0, 0, 1, 1));
            /* beginCropEdit re-arms the crop cursor; restore the tool for the current row
               (Level/Warp) so resetting while straightening keeps the level cursor. */
            setDevelopTransformMode(transformPanel->mode());
        }
    });
    /* Fill Replace (spot/fill/object heal) strip below the Transform panel, above the
       scope header. ALWAYS starts hidden: a visible panel means the replace tool is
       armed, which should never be the case before the user asks (spot button or "F"). */
    replacePanel = new ReplacePanel(developContainer, settings);
    replacePanel->setVisible(false);
    developContainerLayout->addWidget(replacePanel);
    /* Panel visibility tracks the armed state (DevelopProperties owns it): arming shows
       the panel and pushes the current mode to the loupe capture; Escape/spot-button
       disarm hides it through the same signal. */
    connect(developProperties, &DevelopProperties::spotActiveChanged, this, [this](bool active){
        if (!replacePanel) return;
        /* Fill/Object modes shelved (G::useReplaceFillModes): the panel stays hidden
           and arming always captures in Spot mode -- spot cleanup only. */
        replacePanel->setVisible(active && G::useReplaceFillModes);
        if (active) {
            if (imageView) imageView->setSpotReplaceMode(
                G::useReplaceFillModes ? replacePanel->mode() : int(ReplacePanel::SpotMode));
            replacePanel->setPreviewShown(developProperties->isSpotsShown());
            if (developDock) { developDock->setVisible(true); developDock->raise(); }
        }
        else if (!developProperties->isSpotsShown()) {
            /* Never leave the heals silently bypassed with the eye out of reach. */
            developProperties->setSpotsShown(true);
            replacePanel->setPreviewShown(true);
            developParamsChange();
        }
    });
    /* Mode row picked in the panel -> capture behaviour + stored kind. (The loupe's S/F/O
       mode keys were retired when S became the Develop mode spot-tool toggle; the panel's
       own S/F/O still work while it has focus, and it is only shown when the Fill/Object
       modes are un-shelved.) */
    connect(replacePanel, &ReplacePanel::modeChanged, this, [this](int mode){
        if (imageView) imageView->setSpotReplaceMode(mode);
    });
    /* Preview eye: render with/without every committed heal (non-destructive bypass). */
    connect(replacePanel, &ReplacePanel::previewToggled, this, [this](bool shown){
        if (!developProperties) return;
        developProperties->setSpotsShown(shown);
        developParamsChange();
    });
    connect(replacePanel, &ReplacePanel::tipsRequested, this, [this]{
        if (G::popup) G::popup->showPopup(
            "<b>Fill Replace</b><br>"
            "Spot: click a blemish to heal it with cloned surroundings.<br>"
            "Fill: paint the area to replace (Opt/Alt erases), then Enter fills it "
            "with cloned surroundings; Esc clears the paint.<br>"
            "Object: brush over an object to remove it with a regenerative fill.<br><br>"
            "[ and ] resize the brush. Click a pin to remove a heal. Esc exits.", 6000);
    });

    /* The scope dropdown + scope-action buttons live in a gradient header band ABOVE the
       property tree (replacing the old in-tree Scopes header). DevelopProperties drives
       it and handles its signals; the collapse arrow hides/shows the tree. */
    /* G::useScopeHeaderLab swaps in the experimental ScopeHeaderLab (scratch copy for
       reshaping the Scope section); both satisfy ScopeHeaderBase so bindScopeHeader is
       type-agnostic. Retire the flag + lab once the design is copied back. */
    /* Lab UI: the raw-decode controls move out of the Global "Core" tree rows into a
       RawPanel ABOVE the Scopes list (shown for raw files only; DevelopProperties drives
       its visibility + state). Only built under the flag; legacy keeps the Core rows. */
    if (G::useScopeHeaderLab) {
        RawPanel *rawPanel = new RawPanel(developContainer, settings);
        rawPanel->setVisible(false);                 // syncRawPanel reveals it for raw
        developContainerLayout->addWidget(rawPanel);
        developProperties->bindRawPanel(rawPanel);
    }
    ScopeHeaderBase *developScopeHeader = G::useScopeHeaderLab
            ? static_cast<ScopeHeaderBase *>(new ScopeHeaderLab(developContainer))
            : static_cast<ScopeHeaderBase *>(new ScopeHeader(developContainer));
    developProperties->bindScopeHeader(developScopeHeader);
    /* The mask editor and the adjustment tree are NESTED under the SELECTED scope's row
       in the list (setRowDetail), not stacked below the whole list: a mask's submasks and
       the Basic/Color/... sections belong to the scope they edit. Both are created here
       and owned by the panel hierarchy; DevelopProperties shows the mask editor for mask
       scopes and hides it on Global (syncMaskPanel).

       Because the tree is nested it can no longer scroll itself: it fits its content
       height and the whole scope block scrolls inside a QScrollArea. Everything above
       (action row, scopes strip, Transform, Replace, Raw) stays pinned. */
    if (G::useScopeHeaderLab) {
        ScopeHeaderLab *lab = static_cast<ScopeHeaderLab *>(developScopeHeader);

        MaskPanel *maskPanel = new MaskPanel(developContainer);
        maskPanel->setVisible(false);
        lab->setRowDetail(maskPanel, ScopeHeaderLab::MaskDetail,
                          ScopeHeaderLab::kDetailIndent);
        developProperties->bindMaskPanel(maskPanel);

        /* Indent 0 for the tree: its left edge then coincides with the scope list's, so
           the containment rail it paints continues the list's rail at the same x. The
           section headers carry their own indent instead (addHeader/UR_ExtraIndent), so
           the sliders keep their full width. */
        developProperties->setFitToContentHeight(true);
        lab->setRowDetail(developProperties, ScopeHeaderLab::EditsDetail, 0);

        QScrollArea *developScroll = new QScrollArea(developContainer);
        developScroll->setObjectName("developScroll");
        developScroll->setFrameShape(QFrame::NoFrame);
        developScroll->setWidgetResizable(true);
        developScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        /* The app stylesheet fills a plain QWidget opaquely, which would paint over the
           dock background (the same trap ScopeHeaderLab's rowsContainer hit). */
        developScroll->viewport()->setAutoFillBackground(false);
        QWidget *developScrollBody = new QWidget(developScroll);
        developScrollBody->setAttribute(Qt::WA_TranslucentBackground);
        QVBoxLayout *developScrollLayout = new QVBoxLayout(developScrollBody);
        developScrollLayout->setContentsMargins(0, 0, 0, 0);
        developScrollLayout->setSpacing(0);
        developScopeHeader->setParent(developScrollBody);
        developScrollLayout->addWidget(developScopeHeader);
        developScrollLayout->addStretch(1);
        developScroll->setWidget(developScrollBody);
        developContainerLayout->addWidget(developScroll, 1);
    }
    else {
        developContainerLayout->addWidget(developScopeHeader);
        developContainerLayout->addWidget(developProperties, 1);
    }
    developDock->setWidget(developContainer);
    /* The tone-region slider under the histogram drives the active scope's tone-split params. */
    developProperties->bindToneSlider(scopesView->toneRegionSlider());

    /* Loupe cursor readout: hovering the image marks the pixel's value on the scopes. */
    connect(imageView, &ImageView::cursorImagePos, this, &MW::onImageCursorPos);
    connect(imageView, &ImageView::cursorLeftImage, this,
            [this]{ if (scopesView) scopesView->clearMarker(); });

    /* Vectorscope zoom + skin-tone line (right-click menu): restore saved choices and persist. */
    scopesView->setVectorscopeZoom(settings->value("Develop/vectorscopeZoom", 1.0).toDouble());
    connect(scopesView, &ScopesView::vectorscopeZoomChanged, this,
            [this](double z){ settings->setValue("Develop/vectorscopeZoom", z); });
    scopesView->setVectorscopeSkinLine(settings->value("Develop/vectorscopeSkinLine", false).toBool());
    connect(scopesView, &ScopesView::vectorscopeSkinLineChanged, this,
            [this](bool on){ settings->setValue("Develop/vectorscopeSkinLine", on); });
    /* The strip's [X] (top right corner): hide the scopes (the action-row button or the
       Scopes menu brings them back with the same layout). */
    connect(scopesView, &ScopesView::closeRequested, this, &MW::closeDevelopScopes);
    /* Right-click any scope: the scope's own items (if any) plus the scopes-layout menu. */
    connect(scopesView, &ScopesView::menuRequested, this, &MW::showDevelopScopesMenu);
    developDock->setFloating(false);
    developDock->setVisible(true);
    // prevent MW splitter resizing developDock so the header - and + buttons stay visible
    developDock->setMinimumWidth(275);
    connect(developDock, &DockWidget::focus, this, &MW::focusOnDock);
    connect(developDock, &QDockWidget::visibilityChanged, this, &MW::developDockVisibilityChange);

    // customize the developDock titlebar
    QHBoxLayout *developTitleLayout = new QHBoxLayout();
    developTitleLayout->setContentsMargins(0, 0, 0, 0);
    developTitleLayout->setSpacing(0);

    developTitleBar = new DockTitleBar("Develop Editor", developTitleLayout);
    /* No rule under the title: the action row just below it carries the panel separator,
       and two rules that close together read as a mistake. */
    developTitleBar->setBottomBorderVisible(false);
    developDock->setTitleBarWidget(developTitleBar);
    developTitleBar->setToolTip(dockTabToolTip(developDockTabText));

    // show/hide the histogram + vectorscope scopes strip
    developScopesBtn = new BarBtn();
    developScopesBtn->setIcon(":/images/icon16/graphic.png", G::iconOpacity);
    developScopesBtn->setToolTip("Show or hide the histogram and vectorscope.  "
                                 "Right click the scopes (or use Develop > Scopes) to "
                                 "choose which scopes are shown.");
    developScopesBtn->setActive(developScopesVisible);
    connect(developScopesBtn, &BarBtn::clicked, this, &MW::toggleDevelopScopes);
    developActionLayout->addWidget(developScopesBtn);
    developActionLayout->addSpacing(10);

    // show/hide the Transform (crop + perspective) panel
    developTransformBtn = new BarBtn();
    developTransformBtn->setIcon(":/images/icon16/rectangle.png", G::iconOpacity);
    developTransformBtn->setToolTip("Show or hide the Transform (crop) panel  (R)");
    developTransformBtn->setActive(developTransformVisible);
    connect(developTransformBtn, &BarBtn::clicked, this, &MW::toggleDevelopTransform);
    developActionLayout->addWidget(developTransformBtn);
    developActionLayout->addSpacing(10);

    /* Spot-removal tool (regenerative fill): brush over a blemish to heal it; heals are
       recorded in the pinned "Fill" scope. DevelopProperties owns the armed state and
       drives the icon back via spotActiveChanged (full opacity armed, dimmed off). */
    developSpotBtn = new BarBtn();
    developSpotBtn->setIcon(":/images/icon16/spot.png", G::iconOpacity);
    developSpotBtn->setToolTip("Spot removal: click a blemish to heal it  (S)");
    connect(developSpotBtn, &BarBtn::clicked, this, &MW::toggleDevelopReplace);
    connect(developProperties, &DevelopProperties::spotActiveChanged, developSpotBtn,
            [this](bool active){
        developSpotBtn->setIcon(":/images/icon16/spot.png", active ? 1.0 : G::iconOpacity);
        developSpotBtn->setActive(active);
    });
    developActionLayout->addWidget(developSpotBtn);
    developActionLayout->addSpacing(10);

    /* Mask overlay tint on/off ("O"). The button has no glyph: it IS the swatch -- its
       icon is filled with the current overlay colour (G::maskOverlayColor, picked in the
       Mask panel), so the row shows at a glance which colour the veil speaks and whether
       it is on (blue active border, same as the other action buttons). Repainted by
       refreshDevelopMaskTintBtn on both the visibility and the colour signals. */
    developMaskTintBtn = new BarBtn();
    developMaskTintBtn->setToolTip("Show or hide the mask overlay tint  (O)\n"
                                   "Right-click: overlay colour / grayscale background\n"
                                   "Note: the Global scope has no mask, so no overlay.");
    connect(developMaskTintBtn, &BarBtn::clicked, this, &MW::toggleMaskOverlay);
    /* Right-click picks the colour and flips the grayscale background -- the Mask panel's
       chips, reachable without opening (or even having) a mask. */
    developMaskTintBtn->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(developMaskTintBtn, &BarBtn::customContextMenuRequested,
            this, &MW::showDevelopMaskTintMenu);
    connect(imageView, &ImageView::maskTintVisibilityChanged, this,
            [this](bool){ refreshDevelopMaskTintBtn(); });
    /* A mask tool was expanded/collapsed or a committed-mask tint appeared/vanished: the
       swatch shows hollow + extra-dim while there is no mask to tint (Global scope). */
    connect(imageView, &ImageView::maskTintAvailabilityChanged, this,
            [this](bool){ refreshDevelopMaskTintBtn(); });
    /* A swatch in the Mask panel recoloured the overlay. */
    connect(developProperties, &DevelopProperties::maskOverlayRefreshRequested, this,
            &MW::refreshDevelopMaskTintBtn);
    refreshDevelopMaskTintBtn();
    developActionLayout->addWidget(developMaskTintBtn);
    developActionLayout->addSpacing(10);

    /* Preset: show / raise the Presets dock, where a click applies a saved preset (P).
       Saving one is Cmd+Shift+N, the dock context menu, or the [+] in that dock's title
       bar. The colour-wheel glyph matches the Scope / Transform action-row button style,
       and like them it carries the blue active border while its panel is the front tab
       (driven from presetsDockVisibilityChange). */
    developPresetBtn = new BarBtn();
    developPresetBtn->setIcon(":/images/icon16/colorwheel.png", G::iconOpacity);
    developPresetBtn->setToolTip("Develop presets: apply a saved preset  (P)");
    connect(developPresetBtn, &BarBtn::clicked, this, &MW::showPresetsDock);
    developActionLayout->addWidget(developPresetBtn);
    developActionLayout->addSpacing(10);

    /* Export the developed image. STUB for now (MW::developExport is not built yet). */
    BarBtn *developExportBtn = new BarBtn();
    developExportBtn->setIcon(":/images/icon16/lightning.png", G::iconOpacity);
    developExportBtn->setToolTip("Export the developed image  (X)");
    connect(developExportBtn, &BarBtn::clicked, this, &MW::developExport);
    developActionLayout->addWidget(developExportBtn);

    // close the centered group of action buttons
    developActionLayout->addStretch(1);

    // question mark button
    BarBtn *devQuestionBtn = new BarBtn();
    devQuestionBtn->setIcon(":/images/icon16/questionmark.png", G::iconOpacity);
    devQuestionBtn->setToolTip("How this works: develop tips");
    connect(devQuestionBtn, &BarBtn::clicked, developProperties, &DevelopProperties::howThisWorks);
    developTitleLayout->addWidget(devQuestionBtn);

    // Spacer
    developTitleLayout->addSpacing(10);

    // collapse/expand body button
    if (G::useDWCollapse) {
        BarBtn *developCollapseBtn = new BarBtn();
        developCollapseBtn->setIcon(":/images/icon16/collapse.png", G::iconOpacity);
        developCollapseBtn->setToolTip("Collapse panel.");
        connect(developCollapseBtn, &BarBtn::clicked, developDock, &DockWidget::toggleCollapsed);
        connect(developDock, &DockWidget::collapsedChanged, developCollapseBtn, [developCollapseBtn](bool c){
            developCollapseBtn->setIcon(c ? ":/images/icon16/expand.png" : ":/images/icon16/collapse.png", G::iconOpacity);
            developCollapseBtn->setToolTip(c ? "Expand panel." : "Collapse panel.");
        });
        developTitleLayout->addWidget(developCollapseBtn);

        // Spacer
        developTitleLayout->addSpacing(10);
    }

    // close button
    BarBtn *developCloseBtn = new BarBtn();
    developCloseBtn->setIcon(":/images/icon16/close.png", G::iconOpacity);
    developCloseBtn->setToolTip("Hide the Develop Panel");
    connect(developCloseBtn, &BarBtn::clicked, this, &MW::closeDevelopDock);
    developTitleLayout->addWidget(developCloseBtn);

    // Spacer
    developTitleLayout->addSpacing(5);
}

void MW::createHistoryDock()
{
/*
    The Develop edit history (Lightroom's History panel): every committed develop action,
    newest first. Hovering a row previews that state in the loupe; clicking it reverts the
    image to it. The timeline lives in DevelopProperties (which owns the per-image
    EditStack it snapshots), so this dock is a pure view -- created AFTER
    createDevelopDock so developProperties exists to bind to.

    Session scoped: the sidecar stores only the current EditStack, so history is rebuilt
    as the user edits and discarded on quit.
*/
    if (G::isLogger) G::log("MW::createHistoryDock");

    historyDockTabText = "History";
    dockTextNames << historyDockTabText;
    historyDock = new DockWidget(historyDockTabText, "HistoryDock", this);
    historyDock->setObjectName("HistoryDock");

    historyView = new HistoryView(historyDock);
    historyDock->setWidget(historyView);
    if (developProperties) developProperties->bindHistoryView(historyView);

    historyDock->setFloating(false);
    historyDock->setVisible(false);          // shown with the Develop dock
    connect(historyDock, &DockWidget::focus, this, &MW::focusOnDock);
    connect(historyDock, &QDockWidget::visibilityChanged,
            this, &MW::historyDockVisibilityChange);

    // customize the historyDock titlebar
    QHBoxLayout *historyTitleLayout = new QHBoxLayout();
    historyTitleLayout->setContentsMargins(0, 0, 0, 0);
    historyTitleLayout->setSpacing(0);
    historyTitleBar = new DockTitleBar("Develop History", historyTitleLayout);
    historyDock->setTitleBarWidget(historyTitleBar);
    historyTitleBar->setToolTip(dockTabToolTip(historyDockTabText));

    // question mark button
    BarBtn *historyQuestionBtn = new BarBtn();
    historyQuestionBtn->setIcon(":/images/icon16/questionmark.png", G::iconOpacity);
    historyQuestionBtn->setToolTip("How this works: develop history tips");
    connect(historyQuestionBtn, &BarBtn::clicked, this, [this]{
        if (G::popup) G::popup->showPopup(
            "<b>Develop History</b><br>"
            "Every develop action for this image, newest first.<br>"
            "Hover an entry to preview that state; click it to go back to it.<br>"
            "Editing from an earlier entry discards the entries after it.<br><br>"
            "History is per image and lasts for this session -- the sidecar keeps the "
            "current state, not the steps.", 6000);
    });
    historyTitleLayout->addWidget(historyQuestionBtn);

    // Spacer
    historyTitleLayout->addSpacing(10);

    // collapse/expand body button
    if (G::useDWCollapse) {
        BarBtn *historyCollapseBtn = new BarBtn();
        historyCollapseBtn->setIcon(":/images/icon16/collapse.png", G::iconOpacity);
        historyCollapseBtn->setToolTip("Collapse panel.");
        connect(historyCollapseBtn, &BarBtn::clicked, historyDock, &DockWidget::toggleCollapsed);
        connect(historyDock, &DockWidget::collapsedChanged, historyCollapseBtn,
                [historyCollapseBtn](bool c){
            historyCollapseBtn->setIcon(c ? ":/images/icon16/expand.png"
                                          : ":/images/icon16/collapse.png", G::iconOpacity);
            historyCollapseBtn->setToolTip(c ? "Expand panel." : "Collapse panel.");
        });
        historyTitleLayout->addWidget(historyCollapseBtn);

        // Spacer
        historyTitleLayout->addSpacing(10);
    }

    // close button
    BarBtn *historyCloseBtn = new BarBtn();
    historyCloseBtn->setIcon(":/images/icon16/close.png", G::iconOpacity);
    historyCloseBtn->setToolTip("Hide the History Panel");
    connect(historyCloseBtn, &BarBtn::clicked, this, &MW::closeHistoryDock);
    historyTitleLayout->addWidget(historyCloseBtn);

    // Spacer
    historyTitleLayout->addSpacing(5);
}

void MW::createPresetsDock()
{
/*
    The saved develop presets (Lightroom's Presets panel): user-defined, named develop
    recipes. Hovering a row previews that preset applied to the current image; clicking it
    applies it to the ACTIVE scope. The store lives in DevelopProperties (QSettings under
    "Develop Presets"), so this dock is a pure view -- created AFTER createDevelopDock so
    developProperties exists to bind to.

    Unlike History, presets are PERSISTENT: they outlive the session and are not tied to
    any one image.
*/
    if (G::isLogger) G::log("MW::createPresetsDock");

    presetsDockTabText = "Presets";
    dockTextNames << presetsDockTabText;
    presetsDock = new DockWidget(presetsDockTabText, "PresetsDock", this);
    presetsDock->setObjectName("PresetsDock");

    presetsView = new PresetsView(presetsDock);
    presetsDock->setWidget(presetsView);
    if (developProperties) developProperties->bindPresetsView(presetsView);

    presetsDock->setFloating(false);
    presetsDock->setVisible(false);          // shown with the Develop dock
    connect(presetsDock, &DockWidget::focus, this, &MW::focusOnDock);
    connect(presetsDock, &QDockWidget::visibilityChanged,
            this, &MW::presetsDockVisibilityChange);
    /* Clicking a sibling TAB does not change any dock's isVisible(), so the title-bar
       button's border has to learn about tab switches from here. */
    connect(this, &QMainWindow::tabifiedDockWidgetActivated,
            this, [this](QDockWidget *){ updateDevelopPresetBtn(); });

    // customize the presetsDock titlebar
    QHBoxLayout *presetsTitleLayout = new QHBoxLayout();
    presetsTitleLayout->setContentsMargins(0, 0, 0, 0);
    presetsTitleLayout->setSpacing(0);
    presetsTitleBar = new DockTitleBar("Develop Presets", presetsTitleLayout);
    presetsDock->setTitleBarWidget(presetsTitleBar);
    presetsTitleBar->setToolTip(dockTabToolTip(presetsDockTabText));

    /* New preset. The same flow as Cmd+Shift+N, put where the presets are so it can be
       found without knowing the shortcut. */
    BarBtn *presetsNewBtn = new BarBtn();
    presetsNewBtn->setIcon(":/images/icon16/new.png", G::iconOpacity);
    presetsNewBtn->setToolTip("Create a develop preset from this image  (Cmd+Shift+N)");
    connect(presetsNewBtn, &BarBtn::clicked, this, &MW::developSavePreset);
    presetsTitleLayout->addWidget(presetsNewBtn);

    // Spacer
    presetsTitleLayout->addSpacing(10);

    // question mark button
    BarBtn *presetsQuestionBtn = new BarBtn();
    presetsQuestionBtn->setIcon(":/images/icon16/questionmark.png", G::iconOpacity);
    presetsQuestionBtn->setToolTip("How this works: develop preset tips");
    connect(presetsQuestionBtn, &BarBtn::clicked, this, [this]{
        if (G::popup) G::popup->showPopup(
            "<b>Develop Presets</b><br>"
            "Your saved develop recipes. Hover one to preview it on this image; "
            "click it to apply it.<br>"
            "A preset holds only the settings you ticked when you saved it, so "
            "applying it leaves everything else alone.<br>"
            "It is applied to the scope selected in the Develop panel.<br><br>"
            "Click + (or Cmd+Shift+N) to make one from the current image. "
            "Right-click a preset to update, rename or delete it.<br><br>"
            "For a one-off, skip the preset: Cmd+Opt+C copies the settings you tick "
            "and Cmd+Opt+V pastes them onto another image.", 7000);
    });
    presetsTitleLayout->addWidget(presetsQuestionBtn);

    // Spacer
    presetsTitleLayout->addSpacing(10);

    // collapse/expand body button
    if (G::useDWCollapse) {
        BarBtn *presetsCollapseBtn = new BarBtn();
        presetsCollapseBtn->setIcon(":/images/icon16/collapse.png", G::iconOpacity);
        presetsCollapseBtn->setToolTip("Collapse panel.");
        connect(presetsCollapseBtn, &BarBtn::clicked, presetsDock, &DockWidget::toggleCollapsed);
        connect(presetsDock, &DockWidget::collapsedChanged, presetsCollapseBtn,
                [presetsCollapseBtn](bool c){
            presetsCollapseBtn->setIcon(c ? ":/images/icon16/expand.png"
                                          : ":/images/icon16/collapse.png", G::iconOpacity);
            presetsCollapseBtn->setToolTip(c ? "Expand panel." : "Collapse panel.");
        });
        presetsTitleLayout->addWidget(presetsCollapseBtn);

        // Spacer
        presetsTitleLayout->addSpacing(10);
    }

    // close button
    BarBtn *presetsCloseBtn = new BarBtn();
    presetsCloseBtn->setIcon(":/images/icon16/close.png", G::iconOpacity);
    presetsCloseBtn->setToolTip("Hide the Presets Panel");
    connect(presetsCloseBtn, &BarBtn::clicked, this, &MW::closePresetsDock);
    presetsTitleLayout->addWidget(presetsCloseBtn);

    // Spacer
    presetsTitleLayout->addSpacing(5);
}

void MW::setDevelopPanelEnabled(bool visible, bool usable)
{
    if (G::isLogger) G::log("MW::setDevelopPanelEnabled");
    if (!developDock) return;
    /* VISIBLE governs whether the Develop tool is on screen at all; USABLE governs
       whether its controls respond. They differ for a video selection, which cannot be
       developed: the docks stay up (greyed) so the alert rows can explain why, instead of
       the whole tool silently vanishing the moment a video is selected.
       NOTE the dock itself is NOT disabled for the unusable case -- a disabled parent
       greys every child, the alerts included, and setEnabled() on a child cannot override
       a disabled ancestor. Each piece is greyed individually instead: the property panel
       (which reaches its own ScopeHeader + RawPanel), the action row, and the Transform
       and Fill Replace strips. */
    developDock->setEnabled(visible);
    /* setEnabled() greys the frame but does NOT stop a title-bar double-click from
       floating the dock (or a drag from moving it) -- that is governed by features(),
       not enabled state. So strip the features while disabled and restore the captured
       set when on. */
    developDock->setFeatures(visible ? developDockFeatures
                                     : QDockWidget::NoDockWidgetFeatures);
    const bool live = visible && usable;
    if (developProperties) developProperties->setPanelEnabled(live);
    if (developActionRow) developActionRow->setEnabled(live);
    /* Transform and Fill Replace are SIBLINGS of the property panel inside the dock (they
       are pinned strips, not part of the tree), so setPanelEnabled does not reach them --
       they have to be greyed here or a video selection leaves a live crop/heal strip over
       a dead panel. They are usually hidden anyway, but a panel left open from the
       previous still stays on screen when the selection moves to a video.
       Everything in the dock is greyed this way EXCEPT developAlertRows, which must stay
       readable precisely when the rest is disabled -- it says why. */
    if (transformPanel) transformPanel->setEnabled(live);
    if (replacePanel) replacePanel->setEnabled(live);
    /* History and Presets are part of the Develop tool: they come and go with it. Show
       them FIRST -- showing a tabified dock makes it the front tab, so Develop must be
       shown last (and raised) or the group would open on one of their tabs. */
    if (presetsDock) {
        presetsDock->setEnabled(live);
        presetsDock->setVisible(visible);
        if (presetsDockVisibleAction) presetsDockVisibleAction->setChecked(visible);
    }
    if (historyDock) {
        historyDock->setEnabled(live);
        historyDock->setVisible(visible);
        if (historyDockVisibleAction) historyDockVisibleAction->setChecked(visible);
    }
    /* raise() ONLY on an actual hidden -> visible transition. This runs on every file
       selection change in Develop mode (the usable state follows the selection), and an
       unconditional raise would yank Develop to the front tab every time the user
       navigated with History or Presets selected. */
    const bool wasVisible = developDock->isVisible();
    developDock->setVisible(visible);
    if (visible && !wasVisible) developDock->raise();
}

void MW::syncDevelopPanelEnabled()
{
    if (G::isLogger) G::log("MW::syncDevelopPanelEnabled");
    /* The Develop panel is usable only when the user's Develop toggle is on AND we are
       in Develop operation mode. Preview mode is fast, as-shot review, so the panel is
       always greyed there. */
    const bool visible = developAction && developAction->isChecked()
                         && G::operationMode == G::OperationMode::Develop;
    /* Develop operates on decoded still frames, so a video selection greys the controls
       (see the alert rows in updateDevelopSelectionWarning) rather than leaving live-looking
       sliders that silently do nothing -- every render entry and nearly every panel
       method already early-returns on a video / empty path. */
    setDevelopPanelEnabled(visible, !currentIsVideo());
}

void MW::setOperationMode(G::OperationMode mode)
{
/*
    Apply the top-level operation mode (Preview vs Develop) and keep the status-bar dropdown in
    sync. Preview = fast image review (embedded previews + large forward cache); Develop = best-
    quality single-image view/edit. Entering a mode re-targets the image cache: Develop trims the
    forward read-ahead to just the current image (setTargetRange), Preview restores the full
    forward cache. (The raw re-decode on a Develop cache-miss is handled in renderDevelopPreview.)
*/
    if (G::isLogger)
        G::log("MW::setOperationMode", mode == G::OperationMode::Develop ? "Develop" : "Preview");

    /* Develop persists its recipe to the image's XMP sidecar, and the develop preview
       cache folder takes no writes (see Cache/devpreviewcache.h). Refuse the mode rather
       than opening a panel whose every edit would be silently dropped. The D shortcut is
       already greyed by enableSelectionDependentMenus; this covers the other callers. */
    if (mode == G::OperationMode::Develop && isPreviewCacheFolderLoaded()) {
        if (G::popup)
            G::popup->showPopup("Develop is not available here: "
                                + DevPreviewCache::readOnlyReason() + ".", 3000);
        return;
    }

    /* Only show develop (and its History / Presets panels) in Develop Mode. Those two
       first, so Develop ends up the front tab (see setDevelopPanelEnabled). */
    const bool inDevelop = (mode == G::OperationMode::Develop);
    if (presetsDock) presetsDock->setVisible(inDevelop);
    if (historyDock) historyDock->setVisible(inDevelop);
    developDock->setVisible(inDevelop);
    if (inDevelop) developDock->raise();

    /* Save Develop Preset and Copy / Paste Develop Settings carry real shortcuts
       (Cmd+Shift+N, Cmd+Opt+C, Cmd+Opt+V), so gate them by mode here (before the
       no-change return, so they always track the mode) -- a disabled QAction's shortcut
       does not fire, keeping them Develop-only. */
    for (QAction *a : {developSavePresetAction, developCopySettingsAction,
                       developPasteSettingsAction})
        if (a) a->setEnabled(inDevelop);

    if (G::operationMode == mode) return;               // no change
    G::operationMode = mode;
    updateDevelopRenderingHint();     // Preview hides the chip; Develop re-evaluates

    /* Refresh the status bar for the new mode: it hides the metadata / image cache
       running lights in Develop and shows them in Preview. */
    updateStatusBar();

    /* Develop always shows the developed image, so the picture choice is disabled (and
       reads Developed) while in Develop and restored to the user's Preview-mode choice on
       the way out. G::previewSource itself is untouched by the mode. */
    syncPreviewSourceEnabled();

    /* ... and the thumbnails have to follow, because the mode is half of which picture
       they show: the effective source is (Develop ? Developed : G::previewSource). With
       Original chosen, entering Develop must replace the camera thumbs with developed
       ones and leaving must put them back. With Developed chosen the effective source is
       Developed in both modes, so there is nothing to redo -- and skipping it matters,
       since that is the default and this would otherwise re-read every developed
       thumbnail on each D press. */
    if (G::previewSource == G::PreviewSource::Original) refreshDevelopThumbs();

    /* Develop shows a single image, so it always runs in Loupe: force Loupe on entry, and
       refresh the View-menu gating (enableSelectionDependentMenus disables Compare -- and
       its C shortcut -- while in Develop, re-enabling on return to Preview). Loupe (E),
       Grid (G) and Table (T) stay enabled: they are the way back to Preview. */
    if (mode == G::OperationMode::Develop && G::mode != "Loupe")
        loupeDisplay("MW::setOperationMode");
    enableSelectionDependentMenus();

    /* Capture the Preview (embedded) image BEFORE the re-decode below so the Develop
       diagnostics can verify the Develop render (demosaic + edits) actually differs from
       what Preview showed. Small, downscaled copy; the display-vs-Preview check runs in
       updateDevelopScopes. Entering Develop only (leaving it, there is no Develop render
       to verify). */
    if (mode == G::OperationMode::Develop && icd && dm && !dm->currentFilePath.isEmpty()
        && icd->contains(dm->currentFilePath)) {
        const QImage prev = icd->imCache.value(dm->currentFilePath);
        developVerifyPreviewBaseline = prev.isNull()
            ? QImage()
            : prev.scaled(256, 256, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        developVerifyPreviewBaselinePath = dm->currentFilePath;
        developVerifyVsPreviewMaxAbs = -1;   // reset until the display populates it
        developVerifyVsPreviewPath.clear();
    }

    /* Paint the cached develop preview NOW, before the re-decode below is even armed.

       Entering Develop re-decodes the current image (~2-3s on a raw), and until that
       lands the loupe keeps showing the PREVIEW-mode render -- the untouched camera
       picture, i.e. exactly what the user's edits changed. A cached preview is the
       developed look and is already on disk, so show it and let the real render replace
       it.

       Routed through loadImageInterim, the SAME path fileSelectionChange uses, rather
       than setDevelopPreview. setDevelopPreview exists for the drag case, where only the
       pixels change and the displayed dimensions are the current image's -- and that is
       exactly wrong here. A cropped preview has a DIFFERENT shape from the uncropped
       Preview-mode image still on screen, so presenting it at that image's display size
       stretched it to fill the frame until the render landed. loadImageInterim fits from
       the pixmap's own bounding rect, so the crop's aspect ratio survives, and it already
       captures and restores zoom/pan across the swap.

       This is a SEPARATE hook from the one in fileSelectionChange. That one fires on an
       image-cache MISS; a mode switch is a cache HIT (the Preview decode is still
       cached), so it never reaches that branch. */
    if (mode == G::OperationMode::Develop && imageView && dm && !currentIsVideo()) {
        const QString fPath = dm->currentFilePath;
        const QImage cached = devPreview(fPath);
        if (!cached.isNull()) {
            imageView->captureDevelopView(fPath);
            if (imageView->loadImageInterim(fPath, cached)) {
                developInterimIsDevPreview = true;
                updateDevelopRenderingHint();
            }
        }
    }

    /* The mode owns the raw/preview decode: Develop decodes RAW sensor data (best
    quality), Preview shows embedded previews (fast). toggleUseRaw() flips G::useRaw,
    syncs the Develop "Edit: Raw / Embedded Preview" selector, and rebuilds the cache --
    which, in Develop, re-targets to just the current image (setTargetRange). Within
    Develop the Edit-source selector can still OVERRIDE to Embedded Preview. If useRaw is
    already correct, just re-target for the mode's read-ahead change -- but NOT on a
    video: there is no loupe to refresh (the central widget is on VideoTab) and the
    ImageCache does not decode videos, so the forced re-decode is pure cost. The next
    still selection re-targets the cache anyway (fileSelectionChange -> setImageCachePosition).
    The useRaw flip itself is NOT skipped: it is a mode-level setting and must track the
    mode so the next still decodes correctly. */
    const bool wantUseRaw = (mode == G::OperationMode::Develop);
    if (G::useRaw != wantUseRaw)
        toggleUseRaw(wantUseRaw ? Tog::on : Tog::off);   // flips useRaw + reloads cache
    else if (imageCache && dm && !dm->currentFilePath.isEmpty() && !currentIsVideo()) {
        /* useRaw already matches the target, but the MODE change alone re-selects
           the decode: ImageDecoder::load() branches on (operationMode == Develop &&
           useRaw), so Preview shows the embedded preview while Develop demosaics the
           raw. Re-target read-ahead for the mode, then FORCE the current image to
           re-decode + reload so the loupe reflects the new mode -- without this it
           keeps showing the prior mode's image until the user toggles the Edit-source
           selector (which re-decodes via toggleUseRaw). Matches the toggleUseRaw
           refresh (currentImageHasChanged + imageCacheColorManageChange ->
           reloadImageCache -> refreshViewsOnCacheChange -> loupe). */
        imageCache->setCurrentPosition(dm->currentFilePath, "MW::setOperationMode");
        imageView->currentImageHasChanged = true;
        emit imageCacheColorManageChange();
    }

    /* Preview greys out the Develop panel; Develop re-enables it. The panel is not kept
    in sync with the selected image while in Preview (fileSelectionChange skips it), so
    on entering Develop point it at the current image before enabling. Leaving Develop,
    flush any unsaved edits of the image we were on so they persist. */

    if (developProperties) {
        if (mode == G::OperationMode::Develop) {
            const bool selIsVideo = currentIsVideo();
            developProperties->setCurrentImage(selIsVideo || !dm ? QString()
                                                                 : dm->currentFilePath);
            /* Entering Develop re-decodes the current image (~3s). If it has a "Denoise
               raw" amount, start that decode NOW so its progress shows immediately --
               otherwise it would not fire until the clean decode + settle. Produces the
               clean + PMRID bases in one pass and publishes the clean base (which
               ImageDecoder::load then reuses). No-op without a denoise edit / on Apple. */
            if (!selIsVideo && dm && !dm->currentFilePath.isEmpty()) {
                /* Gated on the RECIPE (EditParams::denoiseRaw), which falls back to the
                   Auto run preference when the image says nothing. */
                const auto mj = developProperties->stackJob();
                if (mj.global.wantsDenoiseRaw(G::autoRunDenoise))
                    ensureRawDenoise(dm->currentFilePath, mj.global,
                                     WorkingImageCache::instance().get(dm->currentFilePath),
                                     currentImageIso());
            }
        }
        else {
            developProperties->flushAll();
        }
        /* Show/hide the multi-image warning for the mode we just entered (the banner is
           only reachable in Develop, but its text is stale until refreshed). */
        updateDevelopSelectionWarning();
    }
    syncDevelopPanelEnabled();

    /* Re-enable (or grey) the Develop menu's mode-local items for the mode just entered.
       It also runs on the menu's aboutToShow, but that is not enough on its own: opening
       the menu in Preview DISABLES those actions, and a disabled QAction::trigger() is a
       no-op -- so the mode-local keys the arbiter dispatches through them (R Transform, S
       Spot, ...) would stay dead in Develop until the user happened to open the menu
       again. */
    syncDevelopMenuEnabled();

    /* The isCached red-dot indicator is suppressed in Develop Mode (drawn per-paint in
       IconViewDelegate). Repaint the icon views so dots on rows other than the current
       image are added/removed to match the new mode. */
    if (thumbView) thumbView->viewport()->update();
    if (gridView) gridView->viewport()->update();
}

void MW::updateDevelopSelectionWarning()
{
/*
    Refresh the Develop dock's alert rows from the current selection. Every develop edit
    and every Paste Settings applies to the whole selection, so the multi-image row is
    the standing WARNING that the panel is not editing one image. A video is a SHOW
    STOPPER: Develop cannot run on it at all. Conditions STACK, each in its own row.

    It also lands any propagation the sliders have queued: that batch belongs to the
    selection that was live when the edit was made, and it must be written before the
    new selection can claim it (DevelopProperties::flushPropagation).
*/
    if (G::isLogger) G::log("MW::updateDevelopSelectionWarning");
    if (developProperties) developProperties->flushPropagation();
    if (!developAlertRows) return;

    QList<QPair<DevelopAlert, QString>> alerts;

    /* SHOW STOPPER: the panel is greyed for a video (syncDevelopPanelEnabled) and
       nothing can be edited -- Develop does not apply at all. */
    if (currentIsVideo()) {
        alerts << qMakePair(AlertShowStopper, QString("First image selected is a video."));
        /* MIXED selection with the video CURRENT: the stills cannot be edited either,
           because Develop edits the current image and fans the change out from it -- and
           the current image is the video. selectedEditCount is 0 here (MW empties the
           current path for a video), so the count has to come from selectedStillCount or
           the user is told nothing about the stills they have selected. */
        const int stills = developProperties ? developProperties->selectedStillCount() : 0;
        if (stills > 0)
            alerts << qMakePair(AlertWarning,
                                QString::number(stills) +
                                (stills == 1 ? " still image is" : " still images are") +
                                " also selected - make one of them current to develop"
                                " the selection");
    }
    else {
        /* WARNING: Develop works, it just works on more than the visible image. Videos in
           a multi-image selection are not counted: selectedEditCount goes through
           otherSelectedPaths, which skips them (a video has no develop recipe). */
        const int n = developProperties ? developProperties->selectedEditCount() : 0;
        if (n > 1)
            alerts << qMakePair(AlertWarning,
                                QString("Multiple images will have edits applied."));
    }

    setDevelopAlerts(alerts);
}

void MW::setDevelopAlerts(const QList<QPair<DevelopAlert, QString>> &alerts)
{
/*
    Fill the alert rows below the Develop action row: one row per alert, coloured text on
    the panel background (no fill -- the panel stays one surface). RED = show stopper
    (Develop cannot run on this selection), AMBER = warning (it runs, but on more than the
    visible image). Show stoppers are sorted to the top, so severity reads from position
    as well as colour. An empty list hides the whole block.

    Rows are pooled: labels are reused and the surplus hidden, so a selection change does
    not churn widgets. The colour therefore has to be re-applied to every reused row -- a
    row that carried a show stopper last time may carry a warning now.
*/
    if (!developAlertRows || !developAlertRowsLayout) return;
    if (alerts.isEmpty()) {
        developAlertRows->setVisible(false);
        return;
    }

    /* Most severe first (stable, so same-severity rows keep the caller's order). */
    QList<QPair<DevelopAlert, QString>> ordered = alerts;
    std::stable_sort(ordered.begin(), ordered.end(),
                     [](const QPair<DevelopAlert, QString> &a,
                        const QPair<DevelopAlert, QString> &b){ return a.first > b.first; });

    for (int i = 0; i < ordered.count(); ++i) {
        QLabel *row = nullptr;
        if (i < developAlertLabels.count()) {
            row = developAlertLabels.at(i);
        }
        else {
            row = new QLabel(developAlertRows);
            row->setAlignment(Qt::AlignCenter);
            row->setWordWrap(true);
            developAlertRowsLayout->addWidget(row);
            developAlertLabels << row;
        }
        // red = show stopper, amber = warning
        const QString colour = ordered.at(i).first == AlertShowStopper
                                   ? "#FF4040" : "#FFB340";
        row->setStyleSheet("QLabel { color: " + colour +
                           "; font-weight: bold; background: transparent; }");
        row->setText(ordered.at(i).second);
        row->setVisible(true);
    }
    for (int i = ordered.count(); i < developAlertLabels.count(); ++i)
        developAlertLabels.at(i)->setVisible(false);

    developAlertRows->setVisible(true);
}

void MW::developDockVisibilityChange()
{
    if (G::isLogger) G::log("MW::developDockVisibilityChange");
    /* Keep the mask overlay confined to Develop: drop it when the dock is hidden or tabbed away,
       re-assert the active tool's overlay when it returns. */
    if (!imageView || !developProperties) return;
    if (developDock->isVisible()) {
        /* A TABIFIED dock keeps isVisible() == true even when a sibling tab is selected
           (that is why isSelectedDockTab tests the visible REGION instead). So an
           unconditional raise() here yanked Develop straight back to the front the
           instant the user clicked a sibling tab -- clicking the History tab appeared to
           do nothing. Only raise when Develop really is the front tab, where raise() is
           a no-op anyway; the paths that SHOW the dock raise it explicitly. */
        if (isSelectedDockTab(developDock)) developDock->raise();
        developDockVisibleAction->setChecked(true);
        developProperties->refreshMaskEdit();
    }
    else {
        imageView->endMaskEdit();
    }

    /* Same invariant for the crop tool: a visible Transform panel means crop is active. When the
       dock hides the panel goes with it, so commit + end the crop; when it returns with the panel
       showing, re-activate it. */
    if (transformPanel) {
        if (developDock->isVisible() && developTransformVisible) {
            if (!developCropEditing) enterDevelopCrop();
        }
        else if (developCropEditing) exitDevelopCrop();
    }
}

void MW::historyDockVisibilityChange()
{
    if (G::isLogger) G::log("MW::historyDockVisibilityChange");
    /* createDocks runs BEFORE createActions, so the action can still be null here. */
    if (!historyDock || !developProperties) return;
    if (historyDock->isVisible()) {
        if (historyDockVisibleAction) historyDockVisibleAction->setChecked(true);
    }
    else {
        /* Tabbed away or hidden mid-hover: never leave the loupe showing a previewed
           history state the user can no longer see the row for. */
        developProperties->endHistoryPreview();
    }
}

void MW::presetsDockVisibilityChange()
{
    if (G::isLogger) G::log("MW::presetsDockVisibilityChange");
    /* createDocks runs BEFORE createActions, so the action can still be null here. */
    if (!presetsDock || !developProperties) return;
    if (presetsDock->isVisible()) {
        if (presetsDockVisibleAction) presetsDockVisibleAction->setChecked(true);
    }
    else {
        /* Tabbed away or hidden mid-hover: never leave the loupe showing a previewed
           preset the user can no longer see the row for. */
        developProperties->endPresetPreview();
    }
    updateDevelopPresetBtn();
}

void MW::updateDevelopPresetBtn()
{
    if (!developPresetBtn) return;
    developPresetBtn->setActive(presetsDock && presetsDock->isVisible()
                                && isSelectedDockTab(presetsDock));
}

void MW::createDocks()
{
    if (G::isLogger) G::log("MW::createDocks");
    createFolderDock();
    createFavDock();
    createFilterDock();
    createCatalogDock();
    if (G::useInfoView) createMetadataDock();
    createThumbDock();
    createEmbelDock();
    createDevelopDock();
    createHistoryDock();   // after Develop: it binds to developProperties
    createPresetsDock();   // ditto

    // connect(this, &MW::tabifiedDockWidgetActivated, this, &MW::embelDockActivated);

    addDockWidget(Qt::LeftDockWidgetArea, folderDock);
    addDockWidget(Qt::LeftDockWidgetArea, favDock);
    addDockWidget(Qt::LeftDockWidgetArea, filterDock);
    if (catalogDock) addDockWidget(Qt::LeftDockWidgetArea, catalogDock);
    if (G::useInfoView) addDockWidget(Qt::LeftDockWidgetArea, metadataDock);
    addDockWidget(Qt::LeftDockWidgetArea, thumbDock);
    if (!hideEmbellish) addDockWidget(Qt::RightDockWidgetArea, embelDock);
    addDockWidget(Qt::RightDockWidgetArea, developDock);
    addDockWidget(Qt::RightDockWidgetArea, historyDock);
    addDockWidget(Qt::RightDockWidgetArea, presetsDock);

    MW::setTabPosition(Qt::LeftDockWidgetArea, QTabWidget::North);
    MW::setTabPosition(Qt::RightDockWidgetArea, QTabWidget::North);
    MW::tabifyDockWidget(folderDock, favDock);
    MW::tabifyDockWidget(favDock, filterDock);
    /* Catalog sits beside Filters: both answer "which images?", one over what is
       loaded and one over everything indexed. */
    /* The Catalog tab exists only when it is a separate dock; with the Find dock its
       place in the group is taken by the Find panel's Catalog scope. */
    if (catalogDock) MW::tabifyDockWidget(filterDock, catalogDock);
    if (G::useInfoView)
        MW::tabifyDockWidget(catalogDock ? catalogDock : filterDock, metadataDock);
    /* Do NOT tabify the LEFT-area metadataDock with the RIGHT-area embelDock: that cross-area
       tabify drags embel (and the develop dock tabbed onto it below) into the LEFT group, so
       every dock ends up crammed in one left tab group. There the raised develop tab can't get
       its content width against the squeezed central widget and the dock layout oscillates
       (tab-bar flicker). embel + develop tab together on the RIGHT via the line below. This
       default normally only shows when no saved WindowState is restored. */
    if (!hideEmbellish) MW::tabifyDockWidget(embelDock, developDock);
    /* History and Presets join the same RIGHT group, immediately after Develop -- the
       three are one tool and are shown/hidden together (see setHistoryDockVisibility /
       setPresetsDockVisibility). */
    MW::tabifyDockWidget(developDock, historyDock);
    MW::tabifyDockWidget(historyDock, presetsDock);

    // Re-evaluate responsive dock tab titles when a dock is dragged between
    // docks/areas or floated: dragging into a tab group changes the tab count
    // without a reliable resize/show on the surviving docks.
    for (DockWidget *d : {folderDock, favDock, filterDock, catalogDock, metadataDock,
                          embelDock, developDock, historyDock, presetsDock}) {
        if (!d) continue;       // catalogDock is null with G::useFindDock
        connect(d, &QDockWidget::dockLocationChanged, this, &MW::scheduleDockTabUpdate);
        connect(d, &QDockWidget::topLevelChanged, this, &MW::scheduleDockTabUpdate);
        /* WORK IN PROGRESS - DISABLED.
           Intended: a dock dropped into an existing tab group lands last (rightmost),
           not at the drop position. Disabled because it broke dock-tab selection
           (see MW::moveDroppedDockLast). Re-enable once fixed. */
        // connect(d, &QDockWidget::dockLocationChanged, this, &MW::moveDroppedDockLast);
    }

    // Solo mode enforcement: when a dock is expanded and its area is in solo
    // mode, collapse the other docks in the same area.
    auto wireSolo = [this](DockWidget *d) {
        if (!d) return;
        connect(d, &DockWidget::collapsedChanged, this, [this, d](bool collapsed) {
            if (!collapsed) enforceDockSoloMode(d);
        });
    };
    wireSolo(folderDock);
    wireSolo(favDock);
    wireSolo(filterDock);
    wireSolo(catalogDock);
    if (G::useInfoView) wireSolo(metadataDock);
    wireSolo(thumbDock);
    wireSolo(embelDock);
    wireSolo(developDock);
    wireSolo(historyDock);
    wireSolo(presetsDock);
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

