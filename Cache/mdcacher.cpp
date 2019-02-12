#include "mdcacher.h"

MdCacher::MdCacher(QObject *parent,
                   DataModel *dm,
                   Metadata *metadata,
                   MetaHash *metaHash,
                   ImageHash *iconHash)
            : QThread(parent)
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    this->dm = dm;
    this->metadata = metadata;
    this->metaHash = metaHash;
    this->iconHash = iconHash;
    thumb = new Thumb(this, metadata);
    abort = false;

    // defined in global.h
    thumbMax.setWidth(THUMB_MAX);
    thumbMax.setHeight(THUMB_MAX);
}

MdCacher::~MdCacher()
{
    mutex.lock();
    abort = true;
    condition.wakeOne();
    mutex.unlock();
    wait();
}

void MdCacher::stopMetadateCache()
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
        emit endCaching(thread, allMetadataLoaded);
    }
}

void MdCacher::loadMetadataCache(QVector<ThreadItem> &items,
                                 bool isShowCacheStatus)
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
//        mutex.lock();
        abort = true;
//        condition.wakeOne();
//        mutex.unlock();
        wait();
    }
    abort = false;
    readFailure = 0;

    /* items holds the informaton req'd for this thread
       items.row    = the datamodel row
       items.thread = this thread number (used to report back source)
       items.fPath  = the file to process
    */

    this->items = items;    
    thread = items[0].thread;
    allMetadataLoaded = false;

    /* Create a map container for every row in the list of files to track metadata caching.
    This is used to confirm all the metadata is loaded before ending the metadata cache.
    */
    loadMap.clear();
    for (int i = 0; i < items.count(); ++i) loadMap[i] = false;

    this->isShowCacheStatus = isShowCacheStatus;
    if (isShowCacheStatus) emit showCacheStatus(0, true);

//    qDebug() << "MdCacher::loadMetadataCache  Thread" << items[0].thread;
#ifdef ISTEST
    qDebug() << "Starting Thread" << thread;
#endif
    for (int i = 0; i < items.count(); ++i) {
#ifdef ISTEST
        qDebug() << "Array index =" << i
                 << "items.at(i).thread =" << items.at(i).thread
                 << "items.at(i).row =" << items.at(i).row
                 << "items.at(i).fPath =" << items.at(i).fPath;
#endif
    }

    start(TimeCriticalPriority);
}

