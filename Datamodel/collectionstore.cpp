#include "Datamodel/collectionstore.h"
#include "Datamodel/userdb.h"
#include "Cache/pathkey.h"
#include "Utilities/versionkey.h"
#include "Main/global.h"
#include "Undo/undocommands.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QSet>
#include <QSqlError>
#include <QSqlQuery>
#include <QThread>
#include <QVariant>

#include <memory>

namespace {

qint64 nowSecs() { return QDateTime::currentSecsSinceEpoch(); }

/*  A failed write to userdata.db: what it was and SQLite's reason. Once per distinct
    reason (issueDedup), never with a collection's name: the report can go to a public
    GitHub issue (see "Privacy" in notes/Documentation.md). */
void sqlFail(const QString &what, const QSqlQuery &q, const QString &src)
{
    G::issueDedup("Warning", "Collections: " + what + " failed: " + q.lastError().text(),
                  src);
}
void commitFail(const QString &what, QSqlDatabase &d, const QString &src)
{
    G::issueDedup("Warning", "Collections: " + what + " commit failed: " +
                  d.lastError().text(), src);
}

/*  A top-level node is NULL in the table (so the foreign key has nothing to check) and
    0 in memory. */
QVariant parentBind(qint64 parent) { return parent > 0 ? QVariant(parent) : QVariant(); }

/*  up(node, anc): every node of one kind (bound ?) paired with itself and each of its
    ancestors. A member of a child is thereby a member of every collection above it. */
const char *kAncestorsCte =
    "WITH RECURSIVE up(node, anc) AS ("
    " SELECT id, id FROM node WHERE kind = ?"
    " UNION ALL"
    " SELECT up.node, n.parent FROM up JOIN node n ON n.id = up.anc"
    " WHERE n.parent > 0) ";

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
    UserDb::instance().setPath(p);
}

QString CollectionStore::path() const
{
    return UserDb::instance().path();
}

bool CollectionStore::isAvailable()
{
    return UserDb::instance().isAvailable();
}

QString CollectionStore::unavailableReason() const
{
    return UserDb::instance().unavailableReason();
}

QSqlDatabase CollectionStore::db()
{
    return UserDb::instance().db();
}

bool CollectionStore::isOpen() const
{
    return UserDb::instance().isOpen();
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
    if (!q.exec()) {
        sqlFail("create", q, "CollectionStore::create");
        return 0;
    }
    const qint64 id = q.lastInsertId().toLongLong();
    emit nodesChanged(kind);
    /*  Undo takes the node away and keeps what it held by then (a collection may have
        gained members, a duplicate's copies), so redo brings it back whole, same id. */
    auto gone = std::make_shared<Subtree>();
    UndoManager::record(new ActionCommand(
        tr("New %1").arg(kindName(kind)),
        [this, id, gone] { *gone = snapshot(id); remove(id); },
        [this, gone] { restore(*gone); }));
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
    if (!q.exec()) {
        sqlFail("rename", q, "CollectionStore::rename");
        return false;
    }
    emit nodesChanged(n.kind);
    const QString old = n.name;
    UndoManager::record(new ActionCommand(
        tr("Rename %1").arg(kindName(n.kind)),
        [this, id, old] { rename(id, old); }, [this, id, nm] { rename(id, nm); }));
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
    if (!q.exec()) {
        sqlFail("edit definition", q, "CollectionStore::setDefinition");
        return false;
    }
    emit nodesChanged(n.kind);
    const QString old = n.definition;
    UndoManager::record(new ActionCommand(
        tr("Edit %1").arg(kindName(n.kind)),
        [this, id, old] { setDefinition(id, old); },
        [this, id, definition] { setDefinition(id, definition); }));
    return true;
}

