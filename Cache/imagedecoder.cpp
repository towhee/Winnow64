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
    dmInstance = instance;
    cacheKey = n.key;       // not being used
//    qDebug() << "ImageDecoder::decode" << fPath;
    start();
}

// all code below runs in separate thread

bool ImageDecoder::load()
{
/*  Loads a full size preview into a QImage.  It is invoked from ImageCache::fillCache.

    NOTE: calls to metadata and dm to not appear to impact performance.
*/
//    if (G::isLogger) {mutex.lock(); G::log("ImageDecoder::load", fPath); mutex.unlock();}
    /*
    qDebug() << "ImageDecoder::load" << "fPath =" << fPath;
    //*/

    // null fPath when caching is cycling, waiting to finish.
    if (fPath == "") return false;

    QFileInfo fileInfo(fPath);
    ext = fileInfo.completeSuffix().toLower();

    if (metadata->videoFormats.contains(ext)) {
//        qWarning() << "ImageDecoder::load  Ignore video formats" << fPath;
//        G::error("ImageDecoder::load", fPath, "Ignore video formats.");
        status = Status::Video;
        return false;
    }

    QFile imFile(fPath);

    // is metadata loaded rgh use isMeta in cacheItemList?
    if (!n.metadataLoaded && metadata->hasMetadataFormats.contains(ext)) {
        qWarning() << "ImageDecoder::load  Metadata not loaded" << fPath;
//        G::error("ImageDecoder::load", fPath, "Metadata not loaded.");
        return false;
    }

    if (abort) quit();

    // is file already open by another process
    if (imFile.isOpen()) {
        qWarning() << "ImageDecoder::load  File already open" << fPath;
//        G::error("ImageDecoder::load", fPath, "File already open.");
        return false;
    }

    // try to open image file
    if (!imFile.open(QIODevice::ReadOnly)) {
        imFile.close();
        qWarning() << "ImageDecoder::load  Could not open file for image" << fPath;
        G::error("ImageDecoder::load", fPath, "Could not open file for image.");
        return false;
    }

    // JPG format (including embedded in raw files)
    if (metadata->hasJpg.contains(ext) || ext == "jpg") {
        // make sure legal offset by checking the length
        if (n.lengthFull == 0) {
            imFile.close();
            qWarning() << "ImageDecoder::load  Jpg length = zero" << fPath;
//            G::error("ImageDecoder::load", fPath, "Jpg length = zero.");
            return false;
        }

        // try to read the data
        if (!imFile.seek(n.offsetFull)) {
            imFile.close();
            qWarning() << "ImageDecoder::load Illegal offset to image " << fPath;
//            G::error("ImageDecoder::load", fPath, "Illegal offset to image.");
            return false;
        }

        QByteArray buf = imFile.read(n.lengthFull);
        if (buf.length() == 0) {
            qWarning() << "ImageDecoder::load  Zero JPG buffer" << fPath;
//            G::error("ImageDecoder::load", fPath, "Zero JPG buffer.");
            imFile.close();
            return false;
        }

        if (abort) quit();

        // try to decode the jpg data
        if (!image.loadFromData(buf, "JPEG")) {
            qWarning() << "ImageDecoder::load  image.loadFromData failed" << fPath;
//            G::error("ImageDecoder::load", fPath, "image.loadFromData failed.");
            imFile.close();
            return false;
        }
        imFile.close();
    }

    // HEIC format
    // rgh remove heic (why?)
    else if (metadata->hasHeic.contains(ext)) {
        #ifdef Q_OS_WIN
        Heic heic;
        if (!heic.decodePrimaryImage(fPath, image)) {
            G::error("ImageDecoder::load", fPath, "heic.decodePrimaryImage failed.");
            imFile.close();
            return false;
        }
        /*
        qDebug() << "ImageDecoder::load" << "HEIC image" << image.width() << image.height();
        //*/
        #endif

        #ifdef Q_OS_MAC
        if (!image.load(fPath)) {
            imFile.close();
            qWarning() << "ImageDecoder::load  Could not decode using Qt" << fPath;
//            G::error("ImageDecoder::load", fPath, "Could not decode using Qt.");
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
            QString err = "Could not read tiff because " + QString::number(n.samplesPerPixel)
                    + " samplesPerPixel > 3.";
            qWarning() << "ImageDecoder::load " << err << fPath;
//            G::error("ImageDecoder::load", fPath, err);
            return false;
        }

        // use Winnow decoder
        Tiff tiff;
        if (!tiff.decode(fPath, n.offsetFull, image)) {
            imFile.close();
            QString err = "Could not decode using Winnow Tiff decoder.  "
                        "Trying Qt tiff library to decode" + fPath + ". ";
//            G::error("ImageDecoder::load", fPath, err);
            qWarning() << "ImageDecoder::load "
                     << "Could not decode using Winnow Tiff decoder.  "
                        "Trying Qt tiff library to decode " + fPath + ". ";
            if (abort) quit();
            // use Qt tiff library to decode
            if (!image.load(fPath)) {
                imFile.close();
                qWarning() << "ImageDecoder::load  Could not decode using Qt" << fPath;
//                QString err = "Could not decode using Qt.";
//                G::error("ImageDecoder::load", fPath, err);
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
            qWarning() << "ImageDecoder::load  Could not decode using Qt" << fPath;
//            G::error("ImageDecoder::load", fPath, "Could not decode using Qt.");
            return false;
        }
        imFile.close();
    }

    // check for null image
    if (image.width() == 0 || image.height() == 0) return false;

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
    int degrees;
    if (n.orientation > 0) {
        switch(n.orientation) {
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
    if (G::isLogger) {
//        mutex.lock();
//        G::log("ImageDecoder::run", "Thread " + QString::number(threadId));
//        mutex.unlock();
    }
    if (load()) {
        if (G::isLogger) G::log("ImageDecoder::run", "Image width =" + QString::number(image.width()));
        if (metadata->rotateFormats.contains(ext) && !abort) rotate();
        if (G::colorManage && !abort) colorManage();
        status = Status::Done;
    }
    else {
        if (status == Status::Video) {

        }
        else {
//            G::error("ImageDecoder::run", fPath, "Could not load " + fPath);
            status = Status::Failed;
            fPath = "";
        }
    }

    if (G::isLogger) G::log("ImageDecoder::run", "Thread " + QString::number(threadId) + " done");
    if (!abort) emit done(threadId);
}
