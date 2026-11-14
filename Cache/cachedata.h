#ifndef CACHEDATA_H
#define CACHEDATA_H

#include <QtWidgets>
#include <atomic>
#include <functional>

/*
    THE DECODED FULL-SIZE IMAGES, KEYED BY ROW KEY (PATH). Shared by every thread: the
    ImageCache thread fills and trims it, the GUI thread reads it for the loupe, compare,
    copy, Embellish and stacking. Every access goes through this API and takes the lock;
    the hash itself is private, because a caller that read it directly -- contains()
    then imCache.value(), two locks with a gap -- got a default-constructed image while
    the cache thread trimmed in the gap, or read the hash during a rehash (C25 in the
    2026-11-13 sync review; Documentation 4.4.13).

    A null image is never stored: insert() ignores one. A cache hit is therefore always
    something to paint.

    Changes to the CONTENTS should come from ImageCache (its invalidate() / replace()),
    which also keeps its own bookkeeping and the model's cache badge in step. The
    mutators here are for ImageCache itself.
*/
class ImageCacheData : public QObject
{
    Q_OBJECT
public:
    explicit ImageCacheData(QObject *);

    bool contains(const QString &key) const;
    /*  TEST AND FETCH UNDER ONE LOCK. Returns false when the key is absent; image is
        left untouched. */
    bool get(const QString &key, QImage &image) const;
    int count() const;
    bool isEmpty() const { return count() == 0; }
    QStringList keys() const;
    /*  Visit every entry under the lock. fn must be quick and must not call back into
        this object or emit a signal (a direct connection would run arbitrary code with
        the lock held): collect what you need and act after. */
    void forEach(const std::function<void(const QString &, const QImage &)> &fn) const;

    void insert(const QString &key, const QImage &image);       // a null image is ignored
    /*  Replace the image only if key is still cached (rotation): never resurrects an
        entry the cache trimmed meanwhile. Returns true if replaced. */
    bool replaceIfPresent(const QString &key, const QImage &image);
    void remove(const QString &key);
    // re-key one image (a file rename); false when oldKey is not cached
    bool rename(const QString &oldKey, const QString &newKey);
    void clear();

    // O(1) read, no lock needed
    quint64 sizeBytes() const noexcept {
        return bytes.load(std::memory_order_relaxed);
    }
    double  sizeMB() const noexcept {
        return double(sizeBytes()) / (1024.0 * 1024.0);
    }

private:
    QHash<QString, QImage> imCache;

    /*  Was QReadWriteLock; replaced with QMutex because ThreadSanitizer cannot track
        QReadWriteLock as a synchronization primitive (false-positive races on imCache).
        Reads are infrequent enough that the simpler primitive is fine. */
    mutable QMutex rwLock;

    std::atomic<quint64> bytes{0}; // total pixel bytes of images
};
#endif // CACHEDATA_H
