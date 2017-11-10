#include <QGraphicsDropShadowEffect>
#include <math.h>
#include "global.h"
#include "imageview.h"
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

ImageView::ImageView(QWidget *parent, QWidget *centralWidget, Metadata *metadata,
                     ImageCache *imageCacheThread, ThumbView *thumbView,
                     bool isShootingInfoVisible): QGraphicsView(centralWidget)
{
    {
    #ifdef ISDEBUG
    qDebug() << "ImageView::ImageView";
    #endif
    }

    this->mainWindow = parent;
    this->centralWidget = centralWidget;
    this->metadata = metadata;
    this->imageCacheThread = imageCacheThread;
    this->thumbView = thumbView;
    pixmap = new Pixmap(this, metadata);

    scene = new QGraphicsScene();
    pmItem = new QGraphicsPixmapItem;
    pmItem->setBoundingRegionGranularity(1);
    scene->addItem(pmItem);

//    setOptimizationFlags(QGraphicsView::DontSavePainterState);
//    setViewportUpdateMode(QGraphicsView::SmartViewportUpdate);
//    setViewportUpdateMode(QGraphicsView::FullViewportUpdate);
//    setTransformationAnchor(QGraphicsView::AnchorViewCenter);
//    setAlignment(Qt::AlignCenter);
//    setDragMode(QGraphicsView::ScrollHandDrag);
    setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
    setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setScene(scene);

    cursorIsHidden = false;
    moveImageLocked = false;

    infoDropShadow = new DropShadowLabel(this);
    infoDropShadow->setVisible(isShootingInfoVisible);
    infoDropShadow->setAttribute(Qt::WA_TranslucentBackground);

    // title included in infoLabel, but might want to separate
    titleDropShadow = new DropShadowLabel(this);
    titleDropShadow->setVisible(isShootingInfoVisible);
    titleDropShadow->setAttribute(Qt::WA_TranslucentBackground);

    pickLabel = new QLabel(this);
//    pickLabel->setFixedSize(64,51);
    pickLabel->setFixedSize(40,48);
    pickLabel->setAttribute(Qt::WA_TranslucentBackground);
//    pickPixmap = new QPixmap(":/images/checkmark64.png");
    pickPixmap = new QPixmap(":/images/ThumbsUp48.png");
    // setPixmap during resize event
    pickLabel->setAlignment(Qt::AlignRight | Qt::AlignBottom);
    pickLabel->setVisible(false);

    editsLabel = new CircleLabel(this);
    editsLabel->setVisible(false);

    QGraphicsOpacityEffect *infoEffect = new QGraphicsOpacityEffect;
    infoEffect->setOpacity(0.8);
//    infoLabel->setGraphicsEffect(infoEffect);
//    infoLabelShadow->setGraphicsEffect(infoEffect);
    infoDropShadow->setGraphicsEffect(infoEffect);

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
    qDebug() << "ImageView::loadImage";
    #endif
    }
    // No folder selected yet
    if (fPath == "") return false;

     /* important to keep currentImagePath.  It is used to check if there isn't
     an image (when currentImagePath.isEmpty() == true) - for example when
     no folder has been chosen. */
    currentImagePath = fPath;
    imageIndex = idx;
    bool isLoaded = false;

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
        if (tryPreview && imageCacheThread->imCache.contains(fPath + "_Preview") &&
                previewBigEnough) {
            pmItem->setPixmap(imageCacheThread->imCache.value(fPath + "_Preview"));
            isPreview = true;
            // if prev smaller than view than next image should also be smaller
            // since each image may be different sizes have to equalize scale
            // zoomFit is still for prev image and previewFit is for current preview
            if (!isFit) zoom *= previewFit / zoomFit;
            isLoaded = true;
            loadFullSizeTimer->start();
        }
        // otherwise load full size image from cache
        else {
            pmItem->setPixmap(imageCacheThread->imCache.value(fPath));
            isPreview = false;
            isLoaded = true;
        }
    }
    else {
        // load the image from the image file, may need to wait a bit if another thread
        // reading file
        for (int i=0; i<100000; i++) {
            isLoaded = pixmap->load(fPath, displayPixmap);
            if (isLoaded) break;
        }
        if (isLoaded) {
            pmItem->setPixmap(displayPixmap);
            isPreview = false;
        }
    }

    if (isLoaded) {
        // prevent the viewport scrolling outside the image
        setSceneRect(scene->itemsBoundingRect());
        if (!metadata->isLoaded(currentImagePath)) {
//            qDebug() << "metadata not loaded for" << currentImagePath;
            QFileInfo fileInfo(currentImagePath);
            bool loadMeta = metadata->loadImageMetadata(fileInfo);
//            qDebug() << "LoadMetadata =" << loadMeta;
        }
        shootingInfo = metadata->getShootingInfo(currentImagePath) + "\n" +
                metadata->getTitle(currentImagePath);
//        qDebug() << "shootingInfo" << shootingInfo;

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
//    qDebug() << "elapsed:" << t.nsecsElapsed();

    // ignore any arrow key events that occurred while loading
//    isBusy = true;
//    qApp->processEvents();  // events trapped in MW::event, ignored when isBusy
//    isBusy = false;

    return isLoaded;
}

