#include <QGraphicsDropShadowEffect>
#include <math.h>
#include <cmath>
#include "Main/global.h"
#include "Views/imageview.h"
#include "Develop/Transform/croptransform.h"

namespace {
/* The crop cursor: an arrow pointer with a corner-bracket crop glyph to its lower-right (drawn at
   2x for retina). Each stroke gets a white halo so it reads on any image. Hotspot = arrow tip. */
QCursor buildCropCursor()
{
    const qreal dpr = 2.0;
    const int S = 32;
    QPixmap pm(int(S * dpr), int(S * dpr));
    pm.setDevicePixelRatio(dpr);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing, true);

    /* Arrow pointer (white fill, black outline), tip at (1,1). */
    QPolygonF arrow;
    arrow << QPointF(1, 1) << QPointF(1, 17) << QPointF(5.5, 12.5) << QPointF(9, 20)
          << QPointF(11.5, 19) << QPointF(8, 11.5) << QPointF(14, 11.5);
    QPen ap(Qt::black); ap.setWidthF(1.2); ap.setJoinStyle(Qt::MiterJoin);
    p.setPen(ap); p.setBrush(Qt::white); p.drawPolygon(arrow);

    /* Two offset corner brackets forming a crop frame, lower-right of the arrow. */
    auto brackets = [&](const QColor &c, qreal w) {
        QPen bp(c); bp.setWidthF(w); bp.setCapStyle(Qt::FlatCap);
        p.setPen(bp); p.setBrush(Qt::NoBrush);
        p.drawLine(QPointF(15, 15), QPointF(25, 15));   // top-left bracket
        p.drawLine(QPointF(15, 15), QPointF(15, 25));
        p.drawLine(QPointF(21, 30), QPointF(31, 30));   // bottom-right bracket
        p.drawLine(QPointF(31, 20), QPointF(31, 30));
    };
    brackets(Qt::white, 3.5);   // halo
    brackets(Qt::black, 1.6);   // glyph
    p.end();
    return QCursor(pm, 1, 1);
}

/* The level cursor: the same arrow pointer with a spirit-level glyph (a vial with centre marks and
   a bubble) to its lower-right. Hotspot = arrow tip. */
QCursor buildLevelCursor()
{
    const qreal dpr = 2.0;
    const int S = 32;
    QPixmap pm(int(S * dpr), int(S * dpr));
    pm.setDevicePixelRatio(dpr);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing, true);

    QPolygonF arrow;
    arrow << QPointF(1, 1) << QPointF(1, 17) << QPointF(5.5, 12.5) << QPointF(9, 20)
          << QPointF(11.5, 19) << QPointF(8, 11.5) << QPointF(14, 11.5);
    QPen ap(Qt::black); ap.setWidthF(1.2); ap.setJoinStyle(Qt::MiterJoin);
    p.setPen(ap); p.setBrush(Qt::white); p.drawPolygon(arrow);

    /* Spirit level: a rounded vial with two centre marks and a bubble. Drawn halo then glyph. */
    auto level = [&](const QColor &c, qreal w, qreal bubbleR) {
        QPen bp(c); bp.setWidthF(w); bp.setCapStyle(Qt::RoundCap); bp.setJoinStyle(Qt::RoundJoin);
        p.setPen(bp); p.setBrush(Qt::NoBrush);
        p.drawRoundedRect(QRectF(15.5, 19.5, 15.0, 6.0), 3.0, 3.0);
        p.drawLine(QPointF(21.0, 19.5), QPointF(21.0, 25.5));
        p.drawLine(QPointF(25.0, 19.5), QPointF(25.0, 25.5));
        p.setBrush(c);
        p.drawEllipse(QPointF(23.0, 22.5), bubbleR, bubbleR);
    };
    level(Qt::white, 3.2, 2.7);   // halo
    level(Qt::black, 1.4, 1.8);   // glyph + bubble
    p.end();
    return QCursor(pm, 1, 1);
}
}
#include <QApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include "Develop/brushstamp.h"

#define CLIPBOARD_IMAGE_NAME		"clipboard.png"
#define ROUND(x) (static_cast<int>((x) + 0.5))
//#define ROUND(x) ((int) ((x) + 0.5))
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define MIN(a, b) ((a) < (b) ? (a) : (b))

/*  COORDINATE SPACES

The image is an item on the scene and the view is a viewport of the scene.

The view scaling is controlled by the zoomFactor.  When zoomFactor = 1
the view is at 100%, the same as the original image.

Moving is accomplished by adjusting the viewport scrollbars.

*/

ImageView::ImageView(QWidget *parent,
                     QWidget *centralWidget,
                     Metadata *metadata,
                     DataModel *dm,
                     ImageCacheData *icd,
                     Selection *sel,
                     IconView *thumbView,
                     InfoString *infoString,
                     bool isShootingInfoVisible,
                     bool isRatingBadgeVisible,
                     int classificationBadgeDiam,
                     int infoOverlayFontSize):

                     QGraphicsView(centralWidget)
{
    if (G::isLogger) G::log("ImageView::ImageView");

    this->mainWindow = parent;
    setObjectName("ImageView");
//    this->centralWidget = centralWidget;
    this->metadata = metadata;
    this->dm = dm;
    this->icd = icd;
    this->sel = sel;
    this->thumbView = thumbView;
    this->infoString = infoString;
    this->classificationBadgeDiam = classificationBadgeDiam;
    cropCursor = buildCropCursor();
    levelCursor = buildLevelCursor();
    pixmap = new Pixmap(this, dm, metadata);

    scene = new QGraphicsScene();
    scene->setObjectName("Scene");

    pmItem = new QGraphicsPixmapItem;
    pmItem->setBoundingRegionGranularity(1);
    pmItem->setOpacity(1.0);
    scene->addItem(pmItem);

    setAcceptDrops(true);
    pmItem->setAcceptDrops(true);
    setAttribute(Qt::WA_TranslucentBackground);
    setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
    setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setScene(scene);

    /* Hover moves (no button) are needed for the Develop scopes' live cursor readout. */
    setMouseTracking(true);
    viewport()->setMouseTracking(true);

    cursorIsHidden = false;
    moveImageLocked = false;
    infoOverlay = new DropShadowLabel(this);
    infoOverlay->setStyleSheet("font-size: " + QString::number(infoOverlayFontSize) + "pt;");
    infoOverlay->setVisible(false);
    // infoOverlay->setVisible(isShootingInfoVisible);
    infoOverlay->setAttribute(Qt::WA_TranslucentBackground, true);
    infoOverlay->setAttribute(Qt::WA_TransparentForMouseEvents, true);

    // title included in infoLabel, but might want to separate
    titleDropShadow = new DropShadowLabel(this);
    titleDropShadow->setVisible(isShootingInfoVisible);
    titleDropShadow->setAttribute(Qt::WA_TranslucentBackground);

    classificationLabel = new ClassificationLabel(this);
    classificationLabel->setAlignment(Qt::AlignCenter);
    classificationLabel->setVisible(isRatingBadgeVisible);

    QGraphicsOpacityEffect *infoEffect = new QGraphicsOpacityEffect;
    infoEffect->setOpacity(0.8);
    infoOverlay->setGraphicsEffect(infoEffect);

    rubberBand = new QRubberBand(QRubberBand::Rectangle, this);
    isRubberBand = false;

    // focus predictor
    QString appDir = QCoreApplication::applicationDirPath();
    QString focusPointModelPath = QDir(appDir).filePath("focus_point_model.onnx");
    int imageSize = 512;
    focusPredictor = new FocusPredictor(focusPointModelPath, imageSize);

    // not being used (hide / reveal cursor)
    mouseMovementTimer = new QTimer(this);
    // used to set isMouseDrag
    mouseMovementElapsedTimer = new QElapsedTimer;
    // connect(mouseMovementTimer, SIGNAL(timeout()), this, SLOT(monitorCursorState()));

    // mouse wheel is spinning
    wheelTimer.setSingleShot(true);
    connect(&wheelTimer, &QTimer::timeout, this, &ImageView::wheelStopped);

    // scroll event - pan scene when zoomed
    connect(horizontalScrollBar(), &QScrollBar::valueChanged,
            this, &ImageView::scrollChange);
    connect(verticalScrollBar(), &QScrollBar::valueChanged,
            this, &ImageView::scrollChange);


    // mouseZoomFit = true;
    isMouseDrag = false;
    isLeftMouseBtnPressed = false;
    isMouseDoubleClick = false;
    G::isFirstImageNewInstance = true;
    isBusy = false;
}

bool ImageView::loadImage(QString fPath, bool replace, QString src)
{
/*
    All image decoding and loading into memory happens in ImageCache in a separate thread
    to maximise performance.  This function scales the loaded image in a graphicsView.

    The image shown in ImageView is always the current image.

    Flow:

        • Event triggers MW::fileSelectionChange (click on thumbnail, next, etc)
            • This function is called
                • Nothing happens if image not cached, otherwise scaled and shown
            * Image path is sent to Image Cache
                • If not cached then add to cache
                * If it is the current image then signal this function

    Slideshow: The image cache is not used.  Each image in the slideshow is loaded here.
*/
    isLoadingImage = true;
    QString srcFun = "ImageView::loadImage";
    bool isDebug = false;
    bool isCurrent = (fPath == currentImagePath);

    if (isDebug)
    {
        qDebug() << srcFun
         << "sfRow =" << dm->proxyRowFromPath(fPath, srcFun)
         << "G::isFirstImageNewInstance =" << G::isFirstImageNewInstance
         << "isCurrent =" << isCurrent
         << "replace =" << replace
         << "G::isEmbellish =" << G::isEmbellish
         << " Src:" << src
         << fPath
            ; //*/
    }
    if (G::isLogger || G::isFlowLogger)
    {
        QString row = "row = " + QString::number(dm->proxyRowFromPath(fPath));
        G::log(srcFun, row + " Src:" + src +
               " G::isFirstImageNewInstance = " + QVariant(G::isFirstImageNewInstance).toString() +
               " " + fPath);
    }

    // ignore if result of remote operation
    if (G::isRemote) {
        isLoadingImage = false;
        if (isDebug) qDebug() << srcFun << "quit because G::isRemote = true";
        return false;
    }

    // No folder selected yet
    if (!fPath.length()) {
        isLoadingImage = false;
        if (isDebug) qDebug() << srcFun << "quit because !fPath.length()";
        return false;
    }

    // Already displayed
    if (isCurrent && replace == false) {
        isLoadingImage = false;
        if (isDebug) qDebug() << srcFun << "quit because isCurrent && replace == false";
        return true;
    }

    // do not load image if triggered by embellish remote export
    if (G::isProcessingExportedImages) {
        // qDebug() << "WARNING" << "ImageView::loadImage" << "Processing exported images";
        QString msg = "Processing exported images.  Canceled loadImage.";
        G::issue("Warning", msg, srcFun);
        isLoadingImage = false;
        if (isDebug) qDebug() << srcFun << "quit because G::isProcessingExportedImages";
        return false;
    }

    // set busy in case tryAgain attempt after already moved on to another image
    isBusy = true;
    bool isLoaded = false;

    // SLIDESHOW
    if (G::isSlideShow) {
        // load image without waiting for cache
        // check metadata loaded for image (might not be if random slideshow)
        int dmRow = dm->rowFromPath(fPath);
        if (dmRow == -1) {
            isLoadingImage = false;
            return false;
        }
        if (dm->index(dmRow, G::MetadataStatusColumn).data().toInt() != G::MetaLoaded) {
            QFileInfo fileInfo(fPath);
            if (metadata->loadImageMetadata(fileInfo, dmRow, dm->instance, true, true, false, true, "ImageView::loadImage")) {
                // metadata->m.row = dmRow;
                // metadata->m.instance = dm->instance;
                dm->addMetadataForItem(metadata->m, srcFun); // rgh investigate warning (QVariant issue probably)
            }
            else {
                G::issueDedup("Warning",
                              "Slideshow metadata load failed",
                              srcFun, dmRow, fPath);
            }
        }

        QPixmap displayPixmap;
        isLoaded = pixmap->load(fPath, displayPixmap, srcFun);

        if (isLoaded) {
            pmItem->setPixmap(displayPixmap);
            isBusy = false;
        }
        else {
            G::issueDedup("Error",
                          "Slideshow pixmap load failed",
                          srcFun, dmRow, fPath);
            // set null pixmap
            QPixmap nullPm;
            pmItem->setPixmap(nullPm);
            isLoadingImage = false;
            return false;
        }
    }

    /* Get cached image. Must check if image has been cached before calling
    icd->imCache.find(fPath, image) to prevent a mismatch between the fPath index and the
    image in icd->imCache hash table. Also must check in case where an ejected drive has
    resulted in clearing icd->cacheItemList. */

    int sfRow = dm->proxyRowFromPath(fPath, srcFun);
    if (sfRow == -1 || sfRow >= dm->sf->rowCount()) {
        isLoadingImage = false;
        return false;
    }

    if (icd->contains(fPath)) {
        QImage image; // confirm the cached image is in the image cache
        if (isDebug)
            qDebug() << srcFun + "  row =" << sfRow << fPath;

        pmItem->setPixmap(QPixmap::fromImage(icd->imCache.value(fPath)));
        isLoaded = true;
        if (isDebug)
            qDebug() << srcFun
                     << "w =" << pmItem->pixmap().width()
                     << "h =" << pmItem->pixmap().height()
                     << "isNull =" << pmItem->pixmap().isNull()
                     << fPath;
    }
    else {
        if (isDebug)
            qDebug() << srcFun << "isCached = false";
    }

    /* When the program is opening or resizing it is possible this function could be
    called before the central widget has been fully defined, and has a small default
    size. If that is the case, ignore, as the function will be called again. Also
    ignore if the image failed to be loaded into the graphics scene. */
    if (isLoaded) {
        /* important to keep currentImagePath. It is used to check if there isn't an
        image (when currentImagePath.isEmpty() == true) - for example when no folder
        has been chosen or the same image is being reloaded. It is also used by
        Embellish. */
        currentImagePath = fPath;
        pmItem->setVisible(true);
        // prevent the viewport scrolling outside the image
        setSceneRect(scene->itemsBoundingRect());
        // setSceneRect(pmItem->boundingRect());

        updateShootingInfo();

        /* If this is the first image in a new folder, and the image is smaller than the
        canvas (central widget window) set the scale to fit window, do not scale the
        image beyond 100% to fit the window.  */
        zoomFit = getFitScaleFactor(rect(), pmItem->boundingRect());
        /* If first item in DataModel is a video then zoom has not been set yet but
           G::isFirstImageNewInstance may be false */
        if (G::isFirstImageNewInstance || zoom < 0.1) {
            isFit = true;
            G::isFirstImageNewInstance = false;
        }
        if (isFit) {
            setFitZoom();
        }
        if (isDebug) {
        qDebug() << srcFun
                 << "G::isFirstImageNewInstance =" << G::isFirstImageNewInstance
                 << "row =" << sfRow
                 << "isFit =" << isFit
                 << "isFit =" << isFit
                 << "zoomFit =" << zoomFit
                 << "zoom =" << zoom
                 << "Src:" << src;
        }

        scale(true);    // isNewImage == true

        /* send signal to Embel::build (with new image), blank first parameter means
           local vs remote (ie exported from lightroom to embellish)  */
        if (G::isEmbellish) emit embellish("", "ImageView::loadImage");
        else pmItem->setGraphicsEffect(nullptr);

        // qDebug() << "ImageView::loadImage panToFocus =" << panToFocus;

        // focus prediction
        if (panToFocus) {
            predictPanToFocus();
        }

        // update viewpoint box in IconViewDelegate
        bool adjustCenter = panToFocus;
        bool refresh = false;
        // showNormalizedViewport(adjustCenter, refresh, srcFun);
    }

    isBusy = false;

    if (isLoaded) {
        isLoadingImage = false;
        return true;
    }
    else {
        if (isDebug)
            qDebug() << "ImageView::loadImage isLoaded = false";
        // set null pixmap
        QPixmap nullPm;
        pmItem->setPixmap(nullPm);
        /*
        qDebug() << "ImageView::loadImage failed"
                 << "isFirstImageNewInstance =" << isFirstImageNewInstance
                 << "row =" << sfRow; //*/
        isLoadingImage = false;
        return false;
    }
}

