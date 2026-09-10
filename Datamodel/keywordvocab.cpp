#include "Datamodel/keywordvocab.h"

#include "Cache/cachedb.h"
#include "Cache/catalog.h"
#include "Main/global.h"
#include "Metadata/keywordpaths.h"
#include "Metadata/lrkeywords.h"

#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QFile>
#include <QSaveFile>
#include <QTextStream>
#include <algorithm>

/*
    See keywordvocab.h for what this is and why it is a second table.
*/

namespace {

/*  A path built from a parent's path and a leaf. One definition so a rename, a
    re-parent and an insert cannot disagree about the separator. */
QString joinPath(const QString &parentPath, const QString &leaf)
{
    return parentPath.isEmpty() ? leaf : parentPath + '|' + leaf;
}

}   // namespace

KeywordVocab::KeywordVocab(QObject *parent)
    : QAbstractItemModel(parent)
{
    root = new VocabNode;
}

KeywordVocab::~KeywordVocab()
{
    clearTree();
    delete root;
}

void KeywordVocab::clearTree()
{
    /*  Depth-first delete. The tree owns its nodes; nothing outside holds one except a
        QModelIndex, and every caller of clearTree is inside a model reset. */
    std::function<void(VocabNode *)> kill = [&](VocabNode *n) {
        for (VocabNode *c : n->children) { kill(c); delete c; }
        n->children.clear();
    };
    kill(root);
    byPathFold.clear();
    byId.clear();
}

VocabNode *KeywordVocab::node(const QModelIndex &idx) const
{
    if (!idx.isValid()) return root;
    return static_cast<VocabNode *>(idx.internalPointer());
}

const VocabNode *KeywordVocab::nodeOf(const QModelIndex &idx)
{
    if (!idx.isValid()) return nullptr;
    return static_cast<const VocabNode *>(idx.internalPointer());
}

QModelIndex KeywordVocab::indexOf(VocabNode *n, int column) const
{
    if (!n || n == root || !n->parent) return QModelIndex();
    const int row = n->parent->children.indexOf(n);
    if (row < 0) return QModelIndex();
    return createIndex(row, column, n);
}

QModelIndex KeywordVocab::index(int row, int column, const QModelIndex &parent) const
{
    if (!hasIndex(row, column, parent)) return QModelIndex();
    VocabNode *p = node(parent);
    if (row < 0 || row >= p->children.size()) return QModelIndex();
    return createIndex(row, column, p->children.at(row));
}

QModelIndex KeywordVocab::parent(const QModelIndex &child) const
{
    if (!child.isValid()) return QModelIndex();
    VocabNode *n = node(child);
    if (!n || !n->parent || n->parent == root) return QModelIndex();
    return indexOf(n->parent);
}

int KeywordVocab::rowCount(const QModelIndex &parent) const
{
    if (parent.column() > 0) return 0;
    return node(parent)->children.size();
}

int KeywordVocab::columnCount(const QModelIndex &) const
{
    return ColumnCount;
}

QVariant KeywordVocab::data(const QModelIndex &idx, int role) const
{
    const VocabNode *n = nodeOf(idx);
    if (!n) return QVariant();

    switch (role) {
    case Qt::DisplayRole:
    case Qt::EditRole:
        /*  The LEAF in the name column, because a tree that repeated the whole path on
            every row would be unreadable at depth. The path is the identity and travels
            as PathRole. */
        if (idx.column() == NameColumn) return n->name;
        /*  A zero count is shown BLANK rather than as "0". An empty branch the user just
            created is not a fact about images, and a column of zeros reads as a library
            that has lost its keywords. */
        if (idx.column() == CountColumn) return n->count > 0 ? QString::number(n->count)
                                                            : QString();
        return QVariant();

    case Qt::TextAlignmentRole:
        /*  Counts right-justified, the same as the Filters panel's count columns and the
            Source panel's -- a column of numbers is read down its last digit. */
        if (idx.column() == CountColumn)
            return QVariant::fromValue(Qt::AlignRight | Qt::AlignVCenter);
        return QVariant();

    case Qt::ToolTipRole:
        return n->path == n->name ? n->name : n->path;

    case PathRole:       return n->path;
    case IdRole:         return n->id;
    case CountRole:      return n->count;
    case SynonymsRole:   return n->synonyms;
    case ExportableRole: return n->exportable;
    case AppliedRole:    return appliedFold.value(keywordFold(n->path), false);
    default: return QVariant();
    }
}

QVariant KeywordVocab::headerData(int section, Qt::Orientation o, int role) const
{
    if (o != Qt::Horizontal || role != Qt::DisplayRole) return QVariant();
    return section == NameColumn ? QString("Keyword") : QString("Images");
}

