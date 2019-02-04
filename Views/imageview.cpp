#include <QGraphicsDropShadowEffect>
#include <math.h>
#include "Main/global.h"
#include "Views/imageview.h"
#include <QApplication>

#define CLIPBOARD_IMAGE_NAME		"clipboard.png"
#define ROUND(x) ((int) ((x) + 0.5))
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
                     ImageCache *imageCacheThread,
                     ThumbView *thumbView,
                     InfoString *infoString,
                     bool isShootingInfoVisible,
                     bool isRatingBadgeVisible,
                     int classificationBadgeDiam) :

                     QGraphicsView(centralWidget)
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }

    this->mainWindow = parent;
    this->centralWidget = centralWidget;
    this->metadata = metadata;
    this->imageCacheThread = imageCacheThread;
    this->thumbView = thumbView;
    this->infoString = infoString;
    this->classificationBadgeDiam = classificationBadgeDiam;
    pixmap = new Pixmap(this, metadata);

    scene = new QGraphicsScene();
    pmItem = new QGraphicsPixmapItem;
    pmItem->setBoundingRegionGranularity(1);
    scene->addItem(pmItem);

    setAcceptDrops(true);
    pmItem->setAcceptDrops(true);

    /* setOptimizationFlags(QGraphicsView::DontSavePainterState);
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
    infoOverlay->setVisible(isShootingInfoVisible);
    infoOverlay->setAttribute(Qt::WA_TranslucentBackground);

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

    // rgh is this needed or holdover from prev program
    mouseMovementTimer = new QTimer(this);
//    connect(mouseMovementTimer, SIGNAL(timeout()), this, SLOT(monitorCursorState()));

    loadFullSizeTimer = new QTimer(this);
    loadFullSizeTimer->setInterval(500);
    connect(loadFullSizeTimer, SIGNAL(timeout()), this, SLOT(upgradeToFullSize()));


//    mouseZoomFit = true;
    isMouseDrag = false;
    isLeftMouseBtnPressed = false;
    isMouseDoubleClick = false;
    firstImageLoaded = true;
}

bool ImageView::loadImage(QModelIndex idx, QString fPath)
{
/*
There are two sources for the image (pixmap): a file or the cache.

The first choice is the cache.  In the cache two versions of the image are
stored: the full scale and a preview scaled to dimensions defined in
the preferences.  The preview is used if it is large enough to fit in the
window (viewport) without scaling larger than 1.  If the preview is too
small then the full size is used.

If the image has not been cached (usually the case for the first image to
be shown, as the caching is just starting) then the full size image is read
from the file.

Previews are used to maximise performance paging through all the images.
However, to examine an image in detail, the full scale image is much better. As
soon as the preview has been loaded a timer is started. If the user moves on to
the next image before a timer interval has happened then the timer is reset.
Otherwise, when the timer interval occurs (loadFullSizeTimer->setInterval(500))
then the full scale pixmap from the cache is set as the item pixmap in the
scene.

Moving from one image to the next, the scenario where the currrent image is
full scale and the next is a preview, requires the zoom factor to be normalized
to prevent jarring changes in perceived scale by the user.
*/
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    // No folder selected yet
    if (!fPath.length()) return false;

    /* important to keep currentImagePath.  It is used to check if there isn't
    an image (when currentImagePath.isEmpty() == true) - for example when
    no folder has been chosen. */
    currentImagePath = fPath;
    imageIndex = idx;
    bool isLoaded = false;
    pmItem->setVisible(true);

    // load the image from the image cache if available
    if (imageCacheThread->imCache.contains(fPath)) {
        // load preview from cache
        bool tryPreview = true;     // for testing
        loadFullSizeTimer->stop();

        // get preview size from stored metadata to decide if load preview or full
        setFullDim();               // req'd by setPreviewDim()
        setPreviewDim();            // defines QSize preview
        qreal previewFit = getFitScaleFactor(rect(), QRect(QPoint(0,0), preview));
        bool previewBigEnough = previewFit > zoom;

        // initially load a preview if available and big enough
        if (tryPreview && imageCacheThread->imCache.contains(fPath + "_Preview")
        && previewBigEnough) {
            pmItem->setPixmap(QPixmap::fromImage(imageCacheThread->imCache.value(fPath + "_Preview")));
            isPreview = true;
            /* if preview smaller than view then next image should also be smaller
            since each image may be different sizes have to equalize scale
            zoomFit is still for prev image and previewFit is for current preview */
            if (!isFit) zoom *= previewFit / zoomFit;
            isLoaded = true;
            loadFullSizeTimer->start();
        }
        // otherwise load full size image from cache
        else {
            pmItem->setPixmap(QPixmap::fromImage(imageCacheThread->imCache.value(fPath)));
            isPreview = false;
            isLoaded = true;
        }
    }
    else {
        isLoaded = pixmap->load(fPath, displayPixmap);
        if (isLoaded) {
            pmItem->setPixmap(displayPixmap);
            isPreview = false;
        }
        else {
            pmItem->setPixmap(QPixmap(":/images/error_image.png"));
        }
    }

    if (isLoaded) {
        pmItem->setVisible(true);
        // prevent the viewport scrolling outside the image
        setSceneRect(scene->itemsBoundingRect());
        QModelIndex idx = thumbView->currentIndex();
        QString current = infoString->getCurrentInfoTemplate();
        shootingInfo = infoString->parseTokenString(infoString->infoTemplates[current],
                                                    currentImagePath, idx);

        zoomFit = getFitScaleFactor(centralWidget->rect(), pmItem->boundingRect());
        if (!firstImageLoaded) {
            // check if last image was a zoomFit - if so zoomFit this one too
            // otherwise keep zoom same as previous
            if (isFit) {
                zoom = zoomFit;
            }
            scale();
        }
    }
    return isLoaded;
}

