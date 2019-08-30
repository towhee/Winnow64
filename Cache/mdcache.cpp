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

          startRow    firstVisibleRow     lastVisibleRow
0         |           |                   |          endRow                      dm->rowCount()
|         |           |                   |          |                           |
v         v           v                   v          v                           v
----------xxxxxxxxxxxxvvvvvvvvvvvvvvvvvvvvvxxxxxxxxxxx----------------------------

- = all items in datamodel
x = icon cache range
v = visible icons

To keep it simple and prevent several threads trying to access the same image file at the same
time the following order of events occurs when Winnow is started or a new folder is selected:

    • Winnow is started and an instance of MetadataCache is created (metadataCacheThread). An
    instance of ImageCache also created (imageCacheThread).

    • The user selects a folder or a previous session folder is selected if set in
    preferences. MW::folderSelectionChange starts MetadataCache::loadNewFolder.

    • MetadataCache::loadNewFolder starts metadataCacheThread and metadataCacheThread::run
    reads a chunk of image files, loading the metadata and icons to the datamodel. The number
    of icons required to fill the grid or thumb views is not known until the image previews
    have been read and the best aspect is determined. A second pass is required.

    • MetadataCache::loadNewFolder2ndPass completes processing metadata and icons required in
    the new folder.

    • After the chunk is loaded metadataCacheThread emits a signal to start imageCacheThread.

    *** The new chunk of metadata is added to the datamodel filters.

When Winnow is running and a new folder is selected

    • A new folder is selected and MW::folderSelectionChange executes, which in turn, starts
      metadataCacheThread::loadNewFolder.

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

    • a signal is emitted and imageCacheThread->updateImageCachePosition is called and the
    image cache is updated.

    • Load ImageView either from the cache or directly from the file if not cached.

