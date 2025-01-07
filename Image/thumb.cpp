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

void Thumb::setImageDimensions(QString &fPath, QImage &image, int row)
{
    int w = image.width();
    int h = image.height();
    if (h == 0) {
        QString msg = "Image width and/or height = 0.";
        G::issue("Warning", msg, "Thumb::setImageDimensions", dmRow, fPath);
        return;
    }
    double a = w * 1.0 / h;
    int alignRight = Qt::AlignRight | Qt::AlignVCenter;
    QString d = QString::number(w) + "x" + QString::number(h);
    QString src = "Thumb::setImageDimensions";

    emit setValue(dm->index(row, G::WidthColumn), w, instance, src);
    emit setValue(dm->index(row, G::WidthPreviewColumn), w, instance, src);
    emit setValue(dm->index(row, G::HeightColumn), h, instance, src);
    emit setValue(dm->index(row, G::HeightPreviewColumn), h, instance, src);
    emit setValue(dm->index(row, G::AspectRatioColumn), a, instance, src, Qt::EditRole, alignRight);
    emit setValue(dm->index(row, G::DimensionsColumn), d, instance, src, Qt::EditRole, alignRight);

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
    emit videoFrameDecode(fPath, G::maxIconSize, "dmThumb", dmIdx, dm->instance);
}

Thumb::Status Thumb::loadFromEntireFile(QString &fPath, QImage &image, int row)
{
    QString fun = "Thumb::loadFromEntireFile";
    if (G::isLogger)
        G::log(fun, fPath);
    if (instance != dm->instance) return Status::Fail;

    QFile imFile(fPath);
    if (imFile.isOpen()) {
        G::log(fun, fPath + " isAlready open");
        return Status::Open;
    }
    if (!image.load(fPath)) {
        QString msg = "Could not read thumb using QImage::load.";
        G::issue("Warning", msg, "Thumb::loadFromEntireFile", dmRow, fPath);
        return Status::Fail;
    }

    setImageDimensions(fPath, image, row);

    if (image.isNull()) {
        QString msg = "Null image returned from thumbReader.";
        G::issue("Warning", msg, "Thumb::loadFromEntireFile", dmRow, fPath);
        return Status::Fail;
    }

    return Status::Success;
}

Thumb::Status Thumb::loadFromJpgData(QString &fPath, QImage &image)
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

    QFile imFile(fPath);
    if (imFile.isOpen()) {
        QString msg = "File is already open.";
        G::issue("Warning", msg, "Thumb::loadFromJpgData", dmRow, fPath);
        return Status::Open;
    }

    if (imFile.open(QIODevice::ReadOnly)) {
        bool success = false;
        if (imFile.seek(offsetThumb)) {
            QByteArray buf = imFile.read(lengthThumb);
            success =  image.loadFromData(buf, "JPEG");
        }
        imFile.close();
        if (image.isNull()) {
            QString msg = "Null image.";
            G::issue("Warning", msg, "Thumb::loadFromJpgData", dmRow, fPath);
            return Status::Fail;
        }
        if (success) return Status::Success;
        else return Status::Fail;
    }
    else {
        return Status::Open;
    }
}