void KeywordVocab::sortChildren(VocabNode *n)
{
    /*  Case-insensitive by leaf, which is how a person reads a list of names. The
        database's own order (pathfold) would put "Zebra" before "apple". */
    std::stable_sort(n->children.begin(), n->children.end(),
                     [](const VocabNode *a, const VocabNode *b) {
                         return a->name.compare(b->name, Qt::CaseInsensitive) < 0;
                     });
}

void KeywordVocab::reload()
{
    if (G::isLogger) G::log("KeywordVocab::reload");

    beginResetModel();
    clearTree();

    QSqlDatabase db = CacheDb::instance().db();
    if (db.isOpen()) {
        QSqlQuery q(db);
        /*  SHALLOWEST FIRST, so a parent is always in byId before a child needs it.
            Ordering by the separator count rather than by parent id, because parent ids
            are insertion order and an imported vocabulary need not be in tree order. */
        if (q.exec("SELECT id, name, path, parent, synonyms, exportable FROM vocab"
                   " ORDER BY (LENGTH(path) - LENGTH(REPLACE(path, '|', ''))), pathfold"))
        {
            while (q.next()) {
                VocabNode *n = new VocabNode;
                n->id = q.value(0).toLongLong();
                n->name = q.value(1).toString();
                n->path = q.value(2).toString();
                const qint64 parentId = q.value(3).toLongLong();
                const QString syn = q.value(4).toString();
                if (!syn.isEmpty()) n->synonyms = syn.split('\n', Qt::SkipEmptyParts);
                n->exportable = q.value(5).toInt() != 0;

                n->parent = parentId ? byId.value(parentId, root) : root;
                /*  A node whose parent row is missing is adopted by the root rather than
                    dropped. The database should not produce one, but losing a branch
                    silently because of a bad row is worse than showing it in the wrong
                    place. */
                if (!n->parent) n->parent = root;
                n->parent->children.append(n);

                byId.insert(n->id, n);
                byPathFold.insert(keywordFold(n->path), n);
            }
        }
    }

    std::function<void(VocabNode *)> sortAll = [&](VocabNode *n) {
        sortChildren(n);
        for (VocabNode *c : n->children) sortAll(c);
    };
    sortAll(root);

    endResetModel();
    refreshCounts();
}

Qt::ItemFlags KeywordVocab::flags(const QModelIndex &idx) const
{
    /*  THE INVALID INDEX IS THE ROOT, and it is a drop target on purpose: dragging a
        keyword onto blank space below the tree moves it to the top level, which is the
        only gesture that gets a node OUT of a branch. KeywordTree::dropEvent already
        passes an invalid target through to reparent(). */
    if (!idx.isValid()) return Qt::ItemIsDropEnabled;

    /*  Not editable: renaming goes through the context menu's dialog (see
        KeywordTree), because a rename can rewrite files and has to ask first. */
    return Qt::ItemIsSelectable | Qt::ItemIsEnabled
         | Qt::ItemIsDragEnabled | Qt::ItemIsDropEnabled;
}

void KeywordVocab::refreshCounts()
{
    if (G::isLogger) G::log("KeywordVocab::refreshCounts");
    if (byPathFold.isEmpty()) return;

    /*  ONE QUERY for the whole vocabulary, not one per node. Catalog::keywords() already
        returns every indexed path with its count, and a vocabulary node's count is that
        row's count -- no summing, because every image is linked to every ancestor. */
    QHash<QString, int> counts;
    for (const CatalogKeyword &k : Catalog::instance().keywords())
        counts.insert(keywordFold(k.path), k.count);

    for (auto it = byPathFold.constBegin(); it != byPathFold.constEnd(); ++it)
        it.value()->count = counts.value(it.key(), 0);

    if (root->children.isEmpty()) return;
    /*  One dataChanged over the count column for the whole tree. Emitting per node would
        be thousands of signals for a folder change. */
    emit dataChanged(index(0, CountColumn),
                     index(root->children.size() - 1, CountColumn),
                     {Qt::DisplayRole, CountRole});
}

void KeywordVocab::setAppliedPaths(const QStringList &paths)
{
    appliedFold.clear();
    for (const QString &p : paths) appliedFold.insert(keywordFold(p), true);
    if (root->children.isEmpty()) return;
    emit dataChanged(index(0, NameColumn),
                     index(root->children.size() - 1, NameColumn), {AppliedRole});
}

QModelIndex KeywordVocab::indexForPath(const QString &path) const
{
    VocabNode *n = byPathFold.value(keywordFold(path), nullptr);
    return n ? indexOf(n) : QModelIndex();
}

