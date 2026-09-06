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
    QModelIndex insertChild(const QModelIndex &parent, const QString &name);
    /*  Put a new node BETWEEN idx and its parent, adopting idx. "I should have had a
        Location branch above all these places." */
    QModelIndex insertParentAbove(const QModelIndex &idx, const QString &name);
    bool remove(const QModelIndex &idx);
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
    void rewritePaths(VocabNode *n);
    bool writeNode(const VocabNode *n);
    void sortChildren(VocabNode *n);

    VocabNode *root = nullptr;                  // sentinel; its children are the roots
    QHash<QString, VocabNode *> byPathFold;
    QHash<qint64, VocabNode *> byId;
    QHash<QString, bool> appliedFold;
};

#endif // KEYWORDVOCAB_H
