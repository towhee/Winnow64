#include "Image/pixmap.h"
#include "Main/global.h"

Pixmap::Pixmap(QObject *parent, Metadata *metadata) : QObject(parent)
{
    this->metadata = metadata;
}

bool Pixmap::load(QString &fPath, QPixmap &pm)
{
    QImage image;
    bool success = load(fPath, image);
    pm = QPixmap::fromImage(image);
    return success;
}

bool Pixmap::load(QString &fPath, QImage &image)
{
/*  Reads the embedded jpg (known offset and length) and converts it into a
    QImage.

    This function is dependent on metadata being updated first.  Metadata is
    updated by the mdCache thread that runs every time a new folder is
    selected. This function is used in the imageCache thread that stores
    pixmaps in the heap.

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
    will be updated to show file not readable.
*/
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }

    bool success = false;
    int totDelay = 500;     // milliseconds
    int msDelay = 0;        // total incremented delay
    int msInc = 10;         // amount to increment each try

    QString err;            // type of error

    ulong offsetFullJpg = 0;
    ulong lengthFullJpg = 0;
    QFileInfo fileInfo(fPath);
    QString ext = fileInfo.completeSuffix().toLower();
    QFile imFile(fPath);

    if (metadata->rawFormats.contains(ext)) {
        // raw files handled by Qt
        do {
            // Check if metadata has been cached for this image
            offsetFullJpg = metadata->getOffsetFullJPG(fPath);
            lengthFullJpg = metadata->getLengthFullJPG(fPath);
            if (offsetFullJpg == 0) {
                metadata->loadImageMetadata(fPath, true, false);
                //try again
                offsetFullJpg = metadata->getOffsetFullJPG(fPath);
                lengthFullJpg = metadata->getLengthFullJPG(fPath);
            }
            // try to read the file
            if (offsetFullJpg > 0 && lengthFullJpg > 0) {
                if (imFile.open(QIODevice::ReadOnly)) {
                    bool seekSuccess = imFile.seek(offsetFullJpg);
                    if (seekSuccess) {
                        QByteArray buf = imFile.read(lengthFullJpg);
                        if (image.loadFromData(buf, "JPEG")) {
                            imFile.close();
                            success = true;
                        }
                        else {
                            err = "Could not read image from buffer";
                            break;
                        }
                    }
                    else {
                        err = "Illegal offset to image";
                        break;
                    }
                }
                else {
                    err = "Could not open file for image";    // try again
                    QThread::msleep(msInc);
                    msDelay += msInc;
                }
            }
            err = "Illegal offset to image or no length available";
            break;
            /*
            qDebug() << G::t.restart() << "\t" << "Pixmap::loadPixmap Success =" << success
                     << "msDelay =" << msDelay
                     << "offsetFullJpg =" << offsetFullJpg
                     << "Attempting to load " << imageFullPath;
                     */
        }
        while (msDelay < totDelay && !success);
    }
    else {
        // cooked files like tif, png etc
        do {
            // check if file is locked by another process
            if (imFile.open(QIODevice::ReadOnly)) {
                // close it to allow qt load to work
                imFile.close();
                // directly load the image using qt library
                success = image.load(fPath);
                if (!success) {
                    err = "Could not read image";
                    break;
                }
            }
            else {
                err = "Could not open file for image";    // try again
                QThread::msleep(msInc);
                msDelay += msInc;
            }
              /*
              qDebug() << G::t.restart() << "\t" << "Pixmap::loadPixmap Success =" << success
                       << "msDelay =" << msDelay
                       << "offsetFullJpg =" << offsetFullJpg
                       << "Attempting to load " << imageFullPath;
                       */
        }
        while (msDelay < totDelay && !success);
    }

    // rotate if portrait image
    QTransform trans;
    int orientation = metadata->getOrientation(fPath);
    int rotationDegrees = metadata->getRotation(fPath);
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

    // record any errors
    if (!success) metadata->setErr(fPath, err);

    #ifdef ISDEBUG
    G::track(__FUNCTION__, "Completed load image");
    #endif

    return success;
}