QList<const VocabNode *> KeywordVocab::completions(const QString &prefix, int limit) const
{
    QList<const VocabNode *> out;
    const QString p = keywordFold(prefix);
    if (p.isEmpty()) return out;

    for (auto it = byPathFold.constBegin(); it != byPathFold.constEnd(); ++it) {
        const VocabNode *n = it.value();
        if (keywordFold(n->name).startsWith(p)) out << n;
        else {
            /*  Synonyms complete too, which is most of what they are for: typing
                "Lobster Claw" should offer the Heliconia it is filed under. */
            for (const QString &s : n->synonyms)
                if (keywordFold(s).startsWith(p)) { out << n; break; }
        }
    }
    /*  Shallowest first, then alphabetical: the plain "Vancouver" a user means most often
        is usually nearer the root than an obscure one. */
    std::stable_sort(out.begin(), out.end(),
                     [](const VocabNode *a, const VocabNode *b) {
                         const int da = a->path.count('|'), db = b->path.count('|');
                         if (da != db) return da < db;
                         return a->path.compare(b->path, Qt::CaseInsensitive) < 0;
                     });
    if (limit > 0 && out.size() > limit) out = out.mid(0, limit);
    return out;
}

bool KeywordVocab::writeNode(const VocabNode *n)
{
    QSqlDatabase db = CacheDb::instance().db();
    if (!db.isOpen()) return false;

    QSqlQuery q(db);
    q.prepare("UPDATE vocab SET name = ?, namefold = ?, path = ?, pathfold = ?,"
              " parent = ?, synonyms = ?, exportable = ? WHERE id = ?");
    q.addBindValue(n->name);
    q.addBindValue(keywordFold(n->name));
    q.addBindValue(n->path);
    q.addBindValue(keywordFold(n->path));
    q.addBindValue(n->parent && n->parent != root ? QVariant(n->parent->id) : QVariant());
    /*  AN EMPTY LIST JOINS TO A NULL QString, AND THE COLUMN IS "NOT NULL DEFAULT ''".
        QStringList::join returns a default-constructed QString for an empty list, Qt binds
        a null QString as SQL NULL, and SQLite then refuses the UPDATE -- so EVERY write of
        a node with no synonyms failed, which is very nearly every node there is.

        WHAT THAT COST, because it is not what it looks like. rename checked the result and
        returned false, so it changed the in-memory name and never wrote it, never emitted
        pathChanged, and never offered to retag the photographs; reparent and rewritePaths
        did not check at all, so a moved branch looked moved until the next reload put it
        back. Both failures are invisible in a running session -- the tree on screen is the
        in-memory one -- which is why this survived long enough for a merge to find it.

        NOT the empty-string literal by accident: QString::fromLatin1("") is non-null,
        where QString() is null and binds as NULL. */
    const QString synonyms = n->synonyms.isEmpty() ? QString::fromLatin1("")
                                                   : n->synonyms.join('\n');
    q.addBindValue(synonyms);
    q.addBindValue(n->exportable ? 1 : 0);
    q.addBindValue(n->id);
    if (!q.exec()) {
        G::issue("Warning", "Vocabulary write failed: " + q.lastError().text(),
                 "KeywordVocab::writeNode", -1, n->path);
        return false;
    }
    return true;
}

bool KeywordVocab::rewritePaths(VocabNode *n)
{
    /*  n's own path has already been set by the caller; this fixes its descendants and
        keeps byPathFold in step. Depth-first, so a child is rewritten against a parent
        path that is already correct.

        IT REPORTS NOW, and it did not. Every write here was issued and its result thrown
        away, so a descendant whose row would not write left the tree and the database
        disagreeing for the rest of the session with nothing said -- which is exactly how
        the null-synonyms bug in writeNode stayed hidden. */
    for (VocabNode *c : n->children) {
        byPathFold.remove(keywordFold(c->path));
        c->path = joinPath(n->path, c->name);
        byPathFold.insert(keywordFold(c->path), c);
        if (!writeNode(c)) return false;
        if (!rewritePaths(c)) return false;
    }
    return true;
}

bool KeywordVocab::rename(const QModelIndex &idx, const QString &newName)
{
    VocabNode *n = nodeOf(idx) ? const_cast<VocabNode *>(nodeOf(idx)) : nullptr;
    if (!n) return false;

    const QString name = keywordNodes(newName).join('|');
    /*  A name containing the separator would be a PATH, not a name -- accepting it would
        create a node whose leaf silently disagreed with its own path. */
    if (name.isEmpty() || name.contains('|')) return false;
    if (name == n->name) return true;

    /*  A sibling with the same folded name would give two nodes one identity, which the
        unique index on pathfold would reject anyway -- better to refuse it here, where
        the caller can say why. */
    VocabNode *p = n->parent ? n->parent : root;
    for (const VocabNode *sib : p->children)
        if (sib != n && keywordFold(sib->name) == keywordFold(name)) return false;

    const QString oldPath = n->path;

    byPathFold.remove(keywordFold(n->path));
    n->name = name;
    n->path = joinPath(p == root ? QString() : p->path, name);
    byPathFold.insert(keywordFold(n->path), n);

    if (!writeNode(n) || !rewritePaths(n)) {
        G::issue("Warning", "Keyword rename could not be saved", "KeywordVocab::rename",
                 -1, n->path);
        reload();
        return false;
    }

    /*  The children's ORDER may have changed with the name. Re-sorting is a layout
        change rather than a data change, so the view is told to re-read the branch. */
    beginResetModel();
    sortChildren(p);
    endResetModel();

    emit pathChanged(oldPath, n->path);
    return true;
}

