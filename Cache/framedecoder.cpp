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
    video frame came from. When processing many files, it is not guaranteed that the
    signal/slots will be sequential. FrameDecoder maintains a queue of videos to process.

    After mediaPlayer->stop() there may be a number of QVideoSink frameChanged signals in
    the event queue. It is necessary to execute a qApp->processEvents to clear these
    excess signals.

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
            - frameDecoder->frameChanged calls frameDecoder->getNextThumbNail
       - loop until queue is empty
*/

FrameDecoder::FrameDecoder(QObject *parent)
{
    if (G::isLogger) G::log("FrameDecoder::FrameDecoder");
    status = Status::Idle;
    mediaPlayer = new QMediaPlayer();
    videoSink = new QVideoSink;
    mediaPlayer->setVideoOutput(videoSink);
    connect(videoSink, &QVideoSink::videoFrameChanged, this, &FrameDecoder::frameChanged);
    connect(mediaPlayer, &QMediaPlayer::errorOccurred, this, &FrameDecoder::errorOccurred);
    abort = false;  // version 1.33
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

    status = Status::Busy;
    if (isDebugging)
    {
        qDebug() << "FrameDecoder::getNextThumbNail           "
                 << "row =" << queue.at(0).dmIdx.row()
                 << "queue size =" << queue.size()
                 << "status =" << status
                 << "  " << queue.at(0).fPath
                 << "  src =" << src
            ;
    }
    mediaPlayer->setSource(queue.at(0).fPath);
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
    qint64 duration = 0;

    // wait for first valid frame
    if (im.isNull() || !frame.isValid() || queue.empty()) {
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
                 << "row =" << queue.at(0).dmIdx.row()
                 << "attempts =" << attempts
                 << "validFrame =" << validFrame
                 << "queue size =" << queue.size()
            ;
    }

    if (validFrame) {
        emit setFrameIcon(queue.at(0).dmIdx, pm, queue.at(0).dmInstance, duration, thisFrameDecoder);
    }

    if (validFrame || attempts > 10) {
        mediaPlayer->stop();
        attempts = 0;
        if (!queue.isEmpty()) queue.remove(0);
        // exhaust QVideoSink frameChanged signals after mediaPlayer is stopped
        qApp->processEvents(QEventLoop::AllEvents);
        getNextThumbNail("frameChanged");
    }
}

void FrameDecoder::errorOccurred(QMediaPlayer::Error, const QString &errorString)
{
    qWarning() << "WARNING" << "FrameDecoder::errorOccurred"
               << "row =" << queue.at(0).dmIdx.row() << errorString;
}
