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
    void setFrameIcon(int dmRow, QImage im, int instance, qint64 duration, FrameDecoder *thisFrameDecoder);
    void frameImage(QString path, QImage image, QString source);

public slots:
    void stop();
    void clear();
    void addToQueue(QString fPath, int longSide, QString source = "",
                    int dmRow = -1, int dmInstance = 0);

private:
    void getNextThumbNail(QString src = "");
    void handleFrameChanged(const QVideoFrame &frame);
    void cleanupPlayer();
    bool queueContains(const QString &fPath);

    enum Status { Idle, Busy } status;

    struct Item {
        QString fPath;
        int longSide;
        int dmRow;
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
