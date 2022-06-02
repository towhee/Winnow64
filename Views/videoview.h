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
    void stop();

signals:

private:
    QMediaPlayer *mediaPlayer = nullptr;
    QAudioOutput *audioOutput = nullptr;
};

#endif // VIDEOVIEW_H
