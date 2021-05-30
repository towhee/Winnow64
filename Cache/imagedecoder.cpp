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
*/

ImageDecoder::ImageDecoder(QObject *parent, int id, Metadata *metadata) : QThread(parent)
{
    if (G::isLogger) G::log(__FUNCTION__, "Thread " + QString::number(id));
    threadId = id;
    status = Status::Ready;
    this->metadata = metadata;
}

void ImageDecoder::decode(G::ImageFormat format,
                          QString fPath,
                          ImageMetadata m,
                          QByteArray ba)
{
//    qDebug() << __FUNCTION__
//             << "threadId =" << threadId
//             << fPath
//             << format;
    status = Status::Busy;
    imageFormat = format;
    this->fPath = fPath;
    this->m = m;
    this->ba.clear();
    this->ba = ba;
    QFileInfo fileInfo(fPath);
    ext = fileInfo.completeSuffix().toLower();
    if (format == G::Tif) {
        p.file.setFileName(fPath);
        p.file.open(QIODevice::ReadOnly);
        buf = p.file.map(0, p.file.size());
    }
//    if (isRunning()) {
//        qDebug() << __FUNCTION__ << "threadId =" << threadId << "is Running";
//    }
    start();
}

// all code below runs in separate thread

void ImageDecoder::setReady()
{
    if (G::isLogger) G::log(__FUNCTION__, "Thread " + QString::number(threadId));
    status = Status::Ready;
    ba = nullptr;
    fPath = "";
}

void ImageDecoder::decodeJpg()
{
    if (G::isLogger) G::log(__FUNCTION__, "Thread " + QString::number(threadId));
//    qDebug() << __FUNCTION__ << "threadId =" << threadId;
    // chk nullptr
    image.loadFromData(ba, "JPEG");
}

void ImageDecoder::decodeTif()
{
    if (G::isLogger) G::log(__FUNCTION__, "Thread " + QString::number(threadId));
    Tiff tiff;
    if (!tiff.decode(m, p, image)) {
        decodeUsingQt();
    }
    else {
        p.file.unmap(buf);
    }
}

void ImageDecoder::decodeHeic()
{
    if (G::isLogger) G::log(__FUNCTION__, "Thread " + QString::number(threadId));
    #ifdef Q_OS_WIN
    Heic heic;
    bool success = heic.decodePrimaryImage(m, fPath, image);
    #endif
}

void ImageDecoder::decodeUsingQt()
{
    if (G::isLogger) G::log(__FUNCTION__, "Thread " + QString::number(threadId));
    bool success = image.load(fPath);
}

void ImageDecoder::rotate()
{
    if (G::isLogger) G::log(__FUNCTION__, "Thread " + QString::number(threadId));
//    qDebug() << __FUNCTION__ << fPath << m.orientation << m.rotationDegrees;
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
//#ifdef Q_OS_WIN
    if (G::isLogger) G::log(__FUNCTION__, "Thread " + QString::number(threadId));
    if (metadata->iccFormats.contains(ext)) {
        ICC::transform(m.iccBuf, image);
    }
//#endif
}

void ImageDecoder::run()
{
    if (G::isLogger) G::log(__FUNCTION__, "Thread " + QString::number(threadId));
//    qDebug() << __FUNCTION__ << "threadId =" << threadId;
    switch(imageFormat) {
    case G::Jpg:
//        qDebug() << __FUNCTION__ << "threadId =" << threadId << "case G::Jpg";
        decodeJpg();
        break;
    case G::Tif:
        decodeTif();
        break;
    case G::Heic:
        decodeHeic();
        break;
    case G::UseQt:
        decodeUsingQt();
    }
//    if (metadata->rotateFormats.contains(ext) && (m.orientation > 0 || m.rotationDegrees > 0))
    if (metadata->rotateFormats.contains(ext)) rotate();

    if (G::colorManage) colorManage();
    status = Status::Done;
//    qDebug() << __FUNCTION__ << "threadId =" << threadId << "Emitting done";
    emit done(threadId);
}
