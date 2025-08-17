#include "reader.h"
#include "Main/global.h"

Reader::Reader(int id, DataModel *dm, ImageCache *imageCache): QObject(nullptr)
{
    this->dm = dm;
    metadata = new Metadata;
    this->imageCache = imageCache;
    threadId = id;
    instance = 0;

    connect(this, &Reader::addToDatamodel, dm, &DataModel::addMetadataForItem);
    connect(this, &Reader::setIcon, dm, &DataModel::setIcon);

    thumb = new Thumb(dm);

    frameDecoder = new FrameDecoder();
    connect(frameDecoder, &FrameDecoder::setFrameIcon, dm, &DataModel::setIconFromVideoFrame);
    connect(this, &Reader::videoFrameDecode, frameDecoder, &FrameDecoder::addToQueue);
    frameDecoderthread = new QThread;
    frameDecoder->moveToThread(frameDecoderthread);
    frameDecoderthread->start();

    tiffThumbDecoder = new TiffThumbDecoder();
    connect(tiffThumbDecoder, &TiffThumbDecoder::setIcon, dm, &DataModel::setIcon);
    connect(this, &Reader::tiffMissingThumbDecode, tiffThumbDecoder, &TiffThumbDecoder::addToQueue);
    tiffThumbDecoderThread = new QThread;
    tiffThumbDecoder->moveToThread(tiffThumbDecoderThread);
    tiffThumbDecoderThread->start();

    isDebug = false;
    debugLog = false;
}

Reader::~Reader()
{
    frameDecoderthread->quit();
    frameDecoderthread->wait();
    tiffThumbDecoderThread->quit();
    tiffThumbDecoderThread->wait();
}

void Reader::stop()
{
    QString fun = "Reader::stop";
    if (isDebug)
        qDebug().noquote() << fun.leftJustified(col0Width) << "id =" << threadId;

    {
        QMutexLocker locker(&mutex);
        abort = true;
        thumb->abortProcessing();
    } // Unlock mutex before waiting

    if (readerThread->isRunning()) {
        readerThread->quit();
        // prevent thread waiting on itself if called from same thread
        if (QThread::currentThread() != readerThread) {
            readerThread->wait();
        }
    }
}

void Reader::abortProcessing()
{
    QString fun = "Reader::abortProcessing";
    // qDebug().noquote() << fun.leftJustified(col0Width) << "id =" << threadId;

    mutex.lock();
    abort = true;
    thumb->abortProcessing();
    status = Status::Aborted;
    pending = false;
    mutex.unlock();
}

inline bool Reader::instanceOk()
{
    /*
    qDebug() << "Reader::instanceOk"
             << "reader instance =" << instance
             << "datamodel instance =" << dm->instance;//*/
    return instance == dm->instance;
}

