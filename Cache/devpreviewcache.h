#ifndef DEVPREVIEWCACHE_H
#define DEVPREVIEWCACHE_H

#include <QString>
#include <QByteArray>
#include <QHash>
#include <QMutex>

/*
    On-disk cache of full-resolution devPreviews -- the large tier of the develop-preview
    system. See notes/Documentation.txt "Original and Developed Previews".

    WHAT IT IS FOR

    An image the user has developed has two pictures: the camera's embedded JPEG (the
    origPreview) and the render of its develop recipe (the devPreview). This cache holds
    the devPreview, as a JPEG at full sensor resolution by default (G::devPreviewMaxEdge),
    so that outside Develop mode the loupe can show -- and zoom -- the developed picture
    without decoding the raw at all, and so entering Develop paints the developed look
    immediately instead of showing the UNDEVELOPED image for the ~2-3s the scene-linear
    sensor decode takes.

    FULL RESOLUTION IS WHY THE CAP IS LARGE. At sensor resolution an entry is several MB
    rather than a few hundred KB, so the LRU byte cap below is load-bearing rather than
    theoretical, and the user can trade disk for zoom quality with the "Developed preview
    size" preference.

    The small 256px thumbnail preview that feeds the icon grid is NOT here -- it lives
    inside the image's XMP sidecar (winnow:DevelopPreview -- an on-disk attribute name that
    predates the origPreview/devPreview terminology and must not be renamed), so it travels
    with the file and needs no cache, no index and no orphan handling. This class holds
    only the large, cheap-to-lose tier.

    WHY THE FILES ARE NAMED BY AN OPAQUE ID

    Lookup is by absolute file path, but the cache FILE is named <id>.jpg where id is an
    opaque counter -- never a hash of the path. That is what makes file-operation sync
    cheap: a rename or a move rewrites one index entry's path string, with no file on disk
    to rename and nothing to re-encode. Deleting an image unlinks one file. See
    Utilities/fileops.h, which is the only thing that should be calling onMoved/onDeleted.

    STALENESS

    Every entry records the hash of the develop recipe (EditStack base64) it was rendered
    from. get() returns a hit only when the caller's current recipe hash matches, so an
    edit made anywhere -- including by another Winnow instance or another machine -- can
    never show stale pixels. It just misses and re-renders.

    ORPHANS

    Previews are generated only as a byproduct of editing, so the cache is disposable:
    every failure mode costs a re-render, never data. Three layers keep it bounded:

      1. maxBytes LRU cap. The load-bearing one -- the dominant growth case is thousands
         of images that all still exist, which no orphan sweep would ever reclaim.
      2. sweep(), run once at startup, off the GUI thread, AFTER the first folder load.
         Not at shutdown: a force-quit or crash skips that, and closeEvent is already
         doing synchronous work. An entry whose source file is gone is DEMOTED (live =
         false) rather than deleted, so it evicts first but survives a file that comes
         back from the trash. Crucially the sweep skips any entry whose volume is not
         currently mounted -- Winnow browses memory cards and external drives constantly,
         and an unmounted drive must not look like a mass deletion.
      3. FileOps hooks, for the operations Winnow performs itself.

    THREADING

    put() is called from the GUI thread when develop edits are flushed; get() from the GUI
    thread and potentially from reader threads. All state is behind one mutex. Disk I/O
    happens under that mutex, which is acceptable because the payloads are a few hundred
    KB and puts are debounced -- never on a drag.
*/
class DevPreviewCache
{
public:
    static DevPreviewCache &instance();

    /* Where the cache lives. Defaults to AppDataLocation/PreviewCache; the app never
       calls this, tests point it at a temp dir. Does NOT read the index -- that happens
       lazily on first use, which is the only load path there is. */
    void setCacheDir(const QString &dir);
    QString cacheDir() const;

    void setMaxBytes(qint64 bytes);
    qint64 maxBytes() const;

    /* Store the developed preview for fPath, rendered from the recipe whose base64 blob
       hashes to blobHash. Replaces any existing entry for fPath. A miss on either
       argument, or a failed disk write, is silently ignored -- this is a cache. */
    void put(const QString &fPath, const QByteArray &blobHash, const QByteArray &jpg);

    /* The cached JPEG for fPath, or an empty QByteArray on a miss or a recipe mismatch.
       A hit is marked most-recently-used. */
    QByteArray get(const QString &fPath, const QByteArray &blobHash);

    bool contains(const QString &fPath, const QByteArray &blobHash) const;

    /* File-operation sync. Call via Utilities/fileops.h, not directly. */
    void onMoved(const QString &srcPath, const QString &dstPath);
    void onDeleted(const QString &fPath);

    /* Drop everything, on disk and in memory (the preferences "Clear preview cache"). */
    void clear();

    /* Demote entries whose source file is gone, skipping unmounted volumes. Safe to call
       off the GUI thread. Returns the number of entries demoted. */
    int sweep();

    /* Delete cache files with no index entry, and drop index entries with no file. Called
       after load(); also the repair path when the index is unreadable. */
    void reconcile();

    void load();          // read the index (called by setCacheDir)
    void save();          // write the index if dirty

    qint64 totalBytes() const;
    int count() const;

private:
    DevPreviewCache() = default;
    Q_DISABLE_COPY(DevPreviewCache)

    struct Entry {
        quint64 id = 0;         // names the file on disk: <id in hex>.jpg
        QByteArray blobHash;    // recipe this preview was rendered from
        qint64 bytes = 0;
        qint64 lastUsed = 0;    // seconds since epoch
        bool live = true;       // false => source file was missing at the last sweep
        /* Mount point, so the sweep can tell "gone" from "ejected". */
        QString volRoot;
    };

    /* The index is loaded lazily, on first use, rather than by an explicit init call
       from MW. An init call is a thing a future caller can forget, and forgetting it is
       silent and expensive: the cache starts empty (so every prior-session preview
       misses), nextId restarts at 1 (so new entries OVERWRITE unrelated cache files),
       and the next save() rewrites the index without the entries it never read. */
    void ensureLoadedLocked();
    void loadLocked();
    void reconcileLocked();

    QString filePathLocked(quint64 id) const;
    void removeLocked(const QString &fPath);
    void evictLocked();
    void touchLocked(const QString &fPath);
    static QString volumeRootOf(const QString &path);
    static bool volumeMounted(const QString &volRoot);

    mutable QMutex mutex;
    QHash<QString, Entry> entries;      // absolute image path -> entry
    QString dir;
    quint64 nextId = 1;
    qint64 bytes = 0;
    qint64 capBytes = 20LL * 1024 * 1024 * 1024;  // 20 GB; see G::devPreviewCacheMaxBytes
    bool dirty = false;
    bool loaded = false;
};

#endif // DEVPREVIEWCACHE_H
