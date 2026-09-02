#include "Cache/cachedb.h"
#include "Cache/pathkey.h"
#include "Main/global.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QFile>
#include <QFileInfo>
#include <QSqlError>
#include <QSqlQuery>
#include <QThread>
#include <QThreadStorage>

namespace {

constexpr int kSchemaVersion = 5;

/*
    One connection per thread, closed when the thread ends.

    QSqlDatabase connections are owned by the thread that opened them, and the decoder
    threads that read previews come and go with the thread pool. QThreadStorage deletes
    this on thread exit, which is the only place a connection can be removed without Qt
    complaining that it is still in use.

    generation is the copy of CacheDb::gen this connection was opened against. setPath
    bumps that counter rather than reaching into other threads' connections, so a thread
    whose connection points at a database that is no longer current discovers it on its
    next db() call and reopens.
*/
struct Conn
{
    QString name;
    int generation = -1;

    void release()
    {
        if (name.isEmpty()) return;
        /*
            NOTHING TO DO ONCE THE APPLICATION IS GONE. QThreadStorage deletes this when
            the thread ends, and for thread-pool threads -- which is most of them, since
            the decoders come from a pool -- that happens DURING QCoreApplication
            teardown. Qt's SQL registry is being destroyed at that point, so every call
            below warns "QSqlDatabase requires a QCoreApplication" (once per thread, on
            every quit) and does nothing useful. The connection dies with the process
            either way; there is nothing to leak.

            A pool thread reaped while the app is still running takes the real path below,
            which is the case this cleanup exists for.
        */
        if (!QCoreApplication::instance()) {
            name.clear();
            generation = -1;
            return;
        }
        {
            /* The QSqlDatabase copy must go out of scope before removeDatabase, or Qt
               warns that the connection is still in use and leaks it. */
            QSqlDatabase d = QSqlDatabase::database(name, /*open*/ false);
            if (d.isValid() && d.isOpen()) d.close();
        }
        QSqlDatabase::removeDatabase(name);
        name.clear();
        generation = -1;
    }

    ~Conn() { release(); }
};

QThreadStorage<Conn *> tlsConn;

Conn *threadConn()
{
    if (!tlsConn.hasLocalData()) tlsConn.setLocalData(new Conn);
    return tlsConn.localData();
}

/* Move a database that cannot be opened or migrated out of the way, with its WAL
   sidecars, so the next open creates a healthy one. Best effort: if the rename fails
   there is nothing further to try, and the caller degrades to "no cache". */
void moveAside(const QString &dbPath)
{
    const QString stamp = QString::number(QDateTime::currentSecsSinceEpoch());
    for (const QString &suffix : {QString(), QString("-wal"), QString("-shm")}) {
        const QString from = dbPath + suffix;
        if (!QFileInfo::exists(from)) continue;
        QFile::rename(from, dbPath + suffix + ".corrupt." + stamp);
    }
    QString msg = "Local index database was unreadable; a new one was created.";
    G::issue("Warning", msg, "CacheDb::moveAside", -1, dbPath);
}

}  // namespace

CacheDb &CacheDb::instance()
{
    static CacheDb db;
    return db;
}

int CacheDb::schemaVersion()
{
    return kSchemaVersion;
}

void CacheDb::setPath(const QString &p)
{
    {
        QMutexLocker lk(&mutex);
        if (dbPath == p) return;
        dbPath = p;
        /* Every thread's connection is now stale. They cannot be closed from here -- a
           connection belongs to its own thread -- so bump the generation and let each
           thread reopen on its next call. */
        ++gen;
    }
    closeThisThread();
}

QString CacheDb::path() const
{
    QMutexLocker lk(&mutex);
    return dbPath;
}

void CacheDb::closeThisThread()
{
    if (!tlsConn.hasLocalData()) return;
    tlsConn.localData()->release();
}

