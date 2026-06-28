#include "Develop/workingimagecache.h"
#include "Develop/develop.h"
#include "Develop/outputtransform.h"
#include <QImage>
#include <QElapsedTimer>
#include <QtConcurrent>
#include <QThreadPool>
#include <QFuture>
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

bool WorkingImageCache::render(const WorkingImage &work, const EditParams &edit, QImage &out,
                               RenderTimings *timings)
{
    if (!work.isValid()) return false;

    OutputTransform output;

    /* Identity edit: no Develop, no copy -- transform the cached image straight to QImage. */
    if (edit.isIdentity()) {
        QElapsedTimer t;
        if (timings) t.start();
        const bool ok = output.ToImage(work, out);
        if (timings) timings->toImageMs = t.elapsed();
        return ok;
    }

    /* Non-identity: Develop mutates in place, so work on a copy and leave the cached
       (pre-develop) entry pristine for the next slider value. */
    QElapsedTimer t;
    if (timings) t.start();
    WorkingImage developed = work;
    if (timings) timings->copyMs = t.restart();
    Develop develop;
    Develop::StageTimings stage;
    develop.Apply(developed, edit, timings ? &stage : nullptr);
    if (timings) {
        timings->developMs = t.restart();
        timings->denoiseMs = stage.denoiseMs;
        timings->pointMs   = stage.pointMs;
        timings->textureMs = stage.textureMs;
        timings->dehazeMs  = stage.dehazeMs;
    }
    const bool ok = output.ToImage(developed, out);
    if (timings) timings->toImageMs = t.elapsed();
    return ok;
}

namespace {
/* Data-parallel pixel loop (mirrors Develop::parallelFor, which is file-local there). fn(i0,i1)
   processes a disjoint half-open pixel slice. */
template <class F>
inline void maskParallelFor(size_t n, F fn)
{
    const int maxThreads = qMax(1, QThreadPool::globalInstance()->maxThreadCount());
    const size_t kMinPerChunk = 1u << 16;
    if (maxThreads == 1 || n < kMinPerChunk * 2) { fn(size_t(0), n); return; }
    const int chunks = int(qMin<size_t>(maxThreads, (n + kMinPerChunk - 1) / kMinPerChunk));
    const size_t per = (n + chunks - 1) / chunks;
    QVector<QFuture<void>> futures;
    futures.reserve(chunks);
    for (int k = 0; k < chunks; ++k) {
        const size_t i0 = size_t(k) * per;
        const size_t i1 = qMin(n, i0 + per);
        if (i0 >= i1) break;
        futures.append(QtConcurrent::run(QThreadPool::globalInstance(), [fn, i0, i1]{ fn(i0, i1); }));
    }
    for (QFuture<void> &f : futures) f.waitForFinished();
}
} // namespace

bool WorkingImageCache::renderMasked(const WorkingImage &work, const EditParams &belowEdit,
                                     const EditParams &aboveEdit, const std::vector<float> &mask,
                                     QImage &out, RenderTimings *timings)
{
    if (!work.isValid()) return false;
    const size_t n = size_t(work.width) * size_t(work.height);
    if (mask.size() != n) return false;

    QElapsedTimer t;
    if (timings) t.start();

    /* Develop each side in scene-linear. An identity side (commonly Base) skips the copy + Apply
       and aliases `work`, so a masked layer over an unedited Base costs ~one develop pass. */
    Develop develop;
    WorkingImage belowBuf, aboveBuf;
    const WorkingImage *belowP = &work, *aboveP = &work;
    if (!belowEdit.isIdentity()) { belowBuf = work; develop.Apply(belowBuf, belowEdit, nullptr); belowP = &belowBuf; }
    if (!aboveEdit.isIdentity()) { aboveBuf = work; develop.Apply(aboveBuf, aboveEdit, nullptr); aboveP = &aboveBuf; }
    if (timings) timings->developMs = t.restart();

    /* Fresh destination buffer (metadata copied; pixels written by the blend, no pixel copy). */
    WorkingImage blended;
    blended.width = work.width; blended.height = work.height;
    blended.white = work.white; blended.sceneReferred = work.sceneReferred;
    blended.rgb.resize(n * 3);

    const float *lo = belowP->rgb.data();
    const float *hi = aboveP->rgb.data();
    const float *mk = mask.data();
    float *dst = blended.rgb.data();
    maskParallelFor(n, [=](size_t i0, size_t i1) {
        for (size_t i = i0; i < i1; ++i) {
            const float m = mk[i];
            const float im = 1.0f - m;
            const size_t j = i * 3;
            dst[j+0] = lo[j+0]*im + hi[j+0]*m;
            dst[j+1] = lo[j+1]*im + hi[j+1]*m;
            dst[j+2] = lo[j+2]*im + hi[j+2]*m;
        }
    });
    if (timings) timings->developMs += t.restart();

    OutputTransform output;
    const bool ok = output.ToImage(blended, out);
    if (timings) timings->toImageMs = t.elapsed();
    return ok;
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
