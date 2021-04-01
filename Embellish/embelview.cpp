#include <QGraphicsDropShadowEffect>
#include <math.h>
#include "Main/global.h"
#include "Embellish/embelview.h"
#include <QApplication>

#define CLIPBOARD_IMAGE_NAME		"clipboard.png"
#define ROUND(x) (static_cast<int>((x) + 0.5))
//#define ROUND(x) ((int) ((x) + 0.5))
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define MIN(a, b) ((a) < (b) ? (a) : (b))

/*  COORDINATE SPACES

The image is an item on the scene and the view is a viewport of the scene.

    rect() = the viewport rectangle = this

The view scaling is controlled by the zoomFactor.  When zoomFactor = 1
the view is at 100%, the same as the original image.

Moving is accomplished by adjusting the viewport scrollbars.

 */

EmbelView::EmbelView(QWidget *parent,
                     QWidget *centralWidget,
                     Metadata *metadata,
                     DataModel *dm,
                     ImageCache *imageCacheThread,
                     IconView *thumbView):

                     QGraphicsView(centralWidget)
{
    if (G::isLogger) G::log(__FUNCTION__); 

    this->mainWindow = parent;
    this->metadata = metadata;
    this->dm = dm;
    this->imageCacheThread = imageCacheThread;
    this->thumbView = thumbView;
    pixmap = new Pixmap(this, dm, metadata);

    scene = new QGraphicsScene();
    pmItem = new QGraphicsPixmapItem;
    pmItem->setBoundingRegionGranularity(1);
    scene->addItem(pmItem);

    setAcceptDrops(true);
    pmItem->setAcceptDrops(true);
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

bool EmbelView::loadImage(QString fPath)
{
/*
There are two sources for the image (pixmap): a file or the cache.

The first choice is the image cache.  In the cache two versions of the image are
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
the next image before a timer interval has elapsed then the timer is reset.
Otherwise, when the timer interval occurs (loadFullSizeTimer->setInterval(500))
then the full scale pixmap from the cache is set as the item pixmap in the
scene.

Moving from one image to the next, the scenario where the currrent image is
full scale and the next is a preview, requires the zoom factor to be normalized
to prevent jarring changes in perceived scale by the user.
*/
    if (G::isLogger) G::log(__FUNCTION__); 
    // No folder selected yet
    if (!fPath.length()) return false;
    if (G::memTest) return false;

    /* important to keep currentImagePath.  It is used to check if there isn't
    an image (when currentImagePath.isEmpty() == true) - for example when
    no folder has been chosen. */
    currentImagePath = fPath;
    bool isLoaded = false;
    pmItem->setVisible(true);

    // load the image from the image cache if available
    if (imageCacheThread->imCache.contains(fPath)) {
        pmItem->setPixmap(QPixmap::fromImage(imageCacheThread->imCache.value(fPath)));
        isLoaded = true;
    }
    else {
        // check metadata loaded for image (might not be if random slideshow)
        int dmRow = dm->fPathRow[fPath];
        if (!dm->index(dmRow, G::MetadataLoadedColumn).data().toBool()) {
            QFileInfo fileInfo(fPath);
            if (metadata->loadImageMetadata(fileInfo, true, true, false, true, __FUNCTION__)) {
                metadata->m.row = dmRow;
                dm->addMetadataForItem(metadata->m);
            }
        }
        QPixmap  displayPixmap;

        isLoaded = pixmap->load(fPath, displayPixmap);

        if (isLoaded) {
            pmItem->setPixmap(displayPixmap);
        }
        else {
            pmItem->setPixmap(QPixmap(":/images/error_image.png"));
            isLoaded = true;
        }
    }

    /* When the program is opening or resizing it is possible this function could be called
    before the central widget has been fully defined, and has a small default size.  If that
    is the case, ignore, as the function will be called again.
    Also ignore if the image failed to be loaded into the graphics scene.
    */
    if (isLoaded && rect().height() > 100) {
        pmItem->setVisible(true);
        // prevent the viewport scrolling outside the image
//        setSceneRect(scene->itemsBoundingRect());

        /* If this is the first image in a new folder, and the image is smaller than the
        canvas (central widget window) set the scale to fit window, do not scale the
        image beyond 100% to fit the window.  */
        qDebug() << __FUNCTION__ << "zoomFit = getFitScaleFactor(rect(), pmItem->boundingRect())";
        zoomFit = getFitScaleFactor(rect(), pmItem->boundingRect());
        if (isFirstImageNewFolder) {
            isFit = true;
            isFirstImageNewFolder = false;
        }
        if (isFit) {
            setFitZoom();
        }
        scale();
    }
    QImage im = pmItem->pixmap().toImage();
    imAspect = qreal(im.width()) / im.height();
    qDebug() << __FUNCTION__ << "G::isInitializing" << G::isInitializing;
//    qDebug() << __FUNCTION__ << "imAspect =" << imAspect;
    emit updateEmbel();
    return isLoaded;
}

void EmbelView::clear()
{
    if (G::isLogger) G::log(__FUNCTION__); 
    QPixmap nullPm;
    pmItem->setPixmap(nullPm);
    pmItem->setVisible(false);
}

void EmbelView::noJpgAvailable()
{
    pmItem->setVisible(false);
}

void EmbelView::scale()
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

Geometry

    rect() = The container or canvas or entire scene in monitor pixels
*/
    if (G::isLogger) G::log(__FUNCTION__); 
    /*
    qDebug() << __FUNCTION__
             << "isScrollable =" << isScrollable
             << "isFit =" << isFit
             << "zoom =" << zoom
             << "zoomFit =" << zoomFit
             << "rect() =" << rect()
             << "sceneRect() =" << sceneRect()
            ;
    //  */

    matrix.reset();
    if (G::isSlideShow) {
        setFitZoom();
    }

    if (isFit) setFitZoom();
//    qDebug() << __FUNCTION__ << zoom << toggleZoom;
    matrix.scale(zoom, zoom);
    // when resize before first image zoom == inf
    if (zoom > 10) return;
    setMatrix(matrix);
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

    emit updateStatus(true, "", __FUNCTION__);

    isMouseDoubleClick = false;

    /*
    qDebug() << __FUNCTION__
             << "isPreview =" << isPreview
             << "isScrollable =" << isScrollable
             << "isFit =" << isFit
             << "zoom =" << zoom
             << "zoomFit =" << zoomFit
             << "rect().width() =" << rect().width()
             << "sceneRect().width() =" << sceneRect().width();
//    */
}

