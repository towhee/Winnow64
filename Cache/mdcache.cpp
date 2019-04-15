#include "Main/global.h"
#include "Cache/mdcache.h"

/*
The metadata cache reads the relevant metadata and preview thumbnails from the image files and
stores this information in the datamodel dm. The critical metadata is the offset and length of
the embedded JPGs and their width and height, which is required by the ImageCache and
ImageView to display the images and IconView to display the thumbnails. The other metadata is
in the nice-to-know category.  The full size images are stored in the image cache.

Reading the metadata can become time consuming and the thumbnails or icons can consume a lot
of memory, so the metadata is read in chunks. Each chunk size (number of image files) is the
greater of the default value of maxChunkSize or the number of thumbs visible in the GridView
viewport.

To keep it simple and minimize several threads trying to access the same image file at the same
time the following order of events occurs when Winnow is started or a new folder is selected:

    • Winnow is started and an instance of MetadataCache is created (metadataCacheThread). An
      instance of ImageCache also created (imageCacheThread).

    • After the show event, the IconView first/last/thumbsPerPage is determined and
      metadataCacheThread::loadNewMetadataCache runs.

    • MW::loadNewMetadataCache starts metadataCacheThread and metadataCacheThread::run reads a
      chunk of image files, loading the metadata and icons to the datamodel.

    • After the chunk is loaded metadataCacheThread emits a signal to start imageCacheThread.

    *** The new chunk of metadata is added to the datamodel filters.

When Winnow is running and a new folder is selected

    • A new folder is selected and MW::folderSelectionChange executes, which in turn, starts
      metadataCacheThread::loadNewMetadataCache.

    • loadNewMetadataCache starts metadataCacheThread::run, which reads a chunk of image
      files, loading the metadata and icons to the datamodel.

    • After the chunk is loaded metadataCacheThread emits a signal to start imageCacheThread.

When the user scrolls through the thumbnails:

    • If it is running the imageCacheThread is paused.

    • A 100ms (MW::cacheDelay) singleShot timer is invoked to call MW::loadMetadataChunk. The
      delay allows rapid scrolling without firing the metadataCacheThread until the user
      pauses.

    • As above, the metadata chunk is loaded and the datamodel filter is updated.

    • If it was running the imageCacheThread is resumed. However, image caching is not
      signalled to start (only resume).

When the user selects a thumbnail or a filter or sort has been invoked.

    • MW::fileSelectionChange is called.

    • loadMetadataIconChunk is called and any addtional metadata or icons are loaded.

    • imageCacheThread->updateImageCachePosition is called and the image cache is updated.

    • Load ImageView either from the cache or directly from the file if not cached.

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
    thumb = new Thumb(this, dm, metadata);
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
can use the datamodel dm directly. The greater of:
 - the number of visible cells in the gridView or
 - maxChunkSize
metadata and icons are loaded into the datamodel.
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
    abort = false;
    G::allMetadataLoaded = false;
    iconsCached.clear();
    foundItemsToLoad = true;
    currentRow = 0;
    tpp = thumbsPerPage;

//    int imageCount = dm->rowCount();
//    folderPath = dm->currentFolderPath;
//    this->startRow = 0;
//    if (metadataCacheAll || imageCount < metadataCacheAllIfLessThan)
//        this->endRow = imageCount;
//    else {
//        int segmentSize = thumbsPerPage > metadataChunkSize ? thumbsPerPage : metadataChunkSize;
//        this->endRow = startRow + segmentSize;
//        if (this->endRow >= dm->rowCount()) this->endRow = dm->rowCount();
//    }
//    setIconTargets(startRow , thumbsPerPage);

    setRange();
    action = Action::NewFolder;
    start(TimeCriticalPriority);
}

void MetadataCache::loadAllMetadata()
{
/*
All the metadata if loaded into the datamodel.  This is called prior to filtering or sorting
the datamodel.  This is duplicated in the datamodel, where is can be run in the main thread,
allowing for real time updates to the progress bar.

Loading all the metadata in a separate high priority thread is slightly faster, but if the
progress bar update is more important then use the datamodel function dm::addAllMetadata.
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
    abort = false;
    startRow = 0;
    endRow = dm->rowCount();
    action = Action::AllMetadata;
    start(TimeCriticalPriority);
}

void MetadataCache::loadMetadataIconChunk(int row, int thumbsPerPage)
{
/*
This function is called from MW::fileSelectionChange, MW::filterChange and resize and scroll
events in gridView and thumbView.  A chunk of metadata and icons are added to the datamodel.
If there has been a file selection change then the image cache is updated.
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
    abort = false;

    currentRow = row;
    tpp = thumbsPerPage;

    setRange();

    //    int chunkSize = thumbsPerPage > metadataChunkSize ? thumbsPerPage : metadataChunkSize;


//    setIconTargets(startRow , thumbsPerPage);

    foundItemsToLoad = false;
    for (int i = startRow; i < endRow; ++i) {
        if (dm->sf->index(i, G::PathColumn).data(Qt::DecorationRole).isNull())
            foundItemsToLoad = true;
        if (dm->sf->index(i, G::CreatedColumn).data().isNull())
            foundItemsToLoad = true;
        if (foundItemsToLoad) break;
    }

    action = Action::MetaIconChunk;
    start(TimeCriticalPriority);
}

void MetadataCache::loadIconChunk(int fromRow, int thumbsPerPage)
{
/* Not used at present */
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

