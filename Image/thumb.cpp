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

void Thumb::checkOrientation(QString &fPath, QImage &image)
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__, fPath);
    #endif
    }
    // check orientation and rotate if portrait
//    #ifdef ISPROFILE
//    G::track(__FUNCTION__, "About to QTransform trans");
//    #endif
    QTransform trans;
    int row = dm->fPathRow[fPath];
    qDebug() << __FUNCTION__ << "row =" << row;
    int orientation = dm->index(row, G::OrientationColumn).data().toInt();
    qDebug() << __FUNCTION__ << "orientation = =" << orientation;
    switch(orientation) {
        case 6:
            trans.rotate(90);
            image = image.transformed(trans, Qt::SmoothTransformation);
            qDebug() << __FUNCTION__ << "After orientation 6";
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
    QFile imFile(fPath);
    if (imFile.isOpen()) imFile.close();
    QImageReader thumbReader;
    if (!imFile.open(QIODevice::ReadOnly)) {
        dm->error(row, "Unable to open file.", __FUNCTION__);
        return false;
    }
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
    if (image.isNull()) {
//        qDebug() << __FUNCTION__ << "Could not read thumb using thumbReader.";
        dm->error(row, "Could not read thumb using thumbReader.", __FUNCTION__);
        return false;
    }
    return true;
}

bool Thumb::loadFromJpgData(QString &fPath, QImage &image)
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__, fPath);
    #endif
    }
//    qDebug() << __FUNCTION__ << fPath;
    thumbMax.setWidth(G::maxIconSize);
    thumbMax.setHeight(G::maxIconSize);
    bool success = false;
    err = "";
    QFile imFile(fPath);
     if (imFile.isOpen()) {
         qDebug() << __FUNCTION__ << fPath << "is already open - return";
         return false;
     }
//    if (imFile.isOpen()) imFile.close();
    int row = dm->fPathRow[fPath];

    // Check if metadata has been cached for this image
    if (dm->index(row, G::OffsetThumbColumn).data().isNull()) {
        metadata->loadImageMetadata(fPath, true, false, false, false, __FUNCTION__);
        dm->addMetadataForItem(metadata->m);
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

    if (imFile.isOpen()) imFile.close();
    if (imFile.open(QIODevice::ReadOnly)) {
        if (imFile.seek(offsetThumb)) {
            QByteArray buf = imFile.read(lengthThumb);
            if (image.loadFromData(buf, "JPEG")) {
                qDebug() << __FUNCTION__
                         << "image.loadFromData succeeded for" << fPath
                         << "isReadable =" << imFile.isReadable()
                            ;
//                imFile.close();
                qDebug() << __FUNCTION__ << "1";
                if (image.isNull())
                    dm->error(row, "Empty thumb.", __FUNCTION__);
                qDebug() << __FUNCTION__ << "2";
                image = image.scaled(thumbMax, Qt::KeepAspectRatio);
                qDebug() << __FUNCTION__ << "3";
                success = true;
            }
            else {
                qDebug() << __FUNCTION__ << "image.loadFromData failed for" << fPath
                         << "isReadable =" << imFile.isReadable();
//                imFile.close();
            }
        }
        else {
//            imFile.close();
            dm->error(row, "Illegal offset to thumb.", __FUNCTION__);
        }
        imFile.close();
    }
    else {
        // file busy, wait a bit and try again
        qDebug() << __FUNCTION__ << "Could not open file for thumb.";
        dm->error(row, "Could not open file for thumb.", __FUNCTION__);
    }
    if (err != "") qDebug() << __FUNCTION__ << err;
    return success;
}

