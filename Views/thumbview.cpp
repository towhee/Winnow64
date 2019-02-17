#include "Views/thumbview.h"
#include "Main/mainwindow.h"

/*  ThumbView Overview

ThumbView manages the list of images within a folder and it's children
(optional). The thumbView can either be a QListView of file names or thumbnails.
When a list item is selected the image is shown in imageView.

The thumbView is inside a QDockWidget which allows it to dock and be moved and
resized.

ThumbView does the following:

    Manages the file list of eligible images, including attributes for
    selected and picked. Picked files are shown in green and can be filtered
    and copied to another folder via the ingestDlg class.

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

        PathRole - the file path
        FileNameRole - the file name xxxxxx.ext

   Datamodel columns:

        PickColumn - bool is picked
        RatingColumn
        LabelColumn

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

MW *m2;

ThumbView::ThumbView(QWidget *parent, DataModel *dm, QString objName)
    : QListView(parent)
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    this->dm = dm;
    setObjectName(objName);

    // this works because ThumbView is a friend class of MW.  It is used in the
    // event filter to access the thumbDock
    m2 = qobject_cast<MW*>(parent);
    pickFilter = false;

    setViewMode(QListView::IconMode);
    setSelectionMode(QAbstractItemView::ExtendedSelection);
    setTabKeyNavigation(true);  // not working
    setResizeMode(QListView::Adjust);
    setLayoutMode(QListView::Batched);
    setVerticalScrollMode(QAbstractItemView::ScrollPerItem);
//    setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    verticalScrollBar()->setObjectName("VerticalScrollBar");
    horizontalScrollBar()->setObjectName("HorizontalScrollBar");
    setWordWrap(true);
    setDragEnabled(true);
    setEditTriggers(QAbstractItemView::NoEditTriggers);
    setSelectionBehavior(QAbstractItemView::SelectRows);
    setSelectionRectVisible(true);
    setUniformItemSizes(true);
//    setUniformItemSizes(false);
    setMaximumHeight(100000);
    setContentsMargins(0,0,0,0);
    setSpacing(0);
    setLineWidth(0);
//    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);

    horizontalScrollBar()->setObjectName("ThumbViewHorizontalScrollBar");
    verticalScrollBar()->setObjectName("ThumbViewVerticalScrollBar");
//    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
//    setSizeAdjustPolicy(QAbstractScrollArea::AdjustToContents);
//    setBatchSize(2);

    setModel(this->dm->sf);

    thumbViewDelegate = new ThumbViewDelegate(this, m2->isRatingBadgeVisible);
    thumbViewDelegate->setThumbDimensions(thumbWidth, thumbHeight,
        thumbSpacing, thumbPadding, labelFontSize, showThumbLabels, badgeSize);
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
    qDebug() << G::t.restart() << "\t" << "List all thumbs";
    for (int i=0; i<dm->sf->rowCount(); i++) {
        idx = dm->sf->index(i, 0, QModelIndex());
        qDebug() << G::t.restart() << "\t" << i << idx.data(FileNameRole).toString();
    }
    */
}

void ThumbView::refreshThumb(QModelIndex idx, int role)
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    QVector<int> roles;
    roles.append(role);
    dataChanged(idx, idx, roles);
}

void ThumbView::refreshThumbs() {
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    dataChanged(dm->sf->index(0, 0), dm->sf->index(getLastRow(), 0));
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
    G::track(__FUNCTION__);
    #endif
    }
    setSpacing(thumbSpacing);
    thumbViewDelegate->setThumbDimensions(thumbWidth, thumbHeight,
        thumbSpacing, thumbPadding, labelFontSize, showThumbLabels, badgeSize);
    if(objectName() == "Thumbnails") {
        if (!m2->thumbDock->isFloating())
            emit updateThumbDockHeight();
    }
}

void ThumbView::setThumbParameters(int _thumbWidth, int _thumbHeight,
        int _thumbSpacing, int _thumbPadding, int _labelFontSize,
        bool _showThumbLabels, bool _wrapThumbs, int _badgeSize)
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    thumbWidth = _thumbWidth;
    thumbHeight = _thumbHeight;
    thumbSpacing = _thumbSpacing;
    thumbPadding = _thumbPadding;
    labelFontSize = _labelFontSize;
    showThumbLabels = _showThumbLabels;
    wrapThumbs = _wrapThumbs;
    badgeSize = _badgeSize;
    setThumbParameters();
}

