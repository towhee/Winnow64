#include "thumbcache.h"
#include <QDebug>

ThumbCache::ThumbCache(QObject *parent, ThumbView *thumbView,
                       Metadata *metadata) : QThread(parent)
{
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
        start(LowPriority);
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
//    QSize thumbSpace(GData::thumbWidth, GData::thumbHeight);
    QString fPath;

    int totDelay = 500;     // milliseconds
    int msDelay = 0;        // total incremented delay
    int msInc = 50;         // amount to increment each try
    QString err;            // type of error

    for (int row=0; row < thumbView->thumbViewModel->rowCount(); ++row) {
        if (abort) {
            emit updateIsRunning(false);
            return;
        }
        QModelIndex idx = thumbView->thumbViewModel->index(row, 0, QModelIndex());
        QStandardItem *item = new QStandardItem;
        item = thumbView->thumbViewModel->itemFromIndex(idx);
        fPath = item->data(Qt::ToolTipRole).toString();
        if (G::isThreadTrackingOn) track(fPath, "Reading");
        QImage thumb;
        QFileInfo fileInfo(fPath);
        QString ext = fileInfo.completeSuffix().toLower();
        QFile imFile(fPath);

        // Reading the thumb directly from the image file is faster than
        // using thumbReader to read the entire jpg and then scaling it down.
        // However, not all jpgs have embedded thumbs so make a quick check
        ulong offsetThumb = metadata->getOffsetThumbJPG(fPath);
        bool readThumbFromJPG = (offsetThumb > 0 && ext == "jpg");

        bool success = false;
        if (metadata->rawFormats.contains(ext) || readThumbFromJPG) {
            // read the raw file thumb using metadata for offset and length of
            // embedded JPG
            // Check if metadata has been cached for this image
            int msDelay = 0;
            int msInc = 1;
            do {
                if (abort) {
                    emit updateIsRunning(false);
                    return;
                }
                // confirm metadata has been read already
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
                                    if (G::isThreadTrackingOn) qDebug() << fPath << "Scaling:" << thumb.size();

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
                        // file busy, wait a bit and try again
                        err = "Could not open file for thumb - try again";
                        if (G::isThreadTrackingOn) track(fPath, err);
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
        mutex.lock();

        /*following line generates comment in app output only first time in loop
        QObject::connect: Cannot queue arguments of type 'QVector<int>'
        (Make sure 'QVector<int>' is registered using qRegisterMetaType().)*/
        item->setIcon(QPixmap::fromImage(thumb));
        /*this doesn't work either
        QPixmap pm;
        typedef QVector<QVector<int> > pm;
        qRegisterMetaType<MyArray>("pm");
        pm.fromImage(thumb);
        item->setIcon(pm);*/

        item->setData(true, thumbView->LoadedRole);
        if (!success) {
            metadata->setErr(fPath, err);
            if (G::isThreadTrackingOn) track(fPath, err);
            if (G::isThreadTrackingOn) track(fPath, "FAILED TO LOAD THUMB");
        }
        else {
            if (G::isThreadTrackingOn) track(fPath, "Success:");
        }

        // sometimes QListView does not refresh icons...
//        if (row % 8 == 0) {
//            thumbView->refreshThumbs();
//            qApp->processEvents();
//        }

        mutex.unlock();
        emit updateStatus("", "Loaded thumb " + QString::number(row),
             " of " + QString::number(thumbView->thumbViewModel->rowCount()));
        restart = false;
//        qDebug() << "Thumbnail cached " << fName;
    }
    emit updateIsRunning(false);
    thumbView->refreshThumbs();
}
