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
        int threadId;               // decoder thread working on image
        bool isCached;              // has image been cached
        bool isTarget;              // is this image targeted to be cached
        int priority;               // priority to cache image
        int sizeMB;                 // memory req'd to cache image
    } cacheItem;

    QList<CacheItem> cacheItemList;
};
#endif // CACHEDATA_H