int ThumbView::getThumbSpaceMin()
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    return THUMB_MIN + thumbSpacing * 2 + thumbPadding *2 + 8;
}

int ThumbView::getThumbSpaceMax()
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    return THUMB_MAX + thumbSpacing * 2 + thumbPadding *2 + 8;
}

QSize ThumbView::getThumbCellSize()
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
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
    G::track(__FUNCTION__);
    #endif
    }
    float aspect = thumbWidth / thumbHeight;
    // Difference between thumbSpace and thumbHeight
    int margin = thumbViewDelegate->getThumbCell().height() - thumbHeight;
    int newThumbHeight = thumbSpaceHeight - margin;
    int newThumbWidth = newThumbHeight * aspect;
    return newThumbWidth + margin - 1;
}

int ThumbView::getScrollThreshold(int thumbSpaceHeight)
{
    return viewport()->width() / getThumbSpaceWidth(thumbSpaceHeight);
}

// debugging
void ThumbView::reportThumb()
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    int currThumb = currentIndex().row();
    qDebug() << G::t.restart() << "\t" << "\n ***** THUMB INFO *****";
    qDebug() << G::t.restart() << "\t" << "Row =" << currThumb;
    qDebug() << G::t.restart() << "\t" << "FileNameRole " << G::PathRole << dm->item(currThumb)->data(G::PathRole).toString();

}

int ThumbView::getCurrentRow()
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    return currentIndex().row();
}

int ThumbView::getNextRow()
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
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
    G::track(__FUNCTION__);
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
    G::track(__FUNCTION__);
    #endif
    }
    return dm->sf->rowCount() - 1;
}

int ThumbView::getRandomRow()
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    return qrand() % (dm->sf->rowCount());
}

bool ThumbView::isSelectedItem()
{
    // call before getting current row or index
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    return true;
//    qDebug() << G::t.restart() << "\t" << "selectionModel()->selectedRows().size()" << selectionModel()->selectedRows().size();
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
    G::track(__FUNCTION__);
    #endif
    }
    QRect thumbViewRect = viewport()->rect();
    for (int row = 0; row < dm->sf->rowCount(); ++row) {
        if (visualRect(dm->sf->index(row, 0)).intersects(thumbViewRect)) {
//            QRect thumbRect = visualRect(dm->sf->index(row, 0));
//            qDebug() << G::t.restart() << "\t" << "ThumbView::getFirstVisible  row =" << row
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
    G::track(__FUNCTION__);
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

QString ThumbView::getCurrentFilePath()
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    return currentIndex().data(G::PathRole).toString();
}

// PICKS: Items that have been picked

// used in MW::ingests
bool ThumbView::isPick()
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
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
MW, passing the list on to the ingestDlg for ingestion/copying to another
folder.
*/
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    QFileInfoList fileInfoList;
    for (int row = 0; row < dm->sf->rowCount(); ++row) {
        QModelIndex idx = dm->sf->index(row, G::PickColumn);
        if (idx.data(Qt::EditRole).toString() == "true") {
            QModelIndex pathIdx = dm->sf->index(row, 0);
            QString fPath = pathIdx.data(G::PathRole).toString();
            QFileInfo fileInfo(fPath);
            fileInfoList.append(fileInfo);
        }
    }
    return fileInfoList;
}

int ThumbView::getNextPick()
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
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
    G::track(__FUNCTION__);
    #endif
    }
    int back = currentIndex().row() - 1;
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
    G::track(__FUNCTION__);
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

void ThumbView::sortThumbs(int sortColumn, bool isReverse)
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
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
    G::track(__FUNCTION__);
    #endif
    }
    QModelIndexList indexesList = selectionModel()->selectedIndexes();
    QStringList SelectedThumbsPaths;

    for (int tn = indexesList.size() - 1; tn >= 0 ; --tn) {
        SelectedThumbsPaths << indexesList[tn].data(G::PathRole).toString();
    }
    return SelectedThumbsPaths;
}

bool ThumbView::isThumb(int row)
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    return dm->sf->index(row, 0).data(Qt::DecorationRole).isNull();
}