Thread management

    Thread collisions and preformance degradation occur if metadataCacheThread and
    imageCacheThread run concurrently. The metadata cache thread is initiated from MW and the
    image cache thread is always initiated from this thread - metadataCacheThread.

    Both threads are initiated from singleShot timers.  If file selection is changing rapidly
    (user holding down arrow key) or rapid scrolling is occurring then the timer prevents
    cache updates until a pause occurs.

    Timers cannot be initiated in another thread, so MW must be signaled to execute a 2nd pass
    of the metadata cache for a new folder and when the image cache is initiated.

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
    abort = false;

    // list to make debugging easier
    actionList << "NewFolder"
               << "NewFolder2ndPass"
                << "NewFileSelected"
               << "Scroll"
               << "SizeChange"
               << "AllMetadata";

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
//        qDebug() << "abort = true : " << __FUNCTION__;
        condition.wakeOne();
        mutex.unlock();
        wait();
        abort = false;
        emit updateIsRunning(false, false, __FUNCTION__);
    }
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
        if (!dm->index(i, G::MetadataLoadedColumn).data().toBool()) {
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

void MetadataCache::loadNewFolder()
{
/*
This function is called from MW::folderSelectionChange and will not have any filtering so
we can use the datamodel dm directly. The greater of:

    - the number of visible cells in the gridView or
    - maxChunkSize

metadata and icons are loaded into the datamodel. The number of visible cells are not
known yet because IconView::bestAspect has not been determined.

MetadataCache::loadNewFolder2ndPass is executed immediately after this function.
*/
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    if (isRunning()) {
        mutex.lock();
        qDebug() << "abort = true : " << __FUNCTION__;
        abort = true;
        condition.wakeOne();
        mutex.unlock();
        wait();
    }
    abort = false;
    G::allMetadataLoaded = false;
    iconsCached.clear();
    foundItemsToLoad = true;
//    metadataChunkSize = defaultMetadataChunkSize;
    startRow = 0;
    endRow = metadataChunkSize;
    int rowCount = dm->sf->rowCount();
    if (endRow > rowCount) endRow = rowCount;
    // reset for bestAspect calc
    G::iconWMax = G::minIconSize;
    G::iconHMax = G::minIconSize;
    action = Action::NewFolder;
    start(TimeCriticalPriority);
}

void MetadataCache::loadNewFolder2ndPass()
{
/*
This function is initiated after MetadataCache::loadNewFolder has called run.  A signal is
emitted back to MW, which in turn calculates the best aspect and number of cells visible in
the gridview, based on the metadata read in the first pass.

The greater of:
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
        qDebug() << "abort = true : " << __FUNCTION__;
        condition.wakeOne();
        mutex.unlock();
        wait();
    }
    abort = false;
    action = Action::NewFolder2ndPass;
    setRange();
    foundItemsToLoad = false;
    for (int i = startRow; i < endRow; ++i) {
        if (dm->sf->index(i, G::PathColumn).data(Qt::DecorationRole).isNull())
            foundItemsToLoad = true;
        if (!dm->sf->index(i, G::MetadataLoadedColumn).data().toBool())
            foundItemsToLoad = true;
        if (foundItemsToLoad) break;
    }
    start(TimeCriticalPriority);
}

void MetadataCache::loadAllMetadata()
{
/*
Using dm::addAllMetadata instead (see why below)

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
        qDebug() << "abort = true : " << __FUNCTION__;
        condition.wakeOne();
        mutex.unlock();
        wait();
    }
    abort = false;
    startRow = 0;
    endRow = dm->sf->rowCount();
    action = Action::AllMetadata;
    start(TimeCriticalPriority);
}

void MetadataCache::scrollChange()
{
/*
This function is called when there is a scroll event in a view of the datamodel.

A chunk of metadata and icons are added to the datamodel and icons outside the caching
limits are removed (not visible and not with chunk range)
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
    action = Action::Scroll;
    foundItemsToLoad = false;
    setRange();
    for (int i = startRow; i < endRow; ++i) {
        if (dm->sf->index(i, G::PathColumn).data(Qt::DecorationRole).isNull())
            foundItemsToLoad = true;
        if (!dm->sf->index(i, G::MetadataLoadedColumn).data().toBool())
            foundItemsToLoad = true;
        if (foundItemsToLoad) break;
    }
    if (foundItemsToLoad) start(TimeCriticalPriority);
}

void MetadataCache::sizeChange()
{
/*
This function is called when the number of icons visible in the viewport changes when the icon
size or the viewport change size.
*/
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    if (isRunning()) {
        mutex.lock();
        abort = true;
        qDebug() << "abort = true : " << __FUNCTION__;
        condition.wakeOne();
        mutex.unlock();
        wait();
    }
    abort = false;
    action = Action::Resize;
    foundItemsToLoad = false;
    setRange();
    for (int i = startRow; i < endRow; ++i) {
        if (dm->sf->index(i, G::PathColumn).data(Qt::DecorationRole).isNull())
            foundItemsToLoad = true;
        if (!dm->sf->index(i, G::MetadataLoadedColumn).data().toBool())
            foundItemsToLoad = true;
        if (foundItemsToLoad) break;
    }
    start(TimeCriticalPriority);
}

void MetadataCache::fileSelectionChange(bool okayToImageCache)
{
/*
This function is called from MW::fileSelectionChange. A chunk of metadata and icons are
added to the datamodel. The image cache is updated.
*/
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    if (isRunning()) {
        mutex.lock();
        abort = true;
        qDebug() << "abort = true : " << __FUNCTION__;
        condition.wakeOne();
        mutex.unlock();
        wait();
    }
    abort = false;
    updateImageCache = okayToImageCache;
    action = Action::NewFileSelected;
    foundItemsToLoad = false;
    setRange();
    for (int i = startRow; i < endRow; ++i) {
        if (dm->sf->index(i, G::PathColumn).data(Qt::DecorationRole).isNull())
            foundItemsToLoad = true;
        if (!dm->sf->index(i, G::MetadataLoadedColumn).data().toBool())
            foundItemsToLoad = true;
        if (foundItemsToLoad) break;
    }
    start(TimeCriticalPriority);
}

void MetadataCache::setRange()
{
/*
Define the range of icons to cache: prev + current + next viewports/pages of icons
*/
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    int rowCount = dm->sf->rowCount();

    // default total per page (prev, curr and next pages)
    int dtpp = metadataChunkSize / 3;

    // total per page (tpp)
    tpp = visibleIcons;
    if (dtpp > tpp) tpp = dtpp;

    // if icons visible greater than chunk size then increase chunk size
//    if (tpp > metadataChunkSize) metadataChunkSize = tpp;

    // first to cache (startRow)
    startRow = firstIconVisible - tpp;
    if (startRow < 0) startRow = 0;

    // last to cache (endRow)
    endRow = lastIconVisible + tpp;
    if (endRow - startRow < metadataChunkSize) endRow = startRow + metadataChunkSize;
    if (endRow > rowCount) endRow = rowCount;

    prevFirstIconVisible = firstIconVisible;
    prevLastIconVisible = lastIconVisible;

//    G::track(__FUNCTION__);
/*    qDebug()  <<  __FUNCTION__
              << "source =" << actionList.at(action)
              << "first =" << firstIconVisible
              << "mid =" << midIconVisible
              << "last =" << lastIconVisible
              << "visibleIcons =" << visibleIcons
              << "tpp =" << tpp
              << "metadataChunkSize =" << metadataChunkSize
              << "startRow =" << startRow
              << "endRow =" << endRow;
*/
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
//    mutex.lock();
    while (i.hasNext()) {
        if (abort) return;
        i.next();
        // the datamodel row dmRow
        int dmRow = i.value();
        // the filtered proxy row sfRow
        int sfRow = dm->sf->mapFromSource(dm->index(dmRow, 0)).row();
        /* mapFromSource returns -1 if dm->index(dmRow, 0) is not in the filtered dataset
        This can happen is the user switches folders and the datamodel is cleared before the
        thread abort is processed.
        */
        if (sfRow == -1) return;

        // remove all loaded icons outside target range
        if (sfRow < startRow || sfRow > endRow) {
            i.remove();
            dm->itemFromIndex(dm->index(dmRow, 0))->setIcon(nullPm);
//            qDebug() << __FUNCTION__ << actionList[action]
//                     << "Removing icon for row" << sfRow;
        }
    }
//    mutex.unlock();
}

void MetadataCache::iconMax(QPixmap &thumb)
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    if (G::iconWMax == G::maxIconSize && G::iconHMax == G::maxIconSize) return;

    // for best aspect calc
    int w = thumb.width();
    int h = thumb.height();
    if (w > G::iconWMax) G::iconWMax = w;
    if (h > G::iconHMax) G::iconHMax = h;
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
        if (dm->index(row, G::MetadataLoadedColumn).data().toBool()) continue;

        QString fPath = dm->index(row, 0).data(G::PathRole).toString();
        QFileInfo fileInfo(fPath);
        if (metadata->loadImageMetadata(fileInfo, true, true, false, true, __FUNCTION__)) {
            metadata->imageMetadata.row = row;
            dm->addMetadataForItem(metadata->imageMetadata);
            count++;
        }
    }
    G::allMetadataLoaded = true;
