#ifndef IMAGEVIEW_H
#define IMAGEVIEW_H

#include <QtWidgets>
#include <QHash>
#include "Datamodel/datamodel.h"
//#include "Metadata/metadata.h"
#include "Cache/imagecache.h"
#include "Views/iconview.h"
#include "Views/infostring.h"
#include "Utilities/dropshadowlabel.h"
#include "Utilities/classificationlabel.h"
#include "Image/pixmap.h"

#include "Dialogs/patterndlg.h"

//#ifdef Q_OS_WIN
//#include "Utilities/icc.h"
//#endif


class ImageView : public QGraphicsView
{
    Q_OBJECT

public:
    ImageView(QWidget *parent,
              QWidget *centralWidget,
              Metadata *metadata,
              DataModel *dm,
              ImageCache *imageCacheThread,
              IconView *thumbView,
              InfoString *infoString,
              bool isShootingInfoVisible,
              bool isRatingBadgeVisible,
              int classificationBadgeDiam,
              int infoOverlayFontSize);

    qreal zoom;
    qreal zoomFit;
    bool isFit;
    qreal refZoom;                      // adjusted to real screen pixels
    qreal toggleZoom;

    DropShadowLabel *infoOverlay;
    DropShadowLabel *titleDropShadow;

    bool loadImage(QString imageFileName);
    qreal getFitScaleFactor(QRectF container, QRectF content);
    void clear();
    void setCursorHiding(bool hide);
    bool isBusy;
    void setBackgroundColor(QColor bg);

    void rotateByExifRotation(QImage &image, QString &imageFullPath);
    void rotate(int degrees);
    void moveShootingInfo(QString infoString);
    void sceneGeometry(QPoint &sceneOrigin, QRectF &sceneR, QRect &centralWidgetRect);
    QString shootingInfo;
    int infoOverlayFontSize;
    ClassificationLabel *classificationLabel;
    bool isFirstImageNewFolder;              // new folder, first image, set zoom = fit
    bool limitFit100Pct = true;

    QString diagnostics();

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
    void setFitZoom();
    void hideCursor();
    void refresh();
    void setClassificationBadgeImageDiam(int d);
    void activateRubberBand();

signals:
    void togglePick();
    void updateStatus(bool, QString, QString);
    void killSlideshow();                   // only call when slideshow is active
    void zoomChange(qreal zoomValue);
    void handleDrop(const QMimeData *mimeData);

private slots:
    void upgradeToFullSize();

protected:
    void resizeEvent(QResizeEvent *event);
    void scrollContentsBy(int dx, int dy);
    void wheelEvent(QWheelEvent *event);
    void mouseDoubleClickEvent(QMouseEvent *event);
    void mousePressEvent(QMouseEvent *event);
    void mouseMoveEvent(QMouseEvent *event);
    void mouseReleaseEvent(QMouseEvent *event);
    void enterEvent(QEvent *event);
    void paintEvent(QPaintEvent *event);
    void dropEvent(QDropEvent *event);
    void dragEnterEvent(QDragEnterEvent *event);


private:
    void noJpgAvailable();
    void scale();
    qreal getZoom();

    QPointF getScrollPct();
    void getScrollBarStatus();
    void setScrollBars(QPointF scrollPct);

    void setPreviewDim();
    void setFullDim();

    bool sceneBiggerThanView();
    bool resizeIsSmaller();
    void placeClassificationBadge();
    void transform();

    QWidget *mainWindow;
    QWidget *centralWidget;
    Metadata *metadata;
    DataModel *dm;
    ImageCache *imageCacheThread;
    IconView *thumbView;
    InfoString *infoString;
    Pixmap *pixmap;

    QGraphicsScene *scene;
    QGraphicsPixmapItem *pmItem;
    QGraphicsTextItem *infoItem;
    QMatrix matrix;

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
    int classificationBadgeDiam;

    bool cursorIsHidden;        // use for slideshow and full screen - review rgh
    bool moveImageLocked;       // control when con drag image around
    bool isZoomToggled;
    bool isScrollable;
    bool isMouseDrag;
    bool isTrackpadScroll;
    bool isLeftMouseBtnPressed;
    bool isMouseDoubleClick;
    bool isPreview;

    int scrollCount;

//    qreal previewScaleMax;
    qreal zoomInc = 0.1;    // 10% delta
    qreal zoomMin = 0.05;   // 5% of original  rgh add to pref
    qreal zoomMax = 8.0;    // 800% of original
//    qreal zoom100Pct;

    bool isRubberBand;
    QPoint origin;
    QRubberBand *rubberBand;
};

#endif // IMAGEVIEW_H

