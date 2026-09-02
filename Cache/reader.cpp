#include "reader.h"
#include "Cache/thumbcache.h"
#include "Cache/catalog.h"
#include "Metadata/indexmetadata.h"
#include "Main/global.h"

Reader::Reader(int id, DataModel *dm, ImageCache *imageCache,
               FrameDecoder *frameDecoder): QObject(nullptr)
{
    this->dm = dm;
    metadata = new Metadata;
    /* Point m at this Reader's own metadata struct up front. readMetadata() also sets it,
       but an icon-only read (needIcon && !needMeta) skips readMetadata, and without this
       m would be a wild pointer on the reader's first such task — loadThumb then crashes
       dereferencing it. See read(). */
    m = &metadata->m;
    this->imageCache = imageCache;
    this->frameDecoder = frameDecoder;  // shared instance owned by MetaRead
    threadId = id;
    instance = 0;

    // Switched from BlockingQueuedConnection to QueuedConnection:
    // BQC forced every metadata/icon emission to wait on the UI thread, so with 14
    // readers × 158 images rapid folder-clicks flooded the UI event queue and froze
    // the beachball. Both slots already reject stale emissions via the instance
    // guard (DataModel::addMetadataForItem line ~1658, DataModel::setIcon1 line ~2397),
    // so non-blocking delivery is safe.
    connect(this, &Reader::addToDatamodel, dm, &DataModel::addMetadataForItem,
            Qt::QueuedConnection);
    connect(this, &Reader::setIcon, dm, &DataModel::setIcon1,
            Qt::QueuedConnection);

    thumb = new Thumb(dm, frameDecoder);

    /* FrameDecoder→DataModel signals are connected once in MetaRead. Here we
       only wire this Reader's videoFrameDecode emission into the shared queue. */
    connect(this, &Reader::videoFrameDecode, frameDecoder, &FrameDecoder::addToQueue);
    connect(this, &Reader::setValDm, dm, &DataModel::setValDm);

    isDebug = false;
    debugLog = false;
}

Reader::~Reader()
{
    // frameDecoder is shared and owned by MetaRead — do not delete here.
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
    if (G::isLogger) G::log(fun, fPath);

    // qDebug().noquote() << fun.leftJustified(col0Width) << "id =" << threadId;

    thumb->abortProcessing();
    // FrameDecoder is shared; flushing it from a single Reader would clobber
    // other Readers' pending video work. MetaRead::abortProcessing handles
    // the global FrameDecoder::stop.

    // Tell worker to stop accepting new work
    QMutexLocker lock(&mutex);
    abort  = true;

    // Now wait until pending == false (or timeout)
    QDeadlineTimer deadline(500);
    while (pending) {
        const qint64 remaining = deadline.remainingTime();
        if (remaining <= 0) {
            // timed out — keep abort=true so the reader exits at next check
            break;
        }
        if (!pendingCondition.wait(&mutex, int(remaining))) {
            // wait returned false = deadline expired; keep abort=true
            break;
        }
    }

    status = Status::Aborted;
}

void Reader::signalAbort()
{
    thumb->abortProcessing();
    // FrameDecoder::stop is driven globally by MetaRead — see abortProcessing.
    QMutexLocker lock(&mutex);
    abort = true;
}

bool Reader::isPending()
{
    QMutexLocker lock(&mutex);
    return pending;
}

QString Reader::fPathSnapshot() const
{
    QMutexLocker lock(&mutex);
    return fPath;
}

void Reader::setPending(bool v)
{
    QMutexLocker lock(&mutex);
    if (pending == v) return;
    pending = v;
    if (!pending) pendingCondition.wakeAll();  // notify waiters
}

inline bool Reader::instanceOk()
{
    /*
    qDebug() << "Reader::instanceOk"
             << "reader instance =" << instance
             << "datamodel instance =" << dm->instance;//*/
    return instance == dm->instance;
}

bool Reader::readMetadataFromIndex(const QFileInfo &fileInfo)
{
/*
    Fill metadata->m from the local index, or return false and leave it alone.

    THE MAPPING ITSELF LIVES IN Metadata/indexmetadata.h, because DataModel::addAllMetadata
    needs the same thing in bulk and a second copy would be a second place that knows how
    a shutter speed is spelled.
*/
    const bool got = IndexMetadata::read(metadata, fileInfo, dmRow, instance);
    if (G::isPerfProbe) {
        (got ? G::probeIndexMetaHits : G::probeIndexMetaMisses)
            .fetch_add(1, std::memory_order_relaxed);
    }
    return got;
}

