#include "videoview.h"

VideoView::VideoView(QWidget *parent, IconView *thumbView, Selection *sel) : QWidget{parent}
{
    if (G::isLogger) G::log("VideoView::VideoView");
    setObjectName("VideoView");
    setMouseTracking(true);     // this does not work

    this->thumbView = thumbView;
    this->sel = sel;

    video = new VideoWidget(this);
    video->setObjectName("VideoWidget");
    //video->children()[0]->setMouseTracking(true);

    setAcceptDrops(true);
    video->setAcceptDrops(true);

    setStyleSheet("QSlider{min-height: 24;}");

    t = new QTimer(this);
    t->setInterval(50);
    t->setSingleShot(false);

    connect(video->mediaPlayer, &QMediaPlayer::durationChanged, this, &VideoView::durationChanged);
    connect(t, &QTimer::timeout, this, &VideoView::positionChanged);
    connect(video, &VideoWidget::togglePlayOrPause, this, &VideoView::playOrPause);
    connect(video, &VideoWidget::handleDrop, this, &VideoView::handleDrop);

    playPauseBtn = new QToolButton;
    playPauseBtn->setIcon(QIcon(":/images/icon16/media_play.png"));
    playPauseBtn->setToolTip("Play/Pause.  Keyboard shortcut: Space Key");
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

    isDebug = false;
}

void VideoView::load(QString fPath)
{
    if (G::isLogger || G::isFlowLogger || isDebug)
        qDebug() << "VideoView::load" << fPath;
    video->load(fPath);
    play();
    pause();
}

void VideoView::play()
{
    if (G::isLogger || isDebug)
        qDebug() << "VideoView::play" << video->mediaPlayer->source();
    mousePos = QCursor::pos();
    emit hideMouseCursor();
    if (position >= duration) video->setPosition(0);
    video->play();
    t->start();
    playPauseBtn->setIcon(QIcon(":/images/icon16/media_pause.png"));
}

void VideoView::pause()
{
    if (G::isLogger || isDebug)
        qDebug() << "VideoView::pause";
    emit showMouseCursor();
    video->pause();
    t->stop();
    playPauseBtn->setIcon(QIcon(":/images/icon16/media_play.png"));
}

void VideoView::stop()
{
    if (G::isLogger || G::isFlowLogger || isDebug)
        G::log("VideoView::stop");
    emit showMouseCursor();
    video->stop();
    QGuiApplication::restoreOverrideCursor();
    QGuiApplication::restoreOverrideCursor();
    QGuiApplication::restoreOverrideCursor();
}

void VideoView::scrubMoved(int ms)
{
    if (G::isLogger || isDebug) qDebug() << "VideoView::scrubMoved";
    int currentState = video->playOrPause();  // 0 = playing  1 = paused
    video->setPosition(ms);                   // auto calls play after position change
    if (currentState == VideoWidget::PlayState::Paused)
        video->pause();
}

void VideoView::scrubPressed()
{
    if (G::isLogger || isDebug) qDebug() << "VideoView::scrubPressed";
    int currentState = video->playOrPause();  // 0 = playing  1 = paused
    video->setPosition(scrub->value());       // auto calls play after position change
    if (currentState == VideoWidget::PlayState::Paused)
        video->pause();
}

void VideoView::durationChanged(qint64 duration_ms)
{
    if (G::isLogger || isDebug) qDebug() << "VideoView::durationChanged";
    duration = duration_ms / 1000;
    scrub->setMaximum(duration_ms);
    updatePositionLabel();
}

void VideoView::positionChanged()
{
    if (G::isLogger || isDebug) qDebug() << "VideoView::positionChanged";
    position = video->mediaPlayer->position() / 1000;
    if (!scrub->isSliderDown()) {
        scrub->setValue(video->mediaPlayer->position());
    }
    updatePositionLabel();
    if (position >= duration) {
        pause();
    }
}

void VideoView::updatePositionLabel()
{
    if (G::isLogger || isDebug) qDebug() << "VideoView::updatePositionLabel";
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
    if (G::isLogger || isDebug) qDebug() << "VideoView::playOrPause";
    //qDebug() << "";
    switch (video->playOrPause()) {
    case VideoWidget::PlayState::Playing:  // 1
        pause();
        break;
    case VideoWidget::PlayState::Paused:   // 2
        play();
        break;
    case VideoWidget::PlayState::Unavailable:
        break;
    }
}

void VideoView::wheelEvent(QWheelEvent *event)
{
    if (G::isLogger || isDebug) qDebug() << "VideoView::wheelEvent";

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
        sel->prev();
        deltaSum = 0;
    }

    if (deltaSum < (-G::wheelSensitivity)) {
        sel->next();
        deltaSum = 0;
    }
}

bool VideoView::event(QEvent *event)
{
/*
    Trap back/forward buttons on Logitech mouse to toggle pick status on thumbnail
*/
    //qDebug() << "VideoView::event" << event;
    if (event->type() == QEvent::NativeGesture) {
        if (G::isLogger) G::log("VideoView::event", "QEvent::NativeGesture");
        // qDebug() << "VideoView::event NativeGesture";
        QNativeGestureEvent *e = static_cast<QNativeGestureEvent *>(event);
        int direction = static_cast<int>(e->value());
        emit mouseSideKeyPress(direction);
        return true;
    }
    QWidget::event(event);
    return true;
}

void VideoView::relayDrop(QString fPath)
{
    emit handleDrop(fPath);
}
