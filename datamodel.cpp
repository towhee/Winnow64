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
    setHorizontalHeaderItem(G::PickedColumn, new QStandardItem("Pick"));
    setHorizontalHeaderItem(G::LabelColumn, new QStandardItem("Colour"));
    setHorizontalHeaderItem(G::RatingColumn, new QStandardItem("Rating"));
    setHorizontalHeaderItem(G::TypeColumn, new QStandardItem("Type"));
    setHorizontalHeaderItem(G::SizeColumn, new QStandardItem("Size"));
    setHorizontalHeaderItem(G::CreatedColumn, new QStandardItem("Created"));
    setHorizontalHeaderItem(G::ModifiedColumn, new QStandardItem("Last Modified"));
    setHorizontalHeaderItem(G::CreatorColumn, new QStandardItem("Creator"));
    setHorizontalHeaderItem(G::MegaPixelsColumn, new QStandardItem("MPix"));
    setHorizontalHeaderItem(G::DimensionsColumn, new QStandardItem("Dimensions"));
    setHorizontalHeaderItem(G::ApertureColumn, new QStandardItem("Aperture"));
    setHorizontalHeaderItem(G::ShutterspeedColumn, new QStandardItem("Shutter"));
    setHorizontalHeaderItem(G::ISOColumn, new QStandardItem("ISO"));
    setHorizontalHeaderItem(G::CameraModelColumn, new QStandardItem("Model"));
    setHorizontalHeaderItem(G::LensColumn, new QStandardItem("Lens"));
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

    // collect all unique instances for filtration (use QMap to maintain order)
    QMap<QVariant, QString> typesMap;

    imageFilePathList.clear();

    // rgh not working
    emptyPixMap = QPixmap::fromImage(emptyImg).scaled(160, 160);

    for (fileIndex = 0; fileIndex < dir->entryInfoList().size(); ++fileIndex) {

        // get file info
        fileInfo = dir->entryInfoList().at(fileIndex);
        imageFilePathList.append(fileInfo.absoluteFilePath());

        /* add icon as first column in new row

        // this approach is slower than adding setData(index(row, column), x)
        // for the first item that is req'd to append a new item to the datamodel
        item = new QStandardItem();
        appendRow(item);
        int row = item->index().row();
        setData(index(row, G::PathColumn), fileInfo.fileName(), Qt::DisplayRole);
        setData(index(row, G::PathColumn), fileInfo.filePath(), G::FileNameRole);
        setData(index(row, G::PathColumn), fileInfo.absoluteFilePath(), Qt::ToolTipRole);
        setData(index(row, G::PathColumn), fileInfo.path(), G::PathRole);   // can we combine and eliminate
        setData(index(row, G::PathColumn), QRect(), G::ThumbRectRole);
        setData(index(row, G::PathColumn), Qt::AlignCenter, Qt::TextAlignmentRole);
*/

        item = new QStandardItem();
        item->setData("", Qt::DisplayRole);     // column 0 just displays icon
        item->setData(fileInfo.filePath(), G::FileNameRole);
        item->setData(fileInfo.absoluteFilePath(), Qt::ToolTipRole);
        item->setData(QRect(), G::ThumbRectRole);     // define later when read
        item->setData(fileInfo.path(), G::PathRole);
        item->setData(Qt::AlignCenter, Qt::TextAlignmentRole);
        appendRow(item);

        // add columns that do not require metadata read form files
        int row = item->index().row();

        setData(index(row, G::NameColumn), fileInfo.fileName());
        setData(index(row, G::TypeColumn), fileInfo.suffix().toUpper());
        QString s = fileInfo.suffix().toUpper();
        // build list for filters->addCategoryFromData
        typesMap[s] = s;
        setData(index(row, G::TypeColumn), s);
        setData(index(row, G::SizeColumn), fileInfo.size());
        setData(index(row, G::SizeColumn), int(Qt::AlignRight | Qt::AlignVCenter), Qt::TextAlignmentRole);
//        setData(index(row, G::CreatedColumn), fileInfo.created());
        setData(index(row, G::ModifiedColumn), fileInfo.lastModified());
        setData(index(row, G::PickedColumn), "false");
        setData(index(row, G::LabelColumn), "");
        setData(index(row, G::LabelColumn), Qt::AlignCenter, Qt::TextAlignmentRole);
        setData(index(row, G::RatingColumn), "");
        setData(index(row, G::RatingColumn), Qt::AlignCenter, Qt::TextAlignmentRole);

        /* the rest of the data model columns are added after the metadata
        has been loaded, when the image caching is called.  See
        MW::loadImageCache and thumbView::addMetadataToModel    */

//        qDebug() << "Row =" << row << fPath;
    }

    qDebug() << "Add files elapsed time =" << t.restart() << "ms for "
             << dir->entryInfoList().size() << "files"
             << "from" << dir->dirName();
    filters->addCategoryFromData(typesMap, filters->types);
    return true;
}