bool EmbelView::sceneBiggerThanView()
{
    if (G::isLogger) G::log(__FUNCTION__); 
    QPoint pTL = mapFromScene(0, 0);
    QPoint pBR = mapFromScene(scene->width(), scene->height());
    int sceneViewWidth = pBR.x() - pTL.x();
    int sceneViewHeight = pBR.y() - pTL.y();
    if (sceneViewWidth > rect().width() || sceneViewHeight > rect().height())
        return true;
    else
        return false;
}

qreal EmbelView::getFitScaleFactor(QRectF container, QRectF content)
{
    if (G::isLogger) G::log(__FUNCTION__); 
    qreal hScale = ((qreal)container.width() - 2) / content.width();
    qreal vScale = ((qreal)container.height() - 2) / content.height();
    return (hScale < vScale) ? hScale : vScale;
}

void EmbelView::setScrollBars(QPointF scrollPct)
{
    if (G::isLogger) G::log(__FUNCTION__); 
    getScrollBarStatus();
    scrl.hVal = scrl.hMin + scrollPct.x() * (scrl.hMax - scrl.hMin);
    scrl.vVal = scrl.vMin + scrollPct.y() * (scrl.vMax - scrl.vMin);
    horizontalScrollBar()->setValue(scrl.hVal);
    verticalScrollBar()->setValue(scrl.vVal);
}

