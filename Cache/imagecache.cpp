#include "Cache/imagecache.h"
#include "Main/global.h"

/*  How the Image Cache works:

            CCC         CCCCCCCCCC
                  TTTTTTTTTTTTTTTTTTTTTTTTTTTTT
    .......................*...........................................

    . = all the thumbnails in dm->sf (rows in filtered datamodel)
    * = the current thumbnail selected
    T = all the images targeted to cache.  The sum of T fills the assigned
        memory available for the cache ie 3000 MB
    C = the images currently cached, which can be fragmented if the user
        is jumping around, selecting images willy-nilly

Data structures:

    The image data structures are in a separate class ImageCacheData to facilitate
    concurrent data access. The image cache for QImages reside in the hash table imCache.
    This table is written to by cacheImage() and read by ImageView.  imCache is in a
    separate class ImageCacheData, in an instance pointed to by *icd.  This is likely
    unnecessary but has not been refactored.

    The list toCache and hash toCacheStatus keep track of the datamodel rows to be cached,
    based on additions in the target range, and removals when cached.

Procedure every time a new thumbnail is selected:

    1. Determine the direction of travel.  In this example we are moving forward.

    2. Based on the target weighting caching strategy ahead and behind, set the target
    range. In the illustration we are caching 2 ahead for every 1 behind.  The target
    range is complete when there isn't any more room in the image cache.

    3. Add each row in the target range to the list toCache if not already cached, and is
    not in toCache.  Remove any images outside the target range from the image cache.

    4. Add images that are targeted and not already cached. The targeted images are read
    by multiple decoders, which signal fillCache with a QImage.

Building the cache:

    A number of ImageDecoders are created when ImageCache is created. Each ImageDecoder
    runs in a separate thread. The decoders convert an image file into a QImage.

    The ImageDecoders are launched from ImageCache::launchDecoders, return a QImage back
    to fillCache, the QImage is inserted into imCache and then are dispatched again.

    To return the QImage to fillCache, the ImageDecoders, each in their own thread,
    signal ImageCache::fillCache for each QImage. They form queued connections so that
    only one decoder at a time can add images to imCache.

    The caching process uses the toCache list, toCacheStatus hash, imCache size, and
    datamodel fields to manage the priorities and target range.

    The ImageDecoder has a status attribute that can be Ready, Busy or Done. When the
    decoder is created and when the QImage has been inserted into the image cache the
    status is set to Ready. When the decoder is dispatched from fillCache the status is
    set to Busy. Finally, when the decoder finishes the decoding in ImageDecoder::run the
    status is set to Done. Each decoder signals ImageCache::fillCache when the image has
    been converted into a QImage. Back in fillCache the QImage is added to the imCache.
    If there are more targeted images to be cached, the next one is assigned to the
    decoder, which is run again. The decoders keep running until all the targeted images
    have been cached.

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

    Every time a new image is selected, setCurrentPosition is called. The target range is
    updated and launchDecoders calls fillCache for each ImageDecoder. When the
    ImageDecoder has loaded a QImage, the QImage it is passed back to fillCache and added
    to the ImCache hash.

    Each decoder follows this basic pattern:
    - launch
      - nextToCache
      - decodeNextImage
      - ImageDecoder::decode
      - fillCache
        - cacheImage
        - nextToCache
        - decodeNextImage
        - ImageDecoder::decode
        - loop fillCache

Image size in cache:

    The QImage MB memory (mb) required to cache an image is first estimated in the
    datamodel when the file data is loaded, and replaced by MetaRead when the metadata is
    read if it is available. The mb for each image is required to set the target range
    when building the ImageCache. Since MetaRead and ImageCache can be loading
    simultaneously, the ImageCache setTargetRange function might need the mb before
    MetaRead has loaded, and uses the estimate.

    The concurrent operations make it risky to directly call the datamodel cache size
    form ImageCache, which is in a separate thread.  Use QMetaObject::invokeMethod.

DataModel instance changes:

    The datamodel instance is incremented every time MW::filderSelectionChange is
    invoked and there are additions or removals to the datamodel.

        If there is a reset declared in MW::filderSelectionChange then
        ImageCache::initialize is called and the local instance is updated.

        If there is a datamodel filterChange then ImageCache::filterChange is called
        and the local instance is updated.

        If updateToCache() is invoked and the local instance does not match the
        datamodel instance then setTargetRange clears the toCache/toCacheStatus lists
        and the local instace is updated in updateToCache().

*/


ImageCache::ImageCache(QObject *parent,
                       ImageCacheData *icd,
                       DataModel *dm)
    : QObject(nullptr)
{
    if (debugCaching)
    {
        qDebug() << "ImageCache::ImageCache";
    }
    if (debugLog || G::isLogger) log("ImageCache");

    moveToThread(&imageCacheThread);

    // Signal to ImageCache when datamodel loaded for some row
    connect(dm, &DataModel::rowLoaded, this, [&] { condition.wakeAll(); });

    // data is kept in ImageCacheData icd, a hash table
    this->icd = icd;
    this->dm = dm;
    // new metadata to avoid thread collisions?
    metadata = new Metadata;

    maxMB = 1000;  // maxMB change

    // create n decoder threads
    decoderCount = QThread::idealThreadCount();
    for (int id = 0; id < decoderCount; ++id) {
        ImageDecoder *decoder = new ImageDecoder(id, dm, metadata);
        QThread *thread = new QThread;

        decoder->moveToThread(thread);
        connect(decoder, &ImageDecoder::done, this, &ImageCache::fillCache);
        // connect(this, &ImageCache::decode, decoder, &ImageDecoder::decode);

        thread->start();
        decoders.append(decoder);
        decoderThreads.append(thread);
        cycling.append(false);
    }

    pressureHistory.reserve(50);    // avoid reallocations

    abort = false;
    debugCaching = false;        // turn on local qDebug
    debugLog = false;            // invoke log without G::isLogger or G::isFlowLogger
}

ImageCache::~ImageCache()
{
    stop();
}

void ImageCache::start()
{
    abort = false;
    if (!imageCacheThread.isRunning()) {
        imageCacheThread.start();
    }
}

void ImageCache::stop()
{
/*
    Called when quitting Winnow.
*/
    QString fun = "ImageCache::stop";
    if (debugCaching)
    {
        qDebug().noquote()
            << fun.leftJustified(col0Width, ' ')
            << "isRunning =" << imageCacheThread.isRunning()
            ;
    }
    if (debugLog || G::isLogger)
        log("stop", "isRunning = " + QVariant(imageCacheThread.isRunning()).toString());

    abort = true;

    // Stop all decoder threads first
    for (int id = 0; id < decoderCount; ++id) {
        QMetaObject::invokeMethod(decoders[id], "stop", Qt::BlockingQueuedConnection);
        // decoders[id]->stop();
    }

    if (imageCacheThread.isRunning()) {
        imageCacheThread.quit();
        imageCacheThread.wait();
    }

    // turn off caching activity lights on statusbar
    // emit updateIsRunning(false, false);  // flags = isRunning, showCacheLabel
}

bool ImageCache::instanceClash(bool id)
{
    return decoders[id]->instance != G::dmInstance;
}

void ImageCache::abortProcessing()
{
    // if (G::isLogger || G::isFlowLogger)
    QString isGUI = QVariant(G::isGuiThread()).toString();
    G::log("ImageCache::abortProcessin",
           "starting, isGUI thread = " + isGUI);

    abort = true;
    for (int id = 0; id < decoderCount; ++id) {
        cycling[id] = false;
        decoders[id]->abortProcessing();
        // QMetaObject::invokeMethod(decoders[id], "abortProcessing", Qt::QueuedConnection);
        // QMetaObject::invokeMethod(decoders[id], "abortProcessing", Qt::BlockingQueuedConnection);
    }

    // if (G::isLogger || G::isFlowLogger)
    G::log("ImageCache::abortProcessing", "emit stopped");

    emit stopped("ImageCache");
}

bool ImageCache::isIdle()
{
    for (int id = 0; id < decoderCount; ++id) {
        if (decoders[id]->isBusy()) return false;
    }
    return true;
}

bool ImageCache::isRunning() const
{
    // rgh should we use QAtomicInt running
    return imageCacheThread.isRunning();
}

bool ImageCache::anyDecoderCycling()
{
    for (int id = 0; id < decoderCount; ++id) {
        if (cycling.at(id)) return true;
    }
    return false;
}

quint64 ImageCache::getImCacheSize()
{
    // return the current size of the cache
    if (debugLog || G::isLogger) log("getImCacheSize");

    quint64 cacheMB = 0;

    for (auto it = icd->imCache.constBegin(); it != icd->imCache.constEnd(); ++it) {
        cacheMB += static_cast<quint64>(it.value().sizeInBytes()) / (1 << 20);
    }

    if (debugCaching)
    {
        QString fun = "ImageCache::getImCacheSize";
        qDebug().noquote()
            << fun.leftJustified(col0Width, ' ')
            << "cache size =" << cacheMB;
    }

    return cacheMB;
}

