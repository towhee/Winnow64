#include "Datamodel/datamodel.h"

/*
The datamodel (dm thoughout app) contains information about each eligible image
file in the selected folder (and optionally the subfolder heirarchy).  Eligible
image files, defined in the metadata class, are files Winnow knows how to decode.

The data is structured in columns:

    ● Path:             from QFileInfoList  FilePathRole (absolutePath)
                        from QFileInfoList  ToolTipRole
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
    ● Refined:          refine function     EditRole
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

DataModel::DataModel(QWidget *parent,
                     Metadata *metadata,
                     Filters *filters,
                     bool &combineRawJpg) :

                     QStandardItemModel(parent),
                     combineRawJpg(combineRawJpg)
{
    if (G::isLogger) G::log("DataModel::DataModel");

    lastFunction = "";
    isDebug = false;

    /* Every time a new folder is selected the model instance is incremented to check for concurrency
       issues where a thumb or image decoder is still working on the prior folder */
    instance = -1;
    if (isDebug) qDebug() << "DataModel::DataModel" << "instance =" << instance;

    mw = parent;
    this->metadata = metadata;
    this->filters = filters;

    setModelProperties();

    sf = new SortFilter(this, filters, combineRawJpg);
    sf->setSourceModel(this);
    selectionModel = new QItemSelectionModel(sf);

    // root folder containing images to be added to the data model
    dir = new QDir();
    fileFilters = new QStringList;          // eligible image file types
    emptyImg.load(":/images/no_image.png");

}

