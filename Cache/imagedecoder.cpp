#include "imagedecoder.h"
#include "Main/global.h"
#include "ImageFormats/Raw/rawformat.h"
#include "Develop/develop.h"
#include "Develop/inputtransform.h"
#include "Develop/outputtransform.h"
#include "Develop/workingimagecache.h"
#include <memory>

#ifdef Q_OS_MAC
// Defined in Image/thumb_mac.mm — fast HEIC primary-image decode via ImageIO.
bool macImageIOImage(const QString &fPath, QImage &out);
#endif

/*
   This class decouples the asyncronous image reading (in CacheImage) from the synchronous
   image decoding, which is executed in multiple ImageDecoder threads.

   CacheImage::decode receives the file information and metadata.  Depending on the type of
   image, either a QByteArray buffer is loaded (JPG and other Qt library formats), the QFile
   is mapped to memory (TIF) or the file path is used (HEIC).  Using a buffer or mapping the
   QFile improves performance.  ImageDecoder::run is started.

   The appropriate decoder: decodeJpg, decodeTif, decodeHeic or decodeUsingQt is called.  The
   resulting QImage rotation and color manage are checked.  Done is emitted, signalling
   ImageCache::fillCache, where the QImage is inserted into the Image cache, and if there are
   still more images to cache, CacheImage is called and the cycle continues.

   decode
   load

*/

ImageDecoder::ImageDecoder(int id,
                           DataModel *dm,
                           Metadata *metadata)
    : QObject(nullptr)
{
    if (isDebug) G::log("ImageDecoder::ImageDecoder", "Thread " + QString::number(id));

    // connect(&decoderThread, &QThread::started, this, &ImageDecoder::run);
    // connect(this, &ImageDecoder::done, &decoderThread, &QThread::quit);

    threadId = id;
    // status = Status::Ready;
    fPath = "";
    sfRow = -1;
    instance = 0;
    this->dm = dm;
    this->metadata = metadata;
    isDebug = false;
    isLog = false;

    connect(this, &ImageDecoder::setValSf, dm, &DataModel::setValSf, Qt::QueuedConnection);
}

ImageDecoder::~ImageDecoder()
{
    stop();
}

void ImageDecoder::stop()
{
    QString fun = "ImageDecoder::stop";
    if (isDebug)
    {
        qDebug().noquote()
            << fun.leftJustified(50);
    }

    abort.storeRelease(1);
    if (decoderThread.isRunning()) {
        decoderThread.quit();
        // decoderThread.wait();
    }
}

bool ImageDecoder::quit()
{
    abort.storeRelease(0);
    // status = Status::Abort;
    fPath = "";
    // QImage blank;
    // image = QImage();
    sfRow = -1;
    return false;
}

void ImageDecoder::abortProcessing()
{
    QString fun = "ImageDecoder::abortProcessing";
    if (G::isLogger)
        G::log(fun, "id = " + QString::number(threadId));
    if (isDebug)
    {
        qDebug().noquote()
        << fun.leftJustified(50);
    }

    // Non-blocking: set the atomic abort flag and return. The decoder thread
    // checks this flag at its iteration points and will quiesce on its own.
    // The previous wait-on-condition pattern held the decoder's mutex from a
    // foreign thread (the imageCache thread), creating a double-lock hazard
    // with this decoder's own setIdle/setBusy under load. Late results from
    // an in-flight decode are filtered downstream by the instance check.
    abort.storeRelease(1);

    if (G::isLogger)
        G::log(fun, "id = " + QString::number(threadId) + " aborted.");
}

void ImageDecoder::setIdle(bool v)
{
    QMutexLocker lock(&mutex);
    if (idle == v) return;
    idle = v;
    if (idle) idleCondition.wakeAll();  // notify waiters
}

bool ImageDecoder::isRunning() const
{
    return running.loadRelaxed();
}

