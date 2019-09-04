#include "Views/iconview.h"
#include "Main/mainwindow.h"

/*  IconView Overview

IconView manages the list of images within a folder and it's children (optional). The
thumbView can either be a QListView of file names or thumbnails. When a list item is selected
the image is shown in imageView.

The thumbView is inside a QDockWidget which allows it to dock and be moved and resized.

ThumbView does the following:

    Manages the file list of eligible images, including attributes for selected and picked.
    Picked files are shown in green and can be filtered and copied to another folder via the
    ingestDlg class.

    The thumbViewDelegate class formats the look of the thumbnails.

    Sorts the list based on date acquired, filename and forward/reverse

    Provides functions to navigate the list, including current index, next item, previous
    item, first item, last item and random item.

    Changes in selection trigger the MW::fileSelectionChange which loads the new selection in
    imageView and updates status.

    The mouse click location within the thumb is used in ImageView to pan a zoomed image.

QStandardItemModel roles used:

    1   DecorationRole - holds the thumbnail as an icon
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

Note that a "row" in this class refers to the row in the model, which has one thumb per row,
not the view in the dock, where there can be many thumbs per row

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

    The thumbDock dimensions are controlled by the size of its contents - the thumbView. When
    docked in bottom or top and thumb wrapping is false the maximum height is defined by
    thumbView->setMaximumHeight().

    ThumbDock resize to fit thumbs:

        This only occurs when the thumbDock is located in the top or bottom dock locations and
        wrap thumbs is not checked in prefDlg.

        When a relocation to the top or bottom occurs the height of the thumbDock is adjusted
        to fit the current size of the thumbs, depending on whether a scrollbar is required.
        It can also occur when a new folder is selected and the scrollbar requirement changes,
        depending on the number of images in the folder. This behavior is managed in
        MW::setThumbDockFeatures.

        Also, when thumbs are resized the height of the thumbDock is adjusted to accomodate
        the new thumb height.

    Thumb resize to fit in dock:

        This also only occurs when the thumbDock is located in the top or bottom dock
        locations and wrap thumbs is not checked in prefDlg. If the thumbDock horizontal
        splitter is dragged to make the thumbDock taller or shorter, within the minimum and
        maximum thumbView heights, the thumb sizes are adjusted to fit, factoring in the need
        for a scrollbar.

        This is triggered by MW::eventFilter() and effectuated in thumbsFit().

Loading icons

    At a minimum we want to load icons for all the visible cells in the IconView.  This is a
    little tricky to determine when the program is first opened if the thumbView is not
    visible.  Also, we do not know the best fit size of the thumbs in advance as this is only
    known after checking the aspect of each image in the folder.

    Consequently an incremental approach is taken in MdCache, where the metadata and icons are
    loaded.  A default number of icons (250) are loaded and the best aspect is determined.  The
    number of thumbs visible in the viewport (thumbs per page or tpp) are calculated.  If this
    is more than the number already read then the additional icons are read in a second pass.
*/

extern MW *m2;
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

    setViewMode(QListView::IconMode);
    setSelectionMode(QAbstractItemView::ExtendedSelection);
    setTabKeyNavigation(true);  // not working
    setResizeMode(QListView::Adjust);
//    setLayoutMode(QListView::Batched);    // causes delay that makes scrollTo a headache
    setVerticalScrollMode(QAbstractItemView::ScrollPerItem);
    verticalScrollBar()->setObjectName("VerticalScrollBar");
    horizontalScrollBar()->setObjectName("HorizontalScrollBar");
    setWordWrap(true);
    setDragEnabled(true);
    setEditTriggers(QAbstractItemView::NoEditTriggers);
    setSelectionBehavior(QAbstractItemView::SelectRows);
    setSelectionRectVisible(true);
    setUniformItemSizes(true);
    setMaximumHeight(100000);
    setContentsMargins(0,0,0,0);
    setSpacing(0);
    setLineWidth(0);
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);

    QFont f = font();
    f.setPixelSize(13);
    setFont(f);

    bestAspectRatio = 1;

    horizontalScrollBar()->setObjectName("IconViewHorizontalScrollBar");
    verticalScrollBar()->setObjectName("IconViewVerticalScrollBar");

    setModel(this->dm->sf);

    iconViewDelegate = new IconViewDelegate(this, m2->isRatingBadgeVisible);
    iconViewDelegate->setThumbDimensions(iconWidth, iconHeight,
        labelFontSize, showIconLabels, badgeSize);
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

