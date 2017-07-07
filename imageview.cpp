#include <QGraphicsDropShadowEffect>
#include <math.h>
#include "global.h"
#include "imageview.h"

#define CLIPBOARD_IMAGE_NAME		"clipboard.png"
#define ROUND(x) ((int) ((x) + 0.5))
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define MIN(a, b) ((a) < (b) ? (a) : (b))

/*
The ImageView coordinate system (units are all pixels) and (0, 0) is the top
left corner in each space.

The image space: the scaled size of the pixmap being displayed.  This can vary from
being much larger than the display window (when zoomed in) to smaller when
zoomed out.

The window space: the area in the application that contains the image.  The
window can be a different aspect ratio from the image.

The view space: the portion of the scaled image within the window container.
Since the scaled image can be smaller than the window space, for example, when the image aspect ratio
is different from the window aspect ratio, and scaled to fit in the window. The
view space cannot exceed the window space, as it is defined as the part of the window
where the scaled image is being displayed.

When the view space is smaller than the window space, either in width or height, then
it is centered in the window space.

SEE "WINNOW COORDINATE SPACES.PNG" in images in resources for an illustration

Naming convention

    i.w, i.h = image size
    w.w, w.h = window size
    v.w, v.h = view size

    i.x, i.y = image coordinate
    w.x, w.y = window coordinate
    v.x, v.y = view coordinates

Objects used

    image space  = imageLabel->pixmap()
    window space = this
    view space   = imageLabel  // scaled to window space

The view scaling is controlled by the zoomFactor.  When zoomFactor = 1
the view is as 100%, the same as the original image.

 */

ImageView::ImageView(QWidget *parent, Metadata *metadata,
                     ImageCache *imageCacheThread, bool isShootingInfoVisible) : QWidget(parent)
{
    {
    #ifdef ISDEBUG
    qDebug() << "ImageView::ImageView";
    #endif
    }

    this->mainWindow = parent;
    this->metadata = metadata;
    this->imageCacheThread = imageCacheThread;

    shootingInfoVisible = isShootingInfoVisible;
    cursorIsHidden = false;
    moveImageLocked = false;
    imageLabel = new QLabel;
    imageLabel->setScaledContents(true);

    QHBoxLayout *mainLayout = new QHBoxLayout();
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);
    mainLayout->addWidget(imageLabel);

    scrlArea = new QScrollArea;
    scrlArea->setContentsMargins(0, 0, 0, 0);
    scrlArea->setAlignment(Qt::AlignCenter);
    scrlArea->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scrlArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scrlArea->verticalScrollBar()->blockSignals(true);
    scrlArea->horizontalScrollBar()->blockSignals(true);
    scrlArea->setFrameStyle(0);
    scrlArea->setLayout(mainLayout);
    scrlArea->setWidgetResizable(true);

    QVBoxLayout *scrollLayout = new QVBoxLayout;
    scrollLayout->setContentsMargins(0, 0, 0, 0);
    scrollLayout->setSpacing(0);
    scrollLayout->addWidget(scrlArea);
    this->setLayout(scrollLayout);

    infoDropShadow = new DropShadowLabel(this);
    infoDropShadow->setVisible(shootingInfoVisible);
    infoDropShadow->setAttribute(Qt::WA_TranslucentBackground);

    infoLabel = new QLabel(this);
    infoLabel->setVisible(shootingInfoVisible);
    infoLabelShadow = new QLabel(this);
    infoLabelShadow->setVisible(shootingInfoVisible);
    infoLabel->setAttribute(Qt::WA_TranslucentBackground);
    infoLabelShadow->setAttribute(Qt::WA_TranslucentBackground);

    pickLabel = new QLabel(this);
    pickLabel->setFixedSize(40,48);
    pickLabel->setAttribute(Qt::WA_TranslucentBackground);
    thumbsUp.load(":/images/ThumbsUp48.png");
    pickLabel->setPixmap(QPixmap::fromImage(thumbsUp));
    pickLabel->setVisible(false);

    QGraphicsOpacityEffect *infoEffect = new QGraphicsOpacityEffect;
    infoEffect->setOpacity(0.8);
    infoLabel->setGraphicsEffect(infoEffect);
    infoLabelShadow->setGraphicsEffect(infoEffect);
    infoDropShadow->setGraphicsEffect(infoEffect);

    mouseMovementTimer = new QTimer(this);
    connect(mouseMovementTimer, SIGNAL(timeout()), this, SLOT(monitorCursorState()));

    mouseZoomFit = true;
    isMouseDrag = false;
    isMouseDoubleClick = false;
}