QSqlDatabase CacheDb::db()
{
/*
    The calling thread's connection, opened on first use.

    Returns a closed QSqlDatabase when SQLite is unavailable or the file cannot be brought
    up to the current schema. That is a supported state, not an error: the preview cache
    simply misses on everything and the app runs without one.
*/
    QString p;
    int wantGen;
    {
        QMutexLocker lk(&mutex);
        p = dbPath;
        wantGen = gen;
    }
    if (p.isEmpty()) return QSqlDatabase();

    Conn *c = threadConn();
    if (c->generation == wantGen && !c->name.isEmpty()) {
        QSqlDatabase open = QSqlDatabase::database(c->name, /*open*/ true);
        if (open.isOpen()) return open;
        c->release();               // went away underneath us; fall through and reopen
    }
    c->release();

    if (!QSqlDatabase::isDriverAvailable("QSQLITE")) {
        QString msg = "SQLite driver unavailable; running without a local index.";
        G::issue("Warning", msg, "CacheDb::db", -1, p);
        return QSqlDatabase();
    }

    /* addDatabase and removeDatabase share a process-wide registry, and the open/repair
       path below must not race another thread renaming the same file. One lock covers
       both. */
    QMutexLocker lk(&mutex);
    if (p != dbPath) p = dbPath;    // setPath landed while we were unlocked

    const QString name = QString("winnow_cache_%1_%2")
                             .arg(wantGen)
                             .arg(quintptr(QThread::currentThreadId()), 0, 16);

    for (int attempt = 0; attempt < 2; ++attempt) {
        QSqlDatabase d = QSqlDatabase::addDatabase("QSQLITE", name);
        d.setDatabaseName(p);
        if (d.open() && applyPragmas(d) && migrate(d)) {
            c->name = name;
            c->generation = wantGen;
            return d;
        }

        const QString err = d.lastError().text();
        {
            QSqlDatabase dead = d;
            dead.close();
        }
        d = QSqlDatabase();
        QSqlDatabase::removeDatabase(name);

        if (attempt == 0) {
            moveAside(p);           // and try once more against a fresh file
            continue;
        }
        QString msg = "Could not open the local index database: " + err;
        G::issue("Warning", msg, "CacheDb::db", -1, p);
    }
    return QSqlDatabase();
}

bool CacheDb::applyPragmas(QSqlDatabase &db)
{
/*
    WAL so a preview read on a decoder thread never blocks the write on the GUI thread,
    and synchronous=NORMAL because everything in this file is rebuildable -- paying for a
    full fsync per commit would buy durability we do not need at a cost we would feel on
    every develop flush. busy_timeout covers the brief writer overlap WAL still has.
*/
    QSqlQuery q(db);
    return q.exec("PRAGMA journal_mode = WAL")
           && q.exec("PRAGMA synchronous = NORMAL")
           && q.exec("PRAGMA foreign_keys = ON")
           && q.exec("PRAGMA busy_timeout = 5000");
}

