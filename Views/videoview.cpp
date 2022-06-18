#include "videoview.h"

VideoView::VideoView(QWidget *parent, IconView *thumbView) : QWidget{parent}
{
    if (G::isLogger) G::log(__FUNCTION__);

    this->thumbView = thumbView;

    video = new VideoWidget(this);

    setStyleSheet("QSlider{min-height: 24;}");

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
    video->setPosition(ms * 1000);
}

void VideoView::scrubPressed()
{
    if (G::isLogger) G::log(__FUNCTION__);
    video->setPosition(scrub->value() * 1000);
}

void VideoView::durationChanged(qint64 duration)
{
    if (G::isLogger) G::log(__FUNCTION__);
    this->duration = duration / 1000;
}

void VideoView::positionChanged(qint64 progress)
{
    if (G::isLogger) G::log(__FUNCTION__);
    if (duration > 0 && !scrub->isSliderDown()) {
        scrub->setMaximum(duration);
        scrub->setValue(progress / 1000);
    }
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

void VideoView::wheelEvent(QWheelEvent *event)
{
    if (G::isLogger) G::log(__FUNCTION__);

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
    if (G::isLogger) G::log(__FUNCTION__);
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
}

