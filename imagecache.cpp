#include "imagecache.h"

ImageCache::ImageCache(QObject *parent, Metadata *metadata) : QThread(parent)
{
    this->metadata = metadata;

    restart = false;
    abort = false;
}

ImageCache::~ImageCache()
{
    mutex.lock();
    abort = true;
    condition.wakeOne();
    mutex.unlock();

    wait();
}

/*
How the Image Cache works:


            CCC          CCCCCCCCC
                  TTTTTTTTTTTTTTTTTTTTTTTTTTTTT
    ......................*............................................

    . = all the images in the folder = all the thumbnails
    * = the current thumbnail selected
    T = all the images targeted to cache.  The sum of T fills the assigned
        memory available for the cache ie 1000MB
    C = the images currently cached, which can be fragmented if the user
        is jumping around, selecting images willy-nilly

Procedure every time a new thumbnail is selected:

    1.  Define the target weighting caching strategy ahead and behind.  In
        the illustration we are caching 2 ahead for every 1 behind.

    2.  Assign a priority to cache for each image based on the direction
        of travel and the target weighting strategy.

    3.  Based on the priority and the cache maximum size, assign images
        to target for caching.

    4.  Add images that are targeted and not already cached.  If the cache
        is full before all targets have been cached then remove any cached
        images outside the target range that may have been left over from
        a previous thumbnail selection

*/

void ImageCache::stopImageCache()
{
/* Note that initImageCache and updateImageCache both check if isRunning and
terminate a running thread before starting again.  Use this function to stop
the image caching thread without a new one starting.*/
    {
    #ifdef ISDEBUG
    qDebug() << "ImageCache::stopImageCache";
    #endif
    }
    if (isRunning()) {
        mutex.lock();
        abort = true;
        condition.wakeOne();
        mutex.unlock();
        wait();
        abort = false;
        emit updateIsRunning(false);
    }
}

void ImageCache::track(QString fPath, QString msg)
{
    qDebug() << "•• Thumb Caching  " << fPath << msg;
}


