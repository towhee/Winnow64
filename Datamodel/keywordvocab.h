#ifndef KEYWORDVOCAB_H
#define KEYWORDVOCAB_H

#include <QAbstractItemModel>
#include <QHash>
#include <QList>
#include <QString>
#include <QStringList>

/*
    THE AUTHORED KEYWORD VOCABULARY -- the tree the user curates, as a Qt model.

    TWO TABLES, AND THIS IS THE OTHER ONE. Cache/catalog.cpp's `keyword` table is the
    OBSERVED vocabulary: its rows are created as a side effect of indexing, they cascade
    away with their images, and their identity is pathfold. `vocab` is AUTHORED: its rows
    are user intent, only an explicit delete removes one, it holds branches no image uses
    yet, and its identity is an id that SURVIVES a rename -- which it must, because a
    node's synonyms and its export flag attach to the node rather than to its current
    spelling. One table cannot be keyed both ways. See the schema 10 block in cachedb.cpp.

    They join on pathfold, which is how a node gets its image count and how an
    unfiled keyword found in a file is recognised as unfiled.

    LOADED WHOLE, IN MEMORY, GUI THREAD ONLY. A vocabulary is thousands of nodes, not the
    quarter of a million rows the datamodel is sized for, so there is no reason for the
    lazy fetching and off-thread care that DataModel needs. Every mutator writes SQL and
    updates the in-memory tree in the same call, so the two cannot drift apart within a
    session; nothing else writes the table.

    RENAME AND RE-PARENT REWRITE DESCENDANT PATHS. A node's path is its ancestors' names
    joined, so changing a name high in the tree changes the path of everything beneath it.
    That is done here, in one transaction, rather than left to the caller -- and it is why
    the id, not the path, is the identity: the row survives, its spelling does not.

    WHAT THIS DOES NOT DO is touch a single image. Curating the vocabulary and retagging
    photographs are separate operations with separate consequences, and the second is the
    caller's to trigger (see MW::applyKeywordsToSelection and the retag dialog). A rename
    that silently rewrote a thousand sidecars would be the wrong default in a program
    where the files are the database.
*/

struct VocabNode
{
    qint64 id = 0;
    QString name;               // the leaf
    QString path;               // the identity in the OBSERVED vocabulary: "A|B|C"
    QStringList synonyms;
    bool exportable = true;

    /*  Images carrying this exact path, from the catalog. NOT a subtree total: every
        image is linked to every ancestor prefix, so a branch's own count already IS its
        subtree total. Zero means an empty branch, which is legitimate here and impossible
        in the observed table. */
    int count = 0;

    VocabNode *parent = nullptr;
    QList<VocabNode *> children;
};

class KeywordVocab : public QAbstractItemModel
{
    Q_OBJECT

public:
    explicit KeywordVocab(QObject *parent = nullptr);
    ~KeywordVocab() override;

    enum Column { NameColumn = 0, CountColumn = 1, ColumnCount = 2 };
    enum Role {
        PathRole = Qt::UserRole + 1,    // the full path: the identity
        IdRole,                         // vocab.id: survives a rename
        CountRole,
        SynonymsRole,
        ExportableRole,
        /*  True when the node's path is carried by the image the loupe is showing. The
            dot in the tree, and the only thing here that depends on what is selected. */
        AppliedRole
    };

