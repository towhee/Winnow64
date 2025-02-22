#include "reader.h"
#include "Main/global.h"

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

    thumb = new Thumb(dm, metadata, frameDecoder);


    bool isBlockingQueuedConnection = false;
    if (isBlockingQueuedConnection) {
        // Try Qt::BlockingQueuedConnection: (can be slow)
        connect(this, &Reader::addToDatamodel, dm, &DataModel::addMetadataForItem, Qt::BlockingQueuedConnection);
        connect(this, &Reader::setIcon, dm, &DataModel::setIcon, Qt::BlockingQueuedConnection);
    } else {
        // Try QueuedConnection
        connect(this, &Reader::addToDatamodel, dm, &DataModel::addMetadataForItem, Qt::QueuedConnection);
        connect(this, &Reader::setIcon, dm, &DataModel::setIcon, Qt::QueuedConnection);
    }

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
        << "isGUI" << G::isGuiThread()
        << (fPath.isEmpty() ? "EMPTY PATH" : fPath)
            ;
    }

    // read metadata from file into metadata->m
    int dmRow = dmIdx.row();
    QFileInfo fileInfo(fPath);
    bool isMetaLoaded = metadata->loadImageMetadata(fileInfo, instance, true, true, false, true, "Reader::readMetadata");
    if (abort) return false;

    #ifdef TIMER
    t2 = t.restart();
    #endif

    metadata->m.row = dmRow;
    metadata->m.instance = instance;
    metadata->m.metadataAttempted = true;
    metadata->m.metadataLoaded = isMetaLoaded;

    // req'd to readIcon, in case it runs before datamodel has been up[dated
    offsetThumb = metadata->m.offsetThumb;
    lengthThumb = metadata->m.lengthThumb;

    if (!abort) emit addToDatamodel(metadata->m, "Reader::readMetadata");

    #ifdef TIMER
    t3 = t.restart();
    #endif

    if (!isMetaLoaded) {
        status = Status::MetaFailed;
        QString msg = "Failed to load metadata.";
        G::issue("Warning", msg, "Reader::readMetadata", dmRow, fPath);
        if (isDebug)
        {
            qDebug().noquote()
            << fun.leftJustified(30)
            << "id =" << QString::number(threadId).leftJustified(2, ' ')
            << "row =" << QString::number(dmIdx.row()).leftJustified(4, ' ')
            << msg
                ;
        }
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

    // pass embedded thumb offset and length in case datamodel not updated
    thumb->presetOffset(offsetThumb, lengthThumb);

    // get thumbnail or err.png or generic video
    loadedIcon = thumb->loadThumb(fPath, image, instance, "MetaRead::readIcon");

    #ifdef TIMER
    t4 = t.restart();
    #endif

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
    if (isDebug)
    {
        qDebug().noquote()
        << fun.leftJustified(30)
        << "id =" << QString::number(threadId).leftJustified(2, ' ')
        << msg
            ;
    }
}

void Reader::run()
{
    readMetadata();
    readIcon();

    #ifdef TIMER
    t5 = t.restart();
    msToRead = t2 + t3 + t4 + t5;

    int dmRow = dmIdx.row();

    /*
    qDebug().noquote()
        << "Reader::run"
        << "row =" << QString::number(dmRow).leftJustified(6)
        << "msToRead =" << QString::number(msToRead).leftJustified(8)
        << "meta  =" << QString::number(t2).leftJustified(6)
        << "meta dm =" << QString::number(t3).leftJustified(6)
        << "icon =" << QString::number(t4).leftJustified(6)
        << "icon dm =" << QString::number(t5).leftJustified(6)
        << dm->index(dmRow, G::NameColumn).data().toString()
        ;//*/

    #endif

    QString fun = "Reader::run emit done";
    if (isDebug)
    {
        qDebug().noquote()
        << fun.leftJustified(30)
        << "id =" << QString::number(threadId).leftJustified(2, ' ')
        << "row =" << QString::number(dmIdx.row()).leftJustified(4, ' ')
        << (fPath.isEmpty() ? "EMPTY PATH" : fPath)
            ;
    }

    emit done(threadId);
    if (G::isLogger || G::isFlowLogger) G::log("Reader::run", "Finished");
}
