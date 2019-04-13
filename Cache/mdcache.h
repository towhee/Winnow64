

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

    enum Action {
        NewFolder = 0,
        MetaChunk = 1,
        IconChunk = 2,
        MetaIconChunk = 3,
        AllMetadata = 4
    };

    void loadNewFolder(int thumbsPerPage/*, bool isShowCacheStatus*/);
    void loadMetadataIconChunk(int fromRow, int endRow);
    void loadAllMetadata();
    void loadIconChunk(int fromRow, int thumbsPerPage);
    void stopMetadateCache();
    bool isAllMetadataLoaded();
    bool isAllIconLoaded();

    bool restart;
    QMap<int, bool> loadMap;

    bool metadataCacheAll = false;
    bool iconsCacheAll = false;

    int metadataChunkSize = 250;
    int metadataCacheAllIfLessThan = 250;
    bool metadataCacheChunks = true;

    int iconPagesToCacheAhead = 2;
    int iconsCacheAllIfLessThan = 250;
    bool iconsCacheChunks = true;

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
    void updateIconBestFit();
    void selectFirst();
    void showCacheStatus(int, bool);            // row, clear progress bar

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
    int previousRow = -1;

    // icon caching
    int iconTargetStart;
    int iconTargetEnd;
    QList<int> iconsCached;

    bool foundItemsToLoad;
    Action action;
    QSize thumbMax;         // rgh review hard coding thumb size
    QString err;            // type of error

    bool allIconsLoaded;
    bool isShowCacheStatus;
    bool cacheIcons;

    void createCacheStatus();
    void updateCacheStatus(int row);
    void readMetadataIconChunk();
    void readAllMetadata();
    void readIconChunk();
    void iconCleanup();
    void setIconTargets(int start, int end);

//     bool isShowCacheStatus;

    QElapsedTimer t;
};

#endif // MDCACHE_H