void ImageCache::cacheStatus()
{
    {
    #ifdef ISDEBUG
    qDebug() << "ImageCache::cacheStatus";
    #endif
    }
    /* Displays a statusbar showing the cache status.
     * dependent on setTargetRange() being up-to-date  */

    // trap instance where cache out of sync
    if(cache.totFiles-1 > cacheMgr.length()) return;

    // The app status bar is 25 pixels high.  Create a bitmap the height of the
    // status bar and cache.pxTotWidth wide the same color as the app status bar.
    // Then paint in the cache status progress in the middle of the bitmap.

    /* (where cache.pxTotWidth = 200, htOffset(9) + ht(8) = 17)
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
    QImage pmCacheStatus(QSize(cache.pxTotWidth, 25), QImage::Format_RGB32);
    pmCacheStatus.fill(cacheBGColor);
    QPainter pnt(&pmCacheStatus);

    int htOffset = 9;       // the offset from the top of pnt to the progress bar
    int ht = 8;             // the heigth of the progress bar

    // back color for the entire progress bar for all the files
    QLinearGradient cacheAllColor(0, htOffset, 0, ht+htOffset);
    cacheAllColor.setColorAt(0, QColor(150,150,150));
    cacheAllColor.setColorAt(1, QColor(100,100,100));

    // color for the portion of the total bar that is targeted to be cached
    // this depends on cursor direction, cache size and current cache status
    QLinearGradient cacheTargetColor(0, htOffset, 0, ht+htOffset);
    cacheTargetColor.setColorAt(0, QColor(125,125,125));
    cacheTargetColor.setColorAt(1, QColor(75,75,75));

    // color for the portion that has been cached
    QLinearGradient cacheCurrentColor(0, htOffset, 0, ht+htOffset);
    cacheCurrentColor.setColorAt(0, QColor(108,150,108));
    cacheCurrentColor.setColorAt(1, QColor(58,100,58));

    // color for the current image within the total images
    QLinearGradient cachePosColor(0, htOffset, 0, ht+htOffset);
    cachePosColor.setColorAt(0, QColor(125,0,0));
    cachePosColor.setColorAt(1, QColor(75,0,0));

    // show the rectangle for entire bar, representing all the files available
    pnt.fillRect(QRect(0, htOffset, cache.pxTotWidth, ht), cacheAllColor);

    // show the rectangle for target cache.  If the pos is close to
    // the boundary there could be spillover, which is added to the
    // target range in the other direction.
    int pxTargetStart = pxStart(cache.targetFirst);
    int pxTargetWidth = pxEnd(cache.targetLast) - pxTargetStart;
    pnt.fillRect(QRect(pxTargetStart, htOffset, pxTargetWidth, ht), cacheTargetColor);

    // show the rectangle for the current cache by painting each item that has been cached
    for (int i=0; i<cache.totFiles; ++i) {
        if (cacheMgr.at(i).isCached) {  // crash with i out of limits
            pnt.fillRect(QRect(pxStart(i), htOffset, cache.pxUnitWidth+1, ht), cacheCurrentColor);
        }
    }

    // show the current image position
    pnt.fillRect(QRect(pxStart(cache.key), htOffset, cache.pxUnitWidth+1, ht), cachePosColor);

    // build cache usage string
    QString mbCacheSize = QString::number(cache.currMB)
            + " of "
            + QString::number(cache.maxMB)
            + " MB";
//    qDebug() << "cache size " + mbCacheSize;

    // ping mainwindow to show cache update in the status bar
    emit showCacheStatus(pmCacheStatus, mbCacheSize);
}

ulong ImageCache::getImCacheSize()
{
    // return the current size of the cache
    {
    #ifdef ISDEBUG
    qDebug() << "ImageCache::getImCacheSize";
    #endif
    }
    ulong cacheMB = 0;
    for (int i=0; i<cacheMgr.size(); ++i) {
        if (cacheMgr.at(i).isCached)
            cacheMB += cacheMgr.at(i).sizeMB;
    }
    return cacheMB;
}

bool ImageCache::prioritySort(const CacheItem &p1, const CacheItem &p2)
{
    {
    #ifdef ISDEBUG
    qDebug() << "ImageCache::prioritySort";
    #endif
    }
    return p1.priority < p2.priority;       // sort by priority
}

bool ImageCache::keySort(const CacheItem &k1, const CacheItem &k2)
{
    {
    #ifdef ISDEBUG
    qDebug() << "ImageCache::keySort";
    #endif
    }
    return k1.key < k2.key;       // sort by key to return to thumbnail order
}

void ImageCache::setTargetRange()
// calc cache.targetFirst and cache.targetLast and set isTarget
// in cacheManager.
{
    {
    #ifdef ISDEBUG
    qDebug() << "ImageCache::setTargetRange";
    #endif
    }
    toCache.clear();
    toDecache.clear();

    // sort by priority to make it easy to find highest priority not already cached
    std::sort(cacheMgr.begin(), cacheMgr.end(), &ImageCache::prioritySort);

    // assign target files to cache and build a list by priority
    // also build a list of files to dechache
    uint sumMB = 0;
    for (int i=0; i<cache.totFiles; ++i) {
        sumMB += cacheMgr.at(i).sizeMB;
        if (sumMB < cache.maxMB) {
            cacheMgr[i].isTarget = true;
            if (!cacheMgr.at(i).isCached)
                toCache.append(cacheMgr.at(i).origKey);
        }
        else {
            cacheMgr[i].isTarget = false;
            if (cacheMgr.at(i).isCached)
                toDecache.prepend(cacheMgr.at(i).origKey);
        }
    }

    // return order to key - same as thumbnails
    std::sort(cacheMgr.begin(), cacheMgr.end(), &ImageCache::keySort);

    int i;
    for (i=0; i<cache.totFiles; ++i) {
        if (cacheMgr.at(i).isTarget) {
            cache.targetFirst = i;
            break;
        }
    }
    for (int j=i; j<cache.totFiles; ++j) {
        if (!cacheMgr.at(j).isTarget) {
            cache.targetLast = j-1;
            break;
        }
        cache.targetLast = cache.totFiles-1;
    }
}

bool ImageCache::nextToCache()
{
    {
    #ifdef ISDEBUG
    qDebug() << "ImageCache::nextToCache";
    #endif
    }
//    qDebug() << "ImageCache::nextToCache length =" << toCache.length();
    if (toCache.length() > 0) {
        cache.toCacheKey = toCache.first();
        return true;
    }
    return false;
}

bool ImageCache::nextToDecache()
{
    {
    #ifdef ISDEBUG
    qDebug() << "ImageCache::nextToDecache";
    #endif
    }
    if (toDecache.length() > 0) {
        cache.toDecacheKey = toDecache.first();
        return true;
    }
    return false;
}

void ImageCache::setPriorities(int key)
{
    {
    #ifdef ISDEBUG
    qDebug() << "ImageCache::setPriorities";
    #endif
    }
    // key = current position = current selected thumbnail
    int aheadAmount = 1;
    int behindAmount = 1;                   // default 50/50 weighting
    int wtAhead = cache.wtAhead;
    switch (wtAhead) {
    case 6:
        aheadAmount = 3;
        behindAmount = 2;
        break;
    case 7:
        aheadAmount = 2;
        behindAmount = 1;
        break;
    case 8:
        aheadAmount = 4;
        behindAmount = 1;
        break;
    case 9:
        aheadAmount = 8;
        behindAmount = 1;
        break;
    case 10:
        aheadAmount = 100;
        behindAmount = 1;
        break;
    }
    int aheadPos;
    int behindPos;
    cacheMgr[key].priority = 0;
    int i = 1;                  // start at 1 because current pos preset to zero
    if (cache.isForward) {
        aheadPos = key + 1;
        behindPos = key - 1;
        while (i < cache.totFiles) {
            for (int b = behindPos; b > behindPos - behindAmount; --b) {
                for (int a = aheadPos; a < aheadPos + aheadAmount; ++a) {
                    if (a >= cache.totFiles) break;
                    cacheMgr[a].priority = i++;
                    if (i >= cache.totFiles) break;
                    if (a == aheadPos + aheadAmount - 1 && b < 0) aheadPos += aheadAmount;
                }
                aheadPos += aheadAmount;
                if (b < 0) break;
                cacheMgr[b].priority = i++;
                if (i > cache.totFiles) break;
            }
            behindPos -= behindAmount;
        }
    }
    else {
        aheadPos = key - 1;
        behindPos = key + 1;
        while (i < cache.totFiles) {
            for (int b = behindPos; b < behindPos + behindAmount; ++b) {
                for (int a = aheadPos; a > aheadPos - aheadAmount; --a) {
                    if (a < 0) break;
                    cacheMgr[a].priority = i++;
                    if (i >= cache.totFiles) break;
                    if (a == aheadPos - aheadAmount + 1 && b > cache.totFiles)
                        aheadPos -= aheadAmount;
                }
                aheadPos -= aheadAmount;
                if (b >= cache.totFiles) break;
                cacheMgr[b].priority = i++;
                if (i > cache.totFiles) break;
            }
            behindPos += behindAmount;
        }
    }

//    reportCacheManager("setPriorities");
}

void ImageCache::checkForOrphans()
{
/* If the user jumps around rapidly in a large folder, where the target cache
 * is smaller than the entire folder, it is possible for the nextToCache and
 * nextToDecache collections to get out of sync and leave orphans in the image
 * cache buffer.  This function iterates through the image cache, checking that
 * all cached images are in the target.  If not, they are removed from the
 * cache buffer.
*/
    {
    #ifdef ISDEBUG
    qDebug() << "ImageCache::removeOrphans";
    #endif
    }
    for (int i = 0; i < cache.totFiles; ++i) {
        if (imCache.contains(cacheMgr.at(i).fName)) {
            if (!cacheMgr.at(i).isTarget) {
                imCache.remove(cacheMgr.at(i).fName);
                cacheMgr[i].isCached = false;
/*                qDebug() << "\n***********************************************************************"
                         << "\nREMOVED FROM IMAGE BUFFER:"
                         << cacheMgr.at(i).fName
                         << "\n***********************************************************************";
*/
            }
        }
    }
}

