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
                     IconView *thumbView,
                     InfoString *infoString,
                     bool isShootingInfoVisible,
                     bool isRatingBadgeVisible,
                     int classificationBadgeDiam,
                     int infoOverlayFontSize):

                     QGraphicsView(centralWidget)
{
    if (G::isLogger) G::log(CLASSFUNCTION);

    this->mainWindow = parent;
//    this->centralWidget = centralWidget;
    this->metadata = metadata;
    this->dm = dm;
    this->icd = icd;
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

    // rgh is this needed or holdover from prev program
    mouseMovementTimer = new QTimer(this);
//    connect(mouseMovementTimer, SIGNAL(timeout()), this, SLOT(monitorCursorState()));

//    mouseZoomFit = true;
    isMouseDrag = false;
    isLeftMouseBtnPressed = false;
    isMouseDoubleClick = false;
    isFirstImageNewFolder = true;
}

bool ImageView::loadImage(QString fPath, QString src)
{
/*
    There are two sources for the image (pixmap): a file or the cache.

    The first choice is the image cache. In the cache two versions of the image are stored:
    the full scale and a preview scaled to dimensions defined in the preferences. The preview
    is used if it is large enough to fit in the window (viewport) without scaling larger than
    1. If the preview is too small then the full size is used.

    If the image has not been cached (usually the case for the first image to be shown, as the
    caching is just starting) then the full size image is read from the file.

    Previews are used to maximise performance paging through all the images. However, to
    examine an image in detail, the full scale image is much better. As soon as the preview
    has been loaded a timer is started. If the user moves on to the next image before a timer
    interval has elapsed then the timer is reset. Otherwise, when the timer interval occurs
    (loadFullSizeTimer->setInterval(500)) then the full scale pixmap from the cache is set as
    the item pixmap in the scene.

    Moving from one image to the next, the scenario where the currrent image is full scale and
    the next is a preview, requires the zoom factor to be normalized to prevent jarring
    changes in perceived scale by the user.
*/
    if (G::isLogger || G::isFlowLogger) G::log(CLASSFUNCTION, fPath + " Src:" + src);
//    qDebug() << CLASSFUNCTION << fPath << src;

    // No folder selected yet
    if (!fPath.length()) {
        qWarning() << "WARNING" << CLASSFUNCTION << "Src =" << src << "No folder selected";
        return false;
    }

    // could be a popup from a prior uncached image being loaded
    G::popUp->end();

    // do not load image if triggered by embellish remote export
    if (G::isProcessingExportedImages) {
        qWarning() << "WARNING" << CLASSFUNCTION << "Processing exported images";
        return false;
    }

    /* important to keep currentImagePath. It is used to check if there isn't an image (when
    currentImagePath.isEmpty() == true) - for example when no folder has been chosen or the
    same image is being reloaded. It is also used by Embellish.  */
    currentImagePath = fPath;
    bool isLoaded = false;
    pmItem->setVisible(true);

    if (G::isSlideShow) {
        // load image without waiting for cache
        // check metadata loaded for image (might not be if random slideshow)
        int dmRow = dm->fPathRow[fPath];
        if (!dm->index(dmRow, G::MetadataLoadedColumn).data().toBool()) {
            QFileInfo fileInfo(fPath);
            if (metadata->loadImageMetadata(fileInfo, dm->instance, true, true, false, true, CLASSFUNCTION)) {
                metadata->m.row = dmRow;
                metadata->m.instance = dm->instance;
                dm->addMetadataForItem(metadata->m, "ImageView::loadImage");
            }
        }

        QPixmap displayPixmap;
        isLoaded = pixmap->load(fPath, displayPixmap, "ImageView::loadImage");

        if (isLoaded) {
            pmItem->setPixmap(displayPixmap);
        }
        else {
            // set null pixmap
            QPixmap nullPm;
            pmItem->setPixmap(nullPm);
            return false;
        }
    }

    /* get cached image.   Must check if image has been cached before calling
    icd->imCache.find(fPath, image) to prevent a mismatch between the fPath index and the
    image in icd->imCache hash table. */
    int dmRow = dm->rowFromPath(fPath);
    int sfRow = dm->proxyRowFromModelRow(dmRow);
//    bool isCached = dm->index(dmRow, 0).data(G::CachedRole).toBool() || src == "ImageCache::cacheImage";
    bool isCached = false;
    if (icd->cacheItemList.size() > 0)
        isCached = icd->cacheItemList.at(sfRow).isCached || src == "ImageCache::cacheImage";
//    if (G::isLogger || G::isFlowLogger) G::log(CLASSFUNCTION,
//                                               fPath + " isCached = " +
//                                               (isCached ? "true" : "false"));
    if (isCached) {
        QImage image; // confirm the cached image is in the image cache
        bool imageAvailable = icd->imCache.find(fPath, image);
        if (imageAvailable) {
            pmItem->setPixmap(QPixmap::fromImage(image));
            isLoaded = true;
        }
        else { // not available
            // MacOS: if showPopup thumbs do not scroll when hold arrow key down
            #ifdef Q_OS_WIN
                if (G::isNewFolderLoaded) {
                    G::popUp->showPopup("Buffering image");
                }
            #endif
        }
    }

    /* When the program is opening or resizing it is possible this function could be called
    before the central widget has been fully defined, and has a small default size. If that is
    the case, ignore, as the function will be called again. Also ignore if the image failed to
    be loaded into the graphics scene. */
    if (isLoaded && rect().height() > 50) {
        pmItem->setVisible(true);
        // prevent the viewport scrolling outside the image
        setSceneRect(scene->itemsBoundingRect());

        updateShootingInfo();

        /* If this is the first image in a new folder, and the image is smaller than the
        canvas (central widget window) set the scale to fit window, do not scale the
        image beyond 100% to fit the window.  */
        zoomFit = getFitScaleFactor(rect(), pmItem->boundingRect());
        if (isFirstImageNewFolder) {
            isFit = true;
            isFirstImageNewFolder = false;
        }
        if (isFit) {
            setFitZoom();
        }
        scale();
        /* send signal to Embel::build (with new image), blank first parameter means
           local vs remote (ie exported from lightroom to embellish)  */
        if (G::isEmbellish) emit embellish("", CLASSFUNCTION);
        else pmItem->setGraphicsEffect(nullptr);
    }

    if (isLoaded) return true;
    else {
        // set null pixmap
        QPixmap nullPm;
        pmItem->setPixmap(nullPm);
//        QString msg = "Could not read " + fPath;
//        G::popUp->showPopup(msg, 0);
        return false;
    }
}

