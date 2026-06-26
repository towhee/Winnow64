#include "Develop/workingimagecache.h"
#include "Develop/develop.h"
#include "Develop/outputtransform.h"
#include <QImage>
#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
#include <algorithm>

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

WorkingImage WorkingImageCache::downscaled(const WorkingImage &src, int targetLongEdge)
{
    if (!src.isValid() || targetLongEdge <= 0) return src;

    const int longEdge = std::max(src.width, src.height);
    if (longEdge <= targetLongEdge) return src;   // already small enough

    const double f = static_cast<double>(targetLongEdge) / longEdge;
    const int dw = std::max(1, static_cast<int>(src.width  * f + 0.5));
    const int dh = std::max(1, static_cast<int>(src.height * f + 0.5));

    /* INTER_AREA is the correct (anti-aliasing) filter for shrinking. The WorkingImage buffer
       is interleaved float RGB, so it maps directly onto a CV_32FC3 view with no copy. */
    const cv::Mat srcMat(src.height, src.width, CV_32FC3,
                         const_cast<float *>(src.rgb.data()));
    WorkingImage dst;
    dst.width = dw;
    dst.height = dh;
    dst.white = src.white;
    dst.sceneReferred = src.sceneReferred;
    dst.rgb.resize(static_cast<size_t>(dw) * static_cast<size_t>(dh) * 3);
    cv::Mat dstMat(dh, dw, CV_32FC3, dst.rgb.data());
    cv::resize(srcMat, dstMat, cv::Size(dw, dh), 0, 0, cv::INTER_AREA);
    return dst;
}