void MdCacher::loadMetadata()
{
/*
Load the metadata and thumb (icon) for all the image files assigned to the
thread.
*/
    {
    #ifdef ISDEBUG
    mutex.lock(); G::track(__FUNCTION__); mutex.unlock();
    #endif
    }


    // get metadata for each item assigned to this thread
    int totRows = items.count();
    for (int row = startRow; row < totRows; ++row) {
        if (abort) {
            {
            #ifdef ISDEBUG
            QString s = "Aborting at row = " + QString::number(row);
            mutex.lock(); G::track(__FUNCTION__, s); mutex.unlock();
            #endif
            }
//            emit updateAllMetadataLoaded(allMetadataLoaded);
//            emit updateIsRunning(false, true, __FUNCTION__);
            return;
        }

        // is metadata already cached
        if (loadMap[row]) continue;


        // maybe metadata loaded by user picking image while cache is still building
//        mutex.lock();
//        idx = dm->index(row, 0);
//        QString fPath = idx.data(G::PathRole).toString();
//        bool thumbLoaded = idx.data(Qt::DecorationRole).isValid();
//        bool metadataLoaded = metadata->isLoaded(fPath);
//        mutex.unlock();

        bool thumbLoaded = false;
        bool metadataLoaded = false;

        // file path for this item
        QString fPath = items.at(row).fPath;

        // just in case an empty item
        if (fPath == "") continue;

//        qDebug() << "MdCacher::loadMetadata  Thread" << items[row].thread
//                 << "row" << row
//                 << "dmRow" << items[row].row
//                 << "fPath" << items[row].fPath;

        // update the cache status graphic in the status bar
        if (metadataLoaded && thumbLoaded) {
            loadMap[row] = true;
            if (isShowCacheStatus) emit showCacheStatus(row, false);
            continue;
        }

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
//            mutex.lock();
            if (metadata->loadImageMetadata(fileInfo, true, true, false, true)) {
                // datamodel row so can efficiently update datamodel
                metadata->imageMetadata.row = items[row].row;
                metadataLoaded = true;

#ifdef ISTEST
                qDebug() << "mdcacher thread" << thread << "loading metadata for " << fPath;
#endif
                metaHash->insert(items[row].row, metadata->imageMetadata);

                {
                #ifdef ISDEBUG
                QString s = "Loaded metadata for row " + QString::number(row + 1) + " " + fPath;
                G::track(__FUNCTION__, s);
                #endif
                }
            }
            else {
                qDebug() << "MdCacher thread" << thread << "failed to load metadata for row"
                         << items[row].row << fPath;
            }
        }

        if (!thumbLoaded) {
            QImage image;
            mutex.lock();
            thumbLoaded = thumb->loadThumb(fPath, image);
            mutex.unlock();
            if (thumbLoaded) {
//                qDebug() << "mdcacher thread signaling thread" << thread
//                         << "row =" << items[row].row
//                         << fPath;
//                emit setIcon(items[row].row, image.scaled(THUMB_MAX, THUMB_MAX, Qt::KeepAspectRatio));
                iconHash->insert(items[row].row, image.scaled(THUMB_MAX, THUMB_MAX, Qt::KeepAspectRatio));
            }
            else {
                qDebug() << "MdCacher thread" << thread << "failed to load thumb for row"
                         << items[row].row << fPath;
            }
        }
        else {
            QString s = "Thumb icon not obtained for row " + QString::number(row + 1) + " " + fPath;
            mutex.lock(); G::track(__FUNCTION__, s); mutex.unlock();
        }

/*
        mutex.lock();
        if (metadataLoaded) emit updateDatamodel(metadata->imageMetadata,
                                            items.at(row).thread,
                                            isShowCacheStatus);

        dm->updateMetadataItem(metadata->imageMetadata, thread, isShowCacheStatus);

        metadata->reportMetadata();

        mutex.unlock();

        qDebug() << "Thread" << thread << fPath;
        qDebug() << "createdDate" << metadata->imageMetadata.createdDate;
        qDebug() << "exposureTimeNum" << metadata->imageMetadata.exposureTimeNum;
        qDebug() << "apertureNum" << metadata->imageMetadata.apertureNum;
        qDebug() << "ISONum" << metadata->imageMetadata.ISONum;
        qDebug() << "lens" << metadata->imageMetadata.lens;
        qDebug() << "title" << metadata->imageMetadata.title;
*/


        if (metadataLoaded && thumbLoaded) {
            loadMap[row] = true;
            if (isShowCacheStatus) emit showCacheStatus(row, false);
        }

        if ((row % 20 == 0 && row > 0) || row == totRows - 1) {
            emit processMetadataBuffer();
            emit processIconBuffer();
//            qApp->processEvents(QEventLoop::ExcludeUserInputEvents);
        }
    }
}

void MdCacher::run()
{
/*
Load all metadata, make multiple attempts if not all metadata loaded in the
first pass through the images in the folder.  The QMap loadedMap is used to
track if the metadata and thumb have been cached for each image.  When all items
in loadedMap == true then okay to finish.

While the MdCacher is loading the metadata and thumbs, the user can impact
the process in two ways:

1.  If the user selects an image and the metadata or thumb has not been loaded
    yet then the main program thread goes ahead and loads the metadata and thumb.
    Metadata::loadMetadata checks for this possibility.

2.  If the user scrolls in the gridView or thumbView a signal is fired in MW and
    loadMdCacher is called to load the thumbs for the scroll area.  As a
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
//    emit updateIsRunning(true, true, __FUNCTION__);

//    qDebug() << "MdCacher::run  Thread" << items[0].thread;
//    thread = items[0].thread;

    int rowCount = items.count();
    startRow = 0;

    do {
        if (abort) {
            {
            #ifdef ISDEBUG
            mutex.lock(); G::track(__FUNCTION__, "Aborting ..."); mutex.unlock();
            #endif
            }
            stopMetadateCache();
            return;
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
        qDebug() << "End of while loop in MdCacher::run   allMetadataLoaded =" << allMetadataLoaded;
        if (!allMetadataLoaded) readFailure++;
        if (readFailure > 3) break;
    }
    while (!allMetadataLoaded);  // && t.elapsed() < 30000);
#ifdef ISTEST
    qDebug() << "Exiting thread" << thread;
#endif
    emit endCaching(thread, allMetadataLoaded);

    qApp->processEvents();
}







//    qDebug() << "After" << t.elapsed();

    /* After loading metadata it is okay to cache full size images, where the
    target cache needs to know how big each image is (width, height) and the
    offset to embedded full size jpgs */
//    emit loadImageCache();

    // update status of metadataThreadRunningLabel in statusbar
//    emit updateIsRunning(false, true, __FUNCTION__);

    // update the item counts in Filters
//    emit updateFilterCount();

//    emit finished();