bool ImageCache::isValidKey(int key)
{
    if (key > -1 && key < dm->sf->rowCount()) return true;
    else return false;
}

bool ImageCache::waitForMetaRead(int sfRow, int ms)
{
    QString fun = "ImageCache::waitForMetaRead";
    if (G::allMetadataLoaded) {
        if (debugCaching)
        {
        qDebug().noquote() << fun.leftJustified(col0Width, ' ')
                           << "row =" << sfRow
                           << "  G::allMetadataLoaded = true";
        }
        return true;
    }
    if (dm->sf->index(sfRow, G::MetadataAttemptedColumn).data().toBool()) {
        if (debugCaching)
        {
            qDebug().noquote() << fun.leftJustified(col0Width, ' ')
            << "row =" << sfRow
            << "  isLoaded = true";
        }
        return true;
    }

    bool isLoaded = false;

    QElapsedTimer t;
    t.start();

    // signal DataModel::imageCacheWaiting
    emit waitingForRow(sfRow);

    while(!isLoaded) {
        if (!condition.wait(&gMutex, ms - t.elapsed())) break;
        isLoaded = dm->sf->index(sfRow, G::MetadataAttemptedColumn).data().toBool();
        if (isLoaded) break;
    }

    if (debugCaching)
    {
        fun += " wait";
        qDebug().noquote() << fun.leftJustified(col0Width, ' ')
                           << "row =" << sfRow
                           << "  isLoaded =" << isLoaded
                           << "  t.elapsed() =" << t.elapsed()
            ;
    }

    return isLoaded;
}

