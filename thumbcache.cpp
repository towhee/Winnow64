#include "thumbcache.h"
#include <QDebug>

/* ThumbCache populates the thumbView icons (thumbs) in a separate thread.


*/

ThumbCache::ThumbCache(QObject *parent, ThumbView *thumbView,
                       Metadata *metadata) : QThread(parent)
{
    {
    #ifdef ISDEBUG
    qDebug() << "ThumbCache::ThumbCache";
    #endif
    }
    this->thumbView = thumbView;
    this->metadata = metadata;
    restart = false;
    abort = false;
}

ThumbCache::~ThumbCache()
{
    mutex.lock();
    abort = true;
    condition.wakeOne();
    mutex.unlock();
    wait();
}

void ThumbCache::stopThumbCache()
{
    {
    #ifdef ISDEBUG
    qDebug() << "ThumbCache::stopThumbCache";
    #endif
    }
    if (isRunning()) {
        mutex.lock();
        abort = true;
        condition.wakeOne();
        mutex.unlock();
        wait();
        qDebug() << "ThumbCache::stopThumbCache  isRunning:" << isRunning();
        emit updateIsRunning(false);
    }
}

void ThumbCache::track(QString fPath, QString msg)
{
    qDebug() << "•• Thumb Caching  " << fPath << msg;
}

void ThumbCache::loadThumbCache()
{
    {
    #ifdef ISDEBUG
    qDebug() << "ThumbCache::loadThumbCache";
    #endif
    }
    if (!isRunning()) {
        abort = false;
        start(HighestPriority);
    } else {
        mutex.lock();
        abort = true;
        condition.wakeOne();
        mutex.unlock();
        wait();
        abort = false;
        start(HighestPriority);
    }
}

