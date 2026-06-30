#ifndef IMAGEVIEW_H
#define IMAGEVIEW_H

#include <QtWidgets>
#include <QHash>
#include <QJsonArray>
#include "Develop/brushstamp.h"
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
    void setMaskBrushSettings(double size, double feather, double flow, bool autoMask);

    /* Develop crop editing (Transform panel). beginCropEdit enters the Lightroom-style crop:
       the crop frame is anchored at a fixed centred "stage" in the viewport and the image is
       zoomed/panned BEHIND it, so dragging inside the frame pans the image while the frame stays
       put; the 8 edge/corner handles resize the frame. The crop rectangle is the source of truth
       in normalized image coords (0..1). endCropEdit restores the normal fit/zoom view. */
    void beginCropEdit(double aspect, bool locked);
    void endCropEdit();
    void setCropAspect(double aspect, bool locked);   // aspect = w/h, 0 = free (unconstrained)
    /* Rectify the 4-point warp quad back to a rectangle. The perspective PIXEL warp is the deferred
       engine step; for now this resets the quad to its axis-aligned bounding box and leaves warp
       mode, emitting cropRectifyRequested so the future engine can hook in. No-op unless warping. */
    void rectifyCrop();

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
    /* Brush size changed on the canvas ([ ] keys or two-finger drag); sync the dock. */
    void maskBrushSizeRequested(double size);
    /* Auto-mask toggled on the canvas ("A"); sync the dock checkbox. */
    void maskBrushAutoMaskRequested(bool on);
    /* The crop rectangle changed (drag/resize/pan); normalized image coords, for persistence. */
    void cropChanged(double x, double y, double w, double h);
    /* The user asked to rectify the warp quad (Rectify button); the pixel-warp engine hooks here. */
    void cropRectifyRequested();

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
    int     maskTool     = 0;           // 0 Linear, 1 Radial (MaskTool enum value)
    int     maskOp       = 0;           // MaskOp: 0 = Add, 1 = Subtract (tint colour)
    bool    maskInverted = false;       // flip the ramp
    double  maskFeather  = 50.0;        // 0..100, ramp softness
    /* Linear: endpoints (0%/100%) in normalized image coords. */
    QPointF maskP1, maskP2;
    /* Radial: centre (normalized), semi-axes rx (of W) / ry (of H), rotation in degrees. */
    QPointF maskC = QPointF(0.5, 0.5);
    double  maskRx = 0.25, maskRy = 0.30, maskAngle = 0.0;
    /* Brush current settings (0..100; for the cursor + the next stroke). */
    double  maskBrushSize = 20.0, maskBrushFlow = 50.0;
    bool    maskBrushAutoMask = false;
    /* Brush painting state. Preview buffers are in output-oriented space, capped resolution. main =
       committed strokes; stroke = current in-progress stroke coverage; preview = cached tint image.
       strokePts is the flat [x0,y0,...] normalized point list being painted. */
    int                maskBrushW = 0, maskBrushH = 0;
    std::vector<float> maskBrushMain, maskBrushStroke, maskBrushScratch;
    QImage             maskBrushPreview;
    bool               maskPainting = false, maskBrushErase = false;
    QVector<double>    maskStrokePts;
    QJsonArray         maskBrushStrokesJson;   // committed strokes (to rebuild paramsJson on commit)
    std::shared_ptr<const BrushStamp::Guide> maskGuide;   // auto-mask luminance guide (this image)
    BrushStamp::AutoMaskCtx maskStrokeAM;      // current stroke's auto-mask context
    QPointF            maskBrushLast;          // last stamped point, buffer-pixel coords
    QPoint             maskBrushCursorVp;      // cursor pos for the brush-size circle
    bool               maskBrushCursorOn = false;
    int     maskDrag     = -1;          // active handle (per tool, see maskHitTest); -1 none
    QPointF maskMoveAnchorN;            // image-norm cursor at move start
    QPointF maskP1Anchor, maskP2Anchor; // linear endpoints at move start
    QPointF maskCAnchor;                // radial centre at move start
    double  maskAngleAnchor = 0;        // radial angle at rotate start
    double  maskGrabAngle   = 0;        // cursor angle (rad) at rotate start

    bool    maskHandlesEditable() const { return maskEditMode && maskHover && pmItem && pmItem->isVisible(); }
    QString maskParamsJson() const;                 // serialize the active tool's geometry
    bool    parseMaskParams(const QString &json);   // load geometry (false if invalid)
    QPointF maskNormToViewport(QPointF n) const;    // normalized image -> viewport px
    QPointF maskViewportToNorm(QPoint vp) const;    // viewport px -> normalized image
    QPointF maskViewportToImage(QPoint vp) const;   // viewport px -> image-pixel (pmItem) coords
    int     maskHitTest(QPoint vp) const;           // which handle is under vp (-1 none)
    void    drawLinearMask(QPainter *p, const QRectF &br);  // overlay for the Linear tool
    void    drawRadialMask(QPainter *p, const QRectF &br);  // overlay for the Radial tool
    void    drawBrushMask(QPainter *p, const QRectF &br);   // overlay for the Brush tool
    /* Brush painting helpers (preview buffers in output-oriented space). */
    void    brushBuildBuffers(const QString &paramsJson);   // parse strokes + (re)build buffers
    void    brushEnsureBuffers();                           // rebuild if the pixmap size changed
    void    brushRebuildPreview(QRect region = QRect());    // composite main+stroke -> tint (region or all)
    QRect   brushSegRect(QPointF a, QPointF b) const;       // buffer-px bbox of a dab/segment
    void    adjustBrushSize(double delta);                  // [ ] / two-finger; clamps + syncs dock
    void    brushUndoStroke();                              // remove + re-raster the last stroke
    void    ensureAutoGuide();                              // build the auto-mask guide if missing
    void    toggleAutoMask();                               // "A": flip auto-mask + sync the dock
    void    brushStampTo(QPointF bufPt);                    // stamp segment last..bufPt into stroke
    double  brushRadiusBufPx() const;                       // current brush radius in buffer px
    QPointF brushNormToBuf(QPointF n) const;                // normalized -> preview-buffer px
    bool    maskIsBrush() const { return maskTool == 2; }
    /* Radial: the four axis-end handles in image-pixel coords (0:+x 1:-x 2:+y 3:-y). */
    void    maskRadialAxisHandles(const QRectF &br, QPointF h[4]) const;
    /* Radial: the rotate handle (viewport px), a stub beyond the +x axis handle. */
    QPointF maskRadialRotateHandleVp(const QRectF &br) const;

    /* ------- Develop crop editing (Transform panel) -------
       The crop tool NEVER changes the view transform (no zoom, no auto-pan). cropN (normalized
       image coords) is the source of truth; cropFrameVp (the on-screen frame) is derived from it.
       Handles resize the frame over a static canvas (cropN := f(frame)). Repositioning the crop is
       done by PANNING the canvas under the fixed frame (frame stays put, cropN := f(frame) as the
       image moves). When the user zooms / the window resizes, the image transform changes and the
       frame tracks the same content (frame := f^-1(cropN)). */
    bool    cropEditMode   = false;
    QRectF  cropN          = QRectF(0.0, 0.0, 1.0, 1.0);   // crop in normalized image coords
    double  cropAspect     = 0.0;        // w/h; 0 = free
    bool    cropAspectLocked = false;
    int     cropDrag       = -1;         // -1 none/pan; 0..7 handles (see cropHitTest)
    QRectF  cropFrameVp;                 // the frame in viewport px (derived from cropN, or dragged)

    /* Warp (4-point perspective) sub-mode: Alt-dragging a crop corner breaks the rectangle into a
       free quadrilateral whose 4 corners drag independently. cropQuadN holds the corners in
       normalized image coords (source of truth), cropQuadVp the derived on-screen positions; both
       ordered TL,TR,BR,BL. The Rectify button warps the quad back to a rectangle (pixel warp is the
       deferred engine step). */
    bool    cropWarp       = false;
    QPointF cropQuadN[4];
    QPointF cropQuadVp[4];

    QRectF  cropImageOnScreenRect() const;          // image bounds in viewport px, clipped to view
    QRectF  cropVpRectToN(const QRectF &vp) const;  // a viewport rect -> normalized image rect
    QRectF  cropNToVpRect(const QRectF &n) const;   // normalized image rect -> viewport rect
    QPointF cropVpToN(QPointF vp) const;            // a viewport point -> normalized image point
    QPointF cropNToVp(QPointF n) const;             // a normalized image point -> viewport point
    void    cropEnterWarp();                        // seed the quad from the current rectangle
    QRectF  cropFrameBBoxVp() const;                // bbox of the frame/quad in viewport px
    void    cropSyncFrameFromN();                   // recompute frame/quad from cropN/cropQuadN
    void    cropEmitChanged();                      // clamp cropN to [0,1] and emit cropChanged
    int     cropHitTest(QPoint vp) const;           // handle under vp (-1 none, 8 = inside)
    void    cropResizeFromHandle(QPoint vp);        // move handle cropDrag to vp (aspect-aware)
    void    cropDrawOverlay(QPainter *p, const QRectF &br);
    bool    cropActive() const { return cropEditMode && pmItem && pmItem->isVisible(); }
};

#endif // IMAGEVIEW_H

