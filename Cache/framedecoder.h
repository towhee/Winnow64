#ifndef FRAMEDECODER_H
#define FRAMEDECODER_H

#include <QObject>
#include <QPointer>
#include <QMediaPlayer>
#include <QVideoSink>
#include <QVideoFrame>
#include <QPixmap>
#include <QImage>
#include <QModelIndex>
#include <QList>

class FrameDecoder : public QObject
{
    Q_OBJECT

public:
    FrameDecoder();
    ~FrameDecoder() override;

signals:
    void setFrameIcon(QModelIndex dmIdx, QPixmap pm, int instance, qint64 duration, FrameDecoder *thisFrameDecoder);
    void frameImage(QString path, QImage image, QString source);

public slots:
    void stop();
    void clear();
    void addToQueue(QString fPath, int longSide, QString source = "",
                    QModelIndex dmIdx = QModelIndex(), int dmInstance = 0);

private:
    void getNextThumbNail(QString src = "");
    void handleFrameChanged(const QVideoFrame &frame);
    void cleanupPlayer();
    bool queueContains(const QString &fPath);

    enum Status { Idle, Busy } status;

    struct Item {
        QString fPath;
        int longSide;
        QModelIndex dmIdx;
        int dmInstance;
        QString source;
    };

    QList<Item> queue;

    QPointer<QMediaPlayer> mediaPlayer;
    QPointer<QVideoSink> videoSink;
    int playerGeneration = 0;

    bool abort = false;
    QString prevPath;
    bool frameAlreadyDone = false;
    bool isDebugging = false;
};

#endif // FRAMEDECODER_H

// Before use ChatGPT changes (above)
// #ifndef FRAMEDECODER_H
// #define FRAMEDECODER_H

// #include <QtWidgets>
// #include <QObject>
// #include <QMutex>
// #include <QMediaPlayer>
// #include <QVideoSink>
// #include <QVideoFrame>

// class FrameDecoder : public QObject
// {
//     Q_OBJECT

// public:
//     FrameDecoder();
//     ~FrameDecoder() override;
//     QThread *frameDecoderThread;  // not being used

// signals:
//     void setFrameIcon(QModelIndex dmIdx, QPixmap pm, int instance, qint64 duration,
//                       FrameDecoder *thisFrameDecoder);
//     void frameImage(QString path, QImage image, QString source);

// public slots:
//     void stop();
//     void clear();
//     void addToQueue(QString fPath, int longSide, QString source = "",
//                     QModelIndex dmIdx = QModelIndex(), int dmInstance = 0);
//     void frameChanged(const QVideoFrame frame);
//     //void test();
//     void errorOccurred(QMediaPlayer::Error, const QString &errorString);

// private:
//     void getNextThumbNail(QString src = "");
//     enum Status {Idle, Busy} status;
//     struct Item {
//         QString fPath;
//         int longSide;
//         QModelIndex dmIdx;
//         int dmInstance;
//         QString source;
//     };
//     QList<Item> queue;
//     FrameDecoder *thisFrameDecoder;
//     QMediaPlayer *mediaPlayer;
//     QVideoSink *videoSink;
//     QString prevPath = "";
//     bool frameAlreadyDone;

//     bool abort = false;
//     bool isDebugging;
// };
// #endif // FRAMEDECODER_H
