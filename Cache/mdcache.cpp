#include "Main/global.h"
#include "Cache/mdcache.h"

/*

The metadata cache reads the relevant metadata and preview thumbnails from the
image files and stores this information in the datamodel dm. The critical
metadata is the offset and length of the embedded JPGs and their width and
height, which is required by ImageCache and ImageView to display the images and
IconView to display the thumbnails. The other metadata is in the nice-to-know
category. The full size images are stored in the image cache, which is managed
in another thread, imageCacheThread.

Reading the metadata can become time consuming and the thumbnails or icons can
consume a lot of memory, so the metadata is read in chunks. Each chunk size
(number of image files) is the greater of the default value of maxChunkSize or
the number of thumbs visible in the GridView viewport.

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

When Winnow is running and a new folder is selected (OUT OF DATE)

    • A new folder is selected and MW::folderSelectionChange executes, which in turn, starts
      metadataCacheThread::loadNewFolder.

    • loadNewMetadataCache starts metadataCacheThread::run, which reads a chunk of image
      files, loading the metadata and icons to the datamodel.

    • After the chunk is loaded metadataCacheThread emits a signal to start imageCacheThread.

When the user scrolls through the thumbnails:

    • If it is running the imageCacheThread is aborted and restarted.

    • As above, the metadata chunk is loaded and the datamodel filter is updated.

    • If it was running the imageCacheThread is resumed. However, image caching is not
      signalled to start (only resume).

When the user selects a thumbnail or a filter or sort has been invoked.

    • MW::fileSelectionChange is called.

    • loadMetadataIconChunk is called and any addtional metadata or icons are loaded.

    • a signal is emitted and imageCacheThread->updateImageCachePosition is called and the
    image cache is updated.

    • Load ImageView either from the cache or directly from the file if not cached.

*/

MetadataCache::MetadataCache(QObject *parent,
                             DataModel *dm,
                             Metadata *metadata,
                             FrameDecoder *frameDecoder
                             )
    : QThread(parent)
{
    if (G::isLogger) G::log("MetadataCache::MetadataCache");
    this->dm = dm;
    this->metadata = metadata;
    this->frameDecoder = frameDecoder;
    thumb = new Thumb(dm, metadata, frameDecoder);

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
    delete thumb;
}

void MetadataCache::stop()
{
    if (G::isLogger) G::log("MetadataCache::stop");
    if (isRunning()) {
        mutex.lock();
        abort = true;
        condition.wakeOne();
        mutex.unlock();
        wait();
        abort = false;
        emit updateIsRunning(false, false, "MetadataCache::stop");
    }
    if (G::stop) emit stopped("MDCache");
}

void MetadataCache::scrollChange(QString source)
{
/*
    This function is called when there is a scroll event in a view of the datamodel.

    A chunk of metadata and icons are added to the datamodel and icons outside the caching
    limits are removed (not visible and not with chunk range)
*/
//    return;
    if (G::isLogger) G::log("MetadataCache::scrollChange", "called by =" + source);
    if (isRunning()) {
        mutex.lock();
        abort = true;
        condition.wakeOne();
        mutex.unlock();
        wait();
        if (isRunning()) {
            qDebug() << "MetadataCache::scrollChange" << "ABORT FAILED";
            return;
        }
    }
    if (G::isInitializing || !G::isNewFolderLoaded) return;
    abort = false;
    instance = dm->instance;
    action = Action::Scroll;
    setRange();
    foundItemsToLoad = anyItemsToLoad();
        /*
        qDebug() << "MetadataCache::scrollChange" << "foundItemsToLoad =" << foundItemsToLoad
                 << "start =" << startRow << "end =" << endRow
                 << "firstIconVisible =" << firstIconVisible
                 << "firstIconVisible =" << firstIconVisible
                 << "rowCount =" << dm->sf->rowCount()
                 << "action =" << action;
    //    */
    start(TimeCriticalPriority);
}

void MetadataCache::sizeChange(QString source)
{
/*
    This function is called when the number of icons visible in the viewport changes when the
    icon size or the viewport change size.
*/
//    return;
    if (G::isLogger) G::log("MetadataCache::sizeChange", "called by = " + source);
    if (isRunning()) {
        mutex.lock();
        abort = true;
        condition.wakeOne();
        mutex.unlock();
        wait();
    }
    abort = false;
    instance = dm->instance;
    action = Action::Resize;
    setRange();
    foundItemsToLoad = anyItemsToLoad();
    start(TimeCriticalPriority);
}

