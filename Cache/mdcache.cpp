#include "Main/global.h"
#include "Cache/mdcache.h"

/*
The metadata cache reads the relevant metadata and preview thumbnail from the image files and
stores this information in the datamodel dm. The critical metadata is the offset and length of
the embedded JPGs, which is required by the ImageCache and ImageView to display the images and
IconView to display the thumbnails. The other metadata is in the nice-to-know category.

Reading the metadata can become time consuming and the thumbnails or icons can consume a lot
of memory, so the metadata is read in chunks. Each chunk size (number of image files) is the
greater of the default value of maxSegmentSize or the number of thumbs visible in the GridView
viewport.

To keep it simple and minimize several threads trying to access the same image file at the same
time the following order of events occurs when Winnow is started or a new folder is selected:

    * Winnow is started and an instance of MetadataCache is created (metadataCacheThread). An
      instance of ImageCache also created (imageCacheThread).

    * A new folder is selected and MW::folderSelectionChange executes, which in turn, starts
      metadataCacheThread::loadNewMetadataCache.

    * loadNewMetadataCache starts the thread and metadataCacheThread::run reads a chunk of
      image files, loading the data to the datamodel.

    * After the chunk is loaded metadataCacheThread emits a signal to start imageCacheThread.

    * The new chunk of metadata is added to the datamodel filters.

When the user scrolls through the thumbnails:

    * If it is running the imageCacheThread is paused.

    * A 300ms singleShot timer is invoked to call MW::delayProcessLoadMetadataCacheScrollEvent, The delay allows rapid scrolling without
      firing the metadataCacheThread until the user pauses.

    * As above, the metadata chunk is loaded and the datamodel filter is updated.

    * If it was running the imageCacheThread is resumed.

When the user selects a thumbnail:

    * Check to see if a new chunk of metadata is required (has the metadata already been
      loaded.

    * Check to see if the ImageCache needs to be updated.

    * Load ImageView either from the cache or directly from the file if not cached.
*/

MetadataCache::MetadataCache(QObject *parent, DataModel *dm,
                  Metadata *metadata, ImageCache *imageCacheThread) : QThread(parent)
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    this->dm = dm;
    this->metadata = metadata;
    this->imageCacheThread = imageCacheThread;
    thumb = new Thumb(this, metadata);
    restart = false;
    abort = false;
    thumbMax.setWidth(G::maxIconSize);
    thumbMax.setHeight(G::maxIconSize);
}

MetadataCache::~MetadataCache()
{
    mutex.lock();
    abort = true;
    condition.wakeOne();
    mutex.unlock();
    wait();
}

void MetadataCache::stopMetadateCache()
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    #ifdef ISPROFILE
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
        emit updateIsRunning(false, false, __FUNCTION__);
    }
    loadMap.clear();
}

void MetadataCache::loadNewMetadataCache(int startRow, int thumbsPerPage)
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    #ifdef ISPROFILE
    G::track(__FUNCTION__);
    #endif
    }
    if (isRunning()) {
        mutex.lock();
        abort = true;
        condition.wakeOne();
        mutex.unlock();
        wait();
    }
//    G::track(__FUNCTION__);
    abort = false;
    allMetadataLoaded = false;

    /* Create a map container for every row in the datamodel to track metadata caching.
    This is used to confirm all the metadata is loaded before ending the metadata cache.  If
    the startRow is greater than zero then this means the scroll event has resulted in skipping
    ahead, and we do not want ot lose track of the thumbs already loaded.
    */
//    if (startRow == 0) {
        loadMap.clear();
        for(int i = 0; i < dm->rowCount(); ++i) loadMap[i] = false;
//    }

    folderPath = dm->currentFolderPath;

//    this->isShowCacheStatus = isShowCacheStatus;
//    if (isShowCacheStatus) emit showCacheStatus(0, true);

    runImageCacheWhenDone = true;

    this->startRow = startRow;
    int segmentSize = thumbsPerPage > maxSegmentSize ? thumbsPerPage : maxSegmentSize;
    this->endRow = startRow + segmentSize;
    if (this->endRow >= dm->rowCount()) this->endRow = dm->rowCount();

