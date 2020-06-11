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

bool Thumb::loadFromEntireFile(QString &fPath, QImage &image, int row)
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

        dm->setData(dm->index(row, G::WidthColumn), size.width());
        dm->setData(dm->index(row, G::WidthFullColumn), size.width());
        dm->setData(dm->index(row, G::HeightColumn), size.height());
        dm->setData(dm->index(row, G::HeightFullColumn), size.height());

        size.scale(thumbMax, Qt::KeepAspectRatio);
        thumbReader.setScaledSize(size);
        image = thumbReader.read();
        success = !image.isNull();
        if (!success) {
            err = "Could not read thumb using thumbReader:" + fPath;
            if (G::isThreadTrackingOn) track(fPath, err);
        }
    }
    else err = "Unable to open " + fPath;
    if (err != "") qDebug() << __FUNCTION__ << err;
    return success;
}

bool Thumb::loadFromJpgData(QString &fPath, QImage &image)
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
    if (dm->index(row, G::OffsetThumbColumn).data().isNull()) {
        metadata->loadImageMetadata(fPath, true, false, false, false, __FUNCTION__);
        dm->addMetadataForItem(metadata->imageMetadata);
    }
    uint offsetThumb = dm->index(row, G::OffsetThumbColumn).data().toUInt();
    uint lengthThumb = dm->index(row, G::LengthThumbColumn).data().toUInt();

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

bool Thumb::loadFromTiffData(QString &fPath, QImage &image)
{
    QFile imFile(fPath);
    // check if file is locked by another process
     if (imFile.open(QIODevice::ReadOnly)) {
        // close it to allow qt load to work
        imFile.close();
     }

     // Attempt to decode tiff thumbnail by sampling tiff raw data
     ImageMetadata m = dm->imMetadata(fPath);
     Tiff tiff;
     return tiff.decode(m, fPath, image, G::maxIconSize);
}

bool Thumb::loadThumb(QString &fPath, QImage &image)
{
/*
Load a thumbnail preview as a decoration icon in the datamodel dm in column 0. Raw files, jpg
and tif files can contain smaller previews. Check if they do and load the smaller preview as
that is faster than loading the entire full resolution image just to get a thumbnail.
*/
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__, fPath);
    #endif
    }
//    qDebug() << __FUNCTION__ << "fPath =" << fPath;

    QFileInfo fileInfo(fPath);
    QString ext = fileInfo.suffix().toLower();

    bool moreThanOnce = false;
    bool success = false;
    err = "";
    int row = dm->fPathRow[fPath];

    // The image type might not have metadata we can read, so load entire image and resize
    if (!metadata->getMetadataFormats.contains(ext)) {
        return loadFromEntireFile(fPath, image, row);
    }

    // Check if metadata has been cached for this image
    if (dm->index(row, G::MetadataLoadedColumn).data().isNull()) {
        metadata->loadImageMetadata(fPath, true, false, false, false, __FUNCTION__);
        dm->addMetadataForItem(metadata->imageMetadata);
    }

    // Is there an embedded thumbnail?
    uint offsetThumb = dm->index(row, G::OffsetThumbColumn).data().toUInt();
    uint lengthThumb = dm->index(row, G::LengthThumbColumn).data().toUInt();
    bool thumbFound = (offsetThumb && lengthThumb) || ext == "heic";

    // A raw file may not have any embedded jpg or be corrupted.
    if (metadata->rawFormats.contains(ext) && !thumbFound) {
        QString path = ":/images/badImage1.png";
        loadFromEntireFile(path, image, row);
        return false;
    }

    /* Reading the thumb directly from the image file is faster than using
    QImageReader (thumbReader) to read the entire jpg and then scaling it
    down. However, not all images have embedded thumbs so make a quick check.
    */
    do {
        if (thumbFound) {
            if (ext == "tif") {
                // test for too many samples which causes libTiff to crash
                int samplesPerPixel = dm->index(row, G::samplesPerPixelColumn).data().toInt();
                if (samplesPerPixel > 3) {
                    err = "Could not read tiff because " + QString::number(samplesPerPixel)
                          + " samplesPerPixel > 3";
                    success = false;
                    break;
                }
                // try to get thumb ourselves
                success = loadFromTiffData(fPath, image);
                if (!success) {
                    qDebug() << __FUNCTION__ << "loadFromTiffData Failed, trying loadFromEntireFile" << fPath;
                    success = loadFromEntireFile(fPath, image, row);
                    if (!success) {
                        err = "Failed to load thumb";
                    }
                }
            }
            else if (ext == "heic") {
                ImageMetadata m = dm->imMetadata(fPath);
                Heic heic;
                success = heic.decodeThumbnail(m, fPath, image);
            }
            else success = loadFromJpgData(fPath, image);
            if (!success) {
                err = "Failed to load thumb";
            }
        }
        else  {
            // read the image file (supported by Qt), scaling to thumbnail size
            if(loadFromEntireFile(fPath, image, row)) {
                  success = true;
                  if (success) image.convertTo(QImage::Format_RGB32);
            }
            // check if file is locked by another process
            else {
                err = "Could not open file";    // try again
            }
        }
//        if (success && metadata->noMetadataFormats.contains(ext)) {
//            dm->setData(dm->index(row, G::WidthColumn), image.width());
//            dm->setData(dm->index(row, G::WidthFullColumn), image.width());
//            dm->setData(dm->index(row, G::HeightColumn), image.height());
//            dm->setData(dm->index(row, G::HeightFullColumn), image.height());
//        }
    } while(moreThanOnce);

    if (err != "") {
        qDebug() << __FUNCTION__ << err;
        dm->setData(dm->index(row, G::ErrColumn), err);
    }
    if (success) checkOrientation(fPath, image);
    return success;
}
