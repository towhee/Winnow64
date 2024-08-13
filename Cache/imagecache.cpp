#include "Cache/imagecache.h"


/*  How the Image Cache works:

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

    In addition to the cacheItemList, there are two hashes: keyFromPath and pathFromKey.
    These hashes improve efficiency and resolve some concurrency issues.

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
    //if (debugCaching) qDebug() << "ImageCache::ImageCache";
    log("ImageCache");

    // data is kept in ImageCacheData icd, a concurrent hash table
    this->icd = icd;
    this->dm = dm;
    // new metadata to avoid thread collisions
    metadata = new Metadata;

    // create n decoder threads
    decoderCount = QThread::idealThreadCount();
    //decoderCount = 10;
    icd->cache.decoderCount = decoderCount;
    for (int id = 0; id < decoderCount; ++id) {
        ImageDecoder *d = new ImageDecoder(this, id, dm, metadata);
        d->status = ImageDecoder::Status::Ready;
        decoder.append(d);
        connect(decoder[id], &ImageDecoder::done, this, &ImageCache::fillCache);
    }
    restart = false;
    abort = false;
    debugCaching = false;       // turn on local qDebug
    debugLog = false;           // invoke log
}

ImageCache::~ImageCache()
{
    if (useMutex) gMutex.lock();
    //if (debugCaching) qDebug() << "ImageCache::~ImageCache";
    log("~ImageCache");
    abort = true;
    condition.wakeOne();
    if (useMutex) gMutex.unlock();
    wait();
}

void ImageCache::clearImageCache(bool includeList)
{
    //if (debugCaching) qDebug() << "ImageCache::clearImageCache";
    log("ClearImageCache");
    if (useMutex) QMutexLocker locker(&gMutex);
    icd->imCache.clear();
    icd->cache.currMB = 0;
    // do not clear cacheItemList if called from start slideshow
    if (includeList) icd->cacheItemList.clear();
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
    //if (debugCaching)
    //qDebug() << "ImageCache::stop  isRunning =" << isRunning();
    log("stop", "isRunning = " + QVariant(isRunning()).toString());

    // stop ImageCache first to prevent more decoder dispatch
    if (useMutex) gMutex.lock();
    abort = true;
    if (useMutex) gMutex.unlock();

    // stop decoder threads
    for (int id = 0; id < decoderCount; ++id) {
        decoder[id]->stop();
    }

    /* stop imagecache thread if running, which is unlikely as run() initiates the
       fillCache cycle and ends */
    if (isRunning()) {
        if (useMutex) gMutex.lock();
        //abort = true;
        condition.wakeOne();
        if (useMutex) gMutex.unlock();
        wait();
    }

    abort = false;

    // signal MW all stopped if a folder change
    //if (G::stop) emit stopped("ImageCache");

    // turn off caching activity lights on statusbar
    emit updateIsRunning(false, false);  // flags = isRunning, showCacheLabel
}

bool ImageCache::allDecodersReady()
{
    for (int id = 0; id < decoderCount; ++id) {
        // if (decoder[id]->status != ImageDecoder::Status::Ready) return false;
        if (decoder[id]->isRunning()) return false;
    }
    return true;
}

bool ImageCache::lastDecoderDone()
{
    for (int id = 0; id < decoderCount; ++id) {
        if (decoder[id]->status == ImageDecoder::Status::Busy) return false;
    }
    return true;
}

float ImageCache::getImCacheSize()
{
    // return the current size of the cache
    //if (debugCaching) qDebug() << "ImageCache::getImCacheSize";
    log("getImCacheSize");
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

bool ImageCache::isValidKey(int key)
{
    if (key > -1 && key < icd->cacheItemList.size()) return true;
    else return false;
}

void ImageCache::updateTargets()
{
/*
    Called by setCurrentPosition.
*/
    log("updateTargets");

isCacheItemListComplete = cacheItemListBuildCompleted();
    warnings.clear();

    if (debugCaching)
    {
        qDebug().noquote() << "ImageCache::updateTargets"
                           << "current position =" << icd->cache.key
                           << "total =" << icd->cacheItemList.size()
                           << "  isCacheItemListComplete" << isCacheItemListComplete
            ;

    }

    // Mutex req'd (do not remove 2023-11-13)
    QMutexLocker locker(&gMutex);
    icd->cache.key = dm->currentSfRow;
    setDirection();
    icd->cache.currMB = getImCacheSize();
    setPriorities(icd->cache.key);
    setTargetRange();
    resetCachingFlags();
}

void ImageCache::resetCachingFlags()
{
/*
    Rapidly updating the cache can result in aborted decoder threads that leave
    isCaching and cached states orphaned.  Reset orphans to false in the target
    range.
*/
    log("resetCachingFlags");

    for (int i = icd->cache.targetFirst; i < icd->cache.targetLast; ++i) {
        if (abort) break;
        if (i >= icd->cacheItemList.size()) break;
        // imageCacheList build not finished
        if (!icd->cacheItemList.at(i).isUpdated) break;
        // isCaching
        if (icd->cacheItemList.at(i).isCaching) {
            icd->cacheItemList[i].isCaching = false;
        }
        // isCached and not in imCache
        if (icd->cacheItemList.at(i).isCached &&
            !icd->imCache.contains(icd->cacheItemList.at(i).fPath))
        {
            icd->cacheItemList[i].isCached = false;
        }
    }
}

bool ImageCache::cacheItemListBuildCompleted()
{
    log("cacheItemListComplete");
    for (int i = 0; i < icd->cacheItemList.size(); ++i) {
        if (!icd->cacheItemList.at(i).isUpdated) {
            return false;
        }
    }
    return true;
}

void ImageCache::setDirection()
{
    /*
    If the direction of travel changes then delay reversing the caching direction until a
    directionChangeThreshold (ie 3rd) image is selected in the new direction of travel. This
    prevents needless caching if the user justs reverses direction to check out the previous
    image
*/
    if (debugCaching) {
        qDebug().noquote() << "ImageCache::setDirection";
    }
    log("setDirection");

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

void ImageCache::setPriorities(int key)
{
/*
    Starting at the current key, this algorithm iterates through the icd->cacheItemList,
    following the order (2 ahead, one behind) and assigned an increasing sort order key, which
    is used by setTargetRange to sort icd->cacheItemList by priority.
*/
    if (debugCaching) {
        qDebug().noquote() << "ImageCache::setPriorities" << "  starting with row =" << key;  // row = key
    }
    log("setPriorities", "key = " + QString::number(key));
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

    // update priorities
    if (key < icd->cacheItemList.length()) {
        //gMutex.lock();//rgh
        icd->cacheItemList[key].priority = 0;
        //gMutex.unlock();
    }
    int i = 1;                  // start at 1 because current pos preset to zero
    if (icd->cache.isForward) {
        aheadPos = key + 1;
        behindPos = key - 1;
        while (i < icd->cacheItemList.length()) {
            for (int b = behindPos; b > behindPos - behindAmount; --b) {
                for (int a = aheadPos; a < aheadPos + aheadAmount; ++a) {
                    if (a >= icd->cacheItemList.length()) break;
                    //gMutex.lock();//rgh
                    icd->cacheItemList[a].priority = i++;
                    //gMutex.unlock();
                    if (i >= icd->cacheItemList.length()) break;
                    if (a == aheadPos + aheadAmount - 1 && b < 0) aheadPos += aheadAmount;
                }
                aheadPos += aheadAmount;
                if (b < 0) break;
                //gMutex.lock();//rgh
                icd->cacheItemList[b].priority = i++;
                //gMutex.unlock();
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
                    //gMutex.lock();//rgh
                    icd->cacheItemList[a].priority = i++;
                    //gMutex.unlock();
                    if (i >= icd->cacheItemList.length()) break;
                    if (a == aheadPos - aheadAmount + 1 && b > icd->cacheItemList.length())
                        aheadPos -= aheadAmount;
                }
                aheadPos -= aheadAmount;
                if (b >= icd->cacheItemList.length()) break;
                //gMutex.lock();//rgh
                icd->cacheItemList[b].priority = i++;
                //gMutex.unlock();
                if (i > icd->cacheItemList.length()) break;
            }
            behindPos += behindAmount;
        }
    }
    //    */
}

void ImageCache::setTargetRange()
/*
    The target range is the list of images being targeted to cache, based on the current
    image, the direction of travel, the caching strategy and the maximum memory allotted
    to the image cache.

    The start and end of the target range are determined (cache.targetFirst and
    cache.targetLast) and the boolean isTarget is assigned for each item in the
    cacheItemList.

    Each item in the target range is added to the priority list.

    Any images in imCache that are no longer in the target range are removed.
*/
{
    log("setTargetRange");
    if (debugCaching)
        qDebug() << "ImageCache::setTargetRange";

    // sort by priority to make it easy to find highest priority not already cached
    std::sort(icd->cacheItemList.begin(), icd->cacheItemList.end(), &ImageCache::prioritySort);

    // assign target files to cache
    float sumMB = 0;
    float prevMB = 0;
    priorityList.clear();  // crash
    for (int i = 0; i < icd->cacheItemList.length(); ++i) {
        if (icd->cacheItemList.at(i).isVideo) {
            icd->cacheItemList[i].isTarget = false;
            continue;
        }

        // icd->cacheItemList may not be completely loaded yet, use last image size
        if (!icd->cacheItemList.at(i).metadataLoaded) {
            icd->cacheItemList[i].sizeMB = prevMB;
        }

        sumMB += icd->cacheItemList.at(i).sizeMB;
        prevMB = icd->cacheItemList.at(i).sizeMB;
        if (sumMB < icd->cache.maxMB) {
            icd->cacheItemList[i].isTarget = true;
            priorityList.append(icd->cacheItemList.at(i).key);
        }
        else {
            icd->cacheItemList[i].isTarget = false;

//            icd->cacheItemList[i].isCaching = false;    // new
//            icd->cacheItemList[i].isCached = false;     // new
        }
    }

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
        icd->cache.targetLast = icd->cacheItemList.length() - 1;
    }

    if (debugCaching) {
        qDebug() << "ImageCache::setTargetRange"
                 << " targetFirst =" << icd->cache.targetFirst
                 << "targetLast =" << icd->cache.targetLast
                 << "isForward =" << icd->cache.isForward
            ;
    }

    // remove cached images outside target range
    QVector<QString> keys;
    icd->imCache.getKeys(keys);
    // iterate imCache
    for (int key = 0; key < keys.size(); key++) {
        QString fPath = keys.at(key);
        // i = cacheKey
        // could be issue when deleting an image
        if (!keyFromPath.contains(fPath)) continue; // filter crash

        int i = keyFromPath[fPath]; // filter crash
        if (i < icd->cache.targetFirst || i > icd->cache.targetLast) {
            if (debugCaching)
            {
                qDebug().noquote()
                    << "ImageCache::setTargetRange outside target range"
                    << "i =" << i
                    << "isCached =" << icd->cacheItemList.at(i).isCached
                    << "targetFirst =" << icd->cache.targetFirst
                    << "targetLast =" << icd->cache.targetLast
                    << "fPath =" << fPath
                       ;
            }
            icd->imCache.remove(fPath);
            if (icd->cacheItemList.at(i).isCached) {
                icd->cacheItemList[i].isCached = false;
                emit updateCacheOnThumbs(fPath, false, "ImageCache::setTargetRange");
            }
            if (icd->cacheItemList.at(i).isCaching) {
                icd->cacheItemList[i].isCaching = false;
            }
            if (icd->cacheItemList[i].attempts) {
                icd->cacheItemList[i].attempts = 0;
            }
        }
    }
}