void ImageCache::reportCacheManager(QString title)
{
    {
    #ifdef ISDEBUG
    qDebug() << "ImageCache::reportCacheManager";
    #endif
    }
    qDebug() << "\n" << title << "Key:" << cache.key
             <<  "cacheMB:" << cache.currMB
             << "Wt:" << cache.wtAhead;
    qDebug() << "\nIndex      Key     OrigKey     Priority     Target     Cached       SizeMB    Width      Height      FName";
    for (int i=0; i<cache.totFiles; ++i) {
        qDebug() << i << "\t"
                 << cacheMgr.at(i).key << "\t"
                 << cacheMgr.at(i).origKey << "\t"
                 << cacheMgr.at(i).priority << "\t"
                 << cacheMgr.at(i).isTarget << "\t"
                 << cacheMgr.at(i).isCached << "\t"
                 << cacheMgr.at(i).sizeMB << "\t"
                 << metadata->getWidth(cacheMgr.at(i).fName) << "\t"
                 << metadata->getHeight(cacheMgr.at(i).fName) << "\t"
                 << cacheMgr.at(i).fName;
    }
}

int ImageCache::pxMid(int key)
{
// returns the cache status bar x coordinate for the midpoint of the item key
    {
    #ifdef ISDEBUG
    qDebug() << "ImageCache::pxMid";
    #endif
    }
    return qRound((float)cache.pxUnitWidth * (key+1)
                  - cache.pxUnitWidth/2);
}

