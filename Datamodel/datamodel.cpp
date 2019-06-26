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
    ● OffsetThumbJPG:   from metadata       EditRole
    ● LengthThumbJPG:   from metadata       EditRole
    ● OffsetSmallJPG:   from metadata       EditRole
    ● LengthSmallJPG:   from metadata       EditRole
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

    // edit a isCached role in model based on file path
    QModelIndexList idxList = dm->sf->match(dm->sf->index(0, 0), G::PathRole, fPath);
    QModelIndex idx = idxList[0];
    dm->sf->setData(idx, iscached, G::CachedRole);

    // file path for current index (primary selection)
    fPath = thumbView->currentIndex().data(G::PathRole).toString();

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
    setHorizontalHeaderItem(G::PathColumn, new QStandardItem(QString("Icon")));
    setHorizontalHeaderItem(G::NameColumn, new QStandardItem(QString("File Name")));
    setHorizontalHeaderItem(G::RefineColumn, new QStandardItem("Refine"));
    setHorizontalHeaderItem(G::PickColumn, new QStandardItem("Pick"));
    setHorizontalHeaderItem(G::IngestedColumn, new QStandardItem("Ingested"));
    setHorizontalHeaderItem(G::LabelColumn, new QStandardItem("Colour"));
    setHorizontalHeaderItem(G::RatingColumn, new QStandardItem("Rating"));
    setHorizontalHeaderItem(G::TypeColumn, new QStandardItem("Type"));
    setHorizontalHeaderItem(G::SizeColumn, new QStandardItem("Size"));
    setHorizontalHeaderItem(G::WidthColumn, new QStandardItem("Width"));
    setHorizontalHeaderItem(G::HeightColumn, new QStandardItem("Height"));
    setHorizontalHeaderItem(G::CreatedColumn, new QStandardItem("Created"));
    setHorizontalHeaderItem(G::ModifiedColumn, new QStandardItem("Last Modified"));
    setHorizontalHeaderItem(G::YearColumn, new QStandardItem("Year"));
    setHorizontalHeaderItem(G::DayColumn, new QStandardItem("Day"));
    setHorizontalHeaderItem(G::CreatorColumn, new QStandardItem("Creator"));
    setHorizontalHeaderItem(G::MegaPixelsColumn, new QStandardItem("MPix"));
    setHorizontalHeaderItem(G::DimensionsColumn, new QStandardItem("Dimensions"));
    setHorizontalHeaderItem(G::RotationColumn, new QStandardItem("Rot"));
    setHorizontalHeaderItem(G::ApertureColumn, new QStandardItem("Aperture"));
    setHorizontalHeaderItem(G::ShutterspeedColumn, new QStandardItem("Shutter"));
    setHorizontalHeaderItem(G::ISOColumn, new QStandardItem("ISO"));
    setHorizontalHeaderItem(G::CameraMakeColumn, new QStandardItem("Make"));
    setHorizontalHeaderItem(G::CameraModelColumn, new QStandardItem("Model"));
    setHorizontalHeaderItem(G::LensColumn, new QStandardItem("Lens"));
    setHorizontalHeaderItem(G::FocalLengthColumn, new QStandardItem("Focal length"));
    setHorizontalHeaderItem(G::TitleColumn, new QStandardItem("Title"));
    setHorizontalHeaderItem(G::CopyrightColumn, new QStandardItem("Copyright"));
    setHorizontalHeaderItem(G::EmailColumn, new QStandardItem("Email"));
    setHorizontalHeaderItem(G::UrlColumn, new QStandardItem("Url"));
    setHorizontalHeaderItem(G::_RatingColumn, new QStandardItem("_Rating"));
    setHorizontalHeaderItem(G::_LabelColumn, new QStandardItem("_Label"));
    setHorizontalHeaderItem(G::_CreatorColumn, new QStandardItem("_Creator"));
    setHorizontalHeaderItem(G::_TitleColumn, new QStandardItem("_Title"));
    setHorizontalHeaderItem(G::_CopyrightColumn, new QStandardItem("_Copyright"));
    setHorizontalHeaderItem(G::_EmailColumn, new QStandardItem("_Email"));
    setHorizontalHeaderItem(G::_UrlColumn, new QStandardItem("_Url"));

    setHorizontalHeaderItem(G::OffsetFullJPGColumn, new QStandardItem("OffsetFullJPG"));
    setHorizontalHeaderItem(G::LengthFullJPGColumn, new QStandardItem("LengthFullJPG"));
    setHorizontalHeaderItem(G::OffsetThumbJPGColumn, new QStandardItem("OffsetThumbJPG"));
    setHorizontalHeaderItem(G::LengthThumbJPGColumn, new QStandardItem("LengthThumbJPF"));
    setHorizontalHeaderItem(G::OffsetSmallJPGColumn, new QStandardItem("OffsetSmallJPG"));
    setHorizontalHeaderItem(G::LengthSmallJPGColumn, new QStandardItem("LengthSmallJPG"));

    setHorizontalHeaderItem(G::XmpSegmentOffsetColumn, new QStandardItem("XmpSegmentOffset"));
    setHorizontalHeaderItem(G::XmpNextSegmentOffsetColumn, new QStandardItem("XmpNextSegmentOffset"));
    setHorizontalHeaderItem(G::IsXMPColumn, new QStandardItem("IsXMP"));
    setHorizontalHeaderItem(G::OrientationOffsetColumn, new QStandardItem("OrientationOffset"));
    setHorizontalHeaderItem(G::OrientationColumn, new QStandardItem("Orientation"));
    setHorizontalHeaderItem(G::RotationDegreesColumn, new QStandardItem("RotationDegrees"));
    setHorizontalHeaderItem(G::ErrColumn, new QStandardItem("Err"));
    setHorizontalHeaderItem(G::ShootingInfoColumn, new QStandardItem("ShootingInfo"));

    sf = new SortFilter(this, filters, combineRawJpg);
    sf->setSourceModel(this);

    // root folder containing images to be added to the data model
    dir = new QDir();
    fileFilters = new QStringList;          // eligible image file types
    emptyImg.load(":/images/no_image.png");
}