void ImageView::setDevelopPreview(const QImage &image)
{
/*
    Replace the displayed pixmap with an already-rendered Develop preview. The image has the
    same dimensions as the current one (only the pixels differ), so we swap the pixmap and
    leave zoom / fit / scene rect untouched -- no refit, no scale reset. Called on the GUI
    thread from MW::renderDevelopPreview while the user drags a Develop slider; the interactive
    proxy render is upscaled to these full dimensions by the caller so this contract holds.
*/
    if (G::isLogger) G::log("ImageView::setDevelopPreview");
    if (image.isNull()) return;
    /* Same dimensions (the usual slider-drag case): just swap pixels, leaving zoom/fit/scene. When
       the dimensions change -- the crop geometry was applied or removed -- treat it like a new image
       and re-fit so the cropped result is shown sensibly. */
    const bool sizeChanged = (image.size() != pmItem->pixmap().size());
    pmItem->setPixmap(QPixmap::fromImage(image));
    pmItem->setVisible(true);
    if (sizeChanged) {
        /* Dimensions only change when the crop geometry is applied/removed; treat like a new image
           and re-fit. (During a develop-slider drag the dimensions are constant, so this never
           fires mid-drag.) */
        imAspect = image.height() ? double(image.width()) / image.height() : 1.0;
        resetFitZoom();
    }
}

/* ============================ Develop mask editing ============================

   A spatial mask tool (Linear Gradient first) is edited directly on the loupe. The dock activates
   it (beginMaskEdit) when its caption is selected; the overlay is shown only while the cursor is
   over the view (maskHover). Geometry is normalized image coords (0..1) so it is independent of
   zoom and of proxy/full render resolution. The red tint previews the mask coverage; it is purely
   an overlay here and does not touch developComposite. Endpoint drags rotate+scale the gradient
   about its centre; the centre handle (or the line) translates it.
   ----------------------------------------------------------------------------------------------- */

void ImageView::beginMaskEdit(int tool, int op, bool inverted, const QString &paramsJson, double feather)
{
    if (G::isLogger) G::log("ImageView::beginMaskEdit");
    maskTool = tool;
    maskOp = op;
    maskInverted = inverted;
    maskFeather = feather;
    if (!parseMaskParams(paramsJson)) {     // missing/invalid -> a sensible default
        if (maskTool == 1) { maskC = QPointF(0.5, 0.5); maskRx = 0.25; maskRy = 0.30; maskAngle = 0; }
        else if (maskTool == 0) { maskP1 = QPointF(0.5, 0.34); maskP2 = QPointF(0.5, 0.66); }
    }
    maskPainting = false;
    maskGuide.reset();                                   // recompute the auto-mask guide for this image
    if (maskTool == 2) brushBuildBuffers(paramsJson);    // raster committed strokes into the preview
    maskEditMode = true;
    maskHover = underMouse();        // show at once if the cursor is already over the view
    maskBrushCursorOn = maskHover && maskTool == 2;
    maskDrag = -1;
    viewport()->update();
}

void ImageView::endMaskEdit()
{
    if (!maskEditMode) return;
    if (G::isLogger) G::log("ImageView::endMaskEdit");
    maskEditMode = false;
    maskDrag = -1;
    maskPainting = false;
    maskBrushCursorOn = false;
    maskBrushPreview = QImage();
    maskBrushMain.clear(); maskBrushStroke.clear(); maskBrushW = maskBrushH = 0;
    viewport()->update();
}

void ImageView::setMaskFeather(double feather)
{
    maskFeather = feather;
    if (maskEditMode && maskHover) viewport()->update();
}

void ImageView::setMaskInverted(bool inverted)
{
    maskInverted = inverted;
    if (maskTool == 2) brushRebuildPreview();    // invert is baked into the tint image
    if (maskEditMode && maskHover) viewport()->update();
}

void ImageView::setMaskBrushSettings(double size, double feather, double flow, bool autoMask)
{
    maskBrushSize = size;
    maskFeather = feather;
    maskBrushFlow = flow;
    maskBrushAutoMask = autoMask;
    if (maskEditMode && maskHover) viewport()->update();   // cursor preview (Stage 2)
}

/* ========================= Develop crop editing (Transform panel) =========================
   The crop tool NEVER changes the view transform (no zoom, no auto-pan): the user keeps full
   control of zoom/pan. cropN (normalized image coords) is the source of truth; cropFrameVp (the
   on-screen frame) is derived from it. Handles resize the frame over a STATIC canvas; the crop is
   repositioned by PANNING the canvas under the fixed frame (normal pan; cropN recomputed as the
   image slides). When the user zooms or the window resizes, the frame tracks the same content
   (cropSyncFrameFromN). beginCropEdit/endCropEdit only toggle the overlay -- they leave the image
   exactly where the user had it. */

void ImageView::beginCropEdit(double aspect, bool locked, QRectF initialCrop)
{
    if (G::isLogger) G::log("ImageView::beginCropEdit");
    if (!pmItem || !pmItem->isVisible()) return;

    cropAspect = aspect;
    cropAspectLocked = locked;
    /* Start from the stored crop (re-entering an already-cropped image), else the full image (or
       the aspect-constrained largest inscribed rect). */
    cropN = (initialCrop.isValid() && !initialCrop.isNull()) ? initialCrop
                                                             : QRectF(0.0, 0.0, 1.0, 1.0);
    if (cropN == QRectF(0.0, 0.0, 1.0, 1.0) && cropAspectLocked && cropAspect > 0.0) {
        const QRectF br = pmItem->boundingRect();
        const double imgA = br.width() / br.height();
        double w = 1.0, h = 1.0;
        if (cropAspect > imgA) h = imgA / cropAspect;    // wider than image -> limit height
        else                   w = cropAspect / imgA;    // taller -> limit width
        cropN = QRectF((1.0 - w) / 2.0, (1.0 - h) / 2.0, w, h);
    }

    cropEditMode = true;
    cropWarp = false;                 // always start as a plain rectangle crop
    cropAltHeld = false;
    cropLevelMode = cropLevelDragging = false;
    cropDrag = -1;
    /* Give the view room to pan even at fit zoom (so the image can be dragged under the fixed
       frame, with parts moving off the central widget) WITHOUT changing the zoom: only the
       scrollable area grows, not the transform. Restored in endCropEdit. */
    const QRectF imgScene = pmItem->mapToScene(pmItem->boundingRect()).boundingRect();
    setSceneRect(imgScene.adjusted(-imgScene.width(), -imgScene.height(),
                                    imgScene.width(),  imgScene.height()));
    cropSyncFrameFromN();             // derive the on-screen frame from cropN (no transform change)
    setCursor(Qt::ArrowCursor);
    viewport()->update();
    cropEmitChanged();
}

void ImageView::endCropEdit()
{
    if (!cropEditMode) return;
    if (G::isLogger) G::log("ImageView::endCropEdit");
    cropEditMode = false;
    cropWarp = false;
    cropAltHeld = false;
    cropLevelMode = cropLevelDragging = false;
    cropDrag = -1;
    /* Restore the normal scrollable area and re-centre the (possibly panned) image. Zoom is left
       as the user set it. */
    setSceneRect(scene->itemsBoundingRect());
    centerOn(pmItem);
    setCursor(isScrollable ? Qt::OpenHandCursor : Qt::ArrowCursor);
    viewport()->update();            // clear the overlay; the view transform is untouched
}

void ImageView::beginLevel()
{
    if (!cropActive()) return;
    if (G::isLogger) G::log("ImageView::beginLevel");
    cropLevelMode = true;
    cropLevelDragging = false;
    setCursor(levelCursor);
    viewport()->update();
}

void ImageView::beginWarp()
{
    if (!cropActive()) return;
    if (G::isLogger) G::log("ImageView::beginWarp");
    cropLevelMode = cropLevelDragging = false;
    if (!cropWarp) cropEnterWarp();     // seed the free quad from the current rectangle
    cropAltHeld = true;                 // show the "transform" rubber-band style
    cropSyncFrameFromN();
    setCursor(Qt::ArrowCursor);
    viewport()->update();
}

void ImageView::setCropAspect(double aspect, bool locked)
{
    cropAspect = aspect;
    cropAspectLocked = locked;
    if (!cropEditMode) return;
    cropWarp = false;                 // choosing an aspect implies a rectangle: leave warp mode
    /* Re-fit the current crop centre to the new aspect (shrink to stay inside the image). */
    if (cropAspectLocked && cropAspect > 0.0) {
        const QRectF br = pmItem->boundingRect();
        const QPointF c = cropN.center();
        /* Keep the current crop width, derive height from the aspect (in image-px, then back to
           normalized), then clamp into the image. */
        double w = cropN.width();
        double h = (w * br.width() / cropAspect) / br.height();
        if (h > 1.0) { h = 1.0; w = (h * br.height() * cropAspect) / br.width(); }
        cropN = QRectF(c.x() - w / 2.0, c.y() - h / 2.0, w, h);
        if (cropN.left()   < 0.0) cropN.moveLeft(0.0);
        if (cropN.top()    < 0.0) cropN.moveTop(0.0);
        if (cropN.right()  > 1.0) cropN.moveRight(1.0);
        if (cropN.bottom() > 1.0) cropN.moveBottom(1.0);
    }
    cropSyncFrameFromN();
    viewport()->update();
    cropEmitChanged();
}

QRectF ImageView::cropImageOnScreenRect() const
{
    /* The image's bounding rect in viewport px, clipped to the visible viewport. */
    const QRectF imgVp = mapFromScene(pmItem->mapToScene(pmItem->boundingRect())).boundingRect();
    return imgVp.intersected(QRectF(viewport()->rect()));
}

QRectF ImageView::cropNToVpRect(const QRectF &n) const
{
    /* normalized image rect -> viewport px (tracks the current user zoom/pan). */
    const QRectF br = pmItem->boundingRect();
    const QPointF tl = mapFromScene(pmItem->mapToScene(
                           QPointF(n.left()  * br.width(), n.top()    * br.height())));
    const QPointF brp = mapFromScene(pmItem->mapToScene(
                           QPointF(n.right() * br.width(), n.bottom() * br.height())));
    return QRectF(tl, brp).normalized();
}

QPointF ImageView::cropVpToN(QPointF vp) const
{
    const QRectF br = pmItem->boundingRect();
    if (br.width() <= 0 || br.height() <= 0) return QPointF();
    const QPointF ip = pmItem->mapFromScene(mapToScene(vp.toPoint()));
    return QPointF(ip.x() / br.width(), ip.y() / br.height());
}

QPointF ImageView::cropNToVp(QPointF n) const
{
    const QRectF br = pmItem->boundingRect();
    return mapFromScene(pmItem->mapToScene(QPointF(n.x() * br.width(), n.y() * br.height())));
}

void ImageView::cropEnterWarp()
{
    /* Seed the free quad from the current rectangle (corners TL,TR,BR,BL). */
    cropQuadN[0] = cropN.topLeft();   cropQuadN[1] = cropN.topRight();
    cropQuadN[2] = cropN.bottomRight(); cropQuadN[3] = cropN.bottomLeft();
    for (int i = 0; i < 4; ++i) cropQuadVp[i] = cropNToVp(cropQuadN[i]);
    cropWarp = true;
}

QRectF ImageView::cropFrameBBoxVp() const
{
    if (!cropWarp) return cropFrameVp;
    return QPolygonF(QVector<QPointF>(cropQuadVp, cropQuadVp + 4)).boundingRect();
}

void ImageView::cropSyncFrameFromN()
{
    if (!cropActive()) return;
    if (cropWarp) { for (int i = 0; i < 4; ++i) cropQuadVp[i] = cropNToVp(cropQuadN[i]); }
    else            cropFrameVp = cropNToVpRect(cropN);
}

void ImageView::cropEmitChanged()
{
    if (cropWarp) {
        /* No rectangle while warping; emit the quad's bounding box so the persistence hook still
           gets a sane value (the real quad geometry lands with the warp engine). */
        const QRectF b = QPolygonF(QVector<QPointF>(cropQuadN, cropQuadN + 4))
                             .boundingRect().intersected(QRectF(0, 0, 1, 1));
        emit cropChanged(b.x(), b.y(), b.width(), b.height());
        return;
    }
    cropN = cropN.intersected(QRectF(0, 0, 1, 1));
    emit cropChanged(cropN.x(), cropN.y(), cropN.width(), cropN.height());
}

void ImageView::cropQuad(double q[8]) const
{
    for (int i = 0; i < 4; ++i) { q[i * 2] = cropQuadN[i].x(); q[i * 2 + 1] = cropQuadN[i].y(); }
}

QRectF ImageView::computeRectifyCrop() const
{
    if (!cropActive() || !cropWarp || !pmItem) return QRectF();
    const QImage cur = pmItem->pixmap().toImage();
    if (cur.isNull()) return QRectF();
    QPointF quadPx[4];
    for (int i = 0; i < 4; ++i)
        quadPx[i] = QPointF(cropQuadN[i].x() * cur.width(), cropQuadN[i].y() * cur.height());
    QRectF cropNorm;
    const QImage warped = CropTransform::rectifyPerspective(cur, quadPx, cropNorm);
    if (warped.isNull()) return QRectF();       // degenerate quad
    return cropNorm;
}

QRectF ImageView::cropVpRectToN(const QRectF &vp) const
{
    const QRectF br = pmItem->boundingRect();
    if (br.width() <= 0 || br.height() <= 0) return QRectF();
    const QPointF a = pmItem->mapFromScene(mapToScene(vp.topLeft().toPoint()));
    const QPointF b = pmItem->mapFromScene(mapToScene(vp.bottomRight().toPoint()));
    return QRectF(a.x() / br.width(), a.y() / br.height(),
                  (b.x() - a.x()) / br.width(), (b.y() - a.y()) / br.height());
}

int ImageView::cropHitTest(QPoint vp) const
{
    const qreal g = 14.0;       // grab radius, px
    if (cropWarp) {
        /* Warp: only the 4 free corners are handles (0..3); 8 = inside the quad (pan). */
        for (int i = 0; i < 4; ++i)
            if (QLineF(vp, cropQuadVp[i]).length() <= g) return i;
        if (QPolygonF(QVector<QPointF>(cropQuadVp, cropQuadVp + 4)).containsPoint(vp, Qt::OddEvenFill))
            return 8;
        return -1;
    }
    /* Rectangle handles 0..7: 0 TL,1 TR,2 BR,3 BL corners; 4 T,5 R,6 B,7 L edge mids. 8 = inside. */
    const QRectF f = cropFrameVp;
    if (!f.isValid()) return -1;
    const QPointF pts[8] = {
        f.topLeft(), f.topRight(), f.bottomRight(), f.bottomLeft(),
        QPointF(f.center().x(), f.top()), QPointF(f.right(), f.center().y()),
        QPointF(f.center().x(), f.bottom()), QPointF(f.left(), f.center().y())
    };
    for (int i = 0; i < 8; ++i)
        if (QLineF(vp, pts[i]).length() <= g) return i;
    if (f.contains(vp)) return 8;
    return -1;
}

void ImageView::cropResizeFromHandle(QPoint vp)
{
    /* Move the active handle to vp over a STATIC image (no transform change); rebuild cropN from
       the frame rect. The part of the frame opposite the dragged handle is the anchor (stays put).
       With the aspect locked the free dimension follows so the frame keeps its ratio. The handle is
       clamped to the image's on-screen rect so the crop can never exceed the image. */
    const QRectF vr = cropImageOnScreenRect();
    const qreal x = qBound(vr.left(), qreal(vp.x()), vr.right());
    const qreal y = qBound(vr.top(),  qreal(vp.y()), vr.bottom());

    /* Warp: drag a single quad corner freely (no aspect, no opposite-anchor coupling). */
    if (cropWarp) {
        if (cropDrag < 0 || cropDrag > 3) return;
        cropQuadVp[cropDrag] = QPointF(x, y);
        cropQuadN[cropDrag]  = cropVpToN(cropQuadVp[cropDrag]);
        cropEmitChanged();
        return;
    }

    const QRectF f0 = cropFrameVp;
    const qreal a = (cropAspectLocked && cropAspect > 0.0) ? cropAspect : 0.0;
    const qreal minSz = 16.0;
    QRectF f = f0;

    if (cropDrag <= 3) {                             // ---- corner ----
        /* Anchor = opposite corner. */
        QPointF anchor;
        switch (cropDrag) {
        case 0: anchor = f0.bottomRight(); break;    // TL drags
        case 1: anchor = f0.bottomLeft();  break;    // TR
        case 2: anchor = f0.topLeft();     break;    // BR
        default: anchor = f0.topRight();   break;    // BL
        }
        qreal w = qMax(minSz, std::abs(x - anchor.x()));
        qreal h = (a > 0.0) ? w / a : qMax(minSz, std::abs(y - anchor.y()));
        if (a > 0.0) w = h * a;
        const qreal left = (x < anchor.x()) ? anchor.x() - w : anchor.x();
        const qreal top  = (y < anchor.y()) ? anchor.y() - h : anchor.y();
        f = QRectF(left, top, w, h);
    }
    else {                                           // ---- edge ----
        if (cropDrag == 5 || cropDrag == 7) {        // R / L edge: width changes
            const qreal anchorX = (cropDrag == 5) ? f0.left() : f0.right();
            qreal w = qMax(minSz, std::abs(x - anchorX));
            const qreal left = (x < anchorX) ? anchorX - w : anchorX;
            qreal h = (a > 0.0) ? w / a : f0.height();
            const qreal top = (a > 0.0) ? f0.center().y() - h / 2.0 : f0.top();
            f = QRectF(left, top, w, h);
        } else {                                     // T / B edge: height changes
            const qreal anchorY = (cropDrag == 6) ? f0.top() : f0.bottom();
            qreal h = qMax(minSz, std::abs(y - anchorY));
            const qreal top = (y < anchorY) ? anchorY - h : anchorY;
            qreal w = (a > 0.0) ? h * a : f0.width();
            const qreal left = (a > 0.0) ? f0.center().x() - w / 2.0 : f0.left();
            f = QRectF(left, top, w, h);
        }
    }

    cropFrameVp = f.normalized().intersected(vr);
    cropN = cropVpRectToN(cropFrameVp);
    cropEmitChanged();
}

