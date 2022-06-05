#ifndef VIDEOVIEW_H
#define VIDEOVIEW_H

//#include <QObject>
#include <QWidget>
#include <QtWidgets>
#include <QVideoWidget>
#include <QMediaPlayer>
#include <QMediaDevices>
#include <QAudioDevice>
#include <QAudioOutput>
#include <QVideoSink>
#include <QVideoFrame>
#include <QMediaFormat>
#include <QtWidgets>
#include <QMediaMetaData>
#include "Main/global.h"

class VideoView : public QVideoWidget
{
    Q_OBJECT
public:
    explicit VideoView(QWidget *parent = nullptr);
    void load(QString fPath);
    void play();
    void pause();
    void stop();
private:
    QMediaPlayer *mediaPlayer = nullptr;
    QAudioOutput *audioOutput = nullptr;
protected:
//    void wheelEvent(QWheelEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
//    void mouseMoveEvent(QMouseEvent *event) override;
//    void keyPressEvent(QKeyEvent *event) override;
};

#endif // VIDEOVIEW_H