void EmbelView::getScrollBarStatus()
{
    if (G::isLogger) G::log(__FUNCTION__); 
    scrl.hMin = horizontalScrollBar()->minimum();
    scrl.hMax = horizontalScrollBar()->maximum();
    scrl.hVal = horizontalScrollBar()->value();
    scrl.hPct = qreal(scrl.hVal - scrl.hMin) / (scrl.hMax - scrl.hMin);
    scrl.vMin = verticalScrollBar()->minimum();
    scrl.vMax = verticalScrollBar()->maximum();
    scrl.vVal = verticalScrollBar()->value();
    scrl.vPct = qreal(scrl.vVal - scrl.vMin) / (scrl.vMax - scrl.vMin);
}

QPointF EmbelView::getScrollPct()
{
/* The view center is defined by the scrollbar values.  The value is converted
to a percentage to be used to match position in the next image if zoomed.
*/
    if (G::isLogger) G::log(__FUNCTION__); 
    getScrollBarStatus();
    return QPointF(scrl.hPct, scrl.vPct);
}

void EmbelView::activateRubberBand()
{
    isRubberBand = true;
    setCursor(Qt::CrossCursor);
    QString msg = "Rubberband activated.  Make a selection in the EmbelView.\n"
                  "Press Esc to quit rubberbanding";
    G::popUp->showPopup(msg, 1500);
}

void EmbelView::quitRubberBand()
{
    isRubberBand = false;
    setCursor(Qt::ArrowCursor);
}

void EmbelView::resizeEvent(QResizeEvent *event)
{
/* Manage behavior when window resizes.

● if image zoom is not zoomFit then no change
● if zoomFit then recalc and maintain
● if zoomed and resize to view entire image then engage zoomFit
● if view larger than image and resize to clip image then engage zoomFit.

● move and size pick icon and shooting info as necessary

*/
    if (G::isLogger) G::log(__FUNCTION__); 
    /*
    qDebug() << __FUNCTION__
             << "G::isInitializing =" << G::isInitializing
             << "G::isNewFolderLoaded =" << G::isNewFolderLoaded
             << "isFirstImageNewFolder =" << isFirstImageNewFolder;
    //    */
    if (G::isInitializing) return;
    QGraphicsView::resizeEvent(event);
    qDebug() << __FUNCTION__ << "zoomFit = getFitScaleFactor(rect(), scene->itemsBoundingRect())";
    zoomFit = getFitScaleFactor(rect(), scene->itemsBoundingRect());
//    zoomFit = getFitScaleFactor(rect(), pmItem->boundingRect());
    if (isFit) {
        setFitZoom();
        scale();
    }
}

void EmbelView::thumbClick(float xPct, float yPct)
{
/* When the image is zoomed and a thumbnail is mouse clicked the position
within the thumb is passed here as the percent of the width and height. The
zoom amount is maintained and the main image is panned to the same location as
on the thumb. This makes it quick to check eyes and other details in many
images.
*/
    if (G::isLogger) G::log(__FUNCTION__); 
    if (zoom > zoomFit) {
        centerOn(QPointF(xPct * sceneRect().width(), yPct * sceneRect().height()));
    }
}

qreal EmbelView::getZoom()
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

void EmbelView::updateToggleZoom(qreal toggleZoomValue)
{
/*
Slot for signal from update zoom dialog to set the amount to zoom when user
clicks on the unzoomed image.
*/
    if (G::isLogger) G::log(__FUNCTION__); 
    toggleZoom = toggleZoomValue;
}

void EmbelView::refresh()
{
    if (G::isLogger) G::log(__FUNCTION__); 
    setFitZoom();
    scale();
}

void EmbelView::zoomIn()
{
    if (G::isLogger) G::log(__FUNCTION__); 
    zoom *= (1.0 + zoomInc);
    zoom = zoom > zoomMax ? zoomMax: zoom;
    scale();
}

void EmbelView::zoomOut()
{
    if (G::isLogger) G::log(__FUNCTION__); 
    zoom *= (1.0 - zoomInc);
    zoom = zoom < zoomMin ? zoomMin : zoom;
    scale();
}

void EmbelView::zoomToFit()
{
    if (G::isLogger) G::log(__FUNCTION__); 
    zoom = zoomFit;
    scale();
}

