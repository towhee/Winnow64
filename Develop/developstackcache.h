#ifndef DEVELOPSTACKCACHE_H
#define DEVELOPSTACKCACHE_H

#include <QByteArray>
#include <QString>
#include <QVector>
#include <QMutex>
#include <memory>
#include <vector>
#include "Develop/editstack.h"
#include "Develop/workingimage.h"

/*
    DEVELOP INTERACTIVE STACK CACHE

    What a mask drag actually changes is ONE scope's mask. Everything else the tick
    recomputes -- every other scope's rasterized mask, and (in phase 2) every scope's
    developed pixels -- is bit-for-bit what it was on the previous tick. This cache holds
    those per-scope intermediates so the tick redoes only the part that moved.

    ONE ENTRY PER SCOPE POSITION, not per content. The scope being dragged therefore
    replaces its own entry every tick instead of thrashing a shared, content-keyed store,
    and the untouched scopes above and below it keep theirs.

    PROXY ONLY. MW owns the instance and hands it to developCompositeStack; the full-res
    settle render passes nullptr and recomputes, which keeps the entries small (proxy
    resolution) instead of the ~w*h*4 bytes a full-res mask would cost.

    MUTEX-GUARDED, and every member is private for that reason. It used to be
    GUI-thread-only and lock-free, but the proxy render moved to a worker
    (MW::renderDevelopPreview -> developProxyPool), so the render reads and writes it from
    that thread while the GUI thread still calls reset()/clear() on image, folder, proxy
    and denoise-base changes. Locking is per call and a call happens a handful of times
    per render, so the cost is nothing; DO NOT hand out references into `entries` or `hot`
    -- copy the shared_ptr out under the lock and work from that, which is safe because
    everything the pointers reach is const.

    VALIDITY. reset() drops everything when the image, the proxy dimensions or the EXIF
    orientation change.

    The GLOBAL (base) params are deliberately NOT part of that. They used to be, and it
    made a Global slider drag rebuild every mask on every tick. The stated reason applies
    to exactly one family of tools: the content-range masks (Luminance/Color Range) sample
    a display-referred reference built from the DEVELOPED base (MW::ensureRangeRef), so a
    base change can move their coverage without moving any component. Nothing else has
    that dependency -- every other path-registered reference (the AI fields, the object
    coverages, the brush auto-mask guide) is keyed by PATH alone and is never rebuilt when
    the base moves, so dropping those masks was invalidating for a dependency the code
    does not have. The base signature is now folded into the per-scope MASK KEY only for
    scopes that actually carry a range mask (MW::scopeMaskDependsOnBase).

    The HOT intermediates are a different matter and still carry the base key explicitly:
    `prefix` IS the developed base, so any base change invalidates it.

    A scope's entry is additionally skipped when a reference its components sample was
    not registered yet -- an AI field, a range reference, or the brush auto-mask guide.
    Caching a mask built without its input would freeze the un-confined version on
    screen, the same trap brushRasterCached's autoInputsReady guard avoids one level
    down.
*/

/* Signature of one scope's mask components: everything buildMaskBuffer reads. paramsJson
   is HASHED rather than copied -- it carries every point of every brush stroke, and this
   is built per scope per tick. Same idiom as objectRefKey (mainwindow.cpp), which keys an
   ObjectRef on qHash(paramsJson). */
inline QByteArray maskComponentSignature(const MaskComponent &m)
{
    QByteArray sig;
    sig += QByteArray::number(m.tool);      sig += ':';
    sig += QByteArray::number(m.op);        sig += ':';
    sig += m.enabled  ? '1' : '0';
    sig += m.inverted ? '1' : '0';          sig += ':';
    sig += QByteArray::number(double(m.feather), 'g', 9);   sig += ':';
    sig += QByteArray::number(qulonglong(qHash(m.paramsJson)));
    return sig;
}

inline QByteArray maskComponentsSignature(const QVector<MaskComponent> &components)
{
    QByteArray sig;
    sig.reserve(components.size() * 40);
    for (const MaskComponent &m : components) {
        sig += maskComponentSignature(m);
        sig += '|';
    }
    return sig;
}

class DevelopStackCache
{
public:
    struct Slot {
        QByteArray maskKey;             // maskComponentsSignature of this scope
        QByteArray paramsKey;           // serialized EditParams of this scope
        std::shared_ptr<const std::vector<float>> mask;   // null => rebuild
    };
    /* Full teardown: the cached data AND the render scratch. Called on folder change,
       image change and proxy rebuild -- the points where the buffers are the wrong size
       and the user may be leaving Develop, so holding ~150 MB would not be a cache. */
    void clear()
    {
        QMutexLocker lk(&mutex);
        clearLocked();
        scratchAcc.reset(); scratchLay.reset();
        scratchOutA.reset(); scratchOutB.reset();
        scratchPreA.reset(); scratchPreB.reset();
    }