//    qint64 ms = G::t.elapsed();
//    qreal msperfile = (float)ms / count;
//    qDebug() << "MetadataCache::readAllMetadata for" << count << "files" << ms << "ms" << msperfile << "ms per file;";
}

void MetadataCache::readMetadataIcon(const QModelIndex &idx)
{
    {
    #ifdef ISDEBUG
//    mutex.lock(); G::track(__FUNCTION__); mutex.unlock();
    #endif
    }
    int sfRow = idx.row();
    int dmRow = dm->sf->mapToSource(idx).row();
    QString fPath = idx.data(G::PathRole).toString();

    if (!dm->sf->index(sfRow, G::MetadataLoadedColumn).data().toBool()) {
        QFileInfo fileInfo(fPath);
        if (metadata->loadImageMetadata(fileInfo, true, true, false, true, __FUNCTION__)) {
            metadata->imageMetadata.row = dmRow;
            dm->addMetadataForItem(metadata->imageMetadata);
        }
    }

    // load icon
    if (idx.data(Qt::DecorationRole).isNull()) {
        QImage image;
//        qDebug() << __FUNCTION__ << "row =" << sfRow << fPath;
        bool thumbLoaded = thumb->loadThumb(fPath, image);
        if (thumbLoaded) {
            QPixmap pm = QPixmap::fromImage(image.scaled(G::maxIconSize, G::maxIconSize, Qt::KeepAspectRatio));
            dm->itemFromIndex(dm->index(dmRow, 0))->setIcon(pm);
            iconMax(pm);
            iconsCached.append(dmRow);
        }
    }
}

