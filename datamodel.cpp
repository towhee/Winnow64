#include "datamodel.h"

DataModel::DataModel(QWidget *parent, Metadata *metadata, Filters *filters) : QStandardItemModel(parent)
{
/*
The datamodel (dm thoughout app) contains information about each eligible image
file in the selected folder (and optionally the subfolder heirarchy).  Eligible
image files, defined in the metadata class, are files Winnow knows how to decode.

The data is structured in columns:

    ● Path:             from QFileInfoList  ToolTipRole + G::ThumbRectRole (icon)
    ● File name:        from QFileInfoList
    ● File type:        from QFileInfoList  EditRole
    ● File size:        from QFileInfoList  EditRole
    ● File created:     from QFileInfoList  EditRole
    ● File modified:    from QFileInfoList  EditRole
    ● Picked:           user edited         EditRole
    ● Rating:           user edited         EditRole
    ● Label:            user edited         EditRole
    ● MegaPixels:       from metadata       EditRole
    ● Dimensions:       from metadata       EditRole
    ● Aperture:         from metadata       EditRole
    ● ShutterSpeed:     from metadata       EditRole
    ● ISO:              from metadata       EditRole
    ● CameraModel:      from metadata       EditRole
    ● FocalLength:      from metadata       EditRole
    ● Title:            from metadata       EditRole

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

*/
    {
    #ifdef ISDEBUG
    qDebug() << "DataModel::DataModel";
    #endif
    }
    mw = parent;
    this->metadata = metadata;
    this->filters = filters;

    setSortRole(Qt::EditRole);
    setHorizontalHeaderItem(G::PathColumn, new QStandardItem(QString("Icon")));
    setHorizontalHeaderItem(G::NameColumn, new QStandardItem(QString("File Name")));
    setHorizontalHeaderItem(G::TypeColumn, new QStandardItem("Type"));
    setHorizontalHeaderItem(G::SizeColumn, new QStandardItem("Size"));
    setHorizontalHeaderItem(G::CreatedColumn, new QStandardItem("Created"));
    setHorizontalHeaderItem(G::ModifiedColumn, new QStandardItem("Last Modified"));
    setHorizontalHeaderItem(G::PickedColumn, new QStandardItem("Pick"));
    setHorizontalHeaderItem(G::LabelColumn, new QStandardItem("   Label   "));
    setHorizontalHeaderItem(G::RatingColumn, new QStandardItem("Rating"));
    setHorizontalHeaderItem(G::MegaPixelsColumn, new QStandardItem("MPix"));
    setHorizontalHeaderItem(G::DimensionsColumn, new QStandardItem("Dimensions"));
    setHorizontalHeaderItem(G::ApertureColumn, new QStandardItem("Aperture"));
    setHorizontalHeaderItem(G::ShutterspeedColumn, new QStandardItem("Shutter"));
    setHorizontalHeaderItem(G::ISOColumn, new QStandardItem("ISO"));
    setHorizontalHeaderItem(G::CameraModelColumn, new QStandardItem("Model"));
    setHorizontalHeaderItem(G::FocalLengthColumn, new QStandardItem("Focal length"));
    setHorizontalHeaderItem(G::TitleColumn, new QStandardItem("Title"));

    sf = new SortFilter(this, filters);
    sf->setSourceModel(this);

    // root folder containing images to be added to the data model
    dir = new QDir();
    fileFilters = new QStringList;          // eligible image file types
    emptyImg.load(":/images/no_image.png");

    // changing filters triggers a refiltering
    connect(filters, SIGNAL(itemChanged(QTreeWidgetItem*,int)),
            sf, SLOT(filterChanged(QTreeWidgetItem*,int)));
}

bool DataModel::load(QString &folderPath, bool includeSubfolders)
{
/*
When a new folder is selected load it into the data model.  This clears the
model and populates the data model with all the cached thumbnail pixmaps from
thumbCache.  If load subfolders has been chosen then the entire subfolder
heirarchy is loaded.
*/
    {
    #ifdef ISDEBUG
    qDebug() << "ThumbView::load";
    #endif
    }
    currentFolderPath = folderPath;

    // do some initializing
    fileFilters->clear();
    foreach (const QString &str, metadata->supportedFormats)
            fileFilters->append("*." + str);
    dir->setNameFilters(*fileFilters);
    dir->setFilter(QDir::Files);
    dir->setPath(currentFolderPath);
    dir->setSorting(QDir::Name);

    removeRows(0, rowCount());
    fileInfoList.clear();

    // clear all items for filters based on data content ie file types, camera model
    filters->removeChildrenDynamicFilters();

    // exit if no images, otherwise get file info and add to model
    if (!addFiles() && !includeSubfolders) return false;

    if (includeSubfolders) {
        qDebug() << "DataModel::load including subfolders";
        QDirIterator it(currentFolderPath, QDirIterator::Subdirectories);
        while (it.hasNext()) {
            it.next();
            if (it.fileInfo().isDir() && it.fileName() != "." && it.fileName() != "..") {
                dir->setPath(it.filePath());
                qDebug() << "ITERATING FOLDER" << it.filePath();
                addFiles();
            }
        }
    }

    // if images were found and added to data model
    if (rowCount()) return true;
    else return false;
}

