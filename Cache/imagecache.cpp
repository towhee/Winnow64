#include "Cache/imagecache.h"
#include "Main/global.h"

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
    concurrent data access. The image cache for QImages reside in the hash table imCache.
    This table is written to by fillCache and read by ImageView.

    ImageDecoders, each in their own thread, signal ImageCache::fillCache for each QImage.
    They form queued connections so that only one decoder at a time can add images to
    imCache.

    The caching process uses the cache size, isCaching, isCached, decoder id, decoder
    return status and metadata read attempted fields from the datamodel to manage the
    priorities and target range.

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

    Every time a new image is selected, setCurrentPosition is called. The target range is
    updated and fillCache is called by each ImageDecoder. When the ImageDecoder has
    loaded a QImage it is sent back to fillCache and added to the ImCache hash.

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
    {
        qDebug() << "ImageCache::ImageCache";
    }
    if (debugLog) log("ImageCache");

    // data is kept in ImageCacheData icd, a hash table
    this->icd = icd;
    this->dm = dm;
    // new metadata to avoid thread collisions?
    metadata = new Metadata;

    // create n decoder threads
    decoderCount = QThread::idealThreadCount();
    decoderCount = decoderCount;
    for (int id = 0; id < decoderCount; ++id) {
        ImageDecoder *d = new ImageDecoder(this, id, dm, metadata);
        d->status = ImageDecoder::Status::Ready;
        decoder.append(d);
        connect(decoder[id], &ImageDecoder::done, this, &ImageCache::fillCache);
    }
    abort = false;
    debugMultiFolders = false;   // rgh remove soon
    debugCaching = false;        // turn on local qDebug
    debugLog = false;            // invoke log without G::isLogger or G::isFlowLogger
}

ImageCache::~ImageCache()
{
    stop("ImageCache::~ImageCache");
}

void ImageCache::clearImageCache(bool includeList)
{
    //if (debugCaching) qDebug() << "ImageCache::clearImageCache";
    log("ClearImageCache");
    gMutex.lock();
    icd->imCache.clear();
    gMutex.unlock();
    updateStatus("Clear", "ImageCache::clearImageCache");
}

void ImageCache::stop(QString src)
{
/*
    Note that initImageCache and updateImageCache both check if isRunning and stop a
    running thread before starting again. Use this function to stop the image caching
    thread without a new one starting when there has been a folder change or when closing
    the program. The cache status label in the status bar will be hidden.
*/
    QString fun = "ImageCache::stop";
    if (debugCaching)
    {
        qDebug().noquote()
            << fun.leftJustified(col0Width, ' ')
            << "src =" << src
            << "isRunning =" << isRunning()
            ;
    }
    log("stop", "isRunning = " + QVariant(isRunning()).toString());

    if (isRunning()) {
        gMutex.lock();
        if (debugLog) log("~ImageCache");
        abort = true;
        condition.wakeOne();
        gMutex.unlock();
        wait();
    }

    // stop decoder threads
    for (int id = 0; id < decoderCount; ++id) {
        decoder[id]->stop();
    }

    abort = false;

    // turn off caching activity lights on statusbar
    emit updateIsRunning(false, false);  // flags = isRunning, showCacheLabel
}

bool ImageCache::allDecodersReady()
{
    // rgh confirm this is right
    for (int id = 0; id < decoderCount; ++id) {
        if (decoder[id]->isRunning()) return false;
        if (decoder[id]->status != ImageDecoder::Status::Ready) return false;
    }
    return true;
}