//    loadMetadataCache(startRow, thumbsPerPage);

    start(TimeCriticalPriority);
}

void MetadataCache::loadMetadataCache(int startRow, int thumbsPerPage)
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    if (allMetadataLoaded) return;

    if (isRunning()) {
        mutex.lock();
        abort = true;
        condition.wakeOne();
        mutex.unlock();
        wait();
    }
    abort = false;
    runImageCacheWhenDone = false;

    if (imageCacheThread->isRunning())imageCacheThread->pauseImageCache();

    this->startRow = startRow;
    int chunkSize = thumbsPerPage > maxSegmentSize ? thumbsPerPage : maxSegmentSize;
    this->endRow = startRow + chunkSize;
    if (this->endRow >= dm->rowCount()) this->endRow = dm->rowCount();

/*qDebug() << "MetadataCache::loadMetadataCache  "
         << "thumbsPerPage" << thumbsPerPage
         << "segmentSize" << chunkSize
         << "startRow" << startRow
         << "endRow" << endRow;*/

    // emulate old behavior
//    this->endRow = dm->rowCount();

    start(TimeCriticalPriority);
}

void MetadataCache::loadEntireMetadataCache()
{
/*
Call this function to make sure metadata has been loaded for every image in the datamodel.  This
is required when filtering and sorting the model.
*/
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    if (allMetadataLoaded) return;

    if (isRunning()) {
        mutex.lock();
        abort = true;
        condition.wakeOne();
        mutex.unlock();
        wait();
    }
    abort = false;
    runImageCacheWhenDone = false;

    if (imageCacheThread->isRunning())imageCacheThread->pauseImageCache();

    this->startRow = startRow;
    this->endRow = dm->rowCount();

    start(TimeCriticalPriority);
}

bool MetadataCache::loadMetadataChunk()
{
/*
Load the metadata and thumb (icon) for all the image files in a folder.
*/
    {
    #ifdef ISDEBUG
    mutex.lock(); G::track(__FUNCTION__); mutex.unlock();
    #endif
    }
//    int totRows = dm->rowCount();
    for (int row = startRow; row < endRow; ++row) {
        if (abort) {
            {
            #ifdef ISDEBUG
            QString s = "Aborting at row = " + QString::number(row);
            mutex.lock(); G::track(__FUNCTION__, s); mutex.unlock();
            #endif
            }
            emit updateAllMetadataLoaded(allMetadataLoaded);
            emit updateIsRunning(false, true, __FUNCTION__);
            return false;
        }

        // is metadata already cached
        if (loadMap[row]) continue;

        // maybe metadata loaded by user picking image while cache is still building
        mutex.lock();
        idx = dm->index(row, 0);
        QString fPath = idx.data(G::PathRole).toString();
        bool thumbLoaded = idx.data(Qt::DecorationRole).isValid();
        bool metadataLoaded = metadata->isLoaded(fPath);
        mutex.unlock();

        // update the cache status graphic in the status bar
        if (metadataLoaded && thumbLoaded) {
            loadMap[row] = true;
//            if (isShowCacheStatus) emit showCacheStatus(row, false);
            continue;
        }

        // update status
//        QString s = "Loading metadata " + QString::number(row + 1) + " of " + QString::number(totRows);
//        emit updateStatus(false, s);

        {
        #ifdef ISDEBUG
        QString s = "Attempting to load metadata for row " + QString::number(row + 1) + " " + fPath;
        mutex.lock(); G::track(__FUNCTION__, s); mutex.unlock();
        #endif
        }

        // load metadata
        if (!metadataLoaded) {
            QFileInfo fileInfo(fPath);
            metadataLoaded = true;
            // tried emit signal to metadata but really slow
            // emit loadImageMetadata(fileInfo, true, true, false);
            mutex.lock();
            if (metadata->loadImageMetadata(fileInfo, true, true, false, true)) {
                metadata->imageMetadata.row = row;
                dm->addMetadataItem(metadata->imageMetadata);
                metadataLoaded = true;
                {
                #ifdef ISDEBUG
                QString s = "Loaded metadata for row " + QString::number(row + 1) + " " + fPath;
                G::track(__FUNCTION__, s);
                #endif
                }
            }
            mutex.unlock();
        }

        if (!thumbLoaded) {
            QImage image;
            mutex.lock();
            thumbLoaded = thumb->loadThumb(fPath, image);
            mutex.unlock();
            if (thumbLoaded) {
                emit setIcon(row, image.scaled(G::maxIconSize, G::maxIconSize, Qt::KeepAspectRatio));
            }
        }
        else {
            QString s = "Thumb icon not obtained for row " + QString::number(row + 1) + " " + fPath;
            mutex.lock(); G::track(__FUNCTION__, s); mutex.unlock();
        }

        if (metadataLoaded && thumbLoaded) {
            loadMap[row] = true;
//            if (isShowCacheStatus) emit showCacheStatus(row, false);
        }
//        qDebug() << "MetadataCache::loadMetadataSegment()" << row;
    }
    return true;
}

