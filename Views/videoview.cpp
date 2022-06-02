#include "videoview.h"

VideoView::VideoView(QWidget *parent)
    : QVideoWidget{parent}
{
    if (G::isLogger) G::log(__FUNCTION__);
    mediaPlayer = new QMediaPlayer(this);
    audioOutput = new QAudioOutput(this);
    mediaPlayer->setAudioOutput(audioOutput);
    mediaPlayer->setVideoOutput(this);

//    setStyleSheet("QWidget {background-color:green;}");
//    setStyleSheet("QVideoWidget {background-color:green;}");
//    setStyleSheet("QMediaPlayer {background-color:yellow;}");
//    QPalette p = palette();
//    p.setColor(QPalette::Window, Qt::blue);
////    p.setColor(QPalette::Window, G::backgroundColor);
//    setPalette(p);

}

void VideoView::load(QString fPath)
{
    if (G::isLogger) G::log(__FUNCTION__);
    mediaPlayer->setSource(fPath);
    mediaPlayer->play();
}

void VideoView::stop()
{
    if (G::isLogger) G::log(__FUNCTION__);
    mediaPlayer->stop();
}
