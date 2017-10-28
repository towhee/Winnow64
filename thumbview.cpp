#include "thumbview.h"

/*  ThumbView Overview

ThumbView manages the list of images within a folder and it's children
(optional). The thumbView can either be a QListView of file names or thumbnails.
When a list item is selected the image is shown in imageView.

The thumbView is inside a QDockWidget which allows it to dock and be moved and
resized.

ThumbView does the following:

    Manages the file list of eligible images, including attributes for
    selected and picked. Picked files are shown in green and can be filtered
    and copied to another folder via the copyPickDlg class.

    The thumbViewDelegate class formats the look of the thumbnails.

    Sorts the list based on date acquired, filename and forward/reverse

    Provides functions to navigate the list, including current index, next item,
    previous item, first item, last item and random item.

    Changes in selection trigger the MW::fileSelectionChange which loads the new
    selection in imageView and updates status.

    The mouse click location within the thumb is used in ImageView to pan a
    zoomed image.

QStandardItemModel roles used:

    1   DecorationRole - holds the thumb as an icon
    3   ToolTipRole - the file path
    8   BackgroundRole - the color of the background
   13   SizeHintRole - the QSize of the icon

   User defined roles include:

        LoadedRole - is loaded bool
        FileNameRole - file path qstring
        SortRole - int
        PickRole - bool is picked

Note that a "row" in this class refers to the row in the model, which has one
thumb per row, not the view in the dock, where there can be many thumbs per row

ThumbView behavior as container QDockWidget (thumbDock in MW), changes:

    Behavior is controlled in prefDlg

        ● thumb/icon size
        ● thumb padding
        ● thumb spacing
        ● thumb label
        ● if dock is top or bottom
            ● thumb wrapping
            ● vertical title bar visible

    Triggers:

        ● MW thumbDock Signal dockLocationChanged
        ● MW thumbDock Signal topLevelChanged
        ● MW::eventFilter() resize event

    The thumbDock dimensions are controlled by the size of its contents - the
    thumbView. When docked in bottom or top and thumb wrapping is false the
    maximum height is defined by thumbView->setMaximumHeight().

    ThumbDock resize to fit thumbs:

        This only occurs when the thumbDock is located in the top or bottom
        dock locations and wrap thumbs is not checked in prefDlg.

        When a relocation to the top or bottom occurs the height of the
        thumbDock is adjusted to fit the current size of the thumbs, depending
        on whether a scrollbar is required. It can also occur when a new folder
        is selected and the scrollbar requirement changes, depending on the
        number of images in the folder. This behavior is managed in
        MW::setThumbDockFeatures.

        Also, when thumbs are resized the height of the thumbDock is adjusted
        to accomodate the new thumb height, factoring in whether a scrollbar is
        required.

    Thumb resize to fit in dock:

        This also only occurs when the thumbDock is located in the top or bottom
        dock locations and wrap thumbs is not checked in prefDlg.  If the
        thumbDock horizontal splitter is dragged to make the thumbDock taller
        or shorter, within the minimum and maximum thumbView heights, the thumb
        sizes are adjusted to fit, factoring in the need for a scrollbar.

        This is triggered by MW::eventFilter() and effectuated in thumbsFit().

*/

ThumbView::ThumbView(QWidget *parent, Metadata *metadata)
    : QListView(parent)
{
    {
    #ifdef ISDEBUG
    qDebug() << "ThumbView::ThumbView";
    #endif
    }
    mw = parent;
    this->metadata = metadata;
//    this->filterView = filterView;

//    thumbHeight = G::thumbHeight;
    pickFilter = false;
    setViewMode(QListView::IconMode);

    setSelectionMode(QAbstractItemView::ExtendedSelection);
    setResizeMode(QListView::Adjust);
    setLayoutMode(QListView::Batched);
//    setBatchSize(2);
    setWordWrap(true);
    setDragEnabled(true);
    setEditTriggers(QAbstractItemView::NoEditTriggers);
    setUniformItemSizes(false);
//    setSizeAdjustPolicy(QAbstractScrollArea::AdjustToContents);
    setMaximumHeight(100000);
    this->setContentsMargins(0,0,0,0);

    thumbViewModel = new QStandardItemModel();
    thumbViewModel->setSortRole(Qt::EditRole);
    thumbViewModel->setHorizontalHeaderItem(PathColumn, new QStandardItem(QString("Icon")));
    thumbViewModel->setHorizontalHeaderItem(NameColumn, new QStandardItem(QString("File Name")));
    thumbViewModel->setHorizontalHeaderItem(TypeColumn, new QStandardItem("Type"));
    thumbViewModel->setHorizontalHeaderItem(SizeColumn, new QStandardItem("Size"));
    thumbViewModel->setHorizontalHeaderItem(CreatedColumn, new QStandardItem("Created"));
    thumbViewModel->setHorizontalHeaderItem(ModifiedColumn, new QStandardItem("Last Modified"));
    thumbViewModel->setHorizontalHeaderItem(PickedColumn, new QStandardItem("Pick"));
    thumbViewModel->setHorizontalHeaderItem(LabelColumn, new QStandardItem("Label"));
    thumbViewModel->setHorizontalHeaderItem(RatingColumn, new QStandardItem("Rating"));
    thumbViewModel->setHorizontalHeaderItem(MegaPixelsColumn, new QStandardItem("MPix"));
    thumbViewModel->setHorizontalHeaderItem(DimensionsColumn, new QStandardItem("Dimensions"));
    thumbViewModel->setHorizontalHeaderItem(ApertureColumn, new QStandardItem("Aperture"));
    thumbViewModel->setHorizontalHeaderItem(ShutterspeedColumn, new QStandardItem("Shutter"));
    thumbViewModel->setHorizontalHeaderItem(ISOColumn, new QStandardItem("ISO"));
    thumbViewModel->setHorizontalHeaderItem(CameraModelColumn, new QStandardItem("Model"));
    thumbViewModel->setHorizontalHeaderItem(FocalLengthColumn, new QStandardItem("Focal length"));
    thumbViewModel->setHorizontalHeaderItem(TitleColumn, new QStandardItem("Title"));

//    thumbViewFilter = new ThumbViewFilter();
//    thumbViewFilter = new ThumbViewFilter(filterView);
    thumbViewFilter = new QSortFilterProxyModel;
    thumbViewFilter->setSourceModel(thumbViewModel);
    thumbViewFilter->setFilterRole(PickedRole);
    thumbViewFilter->setSortRole(Qt::EditRole);
    setModel(thumbViewFilter);

    thumbViewSelection = selectionModel();

    thumbViewDelegate = new ThumbViewDelegate(this);
    thumbViewDelegate->setThumbDimensions(thumbWidth, thumbHeight,
        thumbPadding, labelFontSize, showThumbLabels);
    setItemDelegate(thumbViewDelegate);

    // triggers MW::fileSectionChange
    connect(this->selectionModel(),
            SIGNAL(selectionChanged(QItemSelection, QItemSelection)),
            parent, SLOT(fileSelectionChange()));

//    connect(this, SIGNAL(activated(QModelIndex)),
//            this, SLOT(activate(QModelIndex)));

//    connect(this, SIGNAL(doubleClicked(const QModelIndex &)), parent,
//            SLOT(loadImagefromThumb(const QModelIndex &)));

    // used to provide iconRect info to zoom to point clicked on thumb
    // in imageView
    connect(thumbViewDelegate, SIGNAL(update(QModelIndex, QRect)),
            this, SLOT(updateThumbRectRole(QModelIndex, QRect)));

    thumbsDir = new QDir();
    fileFilters = new QStringList;

    emptyImg.load(":/images/no_image.png");

//    QTime time = QTime::currentTime();
}