    /* Make the cache current for this image/geometry, dropping every entry if either
       changed. Returns true when the existing entries survived. The base params are NOT
       part of this -- see VALIDITY above. */
    bool reset(const QString &p, int width, int height, int deg)
    {
        QMutexLocker lk(&mutex);
        if (path == p && w == width && h == height && degrees == deg)
            return true;
        clearLocked();
        path = p; w = width; h = height; degrees = deg;
        return false;
    }

    /* The cached mask for scope `index` if its components still match, else null. */
    std::shared_ptr<const std::vector<float>>
    mask(size_t index, const QByteArray &key) const
    {
        QMutexLocker lk(&mutex);
        if (index >= entries.size()) return nullptr;
        const Slot &s = entries[index];
        return (s.mask && s.maskKey == key) ? s.mask : nullptr;
    }

    void putMask(size_t index, const QByteArray &key,
                 std::shared_ptr<const std::vector<float>> buf)
    {
        QMutexLocker lk(&mutex);
        if (entries.size() <= index) entries.resize(index + 1);
        entries[index].maskKey = key;
        entries[index].mask = std::move(buf);
    }

    /* A scope whose references were not ready must not keep a stale entry either. */
    void dropMask(size_t index)
    {
        QMutexLocker lk(&mutex);
        if (index >= entries.size()) return;
        entries[index].maskKey.clear();
        entries[index].mask.reset();
    }

    /* Scopes were added/removed: positions no longer line up, so nothing survives. */
    void matchScopeCount(size_t n)
    {
        QMutexLocker lk(&mutex);
        if (entries.size() != n) { entries.clear(); entries.resize(n); hot = Hot(); }
    }

    QByteArray paramsKeyAt(size_t index) const
    {
        QMutexLocker lk(&mutex);
        return index < entries.size() ? entries[index].paramsKey : QByteArray();
    }

    void setParamsKey(size_t index, const QByteArray &key)
    {
        QMutexLocker lk(&mutex);
        if (entries.size() <= index) entries.resize(index + 1);
        entries[index].paramsKey = key;
    }

    /* ---- Developed intermediates for the ONE scope being edited ----

       Caching a developed image for EVERY scope would cost two proxy-resolution
       WorkingImages per scope (~46 MB each at 2400x1600). In practice only the scope the
       user is dragging is re-entered tick after tick, so one pair captures the whole win
       at a fixed cost.

         prefix = the accumulator BEFORE scope `index`: the base params applied globally,
                  then every scope below `index` fully blended in.
         layer  = scope `index` developed FROM prefix (null when its params are identity,
                  where the layer would just alias prefix).

       A MASK-only change to `index` leaves both valid, so the tick collapses to one blend
       plus the output transform -- no develop pass at all. A PARAMS change to `index`
       keeps prefix and rebuilds layer. A change anywhere BELOW `index` invalidates both,
       because prefix is exactly what those scopes produce. */
    struct Hot {
        int index = -1;
        /* The base params `prefix` was developed from. Unlike a mask buffer, the prefix
           is a FUNCTION of the base, so it is dead the moment the base moves. */
        QByteArray baseKey;
        std::shared_ptr<const WorkingImage> prefix;
        std::shared_ptr<const WorkingImage> layer;
    };

    /* Copies the pair OUT under the lock (both point at const data), so the caller can
       hold them for the length of a render without blocking the GUI thread's clear(). */
    Hot hotAt(size_t index, const QByteArray &base) const
    {
        QMutexLocker lk(&mutex);
        return (hot.index >= 0 && size_t(hot.index) == index && hot.baseKey == base)
                   ? hot : Hot();
    }

