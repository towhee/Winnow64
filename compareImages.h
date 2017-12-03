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

    qreal toggleZoom;
    qreal zoomValue;
    bool load(const QSize &central);
    void pick(bool isPick, QModelIndex idx);
    void ratingColorClass(QString rating, QString colorClass, QModelIndex idx);

public slots:
    void zoomChangeFromView(qreal zoomValue);
    void updateToggleZoom(qreal toggleZoomValue);

    void go(QString key);
    void zoomOut();
    void zoomIn();
    void zoomToFit();
    void zoom50();
    void zoom100();
    void zoom200();
    void zoomTo(qreal zoomValue);
    void zoomToggle();

signals:
    void updateStatus(bool, QString);
    void zoomChange(qreal zoomValue);

private slots:
    void zoom(QPointF coord, QModelIndex idx, bool isZoom);
    void pan(QPointF scrollPct, QModelIndex idx);
    void startPan(QModelIndex idx);
    void cleanupAfterPan(QPointF deltaPct, QModelIndex idx);
    void align(QPointF basePos, QModelIndex idx);

private:
    Metadata *metadata;
    ThumbView *thumbView;
    ImageCache *imageCacheThread;
    QWidget *centralWidget;

    QGridLayout *gridLayout;

    QList<CompareView*> *imList;
    QList <QSize> *sizeList;
    QModelIndexList selection;

    ImageAlign *imageAlign;

    // req'd to receive pt from compareView
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
    int current();    // compare index row for currentIndex
};

#endif // COMPARE_H
