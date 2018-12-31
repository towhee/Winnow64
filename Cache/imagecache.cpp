#include "Cache/imagecache.h"

ImageCache::ImageCache(QObject *parent, Metadata *metadata) : QThread(parent)
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    this->metadata = metadata;
    pixmap = new Pixmap(this, metadata);

    restart = false;
    abort = false;
}

ImageCache::~ImageCache()
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
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

imCache: a hash structure indexed by image file path holding each QImage

*/

void ImageCache::stopImageCache()
{
/* Note that initImageCache and updateImageCache both check if isRunning and
terminate a running thread before starting again.  Use this function to stop
the image caching thread without a new one starting.*/
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
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
    qDebug() << G::t.restart() << "\t" << "••• Image Caching  " << fPath << msg;
}


void ImageCache::cacheStatus()
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    /* Displays a statusbar showing the cache status.
     * dependent on setTargetRange() being up-to-date  */

    // trap instance where cache out of sync
    if(cache.totFiles - 1 > cacheItemList.length()) return;

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
    // red
//    cachePosColor.setColorAt(0, QColor(125,0,0));
//    cachePosColor.setColorAt(1, QColor(75,0,0));
    // light green
    cachePosColor.setColorAt(0, QColor(158,200,158));
    cachePosColor.setColorAt(1, QColor(58,100,58));

    // show the rectangle for entire bar, representing all the files available
    pnt.fillRect(QRect(0, htOffset, cache.pxTotWidth, ht), cacheAllColor);

    // show the rectangle for target cache.  If the pos is close to
    // the boundary there could be spillover, which is added to the
    // target range in the other direction.
    int pxTargetStart = pxStart(cache.targetFirst);
    int pxTargetWidth = pxEnd(cache.targetLast) - pxTargetStart;
    pnt.fillRect(QRect(pxTargetStart, htOffset, pxTargetWidth, ht), cacheTargetColor);

    // show the rectangle for the current cache by painting each item that has been cached
    for (int i=0; i < cache.totFiles; ++i) {
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
//    qDebug() << G::t.restart() << "\t" << "cache size " + mbCacheSize;

    // ping mainwindow to show cache update in the status bar
    if (cache.isShowCacheStatus) emit showCacheStatus(pmCacheStatus);

#ifdef ISDEBUG
G::track(__FUNCTION__, "END");
#endif
}

QSize ImageCache::scalePreview(ulong w, ulong h)
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
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
    G::track(__FUNCTION__);
    #endif
    }
    return cache.previewSize;
}

ulong ImageCache::getImCacheSize()
{
    // return the current size of the cache
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    ulong cacheMB = 0;
    for (int i = 0; i < cacheItemList.size(); ++i) {
        if (cacheItemList.at(i).isCached)
            cacheMB += cacheItemList.at(i).sizeMB;
    }
    return cacheMB;
}

bool ImageCache::prioritySort(const CacheItem &p1, const CacheItem &p2)
{
    {
    #ifdef ISDEBUG
//    G::track(__FUNCTION__);
    #endif
    }
    return p1.priority < p2.priority;       // sort by priority
}

bool ImageCache::keySort(const CacheItem &k1, const CacheItem &k2)
{
    {
    #ifdef ISDEBUG
    //G::track(__FUNCTION__);
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
    G::track(__FUNCTION__);
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
    G::track(__FUNCTION__);
    #endif
    }
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
    G::track(__FUNCTION__);
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
    G::track(__FUNCTION__);
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

void ImageCache::checkForSurplus()
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
/*
If the user filters and the cache is being rebuilt check for images that are already
cached and no longer needed (not in the target range).  Make sure to call setTargetRange
first.
*/
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    QStringList cachedList;
    QHashIterator<QString, QImage> i(imCache);
    while (i.hasNext()) {
        i.next();
        cachedList << i.key();
//        cout << i.key() << ": " << i.value() << endl;
    }

    for(int i=0; i < cachedList.length(); i++) {
        QString fPath = cachedList.at(i);
        for (int i = 0; i < cache.totFiles; ++i) {
            if(cacheItemList.at(i).fName == fPath && cacheItemList.at(i).isTarget) break;
            imCache.remove(fPath);
        }
    }
}

