#ifndef THUMBCACHE_H
#define THUMBCACHE_H

#include <QAtomicInteger>
#include <QByteArray>
#include <QImage>
#include <QList>
#include <QMutex>
#include <QObject>
#include <QSqlDatabase>
#include <QString>
#include <QThread>
#include <QWaitCondition>

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
class ThumbCache;

/*  ONE WRITER, and the handoff is Qt's event queue rather than a queue of
        our own. The first version was a QThread with a QList, a QMutex and two
        QWaitConditions; ThreadSanitizer complained about the shared list under
        real load and the fix was not obvious from the report. A hand-rolled
        producer/consumer queue is a thing to be argued about at 2am, and this
        is a CACHE -- it is not worth one. A queued signal is a handoff Qt has
        already got right.

            Batching survives the change: the worker accumulates into mPending,
        which only ever exists on the worker's own thread, and a zero-timer
        flushes the accumulation in one transaction. Nothing is shared, so
        nothing needs a lock. */
class ThumbWriter : public QObject
{
Q_OBJECT
public:
explicit ThumbWriter(ThumbCache *owner) : mOwner(owner) {}
public slots:
void take(const QString &fPath, const QImage &im);
void flushPending();
private:
ThumbCache *mOwner;
        /*  Worker thread only -- never touched from anywhere else, which is the
            whole point of accumulating here rather than in a shared queue. */
QList<QPair<QString, QImage>> mPending;
bool mScheduled = false;
};


class ThumbCache
{
    friend class ThumbWriter;
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

    /*  HAND THE ICON THAT WAS JUST DECODED TO THE WRITER. Returns immediately:
        the stat, the skip-if-present check, the JPEG encode and the insert all
        happen on one background thread, batched into transactions.

        IT IS QUEUED BECAUSE DOING IT INLINE WAS MEASURED AND WAS NOT FREE. The
        first version ran on whichever loader thread produced the image, which
        seemed reasonable -- already off the GUI thread, image already in hand.
        Over 50 mixed raw files it cost:

            caching off                29.6 ms per icon
            inline, index empty        43.7 ms per icon   (+47%)
            inline, index already full 34.1 ms per icon   (+15%)

        The encode is a small part of that. The rest is SQLite: every statement
        was its own transaction, and several reader threads were serialising on
        one writer -- so even the WARM case, which does nothing but one indexed
        SELECT and then skips, cost 4.5 ms an icon. One writer thread committing
        in batches removes both, and the loaders go back to paying nothing.

        THE QUEUE IS BOUNDED and drops when full rather than growing. A queued
        QImage is a quarter of a megabyte, so an unbounded backlog behind a slow
        disk is a memory leak with a good excuse. Dropping is free of
        consequence -- this is a cache, and the next visit to the folder offers
        the thumbnail again. */
    void putImage(const QString &fPath, const QImage &im, bool hasDevelopRecipe);

    /*  Finish what is queued and stop the writer. Called at shutdown; also what
        a test calls to make an asynchronous write observable. */
    void flush();

    /*  The cached JPEG for fPath, or an empty QByteArray on a miss or a source
        that has changed since. A hit is marked most-recently-used. */
    QByteArray get(const QString &fPath, qint64 srcSize, qint64 srcMtime);

    /*  The cached thumbnail as an image ready to hand to the model, or a null
        QImage on a miss. THE POINT OF THE WHOLE EXERCISE: this replaces opening
        the file, walking to its embedded preview's segment and decoding that,
        with one indexed read and a small JPEG decode.

        It stats fPath for the staleness comparison, so a file edited by another
        program misses and is re-decoded. Returns RGB32 at no more than
        G::maxIconSize, matching what Thumb::loadThumb produces, so nothing
        downstream can tell where the picture came from. */
    QImage getImage(const QString &fPath, bool hasDevelopRecipe);

    /*  IS THE CAMERA'S OWN THUMBNAIL THE RIGHT PICTURE FOR THIS IMAGE RIGHT
        NOW? Both the read and the write are gated on this, and the write
        matters as much as the read.

        Thumb::loadThumb does not always return the camera's thumbnail: for an
        image with a develop recipe, in Develop mode or with the preview source
        set to Developed, it returns the DEVELOPED thumbnail from the sidecar
        instead. Caching that would poison the index -- the developed picture
        would then be served for the same file in Original mode, showing the
        user an edit they asked not to see. Reading from the index in that case
        is the same mistake in the other direction.

        SO THE TEST IS PER IMAGE, NOT PER MODE, and getting that wrong is how
        this was first written: gating on the mode alone disabled the cache
        completely, because G::previewSource DEFAULTS to Developed. An unedited
        image -- the overwhelming majority -- has no developed thumbnail at any
        setting, so Thumb::devThumb falls through to the camera's and the cache
        is exactly right. Only an EDITED image in a developed-showing mode is
        excluded.

        hasDevelopRecipe comes from ImageMetadata::developEdited, which the
        metadata read has already established, so this costs nothing. */
    static bool wantsOriginalThumb(bool hasDevelopRecipe);

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

    /*  How many thumbnails the writer has actually ENCODED AND STORED this
        session -- not how many were offered. The difference is the skip: a
        revisited folder offers every thumbnail again and stores none of them.

        It exists because that distinction is otherwise invisible. Re-encoding
        an unchanged image produces byte-identical output, so a test that
        watched the row count or the total bytes could not tell a skip from a
        rewrite -- and did not, until deleting the skip failed to fail it. */
    qint64 written() const { return mWritten.loadRelaxed(); }

private:
    ThumbCache() = default;
    ~ThumbCache();
    Q_DISABLE_COPY(ThumbCache)

    void startWriterLocked();
    /*  The bodies of put() and contains() without the lock, so writeBatch can
        call them inside the one transaction it already holds the lock for. */
    void putLocked(QSqlDatabase &db, const QString &fPath, const QByteArray &jpg,
                   int w, int h, qint64 srcSize, qint64 srcMtime);
    bool containsLocked(QSqlDatabase &db, const QString &fPath,
                        qint64 srcSize, qint64 srcMtime) const;
    /*  Write one batch inside a single transaction. Writer thread only. */
    void writeBatch(const QList<QPair<QString, QImage>> &batch);

    QSqlDatabase dbLocked() const;
    /*  Drop rows until the total is within the cap: demoted first, then least
        recently used. Called after a put, with the lock held. */
    void evictLocked(QSqlDatabase &db);

    mutable QMutex mMutex;
    qint64 mMaxBytes = 5LL * 1024 * 1024 * 1024;   // 5 GB; see setMaxBytes
    QThread *mThread = nullptr;
    ThumbWriter *mWriter = nullptr;
    QAtomicInteger<qint64> mWritten = 0;
    /*  How many jobs have been posted and not yet written. Checked before
        posting so the backlog stays bounded -- a queued QImage is a quarter of
        a megabyte, and an unbounded one behind a slow disk is a memory leak
        with a good excuse. Dropping costs nothing: the next visit offers the
        thumbnail again. */
    QAtomicInteger<int> mInFlight = 0;
};

#endif // THUMBCACHE_H