// ASync
void ThumbView::processIconBuffer()
{
    static int count = 0;
    count++;
    forever {
        bool more = true;
        int row;
        QImage image;
        iconHash.takeOne(&row, &image, &more);
#ifdef ISTEST
        qDebug() << "ThumbView::processIconBuffer  Entry:"
                 << count
                 << "row = " << row
                 << "image size" << image.size();
#endif
        if (row >= 0) setIcon(row, image);
        if (!more) return;
    }
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
    G::track(__FUNCTION__);
    #endif
    }
    QString fPath = dm->index(row, 0).data().toString();
#ifdef ISTEST
    qDebug() << "ThumbView::setIcon for row " << row << fPath;
#endif
    QStandardItem *item = new QStandardItem;
    QModelIndex idx = dm->index(row, 0, QModelIndex());
    if (!idx.isValid()) {
//        qDebug() << "row" << row << "is invalid";
        return;
    }
    item = dm->itemFromIndex(idx);
    item->setIcon(QPixmap::fromImage(thumb));
}

// Used by thumbnail navigation (left, right, up, down etc)
void ThumbView::selectThumb(QModelIndex idx)
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    if (idx.isValid()) {
//        G::isMouseClick = true;
        setCurrentIndex(idx);
//        scrollTo(idx, ScrollHint::PositionAtCenter);
    }
}

void ThumbView::selectThumb(int row)
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
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
    G::track(__FUNCTION__);
    #endif
    }
    QModelIndexList idxList = dm->sf->match(dm->sf->index(0, 0), G::PathRole, fName);
    QModelIndex idx = idxList[0];
//    qDebug() << G::t.restart() << "\t" << "selectThumb  idx.row()" << idx.row();
    if(idx.isValid()) selectThumb(idx);
}

void ThumbView::selectNext()
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    if(G::mode == "Compare") return;
    selectThumb(getNextRow());
}

void ThumbView::selectPrev()
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    if(G::mode == "Compare") return;
    selectThumb(getPrevRow());
}

void ThumbView::selectUp()
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    if (G::mode == "Table" || !isWrapping()) selectPrev();
    else setCurrentIndex(moveCursor(QAbstractItemView::MoveUp, Qt::NoModifier));
}

void ThumbView::selectDown()
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    if (G::mode == "Table" || !isWrapping()) selectNext();
    else setCurrentIndex(moveCursor(QAbstractItemView::MoveDown, Qt::NoModifier));
}

void ThumbView::selectFirst()
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    selectThumb(0);
}

void ThumbView::selectLast()
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    selectThumb(getLastRow());
}

void ThumbView::selectRandom()
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    selectThumb(getRandomRow());
}

void ThumbView::selectNextPick()
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    selectThumb(getNextPick());
}

void ThumbView::selectPrevPick()
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    selectThumb(getPrevPick());
}

void ThumbView::thumbsEnlarge()
{
/* This function enlarges the size of the thumbnails in the thumbView, with the objectName
   "Thumbnails", which either resides in a dock or a floating window.
*/
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    if (thumbWidth < THUMB_MIN) thumbWidth = THUMB_MIN;
    if (thumbHeight < THUMB_MIN) thumbHeight = THUMB_MIN;
    if (thumbWidth < THUMB_MAX && thumbHeight < THUMB_MAX)
    {
        thumbWidth *= 1.1;
        thumbHeight *= 1.1;
        if (thumbWidth > THUMB_MAX) thumbWidth = THUMB_MAX;
        if (thumbHeight > THUMB_MAX) thumbHeight = THUMB_MAX;
    }
    setThumbParameters();
    scrollTo(currentIndex(), ScrollHint::PositionAtCenter);
}

void ThumbView::thumbsShrink()
{
/* This function reduces the size of the thumbnails in the thumbView, with the objectName
   "Thumbnails", which either resides in a dock or a floating window.
*/
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    if (thumbWidth > THUMB_MIN  && thumbHeight > THUMB_MIN) {
        thumbWidth *= 0.9;
        thumbHeight *= 0.9;
        if (thumbWidth < THUMB_MIN) thumbWidth = THUMB_MIN;
        if (thumbHeight < THUMB_MIN) thumbHeight = THUMB_MIN;
    }
    setThumbParameters();
    scrollTo(currentIndex(), ScrollHint::PositionAtCenter);
}

