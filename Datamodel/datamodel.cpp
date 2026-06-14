#include "Datamodel/datamodel.h"
#include "Cache/framedecoder.h"
#include "Main/global.h"

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
G::DupRawIdxRole    Raw file index if jpg has matching raw
G::DupHideRawRole   True is there is a matching jpg file
G::DupRawTypeRole   Raw file type if jpg type with matching raw

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

                     QStandardItemModel(parent),
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

    setSortRole(Qt::EditRole);

    // must include all prior Global dataModelColumns (any order okay)
    setHorizontalHeaderItem(G::PathColumn, new QStandardItem("Icon")); horizontalHeaderItem(G::PathColumn)->setData(false, G::GeekRole);
    setHorizontalHeaderItem(G::RowNumberColumn, new QStandardItem("#")); horizontalHeaderItem(G::RowNumberColumn)->setData(false, G::GeekRole);
    setHorizontalHeaderItem(G::NameColumn, new QStandardItem("File Name")); horizontalHeaderItem(G::NameColumn)->setData(false, G::GeekRole);
    setHorizontalHeaderItem(G::FolderNameColumn, new QStandardItem("Folder Name")); horizontalHeaderItem(G::FolderNameColumn)->setData(false, G::GeekRole);
    setHorizontalHeaderItem(G::NSThumb, new QStandardItem("NS Thumb")); horizontalHeaderItem(G::NSThumb)->setData(false, G::GeekRole);
    setHorizontalHeaderItem(G::NSImage, new QStandardItem("NS Image")); horizontalHeaderItem(G::NSImage)->setData(false, G::GeekRole);
    setHorizontalHeaderItem(G::PickColumn, new QStandardItem("Pick")); horizontalHeaderItem(G::PickColumn)->setData(false, G::GeekRole);
    setHorizontalHeaderItem(G::IngestedColumn, new QStandardItem("Ingested")); horizontalHeaderItem(G::IngestedColumn)->setData(false, G::GeekRole);
    setHorizontalHeaderItem(G::LabelColumn, new QStandardItem("Colour")); horizontalHeaderItem(G::LabelColumn)->setData(false, G::GeekRole);
    setHorizontalHeaderItem(G::RatingColumn, new QStandardItem("Rating")); horizontalHeaderItem(G::RatingColumn)->setData(false, G::GeekRole);
    setHorizontalHeaderItem(G::SearchColumn, new QStandardItem("Search")); horizontalHeaderItem(G::SearchColumn)->setData(false, G::GeekRole);
    setHorizontalHeaderItem(G::TypeColumn, new QStandardItem("Type")); horizontalHeaderItem(G::TypeColumn)->setData(false, G::GeekRole);
    setHorizontalHeaderItem(G::VideoColumn, new QStandardItem("Video")); horizontalHeaderItem(G::VideoColumn)->setData(false, G::GeekRole);
    setHorizontalHeaderItem(G::SidecarColumn, new QStandardItem("Sidecar")); horizontalHeaderItem(G::SidecarColumn)->setData(false, G::GeekRole);
    setHorizontalHeaderItem(G::ApertureColumn, new QStandardItem("Aperture")); horizontalHeaderItem(G::ApertureColumn)->setData(false, G::GeekRole);
    setHorizontalHeaderItem(G::ShutterspeedColumn, new QStandardItem("Shutter")); horizontalHeaderItem(G::ShutterspeedColumn)->setData(false, G::GeekRole);
    setHorizontalHeaderItem(G::ISOColumn, new QStandardItem("ISO")); horizontalHeaderItem(G::ISOColumn)->setData(false, G::GeekRole);
    setHorizontalHeaderItem(G::ExposureCompensationColumn, new QStandardItem("  EC  ")); horizontalHeaderItem(G::ExposureCompensationColumn)->setData(false, G::GeekRole);
    setHorizontalHeaderItem(G::DurationColumn, new QStandardItem("Duration")); horizontalHeaderItem(G::DurationColumn)->setData(false, G::GeekRole);
    setHorizontalHeaderItem(G::CameraMakeColumn, new QStandardItem("Make")); horizontalHeaderItem(G::CameraMakeColumn)->setData(false, G::GeekRole);
    setHorizontalHeaderItem(G::CameraModelColumn, new QStandardItem("Model")); horizontalHeaderItem(G::CameraModelColumn)->setData(false, G::GeekRole);
    setHorizontalHeaderItem(G::LensColumn, new QStandardItem("Lens")); horizontalHeaderItem(G::LensColumn)->setData(false, G::GeekRole);
    setHorizontalHeaderItem(G::FocalLengthColumn, new QStandardItem("Focal length")); horizontalHeaderItem(G::FocalLengthColumn)->setData(false, G::GeekRole);
    setHorizontalHeaderItem(G::FocusXColumn, new QStandardItem("FocusX")); horizontalHeaderItem(G::FocusXColumn)->setData(false, G::GeekRole);
    setHorizontalHeaderItem(G::FocusYColumn, new QStandardItem("FocusY")); horizontalHeaderItem(G::FocusYColumn)->setData(false, G::GeekRole);
    setHorizontalHeaderItem(G::GPSCoordColumn, new QStandardItem("GPS Coord")); horizontalHeaderItem(G::GPSCoordColumn)->setData(false, G::GeekRole);
    setHorizontalHeaderItem(G::ByteSizeColumn, new QStandardItem("Size")); horizontalHeaderItem(G::ByteSizeColumn)->setData(false, G::GeekRole);
    setHorizontalHeaderItem(G::WidthColumn, new QStandardItem("Width")); horizontalHeaderItem(G::WidthColumn)->setData(false, G::GeekRole);
    setHorizontalHeaderItem(G::HeightColumn, new QStandardItem("Height")); horizontalHeaderItem(G::HeightColumn)->setData(false, G::GeekRole);
    setHorizontalHeaderItem(G::ModifiedColumn, new QStandardItem("Last Modified")); horizontalHeaderItem(G::ModifiedColumn)->setData(false, G::GeekRole);
    setHorizontalHeaderItem(G::CreatedColumn, new QStandardItem("Created")); horizontalHeaderItem(G::CreatedColumn)->setData(false, G::GeekRole);
    setHorizontalHeaderItem(G::YearColumn, new QStandardItem("Year")); horizontalHeaderItem(G::YearColumn)->setData(false, G::GeekRole);
    setHorizontalHeaderItem(G::DayColumn, new QStandardItem("Day")); horizontalHeaderItem(G::DayColumn)->setData(false, G::GeekRole);
    setHorizontalHeaderItem(G::CreatorColumn, new QStandardItem("Creator")); horizontalHeaderItem(G::CreatorColumn)->setData(false, G::GeekRole);
    setHorizontalHeaderItem(G::MegaPixelsColumn, new QStandardItem("MPix")); horizontalHeaderItem(G::MegaPixelsColumn)->setData(false, G::GeekRole);
    setHorizontalHeaderItem(G::LoadMsecPerMpColumn, new QStandardItem("Msec/Mp")); horizontalHeaderItem(G::LoadMsecPerMpColumn)->setData(false, G::GeekRole);
    setHorizontalHeaderItem(G::DimensionsColumn, new QStandardItem("Dimensions")); horizontalHeaderItem(G::DimensionsColumn)->setData(false, G::GeekRole);
    setHorizontalHeaderItem(G::AspectRatioColumn, new QStandardItem("Aspect Ratio")); horizontalHeaderItem(G::AspectRatioColumn)->setData(false, G::GeekRole);
    setHorizontalHeaderItem(G::IconAspectRatioColumn, new QStandardItem("Icon Aspect Ratio")); horizontalHeaderItem(G::IconAspectRatioColumn)->setData(false, G::GeekRole);
    setHorizontalHeaderItem(G::OrientationColumn, new QStandardItem("Orientation")); horizontalHeaderItem(G::OrientationColumn)->setData(false, G::GeekRole);
    setHorizontalHeaderItem(G::RotationColumn, new QStandardItem("Rot")); horizontalHeaderItem(G::RotationColumn)->setData(false, G::GeekRole);
    setHorizontalHeaderItem(G::CopyrightColumn, new QStandardItem("Copyright")); horizontalHeaderItem(G::CopyrightColumn)->setData(false, G::GeekRole);
    setHorizontalHeaderItem(G::TitleColumn, new QStandardItem("Title")); horizontalHeaderItem(G::TitleColumn)->setData(false, G::GeekRole);
    setHorizontalHeaderItem(G::EmailColumn, new QStandardItem("Email")); horizontalHeaderItem(G::EmailColumn)->setData(false, G::GeekRole);
    setHorizontalHeaderItem(G::UrlColumn, new QStandardItem("Url")); horizontalHeaderItem(G::UrlColumn)->setData(false, G::GeekRole);
    setHorizontalHeaderItem(G::KeywordsColumn, new QStandardItem("Keywords")); horizontalHeaderItem(G::KeywordsColumn)->setData(false, G::GeekRole);
    setHorizontalHeaderItem(G::MetadataReadingColumn, new QStandardItem("Meta Reading")); horizontalHeaderItem(G::MetadataReadingColumn)->setData(true, G::GeekRole);
    setHorizontalHeaderItem(G::MetadataStatusColumn, new QStandardItem("Meta Status")); horizontalHeaderItem(G::MetadataStatusColumn)->setData(true, G::GeekRole);
    setHorizontalHeaderItem(G::IconLoadedColumn, new QStandardItem("Icon Loaded")); horizontalHeaderItem(G::IconLoadedColumn)->setData(true, G::GeekRole);
    setHorizontalHeaderItem(G::CompareColumn, new QStandardItem("Compare")); horizontalHeaderItem(G::CompareColumn)->setData(true, G::GeekRole);
    setHorizontalHeaderItem(G::_RatingColumn, new QStandardItem("_Rating")); horizontalHeaderItem(G::_RatingColumn)->setData(true, G::GeekRole);
    setHorizontalHeaderItem(G::_LabelColumn, new QStandardItem("_Label")); horizontalHeaderItem(G::_LabelColumn)->setData(true, G::GeekRole);
    setHorizontalHeaderItem(G::_CreatorColumn, new QStandardItem("_Creator")); horizontalHeaderItem(G::_CreatorColumn)->setData(true, G::GeekRole);
    setHorizontalHeaderItem(G::_TitleColumn, new QStandardItem("_Title")); horizontalHeaderItem(G::_TitleColumn)->setData(true, G::GeekRole);
    setHorizontalHeaderItem(G::_CopyrightColumn, new QStandardItem("_Copyright")); horizontalHeaderItem(G::_CopyrightColumn)->setData(true, G::GeekRole);
    setHorizontalHeaderItem(G::_EmailColumn, new QStandardItem("_Email")); horizontalHeaderItem(G::_EmailColumn)->setData(true, G::GeekRole);
    setHorizontalHeaderItem(G::_UrlColumn, new QStandardItem("_Url")); horizontalHeaderItem(G::_UrlColumn)->setData(true, G::GeekRole);
    setHorizontalHeaderItem(G::PermissionsColumn, new QStandardItem("Permissions")); horizontalHeaderItem(G::PermissionsColumn)->setData(true, G::GeekRole);
    setHorizontalHeaderItem(G::ReadWriteColumn, new QStandardItem("R/W")); horizontalHeaderItem(G::ReadWriteColumn)->setData(true, G::GeekRole);
    setHorizontalHeaderItem(G::OffsetFullColumn, new QStandardItem("OffsetFull")); horizontalHeaderItem(G::OffsetFullColumn)->setData(true, G::GeekRole);
    setHorizontalHeaderItem(G::LengthFullColumn, new QStandardItem("LengthFull")); horizontalHeaderItem(G::LengthFullColumn)->setData(true, G::GeekRole);
    setHorizontalHeaderItem(G::WidthPreviewColumn, new QStandardItem("WidthPreview")); horizontalHeaderItem(G::WidthPreviewColumn)->setData(true, G::GeekRole);
    setHorizontalHeaderItem(G::HeightPreviewColumn, new QStandardItem("HeightPreview")); horizontalHeaderItem(G::HeightPreviewColumn)->setData(true, G::GeekRole);
    setHorizontalHeaderItem(G::OffsetThumbColumn, new QStandardItem("OffsetThumb")); horizontalHeaderItem(G::OffsetThumbColumn)->setData(true, G::GeekRole);
    setHorizontalHeaderItem(G::LengthThumbColumn, new QStandardItem("LengthThumb")); horizontalHeaderItem(G::LengthThumbColumn)->setData(true, G::GeekRole);
    setHorizontalHeaderItem(G::samplesPerPixelColumn, new QStandardItem("samplesPerPixelFull")); horizontalHeaderItem(G::samplesPerPixelColumn)->setData(true, G::GeekRole);
    setHorizontalHeaderItem(G::isBigEndianColumn, new QStandardItem("isBigEndian")); horizontalHeaderItem(G::isBigEndianColumn)->setData(true, G::GeekRole);
    setHorizontalHeaderItem(G::ifd0OffsetColumn, new QStandardItem("ifd0Offset")); horizontalHeaderItem(G::ifd0OffsetColumn)->setData(true, G::GeekRole);
    setHorizontalHeaderItem(G::ifdOffsetsColumn, new QStandardItem("ifd0Offsets")); horizontalHeaderItem(G::ifdOffsetsColumn)->setData(true, G::GeekRole);
    setHorizontalHeaderItem(G::XmpSegmentOffsetColumn, new QStandardItem("XmpSegmentOffset")); horizontalHeaderItem(G::XmpSegmentOffsetColumn)->setData(true, G::GeekRole);
    setHorizontalHeaderItem(G::XmpSegmentLengthColumn, new QStandardItem("XmpSegmentLengthColumn")); horizontalHeaderItem(G::XmpSegmentLengthColumn)->setData(true, G::GeekRole);
    setHorizontalHeaderItem(G::IsXMPColumn, new QStandardItem("IsXMP")); horizontalHeaderItem(G::IsXMPColumn)->setData(true, G::GeekRole);
    setHorizontalHeaderItem(G::ICCSegmentOffsetColumn, new QStandardItem("ICCSegmentOffsetColumn")); horizontalHeaderItem(G::ICCSegmentOffsetColumn)->setData(true, G::GeekRole);
    setHorizontalHeaderItem(G::ICCSegmentLengthColumn, new QStandardItem("ICCSegmentLengthColumn")); horizontalHeaderItem(G::ICCSegmentLengthColumn)->setData(true, G::GeekRole);
    setHorizontalHeaderItem(G::ICCBufColumn, new QStandardItem("ICCBuf")); horizontalHeaderItem(G::ICCBufColumn)->setData(true, G::GeekRole);
    setHorizontalHeaderItem(G::ICCSpaceColumn, new QStandardItem("ICCSpace")); horizontalHeaderItem(G::ICCSpaceColumn)->setData(true, G::GeekRole);
    setHorizontalHeaderItem(G::CacheSizeColumn, new QStandardItem("CacheSize")); horizontalHeaderItem(G::CacheSizeColumn)->setData(true, G::GeekRole);
    // setHorizontalHeaderItem(G::IsVideoColumn, new QStandardItem("IsVideo")); horizontalHeaderItem(G::IsVideoColumn)->setData(true, G::GeekRole);
    setHorizontalHeaderItem(G::IsCachingColumn, new QStandardItem("IsCaching")); horizontalHeaderItem(G::IsCachingColumn)->setData(true, G::GeekRole);
    setHorizontalHeaderItem(G::IsCachedColumn, new QStandardItem("IsCached")); horizontalHeaderItem(G::IsCachedColumn)->setData(true, G::GeekRole);
    setHorizontalHeaderItem(G::AttemptsColumn, new QStandardItem("Attempts")); horizontalHeaderItem(G::AttemptsColumn)->setData(true, G::GeekRole);
    setHorizontalHeaderItem(G::DecoderIdColumn, new QStandardItem("DecoderId")); horizontalHeaderItem(G::DecoderIdColumn)->setData(true, G::GeekRole);
    setHorizontalHeaderItem(G::DecoderReturnStatusColumn, new QStandardItem("DecoderReturnStatus")); horizontalHeaderItem(G::DecoderReturnStatusColumn)->setData(true, G::GeekRole);
    setHorizontalHeaderItem(G::DecoderErrMsgColumn, new QStandardItem("Decoder Err Msg")); horizontalHeaderItem(G::DecoderErrMsgColumn)->setData(true, G::GeekRole);
    setHorizontalHeaderItem(G::OrientationOffsetColumn, new QStandardItem("OrientationOffset")); horizontalHeaderItem(G::OrientationOffsetColumn)->setData(true, G::GeekRole);
    setHorizontalHeaderItem(G::RotationDegreesColumn, new QStandardItem("RotationDegrees")); horizontalHeaderItem(G::RotationDegreesColumn)->setData(true, G::GeekRole);
    setHorizontalHeaderItem(G::ShootingInfoColumn, new QStandardItem("ShootingInfo")); horizontalHeaderItem(G::ShootingInfoColumn)->setData(true, G::GeekRole);
    setHorizontalHeaderItem(G::SearchTextColumn, new QStandardItem("Search")); horizontalHeaderItem(G::SearchTextColumn)->setData(true, G::GeekRole);
    setHorizontalHeaderItem(G::ErrColumn, new QStandardItem("Load Metadata Errors")); horizontalHeaderItem(G::ErrColumn)->setData(true, G::GeekRole);
    // "🔎" was title for search column
}

