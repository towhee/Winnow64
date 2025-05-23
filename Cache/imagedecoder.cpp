#include "imagedecoder.h"
#include "Main/global.h"

/*
   This class decouples the asyncronous image reading (in CacheImage) from the synchronous
   image decoding, which is executed in multiple ImageDecoder threads.

   CacheImage::decode receives the file information and metadata.  Depending on the type of
   image, either a QByteArray buffer is loaded (JPG and other Qt library formats), the QFile
   is mapped to memory (TIF) or the file path is used (HEIC).  Using a buffer or mapping the
   QFile improves performance.  ImageDecoder::run is started.

   The appropriate decoder: decodeJpg, decodeTif, decodeHeic or decodeUsingQt is called.  The
   resulting QImage rotation and color manage are checked.  Done is emitted, signalling
   ImageCache::fillCache, where the QImage is inserted into the Image cache, and if there are
   still more images to cache, CacheImage is called and the cycle continues.

   decode
   load

*/

ImageDecoder::ImageDecoder(int id,
                           DataModel *dm,
                           Metadata *metadata)
    : QObject(nullptr)
{
    if (isDebug) G::log("ImageDecoder::ImageDecoder", "Thread " + QString::number(id));

    // connect(&decoderThread, &QThread::started, this, &ImageDecoder::run);
    // connect(this, &ImageDecoder::done, &decoderThread, &QThread::quit);

    threadId = id;
    // status = Status::Ready;
    fPath = "";
    sfRow = -1;
    instance = 0;
    this->dm = dm;
    this->metadata = metadata;
    isDebug = false;
    isLog = false;
}

ImageDecoder::~ImageDecoder()
{
    stop();
}

void ImageDecoder::stop()
{
    QString fun = "ImageDecoder::stop";
    if (isDebug)
    {
        qDebug().noquote()
            << fun.leftJustified(50);
    }

    abort = true;
    if (decoderThread.isRunning()) {
        decoderThread.quit();
        decoderThread.wait();
    }
}

bool ImageDecoder::quit()
{
    abort = false;
    // status = Status::Abort;
    fPath = "";
    // QImage blank;
    // image = QImage();
    sfRow = -1;
    return false;
}

void ImageDecoder::abortProcessing()
{
    QString fun = "ImageDecoder::abortProcessing";
    if (G::isLogger) G::log(fun, "id = " + QString::number(threadId));
    if (isDebug)
    {
        qDebug().noquote()
        << fun.leftJustified(50);
    }
    abort = true;
}

bool ImageDecoder::isRunning() const
{
    return running.loadRelaxed();
}

void ImageDecoder::decode(int row, int instance)
{
    // range check
    if (row >= dm->sf->rowCount()) return;

    setBusy();
    abort = false;
    sfRow = row ;
    status = Status::Undefined;
    fPath = dm->sf->index(sfRow,0).data(G::PathRole).toString();
    this->instance = instance;
    image = QImage();
    errMsg = "";
    if (isLog || G::isLogger) G::log("ImageDecoder::decode", "sfRow = " + QString::number(sfRow));

    QString fun = "ImageDecoder::decode";
    if (isDebug)
    {
        qDebug().noquote()
            << fun.leftJustified(50)
            << "decoder" << QVariant(threadId).toString().leftJustified(3)
            << "row =" << QString::number(sfRow).leftJustified(4)
            << fPath;
    }

    // instance check
    if (instance != dm->instance) {
        status = Status::InstanceClash;
        errMsg = "Instance clash.  New folder selected, processing old folder.";
        G::issue("Comment", errMsg, "ImageDecoder::run", sfRow, fPath);
        setIdle();
        emit done(threadId);
        if (isDebug)
        {
            QString fun = "ImageDecoder::decode instance clash";
            qDebug().noquote()
                << fun.leftJustified(50)
                << "decoder" << QVariant(threadId).toString().leftJustified(3)
                << "sfRow =" << QString::number(sfRow).leftJustified(4)
                << "errMsg =" << errMsg
                << "decoder[id]->fPath =" << fPath
                ;
        }
        return;
    }

    QElapsedTimer t;
    t.start();

    // decode
    if (!abort && load()) {
        // if (isDebug) G::log("ImageDecoder::run (if load)", "Image width = " + QString::number(image.width()));
        if (isDebug)
        {
            QString fun = "ImageDecoder::decode loaded";
            qDebug().noquote()
                << fun.leftJustified(50)
                << "decoder" <<  QString::number(threadId).leftJustified(3)
                << "row =" <<  QString::number(sfRow).leftJustified(4)
                << "DecoderToUse:" << decodersText.at(decoderToUse)
                << "ms =" << t.elapsed()
                << fPath;
        }
        if (metadata->rotateFormats.contains(ext) && !abort) rotate();
        if (G::colorManage && !abort) colorManage();
        if (image.isNull()) status = Status::Failed;
    }
    else {
        // if (isDebug)
        {
            QString fun = "ImageDecoder::decode load failed";
            qDebug() << fun.left(50)
                     << "row =" << sfRow
                     << "status =" << statusText.at(status)
                     << "errMsg =" << errMsg
                ; //*/
        }
    }

    setIdle();
    if (!abort) emit done(threadId);
}

