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

    cacheItemList is populated by MetaRead2, which emits a signal to addCacheItemImageMetadata
    for each image as the metadata is read.  Since this is happening concurrently with
    The Decoders generating the images, careful management of mutexes is required.  Potenntial
    orphans are captured at the end of the fillCache function.

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
    if (debugCaching)
        qDebug() << "ImageCache::ImageCache";
    if (debugLog) log("ImageCache");

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
    debugMultiFolders = false;
    debugCaching = false;        // turn on local qDebug
    debugLog = false;            // invoke log
}

ImageCache::~ImageCache()
{
    gMutex.lock();
    //if (debugCaching) qDebug() << "ImageCache::~ImageCache";
    if (debugLog) log("~ImageCache");
    abort = true;
    condition.wakeOne();
    gMutex.unlock();
    wait();
}

void ImageCache::clearImageCache(bool includeList)
{
    //if (debugCaching) qDebug() << "ImageCache::clearImageCache";
    log("ClearImageCache");
    gMutex.lock();
    icd->imCache.clear();
    icd->cache.currMB = 0;
    // do not clear cacheItemList if called from start slideshow
    gMutex.unlock();
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
    if (debugCaching) {
       QString fun = "ImageCache::stop";
        qDebug().noquote()
            << fun.leftJustified(col0Width, ' ')
            << "isRunning =" << isRunning()
            ;
    }
    if (debugLog) log("stop", "isRunning = " + QVariant(isRunning()).toString());
    G::log("ImageCache::stop");

    // stop ImageCache first to prevent more decoder dispatch
    gMutex.lock();
    abort = true;
    gMutex.unlock();

    // stop decoder threads
    for (int id = 0; id < decoderCount; ++id) {
        decoder[id]->stop();
    }

    /* stop imagecache thread if running, which is unlikely as run() initiates the
       fillCache cycle and ends */
    if (isRunning()) {
        gMutex.lock();
        condition.wakeOne();
        gMutex.unlock();
        // wait();
    }

    abort = false;

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

float ImageCache::getImCacheSize()
{
    // return the current size of the cache
    //if (debugCaching) qDebug() << "ImageCache::getImCacheSize";
    if (debugLog) log("getImCacheSize");

    float cacheMB = 0;
    for (int sfRow = 0; sfRow < dm->sf->rowCount(); ++sfRow) {
        if (abort) break;
        if (dm->sf->index(sfRow, G::IsCachedColumn).data().toBool()) {
            cacheMB += dm->sf->index(sfRow, G::CacheSizeColumn).data().toFloat();
        }
    }

    if (debugCaching) {
        QString fun = "ImageCache::getImCacheSize";
        qDebug().noquote()
            << fun.leftJustified(col0Width, ' ')
            << "cache size =" << cacheMB;
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
    if (key > -1 && key < dm->sf->rowCount()) return true;
    else return false;
}

void ImageCache::updateTargets()
{
/*
    Called by setCurrentPosition and fillCache.
*/
    if (G::isFlowLogger)  G::log("ImageCache::updateTargets");
    // log("updateTargets");

    QString fun = "ImageCache::updateTargets";
    if (debugCaching)
    {
        qDebug().noquote() << fun.leftJustified(col0Width, ' ')
                           << "current position =" << icd->cache.key
                           << "total =" << dm->sf->rowCount()
            ;
    }

    // Mutex req'd (do not remove 2023-11-13)
    QMutexLocker locker(&gMutex);

    icd->cache.key = dm->currentSfRow;
    setDirection();
    icd->cache.currMB = getImCacheSize();
    // setPriorities(icd->cache.key);
    setTargetRange(icd->cache.key);
    trimOutsideTargetRange();
}

void ImageCache::resetCacheStateInTargetRange()
{
/*
    Rapidly updating the cache can result in aborted decoder threads that leave
    isCaching and cached states orphaned.  Reset orphan cached state to false in
    the target range.
*/
    QString src = "ImageCache::resetCacheStateInTargetRange";
    if (debugLog) log("resetCacheStateInTargetRange");

    for (int sfRow = icd->cache.targetFirst; sfRow < icd->cache.targetLast; ++sfRow) {
        if (abort) break;
        if (sfRow >= dm->sf->rowCount()) break;

        // isCaching
        if (dm->sf->index(sfRow, G::IsCachingColumn).data().toBool()) {
            dm->setValueSf(dm->sf->index(sfRow, G::IsCachingColumn), false, instance, src);
            dm->setValueSf(dm->sf->index(sfRow, G::DecoderIdColumn), -1, instance, src);
        }

        // in imCache
        if (icd->imCache.contains(dm->sf->index(sfRow, 0).data(G::PathRole).toString())) {
            dm->setValueSf(dm->sf->index(sfRow, G::IsCachedColumn), true, instance, src);
            continue;
        }

        // not in imCache
        else {
            dm->setValueSf(dm->sf->index(sfRow, G::IsCachedColumn), false, instance, src);
            dm->setValueSf(dm->sf->index(sfRow, G::DecoderIdColumn), -1, instance, src);
            dm->setValueSf(dm->sf->index(sfRow, G::DecoderReturnStatusColumn),
                           ImageDecoder::Status::Ready, instance, src);
        }
    }
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
    if (debugLog) log("setDirection");

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

bool ImageCache::updateTarget(int sfRow, bool &isDone)
{

}

void ImageCache::setTargetRange(int key)
{
    QString fun = "ImageCache::setTargetRange";
    if (G::isLogger) G::log(fun, "maxMB = " + QString::number(icd->cache.maxMB));
    if (debugCaching)
    {
        qDebug().noquote() << fun.leftJustified(col0Width, ' ')
        << "current position =" << key
            ;
    }

    int n = dm->sf->rowCount();
    bool isForward = icd->cache.isForward;
    float maxMB = static_cast<float>(icd->cache.maxMB);
    float sumMB = 0;
    float prevMB = 0;
    int aheadAmount = 2;
    int behindAmount = 1;
    int aheadPos = key;
    int behindPos = isForward ? (aheadPos - 1) : (aheadPos + 1);
    bool aheadDone = false;
    bool behindDone = false;
    toCache.clear();

    // Iterate while there is space in the cache
    while (sumMB < maxMB && (!aheadDone || !behindDone)) {
        // Handle "ahead" direction
        for (int a = 0; a < aheadAmount && !aheadDone; ++a) {
            if (isForward ? (aheadPos < n) : (aheadPos >= 0)) {
                bool isVideo = dm->sf->index(aheadPos, G::VideoColumn).data().toBool();
                if (isVideo) continue;
                sumMB +=  dm->sf->index(aheadPos, G::CacheSizeColumn).data().toFloat();
                bool isCached = dm->sf->index(aheadPos, G::IsCachedColumn).data().toBool();
                dm->sf->setData(dm->sf->index(aheadPos, G::IsTargetColumn), true);
                if (!isCached && sumMB < maxMB) {
                    qDebug() << fun << "aheadPos =" << aheadPos;
                    toCache.append(aheadPos);
                    isForward ? icd->cache.targetLast = aheadPos++ : icd->cache.targetFirst = aheadPos--;
                } else aheadDone = true;
            } else aheadDone = true;
        }

        // Handle "behind" direction
        for (int b = 0; b < behindAmount && !behindDone; ++b) {
            if (isForward ? (behindPos >= 0) : (behindPos < n)) {
                bool isVideo = dm->sf->index(behindPos, G::VideoColumn).data().toBool();
                if (isVideo) continue;
                sumMB +=  dm->sf->index(behindPos, G::CacheSizeColumn).data().toFloat();
                bool isCached = dm->sf->index(behindPos, G::IsCachedColumn).data().toBool();
                dm->sf->setData(dm->sf->index(behindPos, G::IsTargetColumn), true);
                if (!isCached && sumMB < maxMB) {
                    qDebug() << fun << "behindPos =" << behindPos;
                    toCache.append(behindPos);
                    isForward ? icd->cache.targetFirst = behindPos-- : icd->cache.targetLast = behindPos++;
                } else  behindDone = true;
            } else  behindDone = true;
        }
    }
}

void ImageCache::trimOutsideTargetRange()
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
    QString src = "ImageCache::trimImCache";
    // iterate imCache
    for (int key = 0; key < icd->imCache.keys().size(); key++) {
        if (abort) break;
        QString fPath = icd->imCache.keys().at(key);

        int sfRow = dm->proxyRowFromPath(fPath); // filter crash
        // fPath not in datamodel if sfRow == -1
        if (sfRow < icd->cache.targetFirst || sfRow > icd->cache.targetLast || sfRow == -1) {
            if (debugCaching)
            {
                QString fun = "ImageCache::setTargetRange outside target range";
                qDebug().noquote()
                    << fun.leftJustified(col0Width, ' ')
                    << "sfRow =" << sfRow
                    << "isCached =" << dm->sf->index(sfRow, G::IsCachedColumn).data().toBool()
                    << "targetFirst =" << icd->cache.targetFirst
                    << "targetLast =" << icd->cache.targetLast
                    << "fPath =" << fPath
                       ;
            }
            icd->imCache.remove(fPath);
            dm->setValueSf(dm->sf->index(sfRow, G::IsCachedColumn), false, instance, src);
            emit updateCacheOnThumbs(fPath, false, "ImageCache::setTargetRange");
        }
    }

    // // trim toCache
    // QMutableListIterator<int> it(toCache);
    // while (it.hasNext()) {
    //     int value = it.next();
    //     if (value < icd->cache.targetFirst || value > icd->cache.targetLast) {
    //         it.remove();
    //     }
    // }
    qDebug() << "toCache:" << toCache;
}

void ImageCache::removeCachedImage(QString fPath)
{
    QString src = "ImageCache::removeCachedImage";
    if (debugLog) log("removeCachedImage", fPath);
    int sfRow = dm->proxyRowFromPath(fPath);
    if (debugCaching)
    {
        QString fun = "ImageCache::removeCachedImage";
        qDebug().noquote()
            << fun.leftJustified(col0Width, ' ')
            << "sfRow =" << sfRow
            << fPath
            ;
    }

    QMutexLocker locker(&gMutex);

    icd->imCache.remove(fPath);
    if (dm->sf->index(sfRow, G::IsCachedColumn).data().toBool()) {
        dm->setValueSf(dm->sf->index(sfRow, G::IsCachedColumn), false, instance, src);
        emit updateCacheOnThumbs(fPath, false, "ImageCache::setTargetRange");
    }
    if (dm->sf->index(sfRow, G::IsCachingColumn).data().toBool()) {
        dm->setValueSf(dm->sf->index(sfRow, G::IsCachingColumn), false, instance, src);
    }
    if (dm->sf->index(sfRow, G::AttemptsColumn).data().toInt()) {
        dm->setValueSf(dm->sf->index(sfRow, G::AttemptsColumn), 0, instance, src);
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
    auto sDebug = [](const QString& sId, const QString& msg) {
        QString s = "";
        qDebug() << "ImageCache::nextToCache" << sId << msg;
    };

    //log("nextToCache", "CacheUpToDate = " + QVariant(cacheUpToDate()).toString());
    bool debugThis = false;
    QString msg;
    QString sId = "id = " + QString::number(id).leftJustified(2);

    if (debugCaching)
    {
        // qDebug() << "ImageCache::nextToCache";
    }

    if (instance != dm->instance) {
        msg = "Instance clash";
        if (debugThis) sDebug(sId, msg);
        return false;
    }

    QMutexLocker locker(&gMutex);

    if (toCache.isEmpty()) return false;

    int sfRow = toCache.at(0);
    QString sRow = QString::number(sfRow).leftJustified(4);
    QString fPath = dm->sf->index(sfRow, 0).data(G::PathRole).toString();

    // out of range
    if (sfRow >= dm->sf->rowCount()) {
        msg = "row = " + sRow + " >= dm->sf->rowCount()";
        if (debugThis) sDebug(sId, msg);
        toCache.takeFirst();
        return false;
    }

    // make sure metadata has been loaded
    if (!dm->sf->index(sfRow, G::MetadataAttemptedColumn).data().toBool()) {
        msg = "row " + sRow + " metadata not attempted";
        if (debugThis) sDebug(sId, msg);
        return false;
    }

    // already in imCache
    if (icd->imCache.contains(fPath)) {
        msg = "row " + sRow + " already in imCache";
        if (debugThis) sDebug(sId, msg);
        toCache.takeFirst();
        return false;
    }

    // max attempts exceeded
    if (dm->sf->index(sfRow, G::AttemptsColumn).data().toInt() > maxAttemptsToCacheImage) {
        msg = "row " + sRow + " maxAttemptsToCacheImage exceeded";
        toCache.takeFirst();
        return false;
    }

    // isCaching and not the same decoder
    if (dm->sf->index(sfRow, G::IsCachingColumn).data().toBool() &&
        (id != dm->sf->index(sfRow, G::DecoderIdColumn).data().toInt()))
    {
        msg = "row " + sRow + " isCaching and not the same decoder";
        if (debugThis) sDebug(sId, msg);
        return false;
    }

    // next item to cache
    msg = "row " + sRow + " success";
    if (debugThis) sDebug(sId, msg);
    icd->cache.toCacheKey = sfRow;
    return true;

    /* OLD PRIORITYLIST VERSION
    if (priorityList.size() > dm->sf->rowCount()) {
        if (debugCaching || debugThis) {
            QString dmInst = QString::number(dm->instance);
            QString inst = QString::number(instance);
            msg = "priorityList is out of date.  dmInstance: " + dmInst + " instance: " + inst + ".";
            if (debugThis) sDebug(sId, msg);
            G::issue("Warning", msg, "ImageCache::nextToCache");
        }
        return false;
    }

    // iterate priority list until find item to cache
    for (int p = 0; p < priorityList.size(); p++) {
        if (abort || G::stop) {
            QString dmInst = QString::number(dm->instance);
            QString inst = QString::number(instance);
            msg = "priorityList is out of date.  dmInstance: " + dmInst + " instance: " + inst + ".";
            if (debugThis) sDebug(sId, msg);
            return false;
        }

        // prevent crash when priority has just updated
        if (p >= priorityList.size()) {
            msg = "p >= priorityList.size()";
            if (debugThis) sDebug(sId, msg);
            return false;
        }

        int sfRow = dm->proxyRowFromPath(priorityList.at(p));
        QString sRow = QString::number(sfRow).leftJustified(4);

        // out of range
        if (sfRow >= dm->sf->rowCount()) {
            msg = "row = " + sRow + " >= dm->sf->rowCount()";
            if (debugThis) sDebug(sId, msg);
            if (debugThis) sDebug(sId, msg);
            return false;
        }

        // make sure metadata has been loaded
        if (!dm->sf->index(sfRow, G::MetadataAttemptedColumn).data().toBool()) {
            msg = "row " + sRow + " metadata not updated";
            if (debugThis) sDebug(sId, msg);
            continue;
        }

        // already in imCache
        if (icd->imCache.contains(priorityList.at(p))) {
            msg = "row " + sRow + " already in imCache";
            if (debugThis) sDebug(sId, msg);
            continue;
        }

        // invalid image
        if (dm->sf->index(sfRow, G::DecoderReturnStatusColumn).data().toInt() == inValidImage) {
            msg = "row " + sRow + " inValidImage";
            if (debugThis) sDebug(sId, msg);
            continue;
        }

        // max attempts exceeded
        if (dm->sf->index(sfRow, G::AttemptsColumn).data().toInt() > maxAttemptsToCacheImage) {
            msg = "row " + sRow + " maxAttemptsToCacheImage exceeded";
            if (debugThis) sDebug(sId, msg);
            continue;
        }

        // isCaching and not the same decoder
        if (dm->sf->index(sfRow, G::IsCachingColumn).data().toBool() &&
            (id != dm->sf->index(sfRow, G::DecoderIdColumn).data().toInt()))
        {
            msg = "row " + sRow + " isCaching and not the same decoder";
            if (debugThis) sDebug(sId, msg);
            continue;
        }

        // next item to cache
        msg = "row " + sRow + " success";
        if (debugThis) sDebug(sId, msg);
        icd->cache.toCacheKey = sfRow;
        return true;
    }

    // nothing found to cache
    if (debugThis) sDebug(sId, msg);
    icd->cache.toCacheKey = -1;
    return false;
    //*/
}

bool ImageCache::isCached(int sfRow)
{
    return dm->sf->index(sfRow, G::IsCachedColumn).data().toBool();
}

bool ImageCache::cacheUpToDate()
{
/*
    Determine if all images in the target range are cached or being cached.
*/
    //log("cacheUpToDate", "Start");

    isCacheUpToDate = true;
    for (int sfRow = icd->cache.targetFirst; sfRow <= icd->cache.targetLast; ++sfRow) {
        if (sfRow >= dm->sf->rowCount()) break;
        if (dm->sf->index(sfRow, G::VideoColumn).data().toBool()) continue;
        if (!dm->sf->index(sfRow, G::IsCachedColumn).data().toBool() &&
            !dm->sf->index(sfRow, G::IsCachingColumn).data().toBool())
        {
            isCacheUpToDate = false;
            break;
        }
    }
    return isCacheUpToDate;

    bool debugThis = false;
    for (int sfRow = icd->cache.targetFirst; sfRow <= icd->cache.targetLast; ++sfRow) {
        if (sfRow >= dm->sf->rowCount()) break;
        // skip videos
        if (dm->sf->index(sfRow, G::VideoColumn).data().toBool()) continue;

        int id = dm->sf->index(sfRow, G::DecoderIdColumn).data().toInt();
        bool isCached = dm->sf->index(sfRow, G::IsCachedColumn).data().toBool();
        bool isCaching = dm->sf->index(sfRow, G::IsCachingColumn).data().toBool();

        // check if caching image is in progress
        if (!isCached && !isCaching) {
            // /*
            log("cacheUpToDate noactivity",
                "row = " + QString::number(sfRow).leftJustified(5) +
                    "row decoder = " + dm->sf->index(sfRow, G::DecoderIdColumn).data().toString().leftJustified(3) +
                    "isCached = " + dm->sf->index(sfRow, G::IsCachedColumn).data().toString().leftJustified(6) +
                    "isCaching = " + dm->sf->index(sfRow, G::IsCachingColumn).data().toString().leftJustified(6) +
                    "attempt = " + dm->sf->index(sfRow, G::AttemptsColumn).data().toString().leftJustified(3)
                );
            //*/
            if (debugThis)
            {
            qDebug().noquote()
                << "ImageCache::cacheUpToDate not caching or cached"
                << "row = " << sfRow
                    << "decoder =" << dm->sf->index(sfRow, G::DecoderIdColumn).data().toInt()
                << "isCached =" << dm->sf->index(sfRow, G::IsCachedColumn).data().toBool()
                << "isCaching =" << dm->sf->index(sfRow, G::IsCachingColumn).data().toBool()
                << "attempts =" << dm->sf->index(sfRow, G::AttemptsColumn).data().toInt();
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

void ImageCache::memChk()
{
/*
    Check to make sure there is still room in system memory (heap) for the image cache. If
    something else (another program) has used the system memory then reduce the size of the
    cache so it still fits.
*/
    if (debugLog) log("memChk");
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
    if (debugLog) log("removeFromCache");

    // Mutex req'd (do not remove 2023-11-13)
    QMutexLocker locker(&gMutex);

    // remove images from imCache
    for (int i = 0; i < pathList.count(); ++i) {
        QString fPathToRemove = pathList.at(i);
        icd->imCache.remove(fPathToRemove);
    }
}

void ImageCache::rename(QString oldPath, QString newPath)
/*
    Called by MW::renameSelectedFiles then RenameFileDlg::renameDatamodel.
*/
{
    if (debugLog) log("rename");
    if (icd->imCache.contains(oldPath)) {
        // CTSL::HashMap<QString, QImage> imCache
        // icd->imCache.rename(oldPath, newPath);

        // QHash<QString, QImage> imCache
        QImage image = icd->imCache.take(oldPath);  // Remove the old key-value pair
        icd->imCache.insert(newPath, image);
    }
    // what about priorityList? rgh
}

void ImageCache::updateStatus(QString instruction, QString source)
/*
    Displays a statusbar showing the image cache status. Also shows the cache size in the info
    panel. All status info is passed by copy to prevent collisions on source data, which is
    being continuously updated by ImageCache.

    Note that the caching progressbar is no longer shown:
    (unless G::showProgress == G::ShowProgress::ImageCache)
*/
{
    emit showCacheStatus(instruction, icd->cache, source);
}

void ImageCache::log(const QString function, const QString comment)
{
    if (debugLog || G::isLogger) {
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
    rpt << reportCacheItemList("");
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
    rpt << "totFiles                 = " << dm->sf->rowCount() << "\n";
    rpt << "currMB                   = " << icd->cache.currMB << "\n";
    rpt << "currMB Check             = " << getImCacheSize() << "\n";
    rpt << "maxMB                    = " << icd->cache.maxMB << "\n";
    rpt << "minMB                    = " << icd->cache.minMB << "\n";
    // rpt << "folderMB                 = " << icd->cache.folderMB << "          (if all metadata loaded)\n";
    rpt << "targetFirst              = " << icd->cache.targetFirst << "\n";
    rpt << "targetLast               = " << icd->cache.targetLast << "\n";
    rpt << "decoderCount             = " << icd->cache.decoderCount << "\n";
    rpt << "\n";
    rpt << "filterOrSortHasChanged   = " << (filterOrSortHasChanged ? "true" : "false") << "\n";
    rpt << "isCacheUpToDate          = " << (isCacheUpToDate ? "true" : "false") << "\n";
    QString s;
    for (const int& i : toBeUpdated) s += QVariant(i).toString() + ", ";
    if (s.length() > 2) s.chop(2);
    else s = "none";
    rpt << "\n";
    rpt << "toBeUpdated              = " << s << "\n";
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
        rpt << QString::number(decoder[id]->sfRow).rightJustified(5);
        rpt << "  ";
        rpt << QString::number(instance).rightJustified(13);
        rpt << "   ";
        rpt << QVariant(decoder[id]->isRunning()).toString();
        rpt << "\n";
    }

    return reportString;
}

QString ImageCache::reportCacheItemList(QString title)
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
    for (int sfRow = 0; sfRow < dm->sf->rowCount(); ++sfRow) {
        // show header every 40 rows
        if (sfRow % 40 == 0) {
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
            rpt.setFieldWidth(40);
            rpt << "File Name";
            rpt.setFieldWidth(1);
            rpt << " ";

            rpt.setFieldWidth(40);
            rpt << "DM File Name";
            rpt.setFieldWidth(1);
            rpt << " ";

            rpt.setFieldWidth(50);
            rpt << "Error message";
            rpt.setFieldWidth(0);
            rpt << "\n";
        }
        rpt.setFieldWidth(9);
        rpt.setFieldAlignment(QTextStream::AlignRight);
        QString fPath = dm->sf->index(sfRow, 0).data(G::PathRole).toString();
        rpt
            << sfRow
            << priorityList.indexOf(fPath)
            << dm->sf->index(sfRow, G::DecoderIdColumn).data().toInt()
            // << (icd->cacheItemList.at(i).isTarget ? "true" : "false")
            << (dm->index(sfRow, G::IsTargetColumn).data().toBool() ? "true" : "false")
            << dm->sf->index(sfRow, G::AttemptsColumn).data().toString()
            << (dm->index(sfRow, G::IsCachingColumn).data().toBool() ? "true" : "false")
            << (dm->index(sfRow, G::IsCachedColumn).data().toBool() ? "true" : "false")
            << rptStatus[dm->sf->index(sfRow, G::DecoderReturnStatusColumn).data().toInt()]
            << (icd->imCache.contains(fPath) ? "true" : "false")
            << (dm->sf->index(sfRow, 0).data(G::UserRoles::CachedRole).toBool() ? "true" : "false")
            << (dm->index(sfRow, G::MetadataAttemptedColumn).data().toBool() ? "true" : "false")
            << (dm->index(sfRow, G::MetadataLoadedColumn).data().toBool() ? "true" : "false")
            << (dm->index(sfRow, G::VideoColumn).data().toBool() ? "true" : "false")
            ;
        rpt.setFieldWidth(11);
        rpt << QString::number(dm->sf->index(sfRow, G::CacheSizeColumn).data().toFloat(), 'f', 3);
        rpt.setFieldWidth(9);
        rpt
            << QString::number(dm->sf->index(sfRow, G::CacheSizeColumn).data().toFloat(), 'f', 3)
            << dm->sf->index(sfRow, G::OffsetFullColumn).data().toInt()
            << dm->sf->index(sfRow, G::LengthFullColumn).data().toInt()
            ;
        rpt.setFieldAlignment(QTextStream::AlignLeft);
        rpt.setFieldWidth(3);
        rpt << "   ";

        rpt.setFieldWidth(40);
        rpt << Utilities::getFileName(fPath);
        rpt.setFieldWidth(1);
        rpt << " ";

        rpt << "\n";
        if (dm->sf->index(sfRow, G::IsCachedColumn).data().toBool()) cachedCount++;
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
    for (int i = 0; i < toCache.size(); ++i) {
        int sfRow = toCache.at(i);
        rpt.setFieldWidth(9);
        rpt.setFieldAlignment(QTextStream::AlignRight);
        rpt << i << sfRow;
        rpt.setFieldWidth(3);
        rpt << "   ";
        rpt.setFieldAlignment(QTextStream::AlignLeft);
        rpt.setFieldWidth(50);
        rpt << dm->sf->index(sfRow, 0).data(G::PathRole).toString();
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

    // check when imCache is empty
    QImage image;
    int mem = 0;

    // CTSL::HashMap<QString, QImage> imCache
    // QVector<QString> keys;
    // icd->imCache.getKeys(keys);
    // if (keys.size() == 0) {
    //     rpt << "\nicd->imCache is empty";
    //     return reportString;
    // }

    // QHash<QString, QImage> imCache
    QStringList keys = icd->imCache.keys();
    if (keys.size() == 0) {
        rpt << "\nicd->imCache is empty";
        return reportString;
    }

    // build list of report items
    struct ImRptItem {
        int hashKey;
        int sfRow;
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
        imRptItem.sfRow = dm->proxyRowFromPath(imRptItem.fPath);
        imRptItem.priorityKey = toCache.indexOf(imRptItem.sfRow);
        image = icd->imCache.value(keys.at(i));    // QHash<QString, QImage> imCache
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
        rpt << rptList.at(item).sfRow;
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
             ;
}

void ImageCache::initImageCache(int &cacheMaxMB,
                                int &cacheMinMB,
                                bool &isShowCacheStatus,
                                int &cacheWtAhead)
{
    log("initImageCache");
    if (!G::useImageCache) return;   // rgh isolate image cache

    // prevent
    // isInitializing = true;

    QString fun = "ImageCache::initImageCache";
    if (debugCaching) {
        qDebug().noquote()
            << fun.leftJustified(col0Width, ' ')
            << "dm->instance =" << dm->instance
            ;
    }

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

    isInitializing = false;

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
    gMutex.lock();
    icd->cache.maxMB = cacheSizeMB;
    icd->cache.minMB = cacheMinMB;
    icd->cache.isShowCacheStatus = isShowCacheStatus;
    icd->cache.wtAhead = cacheWtAhead;
    gMutex.unlock();
}

void ImageCache::rebuildImageCacheParameters(QString &currentImageFullPath, QString source)
{
/*
    When the datamodel is filtered the image cache needs to be updated. The cacheItemList is
    rebuilt for the filtered dataset and isCached updated, the current image is set, and any
    surplus cached images (not in the filtered dataset) are removed from imCache.
    The image cache is now ready to run by calling setCachePosition().
*/
    QString src = "ImageCache::rebuildImageCacheParameters";
    QString fun = "ImageCache::rebuildImageCacheParameters";
    log("rebuildImageCacheParameters");
    if (debugCaching)
    {
        qDebug().noquote()
            << fun.leftJustified(col0Width, ' ')
            << "dm->sf->rowCount() =" << dm->sf->rowCount()
            << "currentImageFullPath =" << currentImageFullPath
            ;
    }
    if (dm->sf->rowCount() == 0) return;

    // do not use mutex - spins forever

    // update instance
    instance = dm->instance;

    // update cacheItemList
    icd->cache.key = 0;
    if (icd->cache.isShowCacheStatus)
        updateStatus("Update all rows", fun);

    setCurrentPosition(currentImageFullPath, fun);
}

void ImageCache::refreshImageCache()
{
/*
    Reload all images in the cache.
*/
    log("refreshImageCache");
    if (debugCaching)
        qDebug() << "ImageCache::refreshImageCache";

    gMutex.lock();
    icd->imCache.clear();
    gMutex.unlock();

    if (isRunning()) {
        stop();
    }
    start(QThread::LowestPriority);
}

void ImageCache::colorManageChange()
{
/*
    Called when color manage is toggled.  Reload all images in the cache.
*/
    if (debugCaching) qDebug() << "ImageCache::colorManageChange";
    log("colorManageChange");
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

    if (isRunning()) {
        stop();
    }
    start(QThread::LowestPriority);
}

void ImageCache::datamodelFolderCountChange(QString src)
{
    log("cacheSizeChange");

    // setTargetRange();
    if (isRunning()) {
        stop();
    }
    start(QThread::LowestPriority);
}

void ImageCache::setCurrentPosition(QString fPath, QString src)
{
/*
    Called from MW::fileSelectionChange to reset the position in the image cache. The image
    cache direction, priorities and target are reset and the cache is updated in fillCache.
*/
    int sfRow = dm->currentSfRow;
    log("ImageCache::setCurrentPosition", "row = " + QString::number(sfRow));
    // log("setCurrentPosition", "row = " + QString::number(sfRow));
    if (debugCaching) {
        QString fun = "ImageCache::decodeNextImage";
        qDebug().noquote() << fun.leftJustified(col0Width, ' ')
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

    if (isInitializing) {
        qDebug() << "ImageCache::setCurrentPosition Initializing";
        return;
    }

    // range check
    if (dm->currentSfRow >= dm->sf->rowCount()) {
        QString msg = "dm->currentSfRow is out of range.";
        int i = dm->currentSfRow;
        G::issue("Warning", msg, "ImageCache::setCurrentPosition", i, fPath);
        qWarning() << "WARNING ImageCache::setCurrentPosition dm->currentSfRow ="
                   << dm->currentSfRow << "is out of range";
        return;
    }

    // Do not use QMutexLocker. Can cause deadlock in updateTargets()

    // reset decoders status if necessary
    for (int id = 0; id < decoderCount; ++id) {
        ImageDecoder *d = decoder[id];
        if (!d->isRunning() && d->status != ImageDecoder::Status::Ready) {
            d->setReady();
        }
    }

    gMutex.lock();
    icd->cache.key = dm->currentSfRow;
    gMutex.unlock();

    // image not cached and not video
    bool isVideo = dm->sf->index(sfRow, G::VideoColumn).data().toBool();

    // not in cache, maybe loading
    if (!icd->imCache.contains(fPath) && !isVideo) {
        if (G::mode == "Loupe") emit centralMsg("Loading Image...");
        if (dm->currentSfRow > 0) {
            QString msg = "Not in imCache, might be loading.";
            G::issue("Warning", msg, "ImageCache::setCurrentPosition", sfRow, fPath);
        }
    }
    // or could not load
    for (int i = 0; i < dm->sf->rowCount(); ++i) {
        if (dm->sf->index(i,0).data(G::PathRole).toString() == fPath) {
            if (dm->sf->index(sfRow, G::DecoderReturnStatusColumn).data().toInt() == ImageDecoder::Status::Invalid) {
                QString errMsg = dm->sf->index(sfRow, G::DecoderErrMsgColumn).data().toString();
                emit centralMsg("Unable to load: " + errMsg + " " +fPath);
                QString msg = "Invalid status, unable to load.";
                G::issue("Warning", msg, "ImageCache::setCurrentPosition", i, fPath);
            }
        }
    }

    if (isRunning()) {
        // reset target range
        updateTargets();
    }
    else {
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
    QString src = "ImageCache::decodeNextImage";
    log ("decodeNextImage");

    QMutexLocker locker(&gMutex);

    int sfRow = icd->cache.toCacheKey;
    if (!isValidKey(sfRow)) {
        decoder[id]->status = ImageDecoder::Status::Ready;
        return;
    }

    dm->setValueSf(dm->sf->index(sfRow, G::IsCachingColumn), true, instance, src);
    dm->setValueSf(dm->sf->index(sfRow, G::DecoderIdColumn), id, instance, src);
    int attempts = dm->sf->index(sfRow, G::AttemptsColumn).data().toInt();
    dm->setValueSf(dm->sf->index(sfRow, G::AttemptsColumn), ++attempts, instance, src);
    /*
    log("decodeNextImage",
        "decoder " + QString::number(id).leftJustified(3) +
        "row = " + QString::number(icd->cache.toCacheKey).leftJustified(5) +
        "row decoder = " + dm->sf->index(sfRow, G::DecoderIdColumn).data().toString().leftJustified(3) +
        "isCached = " + dm->sf->index(sfRow, G::IsCachedColumn).data().toString().leftJustified(6) +
        "isCaching = " + dm->sf->index(sfRow, G::IsCachingColumn).data().toString().leftJustified(6) +
        "attempt = " + dm->sf->index(sfRow, G::AttemptsColumn).data().toString().leftJustified(3) +
        "isMetadata = " + dm->sf->index(sfRow, G::MetadataLoadedColumn).data().toString().leftJustified(6) +
        "status = " + dm->sf->index(sfRow,G::DecoderReturnStatusColumn).data().toString()
        );
    */
    if (debugCaching)
    {
        QString fun = "ImageCache::decodeNextImage";
        qDebug().noquote()
            << fun.leftJustified(col0Width, ' ')
            << "decoder" << QString::number(id).leftJustified(2)
            << "sfRow =" << sfRow
            << "threadId =" << dm->sf->index(sfRow, G::DecoderIdColumn).data().toInt()
            << "isMetadata =" << dm->sf->index(sfRow, G::MetadataLoadedColumn).data().toBool()
            << "isCaching =" << dm->sf->index(sfRow, G::IsCachingColumn).data().toBool()
            << "isCached =" << dm->sf->index(sfRow, G::IsCachedColumn).data().toBool()
            << "attempts =" << dm->sf->index(sfRow, G::AttemptsColumn).data().toInt()
            << "status =" << dm->sf->index(sfRow,G::DecoderReturnStatusColumn).data().toString()
            << dm->sf->index(sfRow,0).data(G::PathRole).toString()
            ;
    }

    if (!abort) decoder[id]->decode(sfRow, instance);
}

void ImageCache::cacheImage(int id, int cacheKey)
{
/*
    Called from fillCache to insert a QImage that has been decoded into icd->imCache.
    Do not cache video files, but do show them as cached for the cache status display.
*/
    QString src = "ImageCache::cacheImage";
    QString comment = "decoder " + QString::number(id).leftJustified(3) +
                      "row = " + QString::number(cacheKey).leftJustified(5) +
                      "decoder[id]->fPath =" + decoder[id]->fPath;
    if (G::isLogger || G::isFlowLogger) log("cacheImage", comment);
    if (debugCaching)
    {
        QString k = QString::number(cacheKey).leftJustified((4));
        QString fun = "ImageCache::cacheImage";
        qDebug().noquote()
               << fun.leftJustified(col0Width, ' ')
               << "decoder" << QString::number(id).leftJustified(2)
               << "row =" << k
               << "G::mode =" << G::mode
               << "errMsg =" << decoder[id]->errMsg
               << "decoder[id]->fPath =" << decoder[id]->fPath
               //<< "dm->currentFilePath =" << dm->currentFilePath
    ;
    }

    gMutex.lock();

    // /* Safety check do not exceed max cache size.  The metadata determination of the image size
    //    could be wrong, resulting in icd->cacheItem.sizeMB being wrong.  If this is the case, then
    //    the target range will be wrong too.  Changed to issue a warning for now.

    //    Do not use for now */
    // /*
    // if ((icd->cache.currMB + icd->cacheItemList[cacheKey].estSizeMB) > icd->cache.maxMB) {
    //     if (debugCaching)
    //     {
    //         QString estMB =  QString::number(icd->cache.currMB + icd->cacheItemList[cacheKey].estSizeMB);
    //         QString maxMB =  QString::number(icd->cache.maxMB);
    //         QString msg = "Estimated cache ." + estMB + " MB exceeds the maximum " + maxMB + " MB.";
    //         G::issue("Warning", msg, "ImageCache::cacheImage", cacheKey, decoder[id]->fPath);
    //         qDebug() << comment << msg;
    //     }
    // } //*/

    // cache the image if not a video
    if (decoder[id]->status != ImageDecoder::Status::Video) {
        // Check if initial sizeMB was estimated (image without preview metadata ie PNG)
        // if (icd->cacheItemList[cacheKey].estSizeMB) setSizeMB(id, cacheKey);
        bool isImage = decoder[id]->image.width() > 0;
        if (!isImage) {
            QString msg = "Decoder returned a null image.";
            G::issue("Warning", msg, "ImageCache::cacheImage", cacheKey, decoder[id]->fPath);
        }
        icd->imCache.insert(decoder[id]->fPath, decoder[id]->image);
        if (toCache.contains(cacheKey)) toCache.remove(toCache.indexOf(cacheKey));
    }

    icd->cache.currMB = getImCacheSize();
    dm->setValueSf(dm->sf->index(cacheKey, G::IsCachingColumn), false, instance, src);
    dm->setValueSf(dm->sf->index(cacheKey, G::IsCachedColumn), true, instance, src);
    gMutex.unlock();

    QString errMsg = dm->sf->index(cacheKey, G::DecoderErrMsgColumn).data().toString();

    // set datamodel isCached = true
    emit setValuePath(decoder[id]->fPath, 0, true, instance, G::CachedRole);
    if (decoder[id]->status != ImageDecoder::Status::Video) {
        // if current image signal ImageView::loadImage
        if (decoder[id]->fPath == dm->currentFilePath) {
            // if (G::mode == "Loupe") {
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
            // }
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
        - cacheUpToDate (checks for orphans and item list completed)
          - no
            - restart
          - yes
            - done

*/

    int cacheKey = -1;       // row for image in cacheKeyHash (default to no key)
    QString src = "";

    if (debugCaching)
    {
        QString fun = "ImageCache::fillCache starting";
        qDebug().noquote() << fun.leftJustified(col0Width, ' ')
                           << "decoder" << QString::number(id).leftJustified(2)
            ;
    }

    // check if aborted
    if (abort || decoder[id]->status == ImageDecoder::Status::Abort) {
        decoder[id]->status = ImageDecoder::Status::Ready;
        return;
    }

    // new decoder just getting started
    if (decoder[id]->status == ImageDecoder::Status::Ready) {
        if (debugCaching)
        {
            QString fun = "ImageCache::fillCache starting";
            qDebug().noquote() << fun.leftJustified(col0Width, ' ')
                               << "decoder" << QString::number(id).leftJustified(2)
                               << "status = Ready"
                ;
        }
        cacheKey = -1;
    }
    // returning instance clash
    else if (decoder[id]->status == ImageDecoder::Status::InstanceClash) {
        if (debugCaching)
        {
            QString fun = "ImageCache::fillCache";
            qDebug().noquote() << fun.leftJustified(col0Width, ' ')
                               << "returning decoder" << id
                               << "instance clash!"
                ;
        }
            cacheKey = -1;
    }
    // returning decoder
    else {
        cacheKey = decoder[id]->sfRow;
        // cacheKey = dm->proxyRowFromPath(decoder[id]->fPath);
        dm->setValueSf(dm->sf->index(cacheKey, G::IsCachingColumn), false, instance, src);
        // more direct version
        // dm->sf->setData(dm->sf->index(cacheKey, G::IsCachingColumn), false);

        // qDebug() << "ImageCache::fillCache errMsg" << cacheKey << id << decoder[id]->errMsg;
        // dm->sf->setData(dm->sf->index(cacheKey, G::DecoderReturnStatusColumn),
        //                 static_cast<int>(decoder[id]->status));
        dm->setValueSf(dm->sf->index(cacheKey, G::DecoderReturnStatusColumn),
                       static_cast<int>(decoder[id]->status), instance, src);
        if (decoder[id]->status == ImageDecoder::Status::Video) {
            dm->setValueSf(dm->sf->index(cacheKey, G::IsCachedColumn), true, instance, src);
        }
        if (decoder[id]->errMsg != "") {
            dm->setValueSf(dm->sf->index(cacheKey, G::DecoderErrMsgColumn), decoder[id]->errMsg, instance, src);
        }
        if (debugCaching)
        {
            QString fun = "ImageCache::fillCache";
            qDebug().noquote() << fun.leftJustified(col0Width, ' ')
                               << "returning decoder" << id
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
            QString fun = "ImageCache::fillCache";
            qDebug().noquote() << fun.leftJustified(col0Width, ' ')
                               << "decoder" << QString::number(id).leftJustified(2)
                               << "status" << decoder[id]->status
                               << "started (cacheKey == -1)"
                ;
        }
        else {
            QString k = QString::number(cacheKey).leftJustified((4));
            QString fun = "ImageCache::fillCache dispatch";
            qDebug().noquote() << fun.leftJustified(col0Width, ' ')
                               << "decoder" << QString::number(id).leftJustified(2)
                               << "row =" << k    // row = key
                               << "decoder isRunning =" << decoder[id]->isRunning()
                               << decoder[id]->fPath
                ;
        }
    }

    // returning decoder add decoded QImage to imCache.
    if (cacheKey != -1) {
        if (decoder[id]->status == ImageDecoder::Status::Success) {
            if (debugCaching)
            {
                QString fun = "ImageCache::fillCache cache";
                qDebug().noquote() << fun.leftJustified(col0Width, ' ')
                               << "decoder" << QString::number(id).leftJustified(2)
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
    }

    // get next image to cache (nextToCache() defines cache.toCacheKey)
    bool isCacheUpToDate = cacheUpToDate();
    bool isNextToCache = nextToCache(id);
    bool isCacheKeyOk = isValidKey(icd->cache.toCacheKey);
    bool okDecodeNextImage = !abort && !isCacheUpToDate && isNextToCache && isCacheKeyOk;

    if (debugCaching)
    {
        QString fun = "ImageCache::fillCache dispatch";
        qDebug().noquote() << fun.leftJustified(col0Width, ' ')
                       << "decoder" << QString::number(id).leftJustified(2)
                       << "row =" << QString::number(icd->cache.toCacheKey).leftJustified(4)
                       << "decoder status =" << decoder[id]->statusText.at(decoder[id]->status)
                       << "abort =" << abort
                       << "isCacheUpToDate =" << isCacheUpToDate
                       << "isNextToCache =" << isNextToCache
                       << "isValidKey(icd->cache.toCacheKey) =" << isValidKey(icd->cache.toCacheKey)
                       << "okDecodeNextImage =" << okDecodeNextImage
                          ;
    }

    // decode the next image in the target range
    if (okDecodeNextImage) {
        if (debugCaching)
        {
            QString fun = "ImageCache::fillCache okDecodeNextImage";
            qDebug().noquote()
                           << fun.leftJustified(col0Width, ' ')
                           << "decoder" << QString::number(id).leftJustified(2)
                           << "row =" << QString::number(icd->cache.toCacheKey).leftJustified(4)
                           << "decoder status =" << decoder[id]->statusText.at(decoder[id]->status)
                           << decoder[id]->fPath
            ;
        }
        decodeNextImage(id);
    }

    // else caching available targets completed
    else {
        decoder[id]->status = ImageDecoder::Status::Ready;

        // quit if abort or new folder selected
        if (abort || decoder[id]->instance != dm->instance) return;

        // did caching start before cacheItemList was completed?
        // if (!cacheItemListCompleted()) updateTargets();  // removed, ok? rgh

        // is this the last active decoder?       make final check for orphans
        bool allDecodersDone = allDecodersReady();

        /* Both MetaRead2 and ImageCache are running at the same time, and sometimes
        ImageCache catches up to MetaRead2 and the datamodel has not been loaded/updated
        for the next item to cache. For MetaRead2 updates to be executed ImageCache must
        be stopped as ImageCache is blocking the updates. */

        // if this is the last active decoder, restart if cacheUpToDate == false
        if (!abort && instance == dm->instance && allDecodersDone && retry < 5) {
            if (debugCaching)
            {
                QString fun = "ImageCache::fillCache make final check";
                qDebug().noquote()
                         << fun.leftJustified(col0Width, ' ')
                         << "retry =" << retry
                         << "ImageCache isRunning =" << isRunning()
                         << "dm->sf->rowCount() =" << dm->sf->rowCount()
                         << "cacheUpToDate() =" << cacheUpToDate()
                    ;
            }
            resetCacheStateInTargetRange();
            if (!cacheUpToDate() && retry++ < 5) {
                if (debugCaching)
                {
                    QString fun = "ImageCache::fillCache Final check";
                    qDebug().noquote()
                        << fun.leftJustified(col0Width, ' ')
                        << "retry =" << retry
                        // << "allDecodersdone = true"
                        << "cacheUpToDate = false  restart ImageCache";
                }
                stop();
                start();
                // return;
            }
            else {
                if (retry) retry = 0;
            }
        }

        // Ok, now we are really done or quit after retrying

        // turn off caching activity lights on statusbar
        emit updateIsRunning(false, true);  // (isRunning, showCacheLabel)

        // update cache progress in status bar
        if (icd->cache.isShowCacheStatus) {
            updateStatus("Update all rows", "ImageCache::fillCache after check cacheUpToDate");
        }

        if (debugCaching)
        {
            QString fun = "ImageCache::fillCache Finished";
            if (cacheKey != -1)
                qDebug().noquote()
                    << fun.leftJustified(col0Width, ' ')
                    << "decoder" <<  QString::number(id).leftJustified(2)
                    << "row =" << QString::number(cacheKey).leftJustified(4)
                    << "allDecodersDone =" << QVariant(allDecodersDone).toString()
                    << "cacheUpToDate = true: decoder set to Ready state";
        }
    }
}

void ImageCache::launchDecoders(QString src)
{
    // qDebug() << "ImageCache::launchDecoders src =" << src;

    // if (!isRunning()) {
    //     start(QThread::LowestPriority);
    //     return;
    // }

    for (int id = 0; id < decoderCount; ++id) {
        bool isCacheUpToDate = cacheUpToDate();
        if (id >= dm->sf->rowCount()) return;

        if (debugLog || G::isLogger) {
            QString msg = "id = " + QString::number(id).leftJustified(2) +
                          "cacheUpToDate = " + QVariant(isCacheUpToDate).toString() +
                          " status = " + decoder[id]->statusText.at(decoder[id]->status)
                ;
            log("launchDecoders", msg);
        }
        // if (debugCaching)
        // {
        //     qDebug() << " ";
        //     QString fun = "ImageCache::launchDecoders";
        //     qDebug().noquote()
        //         << fun.leftJustified(col0Width, ' ')
        //         << "Decoder =" << id
        //         << "status =" << decoder[id]->statusText.at(decoder[id]->status)
        //         << "cacheUpToDate =" << isCacheUpToDate
        //         << "isRunning =" << isRunning()
        //         ;
        // }
        if (isCacheUpToDate) {
            emit updateIsRunning(false, true);  // (isRunning, showCacheLabel)
            break;
        }
        if (decoder[id]->status == ImageDecoder::Status::Ready) {
            decoder[id]->fPath = "";
            if (debugLog)
            {
                QString msg = "id = " + QString::number(id).leftJustified(2) +
                              "cacheUpToDate = " + QVariant(isCacheUpToDate).toString() +
                              " status = Ready, calling fillCache"
                              ;
                log("launchDecoders", msg);
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

    // req'd?
    if (dm->sf->rowCount() == 0) {
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