void DataModel::setModelProperties()
{
    lastFunction = "";
    if (isDebug) qDebug() << "DataModel::setModelProperties" << "instance =" << instance;

    setSortRole(Qt::EditRole);

    // must include all prior Global dataModelColumns (any order okay)
    setHorizontalHeaderItem(G::PathColumn, new QStandardItem("Icon")); horizontalHeaderItem(G::PathColumn)->setData(false, G::GeekRole);
    setHorizontalHeaderItem(G::NameColumn, new QStandardItem("File Name")); horizontalHeaderItem(G::NameColumn)->setData(false, G::GeekRole);
    setHorizontalHeaderItem(G::RefineColumn, new QStandardItem("Refine")); horizontalHeaderItem(G::RefineColumn)->setData(false, G::GeekRole);
    setHorizontalHeaderItem(G::PickColumn, new QStandardItem("Pick")); horizontalHeaderItem(G::PickColumn)->setData(false, G::GeekRole);
    setHorizontalHeaderItem(G::IngestedColumn, new QStandardItem("Ingested")); horizontalHeaderItem(G::IngestedColumn)->setData(false, G::GeekRole);
    setHorizontalHeaderItem(G::LabelColumn, new QStandardItem("Colour")); horizontalHeaderItem(G::LabelColumn)->setData(false, G::GeekRole);
    setHorizontalHeaderItem(G::RatingColumn, new QStandardItem("Rating")); horizontalHeaderItem(G::RatingColumn)->setData(false, G::GeekRole);
    setHorizontalHeaderItem(G::SearchColumn, new QStandardItem("🔎")); horizontalHeaderItem(G::SearchColumn)->setData(false, G::GeekRole);
    setHorizontalHeaderItem(G::TypeColumn, new QStandardItem("Type")); horizontalHeaderItem(G::TypeColumn)->setData(false, G::GeekRole);
    setHorizontalHeaderItem(G::VideoColumn, new QStandardItem("Video")); horizontalHeaderItem(G::VideoColumn)->setData(false, G::GeekRole);
    setHorizontalHeaderItem(G::PermissionsColumn, new QStandardItem("Permissions")); horizontalHeaderItem(G::PermissionsColumn)->setData(true, G::GeekRole);
    setHorizontalHeaderItem(G::ReadWriteColumn, new QStandardItem("R/W")); horizontalHeaderItem(G::ReadWriteColumn)->setData(true, G::GeekRole);
    setHorizontalHeaderItem(G::SizeColumn, new QStandardItem("Size")); horizontalHeaderItem(G::SizeColumn)->setData(false, G::GeekRole);
    setHorizontalHeaderItem(G::WidthColumn, new QStandardItem("Width")); horizontalHeaderItem(G::WidthColumn)->setData(false, G::GeekRole);
    setHorizontalHeaderItem(G::HeightColumn, new QStandardItem("Height")); horizontalHeaderItem(G::HeightColumn)->setData(false, G::GeekRole);
    setHorizontalHeaderItem(G::DimensionsColumn, new QStandardItem("Dimensions")); horizontalHeaderItem(G::DimensionsColumn)->setData(false, G::GeekRole);
    setHorizontalHeaderItem(G::AspectRatioColumn, new QStandardItem("Aspect Ratio")); horizontalHeaderItem(G::AspectRatioColumn)->setData(false, G::GeekRole);
    setHorizontalHeaderItem(G::CreatedColumn, new QStandardItem("Created")); horizontalHeaderItem(G::CreatedColumn)->setData(false, G::GeekRole);
    setHorizontalHeaderItem(G::ModifiedColumn, new QStandardItem("Last Modified")); horizontalHeaderItem(G::ModifiedColumn)->setData(false, G::GeekRole);
    setHorizontalHeaderItem(G::YearColumn, new QStandardItem("Year")); horizontalHeaderItem(G::YearColumn)->setData(false, G::GeekRole);
    setHorizontalHeaderItem(G::DayColumn, new QStandardItem("Day")); horizontalHeaderItem(G::DayColumn)->setData(false, G::GeekRole);
    setHorizontalHeaderItem(G::CreatorColumn, new QStandardItem("Creator")); horizontalHeaderItem(G::CreatorColumn)->setData(false, G::GeekRole);
    setHorizontalHeaderItem(G::MegaPixelsColumn, new QStandardItem("MPix")); horizontalHeaderItem(G::MegaPixelsColumn)->setData(false, G::GeekRole);
    setHorizontalHeaderItem(G::LoadMsecPerMpColumn, new QStandardItem("Msec/Mp")); horizontalHeaderItem(G::LoadMsecPerMpColumn)->setData(false, G::GeekRole);
    setHorizontalHeaderItem(G::RotationColumn, new QStandardItem("Rot")); horizontalHeaderItem(G::RotationColumn)->setData(false, G::GeekRole);
    setHorizontalHeaderItem(G::ApertureColumn, new QStandardItem("Aperture")); horizontalHeaderItem(G::ApertureColumn)->setData(false, G::GeekRole);
    setHorizontalHeaderItem(G::ShutterspeedColumn, new QStandardItem("Shutter")); horizontalHeaderItem(G::ShutterspeedColumn)->setData(false, G::GeekRole);
    setHorizontalHeaderItem(G::ISOColumn, new QStandardItem("ISO")); horizontalHeaderItem(G::ISOColumn)->setData(false, G::GeekRole);
    setHorizontalHeaderItem(G::ExposureCompensationColumn, new QStandardItem("  EC  ")); horizontalHeaderItem(G::ExposureCompensationColumn)->setData(false, G::GeekRole);
    setHorizontalHeaderItem(G::DurationColumn, new QStandardItem("Duration")); horizontalHeaderItem(G::DurationColumn)->setData(false, G::GeekRole);
    setHorizontalHeaderItem(G::CameraMakeColumn, new QStandardItem("Make")); horizontalHeaderItem(G::CameraMakeColumn)->setData(false, G::GeekRole);
    setHorizontalHeaderItem(G::CameraModelColumn, new QStandardItem("Model")); horizontalHeaderItem(G::CameraModelColumn)->setData(false, G::GeekRole);
    setHorizontalHeaderItem(G::LensColumn, new QStandardItem("Lens")); horizontalHeaderItem(G::LensColumn)->setData(false, G::GeekRole);
    setHorizontalHeaderItem(G::FocalLengthColumn, new QStandardItem("Focal length")); horizontalHeaderItem(G::FocalLengthColumn)->setData(false, G::GeekRole);
    setHorizontalHeaderItem(G::GPSCoordColumn, new QStandardItem("GPS Coord")); horizontalHeaderItem(G::GPSCoordColumn)->setData(false, G::GeekRole);
    setHorizontalHeaderItem(G::TitleColumn, new QStandardItem("Title")); horizontalHeaderItem(G::TitleColumn)->setData(false, G::GeekRole);
    setHorizontalHeaderItem(G::CopyrightColumn, new QStandardItem("Copyright")); horizontalHeaderItem(G::CopyrightColumn)->setData(false, G::GeekRole);
    setHorizontalHeaderItem(G::EmailColumn, new QStandardItem("Email")); horizontalHeaderItem(G::EmailColumn)->setData(false, G::GeekRole);
    setHorizontalHeaderItem(G::UrlColumn, new QStandardItem("Url")); horizontalHeaderItem(G::UrlColumn)->setData(false, G::GeekRole);
    setHorizontalHeaderItem(G::KeywordsColumn, new QStandardItem("Keywords")); horizontalHeaderItem(G::KeywordsColumn)->setData(false, G::GeekRole);

    setHorizontalHeaderItem(G::MetadataLoadedColumn, new QStandardItem("Meta Loaded")); horizontalHeaderItem(G::MetadataLoadedColumn)->setData(true, G::GeekRole);
    setHorizontalHeaderItem(G::_RatingColumn, new QStandardItem("_Rating")); horizontalHeaderItem(G::_RatingColumn)->setData(true, G::GeekRole);
    setHorizontalHeaderItem(G::_LabelColumn, new QStandardItem("_Label")); horizontalHeaderItem(G::_LabelColumn)->setData(true, G::GeekRole);
    setHorizontalHeaderItem(G::_CreatorColumn, new QStandardItem("_Creator")); horizontalHeaderItem(G::_CreatorColumn)->setData(true, G::GeekRole);
    setHorizontalHeaderItem(G::_TitleColumn, new QStandardItem("_Title")); horizontalHeaderItem(G::_TitleColumn)->setData(true, G::GeekRole);
    setHorizontalHeaderItem(G::_CopyrightColumn, new QStandardItem("_Copyright")); horizontalHeaderItem(G::_CopyrightColumn)->setData(true, G::GeekRole);
    setHorizontalHeaderItem(G::_EmailColumn, new QStandardItem("_Email")); horizontalHeaderItem(G::_EmailColumn)->setData(true, G::GeekRole);
    setHorizontalHeaderItem(G::_UrlColumn, new QStandardItem("_Url")); horizontalHeaderItem(G::_UrlColumn)->setData(true, G::GeekRole);
//    setHorizontalHeaderItem(G::_OrientationColumn, new QStandardItem("_Url")); horizontalHeaderItem(G::_OrientationColumn)->setData(true, G::GeekRole);
//    setHorizontalHeaderItem(G::_RotationColumn, new QStandardItem("_Url")); horizontalHeaderItem(G::_RotationColumn)->setData(true, G::GeekRole);

    setHorizontalHeaderItem(G::OffsetFullColumn, new QStandardItem("OffsetFull")); horizontalHeaderItem(G::OffsetFullColumn)->setData(true, G::GeekRole);
    setHorizontalHeaderItem(G::LengthFullColumn, new QStandardItem("LengthFull")); horizontalHeaderItem(G::LengthFullColumn)->setData(true, G::GeekRole);
    setHorizontalHeaderItem(G::WidthPreviewColumn, new QStandardItem("WidthPreview")); horizontalHeaderItem(G::WidthPreviewColumn)->setData(true, G::GeekRole);
    setHorizontalHeaderItem(G::HeightPreviewColumn, new QStandardItem("HeightPreview")); horizontalHeaderItem(G::HeightPreviewColumn)->setData(true, G::GeekRole);
    setHorizontalHeaderItem(G::OffsetThumbColumn, new QStandardItem("OffsetThumb")); horizontalHeaderItem(G::OffsetThumbColumn)->setData(true, G::GeekRole);
    setHorizontalHeaderItem(G::LengthThumbColumn, new QStandardItem("LengthThumb")); horizontalHeaderItem(G::LengthThumbColumn)->setData(true, G::GeekRole);
    setHorizontalHeaderItem(G::samplesPerPixelColumn, new QStandardItem("samplesPerPixelFull")); horizontalHeaderItem(G::samplesPerPixelColumn)->setData(true, G::GeekRole);

    setHorizontalHeaderItem(G::isBigEndianColumn, new QStandardItem("isBigEndian")); horizontalHeaderItem(G::isBigEndianColumn)->setData(true, G::GeekRole);
    setHorizontalHeaderItem(G::ifd0OffsetColumn, new QStandardItem("ifd0Offset")); horizontalHeaderItem(G::ifd0OffsetColumn)->setData(true, G::GeekRole);
    setHorizontalHeaderItem(G::XmpSegmentOffsetColumn, new QStandardItem("XmpSegmentOffset")); horizontalHeaderItem(G::XmpSegmentOffsetColumn)->setData(true, G::GeekRole);
    setHorizontalHeaderItem(G::XmpSegmentLengthColumn, new QStandardItem("XmpSegmentLengthColumn")); horizontalHeaderItem(G::XmpSegmentLengthColumn)->setData(true, G::GeekRole);
    setHorizontalHeaderItem(G::IsXMPColumn, new QStandardItem("IsXMP")); horizontalHeaderItem(G::IsXMPColumn)->setData(true, G::GeekRole);
    setHorizontalHeaderItem(G::ICCSegmentOffsetColumn, new QStandardItem("ICCSegmentOffsetColumn")); horizontalHeaderItem(G::ICCSegmentOffsetColumn)->setData(true, G::GeekRole);
    setHorizontalHeaderItem(G::ICCSegmentLengthColumn, new QStandardItem("ICCSegmentLengthColumn")); horizontalHeaderItem(G::ICCSegmentLengthColumn)->setData(true, G::GeekRole);
    setHorizontalHeaderItem(G::ICCBufColumn, new QStandardItem("ICCBuf")); horizontalHeaderItem(G::ICCBufColumn)->setData(true, G::GeekRole);
    setHorizontalHeaderItem(G::ICCSpaceColumn, new QStandardItem("ICCSpace")); horizontalHeaderItem(G::ICCSpaceColumn)->setData(true, G::GeekRole);
    setHorizontalHeaderItem(G::OrientationOffsetColumn, new QStandardItem("OrientationOffset")); horizontalHeaderItem(G::OrientationOffsetColumn)->setData(true, G::GeekRole);
    setHorizontalHeaderItem(G::OrientationColumn, new QStandardItem("Orientation")); horizontalHeaderItem(G::OrientationColumn)->setData(true, G::GeekRole);
    setHorizontalHeaderItem(G::RotationDegreesColumn, new QStandardItem("RotationDegrees")); horizontalHeaderItem(G::RotationDegreesColumn)->setData(true, G::GeekRole);
    setHorizontalHeaderItem(G::ShootingInfoColumn, new QStandardItem("ShootingInfo")); horizontalHeaderItem(G::ShootingInfoColumn)->setData(true, G::GeekRole);
    setHorizontalHeaderItem(G::SearchTextColumn, new QStandardItem("Search")); horizontalHeaderItem(G::SearchTextColumn)->setData(true, G::GeekRole);

}
void DataModel::clearDataModel()
{
    lastFunction = "";
    if (G::isLogger || G::isFlowLogger) G::log("DataModel::clearDataModel");
    // clear the model
    if (mLock) return;
    if (isDebug) qDebug() << "DataModel::clearDataModel" << "instance =" << instance;
    clear();
    setModelProperties();
    // clear all items for filters based on data content ie file types, camera model
    filters->removeChildrenDynamicFilters();
    // reset remaining criteria without signalling filter change as no new data yet
    filters->clearAll();
    // clear the fPath index of datamodel rows
    fPathRow.clear();
    // clear list of fileInfo
    fileInfoList.clear();
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
    // check if combined raw+jpg duplicates
    if(i1.completeBaseName() == i2.completeBaseName()) {
        if (i1.suffix().toLower() == "jpg") s1.replace(".jpg", ".zzz");
        if (i2.suffix().toLower() == "jpg") s2.replace(".jpg", ".zzz");
    }
    return s1 < s2;
}

void DataModel::insert(QString fPath)
{
/*
    Insert a new image into the data model.  Use when a new image is created by embel export
    or meanStack to quickly refresh the active folder with the just saved image.
*/
    lastFunction = "";
    if (G::isLogger) G::log("DataModel::insert");
    if (isDebug) qDebug() << "DataModel::insert" << "instance =" << instance;
    QFileInfo insertFile(fPath);

    // find row greater than insert file absolute path
    int row;
    for (row = 0; row < rowCount(); ++row) {
        QString rowPath = index(row, 0).data(G::PathRole).toString();
        QFileInfo currentFile(rowPath);
        if (lessThan(insertFile, currentFile)) break;
    }

    // insert new row
    insertRow(row);
    fileInfoList.insert(row, insertFile);

    // add the file data
    addFileDataForRow(row, insertFile);
    // read and add metadata
    readMetadataForItem(row, instance);
}

void DataModel::remove(QString fPath)
{
/*
    Delete a row from the data model matching the absolute path.  This is used when an image
    file has been deleted by Winnow.
*/
    lastFunction = "";
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

    // remove from fileInfoList
    int i;
    for (i = 0; i < fileInfoList.count(); ++i) {
        if (fileInfoList.at(i).filePath() == fPath) {
            fileInfoList.removeAt(i);
            break;
        }
    }
}

