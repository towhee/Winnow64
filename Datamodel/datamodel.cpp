#include "Datamodel/datamodel.h"

/*
The datamodel (dm thoughout app) contains information about each eligible image
file in the selected folder (and optionally the subfolder heirarchy).  Eligible
image files, defined in the metadata class, are files Winnow knows how to decode.

The data is structured in columns:

    ● Path:             from QFileInfoList  FilePathRole (absolutePath)
                        from QFileInfoList  ToolTipRole
                                            G::ThumbRectRole (icon)
                                            G::DupIsJpgRole
                                            G::DupRawIdxRole
                                            G::DupHideRawRole
                                            G::DupRawTypeRole
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
                     ProgressBar *progressBar,
                     Filters *filters,
                     bool &combineRawJpg) :

                     QStandardItemModel(parent),
                     combineRawJpg(combineRawJpg)
{

    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    mw = parent;
    this->metadata = metadata;
    this->filters = filters;
    this->progressBar = progressBar;

    setSortRole(Qt::EditRole);

    // must include all prior Global dataModelColumns (any order okay)
    setHorizontalHeaderItem(G::PathColumn, new QStandardItem("Icon")); horizontalHeaderItem(G::PathColumn)->setData(false, G::GeekRole);
    setHorizontalHeaderItem(G::IconLoadedColumn, new QStandardItem("Icon Loaded")); horizontalHeaderItem(G::IconLoadedColumn)->setData(true, G::GeekRole);
    setHorizontalHeaderItem(G::NameColumn, new QStandardItem("File Name")); horizontalHeaderItem(G::NameColumn)->setData(false, G::GeekRole);
    setHorizontalHeaderItem(G::RefineColumn, new QStandardItem("Refine")); horizontalHeaderItem(G::RefineColumn)->setData(false, G::GeekRole);
    setHorizontalHeaderItem(G::PickColumn, new QStandardItem("Pick")); horizontalHeaderItem(G::PickColumn)->setData(false, G::GeekRole);
    setHorizontalHeaderItem(G::IngestedColumn, new QStandardItem("Ingested")); horizontalHeaderItem(G::IngestedColumn)->setData(false, G::GeekRole);
    setHorizontalHeaderItem(G::LabelColumn, new QStandardItem("Colour")); horizontalHeaderItem(G::LabelColumn)->setData(false, G::GeekRole);
    setHorizontalHeaderItem(G::RatingColumn, new QStandardItem("Rating")); horizontalHeaderItem(G::RatingColumn)->setData(false, G::GeekRole);
    setHorizontalHeaderItem(G::SearchColumn, new QStandardItem("🔎")); horizontalHeaderItem(G::SearchColumn)->setData(false, G::GeekRole);
    setHorizontalHeaderItem(G::TypeColumn, new QStandardItem("Type")); horizontalHeaderItem(G::TypeColumn)->setData(false, G::GeekRole);
    setHorizontalHeaderItem(G::SizeColumn, new QStandardItem("Size")); horizontalHeaderItem(G::SizeColumn)->setData(false, G::GeekRole);
    setHorizontalHeaderItem(G::WidthColumn, new QStandardItem("Width")); horizontalHeaderItem(G::WidthColumn)->setData(false, G::GeekRole);
    setHorizontalHeaderItem(G::HeightColumn, new QStandardItem("Height")); horizontalHeaderItem(G::HeightColumn)->setData(false, G::GeekRole);
    setHorizontalHeaderItem(G::DimensionsColumn, new QStandardItem("Dimensions")); horizontalHeaderItem(G::DimensionsColumn)->setData(false, G::GeekRole);
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
    setHorizontalHeaderItem(G::CameraMakeColumn, new QStandardItem("Make")); horizontalHeaderItem(G::CameraMakeColumn)->setData(false, G::GeekRole);
    setHorizontalHeaderItem(G::CameraModelColumn, new QStandardItem("Model")); horizontalHeaderItem(G::CameraModelColumn)->setData(false, G::GeekRole);
    setHorizontalHeaderItem(G::LensColumn, new QStandardItem("Lens")); horizontalHeaderItem(G::LensColumn)->setData(false, G::GeekRole);
    setHorizontalHeaderItem(G::FocalLengthColumn, new QStandardItem("Focal length")); horizontalHeaderItem(G::FocalLengthColumn)->setData(false, G::GeekRole);
    setHorizontalHeaderItem(G::TitleColumn, new QStandardItem("Title")); horizontalHeaderItem(G::TitleColumn)->setData(false, G::GeekRole);
    setHorizontalHeaderItem(G::CopyrightColumn, new QStandardItem("Copyright")); horizontalHeaderItem(G::CopyrightColumn)->setData(false, G::GeekRole);
    setHorizontalHeaderItem(G::EmailColumn, new QStandardItem("Email")); horizontalHeaderItem(G::EmailColumn)->setData(false, G::GeekRole);
    setHorizontalHeaderItem(G::UrlColumn, new QStandardItem("Url")); horizontalHeaderItem(G::UrlColumn)->setData(false, G::GeekRole);

    setHorizontalHeaderItem(G::MetadataLoadedColumn, new QStandardItem("Meta Loaded")); horizontalHeaderItem(G::MetadataLoadedColumn)->setData(true, G::GeekRole);
    setHorizontalHeaderItem(G::_RatingColumn, new QStandardItem("_Rating")); horizontalHeaderItem(G::_RatingColumn)->setData(true, G::GeekRole);
    setHorizontalHeaderItem(G::_LabelColumn, new QStandardItem("_Label")); horizontalHeaderItem(G::_LabelColumn)->setData(true, G::GeekRole);
    setHorizontalHeaderItem(G::_CreatorColumn, new QStandardItem("_Creator")); horizontalHeaderItem(G::_CreatorColumn)->setData(true, G::GeekRole);
    setHorizontalHeaderItem(G::_TitleColumn, new QStandardItem("_Title")); horizontalHeaderItem(G::_TitleColumn)->setData(true, G::GeekRole);
    setHorizontalHeaderItem(G::_CopyrightColumn, new QStandardItem("_Copyright")); horizontalHeaderItem(G::_CopyrightColumn)->setData(true, G::GeekRole);
    setHorizontalHeaderItem(G::_EmailColumn, new QStandardItem("_Email")); horizontalHeaderItem(G::_EmailColumn)->setData(true, G::GeekRole);
    setHorizontalHeaderItem(G::_UrlColumn, new QStandardItem("_Url")); horizontalHeaderItem(G::_UrlColumn)->setData(true, G::GeekRole);

    setHorizontalHeaderItem(G::OffsetFullColumn, new QStandardItem("OffsetFull")); horizontalHeaderItem(G::OffsetFullColumn)->setData(true, G::GeekRole);
    setHorizontalHeaderItem(G::LengthFullColumn, new QStandardItem("LengthFull")); horizontalHeaderItem(G::LengthFullColumn)->setData(true, G::GeekRole);
    setHorizontalHeaderItem(G::WidthFullColumn, new QStandardItem("WidthFull")); horizontalHeaderItem(G::WidthFullColumn)->setData(true, G::GeekRole);
    setHorizontalHeaderItem(G::HeightFullColumn, new QStandardItem("HeightFull")); horizontalHeaderItem(G::HeightFullColumn)->setData(true, G::GeekRole);
    setHorizontalHeaderItem(G::OffsetThumbColumn, new QStandardItem("OffsetThumb")); horizontalHeaderItem(G::OffsetThumbColumn)->setData(true, G::GeekRole);
    setHorizontalHeaderItem(G::LengthThumbColumn, new QStandardItem("LengthThumb")); horizontalHeaderItem(G::LengthThumbColumn)->setData(true, G::GeekRole);
    setHorizontalHeaderItem(G::samplesPerPixelColumn, new QStandardItem("samplesPerPixelFull")); horizontalHeaderItem(G::samplesPerPixelColumn)->setData(true, G::GeekRole);

    setHorizontalHeaderItem(G::isBigEndianColumn, new QStandardItem("isBigEndian")); horizontalHeaderItem(G::isBigEndianColumn)->setData(true, G::GeekRole);
    setHorizontalHeaderItem(G::ifd0OffsetColumn, new QStandardItem("ifd0Offset")); horizontalHeaderItem(G::ifd0OffsetColumn)->setData(true, G::GeekRole);
    setHorizontalHeaderItem(G::XmpSegmentOffsetColumn, new QStandardItem("XmpSegmentOffset")); horizontalHeaderItem(G::XmpSegmentOffsetColumn)->setData(true, G::GeekRole);
    setHorizontalHeaderItem(G::XmpNextSegmentOffsetColumn, new QStandardItem("XmpNextSegmentOffset")); horizontalHeaderItem(G::XmpNextSegmentOffsetColumn)->setData(true, G::GeekRole);
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
    setHorizontalHeaderItem(G::ErrColumn, new QStandardItem("Err")); horizontalHeaderItem(G::ErrColumn)->setData(true, G::GeekRole);



    sf = new SortFilter(this, filters, combineRawJpg);
    sf->setSourceModel(this);

    // root folder containing images to be added to the data model
    dir = new QDir();
    fileFilters = new QStringList;          // eligible image file types
    emptyImg.load(":/images/no_image.png");
}

void DataModel::clearDataModel()
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }    
    // clear the model
    removeRows(0, rowCount());
    setRowCount(0);
//    setColumnCount(0);
    // clear the fPath index of datamodel rows
    fPathRow.clear();
    // clear all items for filters based on data content ie file types, camera model
    filters->removeChildrenDynamicFilters();
    // reset remaining criteria without signalling filter change as no new data yet
    filters->clearAll();
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

void DataModel::remove(QString fPath)
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    // remove row from datamodel
    int row;
    for (row = 0; row < rowCount(); ++row) {
        QString rowPath = index(row, 0).data(G::PathRole).toString();
        if (rowPath == fPath) {
            removeRow(row);
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
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    for (int row = 0; row < sf->rowCount(); ++row) {
        QString searchableText = sf->index(row, G::SearchTextColumn).data().toString();
        qDebug() << __FUNCTION__ << searchableText;
        if (searchableText.contains(text.toLower())) {
            QModelIndex idx = sf->mapToSource(sf->index(row, G::SearchColumn));
            setData(idx, "true");
        }
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

- Note: build QMaps of unique field values for the filters is not done here, but on
  demand when the user selects the filter panel or a menu filter command.
*/
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    currentFolderPath = folderPath;
    filters->filtersBuilt = false;
    loadingModel = true;
    t.restart();            // timer for addFilesMaxDelay

    // clear the model
    clearDataModel();

    // do some initializing
    fileFilters->clear();
    foreach (const QString &str, metadata->supportedFormats)
            fileFilters->append("*." + str);
    dir->setNameFilters(*fileFilters);
    dir->setFilter(QDir::Files);
    dir->setPath(currentFolderPath);

    timeToQuit = false;
    imageCount = 0;
    countInterval = 1000;
    QString step = "Step 1 0f 2: Searching for eligible images.\n\n";
    QString escapeClause = "\n\nPress \"Esc\" to stop.";
    QString root;
    if (dir->isRoot()) root = "Drive ";
    else root = "Folder ";

    // load file list for the current folder
    int folderImageCount = dir->entryInfoList().size();

    // bail if no images and not including subfolders
    if (!folderImageCount && !includeSubfoldersFlag) return false;

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
            emit msg(s);
            qApp->processEvents();
        }
        if (timeToQuit) return false;
    }

    if (!includeSubfoldersFlag) {
        includeSubfolders = false;
        return addFileData();
    }

    // if include subfolders
    includeSubfolders = true;
    QDirIterator it(currentFolderPath, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        if (timeToQuit) return false;
        it.next();
//        qDebug() << __FUNCTION__ << "Scanning" << it.filePath();
        if (it.fileInfo().isDir() && it.fileName() != "." && it.fileName() != "..") {
            folderCount++;
            dir->setPath(it.filePath());
            int folderImageCount = dir->entryInfoList().size();
            // try next subfolder if no images in this folder
            if (!folderImageCount) continue;
            // add supported images in folder to image list
            for (int i = 0; i < folderImageCount; ++i) {
                fileInfoList.append(dir->entryInfoList().at(i));
                imageCount++;
                // report file progress within folder
                if (imageCount % countInterval == 0 && imageCount > 0) {
                    QString s = step +
                                QString::number(imageCount) + " found so far in " +
                                QString::number(folderCount) + " folders" +
                                escapeClause;
                    emit msg(s);
                    qApp->processEvents();
                }
            }
        }
    }
    // if images were found and added to data model
    if (imageCount) return addFileData();
    else return false;
}