bool ImageView::loadImage(QString fPath)
{
    {
    #ifdef ISDEBUG
    qDebug() << "ImageView::loadImage";
    #endif
    }

     /* important to keep currentImagePath.  It is used to check if there isn't
     an image (when currentImagePath.isEmpty() == true) - for example when
     no folder has been chosen. */
    currentImagePath = fPath;

    // No folder selected yet
    if (fPath == "") return false;

    bool isLoaded = false;
    // load the image from the image cache if available
    if (imageCacheThread->imCache.contains(fPath)) {
        displayPixmap = QPixmap::fromImage(imageCacheThread->imCache.value(fPath));
        (displayPixmap.isNull()) ? isLoaded = false : isLoaded = true;
    }
    // load the image from the image file
    if (!isLoaded) {
        for (int i=0; i<10000; i++) {
            isLoaded = loadPixmap(fPath, displayPixmap);
            if (isLoaded) break;
        }
    }
    if (isLoaded) {
        displayPixmap.setDevicePixelRatio(G::devicePixelRatio);
        imageLabel->setPixmap(displayPixmap);
        resizeImage();
    }
    return isLoaded;
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
    // except no pixmap pm in ImageCache::loadPixmap

    bool success = false;
    int totDelay = 500;     // milliseconds
    int msDelay = 0;        // total incremented delay
    int msInc = 10;         // amount to increment each try

    QString err;            // type of error

    ulong offsetFullJpg = 0;
    QImage image;           // not included in ImageCache::loadPixmap
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

    pm = QPixmap::fromImage(image);   // not included in ImageCache::loadPixmap

    // must adjust pixmap dpi in case retinal display macs
//    pm.setDevicePixelRatio(GData::devicePixelRatio);

    // record any errors
    if (!success) metadata->setErr(fPath, err);

    return success;
}

void ImageView::clear() {
    {
    #ifdef ISDEBUG
    qDebug() << "ImageView::clear";
    #endif
    }
    imageLabel->clear();
}

QRect ImageView::windowRect()
{
    {
    #ifdef ISDEBUG
    qDebug() << "ImageView::canvasViewRect";
    #endif
    }
    int w = imageLabel->width();
    int h = imageLabel->height();
    int x = imageLabel->x();
    int y = imageLabel->y();
    return QRect(x, y, w, h);
}

bool ImageView::inImageView(QPoint canvasPt)
{
/*  Used to determine if a mouse click is in the imave view area */
    {
    #ifdef ISDEBUG
    qDebug() << "ImageView::inImageView";
    #endif
    }
    return windowRect().contains(canvasPt, false);
}

