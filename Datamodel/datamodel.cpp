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

Note that more items such as the file offsets to embedded JPG are stored in
the metadata structure which is indexed by the file path.

A QSortFilterProxyModel (SortFilter) is used by ThumbView, TableView,
CompareView and ImageView (sf thoughout the app).

Sorting is applied both from the menu and by clicking TableView headers.  When
sorting occurs all views are updated and the image cache is reindexed.

Filtering is applied from Filters, a QTreeWidget with checkable items for a
number of datamodel columns.  Filtering also updates all views and the image
cache is reloaded.

enum for roles and columns are in global.cpp

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
    setHorizontalHeaderItem(G::LabelColumn, new QStandardItem("Colour"));
    setHorizontalHeaderItem(G::RatingColumn, new QStandardItem("Rating"));
    setHorizontalHeaderItem(G::TypeColumn, new QStandardItem("Type"));
    setHorizontalHeaderItem(G::SizeColumn, new QStandardItem("Size"));
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
    setHorizontalHeaderItem(G::CameraModelColumn, new QStandardItem("Model"));
    setHorizontalHeaderItem(G::LensColumn, new QStandardItem("Lens"));
    setHorizontalHeaderItem(G::FocalLengthColumn, new QStandardItem("Focal length"));
    setHorizontalHeaderItem(G::TitleColumn, new QStandardItem("Title"));

    sf = new SortFilter(this, filters, combineRawJpg);
    sf->setSourceModel(this);

    // root folder containing images to be added to the data model
    dir = new QDir();
    fileFilters = new QStringList;          // eligible image file types
    emptyImg.load(":/images/no_image.png");

    connect(this, SIGNAL(updateMetadata(ImageMetadata)), this, SLOT(updateMetadataItem(ImageMetadata)));
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
    // clear all items for filters based on data content ie file types, camera model
    filters->removeChildrenDynamicFilters();
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
  the rest of the metadata to the datamodel.  Build QMaps of unique field values
  for the filters
*/
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    currentFolderPath = folderPath;

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

//    // clear all items for filters based on data content ie file types, camera model
//    filters->removeChildrenDynamicFilters();

    timeToQuit = false;
    int imageCount = 0;
    QString escapeClause = "Press \"Esc\" to stop.\n\n";
    QString root;
    if (dir->isRoot()) root = "Drive ";
    else root = "Folder ";

    // load file list for the current folder
    int folderImageCount = dir->entryInfoList().size();
    // bail if no images and not including subfolders
    if (!folderImageCount && !includeSubfolders) return false;
    // add supported images in folder to image list
    for (int i = 0; i < folderImageCount; ++i) {
        fileInfoList.append(dir->entryInfoList().at(i));
        imageCount++;
        if (i % 100 == 0 && i > 0) {
            QString s = escapeClause + "Scanning image " +
                        QString::number(i) + " of " +
                        QString::number(folderImageCount) +
                        " in " + root + currentFolderPath;
            emit msg(s);
//            qApp->processEvents();
        }
        if (timeToQuit) return false;
    }
    if (!includeSubfolders) return addFileData();

    // if include subfolders
    int folderCount = 1;
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
                if (i % 100 == 0 && i > 0) {
                    QString s = escapeClause + "Scanning image " +
                                QString::number(i) + " of " +
                                QString::number(folderImageCount) +
                                " in " + currentFolderPath;
                    emit msg(s);
//                    qApp->processEvents();
                }
            }
        }
        // report folder progress
        if (folderCount % 100 == 0 && folderCount > 0) {
            QString s = escapeClause + "Scanning folder " +
                        QString::number(folderCount) +
                        " of parent " + root + currentFolderPath;
            emit msg(s);
            qApp->processEvents();
            if (timeToQuit) return false;
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

        // get file info
        fileInfo = fileInfoList.at(fileIndex);

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
        item->setData("", Qt::DisplayRole);     // column 0 just displays icon
        item->setData(fileInfo.filePath(), G::PathRole);
        item->setData(fileInfo.absoluteFilePath(), Qt::ToolTipRole);
        item->setData(QRect(), G::ThumbRectRole);     // define later when read
        item->setData(false, G::CachedRole);
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
        setData(index(row, G::PickColumn), "false");

        /* Save info for duplicated raw and jpg files, which generally are the
        result of setting raw+jpg in the camera. The datamodel is sorted by
        file path, except raw files with the same path precede jpg files with
        duplicate names. Two roles track duplicates: G::DupHideRawRole flags
        jpg files with duplicate raws and G::DupRawIdxRole points to the
        duplicate raw file from the jpg data row.   For example:

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
        progressBar->updateProgress(row, row + 1, fileInfoList.count(), QColor(85,100,115),
                                "datamodel - adding file info");
        if(row % 100 == 0) qApp->processEvents();

        /* the rest of the data model columns are added after the metadata
        has been loaded, when the image caching is called.  See
        MW::loadImageCache and thumbView::addMetadataToModel    */
    }
    filters->addCategoryFromData(typesMap, filters->types);
    return true;
}