bool Reader::readMetadata()
{
    QString fun = "Reader::readMetadata";
    if (G::isLogger) G::log(fun, fPath);
    if (isDebug)
    {
        qDebug().noquote()
        << fun.leftJustified(col0Width)
        << "id =" << QString::number(threadId).leftJustified(2, ' ')
            << "row =" << QString::number(dmRow).leftJustified(4, ' ')
        // << "isGUI" << G::isGuiThread()
        << (fPath.isEmpty() ? "EMPTY PATH" : fPath)
            ;
    }

    // read metadata from file into metadata->m
    QFileInfo fileInfo(fPath);
    bool isMetaLoaded = false;

    /*  THE INDEX FIRST, when it can answer. A metadata read is ~20 ms for a raw
        and almost all of it is walking the file's own header; the catalog
        already holds everything that is displayed, sorted, filtered and
        searched. What it does NOT hold is the decode geometry, so a row filled
        this way is marked m.fromIndex and its scratch columns are left unset --
        ImageDecoder reads the header itself at the point it actually decodes.
        The header walk moves from every row at load time to only the rows the
        user opens.

        Default off (G::useIndexMetadata): this changes the contract between the
        loader and the decoder. */
    if (G::useIndexMetadata && !abort) {
        isMetaLoaded = readMetadataFromIndex(fileInfo);
    }

    if (!isMetaLoaded && !abort)
        isMetaLoaded = metadata->loadImageMetadata(fileInfo, dmRow, instance, true, true, false, true, "Reader::readMetadata");
    if (abort) return false;

    #ifdef TIMER
    t2 = t.restart();
    #endif

    m = &metadata->m;
    m->row = dmRow;
    m->instance = instance;
    m->metaStatus = isMetaLoaded ? G::MetaLoaded : G::MetaFailed;
    // Do not set m->metadataReading = true (causes some video repeats 2025-07-04

    // req'd to readIcon, in case it runs before datamodel has been updated
    offsetThumb = m->offsetThumb;
    lengthThumb = m->lengthThumb;

    if (!abort) {
        // Backpressure: bump pending counter; DataModel::addMetadataForItem decrements.
        dm->queuedReaderEvents.fetch_add(1, std::memory_order_relaxed);
        emit addToDatamodel(metadata->m, "Reader::readMetadata");
    }
    if (abort) {status = Status::Aborted; return false;}

    #ifdef TIMER
    t3 = t.restart();
    #endif

    if (!isMetaLoaded) {
        status = Status::MetaFailed;
        QString msg = "Failed to load metadata.";
        G::issueDedup("Warning", msg, "Reader::readMetadata", dmRow, fPath);
        if (isDebug)
        {
            qDebug().noquote()
            << fun.leftJustified(col0Width)
            << "id =" << QString::number(threadId).leftJustified(2, ' ')
            << "row =" << QString::number(dmRow).leftJustified(4, ' ')
            << msg
                ;
        }
    }

    return isMetaLoaded;
}

