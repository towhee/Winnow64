#include "Views/compareImages.h"
#include "Main/global.h"

/*
This class manages a group of imageviews that are each shown in a grid in the
central diget.
*/

CompareImages::CompareImages(QWidget *parent,
                             QWidget *centralWidget,
                             Metadata *metadata,
                             DataModel *dm,
                             IconView *thumbView,
                             ImageCache *imageCacheThread)
    : QWidget(parent)
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    this->metadata = metadata;
    this->dm = dm;
    this->thumbView = thumbView;
    this->imageCacheThread = imageCacheThread;
    this->centralWidget = centralWidget;

    // set up a grid to contain the imageviews
    gridLayout = new QGridLayout;
    gridLayout->setContentsMargins(0, 0, 0, 0);
    gridLayout->setMargin(0);
    gridLayout->setSpacing(0);

    setLayout(gridLayout);
    setContentsMargins(0, 0, 0, 0);

    imList = new QList<CompareView*>;
    sizeList = new QList<QSize>;
    imageAlign = new ImageAlign(120, 0.2);
}

bool CompareImages::load(const QSize &centralWidgetSize, bool isRatingBadgeVisible,
                         QItemSelectionModel *selectionModel)
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    cw = centralWidgetSize;

    // clear old stuff
    /* The way QGridLayout works, every time you add rows and columns it grows, but it
       cannot shrink when you remove items.  The solution is to delete the existing
       layout and start over.
    */

    // first get rid of any existing grid contents (except 1x1 option which causes crash)
    bool okToClearGrid = !(gridLayout->rowCount() == 1 && gridLayout->columnCount() == 1);
    if(okToClearGrid) {
        QLayoutItem *item;
        while ((item = gridLayout->takeAt(0)) != nullptr) {
            gridLayout->removeWidget(item->widget());
            delete item->widget();
            delete item;
        }
    }

    // now delete the grid to get rid of excess rows and columns
    delete gridLayout;

    // new grid
    gridLayout = new QGridLayout;
    gridLayout->setContentsMargins(0, 0, 0, 0);
    gridLayout->setMargin(0);
    gridLayout->setSpacing(0);
    setLayout(gridLayout);
    setContentsMargins(0, 0, 0, 0);
    // clear any old CompareViews
    imList->clear();
    sizeList->clear();

    /* iterate selected thumbs to get image dimensions and configure grid.
    Req'd before load images as they need to know grid size to be able to scale
    to fit.
    */
    count = 0;
    selection = selectionModel->selectedRows();
    count = selection.count();
    if (count > 9) count = 9;

    for (int i = 0; i < count; ++i) {
        QString fPath = selection.at(i).data(G::PathRole).toString();
        int row = dm->fPathRow[fPath];
        int width = dm->index(row, G::WidthColumn).data().toInt();
        int height = dm->index(row, G::HeightColumn).data().toInt();
        QSize imSize(width, height);
        sizeList->append(imSize);
    }

    // determine the optimum grid to maximize the size of each image
    configureGrid();

    // gridCell is 4 pixels smaller to accomodate margins and selection border
    QSize gridCell(cw.width() / cols - 4, cw.height() / rows - 4);

    // iterate selected thumbs - load images, append and connect
    for (int i = 0; i < count; ++i) {
        QModelIndex idxPath = selection.at(i);
//        QModelIndex idxPick = dm->sf->index(idxPath.row(), G::PickColumn);
        QString fPath = selection.at(i).data(G::PathRole).toString();
        // create new compareView and append to list
        imList->append(new CompareView(this, gridCell, dm, metadata, imageCacheThread, thumbView));
        imList->at(i)->loadImage(selection.at(i), fPath);
        // set toggleZoom value (from QSettings)
        imList->at(i)->toggleZoom = toggleZoom;

        // classification badge
        int row = idxPath.row();
        bool isPick = dm->sf->index(row, G::PickColumn).data(Qt::EditRole).toString() == "true";
        QString rating = dm->sf->index(row, G::RatingColumn).data(Qt::EditRole).toString();
        QString colorClass = dm->sf->index(row, G::LabelColumn).data(Qt::EditRole).toString();
        updateClassification(isPick, rating, colorClass, isRatingBadgeVisible, idxPath);

        // relay back and forward mouse btns toggle picks from a compareView
        connect(imList->at(i), SIGNAL(togglePick()), this, SLOT(togglePickSignalRelay()));

        // pass mouse click zoom to other images as a pct of width and height
        connect(imList->at(i), SIGNAL(zoomFromPct(QPointF, QModelIndex, bool)),
                this, SLOT(zoom(QPointF, QModelIndex, bool)));

        // sync panning
        connect(imList->at(i), SIGNAL(panFromPct(QPointF, QModelIndex)),
                this, SLOT(pan(QPointF, QModelIndex)));

        // start of pan position
        connect(imList->at(i), SIGNAL(panStartPct(QModelIndex)),
                this, SLOT(startPan(QModelIndex)));

        // cleanup at end of pan
        connect(imList->at(i), SIGNAL(cleanupAfterPan(QPointF,QModelIndex)),
                this, SLOT(cleanupAfterPan(QPointF,QModelIndex)));

        // get zoom factor to report status and send zoomChange to ZoomDlg
        connect(imList->at(i), SIGNAL(zoomChange(qreal)),
                this, SLOT(zoomChangeFromView(qreal)));

        // when select a compareView make sure all others hav been deselected
        connect(imList->at(i), SIGNAL(deselectAll()), this, SLOT(deselectAll()));

        //        // align
        //        connect(imList->at(i), SIGNAL(align(QPointF, QModelIndex)),
        //                this, SLOT(align(QPointF, QModelIndex)));
    }

    loadGrid();

    thumbView->setCurrentIndex(imList->at(0)->imageIndex);
    go("Home");

    return true;
}

