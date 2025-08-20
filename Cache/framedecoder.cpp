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

    ChatGPT was used to make code more robust.  Original code at bottom.
*/
/*
    Generates a thumbnail from the first video frame of a video file.

    For each video file, a new QMediaPlayer and QVideoSink instance is created to decode
    the video frame. QMediaPlayer is connected to a QVideoSink, which emits the
    videoFrameChanged signal when new video frames are available. The FrameDecoder
    captures this signal and processes the first valid frame into a QImage and then
    QPixmap (if needed), which is emitted to the datamodel via the setFrameIcon or
    frameImage signal.

    This one-player-per-video approach ensures decoding threads (used internally by
    FFmpeg) are safely isolated. It avoids reuse of QMediaPlayer instances, which can
    cause crashes if stopped or destroyed while FFmpeg threads are still active. This
    design is robust to rapid datamodel changes and frequent thumbnail requests.

    FrameDecoder maintains an internal queue of video files to process sequentially. As
    each thumbnail is generated (or skipped due to an error), the player and sink are
    cleaned up, and the next item in the queue is processed.

    Summary of workflow:
        - thumb->loadFromVideo calls frameDecoder->addToQueue
        - frameDecoder->addToQueue stores the request and starts processing if idle
        - frameDecoder->getNextThumbNail creates a new mediaPlayer + videoSink per file
        - mediaPlayer->setSource(path) and ->play() start decoding
        - QVideoSink emits videoFrameChanged
        - frameDecoder->handleFrameChanged converts the frame to QImage/QPixmap
        - appropriate signal is emitted to the UI/datamodel
        - mediaPlayer is stopped and deleted
        - queue advances to next file via getNextThumbNail

    Additional improvements:
        - Invalid or null frames are retried a few times before skipping
        - Each QMediaPlayer and QVideoSink is deleted with deleteLater()
        - Uses QPointer to safely handle deletion and avoid dangling pointers
        - Rapid filter changes and aborts via stop() are safe and thread-stable

    ChatGPT was used to improve robustness, eliminate crashes, and modernize threading
    and media handling in the original code (at bottom of file).
*/
FrameDecoder::FrameDecoder() : QObject(nullptr)
{
    status = Idle;
}

FrameDecoder::~FrameDecoder()
{
    stop();  // ensure mediaPlayer and sink cleaned up
}

void FrameDecoder::stop()
{
    abort = true;
    clear();
    cleanupPlayer();
}

void FrameDecoder::clear()
{
    queue.clear();
    prevPath.clear();
    frameAlreadyDone = false;
}

bool FrameDecoder::queueContains(const QString &fPath)
{
    for (int i = 0; i < queue.size(); ++i) {
        if (queue[i].fPath == fPath) return true;
    }
    return false;
}

void FrameDecoder::addToQueue(QString path, int longSide, QString source,
                              int dmRow, int dmInstance)
{
    if (abort) return;
    // if (queueContains(path)) return;

    Item item;
    item.fPath = path;
    item.longSide = longSide;
    item.source = source;
    item.dmRow = dmRow;
    item.dmInstance = dmInstance;

    queue.append(item);
    if (status == Idle) getNextThumbNail("addToQueue");
}

