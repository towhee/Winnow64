

#ifndef MDCACHE_H
#define MDCACHE_H

#include <QtWidgets>
#include <QMutex>
#include <QSize>
#include <QThread>
#include <QWaitCondition>
#include "Datamodel/datamodel.h"
#include "Metadata/metadata.h"
#include "Cache/imagecache.h"
#include "Image/thumb.h"


class MetadataCache : public QThread
{
    Q_OBJECT

public:
    MetadataCache(QObject *parent, DataModel *dm,
                  Metadata *metadata, ImageCache *imageCacheThread);
    ~MetadataCache();
    void loadNewMetadataCache(int startRow, int thumbsPerPage/*, bool isShowCacheStatus*/);
    void loadMetadataCache(int startRow, int endRow);
    void loadAllMetadata();
    void stopMetadateCache();
    bool restart;
    int maxSegmentSize = 250;

    QMap<int, bool> loadMap;

protected:
    void run() Q_DECL_OVERRIDE;

signals:
    void setIcon(int, QImage);
    void loadImageCache();
    void pauseImageCache();
    void resumeImageCache();
    void updateIsRunning(bool, bool, QString);
    void updateAllMetadataLoaded(bool);
    void updateFilters();
    void showCacheStatus(int, bool);            // row, renew progress bar

//    void refreshThumbs();
//    void loadImageMetadata(QFileInfo);
//    void updateStatus(bool, QString);

private:
    QMutex mutex;
    QWaitCondition condition;
    bool abort;
    DataModel *dm;
    Metadata *metadata;
    ImageCache * imageCacheThread;
    Thumb *thumb;
    QString folderPath;

    QModelIndex idx;
    int startRow;
    int endRow;
    QSize thumbMax;         // rgh review hard coding thumb size
    QString err;            // type of error

    bool runImageCacheWhenDone;
    bool allMetadataLoaded;
    bool isShowCacheStatus;
    bool cacheIcons;

    void createCacheStatus();
    void updateCacheStatus(int row);
    bool loadMetadataChunk();
//    void track(QString fPath, QString msg);

//     bool isShowCacheStatus;

    QElapsedTimer t;
};

#endif // MDCACHE_H

