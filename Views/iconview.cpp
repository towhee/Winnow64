#include "Views/iconview.h"
#include "Main/mainwindow.h"

/*  IconView Overview

IconView shows the list of images within a folder and it's children (optional) as
thumbnails/icons. When a list item is selected the image is shown in imageView.

There are two versions of IconView: gridView and thumbView.

    1. The thumbView is inside a QDockWidget which allows it to dock and be moved
       and resized.
    2. The gridView is in the CentralWidget and cannot be moved.

IconView does the following:

    Shows the file list of eligible images, including attributes for selected, picked,
    rating, color class and locked.

    The IconViewDelegate class formats the look of the thumbnails.

    Sorts the list based on date acquired, filename and forward/reverse

    The mouse click location within the thumb is used in ImageView to pan a zoomed image.

DataModel QStandardItemModel roles used:

    1   DecorationRole - holds the thumbnail as an icon
    3   ToolTipRole - the file path
    8   BackgroundRole - the color of the background
   13   SizeHintRole - the QSize of the icon

   User defined roles include:

        PathRole - the file path
        FileNameRole - the file name xxxxxx.ext

   Datamodel columns:

        G::PickColumn
        G::RatingColumn
        G::LabelColumn

Rows

    Note that a "row" in this class refers to the row in the datamodel, which has one
    thumb per row, not the view in the dock, where there can be many thumbs per row.

Icons

    An icon is a QPixmap reduction of an eligible image in the folder.  The icons are
    stored as QIcons in the datamodel in the first column using the DecorationRole.

    ie dm->sf->index(row, 0).data(Qt::DecorationRole)
    and dm->sf->index(row, 0).data(Qt::DecorationRole).isNull

Cells

    In IconView, each thumbnail/icon is located inside a cell, which is managed by
    IconViewDelegate (see iconviewdelegate.cpp documentation at top of file).

    Cell container heirarchy

        • cellRect
        • frameRect
        • thumbRect (iconWidth, iconHeight)

Viewport

    The viewport is the entire area available for the IconView and is returned by
    the viewport() function.  Visible cells are the cells currently visible in the
    viewport.

Page

    A page is the logical number of cells the user would expect to jump to view the next
    or previous cells in the viewport. This is calculated as the number of completely
    visible cells. Since the cells are justified (see below), the last cell in a row of
    cells is always completely visible. The last row of cells in the viewport is often
    only partially visible and is not included in the page.

ThumbView behavior in container QDockWidget (thumbDock in MW), changes:

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

    The thumbDock dimensions are controlled by the size of its contents - the thumbView.
    When docked in bottom or top and thumb wrapping is false the maximum height is
    defined by thumbView->setMaximumHeight().

    Icon and ThumbDock size variables

        When icons are loaded they are resized with the long side = G::maxIconSize.
        When icons are displayed in IconView they can be resized by the user.  The
        resized icon best fit dimensions are iconWidth and iconHeight.

        Cells & Icons:
        G::maxIconSize      - set to 256 px
        G::minIconSize      - set to  40 px
        G::iconWMax         - widest icon px
        G::iconHMax         - highest icon px
        bestAspectRatio     - G::iconHMax / G::iconWMax
        iconWidth           - resized best fit icon width  (IconViewDelegate::thumbRect)
        iconHeight          - resized best fit icon height (IconViewDelegate::thumbRect)
        preferredHeight     - height set by user by resizing icons or dragging the
                              ThumbDock splitter when docked top or bottom.

        ThumbDock:
        G::scrollBarThickness    - set to 14 px
        height()                 - height of the IconView viewport
        preferredThumbDockHeight - height set by user by resizing icons or dragging the
                                   ThumbDock splitter when docked top or bottom.

    ThumbDock resize to fit thumbs:

        This only occurs when the thumbDock is located in the top or bottom dock locations and
        wrap thumbs is not checked in prefDlg.

        When a relocation to the top or bottom occurs the height of the thumbDock is
        adjusted to fit the current size of the thumbs plus the scrollbar height. This
        behavior is managed in MW::setThumbDockFeatures.

        When thumbs are resized the height of the thumbDock is adjusted to accomodate
        the new thumb height.

    Thumb resize to fit in dock:

        This also only occurs when the thumbDock is located in the top or bottom dock
        locations and wrap thumbs is not checked in prefDlg. If the thumbDock horizontal
        splitter is dragged to make the thumbDock taller or shorter, within the minimum
        and maximum thumbView heights, the thumb sizes are adjusted to fit.

        This is triggered by MW::eventFilter() and effectuated in thumbsFit().

Loading icons

    Icons are loaded each time a new folder is selected. MetadataCache::loadIcon reads
    the thumbnail pixmap into the DataModel as an icon. Thumbnails or icons are a maximum
    of 256px and can have various aspect ratios. The IconView thumbView, when anchored to
    the top or bottom of the centralWidget, sets its height based on the tallest aspect.
    To dewtermine this, the widest and highest amounts are determined by calling
    MetadataCache::iconMax for each icon as it is loaded as G::iconWMax and G::iconHMax.

    The best fit is a function of the aspect ratios of the icon population. The IconView
    cell size is defined by the smallest cell that will fit all the icons. For example,
    if all the icons are 16x9 landscape then the cell size will be shorter than if the
    icons were portrait 9x16 aspect. Usually there is a mix, and the thumb size (from
    which the cell size is calc) is the rectangle that all the icons will fit.

    ie |------------|  and  |------|  fit into  |------------|
       |            |       |      |            |            |
       |            |       |      |            |            |
       |------------|       |      |            |            |
                            |      |            |            |
                            |      |            |            |
                            |------|            |------------|

    After all the icons have been loaded IconView::bestAspect is called to define iconWidth
    and iconHeight, the dimensions of the rectangle that will just fit all the icons.  This
    is sent to IconViewDelegate::setThumbDimensions.  The IconViewDelegate is where the icons
    are drawn in IconViewDelegate::paint.

Making icons bigger or smaller

    Icon size can resized in 10% increments with IconView::thumbsEnlarge and IconView::thumbsShrink.
    iconWidth and iconHeight are incremented and IconViewDelegate::setThumbDimensions is called.
    The thumbDock is a container that resizes to fit its contents: the icons.

    If the thumbDock is made taller by dragging the splitter, MW::eventFilter calls
    IconView::thumbsFitTopOrBottom. The thumb size is adjusted to fit the new thumbDock
    height and scrolled to keep the midVisibleThumb in the middle. Other objects visible
    (docks and central widget) are resized.

Justification

    When grid cells are enlarged or shrunk, or when the viewport is resized, keep the
    right hand side margin minimized. This is only when isWrapping() == true.

    The cells can be direcly adjusted using the "[" and "]" keys, and is handled by the
    justify() function.

    When the viewport is resized, the cell size is adjusted by the rejustify() fnction.

Scrolling

    When scrolling occurs the visible icons (cells) change.  The caching processes: ImageCache,
    MetaRead and MetadataCache use this to manage their respective caches.

    MW::createThumbView and MW::createGridView connect scrollbar changes to MW::thumbHasScrolled
    and MW::gridHasScrolled respectively.

    IconView::updateVisible determines the firstVisibleCell, midVisibleCell, lastVisibleCell and
    visibleCellCount in the viewport.  Called by:
        • IconView::setThumbSize
        • IconView::justify
        • IconView::rejustify
        • IconView::resizeEvent
        • MW::updateIconRange

    MW::updateIconRange finds the greatest range of visible images in thumbView, gridView and
    tableView; held in MW::firstVisible, MW::midVisible, MW::lastVisible.

    DataModel::setIconRange IS THIS BEING USED
*/

extern MW *m2;
MW *m2;

