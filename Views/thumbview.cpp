#include "Views/thumbview.h"
#include "mainwindow.h"

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
        to accomodate the new thumb height.

    Thumb resize to fit in dock:

        This also only occurs when the thumbDock is located in the top or bottom
        dock locations and wrap thumbs is not checked in prefDlg.  If the
        thumbDock horizontal splitter is dragged to make the thumbDock taller
        or shorter, within the minimum and maximum thumbView heights, the thumb
        sizes are adjusted to fit, factoring in the need for a scrollbar.

        This is triggered by MW::eventFilter() and effectuated in thumbsFit().

*/

// Declare here as #include "mainwindow" causes errors if put in header
MW *mw;

ThumbView::ThumbView(QWidget *parent, DataModel *dm, QString objName)
    : QListView(parent)
{
    {
    #ifdef ISDEBUG
    qDebug() << "ThumbView::ThumbView";
    #endif
    }
    this->dm = dm;
    setObjectName(objName);

    // this works because ThumbView is a friend class of MW.  It is used in the
    // event filter to access the thumbDock
    mw = qobject_cast<MW*>(parent);

    pickFilter = false;

    setViewMode(QListView::IconMode);
    setSelectionMode(QAbstractItemView::ExtendedSelection);
    setTabKeyNavigation(true);  // not working
    setResizeMode(QListView::Adjust);
    setLayoutMode(QListView::Batched);
//    setVerticalScrollMode(QAbstractItemView::ScrollPerItem);
    setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    verticalScrollBar()->setObjectName("VerticalScrollBar");
    horizontalScrollBar()->setObjectName("HorizontalScrollBar");
    setWordWrap(true);
    setDragEnabled(true);
    setEditTriggers(QAbstractItemView::NoEditTriggers);
    setSelectionBehavior(QAbstractItemView::SelectRows);
    setUniformItemSizes(false);
    setMaximumHeight(100000);
    setContentsMargins(0,0,0,0);

    horizontalScrollBar()->setObjectName("ThumbViewHorizontalScrollBar");
    verticalScrollBar()->setObjectName("ThumbViewVerticalScrollBar");
//    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
//    setSizeAdjustPolicy(QAbstractScrollArea::AdjustToContents);
//    setBatchSize(2);

    setModel(this->dm->sf);

    thumbViewDelegate = new ThumbViewDelegate(this);
    thumbViewDelegate->setThumbDimensions(thumbWidth, thumbHeight,
        thumbPadding, labelFontSize, showThumbLabels);
    setItemDelegate(thumbViewDelegate);

    // used to provide iconRect info to zoom to point clicked on thumb
    // in imageView
    connect(thumbViewDelegate, SIGNAL(update(QModelIndex, QRect)),
            this, SLOT(updateThumbRectRole(QModelIndex, QRect)));
}

void ThumbView::reportThumbs()
{
    /*
    QModelIndex idx;
    qDebug() << "List all thumbs";
    for (int i=0; i<dm->sf->rowCount(); i++) {
        idx = dm->sf->index(i, 0, QModelIndex());
        qDebug() << i << idx.data(FileNameRole).toString();
    }
    */
}

void ThumbView::refreshThumbs() {
    {
    #ifdef ISDEBUG
    qDebug() << "ThumbView::refreshThumbs";
    #endif
    }
    dataChanged(dm->sf->index(0, 0), dm->sf->index(getLastRow(), 0));
//    emit updateThumbDockHeight();
}

