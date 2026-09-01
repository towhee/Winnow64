#ifndef FINDPANEL_H
#define FINDPANEL_H

#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QStringList>
#include <QTimer>
#include <QToolButton>
#include <QWidget>

#include "Cache/catalog.h"

class Filters;

/*
    The body of the Find dock: one search surface whose SCOPE is a control.
    See notes/Documentation.txt "The Find Dock (Folders and Catalog)".

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

    FOLDERS narrows what is loaded: checking an item re-runs the proxy filter and the
    counts move. CATALOG searches the index and its result is not a filter but a set of
    images to LOAD or ADD -- so the footer changes verb with the scope rather than the
    panel changing shape.

    THE TWO WORDS NAME THE SETS THEY SEARCH, which is what a scope switch has to do. The
    earlier pair, Here and Everywhere, named them by distance from the user instead, and
    "Everywhere" in particular promised more than it delivers: the catalog holds the
    folders Winnow has indexed, not every image on the machine.

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
    of the grid Winnow already has, so Catalog reports how many images matched and
    offers to load them; they then appear in the ordinary views where the loupe, Develop,
    ratings and filtering all work on them as on a folder.
*/
class FindPanel : public QWidget
{
    Q_OBJECT

public:
    enum Scope { FolderScope, CatalogScope };

    /* filters is OWNED BY MW and re-parented into this layout -- it is the same widget
       the Filters dock used, so every existing filterDock reference keeps working. */
    explicit FindPanel(Filters *filters, QWidget *parent = nullptr);

    Scope scope() const { return currentScope; }
    void setScope(Scope scope);

    /* Put the cursor in the search box, selecting what is there so typing replaces it.
       F2 and Shift+F2 call this after switching to their scope. */
    void focusSearch();

    /* Re-read the catalog size and, in Catalog scope, its categories. Called when the
       dock becomes visible and after a folder load has added to the catalog. */
    void refresh();

    /* Reflect whether a background scan is running, in the status line. */
    void setScanning(bool scanning);

    /* Re-apply the scope buttons' colours. Called from MW::setBackgroundShade, because
       they are theme-derived and would otherwise stay frozen at the shade in force when
       the panel was built. */
    void updateStyle();

signals:
    /* The user asked for these paths. append = true adds them to what is already loaded
       instead of replacing it. MW owns what either means. */
    void loadResults(const QStringList &paths, bool append);
    /* Open the Catalogued Folders editor. */
    void manageRootsRequested();
    /* Back in Folders scope: the tree is holding the CATALOG's values and must be rebuilt
       from the datamodel. Only MW knows whether the model is ready for that, so it owns
       the rebuild (buildFiltersWhenModelReady) and this only asks. */
    void rebuildFolderCategoriesRequested();
    /*  The user flipped the scope from THIS panel. MW::setScope is the one place
        G::scope changes and the two Catalog tree rows are mirrored, so the panel
        reports rather than decides. */
    void scopeChanged(int scope);

private slots:
    /* Run the Catalog query. Debounced; a no-op in Folders scope, where the tree drives
       the proxy filter directly. */
    void runSearch();

private:
    void updateFooter();
    void applyScope();
    /* The query the box and the checked items currently describe. */
    CatalogQuery currentQuery() const;

    Filters *filters = nullptr;

    QToolButton *foldersBtn = nullptr;
    QToolButton *catalogBtn = nullptr;
    QLineEdit *searchEdit = nullptr;

    QLabel *resultLabel = nullptr;
    QPushButton *loadBtn = nullptr;
    QPushButton *addBtn = nullptr;
    QWidget *loadRow = nullptr;

    QLabel *catalogStatusLabel = nullptr;
    QPushButton *manageRootsBtn = nullptr;

    QTimer *debounce = nullptr;

    /* The most recent Catalog result, held so Load does not have to re-run the query
       -- and so what gets loaded is exactly what the count described. */
    QStringList results;
    int totalMatches = 0;
    /*  True when the catalog scope is showing the most recent images because
        nothing has been asked -- the footer says something different then, and
        it is not a "no matches" case. */
    bool noQuery = true;

    Scope currentScope = FolderScope;
    bool scanning = false;

    /* How many paths a single search will return. The grid copes with far more than a
       user can review, but an unbounded result set on a large catalog would spend seconds
       building a list nobody scrolls to the end of. The count reported is the TRUE total,
       so the user is told when they are seeing a subset. */
    static constexpr int kResultLimit = 5000;
    /*  The most a catalog search will load WITHOUT being asked. Equal to
        kResultLimit today, so every result the panel can produce auto-loads;
        it is a separate constant because the two answer different questions --
        how much may be shown, and how much may be replaced unasked -- and the
        second is the one to lower if a load ever feels heavy. */
    static constexpr int kAutoLoadMax = kResultLimit;
    /* Keystrokes are coalesced into one query: it hits SQLite and FTS5 on the GUI thread,
       and at a quarter of a million rows a query per character would be felt. */
    static constexpr int kDebounceMs = 250;
};

#endif // FINDPANEL_H
