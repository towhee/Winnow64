#include "Cache/imagecache.h"


/*
How the Image Cache works:
            CCC         CCCCCCCCCC
                  TTTTTTTTTTTTTTTTTTTTTTTTTTTTT
    .......................*...........................................
    . = all the thumbnails in dm->sf (filtered datamodel)
    * = the current thumbnail selected
    T = all the images targeted to cache.  The sum of T fills the assigned
        memory available for the cache ie 3000 MB
    C = the images currently cached, which can be fragmented if the user
        is jumping around, selecting images willy-nilly

Procedure every time a new thumbnail is selected:

    1. Assign a priority to cache for each image based on the direction of travel and the
    target weighting strategy.

    2. Define the target weighting caching strategy ahead and behind. In the illustration
    we are caching 2 ahead for every 1 behind.

    3. Based on the priority and the cache maximum size, assign images to target for
    caching.

    4. Add images that are targeted and not already cached. If the cache is full before
    all targets have been cached then remove any cached images outside the target range
    that may have been left over from a previous thumbnail selection. The targeted images
    are read by multiple decoders, which signal fillCache with a QImage.

Data structures:

    The main data structures are in a separate class ImageCacheData to facilitate
    concurrent data access. The image cache for QImages reside in the concurrent hash
    table imCache. This table is written to by fillCache and read by ImageView.

    ImageDecoders, each in their own thread, signal ImageCache::fillCache for each QImage.
    They form queued connections so that only one decoder at a time can add images to
    imCache.

    The caching process is managed by the cacheItemList of cacheItem. Each CacheItem
    corresponds to a row in the filtered DataModel dm->sf, or each image in the view. The
    cache status, target, priority and metadata required for decoding are in CacheItem.

Image size in cache:

    The memory required to cache an image is determined while the metadata is being read
    in DataModel and emitted to ImageCache so that the initial target range can be built.
    This is done using the preview image width and height.  However, for images where
    the width and height cannot be predeterined (ie PNG), the initial file size is used
    as an estimate, and the actual QImage size is calculated just prior to adding the
    image to the image cache; setting the width, height and sizeMB; and updating the
    target range and currMB cached.
*/


ImageCache::ImageCache(QObject *parent,
                       ImageCacheData *icd,
                       DataModel *dm/*,
                       Metadata *metadata*/)
    : QThread(parent)
{
    if (G::isLogger) G::log("ImageCache::ImageCache");

    // data is kept in ImageCacheData icd, a concurrent hash table
    this->icd = icd;
    this->dm = dm;
    // new metadata to avoid thread collisions
    metadata = new Metadata;

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
    debugCaching = false;
}

ImageCache::~ImageCache()
{
    mutex.lock();
    if (G::isLogger) G::log("ImageCache::~ImageCache");
    abort = true;
    condition.wakeOne();
    mutex.unlock();
    wait();
}

void ImageCache::clearImageCache(bool includeList)
{
    if (G::isLogger) G::log("ImageCache::clearImageCache");
    mutex.lock();
    icd->imCache.clear();
    icd->cache.currMB = 0;
    // do not clear cacheItemList if called from start slideshow
    if (includeList) icd->cacheItemList.clear();
    mutex.unlock();
    updateStatus("Clear", "ImageCache::clearImageCache");
}

void ImageCache::stop()
{
/*
    Note that initImageCache and updateImageCache both check if isRunning and
    stop a running thread before starting again. Use this function to stop the
    image caching thread without a new one starting when there has been a
    folder change. The cache status label in the status bar will be hidden.
*/
    if (G::isLogger) G::log("ImageCache::stop");
    abort = true;

    // stop decoder threads
    for (int id = 0; id < decoderCount; ++id) {
//        qDebug() << "ImageCache::stop  Stopping decoder thread" << id;
        decoder[id]->stop();
    }
//    qDebug() << "ImageCache::stop  All decoder threads are stopped";
    // stop imagecache thread
    if (isRunning()) {
//        qDebug() << "ImageCache::stop Stopping ImageCache";
        mutex.lock();
        abort = true;
        condition.wakeOne();
        mutex.unlock();
        wait();
        abort = false;
    }
    // signal MW all stopped if a folder change
    if (G::stop) emit stopped("ImageCache");

//    qDebug() << "ImageCache::stop  ImageCache is stopped";
    // turn off caching activity lights on statusbar
    emit updateIsRunning(false, false);  // flags = isRunning, showCacheLabel

    // reset the image cache
    clearImageCache(true);
}

