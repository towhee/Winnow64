#include "Image/pixmap.h"
#include "Main/global.h"

Pixmap::Pixmap(QObject *parent, DataModel *dm, Metadata *metadata) : QObject(parent)
{
    this->dm = dm;
    this->metadata = metadata;
}

bool Pixmap::load(QString &fPath, QPixmap &pm)
{
    QImage image;
    bool success = load(fPath, image);
    pm = QPixmap::fromImage(image);
    return success;
}

bool Pixmap::loadFromHeic(QString &fPath, QImage &image)
{
    QFile imFile(fPath);
    // check if file is locked by another process
     if (imFile.open(QIODevice::ReadOnly)) {
        // close it to allow qt load to work
        imFile.close();
     }

     // Attempt to decode heic image
     ImageMetadata m = dm->imMetadata(fPath);
     #ifdef Q_OS_WIN
     // rgh remove heic
     Heic heic;
     return heic.decodePrimaryImage(m, fPath, image);
     #endif
}

bool Pixmap::load(QString &fPath, QImage &image)
{
/*  Reads the embedded jpg (known offset and length) and converts it into a
    QImage.

    This function is dependent on metadata being updated first.  Metadata is
    updated by the mdCache metadataCacheThread that runs every time a new folder is
    selected. This function is used in the imageCache thread that stores
    pixmaps on the heap.

    Most of the time the image will be obtained from the imageCache, but when
    the image has yet to be cached this function is called from imageView and
    compareView. This often happens when a new folder is selected and the
    program is trying to load the metadata, thumbnail and image caches plus
    show the first image in the folder.

    Since this function, in the main program thread, may be competing with the
    cache building it will retry attempting to load for a given period of time
    as the image file may be locked by one of the cache builders.

    If it succeeds in opening the file it still has to read the embedded jpg and
    convert it to a QImage.  If this fails then either the file format is not
    being properly read or the file is corrupted.  In this case the metadata
    will be updated to show the file is not readable.
*/
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__, fPath);
    #endif
    }
