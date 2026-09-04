#include "Datamodel/datamodel.h"
#include "Cache/framedecoder.h"
#include "Main/global.h"
#include "Metadata/keywordflatten.h"
#include "Utilities/searchterms.h"
#include "Metadata/indexmetadata.h"

/*
The datamodel (dm thoughout app) contains information about each eligible image
file in the selected folder (and optionally the subfolder heirarchy).  Eligible
image files, defined in the metadata class, are files Winnow knows how to decode.

The data is structured in columns:

    ● Path:             from QFileInfoList  G::PathRole (absolutePath)
                        from QFileInfoList  Qt::ToolTipRole
                                            G::IconRectRole (icon)
                                            G::CachingIcon
                                            G::DupIsJpgRole
                                            G::DupOtherIdxRole
                                            G::DupHideRawRole
                                            G::DupRawTypeRole
                                            G::ColumnRole
                                            G::GeekRole
    ● File name:        from QFileInfoList  EditRole
    ● File type:        from QFileInfoList  EditRole
    ● File size:        from QFileInfoList  EditRole
    ● File created:     from QFileInfoList  EditRole
    ● File modified:    from QFileInfoList  EditRole
    ● Year:             from metadata       createdDate
    ● Day:              from metadata       createdDate
    ● Creator:          from metadata       EditRole
    ● Rejected:         reject function     EditRole
    ● Picked:           user edited         EditRole
    ● Rating:           user edited         EditRole
    ● Label:            user edited         EditRole
    ● MegaPixels:       from metadata       EditRole
    ● Width:            from metadata       EditRole
    ● Height:           from metadata       EditRole
    ● Dimensions:       from metadata       EditRole
    ● Rotation:         user edited         EditRole
    ● Aperture:         from metadata       EditRole
    ● ShutterSpeed:     from metadata       EditRole
    ● ISO:              from metadata       EditRole
    ● CameraModel:      from metadata       EditRole
    ● FocalLength:      from metadata       EditRole
    ● Title:            from metadata       EditRole
    ● Copyright:        from metadata       EditRole
    ● Email:            from metadata       EditRole
    ● Url:              from metadata       EditRole
    ● OffsetFullJPG:    from metadata       EditRole
    ● LengthFullJPG:    from metadata       EditRole
    ● WidthFullJPG:     from metadata       EditRole
    ● HeightFullJPG:    from metadata       EditRole
    ● OffsetThumbJPG:   from metadata       EditRole
    ● LengthThumbJPG:   from metadata       EditRole
//    ● OffsetSmallJPG:   from metadata       EditRole
//    ● LengthSmallJPG:   from metadata       EditRole
    ● XmpSegmentOffset: from metadata       EditRole
    ● XmpNextSegmentOffset:  metadata       EditRole
    ● IsXmp:            from metadata       EditRole
    ● OrientationOffset:from metadata       EditRole

enum for roles and columns are in global.cpp

Note that more items such as the file offsets to embedded JPG are stored in
the metadata structure which is indexed by the file path.

A QSortFilterProxyModel (SortFilter = sf) is used by ThumbView, TableView,
CompareView and ImageView (dm->sf thoughout the app).

Sorting is applied both from the menu and by clicking TableView headers.  When
sorting occurs all views are updated and the image cache is reindexed.

Filtering is applied from Filters, a QTreeWidget with checkable items for a
number of datamodel columns.  Filtering also updates all views and the image
cache is reloaded.

SORTING AND FILTERING FLOW

Sorting and filtering requires that metadata exist for all images in the datamodel.
However, metadata is loaded as required to improve performance in folders with many images.
When filtering is requested the following steps occur:

    • Note current image and selection
    • Load all metadata if required
    • Refresh the proxy sort/filter
    • If criteria results in null dataset report in centralLayout and finished
    • Re-establish current image and selection in filtered proxy
    • Update the filter panel criteria tree item counts
    • Update the status panel filtration status
    • Reset centralLayout in case prior criteria resulted in null dataset
    • Sync image cache cacheItemList to match dm->sf after filtering
    • Make sure icons are loaded in all views and the image cache is up-to-date.

RAW+JPG

When Raw+Jpg are combined the following datamodel roles are used:

G::DupIsJpgRole     True if jpg type and there is a matching raw file
G::DupOtherIdxRole  The paired row, as an int datamodel row, on BOTH halves
G::DupHideRawRole   True if this raw file has a matching jpg
G::DupRawTypeRole   Raw file type if jpg type with matching raw

All four live on column 0. Read them through isDupJpg / isDupHiddenRaw /
dupOtherRow / dupRawType rather than data(G::Dup...Role).

INSTANCE CONFLICT

Each time a new folder is selected the DataModel is cleared and loaded with the metadata
for the images in the new folder, and the instance is incremented.  If MetaRead is being
useed (concurrent loading using another thread) there is a possibility that the MetaRead
thread is out of sync with the current DataModel.  The instance is used to test this.

Note that MetaRead is causing a crash in DataModel::addMetadataForItem when folders are
selected in rapid succession, and I cannot figure out how to fix.

Code examples for model:

    // current selection:
    QModelIndexList selection = thumbView->selectionModel()->selectedRows();
    QModelIndexList selection = selectionModel->selectedRows();

    // an index of Rating in the selection:
    QModelIndex idx = dm->sf->index(selection.at(i).row(), G::RatingColumn);

    // value of Rating:
    rating = idx.data(Qt::EditRole).toString();

    // to edit a value in the model
    dm->sf->setData(index(row, G::RotationColumn), value);

    // edit an isCached role in model based on file path
    QModelIndexList idxList = dm->sf->match(dm->sf->index(0, 0), G::PathRole, fPath);
    QModelIndex idx = idxList[0];
    dm->sf->setData->(index(row, G::IsCachedColumn), isCached));

    // file path for current index (primary selection)
    fPath = thumbView->currentIndex().data(G::PathRole).toString();

    // get current sf (proxy) index from a dm (datamodel) index
    QModelIndex sfIdx = dm->sf->mapFromSource(dmIdx)
    int dmRow = sf->mapFromSource(index(sfRow, 0)).row();

    // to get a QItem from a filtered or sorted datamodel selection
    QModelIndexList selection = thumbView->selectionModel()->selectedRows();
    QModelIndexList selection = selectionModel->selectedRows();
    QModelIndex idx = dm->sf->index(selection.at(i).row(), G::PathColumn);
    QStandardItem *item = new QStandardItem;
    item = dm->itemFromIndex(dm->sf->mapToSource(thumbIdx));

    // to force the model to refresh
    dm->sf->filterChange();        // executes invalidateFilter() in proxy
*/

DataModel::DataModel(QObject *parent,
                     Metadata *metadata,
                     Filters *filters,
                     bool &combineRawJpg) :

                     QAbstractTableModel(parent),
                     combineRawJpg(combineRawJpg)
{
    if (G::isLogger) G::log("DataModel::DataModel");

    /*
    Every time the datamodel changes: either a new folder is selected, or the current one
    is filtered; the model instance is incremented to check for concurrency issues where
    a thumb or image decoder is still working on the prior datamodel instance.
    */
    instance = -1;
    if (isDebug) qDebug() << "DataModel::DataModel" << "instance =" << instance;

    // mw = parent;
    this->metadata = metadata;
    this->filters = filters;

    setModelProperties();

    sf = new SortFilter(this, filters, combineRawJpg);
    sf->setSourceModel(this);
    selectionModel = new QItemSelectionModel(sf);

    /*  Republish the worker view of the proxy whenever its structure or order
        changes. Driven by the proxy's OWN signals rather than by a list of call
        sites, so a path that inserts, removes, re-sorts or re-filters cannot
        forget to do it -- and there is no correct list to maintain, since
        SortFilter::invalidate() is reached from MW, the Filters tree and
        combineRawJpg alike. All five are emitted on the GUI thread. */
    for (auto sig : { &QAbstractItemModel::rowsInserted,
                      &QAbstractItemModel::rowsRemoved }) {
        connect(sf, sig, this, [this]{ rebuildProxySnapshot(); });
    }
    connect(sf, &QAbstractItemModel::modelReset,   this, [this]{ rebuildProxySnapshot(); });
    connect(sf, &QAbstractItemModel::layoutChanged, this, [this]{ rebuildProxySnapshot(); });

    /*  The row-count changes the proxy reports also size the live per-row array.
        It is grown from the SOURCE row count, not the proxy's -- the array is
        indexed by datamodel row, and a filtered-out row still needs its slot. */
    connect(this, &QAbstractItemModel::rowsInserted, this,
            [this]{ resizeRowSync(rowCount()); });
    /*  Splicing the stores used to happen HERE, off rowsInserted/rowsRemoved.
        It now happens inside DataModel::insertRows/removeRows, which are the
        only way the row count can move at all once the model owns its own
        storage -- so the splice and the count cannot disagree. Only the
        worker-thread mirror is still driven by the signal. */
    connect(this, &QAbstractItemModel::rowsRemoved, this,
            [this]{ resizeRowSync(rowCount()); });
    connect(this, &QAbstractItemModel::modelReset, this,
            [this]{ resizeRowSync(rowCount()); });

    resizeRowSync(rowCount());
    rebuildProxySnapshot();

     // eligible image file types
    fileFilters = new QStringList;
    foreach (const QString &str, metadata->supportedFormats) {
        fileFilters->append("*." + str);
        supportedExtSet.insert(str.toLower());   // O(1) suffix lookup in addFolder
    }

    emptyImg.load(":/images/no_image.png");
    setThumbnailLegend();

    iconChunkSize = G::maxIconChunk;

    /* Layer 3: poll memory pressure on the GUI thread. The slot early-returns unless
       G::useJitIconCache, so the timer is cheap when the feature is off. */
    iconPressureClock.start();
    iconPressureTimer = new QTimer(this);
    iconPressureTimer->setInterval(kIconPressurePollMs);
    connect(iconPressureTimer, &QTimer::timeout, this, &DataModel::applyIconCachePressure);
    iconPressureTimer->start();

    abort = false;

    // set true for debug output
    isDebug = false;
}

void DataModel::setModelProperties()
{
    if (isDebug) qDebug() << "DataModel::setModelProperties" << "instance =" << instance;

    /*  setSortRole is gone with QStandardItemModel. It only ever affected
        QStandardItemModel::sort(), which nothing called -- sorting is the
        proxy's (SortFilter::sort), and the proxy has its own sort role. */

    // must include all prior Global dataModelColumns (any order okay)
    /*  THE COLUMN HEADERS, as a table rather than 93 QStandardItems.
        Two facts per column: the name the table view shows, and whether the
        column is a GEEK column (hidden unless the user asks for the diagnostic
        set -- TableView reads it through headerData). They used to be a header
        item each, which is the one piece of QStandardItemModel the swap could
        not simply drop. */
    static const struct { int column; const char *name; bool geek; } kHeaders[] = {
        { G::PathColumn,                 "Icon",                     false },
        { G::RowNumberColumn,            "#",                        false },
        { G::NameColumn,                 "File Name",                false },
        { G::FolderNameColumn,           "Folder Name",              false },
        { G::NSThumbColumn,              "NS Thumb",                 false },
        { G::NSImageColumn,              "NS Image",                 false },
        { G::PickColumn,                 "Pick",                     false },
        { G::IngestedColumn,             "Ingested",                 false },
        { G::LabelColumn,                "Colour",                   false },
        { G::RatingColumn,               "Rating",                   false },
        { G::SearchColumn,               "Search",                   false },
        { G::TypeColumn,                 "Type",                     false },
        { G::VideoColumn,                "Video",                    false },
        { G::SidecarColumn,              "Sidecar",                  false },
        { G::ApertureColumn,             "Aperture",                 false },
        { G::ShutterspeedColumn,         "Shutter",                  false },
        { G::ISOColumn,                  "ISO",                      false },
        { G::ExposureCompensationColumn, "  EC  ",                   false },
        { G::DurationColumn,             "Duration",                 false },
        { G::CameraMakeColumn,           "Make",                     false },
        { G::CameraModelColumn,          "Model",                    false },
        { G::LensColumn,                 "Lens",                     false },
        { G::FocalLengthColumn,          "Focal length",             false },
        { G::FocusXColumn,               "FocusX",                   false },
        { G::FocusYColumn,               "FocusY",                   false },
        { G::GPSCoordColumn,             "GPS Coord",                false },
        { G::ByteSizeColumn,             "Size",                     false },
        { G::WidthColumn,                "Width",                    false },
        { G::HeightColumn,               "Height",                   false },
        { G::ModifiedColumn,             "Last Modified",            false },
        { G::CreatedColumn,              "Created",                  false },
        { G::YearColumn,                 "Year",                     false },
        { G::MonthColumn,                "Month",                    false },
        { G::DayColumn,                  "Day",                      false },
        { G::CreatorColumn,              "Creator",                  false },
        { G::MegaPixelsColumn,           "MPix",                     false },
        { G::LoadMsecPerMpColumn,        "Msec/Mp",                  false },
        { G::DimensionsColumn,           "Dimensions",               false },
        { G::AspectRatioColumn,          "Aspect Ratio",             false },
        { G::IconAspectRatioColumn,      "Icon Aspect Ratio",        false },
        { G::OrientationColumn,          "Orientation",              false },
        { G::RotationColumn,             "Rot",                      false },
        { G::CopyrightColumn,            "Copyright",                false },
        { G::TitleColumn,                "Title",                    false },
        { G::EmailColumn,                "Email",                    false },
        { G::UrlColumn,                  "Url",                      false },
        { G::KeywordsColumn,             "Keywords",                 false },
        { G::KeywordPathsColumn,         "KeywordPaths",             true },
        { G::KeywordsAllColumn,          "All Keywords",             true },
        { G::MetadataReadingColumn,      "Meta Reading",             true },
        { G::MetadataStatusColumn,       "Meta Status",              true },
        { G::IconLoadedColumn,           "Icon Loaded",              true },
        { G::RawRenderColumn,            "Raw Render",               true },
        { G::CompareColumn,              "Compare",                  true },
        { G::_RatingColumn,              "_Rating",                  true },
        { G::_LabelColumn,               "_Label",                   true },
        { G::_CreatorColumn,             "_Creator",                 true },
        { G::_TitleColumn,               "_Title",                   true },
        { G::_CopyrightColumn,           "_Copyright",               true },
        { G::_EmailColumn,               "_Email",                   true },
        { G::_UrlColumn,                 "_Url",                     true },
        { G::PermissionsColumn,          "Permissions",              true },
        { G::ReadWriteColumn,            "R/W",                      true },
        { G::OffsetFullColumn,           "OffsetFull",               true },
        { G::LengthFullColumn,           "LengthFull",               true },
        { G::WidthOrigPreviewColumn,     "WidthPreview",             true },
        { G::HeightOrigPreviewColumn,    "HeightPreview",            true },
        { G::OffsetThumbColumn,          "OffsetThumb",              true },
        { G::LengthThumbColumn,          "LengthThumb",              true },
        { G::samplesPerPixelColumn,      "samplesPerPixelFull",      true },
        { G::isBigEndianColumn,          "isBigEndian",              true },
        { G::ifd0OffsetColumn,           "ifd0Offset",               true },
        { G::ifdOffsetsColumn,           "ifd0Offsets",              true },
        { G::XmpSegmentOffsetColumn,     "XmpSegmentOffset",         true },
        { G::XmpSegmentLengthColumn,     "XmpSegmentLengthColumn",   true },
        { G::IsXMPColumn,                "IsXMP",                    true },
        { G::ICCSegmentOffsetColumn,     "ICCSegmentOffsetColumn",   true },
        { G::ICCSegmentLengthColumn,     "ICCSegmentLengthColumn",   true },
        { G::ICCBufColumn,               "ICCBuf",                   true },
        { G::ICCSpaceColumn,             "ICCSpace",                 true },
        { G::CacheSizeColumn,            "CacheSize",                true },
        { G::IsCachingColumn,            "IsCaching",                true },
        { G::IsCachedColumn,             "IsCached",                 true },
        { G::AttemptsColumn,             "Attempts",                 true },
        { G::DecoderIdColumn,            "DecoderId",                true },
        { G::DecoderReturnStatusColumn,  "DecoderReturnStatus",      true },
        { G::DecoderErrMsgColumn,        "Decoder Err Msg",          true },
        { G::OrientationOffsetColumn,    "OrientationOffset",        true },
        { G::RotationDegreesColumn,      "RotationDegrees",          true },
        { G::ShootingInfoColumn,         "ShootingInfo",             true },
        { G::SearchTextColumn,           "Search",                   true },
        { G::ErrColumn,                  "Load Metadata Errors",     true },
        { G::DevelopColumn,              "Developed",                false },
        { G::AvailabilityColumn,        "Availability",           true },
        { G::DevPreviewKeyColumn,        "DevPreviewKey",            true },
    };
    mHeaderName.assign(G::TotalColumns, QString());
    mHeaderGeek.assign(G::TotalColumns, false);
    for (const auto &h : kHeaders) {
        mHeaderName[h.column] = QString::fromLatin1(h.name);
        mHeaderGeek[h.column] = h.geek;
    }
    /* The two OrigPreview header STRINGS deliberately keep their original text.
       TableView column visibility is persisted in QSettings under the header string
       (see MW::settings "TableFields"), so renaming them to match the origPreview /
       devPreview terminology would silently reset the user's show/hide choice. */
    // setHorizontalHeaderItem(G::IsVideoColumn, new QStandardItem("IsVideo")); horizontalHeaderItem(G::IsVideoColumn)->setData(true, G::GeekRole);
    // "🔎" was title for search column
}

/*  The EXACT byte cost of one datamodel row, summed over every column and every
    role.

    This is the slow, accurate path: ~92 columns x ~6 roles is over 500 data()
    calls for a single row. It must NOT be called per row on a load path. It is
    called by the diagnostics report, and on a 1-in-64 sample by
    addMetadataForItem to calibrate the running bytesUsed estimate -- see
    bytesUsed in datamodel.h.
*/
/*  ---- Worker-thread views of the model (Datamodel/modelsync.h) ---- */

/*  Compare the packed row store against the QStandardItems it shadows.

    This is what makes the storage change safe to complete. The rewrite has no
    working intermediate state -- a model cannot be half-swapped -- so instead of
    changing the storage and hoping, the store is written beside the items and
    then asked to produce the same answers for every row and every column it
    claims to cover. A clean report over real folders is the evidence; until
    then the store is under test and nothing reads from it.

    COMPARISON IS ON THE VARIANT'S TEXT, not on QVariant::operator==. The model
    holds a quint32 where the store holds a qint64, an int where it holds a
    quint8; those compare unequal as variants and identical as values, and it is
    the VALUE the views and the filters read. Where a type genuinely matters --
    CompareColumn is a bool that BuildFilters counts as text -- the store keeps
    the model's own type, which is why this check is on what data() returns
    rather than on what was passed to setData.
*/
RowSyncPtr DataModel::rowSync() const
{
    QMutexLocker lock(&mSyncMutex);
    return mRowSync;
}

ProxySnapshotPtr DataModel::proxySnapshot() const
{
    QMutexLocker lock(&mSyncMutex);
    return mProxySnapshot;
}

/*  Grow (or create) the live per-row array, carrying existing values across.

    Called wherever the row count changes. It never SHRINKS on a plain grow --
    a smaller model comes from clearDataModel, which builds a fresh array. Rows
    are only ever appended within an instance, so index meanings are stable, and
    a worker holding the previous array keeps it alive through its shared_ptr
    while reading values that are still correct for the rows it had.
*/
void DataModel::resizeRowSync(int rows)
{
    if (rows < 0) rows = 0;
    auto fresh = std::make_shared<RowSyncArray>(rows);
    {
        QMutexLocker lock(&mSyncMutex);
        if (mRowSync && mRowSync->size() == rows) return;
        if (mRowSync) fresh->copyFrom(*mRowSync);
        mRowSync = fresh;
    }
}

/*  Republish the proxy's order and identity. GUI THREAD ONLY -- it reads both
    the model and the proxy.

    Driven by the proxy's own structural signals rather than by a list of call
    sites, so a path that inserts, removes, re-sorts or re-filters cannot forget
    to do it. Paths do not change once a row exists, so nothing else can make
    this stale.
*/
void DataModel::rebuildProxySnapshot()
{
    if (!G::isGuiThread()) return;

    auto snap = std::make_shared<ProxySnapshot>();
    snap->instance = instance;
    snap->sourceRows = rowCount();
    if (sf != nullptr) {
        const int n = sf->rowCount();
        snap->dmRowOf.resize(n);
        snap->pathOf.resize(n);
        snap->sfRowOfPath.reserve(n);
        snap->sfRowOfDmRow.reserve(n);
        for (int sfRow = 0; sfRow < n; ++sfRow) {
            const QModelIndex sfIdx = sf->index(sfRow, 0);
            const int dmRow = sf->mapToSource(sfIdx).row();
            const QString fPath = sfIdx.data(G::PathRole).toString();
            snap->dmRowOf[sfRow] = dmRow;
            snap->pathOf[sfRow] = fPath;
            if (!fPath.isEmpty()) snap->sfRowOfPath.insert(fPath, sfRow);
            if (dmRow >= 0) snap->sfRowOfDmRow.insert(dmRow, sfRow);
        }
    }
    {
        QMutexLocker lock(&mSyncMutex);
        mProxySnapshot = snap;
    }
}

qint64 DataModel::rowBytesUsed(int dmRow)
{
    /* roleNames() builds and returns a fresh QHash on every call, and .keys()
       allocates a QList from it. The role set is fixed for the life of the
       model, so build the key list once. */
    static const QList<int> roleKeys = roleNames().keys();

    qint64 bytes = 0;
    for (int col = 0; col < columnCount(); ++col) {
        QModelIndex dmIdx = index(dmRow, col);
        for (int role : roleKeys) {
            QVariant data = this->data(dmIdx, role);
            if (data.isNull()) continue;
            const quint64 cellBytes = Utilities::qvariantBytes(data);
            bytes += cellBytes;
            if (isDebug) {
                qDebug().noquote()
                    << "DataModel::rowBytesUsed"
                    << headerData(col, Qt::Horizontal).toString().leftJustified(25)
                    << "col =" << QString::number(col).rightJustified(2)
                    << "role =" << QString::number(role).rightJustified(1)
                    << "bytes =" << QString::number(cellBytes).rightJustified(12)
                    << "sumbytes =" << bytes;
            }
        }
    }
    return bytes;
}

/*  Fold row dmRow into the running bytesUsed ESTIMATE.

    Called once per metadata load. Only every 64th row is measured exactly;
    bytesUsed is then the sampled mean times the number of loaded rows. The
    previous code called rowBytesUsed() for EVERY row, putting ~550 data()
    calls plus a QHash build on the GUI thread per image, purely to feed a
    diagnostic. Nothing gates on bytesUsed -- the memory governor probes the
    process footprint directly (see the probeTick block in addMetadataForItem)
    -- and Diagnostics::datamodel walks the whole model itself when an exact
    figure is wanted, so an estimate is all this counter has ever needed to be.
*/
void DataModel::sampleRowBytesUsed(int dmRow)
{
    if ((bytesUsedSampleTick++ & 0x3F) == 0) {       // every 64th row
        bytesUsedSampleTotal += rowBytesUsed(dmRow);
        ++bytesUsedSampleCount;
    }
    if (bytesUsedSampleCount > 0) {
        const qint64 mean = bytesUsedSampleTotal / bytesUsedSampleCount;
        bytesUsed = mean * metadataLoadedCount.load(std::memory_order_relaxed);
    }
}

void DataModel::clearDataModel()
{
    if (G::isLogger || G::isFlowLogger) G::log("DataModel::clearDataModel");
    // clear the model
    if (mLock) return;
    if (isDebug)
        qDebug() << "DataModel::clearDataModel" << "instance =" << instance;

    /* Purge queued meta-call events targeting this model before clear() frees
       every QStandardItem. QStandardItemModel coalesces item edits into a
       posted _q_emitItemChanged meta-call; if that (or a stale worker
       setData/setIcon meta-call from the instance being torn down) fires after
       clear(), it dereferences freed item pointers and crashes in
       indexFromItem. This became reachable once processNextBatch was time-
       sliced: the GUI now stays responsive during the folder-add phase, so a
       folder click can reach clearDataModel while an addFolder tick's
       coalesced dataChanged is still in the event queue. Removing them is safe
       — they all target the instance being discarded, and queuedReaderEvents
       is reset below. */
    QCoreApplication::removePostedEvents(this, QEvent::MetaCall);

    clear();
    setModelProperties();
    // clear the fPath index of datamodel rows
    // fPathRow.clear();
    fPathRowClear();
    // clear the raw sensor unpack info cache
    fPathRawInfoClear();
    // clear the folder list
    folderList.clear();
    folderSet.clear();
    pendingPaths.clear();
    folderQueue.clear();
    // clear the folder image count hash
    folderImageCount.clear();
    // reset firstFolderPathWithImages
    firstFolderPathWithImages = "";
    // reset missing thumb (jpg/tiff)
    folderHasMissingEmbeddedThumb = false;
    // folderQueue is empty
    isProcessingFolders = false;
    /*  Fresh worker views. The old ones stay alive in any worker still holding
        a shared_ptr; their instance no longer matches, which is the existing
        staleness protocol every cross-thread write already checks. */
    {
        QMutexLocker lock(&mSyncMutex);
        mRowSync = std::make_shared<RowSyncArray>(0);
        mProxySnapshot = std::make_shared<ProxySnapshot>();
    }
    rowStore.clear();
    iconStore.clear();
    scratchStore.clear();

    // reset the raw/jpg bulk-load pairing trackers (prevRawRow is a row index)
    prevRawSuffix = "";
    prevRawBaseName = "";
    prevRawRow = -1;
    // reset the memory-used estimate and its sampling accumulators
    bytesUsed = 0;
    bytesUsedSampleTotal = 0;
    bytesUsedSampleCount = 0;
    bytesUsedSampleTick = 0;
    // clear memory-overrun latch and Reader→DataModel backpressure counter
    G::memoryOverrunFlag.store(false, std::memory_order_relaxed);
    queuedReaderEvents.store(0, std::memory_order_relaxed);
    /*  The eviction sweep's memory of what it last covered. A new model means the row
        numbers it holds mean nothing, so the next sweep must be a full one. */
    lastEvictStart = lastEvictEnd = -1;
    evictsSinceFullSweep = 0;
    iconChunkMissing = 0;
    // model is now empty: reset the running load-flag counts
    metadataAttemptedCount.store(0, std::memory_order_relaxed);
    metadataLoadedCount.store(0, std::memory_order_relaxed);
    iconLoadedCount.store(0, std::memory_order_relaxed);
    videoRowCount.store(0, std::memory_order_relaxed);
    iconUnloadableCount.store(0, std::memory_order_relaxed);
}

/*  The columns whose tooltip is simply their own text. Chosen explicitly rather
    than "every text column", because which columns offer a tooltip is a
    deliberate UI decision and the list must stay exactly the one the per-row
    writes produced -- a column gaining a tooltip it never had is a visible
    change hiding inside a storage change. */
static bool columnTooltipIsItsOwnText(int column)
{
    switch (column) {
    case G::NameColumn:
    case G::SearchTextColumn:
    case G::CameraMakeColumn:
    case G::CameraModelColumn:
    case G::LensColumn:
    case G::GPSCoordColumn:
    case G::KeywordsColumn:
    case G::KeywordPathsColumn:
    case G::KeywordsAllColumn:
    case G::ShootingInfoColumn:
    case G::TitleColumn:
    case G::CreatorColumn:
    case G::CopyrightColumn:
    case G::EmailColumn:
        return true;
    default:
        return false;
    }
}

/*  ALIGNMENT IS A PROPERTY OF THE COLUMN, NOT OF THE ROW.

    It used to be written per CELL -- 28 writes in addFileDataForRow and
    addMetadataForItem, plus one on every cross-thread setValueDm/setValueSF
    call, which carried an `align` argument defaulting to Qt::AlignLeft. Two
    consequences, and the second is a bug:

    o  A cell needs a QStandardItem to hold a role. Alignment alone therefore
       kept an item alive on 28 columns of every row, holding a number that is
       the same for all 250,000 of them.
    o  The default stamped Qt::AlignLeft onto whatever column a worker happened
       to write, so IsCaching, Attempts, DecoderId, DecoderReturnStatus, Meta
       Status, WidthPreview and NS Image were left-aligned on the rows the cache
       had reached and UNSET on the rest -- measured mid-load as "1 x15, unset
       x35". A column that renders differently depending on how far a background
       load got is a defect, not a layout.

    So the table below is authoritative and the `align` argument is gone. Every
    value here was taken from the writes it replaces; a column absent from it
    had no alignment and still has none.

    RawRenderColumn is deliberately absent even though ImageDecoder wrote it
    with AlignRight: addMetadataForItem sets that column with no alignment at
    all, so it was right-aligned only on rows where a raw had been rendered.
    Consistently unset is the smaller change and matches the column's other
    writer.
*/
static QVariant columnAlignment(int column)
{
    switch (column) {
    case G::TypeColumn:
    case G::RowNumberColumn:
    case G::VideoColumn:
    case G::DevelopColumn:
    case G::SidecarColumn:
    case G::PermissionsColumn:
    case G::ReadWriteColumn:
    case G::PickColumn:
    case G::IngestedColumn:
    case G::SearchColumn:
    case G::LabelColumn:
    case G::RatingColumn:
    case G::WidthColumn:
    case G::HeightColumn:
    case G::DimensionsColumn:
    case G::LoadMsecPerMpColumn:
    case G::OrientationColumn:
    case G::RotationColumn:
    case G::ApertureColumn:
    case G::ISOColumn:
    case G::ExposureCompensationColumn:
        return int(Qt::AlignCenter | Qt::AlignVCenter);
    case G::ByteSizeColumn:
    case G::AspectRatioColumn:
    case G::MegaPixelsColumn:
    case G::ShutterspeedColumn:
    case G::FocalLengthColumn:
    case G::NSThumbColumn:
    case G::NSImageColumn:
        return int(Qt::AlignRight | Qt::AlignVCenter);
    default:
        return QVariant();
    }
}

/*  ROW COUNT IS THE STORE'S SIZE. One source of truth: the packed row store is
    the storage, so asking it how many rows there are cannot disagree with what
    is actually held. QAbstractTableModel keeps no count of its own.
*/
/*  Empty the model, headers included -- the contract QStandardItemModel::clear()
    had, which is why every caller pairs it with setModelProperties().
*/
void DataModel::clear()
{
    beginResetModel();
    rowStore.clear();
    scratchStore.clear();
    iconStore.clear();
    mIssueLists.clear();
    mHeaderName.clear();
    mHeaderGeek.clear();
    endResetModel();
}

int DataModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid()) return 0;         // a table has no children
    return rowStore.size();
}

int DataModel::columnCount(const QModelIndex &parent) const
{
    if (parent.isValid()) return 0;
    return G::TotalColumns;
}

QVariant DataModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation == Qt::Vertical) {
        /*  QStandardItemModel numbered the rows from 1 when no vertical header
            item was set, and TableView hides the vertical header anyway -- but
            reproducing it costs a line and a changed row number would be a
            visible difference nobody asked for. */
        if (role == Qt::DisplayRole && section >= 0 && section < rowCount())
            return section + 1;
        return QVariant();
    }
    if (section < 0 || section >= int(mHeaderName.size())) return QVariant();
    switch (role) {
    /*  Edit and Display are the same slot, exactly as they were in the header
        item this replaces. */
    case Qt::DisplayRole:
    case Qt::EditRole:   return mHeaderName[section];
    case G::GeekRole:    return bool(mHeaderGeek[section]);
    default:             return QVariant();
    }
}

Qt::ItemFlags DataModel::flags(const QModelIndex &index) const
{
    if (!index.isValid()) return Qt::NoItemFlags;
    /*  The default QStandardItem flag set, measured before the swap (47 =
        Selectable | Editable | DragEnabled | DropEnabled | Enabled) and
        reproduced rather than reasoned about. Editable is in it and looks
        wrong; it is harmless because both views are NoEditTriggers, and
        dropping it here would be a behaviour change hiding inside a storage
        change. */
    return Qt::ItemIsSelectable | Qt::ItemIsEditable | Qt::ItemIsDragEnabled
         | Qt::ItemIsDropEnabled | Qt::ItemIsEnabled;
}

/*  INSERTION AND REMOVAL ARE NOW THE ONLY WAY THE ROW COUNT MOVES, which is an
    improvement on what they replace: setRowCount, insertRow and removeRows all
    went through QStandardItemModel and the stores were spliced afterwards, off
    the rowsInserted/rowsRemoved signals, so the splice and the count could in
    principle disagree. Here the splice IS the insertion.
*/
bool DataModel::insertRows(int row, int count, const QModelIndex &parent)
{
    if (parent.isValid() || count <= 0) return false;
    if (row < 0 || row > rowCount()) return false;
    beginInsertRows(QModelIndex(), row, row + count - 1);
    rowStore.insertRows(row, count);
    scratchStore.insertRows(row, count);
    endInsertRows();
    return true;
}

bool DataModel::removeRows(int row, int count, const QModelIndex &parent)
{
    if (parent.isValid() || count <= 0) return false;
    if (row < 0 || row + count > rowCount()) return false;
    beginRemoveRows(QModelIndex(), row, row + count - 1);
    rowStore.removeRows(row, count);
    scratchStore.removeRows(row, count);
    /*  The issue lists are keyed by row, so they shift with everything else. */
    if (!mIssueLists.isEmpty()) {
        QHash<int, QVariant> next;
        next.reserve(mIssueLists.size());
        for (auto it = mIssueLists.cbegin(); it != mIssueLists.cend(); ++it) {
            const int r = it.key();
            if (r >= row && r < row + count) continue;
            next.insert(r >= row + count ? r - count : r, it.value());
        }
        mIssueLists.swap(next);
    }
    endRemoveRows();
    return true;
}

