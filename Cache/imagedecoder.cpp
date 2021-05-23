#include "imagedecoder.h"

/*
   - maybe eliminate datamodel and metadata, passing all req'd info ie ICC and rotation
   - change tif to read file separate from decoding (QIODevice?)
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
    this->ba = ba;
    QFileInfo fileInfo(fPath);
    ext = fileInfo.completeSuffix().toLower();
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
    if (!tiff.decode(m, fPath, image)) {
        decodeUsingQt();
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
#ifdef Q_OS_WIN
    if (G::isLogger) G::log(__FUNCTION__, "Thread " + QString::number(threadId));
    if (metadata->iccFormats.contains(ext)) {
        ICC::transform(m.iccBuf, image);
    }
#endif
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
