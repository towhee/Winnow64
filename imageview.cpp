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


The view scaling is controlled by the zoomFactor.  When zoomFactor = 1
the view is as 100%, the same as the original image.

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
//    this->isCompareMode = isCompareMode;

    scene = new QGraphicsScene();
    pmItem = new QGraphicsPixmapItem;
//    pmItem->setFlag(QGraphicsItem::ItemIsMovable, true);
    pmItem->setBoundingRegionGranularity(1);
//    infoItem = new QGraphicsTextItem(pmItem);
//    infoItem->setHtml("Shooting Info");
    scene->addItem(pmItem);

//    setDragMode(QGraphicsView::ScrollHandDrag);
//    setOptimizationFlags(QGraphicsView::DontSavePainterState);
//    setViewportUpdateMode(QGraphicsView::SmartViewportUpdate);
//    setViewportUpdateMode(QGraphicsView::FullViewportUpdate);
//    setTransformationAnchor(QGraphicsView::AnchorViewCenter);
//    setAlignment(Qt::AlignCenter);
    setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
    setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
//    setMouseTracking(true);
    setScene(scene);

    cursorIsHidden = false;
    moveImageLocked = false;
//    setMouseTracking(true);

    infoDropShadow = new DropShadowLabel(this);
    infoDropShadow->setVisible(isShootingInfoVisible);
    infoDropShadow->setAttribute(Qt::WA_TranslucentBackground);

    infoLabel = new QLabel(this);
    infoLabel->setVisible(isShootingInfoVisible);
    infoLabelShadow = new QLabel(this);
    infoLabelShadow->setVisible(isShootingInfoVisible);
    infoLabel->setAttribute(Qt::WA_TranslucentBackground);
    infoLabelShadow->setAttribute(Qt::WA_TranslucentBackground);

    pickLabel = new QLabel(this);
    pickLabel->setFixedSize(40,48);
    pickLabel->setAttribute(Qt::WA_TranslucentBackground);
    pickPixmap = new QPixmap(":/images/ThumbsUp48.png");
    // setPixmap during resize event
    pickLabel->setAlignment(Qt::AlignRight | Qt::AlignBottom);
    pickLabel->setVisible(false);

    QGraphicsOpacityEffect *infoEffect = new QGraphicsOpacityEffect;
    infoEffect->setOpacity(0.8);
    infoLabel->setGraphicsEffect(infoEffect);
    infoLabelShadow->setGraphicsEffect(infoEffect);
    infoDropShadow->setGraphicsEffect(infoEffect);

    mouseMovementTimer = new QTimer(this);
    connect(mouseMovementTimer, SIGNAL(timeout()), this, SLOT(monitorCursorState()));

    loadFullSizeTimer = new QTimer(this);
    loadFullSizeTimer->setInterval(500);
    connect(loadFullSizeTimer, SIGNAL(timeout()), this, SLOT(loadFullSize()));

//    mouseZoomFit = true;
    isMouseDrag = false;
    isMouseDoubleClick = false;
    firstImageLoaded = true;
}

bool ImageView::loadImage(QModelIndex idx, QString fPath)
{
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
//    qDebug() << "\n";

    // load the image from the image cache if available
    if (imageCacheThread->imCache.contains(fPath)) {
        // load preview from cache
//        t.restart();
//        qDebug() << "Elapsed between images" << t1.nsecsElapsed();
//        t1.restart();

        bool tryPreview = true;     // for testing
        loadFullSizeTimer->stop();

        // get preview size from stored metadata to decide if load preview or full
        setFullDim();               // req'd by setPreviewDim()
        setPreviewDim();            // defines QSize preview
        bool previewBigEnough = getFitScaleFactor(rect(), QRect(QPoint(0,0), preview)) > zoom;

        if (tryPreview && imageCacheThread->imCache.contains(fPath + "_Preview") &&
                previewBigEnough) {
//            qDebug() << "Load preview" << currentImagePath;
            pmItem->setPixmap(imageCacheThread->imCache.value(fPath + "_Preview"));
            isPreview = true;
            isLoaded = true;
            loadFullSizeTimer->start();
        }
        // load full size image from cache
        else {
//            qDebug() << "Load full size" << currentImagePath;
            pmItem->setPixmap(imageCacheThread->imCache.value(fPath));
            isPreview = false;
            isLoaded = true;
        }
    }
    else {
        // load the image from the image file, may need to wait a bit if another thread
        // reading file
        for (int i=0; i<100000; i++) {
            isLoaded = loadPixmap(fPath, displayPixmap);
            if (isLoaded) break;
        }
        if (isLoaded) {
//            qDebug() << "Loaded from file.   displayPixmap.size()" << displayPixmap.size();
    //        displayPixmap.setDevicePixelRatio(G::devicePixelRatio);
            pmItem->setPixmap(displayPixmap);
            isPreview = false;
        }
        else return false;
    }

    setSceneRect(scene->itemsBoundingRect());
    shootingInfo = metadata->getShootingInfo(currentImagePath);

    if (!firstImageLoaded) {
        zoomFit = getFitScaleFactor(centralWidget->rect(), pmItem->boundingRect());
        zoom = zoomFit;
        scale();
    }
//    qDebug() << "elapsed:" << t.nsecsElapsed();

    return isLoaded;
}