int ImageCache::pxStart(int key)
// returns the cache status bar x coordinate for the start of the item key
{
    {
    #ifdef ISDEBUG
    qDebug() << "ImageCache::pxStart";
    #endif
    }
    return qRound((float)cache.pxUnitWidth * key);
}

int ImageCache::pxEnd(int key)
// returns the cache status bar x coordinate for the end of the item key
{
    {
    #ifdef ISDEBUG
    qDebug() << "ImageCache::pxEnd";
    #endif
    }
    return qRound((float)cache.pxUnitWidth * (key+1));
}

void ImageCache::initImageCache(QFileInfoList &imageList, int &cacheSizeMB,
     bool &isShowCacheStatus, int &cacheStatusWidth, int &cacheWtAhead)
{
    {
    #ifdef ISDEBUG
    qDebug() << "ImageCache::initImageCache";
    #endif
    }

    // just in case stopImageCache not called before this
    if (isRunning()) stopImageCache();

    // check if still in same folder
    if (imageList.at(0).absolutePath() == cache.dir) return;

    imCache.clear();
    cacheMgr.clear();

    // cache is a structure to hold cache management parameters
    cache.key = 0;
    cache.prevKey = -1;
    // the cache defaults to the first image and a forward selection direction
    cache.isForward = true;
    // the amount of memory to allocate to the cache
    cache.maxMB = cacheSizeMB;
    cache.isShowCacheStatus = isShowCacheStatus;
    cache.wtAhead = cacheWtAhead;
    // the width of the status progress bar representing all the images
    // to be cached
    cache.pxTotWidth = cacheStatusWidth;
    // the status bar width of a single image
    cache.pxUnitWidth = (float)cache.pxTotWidth/(imageList.size());
    cache.totFiles = imageList.size();
    cache.dir = imageList.at(0).absolutePath();

//    qDebug() << "\n###### Initializing image cache for " << cache.dir << "######";

    // the total memory size of all the images in the folder currently selected
    float folderMB = 0;
    // get some intel on the new folder image list
    for (int i=0; i < imageList.size(); ++i) {
        QFileInfo fileInfo = imageList.at(i);
        QString fPath = fileInfo.absoluteFilePath();

        /* cacheManager is a list of cacheItem used to track the current
           cache status and make future caching decisions for each image  */
        cacheItem.key = i;              // need to be able to sync with imageList
        cacheItem.origKey = i;          // req'd while setting target range
        cacheItem.fName = fileInfo.absoluteFilePath();
        cacheItem.isCached = false;
        cacheItem.isTarget = false;
        cacheItem.priority = i;
        // assume 8 bits X 3 channels + 8 bit depth = (32*w*h)/8/1024000
        ulong w = metadata->getWidth(fPath);
        ulong h = metadata->getHeight(fPath);
        cacheItem.sizeMB = (float)w * h / 256000;
        cacheMgr.append(cacheItem);

        folderMB += cacheItem.sizeMB;
    }
    cache.folderMB = qRound(folderMB);

//    reportCacheManager("Initialize test");
}