qint64 DataModel::rowBytesUsed(int dmRow)
{
    qint64 bytes = 0;
    QHash<int, QByteArray> roles = roleNames(); // Get all available roles
    // qDebug() << "DataModel::rowBytesUsed" << "roles:" << roles;
    for (int col = 0; col < columnCount(); ++col) {
        QModelIndex dmIdx = index(dmRow, col);
        for (int role : roles.keys()) {
            QVariant data = this->data(dmIdx, role);
            if (!data.isNull()) {
                bytes += Utilities::qvariantBytes(data);
                if (isDebug)
                {
                qDebug().noquote()
                    << "DataModel::rowBytesUsed"
                    << horizontalHeaderItem(col)->text().leftJustified(25)
                    << "col =" << QString::number(col).rightJustified(2)
                    << "role =" << QString::number(role).rightJustified(1)
                    << "bytes =" << QString::number(Utilities::qvariantBytes(data)).rightJustified(12)
                    << "sumbytes =" << bytes;
                }
            }
        }
    }
    return bytes;
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
    // reset memory used by model
    bytesUsed = 0;
    // clear memory-overrun latch and Reader→DataModel backpressure counter
    G::memoryOverrunFlag.store(false, std::memory_order_relaxed);
    queuedReaderEvents.store(0, std::memory_order_relaxed);
    // model is now empty: reset the running load-flag counts
    metadataAttemptedCount.store(0, std::memory_order_relaxed);
    metadataLoadedCount.store(0, std::memory_order_relaxed);
    iconLoadedCount.store(0, std::memory_order_relaxed);
    videoRowCount.store(0, std::memory_order_relaxed);
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
    const bool track =
        idx.isValid() &&
        (role == Qt::EditRole || role == Qt::DisplayRole) &&
        (isStatusCol || isBoolCol);

    int  oldStatus = G::MetaNotAttempted;
    bool oldVal = false;
    if (track) {
        if (isStatusCol) oldStatus = QStandardItemModel::data(idx, Qt::DisplayRole).toInt();
        else             oldVal    = QStandardItemModel::data(idx, Qt::DisplayRole).toBool();
    }

    const bool ok = QStandardItemModel::setData(idx, value, role);

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
        else {
            const bool newVal = value.toBool();
            if (newVal != oldVal) {
                std::atomic<int> *counter =
                    col == G::IconLoadedColumn ? &iconLoadedCount : &videoRowCount;
                counter->fetch_add(newVal ? 1 : -1, std::memory_order_relaxed);
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
    int meta = 0, loaded = 0, icon = 0, video = 0;
    const int n = rowCount();
    for (int row = 0; row < n; ++row) {
        const int status = index(row, G::MetadataStatusColumn).data().toInt();
        if (status != G::MetaNotAttempted)                          ++meta;
        if (status == G::MetaLoaded)                                ++loaded;
        if (index(row, G::IconLoadedColumn).data().toBool())        ++icon;
        if (index(row, G::VideoColumn).data().toBool())             ++video;
    }
    metadataAttemptedCount.store(meta,  std::memory_order_relaxed);
    metadataLoadedCount.store(loaded,   std::memory_order_relaxed);
    iconLoadedCount.store(icon,         std::memory_order_relaxed);
    videoRowCount.store(video,          std::memory_order_relaxed);
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
    insertRow(dmRow);

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
    if (G::isPerfProbe)
        qDebug().noquote() << "[PERF] restore sort: sortColumn=" << sf->sortColumn()
                           << "sortOrder=" << (sf->sortOrder() == Qt::DescendingOrder ? "Desc" : "Asc");
    sf->sort(-1);
    sf->setDynamicSortFilter(true);
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
    loadingModel = true;    // rgh is this needed?  Review loadingModel usage
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
            setRowCount(row + valid.size());            // ONE rowsInserted for the batch
            if (!columnCount()) setColumnCount(G::TotalColumns);
            {
                const QSignalBlocker blocker(this);     // suppress per-cell dataChanged
                for (const QFileInfo &fileInfo : valid) {
                    addFileDataForRow(row, fileInfo);
                    row++;
                }
            }
            emit dataChanged(index(first, 0), index(row - 1, columnCount() - 1));
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

        setRowCount(row + 1);
        if (!columnCount()) setColumnCount(G::TotalColumns);

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
    sf->suspend(false);
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

void DataModel::rawJpgPairing(int row, const QString &ext, const QString &baseName)
{
    bool isRaw = metadata->hasJpg.contains(ext);
    bool isJpg = (ext == "jpg" || ext == "jpeg");

    // Only update the fast sequential trackers during the initial bulk folder load
    if (isRaw && loadingModel) {
        prevRawSuffix = ext;
        prevRawBaseName = baseName;
        prevRawIdx = index(row, 0);
    }

    QMutexLocker locker(&dmMutex);

    if (isJpg || isRaw) {

        // --- PATH A: BULK LOAD (Fast Sequential Check) ---
        if (loadingModel) {
            if (isJpg && baseName == prevRawBaseName) {
                setData(prevRawIdx, true, G::DupHideRawRole);
                setData(prevRawIdx, index(row, 0), G::DupOtherIdxRole);
                setData(index(row, 0), prevRawIdx, G::DupOtherIdxRole);
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
                    setData(otherIdx, index(row, 0), G::DupOtherIdxRole);
                    setData(index(row, 0), otherIdx, G::DupOtherIdxRole);
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
                    setData(index(row, 0), otherIdx, G::DupOtherIdxRole);
                    setData(otherIdx, index(row, 0), G::DupOtherIdxRole);
                    setData(otherIdx, true, G::DupIsJpgRole);

                    setData(otherIdx, ext.toUpper(), G::DupRawTypeRole);
                    if (combineRawJpg)
                        setData(otherIdx, "JPG+" + ext.toUpper(), G::TypeColumn);
                    else
                        setData(otherIdx, "JPG", G::TypeColumn);
                    break;
                }
            }
        }
    }
}

void DataModel::addFileDataForRow(int row, QFileInfo fileInfo)
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
    setData(index(row, G::RowNumberColumn), int(Qt::AlignCenter | Qt::AlignVCenter), Qt::TextAlignmentRole);
    setData(index(row, G::PathColumn), fPath, G::PathRole);
    // Show tooltips for each item in datamodel views - this has been moved to
    // IconView::mouseMoveEvent so can show tooltips for icon symbols as well
    setData(index(row, G::PathColumn), QRect(), G::IconRectRole);
    setData(index(row, G::PathColumn), false, G::DupHideRawRole);
    setData(index(row, G::NameColumn), fileInfo.fileName());
    setData(index(row, G::NameColumn), fileInfo.fileName(), Qt::ToolTipRole);
    setData(index(row, G::FolderNameColumn), folderName);
    QString s = fileInfo.suffix().toUpper();
    setData(index(row, G::TypeColumn), s);
    setData(index(row, G::TypeColumn), int(Qt::AlignCenter), Qt::TextAlignmentRole);
    setData(index(row, G::VideoColumn), metadata->videoFormats.contains(ext));
    setData(index(row, G::VideoColumn), int(Qt::AlignCenter | Qt::AlignVCenter), Qt::TextAlignmentRole);
    setData(index(row, G::SidecarColumn), QFile(sidecarPath).exists());
    setData(index(row, G::SidecarColumn), int(Qt::AlignCenter | Qt::AlignVCenter), Qt::TextAlignmentRole);
    uint p = static_cast<uint>(fileInfo.permissions());
    setData(index(row, G::PermissionsColumn), p);
    setData(index(row, G::PermissionsColumn), int(Qt::AlignCenter | Qt::AlignVCenter), Qt::TextAlignmentRole);
    bool isReadWrite = (p & QFileDevice::ReadUser) && (p & QFileDevice::WriteUser);
    setData(index(row, G::ReadWriteColumn), isReadWrite);
    setData(index(row, G::ReadWriteColumn), int(Qt::AlignCenter | Qt::AlignVCenter), Qt::TextAlignmentRole);
    // size
    quint32 bytes = fileInfo.size();
    setData(index(row, G::ByteSizeColumn), bytes);
    setData(index(row, G::ByteSizeColumn), int(Qt::AlignRight | Qt::AlignVCenter), Qt::TextAlignmentRole);

    // estimate cacheSize until read metadata and calc size for QImage
    float mb = static_cast<double>(bytes) / (1 << 20);
    if (raw.contains(ext)) mb *= 3.5;
    if (ext == "jpg" || ext == "jpeg") mb *= 15;
    setData(index(row, G::CacheSizeColumn), mb);

    setData(index(row, G::CompareColumn), false);
    s = fileInfo.birthTime().toString("yyyy-MM-dd hh:mm:ss.zzz");
    setData(index(row, G::CreatedColumn), s);
    s = fileInfo.lastModified().toString("yyyy-MM-dd hh:mm:ss");
    search += s;
    setData(index(row, G::ModifiedColumn), s);
    setData(index(row, G::PickColumn), "Unpicked");
    setData(index(row, G::PickColumn), int(Qt::AlignCenter | Qt::AlignVCenter), Qt::TextAlignmentRole);
    setData(index(row, G::IngestedColumn), "false");
    setData(index(row, G::IngestedColumn), int(Qt::AlignCenter | Qt::AlignVCenter), Qt::TextAlignmentRole);
    setData(index(row, G::MetadataReadingColumn), false);
    setData(index(row, G::MetadataStatusColumn), G::MetaNotAttempted);
    setData(index(row, G::IconLoadedColumn), false);
    setData(index(row, G::SearchColumn), "false");
    setData(index(row, G::SearchColumn), Qt::AlignLeft, Qt::TextAlignmentRole);
    setData(index(row, G::SearchTextColumn), search);
    setData(index(row, G::SearchTextColumn), search, Qt::ToolTipRole);

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
    setData(index(row, G::ByteSizeColumn), int(Qt::AlignRight | Qt::AlignVCenter), Qt::TextAlignmentRole);
    QString s = fileInfo.lastModified().toString("yyyy-MM-dd hh:mm:ss");
    setData(index(row, G::ModifiedColumn), s);

    // created date
    s = fileInfo.birthTime().toString("yyyy-MM-dd hh:mm:ss.zzz");
    setData(index(row, G::CreatedColumn), s);
    qDebug() << "DataModel::updateFileData" << s;

    return true;
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

    // rgh retrying here has random crashes - is this required?  Seems to be working...

    m.pick  = index(row, G::PickColumn).data().toBool();
    m.ingested  = index(row, G::IngestedColumn).data().toBool();
    m.metadataReading = index(row, G::MetadataReadingColumn).data().toBool();
    m.metaStatus = index(row, G::MetadataStatusColumn).data().toInt();
    m.permissions = index(row, G::PermissionsColumn).data().toUInt();
    m.isReadWrite = index(row, G::ReadWriteColumn).data().toBool();

    m.width = index(row, G::WidthColumn).data().toInt();
    m.height = index(row, G::HeightColumn).data().toInt();
    m.widthPreview = index(row, G::WidthPreviewColumn).data().toInt();
    m.heightPreview = index(row, G::HeightPreviewColumn).data().toInt();
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
    if (m.exposureTimeNum < 1.0) {
        double recip = 1 / m.exposureTimeNum;
        if (recip >= 2) m.exposureTime = "1/" + QString::number(qRound(recip));
        else m.exposureTime = QString::number(m.exposureTimeNum, 'g', 2);
    }
    else {
        m.exposureTime = QString::number(m.exposureTimeNum);
    }
    m.exposureTime += " sec";
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
    m.iccSpace = index(row, G::ICCSegmentOffsetColumn).data().toString();

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

    int mod = 10;
    if (rowCount() > 1000) mod = 100;
    int count = 0;
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
    /* Backpressure: matches Reader's fetch_add before emit addToDatamodel.
       RAII so every early-return path also decrements. */
    struct QrEvGuard {
        std::atomic<int> &c;
        ~QrEvGuard() { c.fetch_sub(1, std::memory_order_relaxed); }
    } qrEvGuard{queuedReaderEvents};

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

    QString search = index(row, G::SearchTextColumn).data().toString();

    QMutexLocker locker(&dmMutex);
    mLock = true;
    // block signals while stuffing cells
    const QSignalBlocker b(this);

    setData(index(row, G::SearchColumn), m.isSearch);
    setData(index(row, G::SearchColumn), Qt::AlignCenter, Qt::TextAlignmentRole);
    setData(index(row, G::LabelColumn), m.label);
    setData(index(row, G::LabelColumn), Qt::AlignCenter, Qt::TextAlignmentRole);
    search += m.label;
    setData(index(row, G::_LabelColumn), m._label);
    // if (m.rating == "") m.rating = "No Rating";
    // if (m.rating == "0") m.rating = "No Rating";
    if (m.rating == "0") m.rating = "";
    setData(index(row, G::RatingColumn), m.rating);
    setData(index(row, G::RatingColumn), Qt::AlignCenter, Qt::TextAlignmentRole);
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
        setData(index(row, G::DayColumn), createdDT.toString("yyyy-MM-dd"));
    }

    setData(index(row, G::WidthColumn), QString::number(m.width));
    setData(index(row, G::WidthColumn), Qt::AlignCenter, Qt::TextAlignmentRole);
    setData(index(row, G::HeightColumn), QString::number(m.height));
    setData(index(row, G::HeightColumn), Qt::AlignCenter, Qt::TextAlignmentRole);
    setData(index(row, G::AspectRatioColumn), QString::number((aspectRatio(m.width, m.height, m.orientation)), 'f', 2));
    setData(index(row, G::AspectRatioColumn), int(Qt::AlignRight | Qt::AlignVCenter), Qt::TextAlignmentRole);
    setData(index(row, G::DimensionsColumn), QString::number(m.width) + "x" + QString::number(m.height));
    setData(index(row, G::DimensionsColumn), Qt::AlignCenter, Qt::TextAlignmentRole);
    setData(index(row, G::MegaPixelsColumn), QString::number((m.width * m.height) / 1000000.0, 'f', 2));
    setData(index(row, G::MegaPixelsColumn), int(Qt::AlignRight | Qt::AlignVCenter), Qt::TextAlignmentRole);
    setData(index(row, G::LoadMsecPerMpColumn), m.loadMsecPerMp);
    setData(index(row, G::LoadMsecPerMpColumn), Qt::AlignCenter, Qt::TextAlignmentRole);
    setData(index(row, G::OrientationColumn), QString::number(m.orientation));
    setData(index(row, G::OrientationColumn), Qt::AlignCenter, Qt::TextAlignmentRole);
    setData(index(row, G::RotationColumn), QString::number(m.rotationDegrees));
    setData(index(row, G::RotationColumn), Qt::AlignCenter, Qt::TextAlignmentRole);
    setData(index(row, G::CameraMakeColumn), m.make);
    setData(index(row, G::CameraMakeColumn), m.make, Qt::ToolTipRole);
    search += m.make;
    setData(index(row, G::CameraModelColumn), m.model);
    setData(index(row, G::CameraModelColumn), m.model, Qt::ToolTipRole);
    search += m.model;
    setData(index(row, G::ShutterspeedColumn), m.exposureTimeNum);
    setData(index(row, G::ShutterspeedColumn), int(Qt::AlignRight | Qt::AlignVCenter), Qt::TextAlignmentRole);
    setData(index(row, G::ApertureColumn), m.apertureNum);
    setData(index(row, G::ApertureColumn), Qt::AlignCenter, Qt::TextAlignmentRole);
    setData(index(row, G::ISOColumn), m.ISONum);
    setData(index(row, G::ISOColumn), Qt::AlignCenter, Qt::TextAlignmentRole);
    setData(index(row, G::ExposureCompensationColumn), m.exposureCompensation);
//    setData(index(row, G::ExposureCompensationColumn), m.exposureCompensationNum);
    setData(index(row, G::ExposureCompensationColumn), Qt::AlignCenter, Qt::TextAlignmentRole);
    setData(index(row, G::LensColumn), m.lens);
    setData(index(row, G::LensColumn), m.lens, Qt::ToolTipRole);
    search += m.lens;
    setData(index(row, G::FocalLengthColumn), m.focalLengthNum);
    setData(index(row, G::FocalLengthColumn), int(Qt::AlignRight | Qt::AlignVCenter), Qt::TextAlignmentRole);
    setData(index(row, G::FocusXColumn), m.focusX);
    setData(index(row, G::FocusYColumn), m.focusY);
    setData(index(row, G::GPSCoordColumn), m.gpsCoord);
    setData(index(row, G::GPSCoordColumn), m.gpsCoord, Qt::ToolTipRole);
    setData(index(row, G::KeywordsColumn), QVariant(m.keywords));
    setData(index(row, G::KeywordsColumn), Utilities::stringListToString(m.keywords), Qt::ToolTipRole);
    search += Utilities::stringListToString(m.keywords);
    setData(index(row, G::ShootingInfoColumn), m.shootingInfo);
    setData(index(row, G::ShootingInfoColumn), m.shootingInfo, Qt::ToolTipRole);
    search += m.shootingInfo;
    setData(index(row, G::TitleColumn), m.title);
    setData(index(row, G::TitleColumn), m.title, Qt::ToolTipRole);
    search += m.title;
    setData(index(row, G::_TitleColumn), m._title);
//    if (index(row, G::CreatorColumn).data().toString() != "") m.creator = index(row, G::CreatorColumn).data().toString();
    setData(index(row, G::CreatorColumn), m.creator);
    setData(index(row, G::CreatorColumn), m.creator, Qt::ToolTipRole);
    search += m.creator;
    setData(index(row, G::_CreatorColumn), m._creator);
//    if (index(row, G::CopyrightColumn).data().toString() != "") m.copyright = index(row, G::CopyrightColumn).data().toString();
    setData(index(row, G::CopyrightColumn), m.copyright);
    setData(index(row, G::CopyrightColumn), m.copyright, Qt::ToolTipRole);
    search += m.copyright;
    setData(index(row, G::_CopyrightColumn), m._copyright);
//    if (index(row, G::EmailColumn).data().toString() != "") m.email = index(row, G::EmailColumn).data().toString();
    setData(index(row, G::EmailColumn), m.email);
    setData(index(row, G::EmailColumn), m.email, Qt::ToolTipRole);
    search += m.email;
    setData(index(row, G::_EmailColumn), m._email);
//    if (index(row, G::UrlColumn).data().toString() != "") m.url = index(row, G::UrlColumn).data().toString();
    setData(index(row, G::UrlColumn), m.url);
    search += m.url;
    setData(index(row, G::_UrlColumn), m._url);
    setData(index(row, G::CompareColumn), m.compare);
    setData(index(row, G::OffsetFullColumn), m.offsetFull);
    setData(index(row, G::LengthFullColumn), m.lengthFull);
    setData(index(row, G::WidthPreviewColumn), m.widthPreview);
    setData(index(row, G::HeightPreviewColumn), m.heightPreview);
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
    setData(index(row, G::OrientationOffsetColumn), m.orientationOffset);
    setData(index(row, G::OrientationColumn), m.orientation);
    setData(index(row, G::RotationDegreesColumn), m.rotationDegrees);
    // cancel as causing repeats for some videos
    // setData(index(row, G::MetadataReadingColumn), m.metadataReading);
    setData(index(row, G::MetadataStatusColumn), m.metaStatus);
    // setData(index(row, G::MissingThumbColumn), m.isEmbeddedThumbMissing);
    setData(index(row, G::CompareColumn), m.compare);
    setData(index(row, G::SearchTextColumn), search.toLower());
    setData(index(row, G::SearchTextColumn), search.toLower(), Qt::ToolTipRole);

    // image cache helpers
    // do not set these.  Out of order when multi-folder selection
    // setData(index(row, G::IsCachingColumn), false);
    // setData(index(row, G::IsCachedColumn), false);
    // qDebug() << "add isCached for row =" << row;

    setData(index(row, G::AttemptsColumn), 0);
    setData(index(row, G::DecoderIdColumn), -1);
    setData(index(row, G::DecoderReturnStatusColumn), 0);
    // calc size in MB req'd to store image in cache
    if (!m.video) {
        int w, h;
        m.widthPreview > 0 ? w = m.widthPreview : w = m.width;
        m.heightPreview > 0 ? h = m.heightPreview : h = m.height;
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

    bytesUsed += rowBytesUsed(row);

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

bool DataModel::iconRowVisible(const QModelIndex &dmIdx)
{
/*
    True when the row is currently visible in a view (or the optimization is off). Used by
    setIcon1 / setValDm to skip the dataChanged notification for off-screen rows during a
    bulk load — they are stored without notifying and paint correctly when scrolled to.
    firstVisibleIcon / lastVisibleIcon are sf (proxy) rows maintained by MW::updateIconRange;
    a degenerate range (not yet established) falls back to "visible" so nothing is missed.
*/
    if (!G::useVisibleOnlyIconEmit) return true;
    if (lastVisibleIcon <= firstVisibleIcon) return true;   // range not established → emit
    const int sfRow = sf->mapFromSource(dmIdx).row();
    return sfRow >= firstVisibleIcon && sfRow <= lastVisibleIcon;
}

void DataModel::setValDm(int dmRow, int dmCol, QVariant value, int instance,
                         QString src, int role, int align)
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

    /* Batch the value + alignment writes under a QSignalBlocker and emit ONE dataChanged.
       Called per icon for the NSThumb column during loads; two separate dataChanged here
       were ~half of the load-time GUI stall (measured). One coalesced signal collapses it. */
    {
        const QSignalBlocker blocker(this);
        setData(dmIdx, value, role);
        setData(dmIdx, align, Qt::TextAlignmentRole);
    }
    if (iconRowVisible(dmIdx))
        emit dataChanged(dmIdx, dmIdx);
}

void DataModel::setValSf(int sfRow, int sfCol, QVariant value, int instance,
                         QString src, int role, int align)
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

    sf->setData(sfIdx, value, role);
    sf->setData(sfIdx, align, Qt::TextAlignmentRole);
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
        setData(index(dmRow, G::DurationColumn), durationTime.toString(format));
    }

    QStandardItem *item = itemFromIndex(dmIdx);
    if (itemFromIndex(dmIdx)->icon().isNull()) {
        if (item != nullptr) {
            item->setIcon(QPixmap::fromImage(im));
            setData(index(dmIdx.row(), G::IconLoadedColumn), true);
            setData(index(dmIdx.row(), G::MetadataStatusColumn), G::MetaLoaded);
            setData(index(dmIdx.row(), G::MetadataReadingColumn), false);
            // set aspect ratio for video
            if (im.height() > 0) {
                QString aspectRatio = QString::number(im.width() * 1.0 / im.height(), 'f', 2);
                setData(index(dmRow, G::AspectRatioColumn), aspectRatio);
            }
        }
    }
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
*/
    if (G::isLogger) G::log("DataModel::clearVideoReadingFlag");
    if (G::stop) return;
    if (fromInstance != instance) return;

    QModelIndex dmIdx = index(dmRow, 0);
    if (!dmIdx.isValid()) return;

    QMutexLocker locker(&dmMutex);
    setData(index(dmRow, G::MetadataReadingColumn), false);
    // attempted but no frame decoded -> failed
    setData(index(dmRow, G::MetadataStatusColumn), G::MetaFailed);
}

