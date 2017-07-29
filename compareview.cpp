#include "compareview.h"
#include "global.h"

/*  OVERVIEW

This class manages a view of a scene containing a pixmap of the comparison
image in a grid of comparison images housed in compareImages. Each grid item
view is a separate instance of this class.

Terms:

    Current instance - has the focus and is thumbView->currentIndex
    Other instance - the rest if the grid instances
    Current location - the scroll percent location of the current instance
    Offset location - the offset from the current location for each instance

Each instance becomes the current instance when the mouse enters its grid
location and the thumbView->currentIndex is set.  Each instance can be panned
separately or in sync mode, where all instances pan together.  Because each
instance can be panned independently, the independent panning is recorded
as the offset to the synced pan location.

Mouse click zoom:

    Each image can be zoomed to a mouse location, which propagates to all the
    other instances, which also zoom to the same location. However, the
    location is relative to each instance, since each instance has a separate
    image, which may be a different size and have a different aspect ratio.
    Hence, location is defined as the percentage of the scroll range.

    ● action starts with mouse release in the current instance.
    ● scale the current instance with propagate = true
    ● emit signal to CompareImages to scale/pan all instances
    ● scale each instance with propagate = false (prevent ∞ loop)
    ● pan each instance by scroll percentage and offset correction

Synced panning:

    In the default pan mode all instances pan together.

    ● The pan is initiated by a panning action in the current instance, which
      triggers a wheel event.  The current instance is panned by the default
      QGraphicsView behavior.
    ● A signal is emitted from wheelEvent to CompareImages
    ● CompareImages passes the scroll percentage to each instance
    ● Each instance calculates the new scroll position based on the scroll
      percentage of the scene, corrected for any local offset

*/

CompareView::CompareView(QWidget *parent, QSize gridCell, Metadata *metadata,
                     ImageCache *imageCacheThread, ThumbView *thumbView)
{
    {
    #ifdef ISDEBUG
    qDebug() << "CompareView::CompareView";
    #endif
    }

    this->mainWindow = parent;
    this->gridCell = gridCell;
    this->metadata = metadata;
    this->imageCacheThread = imageCacheThread;
    this->thumbView = thumbView;
    this->setStyleSheet("QGraphicsView  {"
                        "margin:1; "
                        "border-style: solid; "
                        "border-width: 1; "
                        "border-color: rgb(111,111,111);}");

//    frame = new QFrame;
//    QLabel *g = new QLabel;
//    this->setFrame

    scene = new QGraphicsScene();
    pmItem = new QGraphicsPixmapItem;
    pmItem->setBoundingRegionGranularity(1);
    scene->addItem(pmItem);

    setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
    setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setScene(scene);

    pickLabel = new QLabel(this);
    pickLabel->setFixedSize(40,48);
    pickLabel->setAttribute(Qt::WA_TranslucentBackground);
    pickPixmap = new QPixmap(":/images/ThumbsUp48.png");
    // setPixmap during resize event
    pickLabel->setAlignment(Qt::AlignRight | Qt::AlignBottom);
    pickLabel->setVisible(false);

    isMouseDrag = false;
    isMouseDoubleClick = false;
    isMouseClickZoom = false;
    clickZoom = 1.0;
    zoomInc = 0.1;

    connect(horizontalScrollBar(), SIGNAL(valueChanged(int)), this, SLOT(scrollEvent()));
    connect(verticalScrollBar(), SIGNAL(valueChanged(int)), this, SLOT(scrollEvent()));
}

bool CompareView::loadImage(QModelIndex idx, QString fPath)
{
    {
    #ifdef ISDEBUG
    qDebug() << "CompareView::loadImage"  << currentImagePath;
    #endif
    }

     /* important to keep currentImagePath.  It is used to check if there isn't
     an image (when currentImagePath.isEmpty() == true) - for example when
     no folder has been chosen. */
    currentImagePath = fPath;
    imageIndex = idx;

    // No folder selected yet
    if (fPath == "") return false;

    bool isLoaded = false;

    // load the image from the image cache if available
    if (imageCacheThread->imCache.contains(fPath)) {
        pmItem->setPixmap(imageCacheThread->imCache.value(fPath));
        isLoaded = true;
    }
    else {
        // load the image from the image file, may need to wait a bit if another thread
        // reading file
        for (int i=0; i<100000; i++) {
            isLoaded = loadPixmap(fPath, displayPixmap);
            if (isLoaded) break;
        }
        if (isLoaded) {
            pmItem->setPixmap(displayPixmap);
        }
        else return false;
    }

    setSceneRect(scene->itemsBoundingRect());
    zoomFit = getFitScaleFactor(gridCell, pmItem->boundingRect());
    zoom = zoomFit;
    scale(false);

    return isLoaded;
}