bool DataModel::addFileData()
{
/*
    Load the information from the operating system contained in QFileInfo first

    • PathColumn
    • NameColumn        (core sort item)
    • TypeColumn        (core sort item)
    • SizeColumn        (core sort item)
    • CreatedColumn     (core sort item)
    • ModifiedColumn    (core sort item)
    • RefineColumn
    • PickColumn        (core sort item)
    • IngestedColumn
    • SearchColumn
    • ErrColumn
*/
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    // make sure if raw+jpg pair that raw file is first to make combining easier
    std::sort(fileInfoList.begin(), fileInfoList.end(), lessThan);

    QString step = "Step 2 0f 2: Loading eligible images.\n\n";
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
        if (timeToQuit) return false;
        // get file info
        fileInfo = fileInfoList.at(row);
//        addFileDataForRow(row, fileInfo);

        // append hash index of datamodel row for fPath for fast lookups
        QString fPath = fileInfo.filePath();
        // build hash to quickly get row from fPath (ie pixmap.cpp, imageCache...)
        fPathRow[fPath] = row;

        // string to hold aggregated text for searching
        QString search = fPath;

        setData(index(row, G::PathColumn), fPath, G::PathRole);
        QString tip = QString::number(row) + ": " + fileInfo.absoluteFilePath();
        setData(index(row, G::PathColumn), tip, Qt::ToolTipRole);
        setData(index(row, G::PathColumn), QRect(), G::IconRectRole);
        setData(index(row, G::PathColumn), false, G::CachedRole);
        setData(index(row, G::PathColumn), false, G::DupHideRawRole);

        setData(index(row, G::IconLoadedColumn), false);
        setData(index(row, G::NameColumn), fileInfo.fileName());
        setData(index(row, G::NameColumn), fileInfo.fileName(), Qt::ToolTipRole);
        setData(index(row, G::TypeColumn), fileInfo.suffix().toUpper());
        QString s = fileInfo.suffix().toUpper();
        setData(index(row, G::TypeColumn), s);
        setData(index(row, G::TypeColumn), int(Qt::AlignCenter), Qt::TextAlignmentRole);
        setData(index(row, G::SizeColumn), fileInfo.size());
        setData(index(row, G::SizeColumn), int(Qt::AlignRight | Qt::AlignVCenter), Qt::TextAlignmentRole);
        s = fileInfo.birthTime().toString("yyyy-MM-dd hh:mm:ss");
        search += s;
        setData(index(row, G::CreatedColumn), s);
        s = fileInfo.lastModified().toString("yyyy-MM-dd hh:mm:ss");
        search += s;
        setData(index(row, G::ModifiedColumn), s);
        setData(index(row, G::RefineColumn), false);
        setData(index(row, G::RefineColumn), int(Qt::AlignCenter | Qt::AlignVCenter), Qt::TextAlignmentRole);
        setData(index(row, G::PickColumn), "false");
        setData(index(row, G::PickColumn), int(Qt::AlignCenter | Qt::AlignVCenter), Qt::TextAlignmentRole);
        setData(index(row, G::IngestedColumn), "false");
        setData(index(row, G::IngestedColumn), int(Qt::AlignCenter | Qt::AlignVCenter), Qt::TextAlignmentRole);
        setData(index(row, G::MetadataLoadedColumn), "false");
        setData(index(row, G::SearchColumn), "false");
        setData(index(row, G::SearchColumn), Qt::AlignLeft, Qt::TextAlignmentRole);
        setData(index(row, G::SearchTextColumn), search);
        setData(index(row, G::SearchTextColumn), search, Qt::ToolTipRole);
        setData(index(row, G::ErrColumn), "");

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

        if (row % 1000 == 0 && t.elapsed() > addFilesMaxDelay) {
            QString s = step +
                        QString::number(row) + " of " + QString::number(rowCount()) +
                        " loaded." +
                        escapeClause;
            emit msg(s);
            qApp->processEvents();
        }

    }
    loadingModel = false;
    qDebug() << __FUNCTION__ << "model loaded for all images";
    return true;
}

