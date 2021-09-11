#include "Cache/imagecache.h"

ImageCache::ImageCache(QObject *parent,
                       ImageCacheData *icd,
                       DataModel *dm,
                       Metadata *metadata)
    : QThread(parent)
{
    if (G::isLogger) G::log(__FUNCTION__);
    // Pixmap is a class that loads either a QPixmap or QImage from a file
    this->icd = icd;
    this->dm = dm;
    this->metadata = metadata;
    //
//    cacheImage = new CacheImage(this, dm, metadata);
    // create n decoder threads
    decoderCount = QThread::idealThreadCount();
    icd->cache.decoderCount = decoderCount;
    for (int id = 0; id < decoderCount; ++id) {
        ImageDecoder *d = new ImageDecoder(this, id, dm, metadata);
        decoder.append(d);
        connect(decoder[id], &ImageDecoder::done, this, &ImageCache::fillCache);
    }
    restart = false;
    abort = false;
}

ImageCache::~ImageCache()
{
    mutex.lock();if (G::isLogger) G::log(__FUNCTION__);mutex.unlock();
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
    . = all the thumbnails in dm->sf (filtered datamodel)
    * = the current thumbnail selected
    T = all the images targeted to cache.  The sum of T fills the assigned
        memory available for the cache ie 3000 MB
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
Data structures:
cache: cache management parameters
    struct Cache {
        uint key;                   // current image
        uint prevKey;               // used to establish directionof travel
        uint toCacheKey;            // next file to cache
        uint toDecacheKey;          // next file to remove from cache
        bool isForward;             // direction of travel in folder
        int wtAhead;                // ratio cache ahead vs behind
        int totFiles;               // number of images available
        uint currMB;                // the current MB consumed by the cache
        uint maxMB;                 // maximum MB available to cache
        uint folderMB;              // MB required for all files in folder
        int targetFirst;            // beginning of target range to cache
        int targetLast;             // end of the target range to cache
        bool isShowCacheStatus;     // show in app status bar
        bool usePreview;            // cache smaller pixmap for speedier initial display
        QSize previewSize;          // monitor display dimensions for scale of previews
    } cache;
cacheItem: the cache status parameters for an image
    struct CacheItem {
        int key;
        int origKey;
        QString fName;
        bool isCached;
        bool isTarget;
        int priority;
        float sizeMB;
    } cacheItem;
cacheItemList:  the list of all the cacheItems
imCache: a hash structure indexed by image file path holding each QImage
Only functions called from ImageCache::run will run in another thread.  These functions must
respect potential thread collision issues.  The functions and objects are:
Functions that run in thread:
    run
    cacheUpToDate
    checkForOrphans
    getImCacheSize
    makeRoom
    memChk
    nextToCache
    nextToDecache
    removeFromCache
    setCurrentPosition
    setDirection
    setKeyToCurrent
    setPriorities
    setTargetRange
    updateImageCacheList
    reportCache
    reportToCache
    reportCacheProgress
Variables used by thread:
    abort
    pause
    currentPath
    prevCurrentPath
    cacheSizeHasChanged
    filterOrSortHasChanged
    imCache (the list of QImage that is also used by ImageView)
    cache struct
    cacheItemList
Multi-thread solution:
    It takes longer to decode a jpg than to read it off the USB media. Set up loop to read
    jpgs and then launch n worker threads to decode each jpg into a QImage and add to the
    imageCache.
    When create ImageCache create n decoders (one per core)
    Change (new folder, new image, filter, sort)
    Launch decoders
    fillCache(decoder id)
        receive signal worker thread
        if not QImage* == nullPtr
            make room
            insert QImage into cache
            quit if cache full or nothing left to cache
        create QImage
        read image file
        decodeWorker(
    // start fillCache
    repeat n times (QThread::idealThrreadCount())
        fillCache()
    Classes changed:
        ImageCache
        Pixmap -> CacheImage
        ImageDecoder
*/

void ImageCache::clearImageCache(bool includeList)
{
    if (G::isLogger) G::log(__FUNCTION__);
    QMutexLocker locker(&mutex);
    icd->imCache.clear();
    icd->cache.currMB = 0;
    // do not clear cacheItemList if called from start slideshow
    if (includeList) icd->cacheItemList.clear();
    updateStatus("Clear Image Cache", __FUNCTION__);
}

void ImageCache::stopImageCache()
{
/*
    Note that initImageCache and updateImageCache both check if isRunning and stop a running
    thread before starting again. Use this function to stop the image caching thread without a
    new one starting when there has been a folder change. The cache status label in the status
    bar will be hidden.
*/
    if (G::isLogger) G::log(__FUNCTION__);
    if (isRunning()) {
        mutex.lock();
        abort = true;
        condition.wakeOne();
        mutex.unlock();
        wait();
        abort = false;
    }
    /*
    for (int id = 0; id < decoderCount; ++id) {
        qDebug() << __FUNCTION__ << id << decoder[id]->isRunning();
    }
    */
    bool allStopped = false;
    while (!allStopped) {
        allStopped = true;
        for (int id = 0; id < decoderCount; ++id) {
            if (decoder[id]->isRunning()) allStopped = false;
            if (!allStopped) break;
        }
    }

    emit updateIsRunning(false, false);  // flags = isRunning, showCacheLabel
    clearImageCache(true);

    /* confirm all decoders finished
    for (int id = 0; id < decoderCount; ++id) {
        mutex.lock();
        if (decoder[id]->isRunning()) {
            qDebug() << __FUNCTION__ << "decoder" << id << "is still running.";
        }
        else {
            qDebug() << __FUNCTION__ << "decoder" << id << "is finished.";
        }
        mutex.unlock();
    }
    */
}

void ImageCache::pauseImageCache()
{
/*
    Note that initImageCache and updateImageCache both check if isRunning and stop a running
    thread before starting again. Use this function to pause the image caching thread without
    a new one starting when there is a change to the datamodel, such as filtration or sorting.
*/
    if (G::isLogger) G::log(__FUNCTION__);
    mutex.lock();
    pause = true;
    mutex.unlock();
    return;

//    if (G::isLogger) G::log(__FUNCTION__);
//    if (isRunning()) {
//        mutex.lock();
//        abort = true;
//        condition.wakeOne();
//        mutex.unlock();
//        wait();
//        abort = false;
//        emit updateIsRunning(false, true);
//    }
}

void ImageCache::resumeImageCache()
{
/*
    Restart the image cache after it has been paused. This is used by metadataCacheThread
    after scroll events and the imageCacheThread has been paused to resume loading the image
    cache.
*/
    if (G::isLogger) G::log(__FUNCTION__);
    QMutexLocker locker(&mutex);
    pause = false;
    return;
//    mutex.lock();if (G::isLogger) G::log(__FUNCTION__);mutex.unlock();
//    qDebug() << __FUNCTION__ << "Starting thread" << isRunning();
    source = __FUNCTION__;
    if (isRunning()) {
        abort = true;
        condition.wakeOne();
        wait();
        abort = false;
    }
    start(IdlePriority);
}

QSize ImageCache::scalePreview(int w, int h)
{
    if (G::isLogger) G::log(__FUNCTION__);
    QSize preview(w, h);
    preview.scale(icd->cache.previewSize.width(), icd->cache.previewSize.height(),
                  Qt::KeepAspectRatio);
    return preview;
}

QSize ImageCache::getPreviewSize()
{
//    mutex.lock();if (G::isLogger) G::log(__FUNCTION__);mutex.unlock();
    return icd->cache.previewSize;
}

int ImageCache::getImCacheSize()
{
    // return the current size of the cache
    if (G::isLogger) G::log(__FUNCTION__);
    int cacheMB = 0;
    for (int i = 0; i < icd->cacheItemList.size(); ++i) {
        if (icd->cacheItemList.at(i).isCached) {
            cacheMB += icd->cacheItemList.at(i).sizeMB;
        }
    }
    return cacheMB;
}

bool ImageCache::prioritySort(const ImageCacheData::CacheItem &p1,
                              const ImageCacheData::CacheItem &p2)
{
    return p1.priority < p2.priority;       // sort by priority
}

bool ImageCache::keySort(const ImageCacheData::CacheItem &k1,
                         const ImageCacheData::CacheItem &k2)
{
    return k1.key < k2.key;       // sort by key to return to thumbnail order
}

void ImageCache::setKeyToCurrent()
{
/*
    cache.key is the index of the item in cacheItemList that matches dm->currentFilePath
*/
    if (G::isLogger) G::log(__FUNCTION__);
    icd->cache.key = 0;
    for (int i = 0; i < icd->cacheItemList.count(); i++) {
        if (icd->cacheItemList.at(i).fPath == dm->currentFilePath) {
            icd->cache.key = i;
            return;
        }
    }
}

int ImageCache::getCacheKey(QString fPath)
{
    if (G::isLogger) G::log(__FUNCTION__);
    icd->cache.key = 0;
    for (int i = 0; i < icd->cacheItemList.count(); i++) {
        if (icd->cacheItemList.at(i).fPath == fPath) {
            return i;
        }
    }
    return -1;
}

void ImageCache::setDirection()
{
/*
    If the direction of travel changes then delay reversing the caching direction until a
    third image is selected in the new direction of travel. This prevents needless caching if
    the user justs reverses direction to check out the previous image
*/
    if (G::isLogger) G::log(__FUNCTION__);

    int prevKey = icd->cache.prevKey;
    icd->cache.prevKey = icd->cache.key;

    bool startOrEnd = false;
    // start of list
    if (icd->cache.key == 0) {
        icd->cache.isForward = true;
        startOrEnd = true;
    }
    // reverse if at end of list
    if (icd->cache.key == icd->cacheItemList.count() - 1) {
        icd->cache.isForward = false;
        startOrEnd = true;
    }
    if (startOrEnd) {
        icd->cache.sumStep = 0;
        icd->cache.maybeDirectionChange = false;
        return;
    }

    icd->cache.directionChangeThreshold = 3;
    int thisStep = icd->cache.key - prevKey;
    bool maybeDirection = thisStep > 0;

    /*
    qDebug() << __FUNCTION__
             << "cache.maybeDirectionChange =" << cache.maybeDirectionChange
             << "cache.isForward =" << cache.isForward
             << "maybeDirection =" << maybeDirection
             << "thisStep =" << thisStep
             << "cache.key =" << cache.key
             << "prevKey =" << prevKey
                ;
                // */

    // if direction has not maybe changed
    if (!icd->cache.maybeDirectionChange && icd->cache.isForward == maybeDirection) {
        icd->cache.sumStep = 0;
        return;
    }

    // direction maybe changed
    icd->cache.maybeDirectionChange = true;
    icd->cache.sumStep += thisStep;

    // maybe direction change gets to threshold then change cache direction
    if (qFabs(icd->cache.sumStep) >= icd->cache.directionChangeThreshold) {
        icd->cache.isForward = icd->cache.sumStep > 0;
        icd->cache.sumStep = 0;
        icd->cache.maybeDirectionChange = false;
//        qDebug() << __FUNCTION__ << "Cache direction change.  isForward =" << cache.isForward;
    }

//    qDebug() << __FUNCTION__ << thisStep << cache.sumStep << cache.isForward;
}

void ImageCache::setTargetRange()
/*
    The target range is the list of images being targeted to cache, based on the current image,
    the direction of travel, the caching strategy and the maximum memory allotted to the image
    cache.
    The start and end of the target range are determined (cache.targetFirst and
    cache.targetLast) and the boolean isTarget is assigned for each item in in the
    cacheItemList.
*/
{
    if (G::isLogger) G::log(__FUNCTION__);

    // sort by priority to make it easy to find highest priority not already cached
    std::sort(icd->cacheItemList.begin(), icd->cacheItemList.end(), &ImageCache::prioritySort);

    // assign target files to cache
    int sumMB = 0;
    for (int i = 0; i < icd->cacheItemList.length(); ++i) {
        sumMB += icd->cacheItemList.at(i).sizeMB;
        if (sumMB < icd->cache.maxMB) {
            icd->cacheItemList[i].isTarget = true;
         }
        else {
            icd->cacheItemList[i].isTarget = false;
        }
    }

    // return order to key - same as dm->sf (sorted or filtered datamodel)
    std::sort(icd->cacheItemList.begin(), icd->cacheItemList.end(), &ImageCache::keySort);

    int i;
    for (i = 0; i < icd->cacheItemList.length(); ++i) {
        if (icd->cacheItemList.at(i).isTarget) {
            icd->cache.targetFirst = i;
            break;
        }
    }
    for (int j = i; j < icd->cacheItemList.length(); ++j) {
        if (!icd->cacheItemList.at(j).isTarget) {
            icd->cache.targetLast = j - 1;
            break;
        }
        icd->cache.targetLast = icd->cache.totFiles - 1;
    }

    fixOrphans();

    if (debugCaching) {
        qDebug();
        qDebug() << __FUNCTION__
                 << "targetFirst =" << icd->cache.targetFirst
                 << "targetLast =" << icd->cache.targetLast
                 << "isForward =" << icd->cache.isForward
                    ;
    }
}

bool ImageCache::inTargetRange(QString fPath)
{
    for (int i = 0; i < icd->cacheItemList.length(); ++i) {
        if (icd->cacheItemList.at(i).fPath == fPath)
            if (icd->cacheItemList.at(i).isTarget) return true;
    }
    return false;
}

bool ImageCache::cacheHasMissing()
{
    // clear all items where isCaching = true
    for (int i = 0; i < icd->cacheItemList.length(); ++i) {
        if (icd->cacheItemList.at(i).isCaching == true) {
            icd->cacheItemList[i].isCaching = false;
        }
    }
    // still items to cache in target range? If so, get first key and return true
    for (int i = 0; i < icd->cacheItemList.length(); ++i) {
        if (icd->cacheItemList.at(i).isTarget) {
            if (!icd->cacheItemList.at(i).isCached) {
                icd->cache.toCacheKey = i;
                return true;
            }
        }
    }
    return false;
}

bool ImageCache::nextToCache(int id)
{
/*
    The next image to cache is determined by traversing the cacheItemList in ascending order
    to find the highest priority item to cache in the target range based on these criteria:

    - isCaching and isCached are false.
    - decoderId matches item, isCaching is true and isCached = false.  If this is the case then
      we know the previous attempt failed, and we should try again, unless the attempts are
      greater than 2.
*/
    if (G::isLogger) G::log(__FUNCTION__);
    int lastPriority = icd->cacheItemList.length();
    int key = -1;
    // find next priority item
    for (int i = 0; i < icd->cacheItemList.length(); ++i) {
        bool isTarget = icd->cacheItemList.at(i).isTarget;
        bool isCaching = icd->cacheItemList.at(i).isCaching;
        bool isCached = icd->cacheItemList.at(i).isCached;
        int attempts = icd->cacheItemList.at(i).attempts;
        int threadId = icd->cacheItemList.at(i).threadId;
        // chk orphaned items rgh what about imCache - maybe still in imCache
        if (isTarget && !isCached && attempts < maxAttemptsToCacheImage) {
            if (!isCaching || (isCaching && id == threadId)) {
                // higher priorities are lower numbers (highest = 0)
                if (icd->cacheItemList.at(i).priority < lastPriority) {
                    lastPriority = icd->cacheItemList.at(i).priority;
                    key = i;
                }
            }
        }
    }
    if (key > -1) {
        icd->cache.toCacheKey = key;
        if (debugCaching) {
            QString k = QString::number(key).leftJustified((4));
            qDebug().noquote() << __FUNCTION__ << "    decoder" << id << "key =" << k;
        }
        return true;
    }
    return false;
}

bool ImageCache::nextToDecache(int id)
{
/*
    The next image to decache is determined by traversing the cacheItemList in descending
    order to find the first image currently cached.
*/
    if (G::isLogger) G::log(__FUNCTION__);
    int lastPriority = 0;
    int key = -1;
    // find next priority item
    for (int i = icd->cacheItemList.length() - 1; i > -1; --i) {
        bool isCached = icd->cacheItemList.at(i).isCached;
        if (isCached) {
            // higher priorities are lower numbers
            if (icd->cacheItemList.at(i).priority > lastPriority) {
                lastPriority = icd->cacheItemList.at(i).priority;
                key = i;
            }
        }
    }
    if (key > -1) {
        icd->cache.toDecacheKey = key;
        if (debugCaching) {
            QString k = QString::number(key).leftJustified((4));
            qDebug().noquote() << __FUNCTION__ << "  decoder" << id << "key =" << k;
        }
        return true;
    }
    return false;
}

void ImageCache::setPriorities(int key)
{
    if (G::isLogger) G::log(__FUNCTION__, "key = " +QString::number(key));
    // key = current position = current selected thumbnail
    int aheadAmount = 1;
    int behindAmount = 1;                   // default 50/50 weighting
    int wtAhead = icd->cache.wtAhead;
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

//    qDebug() << __FUNCTION__ << "key =" << key << "cacheItemList.length() =" << cacheItemList.length();
    if (key < icd->cacheItemList.length())
        icd->cacheItemList[key].priority = 0;
    int i = 1;                  // start at 1 because current pos preset to zero
    if (icd->cache.isForward) {
        aheadPos = key + 1;
        behindPos = key - 1;
        while (i < icd->cacheItemList.length()) {
            for (int b = behindPos; b > behindPos - behindAmount; --b) {
                for (int a = aheadPos; a < aheadPos + aheadAmount; ++a) {
                    if (a >= icd->cacheItemList.length()) break;
                    icd->cacheItemList[a].priority = i++;
                    if (i >= icd->cacheItemList.length()) break;
                    if (a == aheadPos + aheadAmount - 1 && b < 0) aheadPos += aheadAmount;
                }
                aheadPos += aheadAmount;
                if (b < 0) break;
                icd->cacheItemList[b].priority = i++;
                if (i > icd->cacheItemList.length()) break;
            }
            behindPos -= behindAmount;
        }
    }
    else {
        aheadPos = key - 1;
        behindPos = key + 1;
        while (i < icd->cacheItemList.length()) {
            for (int b = behindPos; b < behindPos + behindAmount; ++b) {
                for (int a = aheadPos; a > aheadPos - aheadAmount; --a) {
                    if (a < 0) break;
                    icd->cacheItemList[a].priority = i++;
                    if (i >= icd->cacheItemList.length()) break;
                    if (a == aheadPos - aheadAmount + 1 && b > icd->cache.totFiles)
                        aheadPos -= aheadAmount;
                }
                aheadPos -= aheadAmount;
                if (b >= icd->cacheItemList.length()) break;
                icd->cacheItemList[b].priority = i++;
                if (i > icd->cacheItemList.length()) break;
            }
            behindPos += behindAmount;
        }
    }

//    reportCacheManager("setPriorities");
}

void ImageCache::fixOrphans()
{
/*

*/
    for (int i = 0; i < icd->cacheItemList.length(); ++i) {
        QString fPath = icd->cacheItemList.at(i).fPath;
        bool isCached = icd->cacheItemList.at(i).isCached;
        bool isCaching = icd->cacheItemList.at(i).isCaching;
        bool inImageCache = icd->imCache.contains(fPath);
        if (icd->cacheItemList.at(i).isTarget) {
            if (isCaching) icd->cacheItemList[i].isCaching = false;
        }
        else {
            if (isCached) icd->cacheItemList[i].isCached = false;
            if (isCaching) icd->cacheItemList[i].isCaching = false;
            if (inImageCache) icd->imCache.remove(fPath);
            if (isCached) emit updateCacheOnThumbs(fPath, false);
        }
    }
}

bool ImageCache::cacheUpToDate()
{
/*
    Determine if all images in the target range are cached or being cached.
*/
    int first = icd->cache.targetFirst;
    int last = icd->cache.targetLast;
    for (int i = first; i < last + 1; ++i) {
        bool isCached = icd->cacheItemList.at(i).isCached;
        bool isCaching = icd->cacheItemList.at(i).isCaching;
        if (!isCached && !isCaching) {
            if (icd->cacheItemList.at(i).attempts < maxAttemptsToCacheImage) {
                icd->cache.toCacheKey = i;
                return false;
            }
        }
    }
    return true;
}

void ImageCache::makeRoom(int id, int cacheKey)
{
/*
    Called from the running thread when a new image from the nextToCache QList is iterated.
    Remove images from the QHash imCache, based on the order of the QList toDecache, until
    there is enough room to add the new image. Update cacheItemList, thumb status indicators,
    and the current available room in the cache.
*/
    if (G::isLogger) G::log(__FUNCTION__);
    if (cacheKey >= icd->cacheItemList.length()) return;
//    icd->cache.currMB = getImCacheSize();
    int room = icd->cache.maxMB - icd->cache.currMB;
    int roomRqd = icd->cacheItemList.at(cacheKey).sizeMB;
    while (room < roomRqd) {
        // make some room by removing lowest priority cached image(s)
        if (nextToDecache(id)) {
            QString s = icd->cacheItemList[icd->cache.toDecacheKey].fPath;
            icd->imCache.remove(s);
            emit updateCacheOnThumbs(s, false);
            icd->cacheItemList[icd->cache.toDecacheKey].isCached = false;
            icd->cacheItemList[icd->cache.toDecacheKey].isCaching = false;
            icd->cache.currMB -= icd->cacheItemList[icd->cache.toDecacheKey].sizeMB;
            room = icd->cache.maxMB - icd->cache.currMB;
            if (debugCaching) {
                QString k = QString::number(icd->cache.toDecacheKey).leftJustified((4));
                qDebug().noquote() << __FUNCTION__
                         << "       decoder" << id << "key =" << k
                         << "room =" << room
                         << "roomRqd =" << roomRqd
                         << "Removed image" << s
                            ;
            }
        }
        else {
            qDebug() << __FUNCTION__ << "No cached images remaining to decache";
            break;
        }
    }
}

void ImageCache::memChk()
{
/*
    Check to make sure there is still room in system memory (heap) for the image cache. If
    something else (another program) has used the system memory then reduce the size of the
    cache so it still fits.
*/
    if (G::isLogger) G::log(__FUNCTION__);

    // get available memory
    #ifdef Q_OS_WIN
    Win::availableMemory();     // sets G::availableMemoryMB
    #endif

    #ifdef Q_OS_MAC
    Mac::availableMemory();     // sets G::availableMemoryMB
    #endif

    // still fit cache?
    int roomInCache = icd->cache.maxMB - icd->cache.currMB;
    if (G::availableMemoryMB < roomInCache) {
        icd->cache.maxMB = icd->cache.currMB + G::availableMemoryMB;
    }
    if (icd->cache.maxMB < icd->cache.minMB) icd->cache.maxMB = icd->cache.minMB;
}

// rgh cachechange not being used???
void ImageCache::removeFromCache(QStringList &pathList)
{
    if (G::isLogger) G::log(__FUNCTION__);
    for (int i = 0; i < pathList.count(); ++i) {
        QString fPath = pathList.at(i);
//        imCache.remove(fPath);
        icd->imCache.remove(fPath);
        for (int j = 0; j < icd->cacheItemList.length(); ++j) {
            if (icd->cacheItemList.at(j).fPath == fPath) {
                icd->cacheItemList.removeAt(j);
            }
        }
        icd->cache.totFiles = icd->cacheItemList.length();
    }

    // trigger change to ImageCache
    setCurrentPosition(dm->currentFilePath);
}

void ImageCache::updateStatus(QString instruction, QString source)
{
//    updateCached();
//    emit showCacheStatus(instruction, icd->cache, source);
}

QString ImageCache::diagnostics()
{
    if (G::isLogger) G::log(__FUNCTION__);
    QString reportString;
    QTextStream rpt;
    rpt.setString(&reportString);
    rpt << Utilities::centeredRptHdr('=', objectName() + " ImageCache Diagnostics");
    rpt << "\n" ;
    rpt << reportCache("");
    rpt << reportImCache();

    rpt << "\n\n" ;
    return reportString;
}

QString ImageCache::reportCache(QString title)
{
    if (G::isLogger) G::log(__FUNCTION__);
    qDebug() << __FUNCTION__;
    QString reportString;
    QTextStream rpt;
    rpt.flush();
    reportString = "";
    rpt.setString(&reportString);

    rpt << "\ncacheItemList (used to manage image cache):";
    rpt  << "\n Title:" << title
         << "  Key:" << icd->cache.key
         << "  cacheMB:" << icd->cache.currMB
         << "  Wt ahead:" << icd->cache.wtAhead
         << "  Direction ahead:" << icd->cache.isForward
         << "  Total files:" << icd->cache.totFiles << "\n";
    int cachedCount = 0;
    for (int i = 0; i < icd->cacheItemList.length(); ++i) {
        int row = dm->fPathRow[icd->cacheItemList.at(i).fPath];
        if (i % 40 == 0) {
            rpt.reset();
            rpt.setFieldAlignment(QTextStream::AlignRight);
            rpt.setFieldWidth(9);
            rpt
                << "Key"
                << "Priority"
                << "Target"
                << "Attempts"
                << "Caching"
                << "Thread"
                << "Cached"
                << "SizeMB"
                   ;
            rpt.setFieldWidth(3);
            rpt << "   ";
            rpt.setFieldAlignment(QTextStream::AlignLeft);
            rpt.setFieldWidth(50);
            rpt << "File Name";
            rpt.setFieldWidth(0);
            rpt << "\n";
        }
        rpt.setFieldWidth(9);
        rpt.setFieldAlignment(QTextStream::AlignRight);
        rpt
            << icd->cacheItemList.at(i).key
            << icd->cacheItemList.at(i).priority
            << icd->cacheItemList.at(i).isTarget
            << icd->cacheItemList.at(i).attempts
            << icd->cacheItemList.at(i).isCaching
            << icd->cacheItemList.at(i).threadId
            << icd->cacheItemList.at(i).isCached
            << icd->cacheItemList.at(i).sizeMB
               ;
        rpt.setFieldWidth(3);
        rpt << "   ";
        rpt.setFieldAlignment(QTextStream::AlignLeft);
        rpt.setFieldWidth(50);
        rpt << icd->cacheItemList.at(i).fPath;
        rpt.setFieldWidth(0);
        rpt << "\n";
        if (icd->cacheItemList.at(i).isCached) cachedCount++;
    }
    rpt << cachedCount << " images reported as cached." << "\n";
//    std::cout << reportString.toStdString() << std::flush;
    return reportString;
}

QString ImageCache::reportImCache()
{
    QString reportString;
    QTextStream rpt;
    rpt.flush();
    reportString = "";
    rpt.setString(&reportString);
    rpt << "\nimCache hash:";
    QHash<QString, QImage>::iterator i;
    // replace with new HashMap imd->imCache
    /*
    for (i = imCache.begin(); i != imCache.end(); ++i) {
        rpt << "\n";
        rpt << G::s(i.key());
        rpt << ": image width = " << G::s(i.value().width());
        rpt << " height = " << G::s(i.value().height());
    }
    */
    QVector<QString> keys;
    // rgh check when imCache is empty
    icd->imCache.getKeys(keys);
    for (int i = 0; i < keys.length(); ++i) {
        rpt << "\n";
        rpt << i << keys.at(i);
    }
    rpt << "\n" << keys.length() << " images in image cache.";

    return reportString;
}

QString ImageCache::reportCacheProgress(QString action)
{
//    if (G::isLogger) {mutex.lock(); G::log(__FUNCTION__); mutex.unlock();}
    QString reportString;
    QTextStream rpt;
    rpt.flush();
    reportString = "";
    rpt.setString(&reportString);
    if (action == "Hdr") {
        rpt << "\nCache Progress:  total cache allocation = " << icd->cache.maxMB << "MB\n";
        rpt.reset();
        rpt.setFieldAlignment(QTextStream::AlignLeft);
        rpt.setFieldWidth(8); rpt << "Action";
        rpt.setFieldAlignment(QTextStream::AlignRight);
        rpt.setFieldWidth(7); rpt << "currMB";
        rpt.setFieldWidth(8); rpt << "nTarget";
        rpt.setFieldWidth(8); rpt << "sTarget";
        rpt.setFieldWidth(8); rpt << "eTarget";
        rpt.setFieldWidth(7); rpt << "nCache";
        rpt.setFieldWidth(9); rpt << "nDecache\n";
        qDebug() << G::t.restart() << "\t" << reportString;
        return reportString;
    }

    rpt.flush();
    rpt.setFieldAlignment(QTextStream::AlignLeft);
    rpt.setFieldWidth(8);  rpt << action;
    rpt.setFieldAlignment(QTextStream::AlignRight);
    rpt.setFieldWidth(7);  rpt << icd->cache.currMB;
    rpt.setFieldWidth(8);  rpt << icd->cache.targetLast - icd->cache.targetFirst;
    rpt.setFieldWidth(8);  rpt << icd->cache.targetFirst;
    rpt.setFieldWidth(8);  rpt << icd->cache.targetLast;
//    rpt.setFieldWidth(7);  rpt << currMB;
//    rpt.setFieldWidth(9);  rpt << currMB;
    return reportString;
}

void ImageCache::reportRunStatus()
{
    QMutexLocker locker(&mutex);
    bool isRun = isRunning();
    qDebug() << __FUNCTION__
             << "isRunning =" << isRun
             << "isForward =" << icd->cache.isForward
             << "abort =" << abort
             << "pause =" << pause
             << "filterSortChange =" << filterOrSortHasChanged
             << "cacheSizeChange =" << cacheSizeHasChanged
             << "currentPath =" << currentPath;
}

void ImageCache::buildImageCacheList()
{
/*
    The imageCacheList must match dm->sf and contains the information required to maintain the
    image cache. It takes the form (example):
    Index Key OrigKey Priority Target Cached  SizeMB  Width Height  File Name
        0   0       0        0      1      0  65.792   5472   3078  D:/Pictures/_xmptest/2017-01-25_0911.tif
        1   1       1        1      1      0   77.97   5472   3648  D:/Pictures/_xmptest/Canon.JPG
        2   2       2        2      1      0   77.97   5472   3648  D:/Pictures/_xmptest/Canon1.cr2
        3   3       3        3      1      0   77.97   5472   3648  D:/Pictures/_xmptest/CanonPs.jpg
    It is built from dm->sf (sorted and/or filtered datamodel).
*/
    if (G::isLogger) G::log(__FUNCTION__);
//    qDebug() << __FUNCTION__ << "for" << dm->currentFolderPath;
    icd->cacheItemList.clear();
    // the total memory size of all the images in the folder currently selected
    float folderMB = 0;
    icd->cache.totFiles = dm->sf->rowCount();

    for (int i = 0; i < dm->sf->rowCount(); ++i) {
        QString fPath = dm->sf->index(i, G::PathColumn).data(G::PathRole).toString();
        /* cacheItemList is a list of cacheItem used to track the current
           cache status and make future caching decisions for each image  */
        icd->cacheItem.key = i;              // need to be able to sync with imageList
        icd->cacheItem.origKey = i;          // req'd while setting target range
        icd->cacheItem.fPath = fPath;
        icd->cacheItem.isCaching = false;
        icd->cacheItem.attempts = 0;
        icd->cacheItem.threadId = -1;
        icd->cacheItem.isCached = false;
        icd->cacheItem.isTarget = false;
        icd->cacheItem.priority = i;
        // 8 bits X 3 channels + 8 bit depth = (32*w*h)/8/1024/1024 = w*h/262144
        float w = dm->sf->index(i, G::WidthColumn).data().toFloat();
        int h = dm->sf->index(i, G::HeightColumn).data().toInt();
        icd->cacheItem.sizeMB = static_cast<int>(w * h / 262144);
        /*
        if (cache.usePreview) {
            QSize p = scalePreview(w, h);
            w = p.width();
            h = p.height();
            cacheItem.sizeMB += (float)w * h / 262144;
        }
        // */
        icd->cacheItem.isMetadata = w > 0;
        icd->cacheItemList.append(icd->cacheItem);

        folderMB += icd->cacheItem.sizeMB;
    }
    icd->cache.folderMB = qRound(folderMB);
}

void ImageCache::updateImageCacheList()
{
/*
    Update the width, height, size and isMetadata fields in the imageCacheList.
*/
    if (G::isLogger) G::log(__FUNCTION__);
    for (int i = 0; i < dm->sf->rowCount(); ++i) {
        if (i >= icd->cacheItemList.length()) {
//            /*
            qDebug() << __FUNCTION__
                 << "cacheItemList[0].fName" << icd->cacheItemList.at(i).fPath
                 << "ITEM" << i
                 << "EXCEEDS CACHEITEMLIST LENGTH" << icd->cacheItemList.length()
                 << dm->currentFilePath;
                 //*/
            return;
        }
        if (!icd->cacheItemList.at(i).isMetadata) {
            // assume 8 bits X 3 channels + 8 bit depth = (32*w*h)/8/1024/1024
            ulong w = dm->sf->index(i, G::WidthColumn).data().toInt();
            ulong h = dm->sf->index(i, G::HeightColumn).data().toInt();
            icd->cacheItemList[i].sizeMB = (float)w * h / 262144;
            icd->cacheItemList[i].isMetadata = w > 0;
        }
    }
}

void ImageCache::initImageCache(int &cacheMaxMB, int &cacheMinMB,
     bool &isShowCacheStatus, int &cacheWtAhead,
     bool &usePreview, int &previewWidth, int &previewHeight)
{
    if (G::isLogger || G::isFlowLogger) G::log(__FUNCTION__);
//    qDebug() << __FUNCTION__ << "cacheSizeMB =" << cacheSizeMB;
    // cancel if no images to cache
    if (!dm->sf->rowCount()) return;

    // just in case stopImageCache not called before this
    if (isRunning()) stopImageCache();

    // cache is a structure to hold cache management parameters
    icd->cache.key = 0;
    icd->cache.prevKey = -1;
    // the cache defaults to the first image and a forward selection direction
    icd->cache.isForward = true;
    // the amount of memory to allocate to the cache
    icd->cache.maxMB = cacheMaxMB;
    icd->cache.minMB = cacheMinMB;
    icd->cache.isShowCacheStatus = isShowCacheStatus;
    icd->cache.wtAhead = cacheWtAhead;
    icd->cache.targetFirst = 0;
    icd->cache.targetLast = 0;
    icd->cache.previewSize = QSize(previewWidth, previewHeight);
    icd->cache.usePreview = usePreview;

    if (icd->cache.isShowCacheStatus) updateStatus("Clear", "ImageCache::initImageCache");

    // populate the new folder image list
    buildImageCacheList();

    source = __FUNCTION__;
}

void ImageCache::updateImageCacheParam(int &cacheSizeMB,
                                       int &cacheMinMB,
                                       bool &isShowCacheStatus,
                                       int &cacheWtAhead,
                                       bool &usePreview,
                                       int &previewWidth,
                                       int &previewHeight)
{
/*
    When various image cache parameters are changed in preferences they are updated here.
*/
    if (G::isLogger) G::log(__FUNCTION__);
    QMutexLocker locker(&mutex);
    icd->cache.maxMB = cacheSizeMB;
    icd->cache.minMB = cacheMinMB;
    icd->cache.isShowCacheStatus = isShowCacheStatus;
    icd->cache.wtAhead = cacheWtAhead;
    icd->cache.usePreview = usePreview;
    icd->cache.previewSize = QSize(previewWidth, previewHeight);
    qDebug() << __FUNCTION__ << "cache.minMB" << icd->cache.minMB;
}

void ImageCache::rebuildImageCacheParameters(QString &currentImageFullPath, QString source)
{
/*
    When the datamodel is filtered the image cache needs to be updated. The cacheItemList is
    rebuilt for the filtered dataset and isCached updated, the current image is set, and any
    surplus cached images (not in the filtered dataset) are removed from imCache.
    The image cache is now ready to run by callin(fPath)) cacg setCachePosition().
*/
    if (G::isLogger) G::log(__FUNCTION__);
    if(dm->sf->rowCount() == 0) return;

    // pause caching    rgh perhaps enclose entire rebuild in a mutex??
    if (isRunning()) pauseImageCache();

//    qDebug() << __FUNCTION__ << "Source:" << source;
//    std::cout << diagnostics().toStdString() << std::flush;

    // build a new cacheItemList for the filtered/sorted dataset
    buildImageCacheList();

    // update cacheItemList
    icd->cache.key = 0;
    // list of files in cacheItemList (all the filtered files) used to check for surplus
    QStringList filteredList;

    // assign cache.key and update isCached status in cacheItemList
    for (int row = 0; row < icd->cacheItemList.length(); ++row) {
        QString fPath = icd->cacheItemList.at(row).fPath;
        filteredList.append(fPath);
        // get key for current image
        if (fPath == currentImageFullPath) icd->cache.key = row;
        // update cacheItemList for images already cached
//        if (imCache.contains(fPath)) cacheItemList[row].isCached = true;
        if (icd->imCache.contains(fPath)) icd->cacheItemList[row].isCached = true;
    }

    // if the sort has been reversed
    if (source == "SortChange") icd->cache.isForward = !icd->cache.isForward;

    setPriorities(icd->cache.key);
    setTargetRange();
//    reportCache("AFTER SORT CHANGE");
//    std::cout << diagnostics().toStdString() << std::flush;

    // remove surplus cached images from imCache if they are not in the filtered dataset
    /*
    QHashIterator<QString, QImage> i(imCache);
    while (i.hasNext()) {
        i.next();
        if (!fList.contains(i.key())) imCache.remove(i.key());
    }
    */
    QVector<QString> keys;
    icd->imCache.getKeys(keys);
    for (int i = 0; i < keys.length(); ++i) {
        if (!filteredList.contains(keys.at(i))) icd->imCache.remove(keys.at(i));
    }

    if (icd->cache.isShowCacheStatus)
        updateStatus("Update all rows", "ImageCache::rebuildImageCacheParameters");

    if (isRunning() && G::isNewFolderLoaded) {
        mutex.lock();
        filterOrSortHasChanged = true;
        pause = false;
        mutex.unlock();
    }
//    else {
//        qDebug() << __FUNCTION__ << "start()";
//        start();
//    }
}

void ImageCache::refreshImageCache()
{
/*
    Reload all images in the cache.
*/
    if (G::isLogger) G::log(__FUNCTION__);
    QMutexLocker locker(&mutex);
    // make all isCached = false
    for (int i = 0; i < icd->cacheItemList.length(); ++i) {
        if (icd->cacheItemList[i].isCached == true) {
            icd->cacheItemList[i].isCached = false;
        }
    }
    refreshCache = true;
    if (!isRunning()) {
        qDebug() << __FUNCTION__ << "start()";
        start();
    }
}

void ImageCache::colorManageChange()
{
/*
    Called when color manage is toggled.  Reload all images in the cache.
*/
    if (G::isLogger) { G::log(__FUNCTION__); }
    refreshImageCache();
}

void ImageCache::cacheSizeChange()
{
    if (G::isLogger) G::log(__FUNCTION__);
    QMutexLocker locker(&mutex);
    cacheSizeHasChanged = true;
    if (!isRunning()) {
        qDebug() << __FUNCTION__ << "start()";
        start();
    }
}

void ImageCache::setCurrentPosition(QString path)
{
    if (G::isLogger || G::isFlowLogger) G::log(__FUNCTION__);
    QMutexLocker locker(&mutex);
    if (G::isLogger) G::log(__FUNCTION__, path);
    pause = false;
    currentPath = path;             // memory check
    /*
    qDebug() << __FUNCTION__
             << "filterSortChange =" << filterOrSortHasChanged
             << "cacheSizeChange =" << cacheSizeHasChanged
             << "refreshCache =" << refreshCache
             << "currentPath =" << currentPath
             << "G::availableMemoryMB =" << G::availableMemoryMB
                ;
                // */
    if (!isRunning()) {
//        qDebug() << __FUNCTION__ << "start()";
        start();
    }
}

void ImageCache::decodeNextImage(int id)
{
    /*
    qDebug() << __FUNCTION__
             << "id =" << id
             << "key =" << icd->cache.toCacheKey
             << "attempts =" << icd->cacheItemList[icd->cache.toCacheKey].attempts
             << icd->cacheItemList[icd->cache.toCacheKey].comment
                ;
                //*/
    QString fPath;
    fPath = icd->cacheItemList.at(icd->cache.toCacheKey).fPath;
    icd->cacheItemList[icd->cache.toCacheKey].isCaching = true;
    icd->cacheItemList[icd->cache.toCacheKey].threadId = id;
    icd->cacheItemList[icd->cache.toCacheKey].attempts += 1;
    if (debugCaching) {
        QString k = QString::number(icd->cache.toCacheKey).leftJustified((4));
        qDebug().noquote() << __FUNCTION__ << "decoder" << id
                           << "key =" << k << decoder[id]->fPath;
    }
    decoder[id]->decode(fPath);
}

void ImageCache::cacheImage(int id, int cacheKey)
{
    if (debugCaching) {
        QString k = QString::number(cacheKey).leftJustified((4));
        qDebug().noquote() << __FUNCTION__ << "     decoder" << id << "key =" << k
                 << decoder[id]->fPath;
    }
    makeRoom(id, cacheKey);
    icd->imCache.insert(decoder[id]->fPath, decoder[id]->image);
    icd->cacheItemList[cacheKey].isCaching = false;
    icd->cacheItemList[cacheKey].isCached = true;
    icd->cache.currMB = getImCacheSize();
    emit updateCacheOnThumbs(decoder[id]->fPath, true);
    if (icd->cache.isShowCacheStatus) {
        updateStatus("Update all rows", "ImageCache::run inside loop");
    }
}

void ImageCache::fillCache(int id, bool positionChange)
{
/*
    A number of ImageDecoders are created when ImageCache is created.  Each ImageDecoder runs
    in a separate thread.  The decoders convert an image file into a QImage and then signal
    this function with their id so the QImage can be inserted into the image cache.
    ImageDecoders are launched from CacheImage::run. CacheImage makes sure the necessary
    metadata is available and reads the file and then runs the decoder. This is very
    important, as file reads have to be sequential while the decoding can be performed
    synchronously, which significantly improves performance.

    The ImageDecoder has a status attribute that can be Ready, Busy or Done.  When the decoder
    is created and when the QImage has been inserted into the image cache the status is set to
    Ready.  When the decoder is called from CacheImage the status is set to Busy.  Finally,
    when the decoder finishes the decoding in ImageDecoder::run the status is set to Done.
    Each decoder signals this function when the image has been converted into a QImage.  Here
    the QImage is added to the imCache.  If there are more targeted images to be cached, the
    next one is assigned to the decoder, which is run again.  The decoders keep running until
    all the targeted images have been cached.

    Every time the ImageCache::run function encounters a change trigger (file selection
    change, cache size, color manage or sort/filter change) the image cache parameters are
    updated and this function is called for each Ready decoder. The decoders not Ready are
    Busy and already in the fillCache loop.

    This function is protected with a mutex as it could be signalled simultaneously by several
    ImageDecoders.

    Each decoder follows this basic pattern:
    - nextToCache
    - decodeNextImage
    - fillCache
    - cacheImage
      - makeRoom
      - nextToDecache
*/
    QMutexLocker locker(&mutex);

//    if (abort) return;

    if (positionChange) {
        setKeyToCurrent();
        setDirection();
        icd->cache.currMB = getImCacheSize();
        setPriorities(icd->cache.key);
        setTargetRange();
        if (cacheSizeHasChanged) makeRoom(0, 0);
    }

    bool okToCache = false;
    /* get the key (index to item in icd->cacheItemList) for decoder[id].  If the decoder has
    not been initiated, then decoder[id]->fPath will be "" and there will be no key available
    and -1 will be returned.
    */
    int cacheKey = getCacheKey(decoder[id]->fPath);

    if (debugCaching) {
        QString k = QString::number(cacheKey).leftJustified((4));
        qDebug().noquote() << __FUNCTION__
                 << "      decoder" << id
                 << "key =" << k
                 << "isRunning =" << decoder[id]->isRunning()
                 << decoder[id]->fPath;
    }

    // if decoder failed
    if (decoder[id]->status == ImageDecoder::Failed) {
        if (icd->cache.toCacheKey < icd->cacheItemList.length()) {
            icd->cacheItemList[icd->cache.toCacheKey].isCaching = false;
            icd->cacheItemList[icd->cache.toCacheKey].comment = "key = " +
                    QString::number(cacheKey) + " status: failed";
        }
        cacheKey = -1;
    }

    // range check
    if (cacheKey >= icd->cacheItemList.length()) {
        qWarning() << __FUNCTION__
                   << "cacheKey =" << cacheKey
                   << "EXCEEDS icd->cacheItemList.length() of"
                   << icd->cacheItemList.length()
                      ;
        cacheKey = -1;
    }

    // in target range
    if (cacheKey != -1) {
        if (inTargetRange(decoder[id]->fPath)) {
            okToCache = true;
        }
    }

    //    if (abort) return;

    /*
    qDebug() << __FUNCTION__
             << "id =" << id
             << "Decoded:"
             << cacheKey
             << "targeted =" << targeted
             << "done =" << done
             << decoder[id]->fPath
                ;
                //*/
    // add decoded QImage to cache if in target range.
    if (okToCache) {
        if (!decoder[id]->isRunning()) cacheImage(id, cacheKey);
    }

    /*
    if (decoder[id]->isRunning()) {
        if (debugCaching) {
            qDebug() << __FUNCTION__ << "isRunning = true decoder id =" << id
                     << "cacheKey =" << cacheKey
                     << "fPath =" << decoder[id]->fPath
                        ;
        }
        return;
    }
    */

    // get next image to cache (nextToCache() defines cache.toCacheKey)
    if (nextToCache(id)) {
        if (!decoder[id]->isRunning()) decodeNextImage(id);
    }
    // caching completed
    else {
        emit updateIsRunning(false, true);  // (isRunning, showCacheLabel)
        if (debugCaching) {
            qDebug() << __FUNCTION__
                     << "     decoder " << id
                     << "cacheUpToDate = true";
        }
        if (icd->cache.isShowCacheStatus) {
            updateStatus("Update all rows", "ImageCache::run after check for orphans");
        }
    }
}

void ImageCache::run()
{
/*
    This thread runs continuously, checking if there has there been a file selection, cache
    size, color manage or sort/filter change, which all trigger a change to the image cache.
    The target range is all the files that will fit into the available cache memory based on
    the cache priorities and direction of travel. We want to make sure the target range is
    cached, decaching anything outside the target range to make room as necessary.
    New file selection is confirmed by comparing the current path to the prevCurrentPath. The
    current path is updated by MetadataCache, which is executed for every
    MW::fileSelectionChange. MetadataCache first checks to see if the thumbnails require
    updating and then emits a signal to update setCurrentPosition which is continuously polled
    here.  Changing to position may require rebuilding the target range to cache.
    When the cache size is changed MW::setImageCacheParameters calls cacheSizeChange which sets
    cacheSizeHasChanged, which is continuously polled.  Changing the cache size will change the
    target range.
    If color manage is toggled from MW::toggleColorManage then all the cached images are
    refreshed (decoded again).
    If there is a sort or filter change to the datamodel then the entire cache list has to be
    updated and the cache rebuilt. This is triggered from MW::resortImageCache or
    MW::filterChange, which call rebuildImageCacheParameters.
    When one of the above triggers occur and there is a change to the image cache, the
    continuous polling activates these actions:
    - Wait for all decoder threads to finish
    - Set the cache key to the current image
    - Determine the direction of travel
    - Update the image cache list in case metadata has changed
    - Update the current size of the image cache
    - Set the target range
    - If the cache size has changed make room by deleting excess images
    - Update statusBar cache progress
    - If the cache is up-to-date then return to continuous polling
    - Update cache isRunning light in statusBar
    - Check available memory in case another app has acquired mem
    - Fill the cache by calling fillCache for each decoder thread which iterate through the
      cacheItemList until the cache is full
        - If decoder has loaded a QImage then insert into the image cache
        - If the decoder is ready then get next QImage and signal fillCache
    - Continue polling for another cache change trigger.
*/
//    if (G::isLogger) G::log(__FUNCTION__);

    source = "";
    prevCurrentPath = currentPath;
    icd->cache.sumStep = 0;
    icd->cache.maybeDirectionChange = false;

    /*
    // prep the cache
    setKeyToCurrent();
    setDirection();

    // may be new metadata for image size
//    updateImageCacheList();

    // update current size of the image cache imCache
    icd->cache.currMB = getImCacheSize();

    // set the priorities and target range to cache
    setPriorities(icd->cache.key);
    setTargetRange();

    // if cache size change then make room in new target range
    if (cacheSizeHasChanged) makeRoom(0, 0);

    std::cout << diagnostics().toStdString() << std::flush;
    //reportCache();
//    if (icd->cache.isShowCacheStatus)
//        updateStatus("Update all rows", "ImageCache::run outer loop");

    // are all images in the target range cached
    if (cacheUpToDate() || abort) {
        return;
    }
    //*/

    // signal MW cache status
    emit updateIsRunning(true, true);

    // check available memory (another app may have used or released some memory)
    memChk();

    // fill the cache with images
    bool positionChange = true;
    for (int id = 0; id < decoderCount; ++id) {
        if (!decoder[id]->isRunning()) {
            decoder[id]->fPath = "";
            if (!abort) fillCache(id, positionChange);
            positionChange = false;
        }
    }
}
