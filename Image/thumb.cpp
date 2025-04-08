#include "Image/thumb.h"
#include "Main/global.h"

/*
   Loads a thumbnail preview from a file based on metadata already extracted by
   mdCache. If the file contains a thumbnail jpg it is extracted. If not, then
   then entire image is read and scaled to thumbMax.
*/

Thumb::Thumb(DataModel *dm)
{
    this->dm = dm;
    metadata = new Metadata;

    thumbMax.setWidth(G::maxIconSize);
    thumbMax.setHeight(G::maxIconSize);

    connect(this, &Thumb::setValueDm, dm, &DataModel::setValueDm, Qt::QueuedConnection);
    connect(this, &Thumb::setValueSf, dm, &DataModel::setValueSf, Qt::QueuedConnection);

    isDebug = false;
}

void Thumb::abortProcessing()
{
    abort = true;
    if (frameDecoder) frameDecoder->stop();
}

void Thumb::checkOrientation(QString &fPath, QImage &image)
{
    QString fun = "Thumb::checkOrientation";
    if (isDebug)
        qDebug().noquote()
            << fun.leftJustified(col0Width)
            << "isGuiThread =" << G::isGuiThread()
            ;
    if (G::isLogger) G::log(fun, fPath);
    // check orientation and rotate if portrait
    QTransform trans;
    int row = dm->rowFromPath(fPath);
    QVariant orientation;
    // Dead lock detected in BlockingQueuedConnection when invoke used from GUI thread
    if (G::isGuiThread())
        orientation = dm->index(row, G::OrientationColumn).data().toInt();  // crash 2025-03-11
    else QMetaObject::invokeMethod(
            dm,
            "valueSf",
            Qt::BlockingQueuedConnection,
            Q_RETURN_ARG(QVariant, orientation),
            Q_ARG(int, row),
            Q_ARG(int, G::OrientationColumn)
        );
    int degrees = 0;
    int rotationDegrees = dm->index(row, G::RotationDegreesColumn).data().toInt();
    /*
    qDebug() << "Thumb::checkOrientation"
             << "orientation =" << orientation << fPath; //*/
    switch (orientation.toInt()) {
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
             << fun.leftJustified(col0Width)
             << "orientation =" << orientation
             << "rotationDegrees   =" << rotationDegrees
             << "degrees =" << QString::number(degrees).leftJustified(3, ' ')
             << "fPath   =" << fPath
                ;
    }
}

void Thumb::setImageDimensions(QString &fPath, QImage &image, int row)
{
    QString fun = "Thumb::setImageDimensions";
    if (isDebug)
        qDebug().noquote()
            << fun.leftJustified(col0Width)
            << "row =" << row;
    if (G::isLogger) G::log(fun, "row = " + QString::number(row));
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

    emit setValueDm(dm->index(row, G::WidthColumn), w, instance, src);
    emit setValueDm(dm->index(row, G::WidthPreviewColumn), w, instance, src);
    emit setValueDm(dm->index(row, G::HeightColumn), h, instance, src);
    emit setValueDm(dm->index(row, G::HeightPreviewColumn), h, instance, src);
    emit setValueDm(dm->index(row, G::AspectRatioColumn), a, instance, src, Qt::EditRole, alignRight);
    emit setValueDm(dm->index(row, G::DimensionsColumn), d, instance, src, Qt::EditRole, alignRight);

}

void Thumb::loadFromVideo(QString &fPath, int dmRow)
{
/*
    see top of FrameDecoder.cpp for documentation
*/
    QString fun = "Thumb::loadFromVideo";
    if (isDebug)
        qDebug().noquote()
            << fun.leftJustified(col0Width)
            << "row =" << dmRow
            << fPath
            ;
    if (G::isLogger) G::log(fun, fPath);

    frameDecoder = new FrameDecoder();
    connect(frameDecoder, &FrameDecoder::setFrameIcon, dm, &DataModel::setIconFromVideoFrame);
    connect(this, &Thumb::videoFrameDecode, frameDecoder, &FrameDecoder::addToQueue);

    QModelIndex dmIdx = dm->index(dmRow, 0);
    if (!abort)
        emit videoFrameDecode(fPath, G::maxIconSize, "dmThumb", dmIdx, dm->instance);
}