bool DataModel::updateFileData(QFileInfo fileInfo)
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    QString fPath = fileInfo.filePath();
    if (!fPathRow.contains(fPath)) return false;
    int row = fPathRow[fPath];
    setData(index(row, G::SizeColumn), fileInfo.size());
    setData(index(row, G::SizeColumn), int(Qt::AlignRight | Qt::AlignVCenter), Qt::TextAlignmentRole);
    QString s = fileInfo.lastModified().toString("yyyy-MM-dd hh:mm:ss");
    setData(index(row, G::ModifiedColumn), s);
    return true;
}

ImageMetadata DataModel::imMetadata(QString fPath)
{
/*
Used by InfoString and IngestDlg
*/
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    int row = fPathRow[fPath];
    bool success = false;

    // check if metadata loaded for this row
    if (index(row, G::MetadataLoadedColumn).data().toBool()) {
        success = true;
    }
    else {
        QFileInfo fileInfo(fPath);
        if (metadata->loadImageMetadata(fileInfo, true, true, false, true, __FUNCTION__)) {
            metadata->m.row = row;
            addMetadataForItem(metadata->m);
            success = true;
        }
    }

    ImageMetadata m;
    if (!success) {
        // rgh to finish err proofing this
        return m;
    }

    m.row = row;
    m.size = index(row, G::SizeColumn).data().toInt();

    m.label = index(row, G::LabelColumn).data().toString();
    m._label = index(row, G::_LabelColumn).data().toString();
    m.rating = index(row, G::RatingColumn).data().toString();
    m._rating = index(row, G::_RatingColumn).data().toString();
    m.createdDate = index(row, G::CreatedColumn).data().toDateTime();
    m.width = index(row, G::WidthColumn).data().toInt();
    m.height = index(row, G::HeightColumn).data().toInt();
    m.dimensions = index(row, G::DimensionsColumn).data().toString();
    m.orientation = index(row, G::OrientationColumn).data().toInt();
    m.rotationDegrees = index(row, G::RotationColumn).data().toInt();
    m.apertureNum = index(row, G::ApertureColumn).data().toDouble();
    m.aperture = "f/" + QString::number(m.apertureNum, 'f', 1);
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

    m.ISONum = index(row, G::ISOColumn).data().toInt();
    m.ISO = QString::number(m.ISONum);
    m.exposureCompensationNum = index(row, G::ExposureCompensationColumn).data().toDouble();
    m.exposureCompensation = QString::number(m.exposureCompensationNum, 'f', 1);
    m.make = index(row, G::CameraMakeColumn).data().toString();
    m.model = index(row, G::CameraModelColumn).data().toString();
    m.lens = index(row, G::LensColumn).data().toString();
    m.focalLengthNum = index(row, G::FocalLengthColumn).data().toInt();
    m.focalLength = QString::number(m.focalLengthNum, 'f', 0) + "mm";
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
    m.shootingInfo = index(row, G::ShootingInfoColumn).data().toString();
    m.loadMsecPerMp = index(row, G::LoadMsecPerMpColumn).data().toInt();
    m.metadataLoaded  = index(row, G::MetadataLoadedColumn).data().toBool();

    m.offsetFull = index(row, G::OffsetFullColumn).data().toUInt();
    m.lengthFull = index(row, G::LengthFullColumn).data().toUInt();
    m.widthFull = index(row, G::WidthFullColumn).data().toInt();
    m.heightFull = index(row, G::HeightFullColumn).data().toInt();
    m.offsetThumb = index(row, G::OffsetThumbColumn).data().toUInt();
    m.lengthThumb = index(row, G::LengthThumbColumn).data().toUInt();
    m.samplesPerPixel = index(row, G::samplesPerPixelColumn).data().toInt();
    m.isBigEnd = index(row, G::isBigEndianColumn).data().toBool();
    m.ifd0Offset = index(row, G::ifd0OffsetColumn).data().toUInt();
    m.xmpSegmentOffset = index(row, G::XmpSegmentOffsetColumn).data().toUInt();
    m.xmpNextSegmentOffset = index(row, G::XmpNextSegmentOffsetColumn).data().toUInt();
    m.orientationOffset = index(row, G::OrientationOffsetColumn).data().toUInt();
    m.isXmp = index(row, G::IsXMPColumn).data().toBool();
    m.iccSegmentOffset = index(row, G::ICCSegmentOffsetColumn).data().toUInt();
    m.iccSegmentLength = index(row, G::ICCSegmentLengthColumn).data().toUInt();
    m.iccBuf = index(row, G::ICCBufColumn).data().toByteArray();
    m.iccSpace = index(row, G::ICCSegmentOffsetColumn).data().toString();
//     = index(row, G::RotationDegreesColumn), m.rotationDegrees
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
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    G::t.restart();
    timeToQuit = false;
    loadingModel = true;
    QString x = QString::number(rowCount());
    G::popUp->setProgressVisible(true);
    G::popUp->setProgressMax(rowCount());
    QString msg = "It may take a moment to load all the metadata for " + x + " files<p>"
            "This is required before any filtering or sorting of metadata can be done.<p>"
            "Press <font color=\"red\"><b>Esc</b></font> to cancel.";
    G::popUp->showPopup(msg, 0, true, 1);
    int count = 0;
    for (int row = 0; row < rowCount(); ++row) {
        qApp->processEvents();
        if (timeToQuit) break;
        // is metadata already cached
        G::popUp->setProgress(row);
        if (index(row, G::MetadataLoadedColumn).data().toBool()) continue;

        QString fPath = index(row, 0).data(G::PathRole).toString();
        QFileInfo fileInfo(fPath);
        if (metadata->loadImageMetadata(fileInfo, true, true, false, true, __FUNCTION__)) {
            metadata->m.row = row;
            addMetadataForItem(metadata->m);
            count++;
        }
        else {
            // rgh add error proofing
        }
    }
    if (!timeToQuit) G::allMetadataLoaded = true;
    loadingModel = false;
    G::popUp->setProgressVisible(false);
    G::popUp->hide();

    timeToQuit = false;

    /*
    qint64 ms = G::t.elapsed();
    qreal msperfile = static_cast<double>(ms) / count;
    qDebug() << "DataModel::addAllMetadata for" << count << "files"
             << ms << "ms" << msperfile << "ms per file;"
             << currentFolderPath;
//    */
}

