#include "Cache/imagecache2.h"


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

    2. Define the target weighting caching strategy ahead and behind. In the illustration we
    are caching 2 ahead for every 1 behind.

    3. Based on the priority and the cache maximum size, assign images to target for caching.

    4. Add images that are targeted and not already cached. If the cache is full before all
    targets have been cached then remove any cached images outside the target range that may
    have been left over from a previous thumbnail selection. The targeted images are read by
    multiple decoders, which signal fillCache with a QImage.

Data structures:

    The main data structures are in a separate class ImageCacheData to facilitate concurrent
    data access. The image cache QImages reside in the concurrent hash table imCache. This
    table is written to by fillCache and read by ImageView. A mutex protects fillCache so that
    only one decoder at a time can add images to imCache.

    The caching process is managed by the cacheItemList of cacheItem.  Each CacheItem
    corresponds to a row in the filtered DataModel dm->sf, or each image in the view.  The
    cache status, target, priority and metadata required for decoding are in CacheItem.
*/


ImageCache2::ImageCache2(QObject *parent,
                       ImageCacheData *icd,
                       DataModel *dm/*,
                       Metadata *metadata*/)
    : QThread(parent)
{
    if (G::isLogger) G::log(__FUNCTION__);
    // Data is kept in ImageCacheData icd, a concurrent hash table
    this->icd = icd;
    this->dm = dm;
    metadata = new Metadata;
//    this->metadata = metadata;
    // create n decoder threads
    decoderCount = QThread::idealThreadCount();
    icd->cache.decoderCount = decoderCount;
    for (int id = 0; id < decoderCount; ++id) {
        ImageDecoder *d = new ImageDecoder(this, id, dm, metadata);
        decoder.append(d);
        connect(decoder[id], &ImageDecoder::done, this, &ImageCache2::fillCache);
    }
    restart = false;
    abort = false;
}

ImageCache2::~ImageCache2()
{
    mutex.lock();
    if (G::isLogger) G::log(__FUNCTION__);
    abort = true;
    condition.wakeOne();
    mutex.unlock();
    wait();
}

void ImageCache2::clearImageCache(bool includeList)
{
    if (G::isLogger) G::log(__FUNCTION__);
    QMutexLocker locker(&mutex);
    icd->imCache.clear();
    icd->cache.currMB = 0;
    // do not clear cacheItemList if called from start slideshow
    if (includeList) icd->cacheItemList.clear();
    updateStatus("Clear", __FUNCTION__);
}

void ImageCache2::stopImageCache()
{
/*
    Note that initImageCache and updateImageCache both check if isRunning and stop a running
    thread before starting again. Use this function to stop the image caching thread without a
    new one starting when there has been a folder change. The cache status label in the status
    bar will be hidden.
*/
    if (G::isLogger) G::log(__FUNCTION__);

    // stop decoders
    for (int id = 0; id < decoderCount; ++id) {
        decoder[id]->stop();
    }

    // stop imagecache thread
    if (isRunning()) {
        mutex.lock();
        abort = true;
        condition.wakeOne();
        mutex.unlock();
        wait();
        abort = false;
    }

    // turn off caching activity lights on statusbar
    emit updateIsRunning(false, false);  // flags = isRunning, showCacheLabel

    // reset the image cache
    clearImageCache(true);
}

int ImageCache2::getImCacheSize()
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

bool ImageCache2::prioritySort(const ImageCacheData::CacheItem &p1,
                              const ImageCacheData::CacheItem &p2)
{
    return p1.priority < p2.priority;       // sort by priority
}

bool ImageCache2::keySort(const ImageCacheData::CacheItem &k1,
                         const ImageCacheData::CacheItem &k2)
{
    return k1.key < k2.key;       // sort by key to return to thumbnail order
}

