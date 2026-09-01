#ifndef CATALOGVIEW_H
#define CATALOGVIEW_H

#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QStringList>
#include <QTimer>
#include <QSet>
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

    THE KEYWORD LIST IS FLAT, not a tree, because the vocabulary is
    (Metadata/keywordflatten.h): a hierarchical path contributes each of its node names as
    an ordinary keyword. A name seen under more than one parent is AMBIGUOUS --
    "Vancouver" under both Canada and USA -- and is coloured, with its parents in the
    tooltip. It is resolved by EXCLUDING: include Vancouver, exclude USA. Opt+click or
    the context menu sets an exclusion, the same gesture the Filters dock uses.

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

    /* Reflect whether a scan is running. The controls that start one now live in the
       Catalogued Folders dialog; this only reports, in the status line. */
    void setScanning(bool scanning);

signals:
    /* The user asked for these paths to be loaded into the datamodel. append = true adds
       them to what is already loaded instead of replacing it. MW owns what either means;
       this widget never touches the model. */
    void loadResults(const QStringList &paths, bool append);
    /* Open the Catalogued Folders dialog -- which folders are indexed, and Scan Now. */
    void manageRootsRequested();

private slots:
    void runSearch();

private:
    /* The query the controls currently describe. */
    CatalogQuery currentQuery() const;
    void rebuildKeywordList();
    void setAvailability();
    /* Set one keyword's state and re-run. state is Qt::Checked (include),
       Qt::PartiallyChecked (exclude) or Qt::Unchecked (ignore). */
    void setKeywordState(QTreeWidgetItem *item, Qt::CheckState state);
    /* Paint one row for its current state: included, excluded, or ambiguous. */
    void styleKeywordItem(QTreeWidgetItem *item) const;

    QLineEdit *searchEdit = nullptr;
    QSpinBox *minRating = nullptr;
    QTreeWidget *keywordTree = nullptr;
    QLabel *resultLabel = nullptr;
    QLabel *unavailableLabel = nullptr;
    QPushButton *loadBtn = nullptr;
    QPushButton *addBtn = nullptr;

    /* What is indexed, and the way to change it. The editor itself is a separate dialog
       (Dialogs/catalogrootsdlg.h): it is configuration, and it was taking up the lower
       third of a panel whose job is asking questions. */
    QLabel *catalogStatusLabel = nullptr;
    QPushButton *manageRootsBtn = nullptr;
    bool scanning = false;

    /* Coalesces keystrokes into one query. */
    QTimer *debounce = nullptr;

    /* The most recent result, held so Load does not have to re-run the query -- and so
       what gets loaded is exactly what the count described. */
    QStringList results;
    int totalMatches = 0;

    /* The keywords the user has included and excluded, as displayed names. Both empty
       means no keyword restriction. Held here rather than read off the widget so a
       refresh can rebuild the list without losing the selection. */
    QSet<QString> includedKeywords;
    QSet<QString> excludedKeywords;

    /* Names the catalog has seen under more than one parent -- the keywords flattening
       made ambiguous. Case-folded; refreshed with the list. EMPTY MEANS UNKNOWN when
       there is no catalog, not "none are ambiguous". */
    QSet<QString> ambiguousKeywords;

    /* Set by itemChanged when QTreeWidget has ALREADY toggled the box for us (a click on
       the indicator), so itemClicked knows not to toggle it a second time. Without it the
       two cancel out and the checkbox looks dead -- the same reason Filters keeps
       itemCheckStateHasChanged. */
    bool keywordCheckJustChanged = false;

    /* How many paths a single search will return. The grid copes with far more than a
       user can review, but an unbounded result set on a large catalog would spend
       seconds building a list nobody scrolls to the end of. The count reported is the
       TRUE total, so the user is told when they are seeing a subset. */
    static constexpr int kResultLimit = 5000;
};

#endif // CATALOGVIEW_H
