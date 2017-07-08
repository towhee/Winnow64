#include "compareview.h"
#include "global.h"

CompareView::CompareView(QWidget *parent, Metadata *metadata, ThumbView *thumbView,
                         ImageCache *imageCacheThread)
{
    this->metadata = metadata;
    this->thumbView = thumbView;
    this->imageCacheThread = imageCacheThread;

    gridLayout = new QGridLayout;
    gridLayout->setMargin(0);
    gridLayout->setSpacing(0);

    scrlArea = new QScrollArea;
    scrlArea->setContentsMargins(0, 0, 0, 0);
    scrlArea->setAlignment(Qt::AlignCenter);
    scrlArea->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scrlArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scrlArea->verticalScrollBar()->blockSignals(true);
    scrlArea->horizontalScrollBar()->blockSignals(true);
    scrlArea->setFrameStyle(0);
    scrlArea->setLayout(gridLayout);
    scrlArea->setWidgetResizable(true);

    mainLayout = new QHBoxLayout();
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);
    mainLayout->addWidget(scrlArea);
    this->setLayout(mainLayout);

    ivList = new QList<ImageView*>;
}

bool CompareView::load(const QSize &centralWidgetSize)
{
    cw = centralWidgetSize;
    qDebug() << "this vs sentralWidget size" << this->size() << cw;
    ivList->clear();
    configureGrid(this->thumbView->selectionModel()->selectedIndexes());
    loadGrid();
}

void CompareView::configureGrid(QModelIndexList sel)
{
    int n = thumbView->selectionModel()->selectedIndexes().count();

    switch (n) {
    case 2: rows = 1;   cols = 2;   break;
    case 3: rows = 1;   cols = 3;   break;
    case 4: rows = 2;   cols = 2;   break;
    case 5: rows = 2;   cols = 3;   break;
    case 6: rows = 2;   cols = 3;   break;
    case 7: rows = 3;   cols = 3;   break;
    case 8: rows = 3;   cols = 3;   break;
    case 9: rows = 3;   cols = 3;   break;
    }
}

void CompareView::loadGrid()
{
    ivList = new QList<ImageView*>;
    QModelIndexList sel = thumbView->selectionModel()->selectedIndexes();
    int n = sel.count();
    int i = 0;
    int row, col;
    for (row = 0; row < rows; ++row) {
        for (col = 0; col < cols; ++col) {
            ivList->append(new ImageView(this, metadata, imageCacheThread, false));
            ivList->at(i)->loadImage(sel.at(i).data(thumbView->FileNameRole).toString());
            gridLayout->addWidget(ivList->at(i), row, col);
            i++;
            if (i == n) break;
        }
    }
}
