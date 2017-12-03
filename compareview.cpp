#include "compareview.h"
#include "global.h"

/*  OVERVIEW

This class manages a view of a scene containing a pixmap of a comparison
image in a grid of comparison images housed in CompareImages. Each grid item
view is a separate instance of this class.

Terms:

    Focus = the instance that has the focus and is thumbView->currentIndex
    Slave = the rest of the grid instances
    Current location - the scroll percent location of the current instance
    Offset location - the offset from the current location for each instance
    Propagate - true for the current instance only to prevent the other instances
                also triggering zoom and pan events.

Each instance becomes the Focus (current instance) when the mouse enters
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

    ● action starts with mouse release in the Focus instance.
    ● scale the Focus instance with propagate = true
    ● emit signal zoomFromPct to CompareImages to scale/pan all Slave instances
    ● scale each Slave instance with propagate = false (prevent ∞ loop)
    ● pan each Slave instance by scroll percentage and offset correction

Synced panning:

    In the default pan mode all instances pan together.

    ● The pan is initiated by a panning action in the Focus instance, which
      triggers a wheel event.  The Focus instance is panned by the default
      QGraphicsView behavior.
    ● The initial position before panning is recorded.  A signal to record the
      initial position is sent to CompareImages and on to the Slave instances.
      This will later be used to re-sync to the Focus instance if lag occurs.
    ● The signal panFromPct is continuously emitted from wheelEvent to
      CompareImages.
    ● CompareImages passes the scroll percentage to each Slave instance.
    ● Each Slave instance calculates the new scroll position based on the scroll
      percentage of the scene, corrected for any local offset.  Not all scroll
      events are signaled so the non-propagating instances can lag and get out
      of sync with the Focus instance.
    ● The mouse release event ends the panning operation. The delta position
      from panning the Focus instance is calculated (final position - start
      position) and sent to CompareImages and on to the other Slave instances.
      They calculate their respective final positions and execute a final scroll
      to re-sync with the Focus instance.

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

    classificationLabel = new CircleLabel(this);
    classificationLabel->setVisible(false);

    isMouseDrag = false;
    isMouseDoubleClick = false;
    isMouseClickZoom = false;
    isLeftMouseBtnPressed = false;
    zoomInc = 0.1;

    // use this connection to slave other instances to the focus instance
    connect(horizontalScrollBar(), SIGNAL(valueChanged(int)), this, SLOT(scrollEvent()));
    connect(verticalScrollBar(), SIGNAL(valueChanged(int)), this, SLOT(scrollEvent()));
}

bool CompareView::loadImage(QModelIndex idx, QString fPath)
{
/*
This function is called from CompareImages for each image in the comparison
selection.  As in ImageView, each pixmap is loaded from the cache if available,
or loads the file otherwise.
*/
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

void CompareView::slaveZoomToPct(QPointF scrollPct, bool isZoom)
{
/*
Called from compareImages after signal zoomFromPct has been emitted
by the focus instance in the compare group when a scale occurs.

Note that the scrollPct coordinates are relative, as a percentage of the
slider range, because each image in the compare group might have different
size or aspect ratios.
*/
    {
    #ifdef ISDEBUG
    qDebug() << "CompareView::zoomToPct" << currentImagePath;
    #endif
    }
    this->isZoom = isZoom;
    isZoom ? zoom = zoomFit : zoom = toggleZoom;
    // false is telling the scale function not to propagate the scale change
    scale(false);
    // pan to coordinates
    setScrollBars(scrollPct);
    // update the scrl struct that holds the current scrollbar parameters
    getScrollBarStatus();
}

void CompareView::scrollEvent()
/*
This slot is called when the scrollbar changes.  If this is the focus instance
then the scrolling must be caused by the user either zooming or panning.  If the
shift modifier is true then this is an independent pan and should not be
propagated to the slave instances.  If there is no shift modifier then the pan
should be propagated.

If this is a slave instance (does not have the focus) then the scrolling is the
result of a signal from the focus instance and no further propagation should
occur.
*/
{
    {
    #ifdef ISDEBUG
    qDebug() << "CompareView::scrollEvent " << currentImagePath;
    #endif
    }
    // is this the focus instance?
    if (imageIndex == thumbView->currentIndex()) {
        // independent panning or do we propagate to slave instances
        if (propagate) emit panFromPct(getScrollDeltaPct(), imageIndex);
        else {
            // update scrl struct scrollbar parameters
            getScrollBarStatus();
            // update the current position
            scrollPosPct = QPointF(scrl.hPct, scrl.vPct);
        }
    }
}

