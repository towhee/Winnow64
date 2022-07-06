#include "Main/global.h"
#include "Cache/mdcache.h"

/*
The metadata cache reads the relevant metadata and preview thumbnails from the image files and
stores this information in the datamodel dm. The critical metadata is the offset and length of
the embedded JPGs and their width and height, which is required by the ImageCache and
ImageView to display the images and IconView to display the thumbnails. The other metadata is
in the nice-to-know category.  The full size images are stored in the image cache, which is
managed in another thread, imageCacheThread.

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

MetadataCache::MetadataCache(QObject *parent, DataModel *dm,
                  Metadata *metadata) : QThread(parent)
{
    if (G::isLogger) G::log(CLASSFUNCTION);
    this->dm = dm;
    this->metadata = metadata;
    thumb = new Thumb(dm, metadata);

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
    if (G::isLogger) G::log(CLASSFUNCTION);
//    qDebug() << CLASSFUNCTION;
//    QString isRun;
//    if (isRunning()) isRun = "true";
//    else isRun = "false";
//    G::track(CLASSFUNCTION, "Start: isRunning = " + isRun);
    if (isRunning()) {
        mutex.lock();
        abort = true;
        condition.wakeOne();
        mutex.unlock();
        wait();
        abort = false;
        emit updateIsRunning(false, false, CLASSFUNCTION);
    }
//    if (isRunning()) isRun = "true";
//    else isRun = "false";
//    G::track(CLASSFUNCTION, "Done:  isRunning = " + isRun);
}

void MetadataCache::scrollChange(QString source)
{
/*
    This function is called when there is a scroll event in a view of the datamodel.

    A chunk of metadata and icons are added to the datamodel and icons outside the caching
    limits are removed (not visible and not with chunk range)
*/
//    return;
    if (G::isLogger) G::log(CLASSFUNCTION, "called by =" + source);
    if (isRunning()) {
        mutex.lock();
        abort = true;
        condition.wakeOne();
        mutex.unlock();
        wait();
        if (isRunning()) {
            qDebug() << CLASSFUNCTION << "ABORT FAILED";
            return;
        }
    }
    if (G::isInitializing || !G::isNewFolderLoaded) return;
    abort = false;
    action = Action::Scroll;
    setRange();
    foundItemsToLoad = anyItemsToLoad();
        /*
        qDebug() << CLASSFUNCTION << "foundItemsToLoad =" << foundItemsToLoad
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
    if (G::isLogger) G::log(CLASSFUNCTION, "called by = " + source);
    if (isRunning()) {
        mutex.lock();
        abort = true;
        condition.wakeOne();
        mutex.unlock();
        wait();
    }
//    qDebug() << CLASSFUNCTION << "called by =" << source;
    qDebug() << "MetadataCache::sizeChange" << source;
    abort = false;
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
    if (G::isLogger || G::isFlowLogger) G::log(CLASSFUNCTION);
//    qDebug() << CLASSFUNCTION;
    if (isRunning()) {
        mutex.lock();
        abort = true;
        condition.wakeOne();
        mutex.unlock();
        wait();
    }
//    qDebug() << "MetadataCache::fileSelectionChange";
    abort = false;
    action = Action::NewFileSelected;
    setRange();
    foundItemsToLoad = anyItemsToLoad();
    start(TimeCriticalPriority);
}

bool MetadataCache::anyItemsToLoad()
{
    if (G::isLogger) G::log(CLASSFUNCTION);
    for (int i = startRow; i < endRow; ++i) {
        // ignore if image does not have metadata  (rgh include so can show video thumbs)
//        QString ext = dm->sf->index(i, G::TypeColumn).data().toString().toLower();
//        if (metadata->noMetadataFormats.contains(ext)) continue;
        // icon not loaded
        if (!dm->iconLoaded(i))
            return true;
        // metadata not loaded
        if (!dm->sf->index(i, G::MetadataLoadedColumn).data().toBool())
            return true;
    }
    // update status of metadataThreadRunningLabel in statusbar
    emit updateIsRunning(false, true, CLASSFUNCTION);
    return false;
}

void MetadataCache::setRange()
{
/*
    Define the range of icons to cache: prev + current + next viewports/pages of icons
    Variables are set in MW::updateIconsVisible
*/
    if (G::isLogger) G::log(CLASSFUNCTION);
    int rowCount = dm->sf->rowCount();

    // default total per page (dtpp) (prev, curr and next pages)
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
    qDebug()  <<  CLASSFUNCTION
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
    if (G::isLogger) G::log(CLASSFUNCTION);
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
//            dm->itemFromIndex(dm->index(dmRow, 0))->setIcon(nullPm);
            QModelIndex dmIdx = dm->index(dmRow, 0);
            emit setIcon(dmIdx, nullPm, dm->instance);
