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
    G::track(__FUNCTION__, fPath);
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
    G::track(__FUNCTION__, fPath);
    #endif
    }
    thumbMax.setWidth(G::maxIconSize);
    thumbMax.setHeight(G::maxIconSize);
    bool success = false;
    err = "";
    QFile imFile(fPath);
    QImageReader thumbReader;
    if (imFile.open(QIODevice::ReadOnly)) {
        // close file to allow qt thumbReader to work
        imFile.close();
        // let thumbReader do its thing
        thumbReader.setFileName(fPath);
        QSize size = thumbReader.size();
        qDebug() << __FUNCTION__ << fPath << "thumbReader.imageCount =" << thumbReader.imageCount();
//        size.scale(thumbMax, Qt::KeepAspectRatio);
//        thumbReader.setScaledSize(size);
        image = thumbReader.read();
        success = !image.isNull();
        if (!success) {
            err = "Could not read thumb using thumbReader";
            if (G::isThreadTrackingOn) track(fPath, err);
        }
    }
    else err = "Unable to open " + fPath;
    if (err != "") qDebug() << __FUNCTION__ << err;
    return success;
}

bool Thumb::loadFromData(QString &fPath, QImage &image)
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__, fPath);
    #endif
    }
    thumbMax.setWidth(G::maxIconSize);
    thumbMax.setHeight(G::maxIconSize);
    bool success = false;
    err = "";
    QFile imFile(fPath);
    int row = dm->fPathRow[fPath];

    // Check if metadata has been cached for this image
    if (dm->index(row, G::OffsetThumbJPGColumn).data().isNull()) {
        metadata->loadImageMetadata(fPath, true, false, false, false, __FUNCTION__);
        dm->addMetadataForItem(metadata->imageMetadata);
    }
    uint offsetThumb = dm->index(row, G::OffsetThumbJPGColumn).data().toUInt();
    uint lengthThumb = dm->index(row, G::LengthThumbJPGColumn).data().toUInt();

    QFileInfo info(imFile);
    QString ext = info.suffix().toLower();

    {
    #ifdef ISDEBUG
    QString s = "File size = " + QString::number(imFile.size());
    s += " Offset embedded thumb = " + QString::number(offsetThumb);
    s += " Length embedded thumb = " + QString::number(lengthThumb);
    G::track(__FUNCTION__, s);
    #endif
    }

    if (imFile.open(QIODevice::ReadOnly)) {
        if (imFile.seek(offsetThumb)) {
            QByteArray buf = imFile.read(lengthThumb);
            if (image.loadFromData(buf, "JPEG")) {
                imFile.close();
                if (image.isNull())
                    err = "Empty thumb " + fPath;
                image = image.scaled(thumbMax, Qt::KeepAspectRatio);
                success = true;
            }
            else {
                imFile.close();
            }
        }
        else {
            imFile.close();
            err = "Illegal offset to thumb " + fPath;
        }
    }
    else {
        // file busy, wait a bit and try again
        err = "Could not open file for thumb " + fPath;
    }
    if (err != "") qDebug() << __FUNCTION__ << err;
    return success;
}

bool Thumb::loadThumb(QString &fPath, QImage &image)
{
/*

*/
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__, fPath);
    #endif
    }
//    qDebug() << __FUNCTION__ << "fPath =" << fPath;

    QFileInfo fileInfo(fPath);
    QString ext = fileInfo.suffix().toLower();

    bool success = false;
    err = "";
    int row = dm->fPathRow[fPath];

    // Check if metadata has been cached for this image
    if (dm->index(row, G::MetadataLoadedColumn).data().isNull()) {
        metadata->loadImageMetadata(fPath, true, false, false, false, __FUNCTION__);
        dm->addMetadataForItem(metadata->imageMetadata);
    }
    uint offsetThumb = dm->index(row, G::OffsetThumbJPGColumn).data().toUInt();
    uint lengthThumb = dm->index(row, G::LengthThumbJPGColumn).data().toUInt();
    bool thumbFound = offsetThumb && lengthThumb;

    /* A raw file may not have any embedded jpg or be corrupted.  */
    if (metadata->rawFormats.contains(ext) && !thumbFound) {
        QString path = ":/images/badImage1.png";
        loadFromEntireFile(path, image);
        return false;
    }

    /* Reading the thumb directly from the image file is faster than using
    QImageReader (thumbReader) to read the entire jpg and then scaling it
    down. However, not all images have embedded thumbs so make a quick check.
    */
    if (thumbFound) {
        success = loadFromData(fPath, image);
        if (!success) {
//            err = "Failed to load thumb for " + fPath;
        }
    }
    else  {
        // read the image file (supported by Qt), scaling to thumbnail size
        if(loadFromEntireFile(fPath, image)) {
              success = true;
        }
        // check if file is locked by another process
        else {
//            err = "Could not open file for image";    // try again
//            if (G::isThreadTrackingOn) track(fPath, err);
        }
    }
    if (err != "") {
        qDebug() << __FUNCTION__ << err;
        dm->setData(dm->index(row, G::ErrColumn), err);
    }
    if (success) checkOrientation(fPath, image);
    return success;
}