void MetadataCache::readIconChunk()
{
/*
Load the thumb (icon) for all the image files in the target range.  This is called after a
sort/filter change and all metadata has been loaded, but the icons visible havew changed.
*/
    {
    #ifdef ISDEBUG
    mutex.lock(); G::track(__FUNCTION__); mutex.unlock();
    #endif
    }

    int start = startRow;
    int end = endRow;
        if (cacheAllIcons) {
        start = 0;
        end = dm->sf->rowCount();
    }
//    qDebug() << __FUNCTION__ << "start =" << start << "end =" << end
//             << "rowCount =" << dm->sf->rowCount();
    for (int row = start; row < end; ++row) {
        if (abort) {
            emit updateIsRunning(false, true, __FUNCTION__);
            return;
        }

        // load icon
        mutex.lock();
        QModelIndex idx = dm->sf->index(row, 0);
        if (idx.data(Qt::DecorationRole).isNull()) {
            int dmRow = dm->sf->mapToSource(idx).row();
            QImage image;
            QString fPath = idx.data(G::PathRole).toString();
//            qDebug() << __FUNCTION__ << "row =" << row << fPath;
            bool thumbLoaded = thumb->loadThumb(fPath, image);
            QPixmap pm;
            if (thumbLoaded) {
                pm = QPixmap::fromImage(image.scaled(G::maxIconSize, G::maxIconSize, Qt::KeepAspectRatio));
                dm->itemFromIndex(dm->index(dmRow, 0))->setIcon(pm);
                iconMax(pm);
                iconsCached.append(dmRow);
            }
//            else {
//                pm = QPixmap(":/images/badImage1.png");
//            }
//            dm->itemFromIndex(dm->index(dmRow, 0))->setIcon(pm);
//            iconMax(pm);
//            iconsCached.append(dmRow);
//            qDebug() << __FUNCTION__ << "loaded icon for row" << row;
        }
        mutex.unlock();
//        qApp->processEvents();
    }
    // reset after a filter change
//    emit updateIconBestFit();
}

