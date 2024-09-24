#include "imagedecoder.h"

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

ImageDecoder::ImageDecoder(QObject *parent,
                           int id,
                           DataModel *dm,
                           Metadata *metadata)
    : QThread(parent)
{
    if (isDebug) G::log("ImageDecoder::ImageDecoder", "Thread " + QString::number(id));
    threadId = id;
    status = Status::Ready;
    fPath = "";
    cacheKey = -1;
    instance = 0;
    this->dm = dm;
    this->metadata = metadata;
    isDebug = false;
}

void ImageDecoder::stop()
{
    if (isRunning()) {
        mutex.lock();
        abort = true;
        condition.wakeOne();
        mutex.unlock();
        wait();
        abort = false;
    }
}

bool ImageDecoder::quit()
{
    abort = false;
    status = Status::Abort;
    fPath = "";
    // QImage blank;
    // image = QImage();
    cacheKey = -1;
    return false;
}

void ImageDecoder::decode(ImageCacheData::CacheItem item, int instance)
{
    status = Status::Busy;
    n = item;
    fPath = n.fPath;
    cacheKey = n.key;       // not being used (ImageCache::fixOrphans artifact)
    this->instance = instance;
    errMsg = "";
    if (G::isLogger) G::log("ImageDecoder::decode", "row = " + QString::number(cacheKey));
    /*
    qDebug() << "ImageDecoder::decode                              "
             << "decoder" << threadId
             << "row =" << cacheKey
             << fPath; //*/
    start(QThread::LowestPriority);
}

// all code below runs in separate thread