IconView::IconView(QWidget *parent, DataModel *dm, ImageCacheData *icd, QString objName)
    : QListView(parent)
{
    this->dm = dm;
    this->icd = icd;
    setObjectName(objName);
    if (isDebug) G::log("IconView::IconView", objectName());

    // this works because ThumbView is a friend class of MW.  It is used in the
    // event filter to access the thumbDock
    m2 = qobject_cast<MW*>(parent);
    this->viewport()->setObjectName(objName + "ViewPort");

    setViewMode(QListView::IconMode);
    setTabKeyNavigation(true);              // not working
    setResizeMode(QListView::Adjust);
//    setLayoutMode(QListView::Batched);    // causes delay that makes scrollTo a headache

    setVerticalScrollMode(QAbstractItemView::ScrollPerItem);
    horizontalScrollBar()->setObjectName("IconViewHorizontalScrollBar");
    verticalScrollBar()->setObjectName("IconViewVerticalScrollBar");
    setWordWrap(true);
    setDragEnabled(true);
    setEditTriggers(QAbstractItemView::NoEditTriggers);
    setSelectionMode(QAbstractItemView::ExtendedSelection);
    setSelectionBehavior(QAbstractItemView::SelectRows);
    setSelectionRectVisible(false);
    setUniformItemSizes(true);
    setMaximumHeight(100000);
    setContentsMargins(0,0,0,0);
    setSpacing(0);
    setLineWidth(0);
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);

    bestAspectRatio = 1;


    setModel(this->dm->sf);

    iconViewDelegate = new IconViewDelegate(this,
                                            m2->isRatingBadgeVisible,
                                            m2->isIconNumberVisible,
                                            dm,
                                            icd,
                                            dm->selectionModel
                                            );
    iconViewDelegate->setThumbDimensions(iconWidth, iconHeight, labelFontSize,
                                         showIconLabels, labelChoice,
                                         badgeSize, iconNumberSize);
    setItemDelegate(iconViewDelegate);

    zoomFrame = new QLabel;

    assignedIconWidth = iconWidth;
    prevIdx = model()->index(-1, -1);

    isDebug = false;

    // used to provide iconRect info to zoom to point clicked on thumb
    // in imageView
    connect(iconViewDelegate, SIGNAL(update(QModelIndex, QRect)),
            this, SLOT(updateThumbRectRole(QModelIndex, QRect)));
    connect(this, &IconView::setValueSf, dm, &DataModel::setValueSf);
}

QString IconView::diagnostics()
{
    if (isDebug) G::log("IconView::diagnostics", objectName());
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
    rpt << "\n" << "labelChoice = " << G::s(labelChoice);
    rpt << "\n" << "badgeSize = " << G::s(badgeSize);
    rpt << "\n" << "scrollWhenReady = " << G::s(scrollWhenReady);
    rpt << "\n" << "assignedIconWidth = " << G::s(assignedIconWidth);
    rpt << "\n" << "skipResize = " << G::s(skipResize);
    rpt << "\n" << "bestAspectRatio = " << G::s(bestAspectRatio);
    rpt << "\n" << "firstVisibleRow = " << G::s(firstVisibleCell);
    rpt << "\n" << "midVisibleRow = " << G::s(midVisibleCell);
    rpt << "\n" << "lastVisibleRow = " << G::s(lastVisibleCell);
    rpt << "\n" << "thumbsPerPage = " << G::s(visibleCellCount);
    rpt << "\n\n" ;
    rpt << iconViewDelegate->diagnostics();
    rpt << "\n\n" ;
    return reportString;
}

void IconView::refreshThumb(QModelIndex idx, int role)
{
    if (isDebug) G::log("IconView::refreshThumb", objectName());
    if (!idx.isValid()) {
        qWarning() << "WARNING" << "WARNING"
                   << "IconView::refreshThumb"
                   << "idx =" << idx
                   << "is invalid";
        return;
    }
    QVector<int> roles;
    roles.append(role);
    dataChanged(idx, idx, roles);
}

void IconView::refreshThumbs() {
    if (isDebug) G::log("IconView::refreshThumbs", objectName());
    int last = dm->sf->rowCount() - 1;
    dataChanged(dm->sf->index(0, 0), dm->sf->index(last, 0));
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
    if (isDebug) G::log("IconView::setThumbParameters", objectName());
    if (G::isInitializing) return;
        /*
        qDebug() << "IconView::setThumbParameters" << objectName()
                 << "iconWidth =" << iconWidth
                 << "iconHeight =" << iconHeight
                    ;
                    //*/

    /* MUST set thumbdock height to fit thumbs BEFORE updating the delegate.  If not, and the
       thumbView cells are taller than the dock then QIconview::visualRect is not calculated
       correctly, and scrolling does not work properly */
    if (objectName() == "Thumbnails") {
        if (!m2->thumbDock->isFloating())
            m2->setThumbDockHeight();
    }
    setSpacing(0);
    if (labelFontSize == 0) labelFontSize = 10;
    iconViewDelegate->setThumbDimensions(iconWidth, iconHeight, labelFontSize,
                                         showIconLabels, labelChoice,
                                         badgeSize, iconNumberSize);
}

void IconView::setThumbParameters(int _thumbWidth, int _thumbHeight,
                                  int _labelFontSize, bool _showThumbLabels,
                                  int _badgeSize, int _iconNumberSize)
{
    if (isDebug) G::log("IconView::setThumbParameters", objectName());

    iconWidth = _thumbWidth;
    iconHeight = _thumbHeight;
//    iconPadding = _thumbPadding;
    labelFontSize = _labelFontSize;
    showIconLabels = _showThumbLabels;
    badgeSize = _badgeSize;
    iconNumberSize = _iconNumberSize;
    setThumbParameters();
}

QSize IconView::getCellSize()
{
    if (isDebug) G::log("IconView::getCellSize");
    return iconViewDelegate->getCellSize();
}

int IconView::getThumbSpaceWidth(int thumbSpaceHeight)
{
/*
    The thumbSpace is the total cell occupied be the thumbnail, including the outer
    margin, like the box model.  The width is calculated by taking the thumbSpace
    height, determining the thumbnail height, calculating the thumbnail width from
    the aspact ratio and adding the margin back to get the space width.

    This function is used when the thumbView wrapping = false and the thumbDock changes
    height to determine whether a scrollbar is required.
*/
    if (isDebug) G::log("IconView::getThumbSpaceWidth", objectName());
    float aspect = iconWidth / iconHeight;
    // Difference between thumbSpace and thumbHeight
    int margin = iconViewDelegate->getCellSize().height() - iconHeight;
    int newThumbHeight = thumbSpaceHeight - margin;
    int newThumbWidth = static_cast<int>(newThumbHeight * aspect);
    return newThumbWidth + margin - 1;
}

int IconView::midVisible()
{
    return iconViewDelegate->firstVisible +
           ((iconViewDelegate->lastVisible - iconViewDelegate->firstVisible) / 2);
}