void ImageView::upgradeToFullSize()
{
/* Called after a delay by timer initiated in loadImage. Two prior conditions
are matched:
    If zoomed then the relative scroll position is set.
    If zoomFit then zoomFit is recalculated.
*/
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    loadFullSizeTimer->stop();
    if(imageCacheThread->imCache.contains(currentImagePath)) {
        pmItem->setPixmap(QPixmap::fromImage(imageCacheThread->imCache.value(currentImagePath)));
        setSceneRect(scene->itemsBoundingRect());
        isPreview = false;
        qreal prevZoomFit = zoomFit;
        zoomFit = getFitScaleFactor(centralWidget->rect(), pmItem->boundingRect());
        zoom *= (zoomFit / prevZoomFit);
        if (isFit) zoom = zoomFit;
        scale();
        if (isZoom) setScrollBars(scrollPct);
    }
}

void ImageView::clear()
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    pmItem->setVisible(false);
    shootingInfo = "";
    infoOverlay->setText("");
}

void ImageView::noJpgAvailable()
{
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

*/
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
/*
    qDebug() << G::t.restart() << "\t" << "isPreview =" << isPreview
             << "isZoom =" << isZoom
             << "isFit =" << isFit
             << "zoom =" << zoom
             << "zoomFit =" << zoomFit
             << "rect().width() =" << rect().width()
             << "sceneRect().width() =" << sceneRect().width();
             */

    matrix.reset();
    matrix.scale(zoom, zoom);
    setMatrix(matrix);
    emit zoomChange(zoom);
    isZoom = (zoom > zoomFit);
    isFit = (zoom == zoomFit);
    if (isZoom) scrollPct = getScrollPct();
    wasZoomFit = zoom == zoomFit;
    if (isZoom) setCursor(Qt::OpenHandCursor);
    else setCursor(Qt::ArrowCursor);

    placeClassificationBadge();
    moveShootingInfo(shootingInfo);
    emit updateStatus(true, "");
}

void ImageView::setFullDim()
{
/* Sets QSize full from metadata.  Req'd by setPreviewDim */
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    full.setWidth(metadata->getWidth(currentImagePath));
    full.setHeight(metadata->getHeight(currentImagePath));
}