void ThumbView::setThumbParameters()
{
/*
When thumb parameters (height, width, padding, fontsize, showLabels) are
changed this function is called from:

    MW::invokeWorkspace
    MW::defaultWorkspace
    MW::loupeDisplay
    MW::gridDisplay
    MW::tableDisplay
    MW::CompareDisplay
    prefdlg signal updateThumbParameters
    prefdlg signal updateGridParameters

There are two instances of ThumbView: thumbView and gridView. They are
distinguished by their assigned object names: "Thumbnails" and "Grid".

Both instances update the delegate. If it is the "Thumbnails" thumbView
instance then the thumb dock is signalled so it can be resized to fit the
possibly altered thumbnail dimensions.
*/
    {
    #ifdef ISDEBUG
    qDebug() << "ThumbView::setThumbParameters";
    #endif
    }
    setSpacing(thumbSpacing);
    thumbViewDelegate->setThumbDimensions(thumbWidth, thumbHeight,
        thumbPadding, labelFontSize, showThumbLabels);
    if(objectName() == "Thumbnails") {
        if (!mw->thumbDock->isFloating())
            emit updateThumbDockHeight();
    }
}

void ThumbView::setThumbParameters(int _thumbWidth, int _thumbHeight,
        int _thumbSpacing, int _thumbPadding, int _labelFontSize,
        bool _showThumbLabels, bool _wrapThumbs)
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
    wrapThumbs = _wrapThumbs;
    setThumbParameters();
}

int ThumbView::getThumbSpaceMin()
{
    {
    #ifdef ISDEBUG
    qDebug() << "ThumbView::getThumbSpaceMin";
    #endif
    }
    return 40 + thumbSpacing * 2 + thumbPadding *2 + 8;
}

int ThumbView::getThumbSpaceMax()
{
    {
    #ifdef ISDEBUG
    qDebug() << "ThumbView::getThumbSpaceMax";
    #endif
    }
    return 160 + thumbSpacing * 2 + thumbPadding *2 + 8;
}

QSize ThumbView::getThumbCellSize()
{
    {
    #ifdef ISDEBUG
    qDebug() << "ThumbView::getThumbCellSize";
    #endif
    }
//    setThumbParameters(false);    // reqd?  rgh
    return thumbViewDelegate->getThumbCell();
}

int ThumbView::getThumbSpaceWidth(int thumbSpaceHeight)
{
/*
The thumbSpace is the total cell occupied be the thumbnail, including the outer
margin, like the box model.  The width is calculated by taking the thumbSpace
height, determining the thumbnail height, calculating the thumbnail width from
the aspact ratio and adding the margin back to get the space width.

This function is used when the thumbView wrapping = false and the thumbDock
changes height to determine whether a scrollbar is required.
*/
    {
    #ifdef ISDEBUG
    qDebug() << "ThumbView::getThumbSpaceWidth";
    #endif
    }
    float aspect = thumbWidth / thumbHeight;
    qDebug() << "aspect =" << aspect;
    // Difference between thumbSpace and thumbHeight
    int margin = thumbViewDelegate->getThumbCell().height() - thumbHeight;
    int newThumbHeight = thumbSpaceHeight - margin;
    int newThumbWidth = newThumbHeight * aspect;
    return newThumbWidth + margin - 1;
}