void DataModel::clear()
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    // clear the model
    removeRows(0, rowCount());
    // clear the fPath index of datamodel rows
    fPathRow.clear();
    // clear all items for filters based on data content ie file types, camera model
    filters->removeChildrenDynamicFilters();
    // reset remaining criteria without signalling filter change as no new data yet
    filters->clearAll();
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
    if(i1.baseName() == i2.baseName()) {
        if (i1.suffix().toLower() == "jpg") s1.replace(".jpg", ".zzz");
        if (i2.suffix().toLower() == "jpg") s2.replace(".jpg", ".zzz");
    }
    return s1 < s2;
}

bool DataModel::load(QString &folderPath, bool includeSubfolders)
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
    filtersBuilt = false;

    //  clear the model
    clear();

    // do some initializing
    fileFilters->clear();
    foreach (const QString &str, metadata->supportedFormats)
            fileFilters->append("*." + str);
    dir->setNameFilters(*fileFilters);
    dir->setFilter(QDir::Files);
    dir->setPath(currentFolderPath);

    fileInfoList.clear();

    timeToQuit = false;
    imageCount = 0;
    countInterval = 1000;
    QString escapeClause = "Press \"Esc\" to stop.\n\n";
    QString scanning = "Searching for eligible images.  ";
    QString root;
    if (dir->isRoot()) root = "Drive ";
    else root = "Folder ";

    // load file list for the current folder
    int folderImageCount = dir->entryInfoList().size();

    // bail if no images and not including subfolders
    if (!folderImageCount && !includeSubfolders) return false;

    int folderCount = 1;
    // add supported images in folder to image list
    for (int i = 0; i < folderImageCount; ++i) {
        fileInfoList.append(dir->entryInfoList().at(i));
        imageCount++;
        if (imageCount % countInterval == 0 && imageCount > 0) {
            QString s = escapeClause + scanning +
                        QString::number(imageCount) + " found so far in " +
                        QString::number(folderCount) + " folders";
            emit msg(s);
            qApp->processEvents();
        }
        if (timeToQuit) return false;
    }
    if (!includeSubfolders) return addFileData();

    // if include subfolders
    QDirIterator it(currentFolderPath, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        if (timeToQuit) return false;
        it.next();
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
                    QString s = escapeClause + scanning +
                            QString::number(imageCount) + " found so far in " +
                            QString::number(folderCount) + " folders";
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
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    QElapsedTimer t;
    t.start();

    progressBar->clearProgress();

    std::sort(fileInfoList.begin(), fileInfoList.end(), lessThan);

    static QStandardItem *item;
    static int fileIndex;
    static QPixmap emptyPixMap;

    // collect all unique instances of file type for filtration (use QMap to maintain order)
    QMap<QVariant, QString> typesMap;

    imageFilePathList.clear();

    // rgh not working
    emptyPixMap = QPixmap::fromImage(emptyImg).scaled(G::maxIconSize, G::maxIconSize);

    for (fileIndex = 0; fileIndex < fileInfoList.count(); ++fileIndex) {

        if (timeToQuit) return false;

        // get file info
        fileInfo = fileInfoList.at(fileIndex);

        // append hash index of datamodel row for fPath for fast lookups
        QString fPath = fileInfo.filePath();

        // build hash to quickly get row from fPath (ie pixmap.cpp, imageCache...)
        fPathRow[fPath] = fileIndex;

        /* add icon as first column in new row

        This can be done two ways:

        // this way is slow
        item = new QStandardItem();
        appendRow(item);
        int row = item->index().row();
        setData(index(row, G::PathColumn), fileInfo.filePath(), G::PathRole);

        // this way is faster
        item = new QStandardItem();
        item->setData("", Qt::DisplayRole);
        appendRow(item);
*/

        item = new QStandardItem();
        item->setData("", Qt::DisplayRole);             // column 0 just displays icon
        item->setData(fPath, G::PathRole);
//        item->setData(fileInfo.filePath(), G::PathRole);
        QString tip = QString::number(fileIndex) + ": " + fileInfo.absoluteFilePath();
        item->setData(tip, Qt::ToolTipRole);
        item->setData(QRect(), G::ThumbRectRole);     // define later when read
        item->setData(false, G::CachedRole);
        item->setData(false, G::DupHideRawRole);      // default - overwrite if matching jpg
        item->setData(Qt::AlignCenter, Qt::TextAlignmentRole);
        appendRow(item);

        // add columns that do not require metadata read from image files
        int row = item->index().row();

        setData(index(row, G::NameColumn), fileInfo.fileName());
        setData(index(row, G::TypeColumn), fileInfo.suffix().toUpper());
        QString s = fileInfo.suffix().toUpper();
        // build list for filters->addCategoryFromData
        typesMap[s] = s;
        setData(index(row, G::TypeColumn), s);
        setData(index(row, G::TypeColumn), int(Qt::AlignCenter), Qt::TextAlignmentRole);
        setData(index(row, G::SizeColumn), fileInfo.size());
        setData(index(row, G::SizeColumn), int(Qt::AlignRight | Qt::AlignVCenter), Qt::TextAlignmentRole);
        setData(index(row, G::ModifiedColumn), fileInfo.lastModified().toString("yyyy-MM-dd hh:mm:ss"));
        setData(index(row, G::RefineColumn), false);
        setData(index(row, G::RefineColumn), int(Qt::AlignCenter | Qt::AlignVCenter), Qt::TextAlignmentRole);
        setData(index(row, G::PickColumn), "false");
        setData(index(row, G::PickColumn), int(Qt::AlignCenter | Qt::AlignVCenter), Qt::TextAlignmentRole);
        setData(index(row, G::IngestedColumn), "false");
        setData(index(row, G::IngestedColumn), int(Qt::AlignCenter | Qt::AlignVCenter), Qt::TextAlignmentRole);

        /* Save info for duplicated raw and jpg files, which generally are the result of
        setting raw+jpg in the camera. The datamodel is sorted by file path, except raw
        files with the same path precede jpg files with duplicate names. Two roles track
        duplicates: G::DupHideRawRole flags jpg files with duplicate raws and
        G::DupRawIdxRole points to the duplicate raw file from the jpg data row. For
        example:

        Row = 0 "G:/DCIM/100OLYMP/P4020001.ORF"  DupHideRawRole = true 	 DupRawIdxRole = (Invalid)
        Row = 1 "G:/DCIM/100OLYMP/P4020001.JPG"  DupHideRawRole = false  DupRawIdxRole = QModelIndex(0,0)) DupRawTypeRole = "ORF"
        Row = 2 "G:/DCIM/100OLYMP/P4020002.ORF"  DupHideRawRole = true 	 DupRawIdxRole = (Invalid)
        Row = 3 "G:/DCIM/100OLYMP/P4020002.JPG"  DupHideRawRole = false  DupRawIdxRole = QModelIndex(2,0)  DupRawTypeRole = "ORF"
        */

        if (fileIndex > 0) {
            QString s1 = fileInfoList.at(fileIndex - 1).baseName();
            QString s2 = fileInfoList.at(fileIndex).baseName();
            if (s1 == s2) {
                if (fileInfoList.at(fileIndex).suffix().toLower() == "jpg") {
                    QModelIndex prevIdx = index(row - 1, 0);
                    // hide raw version
                    setData(prevIdx, true, G::DupHideRawRole);
                    // point to raw version
                    setData(index(row, 0), prevIdx, G::DupRawIdxRole);
                    // set flag to show combined JPG file for filtering when ingesting
                    setData(index(row, 0), true, G::DupIsJpgRole);
                    // build combined suffix to show in type column
                    QString prevType = fileInfoList.at(fileIndex - 1).suffix().toUpper();
                    setData(index(row, 0), prevType, G::DupRawTypeRole);
                    if (combineRawJpg)
                        setData(index(row, G::TypeColumn), s + "+" + prevType);
                    else
                        setData(index(row, G::TypeColumn), "JPG");
                }
            }
        }
        progressBar->updateProgress(row, row + 1, fileInfoList.count(),
                                    G::progressAddFileInfoColor,
                                    "Reading the basic file information for each image");
        if(row % 100 == 0) qApp->processEvents();

        if(row % countInterval == 0 || row == 0) {
            s = "Adding " + QString::number(row) + " of " +
                QString::number(fileInfoList.count()) + " images";
            emit msg(s);
//            qApp->processEvents();        // causes thumb dock to flicker
        }

        /* the rest of the data model columns are added when the metadata
        has been loaded on demand.  see addMetadataItem and mdCache    */
    }
    filters->addCategoryFromData(typesMap, filters->types);
    return true;
}

