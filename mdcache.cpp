#include "global.h"
#include "mdcache.h"

MetadataCache::MetadataCache(QObject *parent, DataModel *dm,
                  Metadata *metadata) : QThread(parent)
{
    {
    #ifdef ISDEBUG
    qDebug() << "MetadataCache::MetadataCache";
    #endif
    }
    this->dm = dm;
    this->metadata = metadata;
    thumb = new Thumb(this, metadata);
    restart = false;
    abort = false;
    thumbMax.setWidth(160);       // rgh review hard coding thumb size
    thumbMax.setHeight(160);      // rgh review hard coding thumb size
}

MetadataCache::~MetadataCache()
{
    mutex.lock();
    abort = true;
    condition.wakeOne();
    mutex.unlock();
    wait();
}

void MetadataCache::stopMetadateCache()
{
    {
    #ifdef ISDEBUG
    qDebug() << "MetadataCache::stopMetadateCache";
    #endif
    }
    if (isRunning()) {
        mutex.lock();
        abort = true;
        condition.wakeOne();
        mutex.unlock();
        wait();
        emit updateIsRunning(false);
    }
}

void MetadataCache::loadMetadataCache(int startRow)
{
    {
    #ifdef ISDEBUG
    qDebug() << "MetadataCache::loadMetadataCache";
    #endif
    }
    qDebug() << "MetadataCache::loadMetadataCache  startRow =" << startRow;
    if (isRunning()) {
        mutex.lock();
        abort = true;
        condition.wakeOne();
        mutex.unlock();
        wait();
    }
    abort = false;
    allMetadataLoaded = false;
    this->startRow = startRow;
    checkIfNewFolder();
    start(TimeCriticalPriority);
}

void MetadataCache::checkIfNewFolder()
{
/*
Create a map for every row in the datamodel to track metadata caching.  This is
used to confirm all the metadata is loaded before ending the metadata cache.
*/
    {
    #ifdef ISDEBUG
    qDebug() << "MetadataCache::checkIfNewFolder";
    #endif
    }
    if (dm->currentFolderPath != folderPath) {
        loadMap.clear();
        for(int i = 0; i < dm->rowCount(); ++i) loadMap[i] = false;
        folderPath = dm->currentFolderPath;
        allMetadataLoaded = false;
        isShowCacheStatus = true;  // rgh
        qDebug() << "pxUnitWidth" << pxUnitWidth;
        createCacheStatus();
        emit showCacheStatus(*cacheStatusImage);
    }
}

void MetadataCache::createCacheStatus()
{
    {
    #ifdef ISDEBUG
    qDebug() << "ImageCache::cacheStatus";
    #endif
    }
    /* Displays a statusbar showing the cache status.
     * dependent on setTargetRange() being up-to-date

     The app status bar is 25 pixels high.  Create a bitmap the height of the
     status bar and cache.pxTotWidth wide the same color as the app status bar.
     Then paint in the cache status progress in the middle of the bitmap.

   (where cache.pxTotWidth = 200, htOffset(9) + ht(8) = 17)
       X = pnt background, P = Cache Progress Area

    0,0  XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX  0,200
         XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
         XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
    0,9  PPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPP
         PPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPP
    0,17 PPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPP
         XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
         XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
    0,25 XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX  200,25

    for image n:
        pxStart(n) returns the x coordinate for the startpoint of image n
        pxMid(n) returns the x coordinate for the midpoint of image n
        pxEnd(n) returns the x coordinate for the endpoint of image n
        cache.pxUnitWidth = the width in pixels represented by an image

*/

    // Match the background color in the app status bar so blends in
    QColor cacheBGColor = QColor(85,85,85);

    // create a label bitmap to paint on
    cacheStatusImage = new QImage(QSize(pxTotWidth, 25), QImage::Format_RGB32);
//    QImage cacheStatusImage(QSize(pxTotWidth, 25), QImage::Format_RGB32);
    cacheStatusImage->fill(cacheBGColor);
    pnt = new QPainter(cacheStatusImage);

    pxTotWidth = 400;  // rgh
    pxUnitWidth = qRound((float)pxTotWidth/dm->rowCount());
    if (pxUnitWidth == 0) pxUnitWidth++;

    htOffset = 9;       // the offset from the top of pnt to the progress bar
    ht = 8;             // the height of the progress bar

    // back color for the entire progress bar for all the files
    QLinearGradient cacheAllColor(0, htOffset, 0, ht+htOffset);
    cacheAllColor.setColorAt(0, QColor(150,150,150));
    cacheAllColor.setColorAt(1, QColor(100,100,100));

    // color for the portion that has been cached
    loadedGradient = new QLinearGradient(0, htOffset, 0, ht+htOffset);
    // Purple/Blue
    loadedGradient->setColorAt(0, QColor(100,100,150));
    loadedGradient->setColorAt(1, QColor(50,50,100));    // show the rectangle for entire bar, representing all the files available
    // Green
//    loadedGradient->setColorAt(0, QColor(108,150,108));
//    loadedGradient->setColorAt(1, QColor(58,100,58));    // show the rectangle for entire bar, representing all the files available

    pnt->fillRect(QRect(0, htOffset, pxTotWidth, ht), cacheAllColor);
}

