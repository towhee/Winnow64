#ifndef FRAMEDECODER_H
#define FRAMEDECODER_H

#include <QtWidgets>
#include <QObject>
#include <QMutex>
#include "Main/global.h"
#include <QMediaPlayer>
#include <QVideoSink>
#include <QVideoFrame>
#include "Main/global.h"

class FrameDecoder : public QObject
{
    Q_OBJECT

public:
    FrameDecoder(QObject *parent = nullptr);
    int id;
    QString fPath;
    QModelIndex dmIdx;

signals:
    void stopped(QString src);
    void setFrameIcon(QModelIndex dmIdx, QPixmap &pm, int instance, qint64 duration,
                      FrameDecoder *thisFrameDecoder);

public slots:
    void addToQueue(QString fPath, QModelIndex dmIdx = QModelIndex(), int dmInstance = 0);
    void clear();
    void stop();
    void frameChanged(const QVideoFrame frame);
    void errorOccurred(QMediaPlayer::Error error, const QString &errorString);

private:
    void getNextThumbNail(QString src = "");
    int queueIndex(QModelIndex dmIdx);
    enum Status {Idle, Busy} status;
    struct Item {
        QString fPath;
        QModelIndex dmIdx;
        int dmInstance;
    };
    QList<Item> queue;

    FrameDecoder *thisFrameDecoder;
    QMediaPlayer *mediaPlayer;
    QVideoSink *videoSink;
    int dmInstance;

    QMutex mutex;

    bool abort = false;
    bool reset =   false;
    bool isDebugging;
};


#endif // FRAMEDECODER_H