qint64 CollectionStore::duplicate(qint64 id)
{
    QSqlDatabase d = db();
    if (!d.isOpen()) return 0;
    const Node n = node(id);
    if (n.id == 0) return 0;
    // one step: create's own undo snapshots the copy, members included
    UndoMacro step(UndoManager::app(), tr("Duplicate %1").arg(kindName(n.kind)));
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
    const Subtree before = snapshot(id);
    // ON DELETE CASCADE takes the subtree and every membership in it
    QSqlQuery q(d);
    q.prepare("DELETE FROM node WHERE id = ?");
    q.addBindValue(id);
    if (!q.exec()) {
        sqlFail("delete", q, "CollectionStore::remove");
        return false;
    }
    emit nodesChanged(n.kind);
    emit membersChanged();
    UndoManager::record(new ActionCommand(
        tr("Delete %1").arg(kindName(n.kind)),
        [this, before] { restore(before); }, [this, id] { remove(id); }));
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
    const QVector<Placement> before = placements(n.kind);
    QSqlQuery q(d);
    q.prepare("UPDATE node SET parent = ?, position = ?, modified = ? WHERE id = ?");
    q.addBindValue(parentBind(newParent));
    q.addBindValue(nextPosition(d, newParent, n.kind));
    q.addBindValue(nowSecs());
    q.addBindValue(id);
    if (!q.exec()) return false;
    emit nodesChanged(n.kind);
    const QVector<Placement> after = placements(n.kind);
    const Kind kind = n.kind;
    UndoManager::record(new ActionCommand(
        tr("Move %1").arg(kindName(kind)),
        [this, kind, before] { setPlacements(kind, before); },
        [this, kind, after] { setPlacements(kind, after); }));
    return true;
}

bool CollectionStore::move(qint64 id, qint64 newParent, qint64 beforeId)
{
    if (G::isLogger) G::log("CollectionStore::move",
                            QString::number(id) + " -> " + QString::number(newParent)
                            + " before " + QString::number(beforeId));
    QSqlDatabase d = db();
    if (!d.isOpen() || id == beforeId) return false;
    const Node n = node(id);
    if (n.id == 0) return false;
    if (newParent > 0) {
        const Node p = node(newParent);
        if (p.id == 0 || p.kind != n.kind) return false;
        if (wouldCycle(id, newParent)) return false;
    }

    const QVector<Placement> before = placements(n.kind);
    // the new siblings in their current order, without the node being moved
    QVector<qint64> order;
    QSqlQuery q(d);
    q.prepare("SELECT id FROM node WHERE IFNULL(parent, 0) = ? AND kind = ?"
              " ORDER BY position, id");
    q.addBindValue(newParent);
    q.addBindValue(static_cast<int>(n.kind));
    if (!q.exec()) return false;
    while (q.next()) {
        const qint64 sib = q.value(0).toLongLong();
        if (sib != id) order << sib;
    }
    int at = order.size();
    if (beforeId > 0) {
        at = order.indexOf(beforeId);
        if (at < 0) return false;               // not a child of newParent
    }
    order.insert(at, id);

    d.transaction();
    const qint64 now = nowSecs();
    QSqlQuery u(d);
    u.prepare("UPDATE node SET position = ? WHERE id = ?");
    for (int i = 0; i < order.size(); ++i) {
        u.addBindValue(i);
        u.addBindValue(order.at(i));
        if (!u.exec()) { d.rollback(); return false; }
    }
    u.prepare("UPDATE node SET parent = ?, modified = ? WHERE id = ?");
    u.addBindValue(parentBind(newParent));
    u.addBindValue(now);
    u.addBindValue(id);
    if (!u.exec()) { d.rollback(); return false; }
    if (!d.commit()) return false;
    emit nodesChanged(n.kind);
    const QVector<Placement> after = placements(n.kind);
    const Kind kind = n.kind;
    UndoManager::record(new ActionCommand(
        tr("Move %1").arg(kindName(kind)),
        [this, kind, before] { setPlacements(kind, before); },
        [this, kind, after] { setPlacements(kind, after); }));
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
    QStringList joined;                     // the images that were not members already
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
        if (!q.exec()) {
            sqlFail("add member", q, "CollectionStore::addMembers");
            continue;
        }
        if (q.numRowsAffected() > 0) {
            ++added;
            joined << p;
        }
    }
    if (!d.commit()) commitFail("add members", d, "CollectionStore::addMembers");
    if (added) {
        emit membersChanged();
        UndoManager::record(new ActionCommand(
            undoStepText(tr("Add to Collection"), added),
            [this, id, joined] { removeMembers(id, joined); },
            [this, id, joined] { addMembers(id, joined); }));
    }
    return added;
}

