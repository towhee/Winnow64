#include "Image/pixmap.h"
#include "Main/global.h"

Pixmap::Pixmap(QObject *parent, DataModel *dm, Metadata *metadata) : QObject(parent)
{
    this->dm = dm;
    this->metadata = metadata;
#if (QT_VERSION >= QT_VERSION_CHECK(6, 0, 0))
    QImageReader::setAllocationLimit(1024);
#endif
    connect(this, &Pixmap::setValue, dm, &DataModel::setValue);
}

bool Pixmap::load(QString &fPath, QPixmap &pm, QString src)
{
    QImage image;
    bool success = load(fPath, image, src);
    pm = QPixmap::fromImage(image);
    return success;
}

bool Pixmap::loadFromHeic(QString &fPath, QImage &image)
{
    QFile imFile(fPath);
    // check if file is locked by another process   rgh why not just imFile.isOpen
     if (imFile.open(QIODevice::ReadOnly)) {
        // close it to allow qt load to work
        imFile.close();
     }

     // Attempt to decode heic image
     ImageMetadata m = dm->imMetadata(fPath);
     #ifdef Q_OS_WIN
     // rgh remove heic
     Heic heic;
     return heic.decodePrimaryImage(fPath, image);
     #endif
}

bool Pixmap::load(QString &fPath, QImage &image, QString src)
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
    if (G::isLogger) G::log("Pixmap::load", fPath);
    QElapsedTimer t;
    t.restart();
//    QElapsedTimer readTime;
    QElapsedTimer decodeTime;
//    QElapsedTimer ICCTime;
//    qint64 tRead;
//    qint64 tDecode;
//    qint64 tICC;
//    readTime.start();

    QFileInfo fileInfo(fPath);
    QString ext = fileInfo.completeSuffix().toLower();

    if (metadata->videoFormats.contains(ext)) return false;

    QFile imFile(fPath);
//    if (imFile.isOpen()) imFile.close();
    int dmRow = dm->fPathRow[fPath];

    // is metadata loaded
    if (!dm->index(dmRow, G::MetadataLoadedColumn).data().toBool()) {
        if (!dm->readMetadataForItem(dmRow)) {
            QString err = "Could not load metadata.";
            G::error("Pixmap::load", fPath, err);
            return false;
        }
    }

    // is file already open by another process
    if (imFile.isOpen()) {
        QString err = "File already open.";
        G::error("Pixmap::load", fPath, err);
        return false;
    }

    // try to open image file
    if (!imFile.open(QIODevice::ReadOnly)) {
        imFile.close();
        QString err = "Could not open file for image.";
        G::error("Pixmap::load", fPath, err);
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
            QString err = "Jpg length = zero.";
            G::error("Pixmap::load", fPath, err);
            return false;
        }

        // try to read the data
        if (!imFile.seek(offsetFullJpg)) {
            imFile.close();
            QString err = "Illegal offset to image.";
            G::error("Pixmap::load", fPath, err);
            return false;
        }

        QByteArray buf = imFile.read(lengthFullJpg);

//        tRead = readTime.elapsed();
//        decodeTime.start();

//        qDebug() << CLASSFUNCTION << "use decodeScan";
//        Jpeg jpg;
//        jpg.decodeScan(buf, image);

        // try to decode the jpg data
        if (!image.loadFromData(buf, "JPEG")) {
            imFile.close();
            QString err = "Could not read image from buffer.";
            G::error("Pixmap::load", fPath, err);
            return false;
        }
