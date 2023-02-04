#include "framedecoder.h"

/*
    Generates a thumbnail from the first video frame in a video file.

    This is accomplished by creating a QMediaPlayer that decodes and plays the video file
    to the virtual canvas QVideoSink. Each time the video frame changes QVideoSink emits
    a videoFrameChanged signal. FrameDecoder::frameChanged receives the signal, converts
    the frame into a QImage, emits setFrameIcon and stops the QMediaPlayer. The
    setFrameIcon signal is received by DataModel::setIconFromFrame, where the icon is
    added to the datamodel.

    This convoluted process is required because QVideoSink does not know which file the
    video frame came from.  When processing many files, it is not guaranteed that the
    signal/slots will be sequential.  FrameDecoder maintains a queue (queueIndex) of
    videos to process.

    Summary of sequence:
        - thumb->loadFromVideo signals to frameDecoder->addToQueue
        - frameDecoder->addToQueue calls frameDecoder->getNextThumbNail
        - start processing loop
            - frameDecoder->getNextThumbNail sets media source to first in queue
              and starts mediaPlayer
            - mediaPlayer signals frameDecoder->frameChanged
            - frameDecoder->frameChanged signals dm->setIconFromVideoFrame
            - frameDecoder->frameChanged calls frameDecoder->getNextThumbNail
        - loop until queue is empty

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
    connect(mediaPlayer, &QMediaPlayer::playbackStateChanged, this, &FrameDecoder::stateChanged);

    abort = false;
    isDebugging = true;
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

void FrameDecoder::stateChanged(QMediaPlayer::PlaybackState state)
{
    if (isDebugging)
        qDebug()  << "FrameDecoder::stateChanged            "
                  << "row =" << dmIdx.row()
                  << "state =" << state
                  << "  " << fPath
                  ;
    if (state == QMediaPlayer::StoppedState) {
        int i = queueIndex(dmIdx);
        if (i != -1) queue.remove(i);
        frameCaptured = false;
        getNextThumbNail("frameChanged");
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

    // aborting or finished queue
    if (queue.isEmpty() || abort) {
        if (isDebugging) qDebug() << "FrameDecoder::getNextThumbNail quiting"
                                  << "queue.isEmpty() =" << queue.isEmpty()
                                  << "  abort =" << abort
                                     ;
        status = Status::Idle;
        return;
    }

    // if reset, clear the queue request received while decoding a frame
    if (reset) {
        queue.clear();
        reset = false;
        return;
    }

    // start media player to process a frame
    status = Status::Busy;

    Item item = queue.first();
    fPath = item.fPath;
    dmIdx = item.dmIdx;
    frameChangedCount = 0;
//    frameCaptured = false;
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

    frameChangedCount++;
//    /*
    if (isDebugging)
        qDebug() << "FrameDecoder::frameChanged Enter      "
                 << "row =" << dmIdx.row()
                 << "count =" << frameChangedCount
                 << "frameCaptured =" << frameCaptured
                 << fPath
                    ;
    //*/

    // when a frame has beem captured, stop media player and process next queue item
//    if (frameCaptured || frameChangedCount > 10) {
//        if (isDebugging)
//            qDebug() << "FrameDecoder::frameChanged Done       "
//                     << "row =" << dmIdx.row()
//                     << "count =" << frameChangedCount
//                     << "frameCaptured =" << frameCaptured
//                     << "COMPLETED, get next"
//                        ;
//        mediaPlayer->stop();
////        int i = queueIndex(dmIdx);
////        if (i != -1) queue.remove(i);
////        frameCaptured = false;
////        getNextThumbNail("frameChanged");
//    }

    // Check the frame is valid
    QImage im = frame.toImage();
    if (im.isNull()) {
            qDebug() << "FrameDecoder::frameChanged  Null frame"
                   << "row =" << dmIdx.row()
                   << "count =" << frameChangedCount
                   << "frameCaptured =" << frameCaptured
                   << fPath;
    }
    else if (!frameCaptured) {
        // frame valid, save icon
        QPixmap pm = QPixmap::fromImage(im.scaled(G::maxIconSize, G::maxIconSize, Qt::KeepAspectRatio));
        qint64 duration = mediaPlayer->duration();
        emit setFrameIcon(dmIdx, pm, dmInstance, duration, thisFrameDecoder);
        frameCaptured = true;
        if (isDebugging)
            qDebug() << "FrameDecoder::frameChanged Captured   "
                     << "row =" << dmIdx.row()
                     << "count =" << frameChangedCount
                     << "frameCaptured =" << frameCaptured
                     << fPath
                        ;
        mediaPlayer->stop();
    }
}

void FrameDecoder::errorOccurred(QMediaPlayer::Error error, const QString &errorString)
{
    qWarning() << "WARNING" << "FrameDecoder::errorOccurred" << "row =" << dmIdx.row() << errorString;
}