void ImageView::setPreviewDim()
{
/*  Sets the QSize preview from metadata and the aspect ratio of the image.
Req'd in advance to decide if the preview is big enough to use.  Uses full,
which is defined in setFullDim.
*/
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    preview = imageCacheThread->getPreviewSize();
    qreal fullAspectRatio = (qreal)(full.height()) / full.width();
    if (full.width() > full.height()) {
        preview.setHeight(preview.width() * fullAspectRatio);
    }
    else {
        preview.setWidth(preview.height() / fullAspectRatio);
    }
}

bool ImageView::sceneBiggerThanView()
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    QPoint pTL = mapFromScene(0, 0);
    QPoint pBR = mapFromScene(scene->width(), scene->height());
    int sceneViewWidth = pBR.x() - pTL.x();
    int sceneViewHeight = pBR.y() - pTL.y();
    if (sceneViewWidth > rect().width() && sceneViewHeight > rect().height())
        return true;
    else
        return false;
}

qreal ImageView::getFitScaleFactor(QRectF container, QRectF content)
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    qreal hScale = ((qreal)container.width() - 2) / content.width();
    qreal vScale = ((qreal)container.height() - 2) / content.height();
    return (hScale < vScale) ? hScale : vScale;
}

void ImageView::setScrollBars(QPointF scrollPct)
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    getScrollBarStatus();
    scrl.hVal = scrl.hMin + scrollPct.x() * (scrl.hMax - scrl.hMin);
    scrl.vVal = scrl.vMin + scrollPct.y() * (scrl.vMax - scrl.vMin);
    horizontalScrollBar()->setValue(scrl.hVal);
    verticalScrollBar()->setValue(scrl.vVal);
}

void ImageView::getScrollBarStatus()
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    scrl.hMin = horizontalScrollBar()->minimum();
    scrl.hMax = horizontalScrollBar()->maximum();
    scrl.hVal = horizontalScrollBar()->value();
    scrl.hPct = qreal(scrl.hVal - scrl.hMin) / (scrl.hMax - scrl.hMin);
    scrl.vMin = verticalScrollBar()->minimum();
    scrl.vMax = verticalScrollBar()->maximum();
    scrl.vVal = verticalScrollBar()->value();
    scrl.vPct = qreal(scrl.vVal - scrl.vMin) / (scrl.vMax - scrl.vMin);
}

QPointF ImageView::getScrollPct()
{
/* The view center is defined by the scrollbar values.  The value is converted
to a percentage to be used to match position in the next image if zoomed.
*/
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    getScrollBarStatus();
    return QPointF(scrl.hPct, scrl.vPct);
}

void ImageView::setClassificationBadgeImageDiam(int d)
{
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
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
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

void ImageView::resizeEvent(QResizeEvent *event)
{
/* Manage behavior when window resizes.

● if image zoom is not zoomFit then no change
● if zoomFit then recalc and maintain
● if zoomed and resize to view entire image then engage zoomFit
● if view larger than image and resize to clip image then engage zoomFit.

● move and size pick icon and shooting info as necessary

*/
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    QGraphicsView::resizeEvent(event);

    zoomFit = getFitScaleFactor(centralWidget->rect(), pmItem->boundingRect());
    static QRect prevRect;
    static bool wasSceneClipped;

    if (firstImageLoaded) {
        zoom = zoomFit;
        scale();
        firstImageLoaded = false;
        wasZoomFit = true;
        wasSceneClipped = false;
    }
    else {
        bool viewPortIsExpanding = false;
        if (prevRect.width() < rect().width() || prevRect.height() < rect().height())
            viewPortIsExpanding = true;
        bool sceneClipped = sceneBiggerThanView();
        if ((sceneClipped && !isZoom) ||
        (zoom == zoomFit) ||
        ((viewPortIsExpanding && !sceneClipped) && wasZoomFit) ||
        (wasSceneClipped && !sceneClipped)) {
            zoom = zoomFit;
            wasZoomFit = true;
            scale();
        }
        wasSceneClipped = sceneClipped;
        prevRect = rect();
    }
    placeClassificationBadge();
    moveShootingInfo(shootingInfo);
}

void ImageView::thumbClick(float xPct, float yPct)
{
/* When the image is zoomed and a thumbnail is mouse clicked the position
within the thumb is passed here as the percent of the width and height. The
zoom amount is maintained and the main image is panned to the same location as
on the thumb. This makes it quick to check eyes and other details in many
images.
*/
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    if (zoom > zoomFit) {
        centerOn(QPointF(xPct * sceneRect().width(), yPct * sceneRect().height()));
    }
}

qreal ImageView::getZoom()
{
    // use view center to make sure inside scene item
    qreal x1 = mapToScene(rect().center()).x();
    qreal x2 = mapToScene(rect().center() + QPoint(1, 0)).x();
    qreal calcZoom = 1.0 / (x2 - x1);
//    qDebug() << G::t.restart() << "\t" << "getZoom():   zoom =" << zoom
//             << "x1 =" << x1 << "x2 =" << x2
//             << "calc zoom =" << calcZoom;
    return calcZoom;
}

void ImageView::updateToggleZoom(qreal toggleZoomValue)
{
/*
Slot for signal from update zoom dialog to set the amount to zoom when user
clicks on the unzoomed image.
*/
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    toggleZoom = toggleZoomValue;
}

void ImageView::zoomIn()
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    zoom *= (1.0 + zoomInc);
    zoom = zoom > zoomMax ? zoomMax: zoom;
    scale();
}

