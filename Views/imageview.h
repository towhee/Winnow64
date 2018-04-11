#ifndef IMAGEVIEW_H
#define IMAGEVIEW_H

#include <QtWidgets>
#include <QHash>
#include "Metadata/metadata.h"
#include "Cache/imagecache.h"
#include "Views/thumbview.h"
#include "Views/infostring.h"
#include "Utilities/dropshadowlabel.h"
#include "Utilities/circlelabel.h"
#include "Image/pixmap.h"

class ImageView : public QGraphicsView
{
    Q_OBJECT

public:
    ImageView(QWidget *parent,
              QWidget *centralWidget,
              Metadata *metadata,
              ImageCache *imageCacheThread,
              ThumbView *thumbView,
              InfoString *infoString,
              bool isShootingInfoVisible, bool isRatingBadgeVisible);

    qreal zoom;
    qreal refZoom;                      // adjusted to real screen pixels
    qreal toggleZoom;

    DropShadowLabel *infoDropShadow;
    DropShadowLabel *titleDropShadow;
    QModelIndex imageIndex;

    bool loadImage(QModelIndex idx, QString imageFileName);
    void noImagesAvailable();
    void setCursorHiding(bool hide);
    bool isBusy;
    bool useWheelToScroll;

    void rotateByExifRotation(QImage &image, QString &imageFullPath);
    void rotate(int degrees);
    void moveShootingInfo(QString infoString);
    QLabel *pickLabel;      // visibility controlled in MW
    CircleLabel *classificationLabel;
    QPixmap *pickPixmap;

public slots:
    void monitorCursorState();
    void copyImage();
    void thumbClick(float xPct, float yPct);
    void updateToggleZoom(qreal toggleZoomValue);
    void zoomIn();
    void zoomOut();
    void zoomToFit();
    void zoomTo(qreal zoomTo);
    void zoomToggle();

signals:
    void togglePick();
    void updateStatus(bool, QString);
    void zoomChange(qreal zoomValue);
    void handleDrop(const QMimeData *mimeData);

private slots:
    void upgradeToFullSize();

protected:
//    bool eventFilter(QObject *obj, QEvent *event);
    void resizeEvent(QResizeEvent *event);
    void scrollContentsBy(int dx, int dy);
//    void dragMoveEvent(QDragMoveEvent *event);
//    void showEvent(QShowEvent *event);
    void wheelEvent(QWheelEvent *event);
    void mouseMoveEvent(QMouseEvent *event);
    void mouseDoubleClickEvent(QMouseEvent *event);
    void mousePressEvent(QMouseEvent *event);
    void mouseReleaseEvent(QMouseEvent *event);
    void enterEvent(QEvent *event);
    void paintEvent(QPaintEvent *event);
    void dropEvent(QDropEvent *event);
    void dragEnterEvent(QDragEnterEvent *event);


private:
    void noJpgAvailable();
    qreal getFitScaleFactor(QRectF container, QRectF content);
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
    void transform();

    QWidget *mainWindow;
    QWidget *centralWidget;
    Metadata *metadata;
    ImageCache *imageCacheThread;
    ThumbView *thumbView;
//    bool &isRatingBadgeVisible;
    InfoString *infoString;
    Pixmap *pixmap;
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

    QPointF scrollPct;          // current position
    QPoint mousePressPt;        // for mouse scrolling

//    QPointF compareMouseRelLoc;
    QSize preview;
    QSize full;

    QString currentImagePath;
    QString shootingInfo;
    bool firstImageLoaded;

    bool cursorIsHidden;        // use for slideshow and full screen - review rgh
    bool moveImageLocked;       // control when con drag image around
    bool isZoom;
    bool isFit;
    bool wasZoomFit;
    bool isMouseDrag;
    bool isTrackpadScroll;
    bool isLeftMouseBtnPressed;
    bool isMouseDoubleClick;
    bool isPreview;

    int scrollCount;

    qreal zoomFit;
//    qreal previewScaleMax;
    qreal zoomInc = 0.1;    // 10% delta
    qreal zoomMin = 0.05;   // 5% of original  rgh add to pref
    qreal zoomMax = 8.0;    // 800% of original

};

#endif // IMAGEVIEW_H

