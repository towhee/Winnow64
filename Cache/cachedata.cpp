#include "Cache/cachedata.h"

ImageCacheData::ImageCacheData(QObject *) {}

bool ImageCacheData::contains(const QString &key)
{
    QReadLocker locker(&rwLock);
    return imCache.contains(key);
}

void ImageCacheData::insert(const QString &key, const QImage &image)
{
    QWriteLocker locker(&rwLock);
    imCache.insert(key, image);
}

void ImageCacheData::remove(const QString &key)
{
    QWriteLocker locker(&rwLock);
    imCache.remove(key);
}

void ImageCacheData::clear()
{
    QWriteLocker locker(&rwLock);
    imCache.clear();
}