bool KeywordVocab::wouldCycle(const QModelIndex &src, const QModelIndex &target) const
{
    const VocabNode *s = nodeOf(src);
    if (!s) return false;
    const VocabNode *t = nodeOf(target);        // null target == the root, never a cycle
    if (!t) return false;
    /*  The same test the retag count uses, on the same folded strings, so "beneath"
        cannot mean two things. keywordIsDescendant is separator-aware: "Fauna|Birdsong"
        is NOT beneath "Fauna|Bird". */
    return keywordIsDescendant(keywordFold(t->path), keywordFold(s->path));
}

bool KeywordVocab::reparent(const QModelIndex &idx, const QModelIndex &newParent)
{
    VocabNode *n = nodeOf(idx) ? const_cast<VocabNode *>(nodeOf(idx)) : nullptr;
    if (!n) return false;
    if (wouldCycle(idx, newParent)) return false;

    VocabNode *p = newParent.isValid()
                       ? const_cast<VocabNode *>(nodeOf(newParent)) : root;
    if (!p) p = root;
    if (n->parent == p) return true;

    for (const VocabNode *sib : p->children)
        if (keywordFold(sib->name) == keywordFold(n->name)) return false;

    const QString oldPath = n->path;

    beginResetModel();
    if (n->parent) n->parent->children.removeOne(n);
    n->parent = p;
    p->children.append(n);
    sortChildren(p);

    byPathFold.remove(keywordFold(n->path));
    n->path = joinPath(p == root ? QString() : p->path, n->name);
    byPathFold.insert(keywordFold(n->path), n);
    const bool written = writeNode(n) && rewritePaths(n);
    endResetModel();

    /*  A MOVE THAT DID NOT REACH THE DATABASE IS NOT A MOVE. It used to be reported as
        one -- the writes were unchecked -- so the branch sat in its new place until the
        next reload silently put it back. reload() re-reads what was actually stored, so
        what the user sees is what they have. */
    if (!written) {
        G::issue("Warning", "Keyword move could not be saved", "KeywordVocab::reparent",
                 -1, n->path);
        reload();
        return false;
    }

    emit pathChanged(oldPath, n->path);
    return true;
}

bool KeywordVocab::wouldMerge(const QModelIndex &idx, const QModelIndex &newParent) const
{
    const VocabNode *n = nodeOf(idx);
    if (!n) return false;
    if (wouldCycle(idx, newParent)) return false;

    const VocabNode *p = newParent.isValid() ? nodeOf(newParent) : root;
    if (!p) p = root;
    if (n->parent == p) return false;           // already there; nothing to merge

    for (const VocabNode *sib : p->children)
        if (keywordFold(sib->name) == keywordFold(n->name)) return true;
    return false;
}

bool KeywordVocab::deleteLeafNode(VocabNode *n)
{
/*
    ONE ROW, AND IT MUST BE CHILDLESS. remove() leans on ON DELETE CASCADE, which is right
    for deleting a branch and wrong here: by the time a merge reaches this the children
    have been moved out from under n, and a cascade would take whatever was still hanging
    off it. mergeNodes empties n first, so there is nothing to cascade to -- but the
    distinction is worth having in the name.
*/
    QSqlDatabase db = CacheDb::instance().db();
    if (!db.isOpen()) return false;

    QSqlQuery q(db);
    q.prepare("DELETE FROM vocab WHERE id = ?");
    q.addBindValue(n->id);
    if (!q.exec()) {
        G::issue("Warning", "Vocabulary merge delete failed: " + q.lastError().text(),
                 "KeywordVocab::deleteLeafNode", -1, n->path);
        return false;
    }

    byId.remove(n->id);
    byPathFold.remove(keywordFold(n->path));
    if (n->parent) n->parent->children.removeOne(n);
    delete n;
    return true;
}