QString IconView::diagnostics()
{
    QString reportString;
    QTextStream rpt;
    rpt.setString(&reportString);
    rpt << Utilities::centeredRptHdr('=', objectName() + " View Diagnostics");
    rpt << "\n";

    rpt << "\n" << "visible = " << G::s(isVisible());
    rpt << "\n" << "iconWidth = " << G::s(iconWidth);
    rpt << "\n" << "iconHeight = " << G::s(iconHeight);
    rpt << "\n" << "labelFontSize = " << G::s(labelFontSize);
    rpt << "\n" << "showIconLabels = " << G::s(showIconLabels);
    rpt << "\n" << "badgeSize = " << G::s(badgeSize);
    rpt << "\n" << "scrollWhenReady = " << G::s(scrollWhenReady);
    rpt << "\n" << "assignedIconWidth = " << G::s(assignedIconWidth);
    rpt << "\n" << "skipResize = " << G::s(skipResize);
    rpt << "\n" << "bestAspectRatio = " << G::s(bestAspectRatio);
    rpt << "\n" << "firstVisibleRow = " << G::s(firstVisibleCell);
    rpt << "\n" << "midVisibleRow = " << G::s(midVisibleCell);
    rpt << "\n" << "lastVisibleRow = " << G::s(lastVisibleCell);
    rpt << "\n" << "thumbsPerPage = " << G::s(visibleCells);
    rpt << "\n\n" ;
    rpt << iconViewDelegate->diagnostics();
    rpt << "\n\n" ;
    return reportString;
}

void IconView::move()
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    moveCursor(QAbstractItemView::MovePageDown, Qt::NoModifier);
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
    /* MUST set thumbdock height to fit thumbs BEFORE updating the delegate.  If not, and the
       thumbView cells are taller than the dock then QIconview::visualRect is not calculated
       correctly, and scrolling does not work properly */
    if(objectName() == "Thumbnails") {
        if (!m2->thumbDock->isFloating())
            m2->setThumbDockHeight();
    }
    setSpacing(0);
    iconViewDelegate->setThumbDimensions(iconWidth, iconHeight,
        labelFontSize, showIconLabels, badgeSize);

    if(objectName() == "Thumbnails") {
        if (!m2->thumbDock->isFloating())
            emit updateThumbDockHeight();
    }
}

void IconView::setThumbParameters(int _thumbWidth, int _thumbHeight, /*int _thumbPadding,*/
                                  int _labelFontSize, bool _showThumbLabels, int _badgeSize)
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    iconWidth = _thumbWidth;
    iconHeight = _thumbHeight;
//    iconPadding = _thumbPadding;
    labelFontSize = _labelFontSize;
    showIconLabels = _showThumbLabels;
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
    float aspect = iconWidth / iconHeight;
    // Difference between thumbSpace and thumbHeight
    int margin = iconViewDelegate->getCellSize().height() - iconHeight;
    int newThumbHeight = thumbSpaceHeight - margin;
    int newThumbWidth = static_cast<int>(newThumbHeight * aspect);
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

uint IconView::getRandomRow()
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    return QRandomGenerator::global()->generate() % static_cast<uint>(dm->sf->rowCount());
}

// rgh not being used
bool IconView::isSelectedItem()
{
    // call before getting current row or index
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    if (selectionModel()->selectedRows().size() > 0)
        return true;
    else
        return false;
}