void ImageView::cropDrawNewFrom(QPoint vp)
{
    /* Rubber-band a brand-new crop from the press anchor to vp, clamped to the image's on-screen
       rect. With the aspect locked, the height follows the dragged width. */
    const QRectF vr = cropImageOnScreenRect();
    const qreal ax = qBound(vr.left(), qreal(cropDrawAnchorVp.x()), vr.right());
    const qreal ay = qBound(vr.top(),  qreal(cropDrawAnchorVp.y()), vr.bottom());
    const qreal bx = qBound(vr.left(), qreal(vp.x()), vr.right());
    const qreal by = qBound(vr.top(),  qreal(vp.y()), vr.bottom());
    qreal w = std::abs(bx - ax);
    qreal h = std::abs(by - ay);
    const qreal a = (cropAspectLocked && cropAspect > 0.0) ? cropAspect : 0.0;
    if (a > 0.0) h = w / a;
    const qreal left = (bx < ax) ? ax - w : ax;
    const qreal top  = (by < ay) ? ay - h : ay;

    cropFrameVp = QRectF(left, top, w, h).intersected(vr);
    cropN = cropVpRectToN(cropFrameVp);
    cropEmitChanged();
}

void ImageView::cropDrawOverlay(QPainter *painter, const QRectF &br)
{
    Q_UNUSED(br);
    painter->save();
    painter->resetTransform();                 // draw in viewport coords (constant on-screen size)
    painter->setRenderHint(QPainter::Antialiasing, true);

    /* "Transform" rubber band: while warping, OR while Alt/Opt is held over a rectangle crop (a
       corner drag will then pull a free perspective quad), use a RED border with CIRCULAR corner
       handles and NO intermediate side handles. The plain rectangle crop is white with square
       corner + side handles. */
    const bool transformStyle = cropWarp || cropAltHeld;
    const qreal hs = 5.0;
    const QColor gridCol(255, 255, 255, 90);
    const QColor borderCol = transformStyle ? QColor(230, 40, 40, 235) : QColor(255, 255, 255, 220);
    const QColor handleFill = transformStyle ? QColor(230, 40, 40, 240) : QColor(255, 255, 255, 235);

    auto drawHandle = [&](QPointF c) {
        if (transformStyle) painter->drawEllipse(c, hs + 1.0, hs + 1.0);   // circular
        else                painter->drawRect(QRectF(c.x() - hs, c.y() - hs, hs * 2, hs * 2));
    };

    if (cropWarp) {
        /* ---- Warp quad (TL,TR,BR,BL) ---- */
        const QPointF *q = cropQuadVp;
        QPolygonF poly(QVector<QPointF>(q, q + 4));

        QPainterPath outside; outside.addRect(QRectF(viewport()->rect()));
        QPainterPath inside;  inside.addPolygon(poly); inside.closeSubpath();
        painter->fillPath(outside.subtracted(inside), QColor(0, 0, 0, 140));

        /* A bilinear 3x3 guide grid (interpolated along the edges). */
        auto lerp = [](QPointF a, QPointF b, qreal t){ return a + (b - a) * t; };
        QPen gp(gridCol); gp.setWidthF(1.0); gp.setCosmetic(true); painter->setPen(gp);
        for (int k = 1; k <= 2; ++k) {
            const qreal t = k / 3.0;
            painter->drawLine(lerp(q[0], q[1], t), lerp(q[3], q[2], t));
            painter->drawLine(lerp(q[0], q[3], t), lerp(q[1], q[2], t));
        }
        QPen bp(borderCol); bp.setWidthF(1.5); bp.setCosmetic(true);
        painter->setPen(bp); painter->setBrush(Qt::NoBrush);
        painter->drawPolygon(poly);

        painter->setBrush(handleFill); painter->setPen(Qt::NoPen);
        for (int i = 0; i < 4; ++i) drawHandle(q[i]);
        painter->restore();
        return;
    }

    /* ---- Rectangle crop ---- */
    const QRectF f = cropFrameVp;
    if (!f.isValid()) { painter->restore(); return; }

    QPainterPath outside;
    outside.addRect(QRectF(viewport()->rect()));
    QPainterPath inside; inside.addRect(f);
    outside = outside.subtracted(inside);
    painter->fillPath(outside, QColor(0, 0, 0, 140));

    /* Rule-of-thirds grid + frame border. */
    QPen grid(gridCol); grid.setWidthF(1.0); grid.setCosmetic(true);
    painter->setPen(grid);
    for (int k = 1; k <= 2; ++k) {
        const qreal x = f.left() + f.width()  * k / 3.0;
        const qreal y = f.top()  + f.height() * k / 3.0;
        painter->drawLine(QPointF(x, f.top()), QPointF(x, f.bottom()));
        painter->drawLine(QPointF(f.left(), y), QPointF(f.right(), y));
    }
    QPen border(borderCol); border.setWidthF(1.5); border.setCosmetic(true);
    painter->setPen(border);
    painter->setBrush(Qt::NoBrush);
    painter->drawRect(f);

    /* Handles: corners always; intermediate side handles only in the plain (non-transform) style. */
    painter->setBrush(handleFill);
    painter->setPen(Qt::NoPen);
    const QPointF corners[4] = { f.topLeft(), f.topRight(), f.bottomRight(), f.bottomLeft() };
    for (const QPointF &c : corners) drawHandle(c);
    if (!transformStyle) {
        const QPointF mids[4] = {
            QPointF(f.center().x(), f.top()), QPointF(f.right(), f.center().y()),
            QPointF(f.center().x(), f.bottom()), QPointF(f.left(), f.center().y())
        };
        for (const QPointF &m : mids)
            painter->drawRect(QRectF(m.x() - hs, m.y() - hs, hs * 2, hs * 2));
    }

    painter->restore();
}

bool ImageView::parseMaskParams(const QString &json)
{
    QJsonParseError err;
    const QJsonDocument doc = QJsonDocument::fromJson(json.toUtf8(), &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject()) return false;
    const QJsonObject o = doc.object();
    if (maskTool == 2) {            // Brush: current settings (strokes handled in Stage 2)
        maskBrushSize = o.value("size").toDouble(20);
        maskBrushFlow = o.value("flow").toDouble(50);
        maskBrushAutoMask = o.value("autoMask").toBool(false);
        return true;
    }
    if (maskTool == 1) {            // Radial
        if (!o.contains("cx") || !o.contains("cy") || !o.contains("rx") || !o.contains("ry"))
            return false;
        maskC = QPointF(o["cx"].toDouble(), o["cy"].toDouble());
        maskRx = o["rx"].toDouble();
        maskRy = o["ry"].toDouble();
        maskAngle = o["angle"].toDouble();
        return true;
    }
    if (!o.contains("x1") || !o.contains("y1") || !o.contains("x2") || !o.contains("y2"))
        return false;               // Linear
    maskP1 = QPointF(o["x1"].toDouble(), o["y1"].toDouble());
    maskP2 = QPointF(o["x2"].toDouble(), o["y2"].toDouble());
    return true;
}

QString ImageView::maskParamsJson() const
{
    QJsonObject o;
    if (maskTool == 1) {            // Radial
        o["cx"] = maskC.x(); o["cy"] = maskC.y();
        o["rx"] = maskRx;    o["ry"] = maskRy;
        o["angle"] = maskAngle;
    }
    else {                          // Linear
        o["x1"] = maskP1.x(); o["y1"] = maskP1.y();
        o["x2"] = maskP2.x(); o["y2"] = maskP2.y();
    }
    return QString::fromUtf8(QJsonDocument(o).toJson(QJsonDocument::Compact));
}

QPointF ImageView::maskNormToViewport(QPointF n) const
{
    const QRectF br = pmItem->boundingRect();
    const QPointF itemPt(n.x() * br.width(), n.y() * br.height());
    return mapFromScene(pmItem->mapToScene(itemPt));
}

QPointF ImageView::maskViewportToNorm(QPoint vp) const
{
    const QRectF br = pmItem->boundingRect();
    if (br.width() <= 0 || br.height() <= 0) return QPointF();
    const QPointF itemPt = pmItem->mapFromScene(mapToScene(vp));
    return QPointF(itemPt.x() / br.width(), itemPt.y() / br.height());
}

QPointF ImageView::maskViewportToImage(QPoint vp) const
{
    return pmItem->mapFromScene(mapToScene(vp));     // image-pixel (pmItem) coords
}

void ImageView::maskRadialAxisHandles(const QRectF &br, QPointF h[4]) const
{
    /* Axis-end handles in image-pixel coords: centre +/- semi-axis along the rotated x and y axes
       (rx is a fraction of width, ry of height). */
    const double a = maskAngle * 0.017453292519943295;
    const double ca = std::cos(a), sa = std::sin(a);
    const QPointF c(maskC.x()*br.width(), maskC.y()*br.height());
    const double ax = maskRx * br.width(), ay = maskRy * br.height();
    const QPointF ux(ca*ax,  sa*ax);     // rotated +x axis * rx
    const QPointF uy(-sa*ay, ca*ay);     // rotated +y axis * ry
    h[0] = c + ux; h[1] = c - ux;        // +x, -x  (resize rx)
    h[2] = c + uy; h[3] = c - uy;        // +y, -y  (resize ry)
}

QPointF ImageView::maskRadialRotateHandleVp(const QRectF &br) const
{
    /* A constant on-screen stub beyond the +x axis handle (viewport coords) so it stays grabbable
       at any zoom and any ellipse size. */
    QPointF h[4];
    maskRadialAxisHandles(br, h);
    const QPointF cv = maskNormToViewport(maskC);
    const QPointF hx = mapFromScene(pmItem->mapToScene(h[0]));   // +x handle in viewport
    QPointF dir = hx - cv;
    const double len = std::hypot(dir.x(), dir.y());
    dir = (len > 1e-6) ? dir / len : QPointF(1, 0);
    return hx + dir * 24.0;              // 24 px beyond the +x handle
}

int ImageView::maskHitTest(QPoint vp) const
{
    if (!maskHandlesEditable()) return -1;
    if (maskTool == 2) return -1;       // Brush has no drag handles (painting lands in Stage 2)
    const double rHandle = 11;          // px pick radius
    const QPointF p(vp);

    if (maskTool == 1) {                // Radial: 0 centre(move), 1..4 axis handles, 5 outline(rotate)
        const QRectF br = pmItem->boundingRect();
        const QPointF c = maskNormToViewport(maskC);
        if (QLineF(p, c).length() <= rHandle) return 0;
        if (QLineF(p, maskRadialRotateHandleVp(br)).length() <= rHandle) return 5;   // rotate stub
        QPointF h[4];
        maskRadialAxisHandles(br, h);
        for (int i = 0; i < 4; ++i)
            if (QLineF(p, mapFromScene(pmItem->mapToScene(h[i]))).length() <= rHandle) return 1 + i;
        return -1;
    }

    /* Linear: 0 p1, 1 p2, 2 move (centre line). */
    const QPointF v1 = maskNormToViewport(maskP1);
    const QPointF v2 = maskNormToViewport(maskP2);
    if (QLineF(p, v1).length() <= rHandle) return 0;
    if (QLineF(p, v2).length() <= rHandle) return 1;
    const QPointF d = v2 - v1;
    const double len2 = d.x()*d.x() + d.y()*d.y();
    if (len2 > 1e-6) {
        double t = ((p.x()-v1.x())*d.x() + (p.y()-v1.y())*d.y()) / len2;
        t = qBound(0.0, t, 1.0);
        const QPointF proj = v1 + t * d;
        if (QLineF(p, proj).length() <= rHandle) return 2;
    }
    return -1;
}

void ImageView::drawForeground(QPainter *painter, const QRectF &rect)
{
    QGraphicsView::drawForeground(painter, rect);
    if (cropActive()) {
        const QRectF br = pmItem->boundingRect();
        if (br.width() > 0 && br.height() > 0) cropDrawOverlay(painter, br);
        /* Level tool: draw the line being dragged (viewport coords, over the crop overlay). */
        if (cropLevelMode && cropLevelDragging) {
            painter->save();
            painter->resetTransform();
            painter->setRenderHint(QPainter::Antialiasing, true);
            QPen halo(QColor(0, 0, 0, 160)); halo.setWidthF(3.0); halo.setCosmetic(true);
            painter->setPen(halo); painter->drawLine(cropLevelP1, cropLevelP2);
            QPen line(QColor(255, 235, 90, 240)); line.setWidthF(1.4); line.setCosmetic(true);
            painter->setPen(line); painter->drawLine(cropLevelP1, cropLevelP2);
            painter->restore();
        }
        return;
    }
    if (!maskHandlesEditable()) return;
    const QRectF br = pmItem->boundingRect();
    if (br.width() <= 0 || br.height() <= 0) return;
    if      (maskTool == 1) drawRadialMask(painter, br);
    else if (maskTool == 2) drawBrushMask(painter, br);
    else                    drawLinearMask(painter, br);
}

void ImageView::drawLinearMask(QPainter *painter, const QRectF &br)
{
    const QPointF s1 = pmItem->mapToScene(QPointF(maskP1.x()*br.width(), maskP1.y()*br.height()));
    const QPointF s2 = pmItem->mapToScene(QPointF(maskP2.x()*br.width(), maskP2.y()*br.height()));

    /* 1) Red tint ramp, in scene coords so it tracks the image. feather widens the transition
       around the centre: 0 = hard step at the midpoint, 100 = full linear ramp p1->p2. */
    {
        const double f  = qBound(0.0, maskFeather, 100.0) / 100.0;
        const double lo = qBound(0.0, 0.5 - 0.5*f, 1.0);
        const double hi = qBound(0.0, 0.5 + 0.5*f, 1.0);
        /* Tint colour conveys the op: Add (selects) red, Subtract (removes) blue. */
        const QColor base = (maskOp == 1) ? QColor(40, 110, 230) : QColor(220, 40, 40);
        QColor clear = base; clear.setAlpha(0);     // mask 0% -> no tint
        QColor full  = base; full.setAlpha(150);    // mask 100% -> tinted
        /* Invert swaps which end of the ramp is covered. */
        const QColor &cLo = maskInverted ? full  : clear;
        const QColor &cHi = maskInverted ? clear : full;
        QLinearGradient grad(s1, s2);
        grad.setColorAt(0.0, cLo);
        grad.setColorAt(lo,  cLo);
        /* Sample a smoothstep falloff (matches evalGrad in the render) so the tinted edge eases in
           and out instead of kinking at lo/hi -- QLinearGradient only interpolates linearly between
           stops, so we add intermediate ones. */
        if (hi > lo) {
            const int N = 6;
            const int aLo = cLo.alpha(), aHi = cHi.alpha();
            for (int k = 1; k < N; ++k) {
                const double s = double(k) / N;
                const double ss = s * s * s * (s * (s * 6.0 - 15.0) + 10.0);   // smootherstep
                QColor c = base;
                c.setAlpha(int(aLo + (aHi - aLo) * ss + 0.5));
                grad.setColorAt(lo + (hi - lo) * s, c);
            }
        }
        grad.setColorAt(hi, cHi);
        grad.setColorAt(1.0, cHi);
        painter->save();
        painter->setPen(Qt::NoPen);
        painter->setBrush(grad);
        painter->drawPolygon(pmItem->mapToScene(br));   // fills only the image area
        painter->restore();
    }

    /* 2) Centre line, perpendicular 0%/100% guide ticks, and handles -- drawn in viewport coords
       (identity transform) so they stay a constant on-screen size at any zoom. */
    painter->save();
    painter->resetTransform();
    const QPointF v1 = mapFromScene(s1);
    const QPointF v2 = mapFromScene(s2);
    QPointF dir = v2 - v1;
    const double dlen = QLineF(v1, v2).length();
    QPointF perp(0, 0);
    if (dlen > 1e-6) { dir /= dlen; perp = QPointF(-dir.y(), dir.x()); }
    const double guide = 46;        // half-length of the perpendicular guide ticks (px)

    QPen guidePen(QColor(255,255,255,210)); guidePen.setWidthF(1.4);
    painter->setPen(guidePen);
    painter->setBrush(Qt::NoBrush);
    painter->drawLine(v1, v2);                                   // centre line
    painter->drawLine(v1 - perp*guide, v1 + perp*guide);         // 0% boundary
    painter->drawLine(v2 - perp*guide, v2 + perp*guide);         // 100% boundary

    auto handle = [&](const QPointF &c, double r, const QColor &fill) {
        painter->setPen(QPen(QColor(0,0,0,180), 1.2));
        painter->setBrush(fill);
        painter->drawEllipse(c, r, r);
    };
    handle((v1+v2)/2.0, 4.5, QColor(255,255,255,235));           // centre (move)
    handle(v1, 5.5, QColor(120,200,255,235));                    // p1 (0%)
    handle(v2, 5.5, QColor(120,200,255,235));                    // p2 (100%)
    painter->restore();
}

