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
    connect(this, &Thumb::setValue, dm, &DataModel::setValue);
    connect(this, &Thumb::videoFrameDecode, frameDecoder, &FrameDecoder::addToQueue);
}

void Thumb::checkOrientation(QString &fPath, QImage &image)
{
    if (G::isLogger) G::log("Thumb::checkOrientation", fPath);
//    qDebug() << CLASSFUNCTION << fPath;
    // check orientation and rotate if portrait
    QTransform trans;
    int row = dm->fPathRow[fPath];
    int orientation = dm->index(row, G::OrientationColumn).data().toInt();
//    qDebug() << CLASSFUNCTION << orientation << fPath;
    switch(orientation) {
        case 3:
            trans.rotate(180);
            image = image.transformed(trans, Qt::SmoothTransformation);
            break;
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

void Thumb::loadFromVideo(QString &fPath, int dmRow)
{
/*
    see top of FrameDecoder.cpp for documentation
*/
    if (G::isLogger) G::log("Thumb::loadFromVideo", fPath);

//    qDebug() << "Thumb::loadFromVideo                  "
//             << "row =" << dmRow
//                ;
    QModelIndex dmIdx = dm->index(dmRow, 0);
    emit videoFrameDecode(fPath, dmIdx, dm->instance);
}

bool Thumb::loadFromEntireFile(QString &fPath, QImage &image, int row)
{
    if (G::isLogger) G::log("Thumb::loadFromEntireFile", fPath);
    thumbMax.setWidth(G::maxIconSize);
    thumbMax.setHeight(G::maxIconSize);
//    QFile imFile(fPath);
//    if (imFile.isOpen()) imFile.close();

    QImageReader thumbReader(fPath);
    thumbReader.setAutoTransform(true);
    image = thumbReader.read();
    int w = image.width();
    int h = image.height();
    double a = w / h;
    emit setValue(dm->index(row, G::WidthColumn), w);
    emit setValue(dm->index(row, G::WidthPreviewColumn), w);
    emit setValue(dm->index(row, G::HeightColumn), h);
    emit setValue(dm->index(row, G::HeightPreviewColumn), h);
    emit setValue(dm->index(row, G::AspectRatioColumn), a);

    image = image.scaled(thumbMax, Qt::KeepAspectRatio);
    if (image.isNull()) {
        G::error("loadFromEntireFile", fPath, "Could not read thumb using thumbReader.");
        qWarning() << "loadFromEntireFile" << "Could not read thumb using thumbReader." << fPath;
        return false;
    }
    return true;
}

bool Thumb::loadFromJpgData(QString &fPath, QImage &image)
{
    if (G::isLogger) G::log("Thumb::loadFromJpgData", fPath);
    thumbMax.setWidth(G::maxIconSize);
    thumbMax.setHeight(G::maxIconSize);
    bool success = false;
    QFile imFile(fPath);
     if (imFile.isOpen()) {
         qDebug() << "Thumb::loadFromJpgData" << fPath << "is already open - return";
         return false;
     }
    int row = dm->fPathRow[fPath];

    // Check if metadata has been cached for this image
    QFileInfo info(imFile);  // qt6.2
    if (dm->index(row, G::OffsetThumbColumn).data().isNull()) {
        metadata->loadImageMetadata(info, true, false, false, false, CLASSFUNCTION); // qt6.2
        dm->addMetadataForItem(metadata->m);
    }
    uint offsetThumb = dm->index(row, G::OffsetThumbColumn).data().toUInt();
    uint lengthThumb = dm->index(row, G::LengthThumbColumn).data().toUInt();

//    QString ext = info.suffix().toLower();

    /*
    QString s = "File size = " + QString::number(imFile.size());
    s += " Offset embedded thumb = " + QString::number(offsetThumb);
    s += " Length embedded thumb = " + QString::number(lengthThumb);
    s += " " + fPath;
    qDebug() << CLASSFUNCTION << s;
//    G::log(CLASSFUNCTION, s);
//*/

    if (imFile.isOpen()) imFile.close();
    if (imFile.open(QIODevice::ReadOnly)) {
        if (imFile.seek(offsetThumb)) {
            QByteArray buf = imFile.read(lengthThumb);
            if (image.loadFromData(buf, "JPEG")) {
                if (image.isNull()) {
                    G::error(CLASSFUNCTION, fPath, "Empty thumb.");
                    qWarning() << "Error loading thumbnail" << imFile.fileName();
                }
                image = image.scaled(thumbMax, Qt::KeepAspectRatio);
                success = true;
            }
        }
        else {
            G::error("Thumb::loadFromJpgData", fPath, "Could not open file for thumb.");
        }
        imFile.close();
    }
    else {
        // file busy, wait a bit and try again
        G::error("Thumb::loadFromJpgData", fPath, "Could not open file for thumb.");
    }
    return success;
}

bool Thumb::loadFromTiffData(QString &fPath, QImage &image)
{
    if (G::isLogger) G::log("Thumb::loadFromTiffData", fPath);
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
    bool getThumb = true;
    return tiff.decode(m, fPath, image, getThumb, G::maxIconSize);
}

bool Thumb::loadThumb(QString &fPath, QImage &image, QString src)
{
/*
    Load a thumbnail preview as a decoration icon in the datamodel dm in column 0. Raw, jpg
    and tif files can contain smaller previews. Check if they do and load the smaller preview
    as that is faster than loading the entire full resolution image just to get a thumbnail.
    This thumbnail is used by the grid and filmstrip views.
*/
    if (G::isLogger) G::log("Thumb::loadThumb", fPath);
    QFileInfo fileInfo(fPath);
    QString ext = fileInfo.suffix().toLower();
    int dmRow = dm->fPathRow[fPath];

    // If video file then just show video icon
    if (metadata->videoFormats.contains(ext)) {
        loadFromVideo(fPath, dmRow);
        return true;
    }

    // The image type might not have metadata we can read, so load entire image and resize
    if (!metadata->hasMetadataFormats.contains(ext)) {
        return loadFromEntireFile(fPath, image, dmRow);
    }

    // Heic natively supported on Mac
    #ifdef Q_OS_MAC
    if (ext == "heic") {
        return loadFromEntireFile(fPath, image, dmRow);
    }
    #endif

    // Check if metadata has been loaded for this image
    if (!dm->index(dmRow, G::MetadataLoadedColumn).data().toBool()) {
        if (!dm->readMetadataForItem(dmRow)) {
            G::error("Thumb::loadThumb", fPath, "Could not load metadata.");
            return false;
        }
    }
    /*
    if (dm->index(dmRow, G::MetadataLoadedColumn).data().isNull()) {
        metadata->loadImageMetadata(fPath, true, false, false, false, CLASSFUNCTION);
        dm->addMetadataForItem(metadata->m);
    }  */

    // Is there an embedded thumbnail?
    uint offsetThumb = dm->index(dmRow, G::OffsetThumbColumn).data().toUInt();
    uint lengthThumb = dm->index(dmRow, G::LengthThumbColumn).data().toUInt();
    bool thumbFound = (offsetThumb) || ext == "heic";
//    if (ext == "tif" && dm->index(dmRow, G::LengthThumbColumn).data().toString() == "");

    /* Reading the thumb directly from the image file is faster than using QImageReader
    (thumbReader) to read the entire image and then scaling it down. However, not all
    images have embedded thumbs, so make a quick check.*/
    if (thumbFound) {
        if (ext == "tif" && lengthThumb == 0) {
            // test for too many samples which causes libTiff to crash
            int samplesPerPixel = dm->index(dmRow, G::samplesPerPixelColumn).data().toInt();
            if (samplesPerPixel > 3) {
                QString err = "Could not read tiff because " + QString::number(samplesPerPixel)
                      + " samplesPerPixel > 3.";
                G::error("Thumb::loadThumb", fPath, err);
                return false;
            }
            // try to get thumb ourselves
            if (!loadFromTiffData(fPath, image)) {
                G::error("Thumb::loadThumb", fPath, "loadFromTiffData Failed, trying loadFromEntireFile.");
                // no thumb, try read entire full size image
                if (!loadFromEntireFile(fPath, image, dmRow)) {
                    G::error("Thumb::loadThumb", fPath, "Failed to load thumb from loadFromEntireFile.");
                    // show bad image png
                    QString path = ":/images/badImage1.png";
                    loadFromEntireFile(path, image, dmRow);
                    return false;
                }
                qDebug() << "Thumb::loadThumb" << fPath << "Loaded thumb using Qt";
            }
//            qDebug() << CLASSFUNCTION << fPath << "Loaded thumb by resampling tiff";
        }
        // rgh remove heic
        else if (ext == "heic") {
            ImageMetadata m = dm->imMetadata(fPath);
            #ifdef Q_OS_WIN
            Heic heic;
            // try to read heic thumbnail
            if (!heic.decodeThumbnail(fPath, image)) {
                G::error(CLASSFUNCTION, fPath, "Unable to read heic thumbnail.");
                return false;
            }
            #endif
        }
        // all other cases where embedded thumbnail - most raw file formats
        else if (!loadFromJpgData(fPath, image)) {
            G::error("Thumb::loadThumb", fPath, "Failed to load thumb.");
            // rgh should we be trying to read the full size thumb in this case?
            return false;
        }
    }
    else  {
        // read the image file (supported by Qt), scaling to thumbnail size
        if (!loadFromEntireFile(fPath, image, dmRow)) {
            G::error("Thumb::loadThumb", fPath, "Could not load thumb.");
            qWarning() << "Thumb::loadThumb" << "Could not load thumb." << fPath;
            return false;
        }
        image.convertTo(QImage::Format_RGB32);
    }

    if (metadata->rotateFormats.contains(ext)) checkOrientation(fPath, image);
    return true;
}

void Thumb::insertThumbnails(QModelIndexList &selection)
{
/*

*/
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
//        qApp->processEvents();
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

//        qDebug() << CLASSFUNCTION << i
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
    G::popUp->hide();
}

