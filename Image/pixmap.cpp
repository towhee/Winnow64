#include "Image/pixmap.h"
#include "Main/global.h"

Pixmap::Pixmap(QObject *parent, DataModel *dm, Metadata *metadata) : QObject(parent)
{
    this->dm = dm;
    this->metadata = metadata;
    #if (QT_VERSION >= QT_VERSION_CHECK(6, 0, 0))
    QImageReader::setAllocationLimit(1024);
    #endif
    connect(this, &Pixmap::setValDm, dm, &DataModel::setValDm);
}

bool Pixmap::loadFromHeic(QString &fPath, QImage &image)
{
    QString fun = "Pixmap::loadFromHeic";
    if (G::isLogger) G::log(fun, fPath);

    #ifdef Q_OS_WIN
    Heic heic;
    // try to read heic thumbnail, then fall back to the primary image
    if (heic.decodeThumbnail(fPath, image)) return true;
    if (heic.decodePrimaryImage(fPath, image)) return true;
    return false;
    #else
    // Heic natively supported on Mac via Qt's image plugin
    return image.load(fPath);
    #endif
}

bool Pixmap::load(QString &fPath, QPixmap &pm, QString src)
{
    QImage image;
    bool success = load(fPath, image, src);
    pm = QPixmap::fromImage(image);
    return success;
}

bool Pixmap::load(QString &fPath, QImage &image, QString src)
{
/*  Reads the embedded jpg (known offset and length) and converts it into a
    QImage.

    This function is dependent on metadata being updated first.  Metadata is
    updated by the metaRead thread that runs every time a new folder is
    selected. This function is signalled by the imageCache thread that stores
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
    QString fun = "Pixmap::load";
    if (G::isLogger) G::log(fun , fPath);
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
    int dmRow = dm->rowFromPath(fPath);

    // metadata read attempted but failed
    if (dm->index(dmRow, G::MetadataStatusColumn).data().toInt() == G::MetaFailed)
    {
        if (!dm->readMetadataForItem(dmRow, dm->instance)) {
            QString msg = "Could not load metadata.";
            G::issue("Error", msg, "Pixmap::load", dmRow, fPath);
            return false;
        }
    }

    // is file already open by another process
    if (imFile.isOpen()) {
        QString msg = "File already open.";
        G::issue("Error", msg, "Pixmap::load", dmRow, fPath);
        return false;
    }

    // try to open image file
    if (!imFile.open(QIODevice::ReadOnly)) {
        imFile.close();
        QString msg = "Could not open file.";
        G::issue("Error", msg, "Pixmap::load", dmRow, fPath);
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
            QString msg = "Jpg length = zero";
            G::issue("Error", msg, "Pixmap::load", dmRow, fPath);
            return false;
        }

        // try to read the data
        if (!imFile.seek(offsetFullJpg)) {
            imFile.close();
            QString msg = "Invalid jpeg offset " + QString::number(offsetFullJpg) + ".";
            G::issue("Error", msg, "Pixmap::load", dmRow, fPath);
            return false;
        }

        QByteArray buf = imFile.read(lengthFullJpg);

//        tRead = readTime.elapsed();
//        decodeTime.start();

//        qDebug() << "Pixmap::load" << "use decodeScan";
//        Jpeg jpg;
//        jpg.decodeScan(buf, image);

        // try to decode the jpg data
        if (!image.loadFromData(buf, "JPEG")) {
            imFile.close();
            QString msg = "Could not read jpeg from buffer.";
            G::issue("Error", msg, "Pixmap::load", dmRow, fPath);
            return false;
        }
//        tDecode = decodeTime.elapsed() ;

        imFile.close();
    }

    // HEIC format
    // rgh remove heic
    else if (metadata->hasHeic.contains(ext)) {
        //qDebug() << "Pixmap::load" << "hasHEIC" << fPath;
        ImageMetadata m = dm->imMetadata(fPath);
        #ifdef Q_OS_WIN
        Heic heic;

        // try to decode
        if (!heic.decodePrimaryImage(fPath, image)) {
            if (imFile.isOpen()) imFile.close();
            QString msg = "Unable to decode heic file.";
            G::issue("Error", msg, "Pixmap::load", dmRow, fPath);
            return false;
        }
        if (imFile.isOpen()) imFile.close();
        //qDebug() << "Pixmap::load" << "HEIC image" << image.width() << image.height();
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
            QString msg = "Could not read tiff, " + QString::number(samplesPerPixel)
                    + " samplesPerPixel > 3. " + fPath + ". ";
            G::issue("Error", msg, "Pixmap::load", dmRow, fPath);
            return false;
        }

        // use Winnow decoder
        ImageMetadata m = dm->imMetadata(fPath);
        Tiff tiff("Pixmap::load");

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
                 << "Pixmap::load" << fPath << "decoded full size using" << decoderUsed
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
            qDebug() << "Pixmap::load"
                     << "Could not decode using Winnow Tiff decoder.  "
                        "Trying Qt tiff library to decode" + fPath + ". ";
            QString msg = "Winnow Tiff decoder failed, trying Qt decoder.";
            G::issue("Warning", msg, "Pixmap::load", dmRow, fPath);

            // use Qt tiff library to decode
            decoderUsed = "Qt";
            if (!image.load(fPath)) {
                imFile.close();
                QString msg = "Could not decode tiff using Qt.";
                G::issue("Error", msg, "Pixmap::load", dmRow, fPath);
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
            QString msg = "Could not decode using default decoder.";
            G::issue("Error", msg, "Pixmap::load", dmRow, fPath);
            return false;
        }
        imFile.close();
    }

    // Successfully loaded to a QImage

    // rotate
    if (metadata->rotateFormats.contains(ext)) {
        int orientation = dm->index(dmRow, G::OrientationColumn).data().toInt();
        int rotationDegrees = dm->index(dmRow, G::RotationDegreesColumn).data().toInt();
        applyOrientation(image, orientation, rotationDegrees);
    }

    // color manage if available
    // ICCTime.start();
    if (G::colorManage && metadata->iccFormats.contains(ext)) {
        QByteArray ba = dm->index(dmRow, G::ICCBufColumn).data().toByteArray();
        ICC::transform(ba, image);
    }
    // tICC = ICCTime.elapsed();

    // tDecode = decodeTime.elapsed() ;

    // calc read/decode performance
    double mp = dm->index(dmRow, G::MegaPixelsColumn).data().toDouble();
    // qint64 msec = tDecode;
    qint64 msec = t.elapsed();
    int msecPerMp = static_cast<int>(msec / mp);
    QString source = "Pixmap::load";
    emit setValDm(dmRow, G::LoadMsecPerMpColumn, msecPerMp, 0, source,
                  Qt::EditRole, Qt::AlignLeft);
    /*
    qDebug() << "Pixmap::load"
             << "Decode:" << tDecode << "ms"
             << "ICC:" << tICC << "ms"
             << "Total:" << msec << "ms"
                ;
    //*/

    return true;
}