void IconView::updateVisible(int sfRow)
{
/*
    The firstVisibleCell and lastVisibleCell are determined in iconViewDelegate.  They are
    reset when an IconView::paintEvent is received by calling resetFirstLastVisible() in
    iconViewDelegate.  When IconView::paintEvent is received all visible cells are
    repainted in iconViewDelegate.

    Deprecated process:

    Determine the first and last visible icons in the IconView viewport for any item in the
    datamodel. There are two options:

    1.  The user has scrolled or jumped to a known sfRow.

        The known datamodel sfRow is the visual midpoint (midVisibleCell) since scrolling
        is set to center the current item in the viewport.

        If we determine the maximum cells in the viewport then it is easy to calculate
        the firstVisibleCell and lastVisibleCell.

    2.  The user has scrolled or jumped to an unknown sfRow (-1).

        Scrolling is set to center the current item in the viewport. The value of the
        scrollbar tells us approximately where we are.

        By iterating back and forth the firstVisible and lastVisible is determined.
*/
    if (isDebug || G::isFlowLogger) G::log("IconView::updateVisible", objectName());

    if (G::isInitializing || G::dmEmpty) {
        firstVisibleCell = 0;
        midVisibleCell = 0;
        lastVisibleCell = 0;
        visibleCellCount = 0;
        return;
    }

    // get from scroll bars
//    visibleCellCount = cellsInViewport();
    int n = dm->sf->rowCount() - 1;
    double x;                               // scrollBar current value (position)
    double max;                             // scrollBar maximum value
    if (isWrapping()) {
        x = verticalScrollBar()->value();
        max = verticalScrollBar()->maximum();
    }
    else {
        x = horizontalScrollBar()->value();
        max = horizontalScrollBar()->maximum();
    }
    double p = x / max;                     // scrollBar percentage in range
    double pRow = p * n;                    // percent of filtered cells or thumbnails
    double pCells = p * visibleCellCount;   // percent of visible cells or thumbnails
    double r = pRow - pCells + p;           // first visible
    firstVisibleCell = qRound(r);
    lastVisibleCell = firstVisibleCell + int(visibleCellCount) + 1;
    midVisibleCell = firstVisibleCell + ((lastVisibleCell - firstVisibleCell) / 2);
    /*
    qDebug() << "IconView::updateVisible"
             << objectName()
             << "scrollbar value =" << x
             << "visibleCellCount =" << visibleCellCount
             << "firstVisibleCell =" << firstVisibleCell
             << "lastVisibleCell =" << lastVisibleCell
             << "midVisibleCell =" << midVisibleCell
        ;
        //*/

    return;

    // get from iconViewDelegate (only works if the icon/image is visible in cell))
    firstVisibleCell = iconViewDelegate->firstVisible;
    lastVisibleCell = iconViewDelegate->lastVisible;
    midVisibleCell = firstVisibleCell + ((lastVisibleCell - firstVisibleCell) / 2);
    visibleCellCount = lastVisibleCell - firstVisibleCell + 1;
    return;

    /* Alternates
    int n = dm->sf->rowCount() - 1;
    bool isGrid = objectName() == "Grid";

    qDebug() << "IconView::updateVisibleCells  row" << sfRow
             << "n =" << n
             << "G::dmEmpty =" << G::dmEmpty
                ;

    if (sfRow >= 0) {
        QSize cell = getCellSize();
        QSize vp = viewport()->size();
        int cellsPerRow = vp.width() / cell.width() + 1;
        int rowsPerVP = 1;
        if (isGrid) rowsPerVP = vp.height() / cell.height() + 1;
        int cellsPerVP = cellsPerRow * rowsPerVP;
        int half = static_cast<int>(cellsPerVP * 0.5);

        qDebug() << "IconView::updateVisibleCells"
                 << objectName()
                 << "row =" << sfRow
                 << "cellsPerRow =" << cellsPerRow
                 << "cellsPerVP =" << cellsPerVP
                 << "half =" << half
            ;

        if (cellsPerVP < n) {
            midVisibleCell = sfRow;
            firstVisibleCell = midVisibleCell - half;
            if (firstVisibleCell < 0) firstVisibleCell = 0;
            lastVisibleCell = firstVisibleCell + cellsPerVP;
            if (lastVisibleCell > n ) {
                lastVisibleCell = n;
                firstVisibleCell = lastVisibleCell - cellsPerVP;
            }
        }
        else {
            firstVisibleCell = 0;
            lastVisibleCell = n;
            midVisibleCell = n / 2;
        }
        visibleCellCount = lastVisibleCell - firstVisibleCell + 1;
    }

    else {
        int first;
        QRect vpRect = viewport()->rect();

        // estimate first icon from scrollbar position/value
        if (isWrapping()) {
            first = verticalScrollBar()->value() * 1.0 / verticalScrollBar()->maximum() * n;
        }
        else {
            first = horizontalScrollBar()->value() * 1.0 / horizontalScrollBar()->maximum() * n;
        }

        // iterate visual rects to find actual first visible
        if (visualRect(dm->sf->index(first, 0)).intersects(vpRect)) {
            while (visualRect(dm->sf->index(first, 0)).intersects(vpRect)) {
                first--;
                if (first < 0) break;
            }
            firstVisibleCell = first + 1;
        }
        else {
            while (!visualRect(dm->sf->index(first, 0)).intersects(vpRect)) {
                first++;
                if (first > n) break;
            }
            firstVisibleCell = first;
        }

        // iterate visual rects to find actual mid and last visible
        for (int row = firstVisibleCell; row < dm->sf->rowCount(); ++row) {
            if (visualRect(dm->sf->index(row, 0)).contains(vpRect.center())) {
                midVisibleCell = row;
            }
            if (!visualRect(dm->sf->index(row, 0)).intersects(vpRect)) {
                lastVisibleCell = row - 1;
                break;
            }
            lastVisibleCell = dm->sf->rowCount() - 1;
        }

        visibleCellCount = lastVisibleCell - firstVisibleCell + 1;
    }

    qDebug() << "IconView::updateVisibleCells"
             << objectName()
             << "row =" << sfRow
             << "firstVisibleCell =" << firstVisibleCell
             << "lastVisibleCell =" << lastVisibleCell
             << "midVisibleCell =" << midVisibleCell
                ;
                //*/
}

int IconView::pageCount()
{
    QSize cell = getCellSize();
    QSize vp = viewport()->size();
    int cellsPerPageRow = vp.width() / cell.width();
    int rowsPerPage = vp.height() / cell.height();
    /*
    qDebug() << "IconView::pageCount"
             << "cellsPerPageRow" << cellsPerPageRow
             << "rowsPerPage" << rowsPerPage
             << "cellsPerPage" << cellsPerPageRow * rowsPerPage
                ;
    //*/
    return cellsPerPageRow * rowsPerPage;
}

int IconView::getThumbsPerPage()
{
/*
    Not being used
*/
    if (isDebug) G::log("IconView::getThumbsPerPage", objectName());
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
    visibleCellCount = tpr * rpp;
    /*qDebug() << "IconView::getThumbsPerPage" << objectName()
             << "G::isInitializing" << G::isInitializing
             << "| G::isLinearLoadDone" << G::isLinearLoadDone
             << "| isVisible = " << isVisible()
             << "| page size =" << vp
             << "| cell size =" << cell
             << "| tpr =" << tpr
             << "| rpp =" << rpp
             << "| thumbsPerPage" << thumbsPerPage;*/
    return visibleCellCount;
}