//bool ThumbView::event(QEvent *event)
//{
///* Just in case we need to override a keystroke in the thumbview */
//    bool override = false;
////    qDebug() << "thumbView events:" << event;
//    if (event->type() == QEvent::UpdateLater ||
//        event->type() == QEvent::Paint) {
////        forceScroll(0);
//    }
//    if (event->type() == QEvent::Resize) {
////        forceScroll(40);
//    }
//    if (event->type() == QEvent::KeyPress) {
//        QKeyEvent *keyEvent = static_cast<QKeyEvent*>(event);
////        if (keyEvent->key() == Qt::Key_Right) {
////            override = true;
////            selectNext();
////        }
////        if (keyEvent->key() == Qt::Key_Left) {
////            override = true;
////            selectPrev();
////        }
//    }
//    if (!override) return QListView::event(event);
//}

void ThumbView::reportThumbs()
{
//    QModelIndex idx;
//    qDebug() << "List all thumbs";
//    for (int i=0; i<thumbViewFilter->rowCount(); i++) {
//        idx = thumbViewFilter->index(i, 0, QModelIndex());
//        qDebug() << i << idx.data(FileNameRole).toString();
//    }
}

void ThumbView::refreshThumbs() {
    {
    #ifdef ISDEBUG
    qDebug() << "ThumbView::refreshThumbs";
    #endif
    }
    // if workspace invoked with diff thumb parameters
//    setThumbParameters();
    this->dataChanged(thumbViewFilter->index(0, 0, QModelIndex()),
      thumbViewFilter->index(getLastRow(), 0, QModelIndex()));
}

void ThumbView::setThumbParameters()
{
/*
Helper function for in class calls where thumb parameters already defined
*/
    {
    #ifdef ISDEBUG
    qDebug() << "ThumbView::setThumbParameters";
    #endif
    }
    qDebug() << "ThumbView::setThumbParameters "
             << " isGrid =" << isGrid
             << " isFloat =" << isFloat
             << "thumbHeight = " << thumbHeight;

    if (isGrid) {
//        setWrapping(true);
//        setSpacing(thumbSpacingGrid);
        thumbViewDelegate->setThumbDimensions(thumbWidthGrid, thumbHeightGrid,
            thumbPaddingGrid, labelFontSizeGrid, showThumbLabelsGrid);
    } else {
//        setWrapping(isThumbWrapWhenTopOrBottomDock && isTopOrBottomDock);
//        if (!isTopOrBottomDock) setWrapping(true);
        setSpacing(thumbSpacing);
        thumbViewDelegate->setThumbDimensions(thumbWidth, thumbHeight,
            thumbPadding, labelFontSize, showThumbLabels);
    }
//    qDebug() << "ThumbView::setThumbParameters isGrid:" << isGrid;
}

void ThumbView::setThumbParameters(int _thumbWidth, int _thumbHeight,
        int _thumbSpacing, int _thumbPadding, int _labelFontSize,
        bool _showThumbLabels)
{
    {
    #ifdef ISDEBUG
    qDebug() << "ThumbView::setThumbParameters with args";
    #endif
    }
    thumbWidth = _thumbWidth;
    thumbHeight = _thumbHeight;
    thumbSpacing = _thumbSpacing;
    thumbPadding = _thumbPadding;
    labelFontSize = _labelFontSize;
    showThumbLabels = _showThumbLabels;

    setThumbParameters();
}

void ThumbView::setThumbGridParameters(int _thumbWidthGrid, int _thumbHeightGrid,
                int _thumbSpacingGrid,int _thumbPaddingGrid, int _labelFontSizeGrid,
                bool _showThumbLabelsGrid)
{
    /*
    Helper function for in class calls where thumb parameters already defined
    */
    {
    #ifdef ISDEBUG
    qDebug() << "ThumbView::setThumbGridParameters";
    #endif
    }
    thumbWidthGrid = _thumbWidthGrid;
    thumbHeightGrid = _thumbHeightGrid;
    thumbSpacingGrid = _thumbSpacingGrid;
    thumbPaddingGrid = _thumbPaddingGrid;
    labelFontSizeGrid = _labelFontSizeGrid;
    showThumbLabelsGrid = _showThumbLabelsGrid;

    setThumbParameters();
}

int ThumbView::getThumbSpaceMin()
{
    return 40 + thumbSpacing * 2 + thumbPadding *2 + 8;
}

int ThumbView::getThumbSpaceMax()
{
    return 160 + thumbSpacing * 2 + thumbPadding *2 + 8;
}

QSize ThumbView::getThumbCellSize()
{
//    int w = thumbWidth + thumbSpacing * 2 + thumbPadding *2 + 8;
//    int h = thumbHeight + thumbSpacing * 2 + thumbPadding *2 + 8;
//    return QSize(w, h);
    return thumbViewDelegate->getThumbCell();
}