void ImageCache::checkAlreadyCached()
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
/* If the user filters and the cache is being rebuilt check for images that are already
cached and update cacheItemList statis.  Make sure to call setTargetRange first.
*/
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    for (int i = 0; i < cache.totFiles; ++i) {
        if (imCache.contains(cacheItemList.at(i).fName)) {
            if (cacheItemList.at(i).isTarget) {
                cacheItemList[i].isCached = true;
            }
        }
    }
}

void ImageCache::checkForOrphans()
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
/*
If the user jumps around rapidly in a large folder, where the target cache
is smaller than the entire folder, it is possible for the nextToCache and
nextToDecache collections to get out of sync and leave orphans in the image
cache buffer.  This function iterates through the image cache, checking that
all cached images are in the target.  If not, they are removed from the
cache buffer.
*/
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    for (int i = 0; i < cache.totFiles; ++i) {
        if (imCache.contains(cacheItemList.at(i).fName)) {
            if (!cacheItemList.at(i).isTarget) {
                imCache.remove(cacheItemList.at(i).fName);
                cacheItemList[i].isCached = false;
            }
        }
    }
}

void ImageCache::reportCache(QString title)
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    QString reportString;
    QTextStream rpt;
    rpt.flush();
    reportString = "";
    rpt.setString(&reportString);

    rpt  << "\n Title:" << title
         << "  Key:" << cache.key
         << "  cacheMB:" << cache.currMB
         << "  Wt ahead:" << cache.wtAhead
         << "  Direction ahead:" << cache.isForward
         << "  Total files:" << cache.totFiles << "\n";

    rpt.reset();
    rpt.setFieldAlignment(QTextStream::AlignRight);
    rpt.setFieldWidth(9);
    rpt << "Index"
        << "Key"
        << "OrigKey"
        << "Priority"
        << "Target"
        << "Cached"
        << "SizeMB"
        << "Width"
        << "Height";
    rpt.setFieldWidth(3);
    rpt << "   ";
    rpt.setFieldAlignment(QTextStream::AlignLeft);
    rpt.setFieldWidth(50);
    rpt << "File Name";
    rpt.setFieldWidth(0);
    rpt << "\n";
    std::cout << reportString.toStdString() << std::flush;

    for (int i=0; i<cache.totFiles; ++i) {
        rpt.flush();
        reportString = "";
        rpt.setFieldWidth(9);
        rpt.setFieldAlignment(QTextStream::AlignRight);
        rpt << i
            << cacheItemList.at(i).key
            << cacheItemList.at(i).origKey
            << cacheItemList.at(i).priority
            << cacheItemList.at(i).isTarget
            << cacheItemList.at(i).isCached
            << cacheItemList.at(i).sizeMB
            << metadata->getWidth(cacheItemList.at(i).fName)
            << metadata->getHeight(cacheItemList.at(i).fName);
        rpt.setFieldWidth(3);
        rpt << "   ";
        rpt.setFieldAlignment(QTextStream::AlignLeft);
        rpt.setFieldWidth(50);
        rpt  << cacheItemList.at(i).fName;
        rpt.setFieldWidth(0);
        rpt << "\n";

        std::cout << reportString.toStdString() << std::flush;
    }
}

void ImageCache::reportCacheProgress(QString action)
{
    QString reportString;
    QTextStream rpt;
    rpt.flush();
    reportString = "";
    rpt.setString(&reportString);
    if (action == "Hdr") {
        rpt << "\nCache Progress:  total cache allocation = " << cache.maxMB << "MB\n";
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
        return;
    }

    rpt.flush();
    rpt.setFieldAlignment(QTextStream::AlignLeft);
    rpt.setFieldWidth(8);  rpt << action;
    rpt.setFieldAlignment(QTextStream::AlignRight);
    rpt.setFieldWidth(7);  rpt << cache.currMB;
    rpt.setFieldWidth(8);  rpt << cache.targetLast - cache.targetFirst;
    rpt.setFieldWidth(8);  rpt << cache.targetFirst;
    rpt.setFieldWidth(8);  rpt << cache.targetLast;
//    rpt.setFieldWidth(7);  rpt << currMB;
//    rpt.setFieldWidth(9);  rpt << currMB;
}

int ImageCache::pxMid(int key)
{
/*
returns the cache status bar x coordinate for the midpoint of the item key
*/
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
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
//    G::track(__FUNCTION__, cacheMgr.at(key).fName);
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
    G::track(__FUNCTION__);
    #endif
    }
    return qRound((float)cache.pxUnitWidth * (key+1));
}