void ImageCache2::setKeyToCurrent()
{
/*
    cache.key is the index of the item in cacheItemList that matches dm->currentFilePath
*/
    if (debugCaching) {
        qDebug().noquote() << __FUNCTION__ << "decoder-1";
    }
    if (G::isLogger) G::log(__FUNCTION__);
    icd->cache.key = 0;
    for (int i = 0; i < icd->cacheItemList.count(); i++) {
        if (icd->cacheItemList.at(i).fPath == currentPath) {
            icd->cache.key = i;
            return;
        }
    }
}

void ImageCache2::setDirection()
{
/*
    If the direction of travel changes then delay reversing the caching direction until a
    directionChangeThreshold (ie 3rd) image is selected in the new direction of travel. This
    prevents needless caching if the user justs reverses direction to check out the previous
    image
*/
    if (G::isLogger) G::log(__FUNCTION__);
    if (debugCaching) {
        qDebug().noquote() << __FUNCTION__ << "   decoder-1";
    }

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
    bool maybeIsForward = thisStep > 0;
    icd->cache.maybeDirectionChange = icd->cache.isForward != maybeIsForward;

    /*
    qDebug() << __FUNCTION__
             << "maybeDirectionChange =" << icd->cache.maybeDirectionChange
             << "isForward =" << icd->cache.isForward
             << "maybeIsForward =" << maybeIsForward
             << "thisStep =" << thisStep
             << "sumStep =" << icd->cache.sumStep
             << "directionChangeThreshold =" << icd->cache.directionChangeThreshold
             << "key =" << icd->cache.key
             << "prevKey =" << prevKey
                ;
                // */

    // if direction has not maybe changed
    if (!icd->cache.maybeDirectionChange) {
        icd->cache.sumStep = 0;
        return;
    }

    // direction maybe changed, increment counter
    icd->cache.sumStep += thisStep;

    // maybe direction change gets to threshold then change cache direction
    if (qFabs(icd->cache.sumStep) >= icd->cache.directionChangeThreshold) {
        icd->cache.isForward = icd->cache.sumStep > 0;
        icd->cache.sumStep = 0;
        icd->cache.maybeDirectionChange = false;
    }

    /*
    qDebug() << __FUNCTION__ << thisStep << cache.sumStep << cache.isForward;
    //*/
}

void ImageCache2::setTargetRange()
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
    if (debugCaching) {
        qDebug().noquote() << __FUNCTION__ << " decoder-1";
    }

    // sort by priority to make it easy to find highest priority not already cached
    std::sort(icd->cacheItemList.begin(), icd->cacheItemList.end(), &ImageCache2::prioritySort);

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
    std::sort(icd->cacheItemList.begin(), icd->cacheItemList.end(), &ImageCache2::keySort);

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

//    fixOrphans();     // this slows thngs down

    /*
    if (debugCaching) {
        qDebug();
        qDebug() << __FUNCTION__
                 << "targetFirst =" << icd->cache.targetFirst
                 << "targetLast =" << icd->cache.targetLast
                 << "isForward =" << icd->cache.isForward
                    ;
    }
    //*/
}

bool ImageCache2::nextToCache(int id)
{
/*
    The next image to cache is determined by traversing the cacheItemList in ascending order
    to find the highest priority item to cache in the target range based on these criteria:

    - isCaching and isCached are false.
    - decoderId matches item, isCaching is true and isCached = false.  If this is the case then
      we know the previous attempt failed, and we should try again, unless the attempts are
      greater than maxAttemptsToCacheImage.
*/
    if (G::isLogger) G::log(__FUNCTION__);

    int lastPriority = icd->cacheItemList.length();
    int key = -1;
    // find next priority item
    for (int i = icd->cache.targetFirst; i <= icd->cache.targetLast; ++i) {
        if (i >= icd->cacheItemList.length()) break;
        int priority = icd->cacheItemList.at(i).priority;
        if (priority >= lastPriority) break;
        bool isCaching = icd->cacheItemList.at(i).isCaching;
        bool isCached = icd->cacheItemList.at(i).isCached;
        int attempts = icd->cacheItemList.at(i).attempts;
        int threadId = icd->cacheItemList.at(i).threadId;
        // chk orphaned items rgh what about imCache - maybe still in imCache
        if (!isCached && attempts < maxAttemptsToCacheImage) {
            if (!isCaching || (isCaching && id == threadId)) {
                // higher priorities are lower numbers (highest = 0)
                if (priority < lastPriority) {
                    lastPriority = priority;
                    key = i;
                }
            }
        }
    }
    if (key > -1) {
        icd->cache.toCacheKey = key;
        if (debugCaching) {
            QString k = QString::number(key).leftJustified((4));
            qDebug().noquote() << __FUNCTION__
                               << "    decoder" << id
                               << "key =" << k
                               << icd->cacheItemList.at(key).fPath
                                  ;
        }
        return true;
    }
    return false;
}