void ImageView::loadFullSize()
{
//    qDebug() << "Loading full size after timer";
    loadFullSizeTimer->stop();
    pmItem->setPixmap(imageCacheThread->imCache.value(currentImagePath));
    setSceneRect(scene->itemsBoundingRect());
    isPreview = false;
    zoomFit = getFitScaleFactor(centralWidget->rect(), pmItem->boundingRect());
    zoom = zoomFit;
    scale();
}

bool ImageView::loadPixmap(QString &fPath, QPixmap &pm)
{
/*  Reads the embedded jpg (known offset and length) and converts it into a
    pixmap.

    This function is dependent on metadata being updated first.  Metadata is
    updated by the mdCache thread that runs every time a new folder is
    selected. It has a sister function in the imageCache thread that stores
    pixmaps in the heap.

    Most of the time the pixmap will be obtained from the imageCache, but
    when the image has yet to be cached this function is called.  This often
    happens when a new folder is selected and the program is trying to load
    the metadata, thumbnail and image caches plus show the first image in the
    folder.

    Since this function, in the main program thread, may be competing with the
    cache building it will retry attempting to load for a given period of time
    as the image file may be locked by one of the cache builders.

    If it succeeds in opening the file it still has to read the embedded jpg and
    convert it to a pixmap.  If this fails then either the file format is not
    being properly read or the file is corrupted.  In this case the metadata
    will be updated to show file not readable.
*/
    {
    #ifdef ISDEBUG
    qDebug() << "ImageView::loadPixmap";
    #endif
    }

    // IMPORTANT: this function also exists in the imageCache class and is
    // used there to cache images.

    // ImageView::loadPixmap and ImageCache::loadPixmap should be the same

    bool success = false;
    int totDelay = 500;     // milliseconds
    int msDelay = 0;        // total incremented delay
    int msInc = 10;         // amount to increment each try

    QString err;            // type of error

    ulong offsetFullJpg = 0;
    QImage image;
    QFileInfo fileInfo(fPath);
    QString ext = fileInfo.completeSuffix().toLower();
    QFile imFile(fPath);

    if (metadata->rawFormats.contains(ext)) {
        // raw files not handled by Qt
        do {
            // Check if metadata has been cached for this image
            offsetFullJpg = metadata->getOffsetFullJPG(fPath);
            if (offsetFullJpg == 0) {
                metadata->loadImageMetadata(fPath);
                //try again
                offsetFullJpg = metadata->getOffsetFullJPG(fPath);
            }
            // try to read the file
            if (offsetFullJpg > 0) {
                if (imFile.open(QIODevice::ReadOnly)) {
                    bool seekSuccess = imFile.seek(offsetFullJpg);
                    if (seekSuccess) {
                        QByteArray buf = imFile.read(metadata->getLengthFullJPG(fPath));
                        if (image.loadFromData(buf, "JPEG")) {
                            imFile.close();
                            success = true;
                        }
                        else {
                            err = "Could not read image from buffer";
                            break;
                        }
                    }
                    else {
                        err = "Illegal offset to image";
                        break;
                    }
                }
                else {
                    err = "Could not open file for image";    // try again
                    QThread::msleep(msInc);
                    msDelay += msInc;
                }
            }
            /*
            qDebug() << "ImageView::loadPixmap Success =" << success
                     << "msDelay =" << msDelay
                     << "offsetFullJpg =" << offsetFullJpg
                     << "Attempting to load " << imageFullPath;
                     */
        }
        while (msDelay < totDelay && !success);
    }
    else {
        // cooked files like tif, png etc
        do {
            // check if file is locked by another process
            if (imFile.open(QIODevice::ReadOnly)) {
                // close it to allow qt load to work
                imFile.close();
                // directly load the image using qt library
                success = image.load(fPath);
                if (!success) {
                    err = "Could not read image";
                    break;
                }
            }
            else {
                err = "Could not open file for image";    // try again
                QThread::msleep(msInc);
                msDelay += msInc;
            }
              /*
              qDebug() << "ImageView::loadPixmap Success =" << success
                       << "msDelay =" << msDelay
                       << "offsetFullJpg =" << offsetFullJpg
                       << "Attempting to load " << imageFullPath;
                       */
        }
        while (msDelay < totDelay && !success);
    }

    // rotate if portrait image
    QTransform trans;
    int orientation = metadata->getImageOrientation(fPath);
    if (orientation) {
        switch(orientation) {
            case 6:
                trans.rotate(90);
                image = image.transformed(trans, Qt::SmoothTransformation) ;
                break;
            case 8:
                trans.rotate(270);
                image = image.transformed(trans, Qt::SmoothTransformation);
                break;
        }
    }

    pm = QPixmap::fromImage(image);

    // must adjust pixmap dpi in case retinal display macs
//    pm.setDevicePixelRatio(GData::devicePixelRatio);

    // record any errors
    if (!success) metadata->setErr(fPath, err);

    return success;
}