void ImageView::zoomOut()
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    zoom *= (1.0 - zoomInc);
    zoom = zoom < zoomMin ? zoomMin : zoom;
    scale();
}

void ImageView::zoomToFit()
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    zoom = zoomFit;
    scale();
}

void ImageView::zoomTo(qreal zoomTo)
{
/*
Called from ZoomDlg when the zoom is changed. From here the message is passed
on to ImageView::scale(), which in turn makes the proper scale change.
*/
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    zoom = zoomTo;
    scale();
}

void ImageView::zoomToggle()
{
/*
Alternate between zoom-to-fit and another zoom value (typically 100% to check
detail).  The other zoom value (toggleZoom) can be assigned in ZoomDlg and
defaults to 1.0
*/
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    if (zoom < zoomFit) zoom = zoomFit;
    else if (zoom == zoomFit) zoom = toggleZoom;
    else if (isZoom) zoom = zoomFit;
    scale();
}

void ImageView::rotate(int degrees)
{
/*
This is called from MW::setRotation and rotates the image currently shown in
loupe view.  Loupe view is a QGraphicsView of the QGraphicsScene scene.  The
scene contains one QGraphicsItem pmItem, which in turn, contains the pixmap.
When the pixmap is rotated the scene bounding rectangle must be adjusted and
the zoom factor to fit recalculated.  Finally, scale() is called to fit the
image if the image was not zoomed.
*/
    // extract pixmap, rotate and reset to pmItem
    QPixmap pm = pmItem->pixmap();
    QTransform trans;
    trans.rotate(degrees);
    pmItem->setPixmap(pm.transformed(trans, Qt::SmoothTransformation));

    // reset the scene
    setSceneRect(scene->itemsBoundingRect());

    // recalc zoomFit factor
    zoomFit = getFitScaleFactor(centralWidget->rect(), pmItem->boundingRect());

    // if in isFit mode then zoom accordingly
    if (isFit) {
        zoom = zoomFit;
        scale();
    }
}

void ImageView::rotateByExifRotation(QImage &image, QString &imageFullPath)
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    #ifdef ISPROFILE
    G::track(__FUNCTION__, "About to QTransform trans");
    #endif
    QTransform trans;
    int orientation = metadata->getOrientation(imageFullPath);

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

void ImageView::transform()
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    rotateByExifRotation(displayImage, currentImagePath);
}