ImageMetadata DataModel::getMetadata(QString fPath)
{
    int row = fPathRow[fPath];

    // check if metadata loaded for this row
    if (index(row, G::CreatedColumn).data().isNull()) {
        QFileInfo fileInfo(fPath);
        if (metadata->loadImageMetadata(fileInfo, true, true, false, true, __FUNCTION__)) {
            metadata->imageMetadata.row = row;
            addMetadataItem(metadata->imageMetadata);
        }
    }

    ImageMetadata m;

    m.row = row;
    m.label = index(row, G::LabelColumn).data().toString();
    m._label = index(row, G::_LabelColumn).data().toString();
    m.rating = index(row, G::RatingColumn).data().toString();
    m._rating = index(row, G::_RatingColumn).data().toString();
    m.createdDate = index(row, G::CreatedColumn).data().toDateTime();
    m.width = index(row, G::WidthColumn).data().toUInt();
    m.height= index(row, G::HeightColumn).data().toUInt();
    m.dimensions = index(row, G::DimensionsColumn).data().toString();
    m.orientation = index(row, G::OrientationColumn).data().toInt();
    m.rotationDegrees = index(row, G::RotationColumn).data().toInt();
    m.apertureNum = index(row, G::ApertureColumn).data().toFloat();
    m.aperture = "f/" + QString::number(m.apertureNum, 'f', 1);
    m.exposureTimeNum = index(row, G::ShutterspeedColumn).data().toFloat();

    if (m.exposureTimeNum < 1.0) {
        uint t = qRound(1 / m.exposureTimeNum);
        m.exposureTime = "1/" + QString::number(t);
    } else {
        m.exposureTime = QString::number(m.exposureTimeNum);
    }
    m.exposureTime += " sec";

    m.ISONum = index(row, G::ISOColumn).data().toFloat();
    m.ISO = QString::number(m.ISONum);
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

    m.offsetFullJPG = index(row, G::OffsetFullJPGColumn).data().toUInt();
    m.lengthFullJPG = index(row, G::LengthFullJPGColumn).data().toUInt();
    m.offsetThumbJPG = index(row, G::OffsetThumbJPGColumn).data().toUInt();
    m.lengthThumbJPG = index(row, G::LengthThumbJPGColumn).data().toUInt();
    m.offsetSmallJPG = index(row, G::OffsetSmallJPGColumn).data().toUInt();
    m.lengthSmallJPG = index(row, G::LengthSmallJPGColumn).data().toUInt();
    m.xmpSegmentOffset = index(row, G::XmpSegmentOffsetColumn).data().toUInt();
    m.xmpNextSegmentOffset = index(row, G::XmpNextSegmentOffsetColumn).data().toUInt();
    m.orientationOffset = index(row, G::OrientationOffsetColumn).data().toUInt();
    m.isXmp = index(row, G::IsXMPColumn).data().toBool();
//     = index(row, G::RotationDegreesColumn), m.rotationDegrees
    return m;
}