void ImageDecoder::decode(int row, int instance)
{
    abort.storeRelease(0);
    sfRow = row;                   // set early so fillCache has valid row
    this->instance = instance;

    if (dm->sf->isSuspended() || row >= dm->sf->rowCount()) {
        status = Status::Failed;
        errMsg = dm->sf->isSuspended() ? "Proxy suspended." : "Row out of range.";
        setIdle();
        emit done(threadId, int(status), sfRow, QImage(), QString(), 0);
        return;
    }

    QElapsedTimer t;
    t.start();

    setBusy();

    status = Status::Undefined;
    fPath = dm->sf->index(sfRow,0).data(G::PathRole).toString();
    image = QImage();
    errMsg = "";
    if (isLog || G::isLogger) G::log("ImageDecoder::decode", "sfRow = " + QString::number(sfRow));

    QString fun = "ImageDecoder::decode";
    if (isDebug)
    {
        qDebug().noquote()
            << fun.leftJustified(50)
            << "decoder" << QVariant(threadId).toString().leftJustified(3)
            << "row =" << QString::number(sfRow).leftJustified(4)
            << fPath;
    }

    // instance check
    if (instance != dm->instance) {
        status = Status::InstanceClash;
        errMsg = "Instance clash.  New folder selected, processing old folder.";
        G::issueDedup("Comment", errMsg, "ImageDecoder::run", sfRow, fPath);
        setIdle();
        emit done(threadId, int(status), sfRow, QImage(), fPath, 0);
        if (isDebug)
        {
            QString fun = "ImageDecoder::decode instance clash";
            qDebug().noquote()
                << fun.leftJustified(50)
                << "decoder" << QVariant(threadId).toString().leftJustified(3)
                << "sfRow =" << QString::number(sfRow).leftJustified(4)
                << "errMsg =" << errMsg
                << "decoder[id]->fPath =" << fPath
                ;
        }
        return;
    }

    // decode
    if (!abort.loadAcquire() && load()) {
        // if (isDebug) G::log("ImageDecoder::run (if load)", "Image width = " + QString::number(image.width()));
        if (isDebug)
        {
            QString fun = "ImageDecoder::decode loaded";
            qDebug().noquote()
                << fun.leftJustified(50)
                << "decoder" <<  QString::number(threadId).leftJustified(3)
                << "row =" <<  QString::number(sfRow).leftJustified(4)
                << "DecoderToUse:" << decodersText.at(decoderToUse)
                << "ms =" << t.elapsed()
                << fPath;
        }
        if (metadata->rotateFormats.contains(ext) && !abort.loadAcquire()) rotate();
        if (!abort.loadAcquire()) applyDevelop();
        if (G::colorManage && !abort.loadAcquire()) colorManage();
        if (image.isNull()) status = Status::Failed;
    }
    else {
        if (isDebug)
        {
            QString fun = "ImageDecoder::decode load failed";
            qDebug() << fun.left(50)
                     << "row =" << sfRow
                     << "status =" << statusText.at(status)
                     << "errMsg =" << errMsg
                     << fPath
                ;
        }
    }

    setIdle();

    nsToDecode = t.nsecsElapsed();

    /*
    qDebug() << fun.left(50)
             << "row =" << sfRow
             << "nsToDecode =" << nsToDecode
             << fPath
        ;//*/

    emit setValSf(sfRow, G::NSImageColumn, nsToDecode, instance,
                  "ImageDecoder::decode", Qt::EditRole,
                  int(Qt::AlignRight | Qt::AlignVCenter));

    emit done(threadId, int(status), sfRow, image, fPath, nsToDecode);
}

