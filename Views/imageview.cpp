#include <QGraphicsDropShadowEffect>
#include <math.h>
#include "Main/global.h"
#include "Views/imageview.h"
#include <QApplication>

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

    /*
    // set QOpenGLWidget as QGraphicsView viewport
    openGLFrame = new OpenGLFrame;
    QSurfaceFormat format;
    format.setSamples(4);
    format.setDepthBufferSize(24);
    format.setStencilBufferSize(8);
    format.setVersion(3, 2);
    format.setProfile(QSurfaceFormat::CoreProfile);
    openGLFrame->setFormat(format);
    this->setViewport(openGLFrame);
    this->setViewportUpdateMode(QGraphicsView::FullViewportUpdate);

    // set scene so can override drawForeground
    scene = new ImageScene(openGLFrame);
    */

    scene = new QGraphicsScene();
    scene->setObjectName("Scene");

    pmItem = new QGraphicsPixmapItem;
    pmItem->setBoundingRegionGranularity(1);
    pmItem->setOpacity(1.0);
    scene->addItem(pmItem);

    setAcceptDrops(true);
    pmItem->setAcceptDrops(true);
    setAttribute(Qt::WA_TranslucentBackground);
    /*
    setOptimizationFlags(QGraphicsView::DontSavePainterState);
    setViewportUpdateMode(QGraphicsView::SmartViewportUpdate);
    setViewportUpdateMode(QGraphicsView::FullViewportUpdate);
    setTransformationAnchor(QGraphicsView::AnchorViewCenter);
    setAlignment(Qt::AlignCenter);
    setDragMode(QGraphicsView::ScrollHandDrag);
    */
    setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
    setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setScene(scene);

    cursorIsHidden = false;
    moveImageLocked = false;
    infoOverlay = new DropShadowLabel(this);
    infoOverlay->setStyleSheet("font-size: " + QString::number(infoOverlayFontSize) + "pt;");
    infoOverlay->setVisible(isShootingInfoVisible);
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
    isFirstImageNewInstance = true;
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

    bool isDebug = false;
    bool isCurrent = (fPath == currentImagePath);

    if (isDebug)
    {
        qDebug() << "\nImageView::loadImage:"
         << "isFirstImageNewInstance =" << isFirstImageNewInstance
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
        G::log("ImageView::loadImage", row + " Src:" + src +
               " isFirstImageNewInstance = " + QVariant(isFirstImageNewInstance).toString() +
               " " + fPath);
    }

    // ignore if result of remote operation
    if (G::isRemote) return false;

    // No folder selected yet
    if (!fPath.length()) {
        return false;
    }

    // Already displayed
    if (isCurrent && replace == false) {
        return true;
    }

    // do not load image if triggered by embellish remote export
    if (G::isProcessingExportedImages) {
        // qDebug() << "WARNING" << "ImageView::loadImage" << "Processing exported images";
        QString msg = "Processing exported images.  Canceled loadImage.";
        G::issue("Warning", msg, "ImageView::loadImage");
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
        if (dmRow == -1) return false;
        if (!dm->index(dmRow, G::MetadataLoadedColumn).data().toBool()) {
            QFileInfo fileInfo(fPath);
            if (metadata->loadImageMetadata(fileInfo, dmRow, dm->instance, true, true, false, true, "ImageView::loadImage")) {
                // metadata->m.row = dmRow;
                // metadata->m.instance = dm->instance;
                dm->addMetadataForItem(metadata->m, "ImageView::loadImage"); // rgh investigate warning (QVariant issue probably)
            }
        }

        QPixmap displayPixmap;
        isLoaded = pixmap->load(fPath, displayPixmap, "ImageView::loadImage");

        if (isLoaded) {
            pmItem->setPixmap(displayPixmap);
            isBusy = false;
        }
        else {
            // set null pixmap
            QPixmap nullPm;
            pmItem->setPixmap(nullPm);
            return false;
        }
    }

    /* Get cached image. Must check if image has been cached before calling
    icd->imCache.find(fPath, image) to prevent a mismatch between the fPath index and the
    image in icd->imCache hash table. Also must check in case where an ejected drive has
    resulted in clearing icd->cacheItemList. */

    int sfRow = dm->proxyRowFromPath(fPath, "ImageView::loadImage");
    if (sfRow == -1 || sfRow >= dm->sf->rowCount()) return false;

    if (icd->contains(fPath)) {
        QImage image; // confirm the cached image is in the image cache
        if (isDebug)
            qDebug() << "ImageView::loadImage  row =" << sfRow << fPath;

        pmItem->setPixmap(QPixmap::fromImage(icd->imCache.value(fPath)));
        isLoaded = true;
        if (isDebug)
            qDebug() << "ImageView::loadImage"
                     << "w =" << pmItem->pixmap().width()
                     << "h =" << pmItem->pixmap().height()
                     << "isNull =" << pmItem->pixmap().isNull()
                     << fPath;
    }
    else {
        if (isDebug)
            qDebug() << "ImageView::loadImage isCached = false";
    }

    /* When the program is opening or resizing it is possible this function could be called
    before the central widget has been fully defined, and has a small default size. If that is
    the case, ignore, as the function will be called again. Also ignore if the image failed to
    be loaded into the graphics scene. */
    /*
    qDebug() << "ImageView::loadImage:"
             << "isFirstImageNewInstance =" << isFirstImageNewInstance
             << "row =" << sfRow
             << "isLoaded =" << isLoaded
             << "rect().height() > 50 =" << QVariant(rect().height() > 50).toString();
    //*/
    if (isLoaded) {
        /* important to keep currentImagePath. It is used to check if there isn't an image (when
        currentImagePath.isEmpty() == true) - for example when no folder has been chosen or the
        same image is being reloaded. It is also used by Embellish.  */
        currentImagePath = fPath;
        pmItem->setVisible(true);
        pmItem->setVisible(true);
        // prevent the viewport scrolling outside the image
        setSceneRect(scene->itemsBoundingRect());

        updateShootingInfo();

        /* If this is the first image in a new folder, and the image is smaller than the
        canvas (central widget window) set the scale to fit window, do not scale the
        image beyond 100% to fit the window.  */
        zoomFit = getFitScaleFactor(rect(), pmItem->boundingRect());
        if (isFirstImageNewInstance) {
            isFit = true;
            isFirstImageNewInstance = false;
        }
        if (isFit) {
            setFitZoom();
        }
        if (isDebug) {
        qDebug() << "ImageView::loadImage:"
                 << "isFirstImageNewInstance =" << isFirstImageNewInstance
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

        // focus prediction
        if (panToFocus) {
            predictPanToFocus();
        }
    }

    isBusy = false;

    if (isLoaded) return true;
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
        return false;
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
    /*
    qDebug() << "ImageView::scale"
             << "isScrollable =" << isScrollable
             << "isFit =" << isFit
             << "zoom =" << zoom
             << "highDpiZoom =" << zoom / G::actDevicePixelRatio
             << "zoomFit =" << zoomFit
             << "rect().width() =" << rect().width()
             << "sceneRect().width() =" << sceneRect().width();
    //  */
    transform.reset();
    if (G::isSlideShow) {
        setFitZoom();
    }

    if (isFit) setFitZoom();
    double highDpiZoom = zoom / G::actDevicePixelRatio;
    transform.scale(highDpiZoom, highDpiZoom);
    // when resize before first image zoom == inf
    if (zoom > 10) return;
    setTransform(transform);
    emit zoomChange(zoom, "ImageView::scale");

    isScrollable = (zoom > zoomFit);
    if (isScrollable) scrollPct = getScrollPct();
    if (panToFocus) emit showLoupeRect(isScrollable);

    // pan to predicted focus point of interest
    int i = dm->currentSfRow;
    float x = dm->sf->index(i, G::FocusXColumn).data().toFloat();
    float y = dm->sf->index(i, G::FocusYColumn).data().toFloat();
    /*
        qDebug() << "ImageView::scale"
                 << "x =" << x
                 << "y =" << y;//*/
    bool isFocus = (x >= 0 && y >= 0);
    // placeTarget(x, y);
    if (isFocus && isScrollable && panToFocus && isNewImage) {
        panTo(x, y);
    }

    if (!G::isSlideShow) {
        if (isScrollable) setCursor(Qt::OpenHandCursor);
        else {
            if (isFit) setCursor(Qt::ArrowCursor);
            else setCursor(Qt::PointingHandCursor);
        }
    }

    placeClassificationBadge();
    setShootingInfo(infoText);
    emit updateStatus(true, "", "ImageView::scale");

    isMouseDoubleClick = false;

    /* debug
    qDebug() << "ImageView::scale"
             << "isScrollable =" << isScrollable
             << "isFit =" << isFit
             << "zoom =" << zoom
             << "zoomFit =" << zoomFit
             << "rect().width() =" << rect().width()
             << "sceneRect().width() =" << sceneRect().width();
//    */
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
    size.
*/
    if (G::isLogger) G::log("ImageView::placeClassificationBadge");
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
    /*
    qDebug() << "ImageView::resizeEvent"
             << "G::isInitializing =" << G::isInitializing
             << "G::isLinearLoadDone =" << G::isLinearLoadDone
             << "isFirstImageNewInstance =" << isFirstImageNewInstance;
    //    */
    if (G::isInitializing) return;
    QGraphicsView::resizeEvent(event);
    zoomFit = getFitScaleFactor(rect(), scene->itemsBoundingRect());
//    zoomFit = getFitScaleFactor(rect(), pmItem->boundingRect());
    if (isFit) {
        setFitZoom();
        scale();
    }
    placeClassificationBadge();
    setShootingInfo(infoText);
    showPredictedFocus();
}