void CompareImages::loadGrid()
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    /*
    int n = gridLayout->count();
    int c = cols;
    int r = rows;
    qDebug() << "n, c, r:" << n << c << r;
    */
    int i = 0;
    int row, col;
    for (row = 0; row < rows; ++row) {
        for (col = 0; col < cols; ++col) {
            gridLayout->addWidget(imList->at(i), row, col);
            // set identical minimum size and stretch to force all cells to remain the same
            // size relative to each other
            gridLayout->setColumnMinimumWidth(col, 1);
            gridLayout->setColumnStretch(col, 1);
            gridLayout->setRowMinimumHeight(row, 1);
            gridLayout->setRowStretch(row, 1);
            i++;
            if (i == count) break;
        }
        if (i == count) break;
    }
}

void CompareImages::configureGrid()
{
/*  Returns the most efficient number of rows and columns to fit n images (between 2 - 9)
    in a grid, based on the aspect ratios of each image.  The algoritm minimizes the total area.
*/
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    long area1, area2, area3;
    switch (count) {
    case 2:
        if (area(1, 2) > area(2, 1)) {
            rows = 1;
            cols = 2;
        }
        else {
            rows = 2;
            cols = 1;
        }
        break;
    case 3:
        area1 = area(2, 2);
        area2 = area(1, 3);
        area3 = area(3, 1);
        if (area1 >= area2 && area1 >= area3) {
            rows = 2;
            cols = 2;
            break;
        }
        if (area2 >= area1 && area2 >= area3) {
            rows = 1;
            cols = 3;
            break;
        }
        if (area3 >= area1 && area3 >= area2) {
            rows = 3;
            cols = 1;
            break;
        }
        break;
    case 4:
        qDebug() << __FUNCTION__ << "1";
        area1 = area(2, 2);
        area2 = area(1, 4);
        area3 = area(4, 1);
        if (area1 >= area2 && area1 >= area3) {
            rows = 2;
            cols = 2;
            break;
        }
        if (area2 > area1 && area2 > area3) {
            rows = 1;
            cols = 4;
            break;
        }
        if (area3 > area1 && area3 > area2) {
            rows = 4;
            cols = 1;
            break;
        }
        break;
    case 5:
    case 6:
        if (area(2, 3) > area(3, 2)) {
            rows = 2;
            cols = 3;
        }
        else {
            rows = 3;
            cols = 2;
        }
        break;
    case 7:
    case 8:
        area1 = area(3, 3);
        area2 = area(2, 4);
        area3 = area(4, 2);
        if (area1 >= area2 && area1 >= area3) {
            rows = 3;
            cols = 3;
            break;
        }
        if (area2 > area1 && area2 > area3) {
            rows = 2;
            cols = 4;
            break;
        }
        if (area3 > area1 && area3 > area2) {
            rows = 4;
            cols = 2;
            break;
        }
        break;
    case 9: rows = 3;   cols = 3;   break;
    }
}

