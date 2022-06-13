#ifndef FRAMEDECODER_H
#define FRAMEDECODER_H

//#include <QtWidgets>
#include <QObject>
#include <QMutex>
#include <QThread>
#include <QWaitCondition>
#include "Main/global.h"
#include <QMediaPlayer>
#include <QVideoSink>
#include <QVideoFrame>

//class FrameDecoder : public QThread
//{
//    Q_OBJECT

//public:
//    FrameDecoder(QModelIndex dmIdx, int dmInstance, QObject *parent = nullptr);
//    void stop();
//    void getFrame(QString fPath);
//    QVideoSink *videoSink;

//protected:
//    void run() Q_DECL_OVERRIDE;

//signals:
//    void setFrameIcon(QModelIndex dmIdx, QPixmap &pm, int instance,
//                      FrameDecoder *thisFrameDecoder);

//public slots:
//    void frameChanged(const QVideoFrame frame);

//private:
//    FrameDecoder *thisFrameDecoder;
//    QMutex mutex;
//    QWaitCondition condition;
//    QMediaPlayer *mediaPlayer;
//    QString fPath;
//    int dmInstance;
//    QModelIndex dmIdx;
//    bool thumbnailAcquired = false;
//    bool abort = false;
//};

class FrameDecoder2 : public QObject
{
    Q_OBJECT

public:
    FrameDecoder2(QModelIndex dmIdx, int dmInstance);
    void getFrame(QString fPath);
    QVideoSink *videoSink;

signals:
    void setFrameIcon(QModelIndex dmIdx, QPixmap &pm, int instance,
                      FrameDecoder2 *thisFrameDecoder);

public slots:
    void frameChanged(const QVideoFrame frame);

private:
    FrameDecoder2 *thisFrameDecoder2;
    QMutex mutex;
    QWaitCondition condition;
    QMediaPlayer *mediaPlayer;
    QString fPath;
    int dmInstance;
    QModelIndex dmIdx;
    bool thumbnailAcquired = false;
    bool abort = false;
};


#endif // FRAMEDECODER_H
