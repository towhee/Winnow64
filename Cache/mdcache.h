

#ifndef MDCACHE_H
#define MDCACHE_H

#include <QtWidgets>
#include <QMutex>
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
        NewFolder2ndPass = 1,
        RefreshCurrent = 2,
        FilterChange = 3,       // if action >= FilterChange then iconCleanup is run
        SortChange = 4,
        NewFileSelected = 5,
        Scroll = 6,
        AllMetadata = 7
    };
    QStringList actionList;

    void loadNewFolder();
    void loadNewFolder2ndPass();
    void loadMetadataIconChunk(int row);
    void fileSelectionChange();
    void loadAllMetadata();
    void stopMetadateCache();
    bool isAllMetadataLoaded();
    bool isAllIconLoaded();
    void setRange();

    int metadataChunkSize;
    int defaultMetadataChunkSize = 250;

    // not being used at present
    int metadataCacheAllIfLessThan = 250;
    int iconPagesToCacheAhead = 2;
    int iconsCacheAllIfLessThan = 250;
    bool iconsCacheChunks = true;

    // Iconview state
    int firstIconVisible;
    int midIconVisible;
    int lastIconVisible;
    int thumbsPerPage;

    int prevFirstIconVisible;
    int prevLastIconVisible;

protected:
    void run() Q_DECL_OVERRIDE;

signals:
    void loadImageCache();
    void updateImageCachePositionAfterDelay();
    void pauseImageCache();
    void resumeImageCache();
    void updateIsRunning(bool, bool, QString);
    void updateIconBestFit();
    void loadMetadataCache2ndPass();
    void selectFirst();
    void scrollToCurrent();
    void showCacheStatus(int, bool);            // row, clear progress bar

private:
    QMutex mutex;
    QWaitCondition condition;
    bool abort;
    DataModel *dm;
    Metadata *metadata;
    ImageCache * imageCacheThread;
    Thumb *thumb;

    int startRow;
    int endRow;
    int tpp;                                    // thumbsPerPage;

    // icon caching
    QList<int> iconsCached;

    bool foundItemsToLoad;
    Action action;

    bool allIconsLoaded;
    bool isShowCacheStatus;
    bool cacheIcons;

    void createCacheStatus();
    void updateCacheStatus(int row);
    void readMetadataIconChunk();
    void readAllMetadata();
    void readIconChunk();
    void iconCleanup();
    void iconMax(QPixmap &thumb);

    QElapsedTimer t;
};

#endif // MDCACHE_H
