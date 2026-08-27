#ifndef CACHEDB_H
#define CACHEDB_H

#include <QMutex>
#include <QSqlDatabase>
#include <QString>

/*
    Winnow's local index database -- one SQLite file under AppDataLocation holding every
    derived, rebuildable index the app keeps about images it has seen.

    WHAT LIVES HERE

    Today: the develop-preview index (Cache/devpreviewcache.cpp, table devpreview). It is
    deliberately not called "the preview database": the schema is versioned and additive,
    and the next tenant is expected to be indexed keywords, which want their own tables
    (keyword, image_keyword, very likely an FTS5 index over them) in the SAME file so that
    a keyword search and a preview lookup are one connection, one transaction boundary and
    one thing to back up, clear or repair.

    NOTHING IRREPLACEABLE GOES IN HERE. Everything in this file is derived from the images
    and their sidecars, so losing it costs re-rendering and re-scanning, never data. That
    is what lets the failure policy be "recreate it and move on". Develop recipes,
    ratings, labels and titles live in the sidecars beside the images, and must stay
    there.

    WHY SQLITE AND NOT THE JSON INDEX IT REPLACES

    The preview index used to be a JSON array rewritten in full on every put. At the few
    hundred entries that shipped first, that was genuinely cheaper than a dependency. At
    the 250,000 the cache is now sized for it is not: the array is tens of MB, holding it
    in memory costs ~50 MB of QString paths, every single put rewrites the whole thing
    (quadratic over a background preview build), and the ONE structure that can attribute
    a cache file to an image is a file that is replaced wholesale, over and over.

    SQLite gives an indexed lookup instead of a hash of every path, an O(1) write instead
    of a full rewrite, real transactions, and -- the reason that matters most here -- a
    file format that survives being extended. Keywords can be added as tables next year
    without touching how previews are stored.

    THREADING

    A QSqlDatabase connection may only be used from the thread that opened it, so this
    hands out ONE CONNECTION PER THREAD, opened on first use and closed when the thread
    ends. WAL is on, so a reader on a decoder thread never blocks the writer on the GUI
    thread. Callers still do their own locking for whatever invariants they hold in memory
    (DevPreviewCache guards its cached byte total); this class only guarantees that the
    connection you are handed is yours.

    FAILURE POLICY

    Every accessor returns a QSqlDatabase that may be closed -- if SQLite is unavailable,
    the file is unwritable, or the schema will not migrate. Callers must check isOpen()
    and degrade to "no cache" rather than refusing to run. A database that exists but
    cannot be opened is MOVED ASIDE (.corrupt) and recreated, because the alternative is a
    user who can never cache anything again and no way to tell them why.
*/
class CacheDb
{
public:
    static CacheDb &instance();

    /* Point at a database file. Closes every open connection first, so the next db() call
       opens the new one. Does NOT open anything itself -- opening is lazy, on first use,
       for the same reason the preview index loads lazily: an init call a future caller
       can forget is a silent, expensive failure. */
    void setPath(const QString &dbPath);
    QString path() const;

    /* The calling thread's connection, opened and migrated on first use. Check isOpen()
       on the result: a closed database is the documented "no cache" state, not an error
       to propagate. */
    QSqlDatabase db();

    /* Close the calling thread's connection and forget it; other threads are unaffected.
       Used by setPath and by tests. */
    void closeThisThread();

    /* The schema version this build writes (PRAGMA user_version). */
    static int schemaVersion();

private:
    CacheDb() = default;
    Q_DISABLE_COPY(CacheDb)

    /* Create or upgrade the schema in an already-open connection. Returns false if the
       database cannot be brought to schemaVersion(), which is the caller's cue to move
       the file aside. */
    static bool migrate(QSqlDatabase &db);
    static bool applyPragmas(QSqlDatabase &db);

    mutable QMutex mutex;   // guards dbPath/gen and the QSqlDatabase connection registry
    QString dbPath;
    /* Bumped by setPath. A connection carries the generation it was opened against, so
       a thread discovers on its next db() call that it points at a database that is no
       longer current -- there is no way to close another thread's connection here. */
    int gen = 0;
};

#endif // CACHEDB_H