int ThumbView::getScrollThreshold(int thumbSpaceHeight)
{
    qDebug() << "getScrollThreshold  viewport()->width()"
             << viewport()->width()
             << "getThumbSpaceWidth"
             << getThumbSpaceWidth(thumbSpaceHeight);
    return viewport()->width() / getThumbSpaceWidth(thumbSpaceHeight);
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
    qDebug() << "FileNameRole " << G::FilePathRole << dm->item(currThumb)->data(G::FilePathRole).toString();
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

int ThumbView::getFirstVisible()
{
/*
Return the datamodel row for the first thumb visible in the thumbView. This is
used to prioritize thumbnail loading when a new folder is selected to show the
user thumbnails as quickly as possible, instead of waiting for all the
thumbnails to be generated. This can take a few seconds if there are thousands
of images in the selected folder.
*/
    {
    #ifdef ISDEBUG
    qDebug() << "ThumbView::getFirstVisible";
    #endif
    }
    QRect thumbViewRect = viewport()->rect();
    for (int row = 0; row < dm->sf->rowCount(); ++row) {
        if (visualRect(dm->sf->index(row, 0)).intersects(thumbViewRect)) {
            QRect thumbRect = visualRect(dm->sf->index(row, 0));
//            qDebug() << "ThumbView::getFirstVisible  row =" << row
//                     << "thumbRect =" << thumbRect
//                     << "thumbViewRect =" << thumbViewRect;
            return row;
        }
    }
    return -1;
}

int ThumbView::getLastVisible()
{
/*
Return the datamodel row for the last thumb visible in the thumbView. This is
used to prioritize thumbnail loading when a new folder is selected to show the
user thumbnails as quickly as possible, instead of waiting for all the
thumbnails to be generated. This can take a few seconds if there are thousands
of images in the selected folder.
*/
    {
    #ifdef ISDEBUG
    qDebug() << "ThumbView::getLastVisible";
    #endif
    }
    QRect thumbViewRect = viewport()->rect();
    for (int row = dm->sf->rowCount() - 1; row >= 0; --row) {
        if (visualRect(dm->sf->index(row, 0)).intersects(thumbViewRect)) {
            return row;
        }
    }
    return -1;
}

// not used but might be useful
QString ThumbView::getCurrentFilename()
{
    {
    #ifdef ISDEBUG
    qDebug() << "ThumbView::getCurrentFilename";
    #endif
    }
    return currentIndex().data(G::FilePathRole).toString();
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
    for (int row = 0; row < dm->sf->rowCount(); ++row) {
        QModelIndex idx = dm->sf->index(row, G::PickColumn);
        if (idx.data(Qt::EditRole).toString() == "true") return true;
    }
    return false;
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
        QModelIndex idx = dm->sf->index(row, G::PickColumn);
        if (idx.data(Qt::EditRole).toString() == "true") {
            QModelIndex pathIdx = dm->sf->index(row, 0);
            QString fPath = pathIdx.data(G::FilePathRole).toString();
            QFileInfo fileInfo(fPath);
            qDebug() << fPath;
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
        idx = dm->sf->index(frwd, G::PickColumn);
        if (idx.data(Qt::EditRole).toString() == "true") return frwd;
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
        idx = dm->sf->index(back, G::PickColumn);
        if (idx.data(Qt::EditRole).toString() == "true") return back;
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
        if (back >=0) idx = dm->sf->index(back, G::PickColumn);
        if (idx.data(Qt::EditRole).toString() == "true") return back;
        if (frwd < rowCount) idx = dm->sf->index(frwd, G::PickColumn);
        if (idx.data(Qt::EditRole).toString() == "true") return frwd;
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

//        int row = getNearestPick();
//        selectThumb(row);
//        dm->sf->setFilterRegExp("true");   // show only picked items
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
        SelectedThumbsPaths << indexesList[tn].data(G::FilePathRole).toString();
    }
    return SelectedThumbsPaths;
}

bool ThumbView::isThumb(int row)
{
    return dm->sf->index(row, 0).data(Qt::DecorationRole).isNull();
}

void ThumbView::setIcon(int row, QImage thumb)
{
/*
This slot is signalled from metadataCacheThread to set the thumbView icon.

If a new folder is selected while the previous folder is being cached a race
condition can arise. Make sure the item is referring to the current directory.
If not, item will be a dereferenced pointer and cause a segmentation fault
crash.
*/
    {
    #ifdef ISDEBUG
    qDebug() << "ThumbView::setIcon" << row;
    #endif
    }
    QStandardItem *item = new QStandardItem;
    QModelIndex idx = dm->index(row, 0, QModelIndex());
    if (!idx.isValid()) return;
    item = dm->itemFromIndex(idx);
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
        G::lastThumbChangeEvent = "KeyStroke";    // either KeyStroke or MouseClick
        setCurrentIndex(idx);
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
    QModelIndexList idxList = dm->sf->match(dm->sf->index(0, 0), G::FilePathRole, fName);
    QModelIndex idx = idxList[0];
    qDebug() << "selectThumb  idx.row()" << idx.row();
    if(idx.isValid()) selectThumb(idx);
}

void ThumbView::selectNext()
{
    {
    #ifdef ISDEBUG
    qDebug() << "ThumbView::selectNext";
    #endif
    }
    if(G::mode == "Compare") return;
    selectThumb(getNextRow());
}

void ThumbView::selectPrev()
{
    {
    #ifdef ISDEBUG
    qDebug() << "ThumbView::selectPrev";
    #endif
    }
    if(G::mode == "Compare") return;
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
    if (G::mode == "Table" || !isWrapping()) selectPrev();
    else setCurrentIndex(moveCursor(QAbstractItemView::MoveUp, Qt::NoModifier));
}

void ThumbView::selectDown()
{
    {
    #ifdef ISDEBUG
    qDebug() << "ThumbView::selectDown";
    #endif
    }
    if (G::mode == "Table" || !isWrapping()) selectNext();
    else setCurrentIndex(moveCursor(QAbstractItemView::MoveDown, Qt::NoModifier));
}

void ThumbView::selectFirst()
{
    {
    #ifdef ISDEBUG
    qDebug() << "ThumbView::selectFirst";
    #endif
    }
    selectThumb(0);
}

void ThumbView::selectLast()
{
    {
    #ifdef ISDEBUG
    qDebug() << "ThumbView::selectLast";
    #endif
    }
    selectThumb(getLastRow());
}

void ThumbView::selectRandom()
{
    {
    #ifdef ISDEBUG
    qDebug() << "ThumbView::selectRandom";
    #endif
    }
    selectThumb(getRandomRow());
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
    if (thumbWidth < 40) thumbWidth = 40;
    if (thumbHeight < 40) thumbHeight = 40;
    if (thumbWidth < 160 && thumbHeight < 160)
    {
        thumbWidth *= 1.1;
        thumbHeight *= 1.1;
        if (thumbWidth > 160) thumbWidth = 160;
        if (thumbHeight > 160) thumbHeight = 160;
    }
    qDebug() << "🔎🔎🔎 Calling setThumbParameters from ThumbView::thumbsEnlarge  thumbHeight =" << thumbHeight;
//    qDebug() << "Calling setThumbParameters from ThumbView::thumbsEnlarge thumbWidth" << thumbWidth ;
    setThumbParameters();
    scrollTo(currentIndex(), ScrollHint::PositionAtCenter);
}

void ThumbView::thumbsShrink()
{
    {
    #ifdef ISDEBUG
    qDebug() << "ThumbView::thumbsShrink";
    #endif
    }
    if (thumbWidth > 40  && thumbHeight > 40) {
        thumbWidth *= 0.9;
        thumbHeight *= 0.9;
        if (thumbWidth < 40) thumbWidth = 40;
        if (thumbHeight < 40) thumbHeight = 40;
    }
    qDebug() << "🔎🔎🔎 Calling setThumbParameters from ThumbView::thumbsShring  thumbHeight =" << thumbHeight;
//    qDebug() << "Calling setThumbParameters from ThumbView::thumbsShrink thumbWidth" << thumbWidth ;
    setThumbParameters();
    scrollTo(currentIndex(), ScrollHint::PositionAtCenter);
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
}

void ThumbView::thumbsFit(Qt::DockWidgetArea area)
{
    {
    #ifdef ISDEBUG
    qDebug() << "ThumbView::thumbsFit";
    #endif
    }
    qDebug() << "🔎🔎🔎 Entering ThumbView::thumbsFit    thumbHeight = " << thumbHeight;
    if (G::mode == "Grid") {
        return;

        // adjust thumb width
        if (thumbWidth < 40 || thumbHeight < 0) {
            thumbWidth = 100;
            thumbHeight = 100;
        }
        int scrollWidth = 12;
//        int scrollWidth = qApp->style()->pixelMetric(QStyle::PM_ScrollBarExtent);
        int width = viewport()->width() - scrollWidth - 2;
        // get the thumb cell width with no padding
        int thumbCellWidth = thumbViewDelegate->getThumbCell().width() - thumbPadding * 2;
//        int thumbCellWidth = thumbWidth + thumbSpacing + 4;
        int rightSideGap = 99999;
        thumbPadding = 0;
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
                thumbPadding = padding;
            }
            padding++;
        } while (improving);
//        qDebug() << "Calling setThumbParameters from ThumbView::thumbsFit thumbWidth" << thumbWidth ;
        setThumbParameters(thumbWidth, thumbHeight, thumbSpacing,
                           thumbPadding, labelFontSize, showThumbLabels, wrapThumbs);
        return;
    }
    // all wrapping is row wrapping
    if (isWrapping()) {
        return;
        qDebug() << "ThumbView::thumbsFit isWrapping = true";
        // adjust thumb width
        int scrollWidth = 12;
//        int scrollWidth = qApp->style()->pixelMetric(QStyle::PM_ScrollBarExtent);
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
//        qDebug() << "Calling setThumbParameters from ThumbView::thumbsFit thumbWidth" << thumbWidth ;
//        setThumbParameters();
    }
    // no wrapping - must be bottom or top dock area
    else if (area == Qt::BottomDockWidgetArea || area == Qt::TopDockWidgetArea
             || !wrapThumbs){
        // set target ht based on space with scrollbar (always on)
        int ht = height();
        int scrollHeight = 12;
//        int scrollHeight = qApp->style()->pixelMetric(QStyle::PM_ScrollBarExtent);
        ht -= scrollHeight;

        // adjust thumb height
        float aspect = thumbWidth / thumbHeight;
        // get the current thumb cell
        int thumbSpaceHeight = thumbViewDelegate->getThumbCell().height();
        // margin = nonthumb space is used to rebuild cell after thumb resize to fit
        int margin = thumbSpaceHeight - thumbHeight;
        int thumbMax = getThumbSpaceMax();
        thumbSpaceHeight = ht < thumbMax ? ht : thumbMax;
        thumbHeight = thumbSpaceHeight - margin;
        thumbWidth = thumbHeight * aspect;

        qDebug() << "🔎🔎🔎 ThumbView::thumbsFit    thumbHeight = " << thumbHeight;

        // change the thumbnail size in thumbViewDelegate
//        qDebug() << "Calling setThumbParameters from ThumbView::thumbsFit thumbWidth" << thumbWidth ;
        setSpacing(thumbSpacing);
        thumbViewDelegate->setThumbDimensions(thumbWidth, thumbHeight,
            thumbPadding, labelFontSize, showThumbLabels);
    }
}

