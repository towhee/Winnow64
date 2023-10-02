#ifndef FRAMEDECODER_H
#define FRAMEDECODER_H

#include <QtWidgets>
#include <QObject>
#include <QMutex>
//#include "Main/global.h"
#include <QMediaPlayer>
#include <QVideoSink>
#include <QVideoFrame>
#include "Datamodel/datamodel.h"

class FrameDecoder : public QObject
{
    Q_OBJECT

public:
    FrameDecoder(DataModel *dm, QObject *parent = nullptr);

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
//    bool isBusy();
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
    int dmInstance;

//    QMutex mutex;

    bool abort = false;
    bool reset =   false;
    bool isDebugging;

    DataModel *dm;


//    int id;
//    QString fPath;
//    QModelIndex dmIdx;
//    int frameChangedCount;
//    bool frameCaptured;

//signals:
//    void stopped(QString src);
//    void setFrameIcon(QModelIndex dmIdx, QPixmap &pm, int instance, qint64 duration);

//public slots:
//    void addToQueue(QString fPath, QModelIndex dmIdx = QModelIndex(), int dmInstance = 0);
//    void clear();
//    void stop();
//    void frameChanged(const QVideoFrame frame);
//    void stateChanged(QMediaPlayer::PlaybackState state);
//    void errorOccurred(QMediaPlayer::Error error, const QString &errorString);

//private:
//    void getNextThumbNail(QString src = "");
//    int queueIndex(QModelIndex dmIdx);
//    enum Status {Idle, Busy} status;
//    struct Item {
//        QString fPath;
//        QModelIndex dmIdx;
//        int frameChangedCount;
//        bool frameCaptured;
//        int dmInstance;
//    };
//    QList<Item> queue;

//    FrameDecoder *thisFrameDecoder;
//    QMediaPlayer *mediaPlayer;
//    QVideoSink *videoSink;
//    int dmInstance;
//    DataModel *dm;

//    QMutex mutex;
//    bool abort;             // version 1.33
//    bool reset = false;     // version 1.33
//    bool isDebugging;
};


#endif // FRAMEDECODER_H