bool IconView::calcViewportRange(int sfRow)
{
/*
Determine the first and last visible icons in the IconView viewport for any item in the
datamodel. Scrolling is set to center the current item in the viewport. There are three
scenarios to consider:

1.  At the start the top of the viewport = the top of the fisrt thumbnail.

2.  At the end the bottom = the bottom of the last thumbnail.

3.  In between the start and end the top of the viewport can = a fractional part of a thumbnail
    and the same for the bottom of the viewport.

Scenario 3 occurs when the viewport has to scroll to center the current thumbnail.

So we have to determine when scrolling will first occur, how many rows of icons will be
visible in the viewport, how many icons are visible and the first/last icons visible.
*/
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    int i = sfRow;
    QSize vp = viewport()->size();
    int rowWidth = vp.width() - G::scrollBarThickness;
    QSize thumb(iconWidth, iconHeight);
    QSize cell = iconViewDelegate->getCellSize(thumb);
    if (cell.width() == 0) return false;
    if (cell.height() == 0) return false;

    int tCells = dm->sf->rowCount();
    int cellsPerVPRow = qCeil(static_cast<qreal>(rowWidth) / cell.width());

    if (!isWrapping()) {
        // simple approach, will report more than visible, does not consider scroll alignment
        firstVisibleCell = i - cellsPerVPRow;
        if (firstVisibleCell < 0) firstVisibleCell = 0;
        lastVisibleCell = i + cellsPerVPRow;
        if (lastVisibleCell > tCells - 1) lastVisibleCell = tCells - 1;

        visibleCells = lastVisibleCell - firstVisibleCell + 1;
        midVisibleCell = firstVisibleCell + visibleCells / 2;

/*        qDebug()
                 << __FUNCTION__
                 << "i:" << i
                 << "firstVisibleCell =" << firstVisibleCell
                 << "lastVisibleCell =" << lastVisibleCell;
                 */
        return true;
    }

    int rowInView = i / cellsPerVPRow;  //qCeil(i / cellsPerVPRow);
//    int posInVPRow = i % cellsPerVPRow;
    int rowsInView = qCeil(static_cast<qreal>(tCells) / cellsPerVPRow);
    int lastVPRow = qCeil(tCells / cellsPerVPRow);

    // rows per viewport
    qreal rowsPerVPDbl = 1.0;
    rowsPerVPDbl = static_cast<qreal>(vp.height()) / cell.height();
    if (rowsPerVPDbl < 1.0) rowsPerVPDbl = 1.0;
    int firstVPRowScrollReq = qCeil(rowsPerVPDbl / 2);
    int lastVRowReqScroll = lastVPRow - firstVPRowScrollReq;
    int rowsPerVP = qCeil(rowsPerVPDbl);
    if (rowInView >= firstVPRowScrollReq && rowInView <= lastVRowReqScroll)
        rowsPerVP += 1;

    visibleCells = cellsPerVPRow * rowsPerVP;
    int visibleRowsAbove = qCeil((rowsPerVP - 1) / 2);

    // vertical alignment is centered, first cell requiring the view to scroll
    int firstCellReqScroll = qCeil(static_cast<qreal>(rowsPerVP) / 2) * cellsPerVPRow;

    // last cell requiring the view to scroll
    int lastCellReqScroll = lastVRowReqScroll * cellsPerVPRow;

    // first visible cell
    int firstVisibleVPRow = 0;
    if (i < firstCellReqScroll) {
         firstVisibleVPRow = 0;
    }
    if (i > lastCellReqScroll) {
        firstVisibleVPRow = rowsInView - rowsPerVP;
    }
    if (i >= firstCellReqScroll && i <= lastCellReqScroll) {
        firstVisibleVPRow = rowInView - visibleRowsAbove;
    }

    firstVisibleCell = firstVisibleVPRow * cellsPerVPRow;
    lastVisibleCell = firstVisibleCell + visibleCells - 1;
    visibleCells = lastVisibleCell - firstVisibleCell + 1;
    midVisibleCell = firstVisibleCell + visibleCells / 2;

/*    qDebug()
             << __FUNCTION__
             << "i:" << i
             << "row =" << rowInView
             << "rows =" << rowsInView
             << "viewport:" << vp << "cell:" << cell
             << "firstVisibleVPRow =" << firstVisibleVPRow
             << "firstVisibleCell =" << firstVisibleCell
             << "lastVisibleCell =" << lastVisibleCell
             << "cellsPerVPRow =" << cellsPerVPRow
             << "rowsPerVP =" << rowsPerVPDbl << rowsPerVP
             << "visibleCells ="  << visibleCells
             << "posInVPRow =" << posInVPRow
             << "rowInView =" << rowInView
             << "firstVPRowScrollReq =" << firstVPRowScrollReq
             << "lastVRowReqScroll =" << lastVRowReqScroll
             << "firstCellReqScroll =" << firstCellReqScroll
             << "lastCellReqScroll =" << lastCellReqScroll
                ;
*/
    return true;
}