void ImageCache::updateImageCache(QFileInfoList &imageList, QString &currentImageFullPath)
{
    {
    #ifdef ISDEBUG
    qDebug() << "ImageView::updateImageCache";
    #endif
    }

    // just in case stopImageCache not called before this
    if (isRunning()) stopImageCache();

    cache.key = imageList.indexOf(currentImageFullPath);
    cacheStatus();
    if (cache.key == cache.prevKey &&
            imageList.at(0).absolutePath() == cache.dir) return;
    cache.isForward = (cache.key >= cache.prevKey);
    cache.prevKey = cache.key;
    cache.currMB = getImCacheSize();
    setPriorities(cache.key);
    setTargetRange();
    start(LowestPriority);
}

void ImageCache::run()
{
    {
    #ifdef ISDEBUG
    qDebug() << "ImageCache::run";
    #endif
    }

/*
   The target range is all the files that will fit into the available cache
   memory based on the cache priorities and direction of travel.  We want to
   make sure the target range is cached, decaching anything outside the target
   range to make room as necessary as image selection changes.
*/

    emit updateIsRunning(true);
    static QString prevFileName ="";
    while (nextToCache()) {
//        qDebug() << "ImageCache::run top of nextToCache while loop.  abort =" << abort;
        if (abort) {
            emit updateIsRunning(false);
            return;
        }

        // check can read image from file
        QString fPath = cacheMgr.at(cache.toCacheKey).fName;
        if (fPath == prevFileName) return;
        if (G::isThreadTrackingOn) track(fPath, "Reading");
        QImage *image = new QImage;
        if (loadPixmap(fPath, *image)) {
            // is there room in cache?
            uint room = cache.maxMB - cache.currMB;
            uint roomRqd = cacheMgr.at(cache.toCacheKey).sizeMB;
            while (room < roomRqd) {
                // make some room by removing lowest priority cached image
                if(nextToDecache()) {
    //                qDebug() << "Removing" << cacheMgr[cache.toDecacheKey].fName
    //                         << "Before count" << imCache.count();
                    imCache.remove(cacheMgr[cache.toDecacheKey].fName);
    //                qDebug() << "After count" << imCache.count();
                    if (!toDecache.isEmpty()) toDecache.removeFirst();
                    cacheMgr[cache.toDecacheKey].isCached = false;
                    cache.currMB -= cacheMgr[cache.toDecacheKey].sizeMB;
                    room = cache.maxMB - cache.currMB;
                }
                else break;
            }
            mutex.lock();
            imCache.insert(fPath, *image);
            cacheMgr[cache.toCacheKey].isCached = true;
            mutex.unlock();
            if (!toCache.isEmpty()) toCache.removeFirst();
            cache.currMB = getImCacheSize();
            cacheStatus();
            delete image;
        }
        prevFileName = fPath;
    }
    checkForOrphans();
    cacheStatus();
    emit updateIsRunning(false);
//    reportCacheManager("Image cache updated for " + cache.dir);
}

bool ImageCache::loadPixmap(QString &fPath, QImage &image)
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

        // !!!!!!!!!!!!!!!!
        // ImageView::loadPixmap and ImageCache::loadPixmap should be the same
        // Except no pixmap pm in ImageCache::loadPixmap

        bool success = false;
        int totDelay = 500;     // milliseconds
        int msDelay = 0;        // total incremented delay
        int msInc = 10;         // amount to increment each try

        QString err;            // type of error

        ulong offsetFullJpg = 0;