bool CompareView::loadPixmap(QString &fPath, QPixmap &pm)
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
    qDebug() << "CompareView::loadPixmap";
    #endif
    }

    // IMPORTANT: this function also exists in the imageCache class and is
    // used there to cache images.

    // CompareView::loadPixmap and ImageCache::loadPixmap should be the same

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
            qDebug() << "CompareView::loadPixmap Success =" << success
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
              qDebug() << "CompareView::loadPixmap Success =" << success
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

void CompareView::zoomToPct(QPointF scrollPct, bool isZoom)
{
    {
    #ifdef ISDEBUG
    qDebug() << "CompareView::zoomToPct" << currentImagePath;
    #endif
    }
/* Called from compareImages after signal zoomFromPct has been emitted
   by another image in the compare group when a scale occurs.

   Note that the scrollPct coordinates are relative, as a percentage of the
   slider range, because each image in the compare group might have different
   size or aspect ratios.
*/
//    qDebug() << "CompareView::zoomToPct  scrollPct" << scrollPct
//             << "isZoom" << isZoom << currentImagePath;
    this->isZoom = isZoom;
    isZoom ? zoom = zoomFit : zoom = clickZoom;
    scale(false);
//    scrollPosPct = scrollPct;      // new position base for delta scrolls
    panToPct(scrollPct);
    getScrollBarStatus();
    reportScrollBarStatus();
}

void CompareView::scrollEvent()
{
    {
    #ifdef ISDEBUG
    qDebug() << "CompareView::scrollEvent " << currentImagePath;
    #endif
    }
    if (imageIndex == thumbView->currentIndex()) {
        if (propagate) emit panFromPct(getScrollDeltaPct(), imageIndex);
        else {
            getScrollBarStatus();
            scrollPosPct = QPointF(scrl.hPct, scrl.vPct);
        }
    }
}

//QPointF CompareView::getOffset(QPointF scrollPct)
//{
//    return scrollPct;
//}

QPointF CompareView::getSceneCoordFromPct(QPointF scrollPct)
{
    {
    #ifdef ISDEBUG
    qDebug() << "CompareView::getSceneCoordFromPct" << currentImagePath;
    #endif
    }
    return QPointF(scrollPct.x() * scene->width(),
                   scrollPct.y() * scene->height());
}

void CompareView::panToDeltaPct(QPointF delta)
{
    {
    #ifdef ISDEBUG
    qDebug() << "CompareView::panToDeltaPct" << currentImagePath;
    #endif
    }
//    qDebug() << "Delta pan" << delta << currentImagePath;
    getScrollBarStatus();
    setScrollBars(QPointF(scrl.hPct + delta.x(), scrl.vPct + delta.y()));
//    reportScrollBarStatus();
    return;
}

void CompareView::panToPct(QPointF scrollPct)
{
    {
    #ifdef ISDEBUG
    qDebug() << "CompareView::panToPct" << currentImagePath;
    #endif
    }
    qreal xPct = scrollPct.x();
    qreal yPct = scrollPct.y();
    qreal w = scene->width();
    qreal h = scene->height();
    qreal x = xPct * w;
    qreal y = yPct * h;
//    qDebug() << "CompareView::panToPct  xPct, yPct" << xPct << yPct
//             << "w" << w << "h" << h << "x" << x << "y" << y
//             << "zoom" << zoom << "zoomFit" << zoomFit
//             << currentImagePath;
//    centerOn(xPct * scene->width(), yPct * scene->height());
//    centerOn(x, y);

    setScrollBars(scrollPct);
    qDebug() << "panToPct:  scrollPct" << scrollPct;
//    reportScrollBarStatus();
    return;
}

