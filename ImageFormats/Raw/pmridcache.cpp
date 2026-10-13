#include "ImageFormats/Raw/pmridcache.h"
#include "Utilities/versionkey.h"
#include "Main/global.h"
#include <chrono>

PmridCache &PmridCache::instance()
{
    static PmridCache cache;
    return cache;
}

QString PmridCache::keyOf(const QString &fPath, int iso)
{
    // see KEY in the header: the source file, so every version shares one entry
    return VersionKey::sourceOf(fPath) + "|iso=" + QString::number(iso);
}

std::shared_ptr<const WorkingImage> PmridCache::get(const QString &fPath, int iso)
{
    const QString key = keyOf(fPath, iso);
    std::lock_guard<std::mutex> lock(mtx);
    for (auto it = lru.begin(); it != lru.end(); ++it) {
        if (it->key != key) continue;
        lru.splice(lru.begin(), lru, it);          // mark most recently used
        return lru.front().img;
    }
    return nullptr;
}

PmridCache::Result PmridCache::tryAcquire(const QString &fPath, int iso,
                                          std::shared_ptr<const WorkingImage> &out)
{
    const QString key = keyOf(fPath, iso);
    out.reset();
    std::lock_guard<std::mutex> lock(mtx);
    for (auto it = lru.begin(); it != lru.end(); ++it) {
        if (it->key != key) continue;
        lru.splice(lru.begin(), lru, it);
        out = lru.front().img;
        return Result::Hit;
    }
    if (inFlight.count(key)) return Result::InFlight;
    inFlight.insert(key);
    return Result::Claimed;
}

PmridCache::Result PmridCache::acquire(const QString &fPath, int iso,
                                       const QAtomicInt *abort,
                                       std::shared_ptr<const WorkingImage> &out)
{
    const QString key = keyOf(fPath, iso);
    out.reset();
    bool waited = false;
    std::unique_lock<std::mutex> lock(mtx);
    for (;;) {
        for (auto it = lru.begin(); it != lru.end(); ++it) {
            if (it->key != key) continue;
            lru.splice(lru.begin(), lru, it);
            out = lru.front().img;
            return waited ? Result::Waited : Result::Hit;
        }
        if (!inFlight.count(key)) {
            inFlight.insert(key);
            return Result::Claimed;
        }
        /* Someone else is making it. Wake on any publish/abandon, and at least every
           100 ms to poll abort -- an ImageCache stop() waits on this thread. */
        if (abort && abort->loadAcquire()) return Result::Aborted;
        waited = true;
        cv.wait_for(lock, std::chrono::milliseconds(100));
        if (abort && abort->loadAcquire()) return Result::Aborted;
    }
}

void PmridCache::publish(const QString &fPath, int iso,
                         const std::shared_ptr<const WorkingImage> &img)
{
    const QString key = keyOf(fPath, iso);
    {
        std::lock_guard<std::mutex> lock(mtx);
        inFlight.erase(key);
        if (img && img->isValid()) {
            for (auto it = lru.begin(); it != lru.end(); ++it) {
                if (it->key != key) continue;
                bytes -= it->bytes;
                lru.erase(it);
                break;
            }
            Entry e;
            e.key = key;
            e.img = img;
            e.bytes = qint64(img->rgb.size()) * qint64(sizeof(float));
            bytes += e.bytes;
            lru.push_front(std::move(e));
            evictLocked();
        }
    }
    cv.notify_all();
    if (G::isLogger) G::log("PmridCache::publish", key);
}

void PmridCache::abandon(const QString &fPath, int iso)
{
    {
        std::lock_guard<std::mutex> lock(mtx);
        inFlight.erase(keyOf(fPath, iso));
    }
    cv.notify_all();     // a waiter claims in the producer's place
}

void PmridCache::evictLocked()
{
    // never the front: the image just produced (the one being edited) always stays
    while (bytes > kMaxBytes && lru.size() > 1) {
        bytes -= lru.back().bytes;
        lru.pop_back();
    }
}

void PmridCache::remove(const QString &fPath)
{
    const QString prefix = VersionKey::sourceOf(fPath) + "|iso=";
    std::lock_guard<std::mutex> lock(mtx);
    for (auto it = lru.begin(); it != lru.end();) {
        if (it->key.startsWith(prefix)) { bytes -= it->bytes; it = lru.erase(it); }
        else ++it;
    }
}

void PmridCache::clear()
{
    std::lock_guard<std::mutex> lock(mtx);
    lru.clear();
    bytes = 0;
}

int PmridCache::count() const
{
    std::lock_guard<std::mutex> lock(mtx);
    return int(lru.size());
}

qint64 PmridCache::currentBytes() const
{
    std::lock_guard<std::mutex> lock(mtx);
    return bytes;
}