int IconView::getThumbsPerPage()
{
/*

*/
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    QString obj = objectName();
    QSize vp = viewport()->size();
//    if ((vp.width() == 0 || vp.height()) == 0 && objectName() == "Grid")
//        vp = m2->centralWidget->size();
    int rowWidth = vp.width() - G::scrollBarThickness;
    QSize thumb(iconWidth, iconHeight);
    QSize cell = iconViewDelegate->getCellSize(thumb);

    // thumbs per row
    if (cell.width() == 0) return 0;
    double tprDbl = static_cast<double>(rowWidth) / cell.width();
    int tpr = rowWidth / cell.width();
    if (tprDbl > tpr + .05) tpr += 1;

    // rows per page (page = viewport)
    if (cell.height() == 0) return 0;
    double rppDbl = static_cast<double>(vp.height()) / cell.height();
    if (rppDbl < 1) rppDbl = 1;
    // get the remainder, if significant add possibility of 2 extra rows
    int rpp = static_cast<int>(rppDbl);
    if (rpp - rppDbl < -0.05) rpp += 2;

    // thumbs per page
    visibleCells = tpr * rpp;
    /*qDebug() << __FUNCTION__ << objectName()
             << "G::isInitializing" << G::isInitializing
             << "| G::isNewFolderLoaded" << G::isNewFolderLoaded
             << "| isVisible = " << isVisible()
             << "| page size =" << vp
             << "| cell size =" << cell
             << "| tpr =" << tpr
             << "| rpp =" << rpp
             << "| thumbsPerPage" << thumbsPerPage;*/
    return visibleCells;
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

bool IconView::allPageIconsLoaded()
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    for (int row = firstVisibleCell; row < dm->sf->rowCount(); ++row) {
        if (dm->index(row, G::PathColumn).data(Qt::DecorationRole).isNull())
            return false;
    }
    return true;
}

void IconView::scannedViewportRange()
{
/*
Set the firstVisibleRow, midVisible, RowlastVisibleRow and thumbsPerPage. This is called when
the application show event occurs, when there is a viewport scroll event or when an icon
justification happens.
*/
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    int row;    // an item, not a row in the grid
    firstVisibleCell = 0;
    QRect iconViewRect = viewport()->rect();
    for (row = 0; row < dm->sf->rowCount(); ++row) {
        if (visualRect(dm->sf->index(row, 0)).intersects(iconViewRect)) {
            firstVisibleCell = row;
            break;
        }
    }
    for (row = firstVisibleCell; row < dm->sf->rowCount(); ++row) {
        if (!visualRect(dm->sf->index(row, 0)).intersects(iconViewRect)) {
            lastVisibleCell = row - 1;
            break;
        }
        lastVisibleCell = dm->sf->rowCount() - 1;
    }
    visibleCells = lastVisibleCell - firstVisibleCell + 1;
    midVisibleCell = firstVisibleCell + visibleCells / 2;


/*    qDebug() << __FUNCTION__ << objectName().leftJustified(10, ' ')
             << "isInitializing =" << G::isInitializing
             << "isVisible =" << isVisible()
             << "firstVisibleRow =" << firstVisibleCell
             << "lastVisibleRow =" << lastVisibleCell
             << "midVisibleRow =" << midVisibleCell
             << "thumbsPerPage =" << visibleCells;
*/
}