Thumb::Status Thumb::loadFromEntireFile(QString &fPath, QImage &image, int row)
{
    QString fun = "Thumb::loadFromEntireFile";
    if (isDebug)
        qDebug().noquote()
            << fun.leftJustified(col0Width)
            << "row =" << row << fPath;
    if (G::isLogger)
        G::log(fun, fPath);
    if (instance != dm->instance) return Status::Fail;

    QFile imFile(fPath);
    if (imFile.isOpen()) {
        G::log(fun, fPath + " isAlready open");
        return Status::Open;
    }

    if (!abort && !image.load(fPath)) {
        QString msg = "Could not read thumb using QImage::load.";
        G::issue("Warning", msg, "Thumb::loadFromEntireFile", dmRow, fPath);
        return Status::Fail;
    }

    if (!abort) setImageDimensions(fPath, image, row);

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
    if (isDebug)
        qDebug().noquote()
            << fun.leftJustified(col0Width)
            << fPath
            ;

    QFile imFile(fPath);
    if (imFile.isOpen()) {
        QString msg = "File is already open.";
        G::issue("Warning", msg, "Thumb::loadFromJpgData", dmRow, fPath);
        return Status::Open;
    }

    if (abort) return Status::Fail;

    if (imFile.open(QIODevice::ReadOnly)) {
        bool success = false;
        if (!abort) {
            if (imFile.seek(offsetThumb)) {
                QByteArray buf = imFile.read(lengthThumb);
                success =  image.loadFromData(buf, "JPEG");
            }
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
    if (isDebug)
        qDebug().noquote()
            << fun.leftJustified(col0Width)
            << "row =" << row
            << fPath
            ;

    if (abort) return Status::Fail;

    QFile imFile(fPath);
    if (imFile.isOpen()) {
        QString msg = "File is already open.";
        G::issue("Warning", msg, "Thumb::loadFromTiff", dmRow, fPath);
        return Status::Open;
    }

    // use QtTiff decoder
    // from QTiffHandler, adapted for Winnow and using Winnow libtiff, which reads jpg encoding

    ImageMetadata m = dm->imMetadata(fPath);
    if (abort) return Status::Fail;
    Tiff tiff("Thumb::loadFromTiff");
    if (abort) return Status::Fail;
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
    if (abort) return Status::Fail;
    if (isMissingThumb && G::modifySourceFiles && G::autoAddMissingThumbnails) {
        if (G::backupBeforeModifying) {
            QString msg = "File(s) have been backed up before embedding thumbnail(s).<p>"
                          "Press <font color=\"red\">ESC</font> to close";
            // use relay because probably in non-gui thread
            emit G::relay->showPopUp(msg, 10000, true, 0.75, Qt::AlignHCenter);
            // emit G::relay->updateStatus(false, msg, "Thumb::loadFromTiff");  // this works
            Utilities::backup(fPath, "backup");
        }
        if (abort) return Status::Fail;
        if (tiff.encodeThumbnail(fPath, image)) {
            emit setValueSf(sfIdx, false, dm->instance, "Thumb::loadFromTiff");
        }
    }

    return Status::Success;

    if (abort) return Status::Fail;

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
    if (abort) return Status::Fail;
    bool getThumb = true;
    if (isEmbeddedThumb && tiff.decode(m, fPath, image, getThumb, G::maxIconSize)) {
        if (image.isNull()) {
            QString msg = "Tiff::decode returned a null image.";
            G::issue("Warning", msg, "Thumb::loadFromTiff", dmRow, fPath);
            return Status::Fail;
        }
        return Status::Success;
    }

     // try load entire tif using Winnow
     // qDebug() << "Thumb::loadFromTiff" << fPath;
    if (abort) return Status::Fail;
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
    QString fun = "Thumb::loadFromHeic";
    if (isDebug)
        qDebug().noquote()
            << fun.leftJustified(col0Width)
            << fPath
            ;
    if (G::isLogger) G::log(fun, fPath);

    if (abort) return Status::Fail;
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
    if (abort) return Status::Fail;
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

void Thumb::presetOffset(uint offset, uint length)
{
/*
    MetaRead reads the metadata and then the thumbnail.  If the thumbnail is embedded
    the offset and length are in the metadata, which is saved in the DataModel.  The
    thumbnail is decoded next, but the DataModel may not yet have been updated, so the
    MetaRead Reader presets the offset and length here.
*/
    QString fun = "Thumb::presetOffset";
    if (isDebug)
        qDebug().noquote()
            << fun.leftJustified(col0Width)
            << "offset =" << offset << "length =" << length << "isGUI =" << G::isGuiThread();
    if (G::isLogger) G::log(fun);
    offsetThumb = offset;
    lengthThumb = length;
    isPresetOffset = true;
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
    QString fun = "Thumb::loadThumb";
    if (isDebug)
        qDebug().noquote()
            << fun.leftJustified(col0Width)
            << "Instance =" << instance << src << fPath;
    if (G::isLogger) G::log(fun, fPath);

    abort = false;

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
            if (!abort) loadFromVideo(fPath, dmRow);
            return true;
        }
    }

    // check permissions
    oldPermissions = fileInfo.permissions();
    if (!(oldPermissions & QFileDevice::ReadUser)) {
        QFileDevice::Permissions newPermissions = fileInfo.permissions() | QFileDevice::ReadUser;
        QFile(fPath).setPermissions(newPermissions);
    }

    // get offset and length (both zero if not embedded thumb)
    if (!isPresetOffset) {
        offsetThumb = dm->index(dmRow, G::OffsetThumbColumn).data().toUInt();
        lengthThumb = dm->index(dmRow, G::LengthThumbColumn).data().toUInt();
    }
    isPresetOffset = false;
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
        if (abort) return false;

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
                if (!abort) setImageDimensions(fPath, image, dmRow);
                break;
            }
        }

        if (ext == "tif" && G::useMyTiff) {
            if (!abort) status = loadFromTiff(fPath, image, dmRow);
            if (status == Status::Success) break;
        }

        // all other image files
        // read the image file (supported by Qt), scaling to thumbnail size
        if (!abort) status = loadFromEntireFile(fPath, image, dmRow);
        if (status == Status::Success) break;

    }

    QFile(fPath).setPermissions(oldPermissions);

    if (status == Status::Success) {
        // scale to max icon size
        image = image.scaled(thumbMax, Qt::KeepAspectRatio);
        image.convertTo(QImage::Format_RGB32);

        // rotate if there is orientation metadata
        if (!abort)
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


