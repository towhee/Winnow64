#include "Main/global.h"
#include "Cache/mdcache.h"

/*
The metadata cache reads the relevant metadata and preview thumbnail from the image files and
stores this information in the datamodel dm. The critical metadata is the offset and length of
the embedded JPGs and their width and height, which is required by the ImageCache and
ImageView to display the images and IconView to display the thumbnails. The other metadata is
in the nice-to-know category.

Reading the metadata can become time consuming and the thumbnails or icons can consume a lot
of memory, so the metadata is read in chunks. Each chunk size (number of image files) is the
greater of the default value of maxChunkSize or the number of thumbs visible in the GridView
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

    * A 100ms (MW::cacheDelay) singleShot timer is invoked to call MW::loadMetadataChunk. The
      delay allows rapid scrolling without firing the metadataCacheThread until the user
      pauses.

    * As above, the metadata chunk is loaded and the datamodel filter is updated.

    * If it was running the imageCacheThread is resumed. However, image caching is not
      signalled to start (only resume).

When the user selects a thumbnail or a filter or sort has been invoked.

    * MW::fileSelectionCahnge is called.

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
//    loadMap.clear();
}

bool MetadataCache::isAllMetadataLoaded()
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    allMetadataLoaded = true;
    for (int i = 0; i < dm->rowCount(); ++i) {
        if (dm->index(i, G::CreatedColumn).data().isNull()) {
            allMetadataLoaded = false;
            break;
        }
    }
    return allMetadataLoaded;
}

bool MetadataCache::isAllIconsLoaded()
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    bool loaded = true;
    for (int i = 0; i < dm->rowCount(); ++i) {
        if (dm->index(i, G::PathColumn).data(Qt::DecorationRole).isNull()) {
            loaded = false;
            break;
        }
    }
    return loaded;
}

void MetadataCache::loadNewMetadataCache(int thumbsPerPage)
{
/*
This function is called from MW::folderSelectionChange and will not have any filtering so we
can use the datamodel dm directly.  The loadMap keeps track of which images have been loaded.
The greater of the number (visible cells in the gridView or maxChunkSize) image files metadata
and icons are loaded into the datamodel.
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
    }
//    G::track(__FUNCTION__);
    abort = false;
    allMetadataLoaded = false;
    cacheImages = CacheImages::New;
    imageCacheWasRunning = false;
    iconsCached.clear();
    foundItemsToLoad = true;

    /* Create a map container for every row in the datamodel to track metadata caching. This
    is used to confirm all the metadata is loaded before ending the metadata cache. If the
    startRow is greater than zero then this means the scroll event has resulted in skipping
    ahead, and we do not want ot lose track of the thumbs already loaded. After the metadata
    and icons are loaded the imageCachingThread is invoked.
    */
//    loadMap.clear();
//    for(int i = 0; i < dm->rowCount(); ++i) loadMap[i] = false;

    folderPath = dm->currentFolderPath;
//    runImageCacheWhenDone = true;

    this->startRow = 0;
    int segmentSize = thumbsPerPage > maxChunkSize ? thumbsPerPage : maxChunkSize;
    this->endRow = startRow + segmentSize;
    if (this->endRow >= dm->rowCount()) this->endRow = dm->rowCount();
//    this->endRow = dm->rowCount();

    qDebug() << "MetadataCache::loadNewMetadataCache  "
             << "startRow" << startRow
             << "endRow" << endRow
             << "foundItemsToLoad" << foundItemsToLoad;

    setIconTargets(startRow , thumbsPerPage);
    start(TimeCriticalPriority);
}