void CompareView::setScrollBars(QPointF scrollPct)
{
    {
    #ifdef ISDEBUG
    qDebug() << "CompareView::setScrollBars" << currentImagePath;
    #endif
    }
//    qDebug() << "setScrollBars: scrollPct" << scrollPct << currentImagePath;
    getScrollBarStatus();
    scrl.hVal = scrl.hMin + scrollPct.x() * (scrl.hMax - scrl.hMin);
    scrl.vVal = scrl.vMin + scrollPct.y() * (scrl.vMax - scrl.vMin);
    horizontalScrollBar()->setValue(scrl.hVal);
    verticalScrollBar()->setValue(scrl.vVal);
}

void CompareView::getScrollBarStatus()
{
    {
    #ifdef ISDEBUG
    qDebug() << "CompareView::getScrolBarStatus" << currentImagePath;
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

void CompareView::reportScrollBarStatus()
{
    {
    #ifdef ISDEBUG
    qDebug() << "CompareView::reportScrollBarStatus" << currentImagePath;
    #endif
    }
    qDebug() << "ScrollBarStatus:" << currentImagePath << "\n"
             << "hMin" << scrl.hMin << "hMax" << scrl.hMax << "hVal" << scrl.hVal << "hPct" << scrl.hPct
             << "vMin" << scrl.vMin << "vMax" << scrl.vMax << "vVal" << scrl.vVal << "vPct" << scrl.vPct
             << "\n";
}

QPointF CompareView::getScrollDeltaPct()
{
    /* Only called from current instance of CompareView  */
    {
    #ifdef ISDEBUG
    qDebug() << "CompareView::getScrollDeltaPct" << currentImagePath;
    #endif
    }
    getScrollBarStatus();
    // difference between new and previous scroll position
    QPointF delta(scrl.hPct - scrollPosPct.x(), scrl.vPct - scrollPosPct.y());
//    qDebug() << "getScrollDeltaPct:  delta" << delta << "scrollPosPct" << scrollPosPct;
//    reportScrollBarStatus();
    // update current scroll position
    scrollPosPct = QPointF(scrl.hPct, scrl.vPct);
//    reportScrollBarStatus();
    return delta;
}

QPointF CompareView::getScrollPct()
{
    /* Only called from current instance of CompareView  */
    {
    #ifdef ISDEBUG
    qDebug() << "CompareView::getScrollPct" << currentImagePath;
    #endif
    }
//    qDebug() << "CompareView::getScrollPct" << currentImagePath;
    getScrollBarStatus();
//    reportScrollBarStatus();
    return QPointF(scrl.hPct, scrl.vPct);
}

//QPointF CompareView::getMousePct()
//{
//    {
//    #ifdef ISDEBUG
//    qDebug() << "CompareView::getScrollPct" << currentImagePath;
//    #endif
//    }
//    QPointF p(mapToScene(mousePt));
//    QPointF scrollPct(p.x() / scene->width(), p.y() / scene->height());
//    qDebug() << "\nCompareView::getMousePct  Mouse" << mousePt
//             << "p(mapToScene(mousePt))" << p << "scrollPct" << scrollPct;
//    panToPct(scrollPct);
//    return getScrollPct();
////    return QPointF(scrollPct);
//}

qreal CompareView::getZoom()
{
    {
    #ifdef ISDEBUG
    qDebug() << "CompareView::getZoom" << currentImagePath;
    #endif
    }
    // use view center to make sure inside scene item
    qreal x1 = mapToScene(rect().center()).x();
    qreal x2 = mapToScene(rect().center() + QPoint(1, 0)).x();
    qreal calcZoom = 1.0 / (x2 - x1);
    return calcZoom;
}

qreal CompareView::getFitScaleFactor(QSize container, QRectF content)
{
    {
    #ifdef ISDEBUG
    qDebug() << "CompareView::getFitScaleFactor" << currentImagePath;
    #endif
    }
    qreal hScale = ((qreal)container.width() - 2) / content.width();
    qreal vScale = ((qreal)container.height() - 2) / content.height();
    return (hScale < vScale) ? hScale : vScale;
}

void CompareView::scale(bool okayToPropagate)
{
/* If called from mouse release then panning is automatic because
setTransformationAnchor(QGraphicsView::AnchorUnderMouse).
*/
    {
    #ifdef ISDEBUG
    qDebug() << "CompareView::scale" << currentImagePath;
    #endif
    }
    matrix.reset();
    matrix.scale(zoom, zoom);
    setMatrix(matrix);
    if (okayToPropagate) {
//        qDebug() << "Propagating from" << currentImagePath;
        getScrollBarStatus();
//        reportScrollBarStatus();
        scrollPosPct = QPointF(scrl.hPct, scrl.vPct);      // new position base for delta scrolls
//        qDebug() << "scrollPosPct" << scrollPosPct;
        emit zoomFromPct(scrollPosPct, imageIndex, isZoom);
//        emit align(scrollPosPct, imageIndex);
    }
    isZoom = (zoom > zoomFit);
    if (isZoom) setCursor(Qt::OpenHandCursor);
    else setCursor(Qt::ArrowCursor);
    movePickIcon();
}

void CompareView::movePickIcon()
{
/* The bright green thumbsUp pixmap shows the pick status for the current
image. This function locates the pixmap in the bottom corner of the image label
as the image is resized and zoomed, adjusting for the aspect ratio of the
image.*/
    {
    #ifdef ISDEBUG
    qDebug() << "CompareView::movePickIcon" << currentImagePath;
    #endif
    }
    QPoint sceneBottomRight;            // bottom right corner of scene in view coord

    sceneBottomRight = mapFromScene(sceneRect().bottomRight());

    int pw = pickLabel->width();        // width of the pick symbol
    int ph = pickLabel->height();       // height of the pick symbol
    int offset = 10;                    // offset pixels from the edge of image
    int x, y = 0;                       // top left coordinates of pick symbol

    // if the image view is not as wide as the window
    if (sceneBottomRight.x() < rect().width())
        x = sceneBottomRight.x() - pw - offset;
    else x = rect().width() - pw - offset;

    // if the image view is not as high as the window
    if (sceneBottomRight.y() < rect().height())
        y = sceneBottomRight.y() - ph - offset;
    else y = rect().height() - ph - offset;

    pickLabel->move(x, y);

    qDebug() << "sceneBottomRight" << sceneBottomRight << "rect()" << rect()
             << "x" << x << "y" << y;

}

void CompareView::resizeEvent(QResizeEvent *event)
{
    {
    #ifdef ISDEBUG
    qDebug() << "CompareView::resizeEvent" << currentImagePath;
    #endif
    }
    QGraphicsView::resizeEvent(event);
    zoomFit = getFitScaleFactor(gridCell, pmItem->boundingRect());
    if (getZoom() <= zoomFit) {
        zoom = zoomFit;
        scale(false);
    }
    qreal f = 0.03;
    int w = width() * f;
    int h = height() * f;
    int d;
    w > h ? d = w : d = h;
    if (d < 20) d = 20;
    if (d > 40) d = 40;
    pickLabel->setPixmap(pickPixmap->scaled(d, d, Qt::KeepAspectRatio));
    movePickIcon();
}

void CompareView::zoomIn()
{
    {
    #ifdef ISDEBUG
    qDebug() << "CompareView::zoomIn" << currentImagePath;
    #endif
    }
    qDebug() << "CompareView::zoomIn" << currentImagePath;
    zoom *= (1.0 + zoomInc);
    zoom = zoom > zoomMax ? zoomMax: zoom;
    scale(false);
}

void CompareView::zoomOut()
{
    {
    #ifdef ISDEBUG
    qDebug() << "CompareView::zoomOut" << currentImagePath;
    #endif
    }
    qDebug() << "CompareView::zoomOut" << currentImagePath;
    zoom *= (1.0 - zoomInc);
    zoom = zoom < zoomMin ? zoomMin : zoom;
    scale(false);
}

void CompareView::zoom100()
{
    {
    #ifdef ISDEBUG
    qDebug() << "CompareView::zoom100" << currentImagePath;
    #endif
    }
    clickZoom = 1;
    zoom = clickZoom;
    zoom = zoom < zoomMin ? zoomMin : zoom;
    scale(false);
}

void CompareView::zoomToFit()
{
    {
    #ifdef ISDEBUG
    qDebug() << "CompareView::zoomToFit" << currentImagePath;
    #endif
    }
//    mouseZoomFit = true;
//    resizeImage();
}

void CompareView::zoomTo(float zoomTo)
{
    {
    #ifdef ISDEBUG
    qDebug() << "CompareView::zoomTo" << currentImagePath;
    #endif
    }
    zoom = zoomTo;
    zoom < 0.05 ? 0.05 : zoom;
    zoom > 8 ? 8: zoom;
//    mouseZoomFit = false;
//    resizeImage();
}

void CompareView::zoomToggle()
{
    {
    #ifdef ISDEBUG
    qDebug() << "CompareView::zoomToggle" << currentImagePath;
    #endif
    }
//    qDebug() << "zoomToggle  isZoom =" << isZoom;
//    mouseZoomFit = isZoom;
    if (!isZoom) zoom = clickZoom;
//    resizeImage();
}

void CompareView::zoom50()
{
    {
    #ifdef ISDEBUG
    qDebug() << "CompareView::zoom50" << currentImagePath;
    #endif
    }
    clickZoom = 0.5;
    zoom = clickZoom;
    zoom = zoom < zoomMin ? zoomMin : zoom;
    scale(false);
}

void CompareView::zoom200()
{
    {
    #ifdef ISDEBUG
    qDebug() << "CompareView::zoom200" << currentImagePath;
    #endif
    }
    clickZoom = 2;
    zoom = clickZoom;
    zoom = zoom < zoomMin ? zoomMin : zoom;
    scale(false);
}

void CompareView::setClickZoom(float clickZoom)
{
    {
    #ifdef ISDEBUG
    qDebug() << "CompareView::setClickZoom" << currentImagePath;
    #endif
    }
    this->clickZoom = clickZoom;
}

void CompareView::resetMouseClickZoom()
{
    isMouseClickZoom = false;
}

// EVENTS

void CompareView::wheelEvent(QWheelEvent *wheelEvent)
{
    {
    #ifdef ISDEBUG
    qDebug() << "CompareView::wheelEvent" << currentImagePath;
    #endif
    }
    propagate = true;

    if (imageIndex == thumbView->currentIndex()) {
        if(wheelEvent->modifiers() & Qt::ShiftModifier) {
//            qDebug() << "wheelEvent shiftModifier";
            propagate = false;
        }
        else {
//            qDebug() << "CompareView::wheelEvent emitting" << currentImagePath;
//            emit panFromPct(getScrollPct(), imageIndex);
        }
    }
    QGraphicsView::wheelEvent(wheelEvent);
}

void CompareView::mousePressEvent(QMouseEvent *event)
{
    {
    #ifdef ISDEBUG
    qDebug() << "CompareView::mousePressEvent" << currentImagePath;
    #endif
    }
    // bad things happen if no image when click
    if (currentImagePath.isEmpty()) return;

    QGraphicsView::mousePressEvent(event);
}

void CompareView::mouseReleaseEvent(QMouseEvent *event)
{
    {
    #ifdef ISDEBUG
    qDebug() << "CompareView::mouseReleaseEvent" << currentImagePath;
    #endif
    }
//    qDebug() << "CompareView::mouseReleaseEvent  mouse x, y"
//             << event->localPos();

//    qDebug() << "\n00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000\n";

    mousePt = event->localPos().toPoint();
    isZoom ? zoom = zoomFit : zoom = clickZoom;
    isMouseClickZoom = true;
    propagate = false;
    scale(true);
    propagate = true;
    QGraphicsView::mouseReleaseEvent(event);
}

void CompareView::enterEvent(QEvent *event)
{
    {
    #ifdef ISDEBUG
    qDebug() << "CompareView::enterEvent" << currentImagePath;
    #endif
    }
    this->setFocus();
    if (imageIndex != thumbView->currentIndex()) {
        thumbView->setCurrentIndex(imageIndex);
        this->setStyleSheet("QGraphicsView  {"
                            "margin:1; "
                            "border-style: solid; "
                            "border-width: 1; "
                            "border-color: rgb(225,225,225);}");
    }
}

void CompareView::leaveEvent(QEvent *event)
{
    {
    #ifdef ISDEBUG
    qDebug() << "CompareView::enterEvent" << currentImagePath;
    #endif
    }
    this->setStyleSheet("QGraphicsView  {"
                        "margin:1; "
                        "border-style: solid; "
                        "border-width: 1; "
                        "border-color: rgb(111,111,111);}");
}
