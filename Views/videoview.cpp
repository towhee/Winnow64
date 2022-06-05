#include "videoview.h"

VideoView::VideoView(QWidget *parent)
    : QVideoWidget{parent}
{
    if (G::isLogger) G::log(__FUNCTION__);
    mediaPlayer = new QMediaPlayer(this);
    audioOutput = new QAudioOutput(this);
    mediaPlayer->setAudioOutput(audioOutput);
    mediaPlayer->setVideoOutput(this);
}

void VideoView::load(QString fPath)
{
    if (G::isLogger) G::log(__FUNCTION__);
    mediaPlayer->setSource(fPath);
}

void VideoView::play()
{
    if (G::isLogger) G::log(__FUNCTION__);
    mediaPlayer->play();
}

void VideoView::pause()
{
    if (G::isLogger) G::log(__FUNCTION__);
    mediaPlayer->pause();
}

void VideoView::stop()
{
    if (G::isLogger) G::log(__FUNCTION__);
    mediaPlayer->stop();
}

void VideoView::mousePressEvent(QMouseEvent *event)
{
/*
*/
    if (G::isLogger) G::log(__FUNCTION__);
    qDebug() << __FUNCTION__ << "mediaPlayer->playbackState()" << mediaPlayer->playbackState();
    qDebug() << __FUNCTION__ << "mediaPlayer->mediaStatus()" << mediaPlayer->mediaStatus();

    if (mediaPlayer->mediaStatus() == QMediaPlayer::MediaStatus::LoadedMedia ||
        mediaPlayer->mediaStatus() == QMediaPlayer::MediaStatus::BufferingMedia ||
        mediaPlayer->mediaStatus() == QMediaPlayer::MediaStatus::BufferedMedia ||
        mediaPlayer->mediaStatus() == QMediaPlayer::MediaStatus::EndOfMedia
       )
    {
        if (mediaPlayer->playbackState() == QMediaPlayer::PlaybackState::PlayingState) {
            mediaPlayer->pause();
        }
        else {
            mediaPlayer->play();
        }
    }
}
