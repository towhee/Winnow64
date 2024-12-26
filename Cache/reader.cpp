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
    // connect(this, &Reader::setIcon, dm, &DataModel::setIcon, Qt::QueuedConnection);

    isDebug = false;
    debugLog = false;
}

void Reader::read(const QModelIndex dmIdx,
                  const QString filePath,
                  const int instance,
                  const bool isReadIcon)
{
    if (isRunning()) stop();
    t.restart();
    abort = false;
    this->dmIdx = dmIdx;
    fPath = filePath;
    this->instance = instance;
    this->isReadIcon = isReadIcon;
    isVideo = dm->index(dmIdx.row(), G::VideoColumn).data().toBool();
    status = Status::Success;
    pending = true;     // set to false when processed in MetaRead::dispatch
    loadedIcon = false;

    QString fun = "Reader::read";
    if (isDebug)
    {
        qDebug().noquote()
            << fun.leftJustified(30)
            << "id =" << QString::number(threadId).leftJustified(2, ' ')
            << "row =" << QString::number(dmIdx.row()).leftJustified(4, ' ')
            // << "status =" << statusText.at(status)
            // << "isRunning =" << isRunning()
            // << "instanceOk() =" << instanceOk()
            << (fPath.isEmpty() ? "EMPTY PATH" : fPath)
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
    // if (isDebug)
    // {
    //     qDebug() << "Reader::stop commencing"
    //              << threadId
    //              << "isRunning =" << isRunning()
    //         ;
    // }

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
    // fPath = "";

    // if (isDebug)
    // {
    //     qDebug() << "Reader::stop done"
    //              << threadId
    //              << "status =" << statusText.at(status)
    //              << "isRunning =" << isRunning()
    //         ;
    // }
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
    QString fun = "Reader::readMetadata";
    if (isDebug)
    {
        qDebug().noquote()
        << fun.leftJustified(30)
        << "id =" << QString::number(threadId).leftJustified(2, ' ')
        << "row =" << QString::number(dmIdx.row()).leftJustified(4, ' ')
        << (fPath.isEmpty() ? "EMPTY PATH" : fPath)
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

    return isMetaLoaded;
}

void Reader::readIcon()
{
    QString fun = "Reader::readIcon";
    if (isDebug)
    {
        qDebug().noquote()
        << fun.leftJustified(30)
        << "id =" << QString::number(threadId).leftJustified(2, ' ')
        << "row =" << QString::number(dmIdx.row()).leftJustified(4, ' ')
        << (fPath.isEmpty() ? "EMPTY PATH" : fPath)
            ;
    }

    if (fPath.isEmpty()) return;

    int dmRow = dmIdx.row();
    QString msg;
    QImage image;
    // get thumbnail or err.png or generic video
    loadedIcon = thumb->loadThumb(fPath, image, instance, "MetaRead::readIcon");

    if (abort) return;

    // videos set the datamodel icon separately from FrameDecoder
    // if !G::renderVideoThumb then generic Video image returned from Thumb
    if (isVideo && G::renderVideoThumb) return;

    if (loadedIcon) {
        pm = QPixmap::fromImage(image.scaled(G::maxIconSize, G::maxIconSize, Qt::KeepAspectRatio));
        emit setIcon(dmIdx, pm, loadedIcon, instance, "MetaRead::readIcon");
        if (!pm.isNull()) return;
    }

    // failed to load icon, load error icon
    pm = QPixmap(":/images/error_image256.png");
    if (status == Status::MetaFailed) status = Status::MetaIconFailed;
    else status = Status::IconFailed;
    msg = "Failed to load thumbnail.";
    G::issue("Warning", msg, "Reader::readIcon", dmRow, fPath);
}

void Reader::run()
{
    readMetadata();
    readIcon();
    emit done(threadId);
    if (G::isLogger || G::isFlowLogger) G::log("Reader::run", "Finished");
}