//            qDebug() << CLASSFUNCTION << actionList[action]
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
    if (G::isLogger) G::log(CLASSFUNCTION);
//    mutex.lock(); qDebug() << CLASSFUNCTION; mutex.unlock();
    int rowsWithIcons = endRow - startRow;
    quint32 iconMem;
    iconMem = static_cast<quint32>(G::iconHMax * G::iconHMax * 3 * rowsWithIcons);
    quint32 metaMem;
    int averageMetaMemReqdPerRow = 18900;   // bytes per row
    metaMem = static_cast<quint32>(dm->rowCount() * averageMetaMemReqdPerRow);
    /*
    qDebug() << CLASSFUNCTION
             << "rowsWithIcons =" << rowsWithIcons
             << "iconMem =" << iconMem / 1024 / 1024
             << "metaMem =" << metaMem / 1024 / 1024
             << "Total reqd =" << (iconMem + metaMem) / (1024 * 1024);
//             */
    return (iconMem + metaMem) / (1024 * 1024);
}

void MetadataCache::iconMax(QPixmap &thumb)
{
    if (G::isLogger) G::log(CLASSFUNCTION);
//    mutex.lock(); qDebug() << CLASSFUNCTION; mutex.unlock();
    if (G::iconWMax == G::maxIconSize && G::iconHMax == G::maxIconSize) return;

    // for best aspect calc
    int w = thumb.width();
    int h = thumb.height();
    if (w > G::iconWMax) G::iconWMax = w;
    if (h > G::iconHMax) G::iconHMax = h;
}

bool MetadataCache::loadIcon(int sfRow)
{
    if (G::isLogger) G::log(CLASSFUNCTION);
//    QString ext = dm->sf->index(sfRow, G::TypeColumn).data().toString().toLower();
    QModelIndex dmIdx = dm->sf->mapToSource(dm->sf->index(sfRow, 0));
    if (!dmIdx.isValid()) return false;
    if (!dm->isIconCaching(sfRow) && !dm->iconLoaded(sfRow)) {
        dm->setIconCaching(sfRow, true);
        int dmRow = dmIdx.row();
        bool isVideo = dm->index(dmRow, G::VideoColumn).data().toBool();
        QImage image;
        QString fPath = dmIdx.data(G::PathRole).toString();
        QPixmap pm;
        bool thumbLoaded = thumb->loadThumb(fPath, image, "MetadataCache::loadIcon");
        if (isVideo) {
            iconsCached.append(dmRow);
            return true;
        }
        if (thumbLoaded) {
            pm = QPixmap::fromImage(image.scaled(G::maxIconSize, G::maxIconSize, Qt::KeepAspectRatio));
            emit setIcon(dmIdx, pm, dm->instance);
            iconMax(pm);
            iconsCached.append(dmRow);
        }
        else {
            pm = QPixmap(":/images/error_image.png");
            qWarning() << CLASSFUNCTION << "Failed to load thumbnail." << fPath;
        }
    }
    return true;
}