// debugging
void ThumbView::reportThumb()
{
    {
    #ifdef ISDEBUG
    qDebug() << "ThumbView::reportThumb";
    #endif
    }
    int currThumb = currentIndex().row();
    qDebug() << "\n ***** THUMB INFO *****";
    qDebug() << "Row =" << currThumb;
    qDebug() << "LoadedRole " << LoadedRole << thumbViewModel->item(currThumb)->data(LoadedRole).toBool();
    qDebug() << "FileNameRole " << FileNameRole << thumbViewModel->item(currThumb)->data(FileNameRole).toString();
    qDebug() << "SortRole " << SortRole << thumbViewModel->item(currThumb)->data(SortRole).toInt();
    qDebug() << "PickedRole " << PickedRole << thumbViewModel->item(currThumb)->data(PickedRole).toString();
    qDebug() << "FileTypeRole " << FileTypeRole << thumbViewModel->item(currThumb)->data(FileTypeRole).toString();
    qDebug() << "FileSizeRole " << FileSizeRole << thumbViewModel->item(currThumb)->data(FileSizeRole).toInt();
    qDebug() << "CreatedRole " << CreatedRole << thumbViewModel->item(currThumb)->data(CreatedRole).toDateTime();
    qDebug() << "ModifiedRole " << ModifiedRole << thumbViewModel->item(currThumb)->data(ModifiedRole).toDateTime();
    qDebug() << "LabelRole " << ModifiedRole << thumbViewModel->item(currThumb)->data(LabelRole).toInt();
    qDebug() << "ModifiedRole " << ModifiedRole << thumbViewModel->item(currThumb)->data(RatingRole).toInt();

    // following crashes when columns not added
//    QModelIndex idx1 = thumbViewFilter->index(currThumb, 1, QModelIndex());
//    qDebug() << "Column 1 Type:" << idx1.data(Qt::DisplayRole);

//    qDebug() << thumbViewModel->item(currThumb)->data(DisplayRole).toString();
//    qDebug() << "\nAll roles:";
//    for (int i=0; i<15; ++i) {
//        qDebug() << i << ":  " << thumbViewModel->item(currThumb)->data(i);
//    }
}

int ThumbView::getCurrentRow()
{
    {
    #ifdef ISDEBUG
    qDebug() << "ThumbView::getCurrentRow";
    #endif
    }
    return currentIndex().row();
}

int ThumbView::getNextRow()
{
    {
    #ifdef ISDEBUG
    qDebug() << "ThumbView::getNextRow";
    #endif
    }
    int row = currentIndex().row();
    if (row == thumbViewFilter->rowCount() - 1)
        return row;
    return row + 1;
}

int ThumbView::getPrevRow()
{
    {
    #ifdef ISDEBUG
    qDebug() << "ThumbView::getPrevRow";
    #endif
    }
    int row = currentIndex().row();
    if (row == 0)
        return 0;
    return row - 1;
}

int ThumbView::getLastRow()
{
    {
    #ifdef ISDEBUG
    qDebug() << "ThumbView::getLastRow";
    #endif
    }
    return thumbViewFilter->rowCount() - 1;
}

int ThumbView::getRandomRow()
{
    {
    #ifdef ISDEBUG
    qDebug() << "ThumbView::getRandomRow";
    #endif
    }
    return qrand() % (thumbViewFilter->rowCount());
}

bool ThumbView::isSelectedItem()
{
    // call before getting current row or index
    {
    #ifdef ISDEBUG
    qDebug() << "ThumbView::isSelectedItem";
    #endif
    }
    if (selectionModel()->selectedRows().size() == 1)
        return true;
    else
        return false;
}

// not used but might be useful
QString ThumbView::getCurrentFilename()
{
    {
    #ifdef ISDEBUG
    qDebug() << "ThumbView::getCurrentFilename";
    #endif
    }
    return currentIndex().data(FileNameRole).toString();
}

// PICKS: Items that have been picked

// used in MW::copyPicks
bool ThumbView::isPick()
{
    {
    #ifdef ISDEBUG
    qDebug() << "ThumbView::isPick";
    #endif
    }
    int pickCount = 0;
    for (int row = 0; row < thumbViewFilter->rowCount(); ++row) {
        QModelIndex idx = thumbViewFilter->index(row, 0, QModelIndex());
        if (idx.data(PickedRole).toString() == "true") ++pickCount;
    }
    return (pickCount > 0);
}

QFileInfoList ThumbView::getPicks()
{
/* Returns a list of all the files that have been picked.  It is used in
MW, passing the list on to the copyPicksDlg for ingestion/copying to another
folder.
*/
    {
    #ifdef ISDEBUG
    qDebug() << "ThumbView::getPicks";
    #endif
    }
    QFileInfoList fileInfoList;
    for (int row = 0; row < thumbViewFilter->rowCount(); ++row) {
        QModelIndex idx = thumbViewFilter->index(row, 0, QModelIndex());
        if (idx.data(PickedRole).toString() == "true") {
            QFileInfo fileInfo(idx.data(FileNameRole).toString());
            fileInfoList.append(fileInfo);
        }
    }
    return fileInfoList;
}

int ThumbView::getNextPick()
{
    {
    #ifdef ISDEBUG
    qDebug() << "ThumbView::getNextPick";
    #endif
    }
    int frwd = currentIndex().row() + 1;
    int rowCount = thumbViewFilter->rowCount();
    QModelIndex idx;
    while (frwd < rowCount) {
        idx = thumbViewFilter->index(frwd, 0, QModelIndex());
        if (idx.data(PickedRole).toString() == "true") return frwd;
        ++frwd;
    }
    return -1;
}

int ThumbView::getPrevPick()
{
    {
    #ifdef ISDEBUG
    qDebug() << "ThumbView::getPrevPick";
    #endif
    }
    int back = currentIndex().row() - 1;
//    int rowCount = thumbViewFilter->rowCount();
    QModelIndex idx;
    while (back >= 0) {
        idx = thumbViewFilter->index(back, 0, QModelIndex());
        if (idx.data(PickedRole).toString() == "true") return back;
        --back;
    }
    return -1;
}