void FrameDecoder::getNextThumbNail(QString src)
{
    if (abort) {
        abort = false;
        queue.clear();
        return;
    }

    if (queue.isEmpty()) {
        status = Idle;
        return;
    }

    status = Busy;
    Item item = queue.first();  // we only remove after successful processing

    // Cleanup old player
    cleanupPlayer();

    // Create fresh QMediaPlayer and QVideoSink
    mediaPlayer = new QMediaPlayer(this);
    videoSink = new QVideoSink(this);
    mediaPlayer->setVideoOutput(videoSink);

    // Increase generation to invalidate old players
    playerGeneration++;
    const int currentGeneration = playerGeneration;

    // Handle frame capture
    connect(videoSink, &QVideoSink::videoFrameChanged, this, [=](const QVideoFrame &frame) {
        if (currentGeneration != playerGeneration) {
            qDebug() << "Discarding frame from stale player generation";
            return;
        }
        handleFrameChanged(frame);
    });

    // Handle invalid media
    connect(mediaPlayer, &QMediaPlayer::mediaStatusChanged, this, [=](QMediaPlayer::MediaStatus status) {
        if (status == QMediaPlayer::InvalidMedia) {
            qWarning() << "Invalid media:" << item.fPath;
            mediaPlayer->stop();
            queue.removeFirst();
            getNextThumbNail("invalid media");
        }
    });

    // Handle invalid media
    connect(mediaPlayer, &QMediaPlayer::mediaStatusChanged, this, [=](QMediaPlayer::MediaStatus status) {
        if (currentGeneration != playerGeneration) return;

        if (status == QMediaPlayer::InvalidMedia) {
            qWarning() << "Invalid media:" << item.fPath;
            mediaPlayer->stop();
            queue.removeFirst();
            getNextThumbNail("invalid media");
        }
    });

    // Handle playback error
    connect(mediaPlayer, &QMediaPlayer::errorOccurred, this, [=](QMediaPlayer::Error, const QString &errStr) {
        if (currentGeneration != playerGeneration) return;

        qWarning() << "Playback error:" << errStr;
        mediaPlayer->stop();
        queue.removeFirst();
        getNextThumbNail("error");
    });

    try {
        mediaPlayer->setSource(item.fPath);
        mediaPlayer->play();
    } catch (...) {
        qWarning() << "Exception during play for" << item.fPath;
        queue.removeFirst();
        getNextThumbNail("exception");
    }
}

void FrameDecoder::handleFrameChanged(const QVideoFrame &frame)
{
    if (abort || queue.isEmpty()) return;

    static int attempts = 0;
    Item item = queue.takeFirst();
    const QString path = item.fPath;

    if (!frame.isValid() || frame.pixelFormat() == QVideoFrameFormat::Format_Invalid) {
        attempts++;
        if (attempts < 10) {
            queue.prepend(item);  // retry
            return;
        }

        qWarning() << "Invalid frame after retries:" << path;
        cleanupPlayer();
        attempts = 0;
        getNextThumbNail("invalid frame");
        return;
    }

    attempts = 0;

    QImage im = frame.toImage();
    if (im.isNull()) {
        qWarning() << "Null image frame:" << path;
        cleanupPlayer();
        getNextThumbNail("null image");
        return;
    }

    QImage scaledIm = (item.longSide > 0)
                ? im.scaled(item.longSide, item.longSide, Qt::KeepAspectRatio)
                : im;

    if (item.source == "dmThumb" && item.dmRow >= 0) {
        qint64 duration = mediaPlayer ? mediaPlayer->duration() : 0;
        /*
        qDebug() << "FrameDecoder::handleFrameChanged emit setFrameIcon"
                 << "dmRow =" << item.dmIdx.row()
                 << "path =" << path
                 << "isMediaPlayerNull =" << mediaPlayer.isNull()
                 << "pm.width() =" << pm.width()
                 << "pm.height() =" << pm.height()
                 << "duration =" << duration
            ; //*/
        emit setFrameIcon(item.dmRow, scaledIm, item.dmInstance, duration, this);
    } else {
        // used in FindDuplicates
        // /*
        qDebug() << "FrameDecoder::handleFrameChanged emit frameImage"
                 << "path =" << path
                 << "scaledIm.width() =" << scaledIm.width()
                 << "scaledIm.height() =" << scaledIm.height()
                 // << "duration =" << duration
            ; //*/
        emit frameImage(path, scaledIm, item.source);
    }

    prevPath = path;
    frameAlreadyDone = true;

    cleanupPlayer();
    if (!abort) getNextThumbNail("frameChanged");
}

void FrameDecoder::cleanupPlayer()
{
    if (mediaPlayer) {
        mediaPlayer->stop();
        mediaPlayer->deleteLater();
        mediaPlayer = nullptr;
    }

    if (videoSink) {
        videoSink->deleteLater();
        videoSink = nullptr;
    }
}