void DataModel::find(QString text)
{
    lastFunction = "";
    if (G::isLogger) G::log("DataModel::find");
    if (isDebug) qDebug() << "DataModel::find" << "instance =" << instance << text;
    mutex.lock();
    for (int row = 0; row < sf->rowCount(); ++row) {
        QString searchableText = sf->index(row, G::SearchTextColumn).data().toString();
        qDebug() << "DataModel::find" << searchableText;
        if (searchableText.contains(text.toLower())) {
            QModelIndex idx = sf->mapToSource(sf->index(row, G::SearchColumn));
            setData(idx, "true");
        }
    }
    mutex.unlock();
}

void DataModel::abortLoad()
{
    lastFunction = "";
    if (G::isLogger) G::log("DataModel::abortLoad", "instance = " + QString::number(instance));
    if (isDebug) qDebug() << "DataModel::abortLoad" << "instance =" << instance;
    if (loadingModel) abortLoadingModel = true;
}

bool DataModel::endLoad(bool success)
{
    lastFunction = "";
    if (G::isLogger) G::log("DataModel::endLoad", "instance = " + QString::number(instance));
    if (isDebug) qDebug() << "DataModel::endLoad" << "instance =" << instance
                          << "success =" << success;

    loadingModel = false;
    abortLoadingModel = false;
    if (success) {
        G::dmEmpty = false;
        return true;
    }
    else {
        clear();
        G::dmEmpty = true;
        return false;
    }
}

bool DataModel::load(QString &folderPath, bool includeSubfoldersFlag)
{
/*
    When a new folder is selected load it into the data model.  This clears the
    model and populates the data model with all the cached thumbnail pixmaps from
    metadataCache.  If include subfolders has been chosen then the entire subfolder
    heirarchy is loaded.

    Steps:
    - filter to only show supported image formats, iterating subfolders if include
      subfolders is set
    - add each image file to the datamodel with QFileInfo related data such as file
      name, path, file size, creation date
    - also determine if there are duplicate raw+jpg files, and if so, populate all
      the Dup...Role values to manage the raw+jpg files
    - after the metadataCacheThread has read all the metadata and thumbnails add
      the rest of the metadata to the datamodel.

    - Note: build QMaps of unique field values for the filters is not done here, but
      on demand when the user selects the filter panel or a menu filter command.
*/
    lastFunction = "";
    if (G::isLogger || G::isFlowLogger) G::log("DataModel::load", folderPath);
//    while (mLock) {
//        qDebug() << "waiting...";
//    }
    clearDataModel();
//    instance++;
//    G::dmInstance = instance;
    if (isDebug) qDebug() << "DataModel::load" << "instance =" << instance << folderPath;
    currentFolderPath = folderPath;
    filters->filtersBuilt = false;
    filters->loadedDataModel(false);
    loadingModel = true;

    if (G::isLinearLoading) {
        emit centralMsg("Commencing to load folder " + folderPath);    // rghmsg
        qApp->processEvents();
    }

    if (G::isLogger || G::isFlowLogger) G::log("DataModel::load", "continue loading");

    // do some initializing
    fileFilters->clear();
    foreach (const QString &str, metadata->supportedFormats)
            fileFilters->append("*." + str);
    dir->setNameFilters(*fileFilters);
    dir->setFilter(QDir::Files);
    dir->setPath(currentFolderPath);

    imageCount = 0;
    countInterval = 100;

    QString step = "Searching for eligible images.\n\n";
    QString escapeClause = "\n\nPress \"Esc\" to stop.";
    QString root;
    if (dir->isRoot()) root = "Drive ";
    else root = "Folder ";

    // load file list for the current folder
    int folderImageCount = dir->entryInfoList().size();

    // bail if no images and not including subfolders
    if (!folderImageCount && !includeSubfoldersFlag) return endLoad(false);

    int folderCount = 1;
    // add supported images in folder to image list

    for (int i = 0; i < folderImageCount; ++i) {
        fileInfoList.append(dir->entryInfoList().at(i));
        imageCount++;
        if (imageCount % countInterval == 0 && imageCount > 0) {
            QString s = step +
                        QString::number(imageCount) + " found so far in " +
                        QString::number(folderCount) + " folders" +
                        escapeClause;
            emit centralMsg(s);        // rghmsg
        }
        if (abortLoadingModel) return endLoad(false);
    }

    if (!includeSubfoldersFlag) {
        includeSubfolders = false;
        return endLoad(addFileData());
    }

    // if include subfolders
    includeSubfolders = true;
    QDirIterator it(currentFolderPath, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        if (abortLoadingModel) break;
        it.next();
        if (it.fileInfo().isDir() && it.fileName() != "." && it.fileName() != "..") {
            folderCount++;
            dir->setPath(it.filePath());
            int folderImageCount = dir->entryInfoList().size();
            // try next subfolder if no images in this folder
            if (!folderImageCount) continue;
            // add supported images in folder to image list
            for (int i = 0; i < folderImageCount; ++i) {
                if (abortLoadingModel) break;
                fileInfoList.append(dir->entryInfoList().at(i));
                imageCount++;
                // report file progress within folder
                if (imageCount % countInterval == 0 && imageCount > 0) {
                    QString s = step +
                                QString::number(imageCount) + " found so far in " +
                                QString::number(folderCount) + " folders" +
                                escapeClause;
                    emit centralMsg(s);    // rghmsg
                    qApp->processEvents();
                }
            }
        }
    }
    if (abortLoadingModel || !imageCount) return endLoad(false);
    // if images were found and added to data model
    return endLoad(addFileData());
}

bool DataModel::addFileData()
{
/*
    Load the information from the operating system contained in QFileInfo first

    • PathColumn
    • NameColumn        (core sort item)
    • TypeColumn        (core sort item)
    • PermissionsColumn (core sort item)  rgh?
    • ReadWriteColumn   (core sort item)  rgh?
    • SizeColumn        (core sort item)
    • CreatedColumn     (core sort item)
    • ModifiedColumn    (core sort item)
    • RefineColumn
    • PickColumn        (core sort item)
    • IngestedColumn
    • SearchColumn
    • ErrColumn
*/
    lastFunction = "";
    if (G::isLogger) G::log("DataModel::addFileData");
    if (isDebug) qDebug() << "DataModel::addFileData" << "instance =" << instance << currentFolderPath;
    QString logmsg = QString::number(fileInfoList.count()) + " images";
//    qDebug() << "DataModel::addFileDataForRow" << logmsg;
    if (G::isLogger || G::isFlowLogger) G::log("DataModel::addFileDataForRow", logmsg);
    // make sure if raw+jpg pair that raw file is first to make combining easier
    std::sort(fileInfoList.begin(), fileInfoList.end(), lessThan);

    QString step = "Loading eligible images.\n\n";
    QString escapeClause = "\n\nPress \"Esc\" to stop.";

    // test if raw file to match jpg when same file names and one is a jpg
    QString suffix;
    QString prevRawSuffix = "";
    QString prevRawBaseName = "";
    QString baseName = "";
    QModelIndex prevRawIdx;

    setRowCount(fileInfoList.count());
    setColumnCount(G::TotalColumns);

    for (int row = 0; row < fileInfoList.count(); ++row) {
        if (abortLoadingModel) return false;
        // get file info
        fileInfo = fileInfoList.at(row);
        addFileDataForRow(row, fileInfo);

        /* Save info for duplicated raw and jpg files, which generally are the result of
        setting raw+jpg in the camera. The datamodel is sorted by file path, except raw files
        with the same path precede jpg files with duplicate names. Two roles track duplicates:
        G::DupHideRawRole flags jpg files with duplicate raws and G::DupOtherIdxRole points to
        the duplicate other file of the pair. For example:

        Row = 0 "G:/DCIM/100OLYMP/P4020001.ORF"  DupHideRawRole = true 	 DupOtherIdxRole = QModelIndex(1,0)
        Row = 1 "G:/DCIM/100OLYMP/P4020001.JPG"  DupHideRawRole = false  DupOtherIdxRole = QModelIndex(0,0)  DupRawTypeRole = "ORF"
        Row = 2 "G:/DCIM/100OLYMP/P4020002.ORF"  DupHideRawRole = true 	 DupOtherIdxRole = QModelIndex(3,0)
        Row = 3 "G:/DCIM/100OLYMP/P4020002.JPG"  DupHideRawRole = false  DupOtherIdxRole = QModelIndex(2,0)  DupRawTypeRole = "ORF"
        */

        suffix = fileInfoList.at(row).suffix().toLower();
        baseName = fileInfoList.at(row).completeBaseName();
        if (metadata->hasJpg.contains(suffix)) {
            prevRawSuffix = suffix;
            prevRawBaseName = fileInfoList.at(row).completeBaseName();
            prevRawIdx = index(row, 0);
        }

        // if row/jpg pair
        if (suffix == "jpg" && baseName == prevRawBaseName) {
            mutex.lock();
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
            mutex.unlock();
        }

        // Load folder progress
        if (G::isLinearLoading) {
            if (row % 100 == 0) {
                QString s = QString::number(row) + " of " + QString::number(rowCount()) +
                            " system file info loaded.";
                emit centralMsg(s);    // rghmsg
                qApp->processEvents();
            }
        }
    }
    if (rowCount() >0) {
        QModelIndex par = index(0,0).parent();
//        qDebug() << "INSTANCE =" << instance
//                 << "index(0,0) =" << index(0,0)
//                 << "p =" << &par
//                    ;
        par = index(0,1).parent();
//        qDebug() << "INSTANCE =" << instance
//                 << "index(0,0) =" << index(0,1)
//                 << "p =" << &par
//                    ;
    }
    return true;
}