bool ImageDecoder::load()
{
/*
    Loads a full size preview into a QImage.  It is invoked from ImageCache::fillCache.
    NOTE: calls to metadata and dm to not appear to impact performance.
*/
    if (isLog || G::isLogger) G::log("ImageDecoder::load",
               "sfRow = " + QString::number(sfRow) + "  " + fPath);
    QString fun = "ImageDecoder::load";
    if (isDebug)
    {
        QString fun = "ImageDecoder::load";
        qDebug().noquote()
            << fun.leftJustified(50)
            << "decoder" <<  QString::number(threadId).leftJustified(3)
            << "row =" << QString::number(sfRow).leftJustified(4)
            ;
    }

    // blank fPath when caching is cycling, waiting to finish.
    if (fPath == "") {
        errMsg = "Blank file path.";
        G::issue("Warning", errMsg, "ImageDecoder::load", sfRow, fPath);
        status = Status::BlankFilePath;
        return false;
    }

    // confirm file exists (the file or folder could have been deleted externally)
    if (!QFile(fPath).exists()) {
        status = Status::NoFile;
        return false;
    }

    ext = isIndependent
              ? indMeta.ext.toLower()
              : dm->sf->index(sfRow, G::TypeColumn).data().toString().toLower();

    // do not cache video files
    if (metadata->videoFormats.contains(ext)) {
        status = Status::Video;
        return false;
    }

    QFile imFile(fPath);

    // is file already open by another process
    if (imFile.isOpen()) {
        errMsg = "File already open.";
        G::issue("Warning", errMsg, "ImageDecoder::load", sfRow, fPath);
        status = Status::FileOpen;
        return false;
    }

    // try to open image file
    if (!imFile.open(QIODevice::ReadOnly)) {
        errMsg = "Unable to open file.";
        G::issue("Warning", errMsg, "ImageDecoder::load", sfRow, fPath);
        status = Status::FileOpen;
        return false;
    }

    decoderToUse = QtImage;  // default unless overridden
    developApplied = false;  // reset per decode; the RAW path sets it when it develops

    /*
        FULL-SENSOR RAW DECODE. RawFormat::Create() returns nullptr for formats with no
        sensor decoder yet, so those RAW files fall through to the embedded-JPG path
        below unchanged. When a decoder exists this is the call site that produces a
        demosaiced image. The decode needs the raw-sensor fields (RawSensorInfo): in
        independent mode indMeta already carries them; in cache mode we fetch them from
        the DataModel's per-file store (populated during the metadata read) via the
        lock-guarded getter -- cheaper and thread-safer than rebuilding the whole
        ImageMetadata, since UnpackCfa only consults rawInfo.
    */
    if (G::useRaw) {
        if (std::unique_ptr<RawFormat> rawFormat = RawFormat::Create(ext)) {
            ImageMetadata rawMeta;
            if (isIndependent) rawMeta = indMeta;
            else {
                dm->fPathRawInfoGet(fPath, rawMeta.rawInfo);
                rawMeta.ISONum = dm->sf->index(sfRow, G::ISOColumn).data().toInt();  // "Denoise raw" conditioning
            }
            std::shared_ptr<const WorkingImage> work;
            if (rawFormat->Decode(imFile, rawMeta, image, &editParams, &abort, &work)) {
                decoderToUse = Raw;
                developApplied = true;   // RAW develops internally; skip the generic pass
                /* Cache the pre-develop WorkingImage so a later edit re-renders without
                   re-decoding/re-demosaicing (UnpackCfa+Demosaic+RawColor is the costly part;
                   Develop+OutputTransform that follow are cheap). */
                WorkingImageCache::instance().put(fPath, work);
                imFile.close();
                status = Status::Success;
                emit setValSf(sfRow, G::RawRenderColumn, true, instance,
                              "ImageDecoder::load", Qt::EditRole,
                              int(Qt::AlignRight | Qt::AlignVCenter));
                return true;
            }
            /* Aborted mid-decode (shutdown / navigation): bail now rather than wasting
               time on the embedded-JPG fallback, whose result would be discarded anyway. */
            if (abort.loadAcquire()) { imFile.close(); return false; }
            errMsg = rawFormat->lastError();
            imFile.seek(0);   // rewind for the embedded-JPG fallback below
        }
    }

    // Embedded jpg?
    bool isEmbeddedJpg = false;
    int offsetFull = isIndependent
                         ? int(indMeta.offsetFull)
                         : dm->sf->index(sfRow, G::OffsetFullColumn).data().toInt();
    int lengthFull = isIndependent
                         ? int(indMeta.lengthFull)
                         : dm->sf->index(sfRow, G::LengthFullColumn).data().toInt();
    // embedded image type but no offset
    if (metadata->hasJpg.contains(ext) && offsetFull == 0) {

    }
    // raw file or jpg
    if (metadata->hasJpg.contains(ext) || (ext == "jpg" && offsetFull)) {
        isEmbeddedJpg = true;
    }
    // heic saved as a jpg
    if (metadata->hasHeic.contains(ext) && lengthFull) {
        isEmbeddedJpg = true;
    }

    /**************************************************************************/
    // EMBEDDED JPG FORMAT (RAW FILES AND SOME HEIC)
    if (isEmbeddedJpg) {
        // make sure legal offset by checking the length
        if (lengthFull == 0) {
            errMsg = "Could not read embedded JPG because length = 0.";
            G::issue("Warning", errMsg, "ImageDecoder::load", sfRow, fPath);
            imFile.close();
            status = Status::Invalid;
            return false;
        }

        // can we read the file
        if (!imFile.seek(offsetFull)) {
            errMsg = "Could not read embedded JPG because offset is invalid.";
            G::issue("Warning", errMsg, "ImageDecoder::load", sfRow, fPath);
            imFile.close();
            status = Status::Invalid;
            return false;
        }

        // read the embedded image into buf
        QByteArray buf = imFile.read(lengthFull);
        if (buf.length() == 0) {
            errMsg = "Could not read embedded JPG because buffer length = 0.";
            G::issue("Warning", errMsg, "ImageDecoder::load", sfRow, fPath);
            imFile.close();
            status = Status::Invalid;
            return false;
        }

        // SELECT DECODER TO USE

        // only Qt image library available in windows code
        #ifdef Q_OS_WIN
        decoderToUse = QtImage;
        #endif

        #ifdef Q_OS_MAC
        decoderToUse = QtImage;            // QtImage or TurboJpg or Rory
        if (decoderToUse == TurboJpg) {
            JpegTurbo jpegTurbo;
            image = jpegTurbo.decode(buf);
            if (image.isNull()) {
                errMsg = "Could not read JPG because JpegTurbo::decode failed.";
                G::issue("Warning", errMsg, "ImageDecoder::load", sfRow, fPath);
                // try Qt decoder
                decoderToUse = QtImage;
            }
        }
        #endif

        if (decoderToUse == QtImage) {
            if (!image.loadFromData(buf, "JPEG")) {
                errMsg = "Could not read JPG because QImage::loadFromData failed.";
                G::issue("Warning", errMsg, "ImageDecoder::load", sfRow, fPath);
                imFile.close();
                status = Status::Invalid;
                return false;
            }
        }
    }

    /**************************************************************************/
    // HEIC format
    else if (metadata->hasHeic.contains(ext)) {
        #ifdef Q_OS_WIN
        Heic heic;
        if (!heic.decodePrimaryImage(fPath, image)) {
            QString errMsg = "heic.decodePrimaryImage failed.";
            G::issue("Error", errMsg, "ImageDecoder::load", sfRow, fPath);
            imFile.close();
            status = Status::Invalid;
            return false;
        }
        /*
        qDebug() << "ImageDecoder::load" << "HEIC image" << image.width() << image.height();
        //*/
        #endif

        #ifdef Q_OS_MAC
        imFile.close();

        // Fast path: Apple's ImageIO uses the platform's hardware HEVC
        // decoder, faster than QImage::load() round-tripping through Qt's
        // image plugin layer.
        bool ok = false;
        // ok = macImageIOImage(fPath, image) && image.width() > 0;

        // Fallback: Qt's HEIF plugin.
        if (!ok) ok = image.load(fPath) && image.width() > 0;

        if (!ok) {
            errMsg = "Could not read heic image (ImageIO and QImage::load failed).";
            G::issue("Warning", errMsg, "ImageDecoder::load", sfRow, fPath);
            status = Status::Invalid;
            return false;
        }
        if (isDebug)
        {
        qDebug() << "ImageDecoder::load" << "HEIC image"
                 << image.width() << image.height()
                 << fPath
            ;
        }
        #endif
    }

    /**************************************************************************/
    // TIFF format
    else if (ext == "tif") {

        /* decoder options:
           QtImage      use QImage::load
           QtTiff       use QTiffHandler override - overall best results incl jpg compression
           LibTiff      use libtiff library directly
           Rory         use Rory decoder
        */

        decoderToUse = QtTiff;            // works for all
        // decoderToUse = Rory;           // works for Zerene, not jpg compression
        // decoderToUse = LibTiff;        // works for all
        // decoderToUse = QtImage;        // works for all except jpg compression

        #ifdef Q_OS_MAC
        if (decoderToUse == LibTiff) {
            class LibTiff libTiff;
            image = libTiff.readTiffToQImage(fPath);
            // is valid? Assign status = Status::Invalid
        }
        #endif

        if (decoderToUse == Rory) {
            // check for sampling format we cannot read
            int samplesPerPixel = isIndependent
                ? indMeta.samplesPerPixel
                : dm->sf->index(sfRow, G::samplesPerPixelColumn).data().toInt();
            if (samplesPerPixel > 3) {
                 errMsg = "TIFF samplesPerPixel more than 3.";
                 G::issue("Warning", errMsg, "ImageDecoder::run", sfRow, fPath);
                 imFile.close();
                 status = Status::Invalid;
                 return false;
            }

            // try Winnow decoder
            Tiff tiff("ImageDecoder::load Id = " + QString::number(threadId));
            if (!tiff.decode(fPath, offsetFull, image)) {
                decoderToUse = QtImage;     // maybe change to QtTiff?
                /*
                qDebug() << "ImageDecoder::load "
                         << "Could not decode using Winnow Tiff decoder.  row =" << n.key <<
                            "Trying Qt tiff library to decode " + fPath + ". ";  //*/
            }
        }

        if (decoderToUse == QtImage) {
            // use Qt tiff decoder
            imFile.close();
            if (!image.load(fPath)) {
                errMsg = "Could not read because Qt tiff decoder failed.";
                G::issue("Error", errMsg, "ImageDecoder::load", sfRow, fPath);
                imFile.close();
                status = Status::Invalid;
                return false;
            }
        }

        if (decoderToUse == QtTiff) {
            // override QTiffHandler code
            Tiff tiff("ImageDecoder::load");
            // qDebug() << "ImageDecoder::load decoderToUse == QtTiff" << fPath;
            if (!tiff.read(fPath, &image)) {
                errMsg = "Could not read because QTiff decoder failed.";
                G::issue("Error", errMsg, "ImageDecoder::load", sfRow, fPath);
                imFile.close();
                status = Status::Invalid;
                return false;
            }
        }
    }

    /**************************************************************************/
    // JPEG format
    else if (ext == "jpg" || ext == "jpeg") {
        #ifdef Q_OS_WIN
        decoderToUse = QtImage;
        #endif
        #ifdef Q_OS_MAC
        decoderToUse = QtImage;            // QtImage or TurboJpg or Rory
        if (decoderToUse == TurboJpg) {
            JpegTurbo jpegTurbo;
            image = jpegTurbo.decode(fPath);
            if (image.isNull()) {
                errMsg = "Could not read because TurboJpg decoder failed.";
                G::issue("Warning", errMsg, "ImageDecoder::load", sfRow, fPath);
                // try using QImage
                decoderToUse = QtImage;
            }
        }
        #endif

        if (decoderToUse == Rory) {
            Jpeg jpeg;
            jpeg.decodeScan(imFile, image);
            if (image.isNull()) {
                errMsg = "Could not read because Rory decoder failed.";
                G::issue("Warning", errMsg, "ImageDecoder::load", sfRow, fPath);
                imFile.close();
                status = Status::Invalid;
                return false;
            }
        }

        if (decoderToUse == QtImage) {
            imFile.close();
            image.load(fPath);  // crash 2024-11-23 EXC_BAD_ACCESS (SIGSEGV)
            if (image.isNull()) {
                errMsg = "Could not read because Qt decoder failed.";
                G::issue("Warning", errMsg, "ImageDecoder::load", sfRow, fPath);
                // imFile.close();
                status = Status::Invalid;
                return false;
            }
        }
    }

    /**************************************************************************/
    // All other formats
    else {
        imFile.close();

        bool ok = false;
        // Qt Image Library.
        if (!ok) ok = image.load(fPath);

        #ifdef Q_OS_MAC
        // Fallback: Apple ImageIO
        if (!ok) ok = macImageIOImage(fPath, image) && image.width() > 0;
        #endif

        if (!ok) {
            errMsg = "Could not read image (decode failed).";
            G::issue("Warning", errMsg, "ImageDecoder::load", sfRow, fPath);
            status = Status::Invalid;
            return false;
        }
    }

    // image loaded, check for null image
    if (image.width() == 0 || image.height() == 0) {
        status = Status::Invalid;
        return false;
    }

    imFile.close();
    status = Status::Success;
    return true;
}

