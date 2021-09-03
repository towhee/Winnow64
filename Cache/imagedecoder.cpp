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
    if (G::isLogger) G::log(__FUNCTION__, "Thread " + QString::number(id));
    threadId = id;
    status = Status::Ready;
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
        status = Status::Ready;
    }
}

void ImageDecoder::decode(QString fPath)
{
    status = Status::Busy;
    this->fPath = fPath;
    start();
}

// all code below runs in separate thread

bool ImageDecoder::load()
{
/*  Loads a full size preview into a QImage.  It is invoked from ImageCache::fillCache.

    NOTE: calls to metadata and dm to not appear to impact performance.
*/
    QMutexLocker locker(&mutex);
    if (G::isLogger) G::log(__FUNCTION__, fPath);
    /*
    qDebug() << __FUNCTION__ << "fPath =" << fPath;
    //*/

    // null fPath when caching is cycling, waiting to finish.
    if (fPath == "") return false;

    QFileInfo fileInfo(fPath);
    QString ext = fileInfo.completeSuffix().toLower();

    if (metadata->videoFormats.contains(ext)) return false;

    QFile imFile(fPath);
    int dmRow = dm->fPathRow[fPath];

    // is metadata loaded rgh use isMeta in cacheItemList?
    if (!dm->metadataLoaded(dmRow)) {
        G::error(__FUNCTION__, fPath, "Could not load metadata.");
        return false;
    }
//    if (!dm->index(dmRow, G::MetadataLoadedColumn).data().toBool()) {
//        //        if (!dm->readMetadataForItem(dmRow)) {
//        G::error(__FUNCTION__, fPath, "Could not load metadata.");
//        return false;
//        //        }
//    }

    if (abort) return false;

    // is file already open by another process
    if (imFile.isOpen()) {
        G::error(__FUNCTION__, fPath, "File already open.");
        return false;
    }

    // try to open image file
    if (!imFile.open(QIODevice::ReadOnly)) {
        imFile.close();
        G::error(__FUNCTION__, fPath, "Could not open file for image.");
        return false;
    }

    // JPG format (including embedded in raw files)
    if (metadata->hasJpg.contains(ext) || ext == "jpg") {
        // get offset and length of embedded Jpg from the datamodel
        uint offsetFullJpg = dm->index(dmRow, G::OffsetFullColumn).data().toUInt();
        uint lengthFullJpg = dm->index(dmRow, G::LengthFullColumn).data().toUInt();

        // make sure legal offset by checking the length
        if (lengthFullJpg == 0) {
            imFile.close();
            G::error(__FUNCTION__, fPath, "Jpg length = zero.");
            return false;
        }

        // try to read the data
        if (!imFile.seek(offsetFullJpg)) {
            imFile.close();
            G::error(__FUNCTION__, fPath, "Illegal offset to image.");
            return false;
        }

        QByteArray buf = imFile.read(lengthFullJpg);
        if (buf.length() == 0) {
            qWarning() << __FUNCTION__ << "Zero JPG buffer";
            G::error(__FUNCTION__, fPath, "JPG zero JPG buffer.");
            imFile.close();
            return false;
        }

        if (abort) return false;

        // try to decode the jpg data
        if (!image.loadFromData(buf, "JPEG")) {
            G::error(__FUNCTION__, fPath, "image.loadFromData failed.");
            imFile.close();
            return false;
        }
        imFile.close();
    }

    // HEIC format
    // rgh remove heic (why?)
    else if (metadata->hasHeic.contains(ext)) {
        ImageMetadata m = dm->imMetadata(fPath);
        #ifdef Q_OS_WIN
        Heic heic;
        if (!heic.decodePrimaryImage(m, fPath, image)) {
            G::error(__FUNCTION__, fPath, "heic.decodePrimaryImage failed.");
            imFile.close();
            return false;
        }
        /*
        qDebug() << __FUNCTION__ << "HEIC image" << image.width() << image.height();
        //*/
        #endif
    }

    // TIFF format
    else if (ext == "tif") {
        // check for sampling format we cannot read
        int samplesPerPixel = dm->index(dmRow, G::samplesPerPixelColumn).data().toInt();
        if (samplesPerPixel > 3) {
            imFile.close();
            QString err = "Could not read tiff because " + QString::number(samplesPerPixel)
                    + " samplesPerPixel > 3.";
            G::error(__FUNCTION__, fPath, err);
            return false;
        }

        // use Winnow decoder
        ImageMetadata m = dm->imMetadata(fPath);
        Tiff tiff;
        if (!tiff.decode(m, fPath, image)) {
            imFile.close();
            QString err = "Could not decode using Winnow Tiff decoder.  "
                        "Trying Qt tiff library to decode" + fPath + ". ";
            G::error(__FUNCTION__, fPath, err);
            if (abort) return false;
            // use Qt tiff library to decode
            if (!image.load(fPath)) {
                imFile.close();
                QString err = "Could not decode using Qt.";
                G::error(__FUNCTION__, fPath, err);
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
        qDebug() << __FUNCTION__
                 << "USEQT: "
                 << "Id =" << threadId
                 << "decoder->fPath =" << fPath
                    ;
                    //*/
        if (!image.load(fPath)) {
            imFile.close();
            G::error(__FUNCTION__, fPath, "Could not decode using Qt.");
            return false;
        }
        imFile.close();
    }
    return true;
}

void ImageDecoder::setReady()
{
    if (G::isLogger) G::log(__FUNCTION__, "Thread " + QString::number(threadId));
    status = Status::Ready;
}

void ImageDecoder::rotate()
{
    if (G::isLogger) G::log(__FUNCTION__, "Thread " + QString::number(threadId));
    QTransform trans;
    int degrees;
    if (m.orientation > 0) {
        switch(m.orientation) {
        case 3:
            degrees = m.rotationDegrees + 180;
            if (degrees > 360) degrees = degrees - 360;
            trans.rotate(degrees);
            image = image.transformed(trans, Qt::SmoothTransformation);
            break;
        case 6:
            degrees = m.rotationDegrees + 90;
            if (degrees > 360) degrees = degrees - 360;
            trans.rotate(degrees);
            image = image.transformed(trans, Qt::SmoothTransformation);
            break;
        case 8:
            degrees = m.rotationDegrees + 270;
            if (degrees > 360) degrees = degrees - 360;
            trans.rotate(degrees);
            image = image.transformed(trans, Qt::SmoothTransformation);
            break;
        }
    }
    else if (m.rotationDegrees > 0){
        trans.rotate(m.rotationDegrees);
        image = image.transformed(trans, Qt::SmoothTransformation);
    }
}

void ImageDecoder::colorManage()
{
    if (G::isLogger) G::log(__FUNCTION__, "Thread " + QString::number(threadId));
    if (metadata->iccFormats.contains(ext)) {
        ICC::transform(m.iccBuf, image);
    }
}

void ImageDecoder::run()
{
    if (G::isLogger) G::log(__FUNCTION__, "Thread " + QString::number(threadId));
    if (load()) {
        if (metadata->rotateFormats.contains(ext) && !abort) rotate();
        if (G::colorManage && !abort) colorManage();
        status = Status::Done;
    }
    else {
        status = Status::Failed;
        fPath = "";
    }
    if (abort) return;
    emit done(threadId);
}