bool DataModel::readMetadataForItem(int row)
{
/*
    Reads the image metadata into the datamodel for the row.
*/
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__, index(row, 0).data(G::PathRole).toString());
    #endif
    }
    QString fPath = index(row, 0).data(G::PathRole).toString();

    // load metadata
    if (!index(row, G::MetadataLoadedColumn).data().toBool()) {
        QFileInfo fileInfo(fPath);

        // only read metadata from files that we know how to
        QString ext = fileInfo.suffix().toLower();
        if (metadata->getMetadataFormats.contains(ext)) {
//                qDebug() << __FUNCTION__ << fPath << ext;
            if (metadata->loadImageMetadata(fileInfo, true, true, false, true, __FUNCTION__)) {
                metadata->m.row = row;
                addMetadataForItem(metadata->m);
            }
            else {
                qDebug() << __FUNCTION__ << "Failed to load metadata for" << fPath;
                return false;
            }
        }
        // cannot read this file type, load empty metadata
        else {
            metadata->clearMetadata();
            metadata->m.row = row;
            addMetadataForItem(metadata->m);
        }
    }
    return true;
}

bool DataModel:: addMetadataForItem(ImageMetadata m)
{
/*
This function is called after the metadata for each eligible image in the selected folder(s)
is being cached or when addAllMetadata is called prior of filtering or sorting. The metadata
is displayed in tableView, which is created in MW, and in InfoView.

If a folder is opened with combineRawJpg all the metadata for the raw file may not have been
loaded, but editable data, (such as rating, label, title, email, url) may have been edited in
the jpg file of the raw+jpg pair. If so, we do not want to overwrite this data.
*/
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    int row = m.row;
    QString search = index(row, G::SearchTextColumn).data().toString();
    if (!metadata->ratings.contains(m.rating)) m.rating = "";
    if (!metadata->labels.contains(m.label)) m.label = "";

    setData(index(row, G::SearchColumn), m.isSearch);
    setData(index(row, G::SearchColumn), Qt::AlignCenter, Qt::TextAlignmentRole);
//    if (index(row, G::LabelColumn).data().toString() != "") m.label = index(row, G::LabelColumn).data().toString();
    setData(index(row, G::LabelColumn), m.label);
    setData(index(row, G::LabelColumn), Qt::AlignCenter, Qt::TextAlignmentRole);
    search += m.label;
    setData(index(row, G::_LabelColumn), m._label);
//    if (index(row, G::RatingColumn).data().toString() != "") m.rating = index(row, G::RatingColumn).data().toString();
    setData(index(row, G::RatingColumn), m.rating);
    setData(index(row, G::RatingColumn), Qt::AlignCenter, Qt::TextAlignmentRole);
    setData(index(row, G::_RatingColumn), m._rating);
    setData(index(row, G::CreatedColumn), m.createdDate);
    setData(index(row, G::YearColumn), m.createdDate.toString("yyyy"));
    setData(index(row, G::DayColumn), m.createdDate.toString("yyyy-MM-dd"));
    search += m.createdDate.toString("yyyy-MM-dd");
    setData(index(row, G::MegaPixelsColumn), QString::number((m.width * m.height) / 1000000.0, 'f', 2));
    setData(index(row, G::MegaPixelsColumn), int(Qt::AlignRight | Qt::AlignVCenter), Qt::TextAlignmentRole);
    setData(index(row, G::LoadMsecPerMpColumn), m.loadMsecPerMp);
    setData(index(row, G::LoadMsecPerMpColumn), Qt::AlignCenter, Qt::TextAlignmentRole);
    setData(index(row, G::WidthColumn), QString::number(m.width));
    setData(index(row, G::WidthColumn), Qt::AlignCenter, Qt::TextAlignmentRole);
    setData(index(row, G::HeightColumn), QString::number(m.height));
    setData(index(row, G::HeightColumn), Qt::AlignCenter, Qt::TextAlignmentRole);
    setData(index(row, G::DimensionsColumn), QString::number(m.width) + "x" + QString::number(m.height));
    setData(index(row, G::DimensionsColumn), Qt::AlignCenter, Qt::TextAlignmentRole);
    setData(index(row, G::RotationColumn), 0);
    setData(index(row, G::RotationColumn), Qt::AlignCenter, Qt::TextAlignmentRole);
    setData(index(row, G::ApertureColumn), m.apertureNum);
    setData(index(row, G::ApertureColumn), Qt::AlignCenter, Qt::TextAlignmentRole);
    setData(index(row, G::ShutterspeedColumn), m.exposureTimeNum);
    setData(index(row, G::ShutterspeedColumn), int(Qt::AlignRight | Qt::AlignVCenter), Qt::TextAlignmentRole);
    setData(index(row, G::ISOColumn), m.ISONum);
    setData(index(row, G::ISOColumn), Qt::AlignCenter, Qt::TextAlignmentRole);
    setData(index(row, G::ExposureCompensationColumn), m.exposureCompensationNum);
    setData(index(row, G::ExposureCompensationColumn), Qt::AlignCenter, Qt::TextAlignmentRole);
    setData(index(row, G::CameraMakeColumn), m.make);
    setData(index(row, G::CameraMakeColumn), m.make, Qt::ToolTipRole);
    search += m.make;
    setData(index(row, G::CameraModelColumn), m.model);
    setData(index(row, G::CameraModelColumn), m.model, Qt::ToolTipRole);
    search += m.model;
    setData(index(row, G::LensColumn), m.lens);
    setData(index(row, G::LensColumn), m.lens, Qt::ToolTipRole);
    search += m.lens;
    setData(index(row, G::FocalLengthColumn), m.focalLengthNum);
    setData(index(row, G::FocalLengthColumn), int(Qt::AlignRight | Qt::AlignVCenter), Qt::TextAlignmentRole);
//    if (index(row, G::TitleColumn).data().toString() != "") m.title = index(row, G::TitleColumn).data().toString();
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
    setData(index(row, G::WidthFullColumn), m.widthFull);
    setData(index(row, G::HeightFullColumn), m.heightFull);
    setData(index(row, G::OffsetThumbColumn), m.offsetThumb);
    setData(index(row, G::LengthThumbColumn), m.lengthThumb);
//    setData(index(row, G::OffsetSmallColumn), m.offsetSmall);
//    setData(index(row, G::LengthSmallColumn), m.lengthSmall);

//    setData(index(row, G::bitsPerSampleColumn), m.bitsPerSample);
//    setData(index(row, G::photoInterpColumn), m.photoInterp);
    setData(index(row, G::samplesPerPixelColumn), m.samplesPerPixel); // reqd for err trapping
//    setData(index(row, G::compressionColumn), m.compression);
//    setData(index(row, G::stripByteCountsColumn), m.stripByteCounts);
//    setData(index(row, G::planarConfigurationFullColumn), m.stripByteCounts);

    setData(index(row, G::isBigEndianColumn), m.isBigEnd);
    setData(index(row, G::ifd0OffsetColumn), m.ifd0Offset);
    setData(index(row, G::XmpSegmentOffsetColumn), m.xmpSegmentOffset);
    setData(index(row, G::XmpNextSegmentOffsetColumn), m.xmpNextSegmentOffset);
    setData(index(row, G::IsXMPColumn), m.isXmp);
    setData(index(row, G::ICCSegmentOffsetColumn), m.iccSegmentOffset);
    setData(index(row, G::ICCSegmentLengthColumn), m.iccSegmentLength);
    setData(index(row, G::ICCBufColumn), m.iccBuf);
    setData(index(row, G::ICCSpaceColumn), m.iccSpace);
    setData(index(row, G::OrientationOffsetColumn), m.orientationOffset);
    setData(index(row, G::OrientationColumn), m.orientation);
    setData(index(row, G::RotationDegreesColumn), m.rotationDegrees);
    setData(index(row, G::ShootingInfoColumn), m.shootingInfo);
    setData(index(row, G::ShootingInfoColumn), m.shootingInfo, Qt::ToolTipRole);
    search += m.shootingInfo;
    setData(index(row, G::MetadataLoadedColumn), m.metadataLoaded);
    setData(index(row, G::SearchTextColumn), search.toLower());
    setData(index(row, G::SearchTextColumn), search.toLower(), Qt::ToolTipRole);
    setData(index(row, G::ErrColumn), m.err);
//    setData(index(row, G::ErrColumn), m.err, Qt::ToolTipRole);

    // req'd for 1st image, probably loaded before metadata cached
    if (row == 0) emit updateClassification();

    return true;
}