QVariant DataModel::data(const QModelIndex &idx, int role) const
{
/*
    Serve the covered columns from the packed row store.

    THIS IS THE SEAM the whole storage change turns on. Every view, delegate,
    proxy and saved column-width setting addresses the model by
    G::dataModelColumns and reads it through data(); none of them care what is
    behind it. So the storage can be replaced here, one column set at a time,
    without any of them changing -- which is what makes a rewrite that cannot be
    landed incrementally land incrementally after all.

    ONLY EditRole AND DisplayRole. QStandardItem keeps those two in the SAME
    slot, so the store returning one value for both matches what the items did;
    every other role (ToolTip, TextAlignment, Decoration, and the G:: custom
    roles) is presentation or per-row bookkeeping that the store does not hold,
    and falls through untouched.

    THE STORES ARE NOW THE ONLY COPY. During the shadow phase the items were
    written too and verifyRowStore() compared the two; that is retired, so a
    covered column reaching QStandardItemModel::data() below would read an unset
    cell rather than fall back to anything.
*/
    if (idx.isValid() &&
        RowStore::covers(idx.column(), role) &&
        rowStore.contains(idx.row()))
    {
        return rowStore.value(idx.row(), idx.column(), role);
    }

    /*  The scratch columns come from the row-keyed side table
        (Datamodel/rowscratch.h). Answered UNCONDITIONALLY once covered -- no
        contains() guard -- because "no entry" is the store's real answer for a
        row nothing has decoded or cached, and it is the same answer the unset
        item gives: an invalid QVariant. Guarding on contains() would fall
        through to the items and hide exactly the case the store exists for. */
    if (idx.isValid() &&
        ScratchStore::covers(idx.column(), role))
    {
        return scratchStore.value(idx.row(), idx.column());
    }

    if (idx.isValid() && role == Qt::TextAlignmentRole) {
        const QVariant a = columnAlignment(idx.column());
        if (a.isValid()) return a;
    }

    /*  TOOLTIPS ARE DERIVED, NOT STORED. Every per-row Qt::ToolTipRole write
        addFileDataForRow and addMetadataForItem used to make wrote the CELL'S
        OWN TEXT back into a second role of the same cell -- 15 of them, on
        every row. That is not a fact about the image, it is a restatement of
        one, and it kept a QStandardItem alive on 15 columns per row purely to
        hold a copy of a string the store already has. Derived here instead.

        The three keyword columns hold a QStringList and their tooltip was the
        joined form, so they are joined with the same Utilities function rather
        than with QVariant::toString (which returns nothing for a list). */
    if (idx.isValid() && role == Qt::ToolTipRole && columnTooltipIsItsOwnText(idx.column())) {
        const QVariant v = data(idx, Qt::EditRole);
        if (!v.isValid()) return QVariant();
        if (v.typeId() == QMetaType::QStringList)
            return Utilities::stringListToString(v.toStringList());
        return v.toString();
    }

    /*  Thumbnails come from the path-keyed icon store (Datamodel/iconstore.h).
        Column 0 only, which is where they have always lived. An absent icon
        returns an INVALID QVariant, not an empty QIcon: every caller tests
        isNull() on the variant to decide whether a thumbnail exists yet. */
    if (idx.isValid() && role == Qt::DecorationRole && idx.column() == 0) {
        const QString fPath = rowStore.contains(idx.row())
                                  ? rowStore.value(idx.row(), G::PathColumn, G::PathRole).toString()
                                  : QString();
        if (!fPath.isEmpty()) {
            const QIcon ic = iconStore.icon(fPath);
            if (!ic.isNull()) return QVariant(ic);
            return QVariant();
        }
    }
    /*  THE LAST ROLE THAT IS NOT IN A STORE -- see mIssueLists in datamodel.h. */
    if (idx.isValid() && idx.column() == G::ErrColumn && role == Qt::UserRole)
        return mIssueLists.value(idx.row());

    /*  Everything else is genuinely unset, which is what the QStandardItem it
        replaced returned for a cell nobody wrote. */
    return QVariant();
}

bool DataModel::setData(const QModelIndex &idx, const QVariant &value, int role)
{
/*
    Intercept writes to the three columns whose totals drive load-completion
    checks, maintaining a running count so isMetaReadFinished() and the
    icon-chunk check don't have to rescan the model on every row.

    QStandardItem stores Qt::EditRole and Qt::DisplayRole in the same slot, so
    the bool flags (written with the default EditRole, read elsewhere with
    DisplayRole) are tracked only for those two roles. Alignment/tooltip/etc.
    writes on the same columns use other roles and are ignored. Counting only
    false↔true transitions keeps each row's contribution at 0 or 1, so the
    count never exceeds rowCount().
*/
    const int col = idx.column();
    const bool isStatusCol = (col == G::MetadataStatusColumn);
    const bool isBoolCol   = (col == G::IconLoadedColumn || col == G::VideoColumn);
    const bool isCacheMBCol = (col == G::CacheSizeColumn);
    /*  Availability is tracked for the same reason Video is: a row whose file is on an
        unplugged drive or gone can never carry an icon, so it must not hold the icon
        chunk open. Written once per set, off the availability pass. */
    const bool isAvailCol  = (col == G::AvailabilityColumn);
    const bool track =
        idx.isValid() &&
        (role == Qt::EditRole || role == Qt::DisplayRole) &&
        (isStatusCol || isBoolCol || isAvailCol);

    /*  THE OLD VALUE COMES FROM data(), NOT FROM THE ITEM. All three tracked
        columns are covered by the row store and the items no longer hold them,
        so QStandardItemModel::data() here would read an unset cell every time,
        every transition would look like a change from false, and the counts
        that isMetaReadFinished() and the icon-chunk check depend on would drift
        upward until they exceeded rowCount(). */
    int  oldStatus = G::MetaNotAttempted;
    int  oldAvail = int(Catalog::Availability::Present);
    bool oldVal = false;
    if (track) {
        if (isStatusCol)     oldStatus = data(idx, Qt::DisplayRole).toInt();
        else if (isAvailCol) oldAvail  = data(idx, Qt::DisplayRole).toInt();
        else                 oldVal    = data(idx, Qt::DisplayRole).toBool();
    }

    /*  THE ITEM WRITE IS GONE FOR EVERY COVERED (COLUMN, ROLE).

        This is the end of the shadow phase. Until now the stores were written
        BESIDE the QStandardItems so verifyRowStore() could compare the two;
        that comparison is what proved every column, and it is retired here
        because there is no longer a second copy to compare against. It was
        spent once, on all ~60 covered columns together, deliberately.

        What replaces the base class: the store write below, and the
        dataChanged() the base class used to emit. Everything the views, the
        proxy and the delegates do still goes through data(), which has served
        these columns from the stores since the shadow phase, so none of them
        can tell the difference -- which was the point of introducing the seam
        there rather than swapping the base class.

        THE ICON COUNTS AS COVERED even though neither store holds it: it lives
        in the path-keyed IconStore, handled a few lines below. Writing it to
        the item as well was pure waste -- nothing read it back from there --
        and it was the reason column 0 still had a QStandardItem on EVERY row
        after the value columns had gone. */
    if (!idx.isValid()) return false;

    /*  THE ONE ROLE THAT IS NOT IN A STORE, handled explicitly rather than by
        an "anything else" hash -- see mIssueLists in datamodel.h. */
    if (col == G::ErrColumn && role == Qt::UserRole) {
        mIssueLists.insert(idx.row(), value);
        emit dataChanged(idx, idx, { role });
        return true;
    }

    /*  There is no base class to fall through to any more: a role neither store
        claims, and that is not one of the two handled here, is DROPPED. That is
        deliberate. A general escape hatch would silently absorb the next role
        somebody adds and it would work by accident, which is precisely how a
        column ends up outside every store without anyone noticing. Every
        setData in the codebase that targets THIS model was enumerated before
        the swap; the only uncovered one is the issue list above. */
    const bool isIcon = (role == Qt::DecorationRole && col == 0);
    const bool covered = isIcon || RowStore::covers(col, role) ||
                         ScratchStore::covers(col, role);
    const bool ok = covered;

    if (track && ok) {
        if (isStatusCol) {
            // Tri-state column feeds two counts: "attempted" (Failed or Loaded)
            // and "loaded" (Loaded only). Each row contributes 0 or 1 to each.
            const int newStatus = value.toInt();
            if (newStatus != oldStatus) {
                const bool wasAttempted = (oldStatus != G::MetaNotAttempted);
                const bool isAttempted  = (newStatus != G::MetaNotAttempted);
                if (isAttempted != wasAttempted)
                    metadataAttemptedCount.fetch_add(isAttempted ? 1 : -1, std::memory_order_relaxed);
                const bool wasLoaded = (oldStatus == G::MetaLoaded);
                const bool isLoaded  = (newStatus == G::MetaLoaded);
                if (isLoaded != wasLoaded)
                    metadataLoadedCount.fetch_add(isLoaded ? 1 : -1, std::memory_order_relaxed);
            }
        }
        else if (isAvailCol) {
            /*  Present -> not Present makes a row unloadable, and back again makes it
                loadable: a drive being plugged in is exactly that transition. Counting
                only the crossings keeps each row's contribution at 0 or 1. */
            const bool wasUnloadable = oldAvail != int(Catalog::Availability::Present);
            const bool isUnloadable  = value.toInt() != int(Catalog::Availability::Present);
            if (isUnloadable != wasUnloadable)
                iconUnloadableCount.fetch_add(isUnloadable ? 1 : -1,
                                              std::memory_order_relaxed);
        }
        else {
            const bool newVal = value.toBool();
            if (newVal != oldVal) {
                std::atomic<int> *counter =
                    col == G::IconLoadedColumn ? &iconLoadedCount : &videoRowCount;
                counter->fetch_add(newVal ? 1 : -1, std::memory_order_relaxed);
            }
        }
    }

    /*  Maintain the packed row store (Datamodel/imagerow.h) beside the items.
        RowStore::covers(column, role) is the SINGLE authority on what the
        store holds, and it is asked here and in data() with nothing else in
        front of it. There used to be an extra "EditRole or DisplayRole" guard
        on THIS side only, left over from before coverage became role-aware --
        so G::PathRole writes were dropped while data() happily served PathRole
        FROM the store, and every path read came back empty. No thumbnails, no
        loupe image: everything downstream needs the path to find the file. Two
        guards that must agree is one guard too many. */
    /*  Thumbnails go to the icon store rather than onto the row. An empty or
        null value is a REMOVE -- that is how clearIconsOutsideChunkRange and
        the fileops icon reset already say "drop this thumbnail", so the icon
        policy keeps working unchanged. */
    /*  NOT gated on ok. ok is what QStandardItemModel made of the write, and the
        item no longer holds the thumbnail -- so a removal (setData with an empty
        QVariant, which is how clearIconsOutsideChunkRange drops an icon) must
        reach the store whatever the base class decides to do with the cell. */
    if (role == Qt::DecorationRole && col == 0) {
        const QString fPath = rowStore.contains(idx.row())
                                  ? rowStore.value(idx.row(), G::PathColumn, G::PathRole).toString()
                                  : QString();
        if (!fPath.isEmpty()) {
            const QIcon ic = qvariant_cast<QIcon>(value);
            if (!value.isValid() || ic.isNull()) iconStore.remove(fPath);
            else                                 iconStore.insert(fPath, ic);
        }
    }

    if (RowStore::covers(col, role)) {
        rowStore.setValue(idx.row(), col, role, value);
        emit dataChanged(idx, idx, { role });
    }

    /*  The scratch side table, on the same terms. ScratchStore::covers is the
        single authority on both sides here too. */
    if (ScratchStore::covers(col, role)) {
        scratchStore.setValue(idx.row(), col, value);
        emit dataChanged(idx, idx, { role });
    }

    /*  Mirror the same facts into the lock-free per-row array the worker
        threads read (Datamodel/modelsync.h). This is the ONE place they are
        maintained, for the same reason the counters above live here: every
        write to these columns goes through setData, so nothing can set one and
        forget to publish it. CacheSize rides along because ImageCache budgets
        its target range on it and it is refined when the metadata arrives. */
    if (ok && idx.isValid() && (role == Qt::EditRole || role == Qt::DisplayRole)) {
        if (track || isCacheMBCol) {
            RowSyncPtr rs;
            { QMutexLocker lock(&mSyncMutex); rs = mRowSync; }
            if (rs) {
                const int row = idx.row();
                if (isStatusCol) {
                    const int st = value.toInt();
                    rs->setFlag(row, RowSync::MetaAttempted, st != G::MetaNotAttempted);
                    rs->setFlag(row, RowSync::MetaLoaded,    st == G::MetaLoaded);
                }
                else if (col == G::IconLoadedColumn) {
                    rs->setFlag(row, RowSync::IconLoaded, value.toBool());
                }
                else if (col == G::VideoColumn) {
                    rs->setFlag(row, RowSync::IsVideo, value.toBool());
                }
                else if (isCacheMBCol) {
                    rs->setCacheMB(row, value.toFloat());
                }
            }
        }
    }
    return ok;
}

void DataModel::recountLoadFlags()
{
/*
    Resync the running counts with a single full scan. Called after structural
    row removals (remove/removeFolder use removeRows, which bypasses setData).
*/
    if (G::isLogger) G::log("DataModel::recountLoadFlags");
    int meta = 0, loaded = 0, icon = 0, video = 0, unloadable = 0;
    const int n = rowCount();
    for (int row = 0; row < n; ++row) {
        const int status = index(row, G::MetadataStatusColumn).data().toInt();
        if (status != G::MetaNotAttempted)                          ++meta;
        if (status == G::MetaLoaded)                                ++loaded;
        if (index(row, G::IconLoadedColumn).data().toBool())        ++icon;
        if (index(row, G::VideoColumn).data().toBool())             ++video;
        if (index(row, G::AvailabilityColumn).data().toInt()
                != int(Catalog::Availability::Present))             ++unloadable;
    }
    metadataAttemptedCount.store(meta,  std::memory_order_relaxed);
    metadataLoadedCount.store(loaded,   std::memory_order_relaxed);
    iconLoadedCount.store(icon,         std::memory_order_relaxed);
    videoRowCount.store(video,          std::memory_order_relaxed);
    iconUnloadableCount.store(unloadable, std::memory_order_relaxed);
}

void DataModel::newInstance()
{
    if (G::isLogger || G::isFlowLogger) G::log("DataModel::newInstance");
    int next = ++instance;
    G::dmInstance = next;
}

bool DataModel::lessThan(const QFileInfo &i1, const QFileInfo &i2)
{
/*
    The datamodel is sorted by absolute path, except jpg extensions follow all
    other image extensions. This makes it easier to determine duplicate images
    when combined raw+jpg is activated.
*/
    QString s1 = i1.absoluteFilePath().toLower();
    QString s2 = i2.absoluteFilePath().toLower();
    return s1 < s2;
}

bool DataModel::lessThanCombineRawJpg(const QFileInfo &i1, const QFileInfo &i2)
{
    /*
    The datamodel is sorted by absolute path, except jpg extensions follow all
    other image extensions. This makes it easier to determine duplicate images
    when combined raw+jpg is activated.
*/
    QString s1 = i1.absoluteFilePath().toLower();
    QString s2 = i2.absoluteFilePath().toLower();
    // check if combined raw+jpg duplicates
    if (i1.completeBaseName() == i2.completeBaseName()) {
        if (i1.suffix().toLower() == "jpg" || i1.suffix().toLower() == "jpeg") {
            s1 = i1.absolutePath().toLower() + "/" + i1.completeBaseName().toLower() + ".zzz";
        }
        if (i2.suffix().toLower() == "jpg" || i2.suffix().toLower() == "jpeg") {
            s2 = i2.absolutePath().toLower() + "/" + i2.completeBaseName().toLower() + ".zzz";
        }

        // if (i1.suffix().toLower() == "jpg") s1.replace(".jpg", ".zzz");
        // if (i2.suffix().toLower() == "jpg") s2.replace(".jpg", ".zzz");
        // if (i1.suffix().toLower() == "jpeg") s1.replace(".jpeg", ".zzz");
        // if (i2.suffix().toLower() == "jpeg") s2.replace(".jpeg", ".zzz");
    }
    return s1 < s2;
}

int DataModel::insert(QString fPath)
{
/*
    Called by MW::dmInsert. (fileOperations.cpp)

    Insert a new image into the data model.  Use when a new image is created by embel
    export or meanStack to quickly refresh the active folder with the just saved image.

    The datamodel must already contain the fPath folder.

    After insertion, the call function should select row: sel->select(fPath);
    This will invoke MetaRead which will load the metadata, icon and imageCache.
*/
    if (G::isLogger) G::log("DataModel::insert");
    if (isDebug) {
        qDebug() << "DataModel::insert"
                 << "instance =" << instance
                 << "fPath =" << fPath;
    }

    QFileInfo insertFileInfo(fPath);
    QString insertFilePath = fPath.toLower();

    // find insertion row
    int dmRow;
    for (dmRow = 0; dmRow < rowCount(); ++dmRow) {
        QString rowPath = index(dmRow, 0).data(G::PathRole).toString().toLower();
        if (insertFilePath < rowPath) {
            break;
        }
    }

    // insert new row
    insertRows(dmRow, 1);

    // update fPathRow hash
    rebuildRowFromPathHash();

    // update current row
    setCurrent(currentFilePath, instance);

    // add the file data to datamodel
    addFileDataForRow(dmRow, insertFileInfo);

    // reset loaded flags so MetaRead knows to load
    G::allMetadataAttempted = false;
    G::iconChunkLoaded = false;
    return dmRow;
}

void DataModel::remove(QString fPath)
{
/*
    Delete a row from the data model matching the absolute path. This is used when
    an image file has been deleted by Winnow. Also update fPathRow.
*/
    if (G::isLogger) G::log("DataModel::remove");
    if (isDebug)
        qDebug() << "DataModel::remove" << "instance =" << instance << fPath;

    // do not use a mutex here  rgh 2025-04-10

    // remove row from datamodel
    int row;
    for (row = 0; row < rowCount(); ++row) {
        QString rowPath = index(row, 0).data(G::PathRole).toString();
        if (rowPath == fPath) {
            QModelIndex par = QModelIndex();
            removeRow(row);
            break;
        }
    }

    // rebuild fPathRow hash
    rebuildRowFromPathHash();

    // removeRow bypasses setData, so resync the running load-flag counts
    recountLoadFlags();

    // update current index
    int last = sf->rowCount() - 1;;
    currentSfRow <= last ? row = currentSfRow : row = last;
    QModelIndex sfIdx = sf->index(row,0);
    setCurrentSF(sfIdx, instance);
}

/* MULTI-SELECT FOLDERS SECTION

    When a folder is selected it is added to the folderQueue. If a folder is unselected,
    it is added to the folderQueue, but the add flag is set to false, and the folder
    images are removed from the datamodel.

    When a folderQueue item (a folder) is processed, all the eligible images in the
    folder are added to the datamodel:
    - add each image file to the datamodel with QFileInfo related data such as file
      name, path, file size, creation date...
    - also determine if there are duplicate raw+jpg files, and if so, populate all
      the Dup...Role values to manage the raw+jpg files

    After the folder has been processed, MW calls MetaRead, where the iamge metadata
    and thumbnails are read and addMetadataForItem is signaled.

*/

bool DataModel::isQueueEmpty()
{
    return folderQueue.isEmpty();
}

void DataModel::enqueueOp(const QString& folderPath, G::FolderOp op)
{
    // Toggle logic is handled by caller; here we only queue valid transitions.
    const bool have = folderSet.contains(folderPath);

    if (op == G::FolderOp::Toggle) {
        have ? op = G::FolderOp::Remove : op = G::FolderOp::Add;
    }

    if (op == G::FolderOp::Add && !have) {
        if (!pendingPaths.contains(folderPath)) {
            pendingPaths.insert(folderPath);
            folderQueue.enqueue({folderPath, G::FolderOp::Add});
        }
    }
    else if (op == G::FolderOp::Remove && have) {
        if (!pendingPaths.contains(folderPath)) {
            pendingPaths.insert(folderPath);
            folderQueue.enqueue({folderPath, G::FolderOp::Remove});
        }
    }
}

void DataModel::setScope(const ScopeRequest &req)
{
/*
    FILL THE MODEL WITH THE SET THE REQUEST DESCRIBES -- the one entry point, replacing
    the two the model used to expose (enqueueFolderSelection for a folder, addPaths for a
    search result). See ScopeRequest in the header for why they are one thing.

    THE DIFFERENCE IS reconcile, AND NOTHING ELSE. A folder scope walks the filesystem,
    because the directory listing IS the set: what is on disk right now is the answer,
    and the index is at best a cache of it. A catalog scope does not, because there is no
    directory to walk -- the results come from a hundred folders -- so the resolved result
    is the set. That result arrives in one of two shapes: whole ROWS from the index, which
    fill the model without opening or stat'ing anything (addCatalogRows), or bare paths,
    each stat'd as it is added (addPaths).

    THE REQUEST IS REMEMBERED whether or not it changed anything. Everything that wants
    to reload, refresh or write back what is loaded has had to infer it from folderList
    until now, which cannot distinguish "these folders" from "a search that happened to
    match images in these folders".

    NOT QUEUED HERE, deliberately. Both callers already defer this onto the event loop --
    a load must not run inside the signal that asked for it, since MW::stop has just torn
    down the reader threads -- and the catalog caller has work to do immediately after the
    fill. Queueing here as well would put that work before the fill instead of after it.
*/
    QString fun = "DataModel::setScope";
    if (G::isLogger || G::isFlowLogger)
        G::log(fun, req.reconcile ? req.query.folder
                                  : QString::number(req.paths.size()) + " paths");

    /* An append leaves the previous request in place: what is loaded is now the union of
       two searches, and neither of them alone describes it. */
    if (!req.append) currentScope = req;

    if (req.reconcile)
        enqueueFolderSelection(req.query.folder, req.op, req.recurse, req.subDirs);
    else if (!req.rows.isEmpty())
        addCatalogRows(req.rows);
    else
        addPaths(req.paths);
}

void DataModel::enqueueFolderSelection(const QString& folderPath,
                                       G::FolderOp op, bool recurse,
                                       const QStringList &subDirs)
{
/*
    op = Add or Delete images in folderPath from datamodel
    recurse = recurse all subfolders of folderPath
    subDirs = subfolder paths discovered by Utilities::subFolderTree;
              consumed directly so we don't re-walk the tree here.
*/

    if (recurse) {
        // if (folderPath.endsWith(".photoslibrary")) return;

        /* Filter macOS .photoslibrary bundles out of the recursion list.
           These hold thousands of derivative preview/master images per
           photo and reliably blow the memory cap. Skip both the bundle
           itself and anything nested inside one. The user can still open
           a .photoslibrary explicitly (non-recurse) if they want. */
        QStringList filtered;
        filtered.reserve(subDirs.size());
        for (const QString &p : subDirs) {
            // if (p.endsWith(".photoslibrary")) continue;
            if (p.contains(".photoslibrary")) continue;
            filtered.append(p);
        }
        // Keep the displayed total consistent with what we'll process.
        subFolderTreeCount = static_cast<quint32>(filtered.size());

        enqueueOp(folderPath, op);

        const QString total = QString::number(subFolderTreeCount);
        QElapsedTimer lastEmit;
        lastEmit.start();
        constexpr qint64 kEmitIntervalMs = 50;
        int i = 0;
        const int n = filtered.size();
        for (const QString &p : filtered) {
            if (abort) return;
            ++i;
            // Throttle UI updates: emit and process events at most every
            // ~50 ms (and always for the final one).
            if (lastEmit.elapsed() >= kEmitIntervalMs || i == n) {
                emit updateStatus(false,
                    "Progress: " + QString::number(i) + " of " + total + " subfolders",
                    "");
                qApp->processEvents();  // req'd for status update and abort
                lastEmit.restart();
            }
            enqueueOp(p, op);
        }
    } else {
        enqueueOp(folderPath, op);
    }

    scheduleProcessing();
}

void DataModel::scheduleProcessing()
{
    if (G::isLogger)
        G::log("", "Folder count = " + QVariant(folderQueue.count()).toString());

    if (isProcessingFolders) return;
    isProcessingFolders = true;

    /* Fresh throttle window per load so the first folder's progress message fires
       immediately (subsequent folders are throttled in addFolder). */
    centralMsgTimer.invalidate();

    /* Phase 1 perf probe: start a dedicated wall timer (G::t is unreliable here — G::log
       restarts it) and zero the per-load accumulators. */
    if (G::isPerfProbe) {
        perfEnumNs = 0;
        perfSortNs = 0;
        perfMsgNs = 0;
        perfInsertNs = 0;
        perfFolders = 0;
        perfLoadTimer.start();
    }

    /* Batched load: turn off the proxy's dynamic sort/filter for the duration so the one
       wide dataChanged per folder (addFolder) does not re-sort the inserted block Z-A.
       Inserted rows still map in source (lessThan / name) order; a single re-sort pass is
       reapplied in restoreProxySortAfterLoad() when the load finishes or aborts. */
    if (G::useBatchedFolderInsert && !sfSortDisabledForLoad) {
        sfSortDisabledForLoad = true;
        sf->setDynamicSortFilter(false);
    }

    processNextBatch();
}

void DataModel::processNextBatch()
{
    /* Folders are processed in time-sliced batches, yielding to the GUI event
       loop between slices so the UI stays responsive while a large (especially
       recursive) tree loads. The yield is time-budgeted, not folder-counted:
       addFolder cost grows with the model size (the sorted proxy re-inserts
       each new row at O(N)), so a fixed folder count per tick produced multi-
       second GUI freezes late in a big load (measured 3.4 s blocks). Capping a
       tick at ~30 ms keeps each freeze to roughly one folder's insert time and
       returns control to the event loop ~30×/s. kMaxFoldersPerTick remains as a
       secondary guard so tiny folders still yield periodically. The remaining
       O(N²) total-load cost is unchanged by this; only responsiveness is. */
    constexpr int    kMaxFoldersPerTick = 64;
    constexpr qint64 kTimeBudgetMs      = 30;

    QElapsedTimer tickTimer;
    tickTimer.start();

    int processed = 0;
    while (processed < kMaxFoldersPerTick && !folderQueue.isEmpty() && !G::stop) {
        if (abort) {
            qDebug() << "processNextBatch1";
            restoreProxySortAfterLoad();
            emit folderChange(abort);
            return;
        }
        auto [folderPath, op] = folderQueue.dequeue();
        pendingPaths.remove(folderPath); // it's leaving the queue now

        if (op == G::FolderOp::Add) {
            // NOTE: ensure addFolder() is fast/non-blocking or hands work to workers.
            // qDebug() << "processNextBatch" << folderPath;
            QString step = "Loading folder " + folderPath + ".\n";
            QString escapeClause = "\nPress \"Esc\" to stop.";
            // emit centralMsg(step + escapeClause);
            addFolder(folderPath);
        } else {
            removeFolder(folderPath);
        }
        ++processed;

        // Yield to the event loop once this tick's time budget is spent; the
        // reschedule below picks up the remaining queue on the next tick.
        if (tickTimer.elapsed() >= kTimeBudgetMs) break;
    }

    if (G::stop) {
        // Optional policy: drop remaining queued work; or keep it to resume later.
        // Here we drop and notify.
        folderQueue.clear();
        pendingPaths.clear();
        isProcessingFolders = false;
        qDebug() << "processNextBatch2";
        restoreProxySortAfterLoad();
        emit folderChange(abort);  // state changed (cleared)
        return;
    }

    if (!folderQueue.isEmpty()) {
        // More to do—schedule next tick.
        QTimer::singleShot(0, this, &DataModel::processNextBatch);
        return;
    }

    // All done.
    isProcessingFolders = false;

    /* Reapply proxy sort/filter in one pass (no-op if it was never disabled). Done before
       the perf report so the wall time includes this final re-sort cost. */
    restoreProxySortAfterLoad();

    if (G::isPerfProbe) {
        const qint64 wallMs = perfLoadTimer.elapsed();
        const double measuredMs =
            (perfEnumNs + perfSortNs + perfMsgNs + perfInsertNs) / 1.0e6;
        qDebug().noquote()
            << "[PERF] Phase1 load"
            << " rows="     << rowCount()
            << " folders="  << perfFolders
            << " enum(ms)=" << QString::number(perfEnumNs / 1.0e6, 'f', 1)
            << " sort(ms)=" << QString::number(perfSortNs / 1.0e6, 'f', 1)
            << " msg(ms)="  << QString::number(perfMsgNs / 1.0e6, 'f', 1)
            << " insert(ms)=" << QString::number(perfInsertNs / 1.0e6, 'f', 1)
            << " other(ms)=" << QString::number(wallMs - measuredMs, 'f', 1)  // event-loop yield / paint / restore
            << " wall(ms)=" << wallMs
            << " batched="  << G::useBatchedFolderInsert;
    }

    emit folderChange(abort);
}
// */

void DataModel::restoreProxySortAfterLoad()
{
    /* Idempotent: only acts if scheduleProcessing turned dynamic sort off for a batched
       load. Re-enabling dynamic sort ALONE replays the proxy's retained sort column/order,
       which can be descending (→ Z-A). The load is meant to show source (name) order
       (folderChangeCompleted: "must retain default order"), so force source order with
       sort(-1) before re-enabling dynamic; sortColumn is then -1 so nothing is replayed. */
    if (!sfSortDisabledForLoad) return;
    sfSortDisabledForLoad = false;
    /*  TIMED SEPARATELY, and printed for a large set rather than only under
        G::isPerfProbe. Both calls make the proxy re-examine every row -- sort(-1) to
        return to source order, then re-enabling dynamic sort -- and each emits
        layoutChanged, which the three views answer over the whole model. At 43,000 rows
        that is a candidate for the stall that begins the moment the fill ends. */
    const bool probeBig = G::isPerfProbe;
    QElapsedTimer rt;
    if (probeBig) {
        rt.start();
        qDebug().noquote() << "[PERF] restore sort: sortColumn=" << sf->sortColumn()
                           << "sortOrder=" << (sf->sortOrder() == Qt::DescendingOrder ? "Desc" : "Asc");
    }
    sf->sort(-1);
    const qint64 sortNs = probeBig ? rt.nsecsElapsed() : 0;
    if (probeBig) rt.restart();
    sf->setDynamicSortFilter(true);
    if (probeBig)
        qDebug().noquote() << "[PERF] restoreProxySortAfterLoad  sort(-1)"
                           << (sortNs / 1000000.0) << "ms  setDynamicSortFilter"
                           << (rt.nsecsElapsed() / 1000000.0) << "ms";
}

void DataModel::addFolder(const QString &folderPath)
{
    QString fun = "DataModel::addFolder";
    if (G::isLogger || G::isFlowLogger)
        G::log(fun, folderPath);

    QMutexLocker locker(&dmMutex);
    abort = false;
    folderList.append(folderPath);
    folderSet.insert(folderPath);
    loadingModel = true;
    locker.unlock(); // Unlock the queue while processing

    const bool probe = G::isPerfProbe;
    QElapsedTimer pt;
    if (probe) pt.start();

    /* Folder file list. entryList (names only, no QFileInfo/stat) + an O(1) suffix check
       against supportedExtSet replaces dir.setNameFilters(*fileFilters)+entryInfoList():
       QDir compiled ~50 wildcard patterns to QRegularExpression on EVERY folder (~66k
       compiles over a 1333-folder tree). QFileInfo is constructed only for eligible files,
       and QDir::NoSort skips QDir's own sort (we std::sort below regardless). */
    QDir dir(folderPath);
    const QStringList names = dir.entryList(QDir::Files, QDir::NoSort);
    QList<QFileInfo> folderFileInfoList;
    folderFileInfoList.reserve(names.size());
    for (const QString &name : names) {
        const int dot = name.lastIndexOf('.');
        if (dot < 0) continue;
        if (supportedExtSet.contains(name.mid(dot + 1).toLower()))
            folderFileInfoList.append(QFileInfo(dir.filePath(name)));
    }

    if (probe) { perfEnumNs += pt.nsecsElapsed(); pt.restart(); }

    if (combineRawJpg) {
        // make sure, if raw+jpg pair, that raw file is first to make combining easier
        std::sort(folderFileInfoList.begin(), folderFileInfoList.end(), lessThanCombineRawJpg);
    }
    else {
        std::sort(folderFileInfoList.begin(), folderFileInfoList.end(), lessThan);
    }

    if (probe) { perfSortNs += pt.nsecsElapsed(); pt.restart(); }

    /* Progress message. emit centralMsg drives MW::setCentralMessage, which does a
       synchronous repaint(); once per folder this cost ~1.3 s over a 1333-folder tree.
       Throttle to ~50 ms (the counter still advances every folder for accuracy). The first
       folder of a load always emits (centralMsgTimer invalidated in scheduleProcessing). */
    ++subFolderTreeCounter;
    constexpr qint64 kCentralMsgMs = 50;
    bool doEmit = true;
    if (G::throttleFolderLoadMsg) {
        doEmit = !centralMsgTimer.isValid() || centralMsgTimer.elapsed() >= kCentralMsgMs;
    }
    if (doEmit) {
        centralMsgTimer.restart();
        QString progress = "Searching for images in: " + QString::number(subFolderTreeCounter) +
                           " of " + QVariant(subFolderTreeCount).toString() + " subfolders";
        QString step = "Loading eligible image file information.<br>" + progress + "<br>";
        emit centralMsg(step + "Press \"Esc\" to stop.");
    }

    if (probe) { perfMsgNs += pt.nsecsElapsed(); pt.restart(); }

    // datamodel size
    int row = rowCount();
    int oldRowCount = rowCount();
    int newRowCount = oldRowCount;

    if (G::useBatchedFolderInsert) {
        /* Add the whole folder in one structural insert and fill the cells with the model's
           signals blocked, emitting a single dataChanged for the range. This replaces N
           rowsInserted (each an O(N) view relayout) + ~20N dataChanged per folder — the
           enumeration freeze. Order is unchanged (rows appended in the same sorted
           folderFileInfoList order; the proxy mirrors source order during load). */
        QList<QFileInfo> valid;
        valid.reserve(folderFileInfoList.size());
        for (const QFileInfo &fileInfo : folderFileInfoList) {
            if (abort) break;
            if (fPathRowContains(fileInfo.filePath())) continue;   // already in (multi-folder)
            if (fileInfo.size() == 0) continue;                    // skip zero-size
            valid.append(fileInfo);
        }
        if (abort) { endLoad(false); return; }
        if (!valid.isEmpty()) {
            const int first = row;
            insertRows(row, valid.size());              // ONE rowsInserted for the batch
            {
                const QSignalBlocker blocker(this);     // suppress per-cell dataChanged
                for (const QFileInfo &fileInfo : valid) {
                    addFileDataForRow(row, fileInfo);
                    row++;
                }
            }
            emit dataChanged(index(first, 0), index(row - 1, columnCount() - 1));
            // the batch's paths exist now; see endLoad for why this is here
            resizeRowSync(rowCount());
            rebuildProxySnapshot();
        }
    }
    else
    for (const QFileInfo &fileInfo : folderFileInfoList) {
        // check for escape key release triggering abort
        if (abort) {
            endLoad(false);
            break;
        }

        // skip if already in datamodel.  This happens when multiple folders selected.
        QString fPath = fileInfo.filePath();
        if (fPathRowContains(fPath)) continue;

        // do not include zero size files
        if (fileInfo.size() == 0) {
            continue;
        }

        insertRows(row, 1);

        // ALL RAW+JPG pairing logic is now encapsulated inside here
        addFileDataForRow(row, fileInfo);

        row++;
    }

    if (probe) { perfInsertNs += pt.nsecsElapsed(); perfFolders++; }

    if (abort) return;

    int folderRowCount = row - newRowCount;
    newRowCount = row;

    if (oldRowCount == 0 && newRowCount > 0) {
        firstFolderPathWithImages = folderPath;
        setCurrent(index(0, 0), instance);
    }

    // update folder image count
    folderImageCount[folderPath] = folderRowCount;

    // qDebug() << "DataModel::addFolder"
    //          << "rowCount =" << rowCount()
    //          << "memMB =" << G::processFootprintMB()
    //          << folderPath;

    if (folderQueue.isEmpty()) {
        endLoad(true);
    }
}