void ImageDecoder::setIdle()
{
    QMutexLocker lock(&mutex);
    if (idle) return;
    idle = true;
    idleCondition.wakeAll();  // notify waiters
}

void ImageDecoder::setBusy()
{
    QMutexLocker lock(&mutex);
    idle = false;
}

bool ImageDecoder::isIdle()
{
    QMutexLocker lock(&mutex);
    return idle;
}

bool ImageDecoder::isBusy()
{
    QMutexLocker lock(&mutex);
    return !idle;
}

void ImageDecoder::rotate()
{
    if (G::isLogger) G::log("ImageDecoder::rotate", "sfRow = " + QString::number(sfRow));
    QTransform trans;
    int degrees = 0;
    int orientation = isIndependent
        ? indMeta.orientation
        : dm->sf->index(sfRow, G::OrientationColumn).data().toInt();
    int rotationDegrees = isIndependent
        ? indMeta.rotationDegrees
        : dm->sf->index(sfRow, G::RotationDegreesColumn).data().toInt();
    if (orientation > 0) {
        switch (orientation) {
        case 3:
            degrees = rotationDegrees + 180;
            if (degrees > 360) degrees = degrees - 360;
            trans.rotate(degrees);
            image = image.transformed(trans, Qt::SmoothTransformation);
            break;
        case 6:
            degrees = rotationDegrees + 90;
            if (degrees > 360) degrees = degrees - 360;
            trans.rotate(degrees);
            image = image.transformed(trans, Qt::SmoothTransformation);
            break;
        case 8:
            degrees = rotationDegrees + 270;
            if (degrees > 360) degrees = degrees - 360;
            trans.rotate(degrees);
            image = image.transformed(trans, Qt::SmoothTransformation);
            break;
        }
    }
    else if (rotationDegrees > 0){
        trans.rotate(rotationDegrees);
        image = image.transformed(trans, Qt::SmoothTransformation);
    }
    /* debug
    qDebug().noquote()
             << "ImageDecoder::rotate "
             << "n.orientation =" << n.orientation
             << "n.rotationDegrees =" << n.rotationDegrees
             << "degrees =" << QString::number(degrees).leftJustified(3, ' ')
             << "n.fPath =" << n.fPath
                ;//*/
}