int IconView::getFirstVisible()
{
/*
    Not being used.

    Return the datamodel row for the first thumb visible in the thumbView. This is
    used to prioritize thumbnail loading when a new folder is selected to show the
    user thumbnails as quickly as possible, instead of waiting for all the
    thumbnails to be generated. This can take a few seconds if there are thousands
    of images in the selected folder.
*/
    if (isDebug) G::log("IconView::getFirstVisible", objectName());
    int n = dm->sf->rowCount()-1;
    QSize cell = getCellSize();
    int iconsPerRow = viewport()->width() / cell.width() + 0;
    int rowsPerPage = viewport()->height() / cell.height() + 1;
    int iconsPerPage = iconsPerRow * rowsPerPage;
    int hScrollPos = this->horizontalScrollBar()->value();
    int hScrollMax = getHorizontalScrollBarMax();
    int vScrollPos = verticalScrollBar()->value();
    int vScrollMax = getVerticalScrollBarMax();
    int first;
    if (isWrapping()) {
        first = vScrollPos * 1.0 / vScrollMax * n;
        qDebug() << "IconView::getFirstVisible" << isWrapping() << "first =" << first;
    }
    else {
        first = hScrollPos * 1.0 / hScrollMax * n;
        qDebug() << "IconView::getFirstVisible" << isWrapping() << "first =" << first;
    }

//    scannedViewportRange();

    QRect iconViewRect = viewport()->rect();
    if (visualRect(dm->sf->index(first, 0)).intersects(iconViewRect)) {
        while (visualRect(dm->sf->index(first, 0)).intersects(iconViewRect)) {
            first--;
            bool in = visualRect(dm->sf->index(first, 0)).intersects(iconViewRect);
            qDebug() << "IconView::getFirstVisible inside" << "first =" << first << in;
            if (first < 0) break;
        }
        firstVisibleCell = first + 2;
    }
    else {
        while (!visualRect(dm->sf->index(first, 0)).intersects(iconViewRect)) {
            first++;
            qDebug() << "IconView::getFirstVisible outside" << "first =" << first;
            if (first > n) break;
        }
        firstVisibleCell = first;
    }

    for (int row = firstVisibleCell; row < dm->sf->rowCount(); ++row) {
        if (!visualRect(dm->sf->index(row, 0)).intersects(iconViewRect)) {
            lastVisibleCell = row;
            break;
        }
        lastVisibleCell = dm->sf->rowCount() - 1;
    }
    visibleCellCount = lastVisibleCell - firstVisibleCell + 1;
    midVisibleCell = firstVisibleCell + visibleCellCount / 2;

    QRect thumbViewRect = viewport()->rect();
    QRect r0 = visualRect(dm->sf->index(0, 0));
    QRect rn = visualRect(dm->sf->index(n, 0));
    int cellW = r0.width();
    int cellH = r0.height();
    QSize a = QSize(rn.x() - r0.x() + r0.width(), rn.y() - r0.y() + rn.height());
    int rows = a.height() / cellH;
    int cols = a.width() / cellW;
    int nExtent = a.width() * a.height();

    //    int mid = (rn.x() - r0.x()) icon.;
    qDebug() << "IconView::getFirstVisible"
             << "ObjectNeme =" << objectName()
             << "isWrapping =" << isWrapping()

             << "cell =" << cell
             << "iconsPerRow =" << iconsPerRow
             << "rowsPerPage =" << rowsPerPage
             << "iconsPerPage =" << iconsPerPage

             << "hScrollPos =" << hScrollPos
             << "hScrollMax =" << hScrollMax
             << "vScrollPos =" << vScrollPos
             << "vScrollMax =" << vScrollMax
             << "firstVisibleCell =" << firstVisibleCell
             << "midVisibleCell =" << midVisibleCell
             << "lastVisibleCell =" << lastVisibleCell
//             << "thumbViewRect =" << thumbViewRect
//             << "cellW =" << cellW
//             << "cellH =" << cellH
//             << "n =" << n
//             << "r0 =" << r0
//             << "rn =" << rn
//             << "a =" << a
//             << "rows =" << rows
//             << "cols =" << cols
//             << "nExtent =" << nExtent
                ;

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
    Not being used.

    Return the datamodel row for the last thumb visible in the IconView. This is
    used to prioritize thumbnail loading when a new folder is selected to show the
    user thumbnails as quickly as possible, instead of waiting for all the
    thumbnails to be generated. This can take a few seconds if there are thousands
    of images in the selected folder.
*/
    if (isDebug) G::log("IconView::getLastVisible", objectName());
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
/*
    Not being used.
*/
    if (isDebug) G::log("IconView::allPageIconsLoaded", objectName());
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
    if (isDebug) G::log("IconView::scannedViewportRange", objectName());
    /* old
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
    */

    int n = dm->sf->rowCount() - 1;
    QSize cell = getCellSize();
    int iconsPerRow = viewport()->width() / cell.width() + 1;
    int rowsPerPage = viewport()->height() / cell.height() + 1;
//    int iconsPerPage = iconsPerRow * rowsPerPage;
    int hScrollPos = this->horizontalScrollBar()->value();
    int hScrollMax = getHorizontalScrollBarMax();
    int vScrollPos = verticalScrollBar()->value();
    int vScrollMax = getVerticalScrollBarMax();
    int first;
    if (isWrapping()) {
        first = vScrollPos * 1.0 / vScrollMax * n;
//        qDebug() << "IconView::getFirstVisible" << isWrapping() << "first =" << first;
    }
    else {
        first = hScrollPos * 1.0 / hScrollMax * n;
//        qDebug() << "IconView::getFirstVisible" << isWrapping() << "first =" << first;
    }

    QRect iconViewRect = viewport()->rect();
    if (visualRect(dm->sf->index(first, 0)).intersects(iconViewRect)) {
        while (visualRect(dm->sf->index(first, 0)).intersects(iconViewRect)) {
            first--;
            bool in = visualRect(dm->sf->index(first, 0)).intersects(iconViewRect);
//            qDebug() << "IconView::getFirstVisible inside" << "first =" << first << in;
            if (first < 0) break;
        }
        firstVisibleCell = first + 2;
    }
    else {
        while (!visualRect(dm->sf->index(first, 0)).intersects(iconViewRect)) {
            first++;
//            qDebug() << "IconView::getFirstVisible outside" << "first =" << first;
            if (first > n) break;
        }
        firstVisibleCell = first;
    }

    for (int row = firstVisibleCell; row < dm->sf->rowCount(); ++row) {
        if (!visualRect(dm->sf->index(row, 0)).intersects(iconViewRect)) {
            lastVisibleCell = row;
            break;
        }
        lastVisibleCell = dm->sf->rowCount() - 1;
    }

    visibleCellCount = lastVisibleCell - firstVisibleCell + 1;
    midVisibleCell = firstVisibleCell + visibleCellCount / 2;

    /*
    qDebug() << "IconView::scannedViewportRange" << objectName().leftJustified(10, ' ')
             << "isInitializing =" << G::isInitializing
             << "isVisible =" << isVisible()
             << "firstVisibleRow =" << firstVisibleCell
             << "lastVisibleRow =" << lastVisibleCell
             << "midVisibleRow =" << midVisibleCell
             << "thumbsPerPage =" << visibleCells;
                //*/
}

bool IconView::isRowVisible(int row)
{
/*
    Dependent on calcViewportRange being up-to-date.
*/
    if (isDebug) G::log("IconView::isRowVisible", objectName());
    //return row >= firstVisibleCell && row <= lastVisibleCell;
    return row >= iconViewDelegate->firstVisible && row <= iconViewDelegate->lastVisible;
}

QModelIndex IconView::upIndex()
{
    if (isDebug) G::log("IconView::downIndex", objectName());
    return moveCursor(QAbstractItemView::MoveUp, Qt::NoModifier);
}

QModelIndex IconView::downIndex()
{
    if (isDebug) G::log("IconView::upIndex", objectName());
    return moveCursor(QAbstractItemView::MoveDown, Qt::NoModifier);
}

QModelIndex IconView::pageUpIndex(int fromRow)
{
    // fromRow is datamodel row
    // Page is number of completely visible cells in viewport
    if (isDebug) G::log("IconView::pageUpIndex", objectName());
    int max = dm->sf->rowCount() - 1;
    int pageUpCell = fromRow + pageCount();
    if (pageUpCell > max) pageUpCell = max;
    scrollToRow(pageUpCell, "IconView::pageUpIndex");
    return dm->sf->index(pageUpCell, 0);
    //return moveCursor(QAbstractItemView::MovePageUp, Qt::NoModifier);
}

QModelIndex IconView::pageDownIndex(int fromRow)
{
    if (isDebug) G::log("IconView::pageDownIndex", objectName());
    // fromRow is datamodel row
    // Page is number of completely visible cells in viewport
    if (isDebug) G::log("IconView::pageUpIndex", objectName());
    int pageDownCell = fromRow - pageCount();
    if (pageDownCell < 0) pageDownCell = 0;
    scrollToRow(pageDownCell, "IconView::pageUpIndex");
    return dm->sf->index(pageDownCell, 0);
    //return moveCursor(QAbstractItemView::MovePageDown, Qt::NoModifier);
}

QString IconView::getCurrentFilePath()
{
/*
    Used by MW::updateStatus to check for instance clash.  Is this needed?
*/
    if (isDebug) G::log("IconView::getCurrentFilePath");
    return currentIndex().data(G::PathRole).toString();
}

// PICKS: Items that have been picked

QFileInfoList IconView::getPicks()
{
/*
    Not being used.

    Returns a list of all the files that have been picked.  It is used in
    MW, passing the list on to the ingestDlg for ingestion/copying to another
    folder.
*/
    if (isDebug) G::log("IconView::getPicks", objectName());
    QFileInfoList fileInfoList;
    for (int row = 0; row < dm->sf->rowCount(); ++row) {
        QModelIndex idx = dm->sf->index(row, G::PickColumn);
        if (idx.data(Qt::EditRole).toString() == "Picked") {
            QModelIndex pathIdx = dm->sf->index(row, 0);
            QString fPath = pathIdx.data(G::PathRole).toString();
            QFileInfo fileInfo(fPath);
            fileInfoList.append(fileInfo);
        }
    }
    return fileInfoList;
}

void IconView::sortThumbs(int sortColumn, bool isReverse)
{
    if (isDebug || G::isFlowLogger) G::log("IconView::sortThumbs", objectName());
    if (isReverse) dm->sf->sort(sortColumn, Qt::DescendingOrder);
    else dm->sf->sort(sortColumn, Qt::AscendingOrder);
    qDebug() << "IconView::sortThumbs";
    scrollTo(currentIndex(), ScrollHint::PositionAtCenter);
}

int IconView::getSelectedCount()
{
/*
   used by MW::updateStatus - move to selection?

    For some reason the selectionModel->selectedRows().count() is not up-to-date but
    selectedIndexes().count() works. This is called from MW::updateStatus to report the
    number of images selected.
*/
    if (isDebug) G::log("IconView::getSelectedCount", objectName());
    return selectionModel()->selectedRows().count();
}

QStringList IconView::getSelectedThumbsList()
{
/*
    This was used by the eliminated tags class and is not used but looks useful.
*/
    if (isDebug) G::log("IconView::getSelectedThumbsList", objectName());
    QModelIndexList indexesList = selectionModel()->selectedIndexes();
    QStringList SelectedThumbsPaths;

    for (int tn = indexesList.size() - 1; tn >= 0 ; --tn) {
        SelectedThumbsPaths << indexesList[tn].data(G::PathRole).toString();
    }
    return SelectedThumbsPaths;
}

