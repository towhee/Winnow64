#include "Cache/cachedata.h"

namespace {
// Subtract `sub` from `counter`, clamping at 0 to avoid quint64 underflow if
// accounting ever drifts (e.g. a prior raw QHash::insert bypassed the API).
inline void safeSubBytes(std::atomic<quint64> &counter, quint64 sub) {
    quint64 cur = counter.load(std::memory_order_relaxed);
    while (true) {
        const quint64 next = (sub > cur) ? 0 : (cur - sub);
        if (counter.compare_exchange_weak(cur, next,
                                          std::memory_order_relaxed,
                                          std::memory_order_relaxed))
            break;
    }
}
}

ImageCacheData::ImageCacheData(QObject *) {}

bool ImageCacheData::contains(const QString &key)
{
    QMutexLocker locker(&rwLock);
    return imCache.contains(key);
}

void ImageCacheData::insert(const QString &key, const QImage &image)
{
    QMutexLocker locker(&rwLock);

    // replace: subtract the old image's bytes before swapping
    if (auto it = imCache.find(key); it != imCache.end()) {
        safeSubBytes(bytes, static_cast<quint64>(it.value().sizeInBytes()));
        it.value() = image; // replace in place to avoid rehash
    }
    // else add new image
    else {
        imCache.insert(key, image);
    }
    // add bytes to cache total bytes
    bytes.fetch_add(static_cast<quint64>(image.sizeInBytes()),
                    std::memory_order_relaxed);
}

void ImageCacheData::remove(const QString &key)
{
    QMutexLocker locker(&rwLock);

    // take() gives us the removed value so we can adjust bytes
    QImage img = imCache.take(key);
    safeSubBytes(bytes, static_cast<quint64>(img.sizeInBytes()));
}

void ImageCacheData::clear()
{
    QMutexLocker locker(&rwLock);
    imCache.clear();
    bytes.store(0, std::memory_order_relaxed);
}