int ThumbView::getNearestPick()
{
/* Returns the model row of the nearest pick, used in toggleFilterPick */
    {
    #ifdef ISDEBUG
    qDebug() << "ThumbView::getNearestPick";
    #endif
    }
    int frwd = currentIndex().row();
    int back = frwd;
    int rowCount = thumbViewFilter->rowCount();
    QModelIndex idx;
    while (back >=0 || frwd < rowCount) {
        if (back >=0) idx = thumbViewFilter->index(back, 0, QModelIndex());
        if (idx.data(PickedRole).toString() == "true") return back;
        if (frwd < rowCount) idx = thumbViewFilter->index(frwd, 0, QModelIndex());
        if (idx.data(PickedRole).toString() == "true") return frwd;
        --back;
        ++frwd;
    }
    return 0;
}

void ThumbView::toggleFilterPick(bool isFilter)
{
    {
    #ifdef ISDEBUG
    qDebug() << "ThumbView::toggleFilterPick";
    #endif
    }
    pickFilter = isFilter;
    if (pickFilter) {
        int row = getNearestPick();
        selectThumb(row);
        thumbViewFilter->setFilterRegExp("true");   // show only picked items
    }
    else
        thumbViewFilter->setFilterRegExp("");       // no filter - show all
}

void ThumbView::sortThumbs(int sortColumn, bool isReverse)
{
    {
    #ifdef ISDEBUG
    qDebug() << "ThumbView::sortThumbs";
    #endif
    }
    if (isReverse) thumbViewFilter->sort(sortColumn, Qt::DescendingOrder);
    else thumbViewFilter->sort(sortColumn, Qt::AscendingOrder);

    scrollTo(currentIndex(), ScrollHint::PositionAtCenter);
    updateImageList();      // req'd to reindex image cache after re-sort
//    refreshThumbs();
}

QStringList ThumbView::getSelectedThumbsList()
{
/* This was used by the eliminated tags class and is not used but looks
useful.
*/
    {
    #ifdef ISDEBUG
    qDebug() << "ThumbView::getSelectedThumbsList";
    #endif
    }
    {
    #ifdef ISDEBUG
    qDebug() << "ThumbView::getSelectedThumbList";
    #endif
    }
    QModelIndexList indexesList = selectionModel()->selectedIndexes();
    QStringList SelectedThumbsPaths;

    for (int tn = indexesList.size() - 1; tn >= 0 ; --tn) {
        SelectedThumbsPaths << indexesList[tn].data(FileNameRole).toString();
    }
    return SelectedThumbsPaths;
}

bool ThumbView::load(QString &dir, bool includeSubfolders)
{
/* When a new folder is selected load it into the thumbView.  This clears the
model and populates the QListView with all the cached thumbnail pixmaps from
thumbCache.
*/
    {
    #ifdef ISDEBUG
    qDebug() << "ThumbView::load";
    #endif
    }
    currentViewDir = dir;
    if (!pickFilter) {
        initLoad();
        // exit if no images, otherwise get thumb info
        if (!addFolderImageDataToModel() && !includeSubfolders) return false;
    }

//    setWrapping(true);
    if (includeSubfolders) {
        qDebug() << "ThumbView::load including subfolders";
        QDirIterator iterator(currentViewDir, QDirIterator::Subdirectories);
        while (iterator.hasNext()) {
            iterator.next();
            if (iterator.fileInfo().isDir() && iterator.fileName() != "." && iterator.fileName() != "..") {
                thumbsDir->setPath(iterator.filePath());
                qDebug() << "ITERATING FOLDER" << iterator.filePath();
                addFolderImageDataToModel();
            }
        }
    }

    if (thumbFileInfoList.size() && selectionModel()->selectedIndexes().size() == 0) {
        selectThumb(0);
        return true;
    }
    return false;   // no images found in any folder
}

void ThumbView::initLoad()
{

/*  Prepares the thumbview for new data
      * thumb layout
      * thumb size
      * file filter
      * sorting
      * clear model and run parameters      */

    {
    #ifdef ISDEBUG
    qDebug() << "ThumbView::loadPrepare";
    #endif
    }
    fileFilters->clear();
    foreach (const QString &str, metadata->supportedFormats)
            fileFilters->append("*." + str);
    thumbsDir->setNameFilters(*fileFilters);
    thumbsDir->setFilter(QDir::Files);
//	if (GData::showHiddenFiles) {
//		thumbsDir->setFilter(thumbsDir->filter() | QDir::Hidden);
//	}

    thumbsDir->setPath(currentViewDir);

    thumbsDir->setSorting(QDir::Name);

    // if use thumbViewModel->clear() headers are deleted
    thumbViewModel->removeRows(0, thumbViewModel->rowCount());
//    thumbViewModel->setSortRole(Qt::EditRole);
    thumbFileInfoList.clear();
}