void ImageView::drawRadialMask(QPainter *painter, const QRectF &br)
{
    const QPointF ci(maskC.x()*br.width(), maskC.y()*br.height());      // centre, image px
    const double ax = qMax(1.0, maskRx*br.width()), ay = qMax(1.0, maskRy*br.height());

    /* 1) Elliptical tint ramp (scene coords) via a transformed radial gradient: a unit circle
       mapped to the rotated image ellipse, then to scene. mask 100% inside -> 0% at the boundary
       (Invert swaps); feather widens the transition inward. */
    {
        const double f = qBound(0.0, maskFeather, 100.0) / 100.0;
        const double inner = qBound(0.0, 1.0 - f, 1.0);
        const QColor base = (maskOp == 1) ? QColor(40, 110, 230) : QColor(220, 40, 40);
        QColor clear = base; clear.setAlpha(0);
        QColor full  = base; full.setAlpha(150);
        const QColor cIn   = maskInverted ? clear : full;       // colour at the centre
        const QColor cEdge = maskInverted ? full  : clear;      // colour at/after the boundary
        QRadialGradient rg(QPointF(0, 0), 1.0);
        rg.setColorAt(0.0, cIn);
        rg.setColorAt(inner, cIn);
        if (1.0 > inner) {                                      // smootherstep-sampled ramp stops
            const int N = 6, aIn = cIn.alpha(), aEd = cEdge.alpha();
            for (int k = 1; k < N; ++k) {
                const double s = double(k) / N;
                const double ss = s * s * s * (s * (s * 6.0 - 15.0) + 10.0);
                QColor c = base; c.setAlpha(int(aIn + (aEd - aIn) * ss + 0.5));
                rg.setColorAt(inner + (1.0 - inner) * s, c);
            }
        }
        rg.setColorAt(1.0, cEdge);
        QBrush brush(rg);
        QTransform E;
        E.translate(ci.x(), ci.y()); E.rotate(maskAngle); E.scale(ax, ay);
        brush.setTransform(E * pmItem->sceneTransform());      // unit circle -> image ellipse -> scene
        painter->save();
        painter->setPen(Qt::NoPen);
        painter->setBrush(brush);
        painter->drawPolygon(pmItem->mapToScene(br));          // fills only the image area
        painter->restore();
    }

    /* 2) Ellipse outline + centre/axis handles, viewport coords (constant on-screen size). */
    painter->save();
    painter->resetTransform();
    const double a = maskAngle * 0.017453292519943295;
    const double ca = std::cos(a), sa = std::sin(a);
    QPolygonF poly;
    const int N = 48;
    for (int i = 0; i <= N; ++i) {
        const double t = (6.283185307179586 * i) / N;
        const double lx = ax * std::cos(t), ly = ay * std::sin(t);
        const QPointF pImg = ci + QPointF(ca*lx - sa*ly, sa*lx + ca*ly);
        poly << mapFromScene(pmItem->mapToScene(pImg));
    }
    QPen outline(QColor(255, 255, 255, 210)); outline.setWidthF(1.4);
    painter->setPen(outline); painter->setBrush(Qt::NoBrush);
    painter->drawPolyline(poly);

    auto handle = [&](const QPointF &c, double r, const QColor &fill) {
        painter->setPen(QPen(QColor(0, 0, 0, 180), 1.2));
        painter->setBrush(fill);
        painter->drawEllipse(c, r, r);
    };
    QPointF h[4];
    maskRadialAxisHandles(br, h);
    /* Rotate stub: a short stem beyond the +x handle ending in a knob. */
    const QPointF hx = mapFromScene(pmItem->mapToScene(h[0]));
    const QPointF rotV = maskRadialRotateHandleVp(br);
    painter->setPen(outline);
    painter->drawLine(hx, rotV);
    for (int i = 0; i < 4; ++i)
        handle(mapFromScene(pmItem->mapToScene(h[i])), 5.0, QColor(120, 200, 255, 235));
    handle(rotV, 5.0, QColor(120, 255, 160, 235));                                    // rotate
    handle(mapFromScene(pmItem->mapToScene(ci)), 4.5, QColor(255, 255, 255, 235));    // centre (move)
    painter->restore();
}

/* ---- Brush ---- */

double ImageView::brushRadiusBufPx() const
{
    const double frac = qBound(0.0, maskBrushSize, 100.0) / 200.0;   // diameter % -> radius fraction
    return frac * std::max(maskBrushW, maskBrushH);
}

QPointF ImageView::brushNormToBuf(QPointF n) const
{
    return QPointF(n.x() * maskBrushW, n.y() * maskBrushH);
}

void ImageView::brushBuildBuffers(const QString &paramsJson)
{
    const QJsonObject o = QJsonDocument::fromJson(paramsJson.toUtf8()).object();
    maskBrushStrokesJson = o.value("strokes").toArray();
    maskBrushW = maskBrushH = 0;        // force a rebuild
    brushEnsureBuffers();
}

void ImageView::brushEnsureBuffers()
{
    /* (Re)build the output-oriented preview buffers from the committed strokes whenever the
       displayed pixmap's size changes (e.g. the overlay began before the new image was shown).
       Capped resolution so a live rebuild stays cheap; strokes are normalized so any size is valid. */
    const QRectF br = pmItem ? pmItem->boundingRect() : QRectF();
    if (br.width() <= 0 || br.height() <= 0) return;
    const int cap = 1280;
    const double longE = std::max(br.width(), br.height());
    const double s = (longE > cap) ? cap / longE : 1.0;
    const int w = std::max(1, int(br.width()  * s));
    const int h = std::max(1, int(br.height() * s));
    if (w == maskBrushW && h == maskBrushH && !maskBrushMain.empty()) return;   // already current
    maskBrushW = w; maskBrushH = h;
    maskBrushMain.assign(size_t(w) * h, 0.0f);
    maskBrushStroke.assign(size_t(w) * h, 0.0f);
    ensureAutoGuide();                  // committed auto-mask strokes need the guide to re-raster
    BrushStamp::rasterize(maskBrushStrokesJson, maskBrushMain.data(), maskBrushScratch,
                          w, h, 0, maskGuide.get());
    brushRebuildPreview();
}

void ImageView::brushRebuildPreview(QRect region)
{
    if (maskBrushW <= 0 || maskBrushH <= 0) { maskBrushPreview = QImage(); return; }
    if (maskBrushPreview.width() != maskBrushW || maskBrushPreview.height() != maskBrushH) {
        maskBrushPreview = QImage(maskBrushW, maskBrushH, QImage::Format_ARGB32);
        region = QRect();                       // size changed -> full rebuild
    }
    if (region.isNull()) region = QRect(0, 0, maskBrushW, maskBrushH);
    region &= QRect(0, 0, maskBrushW, maskBrushH);
    if (region.isEmpty()) return;
    const QColor base = (maskOp == 1) ? QColor(40, 110, 230) : QColor(220, 40, 40);
    const int R = base.red(), Gc = base.green(), B = base.blue();
    const double tint = 150.0 / 255.0;
    const double flow = qBound(0.0, maskBrushFlow, 100.0) / 100.0;
    const float *mn = maskBrushMain.data();
    const float *st = maskBrushStroke.data();
    for (int y = region.top(); y <= region.bottom(); ++y) {
        QRgb *line = reinterpret_cast<QRgb*>(maskBrushPreview.scanLine(y));
        const size_t rowK = size_t(y) * maskBrushW;
        for (int x = region.left(); x <= region.right(); ++x) {
            const size_t k = rowK + x;
            float m = mn[k];
            if (maskPainting && st[k] > 0.0f) {            // composite the live stroke
                const float a = float(flow) * st[k];
                m = maskBrushErase ? m * (1.0f - a) : m + a * (1.0f - m);
            }
            const float disp = maskInverted ? (1.0f - m) : m;
            const int alpha = int(disp * tint * 255.0 + 0.5);
            line[x] = qRgba(R, Gc, B, alpha);
        }
    }
}

QRect ImageView::brushSegRect(QPointF a, QPointF b) const
{
    const double r = brushRadiusBufPx() + 2.0;
    const int x0 = int(std::floor(std::min(a.x(), b.x()) - r));
    const int x1 = int(std::ceil (std::max(a.x(), b.x()) + r));
    const int y0 = int(std::floor(std::min(a.y(), b.y()) - r));
    const int y1 = int(std::ceil (std::max(a.y(), b.y()) + r));
    return QRect(x0, y0, x1 - x0 + 1, y1 - y0 + 1) & QRect(0, 0, maskBrushW, maskBrushH);
}

void ImageView::brushStampTo(QPointF bufPt)
{
    BrushStamp::segmentMax(maskBrushStroke.data(), maskBrushW, maskBrushH,
                           maskBrushLast, bufPt, brushRadiusBufPx(),
                           qBound(0.0, maskFeather, 100.0) / 100.0,
                           maskStrokeAM.on ? &maskStrokeAM : nullptr);
    maskBrushLast = bufPt;
}

void ImageView::adjustBrushSize(double delta)
{
    const double sz = qBound(1.0, maskBrushSize + delta, 100.0);
    if (sz == maskBrushSize) return;
    maskBrushSize = sz;
    emit maskBrushSizeRequested(maskBrushSize);     // sync the dock + persist
    if (maskEditMode && maskHover) viewport()->update();   // cursor circle
}

void ImageView::ensureAutoGuide()
{
    /* Build a small luminance guide from the displayed image (output-oriented), once per image, and
       register it by path so the develop render samples the SAME guide as this preview. */
    if (maskGuide && maskGuide->valid()) return;
    if (!pmItem || pmItem->pixmap().isNull() || currentImagePath.isEmpty()) return;
    const QImage img = pmItem->pixmap().toImage();
    if (img.isNull()) return;
    const int cap = 1024;
    const int longE = std::max(img.width(), img.height());
    const double s = (longE > cap) ? double(cap) / longE : 1.0;
    const int gw = std::max(1, int(img.width()  * s));
    const int gh = std::max(1, int(img.height() * s));
    const QImage small = img.scaled(gw, gh, Qt::IgnoreAspectRatio, Qt::SmoothTransformation)
                            .convertToFormat(QImage::Format_ARGB32);
    auto g = std::make_shared<BrushStamp::Guide>();
    g->w = gw; g->h = gh; g->lum.resize(size_t(gw) * gh);
    for (int y = 0; y < gh; ++y) {
        const QRgb *line = reinterpret_cast<const QRgb*>(small.constScanLine(y));
        for (int x = 0; x < gw; ++x) {
            const QRgb p = line[x];
            g->lum[size_t(y)*gw + x] =
                float((0.299*qRed(p) + 0.587*qGreen(p) + 0.114*qBlue(p)) / 255.0);
        }
    }
    maskGuide = g;
    BrushStamp::putGuide(currentImagePath, g);
}

void ImageView::toggleAutoMask()
{
    maskBrushAutoMask = !maskBrushAutoMask;
    if (maskBrushAutoMask) ensureAutoGuide();
    emit maskBrushAutoMaskRequested(maskBrushAutoMask);   // sync the dock checkbox + persist
    if (maskEditMode && maskHover) viewport()->update();  // cursor hint
}

void ImageView::brushUndoStroke()
{
    if (maskBrushStrokesJson.isEmpty()) return;
    maskBrushStrokesJson.removeLast();
    maskBrushW = maskBrushH = 0;                     // force re-raster of the remaining strokes
    brushEnsureBuffers();
    viewport()->update();
    /* Persist the shortened stroke list and re-render. */
    QJsonObject o;
    o["size"] = maskBrushSize; o["flow"] = maskBrushFlow; o["autoMask"] = maskBrushAutoMask;
    o["strokes"] = maskBrushStrokesJson;
    emit maskGeometryChanged(QString::fromUtf8(QJsonDocument(o).toJson(QJsonDocument::Compact)));
}

void ImageView::drawBrushMask(QPainter *painter, const QRectF &br)
{
    brushEnsureBuffers();           // heal if the pixmap size changed since beginMaskEdit
    /* 1) Tint preview image over the photo (scene coords, scales with the image). */
    if (!maskBrushPreview.isNull()) {
        painter->save();
        painter->setRenderHint(QPainter::SmoothPixmapTransform, true);
        painter->drawImage(pmItem->mapToScene(br).boundingRect(), maskBrushPreview);
        painter->restore();
    }
    /* 2) Brush-size cursor circle (viewport coords, scales with zoom but constant pen width). */
    if (maskBrushCursorOn) {
        const double imgLong = std::max(br.width(), br.height());
        const double rImg = (qBound(0.0, maskBrushSize, 100.0) / 200.0) * imgLong;
        const QPointF cImg = pmItem->mapFromScene(mapToScene(maskBrushCursorVp));
        const QPointF cVp(maskBrushCursorVp);
        const double rVp = QLineF(cVp, mapFromScene(pmItem->mapToScene(cImg + QPointF(rImg, 0)))).length();
        painter->save();
        painter->resetTransform();
        const QColor cur = maskBrushErase    ? QColor(255, 170, 170, 235)   // erase
                         : maskBrushAutoMask ? QColor(150, 255, 150, 235)   // auto-mask
                                             : QColor(255, 255, 255, 235);
        QPen pen(cur);
        pen.setWidthF(1.3);
        if (maskBrushAutoMask) pen.setStyle(Qt::DashLine);     // dashed = auto-mask on
        painter->setPen(pen);
        painter->setBrush(Qt::NoBrush);
        painter->drawEllipse(cVp, rVp, rVp);
        painter->restore();
    }
}

void ImageView::clear()
{
    if (G::isLogger) G::log("ImageView::clear");
    infoText = "";
    infoOverlay->setText("");
    QPixmap nullPm;
    pmItem->setPixmap(nullPm);
    pmItem->setVisible(false);
}

void ImageView::noJpgAvailable()
{
    if (G::isLogger) G::log("ImageView::noJpgAvailable");
    pmItem->setVisible(false);
    infoOverlay->setText("");
}