float ImageCache::getImCacheSize()
{
    // return the current size of the cache
    if (debugCaching) qDebug() << "ImageCache::getImCacheSize";
    if (debugLog) log("getImCacheSize");

    float cacheMB = 0;

    for (auto it = icd->imCache.constBegin(); it != icd->imCache.constEnd(); ++it) {
        QString fPath = it.key();
        int sfRow = dm->proxyRowFromPath(fPath);
        cacheMB += dm->sf->index(sfRow, G::CacheSizeColumn).data().toFloat();
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

void ImageCache::updateToCacheTargets()
{
/*
    Called by setCurrentPosition and fillCache.
*/
    log("updateTargets");

    QString fun = "ImageCache::updateToCacheTargets";
    if (debugCaching)
    {
        qDebug().noquote() << fun.leftJustified(col0Width, ' ')
                           << "current position =" << key
                           << "total =" << dm->sf->rowCount()
            ;
    }

    // rgh resolve key vs sfRow vs ...
    key = dm->currentSfRow;
    setDirection();
    setTargetRange(key);
    resetOutsideTargetRangeCacheState();
    if (debugCaching)
    {
        qDebug().noquote() << fun.leftJustified(col0Width, ' ')
        << "current position =" << key
        << "toCache:" << toCache
            ;
    }
}

bool ImageCache::resetInsideTargetRangeCacheState()
{
/*
    Rapidly updating the cache can result in aborted decoder threads that leave
    isCaching and cached states orphaned.  Reset orphan cached state to false in
    the target range.

    All decoders must be finished before calling this function in order to reeset
    the isCaching and isCached flags.
*/
    QString src = "ImageCache::resetCacheStateInTargetRange";
    if (debugLog) log("resetCacheStateInTargetRange");
    if (debugCaching)
    {
        QString fun = "ImageCache::resetCacheStateInTargetRange";
        qDebug().noquote()
            << fun.leftJustified(col0Width, ' ')
            ;
    }

    QMutexLocker locker(&gMutex);

    // inside target range
    for (int sfRow = targetFirst; sfRow < targetLast; ++sfRow) {
        if (abort) return false;
        if (sfRow >= dm->sf->rowCount()) return false;

        // isCaching: set to false
        if (dm->sf->index(sfRow, G::IsCachingColumn).data().toBool()) {
            emit setValueSf(dm->sf->index(sfRow, G::IsCachingColumn), false, instance, src);
            emit setValueSf(dm->sf->index(sfRow, G::DecoderIdColumn), -1, instance, src);
        }

        // in imCache: then isCached = false and remove from toCache
        QString fPath = dm->sf->index(sfRow, 0).data(G::PathRole).toString();
        if (icd->imCache.contains(fPath)) {
            emit setValueSf(dm->sf->index(sfRow, G::IsCachedColumn), true, instance, src);
            if (toCache.contains(sfRow)) toCache.remove(sfRow);
            continue;
        }

        // not in imCache:
        else {
            emit setValueSf(dm->sf->index(sfRow, G::IsCachedColumn), false, instance, src);
            emit setValueSf(dm->sf->index(sfRow, G::DecoderIdColumn), -1, instance, src);
            emit setValueSf(dm->sf->index(sfRow, G::DecoderReturnStatusColumn),
                           ImageDecoder::Status::Ready, instance, src);
            if (!toCache.contains(sfRow)) toCache.append(sfRow);
        }
    }
    return true;
}

void ImageCache::resetOutsideTargetRangeCacheState()
/*
    Any images in imCache that are no longer in the target range are removed.
*/
{
    QMutexLocker locker(&gMutex);

    QString src = "ImageCache::resetOutsideTargetRangeCacheState";

    // trim imCache outside target range
    auto it = icd->imCache.begin();
    while (it != icd->imCache.end()) {
        QString fPath = it.key();
        int sfRow = dm->proxyRowFromPath(fPath);
        // fPath not in datamodel if sfRow == -1
        if (sfRow < targetFirst || sfRow > targetLast || sfRow == -1) {
            if (debugCaching)
            {
                qDebug().noquote()
                    << src.leftJustified(col0Width, ' ')
                    << "sfRow =" << sfRow
                    << "isCached =" << dm->sf->index(sfRow, G::IsCachedColumn).data().toBool()
                    << "targetFirst =" << targetFirst
                    << "targetLast =" << targetLast
                    << "fPath =" << fPath
                    ;
            }
            it = icd->imCache.erase(it); // Erase and move iterator forward
            emit setValueSf(dm->sf->index(sfRow, G::IsCachedColumn), false, instance, src);
            emit refreshViewsOnCacheChange(fPath, false, "ImageCache::setTargetRange");
        }
        else {
            ++it; // Only move forward if no removal
        }
    }

    // trim toCache outside target range
    QMutableListIterator<int> i(toCache);
    while (i.hasNext()) {
        int value = i.next();
        if (value < targetFirst || value > targetLast) {
            i.remove();
        }
    }

    if (debugCaching)
    {
        qDebug().noquote()
            << src.leftJustified(col0Width, ' ')
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
    if (debugLog) log("setDirection");

    int thisPrevKey = prevKey;
    prevKey = key;
    int thisStep = key - thisPrevKey;

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

void ImageCache::setTargetRange(int key)
{
/*
    The target range is the list of images being targeted to cache, based on the current
    image, the direction of travel, the caching strategy and the maximum memory allotted
    to the image cache.

    The start and end of the target range are determined (targetFirst and targetLast) and
    the boolean isTarget is assigned for each item in the cacheItemList.

    Each item in the target range is added to the toCache list.

    • The direction of travel (dot) dotForward boolean is calculated to indicate whether
      the caching should proceed forward or backward from the current position (key).

    • aheadPos and behindPos specify positions ahead and behind the key, respectively.

    • aheadAmount and behindAmount define how many items to cache in each direction
      during each iteration.

    • The function maintains flags (aheadDone and behindDone) to indicate when caching in
      either direction is complete.
*/
    QMutexLocker locker(&gMutex);

    QString fun = "ImageCache::setTargetRange";
    fun = fun.leftJustified(col0Width, ' ');
    if (G::isLogger) G::log(fun, "maxMB = " + QString::number(maxMB));

    float sumMB = 0;
    int aheadAmount = 2;
    int behindAmount = 1;
    bool aheadDone = false;
    bool behindDone = false;
    // if (!restart)
        toCache.clear();

    int n = dm->sf->rowCount();
    if (key == n - 1) {isForward = false; targetLast = n - 1;}
    if (key == 0)     {isForward = true; targetFirst = 0;}
    int aheadPos = key;
    int behindPos = isForward ? (aheadPos - 1) : (aheadPos + 1);

    if (debugCaching)
    {
        qDebug().noquote() << fun << "current position =" << key << "n =" << n;
    }

    // Iterate while there is space in the cache
    while (sumMB < maxMB && (!aheadDone || !behindDone)) {

        // Handle "ahead" direction
        for (int a = 0; a < aheadAmount && !aheadDone; ++a) {
            // if (aheadPos >= n || aheadPos < 0) break;
            if (isForward ? (aheadPos < n) : (aheadPos >= 0)) {
                // if (debugCaching) {qDebug() << "aheadPos =" << aheadPos;}
                if (!dm->sf->index(aheadPos, G::VideoColumn).data().toBool()) {
                    sumMB +=  dm->sf->index(aheadPos, G::CacheSizeColumn).data().toFloat();
                    QString fPath = dm->sf->index(aheadPos, 0).data(G::PathRole).toString();
                    if (sumMB < maxMB) {
                        if (!toCache.contains(aheadPos) && !icd->imCache.contains(fPath)) {
                            toCache.append(aheadPos);
                        }
                        isForward ? targetLast = aheadPos++ : targetFirst = aheadPos--;
                        // if (debugCaching) {qDebug() << "aheadPos =" << aheadPos;}
                    }
                }
                // isForward ? targetLast = aheadPos++ : targetFirst = aheadPos--;
            }
            else aheadDone = true;
        }

        // Handle "behind" direction
        for (int b = 0; b < behindAmount && !behindDone; ++b) {
            // if (behindPos >= n || behindPos < 0) break;
            if (isForward ? (behindPos >= 0) : (behindPos < n)) {
                // if (debugCaching) {qDebug() << "behindPos =" << behindPos;}
                if (!dm->sf->index(aheadPos, G::VideoColumn).data().toBool()) {
                    sumMB +=  dm->sf->index(behindPos, G::CacheSizeColumn).data().toFloat();
                    QString fPath = dm->sf->index(behindPos, 0).data(G::PathRole).toString();
                    if (sumMB < maxMB) {
                        if (!toCache.contains(behindPos) && !icd->imCache.contains(fPath)) {
                            toCache.append(behindPos);
                        }
                        isForward ? targetFirst = behindPos-- : targetLast = behindPos++;
                        // if (debugCaching) {qDebug() << "behindPos =" << behindPos;}
                    }
                }
                // isForward ? targetFirst = behindPos-- : targetLast = behindPos++;
            }
            else  behindDone = true;
        }
    }

    if (debugCaching)
    {
    qDebug() << fun << "targetFirst =" << targetFirst << "targetLast =" << targetLast;
    qDebug() << fun << "toCache:" << toCache;
    }
}

void ImageCache::removeCachedImage(QString fPath)
{
/*
    Called by MW::insertFiles.  If the inserted file is a replacement of an existing
    image with the same name, then remove the existing image from imCache.  Reset
    isCached, isCaching and attempts.
*/
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

    // QMutexLocker locker(&gMutex);

    // rgh confirm this is working
    icd->imCache.remove(fPath);
    emit refreshViewsOnCacheChange(fPath, false, "ImageCache::setTargetRange");
}

void ImageCache::removeFromCache(QStringList &pathList)
{
/*
    Called when delete image(s).
*/
    if (debugLog) log("removeFromCache");

    // Mutex req'd (do not remove 2023-11-13)
    QMutexLocker locker(&gMutex);

    // rgh confirm this is working
    // remove images from imCache and toCache
    for (int i = 0; i < pathList.count(); ++i) {
        QString fPathToRemove = pathList.at(i);
        icd->imCache.remove(fPathToRemove);
        int sfRow = dm->proxyRowFromPath(fPathToRemove);
        if (toCache.contains(sfRow)) toCache.remove(sfRow);
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

int ImageCache::nextToCache(int id)
{
/*
    The next image to cache is determined by traversing the toCache list in ascending
    order to find the first one not currently being cached:

    • isCaching and isCached are false and attempts < maxAttemptsToCacheImage.

    • decoderId matches item, isCaching is true and isCached = false. If this is the case
      then we know the previous attempt failed, and we should try again if the failure was
      because the file was already open, unless the attempts are greater than
      maxAttemptsToCacheImage.

*/
    auto sDebug = [](const QString& sId, const QString& msg) {
        QString fun = "ImageCache::nextToCache";
        QString s = "";
        qDebug().noquote()
            << fun.leftJustified(50, ' ')
            << sId << msg;
    };

    log("nextToCache", "CacheUpToDate = " + QVariant(cacheUpToDate()).toString());
    QString fun = "ImageCache::nextToCache";
    bool debugThis = false;
    QString msg;
    QString sId = "id = " + QString::number(id).leftJustified(2);

    if (debugCaching)
    {
        qDebug().noquote()
            << fun.leftJustified(col0Width, ' ')
            << "toCache:" << toCache
            ;
    }

    if (instance != dm->instance) {
        if (debugThis) {
            msg = "Instance clash";
            sDebug(sId, msg);
        }
        return -1;
    }

    // QMutexLocker locker(&gMutex);

    if (toCache.isEmpty()) {
        if (debugThis) sDebug(sId, "toCache is empty");
        return -1;
    }

    for (int i = 0; i < toCache.count(); ++i) {
        int sfRow = toCache.at(i);
        QString sRow = QString::number(sfRow).leftJustified(4);
        QString fPath = dm->sf->index(sfRow, 0).data(G::PathRole).toString();

        if (debugThis) {
            msg = "row = " + sRow + " checking" ;
            sDebug(sId, msg);
        }

        // out of range
        if (sfRow >= dm->sf->rowCount()) {
            if (debugThis) {
                msg = "row = " + sRow + " >= dm->sf->rowCount()";
                sDebug(sId, msg);
            }
            continue;
        }

        // make sure metadata has been loaded
        if (!dm->sf->index(sfRow, G::MetadataAttemptedColumn).data().toBool()) {
            if (debugThis) {
                msg = "row " + sRow + " metadata not attempted";
                sDebug(sId, msg);
            }
            continue;
        }

        // already in imCache
        if (icd->imCache.contains(fPath)) {
            if (debugThis) {
                msg = "row " + sRow + " already in imCache";
                sDebug(sId, msg);
            }
            continue;
        }

        // max attempts exceeded
        if (dm->sf->index(sfRow, G::AttemptsColumn).data().toInt() > maxAttemptsToCacheImage) {
            if (debugThis){
                msg = "row " + sRow + " maxAttemptsToCacheImage exceeded";
                sDebug(sId, msg);
            }
            continue;
        }

        // isCaching and not the same decoder
        if (dm->sf->index(sfRow, G::IsCachingColumn).data().toBool() &&
            (id != dm->sf->index(sfRow, G::DecoderIdColumn).data().toInt()))
        {
            if (debugThis) {
                msg = "row " + sRow + " isCaching and not the same decoder";
                sDebug(sId, msg);
            }
            continue;
        }

        // next item to cache
        if (debugThis) {
            msg = "row " + sRow + " success";
            sDebug(sId, msg);
        }
        return sfRow;
    }

    // nothing found to cache
    if (debugCaching)
    {
        qDebug().noquote()
            << fun.leftJustified(col0Width, ' ')
            << "Failed"
            ;
    }
    return -1;
}

bool ImageCache::cacheUpToDate()
{
/*
    Determine if all images in the target range are cached or being cached.  This is only
    used for reporting / diagnostics.
*/
    for (int sfRow = targetFirst; sfRow <= targetLast; ++sfRow) {
        if (!icd->imCache.contains(dm->sf->index(sfRow, 0).data(G::PathRole).toString())) {
            return false;
        }
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
    float currMB = getImCacheSize();
    float roomInCache = maxMB - currMB;
        // int roomInCache = maxMB - currMB;
    if (G::availableMemoryMB < roomInCache) {
        maxMB = currMB + G::availableMemoryMB;
    }
    if (maxMB < minMB) maxMB = minMB;
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
    // rgh get G::showProgress == G::ShowProgress::ImageCache working
    // not all items req'd passed via showCacheStatus since icd->cache struct eliminated
    float currMB = getImCacheSize();
    emit showCacheStatus(instruction, currMB, maxMB, targetFirst, targetLast, source);
}

void ImageCache::log(const QString function, const QString comment)
{
    if (debugLog || ((G::isLogger || G::isFlowLogger) && debugLog)) {
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
    rpt << "retry                    = " << retry;

    rpt << "\n";
    rpt << "instance                 = " << instance << "\n";
    rpt << "G::dmInstance            = " << G::dmInstance << "\n";

    rpt << "\n";
    rpt << "isInitializing           = " << (isInitializing ? "true" : "false") << "\n";
    rpt << "isFinalCheckCompleted    = " << (isFinalCheckCompleted ? "true" : "false") << "\n";
    // rpt << "filterOrSortHasChanged   = " << (filterOrSortHasChanged ? "true" : "false") << "\n";

    rpt << "\n";
    rpt << "totFiles                 = " << dm->sf->rowCount() << "\n";
    rpt << "currMB                   = " << getImCacheSize() << "\n";
    rpt << "maxMB                    = " << maxMB << "\n";
    rpt << "minMB                    = " << minMB << "\n";

    rpt << "\n";
    rpt << "decoderCount             = " << decoderCount << "\n";

    rpt << "\n";
    rpt << "dm->currentSfRow         = " << dm->currentSfRow << "\n";
    rpt << "key                      = " << key << "\n";
    rpt << "prevKey                  = " << prevKey << "\n";

    rpt << "\n";
    rpt << "step                     = " << step << "\n";
    rpt << "sumStep                  = " << sumStep << "\n";
    rpt << "directionChangeThreshold = " << directionChangeThreshold << "\n";
    rpt << "wtAhead                  = " << wtAhead << "\n";
    rpt << "isForward                = " << (isForward ? "true" : "false") << "\n";
    rpt << "targetFirst              = " << targetFirst << "\n";
    rpt << "targetLast               = " << targetLast << "\n";

    rpt << "\n";
    rpt << "toCache count            = " << toCache.count() << "\n";
    rpt << "cacheUpToDate            = " << (cacheUpToDate() ? "true" : "false") << "\n";
    rpt << "\n";
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
    rpt << "\n";
    return reportString;
}

QString ImageCache::reportCacheItemList(QString title)
{
    log("reportCache");
    if (debugCaching) qDebug() << "ImageCache::reportCache";

    QHash<int, QString> rptStatus;
    rptStatus[0] = "Ready";
    rptStatus[1] = "Busy";
    rptStatus[2] = "Pending";
    rptStatus[3] = "Success";
    rptStatus[4] = "Abort";
    rptStatus[5] = "Invalid";
    rptStatus[6] = "Failed";
    rptStatus[7] = "Video";
    rptStatus[8] = "Clash";
    rptStatus[9] = "NoDir";
    rptStatus[10] = "NoPath";
    rptStatus[11] = "NoMeta";
    rptStatus[12] = "FileOpen";

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
                //                << "Updated"
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
                ;
            rpt.setFieldWidth(11);
            rpt << "SizeMB";
            rpt.setFieldWidth(9);
            rpt
                << "Size"
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
            << rptStatus[dm->sf->index(sfRow, G::DecoderReturnStatusColumn).data().toInt()]
            << (icd->imCache.contains(fPath) ? "true" : "false")
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

    if (toCache.isEmpty())
        rpt << "\ntoCache: Empty" << "\n";
    else {
        rpt << "\ntoCache:" << "\n";
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
        int mb;
        QString fPath;
    } imRptItem;
    QList<ImRptItem> rptList;
    for (int i = 0; i < keys.length(); ++i) {
        imRptItem.hashKey = i;
        imRptItem.fPath = keys.at(i);
        imRptItem.sfRow = dm->proxyRowFromPath(imRptItem.fPath);
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
    rpt << "Count";
    // rpt.setFieldWidth(10);
    // rpt << "Priority";
    rpt.setFieldWidth(6);
    rpt << "sfRow" << "W" << "H";
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
        rpt << rptList.at(i).hashKey;
        // rpt.setFieldWidth(10);
        // rpt << rptList.at(item).priorityKey;
        rpt.setFieldWidth(6);
        rpt << rptList.at(i).sfRow;
        rpt << rptList.at(i).w;
        rpt << rptList.at(i).h;
        rpt << rptList.at(i).mb;
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

void ImageCache::initImageCache(int &cacheMaxMB,
                                int &cacheMinMB,
                                bool &isShowCacheStatus,
                                int &cacheWtAhead)
{
    log("initImageCache");
    if (!G::useImageCache) return;   // rgh isolate image cache

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
    if (isRunning()) stop("ImageCache::initImageCache");

    // update folder change instance
    instance = dm->instance;

    // cache management parameters
    key = 0;
    prevKey = -1;
    // the cache defaults to the first image and a forward selection direction
    isForward = true;
    // the amount of memory to allocate to the cache
    maxMB = cacheMaxMB;
    minMB = cacheMinMB;
    isShowCacheStatus = isShowCacheStatus;
    wtAhead = cacheWtAhead;
    targetFirst = 0;
    targetLast = 0;
    directionChangeThreshold = 3;

    if (isShowCacheStatus) {
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
    // gMutex.lock();
    // rgh cache amount fix from pref to here
    maxMB = cacheSizeMB;
    minMB = cacheMinMB;
    isShowCacheStatus = isShowCacheStatus;
    wtAhead = cacheWtAhead;
    // gMutex.unlock();
}

void ImageCache::rebuildImageCacheParameters(QString &currentImageFullPath, QString source)
{
/*
    When the datamodel is filtered the image cache needs to be updated. The cacheItemList is
    rebuilt for the filtered dataset and isCached updated, the current image is set, and any
    surplus cached images (not in the filtered dataset) are removed from imCache.
    The image cache is now ready to run by calling setCachePosition().
*/
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
    key = 0;
    if (isShowCacheStatus) updateStatus("Update all rows", fun);

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
        stop("ImageCache::refreshImageCache");
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
        stop("ImageCache::cacheSizeChang");
    }
    start(QThread::LowestPriority);
}

void ImageCache::datamodelFolderCountChange(QString src)
{
    log("cacheSizeChange");
    if (debugCaching)
        qDebug() << "ImageCache::datamodelFolderCountChange";

    // setTargetRange();
    if (isRunning()) {
        stop("ImageCache::datamodelFolderCountChange");
    }
    start(QThread::LowestPriority);
}

void ImageCache::setCurrentPosition(QString fPath, QString src)
{
/*
    Called from MW::fileSelectionChange to reset the position in the image cache. The image
    cache direction, priorities and target are reset and the cache is updated in fillCache.
*/
    int sfRow = dm->proxyRowFromPath(fPath);
    QString fun = "ImageCache::setCurrentPosition";
    // qDebug() << "ImageCache::setCurrentPosition" << sfRow;
    log(fun, "row = " + QString::number(sfRow));
    // log("setCurrentPosition", "row = " + QString::number(sfRow));
    if (debugCaching) {
        qDebug().noquote() << fun.leftJustified(col0Width, ' ')
                           << "sfRow =" << sfRow
                           << "dm->rowFromPath(fPath) =" << dm->rowFromPath(fPath)
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

    key = dm->currentSfRow;

    // image not cached and not video
    // bool isVideo = dm->sf->index(sfRow, G::VideoColumn).data().toBool();

    // // not in cache, maybe loading
    // if (!icd->imCache.contains(fPath) && !isVideo) {
    //     if (G::mode == "Loupe") emit centralMsg("Loading Image...");
    //     if (!dm->sf->index(sfRow, G::IsCachingColumn).data().toBool()) {
    //         QString msg = "Not in imCache, might be loading.";
    //         G::issue("Warning", msg, "ImageCache::setCurrentPosition", sfRow, fPath);
    //     }
    // }
    // or could not load
    // for (int i = 0; i < dm->sf->rowCount(); ++i) {
    //     if (dm->sf->index(i,0).data(G::PathRole).toString() == fPath) {
    //         if (dm->sf->index(sfRow, G::DecoderReturnStatusColumn).data().toInt() == ImageDecoder::Status::Invalid) {
    //             QString errMsg = dm->sf->index(sfRow, G::DecoderErrMsgColumn).data().toString();
    //             emit centralMsg("Unable to load: " + errMsg + " " +fPath);
    //             QString msg = "Invalid status, unable to load.";
    //             G::issue("Warning", msg, "ImageCache::setCurrentPosition", i, fPath);
    //         }
    //     }
    // }

    if (isRunning()) {
        // reset target range
        updateToCacheTargets();
        launchDecoders(fun);
    }
    else {
        start(QThread::LowestPriority);
    }
}

void ImageCache::decodeNextImage(int id, int sfRow)
{
/*
    Called from fillCache to run a decoder for the next image to decode into a QImage.
    The decoders run in separate threads. When they complete the decoding they signal
    back to fillCache.
*/
    QString src = "ImageCache::decodeNextImage";
    // log ("decodeNextImage");

    // QMutexLocker locker(&gMutex);

    if (!isValidKey(sfRow)) {
        decoder[id]->status = ImageDecoder::Status::Ready;
        return;
    }

    // set isCaching
    emit setValueSf(dm->sf->index(sfRow, G::IsCachingColumn), true, instance, src);
    emit setValueSf(dm->sf->index(sfRow, G::DecoderIdColumn), id, instance, src);
    int attempts = dm->sf->index(sfRow, G::AttemptsColumn).data().toInt();
    emit setValueSf(dm->sf->index(sfRow, G::AttemptsColumn), ++attempts, instance, src);

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
    {
    QString src = "ImageCache::cacheImage";
    QString comment = "decoder " + QString::number(id).leftJustified(3) +
                      "row = " + QString::number(cacheKey).leftJustified(5) +
                      "decoder[id]->fPath =" + decoder[id]->fPath;
    log("cacheImage", comment);
    if (debugCaching)
    {
        QString fun = "ImageCache::cacheImage";
        qDebug().noquote()
               << fun.leftJustified(col0Width, ' ')
               << "decoder" << QString::number(id).leftJustified(2)
               << "row =" << QString::number(cacheKey).leftJustified((4))
               << "G::mode =" << G::mode
               << "errMsg =" << decoder[id]->errMsg
               << "decoder[id]->fPath =" << decoder[id]->fPath
                ;
    }
    }

    QString src = "ImageCache::cacheImage";

    // check if null image
    if (decoder[id]->image.width() <= 0) {
        QString msg = "Decoder returned a null image.";
        G::issue("Warning", msg, "ImageCache::cacheImage", cacheKey, decoder[id]->fPath);
        return;
    }

    // cache the image
    icd->imCache.insert(decoder[id]->fPath, decoder[id]->image);

    // remove from toCache
    if (toCache.contains(cacheKey)) toCache.remove(toCache.indexOf(cacheKey));

    // reset caching and cache flags
    emit setValueSf(dm->sf->index(cacheKey, G::IsCachingColumn), false, instance, src);
    emit setValueSf(dm->sf->index(cacheKey, G::IsCachedColumn), true, instance, src);

    // reset attempts rgh review re nextToCache check (not working)
    emit setValueSf(dm->sf->index(cacheKey, G::AttemptsColumn), 0, instance, src);

    // refresh thumbs (and main image if is current)
    emit refreshViewsOnCacheChange(decoder[id]->fPath, true, "ImageCache::cacheImage");

    // if current image signal ImageView::loadImage
    // if (decoder[id]->fPath == dm->currentFilePath) {
        // clear "Loading Image..." central msg in setCurrentPosition (not being used)
        // emit centralMsg("");
        // load in ImageView
        // emit loadImage(decoder[id]->fPath, "ImageCache::cacheImage");
        // revert central view (req'd? see setCurrentPosition)
        // emit imageCachePrevCentralView();
    // }

    decoder[id]->status = ImageDecoder::Status::Ready;
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
    log("fillCache",
        "decoder " + QString::number(id).leftJustified(3) +
        "row = " + QString::number(decoder[id]->sfRow).leftJustified(5)
    );

    // check if aborted
    if (abort || decoder[id]->status == ImageDecoder::Status::Abort) {
        emit setValueSf(dm->sf->index(cacheKey, G::IsCachingColumn), false, instance, src);
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
        emit setValueSf(dm->sf->index(cacheKey, G::IsCachingColumn), false, instance, src);
        /* more direct version
        // dm->sf->setData(dm->sf->index(cacheKey, G::IsCachingColumn), false);

        // qDebug() << "ImageCache::fillCache errMsg" << cacheKey << id << decoder[id]->errMsg;
        // dm->sf->setData(dm->sf->index(cacheKey, G::DecoderReturnStatusColumn),
        //                 static_cast<int>(decoder[id]->status));
        //*/
        emit setValueSf(dm->sf->index(cacheKey, G::DecoderReturnStatusColumn),
                       static_cast<int>(decoder[id]->status), instance, src);
        if (decoder[id]->status == ImageDecoder::Status::Video) {
            emit setValueSf(dm->sf->index(cacheKey, G::IsCachedColumn), true, instance, src);
        }
        if (decoder[id]->errMsg != "") {
            emit setValueSf(dm->sf->index(cacheKey, G::DecoderErrMsgColumn), decoder[id]->errMsg, instance, src);
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
            emit setValueSf(dm->sf->index(cacheKey, G::IsCachingColumn), false, instance, src);
            emit setValueSf(dm->sf->index(cacheKey, G::IsCachedColumn), false, instance, src);
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

    // get next image to cache
    int toCacheKey = nextToCache(id);
    bool isNextToCache = toCacheKey != -1;
    bool isCacheUpToDate = toCache.isEmpty();
    bool isCacheKeyOk = isValidKey(toCacheKey);
    bool okDecodeNextImage = !abort && !isCacheUpToDate && isNextToCache && isCacheKeyOk;

    if (debugCaching)
    {
        QString fun = "ImageCache::fillCache dispatch";
        qDebug().noquote() << fun.leftJustified(col0Width, ' ')
                       << "decoder" << QString::number(id).leftJustified(2)
                       << "row =" << QString::number(toCacheKey).leftJustified(4)
                       << "decoder status =" << decoder[id]->statusText.at(decoder[id]->status)
                       << "abort =" << abort
                       << "isCacheUpToDate =" << isCacheUpToDate
                       << "isNextToCache =" << isNextToCache
                       << "isValidKey(toCacheKey) =" << isValidKey(toCacheKey)
                       << "okDecodeNextImage =" << okDecodeNextImage
                       << "toCache =" << toCache
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
                           << "row =" << QString::number(toCacheKey).leftJustified(4)
                           << "decoder status =" << decoder[id]->statusText.at(decoder[id]->status)
                           << decoder[id]->fPath
            ;
        }
        decodeNextImage(id, toCacheKey);
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

        /* Both MetaRead and ImageCache are running at the same time, and sometimes
        ImageCache catches up to MetaRead and the datamodel has not been loaded/updated
        for the next item to cache. For MetaRead updates to be executed ImageCache must
        be stopped as ImageCache is blocking the updates. */

        // if this is the last active decoder, restart if cacheUpToDate == false
        if (!abort && instance == dm->instance && allDecodersDone && retry < 5) {
            if (!toCache.isEmpty() && retry++ < 5) {
                if (debugCaching)
                {
                    QString fun = "ImageCache::fillCache Final check";
                    qDebug() << " ";
                    qDebug().noquote()
                        << fun.leftJustified(col0Width, ' ')
                        << "retry =" << retry
                        << "toCache:" << toCache
                        ;
                }
                // gMutex.lock();
                resetInsideTargetRangeCacheState();
                resetOutsideTargetRangeCacheState();
                setTargetRange(dm->currentSfRow);
                // return;
            }
        }

        // Ok, now we are really done or quit after retrying
        retry = 0;

        // update cache progress in status bar
        if (isShowCacheStatus) {
            updateStatus("Update all rows", "ImageCache::fillCache after check cacheUpToDate");
        }

        if (allDecodersDone)
        {
            // turn off caching activity lights on statusbar
            emit updateIsRunning(false, true);  // (isRunning, showCacheLabel)

            if (debugCaching)
            {
            QString fun = "ImageCache::fillCache Finished";
            if (cacheKey != -1)
                qDebug().noquote()
                    << fun.leftJustified(col0Width, ' ')
                    // << "decoder" <<  QString::number(id).leftJustified(2)
                    // << "row =" << QString::number(cacheKey).leftJustified(4)
                    << "allDecodersDone =" << QVariant(allDecodersDone).toString()
                    // << "cacheUpToDate = true: decoder set to Ready state"
                    ;
            }
        }
    }
}

void ImageCache::launchDecoders(QString src)
{
    QString fun = "ImageCache::launchDecoders";
    // qDebug().noquote() << fun.leftJustified(col0Width, ' ') << "src =" << src;

    for (int id = 0; id < decoderCount; ++id) {
        if (debugCaching)
        {
            qDebug().noquote()
            << fun.leftJustified(col0Width, ' ')
            << "Decoder" << QVariant(id).toString().leftJustified(3)
            << "rowCount =" << dm->sf->rowCount()
            << "status =" << decoder[id]->statusText.at(decoder[id]->status)
            << "cacheUpToDate =" << cacheUpToDate()
            << "isRunning =" << isRunning()
                ;
        }

        if (decoder[id]->isRunning()) continue;

        bool isCacheUpToDate = cacheUpToDate();
        if (id >= dm->sf->rowCount()) {
            if (debugCaching)
            {
                qDebug().noquote()
                    << fun.leftJustified(col0Width, ' ')
                    << "Less rows in model than decoders =" << id
                    ;
            }
            return;
        }


        if (isCacheUpToDate) {
            emit updateIsRunning(false, true);  // (isRunning, showCacheLabel)
            QString msg = "id = " + QString::number(id).leftJustified(2) +
                          "cacheUpToDate = " + QVariant(isCacheUpToDate).toString() +
                          " status = " + decoder[id]->statusText.at(decoder[id]->status)
                ;
            log("launchDecoders", msg);
            break;
        }
        if (decoder[id]->status == ImageDecoder::Status::Ready) {
            decoder[id]->fPath = "";
            // if (debugLog)
            {
                QString msg = "id = " + QString::number(id).leftJustified(2) +
                              // "cacheUpToDate = " + QVariant(isCacheUpToDate).toString() +
                              " status = Ready, calling fillCache"
                              ;
            }
            QString msg = "decoder " + QString::number(id).leftJustified(3) +
                          "status = " + decoder[id]->statusText.at(decoder[id]->status)
                ;
            log("launchDecoders launch:", msg);
            if (debugCaching)
            {
            qDebug().noquote()
                << fun.leftJustified(col0Width, ' ')
                << "Decoder" << QVariant(id).toString().leftJustified(3)
                << "calling fillCache"
                ;
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
    updateToCacheTargets();

    // if cache is up-to-date our work is done
    //if (cacheUpToDate()) return;

    // signal MW cache status
    emit updateIsRunning(true, true);   // (isRunning, showCacheLabel)

    launchDecoders("ImageCache::run");
}