void ImageView::moveShootingInfo(QString infoString)
{
/* Locate and format the info label, which currently displays the shooting
information in the top left corner of the image.  The text has a drop shadow
to help make it visible against different coloured backgrounds. */
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    // window (w) and view (v) sizes are updated during resize

    int offset = 10;                        // offset pixels from the edge of image
    int x, y = 0;                           // top left coordinates of info symbol
    QPoint sceneOrigin;

    sceneOrigin = mapFromScene(0.0, 0.0);

    // if the scene is not as wide as the view
    if (sceneOrigin.x() > 0)
        x = sceneOrigin.x() + offset;
    else x = offset;

    // if the scene is not as high as the view
    if (sceneOrigin.y() > 0)
        y = sceneOrigin.y() + offset;
    else y = offset;

    QFont font( "Tahoma", infoOverlayFontSize);
    font.setKerning(true);

    infoOverlay->setFont(font);      // not working
//    int fontSize = 13;          // rgh make this a preference

    infoOverlay->setStyleSheet("font: " + QString::number(infoOverlayFontSize) + "pt;");
    infoOverlay->setText(infoString);
    infoOverlay->adjustSize();
    // make a little wider to account for the drop shadow
    infoOverlay->resize(infoOverlay->width()+10, infoOverlay->height()+10);
    infoOverlay->move(x, y);
}

void ImageView::monitorCursorState()
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
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

void ImageView::setCursorHiding(bool hide)
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
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

//void ImageView::compareZoomAtCoord(QPointF coord, bool isZoom)
//{
///* Same as when user mouse clicks on the image.  Called from compareView to
//replicate zoom in all compare images.
//*/
//    {
//    #ifdef ISDEBUG
//    qDebug() << G::t.restart() << "\t" << "ImageView::compareZoomAtCoord";
//    #endif
//    }
////    qDebug() << G::t.restart() << "\t" << "\n" << currentImagePath;
////    qDebug() << G::t.restart() << "\t" << "ImageView::compareZoomAtCoord" << coord;
//    zoom = toggleZoom;     // if zoomToFit then zoom reset in resize
////    mouseZoomFit = !mouseZoomFit;
//    f.w = imageLabel->geometry().width();
//    f.h = imageLabel->geometry().height();
//    mouse.x = imageLabel->geometry().topLeft().x() + coord.x() * f.w;
//    mouse.y = imageLabel->geometry().topLeft().y() + coord.y() * f.h;
////    qDebug() << G::t.restart() << "\t" << "imageLabel->geometry().topLeft().x()" << imageLabel->geometry().topLeft().x()
////             << "imageLabel->geometry().topLeft().y()" << imageLabel->geometry().topLeft().y()
////             << "w, h, mouse.x, mouse.y" << f.w << f.h
////             << mouse.x << mouse.y << "\n";
////    resizeImage();
//}

// EVENTS

void ImageView::paintEvent(QPaintEvent *event)
{
    QGraphicsView::paintEvent(event); // paint contents normally

    // draw text over the top of the viewport
//    QPainter p(viewport());
//    QPoint pt(30,30); // location for text string, in this case upper left corner
//    QString str;
//    // set string text, in this case the mouse position value
//    str = QString("TEST");
//    p.drawText(pt, str);

//    p.end();
}

void ImageView::scrollContentsBy(int dx, int dy)
{
    scrollCount++;
//    isMouseDrag = (scrollCount > 2);
//    qDebug() << G::t.restart() << "\t" << "scrolling dx =" << dx << "dy =" << dy << scrollCount;
    QGraphicsView::scrollContentsBy(dx, dy);
}

// MOUSE CONTROL

//void ImageView::dragMoveEvent(QDragMoveEvent *event)
//{
//    qDebug() << G::t.restart() << "\t" << "drag";
//}

void ImageView::wheelEvent(QWheelEvent *event)
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }

    // if trackpad scrolling set in preferences then default behavior
    if(useWheelToScroll) {
        QGraphicsView::wheelEvent(event);
        isTrackpadScroll = true;
        return;
    }

    // otherwise trapckpad swiping = next/previous image
    static int delta;
    delta += event->delta();
    int deltaThreshold = 40;

    if(delta > deltaThreshold) {
        thumbView->selectPrev();
        delta = 0;
    }

    if(delta < (-deltaThreshold)) {
        thumbView->selectNext();
        delta = 0;
    }
}