//        tDecode = decodeTime.elapsed() ;

        imFile.close();
    }

    // HEIC format
    // rgh remove heic
    else if (metadata->hasHeic.contains(ext)) {
//        qDebug() << CLASSFUNCTION << "hasHEIC" << fPath;
        ImageMetadata m = dm->imMetadata(fPath);
        #ifdef Q_OS_WIN
        Heic heic;

        // try to decode
        if (!heic.decodePrimaryImage(fPath, image)) {
            if (imFile.isOpen()) imFile.close();
            QString err = "Unable to decode.";
            G::error(CLASSFUNCTION, fPath, err);
            return false;
        }
        if (imFile.isOpen()) imFile.close();
//        qDebug() << CLASSFUNCTION << "HEIC image" << image.width() << image.height();
        #endif
    }

    // TIFF format
    else if (ext == "tif") {
        QString decoderUsed = "Winnow";
        decodeTime.start();
        // check for sampling format we cannot read
        int samplesPerPixel = dm->index(dmRow, G::samplesPerPixelColumn).data().toInt();
        if (samplesPerPixel > 3) {
            imFile.close();
            QString err = "Could not read tiff because " + QString::number(samplesPerPixel)
                    + " samplesPerPixel > 3. " + fPath + ". ";
            G::error("Pixmap::load", fPath, err);
            return false;
        }

        // use Winnow decoder
        ImageMetadata m = dm->imMetadata(fPath);
        Tiff tiff;

        /*
        decodeTime.restart();
        bool useWinnow = true;
        if (useWinnow) {
            tiff.decode(m, fPath, image);
            decoderUsed = "Winnow";
        }
        else {
            image.load(fPath);
            decoderUsed = "Qt";
        }
        qint64 ms = decodeTime.elapsed();
        double secs = (double)ms / 1000;
        int w = dm->index(dmRow, G::WidthColumn).data().toInt();
        int h = dm->index(dmRow, G::HeightColumn).data().toInt();
        double mp = double(w * h) / 1024 / 1024;
        double pxPerMs = mp / secs;
        qDebug().noquote()
                 << CLASSFUNCTION << fPath << "decoded full size using" << decoderUsed
                 << "ms =" << ms
                 << "MP =" << QString::number(mp, 'g', 2)
                 << "MP/sec ="
                 << QLocale(QLocale::English).toString((int)pxPerMs)
                 << G::tiffData
                    ;
                    //*/


//        /*
        if (!tiff.decode(m, fPath, image)) {
            imFile.close();
//            err += "Could not decode " + fPath + ". ";
            qDebug() << "Pixmap::load"
                     << "Could not decode using Winnow Tiff decoder.  "
                        "Trying Qt tiff library to decode" + fPath + ". ";

            // use Qt tiff library to decode
            decoderUsed = "Qt";
            if (!image.load(fPath)) {
                imFile.close();
                QString err = "Could not decode.";
                G::error(CLASSFUNCTION, fPath, err);
                return false;
            }
        }
        //*/

        imFile.close();
    }

    // All other formats
    else {
//        qDebug() << "Pixmap::load all other formats" << fPath;
        // try to decode
        if (!image.load(fPath)) {
            imFile.close();
            QString err = "Could not decode.";
            G::error(CLASSFUNCTION, fPath, err);
            return false;
        }
        imFile.close();
    }

    // Successfully loaded to a QImage

    // rotate if portrait image
    if (metadata->rotateFormats.contains(ext)) {
        QTransform trans;
        int orientation = dm->index(dmRow, G::OrientationColumn).data().toInt();
        int rotationDegrees = dm->index(dmRow, G::RotationDegreesColumn).data().toInt();
        int degrees;
        if (orientation) {
            switch(orientation) {
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
        }
        else if (rotationDegrees){
            trans.rotate(rotationDegrees);
            image = image.transformed(trans, Qt::SmoothTransformation);
        }
    }

    // color manage if available
//    ICCTime.start();
    if (G::colorManage && metadata->iccFormats.contains(ext)) {
        QByteArray ba = dm->index(dmRow, G::ICCBufColumn).data().toByteArray();
        ICC::transform(ba, image);
    }
//    tICC = ICCTime.elapsed();

//    tDecode = decodeTime.elapsed() ;

    // calc read/decode performance
    double mp = dm->index(dmRow, G::MegaPixelsColumn).data().toDouble();
//    qint64 msec = tDecode;
    qint64 msec = t.elapsed();
    int msecPerMp = static_cast<int>(msec / mp);
    emit setValue(dm->index(dmRow, G::LoadMsecPerMpColumn), msecPerMp, Qt::EditRole, Qt::AlignLeft);
//    dm->setData(dm->index(dmRow, G::LoadMsecPerMpColumn), msecPerMp, Qt::EditRole);
    /*
    qDebug() << CLASSFUNCTION
             << "Decode:" << tDecode << "ms"
             << "ICC:" << tICC << "ms"
             << "Total:" << msec << "ms"
                ;
    //*/

    return true;
}
