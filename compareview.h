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
    void pick(bool isPick, QModelIndex idx);
    void showShootingInfo(bool isVisible);

public slots:
    void zoom(QPointF coord, QModelIndex idx, bool isZoom);

private:
    Metadata *metadata;
    ThumbView *thumbView;
    ImageCache *imageCacheThread;

    QHBoxLayout *mainLayout;
    QScrollArea *scrlArea;
    QGridLayout *gridLayout;

    QList<ImageView*> *ivList;
    QModelIndexList selection;

    QSize cw;       // central widget in parent
    int count;      // number of images to compare
    int rows;
    int cols;

    void configureGrid();
    void loadGrid();
    long area(int rows, int cols);
};

#endif // COMPARE_H
