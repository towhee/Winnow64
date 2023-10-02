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
            - frameDecoder->frameChanged converts video frame to QPixmap
            - frameDecoder->frameChanged signals dm->setIconFromVideoFrame
            - frameDecoder->frameChanged stops mediaPlayer
            - mediaPlayer playbackStateChanged signals frameDecoder->stateChanged
            - frameDecoder->stateChanged calls frameDecoder->getNextThumbNail
        - loop until queue is empty

*/

FrameDecoder::FrameDecoder(DataModel *dm, QObject *parent)

{
    if (G::isLogger) G::log("FrameDecoder::FrameDecoder");
    //this->dm = dm;
    status = Status::Idle;
    mediaPlayer = new QMediaPlayer();
    //videoSink = new VideoSink;
    videoSink = new QVideoSink;
    mediaPlayer->setVideoOutput(videoSink);
    connect(videoSink, &QVideoSink::videoFrameChanged, this, &FrameDecoder::frameChanged);
    connect(mediaPlayer, &QMediaPlayer::errorOccurred, this, &FrameDecoder::errorOccurred);
    //connect(mediaPlayer, &QMediaPlayer::playbackStateChanged, this, &FrameDecoder::stateChanged);
    abort = false;  // version 1.33
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

//bool FrameDecoder::isBusy()
//{
//    return status == Status::Busy;
//}

void FrameDecoder::clear()
{
    queue.clear();
    return;
}

void FrameDecoder::addToQueue(QString path, QModelIndex dmIdx, int dmInstance)
{
    if (abort) return;
    Item item;
    item.fPath = path;
    item.dmIdx = dmIdx;
    item.dmInstance = dmInstance;
    queue.append(item);
    if (isDebugging)
    {
        qDebug() << "FrameDecoder::addToQueue                 "
                 << "row =" << dmIdx.row()
                 << "queue size =" << queue.size()
                 << "status =" << status
                 << "  " << item.fPath
            ;
    }
    if (status == Status::Idle) getNextThumbNail("addToQueue");
}

void FrameDecoder::getNextThumbNail(QString src)
{
    if (G::isLogger) G::log("FrameDecoder::getFrame");
    if (abort) {
        abort = false;
        emit stopped("FrameDecoder");
    }

    if (queue.isEmpty()) {
        if (isDebugging)
        {
            qDebug() << "FrameDecoder::getNextThumbNail           "
                     << "queue.isEmpty() =" << queue.isEmpty()
                     << "queue size =" << queue.size()
                     << "  abort =" << abort
                ;
        }
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

    Item item = queue.at(0);
    fPath = item.fPath;
    dmIdx = item.dmIdx;
    dmInstance = item.dmInstance;

    if (isDebugging)
    {
        qDebug() << "FrameDecoder::getNextThumbNail           "
                 << "row =" << item.dmIdx.row()
                 << "queue size =" << queue.size()
                 << "status =" << status
                 << "  " << item.fPath
                 << "  src =" << src
            ;
    }

    mediaPlayer->setSource(item.fPath);
    //mediaPlayer->setVideoOutput(videoSink);
    mediaPlayer->play();
}

void FrameDecoder::frameChanged(const QVideoFrame frame)
{
    if (G::isLogger) G::log("FrameDecoder::frameChanged");
    bool validFrame = false;

    static int attempts = 0;

    if (mediaPlayer->playbackState() == QMediaPlayer::PlaybackState::StoppedState) return;

    QImage im = frame.toImage();
    QPixmap pm;
    qint64 duration;

    // wait for first valid frame
    if (im.isNull() || !frame.isValid()) {
        validFrame = false;
    }
    else {
        validFrame = true;
        pm = QPixmap::fromImage(im.scaled(G::maxIconSize, G::maxIconSize, Qt::KeepAspectRatio));
        duration = mediaPlayer->duration();
    }

    attempts++;

    if (isDebugging)
    {
        qDebug() << "FrameDecoder::frameChanged               "
                 << "row =" << dmIdx.row()
                 << "attempts =" << attempts
                 << "validFrame =" << validFrame
                 << "queue size =" << queue.size()
            ;
    }

    if (validFrame) {
        emit setFrameIcon(dmIdx, pm, dmInstance, duration, thisFrameDecoder);
    }

    if (validFrame || attempts > 10) {
        mediaPlayer->stop();
        attempts = 0;
        if (!queue.isEmpty()) queue.remove(0);
        // pause to exhaust QVideoSink frameChanged signals after mediaPlayer is stopped
        G::wait(1);
        getNextThumbNail("frameChanged");
    }
}

void FrameDecoder::errorOccurred(QMediaPlayer::Error, const QString &errorString)
{
    qWarning() << "WARNING" << "FrameDecoder::errorOccurred" << "row =" << dmIdx.row() << errorString;
}