void MetadataCache::updateIconLoadingProgress(int count, int end)
{
    if (G::isLogger) G::log(CLASSFUNCTION);
    // show progress
    if (!G::isNewFolderLoaded) {
        if (count % countInterval == 0) {
            QString msg = "Loading thumbnails: ";
            msg += QString::number(count) + " of " + QString::number(end);
            emit showCacheStatus(msg);
        }
    }
}

void MetadataCache::readAllMetadata()
{
/*
    Load the thumb (icon) for all the image files in the folder(s).
    Not being used.
*/
    if (G::isLogger || G::isFlowLogger) G::log(CLASSFUNCTION);
    int count = 0;
    int rows = dm->rowCount();
    for (int row = 0; row < rows; ++row) {
        // is metadata already cached
        if (dm->metadataLoaded(row)) continue;

        QString fPath = dm->index(row, 0).data(G::PathRole).toString();
        QFileInfo fileInfo(fPath);
        if (metadata->loadImageMetadata(fileInfo, true, true, false, true, CLASSFUNCTION)) {
            metadata->m.row = row;
            dm->addMetadataForItem(metadata->m);
            count++;
        }
        /*
        if (G::isLogger || G::isFlowLogger)
        {
            QString msg = "Reading metadata: ";
            msg += QString::number(row) + " of " + QString::number(end-1);
            msg += " " + fPath;
            G::log(CLASSFUNCTION, msg);
        }
        //*/
        if (row % countInterval == 0) {
            QString msg = "Reading metadata: ";
            msg += QString::number(row) + " of " + QString::number(rows);
            emit showCacheStatus(msg);
        }
//        QApplication::processEvents();
    }
    G::allMetadataLoaded = true;
}