void ImageView::upgradeToFullSize()
{
/* Called after a delay by timer initiated in loadImage. Two prior conditions
are matched:

    ● If zoomed then the relative scroll position is set.
    ● If zoomFit then zoomFit is recalculated.

*/
    {
    #ifdef ISDEBUG
    qDebug() << "ImageView::upgradeToFullSize";
    #endif
    }
    loadFullSizeTimer->stop();
    pmItem->setPixmap(imageCacheThread->imCache.value(currentImagePath));
    setSceneRect(scene->itemsBoundingRect());
    isPreview = false;
    qreal prevZoomFit = zoomFit;
    zoomFit = getFitScaleFactor(centralWidget->rect(), pmItem->boundingRect());
    zoom *= (zoomFit / prevZoomFit);
    if (isFit) zoom = zoomFit;
    scale();
    if (isZoom) setScrollBars(scrollPct);
}

void ImageView::scale()
{
/* Scales the pixmap to zoom.  Panning is automatically to the cursor position
because setTransformationAnchor(QGraphicsView::AnchorUnderMouse).

● The scroll percentage is stored so it can be matched in the next image if it
is zoomed.
● Flags are set for the zoom condition.
● The cursor to set to pointer if not zoomed and hand if zoomed.
● The pick icon and shooting info text are relocated as necessary.
● The app status is updated.

*/
    {
    #ifdef ISDEBUG
    qDebug() << "ImageView::scale";
    #endif
    }
/*
    qDebug() << "isPreview =" << isPreview
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
    isZoom = (zoom > zoomFit);
    isFit = (zoom == zoomFit);
    if (isZoom) scrollPct = getScrollPct();
    wasZoomFit = zoom == zoomFit;
    if (isZoom) setCursor(Qt::OpenHandCursor);
    else setCursor(Qt::ArrowCursor);

    movePickIcon();
    moveShootingInfo(shootingInfo);
    emit updateStatus(true, "");
}

void ImageView::setFullDim()
{
/* Sets QSize full from metadata.  Req'd by setPreviewDim */
    {
    #ifdef ISDEBUG
    qDebug() << "ImageView::setFullDim";
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
    qDebug() << "ImageView::setPreviewDim";
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
    qDebug() << "ImageView::sceneBiggerThanView" << currentImagePath;
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
    qDebug() << "ImageView::getFitScaleFactor" << currentImagePath;
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
    qDebug() << "ImageView::setScrollBars" << currentImagePath;
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
    qDebug() << "ImageView::getScrollBarStatus" << currentImagePath;
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
    qDebug() << "ImageView::getScrollPct" << currentImagePath;
    #endif
    }
    getScrollBarStatus();
    return QPointF(scrl.hPct, scrl.vPct);
}