void ImageCache::removeCachedImage(QString fPath)
{
    log("removeCachedImage", fPath);
    QMutexLocker locker(&gMutex);
    int i = keyFromPath[fPath];
    if (debugCaching)
    {
        qDebug().noquote()
            << "ImageCache::removeCachedImage"
            << "i =" << i
            << fPath
            ;
    }
    icd->imCache.remove(fPath);
    if (icd->cacheItemList.at(i).isCached) {
        icd->cacheItemList[i].isCached = false;
        emit updateCacheOnThumbs(fPath, false, "ImageCache::setTargetRange");
    }
    if (icd->cacheItemList.at(i).isCaching) {
        icd->cacheItemList[i].isCaching = false;
    }
    if (icd->cacheItemList[i].attempts) {
        icd->cacheItemList[i].attempts = 0;
    }
}

bool ImageCache::nextToCache(int id)
{
/*
    The next image to cache is determined by traversing the cacheItemList in ascending
    order to find the highest priority item to cache in the target range based on these
    criteria:

    • isCaching and isCached are false and attempts < maxAttemptsToCacheImage.

    • decoderId matches item, isCaching is true and isCached = false. If this is the case
      then we know the previous attempt failed, and we should try again if the failure was
      because the file was already open, unless the attempts are greater than
      maxAttemptsToCacheImage.

*/
    //log("nextToCache", "CacheUpToDate = " + QVariant(cacheUpToDate()).toString());
    bool debugThis = false;
    if (debugCaching)
    {
        // qDebug() << "ImageCache::nextToCache";
    }

    if (G::instanceClash(instance, "ImageCache::nextToCache")) {
        return false;
    }

    if (priorityList.size() > icd->cacheItemList.size()) {
        if (debugCaching) {
            QString dmInst = QString::number(dm->instance);
            QString inst = QString::number(instance);
            QString msg = "priorityList is out of date.  dmInstance: " + dmInst + " instance: " + inst + ".";
            G::issue("Warning", msg, "ImageCache::nextToCache");
        }
        return false;
    }

    QString reason;

    // iterate priority list until find item to cache
    for (int p = 0; p < priorityList.size(); p++) {
        if (abort || G::stop) return false;

        reason = "";

        // prevent crash when priority has just updated
        if (p >= priorityList.size()) {
            if (debugCaching) {
                QString msg = "Exceeded priorityList range, priorities may have been just updated.";
                G::issue("Warning", msg, "ImageCache::nextToCache");
                return false;
            }
        }

        // icd->cacheItemList row i
        int i = priorityList.at(p);

        /*
        log("nextToCache row",
            "decoder " + QString::number(id).leftJustified(3) +
            "row = " + QString::number(i).leftJustified(5) +
            "row decoder = " + QString::number(icd->cacheItemList.at(i).decoderId).leftJustified(3) +
            "isCached = " + QVariant(icd->cacheItemList.at(i).isCached).toString().leftJustified(6) +
            "isCaching = " + QVariant(icd->cacheItemList.at(i).isCaching).toString().leftJustified(6) +
            "attempt = " + QString::number(icd->cacheItemList.at(i).attempts).leftJustified(3) +
            "decoder status = " + decoder.at(id)->statusText.at(icd->cacheItemList.at(i).status)
            );
        */

        // out of range
        if (i >= icd->cacheItemList.size()) {
            if (debugCaching) {
                QString msg = "Exceeded cacheItemList range.";
                G::issue("Warning", msg, "ImageCache::nextToCache", i, icd->cacheItemList.at(i).fPath);
                return false;
            }
        }

        // make sure metadata has been loaded
        if (!icd->cacheItemList.at(i).isUpdated) {
            if (debugCaching)
            {
                // qDebug() << "ImageCache::nextToCache metadata not updated"
                //          << "row =" << i
                //          << "p =" << p
                //          // << priorityList
                //     ;
            }
            continue;
        }

        // decoder assigned and not the same decoder
        if (icd->cacheItemList.at(i).decoderId != -1 && id != icd->cacheItemList.at(i).decoderId) {
            if (debugCaching) {
                // qDebug() << "ImageCache::nextToCache                           "
                //          << "row =" << i
                //          << "1st id =" << id
                //          << "2st id =" << icd->cacheItemList.at(i).decoderId
                //          << "(isCaching = true and not same decoder id)"
                //          << "decoder status =" << icd->cacheItemList.at(i).status
                //     ;
            }
            continue;
        }

        if (debugCaching || debugThis)
        {
            // qDebug() << "ImageCache::nextToCache                           "
            //          << "decoder" << id
            //          << "row =" << i
            //          << "row decoder =" << icd->cacheItemList.at(i).decoderId
            //          << "isCached =" << icd->cacheItemList.at(i).isCached
            //          << "isCaching =" << icd->cacheItemList.at(i).isCaching
            //          << "attempt =" << icd->cacheItemList.at(i).attempts
            //          << "decoder status =" << icd->cacheItemList.at(i).status
            //     ;
        }

        // already in imCache
        if (icd->imCache.contains(icd->cacheItemList.at(i).fPath)) continue;
        else reason = "reason: not inCache";

        // already cached
        if (icd->cacheItemList.at(i).isCached) continue;
        else reason = "reason: not isCached";

        // invalid image
        if (icd->cacheItemList.at(i).status == inValidImage) {
            if (debugCaching)
            {
                QString status = decoder[id]->statusText.at(icd->cacheItemList.at(i).status);
                QString msg = "Invalid image.  Decoder status: " + status + ".";
                G::issue("Warning", msg, "ImageCache::nextToCache", i, icd->cacheItemList.at(i).fPath);
            }
            continue;
        }
        else reason = "reason: invalid image status";

        // max attempts exceeded
        if (icd->cacheItemList.at(i).attempts > maxAttemptsToCacheImage /*&& !G::allMetadataLoaded*/) {
            if (debugCaching)
            {
                QString msg = "Exceeded max attempts.";
                G::issue("Warning", msg, "ImageCache::nextToCache", i, icd->cacheItemList.at(i).fPath);
            }
            continue;
        }
        else reason = "reason: max attempts not exceeded";

        // isCaching and not the same decoder
        if (icd->cacheItemList.at(i).isCaching && (id != icd->cacheItemList.at(i).decoderId)) {
            if (debugCaching) {
                // qDebug() << "ImageCache::nextToCache                           "
                //          << "row =" << i
                //          << "1st id =" << id
                //          << "2st id =" << icd->cacheItemList.at(i).decoderId
                //          << "(isCaching = true and not same decoder id)"
                //          << "decoder status =" << icd->cacheItemList.at(i).status
                //     ;
            }
            continue;
        }
        else reason = "not (isCaching and not same decoder)";

        /* debug multi attempts
        if (icd->cacheItemList.at(i).attempts > 0)
        {
            if (debugCaching) {
                qDebug() << "ImageCache::nextToCache                           "
                         << "row =" << i
                         << "attempt =" << icd->cacheItemList.at(i).attempts
                         << "decoder status return =" << icd->cacheItemList.at(i).status
                    ;
            }
        }  //*/

        // next item to cache
        icd->cache.toCacheKey = i;
        if (debugCaching || debugThis)
        {
            qDebug().noquote()
                << "ImageCache::nextToCache = true       "
                << "decoder" << QString::number(id).leftJustified(2)
                << "row =" << QString::number(i).leftJustified(4)
                << "attempt =" << icd->cacheItemList.at(i).attempts
                << "decoder status =" << decoder[id]->statusText.at(decoder[id]->status)
                << reason
                ;
        }
        /*
        log("nextToCache result",
            "decoder " + QString::number(id).leftJustified(3) +
            "row = " + QString::number(i).leftJustified(5) +
            "row decoder = " + QString::number(icd->cacheItemList.at(i).decoderId).leftJustified(3) +
            "isCached = " + QVariant(icd->cacheItemList.at(i).isCached).toString().leftJustified(6) +
            "isCaching = " + QVariant(icd->cacheItemList.at(i).isCaching).toString().leftJustified(6) +
            "attempt = " + QString::number(icd->cacheItemList.at(i).attempts).leftJustified(3) +
            "decoder status = " + decoder.at(id)->statusText.at(icd->cacheItemList.at(i).status)
            );
        */
        return true;
    }

    // nothing found to cache
    icd->cache.toCacheKey = -1;
    if (debugCaching || debugThis)
    {
        qDebug() << "ImageCache::nextToCache = false                   "
             << "decoder" << id
        ;
    }
    return false;

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
    //log("cacheUpToDate", "Start");
    bool debugThis = false;
    isCacheUpToDate = true;
    for (int i = icd->cache.targetFirst; i <= icd->cache.targetLast; ++i) {
        if (i >= icd->cacheItemList.count()) break;
        // skip videos
        if (icd->cacheItemList.at(i).isVideo) continue;

        int id = icd->cacheItemList.at(i).decoderId;
        bool isCached = icd->cacheItemList.at(i).isCached;
        bool isCaching = icd->cacheItemList.at(i).isCaching;
        bool inCache = icd->imCache.contains(icd->cacheItemList.at(i).fPath);

        // the cache contains the image
        if (inCache) continue;

        // isCached is true but no associated decoder
        if (isCached && id == -1) {
            /*
            log("cacheUpToDate passover",
                    "row = " + QString::number(i).leftJustified(5) +
                    "row decoder = " + QString::number(icd->cacheItemList.at(i).decoderId).leftJustified(3) +
                    "isCached = " + QVariant(icd->cacheItemList.at(i).isCached).toString().leftJustified(6) +
                    "isCaching = " + QVariant(icd->cacheItemList.at(i).isCaching).toString().leftJustified(6) +
                    "attempt = " + QString::number(icd->cacheItemList.at(i).attempts).leftJustified(3)
                );
            //*/
            if (debugThis) {
            qDebug().noquote()
                 << "ImageCache::cacheUpToDate isCached is true but no associated decoder"
                 << "row = " << i
                 << "decoder =" << icd->cacheItemList.at(i).decoderId
                 << "isCached =" << icd->cacheItemList.at(i).isCached
                 << "isCaching =" << icd->cacheItemList.at(i).isCaching
                 << "attempts =" << icd->cacheItemList.at(i).attempts;
            }
            icd->cacheItemList[i].isCached = false;
            icd->cacheItemList[i].isCaching = false;
            isCacheUpToDate = false;
            break;
        }

        // check if caching image is in progress
        if (!isCached && !isCaching) {
            /*
            log("cacheUpToDate noactivity",
                "row = " + QString::number(i).leftJustified(5) +
                    "row decoder = " + QString::number(icd->cacheItemList.at(i).decoderId).leftJustified(3) +
                    "isCached = " + QVariant(icd->cacheItemList.at(i).isCached).toString().leftJustified(6) +
                    "isCaching = " + QVariant(icd->cacheItemList.at(i).isCaching).toString().leftJustified(6) +
                    "attempt = " + QString::number(icd->cacheItemList.at(i).attempts).leftJustified(3)
                );
            */
            if (debugThis)
            {
            qDebug().noquote()
                << "ImageCache::cacheUpToDate not caching or cached"
                << "row = " << i
                << "decoder =" << icd->cacheItemList.at(i).decoderId
                << "isCached =" << icd->cacheItemList.at(i).isCached
                << "isCaching =" << icd->cacheItemList.at(i).isCaching
                << "attempts =" << icd->cacheItemList.at(i).attempts;
            }
            isCacheUpToDate = false;
            break;
        }
    }
    if (debugCaching || debugThis)
    {
        qDebug() << "ImageCache::cacheUpToDate"
                 << "isCacheUpToDate =" << isCacheUpToDate
                 << "targetFirst =" << icd->cache.targetFirst
                 << "targetLast =" << icd->cache.targetLast
            ;
    }
    //log("cacheUpToDate", QVariant(isCacheUpToDate).toString());
    return isCacheUpToDate;
}

