#include "Datamodel/datamodel.h"

/*
The datamodel (dm thoughout app) contains information about each eligible image
file in the selected folder (and optionally the subfolder heirarchy).  Eligible
image files, defined in the metadata class, are files Winnow knows how to decode.

The data is structured in columns:

    ● Path:             from QFileInfoList  G::PathRole (absolutePath)
                        from QFileInfoList  Qt::ToolTipRole
                                            G::IconRectRole (icon)
                                            G::CachedRole
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
    dm->sf->setData(idx, iscached, G::CachedRole);

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

    // root folder containing images to be added to the data model
    dir = new QDir();

    // eligible image file types
    fileFilters = new QStringList;
    foreach (const QString &str, metadata->supportedFormats) {
        fileFilters->append("*." + str);
    }

    emptyImg.load(":/images/no_image.png");
    setThumbnailLegend();

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
    setHorizontalHeaderItem(G::FolderNameColumn, new QStandardItem("Folder Name")); horizontalHeaderItem(G::NameColumn)->setData(false, G::GeekRole);
    setHorizontalHeaderItem(G::MSToReadColumn, new QStandardItem("Read ms")); horizontalHeaderItem(G::MSToReadColumn)->setData(false, G::GeekRole);
    setHorizontalHeaderItem(G::PickColumn, new QStandardItem("Pick")); horizontalHeaderItem(G::PickColumn)->setData(false, G::GeekRole);
    setHorizontalHeaderItem(G::IngestedColumn, new QStandardItem("Ingested")); horizontalHeaderItem(G::IngestedColumn)->setData(false, G::GeekRole);
    setHorizontalHeaderItem(G::LabelColumn, new QStandardItem("Colour")); horizontalHeaderItem(G::LabelColumn)->setData(false, G::GeekRole);
    setHorizontalHeaderItem(G::RatingColumn, new QStandardItem("Rating")); horizontalHeaderItem(G::RatingColumn)->setData(false, G::GeekRole);
    setHorizontalHeaderItem(G::SearchColumn, new QStandardItem("Search")); horizontalHeaderItem(G::SearchColumn)->setData(false, G::GeekRole);
    setHorizontalHeaderItem(G::TypeColumn, new QStandardItem("Type")); horizontalHeaderItem(G::TypeColumn)->setData(false, G::GeekRole);
    setHorizontalHeaderItem(G::VideoColumn, new QStandardItem("Video")); horizontalHeaderItem(G::VideoColumn)->setData(false, G::GeekRole);
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
    setHorizontalHeaderItem(G::SizeColumn, new QStandardItem("Size")); horizontalHeaderItem(G::SizeColumn)->setData(false, G::GeekRole);
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
    setHorizontalHeaderItem(G::OrientationColumn, new QStandardItem("Orientation")); horizontalHeaderItem(G::OrientationColumn)->setData(false, G::GeekRole);
    setHorizontalHeaderItem(G::RotationColumn, new QStandardItem("Rot")); horizontalHeaderItem(G::RotationColumn)->setData(false, G::GeekRole);
    setHorizontalHeaderItem(G::CopyrightColumn, new QStandardItem("Copyright")); horizontalHeaderItem(G::CopyrightColumn)->setData(false, G::GeekRole);
    setHorizontalHeaderItem(G::TitleColumn, new QStandardItem("Title")); horizontalHeaderItem(G::TitleColumn)->setData(false, G::GeekRole);
    setHorizontalHeaderItem(G::EmailColumn, new QStandardItem("Email")); horizontalHeaderItem(G::EmailColumn)->setData(false, G::GeekRole);
    setHorizontalHeaderItem(G::UrlColumn, new QStandardItem("Url")); horizontalHeaderItem(G::UrlColumn)->setData(false, G::GeekRole);
    setHorizontalHeaderItem(G::KeywordsColumn, new QStandardItem("Keywords")); horizontalHeaderItem(G::KeywordsColumn)->setData(false, G::GeekRole);
    setHorizontalHeaderItem(G::MetadataAttemptedColumn, new QStandardItem("Meta Attempted")); horizontalHeaderItem(G::MetadataAttemptedColumn)->setData(true, G::GeekRole);
    setHorizontalHeaderItem(G::MetadataLoadedColumn, new QStandardItem("Meta Loaded")); horizontalHeaderItem(G::MetadataLoadedColumn)->setData(true, G::GeekRole);
    setHorizontalHeaderItem(G::MissingThumbColumn, new QStandardItem("Missing Thumb")); horizontalHeaderItem(G::MissingThumbColumn)->setData(true, G::GeekRole);
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
    setHorizontalHeaderItem(G::IsTargetColumn, new QStandardItem("IsTarget")); horizontalHeaderItem(G::IsTargetColumn)->setData(true, G::GeekRole);
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

void DataModel::clearDataModel()
{
    if (G::isLogger || G::isFlowLogger) G::log("DataModel::clearDataModel");
    // clear the model
    if (mLock) return;
    if (isDebug) qDebug() << "DataModel::clearDataModel" << "instance =" << instance;
    clear();
    setModelProperties();
    // clear the fPath index of datamodel rows
    fPathRow.clear();
    // clear the folder list
    folderList.clear();
    // reset firstFolderPathWithImages
    firstFolderPathWithImages = "";
    // reset iconChunkSize
    iconChunkSize = defaultIconChunkSize;
    // reset missing thumb (jpg/tiff)
    folderHasMissingEmbeddedThumb = false;
    // folderQueue is empty
    isProcessing = false;
}

void DataModel::newInstance()
{
    if (G::isLogger || G::isFlowLogger) G::log("DataModel::newInstance");
    instance++;
    // qDebug() << "DataModel::newInstance =" << instance;
    G::dmInstance = instance;
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
        if (i1.suffix().toLower() == "jpg") s1.replace(".jpg", ".zzz");
        if (i2.suffix().toLower() == "jpg") s2.replace(".jpg", ".zzz");
        if (i1.suffix().toLower() == "jpeg") s1.replace(".jpeg", ".zzz");
        if (i2.suffix().toLower() == "jpeg") s2.replace(".jpeg", ".zzz");
    }
    return s1 < s2;
}

int DataModel::insert(QString fPath)
{
/*
    Called by MW::insertFile.

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
    QString insertFileName = insertFileInfo.fileName().toLower();

    // find row greater than insert file absolute path
    int dmRow;
    for (dmRow = 0; dmRow < rowCount(); ++dmRow) {
        QString rowPath = index(dmRow, 0).data(G::PathRole).toString();
        QFileInfo currentFile(rowPath);
        QString currentFileName = currentFile.fileName().toLower();
        if (insertFileName < currentFileName) {
            break;
        }
    }

    // insert new row
    insertRow(dmRow);

    /*
    qDebug() << "DataModel::insert"
             << "dmRow =" << dmRow
             << "fPath =" << fPath;
    //*/

    // update fPathRow hash
    rebuildRowFromPathHash();

    // update imageCount
    imageCount = rowCount();

    // add the file data to datamodel
    addFileDataForRow(dmRow, insertFileInfo);

    // reset loaded flags so MetaRead knows to load
    G::allMetadataLoaded = false;
    G::iconChunkLoaded = false;
    return dmRow;
}

void DataModel::remove(QString fPath)
{
/*
    Delete a row from the data model matching the absolute path.  This is used when an image
    file has been deleted by Winnow.  Also update fileInfoList and fPathRow.
*/
    if (G::isLogger) G::log("DataModel::remove");
    if (isDebug) qDebug() << "DataModel::remove" << "instance =" << instance << fPath;

    // remove row from datamodel
    int row;
    for (row = 0; row < rowCount(); ++row) {
        QString rowPath = index(row, 0).data(G::PathRole).toString();
        if (rowPath == fPath) {
            QModelIndex par;
            beginRemoveRows(par, row, row);
            removeRow(row);
            endRemoveRows();
            break;
        }
    }

    // rebuild fPathRow hash
    rebuildRowFromPathHash();

    // update imageCount
    imageCount = rowCount();
}

// MULTI-SELECT FOLDERS

bool DataModel::isQueueEmpty()
{
    return folderQueue.isEmpty();
}

void DataModel::enqueueOp(const QString folderPath, const QString op)
{
    if (G::isLogger || G::isFlowLogger) G::log("DataModel::enqueueOp", op + " " + folderPath);
    // qDebug() << "DataModel::enqueueOp"
    //          << "op =" << op
    //          << "folderPath =" << folderPath;

    if (op == "Toggle") {
        if (folderList.contains(folderPath)) {
            folderQueue.enqueue(qMakePair(folderPath, false));
        }
        else {
            folderQueue.enqueue(qMakePair(folderPath, true));
        }
    }

    else if (op == "Add") {
        if (!folderList.contains(folderPath)) {
            folderQueue.enqueue(qMakePair(folderPath, true));
        }
    }

    else if (op == "Remove") {
        if (folderList.contains(folderPath)) {
            folderQueue.enqueue(qMakePair(folderPath, false));
        }
    }
}

void DataModel::enqueueFolderSelection(const QString &folderPath, QString op, bool recurse)
{
    QString fun = "DataModel::enqueueFolderSelection";
    QString msg = "op = " + op +
                  " recurse = " + QVariant(recurse).toString() +
                  " folderPath = " + folderPath;
    if (G::isLogger || G::isFlowLogger) G::log(fun, msg);
    // qDebug() << fun
    //          << "op =" << op
    //          << "recurse =" << recurse
    //          << "folderPath =" << folderPath;

    if (recurse) {
        enqueueOp(folderPath, op);
        QDirIterator it(folderPath, QDirIterator::Subdirectories);
        while (it.hasNext()) {
            QString dPath = it.next();
            if (it.fileInfo().isDir() && it.fileName() != "." && it.fileName() != "..") {
                 enqueueOp(dPath, op);
            }
        }
    }
    else {
        enqueueOp(folderPath, op);
    }

    // If not already processing, start the processing
    if (!isProcessing) {
        isProcessing = true;
        processNextFolder();
    }
}