void ImageCache::buildImageCacheList(QStringList &imageList)
{
/* The imageCacheList must match dm->sf and contains the information required
to maintain the image cache.  It takes the form:

Index      Key  OrigKey Priority   Target   Cached   SizeMB    Width   Height   File Name
    0        0        0        0        1        0  65.7923     5472     3078   D:/Pictures/_xmptest/2017-01-25_0911.tif
    1        1        1        1        1        0   77.976     5472     3648   D:/Pictures/_xmptest/Canon.JPG
    2        2        2        2        1        0   77.976     5472     3648   D:/Pictures/_xmptest/Canon1.cr2
    3        3        3        3        1        0   77.976     5472     3648   D:/Pictures/_xmptest/CanonPs.jpg

It is built from the imageList, which is sent from MW.
*/
    cacheItemList.clear();

    // the total memory size of all the images in the folder currently selected
    float folderMB = 0;
    cache.totFiles = imageList.size();

    for (int i=0; i < cache.totFiles; ++i) {
        QString fPath = imageList.at(i);
//        qDebug() << G::t.restart() << "\t" << "Image Cache row =" << i << fPath;
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
}

void ImageCache::initImageCache(QStringList &imageList, int &cacheSizeMB,
     bool &isShowCacheStatus, int &cacheStatusWidth, int &cacheWtAhead,
     bool &usePreview, int &previewWidth, int &previewHeight)
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
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
////        qDebug() << G::t.restart() << "\t" << "cacheMgr.size" << cacheMgr.size() << "folder now size" << imageList.size();
//        if (cacheMgr.size() == imageList.size()) return;
//    }

    imCache.clear();
//    cacheItemList.clear();

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

    // populate the new folder image list
    buildImageCacheList(imageList);
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

void ImageCache::updateCacheStatusCurrentImagePosition(QString &fPath)
{
/*
This function is called from MW::fileSelectionChange to update the position of
the current image in the cache status on the statusbar.  Normally this would be
called from updateImageCache (below) but it is triggered from a QTimer when
cycling through images to improve performance.
*/
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    for (int i = 0; i < cacheItemList.count(); i++) {
        if (cacheItemList.at(i).fName == fPath) {
            cache.key = i;
            break;
        }
    }

    if (cache.isShowCacheStatus) cacheStatus();
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__, "Completed");
    #endif
    }
}

