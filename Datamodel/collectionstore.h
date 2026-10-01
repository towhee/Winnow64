#ifndef COLLECTIONSTORE_H
#define COLLECTIONSTORE_H

#include <QHash>
#include <QObject>
#include <QSqlDatabase>
#include <QString>
#include <QStringList>
#include <QVector>


/*
    THE LIBRARY'S USER-MADE SETS: Collections today, Queries next.

    A COLLECTION is a set of images the user put together by hand -- Lightroom's
    collection. A QUERY (planned) is a saved search that is re-run each time it is
    opened -- Lightroom's smart collection. Both are a NODE in a tree the user arranges,
    both are opened the same way (a click builds a new dataset from the Library), and
    both nest. The one difference is where the set comes from: a collection lists its
    members, a query describes them. So they share one table, told apart by `kind`, and
    one tree widget (Views/collectiontree.h), told apart by whether it accepts dropped
    thumbnails. See notes/Documentation.txt "Collections (and the Queries to Come)".

    A COLLECTION IS A FILTER, not a dataset: the whole Library stays loaded and a
    collection is an item in the Filters panel's Collections category, which narrows it
    and combines with every other category. This class only says who is in what
    (membershipByKey); MW turns that into G::CollectionsColumn.

    NOT IN index.db, DELIBERATELY. The local index (Cache/cachedb.h) holds only what can
    be rebuilt from the images, and its failure policy is to move the file aside and
    start again. A collection is the user's own work -- nothing can rebuild it -- so it
    lives in its own file, collections.db beside the index, and this class NEVER moves
    that file aside: a file it cannot open or cannot migrate leaves Collections
    unavailable (with the reason in the panel) and the file untouched for the user to
    recover.

    IDENTITY IS THE PATH, as it is everywhere else in the index (Cache/pathkey.h). A
    member is stored as its key AND its path: the key is what matches the catalog's
    image.pathkey, the path is what a person reading the file can recognise. A move
    Winnow performs follows the image (FileOps::onMoved -> onMoved); a delete Winnow
    performs takes it out of every collection (onDeleted). A file moved outside Winnow
    stays a member and simply stops loading until it is back, which is what a
    catalogued image does too.

    A VERSION (virtual copy) IS ITS FILE here: membership is by source path, so adding a
    version adds the image, and opening the collection shows the image with its versions
    as the Library does. Per-version membership is a later step.

    GUI THREAD. One connection, opened on first use. onMoved/onDeleted may be called from
    a file-operation thread and hop to this object's thread themselves.
*/
class CollectionStore : public QObject
{
    Q_OBJECT

public:
    /*  Stored as an integer in node.kind: a published format, so values are never
        renumbered. */
    enum class Kind : int { Collection = 0, Query = 1 };

    struct Node
    {
        qint64 id = 0;
        qint64 parent = 0;          // 0 = top level
        Kind kind = Kind::Collection;
        QString name;
        int position = 0;           // order among its siblings
        /*  A Query's saved definition, Query::Expr::toJsonText (Utilities/queryexpr.h);
            empty for a collection. A PUBLISHED FORMAT: see queryexpr.h. */
        QString definition;
    };

    static CollectionStore &instance();

    /*  Point at a file. Closes the open connection; the next call opens the new one.
        Tests point it at a temp file; the app leaves the default, AppDataLocation/
        collections.db. */
    void setPath(const QString &dbPath);
    QString path() const;

    /*  Open (or create) the file. False with a reason when it cannot be used -- the
        panel shows the reason and disables itself. Never moves the file aside. */
    bool isAvailable();
    QString unavailableReason() const { return lastError; }

    /*  Every node of one kind, parents before children, siblings in position order. */
    QVector<Node> nodes(Kind kind);
    Node node(qint64 id);

    /*  Returns the new node's id, or 0. parent 0 = top level; a parent of another kind
        is refused. The node goes last among its siblings. */
    qint64 create(Kind kind, qint64 parent, const QString &name,
                  const QString &definition = QString());
    /*  A Query's saved definition: Query::Expr::toJsonText (Utilities/queryexpr.h). */
    bool setDefinition(qint64 id, const QString &definition);
    /*  A copy beside the original, named "<name> copy": a Query's definition, or a
        collection's members (not its sub-collections). Returns the new id, or 0. */
    qint64 duplicate(qint64 id);
    bool rename(qint64 id, const QString &name);
    /*  Deletes the node, every node beneath it and all their memberships. */
    bool remove(qint64 id);
    /*  Make id a child of newParent (0 = top level), last among its new siblings.
        Refused onto itself, onto one of its own descendants, or across kinds. */
    bool reparent(qint64 id, qint64 newParent);
    /*  True when making id a child of target would put a node inside itself. */
    bool wouldCycle(qint64 id, qint64 target);
    /*  id and every node beneath it. */
    QVector<qint64> subtree(qint64 id);

    /*  Membership (collections only). Each returns how many images actually changed:
        adding what is already there, or removing what is not, counts nothing. */
    int addMembers(qint64 id, const QStringList &paths);
    int removeMembers(qint64 id, const QStringList &paths);
    /*  Images DIRECTLY in each collection of the kind -- the number the tree shows. */
    QHash<qint64, int> memberCounts(Kind kind);
    /*  The distinct member paths of these nodes. */
    QStringList memberPaths(const QVector<qint64> &ids);

    /*  EVERY MEMBERSHIP OF THIS KIND, as image key (Cache/pathkey.h) -> the ids, as
        text, of the nodes it is DIRECTLY in. What G::CollectionsColumn is built from
        (MW::refreshCollectionMembership). One statement. */
    QHash<QString, QStringList> membershipByKey(Kind kind);

    /*  FileOps notifications: a file Winnow moved or deleted. */
    void onMoved(const QString &srcPath, const QString &dstPath);
    void onDeleted(const QString &path);
    void onFolderDeleted(const QString &folder);   // every image under folder, at any depth, versions included

signals:
    /*  The tree of this kind changed shape or name (create, rename, remove, reparent). */
    void nodesChanged(CollectionStore::Kind kind);
    /*  Some membership changed; counts are stale. */
    void membersChanged();

private:
    CollectionStore() = default;
    Q_DISABLE_COPY(CollectionStore)

    QSqlDatabase db();
    bool migrate(QSqlDatabase &d);
    int nextPosition(QSqlDatabase &d, qint64 parent, Kind kind);

    QString dbPath;
    QString connName;
    QString lastError;
    bool opened = false;
    bool failed = false;
};

#endif // COLLECTIONSTORE_H
