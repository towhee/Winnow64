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

    * After the show event, the IconView first/last/thumbsPerPage is determined and
      metadataCacheThread::loadNewMetadataCache runs.

    * MW::loadNewMetadataCache starts metadataCacheThread and metadataCacheThread::run reads a
      chunk of image files, loading the metadata and icons to the datamodel.

    * After the chunk is loaded metadataCacheThread emits a signal to start imageCacheThread.

    *** The new chunk of metadata is added to the datamodel filters.

When Winnow is running and a new folder is selected

    * A new folder is selected and MW::folderSelectionChange executes, which in turn, starts
      metadataCacheThread::loadNewMetadataCache.

    * loadNewMetadataCache starts metadataCacheThread and metadataCacheThread::run reads a
      chunk of image files, loading the metadata and icons to the datamodel.

    * After the chunk is loaded metadataCacheThread emits a signal to start imageCacheThread.

When the user scrolls through the thumbnails:

    * If it is running the imageCacheThread is paused.

    * A 100ms (MW::cacheDelay) singleShot timer is invoked to call MW::loadMetadataChunk. The
      delay allows rapid scrolling without firing the metadataCacheThread until the user
      pauses.

    * As above, the metadata chunk is loaded and the datamodel filter is updated.

    * If it was running the imageCacheThread is resumed. However, image caching is not
      signalled to start (only resume).

When the user selects a thumbnail or a filter or sort has been invoked.

    * MW::fileSelectionChange is called.

    * Check to see if a new chunk of metadata is required (has the metadata already been
      loaded.

    * Check to see if the ImageCache needs to be updated.

    * Load ImageView either from the cache or directly from the file if not cached.

Caching strategies:

    * Load all metadata and icons at the start
    * Load all metadata and icon chunks
    * Load metadata and icon chunks
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
    qRegisterMetaType<QVector<int>>();
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
    G::allMetadataLoaded = true;
    for (int i = 0; i < dm->rowCount(); ++i) {
        if (dm->index(i, G::CreatedColumn).data().isNull()) {
            G::allMetadataLoaded = false;
            break;
        }
    }
    return G::allMetadataLoaded;
}

bool MetadataCache::isAllIconLoaded()
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