bool ImageDecoder::load()
{
/*
    Loads a full size preview into a QImage.  It is invoked from ImageCache::fillCache.
    NOTE: calls to metadata and dm to not appear to impact performance.
*/
    if (isLog || G::isLogger) G::log("ImageDecoder::load",
               "sfRow = " + QString::number(sfRow) + "  " + fPath);
    QString fun = "ImageDecoder::load";
    if (isDebug)
    {
        QString fun = "ImageDecoder::load";
        qDebug().noquote()
            << fun.leftJustified(50)
            << "decoder" <<  QString::number(threadId).leftJustified(3)
            << "row =" << QString::number(sfRow).leftJustified(4)
            ;
    }

    // blank fPath when caching is cycling, waiting to finish.
    if (fPath == "") {
        errMsg = "Blank file path.";
        G::issue("Warning", errMsg, "ImageDecoder::load", sfRow, fPath);
        status = Status::BlankFilePath;
        return false;
    }

    /* get image type (extension)
       can cause crash: QFileInfo fileInfo(fPath);  or
                        ext = fPath.section('.', -1).toLower(); */
    // ext = n.ext;
    ext = dm->sf->index(sfRow, G::TypeColumn).data().toString().toLower();
    // qDebug() << "ImageDecoder::load" << sfRow << ext << fPath;

    // do not cache video files
    if (metadata->videoFormats.contains(ext)) {
        status = Status::Video;
        return false;
    }

    QFile imFile(fPath);

    // is file already open by another process
    if (imFile.isOpen()) {
        errMsg = "File already open.";
        G::issue("Warning", errMsg, "ImageDecoder::load", sfRow, fPath);
        status = Status::FileOpen;
        return false;
    }

    // try to open image file
    if (!imFile.open(QIODevice::ReadOnly)) {
        errMsg = "Unable to open file.";
        G::issue("Warning", errMsg, "ImageDecoder::load", sfRow, fPath);
        status = Status::FileOpen;
        return false;
    }

    decoderToUse = QtImage;  // default unless overridden

    // Embedded jpg?
    bool isEmbeddedJpg = false;
    int offsetFull = dm->sf->index(sfRow, G::OffsetFullColumn).data().toInt();
    int lengthFull = dm->sf->index(sfRow, G::LengthFullColumn).data().toInt();
    // embedded image type but no offset
    if (metadata->hasJpg.contains(ext) && offsetFull == 0) {

    }
    // raw file or jpg
    if (metadata->hasJpg.contains(ext) || (ext == "jpg" && offsetFull)) {
        isEmbeddedJpg = true;
    }
    // heic saved as a jpg
    if (metadata->hasHeic.contains(ext) && lengthFull) {
        isEmbeddedJpg = true;
    }

    /**************************************************************************/
    // EMBEDDED JPG FORMAT (RAW FILES AND SOME HEIC)
    if (isEmbeddedJpg) {
        // make sure legal offset by checking the length
        if (lengthFull == 0) {
            errMsg = "Could not read embedded JPG because length = 0.";
            G::issue("Warning", errMsg, "ImageDecoder::load", sfRow, fPath);
            imFile.close();
            status = Status::Invalid;
            return false;
        }

        // can we read the file
        if (!imFile.seek(offsetFull)) {
            errMsg = "Could not read embedded JPG because offset is invalid.";
            G::issue("Warning", errMsg, "ImageDecoder::load", sfRow, fPath);
            imFile.close();
            status = Status::Invalid;
            return false;
        }

        // read the embedded image into buf
        QByteArray buf = imFile.read(lengthFull);
        if (buf.length() == 0) {
            errMsg = "Could not read embedded JPG because buffer length = 0.";
            G::issue("Warning", errMsg, "ImageDecoder::load", sfRow, fPath);
            imFile.close();
            status = Status::Invalid;
            return false;
        }

        // SELECT DECODER TO USE

        // only Qt image library available in windows code
        #ifdef Q_OS_WIN
        decoderToUse = QtImage;
        #endif

        #ifdef Q_OS_MAC
        decoderToUse = QtImage;            // QtImage or TurboJpg or Rory
        if (decoderToUse == TurboJpg) {
            JpegTurbo jpegTurbo;
            image = jpegTurbo.decode(buf);
            if (image.isNull()) {
                errMsg = "Could not read JPG because JpegTurbo::decode failed.";
                G::issue("Warning", errMsg, "ImageDecoder::load", sfRow, fPath);
                // try Qt decoder
                decoderToUse = QtImage;
            }
        }
        #endif

        if (decoderToUse == QtImage) {
            if (!image.loadFromData(buf, "JPEG")) {
                errMsg = "Could not read JPG because QImage::loadFromData failed.";
                G::issue("Warning", errMsg, "ImageDecoder::load", sfRow, fPath);
                imFile.close();
                status = Status::Invalid;
                return false;
            }
        }
    }

    /**************************************************************************/
    // HEIC format
    else if (metadata->hasHeic.contains(ext)) {
        #ifdef Q_OS_WIN
        Heic heic;
        if (!heic.decodePrimaryImage(fPath, image)) {
            QString errMsg = "heic.decodePrimaryImage failed.";
            G::issue("Error", errMsg, "ImageDecoder::load", sfRow, fPath);
            imFile.close();
            status = Status::Invalid;
            return false;
        }
        /*
        qDebug() << "ImageDecoder::load" << "HEIC image" << image.width() << image.height();
        //*/
        #endif

        #ifdef Q_OS_MAC
        imFile.close();
        if (!image.load(fPath)) {
            errMsg = "Could not read because QImage::load failed.";
            G::issue("Warning", errMsg, "ImageDecoder::load", sfRow, fPath);
            // imFile.close();
            status = Status::Invalid;
            return false;
        }
        else if (image.width() == 0) {
            errMsg = "Unable to read heic image";
            G::issue("Warning", errMsg, "ImageDecoder::load", sfRow, fPath);
            status = Status::Invalid;
            return false;
        }
        imFile.close();
        if (isDebug)
        {
        qDebug() << "ImageDecoder::load" << "HEIC image"
                 << image.width() << image.height()
                 << fPath
            ;
        }
        #endif
    }

    /**************************************************************************/
    // TIFF format
    else if (ext == "tif") {

        /* decoder options:
           QtImage      use QImage::load
           QtTiff       use QTiffHandler override - overall best results incl jpg compression
           LibTiff      use libtiff library directly
           Rory         use Rory decoder
        */

        decoderToUse = QtTiff;         // works for all
        // decoderToUse = Rory;           // works for Zerene, not jpg compression
        // decoderToUse = LibTiff;        // works for all
        // decoderToUse = QtImage;        // works for all except jpg compression

        #ifdef Q_OS_MAC
        if (decoderToUse == LibTiff) {
            class LibTiff libTiff;
            image = libTiff.readTiffToQImage(fPath);
            // is valid? Assign status = Status::Invalid
        }
        #endif

        if (decoderToUse == Rory) {
            // check for sampling format we cannot read
            int samplesPerPixel = dm->sf->index(sfRow, G::samplesPerPixelColumn).data().toInt();
            if (samplesPerPixel > 3) {
                 errMsg = "TIFF samplesPerPixel more than 3.";
                 G::issue("Warning", errMsg, "ImageDecoder::run", sfRow, fPath);
                 imFile.close();
                 status = Status::Invalid;
                 return false;
            }

            // try Winnow decoder
            Tiff tiff("ImageDecoder::load Id = " + QString::number(threadId));
            if (!tiff.decode(fPath, offsetFull, image)) {
                decoderToUse = QtImage;     // maybe change to QtTiff?
                /*
                qDebug() << "ImageDecoder::load "
                         << "Could not decode using Winnow Tiff decoder.  row =" << n.key <<
                            "Trying Qt tiff library to decode " + fPath + ". ";  //*/
            }
        }

        if (decoderToUse == QtImage) {
            // use Qt tiff decoder
            imFile.close();
            if (!image.load(fPath)) {
                errMsg = "Could not read because Qt tiff decoder failed.";
                G::issue("Error", errMsg, "ImageDecoder::load", sfRow, fPath);
                imFile.close();
                status = Status::Invalid;
                return false;
            }
        }

        if (decoderToUse == QtTiff) {
            // override QTiffHandler code
            Tiff tiff("ImageDecoder::load");
            // qDebug() << "ImageDecoder::load decoderToUse == QtTiff" << fPath;
            if (!tiff.read(fPath, &image)) {
                errMsg = "Could not read because QTiff decoder failed.";
                G::issue("Error", errMsg, "ImageDecoder::load", sfRow, fPath);
                imFile.close();
                status = Status::Invalid;
                return false;
            }
        }
    }

    /**************************************************************************/
    // JPEG format
    else if (ext == "jpg" || ext == "jpeg") {
        #ifdef Q_OS_WIN
        decoderToUse = QtImage;
        #endif
        #ifdef Q_OS_MAC
        decoderToUse = QtImage;            // QtImage or TurboJpg or Rory
        if (decoderToUse == TurboJpg) {
            JpegTurbo jpegTurbo;
            image = jpegTurbo.decode(fPath);
            if (image.isNull()) {
                errMsg = "Could not read because TurboJpg decoder failed.";
                G::issue("Warning", errMsg, "ImageDecoder::load", sfRow, fPath);
                // try using QImage
                decoderToUse = QtImage;
            }
        }
        #endif

        if (decoderToUse == Rory) {
            Jpeg jpeg;
            jpeg.decodeScan(imFile, image);
            if (image.isNull()) {
                errMsg = "Could not read because Rory decoder failed.";
                G::issue("Warning", errMsg, "ImageDecoder::load", sfRow, fPath);
                imFile.close();
                status = Status::Invalid;
                return false;
            }
        }

        if (decoderToUse == QtImage) {
            imFile.close();
            image.load(fPath);  // crash 2024-11-23 EXC_BAD_ACCESS (SIGSEGV)
            if (image.isNull()) {
                errMsg = "Could not read because Qt decoder failed.";
                G::issue("Warning", errMsg, "ImageDecoder::load", sfRow, fPath);
                // imFile.close();
                status = Status::Invalid;
                return false;
            }
        }
    }

    /**************************************************************************/
    // All other formats
    else {
        // try to decode
        imFile.close();
        if (!image.load(fPath)) {
            imFile.close();
            errMsg = "Could not read because QImage::load failed.";
            G::issue("Warning", errMsg, "ImageDecoder::load", sfRow, fPath);
            status = Status::Invalid;
            return false;
        }
    }

    // image loaded, check for null image
    if (image.width() == 0 || image.height() == 0) {
        status = Status::Invalid;
        return false;
    }

    imFile.close();
    status = Status::Success;
    return true;
}

