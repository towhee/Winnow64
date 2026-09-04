#ifndef CATALOGSCOPETREE_H
#define CATALOGSCOPETREE_H

#include <QTreeWidget>
#include <QString>

/*
    THE CATALOG AS A SCOPE, SHOWN WHERE SCOPE IS CHOSEN.

    A small tree that sits ABOVE the Folders tree and above the Bookmarks list:

        > 🗀 Catalog                    43050
            🗀 2017                      4171
            🗀 2018                      3824

    Selecting the Catalog row means "look at the whole catalog" the same way clicking a
    folder means "look at that folder" -- scope is chosen in one place, so filtering then
    means the same thing in both. Selecting a YEAR means the same thing plus one filter
    already applied: MW switches to the catalog and checks that year in the Filters
    panel, which is the gesture a person would otherwise make in two places.

    IT IS NOT PART OF THE FOLDERS TREE, deliberately. That tree is a QTreeView over a
    QFileSystemModel, which is filesystem-backed and cannot carry a synthetic root
    without overriding its node structure. A tree above it reads as the first rows,
    belongs to no model of its own, and interferes with neither.

    IT IS SIZED TO ITS CONTENTS: collapsed it costs one row of the panel, and expanded it
    shows EVERY year, because a list of years that stops partway through is not a list of
    the catalog's years. The only limit is the panel itself -- it will not take the last
    kMinFolderRows of the dock, so the folder tree beneath is always still usable -- and
    that limit only bites in a dock too short to hold both.

    THE COUNT COLUMN LINES UP with the count column of the tree below, because the two
    are read as one list. The neighbour's metric string and margin are passed in rather
    than assumed, so this is not tied to FSTree's arithmetic.

    ONE INSTANCE, IN THE FOLDERS PANEL. The same tree was offered above Bookmarks too,
    on the reasoning that scope is chosen in either -- but Bookmarks is a list of folders
    the USER put there, and a row nobody bookmarked reads as clutter in it. Clicking asks
    MW to change scope and MW pushes the result back here, so this and the Find panel
    cannot disagree about what is being looked at.
*/
class CatalogScopeTree : public QTreeWidget
{
    Q_OBJECT

public:
    /*  countMetric/countMargin mirror how the tree BELOW this one sizes its count
        column (see FSTree::resizeColumns and BookMarks::resizeColumns), so the numbers
        in both line up. */
    explicit CatalogScopeTree(const QString &countMetric, int countMargin,
                              QWidget *parent = nullptr);

    /*  The catalogued image count shown beside the name. Passing -1 means "not known
        yet" and shows the bare name -- a count of 0 and an unopened index are different
        facts, and the panel says so rather than implying the library is empty. */
    void setImageCount(qint64 count);

    /*  Light the Catalog row (or the year within it) as the current scope. MW calls
        this from setScope, so all views of the scope agree. */
    void setScopeIsCatalog(bool isCatalog);

    /*  Re-read the years from the catalog, off the GUI thread. Cheap to call: it does
        nothing while the row is collapsed and nothing has asked to see the years. */
    void refreshYears();

    // Re-apply the palette-derived stylesheet; MW::setBackgroundShade calls it.
    void updateStyle();

signals:
    /*  The whole catalog was chosen. */
    void catalogChosen();
    /*  One year of the catalog was chosen: the catalog, prefiltered on that year. */
    void catalogYearChosen(const QString &year);

protected:
    void resizeEvent(QResizeEvent *event) override;

protected:
    /*  The dock resizing changes how many years FIT, and this widget's own geometry does
        not change when the dock grows taller -- so the parent's resize is watched. */
    bool eventFilter(QObject *watched, QEvent *event) override;
    void showEvent(QShowEvent *event) override;

private:
    /*  Rows of the tree BELOW that must survive, however many years there are. */
    static constexpr int kMinFolderRows = 4;

    QTreeWidgetItem *catalogItem = nullptr;
    qint64 imageCount = -1;
    QString countMetric;
    int countMargin;
    bool yearsWanted = false;    // the row has been expanded at least once
    bool yearsPending = false;   // a query is in flight
    QIcon catalogIcon;
    QIcon yearIcon;

    void refreshText();
    void setYears(const QMap<QString, int> &years);
    void fitToContents();
    void resizeColumns();
    static QIcon tintedIcon(const QColor &c);
};

#endif // CATALOGSCOPETREE_H
