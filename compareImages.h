#ifndef COMPARE_H
#define COMPARE_H

#include <QtWidgets>
#include "metadata.h"
#include "thumbview.h"
#include "compareview.h"
#include "imagealign.h"

class CompareImages : public QWidget
{
    Q_OBJECT

public:
    CompareImages(QWidget *parent, QWidget *centralWidget, Metadata *metadata,
                  ThumbView *thumbView, ImageCache *imageCacheThread);
    bool load(const QSize &central);
    void pick(bool isPick, QModelIndex idx);
    void ratingColorClass(QString rating, QString colorClass, QModelIndex idx);
    void zoomOut();
    void zoomIn();
    void zoomToFit();
    void zoom50();
    void zoom100();
    void zoom200();
    void zoomTo(float zoomTo);
    void zoomToggle();

private slots:
    void zoom(QPointF coord, QModelIndex idx, bool isZoom);
    void pan(QPointF scrollPct, QModelIndex idx);
    void align(QPointF basePos, QModelIndex idx);

private:
    Metadata *metadata;
    ThumbView *thumbView;
    ImageCache *imageCacheThread;
    QWidget *centralWidget;

//    QGraphicsGridLayout *gridLayout;
    QGridLayout *gridLayout;

    QList<CompareView*> *imList;
    QList <QSize> *sizeList;
    QModelIndexList selection;

    ImageAlign *imageAlign;

    // req'd to receive pt from imageView
    struct pt
    {
        int x;
        int y;
    };

    QSize cw;       // central widget in parent
    int count;      // number of images to compare
    int rows;
    int cols;

    void configureGrid();
    void loadGrid();
    long area(int rows, int cols);
};

#endif // COMPARE_H
