#ifndef IMAGEVIEW_H
#define IMAGEVIEW_H

#include <QtWidgets>
#include <QHash>
#include "Datamodel/datamodel.h"
#include "Datamodel/selection.h"
#include "Cache/imagecache.h"
//#include "Cache/cachedata.h"
#include "Views/iconview.h"
#include "Views/infostring.h"
#include "Utilities/dropshadowlabel.h"
#include "Utilities/classificationlabel.h"
#include "Image/pixmap.h"
#include "Embellish/embel.h"

class ImageView : public QGraphicsView
{
    Q_OBJECT

public:
    ImageView(QWidget *parent,
              QWidget *centralWidget,
              Metadata *metadata,
              DataModel *dm,
              ImageCacheData *icd,
              Selection *sel,
              IconView *thumbView,
              InfoString *infoString = nullptr,
              bool isShootingInfoVisible = false,
              bool isRatingBadgeVisible = false,
              int classificationBadgeDiam = 0,
              int infoOverlayFontSize = 0);

    QGraphicsScene *scene;

    QGraphicsPixmapItem *pmItem;
    QTransform transform;
    Pixmap *pixmap;
    QString currentImagePath;

    int cwMargin = 20;
    qreal imAspect = 1;
    qreal zoom;
    qreal zoomFit;
    bool isFit;
    qreal refZoom;                      // adjusted to real screen pixels
    qreal toggleZoom;
    bool isRubberBand;
    int wheelDeltaThreshold = 20;

    DropShadowLabel *infoOverlay;
    DropShadowLabel *titleDropShadow;

    QByteArray tileBa;
    QString tileName;

    void exportImage();
    qreal getFitScaleFactor(QRectF container, QRectF content);
    void clear();
    void setCursorHiding(bool hide);
    bool isBusy;
    void setBackgroundColor(QColor bg);
    bool isNullImage();

    void rotateByExifRotation(QImage &image, QString &imageFullPath);
    void rotateImage(int degrees);
    void setShootingInfo(QString infoString);
    void updateShootingInfo();
    QPoint scene2CW(QPointF pctPt);
    void focus();
    void sceneGeometry(QPoint &sceneOrigin, QRectF &sceneR, QRect &centralWidgetRect);
    QString shootingInfo;
    int infoOverlayFontSize;
    ClassificationLabel *classificationLabel;
    bool isFirstImageNewFolder;              // new folder, first image, set zoom = fit
    bool limitFit100Pct = true;

    void showRubber(QRect r);

    QString diagnostics();

public slots:
    bool loadImage(QString fPath, QString src = "");
    void monitorCursorState();
    void copyImage();
    void thumbClick(float xPct, float yPct);
    void updateToggleZoom(qreal toggleZoomValue);
    void zoomIn();
    void zoomOut();
    void zoomToFit();
    void zoomTo(qreal zoomTo);
    void zoomToggle();
    void resetFitZoom();
    void setFitZoom();
    void hideCursor();
    void refresh();
    void setClassificationBadgeImageDiam(int d);
    void activateRubberBand();
    void quitRubberBand();

signals:
    void tryAgain(QString fPath);
    void togglePick();
    void updateStatus(bool, QString, QString);
    void setCentralMessage(QString msg);
    void killSlideshow();                   // only call when slideshow is active
    void keyPress(QKeyEvent *event);
    void mouseSideKeyPress(int direction);  // logitech mouse NativeGesture event
    void zoomChange(qreal zoomValue, QString src);
    void handleDrop(QString fPath);
//    void handleDrop(QDropEvent *event);
//    void handleDrop(const QMimeData *mimeData);
    void embellish(QString fPath, QString src);
    void newTile();

private slots:
    void wheelStopped();
//    void upgradeToFullSize();

protected:
    void resizeEvent(QResizeEvent *event) override;
    void scrollContentsBy(int dx, int dy) override;
    void wheelEvent(QWheelEvent *event) override;
    void nativeGestureEvent(QNativeGestureEvent *event);
    bool event(QEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void enterEvent(QEnterEvent *event) override;
    void leaveEvent(QEvent *event) override;

//    void dropEvent(QDropEvent *event) override;
    void dragEnterEvent(QDragEnterEvent *event) override;

//    void paintEvent(QPaintEvent *event) override;
//    void drawForeground(QPainter *painter, const QRectF &rect) override;

private:
    void noJpgAvailable();
    void scale();
    qreal getZoom();

    QPointF getScrollPct();
    void getScrollBarStatus();
    void setScrollBars(QPointF scrollPct);

    QWidget *mainWindow;
    Metadata *metadata;
    DataModel *dm;
    Selection *sel;
    ImageCacheData *icd;
    IconView *thumbView;

    bool sceneBiggerThanView();
    bool resizeIsSmaller();
    void placeClassificationBadge();

    InfoString *infoString;

    QTimer *mouseMovementTimer;                     // control cursor during slideshow
    QElapsedTimer *mouseMovementElapsedTimer;       // detect valid mouse drag
    int mouseMovementPixels;                        // detect valid mouse drag
    QTimer wheelTimer;
    bool wheelSpinningOnEntry;

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
    QPoint mousePressPt;        // for mouse scrolling and mouse drag
    QPoint mouseDragPt;         // for mouse scrolling and mouse drag

//    QPointF compareMouseRelLoc;
    QSize preview;
    QSize full;

    int classificationBadgeDiam;

    bool cursorIsHidden;        // use for slideshow and full screen - review rgh
    bool moveImageLocked;       // control when con drag image around
    bool isScrollable;
    bool isMouseDrag;
    bool isTrackpadScroll;
    bool isLeftMouseBtnPressed;
    bool isMouseDoubleClick;

    int scrollCount;

//    qreal previewScaleMax;
    qreal zoomInc = 0.1;    // 10% delta
    qreal zoomMin = 0.05;   // 5% of original  rgh add to pref
    qreal zoomMax = 8.0;    // 800% of original
//    qreal zoom100Pct;

    QPoint origin;
    QRubberBand *rubberBand;
};

#endif // IMAGEVIEW_H