void DataModel::addAllMetadata(bool isShowCacheStatus)
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
    isShowCacheStatus = true;
    int count = 0;
    for (int row = 0; row < rowCount(); ++row) {
        // is metadata already cached
        if (!index(row, G::CreatedColumn).data().isNull()) continue;

        QString fPath = index(row, 0).data(G::PathRole).toString();
        QFileInfo fileInfo(fPath);
        if (metadata->loadImageMetadata(fileInfo, true, true, false, true, __FUNCTION__)) {
            metadata->imageMetadata.row = row;
            addMetadataItem(metadata->imageMetadata, isShowCacheStatus);
            count++;
        }
    }
    G::allMetadataLoaded = true;
    qint64 ms = G::t.elapsed();
    qreal msperfile = (float)ms / count;
    qDebug() << "DataModel::addAllMetadata for" << count << "files"
             << ms << "ms" << msperfile << "ms per file;"
             << currentFolderPath;
}

bool DataModel::addMetadataItem(ImageMetadata m, bool isShowCacheStatus)
{
    /*
    This function is called after the metadata for each eligible image in the selected
    folder(s) is being cached or when addAllMetadata is called prior of filtering or
    sorting. The metadata is displayed in tableView, which is created in MW, and in
    InfoView.
    */
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    static int lastProgressRow = 0;
    int row = m.row;

    setData(index(row, G::LabelColumn), m.label);
    setData(index(row, G::LabelColumn), Qt::AlignCenter, Qt::TextAlignmentRole);
    setData(index(row, G::_LabelColumn), m._label);
    setData(index(row, G::RatingColumn), m.rating);
    setData(index(row, G::RatingColumn), Qt::AlignCenter, Qt::TextAlignmentRole);
    setData(index(row, G::_RatingColumn), m._rating);
    setData(index(row, G::CreatedColumn), m.createdDate);
    setData(index(row, G::YearColumn), m.createdDate.toString("yyyy"));
    setData(index(row, G::DayColumn), m.createdDate.toString("yyyy-MM-dd"));
    setData(index(row, G::MegaPixelsColumn), QString::number((m.width * m.height) / 1000000.0, 'f', 2));
    setData(index(row, G::MegaPixelsColumn), int(Qt::AlignRight | Qt::AlignVCenter), Qt::TextAlignmentRole);
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
    setData(index(row, G::CameraMakeColumn), m.make);
    setData(index(row, G::CameraModelColumn), m.model);
    setData(index(row, G::LensColumn), m.lens);
    setData(index(row, G::FocalLengthColumn), m.focalLengthNum);
    setData(index(row, G::FocalLengthColumn), int(Qt::AlignRight | Qt::AlignVCenter), Qt::TextAlignmentRole);
    setData(index(row, G::TitleColumn), m.title);
    setData(index(row, G::_TitleColumn), m._title);
    setData(index(row, G::CreatorColumn), m.creator);
    setData(index(row, G::_CreatorColumn), m._creator);
    setData(index(row, G::CopyrightColumn), m.copyright);
    setData(index(row, G::_CopyrightColumn), m._copyright);
    setData(index(row, G::EmailColumn), m.email);
    setData(index(row, G::_EmailColumn), m._email);
    setData(index(row, G::UrlColumn), m.url);
    setData(index(row, G::_UrlColumn), m._url);
    setData(index(row, G::OffsetFullJPGColumn), m.offsetFullJPG);
    setData(index(row, G::LengthFullJPGColumn), m.lengthFullJPG);
    setData(index(row, G::OffsetThumbJPGColumn), m.offsetThumbJPG);
    setData(index(row, G::LengthThumbJPGColumn), m.lengthThumbJPG);
    setData(index(row, G::OffsetSmallJPGColumn), m.offsetSmallJPG);
    setData(index(row, G::LengthSmallJPGColumn), m.lengthSmallJPG);
    setData(index(row, G::XmpSegmentOffsetColumn), m.xmpSegmentOffset);
    setData(index(row, G::XmpNextSegmentOffsetColumn), m.xmpNextSegmentOffset);
    setData(index(row, G::IsXMPColumn), m.isXmp);
    setData(index(row, G::OrientationOffsetColumn), m.orientationOffset);
    setData(index(row, G::OrientationColumn), m.orientation);
    setData(index(row, G::RotationDegreesColumn), m.rotationDegrees);
    setData(index(row, G::ErrColumn), m.err);
    setData(index(row, G::ShootingInfoColumn), m.shootingInfo);

    if (G::buildingFilters) {
        if (row % 1000 == 0 || row == 0) {
            lastProgressRow = row;
            QString s = "Step 1 0f " + buildSteps + ":  Reading metadata row ";
            s += QString::number(row) + " of " + QString::number(rowCount());
            G::popUp->setPopupText(buildMsg + s);
//            emit setPopupText(buildMsg + s);

            qDebug() << buildMsg + s;
            qApp->processEvents();
        }
    }

    // req'd for 1st image, probably loaded before metadata cached
    if (row == 0) emit updateClassification();

    return true;
}