bool IconView::isRowVisible(int row)
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
//    setViewportParameters();
//    calcViewportRange(row);
    return row >= firstVisibleCell && row <= lastVisibleCell;
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

// Used by thumbnail navigation (left, right, up, down etc)
void IconView::selectThumb(QModelIndex idx)
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    if (idx.isValid()) {
        setCurrentIndex(idx);
//        selectionModel()->setCurrentIndex(idx, QItemSelectionModel::SelectCurrent);
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
//    qDebug() << __FUNCTION__ << "row =" << row;
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
    // rgh use dm->fPathRow[fPath] instead - much faster
    // selectThumb(dm->fPathRow[fName]);
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
    qDebug() << __FUNCTION__;
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
    qDebug() << __FUNCTION__;
//    if (G::mode == "Table" || !isWrapping()) selectNext();
    /*else */setCurrentIndex(moveCursor(QAbstractItemView::MoveDown, Qt::NoModifier));
}

void IconView::selectPageUp()
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    qDebug() << __FUNCTION__;
//    if (G::mode == "Table" || !isWrapping()) selectPrev();
    /*else */setCurrentIndex(moveCursor(QAbstractItemView::MovePageUp, Qt::NoModifier));
}

void IconView::selectPageDown()
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    qDebug() << __FUNCTION__;
    if (G::mode == "Table" || !isWrapping()) selectNext();
    else setCurrentIndex(moveCursor(QAbstractItemView::MovePageDown, Qt::NoModifier));
}