void DataModel::addMetadata(ProgressBar *progressBar, bool isShowCacheStatus)
{
/*
This function is called after the metadata for all the eligible images in
the selected folder have been cached.  The metadata is displayed in tableView,
which is created in MW, and in InfoView.
*/
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }

//    t.restart();

    if(isShowCacheStatus) progressBar->clearProgress();

    hasDupRawJpg = false;

    // collect all unique instances for filtration (use QMap to maintain order)
    QMap<QVariant, QString> modelMap;
    QMap<QVariant, QString> lensMap;
    QMap<QVariant, QString> titleMap;
    QMap<QVariant, QString> flMap;
    QMap<QVariant, QString> creatorMap;
    QMap<QVariant, QString> yearMap;
    QMap<QVariant, QString> dayMap;

    for(int row = 0; row < rowCount(); row++) {
        /*
        qDebug() << "DataModel::addMetadata " << index(row,0).data(G::PathRole).toString()
                 << "\tDupHideRawRole =" << index(row,0).data(G::DupHideRawRole).toBool()
                 << "\tDupRawIdxRole =" << index(row,0).data(G::DupRawIdxRole);
        */
        QModelIndex idx = index(row, G::PathColumn);
        QString fPath = idx.data(G::PathRole).toString();

        QString label = metadata->getLabel(fPath);
        QString rating = metadata->getRating(fPath);
        uint width = metadata->getWidth(fPath);
        uint height = metadata->getHeight(fPath);
        QDateTime createdDate = metadata->getCreatedDate(fPath);
        QString createdDT = createdDate.toString("yyyy-MM-dd hh:mm:ss");
        QString year = createdDate.toString("yyyy");
        QString day = createdDate.toString("yyyy-MM-dd");
        QString mp = QString::number((width * height) / 1000000.0, 'f', 2);
        QString dim = QString::number(width) + "x" + QString::number(height);
        float apertureNum = metadata->getApertureNum(fPath);
        QString ss = metadata->getExposureTime(fPath);
        float ssNum = metadata->getExposureTimeNum(fPath);
        int isoNum = metadata->getISONum(fPath);
        QString model = metadata->getModel(fPath);
        QString lens = metadata->getLens(fPath);
        QString fl = metadata->getFocalLength(fPath);
        int flNum = metadata->getFocalLengthNum(fPath);
        QString title = metadata->getTitle(fPath);
        QString creator = metadata->getCreator(fPath).trimmed();
        QString copyright = metadata->getCopyright(fPath);
        QString email = metadata->getEmail(fPath);
        QString url = metadata->getUrl(fPath);

        if (!creatorMap.contains(creator)) creatorMap[creator] = creator;
        if (!yearMap.contains(year)) yearMap[year] = year;
        if (!dayMap.contains(day)) dayMap[day] = day;
        if (!modelMap.contains(model)) modelMap[model] = model;
        if (!lensMap.contains(lens)) lensMap[lens] = lens;
        if (!flMap.contains(fl)) flMap[flNum] = fl;
        if (!titleMap.contains(title)) titleMap[title] = title;

        setData(index(row, G::LabelColumn), label);
        setData(index(row, G::LabelColumn), Qt::AlignCenter, Qt::TextAlignmentRole);
        setData(index(row, G::RatingColumn), rating);
        setData(index(row, G::RatingColumn), Qt::AlignCenter, Qt::TextAlignmentRole);
        setData(index(row, G::CreatedColumn), createdDT);
        setData(index(row, G::YearColumn), year);
        setData(index(row, G::DayColumn), day);
        setData(index(row, G::MegaPixelsColumn), mp);
        setData(index(row, G::MegaPixelsColumn), int(Qt::AlignRight | Qt::AlignVCenter), Qt::TextAlignmentRole);
        setData(index(row, G::DimensionsColumn), dim);
        setData(index(row, G::RotationColumn), 0);
        setData(index(row, G::RotationColumn), Qt::AlignCenter, Qt::TextAlignmentRole);
        setData(index(row, G::ApertureColumn), apertureNum);
        setData(index(row, G::ApertureColumn), Qt::AlignCenter, Qt::TextAlignmentRole);
        setData(index(row, G::ShutterspeedColumn), ssNum);
        setData(index(row, G::ShutterspeedColumn), int(Qt::AlignRight | Qt::AlignVCenter), Qt::TextAlignmentRole);
        setData(index(row, G::ISOColumn), isoNum);
        setData(index(row, G::ISOColumn), Qt::AlignCenter, Qt::TextAlignmentRole);
        setData(index(row, G::CameraModelColumn), model);
        setData(index(row, G::LensColumn), lens);
        setData(index(row, G::FocalLengthColumn), flNum);
        setData(index(row, G::FocalLengthColumn), int(Qt::AlignRight | Qt::AlignVCenter), Qt::TextAlignmentRole);
        setData(index(row, G::TitleColumn), title);
        setData(index(row, G::CreatorColumn), creator);
        setData(index(row, G::CopyrightColumn), copyright);
        setData(index(row, G::EmailColumn), email);
        setData(index(row, G::UrlColumn), url);

        if(isShowCacheStatus) {
            progressBar->updateProgress(row, row + 1, rowCount(), QColor(100,150,150),
                                    "datamodel - adding metadata");
            if(row % 100 == 0) qApp->processEvents();
        }
    }

    // build filter items
    filters->addCategoryFromData(modelMap, filters->models);
    filters->addCategoryFromData(lensMap, filters->lenses);
    filters->addCategoryFromData(flMap, filters->focalLengths);
    filters->addCategoryFromData(titleMap, filters->titles);
    filters->addCategoryFromData(creatorMap, filters->creators);
    filters->addCategoryFromData(yearMap, filters->years);
    filters->addCategoryFromData(dayMap, filters->days);

    // list used by imageCacheThread, filtered by row+jpg if combined
    for (int i = 0; i < sf->rowCount(); ++i)
            imageFilePathList.append(sf->index(i,0).data(G::PathRole).toString());

    // req'd for 1st image, probably loaded before metadata cached
    emit updateClassification();
    #ifdef ISPROFILE
    G::track(__FUNCTION__, "Leaving...");
    #endif

