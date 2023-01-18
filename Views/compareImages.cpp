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
                             Selection *sel,
                             IconView *thumbView,
                             ImageCacheData *icd
                             )
    : QWidget(parent)
{
    if (G::isLogger) G::log("CompareImages::CompareImages");
    this->metadata = metadata;
    this->dm = dm;
    this->sel = sel;
    this->thumbView = thumbView;
    this->icd = icd;
    this->centralWidget = centralWidget;

    // set up a grid to contain the imageviews
    gridLayout = new QGridLayout;
    gridLayout->setContentsMargins(0, 0, 0, 0);
//    gridLayout->setMargin(0); // qt6.2
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
    if (G::isLogger) G::log("CompareImages::load");
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
//    gridLayout->setMargin(0); // Qt 6.2
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
    if (count > 16) count = 16;

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
        imList->append(new CompareView(this, gridCell, dm, sel, metadata, icd, thumbView));
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
        connect(imList->at(i), SIGNAL(zoomChange(qreal,bool)),
                this, SLOT(zoomChangeFromView(qreal,bool)));

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
    if (G::isLogger) G::log("CompareImages::loadGrid");
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
/*  Returns the most efficient number of rows and columns to fit n images (between 2 - 16)
    in a grid, based on the aspect ratios of each image.  The algoritm minimizes the total area.
*/
    if (G::isLogger) G::log("CompareImages::configureGrid");
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
    case 10:
    case 11:
    case 12:
        area1 = area(3, 4);
        area2 = area(4, 3);
        if (area1 >= area2) {
            rows = 3;
            cols = 4;
            break;
        }
        else {
            rows = 4;
            cols = 3;
            break;
        }
    case 13:
    case 14:
    case 15:
    case 16:
        rows = 4;   cols = 4;   break;
    }
}

long CompareImages::area(int rows, int cols)
{
    if (G::isLogger) G::log("CompareImages::area");
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
    if (G::isLogger) G::log("CompareImages::current");
    QModelIndex idx = thumbView->currentIndex();
    for (int i = 0; i < imList->count(); ++i) {
        if (imList->at(i)->imageIndex == idx) {
            return i;
        }
    }
    return -1;
}

QModelIndex CompareImages::go(QString key)
{
    if (G::isLogger) G::log("CompareImages::go", key);
    int i = 0;
    if (key == "Right") {
        i = current();
        imList->at(i)->deselect();
        if (i == imList->count() - 1) i = 0;
        else i++;
        imList->at(i)->select();
    }
    if (key == "Left") {
        i = current();
        imList->at(i)->deselect();
        if (i == 0) i = imList->count() - 1;
        else i--;
        imList->at(i)->select();
    }
    if (key == "End") {
        i = current();
        imList->at(i)->deselect();
        i = imList->count() - 1;
        imList->at(i)->select();
    }
    if (key == "Home") {
        i = current();
        imList->at(i)->deselect();
        i = 0;
        imList->at(i)->select();
    }
    return imList->at(i)->imageIndex;
}

void CompareImages::updateClassification(bool isPick, QString rating, QString colorClass,
                                         bool isRatingBadgeVisible, QModelIndex idx)
{
    if (G::isLogger) G::log("CompareImages::updateClassification");
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
    if (G::isLogger) G::log("CompareImages::zoom");
    for (int i = 0; i < imList->count(); ++i) {
        if (imList->at(i)->imageIndex != idx) {
            imList->at(i)->slaveZoomToPct(scrollPct, isZoom);
        }
    }
}

void CompareImages::pan(QPointF scrollPct, QModelIndex idx)
{
    if (G::isLogger) G::log("CompareImages::pan");
    for (int i = 0; i < imList->count(); ++i) {
        if (imList->at(i)->imageIndex != idx) {
            imList->at(i)->slavePanToDeltaPct(scrollPct);
        }
    }
}

void CompareImages::startPan(QModelIndex idx)
{
    if (G::isLogger) G::log("CompareImages::startPan");
    for (int i = 0; i < imList->count(); ++i) {
        if (imList->at(i)->imageIndex != idx) {
            imList->at(i)->slaveSetPanStartPct();
        }
    }
}

void CompareImages::cleanupAfterPan(QPointF deltaPct, QModelIndex idx)
{
    if (G::isLogger) G::log("CompareImages::cleanupAfterPan");
    for (int i = 0; i < imList->count(); ++i) {
        if (imList->at(i)->imageIndex != idx) {
            imList->at(i)->slaveCleanupAfterPan(deltaPct);
        }
    }
}

// try to find a commmon feature to align all comparison images - not working very well
void CompareImages::align(QPointF /*basePos*/, QModelIndex idx)
{
    if (G::isLogger) G::log("CompareImages::align");

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
    if (G::isLogger) G::log("CompareImages::updateToggleZoom");
    for (int i = 0; i < imList->count(); ++i)
        imList->at(i)->toggleZoom = toggleZoomValue;
}

void CompareImages::zoomChangeFromView(qreal zoomValue, bool hasfocus)
{
/*
    Signal zoom changes from CompareImages to simplify connections as the
    CompareView instances will not have been created when connections are
    assigned im MW.  However, CompareImages does exist and communicates with
    CompareView instances.
*/
    if (G::isLogger) G::log("CompareImages::zoomChangeFromView");
//    qDebug() << "CompareImages::zoomChangeFromView" << "zoomValue =" << zoomValue;
//    zoomValue /= G::actDevicePixelRatio;
    this->zoomValue = zoomValue;       // used by MW::updateStatus

    // update ZoomDlg
    if (hasfocus) {
        emit zoomChange(zoomValue);
        emit updateStatus(true, "", "CompareImages::zoomChangeFromView");
    }
}

void CompareImages::zoomIn()
{
    if (G::isLogger) G::log("CompareImages::zoomIn");
    for (int i = 0; i < imList->count(); ++i) {
        imList->at(i)->zoomIn();
    }
}

void CompareImages::zoomOut()
{
    if (G::isLogger) G::log("CompareImages::zoomOut");
    for (int i = 0; i < imList->count(); ++i) {
        imList->at(i)->zoomOut();
    }
}

void CompareImages::zoomToFit()
{
    if (G::isLogger) G::log("CompareImages::zoomToFit");
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
    if (G::isLogger) G::log("CompareImages::zoomTo");
    for (int i = 0; i < imList->count(); ++i) {
        imList->at(i)->zoomTo(zoomValue);
    }
}

void CompareImages::zoomToggle()
{
    if (G::isLogger) G::log("CompareImages::zoomToggle");
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
    if (G::isLogger) G::log("CompareImages::deselectAll");
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
    if (G::isLogger) G::log("CompareImages::togglePickSignalRelay");
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