void ImageCache::setSizeMB(int id, int cacheKey)
{
/*
    For images that Winnow does not have the metadata describing the size (width and
    height), ie PNG files, an initial sizeMB is estimated to be used in the target range.
    After a decoder has converted the file into a QImage the sizeMB, dimensions (w,h) and
    target range are updated here.

    If this is not done the ImageCache::setTargetRange algorithm will not be updated correctly.
*/
    log("setSizeMB");
    if (debugCaching) qDebug() << "ImageCache::setSizeMB";
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
    log("memChk");
    if (debugCaching) qDebug() << "ImageCache::memChk";

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
    log("removeFromCache");

    // Mutex req'd (do not remove 2023-11-13)
    QMutexLocker locker(&gMutex);

    // remove items from icd->cacheItemList, i
    for (int i = 0; i < pathList.count(); ++i) {
        QString fPathToRemove = pathList.at(i);
        icd->imCache.remove(fPathToRemove);
        for (int j = icd->cacheItemList.length() - 1; j >= 0; --j) {
            QString fPath = pathFromKey[j];
            if (fPath == fPathToRemove) {
                icd->cacheItemList.removeAt(j);
            }
        }
    }

    /* redo keys (rows) in icd->cacheItemList to make contiguous ie 1,2,3; not 1,2,4;
       redo cacheKeyHash and update cache current size */
    keyFromPath.clear();
    pathFromKey.clear();
    icd->cache.currMB = 0;
    for (int i = 0; i < icd->cacheItemList.length(); ++i) {
        QString fPath = icd->cacheItemList.at(i).fPath;
        icd->cacheItemList[i].key = i;
        if (icd->cacheItemList.at(i).isCached) {
            icd->cache.currMB += icd->cacheItemList.at(i).sizeMB;
        }
        keyFromPath[fPath] = i;
        pathFromKey[i] = fPath;
    }
}

