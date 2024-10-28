#ifndef READER_H
#define READER_H

#include <QtWidgets>
#include <QMutex>
#include <QThread>
#include <QWaitCondition>
#include "Main/global.h"
#include "Metadata/metadata.h"
#include "Metadata/imagemetadata.h"
#include "Datamodel/datamodel.h"
#include "Image/thumb.h"
#include "Cache/imagecache.h"

class Reader : public QThread
{
    Q_OBJECT
public:
    Reader(QObject *parent,
                 int id,
                 DataModel *dm,
                 ImageCache *imageCache);
    //~Reader() override;

    void read(const QModelIndex dmIdx,
                const QString fPath,
                const int instance,
                const bool isReadIcon);
    void stop();

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

protected:
    void run() Q_DECL_OVERRIDE;

signals:
    void addToDatamodel(ImageMetadata m, QString src);
    void setIcon(QModelIndex dmIdx, const QPixmap pm, int fromInstance, QString src);
    void addToImageCache(int row, QString fPath, int instance);
    // void addToImageCache(ImageMetadata m, int instance);
    void done(int threadId);

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
    int metaReadCount;
    bool isVideo;
    bool isDebug;
    QElapsedTimer t;
    int t1, t2, t3, t4, t5, t6, t7;
};

#endif // READER_H
