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
                              QModelIndex dmIdx, int dmInstance)
{
    if (abort) return;
    // if (queueContains(path)) return;

    Item item;
    item.fPath = path;
    item.longSide = longSide;
    item.source = source;
    item.dmIdx = dmIdx;
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

    // Handle frame capture
    connect(videoSink, &QVideoSink::videoFrameChanged, this, [=](const QVideoFrame &frame) {
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

    // Handle errors
    connect(mediaPlayer, &QMediaPlayer::errorOccurred, this, [=](QMediaPlayer::Error, const QString &errStr) {
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

    if (item.source == "dmThumb" && item.dmIdx.isValid()) {
        QPixmap pm = QPixmap::fromImage(scaledIm);
        qint64 duration = mediaPlayer ? mediaPlayer->duration() : 0;
        /*
        qDebug() << "FrameDecoder::handleFrameChanged"
                 << "path =" << path
                 << "pm.width() =" << pm.width()
                 << "pm.height() =" << pm.height()
                 << "duration =" << duration
            ; //*/
        emit setFrameIcon(item.dmIdx, pm, item.dmInstance, duration, this);
    } else {
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

// Before use ChatGPT changes (above)
// #include "framedecoder.h"
// #include "Main/global.h"

// /*
//     Generates a thumbnail from the first video frame in a video file.

//     This is accomplished by creating a QMediaPlayer that decodes and plays the video file
//     to the virtual canvas QVideoSink. Each time the video frame changes QVideoSink emits
//     a videoFrameChanged signal. FrameDecoder::frameChanged receives the signal, converts
//     the frame into a QImage, emits setFrameIcon and stops the QMediaPlayer. The
//     setFrameIcon signal is received by DataModel::setIconFromFrame, where the icon is
//     added to the datamodel.

//     This convoluted process is required because QVideoSink does not know which file the
//     video frame came from. When processing many files, it is not guaranteed that the
//     signal/slots will be sequential. FrameDecoder maintains a queue of videos to process.

//     After mediaPlayer->stop() there may be a number of QVideoSink frameChanged signals in
//     the event queue. It is necessary to execute a qApp->processEvents to clear these
//     excess signals.

//     Summary of sequence:
//         - thumb->loadFromVideo signals to frameDecoder->addToQueue
//         - frameDecoder->addToQueue calls frameDecoder->getNextThumbNail
//         - start processing loop
//             - frameDecoder->getNextThumbNail sets media source to first in queue
//               and starts mediaPlayer
//             - mediaPlayer signals frameDecoder->frameChanged
//             - frameDecoder->frameChanged converts video frame to QPixmap
//             - frameDecoder->frameChanged signals dm->setIconFromVideoFrame
//             - frameDecoder->frameChanged stops mediaPlayer
//             - frameDecoder->frameChanged calls frameDecoder->getNextThumbNail
//        - loop until queue is empty
// */

// FrameDecoder::FrameDecoder()  : QObject(nullptr)
// {
//     if (G::isLogger) G::log("FrameDecoder::FrameDecoder");
//     status = Status::Idle;
//     mediaPlayer = new QMediaPlayer();
//     videoSink = new QVideoSink;
//     mediaPlayer->setVideoOutput(videoSink);
//     connect(videoSink, &QVideoSink::videoFrameChanged, this, &FrameDecoder::frameChanged);
//     connect(mediaPlayer, &QMediaPlayer::errorOccurred, this, &FrameDecoder::errorOccurred);
//     abort = false;
//     isDebugging = false;
// }

// FrameDecoder::~FrameDecoder()
// {
//     if (mediaPlayer) {
//         mediaPlayer->stop();
//         mediaPlayer->setVideoOutput(nullptr);
//         disconnect(mediaPlayer, nullptr, this, nullptr);
//         delete mediaPlayer;
//         mediaPlayer = nullptr;
//     }
//     if (videoSink) {
//         disconnect(videoSink, nullptr, this, nullptr);
//         delete videoSink;
//         videoSink = nullptr;
//     }
// }

// void FrameDecoder::stop()
// {
//     if (G::isLogger) G::log("FrameDecoder::stop");
//     qDebug() << "FrameDecoder::stop  isGuiThread =" << G::isGuiThread();
//     abort = true;
//     clear();
//     if (mediaPlayer) {
//         mediaPlayer->stop();
//     }
// }

// void FrameDecoder::clear()
// {
//     queue.clear();
//     prevPath = "";
//     frameAlreadyDone = false;
//     return;
// }

// void FrameDecoder::addToQueue(QString path, int longSide, QString source,
//                               QModelIndex dmIdx, int dmInstance)
// {
//     if (abort) return;
//     Item item;
//     item.fPath = path;
//     item.longSide = longSide;
//     item.source = source;
//     item.dmIdx = dmIdx;
//     item.dmInstance = dmInstance;
//     queue.append(item);
//     if (isDebugging)
//     {
//         qDebug() << "FrameDecoder::addToQueue                 "
//                  //<< "ddmIdx =" << dmIdx
//                  << "source =" << source
//                  << "longSide =" << longSide
//                  << "row =" << dmIdx.row()
//                  << "queue size =" << queue.size()
//                  << "status =" << status
//                  << "  " << item.fPath
//             ;
//     }
//     if (status == Status::Idle) getNextThumbNail("addToQueue");
// }

// void FrameDecoder::getNextThumbNail(QString src)
// {
//     if (G::isLogger) G::log("FrameDecoder::getFrame");

//     /*
//     if (abort) {
//         abort = false;
//         queue.clear();
//         // emit stopped("FrameDecoder");
//     }

//     if (queue.isEmpty()) {
//         if (isDebugging)
//         {
//             qDebug() << "FrameDecoder::getNextThumbNail           "
//                      << "queue.isEmpty() =" << queue.isEmpty()
//                      << "queue size =" << queue.size()
//                      << "  abort =" << abort
//                 ;
//         }
//         status = Status::Idle;
//         return;
//     }

//     status = Status::Busy;
//     if (isDebugging)
//     {
//         qDebug() << "FrameDecoder::getNextThumbNail           "
//                  << "row =" << queue.at(0).dmIdx.row()
//                  << "queue size =" << queue.size()
//                  << "status =" << status
//                  << "  " << queue.at(0).fPath
//                  << "  src =" << src
//             ;
//     }
//     if (!abort) mediaPlayer->setSource(queue.at(0).fPath);
//     if (!abort) mediaPlayer->play();
//     //*/

//     // ChatGPT
//     if (abort) {
//         abort = false;
//         queue.clear();
//         return;
//     }

//     if (queue.isEmpty()) {
//         status = Status::Idle;
//         return;
//     }

//     status = Status::Busy;

//     if (!mediaPlayer) {
//         qWarning() << "FrameDecoder::getNextThumbNail: mediaPlayer is null!";
//         queue.removeFirst();
//         getNextThumbNail("no mediaPlayer");
//         return;
//     }

//     const QString path = queue.at(0).fPath;
//     try {
//         mediaPlayer->setSource(path);
//         mediaPlayer->play();
//     } catch (...) {
//         qWarning() << "FrameDecoder: exception in setSource/play for" << path;
//         queue.removeFirst();
//         getNextThumbNail("exception");
//     }
// }

// void FrameDecoder::frameChanged(const QVideoFrame frame)
// {
//     if (G::isLogger) G::log("FrameDecoder::frameChanged");

//     /* Original
//     if (abort) return;

//     bool validFrame = false;
//     static int attempts = 0;

//     if (mediaPlayer->playbackState() == QMediaPlayer::PlaybackState::StoppedState) return;

//     QImage im = frame.toImage();

//     // wait for first valid frame
//     if (im.isNull() || !frame.isValid() || queue.empty()) {
//         validFrame = false;
//     }
//     else {
//         validFrame = true;
//     }

//     attempts++;
//     if (isDebugging)
//     {
//         qDebug() << "FrameDecoder::frameChanged               "
//                  << "row =" << queue.at(0).dmIdx.row()
//                  << "valid index =" << queue.at(0).dmIdx.isValid()
//                  << "attempts =" << attempts
//                  << "validFrame =" << validFrame
//                  << "queue size =" << queue.size()
//                  << "source =" << queue.at(0).source
//                  << "isGUI =" << G::isGuiThread()
//             ;
//     }

//     if (!validFrame && attempts < 10) return;

//     if (validFrame) {
//         QString path = queue.at(0).fPath;
//         int ls = queue.at(0).longSide;
//         if (queue.at(0).source == "dmThumb") {
//             // set the icon in datamodel
//             if (queue.at(0).dmIdx.isValid()) {
//                 QPixmap pm = QPixmap::fromImage(im.scaled(ls, ls, Qt::KeepAspectRatio));
//                 qint64 duration = mediaPlayer->duration();

//                 emit setFrameIcon(queue.at(0).dmIdx, pm, queue.at(0).dmInstance,
//                                   duration, thisFrameDecoder);
//             }
//         }
//         else {
//             // autonomous source
//             QString source = queue.at(0).source;
//             // qDebug() << "FrameDecoder::frameChanged  ls =" << ls;
//             if (ls > 0)
//                 emit frameImage(queue.at(0).fPath, im.scaled(ls, ls, Qt::KeepAspectRatio), source);
//             else
//                 emit frameImage(queue.at(0).fPath, im, source);
//         }
//         prevPath = path;
//     }

//     mediaPlayer->stop();
//     attempts = 0;
//     frameAlreadyDone = true;
//     if (!queue.isEmpty()) queue.remove(0);
//     // exhaust QVideoSink frameChanged signals after mediaPlayer is stopped
//     // if (G::useProcessEvents && !isStressTest) qApp->processEvents(QEventLoop::ExcludeUserInputEvents | QEventLoop::ExcludeSocketNotifiers);
//     if (!abort) getNextThumbNail("frameChanged");
//     //*/ end original

//     // ChatGPT 1st version
//     if (abort || queue.isEmpty()) return;

//     static int attempts = 0;

//     // Capture and remove item at top of queue immediately to prevent race
//     Item item = queue.takeFirst();  // <--- no longer use queue.at(0) below
//     const QString path = item.fPath;

//     // Defensive check
//     if (!frame.isValid() || frame.pixelFormat() == QVideoFrameFormat::Format_Invalid) {
//         attempts++;
//         if (attempts < 10) {
//             // Requeue the item (optional)
//             queue.prepend(item);
//             return;
//         }

//         qWarning() << "FrameDecoder: invalid frame for" << path;
//         videoSink->blockSignals(true);
//         mediaPlayer->stop();
//         QCoreApplication::processEvents(QEventLoop::ExcludeSocketNotifiers);
//         videoSink->blockSignals(false);

//         attempts = 0;
//         getNextThumbNail("invalid frame");
//         return;
//     }

//     // Valid frame
//     attempts = 0;
//     QImage im = frame.toImage();
//     if (im.isNull()) {
//         qWarning() << "FrameDecoder: null image from frame for" << path;
//         getNextThumbNail("null image");
//         return;
//     }

//     int ls = item.longSide;

//     if (item.source == "dmThumb" && item.dmIdx.isValid()) {
//         QPixmap pm = QPixmap::fromImage(im.scaled(ls, ls, Qt::KeepAspectRatio));
//         qint64 duration = mediaPlayer->duration();
//         emit setFrameIcon(item.dmIdx, pm, item.dmInstance, duration, thisFrameDecoder);
//     } else {
//         QImage scaledIm = (ls > 0) ? im.scaled(ls, ls, Qt::KeepAspectRatio) : im;
//         emit frameImage(path, scaledIm, item.source);
//     }

//     prevPath = path;
//     frameAlreadyDone = true;

//     // Stop playback and flush signals
//     videoSink->blockSignals(true);
//     mediaPlayer->stop();
//     QCoreApplication::processEvents(QEventLoop::ExcludeSocketNotifiers);
//     videoSink->blockSignals(false);

//     if (!abort) getNextThumbNail("frameChanged");
       // end ChatGPT 1st version
// }

// void FrameDecoder::errorOccurred(QMediaPlayer::Error, const QString &msg)
// {
//     G::issue("Error", msg, "FrameDecoder::errorOccurred", queue.at(0).dmIdx.row(), queue.at(0).fPath);
// }