void MetadataCache::fileSelectionChange(/*bool okayToImageCache*/) // rghcachechange
{
/*
    This function is called from MW::fileSelectionChange. A chunk of metadata and icons are
    added to the datamodel. The image cache is updated.
*/
    if (G::isLogger || G::isFlowLogger) G::log("MetadataCache::fileSelectionChange");
//    qDebug() << "MetadataCache::fileSelectionChange";
    if (isRunning()) {
        mutex.lock();
        abort = true;
        condition.wakeOne();
        mutex.unlock();
        wait();
    }
//    qDebug() << "MetadataCache::fileSelectionChange";
    abort = false;
    instance = dm->instance;
    action = Action::NewFileSelected;
    setRange();
    foundItemsToLoad = anyItemsToLoad();
    start(TimeCriticalPriority);
}

bool MetadataCache::anyItemsToLoad()
{
    if (G::isLogger) G::log("MetadataCache::anyItemsToLoad");
    for (int i = startRow; i < endRow; ++i) {
        // icon not loaded
        if (!dm->iconLoaded(i, instance))
            return true;
        // metadata not loaded
        if (!dm->sf->index(i, G::MetadataLoadedColumn).data().toBool())
            return true;
    }
    // update status of metadataThreadRunningLabel in statusbar
    emit updateIsRunning(false, true, "MetadataCache::anyItemsToLoad");
    return false;
}

void MetadataCache::setRange()
{
/*
    Define the range of icons to cache: prev + current + next viewports/pages of icons
    Variables are set in MW::updateIconsVisible
*/
    if (G::isLogger) G::log("MetadataCache::setRange");
    int rowCount = dm->sf->rowCount();

    // default total per page (dtpp) (prev, curr and next pages)
    int dtpp = metadataChunkSize / 3;

    // total per page (tpp)
    tpp = visibleIcons;
    if (dtpp > tpp) tpp = dtpp;

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
    qDebug()  << "MetadataCache::setRange"
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
    if (G::isLogger) G::log("MetadataCache::iconCleanup");
    QMutableListIterator<int> i(iconsCached);
    QPixmap nullPm;
    QIcon nullIcon;
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
            QModelIndex dmIdx = dm->index(dmRow, 0);
            emit setIcon(dmIdx, nullPm, instance, "MetadataCache::iconCleanup");
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
    if (G::isLogger) G::log("MetadataCache::memRequired");
    int rowsWithIcons = endRow - startRow;
    quint32 iconMem;
    iconMem = static_cast<quint32>(G::iconHMax * G::iconHMax * 3 * rowsWithIcons);
    quint32 metaMem;
    int averageMetaMemReqdPerRow = 18900;   // bytes per row
    metaMem = static_cast<quint32>(dm->rowCount() * averageMetaMemReqdPerRow);
    /*
    qDebug() << "MetadataCache::memRequired"
             << "rowsWithIcons =" << rowsWithIcons
             << "iconMem =" << iconMem / 1024 / 1024
             << "metaMem =" << metaMem / 1024 / 1024
             << "Total reqd =" << (iconMem + metaMem) / (1024 * 1024);
//             */
    return (iconMem + metaMem) / (1024 * 1024);
}

void MetadataCache::iconMax(QPixmap &thumb)
{
    if (G::isLogger) G::log("MetadataCache::iconMax");
    if (G::iconWMax == G::maxIconSize && G::iconHMax == G::maxIconSize) return;

    // for best aspect calc
    int w = thumb.width();
    int h = thumb.height();
    if (w > G::iconWMax) G::iconWMax = w;
    if (h > G::iconHMax) G::iconHMax = h;
}

bool MetadataCache::loadIcon(int sfRow)
{
    if (G::isLogger) G::log("MetadataCache::loadIcon");

    QModelIndex sfIdx = dm->sf->index(sfRow,0);
    QModelIndex dmIdx = dm->sf->mapToSource(sfIdx);

    if (!dmIdx.isValid()) return false;
    if (!dm->iconLoaded(sfRow, instance)) {
        int dmRow = dmIdx.row();
        bool isVideo = dm->index(dmRow, G::VideoColumn).data().toBool();
        QImage image;
        QString fPath = dmIdx.data(G::PathRole).toString();
        QPixmap pm;
        bool thumbLoaded = thumb->loadThumb(fPath, image, instance, "MetadataCache::loadIcon");
        if (isVideo && G::renderVideoThumb) {
            iconsCached.append(dmRow);
            return true;
        }
        if (thumbLoaded) {
            pm = QPixmap::fromImage(image.scaled(G::maxIconSize, G::maxIconSize, Qt::KeepAspectRatio));
//            emit setIcon(dmIdx, pm, instance, "MetadataCache::loadIcon");
        }
        else {
            pm = QPixmap(":/images/error_image256.png");
            qWarning() << "WARNING" << "MetadataCache::loadIcon" << "Failed to load thumbnail." << fPath;
        }
        dm->setIcon(dmIdx, pm, instance, "MetadataCache::loadIcon");
        iconMax(pm);
        iconsCached.append(dmRow);
    }
    return true;
}

