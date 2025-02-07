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

    3. Add each row in the target range to the list toCache if not been cached, and is
    not in toCache.  Remove any images outside the target range from the image cache.

    4. Add images that are targeted and not already cached. If the cache is full before
    all targets have been cached then remove any cached images outside the target range
    that may have been left over from a previous thumbnail selection. The targeted images
    are read by multiple decoders, which signal fillCache with a QImage.

Building the cache:

    A number of ImageDecoders are created when ImageCache is created. Each ImageDecoder
    runs in a separate thread. The decoders convert an image file into a QImage.

    The ImageDecoders are launched from ImageCache::launchDecoders, dispatched from
    fillCache, return a QImage back to fillCache, the QImage is inserted into imCache and
    then are dispatched again.

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
      - fillCache
        - if starting
          - nextToCache
        - if returning with QImage
          - cacheImage
          - nextToCache

    nextToCache detail:
    - nextToCache
      - yes
        - decodeNextImage
        - fillCache
        - cacheImage
      - no
        - cacheUpToDate (checks for orphans and toCache list completed)
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
    if (debugLog || G::isLogger) log("ImageCache");

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
    debugCaching = false;        // turn on local qDebug
    debugLog = false;            // invoke log without G::isLogger or G::isFlowLogger

    moveToThread(this);
}

ImageCache::~ImageCache()
{
    stop("ImageCache::~ImageCache");
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
    if (debugLog || G::isLogger)
        log("stop", "isRunning = " + QVariant(isRunning()).toString());

    if (isRunning()) {
        gMutex.lock();
        abort = true;
        condition.wakeOne();
        gMutex.unlock();

        // Stop all decoder threads first
        for (int id = 0; id < decoderCount; ++id) {
            decoder[id]->stop();
        }

        // Quit the ImageCache thread's event loop
        QMetaObject::invokeMethod(this, "quit", Qt::QueuedConnection);

        if (isRunning()) wait(); // Ensure the thread has fully stopped
    }

    abort = false;

    // turn off caching activity lights on statusbar
    emit updateIsRunning(false, false);  // flags = isRunning, showCacheLabel
}

