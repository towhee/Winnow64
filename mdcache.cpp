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
    restart = false;
    abort = false;
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

void MetadataCache::loadMetadataCache()
{
    {
    #ifdef ISDEBUG
    qDebug() << "MetadataCache::loadMetadataCache";
    #endif
    }
    if (!isRunning()) {
        abort = false;
        start(LowPriority);
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

void MetadataCache::run()
{
    {
    #ifdef ISDEBUG
    qDebug() << "MetadataCache::run";
    #endif
    }
    QElapsedTimer t;
    t.start();

    emit updateIsRunning(true);
    QString fPath;
    int thumbCacheThreshold = 20;
    int totRows = dm->rowCount();
    for (int row=0; row < totRows; ++row) {
        if (abort) {
            emit updateIsRunning(false);
            return;
        }
        QModelIndex idx = dm->index(row, 0);
        fPath = idx.data(Qt::ToolTipRole).toString();
        QString s = "Loading metadata " + QString::number(row + 1) + " of " + QString::number(totRows);
        emit updateStatus(false, s);
        // InfoView::updateInfo might have already loaded by getting here first
        // it is executed when the first image is loaded
        if (!metadata->isLoaded(fPath)) {
            QFileInfo fileInfo(fPath);            
//            emit loadImageMetadata(fileInfo);
            mutex.lock();
            metadata->loadImageMetadata(fileInfo, true, true);
            mutex.unlock();
        }
        if (row == thumbCacheThreshold) {
            emit loadThumbCache();
        }
    }
    qDebug() << "Total elapsed time to read metadata =" << t.elapsed() << "ms";

    /* after read metadata okay to cache thumbs and images, where the target
    cache needs to know how big each image is (width, height) and the offset
    to embedded jpgs */
    if (totRows < thumbCacheThreshold) emit loadThumbCache();
    emit loadImageCache();

    // update status in statusbar
    emit updateIsRunning(false);
}


