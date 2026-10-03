#include "Datamodel/userdb.h"
#include "Cache/cachedb.h"
#include "Cache/catalog.h"
#include "Main/global.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSqlError>
#include <QSqlQuery>
#include <QStandardPaths>

namespace {

const char *kDbName = "userdata.db";
// the file's name before it held more than collections
const char *kOldDbName = "collections.db";

/*  The schema this build writes (PRAGMA user_version). ADDITIVE ONLY, as for the index:
    a new table or column is a new version and a new block in migrate().
      1  node, member (collections, queries, places)
      2  vocab (the keyword vocabulary), imported once from index.db */
constexpr int kSchemaVersion = 2;

}  // namespace

UserDb &UserDb::instance()
{
/*
    Never deleted: it outlives the QSqlDatabase registry's teardown order questions that
    way (see Cache/cachedb.cpp on QThreadStorage at exit).
*/
    static UserDb *s = new UserDb;
    return *s;
}

void UserDb::setPath(const QString &p)
{
    if (G::isLogger) G::log("UserDb::setPath", p);
    if (!connName.isEmpty()) {
        {
            QSqlDatabase d = QSqlDatabase::database(connName, false);
            if (d.isOpen()) d.close();
        }
        QSqlDatabase::removeDatabase(connName);
        connName.clear();
    }
    dbPath = p;
    opened = false;
    failed = false;
    lastError.clear();
}

QString UserDb::path() const
{
    if (!dbPath.isEmpty()) return dbPath;
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
           + "/" + kDbName;
}

void UserDb::adoptCollectionsDb(const QString &target)
{
/*
    collections.db -> userdata.db, once. Only when the new file does not exist yet and
    the old one does, in the same folder. The -wal and -shm files go with it: a WAL
    database whose last session did not checkpoint keeps recent writes in -wal, and
    renaming the main file alone would strand them. Nothing has the file open yet, so
    the rename works on Windows as well.

    A FAILED RENAME IS NOT A LOSS: db() then opens collections.db where it is, and the
    next launch tries again.
*/
    if (QFileInfo::exists(target)) return;
    const QString old = QFileInfo(target).absolutePath() + "/" + kOldDbName;
    if (!QFileInfo::exists(old)) return;

    /*  THE SIDECARS FIRST, THE MAIN FILE LAST. A second Winnow starting at the same
        moment decides by whether userdata.db exists; if the main file moved first it
        could open userdata.db before its -wal had followed, miss the writes still in
        it, and start a new -wal the old one could then not be renamed onto. With the
        main file last, userdata.db appears with its WAL already beside it. A failed
        main rename puts the sidecars back. */
    QStringList moved;
    for (const char *suffix : {"-wal", "-shm"}) {
        const QString from = old + suffix;
        if (QFileInfo::exists(from) && QFile::rename(from, target + suffix))
            moved << suffix;
    }
    if (!QFile::rename(old, target)) {
        for (const QString &suffix : std::as_const(moved))
            QFile::rename(target + suffix, old + suffix);
        G::issue("Warning", "Could not rename collections.db to userdata.db; using it "
                 "where it is.", "UserDb::adoptCollectionsDb", -1, old);
    }
}