bool ImageCache::allDecodersReady()
{
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

void ImageCache::updateToCache()
{
/*
    Called by setCurrentPosition() if isRunning().
    Called by run() if was not isRunning().
    In either case, this is running in the ImageCache thread.
*/
    if (debugLog || G::isLogger || G::isFlowLogger) log("updateToCacheTargets");

    QString fun = "ImageCache::updateToCacheTargets";
    if (debugCaching)
    {
        qDebug().noquote() << fun.leftJustified(col0Width, ' ')
                           << "current position =" << key
                           << "total =" << dm->sf->rowCount()
            ;
    }

    QMutexLocker locker(&gMutex);  // req'd to prevent toCacheAppend() crash
    if (!abort) setDirection();
    if (!abort) setTargetRange(key);
    if (!abort) trimOutsideTargetRange();

    // G::log("updateToCacheTargets", reportToCache());
    if (debugCaching)
    {
        qDebug().noquote() << fun.leftJustified(col0Width, ' ')
        << "current position =" << key
        << "toCache:" << toCache
            ;
    }
}

void ImageCache::trimOutsideTargetRange()
/*
    Any images in imCache that are no longer in the target range are removed.
*/
{
    // QMutexLocker locker(&gMutex);    // may need as crashed on line 373

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
                emit setValueSf(dm->sf->index(sfRow, G::IsCachedColumn), false, instance, src);
                emit refreshViews(fPath, false, "ImageCache::trimOutsideTargetRange");
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
            emit setValueSf(dm->sf->index(sfRow, G::IsCachedColumn), false, instance, src);
        }
    }

    // trim toCache outside target range
    auto it = toCache.begin();  // Ensures a mutable iterator
    toCache.erase(std::remove_if(it, toCache.end(), [&](int sfRow) {
                      return sfRow < targetFirst || sfRow > targetLast;
                  }), toCache.end());

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
    if (debugLog || G::isLogger) log("setDirection");

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

    QString fun = "ImageCache::setTargetRange";
    fun = fun.leftJustified(col0Width, ' ');
    // if (G::isLogger) G::log(fun, "maxMB = " + QString::number(maxMB));
    if (debugCaching)
    qDebug() << "setTargetRange"
             << "key =" << key
             << "threadId =" << currentThreadId();

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

    if (debugCaching)
    {
        qDebug().noquote() << fun << "current position =" << key << "n =" << n;
    }

    // Iterate while there is space in the cache
    while (sumMB < maxMB && (!aheadDone || !behindDone)) {
        if (abort) return;
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
            // if (behindPos >= n || behindPos < 0) break;
            if (isForward ? (behindPos >= 0) : (behindPos < n)) {
                // if (debugCaching) {qDebug() << "behindPos =" << behindPos;}
                if (!dm->sf->index(aheadPos, G::VideoColumn).data().toBool()) {
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

    if (debugCaching)
    {
    qDebug() << fun << "targetFirst =" << targetFirst << "targetLast =" << targetLast
             << toCache;
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

void ImageCache::addToCache(QStringList &pathList)
{
/*
    Called when insert image(s).  Runs on GUI thread.  rgh not req'd.
*/
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
    toCacheStatus.insert(sfRow, {Status::NotCached, -1, instance});
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

    foreach (CacheStatus cs, toCacheStatus) {
        if (cs.status != Status::Caching) return false;
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
    rpt << "isRunning                = " << (isRunning() ? "true" : "false") << "\n";
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
    if (G::isLogger) log("reportCacheItemList");
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
        if (toCacheStatus[sfRow].status == Status::Caching) rpt << "c";
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

void ImageCache::initImageCache(int &cacheMaxMB,
                                int &cacheMinMB,
                                bool &isShowCacheStatus,
                                int &cacheWtAhead)
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

    // reset the image cache
    icd->imCache.clear();
    toCache.clear();
    toCacheStatus.clear();

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
    if (G::isLogger) log("updateImageCacheParam");
    if (debugCaching) qDebug() << "ImageCache::updateImageCacheParam";
    // rgh cache amount fix from pref to here
    maxMB = cacheSizeMB;
    minMB = cacheMinMB;
    isShowCacheStatus = isShowCacheStatus;
    wtAhead = cacheWtAhead;
}

void ImageCache::filterChange(QString &currentImageFullPath, QString src)
{
/*
    When the datamodel is filtered the image cache needs to be updated. The cacheItemList is
    rebuilt for the filtered dataset and isCached updated, the current image is set, and any
    surplus cached images (not in the filtered dataset) are removed from imCache.
    The image cache is now ready to run by calling setCachePosition().
*/
    QString fun = "ImageCache::rebuildImageCacheParameters";
    if (debugLog || G::isLogger || G::isFlowLogger)
        G::log("rebuildImageCacheParameters", "src = " + src);
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

    // // reset the image cache
    // icd->imCache.clear();
    // toCache.clear();
    // toCacheStatus.clear();

    // update instance
    instance = dm->instance;

    if (isShowCacheStatus) updateStatus("Update all rows", fun);

    refreshImageCache();

    // setCurrentPosition(currentImageFullPath, fun);
}

void ImageCache::refreshImageCache()
{
    if (debugLog || G::isLogger || G::isFlowLogger) log("refreshImageCache");
    if (isRunning()) {
        QMetaObject::invokeMethod(this, "updateToCache", Qt::QueuedConnection);
    }
    start(QThread::LowestPriority);
}

void ImageCache::reloadImageCache()
{
/*
    Reload all images in the cache.  Used by colorManageChange.
*/
    if (debugLog || G::isLogger || G::isFlowLogger) log("reloadImageCache");
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
    if (debugLog || G::isLogger || G::isFlowLogger) log("colorManageChange");
    reloadImageCache();
}

void ImageCache::cacheSizeChange()
{
/*
    Called when changes are made to image cache parameters in preferences. The image cache
    direction, priorities and target are reset and orphans fixed.
*/
    if (debugLog || G::isLogger || G::isFlowLogger) log("cacheSizeChange");
    if (debugCaching)
        qDebug() << "ImageCache::cacheSizeChange";

    if (isRunning()) {
        stop("ImageCache::cacheSizeChange");
    }
    start(QThread::LowestPriority);
}

void ImageCache::datamodelFolderCountChange(QString src)
{
    if (debugLog || G::isLogger || G::isFlowLogger)
        log("datamodelFolderCountChange", "src = " + src);
    if (debugCaching)
        qDebug() << "ImageCache::datamodelFolderCountChange";

    if (isRunning()) {
        // Use signal-slot mechanism to ensure execution in ImageCache thread
        QMetaObject::invokeMethod(this, "updateToCache", Qt::QueuedConnection);
    }
    else {
        start(QThread::LowestPriority);
    }
}

void ImageCache::setCurrentPosition(QString fPath, QString src)
{
/*
    Called from MW::fileSelectionChange to reset the position in the image cache. The image
    cache direction, priorities and target are reset and the cache is updated in fillCache.
*/
    int sfRow = dm->proxyRowFromPath(fPath);

    static int prevRow = -1;
    static int prevInstance = -1;

    if (prevRow == sfRow && prevInstance == instance) {
        // debug msg
        return;
    }
    prevRow = sfRow;
    prevInstance = instance;

    QString fun = "ImageCache::setCurrentPosition";
    // G::log(fun, "row = " + QString::number(sfRow) + " src = " + src);
    if (debugLog || G::isLogger || G::isFlowLogger)
       log("setCurrentPosition", "row = " + QString::number(sfRow));
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
        qWarning() << "ImageCache::setCurrentPosition cancelled instance change row =" << sfRow;
        return;
    }

    if (isInitializing) {
        // qDebug() << "ImageCache::setCurrentPosition Initializing";
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

    if (isRunning()) {
        // Use signal-slot mechanism to ensure execution in ImageCache thread
        QMetaObject::invokeMethod(this, "updateToCache", Qt::QueuedConnection);
    }
    else {
        start(QThread::LowestPriority);
    }
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
        G::log(fun, sId + msg);
        // qDebug().noquote()
        //     << fun.leftJustified(50, ' ')
        //     << sId << msg;
    };

    if (debugLog || G::isLogger)
        log("nextToCache", "CacheUpToDate = " + QVariant(cacheUpToDate()).toString());

    QString fun = "ImageCache::nextToCache";
    bool debugThis = false;
    QString msg;
    QString sId = "decoder " + QString::number(id).leftJustified(3);

    if (debugCaching)
    {
        qDebug().noquote()
        << fun.leftJustified(col0Width, ' ')
        << "toCache:" << toCache
            ;
    }

    // ImageCache instance out-of-date
    if (instance != dm->instance) {
        if (debugThis) {
            msg = "Instance clash";
            sDebug(sId, msg);
        }
        return -1;
    }

    QMutexLocker locker(&gMutex);   // req'd 2025-02-03

    if (toCache.isEmpty()) {
        if (debugThis) sDebug(sId, "toCache is empty");
        return -1;
    }

    // iterate toCache
    for (int i = 0; i < toCache.count(); ++i) {
        int sfRow = toCache.at(i);

        QString sRow = QString::number(sfRow).leftJustified(6);
        QString fPath = dm->sf->index(sfRow, 0).data(G::PathRole).toString();

        // if (debugThis) {
        //     msg = "row = " + sRow + " checking" ;
        //     sDebug(sId, msg);
        // }

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

        // // wrong instance
        // if (toCacheStatus[sfRow].instance != instance) {
        //     if (debugThis){
        //         msg = "row " + sRow + " wrong instance";
        //         sDebug(sId, msg);
        //     }
        //     continue;
        // }

        // isCaching and not the same decoder
        if (toCacheStatus.contains(sfRow)) {
            if (toCacheStatus[sfRow].status == Status::Caching) {
                if (id != toCacheStatus[sfRow].decoderId) {
                    if (debugThis){
                        msg = "row " + sRow + " isCaching and not the same decoder";
                        sDebug(sId, msg);
                    }
                    continue;
                }
            }
        }

        // next item to cache
        if (debugThis) {
            msg = "row " + sRow + " success";
            sDebug(sId, msg);
        }

        /*
        QString msg = "decoder " + QString::number(id).leftJustified(2);
        msg += " row = " + QString::number(sfRow).leftJustified(4);
        G::log("nextToCache", msg);
        //*/

        return sfRow;
    }

    // nothing found to cache
    if (debugCaching)
    {
        qDebug().noquote()
        << fun.leftJustified(col0Width, ' ')
        << "Failed"
            ;
        G::log("nextToCache", "nothing found to cache\n");
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

    // QMutexLocker locker(&gMutex);

    if (!isValidKey(sfRow)) {
        decoder[id]->status = ImageDecoder::Status::Ready;
        return;
    }

    // remove from toCache
    // if (toCache.contains(sfRow)) toCacheRemove(sfRow);

    // set isCaching
    if (toCacheStatus.contains(sfRow)) {
        toCacheStatus[sfRow].status = Status::Caching;
        toCacheStatus[sfRow].decoderId = id;
        toCacheStatus[sfRow].instance = instance;
    }
    emit setValueSf(dm->sf->index(sfRow, G::IsCachingColumn), true, instance, src);
    emit setValueSf(dm->sf->index(sfRow, G::DecoderIdColumn), id, instance, src);
    int attempts = dm->sf->index(sfRow, G::AttemptsColumn).data().toInt();
    emit setValueSf(dm->sf->index(sfRow, G::AttemptsColumn), ++attempts, instance, src);

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

void ImageCache::cacheImage(int id, int sfRow)
{
/*
    Called from fillCache to insert a QImage that has been decoded into icd->imCache.
    Do not cache video files, but do show them as cached for the cache status display.
*/
    {
    QString src = "ImageCache::cacheImage";
    QString comment = "decoder " + QString::number(id).leftJustified(3) +
                      "row = " + QString::number(sfRow).leftJustified(5) +
                      reportImCacheRows();
                      // "fPath = " + decoder[id]->fPath;
    if (debugLog || G::isLogger || G::isFlowLogger) log("cacheImage", comment);
    if (debugCaching)
    {
        QString fun = "ImageCache::cacheImage";
        qDebug().noquote()
               << fun.leftJustified(col0Width, ' ')
               << "decoder" << QString::number(id).leftJustified(2)
               << "row =" << QString::number(sfRow).leftJustified((4))
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
        G::issue("Warning", msg, "ImageCache::cacheImage", sfRow, decoder[id]->fPath);
        return;
    }

    gMutex.lock();  // req'd to prevent toCacheAppend() crash

    // cache the image
    icd->imCache.insert(decoder[id]->fPath, decoder[id]->image);

    // // remove from toCache
    if (toCache.contains(sfRow)) toCacheRemove(sfRow);

    gMutex.unlock();

    // change toCacheStatus to Cached
    // if (toCacheStatus.contains(sfRow)) toCacheStatus[sfRow].status = Status::Cached;
    // if (toCache.contains(cacheKey)) toCache.remove(toCache.indexOf(cacheKey));

    // reset caching and cache flags
    emit setValueSf(dm->sf->index(sfRow, G::IsCachingColumn), false, instance, src);
    emit setValueSf(dm->sf->index(sfRow, G::IsCachedColumn), true, instance, src);

    // reset attempts rgh review re nextToCache check (not working)
    emit setValueSf(dm->sf->index(sfRow, G::AttemptsColumn), 0, instance, src);

    // refresh thumbs isCached indicator and imageView main image if is current
    emit refreshViews(decoder[id]->fPath, true, "ImageCache::cacheImage");

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

    int cacheKey = -1;       // sfRow for image in cacheKeyHash (default to no key)
    QString src = "";

    if (debugCaching)
    {
        QString fun = "ImageCache::fillCache starting";
        qDebug().noquote() << fun.leftJustified(col0Width, ' ')
                           << "decoder" << QString::number(id).leftJustified(2)
            ;
    }
    if (debugLog || G::isLogger)
    {
        log("fillCache",
            "decoder " + QString::number(id).leftJustified(3) +
            "row = " + QString::number(decoder[id]->sfRow).leftJustified(4)
        );
    }

    // check if aborted
    if (abort || decoder[id]->status == ImageDecoder::Status::Abort) {
        if (toCacheStatus.contains(cacheKey) && toCacheStatus[cacheKey].status == Status::Caching)
            toCacheStatus[cacheKey].status = NotCached;
        emit setValueSf(dm->sf->index(cacheKey, G::IsCachingColumn), false, instance, src);
        decoder[id]->status = ImageDecoder::Status::Ready;
        if (debugLog || G::isLogger || G::isFlowLogger)
        {
            log("fillCache ABORTED",
                "decoder = " + QString::number(id).leftJustified(3)
                );
        }
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
        if (debugLog || G::isLogger || G::isFlowLogger)
        {
            log("fillCache INSTANCE CLASH",
                "decoder = " + QString::number(id).leftJustified(3)
                );
        }
        cacheKey = -1;
    }

    // returning decoder failed
    else if (decoder[id]->status != ImageDecoder::Status::Success) {
        if (toCacheStatus.contains(cacheKey) && toCacheStatus[cacheKey].status == Status::Caching)
            toCacheStatus[cacheKey].status = NotCached;
        emit setValueSf(dm->sf->index(cacheKey, G::IsCachingColumn), false, instance, src);
        emit setValueSf(dm->sf->index(cacheKey, G::IsCachedColumn), false, instance, src);
        cacheKey = -1;
    }

    // returning decoder successful
    else {
        cacheKey = decoder[id]->sfRow;
        // update datamodel (req'd for status updates and reporting)
        emit setValueSf(dm->sf->index(cacheKey, G::IsCachingColumn), false, instance, src);
        emit setValueSf(dm->sf->index(cacheKey, G::DecoderReturnStatusColumn),
                       static_cast<int>(decoder[id]->status), instance, src);
        // video
        if (decoder[id]->status == ImageDecoder::Status::Video) {
            emit setValueSf(dm->sf->index(cacheKey, G::IsCachedColumn), true, instance, src);
        }
        // errors
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

    // returning decoder: add decoded QImage to imCache.
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
            toCacheStatus[cacheKey].status = Status::Failed;
        }
    }

    // get next image to cache
    // rgh todo add retry here?  Used Status::Failed
    int toCacheKey = nextToCache(id);
    bool isNextToCache = toCacheKey != -1;
    // bool isCacheUpToDate = cacheUpToDate();
    bool isCacheKeyOk = isValidKey(toCacheKey);
    // bool instanceOk = decoder[id]->instance == dm->instance;
    bool okDecodeNextImage = !abort && isNextToCache && isCacheKeyOk;

    if (debugLog || G::isLogger || G::isFlowLogger)
    {
        log("fillCache dispatch",
            "decoder " + QString::number(id).leftJustified(3) +
            "row = " + QString::number(toCacheKey).leftJustified(5) +
            "instance = " + QString::number(instance).leftJustified(5) +
            "decoder status = " + decoder[id]->statusText.at(decoder[id]->status).leftJustified(8) +
            // "isCacheUpToDate = " + QVariant(isCacheUpToDate).toString().leftJustified(6) +
            "isNextToCache = " + QVariant(isNextToCache).toString().leftJustified(6) +
            "isValidKey(toCacheKey) = " + QVariant(isValidKey(toCacheKey)).toString().leftJustified(6) +
            "okDecodeNextImage = " + QVariant(okDecodeNextImage).toString().leftJustified(6) +
            "toCache: " + reportToCacheRows()
        );
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
        // reset decoder status to ready
        decoder[id]->status = ImageDecoder::Status::Ready;

        // is this the last active decoder?
        if (allDecodersReady()) {

            // update cache progress in status bar
            if (isShowCacheStatus) {
                updateStatus("Update all rows", "ImageCache::fillCache after check cacheUpToDate");
            }

            // turn off caching activity lights on statusbar
            emit updateIsRunning(false, true);  // (isRunning, showCacheLabel)

            if (debugLog || G::isLogger || G::isFlowLogger)
                G::log("allDecodersDone");

            // stop ImageCache thread
            abort = true;

            if (debugCaching)
            {
                QString fun = "ImageCache::fillCache Finished";
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

        // less rows to decode than decoders
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

        // already done
        if (cacheUpToDate()) {
            emit updateIsRunning(false, true);  // (isRunning, showCacheLabel)
            if (debugLog || G::isLogger || G::isFlowLogger) {
                QString msg = "ird = " + QString::number(id).leftJustified(2) +
                              "cacheUpToDate = " + QVariant(cacheUpToDate()).toString() +
                              " status = " + decoder[id]->statusText.at(decoder[id]->status)
                    ;
                log("launchDecoders", msg);
            }
            break;
        }

        // get to work decoding
        if (decoder[id]->status == ImageDecoder::Status::Ready) {
            decoder[id]->fPath = "";
            if (debugLog)
            {
                QString msg = "id = " + QString::number(id).leftJustified(2) +
                              // "cacheUpToDate = " + QVariant(isCacheUpToDate).toString() +
                              " status = Ready, calling fillCache"
                              ;
            }
            if (debugLog || G::isLogger || G::isFlowLogger) {
                QString msg = "decoder " + QString::number(id).leftJustified(3) +
                              "status = " + decoder[id]->statusText.at(decoder[id]->status)
                    ;
                log("launchDecoders launch:", msg);
            }
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
    if (debugCaching || G::isLogger || G::isFlowLogger) log("run");

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
    if (!abort) updateToCache();

    // if cache is up-to-date our work is done
    //if (cacheUpToDate()) return;

    // signal MW cache status
    emit updateIsRunning(true, true);   // (isRunning, showCacheLabel)

    if (!abort) launchDecoders("ImageCache::run");

    while (!abort) {}
    abort = false;
}
