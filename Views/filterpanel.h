#ifndef FILTERPANEL_H
#define FILTERPANEL_H

#include <QLabel>
#include <QLineEdit>
#include <QStringList>
#include <QTimer>
#include <QWidget>

#include <climits>
#include "Main/global.h"
#include <QVector>

#include "Cache/catalog.h"

class Filters;

/*
    The body of the Filter dock: one search surface whose SCOPE is a control.
    See notes/Documentation.txt "The Filter Dock (Folders and Catalog)".

    WHY ONE PANEL. Filters searched the datamodel and the Catalog panel searched the
    index, and they looked alike because they are the same question asked of different
    sets. Keeping them apart meant two keyword lists that disagreed, two search boxes with
    different grammars under paired shortcuts, and a Catalog panel that could not reach
    most of what CatalogQuery already supported while Filters had exactly those categories
    with counts and checkboxes. The scope switch is what that difference actually is.

    THE CATEGORY TREE IS THE Filters WIDGET ITSELF, not a copy of it. Both scopes render
    the same categories, the same checkboxes, the same include/exclude gestures, the same
    colours -- only where the values came from changes. Anything else would drift, which
    is the failure the two panels had already demonstrated.

    IN BOTH SCOPES THE PANEL FILTERS; ONLY THE SET DIFFERS. Checking an item re-runs the
    proxy filter and the counts move, over the open folder or over the whole catalog.
    There is no found set to LOAD and nothing to ADD it to: the catalog is browsed whole,
    so the buttons and the footer that described a search RESULT are gone. Functionality
    over what is loaded -- collections, if they are built -- belongs in the main menu and
    the context menus, not in buttons under a filter tree.

    THE SCOPE IS NOT SET FROM THIS PANEL. The Folders|Catalog buttons and the "Manage..."
    row are gone: choosing the catalog is File > Open Catalog (or the Catalog row above
    the Folders and Bookmarks trees, or Shift+F2), and choosing a folder is selecting one.
    Which folders are indexed is configuration, and lives in File > Manage Catalog...
    The panel is the search surface for whichever scope MW has set; it does not own it.

    SWITCHING SCOPE CARRIES THE SEARCH TEXT, AND CLEARS THE CHECKED ITEMS. The text is
    the question, and carrying it is the hand-off in both directions: a folder search that
    found nothing is one click from being asked of the whole library, and a library search
    can be narrowed once it is loaded. It is also why "seed the filters from the catalog
    query after a load" is not a separate feature -- it is this.

    The category CHECKS deliberately do not carry, because the two scopes are different
    vocabularies: a folder offers the three camera models in it, the library offers forty.
    Re-applying checks that merely share a name would silently build a different query
    from the one the user set up, and the items they had checked may not exist on the
    other side at all. The tree is rebuilt for the scope it is showing.

    THE SEARCH BOX IS SHARED. F2 focuses it in Folders, Shift+F2 focuses it in Catalog,
    so the pairing those two shortcuts always implied is now literally true. The text is
    parsed by one grammar (Utilities/searchterms.h) whichever scope runs it.

    IT DOES NOT SHOW THUMBNAILS. A picture list in a narrow dock would be a worse version
    of the grid Winnow already has: what the catalog scope matches is loaded into the
    ordinary views, where the loupe, Develop, ratings and filtering all work on it as on
    a folder.

    NOTHING TO REPORT IS NOTHING SHOWN. The only text left under the tree is the reason
    the panel can show nothing -- no index, or an empty one -- and it hides again as soon
    as that stops being true.
*/
class FilterPanel : public QWidget
{
    Q_OBJECT

public:
    enum Scope { FolderScope, CatalogScope };

    /* filters is OWNED BY MW and re-parented into this layout -- it is the same widget
       the Filters dock used, so every existing filterDock reference keeps working. */
    explicit FilterPanel(Filters *filters, QWidget *parent = nullptr);

    Scope scope() const { return currentScope; }
    void setScope(Scope scope);

    /* Put the cursor in the search box, selecting what is there so typing replaces it.
       F2 and Shift+F2 call this after switching to their scope. */
    void focusSearch();

