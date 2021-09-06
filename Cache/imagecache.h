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
    void updateStatus(QString instruction, QString source); // update cached send signal
    QString reportCache(QString title = "");
    QString reportCacheProgress(QString action);
    void reportRunStatus();
    QString reportImCache();

    int decoderCount = 1;

    QString source;                 // temp for debugging

//    ImageCacheData::Cache icd->cache;

signals:
    void showCacheStatus(QString instruction,
                         ImageCacheData::Cache cache,
                         QString source = "");
    void updateIsRunning(bool, bool);
    void updateCacheOnThumbs(QString fPath, bool isCached);
    void dummyDecoder(int id);

protected:
    void run() Q_DECL_OVERRIDE;

public slots:
    void fillCache(int id);
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
    int maxAttemptsToCacheImage = 10000;

    ImageCacheData *icd;
    DataModel *dm;
    Metadata *metadata;
    QVector<ImageDecoder*> decoder;

    void cacheImage(int id, int cacheKey);  // make room and add image to imageCache
    void decodeNextImage(int id);   // launch decoder for the next image in cacheItemList
    int getImCacheSize();           // add up total MB cached
    void setKeyToCurrent();         // cache key from currentFilePath
    int getCacheKey(QString fPath); // cache key for any path
    void setDirection();            // caching direction
    void setPriorities(int key);    // based on proximity to current position and wtAhead
    void setTargetRange();          // define start and end key in the target range to cache
    bool inTargetRange(QString fPath);  // image targeted to cache
    bool nextToCache(int id);       // find highest priority not cached
    bool nextToDecache(int id);     // find lowest priority cached - return -1 if none cached
    bool cacheHasMissing();         // missed files from first pass (isCaching = true)
    void fixOrphans();              // outside target range with isCached == true
    void makeRoom(int id, int cacheKey); // remove images from cache until there is roomRqd
    void memChk();                  // still room in system memory for cache?
    static bool prioritySort(const ImageCacheData::CacheItem &p1,
                             const ImageCacheData::CacheItem &p2);
    static bool keySort(const ImageCacheData::CacheItem &k1,
                        const ImageCacheData::CacheItem &k2);
    void buildImageCacheList();     //
    void updateImageCacheList();    //
    void refreshImageCache();
    QSize scalePreview(int w, int h);

    QElapsedTimer t;
    bool debugCaching = false;
};

#endif // IMAGECACHE_H