// rgh needs tweaks as identified
void ImageCache::rename(QString oldPath, QString newPath)
{
    log("rename");
    if(icd->imCache.contains(oldPath)) {
        icd->imCache.rename(oldPath, newPath);
    }
    int i = keyFromPath[oldPath];
    icd->cacheItemList[i].fPath = newPath;
}

void ImageCache::updateStatus(QString instruction, QString source)
{
    emit showCacheStatus(instruction, icd->cache, source);
}

void ImageCache::log(const QString function, const QString comment)
{
    if (debugLog || G::isLogger || G::isFlowLogger) {
        G::log("ImageCache::" + function, comment);
    }
}

QString ImageCache::diagnostics()
{
    log("diagnostics");
    if (debugCaching) qDebug() << "ImageCache::diagnostics";
    QString reportString;
    QTextStream rpt;
    rpt.setString(&reportString);
    rpt << Utilities::centeredRptHdr('=', objectName() + " ImageCache Diagnostics");
    rpt << "\n\n";
    rpt << reportCacheParameters();
    rpt << reportCacheDecoders();
    rpt << reportWarnings();
    rpt << reportCache("");
    rpt << reportImCache();

    rpt << "\n\n" ;
    return reportString;
}

QString ImageCache::reportCacheParameters()
{
    log("reportCacheParameters");
    if (debugCaching) qDebug() << "ImageCache::reportCacheParameters";
    QString reportString;
    QTextStream rpt;
    rpt.flush();
    reportString = "";
    rpt.setString(&reportString);

    rpt << "\n";
    rpt << "isRunning                = " << (isRunning() ? "true" : "false") << "\n";
    rpt << "abort                    = " << (abort ? "true" : "false") << "\n";

    rpt << "\n";
    rpt << "instance                 = " << instance << "\n";
    rpt << "G::dmInstance            = " << G::dmInstance << "\n";

    rpt << "\n";
    rpt << "key                      = " << icd->cache.key << "\n";
    rpt << "prevKey                  = " << icd->cache.prevKey << "\n";
    rpt << "directionChangeThreshold = " << icd->cache.directionChangeThreshold << "\n";
    rpt << "wtAhead                  = " << icd->cache.wtAhead << "\n";
    rpt << "isForward                = " << (icd->cache.isForward ? "true" : "false") << "\n";
    rpt << "totFiles                 = " << icd->cacheItemList.length() << "\n";
    rpt << "currMB                   = " << icd->cache.currMB << "\n";
    rpt << "currMB Check             = " << getImCacheSize() << "\n";
    rpt << "maxMB                    = " << icd->cache.maxMB << "\n";
    rpt << "minMB                    = " << icd->cache.minMB << "\n";
    rpt << "folderMB                 = " << icd->cache.folderMB << "          (if all metadata loaded)\n";
    rpt << "targetFirst              = " << icd->cache.targetFirst << "\n";
    rpt << "targetLast               = " << icd->cache.targetLast << "\n";
    rpt << "decoderCount             = " << icd->cache.decoderCount << "\n";
    rpt << "\n";
    rpt << "currentPath              = " << currentPath << "\n";
    rpt << "cacheSizeHasChanged      = " << (cacheSizeHasChanged ? "true" : "false") << "\n";
    rpt << "filterOrSortHasChanged   = " << (filterOrSortHasChanged ? "true" : "false") << "\n";
    rpt << "isCacheUpToDate          = " << (isCacheUpToDate ? "true" : "false") << "\n";
    rpt << "isFinalCheckCompleted    = " << QVariant(isFinalCheckCompleted).toString() << "\n";
    return reportString;
}

QString ImageCache::reportCacheDecoders()
{
    log("reportCacheDecoders");
    if (debugCaching) qDebug() << "ImageCache::reportCacheDecoders";

    QString reportString;

    QTextStream rpt;
    rpt.flush();
    reportString = "";
    rpt.setString(&reportString);
    rpt << "\nDecoders:\n";
    rpt << "  ID  ";
    rpt << "Status        ";
    rpt << "  Key  ";
    rpt << "  DM Instance ";
    rpt << "  isRunning ";
    rpt << "\n";
    for (int id = 0; id < decoderCount; ++id) {
        rpt << QString::number(id).rightJustified(4);
        rpt << "  ";
        rpt << decoder[id]->statusText.at(decoder[id]->status).leftJustified(14);
        rpt << QString::number(decoder[id]->cacheKey).rightJustified(5);
        rpt << "  ";
        rpt << QString::number(instance).rightJustified(13);
        rpt << "   ";
        rpt << QVariant(decoder[id]->isRunning()).toString();
        rpt << "\n";
    }

    return reportString;
}

QString ImageCache::reportWarnings()
{
    log("reportCacheDecoders");
    if (debugCaching) qDebug() << "ImageCache::reportCacheDecoders";

    QString reportString;

    QTextStream rpt;
    rpt.flush();
    reportString = "";
    rpt.setString(&reportString);
    rpt << "\nWarnings:\n";
    for (QString warning : warnings) {
        rpt << "  ";
        rpt << warning;
        rpt << "\n";
    }

    return reportString;
}