int CollectionStore::removeMembers(qint64 id, const QStringList &paths)
{
    if (G::isLogger) G::log("CollectionStore::removeMembers", QString::number(paths.size()));
    QSqlDatabase d = db();
    if (!d.isOpen() || paths.isEmpty()) return 0;
    int removed = 0;
    Subtree left;                           // the rows deleted, as they were, for undo
    d.transaction();
    QSqlQuery read(d);
    read.prepare("SELECT path, added FROM member WHERE node = ? AND pathkey = ?");
    QSqlQuery q(d);
    q.prepare("DELETE FROM member WHERE node = ? AND pathkey = ?");
    for (const QString &p : paths) {
        const QString key = cachePathKey(p);
        read.addBindValue(id);
        read.addBindValue(key);
        if (!read.exec() || !read.next()) continue;
        Member m;
        m.node = id;
        m.pathkey = key;
        m.path = read.value(0).toString();
        m.added = read.value(1).toLongLong();
        q.addBindValue(id);
        q.addBindValue(key);
        if (!q.exec()) {
            sqlFail("remove member", q, "CollectionStore::removeMembers");
            continue;
        }
        if (q.numRowsAffected() > 0) {
            ++removed;
            left.members << m;
        }
    }
    if (!d.commit()) commitFail("remove members", d, "CollectionStore::removeMembers");
    if (removed) {
        emit membersChanged();
        QStringList keys;
        for (const Member &m : std::as_const(left.members)) keys << m.path;
        UndoManager::record(new ActionCommand(
            undoStepText(tr("Remove from Collection"), removed),
            [this, left] { restore(left); },
            [this, id, keys] { removeMembers(id, keys); }));
    }
    return removed;
}