void MetadataCache::run()
{
/*
Load all metadata, make multiple attempts if not all metadata loaded in the
first pass through the images in the folder.  The QMap loadedMap is used to
track if the metadata and thumb have been cached for each image.  When all items
in loadedMap == true then okay to finish.

While the MetadataCache is loading the metadata and thumbs, the user can impact
the process in two ways:

1.  If the user selects an image and the metadata or thumb has not been loaded
    yet then the main program thread goes ahead and loads the metadata and thumb.
    Metadata::loadMetadata checks for this possibility.

2.  If the user scrolls in the gridView or thumbView a signal is fired in MW and
    loadMetadataCache is called to load the thumbs for the scroll area.  As a
    result the current process is aborted and a new one is started with startRow
    set to the first visible thumb.

Since the user can cause a irregular loading sequence, the do loop makes a sweep
through loadedMap after each call to loadMetadata to see if there are any images
that have been missed.
*/
    {
    #ifdef ISDEBUG
    mutex.lock(); G::track(__FUNCTION__); mutex.unlock();
    #endif
    #ifdef ISPROFILE
    G::track(__FUNCTION__);
    #endif
    }

    emit updateIsRunning(true, true, __FUNCTION__);

    mutex.lock();
    int rowCount = dm->rowCount();
    mutex.unlock();

    bool chunkLoaded = false;

    do {
        if (abort) {
            {
            #ifdef ISDEBUG
            mutex.lock(); G::track(__FUNCTION__, "Aborting ..."); mutex.unlock();
            #endif
            }

            emit updateAllMetadataLoaded(allMetadataLoaded);
            emit updateIsRunning(false, false, __FUNCTION__);
            return;
        }
        {
        #ifdef ISDEBUG
        mutex.lock(); G::track(__FUNCTION__, "try loadMetadata"); mutex.unlock();
        #endif
        }

        chunkLoaded = loadMetadataChunk();
        // check if all metadata and thumbs have been loaded
        allMetadataLoaded = true;
        for(int i = 0; i < rowCount; ++i) {
            if (!loadMap[i]) {
                startRow = i;
                allMetadataLoaded = false;
                break;
            }
        }
        {
        #ifdef ISDEBUG
        QString s = "allMetadataLoaded = " + allMetadataLoaded ? "true" : "false";
        mutex.lock(); G::track(__FUNCTION__, s); mutex.unlock();
        #endif
        }
    }
    while (!allMetadataLoaded && !chunkLoaded);  // && t.elapsed() < 30000);
    emit updateAllMetadataLoaded(allMetadataLoaded);

    if (allMetadataLoaded) emit updateFilters();

    qApp->processEvents();

    /* After loading metadata it is okay to cache full size images, where the
    target cache needs to know how big each image is (width, height) and the
    offset to embedded full size jpgs */

//    qDebug() << "if (runImageCacheWhenDone) emit loadImageCache()";
    if (runImageCacheWhenDone) emit loadImageCache();

    if (!imageCacheThread->cacheUpToDate()) {
        qDebug() << "Resuming image caching";
        imageCacheThread->resumeImageCache();
    }

    // update status of metadataThreadRunningLabel in statusbar
    emit updateIsRunning(false, true, __FUNCTION__);

    // update the item counts in Filters
//    emit updateFilterCount();
}