bool ImageCache2::nextToDecache(int id)
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
        int priority = icd->cacheItemList.at(i).priority;
        bool isCached = icd->cacheItemList.at(i).isCached;
        if (isCached) {
            // higher priorities are lower numbers
            if (priority > lastPriority) {
                lastPriority = priority;
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

void ImageCache2::setPriorities(int key)
{
/*
    Starting at the current key, this algorithm iterates through the icd->cacheItemList,
    following the order (2 ahead, one behind) and assigned an increasing sort order key, which
    is used by setTargetRange to sort icd->cacheItemList by priority.
*/
    if (G::isLogger) G::log(__FUNCTION__, "key = " + QString::number(key));
    if (debugCaching) {
        qDebug().noquote() << __FUNCTION__ << "  decoder-1" << "key =" << key;
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

void ImageCache2::fixOrphans()
{
/*
    If the caching process fails, then an image in the target range may be orphaned while
    caching is still active. isCaching is reset to false. If an image outside the target range
    is shown as isCaching it is reset, and if the image is cached then it is removed from the
    imCache and the cached flag is reset to false.
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
            emit updateCacheOnThumbs(fPath, false);
//            if (isCached) emit updateCacheOnThumbs(fPath, false, icd->cache.targetFirst,icd->cache.targetLast);
        }
    }
}

bool ImageCache2::cacheUpToDate()
{
/*
    Determine if all images in the target range are cached or being cached.
*/
    for (int i = icd->cache.targetFirst; i < icd->cache.targetLast + 1; ++i) {
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
    return true;
}

void ImageCache2::makeRoom(int id, int cacheKey)
{
/*
    Called from the running thread when a new image from the nextToCache QList is iterated.
    Remove images from the QHash imCache, based on the order of the QList toDecache, until
    there is enough room to add the new image. Update cacheItemList, thumb status indicators,
    and the current available room in the cache.
*/
    if (G::isLogger) G::log(__FUNCTION__);
    if (cacheKey >= icd->cacheItemList.length()) return;
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
                /*
                qDebug().noquote() << __FUNCTION__
                         << "       decoder" << id << "key =" << k
                         << "room =" << room
                         << "roomRqd =" << roomRqd
                         << "Removed image" << s
                            ;
                            //*/
            }
        }
        else {
            break;
        }
    }
}

void ImageCache2::memChk()
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

void ImageCache2::removeFromCache(QStringList &pathList)
{
/*
    Called when delete an image.
*/
    if (G::isLogger) G::log(__FUNCTION__);
    for (int i = pathList.count() - 1; i > -1; --i) {
        QString fPath = pathList.at(i);
        icd->imCache.remove(fPath);
        for (int j = 0; j < icd->cacheItemList.length(); ++j) {
            if (icd->cacheItemList.at(j).fPath == fPath) {
                icd->cacheItemList.removeAt(j);
            }
        }
        icd->cache.totFiles = icd->cacheItemList.length();
    }

    // trigger change to ImageCache2
    setCurrentPosition(dm->currentFilePath);
}

void ImageCache2::updateStatus(QString instruction, QString source)
{
    emit showCacheStatus(instruction, icd->cache, source);
}

QString ImageCache2::diagnostics()
{
    if (G::isLogger) G::log(__FUNCTION__);
    QString reportString;
    QTextStream rpt;
    rpt.setString(&reportString);
    rpt << Utilities::centeredRptHdr('=', objectName() + " ImageCache2 Diagnostics");
    rpt << "\n" ;
    rpt << reportCache("");
    rpt << reportImCache();

    rpt << "\n\n" ;
    return reportString;
}

QString ImageCache2::reportCache(QString title)
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
                << "Contains"
                << "dmCached"
                << "SizeMB"
                << "Loaded"
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
            << icd->cacheItemList.at(i).isTarget
            << icd->cacheItemList.at(i).attempts
            << icd->cacheItemList.at(i).isCaching
            << icd->cacheItemList.at(i).threadId
            << icd->cacheItemList.at(i).isCached
            << icd->imCache.contains(icd->cacheItemList.at(i).fPath)
            << dm->sf->index(icd->cacheItemList.at(i).key,0).data(G::UserRoles::CachedRole).toBool()
            << icd->cacheItemList.at(i).sizeMB
            << icd->cacheItemList.at(i).metadataLoaded
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
    return reportString;
}

QString ImageCache2::reportImCache()
{
    QString reportString;
    QTextStream rpt;
    rpt.flush();
    reportString = "";
    rpt.setString(&reportString);
    rpt << "\nimCache hash:";
    QHash<QString, QImage>::iterator i;
    QVector<QString> keys;
    // check when imCache is empty
    QImage image;
    icd->imCache.getKeys(keys);
    for (int i = 0; i < keys.length(); ++i) {
        icd->imCache.find(keys.at(i), image);
        int w = image.width();
        int h = image.height();
        rpt << "\n";
        rpt.reset();
        rpt.setFieldAlignment(QTextStream::AlignLeft);
        rpt.setFieldWidth(6);
        rpt << i;
        rpt.setFieldWidth(4);
        rpt << " w = ";
        rpt.setFieldWidth(6);
        rpt << w;
        rpt.setFieldWidth(4);
        rpt << " h = ";
        rpt.setFieldWidth(6);
        rpt << h;
        rpt.reset();
        rpt << " ";
        rpt << keys.at(i);
    }
    rpt << "\n" << keys.length() << " images in image cache.";

    return reportString;
}

QString ImageCache2::reportCacheProgress(QString action)
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
    return reportString;
}