//    qDebug() << "Sync: Time to get metadata =" << t.elapsed();
}

// ASync
void DataModel::processMetadataBuffer()
{
    static int count = 0;
    count++;
    forever {
        bool more = true;
        bool gotOne = false;
        ImageMetadata m;
        metaHash.takeOne(&m, &more, &gotOne);
        #ifdef ISTEST
        qDebug() << "DataModel::processMetadataBuffer  GotOne Entry:"
                 << count
                 << "row = " << m.row
                 << "gotOne =" << gotOne;
        #endif
//        if (gotOne) emit updateMetadata(m);
        if (gotOne) updateMetadataItem(m);
        if (!more) break;
    }
}

// ASync
bool DataModel::updateMetadataItem(ImageMetadata m)
{
    /*
    This function is called after the metadata for all the eligible images in
    the selected folder have been cached.  The metadata is displayed in tableView,
    which is created in MW, and in InfoView.
    */
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }

    int row = m.row;

    QString fPath = index(row, 0).data(G::PathRole).toString();
#ifdef ISTEST
    qDebug() << "Setting the datamodel for row" << row << fPath;
    qDebug() << "m.label" << m.label
             << "m.createdDate" << m.createdDate.toString("yyyy-MM-dd hh:mm:ss");
#endif

    setData(index(row, G::LabelColumn), m.label);
    setData(index(row, G::LabelColumn), Qt::AlignCenter, Qt::TextAlignmentRole);
    setData(index(row, G::RatingColumn), m.rating);
    setData(index(row, G::RatingColumn), Qt::AlignCenter, Qt::TextAlignmentRole);
    setData(index(row, G::CreatedColumn), m.createdDate.toString("yyyy-MM-dd hh:mm:ss"));
    setData(index(row, G::YearColumn), m.year);
    setData(index(row, G::DayColumn), m.day);
    setData(index(row, G::MegaPixelsColumn), QString::number((m.width * m.height) / 1000000.0, 'f', 2));
    setData(index(row, G::MegaPixelsColumn), int(Qt::AlignRight | Qt::AlignVCenter), Qt::TextAlignmentRole);
    setData(index(row, G::DimensionsColumn), QString::number(m.width) + "x" + QString::number(m.height));
    setData(index(row, G::RotationColumn), 0);
    setData(index(row, G::RotationColumn), Qt::AlignCenter, Qt::TextAlignmentRole);
    setData(index(row, G::ApertureColumn), m.apertureNum);
    setData(index(row, G::ApertureColumn), Qt::AlignCenter, Qt::TextAlignmentRole);
    setData(index(row, G::ShutterspeedColumn), m.exposureTimeNum);
    setData(index(row, G::ShutterspeedColumn), int(Qt::AlignRight | Qt::AlignVCenter), Qt::TextAlignmentRole);
    setData(index(row, G::ISOColumn), m.ISONum);
    setData(index(row, G::ISOColumn), Qt::AlignCenter, Qt::TextAlignmentRole);
    setData(index(row, G::CameraModelColumn), m.model);
    setData(index(row, G::LensColumn), m.lens);
    setData(index(row, G::FocalLengthColumn), m.focalLengthNum);
    setData(index(row, G::FocalLengthColumn), int(Qt::AlignRight | Qt::AlignVCenter), Qt::TextAlignmentRole);
    setData(index(row, G::TitleColumn), m.title);
    setData(index(row, G::CreatorColumn), m.creator);
    setData(index(row, G::CopyrightColumn), m.copyright);
    setData(index(row, G::EmailColumn), m.email);
    setData(index(row, G::UrlColumn), m.url);
