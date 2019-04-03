#include "Views/iconview.h"
#include "Main/mainwindow.h"

// Trial to prevent unwanted scrolling - did not work

ScrollBar::ScrollBar(QWidget *parent) :
    QScrollBar(parent)
{

}

void ScrollBar::setValue(int value)
{
    qDebug() << "ScrollBar::setValue" << value;
    QScrollBar::setValue(value);
}

bool ScrollBar::event(QEvent *event)
{
//    qDebug() << "ScrollBar::event" << event;
    return QScrollBar::event(event);
}

void ScrollBar::sliderChange(QAbstractSlider::SliderChange change)
{
//    qDebug() << "ScrollBar::sliderChange" << change;
    QScrollBar::sliderChange(change);
}

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

IconView::IconView(QWidget *parent, DataModel *dm, QString objName)
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

//    ScrollBar *scrollBar = new ScrollBar;
//    setVerticalScrollBar(scrollBar);

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
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);

//    if (objName == "Thumbnails") {
        horizontalScrollBar()->setObjectName("ThumbViewHorizontalScrollBar");
        verticalScrollBar()->setObjectName("IconViewVerticalScrollBar");
//    }
//    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
//    setSizeAdjustPolicy(QAbstractScrollArea::AdjustToContents);
//    setBatchSize(2);

    setModel(this->dm->sf);

    iconViewDelegate = new IconViewDelegate(this, m2->isRatingBadgeVisible);
    iconViewDelegate->setThumbDimensions(thumbWidth, thumbHeight,
        thumbSpacing, thumbPadding, labelFontSize, showThumbLabels, badgeSize);
    setItemDelegate(iconViewDelegate);

    // used to provide iconRect info to zoom to point clicked on thumb
    // in imageView
    connect(iconViewDelegate, SIGNAL(update(QModelIndex, QRect)),
            this, SLOT(updateThumbRectRole(QModelIndex, QRect)));
}

void IconView::reportThumbs()
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

void IconView::refreshThumb(QModelIndex idx, int role)
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

void IconView::refreshThumbs() {
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    dataChanged(dm->sf->index(0, 0), dm->sf->index(getLastRow(), 0));
}

void IconView::setThumbParameters()
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
    setSpacing(0);
    iconViewDelegate->setThumbDimensions(thumbWidth, thumbHeight,
        0, thumbPadding, labelFontSize, showThumbLabels, badgeSize);
    if(objectName() == "Thumbnails") {
        if (!m2->thumbDock->isFloating())
            emit updateThumbDockHeight();
    }
}

void IconView::setThumbParameters(int _thumbWidth, int _thumbHeight,
        int _thumbSpacing, int _thumbPadding, int _labelFontSize,
        bool _showThumbLabels, /*bool _wrapThumbs, */int _badgeSize)
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
//    wrapThumbs = _wrapThumbs;
    badgeSize = _badgeSize;
    setThumbParameters();
}

QSize IconView::getCellSize()
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
//    setThumbParameters(false);    // reqd?  rgh
    return iconViewDelegate->getCellSize();
}

int IconView::getThumbSpaceWidth(int thumbSpaceHeight)
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
    int margin = iconViewDelegate->getCellSize().height() - thumbHeight;
    int newThumbHeight = thumbSpaceHeight - margin;
    int newThumbWidth = newThumbHeight * aspect;
    return newThumbWidth + margin - 1;
}

int IconView::getScrollThreshold(int thumbSpaceHeight)
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    return viewport()->width() / getThumbSpaceWidth(thumbSpaceHeight);
}

// debugging
void IconView::reportThumb()
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

int IconView::getCurrentRow()
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    return currentIndex().row();
}

int IconView::getNextRow()
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

int IconView::getPrevRow()
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

int IconView::getLastRow()
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    return dm->sf->rowCount() - 1;
}

int IconView::getRandomRow()
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    return qrand() % (dm->sf->rowCount());
}

bool IconView::isSelectedItem()
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