void ImageCache::updateToCache()
{
/*
    Called by setCurrentPosition() if isRunning().
    Called by run() if was not isRunning().
    In either case, this is running in the ImageCache thread.
*/
    if (debugLog || G::isLogger || G::isFlowLogger)
        log("updateToCache");

    QString fun = "ImageCache::updateToCache";
    if (debugCaching)
    {
        qDebug().noquote() << fun.leftJustified(col0Width, ' ')
                           << "current position =" << currRow
                           << "total =" << dm->sf->rowCount()
            ;
    }

    QMutexLocker locker(&gMutex);  // req'd to prevent toCacheAppend() crash

    if (!abort) setDirection();
    // prevent additions or deletions to model
    if (!abort) setTargetRange(currRow);
    if (!abort) trimOutsideTargetRange();

    // // current position in target range
    // float currPosPct;
    // int targetRange = targetLast - targetFirst;
    // if (isForward) {
    //     currPosPct = static_cast<float>(targetLast - currRow) / targetRange;
    // }
    // else {
    //     currPosPct = static_cast<float>(currRow - targetFirst) / targetRange;
    // }
    // qDebug() << "ImageCache::updateToCache"
    //          << "currRow =" << currRow
    //          << "targetFirst =" << targetFirst
    //          << "targetLast =" << targetLast
    //          << "currPosPct =" << currPosPct;


    // update datamodel cached status
    // for (int sfRow : removedFromCache) {
    //     if (abort) break;
    //     emit setCached(sfRow, false, instance);
    // }

    if (instance != dm->instance) instance = dm->instance;

    if (debugCaching)
    {
        qDebug().noquote() << fun.leftJustified(col0Width, ' ')
                           << "current position =" << currRow
        << "toCache:" << toCache
            ;
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
    if (debugLog || G::isLogger) log("setDirection");

    int thisPrevKey = prevRow;
    prevRow = currRow;
    int thisStep = currRow - thisPrevKey;

    // cache direction just changed, increment counter
    if (sumStep == 0) {
        sumStep += thisStep;
    }
    // cache direction not changed
    else {
        // immediate direction changed, reset counter
        if (sumStep > 0 != thisStep > 0) sumStep = thisStep;
        // increment counter
        else sumStep += thisStep;
    }

    // immediate direction change exceeds threshold
    if (qAbs(sumStep) > directionChangeThreshold) {
        // immediate direction opposite to cache direction
        if (isForward != sumStep > 0) {
            isForward = sumStep > 0;
            sumStep = 0;
        }
    }
}

void ImageCache::trimOutsideTargetRange()
{
/*
    Any images in imCache that are no longer in the target range are removed.
*/
    QString src = "ImageCache::trimOutsideTargetRange";
    if (debugLog || G::isLogger)
        log("trimOutsideTargetRange");
    if (debugCaching) qDebug() << src;

    // rows being removed
    removedFromCache.clear();
    // path keys being removed
    QVector<QString> keysToRemove;

    {
        QReadLocker locker(&icd->rwLock);
        for (auto it = icd->imCache.cbegin(); it != icd->imCache.cend(); ++it) {
            const QString &fPath = it.key();
            const int sfRow = dm->proxyRowFromPath(fPath, src);

            // Not in datamodel anymore OR outside target range
            const bool notInModel = !isValidKey(sfRow);          // (sfRow == -1)
            const bool outOfRange = !notInModel && (sfRow < targetFirst || sfRow > targetLast);

            if (notInModel || outOfRange) {
                keysToRemove.append(fPath);
                removedFromCache.append(sfRow);
                emit setCached(sfRow, false, instance);
            }
        }
        // release locker
    }

    // Perform removals using the API (write-locked inside remove())
    for (const auto &key : keysToRemove) {
        icd->remove(key);
    }

    if (instance != dm->instance) return;

    // Trim toCacheStatus outside target range
    for (int sfRow : toCache) {
        if (sfRow < targetFirst || sfRow > targetLast) {
            toCacheStatus.remove(sfRow);
        }
    }

    // Trim toCache outside target range
    toCache.erase(std::remove_if(toCache.begin(), toCache.end(),
                  [&](int sfRow) {
                     return (sfRow < targetFirst || sfRow > targetLast);
                  }),
                  toCache.end());

    updateStatus(StatusAction::All, "ImageCache::trimOutsideTargetRange");
}

// --- Cache Pressure Helpers ---

qint64 ImageCache::nowMs() { return QDateTime::currentMSecsSinceEpoch(); }

inline void ImageCache::noteItemSizeMB(float szMB) {
    if (szMB <= 0) return;
    emaItemMB = (emaItemMB < 0) ? szMB : (0.8f * emaItemMB + 0.2f * szMB);
}

inline void ImageCache::updateMotion(int key, bool isForwardNow) {
    const qint64 t = nowMs();
    if (lastMoveMs == 0) { lastMoveMs = t; lastKeyForMotion = key; return; }

    const int dRows = key - lastKeyForMotion;
    const qint64 dt = t - lastMoveMs;

    // only consider pure forward steps in the current direction
    if (isForwardNow && dRows == +1) {
        if (emaStepMs < 0) emaStepMs = dt;
        else               emaStepMs = 0.7 * emaStepMs + 0.3 * dt;
        ++forwardStreak;
    } else {
        // break the streak on any non +1 forward step
        forwardStreak = 0;
        // slowly relax EMA upward to avoid permanently “rapid”
        if (emaStepMs > 0) emaStepMs = 0.9 * emaStepMs + 0.1 * 250.0;
    }

    lastMoveMs = t;
    lastKeyForMotion = key;
}

inline bool ImageCache::isRapidForward() const {
    // qDebug() << "ImageCache::isRapidForward"
    //          << "isForward =" << isForward
    //          << "forwardStreak =" << forwardStreak
    //          << "rapidMinStreak =" << rapidMinStreak
    //          << "emaStepMs =" << emaStepMs
    //          << "rapidStepMsThreshold =" << rapidStepMsThreshold
    //             ;
    return isForward &&
           forwardStreak >= rapidMinStreak &&
           emaStepMs > 0.0 &&
           emaStepMs <= rapidStepMsThreshold;
}

inline int ImageCache::calcResizeStepMB() const {
    // aim for a few images worth, but stay within min/max
    const int byItems = (emaItemMB > 0) ? int(std::ceil(emaItemMB * 3.0f)) : 0;
    int step = qMax(minStepMB, byItems);
    step = qMin(step, maxStepMB);
    return step;
}

void ImageCache::releavePressure() {
    const qint64 t = nowMs();

    // Gate by “rapid forward” unless we still have the first-adjust free pass
    const bool rapidOk = isRapidForward() || firstAdjustFreePass;
    // Only act if user is ripping forward AND cooldown elapsed
    if (!rapidOk) {
        // qDebug() << "ImageCache::releavePressure rapidForward == false"
        //     ;
        return;
    }
    if (!firstAdjustFreePass && (t - lastAdjustMs < adjustCooldownMs)) {
        // qDebug() << "ImageCache::releavePressure cooldown"
        //          << "lastAdjustMs =" << lastAdjustMs
        //          << "adjustCooldownMs =" << adjustCooldownMs
        //     ;
        return;
    }
    // qDebug() << "ImageCache::releavePressure currentPressure =" << currentPressure;

    // No flip-flopping: only grow on high pressure, shrink on very low pressure
    if (currentPressure <= pressureHigh) {
        const int step = calcResizeStepMB();
        const int newMax = qMin(maxMBCeiling, maxMB + step);
        // qDebug() << "ImageCache::releavePressure high check"
        //          << "step =" << step
        //          << "maxMB =" << maxMB
        //          << "newMax =" << newMax
        //     ;
        if (newMax != maxMB) {
            maxMB = newMax;
            lastAdjustMs = t;
            // qDebug() << "ImageCache::releavePressure high"
            //          << "currentPressure =" << currentPressure
            //          << "step =" << step
            //          << "maxMB =" << maxMB
            //     ;
            if (isShowCacheStatus)
                updateStatus(StatusAction::All, "ImageCache::releasePressure");
        }
    }
    else if (currentPressure > pressureLow) {
        const int step = calcResizeStepMB();
        const int newMax = qMax(minMB, maxMB - step);
        // qDebug() << "ImageCache::releavePressure low check"
        //          << "step =" << step
        //          << "maxMB =" << maxMB
        //          << "newMax =" << newMax
        //     ;
        if (newMax != maxMB) {
            maxMB = newMax;
            lastAdjustMs = t;
            // qDebug() << "ImageCache::releavePressure low "
            //          << "currentPressure =" << currentPressure
            //          << "step =" << step
            //          << "maxMB =" << maxMB
            //     ;
            if (isShowCacheStatus)
                updateStatus(StatusAction::All, "ImageCache::releasePressure");
        }
    }
    // otherwise stay put and keep observing
}

// --- End Cache Pressure ---

void ImageCache::setTargetRange(int key)
{
/*
    The target range is the list of images being targeted to cache, based on the current
    image, the direction of travel, the caching strategy and the maximum memory allotted
    to the image cache.  The DataModel is iterated to populate the toCache list of images
    to cache.

    • The direction of travel (dot) isForward boolean is calculated to indicate whether
      the caching should proceed forward or backward from the current position (key).

    • The terms ahead and behind are relative to the direction of travel.  If the direction
      of travel is forward, then ahead refers to rows greater than the key, while if the
      direction of travel is not forward, then ahead refers to rows less than the key.

    • aheadPos and behindPos specify positions ahead and behind the key, respectively.

    • aheadAmount and behindAmount define how many items to cache in each direction
      during each iteration.

    • The function maintains flags (aheadDone and behindDone) to indicate when caching in
      either direction is complete.

    • The start and end of the target range (targetFirst and targetLast) are assigned when
      the cache allocated memory (maxMB) has been reached or all items in the DataModel
      have been iterated.  targetFirst and targetLast are used in trimOutsideTargetRange
      and showCacheStatus.
*/
    if (debugLog || G::isLogger)
        log("setTargetRange", "row = " + QVariant(key).toString());

    if (abort) return;

    const int n = dm->sf->rowCount();
    if (n <= 0 || key < 0 || key >= n) return;

    float sumMB = 0.0f;
    int nearestToTarget = -1;

    // If folders changed, reset target queues
    if (instance != dm->instance) {
        toCache.clear();
        toCacheStatus.clear();
        instance = dm->instance;

        // --- reset motion/pressure & allow one immediate adjust ---
        forwardStreak = 0;
        emaStepMs = -1.0;
        emaItemMB = -1.0f;
        currentPressure = INT_MAX;
        lastAdjustMs = 0;
        firstAdjustFreePass = true;     // <- enable first-adjust bypass
    }

    // Direction helpers
    const int dir = isForward ? +1 : -1;

    // Forward phase aims for ~2/3 of capacity
    const float forwardCap = maxMB * (2.0f / 3.0f);

    // Positions: ahead starts at key, behind starts one step opposite
    int aheadPos  = key;
    int behindPos = key - dir;   // if dir=+1 -> key-1; if dir=-1 -> key+1

    auto addToQueue = [&](int pos) {
        if (abort) return;

        // Videos: mark cached; do not count toward sumMB
        if (dm->valueSf(pos, G::VideoColumn).toBool()) {
            emit setCached(pos, true, instance);
            (pos < key) ? targetFirst = pos : targetLast = pos;
            return;
        }

        const float szMB = dm->sf->index(pos, G::CacheSizeColumn).data().toFloat();
        if (autoMaxMB) noteItemSizeMB(szMB);

        // Only count/queue if we still have room
        if (sumMB + szMB < maxMB) {
            const QString fPath = dm->valueSf(pos, 0, G::PathRole).toString();
            // If not already queued to cache, then add to queue
            if (!toCache.contains(pos) && !icd->contains(fPath)) {
                toCacheAppend(pos);
            }
            // Get pressure on cache
            if (toCache.contains(pos) && nearestToTarget == -1) {
                nearestToTarget = pos;
                if (autoMaxMB) currentPressure = qAbs(nearestToTarget - key);
                pressureHistory.append(currentPressure);
                if (pressureHistory.size() > 50)
                    pressureHistory.removeFirst();
                /*
                qDebug() << "ImageCache::setTargetRange pressure ="
                         << currentPressure
                         << "maxMB =" << maxMB
                    ; //*/
            }
            sumMB += szMB;
            (pos < key) ? targetFirst = pos : targetLast = pos;
        }
    };

    // Fill forward up to ~2/3 capacity
    while (!abort && sumMB < forwardCap) {
        if (aheadPos < 0 || aheadPos >= n) break;
        addToQueue(aheadPos);
        aheadPos += dir;
    }

    // Fill behind until total reaches maxMB
    while (!abort && sumMB < maxMB) {
        if (behindPos < 0 || behindPos >= n) break;
        addToQueue(behindPos);
        behindPos -= dir;
    }

    // Fill forward again if still not full
    while (!abort && sumMB < maxMB) {
        if (aheadPos < 0 || aheadPos >= n) break;
        addToQueue(aheadPos);
        aheadPos += dir;
    }

    // update motion heuristics and try pressure relief
    if (autoMaxMB) {
        maxMBCeiling = G::availableMemoryMB * 0.9;
        updateMotion(key, isForward);
        releavePressure();
    }
}

void ImageCache::removeCachedImage(QString fPath)
{
/*
    Called by MW::insertFiles.  If the inserted file is a replacement of an existing
    image with the same name, then remove the existing image from imCache.  Refresh the
    thumbView and gridView.
*/
    QString src = "ImageCache::removeCachedImage";
    if (debugLog || G::isLogger) log("removeCachedImage", fPath);
    int sfRow = dm->proxyRowFromPath(fPath, src);
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

    icd->remove(fPath);
    // icd->imCache.remove(fPath);
    emit refreshViews(fPath, false, "ImageCache::setTargetRange");
}

void ImageCache::removeFromCache(QStringList &pathList)
{
/*
    Called when delete image(s).  Runs on GUI thread.
*/
    if (debugLog || G::isLogger) log("removeFromCache");

    // Mutex req'd (do not remove 2023-11-13)
    QMutexLocker locker(&gMutex);

    // rgh confirm this is working
    // remove images from imCache and toCache
    for (int i = 0; i < pathList.count(); ++i) {
        QString fPathToRemove = pathList.at(i);
        if (icd->contains(fPathToRemove)) {
            icd->remove(fPathToRemove);
        }
        int sfRow = dm->proxyRowFromPath(fPathToRemove, "ImageCache::removeFromCache");
        if (toCache.contains(sfRow)) {
            toCacheRemove(sfRow);
        }
    }
}

void ImageCache::rename(QString oldPath, QString newPath)
/*
    Called by MW::renameSelectedFiles then RenameFileDlg::renameDatamodel.
*/
{
    if (debugLog || G::isLogger) log("rename");
    if (icd->contains(oldPath)) {
        QImage image = icd->imCache.take(oldPath);  // Remove the old key-value pair
        icd->imCache.insert(newPath, image);
    }
}

void ImageCache::toCacheAppend(int sfRow)
{
    if (G::isLogger) G::log("toCacheAppend", "sfRow = " + QString::number(sfRow));
    if (abort) return;
    toCache.append(sfRow);
    toCacheStatus.insert(sfRow, {false, "", -1, instance});
}

void ImageCache::toCacheRemove(int sfRow)
{
    if (G::isLogger) G::log("toCacheRemove", "sfRow = " + QString::number(sfRow));
    toCache.removeOne(sfRow);
    toCacheStatus.remove(sfRow);
}

bool ImageCache::cacheUpToDate()
{
/*
    Determine if all images in the target range are cached or being cached.  This is used
    to terminate launching decoders in fillCache .
*/
    if (toCache.isEmpty()) return true;

    foreach (CacheItem cs, toCacheStatus) {
        if (!cs.isCaching) return false;
    }

    return true;
}

void ImageCache::memChk()
{
/*
    Check to make sure there is still room in system memory (heap) for the image cache. If
    something else (another program) has used the system memory then reduce the size of the
    cache so it still fits.
*/
    // Refresh available memory (platform sets G::availableMemoryMB)
#ifdef Q_OS_WIN
    Win::availableMemory();
#endif
#ifdef Q_OS_MAC
    Mac::availableMemory();
#endif

    return;

    // Current usage and headroom
    quint64 zero = 0.0;
    quint64 availMB = qMax(zero, G::availableMemoryMB);
    quint64 currMB  = icd->sizeMB();
    // quint64 currMB  = getImCacheSize();
    quint64 roomMB  = maxMB - currMB;

    // If the cache headroom is larger than what's actually free,
    // reduce maxMB so we don't exceed available memory.
    if (availMB < roomMB) {
        maxMB = currMB + availMB;
    }

    // Always enforce the minimum
    if (maxMB < minMB) {
        // maxMB = minMB;  // maxMB change
    }

    // qDebug() << "imageCache::memCheck"
    //          << "G::availableMemoryMB =" << G::availableMemoryMB
    //          << "maxMB =" << maxMB
    //          // << "icd->sizeMB() =" << icd->sizeMB()
    //          << "getImCacheSize() =" << getImCacheSize()
    //     ;

}

void ImageCache::updateStatus(int instruction, QString source)
/*
    Displays a statusbar showing the image cache status. Also shows the cache size in the info
    panel. All status info is passed by copy to prevent collisions on source data, which is
    being continuously updated by ImageCache.

    Note that the caching progressbar is no longer shown:
    (unless G::showProgress == G::ShowProgress::ImageCache)
*/
{
    // rgh get G::showProgress == G::ShowProgress::ImageCache working
    // not all items req'd passed via showCacheStatus since icd->cache struct eliminated
    quint64 currMB  = icd->sizeMB();
    // float currMB = getImCacheSize();
    /*
    qDebug() << "ImageCache::updateStatus"
             << "targetFirst =" << targetFirst
             << "targetLast =" << targetLast
             << "currMB =" << currMB
        ; //*/
    emit showCacheStatus(instruction, autoMaxMB, currMB, maxMB,
                         targetFirst, targetLast, source);
}

void ImageCache::log(const QString function, const QString comment)
{
    G::log("ImageCache::" + function, comment);
}

QString ImageCache::diagnostics()
{
    if (G::isLogger) log("diagnostics");
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
    if (G::isLogger) log("reportCacheParameters");
    if (debugCaching) qDebug() << "ImageCache::reportCacheParameters";
    QString reportString;
    QTextStream rpt;
    rpt.flush();
    reportString = "";
    rpt.setString(&reportString);

    rpt << "\n";
    rpt << "isRunning                = " << (imageCacheThread.isRunning() ? "true" : "false") << "\n";
    rpt << "abort                    = " << (abort ? "true" : "false") << "\n";
    rpt << "retry                    = " << retry;

    rpt << "\n";
    rpt << "instance                 = " << instance << "\n";
    rpt << "G::dmInstance            = " << G::dmInstance << "\n";

    rpt << "\n";
    rpt << "isInitializing           = " << (isInitializing ? "true" : "false") << "\n";
    rpt << "isShowCacheStatus        = " << QVariant(isShowCacheStatus).toString() << "\n";

    rpt << "\n";
    rpt << "totFiles                 = " << dm->sf->rowCount() << "\n";
    rpt << "autoMaxMB                = " << QVariant(autoMaxMB).toString() << "\n";
    rpt << "currMB                   = " << icd->sizeMB() << "\n";
    rpt << "maxMB                    = " << maxMB << "\n";
    rpt << "minMB                    = " << minMB << "\n";

    rpt << "\n";
    rpt << "decoderCount             = " << decoderCount << "\n";

    rpt << "\n";
    rpt << "dm->currentSfRow         = " << dm->currentSfRow << "\n";
    rpt << "key                      = " << currRow << "\n";
    rpt << "prevKey                  = " << prevRow << "\n";

    rpt << "\n";
    rpt << "step                     = " << step << "\n";
    rpt << "sumStep                  = " << sumStep << "\n";
    rpt << "directionChangeThreshold = " << directionChangeThreshold << "\n";
    rpt << "wtAhead                  = " << wtAhead << "\n";
    rpt << "isForward                = " << (isForward ? "true" : "false") << "\n";
    rpt << "\n";
    rpt << "targetFirst              = " << targetFirst << "\n";
    rpt << "targetLast               = " << targetLast << "\n";

    rpt << "\n";
    rpt << "toCache count            = " << toCache.count() << "\n";
    rpt << "cacheUpToDate            = " << (cacheUpToDate() ? "true" : "false") << "\n";
    rpt << "\n";

    QStringList parts;
    parts.reserve(pressureHistory.size());
    for (int v : pressureHistory) {
        parts << QString::number(v);
    }
    rpt << "Pressure History: {" + parts.join(", ") + "}\n";
    rpt << "\n";

    return reportString;
}

QString ImageCache::reportCacheDecoders()
{
    if (G::isLogger) log("reportCacheDecoders");
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
    rpt << "  isIdle    ";
    rpt << "  isCycling    ";
    rpt << "\n";
    for (int id = 0; id < decoderCount; ++id) {
        rpt << QString::number(id).rightJustified(4);
        rpt << "  ";
        rpt << decoders[id]->statusText.at(decoders[id]->status).leftJustified(14);
        rpt << QString::number(decoders[id]->sfRow).rightJustified(5);
        rpt << "  ";
        rpt << QString::number(instance).rightJustified(13);
        rpt << "   ";
        rpt << QVariant(decoders[id]->isIdle()).toString();
        rpt << "          ";
        rpt << QVariant(cycling.at(id)).toString();
        rpt << "\n";
    }
    rpt << "\n";
    return reportString;
}

QString ImageCache::reportCacheItemList(QString title)
{
    if (G::isLogger) log("reportCacheItemList");
    if (debugCaching) qDebug() << "ImageCache::reportCache";

    QStringList rptStatus {
        "Undefine",
        "Success",
        "Abort",
        "Invalid",
        "Failed",
        "Video",
        "Clash",
        "NoDir",
        "BlankFi",
        "NoMeta",
        "FileOpen"
    };

    QString reportString;
    QTextStream rpt;
    rpt.flush();
    reportString = "";
    rpt.setString(&reportString);

    int cachedCount = 0;
    for (int sfRow = 0; sfRow < dm->sf->rowCount(); ++sfRow) {
        // show header every 40 rows
        if (sfRow % 40 == 0) {
            rpt.reset();
            rpt.setFieldAlignment(QTextStream::AlignRight);
            rpt.setFieldWidth(9);
            rpt
                << "Key"
                << "Decoder"
                << "Target"
                << "Attempts"
                << "Caching"
                << "Cached"
                << "Status"
                << "imCached"
                << "Attempt"
                << "MetaLoad"
                << "Video"
                << "SizeMB"
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
            << dm->sf->index(sfRow, G::DecoderIdColumn).data().toInt()
            << (sfRow >= targetFirst && sfRow <= targetLast ? "true" : "false")
            << dm->sf->index(sfRow, G::AttemptsColumn).data().toString()
            << (dm->index(sfRow, G::IsCachingColumn).data().toBool() ? "true" : "false")
            << (dm->index(sfRow, G::IsCachedColumn).data().toBool() ? "true" : "false")
            << rptStatus.at(dm->sf->index(sfRow, G::DecoderReturnStatusColumn).data().toInt())
            << (icd->contains(fPath) ? "true" : "false")
            << (dm->index(sfRow, G::MetadataAttemptedColumn).data().toBool() ? "true" : "false")
            << (dm->index(sfRow, G::MetadataLoadedColumn).data().toBool() ? "true" : "false")
            << (dm->index(sfRow, G::VideoColumn).data().toBool() ? "true" : "false")
            << QString::number(dm->sf->index(sfRow, G::CacheSizeColumn).data().toFloat(), 'f', 2)
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

    if (toCache.isEmpty())
        rpt << "\ntoCache: Empty" << "\n";
    else {
        rpt << "\ntoCache:" << "\n";
        rpt.setFieldWidth(9);
        rpt.setFieldAlignment(QTextStream::AlignRight);
        rpt << "Priority" << "Key";
        rpt.setFieldWidth(11);
        rpt << "isCaching";
        rpt.setFieldAlignment(QTextStream::AlignLeft);
        rpt.setFieldWidth(25);
        rpt << "   Message";
        rpt << "   Path";
        rpt.setFieldWidth(0);
        rpt << "\n";
        for (int i = 0; i < toCache.size(); ++i) {
            int sfRow = toCache.at(i);
            // QString msg;
            // okToDecode(sfRow, msg);
            rpt.setFieldWidth(9);
            rpt.setFieldAlignment(QTextStream::AlignRight);
            rpt << i << sfRow;
            rpt.setFieldWidth(11);
            rpt << QVariant(toCacheStatus[i].isCaching).toString();
            rpt.setFieldWidth(3);
            rpt << "   ";
            rpt.setFieldAlignment(QTextStream::AlignLeft);
            rpt.setFieldWidth(25);
            rpt << toCacheStatus[sfRow].msg;
            rpt.setFieldWidth(50);
            rpt << dm->sf->index(sfRow, 0).data(G::PathRole).toString();
            rpt.setFieldWidth(0);
            rpt << "\n";
        }
    }
    return reportString;
}

QString ImageCache::reportImCache()
{
    if (G::isLogger) log("reportImCache");
    QString reportString;
    QTextStream rpt;
    rpt.flush();
    reportString = "";
    rpt.setString(&reportString);

    // check when imCache is empty
    QImage image;
    int mem = 0;

    QStringList keys = icd->imCache.keys();
    if (keys.size() == 0) {
        rpt << "\nicd->imCache is empty";
        return reportString;
    }

    // build list of report items
    struct ImRptItem {
        int hashKey;
        int sfRow;
        int w;
        int h;
        float mb;
        QString fPath;
    } imRptItem;
    QList<ImRptItem> rptList;
    for (int i = 0; i < keys.length(); ++i) {
        imRptItem.hashKey = i;
        imRptItem.fPath = keys.at(i);
        imRptItem.sfRow = dm->proxyRowFromPath(imRptItem.fPath, "ImageCache::reportImCache");
        image = icd->imCache.value(keys.at(i));    // QHash<QString, QImage> imCache
        imRptItem.w = image.width();
        imRptItem.h = image.height();
        imRptItem.mb = static_cast<float>(image.sizeInBytes() / (1 << 20));
        // imRptItem.mb = static_cast<float>(imRptItem.w * imRptItem.h * 1.0 / 262144);
        rptList.append(imRptItem);
    }
    // sort
    std::sort(rptList.begin(), rptList.end(), [](const ImRptItem &a, const ImRptItem &b) {
        return a.sfRow < b.sfRow;
    });

    // report header
    rpt << "\nimCache hash: ";
    rpt << keys.length() << " items";

    rpt << "\n";
    rpt.reset();
    rpt.setFieldAlignment(QTextStream::AlignRight);
    rpt.setFieldWidth(6);
    rpt << "Count";
    // rpt.setFieldWidth(10);
    // rpt << "Priority";
    rpt.setFieldWidth(6);
    rpt << "sfRow" << "W" << "H";
    rpt.setFieldWidth(9);
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
        // report this item
        rpt.reset();
        rpt.setFieldAlignment(QTextStream::AlignRight);
        rpt.setFieldWidth(6);
        rpt << i;
        // rpt.setFieldWidth(10);
        // rpt << rptList.at(item).priorityKey;
        rpt.setFieldWidth(6);
        rpt << rptList.at(i).sfRow;
        rpt << rptList.at(i).w;
        rpt << rptList.at(i).h;
        rpt.setFieldWidth(9);
        rpt << QString::number(rptList.at(i).mb, 'f', 2);
        rpt.reset();
        rpt.setFieldAlignment(QTextStream::AlignLeft);
        rpt << "  ";
        rpt << rptList.at(i).fPath;
        rpt << "\n";
        mem += (rptList.at(i).mb /*/ 1024 / 1024*/);
    }

    rpt << "\n" << keys.length() << " images in image cache.";
    rpt << "\n" << mem << " MB consumed";

    return reportString;
}

QString ImageCache::reportImCacheRows()
{
    if (G::isLogger) log("reportImCacheRows");
    QString reportString;
    QTextStream rpt;
    rpt.flush();
    reportString = "";
    rpt.setString(&reportString);
    QList<int> imCacheRows;
    auto it = icd->imCache.begin();
    while (it != icd->imCache.end()) {
        QString fPath = it.key();
        int sfRow = dm->proxyRowFromPath(fPath, "ImageCache::reportImCacheRows");
        imCacheRows.append(sfRow);
        ++it;
    }
    rpt << "Cached:  ";
    // sort imCacheRows
    std::sort(imCacheRows.begin(), imCacheRows.end());
    foreach(int sfRow, imCacheRows) {
        rpt << QString::number(sfRow) << " ";
    }
    return reportString;
}

QString ImageCache::reportToCacheRows()
{
    if (G::isLogger) log("reportToCacheRows");
    QString reportString;
    QTextStream rpt;
    rpt.flush();
    reportString = "";
    rpt.setString(&reportString);
    rpt << "toCache: ";
    for (int i = 0; i < toCache.length(); ++i) {
        int sfRow = toCache.at(i);
        rpt << QString::number(sfRow);
        if (toCacheStatus[sfRow].isCaching) rpt << "c";
        rpt << " ";
    }
    return reportString;
}

QString ImageCache::reportToCache()
{
    if (G::isLogger) log("reportToCache");
    QString blank = " ";
    QString reportString;
    QTextStream rpt;
    rpt.setString(&reportString);
    rpt << "targetFirst = " << QString::number(targetFirst);
    rpt << " targetLast = " << QString::number(targetLast);
    rpt << "\n";
    /*
    rpt << blank.leftJustified(63);
    rpt << "sfRow    decoder   status      dmIsCaching   isCached";
    for (int i = 0; i < toCache.length(); i++) {
        rpt << "\n";
        rpt << blank.leftJustified(63);
        int sfRow = toCache.at(i);
        rpt << QString::number(sfRow).leftJustified(9);
        rpt << QString::number(toCacheStatus[sfRow].decoderId).leftJustified(10);
        rpt << statusText.at(toCacheStatus[sfRow].status).leftJustified(12);
        bool dmIsCaching = dm->sf->index(sfRow, G::IsCachingColumn).data().toBool();
        rpt << QVariant(dmIsCaching).toString().leftJustified(14);
        rpt << QVariant(isCached(sfRow)).toString();
    }
    rpt << "\n";
    //*/
    rpt << blank.leftJustified(63);
    rpt << reportToCacheRows();
    rpt << "\n";
    rpt << blank.leftJustified(63);
    rpt << reportImCacheRows();
    // rpt << "\n";
    // rpt << reportImCache();
    return reportString;
}

void ImageCache::initialize(quint64 cacheMaxMB,
                            quint64 cacheMinMB,
                            bool isShowCacheStatus,
                            int cacheWtAhead)
{
    if (!G::useImageCache) return;   // rgh isolate image cache
    QString fun = "ImageCache::initialize";
    if (G::isLogger || G::isFlowLogger) log(fun);

    if (debugCaching)
    {
        qDebug().noquote()
            << fun.leftJustified(col0Width, ' ')
            << "dm->instance =" << dm->instance
            ;
    }

    QMutexLocker locker(&gMutex);

    isInitializing = true;

    // reset the image cache
    icd->clear();
    toCache.clear();
    toCacheStatus.clear();

    // cancel if no images to cache
    if (!dm->sf->rowCount()) return;

    // just in case stopImageCache not called before this
    // if (imageCacheThread.isRunning()) stop("ImageCache::initImageCache");

    // update folder change instance
    instance = dm->instance;

    // cache management parameters
    currRow = 0;
    prevRow = -1;
    // the cache defaults to the first image and a forward selection direction
    isForward = true;
    // the amount of memory to allocate to the cache
    // maxMB = cacheMaxMB; // maxMB change
    minMB = cacheMinMB;
    this->isShowCacheStatus = isShowCacheStatus;
    wtAhead = cacheWtAhead;
    targetFirst = 0;
    targetLast = 0;
    directionChangeThreshold = 3;
    decodeImageCount = 0;
    decodeImageMsTot = 0;
    decodeImageMBTot = 0;
    firstDispatchNewDM = true;

    if (isShowCacheStatus) {
        updateStatus(StatusAction::Clear, "ImageCache::initializeImageCache");
    }

    isInitializing = false;
    abort = false;
}

void ImageCache::adjustCacheMem(quint64 ms, quint64 mb)
{
/*
    Cache size is determined based on showing up to 30 FPS (keyboard repeat rate) = 30ms
    Average decode image ms = t = 215ms
    Synchronous decoders = 20 = n
    Average image cache size = 174MB = s

    t = 215ms
    r = 30ms
    n = 20
    s = 174MB

    ceil_t_over_r = (t + r - 1) / r = 8.13
    imgsNeeded = 8.13 - 20 = -11.9
    Bmin = 0
    out_minMB = 0
*/
    if (G::isLogger) log("adjustCacheMem");
    static quint64 prevMsAve = 1000000;
    const int r = 30;
    const int adjust = 200; // 340
    decodeImageCount++;
    decodeImageMsTot += ms;
    decodeImageMBTot += mb;
    quint64 msAve = decodeImageMsTot / decodeImageCount;
    quint64 mbAve = decodeImageMBTot / decodeImageCount;

    // if difference is greater than 10% then update maxMB
    quint64 diff = (prevMsAve > msAve) ? (prevMsAve - msAve) : (msAve - prevMsAve);
    // change threshold in %
    int changeThreshold = 10;
    bool isSignificantChange = diff * changeThreshold > prevMsAve;
    if (isSignificantChange) {
        maxMB = (msAve + r - 1) / r * mbAve * 2;
        // maxMB = msAve * adjust / decoderCount;
        prevMsAve = msAve;
    }

    // /*
    qDebug() << "imageCache::adjustCacheMem"
             << "G::availableMemoryMB =" << G::availableMemoryMB
             << "decodeImageCount =" << decodeImageCount
             << "ms =" << ms
             << "msAve =" << msAve
             << "maxMB =" << maxMB
             << "isSignificantChange =" << isSignificantChange
        ; //*/
}

void ImageCache::updateInstance()
{
    if (G::isLogger) log("updateInstance");
    qDebug() << "ImageCache::updateInstance" << dm->instance;
    instance = dm->instance;
}

bool ImageCache::getAutoMaxMB()
{
    return autoMaxMB;
}

void ImageCache::setAutoMaxMB(bool autoSize)
{
    autoMaxMB = autoSize;
    updateStatus(StatusAction::Size, "ImageCache::setAutoMaxMB");
}

quint64 ImageCache::getMaxMB()
{
    return maxMB;
}

bool ImageCache::getShowCacheStatus()
{
    return isShowCacheStatus;
}

void ImageCache::setMaxMB(quint64 mb)
{
    if (G::isLogger) log("setMaxMB");
    maxMB = mb;
    memChk();
    updateStatus(StatusAction::Size, "ImageCache::setMaxMB");
}

void ImageCache::setShowCacheStatus(bool isShowCacheStatus)
{
    if (G::isLogger) log("setShowCacheStatus");
    this->isShowCacheStatus = isShowCacheStatus;
}

void ImageCache::updateImageCacheParam(quint64 cacheSizeMB,
                                       quint64 cacheMinMB,
                                       bool isShowCacheStatus,
                                       int cacheWtAhead)
{
/*
    When various image cache parameters are changed in preferences they are updated here.
*/
    if (G::isLogger) log("updateImageCacheParam");
    if (debugCaching) qDebug() << "ImageCache::updateImageCacheParam";
    // rgh cache amount fix from pref to here
    maxMB = cacheSizeMB;
    minMB = cacheMinMB;
    this->isShowCacheStatus = isShowCacheStatus;
    wtAhead = cacheWtAhead;
    reloadImageCache();
}

void ImageCache::filterChange(QString currentImageFullPath, QString src)
{
/*
    When the datamodel is filtered the image cache needs to be updated. The cacheItemList is
    rebuilt for the filtered dataset and isCached updated, the current image is set, and any
    surplus cached images (not in the filtered dataset) are removed from imCache.
    The image cache is now ready to run by calling setCachePosition().
*/
    QString fun = "ImageCache::filterChange";
    if (debugLog || G::isLogger || G::isFlowLogger)
        G::log("filterChange", "src = " + src);
    if (debugCaching)
    {
        qDebug().noquote()
            << fun.leftJustified(col0Width, ' ')
            << "dm->sf->rowCount() =" << dm->sf->rowCount()
            << "currentImageFullPath =" << currentImageFullPath
            ;
    }
    if (dm->sf->rowCount() == 0) return;
    if (G::removingRowsFromDM) return;

    // do not use mutex - spins forever

    // update instance
    instance = dm->instance;

    // reset the image cache
    icd->clear();
    toCache.clear();
    toCacheStatus.clear();
    updateStatus(StatusAction::Clear, "filterChange");

    setCurrentPosition(currentImageFullPath, fun);
}

void ImageCache::refreshImageCache()
{
    if (debugLog || G::isLogger || G::isFlowLogger) log("refreshImageCache");
    QString fun = "ImageCache::refreshImageCache";
    if (debugCaching)
    {
        qDebug().noquote() << fun.leftJustified(col0Width, ' ')
                           << "isRunning =" << imageCacheThread.isRunning();
    }
    // if (imageCacheThread.isRunning()) {
    //     QMetaObject::invokeMethod(this, "updateToCache", Qt::QueuedConnection);
    // }

    abort = false;
    // QMetaObject::invokeMethod(this, "updateToCache", Qt::QueuedConnection);
    dispatch();
}

void ImageCache::reloadImageCache()
{
/*
    Reload all images in the cache.  Used by colorManageChange.
*/
    if (debugCaching)
        qDebug() << "ImageCache::colorManageChange";
    if (debugLog || G::isLogger || G::isFlowLogger) log("reloadImageCache");
    // QMutexLocker locker(&gMutex);   // req'd 2025-02-19
    abort = false;
    // reset the image cache
    icd->clear();
    toCache.clear();
    toCacheStatus.clear();
    dispatch();
}

void ImageCache::colorManageChange()
{
/*
    Called when color manage is toggled.  Reload all images in the cache.
*/
    if (debugCaching)
        qDebug() << "ImageCache::colorManageChange";
    if (debugLog || G::isLogger || G::isFlowLogger) log("colorManageChange");
    reloadImageCache();
}

void ImageCache::cacheSizeChange()
{
/*
    Called when changes are made to image cache parameters in preferences. The image cache
    direction, priorities and target are reset and orphans fixed.
*/
    QString fun = "ImageCache::cacheSizeChange";
    refreshImageCache();
}

void ImageCache::setCurrentPosition(QString fPath, QString src)
{
/*
    Called from MW::fileSelectionChange to reset the position in the image cache. The image
    cache direction, priorities and target are reset and the cache is updated in fillCache.

    Do not use QMutexLocker. Can cause deadlock in updateTargets()

*/
    if (G::removingRowsFromDM) return;

    QString fun = "ImageCache::setCurrentPosition";
    currRow = dm->proxyRowFromPath(fPath, fun);

    if (debugLog || G::isLogger || G::isFlowLogger)
        log("setCurrentPosition", "row = " + QString::number(currRow));
    if (debugCaching)
    {
        qDebug().noquote() << fun.leftJustified(col0Width, ' ')
        << "currRow =" << currRow
        << fPath
        << "src =" << src
        << "isRunning =" << imageCacheThread.isRunning();
    }

    if (G::dmInstance != instance) {
        // if (debugCaching)
        {
            QString msg = "Instance clash from " + src;
            G::issue("Comment", msg, "ImageCache::setCurrentPosition", currRow, fPath);
            // qWarning() << "ImageCache::setCurrentPosition cancelled instance change row =" << currRow;
        }
        return;
    }

    if (isInitializing) {
        qDebug() << fun << "initializing so return without caching";
        return;
    }

    // range check
    if (currRow >= dm->sf->rowCount()) {
        QString msg = "currRow is out of range.";
        int i = dm->currentSfRow;
        G::issue("Warning", msg, "ImageCache::setCurrentPosition", i, fPath);
        qWarning() << "WARNING ImageCache::setCurrentPosition currRow ="
                   << currRow << "is out of range";
        return;
    }

    /*
    // current position in target range
    float effectiveBufferPct = 0;
    if (currRow >= targetFirst || currRow <= targetLast) {
        int targetRange = targetLast - targetFirst + 1;
        if (isForward) {
            effectiveBufferPct = static_cast<float>(targetLast - currRow + 1) / targetRange;
        }
        else {
            effectiveBufferPct  = static_cast<float>(currRow - targetFirst + 1) / targetRange;
        }
    }
    qDebug() << "ImageCache::dispatch"
             << "targetFirst =" << targetFirst
             << "targetLast =" << targetLast
             << "currRow =" << currRow
             << "effectiveBufferPct =" << effectiveBufferPct;
    */

    abort = false;
    dispatch();
}

bool ImageCache::okToDecode(int sfRow, int id, QString &msg)
{
    // skip videos
    if (dm->sf->index(sfRow,G::VideoColumn).data().toBool()) {
        msg = "Video";
        return false;
    }

    // already in imCache
    QString fPath = dm->sf->index(sfRow, 0).data(G::PathRole).toString();
    if (icd->contains(fPath)) {
        msg = "Already in imCache";
        return false;
    }

    // isCaching
    if (toCacheStatus.contains(sfRow)) {
        if (toCacheStatus[sfRow].isCaching) {
            msg = "IsCaching";
            return false;
        }
    }

    // make sure metadata has been loaded
    // if (!dm->sf->index(sfRow, G::MetadataLoadedColumn).data().toBool()) {
    if (!dm->sf->index(sfRow, G::MetadataAttemptedColumn).data().toBool()) {
        if (!waitForMetaRead(sfRow, 50)) {
            msg = "Metadata not loaded";
            return false;
        }
    }

    // next item to cache
    msg = "Success";
    return true;

}

int ImageCache::nextToCache(int id)
{
/*
    The next image to cache is determined by traversing the toCache list in ascending
    order to find the first one not currently being cached:

    • isCaching is false and attempts < maxAttemptsToCacheImage.

    • decoderId matches item, isCaching is true and isCached = false. If this is the case
      then we know the previous attempt failed, and we should try again if the failure was
      because the file was already open, unless the attempts are greater than
      maxAttemptsToCacheImage.

*/
    auto sDebug = [](const QString& sId, const QString& msg) {
        QString fun = "ImageCache::nextToCache";
        // G::log(fun, sId + msg);
        qDebug().noquote()
            << fun.leftJustified(50, ' ')
            << sId << msg;
    };

    if (debugLog || G::isLogger)
        log("nextToCache", "CacheUpToDate = " + QVariant(cacheUpToDate()).toString());

    QString fun = "ImageCache::nextToCache";
    bool debugThis = false;
    QString sId = "decoder " + QString::number(id).leftJustified(3);

    if (debugCaching)
    {
        qDebug().noquote()
        << fun.leftJustified(col0Width, ' ')
        << "toCache:" << toCache
            ;
    }
    /*
    // ImageCache instance out-of-date
    if (instance != dm->instance) {
        if (debugThis) {
            msg = "Instance clash";
            sDebug(sId, msg);
        }
        return -1;
    }
    */

    QMutexLocker locker(&gMutex);   // req'd 2025-02-03

    if (toCache.isEmpty()) {
        if (debugThis) sDebug(sId, "toCache is empty");
        return -1;
    }

    if (debugCaching)
    {
        qDebug().noquote()
            << fun.leftJustified(col0Width, ' ')
            << "toCache count =" << toCache.count()
            << "target count =" << targetLast - targetFirst + 1
            << "from" << targetFirst << "to" << targetLast
            ;
    }

    // iterate toCache
    for (int i = 0; i < toCache.count(); ++i) {
        int sfRow = toCache.at(i);

        QString sRow = QString::number(sfRow).leftJustified(5);

        QString msg;

        if (okToDecode(sfRow, id, msg)) {
            // set here in case called from launchDecoders
            // toCacheStatus[sfRow].status = Status::Caching;
            toCacheStatus[sfRow].msg = msg;
            return sfRow;
        }
        else {
            toCacheStatus[sfRow].msg = msg;
            if (debugThis) {
                msg = "row = " + sRow + msg;
                sDebug(sId, msg);
            }
            continue;
        }
    }
    return -1;
}

void ImageCache::decodeNextImage(int id, int sfRow)
{
/*
    Called from fillCache to run a decoder for the next image to decode into a QImage.
    The decoders run in separate threads. When they complete the decoding they signal
    back to fillCache.
*/
    QString src = "ImageCache::decodeNextImage";
    if (debugLog || G::isLogger || G::isFlowLogger)
    {
        log("decodeNextImage",
            "decoder " + QString::number(id).leftJustified(3) +
            "row = " + QString::number(sfRow).leftJustified(5) +
            "instance = " + QString::number(instance).leftJustified(5)
            );
    }

    // set isCaching
    if (toCacheStatus.contains(sfRow)) {
        toCacheStatus[sfRow].isCaching = true;
        toCacheStatus[sfRow].msg = "";
        toCacheStatus[sfRow].decoderId = id;
        toCacheStatus[sfRow].instance = instance;
    }
    emit setValSf(sfRow, G::IsCachingColumn, true, instance, src);
    emit setValSf(sfRow, G::DecoderIdColumn, id, instance, src);
    int attempts = dm->sf->index(sfRow, G::AttemptsColumn).data().toInt();
    attempts++;
    // qDebug() << src << "row =" << sfRow << "attempts =" << attempts;
    emit setValSf(sfRow, G::AttemptsColumn, attempts, instance, src);

    if (debugLog)
    {
    log("decodeNextImage",
        "decoder " + QString::number(id).leftJustified(3) +
        "row = " + QString::number(sfRow).leftJustified(5) +
        "row decoder = " + dm->sf->index(sfRow, G::DecoderIdColumn).data().toString().leftJustified(3) +
        "isCached = " + dm->sf->index(sfRow, G::IsCachedColumn).data().toString().leftJustified(6) +
        "isCaching = " + dm->sf->index(sfRow, G::IsCachingColumn).data().toString().leftJustified(6) +
        "attempt = " + dm->sf->index(sfRow, G::AttemptsColumn).data().toString().leftJustified(3) +
        "isMetadata = " + dm->sf->index(sfRow, G::MetadataLoadedColumn).data().toString().leftJustified(6) +
        "status = " + dm->sf->index(sfRow,G::DecoderReturnStatusColumn).data().toString()
        );
    }

    if (debugCaching)
    {
        QString fun = "ImageCache::decodeNextImage";
        qDebug().noquote()
            << fun.leftJustified(col0Width, ' ')
            << "decoder" << QString::number(id).leftJustified(3)
            << "row =" << QString::number(sfRow).leftJustified(4)
            << "isRunning =" << QVariant(decoderThreads[id]->isRunning()).toString()
            << "attempts =" << dm->sf->index(sfRow, G::AttemptsColumn).data().toInt()
            << dm->sf->index(sfRow,0).data(G::PathRole).toString()
            ;
    }

    if (!decoderThreads[id]->isRunning()) decoderThreads[id]->start();

    // emit decode(sfRow, instance);

    bool success = false;
    if (!abort)
        success = QMetaObject::invokeMethod(decoders[id], "decode",
                                  Qt::QueuedConnection,
                                  Q_ARG(int, sfRow), Q_ARG(int, instance));

    if (!success) {
        QString fun = "ImageCache::decodeNextImage";
        qDebug().noquote()
            << fun.leftJustified(col0Width, ' ')
            << "decoder" << QString::number(id).leftJustified(3)
            << "row =" << QString::number(sfRow).leftJustified(4)
            << "invokeMethod failed!"
            ;
        if (!abort) fillCache(id);
    }
}

void ImageCache::cacheImage(int id, int sfRow)
{
/*
    Called from fillCache to insert a QImage that has been decoded into icd->imCache.
    Do not cache video files, but do show them as cached for the cache status display.
*/
    QString src = "ImageCache::cacheImage";
    { // log
        if (G::isLogger || G::isFlowLogger) {
            QString comment = "decoder " + QString::number(id).leftJustified(3) +
                              "row = " + QString::number(sfRow).leftJustified(5) +
                              "cache size = " + QString::number(icd->sizeMB());
            log("cacheImage", comment);
        }
        if (debugCaching)
        {
            qDebug().noquote()
                << src.leftJustified(col0Width, ' ')
                << "decoder" << QString::number(id).leftJustified(3)
                << "row =" << QString::number(sfRow).leftJustified((4))
                << "G::mode =" << G::mode
                // << "ms from last cache =" << G::t.restart()
                // << "errMsg =" << decoders[id]->errMsg
                // << "decoder[id]->fPath =" << decoders[id]->fPath
                ;
        }
    }

    // cache the image
    if (!abort) icd->insert(decoders[id]->fPath, decoders[id]->image);

    /*
    qDebug().noquote()
        << src.leftJustified(col0Width, ' ')
        << "decoder" << QString::number(id).leftJustified(3)
        << "row =" << QString::number(sfRow).leftJustified((4))
        << "sumCacheMB =" << getImCacheSize()
        ;//*/

     // remove from toCache
    if (!abort) if (toCache.contains(sfRow)) toCacheRemove(sfRow);

    // update datamodel cache status
    if (!abort) emit setCached(sfRow, true, instance);

    // update cache status in cache progress bar
    if (!abort) updateStatus(StatusAction::All, "ImageCache::cacheImage");
}

bool ImageCache::okToCache(int id, int sfRow)
{
/*
    Called by fillCache.  Returns true to add image to image cache.
*/
    QString src = "ImageCache::okToCache";

    if (instanceClash(id)) return false;

    if (sfRow >= dm->sf->rowCount()) return false;

    // set isCaching to false
    emit setValSf(sfRow, G::IsCachingColumn, false, instance, src);
    if (toCacheStatus.contains(sfRow))
        toCacheStatus[sfRow].isCaching = false;

    // save decoder status
    emit setValSf(sfRow, G::DecoderReturnStatusColumn,
                    static_cast<int>(decoders[id]->status), instance, src);

    // no longer in target range to cache
    if (!toCache.contains(sfRow)) return false;

    // if Imagedecoder failed then remove from toCache
    if (decoders[id]->status != ImageDecoder::Status::Success) {
        if (debugCaching)
        {
            QString fun = "ImageCache::fillCache FAILED";
            qDebug().noquote()
                << fun.leftJustified(col0Width, ' ')
                << "decoder" << QString::number(id).leftJustified(3)
                << "row =" << QString::number(sfRow).leftJustified((4))
                << "status =" << decoders[id]->statusText.at(decoders[id]->status)
                << "errMsg =" << decoders[id]->errMsg
                << "decoder[id]->fPath =" << decoders[id]->fPath
                ;
        }
        if (decoders[id]->errMsg != "") {
            emit setValSf(sfRow, G::DecoderErrMsgColumn,
                          decoders[id]->errMsg, instance, src);
        }
        if (toCache.contains(sfRow)) toCacheRemove(sfRow);
        return false;
    }

    // image cache is full (prevent runaway caching)
    float toCacheMB = decoders[id]->image.sizeInBytes() / (1 << 20);
    quint64 currCacheMB = icd->sizeMB();
    // float currCacheMB = getImCacheSize();
    if ((currCacheMB + toCacheMB) > maxMB) {
        toCache.clear();
        toCacheStatus.clear();
        /*
        qDebug() << "ImageCache::okToCache"
                 << "row =" << sfRow
                 << "maxMB HAS BEEN EXCEEDED"; //*/
        return false;
    }

    /*
    qDebug() << "ImageCache::okToCache"
             << "row =" << sfRow
             << "targetFirst =" << targetFirst
             << "targetLast =" << targetLast
             << "okToCache = true";
                //*/
    return true;
}

bool ImageCache::nullInImCache()
{
    QString src = "ImageCache::nullInImCache";

    bool isEmptyImage = false;
    for (const QString &path : icd->imCache.keys()) {
        // empty image in cache
        if (icd->imCache.value(path).width() == 0) {
            int sfRow = dm->proxyRowFromPath(path, "ImageCache::nullInImCache");
            // add back to toCache list
            toCacheAppend(sfRow);
            // set isCaching to true
            emit setValSf(sfRow, G::IsCachingColumn, true, instance, src);
            isEmptyImage = true;
            qDebug() << "XXXXXXXXXX" << src << "row =" << sfRow;
        }
    }
    return isEmptyImage;
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

    QString src = "";

    int cacheRow = decoders[id]->sfRow;


    if (debugCaching)
    {
        QString fun = "ImageCache::fillCache";
        qDebug().noquote()
            << fun.leftJustified(col0Width, ' ')
            << "decoder" << QString::number(id).leftJustified(3)
            << "row =" << QString::number(cacheRow).leftJustified((4))
            << "decoder status =" << decoders[id]->statusText.at(decoders[id]->status)
            << "image.w =" << decoders[id]->image.width()
             ;
    }
    if (debugLog || G::isLogger)
    {
        log("fillCache",
            "decoder " + QString::number(id).leftJustified(3) +
            "row = " + QString::number(decoders[id]->sfRow).leftJustified(4)
        );
    }

    if (okToCache(id, cacheRow)) {
        if (debugCaching)
        {
            QString fun = "ImageCache::fillCache cache image";
            qDebug().noquote() << fun.leftJustified(col0Width, ' ')
                               << "returning decoder" << id
                               << "row =" << cacheRow
                               << "status =" << decoders[id]->status
                               << "status =" << decoders[id]->statusText.at(decoders[id]->status)
                               << "errMsg =" << decoders[id]->errMsg
                               << "isRunning =" << imageCacheThread.isRunning()
                ;
        }
        if (!abort) cacheImage(id, cacheRow);
        /*
        // adjust cache size based on folder contents
        quint64 mbReqd = dm->sf->index(cacheRow, G::CacheSizeColumn).data().toInt();
        if (!abort) adjustCacheMem(decoders[id]->msToDecode, mbReqd);
        */
    }

    // get next image to cache
    // rgh todo add retry here?  Used Status::Failed
    int toCacheKey = nextToCache(id);
    bool okDecodeNextImage = !abort && toCacheKey != -1 && isValidKey(toCacheKey);

    if (debugLog || G::isLogger || G::isFlowLogger)
    {
        log("fillCache decode next",
            "decoder " + QString::number(id).leftJustified(3) +
            "row = " + QString::number(toCacheKey).leftJustified(5) +
            "instance = " + QString::number(instance).leftJustified(5) +
            "decoder status = " + decoders[id]->statusText.at(decoders[id]->status).leftJustified(8) +
            // "isCacheUpToDate = " + QVariant(isCacheUpToDate).toString().leftJustified(6) +
            // "isNextToCache = " + QVariant(isNextToCache).toString().leftJustified(6) +
            "isValidKey(toCacheKey) = " + QVariant(isValidKey(toCacheKey)).toString().leftJustified(6) +
            "okDecodeNextImage = " + QVariant(okDecodeNextImage).toString().leftJustified(6) +
            "toCache: " + reportToCacheRows()
        );
    }
    if (debugCaching)
    {
        if (toCacheKey == -1) {
            QString fun = "ImageCache::fillCache done";
            qDebug().noquote() << fun.leftJustified(col0Width, ' ')
                               << "decoder" << QString::number(id).leftJustified(2)
                               << "status" << decoders[id]->status
                               << "NOTHING TO CACHE (cacheKey == -1)"
                ;
        }
        else {
            QString fun = "ImageCache::fillCache decode next";
            qDebug().noquote() << fun.leftJustified(col0Width, ' ')
                               << "decoder" << QString::number(id).leftJustified(3)
                               << "row =" << QString::number(toCacheKey).leftJustified(4)
                               << "isIdle =" << decoders[id]->isIdle()
                               << decoders[id]->fPath
                ;
        }
    }

    // decode the next image in the target range
    if (okDecodeNextImage) {
        if (!abort) decodeNextImage(id, toCacheKey);
    }

    // else caching available targets completed
    else {

        cycling[id] = false;

        if (debugCaching)
        {
        QString fun = "ImageCache::fillCache terminating";
        qDebug().noquote()
            << fun.leftJustified(col0Width, ' ')
            << "decoder" << QString::number(id).leftJustified(3)
            << "set to idle"
            ;
        }

        // is this the last active decoder?
        if (!anyDecoderCycling()) {

            // if empty image in ImCache then relaunch decoders
            if (nullInImCache()) {
                launchDecoders(src);
                return;
            }

            // else update cache progress in status bar
            if (isShowCacheStatus) {
                updateStatus(StatusAction::All, "ImageCache::fillCache after check cacheUpToDate");
            }

            // turn off caching activity lights on statusbar
            emit updateIsRunning(false, true);  // (isRunning, showCacheLabel)

            if (debugLog || G::isLogger || G::isFlowLogger)
                G::log("ImageCache::fillCache", "allDecodersDone");

            abort = false;

            if (debugCaching)
            {
                QString fun = "ImageCache::fillCache Terminated";
                qDebug().noquote()
                    << fun.leftJustified(col0Width, ' ')
                    ;
            }

        }
    }
}

void ImageCache::launchDecoders(QString src)
{
    QString fun = "ImageCache::launchDecoders";
    if (debugCaching)
    {
    qDebug().noquote() << fun.leftJustified(col0Width, ' ')
                       << "decoderCount =" << decoderCount
                       << "src =" << src;
    }

    for (int id = 0; id < decoderCount; ++id) {
        if (abort) return;
        if (toCache.isEmpty()) break;
        if (id >= dm->sf->rowCount()) break;
        if (cycling.at(id)) {
            if (debugCaching)
            {
                qDebug().noquote()
                    << fun.leftJustified(col0Width, ' ')
                    << "Decoder" << QVariant(id).toString().leftJustified(3)
                    << "skip because already cycling"
                    ;
            }
            continue;
        }
        // int sfRow = id;
        int sfRow = nextToCache(id);
        if (sfRow == -1) break;

        if (debugLog || G::isLogger || G::isFlowLogger) {
            QString msg = "decoder " + QString::number(id).leftJustified(3) +
                          "status = " + decoders[id]->statusText.at(decoders[id]->status)
                ;
            log("launchDecoders launch:", msg);
        }
        if (debugCaching)
        {
        qDebug().noquote()
            << fun.leftJustified(col0Width, ' ')
            << "Decoder" << QVariant(id).toString().leftJustified(3)
            << "row =" << QVariant(sfRow).toString().leftJustified(4)
            << "calling decodeNextImage"
            ;
        }

        cycling[id] = true;
        if (!abort) decodeNextImage(id, sfRow);
    }
}

void ImageCache::dispatch()
{
/*
    Called by a new file selection, cache size change, sort, filter or color manage change.
    The cache status is updated (current key, direction of travel, priorities and the target
    range) by calling fillCache with a decoder id = -1. Then each ready decoder is sent to
    fillCache. Decoders are assigned image files, which they decode into QImages, and then
    added to imCache. More details are available in the fillCache comments and at the top of
    this class.
*/
    if (debugCaching || G::isLogger || G::isFlowLogger)
        log("dispatch", "row = " + QVariant(currRow).toString());

    // req'd?
    if (dm->sf->rowCount() == 0) {
        // qDebug() << "ImageCache::dispatch dm->sf->rowCount() == 0 so return";
        return;
    }
    if (debugCaching)
    {
        qDebug().noquote() << "ImageCache::dispatch  row =" << currRow;
    }

    abort = false;

    // rgh req'd?
    if (!imageCacheThread.isRunning()) {
        // qDebug() << "ImageCache::dispatch imageCacheThread.start()";
        imageCacheThread.start();
    }

    // check available memory (another app may have used or released some memory)
    memChk();

    updateToCache();

    // if cache is up-to-date our work is done
    if (cacheUpToDate()) {
        // qDebug() << "ImageCache::dispatch cacheUpToDate == true so return";
        return;
    }

    // signal MW cache status
    emit updateIsRunning(true, true);   // (isRunning, showCacheLabel)

    if (!abort) launchDecoders("ImageCache::dispatch");
}
