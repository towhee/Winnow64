
#include "framedecoder.h"
#include "Main/global.h"
#include <QTimer>

#ifdef Q_OS_MAC
// Defined in Cache/framedecoder_mac.mm — synchronous AVFoundation thumbnail.
// Returns false if the asset can't be opened, has no video track, or the
// image generator fails; caller falls back to the QMediaPlayer pipeline.
bool macAVFoundationVideoThumbnail(const QString &fPath, int longSide,
                                   QImage &out, qint64 &outDurationMs);
#endif

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
    // A prior stop() (e.g. from a folder-change abort) leaves abort=true.
    // getNextThumbNail resets it, but addToQueue never gets there if we bail
    // on abort here — so new folders' video thumbs silently drop. A fresh
    // enqueue is new work; clear the stale flag instead of rejecting.
    abort = false;
    // if (queueContains(path)) return;

#ifdef Q_OS_MAC
    // Fast path: AVAssetImageGenerator decodes a thumbnail directly without
    // running the QMediaPlayer/QVideoSink playback pipeline. ~10–50ms per
    // file vs. hundreds of ms through the player, and it sidesteps the
    // CoreMedia VTDecompressionSession race that cleanupPlayer works
    // around. Only used for dmThumb requests bound to a real datamodel row
    // — the FindDuplicates "frameImage" path keeps using QMediaPlayer.
    if (source == "dmThumb" && dmRow >= 0) {
        QElapsedTimer t; t.start();
        QImage im;
        qint64 durationMs = 0;
        if (macAVFoundationVideoThumbnail(path, longSide, im, durationMs)
            && !im.isNull())
        {
            emit setFrameIcon(dmRow, im, dmInstance, durationMs, this);
            qint64 usToDecode = t.nsecsElapsed() / 1000;
            emit setValDm(dmRow, G::NSThumbColumn, usToDecode, dmInstance,
                          "FrameDecoder::addToQueue (AVFoundation)",
                          Qt::EditRole,
                          int(Qt::AlignRight | Qt::AlignVCenter));
            return;
        }
        /* AVFoundation failed. Do NOT fall back to QMediaPlayer for dmThumb:
           files AVFoundation rejects (no moov atom, corrupt) also fail in the
           FFmpeg backend ("Invalid media"), so the fallback produces nothing
           while leaking the player's decoder threads — the leak that exhausted
           pthread_create under sustained bouncing. Report failure so the row is
           marked done (clearVideoReadingFlag) and never retried. The
           QMediaPlayer path remains for the FindDuplicates "frameImage" use. */
        emit videoFrameFailed(dmRow, dmInstance);
        return;
    }
#endif

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
            if (item.source == "dmThumb" && item.dmRow >= 0)
                emit videoFrameFailed(item.dmRow, item.dmInstance);
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
            if (item.source == "dmThumb" && item.dmRow >= 0)
                emit videoFrameFailed(item.dmRow, item.dmInstance);
            queue.removeFirst();
            getNextThumbNail("invalid media");
        }
    });

    // Handle playback error
    connect(mediaPlayer, &QMediaPlayer::errorOccurred, this, [=](QMediaPlayer::Error, const QString &errStr) {
        if (currentGeneration != playerGeneration) return;

        qWarning() << "Playback error:" << errStr;
        mediaPlayer->stop();
        if (item.source == "dmThumb" && item.dmRow >= 0)
            emit videoFrameFailed(item.dmRow, item.dmInstance);
        queue.removeFirst();
        getNextThumbNail("error");
    });

    try {
        tFrame.start();
        mediaPlayer->setSource(item.fPath);
        mediaPlayer->play();
    } catch (...) {
        qWarning() << "Exception during play for" << item.fPath;
        if (item.source == "dmThumb" && item.dmRow >= 0)
            emit videoFrameFailed(item.dmRow, item.dmInstance);
        queue.removeFirst();
        getNextThumbNail("exception");
    }
}

