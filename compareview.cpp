#include "compareview.h"
#include "global.h"

/*  OVERVIEW

This class manages a view of a scene containing a pixmap of a comparison
image in a grid of comparison images housed in CompareImages. Each grid item
view is a separate instance of this class.

Terms:

    Propagator = has the focus and is thumbView->currentIndex
    NonPropagator =  the rest of the grid instances
    Current location - the scroll percent location of the current instance
    Offset location - the offset from the current location for each instance
    Propagate - true for the current instance only to prevent the other instances
                also triggering zoom and pan events.

Each instance becomes the propagator (current instance) when the mouse enters
its grid location and the thumbView->currentIndex is set. Each instance can be
panned separately or in sync mode, where all instances pan together. Because
each instance can be panned independently, the independent panning is recorded
as the offset to the synced pan location.

Mouse click zoom:

    Each image can be zoomed to a mouse location, which propagates to all the
    other instances, which also zoom to the same location. However, the
    location is relative to each instance, since each instance has a separate
    image, which may be a different size and have a different aspect ratio.
    Hence, location is defined as the percentage of the scroll range.

    ● action starts with mouse release in the current instance.
    ● scale the current instance with propagate = true
    ● emit signal zoomFromPct to CompareImages to scale/pan all instances
    ● scale each instance with propagate = false (prevent ∞ loop)
    ● pan each instance by scroll percentage and offset correction

Synced panning:

    In the default pan mode all instances pan together.

    ● The pan is initiated by a panning action in the current instance, which
      triggers a wheel event.  The current instance is panned by the default
      QGraphicsView behavior.
    ● The initial position before panning is recorded.  A signal to record the
      initial position is sent to CompareImages and on to the non-propagators.
      This will later be used to re-sync to the propagator if lag occurs.
    ● The signal panFromPct is continuously emitted from wheelEvent to
      CompareImages.
    ● CompareImages passes the scroll percentage to each instance.
    ● Each instance calculates the new scroll position based on the scroll
      percentage of the scene, corrected for any local offset.  Not all scroll
      events are signaled so the non-propagating instances can lag and get out
      of sync with the propagator.
    ● The mouse release event ends the panning operation.  The delta position
      from panning the propagator is calculated (final position - start position)
      and sent to CompareImages and on to the other instances.  They calculate
      their respective final positions and execute a final scroll to re-sync
      with the propagator.

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
    pixmap = new Pixmap(this, metadata);
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

    editsLabel = new CircleLabel(this);
    editsLabel->setVisible(false);

    isMouseDrag = false;
    isMouseDoubleClick = false;
    isMouseClickZoom = false;
    isLeftMouseBtnPressed = false;
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
            isLoaded = pixmap->load(fPath, displayPixmap);
//            isLoaded = loadPixmap(fPath, displayPixmap);
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
//    qDebug() << "CompareView::zoomToPct" << currentImagePath
//             << "isZoom =" << isZoom
//             << "scrollPct =" << scrollPct;
    this->isZoom = isZoom;
    isZoom ? zoom = zoomFit : zoom = toggleZoom;
    scale(false);
//    scrollPosPct = scrollPct;      // new position base for delta scrolls
    panToPct(scrollPct);
    getScrollBarStatus();
//    reportScrollBarStatus();
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
    reportScrollBarStatus("CV::panToDeltaPct                ");
    return;
}

void CompareView::panToPct(QPointF scrollPct)
{
    {
    #ifdef ISDEBUG
    qDebug() << "CompareView::panToPct" << currentImagePath;
    #endif
    }
    setScrollBars(scrollPct);
    reportScrollBarStatus("CV::panToPct                     ");
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

void CompareView::npSetPanStartPct()
{
/*
When the propagating instance left mouse clicks the scrollPct position is
signalled (panStartPct) to CompareImages, which in turn calls all non-propagating
instances to set the possible start pan scroll position in percent.
*/
    startOfPanPct = getScrollPct();
    qDebug() << "\n" << imageIndex.row()
             << "CompareView::npSetPanStartPct  startOfPanPct = " << startOfPanPct;
}