    /* Re-read the catalog size and, in Catalog scope, its categories. Called when the
       dock becomes visible and after a folder load has added to the catalog. */
    void refresh();

    /* Reflect whether a background scan is running, in the footer. */
    void setScanning(bool scanning);

signals:
    /* The user asked for these paths. append = true adds them to what is already loaded
       instead of replacing it. MW owns what either means. */
    /*  THE QUERY TRAVELS WITH THE RESULT. The paths are what to load; the query is
        what the user asked for, and the model keeps it as its ScopeRequest so a
        reload or a refresh has something to re-run. */
    void loadResults(const QVector<CatalogRow> &rows, bool append,
                     const CatalogQuery &query);
    /* Back in Folders scope: the tree is holding the CATALOG's values and must be rebuilt
       from the datamodel. Only MW knows whether the model is ready for that, so it owns
       the rebuild (buildFiltersWhenModelReady) and this only asks. */
    void rebuildFolderCategoriesRequested();
    /*  The panel changed scope. MW::setScope is the one place G::scope changes and the
        Catalog tree rows are mirrored, so the panel reports rather than decides. The
        scope is now only ever set FROM MW (File > Open Catalog, the Catalog tree rows,
        selecting a folder), so this is a mirror rather than an entry point -- kept
        because MW::setScope early-returns on no change and is the single authority. */
    void scopeChanged(int scope);

private slots:
    /* Run the Catalog query. Debounced; a no-op in Folders scope, where the tree drives
       the proxy filter directly. */
    void runSearch();

private:
    void updateStatus();
    void applyScope();
    /* The query the box and the checked items currently describe. */
    CatalogQuery currentQuery() const;

    Filters *filters = nullptr;

    QLineEdit *searchEdit = nullptr;

    /*  Shown ONLY when there is a reason the panel can show nothing: no index, or an
        empty one. Hidden the rest of the time -- see updateStatus. */
    QLabel *statusLabel = nullptr;

    QTimer *debounce = nullptr;

    /* The most recent Catalog result: what was loaded, held so the next run can tell
       whether the set actually changed before loading it again. */
    /*  WHOLE ROWS, not paths. The search that produced them already selected everything a
        datamodel row displays (Catalog::searchRows), so loading them opens no files. The
        paths are still what the panel compares run-to-run -- see resultPaths. */
    QVector<CatalogRow> results;
    /*  The result as paths, which is what the run-to-run comparison needs. Built on
        demand rather than stored beside results, so the two cannot disagree about what
        the current result is. */
    QStringList resultPaths() const;
    int totalMatches = 0;
    Scope currentScope = FolderScope;
    bool scanning = false;

    /*  How many paths a single search returns, and the most it will load WITHOUT
        being asked. Both were constants (5,000); they are now G::maxSearchResults,
        a preference, because the ceiling on how much of a large library to bring
        in at once is the user's trade to make -- the same one the thumbnail
        ceiling is. The count reported is still the TRUE total, so a user seeing a
        subset is told so.

        The two remain SEPARATE QUESTIONS even though one setting drives both:
        how much may be shown, and how much may be replaced unasked. If a load
        ever feels heavy it is the second to lower, and having them named
        apart is what makes that possible without touching the first. */
    /*  The SQL limit, passed straight through: 0 means no LIMIT clause at all, which is
        the default. Returning INT_MAX instead would put "LIMIT 2147483647" on every
        query -- the same answer, said misleadingly. */
    static int resultLimit()  { return G::maxSearchResults > 0 ? G::maxSearchResults : 0; }
    /*  How much may be loaded WITHOUT being asked. Uncapped by default, because that is
        the whole point: picking the catalog shows the catalog. It stays a separate name
        from resultLimit so that "how much is found" and "how much is loaded unasked"
        can be told apart again if an automatic load ever feels heavy. */
    static int autoLoadMax()  { return G::maxSearchResults > 0 ? G::maxSearchResults
                                                              : INT_MAX; }
    /* Keystrokes are coalesced into one query: it hits SQLite and FTS5 on the GUI thread,
       and at a quarter of a million rows a query per character would be felt. */
    static constexpr int kDebounceMs = 250;
};

#endif // FILTERPANEL_H