void DataModel::updateIconChunkLoaded()
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
    const int needed = span - videoRowCount.load(std::memory_order_relaxed);
    if (span > 0 &&
        iconLoadedCount.load(std::memory_order_relaxed) < needed) {
        G::iconChunkLoaded = false;
        return;
    }
    G::iconChunkLoaded = isAllIconChunkLoaded(startIconRange, endIconRange);
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
    if (QStandardItem *existing = itemFromIndex(dmIdx)) {
        if (!existing->icon().isNull()) {
            setData(index(dmIdx.row(), G::IconLoadedColumn), true);
            setData(index(dmIdx.row(), G::MetadataReadingColumn), false);
            updateIconChunkLoaded();
            return;
        }
    }

    const QVariant vIcon = QVariant(QIcon(pm));
    setData(dmIdx, vIcon, Qt::DecorationRole);
    setData(index(dmIdx.row(), G::IconLoadedColumn), true);
    setData(index(dmIdx.row(), G::MetadataReadingColumn), false);
    updateIconChunkLoaded();
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
    if (QStandardItem *existing = itemFromIndex(dmIdx)) {
        if (!existing->icon().isNull()) {
            // ensure flags are correct even though the pixmap is unchanged (batched)
            {
                const QSignalBlocker blocker(this);
                setData(index(dmRow, G::IconLoadedColumn), true);
                setData(index(dmRow, G::MetadataReadingColumn), false);
            }
            if (iconRowVisible(dmIdx))
                emit dataChanged(index(dmRow, 0), index(dmRow, columnCount() - 1));
            updateIconChunkLoaded();
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
        emit dataChanged(dmIdx, index(dmRow, columnCount() - 1));
    updateIconChunkLoaded();

    /* Layer 2 (measured refinement): accumulate the real per-icon footprint. Once enough
       icons have loaded, recompute the budget with the true average and (one-shot) grow /
       promote a JIT folder when the measured footprint turns out to fit. */
    iconBytesSum.fetch_add(im.sizeInBytes(), std::memory_order_relaxed);
    const int n = iconSamples.fetch_add(1, std::memory_order_relaxed) + 1;
    if (G::useJitIconCache && !iconChunkRefined && !iconCachePressureLatched
        && n >= 200 && iconChunkSize < rowCount())
        refineIconChunkSize();
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
        if (!itemFromIndex(index(row, 0))->icon().isNull()) count++;
    }
    return count;
}