Thumb::Status Thumb::loadFromTiff(QString &fPath, QImage &image, int row)
{
    QString fun = "Thumb::loadFromTiff";
    if (G::isLogger) G::log(fun, fPath);

    QFile imFile(fPath);
    if (imFile.isOpen()) {
        QString msg = "File is already open.";
        G::issue("Warning", msg, "Thumb::loadFromTiff", dmRow, fPath);
        return Status::Open;
    }

    // use QtTiff decoder
    // from QTiffHandler, adapted for Winnow and using Winnow libtiff, which reads jpg encoding

    ImageMetadata m = dm->imMetadata(fPath);
    Tiff tiff("Thumb::loadFromTiff");
    if (!tiff.read(fPath, &image, m.offsetThumb)) {
        QString errMsg = "Could not read because QtTiff read failed.";
        G::issue("Error", errMsg, "Thumb::loadFromTiff", row, fPath);
        return Status::Fail;
    }

    // qDebug() << "Thumb::loadFromTiff" << image.width() << image.height();
    image = image.scaled(G::maxIconSize, G::maxIconSize, Qt::KeepAspectRatio, Qt::FastTransformation);

    // fix missing embedded thumbnail
    QModelIndex sfIdx = dm->sf->index(row, G::MissingThumbColumn);
    bool isMissingThumb = sfIdx.data().toBool();
    if (isMissingThumb && G::modifySourceFiles && G::autoAddMissingThumbnails) {
        if (G::backupBeforeModifying) {
            QString msg = "File(s) have been backed up before embedding thumbnail(s).<p>"
                          "Press <font color=\"red\">ESC</font> to close";
            // use relay because probably in non-gui thread
            emit G::relay->showPopUp(msg, 10000, true, 0.75, Qt::AlignHCenter);
            // emit G::relay->updateStatus(false, msg, "Thumb::loadFromTiff");  // this works
            Utilities::backup(fPath, "backup");
        }
        if (tiff.encodeThumbnail(fPath, image)) {
            dm->setValueSf(sfIdx, false, dm->instance, "Thumb::loadFromTiff");
        }
    }

    return Status::Success;





    if (tiff.read(fPath, &image, m.offsetThumb)) {
        // qDebug() << "Thumb::loadFromTiff" << image.width() << image.height();
        image = image.scaled(G::maxIconSize, G::maxIconSize, Qt::KeepAspectRatio, Qt::FastTransformation);
        return Status::Success;
    }
    else {
        QString errMsg = "Could not read because QtTiff read failed.";
        G::issue("Error", errMsg, "Thumb::loadFromTiff", row, fPath);
        return Status::Fail;
    }

    // deprecated code...

    int samplesPerPixel = dm->index(row, G::samplesPerPixelColumn).data().toInt();
    /*
    if (samplesPerPixel > 3) {
        QString msg = "Samples per pixel > 3.";
        G::issue("Warning", msg, "Thumb::loadFromTiff", dmRow, fPath);
        return Status::Fail;
    }
    //*/

    // ImageMetadata m = dm->imMetadata(fPath);
    // Tiff tiff("Thumb::loadFromTiff");

     // Attempt to decode tiff thumbnail by decoding embedded tiff thumbnail
     bool getThumb = true;
    if (isThumbOffset && tiff.decode(m, fPath, image, getThumb, G::maxIconSize)) {
        if (image.isNull()) {
            QString msg = "Tiff::decode returned a null image.";
            G::issue("Warning", msg, "Thumb::loadFromTiff", dmRow, fPath);
            return Status::Fail;
        }
        return Status::Success;
    }

     // try load entire tif using Winnow
     // qDebug() << "Thumb::loadFromTiff" << fPath;
     if (!tiff.decode(fPath, m.offsetFull, image)) {
         if (image.isNull()) {
             QString msg = "Could not read thumb using Tiff::decoder.";
             G::issue("Warning", msg, "Thumb::loadFromTiff", dmRow, fPath);
             return Status::Fail;
         }
         return Status::Success;
     }

    // use Qt tiff library to decode embedded thumbnail (does not work)
    // image = image.scaled(G::maxIconSize, G::maxIconSize, Qt::KeepAspectRatio, Qt::FastTransformation);

    return Status::Fail;
}

Thumb::Status Thumb::loadFromHeic(QString &fPath, QImage &image)
{
    if (G::isLogger) G::log("Thumb::loadFromHeic", fPath);

    QFile imFile(fPath);
    if (imFile.isOpen()) {
        return Status::Open;
    }

    #ifdef Q_OS_WIN
    Heic heic;
    // try to read heic thumbnail
    if (heic.decodeThumbnail(fPath, image)) {
        if (image.isNull()) {
            QString msg = "Could not read thumb using Heic::decodeThumbnail.";
            G::issue("Warning", msg, "Thumb::loadFromHeic", dmRow, fPath);
            return Status::Fail;
        }
        return Status::Success;
    }

    // try read entire image
    if (heic.decodePrimaryImage(fPath, image)) {
        if (image.isNull()) {
            QString msg = "Could not read thumb using Heic::decodePrimaryImage.";
            G::issue("Warning", msg, "Thumb::loadFromHeic", dmRow, fPath);
            return Status::Fail;
        }
        return Status::Success;
    }

    return Status::Fail;
    #endif

    #ifdef Q_OS_MAC
    // Heic natively supported on Mac
    if (image.load(fPath)) {
        if (image.isNull()) {
            QString msg = "Could not read thumb using QImage::load.";
            G::issue("Warning", msg, "Thumb::loadFromHeic", dmRow, fPath);
            return Status::Fail;
        }
        return Status::Success;
    }
    return Status::Fail;
    #endif
}