bool DataModel::hasFolderChanged()
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
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
            qDebug() << __FUNCTION__ << fileInfoList2.at(i).fileName()
                     << "modified at" << t2;
        }
    }
    return hasChanged;
}

void DataModel::searchStringChange(QString searchString)
{
/*
When the search string in filters is edited a signal is emitted to run this function.  Where
there is a match G::SearchColumn is set to true, otherwise false.  Udate the filtered and
unfiltered counts.
*/
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    qDebug() << __FUNCTION__ << searchString;
    // update datamodel search string match
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
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    filters->types->takeChildren();
    QMap<QVariant, QString> typesMap;
    int rows = sf->rowCount();
    for(int row = 0; row < rows; row++) {
        QString type = sf->index(row, G::TypeColumn).data().toString();
        if (!typesMap.contains(type)) {
            typesMap[type] = type;
        }
    }
    filters->addCategoryFromData(typesMap, filters->types);
    qApp->processEvents();
}

QModelIndex DataModel::proxyIndexFromPath(QString fPath)
{
/*
The hash table fPathRow {path, row} if build when the datamodel is loaded to provide a
quick lookup to get the datamodel row from an image path.
*/
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    int row = fPathRow[fPath];
    QModelIndex idx = sf->mapFromSource(index(row, 0));
    if (idx.isValid()) return idx;
    return index(-1, -1);       // invalid index

    /* deprecated code
    QModelIndexList idxList = sf->match(sf->index(0, 0), G::PathRole, fPath);
    if (idxList.size() > 0 && idxList[0].isValid()) {
        return idxList[0];
    }
    return index(-1, -1);       // invalid index
    */
}

