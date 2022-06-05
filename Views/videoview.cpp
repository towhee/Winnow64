#include "videoview.h"

VideoView::VideoView(QWidget *parent)
    : QWidget{parent}
//    : QVideoWidget{parent}
{
    if (G::isLogger) G::log(__FUNCTION__);
    video = new VideoWidget(this);

    setStyleSheet("QSlider{min-height: 24;}"
//                  "QLabel{height: 24; background-color: red}"
//                  "QToolButton{background-color: red}"
                  );

    connect(video->mediaPlayer, &QMediaPlayer::durationChanged, this, &VideoView::durationChanged);
    connect(video->mediaPlayer, &QMediaPlayer::positionChanged, this, &VideoView::positionChanged);

    playPauseBtn = new QToolButton;
    playPauseBtn->setIcon(style()->standardIcon(QStyle::SP_MediaPlay));
    connect(playPauseBtn, &QToolButton::pressed, this, &VideoView::playOrPause);

    scrub = new QSlider(Qt::Horizontal, this);
    scrub->setRange(0, 100);
    connect(scrub, &QSlider::sliderMoved, this, &VideoView::scrubMoved);
    connect(scrub, &QSlider::sliderPressed, this, &VideoView::scrubPressed);

    position = new QLabel;
    position->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    position->setText("");

    QHBoxLayout *controlLayout = new QHBoxLayout;
    controlLayout->setContentsMargins(0,0,0,0);
    controlLayout->addSpacing(8);
    controlLayout->addWidget(playPauseBtn);
    controlLayout->addWidget(scrub);
    controlLayout->addSpacing(6);
    controlLayout->addWidget(position);
    controlLayout->addSpacing(12);

    QVBoxLayout *videoLayout = new QVBoxLayout(this);
    videoLayout->setContentsMargins(0,0,0,0);
    videoLayout->addWidget(video,1);
    videoLayout->addSpacing(-8);
    videoLayout->addLayout(controlLayout);
    setLayout(videoLayout);
}

void VideoView::load(QString fPath)
{
    if (G::isLogger) G::log(__FUNCTION__);
    video->load(fPath);
    qDebug() << __FUNCTION__ << "duration =" << video->duration();
//    scrub->setRange(0, video->duration());
}

void VideoView::play()
{
    if (G::isLogger) G::log(__FUNCTION__);
    video->play();
}

void VideoView::pause()
{
    if (G::isLogger) G::log(__FUNCTION__);
    video->pause();
}

void VideoView::stop()
{
    if (G::isLogger) G::log(__FUNCTION__);
    video->stop();
}

void VideoView::scrubMoved(int ms)
{
    if (G::isLogger) G::log(__FUNCTION__);
    qDebug() << "VideoView::seek" << ms;
    video->setPosition(ms * 1000);
}

void VideoView::scrubPressed()
{
    if (G::isLogger) G::log(__FUNCTION__);
    qDebug() << "VideoView::seek" << scrub->value();
    video->setPosition(scrub->value() * 1000);
}

void VideoView::durationChanged(qint64 duration)
{
    if (G::isLogger) G::log(__FUNCTION__);
    qDebug() << "VideoView::durationChanged" << duration;
    this->duration = duration / 1000;
//    scrub->setMaximum(duration);
}

void VideoView::positionChanged(qint64 progress)
{
    if (G::isLogger) G::log(__FUNCTION__);
//    qDebug() << "VideoView::positionChanged" << progress;
    if (duration > 0 && !scrub->isSliderDown()) {
        scrub->setMaximum(duration);
        scrub->setValue(progress / 1000);
    }
//    if (!scrub->isSliderDown())
//        scrub->setValue(progress);

    updateDurationInfo(progress / 1000);
}

void VideoView::updateDurationInfo(qint64 currentInfo)
{
    if (G::isLogger) G::log(__FUNCTION__);
    QString tStr;
    if (currentInfo || duration) {
        QTime currentTime((currentInfo / 3600) % 60, (currentInfo / 60) % 60,
            currentInfo % 60, (currentInfo * 1000) % 1000);
        QTime totalTime((duration / 3600) % 60, (duration / 60) % 60,
            duration % 60, (duration * 1000) % 1000);
        QString format = "mm:ss";
        if (duration > 3600)
            format = "hh:mm:ss";
        tStr = currentTime.toString(format) + " / " + totalTime.toString(format);
    }
    position->setText(tStr);
}

void VideoView::playOrPause()
{
    if (G::isLogger) G::log(__FUNCTION__);
    switch (video->playOrPause()) {
    case VideoWidget::PlayState::Playing:
        video->pause();
        playPauseBtn->setIcon(style()->standardIcon(QStyle::SP_MediaPlay));
        break;
    case VideoWidget::PlayState::Paused:
        video->play();
        playPauseBtn->setIcon(style()->standardIcon(QStyle::SP_MediaPause));
        break;
    case VideoWidget::PlayState::Unavailable:
        break;
    }
}

//void VideoView::mousePressEvent(QMouseEvent *event)
//{
///*
//*/
//    if (G::isLogger) G::log(__FUNCTION__);
//    qDebug() << __FUNCTION__ << "mediaPlayer->playbackState()" << mediaPlayer->playbackState();
//    qDebug() << __FUNCTION__ << "mediaPlayer->mediaStatus()" << mediaPlayer->mediaStatus();

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
//}
