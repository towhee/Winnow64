#ifndef COMPARE_H
#define COMPARE_H

#include <QtWidgets>
#include "Metadata/metadata.h"
#include "Views/iconview.h"
#include "Views/compareview.h"
#include "Image/imagealign.h"
#include "Datamodel/datamodel.h"
#include "Datamodel/selection.h"

class CompareImages : public QWidget
{
    Q_OBJECT

public:
    CompareImages(QWidget *parent, QWidget *centralWidget, Metadata *metadata, DataModel *dm,
                  Selection *sel, IconView *thumbView, ImageCacheData *icd);

    qreal toggleZoom;
    qreal zoomValue;

    bool load(const QSize &central, bool isRatingBadgeVisible, QItemSelectionModel *selectionModel);
//    void pick(bool isPick, QModelIndex idx);
    void updateClassification(bool isPick, QString rating, QString colorClass,
                              bool isRatingBadgeVisible, QModelIndex idx);
    void test();

protected:
    void resizeEvent(QResizeEvent *event);

public slots:
    void zoomChangeFromView(qreal zoomValue, bool hasfocus);
    void updateToggleZoom(qreal toggleZoomValue);

    QModelIndex go(QString key);
    void zoomOut();
    void zoomIn();
    void zoomToFit();
    void zoomTo(qreal zoomValue);
    void zoomToggle();

signals:
    void updateStatus(bool, QString, QString);
    void zoomChange(qreal zoomValue);
    void togglePick();

private slots:
    void zoom(QPointF coord, QModelIndex idx, bool isZoom);
    void pan(QPointF scrollPct, QModelIndex idx);
    void startPan(QModelIndex idx);
    void cleanupAfterPan(QPointF deltaPct, QModelIndex idx);
    void align(QPointF, QModelIndex idx);
    void togglePickSignalRelay();
    void deselectAll();

private:
    Metadata *metadata;
    DataModel *dm;
    Selection *sel;
    IconView *thumbView;
    ImageCacheData *icd;
    QWidget *centralWidget;
//    bool &isRatingBadgeVisible;

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
