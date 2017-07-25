#include "compareview.h"

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
//    this->setStyleSheet("QGraphicsView {border-width: 2; color: yellow;}");     // not working

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
    thumbsUp.load(":/images/ThumbsUp48.png");
    pickLabel->setPixmap(QPixmap::fromImage(thumbsUp));
    pickLabel->setVisible(false);

    isMouseDrag = false;
    isMouseDoubleClick = false;

    connect(horizontalScrollBar(), SIGNAL(valueChanged(int)), this, SLOT(scrollEvent()));
    connect(verticalScrollBar(), SIGNAL(valueChanged(int)), this, SLOT(scrollEvent()));
}

bool CompareView::loadImage(QModelIndex idx, QString fPath)
{
    {
    #ifdef ISDEBUG
    qDebug() << "CompareView::loadImage";
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
    zoom = getFitScaleFactor(gridCell, pmItem->boundingRect());
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
    qDebug() << "CompareView::zoomToPct";
    #endif
    }
/* Called from compareImages after signal zoomFromPct has been emitted
   by another image in the compare group when a scale occurs.

   Note that the scrollPct coordinates are relative, as a percentage of the
   slider range, because each image in the compare group might have different
   size or aspect ratios.
*/
    qDebug() << "CompareView::zoomToPct  scrollPct" << scrollPct
             << "isZoom" << isZoom << currentImagePath;
    this->isZoom = isZoom;
    isZoom ? zoom = zoomFit : zoom = clickZoom;
    scale(false);
    /*if (isZoom)*/ panToPct(scrollPct);
}

void CompareView::panToPct(QPointF scrollPct)
{
    {
    #ifdef ISDEBUG
    qDebug() << "CompareView::panToPct";
    #endif
    }
    qreal xPct = scrollPct.x();
    qreal yPct = scrollPct.y();
    qreal w = scene->width();
    qreal h = scene->height();
    qreal x = xPct * w;
    qreal y = yPct * h;
    qDebug() << "CompareView::panToPct  xPct, yPct" << xPct << yPct
             << "w" << w << "h" << h << "x" << x << "y" << y
             << "zoom" << zoom << "zoomFit" << zoomFit;
//    centerOn(xPct * scene->width(), yPct * scene->height());
    centerOn(x, y);
    return;
}

void CompareView::scrollEvent()
{
    {
    #ifdef ISDEBUG
    qDebug() << "CompareView::scrollEvent";
    #endif
    }
    if (hasFocus()) emit panFromPct(getScrollPct(), imageIndex);
}

QPointF CompareView::getSceneCoordFromPct(QPointF scrollPct)
{
    {
    #ifdef ISDEBUG
    qDebug() << "CompareView::getSceneCoordFromPct";
    #endif
    }
    return QPointF(scrollPct.x() * scene->width(),
                   scrollPct.y() * scene->height());
}

QPointF CompareView::getScrollPct()
{
    {
    #ifdef ISDEBUG
    qDebug() << "CompareView::getScrollPct";
    #endif
    }
    int h1 = horizontalScrollBar()->minimum();
    int h2 = horizontalScrollBar()->maximum();
    int h = horizontalScrollBar()->value();
    qreal xPct = qreal(h - h1) / (h2 - h1);
    int v1 = verticalScrollBar()->minimum();
    int v2 = verticalScrollBar()->maximum();
    int v = verticalScrollBar()->value();
    qreal yPct = qreal(v - v1) / (v2 - v1);
    QPointF scrollPct(xPct, yPct);
    qDebug() << "CompareView::getScrollPct()  xPct, yPct" << xPct << yPct
             << "horizontal scroll value" << horizontalScrollBar()->value();
    panToPct(scrollPct);
    return QPointF(scrollPct);
}

QPointF CompareView::getMousePct()
{
    {
    #ifdef ISDEBUG
    qDebug() << "CompareView::getScrollPct";
    #endif
    }
    QPointF p(mapToScene(mousePt));
    qDebug() << "CompareView::getMousePct  Mouse" << mousePt
             << "p(mapToScene(mousePt))" << p << mapToScene(mousePt);
    QPointF scrollPct(p.x() / scene->width(), p.y() / scene->height());
    panToPct(scrollPct);
    return QPointF(scrollPct);
}



//QPointF CompareView::getScrollPct()
//{
//    int h1 = horizontalScrollBar()->minimum();
//    int h2 = horizontalScrollBar()->maximum();
//    int h = horizontalScrollBar()->value();
//    qreal xPct = qreal(h - h1) / (h2 - h1);
//    int v1 = verticalScrollBar()->minimum();
//    int v2 = verticalScrollBar()->maximum();
//    int v = verticalScrollBar()->value();
//    qreal yPct = qreal(v - v1) / (v2 - v1);
//    qDebug() << "CompareView::getScrollPct()  xPct, yPct" << xPct << yPct
//             << "horizontal scroll value" << horizontalScrollBar()->value();
//    return QPointF(xPct, yPct);
//}

