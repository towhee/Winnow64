#include "Cache/imagecache.h"

ImageCache::ImageCache(QObject *parent, DataModel *dm, Metadata *metadata) : QThread(parent)
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    // Pixmap is a class that loads either a QPixmap or QImage from a file
    this->dm = dm;
    this->metadata = metadata;
    getImage = new Pixmap(this, dm, metadata);

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

    . = all the thumbnails in dm->sf (filtered datamodel)
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
        uint toCacheKey;            // next file to cache
        uint toDecacheKey;          // next file to remove from cache
        bool isForward;             // direction of travel in folder
        int wtAhead;                // ratio cache ahead vs behind
        int totFiles;               // number of images available
        uint currMB;                // the current MB consumed by the cache
        uint maxMB;                 // maximum MB available to cache
        uint folderMB;              // MB required for all files in folder
        int targetFirst;            // beginning of target range to cache
        int targetLast;             // end of the target range to cache
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

cacheItemList:  the list of all the cacheItems

imCache: a hash structure indexed by image file path holding each QImage

Only functions called from ImageCache::run will run in another thread.  These functions must
respect potential thread collision issues.  The functions and objects are:

    nextToCache()
    nextToDecache()
    checkForOrphans()
    imCache (the list of QImage that is also used by ImageView)
    Pixmap  (the class used to load embedded jpgs as QImage from the image files)

*/

void ImageCache::clearImageCache()
{
    imCache.clear();
    toCache.clear();
    toDecache.clear();
    cache.currMB = 0;
    emit showCacheStatus("Clear Image Cache", 0, __FUNCTION__);
    // do not clear cacheItemList if called from start slideshow
    if (!G::isSlideShow) cacheItemList.clear();
}

void ImageCache::stopImageCache()
{
/* Note that initImageCache and updateImageCache both check if isRunning and stop a running
thread before starting again. Use this function to stop the image caching thread without a new
one starting when there has been a folder change. The cache status label in the status bar
will be hidden.
*/
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
    }
    emit updateIsRunning(false, false);  // flags = isRunning, showCacheLabel
    clearImageCache();
}

void ImageCache::pauseImageCache()
{
/* Note that initImageCache and updateImageCache both check if isRunning and stop a running
thread before starting again. Use this function to pause the image caching thread without a
new one starting when there is a change to the datamodel, such as filtration or sorting.
*/
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
        emit updateIsRunning(false, true);
    }
}

void ImageCache::resumeImageCache()
{
/*
Restart the image cache after it has been paused. This is used by metadataCacheThread after
scroll events and the imageCacheThread has been paused to resume loading the image cache.
*/
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    if (!isRunning()) start(IdlePriority);
}

QSize ImageCache::scalePreview(int w, int h)
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