void ImageView::resizeImage()
{
    {
    #ifdef ISDEBUG
    qDebug() << "ImageView::resizeImage";
    #endif
    }

    /*  See top of file for a description of the three coordinate spaces
        used in the ImageView class.
    */

    // Program opening - no folder selected yet
    if (currentImagePath == "") return;

    // get the size of the scaled image
    QSize imgSize = imageLabel->pixmap()->size();

    // current image and window dimensions
    // wait until after scaling to define view space.
    w.w = width();                  // window space
    w.h = height();
    i.w = imgSize.width();          // image space
    i.h = imgSize.height();

    // get scale of view (imageLabel)
    if (mouseZoomFit) {
        // fit small images
        if (i.w < w.w && i.h < w.h)
            imgSize.scale(i.w, i.h, Qt::KeepAspectRatio);
        // fit large images
        if (i.w >= w.w || i.h >= w.h) {
            imgSize.scale(w.w, w.h, Qt::KeepAspectRatio);
        }
    }
    else {
        imgSize.scale(i.w * zoom, i.h * zoom, Qt::KeepAspectRatio);
    }

    // resize the imageLabel
    imageLabel->setVisible(false);
    imageLabel->setFixedSize(imgSize);
    imageLabel->setVisible(true);

    // new view space
    v.w = imageLabel->width();
    v.h = imageLabel->height();

    // calc current zoom amount - zoom should not change unless zoomToFit
    floatSize z;
    z.w = (float)v.w / i.w;
    z.h = (float)v.h / i.h;
    zoom = (z.w > z.h) ? z.w : z.h;

    // current zoomFit amount
    z.w = (float)w.w / i.w;
    z.h = (float)w.h / i.h;
    zoomFit = (z.w > z.h) ? z.w : z.h;

    (zoom > zoomFit) ? isZoom = true : isZoom = false;

    if (isZoom) setCursor(Qt::OpenHandCursor);
    else setCursor(Qt::ArrowCursor);

    // can the entire image fit in the view window
//    if (i.w <= v.w && i.h <= v.h) {
//        canZoom = false;
//    }
//    else {
//        canZoom = true;
//    }

/*    qDebug() << "AFTER RESIZE" << currentImagePath
             << "\nimage width  ="  << i.w << "\timage height  =" << i.h
             << "\nview width   ="  << v.w <<  "\tview height   =" << v.h
             << "\nwindow width =" << w.w << "\twindow height =" << w.h
             << "\nimageLabel pos =" << imageLabel->pos()
             << "window pos =" << this->pos()
             << "\nzoom factor  =" << zoom << "zoomFit =" << zoomFit;
*/

    /* realign image based on mouse click position or to center
    - if fit or smaller than window container or coord outside image
    - if zoom using keyboard "Z" then no mouse coord so zoom to center
    */
    if (zoom > zoomFit && (mouse.x > 0 || mouse.y > 0)) moveImage(imgSize);
    else centerImage(imgSize);
    movePickIcon();
    setInfo(metadata->getShootingInfo(currentImagePath));

    // update status with current zoom magnification (call back from status)
//    QString msg = QString::number(zoom*100, 'f', 0) + "% zoom";
    emit updateStatus(true, "");
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
    w.w = width();                      // width of the window container
    w.h = height();                     // height of the window container
    v.w = imageLabel->width();          // width of the image view
    v.h = imageLabel->height();         // height of the image view
    intSize p;
    p.w = pickLabel->width();           // width of the pick symbol
    p.h = pickLabel->height();          // height of the pick symbol
    int offset = 10;                    // offset pixels from the edge of image
    int x, y = 0;                       // top left coordinates of pick symbol

    // if the image view is not as wide as the window
    if (v.w < w.w) x = (w.w - v.w)/2 + v.w - p.w - offset;
    else x = w.w - p.w - offset;

    // if the image view is not as high as the window
    if (v.h < w.h) y = (w.h - v.h)/2 + v.h - p.h - offset;
    else y = w.h - p.h - offset;

    pickLabel->move(x, y);
}

void ImageView::resizeEvent(QResizeEvent *event)
{
    {
    #ifdef ISDEBUG
    qDebug() << "ImageView::resizeEvent";
    #endif
    }
    QWidget::resizeEvent(event);
    resizeImage();
    movePickIcon();
}

void ImageView::centerImage(QSize &imgSize)
{
/* When the aspect ratios of the image and window are different or the scaled
image is smaller than the window the viewspace will have empty margins. The
origin of the viewspace is the top left of the window. This function shifts the
imageLabel to center in the window.
*/
    {
    #ifdef ISDEBUG
    qDebug() << "ImageView::centerImage";
    #endif
    }

    pt nPos;
    nPos.x = (width() - imgSize.width()) / 2;
    nPos.y = (height() - imgSize.height()) / 2;

    if (nPos.x != imageLabel->pos().x() || nPos.y != imageLabel->pos().y()) {
        imageLabel->move(nPos.x, nPos.y);
    }
}