void FrameDecoder::handleFrameChanged(const QVideoFrame &frame)
{
    if (abort || queue.isEmpty()) return;

    Item item = queue.takeFirst();
    const QString path = item.fPath;

    if (!frame.isValid() || frame.pixelFormat() == QVideoFrameFormat::Format_Invalid) {
        frameAttempts++;
        if (frameAttempts < 10) {
            queue.prepend(item);  // retry
            return;
        }

        qWarning() << "Invalid frame after retries:" << path;
        if (item.source == "dmThumb" && item.dmRow >= 0)
            emit videoFrameFailed(item.dmRow, item.dmInstance);
        cleanupPlayer();
        frameAttempts = 0;
        getNextThumbNail("invalid frame");
        return;
    }

    frameAttempts = 0;

    QImage im = frame.toImage();
    if (im.isNull()) {
        qWarning() << "Null image frame:" << path;
        if (item.source == "dmThumb" && item.dmRow >= 0)
            emit videoFrameFailed(item.dmRow, item.dmInstance);
        cleanupPlayer();
        getNextThumbNail("null image");
        return;
    }

    // Check for rotation (Qt 6.7+ style)
    int angle = 0;
    #if QT_VERSION >= QT_VERSION_CHECK(6, 7, 0)
        // Use the new Rotation enum
        auto rotation = frame.rotation();
        if (rotation == QtVideo::Rotation::Clockwise90) angle = 90;
        else if (rotation == QtVideo::Rotation::Clockwise180) angle = 180;
        else if (rotation == QtVideo::Rotation::Clockwise270) angle = 270;
    #else
        // Fallback for older Qt 6 versions
        angle = static_cast<int>(frame.rotationAngle());
    #endif

#ifdef Q_OS_WIN
    // On Windows the multimedia backend delivers QVideoSink frames already
    // oriented (frame.toImage() returns the rotated image) while frame.rotation()
    // still reports the angle. Applying it again would double-rotate, so a
    // portrait clip's thumbnail would come out sideways. Trust toImage().
    angle = 0;
#endif

    // Apply rotation if needed
    if (angle != 0) {
        QTransform tr;
        tr.rotate(angle);
        im = im.transformed(tr);
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

        qint64 msToDecode = tFrame.isValid() ? tFrame.nsecsElapsed() / 1000 : 0;
        emit setValDm(item.dmRow, G::NSThumbColumn, msToDecode, item.dmInstance,
                      "FrameDecoder::handleFrameChanged", Qt::EditRole,
                      int(Qt::AlignRight | Qt::AlignVCenter));
    } else {
        // used in FindDuplicates
        /*
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
    /* Tear down the QMediaPlayer / QVideoSink safely against an in-flight
       VTDecompressionSessionCreateWithOptions on CoreMedia's shared root
       queue. The synchronous mp->stop() + ~QMediaPlayer path was racing
       the format-description serialization (CFDictionary walk inside
       sbufAtom_appendDictionaryAtom) and crashing in objc_msgSend on a
       freed Obj-C object.

       1. Detach the sink so no more frames land on it.
       2. Skip stop() — ~QMediaPlayer drives the AVPlayer release path
          itself, and an extra synchronous stop widens the race window.
       3. Defer deleteLater until mediaStatus leaves the dangerous
          setup window (LoadingMedia/BufferingMedia/StalledMedia).
          3000 ms watchdog parented to mp guarantees no leak if the
          status signal never fires. */

    if (QMediaPlayer *mp = mediaPlayer.data()) {
        mediaPlayer = nullptr;     // null QPointer first so re-entry is a no-op
        mp->setVideoOutput(nullptr);

        const auto s = mp->mediaStatus();
        const bool inSetupWindow =
            s == QMediaPlayer::LoadingMedia   ||
            s == QMediaPlayer::BufferingMedia ||
            s == QMediaPlayer::StalledMedia;

        if (!inSetupWindow) {
            QTimer::singleShot(0, mp, [mp]{ mp->deleteLater(); });
        } else {
            QObject::connect(mp, &QMediaPlayer::mediaStatusChanged, mp,
                [mp](QMediaPlayer::MediaStatus ns) {
                    if (ns != QMediaPlayer::LoadingMedia   &&
                        ns != QMediaPlayer::BufferingMedia &&
                        ns != QMediaPlayer::StalledMedia) {
                        mp->deleteLater();
                    }
                });
            QTimer::singleShot(3000, mp, [mp]{ mp->deleteLater(); });
        }
    }
    if (QVideoSink *vs = videoSink.data()) {
        videoSink = nullptr;
        QTimer::singleShot(0, vs, [vs]{ vs->deleteLater(); });
    }
}