void ThumbView::thumbsFitTopOrBottom()
{
/*
Called by eventFilter when a thumbDock resize event occurs triggered by the
user resizing the thumbDock. Adjust the size of the thumbs to fit the new
thumbDock height.

Note that MW events have been sent to thumbView by installEventFilter in MW
constructor.

For thumbSpace anatomy (see ThumbViewDelegate)
*/
    {
    #ifdef ISDEBUG
    qDebug() << "ThumbView::thumbsFitTopOrBottom";
    #endif
    }
    float aspect = (float)thumbWidth / thumbHeight;
    // viewport available height
    int netViewportHt = height() - G::scrollBarThickness;
    int hMax = getThumbSpaceMax();
    int hMin = getThumbSpaceMin();
    // restrict thumbSpace within limits
    int newThumbSpaceHt = netViewportHt > hMax ? hMax : netViewportHt;
    newThumbSpaceHt = newThumbSpaceHt < hMin ? hMin : newThumbSpaceHt;
    // derive new thumbsize from new thumbSpace
    thumbHeight = thumbViewDelegate->getThumbHeightFromAvailHeight(newThumbSpaceHt);
    // make sure within range (should be from thumbSpace check but just to be sure)
    thumbHeight = thumbHeight > 160 ? 160 : thumbHeight;
    thumbHeight = thumbHeight < 40 ? 40 : thumbHeight;
    thumbWidth = thumbHeight * aspect;
    // check thumbWidth within range
    if(thumbWidth > 160) {
        thumbWidth = 160;
        thumbHeight = 160 / aspect;
    }
    setSpacing(thumbSpacing);
    thumbViewDelegate->setThumbDimensions(thumbWidth, thumbHeight,
        thumbPadding, labelFontSize, showThumbLabels);
}

