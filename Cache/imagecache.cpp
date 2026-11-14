#include "Cache/imagecache.h"
#ifdef Q_OS_LINUX
#include "Utilities/linux.h"
#endif
#include "Main/global.h"
#include "Develop/workingimagecache.h"
#include "ImageFormats/Raw/rawformat.h"
#include "Utilities/ingestprobe.h"
#include "Utilities/icc.h"
#include <QFileInfo>
#include <QTimer>
#include <QSemaphore>
#include <memory>

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

    /*  Wake for deferred rows. DataModel posts rowsChanged (queued, so it reaches this
        thread's event loop) for every key this cache asked it to watch once that row's
        metadata lands. No thread ever blocks waiting for it. See Documentation.md
        4.4.10. */
    connect(dm, &DataModel::rowsChanged, this, &ImageCache::onRowsChanged);
    connect(dm, &DataModel::proxySnapshotPublished, this,
            &ImageCache::onSnapshotPublished, Qt::QueuedConnection);

    // data is kept in ImageCacheData icd, a hash table
    this->icd = icd;
    this->dm = dm;

    maxMB = 1000;  // maxMB change

    // create n decoder threads
    decoderCount = QThread::idealThreadCount();
    decoderMetadatas.reserve(decoderCount);
    for (int id = 0; id < decoderCount; ++id) {
        // Each decoder owns its Metadata: parser state (IFD hash, ImageMetadata, etc.) cannot be shared across threads.
        Metadata *decoderMetadata = new Metadata;
        decoderMetadatas.append(decoderMetadata);
        ImageDecoder *decoder = new ImageDecoder(id, dm, decoderMetadata);
        QThread *thread = new QThread;

        decoder->moveToThread(thread);
        connect(decoder, &ImageDecoder::done, this, &ImageCache::fillCache);
        /* Relay the decoder's RAW demosaic progress to MW (drops sfRow; MW gates on the
           current file path). */
        connect(decoder, &ImageDecoder::demosaicProgress, this,
                [this](int, QString fp, int d, int t){ emit demosaicProgress(fp, d, t); });
        connect(thread, &QThread::finished, decoder, &QObject::deleteLater);
        connect(thread, &QThread::finished, thread, &QObject::deleteLater);

        thread->start();
        decoders.append(decoder);
        decoderThreads.append(thread);
        cycling.append(false);
        decoderIsRaw.append(false);
        decoderPath.append(QString());
        decoderGeneration.append(0);
    }

    pressureHistory.reserve(50);    // avoid reallocations

    abort = false;
    debugCaching = false;        // turn on local qDebug
    debugLog = false;            // invoke log without G::isLogger or G::isFlowLogger
}

ImageCache::~ImageCache()
{
    stop();
    qDeleteAll(decoderMetadatas);
    decoderMetadatas.clear();
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
    QString srcFun = "ImageCache::stop";
    if (debugCaching)
    {
        qDebug().noquote()
            << srcFun.leftJustified(col0Width, ' ')
            << "isRunning =" << imageCacheThread.isRunning()
            ;
    }
    if (debugLog || G::isLogger)
        log("stop", "isRunning = " + QVariant(imageCacheThread.isRunning()).toString());

    abort = true;

    /* Signal abort to every decoder first (non-blocking; just sets each decoder's atomic
       flag, safe to call cross-thread). An in-flight decode polls this flag -- e.g. the RAW
       demosaic checks it per row -- and returns promptly, so the BlockingQueuedConnection
       below does not have to wait for a full decode to finish (which beachballs the UI). */
    for (int id = 0; id < decoderCount; ++id) {
        decoders[id]->abortProcessing();
    }

    // Stop all decoder threads first
    for (int id = 0; id < decoderCount; ++id) {
        QMetaObject::invokeMethod(decoders[id], "stop", Qt::BlockingQueuedConnection);
        // decoders[id]->stop();
    }

    if (imageCacheThread.isRunning()) {
        imageCacheThread.quit();
        imageCacheThread.wait();
    }

    if (isIdle()) {
        emit stopped(srcFun);
    }

    // turn off caching activity lights on statusbar
    // emit updateIsRunning(false, false);  // flags = isRunning, showCacheLabel
}

bool ImageCache::instanceClash(int id)
{
    /*  See the declaration for why the parameter type matters. Bounds-checked because an
        out-of-range id now reaches the subscript instead of being folded to 0 or 1;
        "clash" is the safe answer, since it only refuses an image. */
    if (id < 0 || id >= decoders.count()) return true;
    return decoders[id]->instance != G::dmInstance;
}

void ImageCache::abortProcessing()
{
    if (G::isLogger || G::isFlowLogger)
    {
        QString isGUI = QVariant(G::isGuiThread()).toString();
        G::log("ImageCache::abortProcessing",
               "starting, isGUI thread = " + isGUI);
    }

    abort = true;
    for (int id = 0; id < decoderCount; ++id) {
        cycling[id] = false;
        decoders[id]->abortProcessing();
    }

    // make sure caching status is up-to-date
    for (auto it = toCacheStatus.begin(); it != toCacheStatus.end(); ++it) {
        if (it.value().isCaching) {
            const QString cleanPath = pathAt(it.key());
            setStateCaching(cleanPath, false, -1);
            emit setValKey(cleanPath, G::IsCachingColumn, false, instance, "abort cleanup");
            it.value().isCaching = false;
        }
    }

    if (G::isLogger || G::isFlowLogger)
    G::log("ImageCache::abortProcessing", "emit stopped");

    emit stopped("ImageCache");
}

bool ImageCache::isIdle()
{
    if (isInitializing) return false;
    for (int id = 0; id < decoderCount; ++id) {
        if (decoders[id]->isBusy()) return false;
    }
    return true;
}

bool ImageCache::isRunning() const
{
    return imageCacheThread.isRunning();
}

bool ImageCache::anyDecoderCycling()
{
    for (int id = 0; id < decoderCount; ++id) {
        if (cycling.at(id)) return true;
    }
    return false;
}

void ImageCache::debugRunStatus()
{
    for (int id = 0; id < decoderCount; ++id) {
        auto decoder = decoders.at(id);
        qDebug() << "    - Decoder " << id << ":" << (decoder->isRunning() ? "RUNNING" : "Stopped")
                 << " | isIdle:" << decoder->isIdle();
    }
}

quint64 ImageCache::getImCacheSize()
{
    // return the current size of the cache
    if (debugLog || G::isLogger) log("getImCacheSize");

    // the byte total is kept by ImageCacheData (atomic): no walk of the hash, no lock
    const quint64 cacheMB = static_cast<quint64>(icd->sizeMB());

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
    if (key > -1 && key < rowCountSf()) return true;
    else return false;
}

/*  ---- The only reads of the model from this thread ---- */

void ImageCache::adoptSnapshot()
{
/*
    A SORT KEPT THE INSTANCE, SO THE QUEUE KEPT ITS ROW NUMBERS (C21). toCache and
    toCacheStatus hold snapshot rows, and only an instance change cleared them; after a
    re-sort (or a filter, or a row insert) the queued rows named other images: an
    in-flight decode's isCaching blocked the image that moved into its row, and
    toCacheRemove dropped that image from the queue. Results already file by the
    decoder's key (4.4.13), and in-flight decodes are tracked by path (cacheState), so
    the row-numbered queue is the only thing that can be wrong. If any queued row names
    a different image in the new snapshot, drop the queue: setTargetRange rebuilds it
    from the current path at once.
*/
    const ProxySnapshotPtr fresh = dm->proxySnapshot();
    if (!fresh || fresh == queueSnap) return;
    const ProxySnapshotPtr old = queueSnap;
    queueSnap = fresh;
    if (!currPath.isEmpty()) currRow = fresh->sfRow(currPath);
    if (!old) return;
    bool moved = false;
    for (int r : std::as_const(toCache))
        if (old->path(r) != fresh->path(r)) { moved = true; break; }
    if (!moved)
        for (auto it = toCacheStatus.cbegin(); it != toCacheStatus.cend(); ++it)
            if (old->path(it.key()) != fresh->path(it.key())) { moved = true; break; }
    if (!moved) return;
    toCache.clear();
    toCacheStatus.clear();
    targetFirst = targetLast = currRow;
}

QVariant ImageCache::sfVal(int sfRow, int col, int role) const
{
/*
    A model value by SNAPSHOT row, for the diagnostic reports: the snapshot maps the row
    to its DataModel row, and RowStore reads are locked, so this is safe on any thread.
    The reports used dm->sf (GUI only) and, in places, dm->index(sfRow, ...) -- a proxy
    row used as a model row, i.e. another image's value whenever a filter was on.
*/
    auto snap = qsnap();
    const int dmRow = snap ? snap->dmRow(sfRow) : -1;
    if (dmRow < 0) return QVariant();
    // follow the key: the model may have moved rows since this snapshot
    const int live = dm->rowForKey(dmRow, snap->path(sfRow));
    if (live < 0) return QVariant();
    return dm->index(live, col).data(role);
}

QString ImageCache::pathAt(int sfRow) const
{
    auto snap = qsnap();
    return snap ? snap->path(sfRow) : QString();
}

int ImageCache::sfRowOf(const QString &fPath) const
{
    auto snap = qsnap();
    return snap ? snap->sfRow(fPath) : -1;
}

int ImageCache::rowCountSf() const
{
    auto snap = qsnap();
    return snap ? snap->rowCount() : 0;
}

/*  A row's live flags through the snapshot's dmRow, only while the two describe the same
    rows (ModelView::consistent, Documentation 4.4.15). RowSync is spliced the moment
    rows move and the snapshot follows a turn later; in between, the snapshot's dmRow
    read another image's flags, and a still flagged as video was marked cached (C9).
    Inconsistent answers are the conservative ones: not a video, not loaded, no size;
    okToDecode then defers the row (Structure) until the next snapshot. */
bool ImageCache::isVideoAt(int sfRow) const
{
    const ModelView mv{qsnap(), dm->rowSync()};
    if (!mv.consistent()) return false;
    return mv.sync->has(mv.snap->dmRow(sfRow), RowSync::IsVideo);
}

bool ImageCache::metaLoadedAt(int sfRow) const
{
    const ModelView mv{qsnap(), dm->rowSync()};
    if (!mv.consistent()) return false;
    return mv.sync->has(mv.snap->dmRow(sfRow), RowSync::MetaLoaded);
}

float ImageCache::cacheMBAt(int sfRow) const
{
    const ModelView mv{qsnap(), dm->rowSync()};
    if (!mv.consistent()) return 0.0f;
    return mv.sync->cacheMB(mv.snap->dmRow(sfRow));
}

/*  ---- Per-image cache bookkeeping (see RowCache in imagecache.h) ----

    Every accessor takes the mutex. It is uncontended in practice: only this
    thread writes, and only the GUI-thread diagnostic reports ever read from
    outside. Correctness first -- these were racing on the datamodel before.
*/
ImageCache::RowCache ImageCache::stateOf(const QString &fPath) const
{
    QMutexLocker lock(&cacheStateMutex);
    return cacheState.value(fPath);
}

void ImageCache::setStateCaching(const QString &fPath, bool on, int decoderId)
{
    if (fPath.isEmpty()) return;
    QMutexLocker lock(&cacheStateMutex);
    RowCache &r = cacheState[fPath];
    r.isCaching = on;
    if (on) r.decoderId = decoderId;
}

void ImageCache::setStateCached(const QString &fPath, bool on)
{
    if (fPath.isEmpty()) return;
    QMutexLocker lock(&cacheStateMutex);
    RowCache &r = cacheState[fPath];
    r.isCached = on;
    if (on) r.isCaching = false;
}

int ImageCache::bumpStateAttempts(const QString &fPath)
{
    if (fPath.isEmpty()) return 0;
    QMutexLocker lock(&cacheStateMutex);
    return ++cacheState[fPath].attempts;
}

void ImageCache::unbumpStateAttempts(const QString &fPath)
{
    if (fPath.isEmpty()) return;
    QMutexLocker lock(&cacheStateMutex);
    auto it = cacheState.find(fPath);
    if (it != cacheState.end() && it->attempts > 0) --it->attempts;
}

void ImageCache::setStateStatus(const QString &fPath, int status, const QString &errMsg)
{
    if (fPath.isEmpty()) return;
    QMutexLocker lock(&cacheStateMutex);
    RowCache &r = cacheState[fPath];
    r.status = status;
    if (!errMsg.isNull()) r.errMsg = errMsg;
}

void ImageCache::clearStateFor(const QString &fPath)
{
    QMutexLocker lock(&cacheStateMutex);
    cacheState.remove(fPath);
}

void ImageCache::clearAllState()
{
    QMutexLocker lock(&cacheStateMutex);
    cacheState.clear();
    supersededDecodes.clear();
    capLifted.clear();          // ImageCache thread, as are both callers
}

bool ImageCache::takeSupersededDecode(const QString &fPath)
{
    QMutexLocker lock(&cacheStateMutex);
    return supersededDecodes.remove(fPath);
}

void ImageCache::onRowsChanged(QStringList keys, int aspects)
{
/*
    DataModel posts this when the metadata of a watched key has been written (see
    DataModel::watchRowLoaded). Clear the deferral and dispatch so the row is decoded
    now. Instance changes need no test: the keys are paths, and dispatch() adopts the
    model's instance itself.
*/
    if (G::isLogger || G::isFlowLogger)
        log("onRowsChanged", QString::number(keys.size()) + " keys");
    if (!(aspects & DataModel::AspectMetadata)) return;
    if (!deferred.wake(keys)) return;
    if (abort || isInitializing || G::removingRowsFromDM) return;
    deferred.noteProgress();
    if (!cacheUpToDate()) dispatch();
}

void ImageCache::onSnapshotPublished()
{
/*
    A snapshot that may now match the model. A position waiting for one is retried at
    once rather than on its 10 ms timer; rows deferred because the snapshot was behind
    the model's rows (Structure) are re-evaluated.
*/
    if (!pendingPositionPath.isEmpty()) {
        setCurrentPosition(pendingPositionPath, pendingPositionSrc);
        return;
    }
    if (!deferred.wakeReason(DeferredRows::Structure)) return;
    if (abort || isInitializing || G::removingRowsFromDM) return;
    deferred.noteProgress();
    if (!cacheUpToDate()) dispatch();
}