void DataModel::addCatalogRows(const QVector<CatalogRow> &rows)
{
/*
    Fill the model from rows the CATALOG resolved -- Catalog::searchRows -- without
    opening or stat'ing a single file.

    WHY THIS IS NOT addPaths. addPaths takes a list of file names, stats each one to prove
    it is there, and leaves every row MetaNotAttempted so the reader opens it and walks
    its header. Measured on a 43,050-row catalog that is ~1.6 ms per row: over a minute
    before a filter or a sort can run, for facts the index was already holding. Here the
    row arrives complete -- it IS the metadata -- so the model is usable as soon as the
    insert finishes.

    THE ROWS ARE MARKED MetaLoaded, which is what makes MetaRead skip them:
    MetaRead::needToRead keys on metaAttemptedAt() and iconLoadedAt(), so a hydrated row
    is never read for metadata and still collected for its ICON. That is the correct
    split -- the icon is the one thing the index cannot supply from this table, and the
    thumbnail cache serves it without the file when it can.

    NOTHING HERE PROVES THE FILE EXISTS. That is deliberate and is the difference the
    Availability column carries: a catalog row can name a file on an unplugged drive, and
    the useful thing to do with it is show it marked rather than drop it. MW asks
    Catalog::availabilityOf for the whole set, once, off the GUI thread.

    THE CALLER HAS ALREADY STOPPED the previous load and reset the caches (see
    MW::loadCatalogResults) -- this is the model half only.
*/
    QString fun = "DataModel::addCatalogRows";
    if (G::isLogger || G::isFlowLogger)
        G::log(fun, QString::number(rows.size()) + " rows");

    QMutexLocker locker(&dmMutex);
    abort = false;
    loadingModel = true;
    locker.unlock();

    /* Same proxy bracket, and needed for the same reason, as addPaths and
       scheduleProcessing: one wide dataChanged over an inserted block would otherwise
       make the sorted proxy re-sort it. */
    if (G::useBatchedFolderInsert && !sfSortDisabledForLoad) {
        sfSortDisabledForLoad = true;
        sf->setDynamicSortFilter(false);
    }

    /*  What is actually new: a path already in the model is not loaded twice, which is
        what makes Add (a second search onto the first) work. */
    perfFillInsertNs = perfFillFileDataNs = perfFillIndexFillNs = 0;
    perfFillAddMetaNs = perfFillEmitNs = perfFillPrepNs = 0;
    QElapsedTimer prepTimer;
    if (G::isPerfProbe) prepTimer.start();

    QVector<CatalogRow> ordered;
    ordered.reserve(rows.size());
    for (const CatalogRow &r : rows) {
        if (abort) break;
        if (r.path.isEmpty()) continue;
        if (fPathRowContains(r.path)) continue;
        ordered.append(r);
    }
    if (abort) {
        endLoad(false);
        restoreProxySortAfterLoad();
        emit folderChange(abort);
        return;
    }

    /*  ORDERED BY A PRECOMPUTED KEY, not by a comparator that builds QFileInfos.

        A sort is O(n log n) COMPARISONS, so the obvious comparator -- two QFileInfo
        constructions and absoluteFilePath/completeBaseName/suffix on each -- did that
        work over a million times for 43,000 rows: measured at 1,517 ms against 75 ms for
        the same order from keys built once per row (Winnow --catalogprobe, stage F).
        Twenty times faster, and all of it was in front of the first thumbnail.

        The key reproduces lessThan and lessThanCombineRawJpg exactly: the lower-cased
        path, with a jpg or jpeg of a raw+jpg pair rewritten to sort after its raw. */
    QVector<QPair<QString, int>> keys;
    keys.reserve(ordered.size());
    for (int i = 0; i < ordered.size(); ++i) {
        const CatalogRow &r = ordered.at(i);
        QString key = r.path.toLower();
        if (combineRawJpg) {
            const QString ext = r.ext.toLower();
            if (ext == "jpg" || ext == "jpeg") {
                const int dot = key.lastIndexOf('.');
                if (dot > 0) key = key.left(dot) + ".zzz";
            }
        }
        keys.append({key, i});
    }
    std::sort(keys.begin(), keys.end(),
              [](const QPair<QString, int> &a, const QPair<QString, int> &b) {
                  return a.first < b.first;
              });

    pendingCatalogRows.clear();
    pendingCatalogRows.reserve(ordered.size());
    for (const auto &k : keys) pendingCatalogRows.append(ordered.at(k.second));
    pendingCatalogAt = 0;

    /*  THE TRUE COUNT FROM THE FIRST BATCH. Set before any row is inserted so the status
        bar can say "1 of 43,050" while the rest are still arriving, rather than counting
        up as they land. Cleared in endLoad. */
    expectedRows = rowCount() + pendingCatalogRows.size();
    if (G::isPerfProbe) perfFillPrepNs = prepTimer.nsecsElapsed();

    insertCatalogBatch();
}

void DataModel::insertCatalogBatch()
{
/*
    ONE BATCH, THEN BACK TO THE EVENT LOOP.

    The first version of this looped over every batch and called qApp->processEvents
    between them, which did nothing at all: G::useProcessEvents is FALSE by default, so
    the pump was skipped and 43,000 rows were filled in one blocking pass -- a beachball
    with no thumbnails, which is exactly what it looked like. Posting the next batch
    instead of pumping inside a loop is not a tuning of that; it is the difference between
    a GUI that is running and one that is not, so it does not hang off a flag.

    A batch is one structural insert and one wide dataChanged, which keeps the proxy and
    the three views off the per-row signal path -- the shape that fixed the folder load.
    Between batches the event loop runs normally: thumbnails paint, the scrollbar works,
    Esc is heard.
*/
    QString fun = "DataModel::insertCatalogBatch";

    /*  abort, NOT G::stop. G::stop is owned by MW::stop, which BRACKETS this fill --
        it sets the flag, tears the readers down, queues the load and clears it -- and
        MW::stop early-returns without clearing when it is called while already stopping.
        A fill that consulted it was therefore cancelling itself on the strength of its
        own caller's flag: the headless --catalogload run aborted before its first batch
        and reported 0 rows. Esc still cancels, because MW::stop sets dm->abort too, and
        that is the flag addPaths has always used. */
    if (abort) { finishCatalogFill(); return; }

    constexpr int kInsertBatch = 2000;
    const int from = pendingCatalogAt;
    const int to = qMin(pendingCatalogRows.size(), from + kInsertBatch);
    if (from >= to) { finishCatalogFill(); return; }
    /*  The accumulators are cumulative over the whole fill, so a per-batch figure is the
        difference across this batch. Printing the cumulative total per batch would look
        like a rising cost whether or not one existed -- the exact illusion this is meant
        to test for. */
    const qint64 cellsBefore = perfFillInsertNs + perfFillFileDataNs
                             + perfFillIndexFillNs + perfFillAddMetaNs;

    int row = rowCount();
    const int firstOfBatch = row;
    /*  Under G::isPerfProbe (WINNOW_PERF_PROBE=1). It printed for any large set while this
        was being chased, because a person clicking Catalog was the only way the GUI-side
        cost of a batch could be seen at all. */
    const bool probe = G::isPerfProbe;
    QElapsedTimer pt;
    if (probe) pt.start();

    insertRows(row, to - from);
    if (probe) { perfFillInsertNs += pt.nsecsElapsed(); pt.restart(); }
    {
        const QSignalBlocker blocker(this);
        for (int i = from; i < to; ++i) {
            const CatalogRow &r = pendingCatalogRows.at(i);
            const QFileInfo fi(r.path);
            addFileDataForRow(row, fi, &r);
            if (probe) { perfFillFileDataNs += pt.nsecsElapsed(); pt.restart(); }
            /*  THE METADATA, from the same mapping the per-row path uses
                (Metadata/indexmetadata.h) so a row filled here and a row filled by
                Reader cannot come to hold different things. */
            IndexMetadata::fill(metadata->m, r,
                                QDateTime::fromSecsSinceEpoch(r.srcMtime),
                                row, instance);
            if (probe) { perfFillIndexFillNs += pt.nsecsElapsed(); pt.restart(); }
            addMetadataForItem(metadata->m, fun);
            if (probe) { perfFillAddMetaNs += pt.nsecsElapsed(); pt.restart(); }
            row++;
        }
    }
    const qint64 cellsThisBatch = perfFillInsertNs + perfFillFileDataNs
                                + perfFillIndexFillNs + perfFillAddMetaNs - cellsBefore;
    if (probe) pt.restart();
    emit dataChanged(index(firstOfBatch, 0), index(row - 1, columnCount() - 1));
    if (probe) {
        const qint64 emitNs = pt.nsecsElapsed();
        perfFillEmitNs += emitNs;
        /*  PER BATCH, because the suspicion is QUADRATIC: every batch's dataChanged is
            handled by the proxy and three views over everything inserted SO FAR, so the
            cost per batch would climb as the model grows. One number for the whole fill
            cannot show that shape; twenty-two numbers can. If instead they are flat and
            small, the stall is not here and this rules the insert out. */
        qDebug().noquote() << "[PERF] fill batch" << (firstOfBatch / 2000 + 1)
                           << " rows" << firstOfBatch << "-" << (row - 1)
                           << " insert+cells" << (cellsThisBatch / 1000000.0) << "ms"
                           << " dataChanged" << (emitNs / 1000000.0) << "ms";
    }

    pendingCatalogAt = to;

    if (pendingCatalogAt < pendingCatalogRows.size()) {
        if (expectedRows > kInsertBatch) {
            emit centralMsg(QString::number(row) + " of "
                            + QString::number(expectedRows) + " images loading...");
            emit updateProgress(1.0 * row / qMax(1, expectedRows) * 100);
        }
        QTimer::singleShot(0, this, [this]{ insertCatalogBatch(); });
        return;
    }

    finishCatalogFill();
}

void DataModel::finishCatalogFill()
{
/*
    The tail of the streamed fill: what addPaths does inline at the end of its one pass.
*/
    const bool aborted = abort;

    /*  EVERY STAGE OF THE TAIL IS TIMED, printed for a large set. The stall begins the
        instant the last batch lands, so it is in here or in what folderChange triggers --
        and there are five candidates, each O(rows). */
    const bool probeBig = G::isPerfProbe;
    QElapsedTimer ft;
    if (probeBig) ft.start();

    /* Register the folders the results came from -- what the Folders filter category,
       removeFolder and isFolderLoaded all read. */
    {
        QMutexLocker lk(&dmMutex);
        for (int i = 0; i < pendingCatalogAt; ++i) {
            const CatalogRow &r = pendingCatalogRows.at(i);
            const QString folder = r.folder.isEmpty()
                                       ? QFileInfo(r.path).absoluteDir().path()
                                       : r.folder;
            if (!folderSet.contains(folder)) {
                folderList.append(folder);
                folderSet.insert(folder);
            }
            folderImageCount[folder] = folderImageCount.value(folder) + 1;
        }
    }

    if (rowCount() > 0 && rowCount() == pendingCatalogAt) {
        firstFolderPathWithImages =
            QFileInfo(pendingCatalogRows.first().path).absoluteDir().path();
        setCurrent(index(0, 0), instance);
    }

    if (G::isPerfProbe) {
        const int n = qMax(1, pendingCatalogAt);
        const auto us = [n](qint64 ns){ return QString::number(ns / 1000.0 / n, 'f', 1); };
        qDebug().noquote()
            << "[PERF] catalog fill"
            << " rows="        << pendingCatalogAt
            << " prep(ms)="    << perfFillPrepNs / 1000000.0
            << " insertRows="  << us(perfFillInsertNs)
            << " fileData="    << us(perfFillFileDataNs)
            << " indexFill="   << us(perfFillIndexFillNs)
            << " addMeta="     << us(perfFillAddMetaNs)
            << " emit(ms)="    << perfFillEmitNs / 1000000.0
            << " us/row total="
            << us(perfFillInsertNs + perfFillFileDataNs + perfFillIndexFillNs
                  + perfFillAddMetaNs + perfFillEmitNs);
    }

    pendingCatalogRows.clear();
    pendingCatalogAt = 0;

    /*  EVERY ROW IS ATTEMPTED ALREADY, so say so: this is the flag the filters and the
        sort wait on, and nothing is going to set it later for rows no reader will visit.
        A folder load reaches the same point only after MetaRead has been round them all. */
    if (probeBig) {
        qDebug().noquote() << "[PERF] finishCatalogFill  folders+setCurrent"
                           << ft.elapsed() << "ms";
        ft.restart();
    }

    setAllMetadataAttempted(true);
    if (probeBig) { qDebug().noquote() << "[PERF] finishCatalogFill  allMetaAttempted"
                                       << ft.elapsed() << "ms"; ft.restart(); }

    endLoad(!aborted);
    if (probeBig) { qDebug().noquote() << "[PERF] finishCatalogFill  endLoad"
                                       << ft.elapsed() << "ms"; ft.restart(); }

    restoreProxySortAfterLoad();
    if (probeBig) { qDebug().noquote() << "[PERF] finishCatalogFill  restoreSort"
                                       << ft.elapsed() << "ms"; ft.restart(); }

    emit folderChange(aborted);
    if (probeBig)
        qDebug().noquote() << "[PERF] finishCatalogFill  emit folderChange"
                           << ft.elapsed() << "ms  <-- everything MW does on the load"
                              " completing is inside this number";
}

void DataModel::addPaths(const QStringList &fPaths)
{
/*
    Load an arbitrary set of image paths as ONE virtual folder -- the catalog search
    result. See notes/Documentation.txt "The Catalog (Cross-folder Search)".

    WHY THIS EXISTS ALONGSIDE addFolder. Everything else that fills the model starts from
    a directory: the folder queue's unit is a folder, and addFolder enumerates one. A
    search result is a list of files that may come from a hundred different folders, so
    there is no directory to enumerate. What it must NOT be is a special kind of model --
    the point of loading results into the datamodel is that the grid, the loupe, filters,
    sorting, ratings and Develop all work on them exactly as on a folder.

    EVERY DISTINCT PARENT FOLDER IS REGISTERED in folderList / folderSet /
    folderImageCount. Those are what the Folders filter category, removeFolder and
    isFolderLoaded all read; leaving them empty would give a model whose rows belong to no
    folder at all, and removeFolder would silently find nothing to remove.

    THE CALLER HAS ALREADY STOPPED the previous load and reset the caches (see
    MW::loadCatalogResults) -- this is the model half only.
*/
    QString fun = "DataModel::addPaths";
    if (G::isLogger || G::isFlowLogger)
        G::log(fun, QString::number(fPaths.size()) + " paths");

    QMutexLocker locker(&dmMutex);
    abort = false;
    loadingModel = true;
    locker.unlock();

    /* Same proxy bracket as scheduleProcessing, and needed for the same reason: the one
       wide dataChanged below would otherwise make the sorted proxy re-sort the inserted
       block (Z-A if the retained sort order is descending). restoreProxySortAfterLoad
       puts it back in one pass, in source order. */
    if (G::useBatchedFolderInsert && !sfSortDisabledForLoad) {
        sfSortDisabledForLoad = true;
        sf->setDynamicSortFilter(false);
    }

    /* The catalog is an index, not a guarantee: a row can name a file that has since been
       deleted or is on an unmounted volume. sweep() demotes those, but only when it has
       run since, so the existence check happens here as well -- offering a result that
       cannot be opened is worse than quietly returning fewer. */
    QList<QFileInfo> infos;
    infos.reserve(fPaths.size());
    for (const QString &fPath : fPaths) {
        if (abort) break;
        const QFileInfo fi(fPath);
        if (!fi.exists() || !fi.isFile()) continue;
        if (fi.size() == 0) continue;
        const QString suffix = fi.suffix().toLower();
        if (!supportedExtSet.contains(suffix)) continue;
        infos.append(fi);
    }
    if (abort) {
        endLoad(false);
        restoreProxySortAfterLoad();
        emit folderChange(abort);
        return;
    }

    /* Same ordering rule as addFolder, and for the same reason: with Combine Raw+Jpg on,
       the raw of a pair must precede its JPG or addFileDataForRow cannot pair them. */
    if (combineRawJpg)
        std::sort(infos.begin(), infos.end(), lessThanCombineRawJpg);
    else
        std::sort(infos.begin(), infos.end(), lessThan);

    /* One structural insert for the whole result set, cells filled with signals blocked
       and a single dataChanged for the range -- the batched path from addFolder. A
       result set is routinely thousands of rows from many folders, which is exactly the
       shape that made per-row signals the folder-load bottleneck. */
    int row = rowCount();
    const int first = row;
    QList<QFileInfo> valid;
    valid.reserve(infos.size());
    for (const QFileInfo &fi : infos) {
        if (abort) break;
        if (fPathRowContains(fi.filePath())) continue;   // already loaded
        valid.append(fi);
    }
    if (abort) {
        endLoad(false);
        restoreProxySortAfterLoad();
        emit folderChange(abort);
        return;
    }

    if (!valid.isEmpty()) {
        insertRows(row, valid.size());
        {
            const QSignalBlocker blocker(this);
            for (const QFileInfo &fi : valid) {
                addFileDataForRow(row, fi);
                row++;
            }
        }
        emit dataChanged(index(first, 0), index(row - 1, columnCount() - 1));
    }

    /* Register the folders the results came from. */
    {
        QMutexLocker lk(&dmMutex);
        for (const QFileInfo &fi : valid) {
            const QString folder = fi.absoluteDir().path();
            if (!folderSet.contains(folder)) {
                folderList.append(folder);
                folderSet.insert(folder);
            }
            folderImageCount[folder] = folderImageCount.value(folder) + 1;
        }
    }

    if (first == 0 && rowCount() > 0) {
        firstFolderPathWithImages = valid.isEmpty() ? QString()
                                                    : valid.first().absoluteDir().path();
        setCurrent(index(0, 0), instance);
    }

    endLoad(true);
    restoreProxySortAfterLoad();
    /* The same signal a folder load ends with, so MW::folderChanged and then
       folderChangeCompleted run unchanged -- which is what starts MetaRead, the icon
       chunks and the catalog commit for this set. */
    emit folderChange(abort);
}

void DataModel::removeFolder(const QString &folderPath)
{
    QString fun = "DataModel::removeFolder";
    if (G::isLogger || G::isFlowLogger) G::log(fun, folderPath);

    folderList.removeAll(folderPath);
    folderSet.remove(folderPath);
    folderImageCount.remove(folderPath);
    QModelIndex par = QModelIndex();

    // Collect all rows that need to be removed
    for (int row = rowCount() - 1; row >= 0; row--) {
        QString filePath = index(row, 0).data(G::PathRole).toString();
        QFileInfo info(filePath);
        QString rowFolder = info.dir().absolutePath();
        if (rowFolder == folderPath) {
            // do not use a mutex here
            beginRemoveRows(par, row, row);
            removeRows(row, 1);
            endRemoveRows();
        }
    }
    sf->invalidate();

    // rebuild fPathRow hash
    rebuildRowFromPathHash();

    // removeRows bypasses setData, so resync the running load-flag counts
    recountLoadFlags();

    // update current
    setCurrent(currentFilePath, instance);
    emit updateStatus(true, "", "DataModel::removeFolder");
}

void DataModel::refresh()
{
    if (G::isLogger) G::log("DataModel::refresh");

    QStringList added;
    QStringList removed;
    QStringList modified;

    if (!sourceModified(added, removed, modified)) {
        return;
    }

    // additions
    for (const QString &fPath : added) {
        insert(fPath);
    }

    // removals
    G::removingRowsFromDM = true;
    for (const QString &fPath : removed) {
        remove(fPath);
    }
    G::removingRowsFromDM = false;

    // modifications
    for (const QString &fPath : modified) {
        int row = rowFromPath(fPath);
        setData(index(row, G::MetadataStatusColumn), G::MetaNotAttempted);
        setData(index(row, G::IconLoadedColumn), false);
    }
}

QString DataModel::primaryFolderPath()
{
    if (G::isLogger) G::log("DataModel::primaryFolderPath");
    if (folderList.isEmpty()) return "";
    return folderList.at(0);
}

// END MULTI-SELECT FOLDERS

bool DataModel::contains(QString &path)
{
    if (G::isLogger) G::log("DataModel::contains");
    if (isDebug)
        qDebug() << "DataModel::contains" << "instance =" << instance << path;

    for (int row = 0; row < rowCount(); ++row) {
        if (index(row, 0).data(G::PathRole).toString().toLower() == path.toLower()) {
            // set to same case used by op system
            path = index(row, 0).data(G::PathRole).toString();
            return true;
        }
    }
    return false;
}

void DataModel::find(QString text)
{
    if (G::isLogger) G::log("DataModel::find");
    if (isDebug) qDebug() << "DataModel::find" << "instance =" << instance << text;

    QMutexLocker locker(&dmMutex);
    for (int row = 0; row < sf->rowCount(); ++row) {
        QString searchableText = sf->index(row, G::SearchTextColumn).data().toString();
        qDebug() << "DataModel::find" << searchableText;
        if (searchableText.contains(text.toLower())) {
            QModelIndex idx = sf->mapToSource(sf->index(row, G::SearchColumn));
            setData(idx, "true");
        }
    }
}

bool DataModel::endLoad(bool success)
{
    if (G::isLogger) G::log("DataModel::endLoad", "instance = " + QString::number(instance));
    if (isDebug)
        qDebug() << "DataModel::endLoad" << "instance =" << instance
                 << "success =" << success;

    // abort = false;
    loadingModel = false;
    /*  The streamed fill is over, so rowCount() is the count again -- whether it finished
        or was aborted part-way, since an aborted load's rows are what is actually there. */
    expectedRows = 0;
    sf->suspend(false);

    /*  Republish the worker view now the rows are FILLED.

        The proxy's rowsInserted fires from setRowCount, which the batched
        insert calls BEFORE it writes any cell -- so the snapshot built from
        that signal has the right number of rows and no paths in them. This is
        the point at which both are true. It is cheap and idempotent, so doing
        it here as well as on the signal costs a pass and removes the ordering
        trap. */
    resizeRowSync(rowCount());
    rebuildProxySnapshot();

    if (success) {
        resolveIconChunkSize();
        return true;
    }
    else {
        clear();
        // model emptied on the failure path: reset running load-flag counts
        metadataAttemptedCount.store(0, std::memory_order_relaxed);
        metadataLoadedCount.store(0, std::memory_order_relaxed);
        iconLoadedCount.store(0, std::memory_order_relaxed);
        videoRowCount.store(0, std::memory_order_relaxed);
    iconUnloadableCount.store(0, std::memory_order_relaxed);
        filters->loadingDataModelFailed();
        return false;
    }
}

bool DataModel::okManyImagesWarning()
{
    if (G::isLogger) G::log("DataModel::okManyImagesWarning");
    QString title = "Too Many Images";
    QString max = QString::number(G::maxIconChunk);
    QString folders;
    folderList.count() == 1 ? folders = "folder" : folders = "folders";
    QString msg =
        "There are more than " + max + " images in the " + folders + ".  If you choose " +
        "to continue you may experience sluggish responses or system hangs.\n\n" +
        "Do you wish to continue?"
        ;

    QMessageBox msgBox;
    msgBox.setWindowTitle(title);
    msgBox.setText(msg);
    msgBox.setStandardButtons(QMessageBox::No | QMessageBox::Yes);
    msgBox.setDefaultButton(QMessageBox::No);
    msgBox.setIcon(QMessageBox::Warning);
    msgBox.setStyleSheet(G::css);
    // prevent MacOS enforcing certain UI conventions
    msgBox.setWindowModality(Qt::WindowModal);
    int ret = msgBox.exec();
    // qDebug() << "ret =" << ret;
    if (ret == QMessageBox::Yes) return true;
    return false;
}

/*  Raw+JPG pair accessors. See the note in datamodel.h: every pairing role
    lives on column 0 of the datamodel, and G::DupOtherIdxRole holds the paired
    row as a plain int.

    It used to hold a QModelIndex. That is a persistent reference into a model
    that is cleared and rebuilt on every folder change, and it would not survive
    the row store becoming anything other than a QStandardItemModel. An int row
    can also be range-checked, which a stale QModelIndex cannot.
*/
int DataModel::dupOtherRow(int dmRow) const
{
    if (dmRow < 0 || dmRow >= rowCount()) return -1;
    const QVariant v = index(dmRow, G::PathColumn).data(G::DupOtherIdxRole);
    if (!v.isValid()) return -1;
    bool ok = false;
    const int other = v.toInt(&ok);
    if (!ok || other < 0 || other >= rowCount() || other == dmRow) return -1;
    return other;
}

bool DataModel::isDupJpg(int dmRow) const
{
    if (dmRow < 0 || dmRow >= rowCount()) return false;
    return index(dmRow, G::PathColumn).data(G::DupIsJpgRole).toBool();
}

bool DataModel::isDupHiddenRaw(int dmRow) const
{
    if (dmRow < 0 || dmRow >= rowCount()) return false;
    return index(dmRow, G::PathColumn).data(G::DupHideRawRole).toBool();
}

QString DataModel::dupRawType(int dmRow) const
{
    if (dmRow < 0 || dmRow >= rowCount()) return QString();
    return index(dmRow, G::PathColumn).data(G::DupRawTypeRole).toString();
}

void DataModel::rawJpgPairing(int row, const QString &ext, const QString &baseName)
{
    bool isRaw = metadata->hasJpg.contains(ext);
    bool isJpg = (ext == "jpg" || ext == "jpeg");

    // Only update the fast sequential trackers during the initial bulk folder load
    if (isRaw && loadingModel) {
        prevRawSuffix = ext;
        prevRawBaseName = baseName;
        prevRawRow = row;
    }

    QMutexLocker locker(&dmMutex);

    if (isJpg || isRaw) {

        // --- PATH A: BULK LOAD (Fast Sequential Check) ---
        if (loadingModel) {
            if (isJpg && prevRawRow >= 0 && baseName == prevRawBaseName) {
                QModelIndex prevRawIdx = index(prevRawRow, 0);
                setData(prevRawIdx, true, G::DupHideRawRole);
                setData(prevRawIdx, row, G::DupOtherIdxRole);
                setData(index(row, 0), prevRawRow, G::DupOtherIdxRole);
                setData(index(row, 0), true, G::DupIsJpgRole);

                setData(index(row, 0), prevRawSuffix.toUpper(), G::DupRawTypeRole);
                if (combineRawJpg)
                    setData(index(row, G::TypeColumn), "JPG+" + prevRawSuffix.toUpper());
                else
                    setData(index(row, G::TypeColumn), "JPG");
            }
        }

        // --- PATH B: ISOLATED INSERT AFTER LOAD (Targeted Search) ---
        else {
            QModelIndexList matches = match(index(0, G::NameColumn), Qt::DisplayRole, baseName, 2, Qt::MatchExactly);

            for (const QModelIndex &idx : matches) {
                if (idx.row() == row) continue;

                QString otherSuffix = index(idx.row(), G::TypeColumn).data().toString().toLower();
                QModelIndex otherIdx = index(idx.row(), 0);

                if (isJpg && metadata->hasJpg.contains(otherSuffix)) {
                    setData(otherIdx, true, G::DupHideRawRole);
                    setData(otherIdx, row, G::DupOtherIdxRole);
                    setData(index(row, 0), idx.row(), G::DupOtherIdxRole);
                    setData(index(row, 0), true, G::DupIsJpgRole);

                    setData(index(row, 0), otherSuffix.toUpper(), G::DupRawTypeRole);
                    if (combineRawJpg)
                        setData(index(row, G::TypeColumn), "JPG+" + otherSuffix.toUpper());
                    else
                        setData(index(row, G::TypeColumn), "JPG");
                    break;
                }
                else if (isRaw && (otherSuffix == "jpg" || otherSuffix == "jpeg")) {
                    setData(index(row, 0), true, G::DupHideRawRole);
                    setData(index(row, 0), idx.row(), G::DupOtherIdxRole);
                    setData(otherIdx, row, G::DupOtherIdxRole);
                    setData(otherIdx, true, G::DupIsJpgRole);

                    setData(otherIdx, ext.toUpper(), G::DupRawTypeRole);
                    /* The jpg half carries the combined type label, which must
                       go to that row's TypeColumn. This previously passed
                       G::TypeColumn as the ROLE argument of setData(idx, value,
                       role), so the label landed on column 0 under an arbitrary
                       role and the Type column never changed -- the isJpg
                       branch above always got this right. */
                    if (combineRawJpg)
                        setData(index(idx.row(), G::TypeColumn),
                                "JPG+" + ext.toUpper());
                    else
                        setData(index(idx.row(), G::TypeColumn), "JPG");
                    break;
                }
            }
        }
    }
}

void DataModel::addFileDataForRow(int row, QFileInfo fileInfo, const CatalogRow *cat)
{
/*
    Load the information from the operating system contained in QFileInfo

    • PathColumn
    • NameColumn
    • TypeColumn
    • PermissionsColumn
    • ReadWriteColumn
    • SizeColumn
    • CreatedColumn
    • ModifiedColumn

    Also add non-imagedata fields

    • PickColumn
    • IngestedColumn
    • VideoColumn
    • SidecarColumn
    • MetadataStatusColumn
    • IconLoadedColumn
    • SearchColumn
    • ErrColumn
*/
    if (G::isLogger) G::log("DataModel::addFileDataForRow", "row = " + QString::number(row));
    if (isDebug)
        qDebug() << "DataModel::addFileDataForRow"
                          << "instance =" << instance
                          << "row =" << row
                          << fileInfo.filePath()
                             ;

    // qApp->processEvents();   // process mouse events in fsTree and bookmarks

    // append hash index of datamodel row for fPath for fast lookups
    QString fPath = fileInfo.filePath();
    QString folderName = fileInfo.dir().dirName();
    QString ext = fileInfo.suffix().toLower();
    QString baseName = fileInfo.completeBaseName();
    QString sidecarPath = fileInfo.dir().path() + "/" + baseName + ".xmp";

    // build hash to quickly get dmRow from fPath (ie pixmap.cpp, imageCache...)
    if (fPathRow.contains(fPath)) return;
    fPathRowSet(fPath, row);

    // string to hold aggregated text for searching
    QString search = fPath;

    // Do not lock mutex here, as this is all new data
    // Block signals while stuffing data
    const QSignalBlocker b(this);

    setData(index(row, G::RowNumberColumn), row + 1);
    setData(index(row, G::PathColumn), fPath, G::PathRole);
    // Show tooltips for each item in datamodel views - this has been moved to
    // IconView::mouseMoveEvent so can show tooltips for icon symbols as well
    setData(index(row, G::PathColumn), QRect(), G::IconRectRole);
    setData(index(row, G::PathColumn), false, G::DupHideRawRole);
    setData(index(row, G::NameColumn), fileInfo.fileName());
    setData(index(row, G::FolderNameColumn), folderName);
    QString s = fileInfo.suffix().toUpper();
    setData(index(row, G::TypeColumn), s);
    setData(index(row, G::VideoColumn), metadata->videoFormats.contains(ext));
    /*  The index knows whether there is a sidecar -- it stamps one -- so an indexed row
        does not stat for it. sidecarMtime is 0 exactly when there was none. */
    setData(index(row, G::SidecarColumn),
            cat ? cat->sidecarMtime != 0 : QFile(sidecarPath).exists());
    /* G::DevelopColumn is NOT set here. Deciding whether an image has a develop recipe
       means parsing the sidecar, and this runs on the folder-load path -- the one place
       where per-image work has repeatedly cost visible lag. Metadata::parseSidecar is
       already parsing that same file on a worker thread, so the flag rides in on
       ImageMetadata::developEdited via addMetadataForItem instead. */
    /*  PERMISSIONS ARE THE ONE THING THE INDEX DOES NOT HOLD, and asking the filesystem
        for them is the stat this path exists to avoid. An indexed row is therefore
        assumed writable -- the ordinary case -- and corrected when the row is actually
        verified or written. Assuming the opposite would grey out ratings and renames
        across a whole catalog on the strength of a value nobody had looked up. */
    uint p = cat ? uint(QFileDevice::ReadUser | QFileDevice::WriteUser)
                 : static_cast<uint>(fileInfo.permissions());
    setData(index(row, G::PermissionsColumn), p);
    bool isReadWrite = (p & QFileDevice::ReadUser) && (p & QFileDevice::WriteUser);
    setData(index(row, G::ReadWriteColumn), isReadWrite);
    // size
    quint32 bytes = cat ? quint32(cat->srcSize) : quint32(fileInfo.size());
    setData(index(row, G::ByteSizeColumn), bytes);

    // estimate cacheSize until read metadata and calc size for QImage
    float mb = static_cast<double>(bytes) / (1 << 20);
    if (raw.contains(ext)) mb *= 3.5;
    if (ext == "jpg" || ext == "jpeg") mb *= 15;
    setData(index(row, G::CacheSizeColumn), mb);

    setData(index(row, G::CompareColumn), false);
    /*  Created is the CAPTURE date for an indexed row, which is what the column ends up
        holding anyway once metadata arrives -- birthTime is the placeholder the file read
        uses until then, and it needs a stat. Modified comes from the same srcMtime the
        freshness stamp is made of. */
    s = cat ? cat->captured.toString("yyyy-MM-dd hh:mm:ss.zzz")
            : fileInfo.birthTime().toString("yyyy-MM-dd hh:mm:ss.zzz");
    setData(index(row, G::CreatedColumn), s);
    s = cat ? QDateTime::fromSecsSinceEpoch(cat->srcMtime)
                  .toString("yyyy-MM-dd hh:mm:ss")
            : fileInfo.lastModified().toString("yyyy-MM-dd hh:mm:ss");
    search += s;
    setData(index(row, G::ModifiedColumn), s);
    setData(index(row, G::PickColumn), "Unpicked");
    /*  bool, not the QString "false" this used to be. Ingested had three writers
        disagreeing about the type -- "false" here, true (bool) from the ingest
        pass, "true" (QString) from MW::setIngested -- so the column's type
        depended on which path filled the row last. Settled the way Width and
        Height were. */
    setData(index(row, G::IngestedColumn), false);
    setData(index(row, G::MetadataReadingColumn), false);
    setData(index(row, G::MetadataStatusColumn), G::MetaNotAttempted);
    setData(index(row, G::IconLoadedColumn), false);
    /*  bool, for the same reason: "false" here, m.isSearch and
        SearchTerms::matches() (bool) once metadata arrives. See
        Filters::searchTrue, whose filter value was settled with it. */
    setData(index(row, G::SearchColumn), false);
    setData(index(row, G::SearchTextColumn), search);

    rawJpgPairing(row, ext, baseName);

    // emit one compact notification (only if you need the view to refresh now)
    emit dataChanged(index(row, 0), index(row, columnCount()-1));
}

bool DataModel::updateFileData(QFileInfo fileInfo)
{
    if (G::isLogger) G::log("DataModel::updateFileData");
    QString fPath = fileInfo.filePath();
    // qDebug() << "DataModel::updateFileData" << "Instance =" << instance << fPath;
    // if (!fPathRow.contains(fPath)) return false;
    if (!fPathRowContains(fPath)) return false;
    // int row = fPathRow[fPath];
    int row = fPathRowValue(fPath);
    if (!index(row,0).isValid()) return false;

    QMutexLocker locker(&dmMutex);
    setData(index(row, G::ByteSizeColumn), fileInfo.size());
    QString s = fileInfo.lastModified().toString("yyyy-MM-dd hh:mm:ss");
    setData(index(row, G::ModifiedColumn), s);

    // created date
    s = fileInfo.birthTime().toString("yyyy-MM-dd hh:mm:ss.zzz");
    setData(index(row, G::CreatedColumn), s);
    qDebug() << "DataModel::updateFileData" << s;

    return true;
}

