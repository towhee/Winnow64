#include "compareImages.h"
#include "global.h"

CompareImages::CompareImages(QWidget *parent,
                         QWidget *centralWidget,
                         Metadata *metadata,
                         ThumbView *thumbView,
                         ImageCache *imageCacheThread)
{
    {
    #ifdef ISDEBUG
    qDebug() << "CompareImages::CompareImages";
    #endif
    }
    this->metadata = metadata;
    this->thumbView = thumbView;
    this->imageCacheThread = imageCacheThread;
    this->centralWidget = centralWidget;

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

bool CompareImages::load(const QSize &centralWidgetSize)
{
    {
    #ifdef ISDEBUG
    qDebug() << "CompareImages::load";
    #endif
    }
    cw = centralWidgetSize;

    // clear old stuff
    imList->clear();
    sizeList->clear();
    while (gridLayout->count() > 0) {
        QWidget *w = gridLayout->itemAt(0)->widget();
        gridLayout->removeWidget(w);
        delete w;
    }

    selection = thumbView->selectionModel()->selectedIndexes();
    count = selection.count();
    if (count > 9) count = 9;

    /* iterate selected thumbs to get image dimensions and configure grid.
    Req'd before load images as they need to know grid size to be able to scale
    to fit.
    */
    for (int i = 0; i < count; ++i) {
        QString fPath = selection.at(i).data(thumbView->FileNameRole).toString();
        QSize imSize(metadata->getWidth(fPath), metadata->getHeight(fPath));
        sizeList->append(imSize);
//        qDebug() << "compareImages loading" << i << fPath;
    }

    configureGrid();
    QSize gridCell(cw.width() / cols, cw.height() / rows);
    for (int col = 0; col < cols; ++col)
        gridLayout->setColumnMinimumWidth(col, cw.width() / cols - 1);
    for (int row = 0; row < rows; ++row)
        gridLayout->setRowMinimumHeight(row, cw.height() / rows - 1);

    // iterate selected thumbs - load images, append and connect
    for (int i = 0; i < count; ++i) {
        QString fPath = selection.at(i).data(thumbView->FileNameRole).toString();
        imList->append(new CompareView(this, gridCell, metadata, imageCacheThread, thumbView));
        bool isPick = qvariant_cast<bool>(selection.at(i).data(thumbView->PickedRole));
        imList->at(i)->pickLabel->setVisible(isPick);
        imList->at(i)->loadImage(selection.at(i), fPath);

        // pass mouse click zoom to other images as a pct of width and height
        connect(imList->at(i), SIGNAL(zoomFromPct(QPointF, QModelIndex, bool)),
                this, SLOT(zoom(QPointF, QModelIndex, bool)));
        // sync panning
        connect(imList->at(i), SIGNAL(panFromPct(QPointF, QModelIndex)),
                this, SLOT(pan(QPointF, QModelIndex)));
        // align
        connect(imList->at(i), SIGNAL(align(QPointF, QModelIndex)),
                this, SLOT(align(QPointF, QModelIndex)));
    }

    loadGrid();
//    ivList->at(0)->setFocus();
//    ivList->at(0)->setStyleSheet("QLabel {border: white;}");
    return true;
}

void CompareImages::loadGrid()
{
    {
    #ifdef ISDEBUG
    qDebug() << "CompareImages::loadGrid";
    #endif
    }
    int i = 0;
    int row, col;
    for (row = 0; row < rows; ++row) {
        for (col = 0; col < cols; ++col) {
            gridLayout->addWidget(imList->at(i), row, col);
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
    qDebug() << "CompareImages::configureGrid";
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
        if (area(1, 3) > area(3, 1)) {
            rows = 1;
            cols = 3;
        }
        else {
            rows = 3;
            cols = 1;
        }
        break;
    case 4:
        area1 = area(2, 2);
        area2 = area(1, 4);
        area3 = area(4, 1);
//        qDebug() << "case 4: area1, area2, area3" << area1 << area2 << area3;
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
        if (area(2, 3) > area(3, 2)) {
            rows = 2;
            cols = 3;
        }
        else {
            rows = 3;
            cols = 2;
        }
        break;
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
    case 7: rows = 3;   cols = 3;   break;
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
    qDebug() << "CompareImages::area";
    #endif
    }
    // cw = central widget
    QSize cell(cw.height() / rows, cw.width() / cols);
    long area = 0;

    for (int i = 0; i < count; ++i) {
        QSize imSize = sizeList->at(i);
        imSize.scale(cell, Qt::KeepAspectRatio);
        area += imSize.width() * imSize.height();
//        qDebug() << "rows, cols" << rows << cols
//                 << "central widget" << cw
//                 << "cell size" << cell << "item"
//                 << i << "imSize" << imSize
//                 << "item area" << imSize.width() * imSize.height()
//                 << "sum area" << area;
    }
    return area;
}

void CompareImages::pick(bool isPick, QModelIndex idx)
{
    {
    #ifdef ISDEBUG
    qDebug() << "CompareImages::pick";
    #endif
    }
//    qDebug() << "CompareImages::pick" << isPick << idx;
    for (int i = 0; i < imList->count(); ++i) {
        if (imList->at(i)->imageIndex == idx) {
            imList->at(i)->pickLabel->setVisible(isPick);
        }
    }
}

void CompareImages::zoom(QPointF scrollPct, QModelIndex idx, bool isZoom)
{
    {
    #ifdef ISDEBUG
    qDebug() << "CompareImages::zoom";
    #endif
    }
//    qDebug() << "CompareImages::zoom  scrollPct" << scrollPct << "isZoom" << isZoom;
    for (int i = 0; i < imList->count(); ++i) {
        if (imList->at(i)->imageIndex != idx) {
            imList->at(i)->zoomToPct(scrollPct, isZoom);
        }
    }
}

void CompareImages::pan(QPointF scrollPct, QModelIndex idx)
{
    {
    #ifdef ISDEBUG
    qDebug() << "CompareImages::pan";
    #endif
    }
//    qDebug() << "CompareImages::pan";
    for (int i = 0; i < imList->count(); ++i) {
        if (imList->at(i)->imageIndex != idx) {
            imList->at(i)->panToDeltaPct(scrollPct);
        }
    }
}

void CompareImages::align(QPointF basePos, QModelIndex idx)
{
    {
    #ifdef ISDEBUG
    qDebug() << "CompareImages::align";
    #endif
    }
    qDebug() << "CompareImages::align";
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
            qDebug() << "basePos" << basePos << "scrollPct" << scrollPct;
            imList->at(i)->panToPct(scrollPct);
        }
    }
}

void CompareImages::zoomIn()
{
    for (int i = 0; i < imList->count(); ++i) {
        imList->at(i)->zoomIn();
    }
}

void CompareImages::zoomOut()
{
    for (int i = 0; i < imList->count(); ++i) {
        imList->at(i)->zoomOut();
    }
}

void CompareImages::zoomToFit()
{
    for (int i = 0; i < imList->count(); ++i) {
        imList->at(i)->zoomToFit();
    }
}

void CompareImages::zoom50()
{
    for (int i = 0; i < imList->count(); ++i) {
        imList->at(i)->zoomToFit();
    }
}

void CompareImages::zoom100()
{
    for (int i = 0; i < imList->count(); ++i) {
        imList->at(i)->zoomToFit();
    }
}

void CompareImages::zoom200()
{
    for (int i = 0; i < imList->count(); ++i) {
        imList->at(i)->zoomToFit();
    }
}

//void CompareImages::zoomTo()
//{
//    for (int i = 0; i < imList->count(); ++i) {
//        imList->at(i)->zoomTo();
//    }
//}

void CompareImages::zoomToggle()
{
    for (int i = 0; i < imList->count(); ++i) {
        imList->at(i)->zoomToggle();
    }
}

