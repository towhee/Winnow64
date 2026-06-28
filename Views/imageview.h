#ifndef IMAGEVIEW_H
#define IMAGEVIEW_H

#include <QtWidgets>
#include <QHash>
#include "Datamodel/datamodel.h"
#include "Datamodel/selection.h"
#include "Cache/imagecache.h"
#include "Views/iconview.h"
#include "Views/infostring.h"
#include "Utilities/dropshadowlabel.h"
#include "Utilities/classificationlabel.h"
#include "Image/pixmap.h"
#include "Embellish/embel.h"
#include "Utilities/focuspredictor.h"

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
    bool currentImageHasChanged = false;



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
    void setShootingInfo(QString infoString = "");
    void updateShootingInfo();
    QPoint scene2CW(QPointF pctPt);
    QSizeF vpNormSizeInScene();
    void focus();
    void sceneGeometry(QPoint &sceneOrigin, QRectF &sceneR, QRect &centralWidgetRect);
    void setBullseyeVisible(bool isVisible);
    void placeTarget(float x, float y);

    void changeInfoOverlay();
    QString infoText;
    int infoOverlayFontSize;

    ClassificationLabel *classificationLabel;
    // bool isFirstImageNewInstance;              // new folder, first image, set zoom = fit
    bool limitFit100Pct = true;

    bool panToFocus = false;

    void showRubber(QRect r);

    QString diagnostics();

public slots:
    bool loadImage(QString fPath, bool replace = false, QString src = "");
    /* Swap the displayed pixmap to an already-rendered QImage (a Develop preview) without
       re-decoding or refitting -- the dimensions are unchanged, only the pixels differ. */
    void setDevelopPreview(const QImage &image);
    void monitorCursorState();
    void copyImage();
    void panTo(float xPct, float yPct);
    void predictPanToFocus();
    QSize viewportInScene();
    void showNormalizedViewport(bool adjustCenter, bool refresh, QString src);
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

    /* Develop mask editing. beginMaskEdit makes a spatial mask tool the active overlay target (its
       geometry drawn over the image, draggable); endMaskEdit clears it. setMaskFeather live-updates
       the ramp softness from the Feather slider. Geometry is normalized image coords (0..1). */
    void beginMaskEdit(int tool, int op, bool inverted, const QString &paramsJson, double feather);
    void endMaskEdit();
    void setMaskFeather(double feather);
    void setMaskInverted(bool inverted);

signals:
    void togglePick();
    void updateStatus(bool, QString, QString);
    void setCentralMessage(QString msg);
    void killSlideshow();                   // only call when slideshow is active
    void keyPress(QKeyEvent *event);
    void mouseSideKeyPress(int direction);  // logitech mouse NativeGesture event
    void zoomChange(qreal zoomValue, QString src);
    void loupeRect(QSizeF vpSizeN, qreal vpA, QPointF vpCntr, bool refresh);
    void showLoupeRect(bool isVisible);

    void handleDrop(QString fPath);
//    void handleDrop(QDropEvent *event);
//    void handleDrop(const QMimeData *mimeData);
    void embellish(QString fPath, QString src);
    void newTile();
    void focusClick(QString path, float x, float y, QString type, QImage image);
    /* Cursor over the loupe: normalized (0..1) position within the displayed image, for the
       Develop scopes' live readout marker; cursorLeftImage when it leaves the image. */
    void cursorImagePos(double xFraction, double yFraction);
    void cursorLeftImage();
    /* The mask overlay was dragged: the active tool's new geometry as paramsJson, for the dock to
       persist into the MaskComponent. */
    void maskGeometryChanged(const QString &paramsJson);

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
    void drawForeground(QPainter *painter, const QRectF &rect) override;   // mask overlay

private:
    void noJpgAvailable();
    void scale(bool isNewImage = false);
    qreal getZoom();

    QPointF getScrollPct();
    void getScrollBarStatus();
    void setScrollBars(QPointF scrollPct);
    void scrollChange(int value);

    QWidget *mainWindow;
    Metadata *metadata;
    DataModel *dm;
    Selection *sel;
    ImageCacheData *icd;
    IconView *thumbView;
    FocusPredictor *focusPredictor;

    bool sceneBiggerThanView();
    bool resizeIsSmaller();
    void placeClassificationBadge();
    // void placeTarget(float x, float y);

    InfoString *infoString;

    QPointF focusPrediction;
    QLabel *bullseye;

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

    QPointF scrollPct;                    // current position
    QPoint mousePressPt;                  // for mouse scrolling and mouse drag
    QPoint mouseDragPt;                   // for mouse scrolling and mouse drag
    QPointF vpCntrN = QPointF(0.5,0.5);   // for vp box in IconView

//    QPointF compareMouseRelLoc;
    QSize preview;
    QSize full;

    int classificationBadgeDiam;

    bool isLoadingImage;        // suppress updates in showNormalizedViewport
    bool cursorIsHidden;        // use for slideshow and full screen - review rgh
    bool moveImageLocked;       // control when con drag image around
    bool isScrollable;
    bool isMouseDrag;
    bool isTrackpadScroll;
    bool isLeftMouseBtnPressed;
    bool isMouseDoubleClick;
    bool isLocalMouseClick = false;

    int scrollCount;

//    qreal previewScaleMax;
    qreal zoomInc = 0.1;    // 10% delta
    qreal zoomMin = 0.05;   // 5% of original  rgh add to pref
    qreal zoomMax = 8.0;    // 800% of original
//    qreal zoom100Pct;

    QPoint origin;
    QRubberBand *rubberBand;

    /* ------- Develop mask editing ------- */
    bool    maskEditMode = false;       // a spatial mask tool is the active edit target
    bool    maskHover    = false;       // cursor is over the view (overlay shown only then)
    int     maskTool     = 0;           // MaskTool enum value
    int     maskOp       = 0;           // MaskOp: 0 = Add, 1 = Subtract (tint colour)
    bool    maskInverted = false;       // flip the gradient ramp
    double  maskFeather  = 50.0;        // 0..100, ramp softness
    QPointF maskP1;                     // gradient start (0% line), normalized image coords
    QPointF maskP2;                     // gradient end   (100% line), normalized image coords
    int     maskDrag     = -1;          // active handle: -1 none, 0 p1, 1 p2, 2 move (whole line)
    QPointF maskMoveAnchorN;            // image-norm cursor at move start
    QPointF maskP1Anchor, maskP2Anchor; // endpoints at move start

    bool    maskHandlesEditable() const { return maskEditMode && maskHover && pmItem && pmItem->isVisible(); }
    QString maskParamsJson() const;                 // serialize maskP1/maskP2
    bool    parseMaskParams(const QString &json);   // -> maskP1/maskP2 (false if invalid)
    QPointF maskNormToViewport(QPointF n) const;    // normalized image -> viewport px
    QPointF maskViewportToNorm(QPoint vp) const;    // viewport px -> normalized image
    int     maskHitTest(QPoint vp) const;           // which handle is under vp (-1 none)
};

#endif // IMAGEVIEW_H