void ImageView::moveImage(QSize &imgSize)
{
/*  When a resize occurs, recenter on the mouse click location.

The window (label) and view are different if the aspect ratio of the image and
the window are different. We start with the mouse click coordinates in window
space, convert to view space and then to the image space.

See top of file for coordinate spaces info.
*/
    {
    #ifdef ISDEBUG qDebug() << "ImageView::moveImage";
    #endif
    }
    intSize vAdj;
    pt nPos;

    w.w = width();
    w.h = height();
    i.w = imgSize.width();
    i.h = imgSize.height();

/*   Determine view width and height based on aspect ratio.  Either the
     left/right or top/bottom will be less than the window unless the
     aspect ratio of the image and the window are the same.  The margin
     is defined in vAdg.  */
    v.w = w.w;
    vAdj.w = 0;
    v.h = w.h;
    vAdj.h = 0;
    float aspectRatio = (float)i.h / i.w;
    if (aspectRatio < 1.0) {
        v.h = aspectRatio * w.w;
        vAdj.h = (w.h - v.h) / 2;
    } else {
        v.w = w.h/aspectRatio;
        vAdj.w = (w.w - v.w) / 2;
    }

    // get the view coordinates
    v.x = mouse.x - vAdj.w;
    v.y = mouse.y - vAdj.h;

    // get the image coordinates
    i.x = (int)((float)(v.x) *  i.w / v.w);
    i.y = (int)((float)(v.y * i.h) / v.h);

    // calc the adjustment from the origin
    nPos.x = mouse.x - i.x;
    nPos.y = mouse.y - i.y;

    // move unless the mouse click position translates into the current position
    if (nPos.x != imageLabel->pos().x() || nPos.y != imageLabel->pos().y()) {
        imageLabel->move(nPos.x, nPos.y);
    }
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
        QSize imgSize = imageLabel->pixmap()->size();
        i.w = imgSize.width();
        i.h = imgSize.height();

        pt nPos, offset;

        // translate mouse click on thumb to full size image coordinates
        i.x = xPct * i.w;
        i.y = yPct * i.h;

        // offsets from label origin to center
        offset.x = w.w / 2;
        offset.y = w.h / 2;

        // correct for edge conditions
        if (i.x - offset.x < 0) i.x = offset.x;
        if (i.y - offset.y < 0) i.y = offset.y;
        if (i.x + offset.x > i.w) i.x = i.w - offset.x;
        if (i.y + offset.y > i.h) i.y = i.h - offset.y;

        nPos.x = offset.x - i.x;
        nPos.y = offset.y - i.y;
/*
        qDebug() <<   "Image  width / height" << imageW << imageH
                 << "\nCanvas width / height" << canvasW << canvasH
                 << "\nOffset X / Y         " << offsetX << offsetY
                 << "\nImage  X / Y         " << imageX << imageY
                 << "\nNew    X / Y         " << newX << newY
                 << "\n-----------------------------------------------";
*/
        imageLabel->move(nPos.x, nPos.y);
    }
}
void ImageView::zoomIn()
{
    zoom *= (1.0 + zoomInc);
    qDebug() << "zoomInc" << zoomInc;
    zoom > zoomMax ? zoomMax: zoom;
    mouseZoomFit = false;
    resizeImage();
}

void ImageView::zoomOut()
{
    zoom *= (1.0 - zoomInc);
    zoom < zoomMin ? zoomMin : zoom;
    mouseZoomFit = false;
    resizeImage();
}

void ImageView::zoom100()
{
    zoom = 1;
    mouseZoomFit = false;
    resizeImage();
}

void ImageView::zoomToFit()
{
    mouseZoomFit = true;
    resizeImage();
}

void ImageView::zoomTo(float zoomTo)
{
    zoom = zoomTo;
    zoom < 0.05 ? 0.05 : zoom;
    zoom > 8 ? 8: zoom;
    mouseZoomFit = false;
    resizeImage();
}

void ImageView::zoomToggle()
{
    qDebug() << "zoomToggle  isZoom =" << isZoom;
    mouseZoomFit = isZoom;
    if (!isZoom) zoom = 1;
    resizeImage();
}

void ImageView::setImageLabelSize(QSize newSize)
/* Req'd for placement in compare view where size is not known in advance  */
{
    qDebug() << "Before: this / imageLabel / newSize" << this->size() << imageLabel->size() << newSize;
//    imageLabel->resize(newSize);
    imageLabel->pixmap()->scaled(newSize);
    qDebug() << "After:  this / imageLabel / newSize" << this->size() << imageLabel->size() << newSize;
}

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

void ImageView::refresh()
{
    {
    #ifdef ISDEBUG
    qDebug() << "ImageView::refresh";
    #endif
    }
    displayImage = origImage;
    transform();

    displayPixmap = QPixmap::fromImage(displayImage);
    imageLabel->setPixmap(displayPixmap);
    resizeImage();
}