void ThumbView::updateLayout()
{
    QEvent event{QEvent::LayoutRequest};
    QListView::updateGeometries();
    QListView::event(&event);
}

void ThumbView::scrollToCurrent(int row)
{
/*
This is called from ThumbView::eventFilter after the eventFilter intercepts the
last scrollbar paint event for the newly visible ThumbView.  ThumbView is newly
visible because in a mode change in MW (ie from Grid to Loupe) thumbView was
hidden in Grid but visible in Loupe.  Widgets will not respond while hidden so
we must wait until ThumbView is visible and completely repainted.  It takes a
while for the scrollbars to finish painting, so when that is done, we want to
scroll to the current position.

It is also called from MW::delayScroll, which in turn, is called by
MW::fileSelectionChange when a new image is selected and the selection was
triggered by a mouse click and MW::mouseClickScroll == true.
*/
    {
    #ifdef ISDEBUG
    qDebug() << "ThumbView::scrollToCurrent";
    #endif
    }

    if (!isWrapping())
        horizontalScrollBar()->setValue(getHorizontalScrollBarOffset(row));
    else
        verticalScrollBar()->setValue(getVerticalScrollBarOffset(row));

    readyToScroll = false;
}

int ThumbView::getHorizontalScrollBarOffset(int row)
{
    int pageWidth = viewport()->width();
    int thumbWidth = getThumbCellSize().width();

    if (pageWidth < 40 || thumbWidth < 40)
        return 0;

    float thumbsPerPage = (double)pageWidth / thumbWidth;
    int n = dm->sf->rowCount();
    float pages = float(n) / thumbsPerPage - 1;
    float hMax = pages * pageWidth;

    int startNoScrollItems = pageWidth / thumbWidth / 2;
    float fractpart = fmodf (thumbsPerPage / 2 , 1.0);
    int thumbCntrOffset = (0.5 - fractpart) * thumbWidth;
    int scrollOffset = (row - startNoScrollItems) * thumbWidth + thumbCntrOffset;
    if (scrollOffset < 0) scrollOffset = 0;
    if (scrollOffset > hMax) scrollOffset = hMax;
    /*
    qDebug() << objectName()
             << "Row =" << mw->currentRow
             << "horizontalScrollBarMax Qt vs Me"
             << horizontalScrollBar()->maximum()
             << hMax
             << "Total Pages =" << pages
             << "Item Width =" << thumbWidth
             << "ViewPort Width =" << pageWidth
             << "startNoScrollItems =" << startNoScrollItems
             << "thumbCntrOffset" << thumbCntrOffset
             << "Current/Estimate Pos = " << horizontalScrollBar()->value()
             << scrollOffset;
*/
    return scrollOffset;
}

