

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
        NewFolder,
        NewFolder2ndPass,
        NewFileSelected,
        Scroll,
        AllMetadata
    };
    QStringList actionList;

    void loadNewFolder();
    void loadNewFolder2ndPass();
    void scrollChange(int row);
    void fileSelectionChange(bool isRandomSlideShow);
    void loadAllMetadata();
    void readMetadataIcon(const QModelIndex &idx);
    void stopMetadateCache();
    bool isAllMetadataLoaded();
    bool isAllIconLoaded();
    void setRange();

    int metadataChunkSize = 250;
    int defaultMetadataChunkSize = 250;
    bool cacheAllMetadata = false;
    bool cacheAllIcons = false;

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
//    void scrollToCurrent();
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
    bool updateImageCache;
    Action action;

    bool allIconsLoaded;
    bool isShowCacheStatus;
    bool cacheIcons;

    void createCacheStatus();
    void updateCacheStatus(int row);
    void readMetadataIconChunk();
    void readAllMetadata();
    void readIconChunk();
    void readMetadataChunk();
    void iconCleanup();
    void iconMax(QPixmap &thumb);

    QElapsedTimer t;
};

#endif // MDCACHE_H
