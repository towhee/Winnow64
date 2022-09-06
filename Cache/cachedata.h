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

    // concurrent image cache hash
    CTSL::HashMap<QString, QImage> imCache;

    // cache parameters in struct (used in ImageCache and MainWindow)
    struct Cache {
        int key;                    // current image
        int prevKey;                // used to establish direction of travel
        int toCacheKey;             // next file to cache
        int toDecacheKey;           // next file to remove from cache
        bool isForward;             // direction of travel for caching algorithm
        bool maybeDirectionChange;  // direction change but maybe below change threshold
        int step;                   // difference between key and prevKey
        int sumStep;                // sum of step until threshold
        int directionChangeThreshold;//number of steps before change direction of cache
        int wtAhead;                // ratio cache ahead vs behind * 10 (ie 7 = ratio 7/10)
        int totFiles;               // number of images available
        float currMB;               // the current MB consumed by the cache
        int maxMB;                  // maximum MB available to cache
        int minMB;                  // minimum MB available to cache
        int folderMB;               // MB required for all files in folder // rgh req'd?
        int targetFirst;            // beginning of target range to cache
        int targetLast;             // end of the target range to cache
        int decoderCount;           // number of separate threads used to decode images
        int dmInstance;             // new instance of data model when new folder
        bool isShowCacheStatus;     // show in app status bar
    } cache;

    struct CacheItem {
        int key;                    // same as row in dm->sf (sorted and filtered datamodel)
        int origKey;                // the key of a previous filter or sort
        QString fPath;              // image full path
//        bool isMetadata;            // has metadata for embedded jpg offset and length been loaded
        bool isCaching;             // decoder is working on image
        int attempts;               // number of tries to cache item
        int threadId;               // decoder thread working on image
        bool isCached;              // has image been cached
        bool isTarget;              // is this image targeted to be cached
        int priority;               // priority to cache image
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
        QString comment;            // for debugging
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