void IconView::selectFirst()
{
    {
    #ifdef ISDEBUG
        G::track(__FUNCTION__);
    #endif
    }
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
//    qDebug() << "\n***************************************************************************\n" << __FUNCTION__;
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
    if (iconWidth < ICON_MIN) iconWidth = ICON_MIN;
    if (iconHeight < ICON_MIN) iconHeight = ICON_MIN;
    if (iconWidth < G::maxIconSize && iconHeight < G::maxIconSize)
    {
        iconWidth *= 1.1;
        iconHeight *= 1.1;
        if (iconWidth > G::maxIconSize) iconWidth = G::maxIconSize;
        if (iconHeight > G::maxIconSize) iconHeight = G::maxIconSize;
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
    if (iconWidth > ICON_MIN  && iconHeight > ICON_MIN) {
        iconWidth *= 0.9;
        iconHeight *= 0.9;
        if (iconWidth < ICON_MIN) iconWidth = ICON_MIN;
        if (iconHeight < ICON_MIN) iconHeight = ICON_MIN;
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
    if (assignedIconWidth < 40 || assignedIconWidth > 480) assignedIconWidth = iconWidth;
    int wCell = iconViewDelegate->getCellWidthFromThumbWidth(assignedIconWidth);

    if (wCell == 0) return;
    int tpr = wRow / wCell;

    if (tpr == 0) return;
    wCell = wRow / tpr;

    iconWidth = iconViewDelegate->getThumbWidthFromCellWidth(wCell);
    iconHeight = iconWidth * bestAspectRatio;

    qDebug() << __FUNCTION__ << objectName()
             << "assignedIconWidth =" << assignedIconWidth
             << "wRow =" << wRow
             << "wCell =" << wCell
             << "tpr =" << tpr
             << "iconWidth =" << iconWidth
             << "iconHeight =" << iconHeight;

    skipResize = true;      // prevent feedback loop

    setThumbParameters();
//    setViewportParameters();
    calcViewportRange(currentIndex().row());

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

   The variable assignedThumbWidth remembers the current grid cell size as a reference to
   maintain the grid size during resizing and pad size changes in preferences.

   JustifyAction:
        Shrink = 1,
        Enlarge = -1

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

    iconWidth = iconViewDelegate->getThumbWidthFromCellWidth(wCell);
    iconHeight = iconWidth * bestAspectRatio;

    assignedIconWidth = iconWidth;
    skipResize = true;      // prevent feedback loop

    setThumbParameters();
//    setViewportParameters();
    calcViewportRange(currentIndex().row());

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
/*
The resizeEvent can be triggered by a change in the gridView cell size (thumbWidth) that
requires justification or by a change in the thumbDock splitter. That is filtered by checking
if the ThumbView width has changed or skipResize has been set.

This event is not forwarded to QListView::resize.  This would cause multiple scroll events
which isn't pretty at all.
*/
    event->ignore();    // suppress compiler warning
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    int mid = midVisibleCell;

    static int prevWidth = 0;

    // must come after width parameters
    if (G::isInitializing || !G::isNewFolderLoaded || m2->gridDisplayFirstOpen) {
        prevWidth = width();
        return;
    }

    // only rejustify when user resizes
    if (isWrapping() && width() != prevWidth) {
        QTimer::singleShot(500, this, SLOT(rejustify()));   // calls calcViewportParameters
    }
    prevWidth = width();

    if (isFitTopOrBottom) {
        G::ignoreScrollSignal = true;
//        qDebug() << __FUNCTION__ << mid << midVisibleCell;
        scrollToRow(mid, __FUNCTION__);
        isFitTopOrBottom = false;
        calcViewportRange(mid);
    }
    else {
        m2->numberIconsVisibleChange();
    }
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
    if (iconWidth > G::maxIconSize) iconWidth = G::maxIconSize;
    if (iconHeight > G::maxIconSize) iconHeight = G::maxIconSize;
    if (iconWidth < G::minIconSize) iconWidth = G::minIconSize;
    if (iconHeight < G::minIconSize) iconHeight = G::minIconSize;

//    G::iconWMax = 0;
//    G::iconHMax = 0;
//    for (int row = 0; row < dm->rowCount(); ++row) {
//        QModelIndex idx = dm->index(row, 0);
//        if (idx.data(Qt::DecorationRole).isNull()) continue;
//        QPixmap pm = dm->itemFromIndex(idx)->icon().pixmap(G::maxIconSize);
//        if (G::iconWMax < pm.width()) G::iconWMax = pm.width();
//        if (G::iconHMax < pm.height()) G::iconHMax = pm.height();
//        if (G::iconWMax == G::maxIconSize && G::iconHMax == G::maxIconSize) break;
//    }

    if (G::iconWMax == G::iconHMax && iconWidth > iconHeight)
        iconHeight = iconWidth;
    if (G::iconWMax == G::iconHMax && iconHeight > iconWidth)
        iconWidth = iconHeight;
    if (G::iconWMax > G::iconHMax) iconHeight = iconWidth * ((double)G::iconHMax / G::iconWMax);
    if (G::iconHMax > G::iconWMax) iconWidth = iconHeight * ((double)G::iconWMax / G::iconHMax);

    setThumbParameters();

    bestAspectRatio = (double)iconHeight / iconWidth;
    /*
    qDebug() << __FUNCTION__
             << "G::iconWMax =" << G::iconWMax
             << "G::iconHMax =" << G::iconHMax;*/
}

//void IconView::thumbsFit(Qt::DockWidgetArea area)
//{
//    {
//    #ifdef ISDEBUG
//    G::track(__FUNCTION__);
//    #endif
//    }
//    qDebug() << __FUNCTION__;
//    if (G::mode == "Grid") {
//        return;
//    }
//    // all wrapping is row wrapping
//    if (isWrapping()) {
//        return;

//        // adjust thumb width
//        int scrollWidth = G::scrollBarThickness;
//        int width = viewport()->width() - scrollWidth - 2;
//        int thumbCellWidth = iconViewDelegate->getCellSize().width() - iconPadding * 2;
//        int rightSideGap = 99999;
//        iconPadding = 0;
//        int remain;
//        int padding = 0;
//        bool improving;
//        do {
//            improving = false;
//            int cellWidth = thumbCellWidth + padding * 2;
//            remain = width % cellWidth;
//            if (remain < rightSideGap) {
//                improving = true;
//                rightSideGap = remain;
//                iconPadding = padding;
//            }
//            padding++;
//        } while (improving);
//    }
//    // no wrapping - must be bottom or top dock area
//    else if (area == Qt::BottomDockWidgetArea || area == Qt::TopDockWidgetArea
//             || !isWrapping()){
//        // set target ht based on space with scrollbar (always on)
//        int ht = height();
//        int scrollHeight = G::scrollBarThickness;
//        ht -= scrollHeight;

//        // adjust thumb height
//        float aspect = iconWidth / iconHeight;

//        // get the current thumb cell
//        int cellHeight = iconViewDelegate->getCellSize().height();

//        // padding = noncell space is used to rebuild cell after thumb resize to fit
//        int padding = cellHeight - iconHeight;
//        int maxCellHeight = iconViewDelegate->getCellSize(QSize(G::iconWMax, G::iconHMax)).height();
//        cellHeight = ht < maxCellHeight ? ht : maxCellHeight;
//        iconHeight = cellHeight - padding;
//        iconWidth = iconHeight * aspect;

//        // change the thumbnail size in thumbViewDelegate
//        setSpacing(0);
//        iconViewDelegate->setThumbDimensions(iconWidth, iconHeight,
//            labelFontSize, showIconLabels, badgeSize);
//    }
//}

void IconView::thumbsFitTopOrBottom()
{
/*
Called by MW::eventFilter when a thumbDock resize event occurs triggered by the user resizing
the thumbDock. The thumb size is adjusted to fit the new thumbDock height and scrolled to keep
the midVisibleThumb in the middle. Other objects visible (docks and central widget) are
resized.

For icon cell anatomy (see diagram at top of IconViewDelegate)
*/
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    /* set here, clear in resize.  Used to flag when to just scroll the thumbView when the
    thumbdock splitter triggers this function and when the resize event is from another event.
    */
    isFitTopOrBottom = true;

    float aspect = (float)iconWidth / iconHeight;

    // viewport available height
    int newViewportHt = height() - G::scrollBarThickness;
    qDebug() << "\n" << __FUNCTION__ << "viewportHeight =" << newViewportHt;
    int hMax = iconViewDelegate->getCellHeightFromThumbHeight(G::maxIconSize * bestAspectRatio);
    int hMin = iconViewDelegate->getCellHeightFromThumbHeight(ICON_MIN);

    // restrict icon cell height within limits
    int newThumbSpaceHt = newViewportHt > hMax ? hMax : newViewportHt;
    newThumbSpaceHt = newThumbSpaceHt < hMin ? hMin : newThumbSpaceHt;

    // derive new thumbsize from new thumbSpace
    iconHeight = iconViewDelegate->getCellHeightFromAvailHeight(newThumbSpaceHt);

    // make sure within range (should be from thumbSpace check but just to be sure)
    iconHeight = iconHeight > G::maxIconSize ? G::maxIconSize : iconHeight;
    iconHeight = iconHeight < ICON_MIN ? ICON_MIN : iconHeight;
    iconWidth = iconHeight * aspect;

    // check thumbWidth within range
    if(iconWidth > G::maxIconSize) {
        iconWidth = G::maxIconSize;
        iconHeight = G::maxIconSize / aspect;
    }

    // this is critical - otherwise thumbs bunch up
    setSpacing(0);

    // this will change the icon size, which will triger the resize evcent
    iconViewDelegate->setThumbDimensions(iconWidth, iconHeight,
        labelFontSize, showIconLabels, badgeSize);
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
        verticalScrollBar()->triggerAction(QAbstractSlider::SliderSingleStepAdd);
    }
    else {
        horizontalScrollBar()->triggerAction(QAbstractSlider::SliderSingleStepAdd);
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
        verticalScrollBar()->triggerAction(QAbstractSlider::SliderSingleStepSub);
    }
    else {
        horizontalScrollBar()->triggerAction(QAbstractSlider::SliderSingleStepSub);
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
        verticalScrollBar()->triggerAction(QAbstractSlider::SliderPageStepAdd);
    }
    else {
        horizontalScrollBar()->triggerAction(QAbstractSlider::SliderPageStepAdd);
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
        verticalScrollBar()->triggerAction(QAbstractSlider::SliderPageStepSub);
    }
    else {
        horizontalScrollBar()->triggerAction(QAbstractSlider::SliderPageStepSub);
    }
}

void IconView::scrollToRow(int row, QString source)
{
/*
This is called to scroll to the current image or to sync the other views (thumbView, gridView
and tableView). When the user switches views (loupe, grid and table) the thumbView, gridView
or tableView is made visible. Hidden widgets cannot be updated. It takes time for the view to
be rendered, so we have to query it to make sure it is ready to accept a scrollto request.
This is done by testing the actual maximum size of the scrollbars compared to a calculated
amount in the okToScroll function.

source is the calling function and is used for debugging.
*/
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }

//    qDebug() << __FUNCTION__ << objectName() << "row =" << row
//             << "requested by" << source;

    source = "";    // suppress compiler warning
    QModelIndex idx = dm->sf->index(row, 0);
    scrollTo(idx, QAbstractItemView::PositionAtCenter);
}

