#include <QGraphicsDropShadowEffect>
#include <math.h>
#include <cmath>
#include "Main/global.h"
#include "Views/imageview.h"
#include <QApplication>
#include <QJsonDocument>
#include <QJsonObject>

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
    pmItem->setPixmap(QPixmap::fromImage(image));
    pmItem->setVisible(true);
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
        else               { maskP1 = QPointF(0.5, 0.34); maskP2 = QPointF(0.5, 0.66); }
    }
    maskEditMode = true;
    maskHover = underMouse();        // show at once if the cursor is already over the view
    maskDrag = -1;
    viewport()->update();
}

void ImageView::endMaskEdit()
{
    if (!maskEditMode) return;
    if (G::isLogger) G::log("ImageView::endMaskEdit");
    maskEditMode = false;
    maskDrag = -1;
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
    if (!maskHandlesEditable()) return;
    const QRectF br = pmItem->boundingRect();
    if (br.width() <= 0 || br.height() <= 0) return;
    if      (maskTool == 1) drawRadialMask(painter, br);
    else if (maskTool == 0) drawLinearMask(painter, br);
    /* Brush (2) overlay lands in Stage 2. */
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
    if (maskEditMode) { maskHover = true; viewport()->update(); }

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
    if (maskEditMode && maskHover) {        // hide the mask overlay when the cursor leaves
        maskHover = false;
        maskDrag = -1;
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
    emit keyPress(event);
}

// not used
void ImageView::mouseDoubleClickEvent(QMouseEvent *event)
{
    // if (G::isLogger)
    G::log("ImageView::mouseDoubleClickEvent", "isScrollable =" + QVariant(isScrollable).toString());
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
        if (isScrollable) {
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