void ImageDecoder::applyDevelop()
{
/*
    Applies parametric develop adjustments (EditParams) to the decoded image, in the
    scene-linear working space: InputTransform (sRGB -> linear) -> Develop -> OutputTransform
    (linear -> display). Runs after rotate() and before output colour management.

    No-op while editParams is identity (the common case until per-image edits are stored),
    and skipped for RAW, which develops internally in float (developApplied). This keeps the
    hot browsing path untouched: an unedited image never round-trips through the float buffer.
*/
    if (isLog || G::isLogger) G::log("ImageDecoder::applyDevelop", "sfRow = " + QString::number(sfRow));
    /* Preview mode shows the as-shot image WITHOUT the saved develop recipe (fast review). Only the
       browse/loupe decode is gated on mode; independent decodes (focus stack) still develop. */
    if (!isIndependent && G::operationMode != G::OperationMode::Develop) return;
    if (developApplied || editParams.isIdentity() || image.isNull()) return;

    /* Reuse the pre-develop WorkingImage if a prior decode cached it (skips InputTransform);
       otherwise build it once from the decoded QImage and cache it for next time. The cached
       image is scene-LINEAR (post-InputTransform), matching what the RAW path stores. */
    auto work = WorkingImageCache::instance().get(fPath);
    if (!work) {
        auto built = std::make_shared<WorkingImage>();
        InputTransform input;
        if (!input.FromImage(image, *built)) return;
        WorkingImageCache::instance().put(fPath, built);
        work = built;
    }

    /* Re-render through Develop + OutputTransform. render() copies the cached image (Develop
       mutates in place) and leaves the cache entry pristine for the next slider value. */
    WorkingImageCache::render(*work, editParams, image);
}