long CompareImages::area(int rows, int cols)
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    // cw = central widget
    // Define cell size each image must fit into
    QSize cell(cw.width() / cols, cw.height() / rows);
    long area = 0;

    // scale each image for the cell size and sum total area of each scaled image
    for (int i = 0; i < count; ++i) {
        QSize imSize = sizeList->at(i);
        imSize.scale(cell, Qt::KeepAspectRatio);
        area += imSize.width() * imSize.height();
/*        qDebug() << "rows, cols" << rows << cols
                 << "central widget" << cw
                 << "cell size" << cell << "item"
                 << i << "imSize" << imSize
                 << "item area" << imSize.width() * imSize.height()
                 << "sum area" << area;*/
    }
    return area;
}

int CompareImages::current()
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    QModelIndex idx = thumbView->currentIndex();
    for (int i = 0; i < imList->count(); ++i) {
        if (imList->at(i)->imageIndex == idx) {
            return i;
        }
    }
    return -1;
}

void CompareImages::go(QString key)
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__, key);
    #endif
    }
    if (key == "Right") {
        int i = current();
        imList->at(i)->deselect();
        if (i == imList->count() - 1) i = 0;
        else i++;
        imList->at(i)->select();
    }
    if (key == "Left") {
        int i = current();
        imList->at(i)->deselect();
        if (i == 0) i = imList->count() - 1;
        else i--;
        imList->at(i)->select();
    }
    if (key == "End") {
        int i = current();
        imList->at(i)->deselect();
        i = imList->count() - 1;
        imList->at(i)->select();
    }
    if (key == "Home") {
        int i = current();
        imList->at(i)->deselect();
        i = 0;
        imList->at(i)->select();
    }
}

void CompareImages::updateClassification(bool isPick, QString rating, QString colorClass,
                                         bool isRatingBadgeVisible, QModelIndex idx)
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    for (int i = 0; i < imList->count(); ++i) {
        if (imList->at(i)->imageIndex.row() == idx.row()) {
            imList->at(i)->classificationLabel->setRating(rating);
            imList->at(i)->classificationLabel->setColorClass(colorClass);
            imList->at(i)->classificationLabel->setPick(isPick);
            imList->at(i)->classificationLabel->setRatingColorVisibility(isRatingBadgeVisible);
            imList->at(i)->classificationLabel->refresh();
        }
    }
}

void CompareImages::zoom(QPointF scrollPct, QModelIndex idx, bool isZoom)
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    for (int i = 0; i < imList->count(); ++i) {
        if (imList->at(i)->imageIndex != idx) {
            imList->at(i)->slaveZoomToPct(scrollPct, isZoom);
        }
    }
}

void CompareImages::pan(QPointF scrollPct, QModelIndex idx)
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    for (int i = 0; i < imList->count(); ++i) {
        if (imList->at(i)->imageIndex != idx) {
            imList->at(i)->slavePanToDeltaPct(scrollPct);
        }
    }
}

void CompareImages::startPan(QModelIndex idx)
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    for (int i = 0; i < imList->count(); ++i) {
        if (imList->at(i)->imageIndex != idx) {
            imList->at(i)->slaveSetPanStartPct();
        }
    }
}