bool ThumbView::addFolderImageDataToModel()
{
    {
    #ifdef ISDEBUG
    qDebug() << "ThumbView::addFolderImageDataToModel";
    #endif
    }
    dirFileInfoList = thumbsDir->entryInfoList();
    if (dirFileInfoList.size() == 0) return false;

    static QStandardItem *item;
    static int fileIndex;
    static QPixmap emptyPixMap;

    // rgh not working
    emptyPixMap = QPixmap::fromImage(emptyImg).scaled(thumbWidth, thumbHeight);

    for (fileIndex = 0; fileIndex < dirFileInfoList.size(); ++fileIndex) {

        // get file info
        thumbFileInfo = dirFileInfoList.at(fileIndex);
        thumbFileInfoList.append(thumbFileInfo);
        QString fPath = thumbFileInfo.filePath();

//        qDebug() << "building data model"
//                 << fileIndex << "of" << dirFileInfoList.size()
//                 << fPath;

        // add icon as first column in new row
        item = new QStandardItem();
//        item->setData(fileIndex, SortRole);
        item->setData("", Qt::DisplayRole);
//        item->setData(thumbFileInfo.fileName(), Qt::DisplayRole);
        item->setData(thumbFileInfo.filePath(), FileNameRole);
        item->setData(thumbFileInfo.absoluteFilePath(), Qt::ToolTipRole);
        item->setData("False", PickedRole);
        item->setData("", RatingRole);
        item->setData("", LabelRole);
        item->setData(QRect(), ThumbRectRole);     // define later when read
        item->setData(thumbFileInfo.path(), PathRole);
        item->setData(Qt::AlignCenter, Qt::TextAlignmentRole);
        thumbViewModel->appendRow(item);

        // add columns to model
        int row = item->index().row();

        item = new QStandardItem();
        item->setData(thumbFileInfo.fileName(), Qt::DisplayRole);
        thumbViewModel->setItem(row, NameColumn, item);

        item = new QStandardItem();
        item->setData(thumbFileInfo.suffix().toUpper(), Qt::DisplayRole);
        thumbViewModel->setItem(row, TypeColumn, item);

        item = new QStandardItem();
        item->setData(thumbFileInfo.size(), Qt::DisplayRole);
        item->setData(int(Qt::AlignRight | Qt::AlignVCenter), Qt::TextAlignmentRole);
        thumbViewModel->setItem(row, SizeColumn, item);

        item = new QStandardItem();
        item->setData(thumbFileInfo.created(), Qt::DisplayRole);
        thumbViewModel->setItem(row, CreatedColumn, item);

        item = new QStandardItem();
        item->setData(thumbFileInfo.lastModified(), Qt::DisplayRole);
        thumbViewModel->setItem(row, ModifiedColumn, item);

        item = new QStandardItem();
        item->setData(false, Qt::DisplayRole);
        thumbViewModel->setItem(row, PickedColumn, item);

        item = new QStandardItem();
        item->setData("", Qt::DisplayRole);
        item->setData(Qt::AlignCenter, Qt::TextAlignmentRole);
        thumbViewModel->setItem(row, LabelColumn, item);

        item = new QStandardItem();
        item->setData("", Qt::DisplayRole);
        item->setData(Qt::AlignCenter, Qt::TextAlignmentRole);
        thumbViewModel->setItem(row, RatingColumn, item);

        /* the rest of the data model columns are added after the metadata
        has been loaded, when the image caching is called.  See
        MW::loadImageCache and thumbView::addMetadataToModel    */

//        qDebug() << "Row =" << row << fPath;
    }
//    sortThumbs(NameColumn, false);
    updateImageList();
    return true;
}

void ThumbView::updateImageList()
{
/* The image list of file paths replicates the current sort order and filtration
of thumbViewFilter.  It is used to keep the image cache in sync with the
current state of thumbViewFilter.  This function is called when the user
changes the sort or filter.
*/
    {
    #ifdef ISDEBUG
    qDebug() << "ThumbView::updateImageList";
    #endif
    }
//    qDebug() << "ThumbView::updateImageList";
    imageFilePathList.clear();
    for(int row = 0; row < thumbViewFilter->rowCount(); row++) {
        QString fPath = thumbViewFilter->index(row, 0).data(FileNameRole).toString();
        imageFilePathList.append(fPath);
//        qDebug() << "&&&&&&&&&&&&&&&&&& updateImageList:" << fPath;
    }
}

void ThumbView::addMetadataToModel()
{
/* This function is called after the metadata for all the eligible images in
the selected folder have been cached.  The metadata is displayed in tableView,
which is created in MW.
*/
    {
    #ifdef ISDEBUG
    qDebug() << "ThumbView::addMetadataToModel";
    #endif
    }
    static QStandardItem *item;

    for(int row = 0; row < thumbViewModel->rowCount(); row++) {
        QModelIndex idx = thumbViewModel->index(row, PathColumn);
        QString fPath = idx.data(FileNameRole).toString();

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
        QString fl = metadata->getFocalLength(fPath);
        int flNum = metadata->getFocalLengthNum(fPath);
        QString title = metadata->getTitle(fPath);

        thumbViewModel->setData(thumbViewModel->index(row, MegaPixelsColumn), mp);
        thumbViewModel->setData(thumbViewModel->index(row, DimensionsColumn), dim);
        thumbViewModel->setData(thumbViewModel->index(row, ApertureColumn), apertureNum);
        thumbViewModel->setData(thumbViewModel->index(row, ApertureColumn), Qt::AlignCenter, Qt::TextAlignmentRole);
        thumbViewModel->setData(thumbViewModel->index(row, ShutterspeedColumn), ssNum);
        thumbViewModel->setData(thumbViewModel->index(row, ShutterspeedColumn), int(Qt::AlignRight | Qt::AlignVCenter), Qt::TextAlignmentRole);
        thumbViewModel->setData(thumbViewModel->index(row, ISOColumn), isoNum);
        thumbViewModel->setData(thumbViewModel->index(row, ISOColumn), Qt::AlignCenter, Qt::TextAlignmentRole);
        thumbViewModel->setData(thumbViewModel->index(row, CameraModelColumn), model);
        thumbViewModel->setData(thumbViewModel->index(row, FocalLengthColumn), flNum);
        thumbViewModel->setData(thumbViewModel->index(row, FocalLengthColumn), int(Qt::AlignRight | Qt::AlignVCenter), Qt::TextAlignmentRole);
        thumbViewModel->setData(thumbViewModel->index(row, TitleColumn), title);
    }
}

void ThumbView::setIcon(QStandardItem *item, QImage thumb, QString folderPath)
{
    {
    #ifdef ISDEBUG
    qDebug() << "ThumbView::setIcon" << folderPath;
    #endif
    }
    /* If a new folder is selected while the previous folder is being cached
     * a race condition can arise.  Make sure the item is referring to the
     * current directory.  If not, item will be a dereferenced pointer and
     * cause a segmentation fault crash */
    if (folderPath != currentViewDir) return;
    item->setIcon(QPixmap::fromImage(thumb));
}

// Used by thumbnail navigation (left, right, up, down etc)
void ThumbView::selectThumb(QModelIndex idx)
{
    {
    #ifdef ISDEBUG
    qDebug() << "ThumbView::selectThumb(index)" << idx;
    #endif
    }
    if (idx.isValid()) {
        setCurrentIndex(idx);
//        qDebug() << "Row =" << idx.row();
        thumbViewDelegate->currentIndex = idx;
        scrollTo(idx, ScrollHint::PositionAtCenter);
    }
}

