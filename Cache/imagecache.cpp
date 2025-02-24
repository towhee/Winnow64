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

    // create n decoder threads
    decoderCount = QThread::idealThreadCount();
    for (int id = 0; id < decoderCount; ++id) {
        ImageDecoder *decoder = new ImageDecoder(id, dm, metadata);
        QThread *thread = new QThread;

        decoder->moveToThread(thread);
        connect(decoder, &ImageDecoder::done, this, &ImageCache::fillCache);

        thread->start();
        decoders.append(decoder);
        decoderThreads.append(thread);
        cycling.append(false);
    }

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
            << "isRunning =" << imageCacheThread.isRunning()
            ;
    }
    if (debugLog || G::isLogger)
        log("stop", "isRunning = " + QVariant(imageCacheThread.isRunning()).toString());

    if (imageCacheThread.isRunning()) {
        abort = true;
        imageCacheThread.quit();
        imageCacheThread.wait();

        // Stop all decoder threads first
        for (int id = 0; id < decoderCount; ++id) {
            QMetaObject::invokeMethod(decoders[id], "stop", Qt::QueuedConnection);
        }
    }

    abort = false;

    // turn off caching activity lights on statusbar
    emit updateIsRunning(false, false);  // flags = isRunning, showCacheLabel
}

// void ImageCache::startProcessing()
// {
//     while (!abort) {  // Keep processing while abort is false
//         qDebug() << "Processing images...";
//         QTimer::singleShot(0, this, &ImageCache::startProcessing);
//     }
//     qDebug() << "Processing stopped, but event loop is running.";
// }