void CompareView::npCleanupAfterPan(QPointF deltaPct)
{
/*
When the propagating instance left mouse releases the end scrollPct position is
signalled (panEndPct) to CompareImages, which in turn calls all non-propagating
instances to set the possible end pan scroll position in percent.

This is required because the scroll event does not reliably send all scroll
occurrences which causes the non-propagating instances panning to lag a small
amount.  By recording the startOfPanPct and the endOfPanPct a final scroll
correction can be executed to re-sync all the instances.
*/
    endOfPanPct = startOfPanPct + deltaPct;
    panToPct(endOfPanPct);
    qDebug() << "CompareView::cleanupAfterPan:   propagate =" << propagate;
    qDebug() << "startOfPanPct" << startOfPanPct
             << "endOfPanPct" << endOfPanPct
             << "deltaPanPct" << deltaPct;
}

void CompareView::getScrollBarStatus()
{
    {
    #ifdef ISDEBUG
    qDebug() << "CompareView::getScrollBarStatus" << currentImagePath;
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

void CompareView::reportScrollBarStatus(QString info)
{
    {
    #ifdef ISDEBUG
    qDebug() << "CompareView::reportScrollBarStatus" << currentImagePath;
    #endif
    }
//    qDebug() << "ScrollBarStatus:" << currentImagePath// << "\n"
//             << "hMin" << scrl.hMin << "hMax" << scrl.hMax << "hVal" << scrl.hVal
//             << "vMin" << scrl.vMin << "vMax" << scrl.vMax << "vVal" << scrl.vVal
//             << "hPct" << scrl.hPct << "vPct" << scrl.vPct
//             << "\n";
    getScrollBarStatus();
    int row = imageIndex.row();
    qDebug() << row << info << "Zoom" << zoom << "hPct" << scrl.hPct << "vPct" << scrl.vPct;
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
/*
Returns coordinates in percent for the current scroll position.
Only called from current propagating instance of CompareView
*/
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

QPointF CompareView::getScrollPct(QPoint p)
{
/*
Returns coordinates in percent for point p
Only called from current instance of CompareView
*/
    int hMin = horizontalScrollBar()->minimum();
    int hMax = horizontalScrollBar()->maximum();
    int hVal = p.x();
    qreal hPct = qreal(hVal - hMin) / (hMax - hMin);
    int vMin = verticalScrollBar()->minimum();
    int vMax = verticalScrollBar()->maximum();
    int vVal = p.y();
    qreal vPct = qreal(vVal - vMin) / (vMax - vMin);
    return QPointF(hPct, vPct);
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
/*
See overview at top of this file explaining multiple instances of compareView.

CompareView::scale is called after a mouse click executes a toggle zoom in one
of the compareViews. Scales the image to the current zoom factor. The
okayToPropagate flag prevents additional scaling in the source compareView.

If called from mouse release then panning is automatic because
setTransformationAnchor(QGraphicsView::AnchorUnderMouse).  Signal to ZoomDlg in
case it is open and can sync with local scale change.

If called from zoomTo, as a result of a signal sent from ZoomDlg to
CompareImages::zoomTo and then to the local zoomTo, the return signal will be
circular, with variance checks to prevent a repeating cycle.
*/
    {
    #ifdef ISDEBUG
    qDebug() << "CompareView::scale" << currentImagePath;
    #endif
    }
    // rescale to new zoom factor
    matrix.reset();
    matrix.scale(zoom, zoom);
    setMatrix(matrix);

    // notify ZoomDlg of change in scale
    emit zoomChange(zoom);

    // if not current instance (originator of scale change)
    if (okayToPropagate) {
//        qDebug() << "Propagating from" << currentImagePath;
//        getScrollBarStatus();
        reportScrollBarStatus("CV::scale okayToPropagate emiting");
        scrollPosPct = QPointF(scrl.hPct, scrl.vPct);      // new position base for delta scrolls
//        qDebug() << "CompareView::scale" << currentImagePath
//                 << "EMITING scrollPosPct " << scrollPosPct;
        emit zoomFromPct(scrollPosPct, imageIndex, isZoom);
//        emit align(scrollPosPct, imageIndex);
    }
    reportScrollBarStatus("CV::scale                        ");
    isZoom = (zoom > zoomFit);
    if (isZoom) setCursor(Qt::OpenHandCursor);
    else setCursor(Qt::ArrowCursor);

    movePickIcon();
//    emit updateStatus(true, "");
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

//    int pw = pickLabel->width();        // width of the pick symbol
//    int ph = pickLabel->height();       // height of the pick symbol
//    int offset = 10;                    // offset pixels from the edge of image
//    int x, y = 0;                       // top left coordinates of pick symbol

//    // if the image view is not as wide as the window
//    if (sceneBottomRight.x() < rect().width())
//        x = sceneBottomRight.x() - pw - offset;
//    else x = rect().width() - pw - offset;

//    // if the image view is not as high as the window
//    if (sceneBottomRight.y() < rect().height())
//        y = sceneBottomRight.y() - ph - offset;
//    else y = rect().height() - ph - offset;

//    pickLabel->move(x, y);
//    editsLabel->move(x + pw - offset, y + ph - offset);

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

    //    qDebug() << "sceneBottomRight" << sceneBottomRight << "rect()" << rect()
//             << "x" << x << "y" << y;

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
    toggleZoom = 1;
    zoom = toggleZoom;
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
    zoom = zoomFit;
    scale(true);
}

void CompareView::zoomTo(qreal zoomTo)
{
/*
Called from CompareImages::zoomTo, which in turn receives a signal from ZoomDlg
when the zoom is changed. When scale(false) is called with the new zoom it will
signal back to ZoomDlg (which is reqd when scale changes occur locally). This
can cause circular messaging, so check whether scale has actually changed.
There can be small differences becasue the controls in ZoomDlg are using
integers so the conversion can be off by up to 0.005.
*/
    {
    #ifdef ISDEBUG
    qDebug() << "CompareView::zoomTo" << currentImagePath;
    #endif
    }
    qreal variance = qFabs(zoom - zoomTo);
    if (variance < .005) return;

    zoom = zoomTo;
    scale(false);
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
    if (!isZoom) zoom = toggleZoom;
//    resizeImage();
}

void CompareView::zoom50()
{
    {
    #ifdef ISDEBUG
    qDebug() << "CompareView::zoom50" << currentImagePath;
    #endif
    }
    toggleZoom = 0.5;
    zoom = toggleZoom;
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
    toggleZoom = 2;
    zoom = toggleZoom;
    zoom = zoom < zoomMin ? zoomMin : zoom;
    scale(false);
}

void CompareView::setClickZoom(float toggleZoom)
{
    {
    #ifdef ISDEBUG
    qDebug() << "CompareView::setClickZoom" << currentImagePath;
    #endif
    }
    this->toggleZoom = toggleZoom;
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
        if(wheelEvent->modifiers() & Qt::ShiftModifier) propagate = false;
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

    // prevent zooming when right click for context menu
    if (event->button() == Qt::RightButton) return;//

    isMouseDoubleClick = false;//
    if (event->button() == Qt::LeftButton) {
        propagate = true;
        if (event->modifiers() & Qt::ShiftModifier) propagate = false;
        isLeftMouseBtnPressed = true;
        mousePressPt.setX(event->x());
        mousePressPt.setY(event->y());
        if (isZoom) {
            startOfPanPct = getScrollPct();
            emit panStartPct(imageIndex);
//        qDebug() << "CompareView::mousePressEvent  mousePressPt =" << currentImagePath
//                 << "mousePressPt =" << mousePressPt;
            reportScrollBarStatus("CV::mouse press                  ");
        }
    }
    QGraphicsView::mousePressEvent(event);
}

void CompareView::mouseMoveEvent(QMouseEvent *event)
{
/*
Pan the image during a mouse drag operation.  If the shift key is also pressed
then the pan will not be signaled to the other instances and only the
propagator will pan.  The modifier key test is in the scroll event.
*/
    {
    #ifdef ISDEBUG
//    qDebug() << "CompareView::mouseMoveEvent";
    #endif
    }
    if (isLeftMouseBtnPressed) {
        isMouseDrag = true;
        setCursor(Qt::ClosedHandCursor);

        /* scroll to pan with the mouse drag.  All operations are relative so
           calculate the delta amount between mouse events.  Thehe scroll event
           is fired by changes to the scrollbars which signals the other
           instances to mirror the pan  */
        horizontalScrollBar()->setValue(horizontalScrollBar()->value() -
                                       (event->x() - mousePressPt.x()));
        verticalScrollBar()->setValue(verticalScrollBar()->value() -
                                      (event->y() - mousePressPt.y()));
        // keep point to calc next delta
        mousePressPt.setX(event->x());
        mousePressPt.setY(event->y());
//        qDebug() << "CompareView::mouseMoveEvent" << currentImagePath
//                 << "mousePressPt =" << mousePressPt;
        reportScrollBarStatus("CV::mousemove                    ");
    }
    event->ignore();
}

void CompareView::mouseReleaseEvent(QMouseEvent *event)
{
    {
    #ifdef ISDEBUG
    qDebug() << "CompareView::mouseReleaseEvent" << currentImagePath;
    #endif
    }
    isLeftMouseBtnPressed = false;
    if (isMouseDrag) {
        isMouseDrag = false;
        if (isZoom) {
            endOfPanPct = getScrollPct();
            deltaPanPct = endOfPanPct - startOfPanPct;

            // only propagate if shift modifier was not engaged during pan
            if (propagate) emit cleanupAfterPan(deltaPanPct, imageIndex);
            qDebug() << "Propagating instance end of pan:   propagate =" << propagate;
            qDebug() << "startOfPanPct" << startOfPanPct
                     << "endOfPanPct" << endOfPanPct
                     << "deltaPanPct" << deltaPanPct;
        }

        if (isZoom) setCursor((Qt::OpenHandCursor));
        else setCursor(Qt::ArrowCursor);
        return;
    }

    if (!isZoom && zoom < zoomFit * 0.99)
        zoom = zoomFit;
    else
        isZoom ? zoom = zoomFit : zoom = toggleZoom;
    propagate = false;
    scale(true);
    propagate = true;

    QGraphicsView::mouseReleaseEvent(event);
//    if (isMouseDrag) {
//        isMouseDrag = false;
//        if (isZoom) setCursor((Qt::OpenHandCursor));
//        else setCursor(Qt::ArrowCursor);
//        isLeftMouseBtnPressed = false;
//        return;
//    }
//    if (isLeftMouseBtnPressed) {
//        isLeftMouseBtnPressed = false;
//        mousePt = event->localPos().toPoint();
//        isZoom ? zoom = zoomFit : zoom = toggleZoom;
//        isMouseClickZoom = true;
//        propagate = false;
//        scale(true);
//        propagate = true;
//    }
//    QGraphicsView::mouseReleaseEvent(event);
}

void CompareView::enterEvent(QEvent *event)
{
    {
    #ifdef ISDEBUG
    qDebug() << "CompareView::enterEvent" << currentImagePath;
    #endif
    }
    select();
    // zoomToFit zoom factor can be different so do update
    emit zoomChange(zoom);
    QGraphicsView::enterEvent(event);
}

void CompareView::leaveEvent(QEvent *event)
{
    {
    #ifdef ISDEBUG
    qDebug() << "CompareView::enterEvent" << currentImagePath;
    #endif
    }
    deselect();
    QGraphicsView::leaveEvent(event);
}

void CompareView::select()
{
    {
    #ifdef ISDEBUG
    qDebug() << "CompareView::select" << currentImagePath;
    #endif
    }
    this->setFocus();
    thumbView->setSelectionMode(QAbstractItemView::SingleSelection);
    thumbView->setCurrentIndex(imageIndex);
    thumbView->setSelectionMode(QAbstractItemView::NoSelection);
    this->setStyleSheet("QGraphicsView  {"
                        "margin:1; "
                        "border-style: solid; "
                        "border-width: 2; "
                        "border-color: rgb(225,225,225);}");
}

void CompareView::deselect()
{
    {
    #ifdef ISDEBUG
    qDebug() << "CompareView::deselect" << currentImagePath;
    #endif
    }
    this->setStyleSheet("QGraphicsView  {"
                        "margin:1; "
                        "border-style: solid; "
                        "border-width: 2; "
                        "border-color: rgb(111,111,111);}");
}