int DataModel::proxyRowFromModelRow(int dmRow) {
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    return sf->mapFromSource(index(dmRow, 0)).row();
}

void DataModel::error(int sfRow, const QString &s, const QString src)
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
//    int dmRow = sf->mapToSource(sf->index(sfRow, 0)).row();
    QStringList err = sf->index(sfRow, G::ErrColumn).data().toStringList();
    err << QString::number(err.size() + 1) + ". " + src + ": " + s;
    sf->setData(sf->index(sfRow, G::ErrColumn), err);
    QString tip = "";
    for (int i = 0; i < err.size(); ++i) {
        tip += err.at(i) + "\n";
    }
    sf->setData(sf->index(sfRow, G::ErrColumn), tip, Qt::ToolTipRole);
}

void DataModel::errorList(int sfRow, const QStringList &sl, const QString src)
{
    {
#ifdef ISDEBUG
        G::track(__FUNCTION__);
#endif
    }
    // get current error list for datamodel row
    QStringList err = sf->index(sfRow, G::ErrColumn).data().toStringList();
    // append new error items in sl
    for (int i = 0; i < sl.size(); ++i) {
        err << QString::number(err.size() + i + 1) + ". " + src + ": " + sl.at(i);
    }
    // update datamodel
    sf->setData(sf->index(sfRow, G::ErrColumn), err);
    // rebuild the tooltip text
    QString tip = "";
    for (int i = 0; i < err.size(); ++i) {
        tip += sl.at(i) + "\n";
    }
    sf->setData(sf->index(sfRow, G::ErrColumn), tip, Qt::ToolTipRole);
}

