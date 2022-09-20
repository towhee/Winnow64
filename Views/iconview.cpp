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

    Behavior is controlled in preferences

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

IconView::IconView(QWidget *parent, DataModel *dm, ImageCacheData *icd, QString objName)
    : QListView(parent)
{
    if (G::isLogger) G::log(CLASSFUNCTION);
    this->dm = dm;
    this->icd = icd;
    setObjectName(objName);

    // this works because ThumbView is a friend class of MW.  It is used in the
    // event filter to access the thumbDock
    m2 = qobject_cast<MW*>(parent);

    setViewMode(QListView::IconMode);
    setSelectionMode(QAbstractItemView::ExtendedSelection);//
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

    bestAspectRatio = 1;

    horizontalScrollBar()->setObjectName("IconViewHorizontalScrollBar");
    verticalScrollBar()->setObjectName("IconViewVerticalScrollBar");

    setModel(this->dm->sf);

    iconViewDelegate = new IconViewDelegate(this, m2->isRatingBadgeVisible, icd);
    iconViewDelegate->setThumbDimensions(iconWidth, iconHeight,
        labelFontSize, showIconLabels, labelChoice, badgeSize);
    setItemDelegate(iconViewDelegate);

    zoomFrame = new QLabel;

    assignedIconWidth = iconWidth;
    prevIdx = model()->index(-1, -1);

    // used to provide iconRect info to zoom to point clicked on thumb
    // in imageView
    connect(iconViewDelegate, SIGNAL(update(QModelIndex, QRect)),
            this, SLOT(updateThumbRectRole(QModelIndex, QRect)));
    connect(this, &IconView::setValueSf, dm, &DataModel::setValueSf);
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
    if (G::isLogger) G::log(CLASSFUNCTION);
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
    rpt << "\n" << "gridViewLabelChoice = " << G::s(labelChoice);
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
    if (G::isLogger) G::log(CLASSFUNCTION);
    moveCursor(QAbstractItemView::MovePageDown, Qt::NoModifier);
}

void IconView::refreshThumb(QModelIndex idx, int role)
{
    if (G::isLogger) G::log(CLASSFUNCTION);
    QVector<int> roles;
    roles.append(role);
    dataChanged(idx, idx, roles);
}

void IconView::refreshThumbs() {
    if (G::isLogger) G::log(CLASSFUNCTION);
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

    Both instances update the delegate. If it is the "Thumbnails" IconView instance then
    MW::setThumbDockHeight is called (friend class) so it can be resized to fit the
    possibly altered thumbnail dimensions.
*/
    if (G::isLogger) G::log(CLASSFUNCTION);
    /* MUST set thumbdock height to fit thumbs BEFORE updating the delegate.  If not, and the
       thumbView cells are taller than the dock then QIconview::visualRect is not calculated
       correctly, and scrolling does not work properly */
    if(objectName() == "Thumbnails") {
        if (!m2->thumbDock->isFloating())
            m2->setThumbDockHeight();
    }
    setSpacing(0);
    if (labelFontSize == 0) labelFontSize = 10;
    iconViewDelegate->setThumbDimensions(iconWidth, iconHeight,
        labelFontSize, showIconLabels, labelChoice, badgeSize);
    /*
    qDebug() << CLASSFUNCTION
             << "iconWidth =" << iconWidth
             << "iconHeight =" << iconHeight
                ;
                //*/
}

void IconView::setThumbParameters(int _thumbWidth, int _thumbHeight, /*int _thumbPadding,*/
                                  int _labelFontSize, bool _showThumbLabels, int _badgeSize)
{
    if (G::isLogger) G::log(CLASSFUNCTION);
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
    if (G::isLogger) G::log(CLASSFUNCTION);
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
    if (G::isLogger) G::log(CLASSFUNCTION);
    float aspect = iconWidth / iconHeight;
    // Difference between thumbSpace and thumbHeight
    int margin = iconViewDelegate->getCellSize().height() - iconHeight;
    int newThumbHeight = thumbSpaceHeight - margin;
    int newThumbWidth = static_cast<int>(newThumbHeight * aspect);
    return newThumbWidth + margin - 1;
}

int IconView::getScrollThreshold(int thumbSpaceHeight)
{
    if (G::isLogger) G::log(CLASSFUNCTION);
    return viewport()->width() / getThumbSpaceWidth(thumbSpaceHeight);
}

// debugging
void IconView::reportThumb()
{
    if (G::isLogger) G::log(CLASSFUNCTION);
    int currThumb = currentIndex().row();
    qDebug() << G::t.restart() << "\t" << "\n ***** THUMB INFO *****";
    qDebug() << G::t.restart() << "\t" << "Row =" << currThumb;
    qDebug() << G::t.restart() << "\t" << "FileNameRole " << G::PathRole << dm->item(currThumb)->data(G::PathRole).toString();

}

int IconView::getCurrentRow()
{
    if (G::isLogger) G::log(CLASSFUNCTION);
    return currentIndex().row();
}

int IconView::getNextRow()
{
    if (G::isLogger) G::log(CLASSFUNCTION);
    int row = currentIndex().row();
    if (row == dm->sf->rowCount() - 1)
        return row;
    return row + 1;
}

int IconView::getPrevRow()
{
    if (G::isLogger) G::log(CLASSFUNCTION);
    int row = currentIndex().row();
    if (row == 0)
        return 0;
    return row - 1;
}

int IconView::getLastRow()
{
    if (G::isLogger) G::log(CLASSFUNCTION);
    return dm->sf->rowCount() - 1;
}

uint IconView::getRandomRow()
{
    if (G::isLogger) G::log(CLASSFUNCTION);
    return QRandomGenerator::global()->generate() % static_cast<uint>(dm->sf->rowCount());
}