void Reader::readIcon()
{
/*
    if isVideo emit videoFrameDecode
    else thumb->loadThumb
*/
    QString fun = "Reader::readIcon";
    if (G::isLogger) G::log(fun, fPath);
    if (isDebug)
    {
        qDebug().noquote()
        << fun.leftJustified(col0Width)
        << "id =" << QString::number(threadId).leftJustified(2, ' ')
        << "row =" << QString::number(dmRow).leftJustified(4, ' ')
        << (fPath.isEmpty() ? "EMPTY PATH" : fPath)
            ;
    }

    QElapsedTimer tIcon;
    tIcon.start();

    if (fPath.isEmpty()) {
        qDebug().noquote()
        << fun.leftJustified(col0Width)
        << "id =" << QString::number(threadId).leftJustified(2, ' ')
        << "row =" << QString::number(dmRow).leftJustified(4, ' ')
        << "EMPTY PATH";
        status = Status::IconFailed;
        return;
    }

    QString msg;
    QImage image;

    /* tiff missing embedded thumbnail
    if (m->ext == "tif" && m->isEmbeddedThumbMissing) {
        emit tiffMissingThumbDecode(fPath, dmRow, instance, m->offsetFull);
        return;
    } // */

    // video
    if (isVideo) {
        if (G::renderVideoThumb) {
            /*
            qDebug() << "Reader::readIcon"
                     << fPath
                     << " instance =" << instance
                     << "isReading =" << dm->index(dmRow, G::MetadataReadingColumn).data().toBool()
                ; //*/
            emit videoFrameDecode(fPath, G::maxIconSize, "dmThumb", dmRow, instance);
        }
        return;
    }

    if (abort) {status = Status::Aborted; return;}

    /*  THE INDEX FIRST. This is what the whole thumbnail cache is for: a hit
        replaces opening the file, walking to its embedded preview's segment and
        decoding that, with one indexed read and a small JPEG decode. A miss
        costs one stat and one indexed lookup and falls through unchanged.

        ThumbCache::getImage stands aside in Develop mode and when the preview
        source is Developed, because loadThumb returns a different picture in
        those modes -- see wantsOriginalThumb in Cache/thumbcache.h. */
    if (!abort) {
        image = ThumbCache::instance().getImage(fPath, m && m->developEdited);
        if (!image.isNull()) loadedIcon = true;
    }

    if (abort) {status = Status::Aborted; return;}

    if (!loadedIcon) {
        /*  THE ICON NEEDS THE SEGMENT OFFSETS TOO. A row whose metadata came
            from the index has none -- the catalog stores what is displayed and
            searched, not where the embedded preview lives -- so loadThumb had to
            find the thumbnail without them and, for the formats that depend on
            them, fell back to the error icon. It showed up as an Icon Aspect
            Ratio of exactly 1: error_image256.png is square.

            ImageDecoder::ensureDecodeGeometry covers the DECODE path; this is
            the same gap in the ICON path, and the answer is the same. It costs
            nothing extra here: the cache has already missed, so this thread is
            about to open the file regardless. */
        if (m && m->fromIndex && !abort) {
            QFileInfo fi(fPath);
            if (metadata->loadImageMetadata(fi, dmRow, instance, true, true, false, true,
                                            "Reader::readIcon geometry")) {
                m = &metadata->m;
                offsetThumb = m->offsetThumb;
                lengthThumb = m->lengthThumb;
                /*  USED LOCALLY, NOT PUBLISHED. Emitting addToDatamodel here
                    seemed the tidy thing -- the row would stop being a partial
                    one and the decoder would not repeat the walk -- and it was
                    wrong: DataModel::addMetadataForItem APPENDS to the row's
                    searchable text rather than rebuilding it, so a second call
                    for the same row doubled every field in it. Twenty-three rows
                    in fifty ended with their dimensions and camera twice over,
                    which nothing would have noticed until a search matched the
                    wrong thing.

                    The decoder has its own ensureDecodeGeometry for the same
                    gap, so nothing is lost by keeping this local. */
            }
        }

        // pass embedded thumb offset and length in case datamodel not updated yet
        if (offsetThumb && lengthThumb) thumb->presetOffset(offsetThumb, lengthThumb);

        if (abort) {status = Status::Aborted; return;}

        // get thumbnail or err.png or generic video
        loadedIcon = thumb->loadThumb(fPath, dmRow, image, instance, *m,
                                      "MetaRead::readIcon");
    }

    if (isDebug)
    {
    qDebug().noquote()
        << fun.leftJustified(col0Width)
        << "id =" << QString::number(threadId).leftJustified(2, ' ')
        << "row =" << QString::number(dmRow).leftJustified(4, ' ')
        << "loadedIcon" << loadedIcon
        << "abort =" << abort
        ;
    }

    if (abort) {status = Status::Aborted; return;}

    if (loadedIcon) {
        /*  Cache the thumbnail in the local index BEFORE handing it to the GUI.
            Here rather than at DataModel::setIcon1 because this is already off
            the GUI thread and the image is already in hand -- setIcon1 runs on
            the GUI thread, where a JPEG encode and a database write per icon
            would land squarely on the load path that folder-load latency work
            has repeatedly had to defend. ThumbCache::putImage skips the encode
            it returns immediately, handing the image to one batching writer
            thread -- doing the work inline here was measured at +47% on the
            icon path, see putImage in Cache/thumbcache.h. */
        if (!image.isNull())
            ThumbCache::instance().putImage(fPath, image, m && m->developEdited);

        /* Thumb::loadThumb already scaled to G::maxIconSize (thumbMax), aspect-kept and
           RGB32, so the prior second scale here was a redundant resample + allocation per
           icon. Emit the loaded image directly. */
        if (isDebug)
        qDebug().noquote()
            << fun.leftJustified(col0Width)
            << "id =" << QString::number(threadId).leftJustified(2, ' ')
            << "row =" << QString::number(dmRow).leftJustified(4, ' ')
            << "Emitting setIcon" << "thumb = " << pm << "instance =" << instance;
        // Backpressure: bump pending counter; DataModel::setIcon1 decrements.
        dm->queuedReaderEvents.fetch_add(1, std::memory_order_relaxed);
        emit setIcon(dmRow, image, instance, "MetaRead::readIcon");

        // qint64 msToDecode = tIcon.elapsed();
        qint64 msToDecode = tIcon.nsecsElapsed()/1000;
        // qDebug() << fun << "instance =" << instance << "dmRow =" << dmRow << "microsec =" << msToDecode;
        emit setValDm(dmRow, G::NSThumbColumn, msToDecode, instance,
                      "Reader::readIcon");

        if (!image.isNull()) return;
    }

    // failed to load icon, load error icon
    QImage im = QImage(":/images/error_image256.png");
    dm->queuedReaderEvents.fetch_add(1, std::memory_order_relaxed);
    emit setIcon(dmRow, im, instance, "MetaRead::readIcon");
    if (status == Status::MetaFailed) status = Status::MetaIconFailed;
    else status = Status::IconFailed;
    msg = "Failed to load thumbnail.";
    G::issueDedup("Warning", msg, "Reader::readIcon", dmRow, fPath);
    if (isDebug)
    {
        qDebug().noquote()
        << fun.leftJustified(col0Width)
        << "id =" << QString::number(threadId).leftJustified(2, ' ')
        << "row =" << QString::number(dmRow).leftJustified(4, ' ')
        << msg
            ;
    }
}