bool DataModel::catalogRowFor(int row, CatalogRow &r) const
{
/*
    One row, as plain values for the catalog. Returns false for a row that must
    not be catalogued.

    EXTRACTED FROM catalogRows SO THERE IS ONE BUILDER, not two. The bulk capture
    after a folder load and the write-back after an edit have to produce the same
    row for the same image -- if they drifted, an edited image would be indexed
    differently from an unedited one and only in the fields that differed, which
    is close to undebuggable.
*/
    /*  ROWS THAT ARE NOT MetaLoaded ARE SKIPPED. A row still being read has empty
        keywords and no title, and cataloguing that is worse than cataloguing
        nothing: the freshness stamp would match on the next visit, so the blank
        entry would be treated as current and the image would stay wrong in
        search results until its file changed on disk. */
    if (index(row, G::MetadataStatusColumn).data().toInt() != G::MetaLoaded) return false;

    const QString fPath = index(row, G::PathColumn).data(G::PathRole).toString();
    if (fPath.isEmpty()) return false;
    /* Videos have no keywords or camera metadata worth searching, and the catalog is
       a photo index. */
    if (index(row, G::VideoColumn).data().toBool()) return false;

    r = CatalogRow();
    r.path = fPath;
    const QFileInfo fi(fPath);
    r.folder = fi.absoluteDir().path();
    r.filename = fi.fileName();
    r.ext = fi.suffix().toLower();
    r.srcSize = index(row, G::ByteSizeColumn).data().toLongLong();
    r.srcMtime = index(row, G::ModifiedColumn).data().toDateTime().toSecsSinceEpoch();

    /* The sidecar's timestamp is what makes a keyword edited in Lightroom -- which
       rewrites the .xmp and never touches the raw -- reindex the next time this
       folder is opened. SidecarColumn was set during the file scan, so the stat below
       is paid only by images that actually have one. */
    if (index(row, G::SidecarColumn).data().toBool()) {
        const QFileInfo si(metadata->sidecarPath(fPath));
        if (si.exists()) r.sidecarMtime = si.lastModified().toSecsSinceEpoch();
    }

    r.captured = index(row, G::CreatedColumn).data().toDateTime();
    r.rating = index(row, G::RatingColumn).data().toString().toInt();
    r.label = index(row, G::LabelColumn).data().toString();
    r.pick = index(row, G::PickColumn).data().toString() == "Picked";
    r.title = index(row, G::TitleColumn).data().toString();
    r.creator = index(row, G::CreatorColumn).data().toString();
    r.copyright = index(row, G::CopyrightColumn).data().toString();
    r.make = index(row, G::CameraMakeColumn).data().toString();
    r.model = index(row, G::CameraModelColumn).data().toString();
    r.lens = index(row, G::LensColumn).data().toString();
    r.iso = index(row, G::ISOColumn).data().toInt();
    r.aperture = index(row, G::ApertureColumn).data().toDouble();
    r.shutter = index(row, G::ShutterspeedColumn).data().toDouble();
    r.focalLength = index(row, G::FocalLengthColumn).data().toDouble();
    r.width = index(row, G::WidthColumn).data().toInt();
    r.height = index(row, G::HeightColumn).data().toInt();
    r.gpsCoord = index(row, G::GPSCoordColumn).data().toString();

    /*  schema 6/7: what the ROW displays, beyond what a search needs. */
    r.orientation = index(row, G::OrientationColumn).data().toInt();
    r.exposureComp = index(row, G::ExposureCompensationColumn).data().toString();
    r.focusX = index(row, G::FocusXColumn).data().toDouble();
    r.focusY = index(row, G::FocusYColumn).data().toDouble();
    r.email = index(row, G::EmailColumn).data().toString();
    r.url = index(row, G::UrlColumn).data().toString();
    r._rating = index(row, G::_RatingColumn).data().toString();
    r._label = index(row, G::_LabelColumn).data().toString();
    r._creator = index(row, G::_CreatorColumn).data().toString();
    r._title = index(row, G::_TitleColumn).data().toString();
    r._copyright = index(row, G::_CopyrightColumn).data().toString();
    r._email = index(row, G::_EmailColumn).data().toString();
    r._url = index(row, G::_UrlColumn).data().toString();
    r.developed = index(row, G::DevelopColumn).data().toBool();
    r.devPreviewKey = index(row, G::DevPreviewKeyColumn).data().toString();
    r.shootingInfo = index(row, G::ShootingInfoColumn).data().toString();
    /*  The LITERAL dc:subject list, kept apart from the flat vocabulary below --
        only this one may ever be written back to a file. */
    r.keywordsLiteral = index(row, G::KeywordsColumn).data().toStringList();

    /* The FLAT vocabulary, not the literal dc:subject: the catalog indexes what is
       searched on, and flattening once here means the index and the Filters category
       cannot disagree about the same image. The raw paths travel beside it because
       the catalog derives its ambiguity contexts (which parents a name has been seen
       under) from them. */
    r.keywords = index(row, G::KeywordsAllColumn).data().toStringList();
    r.keywordPaths = index(row, G::KeywordPathsColumn).data().toStringList();
    return true;
}

QHash<QString, QSet<QString>> DataModel::folderPathSets() const
{
/*
    Every row's path, grouped by its parent folder.

    EVERY ROW, not only the read ones -- unlike catalogRows(), which skips anything not
    yet G::MetaLoaded. The question here is "was this file there", and a row exists
    because the enumeration found the file; whether its metadata was read since is a
    different fact. Filtering by read status would make the caller demote catalogued rows
    for files that are sitting right there.
*/
    QHash<QString, QSet<QString>> out;
    const int n = rowCount();
    for (int row = 0; row < n; ++row) {
        const QString fPath = index(row, 0).data(G::PathRole).toString();
        if (fPath.isEmpty()) continue;
        out[QFileInfo(fPath).absoluteDir().path()].insert(fPath);
    }

    /*  A FOLDER THAT ENUMERATED TO NOTHING IS STILL AN ANSWER -- and without this it
        would be the one case the reconcile misses: delete every image in a catalogued
        folder and no row of it survives to name it, so it would never be compared and
        its rows would stay live forever.

        THE DIRECTORY IS STAT'D FIRST, and only for these. "No rows" has a second cause
        that looks identical from here: the enumeration could not READ the folder -- gone,
        unmounted, permissions -- and folderList is appended before the listing is
        attempted, so an unreadable folder reaches here looking exactly like an empty one.
        Demoting on that would throw away a whole folder's keywords because a drive was
        briefly absent. One stat per empty folder settles it; folders that produced rows
        need none, since a row IS the evidence the listing worked. */
    for (const QString &folder : folderList) {
        if (out.contains(folder)) continue;
        const QFileInfo di(folder);
        if (!di.exists() || !di.isDir() || !di.isReadable()) continue;
        out.insert(folder, QSet<QString>());
    }

    return out;
}

QVector<CatalogRow> DataModel::catalogRows() const
{
/*
    Every fully-read row of this model, as plain values for the catalog.

    ONE PASS ON THE GUI THREAD, then the result travels to a pool thread to be
    inserted. The model is not thread-safe and the folder the user is looking at
    is live, so the copy is the price of not doing SQL on the GUI thread. At a
    few hundred bytes a row it is cheap next to the metadata read that just
    finished. The per-row work is catalogRowFor, shared with the edit write-back.
*/
    if (G::isLogger) G::log("DataModel::catalogRows");

    QVector<CatalogRow> rows;
    const int n = rowCount();
    rows.reserve(n);
    for (int row = 0; row < n; ++row) {
        CatalogRow r;
        if (catalogRowFor(row, r)) rows.append(r);
    }
    return rows;
}

ImageMetadata DataModel::imMetadata(QString fPath, bool updateInMetadata)
{
/*
    Returns the struct ImageMetadata containing almost all the metadata available for the
    fPath image for convenient access.  ie rating = m.rating

    If updateInMetadata == true then metadata->m is updated for the fPath.  This is used in
    metadata->writeXMP, since Metadata does not have direct assess to the DataModel dm.
    updateInMetadata == false by default.

    Used by ImageDecoder, InfoString, IngestDlg and XMP sidecars.
*/
    if (G::isLogger) G::log("DataModel::imMetadata", fPath);
    // qDebug() << "DataModel::imMetadata" << "Instance =" << instance << currentFolderPath;

    ImageMetadata m;
    if (fPath == "") return m;

    int sfRow = proxyRowFromPath(fPath, "DataModel::imMetadata");
    // int row = fPathRow[fPath];
    int row = fPathRowValue(fPath);
    if (!index(row,0).isValid()) return m;

    if (isDebug) qDebug() << "DataModel::imMetadata" << "instance =" << instance
                          << "row =" << row
                          << pathFromProxyRow(sfRow);

    // QMutexLocker hangs if tiff file in the first folder selected after start program
    // static int counter = 0;
    // if (counter) QMutexLocker locker(&mutex);
    // counter++;

    // if (mutex.tryLock()) mutex.unlock();
    // QMutexLocker locker(&mutex);

    metadata->m.row = row;  // req'd?

    // file info (calling Metadata not required)
    m.row = sfRow;
    m.fPath = fPath;
    m.fName = index(row, G::NameColumn).data().toString();
    m.type = index(row, G::TypeColumn).data().toString();
    /* ImageDecoder::load() selects the in-house RAW decoder off m.ext in independent
       mode; keep it populated (from the same TypeColumn cache mode uses) so
       decodeIndependent takes the raw path instead of falling back to the display-
       referred embedded JPG. */
    m.ext = m.type;
    m.size = index(row, G::ByteSizeColumn).data().toInt();
    m.video = index(row, G::VideoColumn).data().toInt();
    m.sidecar = index(row, G::SidecarColumn).data().toInt();
    m.label = index(row, G::LabelColumn).data().toString();
    m._label = index(row, G::_LabelColumn).data().toString();
    m.rating = index(row, G::RatingColumn).data().toString();
    m._rating = index(row, G::_RatingColumn).data().toString();
//    m._orientation = index(row, G::_OrientationColumn).data().toInt();
//    m._rotationDegrees = index(row, G::_RotationColumn).data().toInt();
    m.createdDate = index(row, G::CreatedColumn).data().toDateTime();
    m.modifiedDate = index(row, G::ModifiedColumn).data().toDateTime();
    m.year = index(row, G::YearColumn).data().toString();
    m.day = index(row, G::DayColumn).data().toString();

    m.pick  = index(row, G::PickColumn).data().toBool();
    m.ingested  = index(row, G::IngestedColumn).data().toBool();
    m.metadataReading = index(row, G::MetadataReadingColumn).data().toBool();
    m.metaStatus = index(row, G::MetadataStatusColumn).data().toInt();
    m.permissions = index(row, G::PermissionsColumn).data().toUInt();
    m.isReadWrite = index(row, G::ReadWriteColumn).data().toBool();

    m.width = index(row, G::WidthColumn).data().toInt();
    m.height = index(row, G::HeightColumn).data().toInt();
    /* Raw sensor unpack info (raw files only) -- stored at metadata-read time so the RAW
       decode path can use it instead of re-walking the file. */
    fPathRawInfoGet(fPath, m.rawInfo);
    m.widthOrigPreview = index(row, G::WidthOrigPreviewColumn).data().toInt();
    m.heightOrigPreview = index(row, G::HeightOrigPreviewColumn).data().toInt();
    m.dimensions = index(row, G::DimensionsColumn).data().toString();
    m.megapixels = index(row, G::MegaPixelsColumn).data().toFloat();
    m.loadMsecPerMp = index(row, G::LoadMsecPerMpColumn).data().toInt();
    m.aspectRatio = index(row, G::AspectRatioColumn).data().toFloat();
    // m.iconAspectRatio = index(row, G::IconAspectRatioColumn).data().toFloat();
    m.orientation = index(row, G::OrientationColumn).data().toInt();
    m.orientationOffset = index(row, G::OrientationOffsetColumn).data().toUInt();
    m.rotationDegrees = index(row, G::RotationColumn).data().toInt();

    m.make = index(row, G::CameraMakeColumn).data().toString();
    m.model = index(row, G::CameraModelColumn).data().toString();
    m.exposureTimeNum = index(row, G::ShutterspeedColumn).data().toDouble();
    /* A zero shutter speed is the ORDINARY case for a row whose metadata has not been
       read yet, and for a format that carries none. Reciprocating it gave +Inf, and
       qRound(+Inf) is a Q_ASSERT in a debug build -- so the app aborted the moment the
       shooting-info string was built for such a row (the --selftest smoke run hit it
       every time). Guarded rather than reciprocated: with no exposure time there is
       nothing to display. */
    if (m.exposureTimeNum <= 0.0) {
        m.exposureTime.clear();
    }
    else if (m.exposureTimeNum < 1.0) {
        double recip = 1 / m.exposureTimeNum;
        if (recip >= 2) m.exposureTime = "1/" + QString::number(qRound(recip));
        else m.exposureTime = QString::number(m.exposureTimeNum, 'g', 2);
        m.exposureTime += " sec";
    }
    else {
        m.exposureTime = QString::number(m.exposureTimeNum);
        m.exposureTime += " sec";
    }
    m.apertureNum = index(row, G::ApertureColumn).data().toDouble();
    m.aperture = "f/" + QString::number(m.apertureNum, 'f', 1);
    m.ISONum = index(row, G::ISOColumn).data().toInt();
    m.ISO = QString::number(m.ISONum);
    m.exposureCompensationNum = index(row, G::ExposureCompensationColumn).data().toDouble();
    m.exposureCompensation = QString::number(m.exposureCompensationNum, 'f', 1);
    m.lens = index(row, G::LensColumn).data().toString();
    m.focalLengthNum = index(row, G::FocalLengthColumn).data().toInt();
    m.focalLength = QString::number(m.focalLengthNum, 'f', 0) + "mm";
    m.focusX = index(row, G::FocusXColumn).data().toFloat();
    m.focusY = index(row, G::FocusYColumn).data().toFloat();
    m.gpsCoord = index(row, G::GPSCoordColumn).data().toString();
    m.keywords = index(row, G::KeywordsColumn).data().toStringList();
    m.keywordPaths = index(row, G::KeywordPathsColumn).data().toStringList();
    m.shootingInfo = index(row, G::ShootingInfoColumn).data().toString();
    m.duration = index(row, G::DurationColumn).data().toString();

    m.title = index(row, G::TitleColumn).data().toString();
    m._title = index(row, G::_TitleColumn).data().toString();
    m.creator = index(row, G::CreatorColumn).data().toString();
    m._creator = index(row, G::_CreatorColumn).data().toString();
    m.copyright = index(row, G::CopyrightColumn).data().toString();
    m._copyright = index(row, G::_CopyrightColumn).data().toString();
    m.email = index(row, G::EmailColumn).data().toString();
    m._email = index(row, G::_EmailColumn).data().toString();
    m.url = index(row, G::UrlColumn).data().toString();
    m._url = index(row, G::_UrlColumn).data().toString();

    m.offsetFull = index(row, G::OffsetFullColumn).data().toUInt();
    m.lengthFull = index(row, G::LengthFullColumn).data().toUInt();
    m.offsetThumb = index(row, G::OffsetThumbColumn).data().toUInt();
    m.lengthThumb = index(row, G::LengthThumbColumn).data().toUInt();
    m.samplesPerPixel = index(row, G::samplesPerPixelColumn).data().toInt();
    m.isBigEnd = index(row, G::isBigEndianColumn).data().toBool();
    m.ifd0Offset = index(row, G::ifd0OffsetColumn).data().toUInt();
    m.ifdOffsets = index(row, G::ifdOffsetsColumn).data().toList();
    m.xmpSegmentOffset = index(row, G::XmpSegmentOffsetColumn).data().toUInt();
    m.xmpSegmentLength = index(row, G::XmpSegmentLengthColumn).data().toUInt();
    m.isXmp = index(row, G::IsXMPColumn).data().toBool();
    m.iccSegmentOffset = index(row, G::ICCSegmentOffsetColumn).data().toUInt();
    m.iccSegmentLength = index(row, G::ICCSegmentLengthColumn).data().toUInt();
    m.iccBuf = index(row, G::ICCBufColumn).data().toByteArray();
    /* ICCSpaceColumn, not ICCSegmentOffsetColumn -- reading the offset here returned the
       segment's file position as the colour space NAME. Silent: iccSpace is only ever
       displayed or logged, so it read as a stray number rather than as a failure. */
    m.iccSpace = index(row, G::ICCSpaceColumn).data().toString();

    m.searchStr = index(row, G::SearchTextColumn).data().toString();
    m.compare = index(row, G::CompareColumn).data().toBool();

    if (updateInMetadata) metadata->m = m;

    return m;
}

void DataModel::addAllMetadata()
{
/*
    This function is intended to load metadata (but not the icon) quickly for the entire
    datamodel. This information is required for a filter or sort operation, which requires
    the entire dataset. Since the program will be waiting for the update this does not need
    to run as a separate thread and can be executed directly.
*/
    if (G::isLogger || G::isFlowLogger) qDebug() << "DataModel::addAllMetadata";
    if (isDebug) {
        qDebug() << "DataModel::addAllMetadata" << "instance =" << instance;
    }
//    G::t.restart();

    /*  THE INDEX FIRST, IN ONE PASS, when it can answer (G::useIndexMetadata).

        This function is the BLOCKING path -- a filter or sort cannot start until every
        row has been read, so the GUI waits here -- and until now it opened every
        unread file regardless of what the catalog already knew, while Reader (the
        non-blocking path) had been asking the index per row since schema 6. The one
        place where the user actually waits was the one place that did not use it.

        BULK, NOT PER ROW. Catalog::fetchFresh takes a list, so a folder of thousands is
        a page of indexed seeks rather than a query per file, and every file it answers
        for is never opened at all. Paged so the progress message still moves and the
        catalog mutex is released between pages -- a search box frozen for the duration
        would be the obvious cost of doing it in one gulp.

        The mapping lives in Metadata/indexmetadata.h, shared with Reader, so the two
        paths cannot drift apart in what a row ends up holding. */
    QHash<QString, CatalogRow> fresh;
    if (G::useIndexMetadata) {
        constexpr int kFreshPage = 1000;
        QList<CatalogRow> cands;
        cands.reserve(qMin(rowCount(), kFreshPage));
        for (int row = 0; row < rowCount(); ++row) {
            if (abort || G::stop) break;
            if (index(row, G::MetadataStatusColumn).data().toInt() != G::MetaNotAttempted)
                continue;
            const QString fPath = index(row, 0).data(G::PathRole).toString();
            if (fPath.isEmpty()) continue;
            cands.append(IndexMetadata::candidate(QFileInfo(fPath), metadata));
            if (cands.size() >= kFreshPage) {
                fresh.insert(Catalog::instance().fetchFresh(cands));
                cands.clear();
            }
        }
        if (!cands.isEmpty()) fresh.insert(Catalog::instance().fetchFresh(cands));
    }

    int mod = 10;
    if (rowCount() > 1000) mod = 100;
    int count = 0;
    int fromIndexCount = 0;
    for (int row = 0; row < rowCount(); ++row) {
        // Load folder progress
        if (/*G::isLinearLoading && */row % mod == 0) {
            QString s = QString::number(row) + " of " + QString::number(rowCount()) +
                        " metadata loading...";
            emit centralMsg(s);    // rghmsg
            emit updateProgress(1.0 * row / rowCount() * 100);
            if (G::useProcessEvents) qApp->processEvents(QEventLoop::ExcludeUserInputEvents | QEventLoop::ExcludeSocketNotifiers);
        }
        if (abort || G::stop) {
            endLoad(false);
            setAllMetadataAttempted(false);
            return;
        }
        // is metadata already cached (or attempted?)
        if (index(row, G::MetadataStatusColumn).data().toInt() != G::MetaNotAttempted) continue;

        QString fPath = index(row, 0).data(G::PathRole).toString();
        QFileInfo fileInfo(fPath);
        QString ext = fileInfo.suffix().toLower();

        /*  Answered by the index above -- no file is opened for this row. */
        const auto fit = fresh.constFind(fPath);
        if (fit != fresh.cend()) {
            IndexMetadata::fill(metadata->m, *fit, fileInfo, row, instance);
            addMetadataForItem(metadata->m, "DataModel::addAllMetadata");
            count++;
            fromIndexCount++;
            continue;
        }

        if (metadata->loadImageMetadata(fileInfo, row, instance, true, true, false, true, "DataModel::addAllMetadata")) {
            metadata->m.row = row;
            metadata->m.instance = instance;
            addMetadataForItem(metadata->m, "DataModel::addAllMetadata");
            count++;
        }
        else {
            if (metadata->hasMetadataFormats.contains(ext)) {
                errMsg = "Failed to load metadata.";
                G::issue("Warning", errMsg, "DataModel::addAllMetadata", row, fPath);
            }
        }
    }
    if (G::isPerfProbe) {
        qDebug().noquote()
            << "[PERF] addAllMetadata"
            << " rows="      << rowCount()
            << " read="      << count
            << " fromIndex=" << fromIndexCount;
    }

    setAllMetadataAttempted(true);
    emit centralMsg("Metadata loaded");
    if (G::useProcessEvents) qApp->processEvents(QEventLoop::ExcludeUserInputEvents | QEventLoop::ExcludeSocketNotifiers);
    endLoad(true);
    /*
    qint64 ms = G::t.elapsed();
    qreal msperfile = static_cast<double>(ms) / count;
    qDebug() << "DataModel::addAllMetadata for" << count << "files"
             << ms << "ms" << msperfile << "ms per file;"
             << currentFolderPath;
//    */
}

bool DataModel::readMetadataForItem(int row, int instance)
{
/*
    Reads the image metadata into the datamodel for the row.
*/
    QString fun = "DataModel::readMetadataForItem";
    QString errMsg = "";
    if (G::isLogger) G::log(fun, index(row, 0).data(G::PathRole).toString());
    if (isDebug) {
        qDebug() << fun << "instance =" << instance
                 << "row =" << row
                 << pathFromProxyRow(row);
    }

    // might be called from previous folder during folder change
    if (instance != this->instance) {
        errMsg = "Instance clash.";
        G::issueDedup("Comment", errMsg, fun, row);
        return true;
    }
    if (G::stop) return false;

    QString fPath = index(row, 0).data(G::PathRole).toString();

    // load metadata
    /*
     qDebug() << "DataModel::readMetadataForItem"
              << "metaStatus ="
              << index(row, G::MetadataStatusColumn).data().toInt()
              << fPath;//*/

    if (index(row, G::MetadataStatusColumn).data().toInt() == G::MetaNotAttempted) {
        QFileInfo fileInfo(fPath);

        // only read metadata from files that we understand
        QString ext = fileInfo.suffix().toLower();
        if (metadata->hasMetadataFormats.contains(ext)) {
            if (metadata->loadImageMetadata(fileInfo, row, instance, true, true, false, true, "DataModel::readMetadataForItem")) {
                // loadImageMetadata sets m.instance
                addMetadataForItem(metadata->m, "DataModel::readMetadataForItem");
            }
            else {
                errMsg = "Failed to load metadata.";
                G::issue("Error", errMsg, fun, row, fPath);
                return false;
            }
        }
        // cannot read this file type, load empty metadata
        else {
            errMsg = "Cannot read matadata for this file type.";
            G::issue("Warning", errMsg, fun, row, fPath);
            metadata->clearMetadata();
            metadata->m.row = row;
            metadata->m.instance = instance;
            metadata->m.compare = false;
            addMetadataForItem(metadata->m, "DataModel::readMetadataForItem");
            return false;
        }
    }
    return true;
}

bool DataModel::refreshMetadataForItem(int sfRow, int instance)
{
/*
    Reads the image metadata into the datamodel for the proxy row.
*/
    QString fun = "DataModel::refreshMetadataForItem";
    if (G::isLogger) G::log(fun, sf->index(sfRow, 0).data(G::PathRole).toString());
    if (isDebug) qDebug() << fun << "instance =" << instance
                          << "row =" << sfRow
                          << pathFromProxyRow(sfRow);

    // might be called from previous folder during folder change
    if (instance != this->instance) {
        errMsg = "Instance clash.";
        G::issueDedup("Comment", errMsg, fun, sfRow);
        return true;
    }
    if (G::stop) return false;

    QString fPath = sf->index(sfRow, 0).data(G::PathRole).toString();

    // load metadata
    /*
     qDebug() << "DataModel::refreshMetadataForItem"
              << "metaStatus ="
              << sf->index(row, G::MetadataStatusColumn).data().toInt()
              << fPath; //*/

    QFileInfo fileInfo(fPath);

    // only read metadata from files that we know how to
    QString ext = fileInfo.suffix().toLower();
    if (metadata->hasMetadataFormats.contains(ext)) {
        //qDebug() << "DataModel::readMetadataForItem" << fPath;
        if (metadata->loadImageMetadata(fileInfo, sfRow, instance, true, true, false, true, "DataModel::readMetadataForItem")) {
            addMetadataForItem(metadata->m, "DataModel::readMetadataForItem");
        }
        else {
            errMsg = "Failed to load metadata.";
            G::issue("Warning", errMsg, fun, sfRow, fPath);
            dmMutex.unlock();
            return false;
        }
    }
    // cannot read this file type, load empty metadata
    else {
        errMsg = "Cannot read metadata for this file type.";
        G::issue("Warning", errMsg, fun, sfRow, fPath);
        metadata->clearMetadata();
        metadata->m.row = sfRow;
        addMetadataForItem(metadata->m, "DataModel::readMetadataForItem");
        return false;
    }
    return true;
}

void DataModel::imageCacheWaiting(int sfRow, int instance)
{
    // qDebug() << "DataModel::imageCacheWaiting" << "row =" << sfRow;
    if (instance != this->instance) {
        return;
    }

    int dmRow = sf->mapToSource(sf->index(sfRow, 0)).row();
    imageCacheWaitingForRow = dmRow;
    if (metadataLoaded(dmRow)) {
        // qDebug() << "DataModel::imageCacheWaiting" << "row =" << sfRow << "emit rowLoaded()";
        imageCacheWaitingForRow = -1;
        emit rowLoaded();
    }
}

bool DataModel::addMetadataForItemFromReader(ImageMetadata m, QString src)
{
/*
    THE READER'S ENTRY POINT, and the only one that owes the backpressure counter a
    decrement.

    The counter lived inside addMetadataForItem as an RAII guard "matching Reader's
    fetch_add" -- but a dozen callers reach that function without going anywhere near a
    Reader (the catalog fill, addAllMetadata, readMetadataForItem, InfoView, EmbelExport,
    the file operations), and every one of them decremented a counter nothing had
    incremented. A catalog scope calls it once per row on the fill, so the counter ended a
    42,956-row load at -42,956: the probe line reads "readerQueue = -42956".

    That is not cosmetic. queuedReaderEvents is what MetaRead::dispatch consults before
    dispatching more readers (kQueueCap), so a permanently negative counter means the
    backpressure valve is wedged OPEN for the rest of the session -- on precisely the load
    where the GUI thread is most likely to fall behind. MW::allReaderEventsDrained reads
    the same counter to decide the model is fully updated.

    So the pairing now lives at the one place it is true: Reader connects here, this
    decrements, and addMetadataForItem is left as the plain model write every other caller
    wants.
*/
    struct QrEvGuard {
        std::atomic<int> &c;
        ~QrEvGuard() { c.fetch_sub(1, std::memory_order_relaxed); }
    } qrEvGuard{queuedReaderEvents};

    return addMetadataForItem(m, src);
}