void ImageView::scale(bool isNewImage)
{
/*
    Scales the pixmap to zoom.  Panning is automatically to the cursor position
    because setTransformationAnchor(QGraphicsView::AnchorUnderMouse).

    ● The scroll percentage is stored so it can be matched in the next image if it
      is zoomed.
    ● Flags are set for the zoom condition.
    ● The cursor to set to pointer if not zoomed and hand if zoomed.
    ● The pick icon and shooting info text are relocated as necessary.
    ● The app status is updated.
    ● The zoom amount is updated in ZoomDlg if it is open.

    If isSlideshow then hide mouse cursor unless is moves.
*/

    if (G::isLogger) G::log("ImageView::scale");

    // /*
    if (isFit || G::isSlideShow) {
        // 1. Initial fit calculation
        fitInView(pmItem, Qt::KeepAspectRatio);
        zoom = viewportTransform().m11() * G::actDevicePixelRatio;

        // 2. Constraint: Do not exceed 100% (G::actDevicePixelRatio)
        if (zoom > 1.0) {
            zoom = 1.0;
            transform.reset();
            // High DPI 100% is 1.0 / actDevicePixelRatio
            double highDpiZoom = 1.0 / G::actDevicePixelRatio;
            transform.scale(highDpiZoom, highDpiZoom);
            setTransform(transform);

            // Center the image manually since fitInView is overridden
            centerOn(pmItem);
        }
    }
    else {
        // Manual zoom mode
        transform.reset();
        double highDpiZoom = zoom / G::actDevicePixelRatio;
        transform.scale(highDpiZoom, highDpiZoom);
        setTransform(transform);
    }

    emit zoomChange(zoom, "ImageView::scale");

    // The rest of your functional logic remains identical
    isScrollable = (zoom > zoomFit);
    if (isScrollable) scrollPct = getScrollPct();

    // Maintain predictive focus and panning logic
    int i = dm->currentSfRow;
    float x = dm->sf->index(i, G::FocusXColumn).data().toFloat();
    float y = dm->sf->index(i, G::FocusYColumn).data().toFloat();

    if (x >= 0 && y >= 0 && isScrollable && panToFocus && isNewImage) {
        panTo(x, y);
    }

    // Update cursor and overlays
    if (!G::isSlideShow) {
        if (isScrollable) setCursor(Qt::OpenHandCursor);
        else setCursor(isFit ? Qt::ArrowCursor : Qt::PointingHandCursor);
    }

    placeClassificationBadge();
    setShootingInfo(infoText);
    emit updateStatus(true, "", "ImageView::scale");

    /* User zoom while cropping: the crop tool doesn't drive zoom, but the on-screen frame must
       follow the same image content, so re-derive it from cropN. */
    if (cropActive()) { cropSyncFrameFromN(); viewport()->update(); }
    //*/
}

bool ImageView::sceneBiggerThanView()
{
    if (G::isLogger) G::log("ImageView::sceneBiggerThanView");
    QPoint pTL = mapFromScene(0, 0);
    QPoint pBR = mapFromScene(scene->width(), scene->height());
    int sceneViewWidth = pBR.x() - pTL.x();
    int sceneViewHeight = pBR.y() - pTL.y();
    if (sceneViewWidth > rect().width() || sceneViewHeight > rect().height())
        return true;
    else
        return false;
}

qreal ImageView::getFitScaleFactor(QRectF container, QRectF content)
{
    if (G::isLogger) G::log("ImageView::getFitScaleFactor");
//    qDebug() << "ImageView::getFitScaleFactor" << container << content;
    qreal hScale = static_cast<qreal>(container.width() - 2) / content.width() * G::actDevicePixelRatio;
    qreal vScale = static_cast<qreal>(container.height() - 2) / content.height() * G::actDevicePixelRatio;
    return (hScale < vScale) ? hScale : vScale;
}

void ImageView::setScrollBars(QPointF scrollPct)
{
    if (G::isLogger) G::log("ImageView::setScrollBars");
    getScrollBarStatus();
    scrl.hVal = scrl.hMin + scrollPct.x() * (scrl.hMax - scrl.hMin);
    scrl.vVal = scrl.vMin + scrollPct.y() * (scrl.vMax - scrl.vMin);
    horizontalScrollBar()->setValue(scrl.hVal);
    verticalScrollBar()->setValue(scrl.vVal);
}

void ImageView::getScrollBarStatus()
{
    if (G::isLogger) G::log("ImageView::getScrollBarStatus");
    scrl.hMin = horizontalScrollBar()->minimum();
    scrl.hMax = horizontalScrollBar()->maximum();
    scrl.hVal = horizontalScrollBar()->value();
    scrl.hPct = qreal(scrl.hVal - scrl.hMin) / (scrl.hMax - scrl.hMin);
    scrl.vMin = verticalScrollBar()->minimum();
    scrl.vMax = verticalScrollBar()->maximum();
    scrl.vVal = verticalScrollBar()->value();
    scrl.vPct = qreal(scrl.vVal - scrl.vMin) / (scrl.vMax - scrl.vMin);
    /*
    qDebug() << "ImageView::getScrollBarStatus"
             << "scrl.hMin =" << scrl.hMin
             << "scrl.hMax =" << scrl.hMax
             << "scrl.hVal =" << scrl.hVal
             << "scrl.vMin =" << scrl.vMin
             << "scrl.vMax =" << scrl.vMax
             << "scrl.vVal =" << scrl.vVal
             << "scrl.hPct =" << scrl.hPct
             << "scrl.vPct =" << scrl.vPct
                ;
//      */
}

QPointF ImageView::getScrollPct()
{
/*
   The view center is defined by the scrollbar values. The value is converted to a
   percentage to be used to match position in the next image if zoomed.
*/
    if (G::isLogger) G::log("ImageView::getScrollPct");
    getScrollBarStatus();
    return QPointF(scrl.hPct, scrl.vPct);
}

void ImageView::setClassificationBadgeImageDiam(int d)
{
    if (G::isLogger) G::log("ImageView::setClassificationBadgeImageDiam");
    classificationBadgeDiam = d;
    placeClassificationBadge();
}

void ImageView::placeClassificationBadge()
{
/*
    The bright green thumbsUp pixmap shows the pick status for the current image.
    This function locates the pixmap in the bottom corner of the image label as the
    image is resized and zoomed, adjusting for the aspect ratio of the image and
    size.  NOT IN USE.
*/
    if (G::isLogger) G::log("ImageView::placeClassificationBadge");

    classificationLabel->setDiameter(0);  // hide, not being used
    return; // not used

    QPoint sceneBottomRight = mapFromScene(sceneRect().bottomRight());

    int x, y = 0;                       // bottom right coordinates of visible image

    // if the image view is not as wide as the window
    if (sceneBottomRight.x() < rect().width())
        x = sceneBottomRight.x();
    else
        x = rect().width();
\
    // if the image view is not as high as the window
    if (sceneBottomRight.y() < rect().height())
        y = sceneBottomRight.y();
    else
        y = rect().height();

    int o = 5;                          // offset margin from edge
    int d = classificationBadgeDiam;    // diameter of the classification label
    classificationLabel->setDiameter(d);
    classificationLabel->move(x - d - o, y - d - o);
}

void ImageView::setBullseyeVisible(bool isVisible)
{
    // bullseye->setVisible(isVisible);
}

void ImageView::placeTarget(float x, float y)
{
    if (G::isLogger) G::log("ImageView::placeTarget");

    if (x >= 0 && y >= 0) {
        // qDebug() << "bullseye" << bullseye->size();
        // Get the scene rectangle
        QRectF sceneRect = this->sceneRect();

        // Calculate the scene coordinates
        QPointF scenePos(sceneRect.left() + x * sceneRect.width(),
                         sceneRect.top() + y * sceneRect.height());

        // Map the scene coordinates to the view (widget) coordinates
        QPoint viewPos = mapFromScene(scenePos);

        // Move the QLabel so its center is at viewPos
        QSize labelSize = bullseye->size();
        QPoint topLeft(viewPos.x() - labelSize.width() / 2,
                       viewPos.y() - labelSize.height() / 2);

        bullseye->move(topLeft);
        bullseye->setVisible(true);
    }
    else bullseye->setVisible(false);
}

void ImageView::showRubber(QRect r)
{
    rubberBand->setGeometry(r);
    rubberBand->show();
}

void ImageView::activateRubberBand()
{
    if (G::isLogger) G::log("ImageView::activateRubberBand");
    isRubberBand = true;
    setCursor(Qt::CrossCursor);
    QString msg = "Rubberband activated.  Make a selection in the ImageView.\n"
                  "Press Esc to quit rubberbanding.";
    G::popup->showPopup(msg, 2000);
}

void ImageView::quitRubberBand()
{
    if (G::isLogger) G::log("ImageView::quitRubberBand");
    isRubberBand = false;
    setCursor(Qt::ArrowCursor);
}

void ImageView::resizeEvent(QResizeEvent *event)
{
/*
   Manage behavior when window resizes.

    ● if image zoom is not zoomFit then no change
    ● if zoomFit then recalc and maintain
    ● if zoomed and resize to view entire image then engage zoomFit
    ● if view larger than image and resize to clip image then engage zoomFit.
    ● move and size pick icon and shooting info as necessary
    ● show predictive focus change
*/
    if (G::isLogger) G::log("ImageView::resizeEvent");

    // Do not process resize events during initial application startup
    if (G::isInitializing) return;

    // Call the base class implementation
    QGraphicsView::resizeEvent(event);

    // Recalculate the factor required to fit the image in the new window size
    zoomFit = getFitScaleFactor(rect(), scene->itemsBoundingRect());

    if (isFit) {
        // Optimized scaling: Automatically fits and centers the image
        fitInView(pmItem, Qt::KeepAspectRatio);

        // Synchronize the zoom member with the new viewport scale
        zoom = viewportTransform().m11();

        // Apply secondary logic (status updates, badges)
        scale();
    } else {
        // If not in Fit mode, we still need to update the prediction overlays
        bool adjustCenter = true;
        bool refresh = true;
        showNormalizedViewport(adjustCenter, refresh, "ImageView::resizeEvent");
    }

    // Maintain UI element positions relative to the new window boundaries
    placeClassificationBadge();
    setShootingInfo(infoText);

    // Keep the crop frame on the same image content after a window resize.
    if (cropActive()) { cropSyncFrameFromN(); viewport()->update(); }




//     if (G::isLogger) G::log("ImageView::resizeEvent");
//     /*
//     qDebug() << "ImageView::resizeEvent"
//              << "G::isInitializing =" << G::isInitializing
//              << "G::isLinearLoadDone =" << G::isLinearLoadDone
//              << "isFirstImageNewInstance =" << isFirstImageNewInstance;
//     //    */
//     if (G::isInitializing) return;
//     QGraphicsView::resizeEvent(event);
//     zoomFit = getFitScaleFactor(rect(), scene->itemsBoundingRect());
// //    zoomFit = getFitScaleFactor(rect(), pmItem->boundingRect());
//     if (isFit) {
//         setFitZoom();
//         scale();
//     }
//     placeClassificationBadge();
//     setShootingInfo(infoText);
//     showPredictedFocus();
}

QSize ImageView::viewportInScene()
{
/*
    Called by IconView::zoomCursor to draw the outline of the size of the viewport
    relative to the scene as the cursor while hovering on the thumbView.
*/
    QString srcFun = "ImageView::viewportInScene";
    int imW = pmItem->boundingRect().width();
    int imH = pmItem->boundingRect().height();
    QPolygonF p = mapToScene(viewport()->rect());
    qreal x1 = p.at(0).x();
    qreal y1 = p.at(0).y();
    qreal x2 = p.at(2).x();
    qreal y2 = p.at(2).y();
    qreal w = x2 - x1;
    qreal h = y2 - y1;
    return QSize(w, h);
}

void ImageView::showNormalizedViewport(bool adjustCenter, bool refresh, QString src)
{
/*
    Not being used.

    Sends the normalized coordinates for viewport in scene to IconView delegate,
    where they can be used to draw the viewport border inside the thumbnail. Can
    crash if the preceding image was a video.

    Documentation: see FOCUS PREDICTOR in notes/Documentation.txt

*/
    return;
    QString srcFun = "ImageView::showNormalizedViewport";
    qDebug().noquote()
    << srcFun.leftJustified(40)
    << "sfRow =" << dm->currentSfRow
    << "src =" << src
    ;

    if (zoom <= zoomFit) {
        // /*
        qDebug().noquote()
             << srcFun.leftJustified(40)
             << "sfRow =" << dm->currentSfRow
             << "zoom <= zoomFit (no action req'd)"
             << "src =" << src
            ; //*/
        emit showLoupeRect(false);
        return;
    }

    static QSizeF prevVpSizeN = QSizeF();
    static QPointF prevVpCntrN = QPointF();

    // datamodel width/height unreliable (aspect ratio issue or missing)
    int imW = pmItem->boundingRect().width();
    int imH = pmItem->boundingRect().height();
    QRect vpRect = viewport()->rect();

    // generate normalized coordinates for viewport in scene
    QPolygonF p = mapToScene(viewport()->rect());
    qreal x1 = p.at(0).x() / imW;
    qreal y1 = p.at(0).y() / imH;
    qreal x2 = p.at(2).x() / imW;
    qreal y2 = p.at(2).y() / imH;
    qreal w = x2 - x1;
    qreal h = y2 - y1;

    QSizeF vpSizeN = QSizeF(w, h);
    qreal vpA = static_cast<qreal>(vpRect.width()) / vpRect.height();
    if (adjustCenter)
        vpCntrN = QPointF(x1 + w/2, y1 + h/2);
    else
        panTo(vpCntrN.x(), vpCntrN.y());

    // if repeat then return
    if (vpSizeN == prevVpSizeN && vpCntrN == prevVpCntrN) {
        qDebug().noquote()
            << srcFun.leftJustified(40)
            << "sfRow =" << dm->currentSfRow
            << "vpSizeN =" << prevVpSizeN
            << "vpSizeN =" << vpCntrN
            << "REPEAT: DO NOT SIGNAL";
            ;
        return;
    }

    // /*
    qDebug().noquote()
             << srcFun.leftJustified(40)
             << "sfRow =" << dm->currentSfRow
             << "imW =" << imW
             << "imH =" << imH
             << "vpSizeN =" << vpSizeN
             << "vpCntrN =" << vpCntrN
             << "vpA =" << vpA
             << "src =" << src
             << "refresh =" << refresh
        ;//*/

    emit loupeRect(vpSizeN, vpA, vpCntrN, refresh);

    prevVpSizeN = vpSizeN;
    prevVpCntrN = vpCntrN;

}

void ImageView::predictPanToFocus()
{
/*
    Documentation: see FOCUS PREDICTOR in documentation.txt
*/
    if (G::isLogger) G::log("ImageView::focusPrediction");

    focusPrediction = focusPredictor->predict(pmItem->pixmap().toImage());
    panTo(focusPrediction.x(), focusPrediction.y());
}

void ImageView::panTo(float xPct, float yPct)
{
/*
   When the image is zoomed and a thumbnail is mouse clicked the position within the thumb is
   passed here as the percent of the width and height. The zoom amount is maintained and the
   main image is panned to the same location as on the thumb. This makes it quick to check
   eyes and other details in many images.
*/
    QString srcFun = "ImageView::thumbClick";
    if (G::isLogger) G::log("ImageView::thumbClick");
    // qDebug().noquote() << srcFun.leftJustified(40);
    if (zoom > zoomFit) {
        centerOn(QPointF(xPct * sceneRect().width(), yPct * sceneRect().height()));
    }
}

qreal ImageView::getZoom()
{
    // use view center to make sure inside scene item
    if (G::isLogger) G::log("ImageView::thumbClick");
    qreal x1 = mapToScene(rect().center()).x();
    qreal x2 = mapToScene(rect().center() + QPoint(1, 0)).x();
    qreal calcZoom = 1.0 / (x2 - x1);
    /*
    qDebug() << G::t.restart() << "\t" << "getZoom():   zoom =" << zoom
             << "x1 =" << x1 << "x2 =" << x2
             << "calc zoom =" << calcZoom;
    //*/
    return calcZoom;
}

void ImageView::updateToggleZoom(qreal toggleZoomValue)
{
/*
    Slot for signal from update zoom dialog to set the amount to zoom when user
    clicks on the unzoomed image.
*/
    if (G::isLogger) G::log("ImageView::updateToggleZoom");
    toggleZoom = toggleZoomValue;
}

void ImageView::refresh()
{
    if (G::isLogger) G::log("ImageView::refresh");
    setFitZoom();
    scale();
}