bool DataModel::addFiles()
{
    {
    #ifdef ISDEBUG
    qDebug() << "ThumbView::addFiles";
    #endif
    }
    QElapsedTimer t;
    t.start();

    if (dir->entryInfoList().size() == 0) return false;

    static QStandardItem *item;
    static int fileIndex;
    static QPixmap emptyPixMap;
    QMap<QVariant, QString> typesMap;

    imageFilePathList.clear();

    // rgh not working
    emptyPixMap = QPixmap::fromImage(emptyImg).scaled(160, 160);

    for (fileIndex = 0; fileIndex < dir->entryInfoList().size(); ++fileIndex) {

        // get file info
        fileInfo = dir->entryInfoList().at(fileIndex);
        fileInfoList.append(fileInfo);
        imageFilePathList.append(fileInfo.absoluteFilePath());
//        QString fPath = fileInfo.filePath();

//        qDebug() << "building data model"
//                 << fileIndex << "of" << dirFileInfoList.size()
//                 << fPath;

        // add icon as first column in new row
        item = new QStandardItem();
        item->setData("", Qt::DisplayRole);     // column 0 just displays icon
        item->setData(fileInfo.filePath(), G::FileNameRole);
        item->setData(fileInfo.absoluteFilePath(), Qt::ToolTipRole);
        item->setData("False", G::PickedRole);
        item->setData("", G::RatingRole);
        item->setData("", G::LabelRole);
        item->setData(QRect(), G::ThumbRectRole);     // define later when read
        item->setData(fileInfo.path(), G::PathRole);
        item->setData(Qt::AlignCenter, Qt::TextAlignmentRole);
        appendRow(item);

        // add columns to model
        int row = item->index().row();

        item = new QStandardItem();
        item->setData(fileInfo.fileName(), Qt::DisplayRole);
        setItem(row, G::NameColumn, item);

        item = new QStandardItem();
        QString s = fileInfo.suffix().toUpper();
        // build list for filters->addCategoryFromData
        typesMap[s] = s;
        item->setData(s, Qt::DisplayRole);
        setItem(row, G::TypeColumn, item);

        item = new QStandardItem();
        item->setData(fileInfo.size(), Qt::DisplayRole);
        item->setData(int(Qt::AlignRight | Qt::AlignVCenter), Qt::TextAlignmentRole);
        setItem(row, G::SizeColumn, item);

        item = new QStandardItem();
        item->setData(fileInfo.created(), Qt::DisplayRole);
        setItem(row, G::CreatedColumn, item);

        item = new QStandardItem();
        item->setData(fileInfo.lastModified(), Qt::DisplayRole);
        setItem(row, G::ModifiedColumn, item);

        item = new QStandardItem();
        item->setData("false", Qt::EditRole);
        setItem(row, G::PickedColumn, item);

        item = new QStandardItem();
        item->setData("", Qt::DisplayRole);
        item->setData(Qt::AlignCenter, Qt::TextAlignmentRole);
        setItem(row, G::LabelColumn, item);

        item = new QStandardItem();
        item->setData("", Qt::DisplayRole);
        item->setData(Qt::AlignCenter, Qt::TextAlignmentRole);
        setItem(row, G::RatingColumn, item);

        /* the rest of the data model columns are added after the metadata
        has been loaded, when the image caching is called.  See
        MW::loadImageCache and thumbView::addMetadataToModel    */

//        qDebug() << "Row =" << row << fPath;
    }

    qDebug() << "Add files elapsed time =" << t.restart() << "ms for "
             << dir->entryInfoList().size() << "files";
    filters->addCategoryFromData(typesMap, filters->types);
    qDebug() << "Add filters elapsed time =" << t.restart();
    return true;
}