void ThumbView::thumbsRejustify()
{
/* This function controls the resizing behavior of the gridview cells when the window is
resized or the gridview preferences are edited. The grid cells sizes are maintained while
keeping the right side "justified". The cells can be direcly adjusted using the "[" and "]"
keys, but this is handled by the gridShrinkJustified and gridEnlargeJustified functions.

The key to making this work is the variable assignedThumbWidth, which is increased or decreased in the shrink and
enlarge functions, and used to maintain the cell size during the resize and preference adjustment
operations.

It could all be managed from this function, but is being kept separate for greater simplicity.

Again, note this only applied to the gridview with the objectName = "Grid". The thumbview,
with objectName "Thumbnails", is handled by thumbsEnlarge and thumbsShrink.
*/
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    int increment;
    if (thumbWidth < assignedThumbWidth) increment = -1;
    else increment = 1;

    int wCell = getThumbCellSize().width();
    int wRow = width() - G::scrollBarThickness - 8;    // always include scrollbar

    int tpr = wRow / wCell + increment;                // thumbs per row
    wCell = wRow / tpr;

    if (increment == 1 && wCell < THUMB_MIN) return;
    if (increment == -1 && wCell > THUMB_MAX) return;

    int oldThumbWidth = thumbWidth;
    thumbWidth = thumbViewDelegate->getThumbWidthFromCellWidth(wCell);
    thumbHeight = (thumbHeight / oldThumbWidth) * thumbWidth;

    skipResize = true;      // prevent feedback loop

    setThumbParameters();
    scrollTo(currentIndex(), ScrollHint::PositionAtCenter);
}

void ThumbView::gridEnlargeJustified()
{
/*
   This function enlarges the grid cells while keeping the right hand side margin minimized.
   To make this work it is critical to assign the correct value to the row width: wRow.  The
   superclass QListView, uses a width that assumes there will always be a scrollbar and also a
   "margin". The width of the QListView (width()) is reduced by the width of the scrollbar and
   and additional "margin", determined by experimentation, to be 8 pixels.

   The variable assignedThumbWidth remebers the current grid cell size as a reference to
   maintain the grid size during resizing and pad size changes in preferences.
*/
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    int wCell = getThumbCellSize().width();
    int wRow = width() - G::scrollBarThickness - 8;    // always include scrollbar

    int tpr = wRow / wCell - 1;                        // thumbs per row
    wCell = wRow / tpr;

    if (wCell > THUMB_MAX) return;

    int oldThumbWidth = thumbWidth;
    thumbWidth = thumbViewDelegate->getThumbWidthFromCellWidth(wCell);
    thumbHeight = (thumbHeight / oldThumbWidth) * thumbWidth;

    assignedThumbWidth = thumbWidth;
    skipResize = true;      // prevent feedback loop

    setThumbParameters();
    scrollTo(currentIndex(), ScrollHint::PositionAtCenter);
}

void ThumbView::gridShrinkJustified()
{
/*
   This function shrinks the grid cells while keeping the right hand side margin minimized.
   To make this work it is critical to assign the correct value to the row width: wRow.  The
   superclass QListView, uses a width that assumes there will always be a scrollbar and also a
   "margin". The width of the QListView (width()) is reduced by the width of the scrollbar and
   and additional "margin", determined by experimentation, to be 8 pixels.

   The variable assignedThumbWidth remebers the current grid cell size as a reference to
   maintain the grid size during resizing and pad size changes in preferences.
*/
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    int wCell = getThumbCellSize().width();
    int wRow = width() - G::scrollBarThickness - 8;    // always include scrollbar

    int tpr = wRow / wCell + 1;                        // thumbs per row
    wCell = wRow / tpr;

    if (wCell < THUMB_MIN) return;

    int oldThumbWidth = thumbWidth;
    thumbWidth = thumbViewDelegate->getThumbWidthFromCellWidth(wCell);
    thumbHeight = (thumbHeight / oldThumbWidth) * thumbWidth;

    assignedThumbWidth = thumbWidth;
    skipResize = true;      // prevent feedback loop

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
//  G::track(__FUNCTION__);
    #endif
    }
    dm->sf->setData(index, iconRect, G::ThumbRectRole);
}

void ThumbView::resizeEvent(QResizeEvent *event)
{
/*  resizeEvent can be triggered by a change in the gridView cell size (thumbWidth) that
   requires justification.  That is filtered by checking if the ThumbView width has changed
   or skipResize has been set.
*/
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    if (skipResize) {
        skipResize = false;
        return;
    }
    static int count = -1;
    count++;
    QListView::resizeEvent(event);
    static int prevWidth;
    if (count == 0) {
        prevWidth = width();
        return;
    }
    count++;
    if (objectName() == "Thumbnails") return;
    if (width() == prevWidth) return;
    thumbsRejustify();
}