bool Thumb::loadThumb(QString &fPath, QImage &image, int instance, QString src)
{
/*
    Load a thumbnail preview as a decoration icon in the datamodel dm in column 0. Raw,
    jpg, heic and tif files can contain smaller previews. Check if they do and load the
    smaller preview as that is faster than loading the entire full resolution image just
    to get a thumbnail. This thumbnail is used by the grid and filmstrip views.

    Called by MW::fileSelectionChange when an icon has not been loaded and by
    MW::refreshCurrentFolder.
*/
    if (G::isLogger) G::log("Thumb::loadThumb", fPath);
    if (fPath.isEmpty()) qDebug() << "Thumb::loadThumb EMPTY FPATH";
    if (isDebug)
        qDebug() << "Thumb::loadThumb" << "Instance =" << instance << src << fPath;

    dmRow = dm->rowFromPath(fPath);

    if (G::instanceClash(instance, "Thumb::loadThumb")) {
        QString msg = "Instance clash.";
        G::issue("Comment", msg, "Thumb::loadThumb", dmRow, fPath);
        return false;
    }
    this->instance = instance;

    QFileInfo fileInfo(fPath);
    QString ext = fileInfo.suffix().toLower();

    // videos are loaded using videoFrameDecode
    bool isVideo = metadata->videoFormats.contains(ext);
    if (isVideo) {
        if (G::renderVideoThumb) {
            loadFromVideo(fPath, dmRow);
            return true;
        }
    }

    // check permissions
    oldPermissions = fileInfo.permissions();
    if (!(oldPermissions & QFileDevice::ReadUser)) {
        QFileDevice::Permissions newPermissions = fileInfo.permissions() | QFileDevice::ReadUser;
        QFile(fPath).setPermissions(newPermissions);
    }

    // get relevent metadata
    isDimensions = dm->index(dmRow, G::WidthColumn).data().toInt() > 0;
    isAspectRatio = dm->index(dmRow, G::AspectRatioColumn).data().toInt() > 0;
    isThumbOffset = dm->index(dmRow, G::OffsetThumbColumn).data().toInt() > 0;
    isThumbLength = dm->index(dmRow, G::LengthThumbColumn).data().toInt() > 0;
    offsetThumb = dm->index(dmRow, G::OffsetThumbColumn).data().toUInt();
    lengthThumb = dm->index(dmRow, G::LengthThumbColumn).data().toUInt();
    isEmbeddedThumb = offsetThumb && lengthThumb;
    /*
    qDebug() << "Thumb::loadThumb"
             << "dmRow =" << dmRow
             << "offsetThumb =" << offsetThumb
             << "lengthThumb =" << lengthThumb
             << "isEmbeddedThumb =" << isEmbeddedThumb
             << fPath
                ;
                //*/

    Status status = Status::None;
    int attempts = 0;
    int maxAttempts = 10;

    // try up to 10 times if file is open (probably ImageCaching)
    while ((status == Status::None || status == Status::Open) && attempts < maxAttempts) {

        if (G::stop) return false;

        // try again after 100ms
        if (status == Status::Open) {
            attempts++;
            G::wait(100);
        }

        // raw image file or tiff with embedded jpg thumbnail
        if (isEmbeddedThumb) {
            status = loadFromJpgData(fPath, image);
            if (status == Status::Success) break;
        }

        if (ext == "heic") {
            status = loadFromHeic(fPath, image);
            if (status == Status::Success) {
                setImageDimensions(fPath, image, dmRow);
                break;
            }
        }

        if (ext == "tif" && G::useMyTiff) {
            status = loadFromTiff(fPath, image, dmRow);
            if (status == Status::Success) break;
        }

        // all other image files
        // read the image file (supported by Qt), scaling to thumbnail size
        status = loadFromEntireFile(fPath, image, dmRow);
        if (status == Status::Success) break;

    }

    QFile(fPath).setPermissions(oldPermissions);

    if (status == Status::Success) {
        // scale to max icon size
        image = image.scaled(thumbMax, Qt::KeepAspectRatio);
        image.convertTo(QImage::Format_RGB32);

        // rotate if there is orientation metadata
        if (metadata->rotateFormats.contains(ext)) checkOrientation(fPath, image);
    }

    if (status == Status::Success) return true;
    else return false;
}

void Thumb::insertThumbnailsInJpg(QModelIndexList &selection)
{
/*
    Fix missing thumbnails in JPG.  Not being used.  Also see Jpeg::embedThumbnail().
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

        /*
        qDebug() << "Thumb::insertThumbnails" << i
                 << "fPath =" << fPath
                 << "thumbPath =" << thumbPath
                 ; //*/

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