bool KeywordVocab::mergeNodes(VocabNode *src, VocabNode *dst)
{
/*
    Fold src into dst. Recursive, depth-first, and src is gone when this returns true.

    THE CHILDREN ARE COPIED BEFORE THE LOOP because the loop mutates src->children --
    every branch of it either moves a child out or merges it away, and iterating the list
    being emptied is the obvious way to get this wrong.

    A MOVED CHILD'S PATH IS FREE BY CONSTRUCTION. It only moves when dst has no
    counterpart for it, which is exactly the condition under which its new path cannot
    already exist -- so the unique index on pathfold cannot fire. Where a counterpart does
    exist the two are merged instead, and nothing is written at that path at all.

    SYNONYMS ARE UNIONED rather than one side winning. They are alternative spellings, and
    two nodes that turn out to be the same keyword have between them every spelling the
    user has recorded for it; dropping either half would lose completions they had already
    set up.
*/
    QStringList syn = dst->synonyms;
    for (const QString &a : src->synonyms) {
        bool have = false;
        for (const QString &b : syn) {
            if (keywordFold(a) != keywordFold(b)) continue;
            have = true;
            break;
        }
        if (!have) syn << a;
    }
    if (syn != dst->synonyms) {
        dst->synonyms = syn;
        if (!writeNode(dst)) return false;
    }

    const QList<VocabNode *> kids = src->children;
    for (VocabNode *c : kids) {
        VocabNode *twin = nullptr;
        for (VocabNode *d : dst->children) {
            if (keywordFold(d->name) != keywordFold(c->name)) continue;
            twin = d;
            break;
        }

        if (twin) {
            if (!mergeNodes(c, twin)) return false;      // c is deleted inside
            continue;
        }

        src->children.removeOne(c);
        c->parent = dst;
        dst->children.append(c);
        byPathFold.remove(keywordFold(c->path));
        c->path = joinPath(dst->path, c->name);
        byPathFold.insert(keywordFold(c->path), c);
        if (!writeNode(c)) return false;
        /*  CHECKED, LIKE EVERY OTHER CALLER. This was the one place that issued the
            descendant writes and threw the answer away -- the exact shape rewritePaths'
            own comment describes, and the worst place for it: reparentMerging wraps this
            in a transaction precisely so a half-merged vocabulary cannot be read back,
            and a discarded failure returned success and let that transaction COMMIT the
            half-rewritten branch. */
        if (!rewritePaths(c)) return false;
    }

    sortChildren(dst);
    return deleteLeafNode(src);
}

QString KeywordVocab::reparentMerging(const QModelIndex &idx, const QModelIndex &newParent)
{
    VocabNode *n = nodeOf(idx) ? const_cast<VocabNode *>(nodeOf(idx)) : nullptr;
    if (!n) return QString();
    if (wouldCycle(idx, newParent)) return QString();

    VocabNode *p = newParent.isValid()
                       ? const_cast<VocabNode *>(nodeOf(newParent)) : root;
    if (!p) p = root;
    if (n->parent == p) return n->path;

    VocabNode *twin = nullptr;
    for (VocabNode *sib : p->children) {
        if (keywordFold(sib->name) != keywordFold(n->name)) continue;
        twin = sib;
        break;
    }
    /*  Nothing to merge with: this is an ordinary move, and reparent already does it --
        including its own pathChanged. Two ways to move a node would be two places for the
        path rewrite to drift. */
    if (!twin) return reparent(idx, newParent) ? n->path : QString();

    const QString oldPath = n->path;
    const QString newPath = twin->path;

    /*  ONE TRANSACTION FOR THE WHOLE MERGE. It is many UPDATEs and many DELETEs across a
        branch, and a half-merged vocabulary -- some children moved, their old parent still
        standing -- is not a state the tree can be read back from. On failure the rows roll
        back and reload() puts the in-memory tree back in step with them; without that the
        two would disagree for the rest of the session, which is the one thing every
        mutator here is written to prevent. */
    QSqlDatabase db = CacheDb::instance().db();
    if (!db.isOpen()) return QString();
    const bool inTransaction = db.transaction();

    beginResetModel();
    const bool ok = mergeNodes(n, twin);
    endResetModel();

    if (!ok) {
        if (inTransaction) db.rollback();
        reload();
        return QString();
    }
    if (inTransaction && !db.commit()) {
        reload();
        return QString();
    }

    emit pathChanged(oldPath, newPath);
    return newPath;
}

