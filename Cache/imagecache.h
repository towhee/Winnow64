#ifndef IMAGECACHE_H
#define IMAGECACHE_H

#include <QtWidgets>
#include <QObject>
#include "Main/global.h"
#include "Cache/cachedata.h"
#include "Datamodel/datamodel.h"
#include "Metadata/metadata.h"
#include "Image/pixmap.h"
//#include "Image/cacheimage.h"
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
    ImageCache(QObject *parent, ImageCacheData *icd, DataModel *dm/*, Metadata *metadata*/);
    ~ImageCache() override;

    void initImageCache(int &cacheSizeMB, int &cacheMinMB,
             bool &isShowCacheStatus, int &cacheWtAhead);
    void updateImageCacheParam(int &cacheSizeMB, int &cacheMinMB, bool &isShowCacheStatus,
             int &cacheWtAhead);
    void rebuildImageCacheParameters(QString &currentImageFullPath, QString source = "");
    void stop();
    void clearImageCache(bool includeList = true);
//    void pauseImageCache();
//    void resumeImageCache();
    bool cacheUpToDate();           // target range all cached
    bool isCached(int sfRow);
    void removeFromCache(QStringList &pathList);
//    QSize getPreviewSize();

    QString diagnostics();
    void updateStatus(QString instruction, QString source); // update cached send signal
    QString reportCacheParameters();
    QString reportCache(QString title = "");
    QString reportCacheProgress(QString action);
    void reportRunStatus();
    QString reportImCache();

    int decoderCount = 1;

    bool debugCaching = false;
    QString source;                 // temp for debugging

//    ImageCacheData::Cache icd->cache;

signals:
    void stopped(QString src);
    void setValue(QModelIndex dmIdx, QVariant value,
                  int role = Qt::EditRole, int align = Qt::AlignLeft);
    void setValueSf(QModelIndex sfIdx, QVariant value,
                    int role = Qt::EditRole, int align = Qt::AlignLeft);
    void setValuePath(QString fPath, int col, QVariant value, int role);
    void loadImage(QString fPath, QString src);
    void imageCachePrevCentralView();
    void showCacheStatus(QString instruction,
                         ImageCacheData::Cache cache,
                         QString source = "");
    void centralMsg(QString msg);
    void updateIsRunning(bool, bool);
    void updateCacheOnThumbs(QString fPath, bool isCached, QString src);
    void dummyDecoder(int id);

protected:
    void run() Q_DECL_OVERRIDE;

public slots:
    void addCacheItemImageMetadata(ImageMetadata m);
    void fillCache(int id);
    void setCurrentPosition(QString path, QString src);
    void cacheSizeChange();         // flag when cache size is changed in preferences
    void colorManageChange();

private:
    QMutex mutex;
    QWaitCondition condition;
    bool restart;
    bool abort;
    bool pause;
    bool cacheSizeHasChanged = false;
    bool filterOrSortHasChanged = false;
    bool refreshCache;
    QString currentPath;
    int maxAttemptsToCacheImage = 100;
    bool orphansFound;           // prevent multiple orphan checks as each decoder finishes
    bool isCacheUpToDate = false;
    bool sendStatusUpdates = false; // if true, slows scrolling while image cache loading

    ImageCacheData *icd;                // ptr to all cache data (reentrant)
    DataModel *dm;
    Metadata *metadata;
    QVector<ImageDecoder*> decoder;     // all the decoders
    QHash<QString,int> cacheKeyHash;    // cache key for any path
    QList<int> priorityList;

    void cacheImage(int id, int cacheKey);  // make room and add image to imageCache
    void decodeNextImage(int id);   // launch decoder for the next image in cacheItemList
    float getImCacheSize();         // add up total MB cached
    void setKeyToCurrent();         // cache key from currentFilePath
    void setDirection();            // caching direction
    void setPriorities(int key);    // based on proximity to current position and wtAhead
    void setTargetRange();          // define start and end key in the target range to cache
    bool nextToCache(int id);       // find highest priority not cached
    bool nextToDecache(int id);     // find lowest priority cached - return -1 if none cached
    void fixOrphans();              // outside target range with isCached == true
    void setSizeMB(int id, int cacheKey); // Update sizeMB if initially estimated ie PNG file
    void makeRoom(int id, int cacheKey); // remove images from cache until there is roomRqd
    void memChk();                  // still room in system memory for cache?
    int keyFromPath(QString path);
    static bool prioritySort(const ImageCacheData::CacheItem &p1,
                             const ImageCacheData::CacheItem &p2);
    static bool keySort(const ImageCacheData::CacheItem &k1,
                        const ImageCacheData::CacheItem &k2);
    void buildImageCacheList();     //
//    void updateImageCacheList();    //
    void refreshImageCache();
//    QSize scalePreview(int w, int h);

    QElapsedTimer t;
};

#endif // IMAGECACHE_H
