#include "metaread.h"

MetaRead::MetaRead(QObject *parent,
                   DataModel *dm,
                   ImageCache2 *imageCacheThread2)
    : QThread(parent)
{
    if (G::isLogger) G::log(__FUNCTION__);
    this->dm = dm;
    this->imageCacheThread2 = imageCacheThread2;
    metadata = new Metadata;
    thumb = new Thumb(this, dm, metadata);
    abort = false;
}

MetaRead::~MetaRead()
{
}

void MetaRead::stop()
{
    // stop metaRead thread
    if (isRunning()) {
        mutex.lock();
        abort = true;
        condition.wakeOne();
        mutex.unlock();
        wait();
        abort = false;
    }
}

void MetaRead::read(int row)
{
    start();
}

void MetaRead::run()
{
    if (abort) stop();
    int dmRow;
    for (dmRow = 0; dmRow < dm->rowCount(); ++dmRow) {
        QString fPath = dm->index(dmRow, 0).data(G::PathRole).toString();
        QFileInfo fileInfo(fPath);
        if (metadata->loadImageMetadata(fileInfo, true, true, false, true, __FUNCTION__)) {
            metadata->m.row = dmRow;
            if (abort) stop();
            emit addToDatamodel(metadata->m);
            emit addToImageCache(metadata->m);
//            dm->addMetadataForItem(metadata->m);
//            imageCacheThread2->addCacheItem(metadata->m);
        }
        // load icon
        QModelIndex idx = dm->sf->index(dmRow, 0);
        if (idx.isValid() && idx.data(Qt::DecorationRole).isNull()) {
            int dmRow = dm->sf->mapToSource(idx).row();
            QImage image;
            QString fPath = idx.data(G::PathRole).toString();
            /*
            if (G::isTest) QElapsedTimer t; if (G::isTest) t.restart();
            bool thumbLoaded = thumb->loadThumb(fPath, image);
            if (G::isTest) qDebug() << __FUNCTION__ << "Load thumbnail =" << t.nsecsElapsed() << fPath;
            */
            bool thumbLoaded = thumb->loadThumb(fPath, image, "MetadataCache::readIconChunk");
            QPixmap pm;
            if (thumbLoaded) {
                pm = QPixmap::fromImage(image.scaled(G::maxIconSize, G::maxIconSize, Qt::KeepAspectRatio));
                dm->itemFromIndex(dm->index(dmRow, 0))->setIcon(pm);
//                iconMax(pm);
//                iconsCached.append(dmRow);
            }
        }
    }
    if (abort) stop();
//    qDebug() << __FUNCTION__ << "reader emit done" << threadId;
    emit completed();
}
