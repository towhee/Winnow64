
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
#include <QElapsedTimer>
#include <QString>
#include <QVariant>

class FrameDecoder : public QObject
{
    Q_OBJECT

public:
    FrameDecoder();
    ~FrameDecoder() override;

signals:
    void setFrameIcon(int dmRow, QImage im, int instance, qint64 duration, FrameDecoder *thisFrameDecoder);
    void frameImage(QString path, QImage image, QString source);
    /* Emitted when a queued dmThumb job ends without a usable frame
       (invalid media, playback error, invalid frame after retries, null image,
       exception). Lets DataModel clear MetadataReadingColumn so MetaRead's
       dispatcher can move on. */
    void videoFrameFailed(int dmRow, int dmInstance);
    /* Mirrors Reader::setValDm so video thumb decode time can be recorded
       in G::NSThumb the same way image thumb decode time is. */
    void setValDm(int dmRow, int dmCol, QVariant value, int instance, QString src = "FrameDecoder",
                  int role = Qt::EditRole, int align = Qt::AlignLeft);

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

    // Retry counter for invalid-frame loops in handleFrameChanged. Was a
    // function-local static, which made it shared across all FrameDecoder
    // instances (one per Reader thread) and caused a TSan-flagged race.
    int frameAttempts = 0;

    // Measures total decode time for the current item (started in
    // getNextThumbNail, read in handleFrameChanged). Retries via
    // queue.prepend keep accumulating — getNextThumbNail is not re-entered
    // until the item resolves, so a single timer per item is correct.
    QElapsedTimer tFrame;

    QPointer<QMediaPlayer> mediaPlayer;
    QPointer<QVideoSink> videoSink;
    int playerGeneration = 0;

    bool abort = false;
    QString prevPath;
    bool frameAlreadyDone = false;
    bool isDebugging = false;
};

#endif // FRAMEDECODER_H