void ImageView::setInfo(QString infoString)
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

    // if the image is not as wide as the canvas
    if (v.w < w.w) x = (w.w - v.w)/2 + offset;
    else x = offset;

    // if the image is not as high as the canvas
    if (v.h < w.h) y = (w.h - v.h)/2 + offset;
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
    static QPoint lastPos;

    if (QCursor::pos() != lastPos)
    {
        lastPos = QCursor::pos();
        if (cursorIsHidden)
        {
            QApplication::restoreOverrideCursor();
            cursorIsHidden = false;
        }
    }
    else
    {
        if (!cursorIsHidden)
        {
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
        setMouseMoveData(true, event->x(), event->y());
        int x = event->localPos().x();
        int y = event->localPos().y();
        isMouseClickInLabel = inImageView(QPoint(x,y));
/*        qDebug() << "isMouseClickInLabel" << isMouseClickInLabel;
        qDebug() << "mousePressEvent ="
                 << "mouseX =" << x << "mouseY =" << y
                 << "getImageWidth =" << getImageWidth()
                 << "label width =" << imageLabel->width();  */
        if (isZoom) setCursor(Qt::ClosedHandCursor);
        event->accept();
    }
    QWidget::mousePressEvent(event);
}

void ImageView::mouseReleaseEvent(QMouseEvent *event)
{
    {
    #ifdef ISDEBUG
    qDebug() << "ImageView::mouseReleaseEvent";
    #endif
    }
    // bad things happen if no image when click
    if (currentImagePath.isEmpty()) return;
    if (event->button() == Qt::LeftButton) {
//        while (QApplication::overrideCursor()) {
//            QApplication::restoreOverrideCursor();
//        }
        bool okToZoom = (!isMouseDoubleClick
                         && !isMouseDrag
                         && isMouseClickInLabel);
/*        qDebug() << "okToZoom =" << okToZoom
                 << "isMouseDrag =" << isMouseDrag
                 << "isMouseDoubleClick =" << isMouseDoubleClick
                 << "isMouseClickInLabel =" << isMouseClickInLabel
                 << "mouseZoom100Pct =" << mouseZoom100Pct;
                 */
        if (okToZoom) {
            zoom = 1.0;     // if zoomToFit then zoom reset in resize
            mouseZoomFit = !mouseZoomFit;
            setMouseMoveData(false, event->x(), event->y());
            resizeImage();
        }
        isMouseDrag = false;
    }
    if (event->button() == Qt::BackButton) emit togglePick();
    if (isZoom) setCursor(Qt::OpenHandCursor);
    QWidget::mouseReleaseEvent(event);
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
    w.x = imageLabel->pos().x();
    w.y = imageLabel->pos().y();
/*    qDebug() << "mouse x =" << mouse.x << "y =" << mouse.y
            << "w.x = imageLabel->pos().x()" << w.x
             << "w.y = imageLabel->pos().y()" << w.y;
             */
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
    if (event->modifiers() == Qt::ControlModifier) {
    } else {
        if (moveImageLocked)
        {
            pt nPos;
            nPos.x = w.x + (event->pos().x() - mouse.x);
            nPos.y = w.y + (event->pos().y() - mouse.y);
            bool needToMove = false;

            if (v.w > w.w) 	{
                if (nPos.x > 0)
                    nPos.x = 0;
                else if (nPos.x < (w.w - v.w))
                    nPos.x = (w.w - v.w);
                needToMove = true;
            }
            else
                nPos.x = w.x;

            if (v.h > w.h) {
                if (nPos.y > 0)
                    nPos.y = 0;
                else if (nPos.y < (w.h - v.h))
                    nPos.y = (w.h - v.h);
                needToMove = true;
            }
            else
                nPos.y = w.y;

            if (needToMove) {
                imageLabel->move(nPos.x, nPos.y);
                isMouseDrag = true;
            }
        }
    }
}

int ImageView::getImageWidth()
{
    {
    #ifdef ISDEBUG
    qDebug() << "ImageView::getImageWidth";
    #endif
    }
    return imageLabel->pixmap()->width();
}

int ImageView::getImageHeight()
{
    {
    #ifdef ISDEBUG
    qDebug() << "ImageView::getImageHeight";
    #endif
    }
    return imageLabel->pixmap()->height();
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

void ImageView::pasteImage()
{
    {
    #ifdef ISDEBUG
    qDebug() << "ImageView::pasteImage";
    #endif
    }
    if (!QApplication::clipboard()->image().isNull()) {
//        origImage = QApplication::clipboard()->image();
//        refresh();
    }
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
