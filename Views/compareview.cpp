#include "compareview.h"
#include "Main/global.h"

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

CompareView::CompareView(QWidget *parent,
                         QSize gridCell,
                         DataModel *dm, Selection *sel,
                         Metadata *metadata,
                         ImageCacheData *icd,
                         IconView *thumbView)
{
    if (G::isLogger) G::log("CompareView::CompareView");

    this->mainWindow = parent;
    this->gridCell = gridCell;
    this->dm = dm;
    this->sel = sel;
    this->metadata = metadata;
    this->icd = icd;
    this->thumbView = thumbView;
    pixmap = new Pixmap(this, dm, metadata);
    this->setStyleSheet("QGraphicsView  {"
                        "margin:1; "
                        "border-style: solid; "
                        "border-width: 1; "
                        "border-color: rgb(111,111,111);}");

    scene = new QGraphicsScene();
    pmItem = new QGraphicsPixmapItem;
    pmItem->setBoundingRegionGranularity(1);
    scene->addItem(pmItem);

    setAcceptDrops(true);
    pmItem->setAcceptDrops(true);

    setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
    setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setScene(scene);

    classificationLabel = new ClassificationLabel(this);
    classificationLabel->setVisible(false);
    classificationLabel->setAlignment(Qt::AlignCenter);

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
    if (G::isLogger) G::log("CompareView::loadImage");

     /* important to keep currentImagePath.  It is used to check if there isn't
     an image (when currentImagePath.isEmpty() == true) - for example when
     no folder has been chosen. */
    currentImagePath = fPath;
    imageIndex = idx;

    // No folder selected yet
    if (fPath == "") return false;

    bool isLoaded = false;

    // load the image from the image cache if available
    QImage image;
    if (icd->contains(fPath)) {
        pmItem->setPixmap(QPixmap::fromImage(icd->imCache.value(fPath)));
        isLoaded = true;
    }
    else {
        // load the image from the image file, may need to wait a bit if another thread
        // reading file  rgh req'd to iterate
        for (int i=0; i<100000; i++) {
            isLoaded = pixmap->load(fPath, displayPixmap, "CompareView::loadImage");
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
    if (G::isLogger) G::log("CompareView::slaveZoomToPct");
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
    if (G::isLogger) G::log("CompareView::scrollEvent");
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
    if (G::isLogger) G::log("CompareView::getSceneCoordFromPct");
    return QPointF(scrollPct.x() * scene->width(),
                   scrollPct.y() * scene->height());
}

void CompareView::slavePanToDeltaPct(QPointF delta)
{
/*
When the focus instance is panned it signals CompareImages, which in turn calls
each slave instance to make the same pan from its previous position by delta.
*/
    if (G::isLogger) G::log("CompareView::slavePanToDeltaPct");
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
    if (G::isLogger) G::log("CompareView::setScrollBars");
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
    if (G::isLogger) G::log("CompareView::getScrollBarStatus");
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
    if (G::isLogger) G::log("CompareView::reportScrollBarStatus");
    /*
    qDebug() << G::t.restart() << "\t" << "ScrollBarStatus:" << currentImagePath// << "\n"
             << "hMin" << scrl.hMin << "hMax" << scrl.hMax << "hVal" << scrl.hVal
             << "vMin" << scrl.vMin << "vMax" << scrl.vMax << "vVal" << scrl.vVal
             << "hPct" << scrl.hPct << "vPct" << scrl.vPct
             << "\n";
             //*/
    getScrollBarStatus();
    int row = imageIndex.row();
}

QPointF CompareView::getScrollDeltaPct()
{
/*
    Called by the focus instance from scrollEvent to calculate the delta between the
    last scroll point and the current one.
*/
    if (G::isLogger) G::log("CompareView::getScrollDeltaPct");
    getScrollBarStatus();
    // difference between new and previous scroll position
    QPointF delta(scrl.hPct - scrollPosPct.x(), scrl.vPct - scrollPosPct.y());
    // update current scroll position
    scrollPosPct = QPointF(scrl.hPct, scrl.vPct);
    return delta;
}

QPointF CompareView::getScrollPct()
{
/*
    Returns coordinates in percent for the current scroll position.
*/
    if (G::isLogger) G::log("CompareView::getScrollPct");
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
    if (G::isLogger) G::log("CompareView::getZoom");
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
    if (G::isLogger) G::log("CompareView::getFitScaleFactor");
    qreal hScale = static_cast<qreal>(container.width() - 2) / content.width() * G::actDevicePixelRatio;
    qreal vScale = static_cast<qreal>(container.height() - 2) / content.height() * G::actDevicePixelRatio;
    return (hScale < vScale) ? hScale : vScale;
}

void CompareView::scale(bool okayToPropagate)
{
/*
    See overview at top of this file explaining multiple instances of compareView.

    CompareView::scale is called after a mouse click executes a toggle zoom in one
    of the compareViews. The image is scaled to the current zoom factor. The
    okayToPropagate flag prevents additional scaling in the focus instance of
    compareView.

    If called from mouse release with mouseDrag then panning is automatic because
    setTransformationAnchor(QGraphicsView::AnchorUnderMouse). A signal is sent to
    ZoomDlg in case it is open and can sync with local scale change.

    ZoomDlg also signals changes in scale to CompareImages::zoomTo and then to the
    local zoomTo.
*/
    if (G::isLogger) G::log("CompareView::scale");
    qDebug() << "CompareView::scale before"
             << "zoom =" << zoom
             << "zoomFit =" << zoomFit
             << "isZoom =" << isZoom;

    // rescale to new zoom factor
    matrix.reset();
    double highDpiZoom = zoom / G::actDevicePixelRatio;
    matrix.scale(highDpiZoom, highDpiZoom);
    setTransform(matrix);   // qt6.2

    // notify ZoomDlg of change in scale
    emit zoomChange(zoom, hasFocus());

    // if focus instance (originator of scale change)
    if (okayToPropagate) {
        // new position base for delta scrolls
        scrollPosPct = QPointF(scrl.hPct, scrl.vPct);
        // signal slave instances
        emit zoomFromPct(scrollPosPct, imageIndex, isZoom);
    }
    isZoom = (zoom > zoomFit * 1.02);
    if (isZoom) setCursor(Qt::OpenHandCursor);
    else setCursor(Qt::ArrowCursor);

    qDebug() << "CompareView::scale after "
             << "zoom =" << zoom
             << "zoomFit =" << zoomFit
             << "isZoom =" << isZoom;

    // reposition classification icons ("thumbs up", ratings and color class)
    placeClassificationBadge();
}

void CompareView::placeClassificationBadge()
{
/*
    The classification label (ratings / color class / pick status) is positioned in
    the bottom right corner of the image.
*/
    if (G::isLogger) G::log("CompareView::placeClassificationBadge");
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

    // resize if necessary
    /*
    qreal f = 0.05;
    w *= f;
    h *= f;
    int d;                          // dimension of pick image
    w > h ? d = w : d = h;
    if (d < 20) d = 18;
    if (d > 40) d = 40;
    //*/

    int o = 5;                          // offset margin from edge
    int d = 20;                         // diameter of the classification label
    classificationLabel->setDiameter(d);
    classificationLabel->move(x - d - o, y - d - o);
}

void CompareView::resizeEvent(QResizeEvent *event)
{
    if (G::isLogger) G::log("CompareView::resizeEvent");
    QGraphicsView::resizeEvent(event);
    zoomFit = getFitScaleFactor(event->size(), pmItem->boundingRect());
    scale(false);
}

void CompareView::zoomIn()
{
    if (G::isLogger) G::log("CompareView::zoomIn");
    zoom *= (1.0 + zoomInc);
    zoom = zoom > zoomMax ? zoomMax: zoom;
    scale(false);
}

void CompareView::zoomOut()
{
    if (G::isLogger) G::log("CompareView::zoomOut");
    zoom *= (1.0 - zoomInc);
    zoom = zoom < zoomMin ? zoomMin : zoom;
    scale(false);
}

void CompareView::zoomToFit()
/*
    Called from MW menu action to CompareImages and then to each compare instance
*/
{
    if (G::isLogger) G::log("CompareView::zoomToFit");
    zoom = zoomFit;
    // set scale without propagation to slave instances
    scale(false);
}

void CompareView::zoomTo(qreal zoomTo)
{
/*
    Called from CompareImages::zoomTo, which in turn receives a signal from ZoomDlg when the
    zoom is changed. When scale(false) is called with the new zoom it will signal back to
    ZoomDlg (which is reqd when scale changes occur locally).

    Also called from MW::setDisplayResolution when there is a operating system scale change.
*/
    if (G::isLogger) G::log("CompareView::zoomTo");
    zoom = zoomTo;
    scale(false);
}

void CompareView::zoomToggle()
{
/*
    Called from MW menu action to CompareImages and then to each compare instance
*/
    if (G::isLogger) G::log("CompareView::zoomToggle");
//    qDebug() << "CompareView::zoomToggle" << toggleZoom << G::actDevicePixelRatio;
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
    if (G::isLogger) G::log("CompareView::wheelEvent");
    propagate = true;
    if (imageIndex == thumbView->currentIndex()) {
        if(wheelEvent->modifiers() & Qt::ShiftModifier) propagate = false;
    }
    QGraphicsView::wheelEvent(wheelEvent);
}

void CompareView::mousePressEvent(QMouseEvent *event)
{
    if (G::isLogger) G::log("CompareView::mousePressEvent");
    // bad things happen if no image when click
    if (currentImagePath.isEmpty()) return;

    // prevent zooming when right click for context menu
    if (event->button() == Qt::RightButton) return;

    // forward and back buttons
    if (event->button() == Qt::BackButton) {
        emit togglePick();
        return;
    }
    if (event->button() == Qt::ForwardButton) {
        emit togglePick();
        return;
    }

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
    if (isLeftMouseBtnPressed) {
        isMouseDrag = true;
        setCursor(Qt::ClosedHandCursor);

        /* scroll to pan with the mouse drag.  All operations are relative so
           calculate the delta amount between mouse events.  The scroll event
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
    if (G::isLogger) G::log("CompareView::mouseReleaseEvent");
    // prevent zooming when right click for context menu, or forward and back
    // buttons to toggle pick status
    if (event->button() == Qt::RightButton ||
        event->button() == Qt::BackButton ||
        event->button() == Qt::ForwardButton) return;

    isLeftMouseBtnPressed = false;
    if (isMouseDrag) {
        isMouseDrag = false;
        if (isZoom) {
            endOfPanPct = getScrollPct();
            deltaPanPct = endOfPanPct - startOfPanPct;

            // only propagate if shift modifier was not engaged during pan
            if (propagate) emit cleanupAfterPan(deltaPanPct, imageIndex);
        }

        if (isZoom) setCursor((Qt::OpenHandCursor));
        else setCursor(Qt::ArrowCursor);
        return;
    }
    qDebug() << "CompareView::mouseReleaseEvent"
             << "zoom =" << zoom
             << "zoomFit =" << zoomFit
             << "isZoom =" << isZoom;

    if (!isZoom && zoom < zoomFit * 0.98)
        zoom = zoomFit;
    else
        isZoom ? zoom = zoomFit : zoom = toggleZoom;
    propagate = false;
    scale(true);
    propagate = true;

    QGraphicsView::mouseReleaseEvent(event);
}

void CompareView::enterEvent(QEnterEvent *event)
{
    if (G::isLogger) G::log("CompareView::enterEvent");
    select();
    // zoomToFit zoom factor can be different so do update
    emit zoomChange(zoom, /* hasFocus */ true);
    QGraphicsView::enterEvent(event); // qt6.2
}

void CompareView::leaveEvent(QEvent *event)
{
    if (G::isLogger) G::log("CompareView::leaveEvent");
    deselect();
    // emit zoomChange(zoom, /* hasFocus */ true);
    QGraphicsView::leaveEvent(event);
}

void CompareView::select()
{
    if (G::isLogger) G::log("CompareView::select");
    emit deselectAll();
    this->setFocus();
    // req'd for IconViewDelegate to show current item
    dm->currentSfIdx = imageIndex;
    dm->currentSfRow = imageIndex.row();

    thumbView->setSelectionMode(QAbstractItemView::SingleSelection);
    thumbView->setCurrentIndex(imageIndex);
    thumbView->setSelectionMode(QAbstractItemView::NoSelection);

    // thumbView->setSelectionMode(QAbstractItemView::SingleSelection);
    // sel->select(imageIndex, Qt::NoModifier,"CompareView::select");
    // // prevent user selection in thumbView while comparing
    // thumbView->setSelectionMode(QAbstractItemView::NoSelection);

    this->setStyleSheet("QGraphicsView  {"
                        "margin:1; "
                        "border-style: solid; "
                        "border-width: 3; "
                        "border-color: rgb(225,225,225);}");
}

void CompareView::deselect()
{
    if (G::isLogger) G::log("CompareView::deselect");
    this->setStyleSheet("QGraphicsView  {"
                        "margin:1; "
                        "border-style: solid; "
                        "border-width: 2; "
                        "border-color: rgb(111,111,111);}");
}

void CompareView::dragEnterEvent(QDragEnterEvent *event)
{
/*
    Empty function required to propagate drop event (not sure why)
*/
    if (G::isLogger) G::log("CompareView::dragEnterEvent");
    qDebug() << "CompareView::dragEnterEvent";
}