int IconView::getThumbsPerPage()
{
    int rowWidth = viewport()->width() - G::scrollBarThickness;
    int cellWidth = getCellSize().width();
    if (cellWidth == 0) return 0;
    // thumbs per row
    int tpr = rowWidth / getCellSize().width() + 1;
    int cellHeight = getCellSize().height();
    if (cellHeight == 0) return 0;
    // rows per page (page = viewport)
    int rpp = viewport()->height() / cellHeight + 2;
//    qDebug() << "tpr" << tpr
//             << "rpp" << rpp;
    thumbsPerPage = tpr * rpp;
    return thumbsPerPage;
}

int IconView::getFirstVisible()
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
            return row;
        }
    }
    return -1;
}

int IconView::getLastVisible()
{
/*
Return the datamodel row for the last thumb visible in the IconView. This is
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

void IconView::setViewportParameters()
{
/*
Set the firstVisibleRow, lastVisibleRow and thumbsPerPage.  This is called when the application
show event occurs, when there is a viewport scroll event or when an icon justification happens.
*/    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    int row;
//    firstVisibleRow = indexAt(QPoint(0, 0)).row();
    firstVisibleRow = 0;
    QRect thumbViewRect = viewport()->rect();
    for (row = 0; row < dm->sf->rowCount(); ++row) {
        if (visualRect(dm->sf->index(row, 0)).intersects(thumbViewRect)) {
            firstVisibleRow = row;
            break;
        }
    }
    for (row = firstVisibleRow; row < dm->sf->rowCount(); ++row) {
        if (visualRect(dm->sf->index(row, 0)).intersects(thumbViewRect)) {
            lastVisibleRow = row;
        }
        else break;
    }
    thumbsPerPage = lastVisibleRow - firstVisibleRow;
    midVisibleRow = firstVisibleRow + thumbsPerPage / 2;
}

bool IconView::isRowVisible(int row)
{
    setViewportParameters();
    return row >= firstVisibleRow && row <= lastVisibleRow;
}

QString IconView::getCurrentFilePath()
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
bool IconView::isPick()
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

QFileInfoList IconView::getPicks()
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

int IconView::getNextPick()
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

int IconView::getPrevPick()
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

int IconView::getNearestPick()
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

void IconView::sortThumbs(int sortColumn, bool isReverse)
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

void IconView::selectionChanged(const QItemSelection &selected, const QItemSelection &deselected)
{
/*
For some reason the selectionModel rowCount is not up-to-date and the selection is updated
after the MD::fileSelectionChange occurs, hence update the status bar from here.
*/
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    QListView::selectionChanged(selected, deselected);
    if (!G::isInitializing) emit updateStatus(true, "");
}

int IconView::getSelectedCount()
{
/*
For some reason the selectionModel->selectedRows().count() is not up-to-date but
selectedIndexes().count() works. This is called from MW::updateStatus to report the number of
images selected.
*/
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    return selectedIndexes().count();
}

QStringList IconView::getSelectedThumbsList()
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

bool IconView::isThumb(int row)
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    return dm->sf->index(row, 0).data(Qt::DecorationRole).isNull();
}

// ASync
void IconView::processIconBuffer()
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

void IconView::setIcon(int row, QImage thumb)
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
    QString fPath = dm->index(row, 0).data(G::PathRole).toString();
//    qDebug() << "IconView::setIcon: " << fPath;
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
void IconView::selectThumb(QModelIndex idx)
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    if (idx.isValid()) {
//        G::isMouseClick = true;
        setCurrentIndex(idx);
        scrollTo(idx, ScrollHint::PositionAtCenter);
    }
}

void IconView::selectThumb(int row)
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

void IconView::selectThumb(QString &fName)
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

void IconView::selectNext()
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    if(G::mode == "Compare") return;
    selectThumb(getNextRow());
}

void IconView::selectPrev()
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    if(G::mode == "Compare") return;
    selectThumb(getPrevRow());
}

void IconView::selectUp()
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    if (G::mode == "Table" || !isWrapping()) selectPrev();
    else setCurrentIndex(moveCursor(QAbstractItemView::MoveUp, Qt::NoModifier));
}

void IconView::selectDown()
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    if (G::mode == "Table" || !isWrapping()) selectNext();
    else setCurrentIndex(moveCursor(QAbstractItemView::MoveDown, Qt::NoModifier));
}