void ImageDecoder::colorManage()
{
    if (isLog || G::isLogger) G::log("ImageDecoder::colorManage", "sfRow = " + QString::number(sfRow));
    if (isDebug) {
        // G::log("ImageDecoder::colorManage", "Thread " + QString::number(threadId));
    }
    if (!metadata->iccFormats.contains(ext)) return;
    if (abort.loadAcquire()) return;
    if (image.isNull()) return;

    // ICC::transform assumes TYPE_BGRA_8 (4 bytes/pixel). Convert if needed so
    // lcms2's packer does not walk past a narrower buffer (crash site).
    if (image.format() != QImage::Format_ARGB32 &&
        image.format() != QImage::Format_RGB32) {
        image = image.convertToFormat(QImage::Format_ARGB32);
        if (image.isNull()) return;
    }

    QByteArray iccBuf = isIndependent
        ? indMeta.iccBuf
        : dm->sf->index(sfRow, G::ICCBufColumn).data().toByteArray();
    ICC::transform(iccBuf, image);
}

bool ImageDecoder::decodeIndependent(QImage &img, Metadata *metadata, ImageMetadata &m)
{
/*
    This function is called externally, does not require the DataModel and does not
    run in a separate thread.

    It is used by FindDuplicatesDlg.
*/
    QString srcFun = "  ImageDecoder::decodeIndependent";
    if (G::isLogger) G::log(srcFun, m.fPath);

    /*
    Decode from the supplied ImageMetadata, not the live DataModel. sfRow is
    left at -1 (used only for logging) because the path may no longer be in
    dm->sf, e.g. the user navigated to another folder while a focus stack is
    still processing.
    */
    this->metadata = metadata;
    isIndependent = true;
    indMeta = m;
    fPath = m.fPath;
    sfRow = -1;

    if (load()) {
        if (metadata->rotateFormats.contains(ext)) rotate();
        applyDevelop();
        colorManage();
        img = image;
        return true;
    }
    qDebug() << "ImageDecoder::decode failed" << fPath;
    return false;
}