void Reader::read(int dmRow, QString filePath, int instance,
                  bool needMeta, bool needIcon)
{
    QString fun = "Reader::read";
    QString need = "needMeta = "  + QVariant(needMeta).toString() +
                   " needIcon = " + QVariant(needIcon).toString() + " ";
    if (G::isLogger) G::log(fun, need + filePath);
    if (isDebug)
        qDebug().noquote() << fun << "instance =" << instance << "dmRow =" << dmRow << need;
    if (filePath.isEmpty()) {
        qWarning().noquote() << fun << "EMPTY FILEPATH";
    }

    t.restart();

    // If a folder change is in progress or memory cap was breached,
    // don't start a new read.
    if (G::stop || dm->abort
        || G::memoryOverrunFlag.load(std::memory_order_relaxed))
    {
        setPending(false);
        // still cycle the reader so MetaRead::dispatch can wind down.
        if (!abort) emit done(threadId, true);
        return;
    }
    abort = false;
    this->dmRow = dmRow;
    {
        QMutexLocker lock(&mutex);
        fPath = filePath;
    }
    this->instance = instance;
    isVideo = dm->index(dmRow, G::VideoColumn).data().toBool();
    status = Status::Success;
    setPending(true);
    loadedIcon = false;
    offsetThumb = 0;
    lengthThumb = 0;

    if (isDebug)
    {
        qDebug().noquote()
        << fun.leftJustified(col0Width)
        << "id =" << QString::number(threadId).leftJustified(2, ' ')
        << "row =" << QString::number(dmRow).leftJustified(4, ' ')
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
    if (!abort && needIcon) {
        /* Icon-only read: readMetadata() was skipped, so m still points at the previous
           row's metadata (or, on the reader's first task, a default-constructed struct).
           Pull this row's embedded-thumb offsets from the DataModel and preset them, so
           loadThumb uses the correct offsets and skips reading them from the stale m. */
        if (!needMeta) {
            offsetThumb = dm->index(dmRow, G::OffsetThumbColumn).data().toUInt();
            lengthThumb = dm->index(dmRow, G::LengthThumbColumn).data().toUInt();
            thumb->presetOffset(offsetThumb, lengthThumb);
        }
        readIcon();
    }

    // cycle backk to MetaRead::dispatchReaders
    bool isReturning = true;
    if (!abort) emit done(threadId, isReturning);

    setPending(false);

    if (G::isLogger) G::log("Reader::read", "Finished");
    fun = "Reader::read done and returning";
    if (isDebug)
    {
        qDebug().noquote()
        << fun.leftJustified(col0Width)
        << "id =" << QString::number(threadId).leftJustified(2, ' ')
        << "row =" << QString::number(dmRow).leftJustified(5, ' ')
        << "ms =" << t.elapsed()
        << fPath
            ;
    }
}
