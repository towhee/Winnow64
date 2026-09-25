#ifndef LIBTREE_H
#define LIBTREE_H

#include <QIcon>
#include <QMap>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QTreeWidget>
#include <QVector>

class HoverDelegate;

/*
    ONE CATALOG'S CONTRIBUTION TO THE LIBRARY.

    THE LIBRARY IS EVERY CATALOG BROWSED TOGETHER, and today there is exactly one. The
    tree is nonetheless built from a list of these, so a second catalog is a second entry
    rather than a restructuring: with one source the catalog level is not shown at all,
    with two or more each gets a row of its own above its folders. See
    notes/Documentation.txt "The Library Tree (LibTree)" for the Library/Catalog split.
*/
struct LibrarySource
{
    QString id;                         // stable key; "" is the one catalog of today
    QString name;                       // shown only when there is more than one
    QStringList anchors;                // the catalog's include folders: its top rows
    QMap<QString, int> folderCounts;    // LIVE images per folder (liveFolderCounts)
    QSet<QString> offlineAnchors;       // anchors whose volume is not there right now
};

/*
    THE LIBRARY'S FOLDERS, shown the way the Folders tree shows the disk.

    The Source panel's title bar switches between Folders and Library. Folders shows
    FSTree, which LOADS what is clicked. Library shows this, and a click here does NOT
    load anything: the whole Library is already loaded, so a click FILTERS it -- it
    checks the folder in the Filters panel's Folders category. The two widgets are views
    of ONE folder filter, owned by Filters; this one never holds filter state of its
    own. It asks (folderFilterRequested) and MW pushes the result back (syncFromFilters),
    so it and the Filters panel cannot disagree.

    CLICKS, in the vocabulary FSTree already taught:

      click         the folder AND every folder in it -- "2020-2029" is the decade
      Opt+click     this folder only: include it, exclude each folder directly in it
      Cmd+click     add or remove this folder, keeping the others
      Shift+click   every visible folder from the last one clicked to this one
      Library row   no folder filter -- the whole Library

    THE SHAPE COMES FROM Utilities/foldertree.h, which the Filters Folders category is
    built from as well: top-level rows are the catalog's include folders, and folders in
    between that hold no images of their own are still rows, because they are what
    selects a branch.

    READ-ONLY. There is no rename, move or drop here: this lists what the catalog holds,
    and a drag of thumbnails onto a tree has deleted files before (a MoveAction the
    target accepted). Changing which folders are catalogued is Manage Catalog...
*/
class LibTree : public QTreeWidget
{
    Q_OBJECT

public:
    /*  countMetric/countMargin are FSTree's, so the count column is the width it is in
        the Folders view and the two read as the same kind of list. */
    explicit LibTree(const QString &countMetric, int countMargin,
                     QWidget *parent = nullptr);

    /*  Rebuild from the Library's catalogs, keeping expansion and the scroll position.
        -1 for totalCount means the index is not open: the Library row shows no number
        rather than a zero that claims the library is empty. */
    void setSources(const QVector<LibrarySource> &sources, qint64 totalCount);

    /*  Show the folder filter the Filters panel holds. Selected = included; an excluded
        folder is drawn the way Filters draws an exclusion. Emits nothing. */
    void syncFromFilters(const QStringList &includes, const QStringList &excludes);

    /*  The expanded folders, for MW to keep across sessions, and to put back. */
    QStringList expandedPaths() const;
    void setExpandedPaths(const QStringList &paths);

    // Re-apply the palette-derived colours; MW::setBackgroundShade calls it.
    void updateStyle();

signals:
    /*  The folder filter this click asks for. Both empty = the whole Library. */
    void folderFilterRequested(const QStringList &includes, const QStringList &excludes);
    void revealInFoldersRequested(const QString &path);
    void showInFileManagerRequested(const QString &path);
    void manageCatalogRequested();

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void leaveEvent(QEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void contextMenuEvent(QContextMenuEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    enum Role {
        PathRole = Qt::UserRole + 1,    // the folder's path; empty for the Library row
        AnchorsRole,                    // a catalog row: the anchors it stands for
        OfflineRole                     // on a volume that is not mounted
    };

    /*  What a click with these modifiers on this item asks for, given what is
        currently filtered. */
    void requestFor(QTreeWidgetItem *item, Qt::KeyboardModifiers mods);
    QList<QTreeWidgetItem *> visibleItems() const;
    void resizeColumns();
    void styleItem(QTreeWidgetItem *item);
    static QIcon tintedIcon(const QString &resource, const QColor &c);

    QTreeWidgetItem *libraryItem = nullptr;
    HoverDelegate *delegate = nullptr;
    QString countMetric;
    int countMargin;
    QIcon libraryIcon;
    QIcon catalogIcon;
    QIcon folderIcon;
    QColor excludedColor;
    QColor offlineColor;

    QStringList includes;               // the last state MW pushed
    QStringList excludes;
    QSet<QString> expanded;             // folder paths, kept across rebuilds
    QString rangeAnchorPath;            // where a Shift+click range starts
    bool swallowRelease = false;
};

#endif // LIBTREE_H