void DataModel::buildFilters()
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    if (filtersBuilt) return;
//    filtersBuilt = true;

    // collect all unique instances for filtration (use QMap to maintain order)
    QMap<QVariant, QString> modelMap;
    QMap<QVariant, QString> lensMap;
    QMap<QVariant, QString> titleMap;
    QMap<QVariant, QString> flMap;
    QMap<QVariant, QString> creatorMap;
    QMap<QVariant, QString> yearMap;
    QMap<QVariant, QString> dayMap;

    QString s;
    int prev = 0;
    int rows = sf->rowCount();
    for(int row = 0; row < rows; row++) {
        QString model = index(row, G::CameraModelColumn).data().toString();
        if (!modelMap.contains(model)) modelMap[model] = model;

        QString lens = index(row, G::LensColumn).data().toString();
        if (!lensMap.contains(lens)) lensMap[lens] = lens;

        QString title = index(row, G::TitleColumn).data().toString();
        if (!titleMap.contains(title)) titleMap[title] = title;

        QString flNum = index(row, G::FocalLengthColumn).data().toString();
        if (!flMap.contains(flNum)) flMap[flNum] = flNum;

        QString creator = index(row, G::CreatorColumn).data().toString();
        if (!creatorMap.contains(creator)) creatorMap[creator] = creator;

        QString year = index(row, G::YearColumn).data().toString();
        if (!yearMap.contains(year)) yearMap[year] = year;

        QString day = index(row, G::DayColumn).data().toString();
        if (!dayMap.contains(day)) dayMap[day] = day;

        if (row % 1000 == 0 || row == 0) {
            s = "Step 2 0f " + buildSteps + ":  Finding unique items ";
            s += QString::number(row) + " of " + QString::number(rowCount());
            G::popUp->setPopupText(buildMsg + s);
            prev = row;
        }
    }

    // build filter items
    s = "Step 3 0f " + buildSteps + ":  Mapping filters ";
    G::popUp->setPopupText(buildMsg + s);

    filters->addCategoryFromData(modelMap, filters->models);
    filters->addCategoryFromData(lensMap, filters->lenses);
    filters->addCategoryFromData(flMap, filters->focalLengths);
    filters->addCategoryFromData(titleMap, filters->titles);
    filters->addCategoryFromData(creatorMap, filters->creators);
    filters->addCategoryFromData(yearMap, filters->years);
    filters->addCategoryFromData(dayMap, filters->days);

    s = "Step 4 0f " + buildSteps + ":  Tabulating filtered items ...";
    G::popUp->setPopupText(buildMsg + s);
    qApp->processEvents();
    filteredItemCount();
    unfilteredItemCount();

    filtersBuilt = true;
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