void ImageCache2::reportRunStatus()
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

void ImageCache2::addCacheItemImageMetadata(ImageMetadata m)
{
    mutex.lock();
    if (G::isLogger || G::isFlowLogger) G::log(__FUNCTION__);
    int row = cacheKeyHash[m.fPath];
    /* cacheItemList is a list of cacheItem used to track the current
       cache status and make future caching decisions for each image
       8 bits X 3 channels + 8 bit depth = (32*w*h)/8/1024/1024 = w*h/262144
    */
    icd->cacheItemList[row].sizeMB = static_cast<int>(m.width * m.height * 1.0 / 262144);
    icd->cacheItemList[row].isMetadata = m.width > 0;
    // decoder parameters
    icd->cacheItemList[row].metadataLoaded = m.metadataLoaded;
    icd->cacheItemList[row].orientation = m.orientation;
    icd->cacheItemList[row].rotationDegrees = m.rotationDegrees;
    icd->cacheItemList[row].offsetFull = m.offsetFull;
    icd->cacheItemList[row].lengthFull = m.lengthFull;
    icd->cacheItemList[row].samplesPerPixel = m.samplesPerPixel;
    icd->cacheItemList[row].iccBuf = m.iccBuf;

//    icd->cache.folderMB += icd->cacheItem.sizeMB; // req'd?
    mutex.unlock();
}