void CompareImages::cleanupAfterPan(QPointF deltaPct, QModelIndex idx)
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    for (int i = 0; i < imList->count(); ++i) {
        if (imList->at(i)->imageIndex != idx) {
            imList->at(i)->slaveCleanupAfterPan(deltaPct);
        }
    }
}

// try to find a commmon feature to align all comparison images - not working very well
void CompareImages::align(QPointF /*basePos*/, QModelIndex idx)
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }

    int idxRow = 0;
    for (int i = 0; i < imList->count(); ++i) {
        if (imList->at(i)->imageIndex == idx) {
            idxRow = i;
            break;
        }
    }
    QImage *baseImage = new QImage;
    *baseImage = imList->at(idxRow)->pmItem->pixmap().toImage();
    QPointF basePosPct = imList->at(idxRow)->getScrollPct();
    for (int i = 0; i < imList->count(); ++i) {
        if (imList->at(i)->imageIndex != idx) {
            QImage *image = new QImage;
            *image = imList->at(i)->pmItem->pixmap().toImage();
            QPointF scrollPct(imageAlign->alignImage(baseImage, image, basePosPct));
            imList->at(i)->setScrollBars(scrollPct);
        }
    }
}

void CompareImages::updateToggleZoom(qreal toggleZoomValue)
{
/*
Slot for signal from update zoom dialog to set the amount to zoom when user
clicks on the unzoomed image.
*/
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    for (int i = 0; i < imList->count(); ++i)
        imList->at(i)->toggleZoom = toggleZoomValue;
}

void CompareImages::zoomChangeFromView(qreal zoomValue)
{
/*
Signal zoom changes from CompareImages to simplify connections as the
CompareView instances will not have been created when connections are
assigned im MW.  However, CompareImages does exist and communicates with
CompareView instances.
*/
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    this->zoomValue = zoomValue;       // used by MW::updateStatus

    // update ZoomDlg
    emit zoomChange(zoomValue);

    emit updateStatus(true, "");
}

void CompareImages::zoomIn()
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    for (int i = 0; i < imList->count(); ++i) {
        imList->at(i)->zoomIn();
    }
}

void CompareImages::zoomOut()
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    for (int i = 0; i < imList->count(); ++i) {
        imList->at(i)->zoomOut();
    }
}

void CompareImages::zoomToFit()
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    for (int i = 0; i < imList->count(); ++i) {
        imList->at(i)->zoomToFit();
    }
}

void CompareImages::zoomTo(qreal zoomValue)
{
/*
Called ZoomDlg when the zoom is changed. From here the message is passed on to
each instance of CompareView, which in turn makes the proper scale change.
*/
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    for (int i = 0; i < imList->count(); ++i) {
        imList->at(i)->zoomTo(zoomValue);
    }
}

void CompareImages::zoomToggle()
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    for (int i = 0; i < imList->count(); ++i) {
        imList->at(i)->zoomToggle();
    }
}

void CompareImages::deselectAll()
{
/*
This slot is called when a compareView is entered, either via mouse passover or
a left/right/home/end keystroke.  All compareViews are deselected to make sure
there are no selected "orphans".
*/
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    for (int i = 0; i < imList->count(); ++i) {
        imList->at(i)->deselect();
    }
}

void CompareImages::togglePickSignalRelay()
{
/*
Clicking the forward or back mouse buttons in a CompareView emits a togglePick
signal.  However, MW does not know about CompareViews created in CompareImages,
so the signal is relayed on to MW, which does know about CompareImages.
*/
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    emit togglePick();
}

void CompareImages::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
}

void CompareImages::test()
{
    bool okToClearGrid = !(gridLayout->rowCount() == 1 && gridLayout->columnCount() == 1);
    if(okToClearGrid) {
        for(int row = 0; row < gridLayout->rowCount(); ++row) {
            for(int col = 0; col < gridLayout->columnCount(); ++col) {
                QWidget *w = gridLayout->itemAtPosition(row, col)->widget();
                if(w) {
                    gridLayout->removeWidget(w);
                    delete w;
                }
            }
        }
    }
    repaint();

//    delete gridLayout;
    imList->clear();
    this->repaint();
}