bool DataModel::addMetadataForItem(ImageMetadata m, QString src)
{
/*
    This function is called after the metadata for each eligible image in the
    selected folder(s) has been cached or when addAllMetadata is called prior of
    filtering or sorting. The metadata is displayed in tableView and InfoView.

    If a folder is opened with combineRawJpg all the metadata for the raw file may
    not have been loaded, but editable data, (such as rating, label, title, email,
    url) may have been edited in the jpg file of the raw+jpg pair. If so, we do not
    want to overwrite this data.

    dm->queuedReaderEvents is an std::atomic<int> on DataModel that counts Reader-thread
    events emitted to the GUI but not yet drained. A producer/consumer imbalance — e.g.
    recursing an Apple .photoslibrary where Readers race through tiny JPG derivatives
    faster than the GUI can drain its event queue — would otherwise balloon Qt's queue
    and memory. The cap (4× readers) is loose enough that steady-state throughput is
    unaffected; it only engages when the GUI is genuinely falling behind.
*/
    if (G::isLogger) {
        QString msg = "row = " + QString::number(m.row);
        G::log("DataModel::addMetadataForItem", msg);
    }

    if (G::stop) return false;

    /* Memory-overrun fast-path: if the watchdog (or any other subsystem)
       has latched G::memoryOverrunFlag, abandon this insert. The GUI
       thread can be buried processing thousands of queued addMetadata
       events, during which the 250 ms watchdog QTimer may not get a
       chance to fire — checking the latch here lets us bail per row.

       In addition, every Nth call we probe the footprint directly so the
       latch can be tripped from this hot path without depending on the
       watchdog ever scheduling. */
    if (G::memoryOverrunFlag.load(std::memory_order_relaxed)) return false;
    {
        static thread_local int probeTick = 0;
        if ((++probeTick & 0x7F) == 0) {        // every 128 calls
            const quint64 cap = G::memoryAbortMB;
            if (cap) {
                const quint64 footprintMB = G::processFootprintMB();
                if (footprintMB && footprintMB >= cap) {
                    abort = true;
                    bool expected = false;
                    if (G::memoryOverrunFlag.compare_exchange_strong(
                            expected, true,
                            std::memory_order_acq_rel,
                            std::memory_order_relaxed)) {
                        emit memoryOverrun(footprintMB, cap);
                    }
                    return false;
                }
            }
        }
    }

    if (isDebug)
        qDebug() << "DataModel::addMetadataForItem" << "instance =" << instance
                          << "row =" << m.row
                          << pathFromProxyRow(m.row);

    // deal with lagging signals when new folder selected suddenly
    if (instance > -1 && m.instance != instance) {
        if (G::showIssueInConsole)
        errMsg = "Instance clash from " + src;
        G::issueDedup("Comment", errMsg, "DataModel::addMetadataForItem", m.row);
        return false;
    }

    int row = m.row;
    if (rowCount() <= row) return false;

    if (!metadata->ratings.contains(m.rating)) {
//        m.rating = "";
//        m._rating = "";
    }
    if (!metadata->labels.contains(m.label)) {
//        m.label = "";
//        m._label = "";
    }
    if (!index(row, 0).isValid()) {
        return false;
    }

    /* Stash raw sensor unpack info (raw files only) so the RAW decode path can read it without
       re-walking the file. Keyed by the row's fPath to match DataModel::imMetadata's lookup.
       Gated on G::useRaw so the preview-only path (useRaw off) pays nothing here -- a single
       chokepoint covering every vendor parser. When useRaw is off the parsers also skip filling
       rawInfo; the decoder's self-walk covers a later toggle-on. */
    if (G::useRaw && m.rawInfo.isRaw) {
        QString rawPath = index(row, G::PathColumn).data(G::PathRole).toString();
        if (!rawPath.isEmpty()) fPathRawInfoSet(rawPath, m.rawInfo);
    }

    QString search = index(row, G::SearchTextColumn).data().toString();

    QMutexLocker locker(&dmMutex);
    mLock = true;
    // block signals while stuffing cells
    const QSignalBlocker b(this);

    setData(index(row, G::SearchColumn), m.isSearch);
    setData(index(row, G::LabelColumn), m.label);
    search += m.label;
    setData(index(row, G::_LabelColumn), m._label);
    // if (m.rating == "") m.rating = "No Rating";
    // if (m.rating == "0") m.rating = "No Rating";
    if (m.rating == "0") m.rating = "";
    setData(index(row, G::RatingColumn), m.rating);
    /* Develop badge. Comes from Metadata::parseSidecar, which already had the sidecar
       open, so no extra I/O lands on the folder-load path. */
    setData(index(row, G::DevelopColumn), m.developEdited);
    setData(index(row, G::DevPreviewKeyColumn), m.devPreviewKey);
    // if (m._rating == "") m.rating = "No Rating";
    // if (m._rating == "0") m.rating = "No Rating";
    if (m._rating == "0") m.rating = "";
    setData(index(row, G::_RatingColumn), m._rating);

    // Creation dates
    // resolve system vs exif creation dates
    QString sysCreatedDT = index(row, G::CreatedColumn).data().toString();
    QString exifCreatedDT = m.createdDate.toString("yyyy-MM-dd hh:mm:ss.zzz");
    QDateTime createdDT;
    if (m.createdDate.isValid() /*&& exifCreatedDT != sysCreatedDT*/) {
        setData(index(row, G::CreatedColumn), exifCreatedDT);
        createdDT = m.createdDate;
    }
    else {
        createdDT = index(row, G::CreatedColumn).data().toDateTime();
    }
    if (createdDT.isValid()) {
        setData(index(row, G::YearColumn), createdDT.toString("yyyy"));
        /*  NOT createdDT.toString("MMM"), which follows the SYSTEM LOCALE: the value
            would then be the user's language rather than a fact about the image, it
            would change when they changed their locale, and it could not match the
            English CASE Catalog::categorySql spells out -- so the same month would be
            two different filter items in the two scopes. Catalog::monthLabel is the
            one spelling both sides read. */
        setData(index(row, G::MonthColumn), Catalog::monthLabel(createdDT.date().month()));
        setData(index(row, G::DayColumn), createdDT.toString("yyyy-MM-dd"));
    }

    /*  INT, not QString::number. The other writer of these two columns --
        Thumb::setImageDimensions via setValDm -- has always sent an int, so the
        column's type depended on which path wrote the row last. That is not a
        harmless inconsistency: the table sorts on EditRole, and QVariant
        compares ints numerically and strings LEXICALLY, so "1000" sorted before
        "999" for rows the metadata path filled and after it for rows the thumb
        path filled. Every programmatic consumer (Image/pixmap.cpp,
        Image/stack.cpp) already calls toInt(), and the displayed text is
        identical either way. */
    setData(index(row, G::WidthColumn), m.width);
    setData(index(row, G::HeightColumn), m.height);
    setData(index(row, G::AspectRatioColumn), QString::number((aspectRatio(m.width, m.height, m.orientation)), 'f', 2));
    QString dim = QString::number(m.width) + "x" + QString::number(m.height);
    setData(index(row, G::DimensionsColumn), dim);
    search += dim;
    setData(index(row, G::MegaPixelsColumn), QString::number((m.width * m.height) / 1000000.0, 'f', 2));
    setData(index(row, G::LoadMsecPerMpColumn), m.loadMsecPerMp);
    /*  Orientation is NOT written here. It was -- as QString::number(m.orientation)
        -- and then overwritten as an int by the write further down this same
        function, so the string never reached anything and the column read as an
        int. Removing it is what settles the column's type, the way Width and
        Height were settled; the A/B fingerprint is unchanged by the removal,
        which is the proof it was dead. */
    setData(index(row, G::RotationColumn), QString::number(m.rotationDegrees));
    setData(index(row, G::CameraMakeColumn), m.make);
    search += m.make;
    setData(index(row, G::CameraModelColumn), m.model);
    search += m.model;
    setData(index(row, G::ShutterspeedColumn), m.exposureTimeNum);
    setData(index(row, G::ApertureColumn), m.apertureNum);
    setData(index(row, G::ISOColumn), m.ISONum);
    setData(index(row, G::ExposureCompensationColumn), m.exposureCompensation);
//    setData(index(row, G::ExposureCompensationColumn), m.exposureCompensationNum);
    setData(index(row, G::LensColumn), m.lens);
    search += m.lens;
    setData(index(row, G::FocalLengthColumn), m.focalLengthNum);
    setData(index(row, G::FocusXColumn), m.focusX);
    setData(index(row, G::FocusYColumn), m.focusY);
    setData(index(row, G::GPSCoordColumn), m.gpsCoord);
    setData(index(row, G::KeywordsColumn), QVariant(m.keywords));
    search += Utilities::stringListToString(m.keywords);
    setData(index(row, G::KeywordPathsColumn), QVariant(m.keywordPaths));
    /* Ancestor names are only in the hierarchical form, so folding it into the search
       text is what lets a search for "Wildlife" find an image keyworded only "Heron". */
    search += Utilities::stringListToString(m.keywordPaths);
    /* The flat vocabulary the Keywords filter category and the catalog category read:
       both properties reduced to one de-duplicated list of names, so a tag Lightroom
       wrote twice (leaf in dc:subject, path in lr:hierarchicalSubject) is ONE keyword and
       an ancestor is a keyword in its own right. The two source columns above are left as
       the file spelled them -- see G::KeywordsAllColumn on why they must be. */
    QStringList keywordsAll = flattenKeywords(m.keywords, m.keywordPaths);
    setData(index(row, G::KeywordsAllColumn), QVariant(keywordsAll));
    setData(index(row, G::ShootingInfoColumn), m.shootingInfo);
    search += m.shootingInfo;
    setData(index(row, G::TitleColumn), m.title);
    search += m.title;
    setData(index(row, G::_TitleColumn), m._title);
//    if (index(row, G::CreatorColumn).data().toString() != "") m.creator = index(row, G::CreatorColumn).data().toString();
    setData(index(row, G::CreatorColumn), m.creator);
    search += m.creator;
    setData(index(row, G::_CreatorColumn), m._creator);
//    if (index(row, G::CopyrightColumn).data().toString() != "") m.copyright = index(row, G::CopyrightColumn).data().toString();
    setData(index(row, G::CopyrightColumn), m.copyright);
    search += m.copyright;
    setData(index(row, G::_CopyrightColumn), m._copyright);
//    if (index(row, G::EmailColumn).data().toString() != "") m.email = index(row, G::EmailColumn).data().toString();
    setData(index(row, G::EmailColumn), m.email);
    search += m.email;
    setData(index(row, G::_EmailColumn), m._email);
//    if (index(row, G::UrlColumn).data().toString() != "") m.url = index(row, G::UrlColumn).data().toString();
    setData(index(row, G::UrlColumn), m.url);
    search += m.url;
    setData(index(row, G::_UrlColumn), m._url);
    setData(index(row, G::CompareColumn), m.compare);
    /*  THE DECODE GEOMETRY, and it is written only when it was actually READ.
        A row whose metadata came from the local index (m.fromIndex) has none of
        this -- the catalog stores what is displayed and searched, not what is
        needed to decode -- and writing zeros here would be worse than writing
        nothing: the scratch store's set mask would then report the fields as
        WRITTEN, an offset of 0 is a legal offset, and ImageDecoder would decode
        from the front of the file instead of noticing it has to read the header
        itself. Leaving them unset is what makes the gap detectable. */
    if (!m.fromIndex) {
        setData(index(row, G::OffsetFullColumn), m.offsetFull);
        setData(index(row, G::LengthFullColumn), m.lengthFull);
        setData(index(row, G::WidthOrigPreviewColumn), m.widthOrigPreview);
        setData(index(row, G::HeightOrigPreviewColumn), m.heightOrigPreview);
        setData(index(row, G::OffsetThumbColumn), m.offsetThumb);
        setData(index(row, G::LengthThumbColumn), m.lengthThumb);
        setData(index(row, G::samplesPerPixelColumn), m.samplesPerPixel); // reqd for err trapping
        setData(index(row, G::isBigEndianColumn), m.isBigEnd);
        setData(index(row, G::ifd0OffsetColumn), m.ifd0Offset);
        setData(index(row, G::ifdOffsetsColumn), m.ifdOffsets);
        setData(index(row, G::XmpSegmentOffsetColumn), m.xmpSegmentOffset);
        setData(index(row, G::XmpSegmentLengthColumn), m.xmpSegmentLength);
        setData(index(row, G::IsXMPColumn), m.isXmp);
        setData(index(row, G::ICCSegmentOffsetColumn), m.iccSegmentOffset);
        setData(index(row, G::ICCSegmentLengthColumn), m.iccSegmentLength);
        setData(index(row, G::ICCBufColumn), m.iccBuf);
        setData(index(row, G::ICCSpaceColumn), m.iccSpace);
    }
    setData(index(row, G::OrientationOffsetColumn), m.orientationOffset);
    setData(index(row, G::OrientationColumn), m.orientation);
    setData(index(row, G::RotationDegreesColumn), m.rotationDegrees);
    // cancel as causing repeats for some videos
    // setData(index(row, G::MetadataReadingColumn), m.metadataReading);
    setData(index(row, G::MetadataStatusColumn), m.metaStatus);
    // setData(index(row, G::MissingThumbColumn), m.isEmbeddedThumbMissing);
    setData(index(row, G::CompareColumn), m.compare);
    setData(index(row, G::SearchTextColumn), search.toLower());

    // image cache helpers
    // do not set these.  Out of order when multi-folder selection
    // setData(index(row, G::IsCachingColumn), false);
    // setData(index(row, G::IsCachedColumn), false);
    // qDebug() << "add isCached for row =" << row;

    setData(index(row, G::AttemptsColumn), 0);
    setData(index(row, G::DecoderIdColumn), -1);
    setData(index(row, G::DecoderReturnStatusColumn), 0);
    setData(index(row, G::RawRenderColumn), false);
    // calc size in MB req'd to store image in cache
    if (!m.video) {
        int w, h;
        m.widthOrigPreview > 0 ? w = m.widthOrigPreview : w = m.width;
        m.heightOrigPreview > 0 ? h = m.heightOrigPreview : h = m.height;
        // 8 bits X 3 channels + 8 bit depth = (32*w*h)/8/1024/1024 = w*h/262144
        float mb;
        if (w == 0 || h == 0) mb = m.size / 1000000;
        else mb = static_cast<float>(w * h * 1.0 / 262144);
        setData(index(row, G::CacheSizeColumn), mb);
        /*
        QString msg = "row = " + QString::number(row) + " mb = " + QVariant(mb).toString();
        G::log("DataModel::addMetadataForItem", msg);
        //*/
    }

    // emit one compact notification (only if you need the view to refresh now)
    emit dataChanged(index(row, 0), index(row, columnCount()-1));

    // check for missing thumbnail in jpg/tiif
    if (m.isReadWrite)
        if (metadata->canEmbedThumb.contains(m.type.toLower()))
            if (m.isEmbeddedThumbMissing) {
                folderHasMissingEmbeddedThumb = true;
            }

    // req'd for 1st image, probably loaded before metadata cached
    if (row == 0) emit updateClassification();
    mLock = false;
    if (isDebug) qDebug() << "DataModel::addMetadataForItem" << "instance =" << instance << "DONE";

    // Publish so MetaRead worker can poll without a BlockingQueuedConnection.
    G::allMetadataAttempted = isMetaReadFinished();

    // signal ImageCache that row is loaded
    // if (imageCacheWaitingForRow > -1)
    //     qDebug() << "DataModel::addMetadataForItem row =" << row
    //              << "imageCacheWaitingForRow =" << imageCacheWaitingForRow
    //         ;

    if (row == imageCacheWaitingForRow) {
        // qDebug() << "DataModel::addMetadataForItem emit rowLoaded() for row =" << row;
        emit rowLoaded();
        imageCacheWaitingForRow = -1;
    }

    sampleRowBytesUsed(row);

    return true;
}

bool DataModel::metadataLoaded(int dmRow)
{
    if (G::isLogger) G::log("DataModel::metadataLoaded");
    if (isDebug) qDebug() << "DataModel::metadataLoaded" << "instance =" << instance
                          << "row =" << dmRow
                          << folderPathFromModelRow(dmRow);
    return index(dmRow, G::MetadataStatusColumn).data().toInt() == G::MetaLoaded;
}

bool DataModel::isDimensions(int sfRow)
{
    if (index(sfRow, G::WidthColumn).data().toInt() == 0) return false;
    if (index(sfRow, G::HeightColumn).data().toInt() == 0) return false;
    return true;
}

void DataModel::processErr(Error e)
{
    QString d = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss ");
    QString r = "Row: " + QString::number(e.sfRow) + " ";   // datamodel proxy row (sfRow)
    QString s = "Src: " + e.functionName + " ";             // error source function
    QString m = "Error: " + e.msg + "   ";                  // error message
    QString p;                                              // path
    QString l = "\n";                                       // newline separator
    QString o = " ";                                        // offset datetime string width
    o = o.repeated(d.count());

    switch(e.type) {
    case ErrorType::General:
        p = e.fPath;
        break;
    case ErrorType::DM:
        p = sf->index(e.sfRow,0).data(G::PathRole).toString();
        break;
    }

    errMsg = d + m + s + r + p;
    // errMsg = d + m + l + o + s + r + p;

    // save errMsg in datamodel if type = "DM"
    if (e.type == ErrorType::DM) {
        QModelIndex sfIdx = sf->index(e.sfRow, G::ErrColumn);
        QStringList errList = sfIdx.data().toStringList();
        errList << errMsg;
        QVariant v;
        v.setValue(errList);
        setValSf(e.sfRow, G::ErrColumn, v, instance, "DataModel::processErr");
    }
    // qDebug() << errMsg;
    // qDebug() << "DataModel::err" << sfRow << msg << errList << sfIdx.data().toStringList();
}

// void DataModel::errGeneral(Issue issue)
// {
//     // general error

//     // Error e;
//     // e.type = ErrorType::General;
//     // e.functionName = functionName;
//     // e.msg = msg;
//     // e.fPath = fPath;
//     // processErr(e);
// }

void DataModel::issue(const QSharedPointer<Issue>& issue)
{
/*
    Add issue related to a datamodel row
*/
    QString fun = "DataModel::issue";
    /*
    qDebug().noquote()
        << fun.leftJustified(30)
        << issue->TypeDesc.at(issue->type).leftJustified(10)
        << QString::number(issue->sfRow).rightJustified(5)
        << issue->msg.leftJustified(40)
        << issue->src.leftJustified(30)
        ;  //*/
    // check for null fPath
    if (issue->fPath == "" && issue->sfRow > -1) {
        issue->fPath = sf->index(issue->sfRow, 0).data(G::PathRole).toString();
    }

    // retrieve an existing list of issues for this datamodel row
    QModelIndex sfIdx = sf->index(issue->sfRow, G::ErrColumn);
    QVariant retrievedVariant = sfIdx.data(Qt::UserRole);
    QList<QSharedPointer<Issue>> issueList = retrievedVariant.value<QList<QSharedPointer<Issue>>>();

    // add the new issue
    issueList.append(issue);

    // resave the issue list in the datamodel
    QVariant v;
    v.setValue(issueList);
    sf->setData(sfIdx, v, Qt::UserRole); // not working
}

QStringList DataModel::rptIssues(int sfRow)
{
    // report all issues for a row in model
    QStringList list;
    QModelIndex sfIdx = sf->index(sfRow, G::ErrColumn);
    QVariant retrievedVariant = sfIdx.data(Qt::UserRole);
    QList<QSharedPointer<Issue>> issueList = retrievedVariant.value<QList<QSharedPointer<Issue>>>();
    foreach (QSharedPointer<Issue> issue, issueList) {
        bool oneLine = true;
        int offset = 23;
        QString msg = issue->toString(oneLine, offset);
        list.append(msg);
        // qDebug() << msg;
    }
    return list;
}

// void DataModel::errDM(Issue issue)
// {
//     // // error related to a datamodel row

//     // // Error e;
//     // // e.type = ErrorType::DM;
//     // // e.functionName = issue;
//     // // e.msg = msg;
//     // // e.sfRow = sfRow;
//     // // e.fPath = "";
//     // // processErr(e);

//     // // use Issue class

//     // // retrieve an existing list of issues for this datamodel row
//     // QModelIndex sfIdx = sf->index(issue.sfRow, G::ErrColumn);
//     // QVariant retrievedVariant = sfIdx.data(Qt::UserRole);
//     // QList<Issue> issueList = retrievedVariant.value<QList<Issue>>();

//     // // add the new issue
//     // issueList.append(issue);

//     // // resave the issue list in the datamodel
//     // QVariant v;
//     // v.setValue(issueList);
//     // sf->setData(sfIdx, v);
// }

double DataModel::aspectRatio(int w, int h, int orientation)
{
    if (G::isLogger) G::log("DataModel::aspectRatio");
    if (isDebug) qDebug() << "DataModel::aspectRatio";
    if (w == 0 || h == 0) return 1.0;
    // portrait
    if (orientation == 6 || orientation == 8) return h * 1.0 / w;
    // landscape
    else return w * 1.0 / h;
}

QVariant DataModel::valueSf(int row, int column, int role)
{
/*
    Thread safe
*/
    // QMutexLocker locker(&mutex);
    // range check in case model has changed, return invalid result
    if (row >= sf->rowCount()) return QVariant();
    return sf->index(row, column).data(role);
}

void DataModel::scheduleVisibleEmit(int dmRow)
{
/*
    Remember that this row needs a repaint, and make sure exactly one flush is queued.
    See the declaration for why per-write notification was the last of the accessibility
    cost.
*/
    if (dmRow < 0) return;
    pendingEmitRows.insert(dmRow);
    if (pendingEmitScheduled) return;
    pendingEmitScheduled = true;
    if (G::isPerfProbe) pendingEmitClock.start();

    /*  A NON-ZERO INTERVAL, AND THE ZERO IS THE BUG.

        Qt gives a zero timer the LOWEST priority there is: it fires when the event queue
        has nothing else in it. That is the correct choice for a coalescing flush right up
        until the thing filling the queue is the very work being coalesced. Scrolling a
        catalog scope moves the icon chunk, and the loader answers by delivering the whole
        2,000-row window -- measured at 3,628 icons in one second, each one a queued event
        onto this thread. The repaint for the forty rows the user is actually looking at
        was therefore scheduled behind the entire chunk fill: their icons were in the model
        within milliseconds and on screen a second or two later, which is exactly the
        symptom ("a page of thumbnails takes 1-2 seconds") and why the icon counters showed
        nothing wrong -- the icons WERE arriving on time.

        20 ms is about one frame: long enough that a burst still collapses into one
        notification (which is what keeps the accessibility rebuild off the per-icon path),
        short enough that the visible page cannot wait on the invisible remainder of the
        chunk. */
    QTimer::singleShot(kVisibleEmitDeferMs, this, [this]{ flushVisibleEmits(); });
}

void DataModel::flushVisibleEmits()
{
/*
    One dataChanged per CONTIGUOUS RUN of dirty rows.

    Runs rather than one span from the lowest to the highest: the visible rows are
    contiguous in the ordinary case, so this is usually a single notification, but a
    filtered or scattered set must not make the view repaint everything between two
    distant rows.
*/
    pendingEmitScheduled = false;
    if (G::isPerfProbe && pendingEmitClock.isValid()) {
        const qint64 lateNs = pendingEmitClock.nsecsElapsed();
        probeFlushCount++;
        if (lateNs > probeFlushMaxDelayNs) probeFlushMaxDelayNs = lateNs;
    }
    if (pendingEmitRows.isEmpty()) return;

    QList<int> rows(pendingEmitRows.cbegin(), pendingEmitRows.cend());
    pendingEmitRows.clear();
    std::sort(rows.begin(), rows.end());

    /*  THE EMISSION ITSELF IS THE LAST UNMEASURED THING ON THIS THREAD (G::isPerfProbe).
        Everything else on the scroll line is Winnow's own work; this is what Qt does when
        told a row changed -- on macOS, rebuilding the entire Cocoa accessibility element
        array for the model, measured at ~27 ms per notification at 43,000 rows. A scroll
        produces tens of flushes a second, so if this is where the residual stutter lives
        it is worth several hundred ms of every second and appears nowhere else.
        Cross-check with WINNOW_NO_A11Y=1, which turns the bridge off outright. */
    QElapsedTimer emitTimer;
    if (G::isPerfProbe) emitTimer.start();

    const int lastCol = columnCount() - 1;
    int runStart = rows.first();
    int prev = runStart;
    for (int i = 1; i < rows.size(); ++i) {
        if (rows.at(i) == prev + 1) { prev = rows.at(i); continue; }
        emit dataChanged(index(runStart, 0), index(prev, lastCol));
        runStart = prev = rows.at(i);
    }
    emit dataChanged(index(runStart, 0), index(prev, lastCol));

    if (G::isPerfProbe) probeFlushEmitNs += emitTimer.nsecsElapsed();
}

bool DataModel::iconRowVisible(const QModelIndex &dmIdx)
{
/*
    True when the row is currently visible in a view (or the optimization is off). Used by
    setIcon1 / setValDm / setValSf to skip the dataChanged notification for off-screen rows
    during a bulk load — they are stored without notifying and paint correctly when scrolled to.
    firstVisibleIcon / lastVisibleIcon are sf (proxy) rows maintained by MW::updateIconRange;
    a degenerate range (not yet established) falls back to "visible" so nothing is missed.
*/
    /*  COUNTED, because "the throttle is on" and "the throttle is throttling" are
        different claims and only the second one matters. Every true here is a
        dataChanged that reaches the views -- and on macOS an accessibility rebuild over
        the whole model. The counts are reported on the GUI stall line. */
    if (!G::useVisibleOnlyIconEmit) {
        G::probeEmitVisible.fetch_add(1, std::memory_order_relaxed);
        return true;
    }
    if (lastVisibleIcon <= firstVisibleIcon) {
        /*  RANGE NOT ESTABLISHED, so everything is treated as visible -- the safe answer,
            since a suppressed notification for a row that IS on screen leaves a blank
            cell. Counted separately: if a large load spends its time here, the throttle
            is a no-op and the fix is to establish the range before icons arrive. */
        G::probeEmitVisible.fetch_add(1, std::memory_order_relaxed);
        return true;
    }
    const int sfRow = sf->mapFromSource(dmIdx).row();
    const bool visible = sfRow >= firstVisibleIcon && sfRow <= lastVisibleIcon;
    if (visible) G::probeEmitVisible.fetch_add(1, std::memory_order_relaxed);
    else         G::probeEmitSuppressed.fetch_add(1, std::memory_order_relaxed);
    return visible;
}

void DataModel::setValDm(int dmRow, int dmCol, QVariant value, int instance,
                         QString src, int role)
{
/*
    Only call via connection.   Example: emit setValDm(args)
*/
    if (G::stop) return;
    if (isDebug)
    {
    qDebug() << "DataModel::setValDm"
             << "dmRow =" << dmRow
             << "dmCol =" << dmCol
             << "value =" << value
             << "call Instance =" << instance
             << "model Instance =" << this->instance
             << "src =" << src
             ;
    }

    if (instance != this->instance) {
        errMsg = "Instance clash from " + src;
        G::issueDedup("Comment", errMsg, "DataModel::setValueDm", dmRow);
        return ;
    }

    QModelIndex dmIdx = index(dmRow, dmCol);

    if (!dmIdx.isValid()) {
        errMsg = "Invalid dmIdx.  Src: " + src;
        G::issue("Warning", errMsg, "DataModel::setValueDm", dmRow);
        return;
    }

    /* The write is made under a QSignalBlocker and dataChanged emitted here, for
       VISIBLE ROWS ONLY. Called per icon for the NSThumb column during loads, where
       letting every row's write reach the views was ~half of the load-time GUI stall
       (measured). The blocker used to coalesce a value write and an alignment write
       into one signal; the alignment write is gone (alignment is per column now), but
       the throttle is the point and it stays. */
    {
        const QSignalBlocker blocker(this);
        setData(dmIdx, value, role);
    }
    if (iconRowVisible(dmIdx))
        scheduleVisibleEmit(dmIdx.row());
}

void DataModel::setValSf(int sfRow, int sfCol, QVariant value, int instance,
                         QString src, int role)
{
    /*
    Only call via connection.   Example: emit setValSf(args)
*/
    if (isDebug)
    {
        qDebug() << "DataModel::setValSf"
                 << "sfRow =" << sfRow
                 << "sfCol =" << sfCol
                 << "value =" << value
                 << "call Instance =" << instance
                 << "model Instance =" << this->instance
                 << "src =" << src
            ;
    }

    // Instance guard.
    //
    // Exception: always allow IsCachingColumn=false to pass. A stranded
    // isCaching=true blocks okToDecode forever (nextToCache will never pick
    // the row again), while a stale false is idempotent and harmless — the
    // row either gets re-decoded or stays cached. This closes the race where
    // a cleanup emit (okToCache, abortProcessing, resetStaleIsCaching) carries
    // an instance that has since been advanced by another rapid folder click.
    if (instance != this->instance) {
        const bool isCachingCleanup =
            (sfCol == G::IsCachingColumn) && !value.toBool();
        if (!isCachingCleanup) {
            errMsg = "Instance clash from " + src;
            G::issueDedup("Comment", errMsg, "DataModel::setValueSF", sfRow);
            return;
        }
    }

    QModelIndex sfIdx = sf->index(sfRow, sfCol);

    if (!sfIdx.isValid()) {
        errMsg = "Invalid sfIdx.  Src: " + src;
        G::issue("Warning", errMsg, "DataModel::setValueSF", sfRow);
        return;
    }

    /*  THE SAME THROTTLE setValDm ALREADY HAS, and for the same measured reason.

        This called sf->setData, which forwards to the source and lets the resulting
        dataChanged reach the proxy, the three views AND -- on macOS -- the Cocoa
        accessibility bridge, which rebuilds its element array for EVERY ROW IN THE MODEL
        on each notification. A sample of a stalled process put 94% of the main thread in
        exactly that: setValSf -> setData -> dataChanged -> QAbstractItemView::dataChanged
        -> QMacAccessibilityElement::updateTableModel -> populateTableArray -> malloc.
        At 42,979 rows a catalog scope froze for 42 seconds; disabling the bridge with
        WINNOW_NO_A11Y=1 removed the stall entirely and changed nothing else, which is
        what identified it.

        setValDm has written under a blocker and emitted for VISIBLE ROWS ONLY since the
        earlier load-responsiveness work, where per-row signals were measured at about
        half the load stall. Only the sf variant was left un-throttled, and it is the one
        the Reader and Thumb threads use. The asymmetry was the bug.

        A row nobody can see needs no repaint: the write lands in the model either way,
        and the view reads it when it scrolls into view. */
    const QModelIndex dmIdx = sf->mapToSource(sfIdx);
    if (!dmIdx.isValid()) {
        errMsg = "Invalid dmIdx from sfIdx.  Src: " + src;
        G::issue("Warning", errMsg, "DataModel::setValueSF", sfRow);
        return;
    }
    {
        const QSignalBlocker blocker(this);
        setData(dmIdx, value, role);
    }
    if (iconRowVisible(dmIdx))
        scheduleVisibleEmit(dmIdx.row());
}

bool DataModel::setCurrentSF(QModelIndex sfIdx, int instance)
{
    if (instance != this->instance) {
        errMsg = "Instance clash.";
        G::issueDedup("Comment", errMsg, "DataModel::setCurrent", sfIdx.row());
        return false;
    }

    // update current index parameters
    currentSfIdx = sfIdx;
    currentSfRow = sfIdx.row();
    currentDmIdx = sf->mapToSource(currentSfIdx);
    currentDmRow = currentDmIdx.row();
    currentFilePath = sf->index(currentSfRow, 0).data(G::PathRole).toString();
    if (isDebug)
    {
        qDebug() << "DataModel::setCurrent"
                 << "currentSfIdx =" << currentSfIdx
                 << "currentSfRow =" << currentSfRow
                 << "currentDmIdx =" << currentDmIdx
                 << "currentDmRow =" << currentDmRow
                 << "currentFilePath =" << currentFilePath
            ;
    }
    return true;
}

void DataModel::setCurrent(QModelIndex dmIdx, int instance)
{
    if (G::isLogger) G::log("DataModel::setCurrent (dmIdx)");
    if (instance != this->instance) {
        errMsg = "Instance clash.";
        G::issueDedup("Comment", errMsg, "DataModel::setCurrent", dmIdx.row());
        return;
    }

    // update current index parameters
    QMutexLocker locker(&dmMutex);
    QModelIndex sfIdx = sf->mapFromSource(dmIdx);
    currentSfIdx = sfIdx;
    currentSfRow = sfIdx.row();
    currentDmIdx = dmIdx;
    currentDmRow = currentDmIdx.row();
    currentFilePath = sf->index(currentSfRow, 0).data(G::PathRole).toString();
    if (isDebug)
    {
        qDebug() << "DataModel::setCurrent using dmIdx"
                 << "currentSfIdx =" << currentSfIdx
                 << "currentSfRow =" << currentSfRow
                 << "currentDmIdx =" << currentDmIdx
                 << "currentDmRow =" << currentDmRow
                 << "currentFilePath =" << currentFilePath
            ;
    }
}

void DataModel::setCurrent(QString fPath, int instance)
{
    if (G::isLogger) G::log("DataModel::setCurrent (fPath)");
    if (instance != this->instance) {
        errMsg = "Instance clash.";
        G::issueDedup("Comment", errMsg, "DataModel::setCurrent");
        return;
    }

    // update current index parameters
    QMutexLocker locker(&dmMutex);
    // if (fPathRow.contains(fPath)) {
    if (fPathRowContains(fPath)) {
        currentDmIdx = indexFromPath(fPath);
        currentFilePath = fPath;
    } else {
        currentDmIdx = index(0, 0);
        currentFilePath = index(0, 0).data(G::PathRole).toString();
    }
    currentDmRow = currentDmIdx.row();
    QModelIndex sfIdx = sf->mapFromSource(currentDmIdx);
    currentSfRow = sfIdx.row();

    if (isDebug)
    {
        qDebug() << "DataModel::setCurrent using fPath" << fPath
                 << "currentSfIdx =" << currentSfIdx
                 << "currentSfRow =" << currentSfRow
                 << "currentDmIdx =" << currentDmIdx
                 << "currentDmRow =" << currentDmRow
                 << "currentFilePath =" << currentFilePath
            ;
    }
}

void DataModel::setValuePath(QString fPath, int col, QVariant value, int instance, int role)
{
    if (isDebug) {
        qDebug() << "DataModel::setValuePath" << "instance =" << instance
                 << "col =" << col
                 << "fPath =" << fPath
                 // << "fPathRow[fPath] =" << fPathRow[fPath]
                 << "fPathRowValue(fPath) =" << fPathRowValue(fPath)
                ;
    }
    QModelIndex dmIdx = index(fPathRowValue(fPath), col);
    if (instance != this->instance) {
        errMsg = "Instance clash.";
        G::issueDedup("Comment", errMsg, "DataModel::setValuePath", dmIdx.row(), fPath);
        return;
    }
    if (G::stop) return;
    if (!dmIdx.isValid()) {
        errMsg = "Invalid dmIdx.";
        G::issue("Warning", errMsg, "DataModel::setValuePath", dmIdx.row(), fPath);
        return;
    }
    QMutexLocker locker(&dmMutex);
    setData(dmIdx, value, role);
}


void DataModel::setIconFromVideoFrame(int dmRow, QImage im, int fromInstance,
                                      qint64 duration, FrameDecoder *frameDecoder)
{
/*
    This slot is signalled from FrameDecoder, where the thumbnail and video duration are
    defined.  The FrameDecoder is generated from Thumb, which may be called more than once
    for a datamodel row.  We only need the first definition of duration and icon, so the
    rest are ignored.

    If the user is rapidly changing folders it is possible to receive a delayed signal
    from the previous folder. To prevent this, the datamodel instance is incremented
    every time a new folder is loaded, and this is checked against the signal instance.
*/
    //lastFunction = "";  // if req'd inclose in mutex
    if (G::isLogger) G::log("DataModel::setIconFromVideoFrame");
    if (isDebug)
        qDebug() << "DataModel::setIconFromVideoFrame         "
                 << "dmRow =" << dmRow
                 << "instance =" << instance
                 << "fromInstance =" << fromInstance
                 // << "fPath =" << dmIdx.data(G::PathRole).toString()
        ;

    if (G::stop) return;
    if (fromInstance != instance) {
        errMsg = "Instance clash.";
        G::issueDedup("Comment", errMsg, "DataModel::setIconFromVideoFrame", dmRow);
        return;
    }

    QModelIndex dmIdx = index(dmRow,0);

    if (!dmIdx.isValid()) {
        errMsg = "Invalid dmIdx.";
        G::issue("Warning", errMsg, "DataModel::setIconFromVideoFrame");
        return;
    }


    QMutexLocker locker(&dmMutex);
    QString modelDuration = index(dmRow, G::DurationColumn).data().toString();
    if (modelDuration == "") {
        duration /= 1000;
        QTime durationTime((duration / 3600) % 60, (duration / 60) % 60,
            duration % 60, (duration * 1000) % 1000);
        QString format = "mm:ss";
        if (duration > 3600) format = "hh:mm:ss";
        const QSignalBlocker blocker(this);
        setData(index(dmRow, G::DurationColumn), durationTime.toString(format));
    }

    /*  No itemFromIndex() guard. It used to test that the cell had an item
        before writing the icon, which was a proxy for "the row exists"; the
        icon is not on the item any more and a covered column may not create one
        at all, so a null item here would have silently dropped the thumbnail.
        dmIdx.isValid() is the question that was actually being asked. */
    if (dmIdx.isValid() && data(dmIdx, Qt::DecorationRole).isNull()) {
        /*  BLOCKED AND EMITTED ONCE, the pattern setIcon1, setIcon, setValDm and setValSf
            all use. The braces were already here with nothing in them -- the shape was
            started and never finished -- so these were five separate dataChanged
            notifications per video frame, and on macOS each one makes QAbstractItemView
            tell the Cocoa accessibility bridge, which rebuilds its element array for
            every row in the model.

            THE LAST WRITE PATH TO GET THIS. Throttling the other four took a catalog
            scope's freeze from 42 s to 17 s and then stopped helping, because this one
            does not consult iconRowVisible at all -- so it was not even counted, and
            emits(visible) sat frozen at 225 through every stall while a sample of the
            stalled process showed setIconFromVideoFrame at the top of it. A catalog holds
            videos (201 in the icon range here); each frame decodes slowly and arrives
            separately, so this is a rebuild of 42,979 elements per video, spread over the
            whole load.

            The blocker ENDS before videoReadingCleared below: QSignalBlocker blocks every
            signal this object emits, and MetaRead needs that one to drop its in-flight
            marker. */
        {
            const QSignalBlocker blocker(this);
            setData(dmIdx, QVariant(QIcon(QPixmap::fromImage(im))), Qt::DecorationRole);
            setData(index(dmIdx.row(), G::IconLoadedColumn), true);
            setData(index(dmIdx.row(), G::MetadataStatusColumn), G::MetaLoaded);
            setData(index(dmIdx.row(), G::MetadataReadingColumn), false);
            // set aspect ratio for video
            if (im.height() > 0) {
                QString aspectRatio = QString::number(im.width() * 1.0 / im.height(), 'f', 2);
                setData(index(dmRow, G::AspectRatioColumn), aspectRatio);
            }
        }
        if (iconRowVisible(dmIdx))
            scheduleVisibleEmit(dmIdx.row());
    }

    /*  Tell MetaRead the decode is resolved so it can drop its worker-local
        in-flight marker. Emitted unconditionally -- if the icon was already
        set the row is done just the same, and leaving the marker in place
        would keep the dispatcher from ever revisiting the row. */
    const int sfRow = sf->mapFromSource(index(dmRow, 0)).row();
    if (sfRow >= 0) emit videoReadingCleared(sfRow, fromInstance);
}

void DataModel::clearVideoReadingFlag(int dmRow, int fromInstance)
{
/*
    Failure-path counterpart to setIconFromVideoFrame.  MetaRead skips clearing
    MetadataReadingColumn for video rows (Cache/metaread.cpp processReturningReader)
    so dispatch never spawns a second QMediaPlayer on the same file while one
    is in flight.  setIconFromVideoFrame clears the flag on the success path;
    this slot clears it on every failure path FrameDecoder reports
    (invalid media, playback error, invalid frame, null image, exception).
    Without it the row stays MetadataReadingColumn=true forever and the
    dispatcher refuses to revisit it.

    IconLoaded is also set true here so a video whose thumbnail can't be
    decoded is treated as "done". On the success path setIconFromVideoFrame
    sets it; on failure it was left false, so readIcon kept re-emitting
    videoFrameDecode every time the row re-entered the dispatch range. Each
    retry spun up a fresh AVFoundation/QMediaPlayer decode whose threads were
    never joined — a thread leak that exhausted pthread_create under sustained
    folder bouncing. Marking the row done stops the retry loop.
*/
    if (G::isLogger) G::log("DataModel::clearVideoReadingFlag");
    if (G::stop) return;
    if (fromInstance != instance) return;

    QModelIndex dmIdx = index(dmRow, 0);
    if (!dmIdx.isValid()) return;

    {
        QMutexLocker locker(&dmMutex);
        setData(index(dmRow, G::MetadataReadingColumn), false);
        // attempted but no frame decoded -> failed, and done (no retry)
        setData(index(dmRow, G::MetadataStatusColumn), G::MetaFailed);
        setData(index(dmRow, G::IconLoadedColumn), true);
    }

    // failure counterpart of the emit in setIconFromVideoFrame
    const int sfRow = sf->mapFromSource(index(dmRow, 0)).row();
    if (sfRow >= 0) emit videoReadingCleared(sfRow, fromInstance);
}

