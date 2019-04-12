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

class ImageCache : public QThread
{
    Q_OBJECT

public:
    ImageCache(QObject *parent, DataModel *dm, Metadata *metadata);
    ~ImageCache();

    void initImageCache(int &cacheSizeMB,
             bool &isShowCacheStatus, int &cacheWtAhead,
             bool &usePreview, int &previewWidth, int &previewHeight);
    void updateImageCachePosition(QString  &fPath);
    void updateImageCacheParam(int &cacheSizeMB, bool &isShowCacheStatus, int &cacheWtAhead, bool &usePreview, int &previewWidth, int &previewHeight);
    void resortImageCache(QString &currentImageFullPath);
    void filterImageCache(QString &currentImageFullPath);
    void stopImageCache();
    void clearImageCache();
    void pauseImageCache();
    void resumeImageCache();
    bool cacheUpToDate();           // target range all cached
    QSize getPreviewSize();
    void reportCache(QString title = "");

    QHash<QString, QImage> imCache;

    // used by MW::updateImageCacheStatus
    struct Cache {
        int key;                    // current image
        int prevKey;                // used to establish directionof travel
        uint toCacheKey;            // next file to cache
        uint toDecacheKey;          // next file to remove from cache
        bool isForward;             // direction of travel in folder
        float wtAhead;              // ratio cache ahead vs behind
        int totFiles;               // number of images available
        uint currMB;                // the current MB consumed by the cache
        uint maxMB;                 // maximum MB available to cache
        uint folderMB;              // MB required for all files in folder
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
        QString fName;              // image full path
        bool isMetadata;            // has metadata for embedded jpg offset and length been loaded
        bool isCached;              // has image been cached
        bool isTarget;              // is this image targeted to be cached
        int priority;               // priority to cache image
        float sizeMB;               // memory req'd to cache iamge
    } cacheItem;

    QList<CacheItem> cacheItemList, cacheItemListCopy;

//    int pxMid(int key);             // center current position on statusbar
//    int pxStart(int key);           // start current position on statusbar
//    int pxEnd(int key);             // end current position on statusbar

signals:
    void showCacheStatus(QString instruction, int key = 0, QString source = "");
//    void showCacheStatus(QImage imCacheStatus);
    void updateIsRunning(bool, bool);
    void updateCacheOnThumbs(QString fPath, bool isCached);

protected:
    void run() Q_DECL_OVERRIDE;

public slots:

private:
    QMutex mutex;
    QWaitCondition condition;
    bool restart;
    bool abort;

    DataModel *dm;
    Metadata *metadata;
    Pixmap *getImage;

    QList<uint>toCache;
    QList<uint>toDecache;

    ulong getImCacheSize();         // add up total MB cached
    void setPriorities(int key);    // based on proximity to current position and wtAhead
    void setTargetRange();          // define start and end key in the target range to cache
    bool nextToCache();             // find highest priority not cached
    bool nextToDecache();           // find lowest priority cached - return -1 if none cached
    void checkForOrphans();         // check no strays in imageCache from jumping around
    void checkAlreadyCached();      // after filtration check if image targeted and already cached
    void checkForSurplus();         // after filtration get rid of cached images not needed
    static bool prioritySort(const CacheItem &p1, const CacheItem &p2);
    static bool keySort(const CacheItem &k1, const CacheItem &k2);
    void buildImageCacheList(); //
    void updateImageCacheList(); //
    QSize scalePreview(ulong w, ulong h);
    void reportCacheProgress(QString action);
};

#endif // IMAGECACHE_H