bool Reader::readMetadata()
{
    QString fun = "Reader::readMetadata";
    if (isDebug)
    {
        qDebug().noquote()
        << fun.leftJustified(col0Width)
        << "id =" << QString::number(threadId).leftJustified(2, ' ')
        << "row =" << QString::number(dmIdx.row()).leftJustified(4, ' ')
        // << "isGUI" << G::isGuiThread()
        << (fPath.isEmpty() ? "EMPTY PATH" : fPath)
            ;
    }

    // read metadata from file into metadata->m
    int dmRow = dmIdx.row();
    QFileInfo fileInfo(fPath);
    bool isMetaLoaded = false;
    if (!abort) isMetaLoaded = metadata->loadImageMetadata(fileInfo, dmRow, instance, true, true, false, true, "Reader::readMetadata");
    if (abort) return false;

    #ifdef TIMER
    t2 = t.restart();
    #endif

    m = &metadata->m;
    m->row = dmRow;
    m->instance = instance;
    m->metadataAttempted = true;
    m->metadataLoaded = isMetaLoaded;
    // Do not set m->metadataReading = true (causes some video repeats 2025-07-04

    // req'd to readIcon, in case it runs before datamodel has been updated
    offsetThumb = m->offsetThumb;
    lengthThumb = m->lengthThumb;

    if (!abort) emit addToDatamodel(metadata->m, "Reader::readMetadata");
    if (abort) {status = Status::Aborted; return false;}

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
            << fun.leftJustified(col0Width)
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
        << fun.leftJustified(col0Width)
        << "id =" << QString::number(threadId).leftJustified(2, ' ')
        << "row =" << QString::number(dmIdx.row()).leftJustified(4, ' ')
        << (fPath.isEmpty() ? "EMPTY PATH" : fPath)
            ;
    }

    if (fPath.isEmpty()) {
        qDebug().noquote()
        << fun.leftJustified(col0Width)
        << "id =" << QString::number(threadId).leftJustified(2, ' ')
        << "row =" << QString::number(dmIdx.row()).leftJustified(4, ' ')
        << "EMPTY PATH";
        status = Status::IconFailed;
        return;
    }

    int dmRow = dmIdx.row();
    QString msg;
    QImage image;

    // tiff missing embedded thumbnail
    if (m->ext == "tif" && m->isEmbeddedThumbMissing) {
        // tiffThumbDecoder->addToQueue(fPath, dmIdx, instance, m->offsetFull);
        emit tiffMissingThumbDecode(fPath, dmIdx, instance, m->offsetFull);
        return;
    }

    // video
    if (isVideo) {
        if (G::renderVideoThumb) {
            /*
            qDebug() << "Reader::readIcon"
                     << fPath
                     << " instance =" << instance
                     << "isReading =" << dm->index(dmRow, G::MetadataReadingColumn).data().toBool()
                ; //*/
            emit videoFrameDecode(fPath, G::maxIconSize, "dmThumb", dmIdx, instance);
        }
        return;
    }

    if (abort) {status = Status::Aborted; return;}

    // pass embedded thumb offset and length in case datamodel not updated yet
    if (offsetThumb && lengthThumb) thumb->presetOffset(offsetThumb, lengthThumb);

    if (abort) {status = Status::Aborted; return;}

    // get thumbnail or err.png or generic video
    loadedIcon = thumb->loadThumb(fPath, dmIdx, image, instance, "MetaRead::readIcon");

    if (isDebug)
    qDebug().noquote()
        << fun.leftJustified(col0Width)
        << "id =" << QString::number(threadId).leftJustified(2, ' ')
        << "row =" << QString::number(dmIdx.row()).leftJustified(4, ' ')
        << "loadedIcon" << loadedIcon
        << "abort =" << abort
        ;

    #ifdef TIMER
    t4 = t.restart();
    #endif

    if (abort) {status = Status::Aborted; return;}

    if (loadedIcon) {
        pm = QPixmap::fromImage(image.scaled(G::maxIconSize, G::maxIconSize, Qt::KeepAspectRatio));
        if (isDebug)
        qDebug().noquote()
            << fun.leftJustified(col0Width)
            << "id =" << QString::number(threadId).leftJustified(2, ' ')
            << "row =" << QString::number(dmIdx.row()).leftJustified(4, ' ')
            << "Emitting setIcon" << "thumb = " << pm << "instance =" << instance;
        emit setIcon(dmIdx, pm, instance, "MetaRead::readIcon");
        if (!pm.isNull()) return;
    }

    // failed to load icon, load error icon
    pm = QPixmap(":/images/error_image256.png");
    emit setIcon(dmIdx, pm, instance, "MetaRead::readIcon");
    if (status == Status::MetaFailed) status = Status::MetaIconFailed;
    else status = Status::IconFailed;
    msg = "Failed to load thumbnail.";
    G::issue("Warning", msg, "Reader::readIcon", dmRow, fPath);
    if (isDebug)
    {
        qDebug().noquote()
        << fun.leftJustified(col0Width)
        << "id =" << QString::number(threadId).leftJustified(2, ' ')
        << "row =" << QString::number(dmIdx.row()).leftJustified(4, ' ')
        << msg
            ;
    }
}

void Reader::read(QModelIndex dmIdx, QString filePath, int instance,
                  bool needMeta, bool needIcon)
{
    QString fun = "Reader::read";
    if (G::isLogger) G::log(fun, filePath);
    if (filePath.isEmpty()) {
        qWarning().noquote() << fun << "EMPTY FILEPATH";
    }
    abort = false;
    this->dmIdx = dmIdx;
    fPath = filePath;
    this->instance = instance;
    isVideo = dm->index(dmIdx.row(), G::VideoColumn).data().toBool();
    status = Status::Success;
    pending = true;     // set to false when processed in MetaRead::dispatch
    loadedIcon = false;
    offsetThumb = 0;
    lengthThumb = 0;

    if (isDebug)
    {
        qDebug().noquote()
        << fun.leftJustified(col0Width)
        << "id =" << QString::number(threadId).leftJustified(2, ' ')
        << "row =" << QString::number(dmIdx.row()).leftJustified(4, ' ')
        << "okReadMeta =" << needMeta
        << "okReadIcon =" << needIcon
        // << "isGUI =" << G::isGuiThread()
        // << "status =" << statusText.at(status)
        // << "isRunning =" << isRunning()
        // << "instanceOk() =" << instanceOk()
        << (fPath.isEmpty() ? "EMPTY PATH" : fPath)
            ;
    }

    if (!abort && needMeta) readMetadata();
    if (!abort && needIcon) readIcon();

    // cycle backk to MetaRead::dispatchReaders
    bool isReturning = true;
    if (!abort) emit done(threadId, isReturning);

    pending = false;

    if (G::isLogger) G::log("Reader::read", "Finished");
    fun = "Reader::read done and returning";
    if (isDebug)
    {
        qDebug().noquote()
        << fun.leftJustified(col0Width)
        << "id =" << QString::number(threadId).leftJustified(2, ' ')
        << "row =" << QString::number(dmIdx.row()).leftJustified(4, ' ')
            ;
    }
}
