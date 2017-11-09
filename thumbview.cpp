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

ThumbView::ThumbView(QWidget *parent, DataModel *dm)
    : QListView(parent)
{
    {
    #ifdef ISDEBUG
    qDebug() << "ThumbView::ThumbView";
    #endif
    }
    mw = parent;
    this->dm = dm;

    pickFilter = false;
    setViewMode(QListView::IconMode);

    setSelectionMode(QAbstractItemView::ExtendedSelection);
    setTabKeyNavigation(true);  // not working
    setResizeMode(QListView::Adjust);
    setLayoutMode(QListView::Batched);
    setVerticalScrollMode(QAbstractItemView::ScrollPerItem);
//    setBatchSize(2);
    setWordWrap(true);
    setDragEnabled(true);
    setEditTriggers(QAbstractItemView::NoEditTriggers);
    setUniformItemSizes(false);
//    setSizeAdjustPolicy(QAbstractScrollArea::AdjustToContents);
    setMaximumHeight(100000);
    this->setContentsMargins(0,0,0,0);

    setModel(this->dm->sf);

    thumbViewSelection = selectionModel();

    thumbViewDelegate = new ThumbViewDelegate(this);
    thumbViewDelegate->setThumbDimensions(thumbWidth, thumbHeight,
        thumbPadding, labelFontSize, showThumbLabels);
    setItemDelegate(thumbViewDelegate);

    // triggers MW::fileSelectionChange
    connect(this->selectionModel(),
            SIGNAL(currentChanged(QModelIndex,QModelIndex)),
            parent, SLOT(fileSelectionChange()));

    // used to provide iconRect info to zoom to point clicked on thumb
    // in imageView
    connect(thumbViewDelegate, SIGNAL(update(QModelIndex, QRect)),
            this, SLOT(updateThumbRectRole(QModelIndex, QRect)));

/* Misc connections, emptyImg and timer
     triggers MW::fileSelectionChange
     seems to be firing twice
    connect(this->selectionModel(),
            SIGNAL(selectionChanged(QItemSelection, QItemSelection)),
            parent, SLOT(fileSelectionChange()));

     triggers MW::fileSelectionChange
    connect(this->selectionModel(),
            SIGNAL(currentChanged(QModelIndex,QModelIndex)),
            thumbViewDelegate, SLOT(onCurrentChanged(QModelIndex,QModelIndex)));

        [signal] void QItemSelectionModel::currentChanged(const QModelIndex &current, const QModelIndex &previous)

    connect(this, SIGNAL(activated(QModelIndex)),
            this, SLOT(activate(QModelIndex)));

    connect(this, SIGNAL(doubleClicked(const QModelIndex &)), parent,
            SLOT(loadImagefromThumb(const QModelIndex &)));

    thumbsDir = new QDir();
    fileFilters = new QStringList;

    emptyImg.load(":/images/no_image.png");

    QTime time = QTime::currentTime();
    */
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
//    for (int i=0; i<dm->sf->rowCount(); i++) {
//        idx = dm->sf->index(i, 0, QModelIndex());
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
    setThumbParameters();
    this->dataChanged(dm->sf->index(0, 0, QModelIndex()),
      dm->sf->index(getLastRow(), 0, QModelIndex()));
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
//    qDebug() << "ThumbView::setThumbParameters "
//             << " isGrid =" << isGrid
//             << " isFloat =" << isFloat
//             << "thumbHeight = " << thumbHeight;

    if (isGrid) {
//        setWrapping(true);
        setSpacing(thumbSpacingGrid);
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
    // if top/bottom dock resize dock height if scrollbar is/not visible

    qDebug() << "emitting updateThumbDock";
    emit updateThumbDock();
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
    qDebug() << "ThumbView::setThumbGridParameters";
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
    // rgh convert all from roles to columns
    int currThumb = currentIndex().row();
    qDebug() << "\n ***** THUMB INFO *****";
    qDebug() << "Row =" << currThumb;
    qDebug() << "LoadedRole " << G::LoadedRole << dm->item(currThumb)->data(G::LoadedRole).toBool();
    qDebug() << "FileNameRole " << G::FileNameRole << dm->item(currThumb)->data(G::FileNameRole).toString();
    qDebug() << "SortRole " << G::SortRole << dm->item(currThumb)->data(G::SortRole).toInt();
    qDebug() << "PickedRole " << G::PickedRole << dm->item(currThumb)->data(G::PickedRole).toString();
    qDebug() << "FileTypeRole " << G::FileTypeRole << dm->item(currThumb)->data(G::FileTypeRole).toString();
    qDebug() << "FileSizeRole " << G::FileSizeRole << dm->item(currThumb)->data(G::FileSizeRole).toInt();
    qDebug() << "CreatedRole " << G::CreatedRole << dm->item(currThumb)->data(G::CreatedRole).toDateTime();
    qDebug() << "ModifiedRole " << G::ModifiedRole << dm->item(currThumb)->data(G::ModifiedRole).toDateTime();

    // following crashes when columns not added
//    QModelIndex idx1 = dm->sf->index(currThumb, 1, QModelIndex());
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
    if (row == dm->sf->rowCount() - 1)
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
    return dm->sf->rowCount() - 1;
}

int ThumbView::getRandomRow()
{
    {
    #ifdef ISDEBUG
    qDebug() << "ThumbView::getRandomRow";
    #endif
    }
    return qrand() % (dm->sf->rowCount());
}

bool ThumbView::isSelectedItem()
{
    // call before getting current row or index
    {
    #ifdef ISDEBUG
    qDebug() << "ThumbView::isSelectedItem";
    #endif
    }
    return true;
    qDebug() << "selectionModel()->selectedRows().size()" << selectionModel()->selectedRows().size();
    if (selectionModel()->selectedRows().size() > 0)
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
    return currentIndex().data(G::FileNameRole).toString();
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
    for (int row = 0; row < dm->sf->rowCount(); ++row) {
        QModelIndex idx = dm->sf->index(row, 0, QModelIndex());
        if (idx.data(G::PickedRole).toString() == "true") ++pickCount;
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
    for (int row = 0; row < dm->sf->rowCount(); ++row) {
        QModelIndex idx = dm->sf->index(row, 0, QModelIndex());
        if (idx.data(G::PickedRole).toString() == "true") {
            QFileInfo fileInfo(idx.data(G::FileNameRole).toString());
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
    int rowCount = dm->sf->rowCount();
    QModelIndex idx;
    while (frwd < rowCount) {
        idx = dm->sf->index(frwd, 0, QModelIndex());
        if (idx.data(G::PickedRole).toString() == "true") return frwd;
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
//    int rowCount = dm->sf->rowCount();
    QModelIndex idx;
    while (back >= 0) {
        idx = dm->sf->index(back, 0, QModelIndex());
        if (idx.data(G::PickedRole).toString() == "true") return back;
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
    int rowCount = dm->sf->rowCount();
    QModelIndex idx;
    while (back >=0 || frwd < rowCount) {
        if (back >=0) idx = dm->sf->index(back, 0, QModelIndex());
        if (idx.data(G::PickedRole).toString() == "true") return back;
        if (frwd < rowCount) idx = dm->sf->index(frwd, 0, QModelIndex());
        if (idx.data(G::PickedRole).toString() == "true") return frwd;
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
        dm->sf->setFilterRegExp("true");   // show only picked items
    }
    else
        dm->sf->setFilterRegExp("");       // no filter - show all
}

void ThumbView::sortThumbs(int sortColumn, bool isReverse)
{
    {
    #ifdef ISDEBUG
    qDebug() << "ThumbView::sortThumbs";
    #endif
    }
    if (isReverse) dm->sf->sort(sortColumn, Qt::DescendingOrder);
    else dm->sf->sort(sortColumn, Qt::AscendingOrder);

    scrollTo(currentIndex(), ScrollHint::PositionAtCenter);
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
        SelectedThumbsPaths << indexesList[tn].data(G::FileNameRole).toString();
    }
    return SelectedThumbsPaths;
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
    if (folderPath != dm->currentFolderPath) return;
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
//        scrollTo(idx, ScrollHint::PositionAtCenter);
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
    QModelIndex idx = dm->sf->index(row, 0, QModelIndex());
    selectThumb(idx);
}

void ThumbView::selectThumb(QString &fName)
{
    {
    #ifdef ISDEBUG
    qDebug() << "ThumbView::selectThumb(filename)";
    #endif
    }
    QModelIndexList idxList = dm->match(dm->index(0, 0), G::FileNameRole, fName);
    QModelIndex idx = idxList[0];
//    thumbViewDelegate->currentIndex = idx;
    QItemSelection selection(idx, idx);
    thumbViewSelection->select(selection, QItemSelectionModel::Select);
    qDebug() << "scrollto: ThumbView::selectThumb(filename)" << fName;
    if (idx.isValid()) scrollTo(idx, ScrollHint::PositionAtCenter);
}

void ThumbView::selectNext()
{
    {
    #ifdef ISDEBUG
    qDebug() << "ThumbView::selectNext";
    #endif
    }
    selectThumb(getNextRow());
//    if (isSelectedItem()) selectThumb(getNextRow());
}

void ThumbView::selectPrev()
{
    {
    #ifdef ISDEBUG
    qDebug() << "ThumbView::selectPrev";
    #endif
    }
    selectThumb(getPrevRow());
//    if (isSelectedItem()) selectThumb(getPrevRow());
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
    selectThumb(0);
//    if (isSelectedItem()) selectThumb(0);
}

void ThumbView::selectLast()
{
    {
    #ifdef ISDEBUG
    qDebug() << "ThumbView::selectLast";
    #endif
    }
    selectThumb(getLastRow());
//    if (isSelectedItem()) selectThumb(getLastRow());
}

void ThumbView::selectRandom()
{
    {
    #ifdef ISDEBUG
    qDebug() << "ThumbView::selectRandom";
    #endif
    }
    selectThumb(getRandomRow());
//    if (isSelectedItem()) selectThumb(getRandomRow());
}

void ThumbView::selectNextPick()
{
    {
    #ifdef ISDEBUG
    qDebug() << "ThumbView::selectNextPick";
    #endif
    }
    selectThumb(getNextPick());
//    if (isSelectedItem()) selectThumb(getNextPick());
}

void ThumbView::selectPrevPick()
{
    {
    #ifdef ISDEBUG
    qDebug() << "ThumbView::selectPrevPick";
    #endif
    }
    selectThumb(getPrevPick());
//    if (isSelectedItem()) selectThumb(getPrevPick());
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
    dm->sf->setData(index, iconRect, G::ThumbRectRole);
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
        int thumbsCount = dm->sf->rowCount();
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


//        qDebug() << "\nThumbView::thumbsFit else\n"
//                 << "***  thumbView Ht =" << ht
//                 << "thumbSpace Ht =" << thumbSpaceHeight
//                 << "thumbHeight =" << thumbHeight
//                 << "viewport Wd =" << thumbDockWidth
//                 << "thumbSpace Wd =" << thumbCellWidth
//                 << "Max thumbs before scroll =" << maxThumbsBeforeScrollReqd;

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
    int tot = dm->sf->rowCount();
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
    if (event->modifiers() == Qt::NoModifier) {
        // capture position for imageView zoom/pan
        QModelIndex idx = currentIndex();
        qDebug() << "Row =" << idx.row();
        QRect iconRect = idx.data(G::ThumbRectRole).toRect();
        QPoint mousePt = event->pos();
        QPoint iconPt = mousePt - iconRect.topLeft();
        float xPct = (float)iconPt.x() / iconRect.width();
        float yPct = (float)iconPt.y() / iconRect.height();
        /*qDebug() << "ThumbView::mousePressEvent"
                 << "xPct =" << xPct
                 << "yPct =" << yPct
                 << "iconRect =" << iconRect
                 << "idx =" << idx;
                 */
        if (xPct >= 0 && xPct <= 1 && yPct >= 0 && yPct <=1) {
            thumbClick(xPct, yPct);    //signal used in ThumbView::mousePressEvent
        }
    }
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
    QModelIndex firstIndex = dm->sf->index(0, 0);
    QModelIndex lastIndex = dm->sf->index(dm->sf->rowCount() - 1, 0);
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
        urls << QUrl(it->data(G::FileNameRole).toString());
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
        urls << QUrl(it->data(G::FileNameRole).toString());
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
            QPixmap pix = dm->item(indexesList.at(i).row())->icon().pixmap(72);
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
        pix = dm->item(indexesList.at(0).row())->icon().pixmap(128);
        drag->setPixmap(pix);
    }
    drag->setHotSpot(QPoint(pix.width() / 2, pix.height() / 2));
    drag->exec(Qt::CopyAction | Qt::MoveAction | Qt::LinkAction, Qt::IgnoreAction);
}