// rgh not being used
bool IconView::isSelectedItem()
{
    // call before getting current row or index
    if (G::isLogger) G::log(CLASSFUNCTION);
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

1.  At the start the top of the viewport = the top of the first thumbnail.

2.  At the end the bottom = the bottom of the last thumbnail.

3.  In between the start and end the top of the viewport can be a fractional part of a
    thumbnail and the same for the bottom of the viewport.

Scenario 3 occurs when the viewport has to scroll to center the current thumbnail.

So we have to determine when scrolling will first occur, how many rows of icons will be
visible in the viewport, how many icons are visible and the first/last icons visible.
*/
    if (G::isLogger || G::isFlowLogger) G::log(CLASSFUNCTION);
//    qDebug() << CLASSFUNCTION << "sfRow =" << sfRow;
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
                 << CLASSFUNCTION
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

    /*
    qDebug() << CLASSFUNCTION
             << "cellsPerVPRow =" << cellsPerVPRow
             << "rowsPerVP =" << rowsPerVP
                ;
//                */

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

    /*
    qDebug()
        << CLASSFUNCTION
        << "\n\t\ti:" << i
        << "\n\t\trow =" << rowInView
        << "\n\t\trows =" << rowsInView
        << "\n\t\tviewport:" << vp << "cell:" << cell
        << "\n\t\tfirstVisibleVPRow =" << firstVisibleVPRow
        << "\n\t\tfirstVisibleCell =" << firstVisibleCell
        << "\n\t\tlastVisibleCell =" << lastVisibleCell
        << "\n\t\tcellsPerVPRow =" << cellsPerVPRow
        << "\n\t\trowsPerVP =" << rowsPerVPDbl << rowsPerVP
        << "\n\t\tvisibleCells ="  << visibleCells
//        << "posInVPRow =" << posInVPRow
        << "\n\t\trowInView =" << rowInView
        << "\n\t\tfirstVPRowScrollReq =" << firstVPRowScrollReq
        << "\n\t\tlastVRowReqScroll =" << lastVRowReqScroll
        << "\n\t\tfirstCellReqScroll =" << firstCellReqScroll
        << "\n\t\tlastCellReqScroll =" << lastCellReqScroll
        ;
//    */
    return true;
}

int IconView::getThumbsPerPage()
{
/*

*/
    if (G::isLogger) G::log(CLASSFUNCTION);
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
    /*qDebug() << CLASSFUNCTION << objectName()
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
    if (G::isLogger) G::log(CLASSFUNCTION);
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
    if (G::isLogger) G::log(CLASSFUNCTION);
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
    if (G::isLogger) G::log(CLASSFUNCTION);
    for (int row = firstVisibleCell; row < dm->sf->rowCount(); ++row) {
        if (dm->index(row, G::PathColumn).data(Qt::DecorationRole).isNull())
            return false;
    }
    return true;
}

void IconView::scannedViewportRange()
{
/*
    Set the firstVisibleRow, midVisibleRow, lastVisibleRow and thumbsPerPage. This is called
    when the application show event occurs, when there is a viewport scroll event or when an
    icon justification happens.
*/
    if (G::isLogger) G::log(CLASSFUNCTION);
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
    /*
   qDebug() << CLASSFUNCTION << objectName().leftJustified(10, ' ')
             << "isInitializing =" << G::isInitializing
             << "isVisible =" << isVisible()
             << "firstVisibleRow =" << firstVisibleCell
             << "lastVisibleRow =" << lastVisibleCell
             << "midVisibleRow =" << midVisibleCell
             << "thumbsPerPage =" << visibleCells;
//            */
}

bool IconView::isRowVisible(int row)
{
    if (G::isLogger) G::log(CLASSFUNCTION);
//    setViewportParameters();
//    calcViewportRange(row);
    return row >= firstVisibleCell && row <= lastVisibleCell;
}

QString IconView::getCurrentFilePath()
{
    if (G::isLogger) G::log(CLASSFUNCTION);
    return currentIndex().data(G::PathRole).toString();
}

// PICKS: Items that have been picked

// used in MW::ingests
bool IconView::isPick()
{
    if (G::isLogger) G::log(CLASSFUNCTION);
    for (int row = 0; row < dm->sf->rowCount(); ++row) {
        QModelIndex idx = dm->sf->index(row, G::PickColumn);
        if (idx.data(Qt::EditRole).toString() == "true") return true;
    }
    return false;
}

