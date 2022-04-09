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
                  Metadata *metadata);
    ~MetadataCache() override;

    enum Action {
        NewFolder,          // 0
        NewFolder2ndPass,   // 1
        NewFileSelected,    // 2
        Scroll,             // 3
        Resize,             // 4
        AllMetadata         // 5
    };
    QStringList actionList;

//    void loadNewFolder(bool isRefresh = false);
//    void loadNewFolder2ndPass();
//    void loadAllMetadata();
    void scrollChange(QString source);
    void sizeChange(QString source);
    void fileSelectionChange(/*bool okayToImageCache*/); // rghcachechange
    void readMetadataIcon(const QModelIndex &idx);
    void stopMetadataCache();
    bool isAllMetadataLoaded();
    bool isAllIconLoaded();
    void setRange();
    void iconMax(QPixmap &thumb);
    qint32 memRequired();

    int metadataChunkSize = 250;
    int defaultMetadataChunkSize = 250;
    bool cacheAllMetadata = false;
    bool cacheAllIcons = false;
    bool isRefreshFolder = false;

    // not being used at present
    int metadataCacheAllIfLessThan = 250;
    int iconPagesToCacheAhead = 2;
    int iconsCacheAllIfLessThan = 250;
    bool iconsCacheChunks = true;

    // Iconview state
    int firstIconVisible;
    int midIconVisible;
    int lastIconVisible;
    int visibleIcons;

    int prevFirstIconVisible;
    int prevLastIconVisible;

//private:
    QMutex mutex;
    QWaitCondition condition;
    bool abort;
    DataModel *dm;
    Metadata *metadata;
    Thumb *thumb;

    int startRow;
    int endRow;
    int tpp;                                    // thumbsPerPage;
    int countInterval = 100;                    // report progress interval

    // icon caching
    QList<int> iconsCached;

    bool foundItemsToLoad;
    Action action;

    bool allIconsLoaded;
    bool cacheIcons;

    int metadataTry = 3;
    int iconTry = 3;


    void createCacheStatus();
    void updateCacheStatus(int row);
    void readAllMetadata();
    void readIconChunk();
    void readMetadataChunk();
    void iconCleanup();
    bool anyItemsToLoad();

    QElapsedTimer t;

protected:
    void run() Q_DECL_OVERRIDE;

signals:
    void loadImageCache();
    void updateIsRunning(bool/*isRunning*/, bool/*showCacheLabel*/, QString/*calledBy*/);
    void updateIconBestFit();
    void loadMetadataCache2ndPass();
    void selectFirst();
    void showCacheStatus(QString);            // row, clear progress bar
    void finished2ndPass();                   // buildFilters

};

#endif // MDCACHE_H
