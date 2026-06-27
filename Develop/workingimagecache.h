#ifndef WORKINGIMAGECACHE_H
#define WORKINGIMAGECACHE_H

#include <QString>
#include <QHash>
#include <QList>
#include <QMutex>
#include <memory>
#include "Develop/workingimage.h"
#include "Develop/editparams.h"

class QImage;

/*
    Caches the post-decode, PRE-develop WorkingImage (scene-linear float, see
    workingimage.h) keyed by file path, so an interactive develop edit can re-render an
    image without re-decoding or re-demosaicing it.

    The expensive part of showing a RAW is UnpackCfa -> Demosaic -> RawColor, which produces
    the WorkingImage. Develop + OutputTransform that follow are comparatively cheap. So when
    the user drags a slider, the right work is: keep the WorkingImage, re-run only
    Develop + OutputTransform. That is what render() does, fed from this cache:

        first view of an image   : full decode -> put() the pre-develop WorkingImage
        every later slider change : get() the WorkingImage -> render() -> QImage

    Ownership / cost: entries are shared_ptr<const WorkingImage>; put() takes a share (no
    pixel copy) and get() hands one back (no copy). render() copies only when EditParams is
    non-identity (Develop mutates in place); an identity edit transforms the const image
    straight to a QImage with no copy.

    Memory: a full-res WorkingImage is large (W*H*3 floats ~ 4x the 8-bit QImage), so the
    cache is byte-budgeted with LRU eviction. The budget is deliberately small -- this is an
    edit accelerator for the handful of recently-viewed images, NOT a second image cache. The
    most-recently-used entry is never evicted, so a single image larger than the budget is
    still served (the active edit always hits).

    Threading: decoder threads put(); the GUI/editor thread get()s and render()s. All access
    is guarded by one mutex. render() is a free static that touches no cache state, so it can
    run on a WorkingImage the caller already holds.

    This is a process-wide singleton (like the other shared decode helpers): keyed by absolute
    path, it is folder-agnostic, and clear() is called when a new folder loads.
*/
class WorkingImageCache
{
public:
    static WorkingImageCache &instance();

    /* Store the pre-develop WorkingImage for fPath (shares ownership; no pixel copy).
       A null or invalid image is ignored. Marks fPath most-recently-used and evicts LRU
       entries past the byte budget. */
    void put(const QString &fPath, const std::shared_ptr<const WorkingImage> &work);

    /* Fetch the WorkingImage for fPath, or nullptr on a miss. A hit is marked
       most-recently-used. */
    std::shared_ptr<const WorkingImage> get(const QString &fPath);

    bool contains(const QString &fPath) const;
    void remove(const QString &fPath);   // invalidate one entry (e.g. file changed on disk)
    void clear();                        // drop everything (new folder / new instance)

    /* Byte budget. Default kDefaultMaxBytes; settable so the host can size it to the machine.
       Setting a smaller budget evicts immediately. */
    void setMaxBytes(qint64 bytes);
    qint64 maxBytes() const;
    qint64 currentBytes() const;
    int count() const;

    void setEnabled(bool on);            // off => put() is a no-op and the cache is cleared
    bool isEnabled() const;

    /* Per-stage wall-clock timings for one render(), filled when a non-null pointer is passed.
       A latency probe only (used by the Develop preview's [DevTime] logging); pass nullptr in
       normal use. */
    struct RenderTimings {
        qint64 copyMs = 0; qint64 developMs = 0; qint64 toImageMs = 0;
        /* Develop sub-stages (subset of developMs). */
        qint64 denoiseMs = 0; qint64 pointMs = 0; qint64 textureMs = 0; qint64 dehazeMs = 0;
    };

    /* Render a WorkingImage through Develop + OutputTransform into out. Copies the image
       only when edit is non-identity (Develop mutates in place). Returns false if work is
       invalid or the output transform fails. Static and stateless: usable with a cached
       entry or any WorkingImage the caller owns. Fills *timings when non-null. */
    static bool render(const WorkingImage &work, const EditParams &edit, QImage &out,
                       RenderTimings *timings = nullptr);

    /* Area-downsampled copy of src whose longest edge is <= targetLongEdge (white /
       sceneReferred carried through). Used to build the interactive develop PROXY so a slider
       drag renders at screen resolution instead of full sensor resolution. Returns a copy of
       src unchanged when it is already within targetLongEdge or targetLongEdge <= 0. */
    static WorkingImage downscaled(const WorkingImage &src, int targetLongEdge);

    static qint64 bytesOf(const WorkingImage &work);

private:
    WorkingImageCache() = default;
    Q_DISABLE_COPY(WorkingImageCache)

    struct Entry {
        std::shared_ptr<const WorkingImage> work;
        qint64 bytes = 0;
    };

    void evictLocked();                  // call with mutex held
    void touchLocked(const QString &fPath);

    static constexpr qint64 kDefaultMaxBytes = 768LL * 1024 * 1024;   // ~768 MB

    mutable QMutex mutex;
    QHash<QString, Entry> entries;
    QList<QString> lru;                  // front = least-recently-used, back = most-recent
    qint64 totalBytes = 0;
    qint64 budget = kDefaultMaxBytes;
    bool enabled = true;
};

#endif // WORKINGIMAGECACHE_H
