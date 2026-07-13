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
    /* Zoom/pan captured when an interim preview is shown (loadImageInterim), so the
       developed image that replaces it (a different size) keeps the same view instead of
       re-fitting. Keyed to the image path so a navigation elsewhere can't consume it. */
    QString developCaptureForPath;
    QPointF developCaptureScrollPct;
    qreal   developCaptureZoom = 1.0;
    bool    developCaptureIsFit = true;
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
    /* Re-apply the zoom/pan captured for the current image at navigation
       (loadImageInterim), so a later same-image display (clean loadImage, developed
       setDevelopPreview) keeps the view instead of re-fitting. False if no match. */
    bool applyDevelopCapturedView();
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
    /* Paint the embedded JPG preview immediately as a placeholder while the slow
       scene-linear RAW decode runs (Develop mode). Cheap (embedded JPG, not the sensor
       decode); the real developed image replaces it when the decode lands. Returns false
       (shows nothing) if the preview can't be read. */
    bool loadImageInterim(QString fPath);
    /* Swap the displayed pixmap to an already-rendered QImage (a Develop preview) without
       re-decoding or refitting -- dimensions are unchanged, only the pixels differ. */
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
    /* Regenerative spot fill: arm/disarm the spot-removal brush. Stroke capture + pins
       land in the next increment; a finished stroke emits spotStrokeCommitted
       (FillSpotGeom paramsJson: size/feather/pts). */
    void beginSpotEdit();
    void endSpotEdit();
    /* The dock pushes the current image's spot centres (normalized) to draw the pins. */
    void setSpotPins(const QVector<QPointF> &pins);
    /* The whole-layer mask (all Add/Subtract tools composited) as a red coverage tint, shown under
       the active tool's handles while any mask tool is expanded. MW builds it (buildMaskBuffer);
       clear when no tool is expanded. */
    void setLayerMaskTint(const QImage &tint);
    void clearLayerMaskTint();
    /* "M": hide/show the mask overlay tint (both the whole-layer composite and the per-tool preview)
       while editing a mask -- handles/cursor stay so editing continues. No-op outside mask editing. */
    void toggleMaskTint();
    /* Force the mask overlay tint hidden (e.g. an adjustment slider was changed so the
       user can see the effect on the masked pixels). No-op outside mask editing or if
       already hidden. */
    void hideMaskTint();
    void setMaskFeather(double feather);
    void setMaskInverted(bool inverted);
    void setMaskBrushSettings(double size, double feather, double flow, bool autoMask,
                              const QString &autoMaskMode = QStringLiteral("lum"));
    /* Content-range tools (Luminance/Color Range): the dock changed lo/hi/refine (or samples) ->
       rebuild the coverage tint from the shared RangeRef. */
    void setMaskRangeParams(const QString &paramsJson);
    /* Build (once per image) + register the brush auto-mask luminance guide. Public so
       the develop render can guarantee it exists before rasterizing a lum auto-mask
       brush (GUI thread). */
    void ensureAutoGuide();

    /* Develop crop editing (Transform panel). beginCropEdit enters the Lightroom-style crop:
       the crop frame is anchored at a fixed centred "stage" in the viewport and the image is
       zoomed/panned BEHIND it, so dragging inside the frame pans the image while the frame stays
       put; the 8 edge/corner handles resize the frame. The crop rectangle is the source of truth
       in normalized image coords (0..1). endCropEdit restores the normal fit/zoom view. */
    void beginCropEdit(double aspect, bool locked, QRectF initialCrop = QRectF(0, 0, 1, 1));
    void endCropEdit();
    void beginLevel();                  // arm the "draw a level line" tool (crop must be active)
    void beginWarp();                   // enter 4-point perspective mode (seed the quad, drag corners)
    void setCropAspect(double aspect, bool locked);   // aspect = w/h, 0 = free (unconstrained)
    QRectF cropRect() const { return cropN; }         // current crop (normalized), for commit
    /* Warp (4-point perspective) accessors for the commit/persist flow (MW::rectifyDevelopCrop):
       cropIsWarp = a quad is being traced; cropQuad fills the 4 corners (normalized, TL,TR,BR,BL);
       computeRectifyCrop runs the warp engine on the displayed image and returns the largest
       inscribed rectangle (the suggested crop, normalized in the warped canvas), or an invalid rect
       on a degenerate quad. */
    bool   cropIsWarp() const { return cropWarp; }
    void   cropQuad(double q[8]) const;
    QRectF computeRectifyCrop() const;

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
    /* Starting a Brush stroke in "AI" auto-mask mode: ask MW to decode the SAM object under the
       stroke's seed point (output-normalized) so the live stroke can be confined to it. Direct-
       connected (synchronous), so the field is in the BrushStamp store when this returns. */
    void maskBrushSamFieldRequested(double onx, double ony);
    /* The crop rectangle changed (drag/resize/pan); normalized image coords, for persistence. */
    void cropChanged(double x, double y, double w, double h);
    /* A level line was drawn: the leveling angle to ADD to the straighten (degrees, nearest H/V). */
    void levelAngleChanged(double deltaDeg);
    /* Warp mode: user asked to commit the traced quad (Enter/Return or double-click). */
    void warpCommitRequested();
    /* A regenerative-fill spot stroke finished: FillSpotGeom paramsJson (size/feather/
       pts), for DevelopProperties to append as a FillSpot. */
    void spotStrokeCommitted(const QString &paramsJson);
    /* A spot pin was clicked (remove that spot), or Escape disarmed the tool. */
    void spotRemoveRequested(int index);
    void spotToolExited();

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

    /* ------- Regenerative spot fill ------- */
    bool    spotEditMode = false;       // the spot-removal brush is armed
    double  spotBrushSize = 8.0;        // diameter as % of the long edge ([ ] resize)
    double  spotFeather   = 40.0;       // 0..100, composite edge softness
    QVector<double> spotStrokePts;      // in-progress stroke, flat normalized x,y
    bool    spotPainting  = false;
    QPoint  spotCursorVp;               // cursor pos for the brush-size circle
    bool    spotCursorOn  = false;
    QVector<QPointF> spotPins;          // committed spot centres (normalized)
    int     spotHoverPin = -1;          // pin under the cursor (delete hint), -1 none
    int     spotPinHitTest(QPoint vp) const;            // pin under vp (-1 none)
    double  spotSizeMin() const;                        // min size % == 3px diameter
    /* True while a Develop tool (crop / mask / spot) owns the canvas: the default loupe
       click-to-zoom / pan / pick is suppressed so a tool click can't leak to it. */
    bool    developToolActive() const;
    void    drawSpotOverlay(QPainter *p, const QRectF &br);  // pins + stroke + cursor

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
    double  maskBrushSize = 20.0, maskBrushFlow = 100.0;
    bool    maskBrushAutoMask = false;
    QString maskBrushAutoMaskMode = QStringLiteral("lum");   // "lum" (luminance) | "ai" (SAM)
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
    std::shared_ptr<const BrushStamp::Guide> maskBrushSamField;  // AI stroke's SAM field (kept alive
                                                          // while maskStrokeAM.guide points into it)
    BrushStamp::AutoMaskCtx maskStrokeAM;      // current stroke's auto-mask context
    QPointF            maskBrushLast;          // last stamped point, buffer-pixel coords
    QPoint             maskBrushCursorVp;      // cursor pos for the brush-size circle
    bool               maskBrushCursorOn = false;
    /* Object Mask (SAM 2), perimeter-paint: the user traces the object BOUNDARY with a
       solid brush (many strokes; Alt erases). Reuses the maskBrush* buffers -- strokes
       accumulate into maskBrushMain (the perimeter wall), maskBrushStroke is the current
       stroke, maskBrushSize the diameter, maskBrushStrokesJson the committed list.
       maskObjPerim = wall + live stroke; ObjectMask::fillEnclosed fills the enclosed
       region into maskObjFill and sets maskObjClosed (amber open, green + fill closed).
       Each stroke release emits {"size","strokes"}; MW fills + SAM-refines it closed. */
    std::vector<float> maskObjPerim, maskObjFill;   // wall + filled silhouette
    bool               maskObjClosed  = false;      // an enclosed region was found
    bool               maskObjDrawing = false;
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
    /* drawTint=false draws only the tool's handles/guides/cursor/swatches (the whole-layer composite
       tint has already been painted underneath by the layer-mask overlay). */
    void    drawLinearMask(QPainter *p, const QRectF &br, bool drawTint = true);  // overlay for the Linear tool
    void    drawRadialMask(QPainter *p, const QRectF &br, bool drawTint = true);  // overlay for the Radial tool
    void    drawBrushMask(QPainter *p, const QRectF &br, bool drawTint = true);   // overlay for the Brush tool
    void    drawObjectMask(QPainter *p, const QRectF &br, bool drawTint = true);
    /* Object Mask perimeter-paint helpers (reuse brush buffers; amber/green closure). */
    void    objEnsureBuffers();     // size buffers + re-raster committed strokes
    void    objRecomputeFill();     // wall(main+stroke) -> fillEnclosed -> maskObjClosed
    void    objRebuildPreview();    // amber/green wall + fill tint image
    void    objUndoStroke();        // remove + re-raster the last perimeter stroke
    /* Brush painting helpers (preview buffers in output-oriented space). */
    void    brushBuildBuffers(const QString &paramsJson);   // parse strokes + (re)build buffers
    void    brushEnsureBuffers();                           // rebuild if the pixmap size changed
    void    brushRebuildPreview(QRect region = QRect());    // composite main+stroke -> tint (region or all)
    QRect   brushSegRect(QPointF a, QPointF b) const;       // buffer-px bbox of a dab/segment
    void    adjustBrushSize(double delta);                  // [ ] / two-finger; clamps + syncs dock
    void    brushUndoStroke();                              // remove + re-raster stroke
    void    toggleAutoMask();                               // "A": toggle auto-mask
    void    brushStampTo(QPointF bufPt);                    // stamp segment last..bufPt into stroke
    double  brushRadiusBufPx() const;                       // current brush radius in buffer px
    QPointF brushNormToBuf(QPointF n) const;                // normalized -> preview-buffer px
    bool    maskIsBrush() const { return maskTool == 2; }
    /* ------- Content-range mask tools (Color Range = 3, Luminance Range = 4) ------- */
    bool    maskIsRange() const { return maskTool == 3 || maskTool == 4; }
    /* AI masks (Select Subject = 5, Select Sky = 6): share the range tools' tint buffer + draw path,
       but the coverage comes from the SubjectMask/SkyMask map instead of the display-referred
       RangeRef. maskIsContent() = any tint-drawing content mask (range or AI). */
    bool    maskIsSubject()    const { return maskTool == 5; }
    bool    maskIsSky()        const { return maskTool == 6; }
    bool    maskIsBackground() const { return maskTool == 7; }   // = inverted Subject saliency
    bool    maskIsDepth()      const { return maskTool == 8; }   // depth-band over the MiDaS field
    bool    maskIsObject()     const { return maskTool == 9; }   // SAM 2 freehand-lasso object mask
    bool    maskIsContent() const {
        return maskIsRange() || maskIsSubject() || maskIsSky() || maskIsBackground() || maskIsDepth();
    }
    void    rebuildContentPreview();                // dispatch to the subject / sky / depth / range builder
    QString maskRangeParams;                       // lo/hi/refine/samples JSON for the active tool
    QImage  maskRangePreview;                       // coverage tint (output-oriented), like the brush
    QImage  maskLayerTint;                          // whole-layer composite coverage tint (output-oriented), all tools
    bool    maskTintHidden = false;                 // "M": suppress the mask overlay tint while editing
    void    drawRangeMask(QPainter *p, const QRectF &br, bool drawTint = true);   // paint the tint + colour swatches
    void    buildRangePreview();                    // rebuild the tint from the shared RangeRef + params
    void    buildSubjectPreview();                  // rebuild the tint from the shared SubjectRef
    void    buildSkyPreview();                      // rebuild the tint from the shared SkyRef
    void    buildDepthPreview();                    // rebuild the tint from the shared DepthRef (band)
    void    rangeSwatchRects(QVector<QRectF> &out) const;   // on-image colour swatches (viewport px)
    void    rangeSampleAt(QPoint vp, bool add);     // eyedropper: sample the loupe colour into samples
    QColor  maskTintColor() const;                  // Add = red, Subtract = blue (shared tint colour)
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
    bool    cropAltHeld    = false;      // Alt/Opt down: show the "transform" rubber band style
    QPointF cropQuadN[4];
    QPointF cropQuadVp[4];
    static constexpr int kCropDrawNew = 9;   // cropDrag value while rubber-banding a brand-new crop
    QPoint  cropDrawAnchorVp;            // drag-start corner while drawing a new crop
    QCursor cropCursor;                 // arrow + corner-bracket crop glyph (built in the ctor)
    QCursor levelCursor;               // arrow + spirit-level (vial + bubble) glyph, for the Level tool
    /* Level (straighten) tool: draw a line along something that should be horizontal/vertical; the
       line's tilt (reduced to the nearest H/V) is the leveling angle emitted on release. */
    bool    cropLevelMode = false;      // "draw a level line" is armed (one-shot)
    bool    cropLevelDragging = false;
    QPoint  cropLevelP1, cropLevelP2;

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
    void    cropDrawNewFrom(QPoint vp);             // rubber-band a new crop, anchor -> vp
    void    cropDrawOverlay(QPainter *p, const QRectF &br);
    bool    cropActive() const { return cropEditMode && pmItem && pmItem->isVisible(); }
    /* True when the crop is (still) the whole frame -- then a drag on empty area draws a NEW crop
       rectangle instead of panning, and the cursor is a crop crosshair. */
    bool    cropIsFull() const {
        return !cropWarp && cropN.x() <= 0.001 && cropN.y() <= 0.001 &&
               cropN.right() >= 0.999 && cropN.bottom() >= 0.999;
    }
};

#endif // IMAGEVIEW_H