void IconView::selectFirst()
{
    {
    #ifdef ISDEBUG
        G::track(__FUNCTION__);
    #endif
    }
    G::track(__FUNCTION__);
    selectThumb(0);
}

void IconView::selectLast()
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    selectThumb(getLastRow());
}

void IconView::selectRandom()
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    selectThumb(getRandomRow());
}

void IconView::selectNextPick()
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    selectThumb(getNextPick());
}

void IconView::selectPrevPick()
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    selectThumb(getPrevPick());
}

void IconView::thumbsEnlarge()
{
/* This function enlarges the size of the thumbnails in the thumbView, with the objectName
   "Thumbnails", which either resides in a dock or a floating window.
*/
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    if (thumbWidth < ICON_MIN) thumbWidth = ICON_MIN;
    if (thumbHeight < ICON_MIN) thumbHeight = ICON_MIN;
    if (thumbWidth < G::maxIconSize && thumbHeight < G::maxIconSize)
    {
        thumbWidth *= 1.1;
        thumbHeight *= 1.1;
        if (thumbWidth > G::maxIconSize) thumbWidth = G::maxIconSize;
        if (thumbHeight > G::maxIconSize) thumbHeight = G::maxIconSize;
    }
    setThumbParameters();
    scrollTo(currentIndex(), ScrollHint::PositionAtCenter);
}

void IconView::thumbsShrink()
{
/* This function reduces the size of the thumbnails in the thumbView, with the objectName
   "Thumbnails", which either resides in a dock or a floating window.
*/
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    if (thumbWidth > ICON_MIN  && thumbHeight > ICON_MIN) {
        thumbWidth *= 0.9;
        thumbHeight *= 0.9;
        if (thumbWidth < ICON_MIN) thumbWidth = ICON_MIN;
        if (thumbHeight < ICON_MIN) thumbHeight = ICON_MIN;
    }
    setThumbParameters();
    scrollTo(currentIndex(), ScrollHint::PositionAtCenter);
}

//void IconView::resizeRejustify()
//{
////    qDebug() << "IconView::resizeRejustify";
//    static int prevWidth = 0;
//    if (width() == prevWidth) return;
//    prevWidth = width();
//    rejustify();
//}

void IconView::rejustify()
{
/* This function controls the resizing behavior of the thumbnails in gridview and thumbview
when wrapping = true and the window is resized or the gridview preferences are edited. The
grid cells sizes are maintained while keeping the right side "justified". The cells can be
direcly adjusted using the "[" and "]" keys, but this is handled by the justify() function.

The key to making this work is the variable assignedThumbWidth, which is increased or
decreased in the justify() function, and used to maintain the cell size during the
resize and preference adjustment operations.
*/
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
//    qDebug() << objectName() << "::rejustify   "
//             << "isWrapping" << isWrapping();

    if (!isWrapping()) return;

    // get
    int wRow = width() - G::scrollBarThickness - 8;    // always include scrollbar
    if (assignedThumbWidth < 40 || assignedThumbWidth > 480) assignedThumbWidth = thumbWidth;
    int wCell = iconViewDelegate->getCellWidthFromThumbWidth(assignedThumbWidth);

    if (wCell == 0) return;
    int tpr = wRow / wCell;

    if (tpr == 0) return;
    wCell = wRow / tpr;

    thumbWidth = iconViewDelegate->getThumbWidthFromCellWidth(wCell);
    thumbHeight = thumbWidth * bestAspectRatio;

    skipResize = true;      // prevent feedback loop

    setThumbParameters();
    setViewportParameters();

//    qDebug() << objectName() << "::rejustify   "
//             << "firstVisibleRow" << firstVisibleRow
//             << "lastVisibleRow" << lastVisibleRow
//             << "thumbsPerPage" << thumbsPerPage;

//    scrollTo(currentIndex(), ScrollHint::PositionAtCenter);
}

