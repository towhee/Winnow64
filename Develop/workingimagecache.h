#ifndef WORKINGIMAGECACHE_H
#define WORKINGIMAGECACHE_H

#include <QString>
#include <QHash>
#include <QList>
#include <QMutex>
#include <memory>
#include "Develop/workingimage.h"
#include "Develop/editparams.h"
#include "Develop/outputtransform.h"

class QImage;

/*
    Caches the post-decode, PRE-develop WorkingImage (scene-linear float, see
    workingimage.h) keyed by file path, so an interactive develop edit can re-render an
    image without re-decoding or re-demosaicing it.

    The expensive part of showing a RAW is UnpackCfa -> Demosaic, which produces the
    WorkingImage. Develop + OutputTransform that follow are comparatively cheap. So when
    the user drags a slider, the right work is: keep the WorkingImage, re-run only
    Develop + OutputTransform. That is what render() does, fed from this cache:

    WHAT IS CACHED IS CAMERA-NATIVE, NOT WORKING-SPACE. RawColor deliberately does not
    apply the white balance or the colour matrix (see rawcolor.h), so the cached pixels
    are still in the sensor's own primaries and every colour decision -- input profile,
    white balance, calibration -- happens DOWNSTREAM of this cache, in Develop. That is
    what makes changing a camera profile or the temperature a re-render instead of a full
    re-decode. Anything baked in before put() could only be undone by decoding again.

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
        qint64 denoiseMs = 0; qint64 pointMs = 0; qint64 textureMs = 0; qint64 clarityMs = 0;
        qint64 dehazeMs = 0; qint64 vignetteMs = 0; qint64 sharpenMs = 0; qint64 grainMs = 0;
        /* renderStack sub-stages (together they make up developMs on the stack path).
           developMs alone could not tell a full-frame WorkingImage COPY from the develop
           pass from the mask blend -- three very different fixes -- so on a params drag,
           where all three run per tick, it was an unsplittable number. See
           notes/Documentation.txt "Split a number before optimising anything behind it".
             stackCopyMs  the acc = *prefix / layBuf = acc / outLayer snapshots
             stackApplyMs Develop::Apply for the scopes' own params (not the base)
             stackBlendMs the per-scope mask blend into the accumulator
             stackFreeMs  releasing each scope's layer -- INSIDE developMs, so the four
                          buckets above should sum to it
             outFreeMs    releasing the accumulator, AFTER toImageMs, so it is outside
                          every other stage
           The two frees are split because a large free is real work (macOS returns big
           allocations to the OS) and they land on opposite sides of developMs; lumped,
           neither remainder would balance. */
        qint64 stackCopyMs = 0; qint64 stackApplyMs = 0; qint64 stackBlendMs = 0;
        qint64 stackFreeMs = 0; qint64 outFreeMs = 0;
        /* Filled by the caller's compositor (MW::developCompositeStack), not by
           render*(): the per-scope mask rasterisation and the EXIF-rotate +
           proxy-upscale that follow the stack render. maskScopes is how many
           scopes actually built a buffer. */
        qint64 maskBuildMs = 0; int maskScopes = 0; qint64 orientScaleMs = 0;
        /* Also compositor-filled: re-arming the per-scope cache, whose shared_ptr drops
           free the PREVIOUS tick's prefix and layer. */
        qint64 setHotMs = 0;
        /* Resume state for the tick, so a [DevTime] line says WHY it was cheap or dear:
           which scope the stack resumed at, how many scopes there were, whether the
           per-scope cache survived reset(), and whether a prefix was actually offered. */
        int  resumeStart = -1; int stackScopes = 0;
        bool cacheSurvived = false; bool hadPrefix = false; bool hadLayer = false;
    };

    /* Output bit depth of the final OutputTransform. Eight (Format_RGB888) is the
       interactive/loupe path and the default everywhere. Sixteen (Format_RGBX64) is used
       only by the EXPORT path -- the develop pipeline is float throughout, so the pack to
       8 bits is the sole place precision is lost. */
    enum class OutDepth { Eight, Sixteen };

    /* Output colour space of the final OutputTransform, defaulting to sRGB so every
       interactive caller is unaffected. Only the EXPORT path passes anything else; note
       that whatever writes the resulting QImage must TAG it to match (see
       OutputTransform::ColorSpaceOf). */
    using Space = OutputTransform::Space;

    /* Render a WorkingImage through Develop + OutputTransform into out. Copies the image
       only when edit is non-identity (Develop mutates in place). Returns false if work is
       invalid or the output transform fails. Static and stateless: usable with a cached
       entry or any WorkingImage the caller owns. Fills *timings when non-null.

       scratch, when given, is REUSED STORAGE for that copy (see assignReusing in
       Develop/workingimage.h). The no-scope interactive path lands here every tick, and
       allocating + releasing the copy cost 62 ms per tick on a 6.7 MP proxy against 1 ms
       to fill it. Optional: null allocates locally, which is what the one-shot callers
       (export, the reference builders, the settle render) want. Caller owns it and must
       keep it alive for the call. */
    static bool render(const WorkingImage &work, const EditParams &edit, QImage &out,
                       RenderTimings *timings = nullptr,
                       OutDepth depth = OutDepth::Eight,
                       Space space = Space::sRGB,
                       WorkingImage *scratch = nullptr);

    /* One scope of a stack composite: its develop params and a 0..1 mask (row-major
       width*height, matching work; null or empty => the scope applies globally).

       The mask is SHARED, not owned: the interactive proxy re-renders with the same mask
       for every scope the user is not currently dragging, and those buffers are megabytes
       each (see MW's DevelopStackCache), so handing one over must not copy it. */
    struct StackScope {
        EditParams params;
        std::shared_ptr<const std::vector<float>> mask;
    };

    /* Interactive resume point: lets a repeated render skip the scopes it already knows
       the answer for. Optional -- pass nullptr and renderStack computes everything from
       `work`, exactly as before. Owned and populated by MW's DevelopStackCache (see
       Develop/developstackcache.h), which is proxy-only and GUI-thread-only.

       IN   start   first scope to compute; scopes below it are skipped entirely.
            prefix  the accumulator as it stands BEFORE `start` (base applied globally +
                    every scope below `start` blended in). Null => ignore `start` and
                    render the whole stack.
            layer   scope `start` ALREADY developed from prefix; null => develop it here.
                    Valid only when scope `start`'s params are unchanged -- a mask-only
                    edit, which is the case this whole mechanism exists for.
       OUT  capture which scope's intermediates to hand back for the next call
                    (size_t(-1) = none). outPrefix/outLayer are filled with the
                    accumulator before, and the developed image of, that scope. outLayer
                    stays null when the scope's params are identity (its "layer" is just
                    the prefix).

       SCRATCH  accScratch / layScratch / outScratch are REUSED STORAGE, not data. A tick
                allocates and releases three proxy-sized WorkingImages (the accumulator,
                the scope's layer, and the snapshot handed back as outLayer); at ~38 MB
                each on a 3.1 MP proxy that measured 29 ms per free -- 72% of the tick,
                against 2 ms to memcpy the same bytes. Supplying these lets renderStack
                refill existing buffers (assignReusing) instead. All three are optional:
                null means allocate locally, which is what the full-res settle render
                does -- see FULL-RES SCRATCH below for what that costs and why it is
                still the default.

                preScratch does the same for the prefix snapshot, and needs its own pair
                because it is captured on a different schedule: during a GLOBAL slider
                drag the base moves every tick, so the cached prefix is rejected every
                tick and a fresh one is taken.

                outScratch and preScratch must NOT be the buffers the cache is currently
                handing back as `layer` and `prefix` -- renderStack writes them, and
                everything a cache hands out is read as const. The caller alternates two
                buffers for each to guarantee that; see MW::developCompositeStack.

       FULL-RES SCRATCH -- a considered, DEFERRED option, not an oversight.

                The settle render passes null and allocates locally, so it pays the
                allocator every time. MEASURED, 9600x6376 (61 MP) with two mask scopes,
                total 5207 ms: `free 1735` + `outFree 572` = 2307 ms, 44% of the render,
                purely releasing memory. Hoisting the layer buffer out of the scope loop
                (renderStack) removed roughly half of that -- N scopes now share one
                buffer instead of N -- leaving ~1400 ms, of which `outFree` (the
                accumulator, ~735 MB, unavoidable within one render) is the floor.

                WHAT A SCRATCH WOULD BUY: that remaining ~1400 ms, on a render that
                currently takes ~5.2 s. WHAT IT WOULD COST: the accumulator and the layer
                pinned between settle renders -- ~1.5 GB at 61 MP, against a 768 MB
                WorkingImageCache budget and a memory governor that already exists
                because raw decode concurrency blew the footprint once
                (notes/Documentation.txt "Memory governor").

                WHY DEFERRED: the settle render is BACKGROUND. It does not block the UI,
                it runs once when a drag stops, and the proxy is on screen meanwhile --
                so this trades a large permanent footprint for latency the user is not
                waiting on. The interactive path is the one that justified pinning
                memory, and there the buffers are proxy-sized (~38 MB) and released by
                DevelopStackCache::clear() on image or folder change.

                IF REVISITED, the honest design is a scratch OWNED BY THE SETTLE RENDER
                and released when it completes -- reuse across the scopes of ONE render
                (already done for the layer) plus the accumulator, without holding
                anything between renders. That captures most of the win at no standing
                cost. Sizing it per image would also need the memory governor consulted,
                not just assumed.

                DO NOT re-evaluate this with an isolated benchmark. A standalone test
                frees the same 735 MB in ~0 ms and concludes there is nothing here; the
                cost is memory PRESSURE, not size, and only appears with the image cache,
                the source image and the 245 MB mask buffers all resident. Measure in the
                app via [DevTime]'s free / outFree fields. */
    struct StackResume {
        size_t start = 0;
        std::shared_ptr<const WorkingImage> prefix;
        std::shared_ptr<const WorkingImage> layer;
        size_t capture = size_t(-1);
        std::shared_ptr<const WorkingImage> outPrefix, outLayer;
        std::shared_ptr<WorkingImage> accScratch, layScratch, outScratch, preScratch;
    };

    /* Stack composite, blended in scene-linear before the output transform: start from
       base (applied globally), then for each scope develop `work` with its params and
       blend over the accumulator by its mask (acc = acc*(1-m) + scope*m; a global scope
       replaces). One final OutputTransform. An identity params side skips its develop
       (aliases work). */
    static bool renderStack(const WorkingImage &work, const EditParams &base,
                            const std::vector<StackScope> &scopes,
                            QImage &out, RenderTimings *timings = nullptr,
                            OutDepth depth = OutDepth::Eight,
                            Space space = Space::sRGB,
                            StackResume *resume = nullptr);

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