QFileInfoList IconView::getPicks()
{
/*
    Returns a list of all the files that have been picked.  It is used in
    MW, passing the list on to the ingestDlg for ingestion/copying to another
    folder.
*/
    if (G::isLogger) G::log(CLASSFUNCTION);
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
    if (G::isLogger) G::log(CLASSFUNCTION);
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
    if (G::isLogger) G::log(CLASSFUNCTION);
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
    if (G::isLogger) G::log(CLASSFUNCTION);
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

QModelIndex IconView::getNearestSelection(int row)
{
    /* Returns the model index of the closest selected item */
    if (G::isLogger) G::log(CLASSFUNCTION);

    int frwd = row;
    int back = frwd;
    int rowCount = dm->sf->rowCount();
    QModelIndex idx;
    while (back >= 0 || frwd < rowCount) {
        if (back >= 0) idx = dm->sf->index(back, 0);
        if (selectedIndexes().contains(idx)) return idx;
        if (frwd < rowCount) idx = dm->sf->index(frwd, 0);
        if (selectedIndexes().contains(idx)) return idx;
        --back;
        ++frwd;
    }
    return idx;
}

void IconView::sortThumbs(int sortColumn, bool isReverse)
{
    if (G::isLogger || G::isFlowLogger) G::log(CLASSFUNCTION);
    if (isReverse) dm->sf->sort(sortColumn, Qt::DescendingOrder);
    else dm->sf->sort(sortColumn, Qt::AscendingOrder);

    scrollTo(currentIndex(), ScrollHint::PositionAtCenter);
}

void IconView::selectionChanged(const QItemSelection &selected, const QItemSelection &deselected)
{
/*
    The selection change has already occurred when selectionChanged is signalled, and
    the current index has been changed to the most recent selected or deselected item.

    However, we do not want to change the current index for a deselect in a
    muli-selection, and the current index is reset to its prior item. Since
    MW::fileSelectionChange has not been called MW::currSfRow and MW::currSfIdx contain
    the prior current row and index. The current index is reset using the
    QItemSelectionModel::NoUpdate flag to prevent another selectionChange being
    triggered.

    If the only item selected is deselected then the item is reselected as the current
    index must always be selected.

    If there is a new selection without cmd/ctrl then it is the new current index and
    MW::fileSelectionChange is signalled.

    For some reason the selectionModel rowCount is not up-to-date and the selection is
    updated after the MD::fileSelectionChange occurs, hence update the status bar from
    here.
*/

    if (!G::isInitializing) {
        // selection behavior
        bool anchorCurrent = true;
        // select or deselect change
        bool isSelected = !selected.isEmpty();
        bool isDeselected = !deselected.isEmpty();
        // changed row
        int deselectedRow = -1;
        int selectedRow = -1;
        if (isDeselected) deselectedRow = deselected.at(0).indexes().at(0).row();
        if (isSelected) selectedRow = selected.at(0).indexes().at(0).row();
        // current index
        int currentIdxRow = currentIndex().row();
        // prior current: current in datamodel before MW::fileSelectionChange called
        QModelIndex currentSfIdx = m2->currSfIdx;
        int currentSfRow = currentSfIdx.row();
        int selectedRowsCount = selectionModel()->selectedRows().count();
        // is change row also prior current row
        bool isCurrent = false;
        if (deselectedRow == currentSfRow) isCurrent = true;
        if (selectedRow == currentSfRow) isCurrent = true;

        /*  Debugging
        QString type = "";
        if (!selectedRowsCount && isDeselected)
            type = "UniSelection, Deselect current";
        else if (isDeselected && isCurrent && selectedRowsCount)
            type = "MultiSelection, Deselect current";
        else if (isDeselected && !isCurrent && selectedRowsCount)
            type = "MultiSelection, Deselect non-current";
        else if (anchorCurrent && isSelected && selectedRowsCount > 1)
            type = "MultiSelect, Change, AnchorCurrent";
        else
            type = "Other";

        qDebug() << "\n"
                 << "IconView::selectionChanged: "
                 << "\n  type                 =" << type
                 << "\n  anchorCurrent        =" << anchorCurrent
                 << "\n  isSelected           =" << isSelected
                 << "\n  isDeselected         =" << isDeselected
                 << "\n  isCurrent            =" << isCurrent
                 << "\n  selectedRow          =" << selectedRow
                 << "\n  deselectedRow        =" << deselectedRow
                 << "\n  currentRow           =" << currentIdxRow
                 << "\n  currentSfRow         =" << currentSfRow
                 << "\n  nearest selected row =" << getNearestSelection(deselectedRow).row()
                 << "\n  selectedRowsCount    =" << selectedRowsCount
                    ;
        //*/

        // UniSelection, Deselect current: reselect deselection if no selection remaining
        if (!selectedRowsCount && isDeselected) {
            selectionModel()->select(currentIndex(), QItemSelectionModel::Select);
        }

        // MultiSelection, Deselect current: reset current to nearest next selected row
        else if (isDeselected && isCurrent && selectedRowsCount) {
            QModelIndex idx = getNearestSelection(deselectedRow);
            selectionModel()->setCurrentIndex(idx, QItemSelectionModel::NoUpdate);
            emit fileSelectionChange(idx, QModelIndex(), CLASSFUNCTION);
        }

        // MultiSelection, Deselect non-current: reset current index to before
        else if (isDeselected && !isCurrent && selectedRowsCount) {
            selectionModel()->setCurrentIndex(currentSfIdx, QItemSelectionModel::NoUpdate);
        }

        // MultiSelect, Change, AnchorCurrent: reset current to before
        else if (anchorCurrent && isSelected && selectedRowsCount > 1) {
            QListView::selectionChanged(selected, deselected);
            selectionModel()->setCurrentIndex(currentSfIdx, QItemSelectionModel::NoUpdate);
            emit fileSelectionChange(currentIndex(), QModelIndex(), CLASSFUNCTION);
        }

        else {
            // update the View
            QListView::selectionChanged(selected, deselected);
            emit fileSelectionChange(currentIndex(), QModelIndex(), CLASSFUNCTION);
        }

        // update status bar
        QString s = "";
        if (m2->isStressTest) s = "   Stress count: " + QString::number(m2->slideCount);
        emit updateStatus(true, s, CLASSFUNCTION);
    }
}

int IconView::getSelectedCount()
{
/*
    For some reason the selectionModel->selectedRows().count() is not up-to-date but
    selectedIndexes().count() works. This is called from MW::updateStatus to report the
    number of images selected.
*/
    if (G::isLogger) G::log(CLASSFUNCTION);
    return selectionModel()->selectedRows().count();
//    return selectedIndexes().count();
}

QStringList IconView::getSelectedThumbsList()
{
/*
   This was used by the eliminated tags class and is not used but looks useful.
*/
    if (G::isLogger) G::log(CLASSFUNCTION);
    QModelIndexList indexesList = selectionModel()->selectedIndexes();
    QStringList SelectedThumbsPaths;

    for (int tn = indexesList.size() - 1; tn >= 0 ; --tn) {
        SelectedThumbsPaths << indexesList[tn].data(G::PathRole).toString();
    }
    return SelectedThumbsPaths;
}

bool IconView::isThumb(int row)
{
    if (G::isLogger) G::log(CLASSFUNCTION);
    return dm->sf->index(row, 0).data(Qt::DecorationRole).isNull();
}

void IconView::selectThumb(QModelIndex idx)
{
/*
    Used for thumbnail navigation (left, right, up, down etc)
*/
    if (G::isLogger || G::isFlowLogger) G::log(CLASSFUNCTION, QString::number(idx.row()));
    if (idx.isValid()) {
        G::isNewSelection = true;
        selectionModel()->clearSelection();
        setCurrentIndex(idx);
        scrollTo(idx, ScrollHint::PositionAtCenter);
    }
}

void IconView::selectThumb(int row)
{
    if (G::isLogger) G::log(CLASSFUNCTION);
//    qDebug() << CLASSFUNCTION << "row =" << row;
    // some operations assign row = -1 if not found
    if (row < 0) return;
    setFocus();
    QModelIndex idx = dm->sf->index(row, 0, QModelIndex());
    selectThumb(idx);
}

void IconView::selectThumb(QString &fPath)
{
    if (G::isLogger) G::log(CLASSFUNCTION);
    selectThumb(dm->proxyIndexFromPath(fPath));
}

void IconView::selectNext()
{
    if (G::isLogger) G::log(CLASSFUNCTION);
    if (G::mode == "Compare") return;
    selectThumb(getNextRow());
}

void IconView::selectPrev()
{
    if (G::isLogger) G::log(CLASSFUNCTION);
    if(G::mode == "Compare") return;
    selectThumb(getPrevRow());
}

void IconView::selectUp()
{
    if (G::isLogger) G::log(CLASSFUNCTION);
    qDebug() << CLASSFUNCTION;
    if (G::mode == "Table" || !isWrapping()) selectPrev();
    else setCurrentIndex(moveCursor(QAbstractItemView::MoveUp, Qt::NoModifier));
}

void IconView::selectDown()
{
    if (G::isLogger) G::log(CLASSFUNCTION);
    qDebug() << CLASSFUNCTION;
//    if (G::mode == "Table" || !isWrapping()) selectNext();
    /*else */setCurrentIndex(moveCursor(QAbstractItemView::MoveDown, Qt::NoModifier));
}

void IconView::selectPageUp()
{
    if (G::isLogger) G::log(CLASSFUNCTION);
    qDebug() << CLASSFUNCTION;
//    if (G::mode == "Table" || !isWrapping()) selectPrev();
    /*else */setCurrentIndex(moveCursor(QAbstractItemView::MovePageUp, Qt::NoModifier));
}

void IconView::selectPageDown()
{
    if (G::isLogger) G::log(CLASSFUNCTION);
    qDebug() << CLASSFUNCTION;
    if (G::mode == "Table" || !isWrapping()) selectNext();
    else setCurrentIndex(moveCursor(QAbstractItemView::MovePageDown, Qt::NoModifier));
}

void IconView::selectFirst()
{
    if (G::isLogger) G::log(CLASSFUNCTION);
    selectThumb(0);
}

void IconView::selectLast()
{
    if (G::isLogger) G::log(CLASSFUNCTION);
    qDebug() << CLASSFUNCTION;
    selectThumb(getLastRow());
}

void IconView::selectRandom()
{
    if (G::isLogger) G::log(CLASSFUNCTION);
//    qDebug() << "\n***************************************************************************\n" << CLASSFUNCTION;
    selectThumb(getRandomRow());
}

void IconView::selectNextPick()
{
    if (G::isLogger) G::log(CLASSFUNCTION);
    selectThumb(getNextPick());
}

void IconView::selectPrevPick()
{
    if (G::isLogger) G::log(CLASSFUNCTION);
    selectThumb(getPrevPick());
}

void IconView::thumbsEnlarge()
{
/* This function enlarges the size of the thumbnails in the thumbView, with the objectName
   "Thumbnails", which either resides in a dock or a floating window.
*/
    if (G::isLogger) G::log(CLASSFUNCTION);
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
    if (G::isLogger) G::log(CLASSFUNCTION);
    if (iconWidth > ICON_MIN  && iconHeight > ICON_MIN) {
        iconWidth *= 0.9;
        iconHeight *= 0.9;
        if (iconWidth < ICON_MIN) iconWidth = ICON_MIN;
        if (iconHeight < ICON_MIN) iconHeight = ICON_MIN;
    }
    setThumbParameters();
    scrollTo(currentIndex(), ScrollHint::PositionAtCenter);
}

int IconView::justifyMargin()
{
/*
The ListView can hold x amount of icons in a row before it wraps to the next row.  There will
be a right margin where there was not enough room for another icon.  This function returns the
right margin amount.  It is used in MW::gridDisplay to determine if a rejustify is req'd.
*/
    if (G::isLogger) G::log(CLASSFUNCTION);
    int wCell = iconViewDelegate->getCellSize().width();
    int wRow = width() - G::scrollBarThickness - 8;    // always include scrollbar

    // right margin
    return wRow % wCell;
}

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
    if (G::isLogger) G::log(CLASSFUNCTION);
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
    iconHeight = static_cast<int>(iconWidth * bestAspectRatio);
    /*
    qDebug() << CLASSFUNCTION << objectName()
             << "assignedIconWidth =" << assignedIconWidth
             << "wRow =" << wRow
             << "wCell =" << wCell
             << "tpr =" << tpr
             << "iconWidth =" << iconWidth
             << "iconHeight =" << iconHeight;*/

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
    if (G::isLogger) G::log(CLASSFUNCTION);
    int wCell = iconViewDelegate->getCellSize().width();
    int wRow = width() - G::scrollBarThickness - 8;    // always include scrollbar

    // thumbs per row
    int tpr = wRow / wCell + action;
    if (tpr < 1) return;
    wCell = wRow / tpr;

    if (wCell > G::maxIconSize) wCell = wRow / ++tpr;

    iconWidth = iconViewDelegate->getThumbWidthFromCellWidth(wCell);
    iconHeight = static_cast<int>(iconWidth * bestAspectRatio);
    /*
    qDebug() << "IconView::justify"
             << "wRow =" << wRow
             << "tpr =" << tpr
             << "wCell =" << wCell
             << "iconWidth =" << iconWidth
             << "iconHeight =" << iconHeight
                ;
                //*/

    assignedIconWidth = iconWidth;
    skipResize = true;      // prevent feedback loop

    setThumbParameters();
//    setViewportParameters();
    calcViewportRange(currentIndex().row());

//    scrollTo(currentIndex(), ScrollHint::PositionAtCenter);
}