void DataModel::addFileDataForRow(int row, QFileInfo fileInfo)
{
    lastFunction = "";
    if (G::isLogger) G::log("DataModel::addFileDataForRow");
    if (isDebug) qDebug() << "DataModel::addFileDataForRow"
                          << "instance =" << instance
                          << "row =" << row
                          << currentFolderPath
                             ;
    // append hash index of datamodel row for fPath for fast lookups
    QString fPath = fileInfo.filePath();
//    qDebug() << "DataModel::addFileDataForRow" << row << fPath;
    QString ext = fileInfo.suffix().toLower();
    // build hash to quickly get row from fPath (ie pixmap.cpp, imageCache...)
    fPathRow[fPath] = row;

    // string to hold aggregated text for searching
    QString search = fPath;

    mutex.lock();
    setData(index(row, G::PathColumn), fPath, G::PathRole);
    QString tip = QString::number(row) + ": " + fileInfo.absoluteFilePath();
    setData(index(row, G::PathColumn), tip, Qt::ToolTipRole);
    setData(index(row, G::PathColumn), QRect(), G::IconRectRole);
    setData(index(row, G::PathColumn), false, G::CachedRole);
    setData(index(row, G::PathColumn), false, G::CachingIconRole);
    setData(index(row, G::PathColumn), false, G::DupHideRawRole);
    setData(index(row, G::NameColumn), fileInfo.fileName());
    setData(index(row, G::NameColumn), fileInfo.fileName(), Qt::ToolTipRole);
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
    // G::CreatedColumn is extracted from the image metadata, not QFileInfo
    s = fileInfo.lastModified().toString("yyyy-MM-dd hh:mm:ss");
    search += s;
    setData(index(row, G::ModifiedColumn), s);
    setData(index(row, G::RefineColumn), false);
    setData(index(row, G::RefineColumn), int(Qt::AlignCenter | Qt::AlignVCenter), Qt::TextAlignmentRole);
    setData(index(row, G::PickColumn), "false");
    setData(index(row, G::PickColumn), int(Qt::AlignCenter | Qt::AlignVCenter), Qt::TextAlignmentRole);
    setData(index(row, G::IngestedColumn), "false");
    setData(index(row, G::IngestedColumn), int(Qt::AlignCenter | Qt::AlignVCenter), Qt::TextAlignmentRole);
    setData(index(row, G::MetadataLoadedColumn), false);
    setData(index(row, G::SearchColumn), "false");
    setData(index(row, G::SearchColumn), Qt::AlignLeft, Qt::TextAlignmentRole);
    setData(index(row, G::SearchTextColumn), search);
    setData(index(row, G::SearchTextColumn), search, Qt::ToolTipRole);
    mutex.unlock();
}

bool DataModel::updateFileData(QFileInfo fileInfo)
{
    lastFunction = "";
    if (G::isLogger) G::log("DataModel::updateFileData");
//    qDebug() << "DataModel::updateFileData" << "Instance =" << instance << currentFolderPath;
    QString fPath = fileInfo.filePath();
    if (!fPathRow.contains(fPath)) return false;
    int row = fPathRow[fPath];
    if (!index(row,0).isValid()) return false;
    mutex.lock();
    setData(index(row, G::SizeColumn), fileInfo.size());
    setData(index(row, G::SizeColumn), int(Qt::AlignRight | Qt::AlignVCenter), Qt::TextAlignmentRole);
    QString s = fileInfo.lastModified().toString("yyyy-MM-dd hh:mm:ss");
    setData(index(row, G::ModifiedColumn), s);
    mutex.unlock();
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
    lastFunction = "";
    if (G::isLogger) G::log("DataModel::imMetadata", fPath);
//    qDebug() << "DataModel::imMetadata" << "Instance =" << instance << currentFolderPath;
    ImageMetadata m;
    if (fPath == "") return m;

    int row = fPathRow[fPath];
    if (!index(row,0).isValid()) return m;

    if (isDebug) qDebug() << "DataModel::imMetadata" << "instance =" << instance
                          << "row =" << row
                          << currentFolderPath;

    mutex.lock();
    metadata->m.row = row;  // rgh is this req'd

    // file info (calling Metadata not required)
    m.row = row;
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
            mutex.unlock();
            addMetadataForItem(metadata->m, "DataModel::imMetadata");
            mutex.lock();
            success = true;
        }
    }

    if (!success) {
         m.metadataLoaded = true;
        if (metadata->hasMetadataFormats.contains(m.type.toLower())) {
            qWarning() << "WARNING" << "DataModel::imMetadata" << "Metadata not loaded to model for" << fPath;
        }
        mutex.unlock();
        return m;
    }

    m.refine  = index(row, G::RefineColumn).data().toBool();
    m.pick  = index(row, G::PickColumn).data().toBool();
    m.ingested  = index(row, G::IngestedColumn).data().toBool();
    m.metadataLoaded = index(row, G::MetadataLoadedColumn).data().toBool();

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
    m.xmpSegmentOffset = index(row, G::XmpSegmentOffsetColumn).data().toUInt();
    m.xmpSegmentLength = index(row, G::XmpSegmentLengthColumn).data().toUInt();
    m.isXmp = index(row, G::IsXMPColumn).data().toBool();
    m.iccSegmentOffset = index(row, G::ICCSegmentOffsetColumn).data().toUInt();
    m.iccSegmentLength = index(row, G::ICCSegmentLengthColumn).data().toUInt();
    m.iccBuf = index(row, G::ICCBufColumn).data().toByteArray();
    m.iccSpace = index(row, G::ICCSegmentOffsetColumn).data().toString();

    m.searchStr = index(row, G::SearchTextColumn).data().toString();

    if (updateInMetadata) metadata->m = m;

    mutex.unlock();
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
    lastFunction = "DataModel::addAllMetadata";
    if (G::isLogger || G::isFlowLogger) G::log("DataModel::addAllMetadata");
    if (isDebug) qDebug() << "DataModel::addAllMetadata" << "instance =" << instance << currentFolderPath;
//    G::t.restart();
    int count = 0;
    for (int row = 0; row < rowCount(); ++row) {
        // Load folder progress
        if (G::isLinearLoading && row % 100 == 0) {
            QString s = QString::number(row) + " of " + QString::number(rowCount()) +
                        " secondary metadata loading...";
            emit centralMsg(s);    // rghmsg
            qApp->processEvents();
        }
        if (abortLoadingModel || G::dmEmpty) {
            endLoad(false);
            setAllMetadataLoaded(false);
            return;
        }
        // is metadata already cached
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
                qWarning() << "WARNING" << "DataModel::addAllMetadata" << "Failed to load metadata." << fPath;
            }
        }
    }
    setAllMetadataLoaded(true);
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
    lastFunction = "";
    if (G::isLogger) G::log("DataModel::readMetadataForItem", index(row, 0).data(G::PathRole).toString());
//    qDebug() << "DataModel::readMetadataForItem" << "Instance =" << instance << currentFolderPath;
    if (isDebug) qDebug() << "DataModel::readMetadataForItem" << "instance =" << instance
                          << "row =" << row
                          << currentFolderPath;

    // might be called from previous folder during folder change
    if (instance != this->instance) {
        qWarning() << "WARNING" << "DataModel::readMetadataForItem" << row << "Instance conflict = "
                 << "DM instance =" << this->instance
                 << "Src instance =" << instance
                    ;
        return true;
    }
    if (G::stop) return false;

    mutex.lock();

    QString fPath = index(row, 0).data(G::PathRole).toString();

    // load metadata
