#ifndef VIDEOWIDGET_H
#define VIDEOWIDGET_H

#include <QVideoWidget>
#include <QMediaPlayer>
#include <QMediaDevices>
#include <QAudioDevice>
#include <QAudioOutput>
#include <QVideoSink>
#include <QVideoFrame>
#include <QMediaFormat>
#include "Main/global.h"

class VideoWidget: public QVideoWidget
{
    Q_OBJECT
public:
    explicit VideoWidget(QWidget *parent = nullptr);
    enum PlayState { Playing, Paused, Unavailable };
    void load(QString fPath);
    void play();
    void pause();
    void stop();
    PlayState playOrPause();
    int duration();
    void setPosition(int ms);
    QMediaPlayer *mediaPlayer = nullptr;

private:
    QAudioOutput *audioOutput = nullptr;

protected:
//    void wheelEvent(QWheelEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
//    void mouseMoveEvent(QMouseEvent *event) override;
//    void keyPressEvent(QKeyEvent *event) override;
};

#endif // VIDEOWIDGET_H
