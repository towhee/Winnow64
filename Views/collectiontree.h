#ifndef COLLECTIONTREE_H
#define COLLECTIONTREE_H

#include <QPoint>
#include <QSet>
#include <QTreeWidget>
#include <QVector>

#include "Datamodel/collectionstore.h"

class HoverDelegate;

/*
    THE COLLECTIONS PANEL'S TREE -- and, built with Kind::Query, the Queries panel's.

    Every row is a node in CollectionStore. The two panels are the same widget: the only
    behaviour that differs is that a Collections tree takes thumbnails dropped on it
    (acceptsImages) and a Queries tree does not, because a query's members come from its
    definition, not from the user's hand.

    A CLICK FILTERS. The whole Library stays loaded; a collection is an item in the
    Filters panel's Collections category, and this tree is a second view of that
    category, as LibTree is of Folders. The tree only ASKS (filterRequested) and MW
    pushes back what the category actually holds (syncFromFilter) after every filter
    change -- so a collection checked or cleared in the Filters panel shows here too,
    and the two cannot disagree. Collections combine with every other filter.

    CLICKS, in the Folders panel's vocabulary (FSTree):

      click         this collection
      Opt+click     this collection and every collection inside it
      Cmd+click     add or remove this collection, keeping the others
      Shift+click   every visible collection from the last one clicked to this one

    DRAGS. A collection dragged onto another becomes its child; dragged onto the blank
    space below the rows it goes back to the top level. Thumbnails dragged onto a
    collection are added to it -- ALWAYS as a CopyAction, on enter, move and drop: a
    MoveAction accepted here would make IconView::startDrag trash the files.

    The press does not act; the RELEASE does, so that pressing on a row to drag it does
    not first filter by it.
*/
class CollectionTree : public QTreeWidget
{
    Q_OBJECT

public:
    CollectionTree(CollectionStore::Kind kind, const QString &countMetric, int countMargin,
                   QWidget *parent = nullptr);

    CollectionStore::Kind kind() const { return nodeKind; }
    bool acceptsImages() const { return nodeKind == CollectionStore::Kind::Collection; }

    /*  Rebuild from the store, keeping expansion, the scroll position and what is
        checked. Cheap: a few hundred rows at most. */
    void reload();
    /*  Re-read the image counts only. */
    void refreshCounts();
    /*  A Queries tree's counts -- the loaded images each query matches -- which only MW
        can compute (it evaluates the queries over the rows). Kept across reloads. */
    void setCounts(const QHash<qint64, int> &counts);

    /*  What the Collections category includes, pushed by MW. Selected = included.
        Emits nothing. */
    void syncFromFilter(const QVector<qint64> &ids);
    QVector<qint64> checkedIds() const { return checkedSet; }
    QString nameOf(qint64 id) const;

    /*  Add a node under parent (0 = top level) and put its name in edit. A QUERY is not
        made here -- it needs a query -- so a Queries tree asks MW to open the Query
        Builder instead (newQueryRequested). */
    void createNode(qint64 parent);

    /*  Expanded node ids, for MW to keep across sessions. */
    QList<qint64> expandedIds() const;
    void setExpandedIds(const QList<qint64> &ids);

    // Re-apply palette-derived colours; MW::setBackgroundShade calls it.
    void updateStyle();

signals:
    /*  Filter to these nodes. withSub adds every node inside each. */
    void filterRequested(const QVector<qint64> &ids, bool withSub);
    /*  Thumbnails were dropped on a collection. */
    void addPathsRequested(qint64 id, const QStringList &paths);
    /*  Context menu: add / remove the images selected in the grid. */
    void addSelectionRequested(qint64 id);
    void removeSelectionRequested(qint64 id);
    /*  Queries only: open the Query Builder for a new query under parent, or on an
        existing one (the context menu's Edit Query..., or a double-click). */
    void newQueryRequested(qint64 parent);
    void editQueryRequested(qint64 id);
    /*  Clear the collection filter: the whole Library, with every other filter kept. */
    void cancelRequested();
    void expansionChanged();

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void leaveEvent(QEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void contextMenuEvent(QContextMenuEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dragMoveEvent(QDragMoveEvent *event) override;
    void dragLeaveEvent(QDragLeaveEvent *event) override;
    void dropEvent(QDropEvent *event) override;

private:
    enum Role { IdRole = Qt::UserRole + 1 };

    static qint64 idOf(const QTreeWidgetItem *item);
    QTreeWidgetItem *itemFor(qint64 id) const;
    QList<QTreeWidgetItem *> visibleItems() const;
    void requestFor(QTreeWidgetItem *item, Qt::KeyboardModifiers mods);
    void startNodeDrag(QTreeWidgetItem *item);
    /*  Whether a node drag of id may land on target (nullptr = the top level). */
    bool canDropNode(qint64 id, QTreeWidgetItem *target) const;
    void deleteNode(qint64 id);
    void resizeColumns();
    void styleItem(QTreeWidgetItem *item);
    QString kindWord(bool plural = false) const;

    CollectionStore::Kind nodeKind;
    HoverDelegate *delegate = nullptr;
    QString countMetric;
    int countMargin;
    QIcon nodeIcon;

    QVector<qint64> checkedSet;            // the last state MW pushed
    QHash<qint64, int> pushedCounts;        // a Queries tree's counts (setCounts)
    QSet<qint64> expanded;              // kept across rebuilds
    qint64 rangeAnchorId = 0;           // where a Shift+click range starts

    // press -> drag or click
    QTreeWidgetItem *pressItem = nullptr;
    QPoint pressPos;
    Qt::KeyboardModifiers pressMods;
    bool pressIsClick = false;
    bool reloading = false;             // itemChanged from a rebuild is not a rename
};

#endif // COLLECTIONTREE_H
