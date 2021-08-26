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
    void updateStatus(QString instruction, QString source); // update cached send signal
    QString reportCache(QString title = "");
    QString reportCacheProgress(QString action);
    void reportRunStatus();
    QString reportImCache();

    int decoderCount = 1;

//    QHash<QString, QImage> imCache;  // moved to DataModel and changed to concurrent HashMap
    QString source;                 // temp for debugging

//    ImageCacheData::Cache icd->cache;

signals:
    void showCacheStatus(QString instruction,
                         ImageCacheData::Cache cache,
//                         QVector<bool> cached,
                         QString source = "");
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

    int getImCacheSize();           // add up total MB cached
    void setKeyToCurrent();         // cache key from currentFilePath
    int getCacheKey(QString fPath); // cache key for any path
    void setDirection();            // caching direction
    void setPriorities(int key);    // based on proximity to current position and wtAhead
    void setTargetRange();          // define start and end key in the target range to cache
    bool inTargetRange(QString fPath);  // image targeted to cache
    bool nextToCache();             // find highest priority not cached
    bool nextToDecache();           // find lowest priority cached - return -1 if none cached
    bool cacheHasMissing();         // missed files from first pass (isCaching = true)
    void checkForOrphans();         // check no strays in imageCache from jumping around
    void makeRoom(int cacheKey); // remove images from cache until there is roomRqd
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
};

#endif // IMAGECACHE_H