//     qDebug() << "DataModel::readMetadataForItem"
//              << "Metadata loaded ="
//              << index(row, G::MetadataLoadedColumn).data().toBool()
//              << fPath;
    if (!index(row, G::MetadataLoadedColumn).data().toBool()) {
        QFileInfo fileInfo(fPath);

        // only read metadata from files that we know how to
        QString ext = fileInfo.suffix().toLower();
        if (metadata->hasMetadataFormats.contains(ext)) {
//            qDebug() << "DataModel::readMetadataForItem" << fPath;
            if (metadata->loadImageMetadata(fileInfo, instance, true, true, false, true, "DataModel::readMetadataForItem")) {
                metadata->m.row = row;
                addMetadataForItem(metadata->m, "DataModel::readMetadataForItem");
            }
            else {
                qWarning() << "WARNING" << "DataModel::readMetadataForItem" << "Failed to load metadata for " << fPath;
                G::error("", fPath, "Failed to load metadata.");
                mutex.unlock();
                return false;
            }
        }
        // cannot read this file type, load empty metadata
        else {
//            qWarning() << "WARNING" << "DataModel::readMetadataForItem" << "cannot read this file type, load empty metadata for " + fPath;
//            G::error("DataModel::readMetadataForItem", fPath, "Cannot read file type.");
            metadata->clearMetadata();
            metadata->m.row = row;
            addMetadataForItem(metadata->m, "DataModel::readMetadataForItem");
            mutex.unlock();
            return false;
        }
    }
    mutex.unlock();
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
    lastFunction = "";
    mCopy = m;
    line = 973;
    if (G::isLogger) G::log("DataModel::addMetadataForItem");
    /*
    qDebug() << "DataModel::addMetadataForItem"
             << "Instance =" << instance
             << "m.instance =" << m.instance
             << currentFolderPath;
    //*/
    line = 980;  rowCountChk = rowCount();
    if (G::stop) return false;
    if (isDebug) qDebug() << "DataModel::addMetadataForItem" << "instance =" << instance
                          << "row =" << m.row
                          << currentFolderPath;

    // deal with lagging signals when new folder selected suddenly
    if (instance > -1 && m.instance != instance) {
        qWarning() << "WARNING"
                   << "DataModel::addMetadataForItem Instance conflict "
                   << "m.instance =" << m.instance
                   << "instance =" << instance
                   << "row =" << m.row
                   << "src =" << src
                      ;
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

//    mutex.lock();
    mLock = true;
    setData(index(row, G::SearchColumn), m.isSearch);
    setData(index(row, G::SearchColumn), Qt::AlignCenter, Qt::TextAlignmentRole);
    setData(index(row, G::LabelColumn), m.label);
    setData(index(row, G::LabelColumn), Qt::AlignCenter, Qt::TextAlignmentRole);
     search += m.label;
    setData(index(row, G::_LabelColumn), m._label);
    setData(index(row, G::RatingColumn), m.rating);
    setData(index(row, G::RatingColumn), Qt::AlignCenter, Qt::TextAlignmentRole);
    setData(index(row, G::_RatingColumn), m._rating);
    setData(index(row, G::CreatedColumn), m.createdDate.toString("yyyy-MM-dd hh:mm:ss"));
    setData(index(row, G::YearColumn), m.createdDate.toString("yyyy"));
    setData(index(row, G::DayColumn), m.createdDate.toString("yyyy-MM-dd"));
     search += m.createdDate.toString("yyyy-MM-dd");
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
    setData(index(row, G::OffsetFullColumn), m.offsetFull);
    setData(index(row, G::LengthFullColumn), m.lengthFull);
    setData(index(row, G::WidthPreviewColumn), m.widthPreview);
    setData(index(row, G::HeightPreviewColumn), m.heightPreview);
    setData(index(row, G::OffsetThumbColumn), m.offsetThumb);
    setData(index(row, G::LengthThumbColumn), m.lengthThumb);
    setData(index(row, G::samplesPerPixelColumn), m.samplesPerPixel); // reqd for err trapping
//   setData(index(row, G::OffsetSmallColumn), m.offsetSmall);
//   setData(index(row, G::LengthSmallColumn), m.lengthSmall);
//   setData(index(row, G::bitsPerSampleColumn), m.bitsPerSample);
//   setData(index(row, G::photoInterpColumn), m.photoInterp);
//   setData(index(row, G::compressionColumn), m.compression);
//   setData(index(row, G::stripByteCountsColumn), m.stripByteCounts);
//   setData(index(row, G::planarConfigurationFullColumn), m.stripByteCounts);
    line = 1110;
    setData(index(row, G::isBigEndianColumn), m.isBigEnd);
    setData(index(row, G::ifd0OffsetColumn), m.ifd0Offset);
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
    setData(index(row, G::MetadataLoadedColumn), m.metadataLoaded);
    setData(index(row, G::SearchTextColumn), search.toLower());
    setData(index(row, G::SearchTextColumn), search.toLower(), Qt::ToolTipRole);

    // req'd for 1st image, probably loaded before metadata cached
    line = 1128; if (row == 0) emit updateClassification();
//    mutex.unlock();
    mLock = false;
    if (isDebug) qDebug() << "DataModel::addMetadataForItem" << "instance =" << instance << "DONE";
    return true;
}

bool DataModel::metadataLoaded(int dmRow)
{
    lastFunction = "";
    if (G::isLogger) G::log("DataModel::metadataLoaded");
    if (isDebug) qDebug() << "DataModel::metadataLoaded" << "instance =" << instance
                          << "row =" << dmRow
                          << currentFolderPath;
    return index(dmRow, G::MetadataLoadedColumn).data().toBool();
}

bool DataModel::instanceClash(QModelIndex idx, QString src)
{
    lastFunction = "";
    if (isDebug) qDebug() << "DataModel::metadataLoaded" << "instance =" << instance
                          << currentFolderPath;
    return false;
    QModelIndex par = idx.parent();
    bool clash = &par != &instanceParent;
    if (clash) {
        qWarning()  << "WARNING" << "DataModel::instanceClash" << src << idx;
    }
    return &par != &instanceParent;
}

double DataModel::aspectRatio(int w, int h, int orientation)
{
    lastFunction = "";
    if (G::isLogger) G::log("DataModel::aspectRatio");
    if (isDebug) qDebug() << "DataModel::aspectRatio" << "instance =" << instance << currentFolderPath;
    if (w == 0 || h == 0) return 1.0;
    // portrait
    if (orientation == 8) return h * 1.0 / w;
    // landscape
    else return w * 1.0 / h;
}

void DataModel::setValue(QModelIndex dmIdx, QVariant value, int instance,
                         QString src, int role, int align)
{
    lastFunction = "";
    if (G::stop) return;
    if (isDebug) qDebug() << "DataModel::setValue" << "instance =" << instance
                          << "row =" << dmIdx.row()
                          << currentFolderPath;
    if (instance != this->instance) {
        qWarning() << "WARNING" << "DataModel::setValue" << dmIdx << "Instance conflict = "
                 << "DM instance =" << this->instance
                 << "Src instance =" << instance
                    ;
        return;
    }
    QModelIndex cIdx = index(dmIdx.row(),dmIdx.column());
    /*
    qDebug() << "DataModel::setValue"
             << "Instance =" << instance
             << "this Instance =" << this->instance
             << "G::stop =" << G::stop
             << "src =" << src
             << "dmIdx =" << dmIdx
             << "cIdx =" << cIdx
             << "rowCount() =" << rowCount()
             << "value =" << value
             << currentFolderPath;
    //*/
    mutex.lock();
    /*
    qDebug() << "DataModel::setValue"
             << "dmIdx.isValid =" << dmIdx.isValid()
             << "dmIdx =" << dmIdx
             << "value =" << value
                ;
                //*/
    if (!dmIdx.isValid()) {
        qWarning() << "WARNING" << "DataModel::setValue" << "dmIdx.isValid() =" << dmIdx.isValid() << dmIdx;
        return;
    }
    setData(dmIdx, value, role);
    setData(dmIdx, align, Qt::TextAlignmentRole);
    mutex.unlock();
}

void DataModel::setValueSf(QModelIndex sfIdx, QVariant value, int instance,
                           QString src, int role, int align)
{
    lastFunction = "";
    if (G::stop) return;
    if (isDebug) qDebug() << "DataModel::setValueSf" << "instance =" << instance
                          << "row =" << sfIdx.row()
                          << currentFolderPath;
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
    if (instance != this->instance) {
        qWarning() << "WARNING" << "DataModel::setValueSf" << sfIdx << "Instance conflict = "
                 << "DM instance =" << this->instance
                 << "Src instance =" << instance
                    ;
        return ;
    }
    if (!sfIdx.isValid()) {
        qWarning() << "WARNING" << "DataModel::setValueSf" << "sfIdx.isValid() =" << sfIdx.isValid() << sfIdx;
        return;
    }
    mutex.lock();
    sf->setData(sfIdx, value, role);
    setData(sfIdx, align, Qt::TextAlignmentRole);
    mutex.unlock();
}