// not used
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

void CompareView::slavePanToDeltaPct(QPointF delta)
{
/*
When the focus instance is panned it signals CompareImages, which in turn calls
each slave instance to make the same pan from its previous position by delta.
*/
    {
    #ifdef ISDEBUG
    qDebug() << "CompareView::panToDeltaPct" << currentImagePath;
    #endif
    }
    // update scrl struct
    getScrollBarStatus();
    // pan by updating the scrollbars
    setScrollBars(QPointF(scrl.hPct + delta.x(), scrl.vPct + delta.y()));
    return;
}

void CompareView::setScrollBars(QPointF scrollPct)
{
/*
Pan to the coordinates scrollPct.  Coordinates are the percentage of the
scrollbar range becasue each comparison image instance may be a different size
and aspect ratio and we want to pan equally when the slave and focus iamges are
locked to pan together.
*/
    {
    #ifdef ISDEBUG
    qDebug() << "CompareView::setScrollBars" << currentImagePath;
    #endif
    }
    // update the scrl struct with scrollbar parameters
    getScrollBarStatus();
    // percentage of the scrollbar range
    scrl.hVal = scrl.hMin + scrollPct.x() * (scrl.hMax - scrl.hMin);
    scrl.vVal = scrl.vMin + scrollPct.y() * (scrl.vMax - scrl.vMin);
    // set scrollbar values which does the panning
    horizontalScrollBar()->setValue(scrl.hVal);
    verticalScrollBar()->setValue(scrl.vVal);
}

void CompareView::slaveSetPanStartPct()
{
/*
When the Focus instance left mouse clicks, the scrollPct position is signaled
(panStartPct) to CompareImages, which in turn calls all Slave instances to set
the possible start pan scroll position in percent.
*/
    startOfPanPct = getScrollPct();
}

void CompareView::slaveCleanupAfterPan(QPointF deltaPct)
{
/*
When the focus instance left mouse releases, the end scrollPct position is
signaled (cleanupAfterPan) to CompareImages, which in turn calls all slave
instances to set the possible end pan scroll position in percent.

This is required because the scroll event does not reliably send all scroll
occurrences which causes the non-propagating instances panning to lag a small
amount.  By recording the startOfPanPct and the endOfPanPct a final scroll
correction can be executed to re-sync all the instances.
*/
    endOfPanPct = startOfPanPct + deltaPct;
    // pan to coordinates
    setScrollBars(endOfPanPct);
}

void CompareView::getScrollBarStatus()
{
/*
This is a helper function to update the scrl struct with the scrollbar range and
current value information in a more compact form for subsequent panning.
*/
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
/*
Called by the focus instance from scrollEvent to calculate the delta between the
last scroll point and the current one.
*/
    {
    #ifdef ISDEBUG
    qDebug() << "CompareView::getScrollDeltaPct" << currentImagePath;
    #endif
    }
    getScrollBarStatus();
    // difference between new and previous scroll position
    QPointF delta(scrl.hPct - scrollPosPct.x(), scrl.vPct - scrollPosPct.y());
    // update current scroll position
    scrollPosPct = QPointF(scrl.hPct, scrl.vPct);
//    reportScrollBarStatus();
    return delta;
}

QPointF CompareView::getScrollPct()
{
/*
Returns coordinates in percent for the current scroll position.
*/
    {
    #ifdef ISDEBUG
    qDebug() << "CompareView::getScrollPct" << currentImagePath;
    #endif
    }
    // update scrl struct with current scrollbar parameters
    getScrollBarStatus();
    return QPointF(scrl.hPct, scrl.vPct);
}