void ImageView::showPredictedFocus()
{
/*
    Documentation: see FOCUS PREDICTOR at top of mainwindow.cpp
*/
    if (!panToFocus || zoom <= zoomFit) return;

    // generate normalized coordinates for viewport in scene
    qreal w = pmItem->pixmap().width();
    qreal h = pmItem->pixmap().height();
    // qreal w = scene->width();
    // qreal h = scene->height();
    QPolygonF p = mapToScene(viewport()->rect());
    qreal x1 = p.at(0).x() / w;
    qreal y1 = p.at(0).y() / h;
    qreal x2 = p.at(2).x() / w;
    qreal y2 = p.at(2).y() / h;
    QRectF vp = QRectF(QPointF(x1, y1), QPointF(x2, y2));
    qreal imA = w * 1.0 / h;
    // qDebug() << "ImageView::scrollChange  imA =" << imA;
    emit loupeRect(vp, imA);
}

void ImageView::predictPanToFocus()
{
/*
    Documentation: see FOCUS PREDICTOR at top of mainwindow.cpp
*/
    if (G::isLogger) G::log("ImageView::focusPrediction");

    focusPrediction = focusPredictor->predict(pmItem->pixmap().toImage());
    panTo(focusPrediction.x(), focusPrediction.y());
    /*
    qDebug() << "ImageView::getFocusPrediction"
             << "Predicted (normalized):" << focusPrediction
             << "x =" << focusPrediction.x()
             << "y =" << focusPrediction.y()
        ; //*/
}

