#include "Datamodel/collectionstore.h"
#include "Cache/pathkey.h"
#include "Main/global.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QSet>
#include <QSqlError>
#include <QSqlQuery>
#include <QStandardPaths>
#include <QThread>
#include <QVariant>

namespace {

const char *kDbName = "collections.db";

/*  The schema this build writes (PRAGMA user_version). ADDITIVE ONLY, as for the index:
    a new table or column is a new version and a new block in migrate(). */
constexpr int kSchemaVersion = 1;

qint64 nowSecs() { return QDateTime::currentSecsSinceEpoch(); }

/*  A top-level node is NULL in the table (so the foreign key has nothing to check) and
    0 in memory. */
QVariant parentBind(qint64 parent) { return parent > 0 ? QVariant(parent) : QVariant(); }

}  // namespace

CollectionStore &CollectionStore::instance()
{
/*
    ON THE GUI THREAD, whoever asks first. onMoved/onDeleted hop to this object's thread,
    so it must be the application's even if a file-operation thread happened to be the
    first caller. Never deleted: it outlives the QSqlDatabase registry's teardown order
    questions that way (see Cache/cachedb.cpp on QThreadStorage at exit).
*/
    static CollectionStore *s = [] {
        auto *p = new CollectionStore;
        QCoreApplication *app = QCoreApplication::instance();
        if (app && p->thread() != app->thread()) p->moveToThread(app->thread());
        return p;
    }();
    return *s;
}