int ImageCache::getImCacheSize()
{
    // return the current size of the cache
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    int cacheMB = 0;
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
/*
The target range is the list of images being targeted to cache, based on the current image,
the direction of travel, the caching strategy and the maximum memory allotted to the image
cache.

The start and end of the target range are determined (cache.targetFirst and cache.targetLast)
and the boolean isTarget is assigned for each item in in the cacheItemList.
*/
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
    // also build a list of files to decache
    int sumMB = 0;
    for (int i = 0; i < cache.totFiles; ++i) {
        // check if item metadata has been loaded and will be targeted
//        if (sumMB < cache.maxMB && !cacheItemList.at(i).isMetadata) {
//            // need to get metadata to calc memory req'd to cached

//            // file path and dm source row in case filtered or sorted
//            QString fPath = cacheItemList[i].fName;
//            int dmRow = dm->fPathRow[fPath];

//            // load metadata
//            QFileInfo fileInfo(fPath);
//            if (metadata->loadImageMetadata(fileInfo, true, true, false, true, __FUNCTION__)) {
//                metadata->imageMetadata.row = dmRow;
//                dm->addMetadataForItem(metadata->imageMetadata);
//                ulong w = dm->sf->index(i, G::WidthColumn).data().toInt();
//                ulong h = dm->sf->index(i, G::HeightColumn).data().toInt();
//                cacheItemList[i].sizeMB = (float)w * h / 262144;
//                cacheItemList[i].isMetadata = w > 0;
//            }
//        }
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

    // return order to key - same as dm->sf (sorted or filtered datamodel)
    std::sort(cacheItemList.begin(), cacheItemList.end(), &ImageCache::keySort);

    int i;
    for (i = 0; i < cache.totFiles; ++i) {
        if (cacheItemList.at(i).isTarget) {
            cache.targetFirst = i;
            break;
        }
    }
    for (int j = i; j < cache.totFiles; ++j) {
        if (!cacheItemList.at(j).isTarget) {
            cache.targetLast = j - 1;
            return;
        }
        cache.targetLast = cache.totFiles - 1;
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

//    qDebug() << __FUNCTION__ << "key =" << key << "cacheItemList.length() =" << cacheItemList.length();
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
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    for (int i = 0; i < cache.totFiles; ++i)
        if (cacheItemList[i].isTarget)
          if (cacheItemList[i].isCached == false) return false;
    return true;
}

void ImageCache::checkForOrphans()
{
/*
If the user jumps around rapidly in a large folder, where the target cache is smaller than the
entire folder, it is possible for the nextToCache and nextToDecache collections to get out of
sync and leave orphans in the image cache buffer. This function iterates through the image
cache, checking that all cached images are in the target. If not, they are removed from the
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

QString ImageCache::diagnostics()
{
    QString reportString;
    QTextStream rpt;
    rpt.setString(&reportString);
    rpt << Utilities::centeredRptHdr('=', objectName() + " ImageCache Diagnostics");
    rpt << "\n" ;
    rpt << reportCache("");
    rpt << reportImCache();

    rpt << "\n\n" ;
    return reportString;
}

QString ImageCache::reportCache(QString title)
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

    rpt << "\ncacheItemList (used to manage image cache):";
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
        << "isMeta"
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
//    std::cout << reportString.toStdString() << std::flush;

    for (int i = 0; i < cache.totFiles; ++i) {
        int row = dm->fPathRow[cacheItemList.at(i).fName];
//        rpt.flush();
//        reportString = "";
        rpt.setFieldWidth(9);
        rpt.setFieldAlignment(QTextStream::AlignRight);
        rpt << i
            << cacheItemList.at(i).key
            << cacheItemList.at(i).origKey
            << cacheItemList.at(i).isMetadata
            << cacheItemList.at(i).priority
            << cacheItemList.at(i).isTarget
            << cacheItemList.at(i).isCached
            << cacheItemList.at(i).sizeMB
            << dm->index(row, G::WidthColumn).data().toInt()
            << dm->index(row, G::HeightColumn).data().toInt();
//        << metadata->getWidth(cacheItemList.at(i).fName)
//                << metadata->getHeight(cacheItemList.at(i).fName);
        rpt.setFieldWidth(3);
        rpt << "   ";
        rpt.setFieldAlignment(QTextStream::AlignLeft);
        rpt.setFieldWidth(50);
        rpt << cacheItemList.at(i).fName;
        rpt.setFieldWidth(0);
        rpt << "\n";

//        std::cout << reportString.toStdString() << std::flush;
    }
    return reportString;
}

void ImageCache::reportToCache()
{
    qDebug() << "\nTo Cache: " << toCache.length() << "images:" ;
    for (int i = 0; i < toCache.length(); ++i)
        qDebug() << toCache.at(i);
}

QString ImageCache::reportImCache()
{
    QString reportString;
    QTextStream rpt;
    rpt.flush();
    reportString = "";
    rpt.setString(&reportString);
    rpt << "\nimCache hash:";
    QHash<QString, QImage>::iterator i;
    for (i = imCache.begin(); i != imCache.end(); ++i) {
        rpt << "\n";
        rpt << G::s(i.key());
        rpt << ": image width = " << G::s(i.value().width());
        rpt << " height = " << G::s(i.value().height());
    }
    return reportString;
//        qDebug() << i.key() << ": "
//                 << "width =" << i.value().width()
//                 << "height =" << i.value().height();
}

QString ImageCache::reportCacheProgress(QString action)
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
        return reportString;
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
    return reportString;
}

void ImageCache::buildImageCacheList()
{
/*
The imageCacheList must match dm->sf and contains the information required to maintain the
image cache. It takes the form (example):

Index Key OrigKey Priority Target Cached  SizeMB  Width Height  File Name
    0   0       0        0      1      0  65.792   5472   3078  D:/Pictures/_xmptest/2017-01-25_0911.tif
    1   1       1        1      1      0   77.97   5472   3648  D:/Pictures/_xmptest/Canon.JPG
    2   2       2        2      1      0   77.97   5472   3648  D:/Pictures/_xmptest/Canon1.cr2
    3   3       3        3      1      0   77.97   5472   3648  D:/Pictures/_xmptest/CanonPs.jpg

It is built from dm->sf (sorted and/or filtered datamodel).
*/
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
//    qDebug() << __FUNCTION__ << "for" << dm->currentFolderPath;
    cacheItemList.clear();
    // the total memory size of all the images in the folder currently selected
    float folderMB = 0;
    cache.totFiles = dm->sf->rowCount();

    for (int i = 0; i < dm->sf->rowCount(); ++i) {
        QString fPath = dm->sf->index(i, 0).data(G::PathRole).toString();
        /* cacheItemList is a list of cacheItem used to track the current
           cache status and make future caching decisions for each image  */
        cacheItem.key = i;              // need to be able to sync with imageList
        cacheItem.origKey = i;          // req'd while setting target range
        cacheItem.fName = fPath;
        cacheItem.isCached = false;
        cacheItem.isTarget = false;
        cacheItem.priority = i;
        // 8 bits X 3 channels + 8 bit depth = (32*w*h)/8/1024/1024
        float w = dm->sf->index(i, G::WidthColumn).data().toFloat();
        int h = dm->sf->index(i, G::HeightColumn).data().toInt();
        cacheItem.sizeMB = static_cast<int>(w * h / 262144);
//        if (cache.usePreview) {
//            QSize p = scalePreview(w, h);
//            w = p.width();
//            h = p.height();
//            cacheItem.sizeMB += (float)w * h / 262144;
//        }
        cacheItem.isMetadata = w > 0;
        cacheItemList.append(cacheItem);

        folderMB += cacheItem.sizeMB;
    }
    cache.folderMB = qRound(folderMB);
}

void ImageCache::updateImageCacheList()
{
/*
Update the width, height, size and isMetadata fields in the imageCacheList.
*/
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    for (int i = 0; i < dm->sf->rowCount(); ++i) {
        if (i >= cacheItemList.length()) {
/*            qDebug() << __FUNCTION__
                     << "cacheItemList[0].fName" << cacheItemList[0].fName
                     << "ITEM" << i
                     << "EXCEEDS CACHEITEMLIST LENGTH" << cacheItemList.length()
                     << dm->currentFilePath;*/
            return;
        }
        if (!cacheItemList[i].isMetadata) {
            // assume 8 bits X 3 channels + 8 bit depth = (32*w*h)/8/1024/1024
            ulong w = dm->sf->index(i, G::WidthColumn).data().toInt();
            ulong h = dm->sf->index(i, G::HeightColumn).data().toInt();
            cacheItemList[i].sizeMB = (float)w * h / 262144;
//            if (cache.usePreview) {
//                QSize p = scalePreview(w, h);
//                w = p.width();
//                h = p.height();
//                cacheItem.sizeMB += (float)w * h / 262144;
//            }
            cacheItemList[i].isMetadata = w > 0;
        }
    }
}

void ImageCache::initImageCache(int &cacheSizeMB,
     bool &isShowCacheStatus, int &cacheWtAhead,
     bool &usePreview, int &previewWidth, int &previewHeight)
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    // cancel if no images to cache
    if (!dm->sf->rowCount()) return;

//    G::isNewFolderLoaded = true;

    // just in case stopImageCache not called before this
    if (isRunning()) stopImageCache();

    // cache is a structure to hold cache management parameters
    cache.key = 0;
    cache.prevKey = -1;
    // the cache defaults to the first image and a forward selection direction
    cache.isForward = true;
    cache.countAfterDirectionChange = 0;
    // the amount of memory to allocate to the cache
    cache.maxMB = cacheSizeMB;
    cache.isShowCacheStatus = isShowCacheStatus;
    cache.wtAhead = cacheWtAhead;
    cache.targetFirst = 0;
    cache.targetLast = 0;
    cache.previewSize = QSize(previewWidth, previewHeight);
    cache.usePreview = usePreview;

    if (cache.isShowCacheStatus) emit showCacheStatus("Clear", 0, "ImageCache::initImageCache");

    // populate the new folder image list
    buildImageCacheList();
}

void ImageCache::updateImageCacheParam(int &cacheSizeMB, bool &isShowCacheStatus,
               int &cacheWtAhead, bool &usePreview, int &previewWidth, int &previewHeight)
{
/*
When various image cache parameters are changed in preferences they are updated here.
*/
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    cache.maxMB = cacheSizeMB;
    cache.isShowCacheStatus = isShowCacheStatus;
    cache.wtAhead = cacheWtAhead;
    cache.usePreview = usePreview;
    cache.previewSize = QSize(previewWidth, previewHeight);
}

void ImageCache::updateImageCachePosition(/*QString &fPath*/)
{
/*
Updates the cache for the current image in the data model. The cache key is set, forward or
backward progress is determined and the target range is updated. Image caching is reactivated.

THERE IS A PERFORMANCE ISSUE IF THIS IS CALLED DIRECTLY FROM MW::fileSelectionchange.
Apparently there needs to be a slight delay before calling.
*/
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    // just in case stopImageCache not called before this
    if (isRunning()) pauseImageCache();
    if (cacheItemList.count() == 0) return;

//    Q_ASSERT(cacheItemList.length() > 0);   // must be items cached    

    // get cache item key
    cache.key = 0;
    for (int i = 0; i < cacheItemList.count(); i++) {
        if (cacheItemList.at(i).fName == dm->currentFilePath) {
            cache.key = i;
            break;
        }
    }
    Q_ASSERT(cache.key < cacheItemList.length());

    /* if the direction of travel changes then delay reversing the caching direction
       until a second image is selected in the new direction of travel.  This prevents
       needless caching if the user justs reverses direction to check out the previous
       image */
    if ( cache.isForward && cache.key <= cache.prevKey) cache.countAfterDirectionChange++;
    if (!cache.isForward && cache.key >= cache.prevKey) cache.countAfterDirectionChange++;

    if (cache.countAfterDirectionChange > 1) {
        cache.isForward = !cache.isForward;
        cache.countAfterDirectionChange = 0;
    }

/*    qDebug() << __FUNCTION__ << "cache.isForward ="
             << cache.isForward
             << "cache.prevKey =" << cache.prevKey
             << "cache.key =" << cache.key
             << "directionChangeProgressCount =" << cache.countAfterDirectionChange;
*/

    // reverse if at end of list
    if (cache.key == cacheItemList.count() - 1) cache.isForward = false;

    cache.prevKey = cache.key;

    // may be new metadata for image size
    updateImageCacheList();
    cache.currMB = getImCacheSize();

    setPriorities(cache.key);
    setTargetRange();

    if (cache.isShowCacheStatus) {
        emit showCacheStatus("Update all rows",
                             cache.key,
                             "ImageCache::updateImageCachePosition");
    }

    // if all images are cached then we're done
    if (cacheUpToDate()) {
        /* instance where go from blank folder to one image folder.  The first image is
           directly loaded (and cached) in ImageView and the file selection position changes,
           so this function is called, but the cache is up-to-date.  Make sure the image cache
           activity light is turned off in the status bar, which is normally done at the end of
           ImageCache::run.
        */
//        qDebug() << __FUNCTION__ << "cache up-to-date - quitting image cache";
        emit updateIsRunning(false, true);  // bool isRunning, bool showCacheLabel
        return;
    }

//     reportCache();

    start(IdlePriority);
}

void ImageCache::rebuildImageCacheParameters(QString &currentImageFullPath)
{
/*
When the datamodel is filtered the image cache needs to be updated. The cacheItemList is
rebuilt for the filtered dataset and isCached updated, the current image is set, and any
surplus cached images (not in the filtered dataset) are removed from imCache.

The image cache is now ready to run by calling updateImageCachePosition().
*/
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    if(dm->sf->rowCount() == 0) return;
//    if(filteredFilePathList.length() == 0) return;

    // just in case stopImageCache not called before this
    if (isRunning()) pauseImageCache();

    // build a new cacheItemList for the filtered/sorted dataset
    buildImageCacheList();

    // update cacheItemList
    cache.key = 0;
    // list of files in cacheItemList (all the filtered files) used to check for surplus
    QStringList fList;

    // assign cache.key and update isCached status in cacheItemList
    for(int row = 0; row < cache.totFiles; ++row) {
        QString fPath = cacheItemList.at(row).fName;
        fList.append(fPath);
        // get key for current image
        if (fPath == currentImageFullPath) cache.key = row;
        // update cacheItemList for images already cached
        if (imCache.contains(fPath)) cacheItemList[row].isCached = true;
    }

    // remove surplus cached images from imCache if they are not in the filtered dataset
    QHashIterator<QString, QImage> i(imCache);
    while (i.hasNext()) {
        i.next();
        if (!fList.contains(i.key())) imCache.remove(i.key());
    }
}

void ImageCache::run()
{
/*
   The target range is all the files that will fit into the available cache memory based on
   the cache priorities and direction of travel. We want to make sure the target range is
   cached, decaching anything outside the target range to make room as necessary as image
   selection changes.
*/
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    emit updateIsRunning(true, true);
    static QString prevFileName = "";

    while (nextToCache()) {
        if (abort) {
            emit updateIsRunning(false, false);
            return;
        }

        // check can read image from file
        QString fPath = cacheItemList.at(cache.toCacheKey).fName;
        if (fPath == prevFileName) return;

        QImage im;
        if (getImage->load(fPath, im)) {
            // is there room in cache?
            int room = cache.maxMB - cache.currMB;
            int roomRqd = cacheItemList.at(cache.toCacheKey).sizeMB;
            /*
            qDebug() << __FUNCTION__
                     << "maxMB =" << cache.maxMB
                     << "currMB" << cache.currMB
                     << "room =" << room
                     << "toCache =" << cache.toCacheKey
                     << "roomRqd =" << roomRqd;*/
            while (room < roomRqd) {
                // make some room by removing lowest priority cached image
                if (nextToDecache()) {
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
/*            if (cache.usePreview) {
                imCache.insert(fPath + "_Preview", im.scaled(cache.previewSize,
                   Qt::KeepAspectRatio, Qt::FastTransformation));
            }
*/
        }
        cacheItemList[cache.toCacheKey].isCached = true;
        if (!toCache.isEmpty()) toCache.removeFirst();
        cache.currMB = getImCacheSize();
        if(cache.isShowCacheStatus)
            emit showCacheStatus("Update all rows", 0, "ImageCache::run inside loop");
        prevFileName = fPath;
    }
    checkForOrphans();
    cache.currMB = getImCacheSize();
    if(cache.isShowCacheStatus)
        emit showCacheStatus("Update all rows", 0,  "ImageCache::run after check for orphans");

    emit updateIsRunning(false, true);  // (isRunning, showCacheLabel)
//    reportCache("Image cache updated for " + cache.dir);
//    reportCacheManager("Image cache updated for " + cache.dir);
}
