#include "videoview.h"

VideoView::VideoView(QWidget *parent, IconView *thumbView) : QWidget{parent}
{
    if (G::isLogger) G::log(CLASSFUNCTION);

    this->thumbView = thumbView;

    video = new VideoWidget(this);

    setStyleSheet("QSlider{min-height: 24;}");

    connect(video->mediaPlayer, &QMediaPlayer::durationChanged, this, &VideoView::durationChanged);
    connect(video->mediaPlayer, &QMediaPlayer::positionChanged, this, &VideoView::positionChanged);
    connect(video, &VideoWidget::togglePlayOrPause, this, &VideoView::playOrPause);

    playPauseBtn = new QToolButton;
    playPauseBtn->setIcon(QIcon(":/images/icon16/media_play.png"));
    connect(playPauseBtn, &QToolButton::pressed, this, &VideoView::playOrPause);

    scrub = new QSlider(Qt::Horizontal, this);
    scrub->setRange(0, 100);
    connect(scrub, &QSlider::sliderMoved, this, &VideoView::scrubMoved);
    connect(scrub, &QSlider::sliderPressed, this, &VideoView::scrubPressed);

    positionLabel = new QLabel;
    positionLabel->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    positionLabel->setText("");

    QHBoxLayout *controlLayout = new QHBoxLayout;
    controlLayout->setContentsMargins(0,0,0,0);
    controlLayout->addSpacing(8);
    controlLayout->addWidget(playPauseBtn);
    controlLayout->addWidget(scrub);
    controlLayout->addSpacing(6);
    controlLayout->addWidget(positionLabel);
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
    if (G::isLogger) G::log(CLASSFUNCTION);
    video->load(fPath);
    play();
    pause();
}

void VideoView::play()
{
    if (G::isLogger) G::log(CLASSFUNCTION);
    if (position >= duration) video->setPosition(0);
    video->play();
    playPauseBtn->setIcon(QIcon(":/images/icon16/media_pause.png"));
}

void VideoView::pause()
{
    if (G::isLogger) G::log(CLASSFUNCTION);
    video->pause();
    playPauseBtn->setIcon(QIcon(":/images/icon16/media_play.png"));
}

void VideoView::stop()
{
    if (G::isLogger) G::log(CLASSFUNCTION);
    video->stop();
}

void VideoView::scrubMoved(int ms)
{
    if (G::isLogger) G::log(CLASSFUNCTION);
    video->setPosition(ms * 1000);
}

void VideoView::scrubPressed()
{
    if (G::isLogger) G::log(CLASSFUNCTION);
    video->setPosition(scrub->value() * 1000);
}

void VideoView::durationChanged(qint64 duration_ms)
{
    if (G::isLogger) G::log(CLASSFUNCTION);
    duration = duration_ms / 1000;
    scrub->setMaximum(duration);
    updatePositionLabel();
}

void VideoView::positionChanged(qint64 progress_ms)
{
    if (G::isLogger) G::log(CLASSFUNCTION);
    position = progress_ms / 1000;
    if (!duration) return;
    if (duration > 0 && !scrub->isSliderDown()) {
        scrub->setValue(position);
    }
    updatePositionLabel();
    if (position >= duration) {
        pause();
    }
}

void VideoView::updatePositionLabel()
{
    if (G::isLogger) G::log(CLASSFUNCTION);
    QString tStr;
    if (position || duration) {
        QTime currentTime((position / 3600) % 60, (position / 60) % 60,
            position % 60, (position * 1000) % 1000);
        QTime totalTime((duration / 3600) % 60, (duration / 60) % 60,
            duration % 60, (duration * 1000) % 1000);
        QString format = "mm:ss";
        if (duration > 3600)
            format = "hh:mm:ss";
        tStr = currentTime.toString(format) + " / " + totalTime.toString(format);
        positionLabel->setText(tStr);
    }
}

void VideoView::playOrPause()
{
    if (G::isLogger) G::log(CLASSFUNCTION);
    qDebug() << CLASSFUNCTION;
    switch (video->playOrPause()) {
    case VideoWidget::PlayState::Playing:
        pause();
        break;
    case VideoWidget::PlayState::Paused:
        play();
        break;
    case VideoWidget::PlayState::Unavailable:
        break;
    }
}

void VideoView::wheelEvent(QWheelEvent *event)
{
    if (G::isLogger) G::log(CLASSFUNCTION);

    // wheel scrolling / trackpad swiping = next/previous image
    static int deltaSum = 0;
    static int prevDelta = 0;
    int delta = event->angleDelta().y();
    if ((delta > 0 && prevDelta < 0) || (delta < 0 && prevDelta > 0)) {
        deltaSum = delta;
    }
    deltaSum += delta;

    /*
    qDebug() << "ImageView::wheelEvent"
             << "event->angleDelta() =" << event->angleDelta()
             << "event->angleDelta().y() =" << event->angleDelta().y()
             << "deltaSum =" << deltaSum
//             << "event->pixelDelta().y() =" << event->pixelDelta().y()
                ;
                //*/

    if (deltaSum > G::wheelSensitivity) {
        thumbView->selectPrev();
        deltaSum = 0;
    }

    if (deltaSum < (-G::wheelSensitivity)) {
        thumbView->selectNext();
        deltaSum = 0;
    }
}

bool VideoView::event(QEvent *event) {
    /*
        Trap back/forward buttons on Logitech mouse to toggle pick status on thumbnail
    */
    if (G::isLogger) G::log(CLASSFUNCTION);
    if (event->type() == QEvent::NativeGesture) {
        emit togglePick();
        /*
        QNativeGestureEvent *e = static_cast<QNativeGestureEvent *>(event);
        if (e->value() == 0) {
            // forward
        }
        else {
            // back
        }
        //*/
    }
    QWidget::event(event);
    return true;
}