void DataModel::addMetadata()
{
/*
This function is called after the metadata for all the eligible images in
the selected folder have been cached.  The metadata is displayed in tableView,
which is created in MW.
*/
    {
    #ifdef ISDEBUG
    qDebug() << "DataModel::addMetadataToModel";
    #endif
    }
    static QStandardItem *item;
    QMap<QVariant, QString> modelMap;
    QMap<QVariant, QString> titleMap;
    QMap<QVariant, QString> flMap;

    for(int row = 0; row < rowCount(); row++) {
        QModelIndex idx = index(row, G::PathColumn);
        QString fPath = idx.data(G::FileNameRole).toString();

        uint width = metadata->getWidth(fPath);
        uint height = metadata->getHeight(fPath);
        QString mp = QString::number((width * height) / 1000000.0, 'f', 2);
        QString dim = QString::number(width) + "x" + QString::number(height);
        QString aperture = metadata->getAperture(fPath);
        float apertureNum = metadata->getApertureNum(fPath);
        QString ss = metadata->getExposureTime(fPath);
        float ssNum = metadata->getExposureTimeNum(fPath);
        QString iso = metadata->getISO(fPath);
        int isoNum = metadata->getISONum(fPath);
        QString model = metadata->getModel(fPath);
        modelMap[model] = model;
        QString fl = metadata->getFocalLength(fPath);
        int flNum = metadata->getFocalLengthNum(fPath);
        flMap[flNum] = fl;
        QString title = metadata->getTitle(fPath);
        titleMap[title] = title;

        setData(index(row, G::MegaPixelsColumn), mp);
        setData(index(row, G::DimensionsColumn), dim);
        setData(index(row, G::ApertureColumn), apertureNum);
        setData(index(row, G::ApertureColumn), Qt::AlignCenter, Qt::TextAlignmentRole);
        setData(index(row, G::ShutterspeedColumn), ssNum);
        setData(index(row, G::ShutterspeedColumn), int(Qt::AlignRight | Qt::AlignVCenter), Qt::TextAlignmentRole);
        setData(index(row, G::ISOColumn), isoNum);
        setData(index(row, G::ISOColumn), Qt::AlignCenter, Qt::TextAlignmentRole);
        setData(index(row, G::CameraModelColumn), model);
        setData(index(row, G::FocalLengthColumn), flNum);
        setData(index(row, G::FocalLengthColumn), int(Qt::AlignRight | Qt::AlignVCenter), Qt::TextAlignmentRole);
        setData(index(row, G::TitleColumn), title);
    }
    // build filter items
    filters->addCategoryFromData(modelMap, filters->models);
    filters->addCategoryFromData(titleMap, filters->titles);
    filters->addCategoryFromData(flMap, filters->focalLengths);
}

void DataModel::updateImageList()
{
/* The image list of file paths replicates the current sort order and filtration
of SortFilter (sf).  It is used to keep the image cache in sync with the
current state of SortFilter.  This function is called when the user
changes the sort or filter.
*/
    {
    #ifdef ISDEBUG
    qDebug() << "ThumbView::updateImageList";
    #endif
    }
//    qDebug() << "ThumbView::updateImageList";
    imageFilePathList.clear();
    for(int row = 0; row < sf->rowCount(); row++) {
        QString fPath = sf->index(row, 0).data(G::FileNameRole).toString();
        imageFilePathList.append(fPath);
//        qDebug() << "&&&&&&&&&&&&&&&&&& updateImageList:" << fPath;
    }
}


// -----------------------------------------------------------------------------
// SortFilter Class used to filter by row
// -----------------------------------------------------------------------------

SortFilter::SortFilter(QObject *parent, Filters *filters) : QSortFilterProxyModel(parent)
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
//    qDebug() << "SortFilter::filterAcceptsRow";
//    #endif
    }
    if (!G::isNewFolderLoaded) return true;

    static int counter = 0;
    counter++;
//    qDebug() << "Filtering" << counter;
    QString itemCategory = "Just starting";

    int dataModelColumn;
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

//                QString itemName = (*filter)->text(0);      // for debugging
//                qDebug() << itemCategory << itemName
//                         << "Comparing" << dataValue << filterValue << (dataValue == filterValue);

                if (dataValue == filterValue) isMatch = true;
            }
        }
        else {
            // top level item = category
            // check results of category items filter match
            if (isCategoryUnchecked) isMatch = true;
//            qDebug() << "Category" << itemCategory << isMatch;
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

//    qDebug() << "SortFilter::filterAcceptsRow After iteration  isMatch =" << isMatch;

    return isMatch;
}

void SortFilter::filterChanged(QTreeWidgetItem* x, int col)
{
/*
This slot is called when data changes in the filters widget.  The proxy (this)
is invalidated, which forces an update.  This happens with every change in the
filters widget including when the filters are being created in the filter
widget.

If the new folder has been loaded and there are user driven changes to the
filtration then the image cache needs to be reloaded to match the new proxy (sf)
*/
    {
    #ifdef ISDEBUG
    qDebug() << "SortFilter::filterChanged";
    #endif
    }
    this->invalidateFilter();
//    qDebug() << "filterChanged" << x << "column" << col << "isFinished" << G::isNewFolderLoaded;
    if (G::isNewFolderLoaded) emit reloadImageCache();
}
