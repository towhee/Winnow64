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
#include <QMediaMetaData>
#include "Main/global.h"

class VideoWidget: public QVideoWidget
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

signals:
    void togglePlayOrPause();
    void handleDrop(QString fPath);

protected:
    void mousePressEvent(QMouseEvent *event) override;
    bool eventFilter(QObject *obj, QEvent *event) override;
//    void dropEvent(QDropEvent *event) override;
//    void dragEnterEvent(QDragEnterEvent *event) override;
};

#endif // VIDEOWIDGET_H
