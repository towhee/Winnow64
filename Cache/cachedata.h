#ifndef CACHEDATA_H
#define CACHEDATA_H

#include <QtWidgets>
#include "Datamodel/HashMap.h"
#include "Datamodel/HashNode.h"

class ImageCacheData : public QObject
{
    Q_OBJECT
public:
    explicit ImageCacheData(QObject *);

    QMutex mutex;

    // concurrent image cache hash
    CTSL::HashMap<QString, QImage> imCache;

    // cache parameters in struct (used in ImageCache and MainWindow)
    struct Cache {
        int key;                    // current image
        int prevKey;                // used to establish direction of travel
        int toCacheKey;             // next file to cache
        int toDecacheKey;           // next file to remove from cache
        bool isForward;             // direction of travel for caching algorithm
        int step;                   // difference between key and prevKey
        int sumStep;                // sum of step until threshold
        int directionChangeThreshold;//number of steps before change direction of cache
        int wtAhead;                // ratio cache ahead vs behind * 10 (ie 7 = ratio 7/10)
        float currMB;               // the current MB consumed by the cache
        int maxMB;                  // maximum MB available to cache
        int minMB;                  // minimum MB available to cache
        int folderMB;               // MB required for all files in folder // rgh req'd?
        int targetFirst;            // beginning of target range to cache
        int targetLast;             // end of the target range to cache
        int decoderCount;           // number of separate threads used to decode images
        bool isShowCacheStatus;     // show in app status bar
    } cache;

    struct CacheItem {
        int key;                    // same as row in dm->sf (sorted and filtered datamodel)
        QString fPath;              // image full path
        QString ext;                // lower case file extension
        bool isUpdated;             // item updated by MetaRead or MDCache
        bool isCaching;             // decoder is working on image
        int attempts;               // number of tries to cache item
        int decoderId;              // decoder thread working on image
        bool isCached;              // has image been cached
        int status;                 // decoder return status
        bool isTarget;              // is this image targeted to be cached
        int priority;               // priority to cache image
        bool isVideo;               // videos are not cached
        float sizeMB;               // memory req'd to cache image
        bool estSizeMB;             // no meta for width/height, so sizeMB default used initially
        // below reqd for decoding to avoid using DataModel in another thread
        bool metadataLoaded;
        int orientation;
        int rotationDegrees;
        quint32 offsetFull;
        quint32 lengthFull;
        int samplesPerPixel;        // req'd for tiff
        QByteArray iccBuf;
        // messaging
        QString errMsg;
    } cacheItem;

    QList<CacheItem> cacheItemList;

    /*
      Key Priority   Target Attempts  Caching   Thread   Cached   SizeMB   File Name
        0        0        1        1        0        0        1       13   D:/Pictures/Calendar_Beach/20041205_0062.jpg
        1        1        1        1        0        1        1       13   D:/Pictures/Calendar_Beach/20041205_0065.jpg
        2        2        1        1        0        2        1       20   D:/Pictures/Calendar_Beach/2011-11-21_0073.jpg
        3        3        1        3        0        6        1       34   D:/Pictures/Calendar_Beach/2014-07-29_0002.jpg
      ...
    */
};
Q_DECLARE_METATYPE(ImageCacheData::Cache)

class IconCacheData : public QObject
{
    Q_OBJECT
public:
    explicit IconCacheData(QObject *);
    // list of dmRow that have icons
    QList<int>rowsWithIcon;
    int size();
public slots:
    void add(int dmRow);
    void remove(int dmRow);
};

#endif // CACHEDATA_H
