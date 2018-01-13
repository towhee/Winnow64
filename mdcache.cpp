#include "global.h"
#include "mdcache.h"

MetadataCache::MetadataCache(QObject *parent, DataModel *dm,
                  Metadata *metadata) : QThread(parent)
{
    {
    #ifdef ISDEBUG
    qDebug() << "MetadataCache::MetadataCache";
    #endif
    }
    this->dm = dm;
    this->metadata = metadata;
    thumb = new Thumb(this, metadata);
    restart = false;
    abort = false;
    thumbMax.setWidth(160);       // rgh review hard coding thumb size
    thumbMax.setHeight(160);      // rgh review hard coding thumb size
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
    qDebug() << "MetadataCache::stopMetadateCache";
    #endif
    }
    if (isRunning()) {
        mutex.lock();
        abort = true;
        condition.wakeOne();
        mutex.unlock();
        wait();
        emit updateIsRunning(false);
    }
}

void MetadataCache::track(QString fPath, QString msg)
{
    if (G::isThreadTrackingOn) qDebug() << "• Metadata Caching" << fPath << msg;
}

void MetadataCache::loadMetadataCache(int startRow)
{
    {
    #ifdef ISDEBUG
    qDebug() << "MetadataCache::loadMetadataCache";
    #endif
    }
    allMetadataLoaded = false;
    this->startRow = startRow;
    qDebug() << "MetadataCache::loadMetadataCache  startRow =" << startRow;
    if (!isRunning()) {
        abort = false;
        start(TimeCriticalPriority);
    } else {
        mutex.lock();
        abort = true;
        condition.wakeOne();
        mutex.unlock();
        wait();
        abort = false;
        start(TimeCriticalPriority);
    }
}

bool MetadataCache::loadMetadata()
{
/*
Load the metadata and thumb (icon) for all the image files in a folder.
*/
    {
    #ifdef ISDEBUG
    qDebug() << "MetadataCache::loadMetadata";
    #endif
    }
    qDebug() << "MetadataCache::loadMetadata    startRow ="
             << startRow
             << "  allMetadataLoaded =" << allMetadataLoaded;
    bool metadataLoaded = true;
    int thumbCacheThreshold = 20;
    int totRows = dm->rowCount();
    // if start row > 0 then scroll event from user jumped ahead before all metadata loaded
    int firstRow = startRow;
    // might be unloaded metadata before start row
    if (startRow > 0) metadataLoaded = false;
    for (int row = firstRow; row < totRows; ++row) {
        idx = dm->index(row, 0);
        QString fPath = idx.data(Qt::ToolTipRole).toString();
        if (abort) {
            qDebug() << "Aborting at row =" << row << fPath;
            allMetadataLoaded = false;
            emit updateAllMetadataLoaded(allMetadataLoaded);
            emit updateIsRunning(false);
            return false;
        }
        QString s = "Loading metadata " + QString::number(row + 1) + " of " + QString::number(totRows);
        emit updateStatus(false, s);

        /* InfoView::updateInfo might have already loaded by getting here first
           as it is executed when the first image is loaded  */
        qDebug() << "Attempting to load metadata for row" << row
                 << fPath;
        if (!metadata->isLoaded(fPath)) {
            QFileInfo fileInfo(fPath);
//            emit loadImageMetadata(fileInfo, true, true, false);
            mutex.lock();
            if (!metadata->loadImageMetadata(fileInfo, true, true, false)) {
                metadataLoaded = false;
                qDebug () << "Row =" << row
                          << "Unable to load metadata for" << fPath;
            }
            if (row % thumbCacheThreshold == 0) {
                emit refreshThumbs();
            }
            mutex.unlock();
        }

        bool isThumbLoaded = idx.data(Qt::DecorationRole).isValid();
        if (!isThumbLoaded) {
            QImage image;
            isThumbLoaded = thumb->loadThumb(fPath, image);
//            QImage *imagePtr = &image;
            if (isThumbLoaded) emit setIcon(row, image);
            else metadataLoaded = false;
        }
    }
    startRow = 0;
    return metadataLoaded;
}

void MetadataCache::run()
{
    {
    #ifdef ISDEBUG
    qDebug() << "MetadataCache::run";
    #endif
    }
    t.start();

    emit updateIsRunning(true);

    // Load all metadata, make multiple attempts if not all metadata loaded
    allMetadataLoaded = false;
    do {
        if (abort) {
            qDebug() << "Aborting from MetadataCache::run";
            allMetadataLoaded = false;
            emit updateAllMetadataLoaded(allMetadataLoaded);
            emit updateIsRunning(false);
            return;
        }
        allMetadataLoaded = loadMetadata();
        qDebug() << "MetadataCache::run   elapsed time" << t.elapsed();
    }
    while (!allMetadataLoaded && t.elapsed() < 5000);

    qDebug() << "Total elapsed time to cache metadata =" << t.elapsed() << "ms";

    /* after read metadata okay to images, where the target
    cache needs to know how big each image is (width, height) and the offset
    to embedded jpgs */
    emit loadImageCache();

    // update status in statusbar
    emit updateIsRunning(false);
    emit updateAllMetadataLoaded(allMetadataLoaded);
}