void ImageView::movePickIcon()
{
/*
The bright green thumbsUp pixmap shows the pick status for the current image.
This function locates the pixmap in the bottom corner of the image label as the
image is resized and zoomed, adjusting for the aspect ratio of the image and
size.
*/
    {
    #ifdef ISDEBUG
    qDebug() << "ImageView::movePickIcon";
    #endif
    }
    QPoint sceneBottomRight;            // bottom right corner of scene in view coord
    sceneBottomRight = mapFromScene(sceneRect().bottomRight());

    intSize p;
    p.w = pickLabel->width();           // width of the pick symbol
    p.h = pickLabel->height();          // height of the pick symbol
    int offset = 10;                    // offset pixels from the edge of image
    int x, y = 0;                       // top left coordinates of pick symbol

    int w;                              // width of window or image, whichever is smaller in view coord
    int h;                              // height of window or image, whichever is smaller in view coord

    // if the image view is not as wide as the window
    if (sceneBottomRight.x() < rect().width()) {
        x = sceneBottomRight.x() - p.w - offset;
        w = mapFromScene(sceneRect().bottomRight()).x() - mapFromScene(sceneRect().bottomLeft()).x();
    }
    else {
        x = rect().width() - p.w - offset;
        w = rect().width();
    }
\
    // if the image view is not as high as the window
    if (sceneBottomRight.y() < rect().height()) {
        y = sceneBottomRight.y() - p.h - offset;
        h = mapFromScene(sceneRect().bottomRight()).y() - mapFromScene(sceneRect().topRight()).y();
    }
    else {
        y = rect().height() - p.h - offset;
        h = rect().height();
    }

    // resize if necessary
    qreal f = 0.03;
    w *= f;
    h *= f;
    int d;                          // dimension of pick image
    w > h ? d = w : d = h;
    if (d < 20) d = 20;
    if (d > 40) d = 40;
    pickLabel->setPixmap(pickPixmap->scaled(d, d, Qt::KeepAspectRatio));

    pickLabel->move(x, y);
    editsLabel->move(x + p.w - offset, y + p.h - offset);
//    editsLabel->move(x + p.w - offset - d, y + p.h - offset - 0.20 * d);
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
    qDebug() << "ImageView::resizeEvent";
    #endif
    }
    QGraphicsView::resizeEvent(event);
    /* Resize scenarios

     If the view is zoomFit then keep zoomFit after resize

     If the view zoom is less than zoomFit no scaling unless resize to
     smaller than zoom amount - then zoomFit further resizing.

     If the view zoom is greater than zoomFit then no change
     */

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
    movePickIcon();
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
    qDebug() << "ImageView::thumbClick";
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
//    qDebug() << "getZoom():   zoom =" << zoom
//             << "x1 =" << x1 << "x2 =" << x2
//             << "calc zoom =" << calcZoom;
    return calcZoom;
}

void ImageView::zoomIn()
{
    {
    #ifdef ISDEBUG
    qDebug() << "ImageView::zoomIn";
    #endif
    }
    zoom *= (1.0 + zoomInc);
    qDebug() << "zoomInc" << zoomInc;
    zoom = zoom > zoomMax ? zoomMax: zoom;
    scale();
}

void ImageView::zoomOut()
{
    {
    #ifdef ISDEBUG
    qDebug() << "ImageView::zoomOut";
    #endif
    }
    zoom *= (1.0 - zoomInc);
    zoom = zoom < zoomMin ? zoomMin : zoom;
    scale();
}

void ImageView::zoom100()
{
    {
    #ifdef ISDEBUG
    qDebug() << "ImageView::zoom100";
    #endif
    }
    clickZoom = 1;
//    mouseZoomFit = false;
//    resizeImage();
}

void ImageView::zoomToFit()
{
    {
    #ifdef ISDEBUG
    qDebug() << "ImageView::zoomToFit";
    #endif
    }
    zoom = zoomFit;
    scale();
}

