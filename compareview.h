#ifndef COMPARE_H
#define COMPARE_H

#include <QtWidgets>
#include "metadata.h"
#include "thumbview.h"
#include "imageview.h"

class CompareView : public QWidget
{
    Q_OBJECT

public:
    CompareView(QWidget *parent, Metadata *metadata, ThumbView *thumbView,
                ImageCache *imageCacheThread);
    bool load(const QSize &centralWidgetSize);

private:
    Metadata *metadata;
    ThumbView *thumbView;
    ImageCache *imageCacheThread;

    QHBoxLayout *mainLayout;
    QScrollArea *scrlArea;
    QGridLayout *gridLayout;

    QList<ImageView*> *ivList;

    QSize cw;       // central widget in parent
    int rows;
    int cols;

    void configureGrid(QModelIndexList sel);
    void loadGrid();
};

#endif // COMPARE_H
