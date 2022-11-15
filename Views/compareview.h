#ifndef COMPAREVIEW_H
#define COMPAREVIEW_H

#include <QtWidgets>
#include "Metadata/metadata.h"
#include "Datamodel/datamodel.h"
#include "Cache/imagecache.h"
#include "Views/iconview.h"
#include "Utilities/classificationlabel.h"
#include "Image/pixmap.h"

class CompareView : public QGraphicsView
{
    Q_OBJECT

public:
    CompareView(QWidget *parent,
                QSize gridCell,
                DataModel *dm,
                Metadata *metadata,
                ImageCacheData *icd,
                IconView *thumbView);

    qreal zoom;
    qreal toggleZoom;

    QModelIndex imageIndex;
    QGraphicsPixmapItem *pmItem;        // req'd by imageAlign

    bool loadImage(QModelIndex idx, QString imageFileName);
    QPointF getScrollPct();
    void setScrollBars(QPointF scrollPct);
    void slavePanToDeltaPct(QPointF delta);
    void slaveSetPanStartPct();
    void slaveCleanupAfterPan(QPointF deltaPct);

    void select();
    void deselect();

    ClassificationLabel *classificationLabel;

public slots:
    void slaveZoomToPct(QPointF coord, bool isZoom);

    void zoomIn();
    void zoomOut();
    void zoomToFit();
    void zoomTo(qreal zoomTo);
    void zoomToggle();

signals:
    void togglePick();
    void zoomFromPct(QPointF scrollPct, QModelIndex idx, bool isZoom);
    void panFromPct(QPointF scrollPct, QModelIndex idx);
    void panStartPct(QModelIndex idx);
    void cleanupAfterPan(QPointF deltaPct, QModelIndex idx);
    void align(QPointF scrollPct, QModelIndex idx); // not used
    void zoomChange(qreal zoomValue, bool hasfocus);
    void deselectAll();

private slots:
    void scrollEvent();

protected:
    void resizeEvent(QResizeEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *wheelEvent) override;
    void enterEvent(QEnterEvent *event) override;
    void leaveEvent(QEvent *event) override;
    void dragEnterEvent(QDragEnterEvent *event) override;

private:
    QWidget *mainWindow;
    QSize gridCell;
    DataModel *dm;
    Metadata *metadata;
    ImageCacheData *icd;
    IconView *thumbView;
    Pixmap *pixmap;
    QImageReader imageReader;

    QFrame *frame;

    QGraphicsScene *scene;
    QTransform matrix;      // qt6.2

    QPixmap displayPixmap;
    QImage thumbsUp;

    struct intSize
    {
        int w;
        int h;
        int x;
        int y;
    };

    struct scrollStatus {
        int hVal;
        int hMax;
        int hMin;
        qreal hPct;
        int vVal;
        int vMax;
        int vMin;
        qreal vPct;
    } scrl;

    QString currentImagePath;
    QPoint mousePt;             // not used
    QPointF offset;             // offset for "thumb up" and edits Labels
    QPointF scrollPosPct;       // current position
    QPoint mousePressPt;        // for mouse scrolling
    QPointF startOfPanPct;
    QPointF endOfPanPct;
    QPointF deltaPanPct;
//    QPointF getOffset(QPointF scrollPct);

    // Status
    bool isZoom;                // zoom is greater than req'd to fit image
    bool isMouseDrag;
    bool isMouseDoubleClick;
    bool isLeftMouseBtnPressed;
    bool isMouseClickZoom;      // zoom initiated by mouse click
    bool propagate = true;      // sync pan (scrollEvent)

    qreal zoomFit;
    qreal zoomInc = 0.1;    // 10% delta
    qreal zoomMin = 0.05;   // 5% of original  rgh add to pref
    qreal zoomMax = 8.0;    // 800% of original

    qreal getFitScaleFactor(QSize container, QRectF content);
    void scale(bool okayToPropagate);
    qreal getZoom();
    void getScrollBarStatus();
    void reportScrollBarStatus(QString info = "");
    QPointF getScrollDeltaPct();        //
    QPointF getScrollPct(QPoint p);
    QPointF getSceneCoordFromPct(QPointF scrollPct);    // not used

    void placeClassificationBadge();
    bool previewFitsZoom();

    void rotateByExifRotation(QImage &image, QString &imageFullPath);
};

#endif // COMPAREVIEW_H
