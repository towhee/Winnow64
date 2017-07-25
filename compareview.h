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

    bool loadPixmap(QString &imageFullPath, QPixmap &pm);
    bool loadImage(QModelIndex idx, QString imageFileName);
    void panToPct(QPointF scrollPct);

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

private slots:
    void scrollEvent();

protected:
    void resizeEvent(QResizeEvent *event);
    void mousePressEvent(QMouseEvent *event);
    void mouseReleaseEvent(QMouseEvent *event);
    void enterEvent(QEvent *event);

private:
    QWidget *mainWindow;
    QSize gridCell;
    Metadata *metadata;
    ImageCache *imageCacheThread;
    ThumbView *thumbView;
    QImageReader imageReader;

    QGraphicsScene *scene;
    QGraphicsPixmapItem *pmItem;
    QMatrix matrix;

    QPixmap displayPixmap;
    QImage thumbsUp;

    QString currentImagePath;

    bool moveImageLocked;               // control when con drag image around
    bool isZoom;
    QPoint mousePt;
    bool isMouseDrag;
    bool isMouseDoubleClick;
    bool isPreview;

    qreal zoomFit;
    qreal zoomInc = 0.1;    // 10% delta
    qreal zoomMin = 0.05;   // 5% of original  rgh add to pref
    qreal zoomMax = 8.0;    // 800% of original
    qreal clickZoom = 1.0;

    qreal getFitScaleFactor(QSize container, QRectF content);
    void scale(bool propagate);
    qreal getZoom();
    QPointF getScrollPct();
    QPointF getMousePct();
//    QPointF getScrollPctFromCenter();
    QPointF getSceneCoordFromPct(QPointF scrollPct);

    void movePickIcon();
    bool previewFitsZoom();

    void rotateByExifRotation(QImage &image, QString &imageFullPath);
};

#endif // COMPAREVIEW_H
