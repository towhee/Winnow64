#include "Develop/workingimagecache.h"
#include "Develop/develop.h"
#include "Develop/outputtransform.h"
#include <QImage>

WorkingImageCache &WorkingImageCache::instance()
{
    static WorkingImageCache cache;
    return cache;
}

qint64 WorkingImageCache::bytesOf(const WorkingImage &work)
{
    /* Pixel buffer dominates; the struct itself is negligible. */
    return static_cast<qint64>(work.rgb.size()) * static_cast<qint64>(sizeof(float));
}

void WorkingImageCache::put(const QString &fPath,
                            const std::shared_ptr<const WorkingImage> &work)
{
    if (fPath.isEmpty() || !work || !work->isValid()) return;

    QMutexLocker lock(&mutex);
    if (!enabled) return;

    /* Replace any existing entry (e.g. re-decode of the same path). */
    auto it = entries.find(fPath);
    if (it != entries.end()) {
        totalBytes -= it->bytes;
        lru.removeOne(fPath);
    }

    Entry e;
    e.work = work;
    e.bytes = bytesOf(*work);
    entries.insert(fPath, e);
    lru.append(fPath);           // most-recently-used at the back
    totalBytes += e.bytes;

    evictLocked();
}

std::shared_ptr<const WorkingImage> WorkingImageCache::get(const QString &fPath)
{
    QMutexLocker lock(&mutex);
    auto it = entries.constFind(fPath);
    if (it == entries.constEnd()) return nullptr;
    touchLocked(fPath);
    return it->work;
}

bool WorkingImageCache::contains(const QString &fPath) const
{
    QMutexLocker lock(&mutex);
    return entries.contains(fPath);
}

void WorkingImageCache::remove(const QString &fPath)
{
    QMutexLocker lock(&mutex);
    auto it = entries.find(fPath);
    if (it == entries.end()) return;
    totalBytes -= it->bytes;
    entries.erase(it);
    lru.removeOne(fPath);
}

void WorkingImageCache::clear()
{
    QMutexLocker lock(&mutex);
    entries.clear();
    lru.clear();
    totalBytes = 0;
}

void WorkingImageCache::setMaxBytes(qint64 bytes)
{
    QMutexLocker lock(&mutex);
    budget = bytes > 0 ? bytes : 0;
    evictLocked();
}

qint64 WorkingImageCache::maxBytes() const
{
    QMutexLocker lock(&mutex);
    return budget;
}

qint64 WorkingImageCache::currentBytes() const
{
    QMutexLocker lock(&mutex);
    return totalBytes;
}

int WorkingImageCache::count() const
{
    QMutexLocker lock(&mutex);
    return entries.size();
}

void WorkingImageCache::setEnabled(bool on)
{
    QMutexLocker lock(&mutex);
    enabled = on;
    if (!enabled) {
        entries.clear();
        lru.clear();
        totalBytes = 0;
    }
}

bool WorkingImageCache::isEnabled() const
{
    QMutexLocker lock(&mutex);
    return enabled;
}

void WorkingImageCache::touchLocked(const QString &fPath)
{
    /* Move to the most-recently-used end. */
    lru.removeOne(fPath);
    lru.append(fPath);
}

void WorkingImageCache::evictLocked()
{
    /* Trim from the least-recently-used front while over budget, but never evict the last
       (most-recently-used) entry: the image being edited must stay resident even if it alone
       exceeds the budget. */
    while (totalBytes > budget && lru.size() > 1) {
        const QString victim = lru.takeFirst();
        auto it = entries.find(victim);
        if (it != entries.end()) {
            totalBytes -= it->bytes;
            entries.erase(it);
        }
    }
}

bool WorkingImageCache::render(const WorkingImage &work, const EditParams &edit, QImage &out)
{
    if (!work.isValid()) return false;

    OutputTransform output;

    /* Identity edit: no Develop, no copy -- transform the cached image straight to QImage. */
    if (edit.isIdentity())
        return output.ToImage(work, out);

    /* Non-identity: Develop mutates in place, so work on a copy and leave the cached
       (pre-develop) entry pristine for the next slider value. */
    WorkingImage developed = work;
    Develop develop;
    develop.Apply(developed, edit);
    return output.ToImage(developed, out);
}