void MetadataCache::readMetadataIcon(const QModelIndex &idx)
{
/* rgh Currently not used */
    if (G::isLogger) {mutex.lock(); G::log(CLASSFUNCTION); mutex.unlock();}

    int sfRow = idx.row();
    int dmRow = dm->sf->mapToSource(idx).row();
    QString fPath = idx.data(G::PathRole).toString();

    if (!dm->sf->index(sfRow, G::MetadataLoadedColumn).data().toBool()) {
        QFileInfo fileInfo(fPath);
        if (metadata->loadImageMetadata(fileInfo, true, true, false, true, CLASSFUNCTION)) {
            metadata->m.row = dmRow;
            dm->addMetadataForItem(metadata->m);
        }
    }

    // load icon
    if (!dm->iconLoaded(sfRow)) {
        QImage image;
//        qDebug() << CLASSFUNCTION << "row =" << sfRow << fPath;
        bool thumbLoaded = thumb->loadThumb(fPath, image, "MetadataCache::readMetadataIcon");
        if (thumbLoaded) {
            QPixmap pm = QPixmap::fromImage(image.scaled(G::maxIconSize, G::maxIconSize, Qt::KeepAspectRatio));
//            dm->itemFromIndex(dm->index(dmRow, 0))->setIcon(pm);
            QModelIndex dmIdx = dm->index(dmRow, 0);
            emit setIcon(dmIdx, pm, dm->instance);
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
    if (G::isLogger || G::isFlowLogger) {mutex.lock(); G::log(CLASSFUNCTION); mutex.unlock();}
    int start = startRow;
    int end = endRow;
    if (end > dm->sf->rowCount()) end = dm->sf->rowCount();
    if (cacheAllIcons) {
        start = 0;
        end = dm->sf->rowCount();
    }
    int count = 0;
    /*
    qDebug() << CLASSFUNCTION << "start =" << start << "end =" << end
             << "firstIconVisible =" << firstIconVisible
             << "lastIconVisible =" << lastIconVisible
             << "rowCount =" << dm->sf->rowCount()
             << "action =" << action;
//    */

    // process visible icons first
    for (int row = firstIconVisible; row <= lastIconVisible; ++row) {
        if (abort) {
            emit updateIsRunning(false, true, CLASSFUNCTION);
            return;
        }
//        qDebug() << "MetadataCache::readIconChunk 0 row =" << row;
        loadIcon(row);
        updateIconLoadingProgress(count++, end);
    }

    // process icons before visible range
    if (start < firstIconVisible) {
        for (int row = start; row < firstIconVisible; ++row) {
            if (abort) {
                emit updateIsRunning(false, true, CLASSFUNCTION);
                return;
            }
//            qDebug() << "MetadataCache::readIconChunk 1 row =" << row;
            loadIcon(row);
            updateIconLoadingProgress(count++, end);
        }
    }

    // process icons after visible range
    if (end > lastIconVisible + 1) {
        for (int row = lastIconVisible = 1; row < end; ++row) {
            if (abort) {
                emit updateIsRunning(false, true, CLASSFUNCTION);
                return;
            }
//            qDebug() << "MetadataCache::readIconChunk 2 row =" << row;
            loadIcon(row);
            updateIconLoadingProgress(count++, end);
        }
    }
}

void MetadataCache::readMetadataChunk()
{
/*
    Load the metadata for all the image files in the target range.
*/
    if (G::isLogger || G::isFlowLogger) {mutex.lock(); G::log(CLASSFUNCTION); mutex.unlock();}

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
                emit updateIsRunning(false, true, CLASSFUNCTION);
                return;
            }
            if (dm->sf->index(row, G::MetadataLoadedColumn).data().toBool()) continue;
            // file path and dm source row in case filtered or sorted
            QModelIndex idx = dm->sf->index(row, 0);
            int dmRow = dm->sf->mapToSource(idx).row();
            if (!dm->readMetadataForItem(dmRow)) {
                metadataLoadFailed = true;
//                /*
                qDebug() << CLASSFUNCTION
                         << "tryAgain =" << tryAgain
                         << "row =" << row
                            ;
                            //*/
            }
            QString fPath = idx.data(G::PathRole).toString();
            if (G::isLogger || G::isFlowLogger)
                /*
            {
                QString msg = "Reading metadata: ";
                msg += QString::number(row) + " of " + QString::number(end-1);
                msg += " " + fPath;
                G::log(CLASSFUNCTION, msg);
            }
            //*/
            if (row % countInterval == 0) {
                QString msg = "Reading metadata: ";
                msg += QString::number(row) + " of " + QString::number(end)/* + " " + fPath*/;
                emit showCacheStatus(msg);
            }

            // keep event processing up-to-date to improve signal/slot performance emit loadMetadataCache2ndPass()
//            QApplication::processEvents();
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
    QString msg = "action = " + actionList.at(action) +
                  " foundItemsToLoad = " + QVariant(foundItemsToLoad).toString();
    if (G::isLogger || G::isFlowLogger) G::log(CLASSFUNCTION, msg);
//    qDebug() << CLASSFUNCTION << actionList.at(action) << "foundItemsToLoad =" << foundItemsToLoad;

    if (action == Action::MR_Read) {
        read();
        return;
    }


    if (foundItemsToLoad) {
        emit updateIsRunning(true, true, CLASSFUNCTION);
        int rowCount = dm->sf->rowCount();
//        dm->loadingModel = true;

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
//            dm->loadingModel = false;
            emit updateIsRunning(false, true, CLASSFUNCTION);
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
//                    dm->loadingModel = false;
                    return;
                }
            }
        }

//        dm->loadingModel = false;
    }

    // update status of metadataThreadRunningLabel in statusbar
    emit updateIsRunning(false, true, CLASSFUNCTION);
}

//***************************************************************************************
//      MetaRead stuff

