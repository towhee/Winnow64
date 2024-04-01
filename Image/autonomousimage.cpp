#include "autonomousimage.h"

/*
   Decodes a QImage. If the file contains a thumbnail jpg it is extracted. If
   not, then then entire image is read and scaled to thumbMax.
*/

AutonomousImage::AutonomousImage(Metadata *metadata, FrameDecoder *frameDecoder)
    : metadata(metadata), frameDecoder(frameDecoder)
{
    thumbMax.setWidth(G::maxIconSize);
    thumbMax.setHeight(G::maxIconSize);

    connect(this, &AutonomousImage::videoFrameDecode, frameDecoder, &FrameDecoder::addToQueue);

    isDebug = false;
}

void AutonomousImage::checkOrientation(QString &fPath, QImage &image)
{
    if (G::isLogger) G::log("AutonomousImage::checkOrientation", fPath);
    // check orientation and rotate if portrait
    QTransform trans;
    int orientation = m->orientation;
    int degrees = 0;
    int rotationDegrees = m->rotationDegrees;
    /*
    qDebug() << "Thumb::checkOrientation"
             << "orientation =" << orientation << fPath; //*/
    switch (orientation) {
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

    if (isDebug)
    {
        qDebug().noquote()
            << "Thumb::checkOrientation"
            << "orientation =" << orientation
            << "rotationDegrees   =" << rotationDegrees
            << "degrees =" << QString::number(degrees).leftJustified(3, ' ')
            << "fPath   =" << fPath
            ;
    }
}

void AutonomousImage::loadFromVideo(QString &fPath)
{
/*
    see top of FrameDecoder.cpp for documentation
*/
    if (G::isLogger) G::log("AutonomousImage::loadFromVideo", fPath);

    if (isDebug)
    {
        qDebug() << "AutonomousImage::loadFromVideo                     "
                 << "longSide =" << longSide
                 << fPath
            ;
    }

    QModelIndex idx = QModelIndex();  // dummy currently req'd by FrameDecoder
    emit videoFrameDecode(fPath, longSide, source, idx, G::dmInstance);
}

bool AutonomousImage::loadFromEntireFile(QString &fPath, QImage &image)
{
    QString fun = "AutonomousImage::loadFromEntireFile";
    if (G::isLogger) G::log(fun, fPath);

    if (!image.load(fPath)) {
        qWarning() << "WARNING" << "loadFromEntireFile" << "Could not read thumb using QImage::load." << fPath;
        return false;
    }

    //    QFile(fPath).setPermissions(oldPermissions);
    int w = image.width();
    int h = image.height();
    if (h == 0) {
        qWarning() << "WARNING" << "loadFromEntireFile" << "Could not read thumb using thumbReader." << fPath;
        return false;
    }

    aspectRatio = w * 1.0 / h;

    if (image.isNull()) {
        // G::error("Could not read thumb using thumbReader", fun, fPath);
        qWarning() << "WARNING" << "loadFromEntireFile" << "Could not read thumb using thumbReader." << fPath;
        return false;
    }

    return true;
}

bool AutonomousImage::loadFromJpgData(QString &fPath, QImage &image)
{
    QString fun = "AutonomousImage::loadFromJpgData";
    if (G::isLogger) G::log(fun, fPath);

    /*
    qDebug() << "AutonomousImage::loadFromJpgData"
             << "offsetThumb =" << offsetThumb
             << "lengthThumb =" << lengthThumb
             << "fPath =" << fPath
                ;
                //*/
    bool success = false;

    QFile imFile(fPath);
    if (imFile.isOpen()) {
        qWarning() << "WARNING" << "Thumb::loadFromJpgData" << fPath << "is already open - return";
        return false;
    }

    if (imFile.open(QIODevice::ReadOnly)) {
        if (imFile.seek(offsetThumb)) {
            QByteArray buf = imFile.read(lengthThumb);
            success =  image.loadFromData(buf, "JPEG");
        }
        imFile.close();
        return success;
    }
    return false;
}

bool AutonomousImage::loadFromTiff(QString &fPath, QImage &image)
{
    QString fun = "AutonomousImage::loadFromTiffData";
    if (G::isLogger) G::log(fun, fPath);
    QFile imFile(fPath);
    if (imFile.isOpen()) {
        qWarning() << "WARNING" << "Thumb::loadFromTiff" << fPath << "is already open - return";
        return false;
    }

    if (m->samplesPerPixel > 3) {
        QString err = "Could not read tiff because " + QString::number(m->samplesPerPixel)
                      + " samplesPerPixel > 3.";
        // G::error(err, fun, fPath);
        return false;
    }

    Tiff tiff("AutonomousImage::load");

    // Attempt to decode tiff thumbnail by sampling tiff raw data
    bool getThumb = true;
    if (isThumbOffset && tiff.decode(*m, fPath, image, getThumb, G::maxIconSize)) return true;

    // try load entire tif using Winnow decoder
    if (!tiff.decode(fPath, m->offsetFull, image)) return true;

    // use Qt tiff library to decode
    if (image.load(fPath)) return true;

    return false;
}

bool AutonomousImage::loadFromHeic(QString &fPath, QImage &image)
{
    // if (G::isLogger)
        G::log("AutonomousImage::loadFromHeic", fPath);

    #ifdef Q_OS_WIN
    Heic heic;
    // try to read heic thumbnail
    if (heic.decodeThumbnail(fPath, image)) return true;

    // try read entire image
    if (heic.decodePrimaryImage(fPath, image)) return true;

    return false;
    #endif

    #ifdef Q_OS_MAC
    // Heic natively supported on Mac
    return image.load(fPath);
    #endif
}

bool AutonomousImage::image(QString &fPath, QImage &image, int longSide, QString source)
{
/*
    Decode the image file into a QImage, resized to longSide.  If longSide == 0 then
    do not resize.
*/
    if (G::isLogger) G::log("AutonomousImage::image", fPath);
    if (isDebug)
        qDebug() << "AutonomousImage::image" << fPath;

    this->longSide = longSide;
    this->source = source;

    // get status information
    QFileInfo fileInfo(fPath);

    // check permissions
    oldPermissions = fileInfo.permissions();
    if (!(oldPermissions & QFileDevice::ReadUser)) {
        QFileDevice::Permissions newPermissions = fileInfo.permissions() | QFileDevice::ReadUser;
        QFile(fPath).setPermissions(newPermissions);
    }

    thumbMax.setWidth(longSide);
    thumbMax.setHeight(longSide);

    QString ext = fileInfo.suffix().toLower();

    // get metadata for image at fPath
    // if (metadata->loadImageMetadata(fileInfo, instance, true, true, false, true, "AutonomousImage::image")) {
    //     m = &metadata->m;
    // }
    // else {
    //     QString errMsg = "Failed to load metadata";
    //     if (G::isWarningLogger)
    //         qWarning() << "WARNING" << "DataModel::readMetadataForItem" << "Failed to load metadata for " << fPath;
    //     QFile(fPath).setPermissions(oldPermissions);
    //     return false;
    // }
    metadata->loadImageMetadata(fileInfo, instance, true, true, false, true, "AutonomousImage::image");
    m = &metadata->m;

    //int dmRow = dm->rowFromPath(fPath);
    isDimensions = m->width > 0;
    isThumbOffset = m->offsetThumb > 0;
    isThumbLength = m->lengthThumb > 0;
    offsetThumb = m->offsetThumb;
    lengthThumb = m->lengthThumb;
    /*
    qDebug() << "AutonomousImage::image"
             << "offsetThumb =" << offsetThumb
             << "lengthThumb =" << lengthThumb
             << "fPath =" << fPath
                ;
                //*/
    isEmbeddedJpg = offsetThumb && lengthThumb;
    bool isVideo = metadata->videoFormats.contains(ext);

    bool success = false;

    // if video file do we render image or ignore???
    if (isVideo) {
        loadFromVideo(fPath);
        success = true;
    }

    // raw image file or tiff with embedded jpg
    else if (isEmbeddedJpg) {
        success = loadFromJpgData(fPath, image);
    }

    // The image type might not have metadata we can read, so load entire image and resize
    else if (!metadata->hasMetadataFormats.contains(ext)) {
        success = loadFromEntireFile(fPath, image);
    }

    else if (ext == "heic") {
        success = loadFromHeic(fPath, image);
    }

    else if (ext == "tif") {
        success = loadFromTiff(fPath, image);
    }

    // all other image files
    else  {
        // read the image file (supported by Qt), scaling to thumbnail size
        success = loadFromEntireFile(fPath, image);
        image.convertTo(QImage::Format_RGB32);
    }

    if (!isVideo) {
        if (success) {
            if (longSide) {
            // scale to max icon size
            image = image.scaled(thumbMax, Qt::KeepAspectRatio);
            image.convertTo(QImage::Format_RGB32);
            }

            // rotate if there is orientation metadata
            if (metadata->rotateFormats.contains(ext)) checkOrientation(fPath, image);
        }
        else {
            // show bad image png
            QString path = ":/images/badImage1.png";
            loadFromEntireFile(path, image);
            // G::error("Could not load video thumbnail.", "Thumb::loadThumb", fPath);
            qWarning() << "WARNING" << "Thumb::loadThumb" << "Could not load thumb." << fPath;
        }
    }

    QFile(fPath).setPermissions(oldPermissions);

    /*
    qDebug() << "Thumb::loadThumb"
             << "dmRow =" << dmRow
             << "success =" << success
        ; //*/
    return success;
}
