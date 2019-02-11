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
    MdCacher(QObject *parent, DataModel *dm, Metadata *metadata, MetaHash *metaHash);
    ~MdCacher() override;
    void loadMetadataCache(QVector<ThreadItem> &items,
                           bool isShowCacheStatus);
    void stopMetadateCache();
    bool restart;

//    int cacheStatusWidth;

protected:
    void run() Q_DECL_OVERRIDE;

signals:
    void setIcon(int, QImage);
    void processBuffer();
    void updateAllMetadataLoaded(int, bool);
    void finished();

    void refreshThumbs();
    void loadImageMetadata(QFileInfo);
    void loadImageCache();
//    void updateIsRunning(bool, bool, QString);
    void updateStatus(bool, QString);
    void showCacheStatus(int, bool);            // row, renew progress bar

private:
    QMutex mutex;
    QWaitCondition condition;
    volatile bool abort;

    DataModel *dm;
    Metadata *metadata;
    Thumb *thumb;
    QMap<int, bool> loadMap;
    int thread;

//    TSHash<int, ImageMetadata> *metaHash;
    MetaHash *metaHash;

    bool isShowCacheStatus;
    bool allMetadataLoaded;

    ThreadItem threadItem;

    QStringList fList;      // list of files to cache metadata
    QVector <ThreadItem> items;

    QModelIndex idx;
    int startRow;
    QSize thumbMax;         // rgh review hard coding thumb size
    QString err;            // type of error

//    QString folderPath;
    void createCacheStatus();
    void updateCacheStatus(int row);
    void loadMetadata();


    QElapsedTimer t;
};
#endif // MDCACHER_H
