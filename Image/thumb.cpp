#include "Image/thumb.h"

Thumb::Thumb(QObject *parent, Metadata *metadata) : QObject(parent)
{
    this->metadata = metadata;
    thumbMax.setWidth(160);       // rgh review hard coding thumb size
    thumbMax.setHeight(160);      // rgh review hard coding thumb size
}

void Thumb::track(QString fPath, QString msg)
{
    if (G::isThreadTrackingOn) qDebug() << "• Metadata Caching" << fPath << msg;
}

void Thumb::checkOrientation(QString &fPath, QImage &image)
{
    // check orientation and rotate if portrait
    QTransform trans;
    int orientation = metadata->getOrientation(fPath);
    switch(orientation) {
        case 6:
            trans.rotate(90);
            image = image.transformed(trans, Qt::SmoothTransformation);
            break;
        case 8:
            trans.rotate(270);
            image = image.transformed(trans, Qt::SmoothTransformation);
            break;
    }
}

bool Thumb::loadFromEntireFile(QString &fPath, QImage &image)
{
    bool success;
    QFile imFile(fPath);
    QImageReader thumbReader;
    QSize thumbMax(160, 160);       // rgh review hard coding thumb size
    if (imFile.open(QIODevice::ReadOnly)) {
        // close file to allow qt thumbReader to work
        imFile.close();
        // let thumbReader do its thing
        thumbReader.setFileName(fPath);
        QSize size = thumbReader.size();
        size.scale(thumbMax, Qt::KeepAspectRatio);
        thumbReader.setScaledSize(size);
        image = thumbReader.read();
        success = !image.isNull();
        if (!success) {
            err = "Could not read thumb using thumbReader";
            if (G::isThreadTrackingOn) track(fPath, err);
        }
    }
    else success = false;
    return success;
}

bool Thumb::loadFromData(QString &fPath, QImage &image)
{
    bool success = false;
    QFile imFile(fPath);

    ulong offsetThumb = metadata->getOffsetThumbJPG(fPath);
    ulong lengthThumb = metadata->getLengthThumbJPG(fPath);  // new

    if (imFile.open(QIODevice::ReadOnly)) {
        if (imFile.seek(offsetThumb)) {
            QByteArray buf = imFile.read(lengthThumb);
            if (image.loadFromData(buf, "JPEG")) {
                imFile.close();
                if (image.isNull() && G::isThreadTrackingOn )
                    track(fPath, "Empty thumb");
                  if (G::isThreadTrackingOn) qDebug() << fPath << "Scaling:" << image.size();

                image.scaled(thumbMax, Qt::KeepAspectRatio);
                success = true;
            }
            else {
                err = "Could not read thumb from buffer";
                if (G::isThreadTrackingOn) track(fPath, err);
//                emit updateLoadThumb(fPath, err);
//                break;
            }
        }
        else {
            err = "Illegal offset to thumb";
            if (G::isThreadTrackingOn) track(fPath, err);
//            emit updateLoadThumb(fPath, err);
//            break;
        }
    }
    else {
        // file busy, wait a bit and try again
        err = "Could not open file for thumb - try again";
        if (G::isThreadTrackingOn) track(fPath, err);
//        emit updateLoadThumb(fPath, err);
    }
    return success;
}

bool Thumb::loadThumb(QString &fPath, QImage &image)
{
    if (G::isThreadTrackingOn) track(fPath, "Reading");
    QFileInfo fileInfo(fPath);
    QString ext = fileInfo.completeSuffix().toLower();

    /* Reading the thumb directly from the image file is faster than using
    QImageReader (thumbReader) to read the entire jpg and then scaling it
    down. However, not all images have embedded thumbs so make a quick check.
    */
//    bool isLoaded = metadata->isLoaded(fPath);
/*        qDebug() << "offsetThumb"  << offsetThumb
             << "lengthThumb"  << lengthThumb
             << fPath << isLoaded
             << "row =" << row;  */

    bool success = false;
    ulong offsetThumb = metadata->getOffsetThumbJPG(fPath);
    bool readThumbFromJPG = (offsetThumb > 0); // && ext == "jpg");

    if (metadata->rawFormats.contains(ext) || readThumbFromJPG) {
        /* read the raw file thumb using metadata for offset and length of
        embedded JPG.  Check if metadata has been cached for this image.
        */
        if (metadata->isLoaded(fPath)) {
            success = loadFromData(fPath, image);
        }
        else {
            err = "Metadata has not been loaded yet - try again";
            if (G::isThreadTrackingOn) track(fPath, err);
        }
//        if (!success) emit updateLoadThumb(fPath, err);
    }
    else  {
        // read the image file (supported by Qt), scaling to thumbnail size
        if(loadFromEntireFile(fPath, image)) {
              success = true;
        }
        // check if file is locked by another process
        else {
            err = "Could not open file for image";    // try again
            if (G::isThreadTrackingOn) track(fPath, err);
        }
    }

//    if (!success) emit updateLoadThumb(fPath, err);
    if (success) checkOrientation(fPath, image);
    return success;
}
