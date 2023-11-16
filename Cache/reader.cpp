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
    connect(frameDecoder, &FrameDecoder::setFrameIcon, dm, &DataModel::setIconFromVideoFrame);
    //connect(this, &FrameDecoder::abortFrameDecoder, frameDecoder, &FrameDecoder::stop);
    //connect(frameDecoder, &FrameDecoder::stopped, this, &MW::reset);
    thumb = new Thumb(dm, metadata, frameDecoder);
    connect(this, &Reader::setIcon, dm, &DataModel::setIcon, Qt::BlockingQueuedConnection);
    // add metadata to datamodel
    connect(this, &Reader::addToDatamodel,
            dm, &DataModel::addMetadataForItem,
            Qt::BlockingQueuedConnection);
    // add to image cache list
    connect(this, &Reader::addToImageCache,
            imageCache, &ImageCache::addCacheItemImageMetadata,
            Qt::BlockingQueuedConnection);

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
    start();
}

bool Reader::readMetadata()
{
    if (isDebug || G::isLogger) G::log("Reader::readMetadata");
    qDebug() << "Reader::readMetadata" << fPath;
    // read metadata from file into metadata->m
    int dmRow = dmIdx.row();
    QFileInfo fileInfo(fPath);
    bool isMetaLoaded = metadata->loadImageMetadata(fileInfo, instance, true, true, false, true, "Reader::readMetadata");
    if (isMetaLoaded) {
        metadata->m.row = dmRow;
        metadata->m.instance = instance;
        emit addToDatamodel(metadata->m, "Reader::readMetadata");
        if (G::useImageCache) {
            emit addToImageCache(metadata->m);
        }
    }
    return isMetaLoaded;
}

void Reader::readIcon()
{
    if (isDebug || G::isLogger) G::log("Reader::readMetadata");

    QImage image;
    bool thumbLoaded = thumb->loadThumb(fPath, image, instance, "MetaRead::readIcon");
    if (isVideo) return;
    if (thumbLoaded) {
        pm = QPixmap::fromImage(image.scaled(G::maxIconSize, G::maxIconSize, Qt::KeepAspectRatio));
    }
    else {
        pm = QPixmap(":/images/error_image256.png");
        qWarning() << "WARNING" << "MetadataCache::loadIcon" << "Failed to load thumbnail." << fPath;
    }
    emit setIcon(dmIdx, pm, instance, "MetaRead::readIcon");
}

void Reader::run()
{
    if (readMetadata() && isReadIcon) {
        readIcon();
    }
    emit done(threadId);
}