void EmbelView::zoomTo(qreal zoomTo)
{
/*
Called from ZoomDlg when the zoom is changed. From here the message is passed
on to EmbelView::scale(), which in turn makes the proper scale change.
*/
    if (G::isLogger) G::log(__FUNCTION__); 
    zoom = zoomTo;
    isFit = false;
    scale();
}

void EmbelView::resetFitZoom()
{
    if (G::isLogger) G::log(__FUNCTION__); 
    qDebug() << __FUNCTION__
             << "rect() =" << rect()
             << "sceneRect() =" << sceneRect()
             << "scene->itemsBoundingRect() =" << scene->itemsBoundingRect();
    zoomFit = getFitScaleFactor(rect(), scene->itemsBoundingRect());
    zoom = zoomFit;
    if (limitFit100Pct  && zoom > 1) zoom = 1;
    scale();
}

void EmbelView::setFitZoom()
{
    if (G::isLogger) G::log(__FUNCTION__); 
    zoom = zoomFit;
    if (limitFit100Pct  && zoom > 1) zoom = 1;
}

void EmbelView::zoomToggle()
{
/*
Alternate between zoom-to-fit and another zoom value (typically 100% to check
detail).  The other zoom value (toggleZoom) can be assigned in ZoomDlg and
defaults to 1.0
*/
    if (G::isLogger) G::log(__FUNCTION__); 
    isFit = !isFit;
    isFit ? zoom = zoomFit : zoom = toggleZoom;
}

void EmbelView::rotate(int degrees)
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
    zoomFit = getFitScaleFactor(rect(), scene->itemsBoundingRect());
//    zoomFit = getFitScaleFactor(rect(), pmItem->boundingRect());

    // if in isFit mode then zoom accordingly
    if (isFit) {
        zoom = zoomFit;
        scale();
    }
}