VocabNode *KeywordVocab::ensureChild(VocabNode *p, const QString &leaf, bool *created)
{
/*
    The child of p named leaf, made if it is not there. See the header: no model signals
    and no sort, so that insertPaths can do many of these inside a single reset.
*/
    if (created) *created = false;
    if (!p) p = root;

    for (VocabNode *sib : p->children)
        if (keywordFold(sib->name) == keywordFold(leaf)) return sib;

    const QString path = joinPath(p == root ? QString() : p->path, leaf);

    QSqlDatabase db = CacheDb::instance().db();
    if (!db.isOpen()) return nullptr;
    QSqlQuery q(db);
    q.prepare("INSERT INTO vocab (name, namefold, path, pathfold, parent)"
              " VALUES (?, ?, ?, ?, ?)"
              " ON CONFLICT(pathfold) DO NOTHING");
    q.addBindValue(leaf);
    q.addBindValue(keywordFold(leaf));
    q.addBindValue(path);
    q.addBindValue(keywordFold(path));
    q.addBindValue(p == root ? QVariant() : QVariant(p->id));
    if (!q.exec()) {
        G::issue("Warning", "Vocabulary insert failed: " + q.lastError().text(),
                 "KeywordVocab::ensureChild", -1, path);
        return nullptr;
    }
    /*  ON CONFLICT DO NOTHING IS A SUCCESSFUL exec() THAT INSERTED NOTHING, and
        lastInsertId() does NOT report that. sqlite3_last_insert_rowid() keeps the rowid
        of the last SUCCESSFUL insert on the connection, so after a refused row it returns
        somebody else's id -- verified: two inserts then a conflicting one leaves it at 2
        with changes() == 0. The old "!id" test therefore never fired, and the node built
        from that id would alias a live row: into byId over the real node, and every later
        writeNode updating the wrong row.

        The sibling check above catches the ordinary duplicate, so reaching here means the
        tree and the table have drifted -- which is the case this guard exists for and the
        only case it was ever going to see. */
    if (q.numRowsAffected() == 0) {
        G::issue("Warning", "Vocabulary insert refused: that path already exists",
                 "KeywordVocab::ensureChild", -1, path);
        return nullptr;
    }
    const qint64 id = q.lastInsertId().toLongLong();
    if (!id) return nullptr;             // no rowid at all: nothing usable to build on

    VocabNode *n = new VocabNode;
    n->id = id;
    n->name = leaf;
    n->path = path;
    n->parent = p;
    p->children.append(n);
    byId.insert(id, n);
    byPathFold.insert(keywordFold(path), n);

    if (created) *created = true;
    return n;
}

QModelIndex KeywordVocab::insertChild(const QModelIndex &parent, const QString &name)
{
    const QString leaf = keywordNodes(name).join('|');
    if (leaf.isEmpty() || leaf.contains('|')) return QModelIndex();

    VocabNode *p = parent.isValid() ? const_cast<VocabNode *>(nodeOf(parent)) : root;
    if (!p) p = root;

    /*  AN EXISTING NODE COSTS NO RESET. Callers lean on this -- MW::applyKeywordMoves and
        the tag zone's file-it both walk a path calling insertChild per node and expect
        the ones already there to be free. */
    for (VocabNode *sib : p->children)
        if (keywordFold(sib->name) == keywordFold(leaf)) return indexOf(sib);

    beginResetModel();
    VocabNode *n = ensureChild(p, leaf);
    if (n) sortChildren(p);
    endResetModel();

    return n ? indexOf(n) : QModelIndex();
}

int KeywordVocab::insertPaths(const QStringList &paths)
{
/*
    See the header. ONE RESET AND ONE TRANSACTION for the whole list.

    SHALLOWEST FIRST, so a parent exists before its child needs its id -- the same
    ordering reload() and buildFromCatalog use, for the same reason. Sorting here rather
    than asking the caller for it is what makes the function safe to hand a set.

    A FAILURE ROLLS THE ROWS BACK AND RELOADS, exactly as reparentMerging does: a
    half-inserted branch is not a state the tree can be read back from, and the in-memory
    tree must not be left describing rows that were never committed. reload() runs AFTER
    endResetModel, because a reset cannot be nested inside another one.
*/
    if (paths.isEmpty()) return 0;

    QSqlDatabase db = CacheDb::instance().db();
    if (!db.isOpen()) return 0;

    QStringList sorted = paths;
    std::stable_sort(sorted.begin(), sorted.end(),
                     [](const QString &a, const QString &b) {
                         return a.count('|') < b.count('|');
                     });

    const bool inTransaction = db.transaction();

    int added = 0;
    bool failed = false;
    QSet<VocabNode *> touched;          // parents whose child order may have changed

    beginResetModel();
    for (const QString &path : std::as_const(sorted)) {
        VocabNode *p = root;
        QString built;
        for (const QString &leaf : keywordNodes(path)) {
            built = built.isEmpty() ? leaf : built + '|' + leaf;
            VocabNode *have = byPathFold.value(keywordFold(built), nullptr);
            if (have) { p = have; continue; }

            bool created = false;
            VocabNode *n = ensureChild(p, leaf, &created);
            if (!n) { failed = true; break; }
            if (created) { ++added; touched.insert(p); }
            p = n;
        }
        if (failed) break;
    }
    for (VocabNode *p : std::as_const(touched)) sortChildren(p);
    endResetModel();

    if (failed) {
        if (inTransaction) db.rollback();
        G::issue("Warning", "Could not add every keyword to the list",
                 "KeywordVocab::insertPaths");
        reload();
        return 0;
    }
    if (inTransaction && !db.commit()) {
        reload();
        return 0;
    }
    return added;
}

