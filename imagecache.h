#ifndef IMAGECACHE_H
#define IMAGECACHE_H

#include <QtWidgets>
#include <QObject>
#include "global.h"
#include "metadata.h"
#include <algorithm>         // reqd to sort cache
#include <QMutex>
#include <QSize>
#include <QThread>
#include <QWaitCondition>
#include <QGradient>
#include "pixmap.h"

class ImageCache : public QThread
{
    Q_OBJECT

public:
    ImageCache(QObject *parent, Metadata *metadata);
    ~ImageCache();

    void initImageCache(QStringList &imageList, int &cacheSizeMB,
             bool &isShowCacheStatus, int &cacheStatusWidth, int &cacheWtAhead,
             bool &usePreview, int &previewWidth, int &previewHeight);
    void updateImageCache(QString  &fPath);
    void updateCacheStatusCurrentImagePosition(QString &fPath);
    void updateImageCacheParam(int &cacheSizeMB, bool &isShowCacheStatus,
             int &cacheStatusWidth, int &cacheWtAhead, bool &usePreview,
             int &previewWidth, int &previewHeight);
    void reindexImageCache(QStringList filterFilePathList,
             QString &currentImageFullPath);
    void stopImageCache();
    QSize getPreviewSize();

    QHash<QString, QImage> imCache;

signals:
    void showCacheStatus(QImage imCacheStatus);
    void updateIsRunning(bool);

protected:
    void run() Q_DECL_OVERRIDE;

public slots:

private:
    QMutex mutex;
    QWaitCondition condition;
    bool restart;
    bool abort;

    Metadata *metadata;
    Pixmap *pixmap;

    // image cache
    struct CacheItem {
        int key;
        int origKey;
        QString fName;
        bool isCached;
        bool isTarget;
        int priority;
        float sizeMB;
    } cacheItem;

    QList<CacheItem> cacheItemList, cacheItemListCopy;

    struct Cache {
        int key;                    // current image
        int prevKey;                // used to establish directionof travel
        QString dir;                // compare to input to see if different
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
        int pxTotWidth;             // width in pixels of graphic in statusbar
        float pxUnitWidth;          // width of one file on graphic in statusbar
        bool isShowCacheStatus;     // show in app status bar
        bool usePreview;            // cache smaller pixmap for speedier initial display
        QSize previewSize;          // monitor display dimensions for scale of previews
    } cache;

    QList<uint>toCache;
    QList<uint>toDecache;

    ulong getImCacheSize();         // add up total MB cached
    bool cacheUpToDate();           // target range all cached
    void setPriorities(int key);    // based on proximity to current position and wtAhead
    void setTargetRange();          // define start and end key in the target range to cache
    bool nextToCache();             // find highest priority not cached
    bool nextToDecache();           // find lowest priority cached - return -1 if none cached
    void checkForOrphans();         // check no strays in imageCache from jumping around
    static bool prioritySort(const CacheItem &p1, const CacheItem &p2);
    static bool keySort(const CacheItem &k1, const CacheItem &k2);
    void cacheStatus();             // update the cache status visual bar
    int pxMid(int key);             // center current position on statusbar
    int pxStart(int key);           // start current position on statusbar
    int pxEnd(int key);             // end current position on statusbar
    QSize scalePreview(ulong w, ulong h);
    void reportCache(QString title = "");
    void reportCacheProgress(QString action);
    void track(QString fPath, QString msg);
};

#endif // IMAGECACHE_H