QSqlDatabase UserDb::db()
{
/*
    Open on first use. A failure is REMEMBERED rather than retried on every call -- the
    panels ask often, and a file that would not open a moment ago will not open now --
    and the file is left exactly as it was found. setPath clears the memory.
*/
    if (opened) return QSqlDatabase::database(connName, false);
    if (failed) return QSqlDatabase();

    QString p = path();
    QDir().mkpath(QFileInfo(p).absolutePath());
    adoptCollectionsDb(p);
    if (!QFileInfo::exists(p)) {
        // the rename failed: keep using the old file rather than starting an empty one
        const QString old = QFileInfo(p).absolutePath() + "/" + kOldDbName;
        if (QFileInfo::exists(old)) p = old;
    }

    connName = QString("winnow_userdata_%1").arg(reinterpret_cast<quintptr>(this));
    QSqlDatabase d = QSqlDatabase::addDatabase("QSQLITE", connName);
    d.setDatabaseName(p);
    if (!d.open()) {
        lastError = tr("Your collections, queries, places and keyword list could not be "
                       "opened: %1").arg(d.lastError().text());
        failed = true;
        G::issue("Warning", lastError, "UserDb::db", -1, p);
        return QSqlDatabase();
    }
    QSqlQuery q(d);
    /*  BUSY TIMEOUT FIRST, before anything that takes a lock -- journal_mode included.
        Another Winnow (or a test instance) opening the same file at the same moment is
        then a short wait, not a "database is locked" that would leave these features
        unavailable for the whole session. */
    q.exec("PRAGMA busy_timeout = 5000");
    /*  The foreign keys ARE the cascade: deleting a node takes its children and its
        memberships with it, and deleting a vocabulary node takes its branch, each in
        one statement. SQLite has them off per connection unless asked. */
    q.exec("PRAGMA foreign_keys = ON");
    q.exec("PRAGMA journal_mode = WAL");
    q.exec("PRAGMA synchronous = NORMAL");
    if (!migrate(d)) {
        d.close();
        failed = true;
        G::issue("Warning", lastError, "UserDb::db", -1, p);
        return QSqlDatabase();
    }
    opened = true;
    return d;
}

bool UserDb::migrate(QSqlDatabase &d)
{
    QSqlQuery q(d);
    if (!q.exec("PRAGMA user_version") || !q.next()) {
        lastError = tr("Your collections, queries, places and keyword list could not be "
                       "read: %1").arg(q.lastError().text());
        return false;
    }
    const int version = q.value(0).toInt();
    q.finish();
    /*  A NEWER FILE IS LEFT ALONE. The index would be moved aside and rebuilt; this
        cannot be rebuilt, so an older Winnow simply does without these features until
        the newer one is back. */
    if (version > kSchemaVersion) {
        lastError = tr("Your collections, queries, places and keyword list were saved by "
                       "a newer version of Winnow, so this version leaves them "
                       "untouched.");
        return false;
    }
    if (version == kSchemaVersion) return true;

    auto runAll = [&](const QStringList &ddl) {
        for (const QString &s : ddl) {
            if (!q.exec(s)) {
                lastError = tr("Your collections, queries, places and keyword list could "
                               "not be set up: %1").arg(q.lastError().text());
                return false;
            }
        }
        return true;
    };

    d.transaction();
    if (version < 1) {
        const QStringList ddl = {
            /*  One table for every kind. `definition` is a Query's saved search or a
                Place's area; a collection leaves it empty. `position` orders siblings
                -- the order the user dragged them into, not alphabetical, as in
                Lightroom. */
            "CREATE TABLE IF NOT EXISTS node ("
            "  id         INTEGER PRIMARY KEY,"
            "  parent     INTEGER REFERENCES node(id) ON DELETE CASCADE,"
            "  kind       INTEGER NOT NULL DEFAULT 0,"
            "  name       TEXT    NOT NULL,"
            "  position   INTEGER NOT NULL DEFAULT 0,"
            "  definition TEXT    NOT NULL DEFAULT '',"
            "  created    INTEGER NOT NULL DEFAULT 0,"
            "  modified   INTEGER NOT NULL DEFAULT 0)",
            "CREATE INDEX IF NOT EXISTS node_parent ON node(parent)",
            /*  pathkey is the identity (it is what the catalog's image.pathkey holds);
                path is kept so the file is readable, and so a move can be followed. */
            "CREATE TABLE IF NOT EXISTS member ("
            "  node     INTEGER NOT NULL REFERENCES node(id) ON DELETE CASCADE,"
            "  pathkey  TEXT    NOT NULL,"
            "  path     TEXT    NOT NULL,"
            "  added    INTEGER NOT NULL DEFAULT 0,"
            "  PRIMARY KEY (node, pathkey)) WITHOUT ROWID",
            "CREATE INDEX IF NOT EXISTS member_pathkey ON member(pathkey)",
        };
        if (!runAll(ddl)) { d.rollback(); return false; }
    }
    if (version < 2) {
        /*  THE KEYWORD VOCABULARY, column for column as index.db had it (Cache/
            cachedb.cpp), so the import below is a straight copy and KeywordVocab's SQL
            did not change. Keyed by the folded PATH -- the node's identity -- with the
            leaf name indexed for the type-ahead completer. */
        const QStringList ddl = {
            "CREATE TABLE IF NOT EXISTS vocab ("
            "  id         INTEGER PRIMARY KEY,"
            "  name       TEXT    NOT NULL,"
            "  namefold   TEXT    NOT NULL,"
            "  path       TEXT    NOT NULL,"
            "  pathfold   TEXT    NOT NULL,"
            "  parent     INTEGER REFERENCES vocab(id) ON DELETE CASCADE,"
            "  synonyms   TEXT    NOT NULL DEFAULT '',"
            "  exportable INTEGER NOT NULL DEFAULT 1,"
            "  sort       INTEGER NOT NULL DEFAULT 0)",
            "CREATE UNIQUE INDEX IF NOT EXISTS vocab_pathkey ON vocab(pathfold)",
            "CREATE INDEX IF NOT EXISTS vocab_namefold ON vocab(namefold)",
            "CREATE INDEX IF NOT EXISTS vocab_parent   ON vocab(parent)",
        };
        if (!runAll(ddl)) { d.rollback(); return false; }
    }
    if (!q.exec(QString("PRAGMA user_version = %1").arg(kSchemaVersion))) {
        lastError = q.lastError().text();
        d.rollback();
        return false;
    }
    if (!d.commit()) {
        lastError = d.lastError().text();
        return false;
    }

    /*  OUTSIDE THE TRANSACTION: SQLite refuses ATTACH inside one. The schema step is
        committed first, so this runs exactly once per file -- a vocabulary the user
        later empties is never refilled from the old index. A failed import leaves an
        empty vocabulary (the old rows stay in index.db) and is logged, not fatal. */
    if (version < 2) importIndexVocab(d);
    return true;
}

