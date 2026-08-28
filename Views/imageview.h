#ifndef IMAGEVIEW_H
#define IMAGEVIEW_H

#include <QtWidgets>
#include <QHash>
#include "Main/global.h"                // G::maskOverlayGrayscale (mask overlay backdrop)
#include <QJsonArray>
#include <QJsonObject>
#include "Develop/brushstamp.h"
#include "Develop/editstack.h"          // Geometry (crop/straighten/warp)
#include "Datamodel/datamodel.h"
#include "Datamodel/selection.h"
#include "Cache/imagecache.h"
#include "Views/iconview.h"
#include "Views/infostring.h"
#include "Views/scaledpixmapitem.h"
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

    /* ScaledPixmapItem, not QGraphicsPixmapItem: the develop preview may hand it a
       proxy-resolution pixmap while it keeps presenting the FULL image size to the scene,
       so every overlay/hit-test/zoom calculation below still reads image geometry off
       pmItem->boundingRect(). See Views/scaledpixmapitem.h. */
    ScaledPixmapItem *pmItem;
    QTransform transform;
    Pixmap *pixmap;
    QString currentImagePath;

    int cwMargin = 20;
    qreal imAspect = 1;
    qreal zoom;
    qreal zoomFit;
    bool isFit;
    /* Zoom/pan captured when a Develop navigation starts (before the outgoing image is
       replaced), so the developed image that arrives after the demosaic keeps the same
       view instead of re-fitting. Keyed to the image path so a navigation elsewhere
       can't consume it. The capture tracks the live view while the decode is in flight
       (refreshDevelopCapture) so a thumbnail click pan or a zoom made on the interim
       preview is what gets restored, not the pre-navigation view. */
    QString developCaptureForPath;
    QPointF developCaptureScrollPct;
    qreal   developCaptureZoom = 1.0;
    bool    developCaptureIsFit = true;
    bool    developCaptureLocked = false;  // suspend tracking while (re)framing the view
    /* The interim preview is a small embedded JPG, so the zoom that framed the full size
       image must be scaled up to frame the same content. Tracking divides it back out so
       the capture always stays in full size terms. 1.0 whenever a full image is shown. */
    qreal   interimZoomScale = 1.0;
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
       (captureDevelopView), so a later same-image display (clean loadImage, developed
       setDevelopPreview) keeps the view instead of re-fitting. False if no match. */
    bool applyDevelopCapturedView();
    /* Image (== pmItem) coordinates -> coordinates in the pixmap actually held. Identity
       except during a Develop drag, where the item holds a screen-resolution proxy while
       presenting the full image size. Only pixel READS need this. */
    QPointF imageToPixmap(const QPointF &imagePt) const;
    /* While a Develop decode is in flight for the captured image, keep the capture in
       step with the live view (thumbnail click pan, zoom, scroll). */
    void refreshDevelopCapture();
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
    /* Placeholder while the slow scene-linear RAW decode runs. Uses the embedded JPG
       unless `substitute` supplies pixels -- a cached develop preview, which shows the
       DEVELOPED look instead of the untouched camera render. Framing is identical either
       way: both are reduced-resolution stand-ins for the same image. */
    bool loadImageInterim(QString fPath, const QImage &substitute = QImage());
    /* Record the zoom/pan of the outgoing image for a Develop navigation to fPath.
       Must be called before the displayed pixmap is replaced, while the live scroll
       state is still that of the image being left. */
    void captureDevelopView(const QString &fPath);
    /* Swap the displayed pixmap to an already-rendered QImage (a Develop preview)
       without re-decoding or refitting -- the DISPLAYED dimensions are unchanged, only
       the pixels differ.

       displaySize (optional) is the size `image` stands in for: the interactive proxy
       renders at screen resolution, and passing the full size here lets the item present
       that size to the scene while holding the small pixmap, instead of the caller
       upscaling a 50MP QImage per drag tick. Omit it when `image` IS the full render. */
    void setDevelopPreview(const QImage &image, QSize displaySize = QSize());
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

    /* Develop: hold Space to temporarily borrow the loupe zoom/pan gesture over any
       active mask / spot / crop tool (click toggles zoom, drag pans); release resumes the
       tool. MW's global event filter drives this on Space press/release, since ImageView
       usually lacks keyboard focus in Develop mode. A change arriving mid-gesture is
       deferred to the next mouse release (see spacePanDeferred). */
    void setSpacePanOverride(bool on);

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
    /* White-balance dropper: arm/disarm "click a neutral". A click emits wbSampled with
       the normalized image point and the tool disarms itself (the dock does the colour
       solve -- ImageView only reports where). */
    void beginWbPick();
    void endWbPick();
    /* Replace-panel mode (FillSpotGeom::Kind): Spot = click only (no drag), Fill/Object
       = drag a brush stroke. The committed paramsJson carries it as "kind". */
    void setSpotReplaceMode(int mode);
    /* The dock pushes the current image's spot centres (normalized) to draw the pins. */
    void setSpotPins(const QVector<QPointF> &pins);
    /* The whole-mask mask (all Add/Subtract tools composited) as a red coverage tint, shown under
       the active tool's handles while any mask tool is expanded. MW builds it (buildMaskBuffer);
       clear when no tool is expanded. */
    void setScopeMaskTint(const QImage &tint);
    void clearScopeMaskTint();

    /* The Detail panel's SHARPENING mask preview (MW::updateSharpenMaskPreview): the
       sharpen edge gate as a GRAYSCALE image -- white sharpened, black protected -- shown
       over the photo while Opt is held during a Masking drag, as Lightroom does it. It is
       opaque and covers the photo; that is the point, and it is why it lives only for the
       duration of the drag. Unlike the mask veil it is derived from the DISPLAYED
       developed frame, so it is already in displayed (post-geometry) space and is
       stretched straight over the pixmap -- it must NOT go through maskNormToItem. */
    void setSharpenMaskImage(const QImage &mask);
    void clearSharpenMaskImage();

    /* The develop GEOMETRY (crop / straighten / warp) the displayed image was rendered
       WITH, plus the oriented full-frame size it was applied to. Geometry is the LAST
       render stage, while every mask / spot coordinate is normalized in its INPUT space
       (the uncropped oriented frame) so edits stay glued to the photo when the crop
       changes. Pushing it here lets the overlays map between the two: without it a
       cropped image paints its masks in the wrong place. Push identity (default) whenever
       the render suppresses the geometry, e.g. while the crop tool is open. */
    void setDevelopGeometry(const Geometry &g, QSize orientedSrcSize);

    /* "Still rendering" chip, top-right of the loupe. MW raises it while a SLOW develop
       stage is in flight -- the scene-linear RAW decode on an image switch, and the
       full-resolution settle render -- so the interim frame on screen is labelled as
       such. Not used for the sub-second interactive proxy tick. */
    void setRenderingHint(const QString &msg);
    void clearRenderingHint();
    /* Op-indicator content for the on-canvas chip, pushed by MW when it rebuilds the
       tint: the submask being defined and the op the veil is previewing (-1 = none, so
       the chip is skipped). showHint adds the modifier reminder (2nd+ submask only).
       Drawn in drawForeground while the overlay is visible. */
    void setMaskLegend(const QString &submaskName, int op, bool showHint);
    /* True while a brush/object stroke is being painted. */
    bool maskStrokeInFlight() const { return maskPainting || maskObjDrawing; }
    /* True while the brush/object tool's OWN live preview stands in for the whole-mask
       veil: during a stroke, and on past the release until MW pushes a veil that includes
       that stroke (maskBrushVeilStale). Without the second half the coverage blinked back
       to the pre-stroke veil the moment the mouse came up -- the stroke just painted
       vanished until the render tick caught up, exactly when the user is lining up the
       next stroke. Not for a Subtract/Intersect stroke: there the local preview paints
       where coverage is being REMOVED, so the real veil is the honest picture. */
    bool maskBrushOwnsTint() const
    {
        if ((maskTool != 2 && !maskIsObject()) || maskLegendOp > 0) return false;
        if (maskStrokeInFlight()) return true;
        /* Past the release the stand-in only holds while it is actually being drawn:
           the per-tool draw is hover-gated, so off the image the (stale) veil is still
           better than no coverage at all. */
        return maskBrushVeilStale && maskHover && !maskBrushPreview.isNull();
    }
    /* Whether the whole-mask veil would actually be PAINTED. MW asks before building
       it: the veil is a full-resolution overlay rebuilt on every drag tick, and while
       it is hidden drawForeground ignores it, so building it is pure cost. */
    bool maskTintVisible() const { return !maskTintHidden; }
    /* Whether there is anything to tint at all: a mask tool is expanded, or a committed
       mask's composite is on display. False on the Global scope (no mask), where the
       tint toggle has nothing to act on -- the action-row button reads this to show
       itself as unavailable instead of silently doing nothing. */
    bool maskTintAvailable() const { return maskEditMode || !scopeMaskTint.isNull(); }
    /* True while the in-flight stroke is an ERASE (Opt held AND the submask already had
       coverage to erase). Only then does Opt belong to the stroke, so only then must MW
       stop reading it as the Subtract op modifier -- an Opt stroke on an empty submask
       is a normal paint that still means "this submask subtracts". */
    bool maskStrokeIsErase() const { return maskStrokeInFlight() && maskBrushErase; }
    /* "M": hide/show the mask overlay tint (both the whole-mask composite and the per-tool preview)
       while editing a mask -- handles/cursor stay so editing continues. No-op outside mask editing. */
    void toggleMaskTint();
    /* Force the mask overlay tint hidden (e.g. an adjustment slider was changed so the
       user can see the effect on the masked pixels). No-op outside mask editing or if
       already hidden. */
    void hideMaskTint();
    /* Force the mask overlay tint shown again (e.g. another scope was selected, so its
       combined mask should be visible). No-op if it is already showing. */
    void showMaskTint();
    void setMaskFeather(double feather);
    void setMaskInverted(bool inverted);
    void setMaskBrushSettings(double size, double feather, double flow, bool autoMask);
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
    void beginCropEdit(double aspect, bool locked, bool flipped,
                       QRectF initialCrop = QRectF(0, 0, 1, 1));
    void endCropEdit();
    void beginLevel();                  // arm the "draw a level line" tool (crop must be active)
    void beginWarp();                   // enter 4-point perspective mode (seed the quad, drag corners)
    void setCropAspect(double aspect, bool locked, bool flipped);  // aspect = w/h, 0 = free
    /* Toggle the crop between landscape and portrait: inverts the enforced aspect and
       rotates the current crop box 90 degrees (locked -> refit to the inverted ratio;
       free -> swap w/h). */
    void setCropAspectFlip(bool flipped);
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
    /* The mask overlay tint was shown/hidden (by "O", by an adjustment slider, or by
       the start of a new mask edit) so the dock's scope menu shows the matching check
       state. */
    void maskTintVisibilityChanged(bool shown);
    /* There is now (or is no longer) a mask overlay to show/hide -- a tool was expanded
       or collapsed, or a committed-mask composite appeared/vanished. Drives the enabled
       look of the action-row tint button. */
    void maskTintAvailabilityChanged(bool available);
    /* The user began SHAPING the pending submask -- a brush/object stroke, or a mask
       handle drag. The dock latches the combine op held at this instant
       (DevelopProperties::latchMaskOp) so it survives the modifier being released. */
    void maskOpActionStarted();
    /* A brush/object stroke started (true) or finished (false). While it runs Opt means
       "erase from this stroke", so MW re-reads the combine modifiers on each edge: the
       previewed op drops to Add for the stroke and returns to what is held afterwards. */
    void maskStrokeStateChanged(bool painting);
    /* Brush size changed on the canvas ([ ] keys or two-finger drag); sync the dock. */
    void maskBrushSizeRequested(double size);
    /* Feather changed on the canvas (Shift + wheel / two-finger drag over a gradient or
       brush mask); sync the dock slider + persist. */
    void maskFeatherRequested(double feather);
    /* Auto-mask toggled on the canvas ("A"); sync the dock checkbox. */
    void maskBrushAutoMaskRequested(bool on);
    /* The crop rectangle changed (drag/resize/pan); normalized image coords, for persistence. */
    void cropChanged(double x, double y, double w, double h);
    /* A level line was drawn: the leveling angle to ADD to the straighten (degrees, nearest H/V). */
    void levelAngleChanged(double deltaDeg);
    /* Warp mode: user asked to commit the traced quad (Enter/Return or double-click). */
    void warpCommitRequested();
    /* A regenerative-fill spot stroke finished: FillSpotGeom paramsJson (size/feather/
       pts), for DevelopProperties to append as a FillSpot. */
    void spotStrokeCommitted(const QString &paramsJson);
    /* The white-balance dropper was clicked at this normalized image point. skin =
       Opt/Alt was held: correct from a SKIN sample rather than a neutral one. */
    void wbSampled(double nx, double ny, bool skin);
    void wbPickExited();            // dismissed with Esc
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
    void wheelZoom(QWheelEvent *event);     // Develop: wheel / two-finger scroll = zoom
    void nativeGestureEvent(QNativeGestureEvent *event);
    bool event(QEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void keyReleaseEvent(QKeyEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void enterEvent(QEnterEvent *event) override;
    void leaveEvent(QEvent *event) override;

//    void dropEvent(QDropEvent *event) override;
    void dragEnterEvent(QDragEnterEvent *event) override;

    /* Painted AFTER the scene so the white-balance loupe sits on top of every overlay
       (drawForeground has too many early-return paths to guarantee that). */
    void paintEvent(QPaintEvent *event) override;
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
    bool    wbPickMode = false;         // the white-balance dropper is armed
    /* White-balance loupe: the "pick a target neutral" panel that follows the cursor
       while the dropper is armed -- a magnified 5x5 grid of the pixels under the tip
       (exactly the patch onWbSampled averages) plus their RGB readout. The pixels are
       read straight from the displayed pixmap per paint, so the panel can never show a
       stale preview and nothing large is held while armed. See drawSampleLoupe. */
    void    drawSampleLoupe(QPainter &p, QPoint vp, const QString &title,
                            const QString &tip, bool accent);
    QPoint  wbLoupeVp;                  // cursor position (viewport px)
    bool    wbLoupeOn = false;          // cursor is over the image -> draw the panel
    /* Opt/Alt held = the next click samples SKIN, not a neutral. Read from the mouse
       events themselves (they carry the authoritative modifier state and the loupe
       repaints on every move anyway), so the panel needs no keyboard focus -- which it
       does not reliably have in Develop. The CLICK always uses its own modifiers, so
       the action is right even if the title has not caught up. */
    bool    wbAltHeld = false;
    static constexpr int kWbLoupeCells = 5;   // 5x5 = the patch onWbSampled averages
    /* One-shot: suppress the click-to-toggle-zoom on the NEXT mouse release. Needed by
       tools that disarm on the press (the WB dropper), so developToolActive() is already
       false when the release arrives. Consumed at the top of mouseReleaseEvent. */
    bool    suppressClickZoom = false;
    int     spotReplaceMode = 0;        // FillSpotGeom::Kind: 0 spot, 1 fill, 2 object
    double  spotBrushSize = 8.0;        // diameter as % of the long edge ([ ] resize)
    double  spotFeather   = 40.0;       // 0..100, composite edge softness
    QVector<double> spotStrokePts;      // in-progress stroke, flat normalized x,y
    /* Fill mode paints an AREA before committing: strokes accumulate here (each with
       the brush size it was painted at; Opt/Alt strokes erase), shown as a tint in the
       overlay. Enter commits them as ONE FillSpot; Escape clears them. */
    struct SpotPendingStroke { double size; bool erase; QVector<double> pts; };
    QVector<SpotPendingStroke> spotPending;
    bool    spotStrokeErase = false;    // in-progress stroke is an erase (Opt/Alt held)
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
    /* Space held: the loupe zoom/pan gesture temporarily overrides the active tool (the
       tool's mouse branches are skipped so the base pan/zoom path runs). See
       setSpacePanOverride. spacePanDeferred holds a state change requested mid-gesture,
       applied on the next mouse release so an in-progress stroke isn't corrupted. */
    bool    spacePanOverride = false;
    bool    spacePanDeferred = false;
    bool    spacePanDeferredVal = false;
    void    applyDeferredSpacePan();    // apply a deferred Space change at a gesture edge
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
    /* Brush painting state. Preview buffers are in output-oriented space, capped resolution. main =
       committed strokes; stroke = current in-progress stroke coverage; preview = cached tint image.
       strokePts is the flat [x0,y0,...] normalized point list being painted. */
    int                maskBrushW = 0, maskBrushH = 0;
    std::vector<float> maskBrushMain, maskBrushStroke, maskBrushScratch;
    QImage             maskBrushPreview;
    bool               maskPainting = false, maskBrushErase = false;
    /* Set when a stroke is released, cleared when MW pushes (or drops) the rebuilt veil:
       the window in which scopeMaskTint is one stroke out of date. See maskBrushOwnsTint. */
    bool               maskBrushVeilStale = false;
    QVector<double>    maskStrokePts;
    QJsonArray         maskBrushStrokesJson;   // committed strokes (paramsJson on commit)
    /* Throttle shared by every LIVE (mid-gesture) mask-geometry emit -- brush strokes and
       Linear/Radial handle drags both drive a proxy develop render per emit, so both go
       through maskEmitLiveGeometry. Only one such gesture can be in flight at a time, so
       one clock + one pending flag covers both. */
    QElapsedTimer      maskStrokeLiveClock;
    bool               maskStrokeLivePending = false;   // a trailing emit is scheduled
    static constexpr int kMaskStrokeLiveMs = 100;
    /* Wheel = a wheel / two-finger resize of the Radial ellipse: there is no mouse
       button held, so it has no `live` flag of its own -- see maskEmitLiveGeometry. */
    enum class LiveEmit { Brush, Handle, Wheel };       // which gesture is emitting
    std::shared_ptr<const BrushStamp::Guide> maskGuide;   // auto-mask colour guide (this image)
    /* Guide pixels the brush DIAMETER should span; ensureAutoGuide sizes the guide from it
       and the current brush, so a small brush gets a finer guide (see there). */
    static constexpr double kGuidePxPerDab = 96.0;
    BrushStamp::AutoMaskCtx maskStrokeAM;      // current stroke's auto-mask context
    QPointF            maskBrushLast;          // last stamped point, buffer-pixel coords
    /* Distance travelled since the last SPACED dab, carried across mouse-moves so a
       slow drag costs the same as a fast one over the same path -- see
       BrushStamp::kDabSpacing. */
    double             maskBrushDabCarry = 0.0;
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
    /* Radial semi-axes at resize start, in FRAME PIXELS (not normalized): a Shift-drag
       scales both by one factor about the grabbed axis, so the on-screen proportions
       (and a circle's circularity) are preserved. */
    double  maskAxPxAnchor = 0, maskAyPxAnchor = 0;
    double  maskGrabAngle   = 0;        // cursor angle (rad) at rotate start

    bool    maskHandlesEditable() const { return maskEditMode && maskHover && pmItem && pmItem->isVisible(); }
    /* Show the image desaturated? Only while an overlay is actually being shown (a tool
       is expanded or a committed mask's veil is up, and "M"/"O" has not hidden it), so
       hiding the overlay brings the colour back and nothing outside Develop is affected.
       Read by paintEvent, which pushes it to pmItem. */
    bool    maskGrayscaleActive() const
            { return G::maskOverlayGrayscale && !maskTintHidden && pmItem
                     && pmItem->isVisible()
                     && (maskEditMode || !scopeMaskTint.isNull()); }
    /* Shift + wheel / two-finger drag over a gradient or brush mask; clamps + syncs
       the dock. */
    void    adjustMaskFeather(double delta);
    QString maskParamsJson() const;                 // serialize the active tool's geometry
    bool    parseMaskParams(const QString &json);   // load geometry (false if invalid)
    QPointF maskNormToViewport(QPointF n) const;    // normalized image -> viewport px
    QPointF maskViewportToNorm(QPoint vp) const;    // viewport px -> normalized image
    QPointF maskViewportToImage(QPoint vp) const;   // viewport px -> image px (pmItem)
    /* ------- Mask space vs displayed space -------
       Mask / spot coordinates are normalized in the develop geometry stage's INPUT (the
       uncropped oriented frame); the loupe shows its OUTPUT. maskNormToItem maps the
       first to pmItem (displayed pixel) coords and is the ONLY place that difference is
       resolved -- with an identity geometry it is just a scale by the pixmap size, which
       is what every overlay used to do inline. maskItemToNorm is its inverse.
       maskNormFrameSize is the mask space's own pixel size (aspect), for sizing the
       overlay buffers that are rasterized in it. */
    QTransform maskNormToItem() const;
    QTransform maskItemToNorm() const;
    QSizeF     maskNormFrameSize() const;
    /* Paint a raster built in mask space (the brush / object / content tints, and MW's
       whole-mask veil) onto the photo, through that map and clipped to it: the buffer
       spans the UNCROPPED frame, so a crop would otherwise spill it over the canvas.
       Called with the painter in scene coords (drawForeground). */
    void       maskDrawSpaceImage(QPainter *painter, const QImage &img) const;
    /* Length in mask-normalized units of a fraction of the mask frame's long edge (brush
       radii and the like are stored that way), and the same length in displayed px. */
    double     maskNormLongEdgeItemPx() const;
    int     maskHitTest(QPoint vp) const;           // which handle is under vp (-1 none)
    /* drawTint=false draws only the tool's handles/guides/cursor/swatches (the whole-mask composite
       tint has already been painted underneath by the mask overlay). */
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
    void    scaleBrushSize(double factor);                  // two-finger / wheel: relative resize
    void    setBrushSize(double size);                      // quantise + clamp + sync dock
    double  maskBrushSizeMin() const;                       // min size % == 3px diameter
    /* The brush size step (a % of the long edge). The canvas gestures, the dock slider
       (div 10) and the sidecar all quantise to it, so they can never disagree. */
    static constexpr double kBrushSizeStep = 0.1;
    void    brushUndoStroke();                              // remove + re-raster stroke
    QJsonObject brushStrokeJson() const;                    // the stroke under the cursor
    /* The Brush submask's paramsJson: committed strokes, plus the in-progress one when
       withLiveStroke (used mid-swipe so the masked adjustment re-renders live). */
    QString brushParamsJson(bool withLiveStroke) const;
    void    maskEmitLiveGeometry(LiveEmit kind);            // throttled mid-gesture emit
    void    brushEmitLiveGeometry();               // == maskEmitLiveGeometry(Brush)
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
    /* True only when the active mask tool actually consumes a canvas mouse gesture and
       shows its own cursor: Linear/Radial handles, Brush, Color Range eyedropper, Object
       perimeter-paint. The AI masks (Subject/Sky/Background/Depth) and the Luminance
       Range tool have no canvas interaction, so a click there must behave like the normal
       loupe (zoom / pan) rather than being swallowed by developToolActive(). */
    bool    maskToolUsesMouse() const {
        if (!maskEditMode) return false;
        switch (maskTool) {
        case 0: case 1:   // Linear / Radial drag handles
        case 2:           // Brush paint
        case 3:           // Color Range eyedropper
        case 9:           // Object perimeter-paint
            return true;
        default:          // 4 Luminance Range, 5-8 AI masks: no canvas interaction
            return false;
        }
    }
    void    rebuildContentPreview();                // dispatch to the subject / sky / depth / range builder
    QString maskRangeParams;                       // lo/hi/hue/samples JSON, active tool
    QImage  maskRangePreview;                       // coverage tint (output-oriented), like the brush
    QString renderingHint;                          // non-empty => chip shown
    QImage  scopeMaskTint;                          // whole-mask composite coverage tint (output-oriented), all tools
    QImage  sharpenMaskImage;                       // sharpening gate, grayscale (displayed space)
    bool    maskTintHidden = false;                 // "M": suppress the mask overlay tint
    /* The geometry the displayed render carries (see setDevelopGeometry), with the
       input->output pixel transform and output size cached: the warp case builds a
       homography, and the overlays ask for it on every paint. */
    Geometry   developGeom;
    QSize      developGeomSrc;              // oriented full frame (geometry stage input)
    QTransform developGeomFwd;              // input px -> output px
    QSizeF     developGeomOut;              // output px size
    /* On-canvas op indicator (set by setMaskLegend, drawn in drawMaskLegend). */
    QString maskLegendSubmask;                      // submask being defined ("" = none)
    int     maskLegendOp = -1;                      // -1 none, 0 Add, 1 Sub, 2 Intersect
    bool    maskLegendHint = false;                 // show the Opt/Shift+Opt reminder
    void    drawMaskLegend(QPainter *painter);
    void    drawRenderingHint(QPainter &painter);      // op-indicator chip (viewport)
    void    drawRangeMask(QPainter *p, const QRectF &br, bool drawTint = true);   // paint the tint + colour swatches
    void    buildRangePreview();                    // rebuild the tint from the shared RangeRef + params
    void    buildSubjectPreview();                  // rebuild the tint from the shared SubjectRef
    void    buildSkyPreview();                      // rebuild the tint from the shared SkyRef
    void    buildDepthPreview();                    // rebuild the tint from the shared DepthRef (band)
    void    rangeSwatchRects(QVector<QRectF> &out) const;   // on-image colour swatches
    void    rangeSampleAt(QPoint vp, bool add);     // eyedropper: sample into samples
    QPoint  rangeLoupeVp;                           // Color Range sampler cursor pos
    bool    rangeLoupeOn = false;                   // cursor over image -> draw the loupe
    QColor  maskTintColor() const;          // selected tool: Add = amber, Subtract = blue
    /* Radial: the four axis-end handles in image-pixel coords (0:+x 1:-x 2:+y 3:-y). */
    void    maskRadialAxisHandles(const QRectF &br, QPointF h[4]) const;
    /* Radial: the rotate handle (viewport px), a stub beyond the +x axis handle. */
    QPointF maskRadialRotateHandleVp(const QRectF &br) const;
    /* Radial: is the viewport point inside the CORE of the ellipse (the solid part
       within the half-coverage ring), as opposed to the feathered falloff outside it? */
    bool    maskRadialCoreContains(QPoint vp) const;
    /* Radial: wheel / two-finger resize -- scales BOTH semi-axes by `factor`, keeping
       the aspect and the centre (the Shift + handle-drag gesture, without the handle). */
    void    adjustRadialSize(double factor);

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
    bool    cropAspectFlipped = false;   // portrait: honour 1/aspect instead of aspect
    /* Flip memory: cropFlipPrevN is the crop as it was before the last flip; restoring it
       on a flip-back keeps the size stable across landscape<->portrait alternation
       (recomputing would shrink it via clamping). cropFlipResultN is what that flip
       produced, so an intervening manual crop edit is detected and the stash discarded.
       Both invalid = no stash. */
    QRectF  cropFlipPrevN;
    QRectF  cropFlipResultN;
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
    QCursor dropperCursor;             // pipette, hotspot at tip (white balance)
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
    /* The aspect the drag honours: 0 when unlocked, else cropAspect, except a locked
       "As shot" (cropAspect == 0) resolves to the image's native w/h so the frame
       keeps the original proportions. */
    qreal   cropLockedAspect() const;
    void    cropClampN();                           // clamp cropN's edges into [0,1]
    void    cropRefitToLockedAspect();              // reshape cropN to the locked aspect
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