void IconView::updateThumbRectRole(const QModelIndex index, QRect iconRect)
{
/*
    thumbViewDelegate triggers this to provide rect data to calc thumb mouse
    click position that is then sent to imageView to zoom to the same spot.
*/
//    if (G::isLogger) G::log(CLASSFUNCTION);
//    qDebug() << CLASSFUNCTION << index;
    emit setValueSf(index, iconRect, G::IconRectRole);
//    dm->sf->setData(index, iconRect, G::IconRectRole);
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
    if (G::isLogger) G::log(CLASSFUNCTION);
//    if (m2->thumbDock != nullptr) {
//        qDebug() << CLASSFUNCTION
//                 << "m2->thumbDock->isFloating() ="
//                 << m2->thumbDock->isFloating();
//        if (m2->thumbDock->isFloating()) return;
//    }
    int mid = midVisibleCell;

    static int prevWidth = 0;
    /*
    qDebug() << CLASSFUNCTION
             << "isFitTopOrBottom =" << isFitTopOrBottom
             << "isWrapping =" << isWrapping()
             << "G::isInitializing =" << G::isInitializing
             << "G::isNewFolderLoaded =" << G::isNewFolderLoaded
             << " m2->gridDisplayFirstOpen =" <<  m2->gridDisplayFirstOpen
                ;
//    */

    // must come after width parameters
    if (G::isInitializing || !G::isNewFolderLoaded /*|| m2->gridDisplayFirstOpen*/) {
        prevWidth = width();
        return;
    }

    // only rejustify when user resizes
    if (isWrapping() && width() != prevWidth) {
        QTimer::singleShot(500, this, SLOT(rejustify()));   // calls calcViewportParameters
    }
    prevWidth = width();

    // return if grid view has not been opened yet
    if (m2->gridDisplayFirstOpen) return;

    if (isFitTopOrBottom) {
        // thumbDock isWrapping = false situation
        G::ignoreScrollSignal = true;
        scrollToRow(mid, CLASSFUNCTION);
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
    if (G::isLogger) G::log(CLASSFUNCTION);
    if (iconWidth > G::maxIconSize) iconWidth = G::maxIconSize;
    if (iconHeight > G::maxIconSize) iconHeight = G::maxIconSize;
    if (iconWidth < G::minIconSize) iconWidth = G::minIconSize;
    if (iconHeight < G::minIconSize) iconHeight = G::minIconSize;

    if (G::iconWMax == G::iconHMax && iconWidth > iconHeight)
        iconHeight = iconWidth;
    if (G::iconWMax == G::iconHMax && iconHeight > iconWidth)
        iconWidth = iconHeight;
    if (G::iconWMax > G::iconHMax)
        iconHeight = static_cast<int>(iconWidth * (static_cast<double>(G::iconHMax) / G::iconWMax));
    if (G::iconHMax > G::iconWMax)
        iconWidth = static_cast<int>(iconHeight * (static_cast<double>(G::iconWMax) / G::iconHMax));

    setThumbParameters();

    bestAspectRatio = static_cast<double>(iconHeight) / iconWidth;
    /*
    qDebug() << CLASSFUNCTION
             << "G::iconWMax =" << G::iconWMax
             << "G::iconHMax =" << G::iconHMax;
//  */
}

void IconView::thumbsFitTopOrBottom()
{
/*
    Called by MW::eventFilter when a thumbDock resize event occurs triggered by the user
    resizing the thumbDock. The thumb size is adjusted to fit the new thumbDock height and
    scrolled to keep the midVisibleThumb in the middle. Other objects visible (docks and
    central widget) are resized.

    For icon cell anatomy (see diagram at top of IconViewDelegate)
*/
    if (G::isLogger) G::log(CLASSFUNCTION);
    /* isFitTopOrBottom is set here, cleared in resize. Used to flag when to just scroll the
    thumbView when the thumbdock splitter triggers this function and when the resize event is
    from another event.
    */
    isFitTopOrBottom = true;

    // viewport available height
    int newViewportHt = height() - G::scrollBarThickness;

    int maxCellHeight = static_cast<int>(G::maxIconSize * bestAspectRatio);
    int hMax = iconViewDelegate->getCellHeightFromThumbHeight(maxCellHeight);
    int hMin = iconViewDelegate->getCellHeightFromThumbHeight(ICON_MIN);

    // restrict icon cell height within limits
    int newThumbSpaceHt = newViewportHt > hMax ? hMax : newViewportHt;
    newThumbSpaceHt = newThumbSpaceHt < hMin ? hMin : newThumbSpaceHt;

    // derive new cellsize from new cellSpace
    iconHeight = iconViewDelegate->getThumbHeightFromAvailHeight(newThumbSpaceHt);

    // make sure within range (should be from thumbSpace check but just to be sure)
    if (bestAspectRatio < 0.1) bestAspectRatio = 1;
    iconHeight = iconHeight > G::maxIconSize ? G::maxIconSize : iconHeight;
    iconHeight = iconHeight < ICON_MIN ? ICON_MIN : iconHeight;
    iconWidth = static_cast<int>(iconHeight / bestAspectRatio);
    /*
    qDebug() << CLASSFUNCTION
             << "viewportHeight =" << newViewportHt
             << "bestAspectRatio =" << bestAspectRatio
             << "iconHeight =" << iconHeight
             << "iconWidth =" << iconWidth
                ;
//    */

    // check thumbWidth within range
    if(iconWidth > G::maxIconSize) {
        iconWidth = G::maxIconSize;
        iconHeight = static_cast<int>(G::maxIconSize * bestAspectRatio);
    }

    // this is critical - otherwise thumbs bunch up
    setSpacing(0);

    // this will change the icon size, which will trigger the resize event
    iconViewDelegate->setThumbDimensions(iconWidth, iconHeight, labelFontSize,
                                         showIconLabels, labelChoice, badgeSize);
}

void IconView::updateLayout()
{
    if (G::isLogger) G::log(CLASSFUNCTION);
    QEvent event{QEvent::LayoutRequest};
    QListView::updateGeometries();
    QListView::event(&event);
}

void IconView::scrollDown(int /*step*/)
{
    if (G::isLogger) G::log(CLASSFUNCTION);
    if(isWrapping()) {
        verticalScrollBar()->triggerAction(QAbstractSlider::SliderSingleStepAdd);
    }
    else {
        horizontalScrollBar()->triggerAction(QAbstractSlider::SliderSingleStepAdd);
    }
}

void IconView::scrollUp(int /*step*/)
{
    if (G::isLogger) G::log(CLASSFUNCTION);
    if(isWrapping()) {
        verticalScrollBar()->triggerAction(QAbstractSlider::SliderSingleStepSub);
    }
    else {
        horizontalScrollBar()->triggerAction(QAbstractSlider::SliderSingleStepSub);
    }
}

void IconView::scrollPageDown(int /*step*/)
{
    if (G::isLogger) G::log(CLASSFUNCTION);
    if(isWrapping()) {
        verticalScrollBar()->triggerAction(QAbstractSlider::SliderPageStepAdd);
    }
    else {
        horizontalScrollBar()->triggerAction(QAbstractSlider::SliderPageStepAdd);
    }
}

void IconView::scrollPageUp(int /*step*/)
{
    if (G::isLogger) G::log(CLASSFUNCTION);
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
    This is called to scroll to the current image or to sync the other views (thumbView,
    gridView and tableView). When the user switches views (loupe, grid and table) the
    thumbView, gridView or tableView is made visible. Hidden widgets cannot be updated. It
    takes time for the view to be rendered, so we have to query it to make sure it is ready to
    accept a scrollto request. This is done by testing the actual maximum size of the
    scrollbars compared to a calculated amount in the okToScroll function.

    source is the calling function and is used for debugging.
*/
    if (G::isLogger) G::log(CLASSFUNCTION);
    /*
    qDebug() << CLASSFUNCTION << objectName() << "row =" << row
             << "requested by" << source;
//    */
    source = "";    // suppress compiler warning
    QModelIndex idx = dm->sf->index(row, 0);
    scrollTo(idx, QAbstractItemView::PositionAtCenter);
}

bool IconView::waitUntilOkToScroll()
{
/*
Returns true when the scrollbars have been fully rendered.
*/
    if (G::isLogger) G::log(CLASSFUNCTION);
    QTime t = QTime::currentTime().addMSecs(1000);
    while (QTime::currentTime() < t) {
        if (okToScroll()) {
            G::wait(50);
            return true;
        }
        qApp->processEvents(QEventLoop::AllEvents, 50);
    }
    return false;
}

bool IconView::okToScroll()
{
    if (G::isLogger) G::log(CLASSFUNCTION);
    qDebug() << CLASSFUNCTION;
    if (objectName() == "Thumbnails") {
        /*
        qDebug() << CLASSFUNCTION << objectName()
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
    if (G::isLogger) G::log(CLASSFUNCTION);
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
//    */
    return scrollOffset;
}

int IconView::getVerticalScrollBarOffset(int row)
{
    if (G::isLogger) G::log(CLASSFUNCTION);
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
    /*
    qDebug() << "objectName" << objectName()
             << "pageWidth" << pageWidth
             << "pageHeight" << pageHeight
             << "thumbCellWidth" << thumbCellWidth
             << "thumbCellHeight" << thumbCellHeight
             << "thumbRowsPerPage" << thumbRowsPerPage
             << "thumbsPerPage" << thumbsPerPage;
//    */
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
//    */
    return scrollOffset;
}

int IconView::getHorizontalScrollBarMax()
{
/*

*/
    if (G::isLogger) G::log(CLASSFUNCTION);
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
    if (G::isLogger) G::log(CLASSFUNCTION);
    int pageWidth = viewport()->width();
    int pageHeight = viewport()->height();
    int thumbCellWidth = getCellSize().width();
    int thumbCellHeight = getCellSize().height();

    if (thumbCellWidth == 0  || thumbCellHeight == 0 || pageWidth == 0 || pageHeight == 0) return 0;

    float thumbsPerPage = pageWidth / thumbCellWidth * (float)pageHeight / thumbCellHeight;
    if (thumbsPerPage == 0) return 0;
    int n = dm->sf->rowCount();
    float pages = float(n) / thumbsPerPage - 1;
    int vMax = static_cast<int>(pages * pageHeight);
    /*
    qDebug() << G::t.restart() << "\t" << objectName()
             << "Row =" << m2->currentRow
             << "verticalScrollBarMax Qt vs Me"
             << verticalScrollBar()->maximum()
             << vMax;
             */
    return vMax;
}

//void IconView::enterEvent(QEvent *event)
//{
//    if (G::isLogger) G::log(CLASSFUNCTION);
////    setFocus();
//    QListView::enterEvent(event);
//}

void IconView::leaveEvent(QEvent *event)
{
    if (G::isLogger) G::log(CLASSFUNCTION);
//    qDebug() << CLASSFUNCTION << event << QWidget::mapFromGlobal(QCursor::pos());

    setCursor(Qt::ArrowCursor);
    prevIdx = model()->index(-1, -1);
    QListView::leaveEvent(event);
}

void IconView::wheelEvent(QWheelEvent *event)
{
    if (G::isLogger) G::log(CLASSFUNCTION);
    QListView::wheelEvent(event);
}

bool IconView::event(QEvent *event) {
/*
    Trap back/forward buttons on Logitech mouse to toggle pick status on thumbnail
*/
    if (G::isLogger) G::log(CLASSFUNCTION);
//    qDebug() << CLASSFUNCTION << event;
    if (event->type() == QEvent::NativeGesture) {
        qDebug() << "IconView::event" << event;
        QNativeGestureEvent *e = static_cast<QNativeGestureEvent *>(event);
        QPoint d = m2->thumbDock->pos();
        QPoint i = pos();
        QPoint c = e->pos();
        QPoint p;
        if (m2->thumbDock->isFloating()) p = c;
        else p = c - i - d;
        QModelIndex idx = indexAt(p);
        QRect vr = visualRect(model()->index(6,0));
//        /*
        qDebug() << "IconView::event"
                 << "\n  d =" << d
                 << "\n  i =" << i
                 << "\n  c =" << c
                 << "\n  p =" << p
                 << "\n  idx =" << idx
                 << "\n  visualRect =" << vr
                    ;
        //*/
        if (idx.isValid()) {
             m2->togglePickMouseOverItem(idx);
        }
    }
    QListView::event(event);
    return true;
}

void IconView::mousePressEvent(QMouseEvent *event)
{
/*
    Captures the position of the mouse click within the thumbnail. This is sent
    to imageView, which pans to the same center in zoom view. This is handy
    when the user wants to view a specific part of another image that is in a
    different position than the current image.
*/
//    if (indexAt(event->pos()).isValid()) QListView::mousePressEvent(event);
//    return;

//    qDebug() << "IconView::mousePressEvent" << event->pos() << hasMouseTracking();

    if (G::isLogger) G::log(CLASSFUNCTION);
//    qDebug() << CLASSFUNCTION << event << event->pos();
    if (event->button() == Qt::RightButton) {
        // save mouse over index for toggle pick
        mouseOverIndex = indexAt(event->pos());
        return;
    }

    // is start tab currently visible
    if (m2->centralLayout->currentIndex() == m2->StartTab)
        m2->centralLayout->setCurrentIndex(m2->LoupeTab);

    // is this a grid or a thumb view
    if(G::mode == "Grid") G::fileSelectionChangeSource =  "GridMouseClick";
    else G::fileSelectionChangeSource =  "ThumbMouseClick";

    // forward and back buttons
    if (event->button() == Qt::BackButton || event->button() == Qt::ForwardButton) {
//        qDebug() << CLASSFUNCTION << event->pos();
        QModelIndex idx = indexAt(event->pos());
        if (idx.isValid()) {
             m2->togglePickMouseOverItem(idx);
        }
        return;
    }

    // Alt + Shift + Left button (toggle pick status)
    if (event->button() == Qt::LeftButton
        && event->modifiers() == (Qt::ShiftModifier | Qt::AltModifier))
    {
        QModelIndex idx = indexAt(event->pos());
        if (idx.isValid()) {
             m2->togglePickMouseOverItem(idx);
        }
        return;
    }

    // must finish event to update current index in iconView if mouse press on an icon
    QModelIndex currIdx = indexAt(event->pos());

    // clear selection if unmodified mouse click
    if (event->modifiers() == Qt::NoModifier) {
//        selectionModel()->clearSelection();
    }

    // unmodified click or tap
    if (event->modifiers() == Qt::NoModifier) {
        // update selection
        if (currIdx.isValid()) {
            setCurrentIndex(currIdx);
            selectionModel()->clearSelection();
            selectionModel()->select(currIdx, QItemSelectionModel::Rows);
        }

        // reqd for thumb resizing
        if (event->button() == Qt::LeftButton) isLeftMouseBtnPressed = true;

        /* Capture the percent coordinates of the mouse click within the thumbnail
           so that the full scale image can be zoomed to the same point.  */
        QModelIndex idx = currentIndex();
        QRect iconRect = idx.data(G::IconRectRole).toRect();
        QPoint mousePt = event->pos();
        QPoint iconPt = mousePt - iconRect.topLeft();
        float xPct = static_cast<float>(iconPt.x()) / iconRect.width();
        float yPct = static_cast<float>(iconPt.y()) / iconRect.height();
        /*
        qDebug() << CLASSFUNCTION << idx << iconRect << mousePt << iconPt << xPct << yPct;
        //*/
        if (xPct >= 0 && xPct <= 1 && yPct >= 0 && yPct <=1) {
            //signal sent to ImageView
            emit thumbClick(xPct, yPct);
        }
    }

    if (currIdx.isValid()) QListView::mousePressEvent(event);
}

void IconView::mouseMoveEvent(QMouseEvent *event)
{
    if (G::isLogger) G::log(CLASSFUNCTION);
//    qDebug() << CLASSFUNCTION << event;
    if (isLeftMouseBtnPressed) isMouseDrag = true;
    QListView::mouseMoveEvent(event);
}

void IconView::mouseReleaseEvent(QMouseEvent *event)
{
    if (G::isLogger) G::log(CLASSFUNCTION);
    QListView::mouseReleaseEvent(event);
    isLeftMouseBtnPressed = false;
    isMouseDrag = false;
    // if icons scroll after mouse click, redraw cursor for new icon
    zoomCursor(indexAt(event->pos()), /*forceUpdate=*/true, event->pos());
}

//QModelIndex IconView::moveCursor(QAbstractItemView::CursorAction cursorAction,
//                                Qt::KeyboardModifiers modifiers)
//{
//    QModelIndex idx = QListView::moveCursor(cursorAction, modifiers);
//    qDebug() << CLASSFUNCTION << cursorAction << modifiers << idx;
////    QModelIndex idx = QAbstractItemView::moveCursor(cursorAction, modifiers);
////    setCurrentIndex(idx);
//    return idx;

//}

void IconView::mouseDoubleClickEvent(QMouseEvent *event)
{
/*
    Show the image in loupe view.  Scroll the thumbView or gridView to position at
    center.
*/
    if (G::isLogger) G::log(CLASSFUNCTION);
    QListView::mouseDoubleClickEvent(event);
    // do not displayLoupe if already displayed
    if (G::mode != "Loupe" && event->button() == Qt::LeftButton) {
        emit displayLoupe();
    }
    scrollToRow(currentIndex().row(), CLASSFUNCTION);
}

void IconView::zoomCursor(const QModelIndex &idx, bool forceUpdate, QPoint mousePos)
{
/*
    Turns the cursor into a frame showing the cropped ImageView zoom window in the
    thumbnail. This is handy for the user to see where a click on thumbnail to instantly
    pan to in same spot in the imageView zoomed window.

    This is predicated on the zoom > zoomFit and the mouse position pointing to a valid
    thumbnail. This function is called from MW::eventFilter when there is mouse movement
    in the thumbView viewport. It is also called from MW::thumbHasScrolled as the mouse
    pointer might be on a different thumbnail after the thumbnails scroll. Finally it is
    called when there is a window resize MW::resizeEvent that will change the
    centralWidget geometry.
*/
//    if (G::isLogger) G::log(CLASSFUNCTION);
    if (G::isEmbellish) return;
    bool isVideo = dm->index(m2->currSfRow, G::VideoColumn).data().toBool();
    if (isVideo) return;

    if (mousePos.y() > viewport()->rect().bottom() - G::scrollBarThickness) {
        setCursor(Qt::ArrowCursor);
        prevIdx = model()->index(-1, -1);
        return;
    }
    if (idx == prevIdx && !forceUpdate) return;
    if (!showZoomFrame) return;
    if (!idx.isValid()) return;
    if (m2->imageView->isFit) return;

    // preview dimensions
    int imW = dm->sf->index(idx.row(), G::WidthPreviewColumn).data().toInt();
    int imH = dm->sf->index(idx.row(), G::HeightPreviewColumn).data().toInt();
    if (imW == 0 || imH == 0) return;

    qreal zoom = m2->imageView->zoom;
    imW = static_cast<int>(imW * zoom);
    imH = static_cast<int>(imH + zoom);
    qreal imA = static_cast<qreal>(imW) / imH;

    QRect centralRect = m2->centralWidget->rect();
    int cW = centralRect.width();
    int cH = centralRect.height();
    /*
    qDebug() << CLASSFUNCTION << idx
             << "cW =" << cW << "cH =" << cH
             << "imW =" << imW << "imH =" << imH;
//        */
    if (imW < cW && imH < cH) {
        setCursor(Qt::ArrowCursor);
        prevIdx = model()->index(-1, -1);
        return;
    }

    prevIdx = idx;

    qreal hScale = static_cast<qreal>(cW) / imW;
    qreal vScale = static_cast<qreal>(cH) / imH;

    iconRect = idx.data(G::IconRectRole).toRect();
    int w, h;       // zoom frame width and height in pixels

    if (hScale < 1 || vScale <= 1 ) {
        // imageView is zoomed in at least one axis

        // scale is for the side that needs to be reduced the most to fit
        qreal zoomFit = (hScale < vScale) ? hScale : vScale;
        qreal scale = zoomFit / zoom;

        // iv = the cropped image visible in centralRect - the ImageView viewport
        int ivW = (imW > cW) ? cW : imW;
        int ivH = (imH > cH) ? cH : imH;
        // aspect of iv
        qreal ivA = static_cast<qreal>(ivW) / ivH;
        /*
        qDebug() << CLASSFUNCTION << "cW =" << cW << "cH =" << cH
                                 << "ivW =" << ivW << "ivH =" << ivH << "ivA =" << ivA
                                 << "hScale =" << hScale << "vScale =" << vScale;
//        */

        /* some brands create thumbnails with black borders, which are not part of the image,
        and should be excluded. The long side (ie width if landscape, height if portrait, will
        not have a black border.  Using that side and the aspect of the original image can give
        the correct length for the other side of the thumbnail.  */
        int iconW, iconH;
        if (imA > 1) {
            iconW = iconRect.width();
            iconH = static_cast<int>(iconW / imA);
        }
        else {
            iconH = iconRect.height();
            iconW = static_cast<int>(iconH * imA);
        }

        // determine cursor frame dimensions: w, h
        if (hScale < vScale) {
            w = static_cast<int>(iconW * scale);
            h = static_cast<int>(w / ivA);
        }
        else {
            h = static_cast<int>(iconH * scale);
            w = static_cast<int>(h * ivA);
        }

        if (w > iconRect.width()) w = iconRect.width();
        if (h > iconRect.height()) h = iconRect.height();

        QString whichScale = hScale < vScale ? "hScale" : "vScale";
        /*
        qDebug() << CLASSFUNCTION
                 << whichScale
                 << "ivW =" << ivW
                 << "ivH =" << ivH
                 << "w =" << w
                 << "h =" << h
                 << "ivA =" << ivA;
            */
        /*
            qDebug() << CLASSFUNCTION
                     << "zoom =" << zoom
                     << "zoomFit =" << zoomFit
                     << "iconRect =" << iconRect
                     << "imW =" << imW
                     << "imH =" << imH
                     << "imA =" << imA
                     << "ivW =" << ivW
                     << "ivH =" << ivH
                     << "cW =" << cW
                     << "cH =" << cH
                     << "hScale =" << hScale
                     << "vScale =" << vScale
                     << "scale =" << scale
                     << "iconW =" << iconW
                     << "iconH =" << iconH
                     << "w =" << w
                     << "h =" << h
                     << "ivA =" << ivA;
        //            */
    }
    else {
    // imageView smaller than central widget so no cropping
        w = iconRect.width();
        h = iconRect.height();
    }

    // draw the new cursor as a frame

    // check if mac Accessibility has scaled pointer size
    float scale = 1.0;
    #ifdef Q_OS_MAC
        scale = Mac::getMouseCursorMagnification();
        if (scale == 0) scale = 1;
    #endif
    w /= scale;
    h /= scale;

    // make room for border
    int pw = 1;                                     // pen width
    w += (pw * 8);                                  // 2 pens * 2 sides * 2 gaps
    h += (pw * 8);
    cursorRect = QRect(0, 0, w, h);
//    qDebug() << CLASSFUNCTION << cursorRect << G::actDevicePixelRatio << G::sysDevicePixelRatio;
    auto frame = QImage(w, h, QImage::Format_ARGB32);
    int opacity = 0;                                // Set this between 0 and 255
    frame.fill(QColor(0,0,0,opacity));
    QPen oPen, iPen;
    oPen.setWidth(pw);
    iPen.setWidth(pw);
    oPen.setColor(Qt::white);
    iPen.setColor(Qt::black);
    QRect oBorder(0, 0, w-pw-1, h-pw-1);            // outer border
    QRect iBorder(pw, pw, w-3*pw-1, h-3*pw-1);      // inner border
    QPainter p(&frame);
    p.setPen(oPen);
    p.drawRect(oBorder);
    p.setPen(iPen);
    p.drawRect(iBorder);
    setCursor(QCursor(QPixmap::fromImage(frame)));
}

void IconView::invertSelection()
{
/*
    Inverts/toggles which thumbs are selected.  Called from MW::invertSelectionAct
*/
    if (G::isLogger) G::log(CLASSFUNCTION);
    QItemSelection toggleSelection;
    QModelIndex firstIndex = dm->sf->index(0, 0);
    QModelIndex lastIndex = dm->sf->index(dm->sf->rowCount() - 1, 0);
    toggleSelection.select(firstIndex, lastIndex);
    selectionModel()->select(toggleSelection, QItemSelectionModel::Toggle);
}

void IconView::copyThumbs()
{
    if (G::isLogger) G::log(CLASSFUNCTION);
    QModelIndexList selection = selectionModel()->selectedRows();
    if (selection.isEmpty()) return;

    QClipboard *clipboard = QGuiApplication::clipboard();
    QMimeData *mimeData = new QMimeData;
    QList<QUrl> urls;
    for (int i = 0; i < selection.count(); ++i) {
        QString fPath = selection.at(i).data(G::PathRole).toString();
        urls << QUrl::fromLocalFile(fPath);
    }
    mimeData->setUrls(urls);
    clipboard->setMimeData(mimeData);
}

void IconView::startDrag(Qt::DropActions)
{
/*
    Drag and drop thumbs to another program.
*/
    if (G::isLogger) G::log(CLASSFUNCTION);
    qDebug() << CLASSFUNCTION;

    QModelIndexList selection = selectionModel()->selectedRows();
    if (selection.isEmpty()) {
        qDebug() << CLASSFUNCTION << "Empty selection";
        return;
    }

    QDrag *drag = new QDrag(this);
    QMimeData *mimeData = new QMimeData;
    QList<QUrl> urls;

    for (int i = 0; i < selection.count(); ++i) {
        QString fPath = selection.at(i).data(G::PathRole).toString();
        urls << QUrl::fromLocalFile(fPath);
    }

    mimeData->setUrls(urls);
    drag->setMimeData(mimeData);

    /*
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
//    drag->setHotSpot(QPoint(pix.width() / 2, pix.height() / 2));
    //*/

//    /*
    Qt::KeyboardModifiers key = QApplication::queryKeyboardModifiers();
    if (key == Qt::AltModifier) {
        drag->exec(Qt::CopyAction);
    }
    if (key == Qt::NoModifier) {
        drag->exec(Qt::MoveAction,  Qt::IgnoreAction);
    }
    //*/
//    drag->exec(Qt::CopyAction | Qt::LinkAction, Qt::IgnoreAction);
}