void ThumbView::selectThumb(int row)
{
    {
    #ifdef ISDEBUG
    qDebug() << "ThumbView::selectThumb(row)" << row;
    #endif
    }
    // some operations assign row = -1 if not found
    if (row < 0) return;
    setFocus();
    QModelIndex idx = thumbViewFilter->index(row, 0, QModelIndex());
//    qDebug() << idx;
    setCurrentIndex(idx);
    thumbViewDelegate->currentIndex = idx;
    scrollTo(idx, ScrollHint::PositionAtCenter);
}

void ThumbView::selectThumb(QString &fName)
{
    {
    #ifdef ISDEBUG
    qDebug() << "ThumbView::selectThumb(filename)";
    #endif
    }
    QModelIndexList idxList =
        thumbViewModel->match(thumbViewModel->index(0, 0), FileNameRole, fName);
    QModelIndex idx = idxList[0];
    thumbViewDelegate->currentIndex = idx;
    QItemSelection selection(idx, idx);
    thumbViewSelection->select(selection, QItemSelectionModel::Select);
    if (idx.isValid()) scrollTo(idx, ScrollHint::PositionAtCenter);
}

void ThumbView::selectNext()
{
    {
    #ifdef ISDEBUG
    qDebug() << "ThumbView::selectNext";
    #endif
    }
    if (isSelectedItem()) selectThumb(getNextRow());
}

void ThumbView::selectPrev()
{
    {
    #ifdef ISDEBUG
    qDebug() << "ThumbView::selectPrev";
    #endif
    }
    if (isSelectedItem()) selectThumb(getPrevRow());
//    setCurrentIndex(moveCursor(MovePrevious, Qt::NoModifier));
}

void ThumbView::selectUp()
{
    {
    #ifdef ISDEBUG
    qDebug() << "ThumbView::selectUp";
    #endif
    }
    setCurrentIndex(moveCursor(QAbstractItemView::MoveUp, Qt::NoModifier));
}

void ThumbView::selectDown()
{
    {
    #ifdef ISDEBUG
    qDebug() << "ThumbView::selectDown";
    #endif
    }
    setCurrentIndex(moveCursor(QAbstractItemView::MoveDown, Qt::NoModifier));
}

void ThumbView::selectFirst()
{
    {
    #ifdef ISDEBUG
    qDebug() << "ThumbView::selectFirst";
    #endif
    }
    if (isSelectedItem()) selectThumb(0);
}

void ThumbView::selectLast()
{
    {
    #ifdef ISDEBUG
    qDebug() << "ThumbView::selectLast";
    #endif
    }
    if (isSelectedItem()) selectThumb(getLastRow());
}

void ThumbView::selectRandom()
{
    {
    #ifdef ISDEBUG
    qDebug() << "ThumbView::selectRandom";
    #endif
    }
    if (isSelectedItem()) selectThumb(getRandomRow());
}

void ThumbView::selectNextPick()
{
    {
    #ifdef ISDEBUG
    qDebug() << "ThumbView::selectNextPick";
    #endif
    }
    if (isSelectedItem()) selectThumb(getNextPick());
}

void ThumbView::selectPrevPick()
{
    {
    #ifdef ISDEBUG
    qDebug() << "ThumbView::selectPrevPick";
    #endif
    }
    if (isSelectedItem()) selectThumb(getPrevPick());
}

void ThumbView::thumbsEnlarge()
{
    {
    #ifdef ISDEBUG
    qDebug() << "ThumbView::thumbsEnlarge";
    #endif
    }
    qDebug() << "thumbsEnlarge: isGrid/thumbWidthGrid"
             << isGrid << thumbWidthGrid << thumbHeightGrid;
    if (isGrid) {
        if (thumbWidthGrid < 40) thumbWidthGrid = 40;
        if (thumbHeightGrid < 40) thumbHeightGrid = 40;
        if (thumbWidthGrid < 160 && thumbHeightGrid < 160)
        {
            thumbWidthGrid *= 1.1;
            thumbHeightGrid *= 1.1;
            if (thumbWidthGrid > 160) thumbWidthGrid = 160;
            if (thumbHeightGrid > 160) thumbHeightGrid = 160;
        }
    } else {
        if (thumbWidth < 40) thumbWidth = 40;
        if (thumbHeight < 40) thumbHeight = 40;
        if (thumbWidth < 160 && thumbHeight < 160)
        {
            thumbWidth *= 1.1;
            thumbHeight *= 1.1;
            if (thumbWidth > 160) thumbWidth = 160;
            if (thumbHeight > 160) thumbHeight = 160;
        }
    }
    setThumbParameters();
//    if (isAutoFit) thumbsFit(0);
}

void ThumbView::thumbsShrink()
{
    {
    #ifdef ISDEBUG
    qDebug() << "ThumbView::thumbsShrink";
    #endif
    }
    qDebug() << "ThumbView::thumbsShrink";
    if (isGrid) {
        if (thumbWidthGrid > 40  && thumbHeightGrid > 40) {
            thumbWidthGrid *= 0.9;
            thumbHeightGrid *= 0.9;
            if (thumbWidthGrid < 40) thumbWidthGrid = 40;
            if (thumbHeightGrid < 40) thumbHeightGrid = 40;
        }
    } else {
        if (thumbWidth > 40  && thumbHeight > 40) {
            thumbWidth *= 0.9;
            thumbHeight *= 0.9;
            if (thumbWidth < 40) thumbWidth = 40;
            if (thumbHeight < 40) thumbHeight = 40;
        }
    }
    setThumbParameters();
//    if (isAutoFit) thumbsFit();
}

void ThumbView::updateThumbRectRole(const QModelIndex index, QRect iconRect)
{
/* thumbViewDelegate triggers this to provide rect data to calc thumb mouse
click position that is then sent to imageView to zoom to the same spot
*/
    {
    #ifdef ISDEBUG
//    qDebug() << "ThumbView::updateThumbRectRole";
    #endif
    }
    thumbViewFilter->setData(index, iconRect, ThumbRectRole);
}

void ThumbView::resizeEvent(QResizeEvent *event)
{
    {
    #ifdef ISDEBUG
    qDebug() << "ThumbView::resizeEvent";
    #endif
    }
//    qDebug() << "ThumbView::resizeEvent";
    QListView::resizeEvent(event);
    if (isAutoFit) thumbsFit(Qt::NoDockWidgetArea);
}