void IconView::justify(JustifyAction action)
{
/*
   This function enlarges or shrinks the grid cells while keeping the right hand side margin
   minimized. To make this work it is critical to assign the correct value to the row width:
   wRow. The superclass QListView, uses a width that assumes there will always be a scrollbar
   and also a "margin". The width of the QListView (width()) is reduced by the width of the
   scrollbar and and additional "margin", determined by experimentation, to be 8 pixels.

   The variable assignedThumbWidth remebers the current grid cell size as a reference to
   maintain the grid size during resizing and pad size changes in preferences.
*/
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    int wCell = iconViewDelegate->getCellSize().width();
    int wRow = width() - G::scrollBarThickness - 8;    // always include scrollbar

    int tpr = wRow / wCell + action;                        // thumbs per row
    if (tpr < 1) return;
    wCell = wRow / tpr;

    if (wCell > G::maxIconSize) wCell = wRow / ++tpr;

    thumbWidth = iconViewDelegate->getThumbWidthFromCellWidth(wCell);
    thumbHeight = thumbWidth * bestAspectRatio;

    assignedThumbWidth = thumbWidth;
    skipResize = true;      // prevent feedback loop

    setThumbParameters();
    setViewportParameters();

//    scrollTo(currentIndex(), ScrollHint::PositionAtCenter);
}

void IconView::updateThumbRectRole(const QModelIndex index, QRect iconRect)
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

void IconView::resizeEvent(QResizeEvent *event)
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
    QListView::resizeEvent(event);

    m2->loadMetadataCacheAfterDelay();

    // prevent a feedback loop where justify() or rejustify() triggers a resize event
//    if (skipResize) {
//        skipResize = false;
//        return;
//    }

    static int prevWidth = 0;
    if (isWrapping() && width() != prevWidth) {
        QTimer::singleShot(500, this, SLOT(rejustify()));
    }
    prevWidth = width();
}

void IconView::bestAspect()
{
/* This function scans icons in the datamodel to find the greatest height and width of the
icons. The resulting max width and height are sent to IconViewDelegate to define the thumbRect
that holds each icon.  This is also the most compact container available.

The function is called after a new folder is selected and the datamodel icon data has been
loaded.  Both thumbView and gridView have to be called.
*/
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    iconWMax = 0, iconHMax = 0;
    if (thumbWidth > G::maxIconSize) thumbWidth = G::maxIconSize;
    if (thumbHeight > G::maxIconSize) thumbHeight = G::maxIconSize;
    if (thumbWidth < ICON_MIN) thumbWidth = ICON_MIN;
    if (thumbHeight < ICON_MIN) thumbHeight = ICON_MIN;
    for (int row = 0; row < dm->rowCount(); ++row) {
        QModelIndex idx = dm->index(row, 0);
        if (idx.data(Qt::DecorationRole).isNull()) continue;
        QPixmap pm = dm->itemFromIndex(idx)->icon().pixmap(G::maxIconSize);
        if (iconWMax < pm.width()) iconWMax = pm.width();
        if (iconHMax < pm.height()) iconHMax = pm.height();
        if (iconWMax == G::maxIconSize && iconHMax == G::maxIconSize) break;
    }
    if (iconWMax == iconHMax && thumbWidth > thumbHeight)
        thumbHeight = thumbWidth;
    if (iconWMax == iconHMax && thumbHeight > thumbWidth)
        thumbWidth = thumbHeight;
    if (iconWMax > iconHMax) thumbHeight = thumbWidth * ((double)iconHMax / iconWMax);
    if (iconHMax > iconWMax) thumbWidth = thumbHeight * ((double)iconWMax / iconHMax);
    setThumbParameters();
//    iconViewDelegate->setThumbDimensions(thumbWidth, thumbHeight,
//        0, thumbPadding, labelFontSize, showThumbLabels, badgeSize);
//    setSpacing(0);
    bestAspectRatio = (double)thumbHeight / thumbWidth;
}