void ImageView::clear()
{
    if (G::isLogger) G::log(CLASSFUNCTION);
    shootingInfo = "";
    infoOverlay->setText("");
    QPixmap nullPm;
    pmItem->setPixmap(nullPm);
    pmItem->setVisible(false);
}

void ImageView::noJpgAvailable()
{
    if (G::isLogger) G::log(CLASSFUNCTION);
    pmItem->setVisible(false);
    infoOverlay->setText("");
}

void ImageView::scale()
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
    if (G::isLogger) G::log(CLASSFUNCTION);
    /*
    qDebug() << CLASSFUNCTION
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
    emit zoomChange(zoom);

    isScrollable = (zoom > zoomFit);
    if (isScrollable) scrollPct = getScrollPct();

    if (!G::isSlideShow) {
        if (isScrollable) setCursor(Qt::OpenHandCursor);
        else {
            if (isFit) setCursor(Qt::ArrowCursor);
            else setCursor(Qt::PointingHandCursor);
        }
    }

    placeClassificationBadge();
    setShootingInfo(shootingInfo);
    emit updateStatus(true, "", CLASSFUNCTION);

    isMouseDoubleClick = false;

    /*
    qDebug() << CLASSFUNCTION
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
    if (G::isLogger) G::log(CLASSFUNCTION);
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
    if (G::isLogger) G::log(CLASSFUNCTION);
//    qDebug() << CLASSFUNCTION << container << content;
    qreal hScale = static_cast<qreal>(container.width() - 2) / content.width() * G::actDevicePixelRatio;
    qreal vScale = static_cast<qreal>(container.height() - 2) / content.height() * G::actDevicePixelRatio;
    return (hScale < vScale) ? hScale : vScale;
}

void ImageView::setScrollBars(QPointF scrollPct)
{
    if (G::isLogger) G::log(CLASSFUNCTION);
    getScrollBarStatus();
    scrl.hVal = scrl.hMin + scrollPct.x() * (scrl.hMax - scrl.hMin);
    scrl.vVal = scrl.vMin + scrollPct.y() * (scrl.vMax - scrl.vMin);
    horizontalScrollBar()->setValue(scrl.hVal);
    verticalScrollBar()->setValue(scrl.vVal);
}

void ImageView::getScrollBarStatus()
{
    if (G::isLogger) G::log(CLASSFUNCTION);
    scrl.hMin = horizontalScrollBar()->minimum();
    scrl.hMax = horizontalScrollBar()->maximum();
    scrl.hVal = horizontalScrollBar()->value();
    scrl.hPct = qreal(scrl.hVal - scrl.hMin) / (scrl.hMax - scrl.hMin);
    scrl.vMin = verticalScrollBar()->minimum();
    scrl.vMax = verticalScrollBar()->maximum();
    scrl.vVal = verticalScrollBar()->value();
    scrl.vPct = qreal(scrl.vVal - scrl.vMin) / (scrl.vMax - scrl.vMin);
    /*
    qDebug() << CLASSFUNCTION
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
    if (G::isLogger) G::log(CLASSFUNCTION);
    getScrollBarStatus();
    return QPointF(scrl.hPct, scrl.vPct);
}

void ImageView::setClassificationBadgeImageDiam(int d)
{
    if (G::isLogger) G::log(CLASSFUNCTION);
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
    if (G::isLogger) G::log(CLASSFUNCTION);
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

void ImageView::activateRubberBand()
{
    if (G::isLogger) G::log(CLASSFUNCTION);
    isRubberBand = true;
    setCursor(Qt::CrossCursor);
    QString msg = "Rubberband activated.  Make a selection in the ImageView.\n"
                  "Press Esc to quit rubberbanding.";
    G::popUp->showPopup(msg, 2000);
}

void ImageView::quitRubberBand()
{
    if (G::isLogger) G::log(CLASSFUNCTION);
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
*/
    if (G::isLogger) G::log(CLASSFUNCTION);
    /*
    qDebug() << CLASSFUNCTION
             << "G::isInitializing =" << G::isInitializing
             << "G::isNewFolderLoaded =" << G::isNewFolderLoaded
             << "isFirstImageNewFolder =" << isFirstImageNewFolder;
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
    setShootingInfo(shootingInfo);
}

void ImageView::thumbClick(float xPct, float yPct)
{
/*
   When the image is zoomed and a thumbnail is mouse clicked the position within the thumb is
   passed here as the percent of the width and height. The zoom amount is maintained and the
   main image is panned to the same location as on the thumb. This makes it quick to check
   eyes and other details in many images.
*/
    if (G::isLogger) G::log(CLASSFUNCTION);
    if (zoom > zoomFit) {
        centerOn(QPointF(xPct * sceneRect().width(), yPct * sceneRect().height()));
    }
}