void ImageView::zoomIn()
{
    if (G::isLogger) G::log("ImageView::zoomIn");
    /*
    double highDpiZoom = zoom / G::actDevicePixelRatio;
    QPoint pTL = mapFromScene(0, 0);
    QPoint pBR = mapFromScene(scene->width(), scene->height());
    QPointF vCtr = mapToScene(rect().center());
    int sceneViewWidth = pBR.x() - pTL.x();
    int sceneViewHeight = pBR.y() - pTL.y();
    scrl.hMin = horizontalScrollBar()->minimum();
    scrl.hMax = horizontalScrollBar()->maximum();
    scrl.hVal = horizontalScrollBar()->value();
    scrl.hPct = qreal(scrl.hVal - scrl.hMin) / (scrl.hMax - scrl.hMin);
    scrl.vMin = verticalScrollBar()->minimum();
    scrl.vMax = verticalScrollBar()->maximum();
    scrl.vVal = verticalScrollBar()->value();
    scrl.vPct = qreal(scrl.vVal - scrl.vMin) / (scrl.vMax - scrl.vMin);
    qDebug() << "ImageView::zoomIn"
             << "\n  zoom           =" << zoom
             << "\n  highDpiZoom    =" << highDpiZoom
             << "\n  zoomFit        =" << zoomFit
             << "\n  pTL            =" << pTL
             << "\n  pBR            =" << pBR
             << "\n  rect()         =" << rect()
             << "\n  vCtr           =" << vCtr
             << "\n  sceneRect()    =" << sceneRect()
             << "\n  sceneViewW     =" << sceneViewWidth
             << "\n  sceneViewH     =" << sceneViewHeight
             << "\n  scrl.hMin      =" << scrl.hMin
             << "\n  scrl.hMax      =" << scrl.hMax
             << "\n  scrl.hVal      =" << scrl.hVal
             << "\n  scrl.vMin      =" << scrl.vMin
             << "\n  scrl.vMax      =" << scrl.vMax
             << "\n  scrl.vVal      =" << scrl.vVal
             << "\n  scrl.hPct      =" << scrl.hPct
             << "\n  scrl.vPct      =" << scrl.vPct
                ;
                //*/

    QPointF vCtr = mapToScene(rect().center());
    zoom *= (1.0 + zoomInc);
    zoom = zoom > zoomMax ? zoomMax: zoom;
    isFit = false;
    scale();
    if (zoom > zoomFit) {
        centerOn(vCtr);
    }
}

void ImageView::zoomOut()
{
    if (G::isLogger) G::log("ImageView::zoomOut");
    QPointF vCtr = mapToScene(rect().center());
    zoom *= (1.0 - zoomInc);
    zoom = zoom < zoomMin ? zoomMin : zoom;
    isFit = false;
    scale();
    if (zoom > zoomFit) {
        centerOn(vCtr);
    }
}

void ImageView::zoomToFit()
{
    if (G::isLogger) G::log("ImageView::zoomToFit");
    zoom = zoomFit;
    scale();
}

void ImageView::zoomTo(qreal zoomTo)
{
/*
    Called from ZoomDlg when the zoom is changed. From here the message is passed
    on to ImageView::scale(), which in turn makes the proper scale change.
*/
    if (G::isLogger) G::log("ImageView::zoomTo");
    zoom = zoomTo;
    isFit = false;
    scale();
}

void ImageView::resetFitZoom()
{
    if (G::isLogger) G::log("ImageView::resetFitZoom");
    setSceneRect(scene->itemsBoundingRect());
    /*
    qDebug() << "ImageView::resetFitZoom"
             << "rect() =" << rect()
             << "sceneRect() =" << sceneRect()
             << "scene->itemsBoundingRect() =" << scene->itemsBoundingRect();
//    */
    zoomFit = getFitScaleFactor(rect(), scene->itemsBoundingRect());
    zoom = zoomFit;
    if (limitFit100Pct  && zoom > toggleZoom) zoom = toggleZoom;
    scale();
}

void ImageView::setFitZoom()
{
    if (G::isLogger) G::log("ImageView::setFitZoom");
    zoom = zoomFit;
    if (limitFit100Pct && zoom > toggleZoom) zoom = toggleZoom;
}

void ImageView::zoomToggle()
{
/*
    Alternate between zoom-to-fit and another zoom value (typically 100% to check
    detail).  The other zoom value (toggleZoom) can be assigned in ZoomDlg and
    defaults to 1.0
*/
    if (G::isLogger) G::log("ImageView::zoomToggle");
    QString srcFun = "ImageView::zoomToggle";
    // qDebug().noquote() << srcFun.leftJustified(40);

    if (isFit && zoomFit >= toggleZoom) {
        QString toggleZoomPct = QString::number(toggleZoom * 100) + "%";
        G::popup->showPopup("Already at " + toggleZoomPct, 1000);
        isLocalMouseClick = false;
        return;
    }

    isFit = !isFit;
    isFit ? zoom = zoomFit : zoom = toggleZoom;
    scale();

    if (panToFocus && !isLocalMouseClick) {
        predictPanToFocus();
    }
    isLocalMouseClick = false;
}

void ImageView::rotateImage(int degrees)
{
/*
    This is called from MW::setRotation and rotates the image currently shown in loupe view.
    Loupe view is a QGraphicsView of the QGraphicsScene scene. The scene contains one
    QGraphicsItem pmItem, which in turn, contains the pixmap. When the pixmap is rotated the
    scene bounding rectangle must be adjusted and the zoom factor to fit recalculated.
    Finally, scale() is called to fit the image if the image was not zoomed.

    The initial image rotation, based on the image metadata orientation, is done in either
    ImageDecoder, Pixmap or Thumb, depending on where the load image is called from.
*/
    if (G::isLogger) G::log("ImageView::rotateImage");
    //qDebug() << "ImageView::rotateImage degrees =" << degrees;

    // extract pixmap, rotate and reset to pmItem
    QPixmap pm = pmItem->pixmap();
    QTransform trans;
    trans.rotate(degrees);
    pmItem->setPixmap(pm.transformed(trans, Qt::SmoothTransformation));

    // reset the scene
    setSceneRect(scene->itemsBoundingRect());

    // recalc zoomFit factor
    zoomFit = getFitScaleFactor(rect(), pmItem->boundingRect());

    // if in isFit mode then zoom accordingly
    if (isFit) {
        zoom = zoomFit;
        scale();
    }

    // Invalidate the IconViewDelegate cache for this specific image
    if (thumbView) {
        // You'll need to call the clear method on the delegate
        thumbView->iconViewDelegate->clearCacheItem(dm->currentSfRow);
        // Then tell the view to redraw that specific thumb
        thumbView->refreshIcon(dm->currentSfIdx, "ImageView::rotateImage");
    }
}

void ImageView::rotateByExifRotation(QImage &image, QString &imageFullPath)
{
    // not being used

    if (G::isLogger) G::log("ImageView::rotateByExifRotation", imageFullPath);
    qDebug() << "ImageView::rotateByExifRotation" << imageFullPath;

    QTransform trans;
    int dmRow = dm->rowFromPath(imageFullPath);
    if (dmRow == -1) return;
    int orientation = dm->index(dmRow, G::OrientationColumn).data().toInt();

    switch (orientation) {
        case 2:
            image = image.mirrored(true, false);
            break;
        case 3:
            trans.rotate(180);
            image = image.transformed(trans, Qt::SmoothTransformation);
            break;
        case 4:
            image = image.mirrored(false, true);
            break;
        case 5:
            trans.rotate(90);
            image = image.transformed(trans, Qt::SmoothTransformation);
            image = image.mirrored(true, false);
            break;
        case 6:
            trans.rotate(90);
            image = image.transformed(trans, Qt::SmoothTransformation);
            break;
        case 7:
            trans.rotate(90);
            image = image.transformed(trans, Qt::SmoothTransformation);
            image = image.mirrored(false, true);
            break;
        case 8:
            trans.rotate(270);
            image = image.transformed(trans, Qt::SmoothTransformation);
            break;
    }
}

void ImageView::sceneGeometry(QPoint &sceneOrigin, QRectF &scene_Rect, QRect &cwRect)
{
/*
    Return the top left corner of the image showing in the central widget in percent.
    This is used to determine the zoomCursor aspect in ThumbView.
*/
    if (G::isLogger) G::log("ImageView::sceneGeometry");
    sceneOrigin = mapFromScene(0.0, 0.0);
    scene_Rect = sceneRect();
    cwRect = rect();
    qDebug() << "sceneOrigin =" << sceneOrigin
             << "sceneBottomRight =" << mapFromScene(sceneRect().bottomRight())
             << "scene_rect =" << scene_Rect
             << "cwRect =" << cwRect;
}

QPoint ImageView::scene2CW(QPointF pctPt)
{
    QPoint origin = mapFromScene(0.0, 0.0);
    QPoint bottomRight = mapFromScene(sceneRect().bottomRight());
    int w = bottomRight.x() - origin.x();
    int h = bottomRight.y() - origin.y();
    int x = static_cast<int>(pctPt.x() * w) + origin.x();
    int y = static_cast<int>(pctPt.y() * h) + origin.y();
    qDebug() << "pctPt =" << pctPt
             << "origin =" << origin
             << "bottomRight =" << bottomRight
             << "w =" << w
             << "h =" << h
             << "x =" << x
             << "y =" << y
                ;
    return QPoint(x, y);
}

QSizeF ImageView::vpNormSizeInScene()
{
    QString srcFun = "ImageView::vpMidInScene";
    qreal sceneW = scene->width();
    qreal sceneH = scene->height();
    qreal vpW = viewport()->width();
    qreal vpH = viewport()->height();
    qreal vpNormW = vpW / sceneW;
    qreal vpNormH = vpH / sceneH;
    /*
    QSizeF vpNormSize(vpNormW, vpNormH);
    qDebug() << "Scene      =" << scene->sceneRect();
    qDebug() << "Viewport   =" << viewport()->rect();
    qDebug() << "vpNormSize =" << vpNormSize;
    */
    return QSizeF(vpNormW, vpNormH);
}

void ImageView::focus()
{
    if (G::isLogger) G::log("ImageView::focus");
    // int row = dm->fPathRow[dm->currentFilePath];
    int row = dm->fPathRowValue(dm->currentFilePath);
    int fX = dm->sf->index(row, G::FocusXColumn).data().toInt();
    int fY = dm->sf->index(row, G::FocusYColumn).data().toInt();
    int w = dm->sf->index(row, G::WidthColumn).data().toInt();
    int h = dm->sf->index(row, G::HeightColumn).data().toInt();
    int orient = dm->sf->index(row, G::OrientationColumn).data().toInt();
    double x = fX * 1.0 / w;
    double y = fY * 1.0 / h;
    /*
    double x=0,y=0;
    if (orient == 1) {
        x = fX * 1.0 / w;
        y = fY * 1.0 / h;
    }
    if (orient == 8) {
        x = fX * 1.0 / h;
        y = fY * 1.0 / w;
    }*/
//    QPointF pct = QPointF(0,0);
    QPointF pct = QPointF(x, y);
    QPoint p = scene2CW(pct);
    QRect r(p.x() - 10, p.y() - 10, 20, 20);
    showRubber(r);
    QString s = QString::number((int)(x*100)) + "," + QString::number((int)(y*100));
    qDebug() << "x,y =" << s
             << "fX =" << fX
             << "fY =" << fY
             << "w =" << w
             << "g =" << h
        ;
}

bool ImageView::isNullImage()
{
    return pmItem->pixmap().isNull();
}

void ImageView::changeInfoOverlay()
{
    if (G::isLogger) G::log("ImageView::changeInfoOverlay");
    // callback setShootingInfo() to resize infoOverlay if necessary
    infoString->change([this]() { this->setShootingInfo(); });
}

void ImageView::updateShootingInfo()
{
    if (G::isLogger) G::log("ImageView::updateShootingInfo");
    QModelIndex idx = dm->currentSfIdx;
    // QModelIndex idx = thumbView->currentIndex();
    QString current = infoString->getCurrentInfoTemplate();
    infoText = infoString->parseTokenString(infoString->infoTemplates[current],
                                                currentImagePath, idx);
    infoOverlay->setText(infoText);
}

void ImageView::setShootingInfo(QString infoText)
{
/*
    Locate and format the info label, which currently displays the shooting
    information in the top left corner of the image.  The text has a drop shadow
    to help make it visible against different coloured backgrounds.

    window (w) and view (v) sizes are updated during resize
*/
    if (G::isLogger) G::log("ImageView::setShootingInfo");

    if (infoText == "") {
        QModelIndex idx = thumbView->currentIndex();
        QString current = infoString->getCurrentInfoTemplate();
        infoText = infoString->parseTokenString(infoString->infoTemplates[current],
                                                    currentImagePath, idx);    }

    int offset = 10;                        // offset pixels from the edge of image
    int x, y = 0;                           // top left coordinates of info symbol
    // the top left of the image in viewport coordinates
    QPoint viewportCoord = mapFromScene(pmItem->mapToScene(0,0));
    // if the scene is not as wide as the view
    if (viewportCoord.x() > 0)
        x = viewportCoord.x() + offset;
    else x = offset;

    // if the scene is not as high as the view
    if (viewportCoord.y() > 0)
        y = viewportCoord.y() + offset;
    else y = offset;

    QFont font("Tahoma", infoOverlayFontSize);
    font.setKerning(true);
    infoOverlay->setFont(font);      // not working
    infoOverlay->setStyleSheet("font-size: " + QString::number(infoOverlayFontSize) + "pt;");
    infoOverlay->setText(infoText);
    infoOverlay->adjustSize();
    // make a little wider to account for the drop shadow
    infoOverlay->resize(infoOverlay->width()+10, infoOverlay->height()+10);
    infoOverlay->move(x, y);
}

void ImageView::monitorCursorState()
{
    // not being used
    if (G::isLogger) G::log("ImageView::monitorCursorState");
    static QPoint lastPos;

    if (QCursor::pos() != lastPos) {
        lastPos = QCursor::pos();
        if (cursorIsHidden) {
            QApplication::restoreOverrideCursor();
            cursorIsHidden = false;
        }
    }
    else {
        if (!cursorIsHidden) {
            QApplication::setOverrideCursor(Qt::BlankCursor);
            cursorIsHidden = true;
        }
    }
}

void ImageView::setBackgroundColor(QColor bg)
{
    if (G::isLogger) G::log("ImageView::setBackgroundColor");
    scene->setBackgroundBrush(bg);
}

void ImageView::setCursorHiding(bool hide)
{
    if (G::isLogger) G::log("ImageView::setCursorHiding");
    if (hide) {
        mouseMovementTimer->start(500);
    } else {
        mouseMovementTimer->stop();
        if (cursorIsHidden) {
            QApplication::restoreOverrideCursor();
            cursorIsHidden = false;
        }
    }
}

void ImageView::hideCursor()
{
/*
    Called from mouse move event in a delay if in slideshow mode.
*/
    if (G::isLogger) G::log("ImageView::hideCursor");
    setCursor(Qt::BlankCursor);
}

void ImageView::scrollContentsBy(int dx, int dy)
{
    if (G::isLogger) G::log("ImageView::scrollContentsBy");
    scrollCount++;
    QGraphicsView::scrollContentsBy(dx, dy);
}

// EVENTS
// SCROLLING
void ImageView::scrollChange(int /*value*/)
{
    if (G::isLogger) G::log("ImageView::scrollChange");

    /*
    qDebug() << "ImageView::scrollChange"
             << "mapToScene(viewport()->rect()) =" << mapToScene(viewport()->rect())
        ;//*/
    if (!isLoadingImage) {
        bool adjustCenter = true;
        bool refresh = true;
        showNormalizedViewport(adjustCenter, refresh, "ImageView::scrollChange");
    }
}

// MOUSE CONTROL

void ImageView::enterEvent(QEnterEvent *event)
{
    wheelSpinningOnEntry = G::wheelSpinning;

    /* The mask overlay is "active and visible whenever the mouse is over the imageView". */
    if (maskEditMode) {
        maskHover = true;
        if (maskTool == 2) { maskBrushCursorOn = true; setFocus(Qt::MouseFocusReason); }  // for [ ] / Cmd+Z
        viewport()->update();
    }

    if (panToFocus && zoom > zoomFit) {
        // qDebug() << "ImageView::enterEvent emit showLoupeRect";
        emit showLoupeRect(true);
    }

    /*
    qDebug() << "ImageView::enterEvent" << objectName()
             << "G::wheelSpinning =" << G::wheelSpinning
             << "wheelSpinningOnEntry =" << wheelSpinningOnEntry
        ; //*/
}

void ImageView::leaveEvent(QEvent *event)
{
    wheelSpinningOnEntry = false;
    emit cursorLeftImage();     // clear the Develop scopes readout marker
    if (maskEditMode && maskHover && !maskPainting) {   // hide overlay on leave (not mid-stroke)
        maskHover = false;
        maskDrag = -1;
        maskBrushCursorOn = false;
        viewport()->update();
    }
    // emit showLoupeRect(false);
}