void DataModel::processNextFolder() {

    if (G::stop) return;

    // QMutexLocker locker(&mutex);
    if (folderQueue.isEmpty()) {
        isProcessing = false;
        return;
    }
    QPair<QString, bool> folderOperation = folderQueue.dequeue();
    // locker.unlock(); // Unlock the queue while processing

    QString fun = "DataModel::processNextFolder";
    QString msg = "folderOperation.first = " + folderOperation.first + " " +
                  "folderOperation.second = " + QVariant(folderOperation.second).toString();
    if (G::isLogger || G::isFlowLogger) G::log(fun, msg);

    // Process the folder synchronously using QtConcurrent
    QtConcurrent::run([this, folderOperation]() {
        // add images from model
        if (folderOperation.second) {
            addFolder(folderOperation.first);
            // signal MW::loadConcurrentChanged
            emit addedFolderToDM(folderOperation.first, "Add");
        }
        // remove images from model
        else {
            // wait for processing to be stopped
            if (!G::stop) {
                G::removingFolderFromDM = true;
                G::stop = true;
                qDebug() << "DataModel::processNextFolder stop before removing" << folderOperation.first;
                emit stop("DataModel::processNextFolder");
                // QMutexLocker locker(&G::gMutex);
                // while (G::stop) {
                //     G::waitCondition.wait(&G::gMutex); // EXC_BAD_ACCESS (SIGSEGV) exception
                // }
                // locker.unlock();
            }
            removeFolder(folderOperation.first);
            qDebug() << "DataModel::processNextFolder rowCount =" << rowCount() << sf->rowCount();
            // signal MW::loadConcurrentChanged
            emit removedFolderFromDM(folderOperation.first, "Remove");
        }

        // Continue with the next folder operation
        if (!G::stop)
            QMetaObject::invokeMethod(this, "processNextFolder", Qt::QueuedConnection);
    });
}

void DataModel::addFolder(const QString &folderPath)
{
    QString fun = "DataModel::addFolder";
    if (G::isLogger || G::isFlowLogger) G::log(fun, folderPath);

    // qDebug() << fun << "folder =" << folderPath;

    // control
    QMutexLocker locker(&mutex);
    abortLoadingModel = false;
    currentPrimaryFolderPath = folderPath;
    folderList.append(folderPath);
    loadingModel = true;    // rgh is this needed?  Review loadingModel usage
    locker.unlock(); // Unlock the queue while processing

    // folder fileInfo list
    QDir dir(folderPath);
    dir.setNameFilters(*fileFilters);
    dir.setFilter(QDir::Files);
    QList<QFileInfo> folderFileInfoList = dir.entryInfoList();

    if (combineRawJpg) {
        // make sure, if raw+jpg pair, that raw file is first to make combining easier
        std::sort(folderFileInfoList.begin(), folderFileInfoList.end(), lessThanCombineRawJpg);
    }
    else {
        std::sort(folderFileInfoList.begin(), folderFileInfoList.end(), lessThan);
    }

    QString step = "Loading eligible images.\n\n";
    QString escapeClause = "\n\nPress \"Esc\" to stop.";

    // test if raw file to match jpg when same file names and one is a jpg
    QString suffix;
    QString prevRawSuffix = "";
    QString prevRawBaseName = "";
    QString baseName = "";
    QModelIndex prevRawIdx;

    // sf->suspend(true);

    // datamodel size
    int row = rowCount();
    int newRowCount = rowCount() + folderFileInfoList.count();
    setRowCount(newRowCount);
    if (!columnCount()) setColumnCount(G::TotalColumns);
    /*
    qDebug() << "DataModel::addFolder"
             << "newRowCount =" << newRowCount
        ; //*/

    for (const QFileInfo &fileInfo : folderFileInfoList) {
        /*
        qDebug() << "DataModel::addFolder"
                 << "row =" << row
                 << "file =" << fileInfo.fileName()
                 << "folder =" << folderPath
                    ; //*/
        // Ensure thread-safe updates to the model
        // QMetaObject::invokeMethod(this, [this, row, prevRawSuffix, prevRawBaseName, prevRawIdx, fileInfo]() {
            addFileDataForRow(row, fileInfo);
            QString suffix = fileInfo.suffix().toLower();
            QString baseName = fileInfo.completeBaseName();
            if (metadata->hasJpg.contains(suffix)) {
                prevRawSuffix = suffix;
                prevRawBaseName = fileInfo.completeBaseName();
                prevRawIdx = index(row, 0);
            }

            QMutexLocker locker(&mutex);
            // if row/jpg pair
            if ((suffix == "jpg" || suffix == "jpeg") && baseName == prevRawBaseName) {
                // hide raw version
                setData(prevRawIdx, true, G::DupHideRawRole);
                // set raw version other index to jpg pair
                setData(prevRawIdx, index(row, 0), G::DupOtherIdxRole);
                // point to raw version
                setData(index(row, 0), prevRawIdx, G::DupOtherIdxRole);
                // set flag to show combined JPG file for filtering when ingesting
                setData(index(row, 0), true, G::DupIsJpgRole);
                // build combined suffix to show in type column
                setData(index(row, 0), prevRawSuffix.toUpper(), G::DupRawTypeRole);
                if (combineRawJpg)
                    setData(index(row, G::TypeColumn), "JPG+" + prevRawSuffix.toUpper());
                else
                    setData(index(row, G::TypeColumn), "JPG");
            }
        // }, Qt::QueuedConnection);
        row++;
    }

    sf->suspend(false);

    mutex.lock();
    loadingModel = false;
    if (firstFolderPathWithImages == "" && rowCount()) {
        firstFolderPathWithImages = folderPath;
    }
    mutex.unlock();

    endLoad(true);
}

void DataModel::removeFolder(const QString &folderPath)
{
    QString fun = "DataModel::removeFolder";
    if (G::isLogger || G::isFlowLogger) G::log(fun, folderPath);
    qDebug() << fun << folderPath;

    QMutexLocker locker(&mutex);

    folderList.removeAll(folderPath);

    QModelIndex parIdx = QModelIndex();
    QList<int> rowsToRemove;

    // Collect all rows that need to be removed
    for (int row = rowCount() - 1; row >= 0; --row) {
        QString filePath = index(row, 0).data(G::PathRole).toString();
        if (filePath.startsWith(folderPath)) {
            rowsToRemove.append(row);
        }
    }

    // Remove rows in a single batch
    if (!rowsToRemove.isEmpty()) {
        beginRemoveRows(parIdx, rowsToRemove.last(), rowsToRemove.first());
        for (int row : rowsToRemove) {
            removeRow(row);
            // qDebug() << "DataModel::removeFolder   remove row =" << row;
        }
        endRemoveRows();
    }

    sf->invalidate();

    // rebuild fPathRow hash
    rebuildRowFromPathHash();

    // update imageCount
    imageCount = rowCount();

    emit updateStatus(true, "", "DataModel::removeFolder");
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

    QMutexLocker locker(&mutex);
    for (int row = 0; row < sf->rowCount(); ++row) {
        QString searchableText = sf->index(row, G::SearchTextColumn).data().toString();
        qDebug() << "DataModel::find" << searchableText;
        if (searchableText.contains(text.toLower())) {
            QModelIndex idx = sf->mapToSource(sf->index(row, G::SearchColumn));
            setData(idx, "true");
        }
    }
}

void DataModel::abortLoad()
{
    if (G::isLogger) G::log("DataModel::abortLoad", "instance = " + QString::number(instance));
    if (isDebug) qDebug() << "DataModel::abortLoad" << "instance =" << instance;
    if (loadingModel) abortLoadingModel = true;
}

bool DataModel::endLoad(bool success)
{
    if (G::isLogger) G::log("DataModel::endLoad", "instance = " + QString::number(instance));
    if (isDebug) qDebug() << "DataModel::endLoad" << "instance =" << instance
                          << "success =" << success;

    loadingModel = false;
    if (success) {
        G::dmEmpty = false;
        checkChunkSize = iconChunkSize > rowCount();
        return true;
    }
    else {
        clear();
        G::dmEmpty = true;
        filters->loadingDataModelFailed();
        return false;
    }
}