void ImageCache2::buildImageCacheList()
{
/*
    The imageCacheList must match dm->sf and contains the information required to maintain the
    image cache. It takes the form (example):

    Key Priority   Target Attempts  Caching   Thread   Cached   SizeMB   File Name
      0        0        1        2        0        4        1      119   D:/Pictures/Calendar_Beach/2014-09-26_0288.jpg
      1        1        1        1        0        1        1       34   D:/Pictures/Calendar_Beach/2014-07-29_0002.jpg
      2        2        1        2        0        7        1      119   D:/Pictures/Calendar_Beach/2014-04-11_0060.jpg
      3        3        1        2        0        5        1      119   D:/Pictures/Calendar_Beach/2012-08-16_0015.jpg

    It is built from dm->sf (sorted and/or filtered datamodel).

    This is called after the file information has been added to the datamodel
    (DataModel::addFileData) and before MetaRead reads the image metadata in the image file.
    This ensures that the entire datamodel is included before image caching starts.
    The rest of the information is added as it is collected in MetaRead.
*/
    if (G::isLogger || G::isFlowLogger) G::log(__FUNCTION__);

    for (int row = 0; row < dm->sf->rowCount(); ++row) {
        QString fPath = dm->sf->index(row, G::PathColumn).data(G::PathRole).toString();
        if (fPath == "") continue;
        cacheKeyHash[fPath] = row;
        /* cacheItemList is a list of cacheItem used to track the current
           cache status and make future caching decisions for each image  */
        icd->cacheItem.key = row;              // need to be able to sync with imageList
        icd->cacheItem.origKey = row;          // req'd while setting target range
        icd->cacheItem.fPath = fPath;
        icd->cacheItem.isCaching = false;
        icd->cacheItem.attempts = 0;
        icd->cacheItem.threadId = -1;
        icd->cacheItem.isCached = false;
        icd->cacheItem.isTarget = false;
        icd->cacheItem.priority = row;
        icd->cacheItem.metadataLoaded = false;
        icd->cacheItemList.append(icd->cacheItem);
    }
//    icd->cache.folderMB = folderMB;  // req'd
}

void ImageCache2::initImageCache(int &cacheMaxMB,
                                int &cacheMinMB,
                                bool &isShowCacheStatus,
                                int &cacheWtAhead)
{
    mutex.lock();
    if (G::isLogger || G::isFlowLogger) G::log(__FUNCTION__);

    abort = false;

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

    if (icd->cache.isShowCacheStatus) {
        updateStatus("Clear", "ImageCache2::initImageCache");
    }

    // populate the new folder image list
    icd->cacheItemList.clear();
    cacheKeyHash.clear();
    // the total memory size of all the images in the folder currently selected
    icd->cache.totFiles = dm->sf->rowCount();
    buildImageCacheList();

    source = __FUNCTION__;
    mutex.unlock();
}

void ImageCache2::updateImageCacheParam(int &cacheSizeMB,
                                       int &cacheMinMB,
                                       bool &isShowCacheStatus,
                                       int &cacheWtAhead)
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
}

void ImageCache2::rebuildImageCacheParameters(QString &currentImageFullPath, QString source)
{
/*
    When the datamodel is filtered the image cache needs to be updated. The cacheItemList is
    rebuilt for the filtered dataset and isCached updated, the current image is set, and any
    surplus cached images (not in the filtered dataset) are removed from imCache.
    The image cache is now ready to run by calling setCachePosition().
*/
    if (G::isLogger) G::log(__FUNCTION__);
    if(dm->sf->rowCount() == 0) return;

    /*
    qDebug() << __FUNCTION__ << "Source:" << source;
    std::cout << diagnostics().toStdString() << std::flush;
    //*/

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
    }

    // if the sort has been reversed
    if (source == "SortChange") icd->cache.isForward = !icd->cache.isForward;

    setPriorities(icd->cache.key);
    setTargetRange();

    QVector<QString> keys;
    icd->imCache.getKeys(keys);
    for (int i =  - 1; i > -1; --i) {
        if (!filteredList.contains(keys.at(i))) icd->imCache.remove(keys.at(i));
    }
