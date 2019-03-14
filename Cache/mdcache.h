

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

    enum CacheImages {
        No = 0,
        New = 1,
        Update = 2,
        Resume = 3
    };

    void loadNewMetadataCache(int thumbsPerPage/*, bool isShowCacheStatus*/);
    void loadMetadataCache(int startRow, int endRow, CacheImages cacheImages);
    void loadAllMetadata();
    void stopMetadateCache();
    bool isAllMetadataLoaded();
    bool isAllIconsLoaded();

    bool restart;
    int maxChunkSize = 250;
    QMap<int, bool> loadMap;

    int recacheIfLessThan;
    int recacheIfGreaterThan;

protected:
    void run() Q_DECL_OVERRIDE;

signals:
    void setIcon(int, QImage);
    void loadImageCache();
    void pauseImageCache();
    void resumeImageCache();
    void updateIsRunning(bool, bool, QString);
    void updateAllMetadataLoaded(bool);
    void updateIconBestFit();
    void updateFilters();
    void showCacheStatus(int, bool);            // row, renew progress bar

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
    int row;

    // icon caching
    int iconTargetStart;
    int iconTargetEnd;
    QList<int> iconsCached;

    bool foundItemsToLoad;
    bool imageCacheWasRunning;
    CacheImages cacheImages;
    QSize thumbMax;         // rgh review hard coding thumb size
    QString err;            // type of error

    bool allMetadataLoaded;
    bool allIconsLoaded;
    bool isShowCacheStatus;
    bool cacheIcons;

    void createCacheStatus();
    void updateCacheStatus(int row);
    bool loadMetadataIconChunk();
    void iconCleanup();
    void setIconTargets(int start, int end);

//     bool isShowCacheStatus;

    QElapsedTimer t;
};

#endif // MDCACHE_H