void EmbelView::rotateByExifRotation(QImage &image, QString &imageFullPath)
{
    if (G::isLogger) G::log(__FUNCTION__); 
    #ifdef ISPROFILE
    G::track(__FUNCTION__, "About to QTransform trans");
    #endif
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

void EmbelView::transform()
{
    if (G::isLogger) G::log(__FUNCTION__); 
    QImage displayImage;
    rotateByExifRotation(displayImage, currentImagePath);
}

void EmbelView::sceneGeometry(QPoint &sceneOrigin, QRectF &scene_Rect, QRect &cwRect)
{
/*
Return the top left corner of the image showing in the central widget in percent.  This is
used to determine the zoomCursor aspect in ThumbView.
*/
    if (G::isLogger) G::log(__FUNCTION__); 
    sceneOrigin = mapFromScene(0.0, 0.0);
    scene_Rect = sceneRect();
    cwRect = rect();
}

void EmbelView::monitorCursorState()
{
    if (G::isLogger) G::log(__FUNCTION__); 
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

void EmbelView::setBackgroundColor(QColor bg)
{
    scene->setBackgroundBrush(bg);
}

void EmbelView::setCursorHiding(bool hide)
{
    if (G::isLogger) G::log(__FUNCTION__); 
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

void EmbelView::hideCursor()
{
/*
Called from mouse move event in a delay if in slideshow mode.
*/
    setCursor(Qt::BlankCursor);
}

// EVENTS

void EmbelView::paintEvent(QPaintEvent *event)
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

void EmbelView::scrollContentsBy(int dx, int dy)
{
    scrollCount++;
    QGraphicsView::scrollContentsBy(dx, dy);
}

// MOUSE CONTROL

//void EmbelView::dragMoveEvent(QDragMoveEvent *event)
//{
//    qDebug() << G::t.restart() << "\t" << "drag";
//}

void EmbelView::wheelEvent(QWheelEvent *event)
{
    if (G::isLogger) G::log(__FUNCTION__); 

    // if trackpad scrolling set in preferences then default behavior
//    if(useWheelToScroll && isScrollable) {
//        qDebug() << __FUNCTION__ << zoom << isScrollable;
//        QGraphicsView::wheelEvent(event);
//        isTrackpadScroll = true;
//        return;
//    }

    // wheel scrolling / trackpad swiping = next/previous image
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
void EmbelView::mouseDoubleClickEvent(QMouseEvent *event)
{
    if (G::isLogger) G::log(__FUNCTION__); 
    // placeholder function pending use
//    qDebug() << __FUNCTION__ << isFit << zoom << zoomFit;
//    if (isFit && zoom < zoomFit) {
//        zoom = zoomFit;
//        scale();
//        isMouseDoubleClick = true;
//    }
    QWidget::mouseDoubleClickEvent(event);

}

void EmbelView::mousePressEvent(QMouseEvent *event)
{
    if (G::isLogger) G::log(__FUNCTION__); 
    static int n = 0;
    n++;
    // bad things happen if no image when click
    if (currentImagePath.isEmpty()) return;

    // prevent zooming when right click for context menu
    if (event->button() == Qt::RightButton) {
        return;
    }

    // forward and back buttons
    if (event->button() == Qt::BackButton) {
//        thumbView->selectPrev();
        return;
    }
    if (event->button() == Qt::ForwardButton) {
//        thumbView->selectNext();
        return;
    }

    // prevent zooming when double mouse click
//    if (event->button() == Qt::Dou) {
//        return;
//    }
    if (isMouseDoubleClick) return;

    isMouseDoubleClick = false;
    isMouseDrag = false;
    if (event->button() == Qt::LeftButton) {
        if (isRubberBand) {
            setCursor(Qt::CrossCursor);
            origin = event->pos();
            rubberBand->setGeometry(QRect(origin, QSize()));
            rubberBand->show();
            return;
        }
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

void EmbelView::mouseMoveEvent(QMouseEvent *event)
{
/*
Pan the image during a mouse drag operation
Set a delay to hide cursor if in slideshow mode
*/
    {
    #ifdef ISDEBUG
//    G::track(__FUNCTION__);
    #endif
    }
    if (isRubberBand) {
        rubberBand->setGeometry(QRect(origin, event->pos()).normalized());
        return;
    }

    static QPoint prevPos = event->pos();

    if (isLeftMouseBtnPressed && event->pos() != prevPos) {
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

void EmbelView::mouseReleaseEvent(QMouseEvent *event)
{
    if (G::isLogger) G::log(__FUNCTION__); 

    // rubberband
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
        emit newTile();
        return;
    }

    // prevent zooming when right click for context menu
    if (event->button() == Qt::RightButton) {
        return;
    }

    // prevent zooming when forward and back buttons
    if (event->button() == Qt::BackButton || event->button() == Qt::ForwardButton) return;

//    if (isMouseDoubleClick) return;

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

void EmbelView::enterEvent(QEvent *event)
{
    if (G::isLogger) G::log(__FUNCTION__); 
    QVariant x = event->type();     // suppress compiler warning
    this->setFocus();
}

// DRAG AND DROP

void EmbelView::dragEnterEvent(QDragEnterEvent *event)
{
    event->acceptProposedAction();
}

void EmbelView::dropEvent(QDropEvent *event)
{
    emit handleDrop(event->mimeData());
}

QString EmbelView::diagnostics()
{
    QString reportString;
    QTextStream rpt;
    rpt.setString(&reportString);
    rpt << Utilities::centeredRptHdr('=', "EmbelView Diagnostics");
    rpt << "\n";
    rpt << "\n" << "isBusy = " << G::s(isBusy);
    rpt << "\n" << "currentImagePath = " << G::s(currentImagePath);
    rpt << "\n" << "firstImageLoaded = " << G::s(isFirstImageNewFolder);
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
    rpt << "\n" << "full = " << G::s(full.width()) << "," << G::s(full.height());

    rpt << "\n\n" ;
    return reportString;
}

// COPY AND PASTE

void EmbelView::copyImage()
{
    if (G::isLogger) G::log(__FUNCTION__); 
    QApplication::clipboard()->setPixmap(pmItem->pixmap(), QClipboard::Clipboard);
    QString msg = "Copied current image to the clipboard";
    G::popUp->showPopup(msg, 1500);
}
