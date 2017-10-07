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

  */

ThumbView::ThumbView(QWidget *parent, Metadata *metadata, bool iconDisplay) : QListView(parent)
{
    {
    #ifdef ISDEBUG
    qDebug() << "ThumbView::ThumbView";
    #endif
    }
    mw = parent;
    this->metadata = metadata;

    isIconDisplay = iconDisplay;
//    thumbHeight = G::thumbHeight;
    pickFilter = false;
    if (isIconDisplay) setViewMode(QListView::IconMode);
    else setViewMode(QListView::ListMode);

    setSelectionMode(QAbstractItemView::ExtendedSelection);
    if (isIconDisplay) setResizeMode(QListView::Adjust);
    this->setLayoutMode(QListView::Batched);
//    setBatchSize(2);
    setWordWrap(true);
    setDragEnabled(true);
    setEditTriggers(QAbstractItemView::NoEditTriggers);
    setUniformItemSizes(false);
    this->setContentsMargins(0,0,0,0);

    thumbViewModel = new QStandardItemModel();

    // try headers to get to work with multiple columns in a table
    /*
    thumbViewModel = new QStandardItemModel(0, TotalColumns);
    thumbViewModel->setHeaderData(0, Qt::Horizontal, QObject::tr("FileName"));
    thumbViewModel->setHeaderData(1, Qt::Horizontal, QObject::tr("Type"));
    */

//    thumbViewModel->setSortRole(ModifiedRole);    // not working

    thumbViewFilter = new QSortFilterProxyModel;
    thumbViewFilter->setSourceModel(thumbViewModel);
    thumbViewFilter->setFilterRole(PickedRole);
//    thumbViewFilter->setSortRole(ModifiedColumn);     // rgh 2017-05-31
    setModel(thumbViewFilter);

    thumbViewSelection = selectionModel();

    thumbViewDelegate = new ThumbViewDelegate(this);
    thumbViewDelegate->setThumbDimensions(thumbWidth, thumbHeight,
        thumbPadding, labelFontSize, showThumbLabels);
    setItemDelegate(thumbViewDelegate);

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

    if (isIconDisplay) emptyImg.load(":/images/no_image.png");

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
    if (isGrid) {
        setWrapping(true);
        setSpacing(thumbSpacingGrid);
        thumbViewDelegate->setThumbDimensions(thumbWidthGrid, thumbHeightGrid,
            thumbPaddingGrid, labelFontSizeGrid, showThumbLabelsGrid);
    } else {
        setWrapping(isThumbWrapWhenTopOrBottomDock && isTopOrBottomDock);
        if (!isTopOrBottomDock) setWrapping(true);
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

//    qDebug() << "thumbWidthGrid" << thumbWidthGrid
//             << "thumbHeightGrid" << thumbHeightGrid;

    setThumbParameters();
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
    if (selectionModel()->selectedIndexes().size() == 1)
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

void ThumbView::sortThumbs(bool isReverse)
{
    {
    #ifdef ISDEBUG
    qDebug() << "ThumbView::sortThumbs";
    #endif
    }
    int sortColumn = NameColumn;
    qDebug() << "sortIem" << sortColumn;
    // rgh change to use thumbViewFilter
    //    if (isReverse) thumbViewFilter->sort(0, Qt::DescendingOrder);
    //    else thumbViewFilter->sort(0, Qt::AscendingOrder);
    if (isReverse) thumbViewModel->sort(sortColumn, Qt::DescendingOrder);
    else thumbViewModel->sort(sortColumn, Qt::AscendingOrder);
    scrollTo(currentIndex(), ScrollHint::PositionAtCenter);
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
        loadPrepare();
        // exit if no images, otherwise get thumb info
        if (!initThumbs() && !includeSubfolders) return false;
    }

    // exit here if just display file list instead of icons
    if (!isIconDisplay) {
        qDebug() << "isIconDisplay =" << isIconDisplay << "Cancel load thumbView";
        return true;
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
                initThumbs();
            }
        }
    }

    if (thumbFileInfoList.size() && selectionModel()->selectedIndexes().size() == 0) {
        selectThumb(0);
        return true;
    }
    return false;   // no images found in any folder
}

void ThumbView::loadPrepare()
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
    if (!isIconDisplay) thumbHeight = 0;

    fileFilters->clear();
    foreach (const QString &str, metadata->supportedFormats)
            fileFilters->append("*." + str);
    thumbsDir->setNameFilters(*fileFilters);
    thumbsDir->setFilter(QDir::Files);
//	if (GData::showHiddenFiles) {
//		thumbsDir->setFilter(thumbsDir->filter() | QDir::Hidden);
//	}

    thumbsDir->setPath(currentViewDir);