void ImageView::scale()
{
//    qDebug() << "isPreview =" << isPreview
//             << "zoom =" << zoom
//             << "zoomFit =" << zoomFit
//             << "previewScaleMax = " << previewScaleMax
//             << "rect().width() =" << rect().width()
//             << "sceneRect().width() =" << sceneRect().width();
    matrix.reset();
    matrix.scale(zoom, zoom);
    setMatrix(matrix);
    isZoom = (zoom > zoomFit);
    wasZoomFit = zoom == zoomFit;
    if (isZoom) setCursor(Qt::OpenHandCursor);
    else setCursor(Qt::ArrowCursor);

    movePickIcon();
    moveShootingInfo(shootingInfo);
//    QString msg = QString::number(v.w) + ", " + QString::number(v.h);
    emit updateStatus(true, "");
}

qreal ImageView::getPreviewToFull()
{
    return metadata->getWidth(currentImagePath) /
              imageCacheThread->getPreviewSize().width();
}

void ImageView::setFullDim()
{
    full.setWidth(metadata->getWidth(currentImagePath));
    full.setHeight(metadata->getHeight(currentImagePath));
}

void ImageView::setPreviewDim()
{
    preview = imageCacheThread->getPreviewSize();
    qreal fullAspectRatio = (qreal)(full.height()) / full.width();
    if (full.width() > full.height()) {
        preview.setHeight(preview.width() * fullAspectRatio);
    }
    else {
        preview.setWidth(preview.height() / fullAspectRatio);
    }
}

//qreal ImageView::getPreviewScaleMax()
//{
//    {
//    #ifdef ISDEBUG
//    qDebug() << "ImageView::getPreviewScaleMax";
//    #endif
//    }
//    return zoomFit;

//    // use metadata to determine before loading preview


//    QRect a = rect();
//    QSize b = imageCacheThread->getPreviewSize();
//    QRect previewRect(QPoint(0,0), imageCacheThread->getPreviewSize());
//    qreal previewFit = getFitScaleFactor(rect(), previewRect);
//    qreal c = getPreviewToFull();
//    return previewFit * getPreviewToFull();

////    QSize previewSize = imageCacheThread->getPreviewSize();
////    int pWidth = previewSize.width();
////    int pHeight = previewSize.height();
////    int fWidth = metadata->getWidth(currentImagePath);
////    int fHeight = metadata->getHeight(currentImagePath);
////    qreal xScale = (qreal)pWidth / fWidth;
////    qreal yScale = (qreal)pHeight / fHeight;
////    return xScale < yScale ? xScale : yScale;
//}

//bool ImageView::previewFitsZoom()
//{
//    {
//    #ifdef ISDEBUG
//    qDebug() << "ImageView::previewFitsZoom";
//    #endif
//    }
//    return zoom < previewScaleMax;

////    QSize previewSize = imageCacheThread->getPreviewSize();
////    if (previewSize.width() > pmItem->pixmap().width() * zoom &&
////        previewSize.height() > pmItem->pixmap().height() * zoom)
////        return true;
////    else return false;
//}

//QSize ImageView::imageSize()
//{
//    {
//    #ifdef ISDEBUG
//    qDebug() << "ImageView::imageSize";
//    #endif
//    }
//    return QSize(pmItem->pixmap().size());
//}

bool ImageView::sceneBiggerThanView()
{
//    if(rect().contains(sceneRect().toRect())) return false;
//    else return true;

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
    qreal hScale = ((qreal)container.width() - 2) / content.width();
    qreal vScale = ((qreal)container.height() - 2) / content.height();
    return (hScale < vScale) ? hScale : vScale;
}

