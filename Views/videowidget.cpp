#include "videowidget.h"

#ifdef Q_OS_WIN
//=============================================================================
//  Windows: native QVideoWidget
//
//  QVideoWidget uses a QWindow to speed up painting; that window is added as a
//  child of the QVideoWidget, so drop events are not propagated to the parent.
//  An event filter listens for the drop event. The backend applies the video
//  display-matrix rotation automatically, so portrait clips play upright.
//=============================================================================

VideoWidget::VideoWidget(QWidget *parent) : QVideoWidget(parent)
{
    isDebug = false;
    if (G::isLogger || isDebug) G::log("VideoWidget::VideoWidget");
    mediaPlayer = new QMediaPlayer(this);
    audioOutput = new QAudioOutput(this);
    mediaPlayer->setAudioOutput(audioOutput);
    mediaPlayer->setVideoOutput(this);
    if (QWidget *child = findChild<QWidget *>()) {
        child->installEventFilter(this);
        child->setMouseTracking(true);      // this does not work
    }
}

void VideoWidget::load(QString fPath)
{
    if (G::isLogger || isDebug) G::log("VideoWidget::load", fPath);
    mediaPlayer->setSource(fPath);
    // force reset if was hidden
    resize(size() - QSize(1,0));
    resize(size() + QSize(1,0));
}

void VideoWidget::setPosition(int ms)
{
    auto status = mediaPlayer->mediaStatus();
    if (status == QMediaPlayer::LoadedMedia    ||
        status == QMediaPlayer::BufferingMedia ||
        status == QMediaPlayer::BufferedMedia  ||
        status == QMediaPlayer::EndOfMedia)
    {
        mediaPlayer->pause();
        mediaPlayer->setPosition(ms);
        mediaPlayer->play();
    }
}

VideoWidget::PlayState VideoWidget::playOrPause()
{
    auto status = mediaPlayer->mediaStatus();
    if (status == QMediaPlayer::LoadedMedia    ||
        status == QMediaPlayer::BufferingMedia ||
        status == QMediaPlayer::BufferedMedia  ||
        status == QMediaPlayer::EndOfMedia)
    {
        return (mediaPlayer->playbackState() == QMediaPlayer::PlayingState)
                   ? Playing : Paused;
    }
    return Unavailable;
}

bool VideoWidget::eventFilter(QObject *obj, QEvent *event)
{
    if (event->type() == QEvent::Drop) {
        QDropEvent *e = static_cast<QDropEvent*>(event);
        if (e->mimeData()->hasUrls()) {
            QString fPath = e->mimeData()->urls().at(0).toLocalFile();
            emit handleDrop(fPath);
        }
    }
    return QVideoWidget::eventFilter(obj, event);
}

#else
//=============================================================================
//  macOS: manual QVideoSink + paintEvent (fixes a macOS QVideoWidget
//  orientation bug). Behaviour unchanged.
//=============================================================================

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

#endif

//=============================================================================
//  Shared
//=============================================================================

void VideoWidget::play() { mediaPlayer->play(); }
void VideoWidget::pause() { mediaPlayer->pause(); }
void VideoWidget::stop() { mediaPlayer->stop(); }

int VideoWidget::duration()
{
    return static_cast<int>(mediaPlayer->duration());
}

void VideoWidget::mousePressEvent(QMouseEvent *event)
{
    // ignore right click
    if (event->button() == Qt::RightButton) return;
    if (event->button() == Qt::LeftButton) emit togglePlayOrPause();
}

void VideoWidget::mouseMoveEvent(QMouseEvent *event)
{
    // You can now use event->pos() directly for your UI overlays
    Q_UNUSED(event)
}