void ImageView::zoomTo(float zoomTo)
{
    {
    #ifdef ISDEBUG
    qDebug() << "ImageView::zoomTo";
    #endif
    }
    zoom = zoomTo;
    scale();
}

void ImageView::zoomToggle()
{
    {
    #ifdef ISDEBUG
    qDebug() << "ImageView::zoomToggle";
    #endif
    }
    if (zoom < zoomFit) zoom = zoomFit;
    else if (zoom == zoomFit) zoom = clickZoom;
    else if (isZoom) zoom = zoomFit;
    scale();
}

void ImageView::zoom50()
{
    {
    #ifdef ISDEBUG
    qDebug() << "ImageView::zoom50";
    #endif
    }
//    zoomTo(0.5);
    clickZoom = 0.5;
}

void ImageView::zoom200()
{
    {
    #ifdef ISDEBUG
    qDebug() << "ImageView::zoom200";
    #endif
    }
    zoomTo(2.0);
    clickZoom = 2.0;
}

//void ImageView::setClickZoom(float clickZoom)
//{
//    {
//    #ifdef ISDEBUG
//    qDebug() << "ImageView::setClickZoom";
//    #endif
//    }
//    this->clickZoom = clickZoom;
//}

void ImageView::rotateByExifRotation(QImage &image, QString &imageFullPath)
{
    {
    #ifdef ISDEBUG
    qDebug() << "ImageView::rotateByExifRotation";
    #endif
    }
    QTransform trans;
    int orientation = metadata->getImageOrientation(imageFullPath);

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
    qDebug() << "ImageView::transform";
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
    qDebug() << "ImageView::setInfo";
    #endif
    }

//    QPalette palette1 = infoLabelShadow->palette();
//    palette1.setColor(QPalette::WindowText, Qt::black);
//    infoLabelShadow->setPalette(palette1);

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

    QFont font( "Tahoma", 20);
//    QFont font( "Arial", 48);
//    font.setPointSize(100);
    font.setKerning(true);

//    QLabel *info = new QLabel;
//    info->setFont(font);
////    info->setText("TEST");
//    info->setAttribute(Qt::WA_TranslucentBackground);
////    info->setGraphicsEffect(infoEffect);
//    info->setText(infoString);
////    qDebug() << "info =" << info->text();
//    info->adjustSize();
////    // make a little wider to account for the drop shadow
//    info->resize(info->width()+10, info->height());
//    scene->addWidget(info);
//    info->move(x, y);
//    info->setVisible(true);


    infoDropShadow->setFont(font);      // not working
    int fontSize = 14;

    infoDropShadow->setStyleSheet("font: " + QString::number(fontSize) + "pt;");
    infoDropShadow->setText(infoString);
    infoDropShadow->adjustSize();
    // make a little wider to account for the drop shadow
    infoDropShadow->resize(infoDropShadow->width()+10, infoDropShadow->height()+10);
    infoDropShadow->move(x, y);
}

void ImageView::monitorCursorState()
{
    {
    #ifdef ISDEBUG
    qDebug() << "ImageView::monitorCursorState";
    #endif
    }
//    qDebug() << "ImageView::monitorCursorState";
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
    qDebug() << "ImageView::setCursorHiding";
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
//    qDebug() << "ImageView::compareZoomAtCoord";
//    #endif
//    }
////    qDebug() << "\n" << currentImagePath;
////    qDebug() << "ImageView::compareZoomAtCoord" << coord;
//    zoom = clickZoom;     // if zoomToFit then zoom reset in resize
////    mouseZoomFit = !mouseZoomFit;
//    f.w = imageLabel->geometry().width();
//    f.h = imageLabel->geometry().height();
//    mouse.x = imageLabel->geometry().topLeft().x() + coord.x() * f.w;
//    mouse.y = imageLabel->geometry().topLeft().y() + coord.y() * f.h;
////    qDebug() << "imageLabel->geometry().topLeft().x()" << imageLabel->geometry().topLeft().x()
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
    isMouseDrag = (scrollCount > 2);
//    qDebug() << "scrolling dx =" << dx << "dy =" << dy << scrollCount;
    QGraphicsView::scrollContentsBy(dx, dy);
}