qreal ImageView::getZoom()
{
    // use view center to make sure inside scene item
    if (G::isLogger) G::log(CLASSFUNCTION);
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
    if (G::isLogger) G::log(CLASSFUNCTION);
    toggleZoom = toggleZoomValue;
}

void ImageView::refresh()
{
    if (G::isLogger) G::log(CLASSFUNCTION);
    setFitZoom();
    scale();
}

void ImageView::zoomIn()
{
    if (G::isLogger) G::log(CLASSFUNCTION);
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
    qDebug() << CLASSFUNCTION
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
    if (G::isLogger) G::log(CLASSFUNCTION);
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
    if (G::isLogger) G::log(CLASSFUNCTION);
    zoom = zoomFit;
    scale();
}

void ImageView::zoomTo(qreal zoomTo)
{
/*
    Called from ZoomDlg when the zoom is changed. From here the message is passed
    on to ImageView::scale(), which in turn makes the proper scale change.
*/
    if (G::isLogger) G::log(CLASSFUNCTION);
    zoom = zoomTo;
    isFit = false;
    scale();
}

void ImageView::resetFitZoom()
{
    if (G::isLogger) G::log(CLASSFUNCTION);
    setSceneRect(scene->itemsBoundingRect());
    /*
    qDebug() << CLASSFUNCTION
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
    if (G::isLogger) G::log(CLASSFUNCTION);
    zoom = zoomFit;
    if (limitFit100Pct  && zoom > toggleZoom) zoom = toggleZoom;
}

void ImageView::zoomToggle()
{
/*
    Alternate between zoom-to-fit and another zoom value (typically 100% to check
    detail).  The other zoom value (toggleZoom) can be assigned in ZoomDlg and
    defaults to 1.0
*/
    if (G::isLogger) G::log(CLASSFUNCTION);
    isFit = !isFit;
    isFit ? zoom = zoomFit : zoom = toggleZoom;
    scale();
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
    if (G::isLogger) G::log(CLASSFUNCTION);
    qDebug() << CLASSFUNCTION << degrees;

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
    if (G::isLogger) G::log(CLASSFUNCTION, imageFullPath);
    qDebug() << CLASSFUNCTION << imageFullPath;

    QTransform trans;
    int row = dm->fPathRow[imageFullPath];
    int orientation = dm->index(row, G::OrientationColumn).data().toInt();
//    int orientation = metadata->getOrientation(imageFullPath);

    switch(orientation) {
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
    if (G::isLogger) G::log(CLASSFUNCTION);
    sceneOrigin = mapFromScene(0.0, 0.0);
    scene_Rect = sceneRect();
    cwRect = rect();
}

void ImageView::updateShootingInfo()
{
    if (G::isLogger) G::log(CLASSFUNCTION);
    QModelIndex idx = thumbView->currentIndex();
    QString current = infoString->getCurrentInfoTemplate();
    shootingInfo = infoString->parseTokenString(infoString->infoTemplates[current],
                                                currentImagePath, idx);
    infoOverlay->setText(shootingInfo);
//    moveShootingInfo(shootingInfo);
//    qDebug() << CLASSFUNCTION << shootingInfo;
}

void ImageView::setShootingInfo(QString infoString)
{
/*
    Locate and format the info label, which currently displays the shooting
    information in the top left corner of the image.  The text has a drop shadow
    to help make it visible against different coloured backgrounds.

    window (w) and view (v) sizes are updated during resize
*/
    if (G::isLogger) G::log(CLASSFUNCTION);

    int offset = 10;                        // offset pixels from the edge of image
    int x, y = 0;                           // top left coordinates of info symbol
    // the top left of the image in veiwport coordinates
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
    infoOverlay->setText(infoString);
    infoOverlay->adjustSize();
    // make a little wider to account for the drop shadow
    infoOverlay->resize(infoOverlay->width()+10, infoOverlay->height()+10);
    infoOverlay->move(x, y);
}

void ImageView::monitorCursorState()
{
    if (G::isLogger) G::log(CLASSFUNCTION);
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
    if (G::isLogger) G::log(CLASSFUNCTION);
    scene->setBackgroundBrush(bg);
}

void ImageView::setCursorHiding(bool hide)
{
    if (G::isLogger) G::log(CLASSFUNCTION);
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
    if (G::isLogger) G::log(CLASSFUNCTION);
    setCursor(Qt::BlankCursor);
}

// EVENTS

void ImageView::scrollContentsBy(int dx, int dy)
{
    if (G::isLogger) G::log(CLASSFUNCTION);
    scrollCount++;
    QGraphicsView::scrollContentsBy(dx, dy);
}

// MOUSE CONTROL

//void ImageView::dragMoveEvent(QDragMoveEvent *event)
//{
//    qDebug() << G::t.restart() << "\t" << "drag";
//}

void ImageView::wheelEvent(QWheelEvent *event)
{
    if (G::isLogger) G::log(CLASSFUNCTION);

    // wheel scrolling / trackpad swiping = next/previous image
    static int deltaSum = 0;
    static int prevDelta = 0;
    int delta = event->angleDelta().y();
    if ((delta > 0 && prevDelta < 0) || (delta < 0 && prevDelta > 0)) {
        deltaSum = delta;
    }
    deltaSum += delta;
    /*
    qDebug() << CLASSFUNCTION
             << "delta =" << delta
             << "prevDelta =" << prevDelta
             << "deltaSum =" << deltaSum
             << "G::wheelSensitivity =" << G::wheelSensitivity
                ;
                //*/

    if (deltaSum > G::wheelSensitivity) {
        thumbView->selectPrev();
        deltaSum = 0;
    }

    if (deltaSum < (-G::wheelSensitivity)) {
        thumbView->selectNext();
        deltaSum = 0;
    }
}

bool ImageView::event(QEvent *event) {
    /*
        Trap back/forward buttons on Logitech mouse to toggle pick status on thumbnail
    */
//    if (G::isLogger) G::log(CLASSFUNCTION);
//    qDebug() << CLASSFUNCTION << event;
    if (event->type() == QEvent::NativeGesture) {
        emit togglePick();
        /*
        QNativeGestureEvent *e = static_cast<QNativeGestureEvent *>(event);
        if (e->value() == 0) {
            // forward
        }
        else {
            // back
        }
        //*/
    }
    QGraphicsView::event(event);
    return true;
}

// not used
void ImageView::mouseDoubleClickEvent(QMouseEvent *event)
{
    if (G::isLogger) G::log(CLASSFUNCTION);
    // placeholder function pending use
    QGraphicsView::mouseDoubleClickEvent(event);

}

void ImageView::mousePressEvent(QMouseEvent *event)
{
    if (G::isLogger) G::log(CLASSFUNCTION);

    // bad things happen if no image when click
    if (currentImagePath.isEmpty()) return;

    // prevent zooming when right click for context menu
    if (event->button() == Qt::RightButton) {
        return;
    }

    // forward and back buttons
    if (event->button() == Qt::BackButton) {
//        thumbView->selectPrev();
        emit togglePick();
        return;
    }
    if (event->button() == Qt::ForwardButton) {
//        thumbView->selectNext();
        emit togglePick();
        return;
    }

    if (isMouseDoubleClick) return;

    isMouseDoubleClick = false;
    isMouseDrag = false;
    if (event->button() == Qt::LeftButton) {
        /*if (isRubberBand) {
            setCursor(Qt::CrossCursor);
            origin = event->pos();
            rubberBand->setGeometry(QRect(origin, QSize()));
            rubberBand->show();
            return;
        }
        //*/
        isLeftMouseBtnPressed = true;
        scrollCount = 0;                // still used?
        mousePressPt.setX(event->x());
        mousePressPt.setY(event->y());

        QGraphicsView::mousePressEvent(event);

        if (isFit) {
            zoomToggle();
            scale();
            isZoomToggled = true;
            setCursor(Qt::PointingHandCursor);
        }
        else {
            isZoomToggled = false;
        }

    }
//    QGraphicsView::mousePressEvent(event);
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

    if (isLeftMouseBtnPressed && event->pos() != prevPos) {
        if (G::isSlideShow) emit killSlideshow();
        isMouseDrag = true;
        setCursor(Qt::ClosedHandCursor);
        horizontalScrollBar()->setValue(horizontalScrollBar()->value() -
                                       (event->x() - mousePressPt.x()));
        verticalScrollBar()->setValue(verticalScrollBar()->value() -
                                      (event->y() - mousePressPt.y()));
        mousePressPt.setX(event->x());
        mousePressPt.setY(event->y());
    }
    else{
        if (G::isSlideShow) {
            if(event->pos() != prevPos) {
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
    if (G::isLogger) G::log(CLASSFUNCTION);
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
        qDebug() << CLASSFUNCTION << "new tile";
//        emit newTile();
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

    // if mouse dragging then do not toggle zoom
    if (isMouseDrag || isMouseDoubleClick) {
        isMouseDrag = false;
        if (isScrollable) setCursor(Qt::OpenHandCursor);
        else setCursor(Qt::ArrowCursor);
        return;
    }

    // if zoomToggle was not executed on mouse press (image was scrollable and zoom in toggled)
    if (!isZoomToggled) {
        zoomToggle();
        scale();
        if (isScrollable) setCursor(Qt::OpenHandCursor);
        else {
            if (isFit) setCursor(Qt::ArrowCursor);
            else setCursor(Qt::PointingHandCursor);
        }
    }

    QGraphicsView::mouseReleaseEvent(event);
}

// qt6.2
//void ImageView::enterEvent(QEvent *event)
//{
//    if (G::isLogger) G::log(CLASSFUNCTION);
//    QVariant x = event->type();     // suppress compiler warning
////    this->setFocus();
//}

// DRAG AND DROP

void ImageView::dragEnterEvent(QDragEnterEvent *event)
{
/*
    Empty function required to propagate drop event (not sure why)
*/
    if (G::isLogger) G::log(CLASSFUNCTION);
    qDebug() << CLASSFUNCTION;
}

QString ImageView::diagnostics()
{
    if (G::isLogger) G::log(CLASSFUNCTION);
    QString reportString;
    QTextStream rpt;
    rpt.setString(&reportString);
    rpt << Utilities::centeredRptHdr('=', "ImageView Diagnostics");
    rpt << "\n";
    rpt << "\n" << "isBusy = " << G::s(isBusy);
    rpt << "\n" << "shootingInfo = " << G::s(shootingInfo);
    rpt << "\n" << "infoOverlayFontSize = " << G::s(infoOverlayFontSize);
    rpt << "\n" << "currentImagePath = " << G::s(currentImagePath);
    rpt << "\n" << "firstImageLoaded = " << G::s(isFirstImageNewFolder);
    rpt << "\n" << "classificationBadgeDiam = " << G::s(classificationBadgeDiam);
    rpt << "\n" << "cursorIsHidden = " << G::s(cursorIsHidden);
    rpt << "\n" << "moveImageLocked = " << G::s(moveImageLocked);
    rpt << "\n" << "ignoreMousePress = " << G::s(isZoomToggled);
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
    if (G::isLogger) G::log(CLASSFUNCTION);
    QImage image = grab().toImage();
//    image.save();
}

// COPY AND PASTE

void ImageView::copyImage()
{
    if (G::isLogger) G::log(CLASSFUNCTION);
    QPixmap pm = pmItem->pixmap();
    if (pm.isNull()) {
        QImage image;
        if (icd->imCache.find(dm->currentFilePath, image)) {
            pm = QPixmap::fromImage(image);
        }
        else {
            QString msg = "Could not copy the current image to the clipboard";
            G::popUp->showPopup(msg, 1500);
        }
    }
    QApplication::clipboard()->setPixmap(pm, QClipboard::Clipboard);
    QString msg = "Copied current image to the clipboard";
    G::popUp->showPopup(msg, 1500);
}

// not being used, but maybe in the future
//static inline int bound0To255(int val)
//{
//    if (G::isLogger) G::log(CLASSFUNCTION);
//    return ((val > 255) ? 255 : (val < 0) ? 0 : val);
//}

static inline int hslValue(double n1, double n2, double hue)
{
    if (G::isLogger) G::log(__PRETTY_FUNCTION__);
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
    if (G::isLogger) G::log(__PRETTY_FUNCTION__);
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
    if (G::isLogger) G::log(__PRETTY_FUNCTION__);
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