void ImageCache::updateImageCachePosition(QString &fPath)
{
/*
Updates the cache for the current image in the data model.  The cache key is
set, forward or backward progress is determined and the target range is
updated.  Image caching is reactivated.
*/
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
//    qDebug() << G::t.restart() << "\t" << "\nImageView::updateImageCache";

    // just in case stopImageCache not called before this
    if (isRunning()) stopImageCache();

//    cache.key = imageList.indexOf(fPath);

    // get cache item key
    for (int i = 0; i < cacheItemList.count(); i++) {
        if (cacheItemList.at(i).fName == fPath) {
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

void ImageCache::filterImageCache(QStringList &filteredFilePathList,
                                   QString &currentImageFullPath)
{
/*
When the image list is filtered the image cache needs to be updated.  The
imageCacheList is rebuilt, the current image is set, the priorities are
recalculated, the target range is redone, the imCache is checked and surplus
items are removed and isCached in imageCacheList is updated for any images
already cached and retargeted.  The cache status is updated.  Finally the image
caching thread is restarted.
*/
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    if(filteredFilePathList.length() == 0) return;

    // just in case stopImageCache not called before this
    if (isRunning()) stopImageCache();

    buildImageCacheList(filteredFilePathList);
    cache.key = 0;
    for(int row = 0; row < cache.totFiles; ++row) {
        if(cacheItemList.at(row).fName == currentImageFullPath)
            cache.key = row;
    }
    cache.pxUnitWidth = (float)cache.pxTotWidth/cache.totFiles;


    cache.prevKey = cache.key;
    cache.currMB = getImCacheSize();

    setPriorities(cache.key);
    setTargetRange();
    checkAlreadyCached();
    checkForSurplus();

//    reportCache("filterImageCache after setPriorities and setTargetRange");

    if (cache.isShowCacheStatus) cacheStatus();

    start(IdlePriority);
}

void ImageCache::resortImageCache(QStringList &resortedFilePathList,
                                   QString &currentImageFullPath)
{
/*
The cacheMgr is rebuilt to mirror the current sorting in SortFilter (dm->sf).
If there is filtering then the entire cache is reloaded.
*/
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);;
    #endif
    }
    if (isRunning()) stopImageCache();

    cacheItemListCopy = cacheItemList;
    cacheItemList.clear();

    int filterRowCount = resortedFilePathList.count();

//    qDebug() << G::t.restart() << "\t" << "reindexImageCache - filterFilePathList" << resortedFilePathList;
//    qDebug() << G::t.restart() << "\t" << "cache.totFiles" << cache.totFiles;
    int i;
    for(int row = 0; row < filterRowCount; ++row) {
//        qDebug() << G::t.restart() << "\t" << "row" << row;
        if(resortedFilePathList[row] == currentImageFullPath) cache.key = row;
        for (i = 0; i < cache.totFiles; ++i) {
/*            qDebug() << G::t.restart() << "\t" << "i" << i
                     << cacheMgrCopy.at(i).fName
                     << filterFilePathList[row]
                     << currentImageFullPath;
                     */
            if(cacheItemListCopy.at(i).fName == resortedFilePathList[row]) break;
        }
        cacheItem.fName = resortedFilePathList[row];
        cacheItem.isCached = cacheItemListCopy.at(i).isCached;
        cacheItem.isTarget = cacheItemListCopy.at(i).isTarget;
        cacheItem.key = row;
        cacheItem.origKey = cacheItemListCopy.at(i).origKey;
        cacheItem.priority = cacheItemListCopy.at(i).priority;
        cacheItem.sizeMB = cacheItemListCopy.at(i).sizeMB;

        cacheItemList.append(cacheItem);
    }

/*    qDebug() << G::t.restart() << "\t" << "\nIndex      Key     OrigKey     Priority         Target     Cached          SizeMB    Width      Height        FName";
    for (int i=0; i<cache.totFiles; ++i) {
        qDebug() << G::t.restart() << "\t" << i << "\t" <<
                 << cacheMgrCopy.at(i).key << "\t" <<
                 << cacheMgrCopy.at(i).origKey << "\t" <<
                 << cacheMgrCopy.at(i).priority << "\t" <<
                 << cacheMgrCopy.at(i).isTarget << "\t" <<
                 << cacheMgrCopy.at(i).isCached << "\t" <<
                 << cacheMgrCopy.at(i).sizeMB << "\t" <<
                 << metadata->getWidth(cacheMgrCopy.at(i).fName) << "\t" <<
                 << metadata->getHeight(cacheMgrCopy.at(i).fName) << "\t" <<
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
    G::track(__FUNCTION__);
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

        QImage im;
        if (pixmap->load(fPath, im)) {
            // is there room in cache?
            uint room = cache.maxMB - cache.currMB;
            uint roomRqd = cacheItemList.at(cache.toCacheKey).sizeMB;
            while (room < roomRqd) {
                // make some room by removing lowest priority cached image
                if(nextToDecache()) {
                    QString s = cacheItemList[cache.toDecacheKey].fName;
                    imCache.remove(s);
                    emit updateCacheOnThumbs(s, false);
                    if (!toDecache.isEmpty()) toDecache.removeFirst();
                    cacheItemList[cache.toDecacheKey].isCached = false;
                    cache.currMB -= cacheItemList[cache.toDecacheKey].sizeMB;
                    room = cache.maxMB - cache.currMB;
                }
                else break;
            }
            imCache.insert(fPath, im);
            emit updateCacheOnThumbs(fPath, true);
            if (cache.usePreview) {
                imCache.insert(fPath + "_Preview", im.scaled(cache.previewSize,
                   Qt::KeepAspectRatio, Qt::FastTransformation));
            }
        }
        cacheItemList[cache.toCacheKey].isCached = true;
        if (!toCache.isEmpty()) toCache.removeFirst();
        cache.currMB = getImCacheSize();
        if (cache.isShowCacheStatus) cacheStatus();
        prevFileName = fPath;
    }
    checkForOrphans();
    if (cache.isShowCacheStatus) cacheStatus();
    emit updateIsRunning(false);
//    reportCache("Image cache updated for " + cache.dir);
//    reportCacheManager("Image cache updated for " + cache.dir);
}
