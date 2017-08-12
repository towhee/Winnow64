#ifndef IMAGEVIEW_H
#define IMAGEVIEW_H

#include <QtWidgets>
#include <QHash>
#include "metadata.h"
#include "imagecache.h"
#include "thumbcache.h"
#include "dropshadowlabel.h"
#include "pixmap.h"

class ImageView : public QGraphicsView
{
    Q_OBJECT

public:
    ImageView(QWidget *parent, QWidget *centralWidget, Metadata *metadata,
              ImageCache *imageCacheThread, ThumbView *thumbView,
              bool isShootingInfoVisible);

//    QScrollArea *scrlArea;
    QLabel *infoLabel;
    QLabel *infoLabelShadow;
    DropShadowLabel *infoDropShadow;
    QLabel *titleLabel;
    QLabel *titleLabelShadow;
    DropShadowLabel *titleDropShadow;
    qreal zoom;
    QModelIndex imageIndex;

    bool loadPixmap(QString &imageFullPath, QPixmap &pm);
    bool loadImage(QModelIndex idx, QString imageFileName);
    void setCursorHiding(bool hide);
    bool isBusy;
//    void setClickZoom(float clickZoom);

//    void compareZoomAtCoord(QPointF coord, bool isZoom);
//    void deltaMoveImage(QPoint &delta);                   // used by compare

//    void setImageLabelSize(QSize newSize);                  // req'd by compare?
//    QSize imageSize();                                      // compare?

    void rotateByExifRotation(QImage &image, QString &imageFullPath);
    void moveShootingInfo(QString infoString);
    QLabel *pickLabel;      // visibility controlled in MW
    QPixmap *pickPixmap;

public slots:
    void monitorCursorState();
    void copyImage();
    void thumbClick(float xPct, float yPct);
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
    void updateStatus(bool, QString);

private slots:
    void upgradeToFullSize();

protected:
    void resizeEvent(QResizeEvent *event);
//    void showEvent(QShowEvent *event);
    void mouseMoveEvent(QMouseEvent *event);
    void mouseDoubleClickEvent(QMouseEvent *event);
    void mousePressEvent(QMouseEvent *event);
    void mouseReleaseEvent(QMouseEvent *event);
    void enterEvent(QEvent *event);
    void paintEvent(QPaintEvent *event);

private:
    QWidget *mainWindow;
    QWidget *centralWidget;
    Metadata *metadata;
    ImageCache *imageCacheThread;
    ThumbView *thumbView;
    QImageReader imageReader;
    QLabel *imageLabel;

    QGraphicsScene *scene;
    QGraphicsPixmapItem *pmItem;
    QGraphicsTextItem *infoItem;
    QMatrix matrix;

    QPixmap displayPixmap;
    QImage displayImage;

    QTimer *mouseMovementTimer;
    QTimer *loadFullSizeTimer;
    QElapsedTimer t;
    QElapsedTimer t1;

    struct intSize
    {
        int w;
        int h;
        int x;
        int y;
    };

//    intSize f;     // label in fit view

    struct floatSize
    {
        float w;
        float h;
    };

    struct pt
    {
        int x;
        int y;
    };

    typedef pt pt;
    pt mouse;

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

    QPointF scrollPct;       // current position

//    QPointF compareMouseRelLoc;
    QSize preview;
    QSize full;

    QString currentImagePath;
    QString shootingInfo;
    bool firstImageLoaded;

    bool cursorIsHidden;                // use for slideshow and full screen - review rgh
    bool moveImageLocked;               // control when con drag image around
    bool isZoom;
    bool isFit;
    bool wasZoomFit;
    bool isMouseDrag;
    bool isMouseDoubleClick;
    bool isPreview;

    qreal zoomFit;
//    qreal previewScaleMax;
    qreal zoomInc = 0.1;    // 10% delta
    qreal zoomMin = 0.05;   // 5% of original  rgh add to pref
    qreal zoomMax = 8.0;    // 800% of original
    qreal clickZoom = 1.0;  // for now, put in QSetting

    qreal getFitScaleFactor(QRectF container, QRectF content);
//    qreal getPreviewScaleMax();
//    qreal getPreviewToFull();
    void scale();
    qreal getZoom();

    QPointF getScrollPct();
    void getScrollBarStatus();
    void setScrollBars(QPointF scrollPct);

    void setPreviewDim();
    void setFullDim();

    bool sceneBiggerThanView();
    bool resizeIsSmaller();

    void movePickIcon();
    void setMouseMoveData(bool lockMove, int lMouseX, int lMouseY);
//    bool previewFitsZoom();

    void transform();
};

#endif // IMAGEVIEW_H