void MetadataCache::mr_read(int sfRow, QString source)
{
    /*
    This function is called when there is a scroll event in a view of the datamodel.

    A chunk of metadata and icons are added to the datamodel and icons outside the caching
    limits are removed (not visible and not with chunk range)
*/
    //    return;
    if (G::isLogger) G::log(CLASSFUNCTION, "called by =" + source);
    if (isRunning()) {
        mutex.lock();
        abort = true;
        condition.wakeOne();
        mutex.unlock();
        wait();
        if (isRunning()) {
            qDebug() << CLASSFUNCTION << "ABORT FAILED";
            return;
        }
    }
//    if (G::isInitializing || !G::isNewFolderLoaded) return;
    abort = false;
    action = Action::MR_Read;
//    setRange();
//    foundItemsToLoad = anyItemsToLoad();
    /*
        qDebug() << CLASSFUNCTION << "foundItemsToLoad =" << foundItemsToLoad
                 << "start =" << startRow << "end =" << endRow
                 << "firstIconVisible =" << firstIconVisible
                 << "firstIconVisible =" << firstIconVisible
                 << "rowCount =" << dm->sf->rowCount()
                 << "action =" << action;
    //    */
    start(TimeCriticalPriority);
}

void MetadataCache::initialize()
{
    if (G::isLogger || G::isFlowLogger) G::log(CLASSFUNCTION);
    iconsLoaded.clear();
//    mr_visibleIcons.clear();
    priorityQueue.clear();
    imageCachingStarted = false;
}

bool MetadataCache::isNotLoaded(int sfRow)
{
    /* not used  */

    QModelIndex sfIdx = dm->sf->index(sfRow, G::MetadataLoadedColumn);
    if (!sfIdx.data().toBool()) return true;
    if (!dm->iconLoaded(sfRow)) return true;
    return false;
}

bool MetadataCache::okToLoadIcon(int sfRow) {
    if (sfRow >= dm->startIconRange && sfRow <= dm->endIconRange) return true;
    else return false;
}

void MetadataCache::dmRowRemoved(int dmRow)
{
    if (G::isLogger) G::log(CLASSFUNCTION);
//    if (abort) return;
    int idx = iconsLoaded.indexOf(dmRow);
    iconsLoaded.removeAt(idx);
}

void MetadataCache::cleanupIcons()
{
/*
    Remove icons not in priority queue after iconChunkSize
*/
    if (G::isLogger) G::log(CLASSFUNCTION);
    // cleanup non-visible icons
    if (debugCaching) {
        qDebug().noquote() << CLASSFUNCTION
                           << "start"
                              ;
    }

    // check if datamodel size is less than assigned icon cache chunk size
    if (G::loadOnlyVisibleIcons && visibleIconCount >= sfRowCount) return;
    if (iconChunkSize >= sfRowCount) return;

    firstIconRow = dm->startIconRange;
    lastIconRow = dm->endIconRange;
    QPixmap nullPm;

//    goto B;

    //  OPTION A: search dm outside icon range
    A:
    // dm rows outside icon range
    for (int dmRow = 0; dmRow < dm->rowCount(); ++dmRow) {
        if (abort) return;
        int sfRow = dm->proxyRowFromModelRow(dmRow);
        // in range?
        if (sfRow >= firstIconRow && sfRow <= lastIconRow) continue;
        // icon not loaded?
        QModelIndex dmIdx = dm->index(dmRow, 0);
        if (dm->itemFromIndex(dmIdx)->icon().isNull()) continue;
        // remove unwanted icon
        emit setIcon(dmIdx, nullPm, dmInstance);
    }
    return;

    // OPTION B: search iconsLoaded for icons outside range
    B:
    int n = iconsLoaded.size();
//    QPixmap nullPm;
    for (int i = 0; i < iconsLoaded.size(); ++i) {
        if (abort) {
            return;
        }
        int dmRow = iconsLoaded.at(i);
        // check if row has been deleted
        if (dmRow >= dm->rowCount()) {
            /*
            qDebug() << CLASSFUNCTION
                     << "dmRow =" << dmRow
                     << "rowCount =" << dm->rowCount()
                        ;
                        //*/
            #if (QT_VERSION >= QT_VERSION_CHECK(6, 0, 0))
            if (!abort) iconsLoaded.remove(i);
            #endif
            continue;
        }
        QModelIndex dmIdx = dm->index(dmRow, 0);
        int sfRow = dm->proxyRowFromModelRow(dmRow);
        /*
        qDebug() << CLASSFUNCTION
                 << "i =" << i
                 << "iconsLoaded.at(i) = dmRow =" << dmRow
                 << "sfRow =" << sfRow
                 << "dm->firstVisibleRow =" << dm->firstVisibleRow
                 << "dm->lastVisibleRow =" << dm->lastVisibleRow
                 << "isVisible(sfRow) =" << isVisible(sfRow)
                    ;
                    //*/
        if (okToLoadIcon(sfRow)) continue;
        emit setIcon(dmIdx, nullPm, dmInstance);
        #if (QT_VERSION >= QT_VERSION_CHECK(6, 0, 0))
        iconsLoaded.remove(i);
        #endif
    }
    if (debugCaching) {
        qDebug().noquote() << CLASSFUNCTION
                           << "done"
                              ;
    }
}