void DataModel::clearPicks()
{
/*
reset all the picks to false.
*/
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    for(int row = 0; row < sf->rowCount(); row++) {
        setData(index(row, G::PickColumn), "false");
    }
}

QString DataModel::diagnosticsErrors()
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    QString reportString;
    QTextStream rpt;
    rpt.setString(&reportString);
    rpt << Utilities::centeredRptHdr('=', "DataModel Error Listing");
    rpt << "\n";
    QStringList err;
    for(int row = 0; row < sf->rowCount(); row++) {
        err.clear();
        err = sf->index(row, G::ErrColumn).data().toStringList();
        if (err.size() == 0) continue;
        QString fPath = sf->index(row, G::PathColumn).data(G::PathRole).toString();
        rpt << fPath + "\n";
        for (int i = 0; i < err.size(); ++i) {
            rpt << "    " << err.at(i) + "\n";
        }
    }
    rpt << "\n\n" ;
    return reportString;
}

QString DataModel::diagnostics()
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    QString reportString;
    QTextStream rpt;
    rpt.setString(&reportString);
    rpt << Utilities::centeredRptHdr('=', "DataModel Diagnostics");
    rpt << "\n";
    rpt << "\n" << G::sj("currentFolderPath", 27) << G::s(currentFolderPath);
    rpt << "\n" << G::sj("currentFilePath", 27) << G::s(currentFilePath);
    rpt << "\n" << G::sj("currentRow", 27) << G::s(currentRow);
    rpt << "\n" << G::sj("hasDupRawJpg", 27) << G::s(hasDupRawJpg);
    rpt << "\n" << G::sj("filtersBuilt", 27) << G::s(filters->filtersBuilt);
    rpt << "\n" << G::sj("timeToQuit", 27) << G::s(timeToQuit);
    rpt << "\n" << G::sj("imageCount", 27) << G::s(imageCount);
    rpt << "\n" << G::sj("countInterval", 27) << G::s(countInterval);
    for(int row = 0; row < rowCount(); row++) {
        getDiagnosticsForRow(row, rpt);
    }
    rpt << "\n\n" ;
    return reportString;
}

QString DataModel::diagnosticsForCurrentRow()
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
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
    getDiagnosticsForRow(currentRow, rpt);
    rpt << "\n\n" ;
    return reportString;
}