int IconView::fitBadge(int pxAvail)
{
    if (isDebug) G::log("IconView::fitBadge", objectName());

    QString s;
    int n = dm->sf->rowCount();
    if (n < 10) s = "9";
    else if (n < 100) s = "99";
    else if (n < 1000) s = "999";
    else if (n < 10000) s = "9999";
    else if (n < 100000) s = "99999";
    int newBadgeSize;
    badgeSize = m2->classificationBadgeSizeFactor;
    for (newBadgeSize = badgeSize; newBadgeSize > 6; newBadgeSize--) {
        QFont font(this->font().family(), newBadgeSize);
        QFontMetrics fm(font);
        if (fm.horizontalAdvance(s) < pxAvail) break;
    }
    return newBadgeSize;
}

void IconView::setThumbSize()
{
/*

*/
    if (isDebug) G::log("IconView::setThumbSize", objectName());
    if (isDebug) qDebug() << "IconView::setThumbSize";

//    int pxAvail = getCellSize().width() * 1.1 * 0.8;
//    badgeSize = fitBadge(pxAvail);

    visibleCellCount = cellsInViewport();
    updateVisible();
    setThumbParameters();

    QModelIndex scrollToIndex;
    int currentRow = currentIndex().row();
    if (currentRow >= firstVisibleCell && currentRow <= lastVisibleCell)
        scrollToIndex = currentIndex();
    else
        scrollToIndex = dm->sf->index(midVisibleCell, 0);
    /* debug
    qDebug() << "IconView::setThumbSize"
             << "currentRow =" << currentRow
             << "firstVisibleCell =" << firstVisibleCell
             << "lastVisibleCell =" << lastVisibleCell
             << "midVisibleCell =" << midVisibleCell
             << "scrollToIndex.row() =" << scrollToIndex.row()
                ;
                //*/
    G::ignoreScrollSignal = true;
    scrollTo(scrollToIndex, ScrollHint::PositionAtCenter);
}

void IconView::thumbsEnlarge()
{
/*
   This function enlarges the size of the thumbnails in the thumbView, with the objectName
   "Thumbnails", which either resides in a dock or a floating window.
*/
    if (isDebug) G::log("IconView::thumbsEnlarge", objectName());

    if (iconWidth < ICON_MIN) iconWidth = ICON_MIN;
    if (iconHeight < ICON_MIN) iconHeight = ICON_MIN;
    if (iconWidth < G::maxIconSize && iconHeight < G::maxIconSize)
    {
        iconWidth *= 1.1;
        iconHeight *= 1.1;
        if (iconWidth > G::maxIconSize) iconWidth = G::maxIconSize;
        if (iconHeight > G::maxIconSize) iconHeight = G::maxIconSize;
    }

    setThumbSize();
    return;
}

void IconView::thumbsShrink()
{
/*
   This function reduces the size of the thumbnails in the thumbView, with the objectName
   "Thumbnails", which either resides in a dock or a floating window.
*/
    if (isDebug) G::log("IconView::thumbsShrink", objectName());

    if (iconWidth > ICON_MIN  && iconHeight > ICON_MIN) {
        iconWidth *= 0.9;
        iconHeight *= 0.9;
        if (iconWidth < ICON_MIN) iconWidth = ICON_MIN;
        if (iconHeight < ICON_MIN) iconHeight = ICON_MIN;
    }

    setThumbSize();
    return;
}

int IconView::justifyMargin()
{
/*
    The ListView can hold x amount of icons in a row before it wraps to the next row.
    There will be a right margin where there was not enough room for another icon. This
    function returns the right margin amount. It is used in MW::gridDisplay to determine
    if a rejustify is req'd.
*/
    if (isDebug) G::log("IconView::justifyMargin", objectName());
    int wCell = iconViewDelegate->getCellSize().width();
    int wRow = width() - G::scrollBarThickness - 8;    // always include scrollbar

    // right margin
    return wRow % wCell;
}

void IconView::rejustify()
{
/*
    This function controls the resizing behavior of the thumbnails in gridview
    and thumbview when wrapping = true and the window is resized or the
    gridview preferences are edited. The grid cells sizes are maintained while
    keeping the right side "justified". The cells can be direcly adjusted using
    the "[" and "]" keys, but this is handled by the justify() function.

    The key to making this work is the variable assignedThumbWidth, which is
    increased or decreased in the justify() function, and used to maintain the
    cell size during the resize and preference adjustment operations.
*/
    if (isDebug) G::log("IconView::rejustify", objectName());
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
    iconHeight = static_cast<int>(iconWidth / bestAspectRatio);
    /*
    qDebug() << "IconView::rejustify" << objectName()
             << "assignedIconWidth =" << assignedIconWidth
             << "wRow =" << wRow
             << "wCell =" << wCell
             << "tpr =" << tpr
             << "iconWidth =" << iconWidth
             << "iconHeight =" << iconHeight;
    //*/

    skipResize = true;      // prevent feedback loop

    setThumbParameters();
//    setViewportParameters();
//    calcViewportRange(currentIndex().row());
    updateVisible(currentIndex().row());
    pageCount();

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
    if (isDebug) G::log("IconView::justify", objectName());
    qDebug() << "IconView::justify" << objectName();

    int wCell = iconViewDelegate->getCellSize().width();
    int wRow = width() - G::scrollBarThickness - 8;    // always include scrollbar

    // thumbs per row
    int tpr = wRow / wCell + action;
    if (tpr < 1) return;
    wCell = wRow / tpr;

    if (wCell > G::maxIconSize) wCell = wRow / ++tpr;

    iconWidth = iconViewDelegate->getThumbWidthFromCellWidth(wCell);
    iconHeight = static_cast<int>(iconWidth / bestAspectRatio);
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
//    calcViewportRange(currentIndex().row());
    updateVisible(currentIndex().row());

//    scrollTo(currentIndex(), ScrollHint::PositionAtCenter);
}

void IconView::updateThumbRectRole(const QModelIndex index, QRect iconRect)
{
/*
    thumbViewDelegate triggers this to provide rect data to calc thumb mouse
    click position that is then sent to imageView to zoom to the same spot.
*/
//    qDebug() << "IconView::updateThumbRectRole" << index;
    QString src = "IconView::updateThumbRectRole";
    emit setValueSf(index, iconRect, dm->instance, src, G::IconRectRole);
}

void IconView::resizeEvent(QResizeEvent *)
{
/*
    The resizeEvent can be triggered by a change in the gridView cell size (thumbWidth)
    that requires justification; by a change in the thumbDock splitter; or a resize of
    the application window.

    The IconView can be a gridView, a wrapping thumbView, or a non-wrapping thumbView
    (docked top or bottom).

    If the view is wrapping and the width changes then a rejustification of the icons is
    required. There is a delay before rejustification in case there are multiple resize
    events to minimize calls to rejustification. That is filtered by checking if the
    ThumbView width has changed or skipResize has been set.

    This event is not forwarded to QListView::resize. This would cause multiple scroll
    events which isn't pretty at all.
*/
    if (isDebug) G::log("IconView::resizeEvent", objectName());
    /*
    if (m2->thumbDock != nullptr) {
        qDebug() << "IconView::resizeEvent"
                 << "m2->thumbDock->isFloating() ="
                 << m2->thumbDock->isFloating();
        if (m2->thumbDock->isFloating()) return;
    }
    //*/

    static int prevWidth = 0;
    /*
    qDebug() << "IconView::resizeEvent"
             << "Object =" << objectName()
             << "hScroll Policy =" << horizontalScrollBarPolicy()
             << "thumbSplitDrag =" << thumbSplitDrag
             << "isWrapping =" << isWrapping()
             << "G::isInitializing =" << G::isInitializing
             << "G::isLinearLoadDone =" << G::isLinearLoadDone
             << "m2->gridDisplayFirstOpen =" <<  m2->gridDisplayFirstOpen
             << "midVisibleCell =" <<  midVisibleCell
                ;
                //    */

    // must come after width parameters
    if (G::isInitializing || (G::isLoadLinear && !G::isLinearLoadDone) /*|| m2->gridDisplayFirstOpen*/) {
        prevWidth = width();
        return;
    }

    if (thumbSplitDrag) return;

    // Rejustify icons
    bool widthChange = width() != prevWidth;
    if (isWrapping() && widthChange) {
        QTimer::singleShot(500, this, SLOT(rejustify()));   // calls calcViewportParameters
    }
    prevWidth = width();

    // req'd to show/hide scrollbar in thumb dock
    setThumbParameters();

    // return if grid view has not been opened yet
    //if (m2->gridDisplayFirstOpen) return;

    pageCount();
    visibleCellCount = cellsInViewport();
    updateVisible();
    m2->numberIconsVisibleChange();

    int sfRow = currentIndex().row();
    /*
    qDebug() << "IconView::resizeEvent"
             << "Object =" << objectName()
             << "first =" << iconViewDelegate->firstVisible
             << "last  =" << iconViewDelegate->lastVisible
                ;
                //*/
}