    /* ---- Reusable render scratch ----

       A tick allocated and released three proxy-sized WorkingImages: the accumulator,
       the edited scope's layer, and the snapshot handed back as outLayer. At ~38 MB each
       on a 3.1 MP proxy that measured 29 ms PER FREE against 2 ms to memcpy the same
       bytes -- 87 ms of a 121 ms tick, i.e. the interactive Develop render spent most of
       its time in the allocator, not on pixels. Keeping the buffers alive across ticks
       removes all of it; see Develop/workingimage.h assignReusing.

       Held here because this object already has exactly the right lifetime: it is reset
       on image / proxy-size / orientation change, which is precisely when the buffers
       stop being the right size. They are freed by clear(), so leaving Develop or
       changing folder gives the memory back.

       LAYER A/B. layerOut alternates between two buffers so the one renderStack WRITES
       is never the one `hot.layer` is currently serving -- everything the cache hands
       out is read as const, and this is the only place that rule could be broken. */
    std::shared_ptr<WorkingImage> accScratch()
    {
        QMutexLocker lk(&mutex);
        if (!scratchAcc) scratchAcc = std::make_shared<WorkingImage>();
        return scratchAcc;
    }
    std::shared_ptr<WorkingImage> layScratch()
    {
        QMutexLocker lk(&mutex);
        if (!scratchLay) scratchLay = std::make_shared<WorkingImage>();
        return scratchLay;
    }
    /* The spare layer buffer: whichever of the pair `hot.layer` is NOT pointing at. */
    std::shared_ptr<WorkingImage> outScratch()
    {
        QMutexLocker lk(&mutex);
        if (!scratchOutA) scratchOutA = std::make_shared<WorkingImage>();
        if (!scratchOutB) scratchOutB = std::make_shared<WorkingImage>();
        return (hot.layer == scratchOutA) ? scratchOutB : scratchOutA;
    }
    /* Same for the prefix snapshot. It needs its own pair because the prefix is handed
       back on a DIFFERENT schedule from the layer: during a GLOBAL slider drag the base
       moves every tick, so hotAt() rejects the prefix every tick and a fresh one is
       captured -- which meant allocating and releasing a proxy-sized image per tick.
       Measured at setHot 62 ms of a 108 ms tick once the mask cache started working, the
       same allocator churn one level down. */
    std::shared_ptr<WorkingImage> prefixScratch()
    {
        QMutexLocker lk(&mutex);
        if (!scratchPreA) scratchPreA = std::make_shared<WorkingImage>();
        if (!scratchPreB) scratchPreB = std::make_shared<WorkingImage>();
        return (hot.prefix == scratchPreA) ? scratchPreB : scratchPreA;
    }

    void setHot(size_t index, const QByteArray &base,
                std::shared_ptr<const WorkingImage> prefix,
                std::shared_ptr<const WorkingImage> layer)
    {
        QMutexLocker lk(&mutex);
        hot.index = int(index);
        hot.baseKey = base;
        hot.prefix = std::move(prefix);
        hot.layer = std::move(layer);
    }

    /* Lowest scope index whose mask or params differ from what is cached, or 0 when the
       cache holds nothing comparable. Everything below it is bit-for-bit unchanged, so
       `prefix` at this index (if we have it) is still exactly right. */
    size_t firstDirty(const std::vector<QByteArray> &maskKeys,
                      const std::vector<QByteArray> &paramsKeys) const
    {
        QMutexLocker lk(&mutex);
        if (entries.size() != maskKeys.size()) return 0;
        for (size_t i = 0; i < maskKeys.size(); ++i)
            if (entries[i].maskKey != maskKeys[i]
                || entries[i].paramsKey != paramsKeys[i])
                return i;
        return maskKeys.size();          // nothing changed
    }

private:
    /* Drops the cached DATA. Deliberately leaves the scratch buffers alone: they are
       storage, not data, and reset() runs this on every GLOBAL-params change -- i.e.
       every tick of a Global slider drag. Releasing them there re-allocated four
       proxy-sized buffers per tick and pushed the frees into the compositor instead of
       removing them, measured as a REGRESSION from ~122 ms to ~236-394 ms per tick.
       assignReusing resizes them if the geometry changes, so keeping them is safe; only
       clear() (folder change, leaving Develop) actually hands the memory back. */
    void clearLocked()                   // mutex already held
    {
        path.clear(); w = h = degrees = 0; entries.clear();
        hot = Hot();
    }

    mutable QMutex mutex;
    /* What the whole cache was built against; any change invalidates every entry. */
    QString    path;
    int        w = 0, h = 0, degrees = 0;
    /* One per scope POSITION. Do NOT rename this to `slots` (the obvious name): `slots`
       is a Qt keyword macro that expands to nothing, and the failure reads as a pile of
       unrelated syntax errors. */
    std::vector<Slot> entries;
    Hot hot;
    /* Render scratch -- storage, never data; see the accessors above. */
    std::shared_ptr<WorkingImage> scratchAcc, scratchLay;
    std::shared_ptr<WorkingImage> scratchOutA, scratchOutB;   // layer snapshot A/B
    std::shared_ptr<WorkingImage> scratchPreA, scratchPreB;   // prefix snapshot A/B
};

#endif // DEVELOPSTACKCACHE_H