void IconView::thumbsFit(Qt::DockWidgetArea area)
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
        int thumbCellWidth = iconViewDelegate->getCellSize().width() - thumbPadding * 2;
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
             || !isWrapping()){
        // set target ht based on space with scrollbar (always on)
        int ht = height();
        int scrollHeight = G::scrollBarThickness;
        ht -= scrollHeight;

        // adjust thumb height
        float aspect = thumbWidth / thumbHeight;

        // get the current thumb cell
        int cellHeight = iconViewDelegate->getCellSize().height();

        // padding = nonthumb space is used to rebuild cell after thumb resize to fit
        int padding = cellHeight - thumbHeight;
        int maxCellHeight = iconViewDelegate->getCellSize(QSize(iconWMax, iconHMax)).height();
        cellHeight = ht < maxCellHeight ? ht : maxCellHeight;
        thumbHeight = cellHeight - padding;
        thumbWidth = thumbHeight * aspect;

        // change the thumbnail size in thumbViewDelegate
        setSpacing(0);
qDebug() << "thumbsFit   thumbHeight" << thumbHeight << "thumbWidth" << thumbWidth;
        iconViewDelegate->setThumbDimensions(thumbWidth, thumbHeight,
            thumbSpacing, thumbPadding, labelFontSize, showThumbLabels, badgeSize);
    }
}

void IconView::thumbsFitTopOrBottom()
{
/*
Called by MW::eventFilter when a thumbDock resize event occurs triggered by the user resizing
the thumbDock. Adjust the size of the thumbs to fit the new thumbDock height.

For thumbSpace anatomy (see IconViewDelegate)
*/
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    float aspect = (float)thumbWidth / thumbHeight;

    // viewport available height
    int netViewportHt = height() - G::scrollBarThickness;

    int hMax = iconViewDelegate->getCellHeightFromThumbHeight(G::maxIconSize * bestAspectRatio);
    int hMin = iconViewDelegate->getCellHeightFromThumbHeight(ICON_MIN);

    // restrict icon cell height within limits
    int newThumbSpaceHt = netViewportHt > hMax ? hMax : netViewportHt;
    newThumbSpaceHt = newThumbSpaceHt < hMin ? hMin : newThumbSpaceHt;

    // derive new thumbsize from new thumbSpace
    thumbHeight = iconViewDelegate->getCellHeightFromAvailHeight(newThumbSpaceHt);

    // make sure within range (should be from thumbSpace check but just to be sure)
    thumbHeight = thumbHeight > G::maxIconSize ? G::maxIconSize : thumbHeight;
    thumbHeight = thumbHeight < ICON_MIN ? ICON_MIN : thumbHeight;
    thumbWidth = thumbHeight * aspect;

    // check thumbWidth within range
    if(thumbWidth > G::maxIconSize) {
        thumbWidth = G::maxIconSize;
        thumbHeight = G::maxIconSize / aspect;
    }

    // this is critical - otherwise thumbs bunch up
    setSpacing(0);

    iconViewDelegate->setThumbDimensions(thumbWidth, thumbHeight,
        thumbSpacing, thumbPadding, labelFontSize, showThumbLabels, badgeSize);
    scrollToRow(currentIndex().row());
}

void IconView::updateLayout()
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

void IconView::scrollDown(int /*step*/)
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    if(isWrapping()) {
        horizontalScrollBar()->triggerAction(QAbstractSlider::SliderSingleStepAdd);
    }
    else {
        verticalScrollBar()->triggerAction(QAbstractSlider::SliderSingleStepAdd);
    }
}

void IconView::scrollUp(int /*step*/)
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    if(isWrapping()) {
        horizontalScrollBar()->triggerAction(QAbstractSlider::SliderSingleStepSub);
    }
    else {
        verticalScrollBar()->triggerAction(QAbstractSlider::SliderSingleStepSub);
    }
}

void IconView::scrollPageDown(int /*step*/)
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    if(isWrapping()) {
        horizontalScrollBar()->triggerAction(QAbstractSlider::SliderPageStepAdd);
    }
    else {
        verticalScrollBar()->triggerAction(QAbstractSlider::SliderPageStepAdd);
    }
}

void IconView::scrollPageUp(int /*step*/)
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    if(isWrapping()) {
        horizontalScrollBar()->triggerAction(QAbstractSlider::SliderPageStepSub);
    }
    else {
        verticalScrollBar()->triggerAction(QAbstractSlider::SliderPageStepSub);
    }
}

