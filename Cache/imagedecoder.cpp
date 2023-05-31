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
    if (G::isLogger) G::log("ImageDecoder::ImageDecoder", "Thread " + QString::number(id));
    threadId = id;
    status = Status::Ready;
    fPath = "";
    cacheKey = -1;
    instance = 0;
    this->dm = dm;
    this->metadata = metadata;
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
    quit();
}

bool ImageDecoder::quit()
{
    status = Status::Ready;
    fPath = "";
    QImage blank;
    image = blank;
    cacheKey = -1;
    return false;
}

void ImageDecoder::decode(ImageCacheData::CacheItem item, int instance)
//void ImageDecoder::decode(QString fPath)
{
    status = Status::Busy;
    n = item;
    fPath = n.fPath;
    cacheKey = n.key;       // used in ImageCache::fixOrphans
    this->instance = instance;
    errMsg = "";
//    qDebug() << "ImageDecoder::decode" << fPath;
    start();
}

// all code below runs in separate thread

bool ImageDecoder::load()
{
/*  Loads a full size preview into a QImage.  It is invoked from ImageCache::fillCache.

    NOTE: calls to metadata and dm to not appear to impact performance.
*/
    QString fun = "ImageDecoder::load";
    if (G::isLogger) G::log(fun, fPath);

    // null fPath when caching is cycling, waiting to finish.
    if (fPath == "") {
        if (G::isWarningLogger)
        qWarning() << "WARNING" << "ImageDecoder::load  Null file path" << fPath;
        status = Status::NoFile;
        return false;
    }

    // get image type (extension)
    QFileInfo fileInfo(fPath);
    if (!fileInfo.exists()) {
        if (G::isWarningLogger)
        qWarning() << "WARNING" << "ImageDecoder::load  File does not exist" << fPath;
        status = Status::NoFile;
        return false;
    }
    ext = fileInfo.completeSuffix().toLower();

    // ignore video files
    if (metadata->videoFormats.contains(ext)) {
        status = Status::Video;
        return false;
    }

    // is metadata loaded rgh use isMeta in cacheItemList?
    if (!n.metadataLoaded && metadata->hasMetadataFormats.contains(ext)) {
        if (G::isWarningLogger)
        qWarning() << "WARNING" << "ImageDecoder::load  Metadata not loaded"
                   << "threadId =" << threadId
                   << fPath;
        status = Status::NoMetadata;
        // pause for metadata to be loaded
        msleep(100);
        return false;
    }

    if (abort) quit();

    QFile imFile(fPath);

    // is file already open by another process
    if (imFile.isOpen()) {
        errMsg = "File already open.";
        if (G::isWarningLogger)
        qWarning() << "WARNING" << "ImageDecoder::load  File already open" << fPath;
        status = Status::FileOpen;
        return false;
    }

    // try to open image file
    if (!imFile.open(QIODevice::ReadOnly)) {
        imFile.close();
        QString errMsg = "Could not open file for image";
        if (G::isWarningLogger)
        qWarning() << "WARNING" << "ImageDecoder::load" <<  errMsg << fPath;
        G::error(errMsg, fun, fPath);
        // check if drive ejected or folder deleted by another app
        QDir dir(fileInfo.dir());
        if (!dir.exists()) {
            status = Status::NoDir;
            errMsg = "Folder is missing, deleted or in a drive that has been ejected.";
            if (G::isWarningLogger)
            qWarning() << "WARNING" << "ImageDecoder::load  Folder is missing, deleted or in a drive that has been ejected" << fPath;
        }
        else {
            status = Status::Failed;
        }
        return false;
    }

    // JPG format (including embedded in raw files)
    if (metadata->hasJpg.contains(ext) || ext == "jpg") {
        // make sure legal offset by checking the length
        if (n.lengthFull == 0) {
            imFile.close();
            errMsg = "Could not read embedded JPG because offset = 0.";
            if (G::isWarningLogger)
            qWarning() << "WARNING" << "ImageDecoder::load  Jpg length = zero" << fPath;
            status = Status::Invalid;
            return false;
        }

        // try to read the data
        if (!imFile.seek(n.offsetFull)) {
            imFile.close();
            errMsg = "Could not read embedded JPG because offset is invalid.";
            if (G::isWarningLogger)
            qWarning() << "WARNING" << "ImageDecoder::load Illegal offset to image " << fPath;
            status = Status::Invalid;
            return false;
        }

        QByteArray buf = imFile.read(n.lengthFull);
        if (buf.length() == 0) {
            errMsg = "Could not read embedded JPG because buffer length = 0.";
            if (G::isWarningLogger)
            qWarning() << "WARNING" << "ImageDecoder::load  Zero JPG buffer" << fPath;
            imFile.close();
            status = Status::Invalid;
            return false;
        }

        if (abort) quit();

        // try to decode the jpg data
        if (!image.loadFromData(buf, "JPEG")) {
            errMsg = "Could not read JPG because decoder failed.";
            if (G::isWarningLogger)
            qWarning() << "WARNING" << "ImageDecoder::load  image.loadFromData failed"
                       << "instance =" << instance
                       << fPath;
            imFile.close();
            status = Status::Invalid;
            return false;
        }
        imFile.close();
    }

    // HEIC format
    else if (metadata->hasHeic.contains(ext)) {
        #ifdef Q_OS_WIN
        Heic heic;
        if (!heic.decodePrimaryImage(fPath, image)) {
            QString errMsg = "heic.decodePrimaryImage failed";
            G::error(errMsg, fun, fPath);
            imFile.close();
            status = Status::Failed;
            return false;
        }
        /*
        qDebug() << "ImageDecoder::load" << "HEIC image" << image.width() << image.height();
        //*/
        #endif

        #ifdef Q_OS_MAC
        if (!image.load(fPath)) {
            errMsg = "Could not read because decoder failed.";
            imFile.close();
            if (G::isWarningLogger)
            qWarning() << "WARNING" << "ImageDecoder::load  Could not decode using Qt" << fPath;
            status = Status::Invalid;
            return false;
        }
        imFile.close();
        #endif
    }

    // TIFF format
    else if (ext == "tif") {
        // check for sampling format we cannot read
        if (n.samplesPerPixel > 3) {
            imFile.close();
            errMsg = "Could not read tiff because " + QString::number(n.samplesPerPixel)
                    + " samplesPerPixel > 3.";
            if (G::isWarningLogger)
            qWarning() << "WARNING" << "ImageDecoder::load " << errMsg << fPath;
            status = Status::Invalid;
            return false;
        }

        // use Winnow decoder
        Tiff tiff;
        if (!tiff.decode(fPath, n.offsetFull, image)) {
            imFile.close();
            QString err = "Could not decode using Winnow Tiff decoder.  "
                          "Trying Qt tiff library to decode" + fPath + ". ";
//            G::error("ImageDecoder::load", fPath, err);
            if (G::isWarningLogger)
            qWarning() << "WARNING" << "ImageDecoder::load "
                     << "Could not decode using Winnow Tiff decoder.  "
                        "Trying Qt tiff library to decode " + fPath + ". ";
            if (abort) quit();
            // use Qt tiff library to decode
            if (!image.load(fPath)) {
                imFile.close();
                errMsg = "Could not read because Qt tiff decoder failed.";
                if (G::isWarningLogger)
                qWarning() << "WARNING" << "ImageDecoder::load  Could not decode using Qt" << fPath;
                G::error(errMsg, fun, fPath);
                status = Status::Invalid;
                return false;
            }
        }

        imFile.close();
    }

    // All other formats
    else {
        // try to decode
        ImageMetadata m;
        /*
        qDebug() << "ImageDecoder::load"
                 << "USEQT: "
                 << "Id =" << threadId
                 << "decoder->fPath =" << fPath
                    ;
                    //*/
        if (!image.load(fPath)) {
            imFile.close();
            errMsg = "Could not read because decoder failed.";
            if (G::isWarningLogger)
            qWarning() << "WARNING" << "ImageDecoder::load  Could not decode using Qt" << fPath;
            G::error(errMsg, fun, fPath);
            status = Status::Invalid;
            return false;
        }
        imFile.close();
    }

    // image loaded, check for null image
    if (image.width() == 0 || image.height() == 0) {
        status = Status::Invalid;
        return false;
    }

    return true;
}

