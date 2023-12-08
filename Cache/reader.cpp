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
    abort = false;
    this->dmIdx = dmIdx;
    this->fPath = fPath;
    this->instance = instance;
    this->isReadIcon = isReadIcon;
    isVideo = dm->index(dmIdx.row(), G::VideoColumn).data().toBool();
    status = Status::Success;
    pending = true;
    loadedIcon = false;
    start();
    if (isDebug) {
        qDebug().noquote()
            << "Reader::read            start               "
            << "id =" << QString::number(threadId).leftJustified(2, ' ')
            << "row =" << QString::number(dmIdx.row()).leftJustified(4, ' ')
            << "isRunning =" << isRunning()
            ;
    }
}

void Reader::stop()
{
    //qDebug() << "Reader::stop" << threadId << "isRunning =" << isRunning();
    if (isRunning()) {
        mutex.lock();
        abort = true;
        mutex.unlock();
    }
}

bool Reader::readMetadata()
{
    if (G::isLogger) G::log("Reader::readMetadata");
    if (isDebug)
    {
    qDebug().noquote()
             << "Reader::readMetadata                        "
             << "id =" << QString::number(threadId).leftJustified(2, ' ')
             << "row =" << QString::number(dmIdx.row()).leftJustified(4, ' ')
             //<< fPath
            ;
    }
    // read metadata from file into metadata->m
    int dmRow = dmIdx.row();
    QFileInfo fileInfo(fPath);
    bool isMetaLoaded = metadata->loadImageMetadata(fileInfo, instance, true, true, false, true, "Reader::readMetadata");
    if (abort) return false;
    if (isMetaLoaded) {
        metadata->m.row = dmRow;
        metadata->m.instance = instance;
        if (!abort) emit addToDatamodel(metadata->m, "Reader::readMetadata");
        if (!dm->isMetadataLoaded(dmRow)) {
            status = Status::MetaFailed;
            qWarning() << "WARNING" << "MetadataCache::readMetadata  row =" << dmRow << "Failed - emit addToDatamodel." << fPath;
        }
        if (!abort) emit addToImageCache(metadata->m, instance);
    }
    else {
        status = Status::MetaFailed;
        qWarning() << "WARNING" << "MetadataCache::readMetadata  row =" << dmRow << "Failed - metadata not loaded." << fPath;
    }
    return isMetaLoaded;
}

void Reader::readIcon()
{
    if (G::isLogger) G::log("Reader::readIcon");
    if (isDebug)
    {
    qDebug().noquote()
             << "Reader::readIcon                            "
             << "id =" << QString::number(threadId).leftJustified(2, ' ')
             << "row =" << QString::number(dmIdx.row()).leftJustified(4, ' ')
             //<< fPath
            ;
    }

    int dmRow = dmIdx.row();
    QImage image;
    bool thumbLoaded = thumb->loadThumb(fPath, image, instance, "MetaRead::readIcon");
    if (abort) return;
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
    else {
        loadedIcon = true;
    }
}

void Reader::run()
{
    if (!abort && !G::allMetadataLoaded) readMetadata();
    if (!abort && isReadIcon) readIcon();
    if (isDebug) {
    qDebug().noquote()
             << "Reader::run             emiting done        "
             << "id =" << QString::number(threadId).leftJustified(2, ' ')
             << "row =" << QString::number(dmIdx.row()).leftJustified(4, ' ')
             //<< fPath
            ;
    }
    if (!abort) emit done(threadId);
}