void MetadataCache::buildMetadataPriorityQueue(int sfRow)
{
    if (G::isLogger) G::log(CLASSFUNCTION);
    priorityQueue.clear();
    firstIconRow = dm->startIconRange;
    lastIconRow = dm->endIconRange;

    if (debugCaching) {
        qDebug().noquote() << CLASSFUNCTION
                           << "start"
                           << "firstVisible =" << firstIconRow
                           << "lastVisible =" << lastIconRow
                           << "priorityQueue.size() =" << priorityQueue.size()
                           << "sfRowCount =" << sfRowCount
                              ;
    }
    // icon cleanup all icons no longer visible

    // priority alternate ahead/behind until finished
    int behind = sfRow;
    int ahead = sfRow + 1;
    while (behind >= 0 || ahead < sfRowCount) {
        if (behind >= 0) priorityQueue.append(behind--);
        if (ahead < sfRowCount) priorityQueue.append(ahead++);
        if (abort) return;
    }
    if (debugCaching) {
        qDebug().noquote() << CLASSFUNCTION
                           << "start"
                           << "firstVisible =" << firstIconRow
                           << "lastVisible =" << lastIconRow
                           << "priorityQueue.size() =" << priorityQueue.size()
                           << "sfRowCount =" << sfRowCount
                              ;
    }
}

void MetadataCache::readMetadata(QModelIndex sfIdx, QString fPath)
{
//    if (abort) return;
    if (G::isLogger) G::log(CLASSFUNCTION);
    int dmRow = dm->sf->mapToSource(sfIdx).row();
    if (debugCaching) qDebug().noquote() << CLASSFUNCTION << "start  row =" << sfIdx.row();
    QFileInfo fileInfo(fPath);
    if (metadata->loadImageMetadata(fileInfo, true, true, false, true, CLASSFUNCTION)) {
        metadata->m.row = dmRow;
        metadata->m.dmInstance = dmInstance;
//        qDebug() << CLASSFUNCTION << "addToDatamodel: start  row =" << sfIdx.row();
//        if (debugCaching) qDebug().noquote() << CLASSFUNCTION << "start  addToDatamodel"
//                                             << "abort =" << abort
//                                                ;
        if (!abort) emit addToDatamodel(metadata->m);
//        if (debugCaching) qDebug().noquote() << CLASSFUNCTION << "done   addToDatamodel";
//        qDebug() << CLASSFUNCTION << "addToDatamodel: done   row =" << sfIdx.row();
//        dm->addMetadataForItem(metadata->m);
    }
    if (debugCaching) qDebug().noquote() << CLASSFUNCTION << "done row =" << sfIdx.row();
}