QModelIndex KeywordVocab::insertParentAbove(const QModelIndex &idx, const QString &name)
{
    VocabNode *n = nodeOf(idx) ? const_cast<VocabNode *>(nodeOf(idx)) : nullptr;
    if (!n) return QModelIndex();

    VocabNode *oldParent = n->parent ? n->parent : root;
    const QModelIndex parentIdx = oldParent == root ? QModelIndex() : indexOf(oldParent);

    const QModelIndex created = insertChild(parentIdx, name);
    if (!created.isValid()) return QModelIndex();

    /*  insertChild may have re-sorted and therefore invalidated idx, so the node is
        re-found rather than the index re-used. */
    if (!reparent(indexOf(n), created)) return QModelIndex();
    return indexForPath(nodeOf(created)->path);
}

bool KeywordVocab::remove(const QModelIndex &idx)
{
    VocabNode *n = nodeOf(idx) ? const_cast<VocabNode *>(nodeOf(idx)) : nullptr;
    if (!n) return false;

    QSqlDatabase db = CacheDb::instance().db();
    if (!db.isOpen()) return false;
    QSqlQuery q(db);
    /*  The children go by ON DELETE CASCADE, which is the schema saying what this means:
        a branch is deleted with everything under it. The IMAGES are untouched -- their
        keywords live in their own files, and a vocabulary node is only a name for one. */
    q.prepare("DELETE FROM vocab WHERE id = ?");
    q.addBindValue(n->id);
    if (!q.exec()) {
        G::issue("Warning", "Vocabulary delete failed: " + q.lastError().text(),
                 "KeywordVocab::remove", -1, n->path);
        return false;
    }

    beginResetModel();
    std::function<void(VocabNode *)> forget = [&](VocabNode *x) {
        for (VocabNode *c : x->children) forget(c);
        byId.remove(x->id);
        byPathFold.remove(keywordFold(x->path));
    };
    forget(n);
    if (n->parent) n->parent->children.removeOne(n);
    std::function<void(VocabNode *)> kill = [&](VocabNode *x) {
        for (VocabNode *c : x->children) kill(c);
        qDeleteAll(x->children);
        x->children.clear();
    };
    kill(n);
    delete n;
    endResetModel();

    return true;
}

int KeywordVocab::removeUnusedRoots(const QStringList &names)
{
/*
    See the header for the three conditions. The removals go through remove() rather than
    one DELETE, so the model is reset and the tree redrawn exactly as a hand delete would
    -- a bulk path that bypassed it would be a second way for the tree and the table to
    drift.

    THE INDEX IS TAKEN INSIDE THE LOOP because each remove() resets the model, which
    invalidates every index taken before it.
*/
    if (G::isLogger) G::log("KeywordVocab::removeUnusedRoots");

    int gone = 0;
    for (const QString &name : names) {
        const QString trimmed = keywordNodes(name).join('|');
        if (trimmed.isEmpty() || trimmed.contains('|')) continue;   // roots only

        const VocabNode *n = byPathFold.value(keywordFold(trimmed), nullptr);
        if (!n || n->parent != root) continue;
        if (!n->children.isEmpty() || n->count > 0) continue;

        const QModelIndex idx = indexForPath(n->path);
        if (idx.isValid() && remove(idx)) ++gone;
    }
    return gone;
}

bool KeywordVocab::setSynonyms(const QModelIndex &idx, const QStringList &synonyms)
{
    VocabNode *n = nodeOf(idx) ? const_cast<VocabNode *>(nodeOf(idx)) : nullptr;
    if (!n) return false;
    n->synonyms = synonyms;
    if (!writeNode(n)) return false;
    emit dataChanged(idx, idx, {SynonymsRole});
    return true;
}

bool KeywordVocab::setExportable(const QModelIndex &idx, bool exportable)
{
    VocabNode *n = nodeOf(idx) ? const_cast<VocabNode *>(nodeOf(idx)) : nullptr;
    if (!n) return false;
    n->exportable = exportable;
    if (!writeNode(n)) return false;
    emit dataChanged(idx, idx, {ExportableRole});
    return true;
}

int KeywordVocab::buildFromCatalog()
{
/*
    Add a vocabulary node for every keyword path the catalog holds.

    ADDITIVE, NEVER DESTRUCTIVE. A node the user authored and no image uses is exactly
    what vocab exists to hold, so this cannot be a replace: it inserts what is missing and
    leaves everything else alone. That makes it safe to run twice, and safe to run after
    an import.

    SHALLOWEST FIRST, so a parent exists before its child needs its id -- the same
    ordering the schema 10 migration uses, for the same reason.
*/
    if (G::isLogger) G::log("KeywordVocab::buildFromCatalog");

    QStringList paths;
    for (const CatalogKeyword &k : Catalog::instance().keywords())
        if (!k.path.isEmpty()) paths << k.path;

    std::stable_sort(paths.begin(), paths.end(),
                     [](const QString &a, const QString &b) {
                         return a.count('|') < b.count('|');
                     });

    int added = 0;
    for (const QString &p : paths) {
        if (byPathFold.contains(keywordFold(p))) continue;
        const QString parentPath = keywordParentPath(p);
        const QModelIndex parentIdx =
            parentPath.isEmpty() ? QModelIndex() : indexForPath(parentPath);
        /*  A missing parent cannot happen -- prefix expansion means every ancestor is in
            the catalog too, and the sort puts it first -- but if it did, the node lands
            at the root rather than being dropped. */
        if (insertChild(parentIdx, keywordLeafOf(p)).isValid()) ++added;
    }

    refreshCounts();
    return added;
}