int ThumbView::getVerticalScrollBarOffset(int row)
{
    int pageWidth = viewport()->width();
    int pageHeight = viewport()->height();
    int thumbCellWidth = getThumbCellSize().width();
    int thumbCellHeight = getThumbCellSize().height();

    if (pageWidth < 40 ||pageHeight < 40 || thumbCellWidth < 40 || thumbCellHeight < 40)
        return 0;

    float thumbsPerPage = pageWidth / thumbCellWidth * (float)pageHeight / thumbCellHeight;
    float thumbRowsPerPage = (float)pageHeight / thumbCellHeight;
    int n = dm->sf->rowCount();
//    qDebug() << "thumbsPerPage" << thumbsPerPage;
    float pages = (float(n) / thumbsPerPage) - 1;
    int vMax = pages * pageWidth;

    int thumbRow = row / (pageWidth / thumbCellWidth);
    int startNoScrollItems = thumbRowsPerPage / 2;
    float fractpart = fmodf (thumbRowsPerPage / 2 , 1.0);
    int thumbCntrOffset = (0.5 - fractpart) * thumbCellHeight;
    int scrollOffset = (thumbRow - startNoScrollItems) * thumbCellHeight + thumbCntrOffset;
    if (scrollOffset < 0) scrollOffset = 0;
    if (scrollOffset > vMax) scrollOffset = vMax;
    /*
    qDebug() << objectName()
             << "Row =" << mw->currentRow
             << "thumbRow" << thumbRow
             << "verticalScrollBarMax Qt vs Me"
             << verticalScrollBar()->maximum()
             << vMax
             << "Total Pages =" << pages
             << "Item Height =" << thumbCellHeight
             << "ViewPort Height =" << pageHeight
             << "startNoScrollItems =" << startNoScrollItems
             << "thumbCntrOffset" << thumbCntrOffset
             << "Current/Estimate Pos = " << verticalScrollBar()->value()
             << scrollOffset;
*/
    return scrollOffset;
}
int ThumbView::getHorizontalScrollBarMax()
{
/*

*/
    {
    #ifdef ISDEBUG
    qDebug() << "humbView::getHorizontalScrollBarMax()";
    #endif
    }
    int pageWidth = viewport()->width();
    int thumbWidth = getThumbCellSize().width();
    float thumbsPerPage = (double)pageWidth / thumbWidth;
    int n = dm->sf->rowCount();
    float pages = float(n) / thumbsPerPage - 1;
    float hMax = pages * pageWidth;
    return hMax;
}

