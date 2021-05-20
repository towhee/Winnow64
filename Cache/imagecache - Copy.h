#ifndef IMAGECACHE_H
#define IMAGECACHE_H

#include <QtWidgets>
#include <QObject>
#include "Main/global.h"
#include "Datamodel/datamodel.h"
#include "Metadata/metadata.h"
#include <algorithm>         // reqd to sort cache
#include <QMutex>
#include <QSize>
#include <QThread>
#include <QWaitCondition>
#include <QGradient>
#include "Image/pixmap.h"

#ifdef Q_OS_WIN
#include "Utilities/win.h"
#include "Utilities/icc.h"
#endif

#ifdef Q_OS_MAC
#include "Utilities/mac.h"
#endif

class ImageCache : public QThread
{
    Q_OBJECT

public:
    ImageCache(QObject *parent, DataModel *dm, Metadata *metadata);
    ~ImageCache() override;

    void initImageCache(int &cacheSizeMB,
             bool &isShowCacheStatus, int &cacheWtAhead,
             bool &usePreview, int &previewWidth, int &previewHeight);
    void updateImageCacheParam(int &cacheSizeMB, bool &isShowCacheStatus,
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
    void reportToCache();
    QString reportCacheProgress(QString action);
    void reportRunStatus();
    QString reportImCache();

    QHash<QString, QImage> imCache;
    QString source;                 // temp for debugging

    // used by MW::updateImageCacheStatus
    struct Cache {
        int key;                    // current image
        int prevKey;                // used to establish directionof travel
        int toCacheKey;             // next file to cache
        int toDecacheKey;           // next file to remove from cache
        bool isForward;             // direction of travel for caching algorithm
        bool maybeDirectionChange;  // direction change but maybe below change threshold
        int step;                   // difference between key and prevKey
        int sumStep;                // sum of step until threshold
        int directionChangeThreshold;//number of steps before change direction of cache
        int wtAhead;                // ratio cache ahead vs behind * 10 (ie 7 = ratio 7/10)
        int totFiles;               // number of images available
        int currMB;                 // the current MB consumed by the cache
        int maxMB;                  // maximum MB available to cache
        int folderMB;               // MB required for all files in folder
        int targetFirst;            // beginning of target range to cache
        int targetLast;             // end of the target range to cache
        bool isShowCacheStatus;     // show in app status bar
        bool usePreview;            // cache smaller pixmap for speedier initial display
        QSize previewSize;          // monitor display dimensions for scale of previews
    } cache;

    // image cache
    struct CacheItem {
        int key;                    // same as row in dm->sf (sorted and filtered datamodel)
        int origKey;                // the key of a previous filter or sort
        QString fPath;              // image full path
        bool isMetadata;            // has metadata for embedded jpg offset and length been loaded
        bool isCached;              // has image been cached
        bool isTarget;              // is this image targeted to be cached
        int priority;               // priority to cache image
        int sizeMB;                 // memory req'd to cache image
    } cacheItem;

    QList<CacheItem> cacheItemList;

signals:
    void showCacheStatus(QString instruction, int key = 0, QString source = "");
    void updateIsRunning(bool, bool);
    void updateCacheOnThumbs(QString fPath, bool isCached);

protected:
    void run() Q_DECL_OVERRIDE;

public slots:
    void setCurrentPosition(QString path);
    void cacheSizeChange();         // flag when cache size is changed in preferences
    void colorManageChange();

private:
    QMutex mutex;
    QWaitCondition condition;
    bool restart;
    bool abort;
    bool pause;
    bool cacheSizeHasChanged;
    bool filterOrSortHasChanged;
    bool refreshCache;
    QString currentPath;
    QString prevCurrentPath;

    DataModel *dm;
    Metadata *metadata;
    Pixmap *getImage;

    QList<int>toCache;
    QList<int>toDecache;

    int getImCacheSize();           // add up total MB cached
    void setKeyToCurrent();         // cache key from currentFilePath
    void setDirection();            // caching direction
    void setPriorities(int key);    // based on proximity to current position and wtAhead
    void setTargetRange();          // define start and end key in the target range to cache
    bool nextToCache();             // find highest priority not cached
    bool nextToDecache();           // find lowest priority cached - return -1 if none cached
    void checkForOrphans();         // check no strays in imageCache from jumping around
    void makeRoom(int room, int roomRqd); // remove images from cache until there is roomRqd
    void memChk();                  // still room in system memory for cache?
    static bool prioritySort(const CacheItem &p1, const CacheItem &p2);
    static bool keySort(const CacheItem &k1, const CacheItem &k2);
    void buildImageCacheList();     //
    void updateImageCacheList();    //
    void refreshImageCache();
    QSize scalePreview(int w, int h);
};

#endif // IMAGECACHE_H
