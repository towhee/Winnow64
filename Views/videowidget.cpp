#include "videowidget.h"

/*
     QVideoWidget uses a QWindow to speed up painting and this is added as a child of the
     QVideoWidget so the drag and drop event is not propagated to the parent. An event
     filter is used to listen for the drop event.
*/

VideoWidget::VideoWidget(QWidget *parent) : QVideoWidget(parent)
{
    if (G::isLogger) G::log(CLASSFUNCTION);
    mediaPlayer = new QMediaPlayer(this);
    audioOutput = new QAudioOutput(this);
    mediaPlayer->setAudioOutput(audioOutput);
    mediaPlayer->setVideoOutput(this);
    QWidget *child = findChild<QWidget *>();
    child->installEventFilter(this);
}

void VideoWidget::load(QString fPath)
{
    if (G::isLogger) G::log(CLASSFUNCTION);
    mediaPlayer->setSource(fPath);
}

void VideoWidget::play()
{
    if (G::isLogger) G::log(CLASSFUNCTION);
    mediaPlayer->play();
}

void VideoWidget::pause()
{
    if (G::isLogger) G::log(CLASSFUNCTION);
    mediaPlayer->pause();
}

void VideoWidget::stop()
{
    if (G::isLogger) G::log(CLASSFUNCTION);
    mediaPlayer->stop();
}

int VideoWidget::duration()
{
    if (G::isLogger) G::log(CLASSFUNCTION);
    return static_cast<int>(mediaPlayer->duration());
}

void VideoWidget::setPosition(int ms)
{
    if (mediaPlayer->mediaStatus() == QMediaPlayer::MediaStatus::LoadedMedia ||
        mediaPlayer->mediaStatus() == QMediaPlayer::MediaStatus::BufferingMedia ||
        mediaPlayer->mediaStatus() == QMediaPlayer::MediaStatus::BufferedMedia ||
        mediaPlayer->mediaStatus() == QMediaPlayer::MediaStatus::EndOfMedia
       )
    {
        mediaPlayer->pause();
        mediaPlayer->setPosition(ms);
        mediaPlayer->play();
    }
}

VideoWidget::PlayState VideoWidget::playOrPause()
{
    if (G::isLogger) G::log(CLASSFUNCTION);
    if (mediaPlayer->mediaStatus() == QMediaPlayer::MediaStatus::LoadedMedia ||
        mediaPlayer->mediaStatus() == QMediaPlayer::MediaStatus::BufferingMedia ||
        mediaPlayer->mediaStatus() == QMediaPlayer::MediaStatus::BufferedMedia ||
        mediaPlayer->mediaStatus() == QMediaPlayer::MediaStatus::EndOfMedia
       )
    {
        if (mediaPlayer->playbackState() == QMediaPlayer::PlaybackState::PlayingState) {
            return PlayState::Playing;
        }
        else {
            return PlayState::Paused;
        }
    }
    return PlayState::Unavailable;
}

void VideoWidget::mousePressEvent(QMouseEvent *event)
{
    if (G::isLogger) G::log(CLASSFUNCTION);

    // ignore right click
    if (event->button() == Qt::RightButton) {
        return;
    }
    emit togglePlayOrPause();
}

bool VideoWidget::eventFilter(QObject *obj, QEvent *event)
{
    /*
    qDebug() << CLASSFUNCTION
             << "event:" <<event << "\t"
             << "event->type:" << event->type() << "\t"
             << "obj:" << obj << "\t"
             << "obj->objectName:" << obj->objectName()
             << "object->metaObject()->className:" << obj->metaObject()->className()
                ;
                //*/
    if (event->type() == QEvent::Drop) {
        QDropEvent *e = static_cast<QDropEvent *>(event);
        if (e->mimeData()->hasUrls()) {
            QString fPath = e->mimeData()->urls().at(0).toLocalFile();
            emit handleDrop(fPath);
        }
    }
    return QWidget::eventFilter(obj, event);
}