QString ImageCache::reportCache(QString title)
{
    log("reportCache");
    if (debugCaching) qDebug() << "ImageCache::reportCache";

    QHash<int, QString> rptStatus;
    rptStatus[0] = "Ready";
    rptStatus[1] = "Busy";
    rptStatus[2] = "Success";
    rptStatus[3] = "Abort";
    rptStatus[4] = "Invalid";
    rptStatus[5] = "Failed";
    rptStatus[6] = "Video";
    rptStatus[7] = "Clash";
    rptStatus[8] = "NoDir";
    rptStatus[9] = "NoPath";
    rptStatus[10] = "NoMeta";
    rptStatus[11] = "FileOpen";

    QString reportString;
    QTextStream rpt;
    rpt.flush();
    reportString = "";
    rpt.setString(&reportString);

    rpt << "\ncacheItemList:\n";
    int cachedCount = 0;
    for (int i = 0; i < icd->cacheItemList.length(); ++i) {
        // show header every 40 rows
        if (i % 40 == 0) {
            rpt.reset();
            rpt.setFieldAlignment(QTextStream::AlignRight);
            rpt.setFieldWidth(9);
            rpt
                //                << "Updated"
                << "Key"
                << "Priority"
                << "Decoder"
                << "Target"
                << "Attempts"
                << "Caching"
                << "Cached"
                << "Status"
                << "imCached"
                << "dmCached"
                << "Metadata"
                << "Update"
                << "Video"
                ;
            rpt.setFieldWidth(11);
            rpt << "SizeMB";
            rpt.setFieldWidth(9);
            rpt
                << "Estimate"
                << "Offset"
                << "Length"
                ;
            rpt.setFieldAlignment(QTextStream::AlignLeft);
            rpt.setFieldWidth(1);
            rpt.setFieldWidth(3);
            rpt << "   ";
            rpt.setFieldWidth(30);
            rpt << "File Name";
            rpt.setFieldWidth(1);
            rpt << " ";
            rpt.setFieldWidth(50);
            rpt << "Error message";
            rpt.setFieldWidth(0);
            rpt << "\n";
        }
        rpt.setFieldWidth(9);
        rpt.setFieldAlignment(QTextStream::AlignRight);
        rpt
            //            << (icd->cacheItemList.at(i).isUpdated ? "true" : "false")
            << icd->cacheItemList.at(i).key
            << icd->cacheItemList.at(i).priority
            << icd->cacheItemList.at(i).decoderId
            << (icd->cacheItemList.at(i).isTarget ? "true" : "false")
            << icd->cacheItemList.at(i).attempts
            << (icd->cacheItemList.at(i).isCaching ? "true" : "false")
            << (icd->cacheItemList.at(i).isCached ? "true" : "false")
            << rptStatus[icd->cacheItemList.at(i).status]
            << (icd->imCache.contains(icd->cacheItemList.at(i).fPath) ? "true" : "false")
            << (dm->sf->index(icd->cacheItemList.at(i).key,0).data(G::UserRoles::CachedRole).toBool() ? "true" : "false")
            << (icd->cacheItemList.at(i).metadataLoaded ? "true" : "false")
            << (icd->cacheItemList.at(i).isUpdated ? "true" : "false")
            << (icd->cacheItemList.at(i).isVideo ? "true" : "false")
            ;
        rpt.setFieldWidth(11);
        rpt << QString::number(icd->cacheItemList.at(i).sizeMB, 'f', 3);
        rpt.setFieldWidth(9);
        rpt
            << icd->cacheItemList.at(i).estSizeMB
            << icd->cacheItemList.at(i).offsetFull
            << icd->cacheItemList.at(i).lengthFull
            ;
        rpt.setFieldAlignment(QTextStream::AlignLeft);
        rpt.setFieldWidth(3);
        rpt << "   ";
        rpt.setFieldWidth(30);
        rpt << Utilities::getFileName(icd->cacheItemList.at(i).fPath);
        rpt.setFieldWidth(1);
        rpt << " ";
        rpt.setFieldWidth(50);
        rpt << icd->cacheItemList.at(i).errMsg.left(50);
        rpt.setFieldWidth(0);
        rpt << "\n";
        if (icd->cacheItemList.at(i).isCached) cachedCount++;
    }
    rpt << cachedCount << " images reported as cached." << "\n";
    //std::cout << reportString.toStdString() << std::flush;

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
    QString reportString;
    QTextStream rpt;
    rpt.flush();
    reportString = "";
    rpt.setString(&reportString);

    QVector<QString> keys;
    // check when imCache is empty
    QImage image;
    int mem = 0;
    icd->imCache.getKeys(keys);
    if (keys.size() == 0) {
        rpt << "\nicd->imCache is empty";
        return reportString;
    }

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
        imRptItem.cacheItemListKey = keyFromPath[imRptItem.fPath];
        //imRptItem.cacheItemListKey = keyFromPath(imRptItem.fPath);
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
        rptList.append(imRptItem);
    }

    // report header
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
    log("reportCacheProgress");
    if (debugCaching) qDebug() << "ImageCache::reportCacheProgress";
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
             << "filterSortChange =" << filterOrSortHasChanged
             << "cacheSizeChange =" << cacheSizeHasChanged
             << "currentPath =" << currentPath;
}

void ImageCache::syncToDatamodel()  // not req'd
{
    for (int i = 0; i < dm->sf->rowCount(); i++) {
        QString fPathDM = dm->sf->index(i,0).data().toString();
        qDebug() << fPathDM;
        QString fPathIC = icd->cacheItemList.at(i).fPath;
        if (fPathDM != fPathIC) {

        }
    }
}

