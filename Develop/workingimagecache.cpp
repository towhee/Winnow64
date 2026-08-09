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
#include <cmath>

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
                               RenderTimings *timings, OutDepth depth, Space space)
{
    if (!work.isValid()) return false;

    OutputTransform output;
    /* One place decides the final quantisation and colour space, for both paths below. */
    auto toImage = [&output, depth, space](const WorkingImage &src, QImage &dst) {
        return depth == OutDepth::Sixteen ? output.ToImage16(src, dst, space)
                                          : output.ToImage(src, dst, space);
    };

    /* Identity edit: no Develop, no copy -- transform the cached image straight to
       QImage. */
    if (edit.isIdentity()) {
        QElapsedTimer t;
        if (timings) t.start();
        const bool ok = toImage(work, out);
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
        timings->vignetteMs = stage.vignetteMs;
        timings->grainMs = stage.grainMs;
    }
    const bool ok = toImage(developed, out);
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

/* ---- Masked-scope blend space (A/B, one flag to flip + revert) ----
   A (kPerceptualMaskBlend = false): lerp the masked scope in SCENE-LINEAR light -- physically
   faithful, but a STRONG masked adjustment's visible falloff is front-loaded because the display
   gamma steepens it near coverage m=1 (a linear feather then reads abrupt at the size edge).
   B (true, current): lerp in a PERCEPTUAL (gamma 2.2) space, so the DISPLAYED brightness tracks m
   ~linearly and a feather reads evenly. Only feather-transition pixels (0<m<1) pay the pow; m<=0 is
   a skip and m>=1 a copy (numerically identical to the old blend at those ends). */
namespace {
inline constexpr bool kPerceptualMaskBlend = true;
inline float maskEnc(float x) { return x <= 0.0f ? 0.0f : std::pow(x, 1.0f / 2.2f); }  // linear->perc
inline float maskDec(float y) { return y <= 0.0f ? 0.0f : std::pow(y, 2.2f); }         // perc->linear
}

bool WorkingImageCache::renderStack(const WorkingImage &work, const EditParams &base,
                                    const std::vector<StackScope> &scopes,
                                    QImage &out, RenderTimings *timings, OutDepth depth,
                                    Space space, StackResume *resume)
{
    if (!work.isValid()) return false;
    const size_t n = size_t(work.width) * size_t(work.height);

    QElapsedTimer t;
    if (timings) t.start();
    Develop develop;

    /* Accumulator = Global applied globally. Owned (mutable) so scopes can blend into it.
       RESUMED: the caller already holds the accumulator as it stood before scope `start`
       -- everything below it is unchanged -- so start there and skip the base develop and
       every scope beneath. Copied, not aliased: the blends below mutate it. */
    WorkingImage acc;
    size_t first = 0;
    const bool resumed = resume && resume->prefix && resume->prefix->isValid()
                         && resume->start <= scopes.size()
                         && size_t(resume->prefix->width) * size_t(resume->prefix->height) == n;
    if (resumed) {
        acc = *resume->prefix;
        first = resume->start;
    }
    else {
        acc = work;
        if (!base.isIdentity()) develop.Apply(acc, base, nullptr);
    }

    for (size_t i = first; i < scopes.size(); ++i) {
        const StackScope &L = scopes[i];
        const bool masked = L.mask && !L.mask->empty();
        if (masked && L.mask->size() != n) continue;   // malformed mask: skip the scope

        /* Hand back the accumulator as it stands BEFORE this scope blends into it -- that
           is precisely what a later re-render of this scope alone needs to resume from.
           Taken here, before any mutation. When we resumed AT this scope the caller's own
           prefix is still untouched, so re-share it instead of copying it. */
        const bool capture = resume && i == resume->capture;
        if (capture)
            resume->outPrefix = (resumed && i == resume->start)
                                    ? resume->prefix
                                    : std::make_shared<const WorkingImage>(acc);

        /* Develop this scope from the running ACCUMULATOR (the result of the scopes
           below), so where masks overlap the adjustments COMPOUND rather than the top
           scope replacing the one below -- e.g. two overlapping +1-stop scopes give ~+2
           stops in the overlap. In scene-linear this stacking is the natural per-op
           composition (exposure multiplies, etc.). An identity scope aliases acc (no
           copy/Apply) and blends to a no-op. */
        WorkingImage layBuf;
        const WorkingImage *layP = &acc;
        std::shared_ptr<const WorkingImage> layShared;   // set only on a resumed layer
        if (!L.params.isIdentity()) {
            /* A mask-only edit leaves this scope's developed pixels untouched, so the
               caller can hand them straight back and the develop pass is skipped -- the
               point of the whole mechanism. */
            if (resumed && i == resume->start && resume->layer && resume->layer->isValid()
                && size_t(resume->layer->width) * size_t(resume->layer->height) == n) {
                layShared = resume->layer;
                layP = layShared.get();
            }
            else {
                layBuf = acc;
                develop.Apply(layBuf, L.params, nullptr);
                layP = &layBuf;
            }
        }
        if (capture) {
            /* An identity scope has no layer of its own -- it aliases the accumulator --
               so hand back nothing and let the next call re-alias. */
            if (layP == &acc)   resume->outLayer.reset();
            else if (layShared) resume->outLayer = layShared;   // resumed: re-share it
            else                resume->outLayer =
                                    std::make_shared<const WorkingImage>(layBuf);
        }

        const float *hi = layP->rgb.data();
        float *dst = acc.rgb.data();
        if (!masked) {                         // global scope: its params on top of acc
            const float *src = hi;
            maskParallelFor(n, [=](size_t i0, size_t i1) {
                std::copy(src + i0*3, src + i1*3, dst + i0*3);
            });
        }
        else {
            const float *mk = L.mask->data();
            maskParallelFor(n, [=](size_t i0, size_t i1) {
                for (size_t i = i0; i < i1; ++i) {
                    const float m = mk[i];
                    const size_t j = i * 3;
                    if (m <= 0.0f) continue;                       // scope has no effect here
                    if (m >= 1.0f) {                               // fully the scope
                        dst[j+0] = hi[j+0]; dst[j+1] = hi[j+1]; dst[j+2] = hi[j+2];
                        continue;
                    }
                    const float im = 1.0f - m;
                    if (kPerceptualMaskBlend) {                    // B: lerp in perceptual space
                        dst[j+0] = maskDec(maskEnc(dst[j+0])*im + maskEnc(hi[j+0])*m);
                        dst[j+1] = maskDec(maskEnc(dst[j+1])*im + maskEnc(hi[j+1])*m);
                        dst[j+2] = maskDec(maskEnc(dst[j+2])*im + maskEnc(hi[j+2])*m);
                    } else {                                       // A: lerp in scene-linear
                        dst[j+0] = dst[j+0]*im + hi[j+0]*m;
                        dst[j+1] = dst[j+1]*im + hi[j+1]*m;
                        dst[j+2] = dst[j+2]*im + hi[j+2]*m;
                    }
                }
            });
        }
    }
    if (timings) timings->developMs = t.restart();

    OutputTransform output;
    const bool ok = depth == OutDepth::Sixteen ? output.ToImage16(acc, out, space)
                                               : output.ToImage(acc, out, space);
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
    dst.cam = src.cam;      // white balance must resolve identically on the proxy
    dst.rgb.resize(static_cast<size_t>(dw) * static_cast<size_t>(dh) * 3);
    cv::Mat dstMat(dh, dw, CV_32FC3, dst.rgb.data());
    cv::resize(srcMat, dstMat, cv::Size(dw, dh), 0, 0, cv::INTER_AREA);
    return dst;
}