void ThumbCache::run()
{
    {
    #ifdef ISDEBUG
    qDebug() << "ThumbCache::run";
    #endif
    }
    emit updateIsRunning(true);

    QImageReader thumbReader;
    // rgh review hard coding thumb size
    QSize thumbMax(160, 160);
    QString fPath;
    QString folderPath;

    int totDelay = 500;     // milliseconds
    int msDelay = 0;        // total incremented delay
    int msInc = 1;          // amount to increment each try
    QString err;            // type of error

    // req'd to prevent warning when using item->setData - don't know why
    qRegisterMetaType<QVector<int> >("QVector");
    QStandardItem *item = new QStandardItem;

    for (int row = 0; row < thumbView->thumbViewModel->rowCount(); ++row) {
        if (abort) {
            emit updateIsRunning(false);
            return;
        }
        QModelIndex idx = thumbView->thumbViewModel->index(row, 0, QModelIndex());
        if (!idx.isValid()) return;
        item = thumbView->thumbViewModel->itemFromIndex(idx);
        fPath = item->data(Qt::ToolTipRole).toString();
        if (G::isThreadTrackingOn) track(fPath, "Reading");
        QImage thumb;
        QFileInfo fileInfo(fPath);
        folderPath = fileInfo.path();
        QString ext = fileInfo.completeSuffix().toLower();
        QFile imFile(fPath);

        /* Reading the thumb directly from the image file is faster than using
        QImageReader (thumbReader) to read the entire jpg and then scaling it
        down. However, not all jpgs have embedded thumbs so make a quick check.
        */
        ulong offsetThumb = metadata->getOffsetThumbJPG(fPath); bool
        readThumbFromJPG = (offsetThumb > 0 && ext == "jpg");

        bool success = false;
        if (metadata->rawFormats.contains(ext) || readThumbFromJPG) {
            /* read the raw file thumb using metadata for offset and length of
            embedded JPG.  Check if metadata has been cached for this image.
            */
            int msDelay = 0;
            do {
                if (abort) {
                    emit updateIsRunning(false);
                    return;
                }
                // confirm metadata has been read already
//                qDebug() << "ThumbCache::run - check isLoaded" << msDelay << fPath;
                if (metadata->isLoaded(fPath)) {
                    if (offsetThumb) {
                        if (imFile.open(QIODevice::ReadOnly)) {
                            if (imFile.seek(offsetThumb)) {
                                QByteArray buf =
                                    imFile.read(metadata->getLengthThumbJPG(fPath));
                                if (thumb.loadFromData(buf, "JPEG")) {
                                    imFile.close();
                                    if (thumb.isNull() && G::isThreadTrackingOn )
                                        track(fPath, "Empty thumb");
//                                      if (G::isThreadTrackingOn) qDebug() << fPath << "Scaling:" << thumb.size();

                                    thumb.scaled(thumbMax, Qt::KeepAspectRatio);
                                    success = true;
                                }
                                else {
                                    err = "Could not read thumb from buffer";
                                    if (G::isThreadTrackingOn) track(fPath, err);
                                    break;
                                }
                            }
                            else {
                                err = "Illegal offset to thumb";
                                if (G::isThreadTrackingOn) track(fPath, err);
                                break;
                            }
                        }
                        else {
                            // file busy, wait a bit and try again
                            err = "Could not open file for thumb - try again";
                            if (G::isThreadTrackingOn) track(fPath, err);
                        }
                    }
                    else {
                        err = "No offset for thumb";
                        if (G::isThreadTrackingOn) track(fPath, err);
                    }
                }
                else {
                    err = "Metadata has not been loaded yet - try again";
                    if (G::isThreadTrackingOn) track(fPath, err);
                }
                QThread::msleep(msInc);
                msDelay += msInc;
            }
            while (msDelay < totDelay && !success);
        }
        else  {
            // read the image file (supported by Qt), scaling to thumbnail size
            /*
            if (GData::isTimer) qDebug()
                     << "Time to read thumb size embedded JPG using QThumbReader for"
                     << imageFileName << t.elapsed()
                     << " milliseconds";    */

            do {
                // check if file is locked by another process
                if (imFile.open(QIODevice::ReadOnly)) {
                    // close file to allow qt thumbReader to work
                    imFile.close();
                    // let thumbReader do its thing
                    thumbReader.setFileName(fPath);
                    QSize size = thumbReader.size();
                    size.scale(thumbMax, Qt::KeepAspectRatio);
                    thumbReader.setScaledSize(size);
                    thumb = thumbReader.read();
                    success = !thumb.isNull();
                    if (!success) {
                        err = "Could not read thumb using thumbReader";
                        if (G::isThreadTrackingOn) track(fPath, err);
                        QThread::msleep(msInc);
                    }
                }
                else {
                    err = "Could not open file for image";    // try again
                    if (G::isThreadTrackingOn) track(fPath, err);
                    QThread::msleep(msInc);
                    msDelay += msInc;
                }
            }
            while (msDelay < totDelay && !success);
        }

        // check orientation and rotate if portrait
        QTransform trans;
        int orientation = metadata->getImageOrientation(fPath);
        switch(orientation) {
            case 6:
                trans.rotate(90);
                thumb = thumb.transformed(trans, Qt::SmoothTransformation);
                break;
            case 8:
                trans.rotate(270);
                thumb = thumb.transformed(trans, Qt::SmoothTransformation);
                break;
        }
        // record any errors
//        qDebug() << "thumbcache::run  abort" << abort << " emit setIcon(item, thumb)" << folderPath;
        mutex.lock();
        if (!abort) emit setIcon(item, thumb, folderPath);

        // causes crash when change folders and thumbcache was interrupted
//        if (!abort && item->index().isValid()) item->setIcon(QPixmap::fromImage(thumb));

//        item->setData(true, thumbView->LoadedRole);
        if (!success) {
            metadata->setErr(fPath, err);
            if (G::isThreadTrackingOn) track(fPath, err);
            if (G::isThreadTrackingOn) track(fPath, "FAILED TO LOAD THUMB");
        }
        else {
            if (G::isThreadTrackingOn) track(fPath, "Success:");
        }

        // sometimes QListView does not refresh icons...
        if (row % 20 == 0) {
            refreshThumbs();
        }

        mutex.unlock();
        emit updateStatus(false, "Caching thumb " + QString::number(row + 1) +
             " of " + QString::number(thumbView->thumbViewModel->rowCount()));
        restart = false;
//        qDebug() << "Thumbnail cached " << fName;

//        if (abort) {
//            emit updateIsRunning(false);
//            return;
//        }
    }
    emit updateIsRunning(false);
    emit updateStatus(true, "All thumbs cached");
    thumbView->refreshThumbs();
}