//        QImage image;         // not included in ImageCache::loadPixmap
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
                                if (G::isThreadTrackingOn) track(fPath, err);
                                break;
                            }
                        }
                        else {
                            err = "Illegal offset to image";
                            if (G::isThreadTrackingOn) track(fPath, err);
                            break;
                        }
                    }
                    else {
                        err = "Could not open file for image";    // try again
                        if (G::isThreadTrackingOn) track(fPath, err);
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
            // cooked files like jpg, png etc
            do {
                // check if file is locked by another process
                if (imFile.open(QIODevice::ReadOnly)) {
                    // close it to allow qt load to work
                    imFile.close();
                    // directly load the image using qt library
                    success = image.load(fPath);
                    if (!success) {
                        err = "Could not read image";
                        if (G::isThreadTrackingOn) track(fPath, err);
                        break;
                    }
                }
                else {
                    err = "Could not open file for image";    // try again
                    if (G::isThreadTrackingOn) track(fPath, err);
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


//        pm = QPixmap::fromImage(image);  // not included in ImageCache::loadPixmap

        // record any errors
        if (!success) {
            metadata->setErr(fPath, err);
            if (G::isThreadTrackingOn) track(fPath, err);
            if (G::isThreadTrackingOn) track(fPath, "FAILED TO LOAD IMAGE");
        }
        else if (G::isThreadTrackingOn) track(fPath, "Success");

        return success;

}

//bool ImageCache::loadPixmap(QString &fPath, QImage &image)
//{
//    // Get the embedded jpg in raw files, use image.load for cooked files
//    // and convert it into a pixmap in image.
//    {
//    #ifdef ISDEBUG
//    qDebug() << "ImageCache::loadPixmap";
//    #endif
//    }
//    // this thread is competing with the metadataCache and thumbCache
//    // threads to get information from each image file.  Consequently
//    // have to be vigilant for open files.  When open wait a bit and try
//    // again until successful.
//    bool success = false;
//    int totDelay = 50;     // milliseconds
//    int msDelay = 0;            // total incremented delay
//    int msInc = 1;              // amount to increment each try
//    ulong offsetFullJpg = 0;
//    QFileInfo fileInfo(fPath);
//    QString ext = fileInfo.completeSuffix().toLower();
//    if (metadata->rawFormats.contains(ext)) {
//        // raw files not handled by Qt
//        QFile imFile(fPath);
//        do {
//            if (!metadata->isLoaded(fPath)) {
//                metadata->loadImageMetadata(fPath);
//                qDebug() << "No metadata.  Loaded" << fPath;
//            }
//            offsetFullJpg = metadata->getOffsetFullJPG(fPath);
//            // Check if metadata has been cached for this image
//            if (offsetFullJpg > 0) {
//                if (imFile.open(QIODevice::ReadOnly)) {
//                    bool seekSuccess = imFile.seek(offsetFullJpg);
//                    if (seekSuccess) {
//                        QByteArray buf = imFile.read(metadata->getLengthFullJPG(fPath));
//                        success = image.loadFromData(buf, "JPEG");
//                    }
//                }
//            }
///*            qDebug() << "ImageCache::loadPixmap Success =" << success
//                     << "msDelay =" << msDelay
//                     << "offsetFullJpg =" << offsetFullJpg
//                     << "Attempting to load " << imageFullPath;*/
//            if (!success) QThread::msleep(msInc);
//            msDelay += msInc;
//        }
//        while (msDelay < totDelay && offsetFullJpg == 0);
//    }
//    else {
//        // cooked files that Qt can load
//        do {
//            success = image.load(fPath);
///*            qDebug() << "ImageCache::loadPixmap Success =" << success
//                     << "msDelay =" << msDelay
//                     << "offsetFullJpg =" << offsetFullJpg
//                     << "Attempting to load " << imageFullPath;*/
//            if (!success) QThread::msleep(msInc);
//            msDelay += msInc;
//        }
//        while (msDelay < totDelay && !success);
//    }
//    QTransform trans;
//    int orientation = metadata->getImageOrientation(fPath);
//    if (orientation) {
//        switch(orientation) {
//            case 6:
//                trans.rotate(90);
//                image = image.transformed(trans, Qt::SmoothTransformation) ;
//                break;
//            case 8:
//                trans.rotate(270);
//                image = image.transformed(trans, Qt::SmoothTransformation);
//                break;
//        }
//    }
//    return success;
//    // must adjust pixmap dpi in case retinal display macs
////    pm.setDevicePixelRatio(GData::devicePixelRatio);
//}