// MOUSE CONTROL

void ImageView::dragMoveEvent(QDragMoveEvent *event)
{
    qDebug() << "drag";
}

// not used
void ImageView::mouseDoubleClickEvent(QMouseEvent *event)
{
    {
    #ifdef ISDEBUG
    qDebug() << "ImageView::mouseDoubleClickEvent";
    #endif
    }
    // placeholder function pending use

    //    isMouseDoubleClick = true;
    QWidget::mouseDoubleClickEvent(event);
}

void ImageView::mousePressEvent(QMouseEvent *event)
{
    {
    #ifdef ISDEBUG
    qDebug() << "ImageView::mousePressEvent";
    #endif
    }
    // bad things happen if no image when click
    if (currentImagePath.isEmpty()) return;
    isMouseDoubleClick = false;
    if (event->button() == Qt::LeftButton) {
        isLeftMouseBtnPressed = true;
        scrollCount = 0;
        mousePressPt.setX(event->x());
        mousePressPt.setY(event->y());
    }
    QGraphicsView::mousePressEvent(event);
}

void ImageView::mouseMoveEvent(QMouseEvent *event)
{
/* Pan the image during a mouse drag operation
*/
    {
    #ifdef ISDEBUG
//    qDebug() << "ImageView::mouseMoveEvent";
    #endif
    }
    if (isLeftMouseBtnPressed) {
        isMouseDrag = true;
        setCursor(Qt::ClosedHandCursor);
        horizontalScrollBar()->setValue(horizontalScrollBar()->value() -
                                       (event->x() - mousePressPt.x()));
        verticalScrollBar()->setValue(verticalScrollBar()->value() -
                                      (event->y() - mousePressPt.y()));
        mousePressPt.setX(event->x());
        mousePressPt.setY(event->y());
    }
    event->ignore();

//    QGraphicsView::mouseMoveEvent(event);
}

void ImageView::mouseReleaseEvent(QMouseEvent *event)
{
    {
    #ifdef ISDEBUG
    qDebug() << "ImageView::mouseReleaseEvent";
    #endif
    }
    isLeftMouseBtnPressed = false;
    if (isMouseDrag || isMouseDoubleClick) {
        isMouseDrag = false;
        if (isZoom) setCursor((Qt::OpenHandCursor));
        else setCursor(Qt::ArrowCursor);
        return;
    }
    if (!isZoom && zoom < zoomFit * 0.99)
        zoom = zoomFit;
    else
        isZoom ? zoom = zoomFit : zoom = clickZoom;
    scale();
    QGraphicsView::mouseReleaseEvent(event);
}

void ImageView::enterEvent(QEvent *event)
{
    {
    #ifdef ISDEBUG
    qDebug() << "ImageView::enterEvent";
    #endif
    }
    this->setFocus();
//    qDebug() << qApp->focusWidget() << imageIndex << thumbView->currentIndex();
    if (imageIndex != thumbView->currentIndex()) {
        thumbView->setCurrentIndex(imageIndex);
    }
}

// COPY AND PASTE

void ImageView::copyImage()
{
    {
    #ifdef ISDEBUG
    qDebug() << "ImageView::copyImage";
    #endif
    }
    //    QApplication::clipboard()->setImage(displayImage);
}


// not being used, but maybe in the future
static inline int bound0To255(int val)
{
    {
    #ifdef ISDEBUG
    qDebug() << "ImageView::bound0To255";
    #endif
    }
    return ((val > 255)? 255 : (val < 0)? 0 : val);
}

static inline int hslValue(double n1, double n2, double hue)
{
    {
    #ifdef ISDEBUG
    qDebug() << "ImageView::hslValue";
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
    qDebug() << "ImageView::rgbToHsl";
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
    qDebug() << "ImageView::hslToRgb";
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