void ImageView::wheelEvent(QWheelEvent *event)
{
/*
    Mouse wheel scrolling / trackpad swiping = next/previous image.
*/
    if (G::isLogger)
        qDebug() << "ImageView::wheelEvent";

    /* While cropping, swallow the wheel so it can't navigate to another image (which would leave
       the crop overlay stale -- image-switch re-init is a later increment). */
    if (cropActive()) { event->accept(); return; }

    /* Brush active over the image: a two-finger drag (or wheel) resizes the brush instead of
       changing image. Vertical delta; pixelDelta on a trackpad, angleDelta on a mouse wheel. */
    if (maskEditMode && maskTool == 2 && maskHover) {
        const QPoint pd = event->pixelDelta();
        const double dy = !pd.isNull() ? pd.y() : event->angleDelta().y() / 8.0;
        adjustBrushSize(dy * 0.15);     // up = larger
        event->accept();
        return;
    }

    static int accumDelta = 0;
    int triggerDelta = 5;

    if (wheelSpinningOnEntry && G::wheelSpinning) {
        //qDebug() << "ImageView::wheelEvent ignore because wheelSpinningOnEntry && G::wheelSpinning";
        return;
    }
    wheelSpinningOnEntry = false;

    //
    static QElapsedTimer t;

    static bool first = true;
    if (first) {
        t.start();
        first = false;
    }

    // positive delta = previous direction
    int delta = event->angleDelta().y();

    // accumulated delta
    if (accumDelta >= 0 && delta > 0) accumDelta += delta;
    else accumDelta -= delta;

    if (t.elapsed() > G::wheelSensitivity && qAbs(accumDelta) > 20) {
        if (qAbs(delta) == 0) return;
        if (delta > 0) sel->prev();
        else sel->next();
        // qDebug() << "ImageView::wheelEvent accumDelta =" << accumDelta;
        accumDelta = 0;
        t.restart();
    }

    // set spinning flag in case mouse moves to another object while still spinning
    G::wheelSpinning = true;
    // singleshot to flag when wheel has stopped spinning
    wheelTimer.start(100);
    /*
    qDebug() << "ImageView::wheelEvent"
             << "G::wheelSpinning =" << G::wheelSpinning
             << "wheelSpinningOnEntry =" << wheelSpinningOnEntry
        ; //*/
}

void ImageView::wheelStopped()
{
    G::wheelSpinning = false;
    wheelSpinningOnEntry = false;
    //qDebug() << "ImageView::wheelStopped";
}

bool ImageView::event(QEvent *event) {
/*
    Trap back/forward buttons on Logitech mouse to toggle pick status on thumbnail
*/
    /* While a brush mask is active, claim [ ] A from their global action shortcuts (thumb size /
       droplet) so the brush gets them as key presses. */
    if (event->type() == QEvent::ShortcutOverride && maskEditMode && maskTool == 2) {
        QKeyEvent *ke = static_cast<QKeyEvent *>(event);
        if (ke->modifiers() == Qt::NoModifier &&
            (ke->key() == Qt::Key_A || ke->key() == Qt::Key_BracketLeft ||
             ke->key() == Qt::Key_BracketRight)) {
            event->accept();
            return true;
        }
    }

    /* Warp: claim Enter/Return from any global action shortcut while a perspective quad is being
       traced, so the key commits the warp (see keyPressEvent) instead of its usual binding. */
    if (event->type() == QEvent::ShortcutOverride && cropActive() && cropWarp) {
        QKeyEvent *ke = static_cast<QKeyEvent *>(event);
        if (ke->key() == Qt::Key_Return || ke->key() == Qt::Key_Enter) {
            event->accept();
            return true;
        }
    }

    /* Crop: Alt/Opt toggles the "transform" rubber-band look the moment it is pressed/released (the
       mouse-move path also tracks it, for when this view does not hold keyboard focus). */
    if (cropActive() && (event->type() == QEvent::KeyPress || event->type() == QEvent::KeyRelease)) {
        if (static_cast<QKeyEvent *>(event)->key() == Qt::Key_Alt) {
            const bool held = (event->type() == QEvent::KeyPress);
            if (held != cropAltHeld) { cropAltHeld = held; viewport()->update(); }
        }
    }

    if (event->type() == QEvent::NativeGesture) {
        // qDebug() << "ImageView::event" << event << event->type() << QApplication::keyboardModifiers();
        QNativeGestureEvent *e = static_cast<QNativeGestureEvent *>(event);
        int direction = static_cast<int>(e->value());
        emit mouseSideKeyPress(direction);
        return true;
    }

    if (event->type() == QEvent::Enter) {
        if (isScrollable) setCursor(Qt::OpenHandCursor);
        else setCursor(Qt::ArrowCursor);
    }

    QGraphicsView::event(event);
    return true;
}

void ImageView::keyPressEvent(QKeyEvent *event){
    /* Commit a perspective warp with Enter/Return (the ShortcutOverride above frees the key from its
       global binding while the quad is being traced). */
    if (cropActive() && cropWarp &&
        (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter)) {
        emit warpCommitRequested();
        return;
    }
    /* Brush shortcuts: [ / ] resize, Cmd/Ctrl+Z undo the last stroke. */
    if (maskEditMode && maskTool == 2) {
        if (event->key() == Qt::Key_BracketLeft)  { adjustBrushSize(-2); return; }
        if (event->key() == Qt::Key_BracketRight) { adjustBrushSize(+2); return; }
        if (event->key() == Qt::Key_A && event->modifiers() == Qt::NoModifier) { toggleAutoMask(); return; }
        if (event->key() == Qt::Key_Z && (event->modifiers() & (Qt::ControlModifier | Qt::MetaModifier))
            && !(event->modifiers() & Qt::AltModifier)) {
            brushUndoStroke();
            return;
        }
    }
    emit keyPress(event);
}

// not used
void ImageView::mouseDoubleClickEvent(QMouseEvent *event)
{
    // if (G::isLogger)
    G::log("ImageView::mouseDoubleClickEvent", "isScrollable =" + QVariant(isScrollable).toString());
    /* Commit a perspective warp with a double-click while the quad is being traced. */
    if (cropActive() && cropWarp && event->button() == Qt::LeftButton) {
        emit warpCommitRequested();
        return;
    }
    if (event->button() == Qt::LeftButton) {
        isLeftMouseBtnPressed = true;
        mousePressPt.setX(event->x());
        mousePressPt.setY(event->y());
        QGraphicsView::mousePressEvent(event);
        return;
    }
    // isMouseDoubleClick = true;
    // return;
}

void ImageView::mousePressEvent(QMouseEvent *event)
{
    if (G::isLogger)
        G::log("ImageView::mousePressEvent", "isScrollable =" + QVariant(isScrollable).toString());
    /*
    qDebug() << "\nImageView::mousePressEvent"
             << "Button =" << event->button()
        ; //*/


    // bad things happen if no image when click
    if (currentImagePath.isEmpty()) {
        return;
    }

    /* Crop: grabbing a handle resizes the frame (static canvas) -- consume it. Alt + a corner in
       rectangle mode breaks the rectangle into a free 4-point warp quad. Anywhere else is a normal
       canvas pan that slides the image under the fixed frame, so fall through to the pan handling
       below (cropDrag stays -1; the frame is re-derived from cropN as the image moves). */
    if (cropActive() && cropLevelMode && event->button() == Qt::LeftButton) {
        cropLevelP1 = cropLevelP2 = event->pos();   // start the level line
        cropLevelDragging = true;
        isLeftMouseBtnPressed = true;
        return;
    }
    if (cropActive() && event->button() == Qt::LeftButton) {
        const int h = cropHitTest(event->pos());
        if (!cropWarp && (event->modifiers() & Qt::AltModifier) && h >= 0 && h <= 3) {
            cropEnterWarp();        // seed the quad, then drag this corner
        }
        const int maxHandle = cropWarp ? 3 : 7;
        if (h >= 0 && h <= maxHandle) {
            cropDrag = h;
            isLeftMouseBtnPressed = true;
            setCursor(Qt::SizeAllCursor);
            viewport()->update();
            return;
        }
        /* No handle hit. When the crop is still the whole frame, a drag draws a brand-new crop
           rectangle from here (saves shrinking the full-frame crop by its handles). Otherwise it is
           a normal canvas pan. */
        if (cropIsFull()) {
            cropDrag = kCropDrawNew;
            cropDrawAnchorVp = event->pos();
            isLeftMouseBtnPressed = true;
            setCursor(cropCursor);
            return;
        }
        cropDrag = -1;          // pan: handled by the normal Left-button path below
    }

    /* Brush: a left press begins a stroke (Alt = erase). Painting consumes the event so the image
       does not pan. */
    if (maskEditMode && maskTool == 2 && event->button() == Qt::LeftButton) {
        brushEnsureBuffers();
        if (maskBrushW > 0) {
            maskPainting = true;
            maskHover = true;          // painting implies hovering (gates the overlay draw)
            maskBrushErase = (event->modifiers() & Qt::AltModifier);
            maskStrokePts.clear();
            const QPointF n = maskViewportToNorm(event->pos());
            maskStrokePts << n.x() << n.y();
            std::fill(maskBrushStroke.begin(), maskBrushStroke.end(), 0.0f);
            /* Auto-mask context for this stroke (preview buffer is output space -> degrees 0). */
            maskStrokeAM = BrushStamp::AutoMaskCtx();
            if (maskBrushAutoMask) ensureAutoGuide();
            if (maskBrushAutoMask && maskGuide && maskGuide->valid()) {
                maskStrokeAM.on = true;
                maskStrokeAM.guide = maskGuide->lum.data();
                maskStrokeAM.gw = maskGuide->w; maskStrokeAM.gh = maskGuide->h;
                maskStrokeAM.degrees = 0;
                maskStrokeAM.lumRef = BrushStamp::guideLumAt(maskStrokeAM, n.x(), n.y());
            }
            maskBrushLast = brushNormToBuf(n);
            BrushStamp::dabMax(maskBrushStroke.data(), maskBrushW, maskBrushH,
                               maskBrushLast.x(), maskBrushLast.y(),
                               brushRadiusBufPx(), qBound(0.0, maskFeather, 100.0) / 100.0,
                               maskStrokeAM.on ? &maskStrokeAM : nullptr);
            brushRebuildPreview(brushSegRect(maskBrushLast, maskBrushLast));
            maskBrushCursorVp = event->pos();
            maskBrushCursorOn = true;
            viewport()->update();
        }
        return;
    }

    /* Mask edit: grab a handle and start dragging it instead of panning. A miss falls through to
       the normal pan/zoom handling below, so panning a zoomed image still works while editing. */
    if (maskEditMode && event->button() == Qt::LeftButton) {
        const int h = maskHitTest(event->pos());
        if (h >= 0) {
            maskDrag = h;
            if (maskTool == 1) {                // radial: anchor move / rotate
                const QRectF br = pmItem->boundingRect();
                const QPointF ci(maskC.x()*br.width(), maskC.y()*br.height());
                if (h == 0) {                   // centre move
                    maskMoveAnchorN = maskViewportToNorm(event->pos());
                    maskCAnchor = maskC;
                }
                else if (h == 5) {              // rotate
                    const QPointF ip = maskViewportToImage(event->pos());
                    maskGrabAngle = std::atan2(ip.y() - ci.y(), ip.x() - ci.x());
                    maskAngleAnchor = maskAngle;
                }
            }
            else if (h == 2) {                  // linear move: anchor the whole line
                maskMoveAnchorN = maskViewportToNorm(event->pos());
                maskP1Anchor = maskP1;
                maskP2Anchor = maskP2;
            }
            return;
        }
    }

    if (event->button() == Qt::LeftButton) {
        isLeftMouseBtnPressed = true;
        isMouseDrag = false;
        mousePressPt.setX(event->x());
        mousePressPt.setY(event->y());
        mouseDragPt.setX(event->x());
        mouseDragPt.setY(event->y());
        QGraphicsView::mousePressEvent(event);
        mouseMovementElapsedTimer->restart();
        return;
    }

    // prevent zooming when right click for context menu
    if (event->button() == Qt::RightButton) {
        return;
    }

    // back button
    if (event->button() == Qt::BackButton) {
        bool noModifier = event->modifiers() == Qt::NoModifier;
        qDebug() << "ImageView::mousePressEvent BackButton"
                 << "noModifier =" << noModifier
                 << event->button()
                 << event->modifiers()
            ;
        if (noModifier) {
            sel->prev();
        }
        else {
            bool autoAdvanceTemp = G::autoAdvance;
            G::autoAdvance = false;
            emit togglePick();
            sel->prev();
            G::autoAdvance = autoAdvanceTemp;
        }
        return;
    }

    // forward button
    if (event->button() == Qt::ForwardButton) {
        bool noModifier = event->modifiers() && Qt::NoModifier;
        qDebug() << "ImageView::mousePressEvent ForwardButton"
                 << "noModifier =" << noModifier
                 << event->button()
                 << event->modifiers()
            ;
        if (noModifier) {
            sel->next();
        }
        else {
            emit togglePick();  // advances if autoAdvance
            if (!G::autoAdvance) sel->next();
        }
        return;
    }
}