bool IconView::waitUntilOkToScroll()
{
/*
Returns true when the scrollbars have been fully rendered.
*/
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    QTime t = QTime::currentTime().addMSecs(1000);
    while (QTime::currentTime() < t) {
        if (okToScroll()) {
            G::wait(250);
            return true;
        }
        qApp->processEvents(QEventLoop::AllEvents, 50);
    }
    return false;
}

bool IconView::okToScroll()
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    if (objectName() == "Thumbnails") {
        /*
        qDebug() << __FUNCTION__ << objectName()
                 << "horizontalScrollBar()->maximum() =" << horizontalScrollBar()->maximum()
                 << "getHorizontalScrollBarMax() = " << getHorizontalScrollBarMax();
        */
        return horizontalScrollBar()->maximum() > 0.95 * getHorizontalScrollBarMax();
    }
    else {
        return verticalScrollBar()->maximum() > 0.95 * getVerticalScrollBarMax();
    }
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
    if(G::mode == "Grid") G::fileSelectionChangeSource =  "GridMouseClick";
    else G::fileSelectionChangeSource =  "ThumbMouseClick";

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

    // must finish event to update current index in iconView
    QListView::mousePressEvent(event);

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
        /*
        qDebug() << __FUNCTION__ << iconRect << mousePt << iconPt << xPct << yPct;*/
        if (xPct >= 0 && xPct <= 1 && yPct >= 0 && yPct <=1) {
            //signal sent to ImageView
            emit thumbClick(xPct, yPct);
        }
    }

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



//QModelIndex IconView::moveCursor(QAbstractItemView::CursorAction cursorAction,
//                                Qt::KeyboardModifiers modifiers)
//{
//    QModelIndex idx = QListView::moveCursor(cursorAction, modifiers);
//    qDebug() << __FUNCTION__ << cursorAction << modifiers << idx;
////    QModelIndex idx = QAbstractItemView::moveCursor(cursorAction, modifiers);
//    setCurrentIndex(idx);
//    return idx;

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
    scrollToRow(currentIndex().row(), __FUNCTION__);
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

    for (int i = 0; i < selection.count(); ++i) {
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
