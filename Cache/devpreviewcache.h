#ifndef DEVPREVIEWCACHE_H
#define DEVPREVIEWCACHE_H

#include <QString>
#include <QByteArray>
#include <QList>
#include <QMutex>
#include <QSqlDatabase>

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
    inside the image's XMP sidecar (winnow:DevelopPreview -- an on-disk attribute name
    that predates the origPreview/devPreview terminology and must not be renamed), so it
    travels with the file and needs no cache, no index and no orphan handling. This holds
    only the large, cheap-to-lose tier.

    WHERE THE INDEX LIVES

    In SQLite, in the shared local index database beside the JPEGs -- table devpreview,
    see Cache/cachedb.h. It replaced a JSON array that was held entirely in memory and
    rewritten in full on every put, which does not survive being sized for 250,000 images:
    the array is tens of MB, the in-memory copy costs ~50 MB of paths, and a background
    preview build over a large library rewrote the whole thing once per image.

    Nothing else in this class holds the index. The only state kept in memory is the
    running byte total and the next id, both re-derived from the table when it is opened.

    LOOKUP IS BY A NORMALISED KEY, NOT THE RAW PATH. The same file reaches Winnow spelled
    several ways -- QFileInfo::filePath() from a folder scan, dropDir + "/" + name from a
    paste, QUrl::toLocalFile() from a Finder drag -- and a byte-exact index treats each as
    a different image, so the same picture gets cached twice and lookups silently miss.
    Every row carries both: path as the filesystem spelled it (what gets stat'd and what
    the diagnostics show) and pathkey for every WHERE clause. See Cache/pathkey.h.

    WHY THE FILES ARE NAMED BY AN OPAQUE ID

    Lookup is by absolute file path, but the cache FILE is named <id>.jpg where id is an
    opaque counter -- never a hash of the path. That is what makes file-operation sync
    cheap: a rename or a move rewrites one row's path column, with no file on disk to
    rename and nothing to re-encode. Deleting an image unlinks one file. See
    Utilities/fileops.h, which is the only thing that should be calling onMoved/onDeleted.

    STALENESS

    Every entry records two things about what it depicts, and a hit needs both.

      1. The hash of the develop recipe (EditStack base64) it was rendered from, so an
         edit made anywhere -- including by another Winnow instance or another machine --
         misses rather than showing pixels the user has moved on from.
      2. The length and last-modified time of the SOURCE IMAGE, because the recipe hash
         does not identify an image: presets, Paste Settings and multi-image edits give
         whole folders a byte-identical recipe. Without this, a path reused by a different
         image -- a rename done in Finder while Winnow was closed, a restore from backup,
         a card renumbered by the camera -- would serve the previous occupant's picture
         and every check downstream would call it correct.

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
    thread and from every one of ImageCache's decoderCount threads. One mutex guards the
    byte total, the id counter and the table. Each thread gets its own database connection
    (CacheDb), and WAL means a read on a decoder thread does not block the write on the
    GUI thread.

    NO PAYLOAD I/O HAPPENS UNDER THE MUTEX. A full-resolution entry is several MB, so
    holding the lock across the read in get() would serialise every decoder thread on one
    disk read at a time -- undoing the parallel read-ahead this cache exists to enable --
    and holding it across the multi-MB QSaveFile in put() would stall all of them once per
    image for the length of a background preview build. Both take the lock, decide, drop
    it, do the I/O, and take it again to record the result. get() documents the one window
    that opens and why both of its outcomes are safe.

    The long-running passes -- sweep(), reconcile() -- do not hold it across their I/O
    either. At 250,000 entries that would be a multi-second stall on every get(), which is
    a visible freeze in the loupe. They work in pages, taking the lock per page.
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

    /* True when path is the cache directory itself or anything inside it.

       The cache folder is browsable -- it lives under AppDataLocation and the user can
       navigate to it in the folder panel like any other folder -- but it is NOT a photo
       folder. Its files are named by an opaque id that only the index can attribute to an
       image, and reconcile() DELETES any file the index does not name. So a rename, a
       move, a trash or a sidecar written in here does not just edit a copy of a picture:
       it detaches previews from their images, and drops anything Winnow writes alongside
       them at the next launch. Every write path asks this first and refuses. See
       "The Preview Cache Folder Is Read-Only" in notes/Documentation.txt. */
    bool isCachePath(const QString &path) const;

    /* The one user-facing explanation of the refusal, so the popup, the menu tooltips
       and the G::issue warnings all say the same thing. */
    static QString readOnlyReason();

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

    /* Open the index (normally lazy, on first use). */
    void load();
    /* Fold the write-ahead log back into the database. Every mutation is already
       committed when it happens, so this is housekeeping, not a save -- the name is kept
       because MW::closeEvent and the sweep call it. */
    void save();

    qint64 totalBytes() const;
    int count() const;

    /* One row per folder that has cached devPreviews, for the Develop diagnostics
       report. live counts entries whose source image was present at the last sweep;
       the difference is images that are gone or on an unmounted volume. */
    struct FolderStat {
        QString folder;
        int count = 0;
        int live = 0;
        qint64 bytes = 0;
    };
    QList<FolderStat> folderStats() const;

