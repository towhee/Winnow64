#include "imagecache.h"

ImageCache::ImageCache(QObject *parent, Metadata *metadata) : QThread(parent)
{
    {
    #ifdef ISDEBUG
    qDebug() << "ImageCache::ImageCache";
    #endif
    }
    this->metadata = metadata;
    pixmap = new Pixmap(this, metadata);

    restart = false;
    abort = false;
}

ImageCache::~ImageCache()
{
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

    . = all the images in the folder = all the thumbnails
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
        QString dir;                // compare to input to see if different
        uint toCacheKey;            // next file to cache
        uint toDecacheKey;          // next file to remove from cache
        bool isForward;             // direction of travel in folder
        float wtAhead;              // ratio cache ahead vs behind
        int totFiles;               // number of images available
        uint currMB;                // the current MB consumed by the cache
        uint maxMB;                 // maximum MB available to cache
        uint folderMB;              // MB required for all files in folder
        int targetFirst;            // beginning of target range to cache
        int targetLast;             // end of the target range to cache
        int pxTotWidth;             // width in pixels of graphic in statusbar
        float pxUnitWidth;          // width of one file on graphic in statusbar
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

cacheMgr:  the list of all the cacheItems

imCache: a hash structure indexed by image file path holding each QPixmap

*/

void ImageCache::stopImageCache()
{
/* Note that initImageCache and updateImageCache both check if isRunning and
terminate a running thread before starting again.  Use this function to stop
the image caching thread without a new one starting.*/
    {
    #ifdef ISDEBUG
    qDebug() << "ImageCache::stopImageCache";
    #endif
    }
    if (isRunning()) {
        mutex.lock();
        abort = true;
        condition.wakeOne();
        mutex.unlock();
        wait();
        abort = false;
        emit updateIsRunning(false);
    }
}

void ImageCache::track(QString fPath, QString msg)
{
    qDebug() << "••• Image Caching  " << fPath << msg;
}


void ImageCache::cacheStatus()
{
    {
    #ifdef ISDEBUG
    qDebug() << "ImageCache::cacheStatus";
    #endif
    }
    /* Displays a statusbar showing the cache status.
     * dependent on setTargetRange() being up-to-date  */

    // trap instance where cache out of sync
    if(cache.totFiles-1 > cacheItemList.length()) return;

    // The app status bar is 25 pixels high.  Create a bitmap the height of the
    // status bar and cache.pxTotWidth wide the same color as the app status bar.
    // Then paint in the cache status progress in the middle of the bitmap.

    /* (where cache.pxTotWidth = 200, htOffset(9) + ht(8) = 17)
       X = pnt background, P = Cache Progress Area

    0,0  XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX  0,200
         XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
         XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
    0,9  PPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPP
         PPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPP
    0,17 PPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPP
         XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
         XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
    0,25 XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX  200,25

    for image n:
        pxStart(n) returns the x coordinate for the startpoint of image n
        pxMid(n) returns the x coordinate for the midpoint of image n
        pxEnd(n) returns the x coordinate for the endpoint of image n
        cache.pxUnitWidth = the width in pixels represented by an image

*/

    // Match the background color in the app status bar so blends in
    QColor cacheBGColor = QColor(85,85,85);

    // create a label bitmap to paint on
    QImage pmCacheStatus(QSize(cache.pxTotWidth, 25), QImage::Format_RGB32);
    pmCacheStatus.fill(cacheBGColor);
    QPainter pnt(&pmCacheStatus);

    int htOffset = 9;       // the offset from the top of pnt to the progress bar
    int ht = 8;             // the heigth of the progress bar

    // back color for the entire progress bar for all the files
    QLinearGradient cacheAllColor(0, htOffset, 0, ht+htOffset);
    cacheAllColor.setColorAt(0, QColor(150,150,150));
    cacheAllColor.setColorAt(1, QColor(100,100,100));

    // color for the portion of the total bar that is targeted to be cached
    // this depends on cursor direction, cache size and current cache status
    QLinearGradient cacheTargetColor(0, htOffset, 0, ht+htOffset);
    cacheTargetColor.setColorAt(0, QColor(125,125,125));
    cacheTargetColor.setColorAt(1, QColor(75,75,75));

    // color for the portion that has been cached
    QLinearGradient cacheCurrentColor(0, htOffset, 0, ht+htOffset);
    cacheCurrentColor.setColorAt(0, QColor(108,150,108));
    cacheCurrentColor.setColorAt(1, QColor(58,100,58));

    // color for the current image within the total images
    QLinearGradient cachePosColor(0, htOffset, 0, ht+htOffset);
    cachePosColor.setColorAt(0, QColor(125,0,0));
    cachePosColor.setColorAt(1, QColor(75,0,0));

    // show the rectangle for entire bar, representing all the files available
    pnt.fillRect(QRect(0, htOffset, cache.pxTotWidth, ht), cacheAllColor);

    // show the rectangle for target cache.  If the pos is close to
    // the boundary there could be spillover, which is added to the
    // target range in the other direction.
    int pxTargetStart = pxStart(cache.targetFirst);
    int pxTargetWidth = pxEnd(cache.targetLast) - pxTargetStart;
    pnt.fillRect(QRect(pxTargetStart, htOffset, pxTargetWidth, ht), cacheTargetColor);

    // show the rectangle for the current cache by painting each item that has been cached
    for (int i=0; i<cache.totFiles; ++i) {
        if (cacheItemList.at(i).isCached) {
            pnt.fillRect(QRect(pxStart(i), htOffset, cache.pxUnitWidth+1, ht), cacheCurrentColor);
        }
    }

    // show the current image position
    pnt.fillRect(QRect(pxStart(cache.key), htOffset, cache.pxUnitWidth+1, ht), cachePosColor);

    // build cache usage string
    QString mbCacheSize = QString::number(cache.currMB)
            + " of "
            + QString::number(cache.maxMB)
            + " MB";
//    qDebug() << "cache size " + mbCacheSize;

    // ping mainwindow to show cache update in the status bar
    if (cache.isShowCacheStatus) emit showCacheStatus(pmCacheStatus);
}

QSize ImageCache::scalePreview(ulong w, ulong h)
{
    {
    #ifdef ISDEBUG
    qDebug() << "ImageCache::scalePreview";
    #endif
    }
    QSize preview(w, h);
    preview.scale(cache.previewSize.width(), cache.previewSize.height(),
                  Qt::KeepAspectRatio);
    return preview;
}

QSize ImageCache::getPreviewSize()
{
    {
    #ifdef ISDEBUG
    qDebug() << "ImageCache::getPreviewSize";
    #endif
    }
    return cache.previewSize;
}

ulong ImageCache::getImCacheSize()
{
    // return the current size of the cache
    {
    #ifdef ISDEBUG
    qDebug() << "ImageCache::getImCacheSize";
    #endif
    }
    ulong cacheMB = 0;
    for (int i=0; i<cacheItemList.size(); ++i) {
        if (cacheItemList.at(i).isCached)
            cacheMB += cacheItemList.at(i).sizeMB;
    }
    return cacheMB;
}

bool ImageCache::prioritySort(const CacheItem &p1, const CacheItem &p2)
{
    {
    #ifdef ISDEBUG
    qDebug() << "ImageCache::prioritySort";
    #endif
    }
    return p1.priority < p2.priority;       // sort by priority
}

bool ImageCache::keySort(const CacheItem &k1, const CacheItem &k2)
{
    {
    #ifdef ISDEBUG
    qDebug() << "ImageCache::keySort";
    #endif
    }
    return k1.key < k2.key;       // sort by key to return to thumbnail order
}

void ImageCache::setTargetRange()
// calc cache.targetFirst and cache.targetLast and set isTarget
// in cacheManager.
{
    {
    #ifdef ISDEBUG
    qDebug() << "ImageCache::setTargetRange";
    #endif
    }
    toCache.clear();
    toDecache.clear();

    // sort by priority to make it easy to find highest priority not already cached
    std::sort(cacheItemList.begin(), cacheItemList.end(), &ImageCache::prioritySort);

    // assign target files to cache and build a list by priority
    // also build a list of files to dechache
    uint sumMB = 0;
    for (int i=0; i<cache.totFiles; ++i) {
        sumMB += cacheItemList.at(i).sizeMB;
        if (sumMB < cache.maxMB) {
            cacheItemList[i].isTarget = true;
            if (!cacheItemList.at(i).isCached)
                toCache.append(cacheItemList.at(i).origKey);
        }
        else {
            cacheItemList[i].isTarget = false;
            if (cacheItemList.at(i).isCached)
                toDecache.prepend(cacheItemList.at(i).origKey);
        }
    }

    // return order to key - same as thumbnails
    std::sort(cacheItemList.begin(), cacheItemList.end(), &ImageCache::keySort);

    int i;
    for (i=0; i<cache.totFiles; ++i) {
        if (cacheItemList.at(i).isTarget) {
            cache.targetFirst = i;
            break;
        }
    }
    for (int j=i; j<cache.totFiles; ++j) {
        if (!cacheItemList.at(j).isTarget) {
            cache.targetLast = j-1;
            break;
        }
        cache.targetLast = cache.totFiles-1;
    }
}

bool ImageCache::nextToCache()
{
    {
    #ifdef ISDEBUG
    qDebug() << "ImageCache::nextToCache";
    #endif
    }
//    qDebug() << "ImageCache::nextToCache length =" << toCache.length();
    if (toCache.length() > 0) {
        cache.toCacheKey = toCache.first();
        return true;
    }
    return false;
}

bool ImageCache::nextToDecache()
{
    {
    #ifdef ISDEBUG
    qDebug() << "ImageCache::nextToDecache";
    #endif
    }
    if (toDecache.length() > 0) {
        cache.toDecacheKey = toDecache.first();
        return true;
    }
    return false;
}

void ImageCache::setPriorities(int key)
{
    {
    #ifdef ISDEBUG
    qDebug() << "ImageCache::setPriorities";
    #endif
    }
    // key = current position = current selected thumbnail
    int aheadAmount = 1;
    int behindAmount = 1;                   // default 50/50 weighting
    int wtAhead = cache.wtAhead;
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
    cacheItemList[key].priority = 0;
    int i = 1;                  // start at 1 because current pos preset to zero
    if (cache.isForward) {
        aheadPos = key + 1;
        behindPos = key - 1;
        while (i < cache.totFiles) {
            for (int b = behindPos; b > behindPos - behindAmount; --b) {
                for (int a = aheadPos; a < aheadPos + aheadAmount; ++a) {
                    if (a >= cache.totFiles) break;
                    cacheItemList[a].priority = i++;
                    if (i >= cache.totFiles) break;
                    if (a == aheadPos + aheadAmount - 1 && b < 0) aheadPos += aheadAmount;
                }
                aheadPos += aheadAmount;
                if (b < 0) break;
                cacheItemList[b].priority = i++;
                if (i > cache.totFiles) break;
            }
            behindPos -= behindAmount;
        }
    }
    else {
        aheadPos = key - 1;
        behindPos = key + 1;
        while (i < cache.totFiles) {
            for (int b = behindPos; b < behindPos + behindAmount; ++b) {
                for (int a = aheadPos; a > aheadPos - aheadAmount; --a) {
                    if (a < 0) break;
                    cacheItemList[a].priority = i++;
                    if (i >= cache.totFiles) break;
                    if (a == aheadPos - aheadAmount + 1 && b > cache.totFiles)
                        aheadPos -= aheadAmount;
                }
                aheadPos -= aheadAmount;
                if (b >= cache.totFiles) break;
                cacheItemList[b].priority = i++;
                if (i > cache.totFiles) break;
            }
            behindPos += behindAmount;
        }
    }

//    reportCacheManager("setPriorities");
}

bool ImageCache::cacheUpToDate()
{
    for (int i = 0; i < cache.totFiles; ++i)
        if (cacheItemList[i].isTarget)
          if (cacheItemList[i].isCached == false) return false;
    return true;
}

void ImageCache::checkForOrphans()
{
    {
    #ifdef ISDEBUG
    qDebug() << "ImageCache::checkForOrphans";
    #endif
    }
/* If the user jumps around rapidly in a large folder, where the target cache
 is smaller than the entire folder, it is possible for the nextToCache and
 nextToDecache collections to get out of sync and leave orphans in the image
 cache buffer.  This function iterates through the image cache, checking that
 all cached images are in the target.  If not, they are removed from the
 cache buffer.
*/
    {
    #ifdef ISDEBUG
    qDebug() << "ImageCache::removeOrphans";
    #endif
    }
    for (int i = 0; i < cache.totFiles; ++i) {
        if (imCache.contains(cacheItemList.at(i).fName)) {
            if (!cacheItemList.at(i).isTarget) {
                imCache.remove(cacheItemList.at(i).fName);
                cacheItemList[i].isCached = false;
/*              qDebug() << "\n***********************************************************************"
                         << "\nREMOVED FROM IMAGE BUFFER:"
                         << cacheMgr.at(i).fName
                         << "\n***********************************************************************";
*/
            }
        }
    }
}

void ImageCache::reportCache(QString title)
{
    {
    #ifdef ISDEBUG
    qDebug() << "ImageCache::reportCacheManager";
    #endif
    }
    qDebug() << "\n" << title << "Key:" << cache.key
             <<  "cacheMB:" << cache.currMB
             << "Wt ahead:" << cache.wtAhead
             << "Direction ahead:" << cache.isForward
             << "Total files:" << cache.totFiles;
    qDebug() << "\nIndex      Key      OrigKey    Priority         Target      Cached         SizeMB    Width      Height         FName";
    for (int i=0; i<cache.totFiles; ++i) {
        qDebug() << i << "\t"
                 << cacheItemList.at(i).key << "\t"
                 << cacheItemList.at(i).origKey << "\t"
                 << cacheItemList.at(i).priority << "\t"
                 << cacheItemList.at(i).isTarget << "\t"
                 << cacheItemList.at(i).isCached << "\t"
                 << cacheItemList.at(i).sizeMB << "\t"
                 << metadata->getWidth(cacheItemList.at(i).fName) << "\t"
                 << metadata->getHeight(cacheItemList.at(i).fName) << "\t"
                 << cacheItemList.at(i).fName;
    }
}

int ImageCache::pxMid(int key)
{
/*
returns the cache status bar x coordinate for the midpoint of the item key
*/
    {
    #ifdef ISDEBUG
    qDebug() << "ImageCache::pxMid";
    #endif
    }
    return qRound((float)cache.pxUnitWidth * (key+1)
                  - cache.pxUnitWidth/2);
}

int ImageCache::pxStart(int key)
{
/*
returns the cache status bar x coordinate for the start of the item key
*/
    {
    #ifdef ISDEBUG
//    qDebug() << "ImageCache::pxStart" << cacheMgr.at(key).fName;
    #endif
    }
    return qRound((float)cache.pxUnitWidth * key);
}

int ImageCache::pxEnd(int key)
{
/*
returns the cache status bar x coordinate for the end of the item key
*/
    {
    #ifdef ISDEBUG
    qDebug() << "ImageCache::pxEnd";
    #endif
    }
    return qRound((float)cache.pxUnitWidth * (key+1));
}

void ImageCache::initImageCache(QStringList &imageList, int &cacheSizeMB,
     bool &isShowCacheStatus, int &cacheStatusWidth, int &cacheWtAhead,
     bool &usePreview, int &previewWidth, int &previewHeight)
{
    {
    #ifdef ISDEBUG
    qDebug() << "ImageCache::initImageCache";
    #endif
    }
    // cancel if no images to cache
    if (!imageList.size()) return;

    G::isNewFolderLoaded = true;

    // just in case stopImageCache not called before this
    if (isRunning()) stopImageCache();

    /* check if still in same folder with the same number of files. This could
    change if inclSubFolders is changed or images have been added or removed
    from the folder(s) */

//    if (imageList.at(0) == cache.dir) {
////        qDebug() << "cacheMgr.size" << cacheMgr.size() << "folder now size" << imageList.size();
//        if (cacheMgr.size() == imageList.size()) return;
//    }

    imCache.clear();
    cacheItemList.clear();

    // cache is a structure to hold cache management parameters
    cache.key = 0;
    cache.prevKey = -1;
    // the cache defaults to the first image and a forward selection direction
    cache.isForward = true;
    // the amount of memory to allocate to the cache
    cache.maxMB = cacheSizeMB;
    cache.isShowCacheStatus = isShowCacheStatus;
    cache.wtAhead = cacheWtAhead;
    // the width of the status progress bar representing all the images
    // to be cached
    cache.pxTotWidth = cacheStatusWidth;
    // the status bar width of a single image
    cache.pxUnitWidth = (float)cache.pxTotWidth/(imageList.size());
    cache.targetFirst = 0;
    cache.targetLast = 0;
    cache.totFiles = imageList.size();
    cache.dir = imageList.at(0);
    cache.previewSize = QSize(previewWidth, previewHeight);
    cache.usePreview = usePreview;
    // the total memory size of all the images in the folder currently selected
    float folderMB = 0;
    // get some intel on the new folder image list
    for (int i=0; i < imageList.size(); ++i) {
        QString fPath = imageList.at(i);
//        qDebug() << "Image Cache row =" << i << fPath;
        /* cacheManager is a list of cacheItem used to track the current
           cache status and make future caching decisions for each image  */
        cacheItem.key = i;              // need to be able to sync with imageList
        cacheItem.origKey = i;          // req'd while setting target range
        cacheItem.fName = fPath;
        cacheItem.isCached = false;
        cacheItem.isTarget = false;
        cacheItem.priority = i;
        // assume 8 bits X 3 channels + 8 bit depth = (32*w*h)/8/1024000
        ulong w = metadata->getWidth(fPath);
        ulong h = metadata->getHeight(fPath);
        cacheItem.sizeMB = (float)w * h / 256000;
        if (cache.usePreview) {
            QSize p = scalePreview(w, h);
            w = p.width();
            h = p.height();
            cacheItem.sizeMB += (float)w * h / 256000;
        }
        cacheItemList.append(cacheItem);

        folderMB += cacheItem.sizeMB;
    }
    cache.folderMB = qRound(folderMB);

//    reportCacheManager("Initialize test");
}

void ImageCache::updateImageCacheParam(int &cacheSizeMB, bool &isShowCacheStatus,
         int &cacheStatusWidth, int &cacheWtAhead, bool &usePreview,
         int &previewWidth, int &previewHeight)
{
    cache.maxMB = cacheSizeMB;
    cache.isShowCacheStatus = isShowCacheStatus;
    cache.pxTotWidth = cacheStatusWidth;
    cache.wtAhead = cacheWtAhead;
    cache.usePreview = usePreview;
    cache.previewSize = QSize(previewWidth, previewHeight);
}



void ImageCache::updateImageCache(QString &currentImageFullPath)
{
/*
Updates the cache for the current image in the data model.  The cache key is
set, forward or backward progress is determined and the target range is
updated.  Image caching is reactivated.
*/
    {
    #ifdef ISDEBUG
    qDebug() << "ImageView::updateImageCache";
    #endif
    }
//    qDebug() << "\nImageView::updateImageCache";

    // just in case stopImageCache not called before this
    if (isRunning()) stopImageCache();

//    cache.key = imageList.indexOf(currentImageFullPath);

    // get cache item key
    for (int i = 0; i < cacheItemList.count(); i++) {
        if (cacheItemList.at(i).fName == currentImageFullPath) {
            cache.key = i;
            break;
        }
    }

    if (cache.isShowCacheStatus) cacheStatus();
    cache.isForward = (cache.key >= cache.prevKey);
    // reverse if at end of list
    if (cache.key == cacheItemList.count() - 1) cache.isForward = false;
    cache.prevKey = cache.key;
    cache.currMB = getImCacheSize();

    setPriorities(cache.key);
    setTargetRange();

//    reportCache("Start of update: current image: " + currentImageFullPath);

    // if all images are cached then we're done
    if (cacheUpToDate()) return;

    start(IdlePriority);
}

void ImageCache::reindexImageCache(QStringList filterFilePathList,
                                   QString &currentImageFullPath)
{
/*
The cacheMgr is rebuilt to mirror the current sorting in SortFilter (dm->sf).
If there is filtering then the entire cache is reloaded.
*/
    {
    #ifdef ISDEBUG
    qDebug() << "ImageView::reindexImageCache";
    #endif
    }
    qDebug() << "ImageView::reindexImageCache" << currentImageFullPath;
//    return;
    if (isRunning()) stopImageCache();

    cacheItemListCopy = cacheItemList;
    cacheItemList.clear();

    int filterRowCount = filterFilePathList.count();

//    qDebug() << "reindexImageCache - filterFilePathList" << filterFilePathList;
//    qDebug() << "cache.totFiles" << cache.totFiles;
    int i;
    for(int row = 0; row < filterRowCount; ++row) {
//        qDebug() << "row" << row;
        if(filterFilePathList[row] == currentImageFullPath) cache.key = row;
        for (i = 0; i < cache.totFiles; ++i) {
/*            qDebug() << "i" << i
                     << cacheMgrCopy.at(i).fName
                     << filterFilePathList[row]
                     << currentImageFullPath;
                     */
            if(cacheItemListCopy.at(i).fName == filterFilePathList[row]) break;
        }
        cacheItem.fName = filterFilePathList[row];
        cacheItem.isCached = cacheItemListCopy.at(i).isCached;
        cacheItem.isTarget = cacheItemListCopy.at(i).isTarget;
        cacheItem.key = row;
        cacheItem.origKey = cacheItemListCopy.at(i).origKey;
        cacheItem.priority = cacheItemListCopy.at(i).priority;
        cacheItem.sizeMB = cacheItemListCopy.at(i).sizeMB;

        cacheItemList.append(cacheItem);
    }

/*    qDebug() << "\nIndex      Key     OrigKey     Priority         Target     Cached          SizeMB    Width      Height        FName";
    for (int i=0; i<cache.totFiles; ++i) {
        qDebug() << i << "\t"
                 << cacheMgrCopy.at(i).key << "\t"
                 << cacheMgrCopy.at(i).origKey << "\t"
                 << cacheMgrCopy.at(i).priority << "\t"
                 << cacheMgrCopy.at(i).isTarget << "\t"
                 << cacheMgrCopy.at(i).isCached << "\t"
                 << cacheMgrCopy.at(i).sizeMB << "\t"
                 << metadata->getWidth(cacheMgrCopy.at(i).fName) << "\t"
                 << metadata->getHeight(cacheMgrCopy.at(i).fName) << "\t"
                 << cacheMgrCopy.at(i).fName;
    }
    */

    cacheItemListCopy.clear();

    cache.totFiles = filterRowCount;
    cache.pxUnitWidth = (float)cache.pxTotWidth/filterRowCount;

    if (cache.isShowCacheStatus) cacheStatus();

    cache.prevKey = cache.key;
    cache.currMB = getImCacheSize();
    setPriorities(cache.key);
    setTargetRange();
    start(IdlePriority);
}

void ImageCache::run()
{
    {
    #ifdef ISDEBUG
    qDebug() << "ImageCache::run";
    #endif
    }

/*
   The target range is all the files that will fit into the available cache
   memory based on the cache priorities and direction of travel.  We want to
   make sure the target range is cached, decaching anything outside the target
   range to make room as necessary as image selection changes.
*/

    emit updateIsRunning(true);
    static QString prevFileName ="";
    while (nextToCache()) {
        if (abort) {
            emit updateIsRunning(false);
            return;
        }

        // check can read image from file
        QString fPath = cacheItemList.at(cache.toCacheKey).fName;
        if (fPath == prevFileName) return;
        if (G::isThreadTrackingOn) track(fPath, "Reading");
        QPixmap pm;
//        QPixmap *pm = new QPixmap;
//        QElapsedTimer t;
//        t.start();
        if (pixmap->load(fPath, pm)) {
//            qDebug() << "load pixmap elapsed time =" << fPath << t.restart();
            // is there room in cache?
            uint room = cache.maxMB - cache.currMB;
            uint roomRqd = cacheItemList.at(cache.toCacheKey).sizeMB;
            while (room < roomRqd) {
                // make some room by removing lowest priority cached image
                if(nextToDecache()) {
    //                qDebug() << "Removing" << cacheMgr[cache.toDecacheKey].fName
    //                         << "Before count" << imCache.count();
                    imCache.remove(cacheItemList[cache.toDecacheKey].fName);
    //                qDebug() << "After count" << imCache.count();
                    if (!toDecache.isEmpty()) toDecache.removeFirst();
                    cacheItemList[cache.toDecacheKey].isCached = false;
                    cache.currMB -= cacheItemList[cache.toDecacheKey].sizeMB;
                    room = cache.maxMB - cache.currMB;
                }
                else break;
            }
//            pm.setDevicePixelRatio(1.33333333);
//            pm->setDevicePixelRatio(G::devicePixelRatio);

//            mutex.lock();
            imCache.insert(fPath, pm);
//            imCache.insert(fPath, *pm);
            if (cache.usePreview) {
                imCache.insert(fPath + "_Preview", pm.scaled(cache.previewSize,
                   Qt::KeepAspectRatio, Qt::FastTransformation));
            }
            cacheItemList[cache.toCacheKey].isCached = true;
//            mutex.unlock();
            if (!toCache.isEmpty()) toCache.removeFirst();
            cache.currMB = getImCacheSize();
            cacheStatus();
//            delete pm;
        }
        prevFileName = fPath;
    }
    checkForOrphans();
    cacheStatus();
    emit updateIsRunning(false);
//    reportCacheManager("Image cache updated for " + cache.dir);
}