void MetadataCache::updateCacheStatus(int row)
{
    // show the rectangle for the current cache by painting each item that has been cached
    int totRows = dm->rowCount();
    int pxStart = ((float)row / totRows) * pxTotWidth;
//    qDebug() << "pxStart" << pxStart  << "row" << row << "totRows" << totRows;
    pnt->fillRect(QRect(pxStart, htOffset, pxUnitWidth+1, ht), *loadedGradient);

    // ping mainwindow to show cache update in the status bar
    if (isShowCacheStatus) emit showCacheStatus(*cacheStatusImage);
    //    pnt.fillRect(QRect(100, htOffset, 50+1, ht), cacheCurrentColor);
}

void MetadataCache::track(QString fPath, QString msg)
{
    if (G::isThreadTrackingOn) qDebug() << "• Metadata Caching" << fPath << msg;
}

void MetadataCache::loadMetadata()
{
/*
Load the metadata and thumb (icon) for all the image files in a folder.
*/
    {
    #ifdef ISDEBUG
    qDebug() << "MetadataCache::loadMetadata";
    #endif
    }
//    qDebug() << "MetadataCache::loadMetadata    startRow ="
//             << startRow
//             << "  allMetadataLoaded =" << allMetadataLoaded;
    int thumbCacheThreshold = 20;
    int totRows = dm->rowCount();
    for (int row = startRow; row < totRows; ++row) {
        if (abort) {
            qDebug() << "Aborting at row =" << row;
            emit updateAllMetadataLoaded(allMetadataLoaded);
            emit updateIsRunning(false);
            return;
        }
        // is metadata already cached
        if (loadMap[row]) continue;

        // maybe metadata loaded by user picking image while cache is still building
        idx = dm->index(row, 0);
        QString fPath = idx.data(Qt::ToolTipRole).toString();
        bool thumbLoaded = idx.data(Qt::DecorationRole).isValid();
        bool metadataLoaded = metadata->isLoaded(fPath);
        if (metadataLoaded && thumbLoaded) {
            loadMap[row] = true;
            updateCacheStatus(row);
            continue;
        }

        // update status
        QString s = "Loading metadata " + QString::number(row + 1) + " of " + QString::number(totRows);
        emit updateStatus(false, s);
//        qDebug() << "Attempting to load metadata for row" << row << fPath;

        // load metadata
        if (!metadataLoaded) {
          QFileInfo fileInfo(fPath);
            metadataLoaded = true;
            // tried emit signal to meatdata but really slow
            // emit loadImageMetadata(fileInfo, true, true, false);
            mutex.lock();
            if (metadata->loadImageMetadata(fileInfo, true, true, false)) {
                metadataLoaded = true;
            }
            mutex.unlock();

//            if (row % thumbCacheThreshold == 0) {
//                emit refreshThumbs();
//            }
        }

        if (!thumbLoaded) {
            QImage image;
            thumbLoaded = thumb->loadThumb(fPath, image);
//            QImage *imagePtr = &image;
            if (thumbLoaded) emit setIcon(row, image);
        }

        if (metadataLoaded && thumbLoaded) {
            loadMap[row] = true;
            updateCacheStatus(row);
        }
    }
}

void MetadataCache::run()
{
/*
Load all metadata, make multiple attempts if not all metadata loaded in the
first pass through the images in the folder.  The QMap loadedMap is used to
track if the metadata and thumb have been cached for each image.  When all items
in loadedMap == true then okay to finish.

While the MetadataCache is loading the metadata and thumbs the user can impact
the process in two ways:

1.  If the user selects an image and the metadata or thumb has not been loaded
    yet then the main program thread goes ahead and loads the metadata and thumb.
    Metadata::loadMetadata checks for this possibility.

2.  If the user scrolls in the gridView or thumbView a signal is fired in MW and
    loadMetadataCache is called to load the thumbs for the scroll area.  As a
    result the current process is aborted and a new one is started with startRow
    set to the first visible thumb.

Since the user can cause a irregular loading sequence, the do loop makes a sweep
through loadedMap after each call to loadMetadata to see if there are any images
that have been missed.
*/
    {
    #ifdef ISDEBUG
    qDebug() << "MetadataCache::run";
    #endif
    }
    t.start();
    emit updateIsRunning(true);

    do {
        if (abort) {
            qDebug() << "Aborting from MetadataCache::run";
            emit updateAllMetadataLoaded(allMetadataLoaded);
            emit updateIsRunning(false);
            return;
        }
        loadMetadata();
        allMetadataLoaded = true;
        for(int i = 0; i < dm->rowCount(); ++i) {
            if (!loadMap[i]) {
                startRow = i;
                allMetadataLoaded = false;
                break;
            }
        }
    }
    while (!allMetadataLoaded && t.elapsed() < 30000);
    emit updateAllMetadataLoaded(allMetadataLoaded);

    qDebug() << "Total elapsed time to cache metadata =" << t.elapsed() << "ms";

    /* after read metadata okay to images, where the target
    cache needs to know how big each image is (width, height) and the offset
    to embedded jpgs */
    emit loadImageCache();

    // update status in statusbar
    emit updateIsRunning(false);
}