QHash<qint64, int> CollectionStore::memberCounts(Kind kind)
{
    QHash<qint64, int> out;
    QSqlDatabase d = db();
    if (!d.isOpen()) return out;
    QSqlQuery q(d);
    q.prepare(QString(kAncestorsCte) +
              "SELECT up.anc, COUNT(DISTINCT m.pathkey) FROM member m"
              " JOIN up ON up.node = m.node GROUP BY up.anc");
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

QVector<qint64> CollectionStore::collectionsOf(const QString &key, bool treeOrder)
{
    QVector<qint64> out;
    QSqlDatabase d = db();
    if (!d.isOpen() || key.isEmpty()) return out;
    QSqlQuery q(d);
    q.prepare("SELECT m.node FROM member m JOIN node n ON n.id = m.node"
              " WHERE n.kind = ? AND m.pathkey = ?");
    q.addBindValue(static_cast<int>(Kind::Collection));
    q.addBindValue(cachePathKey(key));
    if (!q.exec()) return out;
    QSet<qint64> in;
    while (q.next()) in.insert(q.value(0).toLongLong());
    if (!treeOrder) return QVector<qint64>(in.cbegin(), in.cend());
    // tree order, so a list built from it reads as the Collections panel does
    for (const Node &n : nodes(Kind::Collection))
        if (in.contains(n.id)) out << n.id;
    return out;
}

QHash<QString, QStringList> CollectionStore::membershipByKey(Kind kind)
{
    QHash<QString, QStringList> out;
    QSqlDatabase d = db();
    if (!d.isOpen()) return out;
    QSqlQuery q(d);
    q.prepare(QString(kAncestorsCte) +
              "SELECT DISTINCT m.pathkey, up.anc FROM member m"
              " JOIN up ON up.node = m.node");
    q.addBindValue(static_cast<int>(kind));
    if (!q.exec()) return out;
    while (q.next()) out[q.value(0).toString()] << q.value(1).toString();
    return out;
}

QString CollectionStore::kindName(Kind kind)
{
    switch (kind) {
    case Kind::Query: return tr("Query");
    case Kind::Place: return tr("Place");
    default:          return tr("Collection");
    }
}

CollectionStore::Subtree CollectionStore::snapshot(qint64 id)
{
    Subtree t;
    QSqlDatabase d = db();
    if (!d.isOpen() || id <= 0) return t;
    QSqlQuery q(d);
    q.prepare("SELECT id, IFNULL(parent, 0), kind, name, position, definition, created,"
              " modified FROM node WHERE id = ?");
    QSqlQuery m(d);
    m.prepare("SELECT pathkey, path, added FROM member WHERE node = ?");
    // subtree() walks breadth first: parents come before their children
    for (qint64 n : subtree(id)) {
        q.addBindValue(n);
        if (!q.exec() || !q.next()) continue;
        Row r;
        r.node.id = q.value(0).toLongLong();
        r.node.parent = q.value(1).toLongLong();
        r.node.kind = static_cast<Kind>(q.value(2).toInt());
        r.node.name = q.value(3).toString();
        r.node.position = q.value(4).toInt();
        r.node.definition = q.value(5).toString();
        r.created = q.value(6).toLongLong();
        r.modified = q.value(7).toLongLong();
        t.rows << r;
        m.addBindValue(n);
        if (!m.exec()) continue;
        while (m.next()) {
            Member mem;
            mem.node = n;
            mem.pathkey = m.value(0).toString();
            mem.path = m.value(1).toString();
            mem.added = m.value(2).toLongLong();
            t.members << mem;
        }
    }
    return t;
}

bool CollectionStore::restore(const Subtree &t)
{
/*
    Put back what snapshot() took, ids and all, in one transaction. A plain INSERT, not
    OR REPLACE: an id that is somehow taken again means the stack and the file disagree,
    and overwriting a node the user has would be worse than refusing (said in Issues).
    Memberships are OR IGNORE: one already there is the same fact.
*/
    if (G::isLogger) G::log("CollectionStore::restore", QString::number(t.rows.size()));
    QSqlDatabase d = db();
    if (!d.isOpen() || (t.rows.isEmpty() && t.members.isEmpty())) return false;
    d.transaction();
    QSqlQuery q(d);
    q.prepare("INSERT INTO node (id, parent, kind, name, position, definition, created,"
              " modified) VALUES (?, ?, ?, ?, ?, ?, ?, ?)");
    bool kinds[3] = {false, false, false};
    for (const Row &r : t.rows) {
        q.addBindValue(r.node.id);
        q.addBindValue(parentBind(r.node.parent));
        q.addBindValue(static_cast<int>(r.node.kind));
        q.addBindValue(r.node.name);
        q.addBindValue(r.node.position);
        q.addBindValue(r.node.definition);
        q.addBindValue(r.created);
        q.addBindValue(r.modified);
        if (!q.exec()) {
            const QString why = q.lastError().text();
            d.rollback();
            // the id, not the name: the issue log can reach a public report
            G::issue("Warning", QString("Could not undo: collection id %1 could not be put "
                     "back: %2").arg(r.node.id).arg(why), "CollectionStore::restore");
            return false;
        }
        const int k = static_cast<int>(r.node.kind);
        if (k >= 0 && k < 3) kinds[k] = true;
    }
    int added = 0;
    q.prepare("INSERT OR IGNORE INTO member (node, pathkey, path, added)"
              " VALUES (?, ?, ?, ?)");
    for (const Member &m : t.members) {
        q.addBindValue(m.node);
        q.addBindValue(m.pathkey);
        q.addBindValue(m.path);
        q.addBindValue(m.added);
        if (q.exec()) added += q.numRowsAffected();
    }
    if (!d.commit()) return false;
    for (int k = 0; k < 3; ++k) if (kinds[k]) emit nodesChanged(static_cast<Kind>(k));
    if (added) emit membersChanged();
    return true;
}

QVector<CollectionStore::Placement> CollectionStore::placements(Kind kind)
{
    QVector<Placement> out;
    QSqlDatabase d = db();
    if (!d.isOpen()) return out;
    QSqlQuery q(d);
    q.prepare("SELECT id, IFNULL(parent, 0), position FROM node WHERE kind = ?");
    q.addBindValue(static_cast<int>(kind));
    if (!q.exec()) return out;
    while (q.next())
        out << Placement{q.value(0).toLongLong(), q.value(1).toLongLong(),
                         q.value(2).toInt()};
    return out;
}

bool CollectionStore::setPlacements(Kind kind, const QVector<Placement> &where)
{
/*
    Undo or redo of a move: every node of the kind back where it was. A node deleted
    since (not through this stack) is simply not there to update.
*/
    if (G::isLogger) G::log("CollectionStore::setPlacements");
    QSqlDatabase d = db();
    if (!d.isOpen()) return false;
    d.transaction();
    QSqlQuery q(d);
    q.prepare("UPDATE node SET parent = ?, position = ? WHERE id = ? AND kind = ?");
    for (const Placement &p : where) {
        q.addBindValue(parentBind(p.parent));
        q.addBindValue(p.position);
        q.addBindValue(p.id);
        q.addBindValue(static_cast<int>(kind));
        if (!q.exec()) { d.rollback(); return false; }
    }
    if (!d.commit()) return false;
    emit nodesChanged(kind);
    return true;
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
    if (!isOpen() && !QFileInfo::exists(path())) return;
    QSqlDatabase d = db();
    if (!d.isOpen()) return;
    const QString from = cachePathKey(srcPath);
    const QString to = cachePathKey(dstPath);
    if (from.isEmpty() || to.isEmpty() || from == to) {
        if (from == to && !from.isEmpty()) {
            // a case-only rename: same key, new spelling
            const QString versions = from + VersionKey::kSep;
            QSqlQuery q(d);
            q.prepare("UPDATE member SET path = ? || substr(pathkey, length(?) + 1)"
                      " WHERE pathkey = ? OR substr(pathkey, 1, length(?)) = ?");
            q.addBindValue(QDir::cleanPath(QDir::fromNativeSeparators(dstPath)));
            q.addBindValue(from);
            q.addBindValue(from);
            q.addBindValue(versions);
            q.addBindValue(versions);
            if (!q.exec()) sqlFail("respell a moved image", q, "CollectionStore::onMoved");
        }
        return;
    }
    d.transaction();
    QSqlQuery q(d);
    /*  OR IGNORE, then delete what is left: a collection that already held the
        destination keeps one membership, not a constraint failure. The file's versions
        (from + "/#v" + id) go with it, each keeping its suffix. */
    const QString versions = from + VersionKey::kSep;
    q.prepare("UPDATE OR IGNORE member SET pathkey = ? || substr(pathkey, length(?) + 1),"
              " path = ? || substr(pathkey, length(?) + 1)"
              " WHERE pathkey = ? OR substr(pathkey, 1, length(?)) = ?");
    q.addBindValue(to);
    q.addBindValue(from);
    q.addBindValue(QDir::cleanPath(QDir::fromNativeSeparators(dstPath)));
    q.addBindValue(from);
    q.addBindValue(from);
    q.addBindValue(versions);
    q.addBindValue(versions);
    // a failed UPDATE is logged, but the DELETE after it still runs (not rolled back)
    if (!q.exec()) sqlFail("follow a moved image", q, "CollectionStore::onMoved");
    const bool changed = q.numRowsAffected() > 0;
    q.prepare("DELETE FROM member WHERE pathkey = ? OR substr(pathkey, 1, length(?)) = ?");
    q.addBindValue(from);
    q.addBindValue(versions);
    q.addBindValue(versions);
    if (!q.exec()) sqlFail("drop a moved image's old key", q, "CollectionStore::onMoved");
    if (!d.commit()) commitFail("move", d, "CollectionStore::onMoved");
    if (changed) emit membersChanged();
}

void CollectionStore::onFolderDeleted(const QString &folder)
{
    if (QThread::currentThread() != thread()) {
        QMetaObject::invokeMethod(this, [this, folder] { onFolderDeleted(folder); },
                                  Qt::QueuedConnection);
        return;
    }
    const QString prefix = cachePathKey(folder) + "/";
    if (prefix.size() < 2) return;
    if (!isOpen() && !QFileInfo::exists(this->path())) return;
    QSqlDatabase d = db();
    if (!d.isOpen()) return;
    QSqlQuery q(d);
    q.prepare("DELETE FROM member WHERE substr(pathkey, 1, ?) = ?");
    q.addBindValue(prefix.size());
    q.addBindValue(prefix);
    if (q.exec() && q.numRowsAffected() > 0) emit membersChanged();
}

void CollectionStore::onFolderMoved(const QString &from, const QString &to)
{
/*
    A folder Winnow renamed or moved: every membership under it follows, by key prefix
    -- see onMoved for the one-image form and ThumbCache::onFolderMoved for the prefix
    arithmetic. OR IGNORE then delete what is left, as onMoved does, so a collection
    that already held the destination keeps one membership. A case-only rename keeps
    every key and respells path alone.
*/
    if (QThread::currentThread() != thread()) {
        QMetaObject::invokeMethod(this, [this, from, to] { onFolderMoved(from, to); },
                                  Qt::QueuedConnection);
        return;
    }
    const QString fromKey = cachePathKey(from);
    const QString toKey = cachePathKey(to);
    if (fromKey.isEmpty() || toKey.isEmpty()) return;
    if (!isOpen() && !QFileInfo::exists(this->path())) return;
    QSqlDatabase d = db();
    if (!d.isOpen()) return;
    const QString toPath = QDir::cleanPath(QDir::fromNativeSeparators(to));
    const QString fromPrefix = fromKey + "/";
    d.transaction();
    QSqlQuery q(d);
    /*  path's tail comes from path, not from the key as onMoved's does: there the tail
        is only "/#v<id>", here it holds file names, and the key's are case-folded. */
    q.prepare("UPDATE OR IGNORE member SET pathkey = ? || substr(pathkey, length(?) + 1),"
              " path = ? || substr(path, length(?) + 1)"
              " WHERE substr(pathkey, 1, length(?)) = ?");
    q.addBindValue(toKey);
    q.addBindValue(fromKey);
    q.addBindValue(toPath);
    q.addBindValue(QDir::cleanPath(QDir::fromNativeSeparators(from)));
    q.addBindValue(fromPrefix);
    q.addBindValue(fromPrefix);
    if (!q.exec()) sqlFail("follow a moved folder", q, "CollectionStore::onFolderMoved");
    const bool changed = q.numRowsAffected() > 0;
    if (fromKey != toKey) {
        q.prepare("DELETE FROM member WHERE substr(pathkey, 1, length(?)) = ?");
        q.addBindValue(fromPrefix);
        q.addBindValue(fromPrefix);
        if (!q.exec())
            sqlFail("drop a moved folder's old keys", q, "CollectionStore::onFolderMoved");
    }
    if (!d.commit()) commitFail("folder move", d, "CollectionStore::onFolderMoved");
    if (changed) emit membersChanged();
}

void CollectionStore::onDeleted(const QString &path)
{
    if (QThread::currentThread() != thread()) {
        QMetaObject::invokeMethod(this, [this, path] { onDeleted(path); },
                                  Qt::QueuedConnection);
        return;
    }
    if (!isOpen() && !QFileInfo::exists(this->path())) return;
    QSqlDatabase d = db();
    if (!d.isOpen()) return;
    /*  The file's versions leave with it; a version key leaves alone (a version record
        deleted, MW::deleteVersionRecords). */
    const QString key = cachePathKey(path);
    const QString versions = key + VersionKey::kSep;
    QSqlQuery q(d);
    q.prepare("DELETE FROM member WHERE pathkey = ? OR substr(pathkey, 1, length(?)) = ?");
    q.addBindValue(key);
    q.addBindValue(versions);
    q.addBindValue(versions);
    if (q.exec() && q.numRowsAffected() > 0) emit membersChanged();
}
