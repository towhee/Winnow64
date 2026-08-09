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

    VALIDITY. reset() drops everything when the image, the proxy dimensions, the EXIF
    orientation or the GLOBAL (base) params change. The base params matter even for a
    mask: the content-range tools (Luminance/Color Range) sample a display-referred
    reference built from the DEVELOPED base (MW::ensureRangeRef), so a base change can
    alter a mask's coverage without altering any of its components.

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
    void clear()
    {
        QMutexLocker lk(&mutex);
        clearLocked();
    }

    /* Make the cache current for this image/geometry/base, dropping every entry if any of
       them changed. Returns true when the existing entries survived. */
    bool reset(const QString &p, int width, int height, int deg, const QByteArray &base)
    {
        QMutexLocker lk(&mutex);
        if (path == p && w == width && h == height && degrees == deg && baseKey == base)
            return true;
        clearLocked();
        path = p; w = width; h = height; degrees = deg; baseKey = base;
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
        std::shared_ptr<const WorkingImage> prefix;
        std::shared_ptr<const WorkingImage> layer;
    };

    /* Copies the pair OUT under the lock (both point at const data), so the caller can
       hold them for the length of a render without blocking the GUI thread's clear(). */
    Hot hotAt(size_t index) const
    {
        QMutexLocker lk(&mutex);
        return (hot.index >= 0 && size_t(hot.index) == index) ? hot : Hot();
    }

    void setHot(size_t index, std::shared_ptr<const WorkingImage> prefix,
                std::shared_ptr<const WorkingImage> layer)
    {
        QMutexLocker lk(&mutex);
        hot.index = int(index);
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
    void clearLocked()                   // mutex already held
    {
        path.clear(); w = h = degrees = 0; baseKey.clear(); entries.clear();
        hot = Hot();
    }

    mutable QMutex mutex;
    /* What the whole cache was built against; any change invalidates every entry. */
    QString    path;
    int        w = 0, h = 0, degrees = 0;
    QByteArray baseKey;                 // serialized GLOBAL (base) EditParams
    /* One per scope POSITION. Do NOT rename this to `slots` (the obvious name): `slots`
       is a Qt keyword macro that expands to nothing, and the failure reads as a pile of
       unrelated syntax errors. */
    std::vector<Slot> entries;
    Hot hot;
};

#endif // DEVELOPSTACKCACHE_H