void DataModel::addMetadata()
{
/*
This function is called after the metadata for all the eligible images in
the selected folder have been cached.  The metadata is displayed in tableView,
which is created in MW, and in InfoView.
*/
    {
    #ifdef ISDEBUG
    qDebug() << "DataModel::addMetadataToModel";
    #endif
    }
    QElapsedTimer t;
    t.start();

//    static QStandardItem *item;

    // collect all unique instances for filtration (use QMap to maintain order)
    QMap<QVariant, QString> modelMap;
    QMap<QVariant, QString> lensMap;
    QMap<QVariant, QString> titleMap;
    QMap<QVariant, QString> flMap;
    QMap<QVariant, QString> creatorMap;

    for(int row = 0; row < rowCount(); row++) {
        QModelIndex idx = index(row, G::PathColumn);
        QString fPath = idx.data(G::FileNameRole).toString();

        uint width = metadata->getWidth(fPath);
        uint height = metadata->getHeight(fPath);
        QString created = metadata->getCreated(fPath);
        QString mp = QString::number((width * height) / 1000000.0, 'f', 2);
        QString dim = QString::number(width) + "x" + QString::number(height);
//        QString aperture = metadata->getAperture(fPath);
        float apertureNum = metadata->getApertureNum(fPath);
        QString ss = metadata->getExposureTime(fPath);
        float ssNum = metadata->getExposureTimeNum(fPath);
//        QString iso = metadata->getISO(fPath);
        int isoNum = metadata->getISONum(fPath);
        QString model = metadata->getModel(fPath);
        modelMap[model] = model;
        QString lens = metadata->getLens(fPath);
        lensMap[lens] = lens;
        QString fl = metadata->getFocalLength(fPath);
        int flNum = metadata->getFocalLengthNum(fPath);
        flMap[flNum] = fl;
        QString title = metadata->getTitle(fPath);
        titleMap[title] = title;
        QString creator = metadata->getCreator(fPath);
        creatorMap[creator] = creator;
        QString copyright = metadata->getCopyright(fPath);
        QString email = metadata->getEmail(fPath);
        QString url = metadata->getUrl(fPath);

        setData(index(row, G::CreatedColumn), created);
        setData(index(row, G::MegaPixelsColumn), mp);
        setData(index(row, G::DimensionsColumn), dim);
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
    }
    // build filter items
    filters->addCategoryFromData(modelMap, filters->models);
    filters->addCategoryFromData(lensMap, filters->lenses);
    filters->addCategoryFromData(flMap, filters->focalLengths);
    filters->addCategoryFromData(titleMap, filters->titles);
    filters->addCategoryFromData(creatorMap, filters->creators);

//    qDebug() << "add metadata elapsed time =" << t.restart();

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

/*                QString itemName = (*filter)->text(0);      // for debugging
                qDebug() << itemCategory << itemName
                         << "Comparing" << dataValue << filterValue << (dataValue == filterValue);
*/
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

void SortFilter::filterChanged(QTreeWidgetItem* /* not used */, int /* not used */)
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