bool ImageDecoder::load()
{
/*
    Loads a full size preview into a QImage.  It is invoked from ImageCache::fillCache.
    NOTE: calls to metadata and dm to not appear to impact performance.
*/
    if (G::isLogger) G::log("ImageDecoder::load",
               "row = " + QString::number(cacheKey) + "  " + fPath);
    QString fun = "ImageDecoder::load";
    if (isDebug)
        qDebug() << fun << fPath;

    // blank fPath when caching is cycling, waiting to finish.
    if (fPath == "") {
        errMsg = "Null file path.";
        G::issue("Warning", errMsg, "ImageDecoder::load", n.key, fPath);
        status = Status::BlankFilePath;
        return false;
    }

    /* get image type (extension)
       can cause crash: QFileInfo fileInfo(fPath);  or
                        ext = fPath.section('.', -1).toLower(); */
    ext = n.ext;

    // do not cache video files
    if (metadata->videoFormats.contains(ext)) {
        status = Status::Video;
        return false;
    }

    QFile imFile(fPath);

    // is file already open by another process
    if (imFile.isOpen()) {
        errMsg = "File already open.";
        G::issue("Warning", errMsg, "ImageDecoder::load", n.key, fPath);
        status = Status::FileOpen;
        return false;
    }

    // try to open image file
    if (!imFile.open(QIODevice::ReadOnly)) {
        status = Status::FileOpen;
        return false;
    }

    decoderToUse = QtImage;  // default unless overridden

    // Embedded jpg?
    bool isEmbeddedJpg = false;
    // raw file or jpg
    if ((metadata->hasJpg.contains(ext) || ext == "jpg") && n.offsetFull) {
        isEmbeddedJpg = true;
    }
    // heic saved as a jpg
    if (metadata->hasHeic.contains(ext) && n.lengthFull) {
        isEmbeddedJpg = true;
    }

    // Embedded JPG format (including embedded in raw files)
    if (isEmbeddedJpg) {
        // make sure legal offset by checking the length
        if (n.lengthFull == 0) {
            errMsg = "Could not read embedded JPG because length = 0.";
            G::issue("Warning", errMsg, "ImageDecoder::load", n.key, fPath);
            imFile.close();
            status = Status::Invalid;
            return false;
        }

        // try to read the data
        if (!imFile.seek(n.offsetFull)) {
            errMsg = "Could not read embedded JPG because offset is invalid.";
            G::issue("Warning", errMsg, "ImageDecoder::load", n.key, fPath);
            imFile.close();
            status = Status::Invalid;
            return false;
        }

        QByteArray buf = imFile.read(n.lengthFull);
        if (buf.length() == 0) {
            errMsg = "Could not read embedded JPG because buffer length = 0.";
            G::issue("Warning", errMsg, "ImageDecoder::load", n.key, fPath);
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
                G::issue("Warning", errMsg, "ImageDecoder::load", n.key, fPath);
                // try Qt decoder
                decoderToUse = QtImage;
            }
        }
        #endif

        if (decoderToUse == QtImage) {
            if (!image.loadFromData(buf, "JPEG")) {
                errMsg = "Could not read JPG because QImage::loadFromData failed.";
                G::issue("Warning", errMsg, "ImageDecoder::load", n.key, fPath);
                imFile.close();
                status = Status::Invalid;
                return false;
            }
        }
    }

    // HEIC format
    else if (metadata->hasHeic.contains(ext)) {
        #ifdef Q_OS_WIN
        Heic heic;
        if (!heic.decodePrimaryImage(fPath, image)) {
            QString errMsg = "heic.decodePrimaryImage failed.";
            G::issue("Error", errMsg, "ImageDecoder::load", n.key, fPath);
            imFile.close();
            status = Status::Invalid;
            return false;
        }
        /*
        qDebug() << "ImageDecoder::load" << "HEIC image" << image.width() << image.height();
        //*/
        #endif

        #ifdef Q_OS_MAC
        if (!image.load(fPath)) {
            errMsg = "Could not read because decoder failed.";
            G::issue("Warning", errMsg, "ImageDecoder::load", n.key, fPath);
            imFile.close();
            status = Status::Invalid;
            return false;
        }
        else if (image.width() == 0) {
            errMsg = "Unable to read heic image";
            G::issue("Warning", errMsg, "ImageDecoder::load", n.key, fPath);
            status = Status::Invalid;
            return false;
        }
        imFile.close();
        if (isDebug)
        {
        qDebug() << "ImageDecoder::load" << "HEIC image"
                 << image.width() << image.height()
                 << fPath
            ;
        }
        #endif
    }

    // TIFF format
    else if (ext == "tif") {

        /* decoder options:
           QtImage      use QImage::load
           QtTiff       use QTiffHandler code override
           LibTiff      use libtiff library directly
           Rory         use Rory decoder
        */
        decoderToUse = QtTiff;

        #ifdef Q_OS_MAC
        if (decoderToUse == LibTiff) {
            class LibTiff libTiff;
            image = libTiff.readTiffToQImage(fPath);
        }
        #endif

        if (decoderToUse == Rory) {
            // check for sampling format we cannot read
            if (n.samplesPerPixel > 3) {
                 errMsg = "TIFF samplesPerPixel more than 3.";
                 G::issue("Warning", errMsg, "ImageDecoder::run", cacheKey, fPath);
                 imFile.close();
                 status = Status::Invalid;
                 return false;
            }

            // try Winnow decoder
            Tiff tiff("ImageDecoder::load Id = " + QString::number(threadId));
            if (!tiff.decode(fPath, n.offsetFull, image)) {
                decoderToUse = QtImage;
                /*
                qDebug() << "ImageDecoder::load "
                         << "Could not decode using Winnow Tiff decoder.  row =" << n.key <<
                            "Trying Qt tiff library to decode " + fPath + ". ";  //*/
            }
        }

        if (decoderToUse == QtImage) {
            // use Qt tiff decoder
            if (!image.load(fPath)) {
                errMsg = "Could not read because Qt tiff decoder failed.";
                G::issue("Error", errMsg, "ImageDecoder::load", n.key, fPath);
                imFile.close();
                status = Status::Invalid;
                return false;
            }
        }

        if (decoderToUse == QtTiff) {
            // use QTiffHandler code override
            Tiff tiff("ImageDecoder::load");
            // qDebug() << "ImageDecoder::load decoderToUse == QtTiff" << fPath;
            if (!tiff.read(fPath, &image)) {
                errMsg = "Could not read because QTiff decoder failed.";
                G::issue("Error", errMsg, "ImageDecoder::load", n.key, fPath);
                imFile.close();
                status = Status::Invalid;
                return false;
            }
        }
    }

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
                G::issue("Warning", errMsg, "ImageDecoder::load", n.key, fPath);
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
                G::issue("Warning", errMsg, "ImageDecoder::load", n.key, fPath);
                imFile.close();
                status = Status::Invalid;
                return false;
            }
        }

        if (decoderToUse == QtImage) {
            image.load(fPath);
            if (image.isNull()) {
                errMsg = "Could not read because Qt decoder failed.";
                G::issue("Warning", errMsg, "ImageDecoder::load", n.key, fPath);
                imFile.close();
                status = Status::Invalid;
                return false;
            }
        }
    }

    // All other formats
    else {
        // try to decode
        if (!image.load(fPath)) {
            imFile.close();
            errMsg = "Could not read because decoder failed.";
            G::issue("Warning", errMsg, "ImageDecoder::load", n.key, fPath);
            status = Status::Invalid;
            return false;
        }
    }

    // image loaded, check for null image
    if (image.width() == 0 || image.height() == 0) {
        status = Status::Invalid;
        return false;
    }

    status = Status::Success;
    imFile.close();
    return true;
}