void IconView::scrollToRow(int row)
{
/*
This is called from MW::eventFilter after the eventFilter intercepts the last scrollbar
paint event for the newly visible ThumbView. ThumbView is newly visible because in a mode
change in MW (ie from Grid to Loupe) thumbView was hidden in Grid but visible in Loupe.
Widgets will not respond while hidden so we must wait until ThumbView is visible and
completely repainted. It takes a while for the scrollbars to finish painting, so when
that is done, we want to scroll to the current position.

It is also called from MW::delayScroll, which in turn, is called by MW::fileSelectionChange
when a new image is selected and the selection was triggered by a mouse click and
MW::mouseClickScroll == true.
*/
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    G::track(__FUNCTION__, QString::number(row));
    QModelIndex idx = dm->sf->index(row, 0);
    scrollTo(idx, QAbstractItemView::PositionAtCenter);

//    if (!isWrapping())
//        horizontalScrollBar()->setValue(getHorizontalScrollBarOffset(row));
//    else
//        verticalScrollBar()->setValue(getVerticalScrollBarOffset(row));

    readyToScroll = false;
}

int IconView::getHorizontalScrollBarOffset(int row)
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    int pageWidth = viewport()->width();
    int thumbWidth = getCellSize().width();

    if (pageWidth < ICON_MIN || thumbWidth < ICON_MIN)
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

int IconView::getVerticalScrollBarOffset(int row)
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    if (objectName() == "Thumbnails") return 0;
    int pageWidth = viewport()->width();
    int pageHeight = viewport()->height();
    int thumbCellWidth = getCellSize().width();
    int thumbCellHeight = getCellSize().height();

    if (pageWidth < ICON_MIN || pageHeight < ICON_MIN || thumbCellWidth < ICON_MIN || thumbCellHeight < ICON_MIN)
        return 0;
    if (thumbCellWidth == 0  || thumbCellHeight == 0 || pageWidth == 0 || pageHeight == 0) return 0;

    float thumbsPerPage = pageWidth / thumbCellWidth * (float)pageHeight / thumbCellHeight;
    float thumbRowsPerPage = (float)pageHeight / thumbCellHeight;
    int n = dm->sf->rowCount();
/*    qDebug() << "objectName" << objectName()
             << "pageWidth" << pageWidth
             << "pageHeight" << pageHeight
             << "thumbCellWidth" << thumbCellWidth
             << "thumbCellHeight" << thumbCellHeight
             << "thumbRowsPerPage" << thumbRowsPerPage
             << "thumbsPerPage" << thumbsPerPage;  */
    float pages = (float(n) / thumbsPerPage) - 1;
    int vMax = pages * pageWidth;

//    Q_ASSERT(thumbCellWidth == 0);
//    Q_ASSERT(pageWidth == 0);
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

int IconView::getHorizontalScrollBarMax()
{
/*

*/
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    int pageWidth = viewport()->width();
    int thumbWidth = getCellSize().width();
    if (thumbWidth == 0) return 0;
    float thumbsPerPage = (double)pageWidth / thumbWidth;
    int n = dm->sf->rowCount();
    float pages = float(n) / thumbsPerPage - 1;
    float hMax = pages * pageWidth;
    return hMax;
}

int IconView::getVerticalScrollBarMax()
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
    int thumbCellWidth = getCellSize().width();
    int thumbCellHeight = getCellSize().height();

    if (thumbCellWidth == 0  || thumbCellHeight == 0 || pageWidth == 0 || pageHeight == 0) return 0;

    float thumbsPerPage = pageWidth / thumbCellWidth * (float)pageHeight / thumbCellHeight;
    if (thumbsPerPage == 0) return 0;
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

void IconView::wheelEvent(QWheelEvent *event)
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    QListView::wheelEvent(event);
}

void IconView::mousePressEvent(QMouseEvent *event)
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

void IconView::mouseMoveEvent(QMouseEvent *event)
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    if (isLeftMouseBtnPressed) isMouseDrag = true;
    QListView::mouseMoveEvent(event);
}

void IconView::mouseReleaseEvent(QMouseEvent *event)
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

void IconView::mouseDoubleClickEvent(QMouseEvent *event)
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
    scrollToRow(currentIndex().row());
}

void IconView::invertSelection()
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

void IconView::copyThumbs()        // rgh is this working?
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

void IconView::startDrag(Qt::DropActions)
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