void DataModel::updateIconChunkLoaded(int dmRow)
{
/*
    Refresh G::iconChunkLoaded without rescanning the whole icon chunk on every
    icon set. isAllIconChunkLoaded needs all non-video rows in
    [startIconRange, endIconRange] to carry an icon. Since icons-in-chunk can
    never exceed the model-wide iconLoadedCount, and videos-in-chunk can never
    exceed the model-wide videoRowCount, the chunk cannot be complete until
    iconLoadedCount reaches (span - videoRowCount). Below that threshold we
    answer false for free; only at/above it do we run the authoritative scan.
    On a fresh front-loaded folder iconLoadedCount tracks the chunk directly,
    so the O(chunk) scan runs only as the chunk nears completion instead of
    once per icon — removing the prior O(N·chunk) GUI-thread cost.
*/
    const int span = endIconRange - startIconRange + 1;
    /*  Unloadable rows come off the requirement with the videos. A catalog scope can hold
        rows whose file is on an unplugged drive or gone, and requiring an icon of those
        left this permanently false -- which is what put MetaRead into a redo loop that
        blocked the GUI for 31 seconds (see "The Redo Loop That Could Never Finish"). */
    /*  THE GATE IS MODEL-WIDE AND THE REQUIREMENT IS CHUNK-LOCAL, which makes it a gate
        that never closes once the model is bigger than the chunk. iconLoadedCount counts
        icons ANYWHERE in the model; needed is the span of the WINDOW. On a 42,956-row
        catalog scope with a 2,000-row chunk the count passes 2,000 almost at once and
        stays there, so from then on every single icon write ran the full authoritative
        scan -- 2,000 proxy rows x 3 columns, on the GUI thread, per icon.

        Measured by the scroll probe: "scan calls = 1247  rows = 2495247  ms = 306.8" in
        one second of scrolling, with setIconRange responsible for 2 of those calls. The
        other 1,245 were icon writes.

        So the answer is maintained incrementally instead of recomputed. iconChunkMissing
        is the number of rows in the CURRENT range that still owe an icon; setIconRange
        recomputes it exactly whenever the range moves (which is every scroll event, so
        any drift is corrected within one event), and an icon landing inside the range
        decrements it. O(1) per icon, and the O(span) walk happens once per range change
        rather than once per thumbnail. */
    if (dmRow >= 0) {
        const int sfRow = sf->mapFromSource(index(dmRow, 0)).row();
        if (sfRow >= startIconRange && sfRow <= endIconRange && iconChunkMissing > 0)
            --iconChunkMissing;
        G::iconChunkLoaded = (iconChunkMissing == 0);
        return;
    }

    const int needed = span - videoRowCount.load(std::memory_order_relaxed)
                            - iconUnloadableCount.load(std::memory_order_relaxed);
    if (span > 0 &&
        iconLoadedCount.load(std::memory_order_relaxed) < needed) {
        G::iconChunkLoaded = false;
        return;
    }
    G::iconChunkLoaded = isAllIconChunkLoaded(startIconRange, endIconRange);
}

int DataModel::countIconChunkMissing(int first, int last)
{
/*
    How many rows in [first, last] still owe an icon -- the counting form of
    isAllIconChunkLoaded, and it must apply exactly the same exemptions (video rows, and
    rows whose file cannot be opened at all) or the two would disagree and the icon loader
    would redo forever chasing a row nobody expects an icon from.
*/
    if (first < 0 || last >= sf->rowCount()) return 0;
    int missing = 0;
    for (int row = first; row <= last; ++row) {
        if (sf->index(row, G::VideoColumn).data().toBool()) continue;
        if (sf->index(row, G::AvailabilityColumn).data().toInt()
                != int(Catalog::Availability::Present)) continue;
        if (sf->index(row, 0).data(Qt::DecorationRole).isNull()) ++missing;
    }
    return missing;
}

void DataModel::setIcon(QModelIndex dmIdx, const QPixmap &pm, int fromInstance, QString src)
{
/*
    setIcon is a slot that can be signalled from another thread.  If the user is rapidly
    changing folders it is possible to receive a delayed signal from the previous folder.
    To prevent this, the datamodel instance is incremented every time a new folder is
    loaded, and this is checked against the signal instance.

    In addition, the signal queue from MetaRead is cleared in MW::stop to prevent
    lagging calls when the folder has been changed.

    This function is subject to potential race conditions, so it is critical that it only
    be called via a connection with Qt::BlockingQueuedConnection.

    Do not use QMutexLocker.
*/
    if (G::isLogger) G::log("DataModel::setIcon");

    if (fromInstance != instance) {
        errMsg = "Instance clash from " + src;
        G::issueDedup("Comment", errMsg, "DataModel::setIcon", dmIdx.row());
        return;
    }

    if (isDebug)
    {
        // must come after instance check
        qDebug() << "DataModel::setIcon"
                 << "src =" << src
                 << "instance =" << instance
                 << "fromInstance =" << fromInstance
                 << "row =" << dmIdx.row()
                 << folderPathFromProxyRow(proxyIndexFromModelIndex(dmIdx).row());
    }
    if (loadingModel) {
        // errMsg = "Model is still loading..";
        // G::issue("Warning", errMsg, "DataModel::setIcon", dmIdx.row());
        // return;
    }
    if (G::stop) {
        return;
    }
    if (!dmIdx.isValid()) {
        errMsg = "Invalid dmIdx.";
        G::issue("Warning", errMsg, "DataModel::setIcon");
        return;
    }
    if (dmIdx.row() >= rowCount()) {
        QString r = QString::number(dmIdx.row());
        QString c = QString::number(rowCount());
        errMsg = "Model range exceeded.  Row " + r + " > rowCount " + c;
        G::issue("Warning", errMsg, "DataModel::setIcon", dmIdx.row());
        return;
    }

    if (G::memoryOverrunFlag.load(std::memory_order_relaxed)) return;

    /* Idempotent: see setIcon1 for rationale.  Replacing a live decoration
       runs ~QPixmapIconEngine on the old QIcon, which under memory pressure
       can crash. */
    /*  Through data(), not itemFromIndex()->icon(): the thumbnail lives in the
        path-keyed icon store now, and the item's own icon is always null. */
    /*  BLOCKED AND EMITTED ONCE, the same shape setIcon1 uses two functions down and for
        the same measured reason. These were three bare setData calls, so three separate
        dataChanged notifications per icon -- and on macOS each one makes
        QAbstractItemView tell the Cocoa accessibility bridge, which rebuilds its element
        array for EVERY ROW IN THE MODEL. Throttling setValSf took a catalog scope's
        freeze from 42 s to 15 s; this is the other path the icons arrive by. */
    {
        if (!data(dmIdx, Qt::DecorationRole).isNull()) {
            {
                const QSignalBlocker blocker(this);
                setData(index(dmIdx.row(), G::IconLoadedColumn), true);
                setData(index(dmIdx.row(), G::MetadataReadingColumn), false);
            }
            if (iconRowVisible(dmIdx))
                scheduleVisibleEmit(dmIdx.row());
            if (G::isPerfProbe) probeIconsRedundant++;
            updateIconChunkLoaded(dmIdx.row());
            return;
        }
    }

    const QVariant vIcon = QVariant(QIcon(pm));
    {
        const QSignalBlocker blocker(this);
        setData(dmIdx, vIcon, Qt::DecorationRole);
        setData(index(dmIdx.row(), G::IconLoadedColumn), true);
        setData(index(dmIdx.row(), G::MetadataReadingColumn), false);
    }
    if (iconRowVisible(dmIdx))
        scheduleVisibleEmit(dmIdx.row());
    if (G::isPerfProbe) probeIconsSet++;
    updateIconChunkLoaded(dmIdx.row());
}

void DataModel::setDevelopIcon(int dmRow, const QImage &im)
{
/*
    Replace a row's thumbnail with a newly rendered develop preview, so the grid follows
    an edit as soon as it is flushed.

    setIcon1 refuses to replace a live icon on purpose: several loaders race to deliver a
    thumbnail for the same row, and destroying a QIcon under memory pressure has crashed
    in QPixmapIconEngine. That guard is about UNSOLICITED duplicate deliveries. This is
    the opposite case -- a single, user-initiated, GUI-thread replacement of a thumbnail
    the user just changed -- so it overwrites by design. The memoryOverrunFlag bail is
    kept, because that one is about libmalloc state, not about duplicates.

    The delegate caches its own scaled QPixmap per row, so it has to be told too or it
    keeps painting the old thumb.
*/
    if (G::isLogger) G::log("DataModel::setDevelopIcon");

    if (G::memoryOverrunFlag.load(std::memory_order_relaxed)) return;
    if (im.isNull()) return;

    QModelIndex dmIdx = index(dmRow, 0);
    if (!dmIdx.isValid()) return;

    {
        const QSignalBlocker blocker(this);
        setData(dmIdx, QVariant(QIcon(QPixmap::fromImage(im))), Qt::DecorationRole);
        setData(index(dmRow, G::IconLoadedColumn), true);
        /* A crop changes the aspect ratio, and the delegate lays the cell out from it. */
        if (im.height() > 0)
            setData(index(dmRow, G::IconAspectRatioColumn),
                    (qreal)im.width() / im.height());
    }
    emit dataChanged(dmIdx, index(dmRow, columnCount() - 1));
}

void DataModel::clearDevelopIcon(int dmRow)
{
/*
    Drop a row's icon and its loaded flag so the icon loader fetches it again. setIcon1
    skips rows that already have an icon, so clearing is what makes a re-read happen.
    Mirrors clearIconsOutsideChunkRange, which uses the same mechanism for eviction.
*/
    if (G::isLogger) G::log("DataModel::clearDevelopIcon");

    QModelIndex dmIdx = index(dmRow, 0);
    if (!dmIdx.isValid()) return;
    {
        const QSignalBlocker blocker(this);
        setData(dmIdx, QVariant(), Qt::DecorationRole);
        setData(index(dmRow, G::IconLoadedColumn), false);
    }
    emit dataChanged(dmIdx, index(dmRow, columnCount() - 1));
}

void DataModel::setIcon1(int dmRow, const QImage &im, int fromInstance, QString src)
{
    /*
    setIcon is a slot that can be signalled from another thread. If the user is
    rapidly changing folders it is possible to receive a delayed signal from the
    previous folder. To prevent this, the datamodel instance is incremented every
    time a new folder is loaded, and this is checked against the signal instance.

    In addition, the signal queue from MetaRead is cleared in MW::stop to prevent
    lagging calls when the folder has been changed.

    This function is subject to potential race conditions, so it is critical that it
    only be called via a connection with Qt::BlockingQueuedConnection.

    This is a duplicate of DataModel::setIcon, but with different call parameters, and it
    has a backpressure counter is incremented by Reader. dm->queuedReaderEvents is an
    std::atomic<int> on DataModel that counts Reader-thread events emitted to the GUI but
    not yet drained. A producer/consumer imbalance — e.g. recursing an Apple
    .photoslibrary where Readers race through tiny JPG derivatives faster than the GUI
    can drain its event queue — would otherwise balloon Qt's queue and memory. The cap
    (4× readers) is loose enough that steady-state throughput is unaffected; it only
    engages when the GUI is genuinely falling behind.

    Do not use QMutexLocker.
*/
    /* Backpressure: matches Reader's fetch_add before emit setIcon.
       RAII so every early-return path also decrements. */
    struct QrEvGuard {
        std::atomic<int> &c;
        ~QrEvGuard() { c.fetch_sub(1, std::memory_order_relaxed); }
    } qrEvGuard{queuedReaderEvents};

    /*  WHAT AN ICON COSTS THE GUI THREAD ONCE IT HAS ARRIVED (G::isPerfProbe). Every other
        field on the scroll line measures work AROUND the icons -- the scan, the sweep, the
        repaint, the reads. This is the one that scales with "icons set", which during a
        scroll is thousands a second while only the visible page is ever painted. */
    QElapsedTimer probeSetTimer;
    const bool probeSet = G::isPerfProbe;
    if (probeSet) probeSetTimer.start();
    struct ProbeSet {
        DataModel *dm; QElapsedTimer &t; bool on;
        ~ProbeSet() { if (on) dm->probeIconSetNs += t.nsecsElapsed(); }
    } probeSetGuard{this, probeSetTimer, probeSet};

    if (G::isLogger) G::log("DataModel::setIcon1", "src = " + src);

    if (fromInstance != instance) {
        errMsg = "Instance clash from " + src;
        G::issueDedup("Comment", errMsg, "DataModel::setIcon", dmRow);
        return;
    }

    if (isDebug)
    {
        QModelIndex dmIdx = index(dmRow,0);
        // must come after instance check
        qDebug() << "DataModel::setIcon1"
                 << "row =" << dmRow
                 << "isAlreadyIcon =" << !dmIdx.data(Qt::DecorationRole).isNull()
                 << "src =" << src
                 ;
    }
    if (loadingModel) {
        errMsg = "Model is still loading..";
        G::issue("Warning", errMsg, "DataModel::setIcon", dmRow);
        return;
    }

    if (G::stop) {
        return;
    }

    /* Once the heap cap latches, libmalloc state is fragile; further QPixmap
       allocation here can store/destroy buffers that crash later when freed
       (mirrors addMetadataForItem's bail at line ~1532). */
    if (G::memoryOverrunFlag.load(std::memory_order_relaxed)) return;

    QModelIndex dmIdx = index(dmRow,0);

    if (!dmIdx.isValid()) {
        errMsg = "Invalid dmIdx.";
        G::issue("Warning", errMsg, "DataModel::setIcon");
        return;
    }

    /* Idempotent: skip if a thumb is already set. Multiple paths can deliver
       a thumb for the same row (Reader::setIcon and TiffThumbDecoder::setIcon
       both connect here, plus rapid scrolling can re-dispatch). Replacing a
       live decoration runs ~QPixmapIconEngine on the old QIcon — which under
       memory pressure can hit a freed/poisoned pointer and abort.  Mirrors
       the guard in setIconFromVideoFrame:2214. */
    /*  Through data(), not itemFromIndex()->icon(): the thumbnail lives in the
        path-keyed icon store now, and the item's own icon is always null. */
    {
        if (!data(dmIdx, Qt::DecorationRole).isNull()) {
            // ensure flags are correct even though the pixmap is unchanged (batched)
            {
                const QSignalBlocker blocker(this);
                setData(index(dmRow, G::IconLoadedColumn), true);
                setData(index(dmRow, G::MetadataReadingColumn), false);
            }
            if (iconRowVisible(dmIdx))
                scheduleVisibleEmit(dmRow);
            if (G::isPerfProbe) probeIconsRedundant++;
            updateIconChunkLoaded(dmRow);
            return;
        }
    }

    if (G::isLogger) G::log("DataModel::setIcon1 updating", "src = " + src);

    const QVariant vIcon = QVariant(QIcon(QPixmap::fromImage(im)));
    /* Batch the four per-icon setData under a QSignalBlocker and emit ONE dataChanged,
       mirroring addMetadataForItem. Four separate dataChanged here were measured at
       ~2 ms/icon of synchronous proxy+view propagation (the load-time GUI stall); one
       coalesced signal collapses that. The blocker is scoped so the manual emit fires. */
    {
        const QSignalBlocker blocker(this);
        setData(dmIdx, vIcon, Qt::DecorationRole);
        setData(index(dmRow, G::IconLoadedColumn), true);
        setData(index(dmRow, G::MetadataReadingColumn), false);
        setData(index(dmRow, G::IconAspectRatioColumn), (qreal)im.width()/im.height());
    }
    // Notify views only for visible rows; off-screen icons paint when scrolled to.
    if (iconRowVisible(dmIdx))
        scheduleVisibleEmit(dmRow);
    if (G::isPerfProbe) probeIconsSet++;
    updateIconChunkLoaded(dmRow);

    /* Layer 2 (measured refinement): accumulate the real per-icon footprint. Still
       measured -- avgIconMB and the diagnostics report both read it -- but no longer
       acted on. Layer 2 existed to GROW a JIT window once the true footprint proved it
       would fit, and under the option's current meaning (the screens around the visible
       page) a window that fits memory is not the goal: growing it back to thousands of
       rows is precisely the cost the option exists to avoid. */
    iconBytesSum.fetch_add(im.sizeInBytes(), std::memory_order_relaxed);
    iconSamples.fetch_add(1, std::memory_order_relaxed);
}

bool DataModel::iconLoaded(int sfRow, int instance)
{
    QMutexLocker locker(&dmMutex);
    if (G::isLogger) G::log("DataModel::iconLoaded");
    if (isDebug) qDebug() << "DataModel::iconLoaded" << "instance =" << this->instance
                          << "fromInstance =" << instance
                          << "row =" << sfRow
                          << folderPathFromProxyRow(sfRow);
    // might be called from previous folder during folder change
    if (instance != this->instance) {
        errMsg = "Instance clash.";
        G::issueDedup("Comment", errMsg, "DataModel::iconLoaded", sfRow);
        return false;
    }
    if (sfRow >= sf->rowCount()) return false;
    // QModelIndex dmIdx = sf->mapToSource(sf->index(sfRow, 0));
    // if (dmIdx.isValid()) return !(itemFromIndex(dmIdx)->icon().isNull());
    // else return false;

    return sf->index(sfRow, G::IconLoadedColumn).data().toBool();
}

int DataModel::iconCount()
{
/*
    used for reporting / debugging only
*/
    if (isDebug) qDebug() << "DataModel::iconCount" << "instance =" << instance;
    int count = 0;
    QMutexLocker locker(&dmMutex);
    for (int row = 0; row < rowCount(); ++row) {
//        qDebug() << "DataModel::iconCount  itemFromIndex  row =" << row;
        /*  Through data(), not itemFromIndex()->icon(): the thumbnail lives in
            the path-keyed icon store now, so the item's icon is always null and
            this counted zero on every call. */
        if (!index(row, 0).data(Qt::DecorationRole).isNull()) count++;
    }
    return count;
}

bool DataModel::isAllIconChunkLoaded(int first, int last)
{
    if (isDebug)
        qDebug() << "DataModel::isAllIconChunkLoaded" << "instance =" << instance;

    if (first < 0 || last > sf->rowCount()) return false;

    /*  Probe only (G::isPerfProbe). This walk is the first of the three per-scroll costs
        described in the header -- see DataModel::reportScrollProbe. */
    QElapsedTimer probeTimer;
    if (G::isPerfProbe) {
        probeTimer.start();
        probeScanCalls++;
        probeScanRows += qMax(0, last - first + 1);
    }
    struct ProbeScan {
        DataModel *dm; QElapsedTimer &t; bool on;
        ~ProbeScan() { if (on) dm->probeScanNs += t.nsecsElapsed(); }
    } probeScan{this, probeTimer, G::isPerfProbe};

    for (int row = first; row <= last; ++row) {
        // ignore video
        if (sf->index(row, G::VideoColumn).data().toBool()) continue;
        /*  Ignore a row that cannot produce an icon at all: its file is on an unmounted
            volume or is gone. Present is 0 and is also the value an unset cell reads, so
            a folder load -- which never writes this column -- is unaffected. */
        if (sf->index(row, G::AvailabilityColumn).data().toInt()
                != int(Catalog::Availability::Present)) continue;

        QModelIndex sfIdx = sf->index(row, 0);
        if (!sfIdx.isValid()) {
            /*
            qDebug() << "DataModel::isAllIconChunkLoaded invalid index!"
                     << "row =" << row
                     << "first =" << first
                     << "last =" << last
                ; //*/
            return false;
        }
        if (sfIdx.data(Qt::DecorationRole).isNull()) {
            /*
            qDebug() << "DataModel::allIconChunkLoaded  false for row =" << row
                     << "first =" << first
                     << "last =" << last
                        ; //*/
            return false;
        }
    }
    // qDebug() << "DataModel::allIconChunkLoaded = true";
    return true;

}

bool DataModel::isIconRangeLoaded()
/*
    Called by MetaRead::dispatch when aIsDone && bIsDone (readers have been dispatched
    for all rows) to determine if a redo is required because a metadata or icon read has
    failed.

    Called locally by setIconRange.
*/
{
    for (int row = startIconRange; row <= endIconRange; row++) {
        if (sf->index(row,0).data(Qt::DecorationRole).isNull()) return false;
    }
    return true;
}

void DataModel::setIconRange(int sfRow)
/*
    Called by MW::updateChange and MW::updateIconRange.
*/
{
    // if (iconChunkSize >= rowCount()) {
    //     G::iconChunkLoaded = true;
    //     return;
    // }
    int rows = sf->rowCount();
    int start = qMax(0, sfRow - iconChunkSize / 2);
    int end = qMin(rows - 1, start + iconChunkSize);
    start = qMax(0, end - iconChunkSize);
    /* Atomic stores: MetaRead::setStartRow reads these on the metaReadThread.
       (Was an unguarded plain-int write — TSan-confirmed race, datamodel.h.) */
    startIconRange = start;
    endIconRange = end;
    if (G::isPerfProbe) probeSetRangeCalls++;
    /*  The exact recount, once per range change -- what makes the incremental
        maintenance in updateIconChunkLoaded self-correcting. */
    iconChunkMissing = countIconChunkMissing(startIconRange, endIconRange);
    G::iconChunkLoaded = (iconChunkMissing == 0);
}

void DataModel::setChunkSize(int chunkSize)
{
    iconChunkSize = chunkSize;
    setIconRange(currentSfRow);
}

int DataModel::maxIconChunkOrAll() const
{
/*
    The user's ceiling on how many thumbnails are held in memory at once, or
    "all of them" when they have chosen no limit.

    IT USED TO BE A WARNING RATHER THAN A LIMIT. G::maxIconChunk existed and was
    used for one thing: okManyImagesWarning, which asks "there are more than
    10,000 images... you may experience sluggish responses or system hangs. Do
    you wish to continue?". Nothing enforced it, so answering yes meant caching
    an icon for every row -- 7.7 GB of thumbnails for a 43,000-image scope,
    measured. The number named a risk the user was asked to accept instead of a
    bound the code would keep.

    It is now the bound. Above it the existing sliding window applies
    (setIconRange, needToRead, clearIconsOutsideChunkRange all key off
    iconChunkSize < rowCount()), so a large scope scrolls instead of failing to
    fit -- which is what makes browsing a whole catalog a size question rather
    than a hazard.

    Zero means no limit, for the user who would rather spend the memory.
*/
    const int m = G::maxIconChunk;
    return m > 0 ? m : rowCount();
}

void DataModel::resolveIconChunkSize()
/*
    Decide the icon-cache strategy for the just-loaded folder.

    When G::useJitIconCache is false (default), behaviour is brute force: iconChunkSize is
    set to cover every row, so the whole folder is cached.

    When G::useJitIconCache is true (Layer 1), the folder degrades to just-in-time caching
    only when it needs to. iconBudgetCount() returns how many icons fit the thumbnail memory
    budget (free memory after a safety reserve and the image cache's claim). If the whole
    folder fits, it is cached fully (still brute force for small folders); otherwise
    iconChunkSize is clamped to the budget, turning it into a bounded sliding window. The
    existing machinery (setIconRange, MetaRead::needToRead, clearIconsOutsideChunkRange) does
    the rest, since they all key off iconChunkSize < rowCount().

    At endLoad no icons are loaded yet, so the brute-force budget uses a worst-case
    per-icon estimate. There is no later re-decision: the JIT window is the visible page's
    neighbourhood, which needs no measurement, and growing a window back toward the memory
    budget is the cost the option exists to avoid.

    Called from endLoad on the GUI thread after a folder has loaded.
*/
{
    const int rows = rowCount();

    // new folder: reset the measured-footprint accumulators
    iconBytesSum.store(0, std::memory_order_relaxed);
    iconSamples.store(0, std::memory_order_relaxed);

    if (rows == 0) return;

    if (!G::useJitIconCache) {
        /*  Brute force: cache an icon for every row -- BUT ONLY IF THEY FIT.

            Caching every icon is the right answer for a folder of five hundred
            and stops being one somewhere before forty thousand. An icon is a
            256px ARGB QPixmap, ~178 KB measured, so a 43,000-image recursive
            scope asks for 7.7 GB of thumbnails alone: the run that found this
            reached 6.5 GB resident with only 26,000 icons loaded and never
            finished reading metadata.

            The bounded window that handles this already exists -- setIconRange,
            MetaRead::needToRead and clearIconsOutsideChunkRange all key off
            iconChunkSize < rowCount() -- it was simply never reached, because
            G::useJitIconCache is off by default and this branch returned first.

            So the flag now chooses a PREFERENCE, not safety: with it off the
            whole set is cached whenever the whole set fits, and when it does not
            the window applies regardless. A folder small enough to fit behaves
            exactly as before, which is every folder that was working. */
        const int budget = qMax(iconBudgetCount(), iconChunkFloor());
        iconChunkSize = qMin(rows, qMin(budget, maxIconChunkOrAll()));
        setIconRange(currentSfRow);
        return;
    }

    /*  JIT (Layer 1): HOLD THE SCREENS AROUND THE ONE BEING LOOKED AT, and nothing else.

        This used to size the window from the memory BUDGET, which on a machine with
        memory to spare is most of the model -- so turning JIT on changed almost nothing,
        and the window stayed at the user's thumbnail ceiling (2,000 rows for a 40-row
        page). Every scroll then asked the loader for up to 2,000 thumbnails to show 40,
        and the GUI thread absorbed them as queued icon events: "icons set = 4792" in one
        second, inside a 1,832 ms stall. The reads were not the problem and neither was
        any single line of code -- the WORK ITEM was simply forty times bigger than the
        question being asked.

        The chunk is a memory bound; it was never meant to be the read-ahead list, and
        under this option it stops being one. iconChunkFloor() is the previous, current
        and next screens (3x the visible page, minimum 256), which is what a person
        scrolling actually needs in hand.

        Memory does not need consulting for a window this size, so it is not: the budget
        only ever mattered because the window could be enormous. */
    iconChunkSize = qMin(rows, iconChunkFloor());

    if (isDebug || G::isLogger)
        G::log("DataModel::resolveIconChunkSize",
               QString("rows=%1 visibleIcons=%2 iconChunkSize=%3 mode=%4")
                   .arg(rows).arg(visibleIcons).arg(iconChunkSize.load())
                   .arg(iconChunkSize < rows ? "JIT screens window" : "full"));

    setIconRange(currentSfRow);
}

double DataModel::avgIconMB()
/*
    Per-icon thumbnail footprint in MB. Returns the measured running average once icons
    have actually loaded (Layer 2), otherwise a conservative worst-case estimate: a square
    icon at the long edge, 4 bytes/px (QPixmap ARGB32). Real thumbnails are smaller
    (KeepAspectRatio), so starting worst-case biases the initial decision toward JIT (safe).
*/
{
    const int n = iconSamples.load(std::memory_order_relaxed);
    if (n > 0)
        return double(iconBytesSum.load(std::memory_order_relaxed)) / n / (1024.0 * 1024.0);
    return double(G::maxIconSize) * G::maxIconSize * 4 / (1024.0 * 1024.0);
}

int DataModel::iconBudgetCount()
/*
    Number of icons that fit the thumbnail memory budget, coordinating with the image cache.

    The budget is the free memory remaining after (a) a safety reserve so OS memory pressure
    never fires and (b) the image cache's own remaining claim (G::imageCacheHeadroomMB),
    plus what thumbnails already hold (since those bytes are part of the thumbnail total, and
    are already reflected in the lower available figure). A jitIconCacheMemFraction share of
    that remainder is allocated to thumbnails.

    G::availableMemoryMB is a periodically-refreshed atomic (status bar, ImageCache); the
    last-known value is read here rather than forcing a platform refresh (keeps DataModel
    free of mac.h/win.h).
*/
{
    const double perIconMB = avgIconMB();
    if (perIconMB <= 0) return rowCount();

    const qint64 availMB   = static_cast<qint64>(G::availableMemoryMB);
    const qint64 reserveMB = qMax<qint64>(1024, qint64(availMB * 0.10));
    const qint64 imgHeadMB = G::imageCacheHeadroomMB.load(std::memory_order_relaxed);
    const qint64 heldMB    = qint64(iconBytesSum.load(std::memory_order_relaxed)
                                    / (1024.0 * 1024.0));

    qint64 remainderMB = qMax<qint64>(0, availMB - reserveMB - imgHeadMB) + heldMB;
    qint64 budgetMB    = qMax<qint64>(128, qint64(remainderMB * G::jitIconCacheMemFraction));

    return int(budgetMB / perIconMB);
}

int DataModel::iconChunkFloor()
/*
    Hard minimum icon window: 3x the current visible icons (a page each side of the visible
    page) so scrolling is always smooth, with an absolute floor of 256. This OVERRIDES the
    memory budget (iconBudgetCount) — when the image cache has claimed most of memory and the
    budget would otherwise fall below this, the floor wins and the thumbnails take the memory
    they need. The image cache self-throttles in turn, because its own ceiling (memChk) is
    derived from G::availableMemoryMB, which drops as the thumbnails are held. visibleIcons is
    the span across the currently visible views, maintained by MW::updateIconRange.
*/
{
    return qMax(256, 3 * visibleIcons);
}

int DataModel::memoryPressureLevel()
/*
    Cross-platform pressure level derived from G::availableMemoryMB (refreshed periodically
    by the status bar / ImageCache). 2 = critical, 1 = warn, 0 = normal. Thresholds mirror
    Mac::memoryPressureLevel so behaviour is consistent; reading the published atomic keeps
    DataModel free of mac.h/win.h and uniform across platforms.
*/
{
    // test override: force a level so Layer 3 can be validated deterministically.
    // 3 ("normal but not recovered") reports level 0 here; the release path's roomy
    // check (applyIconCachePressure) reads the override directly to hold the latch.
    if (G::iconPressureTestLevel >= 0)
        return G::iconPressureTestLevel >= 3 ? 0 : qBound(0, G::iconPressureTestLevel, 2);

    const qint64 availMB = static_cast<qint64>(G::availableMemoryMB);
    if (availMB <= 256)  return 2;
    if (availMB <= 1024) return 1;
    return 0;
}

void DataModel::applyIconCachePressure()
/*
    Layer 3: defensive, shrink-only response to memory pressure, with hysteresis.

    Under warn the icon window is halved; under critical it is clamped to the visible page
    and icons outside it are evicted immediately. The valve never grows the window — that is
    the job of Layer 1 (load-time budget) and Layer 2 (measured refinement). A latch is held
    for at least kIconPressureCooldownMs and not released until available memory recovers past
    kIconPressureClearMB, so the cache cannot oscillate between shrink and re-grow.

    Runs on the GUI thread (QTimer), so mutating iconChunkSize / setIconRange / evicting is
    safe. No-op unless G::useJitIconCache (brute force is the user's explicit "cache all").
*/
{
    if (!G::useJitIconCache) return;
    if (loadingModel || G::stop) return;        // don't mutate the model mid-load

    const int rows = rowCount();
    if (rows == 0) return;

    const int level = memoryPressureLevel();
    iconPressureLevel = level;
    const qint64 nowMs = iconPressureClock.elapsed();

    if (level >= 1) {
        const int visiblePage = qMax(visibleIcons, 64);
        const int target = (level == 2)
            ? visiblePage                                   // critical: visible page only
            : qMax(iconChunkSize / 2, visiblePage);         // warn: halve
        if (target < iconChunkSize) {
            iconChunkSize = target;
            setIconRange(currentSfRow);
            clearIconsOutsideChunkRange(instance);          // free memory now
            emit iconChunkResized();                        // re-dispatch within new range
            if (isDebug || G::isLogger)
                G::log("DataModel::applyIconCachePressure",
                       QString("level=%1 shrank iconChunkSize=%2 availMB=%3")
                           .arg(level).arg(iconChunkSize.load())
                           .arg(static_cast<qint64>(G::availableMemoryMB)));
        }
        iconCachePressureLatched = true;
        iconPressureCooldownUntil = nowMs + kIconPressureCooldownMs;
        return;
    }

    // level == 0: relax the latch only after cooldown AND memory has recovered (hysteresis).
    // Relaxing does not re-grow the window; Layer 1/2 do that on the next folder load.
    // The "recovered" gate honors the test override: only level 0 counts as recovered
    // (level 3 reports 0 here but holds the latch via roomy == false).
    const bool roomy = (G::iconPressureTestLevel >= 0)
        ? (G::iconPressureTestLevel == 0)
        : (static_cast<qint64>(G::availableMemoryMB) > kIconPressureClearMB);
    if (iconCachePressureLatched && nowMs >= iconPressureCooldownUntil && roomy) {
        iconCachePressureLatched = false;
        if (isDebug || G::isLogger)
            G::log("DataModel::applyIconCachePressure",
                   QString("latch released availMB=%1")
                       .arg(static_cast<qint64>(G::availableMemoryMB)));
    }
}