//    QDir::SortFlags tempThumbsSortFlags = thumbsSortFlags;
//    if (tempThumbsSortFlags & QDir::Size || tempThumbsSortFlags & QDir::Time) {
//        tempThumbsSortFlags ^= QDir::Reversed;
//    }
//    thumbsDir->setSorting(tempThumbsSortFlags);
    thumbsDir->setSorting(QDir::Name);

    thumbViewModel->clear();
    thumbFileInfoList.clear();
}

bool ThumbView::initThumbs()
{
    {
    #ifdef ISDEBUG
    qDebug() << "ThumbView::initThumbs";
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
        thumbFileInfo = dirFileInfoList.at(fileIndex);
        thumbFileInfoList.append(thumbFileInfo);
        item = new QStandardItem();
//        item->setData(false, LoadedRole);
        item->setData(fileIndex, SortRole);
//        item->setData(thumbFileInfo.created(), SortRole);
        item->setData(thumbFileInfo.filePath(), FileNameRole);
        item->setData(thumbFileInfo.absoluteFilePath(), Qt::ToolTipRole);
        item->setData("False", PickedRole);
        item->setData(QRect(), ThumbRectRole);     // define later when read
        item->setData(thumbFileInfo.fileName(), Qt::DisplayRole);
        item->setData(thumbFileInfo.path(), PathRole);
        item->setData(thumbFileInfo.suffix(), FileTypeRole);
        item->setData(thumbFileInfo.size(), FileSizeRole);
        item->setData(thumbFileInfo.created(), CreatedRole);
        item->setData(thumbFileInfo.lastModified(), ModifiedRole);
        item->setData(0, LabelRole);
        item->setData(0, RatingRole);
        thumbViewModel->appendRow(item);
        // try add columns to model - not working so far
//        int row = item->index().row();
//        QModelIndex idx1 = thumbViewFilter->index(row, TypeColumn, QModelIndex());
//        thumbViewModel->setData(idx1, thumbFileInfo.suffix(), Qt::DisplayRole);
//        thumbViewModel->setData(thumbViewModel->index(fileIndex, SizeColumn), thumbFileInfo.size());
//        thumbViewModel->setData(thumbViewModel->index(fileIndex, CreatedColumn), thumbFileInfo.created());
//        thumbViewModel->setData(thumbViewModel->index(fileIndex, ModifiedColumn), thumbFileInfo.lastModified());
//        thumbViewModel->setData(thumbViewModel->index(fileIndex, PickedColumn), false);
//        thumbViewModel->setData(thumbViewModel->index(fileIndex, LabelColumn), 0);
//        thumbViewModel->setData(thumbViewModel->index(fileIndex, RatingColumn), 0);
    }
    return true;
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
//    qDebug() << "ThumbView::selectThumb(row)" << row;
    QModelIndex idx = thumbViewFilter->index(row, 0, QModelIndex());
    setCurrentIndex(idx);
    thumbViewDelegate->currentIndex = idx;
//    forceScroll(10);
    scrollTo(idx, ScrollHint::PositionAtCenter);
//    if (idx.isValid()) scrollTo(idx, ScrollHint::PositionAtCenter);

//    reportThumb();
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
        if (thumbWidthGrid == 0) thumbWidthGrid = 40;
        if (thumbHeightGrid == 0) thumbHeightGrid = 40;
        if (thumbWidthGrid < 160 && thumbHeightGrid < 160)
        {
            thumbWidthGrid *= 1.1;
            thumbHeightGrid *= 1.1;
            if (thumbWidthGrid > 160) thumbWidthGrid = 160;
            if (thumbHeightGrid > 160) thumbHeightGrid = 160;
        }
    } else {
        if (thumbWidth == 0) thumbWidth = 40;
        if (thumbHeight ==0) thumbHeight = 40;
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
    qDebug() << "ThumbView::updateThumbRectRole";
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
        qDebug() << "ThumbView::thumbsFit else";
//        int thumbArea = dockWidgetArea(thumbDock);
        // adjust thumb height
        float aspect = thumbWidth / thumbHeight;
        int scrollHeight = qApp->style()->pixelMetric(QStyle::PM_ScrollBarExtent);
        int viewportHeight = viewport()->height(); // - scrollHeight;
        int thumbSpaceHeight = thumbViewDelegate->getThumbCell().height();
        int margin = thumbSpaceHeight - thumbHeight;
        thumbSpaceHeight = viewportHeight < 160 ? viewportHeight : 160;
        thumbHeight = thumbSpaceHeight - margin;
        thumbWidth = thumbHeight * aspect;
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