qreal CompareView::getZoom()
{
    {
    #ifdef ISDEBUG
    qDebug() << "CompareView::getZoom";
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
    qDebug() << "CompareView::getFitScaleFactor";
    #endif
    }
    qreal hScale = ((qreal)container.width() - 2) / content.width();
    qreal vScale = ((qreal)container.height() - 2) / content.height();
    return (hScale < vScale) ? hScale : vScale;
}

void CompareView::scale(bool propagate)
{
    {
    #ifdef ISDEBUG
    qDebug() << "CompareView::scale";
    #endif
    }
    matrix.reset();
    matrix.scale(zoom, zoom);
    setMatrix(matrix);
    if (propagate) {
        qDebug() << "Propagating from" << currentImagePath;
        emit zoomFromPct(getMousePct(), imageIndex, isZoom);
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
    qDebug() << "CompareView::movePickIcon";
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
    if (sceneBottomRight.y() < rect().width())
        y = sceneBottomRight.y() - ph - offset;
    else y = rect().width() - ph - offset;

    pickLabel->move(x, y);
}

void CompareView::resizeEvent(QResizeEvent *event)
{
    {
    #ifdef ISDEBUG
    qDebug() << "CompareView::resizeEvent";
    #endif
    }
    QGraphicsView::resizeEvent(event);
    zoomFit = getFitScaleFactor(gridCell, pmItem->boundingRect());
    if (getZoom() <= zoomFit) {
        zoom = zoomFit;
        scale(false);
    }
}

void CompareView::zoomIn()
{
    {
    #ifdef ISDEBUG
    qDebug() << "CompareView::zoomIn";
    #endif
    }
    zoom *= (1.0 + zoomInc);
//    qDebug() << "zoomInc" << zoomInc;
    zoom = zoom > zoomMax ? zoomMax: zoom;
    scale(true);
}

void CompareView::zoomOut()
{
    {
    #ifdef ISDEBUG
    qDebug() << "CompareView::zoomOut";
    #endif
    }
    zoom *= (1.0 - zoomInc);
    zoom = zoom < zoomMin ? zoomMin : zoom;
    scale(true);
}

void CompareView::zoom100()
{
    {
    #ifdef ISDEBUG
    qDebug() << "CompareView::zoom100";
    #endif
    }
    clickZoom = 1;
//    mouseZoomFit = false;
//    resizeImage();
}

void CompareView::zoomToFit()
{
    {
    #ifdef ISDEBUG
    qDebug() << "CompareView::zoomToFit";
    #endif
    }
//    mouseZoomFit = true;
//    resizeImage();
}

void CompareView::zoomTo(float zoomTo)
{
    {
    #ifdef ISDEBUG
    qDebug() << "CompareView::zoomTo";
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
    qDebug() << "CompareView::zoomToggle";
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
    qDebug() << "CompareView::zoom50";
    #endif
    }
    zoomTo(0.5);
    clickZoom = 0.5;
}

void CompareView::zoom200()
{
    {
    #ifdef ISDEBUG
    qDebug() << "CompareView::zoom200";
    #endif
    }
    zoomTo(2.0);
    clickZoom = 2.0;
}

void CompareView::setClickZoom(float clickZoom)
{
    {
    #ifdef ISDEBUG
    qDebug() << "CompareView::setClickZoom";
    #endif
    }
    this->clickZoom = clickZoom;
}

// MOUSE EVENTS

void CompareView::mousePressEvent(QMouseEvent *event)
{
    {
    #ifdef ISDEBUG
    qDebug() << "CompareView::mousePressEvent";
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
    qDebug() << "CompareView::mouseReleaseEvent";
    #endif
    }
//    clickZoom = 0.5;
    qDebug() << "CompareView::mouseReleaseEvent  mouse x, y"
             << event->localPos();
    mousePt = event->localPos().toPoint();
    isZoom ? zoom = zoomFit : zoom = clickZoom;
    scale(true);
    QGraphicsView::mouseReleaseEvent(event);
}

void CompareView::enterEvent(QEvent *event)
{
    {
    #ifdef ISDEBUG
    qDebug() << "CompareView::enterEvent";
    #endif
    }
    this->setFocus();
    if (imageIndex != thumbView->currentIndex()) {
        thumbView->setCurrentIndex(imageIndex);
    }
}

