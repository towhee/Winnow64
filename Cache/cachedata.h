#ifndef CACHEDATA_H
#define CACHEDATA_H

#include <QtWidgets>
#include "Datamodel/HashMap.h"
#include "Datamodel/HashNode.h"

class ImageCacheData : public QObject
{
    Q_OBJECT
public:
    explicit ImageCacheData(QObject *parent);

    // concurrent image cache hash
    CTSL::HashMap<QString, QImage> imCache;

    // cache parameters in struct (used in ImageCache and MainWindow)
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
        int minMB;                  // minimum MB available to cache
        int folderMB;               // MB required for all files in folder
        int targetFirst;            // beginning of target range to cache
        int targetLast;             // end of the target range to cache
        int decoderCount;           // number of separate threads used to decode images
        bool isShowCacheStatus;     // show in app status bar
        bool usePreview;            // cache smaller pixmap for speedier initial display
        QSize previewSize;          // monitor display dimensions for scale of previews
    } cache;

    struct CacheItem {
        int key;                    // same as row in dm->sf (sorted and filtered datamodel)
        int origKey;                // the key of a previous filter or sort
        QString fPath;              // image full path
        bool isMetadata;            // has metadata for embedded jpg offset and length been loaded
        bool isCaching;             // decoder is working on image
        int attempts;               // number of tries to cache item
        int threadId;               // decoder thread working on image
        bool isCached;              // has image been cached
        bool isTarget;              // is this image targeted to be cached
        int priority;               // priority to cache image
        int sizeMB;                 // memory req'd to cache image
        // below reqd for decoding to avoid using DAtaModel in another thread
        bool metadataLoaded;
        int orientation;
        int rotationDegrees;
        quint32 offsetFull;
        quint32 lengthFull;
        int samplesPerPixel;
        QByteArray iccBuf;
        QString comment;            // for debugging
    } cacheItem;



    QList<CacheItem> cacheItemList;
};
Q_DECLARE_METATYPE(ImageCacheData::Cache)

#endif // CACHEDATA_H

/*
  Key Priority   Target Attempts  Caching   Thread   Cached   SizeMB   File Name
    0        0        1        1        0        0        1       13   D:/Pictures/Calendar_Beach/20041205_0062.jpg
    1        1        1        1        0        1        1       13   D:/Pictures/Calendar_Beach/20041205_0065.jpg
    2        2        1        1        0        2        1       20   D:/Pictures/Calendar_Beach/2011-11-21_0073.jpg
    3        3        1        3        0        6        1       34   D:/Pictures/Calendar_Beach/2014-07-29_0002.jpg
    4        4        1        3        0        3        1       35   D:/Pictures/Calendar_Beach/2012-04-03_0208.jpg
    5        5        1        1        0        5        1       37   D:/Pictures/Calendar_Beach/2011-11-21_0219.jpg
    6        6        1        1        0        6        1       39   D:/Pictures/Calendar_Beach/2011-11-22_0329.jpg
    7        7        1        1        0        7        1       39   D:/Pictures/Calendar_Beach/2011-11-22_0390.jpg
    8        8        1        1        0        0        1       39   D:/Pictures/Calendar_Beach/2010-10-02_0025.jpg
    9        9        1        1        0        1        1       52   D:/Pictures/Calendar_Beach/2012-04-03_0074.jpg
   10       10        1        3        0        4        1      119   D:/Pictures/Calendar_Beach/2012-08-16_0015.jpg
   11       11        1        3        0        5        1      119   D:/Pictures/Calendar_Beach/2014-09-26_0288.jpg
   12       12        1        3        0        1        1      119   D:/Pictures/Calendar_Beach/2014-04-11_0060.jpg

*/