int ThumbView::getVerticalScrollBarMax()
{
/*

*/
    {
    #ifdef ISDEBUG
    qDebug() << "ThumbView::getVerticalScrollBarMax()";
    #endif
    }
    int pageWidth = viewport()->width();
    int pageHeight = viewport()->height();
    int thumbCellWidth = getThumbCellSize().width();
    int thumbCellHeight = getThumbCellSize().height();
    float thumbsPerPage = pageWidth / thumbCellWidth * (float)pageHeight / thumbCellHeight;
    int n = dm->sf->rowCount();
    float pages = float(n) / thumbsPerPage - 1;
    int vMax = pages * pageHeight;
    /*
    qDebug() << objectName()
             << "Row =" << mw->currentRow
             << "verticalScrollBarMax Qt vs Me"
             << verticalScrollBar()->maximum()
             << vMax;
             */
    return vMax;
}

void ThumbView::wheelEvent(QWheelEvent *event)
{
    {
    #ifdef ISDEBUG
    qDebug() << "ThumbView::wheelEvent";
    #endif
    }
    QListView::wheelEvent(event);
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
//    qDebug() << "🔎🔎🔎 ThumbView::mousePressEvent ";
    G::lastThumbChangeEvent = "MouseClick";    // either KeyStroke or MouseClick
    QListView::mousePressEvent(event);

    // capture mouse click position for imageView zoom/pan
    if (event->modifiers() == Qt::NoModifier) {
        // reqd for thumb resizing
        if (event->button() == Qt::LeftButton) isLeftMouseBtnPressed = true;

        QModelIndex idx = currentIndex();
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
    // 2nd call to QListView to capture cmd clicks (not sure why req'd)
    QListView::mousePressEvent(event);
}

void ThumbView::mouseMoveEvent(QMouseEvent *event)
{
//    qDebug() << "🔎🔎🔎 ThumbView::mouseMoveEvent event =" << event;
    if (isLeftMouseBtnPressed) isMouseDrag = true;
    QListView::mouseMoveEvent(event);
}

void ThumbView::mouseReleaseEvent(QMouseEvent *event)
{
//    qDebug() << "🔎🔎🔎 ThumbView::mouseReleaseEvent ";
    isLeftMouseBtnPressed = false;
    isMouseDrag = false;
    QListView::mouseReleaseEvent(event);
}

//QModelIndex ThumbView::moveCursor(QAbstractItemView::CursorAction cursorAction,
//                                Qt::KeyboardModifiers modifiers)
//{
//    QModelIndex idx = QAbstractItemView::moveCursor(cursorAction, modifiers);
//    setCurrentIndex(idx);

//}

void ThumbView::mouseDoubleClickEvent(QMouseEvent *event)
{
/*
Show the image in loupe view.  Scroll the thumbView or gridView to position at
center.
*/
    {
    #ifdef ISDEBUG
    qDebug() << "ThumbView::mouseDoubleClickEvent";
    #endif
    }
    QListView::mouseDoubleClickEvent(event);
    // do not displayLoupe if already displayed
    if (G::mode != "Loupe") {
        emit displayLoupe();
    }
    scrollToCurrent(currentIndex().row());
}

//void ThumbView::delaySelectCurrentThumb()
//{
///*
//Called by mouseDoubleClickEvent.
//*/
//    {
//    #ifdef ISDEBUG
//    qDebug() << "ThumbView::delaySelectCurrentThumb";
//    #endif
//    }
//    selectThumb(currentIndex());
//}

void ThumbView::invertSelection()
{
/*
Inverts/toggles which thumbs are selected.  Called from MW::invertSelectionAct
*/
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
//    if(selectedIndexes().count() > 0) {
//        QModelIndex idx = selectedIndexes().at(0);
//        selectionModel()->setCurrentIndex(idx, QItemSelectionModel::Select);
//    }
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
        urls << QUrl(it->data(G::FilePathRole).toString());
    }
    mimeData->setUrls(urls);
    clipboard->setMimeData(mimeData);
}