    // QAbstractItemModel
    QModelIndex index(int row, int column,
                      const QModelIndex &parent = QModelIndex()) const override;
    QModelIndex parent(const QModelIndex &child) const override;
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &idx, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation o,
                        int role = Qt::DisplayRole) const override;
    /*  WITHOUT THIS THERE IS NO DRAG AT ALL. KeywordTree does every part of drag and
        drop itself, but the view will not even CALL startDrag unless the pressed index
        advertises Qt::ItemIsDragEnabled, and the default QAbstractItemModel::flags()
        offers only Selectable|Enabled. Measured, not assumed: a QTreeView with
        setDragEnabled(true) over a model whose flags lack the bit starts 0 drags, and 1
        with it. */
    Qt::ItemFlags flags(const QModelIndex &idx) const override;

    /*  Reload the whole tree from the database, then the counts from the catalog. Cheap
        enough to call on a folder change; a vocabulary is not a library. */
    void reload();
    /*  Counts only, leaving the tree and the view's expansion state alone. What a folder
        load or a keyword edit needs. */
    void refreshCounts();

    /*  Seed the vocabulary from what the catalog has actually indexed -- the button
        behind "Build vocabulary from catalog", and the same work schema 10's migration
        did. ADDITIVE: it never removes a node the user authored, so running it twice is
        harmless and running it after an import merges rather than replaces. */
    int buildFromCatalog();

    /*  Merge a Lightroom keyword export into the vocabulary. MERGES, never replaces: a
        node the user authored and the file does not mention is theirs and stays. Returns
        how many nodes were added; `skipped` collects paths the file named that could not
        be created, so the caller can say so rather than reporting a silent partial
        success. */
    int importLightroom(const QString &filePath, QStringList *skipped = nullptr);
    /*  Write the whole vocabulary in the same format, parents before children. */
    bool exportLightroom(const QString &filePath);

    // --- queries -------------------------------------------------------------------
    QModelIndex indexForPath(const QString &path) const;
    /*  Nodes whose LEAF starts with prefix, each carrying its full path, for the
        type-ahead. DISTINCT NODES, not distinct names: typing "vancouv" must offer both
        Vancouvers as separate entries or the completer would hide the thing path identity
        exists to expose. */
    QList<const VocabNode *> completions(const QString &prefix, int limit = 40) const;
    /*  The paths carried by the current image, so the tree can mark them. */
    void setAppliedPaths(const QStringList &paths);

    // --- mutators. Each writes the database and the tree together. ------------------
    bool rename(const QModelIndex &idx, const QString &newName);
    bool reparent(const QModelIndex &idx, const QModelIndex &newParent);
    /*  Would re-parenting src under newParent land on a node of the same name? That is
        the case plain reparent REFUSES, and the one reparentMerging exists for. Asked by
        the view so it can offer the merge instead of reporting a dead end. */
    bool wouldMerge(const QModelIndex &idx, const QModelIndex &newParent) const;
    /*  MERGE A BRANCH INTO THE ONE ALREADY THERE, recursively, and return the path it now
        occupies (empty on failure).

        WHAT reparent REFUSES. A vocabulary grown from two sources holds the same place
        twice -- "Canada|BC|Vancouver Island" beside "Location|Canada|BC|Vancouver
        Island" -- and the fix is to put one inside the other. reparent cannot: a name
        already taken in the destination is a collision it declines, because merging "has
        to decide what happens to both nodes' images". This is that decision, made once,
        here.

        NAME BY NAME, ALL THE WAY DOWN. Each child of src either has a counterpart under
        dst, in which case the two are merged in turn, or it does not, in which case it
        simply moves. Tails at any depth survive, which is the whole point: filing
        "Canada" under "Location" must keep "Canada|BC|Nanaimo" as
        "Location|Canada|BC|Nanaimo" rather than flattening it.

        ONE pathChanged FOR THE WHOLE MERGE, not one per node. retagKeywordPath rewrites
        every descendant at its PREFIX, so the top-level move describes the whole subtree;
        emitting per node would raise the retag dialog once per keyword moved.

        THE IMAGES ARE STILL NOT TOUCHED HERE. As with every other mutator on this class,
        the vocabulary changes and the caller decides what that means for the files. */
    QString reparentMerging(const QModelIndex &idx, const QModelIndex &newParent);
    QModelIndex insertChild(const QModelIndex &parent, const QString &name);

    /*  EVERY PATH IN THE LIST, WITH THE ANCESTORS IT NAMES, IN ONE RESET AND ONE
        TRANSACTION. insertChild resets the model per node, which is right for a hand
        gesture and wrong for filing a branch: merging an observed branch into the
        vocabulary can add dozens of nodes, and a reset apiece is a reset apiece for the
        Filters marking and for the tree's expansion state to be rebuilt from. Returns
        how many nodes were actually created; a path already present costs a lookup.

        SHALLOWEST FIRST is done here, so callers need not sort. */
    int insertPaths(const QStringList &paths);
    /*  Put a new node BETWEEN idx and its parent, adopting idx. "I should have had a
        Location branch above all these places." */
    QModelIndex insertParentAbove(const QModelIndex &idx, const QString &name);
    bool remove(const QModelIndex &idx);
    /*  Delete the ROOT node for each of these names if it is empty and childless --
        the tail of a flat-keyword tidy (MW::tidyFlatKeywords), where a bare "Nanaimo"
        that has just been filed under Location|...|Nanaimo leaves behind a top-level
        node no image uses.

        THREE CONDITIONS, ALL NECESSARY. Depth 1, because only a root can be the leftover
        of a flat keyword; no children, because a branch is somebody's structure whatever
        its own count says; and a count of zero, which is the actual test -- refreshCounts
        must have run since the images were rewritten or this deletes nodes still in use.
        Returns how many went. */
    int removeUnusedRoots(const QStringList &names);
    bool setSynonyms(const QModelIndex &idx, const QStringList &synonyms);
    bool setExportable(const QModelIndex &idx, bool exportable);

    /*  Would moving src under target make a node its own ancestor? The drop handler and
        reparent both ask, and they must agree. */
    bool wouldCycle(const QModelIndex &src, const QModelIndex &target) const;

    static const VocabNode *nodeOf(const QModelIndex &idx);

signals:
    /*  A node's path changed (rename or re-parent), with the path it had and the path it
        now has. The dock turns this into the retag confirmation; the model itself never
        touches an image. */
    void pathChanged(const QString &oldPath, const QString &newPath);

private:
    VocabNode *node(const QModelIndex &idx) const;
    QModelIndex indexOf(VocabNode *n, int column = 0) const;
    void clearTree();
    /*  Rewrite this node's path and every descendant's, in the tree and in one SQL
        statement, after its name or its parent changed. */
    bool rewritePaths(VocabNode *n);
    /*  Fold src into dst: move or merge every child, union the synonyms, then delete src.
        NO MODEL SIGNALS -- the caller holds one beginResetModel around the whole merge,
        because a reset per node would be a reset per keyword in the branch. */
    bool mergeNodes(VocabNode *src, VocabNode *dst);
    /*  Delete one CHILDLESS node's row and free it, without the model reset and without
        relying on ON DELETE CASCADE -- which would be wrong here, since the children have
        already been moved out from under it. */
    bool deleteLeafNode(VocabNode *n);
    bool writeNode(const VocabNode *n);

    /*  The child of p named leaf, created if it is not there. NO MODEL SIGNALS and no
        sort -- the caller owns both, which is what lets insertPaths do many of these
        inside one reset. created is set false when an existing node is returned. */
    VocabNode *ensureChild(VocabNode *p, const QString &leaf, bool *created = nullptr);
    void sortChildren(VocabNode *n);

    VocabNode *root = nullptr;                  // sentinel; its children are the roots
    QHash<QString, VocabNode *> byPathFold;
    QHash<qint64, VocabNode *> byId;
    QHash<QString, bool> appliedFold;
};

#endif // KEYWORDVOCAB_H