void DataModel::clearIconsOutsideChunkRange(int instance)
{
    if (instance != this->instance) return;

    if (isDebug)
    qDebug() << "DataModel::clearIconsOutsideChunkRange"
                 << "instance =" << instance
                 << "startIconRange =" << startIconRange
                 << "endIconRange =" << endIconRange
            ;

    // check if datamodel size is less than assigned icon cache chunk size
    if (iconChunkSize >= sf->rowCount()) {
        return;
    }

    if (isDebug)
    qDebug() << "DataModel::clearIconsOutsideChunkRange  cleaning..."
             << "startIconRange =" << startIconRange
             << "endIconRange =" << endIconRange
        ;

    QMutexLocker locker(&dmMutex);

    /*  Probe only (G::isPerfProbe): this runs on the GUI thread, queued from
        MetaRead::dispatchFinished, and walks every row OUTSIDE the chunk. See
        DataModel::reportScrollProbe. */
    QElapsedTimer probeTimer;
    const bool probe = G::isPerfProbe;
    qint64 cleared = 0;
    qint64 walked = 0;
    if (probe) { probeTimer.start(); probeEvictCalls++; }
    struct ProbeEvict {
        DataModel *dm; QElapsedTimer &t; qint64 &cleared; qint64 &walked; bool on;
        ~ProbeEvict() {
            if (!on) return;
            dm->probeEvictNs += t.nsecsElapsed();
            dm->probeEvictCleared += cleared;
            dm->probeEvictWalked += walked;
        }
    } probeEvict{this, probeTimer, cleared, walked, probe};

    /*  EVICTING AN ICON IS A HASH ERASE, AND IT WAS COSTING 5.3 ms.

        The two setData calls per row each emitted their own dataChanged, straight out of
        the base class -- the ONE path in the icon machinery that never adopted the
        visible-only, coalesced notification everything else uses (setIcon1,
        addMetadataForItem, setValDm/setValSf). On macOS every dataChanged makes Qt rebuild
        the entire Cocoa accessibility table for the model, ~27 ms at 43,000 rows, and this
        loop emitted two per evicted row.

        Measured by the scroll probe (Winnow --perfprobe, catalog scope, 42,956 rows):
        1,820 icons evicted in 9,650 ms, inside a 17,624 ms GUI STALL. That is the
        beachball, and it is not the walk -- the same pass over the same 43,000 rows that
        clears nothing costs under a millisecond.

        A row outside the icon chunk is by definition off-screen (the chunk contains the
        visible window), so the notification had no one to tell: the view re-reads the cell
        when it is scrolled to. iconRowVisible keeps the safe answer for the degenerate
        case where the visible range is not established yet. */
    auto evict = [&](int sfRow) {
        const QModelIndex sfIdx = sf->index(sfRow, 0);
        if (sfIdx.data(Qt::DecorationRole).isNull()) return;
        const QModelIndex dmIdx = sf->mapToSource(sfIdx);
        {
            const QSignalBlocker blocker(this);
            sf->setData(sfIdx, QVariant(), Qt::DecorationRole);
            sf->setData(sf->index(sfRow, G::IconLoadedColumn), false);
        }
        if (dmIdx.isValid() && iconRowVisible(dmIdx)) scheduleVisibleEmit(dmIdx.row());
        if (probe) cleared++;
    };

    /*  WALK WHAT LEFT THE CHUNK, NOT EVERYTHING OUTSIDE IT.

        This swept every row outside the range -- ~41,000 of 42,956 -- and it is emitted
        once per dispatch cycle, which during a scroll is constant. Once the per-row
        eviction cost was fixed the sweep itself became the top GUI cost measured:
        "evict calls = 222  cleared = 384  ms = 949.4" in one second, i.e. 222 walks of
        the whole model to find a few hundred icons.

        Between two sweeps the chunk has usually MOVED rather than jumped, so the only
        rows that can newly need evicting are the ones the chunk slid off: the difference
        between the previous range and this one. When the range has not moved at all there
        is nothing to do, and when it jumps clear of the old one the full sweep is right.

        A PERIODIC FULL SWEEP still runs (kEvictFullSweepEvery), because a reader can
        deliver an icon for a row that was in range when it was dispatched and out of range
        by the time it lands. Delta-only would leak those; one full sweep every 64 keeps
        the bound without putting the walk back on the hot path. */
    const int rows = sf->rowCount();

    /*  READ-AHEAD IS NOT RETENTION, and conflating them put the loader on a treadmill.

        The chunk answers "what should be FETCHED around the visible page". Eviction was
        using it to answer "what may be KEPT", so with the JIT window at 256 rows every
        icon read was dropped again almost immediately -- the probe showed it exactly:
        "icons set = 2222 ... cleared = 2222", a 1:1 ratio, second after second. Rows at
        the window's edge were read, evicted, and read again, and scrolling back a page
        re-read a page that had been in memory moments earlier.

        The two are different questions with different right answers, and the user has
        already answered both: "Cache thumbs just in time" sets the read-ahead distance,
        and "Thumbnails held in memory" (maxIconChunkOrAll) is the retention bound. So the
        window kept is the chunk grown symmetrically to the retention size -- with the
        defaults, ~870 rows either side of the fetch window -- and an icon has to fall that
        far behind before it is dropped. Reading stays cheap; going back stays instant.

        Retention is purely an eviction concern: needToRead still fetches only within the
        chunk, and iconChunkMissing still asks about the chunk. */
    const int chunkStart = startIconRange.load();
    const int chunkEnd   = qMin(endIconRange.load(), rows - 1);
    const int chunkSpan  = qMax(1, chunkEnd - chunkStart + 1);
    const int retain     = qMax(chunkSpan, maxIconChunkOrAll());
    const int margin     = (retain - chunkSpan) / 2;
    const int start = qMax(0, chunkStart - margin);
    const int end   = qMin(rows - 1, chunkEnd + margin);

    bool full = (lastEvictStart < 0 || lastEvictEnd < 0);
    if (!full && start == lastEvictStart && end == lastEvictEnd) {
        if (++evictsSinceFullSweep < kEvictFullSweepEvery) return;
        full = true;
    }
    /*  Disjoint from the last kept window: everything the last one held has left. That is
        the OLD window, not the whole model -- rows beyond it were evicted by earlier
        sweeps, and anything they missed is caught by the periodic full sweep. Walking the
        model here made a jump-scroll O(rows) again for no gain. */
    const bool disjoint = !full && (end < lastEvictStart || start > lastEvictEnd);

    QVector<QPair<int, int>> spans;
    if (full) {
        spans.append({0, start - 1});
        spans.append({end + 1, rows - 1});
        evictsSinceFullSweep = 0;
    }
    else if (disjoint) {
        spans.append({lastEvictStart, lastEvictEnd});
    }
    else {
        if (start > lastEvictStart) spans.append({lastEvictStart, start - 1});
        if (end < lastEvictEnd)     spans.append({end + 1, lastEvictEnd});
    }

    lastEvictStart = start;
    lastEvictEnd = end;

    for (const auto &span : spans) {
        const int from = qMax(0, span.first);
        const int to   = qMin(rows - 1, span.second);
        for (int i = from; i <= to; ++i) {
            if (abort) return;
            if (probe) walked++;
            evict(i);
        }
    }
}

void DataModel::reportScrollProbe(const QString &src)
{
/*
    One [PERF] line per second while the user scrolls, and nothing at all when they are
    not. Everything in it is cumulative since the last line, so the numbers are "what the
    GUI thread spent in the last second of scrolling" -- which is directly comparable with
    the [PERF] GUI STALL lines MW::armGuiStallWatchdog prints from the same run.

    See the block comment on the counters in datamodel.h for what each figure is.
*/
    if (!G::isPerfProbe) return;
    if (!probeScrollClock.isValid()) { probeScrollClock.start(); probeLastReportMs = 0; }

    const qint64 now = probeScrollClock.elapsed();
    if (probeLastReportMs >= 0 && now - probeLastReportMs < 1000) return;
    const double window = (now - probeLastReportMs) / 1000.0;
    probeLastReportMs = now;

    const qint64 rearmNs    = probeRearmNs.exchange(0, std::memory_order_relaxed);
    const qint64 rearmCalls = probeRearmCalls.exchange(0, std::memory_order_relaxed);
    const qint64 rearmRows  = probeRearmRows.exchange(0, std::memory_order_relaxed);
    const qint64 dispatched = probeDispatched.exchange(0, std::memory_order_relaxed);
    const qint64 redos      = probeRedos.exchange(0, std::memory_order_relaxed);

    /*  A window with no work in it is not worth a line: the report is called from every
        scroll event, and a stationary view would otherwise print zeros forever. */
    if (probeScanCalls == 0 && probeEvictCalls == 0 && rearmCalls == 0
        && probeIconsSet == 0 && probeIconsRedundant == 0) return;

    qDebug().noquote()
        << "[PERF] scroll" << QString::number(window, 'f', 1) << "s  src =" << src
        << "\n         chunk=" << iconChunkSize.load()
        << " range=" << startIconRange.load() << "-" << endIconRange.load()
        << " sfRows=" << sf->rowCount()
        << " visible=" << firstVisibleIcon << "-" << lastVisibleIcon
        << " iconsLoaded=" << iconLoadedCount.load(std::memory_order_relaxed)
        << " unloadable=" << iconUnloadableCount.load(std::memory_order_relaxed)
        << " iconChunkLoaded=" << (bool)G::iconChunkLoaded
        << "\n         scan  calls=" << probeScanCalls
        << " rows=" << probeScanRows
        << " ms=" << QString::number(probeScanNs / 1.0e6, 'f', 1)
        << " (setIconRange calls=" << probeSetRangeCalls << ")"
        << "\n         evict calls=" << probeEvictCalls
        << " walked=" << probeEvictWalked
        << " cleared=" << probeEvictCleared
        << " ms=" << QString::number(probeEvictNs / 1.0e6, 'f', 1)
        << "\n         rearm calls=" << rearmCalls
        << " rows=" << rearmRows
        << " ms=" << QString::number(rearmNs / 1.0e6, 'f', 1) << "(metaReadThread)"
        /*  THE READ PATH ITSELF. redundant is the number that matters: an icon delivered
            for a row that already had one is a file or cache read the pool did for
            nothing, and if the pool is busy doing that then a newly visible row waits
            behind it. redos says where they come from. */
        << "\n         icons set=" << probeIconsSet
        << " ms=" << QString::number(probeIconSetNs / 1.0e6, 'f', 1)
        << " redundant=" << probeIconsRedundant
        << " dispatched=" << dispatched
        << " redos=" << redos
        /*  HOW LONG A VISIBLE ROW WAITED FOR ITS REPAINT after its icon was already in
            the model. This is the gap between "the thumbnail exists" and "the user can
            see it", and nothing else in this line measures it. */
        << "\n         repaint flushes=" << probeFlushCount
        << " emit(ms)=" << QString::number(probeFlushEmitNs / 1.0e6, 'f', 1)
        << " worstWait(ms)=" << QString::number(probeFlushMaxDelayNs / 1.0e6, 'f', 1)
        << " readerQueue=" << queuedReaderEvents.load(std::memory_order_relaxed);

    /*  WHAT A THUMBNAIL CACHE HIT COST IN THIS WINDOW, per hit rather than in total.
        It lives here rather than only on the Phase2 line because that one prints from
        MetaRead::allFinished -- once per load -- so the only way to see the cache under
        an IDLE gui thread (scrolling, not filtering) was to have no line at all. The
        counters are drained, so each report is that window's own hits. */
    /*  Read against a baseline rather than drained: the Phase2 icons line reports the
        same counters for the whole load, and zeroing them here would silently empty it. */
    auto since_ = [](std::atomic<qint64> &a, qint64 &last) {
        const qint64 now = a.load(std::memory_order_relaxed);
        const qint64 d = now - last;
        last = now;
        return d;
    };
    auto sinceInt_ = [](std::atomic<int> &a, qint64 &last) {
        const qint64 now = a.load(std::memory_order_relaxed);
        const qint64 d = now - last;
        last = now;
        return d;
    };
    const qint64 hits = sinceInt_(G::probeIconCacheHits, probeLastCacheHits);
    if (hits > 0) {
        const qint64 statNs   = since_(G::probeThumbStatNs, probeLastThumbStatNs);
        const qint64 lockNs   = since_(G::probeThumbLockNs, probeLastThumbLockNs);
        const qint64 sqlNs    = since_(G::probeThumbSqlNs, probeLastThumbSqlNs);
        const qint64 decodeNs = since_(G::probeThumbDecodeNs, probeLastThumbDecodeNs);
        auto us_ = [hits](qint64 ns) {
            return QString::number(ns / 1000.0 / hits, 'f', 1);
        };
        qDebug().noquote()
            << "         cache hits=" << hits << " per hit(us):"
            << " stat=" << us_(statNs)
            << " lockWait=" << us_(lockNs)
            << " sql=" << us_(sqlNs)
            << " decode=" << us_(decodeNs)
            << " lruStamps=" << sinceInt_(G::probeThumbStamps, probeLastThumbStamps);
    }

    probeScanNs = probeScanCalls = probeScanRows = 0;
    probeEvictNs = probeEvictCalls = probeEvictCleared = probeEvictWalked = 0;
    probeSetRangeCalls = 0;
    probeIconsSet = probeIconsRedundant = 0;
    probeIconSetNs = 0;
    probeFlushCount = 0;
    probeFlushMaxDelayNs = 0;
    probeFlushEmitNs = 0;
}

void DataModel::setCached(int sfRow, bool isCached, int instance)
{
    // Do not use mutex here 2025-02-22
    QString src = "DataModel::setCached";
    QModelIndex sfIdx = sf->index(sfRow, G::IsCachedColumn);
    if (instance != this->instance) {
        errMsg = "Instance clash from " + src;
        G::issueDedup("Comment", errMsg, src, sfIdx.row());

        /*
        qDebug() << src << sfRow << errMsg
                 << "ImageCache instance =" << instance
                 << "DataModel instance =" << this->instance
            ;//*/
        return;
    }
    if (!sfIdx.isValid()) {
        errMsg = "Invalid sfIdx.  Src: " + src;
        G::issue("Warning", errMsg, src, sfIdx.row());
        // qDebug() << src << sfRow << "isCached =" << isCached << errMsg;
        return;
    }
    sf->setData(sfIdx, isCached);
    QString fPath = sf->index(sfRow,0).data(G::PathRole).toString();
    emit refreshViewsOnCacheChange(fPath, isCached, src);
}

void DataModel::setAllMetadataAttempted(bool isAttempted)
{
    if (isDebug)
    qDebug() << "DataModel::setAllMetadataAttempted" << "instance =" << instance;
    G::allMetadataAttempted = isAttempted;
}

bool DataModel::isMetadataAttempted(int sfRow)
{
    if (isDebug) qDebug() << "DataModel::isMetadataAttempted" << "instance =" << instance << folderPathFromProxyRow(sfRow);
    return index(sfRow, G::MetadataStatusColumn).data().toInt() != G::MetaNotAttempted;
}

bool DataModel::isMetadataLoaded(int sfRow)
{
    if (isDebug) qDebug() << "DataModel::isMetadataLoaded" << "instance =" << instance << folderPathFromProxyRow(sfRow);
    return index(sfRow, G::MetadataStatusColumn).data().toInt() == G::MetaLoaded;
}

bool DataModel::isMetaReadFinished()
{
    if (isDebug)
        qDebug() << "DataModel::isMetaReadFinished" << "instance =" << instance;
    // O(1): metadataAttemptedCount is maintained in setData (status-column
    // transitions into/out of NotAttempted) and resynced by recountLoadFlags()
    // after removals, so it equals rowCount() exactly when every row has been
    // attempted. This used to be a full-model scan run once per row in
    // addMetadataForItem — O(N²).
    return metadataAttemptedCount.load(std::memory_order_relaxed) >= rowCount();
}

bool DataModel::isAllMetadataLoaded()
{
    if (isDebug) qDebug() << "DataModel::isAllMetadataLoaded" << "instance =" << instance;
    // O(1): metadataLoadedCount is maintained in setData alongside the
    // attempted count, so it equals rowCount() exactly when every row loaded
    // successfully. Differs from isMetaReadFinished() in that attempted-but-
    // failed rows do not count.
    return metadataLoadedCount.load(std::memory_order_relaxed) >= rowCount();
}

bool DataModel::metaReadHadFailure()
{
    if (isDebug) qDebug() << "DataModel::metaReadHadFailure" << "instance =" << instance;
    // O(1): a failure is a row that was attempted but did not load.
    return metadataLoadedCount.load(std::memory_order_relaxed) <
           metadataAttemptedCount.load(std::memory_order_relaxed);
}

QList<int> DataModel::failedMetadataRows()
{
    if (isDebug) qDebug() << "DataModel::failedMetadataRows" << "instance =" << instance;
    QList<int> rows;
    for (int row = 0; row < rowCount(); ++row) {
        if (index(row, G::MetadataStatusColumn).data().toInt() == G::MetaFailed) {
            if (isDebug)
            qDebug() << "DataModel::failedMetadataRows" << "row =" << row << "failed.";
            rows << row;
        }
    }
    return rows;
}

bool DataModel::isFolderLoaded(const QString &folderPath) const
{
/*
    Read-only, GUI thread, unlocked -- the same access the direct folderList readers this
    replaced were already doing. folderList is only ever appended to by addFolder, or
    cleared on a new load, both on the GUI thread.
*/
    if (folderPath.isEmpty()) return false;
    if (folderSet.contains(folderPath)) return true;    // exact spelling: the usual case
    const QString want = QDir::cleanPath(folderPath);
    for (const QString &f : folderList)
        if (QDir::cleanPath(f) == want) return true;
    return false;
}

bool DataModel::isPath(QString fPath)
{
    if (G::isLogger) G::log("DataModel::isPath");
    for (int row = 0; row < rowCount(); ++row) {
        if (fPath == index(row, 0).data(G::PathRole).toString())
            return true;
    }
    return false;
}

// fPathRow methods for concurrent access

bool DataModel::fPathRowContains(const QString &path)
{
    QReadLocker locker(&fPathRowLock);
    return fPathRow.contains(path);
}

int DataModel::fPathRowValue(const QString &path)
{
    QReadLocker locker(&fPathRowLock);
    return fPathRow[path];
}

void DataModel::fPathRowSet(const QString &path, const int row)
{
    QWriteLocker locker(&fPathRowLock);
    fPathRow[path] = row;
}

void DataModel::fPathRowRemove(const QString &path)
{
    QWriteLocker locker(&fPathRowLock);
    fPathRow.remove(path);
}

void DataModel::fPathRowClear()
{
    QWriteLocker locker(&fPathRowLock);
    fPathRow.clear();
}

bool DataModel::fPathRawInfoGet(const QString &path, RawSensorInfo &info)
{
    QReadLocker locker(&fPathRawInfoLock);
    auto it = fPathRawInfo.constFind(path);
    if (it == fPathRawInfo.constEnd()) return false;
    info = it.value();
    return true;
}

void DataModel::fPathRawInfoSet(const QString &path, const RawSensorInfo &info)
{
    QWriteLocker locker(&fPathRawInfoLock);
    fPathRawInfo.insert(path, info);
}

void DataModel::fPathRawInfoClear()
{
    QWriteLocker locker(&fPathRawInfoLock);
    fPathRawInfo.clear();
}

int DataModel::rowFromPath(QString fPath)
{
    if (isDebug) qDebug() << "DataModel::rowFromPath" << "instance =" << instance << fPath;
    if (G::isLogger) G::log("DataModel::rowFromPath");
    if (fPathRowContains(fPath)) return fPathRowValue(fPath);
    else return -1;
}

int DataModel::proxyRowFromPath(QString fPath, QString src)
{
    if (G::isLogger) G::log("DataModel::proxyRowFromPath", "scr = " + src);
    if (isDebug)
        qDebug() << "DataModel::proxyRowFromPath" << "instance =" << instance
                 << fPath;
    QMutexLocker locker(&dmMutex);
    int dmRow;
    int sfRow = -1;
    if (fPathRowContains(fPath)) {
        dmRow = fPathRowValue(fPath);
        QModelIndex sfIdx = sf->mapFromSource(index(dmRow, 0));
        if (sfIdx.isValid()) sfRow = sfIdx.row();
    }
    return sfRow;
}

QString DataModel::pathFromProxyRow(int sfRow)
{
    if (G::isLogger) G::log("DataModel::proxyRowFromPath");
    return sf->index(sfRow,0).data(G::PathRole).toString();
}

QString DataModel::folderPathFromProxyRow(int sfRow)
{
    if (G::isLogger) G::log("DataModel::proxyRowFromPath");
    QString fPath = sf->index(sfRow,0).data(G::PathRole).toString();
    return QDir(fPath).absolutePath();
}

QString DataModel::folderPathFromModelRow(int dmRow)
{
    if (G::isLogger) G::log("DataModel::proxyRowFromPath");
    QString fPath = index(dmRow,0).data(G::PathRole).toString();
    return QDir(fPath).absolutePath();
}

void DataModel::rebuildRowFromPathHash()
{
    if (G::isLogger) G::log("DataModel::refreshRowFromPath");
    if (isDebug) qDebug() << "DataModel::refreshRowFromPath" << "instance =" << instance;
    QMutexLocker locker(&dmMutex);
    // fPathRow.clear();
    fPathRowClear();
    for (int row = 0; row < rowCount(); ++row) {
        QString fPath = index(row, G::PathColumn).data(G::PathRole).toString();
        // fPathRow[fPath] = row;
        fPathRowSet(fPath, row);
    }
}

bool DataModel::sourceModified(QStringList &added, QStringList &removed, QStringList &modified)
{
/*
    Determine if the eligible file count has changed
    and/or any images have been modified. If a file has been modified since the datamodel
    was loaded then it is added to the modifiedFiles list.

    Called from MW::refreshDataModel.
*/
    if (G::isLogger) G::log("DataModel::sourceModified");
    if (isDebug)
        qDebug() << "DataModel::sourceModified"
                 << "instance =" << instance
                 << folderList;

    bool hasChanged = false;
    QStringList srcImageFiles;

    // added
    foreach(QString folderPath, folderList) {
        // populate srcImageFiles
        QDir d;
        d.setPath(folderPath);
        d.setNameFilters(*fileFilters);
        d.setFilter(QDir::Files);
        foreach(QFileInfo info, d.entryInfoList()) {
            QString fPath = info.filePath();
            srcImageFiles << fPath;
            // in datamodel?
            if (!fPathRowContains(fPath)) {
                added << fPath;
            }
        }
    }

    // removed
    for (auto i = fPathRow.begin(), end = fPathRow.end(); i != end; ++i) {
        QString fPath = i.key();
        if (!srcImageFiles.contains(fPath)) {
            removed << fPath;
        }
    }

    // modified
    for (int i = 0; i < rowCount(); ++i) {
        QDateTime t1 = index(i, G::ModifiedColumn).data().toDateTime();
        // get current
        QString fPath = index(i, G::PathColumn).data(G::PathRole).toString();
        QFileInfo info = QFileInfo(fPath);
        QDateTime t2 = info.lastModified();
        // many file formats to not include ms in datetime
        if (t1.msecsTo(t2) > 1000) {
            modified << fPath;
        }
    }

    if (added.count() || removed.count() || modified.count()) hasChanged = true;

    return hasChanged;
}

void DataModel::searchStringChange(QString searchString)
{
/*
    When the search string in filters is edited a signal is emitted to run this function.
    Where there is a match G::SearchColumn is set to true, otherwise false. Update the
    filtered and unfiltered counts.
*/
    if (G::isLogger) G::log("DataModel::searchStringChange");
    if (isDebug)
         qDebug() << "DataModel::searchStringChange" << "instance =" << instance
                  << "searchString =" << searchString;

    /* Whitespace-only text parses to no terms, which would match every row and light the
       Search filter up for a query that asked for nothing. Treated as no search, like the
       empty string beside it. */
    bool noSearch = filters->ignoreSearchStrings.contains(searchString);
    if (!noSearch && SearchTerms::parse(searchString).isEmpty()) noSearch = true;

    /* PARSED ONCE, ABOVE THE LOOP. This runs over every row in the model on the GUI
       thread while the user is typing, so re-parsing per row would put the tokenizer on
       the critical path of a keystroke at a hundred thousand images.

       Utilities/searchterms.h is the SAME grammar the catalog search uses, which is what
       makes F2 ("here") and Shift+F2 ("everywhere") narrow the same way -- "heron OR
       eagle" used to find images in one and nothing in the other. */
    const SearchTerms terms = noSearch ? SearchTerms() : SearchTerms::parse(searchString);

    // update datamodel search string match
    QMutexLocker locker(&dmMutex);
    for (int row = 0; row < rowCount(); ++row)  {
        // no search string
        if (noSearch) {
            setData(index(row, G::SearchColumn), false);
            filters->searchTrue->setText(0, filters->enterSearchString);
        }
        // there is a search string
        else {
            QString searchableText = index(row, G::SearchTextColumn).data().toString();
            setData(index(row, G::SearchColumn), terms.matches(searchableText));
        }
    }
}

void DataModel::rebuildTypeFilter()
{
/*
    When Raw+Jpg is toggled in the main program MW the file type filter must be rebuilt.
*/
    if (G::isLogger) G::log("DataModel::rebuildTypeFilter");
    if (isDebug) qDebug() << "DataModel::rebuildTypeFilter" << "instance =" << instance;
    QStringList typeList;
    QMap<QString,int> map;
    for (int row = 0; row < rowCount(); row++) {
        map[index(row, G::TypeColumn).data().toString()]++;
    }
    filters->updateCategoryItems(map, filters->types);
}

//void DataModel::rebuildTypeFilter()
//{
//    /*
//    When Raw+Jpg is toggled in the main program MW the file type filter must
//    be rebuilt.
//*/
//    if (G::isLogger) G::log("DataModel::rebuildTypeFilter");
//    filters->types->takeChildren();
//    QMap<QString, QString> typesMap;
//    int rows = sf->rowCount();
//    for(int row = 0; row < rows; row++) {
//        QString type = sf->index(row, G::TypeColumn).data().toString();
//        if (!typesMap.contains(type)) {
//            typesMap[type] = type;
//        }
//    }
//    filters->addCategoryFromData(typesMap, filters->types);
//}

QModelIndex DataModel::indexFromPath(QString fPath)
{
/*
    The hash table fPathRow {path, row} is build when the datamodel is loaded to provide a
    quick lookup to get the datamodel row from an image path.
*/
    if (G::isLogger) G::log("DataModel::proxyIndexFromPath");
    if (isDebug) {
        qDebug() << "DataModel::proxyIndexFromPath" << "instance =" << instance
                 << "fPath =" << fPath;
    }
    // if (!fPathRow.contains(fPath)) {
    if (!fPathRowContains(fPath)) {
        errMsg = "Not in fPathrow.";
        G::issue("Warning", errMsg, "DataModel::proxyIndexFromPath", -1, fPath);
        if (G::isRunByExtern) Utilities::log("MW::proxyIndexFromPath", "Not in fPathrow: " + fPath);
        return index(-1, -1);
    }
    // int dmRow = fPathRow[fPath];
    int dmRow = fPathRowValue(fPath);
    QModelIndex dmIdx = index(dmRow, 0);
    if (dmIdx.isValid()) {
        return dmIdx;
    }
    else {
        errMsg = "Invalid index.";
        G::issue("Warning", errMsg, "DataModel::indexFromPath", dmRow, fPath);
        return index(-1, -1);       // invalid index
    }
}

QModelIndex DataModel::proxyIndexFromPath(QString fPath)
{
    /*
    The hash table fPathRow {path, row} is build when the datamodel is loaded to provide a
    quick lookup to get the datamodel row from an image path.
*/
    if (G::isLogger) G::log("DataModel::proxyIndexFromPath");
    if (isDebug) {
        qDebug() << "DataModel::proxyIndexFromPath" << "instance =" << instance
                 << "fPath =" << fPath;
    }
    // if (!fPathRow.contains(fPath)) {
    if (!fPathRowContains(fPath)) {
        errMsg = "Not in fPathrow.";
        G::issue("Warning", errMsg, "DataModel::proxyIndexFromPath", -1, fPath);
        if (G::isRunByExtern) Utilities::log("MW::proxyIndexFromPath", "Not in fPathrow: " + fPath);
        return index(-1, -1);
    }
    // int dmRow = fPathRow[fPath];
    int dmRow = fPathRowValue(fPath);
    QModelIndex sfIdx = sf->mapFromSource(index(dmRow, 0));
    if (sfIdx.isValid()) {
        return sfIdx;
    }
    else {
        errMsg = "Invalid proxy.";
        G::issue("Warning", errMsg, "DataModel::proxyIndexFromPath", dmRow, fPath);
        return index(-1, -1);       // invalid index
    }
}

QModelIndex DataModel::proxyIndexFromModelIndex(QModelIndex dmIdx)
{
    QMutexLocker locker(&dmMutex);
    if (dmIdx.isValid()) return sf->mapFromSource(dmIdx);
    else return QModelIndex();
}

int DataModel::proxyRowFromModelRow(int dmRow)
{
    if (G::isLogger) G::log("DataModel::proxyRowFromModelRow");
    if (isDebug) qDebug() << "DataModel::proxyRowFromModelRow" << "instance =" << instance
                          << "row =" << dmRow
                          << folderPathFromProxyRow(dmRow);
    return sf->mapFromSource(index(dmRow, 0)).row();
}

int DataModel::modelRowFromProxyRow(int sfRow)
{
    if (G::isLogger) G::log("DataModel::modelRowFromProxyRow");
    if (isDebug) qDebug() << "DataModel::modelRowFromProxyRow" << "instance =" << instance
                          << "row =" << sfRow
                          << folderPathFromProxyRow(sfRow);
    return sf->mapToSource(sf->index(sfRow, 0)).row();
}

QModelIndex DataModel::modelIndexFromProxyIndex(QModelIndex sfIdx)
{
    if (G::isLogger) G::log("DataModel::modelIndexFromProxyIndex");
    return sf->mapToSource(sfIdx);
}

int DataModel::nearestProxyRowFromDmRow(int dmRow)
{
    // does proxy contain dmRow
    QModelIndex dmIdx = index(dmRow, 0);
    QModelIndex sfIdx = sf->mapFromSource(dmIdx);
    if (sfIdx.isValid()) return sfIdx.row();

    // find nearest
    for (int i = 0; i < sf->rowCount(); i++) {
        // backward
        if (dmRow - i >= 0) {
            dmIdx = index(dmRow - i, 0);
        sfIdx = sf->mapFromSource(dmIdx);
            if (sfIdx.isValid()) {
                    return sfIdx.row();
            }
        }
        // ahead
        if (dmRow + i < sf->rowCount()) {
            dmIdx = index(dmRow + i, 0);
            sfIdx = sf->mapFromSource(dmIdx);
            if (sfIdx.isValid()) {
                    return sfIdx.row();
            }
        }
    }
    return -1;
}

void DataModel::saveSelection()
{
    if (G::isLogger) G::log("DataModel::saveSelection");
    savedSelection = selectionModel->selection();
}

void DataModel::restoreSelection()
{
    if (G::isLogger) G::log("DataModel::restoreSelection");
    selectionModel->select(savedSelection, QItemSelectionModel::Select);
}

bool DataModel::isSelected(int row)
{
/*
    req'd by IconViewDelegate (does not connect to Selection class)
*/
//    if (G::isLogger) G::log("DataModel::isSelected");
    return selectionModel->isSelected(sf->index(row, 0));
}

int DataModel::nextPick()
{
    if (G::isLogger) G::log("DataModel::nextPick");
    int frwd = currentSfRow + 1;
    int rowCount = sf->rowCount();
    QModelIndex idx;
    while (frwd < rowCount) {
        idx = sf->index(frwd, G::PickColumn);
        if (idx.data(Qt::EditRole).toString() == "Picked") return frwd;
        ++frwd;
    }
    return -1;
}

int DataModel::prevPick()
{
    if (G::isLogger) G::log("DataModel:prevPick");
    int back = currentSfRow - 1;
    QModelIndex idx;
    while (back >= 0) {
        idx = sf->index(back, G::PickColumn);
        if (idx.data(Qt::EditRole).toString() == "Picked") return back;
        --back;
    }
    return -1;
}

int DataModel::nearestPick()    // not used
{
    if (G::isLogger) G::log("DataModel:nearestPick");
    int frwd = currentSfRow;
    int back = frwd;
    int rowCount = sf->rowCount();
    QModelIndex idx;
    while (back >=0 || frwd < rowCount) {
        if (back >=0) idx = sf->index(back, G::PickColumn);
        if (idx.data(Qt::EditRole).toString() == "true") return back;
        if (frwd < rowCount) idx = sf->index(frwd, G::PickColumn);
        if (idx.data(Qt::EditRole).toString() == "true") return frwd;
        --back;
        ++frwd;
    }
    return 0;
}

//QModelIndex DataModel::nearestFiltered(QModelIndex dmIdx)
//{
//    for (int i = dmIdx.row(); i >= 0; i--) {

//    }
//    return nullptr;
//}

bool DataModel::getSelectionOrPicks(QStringList &list)
{
/*
    Adds each image that is selected or picked as a file path to list. If there
    are picks and a selection then a dialog offers the user a choice to use.
*/
    if (G::isLogger) G::log("DataModel::getSelection");
    if (isDebug)
        qDebug() << "DataModel::getSelection"
                 << "instance =" << instance
                 << "isAnyPick() =" << isAnyPick()
                 << "selectionModel->selectedRows().size() =" << selectionModel->selectedRows().size()
            ;

    bool usePicks = false;

    // nothing picked and nothing selected
    if (!isAnyPick() && selectionModel->selectedRows().size() == 0) {
        G::popup->showPopup("Oops.  There are no picks or selected images.", 2000);
        return false;
    }

    // picked != selected then choose which to use
    if (isAnyPick()) {
        for (int row = 0; row < sf->rowCount(); row++) {
            bool isPicked = sf->index(row, G::PickColumn).data(Qt::EditRole).toString() == "Picked";
            bool isSelected = selectionModel->isSelected(sf->index(row, 0));
            if (isPicked != isSelected) {
                SelectionOrPicksDlg::Option option;
                SelectionOrPicksDlg dlg(option);
                dlg.exec();
                if (option == SelectionOrPicksDlg::Option::Cancel) return false;
                if (option == SelectionOrPicksDlg::Option::Picks) usePicks = true;
                break;
            }
        }
    }

    if (usePicks) {
        for (int row = 0; row < sf->rowCount(); row++) {
            if (sf->index(row, G::PickColumn).data(Qt::EditRole).toString() == "Picked") {
                QModelIndex idx = sf->index(row, 0);
                list << idx.data(G::PathRole).toString();
            }
        }
    }
    else {
        QModelIndexList idxList = selectionModel->selectedRows();
        for (int i = 0; i < idxList.size(); ++i) {
            int row = idxList.at(i).row();
            QModelIndex idx = sf->index(row, 0);
            list << idx.data(G::PathRole).toString();
        }
    }

    return true;
}

bool DataModel::isAnyPick()
{
/*
    Returns true if any row is picked.
*/
    if (G::isLogger) G::log("DataModel::isPick");
    if (isDebug) qDebug() << "DataModel::isPick" << "instance =" << instance;
    for (int row = 0; row < rowCount(); ++row) {
        QModelIndex idx = index(row, G::PickColumn);
        if (idx.data(Qt::EditRole).toString() == "Picked") return true;
    }
    return false;
}

void DataModel::clearPicks()
{
/*
    reset all the picks to false.
*/
    if (G::isLogger) G::log("DataModel::clearPicks");
    if (isDebug) qDebug() << "DataModel::clearPicks" << "instance =" << instance;
    QMutexLocker locker(&dmMutex);
    for (int row = 0; row < sf->rowCount(); row++) {
        setData(index(row, G::PickColumn), "false");
    }
}

int DataModel::recurseImageCount(QString &parentFolder)
{
    if (G::isLogger) G::log("DataModel::recurseImageCount");

    qint64 count = 0;
    for (auto it = folderImageCount.constBegin(),
         end = folderImageCount.constEnd();
         it != end; ++it)
    {
        if (it.key().startsWith(parentFolder)) {
            count += it.value();
        }
    }
    return count;
}

void DataModel::setThumbnailLegend()
{
    QString yellow = "<font color=\"yellow\">Yellow </font>";
    QString white = "<font color=\"white\">White </font>";
    QString green = "<font color=\"green\">Green </font>";
    QString blue = "<font color=\"blue\">Blue </font>";
    QString red = "<font color=\"red\">Red </font>";
    QString redMedBullet = "<font color=\"red\"><b>●</b></font>";
    QString yellowMedBullet = "<font color=\"yellow\"><b>●</b></font>";
    QString lockSym = "🔒";
    QString header = "<p>THUMBNAIL LEGEND:<table>";
    QString rowa = "<tr><td align=\"right\">" + yellow + "</td><td>border = Primary selected</td></tr>";
    QString rowb = "<tr><td align=\"right\">" + white + "</td><td>border = Other selected</td></tr>";
    QString rowc = "<tr><td align=\"right\">" + green + "</td><td>border = Pickedd</td></tr>";
    QString rowd = "<tr><td align=\"right\">" + blue + "</td><td>border = Ingested</td></tr>";
    QString rowe = "<tr><td align=\"right\">" + red + "</td><td>border = Rejected</td></tr>";
    QString row1 = "<tr><td><center>" + redMedBullet    + "</center></td><td>Full size image not cached</td></tr>";
    // QString row2 = "<tr><td><center>" + yellowMedBullet + "</center></td><td>Missing embedded thumbnail</td></tr>";
    QString row3 = "<tr><td><center>" + lockSym         + "</center></td><td>File is locked</td></tr>";
    QString endTable = "</table>";
    QString footnote = "<p>Show/hide this tooltip legend in Preferences > User Interface";
    thumbnailHelp =
            header +
            rowa +
            rowb +
            rowc +
            rowd +
            rowe +
            row1 +
            // row2 +
            row3 +
            endTable +
            footnote
            ;
}