std::shared_ptr<const WorkingImage> ImageDecoder::decodeRawWorking(const ImageMetadata &m,
                                                                   bool denoiseRaw)
{
/*
    Uncached raw sensor decode -> pre-develop WorkingImage, for the "Denoise raw" base
    (MW::ensureRawDenoise). denoiseRaw runs PMRID pre-demosaic (in-house/Winnow engine only, see
    RawFormat::Decode). Consults only the supplied m (fPath + rawInfo + ISONum) so it is safe to
    call from the develop render pool. Does NOT touch WorkingImageCache.
*/
    if (G::isLogger) G::log("  ImageDecoder::decodeRawWorking", m.fPath);
    const QString ext = QFileInfo(m.fPath).suffix().toLower();
    std::unique_ptr<RawFormat> rawFormat = RawFormat::Create(ext);
    if (!rawFormat) return nullptr;                     // no in-house decoder for this format

    QFile file(m.fPath);
    if (!file.open(QIODevice::ReadOnly)) return nullptr;

    QImage throwaway;                                    // develop skipped: identity edit
    const EditParams identity;
    std::shared_ptr<const WorkingImage> work;
    const bool ok = rawFormat->Decode(file, m, throwaway, &identity, &abort, &work, denoiseRaw);
    file.close();
    return ok ? work : nullptr;
}