void ImageDecoder::setReady()
{
    status = Status::Ready;
}

void ImageDecoder::rotate()
{
    if (G::isLogger) G::log("ImageDecoder::rotate", "row = " + QString::number(cacheKey));
    if (isDebug) {
        mutex.lock();
        G::log("ImageDecoder::rotate", "Thread " + QString::number(threadId));
        mutex.unlock();
    }
    QTransform trans;
    int degrees = 0;
    if (n.orientation > 0) {
        switch (n.orientation) {
        case 3:
            degrees = n.rotationDegrees + 180;
            if (degrees > 360) degrees = degrees - 360;
            trans.rotate(degrees);
            image = image.transformed(trans, Qt::SmoothTransformation);
            break;
        case 6:
            degrees = n.rotationDegrees + 90;
            if (degrees > 360) degrees = degrees - 360;
            trans.rotate(degrees);
            image = image.transformed(trans, Qt::SmoothTransformation);
            break;
        case 8:
            degrees = n.rotationDegrees + 270;
            if (degrees > 360) degrees = degrees - 360;
            trans.rotate(degrees);
            image = image.transformed(trans, Qt::SmoothTransformation);
            break;
        }
    }
    else if (n.rotationDegrees > 0){
        trans.rotate(n.rotationDegrees);
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

void ImageDecoder::colorManage()
{
    if (G::isLogger) G::log("ImageDecoder::colorManage", "row = " + QString::number(cacheKey));
    if (isDebug) {
        mutex.lock();
        G::log("ImageDecoder::colorManage", "Thread " + QString::number(threadId));
        mutex.unlock();
    }
    if (metadata->iccFormats.contains(ext)) {
        ICC::transform(n.iccBuf, image);
    }
}

void ImageDecoder::run()
{
    if (isDebug) G::log("ImageDecoder::run", "Thread " + QString::number(threadId));
    if (isDebug)
    {
        mutex.lock();
        G::log("ImageDecoder::run", "Thread " + QString::number(threadId));
        mutex.unlock();
    }

    if (instance != dm->instance) {
        status = Status::InstanceClash;
        errMsg = "Instance clash.  New folder selected, processing old folder.";
        G::issue("Comment", errMsg, "ImageDecoder::run", cacheKey, fPath);
        emit done(threadId);
        return;
    }

    QElapsedTimer t;
    t.start();

    if (load()) {
        if (isDebug) G::log("ImageDecoder::run (if load)", "Image width = " + QString::number(image.width()));
        if (isDebug)
        qDebug().noquote()
            << "ImageDecoder::run                    "
            << "decoder" <<  QString::number(threadId).leftJustified(2)
            << "row =" <<  QString::number(cacheKey).leftJustified(4)
            << "DecoderToUse:" << decodersText.at(decoderToUse)
            << "ms =" << t.elapsed()
            << fPath;
        if (metadata->rotateFormats.contains(ext) && !abort) rotate();
        if (G::colorManage && !abort) colorManage();
        status = Status::Success;
    }
    else {
        if (isDebug)
        qDebug() << "ImageDecoder::run load failed"
                 << "cacheKey =" << cacheKey
                 << "status =" << statusText.at(status)
                 << "errMsg =" << errMsg
                    ; //*/
    }

    if (isDebug) {
        mutex.lock();
        G::log("ImageDecoder::run", "Thread " + QString::number(threadId) + " done");
        mutex.unlock();
    }
    /* debug
        qDebug() << "ImageDecoder::run"
                 << "Id =" << threadId
                 << "decoder->fPath =" << fPath
                    ; //*/
    emit done(threadId);
}

bool ImageDecoder::decodeIndependent(QImage &img, Metadata *metadata, ImageMetadata &m)
{
/*
    This function is called externally, does not require the DataModel and does not
    run in a separate thread.

    It is used by FindDuplicatesDlg.
*/
    this->metadata = metadata;
    fPath = m.fPath;
    n.fPath = m.fPath;
    n.ext = m.ext;
    n.metadataLoaded = m.metadataLoaded;
    n.orientation = m.orientation;
    n.rotationDegrees = m.rotationDegrees;
    n.offsetFull = m.offsetFull;
    n.lengthFull = m.lengthFull;
    n.samplesPerPixel = m.samplesPerPixel;

    if (load()) {
        //if (metadata->rotateFormats.contains(ext)) rotate();
        if (metadata->rotateFormats.contains(ext)) rotate();
        colorManage();
        img = image;
        return true;
    }
    qDebug() << "ImageDecoder::decode failed" << fPath;
    return false;
}