// not used
void ImageView::mouseDoubleClickEvent(QMouseEvent *event)
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    // placeholder function pending use
    // isMouseDoubleClick = true;
    QWidget::mouseDoubleClickEvent(event);
}

void ImageView::mousePressEvent(QMouseEvent *event)
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    static int n = 0;
    n++;
//    qDebug() << "mousePressEvent" << n
//             << "button =" << event->button();
    qDebug() << QTime::currentTime() << "MouseButtonPress" << __FUNCTION__;

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

    isMouseDoubleClick = false;
    if (event->button() == Qt::LeftButton) {
        isLeftMouseBtnPressed = true;
        scrollCount = 0;                // still used?
        mousePressPt.setX(event->x());
        mousePressPt.setY(event->y());
        QGraphicsView::mousePressEvent(event);
    }
//    QGraphicsView::mousePressEvent(event);
}

void ImageView::mouseMoveEvent(QMouseEvent *event)
{
/*
Pan the image during a mouse drag operation
*/
    {
    #ifdef ISDEBUG
//    G::track(__FUNCTION__);
    #endif
    }
//    qDebug() << G::t.restart() << "\t" << "ImageView::mouseMoveEvent" << isMouseDrag;
    if (isLeftMouseBtnPressed) {
        isMouseDrag = true;
//        qDebug() << G::t.restart() << "\t" << "ImageView::mouseMoveEvent  isLeftMouseBtnPressed" << isLeftMouseBtnPressed << "isMouseDrag" << isMouseDrag;
        setCursor(Qt::ClosedHandCursor);
        horizontalScrollBar()->setValue(horizontalScrollBar()->value() -
                                       (event->x() - mousePressPt.x()));
        verticalScrollBar()->setValue(verticalScrollBar()->value() -
                                      (event->y() - mousePressPt.y()));
        mousePressPt.setX(event->x());
        mousePressPt.setY(event->y());
//        qDebug() << G::t.restart() << "\t" << event;
    }
    event->ignore();

//    QGraphicsView::mouseMoveEvent(event);
}

void ImageView::mouseReleaseEvent(QMouseEvent *event)
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    qDebug() << QTime::currentTime() << "MouseButtonRelease" << __FUNCTION__ << "\n";
    // prevent zooming when right click for context menu
    if (event->button() == Qt::RightButton) {
        return;
    }

    // prevent zooming when forward and back buttons
    if (event->button() == Qt::BackButton
    ||  event->button() == Qt::ForwardButton) {
        return;
    }
    if (event->button() == Qt::ForwardButton) {
        thumbView->selectNext();
        return;
    }

    //    qDebug() << G::t.restart() << "\t" << "mouseReleaseEvent" << event << "isMouseDrag" << isMouseDrag;
    isLeftMouseBtnPressed = false;
    if (isMouseDrag || isMouseDoubleClick) {
//        qDebug() << G::t.restart() << "\t" << "mouseReleaseEvent  if (isMouseDrag || isMouseDoubleClick)" << event << "isMouseDrag" << isMouseDrag;
        isMouseDrag = false;
        if (isZoom) setCursor((Qt::OpenHandCursor));
        else setCursor(Qt::ArrowCursor);
        return;
    }

    if (!isZoom && zoom < zoomFit * 0.99)
        zoom = zoomFit;
    else
        isZoom ? zoom = zoomFit : zoom = toggleZoom;
    scale();
    QGraphicsView::mouseReleaseEvent(event);
}

void ImageView::enterEvent(QEvent *event)
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    QVariant x = event->type();     // suppress compiler warning
    this->setFocus();
}

// DRAG AND DROP

void ImageView::dragEnterEvent(QDragEnterEvent *event)
{
    event->acceptProposedAction();
}

void ImageView::dropEvent(QDropEvent *event)
{
    emit handleDrop(event->mimeData());
}

// COPY AND PASTE

void ImageView::copyImage()
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    //    QApplication::clipboard()->setImage(displayImage);
}


// not being used, but maybe in the future
static inline int bound0To255(int val)
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    return ((val > 255)? 255 : (val < 0)? 0 : val);
}

static inline int hslValue(double n1, double n2, double hue)
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
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
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
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
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
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