//    if(isShowCacheStatus) {
//        progressBar->updateProgress(row, row + 1, rowCount(), Qt::yellow,
//                                "datamodel - adding metadata");
//        progressBar->updateProgress(row, row + 1, rowCount(), QColor(100,150,150),
//                                    "datamodel - adding metadata");
//        qApp->processEvents();
//        if(row % 100 == 0) qApp->processEvents();
//    }


    // list used by imageCacheThread, filtered by row+jpg if combined

    // move somewhere else
    if(row == 0)
        for (int i = 0; i < sf->rowCount(); ++i)
            imageFilePathList.append(sf->index(i,0).data(G::PathRole).toString());

    // req'd for 1st image, probably loaded before metadata cached
    if (row == 0) emit updateClassification();

    return true;
}

// ASync
void DataModel::updateFilters()
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
#ifdef ISTEST
    qDebug() << "DataModel::updateFilters()";
#endif

    // collect all unique instances for filtration (use QMap to maintain order)
    QMap<QVariant, QString> modelMap;
    QMap<QVariant, QString> lensMap;
    QMap<QVariant, QString> titleMap;
    QMap<QVariant, QString> flMap;
    QMap<QVariant, QString> creatorMap;
    QMap<QVariant, QString> yearMap;
    QMap<QVariant, QString> dayMap;

    for(int row = 0; row < rowCount(); row++) {
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
    }

    // build filter items
    filters->addCategoryFromData(modelMap, filters->models);
    filters->addCategoryFromData(lensMap, filters->lenses);
    filters->addCategoryFromData(flMap, filters->focalLengths);
    filters->addCategoryFromData(titleMap, filters->titles);
    filters->addCategoryFromData(creatorMap, filters->creators);
    filters->addCategoryFromData(yearMap, filters->years);
    filters->addCategoryFromData(dayMap, filters->days);
}

QModelIndex DataModel::find(QString fPath)
{
    QModelIndexList idxList = sf->match(sf->index(0, 0), G::PathRole, fPath);
    if (idxList.size() > 0 && idxList[0].isValid()) {
        return idxList[0];
    }
    return index(-1, -1);       // invalid index
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

    QTreeWidgetItemIterator it(filters);
    while (*it) {
        if ((*it)->parent()) {
            int col = filters->filterCategoryToDmColumn[(*it)->parent()->text(0)];
            QString searchValue = (*it)->text(1);
            int tot = 0;
            for (int row = 0; row < sf->rowCount(); ++row) {
                QString value = sf->index(row, col).data().toString();
                if (value == searchValue) tot++;
            }
            (*it)->setData(2, Qt::EditRole, QString::number(tot));
            (*it)->setTextAlignment(2, Qt::AlignRight | Qt::AlignVCenter);
        }
        ++it;
    }
}

void DataModel::unfilteredItemCount()
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }

    QTreeWidgetItemIterator it(filters);
    while (*it) {
        if ((*it)->parent()) {
            int col = filters->filterCategoryToDmColumn[(*it)->parent()->text(0)];
            QString searchValue = (*it)->text(1);
            int tot = 0;
            for (int row = 0; row < rowCount(); ++row) {
                QString value = index(row, col).data().toString();
                if (value == searchValue) tot++;
            }
            (*it)->setData(3, Qt::EditRole, QString::number(tot));
            (*it)->setTextAlignment(3, Qt::AlignRight | Qt::AlignVCenter);
        }
        ++it;
    }
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
    invalidateFilter();
}