//    for (int i = 0; i < keys.length(); ++i) {
//        if (!filteredList.contains(keys.at(i))) icd->imCache.remove(keys.at(i));
//    }

    if (icd->cache.isShowCacheStatus)
        updateStatus("Update all rows", "ImageCache2::rebuildImageCacheParameters");

//    setCurrentPosition(currentImageFullPath);
}

void ImageCache2::refreshImageCache()
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
        start();
    }
}

void ImageCache2::colorManageChange()
{
/*
    Called when color manage is toggled.  Reload all images in the cache.
*/
    if (G::isLogger) { G::log(__FUNCTION__); }
    refreshImageCache();
}

void ImageCache2::cacheSizeChange()
{
    /*
    Called when changes are made to image cache parameters in preferences. The image cache
    direction, priorities and target are reset.  makeRoom is executed.
    */
    if (G::isLogger) G::log(__FUNCTION__);
    QMutexLocker locker(&mutex);
    cacheSizeHasChanged = true;
    if (!isRunning()) {
        start();
    }
}

void ImageCache2::setCurrentPosition(QString path)
{
    /*
    Called from MW::fileSelectionChange to reset the position in the image cache. The image
    cache direction, priorities and target are reset and the cache is updated in fillCache.
    */
    if (G::isLogger || G::isFlowLogger) G::log(__FUNCTION__, path);
//    qDebug() << __FUNCTION__ << path;
    mutex.lock();
    currentPath = path;
    mutex.unlock();

    if (!isRunning()) {
        start();
    }
}

void ImageCache2::decodeNextImage(int id)
{
/*
    Called from fillCache to run a decoder for the next image to decode into a QImage.  The
    decoders run in separate threads.  When they complete the decoding they signal back to
    fillCache.
*/
    icd->cacheItemList[icd->cache.toCacheKey].isCaching = true;
    icd->cacheItemList[icd->cache.toCacheKey].threadId = id;
    icd->cacheItemList[icd->cache.toCacheKey].attempts += 1;
    if (debugCaching) {
        QString k = QString::number(icd->cache.toCacheKey).leftJustified((4));
        qDebug().noquote() << __FUNCTION__ << "decoder" << id
                           << "key =" << k
                           << icd->cacheItemList[icd->cache.toCacheKey].fPath
                              ;
    }
    decoder[id]->decode(icd->cacheItemList[icd->cache.toCacheKey]);
}

void ImageCache2::cacheImage(int id, int cacheKey)
{
/*
    Called from fillCache to insert a QImage that has been decoded into icd->imCache.
*/
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
    updateStatus("Update all rows", "ImageCache2::run inside loop");
}

