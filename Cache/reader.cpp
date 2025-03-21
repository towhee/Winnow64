#include "reader.h"
#include "Main/global.h"

Reader::Reader(int id, DataModel *dm, ImageCache *imageCache): QObject(nullptr)
{
    this->dm = dm;
    metadata = new Metadata;
    this->imageCache = imageCache;
    threadId = id;
    instance = 0;

    thumb = new Thumb(dm);

    connect(this, &Reader::addToDatamodel, dm, &DataModel::addMetadataForItem);
    connect(this, &Reader::setIcon, dm, &DataModel::setIcon);

    isDebug = false;
    debugLog = false;
}

void Reader::stop()
{
    QString fun = "Reader::stop";
    if (isDebug) qDebug() << fun.leftJustified(col0Width) << threadId;
    {
        QMutexLocker locker(&mutex);
        abort = true;
        thumb->abortProcessing();
    } // Unlock mutex before waiting

    if (readerThread->isRunning()) {
        readerThread->quit();
        readerThread->wait();
    }
}

void Reader::abortProcessing()
{
    mutex.lock();
    abort = true;
    thumb->abortProcessing();
    mutex.unlock();
}

inline bool Reader::instanceOk()
{
    if (instance != dm->instance)
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
        << "isGUI" << G::isGuiThread()
        << (fPath.isEmpty() ? "EMPTY PATH" : fPath)
            ;
    }

    // read metadata from file into metadata->m
    int dmRow = dmIdx.row();
    QFileInfo fileInfo(fPath);
    bool isMetaLoaded;
    if (!abort) isMetaLoaded = metadata->loadImageMetadata(fileInfo, instance, true, true, false, true, "Reader::readMetadata");
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
    if (abort) return false;

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

    if (fPath.isEmpty()) return;

    int dmRow = dmIdx.row();
    QString msg;
    QImage image;

    // pass embedded thumb offset and length in case datamodel not updated
    if (!abort) thumb->presetOffset(offsetThumb, lengthThumb);

    // get thumbnail or err.png or generic video
    if (!abort) loadedIcon = thumb->loadThumb(fPath, image, instance, "MetaRead::readIcon");

    #ifdef TIMER
    t4 = t.restart();
    #endif

    if (abort) return;

    // videos set the datamodel icon separately from FrameDecoder
    // if !G::renderVideoThumb then generic Video image returned from Thumb
    if (isVideo && G::renderVideoThumb) return;

    if (!abort && loadedIcon) {
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
        << fun.leftJustified(col0Width)
        << "id =" << QString::number(threadId).leftJustified(2, ' ')
        << msg
            ;
    }
}

void Reader::read(QModelIndex dmIdx, QString filePath, int instance, bool isReadIcon)
{
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
        << fun.leftJustified(col0Width)
        << "id =" << QString::number(threadId).leftJustified(2, ' ')
        << "row =" << QString::number(dmIdx.row()).leftJustified(4, ' ')
        << "isGUI =" << G::isGuiThread()
        // << "status =" << statusText.at(status)
        // << "isRunning =" << isRunning()
        // << "instanceOk() =" << instanceOk()
        << (fPath.isEmpty() ? "EMPTY PATH" : fPath)
            ;
    }

    if (!abort) readMetadata();
    if (!abort) readIcon();

    // if (abort) return;
    if (!abort) emit done(threadId);
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
