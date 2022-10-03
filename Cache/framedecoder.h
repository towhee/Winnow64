#ifndef FRAMEDECODER_H
#define FRAMEDECODER_H

//#include <QtWidgets>
#include <QObject>
#include <QMutex>
//#include <QThread>
//#include <QWaitCondition>
#include "Main/global.h"
#include <QMediaPlayer>
#include <QVideoSink>
#include <QVideoFrame>
#include "Main/global.h"

class FrameDecoder : public QObject
{
    Q_OBJECT

public:
    FrameDecoder(QModelIndex dmIdx = QModelIndex(), int dmInstance = 0);
    void getFrame(QString fPath, QModelIndex dmIdx = QModelIndex(), int dmInstance = 0);
    void setStatus(QString status);
    QVideoSink *videoSink;
    QString status;
    QString fPath;
    QModelIndex dmIdx;

signals:
    void setFrameIcon(QModelIndex dmIdx, QPixmap &pm, int instance, qint64 duration,
                      FrameDecoder *thisFrameDecoder);
    void dispatch(int id);

public slots:
    void frameChanged(const QVideoFrame frame);
    void errorOccurred(QMediaPlayer::Error error, const QString &errorString);

private:
    FrameDecoder *thisFrameDecoder;
    QMutex mutex;
//    QWaitCondition condition;
    QMediaPlayer *mediaPlayer;
    int dmInstance;
    bool thumbnailAcquired = false;
    bool abort = false;

    bool isDebugging;
};


#endif // FRAMEDECODER_H