void ImageView::panTo(float xPct, float yPct)
{
/*
   When the image is zoomed and a thumbnail is mouse clicked the position within the thumb is
   passed here as the percent of the width and height. The zoom amount is maintained and the
   main image is panned to the same location as on the thumb. This makes it quick to check
   eyes and other details in many images.
*/
    if (G::isLogger) G::log("ImageView::thumbClick");
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

    showPredictedFocus();
    /*
    qDebug() << "ImageView::scrollChange"
             << "mapToScene(viewport()->rect()) =" << mapToScene(viewport()->rect())
             << x1 << y1 << x2 << y2
        ;//*/
}

// MOUSE CONTROL

void ImageView::enterEvent(QEnterEvent *event)
{
    wheelSpinningOnEntry = G::wheelSpinning;

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
    emit showLoupeRect(false);
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

    prevPos = event->pos();
    event->ignore();

//    QGraphicsView::mouseMoveEvent(event);
}

void ImageView::mouseReleaseEvent(QMouseEvent *event)
{
    if (G::isLogger)
        G::log("ImageView::mouseReleaseEvent", "isScrollable =" + QVariant(isScrollable).toString());
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
    rpt << "\n" << "isBusy = " << G::s(isBusy);
    rpt << "\n" << "shootingInfo = " << G::s(infoText);
    rpt << "\n" << "infoOverlayFontSize = " << G::s(infoOverlayFontSize);
    rpt << "\n" << "currentImagePath = " << G::s(currentImagePath);
    rpt << "\n" << "firstImageLoaded = " << G::s(isFirstImageNewInstance);
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

// not being used, but maybe in the future
//static inline int bound0To255(int val)
//{
//    if (G::isLogger) G::log("ImageView::copyImage");
//    return ((val > 255) ? 255 : (val < 0) ? 0 : val);
//}

static inline int hslValue(double n1, double n2, double hue)
{
    if (G::isLogger) G::log("ImageView  hslValue");
    double value;

    if (hue > 255) {
        hue -= 255;
    } else if (hue < 0) {
        hue += 255;
    }

    if (hue < 42.5) {
        value = n1 + (n2 - n1) * (hue / 42.5);
    } else if (hue < 127.5) {
        value = n2;
    } else if (hue < 170) {
        value = n1 + (n2 - n1) * ((170 - hue) / 42.5);
    } else {
        value = n1;
    }

    return ROUND(value * 255.0);
}

void rgbToHsl(int r, int g, int b, unsigned char *hue, unsigned char *sat, unsigned char *light)
{
    if (G::isLogger) G::log("ImageView  rgbToHsl");
    double h, s, l;
    int		min, max;
    int		delta;

    if (r > g) {
        max = MAX(r, b);
        min = MIN(g, b);
    } else {
        max = MAX(g, b);
        min = MIN(r, b);
    }

    l = (max + min) / 2.0;

    if (max == min) {
        s = 0.0;
        h = 0.0;
    } else {
        delta = (max - min);

        if (l < 128) {
            s = 255 * (double) delta / (double) (max + min);
        } else {
            s = 255 * (double) delta / (double) (511 - max - min);
        }

        if (r == max) {
            h = (g - b) / (double) delta;
        } else if (g == max) {
            h = 2 + (b - r) / (double) delta;
        } else {
            h = 4 + (r - g) / (double) delta;
        }

        h = h * 42.5;
        if (h < 0) {
            h += 255;
        } else if (h > 255) {
            h -= 255;
        }
    }

    *hue = ROUND(h);
    *sat = ROUND(s);
    *light = ROUND(l);
}

void hslToRgb(double h, double s, double l,
                    unsigned char *red, unsigned char *green, unsigned char *blue)
{
    if (G::isLogger) G::log("ImageView  hslToRgb");
    if (s == 0) {
        /* achromatic case */
        *red = l;
        *green = l;
        *blue = l;
    } else {
        double m1, m2;

        if (l < 128)
            m2 = (l * (255 + s)) / 65025.0;
        else
            m2 = (l + s - (l * s) / 255.0) / 255.0;

        m1 = (l / 127.5) - m2;

        /* chromatic case */
        *red = hslValue(m1, m2, h + 85);
        *green = hslValue(m1, m2, h);
        *blue = hslValue(m1, m2, h - 85);
    }
}
