#include "imagedecoder.h"

/*
   - maybe eliminate datamodel adn metadata, passing all req'd info ie ICC and rotation
   - change tif to read file separate from decoding (QIODevice?)
*/

ImageDecoder::ImageDecoder(QObject *parent, int id) : QThread(parent)
{
    threadId = id;
    status = Status::Ready;
}

void ImageDecoder::decode(G::ImageFormat format,
                          QString fPath,
                          ImageMetadata m,
                          QByteArray ba)
{
    imageFormat = format;
    this->fPath = fPath;
    this->m = m;
    this->ba = ba;
    start();
}

// all code below runs in separate thread

void ImageDecoder::setReady()
{
    status = Status::Ready;
    ba = nullptr;
    fPath = "";
}

void ImageDecoder::decodeJpg()
{
    // chk nullptr
    image.loadFromData(ba, "JPEG");
}

void ImageDecoder::decodeTif()
{
    Tiff tiff;
    if (!tiff.decode(m, fPath, image)) {
        decodeUsingQt();
    }
}

void ImageDecoder::decodeHeic()
{
    Heic heic;
    bool success = heic.decodePrimaryImage(m, fPath, image);
}

void ImageDecoder::decodeUsingQt()
{
    bool success = image.load(fPath);
}

void ImageDecoder::afterDecode()
{
    // rotate if portrait image
    QFileInfo fileInfo(fPath);
    QString ext = fileInfo.completeSuffix().toLower();
    int dmRow = dm->fPathRow[fPath];
    if (metadata->rotateFormats.contains(ext)) {
        QTransform trans;
        int orientation = dm->index(dmRow, G::OrientationColumn).data().toInt();
        int rotationDegrees = dm->index(dmRow, G::RotationDegreesColumn).data().toInt();
        int degrees;
        if (orientation) {
            switch(orientation) {
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
        else if (rotationDegrees){
            trans.rotate(rotationDegrees);
            image = image.transformed(trans, Qt::SmoothTransformation);
        }
    }

    // color manage if available
    #ifdef Q_OS_WIN
    if (G::colorManage && metadata->iccFormats.contains(ext)) {
        QByteArray ba = dm->index(dmRow, G::ICCBufColumn).data().toByteArray();
        ICC::transform(ba, image);
    }
    #endif
}

void ImageDecoder::run()
{
    switch(imageFormat) {
    case G::Jpg: decodeJpg();
        break;
    case G::Tif: decodeTif();
        break;
    case G::Heic: decodeHeic();
        break;
    case G::UseQt: decodeUsingQt();
    }
    afterDecode();
    status = Status::Done;
    emit done(threadId);
}
