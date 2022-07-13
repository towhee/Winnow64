#ifndef MDCACHE_H
#define MDCACHE_H

#include <QtWidgets>
#include <QMutex>
#include <QThread>
#include <QWaitCondition>
#include "Datamodel/datamodel.h"
#include "Metadata/metadata.h"
//#include "Cache/imagecache.h"
#include "Image/thumb.h"
#include "Main/global.h"


class MetadataCache : public QThread
{
    Q_OBJECT

public:
    MetadataCache(QObject *parent, DataModel *dm, Metadata *metadata);
    ~MetadataCache() override;

    enum Action {
        NewFolder,          // 0
        NewFolder2ndPass,   // 1
        NewFileSelected,    // 2
        Scroll,             // 3
        Resize,             // 4
        AllMetadata,        // 5
        MR_Read             // 6
    };
    QStringList actionList;

//    void loadNewFolder(bool isRefresh = false);
//    void loadNewFolder2ndPass();
    void mr_read(int sfRow = 0, QString source = "");
    void scrollChange(QString source);
    void sizeChange(QString source);
    void fileSelectionChange(/*bool okayToImageCache*/); // rghcachechange
    void readMetadataIcon(const QModelIndex &idx);
    void stop();
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

    bool loadIcon(int sfRow);
    void updateIconLoadingProgress(int count, int end);
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
    void setIcon(QModelIndex dmIdx, QPixmap &pm, int instance);
    void loadImageCache();
    void updateIsRunning(bool/*isRunning*/, bool/*showCacheLabel*/, QString/*calledBy*/);
    void updateIconBestFit();
    void loadMetadataCache2ndPass();
    void selectFirst();
    void showCacheStatus(QString);            // row, clear progress bar
    void finished2ndPass();                   // buildFilters

// MetaRead stuff
public:
    void initialize();
    void dmRowRemoved(int dmRow);
    int iconChunkSize;
    int firstIconRow;
    int lastIconRow;
private:
    void read();
    void readRow(int row);
    void readMetadata(QModelIndex sfIdx, QString fPath);
    void readIcon(QModelIndex sfIdx, QString fPath);
//    void mr_iconMax(QPixmap &thumb);
    void cleanupIcons();
    void buildMetadataPriorityQueue(int sfRow);
    bool isNotLoaded(int sfRow);
    bool okToLoadIcon(int sfRow);
    int dmInstance;
    int adjIconChunkSize;
    int sfRowCount;
    int visibleIconCount;
    int sfStart;
    int sfRow;
    bool imageCachingStarted = false;
    QList<int> priorityQueue;
    QList<int> iconsLoaded;
//    QList<int> mr_visibleIcons;
    QList<int> outsideIconRange;
    bool debugCaching = false;

signals:
    void addToDatamodel(ImageMetadata m);
    void addToImageCache(ImageMetadata m);
    void done();
    void delayedStartImageCache();
    void runStatus(bool/*isRunning*/, bool/*showCacheLabel*/, QString/*calledBy*/);
    void setImageCachePosition(QString fPath);      // not used

};

#endif // MDCACHE_H