void ImageCache::abortProcessing()
{
    abort = true;

    // static QElapsedTimer timer;
    // static bool firstRun = true;

    // QString fun = "ImageCache::abortProcessing";
    // // if (debugCaching)
    // {
    //     qDebug().noquote()
    //     << fun.leftJustified(col0Width, ' ')
    //     << "firstRun =" << firstRun
    //         ;
    // }

    // if (firstRun) {
    //     timer.start();  // Start the timer on the first run
    //     firstRun = false;
    //     abort = true;
    // }

    // if (!abort && timer.elapsed() < abortDelay) {
    //     QTimer::singleShot(0, this, &ImageCache::startProcessing);
    // } else {
    //     firstRun = true;  // Reset when stopping
    // }
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

float ImageCache::getImCacheSize()
{
    // return the current size of the cache
    if (debugLog || G::isLogger) log("getImCacheSize");

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
    if (dm->sf->index(sfRow, G::MetadataLoadedColumn).data().toBool()) {
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

    emit waitingForRow(sfRow);

    while(!isLoaded) {
        if (!condition.wait(&gMutex, ms - t.elapsed())) break;
        isLoaded = dm->sf->index(sfRow, G::MetadataLoadedColumn).data().toBool();
        if (isLoaded) break;
    }

    if (debugCaching)
    fun += " wait";
    {
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
    if (debugLog || G::isLogger || G::isFlowLogger) log("updateToCache");

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
    if (!abort) setTargetRange(currRow);
    if (!abort) trimOutsideTargetRange();

    if (debugCaching)
    {
        qDebug().noquote() << fun.leftJustified(col0Width, ' ')
                           << "current position =" << currRow
        << "toCache:" << toCache
            ;
    }
}

void ImageCache::trimOutsideTargetRange()
/*
    Any images in imCache that are no longer in the target range are removed.
*/
{
    QString src = "ImageCache::trimOutsideTargetRange";
    if (debugCaching) qDebug() << src;

    // trim imCache outside target range
    {
        auto it = icd->imCache.begin();
        while (it != icd->imCache.end()) {
            QString fPath = it.key();
            int sfRow = dm->proxyRowFromPath(fPath);
            // fPath not in datamodel if sfRow == -1
            if (sfRow < targetFirst || sfRow > targetLast) {
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
                emit setCached(sfRow, false, instance);
                // emit setValueSf(dm->sf->index(sfRow, G::IsCachedColumn), false, instance, src);
                // emit refreshViews(fPath, false, "ImageCache::trimOutsideTargetRange");
            }
            else {
                ++it; // move forward if no removal
            }
        }
    }

    // trim toCacheStatus outside target range
    for (int sfRow : toCache) {
        if (sfRow < targetFirst || sfRow > targetLast) {
            toCacheStatus.remove(sfRow);
            emit setValueSf(dm->sf->index(sfRow, G::IsCachingColumn), false, instance, src);
            emit setCached(sfRow, false, instance);
            // emit setValueSf(dm->sf->index(sfRow, G::IsCachedColumn), false, instance, src);
        }
    }

    // trim toCache outside target range
    auto it = toCache.begin();  // Ensures a mutable iterator
    toCache.erase(std::remove_if(it, toCache.end(), [&](int sfRow) {
                      return sfRow < targetFirst || sfRow > targetLast;
                  }), toCache.end());

    // // cleanup datamodel outside target range
    // for (int sfRow = 0; sfRow < dm->sf->rowCount(); ++sfRow) {
    //     if (sfRow >= targetFirst && sfRow <= targetLast) continue;
    //     if (dm->sf->index(sfRow, G::IsCachingColumn).data().toBool())
    //         emit setValueSf(dm->sf->index(sfRow, G::IsCachingColumn), false, instance, src);
    //     if (dm->sf->index(sfRow, G::IsCachedColumn).data().toBool())
    //         emit setValueSf(dm->sf->index(sfRow, G::IsCachedColumn), false, instance, src);
    // }

    if (debugCaching)
    {
        qDebug().noquote()
            << src.leftJustified(col0Width, ' ')
            << "toCache:" << toCache

            ;
    }

    updateStatus("Update all rows", src);
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
    // /* ChatGPT version
    QString fun = "ImageCache::setTargetRange";

    float sumMB = 0;
    const int aheadAmount = 2;
    const int behindAmount = 1;
    bool aheadDone = false;
    bool behindDone = false;

    int n = dm->sf->rowCount();
    if (key == n - 1) { isForward = false; targetLast = n - 1; }
    if (key == 0)     { isForward = true; targetFirst = 0; }

    int aheadPos = key;
    int behindPos = isForward ? (aheadPos - 1) : (aheadPos + 1);

    // Iterate while there is space in the cache
    while (sumMB < maxMB && (!aheadDone || !behindDone)) {
        // if (abort.load()) return;

        // Handle "ahead" direction
        for (int a = 0; a < aheadAmount && !aheadDone; ++a) {

            bool isValidRow = false;
            bool isVideo = false;
            float cacheSize = 0;
            QString fPath;

            // Fetch metadata safely from the GUI thread
            QMetaObject::invokeMethod(dm->sf, [&] {
                isValidRow = isForward ? (aheadPos < n) : (aheadPos >= 0);
                if (isValidRow) {
                    isVideo = dm->sf->index(aheadPos, G::VideoColumn).data().toBool();
                    cacheSize = dm->sf->index(aheadPos, G::CacheSizeColumn).data().toFloat();
                    fPath = dm->sf->index(aheadPos, 0).data(G::PathRole).toString();
                }
            }, Qt::BlockingQueuedConnection);

            if (!isValidRow) {
                aheadDone = true;
                break;
            }

            if (!isVideo) {
                sumMB += cacheSize;
                if (sumMB < maxMB) {
                    if (!toCache.contains(aheadPos) && !icd->imCache.contains(fPath)) {
                        toCacheAppend(aheadPos);
                    }
                    isForward ? targetLast = aheadPos : targetFirst = aheadPos;
                }
            }
            isForward ? aheadPos++ : aheadPos--;
        }

        // Handle "behind" direction
        for (int b = 0; b < behindAmount && !behindDone; ++b) {

            bool isValidRow = false;
            bool isVideo = false;
            float cacheSize = 0;
            QString fPath;

            // Fetch metadata safely from the GUI thread
            QMetaObject::invokeMethod(dm->sf, [&] {
                isValidRow = isForward ? (behindPos >= 0) : (behindPos < n);
                if (isValidRow) {
                    isVideo = dm->sf->index(behindPos, G::VideoColumn).data().toBool();
                    cacheSize = dm->sf->index(behindPos, G::CacheSizeColumn).data().toFloat();
                    fPath = dm->sf->index(behindPos, 0).data(G::PathRole).toString();
                }
            }, Qt::BlockingQueuedConnection);

            if (!isValidRow) {
                behindDone = true;
                break;
            }

            if (!isVideo) {
                sumMB += cacheSize;
                if (sumMB < maxMB) {
                    if (!toCache.contains(behindPos) && !icd->imCache.contains(fPath)) {
                        toCacheAppend(behindPos);
                    }
                    isForward ? targetFirst = behindPos : targetLast = behindPos;
                }
            }
            isForward ? behindPos-- : behindPos++;
        }
    }

    // if (debugCaching)
    {
        qDebug().noquote()
        << fun << "targetFirst =" << targetFirst << "targetLast =" << targetLast
        << toCache;
    }
    //*/

    /* Rory version

    QString fun = "ImageCache::setTargetRange";
    fun = fun.leftJustified(col0Width, ' ');
    // if (G::isLogger) G::log(fun, "maxMB = " + QString::number(maxMB));

    if (debugCaching)
    qDebug() << "setTargetRange"
             << "key =" << key
             << "threadId =" << QThread::currentThread();

    float sumMB = 0;
    int aheadAmount = 2;
    int behindAmount = 1;
    bool aheadDone = false;
    bool behindDone = false;

    int n = dm->sf->rowCount();
    if (key == n - 1) {isForward = false; targetLast = n - 1;}
    if (key == 0)     {isForward = true; targetFirst = 0;}
    int aheadPos = key;
    int behindPos = isForward ? (aheadPos - 1) : (aheadPos + 1);

    // if (debugCaching)
    {
        if (G::isGuiThread())
        qDebug().noquote() << fun.leftJustified(col0Width, ' ')
                           << "current position =" << key << "n =" << n
                           << "RUNNING IN GUI THREAD"
            ;
    }

    // Iterate while there is space in the cache
    while (sumMB < maxMB && (!aheadDone || !behindDone)) {
        if (abort) return;
        // Handle "ahead" direction
        for (int a = 0; a < aheadAmount && !aheadDone; ++a) {
            // if (!waitForMetaRead(aheadPos)) abort = false;
            if (isForward ? (aheadPos < n) : (aheadPos >= 0)) {
                // if (debugCaching) {qDebug() << "aheadPos =" << aheadPos;}
                if (!dm->sf->index(aheadPos, G::VideoColumn).data().toBool()) {
                    sumMB +=  dm->sf->index(aheadPos, G::CacheSizeColumn).data().toFloat();
                    QString fPath = dm->sf->index(aheadPos, 0).data(G::PathRole).toString();
                    if (sumMB < maxMB) {
                        if (!toCache.contains(aheadPos) && !icd->imCache.contains(fPath)) {
                            if (abort) return;
                            toCacheAppend(aheadPos);
                        }
                        isForward ? targetLast = aheadPos : targetFirst = aheadPos;
                        // if (debugCaching) {qDebug() << "aheadPos =" << aheadPos;}
                    }
                }
                isForward ? aheadPos++ : aheadPos--;
            }
            else aheadDone = true;
        }

        // Handle "behind" direction
        for (int b = 0; b < behindAmount && !behindDone; ++b) {
            // if (!waitForMetaRead(behindPos)) abort = false;
            if (isForward ? (behindPos >= 0) : (behindPos < n)) {
                // if (debugCaching) {qDebug() << "behindPos =" << behindPos;}
                if (!dm->sf->index(behindPos, G::VideoColumn).data().toBool()) {
                    sumMB +=  dm->sf->index(behindPos, G::CacheSizeColumn).data().toFloat();
                    QString fPath = dm->sf->index(behindPos, 0).data(G::PathRole).toString();
                    if (sumMB < maxMB) {
                        if (!toCache.contains(behindPos) && !icd->imCache.contains(fPath)) {
                            if (abort) return;
                            toCacheAppend(behindPos);
                        }
                        isForward ? targetFirst = behindPos : targetLast = behindPos;
                        // if (debugCaching) {qDebug() << "behindPos =" << behindPos;}
                    }
                }
                isForward ? behindPos-- : behindPos++;
            }
            else  behindDone = true;
        }
    }

    // if (debugCaching)
    {
    qDebug().noquote()
             << fun << "targetFirst =" << targetFirst << "targetLast =" << targetLast
             << toCache;
    }
    //*/
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
        icd->imCache.remove(fPathToRemove);
        int sfRow = dm->proxyRowFromPath(fPathToRemove);
        toCacheRemove(sfRow);
    }
}

void ImageCache::rename(QString oldPath, QString newPath)
/*
    Called by MW::renameSelectedFiles then RenameFileDlg::renameDatamodel.
*/
{
    if (debugLog || G::isLogger) log("rename");
    if (icd->imCache.contains(oldPath)) {
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
    if (debugLog || G::isLogger) log("memChk");
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

    rpt << "\n";
    rpt << "totFiles                 = " << dm->sf->rowCount() << "\n";
    rpt << "currMB                   = " << getImCacheSize() << "\n";
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
            << rptStatus.at(dm->sf->index(sfRow, G::DecoderReturnStatusColumn).data().toInt())
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
        int sfRow = dm->proxyRowFromPath(fPath);
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

void ImageCache::initImageCache(int cacheMaxMB,
                                int cacheMinMB,
                                bool isShowCacheStatus,
                                int cacheWtAhead)
{
    if (G::isLogger || G::isFlowLogger) log("initImageCache");
    if (!G::useImageCache) return;   // rgh isolate image cache

    QString fun = "ImageCache::initImageCache";
    if (debugCaching) {
        qDebug().noquote()
            << fun.leftJustified(col0Width, ' ')
            << "dm->instance =" << dm->instance
            ;
    }

    QMutexLocker locker(&gMutex);   // req'd 2025-02-19

    // reset the image cache
    icd->imCache.clear();
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

    isInitializing = false;

    source = "ImageCache::initImageCache";
}

void ImageCache::updateImageCacheParam(int cacheSizeMB,
                                       int cacheMinMB,
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
    // if (debugCaching)
    {
        qDebug().noquote()
            << fun.leftJustified(col0Width, ' ')
            << "dm->sf->rowCount() =" << dm->sf->rowCount()
            << "currentImageFullPath =" << currentImageFullPath
            ;
    }
    if (dm->sf->rowCount() == 0) return;

    // do not use mutex - spins forever

    // // reset the image cache
    // icd->imCache.clear();
    // toCache.clear();
    // toCacheStatus.clear();

    // update instance
    instance = dm->instance;

    // if (isShowCacheStatus) updateStatus("Update all rows", fun);
    // refreshImageCache();

    // setCurrentPosition(currentImageFullPath, fun);
}

void ImageCache::refreshImageCache()
{
    if (debugLog || G::isLogger || G::isFlowLogger) log("refreshImageCache");
    QString fun = "ImageCache::refreshImageCache";
    // if (debugCaching)
    {
        qDebug().noquote() << fun.leftJustified(col0Width, ' ')
                           << "isRunning =" << imageCacheThread.isRunning();
    }
    // if (imageCacheThread.isRunning()) {
    //     QMetaObject::invokeMethod(this, "updateToCache", Qt::QueuedConnection);
    // }

    abort = false;
    QMetaObject::invokeMethod(this, "updateToCache", Qt::QueuedConnection);
    dispatch();
}

void ImageCache::reloadImageCache()
{
/*
    Reload all images in the cache.  Used by colorManageChange.
*/
    // if (debugCaching)
        qDebug() << "ImageCache::colorManageChange";
    if (debugLog || G::isLogger || G::isFlowLogger) log("reloadImageCache");
    // QMutexLocker locker(&gMutex);   // req'd 2025-02-19
    abort = false;
    // reset the image cache
    icd->imCache.clear();
    toCache.clear();
    toCacheStatus.clear();
    dispatch();
}

void ImageCache::colorManageChange()
{
/*
    Called when color manage is toggled.  Reload all images in the cache.
*/
    // if (debugCaching)
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
    // if (debugLog || G::isLogger || G::isFlowLogger) log("cacheSizeChange");
    // if (debugCaching)
    //     qDebug() << "ImageCache::cacheSizeChange";

    // if (imageCacheThread.isRunning()) {
    //     abortProcessing();
    // }
    // start(QThread::LowestPriority);
}

void ImageCache::datamodelFolderCountChange(QString src)
{
    if (debugLog || G::isLogger || G::isFlowLogger)
        log("datamodelFolderCountChange", "src = " + src);
    QString fun = "ImageCache::datamodelFolderCountChange";
    // if (debugCaching)
    {
        qDebug().noquote() << fun.leftJustified(col0Width, ' ')
                           << "src =" << src
                           << "isRunning =" << imageCacheThread.isRunning();
    }
    if (imageCacheThread.isRunning()) {
        // Use signal-slot mechanism to ensure execution in ImageCache thread
        QMetaObject::invokeMethod(this, "updateToCache", Qt::QueuedConnection);
    }
    else {
        // start(QThread::LowestPriority);
    }
}

void ImageCache::setCurrentPosition(QString fPath, QString src)
{
/*
    Called from MW::fileSelectionChange to reset the position in the image cache. The image
    cache direction, priorities and target are reset and the cache is updated in fillCache.

    Do not use QMutexLocker. Can cause deadlock in updateTargets()

*/
    currRow = dm->proxyRowFromPath(fPath);

    QString fun = "ImageCache::setCurrentPosition";
    if (debugLog || G::isLogger || G::isFlowLogger)
        log("setCurrentPosition", "row = " + QString::number(currRow));
    // if (debugCaching)
    {
        qDebug().noquote() << fun.leftJustified(col0Width, ' ')
        << "currRow =" << currRow
        << fPath
        << "src =" << src
        << "isRunning =" << imageCacheThread.isRunning();
    }

    static int prevRow = -1;
    static int prevInstance = -1;

    if (prevRow == currRow && prevInstance == instance) {
        return;
    }
    prevRow = currRow;
    prevInstance = instance;

    if (G::dmInstance != instance) {
        // if (debugCaching)
        {
            QString msg = "Instance clash from " + src;
            G::issue("Comment", msg, "ImageCache::setCurrentPosition", currRow, fPath);
            qWarning() << "ImageCache::setCurrentPosition cancelled instance change row =" << currRow;
        }
        return;
    }

    if (isInitializing) {
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

    // out of range
    if (sfRow >= dm->sf->rowCount()) {
        msg = "Out of range";
        return false;
    }

    // if (!dm->sf->index(sfRow, G::MetadataAttemptedColumn).data().toBool()) {
    //     msg = "Metadata not attempted";
    //     return false;
    // }

    // already in imCache
    QString fPath = dm->sf->index(sfRow, 0).data(G::PathRole).toString();
    if (icd->imCache.contains(fPath)) {
        msg = "Already in imCache";
        return false;
    }

    // max attempts exceeded
    if (dm->sf->index(sfRow, G::AttemptsColumn).data().toInt() > maxAttemptsToCacheImage) {
        msg = "Max attempts exceeded";
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
    if (!dm->sf->index(sfRow, G::MetadataLoadedColumn).data().toBool()) {
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

    // // ImageCache instance out-of-date
    // if (instance != dm->instance) {
    //     if (debugThis) {
    //         msg = "Instance clash";
    //         sDebug(sId, msg);
    //     }
    //     return -1;
    // }

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

    // if (!isValidKey(sfRow)) {
    //     qDebug() << src << "row =" << sfRow << "is invalid";
    //     return;
    // }

    // set isCaching
    if (toCacheStatus.contains(sfRow)) {
        toCacheStatus[sfRow].isCaching = true;
        toCacheStatus[sfRow].msg = "";
        toCacheStatus[sfRow].decoderId = id;
        toCacheStatus[sfRow].instance = instance;
    }
    emit setValueSf(dm->sf->index(sfRow, G::IsCachingColumn), true, instance, src);
    emit setValueSf(dm->sf->index(sfRow, G::DecoderIdColumn), id, instance, src);
    int attempts = dm->sf->index(sfRow, G::AttemptsColumn).data().toInt();
    attempts++;
    // qDebug() << src << "row =" << sfRow << "attempts =" << attempts;
    emit setValueSf(dm->sf->index(sfRow, G::AttemptsColumn), attempts, instance, src);

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

    // if (!abort)
    bool success = QMetaObject::invokeMethod(decoders[id], "decode",
                                  Qt::QueuedConnection,
                                  Q_ARG(int, sfRow), Q_ARG(int, instance));

    // bool success = QMetaObject::invokeMethod(decoders[id], "decode",
    //                           Qt::DirectConnection,
    //                           Q_ARG(int, sfRow), Q_ARG(int, instance));

    if (!success) {
        QString fun = "ImageCache::decodeNextImage";
        qDebug().noquote()
            << fun.leftJustified(col0Width, ' ')
            << "decoder" << QString::number(id).leftJustified(3)
            << "row =" << QString::number(sfRow).leftJustified(4)
            << "invokeMethod failed!"
            ;
        fillCache(id);
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
    QString comment = "decoder " + QString::number(id).leftJustified(3) +
                      "row = " + QString::number(sfRow).leftJustified(5) +
                      reportImCacheRows();
                      // "fPath = " + decoder[id]->fPath;
    if (G::isLogger || G::isFlowLogger) log("cacheImage", comment);
    if (debugCaching)
    {
        qDebug().noquote()
               << src.leftJustified(col0Width, ' ')
               << "decoder" << QString::number(id).leftJustified(3)
               << "row =" << QString::number(sfRow).leftJustified((4))
               // << "G::mode =" << G::mode
               // << "errMsg =" << decoders[id]->errMsg
               << "decoder[id]->fPath =" << decoders[id]->fPath
                ;
    }
    }

    // // check if null image
    // if (decoders[id]->image.width() <= 0) {
    //     QString msg = "Decoder returned a null image.";
    //     G::issue("Warning", msg, "ImageCache::cacheImage", sfRow, decoders[id]->fPath);
    //     if (toCacheStatus.contains(sfRow))
    //         toCacheStatus[sfRow].isCaching = false;
    //     return;
    // }

    gMutex.lock();  // req'd to prevent toCacheAppend() crash

    // cache the image
    icd->imCache.insert(decoders[id]->fPath, decoders[id]->image);

    // // remove from toCache
    if (toCache.contains(sfRow)) toCacheRemove(sfRow);

    gMutex.unlock();

    // if (!icd->imCache.contains(decoders[id]->fPath))
    //     qDebug() << src << "INSERT FAILED for row" << sfRow;

    // reset caching and cache flags
    // emit setValueSf(dm->sf->index(sfRow, G::IsCachingColumn), false, instance, src);
    // emit setValueSf(dm->sf->index(sfRow, G::IsCachedColumn), true, instance, src);
    emit setCached(sfRow, true, instance);

    // reset attempts rgh review re nextToCache check (not working)
    // emit setValueSf(dm->sf->index(sfRow, G::AttemptsColumn), 0, instance, src);

    // refresh thumbs isCached indicator and imageView main image if is current
    // emit refreshViews(decoders[id]->fPath, true, "ImageCache::cacheImage");

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

    // rgh make sure toCache still contains cacheKey (might have been removed)
    if (toCache.contains(cacheRow)) {

        // save decoder status
        emit setValueSf(dm->sf->index(cacheRow, G::DecoderReturnStatusColumn),
                        static_cast<int>(decoders[id]->status), instance, src);

        // save errors
        if (decoders[id]->errMsg != "") {
            emit setValueSf(dm->sf->index(cacheRow, G::DecoderErrMsgColumn), decoders[id]->errMsg, instance, src);
        }

        // check if aborted
        if (abort || decoders[id]->status == ImageDecoder::Status::Abort) {
            if (toCacheStatus.contains(cacheRow) && toCacheStatus[cacheRow].isCaching)
                toCacheStatus[cacheRow].isCaching = false;
            emit setValueSf(dm->sf->index(cacheRow, G::IsCachingColumn), false, instance, src);
            decoders[id]->setIdle();

            if (debugLog || G::isLogger || G::isFlowLogger)
            {
                log("fillCache ABORTED",
                    "decoder = " + QString::number(id).leftJustified(3)
                    );
            }
            if (debugCaching)
            {
                QString fun = "ImageCache::fillCache ABORTED";
                qDebug().noquote()
                    << fun.leftJustified(col0Width, ' ')
                    << "decoder" << QString::number(id).leftJustified(3)
                    << "row =" << QString::number(cacheRow).leftJustified((4))
                    << "abort =" << abort
                    << "decoder status =" << decoders[id]->statusText.at(decoders[id]->status)
                    ;
            }
            return;
        }

        // returning instance clash
        if (decoders[id]->status == ImageDecoder::Status::InstanceClash) {
            if (debugLog || G::isLogger || G::isFlowLogger)
            {
                log("fillCache INSTANCE CLASH",
                    "decoder = " + QString::number(id).leftJustified(3)
                    );
            }
            if (debugCaching)
            {
                QString fun = "ImageCache::fillCache INSTANCE CLASH";
                qDebug().noquote() << fun.leftJustified(col0Width, ' ')
                                   << "decoder" << QString::number(id).leftJustified(3)
                                   << "row =" << QString::number(cacheRow).leftJustified((4))
                    ;
            }
        }

        // cache image returned by decoder
        if (decoders[id]->status == ImageDecoder::Status::Success) {
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
        }
        // rgh is it possible to be a video (skipped in setTargetRaange)
        else if (decoders[id]->status != ImageDecoder::Status::Video) {
            emit setValueSf(dm->sf->index(cacheRow, G::IsCachedColumn), true, instance, src);
        }
        // failure
        else {
            // if (debugCaching)
            {
                QString fun = "ImageCache::fillCache FAILED";
                qDebug().noquote()
                    << fun.leftJustified(col0Width, ' ')
                    << "decoder" << QString::number(id).leftJustified(3)
                    << "row =" << QString::number(cacheRow).leftJustified((4))
                    << "status =" << decoders[id]->statusText.at(decoders[id]->status)
                    << "errMsg =" << decoders[id]->errMsg
                    << "decoder[id]->fPath =" << decoders[id]->fPath
                    ;
            }
            // if (toCacheStatus.contains(cacheKey))
            //     toCacheStatus[cacheKey].isCaching = false;
        }

    }

    // set isCaching to false
    emit setValueSf(dm->sf->index(cacheRow, G::IsCachingColumn), false, instance, src);
    if (toCacheStatus.contains(cacheRow))
        toCacheStatus[cacheRow].isCaching = false;

    decoders[id]->setIdle();

    // get next image toˇ· cache
    // rgh todo add retry here?  Used Status::Failed
    int toCacheKey;
    if (!abort) toCacheKey = nextToCache(id);
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

            // update cache progress in status bar
            if (isShowCacheStatus) {
                updateStatus("Update all rows", "ImageCache::fillCache after check cacheUpToDate");
            }

            // turn off caching activity lights on statusbar
            emit updateIsRunning(false, true);  // (isRunning, showCacheLabel)

            if (debugLog || G::isLogger || G::isFlowLogger)
                G::log("allDecodersDone");

            abort = false;

            // if (debugCaching)
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
        decodeNextImage(id, sfRow);
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
    if (debugCaching || G::isLogger || G::isFlowLogger) log("dispatch");

    // req'd?
    if (dm->sf->rowCount() == 0) {
        return;
    }
    if (debugCaching)
    {
        qDebug().noquote() << "ImageCache::dispatch  abort =" << abort;
    }

    abort = false;

    if (!imageCacheThread.isRunning()) imageCacheThread.start();

    // check available memory (another app may have used or released some memory)
    memChk();

    // reset target range
    // if (!abort)
        updateToCache();

    // if cache is up-to-date our work is done
    if (cacheUpToDate()) return;

    // signal MW cache status
    emit updateIsRunning(true, true);   // (isRunning, showCacheLabel)

    // if (!abort)
        launchDecoders("ImageCache::dispatch");
}