void ImageDecoder::setReady()
{
    if (G::isLogger) {
//        mutex.lock();
//        G::log("ImageDecoder::setRead", "Thread " + QString::number(threadId));
//        mutex.unlock();
    }
    status = Status::Ready;
}

void ImageDecoder::rotate()
{
    if (G::isLogger) {
//        mutex.lock();
//        G::log("ImageDecoder::rotate", "Thread " + QString::number(threadId));
//        mutex.unlock();
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
    if (G::isLogger) {
//        mutex.lock();
//        G::log("ImageDecoder::colorManage", "Thread " + QString::number(threadId));
//        mutex.unlock();
    }
    if (metadata->iccFormats.contains(ext)) {
        ICC::transform(n.iccBuf, image);
    }
}

void ImageDecoder::run()
{
//    if (G::isLogger) G::log("ImageDecoder::run", "Thread " + QString::number(threadId));
//    /*
    if (G::isLogger) {
        mutex.lock();
        G::log("ImageDecoder::run", "Thread " + QString::number(threadId));
        mutex.unlock();
    }
    //*/

    if (instance != dm->instance) {
        status = Status::InstanceClash;
        if (G::isWarningLogger)
        qWarning() << "WARNING" << "ImageDecoder::run  Instance clash" << fPath;
        if (!abort) emit done(threadId);
        return;
    }

    if (load()) {
        if (G::isLogger) G::log("ImageDecoder::run (if load)", "Image width = " + QString::number(image.width()));
        if (metadata->rotateFormats.contains(ext) && !abort) rotate();
        if (G::colorManage && !abort) colorManage();
        status = Status::Done;
    }

    if (G::isLogger) {
        mutex.lock();
        G::log("ImageDecoder::run", "Thread " + QString::number(threadId) + " done");
        mutex.unlock();
    }
    if (!abort) emit done(threadId);
}