void ImageDecoder::setIdle()
{
    idle = true;
}

void ImageDecoder::setBusy()
{
    idle = false;
}

bool ImageDecoder::isIdle()
{
    return idle;
}

bool ImageDecoder::isBusy()
{
    return !idle;
}

void ImageDecoder::rotate()
{
    if (G::isLogger) G::log("ImageDecoder::rotate", "sfRow = " + QString::number(sfRow));
    QTransform trans;
    int degrees = 0;
    int orientation = dm->sf->index(sfRow, G::OrientationColumn).data().toInt();
    int rotationDegrees = dm->sf->index(sfRow, G::RotationDegreesColumn).data().toInt();
    if (orientation > 0) {
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
    else if (rotationDegrees > 0){
        trans.rotate(rotationDegrees);
        image = image.transformed(trans, Qt::SmoothTransformation);
    }
    /* debug
    qDebug().noquote()
             << "ImageDecoder::rotate "
             << "n.orientation =" << n.orientation
             << "n.rotationDegrees =" << n.rotationDegrees
             << "degrees =" << QString::number(degrees).leftJustified(3, ' ')
             << "n.fPath =" << n.fPath
                ;//*/
}

void ImageDecoder::colorManage()
{
    if (isLog || G::isLogger) G::log("ImageDecoder::colorManage", "sfRow = " + QString::number(sfRow));
    if (isDebug) {
        // G::log("ImageDecoder::colorManage", "Thread " + QString::number(threadId));
    }
    if (metadata->iccFormats.contains(ext)) {
        // QMutexLocker locker(&mutex);
        QByteArray iccBuf = dm->sf->index(sfRow, G::ICCBufColumn).data().toByteArray();
        ICC::transform(iccBuf, image);  // crash when mash next
    }
}

// void ImageDecoder::run()
// {
//     if (isDebug)
//     {
//     QString fun = "ImageDecoder::run";
//     qDebug().noquote()
//         << fun.leftJustified(50)
//         << "decoder" << QVariant(threadId).toString().leftJustified(3)
//         << "row =" << QString::number(sfRow).leftJustified(4)
//         << "errMsg =" << errMsg
//         << "decoder[id]->fPath =" << fPath
//         ;
//     }

//     if (abort) {
//         if (isDebug)
//         {
//             QString fun = "ImageDecoder::run aborted";
//             qDebug().noquote()
//                 << fun.leftJustified(50)
//                 << "decoder" << QVariant(threadId).toString().leftJustified(3)
//                 << "sfRow =" << QString::number(sfRow).leftJustified(4)
//                 << "errMsg =" << errMsg
//                 << "decoder[id]->fPath =" << fPath
//                 ;
//         }
//         return;
//     }

//     // if (isLog) G::log("ImageDecoder::run", "Thread " + QString::number(threadId));

//     if (instance != dm->instance) {
//         status = Status::InstanceClash;
//         errMsg = "Instance clash.  New folder selected, processing old folder.";
//         G::issue("Comment", errMsg, "ImageDecoder::run", sfRow, fPath);
//         emit done(threadId);
//         if (isDebug)
//         {
//             QString fun = "ImageDecoder::run instance clash";
//             qDebug().noquote()
//                 << fun.leftJustified(50)
//                 << "decoder" << QVariant(threadId).toString().leftJustified(3)
//                 << "sfRow =" << QString::number(sfRow).leftJustified(4)
//                 << "errMsg =" << errMsg
//                 << "decoder[id]->fPath =" << fPath
//                 ;
//         }
//         return;
//     }

//     QElapsedTimer t;
//     t.start();

//     // if (abort) {
//     //     return;
//     // }

//     if (load()) {
//         // if (isDebug) G::log("ImageDecoder::run (if load)", "Image width = " + QString::number(image.width()));
//         if (isDebug)
//         {
//             QString fun = "ImageDecoder::run loaded";
//             qDebug().noquote()
//                 << fun.leftJustified(50)
//                 << "decoder" <<  QString::number(threadId).leftJustified(3)
//                 << "row =" <<  QString::number(sfRow).leftJustified(4)
//                 << "DecoderToUse:" << decodersText.at(decoderToUse)
//                 << "ms =" << t.elapsed()
//                 << fPath;
//         }
//         if (metadata->rotateFormats.contains(ext) && !abort) rotate();
//         if (G::colorManage && !abort) colorManage();
//         if (image.isNull()) status = Status::Failed;
//         else status = Status::Success;
//     }
//     else {
//         if (isDebug)
//         {
//             QString fun = "ImageDecoder::run load failed";
//             qDebug() << fun.left(50)
//                      << "row =" << sfRow
//                      << "status =" << statusText.at(status)
//                      << "errMsg =" << errMsg
//                         ; //*/
//         }
//     }

//     if (isDebug) {
//         // G::log("ImageDecoder::run", "Thread " + QString::number(threadId) + " done");
//     }

//     if (isDebug)
//     {
//         QString fun = "ImageDecoder::run done";
//         qDebug().noquote()
//                  << fun.leftJustified(50)
//                  << "decoder" <<  QString::number(threadId).leftJustified(3)
//                  << "row =" << QString::number(sfRow).leftJustified(4)
//                  << "status =" << statusText.at(status)
//                  << "w =" << image.width()
//                  << "h =" << image.height()
//                  << "decoder->fPath =" << fPath
//                     ; //*/
//     }
//     if (!abort) emit done(threadId);
// }

bool ImageDecoder::decodeIndependent(QImage &img, Metadata *metadata, ImageMetadata &m)
{
/*
    This function is called externally, does not require the DataModel and does not
    run in a separate thread.

    It is used by FindDuplicatesDlg.
*/
    this->metadata = metadata;
    fPath = m.fPath;
    sfRow = dm->proxyRowFromPath(fPath);

    if (load()) {
        //if (metadata->rotateFormats.contains(ext)) rotate();
        if (metadata->rotateFormats.contains(ext)) rotate();
        colorManage();
        img = image;
        return true;
    }
    qDebug() << "ImageDecoder::decode failed" << fPath;
    return false;
}