void IconView::bestAspect()
{
/*
    When a new folder is loaded: (1. Linear loading - MetadataCache::loadIcon calls
    MetadataCache::iconMax for each icon in the datamodel or 2. Concurrent loading -
    MetaRead::setIcon signals DataModel::setIcon, which calls DataModel::setIconMax) to
    find the greatest height and width of the icons, stored in G::iconWMax and
    G::iconHMax. This is used to define the thumb size in IconViewDelegate.

    The resulting max width and height are sent to IconViewDelegate to define the
    thumbRect that holds each icon. This is also the most compact container available.

    The function is called after a new folder is selected and the datamodel icon data has
    been loaded. Both thumbView and gridView have to be called.
*/
    if (isDebug) G::log("IconView::bestAspect", objectName());

    bestAspectRatio = static_cast<double>(G::iconWMax) / G::iconHMax;
    thumbsFitTopOrBottom("bestAspect");

     /*
    qDebug() << "IconView::bestAspect" << objectName()
             << "G::iconWMax =" << G::iconWMax
             << "G::iconHMax =" << G::iconHMax
             << "iconWidth =" << iconWidth
             << "iconHeight =" << iconHeight
             << "bestAspectRatio =" << bestAspectRatio
                ;
//  */
}

void IconView::thumbsFitTopOrBottom(QString src)
{
/*
    The thumbnail size is adjusted to fit the thumbDock height and scrolled to
    keep the midVisibleThumb in the middle. Other objects visible (docks and
    central widget) are resized.

    Called by MW::eventFilter when a thumbDock resize event occurs triggered by
    the user resizing the thumbDock.

    Called by bestAspect when the height change requires a thumbnail size
    change to fit.

    Called when thumbViewShowLabel is changed.

    For icon cell anatomy see diagram at top of iconviewdelegate.cpp.
*/
    if (isDebug) G::log("IconView::thumbsFitTopOrBottom", objectName());
    /*
    qDebug() << "IconView::thumbsFitTopOrBottom  midVisibleCell =" << midVisibleCell << objectName()
                ; //*/

    // viewport available height
    int newViewportHt = height() - G::scrollBarThickness;

    // best aspect ratio to use
    double ba = bestAspectRatio;
    if (ba > 1.0) ba = 1.0;

    // max/min viewport height adjusted for best aspect ratio to use
    int hMax = static_cast<int>(G::maxIconSize * ba);
    int hMin = static_cast<int>(G::minIconSize * ba);

    // max/min cell size
    int maxCellHeight = iconViewDelegate->getCellHeightFromThumbHeight(hMax);
    int minCellHeight = iconViewDelegate->getCellHeightFromThumbHeight(hMin);

    bool exceedsLimits = newViewportHt > maxCellHeight || newViewportHt < minCellHeight;
    /*
    qDebug() << "IconView::thumbsFitTopOrBottom"
             << "newViewportHt =" << newViewportHt
             << "maxCellHeight =" << maxCellHeight
             << "minCellHeight =" << minCellHeight
             << "exceedsLimits =" << exceedsLimits
                ;
                //*/

    // do nothing if exceed limits
    if (exceedsLimits) return;
//    if (newViewportHt > maxCellHeight) return;
//    if (newViewportHt < minCellHeight) return;

    // newViewportHt is okay, set icon size
    iconHeight = iconViewDelegate->getThumbHeightFromAvailHeight(newViewportHt);
    iconWidth = static_cast<int>(iconHeight * bestAspectRatio);

        /*
        qDebug() << "IconView::thumbsFitTopOrBottom" << objectName()
                 << "newViewportHt =" << newViewportHt
                 << "maxCellHeight =" << maxCellHeight
                 << "minCellHeight =" << minCellHeight
                 << "bestAspectRatio =" << bestAspectRatio
                 << "iconHeight =" << iconHeight
                 << "iconWidth =" << iconWidth
                 << "hMax =" << hMax
                 << "hMin =" << hMin
                 << "G::iconWMax =" << G::iconWMax
                 << "G::iconHMax =" << G::iconHMax
                 << "G::maxIconSize =" << G::maxIconSize
                    ;
    //    */

    // bestAspectRatio == 1 therefore no thumbsize change
//    if (src == "bestAspect" && bestAspectRatio > 0.99999 && bestAspectRatio < 1.000001) return;
    setThumbSize();
}

void IconView::repaintView()
{
    repaint();
}

void IconView::updateLayout()
{
    if (isDebug) G::log("IconView::updateLayout", objectName());
    QEvent event{QEvent::LayoutRequest};
    QListView::updateGeometries();
    QListView::event(&event);
}

void IconView::updateView()
{
/*
    Force the view delegate to update.
*/
    if (isDebug) G::log("IconView::updateView", objectName());
    update();
}

void IconView::scrollDown(int /*step*/)
{
    if (isDebug) G::log("IconView::scrollDown", objectName());
    if(isWrapping()) {
        verticalScrollBar()->triggerAction(QAbstractSlider::SliderSingleStepAdd);
    }
    else {
        horizontalScrollBar()->triggerAction(QAbstractSlider::SliderSingleStepAdd);
    }
}

void IconView::scrollUp(int /*step*/)
{
    if (isDebug) G::log("IconView::scrollUp", objectName());
    if(isWrapping()) {
        verticalScrollBar()->triggerAction(QAbstractSlider::SliderSingleStepSub);
    }
    else {
        horizontalScrollBar()->triggerAction(QAbstractSlider::SliderSingleStepSub);
    }
}

void IconView::scrollPageDown(int /*step*/)
{
    if (isDebug) G::log("IconView::scrollPageDown", objectName());
    if(isWrapping()) {
        verticalScrollBar()->triggerAction(QAbstractSlider::SliderPageStepAdd);
    }
    else {
        horizontalScrollBar()->triggerAction(QAbstractSlider::SliderPageStepAdd);
    }
}

void IconView::scrollPageUp(int /*step*/)
{
    if (isDebug) G::log("IconView::scrollPageUp", objectName());
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
    if (isDebug) G::log("IconView::scrollToRow", objectName());
//    /*
    qDebug() << "IconView::scrollToRow" << objectName() << "row =" << row
             << "requested by" << source;
                // */
    source = "";    // suppress compiler warning
    QModelIndex idx = dm->sf->index(row, 0);
    if (!idx.isValid()) {
        qDebug() << "IconView::scrollToRow" << row;
        return;
    }
    scrollTo(idx, QAbstractItemView::PositionAtCenter);
}

void IconView::scrollToCurrent(QString source)
/*
    Called from MW::fileSelectionChange.
*/
{
    if (isDebug) G::log("IconView::scrollToCurrent", objectName());
//    /*
    qDebug() << "IconView::scrollToCurrent" << dm->currentSfIdx
        << "source =" << source;
    // */
    if (!dm->currentSfIdx.isValid() || G::isInitializing /*|| !readyToScroll()*/) return;
    scrollTo(dm->currentSfIdx, ScrollHint::PositionAtCenter);
    scrollTo(dm->currentSfIdx, ScrollHint::EnsureVisible);
}

void IconView::waitUntilScrollReady()  // can get into infinite loop
{
/*  Not used...
    When the viewport resizes it can take a bit before the scrollbars are ready to
    accept new values.
*/
    qDebug() << "IconView::waitUntilScrollReady";
    if (isDebug) G::log("IconView::scrollToCenter", objectName());
    while (!readyToScroll()) {}
}

bool IconView::readyToScroll()
{
    if (isDebug) G::log("IconView::readyToScroll", objectName());
    if (objectName() == "Thumbnails") {
        /*
        qDebug() << "IconView::okToScroll" << objectName()
                 << "horizontalScrollBar()->maximum() =" << horizontalScrollBar()->maximum()
                 << "getHorizontalScrollBarMax() = " << getHorizontalScrollBarMax();
        //*/
        return horizontalScrollBar()->maximum() > 0.95 * getHorizontalScrollBarMax();
    }
    else {
        return verticalScrollBar()->maximum() > 0.95 * getVerticalScrollBarMax();
    }
}

double IconView::cellsInViewport()
{
    int pageWidth = viewport()->width();
    int pageHeight = viewport()->height();
    int cellWidth = getCellSize().width();
    int cellHeight = getCellSize().height();
    return ((double)pageWidth / cellWidth) * ((double)pageHeight / cellHeight);
}

//int IconView::firstRowFromScrollBars()
//{
//    if (isDebug) G::log("IconView::rowFromHorizontalScrollBarOffset", objectName());


