#include "videowidget.h"

VideoWidget::VideoWidget(QWidget *parent) : QWidget{parent}
{
    isDebug = false;
    setMouseTracking(true); // This will now work natively
    setAcceptDrops(true);

    mediaPlayer = new QMediaPlayer(this);
    audioOutput = new QAudioOutput(this);
    mediaPlayer->setAudioOutput(audioOutput);

    // Manually create the sink to capture frames
    QVideoSink *sink = new QVideoSink(this);
    mediaPlayer->setVideoOutput(sink);

    connect(sink, &QVideoSink::videoFrameChanged, this, [this](const QVideoFrame &frame) {
        currentFrame = frame;
        update();
    });
}

void VideoWidget::paintEvent(QPaintEvent *)
{
    if (!currentFrame.isValid()) return;

    QPainter painter(this);
    QImage img = currentFrame.toImage();

    // Determine rotation from frame or metadata
    int angle = 0;
    auto frameRotation = currentFrame.rotation();
    if (frameRotation == QtVideo::Rotation::Clockwise90) angle = 90;
    else if (frameRotation == QtVideo::Rotation::Clockwise180) angle = 180;
    else if (frameRotation == QtVideo::Rotation::Clockwise270) angle = 270;

    if (angle == 0 && mediaPlayer) {
        QVariant orientation = mediaPlayer->metaData().value(QMediaMetaData::Orientation);
        if (orientation.isValid()) angle = orientation.toInt();
    }

    if (angle != 0) {
        QTransform tr;
        tr.rotate(angle);
        img = img.transformed(tr);
    }

    // Center and scale to fit
    QImage scaledImg = img.scaled(size(), Qt::KeepAspectRatio, Qt::SmoothTransformation);
    int x = (width() - scaledImg.width()) / 2;
    int y = (height() - scaledImg.height()) / 2;

    painter.drawImage(x, y, scaledImg);
}

void VideoWidget::load(QString fPath)
{
    mediaPlayer->setSource(fPath);
}

void VideoWidget::play() { mediaPlayer->play(); }
void VideoWidget::pause() { mediaPlayer->pause(); }
void VideoWidget::stop() { mediaPlayer->stop(); }

void VideoWidget::setPosition(int ms)
{
    if (playOrPause() != Unavailable) {
        mediaPlayer->setPosition(ms);
    }
}

VideoWidget::PlayState VideoWidget::playOrPause()
{
    auto status = mediaPlayer->mediaStatus();
    if (status == QMediaPlayer::NoMedia || status == QMediaPlayer::LoadingMedia)
        return Unavailable;

    return (mediaPlayer->playbackState() == QMediaPlayer::PlayingState)
               ? Playing : Paused;
}

void VideoWidget::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) emit togglePlayOrPause();
}

void VideoWidget::mouseMoveEvent(QMouseEvent *event)
{
    // You can now use event->pos() directly for your UI overlays
}



// #include "videowidget.h"

// /*
//      QVideoWidget uses a QWindow to speed up painting and this is added as a child
//      of the QVideoWidget so the drag and drop event is not propagated to the parent.
//      An event filter is used to listen for the drop event.
// */

// VideoWidget::VideoWidget(QWidget *parent) : QWidget(parent) // Was QVideoWidget
// {
//     isDebug = false;
//     mediaPlayer = new QMediaPlayer(this);
//     audioOutput = new QAudioOutput(this);
//     mediaPlayer->setAudioOutput(audioOutput);

//     // Crucial: Create a QVideoSink manually since QWidget doesn't have one
//     QVideoSink *sink = new QVideoSink(this);
//     mediaPlayer->setVideoOutput(sink);

//     connect(sink, &QVideoSink::videoFrameChanged, this, [this](const QVideoFrame &frame) {
//         currentFrame = frame;
//         update();
//     });

//     QWidget *child = findChild<QWidget *>();
//     if (child) {
//         child->installEventFilter(this);
//         child->setMouseTracking(true);
//     }
// }

// void VideoWidget::load(QString fPath)
// {
//     if (G::isLogger || isDebug) qDebug() << "VideoWidget::load";
//     mediaPlayer->setSource(fPath);
// }

// void VideoWidget::play()
// {
//     if (G::isLogger || isDebug) qDebug() << "VideoWidget::play";
//     mediaPlayer->play();
// }

// void VideoWidget::pause()
// {
//     if (G::isLogger || isDebug) qDebug() << "VideoWidget::pause";
//     mediaPlayer->pause();
// }

// void VideoWidget::stop()
// {
//     if (G::isLogger || isDebug) G::log("VideoWidget::stop");
//     mediaPlayer->stop();
// }

// int VideoWidget::duration()
// {
//     if (G::isLogger || isDebug) G::log("VideoWidget::duration");
//     return static_cast<int>(mediaPlayer->duration());
// }

