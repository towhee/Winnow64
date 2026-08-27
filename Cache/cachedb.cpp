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

constexpr int kSchemaVersion = 2;

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
    silently discard whatever the newer build was keeping. New tables -- keywords being
    the expected next one -- get a new version number and a new block below; existing
    tables are extended with ALTER TABLE ADD COLUMN, never redefined in place.
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

    if (!q.exec(QString("PRAGMA user_version = %1").arg(kSchemaVersion))) {
        db.rollback();
        return false;
    }
    return db.commit();
}