void MetadataCache::loadMetadataCache(int startRow, int thumbsPerPage,
                                      CacheImages cacheImages)
{
/*
This function is called from anywhere in MW after a folder selection change has occurred.  The
datamodel may have been filtered.  The loadMap is not cleared, and it is updated as additional
image files are loaded.  The imageCacheThread is not invoked.
*/
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
//    if (allMetadataLoaded) return;

    if (isRunning()) {
        mutex.lock();
        abort = true;
        condition.wakeOne();
        mutex.unlock();
        wait();
    }
    abort = false;
//    runImageCacheWhenDone = false;
    this->cacheImages = cacheImages;

    if (imageCacheThread->isRunning()){
        imageCacheThread->pauseImageCache();
        imageCacheWasRunning = true;
    }
    else {
        imageCacheWasRunning = false;
    }

    int rowCount = dm->sf->rowCount();
    this->startRow = startRow >= 0 ? startRow : 0;
    int chunkSize = thumbsPerPage > maxChunkSize ? thumbsPerPage : maxChunkSize;
    this->endRow = startRow + chunkSize;
    if (this->endRow >= rowCount) this->endRow = rowCount;

    foundItemsToLoad = false;
    for (int i = startRow; i < endRow; ++i) {
        if (dm->index(i, G::PathColumn).data(Qt::DecorationRole).isNull())
            foundItemsToLoad = true;
        if (dm->index(i, G::CreatedColumn).data().isNull())
            foundItemsToLoad = true;
        if (foundItemsToLoad) break;
    }

    qDebug() << "MetadataCache::loadMetadataCache  "
             << "startRow" << startRow
             << "endRow" << endRow
             << "foundItemsToLoad" << foundItemsToLoad;

    setIconTargets(startRow , thumbsPerPage);
    start(TimeCriticalPriority);
}

void MetadataCache::loadAllMetadata()
{
/*
This function is intended to load metadata (but not the icon) quickly for the entire
datamodel. This information is required for a filter or sort operation, which requires
the entire dataset. Since the program will be waiting for the update this does not need
to run as a separate thread and can be executed directly.
*/
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    G::t.restart();
    t.restart();
    int count = 0;
    for (int row = 0; row < dm->rowCount(); ++row) {
        // is metadata already cached
        if (!dm->index(row, G::CreatedColumn).data().isNull()) continue;

        QString fPath = dm->index(row, 0).data(G::PathRole).toString();
        QFileInfo fileInfo(fPath);
        if (metadata->loadImageMetadata(fileInfo, true, true, false, true)) {
//            G::track(__FUNCTION__, "metadata->loadImageMetadata row " + QString::number(row));
            metadata->imageMetadata.row = row;
            dm->addMetadataItem(metadata->imageMetadata, true);
//            G::track(__FUNCTION__, "dm->addMetadataItem         row " + QString::number(row));
            count++;
//            qDebug() << row << fPath;
        }
    }
    allMetadataLoaded = true;
    qint64 ms = t.elapsed();
    qreal msperfile = (float)ms / count;
    qDebug() << "MetadataCache::loadAllMetadata for" << count << "files" << ms << "ms" << msperfile << "ms per file;";
//    emit updateAllMetadataLoaded(allMetadataLoaded);
}

void MetadataCache::setIconTargets(int start, int thumbsPerPage)
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    if (thumbsPerPage == 0) thumbsPerPage = maxChunkSize;
    iconTargetStart = start - thumbsPerPage;
    if (iconTargetStart < 0) iconTargetStart = 0;
    iconTargetEnd = start + thumbsPerPage * 2;
    if (iconTargetEnd > dm->sf->rowCount()) iconTargetEnd = dm->sf->rowCount();

    if (iconTargetStart < startRow) startRow = iconTargetStart;
    if (iconTargetEnd > endRow) endRow = iconTargetEnd;

    qDebug() << "MetadataCache::setIconTargets"
             << "start" << start
             << "thumbsPerPage" << thumbsPerPage
             << "iconTargetStart" << iconTargetStart
             << "iconTargetEnd" << iconTargetEnd;
}

void MetadataCache::iconCleanup()
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    QMutableListIterator<int> i(iconsCached);
    mutex.lock();
    QPixmap nullPm;
    while (i.hasNext()) {
        i.next();
        int row = i.value();
        if (row < iconTargetStart || row > iconTargetEnd) {
            i.remove();
            QStandardItem *item = new QStandardItem;
            QModelIndex idx = dm->index(row, 0, QModelIndex());
            item = dm->itemFromIndex(idx);
            item->setIcon(nullPm);
        }
    }
    mutex.unlock();
}

