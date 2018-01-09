#include "thumbcache.h"
#include <QDebug>

/*
ThumbCache populates the thumbView icons (thumbs) in a separate thread.

Create a QMap structure tCache to hold
    - fPath (Index)
    - offsetThumbJPG
    - lengthThumbJPG
    - orientation
    - isLoaded (metadata)
    - isThumbLoaded = false

Create a bool isCacheFullyLoaded = false
Need totalImageCount from somewhere

Populate tCache from metadata (may not be complete as metadataCache could still
be running.  If fully loaded then isCacheFullyLoaded = true.

Start cycling through tCache adding each thumbnail (icon) to the datamodel.  For
each image update isThumbLoaded.  If successful then set isThumbLoaded = true.

Check if tCache.count < totalImageCount or any isThumbLoaded = false

if tCache.count < totalImageCount then update tCache

if all okay then return and end thumbCache

Recycle through tCache.



*/

ThumbCache::ThumbCache(QObject *parent, DataModel *dm,
                       Metadata *metadata) : QThread(parent)
{
    {
    #ifdef ISDEBUG
    qDebug() << "ThumbCache::ThumbCache";
    #endif
    }
    this->dm = dm;
    this->metadata = metadata;
//    restart = false;
    abort = false;
    thumbMax.setWidth(160);       // rgh review hard coding thumb size
    thumbMax.setHeight(160);      // rgh review hard coding thumb size
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
//        qDebug() << "ThumbCache::stopThumbCache  isRunning:" << isRunning();
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
        qDebug() << "ThumbCache::loadThumbCache" << abort;
    }
}

void ThumbCache::checkOrientation()
{
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
}

bool ThumbCache::loadFromEntireFile(QFile imFile, QString fPath)
{
    bool success;
    QImageReader thumbReader;
    QSize thumbMax(160, 160);       // rgh review hard coding thumb size
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
//            QThread::msleep(msInc);
        }
    }
    return success;
}

bool ThumbCache::loadFromData(QFile imFile, QString fPath)
{
    bool success = false;
    QSize thumbMax(160, 160);       // rgh review hard coding thumb size

    ulong offsetThumb = metadata->getOffsetThumbJPG(fPath);
    ulong lengthThumb = metadata->getLengthThumbJPG(fPath);  // new

    if (imFile.open(QIODevice::ReadOnly)) {
        if (imFile.seek(offsetThumb)) {
            QByteArray buf = imFile.read(lengthThumb);
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
                emit updateLoadThumb(fPath, err);
//                break;
            }
        }
        else {
            err = "Illegal offset to thumb";
            if (G::isThreadTrackingOn) track(fPath, err);
            emit updateLoadThumb(fPath, err);
//            break;
        }
    }
    else {
        // file busy, wait a bit and try again
        err = "Could not open file for thumb - try again";
        if (G::isThreadTrackingOn) track(fPath, err);
        emit updateLoadThumb(fPath, err);
    }
    return success;
}

bool ThumbCache::loadThumb(int row)
{
//    QModelIndex idx = dm->index(row, 0, QModelIndex());
//    if (!idx.isValid()) return;
//    item = dm->itemFromIndex(idx);
//    fPath = item->data(Qt::ToolTipRole).toString();
//    if (G::isThreadTrackingOn) track(fPath, "Reading");
//    QFileInfo fileInfo(fPath);
//    folderPath = fileInfo.path();
//    QString ext = fileInfo.completeSuffix().toLower();
//    QFile imFile(fPath);

//    /* Reading the thumb directly from the image file is faster than using
//    QImageReader (thumbReader) to read the entire jpg and then scaling it
//    down. However, not all images have embedded thumbs so make a quick check.
//    */
//    bool isLoaded = metadata->isLoaded(fPath);

///*        qDebug() << "offsetThumb"  << offsetThumb
//             << "lengthThumb"  << lengthThumb
//             << fPath << isLoaded
//             << "row =" << row;  */

//    bool success = false;
//    bool readThumbFromJPG = (offsetThumb > 0); // && ext == "jpg");

//    if (metadata->rawFormats.contains(ext) || readThumbFromJPG) {
//        /* read the raw file thumb using metadata for offset and length of
//        embedded JPG.  Check if metadata has been cached for this image.
//        */
//        if (metadata->isLoaded(fPath)) {
//            success = loadFromData(imFile, fPath);
//        }
//        else {
//            err = "Metadata has not been loaded yet - try again";
//            if (G::isThreadTrackingOn) track(fPath, err);
//        }
//        if (!success) emit updateLoadThumb(fPath, err);
//    }
//    else  {
//        // read the image file (supported by Qt), scaling to thumbnail size
//        loadFromEntireFile(imFile, fPath);
//        // check if file is locked by another process
//        else {
//            err = "Could not open file for image";    // try again
//            if (G::isThreadTrackingOn) track(fPath, err);
//        }
//    }

//    if (!success) emit updateLoadThumb(fPath, err);
    return true;
}

void ThumbCache::run()
{
    {
    #ifdef ISDEBUG
    qDebug() << "ThumbCache::run";
    #endif
    }
    qDebug() << "ThumbCache::run   abort =" << abort;
    QElapsedTimer t;
    t.start();
    emit updateIsRunning(true);

    // req'd to prevent warning when using item->setData - don't know why
    qRegisterMetaType<QVector<int> >("QVector");
    QStandardItem *item = new QStandardItem;

    for (int row = 0; row < dm->rowCount(); ++row) {
        if (abort) {
            qDebug() << "ThumbCache::run aborting";
            emit updateIsRunning(false);
            return;
        }        
        loadThumb(row);
    }  // end for

//    if (success) emit updateLoadThumb(fPath, "Loaded");

    // check orientation and rotate if portrait
    checkOrientation();

    // update datamodel with thumb (icon)
//    mutex.lock();
//    if (!abort) emit setIcon(item, thumb, folderPath);
///*
//    // causes crash when change folders and thumbcache was interrupted
//    if (!abort && item->index().isValid()) item->setIcon(QPixmap::fromImage(thumb));
//*/
//    if (!success) {
//        metadata->setErr(fPath, err);
//        if (G::isThreadTrackingOn) track(fPath, err);
//        if (G::isThreadTrackingOn) track(fPath, "FAILED TO LOAD THUMB");
//    }
//    else {
//        if (G::isThreadTrackingOn) track(fPath, "Success:");
//    }

//    // sometimes QListView does not refresh icons...
//    if (row % 20 == 0) {
//        refreshThumbs();
//    }
//    mutex.unlock();

//    emit updateStatus(false, "Caching thumb " + QString::number(row + 1) +
//         " of " + QString::number(dm->rowCount()));
////        restart = false;
//    qDebug() << "ThumbCache::run   Completed"
//             << "Total elapsed time to cache thumbs =" << t.elapsed() << "ms";

//    emit updateIsRunning(false);
//    emit updateStatus(true, "All thumbs cached");
//    refreshThumbs();
}