void MetadataCache::updateIconLoadingProgress(int count, int end)
{
    if (G::isLogger) G::log("MetadataCache::updateIconLoadingProgress");
    if (G::dmEmpty) return;
    // show progress
    if (!G::isNewFolderLoaded && !G::stop) {
        if (count % countInterval == 0) {
            QString msg = "Loading thumbnails: ";
            msg += QString::number(count) + " of " + QString::number(end);
            emit showCacheStatus(msg);
        }
    }
//    if (G::dmEmpty) {
//        emit showCacheStatus("Image loading has been aborted");
//    }
}

void MetadataCache::readAllMetadata()
{
/*
    Load the thumb (icon) for all the image files in the folder(s).
    Not being used.
*/
    if (G::isLogger || G::isFlowLogger) G::log("MetadataCache::readAllMetadata");
    int count = 0;
    int rows = dm->rowCount();
    for (int row = 0; row < rows; ++row) {
        // is metadata already cached
        if (dm->metadataLoaded(row)) continue;

        QString fPath = dm->index(row, 0).data(G::PathRole).toString();
        QFileInfo fileInfo(fPath);
        if (metadata->loadImageMetadata(fileInfo, instance, true, true, false, true, "MetadataCache::readAllMetadata")) {
            metadata->m.row = row;
            // emit??
            emit addToDatamodel(metadata->m, "MetadataCache::readAllMetadata");
//            dm->addMetadataForItem(metadata->m, "MetadataCache::readAllMetadata");
            count++;
        }

        if (G::isLinearLoading) {
            if (row % countInterval == 0) {
                QString msg = "Reading metadata: ";
                msg += QString::number(row) + " of " + QString::number(rows);
                emit showCacheStatus(msg);
            }
        }
    }
    G::allMetadataLoaded = true;
}

