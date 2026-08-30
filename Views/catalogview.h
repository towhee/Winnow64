#ifndef CATALOGVIEW_H
#define CATALOGVIEW_H

#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QCheckBox>
#include <QListWidget>
#include <QSpinBox>
#include <QStringList>
#include <QTimer>
#include <QTreeWidget>
#include <QWidget>

#include "Cache/catalog.h"

/*
    The body of the Catalog dock: search across every image Winnow has catalogued, rather
    than only the folder currently loaded. See notes/Documentation.txt "The Catalog".

    WHAT IT IS FOR. The Filters dock searches the DATAMODEL -- what is loaded right now.
    This searches the INDEX -- everything the app has ever seen -- and its results are not
    a filter but a new set of images to load. The two look similar and answer different
    questions, which is why they are separate docks rather than another Filters category.

    THE RESULT IS NOT SHOWN AS A LIST HERE. A thumbnail list in a narrow dock would be a
    worse version of the grid Winnow already has, so the dock reports how many images
    matched and offers to LOAD them; the images themselves appear in the normal views,
    where the loupe, Develop, rating and filtering all work on them as usual. Loading
    replaces the datamodel exactly as a folder change does (MW::loadCatalogResults).

    SEARCHING IS DEBOUNCED, not run per keystroke: the query hits SQLite and FTS5 on the
    GUI thread, and at a quarter of a million rows a query per character would be felt.
    The delay also stops half-typed FTS syntax from being run at every intermediate state.

    WHEN THERE IS NO CATALOG the controls are disabled and the panel says why, inline.
    Explaining after the fact -- letting the user type a search and only then reporting
    that nothing is indexed -- is the thing this deliberately avoids.
*/
class CatalogView : public QWidget
{
    Q_OBJECT

public:
    explicit CatalogView(QWidget *parent = nullptr);

    /* Re-read the keyword facets and the catalog size. Called when the dock becomes
       visible and after a folder load has added to the catalog, so the panel does not
       show a stale picture of what is indexed. */
    void refresh();

    /* Put the cursor in the search box, selecting whatever is there so typing replaces
       it. The Shift+F2 action calls this after showing the dock. */
    void focusSearch();

    /* The roots panel is driven by MW, which owns the list and its persistence. */
    void setRoots(const QStringList &roots, bool recurse);
    QStringList roots() const;
    bool rootsRecurse() const;
    /* Reflect whether a scan is running, so the button says Stop and the list cannot be
       edited underneath it. */
    void setScanning(bool scanning);

signals:
    /* The user asked for these paths to be loaded into the datamodel. MW owns what that
       means; this widget never touches the model. */
    void loadResults(const QStringList &paths);
    /* The root list changed and should be persisted. */
    void rootsChanged(const QStringList &roots, bool recurse);
    void scanRequested();
    void stopScanRequested();

private slots:
    void runSearch();

private:
    /* The query the controls currently describe. */
    CatalogQuery currentQuery() const;
    void rebuildKeywordTree();
    void setAvailability();

    QLineEdit *searchEdit = nullptr;
    QSpinBox *minRating = nullptr;
    QTreeWidget *keywordTree = nullptr;
    QLabel *resultLabel = nullptr;
    QLabel *unavailableLabel = nullptr;
    QPushButton *loadBtn = nullptr;

    /* Designated roots -- the folders scanned in the background so search covers a
       library before it has been browsed. */
    QListWidget *rootList = nullptr;
    QCheckBox *recurseBox = nullptr;
    QPushButton *addRootBtn = nullptr;
    QPushButton *removeRootBtn = nullptr;
    QPushButton *scanBtn = nullptr;
    bool scanning = false;

    /* Coalesces keystrokes into one query. */
    QTimer *debounce = nullptr;

    /* The most recent result, held so Load does not have to re-run the query -- and so
       what gets loaded is exactly what the count described. */
    QStringList results;
    int totalMatches = 0;

    /* The keyword the user picked in the tree, as its full hierarchical path when it has
       one. Empty means no keyword restriction. */
    QString selectedKeyword;

    /* How many paths a single search will return. The grid copes with far more than a
       user can review, but an unbounded result set on a large catalog would spend
       seconds building a list nobody scrolls to the end of. The count reported is the
       TRUE total, so the user is told when they are seeing a subset. */
    static constexpr int kResultLimit = 5000;
};

#endif // CATALOGVIEW_H
