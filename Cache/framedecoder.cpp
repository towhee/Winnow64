#include "framedecoder.h"

/*
    Generate a thumbnail from the first video frame in a video file.

    This is accomplished by creating a QMediaPlayer that decodes and plays the video file
    to the virtual canvas QVideoSink. Each time the video frame changes QVideoSink emits
    a videoFrameChanged signal. FrameDecoder::frameChanged receives the signal, converts
    the frame into a QImage, emits setFrameIcon and stops the QMediaPlayer. The
    setFrameIcon signal is received by DataModel::setIconFromFrame, where the icon is
    added to the datamodel and then the instance of FrameDecoder is deallocated
    (deleted).

    This convoluted process is required because QVideoSink does not know which file the
    video frame came from.  When processing many files, it is not guaranteed that the
    signal/slots will be sequential.  To solve this a separate instance of FrameDecoder
    is created for each file.  When the thumbnail (icon) has been received by the datamodel
    the FrameDecoder instance is deleted.

    Summary of sequence:
        - thumb->loadFromVideo creates FrameDecoder instance frameDecoder
        - thumb->loadFromVideo calls frameDecoder->getFrame
        - frameDecoder->getFrame starts mediaPlayer
        - mediaPlayer signals frameDecoder->frameChanged
        - frameDecoder->frameChanged signals dm->setIconFromFrame
        - dm->setIconFromFrame deletes frameDecoder
*/

FrameDecoder::FrameDecoder(QObject *parent)
//FrameDecoder::FrameDecoder(QObject *parent, int id)  : QThread(parent)

{
//    if (G::isLogger) G::log("FrameDecoder::FrameDecoder");
    this->id = id;
    status = Status::Idle;
//    thisFrameDecoder = this;
    mediaPlayer = new QMediaPlayer();
    videoSink = new QVideoSink;
    mediaPlayer->setVideoOutput(videoSink);
    connect(videoSink, &QVideoSink::videoFrameChanged, this, &FrameDecoder::frameChanged);
    connect(mediaPlayer, &QMediaPlayer::errorOccurred, this, &FrameDecoder::errorOccurred);

    abort = false;
    isDebugging = false;
}

void FrameDecoder::stop()
{
    if (status == Status::Idle) {
        emit stopped("FrameDecoder");
        return;
    }
    abort = true;
}

bool FrameDecoder::isBusy()
{
    return status == Status::Busy;
}

void FrameDecoder::clear()
{
    queue.clear(); return;

    if (status == Status::Idle) {
        queue.clear();
    }
    else {
        reset = true;
    }
}

void FrameDecoder::addToQueue(QString path, QModelIndex dmIdx, int dmInstance)
{
//    abort = false;
    if (abort) return;
    Item item;
    item.fPath = path;
    item.dmIdx = dmIdx;
    item.dmInstance = dmInstance;
    queue.append(item);
    if (isDebugging) qDebug() << "FrameDecoder::addToQueue              "
                              << "row =" << dmIdx.row()
                              << "queue size =" << queue.size()
                              << "status =" << status
                              << "  " << item.fPath
                              ;
    if (status == Status::Idle) getNextThumbNail("addToQueue");
}

int FrameDecoder::queueIndex(QModelIndex dmIdx)
{
    for (int i = 0; i < queue.size(); i++) {
        if (queue.at(i).dmIdx == dmIdx) {
            return i;
        }
    }
    return -1;
}

void FrameDecoder::getNextThumbNail(QString src)
{
//    if (G::isLogger) G::log("FrameDecoder::getFrame");
    if (abort) {
        abort = false;
        emit stopped("FrameDecoder");
    }

    if (queue.isEmpty() || abort) {
        if (isDebugging) qDebug() << "FrameDecoder::getNextThumbNail quiting"
                                  << "queue.isEmpty() =" << queue.isEmpty()
                                  << "  abort =" << abort
                                     ;
        status = Status::Idle;
        return;
    }

    // clear the queue request received while decoding a frame
    if (reset) {
        queue.clear();
        reset = false;
        return;
    }

    status = Status::Busy;

    Item item = queue.first();
    fPath = item.fPath;
    dmIdx = item.dmIdx;
    dmInstance = item.dmInstance;

    if (isDebugging) qDebug() << "FrameDecoder::getNextThumbNail        "
                              << "row =" << dmIdx.row()
                              << "status =" << status
                              << "  " << fPath
                              << "  src =" << src
                              ;

    mediaPlayer->setSource(fPath);
    mediaPlayer->play();
}

void FrameDecoder::frameChanged(const QVideoFrame frame)
{
    if (G::isLogger) G::log("FrameDecoder::frameChanged");
    /*
    if (isDebugging) qDebug() << "FrameDecoder::frameChanged            "
                              << "row =" << dmIdx.row()
                              ;
    //*/
    mediaPlayer->stop();
    QImage im = frame.toImage();
    if (!im.isNull()) {
        QPixmap pm = QPixmap::fromImage(im.scaled(G::maxIconSize, G::maxIconSize, Qt::KeepAspectRatio));
        qint64 duration = mediaPlayer->duration();
        emit setFrameIcon(dmIdx, pm, dmInstance, duration, thisFrameDecoder);
//        mutex.lock();
        int i = queueIndex(dmIdx);
        if (i != -1) queue.remove(i);
//        mutex.unlock();
        /*
        qDebug() << "FrameDecoder::frameChanged            "
                 << "row =" << dmIdx.row()
                 ;
                 */
    }
    getNextThumbNail("frameChanged");
}

void FrameDecoder::errorOccurred(QMediaPlayer::Error error, const QString &errorString)
{
    qWarning() << "WARNING" << "FrameDecoder::errorOccurred" << "row =" << dmIdx.row() << errorString;
}