void DataModel::getDiagnosticsForRow(int row, QTextStream& rpt)
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    QString s = "";
    rpt << "\n"   << G::sj("DataModel row", 27) << G::s(row);
    rpt << "\n  " << G::sj("FileName", 25) << G::s(index(row, G::NameColumn).data());
    rpt << "\n  " << G::sj("FilePath", 25) << G::s(index(row, 0).data(G::PathRole));
    rpt << "\n  " << G::sj("dupHideRaw", 25) << G::s(index(row, 0).data(G::DupHideRawRole));
    rpt << "\n  " << G::sj("dupRawRow", 25) << G::s(qvariant_cast<QModelIndex>(index(row, 0).data(G::DupOtherIdxRole)).row());
    rpt << "\n  " << G::sj("dupIsJpg", 25) << G::s(index(row, 0).data(G::DupIsJpgRole));
    rpt << "\n  " << G::sj("dupRawType", 25) << G::s(index(row, 0).data(G::DupRawTypeRole));
    rpt << "\n  " << G::sj("type", 25) << G::s(index(row, G::TypeColumn).data());
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
    rpt << "\n  " << G::sj("rotation", 25) << G::s(index(row, G::RotationColumn).data());
    rpt << "\n  " << G::sj("apertureNum", 25) << G::s(index(row, G::ApertureColumn).data());
    rpt << "\n  " << G::sj("exposureTimeNum", 25) << G::s(index(row, G::ShutterspeedColumn).data());
    rpt << "\n  " << G::sj("iso", 25) << G::s(index(row, G::ISOColumn).data());
    rpt << "\n  " << G::sj("exposureCompensationNum", 25) << G::s(index(row, G::ExposureCompensationColumn).data());
    rpt << "\n  " << G::sj("make", 25) << G::s(index(row, G::CameraMakeColumn).data());
    rpt << "\n  " << G::sj("model", 25) << G::s(index(row, G::CameraModelColumn).data());
    rpt << "\n  " << G::sj("lens", 25) << G::s(index(row, G::LensColumn).data());
    rpt << "\n  " << G::sj("focalLengthNum", 25) << G::s(index(row, G::FocalLengthColumn).data());
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
    rpt << "\n  " << G::sj("widthFull", 25) << G::s(index(row, G::WidthFullColumn).data());
    rpt << "\n  " << G::sj("heightFull", 25) << G::s(index(row, G::HeightFullColumn).data());
    rpt << "\n  " << G::sj("offsetThumb", 25) << G::s(index(row, G::OffsetThumbColumn).data());
    rpt << "\n  " << G::sj("lengthThumb", 25) << G::s(index(row, G::LengthThumbColumn).data());
    rpt << "\n  " << G::sj("isBigEndian", 25) << G::s(index(row, G::isBigEndianColumn).data());
    rpt << "\n  " << G::sj("ifd0Offset", 25) << G::s(index(row, G::ifd0OffsetColumn).data());
    rpt << "\n  " << G::sj("xmpSegmentOffset", 25) << G::s(index(row, G::XmpSegmentOffsetColumn).data());
    rpt << "\n  " << G::sj("xmpNextSegmentOffset", 25) << G::s(index(row, G::XmpNextSegmentOffsetColumn).data());

    rpt << "\n  " << G::sj("isXmp", 25) << G::s(index(row, G::IsXMPColumn).data());
    rpt << "\n  " << G::sj("orientationOffset", 25) << G::s(index(row, G::OrientationOffsetColumn).data());
    rpt << "\n  " << G::sj("orientation", 25) << G::s(index(row, G::OrientationColumn).data());
    rpt << "\n  " << G::sj("rotationDegrees", 25) << G::s(index(row, G::RotationDegreesColumn).data());
    rpt << "\n  " << G::sj("shootingInfo", 25) << G::s(index(row, G::ShootingInfoColumn).data());
    rpt << "\n  " << G::sj("loadMsecPerMp", 25) << G::s(index(row, G::LoadMsecPerMpColumn).data());
    rpt << "\n  " << G::sj("searchText", 25) << G::s(index(row, G::SearchTextColumn).data());
//    QStringList sl = index(row, G::ErrColumn).data().toStringList();
//    rpt << "\n  " << G::sj("err", 25) << sl.at(0);
//    QString whitespace = "";
//    for (int i = 1; i < sl.size(); ++i) {
//        rpt << "\n  " << whitespace.leftJustified(26, ' ') << sl.at(i);
//    }
}

// --------------------------------------------------------------------------------------------
// SortFilter Class used to filter by row
// --------------------------------------------------------------------------------------------

SortFilter::SortFilter(QObject *parent, Filters *filters, bool &combineRawJpg) :
    QSortFilterProxyModel(parent),
    combineRawJpg(combineRawJpg)
{
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

    // cycle through the filters and identify matches
    QTreeWidgetItemIterator filter(filters);
    while (*filter) {
        if ((*filter)->parent()) {            
            /* There is a parent therefore not a top level item so this is one of the items to
            match ie rating = one. If the item has been checked then compare the checked
            filter item to the data in the dataModelColumn for the row. If it matches then set
            isMatch = true. If it does not match them isMatch is still false but the row could
            still be accepted if another item in the same category does match.
            */
            if ((*filter)->checkState(0) != Qt::Unchecked) {
                if ((*filter) == filters->searchTrue &&
                (*filter)->text(0) == filters->enterSearchString) {
                    isMatch = true;
                }
                else {
                    isCategoryUnchecked = false;
                    QModelIndex idx = sourceModel()->index(sourceRow, dataModelColumn, sourceParent);
                    QVariant dataValue = idx.data(Qt::EditRole);
                    QVariant filterValue = (*filter)->data(1, Qt::EditRole);
                    /*
                    QString itemName = (*filter)->text(0);      // for debugging
                    qDebug() << G::t.restart() << "\t" << itemCategory << itemName
                             << "Comparing" << dataValue << filterValue
                             << (dataValue == filterValue);
//                    */
                    if (dataValue == filterValue) isMatch = true;
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
//            itemCategory = (*filter)->text(0);      // for debugging
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
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    invalidateFilter();
}