bool CacheDb::migrate(QSqlDatabase &db)
{
/*
    Bring the schema to kSchemaVersion, using PRAGMA user_version as the marker.

    ADDITIVE ONLY, and the version is a floor rather than an exact match: a file written
    by a NEWER Winnow is refused (returning false, so it is moved aside and rebuilt)
    rather than downgraded, because a rebuild costs re-rendering while a downgrade would
    silently discard whatever the newer build was keeping. New tables get a new version
    number and a new block below (version 3 added the catalog); existing tables are
    extended with ALTER TABLE ADD COLUMN, never redefined in place.
*/
    QSqlQuery q(db);

    int version = 0;
    if (q.exec("PRAGMA user_version") && q.next()) version = q.value(0).toInt();
    if (version > kSchemaVersion) return false;
    if (version == kSchemaVersion) return true;

    if (!db.transaction()) return false;

    if (version < 1) {
        /* devpreview: the develop-preview index. One row per image that has a cached
           full-resolution preview; id names the file on disk as <id in hex>.jpg.

           folder is stored rather than derived so the diagnostics report can GROUP BY it
           instead of splitting a quarter of a million paths in memory.

           The evict index is (live, used) because that is exactly the eviction order --
           demoted entries first, then least recently used -- so choosing what to drop is
           an index scan of only the rows actually being dropped. */
        const char *ddl[] = {
            "CREATE TABLE IF NOT EXISTS devpreview ("
            "  id       INTEGER PRIMARY KEY,"
            "  path     TEXT    NOT NULL,"
            "  folder   TEXT    NOT NULL,"
            "  hash     TEXT    NOT NULL,"
            "  bytes    INTEGER NOT NULL,"
            "  used     INTEGER NOT NULL,"
            "  live     INTEGER NOT NULL DEFAULT 1,"
            "  vol      TEXT    NOT NULL DEFAULT '',"
            "  srcsize  INTEGER NOT NULL DEFAULT 0,"
            "  srcmtime INTEGER NOT NULL DEFAULT 0)",
            "CREATE UNIQUE INDEX IF NOT EXISTS devpreview_path ON devpreview(path)",
            "CREATE INDEX IF NOT EXISTS devpreview_evict ON devpreview(live, used)",
            "CREATE INDEX IF NOT EXISTS devpreview_folder ON devpreview(folder)",
        };
        for (const char *sql : ddl) {
            if (!q.exec(QString::fromLatin1(sql))) {
                db.rollback();
                return false;
            }
        }
    }

    if (version < 2) {
        /* pathkey: the NORMALISED lookup key (Cache/pathkey.h). path keeps whatever
           spelling the filesystem gave us -- it is what gets stat'd and what the
           diagnostics report shows -- while every WHERE clause matches on pathkey.

           The backfill has to happen in C++ because the normalisation is not expressible
           in SQL (SQLite's LOWER is ASCII-only, and neither cleanPath nor NFC has an
           equivalent at all), which is why this block computes and writes the column
           row by row before the unique index goes on. */
        const char *addCol =
            "ALTER TABLE devpreview ADD COLUMN pathkey TEXT NOT NULL DEFAULT ''";
        if (!q.exec(QString::fromLatin1(addCol))) {
            db.rollback();
            return false;
        }

        QList<QPair<qint64, QString>> rows;
        {
            QSqlQuery sel(db);
            if (!sel.exec("SELECT id, path FROM devpreview")) {
                db.rollback();
                return false;
            }
            while (sel.next())
                rows.append({sel.value(0).toLongLong(), sel.value(1).toString()});
        }
        {
            QSqlQuery upd(db);
            upd.prepare("UPDATE devpreview SET pathkey = ? WHERE id = ?");
            for (const auto &r : rows) {
                upd.addBindValue(cachePathKey(r.second));
                upd.addBindValue(r.first);
                if (!upd.exec()) {
                    db.rollback();
                    return false;
                }
            }
        }

        /* Two rows that were distinct byte-exactly can fold to ONE key -- the same image
           reached by two spellings is exactly the waste this column exists to stop. The
           unique index below would FAIL on them, and a failed migration costs the user
           their whole cache, so collapse them first. Highest id wins: ids are monotonic,
           so that is the most recently created row. The payloads of the dropped rows
           become strays and reconcile() collects them. */
        const char *dedupe =
            "DELETE FROM devpreview WHERE id NOT IN"
            " (SELECT MAX(id) FROM devpreview GROUP BY pathkey)";
        if (!q.exec(QString::fromLatin1(dedupe))) {
            db.rollback();
            return false;
        }

        if (!q.exec("DROP INDEX IF EXISTS devpreview_path")
            || !q.exec("CREATE UNIQUE INDEX IF NOT EXISTS devpreview_pathkey"
                       " ON devpreview(pathkey)")) {
            db.rollback();
            return false;
        }
    }

    if (version < 3) {
        /* THE CATALOG. Everything Winnow has learned about images it has seen, so a
           keyword or title search can answer across folders instead of only inside the
           one currently loaded. See notes/Documentation.txt "Keywords and Cataloguing".

           STILL DERIVED, STILL REBUILDABLE. Every column here is re-readable from the
           image and its sidecar, which is what keeps the failure policy of this whole
           file ("move it aside and rebuild") honest. The ONE thing that would not be
           rebuildable -- the list of folders the user nominated for background scanning
           -- deliberately lives in QSettings instead, because moveAside() discards this
           file without asking and user intent is not derived data.

           image.folder is denormalised, like devpreview.folder, so a folder category or a
           prune-by-folder is an index seek rather than a quarter of a million paths
           taken apart in memory. */
        const char *ddl[] = {
            "CREATE TABLE IF NOT EXISTS image ("
            "  id           INTEGER PRIMARY KEY,"
            "  pathkey      TEXT    NOT NULL,"
            "  path         TEXT    NOT NULL,"
            "  folder       TEXT    NOT NULL,"
            "  vol          TEXT    NOT NULL DEFAULT '',"
            "  filename     TEXT    NOT NULL DEFAULT '',"
            "  ext          TEXT    NOT NULL DEFAULT '',"
            /* Freshness. sidecarmtime is what makes an edit made in Lightroom -- which
               rewrites the .xmp and never touches the raw -- reindex on next sight. */
            "  srcsize      INTEGER NOT NULL DEFAULT 0,"
            "  srcmtime     INTEGER NOT NULL DEFAULT 0,"
            "  sidecarmtime INTEGER NOT NULL DEFAULT 0,"
            "  indexed      INTEGER NOT NULL DEFAULT 0,"
            "  live         INTEGER NOT NULL DEFAULT 1,"
            /* Category values and range filters. */
            "  captured     INTEGER,"
            "  rating       INTEGER NOT NULL DEFAULT 0,"
            "  label        TEXT    NOT NULL DEFAULT '',"
            "  pick         INTEGER NOT NULL DEFAULT 0,"
            "  title        TEXT    NOT NULL DEFAULT '',"
            "  creator      TEXT    NOT NULL DEFAULT '',"
            "  copyright    TEXT    NOT NULL DEFAULT '',"
            "  make         TEXT    NOT NULL DEFAULT '',"
            "  model        TEXT    NOT NULL DEFAULT '',"
            "  lens         TEXT    NOT NULL DEFAULT '',"
            "  iso          INTEGER NOT NULL DEFAULT 0,"
            "  aperture     REAL    NOT NULL DEFAULT 0,"
            "  shutter      REAL    NOT NULL DEFAULT 0,"
            "  focallength  REAL    NOT NULL DEFAULT 0,"
            "  width        INTEGER NOT NULL DEFAULT 0,"
            "  height       INTEGER NOT NULL DEFAULT 0,"
            "  gpscoord     TEXT    NOT NULL DEFAULT '')",
            "CREATE UNIQUE INDEX IF NOT EXISTS image_pathkey ON image(pathkey)",
            "CREATE INDEX IF NOT EXISTS image_folder   ON image(folder)",
            "CREATE INDEX IF NOT EXISTS image_captured ON image(captured)",
            "CREATE INDEX IF NOT EXISTS image_model    ON image(model)",
            "CREATE INDEX IF NOT EXISTS image_lens     ON image(lens)",
            "CREATE INDEX IF NOT EXISTS image_live     ON image(live, id)",

            /* Keywords, normalised. A flat dc:subject keyword has path = ''. A
               hierarchical one carries its full Lightroom path, and one row is inserted
               per ANCESTOR as well, wired by parent -- which is what lets a search for
               "Wildlife" reach an image whose only keyword is "Heron".

               SUPERSEDED AT VERSION 4, which re-keys this table on the name alone and
               blanks path/pathfold/parent. This block stays as written because it
               describes what a version 3 FILE contains, which is what the migration below
               has to read; it is not what a current database looks like. */
            "CREATE TABLE IF NOT EXISTS keyword ("
            "  id       INTEGER PRIMARY KEY,"
            "  name     TEXT NOT NULL,"
            "  namefold TEXT NOT NULL,"
            "  path     TEXT NOT NULL DEFAULT '',"
            "  pathfold TEXT NOT NULL DEFAULT '',"
            "  parent   INTEGER REFERENCES keyword(id) ON DELETE SET NULL)",
            "CREATE UNIQUE INDEX IF NOT EXISTS keyword_key"
            " ON keyword(pathfold, namefold)",
            "CREATE INDEX        IF NOT EXISTS keyword_name ON keyword(namefold)",

            /* PRAGMA foreign_keys is ON (applyPragmas), so these cascades really fire:
               deleting an image row takes its keyword links with it. */
            "CREATE TABLE IF NOT EXISTS image_keyword ("
            "  image_id   INTEGER NOT NULL REFERENCES image(id)   ON DELETE CASCADE,"
            "  keyword_id INTEGER NOT NULL REFERENCES keyword(id) ON DELETE CASCADE,"
            "  PRIMARY KEY (image_id, keyword_id)) WITHOUT ROWID",
            "CREATE INDEX IF NOT EXISTS image_keyword_kw"
            " ON image_keyword(keyword_id, image_id)",

            /* Free text. rowid == image.id, so a hit joins straight back with no mapping
               table. A PLAIN fts5 table, not contentless and not external-content:
               contentless needs the original text handed back on every delete (so a
               second copy is kept anyway) and external-content cannot span the
               image_keyword join. The duplicate text costs ~200 bytes an image and buys
               prefix, AND/OR/NOT, phrase and BM25 ranking with no query code of ours. */
            "CREATE VIRTUAL TABLE IF NOT EXISTS image_fts USING fts5("
            "  keywords, title, creator, copyright, gear, filename,"
            "  tokenize = 'unicode61 remove_diacritics 2')",
        };
        for (const char *sql : ddl) {
            if (!q.exec(QString::fromLatin1(sql))) {
                db.rollback();
                return false;
            }
        }
    }

    if (version < 4) {
        /* FLAT KEYWORDS. A keyword's identity becomes its NAME, where schema 3 keyed it
           on (path, name) and so stored a hierarchical tag TWICE -- once as the flat
           dc:subject leaf and once as the hierarchy node -- which put the same keyword in
           the category list twice with the image counts split between the entries. See
           Metadata/keywordflatten.h for why flat, and notes/Documentation.txt.

           THIS MIGRATES IN PLACE AND RE-READS NOTHING. Everything needed is already here:
           schema 3's writer linked the image to every ANCESTOR as well as the leaf, so
           the flat link set is already correct and only the rows need merging. Rebuilding
           instead would mean re-parsing an entire library to recover facts the database
           already holds.

           ORDER MATTERS. Contexts are derived BEFORE the surplus rows are deleted,
           because they are derived from those rows' parent pointers. */
        const char *ddl[] = {
            /* Which parents a keyword name has been seen under. This is all that remains
               of the hierarchy, and it exists for one purpose: a name recorded under more
               than one parent is AMBIGUOUS ("Vancouver" under both Canada and USA), which
               the docks colour and the user resolves with an exclude filter. */
            "CREATE TABLE IF NOT EXISTS keyword_context ("
            "  keyword_id INTEGER NOT NULL REFERENCES keyword(id) ON DELETE CASCADE,"
            "  parent_id  INTEGER NOT NULL REFERENCES keyword(id) ON DELETE CASCADE,"
            "  PRIMARY KEY (keyword_id, parent_id)) WITHOUT ROWID",
            "CREATE INDEX IF NOT EXISTS keyword_context_kw"
            " ON keyword_context(keyword_id)",

            /* old id -> the id that survives for that name. MIN(id) is arbitrary but
               stable; whichever row wins, its name column already holds the leaf name,
               because schema 3 stored a hierarchy node's name as its leaf. */
            "CREATE TEMP TABLE kw_canon ("
            "  old_id INTEGER PRIMARY KEY,"
            "  new_id INTEGER NOT NULL)",
            "INSERT INTO kw_canon (old_id, new_id)"
            " SELECT k.id,"
            "        (SELECT MIN(k2.id) FROM keyword k2 WHERE k2.namefold = k.namefold)"
            " FROM keyword k",

            /* Contexts, from the old parent pointers, mapped through the canonical ids.
               The new_id <> parent guard drops a node that would end up its own parent
               (a path like "A|A"), which is not a meaningful ambiguity. */
            "INSERT OR IGNORE INTO keyword_context (keyword_id, parent_id)"
            " SELECT c.new_id, p.new_id"
            " FROM keyword k"
            " JOIN kw_canon c ON c.old_id = k.id"
            " JOIN kw_canon p ON p.old_id = k.parent"
            " WHERE k.parent IS NOT NULL AND c.new_id <> p.new_id",

            /* Re-point the image links, then drop the ones that pointed at a row about to
               go. INSERT OR IGNORE because two rows for the same name on the same image
               -- exactly the Lightroom double this migration exists to remove -- collapse
               onto the (image_id, keyword_id) primary key. */
            "INSERT OR IGNORE INTO image_keyword (image_id, keyword_id)"
            " SELECT ik.image_id, c.new_id"
            " FROM image_keyword ik"
            " JOIN kw_canon c ON c.old_id = ik.keyword_id"
            " WHERE c.new_id <> ik.keyword_id",
            "DELETE FROM image_keyword WHERE keyword_id IN"
            " (SELECT old_id FROM kw_canon WHERE new_id <> old_id)",

            "DELETE FROM keyword WHERE id IN"
            " (SELECT old_id FROM kw_canon WHERE new_id <> old_id)",

            /* One row per name now, so the name alone can carry the unique index. */
            "DROP INDEX IF EXISTS keyword_key",
            "CREATE UNIQUE INDEX IF NOT EXISTS keyword_namekey ON keyword(namefold)",

            /* path/pathfold/parent are DEAD from here on -- kept as columns because this
               file's rule is additive-only (a dropped column cannot be walked back if an
               older build opens the file), but blanked so nothing can read them and
               believe the hierarchy is still maintained. keyword_context is the only
               record of it now. */
            "UPDATE keyword SET path = '', pathfold = '', parent = NULL",

            "DROP TABLE kw_canon",
        };
        for (const char *sql : ddl) {
            if (!q.exec(QString::fromLatin1(sql))) {
                db.rollback();
                return false;
            }
        }
    }

    if (version < 5) {
        /* thumb: the browsing thumbnail, so scrolling an unvisited region of a
           250,000-image catalog does not have to open a file per row. Keyed on
           pathkey (Cache/pathkey.h), the same key the image table uses.

           THE PAYLOAD IS A BLOB, WHICH IS THE OPPOSITE OF WHAT devpreview DOES,
           and the difference is the payload size rather than a change of mind. A
           devPreview is a full-resolution JPEG at one to three megabytes, where a
           file is the right container and the database would be doing nothing but
           adding a copy. A thumbnail is ten to thirty kilobytes; at a quarter of a
           million of them that is 250,000 files and inodes to hold a few
           gigabytes, and the per-file open dominates the read. SQLite is faster
           than the filesystem for blobs of roughly this size and its page cache
           serves a scroll from memory, which is exactly the access pattern here.

           The columns otherwise mirror devpreview on purpose, so the two share one
           eviction and sweep POLICY even though they do not share a container:
           (live, used) is the eviction order -- demoted entries first, then least
           recently used -- srcsize/srcmtime are the staleness stamp that catches an
           image edited outside Winnow, and vol lets the sweep tell "deleted" from
           "that disk is not plugged in". folder is stored rather than derived so a
           diagnostics report can GROUP BY it. */
        const char *ddl[] = {
            "CREATE TABLE IF NOT EXISTS thumb ("
            "  pathkey  TEXT    PRIMARY KEY,"
            "  path     TEXT    NOT NULL,"
            "  folder   TEXT    NOT NULL,"
            "  jpg      BLOB    NOT NULL,"
            "  w        INTEGER NOT NULL DEFAULT 0,"
            "  h        INTEGER NOT NULL DEFAULT 0,"
            "  bytes    INTEGER NOT NULL DEFAULT 0,"
            "  used     INTEGER NOT NULL DEFAULT 0,"
            "  live     INTEGER NOT NULL DEFAULT 1,"
            "  vol      TEXT    NOT NULL DEFAULT '',"
            "  srcsize  INTEGER NOT NULL DEFAULT 0,"
            "  srcmtime INTEGER NOT NULL DEFAULT 0)",
            "CREATE INDEX IF NOT EXISTS thumb_evict ON thumb(live, used)",
            "CREATE INDEX IF NOT EXISTS thumb_folder ON thumb(folder)",
        };
        for (const char *sql : ddl) {
            if (!q.exec(QString::fromLatin1(sql))) {
                db.rollback();
                return false;
            }
        }
    }

    if (!q.exec(QString("PRAGMA user_version = %1").arg(kSchemaVersion))) {
        db.rollback();
        return false;
    }
    return db.commit();
}