float ImageCache::getImCacheSize()
{
    // return the current size of the cache
    if (G::isLogger) G::log("ImageCache::getImCacheSize");
    float cacheMB = 0;
    for (int i = 0; i < icd->cacheItemList.size(); ++i) {
        if (icd->cacheItemList.at(i).isCached) {
            cacheMB += icd->cacheItemList.at(i).sizeMB;
        }
    }
    if (debugCaching) {
        qDebug().noquote() << "ImageCache::getImCacheSize" << " cache size =" << cacheMB;
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
    if (G::isLogger) G::log("ImageCache::setKeyToCurren");
    icd->cache.key = -1;
    for (int i = 0; i < icd->cacheItemList.count(); i++) {
        if (icd->cacheItemList.at(i).fPath == currentPath) {
            icd->cache.key = i;
            if (debugCaching) {
                qDebug().noquote() << "ImageCache::setKeyToCurrent" << "key =" << i;
            }
            return;
        }
    }
    qWarning() << "WARNING" << "ImageCache::setKeyToCurrent" << "FPATH NOT FOUND:" << currentPath;
}

int ImageCache::keyFromPath(QString path)
{
    if (G::isLogger) G::log("ImageCache::keyFromPath");
    for (int i = 0; i < icd->cacheItemList.count(); i++) {
        if (icd->cacheItemList.at(i).fPath == path) {
            return i;
        }
    }
    return -1;
}

bool ImageCache::isValidKey(int key)
{
    if (key > -1 && key < icd->cacheItemList.size()) return true;
    else return false;
}

void ImageCache::setDirection()
{
/*
    If the direction of travel changes then delay reversing the caching direction until a
    directionChangeThreshold (ie 3rd) image is selected in the new direction of travel. This
    prevents needless caching if the user justs reverses direction to check out the previous
    image
*/
    if (G::isLogger) G::log("ImageCache::setDirection");
    if (debugCaching) {
        qDebug().noquote() << "ImageCache::setDirection";
    }

    int prevKey = icd->cache.prevKey;
    icd->cache.prevKey = icd->cache.key;
    int thisStep = icd->cache.key - prevKey;

    // cache direction just changed, increment counter
    if (icd->cache.sumStep == 0) {
        icd->cache.sumStep += thisStep;
    }
    // cache direction not changed
    else {
        // immediate direction changed, reset counter
        if (icd->cache.sumStep > 0 != thisStep > 0) icd->cache.sumStep = thisStep;
        // increment counter
        else icd->cache.sumStep += thisStep;
    }

    // immediate direction change exceeds threshold
    if (qAbs(icd->cache.sumStep) > icd->cache.directionChangeThreshold) {
        // immediate direction opposite to cache direction
        if (icd->cache.isForward != icd->cache.sumStep > 0) {
            icd->cache.isForward = icd->cache.sumStep > 0;
            icd->cache.sumStep = 0;
        }
    }
}

void ImageCache::setTargetRange()
/*
    The target range is the list of images being targeted to cache, based on the current
    image, the direction of travel, the caching strategy and the maximum memory allotted
    to the image cache.

    The start and end of the target range are determined (cache.targetFirst and
    cache.targetLast) and the boolean isTarget is assigned for each item in in the
    cacheItemList.
*/
{
    if (G::isLogger) G::log("ImageCache::setTargetRange");

    // sort by priority to make it easy to find highest priority not already cached
    std::sort(icd->cacheItemList.begin(), icd->cacheItemList.end(), &ImageCache::prioritySort);

    // assign target files to cache
    float sumMB = 0;
    priorityList.clear();
    for (int i = 0; i < icd->cacheItemList.length(); ++i) {
        if (icd->cacheItemList[i].isVideo) {
            icd->cacheItemList[i].isTarget = false;
            continue;
        }
        if (icd->cacheItemList.at(i).sizeMB < 0.001) {
            icd->cacheItemList[i].isTarget = false;
            continue;
        }
        sumMB += icd->cacheItemList.at(i).sizeMB;
        if (sumMB < icd->cache.maxMB) {
            /*
            qDebug() << "ImageCache::setTargetRange"
                     << i
                     << icd->cacheItemList.at(i).sizeMB
                     << sumMB
                     << icd->cache.maxMB
                        ;
                        //*/
            icd->cacheItemList[i].isTarget = true;
            priorityList.append(icd->cacheItemList.at(i).key);
        }
        else {
            icd->cacheItemList[i].isTarget = false;
        }
    }
//    qDebug() << "ImageCache::setTargetRange" << sumMB << icd->cache.maxMB;

    // return order to key - same as dm->sf (sorted or filtered datamodel)
    std::sort(icd->cacheItemList.begin(), icd->cacheItemList.end(), &ImageCache::keySort);

    // targetFirst, targetLast
    int i;
    for (i = 0; i < icd->cacheItemList.length(); ++i) {
        if (icd->cacheItemList[i].isVideo) continue;
        if (icd->cacheItemList.at(i).isTarget) {
            icd->cache.targetFirst = i;
            break;
        }
    }
    for (int j = i; j < icd->cacheItemList.length(); ++j) {
        if (icd->cacheItemList[j].isVideo) continue;
        if (!icd->cacheItemList.at(j).isTarget) {
            icd->cache.targetLast = j - 1;
            break;
        }
        icd->cache.targetLast = icd->cache.totFiles - 1;
    }

//    /*
    if (debugCaching) {
        qDebug() << "ImageCache::setTargetRange"
                 << " targetFirst =" << icd->cache.targetFirst
                 << "targetLast =" << icd->cache.targetLast
                 << "isForward =" << icd->cache.isForward
                    ;
    }
    //*/
}

bool ImageCache::nextToCache(int id)
{
/*
    The next image to cache is determined by traversing the cacheItemList in ascending
    order to find the highest priority item to cache in the target range based on these
    criteria:

    - isCaching and isCached are false.

    - decoderId matches item, isCaching is true and isCached = false. If this is the case
      then we know the previous attempt failed, and we should try again, unless the
      attempts are greater than maxAttemptsToCacheImage.
*/
    if (G::isLogger) G::log("ImageCache::nextToCache");
    if (G::instanceClash(instance, "ImageCache::nextToCache")) {
        return false;
    }
    if (priorityList.size() > icd->cacheItemList.size()) {
        qWarning() << "WARNING"
                   << "ImageCache::nextToCache"
                   << "priorityList is out of date"
                   << " instance =" <<  instance
                   << "dm->instance =" << dm->instance
                      ;
        return false;
    }
    for (int p = 0; p < priorityList.size(); p++) {
        if (abort || G::stop) return false;
        int i = priorityList.at(p);
//        if (i >= icd->cacheItemList.length()) break;   ƒ // debugging
        bool isCaching = icd->cacheItemList.at(i).isCaching;
        bool isCached = icd->cacheItemList.at(i).isCached;
        int attempts = icd->cacheItemList.at(i).attempts;
        if (attempts == maxAttemptsToCacheImage)
            qWarning() << "WARNING" << "ImageCache::nextToCache"
                       << "Exceeded maxAttemptsToCacheImage" << "Row" << i
                       <<  icd->cacheItemList.at(i).fPath;
        int threadId = icd->cacheItemList.at(i).threadId;
        if (!isCached && attempts < maxAttemptsToCacheImage) {
            if (!isCaching || (isCaching && id == threadId)) {
                if (debugCaching) {
                    QString k = QString::number(i).leftJustified((4));
                    qDebug().noquote()
                         << "ImageCache::nextToCache"
                         << "    decoder" << id
                         << "row =" << k
                         << "threadId =" << threadId
                         << "priority =" << p
                         << "isCaching =" << isCaching
                         << "isCached =" << isCached
                         << "attempts =" << attempts
                            ;
                }
                icd->cache.toCacheKey = i;
                return true;
            }
        }
    }
    icd->cache.toCacheKey = -1;
    return false;


//    int lastPriority = icd->cacheItemList.length();
    int lastPriority = icd->cacheItemList.at(icd->cache.targetFirst).priority;
    int key = -1;
    // find next priority item
    for (int i = icd->cache.targetFirst; i <= icd->cache.targetLast; ++i) {
        if (i >= icd->cacheItemList.length()) break;
        int priority = icd->cacheItemList.at(i).priority;
        if (priority >= lastPriority) continue;
        bool isCaching = icd->cacheItemList.at(i).isCaching;
        bool isCached = icd->cacheItemList.at(i).isCached;
        int attempts = icd->cacheItemList.at(i).attempts;
        int threadId = icd->cacheItemList.at(i).threadId;
        // chk orphaned items rgh what about imCache - maybe still in imCache
        if (debugCaching) {
            qDebug() << "ImageCache::nextToCache"
                     << "    decoder" << id
                     << "row =" << i
                     << "threadId =" << threadId
                     << "priority =" << priority
                     << "lastPriority =" << lastPriority
                     << "isCaching =" << isCaching
                     << "isCached =" << isCached
                     << "attempts =" << attempts
                        ;
        }
        if (!isCached && attempts < maxAttemptsToCacheImage) {
            if (!isCaching || (isCaching && id == threadId)) {
                // higher priorities are lower numbers (highest = 0)
                if (priority < lastPriority) {
                    lastPriority = priority;
                    key = i;
                    break;
                }
            }
        }
    }
    if (key > -1) {
        icd->cache.toCacheKey = key;
        if (debugCaching) {
            QString k = QString::number(key).leftJustified((4));
            qDebug().noquote() << "ImageCache::nextToCache"
                               << "    decoder" << id
                               << "key =" << k
                               << icd->cacheItemList.at(key).fPath
                                  ;
        }
        return true;
    }
    // Nothing found
    QString k = QString::number(key).leftJustified((4));
    qWarning().noquote() << "ImageCache::nextToDecache" << "  decoder" << id << "key =" << k << "FAILED";
    return false;
}

//bool ImageCache::nextToDecache(int id)
//{
///*
//    The next image to decache is determined by traversing the cacheItemList in descending
//    order to find the first image currently cached.
//*/
//    if (G::isLogger) G::log("ImageCache::nextToDecache");

//    int lastPriority = 0;
//    int key = -1;
//    // find next priority item
//    for (int i = icd->cacheItemList.length() - 1; i > -1; --i) {
//        int priority = icd->cacheItemList.at(i).priority;
//        bool isCached = icd->cacheItemList.at(i).isCached;
//        if (isCached) {
//            // higher priorities are lower numbers
//            if (priority > lastPriority) {
//                lastPriority = priority;
//                key = i;
//            }
//        }
//    }
//    if (key > -1) {
////        icd->cache.toDecacheKey = key;
//        if (debugCaching) {
//            QString k = QString::number(key).leftJustified((4));
//            qDebug().noquote() << "ImageCache::nextToDecache" << "  decoder" << id << "key =" << k;
//        }
//        return true;
//    }
//    // Nothing found
//    QString k = QString::number(key).leftJustified((4));
//    qWarning().noquote() << "ImageCache::nextToDecache" << "  decoder" << id << "key =" << k << "FAILED";
//    return false;
//}

void ImageCache::setPriorities(int key)
{
/*
    Starting at the current key, this algorithm iterates through the icd->cacheItemList,
    following the order (2 ahead, one behind) and assigned an increasing sort order key, which
    is used by setTargetRange to sort icd->cacheItemList by priority.
*/
    if (G::isLogger) G::log("ImageCache::setPriorities", "key = " + QString::number(key));
    if (debugCaching) {
        qDebug().noquote() << "ImageCache::setPriorities" << "  decoder-1" << "key =" << key;
    }
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
}

void ImageCache::fixOrphans()
{
/*
    If the caching process fails, then an image in the target range may be orphaned while
    caching is still active. isCaching is reset to false. If an image outside the target
    range is shown as isCaching it is reset, and if the image is cached then it is
    removed from the imCache and the cached flag is reset to false.
*/
    orphansFound = false;
    for (int i = 0; i < icd->cacheItemList.length(); ++i) {
        QString fPath = icd->cacheItemList.at(i).fPath;
        bool isCached = icd->cacheItemList.at(i).isCached;
        bool isCaching = icd->cacheItemList.at(i).isCaching;
        bool inImageCache = icd->imCache.contains(fPath);
        if (icd->cacheItemList.at(i).isTarget) {
            if (isCaching) {
                // chk if decoding is active
                int id = icd->cacheItemList[i].threadId;
                if (decoder[id]->isRunning() && decoder[id]->cacheKey == i) {
                    // decoding is active, no action req'd
                }
                else {
                    // reset isCaching to false
                    icd->cacheItemList[i].isCaching = false;
                    orphansFound = true;
                    if (debugCaching) {
                        QString k = QString::number(i).leftJustified((4));
                        qDebug().noquote()
                            << "ImageCache::fixOrphans"
                            << "     row " << k
                            << "orphaned"
                               ;
                    }
                }
            }
        }
        else {
            if (isCached) icd->cacheItemList[i].isCached = false;
            if (isCaching) icd->cacheItemList[i].isCaching = false;
            if (inImageCache) icd->imCache.remove(fPath);
            emit updateCacheOnThumbs(fPath, false, "ImageCache::fixOrphans");
        }
    }

    // no orphans found
    if (!orphansFound) {
        if (debugCaching) {
            qDebug().noquote() << "ImageCache::fixOrphans" << "     No orphans found";
        }
    }
}

bool ImageCache::isCached(int sfRow)
{
    return icd->cacheItemList.at(sfRow).isCached;
}

bool ImageCache::cacheUpToDate()
{
/*
    Determine if all images in the target range are cached or being cached.
*/
    isCacheUpToDate = false;
    for (int i = icd->cache.targetFirst; i < icd->cache.targetLast + 1; ++i) {
        if (icd->cacheItemList[i].isVideo) continue;
        if (i >= icd->cacheItemList.count()) break;
        // check if image was passed over while rapidly traversing the folder
        if (icd->cacheItemList.at(i).isCached && icd->cacheItemList.at(i).threadId == -1) {
            icd->cacheItemList[i].isCached = false;
            icd->cacheItemList[i].isCaching = false;
            return false;
        }
        // check if caching image is in progress
        if (!icd->cacheItemList.at(i).isCached && !icd->cacheItemList.at(i).isCaching)
            return false;
    }
    isCacheUpToDate = true;
    if (debugCaching) {
        qDebug() << "ImageCache::cacheUpToDate"
                 << "true"
                 << "targetFirst =" << icd->cache.targetFirst
                 << "targetLast =" << icd->cache.targetLast
                    ;
    }
    return true;
}

void ImageCache::setSizeMB(int id, int cacheKey)
{
/*
    For images that Winnow does not have the metadata describing the size (width and
    height), ie PNG files, an initial sizeMB is estimated to be used in the target range.
    After a decoder has converted the file into a QImage the sizeMB, dimensions (w,h) and
    target range are updated here.

    If this is not done the ImageCache::makeRoom algorithm will not be updated correctly.
*/
    if (G::isLogger) G::log("ImageCache::setSizeMB");
    QImage *image = &decoder[id]->image;
    int w = image->width();
    int h = image->height();
    // 8 bits X 3 channels + 8 bit depth = (32*w*h)/8/1024/1024 = w*h/262144
    icd->cacheItemList[cacheKey].sizeMB = static_cast<float>(w * h * 1.0 / 262144);
    icd->cacheItemList[cacheKey].estSizeMB = false;
    // recalc currMB
    icd->cache.currMB = getImCacheSize();
}

void ImageCache::memChk()
{
/*
    Check to make sure there is still room in system memory (heap) for the image cache. If
    something else (another program) has used the system memory then reduce the size of the
    cache so it still fits.
*/
    if (G::isLogger) G::log("ImageCache::makeRoom");

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

void ImageCache::removeFromCache(QStringList &pathList)
{
/*
    Called when delete image(s).
*/
    if (G::isLogger) G::log("ImageCache::removeFromCache");

    // remove items from icd->cacheItemList, i
    for (int i = 0; i < pathList.count(); ++i) {
        QString fPath = pathList.at(i);
        icd->imCache.remove(fPath);
        for (int j = 0; j < icd->cacheItemList.length(); ++j) {
            if (icd->cacheItemList.at(j).fPath == fPath) {
                icd->cacheItemList.removeAt(j);
            }
        }
    }
    icd->cache.totFiles = icd->cacheItemList.length();

    /* redo keys (rows) in icd->cacheItemList to make contiguous ie 1,2,3; not 1,2,4;
       redo cacheKeyHash and update cache current size */
    cacheKeyHash.clear();
    icd->cache.currMB = 0;
    for (int i = 0; i < icd->cacheItemList.length(); ++i) {
        QString fPath = icd->cacheItemList.at(i).fPath;
        icd->cacheItemList[i].key = i;
        icd->cacheItemList[i].origKey = i;
        if (icd->cacheItemList[i].isCached) {
            icd->cache.currMB += icd->cacheItemList[i].sizeMB;
        }
        cacheKeyHash[fPath] = i;
    }
}

void ImageCache::updateStatus(QString instruction, QString source)
{
    emit showCacheStatus(instruction, icd->cache, source);
}

QString ImageCache::diagnostics()
{
    if (G::isLogger) G::log("ImageCache::diagnostics");
    QString reportString;
    QTextStream rpt;
    rpt.setString(&reportString);
    rpt << Utilities::centeredRptHdr('=', objectName() + " ImageCache Diagnostics");
    rpt << "\n\n";
    rpt << "Load algorithm: " << (G::isLinearLoading == true ? "Linear" : "Concurrent");
    rpt << "\n";
    rpt << reportCacheParameters();
    rpt << reportCache("");
    rpt << reportImCache();

    rpt << "\n\n" ;
    return reportString;
}

QString ImageCache::reportCacheParameters()
{
    if (G::isLogger) G::log("ImageCache::reportCacheParameters");
    QString reportString;
    QTextStream rpt;
    rpt.flush();
    reportString = "";
    rpt.setString(&reportString);

    rpt << "\n";
    rpt << "isRunning                = " << (isRunning() ? "true" : "false") << "\n";

    rpt << "\n";
    rpt << "directionChangeThreshold = " << icd->cache.directionChangeThreshold << "\n";
    rpt << "wtAhead                  = " << icd->cache.wtAhead << "\n";
    rpt << "isForward                = " << (icd->cache.isForward ? "true" : "false") << "\n";
    rpt << "totFiles                 = " << icd->cache.totFiles << "\n";
    rpt << "currMB                   = " << icd->cache.currMB << "\n";
    rpt << "currMB Check             = " << getImCacheSize() << "\n";
    rpt << "maxMB                    = " << icd->cache.maxMB << "\n";
    rpt << "minMB                    = " << icd->cache.minMB << "\n";
    rpt << "folderMB                 = " << icd->cache.folderMB << "\n";
    rpt << "targetFirst              = " << icd->cache.targetFirst << "\n";
    rpt << "targetLast               = " << icd->cache.targetLast << "\n";
    rpt << "decoderCount             = " << icd->cache.decoderCount << "\n";
    rpt << "\n";
    rpt << "currentPath              = " << currentPath << "\n";
    rpt << "cacheSizeHasChanged      = " << (cacheSizeHasChanged ? "true" : "false") << "\n";
    rpt << "filterOrSortHasChanged   = " << (filterOrSortHasChanged ? "true" : "false") << "\n";
    rpt << "isCacheUpToDate          = " << (isCacheUpToDate ? "true" : "false") << "\n";
    return reportString;
}

QString ImageCache::reportCache(QString title)
{
    if (G::isLogger) G::log("ImageCache::reportCache");
    QString reportString;
    QTextStream rpt;
    rpt.flush();
    reportString = "";
    rpt.setString(&reportString);

    rpt << "\ncacheItemList:\n";
//    rpt  << "\n Title:" << title
//         << "  Key:" << icd->cache.key
//         << "  cacheMB:" << icd->cache.currMB
//         << "  Wt ahead:" << icd->cache.wtAhead
//         << "  Direction ahead:" << icd->cache.isForward
//         << "  Total files:" << icd->cache.totFiles << "\n\n";
    int cachedCount = 0;
    for (int i = 0; i < icd->cacheItemList.length(); ++i) {
        int row = dm->fPathRow[icd->cacheItemList.at(i).fPath];
        // show header every 40 rows
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
                << "imCache"
                << "dmCached"
                << "Video"
                << "SizeMB"
                << "Estimate"
                << "Offset"
                << "Length"
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
            << (icd->cacheItemList.at(i).isTarget ? "true" : "false")
            << icd->cacheItemList.at(i).attempts
            << (icd->cacheItemList.at(i).isCaching ? "true" : "false")
            << icd->cacheItemList.at(i).threadId
            << (icd->cacheItemList.at(i).isCached ? "true" : "false")
            << (icd->imCache.contains(icd->cacheItemList.at(i).fPath) ? "true" : "false")
            << (dm->sf->index(icd->cacheItemList.at(i).key,0).data(G::UserRoles::CachedRole).toBool() ? "true" : "false")
            << (icd->cacheItemList.at(i).isVideo ? "true" : "false")
            << icd->cacheItemList.at(i).sizeMB
            << icd->cacheItemList.at(i).estSizeMB
//            << icd->cacheItemList.at(i).metadataLoaded
            << icd->cacheItemList.at(i).offsetFull
            << icd->cacheItemList.at(i).lengthFull
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

    rpt << "\npriorityList:" << "\n";
    rpt.setFieldWidth(9);
    rpt.setFieldAlignment(QTextStream::AlignRight);
    rpt << "Priority" << "Key";
    rpt.setFieldAlignment(QTextStream::AlignLeft);
    rpt << "   Path";
    rpt.setFieldWidth(0);
    rpt << "\n";
    for (int i = 0; i < priorityList.size(); ++i) {
        int key = priorityList.at(i);
        rpt.setFieldWidth(9);
        rpt.setFieldAlignment(QTextStream::AlignRight);
        rpt << i << key;
        rpt.setFieldWidth(3);
        rpt << "   ";
        rpt.setFieldAlignment(QTextStream::AlignLeft);
        rpt.setFieldWidth(50);
        rpt << icd->cacheItemList.at(key).fPath;
        rpt.setFieldWidth(0);
        rpt << "\n";
    }

    return reportString;
}

QString ImageCache::reportImCache()
{
    QVector<QString> keys;
    // check when imCache is empty
    QImage image;
    int mem = 0;
    icd->imCache.getKeys(keys);

    // build list of report items
    struct ImRptItem {
        int hashKey;
        int cacheItemListKey;
        int priorityKey;
        int w;
        int h;
        int mb;
        QString fPath;
    } imRptItem;
    QList<ImRptItem> rptList;
    for (int i = 0; i < keys.length(); ++i) {
        imRptItem.hashKey = i;
        imRptItem.fPath = keys.at(i);
        imRptItem.cacheItemListKey = keyFromPath(imRptItem.fPath);
        for (int j = 0; j < priorityList.length(); j++) {
            if (priorityList.at(j) == imRptItem.cacheItemListKey) {
                imRptItem.priorityKey = j;
                break;
            }
        }
        icd->imCache.find(keys.at(i), image);
        imRptItem.w = image.width();
        imRptItem.h = image.height();
        imRptItem.mb = static_cast<int>(imRptItem.w * imRptItem.h * 1.0 / 262144);
//        imRptItem.mb = image.sizeInBytes() / 1024 / 1024;
        rptList.append(imRptItem);
    }

    // report header
    QString reportString;
    QTextStream rpt;
    rpt.flush();
    reportString = "";
    rpt.setString(&reportString);
    rpt << "\nimCache hash: ";
    rpt << keys.length() << " items";

    rpt << "\n";
    rpt.reset();
    rpt.setFieldAlignment(QTextStream::AlignRight);
    rpt.setFieldWidth(6);
    rpt << "Hash";
    rpt.setFieldWidth(10);
    rpt << "Priority";
    rpt.setFieldWidth(6);
    rpt << "Key" << "W" << "H";
    rpt << "MB";
    rpt.reset();
    rpt.setFieldAlignment(QTextStream::AlignLeft);
    rpt << "  " << "Path";
    rpt << "\n";

    // report each item, sorted by priority
    int lastPriority = -1;
    int nextPriority;
    int item;
    for (int i = 0; i < rptList.length(); ++i) {
        nextPriority = 999999;
        // find next lowest priority item
        for (int j = 0; j < rptList.length(); ++j) {
            int thisPriority = rptList.at(j).priorityKey;
            if (thisPriority > lastPriority && thisPriority < nextPriority) {
                nextPriority = thisPriority;
                item = j;
            }
        }
        lastPriority = nextPriority;

        // report this item
        rpt.reset();
        rpt.setFieldAlignment(QTextStream::AlignRight);
        rpt.setFieldWidth(6);
        rpt << rptList.at(item).hashKey;
        rpt.setFieldWidth(10);
        rpt << rptList.at(item).priorityKey;
        rpt.setFieldWidth(6);
        rpt << rptList.at(item).cacheItemListKey;
        rpt << rptList.at(item).w;
        rpt << rptList.at(item).h;
        rpt << rptList.at(item).mb;
        rpt.reset();
        rpt.setFieldAlignment(QTextStream::AlignLeft);
        rpt << "  ";
        rpt << rptList.at(item).fPath;
        rpt << "\n";
        mem += (rptList.at(item).mb /*/ 1024 / 1024*/);
    }

    rpt << "\n" << keys.length() << " images in image cache.";
    rpt << "\n" << mem << " MB consumed";

    return reportString;
}

QString ImageCache::reportCacheProgress(QString action)
{
//    if (G::isLogger) {mutex.lock(); G::log("ImageCache::reportCacheProgress"); mutex.unlock();}
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
    return reportString;
}

void ImageCache::reportRunStatus()
{
    bool isRun = isRunning();
    qDebug() << "ImageCache::reportRunStatus"
             << "isRunning =" << isRun
             << "isForward =" << icd->cache.isForward
             << "abort =" << abort
             << "pause =" << pause
             << "filterSortChange =" << filterOrSortHasChanged
             << "cacheSizeChange =" << cacheSizeHasChanged
             << "currentPath =" << currentPath;
}

void ImageCache::addCacheItemImageMetadata(ImageMetadata m)
{
/*
    Concurrent metadata loading alternative to buildImageCacheList. The imageCacheList is
    built row by row, triggered by signal addToImageCache in MetaRead::readMetadata.
*/
    if (G::isLogger /*|| G::isFlowLogger*/) G::log("ImageCache::addCacheItemImageMetadata");
    if (!G::useImageCache) return;  // rgh isolate image cache
    if (G::stop) {
        return;
    }
    if (!cacheKeyHash.contains(m.fPath)) {
        qWarning() << "WARNING" << "ImageCache::addCacheItemImageMetadata"
                   << "cacheKeyHash does not contain" << m.fPath;
        return;
    }

    int row = cacheKeyHash[m.fPath];
    if (row >= icd->cacheItemList.size()) {
        qWarning() << "WARNING" << "ImageCache::addCacheItemImageMetadata"
                   << "row =" << row
                   << "exceeds icd->cacheItemList.size() of" << icd->cacheItemList.size();
        return;
    }

    icd->cacheItemList[row].metadataLoaded = m.metadataLoaded;
    icd->cacheItemList[row].isVideo = m.video;

    if (m.video) {
        icd->cacheItemList[row].sizeMB = 0;
        icd->cacheItemList[row].estSizeMB = false;
        icd->cacheItemList[row].offsetFull = 0;
        icd->cacheItemList[row].lengthFull = 0;
        return;
    }

    /* cacheItemList is a list of cacheItem used to track the current cache status and
    make future caching decisions for each image.
    */

    int w, h;
    m.widthPreview > 0 ? w = m.widthPreview : w = m.width;
    m.heightPreview > 0 ? h = m.heightPreview : h = m.height;
    mutex.lock();
    float sizeMB = static_cast<float>(w * h * 1.0 / 262144);
    if (sizeMB > 0) {
        // 8 bits X 3 channels + 8 bit depth = (32*w*h)/8/1024/1024 = w*h/262144
        icd->cacheItemList[row].sizeMB = sizeMB;
        icd->cacheItemList[row].estSizeMB = false;
    }
    else {
        icd->cacheItemList[row].sizeMB = m.size * 1.0 / 1000000;
        icd->cacheItemList[row].estSizeMB = true;
    }
    // decoder parameters
    icd->cacheItemList[row].orientation = m.orientation;
    icd->cacheItemList[row].rotationDegrees = m.rotationDegrees;
    icd->cacheItemList[row].offsetFull = m.offsetFull;
    icd->cacheItemList[row].lengthFull = m.lengthFull;
    icd->cacheItemList[row].samplesPerPixel = m.samplesPerPixel;
    icd->cacheItemList[row].iccBuf = m.iccBuf;
//    icd->cache.folderMB += icd->cacheItemList[row].sizeMB; // req'd?  Only used for diagnostics
    mutex.unlock();
}

void ImageCache::buildImageCacheList()
{
/*
    The imageCacheList must match dm->sf and contains the information required to maintain the
    image cache. It takes the form (example):

    Key Priority   Target Attempts  Caching   Thread   Cached   SizeMB   File Name
      0        0        1        2        0        4        1      119   D:/Pictures/Calendar_Beach/2014-09-26_0288.jpg
      1        1        1        1        0        1        1       34   D:/Pictures/Calendar_Beach/2014-07-29_0002.jpg
      2        2        1        2        0        7        1      119   D:/Pictures/Calendar_Beach/2014-04-11_0060.jpg
      3        3        1        2        0        5        1      119   D:/Pictures/Calendar_Beach/2012-08-16_0015.jpg

    It is built from dm->sf (sorted / filtered datamodel).
*/
    if (G::isLogger || G::isFlowLogger) G::log("ImageCache::buildImageCacheList");
    icd->cacheItemList.clear();
    cacheKeyHash.clear();
    // the total memory size of all the images in the folder currently selected
    int folderMB = 0;
    icd->cache.totFiles = dm->sf->rowCount();

    for (int i = 0; i < dm->sf->rowCount(); ++i) {
        QString fPath = dm->sf->index(i, G::PathColumn).data(G::PathRole).toString();
        cacheKeyHash[fPath] = i;
        if (fPath == "") continue;
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
        if ((G::isLinearLoading || G::allMetadataLoaded)) {
            ImageMetadata m = dm->imMetadata(fPath);
            icd->cacheItem.metadataLoaded = m.metadataLoaded;
            icd->cacheItem.isVideo = m.video;
            if (!m.video) {
                // 8 bits X 3 channels + 8 bit depth = (32*w*h)/8/1024/1024 = w*h/262144
                int w, h;
                m.widthPreview > 0 ? w = m.widthPreview : w = m.width;
                m.heightPreview > 0 ? h = m.heightPreview : h = m.height;
                int sizeMB = static_cast<int>(w * h * 1.0 / 262144);
                if (sizeMB) {
                    // 8 bits X 3 channels + 8 bit depth = (32*w*h)/8/1024/1024 = w*h/262144
                    icd->cacheItem.sizeMB = sizeMB;
                    icd->cacheItem.estSizeMB = false;
                }
                else {
                    icd->cacheItem.sizeMB = m.size * 1.0 / 1000000;
                    icd->cacheItem.estSizeMB = true;
                }
                // decoder parameters
                icd->cacheItem.orientation = m.orientation;
                icd->cacheItem.rotationDegrees = m.rotationDegrees;
                icd->cacheItem.offsetFull = m.offsetFull;
                icd->cacheItem.lengthFull = m.lengthFull;
                icd->cacheItem.samplesPerPixel = m.samplesPerPixel;
                icd->cacheItem.iccBuf = m.iccBuf;
                folderMB += icd->cacheItem.sizeMB;
            }
            /*
            if (G::isLogger || G::isFlowLogger) {
                if (i % 1000 == 0) {
                    QString msg = "Building Image Cache: ";
                    msg += QString::number(i) + " of " + QString::number(dm->sf->rowCount());
                    emit centralMsg(msg);
                }
            }
            */
        }
        icd->cacheItemList.append(icd->cacheItem);
    } // next row

    if (G::isLinearLoading) icd->cache.folderMB = folderMB;
}

void ImageCache::initImageCache(int &cacheMaxMB,
                                int &cacheMinMB,
                                bool &isShowCacheStatus,
                                int &cacheWtAhead)
{
    if (G::isLogger || G::isFlowLogger) G::log("ImageCache::initImageCache");
    if (!G::useImageCache) return;   // rgh isolate image cache

    if (debugCaching) qDebug() << "ImageCache::initImageCache  dm->instance =" << dm->instance;

    abort = false;

    // cancel if no images to cache
    if (!dm->sf->rowCount()) return;

    // just in case stopImageCache not called before this
    if (isRunning()) stop();

    // update folder change instance
    instance = dm->instance;
    G::imageCacheInstance = instance;

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
    icd->cache.directionChangeThreshold = 3;

    if (icd->cache.isShowCacheStatus) {
        updateStatus("Clear", "ImageCache::initImageCache");
    }

    // populate the new folder image list
    buildImageCacheList();

    source = "ImageCache::initImageCache";
}

void ImageCache::updateImageCacheParam(int &cacheSizeMB,
                                       int &cacheMinMB,
                                       bool &isShowCacheStatus,
                                       int &cacheWtAhead)
{
/*
    When various image cache parameters are changed in preferences they are updated here.
*/
    if (G::isLogger) G::log("ImageCache::updateImageCacheParam");
    mutex.lock();
    icd->cache.maxMB = cacheSizeMB;
    icd->cache.minMB = cacheMinMB;
    icd->cache.isShowCacheStatus = isShowCacheStatus;
    icd->cache.wtAhead = cacheWtAhead;
    mutex.unlock();
}

void ImageCache::rebuildImageCacheParameters(QString &currentImageFullPath, QString source)
{
/*
    When the datamodel is filtered the image cache needs to be updated. The cacheItemList is
    rebuilt for the filtered dataset and isCached updated, the current image is set, and any
    surplus cached images (not in the filtered dataset) are removed from imCache.
    The image cache is now ready to run by calling setCachePosition().
*/
    if (G::isLogger || G::isFlowLogger) G::log("ImageCache::rebuildImageCacheParameters");
    if(dm->sf->rowCount() == 0) return;

    /*
    qDebug() << "ImageCache::rebuildImageCacheParameters" << "Source:" << source;
    std::cout << diagnostics().toStdString() << std::flush;
    //*/

    // update currentPath
    currentPath = currentImageFullPath;

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
        if (icd->imCache.contains(fPath)) icd->cacheItemList[row].isCached = true;
        else icd->imCache.remove(fPath);
    }

    // if the sort has been reversed
    if (source == "SortChange") icd->cache.isForward = !icd->cache.isForward;

    setPriorities(icd->cache.key);
    setTargetRange();

    QVector<QString> keys;
    icd->imCache.getKeys(keys);
    for (int i = keys.length() - 1; i > -1; --i) {
        if (!filteredList.contains(keys.at(i))) icd->imCache.remove(keys.at(i));
    }

    if (icd->cache.isShowCacheStatus)
        updateStatus("Update all rows", "ImageCache::rebuildImageCacheParameters");

    setCurrentPosition(currentPath, "ImageCache::rebuildImageCacheParameters");
}

void ImageCache::refreshImageCache()
{
/*
    Reload all images in the cache.
*/
    if (G::isLogger) G::log("ImageCache::refreshImageCache");
    mutex.lock();
    // make all isCached = false
    for (int i = 0; i < icd->cacheItemList.length(); ++i) {
        if (icd->cacheItemList[i].isCached == true) {
            icd->cacheItemList[i].isCached = false;
        }
    }
    refreshCache = true;
    mutex.unlock();
    if (!isRunning()) {
        start();
    }
}

void ImageCache::colorManageChange()
{
/*
    Called when color manage is toggled.  Reload all images in the cache.
*/
    if (G::isLogger) G::log("ImageCache::colorManageChangeImageCache::colorManageChange");
    refreshImageCache();
}

void ImageCache::cacheSizeChange()
{
    /*
    Called when changes are made to image cache parameters in preferences. The image cache
    direction, priorities and target are reset.  makeRoom is executed.
    */
    if (G::isLogger) G::log("ImageCache::cacheSizeChange");
    mutex.lock();
    cacheSizeHasChanged = true;
    mutex.unlock();
    if (!isRunning()) {
        start();
    }
}

void ImageCache::setCurrentPosition(QString path, QString src)
{
    /*
    Called from MW::fileSelectionChange to reset the position in the image cache. The image
    cache direction, priorities and target are reset and the cache is updated in fillCache.
    */
    if (G::isLogger || G::isFlowLogger) G::log("skipline");
    if (G::isLogger || G::isFlowLogger) G::log("ImageCache::setCurrentPosition", path);
    if (G::instanceClash(instance, "ImageCache::setCurrentPosition")) {
        qWarning() << "WARNING"
                   << "ImageCache::setCurrentPosition"
                   << "Instance clash"
                   << dm->rowFromPath(path)
                   << src
                   << path;
        return;
    }
    if (keyFromPath(path) == -1) {
        qWarning() << "WARNING"
                   << "ImageCache::setCurrentPosition"
                   << "src =" << src
                   << "Not in cacheItemList"
                   << path;
        return;
    }
    if (debugCaching) {
        qDebug();
        qDebug().noquote() << "ImageCache::setCurrentPosition" << path << "src =" << src;
    }

//    qDebug() << "ImageCache::setCurrentPosition" << path << "src =" << src;

    mutex.lock();
    currentPath = path;
    mutex.unlock();

    if (!isRunning()) {
        start();
    }
}

void ImageCache::decodeNextImage(int id)
{
/*
    Called from fillCache to run a decoder for the next image to decode into a QImage.
    The decoders run in separate threads. When they complete the decoding they signal
    back to fillCache.
*/
    int row = icd->cache.toCacheKey;
    icd->cacheItemList[row].isCaching = true;
    icd->cacheItemList[row].threadId = id;
    icd->cacheItemList[row].attempts += 1;
    if (debugCaching) {
        QString k = QString::number(row).leftJustified((4));
        qDebug().noquote()
            << "ImageCache::decodeNextImage"
            << "decoder" << id
            << "row =" << k
            << "threadId =" << icd->cacheItemList.at(row).threadId
            << "isCaching =" << icd->cacheItemList.at(row).isCaching
            << "isCached =" << icd->cacheItemList.at(row).isCached
            << "attempts =" << icd->cacheItemList.at(row).attempts
            << icd->cacheItemList.at(row).fPath
              ;
    }
    decoder[id]->decode(icd->cacheItemList.at(row), instance);
}

void ImageCache::cacheImage(int id, int cacheKey)
{
/*
    Called from fillCache to insert a QImage that has been decoded into icd->imCache.
    Do not cache video files, but do show them as cached for the cache status display.
*/
    if (debugCaching) {
        QString k = QString::number(cacheKey).leftJustified((4));
        qDebug().noquote() << "ImageCache::cacheImage" << "     decoder" << id << k;
    }

    if (decoder[id]->status != ImageDecoder::Status::Video) {
        // Check if initial sizeMB was estimated (image without preview metadata ie PNG)
        if (icd->cacheItemList[cacheKey].estSizeMB) setSizeMB(id, cacheKey);
        icd->imCache.insert(decoder[id]->fPath, decoder[id]->image);
    }

    icd->cache.currMB = getImCacheSize();
    icd->cacheItemList[cacheKey].isCaching = false;
    icd->cacheItemList[cacheKey].isCached = true;
    // set datamodel isCached = true
    emit setValuePath(decoder[id]->fPath, 0, true, instance, G::CachedRole);
    if (decoder[id]->status != ImageDecoder::Status::Video) {
        // if current image signal ImageView::loadImage
        if (decoder[id]->fPath == dm->currentFilePath) {
            // load in ImageView
            emit loadImage(decoder[id]->fPath, "ImageCache::cacheImage");
            // revert central view
            emit imageCachePrevCentralView();
        }
    }
    updateStatus("Update all rows", "ImageCache::cacheImage");
}

void ImageCache::fillCache(int id)
{
/*
    A number of ImageDecoders are created when ImageCache is created. Each ImageDecoder
    runs in a separate thread. The decoders convert an image file into a QImage and then
    signal this function with their id so the QImage can be inserted into the image
    cache. ImageDecoders are launched from CacheImage::run. CacheImage makes sure the
    necessary metadata is available and reads the file and then runs the decoder. This is
    very important, as file reads have to be sequential while the decoding can be
    performed synchronously, which significantly improves performance.

    The ImageDecoder has a status attribute that can be Ready, Busy or Done. When the
    decoder is created and when the QImage has been inserted into the image cache the
    status is set to Ready. When the decoder is called from CacheImage the status is set
    to Busy. Finally, when the decoder finishes the decoding in ImageDecoder::run the
    status is set to Done. Each decoder signals ImageCache::fillCache when the image has
    been converted into a QImage. Here the QImage is added to the imCache. If there are
    more targeted images to be cached, the next one is assigned to the decoder, which is
    run again. The decoders keep running until all the targeted images have been cached.

    Every time the ImageCache::run function encounters a change trigger (file selection
    change, cache size change, color manage change or sort/filter change) the image cache
    parameters are updated and this function is called for each Ready decoder. The
    decoders not Ready are Busy and already in the fillCache loop.

    An image decoder can be running when a new folder is selected, returning an image
    from the previous folder. When an image decoder is run it is seeded with the
    datamodel instance (which is incremented every time a new folder is selected). When
    the decoder signals back to DM::fillCache the decoder instance is checked against
    the current dm->instance to confirm the decoder image is from the current datamodel
    instance.

    Each decoder follows this basic pattern:
    - nextToCache
    - decodeNextImage
    - fillCache
    - cacheImage
      - makeRoom
      - nextToDecache
*/

    if (abort) return;

    int cacheKey;       // row for image in cacheKeyHash
    cacheKey = -1;      // default = no image

    // check decoder return
    if (decoder[id]->status == ImageDecoder::Status::Done) {
         cacheKey = cacheKeyHash[decoder[id]->fPath];  // if not contains return -1
    }

    // Error checking
    {
//        // DM instance check
//        if (decoder[id]->instance != dm->instance) {
//            cacheKey = -1;
//            if (debugCaching)
//                if (decoder[id]->fPath != "")
//                qWarning() << "WARNING" << "ImageCache::fillCache DataModel instance clash:"
//                           << "Decoder DM instance" << decoder[id]->instance
//                           << "DM instance" << dm->instance
//                           << "Decoder id =" << id
//                           << decoder[id]->fPath
//                              ;
//        }

//        // range check
//        if (cacheKey != -1 && cacheKey >= icd->cacheItemList.length()) {
//            if (icd->cacheItemList.length() > 0) {
//                qWarning() << "WARNING" << "ImageCache::fillCache  Out of range error:"
//                           << "Decoder" << id << decoder[id]->fPath
//                           << "cacheKey =" << cacheKey
//                           << "EXCEEDS icd->cacheItemList.length() of"
//                           << icd->cacheItemList.length()
//                              ;
//                QString err = "cacheKey = " + QString::number(cacheKey) +
//                              " exceeds icd->cacheItemList.length() of " +
//                              QString::number(icd->cacheItemList.length());
//                G::error("ImageCache::fillCache", decoder[id]->fPath, err);
//            }
//            cacheKey = -1;
//        }
//        /*
//        // File does not exist
//        if (decoder[id]->status == ImageDecoder::Status::NoFile) {
//            cacheKey = -1;
//            if (debugCaching)
//                if (decoder[id]->fPath != "")
//                qWarning() << "WARNING" << "ImageCache::fillCache File does not exist:"
//                           << "Decoder id =" << id
//                           << decoder[id]->fPath
//                              ;
//        }

//        // unassigned decoder check
//        if (cacheKey != -1 && decoder[id]->fPath == "") {
//            cacheKey = -1;
//            if (debugCaching)
//                qWarning() << "WARNING" << "ImageCache::fillCache Unassigned decoder:"
//                           << "Decoder id =" << id
//                           << "Decoder path =" << decoder[id]->fPath
//                              ;
//        }

//        // null image check
//        if (decoder[id]->status != ImageDecoder::Status::Video) {
//            if (cacheKey != -1 && decoder[id]->image.width() == 0) {
//                cacheKey = -1;
//                {
//                    qWarning() << "WARNING" << "ImageCache::fillCache  Null image:"
//                               << "Decoder" << id
//                               << "Image width = 0"
//                               << "Decoder path =" << decoder[id]->fPath
//                                  ;
//                }
//            }
//        }

//        // folder does not exist
//        if (decoder[id]->status == ImageDecoder::Status::NoDir) {
//            cacheKey = -1;
//            qWarning() << "WARNING" << "ImageCache::fillCache  Folder does not exist:"
//                       << "Decoder" << id
//                       << "Decoder path =" << decoder[id]->fPath
//                          ;
//            emit centralMsg(decoder[id]->errMsg);
//        }

//        // other
//        if (decoder[id]->status != ImageDecoder::Status::Done && decoder[id]->fPath != "") {
//            cacheKey = -1;
//            qWarning() << "WARNING" << "ImageCache::fillCache  Decoder status not = Done:"
//                       << "Decoder" << id
//                       << "Decoder status enum =" << decoder[id]->status
//                       << "Decoder path =" << decoder[id]->fPath
//                          ;
//        }
//        //*/

    }

    if (debugCaching) {
        QString k = QString::number(cacheKey).leftJustified((4));
        qDebug().noquote() << "ImageCache::fillCache"
                 << "      decoder" << id
                 << "row =" << k    // row = key
                 << "isRunning =" << decoder[id]->isRunning()
                 << decoder[id]->fPath
                    ;
    }

    // add decoded QImage to imCache.
    if (cacheKey != -1) {
        cacheImage(id, cacheKey);
    }

    // get next image to cache (nextToCache() defines cache.toCacheKey)
    if (nextToCache(id) && isValidKey(icd->cache.toCacheKey)) {
        decodeNextImage(id);
    }
    else { // caching completed
        decoder[id]->setReady();
        if (orphansFound) fixOrphans();
        emit updateIsRunning(false, true);  // (isRunning, showCacheLabel)
        if (debugCaching) {
            qDebug() << "ImageCache::fillCache"
                     << "      decoder" << id
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
    Called by a new file selection, cache size change, sort, filter or color manage change.
    The cache status is updated (current key, direction of travel, priorities and the target
    range) by calling fillCache with a decoder id = -1. Then each ready decoder is sent to
    fillCache. Decoders are assigned image files, which they decode into QImages, and then
    added to imCache. More details are available in the fillCache comments and at the top of
    this class.
*/
    if (G::isLogger || G::isFlowLogger) G::log("ImageCache::run");
    if (icd->cacheItemList.length() == 0) return;
    if (debugCaching) {
        qDebug().noquote() << "ImageCache::run";
    }

    // check available memory (another app may have used or released some memory)
    memChk();

    // reset target range
    setKeyToCurrent();
    setDirection();
    icd->cache.currMB = getImCacheSize();
    setPriorities(icd->cache.key);
    setTargetRange();
    fixOrphans();
//    if (cacheSizeHasChanged) makeRoom(0, 0);

    // if cache is up-to-date our work is done
    if (cacheUpToDate()) return;

    // signal MW cache status
    emit updateIsRunning(true, true);

    orphansFound = true;

    /* fill the cache with images.  Note use ImageDecoder::Status::Ready because
       decoder[id]->isRunning() resulted in empty images in imCache  */
    for (int id = 0; id < decoderCount; ++id) {
        if (decoder[id]->status == ImageDecoder::Status::Ready) {
            decoder[id]->fPath = "";
            fillCache(id);
        }
        if (cacheUpToDate()) break;
    }
}