private:
    DevPreviewCache() = default;
    Q_DISABLE_COPY(DevPreviewCache)

    /* One row of the devpreview table, as the code here wants it. */
    struct Entry {
        quint64 id = 0;         // names the file on disk: <id in hex>.jpg
        QByteArray blobHash;    // recipe this preview was rendered from
        qint64 bytes = 0;
        qint64 lastUsed = 0;    // seconds since epoch
        bool live = true;       // false => source file was missing at the last sweep
        /* Mount point, so the sweep can tell "gone" from "ejected". */
        QString volRoot;
        /* Identity of the source image when the preview was made -- see STALENESS above.
           Zero means NOT RECORDED: a row imported from the JSON index that predates it.
           Those are grandfathered rather than discarded -- re-rendering a quarter of a
           million previews to close a rare window is the worse trade -- and the first
           sweep stamps them. */
        qint64 srcSize = 0;
        qint64 srcMtime = 0;    // seconds since epoch
    };

    /* The index is opened lazily, on first use, rather than by an explicit init call from
       MW. An init call is a thing a future caller can forget, and forgetting it is silent
       and expensive: the cache reads as empty (so every prior-session preview misses) and
       nextId restarts at 1, so new entries OVERWRITE unrelated cache files. */
    void ensureLoadedLocked();
    /* The calling thread's connection, after ensureLoadedLocked. May be closed -- every
       caller checks isOpen() and degrades to "no cache". */
    QSqlDatabase dbLocked();
    /* One-time import of the JSON index this class used to keep, on the first open of a
       database that does not have one yet. */
    void migrateJsonIndexLocked(QSqlDatabase &db);

    QString dirLocked() const;
    QString filePathLocked(quint64 id) const;
    /* Takes the NORMALISED key (Cache/pathkey.h), not a path. */
    void removeLocked(QSqlDatabase &db, const QString &key);
    void evictLocked(QSqlDatabase &db);
    static QString volumeRootOf(const QString &path);
    static bool volumeMounted(const QString &volRoot);

    mutable QMutex mutex;
    QString dir;
    /* Re-derived from MAX(id) when the table is opened, so it can never restart at 1 and
       clobber the file belonging to another image. */
    quint64 nextId = 1;
    /* Running SUM(bytes), kept in memory so the cap can be tested on every put without a
       query. Re-derived on open. */
    qint64 bytes = 0;
    qint64 capBytes = 20LL * 1024 * 1024 * 1024;  // 20 GB; see G::devPreviewCacheMaxBytes
    bool loaded = false;
};

#endif // DEVPREVIEWCACHE_H