void ImageCache::armIdleBackstop()
{
/*
    Called when the cache goes idle. If rows are still queued, every one of them was
    deferred or refused, and nothing may ever wake them (a geometry deferral has no wake;
    a torn RowSync read can report "Metadata read failed" for a row a moment from
    loaded). One bounded timer retries: see DeferredRows.
*/
    if (abort) return;
    if (!deferred.arm(!toCache.isEmpty() && !cacheUpToDate())) return;
    QTimer::singleShot(DeferredRows::kBackstopMs, this, &ImageCache::idleBackstop);
}

void ImageCache::idleBackstop()
{
    const bool busy = abort || isInitializing || G::removingRowsFromDM;
    if (!deferred.fire(!busy && !toCache.isEmpty() && !cacheUpToDate())) return;
    if (G::isLogger || G::isFlowLogger)
        log("idleBackstop", "fire " + QString::number(deferred.firesSinceProgress()));
    dispatch();
}

void ImageCache::updateToCache()
{
/*
    Called by dispatch().  dispatch() is called by
        - setCurrentPosition()
        - setMaxMB()
        - refreshImageCache()
        - reloadImageCache()

    This is running in the ImageCache thread.
*/
    checkCacheThread("ImageCache::updateToCache");
    if (debugLog || G::isLogger || G::isFlowLogger)
        log("updateToCache");

    QString fun = "ImageCache::updateToCache";
    if (debugCaching)
    {
        qDebug().noquote() << fun.leftJustified(col0Width, ' ')
                           << "current position =" << currRow
                           << "total =" << rowCountSf()
            ;
    }

    QMutexLocker locker(&gMutex);  // req'd to prevent toCacheAppend() crash

    adoptSnapshot();                // the queue's rows mean this snapshot's (4.4.18)
    if (!abort) setDirection();
    if (!abort) setTargetRange(currRow);
    if (!abort) trimOutsideTargetRange();

    if (instance != dm->instance) {
        instance = dm->instance;
    }
    resetStaleIsCaching();

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

    /*  The keys are copied out first: deciding, mirroring and emitting all happen with
        icd's lock released (an emit under it ran receivers with the lock held). */
    const QStringList cachedKeys = icd->keys();
    for (const QString &fPath : cachedKeys) {
        const int sfRow = sfRowOf(fPath);

        // Not in datamodel anymore OR outside target range
        const bool notInModel = !isValidKey(sfRow);          // (sfRow == -1)
        const bool outOfRange = !notInModel && (sfRow < targetFirst || sfRow > targetLast);

        if (notInModel || outOfRange) {
            keysToRemove.append(fPath);
            removedFromCache.append(sfRow);
            setStateCached(fPath, false);
            emit setCached(sfRow, fPath, G::CacheNone, instance);
        }
    }

    // Perform removals using the API (write-locked inside remove())
    for (const auto &key : keysToRemove) {
        icd->remove(key);
        trimmedCount.fetch_add(1, std::memory_order_relaxed);
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

inline void ImageCache::decodeHistory(qint64 nsToDecode)
{
    // If this runs on a single ImageCache thread, no lock needed.
    // If multiple threads may call it, guard with a mutex around this block.

    if (Q_UNLIKELY(decodeMsAvg.window() != lastNDecoders))
        decodeMsAvg.setWindow(lastNDecoders);

    // Convert ns->ms here so the moving average and decoderMs are genuine ms.
    const int ms = static_cast<int>(nsToDecode / 1000000);
    decodeMsAvg.push(ms);
    decoderMs = qMax(1, decodeMsAvg.avg());
}

qint64 ImageCache::nowMs() { return QDateTime::currentMSecsSinceEpoch(); }

inline void ImageCache::noteItemSizeMB(float szMB) {
    if (szMB <= 0) return;
    emaItemMB = (emaItemMB < 0) ? szMB : (0.8f * emaItemMB + 0.2f * szMB);
    emaItemMB *= 2;
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
    /*
    qDebug() << "ImageCache::isRapidForward "
             << "currRow =" << currRow
             << "isForward =" << isForward
             << "forwardStreak =" << forwardStreak
             << "rapidMinStreak =" << rapidMinStreak
             << "emaStepMs =" << emaStepMs
             << "rapidStepMsThreshold =" << rapidStepMsThreshold
                ;//*/
    return isForward &&
           forwardStreak >= rapidMinStreak &&
           emaStepMs > 0.0 &&
           emaStepMs <= rapidStepMsThreshold;
}

inline quint64 ImageCache::calcResizeStepMB() const {
    // aim for a few images worth, but stay within min/max
    const quint64 byItems = (emaItemMB > 0) ? quint64(std::ceil(emaItemMB * 3.0f)) : 0;
    quint64 step = qMax(minStepMB, byItems);
    step = qMin(step, maxStepMB);
    return step;
}

void ImageCache::relievePressure() {
/*
The cushion is the number of images cached ahead of the current position. Pressure
increases as the size of the cushion decreases.  As pressure changes the maximum
cache size is adjusted.

Cache size adjustments are moderated to avoid making unnecessay adjustments based
on how quickly the user is progressing ahead (key mashing) and how recently the cache
was adjusted.

isRapidForward(): How fast the user is progressing ahead. If the user is progressing
slowly then there isn't any urgency to update the cache.

adjustCooldownMs: The minimum delay before making another cache size adjustment. This
moderates "twitchy" behavior in the cache size.

ignorePressureRestraints: Make an adjustment, speed and delay.  This is used at the
start of a new folder(s) and when the target range has been cached to trigger both
initial and final cache adjustments.

cushion: The number of images in the cache ahead of the current position.

cushionLow: The desired minimum number of images in the cached ahead of the current
position.  If the cushion is less then cuchionLow then make a cache size bigger.

cushionHigh: The maximum number of images in the cached ahead of the current
position.  If the cushion is more then cuchionLow then make a cache size smaller.

*/
    const qint64 t = nowMs();

    // Gate by “rapid forward” unless we still have the first-adjust free pass
    const bool rapidOk = isRapidForward();
    qint64 elapsedMs = t - lastAdjustMs;
    const bool isCooldown = elapsedMs > adjustCooldownMs;
    bool makeAnAdjustment = ignorePressureRestraints ||
                            (rapidOk && isCooldown);
    // bool makeAnAdjustment = ignorePressureRestraints ||
    //                         (rapidOk && isCooldown) ||
    //                         autoStrategy == Greedy;
    /*
    qDebug() << "ImageCache::releavePressure"
             << "currRow =" << currRow
             << "t =" << t
             << "lastAdjustMs =" << lastAdjustMs
             << "t - lastAdjustMs =" << t - lastAdjustMs
             << "elapsedMs =" << elapsedMs
             << "firstAdjustFreePass =" << firstAdjustFreePass
        ;//*/
    qint64 stepMB = 0;
    pressureItem.highChk = false;
    pressureItem.lowChk = false;

    if (makeAnAdjustment) {
        // No flip-flopping: only grow on low cushion, shrink on very high cushion
        if (cushion <= cushionLow) {
            pressureItem.highChk = true;
            stepMB = calcResizeStepMB();
            const qint64 newMax = qMin(maxMBCeiling, maxMB + stepMB);

            /*
            qDebug() << "ImageCache::releavePressure high check"
                     << "step =" << step
                     << "maxMB =" << maxMB
                     << "newMax =" << newMax
                ;//*/
            if (newMax != maxMB) {
                maxMB = newMax;
                lastAdjustMs = t;
                /*
                qDebug() << "ImageCache::releavePressure high"
                         << "cushion =" << cushion
                         << "step =" << stepMB
                         << "maxMB =" << maxMB
                    ; //*/
                updateStatus(StatusAction::All, "ImageCache::releasePressure");
            }
        }
        else if (cushion > cushionHigh && autoStrategy != Greedy) {
            pressureItem.lowChk = true;
            stepMB = calcResizeStepMB();
            const quint64 newMax = qMax(minMB, maxMB - stepMB);
            /*
            qDebug() << "ImageCache::releavePressure low check"
                     << "stepMB =" << stepMB
                     << "minMB =" << minMB
                     << "maxMB =" << maxMB
                     << "maxMB - stepMB =" << maxMB - stepMB
                     << "newMax =" << newMax
                ;//*/
            if (newMax != maxMB) {
                maxMB = newMax;
                lastAdjustMs = t;
                /*
                qDebug() << "ImageCache::releavePressure low "
                         << "currentPressure =" << currentPressure
                         << "stepMB =" << stepMB
                         << "maxMB =" << maxMB
                    ;//*/
                updateStatus(StatusAction::All, "ImageCache::releasePressure");
            }
        }
        // otherwise stay put and keep observing
    }
    pressureItem.sfRow = currRow;
    pressureItem.isJustDoIt = ignorePressureRestraints;
    pressureItem.isRapidForward = rapidOk;
    // Use -1 sentinel when there has been no prior adjust — the raw delta
    // would otherwise be ~nowMs() (epoch ms ≈ 1.7e12).
    pressureItem.elapsedMs = (lastAdjustMs > 0) ? elapsedMs : -1;
    pressureItem.isCooldown = isCooldown;
    pressureItem.tSinceMoveMs = lastMoveMs > 0 ? (t - lastMoveMs) : -1;
    pressureItem.cushion = cushion;
    pressureItem.stepMB = stepMB;
    pressureItem.ceilMB = maxMBCeiling;
    pressureItem.cacheMB = icd->sizeMB();
    pressureItem.maxMB = maxMB;

    pressureHistory.append(pressureItem);
    if (pressureHistory.size() > 50) pressureHistory.removeFirst();

    ignorePressureRestraints = false;
}

// --- End Cache Pressure ---

void ImageCache::setTargetRange(int key)
{
/*
    The target range is the list of images being targeted to cache, based on the current
    image, the direction of travel, and the maximum memory allotted to the image cache.
    The DataModel is iterated to populate the toCache list of images to cache.

    • The direction of travel (dot) isForward boolean is calculated to indicate whether
      the caching should proceed forward or backward from the current position (key).

    • The terms ahead and behind are relative to the direction of travel.  If the direction
      of travel is forward, then ahead refers to rows greater than the key, while if the
      direction of travel is not forward, then ahead refers to rows less than the key.

    • aheadPos and behindPos specify positions ahead and behind the key, respectively.

    • The start and end of the target range (targetFirst and targetLast) are assigned when
      the cache allocated memory (maxMB) has been reached or all items in the DataModel
      have been iterated.  targetFirst and targetLast are used in trimOutsideTargetRange
      and showCacheStatus.

    • If isAutoMaxMB is true, then instead of arbitrarily setting the maximum memory
      allotted to the image cache, the target range is set based on maintaining a cushion
      of n images in the ahead direction.  This is managed in releavePressure().
*/
    if (debugLog || G::isLogger)
        log("setTargetRange", "row = " + QVariant(key).toString());

    if (abort) return;

    const int n = rowCountSf();
    if (n <= 0 || key < 0 || key >= n) return;

    // bool useHighCushion = false;

    // float sumMB = icd->sizeMB();
    float sumMB = 0.0f;
    // Forward phase aims for ~2/3 of capacity
    const float forwardMB = maxMB * (2.0f / 3.0f);
    int nearestNotCached = -1;
    targetFirst = key;
    targetLast = key;

    // If folders changed, reset target queues
    if (instance != dm->instance) {
        toCache.clear();
        toCacheStatus.clear();
        instance = dm->instance;
        resetStaleIsCaching();
    }

    // Direction helpers
    const int dir = isForward ? +1 : -1;

    // Positions: ahead starts at key, behind starts one step opposite
    int aheadPos  = key;
    int behindPos = key - dir;   // if dir=+1 -> key-1; if dir=-1 -> key+1

    // Auto counting
    int count = 0;
    targetCount = cushionHigh * 1.5;

    float imgMB;

    // Function to add to target range and to cache queue
    auto addToQueue = [&](int pos) {

        // adjust target range
        (pos < key) ? targetFirst = pos : targetLast = pos;

        const QString fPath = pathAt(pos);

        // Videos: mark cached; do not count toward sumMB
        if (isVideoAt(pos)) {
            setStateCached(fPath, true);
            emit setCached(pos, fPath, G::CacheOriginal, instance);
            return;
        }

        /* Invalid image: OUR state says cached, so fillCache does not loop on it and it
           does not count toward sumMB. The MODEL is told CacheNone, because nothing is in
           icd: CacheOriginal reached MW::refreshViewsOnCacheChange as "cached", the loupe
           loaded the current image from icd, missed, and set an empty pixmap -- a blank
           loupe for a file the cache had merely given up on (2026-11-10). */
        if (stateOf(fPath).status == ImageDecoder::Status::Invalid) {
            setStateCached(fPath, true);
            emit setCached(pos, fPath, G::CacheNone, instance);
            return;
        }

        imgMB = cacheMBAt(pos);
        sumMB += imgMB;
        count++;

        // If not already queued to cache, then add to queue
        if (!toCache.contains(pos) && !icd->contains(fPath)) {
            /*
            QString msg = "pos = " + QVariant(pos).toString();
            msg += " posMB = " + QVariant(posMB).toString();
            msg += " sumMB = " + QVariant(sumMB).toString();
            msg += " maxMB = " + QVariant(maxMB).toString();
            log("setTargetRange addToQueue", msg);
            //*/
            toCacheAppend(pos);
        }

        // Get pressure on cache
        if (isAutoMaxMB) {
            if (toCache.contains(pos) && nearestNotCached == -1) {
                noteItemSizeMB(imgMB);
                nearestNotCached = pos;
                cushion = qAbs(nearestNotCached - key);

                /*
                qDebug() << "ImageCache::setTargetRange"
                         << "pos =" << pos
                         << "cushion =" << cushion
                         << "cushion =" << cushion
                         << "targetFirst =" << targetFirst
                         << "targetLast =" << targetLast
                         << "maxMB =" << maxMB
                    ; //*/
            }
        }
    };

    /* Develop mode: view/edit a SINGLE image at best quality. Forward read-ahead is a low priority
       here -- and each neighbour decode caches its own scene-linear WorkingImage, evicting the one
       the Develop pipeline needs (WorkingImageCache is small relative to a 50-60MP image). So target
       ONLY the current image: no ahead/behind, nothing to churn or evict. Preview mode keeps the
       full forward cache below. */
    if (G::operationMode == G::OperationMode::Develop) {
        addToQueue(key);
        firstDispatchNewDM = false;
        return;
    }

    if (isAutoMaxMB) {
        // Fill forward up to ~2/3 capacity
        while (!abort && count <= cushionHigh && sumMB < forwardMB) {
            if (aheadPos < 0 || aheadPos >= n) break;
            addToQueue(aheadPos);
            aheadPos += dir;
        }

        // Fill behind until total reaches maxMB
        while (!abort && count < targetCount && sumMB < maxMB) {
            if (behindPos < 0 || behindPos >= n) break;
            addToQueue(behindPos);
            behindPos -= dir;
        }

        // Fill forward again if still not full (at start/end of folder when behind is smaller)
        while (!abort && count < targetCount && sumMB < maxMB) {
            if (aheadPos < 0 || aheadPos >= n) break;
            addToQueue(aheadPos);
            aheadPos += dir;
        }

        // update motion heuristics and try pressure relief
        if (nearestNotCached == -1) {
            isForward ? nearestNotCached = targetLast : nearestNotCached = targetFirst;
            imgMB = cacheMBAt(nearestNotCached);
            noteItemSizeMB(imgMB);
            cushion = qAbs(nearestNotCached - key);
        }
        memChk();
        updateMotion(key, isForward);
        relievePressure();
    }
    else {
        // Fill forward up to ~2/3 capacity
        while (!abort && sumMB < forwardMB) {
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

        // Fill forward again if still not full (at start/end of folder when behind is smaller)
        while (!abort && sumMB < maxMB) {
            if (aheadPos < 0 || aheadPos >= n) break;
            addToQueue(aheadPos);
            aheadPos += dir;
        }
    }

    /*
    QString msg = "row = " + QVariant(key).toString();
    msg += " n = " + QVariant(n).toString();
    msg += " behindPos = " + QVariant(behindPos).toString();
    msg += " aheadPos = "  + QVariant(aheadPos).toString();
    msg += " targetFirst = " + QVariant(targetFirst).toString();
    msg += " targetLast = " + QVariant(targetLast).toString();
    log("setTargetRange", msg);
    //*/

    firstDispatchNewDM = false;
}

void ImageCache::markSuperseded(const QStringList &keys)
{
    QMutexLocker lock(&cacheStateMutex);
    for (const QString &k : keys) if (!k.isEmpty()) supersededDecodes.insert(k);
}

QSet<QString> ImageCache::inFlightKeys() const
{
    QSet<QString> keys;
    for (int id = 0; id < decoderPath.size(); ++id)
        if (cycling.at(id) && !decoderPath.at(id).isEmpty()) keys.insert(decoderPath.at(id));
    return keys;
}

void ImageCache::settleSuperseded(const QStringList &keys)
{
    const QSet<QString> inFlight = inFlightKeys();
    QMutexLocker lock(&cacheStateMutex);
    for (const QString &k : keys) if (!inFlight.contains(k)) supersededDecodes.remove(k);
}

void ImageCache::invalidate(const QStringList &keys, const QString &src, bool refill)
{
/*
    ANY THREAD -- see the declaration. The two things that must be true the moment this
    returns are done here, under their own locks: the images are out of icd, and a
    decode already running for one of them is marked superseded. The rest is ImageCache
    thread state and is posted there.

    This replaced five direct icd->remove() calls on the GUI thread (refreshStaleRows,
    devPreviewUpdated, the devPreview builder, deleting a version, applyModelChange via
    removeCachedImage, which also ran ImageCache code on the GUI thread). They left the
    per-path state and the model's badge saying "cached", queued nothing, and let a
    decode in flight land the old picture afterwards (C5, Documentation 4.4.13).
*/
    if (keys.isEmpty()) return;
    if (debugLog || G::isLogger || G::isFlowLogger)
        log("invalidate", QString::number(keys.size()) + " keys from " + src);
    markSuperseded(keys);
    for (const QString &k : keys) icd->remove(k);
    QMetaObject::invokeMethod(this, [this, keys, src, refill] {
        onInvalidated(keys, src, refill);
    }, Qt::QueuedConnection);
}

void ImageCache::onInvalidated(const QStringList &keys, const QString &src, bool refill)
{
    checkCacheThread("ImageCache::onInvalidated");
    settleSuperseded(keys);
    bool inModel = false;
    for (const QString &k : keys) {
        /*  A fresh start for the new content: attempts, status (an Invalid that was
            really the old file) and isCached. The supersede mark is separate and stays
            until the decode it is waiting for lands. */
        clearStateFor(k);
        capLifted.remove(k);
        deferred.wake({k});
        const int sfRow = sfRowOf(k);
        if (sfRow >= 0) inModel = true;
        emit setCached(sfRow, k, G::CacheNone, instance);     // the badge: not cached
    }
    if (!refill || !inModel) return;
    if (abort || isInitializing || G::removingRowsFromDM) return;
    /*  updateToCache -> setTargetRange queues every row in the target the cache does not
        hold, in priority order, so the current image comes first. */
    dispatch();
    Q_UNUSED(src)
}

void ImageCache::replace(const QString &key, const QImage &image)
{
/*
    ANY THREAD. A decode in flight for key read the old orientation: superseded, and
    re-queued when it lands (where okToDecode finds the replaced image and skips it).
*/
    if (key.isEmpty() || image.isNull()) return;
    markSuperseded({key});
    icd->replaceIfPresent(key, image);
    QMetaObject::invokeMethod(this, [this, key] { settleSuperseded({key}); },
                              Qt::QueuedConnection);
}

void ImageCache::bumpGeneration(const QString &src)
{
    ++generation;
    if (debugLog || G::isLogger || G::isFlowLogger)
        log("bumpGeneration", QString::number(generation) + " from " + src);
}

void ImageCache::checkCacheThread(const char *fun)
{
    if (QThread::currentThread() == &imageCacheThread) return;
    if (!imageCacheThread.isRunning()) return;      // start-up, before the thread runs
    G::issueDedup("Error", QString(fun) + " ran off the ImageCache thread",
                  "ImageCache::checkCacheThread");
}

QString ImageCache::fromCacheThread(std::function<QString()> fn, int timeoutMs)
{
    if (QThread::currentThread() == &imageCacheThread || !imageCacheThread.isRunning())
        return fn();
    /*  Shared, so a report that finishes after the wait gave up writes into memory that
        still exists. */
    struct Call { QSemaphore done; QString result; };
    auto call = std::make_shared<Call>();
    QMetaObject::invokeMethod(this, [call, fn] {
        call->result = fn();
        call->done.release();
    }, Qt::QueuedConnection);
    if (!call->done.tryAcquire(1, timeoutMs))
        return QString("\n(ImageCache thread busy for %1 ms: report not available)\n")
            .arg(timeoutMs);
    return call->result;
}

void ImageCache::rename(QString oldPath, QString newPath)
/*
    Called by RenameFileDlg (and for each version it re-keys) on the GUI thread, after the
    file is renamed on disk. Carries the decoded image AND our bookkeeping to the new
    path: left under the old one, the new path started with no status or attempts, and
    the old entry lingered, describing a file that no longer exists.
*/
{
    if (debugLog || G::isLogger) log("rename");
    if (oldPath == newPath) return;
    icd->rename(oldPath, newPath);

    QMutexLocker lock(&cacheStateMutex);
    auto it = cacheState.find(oldPath);
    if (it != cacheState.end()) {
        const RowCache r = it.value();
        cacheState.erase(it);
        cacheState.insert(newPath, r);
    }
    if (supersededDecodes.remove(oldPath)) supersededDecodes.insert(newPath);
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

void ImageCache::resetStaleIsCaching()
{
/*
    Clear any isCaching marker left over from a previous pass. Reads our OWN
    state rather than scanning the proxy for the column: the model's copy is a
    display mirror that may not have caught up, and scanning it from this thread
    was a full-model read off the GUI thread on every target-range change.
*/
    /*  NOT A DECODE THAT IS STILL RUNNING. This runs on every dispatch, and clearing
        isCaching under a live decode left the model saying "not caching" while it was,
        and the next okToDecode free to start a second decode of it (C15). */
    const QSet<QString> inFlight = inFlightKeys();
    QStringList caching;
    {
        QMutexLocker lock(&cacheStateMutex);
        for (auto it = cacheState.cbegin(); it != cacheState.cend(); ++it)
            if (it.value().isCaching && !inFlight.contains(it.key())) caching << it.key();
    }
    for (const QString &fPath : caching) {
        setStateCaching(fPath, false, -1);
        emit setValKey(fPath, G::IsCachingColumn, false, instance,
                       "ImageCache::resetStaleIsCaching");
    }
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
    /*
    const auto ramMB = Mac::totalMemoryMB();
    const auto procMB = Mac::processFootprintMB();
    const auto availMB = Mac::aggressiveAvailMB(); // free + inactive + speculative
    const int  level = Mac::memoryPressureLevel();
    const quint64 cacheMB = (quint64)icd->sizeMB();
    //*/
    Mac::availableMemory();
#endif
#ifdef Q_OS_LINUX
    Linux::availableMemory();
#endif

    /*
    // Small safety reserve: ~3% of RAM (min 256 MB, max 1024 MB)
    int64_t safetyMB = std::clamp<int64_t>((int64_t)(ramMB * 0.03), 256, 1024);

    // Hard cap: never plan to exceed RAM - safety
    const int64_t hardCapFromRAM = std::max<int64_t>(0, ramMB - safetyMB);

    // Opportunistic burst cap: use ~95% of currently reclaimable memory + what we already hold
    // (so we can expand rapidly without forcing immediate eviction elsewhere)
    const int64_t burstCapFromAvail = std::max<int64_t>(0, (int64_t)(availMB * 0.95) + cacheMB);

    // Aggressive ceiling = the *smaller* of the two caps above
    qint64 ceiling = std::min(hardCapFromRAM, burstCapFromAvail);

    // Under OS pressure, clamp hard and immediately
    if (level == 2) {           // CRITICAL
        ceiling = std::min<int64_t>(ceiling, 256);   // slam to 256 MB
    }
    else if (level == 1) {      // WARN
        ceiling = std::min<int64_t>(ceiling, 1024);  // downshift to ~1 GB
    }

    // Ensure we never go below your configured minimums (if any)
    ceiling = std::max<qint64>(ceiling, minMB);

    // Set the target TOTAL budget for the cache
    maxMBCeiling = ceiling;
    //*/

    maxMBCeiling = std::max<qint64>(G::availableMemoryMB, minMB) * memThrottle;

    /* Clamp the ceiling so the image cache never plans past the hard watchdog cap
       (G::memoryAbortMB). The availableMemoryMB-derived ceiling above knows nothing
       about that cap, so on a machine with lots of free RAM the cache would grow toward
       available memory and — combined with the non-cache footprint and in-flight decoder
       working buffers — trip onMemoryOverrun. This is invisible with JPEG/HEIC (tiny
       per-image decode) but bites hard when Decode Raw is on: each concurrent decoder
       holds a full-resolution raw in flight (hundreds of MB), so we must reserve headroom
       for those transients as well as the current non-cache footprint. */
    if (G::memoryAbortMB > 0) {
        const qint64 cacheMB     = static_cast<qint64>(icd->sizeMB());
        const qint64 footprintMB = static_cast<qint64>(G::processFootprintMB());
        // Everything the process holds that is NOT the image cache (Qt, DataModel,
        // thumbnails, plus whatever decoders happen to be running right now).
        const qint64 nonCacheMB  = std::max<qint64>(0, footprintMB - cacheMB);
        /* Reserve for peak decoder transients that may not be in nonCacheMB at this
           instant: decoderCount concurrent decodes, each roughly one decoded image.
           Size the per-image estimate from the current row's decoded size (raw ≈ 200+ MB,
           JPEG small), falling back to a modest floor. */
        float perImgMB = 0.0f;
        if (currRow >= 0 && currRow < rowCountSf())
            perImgMB = cacheMBAt(currRow);
        const qint64 decoderReserveMB =
            static_cast<qint64>(decoderCount) *
            std::max<qint64>(16, static_cast<qint64>(perImgMB));
        const qint64 capBudgetMB =
            static_cast<qint64>(G::memoryAbortMB) - nonCacheMB - decoderReserveMB;
        maxMBCeiling = std::min<qint64>(maxMBCeiling,
                                        std::max<qint64>(minMB, capBudgetMB));
    }

    /* Publish the memory the image cache still intends to claim (ceiling minus what it
       already holds) so DataModel's thumbnail budget can avoid double-counting it. */
    G::imageCacheHeadroomMB.store(
        std::max<qint64>(0, maxMBCeiling - static_cast<qint64>(icd->sizeMB())),
        std::memory_order_relaxed);

    /*
    Mac::availableMemory();
    qDebug() << "imageCache::memCheck"
             // << "ramMB =" << ramMB
             // << "procMB =" << procMB
             // << "availMB =" << availMB
             // << "level =" << level
             // << "safetyMB =" << safetyMB
             // << "hardCapFromRAM =" << hardCapFromRAM
             // << "burstCapFromAvail =" << burstCapFromAvail
             // << "ceiling =" << ceiling
             << "maxMBCeiling =" << maxMBCeiling
             << "G::availableMemoryMB =" << G::availableMemoryMB
             << "maxMB =" << maxMB
             << "icd->sizeMB() =" << icd->sizeMB()
        ;//*/
}

void ImageCache::updateStatus(int instruction, QString source)
/*
    Displays a statusbar showing the image cache status. Also shows the cache size
    in the info panel. All status info is passed by copy to prevent collisions on
    source data, which is being continuously updated by ImageCache.
*/
{
    quint64 currMB  = icd->sizeMB();
    /*
    qDebug() << "ImageCache::updateStatus"
             << "targetFirst =" << targetFirst
             << "targetLast =" << targetLast
             << "currMB =" << currMB
             << "source =" << source
        ; //*/
    if (!G::showCacheProgress) instruction = StatusAction::InfoOnly;
    emit showCacheStatus(instruction, isAutoMaxMB, currMB, maxMB,
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
    rpt << reportHealthChecks();
    rpt << reportMemoryFootprint();
    rpt << reportMemoryWarnings();
    rpt << reportLifetimeCounters();
    rpt << reportCacheParameters();
    rpt << reportCacheDecoders();
    rpt << reportPressureItemList();
    rpt << reportCacheItemList("");
    rpt << reportImCache();

    rpt << "\n\n" ;
    return reportString;
}

QString ImageCache::reportHealthChecks()
{
/*
    Invariant checks printed at the top of the diagnostic so anomalies surface
    before the reader scrolls through tables. Each line prints [OK] or [WARN].
    Order matches the most-common bug signatures we've debugged.
*/
    if (G::isLogger) log("reportHealthChecks");

    QString reportString;
    QTextStream rpt;
    rpt.setString(&reportString);

    auto line = [&](const QString &ok, const QString &name, const QString &detail) {
        rpt << "[" << ok.leftJustified(4, ' ') << "] "
            << name.leftJustified(28, ' ') << ": " << detail << "\n";
    };

    rpt << "Health checks:\n";

    // 1. Byte-accounting consistency: sum imCache vs icd->sizeBytes().
    {
        quint64 sumBytes = 0;
        int n = 0;
        icd->forEach([&](const QString &, const QImage &im) {
            sumBytes += static_cast<quint64>(im.sizeInBytes());
            ++n;
        });
        const quint64 reported = icd->sizeBytes();
        const quint64 diff = (reported > sumBytes) ? reported - sumBytes
                                                   : sumBytes - reported;
        const bool overflowed = reported > (quint64(1) << 60);
        if (overflowed || diff > 1024) {
            line("WARN", "byte accounting",
                 QString("reported=%1 bytes, sum of imCache=%2 bytes across %3 images (drift %4)")
                     .arg(Utilities::fitNumber(reported, 22))
                     .arg(Utilities::fitNumber(sumBytes, 22))
                     .arg(n)
                     .arg(Utilities::fitNumber(diff, 22)));
        } else {
            line("OK", "byte accounting",
                 QString("%1 MB (%2 bytes across %3 images)")
                     .arg(reported / (1024.0 * 1024.0), 0, 'f', 1)
                     .arg(Utilities::fitNumber(reported, 22))
                     .arg(n));
        }
    }

    // 2. imCache empty vs currMB > 0
    if (icd->isEmpty() && icd->sizeBytes() > 0) {
        line("WARN", "imCache vs sizeBytes",
             QString("imCache is empty but sizeBytes() = %1")
                 .arg(Utilities::fitNumber(icd->sizeBytes(), 22)));
    } else {
        line("OK", "imCache vs sizeBytes", "consistent");
    }

    // 3. Stranded rows: Attempts >= cap AND not cached.
    {
        int stranded = 0;
        QList<int> samples;
        const int rows = rowCountSf();
        for (int r = 0; r < rows; ++r) {
            const int att = sfVal(r, G::AttemptsColumn).toInt();
            const bool cached = sfVal(r, G::IsCachedColumn).toBool();
            if (att >= maxAttemptsToCacheImage && !cached) {
                ++stranded;
                if (samples.size() < 5) samples.append(r);
            }
        }
        if (stranded > 0) {
            QStringList s;
            for (int r : samples) s << QString::number(r);
            line("WARN", "stranded rows",
                 QString("%1 of %2 rows at maxAttempts=%3 and uncached (e.g. %4%5)")
                     .arg(stranded).arg(rows).arg(maxAttemptsToCacheImage)
                     .arg(s.join(", "))
                     .arg(stranded > samples.size() ? "..." : ""));
        } else {
            line("OK", "stranded rows", "none");
        }
    }

    // 4. toCache vs toCacheStatus key parity.
    {
        QStringList missing;
        for (int sfRow : toCache) {
            if (!toCacheStatus.contains(sfRow)) missing << QString::number(sfRow);
        }
        QStringList orphan;
        for (auto it = toCacheStatus.cbegin(); it != toCacheStatus.cend(); ++it) {
            if (!toCache.contains(it.key())) orphan << QString::number(it.key());
        }
        if (missing.isEmpty() && orphan.isEmpty()) {
            line("OK", "toCache key parity",
                 QString("%1 rows match").arg(toCache.size()));
        } else {
            line("WARN", "toCache key parity",
                 QString("missing-in-status: [%1]  orphan-in-status: [%2]")
                     .arg(missing.join(",")).arg(orphan.join(",")));
        }
    }

    // 5. Decoder isIdle vs cycling parity.
    {
        QStringList mismatched;
        for (int id = 0; id < decoderCount; ++id) {
            const bool idle = decoders[id]->isIdle();
            const bool cyc  = cycling.at(id);
            // cycling==true means a decode is in flight, so it should NOT be idle.
            if (idle == cyc) mismatched << QString::number(id);
        }
        if (mismatched.isEmpty()) {
            line("OK", "decoder idle/cycling", "consistent");
        } else {
            line("WARN", "decoder idle/cycling",
                 QString("ids with idle==cycling: [%1]").arg(mismatched.join(",")));
        }
    }

    // 6. DM instance mismatch.
    {
        const int dmInst = dm->instance.load();
        QStringList stale;
        for (int id = 0; id < decoderCount; ++id) {
            /* Skip decoders never dispatched this folder. With the raw-decode concurrency
               cap some decoders legitimately stay unused (status still Undefined), so their
               initial instance=0 is not the "returned an image from a previous folder" bug
               this check exists to catch. */
            if (decoders[id]->status == ImageDecoder::Undefined) continue;
            const int dec = decoders[id]->instance.load();
            if (dec != dmInst) stale << QString("%1(dec=%2)").arg(id).arg(dec);
        }
        if (stale.isEmpty()) {
            line("OK", "decoder DM instance",
                 QString("all at dm->instance=%1").arg(dmInst));
        } else {
            line("WARN", "decoder DM instance",
                 QString("dm->instance=%1, stale: [%2]")
                     .arg(dmInst).arg(stale.join(",")));
        }
    }

    // 7. Threading invariant.
    if (imageCacheThread.isRunning()) {
        line("OK", "thread state",
             QString("running, abort=%1").arg(abort ? "true" : "false"));
    } else if (abort) {
        line("OK", "thread state", "stopped (abort=true)");
    } else {
        line("WARN", "thread state", "thread not running but abort=false");
    }

    rpt << "\n";
    return reportString;
}

QString ImageCache::reportLifetimeCounters()
{
/*
    Counters reset on each initialize(). Useful for spotting trim/decode
    races that aren't visible in a single snapshot of state.
*/
    if (G::isLogger) log("reportLifetimeCounters");

    QString reportString;
    QTextStream rpt;
    rpt.setString(&reportString);
    rpt << "Lifetime counters (since folder load):\n";
    rpt << "  cached successfully    : "
        << Utilities::fitNumber(cachedCount.load(),       12) << "\n";
    rpt << "  trimmed from cache     : "
        << Utilities::fitNumber(trimmedCount.load(),      12) << "\n";
    rpt << "  late decodes (raced)   : "
        << Utilities::fitNumber(lateDecodeCount.load(),   12) << "\n";
    rpt << "  hit attempt cap        : "
        << Utilities::fitNumber(attemptCapHitCount.load(),12) << "\n";
    rpt << "\n";
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

    rpt << "\n";
    rpt << "instance                 = " << instance << "\n";
    rpt << "G::dmInstance            = " << G::dmInstance << "\n";

    rpt << "\n";
    rpt << "isInitializing           = " << (isInitializing ? "true" : "false") << "\n";
    // ints only: deferred and the pending path belong to the ImageCache thread
    rpt << "backstop fires           = " << deferred.firesSinceProgress()
        << " of " << DeferredRows::kMaxFires << "\n";
    rpt << "positionRetries          = " << positionRetries << "\n";
    rpt << "showCacheProgress        = " << QVariant(G::showCacheProgress).toString() << "\n";

    rpt << "\n";
    rpt << "totFiles                 = " << rowCountSf() << "\n";
    rpt << "currMB                   = " << icd->sizeMB() << "\n";
    rpt << "currBytes (raw)          = " << Utilities::fitNumber(icd->sizeBytes(), 22) << "\n";
    rpt << "maxMB                    = " << Utilities::fitNumber(maxMB, 14)            << "\n";
    rpt << "minMB                    = " << Utilities::fitNumber(minMB, 14)            << "\n";
    rpt << "maxMBCeiling             = " << Utilities::fitNumber(maxMBCeiling, 14)     << "\n";
    rpt << "G::availableMemoryMB     = " << Utilities::fitNumber(G::availableMemoryMB.load(), 14) << "\n";
    rpt << "memThrottle              = " << memThrottle << "\n";
    rpt << "maxAttemptsToCacheImage  = " << maxAttemptsToCacheImage << "\n";

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
    rpt << "isForward                = " << (isForward ? "true" : "false") << "\n";

    rpt << "\n";
    rpt << "targetFirst              = " << targetFirst << "\n";
    rpt << "targetLast               = " << targetLast << "\n";
    rpt << "cushion                  = " << cushion << "\n";
    rpt << "targetCount              = " << targetCount << "\n";

    rpt << "\n";
    rpt << "autoMaxMB                = " << QVariant(isAutoMaxMB).toString() << "\n";
    rpt << "autoStrategy             = " << getAutoStrategy() << "\n";
    rpt << "cushionLow               = " << cushionLow << "\n";
    rpt << "cushionHigh              = " << cushionHigh << "\n";

    rpt << "\n";
    rpt << "adjustCooldownMs         = " << adjustCooldownMs << "\n";
    rpt << "rapidStepMsThreshold     = " << rapidStepMsThreshold << "\n";
    rpt << "rapidMinStreak           = " << rapidMinStreak << "\n";

    rpt << "\n";
    rpt << "ignorePressureRestraints = " << (ignorePressureRestraints ? "true" : "false") << "\n";
    {
        // lastAdjustMs / lastMoveMs are epoch milliseconds (QDateTime::currentMSecsSinceEpoch).
        // The raw timestamp isn't useful for reading; print elapsed time since instead.
        const qint64 nowMsec = QDateTime::currentMSecsSinceEpoch();
        auto since = [nowMsec](qint64 stamp) {
            return stamp > 0 ? QString::number(nowMsec - stamp) + " ms"
                             : QString("never");
        };
        rpt << "since lastAdjustMs       = " << since(lastAdjustMs) << "\n";
        rpt << "since lastMoveMs         = " << since(lastMoveMs) << "\n";
    }
    rpt << "emaStepMs                = " << emaStepMs << "\n";
    rpt << "forwardStreak            = " << forwardStreak << "\n";
    rpt << "emaItemMB                = " << emaItemMB << "\n";
    rpt << "decoderMs (avg)          = " << decoderMs
        << "  window=" << decodeMsAvg.window() << "\n";

    rpt << "\n";
    rpt << "toCache count            = " << toCache.count() << "\n";
    rpt << "Cached count             = " << icd->count() << "\n";
    rpt << "cacheUpToDate            = " << (cacheUpToDate() ? "true" : "false") << "\n";
    rpt << "\n";

    // QStringList parts;
    // parts.reserve(pressureHistory.size());
    // for (int v : pressureHistory) {
    //     parts << QString::number(v);
    // }
    // rpt << "Pressure History: {" + parts.join(", ") + "}\n";
    // rpt << "\n";

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
    const int dmInst = dm->instance.load();
    rpt << "\nDecoders:  (dm->instance = " << dmInst << ")\n";
    rpt.reset();
    rpt.setFieldAlignment(QTextStream::AlignRight);
    rpt.setFieldWidth(4);  rpt << "ID";
    rpt.setFieldWidth(2);  rpt << "";
    rpt.setFieldAlignment(QTextStream::AlignLeft);
    rpt.setFieldWidth(14); rpt << "Status";
    rpt.setFieldAlignment(QTextStream::AlignRight);
    rpt.setFieldWidth(6);  rpt << "Key";
    rpt.setFieldWidth(11); rpt << "decInstance";
    rpt.setFieldWidth(9);  rpt << "isIdle";
    rpt.setFieldWidth(11); rpt << "isCycling";
    rpt.setFieldWidth(9);  rpt << "decodeMs";
    rpt.setFieldWidth(2);  rpt << "  ";
    rpt.setFieldAlignment(QTextStream::AlignLeft);
    rpt.setFieldWidth(40); rpt << "File";
    rpt.setFieldWidth(0);  rpt << "  errMsg\n";
    for (int id = 0; id < decoderCount; ++id) {
        const int decInstance = decoders[id]->instance.load();
        rpt.setFieldAlignment(QTextStream::AlignRight);
        rpt.setFieldWidth(4);  rpt << id;
        rpt.setFieldWidth(2);  rpt << "";
        rpt.setFieldAlignment(QTextStream::AlignLeft);
        rpt.setFieldWidth(14); rpt << decoders[id]->statusText.at(decoders[id]->status);
        rpt.setFieldAlignment(QTextStream::AlignRight);
        rpt.setFieldWidth(6);  rpt << decoders[id]->sfRow;
        rpt.setFieldWidth(11);
        rpt << (decInstance == dmInst
                ? QString::number(decInstance)
                : QString::number(decInstance) + "!");  // mark stale
        rpt.setFieldWidth(9);  rpt << QVariant(decoders[id]->isIdle()).toString();
        rpt.setFieldWidth(11); rpt << QVariant(cycling.at(id)).toString();
        rpt.setFieldWidth(9);  rpt << static_cast<qint64>(decoders[id]->nsToDecode / 1000000);
        rpt.setFieldWidth(2);  rpt << "  ";
        rpt.setFieldAlignment(QTextStream::AlignLeft);
        rpt.setFieldWidth(40); rpt << Utilities::getFileName(decoders[id]->fPath);
        rpt.setFieldWidth(0);  rpt << "  " << decoders[id]->errMsg << "\n";
    }
    rpt.setFieldWidth(0);
    rpt << "\n";
    return reportString;
}

QString ImageCache::reportPressureItemList()
{
    if (G::isLogger) log("reportPressureItemList");
    if (debugCaching) qDebug() << "ImageCache::reportPressureItemList";

    QString reportString;
    QTextStream rpt;
    rpt.flush();
    reportString = "";
    rpt.setString(&reportString);
    // header
    rpt.reset();
    rpt << "Pressure history: ";
    rpt << "  Auto Relieve Pressure = " << QVariant(isAutoMaxMB).toString();
    rpt << "  Average time to decode = " << QVariant(decoderMs).toString() << " ms";
    rpt << "\n";

    if (!isAutoMaxMB) {
        rpt << "\n";
        return reportString;
    }

    // Formatting helpers: Utilities::fitNumber handles thousands separators
    // and overflow-fill; the sentinel -1 ("never happened") is rendered as
    // "n/a" at the call site so each caller can pick its own placeholder.
    auto sint = [](qint64 v, int w) {
        return v == -1 ? QString("n/a") : Utilities::fitNumber(v, w);
    };

    rpt.setFieldAlignment(QTextStream::AlignRight);
    auto hdr = [&rpt](int w, const QString &s) {
        rpt.setFieldWidth(w); rpt << s;
    };
    hdr( 6, "Key");
    hdr( 8, "Bypass");      // was "First" (isJustDoIt)
    hdr( 9, "RapidFwd");    // was "Rapid"
    hdr(12, "Elapsed");
    hdr(10, "Cooldown");
    hdr(12, "InCooldown");  // was "Cool" (isCooldown)
    hdr(11, "tSinceMove");  // ms since last forward step
    hdr( 9, "Cushion");
    hdr( 9, "HighChk");
    hdr( 9, "LowChk");
    hdr( 9, "StepMB");
    hdr( 9, "MinMB");
    hdr(16, "CeilMB");
    hdr(16, "CacheMB");
    hdr(16, "MaxMB");
    rpt.setFieldWidth(0);
    rpt << "\n";

    // body
    for (PressureItem v : pressureHistory) {
        rpt.setFieldAlignment(QTextStream::AlignRight);
        rpt.setFieldWidth( 6); rpt << QVariant(v.sfRow).toString();
        rpt.setFieldWidth( 8); rpt << QVariant(v.isJustDoIt).toString();
        rpt.setFieldWidth( 9); rpt << QVariant(v.isRapidForward).toString();
        rpt.setFieldWidth(12); rpt << sint(v.elapsedMs, 12);
        rpt.setFieldWidth(10); rpt << QVariant(adjustCooldownMs).toString();
        rpt.setFieldWidth(12); rpt << QVariant(v.isCooldown).toString();
        rpt.setFieldWidth(11); rpt << sint(v.tSinceMoveMs, 11);
        rpt.setFieldWidth( 9); rpt << QVariant(v.cushion).toString();
        rpt.setFieldWidth( 9); rpt << QVariant(v.highChk).toString();
        rpt.setFieldWidth( 9); rpt << QVariant(v.lowChk).toString();
        rpt.setFieldWidth( 9); rpt << QVariant(v.stepMB).toString();
        rpt.setFieldWidth( 9); rpt << QVariant(minMB).toString();
        rpt.setFieldWidth(16); rpt << Utilities::fitNumber(v.ceilMB, 16);
        rpt.setFieldWidth(16); rpt << Utilities::fitNumber(v.cacheMB, 16);
        rpt.setFieldWidth(16); rpt << Utilities::fitNumber(v.maxMB, 16);
        rpt.setFieldWidth(0);
        rpt << "\n";
    }
    rpt << "\n";
    return reportString;
}

QString ImageCache::reportCacheItemList(QString title)
{
    if (G::isLogger) log("reportCacheItemList");
    if (debugCaching) qDebug() << "ImageCache::reportCacheItemList";

    QStringList rptStatus {
        "Undefine",
        "Success",
        "Abort",
        "Invalid",
        "Failed",
        "Video",
        "Clash",
        "NoDir",
        "NoFile",
        "BlankFi",
        "NoMeta",
        "FileOpen",
        "NoGeom"
    };

    QString reportString;
    QTextStream rpt;
    rpt.flush();
    reportString = "";
    rpt.setString(&reportString);

    // Aggregate summary before the row-by-row table so the reader sees the
    // failure pattern (stranded rows, untargeted misses) without scrolling.
    {
        const int nRows = rowCountSf();
        int dmCached = 0;
        int imCached = 0;
        int stranded = 0;
        int inTargetNotCached = 0;
        int outsideTarget = 0;
        int videoOrInvalid = 0;
        int attBucket0 = 0, attBucket1to3 = 0, attBucket4to9 = 0, attBucketCap = 0;
        for (int r = 0; r < nRows; ++r) {
            const QString fPath = sfVal(r, 0, G::KeyRole).toString();
            const int att = sfVal(r, G::AttemptsColumn).toInt();
            const bool dmFlag = sfVal(r, G::IsCachedColumn).toBool();
            const bool inIm = icd->contains(fPath);
            const bool inTarget = (r >= targetFirst && r <= targetLast);
            const bool isVideo = sfVal(r, G::VideoColumn).toBool();
            const int status = sfVal(r, G::DecoderReturnStatusColumn).toInt();
            const bool invalid = (status == ImageDecoder::Status::Invalid);

            if (dmFlag) ++dmCached;
            if (inIm) ++imCached;
            if (isVideo || invalid) ++videoOrInvalid;
            if (att >= maxAttemptsToCacheImage && !inIm) ++stranded;
            if (inTarget && !inIm && !isVideo) ++inTargetNotCached;
            if (!inTarget) ++outsideTarget;

            if      (att == 0)                        ++attBucket0;
            else if (att <= 3)                        ++attBucket1to3;
            else if (att <  maxAttemptsToCacheImage)  ++attBucket4to9;
            else                                      ++attBucketCap;
        }
        rpt << "Cache item summary (" << nRows << " rows):\n";
        rpt << "  Cached (imCache)        : " << imCached << "\n";
        rpt << "  Cached (datamodel flag) : " << dmCached << "\n";
        rpt << "  Stranded (maxAttempts)  : " << stranded
            << (stranded > 0 ? "   <-- anomaly" : "") << "\n";
        rpt << "  In target, not cached   : " << inTargetNotCached << "\n";
        rpt << "  Outside target          : " << outsideTarget << "\n";
        rpt << "  Video / invalid         : " << videoOrInvalid << "\n";
        rpt << "  Attempts distribution   : 0:" << attBucket0
            << "  1-3:" << attBucket1to3
            << "  4-" << (maxAttemptsToCacheImage - 1) << ":" << attBucket4to9
            << "  " << maxAttemptsToCacheImage << ":" << attBucketCap << "\n\n";
    }

    rpt << "Cache Item List: \n";

    int dmCachedCount = 0;
    for (int sfRow = 0; sfRow < rowCountSf(); ++sfRow) {
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
                << "Decode"
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
        QString fPath = sfVal(sfRow, 0, G::KeyRole).toString();
        rpt
            << sfRow
            << sfVal(sfRow, G::DecoderIdColumn).toInt()
            << (sfRow >= targetFirst && sfRow <= targetLast ? "true" : "false")
            << sfVal(sfRow, G::AttemptsColumn).toString()
            << (sfVal(sfRow, G::IsCachingColumn).toBool() ? "true" : "false")
            << (sfVal(sfRow, G::IsCachedColumn).toBool() ? "true" : "false")
            << rptStatus.at(sfVal(sfRow, G::DecoderReturnStatusColumn).toInt())
            << (icd->contains(fPath) ? "true" : "false")
            << (sfVal(sfRow, G::MetadataStatusColumn).toInt() != G::MetaNotAttempted ? "true" : "false")
            << (sfVal(sfRow, G::MetadataStatusColumn).toInt() == G::MetaLoaded ? "true" : "false")
            << (sfVal(sfRow, G::VideoColumn).toBool() ? "true" : "false")
            << QString::number(sfVal(sfRow, G::CacheSizeColumn).toFloat(), 'f', 2)
            << sfVal(sfRow, G::NSThumbColumn).toInt()
            << sfVal(sfRow, G::OffsetFullColumn).toInt()
            << sfVal(sfRow, G::LengthFullColumn).toInt()
            ;
        rpt.setFieldAlignment(QTextStream::AlignLeft);
        rpt.setFieldWidth(3);
        rpt << "   ";

        rpt.setFieldWidth(40);
        rpt << Utilities::getFileName(fPath);
        rpt.setFieldWidth(1);
        rpt << " ";

        rpt.setFieldWidth(50);
        rpt << sfVal(sfRow, G::DecoderErrMsgColumn).toString();
        rpt.setFieldWidth(0);
        rpt << "\n";

        if (sfVal(sfRow, G::IsCachedColumn).toBool()) dmCachedCount++;
    }
    rpt << dmCachedCount << " images reported as cached." << "\n";

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
            rpt << QVariant(toCacheStatus[sfRow].isCaching).toString();
            rpt.setFieldWidth(3);
            rpt << "   ";
            rpt.setFieldAlignment(QTextStream::AlignLeft);
            rpt.setFieldWidth(25);
            rpt << toCacheStatus[sfRow].msg;
            rpt.setFieldWidth(50);
            rpt << sfVal(sfRow, 0, G::KeyRole).toString();
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

    QStringList keys = icd->keys();
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
        imRptItem.sfRow = sfRowOf(imRptItem.fPath);
        image = QImage();
        icd->get(keys.at(i), image);
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
    rpt << "\n" << Utilities::fitNumber(static_cast<qint64>(mem), 14) << " MB consumed";

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
    for (const QString &fPath : icd->keys())
        imCacheRows.append(sfRowOf(fPath));
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
        bool dmIsCaching = sfVal(sfRow, G::IsCachingColumn).toBool();
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

void ImageCache::initialize()
{
    if (!G::useImageCache) return;
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
    pressureHistory.clear();
    // reset raw-decode concurrency accounting for the new folder
    activeRawDecodes.store(0, std::memory_order_relaxed);
    peakActiveRawDecodes = 0;
    for (int i = 0; i < decoderIsRaw.size(); ++i) decoderIsRaw[i] = false;
    // drop cached pre-develop WorkingImages from the previous folder (memory hygiene)
    WorkingImageCache::instance().clear();
    if (isAutoMaxMB) maxMB = 1024;  // only if autoMaxMB?

    // Sync the instance FIRST, before any early-return. Otherwise ICC::instance
    // stays stale across the gap between reset() (which bumps dm->instance) and
    // folderChangeCompleted() -> updateInstance(), and any operation that runs
    // in between (setCurrentPosition, dispatch, etc.) sees a stale instance.
    instance = dm->instance;

    /*  ORDER MATTERS. resetStaleIsCaching() reads our own state to find rows
        left marked caching and mirrors isCaching=false to the datamodel for
        each; clearing first would make it a no-op and leave those columns set.
        Only then drop the rest of the previous instance's bookkeeping -- see
        updateInstance() for why it must not carry over. */
    resetStaleIsCaching();
    clearAllState();
    /*  Deferrals, the idle backstop and a pending position all name the previous
        folder's paths. */
    deferred.clear();
    pendingPositionPath.clear();
    pendingPositionSrc.clear();
    positionRetries = 0;

    /*  Cancel if no images to cache. MW::reset initializes right after clearing the
        model, so this is the first initialize of every load: returning with
        isInitializing still set refused every position until the second one
        (folderChanged), and isIdle() reported busy meanwhile. */
    if (!rowCountSf()) {
        isInitializing = false;
        return;
    }

    // cache management parameters
    currRow = 0;
    prevRow = -1;
    // the cache defaults to the first image and a forward selection direction
    isForward = true;
    targetFirst = 0;
    targetLast = 0;
    directionChangeThreshold = 3;
    decodeImageCount = 0;
    decodeImageMsTot = 0;
    decodeImageMBTot = 0;
    firstDispatchNewDM = true;

    // reset motion/pressure & allow one immediate adjust
    forwardStreak = 0;
    emaStepMs = -1.0;
    emaItemMB = -1.0f;
    // cushion = 0;
    cushion = INT_MAX;
    lastAdjustMs = 0;
    ignorePressureRestraints = true;     // <- enable first-adjust bypass

    // reset diagnostic lifetime counters
    cachedCount.store(0, std::memory_order_relaxed);
    trimmedCount.store(0, std::memory_order_relaxed);
    lateDecodeCount.store(0, std::memory_order_relaxed);
    attemptCapHitCount.store(0, std::memory_order_relaxed);

    updateStatus(StatusAction::Clear, "ImageCache::initializeImageCache");

    isInitializing = false;
    abort = false;
}

void ImageCache::updateInstance()
{
    if (G::isLogger) log("updateInstance");
    instance = dm->instance;
    /* A new instance is a new set of images. Attempts, decode status and the
       caching/cached flags describe the old one and must not carry over --
       a path that reappears in the next folder would inherit its retry count
       and, if it had failed terminally, never be decoded again. */
    clearAllState();
    /*  EXCEPT WHAT IS PLAINLY TRUE. updateInstance runs for the same folder (a version
        insert at the end of a load), so icd still holds those images; leaving them
        "not cached" here contradicted icd (C17). */
    for (const QString &k : icd->keys()) setStateCached(k, true);
}

bool ImageCache::getAutoMaxMB()
{
    return isAutoMaxMB;
}

QString ImageCache::getAutoStrategy()
{
    return autoStrategyStr.at(autoStrategy);
}

void ImageCache::setAutoMaxMB(bool autoSize, AutoStrategy strategy)
{
    isAutoMaxMB = autoSize;
    autoStrategy = strategy;
    switch (strategy) {
        case AutoStrategy::Frugal:
            cushionLow  = 5;
            cushionHigh = 15;
            minStepMB = 128;
            maxStepMB = 512;
            memThrottle = 0.8;
            break;
        case AutoStrategy::Moderate:
            cushionLow  = 5;
            cushionHigh = 25;
            minStepMB = 256;
            maxStepMB = 1048;
            memThrottle = 1.0;
            break;
        case AutoStrategy::Greedy:
            cushionLow  = 50;
            cushionHigh = 100;
            minStepMB = 1048;
            maxStepMB = 4096;
            memThrottle = 1.2;
            break;
        case AutoStrategy::Ignore:
            break;
        default:
            break;
    }

        updateStatus(StatusAction::InfoOnly, "setAutoMaxMB");
}

quint64 ImageCache::getMaxMB()
{
    return maxMB;
}

quint64 ImageCache::getMaxMBCeiling()
{
    return maxMBCeiling;
}

void ImageCache::setMemoryThrottled(bool on)
{
/*
    Called from the GUI-thread memory watchdog. Only flips an atomic, so it is safe to
    call directly across threads; the actual pause/resume work is done on the ImageCache
    thread by memoryPause()/memoryResume() (queued).
*/
    memoryThrottled.store(on, std::memory_order_relaxed);
}

bool ImageCache::isMemoryThrottled() const
{
    return memoryThrottled.load(std::memory_order_relaxed);
}

void ImageCache::noteMemoryWarning(const QString &msg)
{
/*
    Thread-safe append to the memory-warning ring buffer surfaced in diagnostics().
    Written from both the GUI thread (watchdog) and the ImageCache thread (pause/resume).
*/
    const QString stamped =
        QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz") + "  " + msg;
    QMutexLocker locker(&memWarnMutex);
    memoryWarnings.append(stamped);
    while (memoryWarnings.size() > maxMemoryWarnings) memoryWarnings.removeFirst();
}

void ImageCache::memoryPause(quint64 footprintMB, quint64 capMB)
{
/*
    Queued from MW::memoryWatchdogTick when the process footprint reaches the pause
    threshold. Runs on the ImageCache thread. The throttle flag (set by the watchdog
    before this fires) already stops new decodes at decodeNextImage; here we actively
    free memory by shrinking maxMB and re-running the target-range trim so the footprint
    falls back below the resume threshold. No teardown, no dialog — this replaces the old
    hard onMemoryOverrun abort for the normal Decode-Raw pressure case.
*/
    if (abort) return;

    const quint64 beforeMB = icd->sizeMB();
    /* Shrink the cache budget so trimOutsideTargetRange sheds images. Drop to ~70% of
       what is currently held (never below minMB) so we give back a meaningful chunk in
       one step rather than dribbling. */
    const qint64 shrunkMB = std::max<qint64>(minMB, static_cast<qint64>(beforeMB * 0.70));
    maxMB = static_cast<quint64>(shrunkMB);
    maxMBCeiling = std::min<qint64>(maxMBCeiling, shrunkMB);

    /* Recompute the (smaller) target range and evict everything outside it. Decoders are
       parked via the throttle flag, so this frees memory without immediately re-filling. */
    updateToCache();

    const quint64 afterMB = icd->sizeMB();
    noteMemoryWarning(
        QString("PAUSE  footprint=%1 MB >= cap=%2 MB. Decoders parked; cache %3 -> %4 MB "
                "(maxMB=%5).")
            .arg(footprintMB).arg(capMB).arg(beforeMB).arg(afterMB).arg(maxMB));

    if (G::isLogger || G::isFlowLogger)
        log("memoryPause", QString("footprint=%1 cap=%2 cache %3->%4 MB")
                               .arg(footprintMB).arg(capMB).arg(beforeMB).arg(afterMB));

    updateStatus(StatusAction::All, "ImageCache::memoryPause");
}

void ImageCache::memoryResume(quint64 footprintMB)
{
/*
    Queued from MW::memoryWatchdogTick when the footprint has dropped below the resume
    threshold. Runs on the ImageCache thread. The throttle flag has already been cleared
    by the watchdog; relaunch decoders so caching of the target range continues.
*/
    if (abort) return;

    noteMemoryWarning(
        QString("RESUME footprint=%1 MB below resume threshold. Caching resumed "
                "(cache=%2 MB, maxMB=%3).")
            .arg(footprintMB).arg(icd->sizeMB()).arg(maxMB));

    if (G::isLogger || G::isFlowLogger)
        log("memoryResume", QString("footprint=%1 MB").arg(footprintMB));

    /* dispatch() re-runs memChk (recomputing the ceiling now that memory has freed),
       rebuilds the target range and relaunches decoders. */
    dispatch();
}

QString ImageCache::reportMemoryWarnings()
{
    QString reportString;
    QTextStream rpt;
    rpt.setString(&reportString);
    rpt << Utilities::centeredRptHdr('-', "Memory Pressure Warnings");
    rpt << "\n";
    QMutexLocker locker(&memWarnMutex);
    if (memoryWarnings.isEmpty()) {
        rpt << "  (none)\n";
    }
    else {
        for (const QString &w : memoryWarnings) rpt << "  " << w << "\n";
    }
    rpt << "\n";
    return reportString;
}

bool ImageCache::willUseSensorDecode(int sfRow) const
{
/*
    True when a decode of sfRow will take the memory-heavy full-sensor RAW path: Decode Raw
    is on AND the file extension has a sensor decoder. With Decode Raw off, RAW files use the
    embedded JPG (cheap), so this returns false and the concurrency cap does not apply --
    except a DNG whose only JPEG is a thumbnail (RawFormat::PreviewTooSmall), which
    ImageDecoder::load decodes from the sensor data regardless.
*/
    auto snap = dm->proxySnapshot();
    if (!snap || sfRow < 0 || sfRow >= snap->rowCount()) return false;
    const QString fPath = snap->path(sfRow);
    const QString ext = QFileInfo(fPath).suffix().toLower();
    if (G::useRaw) return RawFormat::HasSensorDecoder(ext);
    if (ext != "dng") return false;
    /* dm->index, not dm->sf: RowStore reads are locked (see ImageDecoder::dmVal). */
    const int dmRow = snap->dmRow(sfRow);
    if (dmRow < 0 || dmRow >= dm->rowCount()) return false;
    return RawFormat::PreviewTooSmall(
        ext, dm->index(dmRow, G::WidthOrigPreviewColumn).data().toInt(),
        dm->index(dmRow, G::HeightOrigPreviewColumn).data().toInt());
}

int ImageCache::rawDecodeLimit(int sfRow) const
{
/*
    Maximum number of concurrent full-sensor RAW decodes. A sensor decode's peak transient
    is dominated by the scene-linear float WorkingImage (W*H*3 floats = 3x the 8-bit QImage)
    plus demosaic scratch — about 8x the cached QImage in practice. We let concurrent raw
    decodes use up to ~half the hard memory cap, leaving the other half for the image cache,
    the WorkingImageCache, thumbnails and the app baseline. Clamped to [1, decoderCount], so
    on a big-RAM machine (larger memoryAbortMB) it opens back up toward full concurrency.
*/
    float qimgMB = (sfRow >= 0 && sfRow < rowCountSf())
                       ? cacheMBAt(sfRow)
                       : 0.0f;
    if (qimgMB <= 0.0f) qimgMB = 150.0f;                       // ~33 MP fallback
    const double perDecodeMB = static_cast<double>(qimgMB) * 8.0;
    const double budgetMB = (G::memoryAbortMB > 0)
                                ? static_cast<double>(G::memoryAbortMB) * 0.50
                                : 6000.0;
    const int lim = static_cast<int>(budgetMB / perDecodeMB);
    return std::clamp(lim, 1, decoderCount);
}

QString ImageCache::reportMemoryFootprint()
{
/*
    Confirms where process memory lives: the image cache, the (separate, byte-budgeted)
    WorkingImageCache, or in-flight full-sensor RAW decodes. A large footprint with a small
    imCache and a full WorkingImageCache still short of the total means the memory is in the
    concurrent raw decoders — which the raw decode limit below bounds.
*/
    QString reportString;
    QTextStream rpt;
    rpt.setString(&reportString);
    const double MB = 1024.0 * 1024.0;
    WorkingImageCache &wic = WorkingImageCache::instance();

    rpt << Utilities::centeredRptHdr('-', "Memory Footprint / Raw Decode");
    rpt << "\n";
    rpt << "  process footprint       : " << G::processFootprintMB() << " MB\n";
    rpt << "  memoryAbortMB cap        : " << G::memoryAbortMB << " MB\n";
    const int imCacheCount = icd->count();
    rpt << "  imageCache (imCache)     : " << icd->sizeMB()
        << " MB (" << imCacheCount << " images)\n";
    rpt << "  WorkingImageCache        : " << static_cast<qint64>(wic.currentBytes() / MB)
        << " MB (" << wic.count() << " entries, budget "
        << static_cast<qint64>(wic.maxBytes() / MB) << " MB)\n";
    rpt << "  Decode Raw (G::useRaw)   : " << (G::useRaw ? "ON" : "off") << "\n";
    rpt << "  decoderCount             : " << decoderCount << "\n";
    rpt << "  active raw decodes       : " << activeRawDecodes.load(std::memory_order_relaxed)
        << " (peak " << peakActiveRawDecodes << ")\n";
    const int lim = (currRow >= 0 && currRow < rowCountSf()) ? rawDecodeLimit(currRow) : 0;
    const float qimgMB = (currRow >= 0 && currRow < rowCountSf())
                             ? sfVal(currRow, G::CacheSizeColumn).toFloat() : 0.0f;
    rpt << "  raw decode limit         : " << lim
        << "  (est " << QString::number(qimgMB * 8.0, 'f', 0) << " MB/decode)\n";
    rpt << "\n";
    return reportString;
}

void ImageCache::setMaxMB(quint64 mb)
{
    // if (G::isLogger)
        log("setMaxMB", QVariant(mb).toString());
    maxMB = mb;
    dispatch();
    updateStatus(StatusAction::InfoOnly, "setMaxMB");
}

void ImageCache::filterChange(QString currentImageFullPath, QString src)
{
/*
    When the datamodel is filtered the image cache needs to be updated. The current
    image is set and setCachePosition() is called.
*/
    QString fun = "ImageCache::filterChange";
    if (debugLog || G::isLogger || G::isFlowLogger)
        log("filterChange", "src = " + src);
    if (debugCaching)
    {
        qDebug().noquote()
            << fun.leftJustified(col0Width, ' ')
            << "rowCount =" << rowCountSf()
            << "icd->count() =" << icd->count()
            << "currentImageFullPath =" << currentImageFullPath
            << "inSnapshot =" << (sfRowOf(currentImageFullPath) >= 0)
            << "currentImageFullPath =" << currentImageFullPath
            << "sfRowOf(currentImageFullPath) ="
            << sfRowOf(currentImageFullPath)
            ;
    }
    if (rowCountSf() == 0) return;
    if (G::removingRowsFromDM) return;

    // do not use mutex - spins forever

    // update instance
    instance = dm->instance;

    setCurrentPosition(currentImageFullPath, fun);
}

void ImageCache::refreshImageCache()
{
    // if (debugLog || G::isLogger || G::isFlowLogger)
        log("refreshImageCache");
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
    abort = false;
    clearForReload("ImageCache::reloadImageCache");
    dispatch();
}

void ImageCache::clearForReload(const QString &src)
{
/*
    A reload: every cached image was decoded by settings that just changed (operation
    mode, Decode Raw, preview source, colour management).

    - The generation moves on, so a decode already running -- it chose its pixels by
      the old settings -- is superseded when it lands instead of being cached over the
      right one (C26).
    - Our per-path "cached" and the model's badge follow icd: this used to clear icd
      and leave both saying cached (C5).
    - The queue and the deferrals were built for the old settings.
*/
    bumpGeneration(src);
    const QStringList was = icd->keys();
    icd->clear();
    for (const QString &k : was) {
        setStateCached(k, false);
        emit setCached(sfRowOf(k), k, G::CacheNone, instance);
    }
    toCache.clear();
    toCacheStatus.clear();
    deferred.clear();
    capLifted.clear();
}

void ImageCache::reloadImageCacheSeeded(QString fPath, QImage image, bool colorManage)
{
/*
    Reload all images in the cache, except that fPath's entry is the image supplied rather
    than a decode. Used when leaving Develop: the mode change rebuilds the cache (the decode
    differs between modes), but the developed picture of the current image is already in
    memory -- it is what the loupe is showing. Decoding it again cost seconds (the
    devPreview of the recipe just edited is usually not on disk yet, so the decoder fell
    through to the file) during which Browse waited on pixels it already had.

    The image is the develop render's output, which is sRGB -- the same pixels a devPreview
    holds -- so it is colour-managed exactly as ImageDecoder::colorManage treats a
    devPreview: an empty profile, i.e. "from sRGB". Done here, on the ImageCache thread,
    to keep a full-resolution transform off the GUI thread. setTargetRange skips a key
    icd already holds, so no decoder is dispatched for it.
*/
    if (debugLog || G::isLogger || G::isFlowLogger) log("reloadImageCacheSeeded", fPath);
    abort = false;
    clearForReload("ImageCache::reloadImageCacheSeeded");

    const int sfRow = fPath.isEmpty() ? -1 : sfRowOf(fPath);
    if (sfRow >= 0 && !image.isNull() && G::useImageCache) {
        if (colorManage) {
            // ICC::transform assumes 4 bytes/pixel (see ImageDecoder::colorManage)
            if (image.format() != QImage::Format_ARGB32 &&
                image.format() != QImage::Format_RGB32)
                image = image.convertToFormat(QImage::Format_ARGB32);
            if (!image.isNull()) ICC::transform(QByteArray(), image);
        }
        if (!image.isNull()) {
            icd->insert(fPath, image);
            cachedCount.fetch_add(1, std::memory_order_relaxed);
            setStateCached(fPath, true);
            {
                QMutexLocker lock(&cacheStateMutex);
                cacheState[fPath].attempts = 0;
            }
            // the loupe repaints from it (MW::refreshViewsOnCacheChange)
            emit setCached(sfRow, fPath, G::CacheDeveloped, instance);
        }
    }
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
    checkCacheThread("ImageCache::setCurrentPosition");
    QString fun = "ImageCache::setCurrentPosition";

    /*  A NEWER POSITION REPLACES A PENDING ONE. The retry budget below belongs to one
        path; a different path starts it again. */
    if (fPath != pendingPositionPath) positionRetries = 0;
    pendingPositionPath.clear();
    pendingPositionSrc.clear();
    deferred.noteProgress();

    currPath = fPath;
    adoptSnapshot();
    currRow = sfRowOf(fPath);

    if (debugLog || G::isLogger || G::isFlowLogger) {
        QString msg = "row = " + QString::number(currRow) + " " + fPath;
        log("setCurrentPosition", msg);
    }
    if (debugCaching)
    {
        qDebug().noquote() << fun.leftJustified(col0Width, ' ')
        << "currRow =" << currRow
        << fPath
        << "src =" << src
        << "isRunning =" << imageCacheThread.isRunning();
    }

    /*  NOT READY YET IS NOT A REFUSAL. This used to return on an instance clash, an
        initialize in progress or a row removal, and nothing replayed the position: after
        applyModelChange bumped the model's instance without a filter change to re-sync
        this cache, every later selection was refused and each uncached image stayed
        black (C2). The instance's only job is rejecting LATE decode results
        (okToCache); updateToCache adopts the model's instance itself. What this needs
        is a proxy snapshot of the model's current instance that knows the path. The
        snapshot rebuild is coalesced to the next GUI turn, so until it lands the
        position is retried every kPositionRetryMs, for at most kMaxPositionRetries.
        The same retry covers a path the snapshot does not hold yet (an insert or
        re-key, C22). See Documentation.md 4.4.12. */
    auto snap = dm->proxySnapshot();
    const bool snapStale = !snap || snap->instance != G::dmInstance.load();
    const bool notReady = snapStale || currRow < 0 || isInitializing ||
                          G::removingRowsFromDM;
    if (notReady) {
        if (positionRetries >= kMaxPositionRetries) {
            QString msg = "Position abandoned after " + QString::number(positionRetries) +
                          " retries from " + src + (snapStale ? " (stale snapshot)" :
                          currRow < 0 ? " (path not in snapshot)" : " (busy)");
            G::issueDedup("Warning", msg, "ImageCache::setCurrentPosition", currRow, fPath);
            positionRetries = 0;
            return;
        }
        ++positionRetries;
        pendingPositionPath = fPath;
        pendingPositionSrc = src;
        if (!positionRetryArmed) {
            positionRetryArmed = true;
            QTimer::singleShot(kPositionRetryMs, this, [this] {
                positionRetryArmed = false;
                if (abort) pendingPositionPath.clear();     // folder change or quit
                if (pendingPositionPath.isEmpty()) return;
                setCurrentPosition(pendingPositionPath, pendingPositionSrc);
            });
        }
        return;
    }
    positionRetries = 0;

    // range check
    if (currRow >= rowCountSf()) {
        QString msg = "currRow is out of range.";
        /* currRow, not dm->currentSfRow: that member is written by the GUI
           thread and reading it here was a race for a diagnostic value we
           already hold. */
        G::issue("Warning", msg, "ImageCache::setCurrentPosition", currRow, fPath);
        qWarning() << "WARNING ImageCache::setCurrentPosition currRow ="
                   << currRow << "is out of range";
        return;
    }

    readmitCurrent(pathAt(currRow));

    abort = false;
    dispatch();
}

void ImageCache::readmitCurrent(const QString &fPath)
{
/*
    THE CURRENT IMAGE ALWAYS GETS ANOTHER TRY (C19, Documentation 4.4.14). A terminal
    status blacklists a row for the rest of the folder (okToDecode), and the loupe shows
    only what this cache holds -- so one bad decode, whatever caused it (a file still
    being copied, a network hiccup, geometry misfiled for a moment), left the image
    blank for good however often the user came back to it.

    - A terminal status is cleared each time the image becomes current. Invalid also
      cleared OUR isCached, which setTargetRange sets only so fillCache stops looping on
      it; leaving it set made this re-admit a no-op for exactly the Invalid case.
    - The attempts cap is lifted ONCE per path (one more try), so a file that really
      cannot be read costs one decode per visit, not a loop. capLifted is forgotten with
      the state it overrides: a reload, an invalidate, a new folder.
*/
    if (fPath.isEmpty() || icd->contains(fPath)) return;
    const RowCache cs = stateOf(fPath);
    if (cs.isCaching || cs.status == ImageDecoder::Status::Video) return;
    const bool terminal = cs.status != ImageDecoder::Status::Undefined &&
                          cs.status != ImageDecoder::Status::Success;
    if (terminal) {
        setStateStatus(fPath, ImageDecoder::Status::Undefined);
        setStateCached(fPath, false);
    }
    if (cs.attempts >= maxAttemptsToCacheImage && !capLifted.contains(fPath)) {
        capLifted.insert(fPath);
        QMutexLocker lock(&cacheStateMutex);
        auto it = cacheState.find(fPath);
        if (it != cacheState.end()) it->attempts = maxAttemptsToCacheImage - 1;
    }
}

bool ImageCache::okToDecode(int sfRow, int id, QString &msg)
{
    // skip videos
    if (isVideoAt(sfRow)) {
        msg = "Video";
        return false;
    }

    // already in imCache
    QString fPath = pathAt(sfRow);
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
    /*  By path too: a decode dispatched before adoptSnapshot dropped the row-numbered
        queue is still running, and only cacheState remembers it (4.4.18). */
    if (stateOf(fPath).isCaching) {
        msg = "IsCaching";
        return false;
    }

    // prior attempt to decode failed with a terminal status.
    // "Undefined" / "Abort" are transient (decoder was aborted mid-flight) and
    // must remain retry-eligible, otherwise the row is stranded in toCache.
    /*  NOTE: this test only started working when the state moved here. It read
        G::DecoderReturnStatusColumn with .toString() and compared against status
        NAMES, but the column is written as the enum's INT -- so .toString() gave
        "3" and the list never matched, and a terminally-failed row was retried
        up to maxAttemptsToCacheImage times regardless. The set below is
        unchanged from that list, and deliberately excludes Undefined, Abort and
        InstanceClash, which are transient and must stay retry-eligible. */
    switch (stateOf(fPath).status) {
    case ImageDecoder::Status::Invalid:
    case ImageDecoder::Status::Failed:
    case ImageDecoder::Status::Video:
    case ImageDecoder::Status::NoDir:
    case ImageDecoder::Status::NoFile:
    case ImageDecoder::Status::BlankFilePath:
    case ImageDecoder::Status::NoMetadata:
        msg = "Prior terminal decode failure";
        return false;
    default:
        break;
    }

    /*  The decoder found no embedded-JPG geometry yet (status NoGeometry). Nothing
        wakes this one: the idle backstop clears it. */
    if (deferred.has(fPath, DeferredRows::Geometry)) {
        msg = "Deferred: no embedded-JPG geometry yet";
        return false;
    }

    // cap retries so a persistently-broken decoder can't loop forever
    int attempts = stateOf(fPath).attempts;
    if (attempts >= maxAttemptsToCacheImage) {
        msg = "Max attempts reached";
        attemptCapHitCount.fetch_add(1, std::memory_order_relaxed);
        return false;
    }

    // Gate decoder on metadata fully loaded (offsets/lengths populated by
    // DataModel::addMetadataForItem). MetadataAttempted alone is not enough:
    // failure paths (unreadable type, video FrameDecoder error) set Attempted
    // without populating offsets, which makes the decoder fail with
    // "length = 0" / "offset is invalid" and burns AttemptsColumn.
    const ModelView mv{qsnap(), dm->rowSync()};
    if (!mv.consistent()) {
        // the snapshot is behind the model's rows: its dmRow names another image
        deferred.defer(fPath, DeferredRows::Structure);
        msg = "Deferred: snapshot behind the model";
        return false;
    }
    deferred.clearReason(fPath, DeferredRows::Structure);
    const quint8 mf = mv.sync->flags(mv.snap->dmRow(sfRow));
    bool loaded    = mf & RowSync::MetaLoaded;
    bool attempted = mf & RowSync::MetaAttempted;
    if (!loaded) {
        if (attempted) {
            // metadata read tried and failed — offsets are 0, decoder cannot succeed
            msg = "Metadata read failed";
            return false;
        }
        /*  Not read yet: defer, never wait. Watch first, then test again, so a row
            loaded between the two is caught here and a row loaded after the watch is
            posted by DataModel (watchRowLoaded documents the ordering). */
        dm->watchRowLoaded(fPath);
        if (!metaLoadedAt(sfRow)) {
            deferred.defer(fPath, DeferredRows::Meta);
            msg = "Deferred: metadata not loaded";
            return false;
        }
    }
    deferred.clearReason(fPath, DeferredRows::Meta);

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

    /*  One pass, never blocks: okToDecode defers a row whose metadata is not loaded
        yet (DataModel posts rowsChanged when it lands) instead of waiting for it. The
        first eligible row in priority order wins. */
    for (int i = 0; i < toCache.count(); ++i) {
        int sfRow = toCache.at(i);
        QString msg;
        const bool ok = okToDecode(sfRow, id, msg);
        toCacheStatus[sfRow].msg = msg;
        if (ok) return sfRow;
        if (debugThis) sDebug(sId, "row = " + QString::number(sfRow).leftJustified(5) + msg);
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
    checkCacheThread("ImageCache::decodeNextImage");
    QString src = "ImageCache::decodeNextImage";
    if (debugLog || G::isLogger || G::isFlowLogger)
    {
        log("decodeNextImage",
            "decoder " + QString::number(id).leftJustified(3) +
            "row = " + QString::number(sfRow).leftJustified(5) +
            "instance = " + QString::number(instance).leftJustified(5)
            );
    }

    /* Memory pressure gate: this is the single choke point where any new decode starts
       (reached from both launchDecoders and fillCache). While the memory watchdog holds
       the throttle, park this decoder instead of starting another full-resolution decode.
       memoryResume() relaunches decoders once the footprint recovers. Inert when Decode
       Raw is off — small JPEG/HEIC decodes never push the footprint to the threshold. */
    if (memoryThrottled.load(std::memory_order_relaxed)) {
        cycling[id] = false;
        if (debugCaching)
            qDebug().noquote() << src.leftJustified(col0Width, ' ')
                               << "decoder" << QString::number(id).leftJustified(3)
                               << "parked (memory throttle)";
        return;
    }

    /* Raw-decode concurrency cap: bound how many full-sensor RAW decodes run at once so
       their combined float working set stays under the memory cap. Park this decoder if we
       are already at the limit; a finishing sensor decode (fillCache) frees a slot, and the
       remaining active decoders keep the pipeline full. Not applied to JPEG/HEIC or the
       Decode-Raw-off embedded-JPG path (willUseSensorDecode returns false), so that path is
       unchanged and runs at full concurrency. */
    const bool sensorRaw = willUseSensorDecode(sfRow);
    if (sensorRaw) {
        const int limit = rawDecodeLimit(sfRow);
        if (activeRawDecodes.load(std::memory_order_relaxed) >= limit) {
            cycling[id] = false;
            if (debugCaching)
                qDebug().noquote() << src.leftJustified(col0Width, ' ')
                                   << "decoder" << QString::number(id).leftJustified(3)
                                   << "parked (raw decode limit" << limit << ")";
            return;
        }
        const int now = activeRawDecodes.fetch_add(1, std::memory_order_relaxed) + 1;
        decoderIsRaw[id] = true;
        if (now > peakActiveRawDecodes) peakActiveRawDecodes = now;
    }

    // set isCaching
    if (toCacheStatus.contains(sfRow)) {
        toCacheStatus[sfRow].isCaching = true;
        toCacheStatus[sfRow].msg = "";
        toCacheStatus[sfRow].decoderId = id;
        toCacheStatus[sfRow].instance = instance;
    }
    /*  Our state first, the model after -- the model write is queued to the GUI
        thread and is a display mirror, so a decision taken from it would be
        reading whatever had been applied so far rather than what we just set. */
    const QString decPath = pathAt(sfRow);
    decoderPath[id] = decPath;
    decoderGeneration[id] = generation;
    setStateCaching(decPath, true, id);
    const int attempts = bumpStateAttempts(decPath);
    emit setValKey(decPath, G::IsCachingColumn, true, instance, src);
    emit setValKey(decPath, G::DecoderIdColumn, id, instance, src);
    emit setValKey(decPath, G::AttemptsColumn, attempts, instance, src);

    if (debugLog)
    {
    log("decodeNextImage",
        "decoder " + QString::number(id).leftJustified(3) +
        "row = " + QString::number(sfRow).leftJustified(5) +
        "row decoder = " + QString::number(stateOf(decPath).decoderId).leftJustified(3) +
        "isCached = " + QVariant(stateOf(decPath).isCached).toString().leftJustified(6) +
        "isCaching = " + QVariant(stateOf(decPath).isCaching).toString().leftJustified(6) +
        "attempt = " + QString::number(attempts).leftJustified(3) +
        "metaLoaded = " + QVariant(metaLoadedAt(sfRow)).toString().leftJustified(6) +
        "status = " + QString::number(stateOf(decPath).status)
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
            << "attempts =" << stateOf(pathAt(sfRow)).attempts
            << pathAt(sfRow)
            ;
    }

    if (!decoderThreads[id]->isRunning()) decoderThreads[id]->start();

    // emit decode(sfRow, instance);

    bool success = false;
    if (!abort)
        success = QMetaObject::invokeMethod(decoders[id], "decode",
                                  Qt::QueuedConnection,
                                  Q_ARG(int, sfRow), Q_ARG(int, instance),
                                  Q_ARG(QString, decPath));

    if (!success) {
        QString fun = "ImageCache::decodeNextImage";
        qDebug().noquote()
            << fun.leftJustified(col0Width, ' ')
            << "decoder" << QString::number(id).leftJustified(3)
            << "row =" << QString::number(sfRow).leftJustified(4)
            << "invokeMethod failed!"
            ;
        // Clear the isCaching flag we just set, so the row isn't left stranded.
        // okToCache normally does this via fillCache, but if we skip fillCache
        // (abort == true), the DM row stays isCaching=true forever.
        if (toCacheStatus.contains(sfRow)) {
            toCacheStatus[sfRow].isCaching = false;
        }
        setStateCaching(decPath, false, -1);
        emit setValKey(decPath, G::IsCachingColumn, false, instance, fun);
        decoderPath[id].clear();

        if (!abort) fillCache(id);
    }
}

void ImageCache::cacheImage(int id, int sfRow,
                            const QImage &doneImage,
                            const QString &doneFPath,
                            int cacheSource)
{
/*
    Called from fillCache to insert a QImage that has been decoded into icd->imCache.
    Do not cache video files, but do show them as cached for the cache status display.
    doneImage/doneFPath are snapshots from the done() signal; reading
    decoders[id]->image/fPath would race with the next decode() that resets them.
*/
    checkCacheThread("ImageCache::cacheImage");
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
                ;
        }
    }

    const QString &fPath = doneFPath;

    if (fPath.isEmpty() || doneImage.isNull() || !G::useImageCache) {
        // Nothing to insert — snapshot was empty or image failed ICC/rotate post-load.
        return;
    }

    // cache the image (icd->insert handles locking, duplicates, and bytes accounting)
    if (!abort) {
        if (icd->contains(fPath)) {
            /*  Already cached -- by another decoder, a seeded reload or a replace. Keep
                that image, but the row is done: returning before this left it queued,
                so cacheUpToDate stayed false and decoders churned on it (C12). */
            if (toCache.contains(sfRow)) toCacheRemove(sfRow);
            setStateCached(fPath, true);
            return;
        }
        icd->insert(fPath, doneImage);
        cachedCount.fetch_add(1, std::memory_order_relaxed);
        deferred.noteProgress();    // the idle backstop gets its full budget again
    }

     // remove from toCache
    if (!abort) if (toCache.contains(sfRow)) toCacheRemove(sfRow);

    // update our own state, then mirror to the datamodel for the icon badge
    if (!abort) {
        setStateCached(fPath, true);
        emit setCached(sfRow, fPath, cacheSource, instance);
    }

    // Reset attempts so a later trim/re-decode cycle gets a fresh retry budget.
    // The maxAttemptsToCacheImage cap is meant to stop persistent decode failures,
    // not to penalise rows that were cached, trimmed, and need re-caching.
    if (!abort) {
        QMutexLocker lock(&cacheStateMutex);
        cacheState[fPath].attempts = 0;
    }
    if (!abort) emit setValKey(fPath, G::AttemptsColumn, 0, instance, src);

    // // add a thumbnail if missing in datamodel (why?)
    // if (dm->sf->index(sfRow,0).data(Qt::DecorationRole).isNull()) {
    //     // scale to max icon size
    //     QImage im = doneImage.scaled(G::maxIconSize, Qt::KeepAspectRatio);
    //     im.convertTo(QImage::Format_RGB32);
    //     int dmRow = dm->rowFromKey(fPath);
    //     // no connect for setIcon
    //     emit setIcon(dmRow, im, instance, src);
    // }

    // update cache status in cache progress bar
    if (!abort)
        updateStatus(StatusAction::All, "ImageCache::cacheImage");
}

namespace {
DecodeVerdict::StatusClass classOf(ImageDecoder::Status st)
{
    using SC = DecodeVerdict::StatusClass;
    switch (st) {
    case ImageDecoder::Status::Success:       return SC::Success;
    case ImageDecoder::Status::NoGeometry:    return SC::NoGeometry;
    /*  Transient: nothing about the FILE is known to be wrong (an abort, a decode
        attempted while the model changed). Retry-eligible; the attempts cap bounds it. */
    case ImageDecoder::Status::Undefined:
    case ImageDecoder::Status::Abort:
    case ImageDecoder::Status::InstanceClash: return SC::Transient;
    default:                                  return SC::Terminal;
    }
}
} // namespace

bool ImageCache::okToCache(int id, int sfRow, int doneStatus, const QString &doneFPath)
{
/*
    Called by fillCache when a decoder reports back. Returns true to add the image to the
    cache. doneStatus and doneFPath are the snapshot from the done() signal -- the
    decoder's own members race with its next decode.

    EVERY RESULT IS FILED UNDER THE DECODER'S KEY. This used pathAt(sfRow) in the CURRENT
    snapshot, so after a re-sort the status, the IsCaching clear and the superseded test
    landed on whichever image had moved into that row: a terminal status blanked an
    innocent image and a superseded decode of the real one was accepted (C21). The key
    is the path the decoder reported, else the one this thread assigned it.

    What happens to the result is DecodeVerdict::judge (Cache/decodeverdict.h).
*/
    checkCacheThread("ImageCache::okToCache");
    QString src = "ImageCache::okToCache";
    using DecodeVerdict::Verdict;
    const auto st = static_cast<ImageDecoder::Status>(doneStatus);

    const bool validId = id >= 0 && id < decoders.size();
    const QString assigned = validId ? decoderPath[id] : QString();
    if (validId) decoderPath[id].clear();
    const QString key = doneFPath.isEmpty() ? assigned : doneFPath;

    // the decode is over: nothing is caching under either key
    QStringList ended {assigned};
    if (key != assigned) ended << key;
    for (const QString &k : std::as_const(ended)) {
        if (k.isEmpty()) continue;
        setStateCaching(k, false, -1);
        emit setValKey(k, G::IsCachingColumn, false, instance, src);
    }
    if (toCacheStatus.contains(sfRow)) toCacheStatus[sfRow].isCaching = false;

    DecodeVerdict::Result r;
    r.instanceCurrent = !instanceClash(id);
    r.keyInModel = !key.isEmpty() && sfRowOf(key) >= 0;
    r.generationCurrent = validId && decoderGeneration[id] == generation;
    r.superseded = !key.isEmpty() && takeSupersededDecode(key);  // consumes the mark
    r.status = classOf(st);
    const Verdict v = DecodeVerdict::judge(r);

    if (DecodeVerdict::givesAttemptBack(v)) unbumpStateAttempts(key);

    switch (v) {
    case Verdict::Stale:
        /*  A FINISHED DECODE, THROWN AWAY. The file was read and converted while the
            user waited; during a cull these come from filter changes provoked by the
            classification keys -- see the ingest probe's instance-bump tally. The queue
            that asked for it was rebuilt for the new instance. */
        if (G::isIngestProbe && !r.instanceCurrent)
            IngestProbe::Instance().NoteDiscardedDecode();
        return false;

    case Verdict::Superseded: {
        /*  The pixels are of the image as it was (an invalidate) or decoded by the old
            settings (a reload). Still wanted: queued again, so the next decode reads
            what is there now. */
        setStateStatus(key, int(ImageDecoder::Status::Undefined));
        const int row = sfRowOf(key);
        if (row >= targetFirst && row <= targetLast && !toCache.contains(row))
            toCacheAppend(row);
        return false;
    }

    case Verdict::Accept:
        // trimmed from toCache mid-decode: the image is good, accept it, but count it
        if (!toCache.contains(sfRow)) lateDecodeCount.fetch_add(1, std::memory_order_relaxed);
        setStateStatus(key, doneStatus);
        emit setValKey(key, G::DecoderReturnStatusColumn, doneStatus, instance, src);
        return true;

    case Verdict::Defer:
    case Verdict::Retry:
    case Verdict::Fail:
        break;
    }

    // a failure of some kind: record it
    if (debugCaching)
    {
        QString fun = "ImageCache::fillCache FAILED";
        qDebug().noquote()
            << fun.leftJustified(col0Width, ' ')
            << "decoder" << QString::number(id).leftJustified(3)
            << "row =" << QString::number(sfRow).leftJustified((4))
            << "status =" << decoders[id]->statusText.at(doneStatus)
            << "verdict =" << DecodeVerdict::name(v)
            << "errMsg =" << decoders[id]->errMsg
            ;
    }
    const QString msg = validId ? decoders[id]->errMsg : QString();
    setStateStatus(key, doneStatus, msg);
    emit setValKey(key, G::DecoderReturnStatusColumn, doneStatus, instance, src);
    if (!msg.isEmpty()) emit setValKey(key, G::DecoderErrMsgColumn, msg, instance, src);

    /*  No embedded-JPG geometry yet: retried at once it spent the whole attempt budget in
        a tight loop; deferred, the idle backstop retries it (see DeferredRows). */
    if (v == Verdict::Defer) deferred.defer(key, DeferredRows::Geometry);
    // terminal: off the queue. Transient ones stay; maxAttemptsToCacheImage bounds them.
    if (v == Verdict::Fail && toCache.contains(sfRow)) toCacheRemove(sfRow);
    return false;
}

void ImageCache::fillCache(int id,
                           int doneStatus,
                           int doneSfRow,
                           QImage doneImage,
                           QString doneFPath,
                           qint64 doneMsToDecode,
                           int doneCacheSource)
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
    checkCacheThread("ImageCache::fillCache");

    QString src = "";

    /* A decode cycle for this decoder is ending (completed, failed, or invoke-retry). If it
       was a full-sensor RAW decode, release its slot in the concurrency cap so a parked
       decoder can take it. Guarded by the per-decoder flag so it fires exactly once per
       sensor decode regardless of which fillCache path we came in on. */
    if (id >= 0 && id < decoderIsRaw.size() && decoderIsRaw[id]) {
        decoderIsRaw[id] = false;
        activeRawDecodes.fetch_sub(1, std::memory_order_relaxed);
    }

    // Prefer the snapshot delivered via the done() signal (stable copy from emit time).
    // Fall back to live decoder state only for legacy callers that pass no snapshot
    // (sentinel doneStatus == -1), e.g. the invokeMethod-fail retry below.
    const bool haveSnapshot = (doneStatus != -1);
    int cacheRow       = haveSnapshot ? doneSfRow       : decoders[id]->sfRow;
    int effectiveStatus= haveSnapshot ? doneStatus      : int(decoders[id]->status);
    QImage effectiveImage = haveSnapshot ? doneImage    : decoders[id]->image;
    QString effectiveFPath= haveSnapshot ? doneFPath    : decoders[id]->rowKey;
    qint64 effectiveMs    = haveSnapshot ? doneMsToDecode : decoders[id]->nsToDecode;
    int effectiveSource   = haveSnapshot ? doneCacheSource : decoders[id]->cacheSource();

    if (debugCaching)
    {
        QString fun = "ImageCache::fillCache";
        qDebug().noquote()
            << fun.leftJustified(col0Width, ' ')
            << "decoder" << QString::number(id).leftJustified(3)
            << "row =" << QString::number(cacheRow).leftJustified((4))
            << "decoder status =" << decoders[id]->statusText.at(effectiveStatus)
            << "image.w =" << effectiveImage.width()
             ;
    }
    if (debugLog || G::isLogger)
    {
        log("fillCache",
            "decoder " + QString::number(id).leftJustified(3) +
            "row = " + QString::number(cacheRow).leftJustified(4)
        );
    }

    if (okToCache(id, cacheRow, effectiveStatus, effectiveFPath)) {
        if (debugCaching)
        {
            QString fun = "ImageCache::fillCache cache image";
            qDebug().noquote() << fun.leftJustified(col0Width, ' ')
                               << "returning decoder" << id
                               << "row =" << cacheRow
                               << "status =" << effectiveStatus
                               << "status =" << decoders[id]->statusText.at(effectiveStatus)
                               << "errMsg =" << decoders[id]->errMsg
                               << "isRunning =" << imageCacheThread.isRunning()
                ;
        }
        if (!abort) cacheImage(id, cacheRow, effectiveImage, effectiveFPath, effectiveSource);
        // calc average recent decoder time (not being used except reporting)
        decodeHistory(effectiveMs);
    }

    // get next image to cache
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

            bool moreAvailableToCache = icd->count() < rowCountSf();
            if (cushion < cushionLow && moreAvailableToCache) {
                // qDebug() << "ImageCache::fillCache chk cushion =" << cushion;
                ignorePressureRestraints = true;
                relievePressure();
                dispatch();
                return;
            }

            // else update cache progress in status bar
            updateStatus(StatusAction::All, "ImageCache::fillCache after check cacheUpToDate");

            // turn off caching activity lights on statusbar
            emit updateIsRunning(false, true);  // (isRunning, showCacheLabel)

            if (debugLog || G::isLogger || G::isFlowLogger)
                G::log("ImageCache::fillCache", "allDecodersDone");

            // Stall snapshot: decoders idle yet target range still has uncached rows.
            // Indicates rows are blacklisted (maxAttempts) or some other invariant has
            // broken. Throttled to once per 60s.
            if (autoLogStalls && !toCache.isEmpty() && !cacheUpToDate()) {
                const qint64 nowMsec = QDateTime::currentMSecsSinceEpoch();
                if (nowMsec - lastStallSnapshotMs > 60000) {
                    lastStallSnapshotMs = nowMsec;
                    QString path = QStandardPaths::writableLocation(
                                        QStandardPaths::AppDataLocation) + "/Log";
                    QDir().mkpath(path);
                    QFile f(path + "/imagecache_stall.txt");
                    if (f.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
                        QTextStream out(&f);
                        out << "\n\n===== Stall snapshot @ "
                            << QDateTime::currentDateTime().toString(Qt::ISODate)
                            << " =====\n";
                        out << diagnostics();
                    }
                }
            }

            abort = false;

            /*  Idle with rows still queued means every one of them was deferred
                (metadata or geometry not ready) or refused. A deferred row is woken by
                rowsChanged; the backstop covers the ones nothing wakes. */
            armIdleBackstop();

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
        if (id >= rowCountSf()) break;
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
    Called by a new file selection, cache size change, sort, filter or color manage
    change. The cache status is updated (current key, direction of travel,
    priorities and the target range) by calling fillCache with a decoder id = -1.
    Then each ready decoder is sent to fillCache. Decoders are assigned image files,
    which they decode into QImages, and then added to imCache. More details are
    available in the fillCache comments and at the top of this class.
*/
    checkCacheThread("ImageCache::dispatch");
    if (debugCaching || G::isLogger || G::isFlowLogger)
        log("dispatch", "row = " + QVariant(currRow).toString());

    if (debugCaching)
    {
        qDebug().noquote() << "ImageCache::dispatch  row =" << currRow
                           << "autoMaxMB =" << isAutoMaxMB
                           << "maxMB =" << maxMB
            ;
    }

    abort = false;

    // check available memory (another app may have used or released some memory)
    memChk();

    updateToCache();

    // if cache is up-to-date our work is done
    if (cacheUpToDate()) {
        emit updateIsRunning(false, true);   // (isRunning, showCacheLabel)
        return;
    }

    // signal MW cache status
    emit updateIsRunning(true, true);   // (isRunning, showCacheLabel)

    if (!abort) launchDecoders("ImageCache::dispatch");

    // no decoder found anything to do: every queued row is deferred or refused
    if (!anyDecoderCycling()) armIdleBackstop();
}
