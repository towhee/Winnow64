#ifndef READER_H
#define READER_H

#include <QtWidgets>
#include <QMutex>
#include <QThread>
#include <QWaitCondition>
#include "Metadata/metadata.h"
#include "Metadata/imagemetadata.h"
#include "Datamodel/datamodel.h"
#include "Image/thumb.h"
#include "Cache/imagecache.h"

// #define TIMER    // uncomment to time execution

class Reader : public QObject
{
    Q_OBJECT
public:
    Reader(int id, DataModel *dm, ImageCache *imageCache);
    //~Reader() override;

    QThread *readerThread;  // use if currentThread() not working in stop()
    int threadId = -1;
    int instance = 0;
    bool isReadIcon = true;
    qint64 msToRead;
    QString fPath = "";
    QString errMsg = "";
    QModelIndex dmIdx = QModelIndex();
    Metadata *metadata;
    QPixmap pm;
    bool pending = false;
    bool loadedIcon = false;

    enum Status {
        Ready,
        Success,
        Aborted,
        MetaFailed,
        IconFailed,
        MetaIconFailed
    } status;
    QStringList statusText {
        "Ready",
        "Success",
        "Aborted",
        "MetaFailed",
        "IconFailed",
        "MetaIconFailed"
    };

signals:
    void addToDatamodel(ImageMetadata m, QString src);
    void setIcon(QModelIndex dmIdx, const QPixmap pm, bool ok, int fromInstance, QString src);
    void addToImageCache(int row, QString fPath, int instance);
    // void addToImageCache(ImageMetadata m, int instance);
    void done(int threadId);

public slots:
    void read(QModelIndex dmIdx, QString filePath, int instance, bool isReadIcon);
    void abortProcessing();
    void stop();

private:
    QMutex mutex;
    QWaitCondition condition;
    bool abort = false;
    bool readMetadata();
    void readIcon();
    inline bool instanceOk();
    DataModel *dm;
    ImageCache *imageCache;
    FrameDecoder *frameDecoder;
    Thumb *thumb;

    uint offsetThumb;
    uint lengthThumb;

    int metaReadCount;
    bool isVideo;
    bool isDebug;
    bool debugLog;
    int col0Width = 40;
    QElapsedTimer t;
    int t1, t2, t3, t4, t5, t6, t7;
};

#endif // READER_H