void MetadataCache::loadNewFolder(int thumbsPerPage)
{
/*
This function is called from MW::folderSelectionChange and will not have any filtering so we
can use the datamodel dm directly. The greater of the number (visible cells in the gridView or
maxChunkSize) image files metadata and icons are loaded into the datamodel.
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
    G::track(__FUNCTION__);
    abort = false;
    G::allMetadataLoaded = false;
    iconsCached.clear();
    foundItemsToLoad = true;
    int imageCount = dm->rowCount();

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
    if (metadataCacheAll || imageCount < metadataCacheAllIfLessThan)
        this->endRow = imageCount;
    else {
        int segmentSize = thumbsPerPage > metadataChunkSize ? thumbsPerPage : metadataChunkSize;
        this->endRow = startRow + segmentSize;
        if (this->endRow >= dm->rowCount()) this->endRow = dm->rowCount();
    }
//    this->endRow = dm->rowCount();

//    qDebug() << "MetadataCache::loadNewMetadataCache  "
//             << "startRow" << startRow
//             << "endRow" << endRow
//             << "foundItemsToLoad" << foundItemsToLoad;

    setIconTargets(startRow , thumbsPerPage);
    action = Action::NewFolder;
    start(TimeCriticalPriority);
}

void MetadataCache::loadAllMetadata()
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    G::track(__FUNCTION__);
    if (isRunning()) {
        mutex.lock();
        abort = true;
        condition.wakeOne();
        mutex.unlock();
        wait();
    }
    abort = false;
    startRow = 0;
    endRow = dm->rowCount();
    action = Action::AllMetadata;
    start(TimeCriticalPriority);
}

void MetadataCache::loadMetadataIconChunk(int fromRow, int thumbsPerPage)
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
    G::track(__FUNCTION__);
    qDebug() << "G::isInitializing =" << G::isInitializing
             << "fromRow =" << fromRow;
    if (isRunning()) {
        mutex.lock();
        abort = true;
        condition.wakeOne();
        mutex.unlock();
        wait();
    }
    abort = false;

    int rowCount = dm->sf->rowCount();
    startRow = fromRow >= 0 ? fromRow : 0;
    int chunkSize = thumbsPerPage > metadataChunkSize ? thumbsPerPage : metadataChunkSize;
    endRow = startRow + chunkSize;
    if (endRow > rowCount) this->endRow = rowCount;

    foundItemsToLoad = false;
    for (int i = startRow; i < endRow; ++i) {
        if (dm->sf->index(i, G::PathColumn).data(Qt::DecorationRole).isNull())
            foundItemsToLoad = true;
        if (dm->sf->index(i, G::CreatedColumn).data().isNull())
            foundItemsToLoad = true;
        if (foundItemsToLoad) break;
    }

//    qDebug() << "MetadataCache::loadMetadataCache  "
//             << "startRow" << startRow
//             << "endRow" << endRow
//             << "foundItemsToLoad" << foundItemsToLoad;

    setIconTargets(startRow , thumbsPerPage);
    action = Action::MetaIconChunk;
    start(IdlePriority);
//    start(TimeCriticalPriority);
}

void MetadataCache::loadIconChunk(int fromRow, int thumbsPerPage)
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
    }
    abort = false;
    action = Action::IconChunk;
    setIconTargets(startRow , thumbsPerPage);
    start(IdlePriority);
}

void MetadataCache::setIconTargets(int start, int thumbsPerPage)
{
/*
Icon target range is the rows where we want to see an icon (thumbnail). For example, when the
user scrolls through the gridView, the icons visible in the gridView need to be loaded. The
targets are assigned so we can cache the icons in advance. The number of icons visible in the
gridView or thumbView (whichever is greater) is thumbsPerPage. The strategy is to cache n
pages of icons before and after the current page, where n = iconPagesToCacheAhead.
*/
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    int rowCount = dm->sf->rowCount();
    if (thumbsPerPage == 0) thumbsPerPage = metadataChunkSize;
    iconTargetStart = start - thumbsPerPage * iconPagesToCacheAhead;
    if (iconTargetStart < 0) iconTargetStart = 0;

    if (iconsCacheAllIfLessThan > rowCount) {
        iconTargetEnd = rowCount;
    }
    else {
        iconTargetEnd = start + thumbsPerPage * (iconPagesToCacheAhead + 1);
        if (iconTargetEnd > rowCount) iconTargetEnd = rowCount;
        if (rowCount < iconsCacheAllIfLessThan) iconTargetEnd = rowCount;
    }

    // reset range for loadMetadataIconChunk to include icon target range
    if (iconTargetStart < startRow) startRow = iconTargetStart;
    if (iconTargetEnd > endRow) endRow = iconTargetEnd;

    // set thresholds before calling MW::loadMetadataChunk
    int targetSize = iconTargetEnd - iconTargetStart;
//    if (iconTargetStart > 0) recacheIfLessThan = iconTargetStart + targetSize / 2 - targetSize / 7;
//    else recacheIfLessThan = 0;
//    if (iconTargetEnd < rowCount) recacheIfGreaterThan = startRow + targetSize / 2 + targetSize / 7;
//    else recacheIfGreaterThan = rowCount;

    if (iconTargetStart > 0) recacheIfGreaterThan = iconTargetStart + targetSize / 2 - targetSize / 7;
    else recacheIfGreaterThan = rowCount;
    if (iconTargetEnd < rowCount) recacheIfLessThan = startRow + targetSize / 2 + targetSize / 7;
    else recacheIfLessThan = 0;

    qDebug() << "MetadataCache::setIconTargets"
             << "start" << start
             << "thumbsPerPage" << thumbsPerPage
             << "iconTargetStart" << iconTargetStart
             << "iconTargetEnd" << iconTargetEnd
             << "recacheIfGreaterThan" << recacheIfGreaterThan
             << "recacheIfLessThan" << recacheIfLessThan;
}

void MetadataCache::iconCleanup()
{
/*
When the icon targets (the rows that we want to load icons) change we need to clean up any
other rows that have icons previously loaded in order to minimize memory consumption. Rows
that have icons are tracked in the list iconsCached as the dm row (not dm->sf proxy).
*/
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    QMutableListIterator<int> i(iconsCached);
    QPixmap nullPm;
    mutex.lock();
    while (i.hasNext()) {
        i.next();
        int row = i.value();
        if (row < iconTargetStart || row > iconTargetEnd) {
            i.remove();
            dm->itemFromIndex(dm->index(row, 0))->setIcon(nullPm);
        }
    }
    mutex.unlock();
}

void MetadataCache::readAllMetadata()
{
/*
Load the thumb (icon) for all the image files in the .
*/
    {
    #ifdef ISDEBUG
    mutex.lock(); G::track(__FUNCTION__); mutex.unlock();
    #endif
    }
    G::t.restart();
    int count = 0;
    for (int row = 0; row < dm->rowCount(); ++row) {
        // is metadata already cached
        if (!dm->index(row, G::CreatedColumn).data().isNull()) continue;

        QString fPath = dm->index(row, 0).data(G::PathRole).toString();
        QFileInfo fileInfo(fPath);
        if (metadata->loadImageMetadata(fileInfo, true, true, false, true)) {
            metadata->imageMetadata.row = row;
            dm->addMetadataItem(metadata->imageMetadata, isShowCacheStatus);
            count++;
        }
    }
    G::allMetadataLoaded = true;
    qint64 ms = G::t.elapsed();
    qreal msperfile = (float)ms / count;
    qDebug() << "MetadataCache::readAllMetadata for" << count << "files" << ms << "ms" << msperfile << "ms per file;";
}