void ImageView::mouseMoveEvent(QMouseEvent *event)
{
/*
    Pan the image during a mouse drag operation.
    Set a delay to hide cursor if in slideshow mode.
*/
    /*if (isRubberBand) {
        rubberBand->setGeometry(QRect(origin, event->pos()).normalized());
        return;
    }
    //*/

    static QPoint prevPos = event->pos();

    /* Brush: paint a stroke (left button held), or just track the size cursor on hover. */
    if (maskEditMode && maskTool == 2) {
        maskBrushCursorVp = event->pos();
        maskBrushCursorOn = true;
        if (maskPainting) {
            const QPointF n = maskViewportToNorm(event->pos());
            maskStrokePts << n.x() << n.y();
            const QPointF bp = brushNormToBuf(n);
            const QRect dirty = brushSegRect(maskBrushLast, bp);
            brushStampTo(bp);
            brushRebuildPreview(dirty);     // only the new segment's bbox
        }
        viewport()->update();
        if (maskPainting) return;          // consume while painting; hover falls through to scopes
    }

    /* Crop interaction. A handle drag resizes the frame over the STATIC image (consumed here).
       Otherwise it is a normal canvas pan -- fall through so the image scrolls under the fixed
       frame; cropN is then recomputed from the (unchanged) frame after the pan, below. */
    if (cropActive() && cropLevelMode) {               // drawing / hovering the level line
        if (cropLevelDragging) { cropLevelP2 = event->pos(); viewport()->update(); }
        else setCursor(levelCursor);
        return;
    }
    if (cropActive()) {
        const bool alt = (event->modifiers() & Qt::AltModifier);
        if (alt != cropAltHeld) { cropAltHeld = alt; viewport()->update(); }
        if (cropDrag == kCropDrawNew) {                // rubber-band a brand-new crop from the anchor
            cropDrawNewFrom(event->pos());
            viewport()->update();
            return;
        }
        if (cropDrag >= 0 && cropDrag <= 7) {
            cropResizeFromHandle(event->pos());
            viewport()->update();
            return;
        }
        if (!isLeftMouseBtnPressed) {                  // hover: pick the cursor
            const int h = cropHitTest(event->pos());
            if (h >= 0 && h <= 7)      setCursor(Qt::SizeAllCursor);   // over a handle: resize
            else if (cropIsFull())     setCursor(cropCursor);          // whole frame: draw a new crop
            else if (h == 8)           setCursor(Qt::OpenHandCursor);  // inside a sub-crop: pan
            else                       setCursor(Qt::ArrowCursor);
            return;
        }
        /* else: panning -- let the normal scrollbar pan below run, then recompute cropN. */
    }

    /* Mask edit drag. Linear: endpoint rotate+scale about centre, or translate. Radial: centre
       move, axis-handle resize, or outline rotate. */
    if (maskEditMode && maskDrag >= 0) {
        if (maskTool == 1) {                                // ---- Radial ----
            const QRectF br = pmItem->boundingRect();
            const double bw = br.width(), bh = br.height();
            const QPointF ci(maskC.x()*bw, maskC.y()*bh);   // current centre, image px
            const QPointF ip = maskViewportToImage(event->pos());
            if (maskDrag == 0) {                            // move
                const QPointF d = maskViewportToNorm(event->pos()) - maskMoveAnchorN;
                maskC = maskCAnchor + d;
            }
            else if (maskDrag == 5) {                       // rotate about centre
                const double cur = std::atan2(ip.y() - ci.y(), ip.x() - ci.x());
                maskAngle = maskAngleAnchor + (cur - maskGrabAngle) * 57.29577951308232;
            }
            else {                                          // resize a semi-axis (centre fixed)
                const double a = maskAngle * 0.017453292519943295;
                const double ca = std::cos(a), sa = std::sin(a);
                const double ddx = ip.x() - ci.x(), ddy = ip.y() - ci.y();
                const double lx =  ddx*ca + ddy*sa, ly = -ddx*sa + ddy*ca;   // un-rotated local px
                if (maskDrag == 1 || maskDrag == 2) maskRx = qMax(2.0, std::abs(lx)) / bw;
                else                                maskRy = qMax(2.0, std::abs(ly)) / bh;
            }
        }
        else {                                              // ---- Linear ----
            const QPointF n = maskViewportToNorm(event->pos());
            if (maskDrag == 0 || maskDrag == 1) {
                const QPointF c = (maskP1 + maskP2) / 2.0;  // centre stays fixed -> rotate+scale
                if (maskDrag == 0) { maskP1 = n;          maskP2 = 2.0*c - n; }
                else               { maskP2 = n;          maskP1 = 2.0*c - n; }
            }
            else {                                          // translate
                const QPointF d = n - maskMoveAnchorN;
                maskP1 = maskP1Anchor + d;
                maskP2 = maskP2Anchor + d;
            }
        }
        viewport()->update();
        /* Live re-composite as the handle moves. setActiveMaskParams -> paramsChanged ->
           developParamsChange coalesces the proxy render to one per event-loop turn and debounces
           the full-res settle, so emitting every move is safe (same as a slider drag). */
        emit maskGeometryChanged(maskParamsJson());
        return;
    }
    /* Hover feedback while a mask tool is active (not dragging): show a move cursor over a handle. */
    if (maskEditMode && !isLeftMouseBtnPressed) {
        setCursor(maskHitTest(event->pos()) >= 0 ? Qt::OpenHandCursor : Qt::ArrowCursor);
    }

    /*
    if (isLeftMouseBtnPressed)
    qDebug() << "ImageView::mouseMoveEvent"
             << "isFit =" << isFit
             << "zoom =" << zoom
             << "zoomFit =" << zoomFit
             << "isScrollable =" << isScrollable
             << "isLeftMouseBtnPressed =" << isLeftMouseBtnPressed
             << "event->button() =" << event->button()
             << "zoomToggleOnRelease =" << zoomToggleOnRelease
             << "isMouseDrag =" << isMouseDrag
             //<< "prevPos =" << prevPos
             << "event->pos() =" << event->pos()
        ; //*/

    if (isLeftMouseBtnPressed) {
        if (G::isSlideShow) emit killSlideshow();
        /* While cropping the scene is enlarged so the canvas can be dragged under the fixed frame
           even at fit zoom (otherwise isScrollable would be false and the drag would do nothing). */
        if (isScrollable || cropActive()) {
            setCursor(Qt::ClosedHandCursor);
            int oldHSB = horizontalScrollBar()->value();
            int oldVSB = verticalScrollBar()->value();
            horizontalScrollBar()->setValue(horizontalScrollBar()->value() -
                                           (event->x() - mouseDragPt.x()));
            verticalScrollBar()->setValue(verticalScrollBar()->value() -
                                          (event->y() - mouseDragPt.y()));
            int hDelta = horizontalScrollBar()->value() - oldHSB;
            int vDelta = verticalScrollBar()->value() - oldVSB;
            if (!isMouseDrag) {
                if (qAbs(hDelta) > 0) isMouseDrag = true;
                if (qAbs(vDelta) > 0) isMouseDrag = true;
            }
            /*
            qDebug() << "ImageView::mouseMoveEvent"
                     << "hDelta =" << hDelta
                     << "vDelta =" << vDelta
                     << "isMouseDrag =" << isMouseDrag
                ; //*/
            mouseDragPt.setX(event->x());
            mouseDragPt.setY(event->y());

            /* Crop pan: the frame is fixed on screen and the image scrolls under it. Clamp the
               scroll so the image always covers the frame (you can't pan the crop off the image),
               then recompute cropN from the unchanged frame. */
            if (cropActive() && cropDrag < 0) {
                const QRectF imgVp =
                    mapFromScene(pmItem->mapToScene(pmItem->boundingRect())).boundingRect();
                const QRectF fb = cropFrameBBoxVp();   // frame, or the quad's bounding box
                qreal dx = 0, dy = 0;   // viewport px to move the image to re-cover the frame
                if      (imgVp.left()   > fb.left())   dx = fb.left()   - imgVp.left();
                else if (imgVp.right()  < fb.right())  dx = fb.right()  - imgVp.right();
                if      (imgVp.top()    > fb.top())    dy = fb.top()    - imgVp.top();
                else if (imgVp.bottom() < fb.bottom()) dy = fb.bottom() - imgVp.bottom();
                if (dx != 0) horizontalScrollBar()->setValue(
                                 horizontalScrollBar()->value() - qRound(dx));
                if (dy != 0) verticalScrollBar()->setValue(
                                 verticalScrollBar()->value() - qRound(dy));
                /* The frame/quad is fixed on screen; recompute the crop from it as the image moved. */
                if (cropWarp) for (int i = 0; i < 4; ++i) cropQuadN[i] = cropVpToN(cropQuadVp[i]);
                else          cropN = cropVpRectToN(cropFrameVp);
                cropEmitChanged();
            }
        }
    }
    else {
        if (G::isSlideShow) {
            if (event->pos() != prevPos) {
                setCursor(Qt::ArrowCursor);
                QTimer::singleShot(500, this, SLOT(hideCursor()));
            }
        }
    }

    /* Develop scopes readout: map the cursor to the displayed pixmap and emit its normalized
       position (or "left the image" when outside). One pixmap-coordinate transform; the actual
       pixel sample + scope overlay happen on the receiving side (MW::onImageCursorPos). */
    if (pmItem->isVisible()) {
        const QPointF itemPt = pmItem->mapFromScene(mapToScene(event->pos()));
        const QRectF br = pmItem->boundingRect();
        if (br.width() > 0 && br.height() > 0 && br.contains(itemPt))
            emit cursorImagePos(itemPt.x() / br.width(), itemPt.y() / br.height());
        else
            emit cursorLeftImage();
    }

    prevPos = event->pos();
    event->ignore();

//    QGraphicsView::mouseMoveEvent(event);
}

void ImageView::mouseReleaseEvent(QMouseEvent *event)
{
    if (G::isLogger)
        G::log("ImageView::mouseReleaseEvent", "isScrollable =" + QVariant(isScrollable).toString());

    /* Finish a level line: reduce its tilt to the nearest horizontal/vertical and emit the leveling
       angle (a delta added to the straighten). A too-short line is ignored. */
    if (cropActive() && cropLevelMode) {
        cropLevelMode = false;
        isLeftMouseBtnPressed = false;
        const bool wasDragging = cropLevelDragging;
        cropLevelDragging = false;
        setCursor(Qt::ArrowCursor);
        viewport()->update();
        const QPointF d = cropLevelP2 - cropLevelP1;
        if (wasDragging && QLineF(cropLevelP1, cropLevelP2).length() >= 12.0) {
            double ang = std::atan2(d.y(), d.x()) * 57.29577951308232;   // screen degrees
            double m = std::fmod(ang, 90.0);
            if (m >  45.0) m -= 90.0;
            if (m < -45.0) m += 90.0;
            emit levelAngleChanged(-m);   // rotate to cancel the tilt (sign verified in-app)
        }
        return;
    }

    /* End a crop gesture. The view transform is never touched, so there is no re-stage: just clear
       the drag, release any grab the pan path took, and leave the image where the user left it
       (no zoom-toggle-on-release). */
    if (cropActive() && isLeftMouseBtnPressed) {
        cropDrag = -1;
        isLeftMouseBtnPressed = false;
        QGraphicsView::mouseReleaseEvent(event);     // release the pan grab, if any
        setCursor(Qt::ArrowCursor);
        viewport()->update();
        cropEmitChanged();
        return;
    }

    /* Finish a brush stroke: composite it into the committed mask, append it to the stroke list,
       and persist the updated paramsJson (which triggers the masked re-render in stage 3). */
    if (maskEditMode && maskTool == 2 && maskPainting) {
        maskPainting = false;
        isLeftMouseBtnPressed = false;
        const double flow = qBound(0.0, maskBrushFlow, 100.0) / 100.0;
        BrushStamp::composite(maskBrushMain.data(), maskBrushStroke.data(),
                              size_t(maskBrushW) * maskBrushH, flow, maskBrushErase);
        std::fill(maskBrushStroke.begin(), maskBrushStroke.end(), 0.0f);
        if (maskStrokePts.size() >= 2) {
            QJsonArray pts;
            for (double v : maskStrokePts) pts.append(v);
            QJsonObject stroke;
            stroke["pts"]      = pts;
            stroke["size"]     = maskBrushSize;
            stroke["feather"]  = maskFeather;
            stroke["flow"]     = maskBrushFlow;
            stroke["erase"]    = maskBrushErase;
            stroke["autoMask"] = maskBrushAutoMask;
            maskBrushStrokesJson.append(stroke);
            QJsonObject o;
            o["size"] = maskBrushSize; o["flow"] = maskBrushFlow; o["autoMask"] = maskBrushAutoMask;
            o["strokes"] = maskBrushStrokesJson;
            emit maskGeometryChanged(QString::fromUtf8(QJsonDocument(o).toJson(QJsonDocument::Compact)));
        }
        brushRebuildPreview();
        viewport()->update();
        return;
    }

    /* Finish a mask-handle drag: persist the new geometry (dock writes it into the component). */
    if (maskEditMode && maskDrag >= 0) {
        maskDrag = -1;
        isLeftMouseBtnPressed = false;
        emit maskGeometryChanged(maskParamsJson());
        return;
    }

    /* rubberband
    if (isRubberBand) {
        setCursor(Qt::ArrowCursor);
        QRect rb = rubberBand->geometry();
        rubberBand->hide();
        QPoint p0 = mapToScene(rb.topLeft()).toPoint();
        QPoint p1 = mapToScene(rb.bottomRight()).toPoint();
        QRect r(p0, p1);
        QPixmap pm = pmItem->pixmap().copy(r);
        isRubberBand = false;
        QPixmap tile;
        PatternDlg *patternDlg = new PatternDlg(this, pm, tile, tileName);
        patternDlg->exec();
        QBuffer buffer(&tileBa);
        buffer.open(QIODevice::WriteOnly);
        tile.save(&buffer, "PNG");
        qDebug() << "ImageView::mouseReleaseEvent" << "new tile";
        // emit newTile();
        return;
    }
//    */

    // prevent zooming when right click for context menu
    if (event->button() == Qt::RightButton) {
        return;
    }

    // prevent zooming when forward and back buttons
    if (event->button() == Qt::BackButton || event->button() == Qt::ForwardButton) return;

    isLeftMouseBtnPressed = false;

    // prevent zooming if release is outside the ImageView area
    /*
    qDebug() << "ImageView::mouseReleaseEvent"
             << "viewport()->rect() =" << viewport()->rect()
             << "event->pos() =" << event->pos()
             << "viewport()->rect().contains(event->pos()) =" << viewport()->rect().contains(event->pos())
        ; //*/
    if (!viewport()->rect().contains(event->pos())) return;

    // zoom > zoomFit (set in scale)
    if (isScrollable) setCursor(Qt::OpenHandCursor);
    else setCursor(Qt::ArrowCursor);

    // Amount of movement px
    int px = qMax(qAbs(mousePressPt.x() - event->x()), qAbs(mousePressPt.y() - event->y()));

    // Delay between mouse press and release ms
    int ms = mouseMovementElapsedTimer->elapsed();

    if (isMouseDrag && px < 10 && ms < 200) isMouseDrag = false;

    /*
    qDebug() << "ImageView::mouseReleaseEvent"
             << "px =" << px
             << "ms =" << ms
             << "isMouseDrag =" << isMouseDrag
        ;
    //*/

    bool isZoomToggle = event->modifiers() == Qt::NoModifier;

    if (!isMouseDrag && isZoomToggle) {
        // prevent pan to predictive focus when click on ImageView
        isLocalMouseClick = true;
        zoomToggle();
    }

    QGraphicsView::mouseReleaseEvent(event);
}

// DRAG AND DROP

void ImageView::dragEnterEvent(QDragEnterEvent *event)
{
/*
    Empty function required to propagate drop event (not sure why)
*/
    if (G::isLogger) G::log("ImageView::dragEnterEvent");
    //qDebug() << "ImageView::dragEnterEvent";
}

QString ImageView::diagnostics()
{
    if (G::isLogger) G::log("ImageView::diagnostics");
    QString reportString;
    QTextStream rpt;
    rpt.setString(&reportString);
    rpt << Utilities::centeredRptHdr('=', "ImageView Diagnostics");
    rpt << "\n";
    rpt << "\n" << "pmItem->pixmap().width() = " << pmItem->pixmap().width();
    rpt << "\n" << "pmItem->pixmap().height() = " << pmItem->pixmap().height();
    rpt << "\n" << "isBusy = " << G::s(isBusy);
    rpt << "\n" << "shootingInfo = " << G::s(infoText);
    rpt << "\n" << "infoOverlayFontSize = " << G::s(infoOverlayFontSize);
    rpt << "\n" << "currentImagePath = " << G::s(currentImagePath);
    rpt << "\n" << "firstImageLoaded = " << G::s(G::isFirstImageNewInstance);
    rpt << "\n" << "classificationBadgeDiam = " << G::s(classificationBadgeDiam);
    rpt << "\n" << "cursorIsHidden = " << G::s(cursorIsHidden);
    rpt << "\n" << "moveImageLocked = " << G::s(moveImageLocked);
    rpt << "\n" << "isZoom = " << G::s(isScrollable);
    rpt << "\n" << "isFit = " << G::s(isFit);
    rpt << "\n" << "isMouseDrag = " << G::s(isMouseDrag);
    rpt << "\n" << "isTrackpadScroll = " << G::s(isTrackpadScroll);
    rpt << "\n" << "isLeftMouseBtnPressed = " << G::s(isLeftMouseBtnPressed);
    rpt << "\n" << "isMouseDoubleClick = " << G::s(isMouseDoubleClick);
    rpt << "\n" << "scrollCount = " << G::s(scrollCount);
    rpt << "\n" << "zoomFit = " << G::s(zoomFit);
    rpt << "\n" << "zoomInc = " << G::s(zoomInc);
    rpt << "\n" << "zoomMin = " << G::s(zoomMin);
    rpt << "\n" << "scrl.hVal = " << G::s(scrl.hVal);
    rpt << "\n" << "scrl.hMax = " << G::s(scrl.hMax);
    rpt << "\n" << "scrl.hMin = " << G::s(scrl.hMin);
    rpt << "\n" << "scrl.hPct = " << G::s(scrl.hPct);
    rpt << "\n" << "scrl.vVal = " << G::s(scrl.vVal);
    rpt << "\n" << "scrl.vMax = " << G::s(scrl.vMax);
    rpt << "\n" << "scrl.vMin = " << G::s(scrl.vMin);
    rpt << "\n" << "scrl.vPct = " << G::s(scrl.vPct);
    rpt << "\n" << "scrollPct = " << G::s(scrollPct.x()) << "," << G::s(scrollPct.y());
    rpt << "\n" << "mousePressPt = " << G::s(mousePressPt.x()) << "," << G::s(mousePressPt.y());
    rpt << "\n" << "preview = " << G::s(preview.width()) << "," << G::s(preview.height());
    rpt << "\n" << "full = " << G::s(full.width()) << "," << G::s(full.height());

    rpt << "\n\n" ;
    return reportString;
}

void ImageView::exportImage()
{
    if (G::isLogger) G::log("ImageView::exportImage");
    QImage image = grab().toImage();
}

// COPY AND PASTE

void ImageView::copyImage()
{
    if (G::isLogger) G::log("ImageView::copyImage");
    qDebug() << "ImageView::copyImage";
    QPixmap pm = pmItem->pixmap();
    if (pm.isNull()) {
       if (icd->contains(dm->currentFilePath)) {
            pm = QPixmap::fromImage(icd->imCache.value(dm->currentFilePath));
        }
        else {
            QString msg = "Could not copy the current image to the clipboard";
            G::popup->showPopup(msg, 1500);
        }
    }
    QApplication::clipboard()->setPixmap(pm, QClipboard::Clipboard);
    QString msg = "Copied current image to the clipboard";
    G::popup->showPopup(msg, 1500);
}