bool UserDb::importIndexVocab(QSqlDatabase &d)
{
/*
    Copy index.db's vocab table, ids and all, so every parent link stays valid. index.db
    keeps its copy (its schema is additive only) but nothing reads it any more.

    The index's own migrations must have run first -- an old index seeds its vocabulary
    while migrating -- so the catalog is asked to open it before its path is read.
*/
    if (G::isLogger) G::log("UserDb::importIndexVocab");

    QString idx = legacyIndexPath;
    if (idx.isEmpty()) {
        Catalog::instance().isAvailable();      // opens and migrates index.db
        idx = CacheDb::instance().path();
    }
    // ATTACH creates a missing file; there is nothing to import from one anyway
    if (idx.isEmpty() || !QFileInfo::exists(idx)) return true;

    QSqlQuery q(d);
    q.prepare("ATTACH DATABASE ? AS idx");
    q.addBindValue(idx);
    if (!q.exec()) {
        G::issue("Warning", "Could not read the keyword list from the index: "
                 + q.lastError().text(), "UserDb::importIndexVocab", -1, idx);
        return false;
    }

    bool ok = true;
    int imported = 0;
    if (q.exec("SELECT 1 FROM idx.sqlite_master WHERE type = 'table' AND name = 'vocab'")
        && q.next())
    {
        q.finish();
        /*  FOREIGN KEYS OFF FOR THE COPY. Shallowest first already puts every parent
            ahead of its children, but an orphan row (a parent id the index no longer
            holds) would abort the whole statement, and KeywordVocab::reload adopts
            such a row at the top level rather than losing it. */
        q.exec("PRAGMA foreign_keys = OFF");
        ok = q.exec("INSERT OR IGNORE INTO vocab"
                    " (id, name, namefold, path, pathfold, parent, synonyms, exportable,"
                    "  sort)"
                    " SELECT id, name, namefold, path, pathfold, parent, synonyms,"
                    "  exportable, sort FROM idx.vocab"
                    " ORDER BY (LENGTH(path) - LENGTH(REPLACE(path, '|', ''))), id");
        if (ok) imported = q.numRowsAffected();
        else {
            G::issue("Warning", "Could not copy the keyword list from the index: "
                     + q.lastError().text(), "UserDb::importIndexVocab", -1, idx);
        }
    }
    q.finish();
    q.exec("PRAGMA foreign_keys = ON");
    q.exec("DETACH DATABASE idx");

    if (G::isLogger) G::log("UserDb::importIndexVocab", QString::number(imported));
    return ok;
}
