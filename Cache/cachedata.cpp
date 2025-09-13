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

    // // subtract old if replacing
    // if (auto it = imCache.find(key); it != imCache.end()) {
    //     bytes.fetch_sub(static_cast<quint64>(it.value().sizeInBytes()),
    //                     std::memory_order_relaxed);
    //     it.value() = image; // replace in place to avoid rehash
    // } else {
    //     imCache.insert(key, image);
    // }

    // add new
    bytes.fetch_add(static_cast<quint64>(image.sizeInBytes()),
                    std::memory_order_relaxed);
}

void ImageCacheData::remove(const QString &key)
{
    QWriteLocker locker(&rwLock);
    // imCache.remove(key);

    // take() gives us the removed value so we can adjust bytes
    QImage img = imCache.take(key);
    // if (img.isNull()) return false;
    qDebug() << "ImageCacheData::remove" << key << img.sizeInBytes();

    bytes.fetch_sub(static_cast<quint64>(img.sizeInBytes()),
                    std::memory_order_relaxed);
}

void ImageCacheData::clear()
{
    QWriteLocker locker(&rwLock);
    imCache.clear();
    bytes.store(0, std::memory_order_relaxed);
}