void ThumbView::thumbsFit(Qt::DockWidgetArea area)
{
    {
    #ifdef ISDEBUG
    qDebug() << "ThumbView::thumbsFit";
    #endif
    }
    if (isGrid) {
        qDebug() << "ThumbView::thumbsFit isGrid";
        // adjust thumb width
        if (thumbWidthGrid < 40 || thumbHeightGrid < 0) {
            thumbWidthGrid = 100;
            thumbHeightGrid = 100;
        }
        int scrollWidth = qApp->style()->pixelMetric(QStyle::PM_ScrollBarExtent);
        int width = viewport()->width() - scrollWidth - 2;
        // get the thumb cell width with no padding
        int thumbCellWidth = thumbViewDelegate->getThumbCell().width() - thumbPaddingGrid * 2;
//        int thumbCellWidth = thumbWidthGrid + thumbSpacingGrid + 4;
        int rightSideGap = 99999;
        thumbPaddingGrid = 0;
        int remain;
        int padding = 0;
        bool improving;
        // increase padding until wrapping occurs
        do {
            improving = false;
            int cellWidth = thumbCellWidth + padding * 2;
            remain = width % cellWidth;
            if (remain < rightSideGap) {
                improving = true;
                rightSideGap = remain;
                thumbPaddingGrid = padding;
            }
            padding++;
        } while (improving);
        setThumbGridParameters(thumbWidthGrid, thumbHeightGrid, thumbSpacingGrid,
                               thumbPaddingGrid, labelFontSizeGrid, showThumbLabelsGrid);
        return;
    }
    // all wrapping is row wrapping
    if (isWrapping()) {
        qDebug() << "ThumbView::thumbsFit isWrapping = true";
        // adjust thumb width
        int scrollWidth = qApp->style()->pixelMetric(QStyle::PM_ScrollBarExtent);
        int width = viewport()->width() - scrollWidth - 2;
        int thumbCellWidth = thumbViewDelegate->getThumbCell().width() - thumbPadding * 2;
        int rightSideGap = 99999;
        thumbPadding = 0;
        int remain;
        int padding = 0;
        bool improving;
        do {
            improving = false;
            int cellWidth = thumbCellWidth + padding * 2;
            remain = width % cellWidth;
            if (remain < rightSideGap) {
                improving = true;
                rightSideGap = remain;
                thumbPadding = padding;
            }
            padding++;
        } while (improving);
        setThumbParameters();
    }
    // no wrapping - must be bottom or top dock area
    else if (area == Qt::BottomDockWidgetArea || area == Qt::TopDockWidgetArea){
        // horizontal scrollBar?
        int thumbCellWidth = getThumbCellSize().width() - 1;
        int maxThumbsBeforeScrollReqd = viewport()->width() / thumbCellWidth;
        int thumbsCount = thumbViewFilter->rowCount();
        int thumbDockWidth = viewport()->width();
        bool isScrollBar = thumbsCount >= maxThumbsBeforeScrollReqd;

        // set target ht based on space with/without scrollbar
        int ht = height();
        int scrollHeight = qApp->style()->pixelMetric(QStyle::PM_ScrollBarExtent);
        if (isScrollBar) ht -= scrollHeight;

        // adjust thumb height
        float aspect = thumbWidth / thumbHeight;
        int thumbSpaceHeight = thumbViewDelegate->getThumbCell().height();
        int margin = thumbSpaceHeight - thumbHeight;
        int thumbMax = getThumbSpaceMax();
        thumbSpaceHeight = ht < thumbMax ? ht : thumbMax;
        thumbHeight = thumbSpaceHeight - margin;
        thumbWidth = thumbHeight * aspect;
//        int thumbSpaceWidth = thumbWidth  + thumbSpacing * 2 + thumbPadding *2 + 8;
//        int maxThumbsBeforeScrollReqd = viewportWidth / thumbSpaceWidth;


        qDebug() << "\nThumbView::thumbsFit else\n"
                 << "***  thumbView Ht =" << ht
                 << "thumbSpace Ht =" << thumbSpaceHeight
                 << "thumbHeight =" << thumbHeight
                 << "viewport Wd =" << thumbDockWidth
                 << "thumbSpace Wd =" << thumbCellWidth
                 << "Max thumbs before scroll =" << maxThumbsBeforeScrollReqd;

        // change the thumbnail size in thumbViewDelegate
        setThumbParameters();
    }
}

void ThumbView::forceScroll(int row)
{
    {
    #ifdef ISDEBUG
    qDebug() << "ThumbView::forceScroll";
    #endif
    }

    QScrollBar *sb;
    sb = verticalScrollBar();
    int tot = thumbViewFilter->rowCount();
//    int sbVal = ((float)row / tot) * sb->maximum();
    QSize tSize = thumbViewDelegate->getThumbCell();
    QSize wSize = this->size();
    int perRow = wSize.width() / tSize.width();
    int rowsReqd = ((float)tot / perRow) + 1;
//    int sbMaxCalc = rowsReqd * tSize.height();
//    qDebug() << "sb->minimum" << sb->minimum()
//             << "sb->maximum" << sb->maximum()
//             << "sb->singleStep" << sb->singleStep()
//             << "sb->pageStep" << sb->pageStep()
//             << "sb->value" << sb->value()
//             << "tSize" << tSize
//             << "wSize" << size()
//             << "perRow" << perRow
//             << "rowsReqd" << rowsReqd
//             << "maxCalc" << sbMaxCalc
//             << "sbVal" << sbVal;
//    sb->setValue(sbVal);
//    selectThumb(row);
}

void ThumbView::updateLayout()
{
    QEvent event{QEvent::LayoutRequest};
    QListView::updateGeometries();
    QListView::event(&event);
}

void ThumbView::wheelEvent(QWheelEvent *event)
{
    {
    #ifdef ISDEBUG
    qDebug() << "ThumbView::wheelEvent";
    #endif
    }
    if (event->delta() < 0)
        verticalScrollBar()->setValue(verticalScrollBar()->value() + thumbHeight/4);
    else
        verticalScrollBar()->setValue(verticalScrollBar()->value() - thumbHeight/4);
}