void Pixmap::applyOrientation(QImage &image, int orientation, int rotationDegrees)
{
/*
    Rotate image based on EXIF orientation plus any additional user rotation. Shared by
    Pixmap::load and Pixmap::loadIndependent.
*/
    QTransform trans;
    int degrees = 0;
    if (orientation) {
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
    }
    else if (rotationDegrees) {
        trans.rotate(rotationDegrees);
        image = image.transformed(trans, Qt::SmoothTransformation);
    }
}

bool Pixmap::loadFromJpgData(QString &fPath, QImage &image, uint offset, uint length)
{
/*
    Read an embedded JPG (known offset and length) and decode it into a QImage.
*/
    QString fun = "Pixmap::loadFromJpgData";
    if (G::isLogger) G::log(fun, fPath);

    QFile imFile(fPath);
    if (imFile.isOpen()) {
        G::issue("Warning", "File is already open.", fun, -1, fPath);
        return false;
    }
    if (imFile.open(QIODevice::ReadOnly)) {
        bool success = false;
        if (imFile.seek(offset)) {
            QByteArray buf = imFile.read(length);
            success = image.loadFromData(buf, "JPEG");
        }
        imFile.close();
        return success;
    }
    return false;
}

bool Pixmap::loadFromTiff(QString &fPath, QImage &image, ImageMetadata *m)
{
/*
    Decode a tiff: try sampling an embedded thumbnail, then the Winnow decoder, then
    fall back to the Qt tiff library.
*/
    QString fun = "Pixmap::loadFromTiff";
    if (G::isLogger) G::log(fun, fPath);

    QFile imFile(fPath);
    if (imFile.isOpen()) {
        G::issue("Warning", "File is already open.", fun, m->row, fPath);
        return false;
    }
    if (m->samplesPerPixel > 3) {
        QString msg = "Could not read tiff because " + QString::number(m->samplesPerPixel)
                      + " samplesPerPixel > 3.";
        G::issue("Error", msg, fun, m->row, fPath);
        return false;
    }

    Tiff tiff("Pixmap::loadFromTiff");
    if (tiff.read(fPath, &image)) return true;

    // // try to decode tiff thumbnail by sampling tiff raw data
    // bool isThumbOffset = m->offsetThumb > 0;
    // bool getThumb = true;
    // if (isThumbOffset && tiff.decode(*m, fPath, image, getThumb, G::maxIconSize)) return true;

    // // try the entire tif using the Winnow decoder
    // if (tiff.decode(fPath, m->offsetFull, image)) return true;

    // // use Qt tiff library to decode
    // if (image.load(fPath)) return true;

    return false;
}

