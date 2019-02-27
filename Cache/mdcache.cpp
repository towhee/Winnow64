#include "Main/global.h"
#include "Cache/mdcache.h"

MetadataCache::MetadataCache(QObject *parent, DataModel *dm,
                  Metadata *metadata) : QThread(parent)
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    this->dm = dm;
    this->metadata = metadata;
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

void MetadataCache::loadMetadataCache(int startRow, int endRow, bool isShowCacheStatus)
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
    abort = false;

    allMetadataLoaded = false;
    this->startRow = startRow;
    this->endRow = endRow;

    /* Create a map container for every row in the datamodel to track metadata caching.
    This is used to confirm all the metadata is loaded before ending the metadata cache.  If
    the startRow is greater than zero then this means the scroll event has resulted in skipping
    ahead, and we do not want ot lose track of the thumbs already loaded.
    */
    if (startRow == 0) {
        loadMap.clear();
        for(int i = 0; i < dm->rowCount(); ++i) loadMap[i] = false;
    }

    folderPath = dm->currentFolderPath;
    allMetadataLoaded = false;

    this->isShowCacheStatus = isShowCacheStatus;
    if (isShowCacheStatus) emit showCacheStatus(0, true);

    start(TimeCriticalPriority);
}

void MetadataCache::track(QString fPath, QString msg)
{
    if (G::isThreadTrackingOn) qDebug() << G::t.restart() << "\t" << "• Metadata Caching" << fPath << msg;
}

void MetadataCache::loadMetadata()
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
            return;
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
            if (isShowCacheStatus) emit showCacheStatus(row, false);
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
            if (isShowCacheStatus) emit showCacheStatus(row, false);
        }
    }
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

    t.start();
//    qDebug() << "Before" << t.elapsed();
    emit updateIsRunning(true, true, __FUNCTION__);

    mutex.lock();
    int rowCount = dm->rowCount();
    mutex.unlock();

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

        loadMetadata();
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
    while (!allMetadataLoaded);  // && t.elapsed() < 30000);
    emit updateAllMetadataLoaded(allMetadataLoaded);

    qApp->processEvents();
//    qDebug() << "After" << t.elapsed();

    /* After loading metadata it is okay to cache full size images, where the
    target cache needs to know how big each image is (width, height) and the
    offset to embedded full size jpgs */
    emit loadImageCache();

    // update status of metadataThreadRunningLabel in statusbar
    emit updateIsRunning(false, true, __FUNCTION__);

    // update the item counts in Filters
//    emit updateFilterCount();
}