void ThumbView::thumbsFit(Qt::DockWidgetArea area)
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    if (G::mode == "Grid") {
        return;
    }

    // all wrapping is row wrapping
    if (isWrapping()) {
        return;

        // adjust thumb width
        int scrollWidth = G::scrollBarThickness;
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
    }
    // no wrapping - must be bottom or top dock area
    else if (area == Qt::BottomDockWidgetArea || area == Qt::TopDockWidgetArea
             || !wrapThumbs){
        // set target ht based on space with scrollbar (always on)
        int ht = height();
        int scrollHeight = 12;
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

        // change the thumbnail size in thumbViewDelegate
        setSpacing(thumbSpacing);

        thumbViewDelegate->setThumbDimensions(thumbWidth, thumbHeight,
            thumbSpacing, thumbPadding, labelFontSize, showThumbLabels, badgeSize);
    }
}

void ThumbView::thumbsFitTopOrBottom()
{
/*
Called by MW::eventFilter when a thumbDock resize event occurs triggered by the
user resizing the thumbDock. Adjust the size of the thumbs to fit the new
thumbDock height.

For thumbSpace anatomy (see ThumbViewDelegate)
*/
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
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
    thumbHeight = thumbHeight > THUMB_MAX ? THUMB_MAX : thumbHeight;
    thumbHeight = thumbHeight < THUMB_MIN ? THUMB_MIN : thumbHeight;
    thumbWidth = thumbHeight * aspect;

    // check thumbWidth within range
    if(thumbWidth > THUMB_MAX) {
        thumbWidth = THUMB_MAX;
        thumbHeight = THUMB_MAX / aspect;
    }
    setSpacing(thumbSpacing);

    thumbViewDelegate->setThumbDimensions(thumbWidth, thumbHeight,
        thumbSpacing, thumbPadding, labelFontSize, showThumbLabels, badgeSize);
}

void ThumbView::updateLayout()
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    QEvent event{QEvent::LayoutRequest};
    QListView::updateGeometries();
    QListView::event(&event);
}

void ThumbView::scrollDown(int /*step*/)
{
    if(wrapThumbs) {
        horizontalScrollBar()->triggerAction(QAbstractSlider::SliderSingleStepAdd);
    }
    else {
        verticalScrollBar()->triggerAction(QAbstractSlider::SliderSingleStepAdd);
    }
}

void ThumbView::scrollUp(int /*step*/)
{
    if(wrapThumbs) {
        horizontalScrollBar()->triggerAction(QAbstractSlider::SliderSingleStepSub);
    }
    else {
        verticalScrollBar()->triggerAction(QAbstractSlider::SliderSingleStepSub);
    }
}

void ThumbView::scrollPageDown(int /*step*/)
{
    if(wrapThumbs) {
        horizontalScrollBar()->triggerAction(QAbstractSlider::SliderPageStepAdd);
    }
    else {
        verticalScrollBar()->triggerAction(QAbstractSlider::SliderPageStepAdd);
    }
}

void ThumbView::scrollPageUp(int /*step*/)
{
    if(wrapThumbs) {
        horizontalScrollBar()->triggerAction(QAbstractSlider::SliderPageStepSub);
    }
    else {
        verticalScrollBar()->triggerAction(QAbstractSlider::SliderPageStepSub);
    }
}