bool Pixmap::loadFromEntireFile(QString &fPath, QImage &image)
{
/*
    Load and decode an entire image file using the Qt image library.
*/
    QString fun = "Pixmap::loadFromEntireFile";
    if (G::isLogger) G::log(fun, fPath);

    if (!image.load(fPath)) {
        G::issue("Warning", "Failed to load image.", fun, -1, fPath);
        return false;
    }
    if (image.height() == 0) {
        G::issue("Warning", "Image has width or height = 0.", fun, -1, fPath);
        return false;
    }
    return true;
}

bool Pixmap::loadIndependent(QString &fPath, QImage &image, int longSide, QString src,
                             bool colorManage)
{
/*
    Decode an image file that is not necessarily in the datamodel into a QImage, scaled so
    its long side == longSide (longSide == 0 means do not resize).

    Unlike Pixmap::load (which reads offsets/orientation from the datamodel), this loads the
    file's own metadata, so it works for arbitrary files such as the target images in
    FindDuplicatesDlg. Video files are handled by the caller via FrameDecoder. Absorbed from
    the former AutonomousImage class.
*/
    QString fun = "Pixmap::loadIndependent";
    if (G::isLogger) G::log(fun, fPath);

    QFileInfo fileInfo(fPath);

    // check permissions
    QFileDevice::Permissions oldPermissions = fileInfo.permissions();
    if (!(oldPermissions & QFileDevice::ReadUser)) {
        QFile(fPath).setPermissions(oldPermissions | QFileDevice::ReadUser);
    }

    QString ext = fileInfo.suffix().toLower();
    QSize thumbMax(longSide, longSide);

    // load this file's metadata (it may not be in the datamodel)
    metadata->loadImageMetadata(fileInfo, 0, G::dmInstance, true, true, false, true, fun);
    ImageMetadata *m = &metadata->m;

    uint offsetThumb = m->offsetThumb;
    uint lengthThumb = m->lengthThumb;
    bool isEmbeddedJpg = offsetThumb && lengthThumb;

    bool success = false;

    // raw image file or tiff with embedded jpg
    if (isEmbeddedJpg) {
        success = loadFromJpgData(fPath, image, offsetThumb, lengthThumb);
    }
    // the image type might not have metadata we can read, so load entire image
    else if (!metadata->hasMetadataFormats.contains(ext)) {
        success = loadFromEntireFile(fPath, image);
    }
    else if (ext == "heic") {
        success = loadFromHeic(fPath, image);
    }
    else if (ext == "tif") {
        success = loadFromTiff(fPath, image, m);
    }
    // all other image files
    else {
        success = loadFromEntireFile(fPath, image);
        image.convertTo(QImage::Format_RGB32);
    }

    if (success) {
        if (longSide) {
            image = image.scaled(thumbMax, Qt::KeepAspectRatio);
            image.convertTo(QImage::Format_RGB32);
        }
        // rotate if there is orientation metadata
        if (metadata->rotateFormats.contains(ext))
            applyOrientation(image, m->orientation, m->rotationDegrees);

        // optional colour management (opt-in and gated by the global switch)
        if (colorManage && G::colorManage && metadata->iccFormats.contains(ext)
            && !m->iccBuf.isEmpty()) {
            /* ICC::transform assumes 4 bytes/pixel (TYPE_BGRA_8); convert first so
               lcms2's packer does not walk past a narrower buffer. */
            if (image.format() != QImage::Format_ARGB32 &&
                image.format() != QImage::Format_RGB32) {
                image = image.convertToFormat(QImage::Format_ARGB32);
            }
            ICC::transform(m->iccBuf, image);
        }
    }
    else {
        // show bad image png
        QString badPath = ":/images/badImage1.png";
        loadFromEntireFile(badPath, image);
        G::issue("Error", "Could not load image.", fun, m->row, fPath);
    }

    QFile(fPath).setPermissions(oldPermissions);
    return success;
}
