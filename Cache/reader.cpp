#include "reader.h"

Reader::Reader(QObject *parent,
               int id,
               DataModel *dm,
               ImageCache *imageCache)
    : QThread(parent)
{
    this->dm = dm;
    metadata = new Metadata;
    this->imageCache = imageCache;
    threadId = id;
    instance = 0;
    frameDecoder = new FrameDecoder(this);
    thumb = new Thumb(dm, metadata, frameDecoder);

    connect(frameDecoder, &FrameDecoder::setFrameIcon, dm, &DataModel::setIconFromVideoFrame);
    connect(this, &Reader::addToDatamodel, dm, &DataModel::addMetadataForItem, Qt::BlockingQueuedConnection);
    connect(this, &Reader::setIcon, dm, &DataModel::setIcon, Qt::BlockingQueuedConnection);
    connect(this, &Reader::addToImageCache, imageCache, &ImageCache::addCacheItemImageMetadata, Qt::BlockingQueuedConnection);

    isDebug = false;
}

void Reader::read(const QModelIndex dmIdx,
                  const QString fPath,
                  const int instance,
                  const bool isReadIcon)
{
    this->dmIdx = dmIdx;
    this->fPath = fPath;
    this->instance = instance;
    this->isReadIcon = isReadIcon;
    isVideo = dm->index(dmIdx.row(), G::VideoColumn).data().toBool();
    status = Status::Success;
    pending = true;
    start();
}

void Reader::stop()
{
    if (isRunning()) {
        mutex.lock();
        abort = true;
        condition.wakeOne();
        mutex.unlock();
        //wait();
        abort = false;
    }
    /*
    qDebug() << "Reader::stop              "
             << "id =" << threadId;
    //*/
}

bool Reader::readMetadata()
{
    if (isDebug || G::isLogger) G::log("Reader::readMetadata");
    if (isDebug)
    {
    qDebug() << "Reader::readMetadata              "
             << "id =" << threadId
             << "row =" << dmIdx.row()
             << fPath;
    }
    // read metadata from file into metadata->m
    int dmRow = dmIdx.row();
    QFileInfo fileInfo(fPath);
    bool isMetaLoaded = metadata->loadImageMetadata(fileInfo, instance, true, true, false, true, "Reader::readMetadata");
    if (isMetaLoaded) {
        metadata->m.row = dmRow;
        metadata->m.instance = instance;
        if (!abort) emit addToDatamodel(metadata->m, "Reader::readMetadata");
        if (!dm->isMetadataLoaded(dmRow)) {
            status = Status::MetaFailed;
            qWarning() << "WARNING" << "MetadataCache::readMetadata  row =" << dmRow << "Failed - emit addToDatamodel." << fPath;
        }
    }
    else {
        status = Status::MetaFailed;
        qWarning() << "WARNING" << "MetadataCache::readMetadata  row =" << dmRow << "Failed - metadata not loaded." << fPath;
    }
    return isMetaLoaded;
}

void Reader::readIcon()
{
    if (isDebug || G::isLogger) G::log("Reader::readMetadata");
    if (isDebug)
    {
    qDebug() << "Reader::readIcon                  "
             << "id =" << threadId
             << "row =" << dmIdx.row()
             << fPath;
    }

    int dmRow = dmIdx.row();
    QImage image;
    bool thumbLoaded = thumb->loadThumb(fPath, image, instance, "MetaRead::readIcon");
    if (isVideo) return;
    if (thumbLoaded) {
        pm = QPixmap::fromImage(image.scaled(G::maxIconSize, G::maxIconSize, Qt::KeepAspectRatio));
    }
    else {
        pm = QPixmap(":/images/error_image256.png");
        if (status == Status::MetaFailed) status = Status::MetaIconFailed;
        else status = Status::IconFailed;
        qWarning() << "WARNING" << "MetadataCache::loadIcon  row =" << dmRow << "Failed to load thumbnail." << fPath;
    }
    if (pm.isNull()) {
        if (status == Status::MetaFailed) status = Status::MetaIconFailed;
        else status = Status::IconFailed;
        qWarning() << "WARNING" << "MetadataCache::loadIcon  row =" << dmRow << "Failed - null icon." << fPath;
        return;
    }
    if (!abort) emit setIcon(dmIdx, pm, instance, "MetaRead::readIcon");
    if (!dm->iconLoaded(dmRow, instance)) {
        if (status == Status::MetaFailed) status = Status::MetaIconFailed;
        else status = Status::IconFailed;
        qWarning() << "WARNING" << "MetadataCache::loadIcon  row =" << dmRow << "Failed - emit setIcon." << fPath;
    }
}

void Reader::run()
{
    if (!abort && readMetadata() && isReadIcon) {
        if (!abort) readIcon();
        if (G::useImageCache) {
            if (!abort) emit addToImageCache(metadata->m);
        }
    }
    if (!abort) emit done(threadId);
}