//    int row;
//    int n = dm->sf->rowCount() - 1;
//    double x;                               // scrollBar current value (position)
//    double max;                             // scrollBar maximum value
//    double p;                               // scrollBar percentage in range
//    double cells = cellsInViewport();       // total cells all or partly visible
//    if (isWrapping()) {
//        x = verticalScrollBar()->value();
//        max = verticalScrollBar()->maximum();
//    }
//    else {
//        x = horizontalScrollBar()->value();
//        max = horizontalScrollBar()->maximum();
//    }

//    /*
//    if (isWrapping()) {
//        first = verticalScrollBar()->value() * 1.0 / verticalScrollBar()->maximum() * n;
//    }
//    else {
//        first = horizontalScrollBar()->value() * 1.0 / horizontalScrollBar()->maximum() * n;
//    }
//    */
//    p = x / max;
//    double pRow = p * n;
//    double pCells = p * cells;
//    double r = pRow - pCells + p;
//    row = qRound(r);

//    qDebug() << "IconView::rowFromScrollBars"
//             << "first =" << iconViewDelegate->firstVisible
//             << "last =" << iconViewDelegate->lastVisible
//             << "Visible =" << cells
//             << "x =" << x
//             << "max =" << max
//             << "p =" << p
//             << "n =" << n
//             << "pRow =" << pRow
//             << "pCells =" << pCells
//             << "r =" << r
//             << "row =" << row
//                ;
//    return row;
//}

//int IconView::midRowFromScrollBars()
//{
//    return firstRowFromScrollBars() + cellsInViewport() / 2;
//}

int IconView::getHorizontalScrollBarOffset(int row)
{
    // not being used

    if (isDebug) G::log("IconView::getHorizontalScrollBarOffset", objectName());
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
    // not being used

    if (isDebug) G::log("IconView::getVerticalScrollBarOffset", objectName());
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
    if (isDebug) G::log("IconView::getHorizontalScrollBarMax", objectName());
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
    if (isDebug) G::log("IconView::getVerticalScrollBarMax", objectName());
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
    qDebug() << "IconView::getVerticalScrollBarMax" << objectName()
             << "verticalScrollBarMax Qt vs Me"
             << verticalScrollBar()->maximum()
             << vMax;
             //*/
    return vMax;
}

void IconView::leaveEvent(QEvent *event)
{
    if (isDebug) G::log("IconView::leaveEvent", objectName());
//    qDebug() << "IconView::leaveEvent" << event << QWidget::mapFromGlobal(QCursor::pos());
    mouseOverThumbView = false;
    setCursor(Qt::ArrowCursor);
    prevIdx = model()->index(-1, -1);
    QListView::leaveEvent(event);
}

void IconView::wheelEvent(QWheelEvent *event)
{
    if (isDebug) G::log("IconView::wheelEvent", objectName());
//    qDebug() << "IconView::wheelEvent" << event;
    QListView::wheelEvent(event);
}