bool DataModel::okManyImagesWarning()
{
    if (G::isLogger) G::log("DataModel::tooManyImagesWarning");
    QString title = "Too Many Images";
    QString max = QString::number(G::maxIconChunk);
    QString folders;
    G::includeSubfolders ? folders = "folder" : folders = "folders";
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

void DataModel::updateLoadStatus()
{
    QString step = "Searching for eligible images.\n\n";
    QString escapeClause = "\n\nPress \"Esc\" to stop.";
    QString root;
    if (dir->isRoot()) root = "Drive ";
    else root = "Folder ";
    QString folder;
    folderCount > 1 ? folder = " folders " : folder = " folder ";
    QString imageCountStr = QString::number(imageCount).leftJustified(6);
    QString folderCountStr = QString::number(folderCount).leftJustified(5);

    QString s = step +
                imageCountStr + " found so far in " +
                folderCountStr + folder +
                escapeClause;

    emit centralMsg(s);        // rghmsg
    qApp->processEvents();
    /*
    qDebug() << "DataModel::updateLoadStatus"
             << "imageCount =" << imageCount
             << "G::stop =" << G::stop
             << "abortLoadingModel =" << abortLoadingModel
             << "thread =" << QThread::currentThreadId()
        ;//*/
}

// bool DataModel::load(QString &folderPath, bool includeSubfoldersFlag)
// {
// /*
//     When a new folder is selected load it into the data model.  This clears the
//     model and populates the data model with all the cached thumbnail pixmaps from
//     metadataCache.  If include subfolders has been chosen then the entire subfolder
//     heirarchy is loaded.

//     Steps:
//     - filter to only show supported image formats, iterating subfolders if include
//       subfolders is set
//     - add each image file to the datamodel with QFileInfo related data such as file
//       name, path, file size, creation date
//     - also determine if there are duplicate raw+jpg files, and if so, populate all
//       the Dup...Role values to manage the raw+jpg files
//     - after the metadataReadThread has read all the metadata and thumbnails add
//       the rest of the metadata to the datamodel.

//     Note:
//     - building QMaps of unique field values for the filters is not done here,
//       but on demand when the user selects the filter panel or a menu filter command.
// */
//     if (G::isLogger || G::isFlowLogger) G::log("DataModel::load", folderPath);

//     // clearDataModel();        // already called by MW::reset
//     if (isDebug)
//         qDebug() << "DataModel::load" << "instance =" << instance << folderPath;

//     abortLoadingModel = false;
//     loadingModel = true;
//     subFolderImagesLoaded = false;
//     folderList.clear();
//     folderList.append(folderPath);
//     currentPrimaryFolderPath = folderPath;

//     // sf->suspend(true);

//     // emit centralMsg("Building image file list.");

//     // do some initializing
//     // fileFilters->clear();
//     // foreach (const QString &str, metadata->supportedFormats) {
//     //         fileFilters->append("*." + str);
//     //         if (abortLoadingModel) return endLoad(false);
//     // }
//     dir->setNameFilters(*fileFilters);
//     dir->setFilter(QDir::Files);
//     dir->setPath(currentPrimaryFolderPath);

//     imageCount = 0;
//     countInterval = 100;

//     // load file list for the current folder
//     int folderImageCount = dir->entryInfoList().size();
//     // emit centralMsg("Building image file list.\n" + QString::number(imageCount) + " found.");

//     // bail if no images and not including subfolders
//     // if (!folderImageCount && !includeSubfoldersFlag) return endLoad(true);
//     // if (!folderImageCount && !includeSubfoldersFlag) return endLoad(false);

//     // add supported images in folder to image list
//     folderCount = 1;
//     for (int i = 0; i < folderImageCount; ++i) {
//         fileInfoList.append(dir->entryInfoList().at(i));
//         imageCount++;
//         if (imageCount % countInterval == 0 && imageCount > 0) updateLoadStatus();
//         if (abortLoadingModel) return endLoad(false);
//         if (G::stop) return endLoad(false);
//     }

//     if (!includeSubfoldersFlag) {
//         includeSubfolders = false;
//         return endLoad(addFileData());
//     }

//     // if include subfolders
//     includeSubfolders = true;
//     QDirIterator it(currentPrimaryFolderPath, QDirIterator::Subdirectories);
//     while (it.hasNext()) {
//         if (abortLoadingModel) return endLoad(false);
//         it.next();
//         if (it.fileInfo().isDir() && it.fileName() != "." && it.fileName() != "..") {
//             folderCount++;
//             dir->setPath(it.filePath());
//             int folderImageCount = dir->entryInfoList().size();
//             // try next subfolder if no images in this folder
//             if (!folderImageCount) continue;
//             subFolderImagesLoaded = true;
//             // add supported images in folder to image list
//             for (int i = 0; i < folderImageCount; ++i) {
//                 if (abortLoadingModel) break;
//                 fileInfoList.append(dir->entryInfoList().at(i));
//                 imageCount++;
//                 if (imageCount == G::maxIconChunk) {
//                     if (!okManyImagesWarning()) {
//                         abortLoadingModel = true;
//                         G::includeSubfolders = false;
//                         return endLoad(false);
//                     }
//                 }
//             }
//             updateLoadStatus();
//         }
//     }
//     if (abortLoadingModel || !imageCount) return endLoad(false);

//     // huge image count
//     if (imageCount > hugeThreshold) {
//         iconChunkSize = 100;
//     }

//     // images were found and added to data model
//     return endLoad(addFileData());
// }

// bool DataModel::addFileData()
// {
// /*
//     Load the information from the operating system contained in QFileInfo

//     • PathColumn
//     • NameColumn        (core sort item)
//     • TypeColumn        (core sort item)
//     • PermissionsColumn (core sort item)  rgh?
//     • ReadWriteColumn   (core sort item)  rgh?
//     • SizeColumn        (core sort item)
//     • CreatedColumn     (core sort item)
//     • ModifiedColumn    (core sort item)
//     • PickColumn        (core sort item)
//     • IngestedColumn
//     • SearchColumn
//     • ErrColumn
// */
//     if (G::isLogger || G::isFlowLogger) G::log("DataModel::addFileData");
//     if (isDebug)
//         qDebug() << "DataModel::addFileData" << "instance =" << instance << currentPrimaryFolderPath;

//     if (combineRawJpg) {
//         // make sure, if raw+jpg pair, that raw file is first to make combining easier
//         std::sort(fileInfoList.begin(), fileInfoList.end(), lessThanCombineRawJpg);
//     }
//     else {
//         std::sort(fileInfoList.begin(), fileInfoList.end(), lessThan);
//     }

//     QString step = "Loading eligible images.\n\n";
//     QString escapeClause = "\n\nPress \"Esc\" to stop.";

//     // test if raw file to match jpg when same file names and one is a jpg
//     QString suffix;
//     QString prevRawSuffix = "";
//     QString prevRawBaseName = "";
//     QString baseName = "";
//     QModelIndex prevRawIdx;

//     sf->suspend(true);

//     int n = fileInfoList.count() + rowCount();
//     // if (n == 0) n = 1;
//     setRowCount(n);
//     setColumnCount(G::TotalColumns);

//     for (int row = 0; row < fileInfoList.count(); ++row) {
//         if (abortLoadingModel) return false;

//         // get file info
//         fileInfo = fileInfoList.at(row);
//         addFileDataForRow(row, fileInfo);

//         /* Save info for duplicated raw and jpg files, which generally are the result of
//         setting raw+jpg in the camera. The datamodel is sorted by file path, except raw files
//         with the same path precede jpg files with duplicate names. Two roles track duplicates:
//         G::DupHideRawRole flags jpg files with duplicate raws and G::DupOtherIdxRole points to
//         the duplicate other file of the pair. For example:

//         Row = 0 "G:/DCIM/100OLYMP/P4020001.ORF"  DupHideRawRole = true 	 DupOtherIdxRole = QModelIndex(1,0)
//         Row = 1 "G:/DCIM/100OLYMP/P4020001.JPG"  DupHideRawRole = false  DupOtherIdxRole = QModelIndex(0,0)  DupRawTypeRole = "ORF"
//         Row = 2 "G:/DCIM/100OLYMP/P4020002.ORF"  DupHideRawRole = true 	 DupOtherIdxRole = QModelIndex(3,0)
//         Row = 3 "G:/DCIM/100OLYMP/P4020002.JPG"  DupHideRawRole = false  DupOtherIdxRole = QModelIndex(2,0)  DupRawTypeRole = "ORF"
//         */

//         suffix = fileInfoList.at(row).suffix().toLower();
//         baseName = fileInfoList.at(row).completeBaseName();
//         if (metadata->hasJpg.contains(suffix)) {
//             // qDebug() << "DataModel::addFileData" << row << suffix;
//             prevRawSuffix = suffix;
//             prevRawBaseName = fileInfoList.at(row).completeBaseName();
//             prevRawIdx = index(row, 0);
//         }

//         QMutexLocker locker(&mutex);
//         // if row/jpg pair
//         if ((suffix == "jpg" || suffix == "jpeg") && baseName == prevRawBaseName) {
//             // hide raw version
//             setData(prevRawIdx, true, G::DupHideRawRole);
//             // set raw version other index to jpg pair
//             setData(prevRawIdx, index(row, 0), G::DupOtherIdxRole);
//             // point to raw version
//             setData(index(row, 0), prevRawIdx, G::DupOtherIdxRole);
//             // set flag to show combined JPG file for filtering when ingesting
//             setData(index(row, 0), true, G::DupIsJpgRole);
//             // build combined suffix to show in type column
//             setData(index(row, 0), prevRawSuffix.toUpper(), G::DupRawTypeRole);
//             if (combineRawJpg)
//                 setData(index(row, G::TypeColumn), "JPG+" + prevRawSuffix.toUpper());
//             else
//                 setData(index(row, G::TypeColumn), "JPG");
//         }
//     }

//     sf->suspend(false);
//     return true;
// }

void DataModel::addFileDataForRow(int row, QFileInfo fileInfo)
{
    if (G::isLogger) G::log("DataModel::addFileDataForRow", "row = " + QString::number(row));
    if (isDebug)
        qDebug() << "DataModel::addFileDataForRow"
                          << "instance =" << instance
                          << "row =" << row
                          << currentPrimaryFolderPath
                             ;
    // append hash index of datamodel row for fPath for fast lookups
    QString fPath = fileInfo.filePath();
    QString folderName = fileInfo.dir().dirName();
    QString ext = fileInfo.suffix().toLower();

    // build hash to quickly get dmRow from fPath (ie pixmap.cpp, imageCache...)
    fPathRow[fPath] = row;

    // string to hold aggregated text for searching
    QString search = fPath;

    QMutexLocker locker(&mutex);
    setData(index(row, G::RowNumberColumn), row + 1);
    setData(index(row, G::RowNumberColumn), int(Qt::AlignRight | Qt::AlignVCenter), Qt::TextAlignmentRole);
    setData(index(row, G::PathColumn), fPath, G::PathRole);
    QString tip = fPath;  //fileInfo.absoluteFilePath();
    if (showThumbNailSymbolHelp) tip += thumbnailHelp;
    setData(index(row, G::PathColumn), tip, Qt::ToolTipRole);
    setData(index(row, G::PathColumn), QRect(), G::IconRectRole);
    setData(index(row, G::PathColumn), false, G::CachedRole);
    setData(index(row, G::PathColumn), false, G::DupHideRawRole);
    setData(index(row, G::NameColumn), fileInfo.fileName());
    setData(index(row, G::NameColumn), fileInfo.fileName(), Qt::ToolTipRole);
    setData(index(row, G::FolderNameColumn), folderName);
    setData(index(row, G::TypeColumn), fileInfo.suffix().toUpper());
    QString s = fileInfo.suffix().toUpper();
    setData(index(row, G::VideoColumn), metadata->videoFormats.contains(ext));
    setData(index(row, G::VideoColumn), int(Qt::AlignCenter | Qt::AlignVCenter), Qt::TextAlignmentRole);
    uint p = static_cast<uint>(fileInfo.permissions());
    setData(index(row, G::PermissionsColumn), p);
    setData(index(row, G::PermissionsColumn), int(Qt::AlignCenter | Qt::AlignVCenter), Qt::TextAlignmentRole);
    bool isReadWrite = (p & QFileDevice::ReadUser) && (p & QFileDevice::WriteUser);
    setData(index(row, G::ReadWriteColumn), isReadWrite);
    setData(index(row, G::ReadWriteColumn), int(Qt::AlignCenter | Qt::AlignVCenter), Qt::TextAlignmentRole);
    setData(index(row, G::TypeColumn), s);
    setData(index(row, G::TypeColumn), int(Qt::AlignCenter), Qt::TextAlignmentRole);
    setData(index(row, G::SizeColumn), fileInfo.size());
    setData(index(row, G::SizeColumn), int(Qt::AlignRight | Qt::AlignVCenter), Qt::TextAlignmentRole);
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
    setData(index(row, G::MetadataAttemptedColumn), false);
    setData(index(row, G::MetadataLoadedColumn), false);
    setData(index(row, G::SearchColumn), "false");
    setData(index(row, G::SearchColumn), Qt::AlignLeft, Qt::TextAlignmentRole);
    setData(index(row, G::SearchTextColumn), search);
    setData(index(row, G::SearchTextColumn), search, Qt::ToolTipRole);
}

bool DataModel::updateFileData(QFileInfo fileInfo)
{
    if (G::isLogger) G::log("DataModel::updateFileData");
    qDebug() << "DataModel::updateFileData" << "Instance =" << instance << currentPrimaryFolderPath;
    QString fPath = fileInfo.filePath();
    if (!fPathRow.contains(fPath)) return false;
    int row = fPathRow[fPath];
    if (!index(row,0).isValid()) return false;

    QMutexLocker locker(&mutex);
    setData(index(row, G::SizeColumn), fileInfo.size());
    setData(index(row, G::SizeColumn), int(Qt::AlignRight | Qt::AlignVCenter), Qt::TextAlignmentRole);
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

    int sfRow = proxyRowFromPath(fPath);
    int row = fPathRow[fPath];
    if (!index(row,0).isValid()) return m;

    if (isDebug) qDebug() << "DataModel::imMetadata" << "instance =" << instance
                          << "row =" << row
                          << currentPrimaryFolderPath;

    QMutexLocker locker(&mutex);
    metadata->m.row = row;  // req'd?

    // file info (calling Metadata not required)
    m.row = sfRow;
    m.currRootFolder = G::currRootFolder;
    m.fPath = fPath;
    m.fName = index(row, G::NameColumn).data().toString();
    m.type = index(row, G::TypeColumn).data().toString();
    m.size = index(row, G::SizeColumn).data().toInt();
    m.video = index(row, G::VideoColumn).data().toInt();
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

    // if file type metadata is not supported then finished
//    if (!metadata->hasMetadataFormats.contains(m.type.toLower())) {
//        if (updateInMetadata) metadata->m = m;
//        return m;
//    }

    // Information generated by calling Metadata
    bool success = false;

    // check if metadata loaded for this row
    if (index(row, G::MetadataLoadedColumn).data().toBool()) {
        success = true;
    }
    else {
        QFileInfo fileInfo(fPath);
        if (metadata->loadImageMetadata(fileInfo, instance, true, true, false, true, "DataModel::imMetadata")) {
            m.metadataLoaded = true;
            addMetadataForItem(metadata->m, "DataModel::imMetadata");
            success = true;
        }
    }

    if (!success) {
         m.metadataLoaded = true;
        if (metadata->hasMetadataFormats.contains(m.type.toLower())) {
             errMsg = "Metadata not loaded to model.";
             G::issue("Warning", errMsg, "DataModel::imMetadata", row, fPath);
        }
        //mutex.unlock();
        return m;
    }

    m.pick  = index(row, G::PickColumn).data().toBool();
    m.ingested  = index(row, G::IngestedColumn).data().toBool();
    m.metadataAttempted = index(row, G::MetadataAttemptedColumn).data().toBool();
    m.metadataLoaded = index(row, G::MetadataLoadedColumn).data().toBool();
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
    m.focusX = index(row, G::FocusXColumn).data().toInt();
    m.focusY = index(row, G::FocusYColumn).data().toInt();
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
        qDebug() << "DataModel::addAllMetadata" << "instance =" << instance << currentPrimaryFolderPath;
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
        if (abortLoadingModel || G::dmEmpty) {
            endLoad(false);
            setAllMetadataLoaded(false);
            return;
        }
        // is metadata already cached (or attempted?)
        if (index(row, G::MetadataLoadedColumn).data().toBool()) continue;

        QString fPath = index(row, 0).data(G::PathRole).toString();
        QFileInfo fileInfo(fPath);
        QString ext = fileInfo.suffix().toLower();
        if (metadata->loadImageMetadata(fileInfo, instance, true, true, false, true, "DataModel::addAllMetadata")) {
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
    setAllMetadataLoaded(true);
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
                          << currentPrimaryFolderPath;
    }

    // might be called from previous folder during folder change
    if (instance != this->instance) {
        errMsg = "Instance clash.";
        G::issue("Comment", errMsg, fun, row);
        return true;
    }
    if (G::stop) return false;

    QString fPath = index(row, 0).data(G::PathRole).toString();

    // load metadata
    /*
     qDebug() << "DataModel::readMetadataForItem"
              << "Metadata loaded ="
              << index(row, G::MetadataLoadedColumn).data().toBool()
              << fPath;//*/

    if (!index(row, G::MetadataLoadedColumn).data().toBool()) {
        QFileInfo fileInfo(fPath);

        // only read metadata from files that we know how to
        QString ext = fileInfo.suffix().toLower();
        if (metadata->hasMetadataFormats.contains(ext)) {
            //qDebug() << "DataModel::readMetadataForItem" << fPath;
            if (metadata->loadImageMetadata(fileInfo, instance, true, true, false, true, "DataModel::readMetadataForItem")) {
                metadata->m.row = row;
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
                          << currentPrimaryFolderPath;

    // might be called from previous folder during folder change
    if (instance != this->instance) {
        errMsg = "Instance clash.";
        G::issue("Comment", errMsg, fun, sfRow);
        return true;
    }
    if (G::stop) return false;

    QString fPath = sf->index(sfRow, 0).data(G::PathRole).toString();

    // load metadata
    /*
     qDebug() << "DataModel::refreshMetadataForItem"
              << "Metadata loaded ="
              << sf->index(row, G::MetadataLoadedColumn).data().toBool()
              << fPath; //*/

    QFileInfo fileInfo(fPath);

    // only read metadata from files that we know how to
    QString ext = fileInfo.suffix().toLower();
    if (metadata->hasMetadataFormats.contains(ext)) {
        //qDebug() << "DataModel::readMetadataForItem" << fPath;
        if (metadata->loadImageMetadata(fileInfo, instance, true, true, false, true, "DataModel::readMetadataForItem")) {
            metadata->m.row = sfRow;
            addMetadataForItem(metadata->m, "DataModel::readMetadataForItem");
        }
        else {
            errMsg = "Failed to load metadata.";
            G::issue("Warning", errMsg, fun, sfRow, fPath);
            mutex.unlock();
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

bool DataModel::addMetadataAndIconForItem(ImageMetadata m, QModelIndex dmIdx, const QPixmap &pm,
                                     int fromInstance, QString src)
{
    if (G::isLogger) G::log("DataModel::addMetadataForItem");
    qDebug() << "DataModel::addMetadataAndIconForItem" << m.row;
    addMetadataForItem(m, src);
    setIcon(dmIdx, pm,fromInstance, src);
    return true;
}

bool DataModel::addMetadataForItem(ImageMetadata m, QString src)
{
/*
    This function is called after the metadata for each eligible image in the selected
    folder(s) has been cached or when addAllMetadata is called prior of filtering or sorting.
    The metadata is displayed in tableView and InfoView.

    If a folder is opened with combineRawJpg all the metadata for the raw file may not have
    been loaded, but editable data, (such as rating, label, title, email, url) may have been
    edited in the jpg file of the raw+jpg pair. If so, we do not want to overwrite this data.
*/
    mCopy = m;
    if (G::isLogger) G::log("DataModel::addMetadataForItem");
    /*
    qDebug() << "DataModel::addMetadataForItem"
             << "Instance =" << instance
             << "m.instance =" << m.instance
             << currentFolderPath;
    //*/
    rowCountChk = rowCount();
    if (G::stop) return false;
    if (isDebug) qDebug() << "DataModel::addMetadataForItem" << "instance =" << instance
                          << "row =" << m.row
                          << currentPrimaryFolderPath;

    // deal with lagging signals when new folder selected suddenly
    if (instance > -1 && m.instance != instance) {
        if (G::showIssueInConsole)
        errMsg = "Instance clash from " + src;
        G::issue("Comment", errMsg, "DataModel::addMetadataForItem", m.row);        return false;
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

    mLock = true;
    setData(index(row, G::SearchColumn), m.isSearch);
    setData(index(row, G::SearchColumn), Qt::AlignCenter, Qt::TextAlignmentRole);
    setData(index(row, G::LabelColumn), m.label);
    setData(index(row, G::LabelColumn), Qt::AlignCenter, Qt::TextAlignmentRole);
    search += m.label;
    setData(index(row, G::_LabelColumn), m._label);
    setData(index(row, G::RatingColumn), m.rating);
    if (m.rating == "0") m.rating = "";
    setData(index(row, G::RatingColumn), Qt::AlignCenter, Qt::TextAlignmentRole);
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

//    setData(index(row, G::CreatedColumn), m.createdDate.toString("yyyy-MM-dd hh:mm:ss"));
//    setData(index(row, G::YearColumn), m.createdDate.toString("yyyy"));
//    setData(index(row, G::DayColumn), m.createdDate.toString("yyyy-MM-dd"));
//    search += m.createdDate.toString("yyyy-MM-dd");

    setData(index(row, G::WidthColumn), QString::number(m.width));
    setData(index(row, G::WidthColumn), Qt::AlignCenter, Qt::TextAlignmentRole);
    setData(index(row, G::HeightColumn), QString::number(m.height));
    setData(index(row, G::HeightColumn), Qt::AlignCenter, Qt::TextAlignmentRole);
    setData(index(row, G::AspectRatioColumn), QString::number((aspectRatio(m.width, m.height, m.orientation)), 'f', 2));

    // QString msg = "row = " + QString::number(row) +
    //               " width = " + QString::number(m.width);
    // G::log("DataModel::addMetadataForItem", msg);

    /*
    double ar = aspectRatio(m.width, m.height, m.orientation);
    qDebug().noquote()
             << "DataModel::addMetadataForItem"
             << QString::number(row).rightJustified(4)
             << "aspect ratio =" << QString::number(ar, 'f', 2)
             << "\tw =" << QString::number(m.width).rightJustified(4)
             << "h =" << QString::number(m.height).rightJustified(4)
             << "orientation =" << m.orientation
             << m.fPath;
    //*/
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
    setData(index(row, G::MetadataAttemptedColumn), m.metadataAttempted);
    setData(index(row, G::MetadataLoadedColumn), m.metadataLoaded);
    setData(index(row, G::MissingThumbColumn), m.isEmbeddedThumbMissing);
    setData(index(row, G::CompareColumn), m.compare);
    setData(index(row, G::SearchTextColumn), search.toLower());
    setData(index(row, G::SearchTextColumn), search.toLower(), Qt::ToolTipRole);

    // image cache helpers
    setData(index(row, G::IsTargetColumn), false);
    setData(index(row, G::IsCachingColumn), false);
    setData(index(row, G::IsCachedColumn), false);
    setData(index(row, G::AttemptsColumn), 0);
    setData(index(row, G::DecoderIdColumn), -1);
    setData(index(row, G::DecoderReturnStatusColumn), 0);
    // calc size in MB req'd to store image in cache
    int w, h;
    m.widthPreview > 0 ? w = m.widthPreview : w = m.width;
    m.heightPreview > 0 ? h = m.heightPreview : h = m.height;
    // 8 bits X 3 channels + 8 bit depth = (32*w*h)/8/1024/1024 = w*h/262144
    float sizeMB;
    if (w == 0 || h == 0) sizeMB = m.size;
    else sizeMB = static_cast<float>(w * h * 1.0 / 262144);
    setData(index(row, G::CacheSizeColumn), sizeMB);

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
    return true;
}

bool DataModel::metadataLoaded(int dmRow)
{
    if (G::isLogger) G::log("DataModel::metadataLoaded");
    if (isDebug) qDebug() << "DataModel::metadataLoaded" << "instance =" << instance
                          << "row =" << dmRow
                          << currentPrimaryFolderPath;
    return index(dmRow, G::MetadataLoadedColumn).data().toBool();
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
        setValueSf(sfIdx, v, instance, "DataModel::processErr");
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

bool DataModel::missingThumbnails()
{
    for (int row = 0; row < sf->rowCount(); row++) {
        if (index(row, G::MissingThumbColumn).data().toBool()) {
            folderHasMissingEmbeddedThumb = true;
            return true;
        }
    }
    folderHasMissingEmbeddedThumb = false;
    return false;
}

double DataModel::aspectRatio(int w, int h, int orientation)
{
    if (G::isLogger) G::log("DataModel::aspectRatio");
    if (isDebug) qDebug() << "DataModel::aspectRatio" << "instance =" << instance << currentPrimaryFolderPath;
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
    QMutexLocker locker(&mutex);
    // range check in case model has changed, return invalid result
    if (row >= sf->rowCount()) return QVariant();
    return sf->index(row, column).data(role);
}

void DataModel::setValue(QModelIndex dmIdx, QVariant value, int instance,
                         QString src, int role, int align)
{
    if (G::stop) return;
    if (isDebug) {
        qDebug() << "DataModel::setValue"
                 << "row =" << dmIdx.row()
                     << "value =" << value
                 << "src =" << src
                 << "instance =" << instance
                 << currentPrimaryFolderPath;
    }
    if (instance != this->instance) {
        errMsg = "Instance clash from " + src;
        G::issue("Comment", errMsg, "DataModel::setValuePath", dmIdx.row());
        return;
    }

    if (!dmIdx.isValid()) {
        errMsg = "Invalid dmIdx.  Src: " + src;
        G::issue("Warning", errMsg, "DataModel::setValue", dmIdx.row());
        return;
    }
    QMutexLocker locker(&mutex);
    setData(dmIdx, value, role);
    setData(dmIdx, align, Qt::TextAlignmentRole);
}

void DataModel::setValueSf(QModelIndex sfIdx, QVariant value, int instance,
                           QString src, int role, int align)
{
    if (G::stop) return;
    if (isDebug) qDebug() << "DataModel::setValueSf" << "instance =" << instance
                          << "row =" << sfIdx.row()
                          << currentPrimaryFolderPath;
    /*
    qDebug() << "DataModel::setValueSf"
             << "Instance =" << instance
             << "this Instance =" << this->instance
             << "G::stop =" << G::stop
             << "src =" << src
             << "sfIdx =" << sfIdx
             << "rowCount() =" << rowCount()
             << "value =" << value
             << currentFolderPath;
    //*/

    QMutexLocker locker(&mutex);

    if (instance != this->instance) {
        errMsg = "Instance clash from " + src;
        G::issue("Comment", errMsg, "DataModel::setValueSF", sfIdx.row());
        return ;
    }
    if (!sfIdx.isValid()) {
        errMsg = "Invalid sfIdx.  Src: " + src;
        G::issue("Warning", errMsg, "DataModel::setValueSF", sfIdx.row());
        return;
    }
    if (sfIdx.row() > sf->rowCount() - 1) {
        errMsg = "Index out of range " + src;
        G::issue("Comment", errMsg, "DataModel::setValueSF", sfIdx.row());
        return ;
    }

    sf->setData(sfIdx, value, role);
    setData(sfIdx, align, Qt::TextAlignmentRole);
}

void DataModel::setCurrentSF(QModelIndex sfIdx, int instance)
{
    if (instance != this->instance) {
        errMsg = "Instance clash.";
        G::issue("Comment", errMsg, "DataModel::setCurrent", sfIdx.row());
        return;
    }

    // update current index parameters
    QMutexLocker locker(&mutex);
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
}

void DataModel::setCurrent(QModelIndex dmIdx, int instance)
{
    if (instance != this->instance) {
        errMsg = "Instance clash.";
        G::issue("Comment", errMsg, "DataModel::setCurrent", dmIdx.row());
        return;
    }

    // update current index parameters
    QMutexLocker locker(&mutex);
    QModelIndex sfIdx = sf->mapFromSource(dmIdx);
    currentSfIdx = sfIdx;
    currentSfRow = sfIdx.row();
    currentDmIdx = dmIdx;
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
}

void DataModel::setValuePath(QString fPath, int col, QVariant value, int instance, int role)
{
    if (isDebug) {
        qDebug() << "DataModel::setValuePath" << "instance =" << instance
                 << "col =" << col
                 << "fPath =" << fPath
                 << "fPathRow[fPath] =" << fPathRow[fPath]
                ;
    }
    QModelIndex dmIdx = index(fPathRow[fPath], col);
    if (instance != this->instance) {
        errMsg = "Instance clash.";
        G::issue("Comment", errMsg, "DataModel::setValuePath", dmIdx.row(), fPath);
        return;
    }
    if (G::stop) return;
    if (!dmIdx.isValid()) {
        errMsg = "Invalid dmIdx.";
        G::issue("Warning", errMsg, "DataModel::setValuePath", dmIdx.row(), fPath);
        return;
    }
    QMutexLocker locker(&mutex);
    setData(dmIdx, value, role);
}

void DataModel::setIconFromVideoFrame(QModelIndex dmIdx, QPixmap &pm, int fromInstance,
                                      qint64 duration)
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
                 << "row =" << dmIdx.row()
                 << "instance =" << instance
                 << "fromInstance =" << fromInstance
                 << dmIdx.data(G::PathRole).toString()
                 << "\n"
        ;

    if (G::stop) return;
    if (!dmIdx.isValid()) {
        errMsg = "Invalid dmIdx.";
        G::issue("Warning", errMsg, "DataModel::setIconFromVideoFrame");
        return;
    }
    if (fromInstance != instance) {
        errMsg = "Instance clash.";
        G::issue("Comment", errMsg, "DataModel::setIconFromVideoFrame", dmIdx.row());
        return;
    }
    //qDebug() << "DataModel::setIconFromVideoFrame" << "Instance =" << instance << currentFolderPath;

    int row = dmIdx.row();
    //qDebug() << "DataModel::setIconFromVideoFrame       row =" << row;

    QMutexLocker locker(&mutex);
    QString modelDuration = index(dmIdx.row(), G::DurationColumn).data().toString();
    if (modelDuration == "") {
        duration /= 1000;
        QTime durationTime((duration / 3600) % 60, (duration / 60) % 60,
            duration % 60, (duration * 1000) % 1000);
        QString format = "mm:ss";
        if (duration > 3600) format = "hh:mm:ss";
        setData(index(row, G::DurationColumn), durationTime.toString(format));
    }
    //qDebug() << "DataModel::setIconFromVideoFrame  itemFromIndex" << dmIdx;
    QStandardItem *item = itemFromIndex(dmIdx);
    if (itemFromIndex(dmIdx)->icon().isNull()) {
        if (item != nullptr) {
            item->setIcon(pm);
            // set aspect ratio for video
            if (pm.height() > 0) {
                QString aspectRatio = QString::number(pm.width() * 1.0 / pm.height(), 'f', 2);
                setData(index(row, G::AspectRatioColumn), aspectRatio);
            }
            //setIconMax(pm);
        }
    }
}

void DataModel::setIcon(QModelIndex dmIdx, const QPixmap &pm, int fromInstance, QString src)
{
/*
    setIcon is a slot that can be signalled from another thread.  If the user is rapidly
    changing folders it is possible to receive a delayed signal from the previous folder.
    To prevent this, the datamodel instance is incremented every time a new folder is
    loaded, and this is checked against the signal instance.

    In addition, the signal queue from MetaRead is cleared in MW::stop to prevent
    lagging calls when the folder has been changed.  This probably makes the instance
    checking, which was not totally reliable, to no longer be required.  Keeping it for
    now.

    This function is subject to potential race conditions, so it is critical that it only
    be called via a connection with Qt::BlockingQueuedConnection.
*/
    QMutexLocker locker(&mutex);
    if (G::isLogger) G::log("DataModel::setIcon");
    if (isDebug)
    {
        qDebug() << "DataModel::setIcon"
                 << "src =" << src
                 << "instance =" << instance
                 << "fromInstance =" << fromInstance
                 << "row =" << dmIdx.row()
                 << currentPrimaryFolderPath;
    }
    if (fromInstance != instance) {
        errMsg = "Instance clash from " + src;
        G::issue("Comment", errMsg, "DataModel::setIcon", dmIdx.row());
        return;
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

    const QVariant vIcon = QVariant(QIcon(pm));
    setData(dmIdx, vIcon, Qt::DecorationRole);
    // if (dmIdx.row() > 8500)
    //     qDebug() << "DataModel::setIcon" << dmIdx.row();
}

bool DataModel::iconLoaded(int sfRow, int instance)
{
    QMutexLocker locker(&mutex);
    if (G::isLogger) G::log("DataModel::iconLoaded");
    if (isDebug) qDebug() << "DataModel::iconLoaded" << "instance =" << this->instance
                          << "fromInstance =" << instance
                          << "row =" << sfRow
                          << currentPrimaryFolderPath;
    // might be called from previous folder during folder change
    if (instance != this->instance) {
        errMsg = "Instance clash.";
        G::issue("Comment", errMsg, "DataModel::iconLoaded", sfRow);
        return true;
    }
    if (sfRow >= sf->rowCount()) return true;
    QModelIndex dmIdx = sf->mapToSource(sf->index(sfRow, 0));
    if (dmIdx.isValid()) return !(itemFromIndex(dmIdx)->icon().isNull());
    else return false;
}

int DataModel::iconCount()
{
    if (isDebug) qDebug() << "DataModel::iconCount" << "instance =" << instance << currentPrimaryFolderPath;
    int count = 0;
    for (int row = 0; row < rowCount(); ++row) {
//        qDebug() << "DataModel::iconCount  itemFromIndex  row =" << row;
        if (!itemFromIndex(index(row, 0))->icon().isNull()) count++;
    }
    return count;
}

bool DataModel::isAllIconsLoaded()
{
    if (isDebug) qDebug() << "DataModel::allIconsLoaded" << "instance =" << instance << currentPrimaryFolderPath;
    for (int row = 0; row < rowCount(); ++row) {
        if (itemFromIndex(index(row, 0))->icon().isNull()) {
            //qDebug() << "DataModel::allIconChunkLoaded  false for row =" << row;
            return false;
        }
    }
    return true;

}

bool DataModel::isAllIconChunkLoaded(int first, int last)
{
    if (isDebug) qDebug() << "DataModel::allIconChunkLoaded" << "instance =" << instance << currentPrimaryFolderPath;

    if (first < 0 || last > sf->rowCount()) return false;
    for (int row = first; row <= last; ++row) {
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
        // if (itemFromIndex(sf->index(row, 0))->icon().isNull()) {
        //     //qDebug() << "DataModel::allIconChunkLoaded  false for row =" << row;
        //     return false;
        // }
    }
    // qDebug() << "DataModel::allIconChunkLoaded = true";
    return true;

}

bool DataModel::isIconRangeLoaded()
/*
    Called by MetaRead2::dispatch when aIsDone && bIsDone (readers have been dispatched
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
    Called by MW::load and MW::updateIconRange.
*/
{
    // if (iconChunkSize >= rowCount()) {
    //     G::iconChunkLoaded = true;
    //     return;
    // }
    // row count less than icon range
    int rows = sf->rowCount();
    int start;
    int end;
    start = sfRow - iconChunkSize / 2;
    if (start < 0) start = 0;
    end = start + iconChunkSize;
    if (end >= rows) end = rows - 1;
    // G::iconChunkLoaded = start >= startIconRange && end <= endIconRange;
    mutex.lock();
    startIconRange = start;
    endIconRange = end;
    mutex.unlock();
    G::iconChunkLoaded = isAllIconChunkLoaded(startIconRange, endIconRange);
}

void DataModel::setChunkSize(int chunkSize)
{
    iconChunkSize = chunkSize;
    checkChunkSize = chunkSize > rowCount();
}

void DataModel::clearAllIcons() // not being used
{
    if (isDebug) qDebug() << "DataModel::clearAllIcons" << "instance =" << instance << currentPrimaryFolderPath;
    QMutexLocker locker(&mutex);
    for (int row = 0; row < rowCount(); ++row) {
        setData(index(row, 0), QVariant(), Qt::DecorationRole);
    }
}

void DataModel::setAllMetadataLoaded(bool isLoaded)
{
    // if (isDebug)
        qDebug() << "DataModel::setAllMetadataLoaded" << "instance =" << instance << currentPrimaryFolderPath;
    G::allMetadataLoaded = isLoaded;
}

bool DataModel::isMetadataAttempted(int sfRow)
{
    if (isDebug) qDebug() << "DataModel::isMetadataAttempted" << "instance =" << instance << currentPrimaryFolderPath;
    return index(sfRow, G::MetadataAttemptedColumn).data().toBool();
}

bool DataModel::isMetadataLoaded(int sfRow)
{
    if (isDebug) qDebug() << "DataModel::isMetadataLoaded" << "instance =" << instance << currentPrimaryFolderPath;
    return index(sfRow, G::MetadataLoadedColumn).data().toBool();
}

bool DataModel::isAllMetadataAttempted()
{
    if (isDebug) qDebug() << "DataModel::isAllMetadataLoaded" << "instance =" << instance << currentPrimaryFolderPath;
    for (int row = 0; row < rowCount(); ++row) {
        if (!index(row, G::MetadataAttemptedColumn).data().toBool()) {
            if (isDebug)
            qDebug() << "DataModel::isAllMetadataAttempted" << "row =" << row << "is not loaded.";
            return false;
        }
    }
    return true;
}

bool DataModel::isAllMetadataLoaded()
{
    if (isDebug) qDebug() << "DataModel::isAllMetadataLoaded" << "instance =" << instance << currentPrimaryFolderPath;
    for (int row = 0; row < rowCount(); ++row) {
        if (!index(row, G::MetadataLoadedColumn).data().toBool()) {
            if (isDebug)
            qDebug() << "DataModel::isAllMetadataLoaded" << "row =" << row << "is not loaded.";
            return false;
        }
    }
    return true;
}

QList<int> DataModel::metadataNotLoaded()
{
    if (isDebug) qDebug() << "DataModel::metadataNotLoaded" << "instance =" << instance << currentPrimaryFolderPath;
    QList<int> rows;
    for (int row = 0; row < rowCount(); ++row) {
        if (index(row, G::MetadataAttemptedColumn).data().toBool() &&
            !index(row, G::MetadataLoadedColumn).data().toBool())
        {
            if (isDebug)
            qDebug() << "DataModel::metadataNotLoaded" << "row =" << row << "is not loaded.";
            rows << row;
        }
    }
    return rows;
}

bool DataModel::isPath(QString fPath)
{
    if (G::isLogger) G::log("DataModel::rowFromPath");
    for (int row = 0; row < rowCount(); ++row) {
        if (fPath == index(row, 0).data(G::PathRole).toString())
            return true;
    }
    return false;
}

int DataModel::rowFromPath(QString fPath)
{
    if (isDebug) qDebug() << "DataModel::rowFromPath" << "instance =" << instance << fPath << currentPrimaryFolderPath;
    if (G::isLogger) G::log("DataModel::rowFromPath");
    if (fPathRow.contains(fPath)) return fPathRow[fPath];
    else return -1;
}

int DataModel::proxyRowFromPath(QString fPath)
{
    if (isDebug) qDebug() << "DataModel::proxyRowFromPath" << "instance =" << instance << fPath << currentPrimaryFolderPath;
    if (G::isLogger) G::log("DataModel::proxyRowFromPath");
    int dmRow;
    int sfRow = -1;
    if (fPathRow.contains(fPath)) {
        dmRow = fPathRow[fPath];
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

void DataModel::rebuildRowFromPathHash()
{
    if (G::isLogger) G::log("DataModel::refreshRowFromPath");
    if (isDebug) qDebug() << "DataModel::refreshRowFromPath" << "instance =" << instance << currentPrimaryFolderPath;
    fPathRow.clear();
    for (int row = 0; row < rowCount(); ++row) {
        QString fPath = index(row, G::PathColumn).data(G::PathRole).toString();
        fPathRow[fPath] = row;
    }
}

bool DataModel::hasFolderChanged()
{
/*
    Called from MW::refreshCurrentFolder.  The list of eligible files is read and compared to
    the datamodel.  If the count has changed then return false.  If the count has not changed
    compare the last modified datetime for each file.  If a file has been modified since the
    datamodel was loaded then it is added to the modifiedFiles list and return false.
*/
    if (G::isLogger) G::log("DataModel::hasFolderChanged");
    // if (isDebug)
        qDebug() << "DataModel::hasFolderChanged"
                 << "instance =" << instance
                 << folderList;

    bool hasChanged = false;
    modifiedFiles.clear();
    int currentEligibleFileCount = 0;

    // list of all eligible files in datamodel folderList
    // QList<QFileInfo> fileInfoList2;
    foreach(QString folderPath, folderList) {
        QDir d;
        d.setPath(folderPath);
        d.setNameFilters(*fileFilters);
        d.setFilter(QDir::Files);
        currentEligibleFileCount += d.entryInfoList().size();
        // for (int i = 0; i < d.entryInfoList().size(); ++i) {
        // }
    }

    // check eligible file counts match
    int oldCount = rowCount();
    // int newCount = fileInfoList2.count();
    if (currentEligibleFileCount < oldCount) return true;

    // check eligible file last modified dates match
    bool isFileModification = false;
    for (int i = 0; i < rowCount(); ++i) {
        QDateTime t1 = index(i, G::ModifiedColumn).data().toDateTime();
        // get current
        QString path = index(i, G::PathColumn).data(G::PathRole).toString();
        QFileInfo info = QFileInfo(path);
        QDateTime t2 = info.lastModified();
        if (t1 != t2) {
            hasChanged = true;
            isFileModification = true;
            modifiedFiles.append(info);
            /*
            qDebug() << "DataModel::hasFolderChanged" << fileInfoList2.at(i).fileName()
                     << "modified at" << t2;
                     //*/
        }
    }
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
                  << "searchString =" << searchString
                  << currentPrimaryFolderPath;
    // update datamodel search string match
    QMutexLocker locker(&mutex);
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
    if (isDebug) qDebug() << "DataModel::rebuildTypeFilter" << "instance =" << instance << currentPrimaryFolderPath;
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

QModelIndex DataModel::proxyIndexFromPath(QString fPath)
{
/*
    The hash table fPathRow {path, row} is build when the datamodel is loaded to provide a
    quick lookup to get the datamodel row from an image path.
*/
    if (G::isLogger) G::log("DataModel::proxyIndexFromPath");
    if (isDebug) {
        qDebug() << "DataModel::proxyIndexFromPath" << "instance =" << instance
                 << "fPath =" << fPath
                 << currentPrimaryFolderPath;
    }
    if (!fPathRow.contains(fPath)) {
        errMsg = "Not in fPathrow.";
        G::issue("Warning", errMsg, "DataModel::proxyIndexFromPath", -1, fPath);
        if (G::isFileLogger) Utilities::log("MW::proxyIndexFromPath", "Not in fPathrow: " + fPath);
        return index(-1, -1);
    }
    int dmRow = fPathRow[fPath];
    QModelIndex sfIdx = sf->mapFromSource(index(dmRow, 0));
    if (sfIdx.isValid()) {
        return sfIdx;
    }
    else {
        errMsg = "Invalid proxy.";
        G::issue("Warning", errMsg, "DataModel::proxyIndexFromPath", dmRow, fPath);
        return index(-1, -1);       // invalid index
    }

    /* deprecated code
    QModelIndexList idxList = sf->match(sf->index(0, 0), G::PathRole, fPath);
    if (idxList.size() > 0 && idxList[0].isValid()) {
        return idxList[0];
    }
    return index(-1, -1);       // invalid index
    */
}

QModelIndex DataModel::proxyIndexFromModelIndex(QModelIndex dmIdx)
{
    return sf->mapFromSource(dmIdx);
}

int DataModel::proxyRowFromModelRow(int dmRow)
{
    if (G::isLogger) G::log("DataModel::proxyRowFromModelRow");
    if (isDebug) qDebug() << "DataModel::proxyRowFromModelRow" << "instance =" << instance
                          << "row =" << dmRow
                          << currentPrimaryFolderPath;
    return sf->mapFromSource(index(dmRow, 0)).row();
}

int DataModel::modelRowFromProxyRow(int sfRow)
{
    if (G::isLogger) G::log("DataModel::modelRowFromProxyRow");
    if (isDebug) qDebug() << "DataModel::modelRowFromProxyRow" << "instance =" << instance
                          << "row =" << sfRow
                          << currentPrimaryFolderPath;
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

bool DataModel::getSelection(QStringList &list)
{
/*
    Adds each image that is selected or picked as a file path to list. If there
    are picks and a selection then a dialog offers the user a choice to use.
*/
    if (G::isLogger) G::log("DataModel::getSelection");
    if (isDebug) qDebug() << "DataModel::getSelection" << "instance =" << instance << currentPrimaryFolderPath;

    bool usePicks = false;

    // nothing picked or selected
    if (isPick() && selectionModel->selectedRows().size() == 0) {
        G::popUp->showPopup("Oops.  There are no picks or selected images.", 2000);
        return false;
    }

    // picks = selection
    bool picksEqualsSelection = true;
    for (int row = 0; row < sf->rowCount(); row++) {
        bool isPicked = sf->index(row, G::PickColumn).data(Qt::EditRole).toString() == "true";
        bool isSelected = selectionModel->isSelected(sf->index(row, 0));
        if (isPicked != isSelected) {
            picksEqualsSelection = false;
            break;
        }
    }

    if (!picksEqualsSelection) {
        // use picks or selected
        if (isPick() && selectionModel->selectedRows().size() > 1) {
            SelectionOrPicksDlg::Option option;
            SelectionOrPicksDlg dlg(option);
            dlg.exec();
            if (option == SelectionOrPicksDlg::Option::Cancel) return false;
            if (option == SelectionOrPicksDlg::Option::Picks) usePicks = true;
        }
        else if (isPick()) usePicks = true;
    }

    if (usePicks) {
        for (int row = 0; row < sf->rowCount(); row++) {
            if (sf->index(row, G::PickColumn).data(Qt::EditRole).toString() == "true") {
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

QStringList DataModel::getSelectionOrPicks()
{
    if (G::isLogger) G::log("DataModel::getSelectionOrPicks");
    if (isDebug) qDebug() << "DataModel::getSelectionOrPicks" << "instance =" << instance << currentPrimaryFolderPath;

    QStringList picks;

    // build QStringList of picks
    if (isPick()) {
        for (int row = 0; row < sf->rowCount(); ++row) {
            QModelIndex pickIdx = sf->index(row, G::PickColumn);
            QModelIndex idx = sf->index(row, 0);
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
            QModelIndex idx = sf->index(row, 0);
            picks << idx.data(G::PathRole).toString();
        }
    }

    return picks;
}

bool DataModel::isPick()
{
    if (G::isLogger) G::log("DataModel::isPick");
    if (isDebug) qDebug() << "DataModel::isPick" << "instance =" << instance << currentPrimaryFolderPath;
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
    if (isDebug) qDebug() << "DataModel::clearPicks" << "instance =" << instance << currentPrimaryFolderPath;
    QMutexLocker locker(&mutex);
    for (int row = 0; row < sf->rowCount(); row++) {
        setData(index(row, G::PickColumn), "false");
    }
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
    QString row2 = "<tr><td><center>" + yellowMedBullet + "</center></td><td>Missing embedded thumbnail</td></tr>";
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
            row2 +
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
        setData(dmIdx, tip, Qt::ToolTipRole);
    }
}

QString DataModel::diagnostics()
{
    if (G::isLogger) G::log("DataModel::diagnostics");
    if (isDebug) qDebug() << "DataModel::diagnostics" << "instance =" << instance << currentPrimaryFolderPath;
    QString reportString;
    QTextStream rpt;
    rpt.setString(&reportString);
    rpt << Utilities::centeredRptHdr('=', "DataModel Diagnostics");
    rpt << "\n";
    rpt << "\n" << G::sj("currentPrimaryFolderPath", 27) << G::s(currentPrimaryFolderPath);
    rpt << "\n" << G::sj("firstFolderPathWithImages", 27) << G::s(firstFolderPathWithImages);
    rpt << "\n" << G::sj("currentFilePath", 27) << G::s(currentFilePath);
    rpt << "\n" << G::sj("currentDMRow", 27) << G::s(currentDmRow);
    rpt << "\n" << G::sj("currentSFRow", 27) << G::s(currentSfRow);
    rpt << "\n" << G::sj("firstVisibleRow", 27) << G::s(currentSfRow);
    rpt << "\n" << G::sj("lastVisibleRow", 27) << G::s(currentSfRow);
    rpt << "\n" << G::sj("startIconRange", 27) << G::s(currentSfRow);
    rpt << "\n" << G::sj("endIconRange", 27) << G::s(currentSfRow);
    rpt << "\n" << G::sj("iconChunkSize", 27) << G::s(currentSfRow);
    rpt << "\n" << G::sj("hasDupRawJpg", 27) << G::s(hasDupRawJpg);
    rpt << "\n" << G::sj("loadingModel", 27) << G::s(hasDupRawJpg);
    rpt << "\n" << G::sj("basicFileInfoLoaded", 27) << G::s(hasDupRawJpg);
    rpt << "\n" << G::sj("filtersBuilt", 27) << G::s(filters->filtersBuilt);
    rpt << "\n" << G::sj("timeToQuit", 27) << G::s(abortLoadingModel);
    rpt << "\n" << G::sj("imageCount", 27) << G::s(imageCount);
    rpt << "\n" << G::sj("dmRowCount", 27) << G::s(rowCount());
    rpt << "\n" << G::sj("sfRowCount", 27) << G::s(sf->rowCount());
    rpt << "\n" << G::sj("countInterval", 27) << G::s(countInterval);
    for(int row = 0; row < rowCount(); row++) {
        rpt << "\n";
        getDiagnosticsForRow(row, rpt);
    }
    rpt << "\n\n" ;

    // list fPathRow hash
    QMap<int,QString> rowMap;
    rpt << "fPathRow hash:\n";
    for (auto i = fPathRow.begin(), end = fPathRow.end(); i != end; ++i)
        rowMap.insert(i.value(), i.key());
    for (int i = 0; i < rowMap.count(); i++)
        rpt << i << "\t" << rowMap[i]<< "\n";

        //rpt << i.value() << "\t" << i.key()  << "\n";

    return reportString;
}

QString DataModel::diagnosticsForCurrentRow()
{
    if (G::isLogger) G::log("DataModel::diagnosticsForCurrentRow");
    if (isDebug) qDebug() << "DataModel::diagnosticsForCurrentRow" << "instance =" << instance << currentPrimaryFolderPath;

    if (rowCount() == 0) {
        G::popUp->showPopup("Empty folder or no folder selected");
        return "";
    }

    if (currentSfRow < 0 || currentSfRow >= rowCount()) {
        G::popUp->showPopup("Invalid row " + QString::number(currentSfRow));
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
    if (isDebug) qDebug() << "DataModel::getDiagnosticsForRow" << "instance =" << instance << currentPrimaryFolderPath;

    if (rowCount() == 0) {
        G::popUp->showPopup("Empty folder or no folder selected");
        return;
    }

    if (row < 0 || row >= rowCount()) {
        G::popUp->showPopup("Invalid row " + QString::number(row));
        return;
    }

    QString s = "";
    rpt << "\n"   << G::sj("DataModel row", 27) << G::s(row);
    rpt << "\n  " << G::sj("FileName", 25) << G::s(index(row, G::NameColumn).data());
    rpt << "\n  " << G::sj("FilePath", 25) << G::s(index(row, 0).data(G::PathRole));
    rpt << "\n  " << G::sj("isIcon", 25) << G::s(!itemFromIndex(index(row, G::PathColumn))->icon().isNull());
    rpt << "\n  " << G::sj("isCached", 25) << G::s(index(row, 0).data(G::CachedRole));
    rpt << "\n  " << G::sj("isMetadataAttempted", 25) << G::s(index(row, G::MetadataAttemptedColumn).data());
    rpt << "\n  " << G::sj("isMetadataLoaded", 25) << G::s(index(row, G::MetadataLoadedColumn).data());
    rpt << "\n  " << G::sj("dupHideRaw", 25) << G::s(index(row, 0).data(G::DupHideRawRole));
    rpt << "\n  " << G::sj("dupRawRow", 25) << G::s(qvariant_cast<QModelIndex>(index(row, 0).data(G::DupOtherIdxRole)).row());
    rpt << "\n  " << G::sj("dupIsJpg", 25) << G::s(index(row, 0).data(G::DupIsJpgRole));
    rpt << "\n  " << G::sj("dupRawType", 25) << G::s(index(row, 0).data(G::DupRawTypeRole));
    rpt << "\n  " << G::sj("Column", 25) << G::s(index(row, 0).data(G::ColumnRole));
    rpt << "\n  " << G::sj("isGeek", 25) << G::s(index(row, 0).data(G::GeekRole));
    rpt << "\n  " << G::sj("type", 25) << G::s(index(row, G::TypeColumn).data());
    rpt << "\n  " << G::sj("video", 25) << G::s(index(row, G::VideoColumn).data());
    rpt << "\n  " << G::sj("duration", 25) << G::s(index(row, G::DurationColumn).data());
    rpt << "\n  " << G::sj("bytes", 25) << G::s(index(row, G::SizeColumn).data());
    rpt << "\n  " << G::sj("pick", 25) << G::s(index(row, G::PickColumn).data());
    rpt << "\n  " << G::sj("ingested", 25) << G::s(index(row, G::IngestedColumn).data());
    rpt << "\n  " << G::sj("label", 25) << G::s(index(row, G::LabelColumn).data());
    rpt << "\n  " << G::sj("_label", 25) << G::s(index(row, G::_LabelColumn).data());
    rpt << "\n  " << G::sj("rating", 25) << G::s(index(row, G::RatingColumn).data());
    rpt << "\n  " << G::sj("_rating", 25) << G::s(index(row, G::_RatingColumn).data());
    rpt << "\n  " << G::sj("search", 25) << G::s(index(row, G::SearchColumn).data());
    rpt << "\n  " << G::sj("modifiedDate", 25) << G::s(index(row, G::ModifiedColumn).data());
    rpt << "\n  " << G::sj("createdDate", 25) << G::s(index(row, G::CreatedColumn).data());
    rpt << "\n  " << G::sj("year", 25) << G::s(index(row, G::YearColumn).data());
    rpt << "\n  " << G::sj("day", 25) << G::s(index(row, G::DayColumn).data());
    rpt << "\n  " << G::sj("megapixels", 25) << G::s(index(row, G::MegaPixelsColumn).data());
    rpt << "\n  " << G::sj("width", 25) << G::s(index(row, G::WidthColumn).data());
    rpt << "\n  " << G::sj("height", 25) << G::s(index(row, G::HeightColumn).data());
    rpt << "\n  " << G::sj("dimensions", 25) << G::s(index(row, G::DimensionsColumn).data());
    rpt << "\n  " << G::sj("aspectRatio", 25) << G::s(index(row, G::DimensionsColumn).data());
    rpt << "\n  " << G::sj("rotation", 25) << G::s(index(row, G::RotationColumn).data());
//    rpt << "\n  " << G::sj("_rotation", 25) << G::s(index(row, G::_RotationColumn).data());
    rpt << "\n  " << G::sj("apertureNum", 25) << G::s(index(row, G::ApertureColumn).data());
    rpt << "\n  " << G::sj("exposureTimeNum", 25) << G::s(index(row, G::ShutterspeedColumn).data());
    rpt << "\n  " << G::sj("iso", 25) << G::s(index(row, G::ISOColumn).data());
    rpt << "\n  " << G::sj("exposureCompensationNum", 25) << G::s(index(row, G::ExposureCompensationColumn).data());
    rpt << "\n  " << G::sj("make", 25) << G::s(index(row, G::CameraMakeColumn).data());
    rpt << "\n  " << G::sj("model", 25) << G::s(index(row, G::CameraModelColumn).data());
    rpt << "\n  " << G::sj("lens", 25) << G::s(index(row, G::LensColumn).data());
    rpt << "\n  " << G::sj("focalLengthNum", 25) << G::s(index(row, G::FocalLengthColumn).data());
    rpt << "\n  " << G::sj("foxusX", 25) << G::s(index(row, G::FocusXColumn).data());
    rpt << "\n  " << G::sj("foxusY", 25) << G::s(index(row, G::FocusYColumn).data());
    rpt << "\n  " << G::sj("gpsCoord", 25) << G::s(index(row, G::GPSCoordColumn).data());
    rpt << "\n  " << G::sj("keywords", 25) << G::s(index(row, G::KeywordsColumn).data());
    rpt << "\n  " << G::sj("title", 25) << G::s(index(row, G::TitleColumn).data());
    rpt << "\n  " << G::sj("_title", 25) << G::s(index(row, G::_TitleColumn).data());
    rpt << "\n  " << G::sj("creator", 25) << G::s(index(row, G::CreatorColumn).data());
    rpt << "\n  " << G::sj("_creator", 25) << G::s(index(row, G::_CreatorColumn).data());
    rpt << "\n  " << G::sj("copyright", 25) << G::s(index(row, G::CopyrightColumn).data());
    rpt << "\n  " << G::sj("_copyright", 25) << G::s(index(row, G::_CopyrightColumn).data());
    rpt << "\n  " << G::sj("email", 25) << G::s(index(row, G::EmailColumn).data());
    rpt << "\n  " << G::sj("_email", 25) << G::s(index(row, G::_EmailColumn).data());
    rpt << "\n  " << G::sj("url", 25) << G::s(index(row, G::UrlColumn).data());
    rpt << "\n  " << G::sj("_url", 25) << G::s(index(row, G::_UrlColumn).data());
    rpt << "\n  " << G::sj("offsetFull", 25) << G::s(index(row, G::OffsetFullColumn).data());
    rpt << "\n  " << G::sj("lengthFull", 25) << G::s(index(row, G::LengthFullColumn).data());
    rpt << "\n  " << G::sj("widthPreview", 25) << G::s(index(row, G::WidthPreviewColumn).data());
    rpt << "\n  " << G::sj("heightPreview", 25) << G::s(index(row, G::HeightPreviewColumn).data());
    rpt << "\n  " << G::sj("offsetThumb", 25) << G::s(index(row, G::OffsetThumbColumn).data());
    rpt << "\n  " << G::sj("lengthThumb", 25) << G::s(index(row, G::LengthThumbColumn).data());
    rpt << "\n  " << G::sj("isBigEndian", 25) << G::s(index(row, G::isBigEndianColumn).data());
    rpt << "\n  " << G::sj("ifd0Offset", 25) << G::s(index(row, G::ifd0OffsetColumn).data());
    rpt << "\n  " << G::sj("xmpSegmentOffset", 25) << G::s(index(row, G::XmpSegmentOffsetColumn).data());
    rpt << "\n  " << G::sj("xmpNextSegmentOffset", 25) << G::s(index(row, G::XmpSegmentLengthColumn).data());

    rpt << "\n  " << G::sj("isXmp", 25) << G::s(index(row, G::IsXMPColumn).data());
    rpt << "\n  " << G::sj("orientationOffset", 25) << G::s(index(row, G::OrientationOffsetColumn).data());
    rpt << "\n  " << G::sj("orientation", 25) << G::s(index(row, G::OrientationColumn).data());
    rpt << "\n  " << G::sj("rotationDegrees", 25) << G::s(index(row, G::RotationDegreesColumn).data());
    rpt << "\n  " << G::sj("shootingInfo", 25) << G::s(index(row, G::ShootingInfoColumn).data());
    rpt << "\n  " << G::sj("loadMsecPerMp", 25) << G::s(index(row, G::LoadMsecPerMpColumn).data());
    rpt << "\n  " << G::sj("searchText", 25) << G::s(index(row, G::SearchTextColumn).data());

    rpt << "\n  ";
    rpt << Utilities::centeredRptHdr('=', "Issues");

    QStringList issues = rptIssues(row);
    rpt << "\n\nIssues: " << QString::number(issues.count());
    for (const QString &str : issues) {
        rpt << "\n   " << str;
    }
}

// --------------------------------------------------------------------------------------------
// SortFilter Class used to filter by row
// --------------------------------------------------------------------------------------------

SortFilter::SortFilter(QObject *parent, Filters *filters, bool &combineRawJpg) :
    QSortFilterProxyModel(parent),
    combineRawJpg(combineRawJpg)
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
    // if (sourceRow == 0) {
    // static int counter = 0;
    // counter++;
    // qDebug().noquote() << "SortFilter::filterAcceptsRow " << sourceRow
    //              << "suspendFiltering =" << suspendFiltering
    //              << "counter =" << counter;
    // }

    // Suspend?
    if (suspendFiltering) {
        // qDebug() << "SortFilter::filterAcceptsRow suspendFiltering = true" << sourceRow;
        return true;
    }

    // still loading metadata
    if (!G::allMetadataLoaded) {
        // qDebug() << "SortFilter::filterAcceptsRow G::allMetadataLoaded = false" << sourceRow;
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
        if ((*filter)->parent()) {
            /*
            There is a parent therefore not a top level item so this is one of the items
            to match ie rating = one. If the item has been checked then compare the
            checked filter item to the data in the dataModelColumn for the row. If it
            matches then set isMatch = true. If it does not match then isMatch is still
            false but the row could still be accepted if another item in the same
            category does match.
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

    // force wait until finished to prevent sorting/editing datamodel
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
    if (G::isLogger) G::log("SortFilter::suspend");
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