void DataModel::setShowThumbNailSymbolHelp(bool showHelp)
{
    if (G::isLogger) G::log("DataModel::setShowThumbNailSymbolHelp");
    showThumbNailSymbolHelp = showHelp;
    // refresh datamodel
    for (int row = 0; row < rowCount(); row++) {
        QModelIndex dmIdx = index(row, G::PathColumn);
        QString fPath = dmIdx.data(G::PathRole).toString();
        QString tip = fPath;  //fileInfo.absoluteFilePath();
        if (showThumbNailSymbolHelp) tip += thumbnailHelp;
        // setData(dmIdx, tip, Qt::ToolTipRole);
    }
}

QString DataModel::diagnosticsAllRows()
{
    if (G::isLogger) G::log("DataModel::diagnostics");
    QString reportString;
    QTextStream rpt;
    rpt.setString(&reportString);
    rpt << diagnostics();
    for (int row = 0; row < rowCount(); row++) {
        rpt << "\n";
        getDiagnosticsForRow(row, rpt);
    }

    return reportString;
}

QString DataModel::diagnostics()
{
    if (G::isLogger) G::log("DataModel::diagnostics");
    if (isDebug) qDebug() << "DataModel::diagnostics" << "instance =" << instance;
    QString reportString;
    QTextStream rpt;
    int dots = 30;
    rpt.setString(&reportString);
    rpt << Utilities::centeredRptHdr('=', "DataModel Diagnostics");
    rpt << "\n\n";

    // Health checks first so anomalies surface before the scalar block.
    rpt << reportHealthChecks();

    // Cache totals once where useful below.
    const int     dmRows      = rowCount();
    const int     sfRows      = sf->rowCount();
    const int     fPathRowN   = fPathRow.size();
    const int     queued      = queuedReaderEvents.load();
    int           folderImgSum = 0;
    for (auto it = folderImageCount.cbegin(); it != folderImageCount.cend(); ++it)
        folderImgSum += it.value();

    rpt << "\n" << G::sj("abort", dots) << G::s(abort);
    rpt << "\n" << G::sj("instance", dots) << G::s(instance.load());
    rpt << "\n" << G::sj("G::dmInstance", dots) << G::s(G::dmInstance.load());
    rpt << "\n" << G::sj("instanceParent.isValid", dots) << G::s(instanceParent.isValid());
    rpt << "\n" << G::sj("primaryFolderPath", dots) << G::s(primaryFolderPath());
    rpt << "\n" << G::sj("firstFolderPathWithImages", dots) << G::s(firstFolderPathWithImages);
    rpt << "\n";
    rpt << "\n" << G::sj("dmRowCount", dots) << Utilities::fitNumber(static_cast<qint64>(dmRows), 14);
    rpt << "\n" << G::sj("sfRowCount", dots) << Utilities::fitNumber(static_cast<qint64>(sfRows), 14);
    rpt << "\n" << G::sj("fPathRow.size", dots) << Utilities::fitNumber(static_cast<qint64>(fPathRowN), 14);
    rpt << "\n" << G::sj("iconCount", dots) << Utilities::fitNumber(static_cast<qint64>(iconCount()), 14);
    rpt << "\n" << G::sj("queuedReaderEvents", dots) << Utilities::fitNumber(static_cast<qint64>(queued), 14);
    rpt << "\n";
    rpt << "\n" << G::sj("combineRawJpg", dots) << G::s(combineRawJpg);
    rpt << "\n";
    rpt << "\n" << G::sj("currentFilePath", dots) << G::s(currentFilePath);
    rpt << "\n" << G::sj("currentDmRow", dots) << G::s(currentDmRow);
    rpt << "\n" << G::sj("currentSfRow", dots) << G::s(currentSfRow);
    rpt << "\n" << G::sj("currentDmIdx.row", dots) << G::s(currentDmIdx.row());
    rpt << "\n" << G::sj("currentSfIdx.row", dots) << G::s(currentSfIdx.row());
    rpt << "\n";
    rpt << "\n" << G::sj("firstVisibleIcon", dots) << G::s(firstVisibleIcon);
    rpt << "\n" << G::sj("lastVisibleIcon", dots) << G::s(lastVisibleIcon);
    rpt << "\n" << G::sj("visibleIcons", dots) << G::s(visibleIcons);
    rpt << "\n" << G::sj("startIconRange", dots) << G::s(startIconRange.load());
    rpt << "\n" << G::sj("endIconRange", dots) << G::s(endIconRange.load());
    rpt << "\n" << G::sj("iconChunkSize", dots) << Utilities::fitNumber(static_cast<qint64>(iconChunkSize), 14);
    rpt << "\n" << G::sj("useJitIconCache", dots) << G::s(G::useJitIconCache);
    rpt << "\n" << G::sj("icon cache mode", dots)
        << (G::useJitIconCache && iconChunkSize < rowCount() ? "JIT window" : "brute force (full)");
    rpt << "\n" << G::sj("icon avg footprint", dots)
        << QString::number(avgIconMB() * 1024, 'f', 1) << " KB"
        << (iconSamples.load() > 0
            ? QString(" (measured, n=%1)").arg(iconSamples.load())
            : QString(" (worst-case estimate)"));
    rpt << "\n" << G::sj("icon budget (icons)", dots)
        << Utilities::fitNumber(static_cast<qint64>(iconBudgetCount()), 14);
    rpt << "\n" << G::sj("icon chunk floor (3x vis)", dots)
        << Utilities::fitNumber(static_cast<qint64>(iconChunkFloor()), 14);
    rpt << "\n" << G::sj("imageCacheHeadroomMB", dots)
        << Utilities::fitNumber(G::imageCacheHeadroomMB.load(), 14);
        {
        const int lvl = memoryPressureLevel();
        rpt << "\n" << G::sj("memoryPressureLevel", dots)
            << QString(lvl == 2 ? "2 critical" : lvl == 1 ? "1 warn" : "0 normal")
            << (G::iconPressureTestLevel >= 0
                ? QString(" (test override=%1%2)").arg(G::iconPressureTestLevel)
                      .arg(G::iconPressureTestLevel == 3 ? " not-recovered" : "")
                : "");
    }
    rpt << "\n" << G::sj("iconCachePressureLatched", dots) << G::s(iconCachePressureLatched);
    /* List the rows with a loaded icon as compressed ranges, e.g. "5-22, 80-201".
       Scan the entire (unfiltered) DataModel via the source rows. */
    {
        QString ranges;
        int runStart = -1;
        const int nRows = rowCount();
        for (int row = 0; row <= nRows; ++row) {
            const bool loaded = row < nRows                 // through data(): see iconCount
                && !index(row, 0).data(Qt::DecorationRole).isNull();
            if (loaded && runStart < 0) {
                runStart = row;                 // start of a new run
            }
            else if (!loaded && runStart >= 0) {
                if (!ranges.isEmpty()) ranges += ", ";
                ranges += (row - 1 == runStart)
                    ? QString::number(runStart)
                    : QString("%1-%2").arg(runStart).arg(row - 1);
                runStart = -1;                  // end of the run
            }
        }
        if (ranges.isEmpty()) ranges = "(none)";
        rpt << "\n" << G::sj("icon loaded", dots) << ranges;
    }
    rpt << "\n" << G::sj("scrollToIcon", dots) << G::s(scrollToIcon);
    rpt << "\n" << G::sj("iconSymbolRects.size", dots) << G::s(iconSymbolRects.size());
    rpt << "\n";
    rpt << "\n" << G::sj("hasDupRawJpg", dots) << G::s(hasDupRawJpg);
    rpt << "\n" << G::sj("loadingModel", dots) << G::s(loadingModel);
    rpt << "\n" << G::sj("basicFileInfoLoaded", dots) << G::s(basicFileInfoLoaded);
    rpt << "\n" << G::sj("imageCacheWaitingForRow", dots) << G::s(imageCacheWaitingForRow);
    rpt << "\n" << G::sj("folderHasMissingEmbeddedThumb", dots) << G::s(folderHasMissingEmbeddedThumb);
    rpt << "\n" << G::sj("filtersBuilt", dots) << G::s(filters->filtersBuilt);
    rpt << "\n" << G::sj("bytesUsed", dots) << Utilities::fitNumber(bytesUsed, 18);
    rpt << "\n" << G::sj("showThumbNailSymbolHelp", dots) << G::s(showThumbNailSymbolHelp);
    rpt << "\n";
    rpt << "\n" << G::sj("subFolderTreeCount", dots) << Utilities::fitNumber(static_cast<quint64>(subFolderTreeCount), 14);
    rpt << "\n" << G::sj("subFolderTreeCounter", dots) << Utilities::fitNumber(static_cast<quint64>(subFolderTreeCounter), 14);
    rpt << "\n";
    rpt << "\n" << G::sj("raw", dots) << G::s(raw);
    rpt << "\n" << G::sj("jpg", dots) << G::s(jpg);
    rpt << "\n\n";

    // folderList / folderSet
    rpt << "folderList: " << folderList.size() << " entries"
        << "   (folderSet: " << folderSet.size() << ")\n";
    int i = 0;
    for (QString folder : folderList) {
        QString iStr = QVariant(++i).toString().rightJustified(5);
        rpt << iStr << "  " << folder << "\n";
    }
    rpt << "\n";

    // folderImageCount summary
    rpt << "folderImageCount: " << folderImageCount.size() << " folders, "
        << folderImgSum << " images total\n\n";

    // fPathRow hash — cap at 1000 entries so a pathological folder can't run away.
    QMap<int,QString> rowMap;
    rpt << "fPathRow hash: " << fPathRowN << " entries\n";
    for (auto it = fPathRow.cbegin(), end = fPathRow.cend(); it != end; ++it)
        rowMap.insert(it.value(), it.key());
    const int cap = 1000;
    const int shown = qMin(cap, rowMap.size());
    int n = 0;
    for (auto it = rowMap.cbegin(), end = rowMap.cend(); it != end && n < shown; ++it, ++n) {
        QString iStr = QString::number(it.key()).rightJustified(5);
        rpt << iStr << "  " << it.value() << "\n";
    }
    if (rowMap.size() > cap) {
        rpt << "  (+" << (rowMap.size() - cap) << " more)\n";
    }

    return reportString;
}

QString DataModel::reportHealthChecks()
{
/*
    Invariant checks printed at the top of the diagnostic. Each line prints
    [OK] or [WARN] so anomalies surface before the reader scrolls through the
    scalar state. Mirrors the same pattern used in ImageCache and MetaRead.
*/
    if (G::isLogger) G::log("DataModel::reportHealthChecks");

    QString reportString;
    QTextStream rpt;
    rpt.setString(&reportString);

    auto line = [&](const QString &ok, const QString &name, const QString &detail) {
        rpt << "[" << ok.leftJustified(4, ' ') << "] "
            << name.leftJustified(28, ' ') << ": " << detail << "\n";
    };

    rpt << "Health checks:\n";

    // 1. rowCount() vs fPathRow.size() parity.
    {
        const int n = rowCount();
        const int m = fPathRow.size();
        if (n == m) {
            line("OK", "rowCount vs fPathRow", QString("%1 rows").arg(n));
        } else {
            line("WARN", "rowCount vs fPathRow",
                 QString("rowCount=%1, fPathRow.size=%2").arg(n).arg(m));
        }
    }

    // 2. folderList vs folderSet parity.
    {
        const int n = folderList.size();
        const int m = folderSet.size();
        if (n == m) {
            line("OK", "folderList vs folderSet", QString("%1 folders").arg(n));
        } else {
            line("WARN", "folderList vs folderSet",
                 QString("folderList=%1, folderSet=%2").arg(n).arg(m));
        }
    }

    // 3. queuedReaderEvents backpressure.
    {
        const int q = queuedReaderEvents.load();
        const int threshold = 500;
        if (q > threshold) {
            line("WARN", "queuedReaderEvents",
                 QString("%1 (above threshold %2)").arg(q).arg(threshold));
        } else {
            line("OK", "queuedReaderEvents", QString::number(q));
        }
    }

    // 4. instance vs G::dmInstance.
    {
        const int dm  = instance.load();
        const int gdm = G::dmInstance.load();
        if (dm == gdm) {
            line("OK", "instance vs G::dmInstance",
                 QString("both = %1").arg(dm));
        } else {
            line("WARN", "instance vs G::dmInstance",
                 QString("instance=%1, G::dmInstance=%2").arg(dm).arg(gdm));
        }
    }

    // 5. Visible-range bounds.
    {
        const int first = firstVisibleIcon;
        const int last  = lastVisibleIcon;
        const int sfN   = sf->rowCount();
        const bool ok = (first >= 0 && first <= last && last < sfN) || sfN == 0;
        if (ok) {
            line("OK", "visible range",
                 QString("[%1..%2] of %3").arg(first).arg(last).arg(sfN));
        } else {
            line("WARN", "visible range",
                 QString("[%1..%2] vs sf->rowCount=%3").arg(first).arg(last).arg(sfN));
        }
    }

    // 6. iconChunkSize vs visibleIcons.
    if (visibleIcons > iconChunkSize) {
        line("WARN", "icon chunk vs visible",
             QString("visibleIcons=%1 > iconChunkSize=%2 — cache will thrash")
                 .arg(visibleIcons).arg(iconChunkSize.load()));
    } else {
        line("OK", "icon chunk vs visible",
             QString("visibleIcons=%1, iconChunkSize=%2")
                 .arg(visibleIcons).arg(iconChunkSize.load()));
    }

    // 7. bytesUsed sanity.
    {
        const qint64 b = bytesUsed;
        const qint64 cap = static_cast<qint64>(G::memoryAbortMB) * 1024 * 1024;
        if (b < 0) {
            line("WARN", "bytesUsed",
                 QString("negative (%1) — accounting drift").arg(b));
        } else if (b > cap) {
            line("WARN", "bytesUsed",
                 QString("%1 exceeds memoryAbortMB cap (%2 bytes)")
                     .arg(Utilities::fitNumber(b, 18))
                     .arg(Utilities::fitNumber(cap, 18)));
        } else {
            line("OK", "bytesUsed",
                 QString("%1 bytes").arg(Utilities::fitNumber(b, 18)));
        }
    }

    rpt << "\n";
    return reportString;
}

QString DataModel::diagnosticsForCurrentRow()
{
    if (G::isLogger) G::log("DataModel::diagnosticsForCurrentRow");
    if (isDebug) qDebug() << "DataModel::diagnosticsForCurrentRow" << "instance =" << instance << folderPathFromProxyRow(currentSfRow);

    if (rowCount() == 0) {
        G::popup->showPopup("Empty folder or no folder selected");
        return "";
    }

    if (currentSfRow < 0 || currentSfRow >= rowCount()) {
        G::popup->showPopup("Invalid row " + QString::number(currentSfRow));
        return "";
    }

    QString reportString;
    QTextStream rpt;
    rpt.setString(&reportString);
    rpt << Utilities::centeredRptHdr('=', "DataModel Diagnostics");
    rpt << "\n";
    rpt << "\n" << G::sj("hasDupRawJpg", 27) << G::s(hasDupRawJpg);
    getDiagnosticsForRow(currentSfRow, rpt);
    rpt << "\n\n" ;
    return reportString;
}

void DataModel::getDiagnosticsForRow(int row, QTextStream& rpt)
{
    if (G::isLogger) G::log("DataModel::getDiagnosticsForRow");
    if (isDebug) qDebug() << "DataModel::getDiagnosticsForRow" << "instance =" << instance << folderPathFromProxyRow(row);

    if (rowCount() == 0) {
        G::popup->showPopup("Empty folder or no folder selected");
        return;
    }

    if (row < 0 || row >= rowCount()) {
        G::popup->showPopup("Invalid row " + QString::number(row));
        return;
    }

    QString s = "";
    int dots = 30;
    rpt << "\n"   << G::sj("DataModel row", dots) << G::s(row);
    dots = 28;
    rpt << "\n  " << G::sj("FileName", dots) << G::s(index(row, G::NameColumn).data());
    rpt << "\n  " << G::sj("FilePath", dots) << G::s(index(row, 0).data(G::PathRole));
    rpt << "\n  " << G::sj("isIcon", dots)
        << G::s(!index(row, G::PathColumn).data(Qt::DecorationRole).isNull());
    rpt << "\n  " << G::sj("isCached", dots) << G::s(index(row, G::IsCachedColumn).data());
    rpt << "\n  " << G::sj("MetadataReadingColumn", dots) << G::s(index(row, G::MetadataReadingColumn).data());
    rpt << "\n  " << G::sj("metaStatus", dots) << G::s(index(row, G::MetadataStatusColumn).data());
    rpt << "\n  " << G::sj("isIconLoaded", dots) << G::s(index(row, G::IconLoadedColumn).data());
    rpt << "\n  " << G::sj("isReadWrite", dots) << G::s(index(row, G::ReadWriteColumn).data());
    rpt << "\n  " << G::sj("dupHideRaw", dots) << G::s(index(row, 0).data(G::DupHideRawRole));
    rpt << "\n  " << G::sj("dupRawRow", dots) << G::s(dupOtherRow(row));
    rpt << "\n  " << G::sj("dupIsJpg", dots) << G::s(index(row, 0).data(G::DupIsJpgRole));
    rpt << "\n  " << G::sj("dupRawType", dots) << G::s(index(row, 0).data(G::DupRawTypeRole));
    rpt << "\n  " << G::sj("Column", dots) << G::s(index(row, 0).data(G::ColumnRole));
    rpt << "\n  " << G::sj("isGeek", dots) << G::s(index(row, 0).data(G::GeekRole));
    rpt << "\n  " << G::sj("type", dots) << G::s(index(row, G::TypeColumn).data());
    rpt << "\n  " << G::sj("video", dots) << G::s(index(row, G::VideoColumn).data());
    rpt << "\n  " << G::sj("duration", dots) << G::s(index(row, G::DurationColumn).data());
    rpt << "\n  " << G::sj("bytes", dots) << G::s(index(row, G::ByteSizeColumn).data());
    rpt << "\n  " << G::sj("pick", dots) << G::s(index(row, G::PickColumn).data());
    rpt << "\n  " << G::sj("ingested", dots) << G::s(index(row, G::IngestedColumn).data());
    rpt << "\n  " << G::sj("label", dots) << G::s(index(row, G::LabelColumn).data());
    rpt << "\n  " << G::sj("_label", dots) << G::s(index(row, G::_LabelColumn).data());
    rpt << "\n  " << G::sj("rating", dots) << G::s(index(row, G::RatingColumn).data());
    rpt << "\n  " << G::sj("_rating", dots) << G::s(index(row, G::_RatingColumn).data());
    rpt << "\n  " << G::sj("search", dots) << G::s(index(row, G::SearchColumn).data());
    rpt << "\n  " << G::sj("modifiedDate", dots) << G::s(index(row, G::ModifiedColumn).data());
    rpt << "\n  " << G::sj("createdDate", dots) << G::s(index(row, G::CreatedColumn).data());
    rpt << "\n  " << G::sj("year", dots) << G::s(index(row, G::YearColumn).data());
    rpt << "\n  " << G::sj("day", dots) << G::s(index(row, G::DayColumn).data());
    rpt << "\n  " << G::sj("megapixels", dots) << G::s(index(row, G::MegaPixelsColumn).data());
    rpt << "\n  " << G::sj("width", dots) << G::s(index(row, G::WidthColumn).data());
    rpt << "\n  " << G::sj("height", dots) << G::s(index(row, G::HeightColumn).data());
    rpt << "\n  " << G::sj("dimensions", dots) << G::s(index(row, G::DimensionsColumn).data());
    rpt << "\n  " << G::sj("aspectRatio", dots) << G::s(index(row, G::AspectRatioColumn).data());
    rpt << "\n  " << G::sj("rotation", dots) << G::s(index(row, G::RotationColumn).data());
//    rpt << "\n  " << G::sj("_rotation", dots) << G::s(index(row, G::_RotationColumn).data());
    rpt << "\n  " << G::sj("apertureNum", dots) << G::s(index(row, G::ApertureColumn).data());
    rpt << "\n  " << G::sj("exposureTimeNum", dots) << G::s(index(row, G::ShutterspeedColumn).data());
    rpt << "\n  " << G::sj("iso", dots) << G::s(index(row, G::ISOColumn).data());
    rpt << "\n  " << G::sj("exposureCompensationNum", dots) << G::s(index(row, G::ExposureCompensationColumn).data());
    rpt << "\n  " << G::sj("make", dots) << G::s(index(row, G::CameraMakeColumn).data());
    rpt << "\n  " << G::sj("model", dots) << G::s(index(row, G::CameraModelColumn).data());
    rpt << "\n  " << G::sj("lens", dots) << G::s(index(row, G::LensColumn).data());
    rpt << "\n  " << G::sj("focalLengthNum", dots) << G::s(index(row, G::FocalLengthColumn).data());
    rpt << "\n  " << G::sj("foxusX", dots) << G::s(index(row, G::FocusXColumn).data());
    rpt << "\n  " << G::sj("foxusY", dots) << G::s(index(row, G::FocusYColumn).data());
    rpt << "\n  " << G::sj("gpsCoord", dots) << G::s(index(row, G::GPSCoordColumn).data());
    rpt << "\n  " << G::sj("keywords", dots) << G::s(index(row, G::KeywordsColumn).data());
    rpt << "\n  " << G::sj("keywordPaths", dots) << G::s(index(row, G::KeywordPathsColumn).data());
    rpt << "\n  " << G::sj("keywordsAll", dots) << G::s(index(row, G::KeywordsAllColumn).data());
    rpt << "\n  " << G::sj("title", dots) << G::s(index(row, G::TitleColumn).data());
    rpt << "\n  " << G::sj("_title", dots) << G::s(index(row, G::_TitleColumn).data());
    rpt << "\n  " << G::sj("creator", dots) << G::s(index(row, G::CreatorColumn).data());
    rpt << "\n  " << G::sj("_creator", dots) << G::s(index(row, G::_CreatorColumn).data());
    rpt << "\n  " << G::sj("copyright", dots) << G::s(index(row, G::CopyrightColumn).data());
    rpt << "\n  " << G::sj("_copyright", dots) << G::s(index(row, G::_CopyrightColumn).data());
    rpt << "\n  " << G::sj("email", dots) << G::s(index(row, G::EmailColumn).data());
    rpt << "\n  " << G::sj("_email", dots) << G::s(index(row, G::_EmailColumn).data());
    rpt << "\n  " << G::sj("url", dots) << G::s(index(row, G::UrlColumn).data());
    rpt << "\n  " << G::sj("_url", dots) << G::s(index(row, G::_UrlColumn).data());
    rpt << "\n  " << G::sj("offsetFull", dots) << G::s(index(row, G::OffsetFullColumn).data());
    rpt << "\n  " << G::sj("lengthFull", dots) << G::s(index(row, G::LengthFullColumn).data());
    rpt << "\n  " << G::sj("widthOrigPreview", dots) << G::s(index(row, G::WidthOrigPreviewColumn).data());
    rpt << "\n  " << G::sj("heightOrigPreview", dots) << G::s(index(row, G::HeightOrigPreviewColumn).data());
    rpt << "\n  " << G::sj("offsetThumb", dots) << G::s(index(row, G::OffsetThumbColumn).data());
    rpt << "\n  " << G::sj("lengthThumb", dots) << G::s(index(row, G::LengthThumbColumn).data());
    rpt << "\n  " << G::sj("isBigEndian", dots) << G::s(index(row, G::isBigEndianColumn).data());
    rpt << "\n  " << G::sj("ifd0Offset", dots) << G::s(index(row, G::ifd0OffsetColumn).data());
    rpt << "\n  " << G::sj("xmpSegmentOffset", dots) << G::s(index(row, G::XmpSegmentOffsetColumn).data());
    rpt << "\n  " << G::sj("xmpNextSegmentOffset", dots) << G::s(index(row, G::XmpSegmentLengthColumn).data());

    rpt << "\n  " << G::sj("isXmp", dots) << G::s(index(row, G::IsXMPColumn).data());
    rpt << "\n  " << G::sj("orientationOffset", dots) << G::s(index(row, G::OrientationOffsetColumn).data());
    rpt << "\n  " << G::sj("orientation", dots) << G::s(index(row, G::OrientationColumn).data());
    rpt << "\n  " << G::sj("rotationDegrees", dots) << G::s(index(row, G::RotationDegreesColumn).data());
    rpt << "\n  " << G::sj("shootingInfo", dots) << G::s(index(row, G::ShootingInfoColumn).data());
    rpt << "\n  " << G::sj("loadMsecPerMp", dots) << G::s(index(row, G::LoadMsecPerMpColumn).data());

    rpt << "\n  " << G::sj("cacheSize", dots) << G::s(index(row, G::CacheSizeColumn).data());
    rpt << "\n  " << G::sj("IsCaching", dots) << G::s(index(row, G::IsCachingColumn).data());
    rpt << "\n  " << G::sj("IsCached", dots) << G::s(index(row, G::IsCachedColumn).data());
    rpt << "\n  " << G::sj("Attempts", dots) << G::s(index(row, G::AttemptsColumn).data());
    rpt << "\n  " << G::sj("DecoderId", dots) << G::s(index(row, G::DecoderIdColumn).data());
    rpt << "\n  " << G::sj("DecoderReturnStatus", dots) << G::s(index(row, G::DecoderReturnStatusColumn).data());
    rpt << "\n  " << G::sj("DecoderErrMsg", dots) << G::s(index(row, G::DecoderErrMsgColumn).data());

    QString searchText = index(row, G::SearchTextColumn).data().toString();
    int n = 140;
    for (int i = 0; i < searchText.length(); i += n) {
        rpt << "\n  " << G::sj("searchText", dots) << searchText.mid(i, n);
    }

    rpt << "\n  ";
    rpt << Utilities::centeredRptHdr('=', "Issues");

    QStringList issues = rptIssues(row);
    QString sRow = QString::number(row);
    rpt << "\n\nIssues for row " + sRow + ": " << QString::number(issues.count());
    for (const QString &str : issues) {
        rpt << "\n   " << str;
    }
}

// --------------------------------------------------------------------------------------------
// SortFilter Class used to filter by row
// --------------------------------------------------------------------------------------------

SortFilter::SortFilter(QObject *parent, Filters *filters, bool &combineRawJpg) :
    QSortFilterProxyModel(parent),
    combineRawJpg(combineRawJpg),
    finished(true), suspendFiltering(false)
{
    if (G::isLogger) G::log("SortFilter::SortFilter");
    this->filters = filters;

    /*  Recompile on the tree's OWN signals rather than from a list of call
        sites. itemChanged covers a check state changing; the model's structural
        signals cover BuildFilters adding and removing items as metadata
        arrives. A path that mutates the tree therefore cannot forget to say so,
        which is the same reasoning rebuildProxySnapshot follows.

        COALESCED, because compiling is O(items) and these fire in BURSTS. The comment
        here used to claim the cost "lands once rather than once per item" because
        filtering is suspended during a rebuild -- but suspension stops FILTERING, not
        COMPILING, so a rebuild that added n items paid O(n) compiles over O(n) items.
        With a folder of 500 that is invisible; with 43,000 catalogued images it blocked
        the GUI for a measured 56 seconds after the rows were already in. */
    if (filters) {
        connect(filters, &QTreeWidget::itemChanged, this,
                [this](QTreeWidgetItem *, int){ scheduleCompileFilters(); });
        if (QAbstractItemModel *m = filters->model()) {
            for (auto sig : { &QAbstractItemModel::rowsInserted,
                              &QAbstractItemModel::rowsRemoved }) {
                connect(m, sig, this, [this]{ scheduleCompileFilters(); });
            }
            connect(m, &QAbstractItemModel::modelReset, this,
                    [this]{ scheduleCompileFilters(); });
        }
        compileFilters();
    }
}

void SortFilter::scheduleCompileFilters()
{
/*
    One compile for a burst of tree mutations, on the next turn of the event loop.

    The predicate is derived wholly from the tree, so compiling later gives the same
    answer as compiling n times -- it is the intermediate compiles that were waste. What
    must not happen is FILTERING against a predicate that is a turn behind, which is what
    flushPendingCompile exists for.
*/
    if (compilePending) return;
    compilePending = true;
    QTimer::singleShot(0, this, [this]{
        if (!compilePending) return;      // a flush got there first
        compilePending = false;
        compileFilters();
    });
}

void SortFilter::flushPendingCompile()
{
    if (!compilePending) return;
    compilePending = false;
    compileFilters();
}

void SortFilter::compileFilters()
{
/*
    Walk the Filters tree ONCE and turn it into the plain FilterPredicate that
    filterAcceptsRow evaluates -- see Datamodel/filterpredicate.h for why, and
    for the semantics being preserved.

    The walk is the SAME QTreeWidgetItemIterator the per-row version used, in
    the same order, so a category and its items group here exactly as they
    grouped there. GUI thread only: it reads the widget.
*/
    if (G::isLogger) G::log("SortFilter::compileFilters");
    /*  Any direct compile satisfies a coalesced one -- SortFilter::filterChange calls
        this before it invalidates, so filtering never runs against a predicate a turn
        behind the tree. */
    compilePending = false;
    if (!filters) return;

    auto fresh = std::make_shared<FilterPredicate>();

    QTreeWidgetItemIterator it(filters);
    while (*it) {
        QTreeWidgetItem *item = *it;
        if (item->parent()) {
            /*  A filter item. Items appearing before any category cannot happen
                -- every non-category item has a parent -- but if the tree were
                ever malformed they would land on column 0, which is what the
                per-row version did with its dataModelColumn initialised to 0. */
            if (fresh->categories.isEmpty()) fresh->categories.append(FilterCategory());
            FilterCategory &cat = fresh->categories.last();
            const Qt::CheckState state = item->checkState(0);
            if (state != Qt::Unchecked) {
                if (item == filters->searchTrue && item->text(0) == filters->enterSearchString) {
                    /*  A search is armed but nothing has been typed, so it
                        matched every row. It is a flag rather than an include
                        because it matches without comparing anything -- and,
                        as before, it does not make the category count as
                        filtering for the rows it does not name. */
                    cat.includeAll = true;
                }
                else if (state == Qt::PartiallyChecked) cat.excludes.append(item->data(1, Qt::EditRole));
                else                                    cat.includes.append(item->data(1, Qt::EditRole));
            }
        }
        else {
            FilterCategory cat;
            cat.column = item->data(0, G::ColumnRole).toInt();
            fresh->categories.append(cat);
        }
        ++it;
    }

    QMutexLocker lock(&mPredicateMutex);
    mPredicate = fresh;
}

FilterPredicatePtr SortFilter::filterPredicate() const
{
    QMutexLocker lock(&mPredicateMutex);
    return mPredicate;
}

bool SortFilter::filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const
{
/*
    Is this row visible? Answered against the COMPILED filters
    (Datamodel/filterpredicate.h), not by walking the Filters QTreeWidget.

    The tree walk this replaces ran once per ROW and compared every item in it,
    which is O(rows x distinct values) -- tens of millions of compares at 250k
    rows to answer a question whose inputs changed once. It also read
    (*filter)->checkState(0) while BuildFilters was adding items, which is what
    the `// crash` comment on that line was about. Both go away together: the
    inputs are compiled on the GUI thread when they change, and a row test takes
    its own reference to the result.
*/

    // Suspend?
    if (suspendFiltering) return true;

    // still loading metadata
    if (!G::allMetadataAttempted) return true;

    // Check Raw + Jpg
    if (combineRawJpg) {
        QModelIndex rawIdx = sourceModel()->index(sourceRow, 0, sourceParent);
        if (rawIdx.data(G::DupHideRawRole).toBool()) return false;
    }

    finished = false;
    const FilterPredicatePtr p = filterPredicate();
    if (!p || p->acceptsEverything()) {
        finished = true;
        return true;
    }

    const bool ok = p->accepts([&](int column) {
        return sourceModel()->index(sourceRow, column, sourceParent).data(Qt::EditRole);
    });

    finished = true;
    return ok;
}

void SortFilter::filterChange(QString src)
{
/*
    Note: required because invalidateFilter is private and cannot be called from
    another class.

    This slot is called when data changes in the filters widget.  The proxy (this)
    is invalidated, which forces an update.  This happens with every change in the
    filters widget including when the filters are being created in the filter
    widget.

    If the new folder has been loaded and there are user driven changes to the
    filtration then the image cache needs to be reloaded to match the new proxy (sf)
*/
    if (G::isLogger) G::log("SortFilter::filterChange");

    if (suspendFiltering) return;

    /*  Recompile BEFORE invalidating, or the invalidation would re-test every
        row against the previous check states. */
    /*  Suspended for the invalidate for any caller that is not already inside a wider
        guard -- MW::filterChange holds one across the whole operation, because the views
        defer their layout and pay most of the rebuild after this function has returned.
        See G::A11ySuspend. Nested guards are harmless: the inner one finds the bridge
        already off and does nothing. */
    G::A11ySuspend a11ySuspend;
    compileFilters();
    invalidateRowsFilter();
    return;

    // force wait until finished to prevent sorting/editing datamodel
    // this may be causing occasional crashes
    int waitMs = 2000;
    int ms = 0;
    bool timeIsUp = false;
    while (!finished || timeIsUp) {
        G::wait(10);
        ms += 10;
        if (ms > waitMs) {
            timeIsUp = true;
            // qDebug() << "SortFilter::filterChange  timeIsUp triggered";
        }
    }
    /*
    qDebug() << "SortFilter::filterChange" << ms
             << "finished =" << finished
             << "proxy row count =" << rowCount()
             << "src =" << src
                ; //*/
}

void SortFilter::suspend(bool suspendFiltering, QString src)
{
/*
    Sets the local suspendFiltering flag.  When true, filterAcceptsRow ignores calls.
    When false the filtering is refreshed.
*/
    QString msg = "suspendFiltering = " + QVariant(suspendFiltering).toString() +
              " src = " + src;
    if (G::isLogger) G::log("SortFilter::suspend", msg);
    // qDebug() << "SortFilter::suspend =" << suspendFiltering << "src =" << src;
    this->suspendFiltering = suspendFiltering;
}

bool SortFilter::isFinished()
{
    return finished;
}

bool SortFilter::isSuspended()
{
    return suspendFiltering;
}

void SortFilter::sort(int column, Qt::SortOrder order)
{
    /* Guard against a phantom sort column reaching the proxy from ANY path — notably
       QTableView::setSortingEnabled(true), which calls model()->sort(sortIndicatorSection(),
       ...) directly and so bypasses MW::sortChange / IconView::sortThumbs. A stale section
       (e.g. G::TotalColumns == one past the last real column) would sort by an invalid
       column: keys compare equal and a descending order silently reverses the whole model
       (the Z-A regression). Treat anything out of range as -1 (no sort = source order).
       -1 itself is the valid "clear sort" sentinel and passes through. */
    if (column >= G::TotalColumns || column < -1) {
        if (G::isPerfProbe)
            qDebug().noquote() << "[PERF] SortFilter::sort clamped invalid column"
                               << column << "->" << -1
                               << (order == Qt::DescendingOrder ? "Desc" : "Asc");
        column = -1;
    }
    QSortFilterProxyModel::sort(column, order);
}