void ThumbView::mousePressEvent(QMouseEvent *event)
{
    /*
    Captures the position of the mouse click within the thumbnail. This is sent
    to imageView, which pans to the same center in zoom view. This is handy
    when the user wants to view a specific part of another image that is in a
    different position than the current image.
    */
    {
    #ifdef ISDEBUG
    qDebug() << "ThumbView::mousePressEvent";
    #endif
    }
    QListView::mousePressEvent(event);
    QModelIndex idx = currentIndex();
    qDebug() << "Row =" << idx.row();
    QRect iconRect = idx.data(ThumbRectRole).toRect();
    QPoint mousePt = event->pos();
    QPoint iconPt = mousePt - iconRect.topLeft();
    float xPct = (float)iconPt.x() / iconRect.width();
    float yPct = (float)iconPt.y() / iconRect.height();
/*    qDebug() << "ThumbView::mousePressEvent"
             << "xPct =" << xPct
             << "yPct =" << yPct
             << "iconRect =" << iconRect
             << "idx =" << idx;
             */
    if (xPct >= 0 && xPct <= 1 && yPct >= 0 && yPct <=1) {
        thumbClick(xPct, yPct);    //signal used in ThumbView::mousePressEvent
    }
//    reportThumb();
}

void ThumbView::mouseDoubleClickEvent(QMouseEvent *event)
{
    {
    #ifdef ISDEBUG
    qDebug() << "ThumbView::mouseDoubleClickEvent";
    #endif
    }
    QListView::mouseDoubleClickEvent(event);
    emit displayLoupe();
    // delay reqd
    QTimer::singleShot(100, this, SLOT(delaySelectCurrentThumb()));
}

void ThumbView::delaySelectCurrentThumb()
{
    {
    #ifdef ISDEBUG
    qDebug() << "ThumbView::delaySelectCurrentThumb";
    #endif
    }
//    QKeyEvent *key = new QKeyEvent (QEvent::KeyRelease, Qt::Key_E, Qt::NoModifier, "E");
//    qApp->postEvent(mw, (QEvent *)key);
    selectThumb(currentIndex());
}

// called from MW invertSelectionAct
void ThumbView::invertSelection()
{
/* inverts/toggles which thumbs are selected.  Called from
 * MW::invertSelectionAct
*/
    {
    #ifdef ISDEBUG
    qDebug() << "ThumbView::invertSelection";
    #endif
    }
    {
    #ifdef ISDEBUG
    qDebug() << "ThumbView::invertSelection";
    #endif
    }
    {
    #ifdef ISDEBUG
    qDebug() << "ThumbView::invertSelection";
    #endif
    }
    QItemSelection toggleSelection;
    QModelIndex firstIndex = thumbViewFilter->index(0, 0);
    QModelIndex lastIndex = thumbViewFilter->index(thumbViewFilter->rowCount() - 1, 0);
    toggleSelection.select(firstIndex, lastIndex);
    selectionModel()->select(toggleSelection, QItemSelectionModel::Toggle);
}

void ThumbView::copyThumbs()
{
    {
    #ifdef ISDEBUG
    qDebug() << "ThumbView::copyThumbs";
    #endif
    }
    QModelIndexList indexesList = selectionModel()->selectedIndexes();
    if (indexesList.isEmpty()) {
        return;
    }

    QClipboard *clipboard = QGuiApplication::clipboard();
    QMimeData *mimeData = new QMimeData;
    QList<QUrl> urls;
    for (QModelIndexList::const_iterator it = indexesList.constBegin(),
         end = indexesList.constEnd();
         it != end; ++it)
    {
        urls << QUrl(it->data(FileNameRole).toString());
    }
    mimeData->setUrls(urls);
    clipboard->setMimeData(mimeData);
}

void ThumbView::startDrag(Qt::DropActions)
{
/* Drag and drop thumbs to another program.
 */
    {
    #ifdef ISDEBUG
    qDebug() << "ThumbView::startDrag";
    #endif
    }
    {
    #ifdef ISDEBUG
    qDebug() << "ThumbView::startDrag";
    #endif
    }
    QModelIndexList indexesList = selectionModel()->selectedIndexes();
    if (indexesList.isEmpty()) {
        return;
    }

    QDrag *drag = new QDrag(this);
    QMimeData *mimeData = new QMimeData;
    QList<QUrl> urls;
    for (QModelIndexList::const_iterator it = indexesList.constBegin(),
         end = indexesList.constEnd();
         it != end; ++it)
    {
        urls << QUrl(it->data(FileNameRole).toString());
    }
    mimeData->setUrls(urls);
    drag->setMimeData(mimeData);
    QPixmap pix;
    if (indexesList.count() > 1) {
        pix = QPixmap(128, 112);
        pix.fill(Qt::transparent);
        QPainter painter(&pix);
        painter.setBrush(Qt::NoBrush);
        painter.setPen(QPen(Qt::white, 2));
        int x = 0, y = 0, xMax = 0, yMax = 0;
        for (int i = 0; i < qMin(5, indexesList.count()); ++i) {
            QPixmap pix = thumbViewModel->item(indexesList.at(i).row())->icon().pixmap(72);
            if (i == 4) {
                x = (xMax - pix.width()) / 2;
                y = (yMax - pix.height()) / 2;
            }
            painter.drawPixmap(x, y, pix);
            xMax = qMax(xMax, qMin(128, x + pix.width()));
            yMax = qMax(yMax, qMin(112, y + pix.height()));
            painter.drawRect(x + 1, y + 1, qMin(126, pix.width() - 2), qMin(110, pix.height() - 2));
            x = !(x == y) * 56;
            y = !y * 40;
        }
        painter.end();
        pix = pix.copy(0, 0, xMax, yMax);
        drag->setPixmap(pix);
    } else {
        pix = thumbViewModel->item(indexesList.at(0).row())->icon().pixmap(128);
        drag->setPixmap(pix);
    }
    drag->setHotSpot(QPoint(pix.width() / 2, pix.height() / 2));
    drag->exec(Qt::CopyAction | Qt::MoveAction | Qt::LinkAction, Qt::IgnoreAction);
}