int KeywordVocab::importLightroom(const QString &filePath, QStringList *skipped)
{
/*
    Merge a Lightroom keyword export into the vocabulary.

    A MERGE, NOT A REPLACE, and that is the whole shape of it. A user's vocabulary is
    theirs: branches they built here and the file has never heard of must survive an
    import, or importing would be a destructive operation wearing a friendly name. So
    this only ever ADDS, and a path that already exists is left exactly as it is --
    including its synonyms, which the file might disagree with. Nothing is renamed,
    nothing is moved, nothing is deleted.

    MATCHED BY FOLDED PATH, so a re-import after a small edit in Lightroom adds the few
    new keywords and touches nothing else, and so that "Heron" and "heron" are the same
    keyword rather than two.

    PARENTS FIRST, which lrPaths already guarantees because the file is written
    depth-first -- a child row always follows its parent. insertChild is called for each
    missing ancestor in turn rather than trusting that, because a hand-edited file need
    not be well formed and a missing parent must not lose a whole branch.

    SYNONYMS ARE APPLIED ONLY TO NODES THIS IMPORT CREATED. Overwriting an existing
    node's synonyms would be a replace by the back door.
*/
    if (G::isLogger) G::log("KeywordVocab::importLightroom", filePath);

    QFile f(filePath);
    if (!f.open(QIODevice::ReadOnly)) {
        G::issue("Warning", "Could not open the keyword file.",
                 "KeywordVocab::importLightroom", -1, filePath);
        return 0;
    }
    const QList<LrKeyword> rows = lrParse(&f);
    f.close();
    if (rows.isEmpty()) return 0;

    const QStringList paths = lrPaths(rows);
    int added = 0;

    for (int i = 0; i < paths.size(); ++i) {
        const QString path = paths.at(i);
        if (path.isEmpty()) continue;
        if (byPathFold.contains(keywordFold(path))) continue;   // already ours

        /*  Walk the path, creating what is missing. An ancestor that already exists is
            reused, which is what makes this a merge rather than a parallel tree. */
        QModelIndex parent;
        QString built;
        bool ok = true;
        for (const QString &node : keywordNodes(path)) {
            built = built.isEmpty() ? node : built + '|' + node;
            const QModelIndex have = indexForPath(built);
            if (have.isValid()) { parent = have; continue; }
            parent = insertChild(parent, node);
            if (!parent.isValid()) { ok = false; break; }
            ++added;
        }
        if (!ok) {
            if (skipped) *skipped << path;
            continue;
        }
        if (!rows.at(i).synonyms.isEmpty() && parent.isValid())
            setSynonyms(parent, rows.at(i).synonyms);
    }

    refreshCounts();
    return added;
}

bool KeywordVocab::exportLightroom(const QString &filePath)
{
/*
    Write the vocabulary in the format Lightroom reads.

    SORTED BY FOLDED PATH, which is also depth-first order: a path is a prefix of its
    children, so a parent always sorts immediately before them. That is exactly the order
    the indented format needs, and getting it from the sort rather than from a tree walk
    means the writer cannot disagree with the reader about what "one level deeper" is.
*/
    if (G::isLogger) G::log("KeywordVocab::exportLightroom", filePath);

    QStringList paths;
    QHash<QString, QStringList> synonyms;
    for (auto it = byPathFold.constBegin(); it != byPathFold.constEnd(); ++it) {
        paths << it.value()->path;
        if (!it.value()->synonyms.isEmpty())
            synonyms.insert(it.key(), it.value()->synonyms);
    }
    std::sort(paths.begin(), paths.end(), [](const QString &a, const QString &b) {
        return keywordFold(a) < keywordFold(b);
    });

    /*  QSaveFile: an export that fails half way must not leave a truncated file where
        the user's vocabulary used to be. Same reason Xmp::writeSidecar uses one. */
    QSaveFile out(filePath);
    if (!out.open(QIODevice::WriteOnly | QIODevice::Text)) {
        G::issue("Warning", "Could not write the keyword file.",
                 "KeywordVocab::exportLightroom", -1, filePath);
        return false;
    }
    {
        QTextStream stream(&out);
        lrWrite(stream, paths, synonyms);
    }
    if (!out.commit()) {
        G::issue("Warning", "Could not write the keyword file.",
                 "KeywordVocab::exportLightroom", -1, filePath);
        return false;
    }
    return true;
}
