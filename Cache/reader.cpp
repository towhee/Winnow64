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

    // Try QueuedConnection
    // connect(this, &Reader::addToDatamodel, dm, &DataModel::addMetadataForItem, Qt::QueuedConnection);
    // connect(this, &Reader::setIcon, dm, &DataModel::setIcon, Qt::QueuedConnection);
    // connect(this, &Reader::addToImageCache, imageCache, &ImageCache::updateImageMetadataFromReader, Qt::QueuedConnection);

    // Using Qt::BlockingQueuedConnection:
    connect(this, &Reader::addToDatamodel, dm, &DataModel::addMetadataForItem, Qt::BlockingQueuedConnection);
    connect(this, &Reader::setIcon, dm, &DataModel::setIcon, Qt::BlockingQueuedConnection);
    connect(this, &Reader::addToImageCache, imageCache, &ImageCache::updateCacheItemMetadataFromReader, Qt::BlockingQueuedConnection);

    isDebug = false;
    debugLog = false;
}

void Reader::read(const QModelIndex dmIdx,
                  const QString fPath,
                  const int instance,
                  const bool isReadIcon)
{
    if (isRunning()) stop();
    t.restart();
    abort = false;
    this->dmIdx = dmIdx;
    this->fPath = fPath;
    this->instance = instance;
    this->isReadIcon = isReadIcon;
    isVideo = dm->index(dmIdx.row(), G::VideoColumn).data().toBool();
    status = Status::Success;
    pending = true;     // set to false when processed in MetaRead2::dispatch
    loadedIcon = false;
    if (isDebug)
    {
        qDebug().noquote()
            << "Reader::read            start               "
            << "id =" << QString::number(threadId).leftJustified(2, ' ')
            << "row =" << QString::number(dmIdx.row()).leftJustified(4, ' ')
            << "status =" << statusText.at(status)
            << "isRunning =" << isRunning()
            << "instanceOk() =" << instanceOk()
            ;
    }
    // if (instanceOk())
        start();
}

void Reader::stop()
/*
    Reader uses BlockingQueuedConnections to update the datamodel and imagecache.  This
    conflicts with using wait() - use an event loop instead.
*/
{
    if (isDebug)
    {
        qDebug() << "Reader::stop commencing"
                 << threadId
                 << "isRunning =" << isRunning()
            ;
    }

    if (isRunning()) {
        mutex.lock();
        abort = true;
        condition.wakeOne();
        mutex.unlock();
        /*
        // crashing when rapidly change folders
        mutex.lock();
        abort = true;
        mutex.unlock();
        // BlockingQueuedConnections conflicts with wait()
        QEventLoop loop;
        connect(this, &Reader::finished, &loop, &QEventLoop::quit);
        loop.exec();
        //wait();
        */
    }

    status = Status::Ready;
    pending = false;
    fPath = "";

    if (isDebug)
    {
        qDebug() << "Reader::stop done"
                 << threadId
                 << "status =" << statusText.at(status)
                 << "isRunning =" << isRunning()
            ;
    }
}

inline bool Reader::instanceOk()
{
    if (instance != dm->instance)
    /*
    qDebug() << "Reader::instanceOk"
             << "reader instance =" << instance
             << "datamodel instance =" << dm->instance;//*/
    return instance == dm->instance;
    // return instance == G::dmInstance;
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
             << fPath
            ;
    }
    // read metadata from file into metadata->m
    int dmRow = dmIdx.row();
    QFileInfo fileInfo(fPath);
    bool isMetaLoaded = metadata->loadImageMetadata(fileInfo, instance, true, true, false, true, "Reader::readMetadata");
    if (abort) return false;
    // t2 = t.restart();
    metadata->m.row = dmRow;
    metadata->m.instance = instance;
    metadata->m.metadataAttempted = true;
    metadata->m.metadataLoaded = isMetaLoaded;

    // block until datamodel updated for row with image metadata
    if (!abort) emit addToDatamodel(metadata->m, "Reader::readMetadata");
    // t3 = t.restart();

    if (!dm->isMetadataLoaded(dmRow)) {
        status = Status::MetaFailed;
        QString msg = "Failed to load metadata.";
        G::issue("Warning", msg, "Reader::readMetadata", dmRow, fPath);
    }

    // ImageCache imageCacheList is mirror of dm->sf (current sort/filter)
    int sfRow = dm->proxyRowFromModelRow(dmRow);

    if (isDebug)
    {
        qDebug().noquote()
            << "Reader::readMetadata emit addToImageCache   "
            << "id =" << QString::number(threadId).leftJustified(2, ' ')
            << "dmRow =" << QString::number(dmIdx.row()).leftJustified(4, ' ')
            << "sfRow =" << QString::number(sfRow).leftJustified(4, ' ')
            << fPath
            ;
    }

    if (G::isLogger || G::isFlowLogger) {
        QString msg = "row = " + QString::number(sfRow);
        G::log("Reader::readMetadata", msg);
    }

    if (!abort) emit addToImageCache(sfRow, fPath, instance);

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
             << fPath
            ;
    }

    int dmRow = dmIdx.row();
    QString msg;
    QImage image;
    // get thumbnail or err.png or generic video
    bool thumbLoaded = thumb->loadThumb(fPath, image, instance, "MetaRead::readIcon");

    if (abort) return;

    // videos set the datamodel icon separately from FrameDecoder
    // if !G::renderVideoThumb then generic Video image returned from Thumb
    if (isVideo && G::renderVideoThumb) return;

    if (thumbLoaded) {
        pm = QPixmap::fromImage(image.scaled(G::maxIconSize, G::maxIconSize, Qt::KeepAspectRatio));
    }
    else {
        pm = QPixmap(":/images/error_image256.png");
        if (status == Status::MetaFailed) status = Status::MetaIconFailed;
        else status = Status::IconFailed;
        msg = "Failed to load thumbnail.";
        G::issue("Warning", msg, "Reader::readIcon", dmRow, fPath);
    }
    if (pm.isNull()) {
        if (status == Status::MetaFailed) status = Status::MetaIconFailed;
        else status = Status::IconFailed;
        msg = "Null icon loaded.";
        G::issue("Warning", msg, "Reader::readIcon", dmRow, fPath);
        return;
    }

    // using BlockingQueuedConnection
    // if (dmIdx.row() > 8500) qDebug() << "Reader::readIcon emit setIcon" << dmIdx.row();
    if (!abort) emit setIcon(dmIdx, pm, instance, "MetaRead::readIcon");

    if (!dm->iconLoaded(dmRow, instance)) {
        if (status == Status::MetaFailed) status = Status::MetaIconFailed;
        else status = Status::IconFailed;
        msg = "Failed to load icon.";
        G::issue("Warning", msg, "Reader::loadIcon", dmRow, fPath);
    }
    else {
        loadedIcon = true;
    }

    // loadedIcon = true;
}

void Reader::run()
{
    readMetadata();
    readIcon();
    emit done(threadId);
}