void MetadataCache::readIcon(QModelIndex sfIdx, QString fPath)
{
    if (G::isLogger) G::log(CLASSFUNCTION);
    if (debugCaching) {
        qDebug().noquote() << CLASSFUNCTION
                           << "start  row =" << sfIdx.row()
                              ;
    }

    // check if already caching icon (video icons)
    if (dm->isIconCaching(sfIdx.row())) return;

    QModelIndex dmIdx = dm->sf->mapToSource(sfIdx);
    int dmRow = dmIdx.row();
    bool isVideo = dm->index(dmRow, G::VideoColumn).data().toBool();

    // get thumbnail
    QImage image;
    bool thumbLoaded = thumb->loadThumb(fPath, image, "MetadataCache::readIcon");
    if (isVideo) {
        iconsLoaded.append(dmRow);
        return;
    }
    if (thumbLoaded) {
        QPixmap pm = QPixmap::fromImage(image.scaled(G::maxIconSize, G::maxIconSize, Qt::KeepAspectRatio));
        if (!abort) emit setIcon(dmIdx, pm, dmInstance);
        iconMax(pm);
        iconsLoaded.append(dmRow);
    }
    if (debugCaching) {
        qDebug().noquote() << CLASSFUNCTION
                           << "done   row =" << sfIdx.row()
                              ;
    }
}

void MetadataCache::readRow(int sfRow)
{
    if (G::isLogger) G::log(CLASSFUNCTION);
    if (debugCaching) {
        qDebug().noquote() << CLASSFUNCTION
                           << "start  row =" << sfRow
                              ;
    }
    if (sfRow >= dm->sf->rowCount()) return;
    QModelIndex sfIdx = dm->sf->index(sfRow, 0);
    if (!sfIdx.isValid()) return;
    QString fPath = sfIdx.data(G::PathRole).toString();
    if (!G::allMetadataLoaded) {
        bool metaLoaded = dm->sf->index(sfRow, G::MetadataLoadedColumn).data().toBool();
        if (!metaLoaded && !abort) {
            readMetadata(sfIdx, fPath);
        }
    }

    // load icon
    /*
    qDebug() << CLASSFUNCTION
             << "sfRow =" << sfRow
             << "iconsLoaded.size() =" << iconsLoaded.size()
             << "iconChunkSize =" << iconChunkSize
             << "adjIconChunkSize =" << adjIconChunkSize
             ;
    //*/
    if (okToLoadIcon(sfRow) && !abort) {
        readIcon(sfIdx, fPath);
    }

    // update the imageCache item data
    if (!abort) emit addToImageCache(metadata->m);
    if (debugCaching) {
        qDebug().noquote() << CLASSFUNCTION
                           << "done   row =" << sfRow
                              ;
    }
}

void MetadataCache::read()
{
    if (G::isLogger || G::isFlowLogger) G::log(CLASSFUNCTION, src + " action = " + QString::number(action));

    sfRowCount = dm->sf->rowCount();
    if (debugCaching) {
        qDebug().noquote() << CLASSFUNCTION
                           << "src =" << src
                           << " action =" << QString::number(action)
                           << "sfRowCount =" << sfRowCount
                              ;
    }
    if (sfRow >= sfRowCount) return;

    sfStart = sfRow;
    dmInstance = dm->instance;
    emit runStatus(true, true, "MetadataCache::read");

    // build priority queue for reading metadata and icons
//    G::log(CLASSFUNCTION, "Build priority queue");
    buildMetadataPriorityQueue(sfStart);

    // cleanup unneeded icons
//    G::log(CLASSFUNCTION, "Cleanup icons");
    if (!abort) cleanupIcons();

    G::log(CLASSFUNCTION, "Read metadata and icons");
    int n = static_cast<int>(priorityQueue.size());
    for (int i = 0; i < n; i++) {
        if (abort) {
            break;
        }
//        if (G::isLogger || G::isFlowLogger) G::log(CLASSFUNCTION, "i =" + QString::number(i));
        readRow(priorityQueue.at(i));
        if (!G::allMetadataLoaded && !imageCachingStarted && !abort) {
            if (i == (n - 1) || i == 50) {
                if (!abort) emit delayedStartImageCache();
                imageCachingStarted = true;
            }
        }
    }


    emit runStatus(false, true, "MetadataCache::read");
    if (abort) {
        G::log(CLASSFUNCTION, "aborted");
//        abort = false;
        return;
    }
    G::log(CLASSFUNCTION, "Finished without abort");
    if (!abort) emit updateIconBestFit();
    if (!abort) emit done();
}
