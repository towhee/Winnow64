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
    void frameImage(QString path, QImage image, QString source);

public slots:
    void stop();
    void clear();
    void addToQueue(QString fPath, int longSide, QString source = "",
                    QModelIndex dmIdx = QModelIndex(), int dmInstance = 0);
    void frameChanged(const QVideoFrame frame);
    //void test();
    void errorOccurred(QMediaPlayer::Error, const QString &errorString);

private:
    void getNextThumbNail(QString src = "");
    enum Status {Idle, Busy} status;
    struct Item {
        QString fPath;
        int longSide;
        QModelIndex dmIdx;
        int dmInstance;
        QString source;
    };
    QList<Item> queue;
    FrameDecoder *thisFrameDecoder;
    QMediaPlayer *mediaPlayer;
    QVideoSink *videoSink;

    bool abort = false;
    bool isDebugging;
};
#endif // FRAMEDECODER_H