// void VideoWidget::setPosition(int ms)
// {
//     if (mediaPlayer->mediaStatus() == QMediaPlayer::MediaStatus::LoadedMedia ||
//         mediaPlayer->mediaStatus() == QMediaPlayer::MediaStatus::BufferingMedia ||
//         mediaPlayer->mediaStatus() == QMediaPlayer::MediaStatus::BufferedMedia ||
//         mediaPlayer->mediaStatus() == QMediaPlayer::MediaStatus::EndOfMedia
//        )
//     {
//         mediaPlayer->pause();
//         mediaPlayer->setPosition(ms);
//         mediaPlayer->play();
//     }
// }

// VideoWidget::PlayState VideoWidget::playOrPause()
// {
//     if (G::isLogger || isDebug) qDebug() << "VideoWidget::PlayState VideoWidget::playOrPause";
//     if (mediaPlayer->mediaStatus() == QMediaPlayer::MediaStatus::LoadedMedia ||
//         mediaPlayer->mediaStatus() == QMediaPlayer::MediaStatus::BufferingMedia ||
//         mediaPlayer->mediaStatus() == QMediaPlayer::MediaStatus::BufferedMedia ||
//         mediaPlayer->mediaStatus() == QMediaPlayer::MediaStatus::EndOfMedia
//        )
//     {
//         if (mediaPlayer->playbackState() == QMediaPlayer::PlaybackState::PlayingState) {
//             return PlayState::Playing;
//         }
//         else {
//             return PlayState::Paused;
//         }
//     }
//     return PlayState::Unavailable;
// }

// void VideoWidget::firstFrame(QPixmap &pm)
// {
//     if (G::isLogger || isDebug) qDebug() << "VideoWidget::firstFrame";
// //    this->
// //    QVideoFrame frame = videoSurface()->currentFrame();
// //    if (!frame.isValid()) {
// //        return;
// //    }
// //    QImage image = frame.image();
// //    pm = QPixmap::fromImage(image);
// }

// void VideoWidget::mousePressEvent(QMouseEvent *event)
// {
//     if (G::isLogger || isDebug) qDebug() << "VideoWidget::mousePressEvent";

//     // ignore right click
//     if (event->button() == Qt::RightButton) {
//         return;
//     }
//     emit togglePlayOrPause();
// }

// void VideoWidget::mouseMoveEvent(QMouseEvent *event)
// {
//     // this does not work
//     //qDebug() << "VideoWidget::mousePressEvent" << event;
// }

// bool VideoWidget::eventFilter(QObject *obj, QEvent *event)
// {
//     // does not receive mouseMove events
//     /*
//     qDebug() << "\nVideoWidget::eventFilter"
//              << "event:" <<event << "\t"
//              << "event->type:" << event->type() << "\t"
//              << "obj:" << obj << "\t"
//              << "obj->objectName:" << obj->objectName()
//              << "object->metaObject()->className:" << obj->metaObject()->className()
//                 ;
//                 //*/
//     if (event->type() == QEvent::Drop) {
//         QDropEvent *e = static_cast<QDropEvent*>(event);
//         if (e->mimeData()->hasUrls()) {
//             QString fPath = e->mimeData()->urls().at(0).toLocalFile();
//             emit handleDrop(fPath);
//         }
//     }
//     return QWidget::eventFilter(obj, event);
// }

// void VideoWidget::paintEvent(QPaintEvent *event)
// {
//     // If we don't have a valid frame, let the base class handle it
//     // if (!currentFrame.isValid()) {
//     //     QVideoWidget::paintEvent(event);
//     //     return;
//     // }

//     QPainter painter(this);
//     QImage img = currentFrame.toImage();

//     // 1. Try to get rotation from the frame first
//     int angle = 0;
//     auto frameRotation = currentFrame.rotation();

//     if (frameRotation == QtVideo::Rotation::Clockwise90) angle = 90;
//     else if (frameRotation == QtVideo::Rotation::Clockwise180) angle = 180;
//     else if (frameRotation == QtVideo::Rotation::Clockwise270) angle = 270;

//     // 2. If frame rotation is 0, check the MediaPlayer metadata
//     if (angle == 0 && mediaPlayer) {
//         QVariant orientation = mediaPlayer->metaData().value(QMediaMetaData::Orientation);
//         if (orientation.isValid()) {
//             angle = orientation.toInt();
//         }
//     }

//     qDebug() << "VideoWidget::paintEvent" << event
//              << "frameRotation =" << frameRotation
//              << "angle =" << angle;

//     // Apply the transformation
//     if (angle != 0) {
//         QTransform tr;
//         tr.rotate(angle);
//         img = img.transformed(tr);
//     }

//     // Scaling and Centering
//     QRect targetRect = rect();
//     QImage scaledImg = img.scaled(targetRect.size(), Qt::KeepAspectRatio, Qt::SmoothTransformation);
//     int x = (targetRect.width() - scaledImg.width()) / 2;
//     int y = (targetRect.height() - scaledImg.height()) / 2;

//     painter.drawImage(x, y, scaledImg);
// }