void MetadataCache::setRange()
{
    int rowCount = dm->sf->rowCount();
    // previous, current and next page = chunk
    int chunk = tpp * 3;
    chunk = 250;
    startRow = currentRow - chunk / 2;
    if (startRow < 0) startRow = 0;
    endRow = currentRow + chunk / 2;
    if (endRow > rowCount) endRow = rowCount;

    qDebug() << __FUNCTION__
             << "Current row" << currentRow
             << "thumbsPerPage" << tpp
             << "Chunk size" << chunk
             << "Start row" << startRow
             << "End row" << endRow;
}

void MetadataCache::setIconTargets(int start, int thumbsPerPage)
{
/*
Icon target range is the rows where we want to see an icon (thumbnail). For example, when the
user scrolls through the gridView, the icons visible in the gridView need to be loaded. The
targets are assigned so we can cache the icons in advance. The number of icons visible in the
gridView or thumbView (whichever is greater) is thumbsPerPage. The strategy is to cache n
pages of icons before and after the current page, where n = iconPagesToCacheAhead.

In addition, the thresholds recacheIfGreaterThan and recacheIfLessThan prevent caching
operations while the current item selected is in the middle 2/3 of the target range to prevent
inefficient caching.
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

    if (iconTargetStart > 0) recacheIfLessThan = iconTargetStart + targetSize / 2 - targetSize / 7;
    else recacheIfLessThan = 0;
    if (iconTargetEnd < rowCount) recacheIfGreaterThan = startRow + targetSize / 2 + targetSize / 7;
    else recacheIfGreaterThan = rowCount;

    qDebug() << __FUNCTION__
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
Load the thumb (icon) for all the image files in the folder(s).
*/
    {
    #ifdef ISDEBUG
    mutex.lock(); G::track(__FUNCTION__); mutex.unlock();
    #endif
    }
//    G::t.restart();
    isShowCacheStatus = true;
    int count = 0;
    int rows = dm->rowCount();
    for (int row = 0; row < rows; ++row) {
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
//    qint64 ms = G::t.elapsed();
//    qreal msperfile = (float)ms / count;
//    qDebug() << "MetadataCache::readAllMetadata for" << count << "files" << ms << "ms" << msperfile << "ms per file;";
}

void MetadataCache::readIconChunk()
{
/*
Load the thumb (icon) for all the image files in the icon target range.
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
Load the metadata and thumb (icon) for all the image files in the chunk range defined by
startRow and endRow.
*/
    {
    #ifdef ISDEBUG
    mutex.lock(); G::track(__FUNCTION__); mutex.unlock();
    #endif
    }
//    t.restart();
//    G::t.restart();
    int count = 0;

    if (metadataCacheAll) {
        startRow = 0;
        endRow = dm->rowCount();
    }

    for (row = startRow; row < endRow; ++row) {
        if (abort) {
            emit updateIsRunning(false, true, __FUNCTION__);
            return;
        }

        // file path and dm source row in case filtered or sorted
        mutex.lock();
        idx = dm->sf->index(row, 0);
        int dmRow = dm->sf->mapToSource(idx).row();
        QString fPath = idx.data(G::PathRole).toString();

        // load metadata
        if (dm->sf->index(row, G::CreatedColumn).data().isNull()) {
            QFileInfo fileInfo(fPath);
            /*
               tried emit signal to metadata but really slow
               emit loadImageMetadata(fileInfo, true, true, false);  */
            if (metadata->loadImageMetadata(fileInfo, true, true, false, true)) {
                metadata->imageMetadata.row = dmRow;
                dm->addMetadataItem(metadata->imageMetadata);
            }
        }
        mutex.unlock();
        count++;

        // skip loading icon?
//        if (!iconsCacheAll) {
//            if (row < iconTargetStart) continue;
//            if (row > iconTargetEnd) continue;
//        }

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
Read the metadata and icons for a range of all the images, depending on action into the
datamodel dm.

action:
        NewFolder = 0
        MetaChunk = 1       (read only a range of metadata)
        IconChunk = 2       (read only a range of icons) * not used
        MetaIconChunk = 3   (read a range of metadata and icons)
        AllMetadata = 4     (read all the metadate but no icons)
        Resume = 5

If there has been a file selection change and not a new folder then update image cache.
*/
    {
    #ifdef ISDEBUG
    mutex.lock(); G::track(__FUNCTION__); mutex.unlock();
    #endif
    }
    qDebug() << __FUNCTION__;
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
        if (action == Action::MetaIconChunk) {
            readMetadataIconChunk();
            qDebug() << __FUNCTION__ << "Finished readMetadataIconChunk()";
        }

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

        // clean up orphaned icons outside icon range
        if (action == Action::MetaIconChunk || action == Action::IconChunk) {
//            iconCleanup();
            // resume image caching if it was interrupted
            imageCacheThread->resumeImageCache();
        }

        if (action == Action::NewFolder) {
            /* Only get bestFit on the first cache because the QListView rescrolls
            whenever a change to sizehint occurs */
            emit updateIconBestFit();
            // scroll to first image
            emit selectFirst();
            // start image caching (full size)
            emit loadImageCache();
        }

        // update status of metadataThreadRunningLabel in statusbar
        emit updateIsRunning(false, true, __FUNCTION__);
    }
    /*
    qDebug() << "action =" << action
             << "G::isNewFolderLoaded =" << G::isNewFolderLoaded
             << "dm->currentRow" << dm->currentRow
             << "previousRow" << previousRow; */
    // if a file selection change and not a new folder then update image cache
    if (action == Action::MetaIconChunk && dm->currentRow != previousRow && G::isNewFolderLoaded) {
        imageCacheThread->updateImageCachePosition(dm->currentFilePath);
    }
}
