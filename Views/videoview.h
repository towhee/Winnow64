#ifndef VIDEOVIEW_H
#define VIDEOVIEW_H

//#include <QWidget>
//#include <QtWidgets>
//#include <QVideoWidget>
//#include <QMediaPlayer>
//#include <QMediaDevices>
//#include <QAudioDevice>
//#include <QAudioOutput>
//#include <QVideoSink>
//#include <QVideoFrame>
//#include <QMediaFormat>
//#include <QtWidgets>
//#include <QMediaMetaData>

#include "Views/videowidget.h"
#include "Main/global.h"

//class VideoView : public QVideoWidget
class VideoView : public QWidget
{
    Q_OBJECT
public:
    explicit VideoView(QWidget *parent = nullptr);
    void load(QString fPath);
    void play();
    void pause();
    void stop();

public slots:
    void durationChanged(qint64 duration);
    void positionChanged(qint64 progress);
    void scrubMoved(int ms);
    void scrubPressed();
    void playOrPause();

private:
    void updateDurationInfo(qint64 currentInfo);
    VideoWidget *video;
    QToolButton *playPauseBtn;
    QSlider *scrub;
    QLabel *position;
    qint64 duration;
};

#endif // VIDEOVIEW_H
