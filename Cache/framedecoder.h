#ifndef FRAMEDECODER_H
#define FRAMEDECODER_H

//#include <QtWidgets>
#include <QObject>
//#include <QMutex>
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
    QVideoSink *videoSink;
    QString status;

signals:
    void setFrameIcon(QModelIndex dmIdx, QPixmap &pm, int instance, qint64 duration,
                      FrameDecoder *thisFrameDecoder);

public slots:
    void frameChanged(const QVideoFrame frame);
    void errorOccurred(QMediaPlayer::Error error, const QString &errorString);

private:
    FrameDecoder *thisFrameDecoder;
//    QMutex mutex;
//    QWaitCondition condition;
    QMediaPlayer *mediaPlayer;
//    QString fPath;
    int dmInstance;
    QModelIndex dmIdx;
    bool thumbnailAcquired = false;
    bool abort = false;
};


#endif // FRAMEDECODER_H