bool Thumb::loadFromTiffData(QString &fPath, QImage &image)
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__, fPath);
    #endif
    }
    QFile imFile(fPath);
    if (imFile.isOpen()) imFile.close();
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
Load a thumbnail preview as a decoration icon in the datamodel dm in column 0. Raw, jpg and
tif files can contain smaller previews. Check if they do and load the smaller preview as that
is faster than loading the entire full resolution image just to get a thumbnail.
*/
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__, fPath);
    #endif
    }
    /*
    qDebug() << __FUNCTION__ << "fPath =" << fPath;
//    */
    QFileInfo fileInfo(fPath);
    QString ext = fileInfo.suffix().toLower();
    int dmRow = dm->fPathRow[fPath];
    QString err;

    // The image type might not have metadata we can read, so load entire image and resize
    if (!metadata->getMetadataFormats.contains(ext)) {
        return loadFromEntireFile(fPath, image, dmRow);
    }

    // Check if metadata has been loaded for this image
    if (!dm->index(dmRow, G::MetadataLoadedColumn).data().toBool()) {
        if (!dm->readMetadataForItem(dmRow)) {
            dm->error(dmRow, "Could not load metadata.", __FUNCTION__);
            dm->setData(dm->index(dmRow, G::ErrColumn), err);
            qDebug() << __FUNCTION__ << "Could not load metadata." << fPath;
            return false;
        }
    }
    /*
    if (dm->index(dmRow, G::MetadataLoadedColumn).data().isNull()) {
        metadata->loadImageMetadata(fPath, true, false, false, false, __FUNCTION__);
        dm->addMetadataForItem(metadata->m);
    }  */

    // Is there an embedded thumbnail?
    uint offsetThumb = dm->index(dmRow, G::OffsetThumbColumn).data().toUInt();
    uint lengthThumb = dm->index(dmRow, G::LengthThumbColumn).data().toUInt();
    bool thumbFound = (offsetThumb && lengthThumb) || ext == "heic";

    // A raw file may not have any embedded jpg or be corrupted.  Try read entire image.
    // rgh should we be trying to read the entire image and reduce to thumb in this case?
    /*
    if (!thumbFound) {
        QString path = ":/images/badImage1.png";
        loadFromEntireFile(path, image, dmRow);
        err += "No embedded thumbnail" + fPath + ". ";
        dm->setData(dm->index(dmRow, G::ErrColumn), err);
        qDebug() << __FUNCTION__ << err << fPath;
        return false;
    } */
    /*
    if (metadata->hasJpg.contains(ext) && !thumbFound) {
        QString path = ":/images/badImage1.png";
        loadFromEntireFile(path, image, dmRow);
        return false;
    } */

    /* Reading the thumb directly from the image file is faster than using
    QImageReader (thumbReader) to read the entire image and then scaling it
    down. However, not all images have embedded thumbs so make a quick check.
    */
    if (thumbFound) {
        if (ext == "tif") {
            // test for too many samples which causes libTiff to crash
            int samplesPerPixel = dm->index(dmRow, G::samplesPerPixelColumn).data().toInt();
            if (samplesPerPixel > 3) {
                err = "Could not read tiff because " + QString::number(samplesPerPixel)
                      + " samplesPerPixel > 3. ";
                dm->error(dmRow, err, __FUNCTION__);
                qDebug() << __FUNCTION__ << err;
                return false;
            }
            // try to get thumb ourselves
            if (!loadFromTiffData(fPath, image)) {
                qDebug() << __FUNCTION__ << "loadFromTiffData Failed, trying loadFromEntireFile" << fPath;
                dm->error(dmRow, "loadFromTiffData Failed, trying loadFromEntireFile.", __FUNCTION__);
                // no thumb, try read entire full size image
                if (!loadFromEntireFile(fPath, image, dmRow)) {
                    dm->error(dmRow, "Failed to load thumb from loadFromEntireFile.", __FUNCTION__);
                    qDebug() << __FUNCTION__ << "Failed to load thumb from loadFromEntireFile.";
                    // show bad image png
                    QString path = ":/images/badImage1.png";
                    loadFromEntireFile(path, image, dmRow);
                    return false;
                }
            }
        }
        // rgh remove heic
        else if (ext == "heic") {
            ImageMetadata m = dm->imMetadata(fPath);
            #ifdef Q_OS_WIN
            Heic heic;
            // try to read heic thumbnail
            if (!heic.decodeThumbnail(m, fPath, image)) {
                qDebug() << __FUNCTION__ << "Unable to read heic thumbnail";
                dm->error(dmRow, "Unable to read heic thumbnail.", __FUNCTION__);
                return false;
            }
            #endif
        }
        // all other cases where embedded thumbnail - most raw file formats
        else if (!loadFromJpgData(fPath, image)) {
            dm->error(dmRow, "Failed to load thumb.", __FUNCTION__);
            qDebug() << __FUNCTION__ << "Failed to load thumb." << fPath;
            // rgh should we be trying to read the full size thumb in this case?
            return false;
        }
    }
    else  {
        // read the image file (supported by Qt), scaling to thumbnail size
        if (!loadFromEntireFile(fPath, image, dmRow)) {
            err += "Could not load thumb. ";
            dm->setData(dm->index(dmRow, G::ErrColumn), err);
            qDebug() << __FUNCTION__ << err << fPath;
            return false;
        }
        image.convertTo(QImage::Format_RGB32);
    }

    checkOrientation(fPath, image);
    return true;
}
