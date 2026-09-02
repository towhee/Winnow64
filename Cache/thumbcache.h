#ifndef THUMBCACHE_H
#define THUMBCACHE_H

#include <QByteArray>
#include <QMutex>
#include <QSqlDatabase>
#include <QString>

/*
    THE BROWSING THUMBNAIL, CACHED IN THE INDEX.

    WHY IT EXISTS. Scrolling an unvisited region of a 250,000-image catalog
    cannot open a file per row. Winnow's thumbnails come from an embedded
    preview inside the image, so producing one means opening the file, walking
    to the preview's segment and decoding it -- perfectly affordable for a
    folder of 500, and not affordable at all as the scroll position of a whole
    library. The index is the layer that makes it affordable: the icon is
    decoded ONCE, ever, and after that a scroll is a database read.

    THE READ CHAIN IS memory store -> index -> decode, and it only reads as one
    chain because all three are keyed the same way. IconStore
    (Datamodel/iconstore.h) went path-keyed for exactly this reason; this keys
    on cachePathKey(), the same key the image and keyword tables use.

    THE PAYLOAD IS A BLOB, which is the opposite of what DevPreviewCache does,
    and the difference is payload SIZE rather than a change of mind. A
    devPreview is a full-resolution JPEG at one to three megabytes: a file is
    the right container and the database would only be adding a copy. A
    thumbnail is ten to thirty kilobytes, and a quarter of a million of those is
    250,000 files and inodes to hold a few gigabytes, with a per-file open
    dominating every read. SQLite beats the filesystem for blobs of roughly this
    size and its page cache serves a scroll from memory, which is the access
    pattern here exactly.

    STALENESS. A thumbnail is only valid for the bytes it was made from, so
    every row records the source file's size and modification time and a
    mismatch is a MISS rather than a stale picture. That is the same stamp
    DevPreviewCache keeps and for the same reason: an image edited by another
    program keeps its path, and showing yesterday's picture for today's file is
    the one failure a cache must not have.

    EVICTION AND THE SWEEP mirror DevPreviewCache's policy even though the
    container differs: a byte cap, evicting demoted rows first and then least
    recently used, and a sweep that DEMOTES rather than deletes when the source
    file is missing -- skipping entries whose volume is not mounted, so ejecting
    a drive does not throw away the thumbnails for everything on it.

    THREADING. All public methods take the mutex. The blob reads and writes are
    small enough (tens of kilobytes) that holding it across them is not the
    stall that made DevPreviewCache drop its lock around multi-megabyte file
    I/O; if that ever stops being true, the pattern to copy is next door.
*/
class ThumbCache
{
public:
    static ThumbCache &instance();

    /*  Byte cap for the stored blobs. Default is deliberately generous: at
        ~20 KB a thumbnail, a quarter of a million images is about 5 GB, and the
        whole point is that a library stays browsable. */
    void setMaxBytes(qint64 bytes);
    qint64 maxBytes() const;

    /*  Store the thumbnail for fPath. srcSize and srcMtime are the source
        file's identity at the time it was decoded -- see STALENESS. Replaces
        any existing row. A failed write is silently ignored: this is a cache. */
    void put(const QString &fPath, const QByteArray &jpg, int w, int h,
             qint64 srcSize, qint64 srcMtime);

    /*  The cached JPEG for fPath, or an empty QByteArray on a miss or a source
        that has changed since. A hit is marked most-recently-used. */
    QByteArray get(const QString &fPath, qint64 srcSize, qint64 srcMtime);

    bool contains(const QString &fPath, qint64 srcSize, qint64 srcMtime) const;

    /*  File-operation sync. Call via Utilities/fileops.h, not directly. */
    void onMoved(const QString &srcPath, const QString &dstPath);
    void onDeleted(const QString &fPath);

    /*  Demote entries whose source file is gone, skipping unmounted volumes.
        Safe to call off the GUI thread. Returns the number demoted. */
    int sweep();

    void clear();

    int count() const;
    qint64 totalBytes() const;

private:
    ThumbCache() = default;
    Q_DISABLE_COPY(ThumbCache)

    QSqlDatabase dbLocked() const;
    /*  Drop rows until the total is within the cap: demoted first, then least
        recently used. Called after a put, with the lock held. */
    void evictLocked(QSqlDatabase &db);

    mutable QMutex mMutex;
    qint64 mMaxBytes = 5LL * 1024 * 1024 * 1024;   // 5 GB; see setMaxBytes
};

#endif // THUMBCACHE_H
