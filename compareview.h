#ifndef COMPAREVIEW_H
#define COMPAREVIEW_H

#include <QtWidgets>
#include "metadata.h"
#include "imagecache.h"
#include "thumbcache.h"

class CompareView : public QGraphicsView
{
    Q_OBJECT

public:
    CompareView(QWidget *parent, QSize gridCell, Metadata *metadata,
              ImageCache *imageCacheThread, ThumbView *thumbView);

    qreal zoom;
    QModelIndex imageIndex;
    QGraphicsPixmapItem *pmItem;        // req'd by imageAlign

    bool loadPixmap(QString &imageFullPath, QPixmap &pm);
    bool loadImage(QModelIndex idx, QString imageFileName);
    QPointF getScrollPct();
    void panToPct(QPointF scrollPct);
    void panToDeltaPct(QPointF delta);
    void resetMouseClickZoom();
    QLabel *pickLabel;      // visibility controlled in MW

public slots:
    void zoomToPct(QPointF coord, bool isZoom);

    void setClickZoom(float clickZoom);
    void zoomIn();
    void zoomOut();
    void zoomToFit();
    void zoom50();
    void zoom100();
    void zoom200();
    void zoomTo(float zoomTo);
    void zoomToggle();

signals:
    void togglePick();
    void zoomFromPct(QPointF scrollPct, QModelIndex idx, bool isZoom);
    void panFromPct(QPointF scrollPct, QModelIndex idx);
    void align(QPointF scrollPct, QModelIndex idx);

private slots:
    void scrollEvent();

protected:
    void resizeEvent(QResizeEvent *event);
    void mousePressEvent(QMouseEvent *event);
    void mouseReleaseEvent(QMouseEvent *event);
    void wheelEvent(QWheelEvent *wheelEvent);
    void enterEvent(QEvent *event);
    void leaveEvent(QEvent *event);

private:
    QWidget *mainWindow;
    QSize gridCell;
    Metadata *metadata;
    ImageCache *imageCacheThread;
    ThumbView *thumbView;
    QImageReader imageReader;

    QFrame *frame;

    QGraphicsScene *scene;
    QMatrix matrix;

    QPixmap displayPixmap;
    QPixmap *pickPixmap;
    QImage thumbsUp;

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
    QPoint mousePt;
    QPointF offset;
    QPointF scrollPosPct;       // current position
//    QPointF getOffset(QPointF scrollPct);

    // Status
    bool isZoom;                // zoom is greater than req'd to fit image
    bool isMouseDrag;
    bool isMouseDoubleClick;
    bool isMouseClickZoom;      // zoom initiated by mouse click
    bool propagate = true;      // sync pan (scrollEvent)

    qreal zoomFit;
    qreal zoomInc = 0.1;    // 10% delta
    qreal zoomMin = 0.05;   // 5% of original  rgh add to pref
    qreal zoomMax = 8.0;    // 800% of original
    qreal clickZoom = 1.0;

    qreal getFitScaleFactor(QSize container, QRectF content);
    void scale(bool okayToPropagate);
    qreal getZoom();
    void getScrollBarStatus();
    void setScrollBars(QPointF scrollPct);
    void reportScrollBarStatus();
    QPointF getScrollDeltaPct();
//    QPointF getMousePct();
//    QPointF getScrollPctFromCenter();
    QPointF getSceneCoordFromPct(QPointF scrollPct);

    void movePickIcon();
    bool previewFitsZoom();

    void rotateByExifRotation(QImage &image, QString &imageFullPath);
};

#endif // COMPAREVIEW_H