void MetadataCache::readMetadataIcon(const QModelIndex &idx)
{
/* rgh Currently not used */
    if (G::isLogger) G::log("MetadataCache::readMetadataIcon");

    int sfRow = idx.row();
    int dmRow = dm->sf->mapToSource(idx).row();
    QString fPath = idx.data(G::PathRole).toString();

    if (!dm->sf->index(sfRow, G::MetadataLoadedColumn).data().toBool()) {
        QFileInfo fileInfo(fPath);
        if (metadata->loadImageMetadata(fileInfo, instance, true, true, false, true, "MetadataCache::readMetadataIcon")) {
            metadata->m.row = dmRow;
            dm->addMetadataForItem(metadata->m, "MetadataCache::readAllMetadata");
        }
    }

    // load icon
    if (!dm->iconLoaded(sfRow, instance)) {
        QImage image;
        bool thumbLoaded = thumb->loadThumb(fPath, image, instance, "MetadataCache::readMetadataIcon");
        if (thumbLoaded) {
            QPixmap pm = QPixmap::fromImage(image.scaled(G::maxIconSize, G::maxIconSize, Qt::KeepAspectRatio));
            QModelIndex dmIdx = dm->index(dmRow, 0);
            emit setIcon(dmIdx, pm, instance, "MetadataCache::readMetadataIcon");
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
    if (G::isLogger || G::isFlowLogger) G::log("MetadataCache::readIconChunk");
    int start = startRow;
    int end = endRow;
    if (end > dm->sf->rowCount()) end = dm->sf->rowCount();
    if (cacheAllIcons) {
        start = 0;
        end = dm->sf->rowCount();
    }
    int count = 0;
    /*
    qDebug() << "MetadataCache::readIconChunk" << "start =" << start << "end =" << end
             << "firstIconVisible =" << firstIconVisible
             << "lastIconVisible =" << lastIconVisible
             << "rowCount =" << dm->sf->rowCount()
             << "action =" << action;
//    */

    // process visible icons first
    for (int row = firstIconVisible; row <= lastIconVisible; ++row) {
        if (abort || G::dmEmpty) {
            emit updateIsRunning(false, true, "MetadataCache::readIconChunk");
            return;
        }
        loadIcon(row);
//        updateIconLoadingProgress(count++, end);
    }

    // process icons before visible range
    if (start < firstIconVisible) {
        for (int row = start; row < firstIconVisible; ++row) {
            if (abort || G::dmEmpty) {
                emit updateIsRunning(false, true, "MetadataCache::readIconChunk");
                return;
            }
            loadIcon(row);
//            updateIconLoadingProgress(count++, end);
        }
    }

    // process icons after visible range
    if (end > lastIconVisible + 1) {
        for (int row = lastIconVisible = 1; row < end; ++row) {
            if (abort || G::dmEmpty) {
                emit updateIsRunning(false, true, "MetadataCache::readIconChunk");
                return;
            }
            loadIcon(row);
//            updateIconLoadingProgress(count++, end);
        }
    }
}

void MetadataCache::readMetadataChunk()
{
/*
    Load the metadata for all the image files in the target range.
*/
    if (G::isLogger || G::isFlowLogger) G::log("MetadataCache::readMetadataChunk");

    int tryAgain = 0;
    bool metadataLoadFailed;
    do {
        metadataLoadFailed = false;
        int start = startRow;
        int end = endRow;
        if (cacheAllMetadata) {
            start = 0;
            end = dm->sf->rowCount();
        }

        for (int row = start; row < end; ++row) {
            if (abort) {
                emit updateIsRunning(false, true, "MetadataCache::readMetadataChunk");
                return;
            }
            if (dm->sf->index(row, G::MetadataLoadedColumn).data().toBool()) continue;
            // file path and dm source row in case filtered or sorted
            QModelIndex idx = dm->sf->index(row, 0);
            int dmRow = dm->sf->mapToSource(idx).row();
            if (!dm->readMetadataForItem(dmRow, instance)) {
                metadataLoadFailed = true;
                /*
                qDebug() << "MetadataCache::readMetadataChunk"
                         << "tryAgain =" << tryAgain
                         << "row =" << row
                            ;
                            //*/
            }
            QString fPath = idx.data(G::PathRole).toString();
            /*
            if (G::isLogger || G::isFlowLogger)
            {
                QString msg = "Reading metadata: ";
                msg += QString::number(row) + " of " + QString::number(end-1);
                msg += " " + fPath;
                G::log("MetadataCache::readMetadataChunk", msg);
            }
            //*/
            if (G::isLinearLoading) {
                if (row % countInterval == 0) {
                    QString msg = "Reading metadata: ";
                    msg += QString::number(row) + " of " + QString::number(end)/* + " " + fPath*/;
                    emit showCacheStatus(msg);
                }
            }
        }
    } while (metadataLoadFailed && metadataTry > tryAgain++);
}

void MetadataCache::run()
{
/*
    Read the metadata and icons for a range or all the images, depending on action into the
    datamodel dm.

    If there has been a file selection change and not a new folder then update image cache.
*/
    if (G::isLogger || G::isFlowLogger) G::log("MetadataCache::run", actionList.at(action));

    if (foundItemsToLoad) {
        emit updateIsRunning(true, true, "MetadataCache::run");
        int rowCount = dm->sf->rowCount();

        // read next metadata and icon chunk
        if (action == Action::Scroll ||
            action == Action::Resize ||
            action == Action::NewFileSelected)
        {
            if (!G::allMetadataLoaded) {
                readMetadataChunk();
            }
            if (!G::allIconsLoaded) {
                readIconChunk();
            }
        }

        if (abort) {
            emit updateIsRunning(false, true, "MetadataCache::run");
            return;
        }

        // update allMetadataLoaded flag if metadata has been loaded for every row in dm
        if (!G::allMetadataLoaded) {
            G::allMetadataLoaded = dm->isAllMetadataLoaded();
        }

        // clean up orphaned icons outside icon range   rgh what about other actions
        if (!cacheAllIcons) {
            if (action == Action::NewFileSelected || action == Action::Scroll)  {
                iconCleanup();
                if (abort) {
                    return;
                }
            }
        }
    }

    // update status of metadataThreadRunningLabel in statusbar
    emit updateIsRunning(false, true, "MetadataCache::run");
}
