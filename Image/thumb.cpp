#include "Image/thumb.h"

/*
   Loads a thumbnail preview from a file based on metadata already extracted by
   mdCache. If the file contains a thumbnail jpg it is extracted. If not, then
   then entire image is read and scaled to thumbMax.
*/

Thumb::Thumb(DataModel *dm, Metadata *metadata,
             FrameDecoder *frameDecoder)
{
    this->dm = dm;
    this->metadata = metadata;
    this->frameDecoder = frameDecoder;

    thumbMax.setWidth(G::maxIconSize);
    thumbMax.setHeight(G::maxIconSize);

    connect(this, &Thumb::setValue, dm, &DataModel::setValue, Qt::QueuedConnection);
    connect(this, &Thumb::videoFrameDecode, frameDecoder, &FrameDecoder::addToQueue);

    isDebug = false;
}

void Thumb::checkOrientation(QString &fPath, QImage &image)
{
    if (G::isLogger) G::log("Thumb::checkOrientation", fPath);
    // check orientation and rotate if portrait
    QTransform trans;
    int row = dm->rowFromPath(fPath);
    int orientation = dm->index(row, G::OrientationColumn).data().toInt();
    int degrees = 0;
    int rotationDegrees = dm->index(row, G::RotationDegreesColumn).data().toInt();
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

void Thumb::loadFromVideo(QString &fPath, int dmRow)
{
/*
    see top of FrameDecoder.cpp for documentation
*/
    if (G::isLogger) G::log("Thumb::loadFromVideo", fPath);

    if (isDebug)
    {
    qDebug() << "Thumb::loadFromVideo                     "
             << "row =" << dmRow
             << fPath
                ;
    }

    QModelIndex dmIdx = dm->index(dmRow, 0);
    emit videoFrameDecode(fPath, dmIdx, dm->instance);
}

bool Thumb::loadFromEntireFile(QString &fPath, QImage &image, int row)
{
    QString fun = "Thumb::loadFromEntireFile";
    if (G::isLogger) G::log(fun, fPath);
    if (instance != dm->instance) return false;

//    QImageReader thumbReader(fPath);
//    thumbReader.setAutoTransform(true);
//    image = thumbReader.read();
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
    double a = w * 1.0 / h;
    QString src = "Thumb::loadFromEntireFile";

    int alignRight = Qt::AlignRight | Qt::AlignVCenter;

    emit setValue(dm->index(row, G::WidthColumn), w, instance, src);
    emit setValue(dm->index(row, G::WidthPreviewColumn), w, instance, src);
    emit setValue(dm->index(row, G::HeightColumn), h, instance, src);
    emit setValue(dm->index(row, G::HeightPreviewColumn), h, instance, src);
    emit setValue(dm->index(row, G::AspectRatioColumn), a, instance, src, Qt::EditRole, alignRight);

    // scale thumb/icon
//    image = image.scaled(thumbMax, Qt::KeepAspectRatio);

    if (image.isNull()) {
        G::error("Could not read thumb using thumbReader", fun, fPath);
        qWarning() << "WARNING" << "loadFromEntireFile" << "Could not read thumb using thumbReader." << fPath;
        return false;
    }

    return true;
}

bool Thumb::loadFromJpgData(QString &fPath, QImage &image)
{
    QString fun = "Thumb::loadFromJpgData";
    if (G::isLogger) G::log(fun, fPath);

    /*
    qDebug() << "Thumb::loadFromJpgData"
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

bool Thumb::loadFromTiff(QString &fPath, QImage &image, int row)
{
    QString fun = "Thumb::loadFromTiffData";
    if (G::isLogger) G::log(fun, fPath);
    QFile imFile(fPath);
    if (imFile.isOpen()) {
        qWarning() << "WARNING" << "Thumb::loadFromTiff" << fPath << "is already open - return";
        return false;
    }

    int samplesPerPixel = dm->index(row, G::samplesPerPixelColumn).data().toInt();
    if (samplesPerPixel > 3) {
        QString err = "Could not read tiff because " + QString::number(samplesPerPixel)
              + " samplesPerPixel > 3.";
        G::error(err, fun, fPath);
        return false;
    }

    ImageMetadata m = dm->imMetadata(fPath);
    Tiff tiff;

    // Attempt to decode tiff thumbnail by sampling tiff raw data
    bool getThumb = true;
    if (isThumbOffset && tiff.decode(m, fPath, image, getThumb, G::maxIconSize)) return true;

    // try load entire tif using Winnow decoder
    if (!tiff.decode(fPath, m.offsetFull, image)) return true;

    // use Qt tiff library to decode
    if (image.load(fPath)) return true;

    return false;
}

bool Thumb::loadFromHeic(QString &fPath, QImage &image)
{
    if (G::isLogger) G::log("Thumb::loadFromHeic", fPath);

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

bool Thumb::loadThumb(QString &fPath, QImage &image, int instance, QString src)
{
/*
    Load a thumbnail preview as a decoration icon in the datamodel dm in column 0. Raw,
    jpg, heic and tif files can contain smaller previews. Check if they do and load the
    smaller preview as that is faster than loading the entire full resolution image just
    to get a thumbnail. This thumbnail is used by the grid and filmstrip views.
*/
    if (G::isLogger) G::log("Thumb::loadThumb", fPath);
    if (isDebug) qDebug() << "Thumb::loadThumb" << "Instance =" << instance << src << fPath;
    if (G::instanceClash(instance, "Thumb::loadThumb")) {
        qWarning() << "WARNING" << "Thumb::loadThumb instance clash";
        return false;
    }
    this->instance = instance;

    // get status information
    QFileInfo fileInfo(fPath);
    // check permissions
    oldPermissions = fileInfo.permissions();
    if (!(oldPermissions & QFileDevice::ReadUser)) {
        QFileDevice::Permissions newPermissions = fileInfo.permissions() | QFileDevice::ReadUser;
        QFile(fPath).setPermissions(newPermissions);
    }
    QString ext = fileInfo.suffix().toLower();
    int dmRow = dm->rowFromPath(fPath);

    isDimensions = dm->index(dmRow, G::WidthColumn).data().toInt() > 0;
    isAspectRatio = dm->index(dmRow, G::AspectRatioColumn).data().toInt() > 0;
    isThumbOffset = dm->index(dmRow, G::OffsetThumbColumn).data().toInt() > 0;
    isThumbLength = dm->index(dmRow, G::LengthThumbColumn).data().toInt() > 0;
    offsetThumb = dm->index(dmRow, G::OffsetThumbColumn).data().toUInt();
    lengthThumb = dm->index(dmRow, G::LengthThumbColumn).data().toUInt();
    /*
    qDebug() << "Thumb::loadThumb"
             << "dmRow =" << dmRow
             << "offsetThumb =" << offsetThumb
             << "lengthThumb =" << lengthThumb
                ;
                //*/
    isEmbeddedThumb = offsetThumb && lengthThumb;
    bool isVideo = metadata->videoFormats.contains(ext);
    // req'd ??
    bool isMetadataLoaded = dm->index(dmRow, G::MetadataLoadedColumn).data().toBool();

    bool success = false;

    // if video file then just show video icon unless G::renderVideoThumb
    if (isVideo) {
        if (G::renderVideoThumb) {
            loadFromVideo(fPath, dmRow);
        }
        success = true;
    }

    // raw image file or tiff with embedded jpg
    else if (isEmbeddedThumb) {
        success = loadFromJpgData(fPath, image);
    }

    // The image type might not have metadata we can read, so load entire image and resize
    else if (!metadata->hasMetadataFormats.contains(ext)) {
        success = loadFromEntireFile(fPath, image, dmRow);
    }

    else if (ext == "heic") {
        success = loadFromHeic(fPath, image);
    }

    else if (ext == "tif") {
        success = loadFromTiff(fPath, image, dmRow);
    }

    // all other image files
    else  {
        // read the image file (supported by Qt), scaling to thumbnail size
        success = loadFromEntireFile(fPath, image, dmRow);
        image.convertTo(QImage::Format_RGB32);
    }

    if (!isVideo) {
        if (success && !isVideo) {
            // scale to max icon size
            image = image.scaled(thumbMax, Qt::KeepAspectRatio);
            image.convertTo(QImage::Format_RGB32);

            // rotate if there is orientation metadata
            //if (metadata->rotateFormats.contains(ext)) checkOrientation(fPath, image);
        }
        else {
            // show bad image png
            QString path = ":/images/badImage1.png";
            loadFromEntireFile(path, image, dmRow);
            G::error("Could not load video thumbnail.", "Thumb::loadThumb", fPath);
            qWarning() << "WARNING" << "Thumb::loadThumb" << "Could not load thumb." << fPath;
        }
    }

    /*if (metadata->rotateFormats.contains(ext)) */checkOrientation(fPath, image);
    QFile(fPath).setPermissions(oldPermissions);

    /*
    qDebug() << "Thumb::loadThumb"
             << "dmRow =" << dmRow
             << "success =" << success
        ; //*/
    return success;
}

void Thumb::insertThumbnailsInJpg(QModelIndexList &selection)
{
/*
    Fix missing thumbnails in JPG
*/
    qDebug() << "Thumb::insertThumbnailsInJpg";
    int count = selection.count();

    G::popUp->setProgressVisible(true);
    G::popUp->setProgressMax(count);
    QString txt = "Embedding thumbnail(s) for " + QString::number(count) +
                  " JPG images <p>Press <font color=\"red\"><b>Esc</b></font> to abort.";
    G::popUp->showPopup(txt, 0, true, 1);
    insertingThumbnails = true;


    ExifTool et;
    et.setOverWrite(true);
    QStringList thumbList;
    for (int i = 0; i < count; ++i) {
        G::popUp->setProgress(i+1);
        if (abort) break;

        // check if already a thumbnail
        int offsetThumb = selection.at(i).data(G::OffsetThumbColumn).toInt();
        int offsetFull = selection.at(i).data(G::OffsetFullColumn).toInt();
        if (offsetThumb != offsetFull) continue;

        // collect path information
        QString fPath = selection.at(i).data(G::PathRole).toString();
        QFileInfo info(fPath);
        QString folder = info.dir().path();
        QString base = info.baseName();
        QString thumbPath = folder + "/" + base + "_thumb.jpg";

        // add this thumbPath to the list
        thumbList << thumbPath;

//        qDebug() << "Thumb::insertThumbnails" << i
//                 << "fPath =" << fPath
//                 << "thumbPath =" << thumbPath
//                 ;
//        continue;

        // create a thumbnail size jpg
        QImage thumb = QImage(fPath).scaled(160, 160, Qt::KeepAspectRatio);
        thumb.save(thumbPath, "JPG", 60);

        // add the thumb.jpg to the source file
        et.addThumb(thumbPath, fPath);
    }
    et.close();
    insertingThumbnails = false;

    // delete the thumbnail files
    for (int i = 0; i < thumbList.length(); ++i) {
        QFile::remove(thumbList.at(i));
    }
    G::popUp->setProgressVisible(false);
    G::popUp->reset();
}

/*
void Thumb::insertThumbnails(QModelIndexList &selection)
{
    if (G::isLogger) G::log("Thumb::insertThumbnails");

    int count = selection.count();
    if (count == 0) {
        G::popUp->showPopup("No items selected");
        return;
    }

    // within selection how many missing thumbnails
    int missingThumbnailCount = 0;
    for (int i = 0; i < count; ++i) {
        QModelIndex fullIdx = dm->sf->index(selection.at(i).row(), G::OffsetFullColumn);
        QModelIndex thumbIdx = dm->sf->index(selection.at(i).row(), G::OffsetThumbColumn);
        QModelIndex typeIdx = dm->sf->index(selection.at(i).row(), G::TypeColumn);
        int offsetThumb = thumbIdx.data().toInt();
        int offsetFull = fullIdx.data().toInt();
        QString formatType = typeIdx.data().toString().toLower();
        if (offsetThumb == offsetFull && formatType == "jpg") missingThumbnailCount++;
    }
    if (missingThumbnailCount == 0) {
        G::popUp->showPopup("No jpg with missing thumbnails");
        qDebug() << "No missing thumbnails";
        return;
    }

    G::popUp->setProgressVisible(true);
    G::popUp->setProgressMax(count);
    QString txt = "Adding thumbnail(s) for " + QString::number(missingThumbnailCount) +
                  " images <p>Press <font color=\"red\"><b>Esc</b></font> to abort.";
    G::popUp->showPopup(txt, 0, true, 1);
    insertingThumbnails = true;

    ExifTool et;
    et.setOverWrite(true);
    QStringList thumbList;
    for (int i = 0; i < count; ++i) {
        G::popUp->setProgress(i+1);
        if (abort) break;

        // check if already a thumbnail
        int offsetThumb = selection.at(i).data(G::OffsetThumbColumn).toInt();
        int offsetFull = selection.at(i).data(G::OffsetFullColumn).toInt();
        if (offsetThumb != offsetFull) continue;

        // collect path information
        QString fPath = selection.at(i).data(G::PathRole).toString();
        QFileInfo info(fPath);
        QString folder = info.dir().path();
        QString base = info.baseName();
        QString thumbPath = folder + "/" + base + "_thumb.jpg";

        // add this thumbPath to the list
        thumbList << thumbPath;

//        qDebug() << "Thumb::insertThumbnails" << i
//                 << "fPath =" << fPath
//                 << "thumbPath =" << thumbPath
//                 ;
//        continue;

        // create a thumbnail size jpg
        QImage thumb = QImage(fPath).scaled(160, 160, Qt::KeepAspectRatio);
        thumb.save(thumbPath, "JPG", 60);

        // add the thumb.jpg to the source file
        et.addThumb(thumbPath, fPath);
    }
    et.close();
    insertingThumbnails = false;

    // delete the thumbnail files
    for (int i = 0; i < thumbList.length(); ++i) {
        QFile::remove(thumbList.at(i));
    }
    G::popUp->setProgressVisible(false);
    G::popUp->end();
} */

