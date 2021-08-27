#include "Image/cacheimage.h"
#include "Main/global.h"

/*
   CacheImage is used by ImageCache to read image files and then decode them in a separate
   ImageDecoder thread.  This decouples the file read from the image decoding, as the files
   have to be read asyncronously, while they can be decoded synchronously in multiple
   ImageDecoder threads.

   Pixmap does the same thing, but combines the reading and decoding.


*/

CacheImage::CacheImage(QObject *parent,
                       DataModel *dm,
                       Metadata *metadata)
    : QObject(parent)
{
    this->dm = dm;
    this->metadata = metadata;
}

bool CacheImage::loadFromHeic(QString &fPath, QImage &image)
{
    if (G::isLogger) G::log(__FUNCTION__);
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
     return heic.decodePrimaryImage(m, fPath, image);
     #endif
}

bool CacheImage::load(QString &fPath, ImageDecoder *decoder)
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
    QMutexLocker locker(&mutex);
    if (G::isLogger) G::log(__FUNCTION__, fPath);
//    qDebug() << __FUNCTION__ << "fPath =" << fPath;
    QFileInfo fileInfo(fPath);
    QString ext = fileInfo.completeSuffix().toLower();

    if (metadata->videoFormats.contains(ext)) return false;

    QFile imFile(fPath);
    int dmRow = dm->fPathRow[fPath];

    // is metadata loaded
    if (!dm->index(dmRow, G::MetadataLoadedColumn).data().toBool()) {
        if (!dm->readMetadataForItem(dmRow)) {
            G::error(__FUNCTION__, fPath, "Could not load metadata.");
            return false;
        }
    }

    // is file already open by another process
    if (imFile.isOpen()) {
        G::error(__FUNCTION__, fPath, "File already open.");
        return false;
    }

    // try to open image file
    if (!imFile.open(QIODevice::ReadOnly)) {
        imFile.close();
        G::error(__FUNCTION__, fPath, "Could not open file for image.");
        return false;
    }

    // JPG format (including embedded in raw files)
    if (metadata->hasJpg.contains(ext) || ext == "jpg") {
        // get offset and length of embedded Jpg from the datamodel
        uint offsetFullJpg = dm->index(dmRow, G::OffsetFullColumn).data().toUInt();
        uint lengthFullJpg = dm->index(dmRow, G::LengthFullColumn).data().toUInt();

        // make sure legal offset by checking the length
        if (lengthFullJpg == 0) {
            imFile.close();
            G::error(__FUNCTION__, fPath, "Jpg length = zero.");
            return false;
        }

        // try to read the data
        if (!imFile.seek(offsetFullJpg)) {
            imFile.close();
            G::error(__FUNCTION__, fPath, "Illegal offset to image.");
            return false;
        }

        QByteArray buf = imFile.read(lengthFullJpg);
        if (buf == nullptr) {
            qWarning() << __FUNCTION__ << "nullptr for buf";
        }

        // try to decode the jpg data
        ImageMetadata m = dm->imMetadata(fPath);
        /*
        qDebug() << __FUNCTION__
                 << "JPG:"
                 << "Id =" << decoder->threadId
                 << "decoder->fPath =" << decoder->fPath
                    ;
                    //*/
        decoder->decode(G::Jpg, fPath, m, buf);
        imFile.close();
    }

    // HEIC format
    // rgh remove heic (why?)
    else if (metadata->hasHeic.contains(ext)) {
        ImageMetadata m = dm->imMetadata(fPath);
        #ifdef Q_OS_WIN
        decoder->decode(G::Heic, fPath, m);
        /*
        Heic heic;

        // try to decode
        if (!heic.decodePrimaryImage(m, fPath, image)) {
            if (imFile.isOpen()) imFile.close();
            err += "Unable to decode " + fPath + ". ";
            qDebug() << __FUNCTION__ << err;
            return false;
        }
        if (imFile.isOpen()) imFile.close();
//        qDebug() << __FUNCTION__ << "HEIC image" << image.width() << image.height();
        */
        #endif
    }

    // TIFF format
    else if (ext == "tif") {
        // check for sampling format we cannot read
        int samplesPerPixel = dm->index(dmRow, G::samplesPerPixelColumn).data().toInt();
        if (samplesPerPixel > 3) {
            imFile.close();
            QString err = "Could not read tiff because " + QString::number(samplesPerPixel)
                    + " samplesPerPixel > 3.";
            G::error(__FUNCTION__, fPath, err);
            return false;
        }

        // use Winnow decoder
        ImageMetadata m = dm->imMetadata(fPath);
        decoder->decode(G::Tif, fPath, m);

        imFile.close();
    }

    // All other formats
    else {
        // try to decode
        ImageMetadata m;
        qDebug() << __FUNCTION__
                 << "USEQT: "
                 << "Id =" << decoder->threadId
                 << "decoder->fPath =" << decoder->fPath
                    ;
        decoder->decode(G::UseQt, fPath, m);
        imFile.close();
    }
    return true;
}