void MetadataCache::readMetadataChunk()
{
/*
Load the thumb (icon) for all the image files in the target range.  This is called after a
sort/filter change and all metadata has been loaded, but the icons visible havew changed.
*/
    {
    #ifdef ISDEBUG
    mutex.lock(); G::track(__FUNCTION__); mutex.unlock();
    #endif
    }

    int start = startRow;
    int end = endRow;
    if (cacheAllMetadata) {
        start = 0;
        end = dm->sf->rowCount();
    }
    for (int row = start; row < end; ++row) {
        if (abort) {
//            qDebug() << __FUNCTION__ << "Aborting on row" << row;
            emit updateIsRunning(false, true, __FUNCTION__);
            return;
        }
/*        qDebug() << __FUNCTION__ << "startRow =" << startRow
                 << "endRow =" << endRow
                 << "row =" << row;
*/
        // file path and dm source row in case filtered or sorted
        mutex.lock();
        QModelIndex idx = dm->sf->index(row, 0);
        int dmRow = dm->sf->mapToSource(idx).row();
        QString fPath = idx.data(G::PathRole).toString();

        // load metadata
        if (!dm->sf->index(row, G::MetadataLoadedColumn).data().toBool()) {
            QFileInfo fileInfo(fPath);

            // only read metadata from files that we know how to
            QString ext = fileInfo.suffix().toLower();
            if (metadata->getMetadataFormats.contains(ext)) {
//                qDebug() << __FUNCTION__ << fPath << ext;
                if (metadata->loadImageMetadata(fileInfo, true, true, false, true, __FUNCTION__)) {
                    metadata->imageMetadata.row = dmRow;
                    dm->addMetadataForItem(metadata->imageMetadata);
                }
                else {
                    qDebug() << __FUNCTION__ << "Failed to load metadata for" << fPath;
                }
            }
        }
        mutex.unlock();
//        qApp->processEvents();
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
    int count = 0;
    bool allRead = false;
    while (!allRead && count < 1) {
        allRead = true;
        if (cacheAllMetadata) endRow = dm->sf->rowCount();
        for (int row = startRow; row < endRow; ++row) {
            if (abort) {
                emit updateIsRunning(false, true, __FUNCTION__);
                return;
            }

            // file path and dm source row in case filtered or sorted
            mutex.lock();
            QModelIndex idx = dm->sf->index(row, 0);
            int dmRow = dm->sf->mapToSource(idx).row();
            QString fPath = idx.data(G::PathRole).toString();

            // load metadata
            if (!dm->sf->index(row, G::MetadataLoadedColumn).data().toBool()) {
                QFileInfo fileInfo(fPath);
                /*
                   tried emit signal to metadata but really slow
                   emit loadImageMetadata(fileInfo, true, true, false);  */
                if (metadata->loadImageMetadata(fileInfo, true, true, false, true, __FUNCTION__)) {
                    metadata->imageMetadata.row = dmRow;
                    dm->addMetadataForItem(metadata->imageMetadata);
                }
                else {
                    allRead = false;
//                    qDebug() << "XXX " << __FUNCTION__ << "Failed to load metadata for " << fPath;
                }
            }

//            if (!cacheAllIcons && row > metadataChunkSize) continue;

            // load icon
            if (idx.data(Qt::DecorationRole).isNull()) {
                QImage image;
//                qDebug() << __FUNCTION__ << "row =" << row << fPath;
                bool thumbLoaded = thumb->loadThumb(fPath, image);
                if (thumbLoaded) {
                    QPixmap pm = QPixmap::fromImage(image.scaled(G::maxIconSize, G::maxIconSize, Qt::KeepAspectRatio));
                    dm->itemFromIndex(dm->index(dmRow, 0))->setIcon(pm);
                    iconMax(pm);
                    iconsCached.append(dmRow);
                }
                else {
                    allRead = false;
//                    qDebug() << "XXX " << __FUNCTION__ << "Failed to load icon for " << fPath;
                }
            }
            mutex.unlock();
        }
        count++;

        // confirm all metadata and icons read.  If not try again
/*        allRead = true;
        for (int row = startRow; row < endRow; ++row) {
            if (dm->sf->index(row, G::CreatedColumn).data().isNull()) allRead = false;
            if (dm->sf->index(row, 0).data(Qt::DecorationRole).isNull()) allRead = false;
        }*/
    }
    /*
    qint64 ms = t.elapsed();
    qreal msperfile = (float)ms / count;
    qDebug() << "MetadataCache::loadMetadataIconChunk for" << count << "files" << ms << "ms" << msperfile << "ms per file;";
    */
    return;
}

void MetadataCache::run()
{
/*
Read the metadata and icons for a range of all the images, depending on action into the
datamodel dm.

If there has been a file selection change and not a new folder then update image cache.
*/
    {
    #ifdef ISDEBUG
    mutex.lock(); G::track(__FUNCTION__); mutex.unlock();
    #endif
    }
    if (foundItemsToLoad) {
        emit updateIsRunning(true, true, __FUNCTION__);
        qApp->processEvents();

        mutex.lock();
        int rowCount = dm->rowCount();
        mutex.unlock();

        // pause image caching if it was running
        bool imageCachePaused = false;
        if (imageCacheThread->isRunning()) {
            imageCacheThread->pauseImageCache();
            imageCacheThread->wait();
            imageCachePaused = true;
        }

        // read all metadata but no icons
        if (action == Action::AllMetadata) readAllMetadata();

        // read next metadata and icon chunk
        if (action == Action::NewFileSelected ||
            action == Action::Scroll ||
            action == Action::Resize ||
            action == Action::NewFolder ||
            Action::NewFolder2ndPass)
            if (!G::allMetadataLoaded) readMetadataChunk();
            readIconChunk();
//        readMetadataIconChunk();  // deprecated so can have diff ranges for meta and icons

        if (abort) {
//            mutex.unlock();
//            qDebug() << "!!!!  Aborting MetadataCache::run  ";
            return;
        }

        // update allMetadataLoaded flag
        if (!G::allMetadataLoaded) {
            G::allMetadataLoaded = true;
            mutex.lock();
            for (int i = 0; i < rowCount; ++i) {
                if (!dm->sf->index(i, G::MetadataLoadedColumn).data().toBool()) {
                    G::allMetadataLoaded = false;
                    break;
                }
            }
            mutex.unlock();
        }

        // clean up orphaned icons outside icon range   rgh what about other actions
        if (!cacheAllIcons) {
            if (action == Action::NewFileSelected || action == Action::Scroll)  {
                iconCleanup();
                if (abort) return;
            }
        }

        if (action == Action::NewFolder) {
            emit selectFirst();
            /* make a second pass if more than 250 thumbs visible in gridView or thumbView and
               so can calc best aspect */
            emit loadMetadataCache2ndPass();
        }

        // update status of metadataThreadRunningLabel in statusbar
        emit updateIsRunning(false, true, __FUNCTION__);

        // resume image caching if it was interrupted
        if (imageCachePaused) imageCacheThread->resumeImageCache();
    }
    /*
    qDebug() << "action =" << action
             << "G::isNewFolderLoaded =" << G::isNewFolderLoaded
             << "dm->currentRow" << dm->currentRow
             << "previousRow" << previousRow; */

    // after 2nd pass on new folder initiate the image cache
    if (action == Action::NewFolder2ndPass) {
        emit loadImageCache();
    }
 
    // if a file selection change and not a new folder then update image cache
    if (action == Action::NewFileSelected) {
        /*
        qDebug() << __FUNCTION__ << "fileSelectionChange"
                 << "dm->currentFilePath" << dm->currentFilePath
                 << "dm->currentRow" << dm->currentRow
                 << "previousRow" << previousRow
                 << "G::isNewFolderLoaded" << G::isNewFolderLoaded;
                 */
        if (G::isNewFolderLoaded && updateImageCache) {
            emit updateImageCachePositionAfterDelay();
        }
    }
}
