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
    ImageCache(QObject *parent, ImageCacheData *icd, DataModel *dm);
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
    void rename(QString oldPath, QString newPath);
    //    QSize getPreviewSize();

    QString getFPath(int i);

    QString diagnostics();
    void updateStatus(QString instruction, QString source); // update cached send signal
    QString reportCacheParameters();
    QString reportCacheDecoders();
    QString reportCacheItemList(QString title = "");
    QString reportCacheProgress(QString action);
    void reportRunStatus();
    QString reportImCache();

    int decoderCount = 1;

    bool debugCaching = false;
    bool debugLog = false;
    QString source;                 // temp for debugging

    //    ImageCacheData::Cache icd->cache;

signals:
    void stopped(QString src);
    void setValue(QModelIndex dmIdx, QVariant value,
                  int instance, QString scr = "ImageCache",
                  int role = Qt::EditRole, int align = Qt::AlignLeft);
    void setValueSf(QModelIndex sfIdx, QVariant value, int instance, QString src,
                    int role = Qt::EditRole, int align = Qt::AlignLeft); // not used
    void setValuePath(QString fPath, int col, QVariant value, int instance, int role);
    void loadImage(QString fPath, QString src);
    void imageCachePrevCentralView();
    void showCacheStatus(QString instruction,
                         ImageCacheData::Cache cache,
                         QString source = "");
    void centralMsg(QString msg);
    void updateIsRunning(bool, bool);   // (isRunning, showCacheLabel)
    void updateCacheOnThumbs(QString fPath, bool isCached, QString src);
    void dummyDecoder(int id);

protected:
    void run() Q_DECL_OVERRIDE;

public slots:
    void updateCacheItemMetadataFromReader(int row, int instance);
    // void updateImageMetadataFromReader(ImageMetadata m, int instance);
    void fillCache(int id);
    void setCurrentPosition(QString path, QString src);
    void datamodelFolderCountChange(QString src);
    void cacheSizeChange();         // flag when cache size is changed in preferences
    void colorManageChange();
    void refreshImageCache();
    void removeCachedImage(QString fPath); // remove image from imageCache and update status

private:
    QMutex gMutex;
    QWaitCondition condition;
    int instance;                   // incremented on every DataModel::load
    bool restart;
    bool abort;
    bool paused;
    bool isInitializing;
    bool cacheSizeHasChanged = false;
    bool filterOrSortHasChanged = false;
    QString currentPath;
    int maxAttemptsToCacheImage = 10;
    bool orphansFound;           // prevent multiple orphan checks as each decoder finishes
    bool isCacheUpToDate = false;
    bool isFinalCheckCompleted = false;
    int fileIsOpen = ImageDecoder::Status::FileOpen;
    int inValidImage = ImageDecoder::Status::Invalid;

    ImageCacheData *icd;                // ptr to all cache data (reentrant)
    DataModel *dm;
    Metadata *metadata;
    QVector<ImageDecoder*> decoder;     // all the decoders
    QHash<QString,int> keyFromPath;     // cache key for assigned path
    QHash<int,QString> pathFromKey;     // path
    QSet<int> toBeUpdated;
    // std::list<int> toBeUpdated;
    QList<int> priorityList;

    void launchDecoders(QString src);
    void cacheImage(int id, int cacheKey);  // make room and add image to imageCache
    void decodeNextImage(int id);   // launch decoder for the next image in cacheItemList
    float getImCacheSize();         // add up total MB cached
    bool cacheItemListCompleted();
    void updateTargets();
    void resetCacheStateInTargetRange();       // Set IsCaching = false within current target range
    bool allDecodersReady();        // All decoder status is ready
    void setKeyToCurrent();         // cache key from currentFilePath
    void setDirection();            // caching direction
    void setPriorities(int key);    // based on proximity to current position and wtAhead
    void setTargetRange();          // define start and end key in the target range to cache
    bool nextToCache(int id);       // find highest priority not cached
    //    bool nextToDecache(int id);     // find lowest priority cached - return -1 if none cached
    void setSizeMB(int id, int cacheKey); // Update sizeMB if initially estimated ie PNG file
    void memChk();                  // still room in system memory for cache?
    bool isValidKey(int key);
    // int keyFromPath(QString path);
    static bool prioritySort(const ImageCacheData::CacheItem &p1,
                             const ImageCacheData::CacheItem &p2);
    static bool keySort(const ImageCacheData::CacheItem &k1,
                        const ImageCacheData::CacheItem &k2);
    void buildImageCacheList();     //
    void addCacheItem(int key);
    bool updateCacheItemMetadata(int key);
    // bool updateCacheItemMetadata(ImageMetadata m);
    void log(const QString function, const QString comment = "");

    QElapsedTimer t;
};

#endif // IMAGECACHE_H