void ThumbView::startDrag(Qt::DropActions)
{
/*
Drag and drop thumbs to another program.
*/
    {
    #ifdef ISDEBUG
    qDebug() << "ThumbView::startDrag";
    #endif
    }

    QModelIndexList selection = selectionModel()->selectedRows();
    if (selection.isEmpty()) {
        return;
    }

    QDrag *drag = new QDrag(this);
    QMimeData *mimeData = new QMimeData;
    QList<QUrl> urls;

    for(int i = 0; i < selection.count(); ++i) {
        QString fPath = selection.at(i).data(G::FilePathRole).toString();
        urls << QUrl::fromLocalFile(fPath);
    }
    qDebug() << urls;

    mimeData->setUrls(urls);
    drag->setMimeData(mimeData);
    QPixmap pix;
    if (selection.count() > 1) {
        pix = QPixmap(128, 112);
        pix.fill(Qt::transparent);
        QPainter painter(&pix);
        painter.setBrush(Qt::NoBrush);
        painter.setPen(QPen(Qt::white, 2));
        int x = 0, y = 0, xMax = 0, yMax = 0;
        for (int i = 0; i < qMin(5, selection.count()); ++i) {
            QPixmap pix = dm->item(selection.at(i).row())->icon().pixmap(72);
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
        pix = dm->item(selection.at(0).row())->icon().pixmap(128);
        drag->setPixmap(pix);
    }
    drag->setHotSpot(QPoint(pix.width() / 2, pix.height() / 2));
    drag->exec(Qt::CopyAction | Qt::LinkAction, Qt::IgnoreAction);
//    drag->exec(Qt::CopyAction | Qt::MoveAction | Qt::LinkAction, Qt::IgnoreAction);
}

// -----------------------------------------------------------------------------
// Scrollbar Class used to capture scrollbar show event
// -----------------------------------------------------------------------------

//Scrollbar::Scrollbar(QWidget *parent)
//{
//}

//bool Scrollbar::eventFilter(QObject *obj, QEvent *event)
//{
///*

//*/
//    qDebug() << event;
//    if (event->type() == QEvent::Show) {
//        if (!event->spontaneous()) {
//            qDebug() << "Scrollbar::eventFilter  event->type() == QEvent::Show";
//            emit updateScrollTo();
//        }
//    }
//}

//void Scrollbar::test()
//{
//    qDebug() << "Scrollbar::setFont";
//}