bool IconView::event(QEvent *event) {
/*
     Trap back/forward buttons on Logitech mouse to toggle pick status on thumbnail
*/
//    if (isDebug) G::log("IconView::event");
//    qDebug() << "IconView::event" << event;
    if (event->type() == QEvent::NativeGesture) {
//        qDebug() << "IconView::event" << event;
        QNativeGestureEvent *e = static_cast<QNativeGestureEvent *>(event);
        QPoint d = m2->thumbDock->pos();
        QPoint i = pos();
        QPoint c = e->pos();
        QPoint p;
        if (m2->thumbDock->isFloating()) p = c;
        else p = c - i - d;
        QModelIndex idx = indexAt(p);
        QRect vr = visualRect(model()->index(6,0));
        /*
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

    if (event->type() == QEvent::Enter) {
        mouseOverThumbView = true;
    }

    QListView::event(event);
    return true;
}

void IconView::paintEvent(QPaintEvent *event)
{
//    if (event->region() == viewport()->visibleRegion())
//        qDebug() << "IconView::paintEvent" << event << event->region() << viewport()->visibleRegion();
    iconViewDelegate->resetFirstLastVisible();
    QListView::paintEvent(event);
}

void IconView::keyPressEvent(QKeyEvent *event){
//    prevent QListView default key actions
}

void IconView::mousePressEvent(QMouseEvent *event)
{
/*
    Captures the position of the mouse click within the thumbnail. This is sent
    to imageView, which pans to the same center in zoom view. This is handy
    when the user wants to view a specific part of another image that is in a
    different position than the current image.
*/
    if (isDebug) G::log("IconView::mousePressEvent", objectName());
    //qDebug() << "IconView::mousePressEvent";

    // record modifier (used in IconView::selectionChanged)
    modifiers = event->modifiers();
    // index clicked
    QModelIndex idx = indexAt(event->pos());
    // mouse position
    mousePressPos = event->pos();

    // is start tab currently visible
    if (m2->centralLayout->currentIndex() == m2->StartTab)
        m2->centralLayout->setCurrentIndex(m2->LoupeTab);

    if (event->button() == Qt::RightButton) {
        // save mouse over index for toggle pick
        mouseOverIndex = indexAt(event->pos());
        // make available for MW::copyImagePathFromContext
        m2->mouseOverIdx = mouseOverIndex;
        return;
    }

    // forward and back buttons
    if (event->button() == Qt::BackButton || event->button() == Qt::ForwardButton) {
        if (idx.isValid()) {
            m2->togglePickMouseOverItem(idx);
        }
        return;
    }

    if (!idx.isValid()) return;

    // left button or touch
    if (event->button() == Qt::LeftButton) {
        isLeftMouseBtnPressed = true;
        if (objectName() == "Grid") G::fileSelectionChangeSource = "GridMouseClick";
        else G::fileSelectionChangeSource =  "ThumbMouseClick";
        QString src = "IconView::mousePressEvent " + objectName();

        m2->sel->select(idx, modifiers);
        return;
        /*
        // No modifier = clear selection and select
        if (modifiers & Qt::NoModifier) {
            m2->sel->sm->clear();
            m2->sel->currentIndex(idx);
            return;
        }

        // Ctrl modifier = toggle select
        if (event->modifiers() & Qt::ControlModifier) {
            // check attempt to deselect only selected item (must always be one selected)
            m2->sel->toggleSelect(idx);
            dragQueue.append(idx.row());
            return;
        }

        // Shift modifier = select span
        if (event->modifiers() & Qt::ShiftModifier) {
            // check attempt to deselect only selected item (must always be one selected)
            m2->sel->select(idx, shiftAnchorIndex);
            m2->sel->select(idx, shiftAnchorIndex);
            shiftAnchorIndex = idx;
            dragQueue.append(idx.row());
        }
        //*/
    }
}

void IconView::mouseMoveEvent(QMouseEvent *event)
{
    if (isDebug) G::log("IconView::mouseMoveEvent", objectName());
    if (isLeftMouseBtnPressed) {
        // allow small 'jiggle' tolerance before start drag
        int deltaX = qAbs(mousePressPos.x() - event->pos().x());
        int deltaY = qAbs(mousePressPos.y() - event->pos().y());
        if (deltaX > 2)  isMouseDrag = true;
        if (deltaY > 2)  isMouseDrag = true;
        if (isMouseDrag) {
            QModelIndex idx = indexAt(event->pos());
            if (!dragQueue.contains(idx.row())) {
                if (event->modifiers() & Qt::ControlModifier) m2->sel->toggleSelect(idx);
                if (event->modifiers() & Qt::ShiftModifier)  m2->sel->select(idx);
                dragQueue.append(idx.row());
            }
            if (event->modifiers() & Qt::AltModifier) startDrag(Qt::CopyAction);
            if (!event->modifiers()) startDrag(Qt::MoveAction);
        }
    }
}

void IconView::mouseReleaseEvent(QMouseEvent *event)
{
    if (isDebug) G::log("IconView::mouseReleaseEvent", objectName());
//    qDebug() << "IconView::mouseReleaseEvent" << event->modifiers() << Qt::NoModifier;
    QModelIndex idx = indexAt(event->pos());

    /* debug
    qDebug() << "\nIconView::mouseReleaseEvent"
             << "\n" << "idx =" << idx
             << "\n" << "idx selected =" << selectionModel()->isSelected(idx)
             << "\n" << "row =" << idx.row()
             << "\n" << "row selected =" << selectionModel()->isRowSelected(idx.row())
             << "\n" << "selected row count =" << selectionModel()->selectedRows().count()
             << "\n" << "dm selected =" << dm->isSelected(idx.row())
             << "\n" << "ctrl modifier =" << (event->modifiers() & Qt::ControlModifier)
                ;
                //*/

    if (!event->modifiers() && event->button() == Qt::LeftButton) {
        QString src = "IconView::mouseReleaseEvent";

        // req'd when click on current with others also selected
//        m2->sel->setCurrentIndex(idx);

        // Capture the percent coordinates of the mouse click within the thumbnail
        // so that the full scale image can be zoomed to the same point.
        QRect iconRect =  dm->currentSfIdx.data(G::IconRectRole).toRect();
        QPoint iconPt = event->pos() - iconRect.topLeft();
        xPct = iconPt.x() * 1.0 / iconRect.width();
        yPct = iconPt.y() * 1.0 / iconRect.height();
        /* debug
        qDebug() << "IconView::mousePressEvent"
                 << "\n currentIndex =" << currentIndex()
                 << "\n iconRect     =" << iconRect
                 << "\n mousePt      =" << event->pos()
                 << "\n iconPt       =" << iconPt
                 << "\n xPct         =" << xPct
                 << "\n yPct         =" << yPct;
        //*/
        if (xPct >= 0 && xPct <= 1 && yPct >= 0 && yPct <=1) {
            //signal sent to ImageView
            emit thumbClick(xPct, yPct);
        }
    }

    isLeftMouseBtnPressed = false;
    isMouseDrag = false;
}

void IconView::mouseDoubleClickEvent(QMouseEvent *event)
{
/*
    Show the image in loupe view.  Scroll the thumbView or gridView to position at
    center.
*/
//    qDebug() << "IconView::mouseDoubleClickEvent";
    if (isDebug) G::log("IconView::mouseDoubleClickEvent", objectName());
    // required to rapidly toggle ctrl click select/deselect without a delay
    QListView::mouseDoubleClickEvent(event);
    // display in loupe if Gridview
    if (G::mode != "Loupe" && event->button() == Qt::LeftButton) {
        emit displayLoupe();
        if (objectName() == "GridView")
            scrollToRow(currentIndex().row(), "IconView::mouseDoubleClickEvent");
    }
}

void IconView::zoomCursor(const QModelIndex &idx, QString src, bool forceUpdate, QPoint mousePos)
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
    if (isDebug) G::log("IconView::zoomCursor", objectName());
    /*
    qDebug() << "IconView::zoomCursor"
             << "src =" << src
             << idx
             << "forceUpdate =" << forceUpdate
             << mousePos;
             //*/
    QString failReason = "";
    if (G::isEmbellish) failReason = "G::isEmbellish";
    if (G::isInitializing) failReason = "G::isInitializing";
    if (G::stop) failReason = "G::stop";
    if (G::isLoadLinear && !G::isLinearLoadDone) failReason = "!G::isLinearLoadDone";
    bool isVideo = dm->index(dm->currentSfRow, G::VideoColumn).data().toBool();
    if (isVideo) failReason = "isVideo";

    if (mousePos.y() > viewport()->rect().bottom() - G::scrollBarThickness) {
        setCursor(Qt::ArrowCursor);
        prevIdx = model()->index(-1, -1);
        failReason = "mousePos.y() > viewport()->rect().bottom() - G::scrollBarThickness";
    }

    if (QGuiApplication::queryKeyboardModifiers()) {
        setCursor(Qt::ArrowCursor);
        failReason = "Key modifier pressed";
    }

//    if (idx == prevIdx && !forceUpdate) reason = "idx == prevIdx && !forceUpdate";
    if (!showZoomFrame) failReason = "!showZoomFrame";
    if (!idx.isValid()) failReason = "!idx.isValid()";
    if (m2->imageView->isFit) failReason = "m2->imageView->isFit";

    // preview dimensions
    int imW = dm->sf->index(idx.row(), G::WidthPreviewColumn).data().toInt();
    int imH = dm->sf->index(idx.row(), G::HeightPreviewColumn).data().toInt();
    if (imW == 0 || imH == 0) failReason = "Zero width or height";

    qreal zoom = m2->imageView->zoom;
    imW = static_cast<int>(imW * zoom);
    imH = static_cast<int>(imH + zoom);
    qreal imA = static_cast<qreal>(imW) / imH;

    QRect centralRect = m2->centralWidget->rect();
    int cW = centralRect.width();
    int cH = centralRect.height();
    /*
    qDebug() << "IconView::zoomCursor" << idx
             << "cW =" << cW << "cH =" << cH
             << "imW =" << imW << "imH =" << imH;
//        */
    if (imW < cW && imH < cH) {
        setCursor(Qt::ArrowCursor);
        prevIdx = model()->index(-1, -1);
        failReason = "imW < cW && imH < cH";
    }

    // debugging
    if (failReason.length()) {
//        qWarning() << "WARNING IconView::zoomCursor Failed because" << failReason;
        return;
    }
    else {
//        qDebug() << "IconView::zoomCursor" << "Succeeded";
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
        qDebug() << "IconView::zoomCursor" << "cW =" << cW << "cH =" << cH
                                 << "ivW =" << ivW << "ivH =" << ivH << "ivA =" << ivA
                                 << "hScale =" << hScale << "vScale =" << vScale;
//        */

        /* some brands create thumbnails with black borders, which are not part of the
        image, and should be excluded. The long side (ie width if landscape, height if
        portrait, will not have a black border. Using that side and the aspect of the
        original image can give the correct length for the other side of the thumbnail.
        */
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
        qDebug() << "IconView::zoomCursor"
                 << whichScale
                 << "ivW =" << ivW
                 << "ivH =" << ivH
                 << "w =" << w
                 << "h =" << h
                 << "ivA =" << ivA;
            */
        /*
            qDebug() << "IconView::zoomCursor"
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
//    qDebug() << "IconView::zoomCursor" << cursorRect << G::actDevicePixelRatio << G::sysDevicePixelRatio;
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

void IconView::startDrag(Qt::DropActions)
{
/*
    Drag and drop thumbs to another program.
*/
    if (isDebug) G::log("IconView::startDrag", objectName());
    //qDebug() << "IconView::startDrag";

    isMouseDrag = false;

    QModelIndexList selection = selectionModel()->selectedRows();
    if (selection.isEmpty()) {
        qWarning() << "WARNING" << "IconView::startDrag" << "Empty selection";
        return;
    }

    QList<QUrl> urls;
    for (int i = 0; i < selection.count(); ++i) {
        QString fPath = selection.at(i).data(G::PathRole).toString();
        urls << QUrl::fromLocalFile(fPath);
    }

    QDrag *drag = new QDrag(this);
    QMimeData *mimeData = new QMimeData;

    mimeData->setUrls(urls);
    drag->setMimeData(mimeData);

    //  Fancy drag cursor
    int w = 96;
    int h = 80;

    int w2 = w/2;
    int h2 = h/2;
    int pw = 2;
    QPixmap pix;
    if (selection.count() > 1) {
        pix = QPixmap(w, h);
        pix.fill(Qt::transparent);
        QPainter painter(&pix);
        painter.setBrush(Qt::NoBrush);
        if (pw) painter.setPen(QPen(QColor(255,255,255,128), pw));
        int x = 0, y = 0, xMax = 0, yMax = 0;
        for (int i = 0; i < qMin(5, selection.count()); ++i) {
            QPixmap pix = dm->item(selection.at(i).row())->icon().pixmap(w2);
            pix.scaled(w2, h2);
            if (i == 4) {
                x = (xMax - pix.width()) / 2;
                y = (yMax - pix.height()) / 2;
            }
            painter.drawPixmap(x, y, pix);
            xMax = qMax(xMax, qMin(w, x + pix.width()));
            yMax = qMax(yMax, qMin(h, y + pix.height()));
            if (pw) painter.drawRect(x + 1, y + 1, qMin(w-pw, pix.width() - pw), qMin(h-pw, pix.height() - pw));
            x = !(x == y) * w * 0.44;
            y = !y * h * 0.36;
        }
        painter.end();
        pix = pix.copy(0, 0, xMax, yMax);
        drag->setPixmap(pix);
    } else {
        pix = dm->item(selection.at(0).row())->icon().pixmap(w);
        drag->setPixmap(pix);
    }

    Qt::KeyboardModifiers key = QApplication::queryKeyboardModifiers();
    if (key == Qt::AltModifier) {
        drag->exec(Qt::CopyAction);
    }
    if (key == Qt::NoModifier) {
        drag->exec(Qt::MoveAction);
    }
}
