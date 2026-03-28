#ifndef VIDEOWIDGET_H
#define VIDEOWIDGET_H

#include <QWidget>
#include <QMediaPlayer>
#include <QAudioOutput>
#include <QVideoSink>
#include <QVideoFrame>
#include <QPainter>
#include <QMediaMetaData>
#include "Main/global.h"

class VideoWidget: public QWidget
{
    Q_OBJECT
public:
    explicit VideoWidget(QWidget *parent);
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
    QVideoFrame currentFrame;
    bool isDebug;

signals:
    void togglePlayOrPause();
    void handleDrop(QString fPath);

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void paintEvent(QPaintEvent *event) override;
};

#endif // VIDEOWIDGET_H


// #include <QVideoWidget>
// #include <QMediaPlayer>
// #include <QMediaDevices>
// #include <QAudioDevice>
// #include <QAudioOutput>
// #include <QVideoSink>
// #include <QVideoFrame>
// #include <QMediaFormat>
// #include <QMediaMetaData>
// #include <QVideoFrame>
// #include <QPainter>
// #include "Main/global.h"

// class VideoWidget: public QWidget
// {
//     Q_OBJECT
// public:
//     explicit VideoWidget(QWidget *parent);
//     enum PlayState { Playing, Paused, Unavailable };
//     void load(QString fPath);
//     void play();
//     void pause();
//     void stop();
//     PlayState playOrPause();
//     int duration();
//     void setPosition(int ms);
//     void firstFrame(QPixmap &pm);
//     QMediaPlayer *mediaPlayer = nullptr;

// private:
//     QVideoFrame currentFrame; // To store the frame for paintEvent
//     QAudioOutput *audioOutput = nullptr;
//     bool isDebug;

// signals:
//     void togglePlayOrPause();
//     void handleDrop(QString fPath);

// protected:
//     void mousePressEvent(QMouseEvent *event) override;
//     void mouseMoveEvent(QMouseEvent *event) override;
//     bool eventFilter(QObject *obj, QEvent *event) override;
//     void paintEvent(QPaintEvent *event) override; // Manual rotation logic
// //    void dropEvent(QDropEvent *event) override;
// //    void dragEnterEvent(QDragEnterEvent *event) override;
// };

// #endif // VIDEOWIDGET_H