void ImageView::movePickIcon()
{
/* The bright green thumbsUp pixmap shows the pick status for the current
image. This function locates the pixmap in the bottom corner of the image label
as the image is resized and zoomed, adjusting for the aspect ratio of the
image.*/
    {
    #ifdef ISDEBUG
    qDebug() << "ImageView::movePickIcon";
    #endif
    }
    QPoint sceneBottomRight;            // bottom right corner of scene in view coord

    sceneBottomRight = mapFromScene(sceneRect().bottomRight());

//    qDebug() << "sceneBottomRight" << sceneBottomRight << "rect()" << rect();

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
}

void ImageView::resizeEvent(QResizeEvent *event)
{
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


    qDebug() << "*************************************** RESIZE EVENT **********************************************";
    qDebug() << "centralWidget->rect()" << centralWidget->rect()
             << "pmItem->boundingRect()" << pmItem->boundingRect();

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
        int h1 = horizontalScrollBar()->minimum();
        int h2 = horizontalScrollBar()->maximum();
        horizontalScrollBar()->setValue(h1 + xPct * (h2 - h1));

        int v1 = verticalScrollBar()->minimum();
        int v2 = verticalScrollBar()->maximum();
        verticalScrollBar()->setValue(v1 + xPct * (v2 - v1));
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
//    mouseZoomFit = true;
//    resizeImage();
}

void ImageView::zoomTo(float zoomTo)
{
    {
    #ifdef ISDEBUG
    qDebug() << "ImageView::zoomTo";
    #endif
    }
    zoom = zoomTo;
    zoom < 0.05 ? 0.05 : zoom;
    zoom > 8 ? 8: zoom;
//    mouseZoomFit = false;
//    resizeImage();
}

void ImageView::zoomToggle()
{
    {
    #ifdef ISDEBUG
    qDebug() << "ImageView::zoomToggle";
    #endif
    }
    qDebug() << "zoomToggle  isZoom =" << isZoom;
//    mouseZoomFit = isZoom;
    if (!isZoom) zoom = clickZoom;
//    resizeImage();
}

void ImageView::zoom50()
{
    {
    #ifdef ISDEBUG
    qDebug() << "ImageView::zoom50";
    #endif
    }
    zoomTo(0.5);
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

    QPalette palette1 = infoLabelShadow->palette();
    palette1.setColor(QPalette::WindowText, Qt::black);
    infoLabelShadow->setPalette(palette1);

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

    QFont font( "Tahoma", 16);
    font.setKerning(true);
    infoDropShadow->setFont(font);
    infoDropShadow->setText(infoString);
    infoDropShadow->adjustSize();
    // make a little wider to account for the drop shadow
    infoDropShadow->resize(infoDropShadow->width()+10, infoDropShadow->height());
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


// MOUSE CONTROL

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
//        setMouseMoveData(true, event->x(), event->y());
//        int x = event->localPos().x();
//        int y = event->localPos().y();
//        event->accept();
    }
    QGraphicsView::mousePressEvent(event);
}

void ImageView::mouseReleaseEvent(QMouseEvent *event)
{
    {
    #ifdef ISDEBUG
    qDebug() << "ImageView::mouseReleaseEvent";
    #endif
    }
    if (isMouseDrag || isMouseDoubleClick) {
        isMouseDrag = false;
        return;
    }
    if (!isZoom && zoom < zoomFit * 0.99)
        zoom = zoomFit;
    else
        isZoom ? zoom = zoomFit : zoom = clickZoom;
    scale();
    QGraphicsView::mouseReleaseEvent(event);
}

void ImageView::setMouseMoveData(bool lockMove, int lMouseX, int lMouseY)
{
    {
    #ifdef ISDEBUG
    qDebug() << "ImageView::setMouseMoveData";
    #endif
    }
    moveImageLocked = lockMove;
    mouse.x = lMouseX;
    mouse.y = lMouseY;
//    w.x = imageLabel->pos().x();
//    w.y = imageLabel->pos().y();
/*    qDebug() << "mouse x =" << mouse.x << "y =" << mouse.y
            << "w.x = imageLabel->pos().x()" << w.x
             << "w.y = imageLabel->pos().y()" << w.y;
             */
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

void ImageView::mouseMoveEvent(QMouseEvent *event)
{
/* Pan the image during a mouse drag operation
*/
    {
    #ifdef ISDEBUG
    qDebug() << "ImageView::mouseMoveEvent";
    #endif
    }
// seems to force two mouse clicks after mouse movement to zoiom

//    isMouseDrag = true;

//    if (moveImageLocked) {
//        QPoint delta;
//        delta.setX(event->pos().x() - mouse.x);
//        delta.setY(event->pos().y() - mouse.y);
////        deltaMoveImage(delta);
//        if (isCompareMode && event->modifiers() != Qt::ShiftModifier) {
//            emit comparePan(delta, imageIndex);
//        }
//    }
    QGraphicsView::mouseMoveEvent(event);
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