void CollectionStore::setPath(const QString &p)
{
    if (G::isLogger) G::log("CollectionStore::setPath", p);
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

QString CollectionStore::path() const
{
    if (!dbPath.isEmpty()) return dbPath;
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
           + "/" + kDbName;
}

bool CollectionStore::isAvailable()
{
    return db().isOpen();
}

QSqlDatabase CollectionStore::db()
{
/*
    Open on first use. A failure is REMEMBERED rather than retried on every call -- the
    panel asks often, and a file that would not open a moment ago will not open now --
    and the file is left exactly as it was found. setPath clears the memory.
*/
    if (opened) return QSqlDatabase::database(connName, false);
    if (failed) return QSqlDatabase();

    const QString p = path();
    QDir().mkpath(QFileInfo(p).absolutePath());
    connName = QString("winnow_collections_%1").arg(reinterpret_cast<quintptr>(this));
    QSqlDatabase d = QSqlDatabase::addDatabase("QSQLITE", connName);
    d.setDatabaseName(p);
    if (!d.open()) {
        lastError = tr("The collections file could not be opened: %1")
                        .arg(d.lastError().text());
        failed = true;
        G::issue("Warning", lastError, "CollectionStore::db", -1, p);
        return QSqlDatabase();
    }
    QSqlQuery q(d);
    /*  The foreign keys ARE the cascade: deleting a node takes its children and its
        memberships with it, in the one statement. SQLite has them off per connection
        unless asked. */
    q.exec("PRAGMA foreign_keys = ON");
    q.exec("PRAGMA journal_mode = WAL");
    q.exec("PRAGMA synchronous = NORMAL");
    if (!migrate(d)) {
        d.close();
        failed = true;
        G::issue("Warning", lastError, "CollectionStore::db", -1, p);
        return QSqlDatabase();
    }
    opened = true;
    return d;
}

bool CollectionStore::migrate(QSqlDatabase &d)
{
    QSqlQuery q(d);
    if (!q.exec("PRAGMA user_version") || !q.next()) {
        lastError = tr("The collections file could not be read: %1")
                        .arg(q.lastError().text());
        return false;
    }
    const int version = q.value(0).toInt();
    /*  A NEWER FILE IS LEFT ALONE. The index would be moved aside and rebuilt; this
        cannot be rebuilt, so an older Winnow simply does without Collections until the
        newer one is back. */
    if (version > kSchemaVersion) {
        lastError = tr("The collections file was written by a newer version of Winnow, "
                       "so this version leaves it untouched.");
        return false;
    }
    if (version == kSchemaVersion) return true;

    d.transaction();
    if (version < 1) {
        const char *ddl[] = {
            /*  One table for both kinds. `definition` is a Query's saved search; a
                collection leaves it empty. `position` orders siblings -- the order the
                user dragged them into, not alphabetical, as in Lightroom. */
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
        for (const char *s : ddl) {
            if (!q.exec(QString::fromLatin1(s))) {
                lastError = tr("The collections file could not be set up: %1")
                                .arg(q.lastError().text());
                d.rollback();
                return false;
            }
        }
    }
    if (!q.exec(QString("PRAGMA user_version = %1").arg(kSchemaVersion))) {
        lastError = q.lastError().text();
        d.rollback();
        return false;
    }
    return d.commit();
}

QVector<CollectionStore::Node> CollectionStore::nodes(Kind kind)
{
/*
    Parents before children, so a caller can build a tree in one pass: read every node,
    then emit them breadth-first from the top level down.
*/
    QVector<Node> all;
    QSqlDatabase d = db();
    if (!d.isOpen()) return all;
    QSqlQuery q(d);
    q.prepare("SELECT id, IFNULL(parent, 0), kind, name, position, definition"
              " FROM node WHERE kind = ? ORDER BY position, id");
    q.addBindValue(static_cast<int>(kind));
    if (!q.exec()) return all;
    QHash<qint64, QVector<Node>> byParent;
    while (q.next()) {
        Node n;
        n.id = q.value(0).toLongLong();
        n.parent = q.value(1).toLongLong();
        n.kind = static_cast<Kind>(q.value(2).toInt());
        n.name = q.value(3).toString();
        n.position = q.value(4).toInt();
        n.definition = q.value(5).toString();
        byParent[n.parent] << n;
    }
    QVector<qint64> queue{0};
    for (int i = 0; i < queue.size(); ++i) {
        for (const Node &n : byParent.value(queue.at(i))) {
            all << n;
            queue << n.id;
        }
    }
    return all;
}

CollectionStore::Node CollectionStore::node(qint64 id)
{
    Node n;
    QSqlDatabase d = db();
    if (!d.isOpen() || id <= 0) return n;
    QSqlQuery q(d);
    q.prepare("SELECT id, IFNULL(parent, 0), kind, name, position, definition"
              " FROM node WHERE id = ?");
    q.addBindValue(id);
    if (!q.exec() || !q.next()) return n;
    n.id = q.value(0).toLongLong();
    n.parent = q.value(1).toLongLong();
    n.kind = static_cast<Kind>(q.value(2).toInt());
    n.name = q.value(3).toString();
    n.position = q.value(4).toInt();
    n.definition = q.value(5).toString();
    return n;
}

int CollectionStore::nextPosition(QSqlDatabase &d, qint64 parent, Kind kind)
{
    QSqlQuery q(d);
    q.prepare("SELECT IFNULL(MAX(position), -1) + 1 FROM node"
              " WHERE IFNULL(parent, 0) = ? AND kind = ?");
    q.addBindValue(parent);
    q.addBindValue(static_cast<int>(kind));
    if (!q.exec() || !q.next()) return 0;
    return q.value(0).toInt();
}

qint64 CollectionStore::create(Kind kind, qint64 parent, const QString &name,
                              const QString &definition)
{
    if (G::isLogger) G::log("CollectionStore::create", name);
    const QString nm = name.trimmed();
    QSqlDatabase d = db();
    if (!d.isOpen() || nm.isEmpty()) return 0;
    if (parent > 0) {
        const Node p = node(parent);
        if (p.id == 0 || p.kind != kind) return 0;
    }
    QSqlQuery q(d);
    q.prepare("INSERT INTO node (parent, kind, name, position, definition, created,"
              " modified) VALUES (?, ?, ?, ?, ?, ?, ?)");
    q.addBindValue(parentBind(parent));
    q.addBindValue(static_cast<int>(kind));
    q.addBindValue(nm);
    q.addBindValue(nextPosition(d, parent, kind));
    q.addBindValue(definition.isNull() ? QString("") : definition);
    q.addBindValue(nowSecs());
    q.addBindValue(nowSecs());
    if (!q.exec()) return 0;
    const qint64 id = q.lastInsertId().toLongLong();
    emit nodesChanged(kind);
    return id;
}

bool CollectionStore::rename(qint64 id, const QString &name)
{
    const QString nm = name.trimmed();
    QSqlDatabase d = db();
    if (!d.isOpen() || nm.isEmpty()) return false;
    const Node n = node(id);
    if (n.id == 0) return false;
    if (n.name == nm) return true;
    QSqlQuery q(d);
    q.prepare("UPDATE node SET name = ?, modified = ? WHERE id = ?");
    q.addBindValue(nm);
    q.addBindValue(nowSecs());
    q.addBindValue(id);
    if (!q.exec()) return false;
    emit nodesChanged(n.kind);
    return true;
}

bool CollectionStore::setDefinition(qint64 id, const QString &definition)
{
    QSqlDatabase d = db();
    if (!d.isOpen()) return false;
    const Node n = node(id);
    if (n.id == 0) return false;
    if (n.definition == definition) return true;
    QSqlQuery q(d);
    q.prepare("UPDATE node SET definition = ?, modified = ? WHERE id = ?");
    q.addBindValue(definition.isNull() ? QString("") : definition);
    q.addBindValue(nowSecs());
    q.addBindValue(id);
    if (!q.exec()) return false;
    emit nodesChanged(n.kind);
    return true;
}

qint64 CollectionStore::duplicate(qint64 id)
{
    QSqlDatabase d = db();
    if (!d.isOpen()) return 0;
    const Node n = node(id);
    if (n.id == 0) return 0;
    const qint64 copy = create(n.kind, n.parent, n.name + tr(" copy"), n.definition);
    if (copy == 0 || n.kind != Kind::Collection) return copy;
    QSqlQuery q(d);
    q.prepare("INSERT OR IGNORE INTO member (node, pathkey, path, added)"
              " SELECT ?, pathkey, path, added FROM member WHERE node = ?");
    q.addBindValue(copy);
    q.addBindValue(id);
    if (q.exec() && q.numRowsAffected() > 0) emit membersChanged();
    return copy;
}

bool CollectionStore::remove(qint64 id)
{
    if (G::isLogger) G::log("CollectionStore::remove", QString::number(id));
    QSqlDatabase d = db();
    if (!d.isOpen()) return false;
    const Node n = node(id);
    if (n.id == 0) return false;
    // ON DELETE CASCADE takes the subtree and every membership in it
    QSqlQuery q(d);
    q.prepare("DELETE FROM node WHERE id = ?");
    q.addBindValue(id);
    if (!q.exec()) return false;
    emit nodesChanged(n.kind);
    emit membersChanged();
    return true;
}

QVector<qint64> CollectionStore::subtree(qint64 id)
{
    QVector<qint64> out;
    QSqlDatabase d = db();
    if (!d.isOpen() || id <= 0) return out;
    QSqlQuery q(d);
    q.prepare("WITH RECURSIVE sub(id) AS (SELECT ? UNION ALL"
              " SELECT n.id FROM node n JOIN sub ON n.parent = sub.id)"
              " SELECT id FROM sub");
    q.addBindValue(id);
    if (!q.exec()) return out;
    while (q.next()) out << q.value(0).toLongLong();
    return out;
}

bool CollectionStore::wouldCycle(qint64 id, qint64 target)
{
    if (target <= 0) return false;
    return subtree(id).contains(target);
}

bool CollectionStore::reparent(qint64 id, qint64 newParent)
{
    if (G::isLogger) G::log("CollectionStore::reparent",
                            QString::number(id) + " -> " + QString::number(newParent));
    QSqlDatabase d = db();
    if (!d.isOpen()) return false;
    const Node n = node(id);
    if (n.id == 0) return false;
    if (n.parent == newParent) return true;
    if (newParent > 0) {
        const Node p = node(newParent);
        if (p.id == 0 || p.kind != n.kind) return false;
        if (wouldCycle(id, newParent)) return false;
    }
    QSqlQuery q(d);
    q.prepare("UPDATE node SET parent = ?, position = ?, modified = ? WHERE id = ?");
    q.addBindValue(parentBind(newParent));
    q.addBindValue(nextPosition(d, newParent, n.kind));
    q.addBindValue(nowSecs());
    q.addBindValue(id);
    if (!q.exec()) return false;
    emit nodesChanged(n.kind);
    return true;
}

int CollectionStore::addMembers(qint64 id, const QStringList &paths)
{
    if (G::isLogger) G::log("CollectionStore::addMembers", QString::number(paths.size()));
    QSqlDatabase d = db();
    if (!d.isOpen() || paths.isEmpty()) return 0;
    const Node n = node(id);
    if (n.id == 0 || n.kind != Kind::Collection) return 0;

    int added = 0;
    d.transaction();
    QSqlQuery q(d);
    q.prepare("INSERT OR IGNORE INTO member (node, pathkey, path, added)"
              " VALUES (?, ?, ?, ?)");
    const qint64 now = nowSecs();
    for (const QString &p : paths) {
        const QString key = cachePathKey(p);
        if (key.isEmpty()) continue;
        q.addBindValue(id);
        q.addBindValue(key);
        q.addBindValue(QDir::cleanPath(QDir::fromNativeSeparators(p)));
        q.addBindValue(now);
        if (q.exec()) added += q.numRowsAffected();
    }
    d.commit();
    if (added) emit membersChanged();
    return added;
}

int CollectionStore::removeMembers(qint64 id, const QStringList &paths)
{
    if (G::isLogger) G::log("CollectionStore::removeMembers", QString::number(paths.size()));
    QSqlDatabase d = db();
    if (!d.isOpen() || paths.isEmpty()) return 0;
    int removed = 0;
    d.transaction();
    QSqlQuery q(d);
    q.prepare("DELETE FROM member WHERE node = ? AND pathkey = ?");
    for (const QString &p : paths) {
        q.addBindValue(id);
        q.addBindValue(cachePathKey(p));
        if (q.exec()) removed += q.numRowsAffected();
    }
    d.commit();
    if (removed) emit membersChanged();
    return removed;
}

QHash<qint64, int> CollectionStore::memberCounts(Kind kind)
{
    QHash<qint64, int> out;
    QSqlDatabase d = db();
    if (!d.isOpen()) return out;
    QSqlQuery q(d);
    q.prepare("SELECT m.node, COUNT(*) FROM member m JOIN node n ON n.id = m.node"
              " WHERE n.kind = ? GROUP BY m.node");
    q.addBindValue(static_cast<int>(kind));
    if (!q.exec()) return out;
    while (q.next()) out.insert(q.value(0).toLongLong(), q.value(1).toInt());
    return out;
}

QStringList CollectionStore::memberPaths(const QVector<qint64> &ids)
{
    QStringList out;
    QSqlDatabase d = db();
    if (!d.isOpen() || ids.isEmpty()) return out;
    QStringList marks;
    for (int i = 0; i < ids.size(); ++i) marks << "?";
    QSqlQuery q(d);
    q.prepare("SELECT path FROM member WHERE node IN (" + marks.join(",") + ")"
              " GROUP BY pathkey ORDER BY path");
    for (qint64 id : ids) q.addBindValue(id);
    if (!q.exec()) return out;
    while (q.next()) out << q.value(0).toString();
    return out;
}

QHash<QString, QStringList> CollectionStore::membershipByKey(Kind kind)
{
    QHash<QString, QStringList> out;
    QSqlDatabase d = db();
    if (!d.isOpen()) return out;
    QSqlQuery q(d);
    q.prepare("SELECT m.pathkey, m.node FROM member m JOIN node n ON n.id = m.node"
              " WHERE n.kind = ?");
    q.addBindValue(static_cast<int>(kind));
    if (!q.exec()) return out;
    while (q.next()) out[q.value(0).toString()] << q.value(1).toString();
    return out;
}

void CollectionStore::onMoved(const QString &srcPath, const QString &dstPath)
{
/*
    A MOVE WINNOW MADE FOLLOWS THE IMAGE into every collection that holds it. Only an
    already-open store is touched: a session that never opened Collections has nothing
    in memory to keep in step, and opening the file on a file-operation path would put a
    first-use cost where the user is waiting on a rename.
*/
    if (QThread::currentThread() != thread()) {
        QMetaObject::invokeMethod(this, [this, srcPath, dstPath] {
            onMoved(srcPath, dstPath);
        }, Qt::QueuedConnection);
        return;
    }
    if (!opened && !QFileInfo::exists(path())) return;
    QSqlDatabase d = db();
    if (!d.isOpen()) return;
    const QString from = cachePathKey(srcPath);
    const QString to = cachePathKey(dstPath);
    if (from.isEmpty() || to.isEmpty() || from == to) {
        if (from == to && !from.isEmpty()) {
            // a case-only rename: same key, new spelling
            QSqlQuery q(d);
            q.prepare("UPDATE member SET path = ? WHERE pathkey = ?");
            q.addBindValue(QDir::cleanPath(QDir::fromNativeSeparators(dstPath)));
            q.addBindValue(from);
            q.exec();
        }
        return;
    }
    d.transaction();
    QSqlQuery q(d);
    /*  OR IGNORE, then delete what is left: a collection that already held the
        destination keeps one membership, not a constraint failure. */
    q.prepare("UPDATE OR IGNORE member SET pathkey = ?, path = ? WHERE pathkey = ?");
    q.addBindValue(to);
    q.addBindValue(QDir::cleanPath(QDir::fromNativeSeparators(dstPath)));
    q.addBindValue(from);
    q.exec();
    const bool changed = q.numRowsAffected() > 0;
    q.prepare("DELETE FROM member WHERE pathkey = ?");
    q.addBindValue(from);
    q.exec();
    d.commit();
    if (changed) emit membersChanged();
}

void CollectionStore::onDeleted(const QString &path)
{
    if (QThread::currentThread() != thread()) {
        QMetaObject::invokeMethod(this, [this, path] { onDeleted(path); },
                                  Qt::QueuedConnection);
        return;
    }
    if (!opened && !QFileInfo::exists(this->path())) return;
    QSqlDatabase d = db();
    if (!d.isOpen()) return;
    QSqlQuery q(d);
    q.prepare("DELETE FROM member WHERE pathkey = ?");
    q.addBindValue(cachePathKey(path));
    if (q.exec() && q.numRowsAffected() > 0) emit membersChanged();
}
