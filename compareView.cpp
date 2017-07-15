#include "compareview.h"
#include "global.h"

CompareView::CompareView(QWidget *parent, Metadata *metadata, ThumbView *thumbView,
                         ImageCache *imageCacheThread)
{
    this->metadata = metadata;
    this->thumbView = thumbView;
    this->imageCacheThread = imageCacheThread;

    gridLayout = new QGridLayout;
    gridLayout->setContentsMargins(0, 0, 0, 0);
    gridLayout->setMargin(0);
    gridLayout->setSpacing(0);

    scrlArea = new QScrollArea;
    scrlArea->setContentsMargins(0, 0, 0, 0);
    scrlArea->setFrameStyle(0);
    scrlArea->setLayout(gridLayout);

    mainLayout = new QHBoxLayout();
    mainLayout->setContentsMargins(0, 0, 0, 0);
//    mainLayout->setSpacing(1);
    mainLayout->addWidget(scrlArea);
    this->setLayout(mainLayout);

    ivList = new QList<ImageView*>;

}

bool CompareView::load(const QSize &centralWidgetSize)
{
    cw = centralWidgetSize;
    ivList->clear();
    while (gridLayout->count() > 0) {
        QWidget *w = gridLayout->itemAt(0)->widget();
        gridLayout->removeWidget(w);
    }
    selection = this->thumbView->selectionModel()->selectedIndexes();
    count = selection.count();
    if (count > 9) count = 9;
    for (int i = 0; i < count; ++i) {
        ivList->append(new ImageView(this, metadata, imageCacheThread, thumbView, false, true));
        QString fPath = selection.at(i).data(thumbView->FileNameRole).toString();
        bool isPick = qvariant_cast<bool>(selection.at(i).data(thumbView->PickedRole));
        ivList->at(i)->pickLabel->setVisible(isPick);
        ivList->at(i)->loadImage(selection.at(i), fPath);
        connect(ivList->at(i), SIGNAL(compareZoom(QPointF, QModelIndex, bool)),
                this, SLOT(zoom(QPointF, QModelIndex, bool)));
        connect(ivList->at(i), SIGNAL(comparePan(QPoint, QModelIndex)),
                this, SLOT(pan(QPoint, QModelIndex)));
    }
    configureGrid();
    loadGrid();
//    ivList->at(0)->setFocus();
//    ivList->at(0)->setStyleSheet("QLabel {border: white;}");
}

void CompareView::loadGrid()
{
    int i = 0;
    int row, col;
    for (row = 0; row < rows; ++row) {
        for (col = 0; col < cols; ++col) {
            gridLayout->addWidget(ivList->at(i), row, col);
            i++;
            if (i == count) break;
        }
        if (i == count) break;
    }
}

void CompareView::configureGrid()
{
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

long CompareView::area(int rows, int cols)
{
    QSize cell(cw.width() / cols, cw.height() / rows);
    long area = 0;

    for (int i = 0; i < count; ++i) {
        QSize imSize = ivList->at(i)->imageSize();
        imSize.scale(cell, Qt::KeepAspectRatio);
        area += imSize.width() * imSize.height();
    }
    return area;
}

void CompareView::pick(bool isPick, QModelIndex idx)
{
//    qDebug() << "CompareView::pick" << isPick << idx;
    for (int i = 0; i < ivList->count(); ++i) {
        if (ivList->at(i)->imageIndex == idx) {
            ivList->at(i)->pickLabel->setVisible(isPick);
        }
    }
}

void CompareView::showShootingInfo(bool isVisible)
{
    for (int i = 0; i < ivList->count(); ++i) {
            ivList->at(i)->infoDropShadow->setVisible(isVisible);
    }
}

void CompareView::zoom(QPointF coord, QModelIndex idx, bool isZoom)
{
    qDebug() << "CompareView::zoom";
    for (int i = 0; i < ivList->count(); ++i) {
        if (ivList->at(i)->imageIndex != idx) {
            ivList->at(i)->compareZoomAtCoord(coord, isZoom);
        }
    }
}

void CompareView::pan(QPoint delta, QModelIndex idx)
{
    qDebug() << "CompareView::pan";
    for (int i = 0; i < ivList->count(); ++i) {
        if (ivList->at(i)->imageIndex != idx) {
            ivList->at(i)->deltaMoveImage(delta);
        }
    }
}
