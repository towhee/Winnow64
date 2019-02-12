#ifndef MDCACHER_H
#define MDCACHER_H

#include <QtWidgets>
#include <QMutex>
#include <QSize>
#include <QThread>
#include <QWaitCondition>
#include "Main/global.h"
#include "Datamodel/datamodel.h"
#include "Metadata/metadata.h"
#include "Image/thumb.h"
#include "Cache/threadsafehash.h"

struct ThreadItem{
    int row;
    QString fPath;
    int thread;
};

class MdCacher : public QThread
{
    Q_OBJECT

public:
    MdCacher(QObject *parent,
             DataModel *dm,
             Metadata *metadata,
             MetaHash *metaHash,
             ImageHash *iconHash);
    ~MdCacher() override;
    void loadMetadataCache(QVector<ThreadItem> &items,
                           bool isShowCacheStatus);
    void stopMetadateCache();

    // handy to determine source thread later in debugging
    int thread;

protected:
    void run() Q_DECL_OVERRIDE;

signals:
    void processMetadataBuffer();
    void processIconBuffer();
    void endCaching(int, bool);
    void showCacheStatus(int, bool);            // row, renew progress bar

    // not being used
    void finished();
    void setIcon(int, QImage);
    void refreshThumbs();
    void loadImageMetadata(QFileInfo);
    void loadImageCache();
//    void updateIsRunning(bool, bool, QString);
    void updateStatus(bool, QString);

private:
    QMutex mutex;
    QWaitCondition condition;
    volatile bool abort;

    DataModel *dm;
    Metadata *metadata;
    Thumb *thumb;
    QMap<int, bool> loadMap;

    MetaHash *metaHash;
    ImageHash *iconHash;

    bool isShowCacheStatus;
    bool allMetadataLoaded;

    // struct defined at top of header, also used in MdCacheMgr
    ThreadItem threadItem;
    // info for every file to be processed (row, path, thread#)
    QVector <ThreadItem> items;

    // new start position if metadata was not aquired for a file
    int startRow;

    QSize thumbMax;         // rgh review hard coding thumb size
    QString err;            // type of error
    int readFailure;
    QElapsedTimer t;

    void loadMetadata();
};
#endif // MDCACHER_H