//    qDebug() << __FUNCTION__ << "fPath =" << fPath;
    QElapsedTimer t;
    t.restart();

    QFileInfo fileInfo(fPath);
    QString ext = fileInfo.completeSuffix().toLower();
    QFile imFile(fPath);
    int dmRow = dm->fPathRow[fPath];

    QString err = dm->index(dmRow, G::ErrColumn).data().toString();

    // is metadata loaded
    if (!dm->index(dmRow, G::MetadataLoadedColumn).data().toBool()) {
        if (!dm->readMetadataForItem(dmRow)) {
            err += "Could not load metadata" + fPath + ". ";
            dm->setData(dm->index(dmRow, G::ErrColumn), err);
            qDebug() << __FUNCTION__ << err << fPath;
            return false;
        }
    }

    // is file already open by another process
    if (imFile.isOpen()) {
        err += "File already open" + fPath + ". ";
        qDebug() << __FUNCTION__ << err;
        dm->setData(dm->index(dmRow, G::ErrColumn), err);
        return false;
    }

    // try to open image file
    if (!imFile.open(QIODevice::ReadOnly)) {
        err += "Could not open file for image" + fPath + ". ";
        qDebug() << __FUNCTION__ << err;
        dm->setData(dm->index(dmRow, G::ErrColumn), err);
        return false;
    }

    // JPG format (including embedded in raw files)
    if (metadata->hasJpg.contains(ext)) {
        // get offset and length of embedded Jpg from the datamodel
        uint offsetFullJpg = dm->index(dmRow, G::OffsetFullColumn).data().toUInt();
        uint lengthFullJpg = dm->index(dmRow, G::LengthFullColumn).data().toUInt();

        // make sure legal offset by checking the length
        if (lengthFullJpg == 0) {
            imFile.close();
            err += "Jpg length = zero " + fPath + ". ";
            qDebug() << __FUNCTION__ << err;
            dm->setData(dm->index(dmRow, G::ErrColumn), err);
            return false;
        }

        // try to read the data
        if (!imFile.seek(offsetFullJpg)) {
            imFile.close();
            err += "Illegal offset to image" + fPath + ". ";
            qDebug() << __FUNCTION__ << err;
            dm->setData(dm->index(dmRow, G::ErrColumn), err);
            return false;
        }

        QByteArray buf = imFile.read(lengthFullJpg);

        // try to decode the jpg data
        if (!image.loadFromData(buf, "JPEG")) {
            imFile.close();
            err += "Could not read image from buffer" + fPath + ". ";
            dm->setData(dm->index(dmRow, G::ErrColumn), err);
            return false;
        }

        imFile.close();
    }

    // HEIC format
    // rgh remove heic
    else if (metadata->hasHeic.contains(ext)) {
        ImageMetadata m = dm->imMetadata(fPath);
        #ifdef Q_OS_WIN
        Heic heic;

        // try to decode
        if (!heic.decodePrimaryImage(m, fPath, image)) {
            imFile.close();
            err += "Unable to decode " + fPath + ". ";
            qDebug() << __FUNCTION__ << err;
            dm->setData(dm->index(dmRow, G::ErrColumn), err);
            return false;
        }
        imFile.close();
        #endif
    }

    // TIFF format
    else if (ext == "tif") {
        // check for sampling format we cannot read
        int samplesPerPixel = dm->index(dmRow, G::samplesPerPixelColumn).data().toInt();
        if (samplesPerPixel > 3) {
            imFile.close();
            err += "Could not read tiff because " + QString::number(samplesPerPixel)
                  + " samplesPerPixel > 3. " + fPath + ". ";
            qDebug() << __FUNCTION__ << err;
            dm->setData(dm->index(dmRow, G::ErrColumn), err);
            return false;
        }

        // use Qt tiff library to decode
        bool useTIFFLib = true;
        if (useTIFFLib) {
            // try to decode
            if (!image.load(fPath)) {
                imFile.close();
                err += "Could not decode " + fPath + ". ";
                qDebug() << __FUNCTION__ << err;
                dm->setData(dm->index(dmRow, G::ErrColumn), err);
                return false;
            }
        }
        // use Winnow decoder (only for thumbnails at this time)
        else {
            ImageMetadata m = dm->imMetadata(fPath);
            Tiff tiff;
            if (!tiff.decode(m, fPath, image)) {
                imFile.close();
                err += "Could not decode " + fPath + ". ";
                qDebug() << __FUNCTION__ << err;
                dm->setData(dm->index(dmRow, G::ErrColumn), err);
                return false;
            }
        }
        imFile.close();
    }

    // All other formats
    else {
        // try to decode
        if (!image.load(fPath)) {
            imFile.close();
            err += "Could not decode " + fPath + ". ";
            qDebug() << __FUNCTION__ << err;
            dm->setData(dm->index(dmRow, G::ErrColumn), err);
            return false;
        }
        imFile.close();
    }

    // Successfully loaded to a QImage

    // rotate if portrait image
    #ifdef ISDEBUG
    G::track(__FUNCTION__, "Loaded " + fPath);
    #endif

    if (metadata->getMetadataFormats.contains(ext)) {
        QTransform trans;
        int orientation = dm->index(dmRow, G::OrientationColumn).data().toInt();
        int rotationDegrees = dm->index(dmRow, G::RotationDegreesColumn).data().toInt();
        int degrees;
        if (orientation) {
            switch(orientation) {
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
        ICC::setInProfile(dm->index(dmRow, G::ICCBufColumn).data().toByteArray());
        ICC::transform(image);
    }
    #endif

    // calc read/decode performance
    double mp = dm->index(dmRow, G::MegaPixelsColumn).data().toDouble();
    qint64 msec = t.elapsed();
    int msecPerMp = static_cast<int>(msec / mp);
    dm->setData(dm->index(dmRow, G::LoadMsecPerMpColumn), msecPerMp, Qt::EditRole);

    #ifdef ISDEBUG
    G::track(__FUNCTION__, "Completed load image");
    #endif

    return true;
}


//bool Pixmap::load(QString &fPath, QImage &image)
//{
///*  Reads the embedded jpg (known offset and length) and converts it into a
//    QImage.

//    This function is dependent on metadata being updated first.  Metadata is
//    updated by the mdCache metadataCacheThread that runs every time a new folder is
//    selected. This function is used in the imageCache thread that stores
//    pixmaps on the heap.

//    Most of the time the image will be obtained from the imageCache, but when
//    the image has yet to be cached this function is called from imageView and
//    compareView. This often happens when a new folder is selected and the
//    program is trying to load the metadata, thumbnail and image caches plus
//    show the first image in the folder.

//    Since this function, in the main program thread, may be competing with the
//    cache building it will retry attempting to load for a given period of time
//    as the image file may be locked by one of the cache builders.

//    If it succeeds in opening the file it still has to read the embedded jpg and
//    convert it to a QImage.  If this fails then either the file format is not
//    being properly read or the file is corrupted.  In this case the metadata
//    will be updated to show the file is not readable.
//*/
//    {
//    #ifdef ISDEBUG
//    G::track(__FUNCTION__, fPath);
//    #endif
//    }
////    qDebug() << __FUNCTION__ << "fPath =" << fPath;

//    bool success = false;
//    QString err = "";       // type of error

//    bool moreThanOnce = false;
//    int totDelay = 500;     // milliseconds
//    int msDelay = 0;        // total incremented delay
//    uint msInc = 10;        // amount to increment each try

//    uint offsetFullJpg = 0;
//    uint lengthFullJpg = 0;
//    QFileInfo fileInfo(fPath);
//    QString ext = fileInfo.completeSuffix().toLower();
//    QFile imFile(fPath);
//    int dmRow = dm->fPathRow[fPath];

//    // is metadata loaded
//    if (!dm->index(dmRow, G::MetadataLoadedColumn).data().toBool()) {
//        if (!dm->readMetadataForItem(dmRow)) {
//            err += "Could not load metadata" + fPath + ". ";
//            dm->setData(dm->index(dmRow, G::ErrColumn), err);
//            qDebug() << __FUNCTION__ << err << fPath;
//            return false;
//        }
//    }

//    if (metadata->hasJpg.contains(ext)) {
//        do {
//            offsetFullJpg = dm->index(dmRow, G::OffsetFullColumn).data().toUInt();
//            lengthFullJpg = dm->index(dmRow, G::LengthFullColumn).data().toUInt();
//            // try to read the file
//            if (offsetFullJpg > 0 && lengthFullJpg > 0) {
//                if (imFile.isOpen()) {
//                    err += "File already open" + fPath + ". ";
//                    qDebug() << __FUNCTION__ << err;
//                    break;
//                }
//                if (imFile.open(QIODevice::ReadOnly)) {
//                    bool seekSuccess = imFile.seek(offsetFullJpg);
//                    if (seekSuccess) {
//                        QByteArray buf = imFile.read(lengthFullJpg);
//                        if (image.loadFromData(buf, "JPEG")) {
//                            imFile.close();
//                            #ifdef Q_OS_WIN
//                            if (buf.length() > 0 && G::colorManage)
//                                ICC::setInProfile(dm->index(dmRow, G::ICCBufColumn).data().toByteArray());
//                                ICC::transform(image);
//                            #endif
//                            success = true;
//                            break;
//                        }
//                        else {
//                            err += "Could not read image from buffer" + fPath + ". ";
//                            qDebug() << __FUNCTION__ << err;
//                            imFile.close();
//                            break;
//                        }
//                    }
//                    else {
//                        err += "Illegal offset to image" + fPath + ". ";
//                        qDebug() << __FUNCTION__ << err;
//                        imFile.close();
//                        break;
//                    }
//                }
//                else {
//                    err += "Could not open file for image" + fPath + ". ";    // try again
//                    qDebug() << __FUNCTION__ << err;
//                }
//            }
//            else {
//                err += "Illegal offset to image or no length available" + fPath + ". ";
//                qDebug() << __FUNCTION__ << err;
//                break;
//            }

//            QThread::msleep(msInc);
//            msDelay += msInc;
//            break;
//        }
//        while (msDelay < totDelay && !success);
//    }
//    else {
//        // cooked files like tif, png etc
//        do {
//            // check if file is locked by another process
//             if (imFile.open(QIODevice::ReadOnly)) {
//                // close it to allow qt load to work
//                imFile.close();
//                if (ext == "tif") {
//                    int samplesPerPixel = dm->index(dmRow, G::samplesPerPixelColumn).data().toInt();
//                    if (samplesPerPixel > 3) {
//                        err += "Could not read tiff because " + QString::number(samplesPerPixel)
//                              + " samplesPerPixel > 3. ";
//                        break;
//                    }
//                }
//                if (ext == "tif" && G::isTest == true) {
//                    ImageMetadata m = dm->imMetadata(fPath);
//                    Tiff tiff;
//                    success = tiff.decode(m, fPath, image);
//                }
//                else if (ext == "heic") {
//                    ImageMetadata m = dm->imMetadata(fPath);
//                    #ifdef Q_OS_WIN
//                    Heic heic;
//                    success = heic.decodePrimaryImage(m, fPath, image);
//                    #endif
//                }
//                else {
//                    success = image.load(fPath);
//                    if (success) image.convertTo(QImage::Format_RGB32);
//                }
//                if (!success) {
//                    err += "Could not read image " + fPath + ". ";
////                    qDebug() << __FUNCTION__ << err;
//                    break;
//                }

//                #ifdef Q_OS_WIN
//                if (G::colorManage && metadata->iccFormats.contains(ext)) {
//                    ICC::setInProfile(dm->index(dmRow, G::ICCBufColumn).data().toByteArray());
//                    ICC::transform(image);
//                }
//                #endif
//            }
//            else {
//                err += "Could not open file for image. ";    // try again
//                QThread::msleep(msInc);
//                msDelay += msInc;
//            }
//        }
//        while (moreThanOnce/*msDelay < totDelay && !success*/);
//    }

//    // rotate if portrait image
//    #ifdef ISDEBUG
//    G::track(__FUNCTION__, "Loaded " + fPath);
//    #endif

//    if (metadata->getMetadataFormats.contains(ext)) {
//        QTransform trans;
//        int orientation = dm->index(dmRow, G::OrientationColumn).data().toInt();
//        int rotationDegrees = dm->index(dmRow, G::RotationDegreesColumn).data().toInt();
//        int degrees;
//        if (orientation) {
//            switch(orientation) {
//                case 6:
//                    degrees = rotationDegrees + 90;
//                    if (degrees > 360) degrees = degrees - 360;
//                    trans.rotate(degrees);
//                    image = image.transformed(trans, Qt::SmoothTransformation);
//                    break;
//                case 8:
//                    degrees = rotationDegrees + 270;
//                    if (degrees > 360) degrees = degrees - 360;
//                    trans.rotate(degrees);
//                    image = image.transformed(trans, Qt::SmoothTransformation);
//                    break;
//            }
//        }
//        else if (rotationDegrees){
//            trans.rotate(rotationDegrees);
//            image = image.transformed(trans, Qt::SmoothTransformation);
//        }
//    }

//    // record any errors
//    if (!success) {
//        dm->setData(dm->index(dmRow, G::ErrColumn), err);
//        qDebug() << __FUNCTION__ << fPath << err;
//    }

//    #ifdef ISDEBUG
//    G::track(__FUNCTION__, "Completed load image");
//    #endif

//    return success;
//}