void ThumbView::scrollToCurrent(int row)
{
/*
This is called from MW::eventFilter after the eventFilter intercepts the last scrollbar paint
event for the newly visible ThumbView. ThumbView is newly visible because in a mode change in
MW (ie from Grid to Loupe) thumbView was hidden in Grid but visible in Loupe. Widgets will not
respond while hidden so we must wait until ThumbView is visible and completely repainted. It
takes a while for the scrollbars to finish painting, so when that is done, we want to scroll
to the current position.

It is also called from MW::delayScroll, which in turn, is called by MW::fileSelectionChange
when a new image is selected and the selection was triggered by a mouse click and
MW::mouseClickScroll == true.
*/
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
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
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    int pageWidth = viewport()->width();
    int thumbWidth = getThumbCellSize().width();

    qDebug() << "ThumbView::getHorizontalScrollBarOffset   "
             << "pageWidth" << pageWidth
             << "thumbWidth" << pageWidth;

    if (pageWidth < THUMB_MIN || thumbWidth < THUMB_MIN)
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
    qDebug() << G::t.restart() << "\t" << objectName()
             << "Row =" << m2->currentRow
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
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    int pageWidth = viewport()->width();
    int pageHeight = viewport()->height();
    int thumbCellWidth = getThumbCellSize().width();
    int thumbCellHeight = getThumbCellSize().height();

    if (pageWidth < THUMB_MIN ||pageHeight < THUMB_MIN || thumbCellWidth < THUMB_MIN || thumbCellHeight < THUMB_MIN)
        return 0;

    float thumbsPerPage = pageWidth / thumbCellWidth * (float)pageHeight / thumbCellHeight;
    float thumbRowsPerPage = (float)pageHeight / thumbCellHeight;
    int n = dm->sf->rowCount();
//    qDebug() << G::t.restart() << "\t" << "thumbsPerPage" << thumbsPerPage;
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
    qDebug() << G::t.restart() << "\t" << objectName()
             << "Row =" << m2->currentRow
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
    G::track(__FUNCTION__);
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
    G::track(__FUNCTION__);
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
    qDebug() << G::t.restart() << "\t" << objectName()
             << "Row =" << m2->currentRow
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
    G::track(__FUNCTION__);
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
    G::track(__FUNCTION__);
    #endif
    }

    if (event->button() == Qt::RightButton) return;

    // is this a grid or a thumb view
    if(G::mode == "Grid") G::source =  "GridMouseClick";
    else G::source =  "ThumbMouseClick";

    // forward and back buttons
    if (event->button() == Qt::BackButton) {
//        thumbView->selectPrev();
        m2->togglePick();
        return;
    }
    if (event->button() == Qt::ForwardButton) {
//        thumbView->selectNext();
        m2->togglePick();
        return;
    }

    // capture mouse click position for imageView zoom/pan
    if (event->modifiers() == Qt::NoModifier) {
        // reqd for thumb resizing
        if (event->button() == Qt::LeftButton) isLeftMouseBtnPressed = true;

        /* Capture the percent coordinates of the mouse click within the thumbnail
           so that the full scale image can be zoomed to the same point.  */
        QModelIndex idx = currentIndex();
        QRect iconRect = idx.data(G::ThumbRectRole).toRect();
        QPoint mousePt = event->pos();
        QPoint iconPt = mousePt - iconRect.topLeft();
        float xPct = (float)iconPt.x() / iconRect.width();
        float yPct = (float)iconPt.y() / iconRect.height();
        if (xPct >= 0 && xPct <= 1 && yPct >= 0 && yPct <=1) {
            //signal sent to ImageView
            thumbClick(xPct, yPct);
        }
    }

    QListView::mousePressEvent(event);
}

void ThumbView::mouseMoveEvent(QMouseEvent *event)
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
//    qDebug() << "🔎🔎🔎 ThumbView::mouseMoveEvent event =" << event << event->pos();
    if (isLeftMouseBtnPressed) isMouseDrag = true;
    QListView::mouseMoveEvent(event);
}

void ThumbView::mouseReleaseEvent(QMouseEvent *event)
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
//    qDebug() << G::t.restart() << "\t" << "🔎🔎🔎 ThumbView::mouseReleaseEvent ";
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
    G::track(__FUNCTION__);
    #endif
    }
    QListView::mouseDoubleClickEvent(event);
    // do not displayLoupe if already displayed
    if (G::mode != "Loupe" && event->button() == Qt::LeftButton) {
        emit displayLoupe();
    }
    scrollToCurrent(currentIndex().row());
}

void ThumbView::invertSelection()
{
/*
Inverts/toggles which thumbs are selected.  Called from MW::invertSelectionAct
*/
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    QItemSelection toggleSelection;
    QModelIndex firstIndex = dm->sf->index(0, 0);
    QModelIndex lastIndex = dm->sf->index(dm->sf->rowCount() - 1, 0);
    toggleSelection.select(firstIndex, lastIndex);
    selectionModel()->select(toggleSelection, QItemSelectionModel::Toggle);
}

void ThumbView::copyThumbs()        // rgh is this working?
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
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
        urls << QUrl(it->data(G::PathRole).toString());
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
    G::track(__FUNCTION__);
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
        QString fPath = selection.at(i).data(G::PathRole).toString();
        urls << QUrl::fromLocalFile(fPath);
    }
//    qDebug() << G::t.restart() << "\t" << urls;

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
}
