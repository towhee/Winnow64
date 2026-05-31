#ifndef VIDEOWIDGET_H
#define VIDEOWIDGET_H

#include <QMediaPlayer>
#include <QAudioOutput>
#include "Main/global.h"

/*
    Platform split:

    Windows uses Qt's native QVideoWidget. The multimedia backend applies the
    video display-matrix rotation automatically, so portrait phone clips play
    upright (this is how it worked through v1.03).

    macOS uses a manual QVideoSink + paintEvent. QVideoWidget had an orientation
    bug on macOS (see the "Fix video orientation MacOS bug" commit), so frames
    are grabbed and rotated/painted by hand using QVideoFrame::rotation().
    That frame rotation is not populated on the Windows backend, which is why
    the manual path can't be shared.
*/

#ifdef Q_OS_WIN
#include <QVideoWidget>
#else
#include <QWidget>
#include <QVideoSink>
#include <QVideoFrame>
#include <QPainter>
#include <QMediaMetaData>
#endif

#ifdef Q_OS_WIN
class VideoWidget: public QVideoWidget
#else
class VideoWidget: public QWidget
#endif
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
#ifndef Q_OS_WIN
    QVideoFrame currentFrame;
#endif
    bool isDebug;

signals:
    void togglePlayOrPause();
    void handleDrop(QString fPath);

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
#ifdef Q_OS_WIN
    bool eventFilter(QObject *obj, QEvent *event) override;
#else
    void paintEvent(QPaintEvent *event) override;
#endif
};

#endif // VIDEOWIDGET_H
