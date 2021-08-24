#ifndef IMAGECACHE_H
#define IMAGECACHE_H

#include <QtWidgets>
#include <QObject>
#include "Main/global.h"
#include "Cache/cachedata.h"
#include "Datamodel/datamodel.h"
#include "Metadata/metadata.h"
#include "Image/pixmap.h"
#include "Image/cacheimage.h"
#include "Cache/imagedecoder.h"
#include <algorithm>         // reqd to sort cache
#include <QMutex>
#include <QSize>
#include <QThread>
#include <QWaitCondition>
#include <QGradient>
#include <vector>

#ifdef Q_OS_WIN
#include "Utilities/win.h"
#endif
#include "Utilities/icc.h"

#ifdef Q_OS_MAC
#include "Utilities/mac.h"
#endif

class ImageCache : public QThread
{
    Q_OBJECT

public:
    ImageCache(QObject *parent, ImageCacheData *icd, DataModel *dm, Metadata *metadata);
    ~ImageCache() override;

    void initImageCache(int &cacheSizeMB, int &cacheMinMB,
             bool &isShowCacheStatus, int &cacheWtAhead,
             bool &usePreview, int &previewWidth, int &previewHeight);
    void updateImageCacheParam(int &cacheSizeMB, int &cacheMinMB, bool &isShowCacheStatus,
             int &cacheWtAhead, bool &usePreview, int &previewWidth, int &previewHeight);
    void rebuildImageCacheParameters(QString &currentImageFullPath, QString source = "");
    void stopImageCache();
    void clearImageCache(bool includeList = true);
    void pauseImageCache();
    void resumeImageCache();
    bool cacheUpToDate();           // target range all cached
    void removeFromCache(QStringList &pathList);
    QSize getPreviewSize();
    QString diagnostics();
    QString reportCache(QString title = "");
    QString reportCacheProgress(QString action);
    void reportRunStatus();
    QString reportImCache();

    int decoderCount = 1;

//    QHash<QString, QImage> imCache;  // moved to DataModel and changed to concurrent HashMap
    QString source;                 // temp for debugging

    // used by MW::updateImageCacheStatus
//    struct Cache {
//        int key;                    // current image
//        int prevKey;                // used to establish directionof travel
//        int toCacheKey;             // next file to cache
//        int toDecacheKey;           // next file to remove from cache
//        bool isForward;             // direction of travel for caching algorithm
//        bool maybeDirectionChange;  // direction change but maybe below change threshold
//        int step;                   // difference between key and prevKey
//        int sumStep;                // sum of step until threshold
//        int directionChangeThreshold;//number of steps before change direction of cache
//        int wtAhead;                // ratio cache ahead vs behind * 10 (ie 7 = ratio 7/10)
//        int totFiles;               // number of images available
//        int currMB;                 // the current MB consumed by the cache
//        int maxMB;                  // maximum MB available to cache
//        int minMB;                  // minimum MB available to cache
//        int folderMB;               // MB required for all files in folder
//        int targetFirst;            // beginning of target range to cache
//        int targetLast;             // end of the target range to cache
//        bool isShowCacheStatus;     // show in app status bar
//        bool usePreview;            // cache smaller pixmap for speedier initial display
//        QSize previewSize;          // monitor display dimensions for scale of previews
//    } cache;
//    DataModel::Cache *cache;
    DataModel::Cache cache;

    // image cache
    struct CacheItem {
        int key;                    // same as row in dm->sf (sorted and filtered datamodel)
        int origKey;                // the key of a previous filter or sort
        QString fPath;              // image full path
        bool isMetadata;            // has metadata for embedded jpg offset and length been loaded
        bool isCaching;             // decoder is working on image
        int threadId;               // decoder thread working on image
        bool isCached;              // has image been cached
        bool isTarget;              // is this image targeted to be cached
        int priority;               // priority to cache image
        int sizeMB;                 // memory req'd to cache image
    } cacheItem;

    QList<CacheItem> cacheItemList;

signals:
    void showCacheStatus(QString instruction, int key,
                         DataModel::Cache cache, QString source = "");
    void updateIsRunning(bool, bool);
    void updateCacheOnThumbs(QString fPath, bool isCached);
    void dummyDecoder(int id);

protected:
    void run() Q_DECL_OVERRIDE;

public slots:
//    void fillCache(int id, QString fPath, QImage *image);
    void fillCache(int id, QString fPath);
    void setCurrentPosition(QString path);
    void cacheSizeChange();         // flag when cache size is changed in preferences
    void colorManageChange();

private:
//    QBasicMutex mutex;
    QMutex mutex;
    QWaitCondition condition;
    bool restart;
    bool abort;
    bool pause;
    bool cacheSizeHasChanged;
    bool filterOrSortHasChanged;
    bool refreshCache;
    bool stopFillingCache;
    QString currentPath;
    QString prevCurrentPath;

    ImageCacheData *icd;
    DataModel *dm;
    Metadata *metadata;
    Pixmap *getImage;
    CacheImage *cacheImage;
    QVector<ImageDecoder*> decoder;

//    QList<int>toCache;
//    QList<int>toDecache;

    int getImCacheSize();           // add up total MB cached
    void setKeyToCurrent();         // cache key from currentFilePath
    int getCacheKey(QString fPath); // cache key for any path
    void setDirection();            // caching direction
    void setPriorities(int key);    // based on proximity to current position and wtAhead
    void setTargetRange();          // define start and end key in the target range to cache
    bool inTargetRange(QString fPath);  // image targeted to cache
    bool nextToCache();             // find highest priority not cached
    bool nextToDecache();           // find lowest priority cached - return -1 if none cached
    void checkForOrphans();         // check no strays in imageCache from jumping around
    void makeRoom(int cacheKey); // remove images from cache until there is roomRqd
    void memChk();                  // still room in system memory for cache?
    static bool prioritySort(const CacheItem &p1, const CacheItem &p2);
    static bool keySort(const CacheItem &k1, const CacheItem &k2);
    void buildImageCacheList();     //
    void updateImageCacheList();    //
    void refreshImageCache();
    QSize scalePreview(int w, int h);

    QElapsedTimer t;
};

#endif // IMAGECACHE_H