void DataModel::updateImageList()
{
/* The image list of file paths replicates the current sort order and
filtration of SortFilter (sf). It is used to keep the image cache in sync with
the current state of SortFilter. This function is called when the user changes
the sort or filter or toggles raw+jpg.
*/
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    imageFilePathList.clear();
    for(int row = 0; row < sf->rowCount(); row++) {
        QString fPath = sf->index(row, 0).data(G::PathRole).toString();
        imageFilePathList.append(fPath);
    }
}

void DataModel::filteredItemCount()
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    qDebug() << __FUNCTION__;

    QTreeWidgetItemIterator it(filters);
    while (*it) {
        if ((*it)->parent()) {
            int col = filters->filterCategoryToDmColumn[(*it)->parent()->text(0)];
            QString searchValue = (*it)->text(1);
            int tot = 0;
            for (int row = 0; row < sf->rowCount(); ++row) {
//                QString value = sf->index(row, col).data().toString();
                if (sf->index(row, col).data().toString() == searchValue) tot++;
//                if (value == searchValue) tot++;
            }
            (*it)->setData(2, Qt::EditRole, QString::number(tot));
            (*it)->setTextAlignment(2, Qt::AlignRight | Qt::AlignVCenter);
        }
        ++it;
    }
}

void DataModel::unfilteredItemCount()
{
/*
This function counts the number of occurences of each unique item in the datamodel, and also
if raw+jpg have been combined.  The results as saved in the filters QTreeWidget in columns
3 (all) and 4 (raw+jpg).

It is only necessary to run this function once for a new folder(s) selection when filtration
has been invoked.  Its counterpart, filteredItemCount, has to be run every time the filters
change.
*/
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    // do not run if already totalled
    bool isUnfilteredItemCount = filters->refineTrue->text(3)!= "" || filters->refineFalse->text(3) != "";
    if (isUnfilteredItemCount) return;

    G::track(__FUNCTION__, "Start");

    QTreeWidgetItemIterator it(filters);
    while (*it) {
        if ((*it)->parent()) {
            int col = filters->filterCategoryToDmColumn[(*it)->parent()->text(0)];
            QString searchValue = (*it)->text(1);
            int tot = 0;
            int totRawJpgCombined = 0;
//            G::track(__FUNCTION__, "Start " + (*it)->text(1));
            for (int row = 0; row < rowCount(); ++row) {
                bool hideRaw = index(row, 0).data(G::DupHideRawRole).toBool();
//                QString value = index(row, col).data().toString();
                if (sf->index(row, col).data().toString() == searchValue) {
                    tot++;
                    if (combineRawJpg && !hideRaw) totRawJpgCombined++;
                }
            }
//            G::track(__FUNCTION__, (*it)->text(1));
            (*it)->setData(3, Qt::EditRole, QString::number(tot));
            (*it)->setTextAlignment(3, Qt::AlignRight | Qt::AlignVCenter);
            (*it)->setData(4, Qt::EditRole, QString::number(totRawJpgCombined));
            (*it)->setTextAlignment(4, Qt::AlignRight | Qt::AlignVCenter);
        }
        ++it;
    }

    G::track(__FUNCTION__, "End");
}