bool MetadataCache::loadMetadataIconChunk()
{
/*
Load the metadata and thumb (icon) for all the image files in a folder.
*/
    {
    #ifdef ISDEBUG
    mutex.lock(); G::track(__FUNCTION__); mutex.unlock();
    #endif
    }
    t.restart();
    G::t.restart();
    int count = 0;

    for (row = startRow; row < endRow; ++row) {
//    for (int row = startRow; row < dm->rowCount(); ++row) {
        if (abort) {
            emit updateAllMetadataLoaded(allMetadataLoaded);
            emit updateIsRunning(false, true, __FUNCTION__);
            return false;
        }

        // file path and dm source row in case filtered or sorted
        mutex.lock();
        idx = dm->sf->index(row, 0);
        int dmRow = dm->sf->mapToSource(idx).row();
        QString fPath = idx.data(G::PathRole).toString();
        mutex.unlock();

//        qDebug() << "WWW" << __FUNCTION__
//                 << "row =" << row
//                 << "dm->sf->rowCount" << dm->sf->rowCount()
//                 << "dmRow =" << dmRow << "fPath =" << fPath;

        // load metadata
        mutex.lock();
        if (dm->sf->index(row, G::CreatedColumn).data().isNull()) {
            QFileInfo fileInfo(fPath);
            /*
               tried emit signal to metadata but really slow
               emit loadImageMetadata(fileInfo, true, true, false);  */
            if (metadata->loadImageMetadata(fileInfo, true, true, false, true)) {
                metadata->imageMetadata.row = dmRow;
                dm->addMetadataItem(metadata->imageMetadata);
//                G::track("dm->addMetadataItem         row " + QString::number(row));
            }
        }
        mutex.unlock();
        count++;

        if (row < iconTargetStart) continue;
        if (row > iconTargetEnd) continue;

        // load icon
        mutex.lock();
        if (idx.data(Qt::DecorationRole).isNull()) {
            QImage image;
            bool thumbLoaded = thumb->loadThumb(fPath, image);
//            G::track("Thumb->loadThumb            row " + QString::number(row));
            if (thumbLoaded) {
                emit setIcon(dmRow, image.scaled(G::maxIconSize, G::maxIconSize, Qt::KeepAspectRatio));
                iconsCached.append(dmRow);
            }
        }
        mutex.unlock();
    }
    qint64 ms = t.elapsed();
    qreal msperfile = (float)ms / count;
    qDebug() << "MetadataCache::loadMetadataIconChunk for" << count << "files" << ms << "ms" << msperfile << "ms per file;";
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
    }
    if (foundItemsToLoad) {
        emit updateIsRunning(true, true, __FUNCTION__);

        mutex.lock();
        int rowCount = dm->rowCount();
        mutex.unlock();

        bool chunkLoaded = false;
        chunkLoaded = loadMetadataIconChunk();
        if (abort) {
            qDebug() << "!!!!  Aborting MetadataCache::run  "
                     << "row =" << row;
            return;
        }

        // check if all metadata and thumbs have been loaded
        allMetadataLoaded = true;
        mutex.lock();
        for(int i = 0; i < rowCount; ++i) {
            if (dm->sf->index(i, G::CreatedColumn).data().isNull()) {
                allMetadataLoaded = false;
                break;
            }
        }
        mutex.unlock();

        iconCleanup();

        emit updateAllMetadataLoaded(allMetadataLoaded);

        /* Only get bestFit on the first cache because the QListView rescrolls whenever a
           change to sizehint occurs  */
        if (cacheImages == CacheImages::New) emit updateIconBestFit();

        if (allMetadataLoaded) emit updateFilters();
        qApp->processEvents();

        // update status of metadataThreadRunningLabel in statusbar
        emit updateIsRunning(false, true, __FUNCTION__);
    }

    /* After loading metadata it is okay to cache full size images, where the
    target cache needs to know how big each image is (width, height) and the
    offset to embedded full size jpgs */

    qDebug() << "cacheImages =" << cacheImages
             << " (No = 0, "
                "New = 1, "
                "Update = 2, "
                "Resume = 3)";

    if (cacheImages == CacheImages::New) emit loadImageCache();
    if (cacheImages == CacheImages::Resume && imageCacheWasRunning)
        imageCacheThread->resumeImageCache();

//    if (!imageCacheThread->cacheUpToDate()) {
//        qDebug() << "Resuming image caching";
//        imageCacheThread->resumeImageCache();
//    }

}
