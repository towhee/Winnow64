#include "global.h"
#include "mdcache.h"

MetadataCache::MetadataCache(QObject *parent, ThumbView *thumbView,
                  Metadata *metadata) : QThread(parent)
{
    {
    #ifdef ISDEBUG
    qDebug() << "MetadataCache::MetadataCache";
    #endif
    }
    this->thumbView = thumbView;
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
    emit updateIsRunning(true);
    QString fPath;
    int totRows = thumbView->thumbViewModel->rowCount();
    for (int row=0; row < totRows; ++row) {
        if (abort) {
            emit updateIsRunning(false);
            return;
        }
        QModelIndex idx = thumbView->thumbViewModel->index(row, 0, QModelIndex());
        fPath = idx.data(Qt::ToolTipRole).toString();
        // InfoView::updateInfo might have already loaded by getting here first
        // it is executed when the first image is loaded
        if (metadata->isLoaded(fPath)) continue;
        QFileInfo fileInfo(fPath);
        mutex.lock();
        metadata->loadImageMetadata(fileInfo);
        mutex.unlock();
    }
    // after read metadata okay to cache thumbs and images, where the target
    // cache needs to know how big each image is (width, height)
    emit loadThumbCache();
    emit loadImageCache();

    // used to show status in statusbar
    emit updateIsRunning(false);
}