bool ImageCache2::fillCache(int id, bool positionChange)
{
/*
    A number of ImageDecoders are created when ImageCache2 is created.  Each ImageDecoder runs
    in a separate thread.  The decoders convert an image file into a QImage and then signal
    this function with their id so the QImage can be inserted into the image cache.
    ImageDecoders are launched from CacheImage::run. CacheImage makes sure the necessary
    metadata is available and reads the file and then runs the decoder. This is very
    important, as file reads have to be sequential while the decoding can be performed
    synchronously, which significantly improves performance.

    The ImageDecoder has a status attribute that can be Ready, Busy or Done. When the decoder
    is created and when the QImage has been inserted into the image cache, the status is set to
    Ready. When the decoder is called from CacheImage the status is set to Busy. Finally, when
    the decoder finishes the decoding in ImageDecoder::run the status is set to Done. Each
    decoder signals fillCache when the image has been converted into a QImage. Here the QImage
    is added to the imCache. If there are more targeted images to be cached, the next one is
    assigned to the decoder, which is run again. The decoders keep running until all the
    targeted images have been cached.

    Every time the ImageCache2::run function encounters a change trigger (file selection
    change, cache size, color manage or sort/filter change) the image cache parameters are
    updated and this function is called for each Ready decoder. The decoders not Ready are
    Busy and already in the fillCache loop.

    This function is protected with a mutex as it could be signalled simultaneously by several
    ImageDecoders.  NOTE: mutex does not seem to be required.

    Each decoder follows this basic pattern:
    - nextToCache
    - decodeNextImage
    - fillCache
    - cacheImage
      - makeRoom
      - nextToDecache
*/

//    QMutexLocker locker(&mutex);        // required? - seems to run fine without mutex.
    if (abort) return false;

    // new image selected?
    if (positionChange) {
        setKeyToCurrent();
        setDirection();
        icd->cache.currMB = getImCacheSize();
        setPriorities(icd->cache.key);
        setTargetRange();
        if (cacheSizeHasChanged) makeRoom(0, 0);
    }
    if (id == -1) {
        if (cacheUpToDate()) return true;
        else return false;
    }

    int cacheKey = -1;
    if (decoder[id]->fPath != "") cacheKey = cacheKeyHash[decoder[id]->fPath];

    if (debugCaching) {
        QString k = QString::number(cacheKey).leftJustified((4));
        qDebug().noquote() << __FUNCTION__
                 << "      decoder" << id
                 << "key =" << k
                 << "isRunning =" << decoder[id]->isRunning()
                 << decoder[id]->fPath
                    ;
    }

    // range check
    if (cacheKey >= icd->cacheItemList.length()) {
        if (icd->cacheItemList.length() > 0) {
            qWarning() << __FUNCTION__
                       << "Decoder" << id << decoder[id]->fPath
                       << "cacheKey =" << cacheKey
                       << "EXCEEDS icd->cacheItemList.length() of"
                       << icd->cacheItemList.length()
                          ;
            QString err = "cacheKey = " + QString::number(cacheKey) +
                          " exceeds icd->cacheItemList.length() of " +
                          QString::number(icd->cacheItemList.length());
        G::error(__FUNCTION__, decoder[id]->fPath, err);
        }
        cacheKey = -1;
    }

    // add decoded QImage to imCache.
    if (cacheKey != -1) {
        cacheImage(id, cacheKey);
    }

    // get next image to cache (nextToCache() defines cache.toCacheKey)
    if (!abort && nextToCache(id)) {
        decodeNextImage(id);
    }
    else { // caching completed
        decoder[id]->setReady();
        fixOrphans();
        emit updateIsRunning(false, true);  // (isRunning, showCacheLabel)
        if (debugCaching) {
            qDebug() << __FUNCTION__
                     << "      decoder" << id
                     << "cacheUpToDate = true";
        }
        if (icd->cache.isShowCacheStatus) {
            updateStatus("Update all rows", "ImageCache2::run after check for orphans");
        }
    }
    return true;
}

void ImageCache2::run()
{
/*
    Called by a new file selection, cache size change, sort, filter or color manage change.
    The cache status is updated (current key, direction of travel, priorities and the target
    range) by calling fillCache with a decoder id = -1. Then each ready decoder is sent to
    fillCache. Decoders are assigned image files, which they decode into QImages, and then
    added to imCache. More details are available in the fillCache comments and at the top of
    this class.
*/
    if (G::isLogger || G::isFlowLogger) G::log(__FUNCTION__);
    if (icd->cacheItemList.length() == 0) return;
    if (debugCaching) {
        qDebug().noquote() << __FUNCTION__;
    }

    // check available memory (another app may have used or released some memory)
    memChk();

    // update if position change or cacheSizeHasChanged
    if (fillCache(-1, true)) {   // decoder id, positionChange
        // cache is up-to-date
        return;
    }

    // signal MW cache status
    emit updateIsRunning(true, true);

    /* fill the cache with images.  Note use ImageDecoder::Status::Ready because
       decoder[id]->isRunning() resulted in empty images in imCache  */
    for (int id = 0; id < decoderCount; ++id) {
        if (decoder[id]->status == ImageDecoder::Status::Ready) {
            decoder[id]->fPath = "";
            fillCache(id);
        }
    }
}