void ImageCache::addCacheItemImageMetadata(ImageMetadata m, int instance)
{
/*
    Concurrent metadata loading alternative to buildImageCacheList. The imageCacheList
    metadata information is added row by row, triggered by signal addToImageCache in
    MetaRead::readMetadata.
*/
    log("addCacheItemImageMetadata", "Row = " + QString::number(m.row));
    if (debugCaching)
        qDebug() << "ImageCache::addCacheItemImageMetadata"
                 << "row =" << m.row << m.fName
                    ;
    if (instance != dm->instance) {
        if (debugCaching) {
            QString msg = "Instance clash.";
            G::issue("Comment", msg, "ImageCache::addCacheItemImageMetadata", m.row, m.fPath);
        }
        return;
    }
    if (!G::useImageCache) return;  // rgh isolate image cache
    if (G::stop) {
        return;
    }

    QMutexLocker locker(&gMutex);
    // row same as datamodel
    int row;
    if (!keyFromPath.contains(m.fPath)) {
        // insert new item in icd->cacheItemList
        row = m.row;
        // qDebug() << "ImageCache::addCacheItemImageMetadata" << "row =" << row << m.fPath;
        icd->cacheItem.isUpdated = false;
        icd->cacheItem.key = row;              // need to be able to sync with imageList
        icd->cacheItem.fPath = m.fPath;
        icd->cacheItem.ext = m.ext;
        icd->cacheItem.status = 0;
        icd->cacheItem.isCaching = false;
        icd->cacheItem.attempts = 0;
        icd->cacheItem.decoderId = -1;
        icd->cacheItem.isCached = false;
        icd->cacheItem.isTarget = false;
        icd->cacheItem.priority = row;
        icd->cacheItem.metadataLoaded = dm->sf->index(row, G::MetadataLoadedColumn).data().toBool();

        // insert new row
        G::showIssueInConsole = true;
        if (row < icd->cacheItemList.size())
            icd->cacheItemList.insert(row, icd->cacheItem);
        else
            icd->cacheItemList.append(icd->cacheItem);
        keyFromPath[m.fPath] = row;

        // increment key for rest of list
        for (int i = row + 1; i < icd->cacheItemList.size(); i++) {
            icd->cacheItemList[i].key = i;
        }
    }
    else row = keyFromPath[m.fPath];

    if (row >= icd->cacheItemList.size()) {
        QString n = QString::number(icd->cacheItemList.size());
        QString msg = "Row exceeds cacheItemList size of " + n;
        G::issue("Warning", msg, "ImageCache::addCacheItemImageMetadata", row, m.fPath);
        return;
    }

    icd->cacheItemList[row].metadataLoaded = m.metadataLoaded;
    icd->cacheItemList[row].isVideo = m.video;

    if (m.video) {
        icd->cacheItemList[row].sizeMB = 0;
        icd->cacheItemList[row].estSizeMB = false;
        icd->cacheItemList[row].offsetFull = 0;
        icd->cacheItemList[row].lengthFull = 0;
        icd->cacheItemList[row].isUpdated = true;
        icd->cacheItemList[row].status = ImageDecoder::Status::Video;
        return;
    }

    // cacheItemList is a list of cacheItem used to track the current cache status and
    // make future caching decisions for each image.
    int w, h;
    m.widthPreview > 0 ? w = m.widthPreview : w = m.width;
    m.heightPreview > 0 ? h = m.heightPreview : h = m.height;

    // 8 bits X 3 channels + 8 bit depth = (32*w*h)/8/1024/1024 = w*h/262144
    float sizeMB = static_cast<float>(w * h * 1.0 / 262144);
    if (sizeMB > 0) {
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

    // item has been updated
    icd->cacheItemList[row].isUpdated = true;
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
    log("buildImageCacheList");
    if (debugCaching)
        qDebug() << "ImageCache::buildImageCacheList";
    icd->cacheItemList.clear();
    keyFromPath.clear();
    pathFromKey.clear();
    // the total memory size of all the images in the folder currently selected
    int folderMB = 0;

    for (int i = 0; i < dm->sf->rowCount(); ++i) {
        QString fPath = dm->sf->index(i, G::PathColumn).data(G::PathRole).toString();
        keyFromPath[fPath] = i;
        pathFromKey[i] = fPath;
        if (fPath == "") continue;
        /* cacheItemList is a list of cacheItem used to track the current
           cache status and make future caching decisions for each image  */
        icd->cacheItem.isUpdated = false;
        icd->cacheItem.key = i;              // need to be able to sync with imageList
        icd->cacheItem.fPath = fPath;
        icd->cacheItem.ext = fPath.section('.', -1).toLower();
        icd->cacheItem.status = 0;
        icd->cacheItem.isCaching = false;
        icd->cacheItem.attempts = 0;
        icd->cacheItem.decoderId = -1;
        icd->cacheItem.isCached = false;
        icd->cacheItem.isTarget = false;
        icd->cacheItem.priority = i;
        icd->cacheItem.metadataLoaded = dm->sf->index(i, G::MetadataLoadedColumn).data().toBool();
        if ((G::allMetadataLoaded)) {
            ImageMetadata m = dm->imMetadata(fPath);
            icd->cacheItem.metadataLoaded = m.metadataLoaded;
            icd->cacheItem.isVideo = m.video;
            icd->cacheItem.isUpdated = true;
            if (!m.video) {
                // 8 bits X 3 channels + 8 bit depth = (32*w*h)/8/1024/1024 = w*h/262144
                int w, h;
                m.widthPreview > 0 ? w = m.widthPreview : w = m.width;
                m.heightPreview > 0 ? h = m.heightPreview : h = m.height;
                int sizeMB = static_cast<int>(w * h * 1.0 / 262144);
                /*
                if (i < 20)
                    qDebug() << "ImageCache::buildImageCacheList"
                             << "w =" << w
                             << "h =" << h
                             << "sizeMB =" << sizeMB;
                //*/
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
                icd->cacheItem.isUpdated = true;
            }
        }
        icd->cacheItemList.append(icd->cacheItem);
    } // next row

    if (G::allMetadataLoaded) icd->cache.folderMB = folderMB;
}

void ImageCache::initImageCache(int &cacheMaxMB,
                                int &cacheMinMB,
                                bool &isShowCacheStatus,
                                int &cacheWtAhead)
{
    log("initImageCache");
    if (!G::useImageCache) return;   // rgh isolate image cache

    if (debugCaching) qDebug() << "ImageCache::initImageCache  dm->instance =" << dm->instance;

    abort = false;

    // reset the image cache
    clearImageCache(true);

    // cancel if no images to cache
    if (!dm->sf->rowCount()) return;

    // just in case stopImageCache not called before this
    if (isRunning()) stop();

    // update folder change instance
    instance = dm->instance;

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
    log("updateImageCacheParam");
    if (debugCaching) qDebug() << "ImageCache::updateImageCacheParam";
    //if (useMutex) gMutex.lock();
    icd->cache.maxMB = cacheSizeMB;
    icd->cache.minMB = cacheMinMB;
    icd->cache.isShowCacheStatus = isShowCacheStatus;
    icd->cache.wtAhead = cacheWtAhead;
    //if (useMutex) gMutex.unlock();
}

void ImageCache::rebuildImageCacheParameters(QString &currentImageFullPath, QString source)
{
/*
    When the datamodel is filtered the image cache needs to be updated. The cacheItemList is
    rebuilt for the filtered dataset and isCached updated, the current image is set, and any
    surplus cached images (not in the filtered dataset) are removed from imCache.
    The image cache is now ready to run by calling setCachePosition().
*/
    log("rebuildImageCacheParameters");
    if (debugCaching)
    {
        qDebug() << "ImageCache::rebuildImageCacheParameters";
    }
    if (dm->sf->rowCount() == 0) return;

    // do not use mutex - spins forever

    // update currentPath
    currentPath = currentImageFullPath;

    // update instance
    instance = dm->instance;

    // build a new cacheItemList for the filtered/sorted dataset
    buildImageCacheList();

    // update cacheItemList
    icd->cache.key = 0;
    // list of files in cacheItemList (all the filtered files) used to check for surplus
    QStringList filteredList;
    // assign cache.key and update isCached status in cacheItemList
    for (int row = 0; row < icd->cacheItemList.length(); ++row) {
        QString fPath = pathFromKey[row];
        //QString fPath = icd->cacheItemList.at(row).fPath;
        filteredList.append(fPath);
        // get key for current image
        if (fPath == currentImageFullPath) icd->cache.key = row;
        // update cacheItemList for images already cached
        if (icd->imCache.contains(fPath)) icd->cacheItemList[row].isCached = true;
        /*
        qDebug() << "ImageCache::rebuildImageCacheParameters"
                 << "row =" << row
                 << "icd->cache.key =" << icd->cache.key
                 << "isCached =" << icd->cacheItemList[row].isCached
                    ;
        //*/
    }

    // remove surplus images in icd->imCache
    QVector<QString> keys;
    icd->imCache.getKeys(keys);
    for (int i = keys.length() - 1; i > -1; --i) {
        if (!filteredList.contains(keys.at(i))) {
            //            qDebug() << "ImageCache::rebuildImageCacheParameters"
            //                     << "remove image =" << keys.at(i);
            icd->imCache.remove(keys.at(i));
        }
    }

    // if the sort has been reversed
    if (source == "SortChange") icd->cache.isForward = !icd->cache.isForward;

    setPriorities(icd->cache.key);
    setTargetRange();

    if (icd->cache.isShowCacheStatus)
        updateStatus("Update all rows", "ImageCache::rebuildImageCacheParameters");

    //setCurrentPosition(currentPath, "ImageCache::rebuildImageCacheParameters");
}

void ImageCache::refreshImageCache()
{
/*
    Reload all images in the cache.
*/
    log("refreshImageCache");
    if (debugCaching)
        qDebug() << "ImageCache::refreshImageCache";
    if (useMutex) gMutex.lock();
    // make all isCached = false
    for (int i = 0; i < icd->cacheItemList.length(); ++i) {
        if (icd->cacheItemList[i].isCached == true) {
            icd->cacheItemList[i].isCached = false;
        }
    }
    refreshCache = true;
    isFinalCheckCompleted = false;
    if (useMutex) gMutex.unlock();
    if (!isRunning()) {
        start(QThread::LowestPriority);
    }
}

void ImageCache::colorManageChange()
{
/*
    Called when color manage is toggled.  Reload all images in the cache.
*/
    if (debugCaching) qDebug() << "ImageCache::colorManageChange";
    log("colorManageChange");
    isFinalCheckCompleted = false;
    refreshImageCache();
}

void ImageCache::cacheSizeChange()
{
/*
    Called when changes are made to image cache parameters in preferences. The image cache
    direction, priorities and target are reset and orphans fixed.
*/
    log("cacheSizeChange");
    if (debugCaching)
        qDebug() << "ImageCache::cacheSizeChange";
    if (useMutex) gMutex.lock();
    cacheSizeHasChanged = true;
    isFinalCheckCompleted = false;
    if (useMutex) gMutex.unlock();
    if (!isRunning()) {
        start(QThread::LowestPriority);
    }
}

void ImageCache::setCurrentPosition(QString fPath, QString src)
{
/*
    Called from MW::fileSelectionChange to reset the position in the image cache. The image
    cache direction, priorities and target are reset and the cache is updated in fillCache.
*/
    int sfRow = dm->currentSfRow;
    log("setCurrentPosition", "row = " + QString::number(sfRow));
    if (debugCaching) {
        qDebug().noquote() << "ImageCache::setCurrentPosition"
                           << dm->rowFromPath(fPath)
                           << fPath
                           << "src =" << src
                           << "isRunning =" << isRunning();
    }

    if (G::dmInstance != instance) {
        if (debugCaching) {
            QString msg = "Instance clash from " + src;
            G::issue("Comment", msg, "ImageCache::setCurrentPosition", sfRow, fPath);
        }
        return;
    }

    // reset decoders status if necessary
    for (int id = 0; id < decoderCount; ++id) {
        ImageDecoder *d = decoder[id];
        if (!d->isRunning() && d->status != ImageDecoder::Status::Ready) {
            d->setReady();
        }
    }

    if (useMutex) gMutex.lock();
    currentPath = fPath;
    icd->cache.key = dm->currentSfRow;
    if (useMutex) gMutex.unlock();

    // image not cached and not video
    bool isVideo = icd->cacheItemList.at(icd->cache.key).isVideo;

    // not in cache, maybe loading
    if (!icd->imCache.contains(fPath) && !isVideo) {
        emit centralMsg("Loading Image...");
        if (dm->currentSfRow > 0) {
            QString msg = "Not in imCache, might be loading.";
            G::issue("Warning", msg, "ImageCache::setCurrentPosition", sfRow, fPath);
        }
    }
    // or could not load
    for (int i = 0; i < icd->cacheItemList.length(); ++i) {
        if (icd->cacheItemList.at(i).fPath == fPath) {
            if (icd->cacheItemList.at(i).status == ImageDecoder::Status::Invalid) {
                emit centralMsg("Unable to load: " + icd->cacheItemList.at(i).errMsg + " " +fPath);
                QString msg = "Invalid status, unable to load.";
                G::issue("Warning", msg, "ImageCache::setCurrentPosition", i, fPath);
            }
        }
    }

    isFinalCheckCompleted = false;

    // cacheItemList.at(i).status
    if (isRunning()) {
        // reset target range
        // qDebug() << "ImageCache::setCurrentPosition before updateTargets";
        updateTargets();
    }
    else {
        // qDebug() << "ImageCache::setCurrentPosition start(QThread::LowestPriority)";
        start(QThread::LowestPriority);
    }
}

void ImageCache::decodeNextImage(int id)
{
/*
    Called from fillCache to run a decoder for the next image to decode into a QImage.
    The decoders run in separate threads. When they complete the decoding they signal
    back to fillCache.
*/
    log ("decodeNextImage");
    int row = icd->cache.toCacheKey;
    icd->cacheItemList[row].isCaching = true;
    icd->cacheItemList[row].decoderId = id;
    icd->cacheItemList[row].attempts += 1;
    /*
    log("decodeNextImage",
        "decoder " + QString::number(id).leftJustified(3) +
        "row = " + QString::number(icd->cache.toCacheKey).leftJustified(5) +
        "row decoder = " + QString::number(icd->cacheItemList.at(row).decoderId).leftJustified(3) +
        "isCached = " + QVariant(icd->cacheItemList.at(row).isCached).toString().leftJustified(6) +
        "isCaching = " + QVariant(icd->cacheItemList.at(row).isCaching).toString().leftJustified(6) +
        "attempt = " + QString::number(icd->cacheItemList.at(row).attempts).leftJustified(3) +
        "isMetadata = " + QVariant(icd->cacheItemList.at(row).metadataLoaded).toString().leftJustified(6) +
        "status = " + decoder.at(id)->statusText.at(icd->cacheItemList.at(row).status)
        );
    */
    if (debugCaching)
    {
        QString k = QString::number(row).leftJustified((4));
        qDebug().noquote()
            << "ImageCache::decodeNextImage"
            << "decoder" << id
            << "row =" << k
            << "threadId =" << icd->cacheItemList.at(row).decoderId
            << "isMetadata =" << icd->cacheItemList.at(row).metadataLoaded
            << "isCaching =" << icd->cacheItemList.at(row).isCaching
            << "isCached =" << icd->cacheItemList.at(row).isCached
            << "attempts =" << icd->cacheItemList.at(row).attempts
            << "status =" << icd->cacheItemList.at(row).status
            << icd->cacheItemList.at(row).fPath
            ;
    }

    if (!abort) decoder[id]->decode(icd->cacheItemList.at(row), instance);
}

void ImageCache::cacheImage(int id, int cacheKey)
{
/*
    Called from fillCache to insert a QImage that has been decoded into icd->imCache.
    Do not cache video files, but do show them as cached for the cache status display.
*/
    QString comment = "decoder " + QString::number(id).leftJustified(3) +
                      "row = " + QString::number(cacheKey).leftJustified(5) +
                      "decoder[id]->fPath =" + decoder[id]->fPath;
    if (G::isLogger || G::isFlowLogger) log ("cacheImage", comment);
    if (debugCaching)
    {
        QString k = QString::number(cacheKey).leftJustified((4));
        qDebug().noquote() << "ImageCache::cacheImage                       "
                           << "     decoder" << id
                           << "row =" << k
                           << "errMsg =" << decoder[id]->errMsg
                           << "decoder[id]->fPath =" << decoder[id]->fPath
                           //<< "dm->currentFilePath =" << dm->currentFilePath
            ;
    }

    /* Safety check do not exceed max cache size.  The metadata determination of the image size
       could be wrong, resulting in icd->cacheItem.sizeMB being wrong.  If this is the case, then
       the target range will be wrong too.  Changed to issue a warning for now.*/
    if ((icd->cache.currMB + icd->cacheItemList[cacheKey].estSizeMB) > icd->cache.maxMB) {
        if (debugCaching) {
            QString estMB =  QString::number(icd->cache.currMB + icd->cacheItemList[cacheKey].estSizeMB);
            QString maxMB =  QString::number(icd->cache.maxMB);
            QString msg = "Estimated cache ." + estMB + " MB exceeds the maximum " + maxMB + " MB.";
            G::issue("Warning", msg, "ImageCache::cacheImage", cacheKey, decoder[id]->fPath);
        }
    }

    // cache the image if not a video
    if (decoder[id]->status != ImageDecoder::Status::Video) {
        // Check if initial sizeMB was estimated (image without preview metadata ie PNG)
        if (icd->cacheItemList[cacheKey].estSizeMB) setSizeMB(id, cacheKey);
        bool isImage = decoder[id]->image.width() > 0;
        if (!isImage) {
            QString msg = "Decoder returned a null image.";
            G::issue("Warning", msg, "ImageCache::cacheImage", cacheKey, decoder[id]->fPath);
        }
        icd->imCache.insert(decoder[id]->fPath, decoder[id]->image);
    }

    QMutexLocker locker(&gMutex);
    icd->cache.currMB = getImCacheSize();
    icd->cacheItemList[cacheKey].isCaching = false;
    icd->cacheItemList[cacheKey].isCached = true;
    QString errMsg = icd->cacheItemList[cacheKey].errMsg;

    // set datamodel isCached = true
    emit setValuePath(decoder[id]->fPath, 0, true, instance, G::CachedRole);
    if (decoder[id]->status != ImageDecoder::Status::Video) {
        // if current image signal ImageView::loadImage
        if (decoder[id]->fPath == dm->currentFilePath) {
            /*
            qDebug().noquote() << "ImageCache::cacheImage"
                              << "     decoder" << id
                              << "decoder[id]->fPath =" << decoder[id]->fPath
                              << "dm->currentFilePath =" << dm->currentFilePath
               ; //*/
            // clear "Loading Image..." central msg
            emit centralMsg("");
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
    Read all the image files from the target range and save the QImages in  the
    concurrent image cache hash icd->imCache (see cachedata.h).

    A number of ImageDecoders are created when ImageCache is created. Each ImageDecoder
    runs in a separate thread. The decoders convert an image file into a QImage and then
    signal this function (fillCache) with their id so the QImage can be inserted into the
    image cache. The ImageDecoders are launched from ImageCache::launchDecoders.

    The ImageDecoder has a status attribute that can be Ready, Busy or Done. When the
    decoder is created and when the QImage has been inserted into the image cache the
    status is set to Ready. When the decoder is called from CacheImage the status is set
    to Busy. Finally, when the decoder finishes the decoding in ImageDecoder::run the
    status is set to Done. Each decoder signals ImageCache::fillCache when the image has
    been converted into a QImage. Here the QImage is added to the imCache. If there are
    more targeted images to be cached, the next one is assigned to the decoder, which is
    run again. The decoders keep running until all the targeted images have been cached.

    If there is a file selection change, cache size change, color manage change or
    sort/filter change the image cache parameters are updated and this function is called
    for each Ready decoder. The decoders not Ready are Busy and already in the fillCache
    loop.

    An image decoder can be running when a new folder is selected, returning an image
    from the previous folder. When an image decoder is run it is seeded with the
    datamodel instance (which is incremented every time a new folder is selected). When
    the decoder signals back to ImageCache::fillCache the decoder instance is checked
    against the current dm->instance to confirm the decoder image is from the current
    datamodel instance.

    Each decoder follows this basic pattern:
    - launch
      - fillCache
        - starting
          - nextToCache
        - returning with QImage
          - cacheImage
          - nextToCache

    nextToCache detail:
    - nextToCache
      - yes
        - decodeNextImage
        - fillCache
        - cacheImage
      - no
        - cacheUpToDate (checks for orphans)
          - no
            - decodeNextImage
          - yes
            - done

*/

    int cacheKey = -1;       // row for image in cacheKeyHash (default to no key)

    // check if aborted
    if (abort || decoder[id]->status == ImageDecoder::Status::Abort) {
        icd->cacheItemList[cacheKey].isCaching = false;
        icd->cacheItemList[cacheKey].isCached = false;
        decoder[id]->status = ImageDecoder::Status::Ready;
        return;
    }

    // new decoder just getting started
    if (decoder[id]->status == ImageDecoder::Status::Ready) {
        if (debugCaching)
        {
            qDebug().noquote() << "ImageCache::fillCache"
                               << "     starting decoder" << id
                               << "status = Ready"
                ;
        }
        cacheKey = -1;
    }
    // returning instance clash
    else if (decoder[id]->status == ImageDecoder::Status::InstanceClash) {
            cacheKey = -1;
    }
    // returning decoder
    else {
        cacheKey = keyFromPath[decoder[id]->fPath];
        icd->cacheItemList[cacheKey].isCaching = false;
        icd->cacheItemList[cacheKey].errMsg = decoder[id]->errMsg;
        // qDebug() << "ImageCache::fillCache errMsg" << cacheKey << id << decoder[id]->errMsg;
        icd->cacheItemList[cacheKey].status = static_cast<int>(decoder[id]->status);
        if (decoder[id]->status == ImageDecoder::Status::Video) {
            icd->cacheItemList[cacheKey].isCached = true;
        }
        if (debugCaching)
        {
            qDebug().noquote() << "ImageCache::fillCache"
                               << "     returning decoder" << id
                               << "row =" << cacheKey
                               << "status =" << decoder[id]->status
                               << "status =" << decoder[id]->statusText.at(decoder[id]->status)
                               << "errMsg =" << decoder[id]->errMsg
                               << "isRunning =" << isRunning()
                ;
        }
        // if decoder failed to load an image
        if (decoder[id]->status != ImageDecoder::Status::Success) {
            cacheKey = -1;
        }
    }

    if (G::isLogger) {
        G::log("fillCache  Launch Decoder",
               "Row/cacheKey = " + QString::number(cacheKey) +
               " Decoder = " + QString::number(id) +
               " Status = " + decoder[id]->statusText.at(decoder[id]->status));
    }

    if (debugCaching)
    {
        if (cacheKey == -1) {
            qDebug().noquote() << "ImageCache::fillCache"
                               << "      decoder" << id
                               << "status" << decoder[id]->status
                               << "started (cacheKey == -1)"
                ;
        }
        else {
            QString k = QString::number(cacheKey).leftJustified((4));
            qDebug().noquote() << "ImageCache::fillCache dispatch       "
                               << "decoder" << id
                               << "row =" << k    // row = key
                               << "decoder isRunning =" << decoder[id]->isRunning()
                               << decoder[id]->fPath
                ;
        }
    }

    // add decoded QImage to imCache.
    if (cacheKey != -1 && decoder[id]->status == ImageDecoder::Status::Success) {
        if (debugCaching)
        {
        qDebug().noquote() << "ImageCache::fillCache cache          "
                           << "decoder" <<  QString::number(id).leftJustified(2)
                           << "row =" << QString::number(cacheKey).leftJustified(4)    // row = key
                           << "decoder isRunning =" << decoder[id]->isRunning()
                           << decoder[id]->fPath
            ;
        }
        cacheImage(id, cacheKey);
    }
    // failed to load current image
    else {
        if (decoder[id]->fPath == dm->currentFilePath) {
            emit centralMsg("Unable to load: " + decoder[id]->errMsg + " " + dm->currentFilePath);
        }
    }

    // get next image to cache (nextToCache() defines cache.toCacheKey)
    if (!abort && !cacheUpToDate() && nextToCache(id) && isValidKey(icd->cache.toCacheKey)) {
        if (debugCaching)
        {
        qDebug().noquote() << "ImageCache::fillCache dispatch       "
                           << "decoder" << QString::number(id).leftJustified(2)
                           << "row =" << QString::number(icd->cache.toCacheKey).leftJustified(4)
                           << "decoder status =" << decoder[id]->statusText.at(decoder[id]->status)
                           << decoder[id]->fPath
            ;
        }
        decodeNextImage(id);
    }
    // caching completed
    else {
        decoder[id]->status = ImageDecoder::Status::Ready;
        if (abort || decoder[id]->instance != dm->instance) return;

        // did caching start before cacheItemList was completed?
        if (!isCacheItemListComplete) updateTargets();
        decoder[id]->setReady();

        // if all image decoders except this one are finished then make final check for orphans
        bool allDecodersDone = true;
        for (int d = 0; d < decoderCount; ++d) {
            // if (d == id) continue;
            if (decoder[d]->status != ImageDecoder::Status::Ready) {
                allDecodersDone = false;
                break;
            }
        }

        // if this is the last active decoder, relaunch if cacheUpToDate = false
        if (!isFinalCheckCompleted && allDecodersDone) {
            if (debugCaching)
            {
            qDebug() << "ImageCache::fillCache make final check"
                     << "ImageCache isRunning =" << isRunning()
                     // << "cacheUpToDate() =" << cacheUpToDate()
                ;
            }
            isFinalCheckCompleted = true;
            // resetCachingFlags();
            if (!cacheUpToDate()) {
                // /*
                if (debugCaching)
                {
                qDebug().noquote() << "ImageCache::fillCache Final check allDecodersdone = true"
                                   << "cacheUpToDate = false  launch Decoders"; //*/
                }

                launchDecoders("FinalCheck");
            }
        }

        // Ok, now we are really done
        resetCachingFlags();

        // turn off caching activity lights on statusbar
        emit updateIsRunning(false, true);  // (isRunning, showCacheLabel)
        // update cache progress in status bar
        if (icd->cache.isShowCacheStatus) {
            updateStatus("Update all rows", "ImageCache::fillCache after check cacheUpToDate");
        }
        if (debugCaching)
        {
        if (cacheKey != -1)
            qDebug().noquote()
                << "ImageCache::fillCache finished       "
                << "decoder" <<  QString::number(id).leftJustified(2)
                << "row =" << QString::number(cacheKey).leftJustified(4)
                << "allDecodersDone =" << QVariant(allDecodersDone).toString()
                << "cacheUpToDate = true: decoder set to Ready state";
        }
    }
}

void ImageCache::launchDecoders(QString src)
{
    if (debugCaching || G::isLogger) log("launchDecoders");
    // qDebug() << "ImageCache::launchDecoders src =" << src;

    // if (!isRunning()) {
    //     start(QThread::LowestPriority);
    //     return;
    // }

    for (int id = 0; id < decoderCount; ++id) {
        bool isCacheUpToDate = cacheUpToDate();
        if (debugCaching)
        {
            qDebug()
                << "\nImageCache::launchDecoders"
                << "Decoder =" << id
                << "status =" << decoder[id]->statusText.at(decoder[id]->status)
                << "cacheUpToDate =" << isCacheUpToDate
                ;
        }
        if (isCacheUpToDate) {
            emit updateIsRunning(false, true);  // (isRunning, showCacheLabel)
            break;
        }
        if (decoder[id]->status == ImageDecoder::Status::Ready) {
            decoder[id]->fPath = "";
            if (debugCaching)
            {
                qDebug() << "ImageCache::launchDecoders Decoder =" << id
                         << "fillCache";
            }
            fillCache(id);
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
    if (debugCaching || G::isLogger) log("run");
    if (icd->cacheItemList.length() == 0) {
        return;
    }
    if (debugCaching)
    {
        qDebug().noquote() << "ImageCache::run";
    }

    // check available memory (another app may have used or released some memory)
    memChk();

    // reset target range
    updateTargets();

    // if cache is up-to-date our work is done
    //if (cacheUpToDate()) return;

    // signal MW cache status
    emit updateIsRunning(true, true);   // (isRunning, showCacheLabel)
    launchDecoders("ImageCache::run");
}