// -----------------------------------------------------------------------------
// SortFilter Class used to filter by row
// -----------------------------------------------------------------------------

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
    {
//    #ifdef ISDEBUG
//    G::track(__FUNCTION__);
//    #endif
    }

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
            /* There is a parent therefore not a top level item so this is one
            of the items to match ie rating = one. If the item has been checked
            then compare the checked filter item to the data in the
            dataModelColumn for the row. If it matches then set isMatch = true.
            If it does not match them isMatch is still false but the row could
            still be accepted if another item in the same category does match.
            */

            if ((*filter)->checkState(0) != Qt::Unchecked) {
                isCategoryUnchecked = false;
                QModelIndex idx = sourceModel()->index(sourceRow, dataModelColumn, sourceParent);
                QVariant dataValue = idx.data(Qt::EditRole);
                QVariant filterValue = (*filter)->data(1, Qt::EditRole);

/*                QString itemName = (*filter)->text(0);      // for debugging
                qDebug() << G::t.restart() << "\t" << itemCategory << itemName
                         << "Comparing" << dataValue << filterValue << (dataValue == filterValue);
*/
                if (dataValue == filterValue) isMatch = true;
            }
        }
        else {
            // top level item = category
            // check results of category items filter match
            if (isCategoryUnchecked) isMatch = true;
//            qDebug() << G::t.restart() << "\t" << "Category" << itemCategory << isMatch;
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
    qDebug() << __FUNCTION__;
    invalidateFilter();
}
