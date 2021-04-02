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
    if (G::isLogger) G::log(__FUNCTION__); 
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
    if (G::isLogger) G::log(__FUNCTION__); 
//    qDebug() << __FUNCTION__;
    if (isRunning()) {
        mutex.lock();
        abort = true;
        condition.wakeOne();
        mutex.unlock();
        wait();
        abort = false;
        emit updateIsRunning(false, false, __FUNCTION__);
    }
}

bool MetadataCache::isAllMetadataLoaded()
{
    if (G::isLogger) G::log(__FUNCTION__); 
    qDebug() << __FUNCTION__;
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
    if (G::isLogger) G::log(__FUNCTION__); 
    // not used
    qDebug() << __FUNCTION__;
    bool loaded = true;
    for (int i = 0; i < dm->rowCount(); ++i) {
        if (dm->index(i, G::PathColumn).data(Qt::DecorationRole).isNull()) {
            loaded = false;
            break;
        }
    }
    return loaded;
}

void MetadataCache::loadNewFolder(bool isRefresh)
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
    if (G::isLogger) G::log(__FUNCTION__); 
    if (isRunning()) {
        mutex.lock();
        abort = true;
        condition.wakeOne();
        mutex.unlock();
        wait();
    }
//    qDebug() << "\n@@@" << __FUNCTION__;
    abort = false;
    G::allMetadataLoaded = false;
    isRefreshFolder = isRefresh;
    iconsCached.clear();
    foundItemsToLoad = true;
//    metadataChunkSize = defaultMetadataChunkSize;
    startRow = 0;
    int rowCount = dm->sf->rowCount();
    if (metadataChunkSize > rowCount) {
        endRow = rowCount;
        lastIconVisible = rowCount;
    }
    else {
        endRow = metadataChunkSize;
        lastIconVisible = metadataChunkSize;
    }
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
    if (G::isLogger) G::log(__FUNCTION__); 
    if (isRunning()) {
        mutex.lock();
        abort = true;
        condition.wakeOne();
        mutex.unlock();
        wait();
    }
//    qDebug() << "\n@@@"  << __FUNCTION__;
    abort = false;
    action = Action::NewFolder2ndPass;
    setRange();
    foundItemsToLoad = false;
    for (int i = startRow; i < endRow; ++i) {
        if (dm->sf->index(i, G::PathColumn).data(Qt::DecorationRole).isNull())
            foundItemsToLoad = true;
//        if (!dm->sf->index(i, G::IconLoadedColumn).data().toBool())
//            foundItemsToLoad = true;
        if (!dm->sf->index(i, G::MetadataLoadedColumn).data().toBool())
            foundItemsToLoad = true;
        if (foundItemsToLoad) break;
    }
//    qDebug() << "\n@@@"  << __FUNCTION__ << "foundItemsToLoad =" << foundItemsToLoad;
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
    if (G::isLogger) G::log(__FUNCTION__); 
    if (isRunning()) {
        mutex.lock();
        abort = true;
        condition.wakeOne();
        mutex.unlock();
        wait();
    }
//    qDebug() << "\n@@@"  << __FUNCTION__;
    abort = false;
    startRow = 0;
    endRow = dm->sf->rowCount();
    action = Action::AllMetadata;
    foundItemsToLoad = true;
    start(TimeCriticalPriority);
}

void MetadataCache::scrollChange(QString source)
{
/*
This function is called when there is a scroll event in a view of the datamodel.

A chunk of metadata and icons are added to the datamodel and icons outside the caching
limits are removed (not visible and not with chunk range)
*/
    if (G::isLogger) G::log(__FUNCTION__); 
    if (isRunning()) {
            mutex.lock();
            abort = true;
            condition.wakeOne();
            mutex.unlock();
            wait();
    }
    if (G::isInitializing) return;
//    qDebug() << "\n@@@"  << __FUNCTION__ << "called by =" << source;
    abort = false;
    action = Action::Scroll;
    setRange();
    foundItemsToLoad = false;
    for (int i = startRow; i < endRow; ++i) {
        if (dm->sf->index(i, G::PathColumn).data(Qt::DecorationRole).isNull())
            foundItemsToLoad = true;
//        if (!dm->sf->index(i, G::IconLoadedColumn).data().toBool())
//            foundItemsToLoad = true;
        if (!dm->sf->index(i, G::MetadataLoadedColumn).data().toBool())
            foundItemsToLoad = true;
        if (foundItemsToLoad) break;
    }
//    qDebug() << "\n@@@"  << __FUNCTION__ << "called by =" << source << "foundItemsToLoad =" << foundItemsToLoad;
    start(TimeCriticalPriority);
}