QPointF CompareView::getScrollPct(QPoint p)
{
/*
Returns coordinates in percent for point p, where p is in scrollbar coordinates
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

qreal CompareView::getZoom()
{
/*
Zoom (or scale factor) is the ratio of monitor (viewport) pixels to image
pixels. In other words, how many viewport pixels = one image pixel. A zoom of
100% means each viewport pixel equals an image pixel.
*/
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
/*
Return the scale factor (or zoom) to fit the image inside the viewport.
*/
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
of the compareViews. The image iscaled to the current zoom factor. The
okayToPropagate flag prevents additional scaling in the focus instance of
compareView.

If called from mouse release with mouseDrag then panning is automatic because
setTransformationAnchor(QGraphicsView::AnchorUnderMouse). A signal is sent to
ZoomDlg in case it is open and can sync with local scale change.

ZoomDlg also signals changes in scale to CompareImages::zoomTo and then to the
local zoomTo. Since signals are sent from both ZoomDlg and CompareView this could
loupe repeatedly, so variance checks prevent a repeating cycle.
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

    // if focus instance (originator of scale change)
    if (okayToPropagate) {
        // new position base for delta scrolls
        scrollPosPct = QPointF(scrl.hPct, scrl.vPct);
        // signal slave instances
        emit zoomFromPct(scrollPosPct, imageIndex, isZoom);
    }
    isZoom = (zoom > zoomFit);
    if (isZoom) setCursor(Qt::OpenHandCursor);
    else setCursor(Qt::ArrowCursor);

    // reposition classification icons ("thumbs up", ratings and color class)
    movePickIcon();
}

void CompareView::movePickIcon()
{
/*
The bright green thumbsUp pixmap shows the pick status for the current image.
This function locates the pixmap in the bottom corner of the image label as the
image is resized and zoomed, adjusting for the aspect ratio of the image.

The classification label (ratings / color class) is also positioned.
*/
    {
    #ifdef ISDEBUG
    qDebug() << "CompareView::movePickIcon" << currentImagePath;
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

    // make pick label 3% of image scale within range of 20 - 40 pixels
    qreal f = 0.03;
    w *= f;
    h *= f;
    int d;                          // dimension of pick image
    w > h ? d = w : d = h;
    if (d < 20) d = 20;
    if (d > 40) d = 40;
    pickLabel->setPixmap(pickPixmap->scaled(d, d, Qt::KeepAspectRatio));

    pickLabel->move(x, y);
    classificationLabel->move(x + p.w - offset, y + p.h - offset);
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
    // make pick label 3% of image scale within range of 20 - 40 pixels
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

void CompareView::zoomToFit()
/*
Called from MW menu action to CompareImages and then to each compare instance
*/
{
    {
    #ifdef ISDEBUG
    qDebug() << "CompareView::zoomToFit" << currentImagePath;
    #endif
    }
    zoom = zoomFit;
    // set scale without propagation to slave instances
    scale(false);
}

void CompareView::zoomTo(qreal zoomTo)
{
/*
Called from CompareImages::zoomTo, which in turn receives a signal from ZoomDlg
when the zoom is changed. When scale(false) is called with the new zoom it will
signal back to ZoomDlg (which is reqd when scale changes occur locally). This
can cause circular messaging, so check whether scale has actually changed.
There can be small differences because the controls in ZoomDlg are using
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
/*
Called from MW menu action to CompareImages and then to each compare instance
*/
    {
    #ifdef ISDEBUG
    qDebug() << "CompareView::zoomToggle" << currentImagePath;
    #endif
    }
    if (!isZoom) zoom = toggleZoom;
}

// EVENTS

void CompareView::wheelEvent(QWheelEvent *wheelEvent)
{
/*
Called when trackpad scroll occurs.  The Focus instance is panned by the default
QGraphicsView behavior.  The panning is not propagated to the slave instances if
the shift modifier key is pressed.
*/
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

    isMouseDoubleClick = false;
    if (event->button() == Qt::LeftButton) {
        propagate = true;
        if (event->modifiers() & Qt::ShiftModifier) propagate = false;
        isLeftMouseBtnPressed = true;
        mousePressPt.setX(event->x());
        mousePressPt.setY(event->y());
        if (isZoom) {
            startOfPanPct = getScrollPct();
            emit panStartPct(imageIndex);
        }
    }
    QGraphicsView::mousePressEvent(event);
}

void CompareView::mouseMoveEvent(QMouseEvent *event)
{
/*
Pan the image during a mouse drag operation.  If the shift key is also pressed
then the pan will not be signaled to the other instances and only the
focus instance will pan.  The modifier key test is in the scroll event.
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
//            qDebug() << "Propagating instance end of pan:   propagate =" << propagate;
//            qDebug() << "startOfPanPct" << startOfPanPct
//                     << "endOfPanPct" << endOfPanPct
//                     << "deltaPanPct" << deltaPanPct;
        }

        if (isZoom) setCursor((Qt::OpenHandCursor));
        else setCursor(Qt::ArrowCursor);
        return;
    }

    if (!isZoom && zoom < zoomFit * 0.98)
        zoom = zoomFit;
    else
        isZoom ? zoom = zoomFit : zoom = toggleZoom;
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
