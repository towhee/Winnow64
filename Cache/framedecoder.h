#ifndef FRAMEDECODER_H
#define FRAMEDECODER_H

#include <QtWidgets>
#include <QObject>
#include <QMutex>
#include "Main/global.h"
#include <QMediaPlayer>
#include <QVideoSink>
#include <QVideoFrame>

class FrameDecoder : public QObject
{
    Q_OBJECT

public:
    FrameDecoder(QObject *parent = nullptr);

signals:
    void stopped(QString src);
    void setFrameIcon(QModelIndex dmIdx, QPixmap &pm, int instance, qint64 duration,
                      FrameDecoder *thisFrameDecoder);

public slots:
    void addToQueue(QString fPath, QModelIndex dmIdx = QModelIndex(), int dmInstance = 0);
    void clear();
    void stop();
    void frameChanged(const QVideoFrame frame);
    void errorOccurred(QMediaPlayer::Error, const QString &errorString);

private:
    void getNextThumbNail(QString src = "");
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

    bool abort = false;
    bool isDebugging;
};
#endif // FRAMEDECODER_H