bool DataModel::isAllIconChunkLoaded(int first, int last)
{
    if (isDebug)
        qDebug() << "DataModel::isAllIconChunkLoaded" << "instance =" << instance;

    if (first < 0 || last > sf->rowCount()) return false;

    for (int row = first; row <= last; ++row) {
        // ignore video
        if (sf->index(row, G::VideoColumn).data().toBool()) continue;

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
    // mutex.lock();
    startIconRange = start;
    endIconRange = end;
    // mutex.unlock();
    G::iconChunkLoaded = isAllIconChunkLoaded(startIconRange, endIconRange);
}

void DataModel::setChunkSize(int chunkSize)
{
    iconChunkSize = chunkSize;
    setIconRange(currentSfRow);
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

    At endLoad no icons are loaded yet, so the budget uses a worst-case per-icon estimate;
    refineIconChunkSize() (Layer 2) revisits the decision once real icons have loaded.

    Called from endLoad on the GUI thread after a folder has loaded.
*/
{
    const int rows = rowCount();

    // new folder: reset the measured-footprint accumulators (Layer 2)
    iconBytesSum.store(0, std::memory_order_relaxed);
    iconSamples.store(0, std::memory_order_relaxed);
    iconChunkRefined = false;

    if (rows == 0) return;

    if (!G::useJitIconCache) {
        // brute force: cache an icon for every row in the datamodel
        iconChunkSize = rows;
        setIconRange(currentSfRow);
        return;
    }

    /* JIT (Layer 1): no icons are loaded yet at endLoad, so iconBudgetCount() uses the
       worst-case per-icon estimate. Small folder -> full populate; large -> bounded
       window. refineIconChunkSize() revisits this once real icons have loaded. */
    const int budgetIcons = iconBudgetCount();
    iconChunkSize = qMin(rows, qMax(budgetIcons, iconChunkFloor()));

    if (isDebug || G::isLogger)
        G::log("DataModel::resolveIconChunkSize",
               QString("rows=%1 budgetIcons=%2 iconChunkSize=%3 mode=%4")
                   .arg(rows).arg(budgetIcons).arg(iconChunkSize)
                   .arg(iconChunkSize < rows ? "JIT window" : "full"));

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

void DataModel::refineIconChunkSize()
/*
    Layer 2: once enough real icons have loaded, recompute the budget with the measured
    per-icon average (avgIconMB) and grow the window — or promote a JIT folder back to full
    brute force — when the true footprint turns out to fit. One-shot per folder, and grow-only:
    shrinking on measured data would be reacting to memory, which is Layer 3 (not implemented).
    Called from setIcon1 on the GUI thread.
*/
{
    iconChunkRefined = true;
    const int rows = rowCount();
    if (rows == 0) return;

    const int budgetIcons = iconBudgetCount();
    const int newChunk = qMin(rows, qMax(budgetIcons, iconChunkFloor()));
    if (newChunk <= iconChunkSize) return;      // grow-only

    iconChunkSize = newChunk;

    if (isDebug || G::isLogger)
        G::log("DataModel::refineIconChunkSize",
               QString("rows=%1 avgKB=%2 budgetIcons=%3 iconChunkSize=%4 mode=%5")
                   .arg(rows).arg(avgIconMB() * 1024, 0, 'f', 1)
                   .arg(budgetIcons).arg(iconChunkSize)
                   .arg(iconChunkSize < rows ? "JIT window" : "full"));

    setIconRange(currentSfRow);
    emit iconChunkResized();            // re-dispatch MetaRead to fill the grown window
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
                           .arg(level).arg(iconChunkSize)
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

    for (int i = 0; i < startIconRange; i++) {
        if (abort) return;
        if (!sf->index(i, 0).data(Qt::DecorationRole).isNull()) {
            sf->setData(sf->index(i, 0), QVariant(), Qt::DecorationRole);
            sf->setData(sf->index(i, G::IconLoadedColumn), false);
        }
    }
    for (int i = endIconRange + 1; i < sf->rowCount(); i++) {
        if (abort) return;
        if (!sf->index(i, 0).data(Qt::DecorationRole).isNull()) {
            sf->setData(sf->index(i, 0), QVariant(), Qt::DecorationRole);
            sf->setData(sf->index(i, G::IconLoadedColumn), false);
        }
    }
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
    // update datamodel search string match
    QMutexLocker locker(&dmMutex);
    for (int row = 0; row < rowCount(); ++row)  {
        // no search string
        if (filters->ignoreSearchStrings.contains(searchString)) {
            setData(index(row, G::SearchColumn), false);
            filters->searchTrue->setText(0, filters->enterSearchString);
        }
        // there is a search string
        else {
            QString searchableText = index(row, G::SearchTextColumn).data().toString();
            setData(index(row, G::SearchColumn), searchableText.contains(searchString));
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
    rpt << "\n" << G::sj("startIconRange", dots) << G::s(startIconRange);
    rpt << "\n" << G::sj("endIconRange", dots) << G::s(endIconRange);
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
    rpt << "\n" << G::sj("iconChunkRefined", dots) << G::s(iconChunkRefined);
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
            const bool loaded = row < nRows
                && !itemFromIndex(index(row, 0))->icon().isNull();
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
                 .arg(visibleIcons).arg(iconChunkSize));
    } else {
        line("OK", "icon chunk vs visible",
             QString("visibleIcons=%1, iconChunkSize=%2")
                 .arg(visibleIcons).arg(iconChunkSize));
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
    rpt << "\n  " << G::sj("isIcon", dots) << G::s(!itemFromIndex(index(row, G::PathColumn))->icon().isNull());
    rpt << "\n  " << G::sj("isCached", dots) << G::s(index(row, G::IsCachedColumn).data());
    rpt << "\n  " << G::sj("MetadataReadingColumn", dots) << G::s(index(row, G::MetadataReadingColumn).data());
    rpt << "\n  " << G::sj("metaStatus", dots) << G::s(index(row, G::MetadataStatusColumn).data());
    rpt << "\n  " << G::sj("isIconLoaded", dots) << G::s(index(row, G::IconLoadedColumn).data());
    rpt << "\n  " << G::sj("isReadWrite", dots) << G::s(index(row, G::ReadWriteColumn).data());
    rpt << "\n  " << G::sj("dupHideRaw", dots) << G::s(index(row, 0).data(G::DupHideRawRole));
    rpt << "\n  " << G::sj("dupRawRow", dots) << G::s(qvariant_cast<QModelIndex>(index(row, 0).data(G::DupOtherIdxRole)).row());
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
    rpt << "\n  " << G::sj("widthPreview", dots) << G::s(index(row, G::WidthPreviewColumn).data());
    rpt << "\n  " << G::sj("heightPreview", dots) << G::s(index(row, G::HeightPreviewColumn).data());
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
}

bool SortFilter::filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const
{
/*
    The QTreeWidget filters is used to match checked filter items with the proper
    column in data model.  The top level items in the QTreeWidget are referred to
    as categories, and each category has one or more filter items.  Categories
    map to columns in the data model ie Picked, Rating, Label ...
*/

    // Suspend?
    if (suspendFiltering) {
        // qDebug() << "SortFilter::filterAcceptsRow suspendFiltering = true" << sourceRow;
        return true;
    }

    // still loading metadata
    if (!G::allMetadataAttempted) {
        // qDebug() << "SortFilter::filterAcceptsRow G::allMetadataAttempted = false" << sourceRow;
        return true;
    }

    // Check Raw + Jpg
    if (combineRawJpg) {
        QModelIndex rawIdx = sourceModel()->index(sourceRow, 0, sourceParent);
        // qDebug() << "SortFilter::filterAcceptsRow combineRawJpg = true" << sourceRow;
        if (rawIdx.data(G::DupHideRawRole).toBool()) {
            // qDebug() << "SortFilter::filterAcceptsRow DupHideRaw = true" << sourceRow;
            return false;
        }
        else {
            // qDebug() << "SortFilter::filterAcceptsRow DupHideRaw = false" << sourceRow;
        }
    }

    finished = false;
    static int counter = 0;
    int dataModelColumn = 0;
    bool isMatch = true;                   // overall match
    bool isCategoryUnchecked = true;
    QString itemCategory;                  // for debugging

    // cycle through the filters and identify matches
    QTreeWidgetItemIterator filter(filters);
    while (*filter) {
        // guard against filters changing
        if (!filters) {
            finished = true;
            return true;
        }
        if ((*filter)->parent()) {
            /*
            There is a parent therefore not a top level item so this is one of the items
            to match ie rating = one. If the item has been checked then compare the
            checked filter item to the data in the dataModelColumn for the row. If it
            matches then set isMatch = true. If it does not match then isMatch is still
            false but the row could still be accepted if another item in the same
            category does match.

            For search string matching see DataModel::searchStringChange, which sets the
            datamodel G::SearchColumn true/false.
            */
            if ((*filter)->checkState(0) != Qt::Unchecked) {  // crash
                if ((*filter) == filters->searchTrue &&
                    (*filter)->text(0) == filters->enterSearchString)
                {
                    isMatch = true;
                }
                else {
                    isCategoryUnchecked = false;
                    QModelIndex idx = sourceModel()->index(sourceRow, dataModelColumn, sourceParent);
                    QVariant dataValue = idx.data(Qt::EditRole);
                    QVariant filterValue = (*filter)->data(1, Qt::EditRole);
                    QString itemName = (*filter)->text(0);      // for debugging
                    /*
                    qDebug() << "DataModel::filterAcceptsRow"
                             << counter++
                             << "\tCat =" << itemCategory
                             << "dataModelColumn =" << dataModelColumn
                             << "itemName =" << itemName
                             << "sfRow" << sourceRow
                             << "Comparing" << dataValue << filterValue
                             << (dataValue == filterValue)
                                ;
                                //*/
                    /*
                    qDebug() << "DataModel::filterAcceptsRow" << dataValue << dataValue.typeId()
                             << dataModelColumn << G::KeywordsColumn
                             << dataValue.typeId()
                             << QMetaType::QStringList
                                ;
                                //*/
                    if (dataValue.typeId() == QMetaType::QStringList) {  // keywords
                        if (dataValue.toStringList().contains(filterValue)) isMatch = true;
                    }
                    else {
                        if (dataValue == filterValue) isMatch = true;
                    }
                }
            }
        }
        else {
            // top level item = category
            // check results of category items filter match
            if (isCategoryUnchecked) isMatch = true;
            /*
            qDebug() << "Category" << itemCategory
                     << "isCategoryUnchecked =" << isCategoryUnchecked
                     << "dataModelColumn =" << (*filter)->data(0, G::ColumnRole).toInt()
                         ;
            //*/

            if (!isMatch) {
                finished = true;
                return false;   // no match in category
            }

            /*
            Prepare for category items filter match.  If no item is checked
            or one checked item matches the data, then the row is okay to show
            the top level items contain reference to the data model column.
            */
            dataModelColumn = (*filter)->data(0, G::ColumnRole).toInt();
            isCategoryUnchecked = true;
            isMatch = false;
            itemCategory = (*filter)->text(0);      // for debugging
        }
        if (suspendFiltering) {
            finished = true;
            return false;
        }
        ++filter;
    }
    // check results of category items filter match for the last group
    if (isCategoryUnchecked) isMatch = true;
    finished = true;

    //qDebug() << "SortFilter::filterAcceptsRow  sf->rowCount =" << rowCount();
    return isMatch;
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