void MetadataCache::sizeChange(QString source)
{
/*
This function is called when the number of icons visible in the viewport changes when the icon
size or the viewport change size.
*/
    if (G::isLogger) G::log(__FUNCTION__); 
    if (isRunning()) {
        mutex.lock();
        abort = true;
        condition.wakeOne();
        mutex.unlock();
        wait();
    }
//    qDebug() << "\n@@@"  << __FUNCTION__ << "called by =" << source;
    abort = false;
    action = Action::Resize;
    foundItemsToLoad = false;
    setRange();
    for (int i = startRow; i < endRow; ++i) {
        if (dm->sf->index(i, G::PathColumn).data(Qt::DecorationRole).isNull())
            foundItemsToLoad = true;
//        if (!dm->sf->index(i, G::IconLoadedColumn).data().toBool())
//            foundItemsToLoad = true;
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
    if (G::isLogger) G::log(__FUNCTION__); 
//    if (isRunning()) {
//        mutex.lock();
//        abort = true;
//        condition.wakeOne();
//        mutex.unlock();
//        wait();
//    }
//    qDebug() << "\n@@@"  << __FUNCTION__;
//    abort = false;
//    updateImageCache = okayToImageCache;
//    action = Action::NewFileSelected;
    foundItemsToLoad = false;
    setRange();
    for (int i = startRow; i < endRow; ++i) {
        if (dm->sf->index(i, G::PathColumn).data(Qt::DecorationRole).isNull())
            foundItemsToLoad = true;
//        if (!dm->sf->index(i, G::IconLoadedColumn).data().toBool())
//            foundItemsToLoad = true;
        if (!dm->sf->index(i, G::MetadataLoadedColumn).data().toBool())
            foundItemsToLoad = true;
        if (foundItemsToLoad) break;
    }
    if (foundItemsToLoad) {
        if (isRunning()) {
            mutex.lock();
            abort = true;
            condition.wakeOne();
            mutex.unlock();
            wait();
        }
        abort = false;
        updateImageCache = okayToImageCache;
        action = Action::NewFileSelected;
        start(TimeCriticalPriority);
    }
    else {
        emit updateImageCachePosition();
    }
}

void MetadataCache::setRange()
{
/*
    Define the range of icons to cache: prev + current + next viewports/pages of icons
    Variables are set in MW::updateIconsVisible
*/
    if (G::isLogger) G::log(__FUNCTION__); 
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
    if (endRow >= rowCount) endRow = rowCount;

    prevFirstIconVisible = firstIconVisible;
    prevLastIconVisible = lastIconVisible;

    /*
    qDebug()  <<  __FUNCTION__
              << "source =" << actionList.at(action)
              << "firstIconVisible =" << firstIconVisible
              << "midIconVisible =" << midIconVisible
              << "lastIconVisible =" << lastIconVisible
              << "visibleIcons =" << visibleIcons
              << "tpp =" << tpp
              << "metadataChunkSize =" << metadataChunkSize
              << "rowCount =" << rowCount
              << "startRow =" << startRow
              << "endRow =" << endRow;
//            */
}

void MetadataCache::iconCleanup()
{
/*
    When the icon targets (the rows that we want to load icons) change we need to clean up any
    other rows that have icons previously loaded in order to minimize memory consumption. Rows
    that have icons are tracked in the list iconsCached as the dm row (not dm->sf proxy).
*/
    if (G::isLogger) G::log(__FUNCTION__); 
    QMutableListIterator<int> i(iconsCached);
    QPixmap nullPm;
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
}

qint32 MetadataCache::memRequired()
{
/*
    Calculate the datamodel dm memory required. On the average, each row requires 16000 bytes
    to store the metadata excluding the icons. The icons are only stored for the part of the
    datamodel about to be viewed. Icon memory = w*h*3 bytes. Return mem reqd in MB.
*/
    if (G::isLogger) G::log(__FUNCTION__); 
//    mutex.lock(); qDebug() << __FUNCTION__; mutex.unlock();
    int rowsWithIcons = endRow - startRow;
    quint32 iconMem;
    iconMem = static_cast<quint32>(G::iconHMax * G::iconHMax * 3 * rowsWithIcons);
    quint32 metaMem;
    int averageMetaMemReqdPerRow = 18900;   // bytes per row
    metaMem = static_cast<quint32>(dm->rowCount() * averageMetaMemReqdPerRow);
    /*
    qDebug() << __FUNCTION__
             << "rowsWithIcons =" << rowsWithIcons
             << "iconMem =" << iconMem / 1024 / 1024
             << "metaMem =" << metaMem / 1024 / 1024
             << "Total reqd =" << (iconMem + metaMem) / (1024 * 1024);
//             */
    return (iconMem + metaMem) / (1024 * 1024);
}

void MetadataCache::iconMax(QPixmap &thumb)
{
    if (G::isLogger) G::log(__FUNCTION__); 
//    mutex.lock(); qDebug() << __FUNCTION__; mutex.unlock();
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
    if (G::isLogger) {mutex.lock(); G::log(__FUNCTION__); mutex.unlock();}

//    G::t.restart();
    qDebug() << __FUNCTION__;
//    isShowCacheStatus = true;
    int count = 0;
    int rows = dm->rowCount();
    for (int row = 0; row < rows; ++row) {
        // is metadata already cached
        if (dm->index(row, G::MetadataLoadedColumn).data().toBool()) continue;

        QString fPath = dm->index(row, 0).data(G::PathRole).toString();
        QFileInfo fileInfo(fPath);
        if (metadata->loadImageMetadata(fileInfo, true, true, false, true, __FUNCTION__)) {
            metadata->m.row = row;
            dm->addMetadataForItem(metadata->m);
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
/* Currently not used */
    if (G::isLogger) {mutex.lock(); G::log(__FUNCTION__); mutex.unlock();}

    int sfRow = idx.row();
    int dmRow = dm->sf->mapToSource(idx).row();
    QString fPath = idx.data(G::PathRole).toString();

    if (!dm->sf->index(sfRow, G::MetadataLoadedColumn).data().toBool()) {
        QFileInfo fileInfo(fPath);
        if (metadata->loadImageMetadata(fileInfo, true, true, false, true, __FUNCTION__)) {
            metadata->m.row = dmRow;
            dm->addMetadataForItem(metadata->m);
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
    sort/filter change and all metadata has been loaded, but the icons visible have changed.
*/
    if (G::isLogger) {mutex.lock(); G::log(__FUNCTION__); mutex.unlock();}

    int start = startRow;
    int end = endRow;
    if (end > dm->sf->rowCount()) end = dm->sf->rowCount();
    if (cacheAllIcons) {
        start = 0;
        end = dm->sf->rowCount();
    }
    /*
    qDebug() << __FUNCTION__ << "start =" << start << "end =" << end
             << "rowCount =" << dm->sf->rowCount()
             << "action =" << action;
//    */

    // process visible icons first
    for (int row = firstIconVisible; row <= lastIconVisible; ++row) {
        if (abort) {
            emit updateIsRunning(false, true, __FUNCTION__);
            return;
        }

        // load icon
        mutex.lock();
        QModelIndex idx = dm->sf->index(row, 0);
        if (idx.isValid() && idx.data(Qt::DecorationRole).isNull()) {
            int dmRow = dm->sf->mapToSource(idx).row();
            QImage image;
            QString fPath = idx.data(G::PathRole).toString();
            /*
            if (G::isTest) QElapsedTimer t; if (G::isTest) t.restart();
            bool thumbLoaded = thumb->loadThumb(fPath, image);
            if (G::isTest) qDebug() << __FUNCTION__ << "Load thumbnail =" << t.nsecsElapsed() << fPath;
            */
            bool thumbLoaded = thumb->loadThumb(fPath, image);
            QPixmap pm;
            if (thumbLoaded) {
                pm = QPixmap::fromImage(image.scaled(G::maxIconSize, G::maxIconSize, Qt::KeepAspectRatio));
                dm->itemFromIndex(dm->index(dmRow, 0))->setIcon(pm);
                iconMax(pm);
                iconsCached.append(dmRow);
            }
        }
        mutex.unlock();
    }

    // process entire range
    for (int row = start; row <= end; ++row) {
        if (abort) {
            emit updateIsRunning(false, true, __FUNCTION__);
            return;
        }

        // load icon
        mutex.lock();
        QModelIndex idx = dm->sf->index(row, 0);
        if (idx.isValid() && idx.data(Qt::DecorationRole).isNull()) {
            int dmRow = dm->sf->mapToSource(idx).row();
            QImage image;
            QString fPath = idx.data(G::PathRole).toString();
            /*
            if (G::isTest) QElapsedTimer t; if (G::isTest) t.restart();
            bool thumbLoaded = thumb->loadThumb(fPath, image);
            if (G::isTest) qDebug() << __FUNCTION__ << "Load thumbnail =" << t.nsecsElapsed() << fPath;
            */
            bool thumbLoaded = thumb->loadThumb(fPath, image);
            QPixmap pm;
            if (thumbLoaded) {
                pm = QPixmap::fromImage(image.scaled(G::maxIconSize, G::maxIconSize, Qt::KeepAspectRatio));
                dm->itemFromIndex(dm->index(dmRow, 0))->setIcon(pm);
                iconMax(pm);
                iconsCached.append(dmRow);
            }
        }
        mutex.unlock();
    }

    // reset after a filter change
//    emit updateIconBestFit();
}

void MetadataCache::readMetadataChunk()
{
/*
    Load the thumb (icon) for all the image files in the target range. This is called after a
    sort/filter change and all metadata has been loaded, but the icons visible have changed.
*/
    if (G::isLogger) {mutex.lock(); G::log(__FUNCTION__); mutex.unlock();}

//    mutex.lock(); qDebug() << __FUNCTION__; mutex.unlock();
    int start = startRow;
    int end = endRow;
    if (cacheAllMetadata) {
        start = 0;
        end = dm->sf->rowCount();
    }

    /*
    qDebug() << __FUNCTION__ << "startRow =" << startRow
             << "endRow =" << endRow
             << "cacheAllMetadata =" << cacheAllMetadata;
//        */
    for (int row = start; row < end; ++row) {
        if (abort) {
//            mutex.lock();
//            qDebug() << __FUNCTION__ << "Aborting on row" << row;
//            mutex.unlock();
            emit updateIsRunning(false, true, __FUNCTION__);
            return;
        }
        // file path and dm source row in case filtered or sorted
        mutex.lock();     // rgh mutex
        QModelIndex idx = dm->sf->index(row, 0);
        int dmRow = dm->sf->mapToSource(idx).row();

        dm->readMetadataForItem(dmRow);
        /*
        QString fPath = idx.data(G::PathRole).toString();

        // load metadata
        if (!dm->sf->index(row, G::MetadataLoadedColumn).data().toBool()) {
            QFileInfo fileInfo(fPath);

            // only read metadata from files that we know how to
            QString ext = fileInfo.suffix().toLower();
            if (metadata->getMetadataFormats.contains(ext)) {
                if (metadata->loadImageMetadata(fileInfo, true, true, false, true, __FUNCTION__)) {
                    metadata->imageMetadata.row = dmRow;
                    dm->addMetadataForItem(metadata->imageMetadata);
                }
                else {
                    qDebug() << __FUNCTION__ << "Failed to load metadata for" << fPath;
                }
            }
            // cannot read this file type, load empty metadata
            else {
                metadata->clearMetadata();
                metadata->imageMetadata.row = dmRow;
                dm->addMetadataForItem(metadata->imageMetadata);
            }
        }
        */
        mutex.unlock();   // rgh mutex
        qApp->processEvents();
    }
}

void MetadataCache::readMetadataIconChunk()
{
/*
    Not being used (replaced by separate reads for metadata and icons
    Load the metadata and thumb (icon) for all the image files in the chunk range defined by
    startRow and endRow.
*/
    if (G::isLogger) G::log(__FUNCTION__);
    mutex.lock(); qDebug() << __FUNCTION__; mutex.unlock();
    if (cacheAllMetadata) endRow = dm->sf->rowCount();
    for (int row = startRow; row < endRow; ++row) {
        if (abort) {
            emit updateIsRunning(false, true, __FUNCTION__);
            return;
        }

        // file path and dm source row in case filtered or sorted
//        mutex.lock(); // rgh mutex
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
                metadata->m.row = dmRow;
                dm->addMetadataForItem(metadata->m);
            }
        }

        // load icon
        if (idx.data(Qt::DecorationRole).isNull()) {
            QImage image;
//            qDebug() << __FUNCTION__ << "row =" << row << fPath;
            bool thumbLoaded = thumb->loadThumb(fPath, image);
            if (thumbLoaded) {
                QPixmap pm = QPixmap::fromImage(image.scaled(G::maxIconSize, G::maxIconSize, Qt::KeepAspectRatio));
                dm->itemFromIndex(dm->index(dmRow, 0))->setIcon(pm);
                iconMax(pm);
                iconsCached.append(dmRow);
            }
        }
//        mutex.unlock();   // rgh mutex
    }
}

void MetadataCache::run()
{
/*
    Read the metadata and icons for a range or all the images, depending on action into the
    datamodel dm.

    If there has been a file selection change and not a new folder then update image cache.
*/
    if (G::isLogger) G::log(__FUNCTION__);
    if (foundItemsToLoad) {
        emit updateIsRunning(true, true, __FUNCTION__);
        qApp->processEvents();

        mutex.lock();     // rgh mutex
        int rowCount = dm->rowCount();
        dm->loadingModel = true;
        mutex.unlock();   // rgh mutex

        // pause image caching if it was running
        bool imageCachePaused = false;
        if (imageCacheThread->isRunning()) {
            imageCacheThread->pauseImageCache();
            imageCacheThread->wait();
            imageCachePaused = true;
        }

        // read all metadata but no icons
        if (action == Action::AllMetadata) {
            readAllMetadata();
        }

//        qDebug() << __FUNCTION__ << action;
        // read next metadata and icon chunk
        if (action == Action::NewFileSelected ||
            action == Action::Scroll ||
            action == Action::Resize ||
            action == Action::NewFolder ||
            action == Action::NewFolder2ndPass)
        {
            if (!G::allMetadataLoaded) readMetadataChunk();
            if (!G::memTest) readIconChunk();
        }

        if (abort) {
            dm->loadingModel = false;
//            qDebug() << "!!!!  Aborting MetadataCache::run  ";
            return;
        }

        // update allMetadataLoaded flag
        if (!G::allMetadataLoaded) {
            G::allMetadataLoaded = true;
            mutex.lock();     // rgh mutex
            for (int i = 0; i < rowCount; ++i) {
                if (!dm->sf->index(i, G::MetadataLoadedColumn).data().toBool()) {
                    G::allMetadataLoaded = false;
                    break;
                }
            }
            mutex.unlock();   // rgh mutex
        }

        // clean up orphaned icons outside icon range   rgh what about other actions
        if (!cacheAllIcons) {
            if (action == Action::NewFileSelected || action == Action::Scroll)  {
                iconCleanup();
                if (abort) return;
            }
        }

        if (action == Action::NewFolder) {
            if (isRefreshFolder) emit refreshCurrentAfterReload();
            else emit selectFirst();
            /* make a second pass if more than 250 thumbs visible in gridView or thumbView and
               so can calc best aspect */
            emit loadMetadataCache2ndPass();
        }

        // update status of metadataThreadRunningLabel in statusbar
        emit updateIsRunning(false, true, __FUNCTION__);

        // resume image caching if it was interrupted
        if (imageCachePaused) imageCacheThread->resumeImageCache();

        mutex.lock();     // rgh mutex
        dm->loadingModel = false;
        mutex.unlock();   // rgh mutex

    }
    /*
    qDebug() << "action =" << action
             << "G::isNewFolderLoaded =" << G::isNewFolderLoaded
             << "dm->currentRow" << dm->currentRow
             << "previousRow" << previousRow; */

    // after 2nd pass on new folder initiate the image cache
    if (action == Action::NewFolder2ndPass) {
        if (!G::memTest) G::metaCacheMB = memRequired();
        emit finished2ndPass();
        emit loadImageCache();
    }
 
    // if a file selection change and not a new folder then update image cache
    if (action == Action::NewFileSelected) {
        /*
        qDebug() << __FUNCTION__ << "fileSelectionChange"
                 << "dm->currentFilePath" << dm->currentFilePath
                 << "dm->currentRow" << dm->currentRow
                 << "G::isNewFolderLoaded" << G::isNewFolderLoaded
                 << "updateImageCache =" << updateImageCache;
//                 */
        if (G::isNewFolderLoaded && updateImageCache) {
            emit updateImageCachePosition();
        }
    }

}