void DataModel::setValuePath(QString fPath, int col, QVariant value, int instance, int role)
{
    lastFunction = "";
    if (isDebug) qDebug() << "DataModel::setValuePath" << "instance =" << instance
                          << "col =" << col
                          << currentFolderPath;
    /*
    qDebug() << "DataModel::setValuePath"
             << fPath
             << fPathRow[fPath]
             << col
                ;
                //*/
    if (instance != this->instance) {
        qWarning() << "WARNING" << "DataModel::setValuePath" << "Instance conflict = "
                 << "DM instance =" << this->instance
                 << "Src instance =" << instance
                    ;
        return;
    }
    if (G::stop) return;
//    qDebug() << "DataModel::setValuePath" << "Instance =" << instance << currentFolderPath;
    QModelIndex dmIdx = index(fPathRow[fPath], col);
    if (!dmIdx.isValid()) {
        qWarning() << "WARNING" << "DataModel::setValuePath" << "dmIdx.isValid() =" << dmIdx.isValid() << dmIdx;
        return;
    }
    mutex.lock();
    setData(dmIdx, value, role);
    mutex.unlock();
}

void DataModel::setIconFromVideoFrame(QModelIndex dmIdx, QPixmap &pm, int fromInstance,
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
    lastFunction = "";
    if (G::isLogger) G::log("DataModel::setIconFromVideoFrame");
    if (isDebug) qDebug() << "DataModel::setIconFromVideoFrame" << "instance =" << instance
                          << "fromInstance =" << fromInstance
                          << "row =" << dmIdx.row()
                          << currentFolderPath;

    if (G::stop) return;
    if (!dmIdx.isValid()) {
        qWarning() << "WARNING" << "DataModel::setIconFromVideoFrame" << "dmIdx.isValid() =" << dmIdx.isValid() << dmIdx;
        return;
    }
    if (fromInstance != instance) {
        qWarning() << "WARNING" << "setIconFromVideoFrame" << dmIdx << "Instance conflict = "
                   << "DM instance =" << instance
                   << "Src instance =" << fromInstance;
        return;
    }
//    qDebug() << "DataModel::setIconFromVideoFrame" << "Instance =" << instance << currentFolderPath;

    int row = dmIdx.row();
//    qDebug() << "DataModel::setIconFromVideoFrame       row =" << row;
    QString modelDuration = index(dmIdx.row(), G::DurationColumn).data().toString();
    if (modelDuration == "") {
        duration /= 1000;
        QTime durationTime((duration / 3600) % 60, (duration / 60) % 60,
            duration % 60, (duration * 1000) % 1000);
        QString format = "mm:ss";
        if (duration > 3600) format = "hh:mm:ss";
        setData(index(row, G::DurationColumn), durationTime.toString(format));
    }
//    qDebug() << "DataModel::setIconFromVideoFrame  itemFromIndex" << dmIdx;
    QStandardItem *item = itemFromIndex(dmIdx);
    if (itemFromIndex(dmIdx)->icon().isNull()) {
        if (item != nullptr) {
            item->setIcon(pm);
            // set aspect ratio for video
            if (pm.height() > 0) {
                QString aspectRatio = QString::number(pm.width() * 1.0 / pm.height(), 'f', 2);
                setData(index(row, G::AspectRatioColumn), aspectRatio);
            }
            setIconMax(pm);
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
*/
    lastFunction = "";
    if (G::isLogger) G::log("DataModel::setIcon");
    if (isDebug) {
        qDebug() << "DataModel::setIcon" << "instance =" << instance
                 << "fromInstance =" << fromInstance
                 << "row =" << dmIdx.row()
                 << currentFolderPath;
    }
    if (loadingModel) {
        qWarning() << "WARNING" << "DataModel::setIcon" << "loadingModel =" << loadingModel;
        return;
    }
    if (fromInstance != instance) {
        qWarning() << "WARNING" << "DataModel::setIcon" << dmIdx << "Instance conflict = "
                 << "DM instance =" << instance
                 << "Src instance =" << fromInstance
                 << "Src =" << src
                    ;
        return;
    }
    if (loadingModel) {
        qWarning() << "WARNING" << "DataModel::setIcon" << dmIdx << "loadingModel = " << loadingModel;
        return;
    }
    if (G::stop) {
        qWarning() << "WARNING" << "DataModel::setIcon" << dmIdx << "G::stop = " << G::stop;
        return;
    }
    if (!dmIdx.isValid()) {
        qWarning() << "WARNING" << "DataModel::setIcon" << "dmIdx.isValid() =" << dmIdx.isValid() << dmIdx;
        return;
    }

    // mutex.lock();
    QStandardItem *item = itemFromIndex(dmIdx);
    /* const QIcon icon(pm) - required to prevent occasional malloc deallocation error
       in qarraydata.h deallocate:
        static void deallocate(QArrayData *data) noexcept
        {
            static_assert(sizeof(QTypedArrayData) == sizeof(QArrayData));
            QArrayData::deallocate(data, sizeof(T), alignof(AlignmentDummy)); // crashes here
        }
    */
    const QIcon icon(pm);
    item->setIcon(icon);
    setData(dmIdx, false, G::CachingIconRole);
    setIconMax(pm);
    // mutex.unlock();
}

void DataModel::setIconMax(const QPixmap &pm)
{
/*
    Used locally in DataModel.
*/
    lastFunction = "";
    if (G::isLogger) G::log("DataModel::setIconMax");
    if (isDebug) qDebug() << "DataModel::setIconMax" << "instance =" << instance << currentFolderPath;
    if (G::iconWMax == G::maxIconSize && G::iconHMax == G::maxIconSize) return;

//    qDebug() << "DataModel::setIconMax" << "Instance =" << instance << currentFolderPath;
    // for best aspect calc
    int w = pm.width();
    int h = pm.height();
    if (w > G::iconWMax) G::iconWMax = w;
    if (h > G::iconHMax) G::iconHMax = h;
}

bool DataModel::iconLoaded(int sfRow, int instance)
{
    lastFunction = "";
    if (G::isLogger) G::log("DataModel::iconLoaded");
    if (isDebug) qDebug() << "DataModel::iconLoaded" << "instance =" << this->instance
                          << "fromInstance =" << instance
                          << "row =" << sfRow
                          << currentFolderPath;
    /*
    qDebug() << "DataModel::iconLoaded"
             << "G::stop =" << G::stop
             << "sfRow =" << sfRow
             << "rowCount() =" << rowCount()
             << "Instance =" << instance
             << currentFolderPath;
             //*/
    // might be called from previous folder during folder change
    if (instance != this->instance) {
        qWarning() << "WARNING" << "DataModel::iconLoaded" << sfRow << "Instance conflict = "
                 << "DM instance =" << this->instance
                 << "Src instance =" << instance
                    ;
        return true;
    }
    if (sfRow >= sf->rowCount()) return true;
    QModelIndex dmIdx = sf->mapToSource(sf->index(sfRow, 0));
//    qDebug() << "DataModel::iconLoaded  itemFromIndex" << dmIdx;
    return !(itemFromIndex(dmIdx)->icon().isNull());
}

int DataModel::iconCount()
{
    lastFunction = "";
    if (isDebug) qDebug() << "DataModel::iconCount" << "instance =" << instance << currentFolderPath;
    int count = 0;
    for (int row = 0; row < rowCount(); ++row) {
//        qDebug() << "DataModel::iconCount  itemFromIndex  row =" << row;
        if (!itemFromIndex(index(row, 0))->icon().isNull()) count++;
    }
    return count;
}

bool DataModel::allIconsLoaded()
{
    lastFunction = "";
    if (isDebug) qDebug() << "DataModel::allIconsLoaded" << "instance =" << instance << currentFolderPath;
    for (int row = 0; row < rowCount(); ++row) {
//        qDebug() << "DataModel::allIconsLoaded  itemFromIndex  row =" << row;
        if (itemFromIndex(index(row, 0))->icon().isNull()) return false;
    }
    return true;
}

void DataModel::clearAllIcons()
{
    lastFunction = "";
    if (isDebug) qDebug() << "DataModel::clearAllIcons" << "instance =" << instance << currentFolderPath;
    mutex.lock();
    QStandardItem *item;
    for (int row = 0; row < rowCount(); ++row) {
//        qDebug() << "DataModel::clearAllIcons  itemFromIndex  row =" << row;
        item = itemFromIndex(index(row, 0));
        if (!item->icon().isNull()) {
            item->setIcon(QIcon());
        }
    }
    mutex.unlock();
}

void DataModel::setIconRange(int first, int last)
{
    lastFunction = "";
    if (isDebug) qDebug() << "DataModel::setIconRange" << "instance =" << instance << currentFolderPath;
    mutex.lock();
    startIconRange = first;
    endIconRange = last;
    mutex.unlock();
}

void DataModel::clearOutOfRangeIcons(int startRow)
{
/*
    Not used.  See MetaRead::cleanupIcons
*/
//    qDebug() << "DataModel::clearOutOfRangeIcons" << startRow;
    lastFunction = "";
    if (isDebug) qDebug() << "DataModel::clearOutOfRangeIcons" << "instance =" << instance
                          << "startRow =" << startRow
                          << currentFolderPath;
    mutex.lock();
    startIconRange = startRow - iconChunkSize / 2;
    if (startIconRange< 0) startIconRange = 0;
    endIconRange = startIconRange + iconChunkSize;
    for (int row = 0; row < rowCount(); ++row) {
        int sfRow = sf->mapFromSource(index(row, 0)).row();
        if (sfRow >= startIconRange && sfRow <= endIconRange) {
            continue;
        }
//        qDebug() << "DataModel::clearOutOfRangeIcons  itemFromIndex  row =" << row;
        QStandardItem *item = itemFromIndex(index(row, 0));
        if (item->icon().isNull()) {
            continue;
        }
        item->setIcon(QIcon());
    }
    mutex.unlock();
}

void DataModel::setAllMetadataLoaded(bool isLoaded)
{
    lastFunction = "";
    if (isDebug) qDebug() << "DataModel::setAllMetadataLoaded" << "instance =" << instance << currentFolderPath;
    G::allMetadataLoaded = isLoaded;
    filters->loadedDataModel(isLoaded);
}

bool DataModel::isAllMetadataLoaded()
{
    lastFunction = "";
    if (isDebug) qDebug() << "DataModel::isAllMetadataLoaded" << "instance =" << instance << currentFolderPath;
    for (int row = 0; row < rowCount(); ++row) {
        if (!index(row, G::MetadataLoadedColumn).data().toBool()) return false;
    }
    return true;
}

int DataModel::rowFromPath(QString fPath)
{
    lastFunction = "";
    if (isDebug) qDebug() << "DataModel::rowFromPath" << "instance =" << instance << fPath << currentFolderPath;
    if (G::isLogger) G::log("DataModel::rowFromPath");
    if (fPathRow.contains(fPath)) return fPathRow[fPath];
    else return -1;
}

void DataModel::refreshRowFromPath()
{
    lastFunction = "";
    if (G::isLogger) G::log("DataModel::refreshRowFromPath");
    if (isDebug) qDebug() << "DataModel::refreshRowFromPath" << "instance =" << instance << currentFolderPath;
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
    lastFunction = "";
    if (G::isLogger) G::log("DataModel::hasFolderChanged");
    if (isDebug) qDebug() << "DataModel::hasFolderChanged" << "instance =" << instance << currentFolderPath;
    bool hasChanged = false;
    modifiedFiles.clear();
    QList<QFileInfo> fileInfoList2;
    dir->setPath(currentFolderPath);
    for (int i = 0; i < dir->entryInfoList().size(); ++i) {
        fileInfoList2.append(dir->entryInfoList().at(i));
    }
    if (includeSubfolders) {
        QDirIterator it(currentFolderPath, QDirIterator::Subdirectories);
        while (it.hasNext()) {
            it.next();
            if (it.fileInfo().isDir() && it.fileName() != "." && it.fileName() != "..") {
                dir->setPath(it.filePath());
                int folderImageCount = dir->entryInfoList().size();
                // try next subfolder if no images in this folder
                if (!folderImageCount) continue;
                // add supported images in folder to image list
                for (int i = 0; i < folderImageCount; ++i) {
                    fileInfoList2.append(dir->entryInfoList().at(i));
                }
            }
        }
    }

    int oldCount = fileInfoList.count();
    int newCount = fileInfoList2.count();
    if (newCount < oldCount) return true;

    bool isFileModification = false;
    for (int i = 0; i < fileInfoList.count(); ++i) {
        QDateTime t = fileInfoList.at(i).lastModified();
        QDateTime t2 = fileInfoList2.at(i).lastModified();
        if (t != t2) {
            hasChanged = true;
            isFileModification = true;
            modifiedFiles.append(fileInfoList2.at(i));
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
    lastFunction = "";
    if (G::isLogger) G::log("DataModel::searchStringChange");
    if (isDebug) qDebug() << "DataModel::searchStringChange" << "instance =" << instance
                          << "searchString =" << searchString
                          << currentFolderPath;
    // update datamodel search string match
    mutex.lock();
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
    mutex.unlock();
}

void DataModel::rebuildTypeFilter()
{
/*
    When Raw+Jpg is toggled in the main program MW the file type filter must
    be rebuilt.
*/
    lastFunction = "";
    if (G::isLogger) G::log("DataModel::rebuildTypeFilter");
    if (isDebug) qDebug() << "DataModel::rebuildTypeFilter" << "instance =" << instance << currentFolderPath;
    filters->types->takeChildren();
    QStringList typeList;
    int rows = sf->rowCount();
    for(int row = 0; row < rows; row++) {
        QString type = sf->index(row, G::TypeColumn).data().toString();
        if (!typeList.contains(type)) {
            typeList.append(type);
        }
    }
    filters->addCategoryFromData(typeList, filters->types);
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
    lastFunction = "";
    if (G::isLogger) G::log("DataModel::proxyIndexFromPath");
    if (isDebug) qDebug() << "DataModel::proxyIndexFromPath" << "instance =" << instance
                          << "fPath =" << fPath
                          << currentFolderPath;
    if (!fPathRow.contains(fPath)) {
        qWarning() << "WARNING" << "DataModel::proxyIndexFromPath" << "Not in fPathrow";
        return index(-1, -1);
    }
    int dmRow = fPathRow[fPath];
    QModelIndex sfIdx = sf->mapFromSource(index(dmRow, 0));
    if (sfIdx.isValid()) {
        return sfIdx;
    }
    else {
        qWarning() << "WARNING" << "DataModel::proxyIndexFromPath" << "Invalid proxy";
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

int DataModel::proxyRowFromModelRow(int dmRow) {
    lastFunction = "";
    if (G::isLogger) G::log("DataModel::proxyRowFromModelRow");
    if (isDebug) qDebug() << "DataModel::proxyRowFromModelRow" << "instance =" << instance
                          << "row =" << dmRow
                          << currentFolderPath;
    return sf->mapFromSource(index(dmRow, 0)).row();
}

int DataModel::modelRowFromProxyRow(int sfRow)
{
    lastFunction = "";
    if (G::isLogger) G::log("DataModel::modelRowFromProxyRow");
    if (isDebug) qDebug() << "DataModel::modelRowFromProxyRow" << "instance =" << instance
                          << "row =" << sfRow
                          << currentFolderPath;
    return sf->mapToSource(sf->index(sfRow, 0)).row();
}

//void DataModel::selectNext()
//{
//    if (G::isLogger) G::log("DataModel::selectNext");
//    if (G::mode == "Compare") return;
//    int row = currentRow + 1;
//    if (row >= sf->rowCount()) return;
//    select(row);
//}

//void DataModel::selectPrev()
//{
//    if (G::isLogger) G::log("DataModel::selectPrev");
//    if (G::mode == "Compare") return;
//    int row = currentRow - 1;
//    if (row < 0) return;
//    select(row);
//}

void DataModel::selectAll()
{
    lastFunction = "";
    if (G::isLogger) G::log("DataModel::selectAll");
    if (isDebug) qDebug() << "DataModel::selectAll" << "instance =" << instance << currentFolderPath;
    QItemSelection selection;
    QModelIndex first = sf->index(0, 0);
    QModelIndex last = sf->index(sf->rowCount() - 1, 0);
    selection.select(first, last);
    selectionModel->select(selection,
                           QItemSelectionModel::Clear |
                           QItemSelectionModel::Select |
                           QItemSelectionModel::Current |
                           QItemSelectionModel::Rows);
}

void DataModel::selectFirst()
{
    lastFunction = "";
    if (G::isLogger) G::log("DataModel::selectFirst");
    if (isDebug) qDebug() << "DataModel::selectFirst" << "instance =" << instance << currentFolderPath;
    select(sf->index(0, 0));
}

void DataModel::selectLast()
{
    lastFunction = "";
    if (G::isLogger) G::log("DataModel::selectLast");
    if (isDebug) qDebug() << "DataModel::selectLast" << "instance =" << instance << currentFolderPath;
    select(sf->index(sf->rowCount() - 1, 0));
}

void DataModel::select(QString &fPath)
{
    lastFunction = "";
    if (G::isLogger) G::log("DataModel::select QString");
    if (isDebug) qDebug() << "DataModel::select QString" << "instance =" << instance << currentFolderPath;
    select(proxyIndexFromPath(fPath));
}

void DataModel::select(int row)
{
    lastFunction = "";
    if (G::isLogger) G::log("DataModel::select int");
    if (isDebug) qDebug() << "DataModel::select int" << "instance =" << instance
                          << "row =" << row
                          << currentFolderPath;
    select(sf->index(row,0));
}

void DataModel::select(QModelIndex idx)
{
    lastFunction = "";
    if (G::isLogger) G::log("DataModel::select QModelIndex");
    if (isDebug) qDebug() << "DataModel::select QModelIndex" << "instance =" << instance
                          << "idx =" << idx
                          << currentFolderPath;
    if (idx.isValid()) {
        selectionModel->setCurrentIndex(idx, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
    }
}

void DataModel::saveSelection()
{
/*
    This function saves the current selection. This is required, even though the three views
    (thumbView, gridView and tableViews) share the same selection model, because when a view
    is hidden it loses the current index and selection, which has to be re-established each
    time it is made visible.
*/
    lastFunction = "";
    if (G::isLogger) G::log("DataModel::saveSelection");
    if (isDebug) qDebug() << "DataModel::saveSelection" << "instance =" << instance << currentFolderPath;
    selectedRows = selectionModel->selectedRows();
    currentSfIdx = selectionModel->currentIndex();
}

void DataModel::recoverSelection()
{
    lastFunction = "";
    if (G::isLogger) G::log("DataModel::recoverSelection");
    if (isDebug) qDebug() << "DataModel::recoverSelection" << "instance =" << instance << currentFolderPath;
    QItemSelection selection;
    QModelIndex idx;
    foreach (idx, selectedRows)
        selection.select(idx, idx);
    selectionModel->select(selection, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
}

bool DataModel::getSelection(QStringList &list)
{
/*
    Adds each image that is selected or picked as a file path to list. If there are picks and
    a selection then a dialog offers the user a choice to use.
*/
    lastFunction = "";
    if (G::isLogger) G::log("DataModel::getSelection");
    if (isDebug) qDebug() << "DataModel::getSelection" << "instance =" << instance << currentFolderPath;

    bool usePicks = false;

    // nothing picked or selected
    if (isPick() && selectionModel->selectedRows().size() == 0) {
//        QMessageBox::information(mw, "Oops",
//           "There are no picks or selected images to report.    ",
//           QMessageBox::Ok);
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
    lastFunction = "";
    if (G::isLogger) G::log("DataModel::getSelectionOrPicks");
    if (isDebug) qDebug() << "DataModel::getSelectionOrPicks" << "instance =" << instance << currentFolderPath;

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
    lastFunction = "";
    if (G::isLogger) G::log("DataModel::isPick");
    if (isDebug) qDebug() << "DataModel::isPick" << "instance =" << instance << currentFolderPath;
    for (int row = 0; row < sf->rowCount(); ++row) {
        QModelIndex idx = sf->index(row, G::PickColumn);
        if (idx.data(Qt::EditRole).toString() == "true") return true;
    }
    return false;
}

void DataModel::clearPicks()
{
/*
    reset all the picks to false.
*/
    lastFunction = "";
    if (G::isLogger) G::log("DataModel::clearPicks");
    if (isDebug) qDebug() << "DataModel::clearPicks" << "instance =" << instance << currentFolderPath;
    mutex.lock();
    for(int row = 0; row < sf->rowCount(); row++) {
        setData(index(row, G::PickColumn), "false");
    }
    mutex.unlock();
}

QString DataModel::diagnosticsErrors()
{
    lastFunction = "";
    if (G::isLogger) G::log("DataModel::diagnosticsErrors");
    if (isDebug) qDebug() << "DataModel::diagnosticsErrors" << "instance =" << instance << currentFolderPath;
    QString reportString;
    QTextStream rpt;
    rpt.setString(&reportString);
    rpt << Utilities::centeredRptHdr('=', "Error Listing");
    rpt << "\n\n";
    if (G::err.isEmpty()) {
        rpt << "No errors" << "\n";
        rpt << "\n\n" ;
        return reportString;
    }
    QMapIterator<QString,QStringList> item(G::err);
    while (item.hasNext()) {
        item.next();
        if (item.value().size() == 0) continue;
        // key = file path
        rpt << item.key() + "\n";
        // value = QStringList of errors for the key
        for (int error = 0; error < item.value().size(); ++error) {
            rpt << "  " << item.value().at(error) + "\n";
        }
    }
    rpt << "\n\n" ;
    return reportString;
}

QString DataModel::diagnostics()
{
    lastFunction = "";
    if (G::isLogger) G::log("DataModel::diagnostics");
    if (isDebug) qDebug() << "DataModel::diagnostics" << "instance =" << instance << currentFolderPath;
    QString reportString;
    QTextStream rpt;
    rpt.setString(&reportString);
    rpt << Utilities::centeredRptHdr('=', "DataModel Diagnostics");
    rpt << "\n";
    rpt << "\n" << G::sj("currentFolderPath", 27) << G::s(currentFolderPath);
    rpt << "\n" << G::sj("currentFilePath", 27) << G::s(currentFilePath);
    rpt << "\n" << G::sj("currentRow", 27) << G::s(currentSfRow);
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
    rpt << "\n" << G::sj("countInterval", 27) << G::s(countInterval);
    for(int row = 0; row < rowCount(); row++) {
        rpt << "\n";
        getDiagnosticsForRow(row, rpt);
    }
    rpt << "\n\n" ;
    return reportString;
}

QString DataModel::diagnosticsForCurrentRow()
{
    lastFunction = "";
    if (G::isLogger) G::log("DataModel::diagnosticsForCurrentRow");
    if (isDebug) qDebug() << "DataModel::diagnosticsForCurrentRow" << "instance =" << instance << currentFolderPath;
    QString reportString;
    QTextStream rpt;
    rpt.setString(&reportString);
    rpt << Utilities::centeredRptHdr('=', "DataModel Diagnostics");
    rpt << "\n";
//    rpt << "\n" << G::sj("", 25)"currentFolderPath = " << G::s(currentFolderPath);
//    rpt << "\n" << G::sj("", 25)"currentFilePath = " << G::s(currentFilePath);
//    rpt << "\n" << G::sj("", 25)"currentRow = " << G::s(currentRow);
    rpt << "\n" << G::sj("hasDupRawJpg", 27) << G::s(hasDupRawJpg);
//    rpt << "\n" << G::sj("", 25)"filtersBuilt = " << G::s(filtersBuilt);
//    rpt << "\n" << G::sj("", 25)"timeToQuit = " << G::s(timeToQuit);
//    rpt << "\n" << G::sj("", 25)"imageCount = " << G::s(imageCount);
//    rpt << "\n" << G::sj("", 25)"countInterval = " << G::s(countInterval);
    getDiagnosticsForRow(currentSfRow, rpt);
    rpt << "\n\n" ;
    return reportString;
}

void DataModel::getDiagnosticsForRow(int row, QTextStream& rpt)
{
    lastFunction = "";
    if (G::isLogger) G::log("DataModel::getDiagnosticsForRow");
    if (isDebug) qDebug() << "DataModel::getDiagnosticsForRow" << "instance =" << instance << currentFolderPath;
    QString s = "";
    rpt << "\n"   << G::sj("DataModel row", 27) << G::s(row);
    rpt << "\n  " << G::sj("FileName", 25) << G::s(index(row, G::NameColumn).data());
    rpt << "\n  " << G::sj("FilePath", 25) << G::s(index(row, 0).data(G::PathRole));
    rpt << "\n  " << G::sj("isIcon", 25) << G::s(!itemFromIndex(index(row, G::PathColumn))->icon().isNull());
    rpt << "\n  " << G::sj("isCached", 25) << G::s(index(row, 0).data(G::CachedRole));
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
    rpt << "\n  " << G::sj("refine", 25) << G::s(index(row, G::RefineColumn).data());
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
    // Check Raw + Jpg
    if (combineRawJpg) {
        QModelIndex rawIdx = sourceModel()->index(sourceRow, 0, sourceParent);
        if (rawIdx.data(G::DupHideRawRole).toBool()) return false;
    }

    if (!G::isNewFolderLoaded) return true;

    static int counter = 0;
    counter++;
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
            if ((*filter)->checkState(0) != Qt::Unchecked) {
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
                    qDebug() << "DataModel::filterAcceptsRow" << "\t" << itemCategory << itemName
                             << "sfRow" << sourceRow
                             << "Comparing" << dataValue << filterValue
                             << (dataValue == filterValue);
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
//            qDebug() << G::t.restart() << "\t" << G::sj("", 25)"Category" << itemCategory << isMatch;
            if (!isMatch) return false;   // no match in category

            /* prepare for category items filter match.  If no item is checked
            or one checked item matches the data then the row is okay to show
            */
            // the top level items contain reference to the data model column
            dataModelColumn = (*filter)->data(0, G::ColumnRole).toInt();
            isCategoryUnchecked = true;
            isMatch = false;
            itemCategory = (*filter)->text(0);      // for debugging
        }
        ++filter;
    }
    // check results of category items filter match for the last group
    if (isCategoryUnchecked) isMatch = true;
    return isMatch;
}

void SortFilter::filterChange()
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
    invalidateFilter();
}