void MetadataCache::readIconChunk()
{
/*
Load the thumb (icon) for all the image files in the .
*/
    {
    #ifdef ISDEBUG
    mutex.lock(); G::track(__FUNCTION__); mutex.unlock();
    #endif
    }
    for (row = iconTargetStart; row <= iconTargetStart; ++row) {
        if (abort) {
            emit updateIsRunning(false, true, __FUNCTION__);
            return;
        }

        // load icon
        mutex.lock();
        idx = dm->sf->index(row, 0);
        int dmRow = dm->sf->mapToSource(idx).row();
        QString fPath = idx.data(G::PathRole).toString();
        if (idx.data(Qt::DecorationRole).isNull()) {
            QImage image;
            bool thumbLoaded = thumb->loadThumb(fPath, image);
            if (thumbLoaded) {
                emit setIcon(dmRow, image.scaled(G::maxIconSize, G::maxIconSize, Qt::KeepAspectRatio));
                iconsCached.append(dmRow);
            }
        }
        mutex.unlock();
    }
}

void MetadataCache::readMetadataIconChunk()
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

//    if (G::allMetadataLoaded) return;

    if (metadataCacheAll) {
        startRow = 0;
        endRow = dm->rowCount();
    }

    for (row = startRow; row < endRow; ++row) {
//    for (int row = startRow; row < dm->rowCount(); ++row) {
        if (abort) {
            emit updateIsRunning(false, true, __FUNCTION__);
            return;
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

        // skip loading icon?
        if (!iconsCacheAll) {
            if (row < iconTargetStart) continue;
            if (row > iconTargetEnd) continue;
        }

/*      Debugging stuff
        QStandardItem *item = new QStandardItem;
        item = dm->itemFromIndex(idx);

        idx.data(Qt::DecorationRole) = QVariant(QIcon, QIcon(availableSizes[normal,Off]=(QSize(256, 144)),cacheKey=0x15000000000))
        qDebug() << "MetadataCache::loadMetadataIconChunk  "
                 << "row =" << row
                 << "dmRow =" << dmRow
                 << "iconTargetStart =" << iconTargetStart
                 << "iconTargetEnd =" << iconTargetEnd
                 << "idx.data(Qt::DecorationRole) =" << idx.data(Qt::DecorationRole);

                 << "item->icon() =" << item
                 << fPath;
                 */

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
//    qint64 ms = t.elapsed();
//    qreal msperfile = (float)ms / count;
//    qDebug() << "MetadataCache::loadMetadataIconChunk for" << count << "files" << ms << "ms" << msperfile << "ms per file;";
    return;
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

        // pause image caching if it was running
        if (imageCacheThread->isRunning()) imageCacheThread->pauseImageCache();

        // read all metadata but no icons
        if (action == Action::AllMetadata) readAllMetadata();

        // read a chunk of icons
        if (action == Action::IconChunk) readIconChunk();

        // read next metadata and icon chunk
        if (action == Action::MetaIconChunk) readMetadataIconChunk();

        // read metadata and icons in a new folder
        if (action == Action::NewFolder) readMetadataIconChunk();

        if (abort) {
            qDebug() << "!!!!  Aborting MetadataCache::run  "
                     << "row =" << row;
            return;
        }

        // check if all metadata and thumbs have been loaded
        if (!G::allMetadataLoaded) {
            G::allMetadataLoaded = true;
            mutex.lock();
            for(int i = 0; i < rowCount; ++i) {
                if (dm->sf->index(i, G::CreatedColumn).data().isNull()) {
                    G::allMetadataLoaded = false;
                    break;
                }
            }
            mutex.unlock();
        }

        if (action == Action::MetaIconChunk || action == Action::IconChunk) {
            iconCleanup();
            // resume image caching if it was interrupted
            imageCacheThread->resumeImageCache();
        }

        if (action == Action::NewFolder) {
            qDebug() << "if (action == Action::NewFolder)";
            /* Only get bestFit on the first cache because the
            QListView rescrolls whenever a change to sizehint occurs */
            emit updateIconBestFit();
            // scroll to first image
            emit selectFirst();
            emit loadImageCache();
        }

        // update status of metadataThreadRunningLabel in statusbar
        emit updateIsRunning(false, true, __FUNCTION__);

    }

    /* After loading metadata it is okay to cache full size images, where the
    target cache needs to know how big each image is (width, height) and the
    offset to embedded full size jpgs */



//    if (action == Action::Resume && imageCacheWasRunning)
//        imageCacheThread->resumeImageCache();

//    if (!imageCacheThread->cacheUpToDate()) {
//        qDebug() << "Resuming image caching";
//        imageCacheThread->resumeImageCache();
//    }

}
