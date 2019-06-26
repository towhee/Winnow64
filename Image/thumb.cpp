#include "Image/thumb.h"

/*
   Loads a thumbnail preview from a file based on metadata already extracted by mdCache.  If
   the file contains a thumbnail jpg it is extracted.  If not, then then entire image is read
   and scaled to thumbMax.
*/

Thumb::Thumb(QObject *parent, DataModel *dm, Metadata *metadata) : QObject(parent)
{
    this->dm = dm;
    this->metadata = metadata;
}

void Thumb::track(QString fPath, QString msg)
{
    if (G::isThreadTrackingOn) qDebug() << G::t.restart() << "\t" << "• Metadata Caching" << fPath << msg;
}

void Thumb::checkOrientation(QString &fPath, QImage &image)
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    #ifdef ISPROFILE
    G::track(__FUNCTION__);
    #endif
    }
    // check orientation and rotate if portrait
    #ifdef ISPROFILE
    G::track(__FUNCTION__, "About to QTransform trans");
    #endif
    QTransform trans;
    int row = dm->fPathRow[fPath];
    int orientation = dm->index(row, G::OrientationColumn).data().toInt();
//    int orientation = metadata->getOrientation(fPath);
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
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    thumbMax.setWidth(G::maxIconSize);
    thumbMax.setHeight(G::maxIconSize);
//    qDebug() << "thumbMax" << thumbMax;
    bool success;
    QFile imFile(fPath);
    QImageReader thumbReader;
//    qDebug() << __FUNCTION__  << "Open file" << fPath;
//    QSize thumbMax(THUMB_MAX, THUMB_MAX);       // rgh review hard coding thumb size
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
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__, fPath);
    #endif
    #ifdef ISPROFILE
    G::track(__FUNCTION__);
    #endif
    }
    thumbMax.setWidth(G::maxIconSize);
    thumbMax.setHeight(G::maxIconSize);
//    qDebug() << "thumbMax" << thumbMax;
    bool success = false;
    QFile imFile(fPath);
    int row = dm->fPathRow[fPath];

    // Check if metadata has been cached for this image
    if (dm->index(row, G::OffsetThumbJPGColumn).data().isNull()) {
        metadata->loadImageMetadata(fPath, true, false, false, false, __FUNCTION__);
        dm->addMetadataItem(metadata->imageMetadata);
    }
    uint offsetThumb = dm->index(row, G::OffsetThumbJPGColumn).data().toUInt();
    uint lengthThumb = dm->index(row, G::LengthThumbJPGColumn).data().toUInt();


//    qint64 offsetThumb = metadata->getOffsetThumbJPG(fPath);
//    qint64 lengthThumb = metadata->getLengthThumbJPG(fPath);  // new

//    ulong lengthThumb = metadata->getLengthThumbJPG(fPath);  // new

    {
    #ifdef ISDEBUG
    QString s = "File size = " + QString::number(imFile.size());
    s += " Offset embedded thumb = " + QString::number(offsetThumb);
    s += " Length embedded thumb = " + QString::number(lengthThumb);
    G::track(__FUNCTION__, s);
    #endif
    }

//    qDebug() << __FUNCTION__  << "Open file" << fPath;
    if (imFile.open(QIODevice::ReadOnly)) {
        if (imFile.seek(offsetThumb)) {
            QByteArray buf = imFile.read(lengthThumb);
            if (image.loadFromData(buf, "JPEG")) {
                imFile.close();
                if (image.isNull() && G::isThreadTrackingOn )
                    track(fPath, "Empty thumb");
                if (G::isThreadTrackingOn) qDebug() << G::t.restart() << "\t" << fPath << "Scaling:" << image.size();

                image = image.scaled(thumbMax, Qt::KeepAspectRatio);
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
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    #ifdef ISPROFILE
    G::track(__FUNCTION__);
    #endif
    }
//    qDebug() << __FUNCTION__ << "fPath =" << fPath;

    QFileInfo fileInfo(fPath);
    QString ext = fileInfo.completeSuffix().toLower();

    bool success = false;
    int row = dm->fPathRow[fPath];

    // Check if metadata has been cached for this image
    if (dm->index(row, G::OffsetThumbJPGColumn).data().isNull()) {
        metadata->loadImageMetadata(fPath, true, false, false, false, __FUNCTION__);
        dm->addMetadataItem(metadata->imageMetadata);
    }
    uint offsetThumb = dm->index(row, G::OffsetThumbJPGColumn).data().toUInt();
    uint lengthThumb = dm->index(row, G::LengthThumbJPGColumn).data().toUInt();

    /* A raw file may not have any embedded jpg or be corrupted.  */

    if (metadata->rawFormats.contains(ext) && lengthThumb == 0) {
        fPath = ":/images/badImage1.png";
        loadFromEntireFile(fPath, image);
        return true;
    }

    /* Reading the thumb directly from the image file is faster than using
    QImageReader (thumbReader) to read the entire jpg and then scaling it
    down. However, not all images have embedded thumbs so make a quick check.
    */

    bool readThumbFromJPG = (offsetThumb > 0); // && ext == "jpg");

    if (metadata->rawFormats.contains(ext) || readThumbFromJPG) {
        /* read the raw file thumb using metadata for offset and length of
        embedded JPG.  Check if metadata has been cached for this image.
        */
        success = loadFromData(fPath, image);

//        if (metadata->isLoaded(fPath)) {
//            success = loadFromData(fPath, image);
//        }
//        else {
//            err = "Metadata has not been loaded yet - try again";
//            if (G::isThreadTrackingOn) track(fPath, err);
//        }
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
    #ifdef ISPROFILE
    G::track(__FUNCTION__, "About to checkOrientation");
    #endif
    if (success) checkOrientation(fPath, image);
    return success;
}
