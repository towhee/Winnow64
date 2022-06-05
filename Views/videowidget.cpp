#include "videowidget.h"

VideoWidget::VideoWidget(QWidget *parent) : QVideoWidget(parent)
{
    if (G::isLogger) G::log(__FUNCTION__);
    mediaPlayer = new QMediaPlayer(this);
    audioOutput = new QAudioOutput(this);
    mediaPlayer->setAudioOutput(audioOutput);
    mediaPlayer->setVideoOutput(this);
}

void VideoWidget::load(QString fPath)
{
    if (G::isLogger) G::log(__FUNCTION__);
    mediaPlayer->setSource(fPath);
}

void VideoWidget::play()
{
    if (G::isLogger) G::log(__FUNCTION__);
    mediaPlayer->play();
}

void VideoWidget::pause()
{
    if (G::isLogger) G::log(__FUNCTION__);
    mediaPlayer->pause();
}

void VideoWidget::stop()
{
    if (G::isLogger) G::log(__FUNCTION__);
    mediaPlayer->stop();
}

int VideoWidget::duration()
{
    if (G::isLogger) G::log(__FUNCTION__);
    qDebug() << __FUNCTION__ << mediaPlayer->duration();
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
    if (G::isLogger) G::log(__FUNCTION__);
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
    if (G::isLogger) G::log(__FUNCTION__);
    qDebug() << __FUNCTION__ << "mediaPlayer->playbackState()" << mediaPlayer->playbackState();
    qDebug() << __FUNCTION__ << "mediaPlayer->mediaStatus()" << mediaPlayer->mediaStatus();

    switch (playOrPause()) {
    case PlayState::Playing:
        mediaPlayer->pause();
        break;
    case PlayState::Paused:
        mediaPlayer->play();
        break;
    case PlayState::Unavailable:
        break;
    }

//    if (mediaPlayer->mediaStatus() == QMediaPlayer::MediaStatus::LoadedMedia ||
//        mediaPlayer->mediaStatus() == QMediaPlayer::MediaStatus::BufferingMedia ||
//        mediaPlayer->mediaStatus() == QMediaPlayer::MediaStatus::BufferedMedia ||
//        mediaPlayer->mediaStatus() == QMediaPlayer::MediaStatus::EndOfMedia
//       )
//    {
//        if (mediaPlayer->playbackState() == QMediaPlayer::PlaybackState::PlayingState) {
//            mediaPlayer->pause();
//        }
//        else {
//            mediaPlayer->play();
//        }
//    }
}
